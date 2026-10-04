#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 .ttc / .otc 字体集合里取出一个子字体，另存为独立的 .ttf / .otf。

为什么需要它：raylib 的 LoadFontEx 只按扩展名判断（.ttf / .otf / .fnt），
遇到 .ttc 会直接放弃、退回默认字体，中文就显示不出来。而 Linux 上的中文字体
基本都是 .ttc，所以先抽取一次，把结果放进 assets/fonts/ 里。

用法:
    python3 tools/ttc2ttf.py <输入字体> <输出文件> [字面序号或名称关键字]

例子:
    python3 tools/ttc2ttf.py /usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc \\
        assets/fonts/NotoSansSC-Regular.ttf SC

第三个参数给关键字（如 SC / 简体 / Sans）时按名称匹配字面；给数字时按序号取。
默认取第 0 个字面。输入本身就是 .ttf/.otf 时直接复制。
"""

import os
import struct
import sys


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def read_faces(data):
    """返回 [(offset, sfnt_version), ...]；非集合字体返回单个 0 偏移。"""
    if data[:4] == b"ttcf":
        num = u32(data, 8)
        return [(u32(data, 12 + 4 * i), 0x00010000) for i in range(num)]
    return [(0, u32(data, 0))]


def face_name(data, offset):
    """读 name 表里的 full name（nameID 4），失败时退回 family（1）。"""
    num_tables = u16(data, offset + 4)
    name_off = None
    for i in range(num_tables):
        rec = offset + 12 + 16 * i
        if data[rec:rec + 4] == b"name":
            name_off = u32(data, rec + 8)
            break
    if name_off is None:
        return ""

    count = u16(data, name_off + 2)
    str_off = name_off + u16(data, name_off + 4)
    best = ""
    for i in range(count):
        rec = name_off + 6 + 12 * i
        platform, encoding, language, name_id = (
            u16(data, rec), u16(data, rec + 2), u16(data, rec + 4), u16(data, rec + 6))
        length, offset = u16(data, rec + 8), u16(data, rec + 10)
        if name_id not in (4, 1):
            continue
        raw = data[str_off + offset:str_off + offset + length]
        if platform == 3:                      # Windows：UTF-16BE
            text = raw.decode("utf-16-be", "ignore")
        elif platform == 1:                    # Mac：单字节
            text = raw.decode("latin-1", "ignore")
        else:
            continue
        if name_id == 4:                       # full name 优先
            return text.strip()
        if not best:
            best = text.strip()
    return best


def extract(data, face_offset):
    """把某个字面的表目录复制成独立字体（表数据原样搬运，重新计算偏移）。"""
    num_tables = u16(data, face_offset + 4)
    search_range = u16(data, face_offset + 6)
    entry_selector = u16(data, face_offset + 8)
    range_shift = u16(data, face_offset + 10)

    tables = []
    for i in range(num_tables):
        rec = face_offset + 12 + 16 * i
        tag = data[rec:rec + 4]
        checksum = u32(data, rec + 4)
        offset = u32(data, rec + 8)
        length = u32(data, rec + 12)
        tables.append([tag, checksum, offset, length])

    # 保持与源文件相同的表顺序（head 必须排在前面，字体校验会用到）
    head = offset = 12 + 16 * num_tables
    if head % 4:
        offset += 4 - head % 4

    out = bytearray()
    out += data[face_offset:face_offset + 4]           # sfntVersion
    out += struct.pack(">HHHH", num_tables, search_range, entry_selector, range_shift)

    body = bytearray()
    head_pos = None
    for tag, checksum, src_off, length in tables:
        while offset % 4:
            body += b"\0"
            offset += 1
        if tag == b"head":
            head_pos = offset + 8                      # checkSumAdjustment 字段位置
        out += struct.pack(">4sIII", tag, checksum, offset, length)
        body += data[src_off:src_off + length]
        offset += length

    out += body
    if head_pos is not None:
        # 按规范补上 checkSumAdjustment（stb_truetype 不校验，但别的工具会看）
        struct.pack_into(">I", out, head_pos, 0)
        total = 0
        for i in range(0, len(out) - 3, 4):
            total = (total + u32(out, i)) & 0xFFFFFFFF
        tail = len(out) % 4
        if tail:
            total = (total + u32(out + b"\0" * (4 - tail), len(out) - tail)) & 0xFFFFFFFF
        struct.pack_into(">I", out, head_pos, (0xB1B0AFBA - total) & 0xFFFFFFFF)
    return bytes(out)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1

    src, dst = sys.argv[1], sys.argv[2]
    want = sys.argv[3] if len(sys.argv) > 3 else None

    data = open(src, "rb").read()
    faces = read_faces(data)

    if len(faces) == 1 and not want:
        out = data                                    # 本来就是单体字体
        chosen = 0
    else:
        chosen = None
        if want and want.isdigit():
            idx = int(want)
            if idx < len(faces):
                chosen = idx
        elif want:
            for i, (off, _) in enumerate(faces):
                name = face_name(data, off)
                if want.lower() in name.lower():
                    chosen = i
                    break
        if chosen is None:
            chosen = 0
        out = extract(data, faces[chosen][0])

    os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
    with open(dst, "wb") as f:
        f.write(out)

    name = face_name(data, faces[chosen][0]) if len(faces) > 1 else ""
    print(f"{src} -> {dst}  (face {chosen}{' ' + name if name else ''}, "
          f"{len(faces)} face(s), {len(out)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
