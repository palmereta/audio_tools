#!/usr/bin/env python3
"""Empaqueta los renders de Blender (assets/render/out) en assets/ui para embeber en el plugin.

- Frente de 1U sobre el color del editor, en JPEG (8 px/mm; el editor lo muestra a ~3,3 px/mm y a 2x en retina).
- Filmstrips de perillas (128 cuadros → hoja de 16 columnas), botones apretados y el LED encendido, a 8 px/mm.
- ui.json: proyección, layout (assets/layoutfs1.json) y metadatos de los sprites.
"""
import json, os
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "render", "out")
DST = os.path.join(ROOT, "assets", "ui")
os.makedirs(DST, exist_ok=True)
for f in os.listdir(DST): os.remove(os.path.join(DST, f))
EDITOR_BG = (17, 22, 28)
SCALE = 0.5

bg = json.load(open(os.path.join(SRC, "bg.json")))
layout = json.load(open(os.path.join(ROOT, "assets", "layoutfs1.json")))
sp = json.load(open(os.path.join(SRC, "sprites.json")))

im = Image.open(os.path.join(SRC, "bg.png")).convert("RGBA")
out = Image.new("RGB", im.size, EDITOR_BG); out.paste(im, (0, 0), im)
out.save(os.path.join(DST, "bg.jpg"), quality=90, optimize=True, subsampling=0)

def fade_edges(im, border=0.1):
    w, h = im.size
    mask = Image.new("L", (w, h), 0)
    b = int(min(w, h) * border)
    ImageDraw.Draw(mask).rectangle([b, b, w - 1 - b, h - 1 - b], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(b * 0.6))
    im.putalpha(ImageChops.multiply(im.getchannel("A"), mask))
    return im

def shrink(im):
    return im.resize((max(1, round(im.width * SCALE)), max(1, round(im.height * SCALE))), Image.LANCZOS)

def scale_meta(m):
    m = dict(m)
    for k in ("w", "h", "size", "anchorX", "anchorY"):
        if k in m: m[k] = m[k] * SCALE
    return m

meta = {"knobs": {}, "buttons": {}}
for key, m in sp["knobs"].items():
    folder = os.path.join(SRC, f"knob_{key}")
    frames = [shrink(fade_edges(Image.open(os.path.join(folder, f"{f:03d}.png")).convert("RGBA"))) for f in range(m["frames"])]
    s = frames[0].size[0]
    cols = 16; rows = (m["frames"] + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * s, rows * s), (0, 0, 0, 0))
    for f, fr in enumerate(frames): sheet.paste(fr, ((f % cols) * s, (f // cols) * s))
    sheet.save(os.path.join(DST, f"knob_{key}.png"), optimize=True)
    mm = scale_meta(m); mm["size"] = s; mm["cols"] = cols
    meta["knobs"][key] = mm
for key, m in sp["buttons"].items():
    shrink(fade_edges(Image.open(os.path.join(SRC, key + ".png")).convert("RGBA"))).save(os.path.join(DST, key + ".png"), optimize=True)
    meta["buttons"][key] = scale_meta(m)
shrink(fade_edges(Image.open(os.path.join(SRC, "led_on.png")).convert("RGBA"), 0.05)).save(os.path.join(DST, "led_on.png"), optimize=True)
meta["led_on"] = scale_meta(sp["led_on"])

json.dump(dict(background=bg, layout=layout, sprites=meta), open(os.path.join(DST, "ui.json"), "w"), indent=1)
tot = sum(os.path.getsize(os.path.join(DST, f)) for f in os.listdir(DST))
print("assets/ui:", len(os.listdir(DST)), "archivos,", round(tot / 1e6, 2), "MB")
