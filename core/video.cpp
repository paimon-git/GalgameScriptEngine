#include "video.h"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
// 在剪映安装目录里查找自带的 ffmpeg
std::string findInJianying()
{
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("D:/tools/JianyingPro/Apps/*", &fd);
    if (h == INVALID_HANDLE_VALUE) return {};
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        std::string p = "D:/tools/JianyingPro/Apps/" + std::string(fd.cFileName) + "/ffmpeg.exe";
        if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            FindClose(h);
            return p;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#endif
    return {};
}
}

std::string VideoPlayer::findFfmpeg()
{
    const char* env = getenv("QLWT_FFMPEG");
    if (env && *env)
    {
#ifdef _WIN32
        if (GetFileAttributesA(env) != INVALID_FILE_ATTRIBUTES) return env;
#else
        return env;
#endif
    }

#ifdef _WIN32
    // PATH 中的 ffmpeg
    {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        HANDLE r = nullptr, w = nullptr;
        if (CreatePipe(&r, &w, &sa, 0))
        {
            STARTUPINFOA si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = w;
            si.hStdError = w;
            PROCESS_INFORMATION pi{};
            char cmd[] = "ffmpeg -version";
            if (CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &si, &pi))
            {
                TerminateProcess(pi.hProcess, 0);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                CloseHandle(r);
                CloseHandle(w);
                return "ffmpeg";
            }
            CloseHandle(r);
            CloseHandle(w);
        }
    }

    const char* dirs[] = {
        "C:/Program Files/ffmpeg/bin/ffmpeg.exe",
        "C:/ffmpeg/bin/ffmpeg.exe",
        "C:/msys64/usr/bin/ffmpeg.exe",
    };
    for (const char* d : dirs)
        if (GetFileAttributesA(d) != INVALID_FILE_ATTRIBUTES) return d;
#endif

    return findInJianying();
}

bool VideoPlayer::open(const std::string& ffmpegPath, const std::string& file,
                       int outW, int outH)
{
    close();
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE r = nullptr, w = nullptr;
    if (!CreatePipe(&r, &w, &sa, 0)) return false;
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);

    // stderr 丢弃到 NUL，避免管道阻塞
    HANDLE nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, 0, nullptr);

    std::string cmd = "\"" + ffmpegPath + "\" -loglevel error -i \"" + file +
                      "\" -r 30 -s " + std::to_string(outW) + "x" + std::to_string(outH) +
                      " -pix_fmt rgba -f rawvideo -";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = w;
    si.hStdError = nul ? nul : GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(w);
    if (nul) CloseHandle(nul);
    if (!ok)
    {
        CloseHandle(r);
        return false;
    }
    CloseHandle(pi.hThread);
    hProcess_ = pi.hProcess;
    hReadPipe_ = r;
    return true;
#else
    (void)ffmpegPath; (void)file; (void)outW; (void)outH;
    return false;
#endif
}

bool VideoPlayer::readFrame(std::vector<unsigned char>& rgba)
{
#ifdef _WIN32
    if (!hReadPipe_) return false;
    // 管道读取可能返回短数据，循环直到读满整帧
    size_t got = 0;
    while (got < rgba.size())
    {
        DWORD n = 0;
        if (!ReadFile(static_cast<HANDLE>(hReadPipe_), rgba.data() + got,
                      static_cast<DWORD>(rgba.size() - got), &n, nullptr))
            return false;
        if (n == 0) return false;
        got += n;
    }
    return true;
#else
    (void)rgba;
    return false;
#endif
}

void VideoPlayer::close()
{
#ifdef _WIN32
    if (hProcess_)
    {
        TerminateProcess(static_cast<HANDLE>(hProcess_), 0);
        CloseHandle(static_cast<HANDLE>(hProcess_));
        hProcess_ = nullptr;
    }
    if (hReadPipe_)
    {
        CloseHandle(static_cast<HANDLE>(hReadPipe_));
        hReadPipe_ = nullptr;
    }
#endif
}
