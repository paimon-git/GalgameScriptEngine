#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// ============================================================
//  QLWT 脚本语言（.gal）语法定义
//
//  title <文本>
//  init_character <名字> <贴图> <#RRGGBB颜色> <left|center|right|0..1>
//  init_character_face <名字> <表情目录>
//  init_bg <场景ID> <贴图>
//  show_bg <场景ID>
//  show_character <名字>
//  hide_character <名字>
//  change_face <名字> <表情文件名>
//  say <名字> "<台词>"
//  choice <名字> "<提问>" {
//      "选项文本", jump <标签>
//      "选项文本", jump <标签>
//  }
//  @<标签名>
//  jump <标签名>
//  wait <秒数>
//  play_bgm <音频文件>  /  play_se <音频文件>  /  stop_bgm
//  game_end
//
//  # 开头为注释；#RRGGBB（6位十六进制）会被识别为颜色
// ============================================================

struct SayStmt      { std::string character; std::string text; };
struct ChoiceStmt
{
    std::string character;
    std::string text;
    std::vector<std::pair<std::string, std::string>> options;  // 文本 -> 目标标签
};
struct InitCharacterStmt { std::string name, texture; unsigned int color = 0xFFFFFF; std::string position; };
struct InitCharacterFaceStmt { std::string name, dir; };
struct InitBgStmt { std::string id, texture; };
struct ShowBgStmt { std::string id; };
struct ShowCharacterStmt { std::string name; };
struct HideCharacterStmt { std::string name; };
struct ChangeFaceStmt { std::string name, file; };
struct JumpStmt { std::string label; };
struct LabelStmt { std::string name; };
struct WaitStmt { float seconds = 0.0f; };
struct NarrateStmt { std::string text; };
struct MoveStmt { std::string name; std::string position; };
struct CardStmt { std::string text; };
struct ShowCgStmt { std::string file; };
struct HideCgStmt {};
struct PlayBgmStmt { std::string file; };
struct StopBgmStmt {};
struct PlaySeStmt { std::string file; };
struct GameEndStmt {};
struct ChapterEndStmt {};          // 章节体隐式结束（防止流入下一章）
struct TitleStmt { std::string text; };

// 战斗节点：打完按结果跳到对应标签
struct BattleStmt
{
    std::string id;
    std::string winLabel;
    std::string loseLabel;
};

using Stmt = std::variant<
    SayStmt, ChoiceStmt, InitCharacterStmt, InitCharacterFaceStmt, InitBgStmt,
    ShowBgStmt, ShowCharacterStmt, HideCharacterStmt, ChangeFaceStmt, JumpStmt,
    LabelStmt, WaitStmt, NarrateStmt, MoveStmt, CardStmt, ShowCgStmt, HideCgStmt, ChapterEndStmt,
    PlayBgmStmt, StopBgmStmt, PlaySeStmt, GameEndStmt, TitleStmt, BattleStmt>;

// ---------------- 战斗定义（写在剧本头部，解析期收集）----------------
//   init_battle <id> <敌人名> <hp> <atk> <spd>      每写一条 = 一个敌人
//   init_battle_party <id> <成员1> [成员2] [成员3]   出战成员（用已注册的角色名）
struct EnemyDef
{
    std::string name;
    int hp = 100;
    int atk = 12;
    int spd = 8;
};

struct BattleDef
{
    std::string id;
    std::vector<EnemyDef> enemies;
    std::vector<std::string> party;
};

// 章节信息：大章节（脚本）下的小章节
struct ChapterInfo
{
    int id = 0;
    std::string name;
    std::string picture;           // 章节封面
    size_t stmtIndex = 0;          // 章节体在语句列表中的起始位置
};

struct Script
{
    std::string title;
    // ---- 剧情树元信息（写在脚本头部的元命令，见下）----
    //   series   星屑祭典                      所属系列（剧情树的根）
    //   branch   main | side                   主线 / 支线
    //   order    3                             同系列内排序（小的在前）
    //   subtitle 黎璘 · 云志                    卡片副标题（可选）
    //   requires star_festival:4               解锁前置 = 某脚本的第 N 章已观看（可选）
    std::string series;
    std::string branch = "main";
    std::string subtitle;
    int order = 0;
    std::string requiresScript;      // 前置脚本文件名（不含 .gal）
    int requiresChapter = 0;         // 前置章节号；0 = 无前置
    // 可以写多条 requires，全部满足才解锁（AND）
    std::vector<std::pair<std::string, int>> requiresList;
    std::string sourcePath;
    std::vector<Stmt> stmts;
    std::map<std::string, size_t> labels;   // 标签名 -> 语句下标
    std::vector<ChapterInfo> chapters;
    std::map<std::string, BattleDef> battles;   // 战斗 id -> 定义
};

class ScriptError : public std::runtime_error
{
public:
    ScriptError(int line, const std::string& msg)
        : std::runtime_error("line " + std::to_string(line) + ": " + msg), line_(line) {}
    int line() const { return line_; }

private:
    int line_;
};

// 解析 .gal 文本（UTF-8）。失败抛出 ScriptError。
Script parseGal(const std::string& text, const std::string& sourcePath);
