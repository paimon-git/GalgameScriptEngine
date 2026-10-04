#include "renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include <raylib.h>

namespace renderer
{
namespace
{
// ---- 毛玻璃：高斯模糊着色器 -----------------------------------------------
// 场景快照（低分辨率）-> 二维高斯采样 -> 叠加 tint -> 按圆角 SDF 裁剪。
// 采样偏移在「快照纹素」单位里算，所以快照分辨率越低模糊越强、开销越小。
const char* kBlurVert = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
)";

const char* kBlurFrag = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 uTexel;      // 1 / 快照尺寸
uniform float uStep;      // 采样间隔（快照纹素）
uniform int uSamples;     // 单边采样数
uniform vec2 uUVMin;      // 面板左上角在快照里的 uv
uniform vec2 uUVSize;     // 面板在快照里的 uv 尺寸
uniform vec2 uPanelSize;  // 面板尺寸（设计像素）
uniform float uRadius;    // 圆角半径（设计像素）
uniform vec4 uTint;       // 叠加色（a 为混入比例）
out vec4 finalColor;

void main()
{
    // 二维高斯：先沿 x 再沿 y 逐步叠加（权重为可分离分布的乘积）
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    const float sigma = 1.8;
    for (int i = -3; i <= 3; ++i)
    {
        if (i < -uSamples || i > uSamples) continue;
        for (int j = -3; j <= 3; ++j)
        {
            if (j < -uSamples || j > uSamples) continue;
            float w = exp(-(float(i*i) + float(j*j))/(2.0*sigma*sigma));
            vec2 off = vec2(float(i), float(j))*uTexel*uStep;
            acc += texture(texture0, fragTexCoord + off).rgb*w;
            wsum += w;
        }
    }
    vec3 blurred = acc/wsum;
    vec3 col = mix(blurred, uTint.rgb, uTint.a);

    // 圆角遮罩：和 drawPanel 同样的圆角，边缘 1px 抗锯齿
    vec2 t = (fragTexCoord - uUVMin)/uUVSize;
    vec2 p = (t - 0.5)*uPanelSize;
    vec2 half = uPanelSize*0.5;
    float r = min(uRadius, min(half.x, half.y));
    vec2 q = abs(p) - half + r;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    float aa = max(fwidth(d), 0.6);
    float a = 1.0 - smoothstep(-aa, aa, d);
    finalColor = vec4(col, a*fragColor.a);
}
)";

Shader gBlurShader{};
bool gBlurReady = false;
int gLocTexel = -1, gLocStep = -1, gLocSamples = -1, gLocUVMin = -1,
    gLocUVSize = -1, gLocPanel = -1, gLocRadius = -1, gLocTint = -1;
Texture2D gBackdrop{};
float gBackdropScale = 0.5f;

// ---- 主题状态 --------------------------------------------------------------
Color gAccent{90, 140, 255, 255};
float gCornerRadius = kCornerRadius;
int gButtonStyle = 0;            // 0=深色 1=浅色 2=描边
float gFrameDelta = 1.0f / 60.0f;

bool ensureBlurShader()
{
    if (gBlurReady) return gBackdrop.id != 0;
    gBlurShader = LoadShaderFromMemory(kBlurVert, kBlurFrag);
    if (gBlurShader.id == 0) return false;
    gLocTexel   = GetShaderLocation(gBlurShader, "uTexel");
    gLocStep    = GetShaderLocation(gBlurShader, "uStep");
    gLocSamples = GetShaderLocation(gBlurShader, "uSamples");
    gLocUVMin   = GetShaderLocation(gBlurShader, "uUVMin");
    gLocUVSize  = GetShaderLocation(gBlurShader, "uUVSize");
    gLocPanel   = GetShaderLocation(gBlurShader, "uPanelSize");
    gLocRadius  = GetShaderLocation(gBlurShader, "uRadius");
    gLocTint    = GetShaderLocation(gBlurShader, "uTint");
    gBlurReady = true;
    return gBackdrop.id != 0;
}

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

// 透明 = 未指定，回退到主题色
Color resolveAccent(Color c) { return c.a == 0 ? gAccent : c; }

Palette gPalette{};
std::map<std::string, Texture2D> gBlurCache;

// HSV -> RGB（h: 0..360, s/v: 0..1），用于按主题色相生成整套暗色面板
Color hsv(float h, float s, float v)
{
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (hp < 1.0f)      { r = c; g = x; }
    else if (hp < 2.0f) { r = x; g = c; }
    else if (hp < 3.0f) { g = c; b = x; }
    else if (hp < 4.0f) { g = x; b = c; }
    else if (hp < 5.0f) { r = x; b = c; }
    else                { r = c; b = x; }
    float m = v - c;
    auto q = [](float f) { return static_cast<unsigned char>(f * 255.0f + 0.5f); };
    return Color{q(r + m), q(g + m), q(b + m), 255};
}

void rebuildPalette(Color accent)
{
    float h = hueOf(accent);
    float s = 0.0f;
    {
        float mx = std::max({accent.r, accent.g, accent.b}) / 255.0f;
        float mn = std::min({accent.r, accent.g, accent.b}) / 255.0f;
        s = mx > 0.0f ? (mx - mn) / mx : 0.0f;
    }
    // 主题色越灰，面板越中性，避免灰主题色时面板染色发脏
    float k = std::min(1.0f, s / 0.60f);

    gPalette.accent        = accent;
    gPalette.surface       = hsv(h, 0.50f * k, 0.125f);
    gPalette.surfaceAlt    = hsv(h, 0.46f * k, 0.170f);
    gPalette.surfaceHot    = hsv(h, 0.44f * k, 0.290f);
    gPalette.surfaceSunken = hsv(h, 0.50f * k, 0.100f);
    gPalette.surface.a       = 247;
    gPalette.surfaceAlt.a    = 240;
    gPalette.surfaceHot.a    = 246;
    gPalette.surfaceSunken.a = 215;
    gPalette.line          = Color{255, 255, 255, 40};
    gPalette.lineStrong    = mix(accent, Color{255, 255, 255, 255}, 0.35f);
    gPalette.lineStrong.a  = 165;
    gPalette.text          = Color{245, 247, 255, 255};
    gPalette.textDim       = Color{172, 182, 208, 255};
    gPalette.textMuted     = Color{126, 131, 148, 255};
    gPalette.highlight     = Color{255, 205, 90, 255};
    gPalette.danger        = Color{255, 120, 120, 255};
}

}

void setTheme(Color accent, float cornerRadiusPx, int buttonStyle)
{
    gAccent = accent;
    gCornerRadius = cornerRadiusPx < 0.0f ? 0.0f : (cornerRadiusPx > 28.0f ? 28.0f : cornerRadiusPx);
    gButtonStyle = (buttonStyle < 0 || buttonStyle > 2) ? 0 : buttonStyle;
    rebuildPalette(accent);
}

Color accent() { return gAccent; }
float cornerRadius() { return gCornerRadius; }
int buttonStyle() { return gButtonStyle; }
const Palette& palette() { return gPalette; }

Texture2D blurredImage(const std::string& path, int blurSize, float downscale)
{
    if (downscale <= 0.0f || downscale > 1.0f) downscale = 1.0f;
    char key[600];
    std::snprintf(key, sizeof(key), "%s#%d@%.2f", path.c_str(), blurSize, downscale);
    auto it = gBlurCache.find(key);
    if (it != gBlurCache.end()) return it->second;

    Texture2D tex{};
    auto t0 = std::chrono::steady_clock::now();
    Image img = LoadImage(path.c_str());
    if (img.data != nullptr)
    {
        if (downscale < 0.999f)
        {
            int w = std::max(1, static_cast<int>(img.width * downscale + 0.5f));
            int h = std::max(1, static_cast<int>(img.height * downscale + 0.5f));
            ImageResize(&img, w, h);
        }
        // 半径按缩放同步缩减，视觉上的模糊半径保持不变
        int r = std::max(1, static_cast<int>(blurSize * downscale + 0.5f));
        ImageBlurGaussian(&img, r);
        tex = LoadTextureFromImage(img);
        if (tex.id) SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }
    gBlurCache[key] = tex;
    printf("[renderer] 背景模糊 '%s' 已缓存（半径 %d，缩放 %.2f，首次耗时 %.1fms）\n",
           path.c_str(), blurSize, downscale,
           std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count());
    return tex;
}

Texture2D loadSmoothTexture(const std::string& path)
{
    Texture2D tex = LoadTexture(path.c_str());
    if (tex.id == 0) return tex;
    // 对照排查用：设了 QLWT_NO_SMOOTH 就退回 raylib 默认的最近邻采样
    if (std::getenv("QLWT_NO_SMOOTH") == nullptr)
    {
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    }
    return tex;
}

Color mix(Color a, Color b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return Color{static_cast<unsigned char>(a.r + (b.r - a.r) * t),
                 static_cast<unsigned char>(a.g + (b.g - a.g) * t),
                 static_cast<unsigned char>(a.b + (b.b - a.b) * t),
                 static_cast<unsigned char>(a.a + (b.a - a.a) * t)};
}

float hueOf(Color c)
{
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    float d = mx - mn;
    if (d <= 0.0001f) return 210.0f;   // 灰阶：给一个中性蓝
    float h;
    if (mx == r)      h = 60.0f * std::fmod((g - b) / d, 6.0f);
    else if (mx == g) h = 60.0f * ((b - r) / d + 2.0f);
    else              h = 60.0f * ((r - g) / d + 4.0f);
    if (h < 0.0f) h += 360.0f;
    return h;
}

void beginFrame(float dt)
{
    gFrameDelta = dt < 0.0f ? 0.0f : (dt > 0.05f ? 0.05f : dt);
}

float frameDelta() { return gFrameDelta; }

float approach(float current, float target, float speed, float dt)
{
    if (dt <= 0.0f) return current;
    // 一阶低通：v' = target + (v - target) * e^(-speed*dt)
    // 任意帧率下收敛速度一致，且严格单调，不会在目标附近来回跳（闪烁）
    float v = target + (current - target) * std::exp(-speed * dt);
    if (std::fabs(target - v) < 0.0015f) v = target;   // 收敛后吸附，避免无限逼近
    return v;
}

float roundness(float radiusPx, Rectangle rect)
{
    return roundnessOf(radiusPx, rect);
}

void setBlurBackdrop(Texture2D backdrop, float texelsPerPixel)
{
    gBackdrop = backdrop;
    gBackdropScale = texelsPerPixel > 0.0f ? texelsPerPixel : 1.0f;
}

bool blurBackdropReady() { return gBackdrop.id != 0; }

void drawBlurPanel(Rectangle rect, Color tint, float radiusPx,
                   int blurSamples, float blurStep)
{
    if (rect.width < 2.0f || rect.height < 2.0f) return;
    if (!ensureBlurShader()) return;

    // 面板在设计空间里的位置 -> 快照纹理坐标
    const float texW = static_cast<float>(gBackdrop.width);
    const float texH = static_cast<float>(gBackdrop.height);
    const float srcX = rect.x * gBackdropScale;
    const float srcY = rect.y * gBackdropScale;
    const float srcW = rect.width * gBackdropScale;
    const float srcH = rect.height * gBackdropScale;
    // 渲染目标纹理是上下翻转的（raylib 画 RT 用负高度来纠正）。用负高度时，
    // DrawTexturePro 内部会把 source.y 加上高度，所以这里要传「翻转坐标系下的下边缘」
    // = 纹理高 - 面板上边 - 面板高；传 rect.y 会让取样整体偏到画面另一侧。
    const Rectangle src{srcX, texH - srcY - srcH, srcW, -srcH};

    // blurStep 以「设计像素」为单位，换算成快照纹素；这样离屏/快照分辨率怎么变，
    // 模糊的视觉半径都不变。
    const float stepTexels = blurStep * gBackdropScale;
    const float texel[2] = {1.0f / texW, 1.0f / texH};
    // DrawTexturePro 内部实际使用的起点是 src.y + srcH = texH - srcY，
    // uv 在竖直方向递减（尺寸取负），着色器里的局部坐标才是 0..1。
    const float uvMin[2] = {srcX / texW, (texH - srcY) / texH};
    const float uvSize[2] = {srcW / texW, -srcH / texH};
    const float panel[2] = {rect.width, rect.height};
    const float tintv[4] = {tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f,
                            tint.a / 255.0f};

    BeginShaderMode(gBlurShader);
    SetShaderValue(gBlurShader, gLocTexel, texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(gBlurShader, gLocStep, &stepTexels, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gBlurShader, gLocSamples, &blurSamples, SHADER_UNIFORM_INT);
    SetShaderValue(gBlurShader, gLocUVMin, uvMin, SHADER_UNIFORM_VEC2);
    SetShaderValue(gBlurShader, gLocUVSize, uvSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(gBlurShader, gLocPanel, panel, SHADER_UNIFORM_VEC2);
    SetShaderValue(gBlurShader, gLocRadius, &radiusPx, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gBlurShader, gLocTint, tintv, SHADER_UNIFORM_VEC4);
    DrawTexturePro(gBackdrop, src, rect, {0, 0}, 0.0f, WHITE);
    EndShaderMode();
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
        drawRoundedBorder(rect, radius, borderWidth, border);
}

void drawRoundedBorder(Rectangle rect, float radiusPx, float lineThick, Color color)
{
    if (lineThick <= 0.0f || color.a == 0) return;
    // 向内收缩半个到一个线宽，使描边的外缘正好落在 rect 上
    float t = std::min(lineThick, std::min(rect.width, rect.height) * 0.5f);
    if (t <= 0.0f || rect.width <= 0.0f || rect.height <= 0.0f) return;
    Rectangle in{rect.x + t, rect.y + t, rect.width - 2.0f * t, rect.height - 2.0f * t};
    float inner = radiusPx - t;
    float half = std::min(in.width, in.height) * 0.5f;
    if (inner < 0.0f) inner = 0.0f;
    if (inner > half) inner = half;
    DrawRectangleRoundedLinesEx(in, roundnessOf(inner, in), 16, t, color);
}

bool drawButton(Rectangle rect, const std::string& label,
                const Font& font, float fontSize,
                float& hoverAnim, bool hovered, bool pressed, bool enabled, Color accent,
                float alpha)
{
    if (!enabled) { hovered = pressed = false; }
    if (alpha <= 0.01f) return false;   // 完全透明：跳过绘制（不可见时无需推进动画）
    if (alpha > 1.0f) alpha = 1.0f;
    accent = resolveAccent(accent);

    // 悬停动画：帧率无关的指数趋近（单调收敛，不会在目标附近抖动）
    float target = (hovered && enabled) ? 1.0f : 0.0f;
    hoverAnim = approach(hoverAnim, target, 14.0f, gFrameDelta);

    float e = easeInOut(hoverAnim);   // 非线性悬停变色

    Color baseNorm, baseHover, textBase;
    switch (gButtonStyle)
    {
    case 1:   // 浅色：浅底深字
        baseNorm = mix(gPalette.surfaceAlt, Color{255, 255, 255, 255}, 0.88f);
        baseHover = mix(gPalette.surfaceHot, Color{255, 255, 255, 255}, 0.93f);
        baseNorm.a = 235;
        baseHover.a = 248;
        textBase = Color{28, 34, 52, 255};
        break;
    case 2:   // 描边：近乎透明底 + 主题色高亮
        baseNorm = Color{255, 255, 255, 14};
        baseHover = Color{accent.r, accent.g, accent.b, 52};
        textBase = gPalette.text;
        break;
    default:  // 深色：底色随主题色偏移
        baseNorm = gPalette.surfaceAlt;
        baseNorm.a = 230;
        baseHover = mix(gPalette.surfaceHot, accent, 0.45f);
        baseHover.a = 242;
        textBase = gPalette.text;
        break;
    }

    Color base = enabled ? lerpColor(baseNorm, baseHover, e) : gPalette.surfaceSunken;
    Color borderNorm = gPalette.line;
    Color borderHover{static_cast<unsigned char>(accent.r * 0.65f + 255 * 0.35f),
                      static_cast<unsigned char>(accent.g * 0.65f + 255 * 0.35f),
                      static_cast<unsigned char>(accent.b * 0.65f + 255 * 0.35f), 155};
    Color border = enabled ? lerpColor(borderNorm, borderHover, e) : Color{255, 255, 255, 25};
    float r = std::min(gCornerRadius, rect.height * 0.24f);   // 圆角矩形，不做胶囊

    // 按下：在悬停色基础上再压暗一档，而不是瞬间跳到另一个颜色
    if (pressed)
    {
        base = (gButtonStyle == 1)
                   ? Color{static_cast<unsigned char>(base.r * 0.82f),
                           static_cast<unsigned char>(base.g * 0.82f),
                           static_cast<unsigned char>(base.b * 0.82f), base.a}
                   : Color{static_cast<unsigned char>(base.r * 0.62f),
                           static_cast<unsigned char>(base.g * 0.62f),
                           static_cast<unsigned char>(base.b * 0.62f), base.a};
    }

    // 整体透明度（用于抽屉打开时让菜单按钮平滑淡出，而不是"啪"地消失）
    base.a = static_cast<unsigned char>(base.a * alpha);
    border.a = static_cast<unsigned char>(border.a * alpha);
    Color overlay{accent.r, accent.g, accent.b,
                  static_cast<unsigned char>(46 * e * alpha)};

    DrawRectangleRounded(rect, roundnessOf(r, rect), 16, base);
    if (e > 0.01f && enabled && !pressed)
    {
        DrawRectangleRounded(rect, roundnessOf(r, rect), 16, overlay);
    }
    drawRoundedBorder(rect, r, 1.5f, border);

    if (enabled)
    {
        Vector2 m = MeasureTextEx(font, label.c_str(), fontSize, fontSize / 10.0f);
        unsigned char ta = static_cast<unsigned char>((215 + 40 * e) * alpha);
        Color textColor = textBase;
        textColor.a = ta;
        DrawTextEx(font, label.c_str(),
                   {rect.x + (rect.width - m.x) * 0.5f,
                    rect.y + (rect.height - m.y) * 0.5f - 2.0f},
                   fontSize, fontSize / 10.0f, textColor);
    }
    return enabled && pressed;
}

float drawSlider(Rectangle rect, float value01, bool drag, Color accent)
{
    accent = resolveAccent(accent);
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
    accent = resolveAccent(accent);
    Color offFill = gPalette.surfaceHot;
    offFill.a = 220;
    Color fill = on ? accent : offFill;
    Color border = Color{255, 255, 255, 70};
    float rr = roundnessOf(std::min(gCornerRadius, rect.height * 0.5f), rect);
    DrawRectangleRounded(rect, rr, 16, fill);
    drawRoundedBorder(rect, std::min(gCornerRadius, rect.height * 0.5f), 1.5f, border);
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
