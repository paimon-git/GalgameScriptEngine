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

    int clampVol(int v) const { return v < 0 ? 0 : (v > 100 ? 100 : v); }
    int clampSpeed(int v) const { return v < 5 ? 5 : (v > 200 ? 200 : v); }
};
