#!/usr/bin/env python3
"""Serigrafía del ATMD: tinta blanca sobre transparente, dibujada desde assets/layoutmd.json.

Todo sale del layout (mm, origen en la esquina superior izquierda de la cara impresa, y hacia abajo):
títulos de sección al pie, subtítulos, rótulos, diales con rayas y números alrededor de cada perilla,
leyendas de RANGE/WAVEFORM (con el diente de sierra invertido del osc 3), leyendas de las teclas
basculantes, líneas de unión del mezclador y divisores. Sin logo ni placa del fabricante: la placa del
listón de madera lleva "Audio Tools" / "ATMD", dibujada acá.

Salidas (12 px/mm):
  assets/silkscreenmd.png          cara del panel, 672,2 x 178,8 mm
  assets/silkscreenmd_preview.jpg  vista previa sobre el color del panel (1/3)
  assets/lhcmd.png                 placa del controlador de la izquierda (GLIDE, DECAY, PITCH, MOD.)
  assets/nameplatemd.png           placa del listón (RGBA, fondo negro anodizado incluido)

Tipografía: Futura (Medium para los títulos, Bold para rótulos y números), como pide layout.look.
"""
import json, math, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
L = json.load(open(os.path.join(ROOT, "assets", "layoutmd.json")))
PX = 12.0
FONT = "/System/Library/Fonts/Supplemental/Futura.ttc"
F_MED, F_BOLD = 0, 2
INK = (255, 255, 255, 255)

img = draw = None
_fonts = {}


def font(cap_mm, idx):
    """Fuente cuyo alto de mayúscula es cap_mm."""
    probe = ImageFont.truetype(FONT, 200, index=idx)
    b = probe.getbbox("H")
    size = max(4, int(round(cap_mm * PX * 200.0 / (b[3] - b[1]))))
    k = (size, idx)
    if k not in _fonts: _fonts[k] = ImageFont.truetype(FONT, size, index=idx)
    return _fonts[k]


def text(s, x, y, cap, idx=F_BOLD, align="center", cond=0.92, track=0.04, lead=1.55):
    """Texto en mm centrado verticalmente en la altura de mayúscula (y = centro de la línea de mayúsculas)."""
    if not s: return
    f = font(cap, idx)
    hb = f.getbbox("H"); cap_px = hb[3] - hb[1]
    lines = s.split("\n")
    for li, ln in enumerate(lines):
        yc = (y + (li - (len(lines) - 1) / 2.0) * cap * lead) * PX
        widths = [f.getlength(c) for c in ln]
        tr = track * cap * PX
        tw = sum(widths) + tr * max(0, len(ln) - 1)
        pad = int(cap * PX)
        lay = Image.new("RGBA", (int(tw) + 2 * pad, int(cap_px * 3) + 2 * pad), (0, 0, 0, 0))
        ld = ImageDraw.Draw(lay)
        xx = pad; top = pad + cap_px - hb[1]
        for c, w in zip(ln, widths):
            ld.text((xx, top), c, font=f, fill=INK); xx += w + tr
        nw = max(1, int(round(lay.width * cond)))
        lay = lay.resize((nw, lay.height), Image.LANCZOS)
        real = tw * cond; padc = pad * cond
        if align == "center": x0 = x * PX - real / 2 - padc
        elif align == "left": x0 = x * PX - padc
        else: x0 = x * PX - real - padc
        y0 = yc - (pad + cap_px * 1.5)            # centro de la mayúscula en top + cap/2
        img.alpha_composite(lay, (int(round(x0)), int(round(y0))))


def line(pts, w=0.35):
    draw.line([(x * PX, y * PX) for x, y in pts], fill=INK, width=max(1, int(round(w * PX))), joint="curve")
    r = w * PX / 2.0                               # puntas redondeadas
    for x, y in (pts[0], pts[-1]):
        draw.ellipse([x * PX - r, y * PX - r, x * PX + r, y * PX + r], fill=INK)


def polyline(pts, w=0.32):
    draw.line([(x * PX, y * PX) for x, y in pts], fill=INK, width=max(1, int(round(w * PX))), joint="curve")


def polar(x, y, ang, r):
    a = math.radians(ang)
    return x + math.sin(a) * r, y - math.cos(a) * r


# ───── símbolos de forma de onda (alto s mm, centrados) ─────
def sym(name, x, y, s=2.4, w=0.3):
    k = s
    def P(pts): return [(x + px * k, y + py * k) for px, py in pts]
    if name == "tri":
        polyline(P([(-0.62, 0.5), (0.0, -0.5), (0.62, 0.5)]), w)
    elif name == "trisaw":          # "diente de tiburón": subida recta, bajada cóncava
        pts = [(-0.62, 0.5), (-0.05, -0.5)]
        for i in range(1, 9):
            t = i / 8.0
            pts.append((-0.05 + 0.67 * t, -0.5 + 1.0 * (1 - (1 - t) ** 2.2)))
        polyline(P(pts), w)
    elif name == "saw":
        polyline(P([(-0.62, 0.5), (0.45, -0.5), (0.45, 0.5)]), w)
    elif name == "revsaw":          # osc 3: subida vertical, bajada en rampa
        polyline(P([(-0.45, 0.5), (-0.45, -0.5), (0.62, 0.5)]), w)
    elif name == "square":
        polyline(P([(-0.75, 0.5), (-0.38, 0.5), (-0.38, -0.5), (0.38, -0.5), (0.38, 0.5), (0.75, 0.5)]), w)
    elif name == "pulse_wide":
        polyline(P([(-0.75, 0.5), (-0.3, 0.5), (-0.3, -0.5), (0.18, -0.5), (0.18, 0.5), (0.75, 0.5)]), w)
    elif name == "pulse_narrow":
        polyline(P([(-0.75, 0.5), (-0.12, 0.5), (-0.12, -0.5), (0.12, -0.5), (0.12, 0.5), (0.75, 0.5)]), w)


def fix(s):
    """'OSCILLATOR-1' → 'OSCILLATOR –1' como en la serigrafía original."""
    for d in "123":
        s = s.replace("OSCILLATOR-" + d, "OSCILLATOR –" + d)
    return s


# ───── diales ─────
TICK_IN, TICK_OUT, TICK_MAJOR = 1.7, 4.2, 4.9     # mm por fuera del radio del dial
NUM_R = 7.4                                       # centro de los números, por fuera del radio
NUM_CAP = 2.3


def dial(k):
    x, y, r = k["x"], k["y"], k["r"]
    legend_angles = {round(a, 1) for s, a in k["legends"] if not s[0].isalpha() or s in ("LO",)}
    if k["kind"] == "rotary-switch":
        for s, a in k["legends"]:
            line([polar(x, y, a, r + 1.2), polar(x, y, a, r + 3.2)], 0.32)
            px_, py_ = polar(x, y, a, r + 6.8)
            if s.startswith("sym:"): sym(s[4:], px_, py_, 2.4)
            else: text(s, px_, py_, NUM_CAP, F_BOLD, cond=0.95)
    else:
        for a in k["ticks"]:
            major = round(a, 1) in legend_angles
            line([polar(x, y, a, r + TICK_IN), polar(x, y, a, r + (TICK_MAJOR if major else TICK_OUT))], 0.36 if major else 0.32)
        nums = [(s, a) for s, a in k["legends"] if not s[0].isalpha() or s[0] in "-"]
        words = [(s, a) for s, a in k["legends"] if s[0].isalpha()]
        for s, a in nums:
            rr = r + NUM_R + (0.6 if len(s) >= 3 else 0.0)
            px_, py_ = polar(x, y, a, rr)
            text(s, px_, py_, NUM_CAP, F_BOLD, cond=0.95, track=0.02)
        for s, a in words:
            taken = any(abs(a - b) < 1 for _, b in nums)
            px_, py_ = polar(x, y, a, r + (NUM_R if taken else 5.4))
            if taken: py_ += 4.0                  # OSC. 3 / NOISE bajo el 0 y el 10
            else: py_ += 0.8                      # M-SEC. / SEC. en las puntas del dial
            text(s, px_, py_, 2.1, F_BOLD, cond=0.9)
    lab = k.get("label", {})
    if lab.get("text"):
        big = k["kind"] == "big"
        text(fix(lab["text"]), lab["x"], lab["y"], 4.0 if big else 2.9, F_BOLD, cond=0.9 if big else 0.92)


def silkscreen():
    global img, draw
    W, H = L["panel"]["size"]
    img = Image.new("RGBA", (int(round(W * PX)), int(round(H * PX))), (0, 0, 0, 0)); draw = ImageDraw.Draw(img)
    dv = L["dividers"]; sw = dv["stroke"]
    for v in dv["vertical"]: line([(v["x"], v["y0"]), (v["x"], v["y1"])], sw)
    for h in dv["horizontal"]: line([(h["x0"], h["y"]), (h["x1"], h["y"])], sw)
    for t in L["texts"]:
        s = fix(t["text"])
        if t["kind"] == "section": text(s, t["x"], t["y"], t["cap_h"] * 1.05, F_MED, cond=1.0, track=0.08)
        elif t["kind"] == "subheader": text(s, t["x"], t["y"], t["cap_h"], F_BOLD, cond=0.9)
        else: text(s, t["x"], t["y"], t["cap_h"] * 0.92, F_BOLD, cond=0.92)
    for k in L["knobs"]: dial(k)
    # Teclas basculantes: ON junto a la mitad derecha (horizontales) o arriba (verticales).
    for s in L["switches"]:
        x, y, w, h = s["x"], s["y"], s["w"], s["h"]
        lab = s.get("label")
        if s["id"] == "noise_color": pass                       # WHITE / PINK vienen en texts
        elif s["orient"] == "h":
            off, on = s["legends"]
            above = s["id"] == "kbd_ctrl1"                      # su rótulo va debajo: el ON sube
            oy = y - h / 2 - 2.6 if above else y + h / 2 + 2.6
            text(on, x + w / 2 - 2.8, oy + (-0.3 if above else 0.4), 2.2, F_BOLD)
            if off: text(off, x - w / 2 - 3.0, y, 2.6, F_BOLD)
        else:
            if s["id"] == "power": text("ON", x, y - h / 2 - 3.6, 2.2, F_BOLD)
            # OSC. 3 CONTROL: la foto de referencia no muestra ON junto a la tecla (el rótulo va arriba).
        if lab and lab.get("text"): text(lab["text"], lab["x"], lab["y"], 2.6, F_BOLD, cond=0.9)
    # Líneas de unión del mezclador: perilla → tecla ON de su fila.
    sw_by = {s["id"]: s for s in L["switches"]}
    kn = {k["id"]: k for k in L["knobs"]}
    for kid, sid in (("mix_osc1", "mix_osc1_on"), ("mix_osc2", "mix_osc2_on"), ("mix_osc3", "mix_osc3_on")):
        k, s = kn[kid], sw_by[sid]
        line([(k["x"] + k["r"] + NUM_R + 2.4, s["y"]), (s["x"] - s["w"] / 2 - 1.2, s["y"])], 0.4)
    for kid, sid in (("ext_level", "mix_ext_on"), ("mix_noise", "mix_noise_on")):
        k, s = kn[kid], sw_by[sid]
        line([(s["x"] + s["w"] / 2 + 1.2, s["y"]), (k["x"] - k["r"] - NUM_R - 2.4, s["y"])], 0.4)
    out = os.path.join(ROOT, "assets", "silkscreenmd.png")
    img.save(out, optimize=True)
    look = L["look"]["panel_color"].lstrip("#")
    bgc = tuple(int(look[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
    prev = Image.new("RGBA", img.size, bgc); prev.alpha_composite(img)
    prev.convert("RGB").resize((img.width // 3, img.height // 3), Image.LANCZOS).save(
        os.path.join(ROOT, "assets", "silkscreenmd_preview.jpg"), quality=88)
    return out


def lhc_plate():
    """Placa del controlador de la izquierda: rótulos en el marco del deck (origen = esquina de la placa)."""
    global img, draw
    lhc = L["deck"]["lhc"]; x0, y0 = lhc["x0"], lhc["y0"]
    W, H = lhc["x1"] - x0, lhc["y1"] - y0
    img = Image.new("RGBA", (int(round(W * PX)), int(round(H * PX))), (0, 0, 0, 0)); draw = ImageDraw.Draw(img)
    for s in lhc["switches"]:
        x, y = s["x"] - x0, s["y"] - y0
        text(s["label"], x - s["w"] / 2 - 2.0, y, 2.5, F_BOLD, align="right", cond=0.9)
        text("ON", x + s["w"] / 2 + 1.6, y - 2.6, 2.1, F_BOLD, align="left")
    for wl in lhc["wheels"]:
        x, y = wl["x"] - x0, wl["y"] - y0
        text(wl["label"], x, y + wl["slot_len"] / 2 + 5.6, 2.6, F_BOLD, cond=0.9)
    img.save(os.path.join(ROOT, "assets", "lhcmd.png"), optimize=True)


def nameplate():
    """Placa propia del listón: aluminio negro anodizado con "ATMD" y "Audio Tools" (sin marca del fabricante)."""
    global img, draw
    nb = L["case"]["name_board"]["nameplate"]
    W, H = nb["x1"] - nb["x0"], nb["y1"] - nb["y0"]
    base = Image.new("RGBA", (int(round(W * PX)), int(round(H * PX))), (14, 14, 16, 255))
    img = Image.new("RGBA", base.size, (0, 0, 0, 0)); draw = ImageDraw.Draw(img)
    # Filete fino a 2 mm del borde, como las placas grabadas de época.
    m = 2.2; w = 0.35
    line([(m, m), (W - m, m), (W - m, H - m), (m, H - m), (m, m)], w)
    text("ATMD", W / 2, H * 0.42, 9.0, F_BOLD, cond=1.0, track=0.10)
    text("Audio Tools", W / 2, H * 0.76, 3.0, F_MED, cond=1.0, track=0.08)
    ink = Image.new("RGBA", base.size, (205, 207, 210, 255))
    base.paste(ink, (0, 0), img)
    base.save(os.path.join(ROOT, "assets", "nameplatemd.png"), optimize=True)


if __name__ == "__main__":
    out = silkscreen()
    lhc_plate()
    nameplate()
    print("ok", out, Image.open(out).size)
