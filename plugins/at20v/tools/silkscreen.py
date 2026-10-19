#!/usr/bin/env python3
"""Serigrafía del panel AT20V (tinta blanca sobre transparente) a partir de assets/layout.json.

Dibuja títulos, escalas de perillas, rótulos de jacks, el diagrama de bloques de la patchera,
divisiones y el logo. Sale a assets/render/silkscreen.png a 4x (6400 px de ancho), que Blender
usa como máscara de tinta. Las posiciones del diagrama están en píxeles de la foto de
referencia (ver tools/layout.py) y se convierten con la misma escala.
"""
import json, math, os, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import layout as LY

SS = 4                                  # supermuestreo respecto de las unidades de panel
EXT = "--ext" in sys.argv               # página EXTENSIONS (vocoder, polifonía, efectos, sistema)
D = json.load(open(os.path.join(ROOT, "assets", "layout_ext.json" if EXT else "layout.json")))
W, H = int(D["panel"]["w"] * SS), int(round(D["panel"]["h"] * SS))
K = LY.S * SS                           # px de foto → px de esta imagen
INK = (255, 255, 255, 255)
FONT = "/System/Library/Fonts/HelveticaNeue.ttc"
COND = 0.70                             # la tipografía del MS-20 es una grotesca algo condensada

img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
dr = ImageDraw.Draw(img)

def fx(x): return (x - LY.PX0) * K
def fy(y): return (y - LY.PY0) * K
def U(v): return v * SS                 # unidades de panel → px

_fonts = {}
def font(size, idx=10):
    key = (int(size), idx)
    if key not in _fonts: _fonts[key] = ImageFont.truetype(FONT, int(size), index=idx)
    return _fonts[key]

FALLBACK = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"

def text(s, cx, cy, size, align="center", idx=10, cond=COND, track=0.04):
    """Texto condensado: se dibuja en una capa aparte y se comprime en x."""
    f = font(size, idx)
    fb = ImageFont.truetype(FALLBACK, int(size))
    lines = s.split("\n")
    lh = size * 1.08
    for li, line in enumerate(lines):
        # tracking manual: letra por letra
        fonts = [fb if c in "∝" else f for c in line]
        widths = [ff.getlength(c) for ff, c in zip(fonts, line)]
        tw = sum(widths) + track * size * max(0, len(line) - 1)
        layer = Image.new("RGBA", (int(tw + size), int(size * 1.4)), (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        x = size * 0.5
        for c, w, ff in zip(line, widths, fonts):
            ld.text((x, size * 0.2), c, font=ff, fill=INK)
            x += w + track * size
        nw = max(1, int(layer.width * cond))
        layer = layer.resize((nw, layer.height), Image.LANCZOS)
        y = cy - lh * len(lines) / 2 + li * lh - size * 0.2
        real_w = tw * cond
        if align == "center": x0 = cx - real_w / 2 - size * 0.5 * cond
        elif align == "left": x0 = cx - size * 0.5 * cond
        else: x0 = cx - real_w - size * 0.5 * cond
        img.alpha_composite(layer, (int(round(x0)), int(round(y))))

def line(pts, w=2.2):
    dr.line([(fx(x), fy(y)) for x, y in pts], fill=INK, width=max(1, int(round(w * K))), joint="curve")

def arrow(x0, y0, x1, y1, w=2.2, head=11):
    line([(x0, y0), (x1, y1)], w)
    a = math.atan2(y1 - y0, x1 - x0)
    h = head
    p1 = (x1 - h * math.cos(a - 0.45), y1 - h * math.sin(a - 0.45))
    p2 = (x1 - h * math.cos(a + 0.45), y1 - h * math.sin(a + 0.45))
    dr.polygon([(fx(x1), fy(y1)), (fx(p1[0]), fy(p1[1])), (fx(p2[0]), fy(p2[1]))], fill=INK)

def box(x0, y0, x1, y1, label, size=27, w=2.6):
    dr.rectangle([fx(x0), fy(y0), fx(x1), fy(y1)], outline=INK, width=int(round(w * K)))
    text(label, fx(x0 + 14), fy((y0 + y1) / 2), size * K, align="left")

def tri(x0, y0, x1, y1, label="", size=27):
    pts = [(x0, y0), (x1, (y0 + y1) / 2), (x0, y1)]
    dr.polygon([(fx(x), fy(y)) for x, y in pts], outline=INK, width=int(round(2.4 * K)))
    if label: text(label, fx(x0 + 8), fy((y0 + y1) / 2), size * K, align="left")

# ── Símbolos de forma de onda (trazos en un cuadro unitario, y hacia abajo) ──
SYMS = {
    "tri":    [[(0, 1), (0.5, 0), (1, 1)]],
    "saw":    [[(0, 1), (0.15, 0), (1, 1)]],
    "square": [[(0, 1), (0.15, 1), (0.15, 0), (0.75, 0), (0.75, 1), (1, 1)]],
    "pulse":  [[(0, 1), (0.25, 1), (0.25, 0), (0.45, 0), (0.45, 1), (1, 1)]],
    "line":   [[(0, 1), (1, 1)]],
    "noise":  [[(0, .7), (.08, .2), (.16, .8), (.24, .1), (.32, .9), (.4, .3), (.48, .7), (.56, .0),
                (.64, .8), (.72, .2), (.8, .9), (.88, .3), (1, .6)]],
    "pwm":    [[(0, 1), (0, 0), (.3, 0), (.3, 1), (.42, 1)], [(.58, 1), (.7, 1), (.7, 0), (.82, 0), (.82, 1), (1, 1)],
               [(.33, .5), (.67, .5)]],
    "mgtop":  [[(0, 1), (0, 0), (.3, 0), (.3, 1), (.6, 1), (.6, 0), (.9, 0)], [(.2, 1.6), (.45, 1.25), (.7, 1.6)]],
    "mgleft": [[(0, 1), (0, 0), (.15, 0), (.15, 1), (.45, 1), (.45, 0), (.55, 0)], [(.62, 0), (.95, 1)]],
    "mgright":[[(.05, 1), (.38, 0)], [(.45, 1), (.45, 0), (.75, 0), (.75, 1), (.85, 1), (.85, 0), (1, 0)]],
}

def symbol(name, cx, cy, size):
    """size en px de imagen (alto del símbolo)."""
    w = size * (1.6 if name in ("noise", "pwm", "mgtop", "mgleft", "mgright") else 1.0)
    for stroke in SYMS[name]:
        pts = [(cx - w / 2 + x * w, cy - size / 2 + y * size) for x, y in stroke]
        dr.line(pts, fill=INK, width=max(2, int(size * 0.09)), joint="curve")

# ── Escalas de perillas ──
def knob_scale(k):
    cx, cy = U(k["pos"][0]), U(k["pos"][1])
    r = U(k["radius"])
    big = k["kind"] == "big"
    a0, a1 = math.radians(k["startDeg"]), math.radians(k["endDeg"])
    r_in, r_out = r * 1.10, r * (1.26 if big else 1.30)
    r_txt = r * (1.43 if big else 1.50)
    num_size = (24 if big else 21) * K
    tickw = max(2, int(2.6 * K))
    for s in k["scale"]:
        a = a0 + (a1 - a0) * s["t"]
        sx, sy = math.sin(a), -math.cos(a)
        t = s["text"]
        if k["steps"] > 0 and t:          # selectores: marca larga hasta el símbolo
            dr.line([(cx + sx * r_in, cy + sy * r_in), (cx + sx * r * 1.42, cy + sy * r * 1.42)], fill=INK, width=tickw)
            tx, ty = cx + sx * r * 1.72, cy + sy * r * 1.72
        else:
            dr.line([(cx + sx * r_in, cy + sy * r_in), (cx + sx * r_out, cy + sy * r_out)], fill=INK, width=tickw)
            tx, ty = cx + sx * r_txt, cy + sy * r_txt
        if not t: continue
        if t.startswith("sym:"):
            nm = t[4:]
            symbol(nm, tx, ty, num_size * (0.9 if nm in ("tri", "saw", "square", "pulse", "line") else 0.8))
        else:
            text(t, tx, ty, num_size * (1.15 if "′" in t or t == "RING" else 1.0), cond=0.92, track=0.0)
    if k["label"]:
        ly = cy + r * (1.42 if big else 1.62)
        text(k["label"], cx, ly, 28 * K)

for k in D["knobs"]:
    knob_scale(k)

# ── Títulos y divisiones ──
for t in D["titles"]:
    x, y = U(t["pos"][0]), U(t["pos"][1])
    text(t["text"], x, y, t["size"] * SS * 1.08, align=t["align"], cond=0.66, track=0.07)
for d in D["dividers"]:
    dr.line([(U(x), U(y)) for x, y in d], fill=INK, width=int(round(8 * K)), joint="curve")

if EXT:
    # LED de señal de entrada del vocoder y placa de marca.
    l = D["leds"][0]
    text("SIGNAL", U(l["pos"][0]) - 16 * K, U(l["pos"][1]), 22 * K, align="right")
    lx, ly = U(1245 + 60 + 2 * 118) - 10 * K, U(442)
    f_logo = font(72 * K, 9)
    logo = Image.new("RGBA", (int(400 * K), int(110 * K)), (0, 0, 0, 0))
    ImageDraw.Draw(logo).text((0, 0), "AT20V", font=f_logo, fill=(236, 236, 236, 255))
    logo = logo.resize((int(logo.width * 0.92), logo.height), Image.LANCZOS)
    img.alpha_composite(logo, (int(lx - 60 * K), int(ly - 60 * K)))
    text("AUDIO TOOLS", lx - 58 * K, ly + 40 * K, 30 * K, align="left", idx=0, cond=0.85, track=0.12)
    out_dir = os.path.join(ROOT, "assets", "render")
    os.makedirs(out_dir, exist_ok=True)
    img.save(os.path.join(out_dir, "silkscreen_ext.png"))
    prev = Image.new("RGBA", (W, H), (40, 40, 42, 255)); prev.alpha_composite(img)
    for k in D["knobs"]:
        cx, cy, r = U(k["pos"][0]), U(k["pos"][1]), U(k["radius"])
        ImageDraw.Draw(prev).ellipse([cx - r, cy - r, cx + r, cy + r], fill=(14, 14, 14, 255))
    prev.convert("RGB").resize((W // 2, H // 2), Image.LANCZOS).save(os.path.join(out_dir, "silkscreen_ext_preview.jpg"), quality=90)
    print("ok ext", W, H)
    sys.exit(0)

# ── Rótulos de jacks ──
for j in D["jacks"]:
    cx, cy = U(j["pos"][0]), U(j["pos"][1])
    if j["top"]:
        n = j["top"].count(" ") if j["top"] in ("VCO 2 CV IN", "VCO 1+2 CV IN", "KBD CV OUT", "CONTROL IN PUT", "EG 1 TRIG IN", "F∝V CV OUT") else -1
        label = j["top"]
        if n >= 0:  # dos renglones como en el original
            parts = {"VCO 2 CV IN": "VCO 2\nCV IN", "VCO 1+2 CV IN": "VCO 1+2\nCV IN", "KBD CV OUT": "KBD\nCV OUT",
                     "CONTROL IN PUT": "CONTROL\nIN PUT", "EG 1 TRIG IN": "EG 1\nTRIG IN", "F∝V CV OUT": "F∝V\nCV OUT"}
            label = parts[label]
            text(label, cx, cy - 108 * K, 27 * K)
        else:
            text(label, cx, cy - 86 * K, 27 * K)
    if j["bottom"]:
        text(j["bottom"], cx, cy + 92 * K, 27 * K)

# ── Diagrama de bloques de la patchera (px de foto) ──
box(4327, 1092, 4528, 1191, "VOLTAGE\nCONTROLLED\nOSCILLATOR 1", 24)
box(4582, 1092, 4783, 1191, "VOLTAGE\nCONTROLLED\nOSCILLATOR 2", 24)
box(4849, 1092, 4958, 1191, "VCO\nMIXER", 24)
box(5026, 1093, 5222, 1194, "VOLTAGE\nCONTROLLED\nHP FILTER", 24)
box(5280, 1093, 5484, 1194, "VOLTAGE\nCONTROLLED\nLP FILTER", 24)
box(5533, 1095, 5730, 1196, "VOLTAGE\nCONTROLLED\nAMPLIFIER", 24)
line([(4528, 1140), (4548, 1140), (4548, 1078), (4800, 1078), (4800, 1118)]); arrow(4800, 1118, 4849, 1118)
arrow(4783, 1165, 4849, 1165)
arrow(4958, 1142, 5026, 1142); arrow(5222, 1142, 5280, 1142); arrow(5484, 1142, 5533, 1142)
arrow(5730, 1142, 5815, 1142)
arrow(6075, 1142, 6185, 1142)                                  # VCA → SIGNAL OUT (pasa por el volumen)
line([(4393, 1288), (4393, 1233), (5369, 1233)])               # bus TOTAL
arrow(4431, 1233, 4431, 1194); arrow(4673, 1233, 4673, 1194)
line([(4673, 1288), (4673, 1250), (4431, 1250)])               # FREQ
line([(4902, 1288), (4902, 1255), (4985, 1255), (4985, 1178)]); arrow(4985, 1178, 5026, 1178)
arrow(5123, 1288, 5123, 1196); arrow(5369, 1288, 5369, 1196); arrow(5632, 1288, 5632, 1198)

box(4480, 1582, 4681, 1683, "MODULATION\nGENERATOR", 24)
box(5147, 1583, 5346, 1684, "ENVELOPE\nGENERATOR\n1", 24)
box(5428, 1584, 5628, 1686, "ENVELOPE\nGENERATOR\n2", 24)
line([(4440, 1633), (4480, 1633)]); line([(4464, 1633), (4464, 1523), (4393, 1523)]); arrow(4393, 1523, 4393, 1492)
line([(4681, 1633), (4735, 1633)])
line([(5023, 1633), (5147, 1633)])
line([(5070, 1633), (5070, 1524), (4673, 1524)]); arrow(4673, 1524, 4673, 1492)
line([(5070, 1633), (5070, 1880)]); arrow(5070, 1880, 5120, 1880)
line([(4975, 1680), (4975, 1784)])
line([(5428, 1660), (5407, 1660), (5407, 1524), (5123, 1524)]); arrow(5123, 1524, 5123, 1492); arrow(5369, 1524, 5369, 1492)
line([(5407, 1560), (5470, 1560), (5470, 1262), (5632, 1262)])
line([(5628, 1633), (5690, 1633)])
arrow(5274, 1831, 5274, 1688); line([(5274, 1831), (5432, 1831)])
line([(5530, 1831), (6040, 1831)]); arrow(5603, 1831, 5603, 1690)
box(6132, 1716, 6231, 1760, "KBD", 24)
line([(6140, 1831), (6235, 1831)]); line([(6182, 1831), (6182, 1760)])
arrow(6140, 1633, 6235, 1633); line([(6182, 1633), (6182, 1716)])
line([(6040, 1633), (5990, 1633), (5990, 1570)]); arrow(5990, 1570, 5945, 1570)
text("VCO 1", fx(5905), fy(1570), 26 * K)
line([(5950, 1733), (5990, 1733), (5990, 1633)])
line([(5860, 1784), (5860, 1830)]); arrow(5860, 1830, 5830, 1830)
text("VCO 2", fx(5790), fy(1830), 26 * K)

box(4480, 1975, 4679, 2076, "SAMPLE\n& HOLD", 24)
arrow(4588, 1880, 4588, 1975); arrow(4440, 2028, 4480, 2028); line([(4679, 2028), (4735, 2028)])
tri(5076, 1975, 5163, 2078, "VCA", 22)
arrow(5020, 2028, 5076, 2028); line([(5163, 2028), (5230, 2028)])
line([(5170, 1880), (5170, 1920), (5110, 1920)]); arrow(5110, 1920, 5110, 1990)
box(5570, 1975, 5769, 2077, "NOISE\nGENERATOR", 24)
line([(5527, 2029), (5570, 2029)]); line([(5769, 2029), (5822, 2029)])

tri(4535, 2249, 4627, 2351, "AMP", 22)
line([(4436, 2300), (4535, 2300)]); line([(4627, 2300), (4735, 2300)])
box(4873, 2252, 5068, 2351, "BAND PASS\nFILTER", 24)
line([(4825, 2300), (4873, 2300)]); line([(5068, 2300), (5125, 2300)])
box(5269, 2252, 5474, 2351, "F-V\nCONVERTER", 24)
line([(5220, 2300), (5269, 2300)]); line([(5474, 2300), (5520, 2300)])
box(5727, 2254, 5925, 2352, "ENVELOPE\nFOLLOWER", 24)
line([(4660, 2300), (4660, 2385), (5640, 2385), (5640, 2300)]); arrow(5640, 2300, 5727, 2300)
line([(5925, 2300), (6035, 2300)]); line([(6130, 2300), (6230, 2300)])
text("PEAK", fx(5829), fy(2215), 27 * K)

# Rótulos chicos junto a salidas.
symbol("noise", fx(5560), fy(1943), 20 * K)
symbol("noise", fx(5950), fy(1943), 20 * K)
for gx, gy in [(4600, 1905), (5490, 1905), (6020, 1905), (6295, 1905), (6290, 2105), (6290, 2380)]:
    text("GND", fx(gx + 80), fy(gy), 22 * K, align="left")
    line([(gx, gy - 14), (gx + 22, gy - 14), (gx + 22, gy + 2)]); line([(gx + 12, gy + 2), (gx + 32, gy + 2)])
text("+2.5V\n−2.5V", fx(4470), fy(1745), 20 * K, align="left")
symbol("saw", fx(4300), fy(1745), 18 * K); symbol("tri", fx(4345), fy(1745), 18 * K); symbol("saw", fx(4390), fy(1745), 18 * K)
text("+5V\n0V", fx(4770), fy(1745), 20 * K, align="left"); symbol("pwm", fx(4690), fy(1745), 18 * K)
text("+5V\n0V", fx(4835), fy(1905), 20 * K, align="left")
symbol("line", fx(6075), fy(1945), 18 * K)
text("−5V~+5V", fx(5975), fy(2098), 27 * K)
dr.arc([fx(6045), fy(1925), fx(6120), fy(1985)], 180, 360, fill=INK, width=int(2.4 * K))
dr.rectangle([fx(6265), fy(1935), fx(6295), fy(1945)], fill=INK); dr.rectangle([fx(6276), fy(1945), fx(6284), fy(1958)], fill=INK)

# ── Logo propio (no es la marca original) ──
lx, ly = fx(5770), fy(2470)
f_logo = font(150 * K, 9)
logo = Image.new("RGBA", (int(700 * K), int(220 * K)), (0, 0, 0, 0))
ImageDraw.Draw(logo).text((0, 0), "AT20V", font=f_logo, fill=(236, 236, 236, 255))
logo = logo.resize((int(logo.width * 0.92), logo.height), Image.LANCZOS)
img.alpha_composite(logo, (int(lx), int(ly - 30 * K)))
text("SYNTHESIZER", lx + 4 * K, fy(2630), 66 * K, align="left", idx=0, cond=0.80, track=0.0)

out_dir = os.path.join(ROOT, "assets", "render")
os.makedirs(out_dir, exist_ok=True)
img.save(os.path.join(out_dir, "silkscreen.png"))

# Vista previa sobre gris del panel, para comparar con la foto.
prev = Image.new("RGBA", (W, H), (40, 40, 42, 255))
prev.alpha_composite(img)
for k in D["knobs"]:
    cx, cy, r = U(k["pos"][0]), U(k["pos"][1]), U(k["radius"])
    ImageDraw.Draw(prev).ellipse([cx - r, cy - r, cx + r, cy + r], fill=(14, 14, 14, 255))
for j in D["jacks"]:
    cx, cy, r = U(j["pos"][0]), U(j["pos"][1]), U(D["jackRadius"])
    ImageDraw.Draw(prev).ellipse([cx - r, cy - r, cx + r, cy + r], fill=(170, 170, 170, 255))
    ImageDraw.Draw(prev).ellipse([cx - r * .45, cy - r * .45, cx + r * .45, cy + r * .45], fill=(5, 5, 5, 255))
prev.convert("RGB").resize((W // 2, H // 2), Image.LANCZOS).save(os.path.join(out_dir, "silkscreen_preview.jpg"), quality=90)
print("ok", W, H)
