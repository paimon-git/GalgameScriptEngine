#include "save.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

std::string saveSlotPath(int slot)
{
    return "saves/slot" + std::to_string(slot) + ".dat";
}

bool saveSlot(int slot, const SaveData& d)
{
    std::error_code ec;
    std::filesystem::create_directories("saves", ec);
    std::ofstream out(saveSlotPath(slot), std::ios::trunc);
    if (!out) return false;

    out << "valid=1\n";
    out << "ip=" << d.ip << "\n";
    out << "vmstate=" << d.vmState << "\n";
    out << "wait=" << d.waitRemaining << "\n";
    out << "title=" << d.title << "\n";
    out << "bg=" << d.bg << "\n";
    out << "cg=" << d.cg << "\n";
    out << "saychar=" << d.sayCharacter << "\n";
    out << "saytext=" << d.sayText << "\n";
    out << "auto=" << (d.autoMode ? 1 : 0) << "\n";
    out << "savedat=" << d.savedAt << "\n";

    out << "chars=" << d.chars.size() << "\n";
    for (size_t i = 0; i < d.chars.size(); ++i)
    {
        const auto& c = d.chars[i];
        out << "c" << i << "_name=" << c.name << "\n";
        out << "c" << i << "_body=" << c.bodyPath << "\n";
        out << "c" << i << "_face=" << c.facePath << "\n";
        out << "c" << i << "_dir=" << c.faceDir << "\n";
        out << "c" << i << "_anchor=" << c.anchorX << "\n";
        out << "c" << i << "_visible=" << (c.visible ? 1 : 0) << "\n";
    }

    out << "bgs=" << d.bgs.size() << "\n";
    for (size_t i = 0; i < d.bgs.size(); ++i)
    {
        out << "b" << i << "_id=" << d.bgs[i].first << "\n";
        out << "b" << i << "_tex=" << d.bgs[i].second << "\n";
    }

    out << "log=" << d.log.size() << "\n";
    for (size_t i = 0; i < d.log.size(); ++i)
    {
        out << "l" << i << "_name=" << d.log[i].name << "\n";
        out << "l" << i << "_text=" << d.log[i].text << "\n";
    }
    out.close();
    return true;
}

bool loadSlot(int slot, SaveData& d)
{
    d = SaveData{};
    std::ifstream in(saveSlotPath(slot));
    if (!in) return false;

    size_t charCount = 0, logCount = 0;
    std::string line;
    while (std::getline(in, line))
    {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "valid") d.valid = (val == "1");
        else if (key == "ip") d.ip = std::stoull(val);
        else if (key == "vmstate") d.vmState = std::atoi(val.c_str());
        else if (key == "wait") d.waitRemaining = static_cast<float>(std::atof(val.c_str()));
        else if (key == "title") d.title = val;
        else if (key == "bg") d.bg = val;
        else if (key == "cg") d.cg = val;
        else if (key == "saychar") d.sayCharacter = val;
        else if (key == "saytext") d.sayText = val;
        else if (key == "auto") d.autoMode = (val == "1");
        else if (key == "savedat") d.savedAt = val;
        else if (key == "chars") charCount = std::stoull(val);
        else if (key == "log") logCount = std::stoull(val);
        else if (key == "bgs") d.bgs.resize(std::stoull(val));
        else if (key.size() > 2 && key[0] == 'b' && key.find("_id") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.bgs.size() <= idx) d.bgs.resize(idx + 1);
            d.bgs[idx].first = val;
        }
        else if (key.size() > 2 && key[0] == 'b' && key.find("_tex") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.bgs.size() <= idx) d.bgs.resize(idx + 1);
            d.bgs[idx].second = val;
        }
        else if (key.size() > 2 && key[0] == 'c' && key.find("_name") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.chars.size() <= idx) d.chars.resize(idx + 1);
            d.chars[idx].name = val;
        }
        else if (key.size() > 2 && key[0] == 'c' && key.find("_body") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.chars.size() <= idx) d.chars.resize(idx + 1);
            d.chars[idx].bodyPath = val;
        }
        else if (key.size() > 2 && key[0] == 'c' && key.find("_face") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.chars.size() <= idx) d.chars.resize(idx + 1);
            d.chars[idx].facePath = val;
        }
        else if (key.size() > 2 && key[0] == 'c' && key.find("_dir") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.chars.size() <= idx) d.chars.resize(idx + 1);
            d.chars[idx].faceDir = val;
        }
        else if (key.size() > 2 && key[0] == 'c' && key.find("_anchor") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.chars.size() <= idx) d.chars.resize(idx + 1);
            d.chars[idx].anchorX = static_cast<float>(std::atof(val.c_str()));
        }
        else if (key.size() > 2 && key[0] == 'c' && key.find("_visible") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.chars.size() <= idx) d.chars.resize(idx + 1);
            d.chars[idx].visible = (val == "1");
        }
        else if (key.size() > 2 && key[0] == 'l' && key.find("_name") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.log.size() <= idx) d.log.resize(idx + 1);
            d.log[idx].name = val;
        }
        else if (key.size() > 2 && key[0] == 'l' && key.find("_text") != std::string::npos)
        {
            size_t idx = std::stoul(key.substr(1, key.find('_') - 1));
            if (d.log.size() <= idx) d.log.resize(idx + 1);
            d.log[idx].text = val;
        }
    }
    (void)charCount;
    (void)logCount;
    return d.valid;
}

namespace
{
std::string progressPath()
{
    return "progress.dat";
}

std::set<std::string> progressRead()
{
    std::set<std::string> out;
    std::ifstream in(progressPath());
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty()) out.insert(line);
    }
    return out;
}

void progressWrite(const std::set<std::string>& data)
{
    std::ofstream out(progressPath(), std::ios::trunc);
    for (const auto& s : data) out << s << "\n";
}
}

bool progressIsViewed(const std::string& scriptPath, int chapterId)
{
    return progressRead().count(scriptPath + "|" + std::to_string(chapterId)) != 0;
}

void progressMarkViewed(const std::string& scriptPath, int chapterId)
{
    auto data = progressRead();
    data.insert(scriptPath + "|" + std::to_string(chapterId));
    progressWrite(data);
}

void progressEnsureCreated()
{
    if (!std::filesystem::exists(progressPath())) progressWrite({});
}
