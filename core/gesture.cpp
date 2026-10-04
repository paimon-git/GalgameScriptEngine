#include "gesture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#ifdef __linux__
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace
{
constexpr int kMaxSlots = 10;
constexpr float kSwipeGain = 1.6f;        // 触摸板滑一整块 ≈ 屏幕的多少倍
constexpr int kTouchHoldFrames = 15;      // ≈0.25s @60fps：这段时间内的滚动事件不再重复处理
constexpr float kDefaultRange = 1024.0f;  // 设备没报范围时的缺省值
// 双指拖动松手后的惯性滑行：每帧按 kMomDecay 衰减，慢到 kMomMin 就停
constexpr float kMomDecay = 0.90f;
constexpr float kMomMin = 0.5f;           // 设计像素 / 帧
constexpr float kVelBlend = 0.7f;         // 速度取指数滑动平均，新样本占这么多
constexpr float kMaxVel = 90.0f;          // 速度上限（设计像素 / 帧），防驱动抽风甩飞画面
// 捏合判定：两指间距要"持续同向"变化到起始间距的这个比例，才算真的在捏合。
// 单纯双指拖动时两指间距会有小幅抖动，若直接按每帧比值缩放，抖动会被放大成画面漂移。
constexpr float kPinchStart = 0.035f;

// 一块多点触控设备当前的解析状态。
//
// 协议 B（现在的主流）：ABS_MT_SLOT 切手指，ABS_MT_TRACKING_ID < 0 表示离开。
// 协议 A（老 Synaptics / Elan 等）：没有 slot / tracking id，每根手指用一次
//   SYN_MT_REPORT 结束，整帧再用 SYN_REPORT 结束。两种都在这里统一成
//   "哪些 slot 上有手指 + 各自坐标"，后面的增量算法就与协议无关了。
struct MtState
{
    int tracking[kMaxSlots];
    int x[kMaxSlots];
    int y[kMaxSlots];
    int slot = 0;

    bool usesProtoA = false;    // 这台设备用过协议 A（粘性）
    bool protoA = false;        // 本帧见过 SYN_MT_REPORT
    int protoACount = 0;        // 本帧已经收完整的手指数
    bool protoASawPos = false;  // 当前这段手指收到过坐标
    int protoAx = 0, protoAy = 0;

    bool hadTwo = false;        // 上一帧是不是两指基准（用来算增量）
    int prevSig = -1;           // 上一帧的"哪几根手指"签名（变手指数时不跳）
    float pinchRef = 0.0f;      // 本次两指手势的起始间距（设备单位）
    float pinchAccum = 0.0f;    // 同方向累积的间距变化；方向一反转就清零
    int pinchSign = 0;
    bool pinching = false;
    float prevMidX = 0.0f, prevMidY = 0.0f, prevDist = 0.0f;

    MtState() { reset(); }

    void reset()
    {
        for (int i = 0; i < kMaxSlots; ++i)
        {
            tracking[i] = -1;   // -1 = 这根手指没按下
            x[i] = 0;
            y[i] = 0;
        }
        slot = 0;
        usesProtoA = false;
        protoA = false;
        protoACount = 0;
        protoASawPos = false;
        protoAx = 0;
        protoAy = 0;
        hadTwo = false;
        prevSig = -1;
        pinchRef = 0.0f;
        pinchAccum = 0.0f;
        pinchSign = 0;
        pinching = false;
        prevMidX = 0.0f;
        prevMidY = 0.0f;
        prevDist = 0.0f;
    }
};

struct Device
{
    int fd = -1;
    std::string path;
    int xMin = 0, xMax = 1024, yMin = 0, yMax = 1024;
    MtState st;
};

std::vector<Device> gDevices;
std::string gInfo = "未启用";

gesture::Frame gFrame;     // 本帧累计（poll 与自检都写这里）
gesture::Frame gCurrent;   // 上一帧快照，场景读这个
MtState gSynthetic;        // --check-gesture / 手喂事件用

int gTouchHoldFrames = 0;

// 惯性：最近一次双指拖动的速度（设计像素 / 帧），松手后继续按这个速度滑
float gVelX = 0.0f, gVelY = 0.0f;
bool gTwoFingerSeen = false;   // 本帧有没有出现过 ≥2 指（由 endFrame 置位）

// 屏幕像素换算用：整块触摸板宽度 ≈ 1 / kSwipeGain 屏
float gNormToScreenX = 1280.0f * kSwipeGain / kDefaultRange;
float gNormToScreenY = 720.0f * kSwipeGain / kDefaultRange;

// 一帧结束：把当前手指状态换算成平移 / 缩放增量。
//   中心点移动 → 双指拖动（手指往右，画面内容也跟着往右）
//   两点距离比 → 捏合缩放
void endFrame(MtState& st)
{
    int idx[kMaxSlots], n = 0;
    for (int i = 0; i < kMaxSlots; ++i)
        if (st.tracking[i] >= 0) idx[n++] = i;

    if (n >= 2)
    {
        const float ax = static_cast<float>(st.x[idx[0]]);
        const float ay = static_cast<float>(st.y[idx[0]]);
        const float bx = static_cast<float>(st.x[idx[1]]);
        const float by = static_cast<float>(st.y[idx[1]]);
        const float mx = (ax + bx) * 0.5f;
        const float my = (ay + by) * 0.5f;
        const float dist = std::hypot(ax - bx, ay - by);
        // 手指组合变了（比如中途放下第三根、或抬起其中一根再换一根）就重新取基准。
        // 否则会拿上一组手指的位置去减，画面会突然跳一下。
        int sig = 0;
        for (int i = 0; i < n; ++i) sig = sig * 31 + idx[i] + 1;
        const bool steady = st.hadTwo && sig == st.prevSig &&
                            st.prevDist > 4.0f && dist > 4.0f;
        if (steady)
        {
            gFrame.panX += (mx - st.prevMidX) * gNormToScreenX;
            gFrame.panY += (my - st.prevMidY) * gNormToScreenY;
        }

        // 捏合判定用"同方向累积"而不是单帧比值：
        //   * 一次有意捏合，间距会连续朝一个方向变，很快超过 kPinchStart；
        //   * 双指平移时间距只是来回抖，方向一反转就清零，攒不起来。
        // 但一旦确认在捏合，就回到逐帧比值 —— 慢慢捏也能平滑地一直缩放。
        if (!steady)
        {
            st.pinchRef = dist;
            st.pinchAccum = 0.0f;
            st.pinchSign = 0;
            st.pinching = false;
        }
        else
        {
            const float dd = dist - st.prevDist;
            if (dd != 0.0f)
            {
                const int sign = dd > 0.0f ? 1 : -1;
                if (st.pinchSign != 0 && sign != st.pinchSign)
                {
                    // 方向反了：这一步算作抖动，不计入累积
                    st.pinchAccum = 0.0f;
                    st.pinchSign = sign;
                }
                else
                {
                    st.pinchSign = sign;
                    st.pinchAccum += dd;
                }
            }
            if (!st.pinching)
            {
                if (st.pinchRef > 4.0f &&
                    std::fabs(st.pinchAccum) >= kPinchStart * st.pinchRef)
                    st.pinching = true;
            }
            else
            {
                // 不设"变化太小就跳过"的阈值：g.zoom 是每帧的倍率、每帧都会清零，
                // 一旦丢掉小的那部分，慢速捏合就永远攒不起来（= 捏合没反应）。
                gFrame.zoom *= std::clamp(dist / st.prevDist, 0.8f, 1.25f);
            }
        }
        st.prevSig = sig;
        st.prevMidX = mx;
        st.prevMidY = my;
        st.prevDist = dist;
        st.hadTwo = true;
        gFrame.active = true;
        gTwoFingerSeen = true;
    }
    else
    {
        st.hadTwo = false;
        st.prevSig = -1;
    }
    gFrame.fingers = n;
    if (n > 0) gTouchHoldFrames = kTouchHoldFrames;
}

// 一帧收尾：把本帧的双指拖动记成速度；手指不在了就继续滑行并衰减。
// 放在这里而不是各场景里，是为了地图平移、列表滚动、设置抽屉都能有同一份惯性。
void commitFrame()
{
    if (gTwoFingerSeen)
    {
        // 手指还在：速度跟着当前帧的位移走；停住不动时它会自然衰减到 0
        gVelX = gVelX * (1.0f - kVelBlend) + gFrame.panX * kVelBlend;
        gVelY = gVelY * (1.0f - kVelBlend) + gFrame.panY * kVelBlend;
        gVelX = std::clamp(gVelX, -kMaxVel, kMaxVel);
        gVelY = std::clamp(gVelY, -kMaxVel, kMaxVel);
    }
    else if (std::fabs(gVelX) > kMomMin || std::fabs(gVelY) > kMomMin)
    {
        gFrame.panX += gVelX;
        gFrame.panY += gVelY;
        gFrame.active = true;
        gVelX *= kMomDecay;
        gVelY *= kMomDecay;
        if (std::fabs(gVelX) < kMomMin) gVelX = 0.0f;
        if (std::fabs(gVelY) < kMomMin) gVelY = 0.0f;
    }
    else
    {
        gVelX = 0.0f;
        gVelY = 0.0f;
    }
    gTwoFingerSeen = false;
}

void applyEvent(MtState& st, unsigned short type, unsigned short code, int value)
{
    if (type == EV_ABS)
    {
        if (code == ABS_MT_SLOT)
            st.slot = std::clamp(value, 0, kMaxSlots - 1);
        else if (code == ABS_MT_TRACKING_ID)
            st.tracking[st.slot] = value;
        else if (code == ABS_MT_POSITION_X)
        {
            st.x[st.slot] = value;
            st.protoAx = value;
            st.protoASawPos = true;
        }
        else if (code == ABS_MT_POSITION_Y)
        {
            st.y[st.slot] = value;
            st.protoAy = value;
            st.protoASawPos = true;
        }
    }
    else if (type == EV_SYN)
    {
        if (code == SYN_MT_REPORT)
        {
            // 协议 A：这一小段是一根手指，攒起来等 SYN_REPORT
            st.usesProtoA = true;
            st.protoA = true;
            if (st.protoASawPos)
            {
                if (st.protoACount < kMaxSlots)
                {
                    st.x[st.protoACount] = st.protoAx;
                    st.y[st.protoACount] = st.protoAy;
                    ++st.protoACount;
                }
                st.protoASawPos = false;
            }
        }
        else if (code == SYN_REPORT)
        {
            if (st.usesProtoA)
            {
                // 协议 A 只能靠"这一帧报了几根"来判断手指在不在
                if (!st.protoA) st.protoACount = 0;   // 整帧一根手指都没报 = 全部抬起
                for (int i = 0; i < kMaxSlots; ++i)
                    st.tracking[i] = (i < st.protoACount) ? 0 : -1;
                st.protoA = false;
                st.protoACount = 0;
                st.protoASawPos = false;
            }
            endFrame(st);
        }
    }
}
} // namespace

namespace gesture
{
void setRanges(int xMin, int xMax, int yMin, int yMax)
{
    if (xMax > xMin) gNormToScreenX = 1280.0f * kSwipeGain / static_cast<float>(xMax - xMin);
    if (yMax > yMin) gNormToScreenY = 720.0f * kSwipeGain / static_cast<float>(yMax - yMin);
}

void feedEvent(unsigned short type, unsigned short code, int value)
{
    applyEvent(gSynthetic, type, code, value);
}

Frame take()
{
    commitFrame();             // 测试路径：自检也走一份同样的惯性收尾
    Frame f = gFrame;          // feedEvent 直接累计在 gFrame 里
    gFrame = Frame{};
    gCurrent = Frame{};
    return f;
}

#ifdef __linux__
namespace
{
// 触摸屏是 INPUT_PROP_DIRECT（手指直接点在屏幕上），和"相对"的触摸板不是一回事
bool isTouchscreen(int fd)
{
    unsigned long bits[(INPUT_PROP_MAX / 8) + 1] = {0};
    if (::ioctl(fd, EVIOCGPROP(sizeof(bits)), bits) < 0) return false;
    return (bits[INPUT_PROP_DIRECT / 8] & (1UL << (INPUT_PROP_DIRECT % 8))) != 0;
}

enum class Kind { NotMt, Touchpad, Touchscreen };

// 试探一个 /dev/input/event*：是不是支持多点触控坐标、是触摸板还是触摸屏
Kind probe(const std::string& path, Device& dev, std::string& desc)
{
    desc = "不是多点触控设备";
    int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        desc = "打不开（多半是没有读 /dev/input 的权限）";
        return Kind::NotMt;
    }
    unsigned long absBits[(ABS_MAX / 8) + 1] = {0};
    if (::ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0)
    {
        ::close(fd);
        return Kind::NotMt;
    }
    auto has = [&](int code) { return absBits[code / 8] & (1UL << (code % 8)); };
    if (!has(ABS_MT_POSITION_X) || !has(ABS_MT_POSITION_Y))
    {
        ::close(fd);
        return Kind::NotMt;
    }

    struct input_absinfo xi {}, yi {};
    if (::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &xi) == 0)
        dev.xMin = xi.minimum, dev.xMax = xi.maximum;
    if (::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &yi) == 0)
        dev.yMin = yi.minimum, dev.yMax = yi.maximum;

    char name[256] = {0};
    const bool screen = isTouchscreen(fd);
    ::ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    desc = path + (name[0] ? " (" + std::string(name) + ")" : "") +
           (screen ? " · 触摸屏" : " · 触摸板");
    dev.path = desc;
    dev.fd = fd;
    dev.st.reset();
    return screen ? Kind::Touchscreen : Kind::Touchpad;
}
} // namespace

bool init(bool verbose)
{
    gDevices.clear();
    gFrame = Frame{};
    gCurrent = Frame{};
    gSynthetic.reset();
    gTouchHoldFrames = 0;
    gVelX = 0.0f;
    gVelY = 0.0f;
    gTwoFingerSeen = false;
    DIR* dir = ::opendir("/dev/input");
    if (!dir)
    {
        gInfo = "打不开 /dev/input（权限不够）。想启用捏合缩放："
                "sudo usermod -aG input $USER 然后重新登录";
        printf("[gesture] %s\n", gInfo.c_str());
        return false;
    }
    std::vector<Device> pads, screens;
    while (dirent* ent = ::readdir(dir))
    {
        const std::string name = ent->d_name;
        if (name.rfind("event", 0) != 0) continue;
        Device dev;
        std::string desc;
        const Kind kind = probe("/dev/input/" + name, dev, desc);
        if (verbose) printf("[gesture]   %-16s %s\n", name.c_str(), desc.c_str());
        if (kind == Kind::Touchpad) pads.push_back(std::move(dev));
        else if (kind == Kind::Touchscreen) screens.push_back(std::move(dev));
    }
    ::closedir(dir);

    // 优先只认触摸板；机器上只有触摸屏（平板 / 二合一）时才退回去用它。
    // 两类绝不混在一起，否则屏幕上的手指会被当成板上的第二根，手势立刻乱掉。
    if (!pads.empty())
    {
        for (auto& d : screens) if (d.fd >= 0) ::close(d.fd);
        gDevices = std::move(pads);
    }
    else if (!screens.empty())
    {
        gDevices = std::move(screens);
        printf("[gesture] 没找到触摸板，退而使用触摸屏手势\n");
    }

    if (gDevices.empty())
    {
        gInfo = "读不到触摸板设备（多数是权限问题）。想启用双指手势与捏合缩放："
                "sudo usermod -aG input $USER，然后重新登录（或重启）";
        printf("[gesture] %s\n", gInfo.c_str());
        return false;
    }
    setRanges(gDevices[0].xMin, gDevices[0].xMax, gDevices[0].yMin, gDevices[0].yMax);
    std::string all;
    for (const auto& d : gDevices) all += (all.empty() ? "" : ", ") + d.path;
    gInfo = all;
    printf("[gesture] 触摸板手势已启用：%s\n", gInfo.c_str());
    return true;
}

void poll()
{
    gFrame = Frame{};
    if (gTouchHoldFrames > 0) --gTouchHoldFrames;

    // 每块设备各留一份手指状态：两块设备同时有手指时也不会串味
    for (auto& dev : gDevices)
    {
        struct input_event ev[64];
        ssize_t n = 0;
        while ((n = ::read(dev.fd, ev, sizeof(ev))) > 0)
        {
            const int count = static_cast<int>(n / sizeof(struct input_event));
            for (int i = 0; i < count; ++i)
                applyEvent(dev.st, ev[i].type, ev[i].code, ev[i].value);
        }
    }
    commitFrame();                 // 松手后的惯性滑行在这里产生
    gCurrent = gFrame;
    gCurrent.touching = gTouchHoldFrames > 0;
}
#else
bool init(bool verbose)
{
    (void)verbose;
    gInfo = "非 Linux 平台，手势已降级为 Ctrl+滚轮";
    return false;
}
void poll() {}
#endif

bool available() { return !gDevices.empty(); }
const std::string& info() { return gInfo; }
const Frame& frame() { return gCurrent; }
}
