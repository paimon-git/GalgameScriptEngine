#pragma once

#include <memory>
#include <string>

#include "../game/game.h"
#include "script.h"

// .gal 脚本解释器：顺序执行指令，遇到 say / choice / wait / game_end 时暂停
class VM
{
public:
    enum class State { Running, WaitSay, WaitChoice, WaitTimer, WaitNarrate, WaitCard, WaitHide, WaitCg, Ended, Error };

    void load(const Script& script);
    void step(Game& game);
    void tick(float dt);                 // 仅 WaitTimer 时推进
    void jumpTo(const std::string& label, Game& game);   // 选项选中后跳转并继续执行
    void restore(size_t ip, State state, float waitRemaining);   // 读档恢复
    void startAt(size_t index, Game& game);   // 从指定章节起点开始（先执行初始化指令）

    State state() const { return state_; }
    const SayStmt* say() const { return say_; }
    const ChoiceStmt* choice() const { return choice_; }
    const NarrateStmt* narrate() const { return narrate_; }
    const CardStmt* card() const { return card_; }
    const std::string& hideName() const { return hideName_; }
    float waitRemaining() const { return waitRemaining_; }
    const std::string& error() const { return error_; }
    size_t ip() const { return ip_; }
    size_t steps() const { return steps_; }

private:
    const Script* script_ = nullptr;
    size_t ip_ = 0;
    size_t steps_ = 0;
    State state_ = State::Running;
    const SayStmt* say_ = nullptr;
    const ChoiceStmt* choice_ = nullptr;
    const NarrateStmt* narrate_ = nullptr;
    const CardStmt* card_ = nullptr;
    std::string hideName_;
    float waitRemaining_ = 0.0f;
    std::string error_;
};
