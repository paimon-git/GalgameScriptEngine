#pragma once

#include <memory>
#include <string>

#include <raylib.h>

#include "../core/script.h"
#include "scene.h"

// 开始菜单：左侧按钮 + 静态背景 + 从右侧滑出的设置抽屉
class TitleScene : public Scene
{
public:
    TitleScene(Engine* e, std::string scriptPath,
               std::shared_ptr<Script> script, std::string parseError, bool direct = false);
    ~TitleScene() override;

    void update(float dt) override;
    void draw() override;
    void debugAuto(int frame) override;

private:
    float menuX() const;
    float menuY() const;
    void updateDrawer(float dt);
    void drawDrawer();
    float drawerViewTop() const;
    float drawerViewH() const;
    float drawerMaxScroll() const;

    std::string scriptPath_;
    std::string titleText_;
    std::string parseError_;
    std::shared_ptr<Script> script_;
    bool direct_ = false;          // --direct：开始游戏直接进第一段剧情，跳过章节选择
    ::Texture2D bg_{};
    float hoverAnim_[3] = {0.0f, 0.0f, 0.0f};

    // 设置抽屉
    bool settingsOpen_ = false;
    float drawerT_ = 0.0f;          // 0..1 开合进度
    int dragSlider_ = -1;           // 0..2 速度/音量，3..5 主题色 RGB，6 圆角
    float backHover_ = 0.0f;
    // 抽屉内容滚动
    float drawerScroll_ = 0.0f;
    float drawerScrollTarget_ = 0.0f;
    // 外观控件
    float presetHover_[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float styleHover_[3] = {0, 0, 0};
};
