#pragma once

#include <string>
#include <vector>

// 存档数据结构：脚本位置 + 场景状态快照
struct CharSave
{
    std::string name;
    std::string bodyPath;
    std::string facePath;
    std::string faceDir;
    float anchorX = 0.5f;
    bool visible = false;
};

struct LogLineSave
{
    std::string name;
    std::string text;
};

struct SaveData
{
    bool valid = false;
    size_t ip = 0;
    int vmState = 0;
    float waitRemaining = 0.0f;
    std::string title;
    std::string bg;
    std::string cg;
    std::string sayCharacter;
    std::string sayText;
    bool autoMode = false;
    std::string savedAt;
    std::string preview;
    std::vector<CharSave> chars;
    std::vector<std::pair<std::string, std::string>> bgs;   // 场景ID -> 贴图路径
    std::vector<LogLineSave> log;
};

constexpr int kSaveSlots = 6;

std::string saveSlotPath(int slot);          // saves/slot1.dat
bool saveSlot(int slot, const SaveData& data);
bool loadSlot(int slot, SaveData& out);

// 章节进度：progress.dat 记录已观看的章节（scriptPath|chapterId）
bool progressIsViewed(const std::string& scriptPath, int chapterId);
void progressMarkViewed(const std::string& scriptPath, int chapterId);
void progressEnsureCreated();
