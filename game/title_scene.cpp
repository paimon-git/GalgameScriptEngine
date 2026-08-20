#include "title_scene.h"

#include <algorithm>
#include <cmath>

#include <raylib.h>

#include "../core/engine.h"
#include "../renderer/renderer.h"
#include "chapter_select.h"
#include "game_scene.h"

namespace
{
constexpr float kBtnW = 300.0f;
constexpr float kBtnH = 64.0f;
constexpr float kBtnX = 100.0f;
constexpr float kBtnY = 330.0f;
constexpr float kBtnGap = 80.0f;

// 设置抽屉
constexpr float kPanelW = 480.0f;
constexpr float kRowH = 110.0f;
constexpr float kSliderW = 300.0f;
constexpr float kSliderH = 40.0f;

float easeOutCubic(float t)
{
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}
}

TitleScene::TitleScene(Engine* e, std::string scriptPath,
                       std::shared_ptr<Script> script, std::string parseError)
    : Scene(e),
      scriptPath_(std::move(scriptPath)),
      titleText_(script ? script->title : std::string()),
      parseError_(std::move(parseError)),
      script_(std::move(script))
{
    if (FileExists("assets/bg/title.png"))
        bg_ = LoadTexture("assets/bg/title.png");
}

TitleScene::~TitleScene()
{
    if (bg_.id) UnloadTexture(bg_);
}

float TitleScene::menuX() const
{
    float w = static_cast<float>(GetScreenWidth());
    return std::min(kBtnX, w - kBtnW - 40.0f);
}

float TitleScene::menuY() const
{
    float h = static_cast<float>(GetScreenHeight());
    return std::min(kBtnY, h * 0.45f);
}

void TitleScene::update(float dt)
{
    updateEntryFade(dt);

    // 抽屉开合动画
    float target = settingsOpen_ ? 1.0f : 0.0f;
    float step = 5.0f * dt;
    if (target > drawerT_)
    {
        drawerT_ += step;
        if (drawerT_ >= target) drawerT_ = target;     // 吸附到目标，避免遮罩抖动
    }
    else
    {
        drawerT_ -= step;
        if (drawerT_ <= target) drawerT_ = target;
    }

    Vector2 mouse = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    // 设置抽屉打开时优先处理抽屉
    if (settingsOpen_)
    {
        updateDrawer(dt);
        return;
    }

    float cx = menuX();
    float y = menuY();
    bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    Rectangle startRect{cx, y, kBtnW, kBtnH};
    Rectangle setRect{cx, y + kBtnGap, kBtnW, kBtnH};
    Rectangle quitRect{cx, y + kBtnGap * 2, kBtnW, kBtnH};

    if ((CheckCollisionPointRec(mouse, startRect) && pressed) || enter)
        engine()->switchScene(std::make_shared<ChapterSelectScene>(engine(), scriptPath_, parseError_));
    else if (CheckCollisionPointRec(mouse, setRect) && pressed)
        settingsOpen_ = true;
    else if (CheckCollisionPointRec(mouse, quitRect) && pressed)
        engine()->quit();
}

void TitleScene::updateDrawer(float dt)
{
    (void)dt;
    float w = static_cast<float>(GetScreenWidth());
    Vector2 mouse = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    float px = w - kPanelW;

    // 点击抽屉外区域关闭
    if (pressed && mouse.x < px - 20.0f)
    {
        settingsOpen_ = false;
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE))
    {
        settingsOpen_ = false;
        return;
    }

    // 滑条拖拽
    if (pressed)
    {
        dragSlider_ = -1;
        for (int i = 0; i < 3; ++i)
        {
            Rectangle r{px + 70.0f, 180.0f + i * kRowH, kSliderW, kSliderH};
            if (CheckCollisionPointRec(mouse, r)) dragSlider_ = i;
        }
    }
    if (released) dragSlider_ = -1;

    bool changed = false;
    if (dragSlider_ >= 0)
    {
        Rectangle r{px + 70.0f, 180.0f + dragSlider_ * kRowH, kSliderW, kSliderH};
        float v01 = (mouse.x - r.x) / r.width;
        v01 = v01 < 0 ? 0 : (v01 > 1 ? 1 : v01);
        if (dragSlider_ == 0) engine()->settings.textSpeed = 5 + static_cast<int>(v01 * 195.0f);
        else if (dragSlider_ == 1) engine()->settings.bgmVolume = static_cast<int>(v01 * 100.0f);
        else engine()->settings.sfxVolume = static_cast<int>(v01 * 100.0f);
        changed = true;
    }

    // 全屏开关
    Rectangle toggle{px + 70.0f, 520.0f, 92.0f, 40.0f};
    if (pressed && CheckCollisionPointRec(mouse, toggle))
    {
        engine()->settings.fullscreen = !engine()->settings.fullscreen;
        ToggleFullscreen();
        changed = true;
    }

    // 返回（关闭抽屉）
    Rectangle backBtn{px + 70.0f, 620.0f, kPanelW - 140.0f, 56.0f};
    if (pressed && CheckCollisionPointRec(mouse, backBtn))
        settingsOpen_ = false;

    if (changed)
    {
        engine()->settings.save("settings.cfg");
        SetMasterVolume(engine()->settings.bgmVolume / 100.0f);
    }
}

void TitleScene::draw()
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());

    // 纯静态背景图
    if (bg_.id)
    {
        float scale = w / static_cast<float>(bg_.width);
        float bh = static_cast<float>(bg_.height) * scale;
        DrawTexturePro(bg_,
                       {0, 0, static_cast<float>(bg_.width), static_cast<float>(bg_.height)},
                       {0, (h - bh) * 0.5f, w, bh},
                       {0, 0}, 0.0f, WHITE);
    }
    else
    {
        ClearBackground(Color{215, 226, 244, 255});
    }

    // 左侧静态压暗
    renderer::drawGradientH({0, 0, w * 0.46f, h},
                            Color{10, 14, 28, 175}, Color{10, 14, 28, 20}, 96);

    const Font& font = engine()->fonts.font();
    float cx = menuX();
    float ty = std::min(150.0f, h * 0.24f);

    std::string title = titleText_.empty() ? "QLWT" : titleText_;
    engine()->fonts.draw(title, cx, ty, 60.0f, Color{255, 255, 255, 255}, 6.0f, true);
    engine()->fonts.draw("QLWT 视觉小说引擎", cx, ty + 86.0f, 22.0f,
                         Color{215, 226, 248, 240}, 2.2f);

    // 菜单按钮（与 update 使用同一坐标；抽屉打开时不绘制，避免背后动画干扰）
    float y = menuY();
    Vector2 mouse = GetMousePosition();
    if (drawerT_ < 0.5f)
    {
        bool h0 = CheckCollisionPointRec(mouse, {cx, y, kBtnW, kBtnH});
        bool h1 = CheckCollisionPointRec(mouse, {cx, y + kBtnGap, kBtnW, kBtnH});
        bool h2 = CheckCollisionPointRec(mouse, {cx, y + kBtnGap * 2, kBtnW, kBtnH});

        renderer::drawButton({cx, y, kBtnW, kBtnH}, "开始游戏", font, 28.0f,
                             hoverAnim_[0], h0, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && h0,
                             parseError_.empty());
        renderer::drawButton({cx, y + kBtnGap, kBtnW, kBtnH}, "设置", font, 28.0f,
                             hoverAnim_[1], h1, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && h1);
        renderer::drawButton({cx, y + kBtnGap * 2, kBtnW, kBtnH}, "退出", font, 28.0f,
                             hoverAnim_[2], h2, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && h2);
    }

    if (!parseError_.empty())
    {
        engine()->fonts.draw("脚本错误：" + parseError_,
                             (w - MeasureTextEx(font, ("脚本错误：" + parseError_).c_str(),
                                                20.0f, 2.0f).x) * 0.5f,
                             h * 0.86f, 20.0f, Color{255, 150, 150, 255}, 2.0f);
    }

    engine()->fonts.draw("v1.2  ·  QLWT ENGINE", 24.0f, h - 34.0f,
                         17.0f, Color{255, 255, 255, 150}, 1.7f);

    // 设置抽屉（从右侧滑出）
    if (drawerT_ > 0.001f) drawDrawer();

    // 入场淡入遮罩（最后绘制，盖在一切之上）
    float fadeA = entryFadeAlpha();
    if (fadeA > 1.0f)
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                      Color{0, 0, 0, static_cast<unsigned char>(fadeA)});
}

void TitleScene::drawDrawer()
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    const Font& font = engine()->fonts.font();

    float px = w - kPanelW * easeOutCubic(drawerT_);

    // 左侧半透明遮罩
    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                  Color{8, 12, 24, static_cast<unsigned char>(70 * renderer::easeInOut(drawerT_))});

    // 面板：右缘延伸出屏幕，左缘圆角与按钮一致
    Rectangle panelRect{px - renderer::kCornerRadius, 0,
                        kPanelW + renderer::kCornerRadius, h};
    float rad = renderer::roundness(renderer::kCornerRadius, panelRect);
    DrawRectangleRounded(panelRect, rad, 12, Color{16, 20, 34, 245});
    DrawRectangleRoundedLinesEx(panelRect, rad, 12, 2.0f,
                                Color{255, 255, 255, 40});

    engine()->fonts.draw("设置", px + 50.0f, 46.0f, 38.0f,
                         Color{255, 255, 255, 255}, 3.8f);
    renderer::drawAccentLine({px + 50.0f, 96.0f, kPanelW - 100.0f, 4.0f},
                             Color{90, 140, 255, 255}, 3.0f);

    const char* labels[3] = {"文字速度", "音乐音量", "音效音量"};
    float values01[3] = {
        (engine()->settings.textSpeed - 5.0f) / 195.0f,
        engine()->settings.bgmVolume / 100.0f,
        engine()->settings.sfxVolume / 100.0f,
    };
    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < 3; ++i)
    {
        float ry = 180.0f + i * kRowH;
        engine()->fonts.draw(labels[i], px + 50.0f, ry - 34.0f, 26.0f,
                             Color{235, 238, 250, 255}, 2.6f);
        Rectangle r{px + 70.0f, ry, kSliderW, kSliderH};
        float v = renderer::drawSlider(r, values01[i], dragSlider_ == i,
                                       Color{90, 140, 255, 255});
        std::string val = std::to_string(static_cast<int>(
            i == 0 ? (5 + v * 195.0f) : (i == 1 ? v * 100.0f : v * 100.0f)));
        engine()->fonts.draw(val, r.x + r.width + 22.0f, ry + 8.0f, 22.0f,
                             Color{170, 180, 210, 255}, 2.2f);
    }

    // 全屏
    engine()->fonts.draw("全屏模式", px + 50.0f, 486.0f, 26.0f,
                         Color{235, 238, 250, 255}, 2.6f);
    Rectangle toggle{px + 70.0f, 520.0f, 92.0f, 40.0f};
    bool hoverToggle = CheckCollisionPointRec(mouse, toggle);
    renderer::drawToggle(toggle, engine()->settings.fullscreen, false,
                         Color{90, 140, 255, 255});
    engine()->fonts.draw(engine()->settings.fullscreen ? "开" : "关",
                         px + 180.0f, 528.0f, 22.0f,
                         hoverToggle ? Color{200, 215, 255, 255} : Color{170, 180, 210, 255},
                         2.2f);

    // 返回
    Rectangle backBtn{px + 70.0f, 620.0f, kPanelW - 140.0f, 56.0f};
    bool hoverBack = CheckCollisionPointRec(mouse, backBtn);
    renderer::drawButton(backBtn, "返回", font, 26.0f,
                         backHover_, hoverBack,
                         IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hoverBack);
}

void TitleScene::debugAuto(int frame)
{
    if (frame == 25) settingsOpen_ = true;
    else if (frame == 70) engine()->selftestShot("settings_drawer");
    else if (frame == 90) settingsOpen_ = false;
    else if (frame == 110) engine()->switchScene(std::make_shared<ChapterSelectScene>(
        engine(), scriptPath_, parseError_));
}
