#!/usr/bin/env python3
"""Empaqueta los renders de Blender (assets/render/out) en assets/ui para embeber en el plugin.

- Fondo de los tres gabinetes sobre el color del editor, en JPEG (4 px/mm: pantalla 2x).
- Filmstrips de perillas (128 cuadros → hoja de 16 columnas) a 4 px/mm, tapas de fader, botones de
  las llaves deslizantes, bola de los selectores de onda, palancas, botones apretados y fichas de 1/4".
- ui.json: proyección, layout (assets/layout700.json) y metadatos de los sprites.
"""
import json, os
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "render", "out")
DST = os.path.join(ROOT, "assets", "ui")
os.makedirs(DST, exist_ok=True)
for f in os.listdir(DST): os.remove(os.path.join(DST, f))
EDITOR_BG = (14, 14, 15)
SCALE = 0.5            # los sprites se renderizan a 8 px/mm; el fondo está a 4

bg = json.load(open(os.path.join(SRC, "bg.json")))
layout = json.load(open(os.path.join(ROOT, "assets", "layout700.json")))
sp = json.load(open(os.path.join(SRC, "sprites.json")))

im = Image.open(os.path.join(SRC, "bg.png")).convert("RGBA")
out = Image.new("RGB", im.size, EDITOR_BG); out.paste(im, (0, 0), im)
out.save(os.path.join(DST, "bg.jpg"), quality=90, optimize=True, subsampling=0)

def fade_edges(im, border=0.14):
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
for nm in ("fader_v", "fader_h", "nub_v", "nub_h", "lever", "bat"):
    meta[nm] = scale_meta(sp[nm])
for f in ("fader_v", "fader_h", "nub_v", "nub_h", "lever", "bat_up", "bat_mid", "bat_down", "bat_left", "bat_right"):
    shrink(fade_edges(Image.open(os.path.join(SRC, f + ".png")).convert("RGBA"))).save(os.path.join(DST, f + ".png"), optimize=True)
for key, m in sp["buttons"].items():
    shrink(fade_edges(Image.open(os.path.join(SRC, m["file"] + ".png")).convert("RGBA"))).save(os.path.join(DST, m["file"] + ".png"), optimize=True)
    meta["buttons"][key] = scale_meta(m)
for f in os.listdir(os.path.join(SRC, "plugs")):
    shrink(Image.open(os.path.join(SRC, "plugs", f)).convert("RGBA")).save(os.path.join(DST, "plug_" + f), optimize=True)
meta["plugs"] = scale_meta(sp["plugs"])

json.dump(dict(background=bg, layout=layout, sprites=meta), open(os.path.join(DST, "ui.json"), "w"), separators=(",", ":"))
tot = sum(os.path.getsize(os.path.join(DST, f)) for f in os.listdir(DST))
print(f"{len(os.listdir(DST))} archivos, {tot / 1e6:.2f} MB")
