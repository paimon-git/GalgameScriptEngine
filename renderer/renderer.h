#pragma once

#include <raylib.h>

#include <string>

// UI 绘制辅助：现代扁平风格的圆角面板 / 按钮 / 滑条 / 文字
namespace renderer
{
// 全项目统一圆角：按钮与面板保持同一圆心角
constexpr float kCornerRadius = 12.0f;

Color hexToColor(unsigned int rgba);   // 0xRRGGBB 或 0xAARRGGBB

// ---- 全局主题（由设置界面驱动）-------------------------------------------
// 主题色 / 圆角半径 / 按钮样式对所有控件生效；控件若显式传入 accent 则以显式值为准。
void setTheme(Color accent, float cornerRadiusPx, int buttonStyle);
Color accent();
float cornerRadius();
int buttonStyle();

// 背景高斯模糊（带进程级缓存）。
// 章节选择这类场景会被反复创建/销毁，如果每次构造都重算模糊，进入界面时会卡一下
// （实测 1280x720 要 70ms）。这里按「路径+半径+缩放」缓存，只算一次。
//   downscale：先缩小再模糊，可以大幅省时间（模糊后再放大几乎看不出差别）
Texture2D blurredImage(const std::string& path, int blurSize, float downscale = 0.5f);

// 载入贴图并设置平滑采样（mipmap + 三线性）。
// raylib 的 LoadTexture 默认是最近邻，只要贴图不是 1:1 绘制就会出锯齿——
// 角色立绘（480x900 → 缩到约 0.7 倍）、章节封面、标题背景都走这个函数。
// 设 QLWT_NO_SMOOTH=1 可以退回默认采样，用于对照排查。
Texture2D loadSmoothTexture(const std::string& path);

// ---- 颜色体系 -------------------------------------------------------------
// 面板底色 / 卡片 / 描边 / 文字都从主题色的【色相】推导（明度、饱和度沿用固定档位），
// 这样换成粉、绿、橙任何主题色，整块 UI 都是一个色系，不会出现"蓝面板配粉按钮"。
struct Palette
{
    Color accent;         // 主题色
    Color surface;        // 面板底
    Color surfaceAlt;     // 卡片 / 列表行底
    Color surfaceHot;     // 悬停底
    Color surfaceSunken;  // 禁用 / 凹陷
    Color line;           // 常规描边、分隔线
    Color lineStrong;     // 强调描边
    Color text;           // 主文字
    Color textDim;        // 次要文字
    Color textMuted;      // 禁用文字
    Color highlight;      // "新内容 / 下一章" 提示色（语义色，全局统一）
    Color danger;         // 错误
};
const Palette& palette();

// 颜色混合（t=0 取 a，t=1 取 b）与取色相，供各界面统一调色
Color mix(Color a, Color b, float t);
float hueOf(Color c);

// 每帧开始调用一次：记录本帧 dt（已钳制到合理区间）。
// 控件动画统一从这里取 dt，避免"在 draw 里各取各的帧时间"导致的抖动/闪烁。
void beginFrame(float dt);
float frameDelta();

// 帧率无关的指数趋近：单调收敛、不会来回振荡（悬停闪烁的根因之一就是
// 线性步进在目标附近反复跨越）。
float approach(float current, float target, float speed, float dt);

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

// 内描边：raylib 的 DrawRectangleRoundedLinesEx 把描边画在矩形【外侧】
// （外缘 = rect 再外扩 lineThick），于是控件会比布局大一圈，
// 处在裁剪区里时还会被切掉半边边框。这个包装把描边改成"贴着外缘向内 lineThick 像素"，
// 保证控件的可见尺寸 == 布局尺寸，边框永远完整。
void drawRoundedBorder(Rectangle rect, float radiusPx, float lineThick, Color color);

// ---- 毛玻璃面板 -----------------------------------------------------------
// 引擎每帧（有需要时）把「背景 + 立绘」渲染到一张低分辨率快照上，
// 用 setBlurBackdrop 交给渲染器；对话框这类面板就用它做高斯模糊。
//
//   texelsPerPixel：快照纹理像素 / 设计像素 的比例（快照按 1/2 设计分辨率渲染时为 0.5）
void setBlurBackdrop(Texture2D backdrop, float texelsPerPixel);
bool blurBackdropReady();

// 画一块毛玻璃：rect 内部是「背后场景的高斯模糊 + tint 叠加色」，圆角与
// drawPanel 一致；调用方自己再画边框即可（边框保持原有样式不变）。
//   tint.a 越大颜色越实（0 = 纯模糊，1 = 完全被颜色盖住）
//   blurSamples：单边采样数（3 -> 7x7 次采样），blurStep：采样间隔（设计像素）
void drawBlurPanel(Rectangle rect, Color tint, float radiusPx,
                   int blurSamples = 3, float blurStep = 3.0f);

// 按钮：圆角矩形，hover 时平滑过渡变色（hoverAnim 保存动画进度 0..1）
// accent 传透明色表示"使用当前主题色"
bool drawButton(Rectangle rect, const std::string& label,
                const Font& font, float fontSize,
                float& hoverAnim, bool hovered, bool pressed, bool enabled = true,
                Color accent = Color{0, 0, 0, 0}, float alpha = 1.0f);

// 滑条：返回新值；drag 表示鼠标是否按住该滑条；accent 传透明色 = 主题色
float drawSlider(Rectangle rect, float value01, bool drag,
                 Color accent = Color{0, 0, 0, 0});

// 开关：返回新状态；accent 传透明色 = 主题色
bool drawToggle(Rectangle rect, bool on, bool clicked, Color accent = Color{0, 0, 0, 0});

// 顶部细高亮线（现代面板装饰）
void drawAccentLine(Rectangle rect, Color color, float width = 3.0f);

// 平滑渐变：用多段细条插值绘制，避免 8 位色阶断层（条带化）
void drawGradientH(Rectangle rect, Color c1, Color c2, int strips = 64);
void drawGradientV(Rectangle rect, Color c1, Color c2, int strips = 64);
}
