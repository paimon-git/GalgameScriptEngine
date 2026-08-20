#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("assets/scripts", ec))
    {
        if (!entry.is_regular_file(ec)) continue;
        std::string p = entry.path().string();
        std::string stem = entry.path().stem().string();
        std::cout << "file: '" << p << "' stem: '" << stem << "'\n";
        std::cout << "  path u8: '";
        for (unsigned char c : p) std::cout << std::hex << (int)c << ' ';
        std::cout << "'\n";
        std::ifstream in(p);
        std::cout << "  open: " << (in ? "OK" : "FAIL") << "\n";
        std::cout << "  ends .gal: " << (p.size() >= 4 && p.substr(p.size() - 4) == ".gal") << "\n";
    }
    std::cout << "error: " << ec.message() << "\n";
    return 0;
}
