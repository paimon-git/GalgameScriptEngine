#include "chapter_select.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../core/engine.h"
#include "../core/file_util.h"
#include "../core/save.h"
#include "../renderer/renderer.h"
#include "game_scene.h"
#include "title_scene.h"

namespace
{
const Color kAccent{90, 150, 255, 255};
float corner(Rectangle r)
{
    return renderer::roundness(renderer::kCornerRadius, r);
}

// 圆角缺口：把矩形四角挖成标准内切圆弧（圆心在角内侧 rad 处），
// 用与卡片相同的底色填充缺口，避免“四角画圆”造成的整圆观感。
void drawRoundedCornerNotches(Rectangle r, float rad, Color fill)
{
    if (rad <= 0.0f || fill.a == 0) return;
    const float starts[4] = {180.0f, 0.0f, 180.0f, 0.0f};
    const float dirs[4]   = {1.0f, -1.0f, -1.0f, 1.0f};
    const float dxs[4]    = {1.0f, -1.0f, 1.0f, -1.0f};
    const float dys[4]    = {1.0f, 1.0f, -1.0f, -1.0f};

    for (int c = 0; c < 4; ++c)
    {
        float x0 = (dxs[c] > 0) ? r.x : r.x + r.width;
        float y0 = (dys[c] > 0) ? r.y : r.y + r.height;
        float cx = x0 + dxs[c] * rad;
        float cy = y0 + dys[c] * rad;
        // 四个角都是四分之一圆弧（90°），分 9 段，每段 10°
        float step = dirs[c] * 10.0f;

        Vector2 pts[12];
        int n = 0;
        pts[n++] = {x0, y0};
        pts[n++] = {x0, y0 + dys[c] * rad};
        for (int i = 1; i < 9; ++i)
        {
            float a = (starts[c] + step * i) * DEG2RAD;
            pts[n++] = {cx + rad * std::cos(a), cy + rad * std::sin(a)};
        }
        pts[n++] = {x0 + dxs[c] * rad, y0};
        // 右上/左下是镜像角，环绕方向相反会被背面剔除：
        // 保持角点 P0 在首位，反转其余顶点以翻转环绕方向
        if (c == 1 || c == 2)
        {
            for (int a = 1, b = n - 1; a < b; ++a, --b)
            {
                Vector2 t = pts[a];
                pts[a] = pts[b];
                pts[b] = t;
            }
        }
        DrawTriangleFan(pts, n, fill);
    }
}
}

ChapterSelectScene::ChapterSelectScene(Engine* e, std::string defaultScript,
                                       std::string parseError)
    : Scene(e),
      defaultScript_(std::move(defaultScript)),
      parseError_(std::move(parseError))
{
    if (parseError_.empty() && !defaultScript_.empty())
    {
        std::string text = readFileUtf8(defaultScript_);
        if (!text.empty())
        {
            try
            {
                titleScript_ = std::make_shared<Script>(parseGal(text, defaultScript_));
            }
            catch (const ScriptError&)
            {
            }
        }
    }
    if (FileExists("assets/bg/title.png"))
        bg_ = LoadTexture("assets/bg/title.png");
    scanScripts();
    refreshViewed();
    bigHover_.assign(big_.size(), 0.0f);
}

ChapterSelectScene::~ChapterSelectScene()
{
    if (bg_.id) UnloadTexture(bg_);
    for (auto& kv : coverTex_) if (kv.second.id) UnloadTexture(kv.second);
}

void ChapterSelectScene::scanScripts()
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("assets/scripts", ec))
    {
        if (!entry.is_regular_file(ec)) continue;
        std::string p = entry.path().string();
        std::replace(p.begin(), p.end(), '\\', '/');
        if (p.size() < 4 || p.substr(p.size() - 4) != ".gal") continue;
        std::string stem = entry.path().stem().string();
        if (stem.rfind("test", 0) == 0 || stem[0] == '_') continue;   // 跳过测试脚本

        std::string text = readFileUtf8(p);
        if (text.empty()) continue;
        try
        {
            auto script = std::make_shared<Script>(parseGal(text, p));
            BigChapter bc;
            bc.path = p;
            bc.title = script->title.empty() ? entry.path().stem().string() : script->title;
            bc.script = script;
            if (!script->chapters.empty())
                bc.cover = script->chapters.front().picture;
            big_.push_back(std::move(bc));
        }
        catch (const ScriptError& e)
        {
            printf("[chapter] skip %s: %s\n", p.c_str(), e.what());
        }
    }
    std::sort(big_.begin(), big_.end(),
              [](const BigChapter& a, const BigChapter& b) { return a.path < b.path; });
}

void ChapterSelectScene::refreshViewed()
{
    viewed_.clear();
    for (const auto& bc : big_)
    {
        int count = 0;
        for (const auto& ch : bc.script->chapters)
        {
            if (progressIsViewed(bc.path, ch.id))
            {
                viewed_.insert(bc.path + "|" + std::to_string(ch.id));
                ++count;
            }
        }
        big_[&bc - big_.data()].viewedCount = count;
    }
}

Texture2D ChapterSelectScene::coverTexture(const std::string& path)
{
    auto it = coverTex_.find(path);
    if (it != coverTex_.end()) return it->second;
    Texture2D tex{};
    if (!path.empty() && FileExists(path.c_str()))
        tex = LoadTexture(path.c_str());
    coverTex_[path] = tex;
    return tex;
}

int ChapterSelectScene::nextChapterIndex(const BigChapter& bc) const
{
    for (size_t i = 0; i < bc.script->chapters.size(); ++i)
    {
        const auto& ch = bc.script->chapters[i];
        if (!viewed_.count(bc.path + "|" + std::to_string(ch.id))) return static_cast<int>(i);
    }
    return -1;
}

void ChapterSelectScene::startChapter(const BigChapter& bc, int chapterIdx)
{
    if (chapterIdx < 0)
    {
        printf("[chapter] start whole script '%s'\n", bc.path.c_str());
        engine()->switchScene(std::make_shared<GameScene>(
            engine(), bc.path, bc.script, parseError_, -1));
        return;
    }
    const auto& ch = bc.script->chapters[chapterIdx];
    printf("[chapter] start '%s' 章节%d: %s\n", bc.path.c_str(), ch.id, ch.name.c_str());
    engine()->switchScene(std::make_shared<GameScene>(
        engine(), bc.path, bc.script, parseError_, chapterIdx));
}

void ChapterSelectScene::update(float dt)
{
    updateEntryFade(dt);
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    Vector2 mouse = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool back = IsKeyPressed(KEY_ESCAPE);

    hoverBig_ = -1;
    hoverSub_ = -1;

    if (bigIndex_ < 0)
    {
        // 大章节横向卡片
        float cw = 340.0f;
        float chh = 250.0f;
        float gap = 30.0f;
        float totalW = big_.size() * (cw + gap) - gap;
        float x0 = (w - totalW) * 0.5f;
        float y0 = (h - chh) * 0.5f - 20.0f;
        for (size_t i = 0; i < big_.size(); ++i)
        {
            Rectangle r{x0 + i * (cw + gap), y0, cw, chh};
            if (CheckCollisionPointRec(mouse, r)) hoverBig_ = static_cast<int>(i);
            float target = (hoverBig_ == static_cast<int>(i)) ? 1.0f : 0.0f;
            float speed = 7.0f * GetFrameTime();
            bigHover_[i] += (target > bigHover_[i]) ? speed : -speed;
            if (bigHover_[i] < 0.0f) bigHover_[i] = 0.0f;
            if (bigHover_[i] > 1.0f) bigHover_[i] = 1.0f;
        }
        if (pressed && hoverBig_ >= 0)
        {
            if (big_[hoverBig_].script->chapters.empty())
            {
                startChapter(big_[hoverBig_], -1);
                return;
            }
            bigIndex_ = hoverBig_;
            subHover_.assign(big_[bigIndex_].script->chapters.size(), 0.0f);
            return;
        }
        if (back)
            engine()->switchScene(std::make_shared<TitleScene>(
                engine(), defaultScript_, titleScript_, parseError_));
    }
    else
    {
        // 左侧大图 + 右侧小章节列表（全部可独立进入）
        const BigChapter& bc = big_[bigIndex_];
        float rowH = 76.0f;
        float gap = 14.0f;
        float y0 = 150.0f;
        for (size_t i = 0; i < bc.script->chapters.size(); ++i)
        {
            Rectangle r{540.0f, y0 + i * (rowH + gap), 660.0f, rowH};
            if (CheckCollisionPointRec(mouse, r)) hoverSub_ = static_cast<int>(i);
            float target = (hoverSub_ == static_cast<int>(i)) ? 1.0f : 0.0f;
            float speed = 7.0f * GetFrameTime();
            subHover_[i] += (target > subHover_[i]) ? speed : -speed;
            if (subHover_[i] < 0.0f) subHover_[i] = 0.0f;
            if (subHover_[i] > 1.0f) subHover_[i] = 1.0f;
        }
        if (pressed && hoverSub_ >= 0)
        {
            // 进度锁定：仅已观看章节与下一章可进入
            int nxt = nextChapterIndex(bc);
            bool viewed = viewed_.count(bc.path + "|" +
                                        std::to_string(bc.script->chapters[hoverSub_].id)) != 0;
            if (nxt < 0 || hoverSub_ <= nxt || viewed)
            {
                startChapter(bc, hoverSub_);
                return;
            }
        }
        if (back) bigIndex_ = -1;
    }

    Rectangle backBtn{24.0f, 24.0f, 120.0f, 44.0f};
    if (pressed && CheckCollisionPointRec(mouse, backBtn))
    {
        if (bigIndex_ >= 0) bigIndex_ = -1;
        else
            engine()->switchScene(std::make_shared<TitleScene>(
                engine(), defaultScript_, titleScript_, parseError_));
    }
}

void ChapterSelectScene::draw()
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());

    if (bg_.id)
    {
        float scale = w / static_cast<float>(bg_.width);
        float bh = static_cast<float>(bg_.height) * scale;
        DrawTexturePro(bg_,
                       {0, 0, static_cast<float>(bg_.width), static_cast<float>(bg_.height)},
                       {0, (h - bh) * 0.5f, w, bh},
                       {0, 0}, 0.0f, WHITE);
    }
    else
    {
        ClearBackground(Color{18, 22, 34, 255});
    }
    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h), Color{8, 12, 24, 120});

    engine()->fonts.draw("章节选择", 170.0f, 40.0f, 42.0f, Color{255, 255, 255, 255}, 4.2f, true);

    if (bigIndex_ < 0) drawBigList();
    else drawSubList(big_[bigIndex_]);

    Rectangle backBtn{24.0f, 24.0f, 120.0f, 44.0f};
    Vector2 mouse = GetMousePosition();
    bool hoverBack = CheckCollisionPointRec(mouse, backBtn);
    renderer::drawButton(backBtn, bigIndex_ < 0 ? "返回" : "上级", engine()->fonts.font(),
                         20.0f, backHover_, hoverBack,
                         IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hoverBack);

    if (!parseError_.empty())
        engine()->fonts.draw(parseError_, 60.0f, h - 60.0f, 20.0f,
                             Color{255, 140, 140, 255}, 2.0f);

    // 入场淡入遮罩（最后绘制）
    float fadeA = entryFadeAlpha();
    if (fadeA > 1.0f)
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                      Color{0, 0, 0, static_cast<unsigned char>(fadeA)});
}

void ChapterSelectScene::drawCardTexture(Rectangle r, const std::string& path,
                                         Color cornerFill, float cornerRad)
{
    Texture2D tex = coverTexture(path);
    if (tex.id)
    {
        // 裁剪式铺满：背景完整覆盖卡片区域并居中，不留黑边
        float scale = std::max(r.width / static_cast<float>(tex.width),
                               r.height / static_cast<float>(tex.height));
        float dw = tex.width * scale;
        float dh = tex.height * scale;
        float rs = engine()->renderScale();
        BeginScissorMode(static_cast<int>(r.x * rs), static_cast<int>(r.y * rs),
                         static_cast<int>(r.width * rs), static_cast<int>(r.height * rs));
        DrawTexturePro(tex,
                       {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                       {r.x + (r.width - dw) * 0.5f, r.y + (r.height - dh) * 0.5f, dw, dh},
                       {0, 0}, 0.0f, WHITE);
        EndScissorMode();
    }
    else
    {
        DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y),
                      static_cast<int>(r.width), static_cast<int>(r.height),
                      Color{38, 48, 78, 255});
    }

    // 圆角：四个角用卡片底色填充标准圆弧缺口，与卡片统一圆角矩形
    if (cornerFill.a > 0)
        drawRoundedCornerNotches(r, cornerRad, cornerFill);
}

void ChapterSelectScene::drawBigList()
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    float cw = 340.0f;
    float chh = 250.0f;
    float gap = 30.0f;
    float totalW = big_.size() * (cw + gap) - gap;
    float x0 = (w - totalW) * 0.5f;
    float y0 = (h - chh) * 0.5f - 20.0f;
    for (size_t i = 0; i < big_.size(); ++i)
    {
        Rectangle r{x0 + i * (cw + gap), y0, cw, chh};
        float a = renderer::easeInOut(bigHover_[i]);
        Color border{90, 150, 255, static_cast<unsigned char>(90 + 120 * a)};
        DrawRectangleRounded(r, corner(r), 12, Color{16, 20, 32, 245});
        DrawRectangleRoundedLinesEx(r, corner(r), 12, 2.0f, border);

        // 封面
        Rectangle art{r.x + 8.0f, r.y + 8.0f, r.width - 16.0f, chh - 92.0f};
        // 图片内缩 8px，圆角与卡片圆心同心：半径 = 卡片圆角 - 内缩
        drawCardTexture(art, big_[i].cover, Color{16, 20, 32, 245},
                        renderer::kCornerRadius - 8.0f);
        renderer::drawGradientV({art.x, art.y + art.height * 0.45f,
                                 art.width, art.height * 0.55f},
                                Color{0, 0, 0, 0}, Color{16, 20, 32, 235}, 48);

        // 标题与进度
        engine()->fonts.draw(big_[i].title, r.x + 18.0f, r.y + chh - 74.0f, 26.0f,
                             Color{255, 255, 255, 255}, 2.6f);
        int total = static_cast<int>(big_[i].script->chapters.size());
        std::string prog = std::to_string(big_[i].viewedCount) + " / " + std::to_string(total) +
                           " 已观看";
        engine()->fonts.draw(prog, r.x + 18.0f, r.y + chh - 40.0f, 18.0f,
                             Color{170, 180, 205, 255}, 1.8f);
        float barW = cw - 36.0f;
        float ratio = total > 0 ? static_cast<float>(big_[i].viewedCount) / total : 0.0f;
        DrawRectangleRounded({r.x + 18.0f, r.y + chh - 20.0f, barW, 6.0f}, 3.0f, 6,
                             Color{255, 255, 255, 30});
        if (ratio > 0.0f)
            DrawRectangleRounded({r.x + 18.0f, r.y + chh - 20.0f, barW * ratio, 6.0f}, 3.0f, 6,
                                 kAccent);

        if (hoverBig_ == static_cast<int>(i))
        {
            // 静态光晕：不再用正弦脉冲，避免悬停时边框每帧明暗跳变（闪烁）
            DrawRectangleRoundedLinesEx(r, corner(r), 12, 3.0f,
                                        Color{150, 190, 255, 175});
        }
    }
    if (big_.empty())
        engine()->fonts.draw("未找到带章节的脚本", x0, h * 0.5f, 26.0f,
                             Color{200, 205, 220, 255}, 2.6f);
}

void ChapterSelectScene::drawSubList(const BigChapter& bc)
{
    float w = static_cast<float>(GetScreenWidth());
    float h = static_cast<float>(GetScreenHeight());
    float t = static_cast<float>(engine()->time.elapsed());
    int nextIdx = nextChapterIndex(bc);

    // 左侧：大章节图
    Rectangle left{40.0f, 120.0f, 450.0f, 500.0f};
    DrawRectangleRounded(left, corner(left), 12, Color{16, 20, 32, 245});
    drawCardTexture({left.x + 8.0f, left.y + 8.0f, left.width - 16.0f, left.height - 130.0f},
                    bc.cover, Color{16, 20, 32, 245}, renderer::kCornerRadius - 8.0f);
    renderer::drawGradientV({left.x, left.y + left.height - 170.0f,
                             left.width, 170.0f},
                            Color{0, 0, 0, 0}, Color{16, 20, 32, 245}, 48);
    engine()->fonts.draw(bc.title, left.x + 22.0f, left.y + left.height - 118.0f, 30.0f,
                         Color{255, 255, 255, 255}, 3.0f);
    int total = static_cast<int>(bc.script->chapters.size());
    std::string prog = std::to_string(bc.viewedCount) + " / " + std::to_string(total) + " 已观看";
    engine()->fonts.draw(prog, left.x + 22.0f, left.y + left.height - 72.0f, 19.0f,
                         Color{170, 180, 205, 255}, 1.9f);
    float barW = left.width - 44.0f;
    float ratio = total > 0 ? static_cast<float>(bc.viewedCount) / total : 0.0f;
    DrawRectangleRounded({left.x + 22.0f, left.y + left.height - 44.0f, barW, 8.0f}, 4.0f, 8,
                         Color{255, 255, 255, 30});
    if (ratio > 0.0f)
        DrawRectangleRounded({left.x + 22.0f, left.y + left.height - 44.0f, barW * ratio, 8.0f},
                             4.0f, 8, kAccent);

    // 右侧：小章节列表（全部可进入）
    engine()->fonts.draw("章节列表", 540.0f, 106.0f, 24.0f,
                         Color{220, 226, 242, 255}, 2.4f);
    float rowH = 76.0f;
    float gap = 14.0f;
    float y0 = 150.0f;
    float rx = 540.0f;
    float rw = 660.0f;

    for (size_t i = 0; i < bc.script->chapters.size(); ++i)
    {
        Rectangle r{rx, y0 + i * (rowH + gap), rw, rowH};
        const auto& ch = bc.script->chapters[i];
        bool viewed = viewed_.count(bc.path + "|" + std::to_string(ch.id)) != 0;
        bool next = (nextIdx == static_cast<int>(i));
        bool locked = nextIdx >= 0 && static_cast<int>(i) > nextIdx;
        float a = locked ? 0.0f : renderer::easeInOut(subHover_[i]);

        Color fill{static_cast<unsigned char>(22 + 30 * a),
                   static_cast<unsigned char>(26 + 42 * a),
                   static_cast<unsigned char>(38 + 70 * a), 240};
        Color border{255, 255, 255, static_cast<unsigned char>(35 + 110 * a)};
        DrawRectangleRounded(r, corner(r), 12, fill);
        DrawRectangleRoundedLinesEx(r, corner(r), 12, 1.5f, border);

        // 缩略图：圆角矩形封面（无封面时显示占位色块）
        // 缩略图四边统一内缩 8px，圆角与行卡片同心
        Rectangle thumb{r.x + 8.0f, r.y + 8.0f, 104.0f, rowH - 16.0f};
        drawCardTexture(thumb, ch.picture, fill, renderer::kCornerRadius - 8.0f);

        // 名称与状态
        Color nameColor = viewed ? Color{240, 244, 255, 255}
                                 : (next ? Color{255, 214, 120, 255} : Color{140, 143, 152, 255});
        engine()->fonts.draw(ch.name, r.x + 126.0f, r.y + 12.0f, 24.0f, nameColor, 2.4f);
        std::string state = viewed ? "已观看"
                                   : (next ? "下一章" : "未开启");
        engine()->fonts.draw(state, r.x + 126.0f, r.y + 44.0f, 17.0f,
                             viewed ? Color{120, 190, 255, 255}
                                    : (next ? Color{255, 214, 120, 255} : Color{110, 114, 124, 255}),
                             1.7f);
        if (viewed || next)
        {
            engine()->fonts.draw("进入 →", r.x + r.width - 92.0f, r.y + 26.0f, 19.0f,
                                 Color{150, 180, 255,
                                       static_cast<unsigned char>(120 + 120 * a)}, 1.9f);
        }
        else
        {
            engine()->fonts.draw("未解锁", r.x + r.width - 92.0f, r.y + 26.0f, 19.0f,
                                 Color{100, 104, 114, 255}, 1.9f);
        }

        if (next) drawGlow(r, t);
    }
    (void)w;
    (void)h;
}

void ChapterSelectScene::drawGlow(Rectangle r, float timeSec) const
{
    (void)timeSec;
    // 金色轮廓：紧贴行的边缘，只有 2px 贴身微光，不产生悬浮阴影
    const float baseR = renderer::kCornerRadius;
    Rectangle outer{r.x - 2.0f, r.y - 2.0f, r.width + 4.0f, r.height + 4.0f};
    float rd = renderer::roundness(baseR + 2.0f, outer);
    DrawRectangleRoundedLinesEx(outer, rd, 16, 2.0f, Color{255, 205, 90, 110});
    DrawRectangleRoundedLinesEx(r, corner(r), 16, 3.0f, Color{255, 218, 112, 255});
}

void ChapterSelectScene::debugAuto(int frame)
{
    if (frame == 150) engine()->selftestShot("chapter_big");
    else if (frame == 170)
    {
        if (!big_.empty())
        {
            bigIndex_ = 0;
            subHover_.assign(big_[0].script->chapters.size(), 0.0f);
        }
    }
    else if (frame == 220) engine()->selftestShot("chapter_select");
    else if (frame == 250)
    {
        if (bigIndex_ >= 0 && !big_[bigIndex_].script->chapters.empty())
            startChapter(big_[bigIndex_], 0);
    }
}
