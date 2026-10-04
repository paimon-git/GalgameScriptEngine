#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按资产清单生成一个角色的立绘（出图 → 抠透明底 → 缩放到引擎规格）。

依赖（都在 ai-imagegen 里，不用装进项目）：
    /home/hoshino/project/ai-imagegen/ComfyUI              出图服务（先启动）
    /home/hoshino/project/ai-imagegen/cutout/.venv         本脚本的运行环境（含 rembg/PIL）

用法：
    /home/hoshino/project/ai-imagegen/cutout/.venv/bin/python \
        tools/imagegen/make_character.py wu_hongtao

之后要做表情时，本脚本输出的 *_base_raw.png 就是 img2img 的输入。
"""

import json
import os
import shutil
import sys
import time
import urllib.request

from PIL import Image, ImageDraw

API = "http://127.0.0.1:8188"
COMFY = "/home/hoshino/project/ai-imagegen/ComfyUI"
OUT_DIR = os.path.join(COMFY, "output")
IN_DIR = os.path.join(COMFY, "input")
PROJ = "/home/hoshino/project/GalgameScriptEngine"
PREVIEW = os.path.join(PROJ, "gen_preview")

# 引擎规格（见 docs/star_festival_assets.md）
SPRITE_W, SPRITE_H = 480, 900

MODEL = "animagine-xl-3.1.safetensors"
QUALITY = "masterpiece, best quality, very aesthetic, absurdres"
NEG_BASE = ("lowres, bad anatomy, bad hands, extra digits, fewer digits, text, error, "
            "worst quality, low quality, low score, bad score, average score, signature, "
            "watermark, username, blurry, multiple views, 2boys, 2girls, cropped")

# ---- 角色设定：提示词照着 docs/star_festival_assets.md 的外形描述写 ----
BASE_TAGS = ("full body, standing, simple background, white background, "
             "anime style, visual novel sprite, cel shading, front view")


def boy(desc, extra_neg=""):
    return dict(pos=f"1boy, solo, chinese high school student, {desc}, {BASE_TAGS}",
                neg="girl, female, long hair, out of frame, multiple legs, extra arms " + extra_neg)


def girl(desc, extra_neg=""):
    return dict(pos=f"1girl, solo, chinese high school girl, {desc}, {BASE_TAGS}",
                neg="boy, male, short hair, out of frame, multiple legs, extra arms " + extra_neg)


CHARACTERS = {
    "wu_hongtao": dict(
        cn="吴鸿韬",
        pos=("1boy, solo, full body, standing, chinese high school student, short black hair, "
             "calm expression, navy school uniform blazer, white shirt, dark trousers, "
             "canvas messenger bag, arms at sides, simple background, white background, "
             "anime style, visual novel sprite, cel shading, front view"),
        neg="girl, female, long hair, out of frame, multiple legs, extra arms"),
    "li_lin": dict(
        cn="黎璘",
        pos=("1girl, solo, full body, standing, chinese high school girl, long black hair, "
             "gentle smile, warm beige scarf, navy school uniform, holding a book under one arm, "
             "library committee, " + BASE_TAGS),
        neg="boy, male, short hair, out of frame, multiple legs, extra arms"),
    "zhang_yuteng": dict(
        cn="张誉腾",
        pos=("1boy, solo, full body, standing, chinese high school student, short black hair, "
             "serious expression, neat navy school uniform, necktie, red student council armband, "
             "arms at sides, " + BASE_TAGS),
        neg="girl, female, long hair, out of frame, multiple legs, extra arms"),
    "liang_zhiyi": dict(
        cn="梁知奕",
        pos=("1girl, solo, full body, standing, chinese high school girl, medium length hair, "
             "confident smirk, loose grey cardigan over navy school uniform, spinning a pen, "
             + BASE_TAGS),
        neg="boy, male, short hair, out of frame, multiple legs, extra arms"),
    "yin_botao": dict(
        cn="尹博涛",
        pos=("1boy, solo, full body, standing, chinese high school student, short spiky hair, "
             "energetic grin, blue track jacket, sports shorts, kinesiology tape on calf, "
             "running shoes," + BASE_TAGS),
        neg="girl, female, long hair, out of frame, multiple legs, extra arms"),
    "li_junhao": dict(
        cn="李君浩",
        pos=("1boy, solo, full body, standing, chinese high school first year boy, small stature, "
             "short brown hair, shy expression, navy school uniform, backpack with a gear keychain, "
             + BASE_TAGS),
        neg="girl, female, long hair, adult, tall, out of frame, multiple legs, extra arms"),
    "li_junchen": dict(
        cn="李俊辰",
        pos=("1boy, solo, full body, standing, chinese high school student, messy dark hair, "
             "bored expression, white shirt unbuttoned over a black t-shirt, canvas brush pouch "
             "on shoulder, hands in pockets, " + BASE_TAGS),
        neg="girl, female, long hair, out of frame, multiple legs, extra arms"),
    "shao_qinghe": dict(
        cn="邵清和",
        pos=("1man, solo, full body, standing, young male teacher, short black hair, tired "
             "expression, white dress shirt, dark vest, sleeves rolled up, teacher ID badge, "
             + BASE_TAGS),
        neg="girl, female, long hair, child, student, out of frame, multiple legs, extra arms"),
    "wei_siyuan": dict(
        cn="魏思远",
        pos=("1boy, solo, full body, standing, college student, short hair, friendly smile, "
             "winter coat, scarf, hiking backpack, " + BASE_TAGS),
        neg="girl, female, long hair, child, out of frame, multiple legs, extra arms"),
    # 续篇《星屑祭典 · 雪夜》新增角色
    "gu_xinghe": dict(
        cn="顾星禾",
        pos=("1girl, solo, full body, standing, chinese high school first year girl, short black "
             "hair with a small hair clip, bright cheerful expression, camera hanging on a neck "
             "strap, navy school uniform with a red ribbon, " + BASE_TAGS),
        neg="boy, male, adult, tall, out of frame, multiple legs, extra arms"),
    # 第三部《星屑祭典 · 盛夏》新增角色
    "shen_yan": dict(
        cn="沈砚",
        pos=("1boy, solo, full body, standing, chinese high school first year boy, small stature, "
             "short black hair, round glasses, nervous shy expression, holding a notebook against "
             "his chest, navy school uniform, white shirt, " + BASE_TAGS),
        neg="girl, female, long hair, adult, tall, old man, out of frame, multiple legs, extra arms"),
}

W, H = 768, 1344      # SDXL 竖构图（64 的倍数），比 480x900 略宽，便于裁剪对齐
STEPS, CFG = 28, 5.5


def queue(workflow, client="gensprite"):
    req = urllib.request.Request(
        API + "/prompt",
        data=json.dumps({"prompt": workflow, "client_id": client}).encode(),
        headers={"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(req, timeout=30).read())["prompt_id"]


def wait_images(prompt_id, timeout=600):
    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(2)
        h = json.loads(urllib.request.urlopen(f"{API}/history/{prompt_id}", timeout=30).read())
        if prompt_id in h:
            for node in h[prompt_id]["outputs"].values():
                for img in node.get("images", []):
                    return os.path.join(OUT_DIR, img.get("subfolder", ""), img["filename"]), time.time() - t0
    raise TimeoutError("ComfyUI 出图超时")


def txt2img(pos, neg, seed, prefix, w=W, h=H, steps=STEPS, cfg=CFG):
    wf = {
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": MODEL}},
        "5": {"class_type": "EmptyLatentImage", "inputs": {"width": w, "height": h, "batch_size": 1}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": pos, "clip": ["4", 1]}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"text": neg, "clip": ["4", 1]}},
        "3": {"class_type": "KSampler",
              "inputs": {"seed": seed, "steps": steps, "cfg": cfg,
                         "sampler_name": "euler_ancestral", "scheduler": "normal",
                         "denoise": 1.0, "model": ["4", 0], "positive": ["6", 0],
                         "negative": ["7", 0], "latent_image": ["5", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    }
    return wait_images(queue(wf))


def cutout(rgba):
    """动漫专用分割模型抠透明底（懒加载，只初始化一次）"""
    global _SESSION
    try:
        _SESSION
    except NameError:
        from rembg import new_session
        _SESSION = new_session("isnet-anime")
    from rembg import remove
    return remove(rgba, session=_SESSION, alpha_matting=False)


def fit_params(img, target_w=SPRITE_W, target_h=SPRITE_H):
    """算出"原图 -> 立绘"的缩放与偏移：水平居中、脚底贴画布底边"""
    alpha = img.getchannel("A")
    bbox = alpha.point(lambda v: 255 if v > 24 else 0).getbbox()
    if not bbox:
        raise ValueError("抠图后没有找到角色主体")
    x0, y0, x1, y1 = bbox
    cw, ch = x1 - x0, y1 - y0
    s = min(target_w / cw, target_h / ch)
    off_x = (target_w - cw * s) * 0.5 - x0 * s
    off_y = target_h - ch * s - y0 * s          # 脚底对齐底边
    return bbox, s, off_x, off_y


def warp_to_sprite(img, s, off_x, off_y, target_w=SPRITE_W, target_h=SPRITE_H):
    """按同一套变换把原图放进立绘画布，保证立绘与表情贴图逐像素对齐"""
    scaled = img.resize((max(1, round(img.width * s)), max(1, round(img.height * s))), Image.LANCZOS)
    canvas = Image.new("RGBA", (target_w, target_h), (0, 0, 0, 0))
    canvas.paste(scaled, (round(off_x), round(off_y)), scaled)
    return canvas


def head_box(sprite):
    """从立绘 alpha 推出头部大致方框（Sprite 坐标）：站立人物取身体上端约 18%"""
    a = sprite.getchannel("A")
    bbox = a.point(lambda v: 255 if v > 24 else 0).getbbox()
    x0, y0, x1, y1 = bbox
    ch = y1 - y0
    band = a.crop((x0, y0, x1, y0 + int(ch * 0.18)))
    bb = band.getbbox()
    if not bb:
        raise ValueError("找不到头部区域")
    hx0, hy0, hx1, hy1 = bb
    pad_x = int((hx1 - hx0) * 0.10)
    return (max(0, x0 + hx0 - pad_x), max(0, y0 + hy0 - 4),
            min(sprite.width, x0 + hx1 + pad_x), min(sprite.height, y0 + hy1 + 4))


def sprite_rect_to_raw(rect, s, off_x, off_y):
    """立绘坐标 -> 原图坐标（用于在原图上生成表情）"""
    x0, y0, x1, y1 = rect
    return (int((x0 - off_x) / s), int((y0 - off_y) / s),
            int((x1 - off_x) / s), int((y1 - off_y) / s))


# 表情：只在脸部区域重绘，同一 seed 保证风格一致
EXPRESSIONS = {
    "normal":   "neutral expression, closed mouth, calm",
    "happy":    "happy, smiling, open mouth, happy eyes, cheerful",
    "sad":      "sad, downturned mouth, sad eyes, sorrowful",
    "angry":    "angry, frown, furrowed brow, angry eyes, glaring",
    "blush":    "blush, blushing cheeks, shy, embarrassed, averting eyes",
    "surprise": "surprised, wide eyes, open mouth, shocked",
}


def inpaint(raw_path, mask_path, pos, neg, seed, prefix, denoise=0.65):
    """ComfyUI 局部重绘（只改蒙版区域，其余像素保持不变）"""
    wf = {
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": MODEL}},
        "10": {"class_type": "LoadImage", "inputs": {"image": os.path.basename(raw_path)}},
        "12": {"class_type": "LoadImageMask",
               "inputs": {"image": os.path.basename(mask_path), "channel": "red"}},
        "11": {"class_type": "VAEEncode", "inputs": {"pixels": ["10", 0], "vae": ["4", 2]}},
        "13": {"class_type": "SetLatentNoiseMask", "inputs": {"samples": ["11", 0], "mask": ["12", 0]}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": pos, "clip": ["4", 1]}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"text": neg, "clip": ["4", 1]}},
        "3": {"class_type": "KSampler",
              "inputs": {"seed": seed, "steps": STEPS, "cfg": CFG,
                         "sampler_name": "euler_ancestral", "scheduler": "normal",
                         "denoise": denoise, "model": ["4", 0], "positive": ["6", 0],
                         "negative": ["7", 0], "latent_image": ["13", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    }
    return wait_images(queue(wf, client="inpaint"))


def build(key, want_faces=True):
    cfg = CHARACTERS[key]
    os.makedirs(PREVIEW, exist_ok=True)
    os.makedirs(IN_DIR, exist_ok=True)

    print(f"[1/3] 出图：{cfg['cn']} 全身立绘 {W}x{H} ...", flush=True)
    raw_path, dt = txt2img(f"{QUALITY}, {cfg['pos']}", f"{NEG_BASE}, {cfg['neg']}",
                           seed=20261004, prefix=f"{key}_base")
    print(f"      {os.path.basename(raw_path)}  {dt:.1f}s", flush=True)

    raw = Image.open(raw_path).convert("RGBA")
    keep = os.path.join(PREVIEW, f"{key}_base_raw.png")
    raw.save(keep)

    print("[2/3] 抠透明底（isnet-anime）...", flush=True)
    cut = cutout(raw)
    cut.save(os.path.join(PREVIEW, f"{key}_cutout.png"))

    print(f"[3/3] 缩放到 {SPRITE_W}x{SPRITE_H} 并对齐脚底 ...", flush=True)
    _, s, off_x, off_y = fit_params(cut)
    sprite = warp_to_sprite(cut, s, off_x, off_y)
    dst_dir = os.path.join(PROJ, "assets", "char")
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, f"{key}_body.png")
    sprite.save(dst)
    sprite.save(os.path.join(PREVIEW, f"{key}_body_{SPRITE_W}x{SPRITE_H}.png"))

    a = sprite.getchannel("A")
    n = a.width * a.height
    print(f"完成 → {dst}")
    print(f"  透明 {100*sum(1 for v in a.getdata() if v < 16)/n:.1f}%   "
          f"尺寸 {sprite.size[0]}x{sprite.size[1]}")

    if not want_faces:
        return

    # ---------------- 表情贴图 ----------------
    face_dir = os.path.join(dst_dir, f"{key}_face")
    os.makedirs(face_dir, exist_ok=True)
    hb = head_box(sprite)                       # 立绘坐标下的头部框
    rb = sprite_rect_to_raw(hb, s, off_x, off_y)  # 原图坐标下的头部框
    print(f"[表情] 头部框 立绘{hb} → 原图{rb}", flush=True)

    # 蒙版：白=重绘区域。只覆盖头部，其余像素原样保留
    mask = Image.new("L", raw.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(rb, radius=int((rb[2] - rb[0]) * 0.25), fill=255)
    mask_name = f"{key}_head_mask.png"
    mask.save(os.path.join(IN_DIR, mask_name))
    shutil.copy(keep, os.path.join(IN_DIR, os.path.basename(keep)))

    for name, tag in EXPRESSIONS.items():
        if name == "normal":
            src = raw          # 基础图本身就是普通表情，直接用
            dt = 0.0
        else:
            pos = f"{QUALITY}, {cfg['pos']}, {tag}, face focus"
            out_path, dt = inpaint(os.path.basename(keep), mask_name,
                                   pos, f"{NEG_BASE}, {cfg['neg']}, different character",
                                   seed=20261004, prefix=f"{key}_face_{name}")
            src = Image.open(out_path).convert("RGBA")

        warped = warp_to_sprite(src, s, off_x, off_y)
        # 关键：重绘产物是「带背景的原图」，必须套用立绘已抠好的 alpha，
        # 否则脸贴图会是一整块不透明矩形，把头部周围的背景一起糊上去。
        warped.putalpha(sprite.getchannel("A"))
        overlay = Image.new("RGBA", (SPRITE_W, SPRITE_H), (0, 0, 0, 0))
        overlay.paste(warped.crop(hb), (hb[0], hb[1]))
        overlay.save(os.path.join(face_dir, f"{name}.png"))
        overlay.save(os.path.join(PREVIEW, f"{key}_face_{name}.png"))

        # 自检：蒙版之外（不该被重绘的地方）贴图应与立绘逐像素一致，
        # 这样叠加到立绘上才不会出现错位/接缝。alpha 和 RGB 都要比。
        wmask = warp_to_sprite(mask.convert("RGBA"), s, off_x, off_y).getchannel("A")
        diff = 0
        so, oo, mo = sprite.getdata(), overlay.getdata(), wmask.getdata()
        for (r1, g1, b1, a1), (r2, g2, b2, a2), m in zip(so, oo, mo):
            if m < 8:
                if abs(a1 - a2) > 8 or (a2 > 8 and
                        (abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2)) > 24):
                    diff += 1
        print(f"        {name:<9} {'(基础图)' if dt == 0 else f'{dt:.0f}s'}   贴图覆盖 "
              f"{100*sum(1 for p in overlay.getdata() if p[3] > 8)/n:4.1f}%  蒙版外偏差 {diff}",
              flush=True)


def rebuild_faces(key):
    """
    重用已经生成好的重绘产物，重做脸部贴图（把立绘已抠好的 alpha 套上去）。
    不调用 ComfyUI，因此不需要 GPU —— 用来修"脸贴图带背景"这类后处理问题。
    """
    cut = Image.open(os.path.join(PREVIEW, f"{key}_cutout.png")).convert("RGBA")
    _, s, off_x, off_y = fit_params(cut)
    sprite = Image.open(os.path.join(PROJ, "assets", "char", f"{key}_body.png")).convert("RGBA")
    hb = head_box(sprite)
    mask = Image.open(os.path.join(IN_DIR, f"{key}_head_mask.png")).convert("L")
    raw = Image.open(os.path.join(PREVIEW, f"{key}_base_raw.png")).convert("RGBA")
    face_dir = os.path.join(PROJ, "assets", "char", f"{key}_face")
    os.makedirs(face_dir, exist_ok=True)

    wmask = warp_to_sprite(mask.convert("RGBA"), s, off_x, off_y).getchannel("A")
    body_alpha = sprite.getchannel("A")
    n = SPRITE_W * SPRITE_H

    for name in EXPRESSIONS:
        if name == "normal":
            src = raw
        else:
            p = os.path.join(OUT_DIR, f"{key}_face_{name}_00001_.png")
            if not os.path.exists(p):
                print(f"        {name:<9} 缺少重绘产物，跳过", flush=True)
                continue
            src = Image.open(p).convert("RGBA")

        warped = warp_to_sprite(src, s, off_x, off_y)
        warped.putalpha(body_alpha)          # ← 关键修正
        overlay = Image.new("RGBA", (SPRITE_W, SPRITE_H), (0, 0, 0, 0))
        overlay.paste(warped.crop(hb), (hb[0], hb[1]))
        overlay.save(os.path.join(face_dir, f"{name}.png"))
        overlay.save(os.path.join(PREVIEW, f"{key}_face_{name}.png"))

        diff = 0
        cover = 0
        for (r1, g1, b1, a1), (r2, g2, b2, a2), m in zip(sprite.getdata(), overlay.getdata(),
                                                         wmask.getdata()):
            if a2 > 8:
                cover += 1
            if m < 8 and (abs(a1 - a2) > 8 or
                          (a2 > 8 and abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2) > 24)):
                diff += 1
        print(f"        {name:<9} 覆盖 {100*cover/n:4.1f}%  蒙版外偏差 {diff}", flush=True)


def main():
    argv = sys.argv[1:]
    want_faces = "--no-faces" not in argv
    plain = [a for a in argv if not a.startswith("--")]

    if "--refaces" in argv:
        # 只用已有的重绘产物重做脸部贴图（修 alpha），不占用 GPU
        keys = list(CHARACTERS) if "--all" in argv else [plain[0] if plain else "wu_hongtao"]
        for i, key in enumerate(keys, 1):
            print(f"\n===== [{i}/{len(keys)}] {key} ({CHARACTERS[key]['cn']}) 重做脸部贴图 =====", flush=True)
            try:
                rebuild_faces(key)
            except Exception as e:
                print(f"----- {key} 失败：{type(e).__name__}: {e} -----", flush=True)
        return

    if "--all" in argv:
        keys = [k for k in CHARACTERS if k != "wu_hongtao"]      # 吴鸿韬已完成
    else:
        keys = [plain[0] if plain else "wu_hongtao"]

    for i, key in enumerate(keys, 1):
        print(f"\n===== [{i}/{len(keys)}] {key} ({CHARACTERS[key]['cn']}) =====", flush=True)
        t0 = time.time()
        try:
            build(key, want_faces)
            print(f"----- {key} 完成，用时 {time.time()-t0:.0f}s -----", flush=True)
        except Exception as e:
            print(f"----- {key} 失败：{type(e).__name__}: {e} -----", flush=True)


if __name__ == "__main__":
    main()
