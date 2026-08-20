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
    bool init(int screenX, int screenY, const char* windowName,
              const std::vector<std::string>& texts,
              bool selftest, int selftestFrames);
    void run();

    void quit() { running_ = false; }
    void switchScene(std::shared_ptr<Scene> scene);
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
    int designW_ = 0;
    int designH_ = 0;
    int rtW_ = 0;
    int rtH_ = 0;
};
