#pragma once

#include <memory>
#include <string>
#include <vector>

#include "font.h"
#include "input.h"
#include "settings.h"
#include "time.h"

class Scene;

// 引擎主控：窗口、主循环、场景切换、全局输入/时间/设置/字体
class Engine
{
public:
    // 设计分辨率：所有场景按这个尺寸排版，窗口/全屏时整块画布等比缩放
    static constexpr int kDesignWidth = 1280;
    static constexpr int kDesignHeight = 720;

    bool init(int screenX, int screenY, const char* windowName,
              const std::vector<std::string>& texts,
              bool selftest, int selftestFrames);
    void run();

    void quit() { running_ = false; }
    void switchScene(std::shared_ptr<Scene> scene);
    // 把 settings 里的外观项同步到渲染器（主题色 / 圆角 / 按钮样式）
    void applyTheme();
    float renderScale() const
    {
        if (designW_ > 0 && rt_.texture.id != 0)
            return static_cast<float>(rt_.texture.width) / static_cast<float>(designW_);
        return 1.0f;
    }
    // 自检模式：按状态命名保存一张截图（非自检时无操作）
    void selftestShot(const std::string& tag);
    void cleanupSelftestFiles();

    Input input;
    Time time;
    Settings settings;
    FontManager fonts;

    bool selftest = false;
    bool selftestPassed = false;
    int selftestFrames = 20000;

private:
    void recreateRenderTarget();

    bool running_ = true;
    std::shared_ptr<Scene> scene_;
    std::shared_ptr<Scene> nextScene_;
    bool transitioning_ = false;
    float transitionT_ = 0.0f;
    RenderTexture2D rt_{};
    RenderTexture2D backdrop_{};   // 1/2 设计分辨率的场景快照（毛玻璃用）
    int designW_ = 0;
    int designH_ = 0;
    int rtW_ = 0;
    int rtH_ = 0;
    std::string shotPending_;   // 待截图的标签（在交换缓冲前取像，见 run()）
};
