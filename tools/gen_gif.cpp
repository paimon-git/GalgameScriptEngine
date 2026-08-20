// 生成测试用动画 GIF（星空极光），用于验证引擎的视频 CG 支持
// 编译: g++ tools/gen_gif.cpp -o tools/gen_gif.exe
// 运行: tools/gen_gif.exe assets/cg/sky.gif

#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
struct BitWriter
{
    std::vector<uint8_t> bytes;
    int bitPos = 0;

    void write(int code, int nbits)
    {
        for (int b = 0; b < nbits; ++b)
        {
            if (bitPos == 0) bytes.push_back(0);
            if ((code >> b) & 1) bytes.back() |= static_cast<uint8_t>(1 << bitPos);
            bitPos = (bitPos + 1) & 7;
        }
    }
};

// 标准 GIF LZW 压缩
std::vector<uint8_t> lzwCompress(const std::vector<uint8_t>& data, int minCodeSize, int variant)
{
    const int clearCode = 1 << minCodeSize;
    const int eoiCode = clearCode + 1;
    int codeSize = minCodeSize + 1;
    int nextCode = eoiCode + 1;

    std::map<uint32_t, int> dict;
    auto reset = [&]()
    {
        dict.clear();
        for (int i = 0; i < (1 << minCodeSize); ++i) dict[i] = i;
        nextCode = eoiCode + 1;
        codeSize = minCodeSize + 1;
    };

    BitWriter bw;
    reset();
    bw.write(clearCode, codeSize);

    if (data.empty())
    {
        bw.write(clearCode, codeSize);
        bw.write(eoiCode, codeSize);
        return bw.bytes;
    }

    int prefix = data[0];
    for (size_t i = 1; i < data.size(); ++i)
    {
        int c = data[i];
        // 前缀 +1 偏移，避免与字面量码（0..255）的键冲突
        uint32_t key = ((static_cast<uint32_t>(prefix) + 1) << 8) | static_cast<uint32_t>(c);
        auto it = dict.find(key);
        if (it != dict.end())
        {
            prefix = it->second;
        }
        else
        {
            bw.write(prefix, codeSize);
            if (nextCode < 4096)
            {
                dict[key] = nextCode++;
                // GIF 规范：编码器比解码器晚一个码切换码宽
                if (variant == 0 && nextCode == (1 << codeSize) && codeSize < 12) ++codeSize;
                else if (variant == 1 && nextCode == (1 << codeSize) - 1 && codeSize < 12) ++codeSize;
                else if (variant == 2 && nextCode == (1 << codeSize) + 1 && codeSize < 12) ++codeSize;
            }
            else
            {
                bw.write(clearCode, codeSize);
                reset();
            }
            prefix = c;
        }
    }
    bw.write(prefix, codeSize);
    bw.write(eoiCode, codeSize);
    return bw.bytes;
}

// 3-3-2 RGB 量化，固定 256 色调色板
int colorIndex(uint8_t r, uint8_t g, uint8_t b)
{
    return (r >> 5) << 5 | (g >> 5) << 2 | (b >> 6);
}

void paletteColor(int idx, uint8_t& r, uint8_t& g, uint8_t& b)
{
    r = static_cast<uint8_t>(((idx >> 5) & 7) * 36 + 18);
    g = static_cast<uint8_t>(((idx >> 2) & 7) * 36 + 18);
    b = static_cast<uint8_t>((idx & 3) * 85 + 21);
}

uint8_t clamp8(int v)
{
    return v < 0 ? 0 : (v > 255 ? 255 : static_cast<uint8_t>(v));
}
}

int main(int argc, char** argv)
{
    int W = 640;
    int H = 360;
    int FRAMES = 20;
    int VARIANT = 2;
    if (argc > 1) W = std::atoi(argv[1]);
    if (argc > 2) H = std::atoi(argv[2]);
    if (argc > 3) FRAMES = std::atoi(argv[3]);
    if (argc > 4) VARIANT = std::atoi(argv[4]);
    const char* outPath = argc > 5 ? argv[5] : "assets/cg/sky.gif";
    const int DELAY_CS = 10;   // 每帧 0.1 秒

    std::vector<uint8_t> gct(256 * 3);
    for (int i = 0; i < 256; ++i)
    {
        uint8_t r, g, b;
        paletteColor(i, r, g, b);
        gct[i * 3 + 0] = r;
        gct[i * 3 + 1] = g;
        gct[i * 3 + 2] = b;
    }

    // 星星（确定性随机）
    struct Star { int x, y; float phase; };
    std::vector<Star> stars;
    unsigned seed = 7;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 16) & 0x7FFF; };
    for (int i = 0; i < 260; ++i)
    {
        stars.push_back({static_cast<int>(rnd() % W),
                         static_cast<int>(rnd() % 240),
                         static_cast<float>(rnd() % 628) / 100.0f});
    }

    std::vector<std::vector<uint8_t>> frames;
    for (int f = 0; f < FRAMES; ++f)
    {
        float t = static_cast<float>(f) / FRAMES;
        std::vector<uint8_t> px(static_cast<size_t>(W) * H);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                // 背景渐变
                float g0 = static_cast<float>(y) / H;
                int r = 7 + static_cast<int>(g0 * 14);
                int g = 11 + static_cast<int>(g0 * 20);
                int b = 32 + static_cast<int>(g0 * 30);

                // 极光带
                for (int band = 0; band < 2; ++band)
                {
                    float cy = 110.0f + 55.0f * std::sin(x * 0.011f + t * 6.28f * (band ? 1.3f : 0.8f) + band * 2.4f);
                    float dy = y - cy;
                    float w = 26.0f + 8.0f * std::sin(x * 0.02f + t * 4.0f);
                    float k = std::exp(-(dy * dy) / (2.0f * w * w));
                    if (band == 0) { r += static_cast<int>(60 * k); g += static_cast<int>(180 * k); b += static_cast<int>(120 * k); }
                    else { r += static_cast<int>(110 * k); g += static_cast<int>(60 * k); b += static_cast<int>(190 * k); }
                }

                // 星星闪烁
                for (const auto& s : stars)
                {
                    if (x == s.x && y == s.y)
                    {
                        float tw = 0.5f + 0.5f * std::sin(t * 6.28f * 2.0f + s.phase);
                        int br = static_cast<int>(140 + 100 * tw);
                        r += br; g += br; b += br;
                    }
                }

                px[y * W + x] = static_cast<uint8_t>(colorIndex(clamp8(r), clamp8(g), clamp8(b)));
            }
        }
        frames.push_back(px);
    }

    // 组装 GIF 文件
    std::vector<uint8_t> out;
    const char* header = "GIF89a";
    out.insert(out.end(), header, header + 6);
    out.push_back(static_cast<uint8_t>(W & 0xFF));
    out.push_back(static_cast<uint8_t>((W >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(H & 0xFF));
    out.push_back(static_cast<uint8_t>((H >> 8) & 0xFF));
    out.push_back(0xF7);   // GCT 存在，颜色分辨率 7，GCT 大小 7（256 色）
    out.push_back(0);
    out.push_back(0);
    out.insert(out.end(), gct.begin(), gct.end());

    for (int f = 0; f < FRAMES; ++f)
    {
        // 图形控制扩展
        out.push_back(0x21); out.push_back(0xF9); out.push_back(0x04);
        out.push_back(0x04);   // 不释放，无透明
        out.push_back(static_cast<uint8_t>(DELAY_CS & 0xFF));
        out.push_back(static_cast<uint8_t>((DELAY_CS >> 8) & 0xFF));
        out.push_back(0);
        out.push_back(0);
        // 图像描述
        out.push_back(0x2C);
        out.push_back(0); out.push_back(0); out.push_back(0); out.push_back(0);
        out.push_back(static_cast<uint8_t>(W & 0xFF));
        out.push_back(static_cast<uint8_t>((W >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(H & 0xFF));
        out.push_back(static_cast<uint8_t>((H >> 8) & 0xFF));
        out.push_back(0);
        // LZW
        out.push_back(8);
        std::vector<uint8_t> lzw = lzwCompress(frames[f], 8, VARIANT);
        size_t i = 0;
        while (i < lzw.size())
        {
            size_t n = lzw.size() - i;
            if (n > 255) n = 255;
            out.push_back(static_cast<uint8_t>(n));
            out.insert(out.end(), lzw.begin() + i, lzw.begin() + i + n);
            i += n;
        }
        out.push_back(0);
    }
    out.push_back(0x3B);

    const char* path = outPath;
    FILE* fp = fopen(path, "wb");
    if (!fp) { printf("cannot write %s\n", path); return 1; }
    fwrite(out.data(), 1, out.size(), fp);
    fclose(fp);
    printf("wrote %s (%zu bytes, %d frames %dx%d)\n", path, out.size(), FRAMES, W, H);
    return 0;
}
