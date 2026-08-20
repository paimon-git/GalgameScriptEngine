#include "renderer.h"

#include <algorithm>

#include <raylib.h>

namespace renderer
{
namespace
{
float roundnessOf(float radiusPx, Rectangle rect)
{
    float half = std::min(rect.width, rect.height) * 0.5f;
    if (half <= 0.0f) return 0.0f;
    return std::min(1.0f, radiusPx / half);
}

Color lerpColor(Color a, Color b, float t)
{
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t),
    };
}
}

float roundness(float radiusPx, Rectangle rect)
{
    return roundnessOf(radiusPx, rect);
}

Color hexToColor(unsigned int rgba)
{
    if (rgba > 0xFFFFFF)  // 带 alpha
        return Color{static_cast<unsigned char>((rgba >> 24) & 0xFF),
                     static_cast<unsigned char>((rgba >> 16) & 0xFF),
                     static_cast<unsigned char>((rgba >> 8) & 0xFF),
                     static_cast<unsigned char>(rgba & 0xFF)};
    return Color{static_cast<unsigned char>((rgba >> 16) & 0xFF),
                 static_cast<unsigned char>((rgba >> 8) & 0xFF),
                 static_cast<unsigned char>(rgba & 0xFF),
                 255};
}

void drawPanel(Rectangle rect, Color fill, float radius, Color border, float borderWidth)
{
    float r = roundnessOf(radius, rect);
    DrawRectangleRounded(rect, r, 16, fill);
    if (border.a > 0)
        DrawRectangleRoundedLinesEx(rect, r, 16, borderWidth, border);
}

bool drawButton(Rectangle rect, const std::string& label,
                const Font& font, float fontSize,
                float& hoverAnim, bool hovered, bool pressed, bool enabled, Color accent)
{
    if (!enabled) { hovered = pressed = false; }

    // 悬停线性动画：向目标状态过渡
    float target = (hovered && enabled) ? 1.0f : 0.0f;
    float speed = 7.0f * GetFrameTime();
    if (target > hoverAnim)
    {
        hoverAnim += speed;
        if (hoverAnim >= target) hoverAnim = target;   // 吸附到目标，避免来回振荡
    }
    else
    {
        hoverAnim -= speed;
        if (hoverAnim <= target) hoverAnim = target;
    }

    float e = easeInOut(hoverAnim);   // 非线性悬停变色
    Color baseNorm{38, 44, 66, 230};
    Color baseHover{62, 92, 158, 242};
    Color base = enabled ? lerpColor(baseNorm, baseHover, e) : Color{30, 32, 42, 160};
    Color borderNorm{255, 255, 255, 45};
    Color borderHover{150, 190, 255, 150};
    Color border = enabled ? lerpColor(borderNorm, borderHover, e) : Color{255, 255, 255, 25};
    float r = std::min(kCornerRadius, rect.height * 0.24f);   // 圆角矩形，不做胶囊

    if (pressed) base = Color{20, 23, 34, 240};

    DrawRectangleRounded(rect, roundnessOf(r, rect), 16, base);
    if (e > 0.01f && enabled && !pressed)
    {
        DrawRectangleRounded(rect, roundnessOf(r, rect), 16,
                             Color{accent.r, accent.g, accent.b,
                                   static_cast<unsigned char>(46 * e)});
    }
    DrawRectangleRoundedLinesEx(rect, roundnessOf(r, rect), 16, 1.5f, border);

    if (enabled)
    {
        Vector2 m = MeasureTextEx(font, label.c_str(), fontSize, fontSize / 10.0f);
        Color textColor{245, 247, 255, static_cast<unsigned char>(215 + 40 * e)};
        DrawTextEx(font, label.c_str(),
                   {rect.x + (rect.width - m.x) * 0.5f,
                    rect.y + (rect.height - m.y) * 0.5f - 2.0f},
                   fontSize, fontSize / 10.0f, textColor);
    }
    return enabled && pressed;
}

float drawSlider(Rectangle rect, float value01, bool drag, Color accent)
{
    value01 = value01 < 0 ? 0 : (value01 > 1 ? 1 : value01);
    float trackY = rect.y + rect.height * 0.5f - 4.0f;
    DrawRectangleRounded({rect.x, trackY, rect.width, 8.0f}, 4.0f, 12,
                         Color{255, 255, 255, 26});
    float fillW = rect.width * value01;
    if (fillW > 8.0f)
        DrawRectangleRounded({rect.x, trackY, fillW, 8.0f}, 4.0f, 12, accent);

    Vector2 knob(rect.x + rect.width * value01, rect.y + rect.height * 0.5f);
    float kr = drag ? 15.0f : 13.0f;
    DrawCircleV(knob, kr + 4.0f, Color{0, 0, 0, 60});
    DrawCircleV(knob, kr, drag ? Color{255, 255, 255, 255} : Color{225, 232, 255, 255});
    DrawCircleV(knob, kr * 0.45f, accent);

    if (drag)
    {
        float mx = static_cast<float>(GetMouseX());
        value01 = (mx - rect.x) / rect.width;
        value01 = value01 < 0 ? 0 : (value01 > 1 ? 1 : value01);
    }
    return value01;
}

bool drawToggle(Rectangle rect, bool on, bool clicked, Color accent)
{
    Color fill = on ? accent : Color{70, 74, 92, 220};
    Color border = Color{255, 255, 255, 70};
    DrawRectangleRounded(rect, roundnessOf(kCornerRadius, rect), 16, fill);
    DrawRectangleRoundedLinesEx(rect, roundnessOf(kCornerRadius, rect), 16, 1.5f, border);
    float pad = 4.0f;
    float d = rect.height - pad * 2.0f;
    float kx = on ? rect.x + rect.width - d - pad : rect.x + pad;
    DrawCircleV({kx + d * 0.5f, rect.y + rect.height * 0.5f}, d * 0.5f,
                Color{255, 255, 255, 245});
    if (clicked) on = !on;
    return on;
}

void drawAccentLine(Rectangle rect, Color color, float width)
{
    DrawRectangleRounded({rect.x + rect.width * 0.08f, rect.y,
                          rect.width * 0.84f, width},
                         width * 0.5f, 4, color);
}

namespace
{
Color gradientStep(Color a, Color b, float t)
{
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t),
    };
}
}

void drawGradientH(Rectangle rect, Color c1, Color c2, int strips)
{
    if (strips <= 0) strips = 64;
    float w = rect.width / static_cast<float>(strips);
    for (int i = 0; i < strips; ++i)
    {
        float t = (i + 0.5f) / static_cast<float>(strips);
        int x0 = static_cast<int>(rect.x + i * w);
        int x1 = static_cast<int>(rect.x + (i + 1) * w);
        if (x1 <= x0) x1 = x0 + 1;
        DrawRectangle(x0, static_cast<int>(rect.y), x1 - x0, static_cast<int>(rect.height),
                      gradientStep(c1, c2, t));
    }
}

void drawGradientV(Rectangle rect, Color c1, Color c2, int strips)
{
    if (strips <= 0) strips = 64;
    float h = rect.height / static_cast<float>(strips);
    for (int i = 0; i < strips; ++i)
    {
        float t = (i + 0.5f) / static_cast<float>(strips);
        int y0 = static_cast<int>(rect.y + i * h);
        int y1 = static_cast<int>(rect.y + (i + 1) * h);
        if (y1 <= y0) y1 = y0 + 1;
        DrawRectangle(static_cast<int>(rect.x), y0,
                      static_cast<int>(rect.width), y1 - y0,
                      gradientStep(c1, c2, t));
    }
}
}
