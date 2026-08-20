#include "time.h"

#include <algorithm>

#include <raylib.h>

void Time::update()
{
    // 限制最大帧间隔，避免卡顿 / 失焦时角色瞬移
    delta_ = std::min(GetFrameTime(), 0.05f);
    elapsed_ += delta_;
    ++frame_;
}
