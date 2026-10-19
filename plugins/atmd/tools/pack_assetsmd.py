#!/usr/bin/env python3
"""Empaqueta los renders de Blender (assets/render/out) en assets/ui para embeber en el plugin ATMD.

- bg.jpg: el instrumento sin controles (teclas arriba) sobre el color del editor.
- keysdown.jpg: el teclado con todas las teclas apretadas, recortado (keys.crop).
- Hojas de sprites (PNG con alfa y sombra): perillas (128 cuadros), ruedas (33), teclas basculantes y
  lámparas (un PNG por estado). Cada sprite se recorta a su contenido; el ancla se corrige.
- ui.json: proyección de los dos planos (panel y deck), sprites, controles con su posición en px,
  polígonos de las teclas y posiciones de las ruedas. Contrato en docs/ui-assets.md.

Uso:  python3 tools/pack_assetsmd.py [--test]     (--test compone una imagen de prueba con todo encima)
"""
import json, math, os, sys
from PIL import Image, ImageFilter, ImageChops

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "render", "out")
DST = os.path.join(ROOT, "assets", "ui")
os.makedirs(DST, exist_ok=True)
EDITOR_BG = (14, 14, 15)
JPEG_Q = 92

L = json.load(open(os.path.join(ROOT, "assets", "layoutmd.json")))
BG = json.load(open(os.path.join(SRC, "bg.json")))
SP = json.load(open(os.path.join(SRC, "sprites.json")))
K = BG["pxPerMm"]
PA, PH = BG["panel"]["affine"], BG["panel"]["perMmHeight"]
DA, DH = BG["deck"]["affine"], BG["deck"]["perMmHeight"]
HT = BG["heights"]


def panel_px(x, y, h=0.0):
    return [PA[0][0] * x + PA[0][2], PA[1][1] * y + PA[1][2] + PH[1] * h]


def deck_px(x, y, z):
    return [DA[0][0] * x + DA[0][2], DA[1][1] * y + DA[1][2] + DH[1] * z]


def r1(p): return [round(p[0], 1), round(p[1], 1)]


def flatten(im):
    out = Image.new("RGB", im.size, EDITOR_BG); out.paste(im, (0, 0), im); return out


# ───── fondos ─────
bg = Image.open(os.path.join(SRC, "bg.png")).convert("RGBA")
assert bg.size == (BG["width"], BG["height"]), (bg.size, BG["width"], BG["height"])
flatten(bg).save(os.path.join(DST, "bg.jpg"), quality=JPEG_Q, optimize=True, subsampling=0)
kd = Image.open(os.path.join(SRC, "keysdown.png")).convert("RGBA")
crop = BG["keysCrop"]
assert kd.size == (crop[2] - crop[0], crop[3] - crop[1]), (kd.size, crop)
flatten(kd).save(os.path.join(DST, "keysdown.jpg"), quality=JPEG_Q, optimize=True, subsampling=0)


# ───── sprites ─────
def bbox_union(images, thr=2):
    box = None
    for im in images:
        b = im.split()[3].point(lambda v: 255 if v > thr else 0).getbbox()
        if b is None: continue
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    return box


def glow(im, color, radius_px, strength):
    """Halo de una lámpara encendida: desenfoque de los píxeles brillantes, debajo del sprite."""
    r, g, b, a = im.split()
    lum = Image.merge("RGB", (r, g, b)).convert("L").point(lambda v: 255 if v > 200 else int(v * v / 200 / 1.6) if v > 120 else 0)
    halo = lum.filter(ImageFilter.GaussianBlur(radius_px)).point(lambda v: min(255, int(v * strength)))
    layer = Image.new("RGBA", im.size, color + (0,)); layer.putalpha(halo)
    out = layer.copy(); out.alpha_composite(im)
    return out


def pack_sheet(name, cols, extra=None):
    m = SP[name]
    frames = [Image.open(os.path.join(SRC, name, f"{f:03d}.png")).convert("RGBA") for f in range(m["frames"])]
    b = bbox_union(frames)
    b = (max(0, b[0] - 1), max(0, b[1] - 1), min(frames[0].width, b[2] + 1), min(frames[0].height, b[3] + 1))
    fw, fh = b[2] - b[0], b[3] - b[1]
    rows = (m["frames"] + cols - 1) // cols
    sh = Image.new("RGBA", (cols * fw, rows * fh), (0, 0, 0, 0))
    for f, im in enumerate(frames):
        sh.paste(im.crop(b), ((f % cols) * fw, (f // cols) * fh))
    sh.save(os.path.join(DST, name + ".png"), optimize=True)
    out = dict(file=name + ".png", w=fw, h=fh, frames=m["frames"], cols=cols, rows=rows,
               anchor=[round(m["anchor"][0] - b[0], 2), round(m["anchor"][1] - b[1], 2)], plane=m["plane"])
    for k in ("degPerFrame", "values", "planeZ", "note"):
        if k in m: out[k] = m[k]
    if extra: out.update(extra)
    return out


def pack_single(name, glow_col=None):
    m = SP[name]
    im = Image.open(os.path.join(SRC, name + ".png")).convert("RGBA")
    if glow_col: im = glow(im, glow_col, 2.2 * K, 1.4)
    b = bbox_union([im])
    b = (max(0, b[0] - 1), max(0, b[1] - 1), min(im.width, b[2] + 1), min(im.height, b[3] + 1))
    im.crop(b).save(os.path.join(DST, name + ".png"), optimize=True)
    out = dict(file=name + ".png", w=b[2] - b[0], h=b[3] - b[1], frames=1, cols=1, rows=1,
               anchor=[round(m["anchor"][0] - b[0], 2), round(m["anchor"][1] - b[1], 2)], plane=m["plane"])
    if "planeZ" in m: out["planeZ"] = m["planeZ"]
    return out


sprites = {}
for n in ("knob_std", "knob_big", "knob_ptr"):
    sprites[n] = pack_sheet(n, 16)
for n in ("pitch_wheel", "mod_wheel"):
    sprites[n] = pack_sheet(n, 11)
for n in sorted(SP):
    if n.startswith("rocker_"): sprites[n] = pack_single(n)
    elif n.startswith("lamp_"):
        on = n.endswith("_1")
        col = (255, 140, 30) if "overload" in n else (255, 40, 25)
        sprites[n] = pack_single(n, col if on else None)

# ───── controles ─────
controls = []
for kn in L["knobs"]:
    spr = {"rotary-switch": "knob_ptr", "big": "knob_big"}.get(kn["kind"], "knob_std")
    c = dict(id=kn["id"], type="rotary" if kn["kind"] == "rotary-switch" else "knob", plane="panel", x=kn["x"], y=kn["y"],
             px=r1(panel_px(kn["x"], kn["y"])), sprite=spr,
             hitRadiusPx=[round(kn["r"] * K, 1), round(kn["r"] * K * PA[1][1] / K, 1)])
    if kn["kind"] == "rotary-switch": c.update(steps=kn["steps"], detentsDeg=kn["detents"])
    else: c.update(a0=kn["a0"], a1=kn["a1"], legends=kn["legends"])
    controls.append(c)


def rect_px(fn, x, y, w, h, z):
    pts = [fn(x - w / 2, y - h / 2, z), fn(x + w / 2, y - h / 2, z), fn(x + w / 2, y + h / 2, z), fn(x - w / 2, y + h / 2, z)]
    return [r1(p) for p in pts]


for s in L["switches"]:
    controls.append(dict(id=s["id"], type="rocker", plane="panel", x=s["x"], y=s["y"], px=r1(panel_px(s["x"], s["y"])),
                         sprites=[f"rocker_{s['color']}_{s['orient']}_0", f"rocker_{s['color']}_{s['orient']}_1"],
                         orient=s["orient"], hit=rect_px(lambda a, b, z: panel_px(a, b, z), s["x"], s["y"], s["w"], s["h"], 0.0)))
for s in L["deck"]["lhc"]["switches"]:
    z = HT["lhcPlate"]
    controls.append(dict(id=s["id"], type="rocker", plane="deck", z=z, x=s["x"], y=s["y"], px=r1(deck_px(s["x"], s["y"], z)),
                         sprites=["rocker_cream_h_0", "rocker_cream_h_1"], orient="h",
                         hit=rect_px(deck_px, s["x"], s["y"], s["w"], s["h"], z)))
for lp in L["lamps"]:
    controls.append(dict(id=lp["id"], type="lamp", plane="panel", x=lp["x"], y=lp["y"], px=r1(panel_px(lp["x"], lp["y"])),
                         sprites=[f"lamp_{lp['id']}_0", f"lamp_{lp['id']}_1"]))
wheels = []
for w in L["deck"]["lhc"]["wheels"]:
    z = HT["lhcPlate"]
    c = dict(id=w["id"], type="wheel", plane="deck", z=z, x=w["x"], y=w["y"], px=r1(deck_px(w["x"], w["y"], z)), sprite=w["id"],
             values=sprites[w["id"]]["values"], centerDetent=w["center_detent"],
             hit=rect_px(deck_px, w["x"], w["y"], w["slot_w"] + 4, w["slot_len"] + 6, z),
             dragPxPerFullTravel=round(w["slot_len"] * K * DA[1][1] / K, 1))
    controls.append(c); wheels.append(c)

# ───── teclas: polígonos en px del fondo ─────
nbY, nbZtop, nbZbot = HT["nameBoardFront"]
rear_sy = deck_px(0, nbY, nbZbot)[1] - 0.5                       # el canto del listón tapa lo de atrás
lip_sy = deck_px(0, L["case"]["front_lip"]["y0"], HT["lipTop"])[1]  # borde del frente de madera
wt, bt = HT["whiteKeyTop"], HT["blackKeyTop"]
keys = []
for k in BG["keys"]:
    if k["black"]:
        y_front = deck_px(0, k["y1"], wt + 1.0)[1] + 0.5
        x0, x1 = deck_px(k["x0"], 0, 0)[0], deck_px(k["x1"], 0, 0)[0]
        hull = [[x0, rear_sy], [x1, rear_sy], [x1, y_front], [x0, y_front]]
    else:
        # Contorno superior (cola angosta entre negras, cabeza ancha) y, adelante, la cara frontal hasta el
        # borde del frente de madera: así el polígono cubre la tecla arriba y apretada.
        hull = []; ol = k["outline"]; n = len(ol)
        for i, (x, y) in enumerate(ol):
            p = deck_px(x, y, wt); p[1] = max(p[1], rear_sy)
            if abs(y - k["y1"]) < 1e-6:
                if abs(ol[(i + 1) % n][1] - k["y1"]) < 1e-6: hull += [p, [p[0], lip_sy]]
                else: hull += [[p[0], lip_sy], p]
            else:
                hull.append(p)
    keys.append(dict(note=k["note"], black=k["black"], hull=[r1(p) for p in hull]))

meta = dict(
    version=1,
    generatedBy="tools/pack_assetsmd.py from tools/blender/scenemd.py renders",
    editorScale=0.5,
    background=dict(file="bg.jpg", width=BG["width"], height=BG["height"], pxPerMm=K),
    projection=dict(
        camera=dict(type="orthographic", tiltDeg=BG["tiltDeg"], viewFromVerticalDeg=BG["viewFromVerticalDeg"],
                    note="view direction = bisector of the panel normal and the vertical; both planes foreshortened by cos(viewFromVerticalDeg)"),
        panel=dict(BG["panel"], panelXOffsetMm=BG["panelXOffsetMm"]),
        deck=BG["deck"], heights=HT, x0Mm=BG["x0Mm"], s0Mm=BG["s0Mm"], checkPx=BG["projectionCheckPx"]),
    keys=dict(file="keysdown.jpg", crop=crop, firstNote=L["keyboard"]["first_note"], lastNote=L["keyboard"]["last_note"],
              hitOrder="test black keys first", keys=keys),
    sprites=sprites,
    controls=controls,
    wheels=[dict(id=w["id"], px=w["px"], hit=w["hit"]) for w in wheels],
)
json.dump(meta, open(os.path.join(DST, "ui.json"), "w"), separators=(",", ":"))

total = 0
for f in sorted(os.listdir(DST)):
    sz = os.path.getsize(os.path.join(DST, f)); total += sz
    print(f"{sz / 1e6:6.2f} MB  {f}")
print(f"{total / 1e6:6.2f} MB  total")


# ───── imagen de prueba: fondo + todos los sprites en sus valores por defecto ─────
def sprite_frame(name, f):
    s = sprites[name]; im = Image.open(os.path.join(DST, s["file"])).convert("RGBA")
    c, r = f % s["cols"], f // s["cols"]
    return im.crop((c * s["w"], r * s["h"], (c + 1) * s["w"], (r + 1) * s["h"])), s["anchor"]


def knob_angle(kn, v):
    if kn["kind"] == "rotary-switch": return kn["detents"][int(v)]
    nums = []
    for s, a in kn["legends"]:
        try: nums.append((float(s), a))
        except ValueError: pass
    mono = all(b[0] > a[0] and b[1] > a[1] for a, b in zip(nums[:-1], nums[1:]))
    if mono and len(nums) >= 2 and nums[0][0] <= v <= nums[-1][0] and not (nums[0][0] == 0 and nums[-1][0] == 10):
        (v0, a0), (v1, a1) = nums[0], nums[-1]
        return a0 + (v - v0) * (a1 - a0) / (v1 - v0)
    return kn["a0"] + (kn["a1"] - kn["a0"]) * v / 10.0


def compose(pressed=(), values=None):
    values = values or {}
    im = Image.open(os.path.join(DST, "bg.jpg")).convert("RGBA")
    if pressed:
        kdim = Image.open(os.path.join(DST, "keysdown.jpg")).convert("RGBA")
        from PIL import ImageDraw
        for k in keys:
            if k["note"] not in pressed: continue
            mask = Image.new("L", im.size, 0); ImageDraw.Draw(mask).polygon([tuple(p) for p in k["hull"]], fill=255)
            layer = Image.new("RGBA", im.size, (0, 0, 0, 0)); layer.paste(kdim, (crop[0], crop[1]))
            im.paste(layer, (0, 0), ImageChops.multiply(mask, layer.split()[3]))
    def put(name, f, px):
        spr, (ax, ay) = sprite_frame(name, f)
        im.alpha_composite(spr, (int(round(px[0] - ax)), int(round(px[1] - ay))))
    kn_by = {k["id"]: k for k in L["knobs"]}
    for c in controls:
        if c["type"] in ("knob", "rotary"):
            kn = kn_by[c["id"]]
            ang = knob_angle(kn, values.get(c["id"], kn["default"]))
            n = sprites[c["sprite"]]["frames"]
            put(c["sprite"], int(round(ang / 360.0 * n)) % n, c["px"])
        elif c["type"] == "rocker":
            src = next((s for s in L["switches"] + L["deck"]["lhc"]["switches"] if s["id"] == c["id"]))
            put(c["sprites"][int(values.get(c["id"], src["default"]))], 0, c["px"])
        elif c["type"] == "lamp":
            put(c["sprites"][1 if c["id"] == "power_led" or values.get(c["id"]) else 0], 0, c["px"])
        elif c["type"] == "wheel":
            s = sprites[c["sprite"]]; v = values.get(c["id"], 0.0); v0, v1 = s["values"]
            put(c["sprite"], int(round((v - v0) / (v1 - v0) * (s["frames"] - 1))), c["px"])
    return im.convert("RGB")


if "--test" in sys.argv:
    compose().save(os.path.join(SRC, "composite_test.jpg"), quality=92)
    compose(pressed=(48, 54, 60, 61, 63, 66, 72, 84), values=dict(pitch_wheel=0.6, mod_wheel=0.8, overload=1,
            mix_osc2_on=1, glide_on=1, osc1_wave=0, osc2_range=5, tune=2.0, filter_cutoff=-3.0)).save(
        os.path.join(SRC, "composite_test2.jpg"), quality=92)
    print("test:", os.path.join(SRC, "composite_test.jpg"))
