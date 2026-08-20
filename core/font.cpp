#include "font.h"

#include <cstdio>
#include <set>

namespace
{
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
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/Deng.ttf",
    "C:/Windows/Fonts/NotoSansSC-VF.ttf",
};
}

std::vector<std::string> uiTexts()
{
    return {
        "QLWT 视觉小说引擎",
        "开始游戏", "设置", "退出",
        "文字速度", "音乐音量", "音效音量", "全屏模式",
        "慢", "标准", "快",
        "开", "关", "返回", "上级",
        "自动", "自动播放", "快进", "快进中", "历史",
        "历史记录（L / Esc 关闭）",
        "↑ 查看更早  ·  ↓ 返回最新",
        "F5 存档  ·  F9 读档",
        "Esc 关闭  ·  点击或按数字键执行",
        "存档", "读档", "存档位", "空档位",
        "章节选择", "章节列表", "已观看", "下一章", "未开启", "未解锁",
        "进入 →", "个章节", "未找到带章节的脚本",
        "点击任意处返回标题", "点击返回标题",
        "END",
        "选择", "按下数字键或点击选项",
        "没有更多内容了", "脚本错误",
    };
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
    for (int c = 0x4E00; c <= 0x9FFF; ++c) cps.insert(c);   // 全部常用汉字，杜绝缺字
    cps.insert(0x2018); cps.insert(0x2019); cps.insert(0x201C); cps.insert(0x201D);
    cps.insert(0x2026); cps.insert(0x2192); cps.insert(0x25BC); cps.insert(0x25B6);
    cps.insert(0x00B7); cps.insert(0x2014); cps.insert(0x2018); cps.insert(0x2019);

    for (const auto& t : texts) collectCodepoints(t, cps);

    std::vector<int> codepoints(cps.begin(), cps.end());
    for (const char* path : kFontCandidates)
    {
        if (!FileExists(path)) continue;
        Font f = LoadFontEx(path, 36, codepoints.data(), static_cast<int>(codepoints.size()));
        if (f.texture.id != 0 && f.glyphCount > 0)
        {
            font_ = f;
            ok_ = true;
            SetTextureFilter(font_.texture, TEXTURE_FILTER_BILINEAR);
            printf("[font] loaded '%s' (%d glyphs)\n", path, f.glyphCount);
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
