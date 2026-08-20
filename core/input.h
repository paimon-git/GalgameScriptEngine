#pragma once

#include <raylib.h>

// 输入封装：键盘 / 鼠标的按下、刚按下、刚松开与位置查询
class Input
{
public:
    void update() {}

    bool keyDown(int key) const;
    bool keyPressed(int key) const;
    bool keyReleased(int key) const;

    bool mouseDown(int button) const;
    bool mousePressed(int button) const;
    bool mouseReleased(int button) const;

    Vector2 mousePosition() const;

    // 常用快捷查询
    bool advancePressed() const;                 // 空格 / 回车 / 鼠标左键
    bool ctrlDown() const;                       // 按住 Ctrl = 快进
};
