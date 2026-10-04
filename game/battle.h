#pragma once

#include <string>
#include <vector>

#include <raylib.h>

#include "../core/script.h"

class FontManager;

// 回合制指令战斗（"观测战"）：
//   我方 3 人（用剧本里注册过的角色，数值查内置表），敌方 1~3 个（剧本里 init_battle 定义）。
//   每人每回合选 攻击 / 技能 / 守；敌我按 SPD 排行动顺序。
//   打完 → won() 告诉 VM 跳 win / lose 标签（输了不 Game Over，剧情照走）。
//
// 主题上敌人不是"怪物"，是灯、云、杂讯这些挡在看星星路上的东西：
//   名字里带「光 / 灯 / 霓虹」画成发光球，带「云 / 雨 / 雾」画成云团，
//   带「杂讯 / 噪音」画成雪花噪点，其余画成六边形。
class Battle
{
public:
    // 一条战斗单位
    struct Unit
    {
        std::string name;          // 角色名 / 敌人名
        bool ally = true;
        int hp = 100, hpMax = 100;
        int atk = 12, def = 6, spd = 10;
        int sp = 0, spMax = 6;
        int guard = 0;             // 防御中（本回合受伤减半）
        int stun = 0;              // 停止行动剩余回合
        int atkBuff = 0;           // 攻击提升剩余回合
        int vuln = 0;              // 受伤增加剩余回合（被"观测"标记）
        float flash = 0.0f;        // 受击闪白
        float shake = 0.0f;        // 受击抖动
        bool alive() const { return hp > 0; }
    };

    void setup(const BattleDef& def);
    void update(float dt);
    void draw(const FontManager& fonts);
    bool finished() const { return phase_ == Phase::Result; }
    bool won() const { return won_; }
    void debugAuto(int frame);     // 窗口自检：自动出招
    // 无头自检：不开窗口、不读输入，按固定策略把这场战斗跑完
    // （只验证"能打完、不会死循环"；返回 true = 正常结束）
    bool runHeadless(int maxSteps = 20000);

    const std::string& title() const { return title_; }
    int round() const { return round_; }

private:
    enum class Phase { Intro, Command, Acting, Result };

    static const int kPartySize = 3;
    static const int kMaxEnemies = 3;

    void beginRound();
    void nextTurn();
    void actAlly(int cmd);
    void actEnemy(int idx);
    void dealDamage(int targetIdx, int amount, bool crit);
    void healAll(int amount);
    void applyBuff(int kind, int value);
    void pushLog(const std::string& s);
    void checkEnd();
    int aliveEnemies() const;
    int aliveAllies() const;
    void enemyRect(int idx, Rectangle& out) const;
    void allyRect(int idx, Rectangle& out) const;
    const char* skillName(int ally) const;
    bool isCommandHovered(int cmd, Rectangle& out) const;

    std::vector<Unit> allies_;
    std::vector<Unit> enemies_;
    std::vector<int> order_;        // 行动顺序（1..n 我方，负数为敌方下标）
    size_t orderPos_ = 0;
    int round_ = 0;
    int maxRounds_ = 12;
    std::string title_;
    bool won_ = true;
    Phase phase_ = Phase::Intro;
    float timer_ = 0.0f;
    float introT_ = 0.0f;
    float resultT_ = 0.0f;
    int cmdHover_ = -1;
    float cmdHoverAnim_[3] = {0.0f, 0.0f, 0.0f};
    int autoCmd_ = -1;              // 自检用：指定这一手出什么
    int buffValue_ = 30;            // 当前攻击加成百分比（技能决定）
    std::vector<std::string> log_;
    std::vector<std::pair<Vector2, std::pair<int, float>>> popups_;   // 飘字：位置/数值/剩余时间
    float time_ = 0.0f;
};
