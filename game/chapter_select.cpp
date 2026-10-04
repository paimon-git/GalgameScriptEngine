#include "chapter_select.h"
#include "../core/canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <rlgl.h>

#include "../core/engine.h"
#include "../core/file_util.h"
#include "../core/gesture.h"
#include "../core/lang.h"
#include "../core/save.h"
#include "../renderer/renderer.h"
#include "game_scene.h"
#include "title_scene.h"

namespace
{
// ---- 布局常量（按设计分辨率 1280x720 排版）--------------------------------
constexpr float kListX   = 512.0f;    // 右侧章节列表：左缘
constexpr float kListW   = 690.0f;    // 列表宽度
constexpr float kListTop = 126.0f;    // 视口上缘
constexpr float kListBot = 686.0f;    // 视口下缘
// 行高与行距保持旧版列表的节奏（步距 90），只换卡片样式，避免"按钮突然变大"
constexpr float kRowH    = 78.0f;
constexpr float kRowGap  = 12.0f;
constexpr float kStep    = kRowH + kRowGap;
constexpr float kBarW    = 8.0f;
constexpr float kBarGap  = 18.0f;
constexpr float kBarX    = kListX + kListW + kBarGap;

// 剧情地图布局（世界坐标，缩放/平移另外算）：
//   上：主线一排（300x210）
//   下：支线独立区块（240x170，每行 4 张，四周留出底板边距）
constexpr float kCardW = 300.0f;        // 主线卡片
constexpr float kCardH = 210.0f;
constexpr float kSideCardW = 240.0f;    // 支线卡片（小一号，挂在上下车道）
constexpr float kSideCardH = 170.0f;
constexpr float kCardGap = 30.0f;
constexpr float kCardMargin = 40.0f;

// 剧情树：主线横着一条，支线从它上面/下面竖着一条条伸出去
constexpr float kBranchDrop = 64.0f;     // 主线卡片边缘到第一条支线卡片的距离
constexpr float kColGap = 22.0f;         // 同一条支线里上下两张卡片的间距
// 自由缩放的上下限
constexpr float kZoomMin = 0.35f;
constexpr float kZoomMax = 2.0f;
// 一级界面左上角的"复位视图"按钮（返回按钮在它左边）
constexpr float kResetBtnX = 156.0f;
constexpr float kResetBtnY = 24.0f;
constexpr float kResetBtnW = 132.0f;
constexpr float kResetBtnH = 44.0f;
// 旁边的缩放按钮（触控板用户没有滚轮也能缩放）
constexpr float kZoomBtnW = 52.0f;
constexpr float kZoomBtnOutX = kResetBtnX + kResetBtnW + 10.0f;
constexpr float kZoomBtnInX = kZoomBtnOutX + kZoomBtnW + 8.0f;

// 小章节界面左侧大图（整卡）矩形
Rectangle subPanelRect()
{
    return Rectangle{40.0f, 120.0f, 436.0f, 500.0f};
}

Rectangle lerpRect(Rectangle a, Rectangle b, float t)
{
    return Rectangle{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                     a.width + (b.width - a.width) * t,
                     a.height + (b.height - a.height) * t};
}

// 主题圆角（设置里可调）
float uiCorner() { return renderer::cornerRadius(); }

float cornerOf(Rectangle r) { return renderer::roundness(uiCorner(), r); }

// 统一调色板：面板 / 卡片 / 描边 / 文字都从主题色推导，避免拼色
const renderer::Palette& pal() { return renderer::palette(); }
Color withAlpha(Color c, unsigned char a) { c.a = a; return c; }

Color mixColor(Color a, Color b, float t)
{
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t),
    };
}

// 状态胶囊：文字 + 内边距，半高圆角（胶囊形）
void drawPill(const Font& f, Vector2 at, const std::string& s, Color fill, Color text,
              float fs, float padX)
{
    Vector2 m = MeasureTextEx(f, s.c_str(), fs, fs / 10.0f);
    Rectangle r{at.x, at.y, m.x + padX * 2.0f, m.y + 9.0f};
    DrawRectangleRounded(r, renderer::roundness(r.height * 0.5f, r), 20, fill);
    DrawTextEx(f, s.c_str(), {r.x + padX, r.y + 4.5f}, fs, fs / 10.0f, text);
}

// 序号徽章：圆角方块 + 居中两位数字
void drawIndexBadge(const Font& f, Rectangle r, int index, Color fill, Color text)
{
    DrawRectangleRounded(r, renderer::roundness(r.height * 0.32f, r), 12, fill);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d", index < 0 ? 0 : index);
    float fs = r.height * 0.55f;      // 跟着徽章大小走（主线小卡片 / 支线更小的卡片都能用）
    Vector2 m = MeasureTextEx(f, buf, fs, fs / 10.0f);
    DrawTextEx(f, buf, {r.x + (r.width - m.x) * 0.5f, r.y + (r.height - m.y) * 0.5f - 1.0f},
               fs, fs / 10.0f, text);
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
    {
        bg_ = renderer::loadSmoothTexture("assets/bg/title.png");
        // 背景高斯模糊：静态图 + 进程级缓存，只在首次进入时算一次（见 renderer::blurredImage）
        bgBlur_ = renderer::blurredImage("assets/bg/title.png", 18, 0.5f);
    }
    scanScripts();
    refreshViewed();
    bigHover_.assign(big_.size(), 0.0f);
}

ChapterSelectScene::~ChapterSelectScene()
{
    if (bg_.id) UnloadTexture(bg_);
    // bgBlur_ 由 renderer 的缓存持有，不能在这里释放（否则缓存里会留下野 id）
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
            bc.subtitle = script->subtitle;
            bc.branch = script->branch.empty() ? "main" : script->branch;
            bc.order = script->order;
            bc.series = script->series.empty() ? bc.title : script->series;
            bc.script = script;
            if (!script->chapters.empty())
                bc.cover = script->chapters.front().picture;
            if (!script->requiresScript.empty())
            {
                bc.requiresPath = "assets/scripts/" + script->requiresScript + ".gal";
                bc.requiresChapter = script->requiresChapter;
            }
            for (const auto& r : script->requiresList)
                bc.reqs.emplace_back("assets/scripts/" + r.first + ".gal", r.second);
            big_.push_back(std::move(bc));
        }
        catch (const ScriptError& e)
        {
            printf("[chapter] skip %s: %s\n", p.c_str(), e.what());
        }
    }
    // 剧情树排序：主线在前、支线在后，同一条线上按 order（没写就按文件名）排
    std::sort(big_.begin(), big_.end(), [](const BigChapter& a, const BigChapter& b) {
        if (a.series != b.series) return a.series < b.series;
        bool am = a.branch != "side", bm = b.branch != "side";
        if (am != bm) return am;
        if (a.order != b.order) return a.order < b.order;
        return a.path < b.path;
    });
    buildBigLayout();
    // 一进来就先摆好全览视图（不要动画，直接到位）
    resetView();
    zoom_ = zoomTarget_;
    pan_ = panTarget_;
}

// 剧情树布局（世界坐标；缩放/平移是另一层变换）：
//   主线：横着一条，从左到右按 order
//   支线：每卷的支线竖着摞成一条，挂在那一卷的上面或下面（哪边空就往哪边放），
//         一条竖线串下来，卡片依次排在线上。
void ChapterSelectScene::buildBigLayout()
{
    const float gap = kCardGap, margin = kCardMargin;

    bigRect_.assign(big_.size(), Rectangle{});
    float cursor = margin;

    // ---- 主线：一行 ----
    std::vector<int> mainIdx;
    for (size_t i = 0; i < big_.size(); ++i)
    {
        if (!isMain(big_[i])) continue;
        bigRect_[i] = Rectangle{cursor, 0.0f, kCardW, kCardH};
        mainIdx.push_back(static_cast<int>(i));
        cursor += kCardW + gap;
    }
    const float mainRight = mainIdx.empty() ? margin : cursor - gap;

    // ---- 支线：挂在各自那一卷上，一条竖线，卡片依次排下去 ----
    // 每卷的支线在上下两侧对半分（4 条就上面 2 条、下面 2 条），整棵树更矮也更对称
    float top = 0.0f, bottom = kCardH;
    for (int mi : mainIdx)
    {
        // 这一卷下面挂了几条支线
        std::vector<int> kids;
        for (size_t i = 0; i < big_.size(); ++i)
            if (!isMain(big_[i]) && prereqIndex(big_[i]) == mi) kids.push_back(static_cast<int>(i));
        if (kids.empty()) continue;

        const float colX = bigRect_[mi].x + (kCardW - kSideCardW) * 0.5f;
        const size_t upCount = (kids.size() + 1) / 2;
        for (size_t k = 0; k < kids.size(); ++k)
        {
            const bool upward = k < upCount;
            // 上侧的序号从近到远；下侧同理，所以上侧要倒过来算
            const float step = static_cast<float>(k < upCount ? k : (k - upCount));
            float y = 0.0f;
            if (upward)
            {
                y = -(kBranchDrop + (step + 1) * kSideCardH + step * kColGap);
                top = std::min(top, y);
            }
            else
            {
                y = kCardH + kBranchDrop + step * (kSideCardH + kColGap);
                bottom = std::max(bottom, y + kSideCardH);
            }
            bigRect_[kids[k]] = Rectangle{colX, y, kSideCardW, kSideCardH};
        }
    }

    bigMainBox_ = Rectangle{margin - 26.0f, top - 46.0f,
                            std::max(200.0f, mainRight - margin) + 52.0f,
                            (bottom - top) + 92.0f};
    bigSideBox_ = bigMainBox_;      // 现在只有一块整体，没有独立的支线区块
}

void ChapterSelectScene::refreshViewed()
{
    viewed_.clear();
    for (auto& bc : big_)
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
        bc.viewedCount = count;
    }

    // 解锁判定：所有 requires 都满足才解锁（没写 = 直接解锁）。
    // 需要给玩家一个能看懂的提示，所以这里顺便查一下前置脚本的名字和章节名。
    for (auto& bc : big_)
    {
        bc.locked = false;
        bc.requiresText.clear();
        bc.reqLines.clear();
        if (bc.reqs.empty()) continue;

        for (const auto& req : bc.reqs)
        {
            const BigChapter* pre = nullptr;
            for (const auto& cand : big_)
                if (cand.path == req.first) { pre = &cand; break; }
            // 章节号 0 → 前置脚本全部章节都要看完
            int need = req.second;
            if (need <= 0 && pre) need = static_cast<int>(pre->script->chapters.size());
            if (need <= 0) continue;
            if (viewed_.count(req.first + "|" + std::to_string(need))) continue;   // 这条满足了

            bc.locked = true;
            std::string preName = pre ? pre->title : req.first;
            std::string chName;
            if (pre)
                for (const auto& ch : pre->script->chapters)
                    if (ch.id == need) { chName = ch.name; break; }
            if (chName.empty()) chName = std::to_string(need);
            std::string hint = lang::trf("chapter.require_hint", {preName, chName});
            bc.requiresText = bc.requiresText.empty() ? hint : (bc.requiresText + "  +  " + hint);
            bc.reqLines.push_back(hint);
        }
    }
}

Texture2D ChapterSelectScene::coverTexture(const std::string& path)
{
    auto it = coverTex_.find(path);
    if (it != coverTex_.end()) return it->second;
    Texture2D tex{};
    if (!path.empty() && FileExists(path.c_str()))
        tex = renderer::loadSmoothTexture(path);   // 封面会被缩放/放大，必须设过滤
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

// 一级卡片的矩形（update / drawBigList / 画树枝三处必须用同一套算法）
Rectangle ChapterSelectScene::bigCardRectFor(int index, float dx) const
{
    if (index < 0 || index >= static_cast<int>(bigRect_.size())) return Rectangle{};
    Rectangle r = worldRect(bigRect_[index]);
    r.x += dx;
    return r;
}

// 世界坐标 → 屏幕坐标
Rectangle ChapterSelectScene::worldRect(Rectangle r) const
{
    return Rectangle{r.x * zoom_ + pan_.x, r.y * zoom_ + pan_.y,
                     r.width * zoom_, r.height * zoom_};
}

Vector2 ChapterSelectScene::screenToWorld(Vector2 p) const
{
    return Vector2{(p.x - pan_.x) / zoom_, (p.y - pan_.y) / zoom_};
}

void ChapterSelectScene::resetView()
{
    const float vw = static_cast<float>(canvas::width());
    const float vh = static_cast<float>(canvas::height());
    // 复位 = 把整棵树（横着的主线 + 上下的支线）都装进屏幕，先看全景再放大看细节
    const float z = std::min((vw - 120.0f) / std::max(200.0f, bigMainBox_.width),
                             (vh - 110.0f) / std::max(200.0f, bigMainBox_.height));
    zoomTarget_ = std::clamp(z, 0.5f, 1.0f);
    const float cx = bigMainBox_.x + bigMainBox_.width * 0.5f;
    const float cy = bigMainBox_.y + bigMainBox_.height * 0.5f;
    panTarget_ = Vector2{vw * 0.5f - cx * zoomTarget_, vh * 0.53f - cy * zoomTarget_};
}

int ChapterSelectScene::prereqIndex(const BigChapter& bc) const
{
    if (bc.requiresPath.empty()) return -1;
    for (size_t i = 0; i < big_.size(); ++i)
        if (big_[i].path == bc.requiresPath) return static_cast<int>(i);
    return -1;
}

// 剧情树连线：主线横着一条（卡片之间用短线串起来），
// 每条支线是从主线卡片上竖着垂下去的一条线，卡片依次挂在线上。
void ChapterSelectScene::drawRouteTree(float dx)
{
    if (big_.empty()) return;
    const Color acc = renderer::accent();
    const Color branchCol = pal().highlight;
    const float lw = std::max(1.2f, 3.0f * zoom_);

    // ---- 主线 ------------
    Vector2 prev{};
    bool havePrev = false;
    for (size_t i = 0; i < big_.size(); ++i)
    {
        if (!isMain(big_[i])) continue;
        Rectangle r = bigCardRectFor(static_cast<int>(i), dx);
        if (r.x + r.width < -60.0f || r.x > canvas::width() + 60.0f) continue;
        Vector2 c{r.x + r.width * 0.5f, r.y + r.height * 0.5f};
        if (havePrev) DrawLineEx(prev, c, lw, withAlpha(acc, 140));
        DrawCircleV(c, std::max(2.0f, 5.0f * zoom_), withAlpha(acc, 225));
        prev = c;
        havePrev = true;
    }

    // ---- 支线：每一卷往下/往上一条竖线，卡片串在线上 ------------
    for (size_t mi = 0; mi < big_.size(); ++mi)
    {
        if (!isMain(big_[mi])) continue;
        Rectangle pr = bigCardRectFor(static_cast<int>(mi), dx);
        if (pr.width <= 0.0f) continue;

        // 收集这一卷的支线，按"离主线由近到远"排序
        std::vector<int> kids;
        for (size_t i = 0; i < big_.size(); ++i)
            if (!isMain(big_[i]) && prereqIndex(big_[i]) == static_cast<int>(mi))
                kids.push_back(static_cast<int>(i));
        if (kids.empty()) continue;
        const bool upward = bigCardRectFor(kids[0], dx).y + bigCardRectFor(kids[0], dx).height * 0.5f <
                            pr.y + pr.height * 0.5f;
        std::sort(kids.begin(), kids.end(), [&](int a, int b) {
            float ya = bigCardRectFor(a, dx).y, yb = bigCardRectFor(b, dx).y;
            return upward ? (ya > yb) : (ya < yb);
        });

        const float colX = pr.x + pr.width * 0.5f;
        float fromY = upward ? pr.y : (pr.y + pr.height);
        for (int k : kids)
        {
            Rectangle r = bigCardRectFor(k, dx);
            const float toY = upward ? (r.y + r.height) : r.y;
            Color line = big_[k].locked ? withAlpha(pal().line, 115) : withAlpha(branchCol, 180);
            DrawLineEx({colX, fromY}, {colX, toY}, lw * 0.7f, line);
            DrawCircleV({colX, toY}, std::max(2.0f, 4.0f * zoom_), line);
            fromY = upward ? r.y : (r.y + r.height);
        }
    }
}

// 未解锁的卡片：条件写在"进入这张卡的线"上，一条条件一行。
//   主线卷：入度线是它和上一卷之间那段横线 → 文字压在那段线正上方
//   支线：入度线是从它那一卷垂下来的竖线 → 文字贴在这条竖线旁边、卡片外的空隙里
// 文字画在卡片之后（盖在上面），底下垫一块半透明底板保证读得清。
void ChapterSelectScene::drawRequirementLabels(float dx)
{
    if (big_.empty()) return;
    const float fs = std::clamp(15.0f * zoom_, 11.0f, 19.0f);
    const float lh = fs * 1.3f;
    const Color plate = Color{8, 10, 20, 198};
    const Color fg = withAlpha(pal().textMuted, 240);
    const float vw = static_cast<float>(canvas::width());
    const float vh = static_cast<float>(canvas::height());
    std::vector<Rectangle> placed;   // 已经放下的标签，避免主线上的两块叠在一起

    for (size_t i = 0; i < big_.size(); ++i)
    {
        const BigChapter& bc = big_[i];
        if (!bc.locked || bc.reqLines.empty()) continue;
        Rectangle r = bigCardRectFor(static_cast<int>(i), dx);
        if (r.width <= 0.0f) continue;

        // 文本块宽度
        float tw = 0.0f;
        for (const auto& s : bc.reqLines) tw = std::max(tw, engine()->fonts.measure(s, fs).x);
        const float th = lh * static_cast<float>(bc.reqLines.size());
        float x = 0.0f, y = 0.0f;

        if (isMain(bc))
        {
            // 找上一卷（入度线的另一端）
            Rectangle pr{};
            bool found = false;
            for (int k = static_cast<int>(i) - 1; k >= 0; --k)
                if (isMain(big_[k])) { pr = bigCardRectFor(k, dx); found = pr.width > 0.0f; break; }
            if (!found) continue;                       // 第一卷没有入度线
            const float lineY = r.y + r.height * 0.5f;  // 主线脊线所在高度
            x = (pr.x + pr.width + r.x) * 0.5f - tw * 0.5f;
            y = lineY - th - 7.0f;
            // 和上一卷的标签撞了就往上一格，别叠在一起
            Rectangle box{x - 8.0f, y - 4.0f, tw + 16.0f, th + 8.0f};
            for (int guard = 0; guard < 6; ++guard)
            {
                bool hit = false;
                for (const auto& q : placed)
                    if (CheckCollisionRecs(box, q)) { hit = true; break; }
                if (!hit) break;
                y -= th + 8.0f;
                box.y = y - 4.0f;
            }
            placed.push_back(box);
        }
        else
        {
            int pi = prereqIndex(bc);
            if (pi < 0) continue;
            Rectangle pr = bigCardRectFor(pi, dx);
            if (pr.width <= 0.0f) continue;
            const bool upward = (r.y + r.height * 0.5f) < (pr.y + pr.height * 0.5f);
            const float gapTop = upward ? (r.y + r.height) : (pr.y + pr.height);
            const float gapBot = upward ? pr.y : r.y;
            const float colX = r.x + r.width * 0.5f;    // 竖线所在位置
            x = colX + 10.0f;
            y = (gapTop + gapBot) * 0.5f - th * 0.5f;
        }

        // 视口外不画
        if (x + tw < -40.0f || x > vw + 40.0f || y + th < -40.0f || y > vh + 40.0f) continue;

        Rectangle bg{x - 8.0f, y - 4.0f, tw + 16.0f, th + 8.0f};
        DrawRectangleRounded(bg, renderer::roundness(6.0f, bg), 10, plate);
        if (std::getenv("QLWT_DEBUG_LABELS"))
            printf("[labels] %s → x=%.0f y=%.0f w=%.0f lines=%zu\n",
                   bc.title.c_str(), x, y, tw, bc.reqLines.size());
        for (size_t k = 0; k < bc.reqLines.size(); ++k)
            engine()->fonts.draw(bc.reqLines[k], x, y + lh * static_cast<float>(k), fs, fg,
                                 fs / 10.0f);
    }
}

// 右上角图例：蓝线 = 主线（横着一条），金线 = 支线（每条竖着一条）
void ChapterSelectScene::drawTreeLegend()
{
    const float w = static_cast<float>(canvas::width());
    const Color acc = renderer::accent();
    const float y = 46.0f;
    float x = w - 322.0f;

    // 主线：一小段横线
    DrawLineEx({x, y + 9.0f}, {x + 36.0f, y + 9.0f}, 3.0f, withAlpha(acc, 190));
    DrawCircleV({x + 18.0f, y + 9.0f}, 4.0f, withAlpha(acc, 225));
    engine()->fonts.draw(lang::tr("chapter.branch_main"), x + 44.0f, y, 17.0f,
                         withAlpha(pal().textDim, 225), 1.7f);

    // 支线：一小段竖线
    x = w - 190.0f;
    DrawLineEx({x + 8.0f, y - 2.0f}, {x + 8.0f, y + 20.0f}, 2.0f,
               withAlpha(pal().highlight, 190));
    DrawCircleV({x + 8.0f, y + 20.0f}, 4.0f, withAlpha(pal().highlight, 205));
    engine()->fonts.draw(lang::tr("chapter.branch_side"), x + 26.0f, y, 17.0f,
                         withAlpha(pal().textDim, 225), 1.7f);
}

Rectangle ChapterSelectScene::listViewport() const
{
    return Rectangle{kListX, kListTop, kListW, kListBot - kListTop};
}

float ChapterSelectScene::listMaxScroll() const
{
    if (bigIndex_ < 0 || bigIndex_ >= static_cast<int>(big_.size())) return 0.0f;
    const size_t n = big_[bigIndex_].script->chapters.size();
    const float contentH = n > 0 ? static_cast<float>(n) * kStep - kRowGap : 0.0f;
    return std::max(0.0f, contentH - (kListBot - kListTop));
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

// 进入小章节列表 / 返回大章节列表：只改目标值，实际绘制由 viewT_ 插值
void ChapterSelectScene::enterSub(int index)
{
    if (index < 0 || index >= static_cast<int>(big_.size())) return;
    bigIndex_ = index;
    subHover_.assign(big_[bigIndex_].script->chapters.size(), 0.0f);
    subScroll_ = subScrollTarget_ = 0.0f;
    scrollInit_ = true;
    listDragging_ = listDragMoved_ = false;
    listPressRow_ = -1;
    barDragging_ = false;
    // hero 转场：被点中的卡片封面 -> 左侧大图封面
    heroIndex_ = index;
    heroFrom_ = bigCardRectFor(index);
    heroTo_ = subPanelRect();
    heroKFrom_ = 0.0f;   // 卡片形态
    heroKTo_ = 1.0f;     // 大图形态
    viewTarget_ = 1.0f;
}

void ChapterSelectScene::backToBig()
{
    // 反向 hero：左侧封面 -> 原大卡片位置
    heroFrom_ = subPanelRect();
    heroTo_ = bigCardRectFor(bigIndex_);
    heroKFrom_ = 1.0f;   // 从大图形态出发
    heroKTo_ = 0.0f;     // 收回卡片形态
    viewTarget_ = 0.0f;
}

void ChapterSelectScene::update(float dt)
{
    updateEntryFade(dt);

    // ---- 大章节 <-> 小章节 切换动画 ----
    viewT_ = renderer::approach(viewT_, viewTarget_, 9.0f, dt);
    bool viewAnimating = std::fabs(viewT_ - viewTarget_) > 0.004f;
    if (!viewAnimating)
    {
        viewT_ = viewTarget_;
        // 退场动画播完才真正回到大列表（否则小列表没数据可画）
        if (viewTarget_ == 0.0f && bigIndex_ >= 0)
        {
            bigIndex_ = -1;
            // 不要动 pan：hero 返航落点是用当时的视图变换算的，
            // 这里改视图会让整张图在动画结束的那一帧跳一下。
        }
    }

    Vector2 mouse = GetMousePosition();
    bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool back = IsKeyPressed(KEY_ESCAPE);
    float wheel = GetMouseWheelMove();

    // 自检：模拟一次真实的"按下 -> 松开"点击章节行（走完全相同的输入路径，
    // 这样这类点击 bug 才能被自检覆盖到）
    if (debugClickRow_ >= 0 && bigIndex_ >= 0)
    {
        Rectangle v = listViewport();
        float rowY = v.y + debugClickRow_ * kStep - subScroll_ + kRowH * 0.5f;
        mouse = Vector2{kListX + 200.0f, rowY};
        if (debugClickPhase_ == 0)
        {
            pressed = true; held = true; released = false;
            debugClickPhase_ = 1;
        }
        else
        {
            pressed = false; held = false; released = true;
            debugClickRow_ = -1;
        }
    }

    hoverBig_ = -1;
    hoverSub_ = -1;

    if (viewAnimating) return;   // 动画期间不响应点击，避免误触

    if (bigIndex_ < 0)
    {
        // ---------------- 一级：剧情地图（滚轮缩放 / 拖动平移）----------------
        // 自检：在指定屏幕坐标模拟一次真实点击（走同一条命中测试路径）
        if (debugBigClickPhase_ > 0)
        {
            mouse = debugBigClick_;
            if (debugBigClickPhase_ == 1)
            {
                pressed = true; held = true; released = false;
                debugBigClickPhase_ = 2;
            }
            else
            {
                pressed = false; held = false; released = true;
                debugBigClickPhase_ = 0;
            }
        }
        // 缩放以鼠标位置为锚点：先记下鼠标下面那个世界点，换完缩放再把它挪回鼠标处。
        // 这里用"目标"的 pan/zoom 算，而不是正在缓动的 pan_/zoom_：
        // 否则下面"双指拖动 + 捏合"同时发生时，刚叠加进去的平移会被这一行覆盖掉。
        auto zoomAt = [&](float factor) {
            Vector2 before{(mouse.x - panTarget_.x) / zoomTarget_,
                           (mouse.y - panTarget_.y) / zoomTarget_};
            zoomTarget_ = std::clamp(zoomTarget_ * factor, kZoomMin, kZoomMax);
            panTarget_ = Vector2{mouse.x - before.x * zoomTarget_,
                                 mouse.y - before.y * zoomTarget_};
        };
        // ---- 触摸板手势（双指拖动 / 捏合）----
        // 引擎每帧读一次内核的多点触控，这里直接拿增量：
        //   双指移动 → 平移；两指距离变化 → 缩放（以鼠标位置为锚点）
        const gesture::Frame& g = gesture::frame();
        if (g.active)
        {
            panTarget_.x += g.panX;
            panTarget_.y += g.panY;
            // 手势直接跟手：pan_ 也一起走，别让内容落后手指一截（一阶缓动会有 ~60ms 拖尾）
            pan_.x += g.panX;
            pan_.y += g.panY;
            // 阈值要足够小：g.zoom 是"这一帧"的倍率、每帧都会清零，
            // 慢慢捏的时候每帧只有 1.000x，卡 0.001 会把整段慢速捏合全丢掉。
            if (std::fabs(g.zoom - 1.0f) > 1e-6f) zoomAt(g.zoom);
        }
        const bool touchSwallowWheel = g.touching;   // 手指还在板上就不要再吃滚动事件了

        // ---- 触控板 / 鼠标滚轮 ----------------
        // 触控板双指滑动产生的是一串连续的小数（0.05~0.4 这种），鼠标滚轮一格是接近 1 的整数；
        // 所以：小数 = 双指平移（含横向），整数格 / Ctrl(⌘) / Shift = 缩放。
        // （能读到触摸板设备时走上面的原生手势，这里只是兜底）
        if (!mapDragging_ && !touchSwallowWheel)
        {
            Vector2 wv = GetMouseWheelMoveV();
            const bool wheelMod = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                                  IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ||
                                  IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            const float mag = std::max(std::fabs(wv.x), std::fabs(wv.y));
            // 判定是触控板连续滚动还是鼠标滚轮一格：
            //   * 带横向分量的一律当触控板（鼠标滚轮很少有横向）
            //   * 纵向看幅度：触控板每次只报零点几，鼠标滚轮一格在 Wayland 下
            //     经过 GLFW 的 -value/10 换算常常是 1.0~1.5，比触控板大得多。
            // 之前用"是不是整数"来区分，结果 1.5 这种滚轮值被误判成触控板平移。
            const bool trackpad = mag > 0.0f &&
                                  (std::fabs(wv.x) > 0.001f || mag < 0.8f);
            if (trackpad && !wheelMod)
            {
                // 方向要和"按住拖动 / 原生双指手势"一致（内容跟着手走）：
                //   纵向：GLFW 向上滚为正，此时内容应该往下走 → +y
                //   横向：GLFW 向右滚为正，此时内容应该往左走 → -x
                // （原来的纵向是 -y，和列表滚动、原生手势、鼠标拖动都相反。）
                const Vector2 d{-wv.x * 300.0f, wv.y * 300.0f};
                panTarget_.x += d.x;
                panTarget_.y += d.y;
                pan_.x += d.x;
                pan_.y += d.y;
            }
            else if (mag > 0.0f)
            {
                zoomAt(1.0f + wv.y * 0.12f);
            }
        }
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) zoomAt(1.15f);
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) zoomAt(1.0f / 1.15f);
        if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_HOME)) resetView();

        // 拖动平移：左键按住拖动（超过 6px 才算拖动，否则还是点击）；
        // 右键/中键拖动同样平移 —— 触控板"双指按下拖动"在系统里就是右键/中键
        if (pressed)
        {
            mapDragging_ = true;
            mapDragMoved_ = false;
            mapDragStart_ = mouse;
            mapDragPanStart_ = panTarget_;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
        {
            mapDragging_ = true;
            mapDragMoved_ = true;      // 右键拖动永远不会被当成点击
            mapDragStart_ = mouse;
            mapDragPanStart_ = panTarget_;
        }
        if (mapDragging_ && held)
        {
            Vector2 d{mouse.x - mapDragStart_.x, mouse.y - mapDragStart_.y};
            if (!mapDragMoved_ && std::fabs(d.x) + std::fabs(d.y) > 6.0f) mapDragMoved_ = true;
            if (mapDragMoved_)
            {
                panTarget_ = Vector2{mapDragPanStart_.x + d.x, mapDragPanStart_.y + d.y};
                pan_ = panTarget_;   // 拖拽直接跟手
            }
        }
        if (mapDragging_ &&
            (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)))
        {
            Vector2 d{mouse.x - mapDragStart_.x, mouse.y - mapDragStart_.y};
            if (std::fabs(d.x) + std::fabs(d.y) > 6.0f) mapDragMoved_ = true;
            panTarget_ = Vector2{mapDragPanStart_.x + d.x, mapDragPanStart_.y + d.y};
            pan_ = panTarget_;   // 拖拽直接跟手
        }
        if (released || IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) ||
            IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE))
            mapDragging_ = false;

        zoom_ = renderer::approach(zoom_, zoomTarget_, 14.0f, dt);
        pan_.x = renderer::approach(pan_.x, panTarget_.x, 16.0f, dt);
        pan_.y = renderer::approach(pan_.y, panTarget_.y, 16.0f, dt);

        // 复位按钮（只在真正的一级界面里出现）
        Rectangle resetBtn{kResetBtnX, kResetBtnY, kResetBtnW, kResetBtnH};
        resetHover_ = renderer::approach(
            resetHover_, CheckCollisionPointRec(mouse, resetBtn) ? 1.0f : 0.0f, 14.0f, dt);
        Rectangle zoomOutBtn{kZoomBtnOutX, kResetBtnY, kZoomBtnW, kResetBtnH};
        Rectangle zoomInBtn{kZoomBtnInX, kResetBtnY, kZoomBtnW, kResetBtnH};
        zoomHover_[0] = renderer::approach(
            zoomHover_[0], CheckCollisionPointRec(mouse, zoomOutBtn) ? 1.0f : 0.0f, 14.0f, dt);
        zoomHover_[1] = renderer::approach(
            zoomHover_[1], CheckCollisionPointRec(mouse, zoomInBtn) ? 1.0f : 0.0f, 14.0f, dt);
        if (released && !mapDragMoved_)
        {
            if (CheckCollisionPointRec(mouse, zoomOutBtn)) { zoomAt(1.0f / 1.15f); return; }
            if (CheckCollisionPointRec(mouse, zoomInBtn)) { zoomAt(1.15f); return; }
        }
        if (released && !mapDragMoved_ && CheckCollisionPointRec(mouse, resetBtn))
        {
            resetView();
            return;
        }

        for (size_t i = 0; i < big_.size(); ++i)
        {
            Rectangle r = bigCardRectFor(static_cast<int>(i));
            if (r.width <= 0.0f) continue;
            if (!mapDragMoved_ && CheckCollisionPointRec(mouse, r)) hoverBig_ = static_cast<int>(i);
            bigHover_[i] = renderer::approach(
                bigHover_[i], hoverBig_ == static_cast<int>(i) ? 1.0f : 0.0f, 14.0f, dt);
        }

        // 自检钩子：走与真实点击完全相同的位置（update 内部、动画推进之后），
        // 这样才能复现"点击当帧"的画面
        if (debugEnter_ >= 0)
        {
            int idx = debugEnter_;
            debugEnter_ = -1;
            enterSub(idx);
            return;
        }

        // 点击改成"松开时生效"：这样按住拖动平移不会顺手把卡片点开
        if (released && !mapDragMoved_ && hoverBig_ >= 0 && !big_[hoverBig_].locked)
        {
            if (big_[hoverBig_].script->chapters.empty())
            {
                startChapter(big_[hoverBig_], -1);
                return;
            }
            enterSub(hoverBig_);
            return;
        }
        if (back)
            engine()->switchScene(std::make_shared<TitleScene>(
                engine(), defaultScript_, titleScript_, parseError_));
    }
    else
    {
        // ---------------- 二级：小章节列表（可滚动）----------------
        const BigChapter& bc = big_[bigIndex_];
        const int n = static_cast<int>(bc.script->chapters.size());
        if (static_cast<int>(subHover_.size()) != n) subHover_.assign(n, 0.0f);

        Rectangle view = listViewport();
        const float contentH = n > 0 ? n * kStep - kRowGap : 0.0f;
        const float maxScroll = std::max(0.0f, contentH - view.height);

        // 首次进入：把“下一章”滚进视野中央，避免玩家看到一屏已读章节
        if (scrollInit_)
        {
            int nxt = nextChapterIndex(bc);
            int focus = nxt >= 0 ? nxt : 0;
            subScrollTarget_ = std::clamp(
                focus * kStep + kRowH * 0.5f - view.height * 0.5f, 0.0f, maxScroll);
            subScroll_ = subScrollTarget_;
            scrollInit_ = false;
        }

        Rectangle track{kBarX, view.y, kBarW, view.height};
        const bool barVisible = maxScroll > 0.5f;
        const bool overBar = barVisible &&
            CheckCollisionPointRec(mouse, {track.x - 8.0f, track.y,
                                           track.width + 16.0f, track.height});
        const bool inView = CheckCollisionPointRec(mouse, view);
        barHover_ = renderer::approach(barHover_, overBar ? 1.0f : 0.0f, 16.0f, dt);

        // 触摸板双指滑动 / 滚轮都能滚这个列表
        const gesture::Frame& g2 = gesture::frame();
        if (barVisible && g2.active && std::fabs(g2.panY) > 0.01f)
            subScrollTarget_ -= g2.panY * 0.55f;      // 设计像素 → 列表滚动量
        if (barVisible && !g2.touching && (inView || overBar) && wheel != 0.0f)
            subScrollTarget_ -= wheel * 96.0f;
        // 键盘
        if (IsKeyPressed(KEY_DOWN))     subScrollTarget_ += kStep;
        if (IsKeyPressed(KEY_UP))       subScrollTarget_ -= kStep;
        if (IsKeyPressed(KEY_PAGE_DOWN)) subScrollTarget_ += view.height - kRowH;
        if (IsKeyPressed(KEY_PAGE_UP))   subScrollTarget_ -= view.height - kRowH;
        if (IsKeyPressed(KEY_HOME))     subScrollTarget_ = 0.0f;
        if (IsKeyPressed(KEY_END))      subScrollTarget_ = maxScroll;
        subScrollTarget_ = std::clamp(subScrollTarget_, 0.0f, maxScroll);

        // 滚动条拖拽
        const float thumbH = barVisible
            ? std::max(52.0f, view.height * (view.height / contentH)) : 0.0f;
        const float thumbY = view.y + (maxScroll > 0.0f
            ? (subScroll_ / maxScroll) * (view.height - thumbH) : 0.0f);
        const Rectangle thumb{track.x, thumbY, track.width, thumbH};
        const bool onThumb = barVisible &&
            CheckCollisionPointRec(mouse, {thumb.x - 8.0f, thumb.y,
                                           thumb.width + 16.0f, thumb.height});
        if (pressed && onThumb)
        {
            barDragging_ = true;
            barDragOffset_ = mouse.y - thumb.y;
            listDragging_ = false;
        }
        if (barDragging_)
        {
            if (!held) barDragging_ = false;
            else if (view.height > thumbH)
            {
                float t = (mouse.y - barDragOffset_ - view.y) / (view.height - thumbH);
                subScrollTarget_ = std::clamp(t, 0.0f, 1.0f) * maxScroll;
                subScroll_ = subScrollTarget_;   // 拖拽跟手，不做缓动
            }
        }

        // 悬停：只有完整落在视口内的行才响应
        for (int i = 0; i < n; ++i)
        {
            Rectangle r{kListX, view.y + i * kStep - subScroll_, kListW, kRowH};
            bool visible = r.y >= view.y - 0.5f && r.y + r.height <= view.y + view.height + 0.5f;
            if (visible && CheckCollisionPointRec(mouse, r)) hoverSub_ = i;
            subHover_[i] = renderer::approach(
                subHover_[i], hoverSub_ == i ? 1.0f : 0.0f, 14.0f, dt);
        }

        // 列表拖拽 / 点击
        if (!barDragging_)
        {
            if (pressed && inView)
            {
                listDragging_ = true;
                listDragMoved_ = false;
                listDragStartY_ = mouse.y;
                listDragStartScroll_ = subScrollTarget_;
                listPressRow_ = hoverSub_;
            }
            // 只在"仍按住"时更新拖拽。这里绝不能因为已松开就清 listDragging_，
            // 否则下面的松手判定拿不到这次点击 —— 表现就是"点章节没反应"。
            if (listDragging_ && held)
            {
                float dy = mouse.y - listDragStartY_;
                if (std::fabs(dy) > 6.0f) listDragMoved_ = true;
                if (listDragMoved_)
                {
                    subScrollTarget_ = std::clamp(listDragStartScroll_ - dy, 0.0f, maxScroll);
                    subScroll_ = subScrollTarget_;
                    hoverSub_ = -1;
                }
            }
        }

        if (released)
        {
            if (listDragging_ && !listDragMoved_ && listPressRow_ >= 0 &&
                listPressRow_ < n)
            {
                int idx = listPressRow_;
                const auto& ch = bc.script->chapters[idx];
                int nxt = nextChapterIndex(bc);
                bool viewed = viewed_.count(bc.path + "|" + std::to_string(ch.id)) != 0;
                if (nxt < 0 || idx <= nxt || viewed)
                {
                    listDragging_ = false;
                    listPressRow_ = -1;
                    startChapter(bc, idx);
                    return;
                }
            }
            listDragging_ = false;
            listPressRow_ = -1;
        }

        subScroll_ = renderer::approach(subScroll_, subScrollTarget_, 22.0f, dt);

        if (back)
            backToBig();
    }

    Rectangle backBtn{24.0f, 24.0f, 120.0f, 44.0f};
    if (pressed && CheckCollisionPointRec(mouse, backBtn))
    {
        if (bigIndex_ >= 0)
            backToBig();
        else
            engine()->switchScene(std::make_shared<TitleScene>(
                engine(), defaultScript_, titleScript_, parseError_));
    }
}

void ChapterSelectScene::draw()
{
    float w = static_cast<float>(canvas::width());
    float h = static_cast<float>(canvas::height());
    const Color acc = renderer::accent();

    Texture2D bgTex = bgBlur_.id ? bgBlur_ : bg_;
    if (bgTex.id)
    {
        float scale = w / static_cast<float>(bgTex.width);
        float bh = static_cast<float>(bgTex.height) * scale;
        DrawTexturePro(bgTex,
                       {0, 0, static_cast<float>(bgTex.width), static_cast<float>(bgTex.height)},
                       {0, (h - bh) * 0.5f, w, bh},
                       {0, 0}, 0.0f, WHITE);
    }
    else
    {
        ClearBackground(pal().surface);
    }
    // 压暗遮罩：模糊后的背景亮度偏高，压一层让面板/文字保持对比（数值越小背景越亮）
    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                  withAlpha(pal().surfaceSunken, 110));

    // 标题 + 标题下方的短强调线。
    // 注意：这里不能用横跨整宽的强调线——右侧"章节列表 / 共 N 章"的小标题
    // 底部正好落在那一行上，会被线压住。短下划线只铺在标题宽度内，和任何文字都不重叠。
    const float titleX = 170.0f, titleY = 38.0f, titleSize = 42.0f;
    engine()->fonts.draw(lang::tr("chapter.title"), titleX, titleY, titleSize,
                         pal().text, 4.2f, true);
    float titleW = engine()->fonts.measure("章节选择", titleSize).x + 6.0f;
    DrawRectangleRounded({titleX, titleY + titleSize + 16.0f, titleW, 4.0f}, 2.0f, 8, acc);

    // 两套布局的切换：被点中的封面作为"共享元素"飞过去变成左侧大图，
    // 其余部分在中间一次快速暗场里完成换场（飞行体始终浮在暗场之上）。
    if (bigIndex_ < 0)
    {
        drawBigList();
    }
    else
    {
        float t = viewT_;
        if (t >= 0.998f)
        {
            // 已经完全进入小章节
            drawSubList(big_[bigIndex_]);
        }
        else if (t <= 0.002f)
        {
            // 点击后的第一帧：viewTarget_ 刚变成 1，但 viewT_ 还没推进，
            // 此时画面仍应是大列表（否则会闪一帧完整的小章节界面 = 抽搐）
            drawBigList();
        }
        else
        {
            // 大列表向左滑出（被点中的那张整个交给飞行体），章节列表从右侧滑入；
            // 两者同步移动，不存在"交换瞬间"，所以不需要任何遮罩。
            float e = renderer::easeInOut(t);

            float dxBig = -w * e;
            if (dxBig > -w)
            {
                rlPushMatrix();
                rlTranslatef(dxBig, 0.0f, 0.0f);
                drawBigList(dxBig, heroIndex_);
                rlPopMatrix();
            }

            float dxSub = w * (1.0f - renderer::easeOutCubic(t));
            rlPushMatrix();
            rlTranslatef(dxSub, 0.0f, 0.0f);
            drawSubList(big_[bigIndex_], dxSub, true);   // 左侧大图交给飞行体
            rlPopMatrix();

            drawHeroTransition(t);
        }
    }

    // 列表可滚动时给出操作提示
    if (bigIndex_ >= 0 && listMaxScroll() > 0.5f)
        engine()->fonts.draw(lang::tr("chapter.scroll_hint"), 40.0f, h - 88.0f, 20.0f,
                             withAlpha(pal().textDim, 210), 2.0f);

    Rectangle backBtn{24.0f, 24.0f, 120.0f, 44.0f};
    Vector2 mouse = GetMousePosition();
    bool hoverBack = CheckCollisionPointRec(mouse, backBtn);
    renderer::drawButton(backBtn, bigIndex_ < 0 ? lang::tr("common.back") : lang::tr("common.up"), engine()->fonts.font(),
                         20.0f, backHover_, hoverBack,
                         IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hoverBack);

    // 一级界面：复位视图按钮（缩放/平移乱了以后一键回到全览）
    if (bigIndex_ < 0 && viewT_ <= 0.001f)
    {
        Rectangle resetBtn{kResetBtnX, kResetBtnY, kResetBtnW, kResetBtnH};
        bool hr = CheckCollisionPointRec(mouse, resetBtn);
        renderer::drawButton(resetBtn, lang::tr("chapter.reset_view"), engine()->fonts.font(),
                             20.0f, resetHover_, hr,
                             IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hr);
        // 触控板没有滚轮也能缩放
        Rectangle zoomOutBtn{kZoomBtnOutX, kResetBtnY, kZoomBtnW, kResetBtnH};
        Rectangle zoomInBtn{kZoomBtnInX, kResetBtnY, kZoomBtnW, kResetBtnH};
        bool ho = CheckCollisionPointRec(mouse, zoomOutBtn);
        bool hi = CheckCollisionPointRec(mouse, zoomInBtn);
        renderer::drawButton(zoomOutBtn, "－", engine()->fonts.font(), 22.0f,
                             zoomHover_[0], ho, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ho,
                             zoomTarget_ > kZoomMin + 0.001f);
        renderer::drawButton(zoomInBtn, "＋", engine()->fonts.font(), 22.0f,
                             zoomHover_[1], hi, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hi,
                             zoomTarget_ < kZoomMax - 0.001f);
    }

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
                                         Color cornerFill, float cornerRad, float zoom, float dx)
{
    Texture2D tex = coverTexture(path);
    if (tex.id)
    {
        // 裁剪式铺满：背景完整覆盖卡片区域并居中，不留黑边
        float scale = std::max(r.width / static_cast<float>(tex.width),
                               r.height / static_cast<float>(tex.height)) *
                      (zoom > 0.0f ? zoom : 1.0f);
        float dw = tex.width * scale;
        float dh = tex.height * scale;
        float rs = engine()->renderScale();
        // 注意：几何由外层 rlTranslatef 平移，这里的裁剪框要自己加上同样的偏移，
        // 否则滑动动画中封面会被裁歪
        BeginScissorMode(static_cast<int>((r.x + dx) * rs), static_cast<int>(r.y * rs),
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
                      renderer::mix(pal().surfaceAlt, pal().accent, 0.22f));
    }

    // 圆角：四个角用卡片底色填充标准圆弧缺口，与卡片统一圆角矩形
    if (cornerFill.a > 0)
        drawRoundedCornerNotches(r, cornerRad, cornerFill);
}

// 同一个函数画三种形态：列表卡片(k=0) / 左侧大图(k=1) / 转场飞行体(0<k<1)。
// 内边距、封面高度、字号、进度条位置全部按 k 插值，所以首尾两帧与静态画面完全一致。
void ChapterSelectScene::drawBigCardBody(Rectangle r, const BigChapter& bc, int index,
                                         float k, float hover, float dx)
{
    const Color acc = renderer::accent();
    const Font& font = engine()->fonts.font();

    // 卡片有两种尺寸（主线 300x210、支线 240x170），左侧大图又是 436x500，
    // 所以列表形态的间距/字号按"卡片高度 / 250"缩放；k->1 时回到大图的固定规格。
    const float sList = r.height / 250.0f;
    const float s = sList + (1.0f - sList) * k;

    float inset   = 18.0f * s + 4.0f * k;                    // 文字左边距
    float coverH  = 150.0f * s + 212.0f * k;                 // 封面高度
    float titleY  = r.y + r.height - (76.0f * s + 46.0f * k);
    float titleFs = 26.0f * s + 4.0f * k;
    float progY   = r.y + r.height - (40.0f * s + 36.0f * k);
    float progFs  = 18.0f * s + 1.0f * k;
    float barY    = r.y + r.height - (20.0f * s + 28.0f * k);
    float barH    = 6.0f * s + 2.0f * k;
    float barX    = r.x + inset;
    float barW    = r.width - inset * 2.0f;

    DrawRectangleRounded(r, cornerOf(r), 12, pal().surface);

    Rectangle art{r.x + 8.0f, r.y + 8.0f, r.width - 16.0f, coverH};
    drawCardTexture(art, bc.cover, pal().surface, uiCorner() - 8.0f, 1.0f + 0.05f * hover, dx);

    // 底部渐变：从封面中部往下压暗，给标题让位（末端色 = 卡片底色，延伸无痕）
    renderer::drawGradientV({art.x, art.y + art.height * 0.45f, art.width,
                             (r.y + r.height - 12.0f) - (art.y + art.height * 0.45f)},
                            Color{0, 0, 0, 0}, withAlpha(pal().surface, 245), 48);

    // 序号徽章只属于列表卡片形态，随 k 缩小消失
    if (k < 0.98f)
    {
        float bs = 46.0f * s * (1.0f - k);
        drawIndexBadge(font, {r.x + 16.0f, r.y + 16.0f, bs, bs}, index + 1,
                       withAlpha(pal().surfaceSunken, 200),
                       withAlpha(pal().text, static_cast<unsigned char>(245 * (1.0f - k))));
    }

    engine()->fonts.draw(bc.title, r.x + inset, titleY, titleFs, pal().text, 2.6f + 0.4f * k);

    // 副标题（支线写「黎璘 · 云志」这类），压在封面下缘的渐变上
    if (!bc.subtitle.empty() && k < 0.98f)
        engine()->fonts.draw(bc.subtitle, r.x + inset, titleY - 30.0f * s, 17.0f * s,
                             withAlpha(pal().textDim, 235), 1.6f);

    // 主线 / 支线 徽章（右上角）
    {
        const bool main = isMain(bc);
        const char* key = main ? "chapter.branch_main" : "chapter.branch_side";
        Vector2 m = engine()->fonts.measure(lang::tr(key), 15.0f * s);
        Color fill = main ? withAlpha(acc, 215) : Color{198, 146, 84, 225};
        drawPill(font, {r.x + r.width - m.x - 30.0f * s, r.y + 16.0f * s}, lang::tr(key), fill,
                 Color{255, 255, 255, 245}, 15.0f * s, 9.0f * s);
    }

    int total = static_cast<int>(bc.script->chapters.size());
    std::string prog = lang::trf("chapter.progress", {std::to_string(bc.viewedCount), std::to_string(total)});
    engine()->fonts.draw(prog, r.x + inset, progY, progFs, pal().textDim, 1.8f + 0.1f * k);

    float ratio = total > 0 ? static_cast<float>(bc.viewedCount) / total : 0.0f;
    DrawRectangleRounded({barX, barY, barW, barH}, barH * 0.5f, 8, withAlpha(pal().line, 30));
    if (ratio > 0.0f)
        DrawRectangleRounded({barX, barY, barW * ratio, barH}, barH * 0.5f, 8, acc);

    // 描边：卡片形态随悬停变亮变粗，大图形态是淡淡的常规线
    float bw = 1.5f + 1.5f * hover;
    Color border = mixColor(withAlpha(pal().line, 40), withAlpha(acc, 220), hover);
    border = renderer::mix(border, withAlpha(pal().line, 40), k);
    bw = bw * (1.0f - k) + 1.5f * k;
    renderer::drawRoundedBorder(r, uiCorner(), bw, border);

    // 未解锁：压暗 + 锁 + 解锁条件
    if (bc.locked)
    {
        DrawRectangleRounded(r, cornerOf(r), 12, Color{8, 10, 20, 176});
        Vector2 m = engine()->fonts.measure(lang::tr("chapter.locked"), 18.0f * s);
        drawPill(font, {r.x + (r.width - m.x - 24.0f * s) * 0.5f, r.y + r.height * 0.42f},
                 lang::tr("chapter.locked"), withAlpha(pal().surfaceSunken, 235),
                 withAlpha(pal().textMuted, 245), 18.0f * s, 12.0f * s);
        // 解锁条件不写在这里——它写在"进入这张卡的那条线"上（见 drawRequirementLabels）
    }
}

void ChapterSelectScene::drawBigList(float dx, int skipCard)
{
    float w = static_cast<float>(canvas::width());
    float h = static_cast<float>(canvas::height());
    const Color acc = renderer::accent();

    // 顺序：两块底板 → 主线连线 → 卡片（卡片盖在连线与底板上）
    if (viewT_ <= 0.001f)
    {
        drawTreeLegend();     // 主线 / 支线 两块底板与标题
        drawRouteTree(dx);
    }

    for (size_t i = 0; i < big_.size(); ++i)
    {
        Rectangle r = bigCardRectFor(static_cast<int>(i), dx);
        if (r.width <= 0.0f) continue;
        if (r.x + r.width < -40.0f || r.x > w + 40.0f) continue;   // 视口外不画
        if (r.y + r.height < -40.0f || r.y > h + 40.0f) continue;

        float a = renderer::easeInOut(bigHover_[i]);
        // hero 转场期间，被点中的那张卡整个交给飞行体绘制（否则同屏会出现两个）
        if (static_cast<int>(i) == skipCard) continue;
        drawBigCardBody(r, big_[i], static_cast<int>(i), 0.0f, a, dx);

        // 边框：单条轮廓随悬停变亮变粗（不叠加第二条线，避免描边抖闪）
        Color border = mixColor(Color{255, 255, 255, 40},
                                Color{acc.r, acc.g, acc.b, 220}, a);
        renderer::drawRoundedBorder(r, uiCorner(), 1.5f + 1.5f * a, border);
    }

    // 解锁条件写在入度线上（画在卡片之后，保证压在封面上也读得清）
    if (viewT_ <= 0.001f) drawRequirementLabels(dx);

    if (big_.empty())
        engine()->fonts.draw(lang::tr("chapter.none"), kCardMargin, h * 0.5f, 26.0f,
                             pal().textDim, 2.6f);

    // 缩放提示 + 当前比例（右下角）
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(zoom_ * 100.0f + 0.5f));
        std::string hint = std::string(lang::tr("chapter.zoom_hint")) + "   " + buf;
        Vector2 m = engine()->fonts.measure(hint, 17.0f);
        engine()->fonts.draw(hint, w - m.x - 26.0f, h - 34.0f, 17.0f,
                             withAlpha(pal().textDim, 205), 1.7f);
    }
}

void ChapterSelectScene::drawSubList(const BigChapter& bc, float dx, bool skipLeftPanel)
{
    float w = static_cast<float>(canvas::width());
    float h = static_cast<float>(canvas::height());
    (void)h;
    const Font& font = engine()->fonts.font();
    const Color acc = renderer::accent();
    const int nextIdx = nextChapterIndex(bc);
    const int n = static_cast<int>(bc.script->chapters.size());

    // ---------------- 左侧：大章节图（与列表卡片同一个画法，k=1）----------------
    Rectangle left{40.0f, 120.0f, 436.0f, 500.0f};
    if (!skipLeftPanel)
        drawBigCardBody(left, bc, 0, 1.0f, 0.0f, dx);

    // ---------------- 右侧：章节列表 ----------------
    engine()->fonts.draw(lang::tr("chapter.list"), kListX, 74.0f, 24.0f, pal().text, 2.4f);
    std::string count = lang::trf("chapter.count", {std::to_string(n)});
    engine()->fonts.draw(count, kListX + 148.0f, 80.0f, 18.0f,
                         pal().textMuted, 1.8f);

    // 这条线是主线还是支线（剧情树的当前位置）
    {
        const bool main = isMain(bc);
        const char* key = main ? "chapter.branch_main" : "chapter.branch_side";
        Vector2 m = engine()->fonts.measure(lang::tr(key), 16.0f);
        Color fill = main ? withAlpha(acc, 215) : Color{198, 146, 84, 225};
        drawPill(font, {kListX + kListW - m.x - 28.0f, 72.0f}, lang::tr(key), fill,
                 Color{255, 255, 255, 245}, 16.0f, 10.0f);
    }

    Rectangle view = listViewport();
    const float contentH = n > 0 ? n * kStep - kRowGap : 0.0f;
    const float rs = engine()->renderScale();

    // 裁剪区左右各留 6px 余量：边框/高亮全部画在行内，但留余量可以兜住
    // 缩放取整带来的 1px 溢出，避免最左/最右一列被削掉。
    BeginScissorMode(static_cast<int>((view.x - 6.0f + dx) * rs), static_cast<int>(view.y * rs),
                     static_cast<int>((view.width + 12.0f) * rs),
                     static_cast<int>(view.height * rs));
    for (int i = 0; i < n; ++i)
    {
        Rectangle r{kListX, view.y + i * kStep - subScroll_, kListW, kRowH};
        if (r.y + r.height < view.y - 2.0f || r.y > view.y + view.height + 2.0f) continue;

        const auto& ch = bc.script->chapters[i];
        bool viewed = viewed_.count(bc.path + "|" + std::to_string(ch.id)) != 0;
        bool next = (nextIdx == i);
        bool locked = nextIdx >= 0 && i > nextIdx;
        float a = locked ? 0.0f : renderer::easeInOut(subHover_[i]);

        Color base = withAlpha(pal().surfaceAlt, 238);
        Color hot = renderer::mix(pal().surfaceAlt, acc, 0.34f);
        hot.a = 246;
        Color fill = locked ? pal().surfaceSunken : mixColor(base, hot, a);
        DrawRectangleRounded(r, cornerOf(r), 12, fill);

        // 左侧高亮竖条（悬停时出现）
        if (a > 0.02f)
            DrawRectangleRounded({r.x + 1.0f, r.y + 12.0f, 4.0f, r.height - 24.0f}, 2.0f, 8,
                                 Color{acc.r, acc.g, acc.b, static_cast<unsigned char>(230 * a)});

        // 序号徽章
        Color badgeFill, badgeText;
        if (locked)
        {
            badgeFill = withAlpha(pal().surfaceSunken, 220);
            badgeText = pal().textMuted;
        }
        else if (next)
        {
            badgeFill = pal().highlight;
            badgeFill.a = 235;
            badgeText = Color{30, 26, 12, 255};
        }
        else
        {
            badgeFill = Color{static_cast<unsigned char>(acc.r * 0.85f),
                              static_cast<unsigned char>(acc.g * 0.85f),
                              static_cast<unsigned char>(acc.b * 0.85f), 225};
            badgeText = pal().text;
        }
        drawIndexBadge(font, {r.x + 18.0f, r.y + (kRowH - 44.0f) * 0.5f, 44.0f, 44.0f},
                       i + 1, badgeFill, badgeText);

        // 标题
        Color nameColor = locked ? pal().textMuted : pal().text;
        engine()->fonts.draw(ch.name, r.x + 78.0f, r.y + 11.0f, 24.0f, nameColor, 2.4f);

        // 状态胶囊
        std::string state = viewed ? lang::tr("chapter.viewed") : (next ? lang::tr("chapter.next") : lang::tr("chapter.locked"));
        Color pillFill, pillText;
        if (locked)
        {
            pillFill = withAlpha(pal().line, 18);
            pillText = pal().textMuted;
        }
        else if (next)
        {
            pillFill = withAlpha(pal().highlight, 42);
            pillText = renderer::mix(pal().highlight, Color{255, 255, 255, 255}, 0.3f);
        }
        else if (viewed)
        {
            pillFill = Color{acc.r, acc.g, acc.b, 46};
            pillText = Color{static_cast<unsigned char>(acc.r * 0.5f + 120.0f),
                             static_cast<unsigned char>(acc.g * 0.5f + 128.0f),
                             static_cast<unsigned char>(acc.b * 0.5f + 140.0f), 255};
        }
        else
        {
            pillFill = withAlpha(pal().line, 18);
            pillText = pal().textMuted;
        }
        // 字号经过字体度量校正后整体变大，这里把胶囊往下挪一点，别贴到标题
        drawPill(font, {r.x + 78.0f, r.y + 45.0f}, state, pillFill, pillText, 15.5f, 10.0f);

        // 右侧操作提示
        if (!locked)
        {
            engine()->fonts.draw(lang::tr("chapter.enter"), r.x + r.width - 100.0f, r.y + 26.0f, 19.0f,
                                 Color{static_cast<unsigned char>(acc.r * 0.55f + 120.0f),
                                       static_cast<unsigned char>(acc.g * 0.55f + 130.0f),
                                       static_cast<unsigned char>(acc.b * 0.55f + 145.0f),
                                       static_cast<unsigned char>(120 + 120 * a)}, 1.9f);
        }
        else
        {
            engine()->fonts.draw(lang::tr("chapter.locked_short"), r.x + r.width - 92.0f, r.y + 26.0f, 19.0f,
                                 withAlpha(pal().textMuted, 200), 1.9f);
        }

        // 边框（内描边：可见尺寸 == 行尺寸，不会被裁剪框切掉）
        if (next)
        {
            renderer::drawRoundedBorder(r, uiCorner(), 2.5f, withAlpha(pal().highlight, 235));
            Rectangle inner{r.x + 4.0f, r.y + 4.0f, r.width - 8.0f, r.height - 8.0f};
            renderer::drawRoundedBorder(inner, std::max(0.0f, uiCorner() - 4.0f), 1.5f,
                                        withAlpha(pal().highlight, 95));
        }
        else
        {
            Color border = locked ? withAlpha(pal().line, 16)
                                  : mixColor(pal().line,
                                             Color{acc.r, acc.g, acc.b, 200}, a);
            renderer::drawRoundedBorder(r, uiCorner(), 1.5f, border);
        }
    }
    EndScissorMode();

    // 这里原本有两条"上下边缘渐隐带"，用意是暗示还有内容，
    // 但列表背后是页面背景、而渐隐带用的是面板色，颜色对不上，
    // 反而在视口边界形成了一条很明显的暗带（实测有 -22 亮度跳变）。
    // 列表本身是一张张独立卡片、卡片间有间隙，边界裁切已经足够自然，直接去掉。

    // 滚动条
    drawScrollbar({kBarX, view.y, kBarW, view.height}, view.height, contentH, subScroll_);
    (void)w;
}

void ChapterSelectScene::drawScrollbar(Rectangle track, float viewH, float contentH, float scroll)
{
    float maxScroll = std::max(0.0f, contentH - viewH);
    if (maxScroll <= 0.5f) return;
    float rr = renderer::roundness(track.width * 0.5f, track);
    DrawRectangleRounded(track, rr, 16, withAlpha(pal().line, 20));

    float thumbH = std::max(52.0f, viewH * (viewH / contentH));
    float t = scroll / maxScroll;
    Rectangle thumb{track.x, track.y + t * (viewH - thumbH), track.width, thumbH};
    const Color acc = renderer::accent();
    DrawRectangleRounded(thumb, rr, 16,
                         Color{acc.r, acc.g, acc.b,
                               static_cast<unsigned char>(150 + 95 * barHover_)});
}

// 共享元素转场：封面从 heroFrom_ 飞到 heroTo_，中途微微"抬起"并带投影，落位有回弹
void ChapterSelectScene::drawHeroTransition(float t)
{
    if (heroIndex_ < 0 || heroIndex_ >= static_cast<int>(big_.size())) return;
    const BigChapter& bc = big_[heroIndex_];

    // 进入时 t: 0->1，返回时 t: 1->0，都映射成 0->1 的飞行进度
    float p = (viewTarget_ == 1.0f) ? t : (1.0f - t);
    p = std::clamp(p, 0.0f, 1.0f);

    // 平滑飞行 + 末尾一点过冲（落位"吸附"感），两端严格为 0 / 1 保证无缝衔接
    float e = renderer::easeInOut(p) + 0.08f * std::sin(3.14159265f * p * p * p);
    Rectangle r = lerpRect(heroFrom_, heroTo_, e);

    // 中段"抬起"：轻微放大 + 投影，让卡片看起来是浮在界面上飞过去（两端都为 0，不破坏衔接）
    float arc = std::sin(3.14159265f * p);
    float lift = 1.0f + 0.04f * arc;
    Rectangle rr{r.x - r.width * (lift - 1.0f) * 0.5f,
                 r.y - r.height * (lift - 1.0f) * 0.5f,
                 r.width * lift, r.height * lift};

    if (arc > 0.01f)
        DrawRectangleRounded({rr.x + 8.0f, rr.y + 12.0f, rr.width, rr.height},
                             cornerOf(rr), 12,
                             Color{0, 0, 0, static_cast<unsigned char>(110 * arc)});

    // 整张卡（封面 + 标题 + 进度）的形态按 k 连续变形。
    // 注意 k 必须跟着"起点/终点形态"走，不能直接用飞行进度：
    // 进入时 卡片->大图(k: 0->1)，返回时 大图->卡片(k: 1->0)，
    // 否则一按返回，左侧大图会瞬间塌成小卡片样式（很明显的跳变）。
    float k = heroKFrom_ + (heroKTo_ - heroKFrom_) * e;
    drawBigCardBody(rr, bc, heroIndex_, std::clamp(k, 0.0f, 1.0f), 0.0f, 0.0f);
    if (arc > 0.1f)
        renderer::drawRoundedBorder(rr, uiCorner(), 2.0f,
                                    withAlpha(pal().lineStrong,
                                              static_cast<unsigned char>(210 * arc)));
}

void ChapterSelectScene::debugAuto(int frame)
{
    if (frame == 150) engine()->selftestShot("chapter_big");
    else if (frame == 161)
    {
        engine()->selftestShot("chapter_preclick");                        // 点击前一帧
        // 从这一帧开始，按"第一张大卡片的屏幕中心"模拟真实点击 ——
        // 缩放/平移之后再点，走的是同一个命中测试，能验出变换算错的情况
        if (!big_.empty())
        {
            Rectangle r = bigCardRectFor(0);
            debugBigClick_ = Vector2{r.x + r.width * 0.5f, r.y + r.height * 0.5f};
            debugBigClickPhase_ = 1;
        }
    }
    else if (frame == 162 && !big_.empty())
    {
        engine()->selftestShot("chapter_click");   // 点击当帧（曾闪一帧小章节界面）
    }
    else if (frame == 163) engine()->selftestShot("chapter_transition_a");  // 起飞
    else if (frame == 167) engine()->selftestShot("chapter_transition_b");  // 飞行中段
    else if (frame == 172) engine()->selftestShot("chapter_transition_c");  // 落位
    else if (frame == 190) engine()->selftestShot("chapter_select_top");    // 动画结束、未滚动（玩家首见）
    else if (frame == 195)
    {
        subScrollTarget_ = listMaxScroll();   // 自检滚动到底部，验证长列表可见
    }
    else if (frame == 225) engine()->selftestShot("chapter_select");
    else if (frame == 240)   // 触发返回动画，并在同一帧截图（此时刚走了一步）
    {
        backToBig();
        engine()->selftestShot("chapter_back_a");
    }
    else if (frame == 244) engine()->selftestShot("chapter_back_b");
    else if (frame == 254) engine()->selftestShot("chapter_back_c");
    else if (frame == 260 && !big_.empty())
    {
        enterSub(0);          // 返回动画验完后，再进一次二级列表
        viewT_ = 1.0f;        // 自检直接到位，省掉等待
        subScroll_ = subScrollTarget_ = 0.0f;
    }
    else if (frame == 262 && bigIndex_ >= 0 && !big_[bigIndex_].script->chapters.empty())
    {
        debugClickRow_ = nextChapterIndex(big_[bigIndex_]);   // 模拟点击"下一章"
        if (debugClickRow_ < 0) debugClickRow_ = 0;
        debugClickPhase_ = 0;
    }
}
