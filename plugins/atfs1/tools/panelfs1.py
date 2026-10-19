#!/usr/bin/env python3
"""Frente del ATFS1 (basado en el Yamaha FS1R, 1998): rack de 1U en milímetros.

Posiciones medidas sobre fotos del frente (Wikimedia Commons, CC0; ver docs/ref/CREDITS.md) a escala
del ancho de rack de 482,6 mm. La gráfica es propia: sin el logo ni la marca del fabricante; el modelo
lleva el logotipo ATFS1.

Genera:
  assets/layoutfs1.json              controles, LCD, tornillos (Blender y editor)
  assets/render/silkscreenfs1.png    serigrafía (10 px/mm, con transparencia)

Coordenadas: origen arriba a la izquierda, x a la derecha, y hacia abajo, mm.
"""
import json, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 482.6, 44.0
PX = 10
FONT = "/System/Library/Fonts/HelveticaNeue.ttc"
COND = "/System/Library/Fonts/Avenir Next Condensed.ttc"
INK = (35, 38, 44, 255)
STRIP = (218, 216, 208, 255)
BLUE = (47, 80, 184, 255)
WHITE = (250, 250, 248, 255)

def f(size_mm, idx=0, path=FONT): return ImageFont.truetype(path, int(round(size_mm * PX)), index=idx)
def p(x, y): return (x * PX, y * PX)

knobs = [
    dict(id="volume", x=61.8, y=20.4, d=9.0, kind="small", param="volume"),
    dict(id="k1", x=307.0, y=21.5, d=15.0, kind="big", param="attack"),
    dict(id="k2", x=335.9, y=21.5, d=15.0, kind="big", param="release"),
    dict(id="k3", x=364.9, y=21.5, d=15.0, kind="big", param="formant"),
    dict(id="k4", x=393.8, y=21.5, d=15.0, kind="big", param="fm"),
]
GX, EX, BX, MX, PX2 = 208.0, 217.7, 232.9, 242.8, 252.4
ROWS = (13.6, 23.4, 33.5)
buttons = [
    dict(id="play", x=GX, y=ROWS[0], d=4.8, kind="grey"), dict(id="util", x=GX, y=ROWS[1], d=4.8, kind="grey"),
    dict(id="search", x=GX, y=ROWS[2], d=4.8, kind="grey"),
    dict(id="perform", x=EX, y=ROWS[0], d=4.8, kind="grey"), dict(id="effect", x=EX, y=ROWS[1], d=4.8, kind="grey"),
    dict(id="voice", x=EX, y=ROWS[2], d=4.8, kind="grey"),
    dict(id="mute", x=BX, y=ROWS[0], d=6.4, kind="black"), dict(id="enter", x=BX, y=ROWS[1], d=6.4, kind="black"),
    dict(id="exit", x=BX, y=ROWS[2], d=6.4, kind="black"),
    dict(id="part_dn", x=MX, y=ROWS[0], d=6.4, kind="black"), dict(id="part_up", x=PX2, y=ROWS[0], d=6.4, kind="black"),
    dict(id="cur_dn", x=MX, y=ROWS[1], d=6.4, kind="black"), dict(id="cur_up", x=PX2, y=ROWS[1], d=6.4, kind="black"),
    dict(id="val_dn", x=MX, y=ROWS[2], d=6.4, kind="black"), dict(id="val_up", x=PX2, y=ROWS[2], d=6.4, kind="black"),
    dict(id="knob_mode", x=287.2, y=21.3, d=4.0, kind="hidden"),
]
leds = [dict(id="led_top", x=287.2, y=11.7, d=3.4), dict(id="led_bot", x=287.2, y=30.9, d=3.4)]
lcd = dict(x0=87.4, y0=8.2, x1=178.6, y1=35.1, glass=dict(x0=90.0, y0=10.4, x1=176.0, y1=33.0))
jack = dict(x=43.6, y=20.6, d=9.4)
power = dict(x0=418.2, y0=7.0, x1=438.5, y1=29.3)
screws = [(8.9, 4.7), (8.9, 38.0), (473.7, 4.7), (473.7, 38.0), (31.4, 4.9), (31.4, 38.4), (453.2, 4.9), (453.2, 38.4),
          (272.7, 4.9), (272.7, 38.4)]
LCD_LABELS = [("PART", 94.6), ("MIDI", 106.4), ("BANK/PGM#", 120.2), ("VOL", 132.2), ("FLT", 138.5), ("PAN", 144.8),
              ("REV", 151.3), ("VAR", 158.0), ("INS", 164.1), ("KEY", 171.3)]

im = Image.new("RGBA", (int(W * PX), int(H * PX)), (0, 0, 0, 0))
d = ImageDraw.Draw(im)

def text(s, x, y, size, color=INK, font=None, anchor="mm"):
    d.text(p(x, y), s, fill=color, font=font or f(size), anchor=anchor)

def pill(x0, y0, x1, y1, color=STRIP):
    d.rounded_rectangle([*p(x0, y0), *p(x1, y1)], radius=(y1 - y0) / 2 * PX, fill=color)

def box(s, x, y, w, h=2.6, size=1.9):
    d.rectangle([*p(x - w / 2, y - h / 2), *p(x + w / 2, y + h / 2)], fill=BLUE)
    text(s, x, y + 0.05, size, WHITE, f(size, 1))

# Marca propia y modelo.
text("AUDIO TOOLS", 19.6, 22.0, 2.3, INK, f(2.3, 1), "mm")
logo = f(5.6, 7, COND)                          # Avenir Next Condensed Heavy, en contorno
d.text(p(88.0, 4.6), "ATFS1", font=logo, fill=(0, 0, 0, 0), stroke_width=int(0.28 * PX), stroke_fill=INK, anchor="lm")
d.text(p(88.0, 4.6), "ATFS1", font=logo, fill=(0, 0, 0, 0), anchor="lm")
for i, s in enumerate(("TONE GENERATOR", "FORMANT SHAPING / FM SYNTHESIS")):
    y = 3.1 + i * 2.6
    d.rectangle([*p(124.0, y - 0.7), *p(125.4, y + 0.7)], fill=INK)
    text(s, 126.4, y, 2.0, INK, f(2.0, 10), "lm")

text("PHONES", jack["x"], 30.6, 1.9)
text("VOLUME", 61.8, 30.6, 1.9)
d.ellipse([*p(61.8 - 6.2, 20.4 - 6.2), *p(61.8 + 6.2, 20.4 + 6.2)], fill=(206, 204, 197, 255))
for s, x in LCD_LABELS: text(s, x, 38.2, 1.7)

# Botonera.
text("PLAY", GX, 9.2, 1.8); text("UTIL", GX, 19.1, 1.8); text("SEARCH", GX - 1.0, 29.2, 1.8)
text("EDIT", EX, 6.4, 1.8)
box("PERFORM", EX, 9.4, 9.6); box("EFFECT", EX + 0.6, 19.3, 8.6); box("VOICE", EX + 0.6, 29.4, 7.6)
text("MUTE/", BX, 5.0, 1.8); text("SOLO", BX, 7.6, 1.8); text("ENTER", BX, 18.9, 1.8); text("EXIT", BX, 29.0, 1.8)
for y, s in ((5.4, "PART"), (18.9, "CURSOR"), (29.0, "VALUE")):
    text(s, (MX + PX2) / 2, y, 1.9)
    for cx, sign in ((MX - 1.4, "-"), (PX2 + 1.4, "+")):
        d.ellipse([*p(cx - 1.0, y - 1.0), *p(cx + 1.0, y + 1.0)], outline=INK, width=2)
        d.line([*p(cx - 0.55, y), *p(cx + 0.55, y)], fill=INK, width=2)
        if sign == "+": d.line([*p(cx, y - 0.55), *p(cx, y + 0.55)], fill=INK, width=2)
d.line([*p(MX - 1.0, 8.3), *p(PX2 + 1.0, 8.3)], fill=INK, width=2)
d.line([*p(MX - 1.0, 8.3), *p(MX - 1.0, 9.1)], fill=INK, width=2); d.line([*p(PX2 + 1.0, 8.3), *p(PX2 + 1.0, 9.1)], fill=INK, width=2)
d.rectangle([*p((MX + PX2) / 2 - 2.0, 7.5), *p((MX + PX2) / 2 + 2.0, 9.1)], fill=(0, 0, 0, 0))
text("ALL", (MX + PX2) / 2, 8.3, 1.6)

# Sección de perillas: bandas claras, círculos y rótulos.
pill(279.4, 9.1, 407.8, 12.7); pill(279.4, 28.6, 407.8, 32.4)
names = ("ATTACK", "RELEASE", "FORMANT", "FM")
blue = ("PART/OP", "GROUP", "CURSOR", "VALUE")
for i, k in enumerate(knobs[1:]):
    x = k["x"]
    d.ellipse([*p(x - 10.4, 21.5 - 10.4), *p(x + 10.4, 21.5 + 10.4)], fill=STRIP)
    pill(x - 8.2, 8.4, x + 8.2, 13.2, STRIP)
    text(names[i], x, 10.8, 2.3, INK, f(2.3, 10))
    cx = x - 8.6
    d.ellipse([*p(cx - 1.5, 30.5 - 1.5), *p(cx + 1.5, 30.5 + 1.5)], fill=STRIP, outline=INK, width=2)
    text(str(i + 1), cx, 30.55, 1.9, INK, f(1.9, 10))
    box(blue[i], x, 37.4, 13.0, 2.7, 1.9)
text("POWER", (power["x0"] + power["x1"]) / 2, 4.2, 2.0)
d.rectangle([*p(419.8, 39.6), *p(421.2, 40.2)], fill=INK)
text("ON / ", 422.0, 39.9, 1.7, anchor="lm")
d.rectangle([*p(426.6, 39.3), *p(427.8, 40.5)], fill=INK)
text("OFF", 428.4, 39.9, 1.7, anchor="lm")

os.makedirs(os.path.join(ROOT, "assets", "render"), exist_ok=True)
im.save(os.path.join(ROOT, "assets", "render", "silkscreenfs1.png"))
layout = dict(size=[W, H], knobs=knobs, buttons=buttons, leds=leds, lcd=lcd, jack=jack, power=power,
              screws=[dict(x=x, y=y) for x, y in screws], lcdLabels=[dict(text=s, x=x) for s, x in LCD_LABELS])
json.dump(layout, open(os.path.join(ROOT, "assets", "layoutfs1.json"), "w"), indent=1)
print("ok", im.size)
