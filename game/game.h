#pragma once

#include "../core/gif.h"
#include "../core/save.h"
#include "../core/video.h"
#include <raylib.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "character.h"

// 游戏状态：背景、角色、立绘资源缓存
class Game
{
public:
    explicit Game(bool loadAssets = true);
    ~Game();

    void initCharacter(const std::string& name, const std::string& texture,
                       unsigned int color, const std::string& position);
    void setFaceDir(const std::string& name, const std::string& dir);
    void changeFace(const std::string& name, const std::string& file);
    void initBg(const std::string& id, const std::string& texture);
    void showBg(const std::string& id);
    void showCharacter(const std::string& name);
    void hideCharacter(const std::string& name);
    void forceHideCharacter(const std::string& name);   // 黑屏过渡中直接完全隐藏
    void setSpeaking(const std::string& name);
    void moveCharacter(const std::string& name, const std::string& position);

    // 全屏 CG
    void showCg(const std::string& file);
    void hideCg();
    bool cgActive() const { return cgActive_; }
    float cgFade() const { return cgFade_; }
    float cgBlack() const { return cgBlack_; }
    Texture2D cgTexture() const { return cgTexture_; }
    bool cgFullyShown() const { return cgActive_ && cgPhase_ == CgPhase::Shown; }
    bool cgAutoFinished() const { return cgAutoFinished_; }   // 播放结束且退出过渡完成
    void resetCgAuto() { cgAutoFinished_ = false; }

    // 存档 / 读档
    void capture(SaveData& out) const;
    void restore(const SaveData& in);

    // 音频
    void playBgm(const std::string& file);
    void stopBgm();
    void playSe(const std::string& file);
    void setVolumes(int bgm, int sfx);

    void update(float dt);
    void draw() const;

    bool hasCharacter(const std::string& name) const { return characters.count(name) != 0; }
    Color colorOf(const std::string& name) const;

    // 立绘坐标换算：left / center / right / 0..1 小数
    static float parsePosition(const std::string& pos);

private:
    Texture2D loadTexture(const std::string& path);
    Texture2D cachedTexture(const std::string& path);
    bool prepareCg(const std::string& file);
    Character* find(const std::string& name);

    bool loadAssets_ = true;
    std::unordered_map<std::string, Character> characters;
    std::unordered_map<std::string, Texture2D> backgrounds;
    std::unordered_map<std::string, std::string> bgPaths_;
    std::unordered_map<std::string, Texture2D> textureCache;
    std::string currentBgId;
    std::string speakingName;
    std::vector<std::string> drawOrder;   // 出场顺序
    Music bgm_{};
    std::unordered_map<std::string, Sound> seCache;
    int bgmVolume_ = 80;
    int sfxVolume_ = 80;

    // CG 过渡状态机：黑屏淡入 -> 揭示 -> 显示 -> 黑屏淡出 -> 恢复场景
    enum class CgPhase { Idle, ToBlack, Reveal, Shown, ToBlackOut, FadeOutBack };
    bool cgActive_ = false;
    CgPhase cgPhase_ = CgPhase::Idle;
    float cgFade_ = 0.0f;
    float cgBlack_ = 0.0f;
    Texture2D cgTexture_{};
    std::string pendingCgFile_;   // 等待当前过渡完成后再切换的 CG
    std::string cgFile_;          // 当前 CG 文件路径

    // 动画 CG（GIF / APNG）
    Image cgAnim_{};
    std::vector<Texture2D> cgFrames_;
    int cgFrameCount_ = 0;
    int cgFrameIdx_ = 0;
    float cgFrameTimer_ = 0.0f;
    std::vector<float> cgFrameDelays_;
    float cgHold_ = 3.0f;          // 静态图片自动结束秒数
    bool cgPlaybackDone_ = false;
    bool cgAutoFinished_ = false;

    // MP4 等视频 CG（ffmpeg 管道解码）
    VideoPlayer cgVideo_;
    Texture2D cgVideoTex_{};
    bool cgVideoMode_ = false;
    int cgVideoFramesRead_ = 0;
    std::vector<unsigned char> cgVideoRgba_;
};
