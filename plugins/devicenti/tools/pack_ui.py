#!/usr/bin/env python3
"""DeviceNTI V: the Blender UI renders (tools/blender/devicenti.py --mode ui) → assets/ui.

    python3 tools/pack_ui.py <render dir>

Writes assets/ui/bg.jpg (the unit with keys up, LEDs off, screens dark, knobs without pointer), assets/ui/sprites.png
(one atlas with every key pressed and lit and every LED green and red, cropped from the full renders at the same
pixels; keys come from two renders with every other key changed, so no crop shows a changed neighbour) and assets/ui/ui.json (the render's projection data plus, per control, where its sprites sit in the atlas).
The editor draws the background, puts a sprite over its crop rectangle when a key is down or lit or an LED is on,
draws knob pointers and screens through the plane homographies, and hit-tests through their inverses.
"""
import json, os, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "blender", "out")
dst = os.path.join(HERE, "..", "assets", "ui")
os.makedirs(dst, exist_ok=True)

ui = json.load(open(os.path.join(src, "ui.json")))
bg = Image.open(os.path.join(src, "bg.png")).convert("RGB")
# Keys were rendered in two passes (every other key pressed / lit): a key takes its sprite from its own pass.
files = {"down_a": "keys_down_a.png", "down_b": "keys_down_b.png", "lit_a": "keys_lit_a.png", "lit_b": "keys_lit_b.png", "green": "leds_green.png", "red": "leds_red.png"}
imgs = {k: Image.open(os.path.join(src, v)).convert("RGB") for k, v in files.items()}
keys = [c["id"] for c in ui["controls"] if c["kind"] == "key"]
def source(c, v):
    if v in ("down", "lit"): return imgs[v + ("_a" if keys.index(c["id"]) % 2 == 0 else "_b")]
    return imgs[v]

# Shelf-pack every crop into one atlas.
items = []
for c in ui["controls"]:
    if c["kind"] == "key": items += [(c, "down"), (c, "lit")]
    elif c["kind"] == "led": items += [(c, "green"), (c, "red")]
items.sort(key=lambda it: -it[0]["crop"][3])
W = 2048; x = y = shelf = 0; place = []
for c, v in items:
    w, h = c["crop"][2], c["crop"][3]
    if x + w > W: x = 0; y += shelf + 2; shelf = 0
    place.append((c, v, x, y)); x += w + 2; shelf = max(shelf, h)
H = y + shelf
atlas = Image.new("RGB", (W, H), (0, 0, 0))
for c, v, ax, ay in place:
    cx, cy, w, h = c["crop"]
    atlas.paste(source(c, v).crop((cx, cy, cx + w, cy + h)), (ax, ay))
    c.setdefault("sprites", {})[v] = [ax, ay, w, h]

bg.save(os.path.join(dst, "bg.jpg"), quality=92, optimize=True, progressive=False, subsampling=0)
atlas.save(os.path.join(dst, "sprites.png"), optimize=True)
json.dump(ui, open(os.path.join(dst, "ui.json"), "w"), separators=(",", ":"))
for f in ("bg.jpg", "sprites.png", "ui.json"):
    print(f, os.path.getsize(os.path.join(dst, f)), "bytes")
print("atlas", W, "x", H, "with", len(place), "sprites")
