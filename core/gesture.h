#pragma once

#include <string>

// Linux 触摸板手势（双指拖动 / 捏合缩放）
//
// 为什么要自己读设备：GLFW（raylib 的窗口层）只把触摸板暴露成"滚动轴"，
// 拿不到 pinch 手势事件；而内核的 multi-touch 协议里本来就有两根手指的坐标，
// 自己算一下"中心点移动"和"两点距离比"就是双指拖动和捏合。
//
//   * 只读 /dev/input/event*，不做任何写入
//   * 兼容多点触控协议 A（老设备，SYN_MT_REPORT）与协议 B（ABS_MT_SLOT）
//   * 触摸屏（INPUT_PROP_DIRECT）会被跳过，只认触摸板，免得两类手指混在一起
//   * 读不到设备（不在 input 组 / 没有触摸板）就自动降级：available() == false，
//     界面回退到 Ctrl+滚轮缩放 + 拖动平移
namespace gesture
{
// 启动时调一次：扫描触摸板设备，打印一条日志说明是否启用
// verbose = true 时把每个 /dev/input/event* 的判定结果都打出来（--gesture-monitor 用）
bool init(bool verbose = false);
bool available();
const std::string& info();

// 主循环每帧调一次：把这一帧的手势累计到 frame 里
void poll();

struct Frame
{
    bool active = false;    // 本帧有没有手势
    bool touching = false;  // 最近 250ms 内触摸板上有手指（用来抑制重复的滚动事件）
    int fingers = 0;        // 本帧板上有几根手指（排查手势问题时看这个）
    float panX = 0.0f;      // 双指移动（设计分辨率下的像素，屏幕方向）
    float panY = 0.0f;
    float zoom = 1.0f;      // 捏合倍率（1.0 = 不动；>1 放大）
};
const Frame& frame();

// ---- 纯逻辑核心（供自检用，可以不碰真设备）----
// 喂一个 input_event（type/code/value 就是 struct input_event 的三个字段）
void feedEvent(unsigned short type, unsigned short code, int value);
// 取走这一帧累计的手势并清零（--check-gesture 用它验证算法）
Frame take();
void setRanges(int xMin, int xMax, int yMin, int yMax);
}
