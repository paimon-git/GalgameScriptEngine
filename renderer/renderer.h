#pragma once

#include <raylib.h>

#include <string>

// UI 绘制辅助：现代扁平风格的圆角面板 / 按钮 / 滑条 / 文字
namespace renderer
{
// 全项目统一圆角：按钮与面板保持同一圆心角
constexpr float kCornerRadius = 12.0f;

Color hexToColor(unsigned int rgba);   // 0xRRGGBB 或 0xAARRGGBB

// raylib 圆角参数为 0..1 比例，此函数把像素半径换算为比例
float roundness(float radiusPx, Rectangle rect);

// 缓动曲线：smoothstep（两头平滑，先快后慢）
inline float easeInOut(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// 缓出曲线：快速启动，缓慢停止
inline float easeOutCubic(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

void drawPanel(Rectangle rect, Color fill, float radius,
               Color border = Color{255, 255, 255, 40}, float borderWidth = 2.0f);

// 按钮：圆角矩形，hover 时线性过渡变色（hoverAnim 保存动画进度 0..1）
bool drawButton(Rectangle rect, const std::string& label,
                const Font& font, float fontSize,
                float& hoverAnim, bool hovered, bool pressed, bool enabled = true,
                Color accent = Color{90, 140, 255, 255});

// 滑条：返回新值；drag 表示鼠标是否按住该滑条
float drawSlider(Rectangle rect, float value01, bool drag,
                 Color accent = Color{90, 140, 255, 255});

// 开关：返回新状态
bool drawToggle(Rectangle rect, bool on, bool clicked, Color accent = Color{90, 140, 255, 255});

// 顶部细高亮线（现代面板装饰）
void drawAccentLine(Rectangle rect, Color color, float width = 3.0f);

// 平滑渐变：用多段细条插值绘制，避免 8 位色阶断层（条带化）
void drawGradientH(Rectangle rect, Color c1, Color c2, int strips = 64);
void drawGradientV(Rectangle rect, Color c1, Color c2, int strips = 64);
}
