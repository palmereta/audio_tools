#!/usr/bin/env python3
"""Panel del AT700 (basado en el Roland System-700, 1976) en milímetros.

Un solo lienzo con los tres gabinetes como se arman en el estudio: arriba el secuenciador analógico
(717A), al medio la consola principal (fila superior de módulos de 280 mm, inferior de 140 mm) y
abajo el teclado (701A). Medidas tomadas de los dibujos acotados del manual de 1976 (ver
docs/hardware-spec.md); la gráfica es propia: sin el logo de Roland.

Genera:
  assets/layout700.json             controles, jacks, zonas y vistas (Blender y editor)
  Source/dsp/Jacks700.generated.h   enum de jacks con dirección, módulo y conexión interna
  Source/dsp/Params700.generated.h  parámetros del panel
  assets/render/silkscreen700.png   serigrafía (tinta blanca, 8 px/mm)

Coordenadas: origen arriba a la izquierda del lienzo, x a la derecha, y hacia abajo, mm.
"""
import json, math, os, re
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W_MM, H_MM = 900.0, 1090.0
PX = 8                                    # px/mm de la serigrafía
FONT = "/System/Library/Fonts/HelveticaNeue.ttc"
F_REG, F_BOLD, F_COND, F_CBLACK, F_MED = 0, 1, 4, 9, 10

knobs, sliders, switches, toggles, levers, buttons, leds, jacks, meters = [], [], [], [], [], [], [], [], []
panels, tags, rects, lines, texts, scales = [], [], [], [], [], []
extra = {}

KSIZE = {"KS": 12.5, "KM": 15.0, "KL": 21.0, "KP": 25.0, "KF": 9.5, "KQ": 12.5}

# ───────── primitivas ─────────
def knob(pid, x, y, kind, steps=0, a0=-150, a1=150, legends=None, scale="ticks", ticks=11, labels=None, lr=None):
    """scale: ticks (marcas), legend (leyendas en cada paso), none. labels: [(texto, fracción 0..1)]."""
    knobs.append(dict(id=pid, x=x, y=y, size=kind, d=KSIZE[kind], steps=steps, a0=a0, a1=a1, legends=legends or [],
                      scale=scale, ticks=ticks, labels=labels or [], lr=lr))

def slider(pid, x, y0, y1, horiz=False, scale="0-10", detent=None):
    """Vertical: y0 arriba (valor 1), y1 abajo (valor 0). Horizontal: x..y0 es el rango izquierda→derecha, y1 la fila."""
    if horiz: sliders.append(dict(id=pid, horiz=True, x0=x, x1=y0, y=y1, scale=scale, detent=detent))
    else: sliders.append(dict(id=pid, horiz=False, x=x, y0=y0, y1=y1, scale=scale, detent=detent))

def switch(pid, x, y, positions, orient="v", legends=(), length=None, lpos="right"):
    """Llave deslizante: posición 0 arriba (vertical) o a la izquierda (horizontal)."""
    length = length or (3.2 * positions + 2.0)
    switches.append(dict(id=pid, x=x, y=y, positions=positions, orient=orient, legends=list(legends), length=length, lpos=lpos))

def toggle(pid, x, y, positions=2, legends=(), lpos="vertical"):
    toggles.append(dict(id=pid, x=x, y=y, positions=positions, legends=list(legends), lpos=lpos))

def lever(pid, x, y0, y1, positions):
    levers.append(dict(id=pid, x=x, y0=y0, y1=y1, positions=positions))

def button(bid, x, y, shape="round", color="red", d=8.0, w=None, h=None, kind="momentary"):
    buttons.append(dict(id=bid, x=x, y=y, shape=shape, color=color, d=d, w=w or d, h=h or d, kind=kind))

def led(lid, x, y, color="red"):
    leds.append(dict(id=lid, x=x, y=y, color=color))

MODULE = [""]
def jack(jid, x, y, io, label="", normal=None, poly=None, lpos="above", lsize=1.7):
    """io: in | out | mult (las de los múltiples, sirven de entrada o de salida)."""
    jacks.append(dict(id=jid, x=x, y=y, io=io, normal=normal, module=MODULE[0], label=label))
    if label:
        text(label, x, y - 7.6 if lpos == "above" else y + 7.6, lsize)

def tag(s, x, y, size=1.6, w=None):
    tags.append(dict(text=s, x=x, y=y, size=size, w=w))

def rect(x0, y0, x1, y1, label="", size=2.6, radius=1.2, fill=False):
    rects.append(dict(x0=x0, y0=y0, x1=x1, y1=y1, label=label, size=size, radius=radius, fill=fill))

def line(*pts, arrow=False):
    lines.append(dict(pts=[list(p) for p in pts], arrow=arrow))

def text(s, x, y, mm, face=None, anchor="mm"):
    texts.append(dict(s=s, x=x, y=y, mm=mm, face=face if face is not None else (F_COND if mm >= 2.4 else F_MED), anchor=anchor))

def meter(mid, x, y, w, h):
    meters.append(dict(id=mid, x=x, y=y, w=w, h=h))

def panel(code, x0, y0, w, h, title=(), tsize=2.5, foot=True):
    panels.append(dict(code=code, x0=x0, y0=y0, x1=x0 + w, y1=y0 + h))
    MODULE[0] = code
    for i, t in enumerate(title):
        text(t, x0 + w / 2, y0 + 9.5 + i * 4.4, tsize, F_COND)
    if foot:
        text("Audio Tools", x0 + 4.0, y0 + h - 9.0, 1.9, F_BOLD, anchor="lm")
        text(code, x0 + w - 4.0, y0 + h - 9.0, 1.7, F_MED, anchor="rm")

def vscale(x, y_top, y_bot, labels=("10", "5", "0"), n=11, width=3.2, both=True, labx=None):
    """Escala de escalera de un fader vertical (líneas a los dos lados y números a la izquierda)."""
    scales.append(dict(kind="v", x=x, y0=y_top, y1=y_bot, n=n, width=width, both=both, labels=list(labels), labx=labx))

def hscale(y, x_l, x_r, labels=(), n=11, width=2.6, above=True):
    scales.append(dict(kind="h", y=y, x0=x_l, x1=x_r, n=n, width=width, labels=list(labels), above=above))

# Recorrido de los faders (mm): tapa de 8,5 × 4,5; ranura de 2,6.
def fader(pid, x, top, bot, labels=("10", "5", "0"), lab=True):
    slider(pid, x, top, bot)
    vscale(x, top, bot, labels if lab else (), labx=x - 7.4 if lab else None)

# ───────── gabinetes y vistas ─────────
SEQ = dict(x0=50.0, y0=24.0, w=800.0, h=286.0)
CAB_SEQ = dict(x0=24.0, y0=6.0, x1=876.0, y1=330.0)
UP_Y, LO_Y, CX0 = 356.0, 636.0, 58.5
CAB_CON = dict(x0=28.0, y0=338.0, x1=872.0, y1=866.0)
TRAY = dict(x0=CX0, y0=LO_Y + 140.0, x1=CX0 + 783.0, y1=852.0)
KB = dict(x0=24.0, y0=884.0, x1=876.0, y1=1076.0)
KB_PANEL = dict(x0=34.0, y0=896.0, w=150.0, h=170.0)
KEYS = dict(x0=196.0, x1=866.0, y0=900.0, y1=1060.0, n=61)
extra["cabinets"] = dict(seq=CAB_SEQ, console=CAB_CON, keyboard=KB, tray=TRAY, keys=KEYS)
extra["views"] = {"sequencer": [0.0, 0.0, 900.0, 520.0], "console": [0.0, 332.0, 900.0, 872.0],
                  "keyboard": [0.0, 560.0, 900.0, 1090.0], "all": [0.0, 0.0, 900.0, 1090.0]}

# ═════════════ CONSOLA: fila superior (280 mm) ═════════════
WAVES = ["sine", "tri", "saw", "pulse"]
def vco(n, x0):
    code = {1: "702A", 2: "702B", 3: "702C"}[n]
    panel(code, x0, UP_Y, 65.0, 280.0, ("VOLTAGE CONTROLLED", f"OSCILLATOR -{n}"))
    X = lambda v: x0 + v
    Y = lambda v: UP_Y + v
    for i, (w, yy) in enumerate(zip(WAVES, (31.0, 46.5, 62.0, 77.5))):
        jack(f"vco{n}_{w}", X(15.5), Y(yy), "out")
        extra.setdefault("wavesyms", []).append(dict(kind=w, x=X(26.5), y=Y(yy)))
        line((X(31.5), Y(yy)), (X(40.0), Y(yy)), (X(40.0), Y(37.0 + 10.3 * i)), (X(46.0), Y(37.0 + 10.3 * i)))
    text("WAVE FORM", X(50.5), Y(26.6), 1.55, F_MED); text("SELECTOR", X(50.5), Y(28.9), 1.55, F_MED)
    lever(f"vco{n}_wave", X(50.0), Y(37.0), Y(67.9), 4)
    text("PULSE WIDTH", X(16.0), Y(89.5), 1.65); text("RANGE", X(47.5), Y(89.5), 1.65)
    knob(f"vco{n}_pw", X(16.0), Y(104.0), "KS", labels=[("10%", 0.0), ("50%", 0.5), ("90%", 1.0)])
    knob(f"vco{n}_range", X(47.5), Y(104.0), "KS", steps=6, a0=-100, a1=100, scale="legend",
         legends=["LO", "32'", "16'", "8'", "4'", "2'"])
    text("PITCH", X(35.0), Y(124.5), 1.8)
    knob(f"vco{n}_pitch", X(35.0), Y(146.5), "KP", a0=-150, a1=150, ticks=25,
         labels=[(str(v), (v + 12) / 24) for v in range(-12, 13, 2)])
    knob(f"vco{n}_fine", X(35.0), Y(146.5), "KF", scale="none")
    text("(SEMITONE)", X(35.0), Y(167.0), 1.5)
    if n == 1:
        text("PWM", X(12.0), Y(174.0), 1.7)
        line((X(12.0), Y(170.5)), (X(12.0), Y(118.0)), (X(14.0), Y(113.0)))
        fader("vco1_pwm", X(12.0), Y(194.0), Y(228.0))
        jack("vco1_pwm", X(12.0), Y(252.0), "in")
        toggle("vco1_keycv", X(25.0), Y(221.0), 2, ("ON", "OFF"))
        tag("KEY CV", X(25.0), Y(252.0))
        mods = [(37.9, "LFO1", None), (52.1, None, "vco1_ext")]
    else:
        text("SYNC.", X(9.0), Y(134.5), 1.7)
        toggle(f"vco{n}_syncmode", X(10.0), Y(141.5), 2, ("S", "W"), lpos="horizontal")
        jack(f"vco{n}_sync", X(10.0), Y(153.5), "in")
        toggle(f"vco{n}_keycv", X(7.6), Y(221.0), 2, ("ON", "OFF"))
        tag("KEY CV", X(7.6), Y(252.0), 1.45)
        mods = [(21.0, "LFO1", None), (38.6, "ADSR1" if n == 2 else "S&H", None), (56.0, None, f"vco{n}_ext")]
    text("MODULATION", X(44.0 if n == 1 else 38.6), Y(174.0), 1.6)
    for i, (mx, bus, jk) in enumerate(mods):
        toggle(f"vco{n}_att{i+1}", X(mx), Y(184.0), 2, ("1/10", "1"))
        fader(f"vco{n}_mod{i+1}", X(mx), Y(194.0), Y(228.0), lab=(i == 0))
        if bus: tag(bus, X(mx), Y(252.0), 1.45)
        else: jack(jk, X(mx), Y(252.0), "in")

def noise_rm(x0):
    panel("708A", x0, UP_Y, 65.0, 280.0, ("NOISE GENERATOR",))
    X = lambda v: x0 + v
    Y = lambda v: UP_Y + v
    rect(X(9.0), Y(22.0), X(27.0), Y(52.0), "NOISE\nGEN.", 2.0)
    text("WHITE NOISE OUT", X(46.0), Y(28.5), 1.45); jack("noise_white", X(47.5), Y(37.0), "out")
    text("PINK NOISE OUT", X(46.0), Y(54.0), 1.45); jack("noise_pink", X(47.5), Y(62.5), "out")
    line((X(27.0), Y(37.0)), (X(41.0), Y(37.0)), arrow=True); line((X(27.0), Y(46.0)), (X(33.0), Y(46.0)), (X(33.0), Y(62.5)), (X(41.0), Y(62.5)), arrow=True)
    line((X(6.0), Y(87.5)), (X(59.0), Y(87.5)))
    text("RING MODULATOR", X(32.5), Y(97.0), 2.3, F_COND)
    text("R.M. OUT", X(48.0), Y(106.5), 1.45); jack("rm_out", X(48.0), Y(115.5), "out")
    led("rm_sig", X(48.0), Y(127.0), "green")
    rect(X(9.0), Y(142.0), X(43.0), Y(158.0), "RING MOD.", 2.2)
    text("X", X(12.5), Y(156.0), 1.4); text("Y", X(37.5), Y(156.0), 1.4)
    line((X(48.0), Y(130.0)), (X(48.0), Y(150.0)), (X(43.0), Y(150.0)))
    lever("rm_x", X(12.5), Y(192.0), Y(214.0), 4); lever("rm_y", X(37.5), Y(192.0), Y(214.0), 4)
    for i, s in enumerate(("VCO1", "VCO2", "VCO3")): tag(s, X(23.5), Y(192.0 + 7.33 * i), 1.35)
    for i, s in enumerate(("VCO1", "VCO2", "NOISE")): tag(s, X(48.5), Y(192.0 + 7.33 * i), 1.35)
    line((X(12.5), Y(188.0)), (X(12.5), Y(158.0))); line((X(37.5), Y(188.0)), (X(37.5), Y(158.0)))
    jack("rm_x", X(16.0), Y(248.0), "in"); jack("rm_y", X(46.0), Y(248.0), "in")
    line((X(12.5), Y(216.0)), (X(12.5), Y(226.0)), (X(16.0), Y(226.0)), (X(16.0), Y(241.0)))
    line((X(37.5), Y(216.0)), (X(37.5), Y(226.0)), (X(46.0), Y(226.0)), (X(46.0), Y(241.0)))

def vcf(n, x0):
    code = "703E" if n == 1 else "703B"
    panel(code, x0, UP_Y, 98.0, 280.0, (f"VOLTAGE CONTROLLED FILTER -{n}",))
    X = lambda v: x0 + v
    Y = lambda v: UP_Y + v
    ins = ["VCO1", "VCO2", "VCO3", "WHITE NOISE" if n == 1 else "RING MOD"]
    xs = [15.6, 33.4, 50.6, 68.1, 85.5]
    for i, (xx, s) in enumerate(zip(xs, ins + [None])):
        if s: tag(s.replace("WHITE NOISE", "W.NOISE"), X(xx), Y(26.0), 1.45)
        else: jack(f"vcf{n}_in", X(xx), Y(25.0), "in")
        fader(f"vcf{n}_lvl{i+1}", X(xx), Y(43.5), Y(77.5), lab=(i == 0))
    line((X(15.6), Y(81.0)), (X(15.6), Y(84.0)), (X(85.5), Y(84.0)), (X(85.5), Y(81.0)))
    for xx in xs[1:4]: line((X(xx), Y(81.0)), (X(xx), Y(84.0)))
    line((X(50.6), Y(84.0)), (X(50.6), Y(102.5)))
    rect(X(31.0), Y(102.5), X(77.5), Y(172.0), radius=2.0)
    text("CUTOFF", X(46.5), Y(107.5), 1.7); text("FREQ.", X(46.5), Y(110.2), 1.7); text("RES.", X(65.0), Y(109.0), 1.7)
    slider(f"vcf{n}_cutoff", X(46.5), Y(116.0), Y(165.5))
    vscale(X(46.5), Y(116.0), Y(165.5), ("Hz", "10K", "1K", "100"), n=13, labx=X(38.6))
    slider(f"vcf{n}_res", X(65.0), Y(116.0), Y(165.5))
    vscale(X(65.0), Y(116.0), Y(165.5), ("10", "5", "0"), labx=X(58.6))
    text("VCF OUT", X(86.0), Y(93.0), 1.5); jack(f"vcf{n}_out", X(86.0), Y(101.5), "out")
    led(f"vcf{n}_over", X(86.0), Y(115.5), "red"); led(f"vcf{n}_sig", X(86.0), Y(123.5), "green")
    line((X(77.5), Y(137.0)), (X(86.0), Y(137.0)), (X(86.0), Y(127.0)))
    if n == 1:
        text("RES.", X(14.0), Y(130.0), 1.6); text("C.V. IN", X(14.0), Y(132.6), 1.6)
        jack("vcf1_rescv", X(14.0), Y(141.0), "in")
        line((X(20.5), Y(141.0)), (X(31.0), Y(141.0)), arrow=True)
    else:
        text("FILTER MODE", X(13.5), Y(128.0), 1.6)
        switch("vcf2_mode", X(13.0), Y(139.0), 3, "v", ("HP", "BP", "LP"), length=13.0)
        line((X(20.0), Y(139.0)), (X(31.0), Y(139.0)))
    line((X(54.0), Y(172.0)), (X(54.0), Y(180.5)))
    text("MODULATION", X(49.0), Y(187.5), 1.6)
    line((X(8.0), Y(191.0)), (X(8.0), Y(187.5)), (X(36.0), Y(187.5))); line((X(62.0), Y(187.5)), (X(90.0), Y(187.5)), (X(90.0), Y(191.0)))
    mx = [22.1, 40.3, 57.1, 75.8]
    for i, xx in enumerate(mx): fader(f"vcf{n}_mod{i+1}", X(xx), Y(199.5), Y(233.0), lab=(i == 0))
    tag("KEY CV", X(22.1), Y(250.0), 1.45)
    toggle(f"vcf{n}_adsr", X(40.3), Y(250.0), 2, ("ADSR1", "ADSR2"), lpos="horizontal")
    tag("LFO1", X(57.1), Y(250.0), 1.45)
    jack(f"vcf{n}_cv", X(75.8), Y(250.0), "in")

def vca(n, x0):
    panel(f"704{'A' if n == 1 else 'B'}", x0, UP_Y, 65.0, 280.0, ("VOLTAGE CONTROLLED", f"AMPLIFIER -{n}"))
    X = lambda v: x0 + v
    Y = lambda v: UP_Y + v
    tag("VCF1", X(14.9), Y(26.0), 1.45); tag("VCF2", X(31.6), Y(26.0), 1.45); jack(f"vca{n}_in", X(48.0), Y(25.0), "in")
    for i, xx in enumerate((14.9, 31.6, 48.0)): fader(f"vca{n}_lvl{i+1}", X(xx), Y(43.5), Y(77.5), lab=(i == 0))
    line((X(14.9), Y(81.0)), (X(14.9), Y(84.0)), (X(48.0), Y(84.0)), (X(48.0), Y(81.0))); line((X(31.6), Y(81.0)), (X(31.6), Y(84.0)))
    line((X(31.6), Y(84.0)), (X(31.6), Y(88.0)), (X(9.0), Y(88.0)), (X(9.0), Y(156.0)), (X(21.0), Y(156.0)), arrow=True)
    text("VCA OUT", X(48.5), Y(93.5), 1.5); jack(f"vca{n}_out", X(48.5), Y(102.0), "out")
    led(f"vca{n}_over", X(52.0), Y(114.0), "red"); led(f"vca{n}_sig", X(52.0), Y(121.0), "green")
    text("VCA GAIN", X(29.0), Y(103.0), 1.6)
    knob(f"vca{n}_gain", X(29.0), Y(121.0), "KM", labels=[("0", 0.0), ("5", 0.5), ("10", 1.0)])
    line((X(29.0), Y(131.5)), (X(29.0), Y(146.0)))
    rect(X(21.0), Y(146.0), X(42.0), Y(165.0), "VCA", 3.0, radius=1.6)
    line((X(42.0), Y(156.0)), (X(52.0), Y(156.0)), (X(52.0), Y(124.5)))
    toggle(f"vca{n}_exp", X(31.6), Y(173.5), 2, ("LIN", "EXP"), lpos="horizontal")
    text("MODULATION", X(31.6), Y(185.5), 1.6)
    for i, xx in enumerate((23.2, 41.0)): fader(f"vca{n}_mod{i+1}", X(xx), Y(199.5), Y(233.0), lab=(i == 0))
    toggle(f"vca{n}_adsr", X(23.2), Y(250.0), 2, ("ADSR1", "ADSR2"), lpos="horizontal")
    jack(f"vca{n}_cv", X(41.0), Y(250.0), "in")

def output(x0):
    panel("711A", x0, UP_Y, 197.0, 280.0, ())
    X = lambda v: x0 + v
    Y = lambda v: UP_Y + v
    text("REVERBERATOR", X(33.0), Y(12.0), 2.3, F_COND); text("PANNING", X(88.7), Y(12.0), 2.3, F_COND); text("PHASE SHIFTER", X(157.0), Y(12.0), 2.3, F_COND)
    tag("VCA1", X(17.7), Y(31.6), 1.45); tag("VCA2", X(34.2), Y(31.6), 1.45); jack("rev_in", X(49.7), Y(31.0), "in")
    for i, xx in enumerate((17.7, 34.2, 49.7)): fader(f"rev_lvl{i+1}", X(xx), Y(47.0), Y(81.0), lab=(i == 0))
    line((X(17.7), Y(84.5)), (X(17.7), Y(88.0)), (X(49.7), Y(88.0)), (X(49.7), Y(84.5))); line((X(34.2), Y(84.5)), (X(34.2), Y(105.0)), arrow=True)
    rect(X(24.0), Y(105.0), X(44.4), Y(122.0), "REV.", 2.8)
    line((X(34.2), Y(122.0)), (X(34.2), Y(133.0)), arrow=True)
    jack("rev_out", X(34.2), Y(140.5), "out"); text("REV. OUT", X(34.2), Y(149.0), 1.5)
    for i, (xx, s) in enumerate(((72.8, "VCA1"), (88.7, "VCA2"), (105.1, "REV"))):
        tag(s, X(xx), Y(23.5), 1.45)
        jack(f"pan_in{i+1}", X(xx), Y(33.0), "in", normal=["BUS_VCA1", "BUS_VCA2", "rev_out"][i])
        fader(f"pan_lvl{i+1}", X(xx), Y(47.0), Y(81.0), lab=(i == 0))
        tag(f"PAN{i+1}", X(xx), Y(94.5), 1.45)
    for i, yy in enumerate((117.0, 140.8, 165.0)):
        text(f"PAN{i+1}", X(88.7), Y(yy - 10.0), 1.55); text("L", X(70.5), Y(yy - 10.0), 1.5); text("R", X(106.5), Y(yy - 10.0), 1.5)
        slider(f"pan_pos{i+1}", X(70.0), X(107.4), Y(yy), horiz=True, detent=0.5)
        hscale(Y(yy) - 3.4, X(70.0), X(107.4), n=13)
    line((X(66.0), Y(100.0)), (X(66.0), Y(178.0)), (X(30.0), Y(178.0)), (X(30.0), Y(187.0)))
    line((X(111.0), Y(100.0)), (X(111.0), Y(178.0)), (X(147.0), Y(178.0)), (X(147.0), Y(187.0)))
    text("L  LR  R", X(88.7), Y(177.0), 1.45)
    switch("std_dest", X(88.7), Y(182.0), 3, "h", (), length=11.0)
    meter("vu_l", X(44.0), Y(200.0), 40.0, 26.0); text("L ch", X(44.0), Y(220.5), 1.7)
    meter("vu_r", X(149.5), Y(200.0), 40.0, 26.0); text("R ch", X(149.5), Y(220.5), 1.7)
    jack("out_l", X(44.0), Y(248.0), "out"); text("OUT", X(44.0), Y(256.5), 1.5)
    jack("out_r", X(149.5), Y(248.0), "out"); text("OUT", X(149.5), Y(256.5), 1.5)
    line((X(44.0), Y(223.5)), (X(44.0), Y(240.0)), arrow=True); line((X(149.5), Y(223.5)), (X(149.5), Y(240.0)), arrow=True)
    rect(X(78.0), Y(190.0), X(119.5), Y(266.0), radius=2.2)
    text("LEVEL", X(98.7), Y(195.5), 1.6)
    knob("std_level", X(98.7), Y(210.5), "KS", labels=[("OFF", 0.0), ("5", 0.5), ("10", 1.0)])
    knob("std_freq", X(98.7), Y(241.0), "KS", steps=6, a0=-90, a1=90, scale="legend",
         legends=["110", "220", "440", "880", "1760", "1KHz"])
    text("STANDARD OSC", X(98.7), Y(261.5), 1.7)
    tag("VCA1", X(138.4), Y(23.5), 1.45); tag("VCA2", X(154.0), Y(23.5), 1.45)
    jack("ps_in1", X(138.4), Y(33.0), "in", normal="BUS_VCA1"); jack("ps_in2", X(154.0), Y(33.0), "in", normal="BUS_VCA2")
    text("RESONANCE", X(178.4), Y(37.5), 1.5)
    for i, xx in enumerate((138.4, 154.0)): fader(f"ps_lvl{i+1}", X(xx), Y(47.0), Y(81.0), lab=(i == 0))
    fader("ps_res", X(178.4), Y(47.0), Y(81.0))
    line((X(138.4), Y(84.5)), (X(138.4), Y(88.0)), (X(154.0), Y(88.0)), (X(154.0), Y(84.5))); line((X(146.0), Y(88.0)), (X(146.0), Y(96.0)), arrow=True)
    line((X(178.4), Y(84.5)), (X(178.4), Y(90.0)), (X(162.0), Y(90.0)), (X(162.0), Y(96.0)))
    rect(X(135.0), Y(96.0), X(162.5), Y(112.0), "PHASE", 2.6)
    text("P.S. OUT", X(178.0), Y(98.0), 1.45); jack("ps_out", X(178.0), Y(106.5), "out")
    line((X(162.5), Y(104.0)), (X(171.0), Y(104.0)))
    text("AUTO SPEED", X(142.9), Y(127.0), 1.55); text("INTENSITY", X(168.6), Y(127.0), 1.55)
    knob("ps_speed", X(142.9), Y(141.0), "KS", labels=[("0.03", 0.0), ("0.1", 0.22), ("0.5", 0.46), ("1", 0.56), ("2", 0.66), ("5", 0.86), ("10Hz", 1.0)])
    knob("ps_int", X(168.6), Y(141.0), "KS", labels=[("0", 0.0), ("5", 0.5), ("10", 1.0)])
    switch("ps_auto", X(170.5), Y(165.0), 2, "h", (), length=9.0)
    text("AUTO", X(160.0), Y(165.0), 1.4); text("EXT", X(181.0), Y(165.0), 1.4)
    jack("ps_ext", X(177.5), Y(181.0), "in"); text("EXT IN", X(177.5), Y(189.5), 1.5)
    line((X(142.9), Y(150.0)), (X(142.9), Y(158.0)), (X(166.0), Y(158.0)), (X(166.0), Y(161.0)))

# ═════════════ CONSOLA: fila inferior (140 mm) ═════════════
def monitor(x0):
    panel("712A", x0, LO_Y, 131.0, 140.0, ())
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    text("MONITOR", X(33.0), Y(11.5), 2.3, F_COND); text("VOLTAGE PROCESSOR", X(98.0), Y(11.5), 2.3, F_COND)
    line((X(65.5), Y(6.0)), (X(65.5), Y(132.0)))
    text("MONITOR LEVEL", X(33.0), Y(24.0), 1.6)
    knob("monitor", X(33.0), Y(42.0), "KM", labels=[("0", 0.0), ("5", 0.5), ("10", 1.0)])
    text("STEREO HEADPHONES", X(33.0), Y(66.5), 1.45)
    extra["phones"] = dict(x=X(33.0), y=Y(75.5))
    rect(X(8.5), Y(86.0), X(61.0), Y(116.0), radius=1.6)
    text("EXT CONTROLLER", X(34.5), Y(91.0), 1.6)
    extra["din"] = dict(x=X(24.0), y=Y(104.0))
    switch("extctl", X(45.0), Y(104.0), 2, "v", ("CONSOLE", "OFF"), length=8.0)
    tag("+10V", X(87.8), Y(22.5), 1.45)
    fader("vp_level", X(87.8), Y(31.0), Y(80.0))
    rect(X(104.5), Y(29.0), X(127.5), Y(87.0), radius=1.6)
    text("KEY C.V.", X(116.0), Y(33.5), 1.45); text("OUT", X(116.0), Y(35.9), 1.45)
    jack("kb_cvout", X(116.0), Y(45.5), "out")
    text("GATE", X(116.0), Y(65.0), 1.45); text("OUT", X(116.0), Y(67.4), 1.45)
    jack("kb_gateout", X(116.0), Y(77.0), "out")
    line((X(87.8), Y(83.5)), (X(87.8), Y(94.0)), (X(82.2), Y(94.0)), (X(82.2), Y(104.0)))
    line((X(87.8), Y(94.0)), (X(95.0), Y(94.0)))
    extra.setdefault("amps", []).append(dict(x=X(100.0), y=Y(94.0), s=5.0))
    line((X(105.0), Y(94.0)), (X(116.0), Y(94.0)), (X(116.0), Y(104.0)))
    text("+OUT", X(82.2), Y(102.0), 1.45); jack("vp_pos", X(82.2), Y(112.0), "out")
    text("-OUT", X(116.0), Y(102.0), 1.45); jack("vp_neg", X(116.0), Y(112.0), "out")

LFO_LABELS = [("0.03", 0.0), ("0.05", 0.074), ("0.1", 0.17), ("0.2", 0.27), ("0.5", 0.4), ("1", 0.5), ("2", 0.6), ("5", 0.73), ("10", 0.83), ("20", 0.93), ("30Hz", 1.0)]
def lfo(n, x0):
    code = "706A" if n == 1 else "706B"
    panel(code, x0, LO_Y, 65.0, 140.0, ("LOW FREQUENCY", "OSCILLATOR" + (" -2" if n == 2 else "")), tsize=2.2)
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    if n == 1:
        text("DELAY TIME", X(17.7), Y(28.0), 1.6)
        knob("lfo1_delay", X(17.7), Y(43.5), "KS", labels=[("2ms", 0.0), ("100", 0.36), ("500", 0.55), ("1", 0.64), ("2", 0.73), ("5", 0.86), ("10sec", 1.0)])
    else:
        text("PULSE WIDTH", X(17.7), Y(28.0), 1.6)
        knob("lfo2_pw", X(17.7), Y(43.5), "KS", labels=[("10%", 0.0), ("50%", 0.5), ("90%", 1.0)])
    jack(f"lfo{n}_out", X(46.5), Y(28.0), "out"); text("LFO", X(55.5), Y(26.8), 1.4); text("OUT", X(55.5), Y(29.2), 1.4)
    led(f"lfo{n}", X(46.5), Y(41.5), "red")
    switch(f"lfo{n}_wave", X(46.5), Y(53.5), 4, "h", (), length=15.0)
    extra.setdefault("wavesyms", []).extend(dict(kind=w, x=X(46.5) + (i - 1.5) * 3.75, y=Y(59.5), s=0.55) for i, w in enumerate(("pulse" if n == 2 else "square", "rsaw", "tri", "sine")))
    text("FREQ C.V. IN", X(12.5), Y(74.0), 1.5)
    jack(f"lfo{n}_cv", X(12.5), Y(84.0), "in")
    text("KEY TRIG.", X(12.5), Y(99.5), 1.6)
    toggle(f"lfo{n}_keytrig", X(12.5), Y(110.0), 2, ("ON", "OFF"))
    text("FREQUENCY", X(43.5), Y(74.0), 1.6)
    knob(f"lfo{n}_freq", X(43.5), Y(99.0), "KL", labels=LFO_LABELS)

def sample_hold(x0):
    panel("709A", x0, LO_Y, 65.0, 140.0, ("SAMPLE & HOLD",), tsize=2.2)
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    text("SAMPLE MODE", X(25.0), Y(23.0), 1.55)
    switch("sh_mode", X(25.5), Y(36.5), 3, "v", (), length=11.0)
    tag("LFO1", X(13.0), Y(32.8), 1.35); tag("NOISE", X(13.0), Y(37.0), 1.35)
    rect(X(34.0), Y(31.0), X(51.0), Y(42.0), "S.& H.", 2.0)
    line((X(29.0), Y(36.5)), (X(34.0), Y(36.5)), arrow=True)
    text("S&H OUT", X(53.5), Y(17.5), 1.45); jack("sh_out", X(53.5), Y(25.5), "out")
    line((X(51.0), Y(36.5)), (X(53.5), Y(36.5)), (X(53.5), Y(32.0)))
    jack("sh_in", X(11.0), Y(55.0), "in"); text("EXT SIG.", X(11.0), Y(63.5), 1.45)
    line((X(11.0), Y(48.5)), (X(11.0), Y(44.5)), (X(25.5), Y(44.5)), (X(25.5), Y(42.0)))
    text("CLOCK OUT", X(53.5), Y(47.0), 1.45); jack("sh_clkout", X(53.5), Y(55.0), "out")
    switch("sh_clock", X(30.5), Y(72.0), 2, "h", (), length=9.0)
    text("EXT", X(23.0), Y(76.5), 1.35); text("INT", X(38.0), Y(76.5), 1.35)
    line((X(30.5), Y(68.0)), (X(30.5), Y(46.0)), (X(42.5), Y(46.0)), (X(42.5), Y(42.0)))
    line((X(35.5), Y(72.0)), (X(46.5), Y(72.0)), (X(46.5), Y(81.0)))
    text("CLOCK RATE", X(42.5), Y(84.0), 1.55)
    knob("sh_rate", X(46.5), Y(103.0), "KS", labels=[("0.3", 0.0), ("0.5", 0.11), ("1", 0.26), ("2", 0.41), ("5", 0.61), ("10", 0.76), ("30Hz", 1.0)])
    text("EXT CLOCK", X(11.0), Y(96.0), 1.45); jack("sh_clkin", X(11.0), Y(106.0), "in")
    line((X(11.0), Y(89.5)), (X(11.0), Y(80.0)), (X(25.5), Y(80.0)), (X(25.5), Y(76.0)))

def amp_ef(x0):
    panel("707A", x0, LO_Y, 131.0, 140.0, ())
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    text("AMPLIFIER", X(65.5), Y(13.0), 2.2, F_COND)
    jack("amp_in", X(10.0), Y(32.5), "in"); text("IN", X(10.0), Y(24.5), 1.45)
    rect(X(30.0), Y(21.0), X(53.0), Y(42.0), radius=1.6)
    switch("amp_range", X(36.5), Y(31.5), 3, "v", (), length=11.0)
    for i, s in enumerate(("x1000", "x100", "x10")): text(s, X(45.0), Y(27.6 + 3.9 * i), 1.35, anchor="lm")
    line((X(16.5), Y(32.5)), (X(30.0), Y(32.5)), arrow=True); line((X(53.0), Y(32.5)), (X(66.0), Y(32.5)))
    slider("amp_gain", X(68.0), X(101.0), Y(32.5), horiz=True)
    hscale(Y(32.5) - 3.6, X(68.0), X(101.0), ("0", "0.5", "1"), n=11)
    line((X(104.0), Y(32.5)), (X(110.5), Y(32.5)), arrow=True)
    jack("amp_out", X(116.7), Y(32.5), "out"); text("OUT", X(116.7), Y(24.5), 1.45)
    line((X(7.0), Y(48.0)), (X(124.0), Y(48.0)))
    text("ENVELOPE FOLLOWER", X(65.5), Y(55.0), 2.2, F_COND)
    tag("AMP", X(22.0), Y(61.0), 1.35)
    jack("ef_in", X(10.0), Y(72.0), "in", normal="amp_out"); text("IN", X(10.0), Y(64.0), 1.45)
    rect(X(55.0), Y(62.0), X(85.0), Y(83.0), "E.F.", 2.4)
    led("ef", X(78.5), Y(72.5), "green")
    line((X(16.5), Y(72.0)), (X(55.0), Y(72.0)), arrow=True); line((X(85.0), Y(72.0)), (X(110.5), Y(72.0)), arrow=True)
    jack("ef_out", X(116.7), Y(72.0), "out"); text("OUT", X(116.7), Y(64.0), 1.45)
    line((X(7.0), Y(88.0)), (X(124.0), Y(88.0)))
    text("INTEGRATER", X(65.5), Y(94.5), 2.2, F_COND)
    tag("E.F", X(22.0), Y(100.0), 1.35)
    jack("int_in", X(10.0), Y(109.0), "in", normal="ef_out"); text("IN", X(10.0), Y(101.0), 1.45)
    slider("int_lag", X(48.0), X(83.0), Y(109.0), horiz=True)
    hscale(Y(109.0) - 3.6, X(48.0), X(83.0), ("0", "0.1", "0.2", "0.5", "1", "2", "3 sec"), n=7)
    text("LAG TIME", X(65.5), Y(116.5), 1.55)
    line((X(16.5), Y(109.0)), (X(45.0), Y(109.0))); line((X(86.0), Y(109.0)), (X(110.5), Y(109.0)), arrow=True)
    jack("int_out", X(116.7), Y(109.0), "out"); text("OUT", X(116.7), Y(101.0), 1.45)

ADSR_T = ("10", "5", "2", "1", "500", "200", "100", "2")
def adsr(x0):
    panel("705A", x0, LO_Y, 131.0, 140.0, ("DUAL ENVELOPE GENERATOR",), tsize=2.2)
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    for n, dy in ((1, 0.0), (2, 53.0)):
        text(f"ADSR -{n}", X(65.5), Y(19.0 + dy) if n == 1 else Y(73.5), 2.2, F_COND)
        text("GATE", X(10.5), Y(27.5 + dy), 1.4); text("+", X(10.5), Y(29.8 + dy), 1.4); text("TRIG", X(10.5), Y(32.0 + dy), 1.4)
        toggle(f"adsr{n}_mode", X(14.0), Y(37.0 + dy), 3, ("", "GATE", ""), lpos="left")
        text("EXT GATE", X(12.0), Y(44.0 + dy), 1.4)
        jack(f"adsr{n}_gate", X(14.0), Y(52.5 + dy), "in")
        for i, (p, lab) in enumerate((("a", "ATTACK TIME"), ("d", "DECAY TIME"), ("s", "SUSTAIN LEVEL"), ("r", "RELEASE TIME"))):
            xx = 34.0 + 21.5 * i
            slider(f"adsr{n}_{p}", X(xx), Y(29.0 + dy), Y(59.5 + dy))
            vscale(X(xx), Y(29.0 + dy), Y(59.5 + dy), ("10", "5", "0") if p == "s" else ADSR_T, n=8 if p != "s" else 11, labx=X(xx - 7.6))
            text(lab, X(xx), Y(65.0 + dy), 1.35)
        text("TIME", X(118.0), Y(26.0 + dy), 1.45)
        toggle(f"adsr{n}_time", X(118.0), Y(35.5 + dy), 2, ("x1", "x1/10"))
        text("OUT", X(118.0), Y(46.5 + dy), 1.45)
        jack(f"adsr{n}_out", X(118.0), Y(55.0 + dy), "out")
    line((X(24.0), Y(68.8)), (X(127.0), Y(68.8)))
    button("adsr_manual", X(14.0), Y(70.5), "round", "white", d=10.0)
    text("MANUAL GATE", X(14.0), Y(78.5), 1.4)

def mixer(x0):
    panel("716A", x0, LO_Y, 65.0, 140.0, ("MIXER",), tsize=2.2)
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    for i, xx in enumerate((15.4, 33.1, 50.6)):
        jack(f"mix_in{i+1}", X(xx), Y(25.5), "in")
        fader(f"mix_lvl{i+1}", X(xx), Y(40.0), Y(73.0), lab=(i == 0))
    line((X(15.4), Y(77.0)), (X(15.4), Y(80.0)), (X(50.6), Y(80.0)), (X(50.6), Y(77.0))); line((X(33.1), Y(77.0)), (X(33.1), Y(84.0)))
    extra.setdefault("amps", []).extend([dict(x=X(30.0), y=Y(95.5), s=5.0), dict(x=X(30.0), y=Y(112.0), s=5.0)])
    line((X(33.1), Y(84.0)), (X(20.0), Y(84.0)), (X(20.0), Y(95.5)), (X(25.0), Y(95.5)))
    line((X(35.0), Y(95.5)), (X(44.5), Y(95.5))); line((X(40.0), Y(95.5)), (X(40.0), Y(103.0)), (X(20.0), Y(103.0)), (X(20.0), Y(112.0)), (X(25.0), Y(112.0)))
    line((X(35.0), Y(112.0)), (X(44.5), Y(112.0)))
    text("INV. OUT", X(51.0), Y(87.5), 1.45); jack("mix_inv", X(51.0), Y(95.5), "out")
    text("NON-INV. OUT", X(48.0), Y(104.0), 1.4); jack("mix_out", X(51.0), Y(112.0), "out")

def multiple(x0):
    panel("710A", x0, LO_Y, 65.0, 140.0, ("MULTIPLE JACK",), tsize=2.2)
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    for c, xx in enumerate((20.0, 45.0)):
        line((X(xx), Y(24.0)), (X(xx), Y(112.0)))
        for r in range(5):
            jack(f"mult{c+1}_{r+1}", X(xx), Y(24.0 + 22.0 * r), "mult")

def power(x0):
    panel("718A", x0, LO_Y, 65.0, 140.0, (), foot=False)
    X = lambda v: x0 + v
    Y = lambda v: LO_Y + v
    rect(X(10.0), Y(10.0), X(55.0), Y(56.0), radius=2.4)
    text("Audio Tools", X(32.5), Y(24.0), 3.6, F_BOLD)
    text("SYNTHESIZER", X(32.5), Y(36.0), 2.6, F_COND)
    text("AT-700", X(32.5), Y(44.0), 3.2, F_CBLACK)
    text("POWER", X(32.5), Y(80.0), 2.0, F_COND)
    toggle("power", X(32.5), Y(98.0), 2, ("ON", "OFF"))
    led("power", X(48.5), Y(98.0), "red")
    text("718A", X(61.0), Y(134.0), 1.7, F_MED, anchor="rm")

# ═════════════ SECUENCIADOR 717A ═════════════
STEP_X = [196.0 + 42.25 * i for i in range(12)]
def sequencer():
    x0, y0 = SEQ["x0"], SEQ["y0"]
    panel("717A", x0, y0, SEQ["w"], SEQ["h"], (), foot=True)
    X = lambda v: x0 + v
    Y = lambda v: y0 + v
    text("ANALOG SEQUENCER", X(430.0), Y(12.5), 2.6, F_COND)
    # Pulse shaper
    rect(X(21.0), Y(24.0), X(61.0), Y(216.0), radius=2.0)
    text("PULSE", X(41.0), Y(31.5), 2.0, F_COND); text("SHAPER", X(41.0), Y(35.5), 2.0, F_COND)
    text("OUT", X(41.0), Y(47.5), 1.45)
    jack("ps1_out1", X(31.3), Y(56.0), "out"); jack("ps1_out2", X(50.8), Y(56.0), "out")
    led("pshaper", X(41.0), Y(66.0), "red")
    text("SWITCH-ON TIME", X(41.0), Y(76.0), 1.45)
    knob("pshaper_time", X(41.0), Y(92.0), "KS", labels=[("0", 0.0), ("0.1", 0.25), ("0.2", 0.4), ("1", 0.75), ("2", 0.9), ("3sec", 1.0)])
    text("THRESHOLD LEVEL", X(41.0), Y(113.0), 1.45)
    knob("pshaper_thr", X(41.0), Y(130.0), "KS", labels=[("0", 0.0), ("5", 0.5), ("10", 1.0)])
    line((X(41.0), Y(140.0)), (X(41.0), Y(180.0)))
    text("PULSE IN", X(41.0), Y(184.0), 1.45); jack("ps1_in", X(41.0), Y(194.0), "in")
    # Master controller
    rect(X(69.5), Y(24.0), X(152.5), Y(117.0), radius=2.0)
    text("MASTER CONTROLLER", X(111.0), Y(31.5), 2.0, F_COND)
    for i, (nm, col) in enumerate((("START", "white"), ("CONTINUE", "white"), ("STOP", "orange"), ("END", "orange"))):
        xx = 82.4 + 19.13 * i
        led(f"seq_{nm.lower()}", X(xx), Y(39.5), "red")
        text(nm, X(xx), Y(46.0), 1.35)
        button(f"seq_{nm.lower()}", X(xx), Y(58.5), "square", col, w=16.5, h=15.0)
        jack(f"seq_cp_{nm.lower()}", X(xx), Y(82.5), "in")
    text("CONTROL PULSE IN", X(111.0), Y(92.0), 1.4)
    jack("seq_startout", X(82.4), Y(104.5), "out"); text("START PULSE", X(98.0), Y(103.3), 1.3); text("OUT", X(98.0), Y(105.6), 1.3)
    jack("seq_endout", X(139.8), Y(104.5), "out"); text("END PULSE", X(124.0), Y(103.3), 1.3); text("OUT", X(124.0), Y(105.6), 1.3)
    # Clock oscillator
    rect(X(69.5), Y(124.5), X(152.5), Y(249.0), radius=2.0)
    text("CLOCK OSCILLATOR", X(111.0), Y(132.0), 2.0, F_COND)
    text("PULSE WIDTH", X(111.0), Y(140.0), 1.45)
    knob("clk_pw", X(111.0), Y(154.0), "KS", labels=[("10%", 0.0), ("50%", 0.5), ("90%", 1.0)])
    led("seq_clock", X(135.0), Y(154.0), "red")
    text("CLOCK RATE", X(111.0), Y(172.5), 1.45)
    knob("clk_rate", X(111.0), Y(195.0), "KL", labels=[("2", 0.0), ("3", 0.18), ("5", 0.4), ("8", 0.6), ("10", 0.7), ("15", 0.87), ("20", 1.0)])
    text("(STEP/MINUTE)", X(111.0), Y(212.5), 1.35)
    text("RANGE", X(83.0), Y(184.0), 1.4)
    toggle("clk_range", X(83.5), Y(195.0), 2, ("x10", "x1"), lpos="left")
    text("CONTROL", X(141.5), Y(184.5), 1.3); text("VOLTAGE", X(141.5), Y(186.8), 1.3); text("IN", X(141.5), Y(189.1), 1.3)
    jack("clk_cv", X(141.5), Y(197.5), "in")
    text("EXT CLOCK", X(92.0), Y(225.0), 1.4); jack("clk_ext", X(92.0), Y(235.0), "in")
    text("CLOCK OUT", X(133.0), Y(225.0), 1.4)
    jack("clk_out1", X(124.5), Y(235.0), "out"); jack("clk_out2", X(141.5), Y(235.0), "out")
    # Pasos
    for lab, yy in (("CH-A", 40.0), ("CH-B", 73.0), ("CH-C", 105.5)): text(lab, X(171.5), Y(yy), 1.9, F_COND)
    text("DIVIDE", X(166.0), Y(142.0), 1.35); text("FINE", X(166.0), Y(175.5), 1.35)
    text("STEP", X(172.5), Y(157.0), 1.35); text("TIME", X(172.5), Y(159.3), 1.35)
    line((X(172.5), Y(145.0)), (X(172.5), Y(154.5))); line((X(172.5), Y(161.5)), (X(172.5), Y(172.0)))
    for i, sx in enumerate(STEP_X):
        s = i + 1
        text(str(s), X(sx), Y(21.0), 2.0, F_COND)
        for ch, yy in (("a", 40.0), ("b", 73.0), ("c", 105.5)):
            knob(f"step{s}_{ch}", X(sx), Y(yy), "KQ", labels=[("0", 0.0), ("10", 1.0)], ticks=11)
        knob(f"step{s}_div", X(sx), Y(142.5), "KQ", steps=6, a0=-120, a1=60, scale="legend",
             legends=["1/32", "1/16", "1/8", "1/4", "1/2", "1"])
        knob(f"step{s}_fine", X(sx), Y(175.5), "KQ", labels=[("1", 0.0), ("1.5", 0.5), ("2", 1.0)])
        switch(f"step{s}_hold", X(sx), Y(195.0), 2, "h", (), length=8.0)
        text("NORMAL", X(sx - 5.0), Y(200.0), 1.2); text("HOLD", X(sx + 5.0), Y(200.0), 1.2)
        led(f"step{s}", X(sx), Y(214.0), "red")
        text("MANUAL", X(sx), Y(221.0), 1.25)
        button(f"step{s}_manual", X(sx), Y(229.5), "round", "red", d=8.5)
        text("ANALOG IN", X(sx - 8.3), Y(243.5), 1.15); text("GATE OUT", X(sx + 8.3), Y(243.5), 1.15)
        jack(f"step{s}_in", X(sx - 8.3), Y(252.0), "in"); jack(f"step{s}_gate", X(sx + 8.3), Y(252.0), "out")
    # Parallel out
    rect(X(690.0), Y(24.0), X(779.0), Y(131.0), radius=2.0)
    text("RANGE", X(720.0), Y(29.5), 1.4); text("OUTPUT", X(759.0), Y(29.5), 1.4)
    for ch, yy in (("a", 41.0), ("b", 72.5), ("c", 104.0)):
        text(f"CH {ch.upper()}", X(700.0), Y(yy), 1.5)
        switch(f"par_range_{ch}", X(718.0), Y(yy), 3, "v", ("10V", "4V", "2V"), length=10.0, lpos="right")
        line((X(729.0), Y(yy)), (X(743.0), Y(yy)))
        jack(f"par_{ch}1", X(749.0), Y(yy), "out"); jack(f"par_{ch}2", X(769.5), Y(yy), "out")
    text("PARALLEL OUT", X(734.5), Y(124.5), 2.0, F_COND)
    # Series out
    rect(X(690.0), Y(138.0), X(779.0), Y(258.0), radius=2.0)
    for ch, xx in (("A", 704.3), ("B", 731.8), ("C", 758.6)):
        text(f"CH {ch}", X(xx), Y(145.5), 1.45)
        led(f"ser_{ch.lower()}", X(xx), Y(153.0), "red")
        text("MANUAL", X(xx), Y(160.5), 1.25)
        button(f"ser_manual_{ch.lower()}", X(xx), Y(170.0), "round", "red", d=8.5)
    text("MODE", X(706.0), Y(186.5), 1.4)
    switch("ser_mode", X(699.0), Y(204.0), 4, "v", ("A", "A-B", "A-B-C", "MANUAL"), length=15.0, lpos="right")
    switch("ser_repeat", X(724.0), Y(201.0), 2, "v", ("STOP", "REPEAT"), length=8.0, lpos="right")
    text("STEP SELECTOR", X(758.6), Y(186.5), 1.35)
    knob("ser_steps", X(758.6), Y(204.0), "KS", steps=12, a0=-150, a1=150, scale="legend",
         legends=[str(i) for i in range(1, 13)])
    jack("ser_out1", X(723.5), Y(237.0), "out"); jack("ser_out2", X(744.0), Y(237.0), "out")
    text("SERIES OUT", X(734.0), Y(249.5), 2.0, F_COND)

# ═════════════ TECLADO 701A ═════════════
def keyboard():
    x0, y0, w, h = KB_PANEL["x0"], KB_PANEL["y0"], KB_PANEL["w"], KB_PANEL["h"]
    panel("701A", x0, y0, w, h, (), foot=True)
    X = lambda v: x0 + v
    Y = lambda v: y0 + v
    text("KEYBOARD CONTROLLER", X(w / 2), Y(9.0), 2.2, F_COND)
    tag("KEY CV OUT", X(23.0), Y(18.5), 1.35)
    text("PITCH", X(60.0), Y(18.0), 1.6); text("PORTAMENTO TIME", X(108.0), Y(18.0), 1.5)
    knob("kb_pitch", X(60.0), Y(33.0), "KM", labels=[(str(v), (v + 6) / 12) for v in range(-6, 7, 2)])
    text("(SEMITONE)", X(60.0), Y(46.5), 1.3)
    knob("kb_porta", X(108.0), Y(35.0), "KS", labels=[("0.1", 0.0), ("1", 0.5), ("5", 0.85), ("10sec", 1.0)])
    line((X(23.0), Y(21.5)), (X(23.0), Y(50.0)))
    switch("kb_bendrange", X(21.0), Y(62.0), 4, "v", ("5V", "1V", "0.1V", "OFF"), length=13.0, lpos="right")
    text("PORTAMENTO", X(118.0), Y(52.5), 1.35)
    toggle("kb_portaon", X(110.0), Y(60.0), 2, ("OFF", "ON"), lpos="right")
    text("BENDER", X(66.0), Y(77.5), 1.5)
    slider("kb_bender", X(38.0), X(94.0), Y(84.0), horiz=True, detent=0.5)
    text("−", X(36.0), Y(91.0), 2.0); text("0", X(66.0), Y(91.0), 1.6); text("+", X(96.0), Y(91.0), 2.0)
    # Extensión: los jacks del panel trasero, al frente, más velocidad, rueda y aftertouch por MIDI.
    line((X(6.0), Y(99.0)), (X(w - 6.0), Y(99.0)))
    text("REAR PANEL OUTPUTS", X(w / 2), Y(104.0), 1.5)
    for i, (jid, lab) in enumerate((("kb_cv1", "KEY CV1"), ("kb_cv2", "KEY CV2"), ("kb_gate", "GATE OUT"), ("kb_trig", "KEY TRIG."), ("kb_bend", "BEND CV"))):
        xx = 17.0 + 29.0 * i
        if i == 4: xx = 133.0
        jack(jid, X(xx if i < 4 else 133.0), Y(117.0), "out", lab, lsize=1.35)
    text("MIDI", X(w / 2), Y(131.5), 1.5)
    for i, (jid, lab) in enumerate((("kb_vel", "VELOCITY"), ("kb_mod", "MOD WHEEL"), ("kb_at", "AFTERTOUCH"))):
        jack(jid, X(35.0 + 40.0 * i), Y(146.5), "out", lab, lsize=1.35)

# ───────── armado ─────────
for n, x in ((1, CX0), (2, CX0 + 65.0), (3, CX0 + 130.0)): MODULE[0] = ""; vco(n, x)
noise_rm(CX0 + 195.0)
vcf(1, CX0 + 260.0); vcf(2, CX0 + 358.0)
vca(1, CX0 + 456.0); vca(2, CX0 + 521.0)
output(CX0 + 586.0)
monitor(CX0); lfo(1, CX0 + 131.0); lfo(2, CX0 + 196.0); sample_hold(CX0 + 261.0); amp_ef(CX0 + 326.0)
adsr(CX0 + 457.0); mixer(CX0 + 588.0); multiple(CX0 + 653.0); power(CX0 + 718.0)
sequencer()
keyboard()

# Teclas del 701A: 61 (C a C), 36 blancas.
def build_keys():
    ks = []
    n_white = 36
    ww = (KEYS["x1"] - KEYS["x0"]) / n_white
    wi = 0
    for k in range(KEYS["n"]):
        pc = k % 12
        black = pc in (1, 3, 6, 8, 10)
        if not black:
            ks.append(dict(i=k, black=False, x0=KEYS["x0"] + wi * ww, x1=KEYS["x0"] + (wi + 1) * ww))
            wi += 1
        else:
            cx = KEYS["x0"] + wi * ww
            ks.append(dict(i=k, black=True, x0=cx - ww * 0.29, x1=cx + ww * 0.29))
    return ks
extra["keylist"] = build_keys()

# ───────── módulos por voz (polifonía) ─────────
# Los módulos de cada voz se duplican en modo poly; el resto es global (LFO, S&H, salida, secuenciador).
VOICE_MODULES = {"702A", "702B", "702C", "708A", "703E", "703B", "704A", "704B", "705A", "707A", "716A", "710A", "701A"}
GLOBAL_JACKS = {"noise_white", "noise_pink", "kb_bend", "kb_mod", "kb_at"}

def write_layout():
    d = dict(size=[W_MM, H_MM], knobs=knobs, sliders=sliders, switches=switches, toggles=toggles, levers=levers,
             buttons=buttons, leds=leds, jacks=jacks, meters=meters, panels=panels, **extra)
    os.makedirs(os.path.join(ROOT, "assets"), exist_ok=True)
    json.dump(d, open(os.path.join(ROOT, "assets", "layout700.json"), "w"), indent=1)

NORMALS = ["BUS_VCA1", "BUS_VCA2"]
def write_jacks_header():
    ids = [j["id"] for j in jacks]
    L = ["#pragma once", "// Generado por tools/panel700.py — no editar a mano.", "", "#include <cstdint>", "", "namespace dh::jk700 {", "",
         "enum Jack : int", "{"]
    L += [f"    {j['id']}," for j in jacks]
    L += ["    kNumJacks", "};", "",
          "// Fuentes internas que no son jacks (buses de la consola) para las entradas con conexión de corte.",
          "enum Normal : int { None = -1, " + ", ".join(f"{n} = {1000 + i}" for i, n in enumerate(NORMALS)) + " };", "",
          "enum Io : int8_t { In = 0, Out = 1, Mult = 2 };", "",
          "struct JackDef { const char* id; Io io; int normal; bool voice; float x, y; };", "",
          "inline const JackDef jacks[kNumJacks] = {"]
    for j in jacks:
        n = j["normal"]
        ns = "None" if n is None else n
        io = {"in": "In", "out": "Out", "mult": "Mult"}[j["io"]]
        voice = j["module"] in VOICE_MODULES and j["id"] not in GLOBAL_JACKS
        L.append(f"    {{ \"{j['id']}\", {io}, {ns}, {'true' if voice else 'false'}, {j['x']:.2f}f, {j['y']:.2f}f }},")
    L += ["};", "", "} // namespace dh::jk700", ""]
    open(os.path.join(ROOT, "Source", "dsp", "Jacks700.generated.h"), "w").write("\n".join(L))

# ───────── serigrafía ─────────
_fonts = {}
def font(mm, idx=F_BOLD):
    key = (max(6, round(mm * PX)), idx)
    if key not in _fonts: _fonts[key] = ImageFont.truetype(FONT, key[0], index=idx)
    return _fonts[key]

def draw_wave(d, kind, x, y, s, w):
    """Símbolo de forma de onda (s = escala, mm)."""
    pts = {"sine": [(t / 20.0, -math.sin(t / 20.0 * 2 * math.pi)) for t in range(21)],
           "tri": [(0, 0), (0.25, -1), (0.75, 1), (1, 0)],
           "saw": [(0, 1), (0.5, -1), (0.5, 1), (1, -1), (1, 1)],
           "rsaw": [(0, -1), (0.5, 1), (0.5, -1), (1, 1), (1, -1)],
           "pulse": [(0, 1), (0, -1), (0.3, -1), (0.3, 1), (0.6, 1), (0.6, -1), (0.9, -1), (0.9, 1)],
           "square": [(0, 1), (0, -1), (0.5, -1), (0.5, 1), (1, 1), (1, -1)]}[kind]
    sx, sy = 6.0 * s, 2.2 * s
    P = [((x - sx / 2 + px * sx) * PX, (y + py * sy) * PX) for px, py in pts]
    d.line(P, fill=255, width=w)

def write_silkscreen():
    Wp, Hp = int(W_MM * PX), int(H_MM * PX)
    ink = Image.new("L", (Wp, Hp), 0)
    di = ImageDraw.Draw(ink)
    P = lambda v: v * PX
    lw = max(2, round(P(0.28)))

    def txt(s, x, y, mm, face=F_BOLD, anchor="mm", fill=255, dr=None):
        (dr or di).text((P(x), P(y)), s, font=font(mm, face), fill=fill, anchor=anchor)

    for t in texts:
        if "\n" in t["s"]: continue
        txt(t["s"], t["x"], t["y"], t["mm"], t["face"], t["anchor"])
    for ln in lines:
        pts = [(P(x), P(y)) for x, y in ln["pts"]]
        di.line(pts, fill=255, width=lw, joint="curve")
        if ln["arrow"] and len(pts) >= 2:
            (x1, y1), (x2, y2) = pts[-2], pts[-1]
            a = math.atan2(y2 - y1, x2 - x1); s = P(1.1)
            di.polygon([(x2, y2), (x2 - s * math.cos(a - 0.45), y2 - s * math.sin(a - 0.45)), (x2 - s * math.cos(a + 0.45), y2 - s * math.sin(a + 0.45))], fill=255)
    for r in rects:
        di.rounded_rectangle([P(r["x0"]), P(r["y0"]), P(r["x1"]), P(r["y1"])], radius=P(r["radius"]), outline=255, width=lw)
        if r["label"]:
            parts = r["label"].split("\n")
            for i, s in enumerate(parts):
                txt(s, (r["x0"] + r["x1"]) / 2, (r["y0"] + r["y1"]) / 2 + (i - (len(parts) - 1) / 2) * r["size"] * 1.15, r["size"], F_COND)
    for tg in tags:
        f = font(tg["size"], F_COND)
        bb = f.getbbox(tg["text"])
        tw = (bb[2] - bb[0]) / PX + 1.6
        th = tg["size"] * 1.25
        x0, y0 = tg["x"] - tw / 2, tg["y"] - th / 2
        di.rectangle([P(x0), P(y0), P(x0 + tw), P(y0 + th)], outline=255, width=max(2, round(P(0.2))))
        txt(tg["text"], tg["x"], tg["y"] + 0.05, tg["size"], F_COND)
    for w in extra.get("wavesyms", []):
        draw_wave(di, w["kind"], w["x"], w["y"], w.get("s", 0.85), lw)
    for a in extra.get("amps", []):
        s = a["s"]
        di.polygon([(P(a["x"] - s / 2), P(a["y"] - s / 2)), (P(a["x"] - s / 2), P(a["y"] + s / 2)), (P(a["x"] + s / 2), P(a["y"]))], outline=255, width=lw)
    # Escalas de los faders.
    for s in scales:
        if s["kind"] == "v":
            for i in range(s["n"]):
                yy = s["y1"] - (s["y1"] - s["y0"]) * i / (s["n"] - 1)
                major = i in (0, s["n"] - 1) or (s["n"] == 11 and i == 5)
                wdt = s["width"] + (0.6 if major else 0.0)
                di.line([P(s["x"] - 2.3 - wdt), P(yy), P(s["x"] - 2.3), P(yy)], fill=255, width=max(2, round(P(0.32 if major else 0.22))))
                if s["both"]: di.line([P(s["x"] + 2.3), P(yy), P(s["x"] + 2.3 + wdt), P(yy)], fill=255, width=max(2, round(P(0.32 if major else 0.22))))
            if s["labx"] is not None and s["labels"]:
                for i, lab in enumerate(s["labels"]):
                    yy = s["y0"] + (s["y1"] - s["y0"]) * i / max(1, len(s["labels"]) - 1)
                    txt(lab, s["labx"] - 0.4, yy, 1.45, F_MED, anchor="rm")
        else:
            for i in range(s["n"]):
                xx = s["x0"] + (s["x1"] - s["x0"]) * i / (s["n"] - 1)
                big = i in (0, s["n"] - 1) or i == (s["n"] - 1) // 2
                di.line([P(xx), P(s["y"] - (1.9 if big else 1.2)), P(xx), P(s["y"])], fill=255, width=max(2, round(P(0.25))))
            for i, lab in enumerate(s["labels"]):
                xx = s["x0"] + (s["x1"] - s["x0"]) * i / max(1, len(s["labels"]) - 1)
                txt(lab, xx, s["y"] - 3.4, 1.4, F_MED)
    # Escalas de perillas.
    for k in knobs:
        r = k["d"] / 2
        if k["scale"] == "none": continue
        if k["scale"] == "legend":
            n = k["steps"]
            for i, s in enumerate(k["legends"]):
                a = math.radians(k["a0"] + (k["a1"] - k["a0"]) * i / (n - 1))
                rr = r + (4.6 if len(s) > 2 else 3.6)
                txt(s, k["x"] + math.sin(a) * rr, k["y"] - math.cos(a) * rr, 1.4 if k["size"] != "KQ" else 1.2, F_MED)
                p0 = (k["x"] + math.sin(a) * (r + 0.6), k["y"] - math.cos(a) * (r + 0.6))
                p1 = (k["x"] + math.sin(a) * (r + 1.6), k["y"] - math.cos(a) * (r + 1.6))
                di.ellipse([P(p1[0] - 0.35), P(p1[1] - 0.35), P(p1[0] + 0.35), P(p1[1] + 0.35)], fill=255)
            continue
        n = k["ticks"]
        for i in range(n):
            a = math.radians(k["a0"] + (k["a1"] - k["a0"]) * i / (n - 1))
            l = 1.5 if i in (0, n - 1) or i == (n - 1) // 2 else 1.0
            p0 = (k["x"] + math.sin(a) * (r + 0.7), k["y"] - math.cos(a) * (r + 0.7))
            p1 = (k["x"] + math.sin(a) * (r + 0.7 + l), k["y"] - math.cos(a) * (r + 0.7 + l))
            di.line([P(p0[0]), P(p0[1]), P(p1[0]), P(p1[1])], fill=255, width=max(2, round(P(0.24))))
        for s, f in k["labels"]:
            a = math.radians(k["a0"] + (k["a1"] - k["a0"]) * f)
            rr = r + 2.6 + 0.55 * len(s)
            txt(s, k["x"] + math.sin(a) * rr, k["y"] - math.cos(a) * rr, 1.35 if k["size"] in ("KQ", "KS") else 1.5, F_MED)
    # Leyendas de llaves deslizantes y palancas.
    for s in switches:
        if not s["legends"]: continue
        n = s["positions"]
        for i, lab in enumerate(s["legends"]):
            if s["orient"] == "v":
                yy = s["y"] - s["length"] / 2 + 2.0 + (s["length"] - 4.0) * i / max(1, n - 1)
                txt(lab, s["x"] + 3.0, yy, 1.35, F_MED, anchor="lm")
            else:
                xx = s["x"] - s["length"] / 2 + 2.0 + (s["length"] - 4.0) * i / max(1, n - 1)
                txt(lab, xx, s["y"] - 3.4, 1.3, F_MED)
    for t in toggles:
        legs = t["legends"]
        if not legs: continue
        if t["lpos"] == "vertical":
            txt(legs[0], t["x"], t["y"] - 4.6, 1.35, F_MED); txt(legs[-1], t["x"], t["y"] + 4.6, 1.35, F_MED)
        elif t["lpos"] == "horizontal":
            txt(legs[0], t["x"] - 4.2, t["y"] + 0.1, 1.3, F_MED, anchor="rm"); txt(legs[-1], t["x"] + 4.2, t["y"] + 0.1, 1.3, F_MED, anchor="lm")
        elif t["lpos"] == "left":
            for i, lab in enumerate(legs):
                if lab: txt(lab, t["x"] - 3.6, t["y"] - 2.6 + 2.6 * i, 1.3, F_MED, anchor="rm")
        else:
            txt(legs[0], t["x"] + 3.6, t["y"] - 2.4, 1.3, F_MED, anchor="lm"); txt(legs[-1], t["x"] + 3.6, t["y"] + 2.4, 1.3, F_MED, anchor="lm")
    # VU: escala.
    for m in meters:
        x0, y0 = m["x"] - m["w"] / 2, m["y"] - m["h"] / 2
        di.rectangle([P(x0), P(y0), P(x0 + m["w"]), P(y0 + m["h"])], outline=255, width=lw)
    # Marcos de los módulos: una línea fina en el borde (las juntas reales son geometría).
    os.makedirs(os.path.join(ROOT, "assets", "render"), exist_ok=True)
    ink.save(os.path.join(ROOT, "assets", "render", "silkscreen700.png"), optimize=True)
    prev = Image.new("RGB", (Wp, Hp), (18, 18, 19))
    prev.paste((235, 235, 230), mask=ink)
    for p in panels:
        ImageDraw.Draw(prev).rectangle([P(p["x0"]), P(p["y0"]), P(p["x1"]), P(p["y1"])], outline=(70, 70, 72), width=2)
    prev.resize((Wp // 4, Hp // 4)).save(os.path.join(ROOT, "assets", "render", "silkscreen700_preview.jpg"), quality=88)

def write_vu_face():
    """Esfera del VU (crema, escala −20…+3 dB con zona roja). El pivote de la aguja queda abajo al centro,
    fuera de la ventana: la aguja la dibuja el editor con los mismos ángulos (VU_A0..VU_A1)."""
    w, h = 800, 520
    im = Image.new("RGB", (w, h), (232, 222, 196))
    d = ImageDraw.Draw(im)
    cx, cy, R = w / 2, h * 1.18, h * 0.98
    def ang(db):   # −20..+3 dB → −48°..+48° (escala de VU con 0 VU a 2/3)
        vu = 10 ** (db / 20)
        return -48 + 96 * min(1.0, vu / 1.4125)
    for db in (-20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3):
        a = math.radians(ang(db)); red = db > 0
        r0, r1 = R * 0.80, R * (0.88 if db in (-20, -10, -5, 0, 3) else 0.85)
        d.line([(cx + math.sin(a) * r0, cy - math.cos(a) * r0), (cx + math.sin(a) * r1, cy - math.cos(a) * r1)],
               fill=(200, 30, 20) if red else (20, 20, 20), width=5)
        lab = str(abs(db)) if db != 0 else "0"
        f = ImageFont.truetype(FONT, 38, index=F_BOLD)
        d.text((cx + math.sin(a) * R * 0.94, cy - math.cos(a) * R * 0.94), lab, font=f, fill=(200, 30, 20) if red else (20, 20, 20), anchor="mm")
    for i in range(80):
        a0 = math.radians(-48 + 96 * i / 80); a1 = math.radians(-48 + 96 * (i + 1) / 80)
        red = -48 + 96 * i / 80 > ang(0)
        d.line([(cx + math.sin(a0) * R * 0.80, cy - math.cos(a0) * R * 0.80), (cx + math.sin(a1) * R * 0.80, cy - math.cos(a1) * R * 0.80)],
               fill=(200, 30, 20) if red else (20, 20, 20), width=7 if red else 4)
    d.text((cx, h * 0.62), "VU", font=ImageFont.truetype(FONT, 64, index=F_BOLD), fill=(20, 20, 20), anchor="mm")
    d.text((cx - R * 0.52, h * 0.22), "−", font=ImageFont.truetype(FONT, 50, index=F_BOLD), fill=(20, 20, 20), anchor="mm")
    d.text((cx + R * 0.52, h * 0.22), "+", font=ImageFont.truetype(FONT, 50, index=F_BOLD), fill=(200, 30, 20), anchor="mm")
    im.save(os.path.join(ROOT, "assets", "render", "vu_face.png"))

# ───────── parámetros del panel ─────────
DEFAULTS = {
    "vco1_range": 3, "vco2_range": 3, "vco3_range": 3, "vco1_pitch": 0.5, "vco2_pitch": 0.5, "vco3_pitch": 0.5,
    "vco1_fine": 0.5, "vco2_fine": 0.5, "vco3_fine": 0.5, "vco1_pw": 0.5, "vco2_pw": 0.5, "vco3_pw": 0.5,
    "vco1_wave": 2, "vco2_wave": 2, "vco3_wave": 3, "vco1_keycv": 0, "vco2_keycv": 0, "vco3_keycv": 0,
    "vco1_att1": 1, "vco1_att2": 1, "vco2_att1": 1, "vco2_att2": 1, "vco2_att3": 1, "vco3_att1": 1, "vco3_att2": 1, "vco3_att3": 1,
    "vcf1_lvl1": 0.8, "vcf1_cutoff": 0.62, "vcf1_mod1": 1.0, "vcf1_mod2": 0.35, "vcf2_cutoff": 0.7, "vcf2_mode": 2,
    "vca1_lvl1": 0.8, "vca1_mod1": 1.0, "vca2_lvl2": 0.0, "vca1_exp": 0,
    "adsr1_a": 0.18, "adsr1_d": 0.55, "adsr1_s": 0.6, "adsr1_r": 0.5, "adsr1_mode": 0, "adsr1_time": 0,
    "adsr2_a": 0.2, "adsr2_d": 0.5, "adsr2_s": 0.5, "adsr2_r": 0.5, "adsr2_mode": 0, "adsr2_time": 0,
    "lfo1_freq": 0.55, "lfo2_freq": 0.45, "lfo1_wave": 2, "lfo2_wave": 2, "lfo2_pw": 0.5, "lfo1_keytrig": 1, "lfo2_keytrig": 1,
    "sh_rate": 0.45, "sh_mode": 1, "sh_clock": 1,
    "pan_lvl1": 0.75, "pan_lvl2": 0.75, "pan_lvl3": 0.5, "pan_pos1": 0.5, "pan_pos2": 0.5, "pan_pos3": 0.5, "rev_lvl1": 0.35,
    "std_level": 0.0, "std_freq": 2, "std_dest": 1, "ps_speed": 0.35, "ps_int": 0.6, "ps_res": 0.3, "ps_auto": 0,
    "monitor": 0.7, "vp_level": 0.5, "power": 0, "extctl": 1, "amp_range": 2, "amp_gain": 0.5, "int_lag": 0.4,
    "kb_pitch": 0.5, "kb_porta": 0.3, "kb_portaon": 0, "kb_bendrange": 1, "kb_bender": 0.5,
    "pshaper_time": 0.3, "pshaper_thr": 0.5, "clk_pw": 0.5, "clk_rate": 0.6, "clk_range": 0,
    "par_range_a": 0, "par_range_b": 0, "par_range_c": 0, "ser_mode": 0, "ser_repeat": 1, "ser_steps": 11,
    "rm_x": 0, "rm_y": 1,
}
NICE = {"pw": "Pulse Width", "range": "Range", "pitch": "Pitch", "fine": "Fine", "wave": "Wave Form", "keycv": "Key CV",
        "pwm": "PWM", "syncmode": "Sync Mode", "cutoff": "Cutoff", "res": "Resonance", "mode": "Mode", "adsr": "ADSR Select",
        "gain": "Gain", "exp": "Lin/Exp", "a": "Attack", "d": "Decay", "s": "Sustain", "r": "Release", "time": "Time Range",
        "delay": "Delay Time", "freq": "Frequency", "keytrig": "Key Trig", "rate": "Rate"}
WAVE_CH = "Sine|Triangle|Sawtooth|Pulse"

def pname(pid):
    m = re.match(r"^vco(\d)_(mod|att)(\d)$", pid)
    if m:
        n, kind, i = int(m.group(1)), m.group(2), int(m.group(3))
        src = (["LFO1", "Ext"] if n == 1 else ["LFO1", "ADSR1" if n == 2 else "S&H", "Ext"])[i - 1]
        return f"VCO{n} Mod {src}" + (" Attenuator" if kind == "att" else "")
    m = re.match(r"^vcf(\d)_lvl(\d)$", pid)
    if m:
        n, i = int(m.group(1)), int(m.group(2))
        return f"VCF{n} In " + ["VCO1", "VCO2", "VCO3", "Noise" if n == 1 else "Ring Mod", "Ext"][i - 1]
    m = re.match(r"^vcf(\d)_mod(\d)$", pid)
    if m: return f"VCF{m.group(1)} Mod " + ["Key CV", "ADSR", "LFO1", "Ext"][int(m.group(2)) - 1]
    m = re.match(r"^vca(\d)_lvl(\d)$", pid)
    if m: return f"VCA{m.group(1)} In " + ["VCF1", "VCF2", "Ext"][int(m.group(2)) - 1]
    m = re.match(r"^vca(\d)_mod(\d)$", pid)
    if m: return f"VCA{m.group(1)} Mod " + ["ADSR", "Ext"][int(m.group(2)) - 1]
    m = re.match(r"^(rev|pan|ps)_lvl(\d)$", pid)
    if m: return {"rev": "Reverb In ", "pan": "Pan Level ", "ps": "Phaser In "}[m.group(1)] + ["VCA1", "VCA2", "Ext" if m.group(1) == "rev" else "Rev"][int(m.group(2)) - 1]
    m = re.match(r"^pan_pos(\d)$", pid)
    if m: return f"Pan {m.group(1)}"
    m = re.match(r"^mix_lvl(\d)$", pid)
    if m: return f"Mixer In {m.group(1)}"
    m = re.match(r"^step(\d+)_(\w+)$", pid)
    if m: return f"Step {m.group(1)} " + {"a": "CH-A", "b": "CH-B", "c": "CH-C", "div": "Divide", "fine": "Fine", "hold": "Hold"}[m.group(2)]
    m = re.match(r"^(vco|vcf|vca|lfo|adsr)(\d)_(\w+)$", pid)
    if m: return f"{m.group(1).upper()}{m.group(2)} {NICE.get(m.group(3), m.group(3).title())}"
    fixed = {"rm_x": "Ring Mod X", "rm_y": "Ring Mod Y", "std_level": "Standard Osc Level", "std_freq": "Standard Osc Freq",
             "std_dest": "Standard Osc Out", "ps_res": "Phaser Resonance", "ps_speed": "Phaser Speed", "ps_int": "Phaser Intensity",
             "ps_auto": "Phaser Auto/Ext", "monitor": "Monitor Level", "extctl": "Ext Controller", "vp_level": "Voltage Processor",
             "sh_mode": "S&H Sample Mode", "sh_clock": "S&H Clock", "sh_rate": "S&H Clock Rate", "amp_range": "Amp Gain Range",
             "amp_gain": "Amp Gain", "int_lag": "Integrator Lag", "power": "Power", "kb_pitch": "Keyboard Pitch",
             "kb_porta": "Portamento Time", "kb_portaon": "Portamento", "kb_bendrange": "Bend Range", "kb_bender": "Bender",
             "pshaper_time": "Pulse Shaper Time", "pshaper_thr": "Pulse Shaper Threshold", "clk_pw": "Clock Pulse Width",
             "clk_rate": "Clock Rate", "clk_range": "Clock Range", "ser_mode": "Series Mode", "ser_repeat": "Series Stop/Repeat",
             "ser_steps": "Step Selector", "vcf2_mode": "VCF2 Filter Mode", "adsr_manual": "Manual Gate"}
    if pid in fixed: return fixed[pid]
    m = re.match(r"^par_range_(\w)$", pid)
    if m: return f"Parallel Range CH-{m.group(1).upper()}"
    return pid

def choices(pid, positions, legends=()):
    special = {"vco1_wave": WAVE_CH, "vco2_wave": WAVE_CH, "vco3_wave": WAVE_CH, "lfo1_wave": "Square|Saw|Triangle|Sine",
               "lfo2_wave": "Pulse|Saw|Triangle|Sine", "vcf2_mode": "HP|BP|LP", "rm_x": "VCO1|VCO2|VCO3|Ext", "rm_y": "VCO1|VCO2|Noise|Ext",
               "sh_mode": "LFO1|Noise|Ext", "sh_clock": "Ext|Int", "amp_range": "x1000|x100|x10", "std_dest": "L|LR|R",
               "ps_auto": "Auto|Ext", "extctl": "Console|Off", "ser_mode": "A|A-B|A-B-C|Manual", "ser_repeat": "Stop|Repeat",
               "kb_bendrange": "5V|1V|0.1V|Off", "power": "On|Off", "clk_range": "x10|x1", "kb_portaon": "Off|On"}
    if pid in special: return special[pid]
    if re.match(r"^step\d+_hold$", pid): return "Normal|Hold"
    if re.match(r"^par_range_\w$", pid): return "10V|4V|2V"
    if re.match(r"^adsr\d_mode$", pid): return "Gate+Trig|Gate|Ext Gate"
    if re.match(r"^adsr\d_time$", pid): return "x1|x1/10"
    if re.match(r"^vco\d_att\d$", pid): return "1/10|1"
    if re.match(r"^vco\d_keycv$", pid): return "On|Off"
    if re.match(r"^vco\d_syncmode$", pid): return "Strong|Weak"
    if re.match(r"^v(cf|ca)\d_adsr$", pid): return "ADSR1|ADSR2"
    if re.match(r"^vca\d_exp$", pid): return "Lin|Exp"
    if re.match(r"^lfo\d_keytrig$", pid): return "On|Off"
    if legends and any(legends): return "|".join(legends)
    return "|".join(str(i) for i in range(positions))

def pdefault(pid):
    if pid in DEFAULTS: return DEFAULTS[pid]
    m = re.match(r"^step(\d+)_(\w+)$", pid)
    if m:
        return {"a": 0.5, "b": 0.5, "c": 0.5, "div": 3, "fine": 0.0, "hold": 0}[m.group(2)]
    if re.match(r"^vco\d_att\d$", pid): return 1
    if re.match(r"^lfo\d_keytrig$", pid): return 1
    return 0.0

def collect_params():
    ps = []
    for k in knobs: ps.append(dict(id=k["id"], steps=k["steps"], choices=choices(k["id"], k["steps"], k["legends"]) if k["steps"] else ""))
    for s in sliders: ps.append(dict(id=s["id"], steps=0, choices=""))
    for s in switches: ps.append(dict(id=s["id"], steps=s["positions"], choices=choices(s["id"], s["positions"], s["legends"])))
    for t in toggles: ps.append(dict(id=t["id"], steps=t["positions"], choices=choices(t["id"], t["positions"], t["legends"])))
    for l in levers: ps.append(dict(id=l["id"], steps=l["positions"], choices=choices(l["id"], l["positions"])))
    for p_ in ps:
        p_["name"] = pname(p_["id"]); p_["def"] = pdefault(p_["id"])
    return ps

def write_params_header(ps):
    L = ["#pragma once", "// Generado por tools/panel700.py — no editar a mano.", "", "namespace dh {", "",
         "enum class P700 : int", "{"]
    L += [f"    {p_['id']}," for p_ in ps]
    L += ["    count", "};", "constexpr int kNumP700 = (int) P700::count;", "",
          "struct PanelParam700 { const char* id; const char* name; int steps; float def; const char* choices; };", "",
          "inline const PanelParam700& panelParam700 (int i)", "{", "    static const PanelParam700 t[kNumP700] = {"]
    for p_ in ps:
        L.append(f"        {{ \"{p_['id']}\", \"{p_['name']}\", {p_['steps']}, {float(p_['def'])}f, \"{p_['choices']}\" }},")
    L += ["    };", "    return t[i];", "}", "", "} // namespace dh", ""]
    open(os.path.join(ROOT, "Source", "dsp", "Params700.generated.h"), "w").write("\n".join(L))

if __name__ == "__main__":
    ids = [j["id"] for j in jacks]
    assert len(ids) == len(set(ids)), "jacks repetidos"
    write_layout()
    write_jacks_header()
    ps = collect_params()
    pid = [p["id"] for p in ps]
    dup = {x for x in pid if pid.count(x) > 1}
    assert not dup, f"parámetros repetidos: {dup}"
    write_params_header(ps)
    write_silkscreen()
    write_vu_face()
    print(len(ps), "parámetros;", len(knobs), "perillas,", len(sliders), "faders,", len(switches), "llaves,", len(toggles),
          "palancas,", len(levers), "selectores,", len(buttons), "botones,", len(jacks), "jacks,", len(leds), "leds")
