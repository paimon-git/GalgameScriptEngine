#pragma once

#include <memory>
#include <string>
#include <vector>

#include <raylib.h>

#include "../core/save.h"
#include "../core/script.h"
#include "../core/vm.h"
#include "choice.h"
#include "dialogue.h"
#include "game.h"
#include "scene.h"

struct LogEntry
{
    std::string name;
    std::string text;
    Color color{255, 255, 255, 255};
};

// 游戏场景：执行 .gal 脚本，渲染背景/角色/对话框/选项/历史
class GameScene : public Scene
{
public:
    GameScene(Engine* e, std::string scriptPath,
              std::shared_ptr<Script> script, std::string parseError,
              int startChapter = -1);

    void update(float dt) override;
    void draw() override;
    void debugAuto(int frame) override;
    void afterDraw() override;

private:
    void start();
    void syncToVM();
    void userAdvance();
    void choose(int idx);
    void addLog(const std::string& name, const std::string& text);
    void backToTitle();
    void queueShot(const std::string& tag);
    void drawLog() const;
    void drawOverlays() const;
    void drawNarrate() const;
    void drawCard() const;
    void openSaveMenu();
    void openLoadMenu();
    void handleMenu(float dt);
    void drawMenu() const;
    void handleEscMenu();
    void drawEscMenu();
    SaveData buildSave() const;
    void applyLoad(const SaveData& data);

    std::string scriptPath_;
    std::string parseError_;
    std::shared_ptr<Script> script_;
    int currentChapter_ = -1;      // 当前章节下标（整脚本播放为 -1）
    bool chapterAdvancing_ = false;
    bool chapterViewedMarked_ = false;
    float chapterAdvanceFade_ = 0.0f;
    bool chapterSwitchSent_ = false;
    bool chapterPrompt_ = false;   // 章节结束询问面板
    float promptHover_[2] = {0.0f, 0.0f};
    VM vm_;
    Game game_{true};
    DialogueBox dlg_;
    ChoicePanel choice_;

    std::vector<LogEntry> log_;
    bool logOpen_ = false;
    int logScroll_ = 0;

    bool autoMode_ = false;
    float autoTimer_ = 0.0f;
    float inputCooldown_ = 0.35f;  // 开场输入冷却，防止启动点击误跳对话
    float fadeAlpha_ = 1.0f;
    bool prevCgShown_ = false;
    int cgShot_ = 0;
    int dlgShotCount_ = 0;
    std::string pendingShot_;
    int shotAtFrame_ = 0;
    bool ended_ = false;
    float endTimer_ = 0.0f;

    // 顶部旁白
    std::string narrateText_;
    float narrateAlpha_ = 0.0f;
    bool narrateActive_ = false;

    // 章节 / 地点卡片
    std::string cardText_;
    float cardT_ = 0.0f;
    bool cardActive_ = false;
    bool cardClosing_ = false;
    float cardCloseT_ = 0.0f;
    int narrateHold_ = 0;          // 自检：旁白最短停留帧

    // 角色消失黑屏过渡
    std::string hideName_;
    float hideT_ = 0.0f;
    float hideBlack_ = 0.0f;
    bool hideCleared_ = false;

    // 存档 / 读档
    bool saveMenu_ = false;
    bool loadMenu_ = false;
    bool escMenu_ = false;
    float escHover_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int menuHover_ = -1;
    std::vector<float> menuHoverAnim_;
    std::vector<SaveData> slotData_;
};
