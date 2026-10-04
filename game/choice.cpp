#include "choice.h"
#include "../core/canvas.h"

#include <algorithm>

#include <raylib.h>

#include "../renderer/renderer.h"

namespace
{
// 选项按钮排布：浮在底部对话框上方（无全屏遮罩）
void optionLayout(float screenW, float screenH, size_t count,
                  float& optW, float& optH, float& gap, float& y, float& x)
{
    optW = std::min(560.0f, screenW * 0.58f);
    optH = 56.0f;
    gap = 14.0f;
    float totalH = optH * static_cast<float>(count) + gap * (static_cast<float>(count) - 1.0f);
    y = screenH - 300.0f - totalH;
    x = (screenW - optW) * 0.5f;
}
}

void ChoicePanel::open(const std::string& character, const std::string& question,
                       const std::vector<std::pair<std::string, std::string>>& options)
{
    character_ = character;
    question_ = question;
    options_ = options;
    active_ = true;
    hoverIndex_ = -1;
    hoverAnim_.assign(options.size(), 0.0f);
    closing_ = false;
    pendingIndex_ = -1;
    fade_ = 0.0f;
}

int ChoicePanel::update()
{
    if (!active_) return -1;

    // 淡入 / 淡出（选中后先淡出，结束才返回下标）
    if (!closing_)
    {
        fade_ = std::min(1.0f, fade_ + 8.0f * GetFrameTime());
    }
    else
    {
        fade_ -= 8.0f * GetFrameTime();
        if (fade_ <= 0.0f)
        {
            fade_ = 0.0f;
            closing_ = false;
            active_ = false;
            int idx = pendingIndex_;
            pendingIndex_ = -1;
            return idx;
        }
        return -1;
    }

    Vector2 mouse = GetMousePosition();
    float w = static_cast<float>(canvas::width());
    float h = static_cast<float>(canvas::height());

    float optW, optH, gap, y, x;
    optionLayout(w, h, options_.size(), optW, optH, gap, y, x);

    hoverIndex_ = -1;
    for (size_t i = 0; i < options_.size(); ++i)
    {
        Rectangle r{x, y + i * (optH + gap), optW, optH};
        if (CheckCollisionPointRec(mouse, r))
        {
            hoverIndex_ = static_cast<int>(i);
        }
        float target = (static_cast<int>(i) == hoverIndex_) ? 1.0f : 0.0f;
        float speed = 7.0f * GetFrameTime();
        if (target > hoverAnim_[i])
        {
            hoverAnim_[i] += speed;
            if (hoverAnim_[i] >= target) hoverAnim_[i] = target;
        }
        else
        {
            hoverAnim_[i] -= speed;
            if (hoverAnim_[i] <= target) hoverAnim_[i] = target;
        }
    }

    if (hoverIndex_ >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        pendingIndex_ = hoverIndex_;
        closing_ = true;
        return -1;
    }

    for (int i = 0; i < static_cast<int>(options_.size()) && i < 9; ++i)
    {
        if (IsKeyPressed(KEY_ONE + i))
        {
            pendingIndex_ = i;
            closing_ = true;
            return -1;
        }
    }
    return -1;
}

void ChoicePanel::draw(const Font& font, float screenW, float screenH) const
{
    if (!active_ || fade_ <= 0.01f) return;
    float v = renderer::easeInOut(fade_);

    float optW, optH, gap, y, x;
    optionLayout(screenW, screenH, options_.size(), optW, optH, gap, y, x);

    for (size_t i = 0; i < options_.size(); ++i)
    {
        Rectangle r{x, y + i * (optH + gap), optW, optH};
        bool hovered = (static_cast<int>(i) == hoverIndex_);
        float a = renderer::easeInOut(hoverAnim_[i]);
        // 统一走调色板：底色 = 面板色 → 往主题色偏移；描边 = 常规线 → 主题色
        const renderer::Palette& p = renderer::palette();
        Color fill = renderer::mix(p.surfaceAlt, renderer::mix(p.surfaceAlt, p.accent, 0.42f), a);
        fill.a = static_cast<unsigned char>(232 + 16 * a);
        Color border = renderer::mix(Color{p.line.r, p.line.g, p.line.b, 45},
                                     Color{p.accent.r, p.accent.g, p.accent.b, 200}, a);
        float radiusPx = std::min(renderer::kCornerRadius, r.height * 0.24f);
        float rad = renderer::roundness(radiusPx, r);

        DrawRectangleRounded(r, rad, 16, Fade(fill, v));
        renderer::drawRoundedBorder(r, radiusPx, 1.5f, Fade(border, v));

        // 序号
        std::string num = std::to_string(i + 1);
        DrawCircleV({r.x + 38.0f, r.y + r.height * 0.5f}, 17.0f,
                    Fade(hovered ? renderer::accent() : p.surfaceHot, v));
        float numFs = 20.0f;
        Vector2 numM = MeasureTextEx(font, num.c_str(), numFs, numFs / 10.0f);
        DrawTextEx(font, num.c_str(),
                   {r.x + 38.0f - numM.x * 0.5f, r.y + r.height * 0.5f - numM.y * 0.5f - 1.0f},
                   numFs, numFs / 10.0f, Fade(Color{255, 255, 255, 255}, v));

        // 选项文本
        float txtFs = 26.0f;
        float txtSpacing = txtFs / 10.0f;
        DrawTextEx(font, options_[i].first.c_str(), {r.x + 72.0f, r.y + (r.height - 32.0f) * 0.5f},
                   txtFs, txtSpacing, Fade(p.text, v));

        if (hovered)
        {
            DrawTextEx(font, "→", {r.x + r.width - 46.0f, r.y + (r.height - 32.0f) * 0.5f},
                       26.0f, 1.0f,
                       Fade(renderer::mix(p.accent, Color{255, 255, 255, 255}, 0.35f), v));
        }
    }
}
