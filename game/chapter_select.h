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
        std::shared_ptr<Script> script;
        std::string cover;         // 大章节封面 = 第一章的 picture
        int viewedCount = 0;
    };

    void scanScripts();
    void refreshViewed();
    Texture2D coverTexture(const std::string& path);
    int nextChapterIndex(const BigChapter& bc) const;
    void startChapter(const BigChapter& bc, int chapterIdx);   // -1 = 整脚本
    void drawBigList();
    void drawSubList(const BigChapter& bc);
    void drawGlow(Rectangle r, float timeSec) const;
    void drawCardTexture(Rectangle r, const std::string& path, Color cornerFill,
                         float cornerRad);

    std::string defaultScript_;
    std::string parseError_;
    std::shared_ptr<Script> titleScript_;
    std::vector<BigChapter> big_;
    int bigIndex_ = -1;
    std::set<std::string> viewed_;
    std::map<std::string, Texture2D> coverTex_;
    std::vector<float> bigHover_;
    std::vector<float> subHover_;
    int hoverBig_ = -1;
    int hoverSub_ = -1;
    Texture2D bg_{};
    float backHover_ = 0.0f;
};
