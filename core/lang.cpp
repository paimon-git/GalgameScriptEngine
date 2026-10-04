#include "lang.h"

#include <cstdio>
#include <fstream>
#include <map>

#include "file_util.h"

namespace
{
std::map<std::string, std::string> gTexts;

// 去掉首尾空白
std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}
}

namespace lang
{
bool load(const std::string& path)
{
    std::ifstream in(path);
    if (!in) return false;

    gTexts.clear();
    std::string line;
    while (std::getline(in, line))
    {
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
            line.erase(0, 3);   // 兼容带 BOM 的语言文件
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(t.substr(0, eq));
        std::string value = trim(t.substr(eq + 1));
        if (!key.empty()) gTexts[key] = value;
    }
    printf("[lang] loaded '%s' (%zu entries)\n", path.c_str(), gTexts.size());
    return !gTexts.empty();
}

const char* tr(const char* key)
{
    auto it = gTexts.find(key);
    return it == gTexts.end() ? key : it->second.c_str();
}

std::string trf(const char* key, std::initializer_list<std::string> args)
{
    std::string fmt = tr(key);
    std::string out;
    auto it = args.begin();
    for (size_t i = 0; i < fmt.size(); ++i)
    {
        if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == 's' && it != args.end())
        {
            out += *it++;
            ++i;
        }
        else
        {
            out += fmt[i];
        }
    }
    return out;
}

std::vector<std::string> allValues()
{
    std::vector<std::string> out;
    out.reserve(gTexts.size());
    for (const auto& kv : gTexts) out.push_back(kv.second);
    return out;
}
}
