#include "script.h"

#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <sstream>

namespace
{
enum class TokKind { Word, String, Number, Color, LBrace, RBrace, Comma, Label, EOL };

struct Token
{
    TokKind kind;
    std::string text;
    int line = 0;
};

bool isHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::vector<Token> tokenize(const std::string& text)
{
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1;

    while (i < text.size())
    {
        char c = text[i];
        if (c == '\n')
        {
            tokens.push_back({TokKind::EOL, "", line});
            ++line;
            ++i;
            continue;
        }
        if (c == '\r') { ++i; continue; }
        if (c == ' ' || c == '\t') { ++i; continue; }

        if (c == '#')
        {
            // # 后跟恰好 6 位十六进制 => 颜色；否则整行注释
            bool isColor = false;
            if (i + 7 <= text.size())
            {
                std::string hex = text.substr(i + 1, 6);
                isColor = true;
                for (char h : hex) if (!isHex(h)) { isColor = false; break; }
            }
            if (isColor)
            {
                tokens.push_back({TokKind::Color, text.substr(i, 7), line});
                i += 7;
                continue;
            }
            // 注释：跳过到行尾
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }

        if (c == '{') { tokens.push_back({TokKind::LBrace, "{", line}); ++i; continue; }
        if (c == '}') { tokens.push_back({TokKind::RBrace, "}", line}); ++i; continue; }
        if (c == ',') { tokens.push_back({TokKind::Comma, ",", line}); ++i; continue; }

        if (c == '@')
        {
            size_t start = ++i;
            while (i < text.size() && text[i] != ' ' && text[i] != '\t' &&
                   text[i] != '\n' && text[i] != '\r')
                ++i;
            std::string label = text.substr(start, i - start);
            if (!label.empty() && label.back() == ':') label.pop_back();  // 兼容 @part_end:
            tokens.push_back({TokKind::Label, label, line});
            continue;
        }

        if (c == '"')
        {
            ++i;
            std::string s;
            while (i < text.size() && text[i] != '"')
            {
                if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == '"')
                {
                    s += '"';
                    i += 2;
                    continue;
                }
                s += text[i];
                ++i;
            }
            if (i >= text.size())
                throw ScriptError(line, "unterminated string");
            ++i;
            tokens.push_back({TokKind::String, s, line});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-')
        {
            size_t start = i;
            while (i < text.size())
            {
                char d = text[i];
                if (std::isdigit(static_cast<unsigned char>(d)) || d == '.' || d == '-') ++i;
                else break;
            }
            tokens.push_back({TokKind::Number, text.substr(start, i - start), line});
            continue;
        }

        // 普通词（名字 / 路径 / 关键字）：不含空白与特殊字符
        size_t start = i;
        while (i < text.size())
        {
            char w = text[i];
            if (w == ' ' || w == '\t' || w == '\n' || w == '\r' ||
                w == '"' || w == '{' || w == '}' || w == ',' || w == '#' || w == '@')
                break;
            ++i;
        }
        if (i == start) ++i;   // 未知字符，跳过避免死循环
        tokens.push_back({TokKind::Word, text.substr(start, i - start), line});
    }
    tokens.push_back({TokKind::EOL, "", line});
    return tokens;
}

class Parser
{
public:
    explicit Parser(std::vector<Token> tokens)
        : toks_(std::move(tokens)) {}

    Script parse()
    {
        Script script;
        while (!atEnd())
        {
            skipEols();
            if (atEnd()) break;
            if (cur().kind == TokKind::Word && cur().text == "chapter")
            {
                parseChapter(script);
                continue;
            }
            Stmt stmt = parseStatement();
            if (std::holds_alternative<LabelStmt>(stmt))
            {
                std::string name = std::get<LabelStmt>(stmt).name;
                if (script.labels.count(name))
                    throw ScriptError(cur().line, "duplicate label @" + name);
                script.labels[name] = script.stmts.size();
            }
            script.stmts.push_back(std::move(stmt));
            expectEol();
        }
        return script;
    }

private:
    std::vector<Token> toks_;
    size_t pos_ = 0;

    void parseChapter(Script& script)
    {
        next();   // 'chapter'
        int id = std::atoi(takeWord("chapter id").c_str());
        std::string name = cur().kind == TokKind::String ? next().text : takeWord("chapter name");
        std::string picture;
        if (cur().kind == TokKind::Word && cur().text == "picture")
        {
            next();
            picture = takeWord("chapter picture path");
        }
        if (cur().kind != TokKind::LBrace)
            throw ScriptError(cur().line, "expected '{' after chapter header");
        ++pos_;

        ChapterInfo info;
        info.id = id;
        info.name = name;
        info.picture = picture;
        info.stmtIndex = script.stmts.size();

        while (true)
        {
            skipEols();
            if (atEnd())
                throw ScriptError(toks_.back().line, "unterminated chapter block, missing '}'");
            if (cur().kind == TokKind::RBrace)
            {
                ++pos_;
                break;
            }
            Stmt stmt = parseStatement();
            if (std::holds_alternative<LabelStmt>(stmt))
            {
                std::string label = std::get<LabelStmt>(stmt).name;
                if (script.labels.count(label))
                    throw ScriptError(cur().line, "duplicate label @" + label);
                script.labels[label] = script.stmts.size();
            }
            script.stmts.push_back(std::move(stmt));
            expectEol();
        }
        // 章节体隐式结束，防止流入下一章
        script.stmts.push_back(ChapterEndStmt{});
        script.chapters.push_back(std::move(info));
    }

    bool atEnd() const { return pos_ >= toks_.size(); }
    const Token& cur() const { return toks_[pos_]; }
    const Token& peek(int n = 1) const
    {
        size_t p = pos_ + n;
        return p < toks_.size() ? toks_[p] : toks_.back();
    }
    Token next() { return toks_[pos_++]; }

    void skipEols()
    {
        while (!atEnd() && cur().kind == TokKind::EOL) ++pos_;
    }

    std::string takeWord(const char* what)
    {
        if (atEnd() || (cur().kind != TokKind::Word && cur().kind != TokKind::Number))
            throw ScriptError(atEnd() ? toks_.back().line : cur().line,
                              std::string("expected ") + what);
        return next().text;
    }

    std::string takeString(const char* what)
    {
        if (atEnd() || cur().kind != TokKind::String)
            throw ScriptError(atEnd() ? toks_.back().line : cur().line,
                              std::string("expected quoted string for ") + what);
        return next().text;
    }

    unsigned int takeColor()
    {
        if (atEnd() || cur().kind != TokKind::Color)
            throw ScriptError(atEnd() ? toks_.back().line : cur().line,
                              "expected color like #RRGGBB");
        std::string hex = next().text.substr(1);
        return static_cast<unsigned int>(std::strtoul(hex.c_str(), nullptr, 16));
    }

    void expectEol()
    {
        if (!atEnd() && cur().kind != TokKind::EOL)
            throw ScriptError(cur().line, "unexpected token '" + cur().text + "'");
        while (!atEnd() && cur().kind == TokKind::EOL) ++pos_;
    }

    Stmt parseStatement()
    {
        if (cur().kind == TokKind::Label)
        {
            LabelStmt s;
            s.name = next().text;
            return s;
        }
        if (cur().kind != TokKind::Word)
            throw ScriptError(cur().line, "expected command");

        std::string cmd = next().text;
        int line = toks_[pos_ - 1].line;

        if (cmd == "title")
        {
            TitleStmt s;
            s.text = cur().kind == TokKind::String ? next().text : takeWord("title");
            return s;
        }

        if (cmd == "init_character")
        {
            InitCharacterStmt s;
            s.name = takeWord("character name");
            s.texture = takeWord("texture path");
            s.color = takeColor();
            s.position = takeWord("position");
            return s;
        }
        if (cmd == "init_character_face")
        {
            InitCharacterFaceStmt s;
            s.name = takeWord("character name");
            s.dir = takeWord("face directory");
            return s;
        }
        if (cmd == "init_bg")
        {
            InitBgStmt s;
            s.id = takeWord("scene id");
            s.texture = takeWord("texture path");
            return s;
        }
        if (cmd == "show_bg")
        {
            ShowBgStmt s;
            s.id = takeWord("scene id");
            return s;
        }
        if (cmd == "show_character")
        {
            ShowCharacterStmt s;
            s.name = takeWord("character name");
            return s;
        }
        if (cmd == "hide_character")
        {
            HideCharacterStmt s;
            s.name = takeWord("character name");
            return s;
        }
        if (cmd == "change_face")
        {
            ChangeFaceStmt s;
            s.name = takeWord("character name");
            s.file = takeWord("face file");
            return s;
        }
        if (cmd == "say")
        {
            SayStmt s;
            s.character = takeWord("character name");
            s.text = takeString("dialogue text");
            return s;
        }
        if (cmd == "choice")
        {
            ChoiceStmt s;
            s.character = takeWord("character name");
            s.text = takeString("question text");
            if (cur().kind != TokKind::LBrace)
                throw ScriptError(cur().line, "expected '{' after choice");
            ++pos_;
            while (true)
            {
                skipEols();
                if (atEnd())
                    throw ScriptError(toks_.back().line, "unterminated choice block, missing '}'");
                if (cur().kind == TokKind::RBrace) { ++pos_; break; }
                if (cur().kind != TokKind::String)
                    throw ScriptError(cur().line, "expected option string in choice block");
                std::string optText = next().text;
                if (cur().kind != TokKind::Comma)
                    throw ScriptError(cur().line, "expected ',' after option text");
                ++pos_;
                if (cur().kind != TokKind::Word || cur().text != "jump")
                    throw ScriptError(cur().line, "expected 'jump <label>' in choice block");
                ++pos_;
                std::string label = takeWord("label");
                s.options.emplace_back(optText, label);
            }
            if (s.options.empty())
                throw ScriptError(line, "choice block must contain at least one option");
            return s;
        }
        if (cmd == "jump")
        {
            JumpStmt s;
            s.label = takeWord("label");
            return s;
        }
        if (cmd == "wait")
        {
            WaitStmt s;
            s.seconds = static_cast<float>(std::atof(takeWord("seconds").c_str()));
            return s;
        }
        if (cmd == "narrate")
        {
            NarrateStmt s;
            s.text = takeString("narration text");
            return s;
        }
        if (cmd == "move")
        {
            MoveStmt s;
            s.name = takeWord("character name");
            s.position = takeWord("position");
            return s;
        }
        if (cmd == "card")
        {
            CardStmt s;
            s.text = cur().kind == TokKind::String ? next().text : takeWord("card text");
            return s;
        }
        if (cmd == "show_cg")
        {
            ShowCgStmt s;
            s.file = takeWord("cg texture path");
            return s;
        }
        if (cmd == "hide_cg") return HideCgStmt{};
        if (cmd == "play_bgm")
        {
            PlayBgmStmt s;
            s.file = takeWord("audio file");
            return s;
        }
        if (cmd == "stop_bgm") return StopBgmStmt{};
        if (cmd == "play_se")
        {
            PlaySeStmt s;
            s.file = takeWord("audio file");
            return s;
        }
        if (cmd == "game_end") return GameEndStmt{};

        throw ScriptError(line, "unknown command '" + cmd + "'");
    }
};
}

Script parseGal(const std::string& text, const std::string& sourcePath)
{
    std::string src = text;
    // 兼容带 UTF-8 BOM 的脚本文件
    if (src.size() >= 3 &&
        static_cast<unsigned char>(src[0]) == 0xEF &&
        static_cast<unsigned char>(src[1]) == 0xBB &&
        static_cast<unsigned char>(src[2]) == 0xBF)
    {
        src.erase(0, 3);
    }
    Parser p(tokenize(src));
    Script s = p.parse();
    s.sourcePath = sourcePath;

    // 校验所有 jump 目标 / choice 目标存在
    for (const auto& stmt : s.stmts)
    {
        if (std::holds_alternative<JumpStmt>(stmt))
        {
            const std::string& label = std::get<JumpStmt>(stmt).label;
            if (!s.labels.count(label))
                throw ScriptError(0, "jump to undefined label '" + label + "'");
        }
        else if (std::holds_alternative<ChoiceStmt>(stmt))
        {
            for (const auto& opt : std::get<ChoiceStmt>(stmt).options)
            {
                if (!s.labels.count(opt.second))
                    throw ScriptError(0, "choice jumps to undefined label '" + opt.second + "'");
            }
        }
    }
    return s;
}
