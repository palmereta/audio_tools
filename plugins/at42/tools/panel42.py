#!/usr/bin/env python3
"""Panel del AT42 (basado en la familia Elta SOLAR 42, superset 42N) en milímetros.

Genera:
  assets/layout42.json             todos los controles, jacks y zonas (Blender y editor)
  Source/dsp/Jacks42.generated.h   enum de jacks con su dirección y conexión interna (normal)
  assets/render/silkscreen42.png   serigrafía (máscara de tinta, 12 px/mm) + _tabs (rótulos calados)

Coordenadas: origen arriba a la izquierda, x a la derecha, y hacia abajo, mm (docs/hardware-spec.md
§2, medidas sobre los renders 1:1 del fabricante). La gráfica es propia: sin logos ni dibujos de Elta.
"""
import json, math, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W_MM, H_MM = 495.0, 320.0
PX = 12                                   # px/mm de la serigrafía
FONT = "/System/Library/Fonts/HelveticaNeue.ttc"
F_BOLD, F_COND, F_CBLACK, F_MED = 1, 4, 9, 10

knobs, buttons, toggles, leds, jacks, boxes, labels = [], [], [], [], [], [], []
extra = {}

# Tamaños de perilla (diámetro de la falda, mm): ver §2.0.
KSIZE = {"KS": 8.0, "KM": 12.1, "KL": 14.0, "KXL": 17.9, "RSW": 18.8, "RSO": 14.0}
COL = {"orange": "#EA8C3F", "teal": "#005E7A", "green": "#005E3A", "grey": "#C4C4C4", "black": "#1A1A1A", "red": "#D8352B"}

def knob(pid, x, y, size, color, label="", lpos="below", steps=0, scale="arc", a0=-150, a1=150, legends=None, ly=None, lmm=None):
    knobs.append(dict(id=pid, x=x, y=y, size=size, d=KSIZE[size], color=color, steps=steps, scale=scale,
                      a0=a0, a1=a1, legends=legends or []))
    if label:
        r = KSIZE[size] / 2
        if ly is None: ly = y + r + 2.9 if lpos == "below" else y - r - 2.6
        labels.append((label, x, ly, lmm or (2.2 if size != "KS" else 2.0)))

def button(pid, x, y, color, label="", lpos="below", kind="latch", d=6.5):
    buttons.append(dict(id=pid, x=x, y=y, color=color, kind=kind, d=d))
    if label: labels.append((label, x, y + d / 2 + 2.6 if lpos == "below" else y - d / 2 - 2.2, 1.9))

def toggle(pid, x, y, positions, legends=(), lpos="side", lside="right"):
    toggles.append(dict(id=pid, x=x, y=y, positions=positions, legends=list(legends), lside=lside))

def led(lid, x, y, color):
    leds.append(dict(id=lid, x=x, y=y, color=color))

def jack(jid, x, y, io, label="", normal=None, lpos="below"):
    jacks.append(dict(id=jid, x=x, y=y, io=io, normal=normal, label=label))
    if label: labels.append((label, x, y + 5.6 if lpos == "below" else y - 5.0, 1.8))

def box(title, x0, y0, x1, y1, tab="tl", tabw=None, tfs=3.0):
    boxes.append(dict(title=title, x0=x0, y0=y0, x1=x1, y1=y1, tab=tab, tabw=tabw, tfs=tfs))

# ───────── voces drone clásicas (1, 2, 4, 5): 5 osciladores de sierra sin V/oct ─────────
CLASSIC = {1: 0.0, 2: 81.3, 4: 325.1, 5: 406.4}
COLX = [12.8, 25.5, 38.3, 50.9, 63.6]
for v, dx in CLASSIC.items():
    box(f"DRONE {v}", 4.6 + dx, 53.3, 84.5 + dx, 115.4, tab="above", tabw=17.5)
    for i, cx in enumerate(COLX):
        labels.append((str(i + 1), cx + dx, 55.1, 1.9))
        button(f"d{v}_mute{i+1}", cx + dx, 60.9, "black")
        knob(f"d{v}_tune{i+1}", cx + dx, 71.0, "KS", "black", scale="dot")
        button(f"d{v}_mod{i+1}", cx + dx, 83.7, "grey")
    extra.setdefault("bars", []).append(dict(id=f"d{v}_bar", x0=69.2 + dx, y0=55.3, x1=83.4 + dx, y1=66.5))
    knob(f"d{v}_volt", 76.3 + dx, 78.6, "KM", "teal", "VOLT")
    button(f"d{v}_hold", 12.8 + dx, 93.9, "light")
    led(f"d{v}_gate", 7.2 + dx, 100.9, "amber")      # a la izquierda: deja lugar al rótulo "hold" debajo del botón
    knob(f"d{v}_att", 25.5 + dx, 93.9, "KS", "black", scale="dot")
    knob(f"d{v}_rls", 38.3 + dx, 93.9, "KS", "black", scale="dot")
    knob(f"d{v}_cv", 51.0 + dx, 93.9, "KS", "black", scale="dot")
    # Debajo de su perilla o botón (encima quedaban tapados por la fila de MOD y por la propia perilla).
    for t, x in (("hold", 12.8), ("att", 25.5), ("rls", 38.3), ("cv", 51.0)): labels.append((t, x + dx, 100.4, 1.7))
    jack(f"d{v}_gate", 12.8 + dx, 109.1, "in", normal=f"KEY{v}")
    jack(f"d{v}_env", 38.2 + dx, 109.1, "out")
    jack(f"d{v}_cvin", 50.9 + dx, 109.1, "in")
    for t, x in (("gate", 12.8), ("env", 38.2), ("cv", 50.9)): labels.append((t, x + dx, 103.7, 1.5))
    extra.setdefault("orbs", []).append(dict(id=f"d{v}_orb", voice=v, x=70.0 + dx, y=102.8, d=23.3))
    # Pestaña vertical con los nombres de las filas.
    extra.setdefault("sidetabs", []).append(dict(x=5.6 + dx, y0=57.5, y1=87.5, text="MUTE · TUNE · MOD"))

# ───────── voces Papa Srapa (3, 6): modulador + tono, ruido, S&H ─────────
PAPA = {3: 0.0, 6: 406.4}
for v, dx in PAPA.items():
    box(f"DRONE {v}", 4.6 + dx, 116.8, 84.5 + dx, 181.5, tab="tr", tabw=17.8)
    button(f"d{v}_x10", 23.3 + dx, 126.9, "black", kind="latch", d=5.5)
    button(f"d{v}_fm", 44.5 + dx, 126.9, "black", kind="latch", d=5.5)
    button(f"d{v}_am", 65.6 + dx, 126.9, "black", kind="latch", d=5.5)
    labels += [("x10", 23.3 + dx, 120.6, 1.6), ("fm", 44.5 + dx, 120.6, 1.6), ("am", 64.9 + dx, 120.6, 1.6)]
    knob(f"d{v}_rate", 12.8 + dx, 134.5, "KM", "teal", "rate")
    knob(f"d{v}_fmamt", 33.9 + dx, 134.5, "KM", "teal", "mod")
    knob(f"d{v}_div", 55.0 + dx, 134.5, "KM", "teal", "divider")
    knob(f"d{v}_pitch", 76.1 + dx, 134.5, "KM", "teal", "PITCH")
    button(f"d{v}_hi", 65.6 + dx, 145.6, "black", d=5.0)
    labels.append(("hi", 70.6 + dx, 145.6, 1.6))
    jack(f"d{v}_modout", 23.5 + dx, 145.9, "out")
    jack(f"d{v}_fmin", 44.6 + dx, 145.9, "in")
    labels += [("lfo", 29.9 + dx, 145.9, 1.5), ("cv", 50.1 + dx, 145.9, 1.5)]
    button(f"d{v}_hold", 12.8 + dx, 157.4, "light")
    led(f"d{v}_gate", 7.2 + dx, 164.4, "amber")
    knob(f"d{v}_att", 25.5 + dx, 157.4, "KS", "black", scale="dot")
    knob(f"d{v}_rls", 38.2 + dx, 157.4, "KS", "black", scale="dot")
    labels += [("hold", 12.8 + dx, 163.7, 1.7), ("att", 25.5 + dx, 163.7, 1.7), ("rls", 38.2 + dx, 163.7, 1.7)]
    knob(f"d{v}_noise", 57.2 + dx, 157.3, "KM", "teal")
    labels.append(("NOISE", 57.2 + dx, 148.4, 1.9))
    led(f"d{v}_sh", 76.3 + dx, 157.4, "red")
    labels.append(("S&H", 76.3 + dx, 151.8, 1.9))
    jack(f"d{v}_gate", 12.8 + dx, 172.6, "in", normal=f"KEY{v}")
    jack(f"d{v}_env", 38.2 + dx, 172.6, "out")
    jack(f"d{v}_shin", 50.9 + dx, 172.6, "in", normal=f"NOISE{v}")
    jack(f"d{v}_shclk", 63.6 + dx, 172.6, "in")
    jack(f"d{v}_shout", 76.3 + dx, 172.6, "out")
    for t, x in (("gate", 12.8), ("env", 38.2), ("in", 50.9), ("clock", 63.6), ("out", 76.3)): labels.append((t, x + dx, 166.8, 1.5))

# ───────── VCO A / B (AS3340) y sus envolventes ─────────
OCT_LEG = ["LO", "64'", "32'", "16'", "8'", "4'", "2'", "1'"]
WAVE_LEG = ["sin", "tri", "saw", "sqr", "morph", "morph"]
for name, dx, title in (("a", 0.0, "VCO A"), ("b", 244.0, "VCO B")):
    x0 = 85.9 + dx
    box(title, x0, 116.8, x0 + 80.0, 181.5, tab="tl", tabw=15.0)
    knob(f"vco{name}_fm", 100.4 + dx, 134.5, "KL", "green", "fm")
    knob(f"vco{name}_oct", 125.8 + dx, 134.5, "RSO", "black", steps=8, scale="legend", a0=-125, a1=125, legends=OCT_LEG)
    knob(f"vco{name}_tune", 151.5 + dx, 134.5, "KL", "green", "tune")
    knob(f"vco{name}_shape", 100.4 + dx, 157.5, "KL", "green", "shape", ly=166.2, lmm=2.0)
    knob(f"vco{name}_wave", 125.8 + dx, 157.5, "RSW", "green", steps=6, scale="legend", a0=-125, a1=125, legends=WAVE_LEG)
    knob(f"vco{name}_pw", 151.5 + dx, 157.5, "KL", "green", "pw", ly=166.2, lmm=2.0)
    jack(f"vco{name}_voct", 94.1 + dx, 172.6, "in", normal="kb_voct")
    jack(f"vco{name}_fm", 116.9 + dx, 172.6, "in", normal=("vcoa_out" if name == "b" else None))
    jack(f"vco{name}_shape", 134.7 + dx, 172.6, "in")
    if name == "a": jack("vcoa_out", 157.8, 172.6, "out")
    else: jack("vcob_sync", 157.8 + dx, 172.6, "in")
    # Debajo de los jacks: encima los tapaban las perillas de abajo y la falda del selector de onda.
    for t, x in (("1v/oct", 94.1), ("fm", 116.9), ("shape", 134.7), ("out" if name == "a" else "sync", 157.8)): labels.append((t, x + dx, 179.0, 1.5))
EG = {"a": (174.4, 182.5, 166.1), "b": (272.1, 280.0, 263.8)}
for name, (xk, xh, bx) in EG.items():
    # Pestaña angosta: la de 23 mm quedaba debajo del jack gate (A) y del vca cv (B).
    box(f"ENVELOPE {name.upper()}", bx, 147.6, bx + 68.3, 181.5, tab="bl" if name == "a" else "br", tabw=11.6, tfs=2.2)
    button(f"eg{name}_hold", xh, 153.6, "black")
    button(f"eg{name}_loop", xh + 16.2, 153.6, "black")
    labels += [("hold", xh - 6.7, 153.6, 1.6), ("loop", xh + 9.5, 153.6, 1.6)]
    led(f"eg{name}", xh + 32.5, 153.6, "amber")
    for i, (p, t) in enumerate((("a", "attack"), ("d", "decay"), ("s", "sustain"), ("r", "release"))):
        knob(f"eg{name}_{p}", xk + 16.2 * i, 162.5, "KM", "green")
        labels.append((t, xk + 16.2 * i, 169.6, 1.6))
    jack(f"eg{name}_gate", xh, 175.4, "in", normal="kb_gatel")
    jack(f"eg{name}_out", xh + 16.2, 175.4, "out")
    jack(f"eg{name}_vca", xh + 32.5, 175.4, "in", normal=f"eg{name}_out")

# ───────── efector doble y filtro ─────────
box("DUAL EFFECTOR", 166.8, 43.1, 328.6, 109.7, tab="top")
for i, (ax, t) in enumerate(((174.4, "X"), (198.7, "Y"), (223.1, "Z"))):
    jack(f"fx_cv{t.lower()}", ax, 57.6, "in")
    labels.append((f"cv {t.lower()}", ax, 51.0, 1.6))
    knob(f"fx_{t.lower()}", ax, 73.5, "KM", "orange", t, scale="ticks")
extra["slot"] = dict(x0=231.9, y0=49.7, x1=263.3, y1=61.5)
toggle("fx_prog_l", 238.6, 68.4, 3, ("1", "2", "3"))
toggle("fx_prog_r", 256.4, 68.4, 3, ("1", "2", "3"))
labels += [("L", 238.6, 73.7, 1.6), ("R", 256.4, 73.7, 1.6), ("program", 247.5, 68.4, 1.5)]
knob("flt_mod_l", 238.8, 80.4, "KS", "black", scale="dot")
knob("flt_mod_r", 256.5, 80.4, "KS", "black", scale="dot")
labels += [("mod L", 238.8, 86.4, 1.5), ("mod R", 256.5, 86.4, 1.5)]
knob("fx_blend", 271.8, 73.6, "KM", "orange", "BLEND", scale="ticks")
knob("master", 296.3, 73.6, "KXL", "orange", "MASTER", scale="ticks")
knob("width", 320.6, 73.5, "KM", "orange", "WIDTH", scale="ticks")
# Filtro (franja inferior del efector).
labels.append(("FILTER L", 182.5, 90.6, 1.7)); labels.append(("FILTER R", 312.4, 90.6, 1.7))
knob("flt_freq_l", 182.5, 99.1, "KM", "orange", "freq", scale="ticks")
knob("flt_res_l", 206.8, 99.1, "KM", "orange", "res", scale="ticks")
button("flt_bp_l", 194.6, 93.6, "black", d=5.0); labels.append(("bp", 194.6, 88.0, 1.5))
jack("flt_cvl", 218.5, 99.2, "in", "cv L")
knob("flt_dist", 230.3, 99.1, "KM", "orange", "dist", scale="ticks")
button("flt_link", 247.6, 99.2, "black", "link", d=5.5)
knob("flt_gain", 264.6, 99.2, "KM", "orange", "gain", scale="ticks")
jack("flt_cvr", 276.6, 99.2, "in", "cv R", normal="flt_cvl")
knob("flt_freq_r", 288.1, 99.1, "KM", "orange", "freq", scale="ticks")
knob("flt_res_r", 312.4, 99.1, "KM", "orange", "res", scale="ticks")
button("flt_bp_r", 300.3, 93.6, "black", d=5.0); labels.append(("bp", 300.3, 88.0, 1.5))

# ───────── mezclador de 10 canales ─────────
box("VOICE MIXER", 166.8, 110.4, 328.6, 146.6, tab="below", tabw=24.0)      # colgando del borde: dentro la tapaban las perillas VCO A/B
MIXCH = ["DRONE 1", "DRONE 2", "DRONE 3", "EXT IN", "VCO A", "VCO B", "PREAMP", "DRONE 4", "DRONE 5", "DRONE 6"]
MIXX = [174.4, 190.6, 207.0, 223.1, 239.5, 255.7, 271.8, 288.0, 304.2, 320.6]
for i, (x, t) in enumerate(zip(MIXX, MIXCH)):
    knob(f"mix_pan{i+1}", x, 119.3, "KM", "black", scale="dot", a0=-135, a1=135)
    knob(f"mix_vol{i+1}", x, 139.3, "KM", "grey", scale="dot", a0=-135, a1=135)
    labels.append((t, x, 128.3, 1.55))
extra["mixband"] = dict(x0=167.6, y0=125.8, x1=327.8, y1=130.8)

# ───────── fila inferior de CV ─────────
box("LFO A", 4.4, 182.3, 60.5, 212.2, tab="bottomband")
knob("lfoa_wave", 13.6, 192.4, "KL", "red"); knob("lfoa_rate", 52.1, 192.4, "KL", "red")
jack("lfoa_out", 28.3, 192.4, "out"); led("lfoa", 28.4, 201.6, "blue")
toggle("lfoa_speed", 39.8, 195.0, 3, ("/6", "x1", "x10"), lside="left")     # a la derecha las tapaba la perilla rate
labels += [("wave", 13.6, 202.3, 1.5), ("rate", 52.1, 202.3, 1.5), ("out", 28.3, 186.6, 1.5)]
box("JOYSTICK", 61.5, 182.3, 123.2, 212.2, tab="bottomband")
knob("joy_offx", 73.0, 192.4, "KL", "red"); knob("joy_offy", 113.1, 192.4, "KL", "red")
jack("joy_xout", 93.2, 187.4, "out"); jack("joy_yout", 93.2, 197.5, "out")
led("joy_x", 84.0, 192.4, "red"); led("joy_y", 102.3, 192.4, "red")
labels += [("offset x", 73.0, 202.3, 1.5), ("offset y", 113.1, 202.3, 1.5), ("x", 98.3, 187.4, 1.5), ("y", 98.3, 197.5, 1.5)]
box("5 STEP SEQUENCER", 124.3, 182.3, 316.3, 212.2, tab="bottomband")
knob("seq_pulser", 134.2, 192.4, "KL", "red"); labels.append(("pulser", 134.2, 202.3, 1.5))
jack("seq_clkout", 152.5, 187.3, "out"); jack("seq_clkin", 152.5, 197.5, "in", normal="seq_clkout")
labels += [("out", 158.1, 187.3, 1.4), ("ext", 158.1, 197.5, 1.4)]
toggle("seq_stages", 166.5, 192.4, 3, ("3", "4", "5"))
for i, x in enumerate((181.0, 205.4, 229.7, 254.1, 278.5)):
    knob(f"seq_step{i+1}", x, 192.4, "KL", "red")
    labels.append((f"step {i+1}", x, 202.3, 1.5))
    led(f"seq_step{i+1}", x + 12.1, 187.9, "red")
    toggle(f"seq_gate{i+1}", x + 12.1, 196.0, 2, ("", "gate"))
jack("seq_cvout", 306.7, 187.3, "out"); jack("seq_gateout", 306.7, 197.5, "out")
labels += [("cv", 312.4, 187.3, 1.4), ("gate", 313.3, 197.5, 1.4)]
box("PREAMP", 317.2, 182.3, 355.5, 212.2, tab="bottomband")
jack("pre_in", 325.0, 192.4, "in", normal="AUDIO_IN"); knob("pre_gain", 339.7, 192.4, "KL", "red")
led("pre_clip", 351.9, 192.4, "blue")
labels += [("source", 325.0, 186.6, 1.5), ("gain", 339.7, 202.3, 1.5)]
box("ENVELOPE FOLLOWER", 356.4, 182.3, 433.5, 212.2, tab="bottomband")
knob("ef_att", 364.1, 192.4, "KM", "red", "attack"); knob("ef_rls", 388.5, 192.4, "KM", "red", "release")
led("ef_env", 402.2, 192.4, "red"); jack("ef_env", 409.8, 192.4, "out")
led("ef_gate", 417.4, 192.4, "green"); jack("ef_gate", 425.0, 192.4, "out")
labels += [("env", 409.8, 186.6, 1.5), ("gate", 425.0, 186.6, 1.5)]
box("LFO B", 434.5, 182.3, 490.9, 212.2, tab="bottomband")
knob("lfob_wave", 443.2, 192.4, "KL", "red"); knob("lfob_rate", 481.4, 192.4, "KL", "red")
jack("lfob_out", 458.0, 192.4, "out"); led("lfob", 457.9, 201.5, "blue")
toggle("lfob_speed", 469.3, 195.0, 3, ("/6", "x1", "x10"), lside="left")
labels += [("wave", 443.2, 202.3, 1.5), ("rate", 481.4, 202.3, 1.5), ("out", 458.0, 186.6, 1.5)]

# ───────── teclado táctil, teclas drone y joystick ─────────
extra["touch"] = dict(x0=82.5, y0=227.5, x1=412.4, y1=307.5)
PLATES = [(95.0, 270.9), (119.7, 263.2), (144.0, 255.7), (168.5, 263.2), (193.1, 270.9), (217.5, 277.9),
          (278.0, 277.9), (302.2, 270.9), (327.0, 263.2), (351.5, 255.7), (376.1, 263.2), (400.6, 270.9)]
extra["plates"] = [dict(i=i, x=x, y=y, w=19.0, h=50.0) for i, (x, y) in enumerate(PLATES)]
jack("kb_clkin", 184.1, 234.7, "in"); jack("kb_voct", 196.7, 237.8, "out"); jack("kb_gatel", 209.5, 241.0, "out")
jack("kb_gater", 285.8, 241.0, "out"); jack("kb_press", 298.2, 237.8, "out"); jack("kb_reset", 311.0, 234.7, "in")
for jid, t in (("kb_clkin", "clock"), ("kb_voct", "v/oct"), ("kb_gatel", "gate"), ("kb_gater", "gate R"), ("kb_press", "press"), ("kb_reset", "reset")):
    j = next(j for j in jacks if j["id"] == jid); j["dark"] = True
button("kb_down", 225.5, 245.1, "red", kind="momentary", d=9.5)
button("kb_up", 269.7, 245.1, "red", kind="momentary", d=9.5)
knob("kb_enc", 247.5, 245.0, "KL", "red", scale="none", a0=0, a1=360)
extra["display"] = dict(x0=233.5, y0=254.0, x1=261.5, y1=266.0)
extra["dronekeys"] = [dict(key=k, x=x, y=y, s=17.0) for k, x, y in
                      ((1, 447.0, 240.5), (4, 467.0, 240.5), (2, 447.0, 267.5), (5, 467.0, 267.5), (3, 447.0, 294.5), (6, 467.0, 294.5))]
extra["joystick"] = dict(x=37.2, y=267.5, d=23.5, cap=10.1, travel=6.0)

# ───────── salida: layout JSON y header de jacks ─────────
def write_layout():
    d = dict(size=[W_MM, H_MM], knobs=knobs, buttons=buttons, toggles=toggles, leds=leds, jacks=jacks, boxes=boxes, **extra)
    json.dump(d, open(os.path.join(ROOT, "assets", "layout42.json"), "w"), indent=1)

def write_jacks_header():
    ids = [j["id"] for j in jacks]
    L = ["#pragma once", "// Generado por tools/panel42.py — no editar a mano.", "", "namespace dh::jk42 {", "",
         "enum Jack : int", "{"]
    L += [f"    {j['id']}," for j in jacks]
    L += ["    kNumJacks", "};", "",
          "// Fuentes internas que no son jacks (conexiones normalizadas del hardware).",
          "enum Normal : int { None = -1, KEY1 = 1000, KEY2, KEY3, KEY4, KEY5, KEY6, NOISE3, NOISE6, AUDIO_IN };", "",
          "struct JackDef { const char* id; bool isOutput; int normal; float x, y; };", "",
          "inline const JackDef jacks[kNumJacks] = {"]
    for j in jacks:
        n = j["normal"]
        ns = "None" if n is None else (n if n in ("KEY1", "KEY2", "KEY3", "KEY4", "KEY5", "KEY6", "NOISE3", "NOISE6", "AUDIO_IN") else n)
        L.append(f"    {{ \"{j['id']}\", {'true' if j['io'] == 'out' else 'false'}, {ns}, {j['x']}f, {j['y']}f }},")
    L += ["};", "", "} // namespace dh::jk42", ""]
    open(os.path.join(ROOT, "Source", "dsp", "Jacks42.generated.h"), "w").write("\n".join(L))

# ───────── serigrafía ─────────
_fonts = {}
def font(mm, idx=F_BOLD):
    key = (round(mm * PX), idx)
    if key not in _fonts: _fonts[key] = ImageFont.truetype(FONT, key[0], index=idx)
    return _fonts[key]

def write_silkscreen():
    W, H = int(W_MM * PX), int(H_MM * PX)
    ink = Image.new("L", (W, H), 0)         # líneas y texto
    tab = Image.new("L", (W, H), 0)         # rellenos de pestañas (el texto va calado)
    di, dt = ImageDraw.Draw(ink), ImageDraw.Draw(tab)
    P = lambda v: v * PX

    def text(dr, s, x, y, mm, idx=F_BOLD, fill=255, anchor="mm"):
        dr.text((P(x), P(y)), s, font=font(mm, idx), fill=fill, anchor=anchor)

    for b in boxes:
        x0, y0, x1, y1 = (P(v) for v in (b["x0"], b["y0"], b["x1"], b["y1"]))
        di.rounded_rectangle([x0, y0, x1, y1], radius=P(1.6), outline=255, width=max(2, round(P(0.35))))
        t = b["title"]; tw = (b["tabw"] or (len(t) * 2.2 + 6))
        th = 5.0
        if b["tab"] == "above":
            tx0 = b["x0"]
            r = [P(tx0), y0 - P(th), P(tx0 + tw), y0 + P(0.4)]
            ty = b["y0"] - th / 2
        elif b["tab"] in ("tl", "tr", "top"):
            tx0 = b["x0"] if b["tab"] == "tl" else (b["x1"] - tw if b["tab"] == "tr" else (b["x0"] + b["x1"]) / 2 - tw / 2)
            r = [P(tx0), y0, P(tx0 + tw), y0 + P(th)]
            ty = b["y0"] + th / 2
        elif b["tab"] == "below":            # colgando debajo del borde inferior
            tx0 = (b["x0"] + b["x1"]) / 2 - tw / 2
            r = [P(tx0), y1 - P(0.4), P(tx0 + tw), y1 + P(th)]
            ty = b["y1"] + th / 2
        elif b["tab"] in ("bl", "br", "bottom"):
            tx0 = b["x0"] if b["tab"] == "bl" else (b["x1"] - tw if b["tab"] == "br" else (b["x0"] + b["x1"]) / 2 - tw / 2)
            r = [P(tx0), y1 - P(th), P(tx0 + tw), y1]
            ty = b["y1"] - th / 2
        else:  # banda inferior a todo el ancho
            tx0, tw = b["x0"], b["x1"] - b["x0"]
            r = [x0, y1 - P(th), x1, y1]
            ty = b["y1"] - th / 2
        dt.rounded_rectangle(r, radius=P(1.0), fill=255)
        text(dt, t, tx0 + tw / 2, ty + 0.15, b.get("tfs", 3.0), F_COND, fill=0)

    for (s, x, y, mm) in labels:
        text(di, s, x, y, mm, F_BOLD if mm >= 2.0 else F_MED)

    # Escalas de perillas.
    for k in knobs:
        r = k["d"] / 2
        if k["scale"] in ("arc", "ticks"):
            rr = r + 1.6
            a0, a1 = math.radians(k["a0"]), math.radians(k["a1"])
            box_ = [P(k["x"] - rr), P(k["y"] - rr), P(k["x"] + rr), P(k["y"] + rr)]
            di.arc(box_, start=math.degrees(a0) - 90, end=math.degrees(a1) - 90, fill=255, width=max(2, round(P(0.22))))
            n = 11 if k["scale"] == "ticks" else 2
            for i in range(n):
                a = a0 + (a1 - a0) * i / (n - 1)
                p0 = (k["x"] + math.sin(a) * (rr), k["y"] - math.cos(a) * (rr))
                p1 = (k["x"] + math.sin(a) * (rr + (1.2 if i in (0, n - 1) else 0.8)), k["y"] - math.cos(a) * (rr + (1.2 if i in (0, n - 1) else 0.8)))
                di.line([P(p0[0]), P(p0[1]), P(p1[0]), P(p1[1])], fill=255, width=max(2, round(P(0.25))))
        elif k["scale"] == "dot":
            for a in (k["a0"], k["a1"]):
                ar = math.radians(a)
                cx, cy = k["x"] + math.sin(ar) * (r + 1.3), k["y"] - math.cos(ar) * (r + 1.3)
                di.ellipse([P(cx - 0.35), P(cy - 0.35), P(cx + 0.35), P(cy + 0.35)], fill=255)
        elif k["scale"] == "legend":
            n = k["steps"]
            for i, s in enumerate(k["legends"]):
                a = math.radians(k["a0"] + (k["a1"] - k["a0"]) * i / (n - 1))
                cx, cy = k["x"] + math.sin(a) * (r + 3.4), k["y"] - math.cos(a) * (r + 3.4)
                text(di, s, cx, cy, 1.75, F_MED)
                t0 = (k["x"] + math.sin(a) * (r + 0.6), k["y"] - math.cos(a) * (r + 0.6))
                t1 = (k["x"] + math.sin(a) * (r + 1.5), k["y"] - math.cos(a) * (r + 1.5))
                di.line([P(t0[0]), P(t0[1]), P(t1[0]), P(t1[1])], fill=255, width=max(2, round(P(0.22))))

    # Leyendas de los toggles (a la derecha, de arriba abajo).
    for t in toggles:
        n = t["positions"]
        for i, s in enumerate(t["legends"]):
            if not s: continue
            yy = t["y"] - 3.2 + 6.4 * i / max(1, n - 1) if n > 1 else t["y"]
            if t.get("lside") == "left": text(di, s, t["x"] - 3.9, yy, 1.5, F_MED, anchor="rm")
            else: text(di, s, t["x"] + 3.9, yy, 1.5, F_MED, anchor="lm")

    # Rótulos verticales de las filas de las voces clásicas (MUTE, TUNE, MOD).
    def vtext(s_, x, y, mm):
        f = font(mm, F_COND)
        bb = f.getbbox(s_)
        im = Image.new("L", (bb[2] + 4, bb[3] + 4), 0)
        ImageDraw.Draw(im).text((2, 2), s_, font=f, fill=255)
        im = im.rotate(90, expand=True)
        ink.paste(255, (int(P(x) - im.width / 2), int(P(y) - im.height / 2)), im)
    for dx in CLASSIC.values():
        for s_, y in (("MUTE", 60.9), ("TUNE", 71.0), ("MOD", 83.7)): vtext(s_, 6.9 + dx, y, 1.7)
    # Banda negra del mezclador con los nombres calados.
    mb = extra["mixband"]
    dt.rectangle([P(mb["x0"]), P(mb["y0"]), P(mb["x1"]), P(mb["y1"])], fill=255)
    for (s, x, y, mm) in labels:
        if mb["y0"] < y < mb["y1"]:
            text(dt, s, x, y, mm, F_COND, fill=0)
            text(di, s, x, y, mm, F_COND, fill=0)
    # Barra OSC STATUS: ventana negra.
    for b in extra["bars"]:
        dt.rectangle([P(b["x0"]), P(b["y0"]), P(b["x1"]), P(b["y1"])], fill=255)
        text(di, "1 2 3 4 5", (b["x0"] + b["x1"]) / 2, b["y1"] + 1.6, 1.5, F_MED)
    # Pantalla, ranura del cartucho y rótulos del teclado.
    s = extra["slot"]; di.rounded_rectangle([P(s["x0"]), P(s["y0"]), P(s["x1"]), P(s["y1"])], radius=P(1.0), outline=255, width=round(P(0.3)))
    text(di, "cartridge", (s["x0"] + s["x1"]) / 2, s["y0"] - 1.6, 1.5, F_MED)
    T = extra["touch"]
    text(di, "TOUCH SENSITIVE CONTROLLER", T["x1"] - 32.0, T["y0"] - 4.3, 2.0, F_BOLD)
    text(di, "DRONE KEYS", 457.0, 223.0, 2.0, F_BOLD)
    for dk in extra["dronekeys"]:
        text(di, str(dk["key"]), dk["x"], dk["y"] - dk["s"] / 2 - 2.6, 1.9, F_BOLD)
    j = extra["joystick"]
    for (dx, dy, a) in ((0, -1, "▲"), (0, 1, "▼"), (-1, 0, "◀"), (1, 0, "▶")):
        cx, cy = j["x"] + dx * 21.0, j["y"] + dy * 21.0
        tri = {"▲": [(0, -1.4), (-1.3, 1), (1.3, 1)], "▼": [(0, 1.4), (-1.3, -1), (1.3, -1)],
               "◀": [(-1.4, 0), (1, -1.3), (1, 1.3)], "▶": [(1.4, 0), (-1, -1.3), (-1, 1.3)]}[a]
        di.polygon([(P(cx + px), P(cy + py)) for px, py in tri], fill=255)
    # Marca propia: "Audio Tools" a la izquierda, "AT42" y un sol de líneas a la derecha.
    text(di, "Audio Tools", 12.0, 36.5, 5.0, F_BOLD, anchor="lm")
    text(di, "drone machine", 12.5, 43.0, 2.2, F_MED, anchor="lm")
    text(di, "AT42", 466.0, 34.5, 13.5, F_CBLACK, anchor="rm")
    cx, cy = 404.0, 34.0
    for i in range(13):
        a = math.radians(-90 + 180 * i / 12 - 90)
        di.line([P(cx + math.cos(a) * 3.2), P(cy + math.sin(a) * 3.2), P(cx + math.cos(a) * 6.2), P(cy + math.sin(a) * 6.2)], fill=255, width=round(P(0.45)))
    di.ellipse([P(cx - 2.3), P(cy - 2.3), P(cx + 2.3), P(cy + 2.3)], fill=255)
    di.rectangle([P(cx - 7.0), P(cy), P(cx + 7.0), P(cy + 0.5)], fill=255)
    # Leyendas del borde trasero.
    for s, x in (("EXT AUDIO", 18.0), ("OUT R", 34.5), ("OUT L", 56.7), ("POWER 12V DC", 468.0)):
        text(di, s, x, 6.0, 1.7, F_MED)

    os.makedirs(os.path.join(ROOT, "assets", "render"), exist_ok=True)
    rgba = Image.merge("RGBA", (ink, tab, Image.new("L", (W, H), 0), Image.new("L", (W, H), 255)))
    rgba.save(os.path.join(ROOT, "assets", "render", "silkscreen42.png"))
    prev = Image.new("RGB", (W, H), (233, 223, 210))
    prev.paste((24, 24, 24), mask=tab)
    blk = Image.new("RGB", (W, H), (24, 24, 24))
    prev.paste(blk, mask=Image.eval(ink, lambda v: v))
    prev.resize((W // 4, H // 4)).save(os.path.join(ROOT, "assets", "render", "silkscreen42_preview.jpg"), quality=88)


# ───────── parámetros del panel (host, presets, motor) ─────────
SECTION = {"d": "Drone", "vco": "VCO", "eg": "Env", "fx": "FX", "flt": "Filter", "mix": "Mixer", "lfoa": "LFO A", "lfob": "LFO B",
           "joy": "Joystick", "seq": "Seq", "pre": "Preamp", "ef": "Follower", "kb": "Keys"}
NICE = {"mute": "Mute", "tune": "Tune", "mod": "Mod", "volt": "Volt", "att": "Attack", "rls": "Release", "cv": "CV", "hold": "Hold",
        "x10": "Rate x10", "fm": "FM", "am": "AM", "hi": "Hi Range", "rate": "Rate", "fmamt": "Mod Amount", "div": "Divider",
        "pitch": "Pitch", "noise": "Noise", "oct": "Octave", "shape": "Shape", "wave": "Wave", "pw": "PW", "loop": "Loop",
        "a": "Attack", "d": "Decay", "s": "Sustain", "r": "Release", "x": "X", "y": "Y", "z": "Z", "blend": "Blend",
        "prog_l": "Program L", "prog_r": "Program R", "cart_l": "Cartridge L", "cart_r": "Cartridge R", "freq_l": "Freq L",
        "res_l": "Res L", "bp_l": "BP L", "freq_r": "Freq R", "res_r": "Res R", "bp_r": "BP R", "dist": "Dist", "link": "Link",
        "gain": "Gain", "mod_l": "Mod L", "mod_r": "Mod R", "speed": "Speed", "offx": "Offset X", "offy": "Offset Y",
        "pulser": "Pulser", "stages": "Stages", "master": "Master", "width": "Width", "hand": "Hand"}
CARTS = "Cathedral|Magic|Time|Vibrotrem|Filter|Vibe|Pitch Shifter|Infinity|String Ringer|Syntex-1|Digital|Generator|Ochre"
MIXNAMES = ["Drone 1", "Drone 2", "Drone 3", "Ext In", "VCO A", "VCO B", "Preamp", "Drone 4", "Drone 5", "Drone 6"]

def pname(pid):
    if pid in ("master", "width"): return NICE[pid]
    m = __import__("re").match(r"^(d)(\d)_(\w+?)(\d?)$", pid)
    if m:
        # "d3_x10" no es "x1" + índice 0: el nombre entero va primero ("Drone 3 Rate x10", no "Drone 3 x1 0").
        if m.group(3) + m.group(4) in NICE: return f"Drone {m.group(2)} {NICE[m.group(3) + m.group(4)]}"
        base = NICE.get(m.group(3), m.group(3)); idx = m.group(4)
        return f"Drone {m.group(2)} {base}{(' ' + idx) if idx else ''}"
    m = __import__("re").match(r"^mix_(pan|vol)(\d+)$", pid)
    if m: return f"Mixer {MIXNAMES[int(m.group(2)) - 1]} {'Pan' if m.group(1) == 'pan' else 'Volume'}"
    m = __import__("re").match(r"^seq_(step|gate)(\d)$", pid)
    if m: return f"Seq {'Step' if m.group(1) == 'step' else 'Gate'} {m.group(2)}"
    m = __import__("re").match(r"^(vco|eg)([ab])_(\w+)$", pid)
    if m: return f"{'VCO' if m.group(1) == 'vco' else 'Env'} {m.group(2).upper()} {NICE.get(m.group(3), m.group(3))}"
    sec, _, rest = pid.partition("_")
    return f"{SECTION.get(sec, sec)} {NICE.get(rest, rest.replace('_', ' ').title())}"

DEFAULTS = {"_tune": 0.5, "_volt": 0.15, "_att": 0.25, "_rls": 0.5, "_cv": 0.5, "_rate": 0.4, "_fmamt": 0.35, "_div": 0.3,
            "_pitch": 0.5, "vcoa_oct": 4, "vcob_oct": 4, "_tune_vco": 0.5, "_wave": 2, "_pw": 0.5, "_a": 0.05, "_d": 0.4, "_s": 0.6,
            "_r": 0.4, "fx_x": 0.5, "fx_y": 0.5, "fx_z": 0.5, "fx_blend": 0.35, "master": 0.75, "width": 0.5, "flt_freq_l": 0.85,
            "flt_freq_r": 0.85, "flt_res_l": 0.15, "flt_res_r": 0.15, "flt_gain": 0.3, "flt_mod_l": 0.5, "flt_mod_r": 0.5,
            "lfoa_speed": 1, "lfob_speed": 1, "lfoa_rate": 0.35, "lfob_rate": 0.55, "joy_offx": 0.5, "joy_offy": 0.5,
            "seq_pulser": 0.45, "seq_stages": 2, "seq_step": 0.5, "seq_gate": 1, "pre_gain": 0.3, "ef_att": 0.2, "ef_rls": 0.4,
            "d3_fm": 1, "d6_fm": 1,
            # Explícitos: los sufijos "_wave" (de los VCO, 6 pasos) y "_r" (release) también caían en estos y les daban 2 y 0,4.
            "lfoa_wave": 1.0, "lfob_wave": 1.0, "flt_bp_r": 0, "fx_prog_r": 0}
MIXDEF = {"pan": [0.3, 0.7, 0.35, 0.5, 0.42, 0.58, 0.5, 0.25, 0.75, 0.65], "vol": [0.6, 0.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.6, 0.0]}

def pdefault(pid):
    m = __import__("re").match(r"^mix_(pan|vol)(\d+)$", pid)
    if m: return MIXDEF[m.group(1)][int(m.group(2)) - 1]
    if pid in DEFAULTS: return DEFAULTS[pid]
    base = __import__("re").sub(r"\d+$", "", pid) if pid.startswith("d") and "_" in pid else pid
    for k, v in DEFAULTS.items():
        if k.startswith("_") and base.endswith(k): return v
        if k.startswith("seq_") and pid.startswith(k): return v
    return 0.0

def collect_params():
    ps = []
    for k in knobs:
        if k["id"] == "kb_enc": continue
        ps.append(dict(id=k["id"], steps=k["steps"], choices="|".join(k["legends"]) if k["steps"] else ""))
    for b in buttons:
        if b["kind"] == "latch": ps.append(dict(id=b["id"], steps=2, choices="Off|On"))
    for t in toggles:
        ch = "|".join(t["legends"]) if any(t["legends"]) else "Off|On"
        if t["id"].startswith("seq_gate"): ch = "Off|On"
        ps.append(dict(id=t["id"], steps=t["positions"], choices=ch))
    ps += [dict(id="joy_x", steps=0, choices=""), dict(id="joy_y", steps=0, choices=""),
           dict(id="fx_cart_l", steps=13, choices=CARTS), dict(id="fx_cart_r", steps=13, choices=CARTS)]
    # La mano sobre cada orbe (1.0.8): la escribe el panel (pasar o apoyar el mouse) y se puede automatizar. Al final de
    # la lista para no mover los índices de los demás.
    ps += [dict(id=f"d{v}_hand", steps=0, choices="") for v in CLASSIC]
    for p_ in ps:
        p_["name"] = pname(p_["id"]); p_["def"] = pdefault(p_["id"])
        if p_["id"] in ("joy_x", "joy_y"): p_["name"] = "Joystick " + p_["id"][-1].upper(); p_["def"] = 0.5
        if p_["id"] in ("fx_cart_l", "fx_cart_r"): p_["def"] = 0 if p_["id"].endswith("l") else 2
        if p_["id"] in ("vcoa_tune", "vcob_tune"): p_["def"] = 0.5
    return ps

def write_params_header(ps):
    L = ["#pragma once", "// Generado por tools/panel42.py — no editar a mano.", "", "namespace dh {", "",
         "enum class P42 : int", "{"]
    L += [f"    {p_['id']}," for p_ in ps]
    L += ["    count", "};", "constexpr int kNumP42 = (int) P42::count;", "",
          "struct PanelParam42 { const char* id; const char* name; int steps; float def; const char* choices; };", "",
          "inline const PanelParam42& panelParam42 (int i)", "{", "    static const PanelParam42 t[kNumP42] = {"]
    for p_ in ps:
        L.append(f"        {{ \"{p_['id']}\", \"{p_['name']}\", {p_['steps']}, {float(p_['def'])}f, \"{p_['choices']}\" }},")
    L += ["    };", "    return t[i];", "}", "", "} // namespace dh", ""]
    open(os.path.join(ROOT, "Source", "dsp", "Params42.generated.h"), "w").write("\n".join(L))

if __name__ == "__main__":
    write_layout()
    write_jacks_header()
    write_silkscreen()
    ps = collect_params(); write_params_header(ps)
    print(len(ps), "parámetros")
    print("ok", len(knobs), "perillas,", len(buttons), "botones,", len(toggles), "llaves,", len(jacks), "jacks,", len(leds), "leds")
