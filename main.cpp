#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/engine.h"
#include "core/file_util.h"
#include "core/save.h"
#include "core/script.h"
#include "core/vm.h"
#include "game/game.h"
#include "game/title_scene.h"

namespace
{
std::string readFile(const std::string& path)
{
    return readFileUtf8(path);
}

std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i)
    {
        if (i == text.size() || text[i] == '\n')
        {
            out.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

void setCwdToExe(char* argv0)
{
    try
    {
        std::filesystem::path p = std::filesystem::absolute(argv0);
        std::filesystem::current_path(p.parent_path());
    }
    catch (...)
    {
        // 无法定位 exe 目录时保持当前工作目录
    }
}

// 无头脚本自检：不创建窗口，直接解释执行脚本，验证可达 game_end
int checkScript(const std::string& path)
{
    std::string text = readFile(path);
    if (text.empty())
    {
        printf("SCRIPT CHECK FAILED: cannot read %s\n", path.c_str());
        return 1;
    }
    Script script;
    try
    {
        script = parseGal(text, path);
    }
    catch (const ScriptError& e)
    {
        printf("SCRIPT CHECK FAILED: %s\n", e.what());
        return 1;
    }

    Game game(false);
    VM vm;
    vm.load(script);
    int guard = 0;
    while (guard++ < 200000)
    {
        switch (vm.state())
        {
        case VM::State::Running:
        case VM::State::WaitSay:
        case VM::State::WaitNarrate:
        case VM::State::WaitCard:
        case VM::State::WaitHide:
        case VM::State::WaitCg:
            vm.step(game);
            break;
        case VM::State::WaitChoice:
        {
            const ChoiceStmt* c = vm.choice();
            if (!c || c->options.empty())
            {
                printf("SCRIPT CHECK FAILED: empty choice\n");
                return 1;
            }
            vm.jumpTo(c->options[0].second, game);
            break;
        }
        case VM::State::WaitTimer:
            vm.tick(vm.waitRemaining() + 0.001f);
            break;
        case VM::State::Ended:
            printf("SCRIPT CHECK OK (%zu statements, %zu steps)\n",
                   script.stmts.size(), vm.steps());
            return 0;
        case VM::State::Error:
            printf("SCRIPT CHECK FAILED: %s\n", vm.error().c_str());
            return 1;
        }
    }
    printf("SCRIPT CHECK FAILED: too many steps (possible infinite loop)\n");
    return 1;
}

// 无头存档/读档自检：中途捕获快照，用全新实例恢复后继续到 game_end
int checkSaveLoad(const std::string& path)
{
    std::string text = readFile(path);
    if (text.empty())
    {
        printf("SAVE CHECK FAILED: cannot read %s\n", path.c_str());
        return 1;
    }
    Script script;
    try
    {
        script = parseGal(text, path);
    }
    catch (const ScriptError& e)
    {
        printf("SAVE CHECK FAILED: %s\n", e.what());
        return 1;
    }

    // 阶段 1：跑到第 4 句对话处捕获快照
    Game g(false);
    VM vm;
    vm.load(script);
    SaveData snap;
    bool captured = false;
    int sayCount = 0;
    for (int guard = 0; guard < 200000 && !captured; ++guard)
    {
        switch (vm.state())
        {
        case VM::State::Running:
            vm.step(g);
            break;
        case VM::State::WaitSay:
            if (sayCount == 2)
            {
                snap.ip = vm.ip();
                snap.vmState = static_cast<int>(vm.state());
                snap.valid = true;
                g.capture(snap);
                captured = true;
            }
            else
            {
                vm.step(g);
                ++sayCount;
            }
            break;
        case VM::State::WaitChoice:
            if (vm.choice() && !vm.choice()->options.empty())
                vm.jumpTo(vm.choice()->options[0].second, g);
            break;
        case VM::State::WaitTimer:
            vm.tick(vm.waitRemaining() + 0.001f);
            break;
        case VM::State::WaitNarrate:
        case VM::State::WaitCard:
        case VM::State::WaitHide:
        case VM::State::WaitCg:
            vm.step(g);
            break;
        case VM::State::Ended:
            printf("SAVE CHECK FAILED: script ended before capture point\n");
            return 1;
        case VM::State::Error:
            printf("SAVE CHECK FAILED: %s\n", vm.error().c_str());
            return 1;
        }
    }
    if (!captured)
    {
        printf("SAVE CHECK FAILED: cannot reach capture point\n");
        return 1;
    }

    // 阶段 2：全新 VM + Game 恢复，继续执行到 game_end
    Game g2(false);
    VM vm2;
    vm2.load(script);
    vm2.restore(snap.ip, VM::State::Running, 0.0f);
    g2.restore(snap);

    for (int guard = 0; guard < 200000; ++guard)
    {
        switch (vm2.state())
        {
        case VM::State::Running:
            vm2.step(g2);
            break;
        case VM::State::WaitSay:
        case VM::State::WaitNarrate:
        case VM::State::WaitCard:
        case VM::State::WaitHide:
        case VM::State::WaitCg:
            vm2.step(g2);
            break;
        case VM::State::WaitChoice:
            if (vm2.choice() && !vm2.choice()->options.empty())
                vm2.jumpTo(vm2.choice()->options[0].second, g2);
            break;
        case VM::State::WaitTimer:
            vm2.tick(vm2.waitRemaining() + 0.001f);
            break;
        case VM::State::Ended:
            printf("SAVE CHECK OK (ip=%zu, %zu characters, %zu log lines)\n",
                   snap.ip, snap.chars.size(), snap.log.size());
            return 0;
        case VM::State::Error:
            printf("SAVE CHECK FAILED: %s\n", vm2.error().c_str());
            return 1;
        }
    }
    printf("SAVE CHECK FAILED: timeout after restore\n");
    return 1;
}

// 无头章节自检：每个 chapter 都能从起点执行到章节结束
int checkChapterScripts(const std::string& path)
{
    std::string text = readFile(path);
    if (text.empty())
    {
        printf("CHAPTER CHECK FAILED: cannot read %s\n", path.c_str());
        return 1;
    }
    Script script;
    try
    {
        script = parseGal(text, path);
    }
    catch (const ScriptError& e)
    {
        printf("CHAPTER CHECK FAILED: %s\n", e.what());
        return 1;
    }
    if (script.chapters.empty())
    {
        printf("CHAPTER CHECK: no chapters in %s\n", path.c_str());
        return 0;
    }

    for (size_t ci = 0; ci < script.chapters.size(); ++ci)
    {
        const auto& ch = script.chapters[ci];
        Game g(false);
        VM vm;
        vm.load(script);
        vm.startAt(ch.stmtIndex, g);

        bool ok = false;
        for (int guard = 0; guard < 200000; ++guard)
        {
            switch (vm.state())
            {
            case VM::State::Running:
            case VM::State::WaitSay:
            case VM::State::WaitNarrate:
            case VM::State::WaitCard:
            case VM::State::WaitHide:
            case VM::State::WaitCg:
                vm.step(g);
                break;
            case VM::State::WaitChoice:
                if (vm.choice() && !vm.choice()->options.empty())
                    vm.jumpTo(vm.choice()->options[0].second, g);
                break;
            case VM::State::WaitTimer:
                vm.tick(vm.waitRemaining() + 0.001f);
                break;
            case VM::State::Ended:
                ok = true;
                break;
            case VM::State::Error:
                printf("CHAPTER CHECK FAILED: chapter %d: %s\n", ch.id, vm.error().c_str());
                return 1;
            }
            if (ok) break;
        }
        if (!ok)
        {
            printf("CHAPTER CHECK FAILED: chapter %d '%s' timeout\n", ch.id, ch.name.c_str());
            return 1;
        }
        printf("  chapter %d '%s' OK\n", ch.id, ch.name.c_str());
    }
    printf("CHAPTER CHECK OK (%zu chapters)\n", script.chapters.size());
    return 0;
}

void printUsage()
{
    printf("QLWT 视觉小说引擎\n");
    printf("用法: qlwt.exe [选项]\n");
    printf("  --script <文件>       指定 .gal 脚本（默认 assets/scripts/demo.gal）\n");
    printf("  --check-script        无窗口自检脚本，验证可执行到 game_end\n");
    printf("  --check-save          无窗口自检存档/读档（中途存档后恢复并跑到结尾）\n");
    printf("  --check-chapters      无窗口自检每个章节都能从起点执行到结束\n");
    printf("  --selftest [--frames N]  自动运行窗口自检（默认 20000 帧）\n");
}
}

int main(int argc, char** argv)
{
    setCwdToExe(argv[0]);

    std::string scriptPath = "assets/scripts/demo.gal";
    bool selftest = false;
    bool check = false;
    bool checkSave = false;
    bool checkChapters = false;
    int frames = 20000;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--script" && i + 1 < argc) scriptPath = argv[++i];
        else if (a == "--check-script")
        {
            check = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') scriptPath = argv[++i];
        }
        else if (a == "--check-save")
        {
            checkSave = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') scriptPath = argv[++i];
        }
        else if (a == "--check-chapters")
        {
            checkChapters = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') scriptPath = argv[++i];
        }
        else if (a == "--selftest") selftest = true;
        else if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--help" || a == "-h") { printUsage(); return 0; }
    }

    if (check) return checkScript(scriptPath);
    if (checkSave) return checkSaveLoad(scriptPath);
    if (checkChapters) return checkChapterScripts(scriptPath);

    std::string text = readFile(scriptPath);
    std::shared_ptr<Script> script;
    std::string parseError;
    // 默认脚本不存在/为空时不再报错：实际入口是章节选择界面，
    // 它会自己扫描 assets/scripts/ 下的脚本。只有脚本存在但
    // 语法错误时才在标题界面提示。
    if (!text.empty())
    {
        try
        {
            script = std::make_shared<Script>(parseGal(text, scriptPath));
        }
        catch (const ScriptError& e)
        {
            parseError = e.what();
        }
    }

    // 收集界面文案 + 脚本全文，用于构建字体字形
    std::vector<std::string> texts = uiTexts();
    for (const auto& line : splitLines(text)) texts.push_back(line);

    Engine engine;
    if (!engine.init(1280, 720, "QLWT 视觉小说引擎", texts, selftest, frames))
    {
        printf("failed to initialize window\n");
        return 1;
    }
    progressEnsureCreated();   // 开始游戏时确保章节进度文件存在
    engine.switchScene(std::make_shared<TitleScene>(&engine, scriptPath, script, parseError));
    engine.run();

    return selftest ? (engine.selftestPassed ? 0 : 2) : 0;
}
