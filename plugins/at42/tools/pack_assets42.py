#!/usr/bin/env python3
"""Empaqueta los renders de Blender (assets/render/out) en assets/ui para embeber en el plugin.

- Fondos (crema, negro, maíz, azul) sobre el color del editor, en JPEG.
- Filmstrips de perillas (128 cuadros → hoja de 16 columnas), botones apretados, palanca de las llaves,
  tecla drone apretada, capuchón del joystick y fichas.
- ui.json: proyección, layout (assets/layout42.json) y metadatos de los sprites.
"""
import json, os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "render", "out")
DST = os.path.join(ROOT, "assets", "ui")
os.makedirs(DST, exist_ok=True)
for f in os.listdir(DST): os.remove(os.path.join(DST, f))
EDITOR_BG = (14, 14, 15)

bg = json.load(open(os.path.join(SRC, "bg.json")))
layout = json.load(open(os.path.join(ROOT, "assets", "layout42.json")))
sp = json.load(open(os.path.join(SRC, "sprites.json")))

def flatten(name):
    im = Image.open(os.path.join(SRC, name)).convert("RGBA")
    out = Image.new("RGB", im.size, EDITOR_BG); out.paste(im, (0, 0), im); return out

for c in ("cream", "black", "maize", "blue"):
    flatten(f"bg_{c}.png").save(os.path.join(DST, f"bg_{c}.jpg"), quality=92, optimize=True, subsampling=0)

def fade_edges(im, border=0.14):
    """Desvanece el alfa cerca del borde del cuadro: la sombra no termina en un recuadro."""
    w, h = im.size
    mask = Image.new("L", (w, h), 0)
    from PIL import ImageDraw, ImageFilter
    b = int(min(w, h) * border)
    ImageDraw.Draw(mask).rectangle([b, b, w - 1 - b, h - 1 - b], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(b * 0.6))
    a = im.getchannel("A")
    from PIL import ImageChops
    im.putalpha(ImageChops.multiply(a, mask))
    return im

for key, m in sp["knobs"].items():
    folder = os.path.join(SRC, f"knob_{key}")
    first = Image.open(os.path.join(folder, "000.png")); s = first.size[0]
    cols = 16; rows = (m["frames"] + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * s, rows * s), (0, 0, 0, 0))
    for f in range(m["frames"]):
        sheet.paste(fade_edges(Image.open(os.path.join(folder, f"{f:03d}.png")).convert("RGBA")), ((f % cols) * s, (f // cols) * s))
    sheet.save(os.path.join(DST, f"knob_{key}.png"), optimize=True)
    m["cols"] = cols
for key, m in sp["buttons"].items():
    fade_edges(Image.open(os.path.join(SRC, m["file"] + ".png")).convert("RGBA")).save(os.path.join(DST, m["file"] + ".png"), optimize=True)
for f in ("bat_up", "bat_mid", "bat_down", "dkey_down", "joy_cap"):
    fade_edges(Image.open(os.path.join(SRC, f + ".png")).convert("RGBA")).save(os.path.join(DST, f + ".png"), optimize=True)
for f in os.listdir(os.path.join(SRC, "plugs")):
    Image.open(os.path.join(SRC, "plugs", f)).save(os.path.join(DST, "plug_" + f), optimize=True)

json.dump(dict(background=bg, layout=layout, sprites=sp), open(os.path.join(DST, "ui.json"), "w"), separators=(",", ":"))
tot = 0
for f in sorted(os.listdir(DST)): tot += os.path.getsize(os.path.join(DST, f))
print(f"{len(os.listdir(DST))} archivos, {tot / 1e6:.2f} MB")
