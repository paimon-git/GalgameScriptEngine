#include "engine.h"

#include <filesystem>
#include <cstdio>
#include <cstdlib>

#include <rlgl.h>

#include "../game/scene.h"
#include "../renderer/renderer.h"

bool Engine::init(int screenX, int screenY, const char* windowName,
                  const std::vector<std::string>& texts,
                  bool selftestMode, int frames)
{
    // VSync: 2x supersampling is done via an offscreen render target,
    // no post-process FXAA so text and thin borders stay clean.
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(screenX, screenY, windowName);
    if (!IsWindowReady()) return false;
    // 禁用 raylib 默认的 ESC 退出键，ESC 交给各场景处理（菜单/返回）
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();

    settings.load("settings.cfg");
    if (settings.fullscreen) SetWindowState(FLAG_FULLSCREEN_MODE);

    fonts.init(texts);
    designW_ = screenX;
    designH_ = screenY;
    rtW_ = 0;
    rtH_ = 0;
    recreateRenderTarget();
    selftest = selftestMode;
    selftestFrames = frames;
    printf("[engine] ready (%dx%d)\n", screenX, screenY);
    return true;
}

void Engine::run()
{
    while (!WindowShouldClose() && running_)
    {
        time.update();
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
            // 2x supersampling: render the scene at double resolution into the
            // offscreen target, then scale it down to the window. All geometry
            // edges are naturally anti-aliased by the downsample.
            BeginTextureMode(rt_);
            ClearBackground(Color{0, 0, 0, 255});
            rlPushMatrix();
            rlScalef(2.0f, 2.0f, 1.0f);
            scene_->draw();
            if (transitioning_)
            {
                float a = renderer::easeInOut(transitionT_) * 255.0f;
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                              Color{0, 0, 0, static_cast<unsigned char>(a)});
            }
            rlPopMatrix();
            EndTextureMode();
            DrawTexturePro(rt_.texture,
                           {0, 0, static_cast<float>(rt_.texture.width),
                            -static_cast<float>(rt_.texture.height)},
                           {0, 0, static_cast<float>(GetScreenWidth()),
                            static_cast<float>(GetScreenHeight())},
                           {0, 0}, 0.0f, WHITE);
        }
        else
        {
            scene_->draw();
            if (transitioning_)
            {
                float a = renderer::easeInOut(transitionT_) * 255.0f;
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                              Color{0, 0, 0, static_cast<unsigned char>(a)});
            }
        }
        EndDrawing();
        if (scene_) scene_->afterDraw();

        if (selftest)
        {
            if (time.frame() == 20) selftestShot("title");
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
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (w == rtW_ && h == rtH_) return;
    if (rt_.texture.id) UnloadRenderTexture(rt_);
    // 2x supersampling render target
    rt_ = LoadRenderTexture(w * 2, h * 2);
    SetTextureFilter(rt_.texture, TEXTURE_FILTER_BILINEAR);
    rtW_ = w;
    rtH_ = h;
}

void Engine::selftestShot(const std::string& tag)
{
    if (!selftest) return;
    std::string name = "shot_" + tag + ".png";
    TakeScreenshot(name.c_str());
    printf("[engine] screenshot saved: %s (frame %d)\n", name.c_str(), time.frame());
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
