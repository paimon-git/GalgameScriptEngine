#include "game_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

#include "../core/engine.h"
#include "../renderer/renderer.h"
#include "chapter_select.h"
#include "title_scene.h"

GameScene::GameScene(Engine* e, std::string scriptPath,
                     std::shared_ptr<Script> script, std::string parseError,
                     int startChapter)
    : Scene(e),
      scriptPath_(std::move(scriptPath)),
      parseError_(std::move(parseError)),
      script_(std::move(script))
{
    if (parseError_.empty() && script_)
    {
        if (startChapter >= 0 &&
            startChapter < static_cast<int>(script_->chapters.size()))
        {
            const auto& ch = script_->chapters[startChapter];
            currentChapter_ = startChapter;
            vm_.load(*script_);
            vm_.startAt(ch.stmtIndex, game_);
            vm_.step(game_);
            printf("[scene] chapter %d '%s' started\n", ch.id, ch.name.c_str());
        }
        else
        {
            start();
        }
    }
    else
    {
        printf("[scene] game scene opened with script error\n");
    }
}

void GameScene::start()
{
    vm_.load(*script_);
    vm_.step(game_);
    printf("[scene] game started: %zu statements\n", script_->stmts.size());
}

void GameScene::backToTitle()
{
    engine()->switchScene(std::make_shared<TitleScene>(
        engine(), scriptPath_, script_, parseError_));
}

void GameScene::queueShot(const std::string& tag)
{
    if (!pendingShot_.empty()) return;
    pendingShot_ = tag;
    int delay = 3;
    if (tag == "card") delay = 24;                          // 等待卡片淡入完成
    else if (tag == "narrate") delay = 16;                  // 旁白淡入约 0.3 秒
    else if (tag.rfind("cg", 0) == 0) delay = 30;           // CG 淡入 0.5 秒
    else if (tag == "choice") delay = 38;                   // 等待提问打完字 + 选项完全淡入
    shotAtFrame_ = engine()->time.frame() + delay;
}

void GameScene::syncToVM()
{
    const Font& font = engine()->fonts.font();
    float w = static_cast<float>(GetScreenWidth());

    switch (vm_.state())
    {
    case VM::State::WaitSay:
        if (!dlg_.active() && vm_.say())
        {
            dlg_.open(vm_.say()->character, vm_.say()->text,
                      game_.colorOf(vm_.say()->character), font, w);
            if (dlgShotCount_++ == 0) queueShot("dialogue");   // 只截第一句，保证是明亮场景
        }
        break;
    case VM::State::WaitChoice:
        if (!choice_.active() && vm_.choice())
        {
            // 提问显示在正常对话框里，选项浮在对话框上方
            const ChoiceStmt* c = vm_.choice();
            dlg_.open(c->character, c->text,
                      game_.colorOf(c->character), font, w);
            choice_.open(c->character, c->text, c->options);
            queueShot("choice");
        }
        break;
    default:
        break;
    }
}

void GameScene::userAdvance()
{
    if (!dlg_.active()) return;
    if (dlg_.advance())   // 打字已完成，推进剧情
    {
        addLog(dlg_.name(), dlg_.text());
        dlg_.close();
        vm_.step(game_);
        autoTimer_ = 0.0f;
    }
}

void GameScene::choose(int idx)
{
    const ChoiceStmt* c = vm_.choice();
    if (!c || idx < 0 || idx >= static_cast<int>(c->options.size())) return;
    const auto& opt = c->options[idx];
    addLog(c->character, c->text + "　→　" + opt.first);
    choice_.close();
    dlg_.close();
    vm_.jumpTo(opt.second, game_);
    autoTimer_ = 0.0f;
}

void GameScene::addLog(const std::string& name, const std::string& text)
{
    log_.push_back({name, text, game_.colorOf(name)});
    if (log_.size() > 300) log_.erase(log_.begin());
}

void GameScene::update(float dt)
{
    // 章节结束：自动淡出并进入下一章
    if (chapterAdvancing_)
    {
        chapterAdvanceFade_ += dt / 0.35f;
        if (chapterAdvanceFade_ >= 1.0f && !chapterSwitchSent_)
        {
            int next = currentChapter_ + 1;
            chapterSwitchSent_ = true;
            printf("[scene] auto next chapter %d\n", next + 1);
            engine()->switchScene(std::make_shared<GameScene>(
                engine(), scriptPath_, script_, parseError_, next));
            return;
        }
        return;
    }

    // 章节结束询问：继续下一章 / 返回章节选择
    if (chapterPrompt_)
    {
        float w = static_cast<float>(GetScreenWidth());
        float h = static_cast<float>(GetScreenHeight());
        Vector2 mouse = GetMousePosition();
        Rectangle btn1{(w - 430.0f) * 0.5f, h * 0.5f + 42.0f, 190.0f, 54.0f};
        Rectangle btn2{(w + 50.0f) * 0.5f, h * 0.5f + 42.0f, 190.0f, 54.0f};
        bool h1 = CheckCollisionPointRec(mouse, btn1);
        bool h2 = CheckCollisionPointRec(mouse, btn2);
        float speed = 7.0f * GetFrameTime();
        for (int i = 0; i < 2; ++i)
        {
            bool target = (i == 0) ? h1 : h2;
            if (target > promptHover_[i])
            {
                promptHover_[i] += speed;
                if (promptHover_[i] >= target) promptHover_[i] = target;
            }
            else
            {
                promptHover_[i] -= speed;
                if (promptHover_[i] <= target) promptHover_[i] = target;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (h1)
            {
                chapterPrompt_ = false;
                chapterAdvancing_ = true;
                chapterAdvanceFade_ = 0.0f;
            }
            else if (h2)
            {
                chapterPrompt_ = false;
                engine()->switchScene(std::make_shared<ChapterSelectScene>(
                    engine(), scriptPath_, parseError_));
            }
        }
        if (IsKeyPressed(KEY_ENTER))
        {
            chapterPrompt_ = false;
            chapterAdvancing_ = true;
            chapterAdvanceFade_ = 0.0f;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            chapterPrompt_ = false;
            engine()->switchScene(std::make_shared<ChapterSelectScene>(
                engine(), scriptPath_, parseError_));
        }
        return;
    }

    inputCooldown_ = std::max(0.0f, inputCooldown_ - dt);
    game_.update(dt);
    if (engine()->selftest && game_.cgFullyShown() && !prevCgShown_)
        if (pendingShot_.empty()) queueShot("cg" + std::to_string(++cgShot_));
    prevCgShown_ = game_.cgFullyShown();
    if (fadeAlpha_ > 0.0f)
        fadeAlpha_ = std::max(0.0f, fadeAlpha_ - dt * 1.6f);

    const Input& input = engine()->input;

    if (!parseError_.empty() || vm_.state() == VM::State::Error)
    {
        if (inputCooldown_ <= 0.0f && input.advancePressed()) backToTitle();
        return;
    }

    if (ended_)
    {
        endTimer_ += dt;
        if (endTimer_ > 1.0f && inputCooldown_ <= 0.0f && input.advancePressed()) backToTitle();
        return;
    }

    // 存档 / 读档菜单（暂停游戏）
    if (saveMenu_ || loadMenu_)
    {
        handleMenu(dt);
        return;
    }
    if (IsKeyPressed(KEY_F5) && !choice_.active())
    {
        openSaveMenu();
        return;
    }
    if (IsKeyPressed(KEY_F9))
    {
        openLoadMenu();
        return;
    }

    if (logOpen_)
    {
        if (IsKeyPressed(KEY_L) || IsKeyPressed(KEY_ESCAPE)) logOpen_ = false;
        int wheel = GetMouseWheelMove();
        if (wheel != 0 || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT))
            ++logScroll_;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT))
            logScroll_ = std::max(0, logScroll_ - 1);
        return;
    }

    // Esc 暂停菜单
    if (escMenu_)
    {
        handleEscMenu();
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE))
    {
        escMenu_ = true;
        return;
    }

    if (IsKeyPressed(KEY_L))
    {
        logOpen_ = true;
        logScroll_ = 0;
        return;
    }
    if (IsKeyPressed(KEY_A)) autoMode_ = !autoMode_;

    bool skip = input.ctrlDown();

    // 顶部旁白（无对话框）
    if (vm_.state() == VM::State::WaitNarrate)
    {
        if (!narrateActive_ && vm_.narrate())
        {
            narrateText_ = vm_.narrate()->text;
            narrateActive_ = true;
            narrateAlpha_ = 0.0f;
            narrateHold_ = engine()->time.frame() + 20;
            queueShot("narrate");
        }
        narrateAlpha_ = std::min(1.0f, narrateAlpha_ + dt * 3.0f);
        if (inputCooldown_ <= 0.0f && input.advancePressed())
        {
            narrateActive_ = false;
            vm_.step(game_);
        }
        return;
    }

    // 章节 / 地点卡片（自动淡入淡出，点击跳过）
    if (vm_.state() == VM::State::WaitCard)
    {
        if (!cardActive_ && vm_.card())
        {
            cardText_ = vm_.card()->text;
            cardActive_ = true;
            cardT_ = 0.0f;
            cardClosing_ = false;
            cardCloseT_ = 0.0f;
            queueShot("card");
        }
        if (cardActive_)
        {
            if (cardClosing_)
            {
                cardCloseT_ += dt;
                if (cardCloseT_ >= 0.25f)
                {
                    cardActive_ = false;
                    cardClosing_ = false;
                    vm_.step(game_);
                }
            }
            else
            {
                cardT_ += dt;
                // 章节卡显示停顿：淡入完成后等待点击（至少停留 0.9 秒）
                if (cardT_ >= 0.9f && inputCooldown_ <= 0.0f && input.advancePressed())
                {
                    cardClosing_ = true;
                    cardCloseT_ = 0.0f;
                }
            }
        }
        return;
    }

    // 角色消失：黑屏淡入 -> 角色完全隐藏 -> 黑屏淡出，之后才继续切背景
    if (vm_.state() == VM::State::WaitHide)
    {
        if (hideName_.empty()) hideName_ = vm_.hideName();
        hideT_ += dt;
        const float t1 = 0.12f;    // 黑屏淡入
        const float t2 = 0.20f;    // 黑屏保持 + 角色消失
        const float t3 = 0.12f;    // 黑屏淡出
        if (hideT_ < t1)
        {
            hideBlack_ = hideT_ / t1;
        }
        else if (hideT_ < t1 + t2)
        {
            hideBlack_ = 1.0f;
            if (!hideCleared_)
            {
                game_.forceHideCharacter(hideName_);
                hideCleared_ = true;
                if (engine()->selftest && pendingShot_.empty())
                {
                    pendingShot_ = "hide_black";
                    shotAtFrame_ = engine()->time.frame() + 2;
                }
            }
        }
        else if (hideT_ < t1 + t2 + t3)
        {
            hideBlack_ = 1.0f - (hideT_ - t1 - t2) / t3;
        }
        else
        {
            hideBlack_ = 0.0f;
            hideCleared_ = false;
            hideName_.clear();
            hideT_ = 0.0f;
            vm_.step(game_);
        }
        return;
    }

    // 全屏 CG：视频播完 / 图片 3 秒后自动继续；点击可跳过
    if (vm_.state() == VM::State::WaitCg)
    {
        if (game_.cgAutoFinished())
        {
            game_.resetCgAuto();
            vm_.step(game_);
        }
        else if (inputCooldown_ <= 0.0f && input.advancePressed())
        {
            game_.hideCg();   // 点击跳过 CG
        }
        return;
    }

    syncToVM();

    if (choice_.active())
    {
        // 提问照常打字显示；打字完成后选项才淡入并可交互
        if (dlg_.active())
        {
            float speed = static_cast<float>(engine()->settings.textSpeed);
            dlg_.update(dt, speed, skip);
            if (!dlg_.typingFinished() && inputCooldown_ <= 0.0f && input.advancePressed())
                dlg_.advance();   // 打字中点击 = 显示全文，不推进剧情
        }
        if (!dlg_.active() || dlg_.typingFinished())
        {
            int sel = choice_.update();
            if (sel >= 0) choose(sel);
        }
        return;
    }

    if (dlg_.active())
    {
        float speed = static_cast<float>(engine()->settings.textSpeed);
        dlg_.update(dt, speed, skip);

        if (inputCooldown_ <= 0.0f && input.advancePressed())
        {
            userAdvance();
        }
        else if (skip)
        {
            autoTimer_ += dt * 12.0f;
            if (autoTimer_ > 0.35f) userAdvance();
        }
        else if (autoMode_ && dlg_.typingFinished())
        {
            autoTimer_ += dt;
            if (autoTimer_ > 1.6f) userAdvance();
        }
        return;
    }

    if (vm_.state() == VM::State::WaitTimer)
    {
        vm_.tick(dt);
        if (vm_.state() == VM::State::Running) vm_.step(game_);
    }
    else if (vm_.state() == VM::State::Running)
    {
        vm_.step(game_);
    }

    if (vm_.state() == VM::State::Ended)
    {
        if (currentChapter_ >= 0 && !chapterViewedMarked_)
        {
            progressMarkViewed(scriptPath_, script_->chapters[currentChapter_].id);
            chapterViewedMarked_ = true;
        }
        // 非最后一章：自动进入下一章；最后一章才显示结局
        if (currentChapter_ >= 0 &&
            currentChapter_ + 1 < static_cast<int>(script_->chapters.size()))
        {
            if (!chapterPrompt_ && !chapterAdvancing_)
            {
                chapterPrompt_ = true;
                printf("[scene] chapter %d end -> prompt\n", currentChapter_ + 1);
                if (engine()->selftest && pendingShot_.empty())
                {
                    pendingShot_ = "chapter_prompt";
                    shotAtFrame_ = engine()->time.frame() + 2;
                }
            }
        }
        else if (!ended_)
        {
            ended_ = true;
            endTimer_ = 0.0f;
            printf("[scene] game end\n");
        }
    }
}

void GameScene::draw()
{
    const Font& font = engine()->fonts.font();
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());

    game_.draw();
    if (narrateActive_) drawNarrate();
    if (cardActive_) drawCard();
    if (dlg_.active()) dlg_.draw(font, w, h, static_cast<float>(engine()->time.elapsed()));
    if (choice_.active()) choice_.draw(font, w, h);
    if (logOpen_) drawLog();

    // 状态指示
    if (autoMode_ && dlg_.active() && !choice_.active())
    {
        renderer::drawPanel({w - 172.0f, 22.0f, 148.0f, 40.0f},
                            Color{20, 24, 40, 190}, renderer::kCornerRadius,
                            Color{120, 160, 255, 120}, 1.5f);
        engine()->fonts.draw("自动播放", w - 150.0f, 31.0f, 20.0f,
                             Color{170, 200, 255, 255}, 2.0f);
    }
    if (engine()->input.ctrlDown() && dlg_.active() && !choice_.active())
    {
        renderer::drawPanel({22.0f, 22.0f, 148.0f, 40.0f},
                            Color{20, 24, 40, 190}, renderer::kCornerRadius,
                            Color{255, 170, 90, 150}, 1.5f);
        engine()->fonts.draw("快进中", 44.0f, 31.0f, 20.0f,
                             Color{255, 200, 150, 255}, 2.0f);
    }

    if (dlg_.active() && !choice_.active() && !saveMenu_ && !loadMenu_ && !escMenu_)
    {
        engine()->fonts.draw("Esc 菜单 · F5 存档 · F9 读档", w - 290.0f, h - 32.0f, 16.0f,
                             Color{255, 255, 255, 120}, 1.6f);
    }

    drawOverlays();

    if (saveMenu_ || loadMenu_) drawMenu();
    if (escMenu_ && !saveMenu_ && !loadMenu_) drawEscMenu();

    // 章节结束询问面板
    if (chapterPrompt_)
    {
        float pw = 560.0f;
        float ph = 300.0f;
        float px = (w - pw) * 0.5f;
        float py = (h - ph) * 0.5f;
        const Font& font = engine()->fonts.font();

        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h), Color{0, 0, 0, 150});
        renderer::drawPanel({px, py, pw, ph}, Color{16, 20, 32, 248},
                            renderer::kCornerRadius, Color{255, 255, 255, 55}, 2.0f);
        renderer::drawAccentLine({px + 36.0f, py + 14.0f, pw - 72.0f, 4.0f},
                                 Color{255, 205, 90, 255}, 3.0f);
        engine()->fonts.draw("本章结束", px + 40.0f, py + 38.0f, 34.0f,
                             Color{255, 255, 255, 255}, 3.4f);
        engine()->fonts.draw("是否进入下一章？", px + 40.0f, py + 108.0f, 26.0f,
                             Color{220, 226, 242, 255}, 2.6f);

        Rectangle btn1{(w - 430.0f) * 0.5f, h * 0.5f + 42.0f, 190.0f, 54.0f};
        Rectangle btn2{(w + 50.0f) * 0.5f, h * 0.5f + 42.0f, 190.0f, 54.0f};
        Vector2 mouse = GetMousePosition();
        renderer::drawButton(btn1, "继续下一章", font, 22.0f, promptHover_[0],
                             CheckCollisionPointRec(mouse, btn1),
                             IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, btn1),
                             true, Color{255, 205, 90, 255});
        renderer::drawButton(btn2, "返回章节选择", font, 22.0f, promptHover_[1],
                             CheckCollisionPointRec(mouse, btn2),
                             IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, btn2));
        engine()->fonts.draw("Enter 继续  ·  Esc 返回", px + 40.0f, py + ph - 44.0f,
                             18.0f, Color{150, 160, 185, 255}, 1.8f);
    }

    // 章节切换淡出
    if (chapterAdvanceFade_ > 0.0f)
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                      Color{0, 0, 0,
                            static_cast<unsigned char>(chapterAdvanceFade_ * 255.0f)});
}

void GameScene::afterDraw()
{
    // 自检：本帧绘制完成后再截图，保证画面包含刚打开的 UI
    if (!pendingShot_.empty() && engine()->time.frame() >= shotAtFrame_)
    {
        engine()->selftestShot(pendingShot_);
        pendingShot_.clear();
    }
}

void GameScene::drawNarrate() const
{
    float w = static_cast<float>(GetScreenWidth());
    const Font& font = engine()->fonts.font();
    float nv = renderer::easeInOut(narrateAlpha_);
    unsigned char a = static_cast<unsigned char>(nv * 255.0f);

    // 顶部旁白文字（居中，自动换行）
    float maxW = w * 0.78f;
    float lineH = 38.0f;
    float y = 74.0f;
    float panelY = y - 18.0f;
    float panelH = lineH + 12.0f;
    // 顶部压暗带：贴合旁白面板边界，避免阴影在面板下方拖出一截
    renderer::drawGradientV({0, 0, w, panelY + panelH + 12.0f},
                            Color{0, 0, 0, static_cast<unsigned char>(110 * nv)},
                            Color{0, 0, 0, 0}, 48);
    renderer::drawPanel({w * 0.11f, panelY, w * 0.78f, panelH},
                        Color{10, 14, 26, static_cast<unsigned char>(90 * nv)},
                        renderer::kCornerRadius, Color{255, 255, 255, 0}, 0.0f);
    std::string line;
    float size = 26.0f;
    float spacing = 2.6f;
    float cursorY = y;
    size_t i = 0;
    while (i <= narrateText_.size())
    {
        if (i == narrateText_.size())
        {
            if (!line.empty())
            {
                float lx = (w - MeasureTextEx(font, line.c_str(), size, spacing).x) * 0.5f;
                DrawTextEx(font, line.c_str(), {lx, cursorY}, size, spacing,
                           Color{255, 255, 255, a});
            }
            break;
        }
        unsigned char c = static_cast<unsigned char>(narrateText_[i]);
        int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 :
                  ((c & 0xF0) == 0xE0) ? 3 : 4;
        std::string ch = narrateText_.substr(i, len);
        i += len;
        if (ch == "\n")
        {
            float lx = (w - MeasureTextEx(font, line.c_str(), size, spacing).x) * 0.5f;
            DrawTextEx(font, line.c_str(), {lx, cursorY}, size, spacing,
                       Color{255, 255, 255, a});
            line.clear();
            cursorY += lineH;
            continue;
        }
        float lw = MeasureTextEx(font, (line + ch).c_str(), size, spacing).x;
        if (lw > maxW && !line.empty())
        {
            float lx = (w - MeasureTextEx(font, line.c_str(), size, spacing).x) * 0.5f;
            DrawTextEx(font, line.c_str(), {lx, cursorY}, size, spacing,
                       Color{255, 255, 255, a});
            line = ch;
            cursorY += lineH;
        }
        else
        {
            line += ch;
        }
    }
}

void GameScene::drawCard() const
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    const Font& font = engine()->fonts.font();

    float a;
    if (cardClosing_)
        a = renderer::easeInOut(1.0f - std::min(1.0f, cardCloseT_ / 0.25f));
    else
        a = renderer::easeInOut(std::min(1.0f, cardT_ / 0.35f));

    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                  Color{6, 10, 20, static_cast<unsigned char>(150 * a)});

    float size = 46.0f;
    Vector2 m = MeasureTextEx(font, cardText_.c_str(), size, 4.6f);
    float cx = (w - m.x) * 0.5f;
    float cy = h * 0.5f - m.y * 0.5f;
    Color text{255, 255, 255, static_cast<unsigned char>(255 * a)};

    // 两侧装饰线（蓝档案风格）
    float lineW = 90.0f;
    float lineY = cy + m.y * 0.5f + 6.0f;
    Color line{120, 170, 255, static_cast<unsigned char>(220 * a)};
    DrawRectangleRounded({cx - lineW - 44.0f, lineY, lineW, 3.0f}, 1.5f, 4, line);
    DrawRectangleRounded({cx + m.x + 44.0f, lineY, lineW, 3.0f}, 1.5f, 4, line);

    DrawTextEx(font, cardText_.c_str(), {cx, cy}, size, 4.6f, text);
}

void GameScene::drawLog() const
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    float lw = w * 0.86f;
    float lh = h * 0.74f;
    float lx = (w - lw) * 0.5f;
    float ly = (h - lh) * 0.5f;
    const Font& font = engine()->fonts.font();

    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h), Color{0, 0, 0, 160});
    renderer::drawPanel({lx, ly, lw, lh}, Color{12, 14, 24, 242}, renderer::kCornerRadius,
                        Color{255, 255, 255, 40}, 2.0f);
    engine()->fonts.draw("历史记录（L / Esc 关闭）", lx + 40.0f, ly + 26.0f, 26.0f,
                         Color{255, 255, 255, 255}, 2.6f);

    float rowH = 50.0f;
    float topY = ly + 86.0f;
    int visible = static_cast<int>((lh - 140.0f) / rowH);
    int maxScroll = std::max(0, static_cast<int>(log_.size()) - visible);
    int scroll = std::min(logScroll_, maxScroll);

    int startIdx = static_cast<int>(log_.size()) - 1 - scroll;
    for (int i = 0; i < visible && startIdx - i >= 0; ++i)
    {
        const LogEntry& e = log_[startIdx - i];
        float ry = topY + i * rowH;
        engine()->fonts.draw(e.name, lx + 42.0f, ry + 10.0f, 22.0f, e.color, 2.2f);

        // 截断过长的文本
        std::string text = e.text;
        float maxW = lw - 190.0f;
        Vector2 m = MeasureTextEx(font, text.c_str(), 22.0f, 2.2f);
        while (m.x > maxW && !text.empty())
        {
            size_t n = text.size() - 1;
            while (n > 0 && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80) --n;
            text.erase(n);
            m = MeasureTextEx(font, text.c_str(), 22.0f, 2.2f);
        }
        engine()->fonts.draw(text, lx + 150.0f, ry + 10.0f, 22.0f,
                             Color{225, 228, 240, 255}, 2.2f);
    }
    engine()->fonts.draw("↑ 查看更早  ·  ↓ 返回最新", lx + 40.0f, ly + lh - 44.0f,
                         18.0f, Color{160, 170, 195, 255}, 1.8f);
}

void GameScene::drawOverlays() const
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    const Font& font = engine()->fonts.font();

    // 角色消失黑屏过渡
    if (hideBlack_ > 0.0f)
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                      Color{0, 0, 0, static_cast<unsigned char>(renderer::easeInOut(hideBlack_) * 255.0f)});

    // 开场淡入
    if (fadeAlpha_ > 0.0f)
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                      Color{0, 0, 0, static_cast<unsigned char>(renderer::easeInOut(fadeAlpha_) * 255.0f)});

    // 结束画面
    if (ended_)
    {
        unsigned char a = static_cast<unsigned char>(std::min(200.0f, endTimer_ * 280.0f));
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                      Color{4, 6, 12, a});
        if (endTimer_ > 0.6f)
        {
            std::string title = script_ ? script_->title : std::string();
            engine()->fonts.draw("END", (w - 150.0f) * 0.5f, h * 0.36f, 76.0f,
                                 Color{255, 255, 255, 255}, 7.6f, true);
            if (!title.empty())
                engine()->fonts.draw(title, (w - MeasureTextEx(font, title.c_str(), 30.0f, 3.0f).x) * 0.5f,
                                     h * 0.53f, 30.0f, Color{200, 210, 235, 230}, 3.0f);
            if (endTimer_ > 1.2f)
            {
                float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(engine()->time.elapsed()) * 3.0f);
                engine()->fonts.draw("点击任意处返回标题",
                                     (w - MeasureTextEx(font, "点击任意处返回标题", 22.0f, 2.2f).x) * 0.5f,
                                     h * 0.62f, 22.0f,
                                     Color{255, 255, 255, static_cast<unsigned char>(120 + 120 * pulse)},
                                     2.2f);
            }
        }
    }

    // 脚本错误
    if (!parseError_.empty() || vm_.state() == VM::State::Error)
    {
        std::string msg = parseError_.empty() ? vm_.error() : parseError_;
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h), Color{30, 8, 10, 215});
        renderer::drawPanel({(w - 700.0f) * 0.5f, h * 0.38f, 700.0f, 150.0f},
                            Color{50, 16, 22, 245}, renderer::kCornerRadius,
                            Color{255, 100, 100, 140}, 2.0f);
        engine()->fonts.draw("脚本错误", (w - MeasureTextEx(font, "脚本错误", 34.0f, 3.4f).x) * 0.5f,
                             h * 0.38f + 28.0f, 34.0f, Color{255, 150, 150, 255}, 3.4f);
        engine()->fonts.draw(msg, (w - MeasureTextEx(font, msg.c_str(), 22.0f, 2.2f).x) * 0.5f,
                             h * 0.38f + 88.0f, 22.0f, Color{255, 210, 210, 255}, 2.2f);
        engine()->fonts.draw("点击返回标题",
                             (w - MeasureTextEx(font, "点击返回标题", 20.0f, 2.0f).x) * 0.5f,
                             h * 0.38f + 128.0f, 20.0f, Color{255, 255, 255, 180}, 2.0f);
    }
}

namespace
{
std::string saveTimeString()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%m-%d %H:%M", &tm);
    return buf;
}
}

void GameScene::openSaveMenu()
{
    if (choice_.active()) return;   // 选择中不存档
    saveMenu_ = true;
    loadMenu_ = false;
    menuHover_ = -1;
    menuHoverAnim_.assign(kSaveSlots, 0.0f);
    slotData_.clear();
    for (int i = 1; i <= kSaveSlots; ++i)
    {
        SaveData d;
        loadSlot(i, d);
        slotData_.push_back(d);
    }
}

void GameScene::openLoadMenu()
{
    loadMenu_ = true;
    saveMenu_ = false;
    menuHover_ = -1;
    menuHoverAnim_.assign(kSaveSlots, 0.0f);
    slotData_.clear();
    for (int i = 1; i <= kSaveSlots; ++i)
    {
        SaveData d;
        loadSlot(i, d);
        slotData_.push_back(d);
    }
}

SaveData GameScene::buildSave() const
{
    SaveData d;
    d.valid = true;
    d.ip = vm_.ip();
    d.vmState = static_cast<int>(vm_.state());
    d.waitRemaining = vm_.waitRemaining();
    d.title = script_ ? script_->title : std::string();
    game_.capture(d);
    if (dlg_.active())
    {
        d.sayCharacter = dlg_.name();
        d.sayText = dlg_.text();
    }
    d.autoMode = autoMode_;
    for (const auto& e : log_) d.log.push_back({e.name, e.text});
    d.preview = d.sayText.empty() ? std::string("……") : d.sayText;
    return d;
}

void GameScene::applyLoad(const SaveData& data)
{
    game_.restore(data);

    // 对话中存档：ip 已在 say 之后，恢复对话框后从 Running 继续
    if (data.vmState == static_cast<int>(VM::State::WaitTimer) && data.waitRemaining > 0.0f)
        vm_.restore(data.ip, VM::State::WaitTimer, data.waitRemaining);
    else
        vm_.restore(data.ip, VM::State::Running, 0.0f);

    dlg_.close();
    if (!data.sayText.empty() && !data.sayCharacter.empty())
    {
        dlg_.open(data.sayCharacter, data.sayText,
                  game_.colorOf(data.sayCharacter), engine()->fonts.font(),
                  static_cast<float>(GetScreenWidth()));
        game_.setSpeaking(data.sayCharacter);
    }

    log_.clear();
    for (const auto& l : data.log)
        log_.push_back({l.name, l.text, game_.colorOf(l.name)});
    autoMode_ = data.autoMode;

    // 重置所有过渡 / UI 状态
    narrateActive_ = false;
    cardActive_ = false;
    cardClosing_ = false;
    hideName_.clear();
    hideT_ = 0.0f;
    hideBlack_ = 0.0f;
    hideCleared_ = false;
    fadeAlpha_ = 0.0f;
    chapterAdvancing_ = false;
    chapterAdvanceFade_ = 0.0f;
    chapterSwitchSent_ = false;
    chapterPrompt_ = false;
    choice_.close();
    dlgShotCount_ = 1;
    prevCgShown_ = game_.cgFullyShown();
    pendingShot_.clear();
    autoTimer_ = 0.0f;
}

void GameScene::handleMenu(float dt)
{
    (void)dt;
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    float pw = 760.0f;
    float ph = 560.0f;
    float px = (w - pw) * 0.5f;
    float py = (h - ph) * 0.5f;
    float rowH = 62.0f;
    float gap = 12.0f;

    Vector2 mouse = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    menuHover_ = -1;
    for (int i = 0; i < kSaveSlots; ++i)
    {
        Rectangle r{px + 46.0f, py + 104.0f + i * (rowH + gap), pw - 92.0f, rowH};
        if (CheckCollisionPointRec(mouse, r)) menuHover_ = i;
        float target = (menuHover_ == i) ? 1.0f : 0.0f;
        float speed = 7.0f * GetFrameTime();
        if (target > menuHoverAnim_[i])
        {
            menuHoverAnim_[i] += speed;
            if (menuHoverAnim_[i] >= target) menuHoverAnim_[i] = target;
        }
        else
        {
            menuHoverAnim_[i] -= speed;
            if (menuHoverAnim_[i] <= target) menuHoverAnim_[i] = target;
        }
    }

    auto apply = [&](int idx)
    {
        if (saveMenu_)
        {
            SaveData d = buildSave();
            d.savedAt = saveTimeString();
            if (saveSlot(idx + 1, d)) slotData_[idx] = d;
        }
        else if (slotData_[idx].valid)
        {
            applyLoad(slotData_[idx]);
            saveMenu_ = loadMenu_ = false;
        }
    };

    if (pressed && menuHover_ >= 0) apply(menuHover_);
    for (int i = 0; i < kSaveSlots; ++i)
        if (IsKeyPressed(KEY_ONE + i)) apply(i);

    if (IsKeyPressed(KEY_ESCAPE)) saveMenu_ = loadMenu_ = false;
}

void GameScene::drawMenu() const
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    float pw = 760.0f;
    float ph = 560.0f;
    float px = (w - pw) * 0.5f;
    float py = (h - ph) * 0.5f;
    float rowH = 62.0f;
    float gap = 12.0f;
    const Font& font = engine()->fonts.font();

    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h), Color{0, 0, 0, 150});
    renderer::drawPanel({px, py, pw, ph}, Color{14, 17, 28, 248}, renderer::kCornerRadius,
                        Color{255, 255, 255, 45}, 2.0f);
    renderer::drawAccentLine({px + 40.0f, py + 16.0f, pw - 80.0f, 4.0f},
                             Color{90, 140, 255, 255}, 3.0f);
    engine()->fonts.draw(saveMenu_ ? "存档" : "读档", px + 44.0f, py + 34.0f, 34.0f,
                         Color{255, 255, 255, 255}, 3.4f);

    for (int i = 0; i < kSaveSlots; ++i)
    {
        Rectangle r{px + 46.0f, py + 104.0f + i * (rowH + gap), pw - 92.0f, rowH};
        const SaveData& d = slotData_[i];
        float a = renderer::easeInOut(menuHoverAnim_[i]);
        Color fill{static_cast<unsigned char>(22 + 30 * a),
                   static_cast<unsigned char>(26 + 42 * a),
                   static_cast<unsigned char>(38 + 70 * a), 235};
        Color border{255, 255, 255, static_cast<unsigned char>(35 + 110 * a)};
        DrawRectangleRounded(r, renderer::roundness(renderer::kCornerRadius, r), 12, fill);
        DrawRectangleRoundedLinesEx(r, renderer::roundness(renderer::kCornerRadius, r),
                                    12, 1.5f, border);

        std::string slot = "存档位 " + std::to_string(i + 1);
        engine()->fonts.draw(slot, r.x + 20.0f, r.y + 9.0f, 22.0f,
                             Color{220, 226, 242, 255}, 2.2f);

        if (d.valid)
        {
            // 预览文本（截断）
            std::string preview = d.preview;
            float maxW = r.width - 230.0f;
            Vector2 m = MeasureTextEx(font, preview.c_str(), 22.0f, 2.2f);
            while (m.x > maxW && !preview.empty())
            {
                size_t n = preview.size() - 1;
                while (n > 0 && (static_cast<unsigned char>(preview[n]) & 0xC0) == 0x80) --n;
                preview.erase(n);
                m = MeasureTextEx(font, preview.c_str(), 22.0f, 2.2f);
            }
            engine()->fonts.draw(preview, r.x + 20.0f, r.y + 33.0f, 22.0f,
                                 Color{185, 195, 215, 255}, 2.2f);
            if (!d.savedAt.empty())
                engine()->fonts.draw(d.savedAt, r.x + r.width - 110.0f, r.y + 12.0f, 20.0f,
                                     Color{150, 160, 185, 255}, 2.0f);
        }
        else
        {
            engine()->fonts.draw("空档位", r.x + 20.0f, r.y + 33.0f, 22.0f,
                                 Color{110, 120, 145, 255}, 2.2f);
        }
    }
    engine()->fonts.draw("Esc 关闭  ·  点击或按数字键执行", px + 44.0f, py + ph - 44.0f,
                         18.0f, Color{150, 160, 185, 255}, 1.8f);
}

void GameScene::handleEscMenu()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        escMenu_ = false;
        return;
    }
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    float pw = 380.0f;
    float ph = 430.0f;
    float px = (w - pw) * 0.5f;
    float py = (h - ph) * 0.5f;
    Vector2 mouse = GetMousePosition();
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    for (int i = 0; i < 4; ++i)
    {
        Rectangle r{px + 40.0f, py + 96.0f + i * 70.0f, pw - 80.0f, 56.0f};
        if (!CheckCollisionPointRec(mouse, r)) continue;
        if (i == 0)
        {
            escMenu_ = false;
        }
        else if (i == 1)
        {
            openSaveMenu();
        }
        else if (i == 2)
        {
            openLoadMenu();
        }
        else
        {
            escMenu_ = false;
            backToTitle();
        }
        return;
    }
}

void GameScene::drawEscMenu()
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    const Font& font = engine()->fonts.font();
    float pw = 380.0f;
    float ph = 430.0f;
    float px = (w - pw) * 0.5f;
    float py = (h - ph) * 0.5f;

    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                  Color{0, 0, 0, 150});
    renderer::drawPanel({px, py, pw, ph}, Color{16, 20, 32, 248},
                        renderer::kCornerRadius, Color{255, 255, 255, 55}, 2.0f);
    renderer::drawAccentLine({px + 36.0f, py + 14.0f, pw - 72.0f, 4.0f},
                             Color{90, 140, 255, 255}, 3.0f);
    engine()->fonts.draw("菜单", px + 40.0f, py + 30.0f, 34.0f,
                         Color{255, 255, 255, 255}, 3.4f);

    const char* labels[4] = {"继续游戏", "存档", "读档", "返回标题"};
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 4; ++i)
    {
        Rectangle r{px + 40.0f, py + 96.0f + i * 70.0f, pw - 80.0f, 56.0f};
        renderer::drawButton(r, labels[i], font, 24.0f, escHover_[i],
                             CheckCollisionPointRec(mouse, r),
                             IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, r),
                             true, Color{90, 140, 255, 255});
    }
    engine()->fonts.draw("Esc 关闭菜单", (w - MeasureTextEx(font, "Esc 关闭菜单", 18.0f, 1.8f).x) * 0.5f,
                         py + ph - 42.0f, 18.0f, Color{150, 160, 185, 255}, 1.8f);
}

void GameScene::debugAuto(int frame)
{
    // 存档 / 读档回归：先存档，选项后再读档，验证能继续到结尾
    if (frame == 280)
    {
        if (!choice_.active())
        {
            SaveData d = buildSave();
            d.savedAt = "TEST";
            saveSlot(1, d);
            printf("[dbg] saved slot1 @ ip=%zu\n", d.ip);
        }
    }
    else if (frame == 320)
    {
        SaveData d;
        if (loadSlot(1, d) && d.valid)
        {
            applyLoad(d);
            printf("[dbg] loaded slot1 @ ip=%zu\n", d.ip);
        }
    }

    if (!parseError_.empty() || vm_.state() == VM::State::Error)
    {
        engine()->selftestPassed = false;
        engine()->quit();
        return;
    }
    if (ended_)
    {
        engine()->selftestPassed = true;
        engine()->quit();
        return;
    }
    // 章节结束询问：等截图拍完再点“继续下一章”
    if (chapterPrompt_)
    {
        if (pendingShot_ != "chapter_prompt")
        {
            chapterPrompt_ = false;
            chapterAdvancing_ = true;
            chapterAdvanceFade_ = 0.0f;
        }
    }
    if (frame % 25 != 0) return;

    if (choice_.active() && !choice_.closing() && pendingShot_ != "choice")
    {
        choose(0);
    }
    else if (dlg_.active())
    {
        if (!dlg_.typingFinished())
        {
            dlg_.advance();
        }
        else
        {
            addLog(dlg_.name(), dlg_.text());
            dlg_.close();
            vm_.step(game_);
        }
    }
    else if (vm_.state() == VM::State::WaitNarrate && frame >= narrateHold_)
    {
        narrateActive_ = false;
        vm_.step(game_);
    }
    else if (vm_.state() == VM::State::WaitCard && cardActive_ && !cardClosing_ && cardT_ >= 0.9f)
    {
        cardClosing_ = true;
        cardCloseT_ = 0.0f;
    }
}
