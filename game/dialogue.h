#pragma once

#include <raylib.h>

#include <string>
#include <vector>

// 底部对话框：名字牌、打字机效果、自动换行、继续指示
class DialogueBox
{
public:
    void open(const std::string& name, const std::string& text, Color nameColor,
              const Font& font, float screenW);
    void close() { active_ = false; }
    void update(float dt, float textSpeed, bool revealAll);
    void draw(const Font& font, float screenW, float screenH, float timeSec) const;

    bool active() const { return active_; }
    bool typingFinished() const { return active_ && charsShown_ >= totalChars_; }

    // 打字中点击 => 直接显示全文并返回 false；打完后点击 => 返回 true（推进剧情）
    bool advance();

    const std::string& name() const { return name_; }
    const std::string& text() const { return text_; }
    Color nameColor() const { return nameColor_; }

private:
    Color lerpColor(Color a, Color b, float t) const;
    void wrap(const Font& font, float maxWidth, float fontSize);
    int codepointCount(const std::string& s) const;

    bool active_ = false;
    std::string name_;
    std::string text_;
    Color nameColor_{255, 255, 255, 255};
    std::vector<std::string> lines_;
    int totalChars_ = 0;
    float charsShown_ = 0.0f;
    Color curTheme_{255, 255, 255, 255};   // 当前面板主题色（随说话角色过渡）
};
