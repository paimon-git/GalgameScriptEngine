#pragma once

class Engine;

// 场景基类：标题 / 设置 / 游戏 共用同一套更新-绘制循环
class Scene
{
public:
    explicit Scene(Engine* e) : engine_(e) {}
    virtual ~Scene() = default;

    virtual void update(float dt) = 0;
    virtual void draw() = 0;

    // 毛玻璃背景：返回 true 时引擎会额外用 drawBackdropOnly() 把「背景 + 立绘」
    // 渲染到一张低分辨率快照上，供对话框做高斯模糊取样。
    virtual bool needsBackdrop() const { return false; }
    virtual void drawBackdropOnly() {}

    // 自检模式钩子：帧号驱动自动操作
    virtual void debugAuto(int /*frame*/) {}
    // 自检模式钩子：整帧绘制完成（EndDrawing 之后）调用
    virtual void afterDraw() {}

protected:
    Engine* engine() const { return engine_; }

    // 场景入场淡入：从黑屏缓动淡出（smoothstep）
    void updateEntryFade(float dt)
    {
        if (entryT_ < 1.0f)
        {
            entryT_ += dt / 0.40f;
            if (entryT_ > 1.0f) entryT_ = 1.0f;
        }
    }
    float entryFadeAlpha() const
    {
        float t = entryT_ * entryT_ * (3.0f - 2.0f * entryT_);
        return (1.0f - t) * 255.0f;
    }

private:
    Engine* engine_ = nullptr;
    float entryT_ = 0.0f;
};
