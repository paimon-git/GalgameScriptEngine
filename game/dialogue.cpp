#include "dialogue.h"

#include <cmath>

#include "../renderer/renderer.h"

namespace
{
int utf8Codepoints(const std::string& s)
{
    int n = 0;
    size_t i = 0;
    while (i < s.size())
    {
        unsigned char c = static_cast<unsigned char>(s[i]);
        i += (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 :
             ((c & 0xF0) == 0xE0) ? 3 : 4;
        ++n;
    }
    return n;
}
}

int DialogueBox::codepointCount(const std::string& s) const
{
    return utf8Codepoints(s);
}

void DialogueBox::open(const std::string& name, const std::string& text, Color nameColor,
                       const Font& font, float screenW)
{
    name_ = name;
    text_ = text;
    nameColor_ = nameColor;
    active_ = true;
    if (curTheme_.r == 255 && curTheme_.g == 255 && curTheme_.b == 255)
        curTheme_ = nameColor;   // 首次直接使用角色色
    totalChars_ = utf8Codepoints(text_);
    charsShown_ = 0.0f;
    lines_.clear();

    float fontSize = 30.0f;
    float maxWidth = screenW - 80.0f - 84.0f;
    wrap(font, maxWidth, fontSize);
}

void DialogueBox::wrap(const Font& font, float maxWidth, float fontSize)
{
    std::string line;
    float spacing = fontSize / 10.0f;
    size_t i = 0;
    while (i < text_.size())
    {
        unsigned char c = static_cast<unsigned char>(text_[i]);
        int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 :
                  ((c & 0xF0) == 0xE0) ? 3 : 4;
        std::string ch = text_.substr(i, len);
        i += len;
        if (ch == "\n")
        {
            lines_.push_back(line);
            line.clear();
            continue;
        }
        float w = MeasureTextEx(font, (line + ch).c_str(), fontSize, spacing).x;
        if (w > maxWidth && !line.empty())
        {
            lines_.push_back(line);
            line = ch;
        }
        else
        {
            line += ch;
        }
    }
    if (!line.empty() || lines_.empty()) lines_.push_back(line);
}

void DialogueBox::update(float dt, float textSpeed, bool revealAll)
{
    if (!active_ || typingFinished()) return;
    // 面板主题色向当前说话角色颜色平滑过渡
    float t = 1.0f - std::exp(-6.0f * dt);
    curTheme_ = lerpColor(curTheme_, nameColor_, t);
    charsShown_ += textSpeed * dt * (revealAll ? 8.0f : 1.0f);
    if (charsShown_ >= totalChars_) charsShown_ = static_cast<float>(totalChars_);
}

Color DialogueBox::lerpColor(Color a, Color b, float t) const
{
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t),
    };
}

bool DialogueBox::advance()
{
    if (!active_) return true;
    if (!typingFinished())
    {
        charsShown_ = static_cast<float>(totalChars_);
        return false;
    }
    return true;
}

void DialogueBox::draw(const Font& font, float screenW, float screenH, float timeSec) const
{
    if (!active_) return;

    float panelW = screenW - 80.0f;
    float panelH = 210.0f;
    float panelX = 40.0f;
    float panelY = screenH - panelH - 28.0f;
    Rectangle panel{panelX, panelY, panelW, panelH};

    // 面板背景随说话角色颜色主题化（千恋万花风格）
    // 面板底色以【主题面板色】为主，只掺一点点说话角色的颜色（先降饱和，
    // 否则角色色偏蓝时会把整块面板带偏，和设置/章节界面看起来不像一套）。
    // "谁在说话"主要靠名字框和面板顶部细线——它们仍然是完整的角色色。
    Color charTint = renderer::mix(curTheme_, renderer::palette().surface, 0.55f);
    Color panelFill = renderer::mix(renderer::palette().surface, charTint, 0.16f);
    panelFill.a = 255;
    Color borderColor = lerpColor(curTheme_, Color{255, 255, 255, 255}, 0.55f);
    borderColor.a = 60;
    // 毛玻璃：面板内部是「背后场景的高斯模糊 + 主题色叠加」，边框另外画（保持原样）。
    // 叠加色的 alpha 控制通透度：越大越接近原来的实心面板（越小越透、背景越明显）。
    // 190 ≈ 七五成面板色：主体仍是面板（文字对比够），同时能看出一层模糊的背景。
    if (renderer::blurBackdropReady())
    {
        Color glassTint = panelFill;
        glassTint.a = 190;
        renderer::drawBlurPanel(panel, glassTint, renderer::kCornerRadius, 3, 6.0f);
        renderer::drawRoundedBorder(panel, renderer::kCornerRadius, 2.0f, borderColor);
    }
    else
    {
        renderer::drawPanel(panel, panelFill, renderer::kCornerRadius, borderColor, 2.0f);
    }
    renderer::drawAccentLine({panel.x, panel.y + 8.0f, panel.width, 4.0f}, curTheme_, 3.0f);

    // 名字牌
    float fs = 26.0f;
    Vector2 nm = MeasureTextEx(font, name_.c_str(), fs, fs / 10.0f);
    float plateW = nm.x + 56.0f;
    float plateH = 46.0f;
    Rectangle plate{panel.x + 28.0f, panel.y - plateH * 0.42f, plateW, plateH};
    renderer::drawPanel(plate, curTheme_, renderer::kCornerRadius, Color{255, 255, 255, 110}, 1.5f);
    DrawTextEx(font, name_.c_str(),
               {plate.x + (plate.width - nm.x) * 0.5f,
                plate.y + (plate.height - nm.y) * 0.5f - 1.0f},
               fs, fs / 10.0f, Color{255, 255, 255, 255});

    // 对话正文（打字机效果）
    float txtSize = 30.0f;
    float spacing = txtSize / 10.0f;
    float lineH = txtSize * 1.55f;
    float txtX = panel.x + 42.0f;
    float txtY = panel.y + 46.0f;
    int shown = static_cast<int>(charsShown_);
    int remaining = shown;

    for (const auto& line : lines_)
    {
        if (remaining <= 0) break;
        int n = utf8Codepoints(line);
        if (remaining < n)
        {
            // 截断到剩余字符
            std::string partial;
            int count = 0;
            size_t i = 0;
            while (i < line.size() && count < remaining)
            {
                unsigned char c = static_cast<unsigned char>(line[i]);
                int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 :
                          ((c & 0xF0) == 0xE0) ? 3 : 4;
                partial += line.substr(i, len);
                i += len;
                ++count;
            }
            DrawTextEx(font, partial.c_str(), {txtX, txtY}, txtSize, spacing,
                       Color{245, 247, 255, 255});
            break;
        }
        DrawTextEx(font, line.c_str(), {txtX, txtY}, txtSize, spacing,
                   Color{245, 247, 255, 255});
        remaining -= n;
        txtY += lineH;
    }

    // 继续指示
    if (typingFinished())
    {
        float pulse = 0.55f + 0.45f * std::sin(timeSec * 4.0f);
        DrawTextEx(font, "▼",
                   {panel.x + panel.width - 46.0f, panel.y + panel.height - 46.0f},
                   24.0f, 1.0f,
                   Color{255, 255, 255, static_cast<unsigned char>(160 + 95 * pulse)});
    }
}
