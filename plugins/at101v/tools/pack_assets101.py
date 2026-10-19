#!/usr/bin/env python3
"""Empaqueta los renders de Blender (assets/render/out) en assets/ui para embeber en el plugin.

- Fondos (gris, azul, rojo) sobre el color del editor, en JPEG; teclas apretadas recortadas.
- Filmstrips: perilla (128 cuadros → hoja 16×8) y palanca del bender (21 cuadros).
- ui.json: proyección, layout del panel (assets/layout101.json), sprites y polígonos de las teclas.
"""
import json, math, os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "render", "out")
DST = os.path.join(ROOT, "assets", "ui")
os.makedirs(DST, exist_ok=True)
EDITOR_BG = (14, 14, 15)

bg = json.load(open(os.path.join(SRC, "bg.json")))
layout = json.load(open(os.path.join(ROOT, "assets", "layout101.json")))
sprites = json.load(open(os.path.join(SRC, "sprites.json")))
PX, X0, Y0, C, S = bg["pxmm"], bg["x0"], bg["y0"], bg["cosT"], bg["sinT"]

def to_px(x, y, z=0.0):
    return [round((x - X0) * PX, 1), round(((y - Y0) * C - z * S) * PX, 1)]

def flatten(name):
    im = Image.open(os.path.join(SRC, name)).convert("RGBA")
    out = Image.new("RGB", im.size, EDITOR_BG)
    out.paste(im, (0, 0), im)
    return out

for color in ("grey", "blue", "red"):
    flatten(f"bg_{color}.png").save(os.path.join(DST, f"bg_{color}.jpg"), quality=93, optimize=True, subsampling=0)

# Teclado: sólo hace falta la zona de las teclas de la imagen "todas abajo".
kb = layout["keyboard"]
kx0, ky0 = to_px(kb["x0"] - 2, kb["y0"] - 2)
kx1, ky1 = to_px(kb["x1"] + 2, layout["size"][1] + 2, -16)
crop = [int(kx0), int(ky0), int(math.ceil(kx1)), int(math.ceil(ky1))]
flatten("keysdown.png").crop(tuple(crop)).save(os.path.join(DST, "keysdown.jpg"), quality=93, optimize=True, subsampling=0)

hulls = []
for k in kb["keys"]:
    top = bg["blackTop"] if k["black"] else bg["whiteTop"]
    x0, x1, y0, y1 = k["x0"], k["x1"], kb["y0"], k["y1"]
    pts = [to_px(x0, y0, top), to_px(x1, y0, top), to_px(x1, y1, top), to_px(x1, y1, top - 5.5), to_px(x0, y1, top - 5.5), to_px(x0, y1, top)]
    hulls.append(dict(note=k["note"], black=k["black"], hull=pts))

def sheet(folder, frames, cols, name):
    first = Image.open(os.path.join(SRC, folder, "000.png"))
    w, h = first.size
    rows = (frames + cols - 1) // cols
    sh = Image.new("RGBA", (cols * w, rows * h), (0, 0, 0, 0))
    for f in range(frames):
        sh.paste(Image.open(os.path.join(SRC, folder, f"{f:03d}.png")).convert("RGBA"), ((f % cols) * w, (f // cols) * h))
    sh.save(os.path.join(DST, name), optimize=True)
    return cols

sprites["knob"]["cols"] = sheet("knob", sprites["knob"]["frames"], 16, "knob.png")
sprites["lever"]["cols"] = sheet("lever", sprites["lever"]["frames"], 7, "lever.png")
for f in ("cap_green", "cap_orange", "swcap_yellow", "nub", "button_down", "power_down", "led_on"):
    Image.open(os.path.join(SRC, f + ".png")).save(os.path.join(DST, f + ".png"), optimize=True)

meta = dict(background=bg, layout=layout, sprites=sprites, keys=dict(crop=crop, keys=hulls),
            knobAngles=dict(wave=[-55, 55], range=[-50, 50], other=[-150, 150]))
json.dump(meta, open(os.path.join(DST, "ui.json"), "w"), separators=(",", ":"))
total = 0
for f in sorted(os.listdir(DST)):
    sz = os.path.getsize(os.path.join(DST, f)); total += sz
    print(f"{sz / 1e6:6.2f} MB  {f}")
print(f"{total / 1e6:6.2f} MB  total")
