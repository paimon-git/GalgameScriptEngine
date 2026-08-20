#include "gif.h"

#include <cstdio>
#include <cstring>
#include <map>

namespace
{
struct BitReader
{
    const std::vector<unsigned char>& d;
    size_t p = 0;
    int bit = 0;

    explicit BitReader(const std::vector<unsigned char>& dd) : d(dd) {}

    int read(int nbits)
    {
        int v = 0;
        for (int b = 0; b < nbits; ++b)
        {
            if (p >= d.size()) return -1;
            if ((d[p] >> bit) & 1) v |= (1 << b);
            if (++bit == 8) { bit = 0; ++p; }
        }
        return v;
    }
};

// GIF LZW 解码（含 KwKwK 特殊码）
std::vector<unsigned char> lzwDecode(const std::vector<unsigned char>& codes, int mcs)
{
    const int cc = 1 << mcs;
    const int eoi = cc + 1;
    int cs = mcs + 1;
    int nc = eoi + 1;
    std::map<int, std::vector<unsigned char>> dict;
    auto reset = [&]()
    {
        dict.clear();
        for (int i = 0; i < (1 << mcs); ++i) dict[i] = {static_cast<unsigned char>(i)};
        nc = eoi + 1;
        cs = mcs + 1;
    };

    BitReader br(codes);
    reset();
    std::vector<unsigned char> out;
    int prev = -1;
    for (int guard = 0; guard < 10000000; ++guard)
    {
        int code = br.read(cs);
        if (code < 0 || code == eoi) break;
        if (code == cc)
        {
            reset();
            prev = -1;
            continue;
        }
        std::vector<unsigned char> entry;
        if (dict.count(code))
        {
            entry = dict[code];
        }
        else if (prev >= 0 && dict.count(prev))
        {
            entry = dict[prev];
            if (!entry.empty()) entry.push_back(entry[0]);
        }
        else
        {
            break;
        }
        out.insert(out.end(), entry.begin(), entry.end());
        if (prev >= 0 && dict.count(prev))
        {
            std::vector<unsigned char> nw = dict[prev];
            nw.push_back(entry[0]);
            dict[nc++] = nw;
            if (nc == (1 << cs) && cs < 12) ++cs;
        }
        prev = code;
    }
    return out;
}

bool readSubBlocks(const std::vector<unsigned char>& data, size_t& p,
                   std::vector<unsigned char>& out)
{
    out.clear();
    while (p < data.size())
    {
        int n = data[p++];
        if (n == 0) return true;
        if (p + n > data.size()) return false;
        out.insert(out.end(), data.begin() + p, data.begin() + p + n);
        p += n;
    }
    return false;
}
}

bool loadGifFile(const std::string& path, std::vector<GifFrame>& out)
{
    out.clear();

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> data(static_cast<size_t>(sz));
    if (sz > 0) fread(data.data(), 1, static_cast<size_t>(sz), fp);
    fclose(fp);

    if (data.size() < 13) return false;
    if (memcmp(data.data(), "GIF87a", 6) != 0 && memcmp(data.data(), "GIF89a", 6) != 0)
        return false;

    unsigned char packed = data[10];
    std::vector<unsigned char> globalPalette;
    size_t p = 13;
    if (packed & 0x80)
    {
        int n = 2 << (packed & 7);
        if (p + static_cast<size_t>(n) * 3 > data.size()) return false;
        globalPalette.assign(data.begin() + p, data.begin() + p + static_cast<size_t>(n) * 3);
        p += static_cast<size_t>(n) * 3;
    }

    std::vector<unsigned char> activePalette = globalPalette;
    int transparentIndex = -1;
    float delay = 0.1f;

    while (p < data.size())
    {
        unsigned char block = data[p];
        if (block == 0x3B) break;                 // 结束
        if (block == 0x21)                        // 扩展块
        {
            unsigned char label = data[p + 1];
            if (label == 0xF9 && p + 8 <= data.size())
            {
                unsigned char gpacked = data[p + 3];
                transparentIndex = (gpacked & 1) ? data[p + 6] : -1;
                delay = static_cast<float>(data[p + 4] | (data[p + 5] << 8)) / 100.0f;
                if (delay <= 0.0f) delay = 0.1f;
            }
            size_t q = p + 2;
            while (q < data.size())
            {
                int n = data[q++];
                if (n == 0) break;
                q += n;
            }
            p = q;
            continue;
        }
        if (block == 0x2C)                        // 图像块
        {
            size_t q = p + 1;
            if (q + 9 > data.size()) break;
            int iw = data[q + 4] | (data[q + 5] << 8);
            int ih = data[q + 6] | (data[q + 7] << 8);
            unsigned char ipacked = data[q + 8];
            q += 9;
            if (ipacked & 0x80)
            {
                int n = 2 << (ipacked & 7);
                if (q + static_cast<size_t>(n) * 3 > data.size()) break;
                activePalette.assign(data.begin() + q, data.begin() + q + static_cast<size_t>(n) * 3);
                q += static_cast<size_t>(n) * 3;
            }
            else
            {
                activePalette = globalPalette;
            }
            if (q >= data.size()) break;
            int mcs = data[q++];
            std::vector<unsigned char> lzw;
            if (!readSubBlocks(data, q, lzw)) break;
            p = q;

            std::vector<unsigned char> idx = lzwDecode(lzw, mcs);
            if (static_cast<int>(idx.size()) < iw * ih) break;

            GifFrame f;
            f.width = iw;
            f.height = ih;
            f.delay = delay;
            f.rgba.resize(static_cast<size_t>(iw) * ih * 4);
            for (int i = 0; i < iw * ih; ++i)
            {
                int c = idx[i];
                if (c == transparentIndex)
                {
                    f.rgba[i * 4 + 3] = 0;
                    continue;
                }
                if (c >= 0 && c * 3 + 2 < static_cast<int>(activePalette.size()))
                {
                    f.rgba[i * 4] = activePalette[c * 3];
                    f.rgba[i * 4 + 1] = activePalette[c * 3 + 1];
                    f.rgba[i * 4 + 2] = activePalette[c * 3 + 2];
                }
                f.rgba[i * 4 + 3] = 255;
            }
            out.push_back(std::move(f));
            continue;
        }
        ++p;
    }
    return !out.empty();
}
