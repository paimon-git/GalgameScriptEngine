#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成背景与 CG（资产清单见 docs/star_festival_assets.md）。

画风：animagine-xl-3.1 + 自然语言美术说明（不是 tag 堆砌）+ CFG 4.5 + 轻微去饱和。
       早先用 "masterpiece, absurdres, ..." 那种 tag 写法 + CFG 5.5，出图高饱和高对比，
       "AI 味"很重；这一版按对照实验（gen_preview/style_compare.png）换成了给画师写说明的写法。

背景：文生图 1344x768 → 缩到 1280x720 → 轻微去饱和 → assets/bg/<id>.png
CG  ：同上 → assets/cg/<id>.png

用法：
    cutout/.venv/bin/python tools/imagegen/make_scenes.py              # 只补缺失的
    cutout/.venv/bin/python tools/imagegen/make_scenes.py --force      # 全部重出（换画风用这个）
    cutout/.venv/bin/python tools/imagegen/make_scenes.py bg --force   # 只重出背景
    cutout/.venv/bin/python tools/imagegen/make_scenes.py cg           # 只补缺失的 CG
    cutout/.venv/bin/python tools/imagegen/make_scenes.py --sheet      # 出一张「旧 vs 新」对照图
"""

import os
import sys

from PIL import Image, ImageDraw, ImageEnhance

from make_character import OUT_DIR, PREVIEW, PROJ, wait_images, queue

BG_DIR = os.path.join(PROJ, "assets", "bg")
CG_DIR = os.path.join(PROJ, "assets", "cg")
OLD_DIR = os.path.join(PREVIEW, "old_style")      # 换画风前的旧图备份（--sheet 拿它做对比）
GEN_W, GEN_H = 1344, 768          # SDXL 友好的 16:9
OUT_W, OUT_H = 1280, 720
MODEL = "animagine-xl-3.1.safetensors"
STEPS, CFG = 30, 4.5              # 低 CFG：压掉高对比 / 塑料感
SATURATION = 0.90                 # 出图后轻微去饱和，和旧素材拉开距离

# 提示词：一句句写画面，最后统一补一段风格说明（对照实验里最"不 AI"的写法）
STYLE = ("Painted anime background for a visual novel. Clean readable shapes, muted colors, "
         "soft shadows, calm composition. No text.")
NEG = ("lowres, bad anatomy, bad hands, extra digits, text, error, worst quality, low quality, "
       "signature, watermark, username, blurry, multiple views, cropped, people, humans, 1girl, "
       "1boy, oversaturated, hdr, high contrast, glossy, plastic, noisy, cluttered, 3d render, "
       "cgi, airbrush, bloom, lens flare, chromatic aberration")
NEG_PPL = ("lowres, bad anatomy, bad hands, extra digits, text, error, worst quality, low quality, "
           "signature, watermark, username, blurry, multiple views, cropped, extra people, "
           "oversaturated, hdr, high contrast, glossy, plastic, noisy, cluttered, 3d render, cgi")


def brief(text, people=False):
    """把一句画面说明包成完整提示词。people=True 时不排除人物。"""
    if people:
        return f"{text} Figures are simple and seen from behind, faces not visible. {STYLE}"
    return f"{text} {STYLE}"


# ---------------- 背景 ----------------
# 键 = 文件名（ASCII），值 = (画面说明, 是否允许出现人物)
BACKGROUNDS = {
    # —— 第一部《星屑祭典》——
    "clubroom": ("A quiet after-school astronomy club room in an old school building. Warm "
                 "late-afternoon light falls through the window across wooden desks, a brass star "
                 "chart hangs on the wall, telescopes and boxes are piled in the corner, dust "
                 "floats in the air.", False),
    "corridor": ("A long corridor in an old school building at sunset. Orange light slants "
                 "through the windows and long shadows stretch across the floor, classroom doors "
                 "line both sides, an old wall clock hangs at the far end.", False),
    "playground": ("A school playground at noon. A running track lies in the distance, a row of "
                   "plane trees sheds yellow leaves over empty benches, the sky is clear and pale.",
                   False),
    "rooftop": ("A school rooftop at dusk. A chain-link fence and a water tank stand against the "
                "wind, the sky runs from orange to violet above a distant city skyline.", False),
    "observatory": ("The inside of an old observatory dome in daylight. A large equatorial "
                    "telescope stands in the center, the shutter is half open, a beam of light "
                    "falls on the bare concrete floor.", False),
    "faculty_room": ("A school faculty office in the afternoon. Desks are covered with stacked "
                     "papers, old photo albums sit on the windowsill, warm sunlight and a standing "
                     "fan.", False),
    "rain_street": ("A street outside a school in heavy rain. Cold blue tones, wet asphalt full "
                    "of reflections, the roadside is empty, a deep grey overcast sky.", False),
    "festival_night": ("A school festival at night. Red paper lanterns are strung overhead, food "
                       "stalls line the path, the observatory dome stands in the background, warm "
                       "lights of the crowd glow far away.", False),
    "classroom": ("A high school classroom. Rows of wooden desks and chairs, a blackboard, "
                  "sunlight through the windows and long white curtains. The room is empty.",
                  False),
    "street": ("A quiet street in front of a school gate at dusk. Street lamps are just turning "
               "on, trees and utility poles line the road, the roadside is empty, the light is "
               "warm and low.", False),
    "title": ("A night sky full of stars with the Milky Way, seen above the silhouette of an old "
              "school building and an observatory dome, one shooting star crosses the frame. "
              "Deep blue and violet gradient, atmospheric and quiet.", False),
    # —— 第二部《星屑祭典 · 雪夜》——
    "clubroom_winter": ("A school club room in winter. Snow falls outside the window, a small "
                        "heater glows in the corner, a star chart poster hangs on the wall, a "
                        "telescope stands beside the window, the interior light is warm.", False),
    "snow_school": ("An old school building under thick winter snow. A small observatory dome sits "
                    "on the roof, bare trees line the yard, the light is pale and cold.", False),
    "observatory_winter": ("The inside of an old observatory dome on a winter night. A warm lamp "
                           "burns beside a large equatorial telescope with frost on its metal, the "
                           "shutter is half open onto the night sky.", False),
    "spring_observatory": ("A small school astronomy museum on a spring afternoon. The renovated "
                           "old building carries an observatory dome on its roof, cherry blossoms "
                           "are in bloom, a new wooden signboard stands by the gate.", False),
    # —— 第三部《星屑祭典 · 盛夏》——
    "summer_street": ("A small town street on a hot summer noon. Cicadas, tangled power lines, an "
                      "old bus stop with a wooden bench, glare on the asphalt, deep green trees "
                      "and a white-blue sky.", False),
    "planetarium_hall": ("The exhibition hall of a small astronomy museum at night, dressed up for "
                         "a light show. Colourful LED strips run along the ceiling, a projector "
                         "paints a glowing dome overhead, coloured light spills over an old "
                         "telescope on display, everything a little too bright and theatrical.",
                         False),
    "summer_observatory": ("The inside of an observatory dome on a humid summer night. The shutter "
                           "is open, a large equatorial telescope points up, an electric fan turns "
                           "in the corner, moonlight and the orange glow of the town lie on the "
                           "floor.", False),
    "summer_hill": ("A grassy hill outside town just after sunset. A dirt path climbs over the "
                    "ridge, the sky fades from warm orange into deep blue, the first stars appear, "
                    "the distant town begins to glow.", False),
    "typhoon_street": ("A street during a summer typhoon. Rain lashes sideways across the frame, "
                       "water runs along the kerb, trees lean in the wind, the sky is dark grey "
                       "and nobody is outside.", False),
    "summer_rooftop": ("A school rooftop on a clear summer night. The Milky Way arches overhead, a "
                       "chain-link fence stands at the edge, the air is warm and the lights of "
                       "town spread far below.", False),
    # —— 支线《云志》——
    "library_old": ("The back room of an old school library where old periodicals are kept. "
                    "Tall wooden shelves sag under bound volumes, cardboard boxes are stacked on "
                    "the floor, one dusty window lets in a narrow beam of afternoon light.",
                    False),
    # —— 卷四《终章》——
    "town_dark": ("A small town at night with almost every light switched off. Seen from above, "
                  "the streets are dark and quiet, only a few distant lamps remain, the sky above "
                  "is dense with stars and the Milky Way.", False),
    "hill_night": ("A grassy hilltop above a town at night. The town spreads out far below as a "
                   "warm orange glow, a dirt path leads over the ridge, the sky is deep blue and "
                   "full of stars.", False),
    "lens_shop": ("The inside of an old optical workshop. A wooden workbench is covered with "
                  "half-finished glass lenses, polishing tools and a brass lamp, dust hangs in "
                  "the light of a small window, the walls are dark with age.", False),
    # —— 角色支线（李君浩 / 李俊辰 / 张誉腾 / 顾星禾与沈砚）——
    "clock_shop": ("The inside of a small old clock repair shop. Glass display cases hold "
                   "antique clocks and pocket watches, dozens of wall clocks hang behind the "
                   "counter, a wooden workbench sits under a green banker's lamp, everything "
                   "smells of brass and oil.", False),
    "art_room": ("A high school art room in the late afternoon. Wooden easels stand in a loose "
                 "circle, paint jars and palettes are scattered on the tables, a plaster bust "
                 "sits by the window, dust floats in the warm light.", False),
    "student_council": ("A small student council office. A long table is covered with stacked "
                        "documents and a stamp pad, filing cabinets line the wall, a notice board "
                        "is covered with pinned schedules, cold white ceiling light.", False),
    "darkroom": ("A photography darkroom lit only by a red safelight. Developing trays are "
                 "filled with liquid, wet prints hang from a line with wooden pegs, an enlarger "
                 "stands in the corner.", False),
}

# ---------------- 全屏 CG ----------------
CG = {
    # —— 第一部 ——
    "dismissal_notice": ("A close-up of a formal school club dismissal notice lying on a wooden "
                         "desk, a red wax seal pressed at the corner, the empty classroom behind "
                         "it lit by dim evening light.", False),
    "broken_telescope": ("A close-up of an old equatorial telescope, mould along the tube and a "
                         "broken drive gear with missing teeth lying on the table beside it, dust "
                         "in the air.", False),
    "gear_fixed": ("A close-up of a small brass gear fitted onto a telescope drive shaft, a hand "
                   "tool resting beside it, warm lamp light, a quiet triumphant mood.", False),
    "rain_observatory": ("The inside of a leaking observatory dome during heavy rain. Water runs "
                         "down the inner wall, seven iron buckets stand on the floor catching "
                         "drips, the light is grey and gloomy.", False),
    "old_logbook": ("An open old photo album on a desk. A faded photograph shows a group of high "
                    "school students crowded around a brand new telescope, handwritten notes fill "
                    "the margins, warm nostalgic light.", False),
    "poster_wall": ("A school corridor after the posters were torn down. Shreds of paper litter "
                    "the floor and spill out of a bin, the wall is bare, the light is cold and "
                    "early.", False),
    "clouds_parting": ("A night sky seen from below, thick clouds splitting open down the middle "
                       "to reveal a deep blue gap and stars, dramatic and still.", False),
    "starry_night": ("A vast starry night sky with the Milky Way and one bright shooting star "
                     "streaking across it, wide and quiet.", False),
    "rooftop_promise": ("Two high school students sitting side by side on a school rooftop at "
                        "night, seen from behind, festival lights below and the Milky Way above "
                        "them.", True),
    # —— 第二部 ——
    "demolition_notice": ("A close-up of a school building demolition notice on a wooden desk, a "
                          "red official stamp at the corner, cold winter light through the window.",
                          False),
    "old_blueprint": ("An old yellowed architectural blueprint of an observatory dome spread out on "
                      "a desk, faded ink, a faded photograph clipped to the corner, warm desk lamp "
                      "light.", False),
    "meteor_winter": ("A meteor shower over a snowy field at night, many shooting stars radiating "
                      "from the constellation Gemini, a snow-covered school rooftop in the "
                      "foreground.", False),
    "spring_clubroom": ("A school astronomy club room in spring, cherry blossom petals drifting "
                        "through the open window, several high school students standing together "
                        "seen from behind beside the telescope, warm sunlight.", True),
    # —— 第三部 ——
    "lightshow": ("A wide night shot of a small observatory dome wrapped in bright decorative LED "
                  "strings and floodlights, harsh colour glow washing over the roof, the stars "
                  "barely visible behind it.", False),
    "shen_yan_notebook": ("A close-up of an open hand-drawn star chart notebook on a desk, careful "
                          "pencil sketches of constellations, small neat notes in the margins, a "
                          "mechanical pencil beside it, warm desk lamp light.", False),
    "light_meter": ("Two high school students standing on an empty hill at night, seen from "
                    "behind, one of them holding a small light meter up toward the sky while the "
                    "orange glow of a town dims the stars on the horizon.", True),
    "perseid_shower": ("A meteor shower filling a deep blue night sky, dozens of bright meteors "
                       "radiating out from the constellation Perseus, the silhouette of a small "
                       "observatory dome in the foreground.", False),
    "summer_promise": ("High school students standing together on a hillside under a summer night "
                       "sky full of meteors, seen from behind, a small observatory dome beside "
                       "them, warm and quiet.", True),
    # —— 卷四《终章》——
    "lights_out": ("A wide night view of a small town as the last street lights go out. The "
                   "orange glow dies along the river, the buildings turn into dark silhouettes, "
                   "the Milky Way comes out above the rooftops.", False),
    "grad_orion": ("The constellation Orion in the southeastern sky on a clear summer night. "
                   "Three bright belt stars, faint nebula below the belt, the silhouette of a "
                   "school rooftop along the bottom edge.", False),
    # —— 角色支线 ——
    "gear_watch": ("A close-up of an old brass pocket watch lying open on a workbench, its "
                   "movement exposed, tiny gears meshing, a pair of tweezers resting beside it, "
                   "warm lamp light.", False),
    "wall_mural": ("A long outdoor wall covered by a half-finished hand-painted star map. Chalk "
                   "guide lines and paint buckets sit on the ground, a star chart drawn in "
                   "white and pale blue over dark paint, morning light.", False),
    "darkroom_print": ("A close-up of a monochrome photograph rising in a developing tray under "
                       "red light. The image shows an observatory dome against a starry sky, "
                       "the liquid rippling.", False),
}


def txt2img(pos, neg, seed, prefix):
    wf = {
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": MODEL}},
        "5": {"class_type": "EmptyLatentImage", "inputs": {"width": GEN_W, "height": GEN_H, "batch_size": 1}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": pos, "clip": ["4", 1]}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"text": neg, "clip": ["4", 1]}},
        "3": {"class_type": "KSampler",
              "inputs": {"seed": seed, "steps": STEPS, "cfg": CFG,
                         "sampler_name": "euler_ancestral", "scheduler": "normal", "denoise": 1.0,
                         "model": ["4", 0], "positive": ["6", 0], "negative": ["7", 0],
                         "latent_image": ["5", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    }
    return wait_images(queue(wf, client="scenes"))


def grade(im):
    """轻微去饱和：让整套素材的"数码味"再低一点。"""
    return ImageEnhance.Color(im).enhance(SATURATION)


def render(items, out_dir, tag, seed0, force):
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(PREVIEW, exist_ok=True)
    for i, (key, (text, people)) in enumerate(items.items()):
        dst = os.path.join(out_dir, f"{key}.png")
        if os.path.exists(dst) and not force:
            print(f"  {key:<20} 已存在，跳过", flush=True)
            continue
        pos = brief(text, people)
        neg = NEG_PPL if people else NEG
        path, dt = txt2img(pos, neg, seed=seed0 + i * 7, prefix=f"{tag}_{key}")
        im = Image.open(path).convert("RGB").resize((OUT_W, OUT_H), Image.LANCZOS)
        im = grade(im)
        im.save(dst)
        im.save(os.path.join(PREVIEW, f"{tag}_{key}.png"))
        print(f"  {key:<20} {dt:4.1f}s  → {os.path.relpath(dst, PROJ)}", flush=True)


def sheet():
    """旧画风 vs 新画风对照图（旧图来自 gen_preview/old_style，脚本跑之前会自己备份）。"""
    pairs = [("clubroom", "clubroom"), ("observatory", "observatory"), ("snow_school", "snow_school"),
             ("festival_night", "festival_night"), ("classroom", "classroom"), ("title", "title")]
    w, h = 480, 270
    out = Image.new("RGB", (w * 2, h * len(pairs)), (18, 20, 28))
    d = ImageDraw.Draw(out)
    for row, (old_id, new_id) in enumerate(pairs):
        for col, (src_dir, name, label) in enumerate([
                (OLD_DIR, old_id, "旧"), (BG_DIR, new_id, "新")]):
            p = os.path.join(src_dir, f"{name}.png")
            if not os.path.exists(p):
                continue
            im = Image.open(p).convert("RGB").resize((w, h), Image.LANCZOS)
            out.paste(im, (col * w, row * h))
            d.text((col * w + 8, row * h + 6), f"{label} · {new_id}", fill=(255, 255, 255))
    dst = os.path.join(PREVIEW, "style_before_after.png")
    out.save(dst)
    print("对照图 ->", dst, out.size)


def backup():
    """第一次换画风前，把旧图挪一份到 gen_preview/old_style（已存在就不覆盖）。"""
    import shutil
    for src_dir in (BG_DIR, CG_DIR):
        for name in os.listdir(src_dir):
            if not name.endswith(".png"):
                continue
            side = "bg" if src_dir == BG_DIR else "cg"
            dst_dir = os.path.join(OLD_DIR, side)
            os.makedirs(dst_dir, exist_ok=True)
            dst = os.path.join(dst_dir, name)
            if not os.path.exists(dst):
                shutil.copy2(os.path.join(src_dir, name), dst)
    print("旧图备份 ->", OLD_DIR, flush=True)


def main():
    argv = sys.argv[1:]
    force = "--force" in argv
    what = next((a for a in argv if not a.startswith("--")), "all")

    if "--sheet" in argv:
        sheet()
        return
    if force:
        backup()
    if what in ("all", "bg"):
        print("=== 背景 ===", flush=True)
        render(BACKGROUNDS, BG_DIR, "bg", 31001, force)
    if what in ("all", "cg"):
        print("=== CG ===", flush=True)
        render(CG, CG_DIR, "cg", 41001, force)


if __name__ == "__main__":
    main()
