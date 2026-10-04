#pragma once

#include <string>

// 用户设置，保存到 exe 同目录 settings.cfg
class Settings
{
public:
    void load(const std::string& path);
    void save(const std::string& path) const;

    int textSpeed = 28;    // 文字速度：每秒显示的字符数
    int bgmVolume = 80;    // 音乐音量 0..100
    int sfxVolume = 80;    // 音效音量 0..100
    bool fullscreen = false;

    // ---- 外观（自定义按钮颜色等）----
    unsigned int uiAccent = 0x5A8CFF;   // 主题色 0xRRGGBB，驱动按钮/滑条/开关/高亮
    int uiCorner = 12;                  // 圆角半径（设计像素）
    int uiButtonStyle = 0;              // 0=深色 1=浅色 2=描边

    int clampVol(int v) const { return v < 0 ? 0 : (v > 100 ? 100 : v); }
    int clampSpeed(int v) const { return v < 5 ? 5 : (v > 200 ? 200 : v); }
    int clampCorner(int v) const { return v < 0 ? 0 : (v > 28 ? 28 : v); }
    int clampStyle(int v) const { return v < 0 ? 0 : (v > 2 ? 2 : v); }
};
