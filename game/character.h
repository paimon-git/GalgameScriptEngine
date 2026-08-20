#pragma once

#include <raylib.h>

#include <string>

// 单个角色：名称、默认立绘、表情立绘、对话颜色、屏幕位置、淡入淡出与呼吸动画
struct Character
{
    std::string name;
    std::string bodyPath;          // 本体贴图路径（存档用）
    std::string facePath;          // 当前表情贴图路径（存档用）
    Texture2D body{};              // 默认立绘
    Texture2D face{};              // 当前表情立绘（change_face 替换）
    std::string faceDir;           // 表情贴图目录
    Color nameColor{255, 255, 255, 255};
    float anchorX = 0.5f;          // 0..1 屏宽比例

    float alpha = 0.0f;            // 当前透明度
    float targetAlpha = 0.0f;
    float fadeT = 0.0f;            // 淡入淡出进度（0..1，显示时用缓动曲线）
    float breathPhase = 0.0f;

    // 位置移动补间
    float moveFrom = 0.0f;
    float moveTarget = 0.0f;
    float moveT = 1.0f;            // 1 = 不在移动
    float moveDuration = 0.8f;
    float speakOffset = 0.0f;      // 说话时轻微前移（平滑过渡）

    void update(float dt);
    void draw(float screenW, float screenH, bool speaking) const;
    void moveTo(float x, float duration);
    bool moving() const { return moveT < 1.0f; }

    Texture2D currentTexture() const
    {
        return face.id ? face : body;
    }
};
