#include "title_scene.h"
#include "../core/canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <raylib.h>

#include "../core/engine.h"
#include "../core/gesture.h"
#include "../core/lang.h"
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
constexpr float kSliderW = 300.0f;
constexpr float kSliderH = 40.0f;
constexpr float kPad = 50.0f;       // 抽屉左右内边距

// 抽屉「内容坐标」（0 = 可视区顶部）；绘制时统一偏移 -drawerScroll_
// 每个滑条：标签在 labelY（高约 30），滑条在 labelY+34（高 40）。
// 下一行至少从上一行滑条底部 +20 开始，避免标签压到上一个控件上。
constexpr float kSliderLabelY[7] = {  8.0f, 124.0f, 240.0f, 496.0f, 596.0f, 696.0f, 800.0f};
constexpr float kSliderY[7]      = { 42.0f, 158.0f, 274.0f, 530.0f, 630.0f, 730.0f, 834.0f};
constexpr float kSectionAppearY  = 358.0f;
constexpr float kAccentLabelY    = 400.0f;
constexpr float kSwatchY         = 436.0f;   // 色块 42x42，间距 48
constexpr float kStyleLabelY     = 900.0f;
constexpr float kStyleY          = 934.0f;
constexpr float kFullLabelY      = 1030.0f;
constexpr float kFullToggleY     = 1064.0f;
constexpr float kBackY           = 1148.0f;
constexpr float kDrawerContentH  = 1230.0f;

constexpr float kSwatchSize = 42.0f;
constexpr float kSwatchPitch = 48.0f;   // 48*7+42 = 378，右端 x=面板左+428，不碰滚动条

constexpr float kViewTop = 112.0f;      // 固定表头之下才是可滚动区
constexpr float kViewBottomPad = 16.0f;

// 主题色预设
const unsigned int kPresets[8] = {
    0x5A8CFF, 0x8C6CF0, 0xE0559B, 0xF0724A,
    0xE0B34C, 0x3FBF8F, 0x38B6D9, 0xD8DEE9,
};

// 文案 key（真正的文字在 assets/lang/zh_CN.lang 里）
const char* kSliderKeys[7] = {"settings.text_speed", "settings.bgm_volume", "settings.sfx_volume",
                              "settings.rgb_r", "settings.rgb_g", "settings.rgb_b",
                              "settings.corner"};
const char* kStyleKeys[3] = {"settings.style_dark", "settings.style_light", "settings.style_outline"};

float easeOutCubic(float t)
{
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

Color rgbOf(unsigned int hex)
{
    return Color{static_cast<unsigned char>((hex >> 16) & 0xFF),
                 static_cast<unsigned char>((hex >> 8) & 0xFF),
                 static_cast<unsigned char>(hex & 0xFF), 255};
}

float sliderValue01(int i, const Settings& s)
{
    switch (i)
    {
    case 0: return (s.textSpeed - 5.0f) / 195.0f;
    case 1: return s.bgmVolume / 100.0f;
    case 2: return s.sfxVolume / 100.0f;
    case 3: return ((s.uiAccent >> 16) & 0xFF) / 255.0f;
    case 4: return ((s.uiAccent >> 8) & 0xFF) / 255.0f;
    case 5: return (s.uiAccent & 0xFF) / 255.0f;
    default: return s.uiCorner / 28.0f;
    }
}

int sliderDisplay(int i, const Settings& s)
{
    switch (i)
    {
    case 0: return s.textSpeed;
    case 1: return s.bgmVolume;
    case 2: return s.sfxVolume;
    case 3: return (s.uiAccent >> 16) & 0xFF;
    case 4: return (s.uiAccent >> 8) & 0xFF;
    case 5: return s.uiAccent & 0xFF;
    default: return s.uiCorner;
    }
}

void applySlider(int i, float v01, Settings& s)
{
    v01 = std::clamp(v01, 0.0f, 1.0f);
    switch (i)
    {
    case 0: s.textSpeed = 5 + static_cast<int>(v01 * 195.0f); break;
    case 1: s.bgmVolume = static_cast<int>(v01 * 100.0f); break;
    case 2: s.sfxVolume = static_cast<int>(v01 * 100.0f); break;
    case 3: s.uiAccent = (s.uiAccent & 0x00FFFFu) |
                         (static_cast<unsigned int>(v01 * 255.0f + 0.5f) << 16); break;
    case 4: s.uiAccent = (s.uiAccent & 0xFF00FFu) |
                         (static_cast<unsigned int>(v01 * 255.0f + 0.5f) << 8); break;
    case 5: s.uiAccent = (s.uiAccent & 0xFFFF00u) |
                         static_cast<unsigned int>(v01 * 255.0f + 0.5f); break;
    default: s.uiCorner = static_cast<int>(v01 * 28.0f + 0.5f); break;
    }
}
}

TitleScene::TitleScene(Engine* e, std::string scriptPath,
                       std::shared_ptr<Script> script, std::string parseError, bool direct)
    : Scene(e),
      scriptPath_(std::move(scriptPath)),
      titleText_(script ? script->title : std::string()),
      parseError_(std::move(parseError)),
      script_(std::move(script)),
      direct_(direct)
{
    if (FileExists("assets/bg/title.png"))
        bg_ = renderer::loadSmoothTexture("assets/bg/title.png");
}

TitleScene::~TitleScene()
{
    if (bg_.id) UnloadTexture(bg_);
}

float TitleScene::menuX() const
{
    float w = static_cast<float>(canvas::width());
    return std::min(kBtnX, w - kBtnW - 40.0f);
}

float TitleScene::menuY() const
{
    float h = static_cast<float>(canvas::height());
    return std::min(kBtnY, h * 0.45f);
}

float TitleScene::drawerViewTop() const { return kViewTop; }

float TitleScene::drawerViewH() const
{
    float h = static_cast<float>(canvas::height());
    return std::max(80.0f, h - kViewTop - kViewBottomPad);
}

float TitleScene::drawerMaxScroll() const
{
    return std::max(0.0f, kDrawerContentH - drawerViewH());
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
    {
        // --direct：跳过章节选择，直接进默认脚本的第一章（调试某一段剧情用）
        if (direct_ && script_)
            engine()->switchScene(std::make_shared<GameScene>(engine(), scriptPath_, script_,
                                                              parseError_, 0));
        else
            engine()->switchScene(std::make_shared<ChapterSelectScene>(engine(), scriptPath_, parseError_));
    }
    else if (CheckCollisionPointRec(mouse, setRect) && pressed)
        settingsOpen_ = true;
    else if (CheckCollisionPointRec(mouse, quitRect) && pressed)
        engine()->quit();
}

void TitleScene::updateDrawer(float dt)
{
    (void)dt;
    float w = static_cast<float>(canvas::width());
    Vector2 mouse = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    float wheel = GetMouseWheelMove();

    float px = w - kPanelW * easeOutCubic(drawerT_);
    float viewTop = drawerViewTop();
    float viewH = drawerViewH();
    float maxScroll = drawerMaxScroll();
    bool inViewport = mouse.y >= viewTop && mouse.y <= viewTop + viewH;

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

    // 滚动：原生双指拖动优先，鼠标滚轮兜底。
    // 手指还在板上时不要再吃同一动作产生的滚动轴事件，否则会滚两倍。
    const gesture::Frame& g = gesture::frame();
    if (mouse.x >= px - 20.0f)
    {
        if (maxScroll > 0.0f && g.active && std::fabs(g.panY) > 0.01f)
            drawerScrollTarget_ -= g.panY * 0.6f;
        if (maxScroll > 0.0f && !g.touching && wheel != 0.0f)
            drawerScrollTarget_ -= wheel * 90.0f;
        if (IsKeyPressed(KEY_PAGE_DOWN)) drawerScrollTarget_ += viewH - 80.0f;
        if (IsKeyPressed(KEY_PAGE_UP))   drawerScrollTarget_ -= viewH - 80.0f;
    }
    if (IsKeyPressed(KEY_DOWN)) drawerScrollTarget_ += 60.0f;
    if (IsKeyPressed(KEY_UP))   drawerScrollTarget_ -= 60.0f;
    if (IsKeyPressed(KEY_HOME)) drawerScrollTarget_ = 0.0f;
    if (IsKeyPressed(KEY_END))  drawerScrollTarget_ = maxScroll;
    drawerScrollTarget_ = std::clamp(drawerScrollTarget_, 0.0f, maxScroll);

    auto toScreenY = [&](float cy) { return viewTop + cy - drawerScroll_; };

    bool changed = false;

    // ---- 滑条拖拽（0..2 速度/音量，3..5 主题色 RGB，6 圆角）----
    if (pressed)
    {
        dragSlider_ = -1;
        if (inViewport)
        {
            for (int i = 0; i < 7; ++i)
            {
                Rectangle r{px + 70.0f, toScreenY(kSliderY[i]), kSliderW, kSliderH};
                if (CheckCollisionPointRec(mouse, r)) { dragSlider_ = i; break; }
            }
        }
    }
    if (released) dragSlider_ = -1;

    if (dragSlider_ >= 0)
    {
        Rectangle r{px + 70.0f, toScreenY(kSliderY[dragSlider_]), kSliderW, kSliderH};
        applySlider(dragSlider_, (mouse.x - r.x) / r.width, engine()->settings);
        changed = true;
    }

    // ---- 主题色预设 ----
    for (int i = 0; i < 8; ++i)
    {
        Rectangle r{px + kPad + i * kSwatchPitch, toScreenY(kSwatchY),
                    kSwatchSize, kSwatchSize};
        bool hov = inViewport && CheckCollisionPointRec(mouse, r);
        presetHover_[i] = renderer::approach(presetHover_[i], hov ? 1.0f : 0.0f, 16.0f,
                                             renderer::frameDelta());
        if (pressed && inViewport && CheckCollisionPointRec(mouse, r))
        {
            engine()->settings.uiAccent = kPresets[i];
            changed = true;
        }
    }

    // ---- 按钮样式 ----
    float bw = (kPanelW - kPad * 2.0f - 24.0f) / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        Rectangle r{px + kPad + i * (bw + 12.0f), toScreenY(kStyleY), bw, 46.0f};
        bool hov = inViewport && CheckCollisionPointRec(mouse, r);
        styleHover_[i] = renderer::approach(styleHover_[i], hov ? 1.0f : 0.0f, 16.0f,
                                            renderer::frameDelta());
        if (pressed && inViewport && hov)
        {
            engine()->settings.uiButtonStyle = i;
            changed = true;
        }
    }

    // ---- 全屏开关 ----
    Rectangle toggle{px + 70.0f, toScreenY(kFullToggleY), 92.0f, 40.0f};
    if (pressed && inViewport && CheckCollisionPointRec(mouse, toggle))
    {
        engine()->settings.fullscreen = !engine()->settings.fullscreen;
        ToggleFullscreen();
        changed = true;
    }

    // ---- 返回（关闭抽屉）----
    Rectangle backBtn{px + kPad, toScreenY(kBackY), kPanelW - kPad * 2.0f, 54.0f};
    if (pressed && inViewport && CheckCollisionPointRec(mouse, backBtn))
        settingsOpen_ = false;

    if (changed)
    {
        engine()->applyTheme();                    // 立刻生效，方便边调边看
        engine()->settings.save("settings.cfg");
        SetMasterVolume(engine()->settings.bgmVolume / 100.0f);
    }

    drawerScroll_ = renderer::approach(drawerScroll_, drawerScrollTarget_, 22.0f,
                                       renderer::frameDelta());
}

void TitleScene::draw()
{
    float w = static_cast<float>(canvas::width());
    float h = static_cast<float>(canvas::height());

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

    // 左侧静态压暗：末端必须淡到 alpha=0，否则渐变截断处会出现一条硬边
    // （之前末端是 alpha=20，剩下的 8% 不透明度在截断处形成可见的竖线）
    renderer::drawGradientH({0, 0, w * 0.62f, h},
                            Color{10, 14, 28, 185}, Color{10, 14, 28, 0}, 128);

    const Font& font = engine()->fonts.font();
    float cx = menuX();
    float ty = std::min(150.0f, h * 0.24f);

    // 开始界面的大标题：优先用语言文件里的 title.main（想换标题改那一行就行，
    // 留空则回落到默认脚本的 title，再不行才是引擎名）。
    std::string title = lang::tr("title.main");
    if (title.empty() || title == "title.main") title = titleText_;
    if (title.empty()) title = "QLWT";

    // 标题太长时自动缩字号：左侧面板只占屏幕 62%，别让字跑到面板外
    float titleSize = 60.0f;
    float titleMaxW = w * 0.62f - cx - 36.0f;
    while (titleSize > 26.0f && engine()->fonts.measure(title, titleSize).x > titleMaxW)
        titleSize -= 2.0f;

    engine()->fonts.draw(title, cx, ty, titleSize, Color{255, 255, 255, 255}, 6.0f, true);
    engine()->fonts.draw(lang::tr("title.subtitle"), cx, ty + 86.0f, 22.0f,
                         Color{215, 226, 248, 240}, 2.2f);

    // 菜单按钮：抽屉展开时平滑淡出，而不是在某一帧突然消失（"按钮闪"）
    float y = menuY();
    Vector2 mouse = GetMousePosition();
    float menuA = 1.0f - renderer::easeInOut(std::min(1.0f, drawerT_ * 2.2f));
    if (menuA > 0.01f)
    {
        bool interactive = menuA > 0.9f;
        bool h0 = interactive && CheckCollisionPointRec(mouse, {cx, y, kBtnW, kBtnH});
        bool h1 = interactive && CheckCollisionPointRec(mouse, {cx, y + kBtnGap, kBtnW, kBtnH});
        bool h2 = interactive && CheckCollisionPointRec(mouse, {cx, y + kBtnGap * 2, kBtnW, kBtnH});

        renderer::drawButton({cx, y, kBtnW, kBtnH}, lang::tr("title.start"), font, 28.0f,
                             hoverAnim_[0], h0, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && h0,
                             parseError_.empty(), Color{0, 0, 0, 0}, menuA);
        renderer::drawButton({cx, y + kBtnGap, kBtnW, kBtnH}, lang::tr("title.settings"), font, 28.0f,
                             hoverAnim_[1], h1, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && h1,
                             true, Color{0, 0, 0, 0}, menuA);
        renderer::drawButton({cx, y + kBtnGap * 2, kBtnW, kBtnH}, lang::tr("title.quit"), font, 28.0f,
                             hoverAnim_[2], h2, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && h2,
                             true, Color{0, 0, 0, 0}, menuA);
    }

    if (!parseError_.empty())
    {
        engine()->fonts.draw(std::string(lang::tr("game.error_prefix")) + parseError_,
                             (w - MeasureTextEx(font, (std::string(lang::tr("game.error_prefix")) + parseError_).c_str(),
                                                20.0f, 2.0f).x) * 0.5f,
                             h * 0.86f, 20.0f, Color{255, 150, 150, 255}, 2.0f);
    }

    engine()->fonts.draw(lang::tr("title.version"), 24.0f, h - 34.0f,
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
    float w = static_cast<float>(canvas::width());
    float h = static_cast<float>(canvas::height());
    const Font& font = engine()->fonts.font();
    const Settings& st = engine()->settings;
    const Color acc = renderer::accent();

    float px = w - kPanelW * easeOutCubic(drawerT_);
    float viewTop = drawerViewTop();
    float viewH = drawerViewH();
    float maxScroll = drawerMaxScroll();
    auto toScreenY = [&](float cy) { return viewTop + cy - drawerScroll_; };

    // 左侧半透明遮罩
    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                  Color{8, 12, 24, static_cast<unsigned char>(70 * renderer::easeInOut(drawerT_))});

    // 面板：右缘延伸出屏幕，左缘圆角与按钮一致
    Rectangle panelRect{px - renderer::kCornerRadius, 0,
                        kPanelW + renderer::kCornerRadius, h};
    float rad = renderer::roundness(renderer::kCornerRadius, panelRect);
    DrawRectangleRounded(panelRect, rad, 12, renderer::palette().surface);
    renderer::drawRoundedBorder(panelRect, renderer::kCornerRadius, 2.0f,
                                renderer::palette().line);

    // ---- 固定表头 ----
    engine()->fonts.draw(lang::tr("settings.title"), px + kPad, 42.0f, 38.0f,
                         renderer::palette().text, 3.8f);
    engine()->fonts.draw(lang::tr("settings.scroll_hint"), px + kPanelW - kPad - 94.0f, 56.0f, 17.0f,
                         renderer::palette().textMuted, 1.7f);
    renderer::drawAccentLine({px + kPad, 94.0f, kPanelW - kPad * 2.0f, 4.0f}, acc, 3.0f);

    Vector2 mouse = GetMousePosition();

    // ---- 可滚动内容 ----
    float rs = engine()->renderScale();
    BeginScissorMode(static_cast<int>(px * rs), static_cast<int>(viewTop * rs),
                     static_cast<int>(kPanelW * rs), static_cast<int>(viewH * rs));

    for (int i = 0; i < 7; ++i)
    {
        float sy = toScreenY(kSliderY[i]);
        if (sy + kSliderH < viewTop - 8.0f || sy > viewTop + viewH + 8.0f) continue;

        engine()->fonts.draw(lang::tr(kSliderKeys[i]), px + kPad, toScreenY(kSliderLabelY[i]),
                             25.0f, renderer::palette().text, 2.5f);
        Rectangle r{px + 70.0f, sy, kSliderW, kSliderH};
        renderer::drawSlider(r, sliderValue01(i, st), dragSlider_ == i);
        engine()->fonts.draw(std::to_string(sliderDisplay(i, st)),
                             r.x + r.width + 22.0f, sy + 8.0f, 22.0f,
                             renderer::palette().textDim, 2.2f);
    }

    // ---- 外观 ----
    engine()->fonts.draw(lang::tr("settings.section_appearance"), px + kPad, toScreenY(kSectionAppearY), 22.0f,
                         renderer::palette().textDim, 2.2f);
    DrawRectangleRounded({px + kPad + 60.0f, toScreenY(kSectionAppearY) + 14.0f,
                          kPanelW - kPad * 2.0f - 60.0f, 2.0f},
                         1.0f, 4, renderer::palette().line);

    // 主题色
    engine()->fonts.draw(lang::tr("settings.accent"), px + kPad, toScreenY(kAccentLabelY), 25.0f,
                         renderer::palette().text, 2.5f);
    {
        char hex[16];
        std::snprintf(hex, sizeof(hex), "#%06X", st.uiAccent & 0xFFFFFFu);
        engine()->fonts.draw(hex, px + kPad + 132.0f, toScreenY(kAccentLabelY) + 2.0f,
                             20.0f, renderer::palette().textDim, 2.0f);
    }
    for (int i = 0; i < 8; ++i)
    {
        Rectangle r{px + kPad + i * kSwatchPitch, toScreenY(kSwatchY),
                    kSwatchSize, kSwatchSize};
        float e = renderer::easeInOut(presetHover_[i]);
        float rr = renderer::roundness(std::min(10.0f + 6.0f * e, 21.0f), r);
        Color c = rgbOf(kPresets[i]);
        DrawRectangleRounded(r, rr, 16, c);
        bool selected = (st.uiAccent & 0xFFFFFFu) == kPresets[i];
        if (selected)
            DrawRectangleRoundedLinesEx({r.x - 3.0f, r.y - 3.0f, r.width + 6.0f, r.height + 6.0f},
                                        renderer::roundness(12.0f, r), 16, 2.5f, WHITE);
        else
            renderer::drawRoundedBorder(r, std::min(10.0f + 6.0f * e, 21.0f), 1.5f + e,
                                        Color{255, 255, 255, static_cast<unsigned char>(50 + 120 * e)});
    }

    // 按钮样式
    engine()->fonts.draw(lang::tr("settings.button_style"), px + kPad, toScreenY(kStyleLabelY), 25.0f,
                         renderer::palette().text, 2.5f);
    float bw = (kPanelW - kPad * 2.0f - 24.0f) / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        Rectangle r{px + kPad + i * (bw + 12.0f), toScreenY(kStyleY), bw, 46.0f};
        bool on = (st.uiButtonStyle == i);
        float e = renderer::easeInOut(std::max(styleHover_[i], on ? 1.0f : 0.0f));
        float rr = renderer::roundness(std::min(renderer::cornerRadius(), 11.0f), r);
        Color fill = on ? Color{acc.r, acc.g, acc.b, 210}
                        : renderer::mix(renderer::palette().surfaceAlt,
                                        renderer::palette().surfaceHot, e);
        DrawRectangleRounded(r, rr, 16, fill);
        renderer::drawRoundedBorder(r, std::min(renderer::cornerRadius(), 11.0f), 1.5f,
                                    on ? Color{255, 255, 255, 120}
                                       : Color{255, 255, 255, static_cast<unsigned char>(40 + 80 * e)});
        Vector2 m = MeasureTextEx(font, lang::tr(kStyleKeys[i]), 21.0f, 2.1f);
        DrawTextEx(font, lang::tr(kStyleKeys[i]),
                   {r.x + (r.width - m.x) * 0.5f, r.y + (r.height - m.y) * 0.5f - 1.0f},
                   21.0f, 2.1f, renderer::palette().text);
    }

    // 全屏
    engine()->fonts.draw(lang::tr("settings.fullscreen"), px + kPad, toScreenY(kFullLabelY), 25.0f,
                         renderer::palette().text, 2.5f);
    Rectangle toggle{px + 70.0f, toScreenY(kFullToggleY), 92.0f, 40.0f};
    renderer::drawToggle(toggle, st.fullscreen, false);
    engine()->fonts.draw(st.fullscreen ? lang::tr("common.on") : lang::tr("common.off"), px + 180.0f,
                         toScreenY(kFullToggleY) + 8.0f, 22.0f,
                         renderer::palette().textDim, 2.2f);

    // 返回
    Rectangle backBtn{px + kPad, toScreenY(kBackY), kPanelW - kPad * 2.0f, 54.0f};
    bool hoverBack = CheckCollisionPointRec(mouse, backBtn) &&
                     mouse.y >= viewTop && mouse.y <= viewTop + viewH;
    renderer::drawButton(backBtn, lang::tr("common.back"), font, 26.0f, backHover_, hoverBack,
                         IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hoverBack);

    EndScissorMode();

    // 抽屉滚动条
    if (maxScroll > 0.5f)
    {
        Rectangle track{px + kPanelW - 14.0f, viewTop, 6.0f, viewH};
        float rr = renderer::roundness(3.0f, track);
        DrawRectangleRounded(track, rr, 12, renderer::palette().line);
        float thumbH = std::max(48.0f, viewH * (viewH / kDrawerContentH));
        float t = drawerScroll_ / maxScroll;
        DrawRectangleRounded({track.x, track.y + t * (viewH - thumbH), track.width, thumbH},
                             rr, 12, Color{acc.r, acc.g, acc.b, 200});
    }
}

void TitleScene::debugAuto(int frame)
{
    if (frame == 25) settingsOpen_ = true;
    else if (frame == 60) engine()->selftestShot("settings_drawer");     // 顶部：音量 + 主题色色块
    else if (frame == 72) drawerScrollTarget_ = 300.0f;                  // 滚到中间
    else if (frame == 85) engine()->selftestShot("settings_appearance"); // 主题色 RGB 三滑条
    else if (frame == 97) drawerScrollTarget_ = drawerMaxScroll();
    else if (frame == 108) engine()->selftestShot("settings_bottom");    // 底部：样式/全屏/返回
    else if (frame == 118) settingsOpen_ = false;
    else if (frame == 130)
    {
        // --direct：自检也走"直接进剧情"，方便单独看某一段（比如观测战）
        if (direct_ && script_)
            engine()->switchScene(std::make_shared<GameScene>(engine(), scriptPath_, script_,
                                                              parseError_, 0));
        else
            engine()->switchScene(std::make_shared<ChapterSelectScene>(engine(), scriptPath_,
                                                                       parseError_));
    }
}
