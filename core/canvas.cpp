#include "canvas.h"

#include <algorithm>

#include <raylib.h>

namespace
{
int gDesignW = 1280;
int gDesignH = 720;
float gScale = 1.0f;
float gOffsetX = 0.0f;
float gOffsetY = 0.0f;
}

namespace canvas
{

void setDesign(int w, int h)
{
    if (w > 0) gDesignW = w;
    if (h > 0) gDesignH = h;
}

int width() { return gDesignW; }
int height() { return gDesignH; }
float scale() { return gScale; }
float offsetX() { return gOffsetX; }
float offsetY() { return gOffsetY; }

void update()
{
    const int winW = GetScreenWidth();
    const int winH = GetScreenHeight();
    if (winW <= 0 || winH <= 0) return;

    // 等比缩放铺满窗口：16:9 的窗口正好铺满，其它比例左右或上下留黑边
    gScale = std::min(static_cast<float>(winW) / static_cast<float>(gDesignW),
                      static_cast<float>(winH) / static_cast<float>(gDesignH));
    gOffsetX = (static_cast<float>(winW) - static_cast<float>(gDesignW) * gScale) * 0.5f;
    gOffsetY = (static_cast<float>(winH) - static_cast<float>(gDesignH) * gScale) * 0.5f;

    // raylib 取鼠标的公式是 (raw + offset) * scale，这里反解成设计坐标：
    // design = (raw - offsetPx) / scale
    SetMouseOffset(-static_cast<int>(gOffsetX), -static_cast<int>(gOffsetY));
    SetMouseScale(1.0f / gScale, 1.0f / gScale);
}

}
