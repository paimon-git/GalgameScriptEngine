#include "vm.h"

#include <cstdio>

void VM::load(const Script& script)
{
    script_ = &script;
    ip_ = 0;
    steps_ = 0;
    state_ = State::Running;
    say_ = nullptr;
    choice_ = nullptr;
    narrate_ = nullptr;
    card_ = nullptr;
    hideName_.clear();
    error_.clear();
}

void VM::step(Game& game)
{
    if (!script_ || state_ == State::Error) return;

    while (ip_ < script_->stmts.size())
    {
        const Stmt& stmt = script_->stmts[ip_];
        ++steps_;

        if (std::holds_alternative<TitleStmt>(stmt))
        {
            // 标题在解析时已提取，运行时忽略
        }
        else if (std::holds_alternative<LabelStmt>(stmt))
        {
            // 标签无操作
        }
        else if (std::holds_alternative<InitCharacterStmt>(stmt))
        {
            const auto& s = std::get<InitCharacterStmt>(stmt);
            game.initCharacter(s.name, s.texture, s.color, s.position);
        }
        else if (std::holds_alternative<InitCharacterFaceStmt>(stmt))
        {
            const auto& s = std::get<InitCharacterFaceStmt>(stmt);
            game.setFaceDir(s.name, s.dir);
        }
        else if (std::holds_alternative<InitBgStmt>(stmt))
        {
            const auto& s = std::get<InitBgStmt>(stmt);
            game.initBg(s.id, s.texture);
        }
        else if (std::holds_alternative<ShowBgStmt>(stmt))
        {
            game.showBg(std::get<ShowBgStmt>(stmt).id);
        }
        else if (std::holds_alternative<ShowCharacterStmt>(stmt))
        {
            game.showCharacter(std::get<ShowCharacterStmt>(stmt).name);
        }
        else if (std::holds_alternative<HideCharacterStmt>(stmt))
        {
            // 角色消失走黑屏过渡（由场景驱动，完成后继续执行）
            hideName_ = std::get<HideCharacterStmt>(stmt).name;
            state_ = State::WaitHide;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<ChangeFaceStmt>(stmt))
        {
            const auto& s = std::get<ChangeFaceStmt>(stmt);
            game.changeFace(s.name, s.file);
        }
        else if (std::holds_alternative<JumpStmt>(stmt))
        {
            const std::string& label = std::get<JumpStmt>(stmt).label;
            auto it = script_->labels.find(label);
            if (it == script_->labels.end())
            {
                error_ = "jump to undefined label @" + label;
                state_ = State::Error;
                return;
            }
            ip_ = it->second;
            continue;
        }
        else if (std::holds_alternative<WaitStmt>(stmt))
        {
            waitRemaining_ = std::get<WaitStmt>(stmt).seconds;
            state_ = State::WaitTimer;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<NarrateStmt>(stmt))
        {
            narrate_ = &std::get<NarrateStmt>(stmt);
            state_ = State::WaitNarrate;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<CardStmt>(stmt))
        {
            card_ = &std::get<CardStmt>(stmt);
            state_ = State::WaitCard;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<MoveStmt>(stmt))
        {
            const auto& s = std::get<MoveStmt>(stmt);
            game.moveCharacter(s.name, s.position);
        }
        else if (std::holds_alternative<ShowCgStmt>(stmt))
        {
            game.showCg(std::get<ShowCgStmt>(stmt).file);
            state_ = State::WaitCg;   // 阻塞：视频播完 / 图片 3 秒后自动继续
            ++ip_;
            return;
        }
        else if (std::holds_alternative<HideCgStmt>(stmt))
        {
            game.hideCg();
        }
        else if (std::holds_alternative<SayStmt>(stmt))
        {
            say_ = &std::get<SayStmt>(stmt);
            game.setSpeaking(say_->character);
            state_ = State::WaitSay;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<ChoiceStmt>(stmt))
        {
            choice_ = &std::get<ChoiceStmt>(stmt);
            game.setSpeaking(choice_->character);
            state_ = State::WaitChoice;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<GameEndStmt>(stmt))
        {
            state_ = State::Ended;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<ChapterEndStmt>(stmt))
        {
            state_ = State::Ended;
            ++ip_;
            return;
        }
        else if (std::holds_alternative<PlayBgmStmt>(stmt))
        {
            const auto& s = std::get<PlayBgmStmt>(stmt);
            game.playBgm(s.file);
        }
        else if (std::holds_alternative<StopBgmStmt>(stmt))
        {
            game.stopBgm();
        }
        else if (std::holds_alternative<PlaySeStmt>(stmt))
        {
            const auto& s = std::get<PlaySeStmt>(stmt);
            game.playSe(s.file);
        }
        ++ip_;
    }

    if (ip_ >= script_->stmts.size())
    {
        // 脚本自然结束视同 game_end
        state_ = State::Ended;
    }
}

void VM::tick(float dt)
{
    if (state_ != State::WaitTimer) return;
    waitRemaining_ -= dt;
    if (waitRemaining_ <= 0.0f)
    {
        waitRemaining_ = 0.0f;
        state_ = State::Running;
    }
}

void VM::jumpTo(const std::string& label, Game& game)
{
    if (!script_ || state_ != State::WaitChoice) return;
    auto it = script_->labels.find(label);
    if (it == script_->labels.end())
    {
        error_ = "choice jumps to undefined label @" + label;
        state_ = State::Error;
        return;
    }
    ip_ = it->second;
    state_ = State::Running;
    step(game);
}

void VM::restore(size_t ip, State state, float waitRemaining)
{
    ip_ = ip;
    state_ = state;
    waitRemaining_ = waitRemaining;
    say_ = nullptr;
    choice_ = nullptr;
    narrate_ = nullptr;
    card_ = nullptr;
    hideName_.clear();
    error_.clear();
}

void VM::startAt(size_t index, Game& game)
{
    ip_ = 0;
    state_ = State::Running;
    say_ = nullptr;
    choice_ = nullptr;
    narrate_ = nullptr;
    card_ = nullptr;
    hideName_.clear();
    error_.clear();

    // 先执行脚本开头的初始化指令（角色/表情/背景/标题），跳过剧情
    size_t limit = index < script_->stmts.size() ? index : script_->stmts.size();
    for (size_t i = 0; i < limit; ++i)
    {
        const Stmt& s = script_->stmts[i];
        if (std::holds_alternative<TitleStmt>(s)) continue;
        if (std::holds_alternative<InitCharacterStmt>(s))
        {
            const auto& c = std::get<InitCharacterStmt>(s);
            game.initCharacter(c.name, c.texture, c.color, c.position);
        }
        else if (std::holds_alternative<InitCharacterFaceStmt>(s))
        {
            const auto& c = std::get<InitCharacterFaceStmt>(s);
            game.setFaceDir(c.name, c.dir);
        }
        else if (std::holds_alternative<InitBgStmt>(s))
        {
            const auto& c = std::get<InitBgStmt>(s);
            game.initBg(c.id, c.texture);
        }
    }
    ip_ = index;
}
