#!/usr/bin/env python3
"""Panel del AT101V: fuente única de verdad del layout y de la serigrafía.

Medido sobre la vista cenital CC BY-SA 3.0 de docs/ref (2398x1312 px = 570x311 mm) con grilla en mm
y contrastado con la tabla de docs/hardware-spec.md §1. Todas las coordenadas están en mm desde la
esquina trasera izquierda del instrumento (x a la derecha, y hacia el frente).

Salidas:
  assets/layout101.json            (para Blender)
  (el editor lee el layout de assets/ui/ui.json, que arma tools/pack_assets101.py)
  assets/render/silkscreen101.png  (tinta blanca sobre transparente, 12 px/mm)
"""
import json, math, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W_MM, H_MM = 570.0, 311.0

# ───── controles ─────
SLIDER_Y10, SLIDER_Y0 = 73.2, 103.7          # línea del cap en 10 y en 0
BEND_Y10, BEND_Y0 = 230.6, 250.5
SLIDERS = [  # id, x, color de la línea, rótulo
    ("lfo_rate", 72.5, "green", "LFO/CLK\nRATE"),
    ("vco_mod", 132.2, "orange", "MOD"),
    ("pw", 188.1, "orange", ""),
    ("mix_pulse", 235.3, "green", "sym:pulse"),
    ("mix_saw", 250.3, "green", "sym:saw"),
    ("mix_sub", 265.9, "green", "SUB OSC"),
    ("mix_noise", 311.2, "green", "NOISE"),
    ("vcf_freq", 337.1, "orange", "FREQ"),
    ("vcf_res", 352.5, "orange", "RES"),
    ("vcf_env", 374.0, "orange", "ENV"),
    ("vcf_mod", 389.5, "orange", "MOD"),
    ("vcf_kybd", 404.7, "orange", "KYBD"),
    ("env_a", 480.8, "orange", "A"),
    ("env_d", 496.0, "orange", "D"),
    ("env_s", 511.5, "orange", "S"),
    ("env_r", 526.5, "orange", "R"),
]
BENDER_SLIDERS = [("bend_vco", 14.1, "VCO"), ("bend_vcf", 29.1, "VCF"), ("bend_lfo", 43.7, "MOD")]
SWITCHES = [  # id, x, detentes (y de la línea amarilla, de arriba hacia abajo), leyendas, lado
    ("pwm_src", 207.8, [84.0, 91.0, 98.0], ["LFO", "MAN", "ENV"], "right"),
    ("sub_wave", 285.4, [84.0, 91.0, 98.0], ["1OCT\nDOWN", "2OCT\nDOWN", "2OCT\nDOWN"], "right"),
    ("vca_mode", 435.5, [86.0, 95.0], ["ENV", "GATE"], "left"),
    ("env_trig", 463.0, [84.0, 91.0, 98.0], ["GATE\n+\nTRIG", "GATE", "LFO"], "left"),
]
MINI_SWITCHES = [  # id, y, detentes en x, leyendas, rótulo
    ("porta_mode", 227.9, [61.0, 68.4, 75.8], ["AUTO", "OFF", "ON"], "PORTAMENTO"),
    ("transpose", 252.2, [61.0, 68.4, 75.8], ["L", "M", "H"], "TRANSPOSE"),
]
KNOBS = [  # id, x, y, radio (mm), tipo, pasos, leyendas
    ("tune", 40.7, 87.6, 10.5, "tune", 0),
    ("lfo_wave", 100.3, 88.1, 9.5, "wave", 4),
    ("vco_range", 160.3, 88.1, 9.5, "range", 4),
    ("volume", 21.6, 194.1, 9.2, "n010", 0),
    ("portamento", 69.0, 194.1, 9.2, "n010", 0),
]
BUTTON_Y, LED_Y = 138.8, 129.8
BUTTONS = [  # id, x, rótulo, rótulo inferior
    ("seq_load", 105.4, "LOAD", ""), ("seq_play", 121.5, "PLAY", ""),
    ("arp_down", 144.0, "DOWN", ""), ("arp_ud", 160.0, "U & D", ""), ("arp_up", 176.3, "UP", ""),
    ("hold", 198.8, "HOLD", "LEGATO"), ("key_transpose", 220.7, "KEY\nTRANSPOSE", "REST"),
]
POWER = (40.0, 137.8)
RATE_LED = (82.5, 58.7)
BENDER = dict(cx=43.3, cy=289.7, w=42.7, h=9.7)
JACKS = [  # x, y, diámetro, rótulo
    (33.8, 12.8, 5.5, ""), (47.3, 12.8, 3.5, ""),
    (373.7, 13.6, 7.0, "DC IN\n9V~12V"), (396.0, 13.6, 9.0, "EXT CLK\nIN"), (416.8, 13.6, 9.0, "HOLD"),
    (439.5, 13.6, 5.0, "CV"), (457.6, 13.6, 5.0, "GATE"), (477.6, 13.6, 5.0, "CV"), (495.7, 13.6, 5.0, "GATE"),
    (518.9, 13.6, 9.0, "PHONES"), (539.2, 13.6, 9.0, "OUTPUT"),
]
KEYBOARD = dict(x0=89.6, x1=534.0, y0=167.3, y_white=305.6, y_black=254.4, first_note=53)  # F3
BLACK_OFFSET = {1: -3.0, 3: 3.0, 6: -2.5, 8: 0.0, 10: 3.0}   # C#, D#, F#, G#, A#: corrimiento en mm
SECTIONS = [  # título, x0, x1
    ("", 12.0, 60.0), ("MODULATOR", 60.0, 120.0), ("VCO", 120.0, 223.5), ("SOURCE MIXER", 223.5, 325.3),
    ("VCF", 325.3, 419.2), ("VCA", 419.2, 445.3), ("ENV", 445.3, 554.0),
]
HEADER_Y0, HEADER_Y1, CTRL_Y1 = 35.0, 48.0, 124.0
# Grupos de faders: números a la izquierda del primero y a la derecha del último.
NUM_LEFT = {"lfo_rate", "vco_mod", "pw", "mix_pulse", "mix_noise", "vcf_freq", "vcf_env", "env_a"}
NUM_RIGHT = {"lfo_rate", "vco_mod", "mix_sub", "mix_noise", "vcf_res", "vcf_kybd", "env_r"}

def keyboard_keys():
    k = KEYBOARD; whites = []; keys = []
    wpitch = (k["x1"] - k["x0"]) / 19.0
    note = k["first_note"]; wi = 0
    for i in range(32):
        n = note + i; pc = n % 12
        if pc in (1, 3, 6, 8, 10):
            boundary = k["x0"] + wi * wpitch
            cx = boundary + BLACK_OFFSET[pc]
            keys.append(dict(note=n, black=True, x0=cx - 7.0, x1=cx + 7.0, y0=k["y0"], y1=k["y_black"]))
        else:
            keys.append(dict(note=n, black=False, x0=k["x0"] + wi * wpitch + 0.4, x1=k["x0"] + (wi + 1) * wpitch - 0.4,
                             y0=k["y0"], y1=k["y_white"]))
            wi += 1
    return keys

def build():
    return dict(
        size=[W_MM, H_MM],
        sliders=[dict(id=i, x=x, y10=SLIDER_Y10, y0=SLIDER_Y0, color=c) for i, x, c, _ in SLIDERS]
               + [dict(id=i, x=x, y10=BEND_Y10, y0=BEND_Y0, color="orange", short=True) for i, x, _ in BENDER_SLIDERS],
        switches=[dict(id=i, x=x, ys=ys, legends=l, side=s) for i, x, ys, l, s in SWITCHES],
        mini=[dict(id=i, y=y, xs=xs, legends=l, label=lab) for i, y, xs, l, lab in MINI_SWITCHES],
        knobs=[dict(id=i, x=x, y=y, r=r, kind=kind, steps=st) for i, x, y, r, kind, st in KNOBS],
        buttons=[dict(id=i, x=x, y=BUTTON_Y, led=[x, LED_Y]) for i, x, _, _ in BUTTONS],
        power=dict(x=POWER[0], y=POWER[1], led=[POWER[0], LED_Y - 0.5]),
        rateLed=list(RATE_LED), bender=BENDER, jacks=[dict(x=x, y=y, d=d) for x, y, d, _ in JACKS],
        keyboard=dict(KEYBOARD, keys=keyboard_keys()),
        sections=[dict(title=t, x0=a, x1=b) for t, a, b in SECTIONS],
        headerY=[HEADER_Y0, HEADER_Y1, CTRL_Y1],
    )

# Ángulos de las perillas: continuas ±150°; selectores en arco superior.
def knob_angles(kind, steps):
    if kind == "wave": return -55.0, 55.0
    if kind == "range": return -50.0, 50.0
    return -150.0, 150.0

def write_header(d, path):
    L = ["#pragma once", "// Generado por tools/panel101.py — no editar a mano.", "", "namespace dh::panel101 {", "",
         f"constexpr float widthMm = {W_MM}f, heightMm = {H_MM}f;", "",
         "struct SliderDef { const char* id; float x, y10, y0; bool shortTravel; };",
         "struct SwitchDef { const char* id; float x; int n; float ys[3]; };",
         "struct MiniDef { const char* id; float y; int n; float xs[3]; };",
         "struct KnobDef { const char* id; float x, y, r, startDeg, endDeg; int steps; };",
         "struct ButtonDef { const char* id; float x, y, ledX, ledY; };",
         "struct KeyDef { int note; bool black; float x0, x1, y0, y1; };", ""]
    L.append("inline constexpr SliderDef sliders[] = {")
    for s in d["sliders"]: L.append(f'    {{ "{s["id"]}", {s["x"]}f, {s["y10"]}f, {s["y0"]}f, {"true" if s.get("short") else "false"} }},')
    L += ["};", "inline constexpr SwitchDef switches[] = {"]
    for s in d["switches"]:
        ys = s["ys"] + [0.0] * (3 - len(s["ys"]))
        L.append(f'    {{ "{s["id"]}", {s["x"]}f, {len(s["ys"])}, {{ {ys[0]}f, {ys[1]}f, {ys[2]}f }} }},')
    L += ["};", "inline constexpr MiniDef minis[] = {"]
    for s in d["mini"]: L.append(f'    {{ "{s["id"]}", {s["y"]}f, 3, {{ {s["xs"][0]}f, {s["xs"][1]}f, {s["xs"][2]}f }} }},')
    L += ["};", "inline constexpr KnobDef knobs[] = {"]
    for k in d["knobs"]:
        a0, a1 = knob_angles(k["kind"], k["steps"])
        L.append(f'    {{ "{k["id"]}", {k["x"]}f, {k["y"]}f, {k["r"]}f, {a0}f, {a1}f, {k["steps"]} }},')
    L += ["};", "inline constexpr ButtonDef buttons[] = {"]
    for b in d["buttons"]: L.append(f'    {{ "{b["id"]}", {b["x"]}f, {b["y"]}f, {b["led"][0]}f, {b["led"][1]}f }},')
    L += ["};", f'inline constexpr ButtonDef power {{ "power", {d["power"]["x"]}f, {d["power"]["y"]}f, {d["power"]["led"][0]}f, {d["power"]["led"][1]}f }};',
          f"inline constexpr float rateLedX = {RATE_LED[0]}f, rateLedY = {RATE_LED[1]}f;",
          f"inline constexpr float benderX = {BENDER['cx']}f, benderY = {BENDER['cy']}f, benderW = {BENDER['w']}f, benderH = {BENDER['h']}f;",
          "inline constexpr KeyDef keys[] = {"]
    for k in d["keyboard"]["keys"]:
        L.append(f'    {{ {k["note"]}, {"true" if k["black"] else "false"}, {k["x0"]:.2f}f, {k["x1"]:.2f}f, {k["y0"]}f, {k["y1"]}f }},')
    L += ["};", "", "} // namespace dh::panel101", ""]
    open(path, "w").write("\n".join(L))

# ───── serigrafía ─────
PX = 12.0          # px por mm de la textura
FONT = "/System/Library/Fonts/HelveticaNeue.ttc"
img = draw = None
_fonts = {}
def font(size_mm, idx=1):
    k = (round(size_mm * PX), idx)
    if k not in _fonts: _fonts[k] = ImageFont.truetype(FONT, k[0], index=idx)
    return _fonts[k]

def text(s, x, y, size, align="center", idx=1, cond=0.86, track=0.02):
    """Texto en mm. idx 1 = Helvetica Neue Bold (la serigrafía del 101 es una grotesca negrita)."""
    f = font(size, idx); lines = s.split("\n"); lh = size * 1.05
    for li, line in enumerate(lines):
        widths = [f.getlength(c) for c in line]
        tw = sum(widths) + track * size * PX * max(0, len(line) - 1)
        lay = Image.new("RGBA", (int(tw + size * PX) + 2, int(size * PX * 1.5)), (0, 0, 0, 0))
        ld = ImageDraw.Draw(lay); xx = size * PX * 0.5
        for c, w in zip(line, widths):
            ld.text((xx, size * PX * 0.15), c, font=f, fill=(255, 255, 255, 255)); xx += w + track * size * PX
        lay = lay.resize((max(1, int(lay.width * cond)), lay.height), Image.LANCZOS)
        real = tw * cond
        cy = (y - lh * len(lines) / 2 + li * lh) * PX - size * PX * 0.12
        if align == "center": x0 = x * PX - real / 2 - size * PX * 0.5 * cond
        elif align == "left": x0 = x * PX - size * PX * 0.5 * cond
        else: x0 = x * PX - real - size * PX * 0.5 * cond
        img.alpha_composite(lay, (int(x0), int(cy)))

def line(pts, w=0.35):
    draw.line([(x * PX, y * PX) for x, y in pts], fill=(255, 255, 255, 255), width=max(1, int(w * PX)), joint="curve")

def sym(name, x, y, s):
    """Símbolos de forma de onda: s = alto en mm."""
    shapes = {
        "pulse": [(-0.6, .5), (-0.6, -.5), (0, -.5), (0, .5), (0.6, .5), (0.6, -.5)],
        "saw": [(-0.6, .5), (0.5, -.5), (0.5, .5)],
        "tri": [(-0.7, .3), (-0.35, -.4), (0.05, .4), (0.45, -.4), (0.7, -.1)],
        "square": [(-0.6, .45), (-0.6, -.45), (0.0, -.45), (0.0, .45), (0.6, .45), (0.6, -.45)],
        "narrow": [(-0.6, .45), (-0.6, -.45), (-0.3, -.45), (-0.3, .45), (0.6, .45)],
        "env": [(-0.7, .5), (-0.4, -.5), (-0.1, 0.0), (0.4, 0.0), (0.7, .5)],
        "gate": [(-0.7, .5), (-0.5, .5), (-0.5, -.5), (0.5, -.5), (0.5, .5), (0.7, .5)],
    }
    line([(x + px * s, y + py * s) for px, py in shapes[name]], 0.3)

DOT_FONT = {  # 5x7 para el logo en matriz de puntos
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
}

def dot_logo(s, x0, y0, height):
    pitch = height / 7.0; dot = pitch * 0.72
    x = x0
    for ch in s:
        rows = DOT_FONT[ch]
        for r, row in enumerate(rows):
            for c, bit in enumerate(row):
                if bit == "1":
                    px, py = x + c * pitch, y0 + r * pitch
                    draw.rectangle([px * PX, py * PX, (px + dot) * PX, (py + dot) * PX], fill=(255, 255, 255, 255))
        x += 6 * pitch

def silkscreen(d):
    global img, draw
    img = Image.new("RGBA", (int(W_MM * PX), int(H_MM * PX)), (0, 0, 0, 0)); draw = ImageDraw.Draw(img)
    # Encabezados de sección.
    for t, a, b in SECTIONS:
        if t: text(t, (a + b) / 2, (HEADER_Y0 + HEADER_Y1) / 2 + 0.4, 4.6, idx=1, cond=0.82)
    # Faders: escala con 11 rayas a cada lado y números 10/5/0.
    for s in SLIDERS + [(i, x, "orange", lab) for i, x, lab in BENDER_SLIDERS]:
        sid, x, _, lab = s
        short = sid.startswith("bend_")
        y10, y0 = (BEND_Y10, BEND_Y0) if short else (SLIDER_Y10, SLIDER_Y0)
        for i in range(11):
            yy = y0 + (y10 - y0) * i / 10
            for side in (-1, 1):
                if short and side == 1 and sid != "bend_lfo": continue
                if short and side == -1 and sid == "bend_lfo": continue
                xa, xb = x + side * 4.0, x + side * (6.8 if i % 5 == 0 else 6.0)
                line([(xa, yy), (xb, yy)], 0.32)
            if i in (0, 5, 10):
                for side in (-1, 1):
                    if short and ((side == 1) != (sid == "bend_lfo")): continue
                    # Números sólo en los bordes de cada grupo de faders (como el original).
                    if not short and sid not in (NUM_LEFT if side == -1 else NUM_RIGHT): continue
                    text(str(i), x + side * 8.3, yy, 1.9, idx=1, cond=0.9)
        if short:
            text(lab, x, BEND_Y10 - 9.5, 2.4)
        elif not lab:
            pass
        elif lab.startswith("sym:"):
            sym(lab[4:], x, 60.0, 2.6)
        else:
            text(lab, x, 59.5 if "\n" not in lab else 56.6, 2.5)
    # Leyendas de los selectores deslizantes.
    for sid, x, ys, legs, side in SWITCHES:
        for yy, lg in zip(ys, legs):
            sx = x + (6.5 if side == "right" else -6.5)
            text(lg, sx, yy, 1.8, align="left" if side == "right" else "right", cond=0.85)
        if sid == "sub_wave":
            for yy, nm in zip(ys, ["square", "square", "narrow"]): sym(nm, x + 9.5, yy + 3.0, 1.6)
        if sid == "pwm_src":
            sym("tri", x + 13.0, ys[0], 1.8); sym("env", x + 13.0, ys[2], 1.8)
        if sid == "vca_mode":
            sym("env", x - 9.0, ys[0] + 2.4, 1.6); sym("gate", x - 9.0, ys[1] + 2.4, 1.6)
    text("PULSE WIDTH", 197.5, 59.7, 2.5)    # rótulo compartido PW + PWM
    line([(184.0, 118.5), (184.0, 120.0), (212.0, 120.0), (212.0, 118.5)], 0.3)
    line([(262.0, 118.5), (262.0, 120.0), (289.0, 120.0), (289.0, 118.5)], 0.3)
    # Perillas: escalas.
    for kid, x, y, r, kind, st in KNOBS:
        a0, a1 = knob_angles(kind, st)
        lab = {"tune": "TUNE", "lfo_wave": "WAVE FORM", "vco_range": "RANGE", "volume": "VOLUME", "portamento": "PORTAMENTO"}[kid]
        text(lab, x, y - r - (9.0 if kid in ("tune",) else 7.6), 2.5)
        if kind == "tune":
            draw.arc([(x - r - 2.2) * PX, (y - r - 2.2) * PX, (x + r + 2.2) * PX, (y + r + 2.2) * PX], 125, 415,
                     fill=(255, 255, 255, 255), width=int(0.35 * PX))
            text("b", x - r + 1.0, y + r + 2.5, 2.4, idx=0); text("#", x + r - 1.0, y + r + 2.5, 2.4, idx=0)
        elif kind == "wave":
            for t, nm in zip([0, 1 / 3, 2 / 3, 1], ["tri", "square", "RANDOM", "NOISE"]):
                a = math.radians(a0 + (a1 - a0) * t); px, py = x + math.sin(a) * (r + 3.6), y - math.cos(a) * (r + 3.6)
                if nm in ("tri", "square"): sym(nm, px - 0.5, py, 1.8)
                else: text(nm, px + 3.0, py + (0 if nm == "RANDOM" else 1.6), 1.8)
                line([(x + math.sin(a) * (r + 0.6), y - math.cos(a) * (r + 0.6)), (x + math.sin(a) * (r + 2.0), y - math.cos(a) * (r + 2.0))], 0.3)
        elif kind == "range":
            for t, nm in zip([0, 1 / 3, 2 / 3, 1], ["16'", "8'", "4'", "2'"]):
                a = math.radians(a0 + (a1 - a0) * t); px, py = x + math.sin(a) * (r + 3.8), y - math.cos(a) * (r + 3.8)
                text(nm, px, py, 1.9)
                line([(x + math.sin(a) * (r + 0.6), y - math.cos(a) * (r + 0.6)), (x + math.sin(a) * (r + 2.0), y - math.cos(a) * (r + 2.0))], 0.3)
        else:
            for i in range(11):
                a = math.radians(a0 + (a1 - a0) * i / 10)
                line([(x + math.sin(a) * (r + 0.8), y - math.cos(a) * (r + 0.8)), (x + math.sin(a) * (r + 2.2), y - math.cos(a) * (r + 2.2))], 0.3)
            for i in (0, 5, 10):
                a = math.radians(a0 + (a1 - a0) * i / 10)
                text(str(i), x + math.sin(a) * (r + 4.2), y - math.cos(a) * (r + 4.2), 1.9)
    # Mini interruptores.
    for sid, y, xs, legs, lab in MINI_SWITCHES:
        for xx, lg in zip(xs, legs): text(lg, xx, y - 5.6, 1.9)
        text(lab, 68.4, y + 5.4, 2.3)
    line([(68.4, 205.5), (68.4, 214.0)], 0.3); line([(61.0, 217.0), (61.0, 214.0), (75.8, 214.0), (75.8, 217.0)], 0.3)
    # Botones de función.
    for bid, x, lab, lab2 in BUTTONS:
        text(lab, x, BUTTON_Y + 6.4 if "\n" not in lab else BUTTON_Y + 7.6, 2.0)
    text("POWER", POWER[0], POWER[1] + 9.5, 2.5)
    line([(99.5, 145.0), (99.5, 155.5), (220.7, 155.5), (220.7, 153.5)], 0.3)
    text("SEQUENCER", 113.5, 152.6, 2.4); text("ARPEGGIO", 160.0, 152.6, 2.4)
    text("LEGATO", 198.8, 153.0, 2.0); text("REST", 220.7, 153.0, 2.0)
    line([(198.8, 155.5), (198.8, 154.6)], 0.3)
    # Bender.
    text("BENDER", 21.6, 266.5, 2.4)
    line([(14.1, 262.0), (14.1, 263.2), (29.1, 263.2), (29.1, 262.0)], 0.3)
    text("LFO", 41.5, 211.0, 2.4); sym("tri", 48.0, 211.0, 2.2)
    line([(43.7, 262.0), (43.7, 281.0)], 0.3)
    draw.polygon([(43.7 * PX, 281.6 * PX), (43.0 * PX, 279.8 * PX), (44.4 * PX, 279.8 * PX)], fill=(255, 255, 255, 255))
    line([(24.0, 297.6), (62.5, 297.6)], 0.3)
    for xx, dx in ((24.0, 1), (62.5, -1)):
        draw.polygon([(xx * PX, 297.6 * PX), ((xx + dx * 1.6) * PX, 297.0 * PX), ((xx + dx * 1.6) * PX, 298.2 * PX)], fill=(255, 255, 255, 255))
    text("BENDER", 43.3, 300.6, 2.4)
    # Tira de jacks.
    text("MODULATION GRIP", 25.0, 24.6, 2.0, align="left")
    for x, y, dd, lab in JACKS:
        if lab: text(lab, x, 29.5 if "\n" not in lab else 28.5, 1.9)
    text("IN", 448.5, 32.6, 1.8); text("OUT", 486.6, 32.6, 1.8)
    line([(437.0, 31.0), (437.0, 32.6), (445.0, 32.6)], 0.28); line([(452.0, 32.6), (460.0, 32.6), (460.0, 31.0)], 0.28)
    line([(475.0, 31.0), (475.0, 32.6), (482.0, 32.6)], 0.28); line([(491.0, 32.6), (498.0, 32.6), (498.0, 31.0)], 0.28)
    # Marca propia: "Audio Tools" + logo AT101V en matriz de puntos.
    text("Audio Tools", 372.0, 152.4, 6.6, align="left", idx=1, cond=0.92, track=0.0)
    dot_logo("AT101V", 452.0, 138.5, 19.0)
    out = os.path.join(ROOT, "assets", "render"); os.makedirs(out, exist_ok=True)
    img.save(os.path.join(out, "silkscreen101.png"))
    prev = Image.new("RGBA", img.size, (126, 122, 112, 255)); prev.alpha_composite(img)
    prev.convert("RGB").resize((img.width // 3, img.height // 3), Image.LANCZOS).save(os.path.join(out, "silkscreen101_preview.jpg"), quality=88)

if __name__ == "__main__":
    d = build()
    os.makedirs(os.path.join(ROOT, "assets"), exist_ok=True)
    json.dump(d, open(os.path.join(ROOT, "assets", "layout101.json"), "w"), indent=1)
    os.makedirs(os.path.join(ROOT, "Source", "ui"), exist_ok=True)
    silkscreen(d)
    print("ok", len(d["sliders"]), "faders,", len(d["keyboard"]["keys"]), "teclas")
