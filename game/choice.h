#pragma once

#include <raylib.h>

#include <string>
#include <utility>
#include <vector>

// 选项面板：遮罩 + 提问 + 可点击/数字键选择的选项列表
class ChoicePanel
{
public:
    void open(const std::string& character, const std::string& question,
              const std::vector<std::pair<std::string, std::string>>& options);
    void close() { if (active_) closing_ = true; }
    bool active() const { return active_; }
    bool closing() const { return closing_; }

    // 带淡入淡出；选中后先淡出，完全消失时返回下标，其余返回 -1
    int update();
    void draw(const Font& font, float screenW, float screenH) const;

private:
    bool active_ = false;
    std::string character_;
    std::string question_;
    std::vector<std::pair<std::string, std::string>> options_;
    int hoverIndex_ = -1;
    std::vector<float> hoverAnim_;
    float fade_ = 0.0f;
    bool closing_ = false;
    int pendingIndex_ = -1;
};
