#pragma once

#include <raylib.h>

#include <string>
#include <vector>

// 中文字体管理：按“实际用到的字符”收集字形，避免生成超大字体图集
class FontManager
{
public:
    ~FontManager();
    // texts：所有会显示到屏幕上的文本（脚本内容 + 界面文案）
    bool init(const std::vector<std::string>& texts);

    const Font& font() const { return font_; }
    bool valid() const { return ok_; }

    Vector2 measure(const std::string& text, float size) const;
    void draw(const std::string& text, float x, float y, float size,
              Color color, float spacing = 1.2f, bool shadow = false) const;
    // 按宽度自动换行绘制，返回最终 Y 坐标
    float drawWrapped(const std::string& text, float x, float y, float maxWidth,
                      float size, float lineHeight, Color color, float spacing = 1.2f) const;

private:
    Font font_{};
    bool ok_ = false;
};

// 界面固定文案（集中管理，便于字体收集）
std::vector<std::string> uiTexts();
