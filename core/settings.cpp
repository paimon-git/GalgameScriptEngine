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
        std::string raw = line.substr(eq + 1);
        int value = std::atoi(raw.c_str());
        if (key == "textSpeed") textSpeed = clampSpeed(value);
        else if (key == "bgmVolume") bgmVolume = clampVol(value);
        else if (key == "sfxVolume") sfxVolume = clampVol(value);
        else if (key == "fullscreen") fullscreen = (value != 0);
        else if (key == "uiAccent") uiAccent = static_cast<unsigned int>(std::strtoul(raw.c_str(), nullptr, 16)) & 0xFFFFFFu;
        else if (key == "uiCorner") uiCorner = clampCorner(value);
        else if (key == "uiButtonStyle") uiButtonStyle = clampStyle(value);
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
    out << "uiAccent=" << std::hex << (uiAccent & 0xFFFFFFu) << std::dec << "\n";
    out << "uiCorner=" << uiCorner << "\n";
    out << "uiButtonStyle=" << uiButtonStyle << "\n";
}
