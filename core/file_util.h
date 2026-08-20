#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// 以 UTF-8 路径读取文件内容。
// MinGW 的 std::ifstream 用 ANSI 码页打开窄字符串路径，含中文的脚本文件名会打不开；
// 这里先把 UTF-8 转成 std::filesystem::path（内部为宽字符），再用流打开。
inline std::string readFileUtf8(const std::string& path)
{
#if defined(__cpp_char8_t)
    std::filesystem::path p(
        reinterpret_cast<const char8_t*>(path.data()));
#else
    std::filesystem::path p = std::filesystem::u8path(path);
#endif
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
