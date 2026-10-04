#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
画风对比：同一批场景，用「模型 × 提示词写法 × 采样参数」各出一张，拼成对照图。
用来挑背景画风（"太 AI 了" 多半是模型偏角色向 + tag 堆砌的提示词 + 高 CFG）。

用法：cutout/.venv/bin/python tools/imagegen/compare_style.py [--rebuild]
输出：gen_preview/style_compare.png（行 = 场景，列 = 方案）
      --rebuild 忽略 ComfyUI 输出目录里已有的同名图，全部重出
"""

import glob
import os
import sys

from PIL import Image, ImageDraw

from make_character import OUT_DIR, PREVIEW, PROJ, wait_images, queue

GEN_W, GEN_H = 1344, 768
OUT_W, OUT_H = 640, 360

# 两种提示词写法：tag 堆砌（现在用的） vs 像给画师写的美术说明
TAG_SUFFIX = ("masterpiece, best quality, very aesthetic, absurdres, "
              "no humans, scenery, anime background, detailed environment")
NL_SUFFIX = ("Painted anime background for a visual novel. Clean readable shapes, muted colors, "
             "soft shadows, calm composition, no text, no people.")
NEG_TAG = ("lowres, bad anatomy, bad hands, text, error, worst quality, low quality, signature, "
           "watermark, username, blurry, people, humans, 1girl, 1boy")
NEG_NL = (NEG_TAG + ", oversaturated, hdr, high contrast, glossy, plastic, noisy, cluttered")
NEG_MUTED = (NEG_NL + ", 3d render, cgi, digital painting, airbrush, vignette, lens flare, "
             "chromatic aberration, bloom")

# 在「美术说明」基础上再叠一层画法，用来压掉塑料感 / 数码味
STYLE_SUFFIX = {
    "": "",
    "水彩手绘": ", hand painted watercolor illustration, visible paper grain, soft pigment "
                "bleeding, limited muted palette",
    "动画美术板": ", flat cel shading, clean thin line art, matte finish, color script look, "
                  "soft natural light, minimal detail",
}

# 两个有代表性的场景
SCENES = {
    "clubroom": dict(
        tag="old school club room, star chart poster on wall, telescope by the window, wooden desks, "
            "dusty, warm afternoon light",
        nl="A quiet after-school astronomy club room in an old school building. Warm afternoon light "
           "comes through the window and falls across wooden desks, a star chart poster and an old "
           "telescope in the corner."),
    "snow_school": dict(
        tag="old school building covered in thick snow, observatory dome on the roof, bare winter "
            "trees, pale afternoon light",
        nl="An old school building under thick winter snow. A small observatory dome sits on its "
           "roof, bare trees line the yard, the light is pale and cold."),
}

# 变体：模型 + 提示词写法 + 采样参数（前四个带 reuse 前缀，直接复用已有出图，省 GPU）
VARIANTS = [
    ("animagine+tag",      "animagine-xl-3.1.safetensors", "tag", "",         28, 5.5),
    ("animagine+美术说明",   "animagine-xl-3.1.safetensors", "nl",  "",         28, 5.0),
    ("noobai+美术说明",     "NoobAI-XL-v1.1.safetensors",  "nl",  "",         28, 4.5),
    ("noobai+tag",         "NoobAI-XL-v1.1.safetensors",  "tag", "",         28, 4.5),
    ("animagine+说明+低CFG", "animagine-xl-3.1.safetensors", "nl",  "",         28, 3.5),
    ("animagine+水彩手绘",   "animagine-xl-3.1.safetensors", "nl",  "水彩手绘",   28, 4.5),
    ("animagine+动画美术板", "animagine-xl-3.1.safetensors", "nl",  "动画美术板", 28, 5.0),
]

# 前 4 个方案的旧出图仍在 ComfyUI/output，直接读回来，不用重跑
REUSE = 4


def gen(model, pos, neg, seed, prefix, steps, cfg):
    wf = {
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": model}},
        "5": {"class_type": "EmptyLatentImage", "inputs": {"width": GEN_W, "height": GEN_H, "batch_size": 1}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": pos, "clip": ["4", 1]}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"text": neg, "clip": ["4", 1]}},
        "3": {"class_type": "KSampler",
              "inputs": {"seed": seed, "steps": steps, "cfg": cfg,
                         "sampler_name": "euler_ancestral", "scheduler": "normal", "denoise": 1.0,
                         "model": ["4", 0], "positive": ["6", 0], "negative": ["7", 0],
                         "latent_image": ["5", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    }
    return wait_images(queue(wf, client="compare"), timeout=900)


def cached(prefix):
    """ComfyUI 输出目录里有没有这套出图（cmp_<scene>_<vi>_00001_.png）。"""
    hits = sorted(glob.glob(os.path.join(OUT_DIR, "", prefix + "_*.png")),
                  key=os.path.getmtime)
    return hits[-1] if hits else None


def stats(im):
    """客观指标：平均饱和度 / 对比度 / 高饱和像素占比，用来量化'太 AI'。"""
    small = im.resize((160, 90), Image.LANCZOS)
    px = list(small.getdata())
    sat = con = 0.0
    hot = 0
    for r, g, b in px:
        mx, mn = max(r, g, b), min(r, g, b)
        s = 0 if mx == 0 else (mx - mn) / mx
        sat += s
        con += mx - mn
        if s > 0.8:
            hot += 1
    n = len(px)
    return sat / n * 100, con / n, hot / n * 100


def main():
    os.makedirs(PREVIEW, exist_ok=True)
    rebuild = "--rebuild" in sys.argv
    shots = {}
    for si, (skey, sinfo) in enumerate(SCENES.items()):
        for vi, (vname, model, style, suffix, steps, cfg) in enumerate(VARIANTS):
            pos = (f"{TAG_SUFFIX}, {sinfo['tag']}" if style == "tag"
                   else f"{sinfo['nl']} {NL_SUFFIX}{STYLE_SUFFIX[suffix]}")
            neg = NEG_TAG if style == "tag" else (NEG_MUTED if suffix else NEG_NL)
            prefix = f"cmp_{skey}_{vi}"
            path, dt = None, 0.0
            if vi < REUSE and not rebuild:
                path = cached(prefix)
            if path is None:
                path, dt = gen(model, pos, neg, 90000 + si * 100 + vi, prefix, steps, cfg)
            im = Image.open(path).convert("RGB").resize((OUT_W, OUT_H), Image.LANCZOS)
            shots[(skey, vname)] = im
            s, c, h = stats(im)
            print(f"  {skey:<12} {vname:<16} {dt:5.1f}s  饱和度{s:5.1f}  对比度{c:5.1f}  艳色{h:4.1f}%",
                  flush=True)

    sheet = Image.new("RGB", (OUT_W * len(VARIANTS), OUT_H * len(SCENES)), (18, 20, 28))
    d = ImageDraw.Draw(sheet)
    for si, skey in enumerate(SCENES):
        for vi, (vname, *_rest) in enumerate(VARIANTS):
            sheet.paste(shots[(skey, vname)], (vi * OUT_W, si * OUT_H))
            d.text((vi * OUT_W + 8, si * OUT_H + 6), f"{vi + 1}. {skey} / {vname}",
                   fill=(255, 255, 255))
    dst = os.path.join(PREVIEW, "style_compare.png")
    sheet.save(dst)
    print("对照图 ->", dst, sheet.size)
    print("（行=场景，列=方案，列号见图上角标）")


if __name__ == "__main__":
    main()
