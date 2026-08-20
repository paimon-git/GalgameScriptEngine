#pragma once

#include <string>
#include <vector>

// 基于 ffmpeg 管道的视频播放器（Windows）
// 播放时把 MP4/MOV/WebM 等解码为原始 RGBA 帧，供引擎实时上屏
class VideoPlayer
{
public:
    VideoPlayer() = default;
    ~VideoPlayer() { close(); }

    // 返回可用 ffmpeg 路径：环境变量 QLWT_FFMPEG > PATH > 常见安装位置
    static std::string findFfmpeg();

    // 打开视频并输出为 outW x outH 的 RGBA 帧；成功返回 true
    bool open(const std::string& ffmpegPath, const std::string& file, int outW, int outH);

    // 读取下一帧到 rgba（尺寸必须与 open 一致）；视频结束返回 false
    bool readFrame(std::vector<unsigned char>& rgba);

    void close();

private:
    void* hProcess_ = nullptr;
    void* hReadPipe_ = nullptr;
};
