#pragma once

#include <string>
#include <vector>

// GIF 解析（自实现，绕开 raylib 对特殊 LZW 码流的兼容问题）
struct GifFrame
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;   // RGBA 每像素
    float delay = 0.1f;                // 秒
};

// 解析多帧 GIF；返回 false 表示失败或没有有效帧
bool loadGifFile(const std::string& path, std::vector<GifFrame>& out);
