#include "font.h"

#include "lang.h"

#include <cstdio>
#include <set>

namespace
{
// 字形光栅化尺寸：只光栅化"真正用到的字"之后图集很小，可以开到高分辨率。
// 显示尺寸约 40~65px（设计字号 × 画布缩放），所以 72px 足够清晰。
constexpr int kFontRasterSize = 72;
// 度量校正系数：1.0 = 完全校正（请求的字号 == 字形实际高度）。
// 该字体的 em 只有行高的约 0.68，所以 0.70 ≈ 原来（未校正）的字号大小。
// 清晰度靠"字重 + 高分辨率图集"来保证，而不是把字放大。
constexpr float kGlyphScaleGain = 0.70f;

// 把 UTF-8 文本拆成 Unicode 码点
void collectCodepoints(const std::string& text, std::set<int>& out)
{
    size_t i = 0;
    while (i < text.size())
    {
        unsigned char c = static_cast<unsigned char>(text[i]);
        int cp = 0;
        int len = 1;
        if (c < 0x80) { cp = c; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { ++i; continue; }

        bool ok = (i + len <= text.size());
        for (int k = 1; ok && k < len; ++k)
        {
            unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (ok) out.insert(cp);
        i += len;
    }
}

const char* kFontCandidates[] = {
    // Medium 优先：小字号下笔画比 Regular 更实，抗锯齿后不会灰成一片
    "assets/fonts/NotoSansSC-Medium.ttf",
    // 项目内自备字体（优先）。Linux/macOS 上的中文字体普遍是 .ttc，
    // 而 raylib 只认 .ttf/.otf，所以先用 tools/ttc2ttf.py 抽一个放到这里：
    //   python3 tools/ttc2ttf.py "$(fc-match -f '%{file}' :lang=zh-cn)"
    //       assets/fonts/NotoSansSC-Regular.ttf SC        （或直接 make font）
    "assets/fonts/NotoSansSC-Regular.ttf",
    "assets/fonts/game.ttf",
    "assets/fonts/game.otf",
    // Windows
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/Deng.ttf",
    "C:/Windows/Fonts/NotoSansSC-VF.ttf",
    // Linux / macOS（只列 .ttf / .otf：raylib 不支持 .ttc / .otc）
    "/usr/share/fonts/opentype/noto/NotoSansSC-Regular.otf",
    "/usr/share/fonts/opentype/noto/NotoSerifSC-Regular.otf",
    "/usr/share/fonts/opentype/source-han-sans/SourceHanSansSC-Regular.otf",
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    "/Library/Fonts/Arial Unicode.ttf",
};
}

std::vector<std::string> uiTexts()
{
    // 界面文案全部来自语言文件（assets/lang/*.lang）。字体照它收集字形，
    // 所以新增文案只要写进语言文件就不会缺字，不需要再来这里登记。
    return lang::allValues();
}

FontManager::~FontManager()
{
    if (ok_ && font_.texture.id != 0) UnloadFont(font_);
}

bool FontManager::init(const std::vector<std::string>& texts)
{
    std::set<int> cps;
    // 基础 ASCII + 常用标点/全角区
    for (int c = 0x20; c <= 0x7E; ++c) cps.insert(c);
    for (int c = 0x3000; c <= 0x303F; ++c) cps.insert(c);
    for (int c = 0xFF00; c <= 0xFFEF; ++c) cps.insert(c);
    for (int c = 0x2190; c <= 0x2199; ++c) cps.insert(c);   // 方向箭头 ↑ ↓ → ← 等
    // 不再把整个 CJK 区（2 万多字）都塞进图集：那样图集会被撑到几千像素，
    // 只能按 36px 光栅化，而实际显示需要 40~65px，字形一直被放大采样、边缘发虚。
    // 现在只光栅化"真正用到的字"（界面文案 + assets/scripts 下所有剧本，见 main.cpp）。
    cps.insert(0x2018); cps.insert(0x2019); cps.insert(0x201C); cps.insert(0x201D);
    cps.insert(0x2026); cps.insert(0x2192); cps.insert(0x25BC); cps.insert(0x25B6);
    cps.insert(0x00B7); cps.insert(0x2014); cps.insert(0x2018); cps.insert(0x2019);

    for (const auto& t : texts) collectCodepoints(t, cps);

    std::vector<int> codepoints(cps.begin(), cps.end());
    for (const char* path : kFontCandidates)
    {
        if (!FileExists(path)) continue;
        Font f = LoadFontEx(path, kFontRasterSize, codepoints.data(),
                            static_cast<int>(codepoints.size()));
        if (f.texture.id != 0 && f.glyphCount > 0)
        {
            font_ = f;
            ok_ = true;
            // mipmap：文字经常被缩到更小尺寸显示（UI 小字、标题大小不一），
            // 有 mip 链才能平滑缩小而不是闪烁/走样
            GenTextureMipmaps(&font_.texture);
            // 用三线性：字形是按 72px 光栅化的，实际显示多在 40~60px，
            // 缩小采样要经过 mip 链才不会丢笔画细节（BILINEAR 不走 mip）
            SetTextureFilter(font_.texture, TEXTURE_FILTER_TRILINEAR);
            // 校正字体度量：不同 CJK 字体的行高表差异很大（Noto Sans SC 的
            // ascent+descent 比 em 大不少），不校正的话同一个"字号"渲染出来能小 40%，
            // 笔画细到抗锯齿后只剩中灰，看起来就像蒙了一层灰。
            // 把 baseSize 按实际 em 比例缩小，DrawTextEx / MeasureTextEx 会一起等比例
            // 放大，所有调用点不需要改。
            // 用"设置"探测：它是 uiTexts() 里的固定文案，保证已经被光栅化进图集。
            // （之前用"永"探测，那个字不在图集里，量到的是缺字回退的 '?'，宽度只有一半，
            //   于是 baseSize 算错、字被放大到离谱。）
            float probe = MeasureTextEx(font_, "设置",
                                        static_cast<float>(kFontRasterSize), 0.0f).x * 0.5f;
            if (probe > 1.0f)
            {
                float emRatio = probe / static_cast<float>(kFontRasterSize);
                font_.baseSize = static_cast<int>(
                    kFontRasterSize * emRatio / kGlyphScaleGain + 0.5f);
            }
            printf("[font] loaded '%s' (%d glyphs, atlas %dx%d, raster %dpx)\n",
                   path, f.glyphCount, f.texture.width, f.texture.height, kFontRasterSize);
            // 图集只包含"登记过的字"：如果本次请求的码点有没能光栅化的，
            // 说明界面文案没写进 uiTexts()（显示出来会是缺字问号），这里明确报出来。
            if (f.glyphCount < static_cast<int>(codepoints.size()))
                printf("[font] WARNING: %d/%d 个字形未生成（界面文案可能漏登记 uiTexts）\n",
                       static_cast<int>(codepoints.size()) - f.glyphCount,
                       static_cast<int>(codepoints.size()));
            return true;
        }
    }
    // 全部失败则退回 raylib 默认字体（不支持中文）
    font_ = GetFontDefault();
    ok_ = false;
    printf("[font] WARNING: no CJK font found, falling back to default font\n");
    return false;
}

Vector2 FontManager::measure(const std::string& text, float size) const
{
    return MeasureTextEx(font_, text.c_str(), size, size / 12.0f);
}

void FontManager::draw(const std::string& text, float x, float y, float size,
                       Color color, float spacing, bool shadow) const
{
    if (shadow)
    {
        DrawTextEx(font_, text.c_str(), {x + 2.0f, y + 3.0f}, size,
                   spacing, Color{0, 0, 0, static_cast<unsigned char>(color.a * 0.55f)});
    }
    DrawTextEx(font_, text.c_str(), {x, y}, size, spacing, color);
}

float FontManager::drawWrapped(const std::string& text, float x, float y, float maxWidth,
                               float size, float lineHeight, Color color, float spacing) const
{
    if (text.empty()) return y;
    std::string line;
    float cursorY = y;
    size_t i = 0;
    while (i < text.size())
    {
        int cp = 0;
        int len = 1;
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { cp = c; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { ++i; continue; }

        std::string ch = text.substr(i, len);
        float w = MeasureTextEx(font_, (line + ch).c_str(), size, spacing).x;
        if (cp == '\n' || w > maxWidth)
        {
            if (!line.empty())
            {
                DrawTextEx(font_, line.c_str(), {x, cursorY}, size, spacing, color);
                cursorY += lineHeight;
                line.clear();
            }
            if (cp == '\n') { i += len; continue; }
        }
        line += ch;
        i += len;
    }
    if (!line.empty())
    {
        DrawTextEx(font_, line.c_str(), {x, cursorY}, size, spacing, color);
        cursorY += lineHeight;
    }
    return cursorY;
}
