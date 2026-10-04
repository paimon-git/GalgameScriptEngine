#include "engine.h"

#include <filesystem>
#include <cstdio>
#include <cstdlib>

#include <rlgl.h>

#include "canvas.h"
#include "gesture.h"

#include "../game/scene.h"
#include "../renderer/renderer.h"

bool Engine::init(int screenX, int screenY, const char* windowName,
                  const std::vector<std::string>& texts,
                  bool selftestMode, int frames)
{
    // VSync: 2x supersampling is done via an offscreen render target,
    // no post-process FXAA so text and thin borders stay clean.
    // 窗口允许自由缩放；场景始终按设计分辨率排版，最后整块画布等比缩放上屏
    // HIGHDPI：在 150%/200% 缩放的屏幕上按真实帧缓冲分辨率渲染（否则内容只有
    // 逻辑分辨率，被合成器放大后整屏发虚）。
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(screenX, screenY, windowName);
    if (!IsWindowReady()) return false;
    SetWindowMinSize(640, 360);
    // 禁用 raylib 默认的 ESC 退出键，ESC 交给各场景处理（菜单/返回）
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();

    settings.load("settings.cfg");
    if (settings.fullscreen) SetWindowState(FLAG_FULLSCREEN_MODE);
    applyTheme();

    fonts.init(texts);
    // 排版基准是固定的设计分辨率，screenX/screenY 只是初始窗口大小
    designW_ = kDesignWidth;
    designH_ = kDesignHeight;
    canvas::setDesign(designW_, designH_);
    canvas::update();
    rtW_ = 0;
    rtH_ = 0;
    recreateRenderTarget();
    selftest = selftestMode;
    selftestFrames = frames;
    printf("[engine] ready (design %dx%d, window %dx%d, render %dx%d)\n", designW_, designH_,
           GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight());
    return true;
}

void Engine::run()
{
    while (!WindowShouldClose() && running_)
    {
        time.update();
        gesture::poll();                // 触摸板手势（双指拖动 / 捏合）先读进来
        canvas::update();               // 窗口大小可能被拖动改变，每帧重算缩放
        recreateRenderTarget();         // 画布在屏幕上变大时提高离屏分辨率
        renderer::beginFrame(time.delta());   // 控件动画统一用这一份 dt
        if (selftest && scene_) scene_->debugAuto(time.frame());
        scene_->update(time.delta());

        // Scene switch transition: old scene fades out to black, then new scene fades in.
        if (transitioning_)
        {
            transitionT_ += time.delta() / 0.22f;
            if (transitionT_ >= 1.0f)
            {
                scene_ = std::move(nextScene_);
                nextScene_.reset();
                transitioning_ = false;
                transitionT_ = 0.0f;
            }
        }

        BeginDrawing();
        if (rt_.texture.id)
        {
            // 毛玻璃：需要时先把「背景 + 立绘」渲染到一张低分辨率快照，
            // 对话框面板再从它做高斯模糊（主渲染里那份不能自我取样）
            if (scene_ && scene_->needsBackdrop() && backdrop_.texture.id)
            {
                // 快照尺寸 = 离屏尺寸/2，画进去时的缩放必须用「快照/设计」这个比例，
                // 不能写死 0.5：否则场景只填了快照的一角，模糊会取到错位的内容。
                const float backdropScale =
                    static_cast<float>(backdrop_.texture.width) / static_cast<float>(designW_);
                BeginTextureMode(backdrop_);
                ClearBackground(Color{0, 0, 0, 255});
                rlPushMatrix();
                rlScalef(backdropScale, backdropScale, 1.0f);
                scene_->drawBackdropOnly();
                rlPopMatrix();
                EndTextureMode();
                renderer::setBlurBackdrop(backdrop_.texture, backdropScale);
            }
            else
            {
                renderer::setBlurBackdrop(Texture2D{}, 1.0f);
            }

            // 2x supersampling: render the scene at double resolution into the
            // offscreen target, then scale it down to the window. All geometry
            // edges are naturally anti-aliased by the downsample.
            BeginTextureMode(rt_);
            ClearBackground(Color{0, 0, 0, 255});
            rlPushMatrix();
            const float rs = renderScale();   // 离屏分辨率 / 设计分辨率
            rlScalef(rs, rs, 1.0f);
            scene_->draw();
            if (transitioning_)
            {
                float a = renderer::easeInOut(transitionT_) * 255.0f;
                DrawRectangle(0, 0, designW_, designH_,
                              Color{0, 0, 0, static_cast<unsigned char>(a)});
            }
            rlPopMatrix();
            EndTextureMode();
            // 上屏：先铺黑边，再把画布等比缩放居中贴上去
            ClearBackground(Color{0, 0, 0, 255});
            const float sc = canvas::scale();
            DrawTexturePro(rt_.texture,
                           {0, 0, static_cast<float>(rt_.texture.width),
                            -static_cast<float>(rt_.texture.height)},
                           {canvas::offsetX(), canvas::offsetY(),
                            static_cast<float>(designW_) * sc,
                            static_cast<float>(designH_) * sc},
                           {0, 0}, 0.0f, WHITE);
        }
        else
        {
            ClearBackground(Color{0, 0, 0, 255});
            rlPushMatrix();
            rlTranslatef(canvas::offsetX(), canvas::offsetY(), 0.0f);
            rlScalef(canvas::scale(), canvas::scale(), 1.0f);
            scene_->draw();
            if (transitioning_)
            {
                float a = renderer::easeInOut(transitionT_) * 255.0f;
                DrawRectangle(0, 0, designW_, designH_,
                              Color{0, 0, 0, static_cast<unsigned char>(a)});
            }
            rlPopMatrix();
        }
        // 自检截图：必须在交换缓冲之前把这一帧的像素读走
        if (!shotPending_.empty())
        {
            rlDrawRenderBatchActive();   // 把 raylib 尚未提交的绘制批次刷出去
            // 注意：不要用 LoadImageFromScreen()/TakeScreenshot()，它们在 DPI 缩放
            // 下会按「屏幕尺寸 × 缩放」去读一个没这么大的帧缓冲，多出来的部分是黑的。
            // 这里直接按真实帧缓冲尺寸取像（帧缓冲 = 本次实际绘制的区域）。
            const int shotW = GetRenderWidth();
            const int shotH = GetRenderHeight();
            const std::string name = "shot_" + shotPending_ + ".png";
            Image shot{};
            shot.data = rlReadScreenPixels(shotW, shotH);   // 内部已竖直翻转
            shot.width = shotW;
            shot.height = shotH;
            shot.mipmaps = 1;
            shot.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            ExportImage(shot, name.c_str());
            UnloadImage(shot);
            printf("[engine] screenshot saved: %s (frame %d)\n", name.c_str(), time.frame());
            shotPending_.clear();
        }

        EndDrawing();
        if (scene_) scene_->afterDraw();

        if (selftest)
        {
            // 截标题那一帧的时机：默认第 20 帧，可用 QLWT_SHOT_FRAME 覆盖
            // （标题界面有入场淡入，想截到完全亮起来的画面就把它调大一点）
            static const int shotFrame = [] {
                const char* v = std::getenv("QLWT_SHOT_FRAME");
                return v ? std::atoi(v) : 20;
            }();
            if (time.frame() == shotFrame) selftestShot("title");
        }

        if (selftest && time.frame() > selftestFrames) break;
    }

    if (selftest)
    {
        printf(selftestPassed ? "SELFTEST OK\n" : "SELFTEST FAILED (timeout)\n");
        if (std::getenv("QLWT_KEEP_SHOTS") == nullptr)
            cleanupSelftestFiles();
    }

    CloseAudioDevice();
    CloseWindow();
}

void Engine::applyTheme()
{
    renderer::setTheme(renderer::hexToColor(settings.uiAccent),
                       static_cast<float>(settings.uiCorner),
                       settings.uiButtonStyle);
}

void Engine::switchScene(std::shared_ptr<Scene> scene)
{
    if (!scene_ || transitioning_)
    {
        nextScene_ = std::move(scene);
        if (!scene_)
        {
            scene_ = std::move(nextScene_);
            nextScene_.reset();
        }
        return;
    }
    nextScene_ = std::move(scene);
    transitioning_ = true;
    transitionT_ = 0.0f;
}

void Engine::recreateRenderTarget()
{
    // 离屏分辨率跟着画布在屏幕上的实际大小走：
    //   * 小窗口时至少 2x（超采样，边缘更干净）
    //   * 大窗口 / 全屏时提到 3x、4x，保证上屏是缩小而不是放大（放大就发虚）
    // 用「帧缓冲像素 / 设计像素」来定离屏分辨率：高 DPI 屏幕上帧缓冲比逻辑窗口大，
    // 只按逻辑缩放算的话上屏仍会被放大。
    const float fbW = static_cast<float>(GetRenderWidth());
    const float fbH = static_cast<float>(GetRenderHeight());
    const float screenScale = std::min(fbW / static_cast<float>(designW_),
                                       fbH / static_cast<float>(designH_));
    // 离屏分辨率 = 画布在屏幕上的实际像素尺寸，上屏就是 1:1 拷贝。
    // 以前的「整数倍超采样再缩回屏幕」会让文字经历两次重采样（字形图集先放大、
    // 整屏再缩回），边缘发虚；1:1 既最清晰，也省掉了整屏重采样的开销。
    const int w = std::max(1, static_cast<int>(screenScale * designW_ + 0.5f));
    const int h = std::max(1, static_cast<int>(screenScale * designH_ + 0.5f));
    if (w == rtW_ && h == rtH_) return;
    if (rt_.texture.id) UnloadRenderTexture(rt_);
    rt_ = LoadRenderTexture(w, h);
    SetTextureFilter(rt_.texture, TEXTURE_FILTER_BILINEAR);

    // 毛玻璃取样用的场景快照：1/2 设计分辨率。分辨率越低越省、模糊越柔，
    // 边缘用 CLAMP，避免面板靠近画面边缘时高斯采样绕回另一侧。
    if (backdrop_.texture.id) UnloadRenderTexture(backdrop_);
    backdrop_ = LoadRenderTexture(w / 2, h / 2);
    SetTextureFilter(backdrop_.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(backdrop_.texture, TEXTURE_WRAP_CLAMP);

    rtW_ = w;
    rtH_ = h;
}

void Engine::selftestShot(const std::string& tag)
{
    if (!selftest) return;
    // 真正的取像放到 run() 里、EndDrawing()（交换缓冲）之前做：
    // raylib 的 TakeScreenshot 在交换之后读像素，大概率只能拿到黑屏。
    shotPending_ = tag;
}

void Engine::cleanupSelftestFiles()
{
    std::error_code ec;
    std::filesystem::remove("progress.dat", ec);
    std::filesystem::remove("settings.cfg", ec);
    std::filesystem::remove_all("saves", ec);
    for (const auto& entry : std::filesystem::directory_iterator("."))
    {
        const std::string name = entry.path().filename().string();
        if (name.rfind("shot_", 0) == 0 && name.size() > 4 &&
            name.compare(name.size() - 4, 4, ".png") == 0)
            std::filesystem::remove(entry.path(), ec);
    }
}
