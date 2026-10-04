#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/engine.h"
#include "core/file_util.h"
#ifdef __linux__
#include <linux/input.h>
#endif
#include "core/gesture.h"
#include "core/lang.h"
#include "core/save.h"
#include "core/script.h"
#include "core/vm.h"
#include "game/battle.h"
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
        case VM::State::WaitBattle:
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
        case VM::State::WaitBattle:
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
        case VM::State::WaitBattle:
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

// 触摸板手势监视器：不开窗口，把设备枚举结果和每帧手势实时打出来。
// 真机上"捏合没反应"时用它定位：是设备没读到，还是没识别出两根手指。
int gestureMonitor()
{
#ifdef __linux__
    printf("QLWT 触摸板手势监视器（Ctrl+C 退出）\n");
    if (!gesture::init(true)) return 1;
    printf("把两根手指放上触摸板：捏合看 zoom，拖动看 pan。\n");
    bool printed = false;
    while (true)
    {
        gesture::poll();
        const gesture::Frame& f = gesture::frame();
        if (f.active || f.touching)
        {
            printf("\r手指=%d   pan=(%8.2f, %8.2f)   zoom=%.4f      ",
                   f.fingers, f.panX, f.panY, f.zoom);
            std::fflush(stdout);
            printed = true;
        }
        else if (printed)
        {
            printf("\r%*s\r", 64, "");
            std::fflush(stdout);
            printed = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
#else
    printf("触摸板手势监视器只在 Linux 下可用\n");
    return 1;
#endif
}

// 触摸板手势自检：不碰真设备，直接喂一组合成的多点触控事件，
// 验证"双指拖动 → 平移""两指分开 → 放大""两指并拢 → 缩小"算得对不对。
int checkGesture()
{
#ifdef __linux__
    gesture::setRanges(0, 1024, 0, 768);      // 假设一块 1024x768 的触摸板
    auto syn = [] { gesture::feedEvent(EV_SYN, SYN_REPORT, 0); };
    auto touch = [](int slot, int id, int x, int y) {
        gesture::feedEvent(EV_ABS, ABS_MT_SLOT, slot);
        gesture::feedEvent(EV_ABS, ABS_MT_TRACKING_ID, id);
        gesture::feedEvent(EV_ABS, ABS_MT_POSITION_X, x);
        gesture::feedEvent(EV_ABS, ABS_MT_POSITION_Y, y);
    };
    auto move = [](int slot, int x, int y) {
        gesture::feedEvent(EV_ABS, ABS_MT_SLOT, slot);
        gesture::feedEvent(EV_ABS, ABS_MT_POSITION_X, x);
        gesture::feedEvent(EV_ABS, ABS_MT_POSITION_Y, y);
    };

    int failed = 0;
    auto report = [&](const char* what, bool ok, const gesture::Frame& f) {
        printf("  %-22s pan=(%.1f, %.1f)  zoom=%.3f  %s\n", what, f.panX, f.panY, f.zoom,
               ok ? "✓" : "✗");
        if (!ok) ++failed;
    };

    // 两根手指按下
    touch(0, 1, 400, 300);
    touch(1, 2, 600, 300);
    syn();
    gesture::take();

    // 双指向右平移 50（设备单位）
    move(0, 450, 300);
    move(1, 650, 300);
    syn();
    gesture::Frame p = gesture::take();
    report("双指右移", p.panX > 50.0f && std::fabs(p.zoom - 1.0f) < 0.02f && p.active, p);

    // 松手后还会按惯性往前滑几帧，然后停下来
    int glideFrames = 0;
    float glideTotal = 0.0f;
    gesture::Frame glide = p;
    while (glideFrames < 240)
    {
        glide = gesture::take();
        if (!glide.active) break;
        glideTotal += glide.panX;
        ++glideFrames;
    }
    report("松手后惯性滑行", glideFrames >= 3 && glideFrames < 120 &&
                                 glideTotal > 10.0f && !glide.active, glide);

    // 双指平移时间距有自然抖动（±2% 来回）：不该被当成捏合，
    // 否则画面会被"鼠标锚点缩放"带着漂，表现就是拖动不顺滑、拖不出斜线。
    // 同时验证斜向拖动：x、y 两个轴都要跟着动。
    touch(0, 1, 400, 300);
    touch(1, 2, 600, 300);
    syn();
    gesture::take();
    float wobbleZoom = 1.0f, wobblePanX = 0.0f, wobblePanY = 0.0f;
    for (int i = 0; i < 60; ++i)
    {
        const int cx = 500 + i * 3;                 // 中点斜着往右下走
        const int cy = 300 + i * 2;
        const int half = (i % 2 == 0) ? 102 : 98;   // 间距 204 / 196 来回抖
        touch(0, 1, cx - half, cy);
        touch(1, 2, cx + half, cy);
        syn();
        gesture::Frame f = gesture::take();
        wobbleZoom *= f.zoom;
        wobblePanX += f.panX;
        wobblePanY += f.panY;
    }
    report("抖动不误判捏合 + 斜向可拖",
           std::fabs(wobbleZoom - 1.0f) < 0.01f && wobblePanX > 100.0f && wobblePanY > 50.0f,
           gesture::Frame{});

    // 慢速捏合：每帧两指距离只变 0.04%，低于旧代码 0.05% 的"太小就跳过"阈值。
    // 之前这种捏合会被整段丢光（每帧的 zoom 都还是 1.0），表现就是"慢慢捏没反应"。
    // 先抬手再放下，拿一个干净的两指基准
    gesture::feedEvent(EV_ABS, ABS_MT_SLOT, 0);
    gesture::feedEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
    gesture::feedEvent(EV_ABS, ABS_MT_SLOT, 1);
    gesture::feedEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
    syn();
    gesture::take();
    touch(0, 1, 1000, 300);
    touch(1, 2, 3000, 300);
    syn();
    gesture::take();                       // 基准：两指相距 2000 设备单位
    float zslow = 1.0f;
    for (int i = 1; i <= 300; ++i)
    {
        const float d = 2000.0f * (1.0f + 0.0004f * static_cast<float>(i));
        move(0, static_cast<int>(2000.0f - d * 0.5f), 300);
        move(1, static_cast<int>(2000.0f + d * 0.5f), 300);
        syn();
        zslow *= gesture::take().zoom;
    }
    // 总间距变化 12%；其中前 2% 用于确认"这是在捏合"，之后才逐帧缩放
    report("慢速捏合能累积", zslow > 1.05f, gesture::Frame{});

    // 复位到一对干净的两指基准，后面的开合测试从这算
    move(0, 450, 300);
    move(1, 650, 300);
    syn();
    gesture::take();
    for (int i = 0; i < 40; ++i)           // 停住不动，把上一步"瞬移"留下的速度耗掉
    {
        syn();
        gesture::take();
    }

    // 两指分开：250 → 300（放大 1.2 倍）
    move(0, 400, 300);
    move(1, 700, 300);
    syn();
    gesture::Frame zi = gesture::take();
    report("两指分开", zi.zoom > 1.1f && zi.active, zi);

    // 两指并拢：300 → 200（缩小）
    move(0, 450, 300);
    move(1, 650, 300);
    syn();
    gesture::Frame zo = gesture::take();
    report("两指并拢", zo.zoom < 0.9f && zo.active, zo);

    // 抬起两根手指
    gesture::feedEvent(EV_ABS, ABS_MT_SLOT, 0);
    gesture::feedEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
    gesture::feedEvent(EV_ABS, ABS_MT_SLOT, 1);
    gesture::feedEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
    syn();
    gesture::Frame up = gesture::take();
    report("抬手后不再输出", !up.active, up);

    // 换手指（抬起一根、又放下另一根）时重新取基准，不该产生跳变
    touch(0, 1, 400, 300);
    touch(1, 2, 600, 300);
    syn();
    gesture::take();
    gesture::feedEvent(EV_ABS, ABS_MT_SLOT, 0);
    gesture::feedEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
    touch(2, 3, 1000, 300);      // 新的第二根，离得远也不该让画面跳
    syn();
    gesture::Frame swap = gesture::take();
    report("换手指不跳变", swap.active && std::fabs(swap.panX) < 0.01f, swap);

    // 协议 A：没有 SLOT / TRACKING_ID，每根手指用一次 SYN_MT_REPORT 结束
    auto protoATouch = [](int x, int y) {
        gesture::feedEvent(EV_ABS, ABS_MT_POSITION_X, x);
        gesture::feedEvent(EV_ABS, ABS_MT_POSITION_Y, y);
        gesture::feedEvent(EV_SYN, SYN_MT_REPORT, 0);
    };
    protoATouch(400, 300);
    protoATouch(600, 300);
    syn();
    gesture::take();
    protoATouch(460, 300);      // 两指一起右移 60
    protoATouch(660, 300);
    syn();
    gesture::Frame pa = gesture::take();
    report("协议A 双指右移", pa.panX > 50.0f && std::fabs(pa.zoom - 1.0f) < 0.02f && pa.active, pa);
    gesture::feedEvent(EV_SYN, SYN_REPORT, 0);   // 一根手指都不报 = 抬起
    gesture::Frame paUp = gesture::take();
    report("协议A 松手滑行", paUp.active && paUp.panX > 10.0f, paUp);
    int stopFrames = 0;
    while (stopFrames < 240)
    {
        if (!gesture::take().active) break;
        ++stopFrames;
    }
    report("协议A 惯性会停", stopFrames < 120, gesture::Frame{});

    printf("GESTURE CHECK %s\n", failed ? "FAILED" : "OK");
    return failed ? 1 : 0;
#else
    printf("GESTURE CHECK SKIPPED (非 Linux 平台)\n");
    return 0;
#endif
}

// 观测战自检：把每个剧本里定义过的战斗都无头跑一遍，
// 验证"能打完、不会卡死"（输赢都算通过，只要求正常结束）
int checkBattles()
{
    int total = 0, failed = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("assets/scripts", ec))
    {
        if (!entry.is_regular_file(ec)) continue;
        std::string p = entry.path().string();
        std::replace(p.begin(), p.end(), '\\', '/');
        if (p.size() < 4 || p.substr(p.size() - 4) != ".gal") continue;
        std::string text = readFile(p);
        if (text.empty()) continue;
        Script script;
        try
        {
            script = parseGal(text, p);
        }
        catch (const ScriptError&)
        {
            continue;
        }
        for (const auto& kv : script.battles)
        {
            ++total;
            Battle b;
            b.setup(kv.second);
            const bool ok = b.runHeadless();
            printf("  %-34s %-14s %s  %s (%d 回合)\n", p.c_str(), kv.first.c_str(),
                   ok ? (b.won() ? "WIN " : "LOSE") : "卡住了",
                   ok ? "✓" : "✗", b.round());
            if (!ok) ++failed;
        }
    }
    if (total == 0)
    {
        printf("BATTLE CHECK: 没有找到任何战斗定义\n");
        return 0;
    }
    printf("BATTLE CHECK %s (%d 场，失败 %d)\n", failed ? "FAILED" : "OK", total, failed);
    return failed ? 1 : 0;
}

// 素材自检：把所有剧本里引用到的素材路径都揪出来，逐个检查
//   * 存在性（缺文件 / 路径含中文）
//   * 可加载性（图片能不能解出来、音频能不能解码，防止坏文件）
int checkAssets()
{
    std::vector<std::string> paths;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("assets/scripts", ec))
    {
        if (!entry.is_regular_file(ec)) continue;
        std::string p = entry.path().string();
        if (p.size() < 4 || p.substr(p.size() - 4) != ".gal") continue;
        std::string stem = entry.path().stem().string();
        if (stem.rfind("test", 0) == 0 || stem[0] == '_') continue;

        for (const auto& line : splitLines(readFile(p)))
        {
            // 跳过整行注释；行内注释也切掉（`#RRGGBB` 这种颜色除外，跟分词器保持一致），
            // 否则被注释掉的素材引用会被当成"缺失"报出来
            size_t hash = std::string::npos;
            for (size_t k = 0; k < line.size(); ++k)
            {
                if (line[k] != '#') continue;
                bool isColor = k + 6 < line.size();
                for (size_t j = 1; isColor && j <= 6; ++j)
                {
                    char c = line[k + j];
                    if (!std::isxdigit(static_cast<unsigned char>(c))) isColor = false;
                }
                if (!isColor) { hash = k; break; }
            }
            std::string code = hash == std::string::npos ? line : line.substr(0, hash);
            if (code.find_first_not_of(" \t") == std::string::npos) continue;

            size_t at = code.find("assets/");
            while (at != std::string::npos)
            {
                size_t end = at;
                while (end < code.size() && code[end] != ' ' && code[end] != '\t' &&
                       code[end] != '"' && code[end] != '\'' && code[end] != '#')
                    ++end;
                paths.push_back(code.substr(at, end - at));
                at = code.find("assets/", end);
            }
        }
    }
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    int missing = 0, broken = 0;
    std::vector<std::string> audio;
    InitAudioDevice();                       // 没有声卡时会失败，那就只查文件在不在
    const bool canAudio = IsAudioDeviceReady();
    for (const auto& path : paths)
    {
        if (!FileExists(path.c_str()))
        {
            printf("  ✗ 缺失: %s\n", path.c_str());
            ++missing;
            continue;
        }
        const bool isImg = path.find(".png") != std::string::npos ||
                           path.find(".jpg") != std::string::npos ||
                           path.find(".gif") != std::string::npos;
        const bool isAudio = path.find(".wav") != std::string::npos ||
                             path.find(".ogg") != std::string::npos ||
                             path.find(".mp3") != std::string::npos;
        if (isImg)
        {
            Image img = LoadImage(path.c_str());
            if (img.data == nullptr)
            {
                printf("  ✗ 打不开: %s\n", path.c_str());
                ++broken;
            }
            else
            {
                UnloadImage(img);
            }
        }
        else if (isAudio && canAudio)
        {
            Music m = LoadMusicStream(path.c_str());
            bool ok = m.frameCount > 0;
            if (m.frameCount > 0) UnloadMusicStream(m);
            if (!ok)
            {
                Sound s = LoadSound(path.c_str());   // 短音效按 Sound 再试一次
                ok = s.frameCount > 0;
                if (s.frameCount > 0) UnloadSound(s);
            }
            if (!ok)
            {
                printf("  ✗ 解不开: %s\n", path.c_str());
                ++broken;
            }
            else
            {
                audio.push_back(path);
            }
        }
    }
    if (canAudio) CloseAudioDevice();

    printf("素材自检: %zu 个引用%s，缺失 %d，损坏 %d%s\n", paths.size(),
           canAudio ? "" : "（音频只查存在性：没有可用音频设备）", missing, broken,
           canAudio ? "" : "");
    if (!audio.empty()) printf("  音频可解码 %zu 个\n", audio.size());
    if (missing || broken) return 1;
    printf("ASSET CHECK OK\n");
    return 0;
}

// 无窗口打印剧情树：主线连成一条脊，支线挂在各自的解锁点下面。
// 用来核对 branch / order / requires 这三条元信息，以及当前进度下的解锁状态。
int printTree()
{
    struct Item
    {
        std::string path, title, series, branch, subtitle, needScript;
        std::map<int, std::string> chNames;
        std::vector<std::pair<std::string, int>> reqs;
        int order = 0, need = 0, chapters = 0, viewed = 0;
    };
    std::vector<Item> items;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("assets/scripts", ec))
    {
        if (!entry.is_regular_file(ec)) continue;
        std::string p = entry.path().string();
        std::replace(p.begin(), p.end(), '\\', '/');
        if (p.size() < 4 || p.substr(p.size() - 4) != ".gal") continue;
        std::string stem = entry.path().stem().string();
        if (stem.rfind("test", 0) == 0 || stem[0] == '_') continue;
        std::string text = readFile(p);
        if (text.empty()) continue;
        Script s;
        try
        {
            s = parseGal(text, p);
        }
        catch (const ScriptError& e)
        {
            printf("  !! 解析失败 %s: %s\n", p.c_str(), e.what());
            continue;
        }
        Item it;
        it.path = p;
        it.title = s.title.empty() ? stem : s.title;
        it.series = s.series.empty() ? it.title : s.series;
        it.branch = s.branch.empty() ? "main" : s.branch;
        it.subtitle = s.subtitle;
        it.order = s.order;
        it.needScript = s.requiresScript;
        it.need = s.requiresChapter;
        it.reqs = s.requiresList;
        it.chapters = static_cast<int>(s.chapters.size());
        for (const auto& ch : s.chapters)
        {
            it.chNames[ch.id] = ch.name;
            if (progressIsViewed(p, ch.id)) ++it.viewed;
        }
        items.push_back(std::move(it));
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.series != b.series) return a.series < b.series;
        bool am = a.branch != "side", bm = b.branch != "side";
        if (am != bm) return am;
        if (a.order != b.order) return a.order < b.order;
        return a.path < b.path;
    });

    auto find = [&](const std::string& stem) -> const Item* {
        for (const auto& it : items)
        {
            std::string s = it.path;
            size_t sl = s.find_last_of('/');
            if (sl != std::string::npos) s = s.substr(sl + 1);
            if (s.size() > 4) s = s.substr(0, s.size() - 4);
            if (s == stem) return &it;
        }
        return nullptr;
    };

    int totalCh = 0, totalViewed = 0;
    for (const auto& it : items) { totalCh += it.chapters; totalViewed += it.viewed; }
    printf("剧情树（%zu 个大章节 / %d 个小章节，已看 %d）\n", items.size(), totalCh, totalViewed);

    for (const char* br : {"main", "side"})
    {
        printf("\n%s\n", br[0] == 'm' ? "主线" : "支线");
        for (const auto& it : items)
        {
            bool isMain = it.branch != "side";
            if ((br[0] == 'm') != isMain) continue;

            std::string hint;
            for (const auto& req : it.reqs)
            {
                const Item* pre = find(req.first);
                int need = req.second > 0 ? req.second : (pre ? pre->chapters : 0);
                if (need <= 0) continue;
                if (progressIsViewed("assets/scripts/" + req.first + ".gal", need)) continue;
                std::string chName = std::to_string(need);
                if (pre)
                {
                    auto it2 = pre->chNames.find(need);
                    if (it2 != pre->chNames.end()) chName = it2->second;
                }
                std::string one = "「" + std::string(pre ? pre->title : req.first) + "」" + chName;
                hint = hint.empty() ? ("  ← 未解锁：需要 " + one)
                                    : (hint + "  +  " + one);
            }
            std::string line = "  ├─ " + it.title;
            if (!it.subtitle.empty()) line += "  · " + it.subtitle;
            line += "（" + std::to_string(it.viewed) + "/" + std::to_string(it.chapters) + " 章）";
            line += hint;
            printf("%s\n", line.c_str());
        }
    }
    return 0;
}

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
            case VM::State::WaitBattle:
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
    printf("  --script <文件>       指定 .gal 脚本（默认 assets/scripts/star_festival.gal）\n");
    printf("  --tree                无窗口打印剧情树（主线 / 支线 / 解锁状态）\n");
    printf("  --direct              开始游戏直接进默认脚本第一章（跳过章节选择，调试用）\n");
    printf("  --check-assets        无窗口自检素材：存在性 + 图片/音频可解码\n");
    printf("  --check-battle        无窗口自检观测战：每场战斗都能正常打完\n");
    printf("  --check-gesture       无窗口自检触摸板手势（合成事件，不需要真设备）\n");
    printf("  --gesture-monitor     实时打印读到的触摸板设备与手势数值（排查捏合/拖动）\n");
    printf("  --check-script        无窗口自检脚本，验证可执行到 game_end\n");
    printf("  --check-save          无窗口自检存档/读档（中途存档后恢复并跑到结尾）\n");
    printf("  --check-chapters      无窗口自检每个章节都能从起点执行到结束\n");
    printf("  --selftest [--frames N]  自动运行窗口自检（默认 20000 帧）\n");
    printf("  --size <宽x高>        指定初始窗口大小（默认 1280x720，可随时拖动缩放）\n");
}
}

int main(int argc, char** argv)
{
    setCwdToExe(argv[0]);

    std::string scriptPath = "assets/scripts/star_festival.gal";
    bool selftest = false;
    bool check = false;
    bool checkSave = false;
    bool checkChapters = false;
    bool tree = false;
    bool checkAssetsFlag = false;
    bool direct = false;
    bool checkBattleFlag = false;
    bool checkGestureFlag = false;
    bool gestureMonitorFlag = false;
    int frames = 20000;
    int winW = 1280;
    int winH = 720;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--script" && i + 1 < argc) scriptPath = argv[++i];
        else if (a == "--tree") tree = true;
        else if (a == "--check-assets") checkAssetsFlag = true;
        else if (a == "--direct") direct = true;
        else if (a == "--check-battle") checkBattleFlag = true;
        else if (a == "--check-gesture") checkGestureFlag = true;
        else if (a == "--gesture-monitor") gestureMonitorFlag = true;
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
        else if (a == "--size" && i + 1 < argc)
        {
            int w = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0)
            {
                winW = w;
                winH = h;
            }
        }
        else if (a == "--help" || a == "-h") { printUsage(); return 0; }
    }

    if (check) return checkScript(scriptPath);
    if (checkSave) return checkSaveLoad(scriptPath);
    if (checkChapters) return checkChapterScripts(scriptPath);
    if (tree) return printTree();
    if (checkAssetsFlag) return checkAssets();
    if (checkBattleFlag) return checkBattles();
    if (checkGestureFlag) return checkGesture();
    if (gestureMonitorFlag) return gestureMonitor();

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
    // 先载入语言文件：界面文案与字体字形都以它为准
    lang::load("assets/lang/zh_CN.lang");
    gesture::init();                     // 触摸板手势：读不到设备就自动降级
    std::vector<std::string> texts = uiTexts();
    for (const auto& line : splitLines(text)) texts.push_back(line);
    // 字体字形按「实际用到的文字」生成（见 core/font.cpp）：把所有剧本的文本都收进来，
    // 这样某个脚本里的字不会因为没被光栅化而缺字。
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("assets/scripts", ec))
        {
            if (!entry.is_regular_file(ec)) continue;
            std::string p = entry.path().string();
            std::replace(p.begin(), p.end(), '\\', '/');
            if (p.size() < 4 || p.substr(p.size() - 4) != ".gal") continue;
            for (const auto& line : splitLines(readFile(p))) texts.push_back(line);
        }
    }

    Engine engine;
    // 窗口标题也跟着语言文件走（title.main 缺省时退回引擎名）
    std::string winTitle = lang::tr("title.main");
    if (winTitle.empty() || winTitle == "title.main") winTitle = "QLWT";
    winTitle += "  ·  QLWT ENGINE";
    if (!engine.init(winW, winH, winTitle.c_str(), texts, selftest, frames))
    {
        printf("failed to initialize window\n");
        return 1;
    }
    progressEnsureCreated();   // 开始游戏时确保章节进度文件存在
    engine.switchScene(std::make_shared<TitleScene>(&engine, scriptPath, script, parseError, direct));
    engine.run();

    return selftest ? (engine.selftestPassed ? 0 : 2) : 0;
}
