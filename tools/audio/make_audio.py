#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
用程序合成整套 BGM 与音效（不引用任何第三方素材，全是自己算出来的波形）。

    BGM  6 首 → assets/audio/bgm_*.ogg（44.1kHz 立体声，Vorbis，可循环）
    SE   4 个 → assets/audio/se_bell / se_door / se_thunder / se_meteor (.wav)

依赖：numpy（脚本里只用 numpy），OGG 编码交给系统 ffmpeg 的 libvorbis。

用法：
    python3 tools/audio/make_audio.py            # 全做
    python3 tools/audio/make_audio.py bgm        # 只做 BGM
    python3 tools/audio/make_audio.py se         # 只做音效
    python3 tools/audio/make_audio.py daily      # 只做某一条

设计说明（音乐都是"和声进行 + 琶音 + 铺底 + 打击"这几层叠出来的）：
    * 每首都以完整小节数结尾，首尾各有 0.6s 淡入淡出，循环播放不会"啪"一下
    * 弦乐铺底用多路失谐正弦叠加，钢琴/电钢用加法合成 + 指数包络
    * 混响是自己生成的脉冲响应做 FFT 卷积，量很小但足够把声音铺开
"""

import os
import subprocess
import sys

import numpy as np

SR = 44100
PROJ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AUDIO = os.path.join(PROJ, "assets", "audio")


# ---------------------------------------------------------------- 基础工具
def t_of(n):
    return np.arange(n) / SR


def fft_filter(x, lo=None, hi=None):
    """频域平滑滤波：lo/hi 是 -6dB 大致位置（Hz）。"""
    if lo is None and hi is None:
        return x
    n = len(x)
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    gain = np.ones_like(f)
    if hi:
        gain *= 1.0 / (1.0 + (f / hi) ** 4)
    if lo:
        gain *= (f / lo) ** 4 / (1.0 + (f / lo) ** 4)
    return np.fft.irfft(spec * gain, n)


def fft_convolve(x, h):
    n = len(x) + len(h) - 1
    N = 1 << (n - 1).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(h, N), N)[:n]
    return y


def impulse_response(decay=1.6, damp=4200.0, seed=7):
    """人造混响脉冲：指数衰减的噪声，低通后高频衰减更快。"""
    rng = np.random.default_rng(seed)
    n = int(decay * SR)
    t = t_of(n)
    h = rng.standard_normal(n) * np.exp(-3.2 * t)
    h = fft_filter(h, hi=damp)
    h[:200] *= np.linspace(0.0, 1.0, 200)      # 去掉最开头的爆音
    return h / np.abs(h).max()


def reverb(x, mix=0.3, decay=1.6, damp=4200.0, seed=7):
    wet = fft_convolve(x, impulse_response(decay, damp, seed))
    wet = wet[: len(x)]
    wet /= max(1e-9, np.abs(wet).max())
    return (1.0 - mix) * x + mix * wet * np.abs(x).max()


def adsr(n, a=0.01, d=0.15, s=0.7, r=0.4):
    t = t_of(n)
    dur = n / SR
    env = np.ones(n)
    env *= np.clip(t / max(a, 1e-4), 0.0, 1.0)
    sus = np.clip(1.0 - (t - a) / max(d, 1e-4), s, 1.0)
    env *= np.where(t > a, sus, 1.0)
    rel = np.clip((dur - t) / max(r, 1e-4), 0.0, 1.0)
    return env * rel


def add(buf, x, at):
    """把 x 叠加到 buf 的 at 秒处（越界自动裁剪）。"""
    i = int(at * SR)
    if i >= len(buf):
        return
    n = min(len(x), len(buf) - i)
    if n > 0:
        buf[i:i + n] += x[:n]


# ---------------------------------------------------------------- 音色
def epiano(f, dur, amp=0.22):
    """电钢：FM 调制 + 少量泛音，尾巴干净。"""
    n = int(dur * SR)
    t = t_of(n)
    mod = np.sin(2 * np.pi * f * 2.0 * t) * 1.7 * np.exp(-t * 6.0)
    x = np.sin(2 * np.pi * f * t + mod)
    x += 0.28 * np.sin(2 * np.pi * f * 2.0 * t) * np.exp(-t * 7.0)
    x += 0.12 * np.sin(2 * np.pi * f * 3.0 * t) * np.exp(-t * 9.0)
    return amp * x * adsr(n, 0.006, 0.5, 0.45, 0.5)


def piano(f, dur, amp=0.2):
    n = int(dur * SR)
    t = t_of(n)
    x = np.sin(2 * np.pi * f * t) * np.exp(-t * 2.0)
    x += 0.45 * np.sin(2 * np.pi * 2 * f * t) * np.exp(-t * 3.6)
    x += 0.22 * np.sin(2 * np.pi * 3 * f * t) * np.exp(-t * 5.5)
    x += 0.10 * np.sin(2 * np.pi * 4.02 * f * t) * np.exp(-t * 7.0)
    return amp * x * adsr(n, 0.004, 0.8, 0.3, 0.6)


def pad(f, dur, amp=0.11, detune=0.0035, seed=0):
    """铺底：五路轻微失谐的正弦，慢起慢落。"""
    n = int(dur * SR)
    t = t_of(n)
    x = np.zeros(n)
    for k in (-2, -1, 0, 1, 2):
        x += np.sin(2 * np.pi * f * (1.0 + k * detune) * t + k * 0.9)
    x /= 5.0
    x = fft_filter(x, hi=2600)
    return amp * x * adsr(n, 1.2, 1.0, 0.85, 1.4)


def pluck(f, dur, amp=0.16):
    """拨弦感：三角波 + 快速衰减。"""
    n = int(dur * SR)
    t = t_of(n)
    x = 0.0
    for h, a in ((1, 1.0), (2, 0.33), (3, 0.16), (4, 0.08)):
        x = x + a * np.sin(2 * np.pi * h * f * t)
    return amp * x * adsr(n, 0.003, 0.25, 0.22, 0.35)


def strings(f, dur, amp=0.09, detune=0.006):
    n = int(dur * SR)
    t = t_of(n)
    x = np.zeros(n)
    for k in (-3, -1, 1, 3):
        x += np.sin(2 * np.pi * f * (1 + k * detune) * t)
    x /= 4
    vib = 1.0 + 0.002 * np.sin(2 * np.pi * 4.6 * t)
    x = fft_filter(x, hi=3200) * vib
    return amp * x * adsr(n, 0.6, 0.8, 0.8, 1.0)


def bell_tone(f, dur, amp=0.3):
    """钟：非谐分音，衰减长。"""
    n = int(dur * SR)
    t = t_of(n)
    x = np.zeros(n)
    for r, a, d in ((1.0, 1.0, 1.4), (2.01, 0.55, 2.0), (2.99, 0.32, 2.6),
                    (4.17, 0.2, 3.2), (5.43, 0.12, 4.0), (6.79, 0.07, 4.6)):
        x += a * np.sin(2 * np.pi * f * r * t) * np.exp(-t * d)
    return amp * x


# ---------------------------------------------------------------- 打击
def kick(amp=0.5, dur=0.45):
    n = int(dur * SR)
    t = t_of(n)
    f = 118 * np.exp(-t * 22) + 44
    x = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 7.0)
    return amp * x


def taiko(amp=0.45, dur=0.6):
    n = int(dur * SR)
    t = t_of(n)
    f = 90 * np.exp(-t * 14) + 58
    body = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 5.0)
    rng = np.random.default_rng(3)
    skin = fft_filter(rng.standard_normal(n), lo=180, hi=2600) * np.exp(-t * 18)
    return amp * (body + 0.35 * skin / max(1e-9, np.abs(skin).max()))


def snare(amp=0.22, dur=0.3, seed=5):
    rng = np.random.default_rng(seed)
    n = int(dur * SR)
    t = t_of(n)
    x = fft_filter(rng.standard_normal(n), lo=260, hi=7000) * np.exp(-t * 14)
    x += 0.3 * np.sin(2 * np.pi * 190 * t) * np.exp(-t * 20)
    return amp * x / max(1e-9, np.abs(x).max())


def shaker(amp=0.09, dur=0.12, seed=9):
    rng = np.random.default_rng(seed)
    n = int(dur * SR)
    t = t_of(n)
    x = fft_filter(rng.standard_normal(n), lo=3800, hi=12000) * np.exp(-t * 26)
    return amp * x / max(1e-9, np.abs(x).max())


def noise_bed(dur, amp=0.2, lo=None, hi=1200, seed=11, mod=None):
    rng = np.random.default_rng(seed)
    n = int(dur * SR)
    x = fft_filter(rng.standard_normal(n), lo=lo, hi=hi)
    x /= max(1e-9, np.abs(x).max())
    if mod is not None:
        x = x * mod(n)
    return amp * x


# ---------------------------------------------------------------- 音符名
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6,
        "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def hz(name, octave=4):
    return 440.0 * 2.0 ** ((NOTE[name] + (octave - 4) * 12 - 9) / 12.0)


def chord(names, octave=4):
    return [hz(n, octave) for n in names]


# ---------------------------------------------------------------- 六首 BGM
def bgm_daily():
    """平淡的日常，轻微怀旧：C - Am - F - G，电钢琶音。"""
    bpm, bars = 72, 16
    beat = 60.0 / bpm
    dur = bars * 4 * beat
    buf = np.zeros(int(dur * SR) + SR)
    prog = [["C", "E", "G"], ["A", "C", "E"], ["F", "A", "C"], ["G", "B", "D"]]
    for b in range(bars):
        at = b * 4 * beat
        ch = prog[b % 4]
        for f in chord(ch, 4):
            add(buf, pad(f, 4 * beat, 0.075, seed=b), at)
            add(buf, pad(f / 2, 4 * beat, 0.05, seed=b + 100), at)
        # 琶音：每小节八分音符
        seq = ch + [ch[1]]
        for i in range(8):
            f = hz(seq[i % len(seq)], 5 if i % 2 else 4)
            add(buf, epiano(f, 1.1 * beat, 0.15), at + i * beat * 0.5)
        if b % 4 == 3:
            add(buf, pluck(hz(ch[0], 5), 1.5, 0.10), at + 3 * beat)
    return buf[: int(dur * SR)]


def bgm_tension():
    """悬着心、对峙、倒计时：A 小调，低音脉冲 + 秒针。"""
    bpm, bars = 92, 16
    beat = 60.0 / bpm
    dur = bars * 4 * beat
    buf = np.zeros(int(dur * SR) + SR)
    prog = [["A", "C", "E"], ["A", "C", "E"], ["F", "A", "C"], ["G#", "B", "D"]]
    for b in range(bars):
        at = b * 4 * beat
        ch = prog[b % 4]
        add(buf, pad(hz(ch[0], 2), 4 * beat, 0.17, seed=b), at)
        add(buf, strings(hz(ch[0], 3), 4 * beat, 0.07), at)
        for i in range(4):
            if i == 0 or b % 2 == 1:
                add(buf, kick(0.42 if i == 0 else 0.3), at + i * beat)
        # 秒针：每拍前半拍一声，越到后段越密
        ticks = 4 if b < 8 else 8
        for i in range(ticks):
            add(buf, shaker(0.075, 0.09), at + i * (4 * beat / ticks))
        if b >= 8:
            add(buf, strings(hz(ch[1], 4) * 1.0, 2 * beat, 0.05), at + 2 * beat)
    return buf[: int(dur * SR)]


def bgm_rain():
    """压抑、湿冷：雨声床 + 低音铺底 + 稀疏钢琴。"""
    dur, beat = 58.0, 0.62
    buf = np.zeros(int(dur * SR) + SR)
    rng = np.random.default_rng(21)
    n = int(dur * SR)
    # 雨：两层噪声，一层高频沙沙，一层低频闷响，缓慢起伏
    hiss = fft_filter(rng.standard_normal(n), lo=900, hi=9000)
    hiss /= np.abs(hiss).max()
    rumble = fft_filter(rng.standard_normal(n), lo=60, hi=420)
    rumble /= np.abs(rumble).max()
    swell = 0.75 + 0.25 * np.sin(2 * np.pi * np.arange(n) / SR / 11.0)
    buf[:n] += 0.16 * hiss * swell + 0.13 * rumble
    # 偶尔的雨滴
    for k in range(90):
        at = rng.uniform(0, dur - 0.3)
        f = rng.uniform(700, 2200)
        m = int(0.09 * SR)
        drop = np.sin(2 * np.pi * f * t_of(m)) * np.exp(-t_of(m) * 40)
        add(buf, drop * 0.05, at)
    # 铺底 + 钢琴
    prog = [["D", "F", "A"], ["B", "D", "F"], ["G", "A#", "D"], ["A", "C", "E"]]
    bar = 4 * beat
    bars = int(dur / bar)
    for b in range(bars):
        at = b * bar
        ch = prog[b % 4]
        add(buf, pad(hz(ch[0], 2), bar, 0.11, seed=b + 40), at)
        add(buf, pad(hz(ch[2], 3), bar, 0.05, seed=b + 80), at)
        if b % 2 == 0:
            add(buf, piano(hz(ch[0], 4), 2.4, 0.11), at + beat * 0.5)
            add(buf, piano(hz(ch[2], 4), 2.0, 0.08), at + beat * 2.5)
    return buf[:n]


def bgm_festival():
    """祭典喧闹、期待与紧张：D 五声，太鼓 + 快琶音。"""
    bpm, bars = 126, 24
    beat = 60.0 / bpm
    dur = bars * 4 * beat
    buf = np.zeros(int(dur * SR) + SR)
    scale = ["D", "E", "F#", "A", "B"]
    prog = [["D", "F#", "A"], ["B", "D", "F#"], ["G", "B", "D"], ["A", "C#", "E"]]
    for b in range(bars):
        at = b * 4 * beat
        ch = prog[b % 4]
        add(buf, pad(hz(ch[0], 2), 4 * beat, 0.09, seed=b), at)
        for i in range(4):
            add(buf, taiko(0.5 if i in (0, 2) else 0.34), at + i * beat)
            add(buf, shaker(0.1, 0.1), at + i * beat + beat * 0.5)
        if b % 4 == 3:
            add(buf, snare(0.2), at + 3 * beat)
            add(buf, snare(0.26), at + 3.5 * beat)
        # 快速琶音
        for i in range(16):
            f = hz(scale[(i * 3 + b) % len(scale)], 5)
            add(buf, pluck(f, 0.5, 0.085), at + i * beat * 0.25)
        # 旋律
        mel = [0, 2, 3, 2, 4, 3, 2, 0]
        if b % 4 in (1, 3):
            for i, mi in enumerate(mel):
                add(buf, epiano(hz(scale[mi % len(scale)], 5), 0.7, 0.12),
                    at + i * beat * 0.5)
    return buf[: int(dur * SR)]


def bgm_starry():
    """温柔、辽阔、释然：F 大调长铺底 + 高音闪烁。"""
    dur, beat = 62.0, 0.86
    bar = 4 * beat
    buf = np.zeros(int(dur * SR) + SR)
    prog = [["F", "A", "C"], ["D", "F", "A"], ["A#", "D", "F"], ["C", "E", "G"]]
    bars = int(dur / bar)
    for b in range(bars):
        at = b * bar
        ch = prog[b % 4]
        for f in chord(ch, 3):
            add(buf, pad(f, bar * 1.05, 0.07, seed=b), at)
        add(buf, pad(hz(ch[0], 2), bar * 1.05, 0.09, seed=b + 50), at)
        add(buf, strings(hz(ch[2], 4), bar, 0.045), at + bar * 0.25)
        # 高音闪烁
        for i in range(3):
            if (b + i) % 3 != 2:
                f = hz(ch[(i + 1) % 3], 5)
                add(buf, pluck(f, 2.6, 0.055), at + bar * (0.25 + 0.25 * i))
    return buf[: int(dur * SR)]


def bgm_warm():
    """尾声，安心与一点点心动：Cmaj7 - Am7 - Fmaj7 - G，钢琴旋律。"""
    bpm, bars = 76, 16
    beat = 60.0 / bpm
    dur = bars * 4 * beat
    buf = np.zeros(int(dur * SR) + SR)
    prog = [["C", "E", "G", "B"], ["A", "C", "E", "G"],
            ["F", "A", "C", "E"], ["G", "B", "D", "F"]]
    mel = [[5, 4, 2, 4], [4, 2, 0, 2], [0, 2, 4, 5], [4, 2, 1, 2]]
    for b in range(bars):
        at = b * 4 * beat
        ch = prog[b % 4]
        for f in chord(ch, 4):
            add(buf, pad(f, 4 * beat, 0.055, seed=b), at)
        add(buf, pad(hz(ch[0], 3), 4 * beat, 0.07, seed=b + 30), at)
        add(buf, piano(hz(ch[0], 3), 2.6, 0.11), at + beat * 0.0)
        add(buf, piano(hz(ch[2], 4), 2.2, 0.08), at + beat * 2.0)
        # 旋律（八分）
        for i, deg in enumerate(mel[b % 4]):
            scale = ["C", "D", "E", "F", "G", "A", "B"]
            f = hz(scale[deg % 7], 5)
            add(buf, epiano(f, 1.0, 0.10), at + beat * (0.5 + i * 0.75))
    return buf[: int(dur * SR)]


BGM = {
    "bgm_daily": bgm_daily,
    "bgm_tension": bgm_tension,
    "bgm_rain": bgm_rain,
    "bgm_festival": bgm_festival,
    "bgm_starry": bgm_starry,
    "bgm_warm": bgm_warm,
}

# 每首的混响参数（太干会像电子琴，太湿会糊）
BGM_VERB = {
    "bgm_daily": (0.26, 1.5, 5000),
    "bgm_tension": (0.20, 1.2, 4200),
    "bgm_rain": (0.30, 1.8, 3600),
    "bgm_festival": (0.22, 1.3, 4600),
    "bgm_starry": (0.42, 2.6, 3400),
    "bgm_warm": (0.30, 1.8, 4200),
}


# ---------------------------------------------------------------- 四个音效
def se_bell():
    """走廊尽头的钟，敲了七下。"""
    dur = 7.4
    buf = np.zeros(int(dur * SR) + SR)
    for i in range(7):
        add(buf, bell_tone(392.0, 2.8, 0.34), i * 0.95)
        add(buf, bell_tone(392.0 * 2.0, 1.6, 0.10), i * 0.95 + 0.004)
    return buf[: int(dur * SR)]


def se_door():
    """推开铁门：门轴吱呀 + 门框闷响。"""
    n = int(1.7 * SR)
    buf = np.zeros(n)
    rng = np.random.default_rng(4)
    t = t_of(n)
    creak_f = 240 * np.exp(-t * 0.7) + 70 + 26 * np.sin(2 * np.pi * 7.3 * t)
    creak = np.sin(2 * np.pi * np.cumsum(creak_f) / SR)
    creak *= (0.35 + 0.65 * np.abs(np.sin(2 * np.pi * 2.1 * t)))
    creak = fft_filter(creak, lo=120, hi=3000) * np.exp(-t * 1.5)
    thud = fft_filter(rng.standard_normal(n), lo=40, hi=300) * np.exp(-t * 9)
    latch = np.zeros(n)
    m = int(0.06 * SR)
    latch[:m] = fft_filter(rng.standard_normal(m), lo=1500, hi=6000) * np.exp(-t_of(m) * 60)
    buf += 0.30 * creak / max(1e-9, np.abs(creak).max())
    buf += 0.42 * thud / max(1e-9, np.abs(thud).max())
    buf += 0.30 * latch / max(1e-9, np.abs(latch).max())
    return buf


def se_thunder():
    """闷雷：远处滚过来，久久不散。"""
    dur = 4.2
    n = int(dur * SR)
    buf = np.zeros(n)
    rng = np.random.default_rng(6)
    t = t_of(n)
    body = fft_filter(rng.standard_normal(n), lo=None, hi=180)
    body /= max(1e-9, np.abs(body).max())
    env = np.clip(t / 0.35, 0, 1) * np.exp(-t * 0.75)
    env *= 1.0 + 0.35 * np.sin(2 * np.pi * 0.9 * t)
    crack = fft_filter(rng.standard_normal(n), lo=300, hi=3000) * np.exp(-t * 22)
    buf += 0.85 * body * env
    buf += 0.25 * crack / max(1e-9, np.abs(crack).max())
    return buf


def se_meteor():
    """第一颗流星：一条飞快划过的气声，尾巴上带一点亮。"""
    dur = 2.2
    n = int(dur * SR)
    buf = np.zeros(n)
    rng = np.random.default_rng(8)
    t = t_of(n)
    whoosh = np.zeros(n)
    # 用分段的带通叠加模拟扫频（纯 numpy 里最省事的做法）
    for f0 in (500, 1100, 2000, 3400, 5200):
        seg = fft_filter(rng.standard_normal(n), lo=f0 * 0.7, hi=f0 * 1.5)
        w = np.exp(-((t - 0.55) ** 2) / (0.02 + 0.00012 * (f0 - 480)))
        whoosh += seg * w / max(1e-9, np.abs(seg * w).max())
    whoosh /= max(1e-9, np.abs(whoosh).max())
    sparkle = np.zeros(n)
    for i, f in enumerate((1568.0, 2093.0, 2637.0)):
        add(sparkle, bell_tone(f, 1.1, 0.12), 0.62 + i * 0.06)
    buf += 0.55 * whoosh * np.clip((t - 0.1) * 3.0, 0, 1) * np.exp(-t * 0.9)
    buf += 0.16 * sparkle
    return buf


def se_shutter():
    """相机快门：反光板抬起「咔」+ 帘幕落下「嚓」，中间隔 70ms。"""
    n = int(0.55 * SR)
    buf = np.zeros(n)
    rng = np.random.default_rng(12)
    for at, cut, amp in ((0.02, (900, 5200), 0.55), (0.09, (500, 3400), 0.42)):
        m = int(0.07 * SR)
        t = t_of(m)
        click = fft_filter(rng.standard_normal(m), lo=cut[0], hi=cut[1]) * np.exp(-t * 55)
        click += 0.35 * np.sin(2 * np.pi * 2600 * t) * np.exp(-t * 70)
        add(buf, click / max(1e-9, np.abs(click).max()) * amp, at)
    # 一点机械余振
    ring = np.sin(2 * np.pi * 740 * t_of(n)) * np.exp(-t_of(n) * 26)
    buf += 0.07 * ring
    return buf


def se_page():
    """翻书页：两下纸张摩擦，最后轻轻一响。"""
    n = int(0.8 * SR)
    buf = np.zeros(n)
    rng = np.random.default_rng(15)
    for at, dur in ((0.0, 0.28), (0.30, 0.24)):
        m = int(dur * SR)
        t = t_of(m)
        paper = fft_filter(rng.standard_normal(m), lo=1200, hi=9000)
        paper /= max(1e-9, np.abs(paper).max())
        bump = np.exp(-((t - dur * 0.45) ** 2) / (0.02 * dur))
        add(buf, paper * bump * 0.5, at)
    m = int(0.08 * SR)
    tap = fft_filter(rng.standard_normal(m), lo=300, hi=2500) * np.exp(-t_of(m) * 40)
    add(buf, tap / max(1e-9, np.abs(tap).max()) * 0.25, 0.56)
    return buf


def se_wind():
    """一阵风：低频呼呼 + 高处一点口哨声，慢慢来慢慢走。"""
    dur = 4.0
    n = int(dur * SR)
    rng = np.random.default_rng(18)
    t = t_of(n)
    body = fft_filter(rng.standard_normal(n), lo=40, hi=700)
    body /= max(1e-9, np.abs(body).max())
    gust = 0.5 + 0.5 * np.sin(2 * np.pi * 0.22 * t - 1.2)
    gust = np.clip(gust, 0, 1) ** 1.6
    body = body * gust
    whistle = np.zeros(n)
    for f0 in (520.0, 810.0):
        vib = 1.0 + 0.03 * np.sin(2 * np.pi * 1.7 * t)
        w = np.sin(2 * np.pi * f0 * vib * t) * gust * 0.05
        whistle += w
    out = 0.85 * body + whistle
    edge = int(0.4 * SR)
    out[:edge] *= np.linspace(0, 1, edge)
    out[-edge:] *= np.linspace(1, 0, edge)
    return out


def se_clock():
    """钟表滴答：一秒两下（嘀 / 嗒），交替略不同的音色。"""
    dur = 3.0
    n = int(dur * SR)
    buf = np.zeros(n)
    rng = np.random.default_rng(23)
    for i in range(6):
        m = int(0.05 * SR)
        t = t_of(m)
        hi = 4200 if i % 2 == 0 else 3100
        click = fft_filter(rng.standard_normal(m), lo=700, hi=hi) * np.exp(-t * 75)
        click += 0.4 * np.sin(2 * np.pi * (2100 if i % 2 == 0 else 1500) * t) * np.exp(-t * 90)
        add(buf, click / max(1e-9, np.abs(click).max()) * (0.34 if i % 2 == 0 else 0.27), i * 0.5)
    return buf


SE = {
    "se_bell": se_bell,
    "se_door": se_door,
    "se_thunder": se_thunder,
    "se_meteor": se_meteor,
    "se_shutter": se_shutter,
    "se_page": se_page,
    "se_wind": se_wind,
    "se_clock": se_clock,
}


# ---------------------------------------------------------------- 导出
def to_stereo(x, width=0.35, seed=3):
    """单声道 → 立体声：轻微左右的延迟与音量差，避免"中间一根针"。"""
    rng = np.random.default_rng(seed)
    d = int(rng.uniform(0.004, 0.010) * SR)
    left = x.copy()
    right = np.concatenate([np.zeros(d), x[:-d]]) * (1.0 + width * 0.15)
    left = left * (1.0 - width * 0.15)
    out = np.stack([left, right], axis=1)
    # 高频轻微去相关（听起来更宽）
    return out


def write_wav(path, x, stereo=True):
    import wave
    data = np.clip(x, -1.0, 1.0)
    if stereo:
        data = to_stereo(data)
    pcm = (data * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2 if stereo else 1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def normalize(x, fade=0.6, target_rms=0.20, ceiling=0.80):
    """响度对齐 + 软限幅。

    只按峰值归一化的话，'热闹' 的曲子听起来会比 '安静' 的响一截
    （安静曲子的峰值稀疏，峰值一样但 RMS 低很多）。所以：
      1. 先把整首按 RMS 拉到目标响度（-14 dBFS 左右）
      2. 用 tanh 软限幅压掉突起，再确保峰值不超天花板（只降不升，
         否则又会把响度对齐的结果抹平）
      3. 首尾淡入淡出，循环播放不会"啪"一声
    """
    rms = float(np.sqrt(np.mean(x ** 2)))
    if rms > 1e-6:
        x = x * min(3.0, max(0.3, target_rms / rms))
    x = np.tanh(x * 1.1) / np.tanh(1.1)
    peak = float(np.abs(x).max())
    if peak > ceiling:
        x = x * (ceiling / peak)
    k = int(fade * SR)
    if k * 2 < len(x):
        x[:k] *= np.linspace(0, 1, k)
        x[-k:] *= np.linspace(1, 0, k)
    return x


def encode_ogg(wav_path, ogg_path, quality="4"):
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav_path,
                    "-c:a", "libvorbis", "-q:a", quality, ogg_path], check=True)


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    os.makedirs(AUDIO, exist_ok=True)
    tmp = "/tmp/qlwt_audio"
    os.makedirs(tmp, exist_ok=True)

    for name, fn in BGM.items():
        if what not in ("all", "bgm", name):
            continue
        print(f"[BGM] {name} ...", flush=True)
        x = normalize(fn())
        mix, decay, damp = BGM_VERB[name]
        x = reverb(x, mix=mix, decay=decay, damp=damp, seed=hash(name) % 100)
        x = normalize(x)
        wav = os.path.join(tmp, name + ".wav")
        write_wav(wav, x)
        ogg = os.path.join(AUDIO, name + ".ogg")
        encode_ogg(wav, ogg)
        print(f"      {len(x)/SR:.1f}s → {os.path.relpath(ogg, PROJ)} "
              f"({os.path.getsize(ogg)//1024} KB)", flush=True)

    for name, fn in SE.items():
        if what not in ("all", "se", name):
            continue
        print(f"[SE ] {name} ...", flush=True)
        x = normalize(fn(), fade=0.01, target_rms=0.30, ceiling=0.85)
        dst = os.path.join(AUDIO, name + ".wav")
        write_wav(dst, x)
        print(f"      {len(x)/SR:.2f}s → {os.path.relpath(dst, PROJ)} "
              f"({os.path.getsize(dst)//1024} KB)", flush=True)


if __name__ == "__main__":
    main()
