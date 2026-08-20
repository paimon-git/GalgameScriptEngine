#include "game.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "../core/gif.h"
#include "../renderer/renderer.h"

Game::Game(bool loadAssets)
    : loadAssets_(loadAssets)
{
}

Game::~Game()
{
    if (bgm_.frameCount > 0)
    {
        StopMusicStream(bgm_);
        UnloadMusicStream(bgm_);
    }
    for (auto& kv : seCache) UnloadSound(kv.second);
    seCache.clear();
    for (auto& kv : textureCache)
        if (kv.second.id) UnloadTexture(kv.second);
    textureCache.clear();
    backgrounds.clear();
    if (cgAnim_.data) UnloadImage(cgAnim_);
    for (auto& t : cgFrames_) if (t.id) UnloadTexture(t);
    cgFrames_.clear();
    cgVideo_.close();
    if (cgVideoTex_.id) { UnloadTexture(cgVideoTex_); cgVideoTex_ = Texture2D{}; }
    cgVideoMode_ = false;
    cgVideoFramesRead_ = 0;
}

// 载入 CG 资源：GIF/APNG 动画或静态图，成功返回 true
bool Game::prepareCg(const std::string& file)
{
    std::string lower = file;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool isVideo = lower.size() > 4 &&
                   (lower.substr(lower.size() - 4) == ".mp4" ||
                    lower.substr(lower.size() - 4) == ".mov" ||
                    lower.substr(lower.size() - 5) == ".webm" ||
                    lower.substr(lower.size() - 4) == ".mkv" ||
                    lower.substr(lower.size() - 4) == ".avi");

    // 视频 CG：ffmpeg 管道解码
    if (isVideo)
    {
        std::string ffmpeg = VideoPlayer::findFfmpeg();
        if (ffmpeg.empty())
        {
            printf("[game] WARNING: ffmpeg not found (MP4 CG 需要 ffmpeg，"
                   "可设置环境变量 QLWT_FFMPEG 或改用 GIF/APNG)\n");
            return false;
        }
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        if (sw <= 0) sw = 1280;
        if (sh <= 0) sh = 720;
        if (!cgVideo_.open(ffmpeg, file, sw, sh))
        {
            printf("[game] WARNING: cannot open video: %s\n", file.c_str());
            return false;
        }
        cgVideoRgba_.assign(static_cast<size_t>(sw) * sh * 4, 0);
        Image img{};
        img.width = sw;
        img.height = sh;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.data = cgVideoRgba_.data();
        cgVideoTex_ = LoadTextureFromImage(img);
        cgVideoMode_ = true;
        cgVideoFramesRead_ = 0;
        cgFrameTimer_ = 0.0f;   // 重置播放计时，否则第二次播放同一视频会直接跳到结尾
        printf("[game] cg video: %s (%dx%d @30fps)\n", file.c_str(), sw, sh);
        return true;
    }

    bool isAnim = lower.size() > 4 &&
                  (lower.substr(lower.size() - 4) == ".gif" ||
                   lower.substr(lower.size() - 5) == ".apng");

    int frames = 0;
    Image anim{};
    if (isAnim && FileExists(file.c_str()))
        anim = LoadImageAnim(file.c_str(), &frames);

    // 优先使用自实现 GIF 解析（兼容所有 LZW 码流，含 KwKwK）
    std::vector<GifFrame> gifFrames;
    if (isAnim && FileExists(file.c_str()) && loadGifFile(file, gifFrames) && gifFrames.size() > 1)
    {
        if (anim.data) UnloadImage(anim);
        cgFrameCount_ = static_cast<int>(gifFrames.size());
        cgFrameIdx_ = 0;
        cgFrameTimer_ = 0.0f;
        cgFrameDelays_.clear();
        for (auto& t : cgFrames_) if (t.id) UnloadTexture(t);
        cgFrames_.clear();
        for (const auto& fr : gifFrames)
        {
            Image img{};
            img.width = fr.width;
            img.height = fr.height;
            img.mipmaps = 1;
            img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            img.data = const_cast<unsigned char*>(fr.rgba.data());
            Texture2D tex = LoadTextureFromImage(img);
            cgFrames_.push_back(tex);
            cgFrameDelays_.push_back(fr.delay);
        }
        printf("[game] cg gif: %s (%d frames)\n", file.c_str(), cgFrameCount_);
        return true;
    }

    if (anim.data && frames > 1)
    {
        cgAnim_ = anim;
        cgFrameCount_ = frames;
        cgFrameIdx_ = 0;
        cgFrameTimer_ = 0.0f;
        cgFrameDelays_.assign(frames, 0.1f);
        for (auto& t : cgFrames_) if (t.id) UnloadTexture(t);
        cgFrames_.clear();
        int fh = anim.height / frames;
        for (int i = 0; i < frames; ++i)
        {
            Image frame = ImageFromImage(anim, {0, static_cast<float>(i * fh),
                                                static_cast<float>(anim.width),
                                                static_cast<float>(fh)});
            Texture2D tex = LoadTextureFromImage(frame);
            UnloadImage(frame);
            cgFrames_.push_back(tex);
        }
        printf("[game] cg anim: %s (%d frames)\n", file.c_str(), frames);
        return true;
    }

    if (anim.data) UnloadImage(anim);
    cgFrameCount_ = 0;
    cgFrames_.clear();
    Texture2D t = cachedTexture(file);
    if (!t.id) return false;
    cgTexture_ = t;
    cgHold_ = 3.0f;
    printf("[game] cg image: %s (3s auto)\n", file.c_str());
    return true;
}

Texture2D Game::loadTexture(const std::string& path)
{
    if (!loadAssets_) return Texture2D{};
    if (!FileExists(path.c_str()))
    {
        printf("[game] WARNING: texture not found: %s\n", path.c_str());
        return Texture2D{};
    }
    return LoadTexture(path.c_str());
}

Texture2D Game::cachedTexture(const std::string& path)
{
    auto it = textureCache.find(path);
    if (it != textureCache.end()) return it->second;
    Texture2D t = loadTexture(path);
    textureCache[path] = t;
    return t;
}

float Game::parsePosition(const std::string& pos)
{
    if (pos == "left") return 0.18f;
    if (pos == "center") return 0.50f;
    if (pos == "right") return 0.82f;
    return static_cast<float>(std::atof(pos.c_str()));
}

Color Game::colorOf(const std::string& name) const
{
    auto it = characters.find(name);
    return it == characters.end() ? Color{255, 255, 255, 255} : it->second.nameColor;
}

Character* Game::find(const std::string& name)
{
    auto it = characters.find(name);
    return it == characters.end() ? nullptr : &it->second;
}

void Game::initCharacter(const std::string& name, const std::string& texture,
                         unsigned int color, const std::string& position)
{
    Character c;
    c.name = name;
    c.bodyPath = texture;
    c.body = cachedTexture(texture);
    c.nameColor = Color{static_cast<unsigned char>((color >> 16) & 0xFF),
                        static_cast<unsigned char>((color >> 8) & 0xFF),
                        static_cast<unsigned char>(color & 0xFF), 255};
    c.anchorX = parsePosition(position);
    bool isNew = characters.find(name) == characters.end();
    characters[name] = c;
    if (isNew) drawOrder.push_back(name);
    printf("[game] character '%s' ready\n", name.c_str());
}

void Game::setFaceDir(const std::string& name, const std::string& dir)
{
    Character* c = find(name);
    if (!c)
    {
        printf("[game] WARNING: setFaceDir unknown character '%s'\n", name.c_str());
        return;
    }
    c->faceDir = dir;
    // 自动加载默认表情，让角色一登场就有面部
    std::string normal = dir + "/normal.png";
    if (FileExists(normal.c_str())) changeFace(name, "normal.png");
}

void Game::changeFace(const std::string& name, const std::string& file)
{
    Character* c = find(name);
    if (!c)
    {
        printf("[game] WARNING: changeFace unknown character '%s'\n", name.c_str());
        return;
    }
    std::string path = file;
    if (!c->faceDir.empty() && file.find('/') == std::string::npos &&
        file.find('\\') == std::string::npos)
    {
        path = c->faceDir + "/" + file;
    }
    Texture2D t = cachedTexture(path);
    if (t.id)
    {
        c->face = t;
        c->facePath = path;
        printf("[game] face '%s' -> %s\n", name.c_str(), path.c_str());
    }
}

void Game::initBg(const std::string& id, const std::string& texture)
{
    backgrounds[id] = cachedTexture(texture);
    bgPaths_[id] = texture;
    printf("[game] bg '%s' ready\n", id.c_str());
}

void Game::showBg(const std::string& id)
{
    if (backgrounds.count(id) == 0)
    {
        printf("[game] WARNING: unknown bg '%s'\n", id.c_str());
        return;
    }
    currentBgId = id;
}

void Game::showCharacter(const std::string& name)
{
    Character* c = find(name);
    if (!c)
    {
        printf("[game] WARNING: showCharacter unknown character '%s'\n", name.c_str());
        return;
    }
    c->targetAlpha = 255.0f;
    if (c->alpha <= 0.01f) c->fadeT = 0.0f;   // 从完全隐藏开始淡入
}

void Game::hideCharacter(const std::string& name)
{
    Character* c = find(name);
    if (!c)
    {
        printf("[game] WARNING: hideCharacter unknown character '%s'\n", name.c_str());
        return;
    }
    c->targetAlpha = 0.0f;
}

void Game::forceHideCharacter(const std::string& name)
{
    Character* c = find(name);
    if (!c) return;
    c->targetAlpha = 0.0f;
    c->alpha = 0.0f;
    c->fadeT = 0.0f;
}

void Game::setSpeaking(const std::string& name)
{
    speakingName = name;
}

void Game::moveCharacter(const std::string& name, const std::string& position)
{
    Character* c = find(name);
    if (!c)
    {
        printf("[game] WARNING: move unknown character '%s'\n", name.c_str());
        return;
    }
    float x = parsePosition(position);
    c->moveTo(x, 0.8f);
    printf("[game] move '%s' -> %s\n", name.c_str(), position.c_str());
}

void Game::showCg(const std::string& file)
{
    if (!loadAssets_) return;
    if (!prepareCg(file))
    {
        printf("[game] WARNING: cg not found: %s\n", file.c_str());
        return;
    }
    if (cgActive_)
    {
        pendingCgFile_ = file;
        if (cgPhase_ == CgPhase::Shown) cgPhase_ = CgPhase::ToBlackOut;
        printf("[game] cg queued: %s\n", file.c_str());
        return;
    }
    cgActive_ = true;
    cgPhase_ = CgPhase::ToBlack;
    cgFade_ = 0.0f;
    cgBlack_ = 0.0f;
    cgPlaybackDone_ = false;
    cgAutoFinished_ = false;
    cgFile_ = file;
}

void Game::hideCg()
{
    if (cgActive_) cgPhase_ = CgPhase::ToBlackOut;
}

void Game::capture(SaveData& out) const
{
    out.bg = currentBgId;
    out.bgs.clear();
    for (const auto& kv : bgPaths_) out.bgs.emplace_back(kv.first, kv.second);
    out.cg = cgActive_ ? cgFile_ : std::string();
    out.chars.clear();
    for (const auto& name : drawOrder)
    {
        auto it = characters.find(name);
        if (it == characters.end()) continue;
        const Character& c = it->second;
        CharSave cs;
        cs.name = c.name;
        cs.bodyPath = c.bodyPath;
        cs.facePath = c.facePath;
        cs.faceDir = c.faceDir;
        cs.anchorX = c.anchorX;
        cs.visible = c.targetAlpha > 0.0f;
        out.chars.push_back(cs);
    }
}

void Game::restore(const SaveData& in)
{
    // 立绘与背景纹理都缓存在 textureCache，这里重新引用即可
    characters.clear();
    drawOrder.clear();
    for (const auto& cs : in.chars)
    {
        Character c;
        c.name = cs.name;
        c.bodyPath = cs.bodyPath;
        c.facePath = cs.facePath;
        c.faceDir = cs.faceDir;
        c.anchorX = cs.anchorX;
        c.body = cachedTexture(cs.bodyPath);
        if (!cs.facePath.empty()) c.face = cachedTexture(cs.facePath);
        c.targetAlpha = cs.visible ? 255.0f : 0.0f;
        c.alpha = cs.visible ? 255.0f : 0.0f;
        c.fadeT = cs.visible ? 1.0f : 0.0f;
        characters[cs.name] = c;
        drawOrder.push_back(cs.name);
    }

    speakingName.clear();
    for (const auto& b : in.bgs) initBg(b.first, b.second);
    if (in.bg.empty()) currentBgId.clear();
    else if (backgrounds.count(in.bg)) currentBgId = in.bg;

    // CG 恢复为直接显示状态
    if (!in.cg.empty())
    {
        showCg(in.cg);
        cgPhase_ = CgPhase::Shown;
        cgFade_ = 1.0f;
        cgBlack_ = 0.0f;
        cgPlaybackDone_ = true;
        cgAutoFinished_ = false;
        pendingCgFile_.clear();
    }
    else
    {
        cgActive_ = false;
        cgPhase_ = CgPhase::Idle;
        cgFade_ = 0.0f;
        cgBlack_ = 0.0f;
        cgFile_.clear();
        pendingCgFile_.clear();
    }
}

void Game::playBgm(const std::string& file)
{
    if (!loadAssets_ || !IsAudioDeviceReady()) return;
    if (!FileExists(file.c_str()))
    {
        printf("[game] WARNING: bgm not found: %s\n", file.c_str());
        return;
    }
    if (bgm_.frameCount > 0)
    {
        StopMusicStream(bgm_);
        UnloadMusicStream(bgm_);
    }
    bgm_ = LoadMusicStream(file.c_str());
    SetMusicVolume(bgm_, bgmVolume_ / 100.0f);
    PlayMusicStream(bgm_);
    printf("[game] bgm: %s\n", file.c_str());
}

void Game::stopBgm()
{
    if (bgm_.frameCount > 0) StopMusicStream(bgm_);
}

void Game::playSe(const std::string& file)
{
    if (!loadAssets_ || !IsAudioDeviceReady()) return;
    auto it = seCache.find(file);
    if (it == seCache.end())
    {
        if (!FileExists(file.c_str()))
        {
            printf("[game] WARNING: se not found: %s\n", file.c_str());
            return;
        }
        Sound s = LoadSound(file.c_str());
        seCache[file] = s;
        it = seCache.find(file);
    }
    SetSoundVolume(it->second, sfxVolume_ / 100.0f);
    PlaySound(it->second);
}

void Game::setVolumes(int bgm, int sfx)
{
    bgmVolume_ = bgm;
    sfxVolume_ = sfx;
    if (bgm_.frameCount > 0) SetMusicVolume(bgm_, bgmVolume_ / 100.0f);
    for (auto& kv : seCache) SetSoundVolume(kv.second, sfxVolume_ / 100.0f);
}

void Game::update(float dt)
{
    for (auto& kv : characters)
    {
        kv.second.update(dt);
        // 说话位移平滑过渡，避免瞬移
        float target = (kv.first == speakingName) ? 8.0f : 0.0f;
        float k = std::min(1.0f, 8.0f * dt);
        kv.second.speakOffset += (target - kv.second.speakOffset) * k;
    }
    if (bgm_.frameCount > 0) UpdateMusicStream(bgm_);

    // CG 过渡：黑屏淡入 -> 揭示 -> 显示 -> 黑屏淡出 -> 恢复场景
    switch (cgPhase_)
    {
    case CgPhase::ToBlack:
        cgBlack_ += dt / 0.35f;
        if (cgBlack_ >= 1.0f)
        {
            cgBlack_ = 1.0f;
            cgFade_ = 1.0f;        // 黑屏全黑：CG 直接完全不透明，不再淡入
            cgPhase_ = CgPhase::Reveal;
        }
        break;
    case CgPhase::Reveal:
        cgBlack_ -= dt / 0.30f;    // 只淡出黑屏，CG 保持完全不透明
        if (cgBlack_ <= 0.0f)
        {
            cgBlack_ = 0.0f;
            cgPhase_ = CgPhase::Shown;
            // 过渡期间又收到新 CG：显示完成后立即进入黑屏切换
            if (!pendingCgFile_.empty()) cgPhase_ = CgPhase::ToBlackOut;
        }
        break;
    case CgPhase::Shown:
        // 播放：GIF/APNG 播完自动结束；静态图 3 秒自动结束
        if (!pendingCgFile_.empty())
        {
            cgPhase_ = CgPhase::ToBlackOut;
            break;
        }
        if (!cgPlaybackDone_)
        {
            if (cgVideoMode_)
            {
                // 视频：按 30fps 节奏从管道取帧上屏
                cgFrameTimer_ += dt;
                int targetIdx = static_cast<int>(cgFrameTimer_ * 30.0f);
                bool eof = false;
                while (cgVideoFramesRead_ < targetIdx)
                {
                    if (!cgVideo_.readFrame(cgVideoRgba_))
                    {
                        eof = true;
                        break;
                    }
                    ++cgVideoFramesRead_;
                    UpdateTexture(cgVideoTex_, cgVideoRgba_.data());
                }
                if (eof)
                {
                    cgPlaybackDone_ = true;
                    cgPhase_ = CgPhase::ToBlackOut;
                }
            }
            else if (cgFrameCount_ > 1)
            {
                cgFrameTimer_ += dt;
                bool done = false;
                while (!cgFrameDelays_.empty() &&
                       cgFrameIdx_ < static_cast<int>(cgFrameDelays_.size()) &&
                       cgFrameTimer_ >= cgFrameDelays_[cgFrameIdx_])
                {
                    cgFrameTimer_ -= cgFrameDelays_[cgFrameIdx_];
                    if (cgFrameIdx_ + 1 >= cgFrameCount_)
                    {
                        done = true;
                        break;
                    }
                    ++cgFrameIdx_;
                }
                if (done)
                {
                    cgFrameIdx_ = cgFrameCount_ - 1;
                    cgPlaybackDone_ = true;
                    cgPhase_ = CgPhase::ToBlackOut;
                }
            }
            else
            {
                cgHold_ -= dt;
                if (cgHold_ <= 0.0f)
                {
                    cgPlaybackDone_ = true;
                    cgPhase_ = CgPhase::ToBlackOut;
                }
            }
        }
        break;
    case CgPhase::ToBlackOut:
        cgBlack_ += dt / 0.35f;
        if (cgBlack_ >= 1.0f)
        {
            cgBlack_ = 1.0f;
            if (!pendingCgFile_.empty())
            {
                // 无缝切换：黑屏保持，直接换新 CG 再淡入（避免淡入淡出两轮）
                std::string next = pendingCgFile_;
                pendingCgFile_.clear();
                if (prepareCg(next))
                {
                    cgFile_ = next;
                    cgFade_ = 1.0f;   // 新 CG 同样直接完全不透明
                    cgPhase_ = CgPhase::Reveal;
                }
                else
                {
                    cgPhase_ = CgPhase::FadeOutBack;
                    cgActive_ = false;
                }
            }
            else
            {
                // 纯隐藏：全黑时移除 CG，黑屏淡出露出场景
                cgActive_ = false;
                cgPhase_ = CgPhase::FadeOutBack;
            }
        }
        break;
    case CgPhase::FadeOutBack:
        cgBlack_ -= dt / 0.35f;
        if (cgBlack_ <= 0.0f)
        {
            cgBlack_ = 0.0f;
            cgFade_ = 0.0f;
            cgPhase_ = CgPhase::Idle;
            cgAutoFinished_ = true;   // 播放结束（自动结束或手动跳过）
            cgFile_.clear();
        }
        break;
    default:
        break;
    }

}

void Game::draw() const
{
    // 背景
    auto bit = backgrounds.find(currentBgId);
    if (bit != backgrounds.end() && bit->second.id)
    {
        Texture2D tex = bit->second;
        float sw = static_cast<float>(GetScreenWidth());
        float sh = static_cast<float>(GetScreenHeight());
        float scale = sw / static_cast<float>(tex.width);
        float h = static_cast<float>(tex.height) * scale;
        DrawTexturePro(tex,
                       {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                       {0, (sh - h) * 0.5f, sw, h},
                       {0, 0}, 0.0f, WHITE);
    }
    else
    {
        ClearBackground(Color{16, 18, 28, 255});
    }

    float sw = static_cast<float>(GetScreenWidth());
    float sh = static_cast<float>(GetScreenHeight());

    // 角色：非说话者先画，说话者最后（置顶）
    for (const auto& name : drawOrder)
    {
        auto it = characters.find(name);
        if (it != characters.end() && name != speakingName)
            it->second.draw(sw, sh, false);
    }
    if (!speakingName.empty())
    {
        auto it = characters.find(speakingName);
        if (it != characters.end()) it->second.draw(sw, sh, true);
    }

    // 全屏 CG（盖在角色上，垫在 UI 下）
    if (cgActive_)
    {
        float fadeA = renderer::easeInOut(cgFade_);
        Texture2D tex = cgTexture_;
        if (cgVideoMode_) tex = cgVideoTex_;
        if (cgFrameCount_ > 1 && !cgFrames_.empty())
            tex = cgFrames_[cgFrameIdx_ % static_cast<int>(cgFrames_.size())];
        if (tex.id)
        {
            float scale = sw / static_cast<float>(tex.width);
            float h = static_cast<float>(tex.height) * scale;
            DrawTexturePro(tex,
                           {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                           {0, (sh - h) * 0.5f, sw, h},
                           {0, 0}, 0.0f,
                           Color{255, 255, 255, static_cast<unsigned char>(fadeA * 255.0f)});
        }
    }

    // CG 过渡黑屏
    if (cgBlack_ > 0.0f)
    {
        float blackA = renderer::easeInOut(cgBlack_);
        DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh),
                      Color{0, 0, 0, static_cast<unsigned char>(blackA * 255.0f)});
    }
}
