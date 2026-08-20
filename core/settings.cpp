#include "settings.h"

#include <cstdlib>
#include <fstream>

void Settings::load(const std::string& path)
{
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    while (std::getline(in, line))
    {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        int value = std::atoi(line.c_str() + eq + 1);
        if (key == "textSpeed") textSpeed = clampSpeed(value);
        else if (key == "bgmVolume") bgmVolume = clampVol(value);
        else if (key == "sfxVolume") sfxVolume = clampVol(value);
        else if (key == "fullscreen") fullscreen = (value != 0);
    }
}

void Settings::save(const std::string& path) const
{
    std::ofstream out(path);
    if (!out) return;
    out << "textSpeed=" << textSpeed << "\n";
    out << "bgmVolume=" << bgmVolume << "\n";
    out << "sfxVolume=" << sfxVolume << "\n";
    out << "fullscreen=" << (fullscreen ? 1 : 0) << "\n";
}
