#!/usr/bin/env python3
"""Empaqueta los renders de Blender (assets/render/out) en assets/ui para embeber en el plugin.

- Filmstrips de perillas: 128 cuadros → hoja 16×8.
- Fichas, LED y fondo: se copian optimizados.
- ui.json: metadatos (anclas, tamaños, proyección) que lee el editor.
"""
import json, os, shutil
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "render", "out")
DST = os.path.join(ROOT, "assets", "ui")
os.makedirs(DST, exist_ok=True)

meta = {"background": json.load(open(os.path.join(SRC, "background.json")))}
for kind in ("small", "big"):
    m = json.load(open(os.path.join(SRC, f"knob_{kind}.json")))
    n, size, cols = m["frames"], m["size"], 16
    rows = (n + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * size, rows * size), (0, 0, 0, 0))
    for f in range(n):
        im = Image.open(os.path.join(SRC, f"knob_{kind}", f"{f:03d}.png")).convert("RGBA")
        sheet.paste(im, ((f % cols) * size, (f // cols) * size))
    sheet.save(os.path.join(DST, f"knob_{kind}.png"), optimize=True)
    m["cols"] = cols
    meta[f"knob_{kind}"] = m

meta["plugs"] = json.load(open(os.path.join(SRC, "plugs.json")))
for f in os.listdir(os.path.join(SRC, "plugs")):
    Image.open(os.path.join(SRC, "plugs", f)).save(os.path.join(DST, "plug_" + f), optimize=True)
Image.open(os.path.join(SRC, "led_on.png")).save(os.path.join(DST, "led_on.png"), optimize=True)
bg = Image.open(os.path.join(SRC, "background.png")).convert("RGB")
bg.save(os.path.join(DST, "background.png"), optimize=True)
# Página EXTENSIONS (mismo pipeline que el panel principal, otra serigrafía).
EXT = os.path.join(ROOT, "assets", "render", "out_ext")
if os.path.exists(os.path.join(EXT, "background.png")):
    Image.open(os.path.join(EXT, "background.png")).convert("RGB").save(os.path.join(DST, "background_ext.png"), optimize=True)

# Teclado: arriba/apretado, hoja de la rueda, pulsador y polígonos de cada tecla.
KEYS = os.path.join(SRC, "keys")
if os.path.exists(os.path.join(SRC, "keys.json")):
    km = json.load(open(os.path.join(SRC, "keys.json")))
    for f in ("keys_up", "keys_down"):
        Image.open(os.path.join(KEYS, f + ".png")).convert("RGB").save(os.path.join(DST, f + ".png"), optimize=True)
    frames = km["wheel"]["frames"]
    first = Image.open(os.path.join(KEYS, "wheel_000.png"))
    fw, fh = first.size
    cols = 11
    sheet = Image.new("RGBA", (cols * fw, ((frames + cols - 1) // cols) * fh), (0, 0, 0, 0))
    for f in range(frames):
        sheet.paste(Image.open(os.path.join(KEYS, f"wheel_{f:03d}.png")).convert("RGBA"), ((f % cols) * fw, (f // cols) * fh))
    sheet.save(os.path.join(DST, "wheel.png"), optimize=True)
    km["wheel"].update(cols=cols, fw=fw, fh=fh)
    Image.open(os.path.join(KEYS, "switch_down.png")).save(os.path.join(DST, "switch_down.png"), optimize=True)
    meta["keys"] = km

json.dump(meta, open(os.path.join(DST, "ui.json"), "w"), indent=1)
for f in sorted(os.listdir(DST)):
    print(f"{os.path.getsize(os.path.join(DST, f)) / 1e6:6.2f} MB  {f}")
