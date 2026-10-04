#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <raylib.h>

#include "../core/script.h"
#include "scene.h"

// 章节选择（蓝档案风格）：
// 一级 = 大章节（脚本）横向卡片；二级 = 左侧大图 + 右侧小章节列表
// 列表支持滚轮 / 键盘 / 拖拽滚动，内容超出视口时显示滚动条。
class ChapterSelectScene : public Scene
{
public:
    ChapterSelectScene(Engine* e, std::string defaultScript, std::string parseError);
    ~ChapterSelectScene() override;

    void update(float dt) override;
    void draw() override;
    void debugAuto(int frame) override;

private:
    struct BigChapter
    {
        std::string path;
        std::string title;
        std::string subtitle;      // 卡片副标题（支线写「黎璘 · 云志」这种）
        std::string series;        // 所属系列（剧情树的根）
        std::string branch = "main";   // main / side
        int order = 0;             // 同系列内排序
        std::shared_ptr<Script> script;
        std::string cover;         // 大章节封面 = 第一章的 picture
        int viewedCount = 0;
        bool locked = false;       // 前置章节没看完 → 锁住
        std::string requiresText;  // 解锁条件（已解锁则为空）
        std::vector<std::string> reqLines;   // 未满足的前置条件，一条一行
        std::string requiresPath;  // 前置脚本路径（画树枝用）
        int requiresChapter = 0;   // 前置章节号
        std::vector<std::pair<std::string, int>> reqs;   // 全部前置（AND）
    };

    void scanScripts();
    void refreshViewed();
    Texture2D coverTexture(const std::string& path);
    int nextChapterIndex(const BigChapter& bc) const;
    void startChapter(const BigChapter& bc, int chapterIdx);   // -1 = 整脚本
    void enterSub(int index);      // 进入小章节列表（带动画）
    void backToBig();              // 返回大章节列表（带动画）
    // 剧情地图：一级界面是一张可以自由缩放/平移的图，
    // 主线一排在上，支线整块单独放在下面的区域里（不再挂在主线上）
    void buildBigLayout();         // 世界坐标布局（不含缩放/平移）
    Rectangle bigCardRectFor(int index, float dx = 0.0f) const;
    void drawRouteTree(float dx);
    void drawTreeLegend();
    void drawMapChrome();          // 主线/支线两块的标题与底板
    void drawRequirementLabels(float dx);   // 解锁条件写在"进入这张卡的线"上
    void resetView();              // 复位缩放与平移
    Vector2 screenToWorld(Vector2 p) const;
    Rectangle worldRect(Rectangle r) const;   // 世界坐标 → 屏幕坐标
    int prereqIndex(const BigChapter& bc) const;
    bool isMain(const BigChapter& bc) const { return bc.branch != "side"; }
    void drawBigList(float dx = 0.0f, int skipCard = -1);
    void drawSubList(const BigChapter& bc, float dx = 0.0f, bool skipLeftPanel = false);
    // 大卡片的统一画法：同一套函数画"列表卡片"(k=0) / "左侧大图"(k=1) / 飞行体(0<k<1)，
    // 保证转场首尾两帧和静态画面逐像素一致（没有这一步就只能靠遮罩硬切）。
    void drawBigCardBody(Rectangle r, const BigChapter& bc, int index, float k, float hover,
                         float dx);
    // 共享元素（hero）转场：被点中的封面从大卡片位置飞到左侧封面位
    void drawHeroTransition(float t);
    void drawCardTexture(Rectangle r, const std::string& path, Color cornerFill,
                         float cornerRad, float zoom = 1.0f, float dx = 0.0f);
    void drawScrollbar(Rectangle track, float viewH, float contentH, float scroll);
    Rectangle listViewport() const;
    float listMaxScroll() const;

    std::string defaultScript_;
    std::string parseError_;
    std::shared_ptr<Script> titleScript_;
    std::vector<BigChapter> big_;
    std::vector<Rectangle> bigRect_;    // 每张大卡片的世界坐标
    Rectangle bigMainBox_{};            // 主线那一块（世界坐标，画底板用）
    Rectangle bigSideBox_{};            // 支线那一块（世界坐标）
    int bigIndex_ = -1;
    std::set<std::string> viewed_;
    std::map<std::string, Texture2D> coverTex_;
    std::vector<float> bigHover_;
    std::vector<float> subHover_;
    int hoverBig_ = -1;
    int hoverSub_ = -1;
    Texture2D bg_{};
    Texture2D bgBlur_{};        // 背景的高斯模糊版本（构建时算一次，之后直接贴）
    float backHover_ = 0.0f;

    // 大章节 <-> 小章节 的切换动画：0 = 大列表，1 = 小列表
    float viewT_ = 0.0f;
    float viewTarget_ = 0.0f;
    int heroIndex_ = -1;            // 正在飞行的是哪个大章节
    Rectangle heroFrom_{};          // 飞行起点（封面矩形）
    Rectangle heroTo_{};            // 飞行终点（封面矩形）
    float heroKFrom_ = 0.0f;        // 起点形态：0 = 列表卡片，1 = 左侧大图
    float heroKTo_ = 1.0f;          // 终点形态

    // 一级（剧情地图）视图变换：屏幕 = 世界 * zoom + pan
    float zoom_ = 0.85f;
    float zoomTarget_ = 0.85f;
    Vector2 pan_{0.0f, 0.0f};
    Vector2 panTarget_{0.0f, 0.0f};
    bool mapDragging_ = false;
    bool mapDragMoved_ = false;
    Vector2 mapDragStart_{};
    Vector2 mapDragPanStart_{};
    float resetHover_ = 0.0f;
    float zoomHover_[2] = {0.0f, 0.0f};   // 0 = 缩小，1 = 放大
    // 自检：在屏幕某点模拟真实的按下/松开，用来验证"缩放平移之后的命中测试"
    Vector2 debugBigClick_{0.0f, 0.0f};
    int debugBigClickPhase_ = 0;
    // 二级（小章节）纵向滚动
    float subScroll_ = 0.0f;
    float subScrollTarget_ = 0.0f;
    bool scrollInit_ = false;
    bool listDragging_ = false;
    bool listDragMoved_ = false;
    float listDragStartY_ = 0.0f;
    float listDragStartScroll_ = 0.0f;
    int listPressRow_ = -1;
    bool barDragging_ = false;
    float barDragOffset_ = 0.0f;
    float barHover_ = 0.0f;
    int debugEnter_ = -1;   // 自检：模拟"点击卡片"发生在 update 内部的时机
    int debugClickRow_ = -1;  // 自检：模拟点击某一行章节（先按下、再松开的真实序列）
    int debugClickPhase_ = 0;
};
