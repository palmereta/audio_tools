#!/usr/bin/env python3
"""Genera los presets de fábrica del AT700 en assets/presets/NN_nombre.json.

Valores del panel 0..1 (selectores como índice). Las funciones convierten unidades reales (Hz,
segundos, semitonos) a la posición del control con las mismas curvas del motor (Engine700.h).
Las conexiones internas de la consola ya están hechas; los cables son los que el original pedía
para cada sonido (sync, duofónico, S&H, secuenciador...).
"""
import json, math, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")

def cut(hz): return round(math.log(hz / 20.0) / math.log(1500.0), 4)
def tm(sec): return round(max(0.0, min(1.0, math.log(sec / 0.002) / math.log(5000.0))), 4)
def lfo_hz(hz): return round(math.log(hz / 0.03) / math.log(1000.0), 4)
def sh_hz(hz): return round(math.log(hz / 0.3) / math.log(100.0), 4)
def ps_hz(hz): return round(math.log(hz / 0.03) / math.log(10.0 / 0.03), 4)
def porta(sec): return round(math.log(sec / 0.1) / math.log(100.0), 4)
def clk(spm): return round(math.log(spm / 2.0) / math.log(10.0), 4)
def semis(st): return round(0.5 + st / 24.0, 4)
def cents(c): return round(0.5 + c / 200.0, 4)
RANGE = {"LO": 0, "32": 1, "16": 2, "8": 3, "4": 4, "2": 5}
WAVE = {"sine": 0, "tri": 1, "saw": 2, "pulse": 3}

presets = []

def vco(n, wave="saw", rng="8", st=0.0, ct=0.0, pw=0.5, key=True, mods=(), att=None, pwm=0.0, sync_weak=False):
    d = {f"vco{n}_wave": WAVE[wave], f"vco{n}_range": RANGE[rng], f"vco{n}_pitch": semis(st), f"vco{n}_fine": cents(ct),
         f"vco{n}_pw": pw, f"vco{n}_keycv": 0 if key else 1}
    for i, m in enumerate(mods): d[f"vco{n}_mod{i+1}"] = m
    for i, a in enumerate(att or []): d[f"vco{n}_att{i+1}"] = a          # 0 = 1/10, 1 = x1
    if n == 1: d["vco1_pwm"] = pwm
    if n > 1: d[f"vco{n}_syncmode"] = 1 if sync_weak else 0
    return d

def vcf(n, levels=(0.8, 0, 0, 0, 0), hz=1200, res=0.0, key=1.0, env=0.3, lfo=0.0, ext=0.0, adsr=1, mode=2):
    d = {f"vcf{n}_cutoff": cut(hz), f"vcf{n}_res": res, f"vcf{n}_mod1": key, f"vcf{n}_mod2": env, f"vcf{n}_mod3": lfo,
         f"vcf{n}_mod4": ext, f"vcf{n}_adsr": adsr - 1}
    for i, l in enumerate(levels): d[f"vcf{n}_lvl{i+1}"] = l
    if n == 2: d["vcf2_mode"] = {"HP": 0, "BP": 1, "LP": 2}[mode] if isinstance(mode, str) else mode
    return d

def vca(n, levels=(0.8, 0, 0), gain=0.0, env=1.0, ext=0.0, adsr=1, exp=False):
    d = {f"vca{n}_gain": gain, f"vca{n}_mod1": env, f"vca{n}_mod2": ext, f"vca{n}_adsr": adsr - 1, f"vca{n}_exp": 1 if exp else 0}
    for i, l in enumerate(levels): d[f"vca{n}_lvl{i+1}"] = l
    return d

def adsr(n, a=0.005, d=0.4, s=0.6, r=0.4, mode="G+T", fast=False):
    sc = 10.0 if fast else 1.0
    return {f"adsr{n}_a": tm(a * sc), f"adsr{n}_d": tm(d * sc), f"adsr{n}_s": s, f"adsr{n}_r": tm(r * sc),
            f"adsr{n}_mode": {"G+T": 0, "GATE": 1, "EXT": 2}[mode], f"adsr{n}_time": 1 if fast else 0}

def lfo(n, hz=5.0, wave="tri", delay=0.002, pw=0.5, keytrig=False):
    w = {"square": 0, "pulse": 0, "saw": 1, "tri": 2, "sine": 3}[wave]
    d = {f"lfo{n}_freq": lfo_hz(hz), f"lfo{n}_wave": w, f"lfo{n}_keytrig": 0 if keytrig else 1}
    if n == 1: d["lfo1_delay"] = tm(delay)
    else: d["lfo2_pw"] = pw
    return d

def out(p1=0.75, p2=0.0, rev=0.35, pos=(0.5, 0.5, 0.5), rev_in=(0.35, 0.0, 0.0), ps=None, monitor=0.7):
    d = {"pan_lvl1": p1, "pan_lvl2": p2, "pan_lvl3": rev, "pan_pos1": pos[0], "pan_pos2": pos[1], "pan_pos3": pos[2],
         "rev_lvl1": rev_in[0], "rev_lvl2": rev_in[1], "rev_lvl3": rev_in[2], "monitor": monitor}
    if ps:
        d.update({"ps_lvl1": ps.get("in1", 0.8), "ps_lvl2": ps.get("in2", 0.0), "ps_res": ps.get("res", 0.3),
                  "ps_speed": ps_hz(ps.get("hz", 0.3)), "ps_int": ps.get("int", 0.7), "ps_auto": 0})
    return d

def kb(portamento=None, bend=1, pitch=0.0):
    d = {"kb_bendrange": bend, "kb_pitch": round(0.5 + pitch / 12.0, 4)}
    if portamento: d.update({"kb_portaon": 1, "kb_porta": porta(portamento)})
    return d

def seq(steps_a, steps_b=None, steps_c=None, rate=120, x10=None, div=None, hold=(), n=None, rng=("2V", "10V", "10V"), mode="A", repeat=True, pw=0.5, fine=None):
    """steps_*: valores en voltios (con el rango elegido) o semitonos si el rango es 2V (st/12)."""
    R = {"10V": 0, "4V": 1, "2V": 2}
    full = {"10V": 10.0, "4V": 4.0, "2V": 2.0}
    # rate en BPM: el reloj del 717A cuenta redondas por minuto con DIVIDE en "1" (BPM / 4).
    whole = rate / 4.0
    if x10 is None: x10 = whole > 20.0
    d = {"clk_rate": clk(whole / (10.0 if x10 else 1.0)), "clk_range": 0 if x10 else 1, "clk_pw": pw,
         "par_range_a": R[rng[0]], "par_range_b": R[rng[1]], "par_range_c": R[rng[2]],
         "ser_mode": {"A": 0, "A-B": 1, "A-B-C": 2, "MANUAL": 3}[mode], "ser_repeat": 1 if repeat else 0,
         "ser_steps": (n or len(steps_a)) - 1}
    for ch, vals, r in (("a", steps_a, rng[0]), ("b", steps_b, rng[1]), ("c", steps_c, rng[2])):
        if vals is None: continue
        for i, v in enumerate(vals):
            d[f"step{i+1}_{ch}"] = round(max(0.0, min(1.0, v / full[r])), 4)
    DIV = {"1/32": 0, "1/16": 1, "1/8": 2, "1/4": 3, "1/2": 4, "1": 5}
    for i in range(12):
        d[f"step{i+1}_div"] = DIV[(div[i] if isinstance(div, (list, tuple)) else div) if div else "1/8"]
        d[f"step{i+1}_fine"] = (fine[i] if fine else 0.0)
        d[f"step{i+1}_hold"] = 1 if (i + 1) in hold else 0
    return d

def st(*notes): return [n / 12.0 for n in notes]          # semitonos → voltios (1 V/oct)

def cable(src, dst, color="red"): return {"src": src, "dst": dst, "color": color}

def preset(name, cat, *parts, settings=None, cables=None, demo="", run=False):
    p = {}
    for part in parts: p.update(part)
    presets.append(dict(name=name, category=cat, panel=p, settings=settings or {}, cables=cables or [], demoNotes=demo, run=run))

# Bus: el VCO-1 suena por el VCF-1 si su fader de entrada está arriba. Ajustes de fábrica que
# se repiten: VCA-1 a PAN 1, VCA-2 a PAN 2, reverb a PAN 3.
POLY6 = {"voices": 3}
POLY8 = {"voices": 4}
MONO = {"voices": 1}

# ───────── Basics ─────────
preset("Init", "Basics", demo="60:1,64:1,67:2")
preset("Fat Saw Lead", "Lead",
       vco(1, "saw"), vco(2, "saw", ct=7), vco(3, "saw", rng="16", ct=-5),
       vcf(1, (0.7, 0.6, 0.5, 0, 0), hz=260, res=0.25, env=0.42),
       vca(1), adsr(1, 0.006, 0.6, 0.55, 0.35), lfo(1, 5.2, delay=0.6), kb(portamento=0.15), out(rev=0.3, rev_in=(0.3, 0, 0)),
       settings=MONO, demo="60:0.5,63:0.5,67:0.5,70:1,72:1.5")
preset("Tomita Brass", "Brass",
       vco(1, "saw"), vco(2, "saw", ct=-6), vco(3, "pulse", rng="16", pw=0.35),
       vcf(1, (0.7, 0.7, 0.4, 0, 0), hz=150, res=0.12, env=0.36), adsr(1, 0.09, 0.9, 0.7, 0.45),
       vca(1, env=1.0), lfo(1, 5.5, delay=1.2), out(rev=0.45, rev_in=(0.45, 0, 0)),
       {"vco1_mod1": 0.03, "vco2_mod1": 0.025},
       settings=POLY6, demo="53+60+65:1.5,55+62+67:1.5,57+64+69:2.5")
preset("Ensemble Strings", "Strings",
       vco(1, "pulse", pw=0.3, pwm=0.55), vco(2, "saw", ct=8), vco(3, "saw", ct=-9),
       vcf(1, (0.6, 0.6, 0.6, 0, 0), hz=700, res=0.05, env=0.12), adsr(1, 0.6, 1.5, 0.85, 1.6),
       vca(1), lfo(2, 0.7, "tri"), out(p1=0.8, rev=0.5, rev_in=(0.5, 0, 0), ps={"in1": 0.85, "res": 0.25, "hz": 0.25, "int": 0.8}),
       settings=POLY6, cables=[cable("lfo2_out", "vco1_pwm", "blue"), cable("ps_out", "pan_in1", "yellow")],
       demo="48+55+64+71:3,50+57+65+69:3,47+55+62+67:3")
preset("Phaser Pad", "Pads",
       vco(1, "saw"), vco(2, "saw", st=12, ct=5), vco(3, "tri", st=7),
       vcf(1, (0.6, 0.5, 0.4, 0, 0), hz=420, res=0.35, env=0.22, lfo=0.05), adsr(1, 1.2, 2.0, 0.8, 2.5),
       vca(1), lfo(1, 0.15, "tri"), out(p1=0.3, p2=0.8, rev=0.55, rev_in=(0.5, 0, 0), ps={"in1": 0.9, "res": 0.55, "hz": 0.12, "int": 0.9}, pos=(0.3, 0.7, 0.5)),
       settings=POLY6, cables=[cable("ps_out", "pan_in2", "yellow")], demo="45+52+60+64:4,41+48+57+60:4")

# ───────── Bass ─────────
preset("700 Bass", "Bass",
       vco(1, "saw", rng="16"), vco(2, "pulse", rng="16", pw=0.4, ct=4),
       vcf(1, (0.8, 0.6, 0, 0, 0), hz=95, res=0.3, env=0.45), adsr(1, 0.002, 0.28, 0.25, 0.12),
       vca(1), adsr(2, 0.002, 0.5, 0.7, 0.1), {"vca1_adsr": 1}, out(rev=0.08, rev_in=(0.1, 0, 0)),
       settings=MONO, demo="36:0.25,36:0.25,48:0.25,36:0.25,39:0.25,36:0.25,46:0.25,48:0.25")
preset("Sheffield Bass", "Bass",
       vco(1, "saw", rng="16"), vco(2, "saw", rng="16", ct=-9), vco(3, "pulse", rng="32", pw=0.5),
       vcf(1, (0.7, 0.7, 0.45, 0, 0), hz=120, res=0.18, env=0.5), adsr(1, 0.002, 0.18, 0.0, 0.1, fast=False),
       vca(1, env=1.0), adsr(2, 0.002, 0.3, 0.8, 0.08), {"vca1_adsr": 1}, out(rev=0.1, rev_in=(0.12, 0, 0)),
       settings=MONO, demo="40:0.25,40:0.25,52:0.25,40:0.25,43:0.25,45:0.25,40:0.25,47:0.25")
preset("Sub Pulse", "Bass",
       vco(1, "pulse", rng="32", pw=0.5), vco(2, "sine", rng="16"),
       vcf(1, (0.8, 0.5, 0, 0, 0), hz=70, res=0.1, env=0.3), adsr(1, 0.004, 0.5, 0.4, 0.2), vca(1), out(rev=0.05),
       settings=MONO, demo="36:0.5,43:0.5,41:0.5,38:0.5")

# ───────── Lead ─────────
preset("Sync Sweep", "Lead",
       vco(1, "saw", key=True), vco(2, "saw", st=7, mods=(0.0, 0.32, 0.0)),
       vcf(2, (0.0, 0.8, 0.0, 0.0, 0.0), hz=900, res=0.2, env=0.18, mode="LP"),
       vca(1, levels=(0.0, 0.8, 0.0)), adsr(1, 0.01, 1.4, 0.2, 0.6), lfo(1, 5.0, delay=0.8), out(rev=0.25, rev_in=(0.3, 0, 0)),
       {"vcf2_mod1": 1.0, "vcf1_lvl1": 0.0},
       settings=MONO, cables=[cable("vco1_saw", "vco2_sync", "yellow")], demo="57:1,60:1,62:0.5,64:1.5")
preset("Tomita Whistle", "Lead",
       vcf(1, (0, 0, 0, 0, 0), hz=118, res=0.93, env=0.0, lfo=0.012), adsr(1, 0.12, 0.6, 0.85, 0.7),
       vca(1, env=1.0), lfo(1, 5.6, "sine", delay=0.5), kb(portamento=0.12), out(rev=0.5, rev_in=(0.55, 0, 0)),
       settings=MONO, demo="72:1,74:0.5,76:0.5,79:1.5,77:0.5,76:1")
preset("Flute", "Lead",
       vco(1, "tri"), vco(2, "sine", st=12, ct=3),
       vcf(1, (0.8, 0.25, 0, 0.06, 0), hz=420, res=0.05, env=0.18), adsr(1, 0.08, 0.6, 0.75, 0.3),
       vca(1), lfo(1, 5.0, "sine", delay=0.7), {"vco1_mod1": 0.022, "vco2_mod1": 0.022}, out(rev=0.4, rev_in=(0.45, 0, 0)),
       settings=MONO, demo="67:0.75,69:0.25,71:1,74:1,72:0.5,71:0.5,69:1")
preset("PWM Lead", "Lead",
       vco(1, "pulse", pw=0.5, pwm=0.6), vco(2, "saw", ct=6),
       vcf(1, (0.8, 0.4, 0, 0, 0), hz=500, res=0.2, env=0.25), adsr(1, 0.01, 0.8, 0.6, 0.4),
       vca(1), lfo(2, 4.0, "tri"), lfo(1, 5.5, delay=0.4), kb(portamento=0.08), out(rev=0.25),
       settings=MONO, cables=[cable("lfo2_out", "vco1_pwm", "blue")], demo="62:0.5,65:0.5,69:1,67:0.5,65:0.5,62:1")
preset("Duophonic Fifths", "Lead",
       vco(1, "saw"), vco(2, "pulse", key=False, mods=(0, 0, 1.0), pw=0.4),
       vcf(1, (0.7, 0.7, 0, 0, 0), hz=380, res=0.2, env=0.3), adsr(1, 0.01, 0.7, 0.6, 0.4), vca(1), out(rev=0.3),
       {"vco2_att3": 1}, settings={"voices": 0},
       cables=[cable("kb_cv2", "vco2_ext", "white")], demo="55+62:1,57+64:1,53+60:1,55+67:1.5")
preset("Glide Saw", "Lead",
       vco(1, "saw"), vco(2, "saw", st=-12, ct=4),
       vcf(1, (0.8, 0.6, 0, 0, 0), hz=300, res=0.28, env=0.32), adsr(1, 0.01, 1.0, 0.6, 0.5),
       vca(1), kb(portamento=0.35), lfo(1, 4.5, delay=0.9), {"vco1_mod1": 0.02, "vco2_mod1": 0.02}, out(rev=0.3),
       settings=MONO, demo="60:1,67:1,72:0.5,70:0.5,67:1")

# ───────── Keys / bells ─────────
preset("Ring Bells", "Bells",
       vco(1, "sine", st=0), vco(2, "sine", st=7, ct=37, key=True), {"rm_x": 0, "rm_y": 1},
       vcf(2, (0.0, 0.0, 0.0, 0.85, 0.0), hz=5200, res=0.1, key=0.0, env=0.0, mode="LP"),
       vca(2, levels=(0.0, 0.85, 0.0)), adsr(2, 0.002, 2.6, 0.0, 2.4), {"vca2_adsr": 1},
       out(p1=0.0, p2=0.7, rev=0.55, rev_in=(0.0, 0.6, 0.0)), {"vcf1_lvl1": 0.0},
       settings=POLY6, demo="72:1,79:1,84:1,76+83:2")
preset("Tine Keys", "Keys",
       vco(1, "tri"), vco(2, "sine", st=12), vco(3, "sine", st=31, ct=6),
       vcf(1, (0.7, 0.4, 0.12, 0, 0), hz=900, res=0.0, env=0.25), adsr(1, 0.002, 1.4, 0.15, 0.5), vca(1),
       out(rev=0.35, ps={"in1": 0.6, "res": 0.1, "hz": 0.8, "int": 0.4}, p1=0.5, p2=0.6, pos=(0.35, 0.65, 0.5)), settings=POLY6,
       cables=[cable("ps_out", "pan_in2", "yellow")], demo="60+64+67:1,62+65+69:1,64+67+71:1,65+69+72:2")
preset("Clavi", "Keys",
       vco(1, "pulse", pw=0.22), vco(2, "pulse", st=12, pw=0.4),
       vcf(2, (0.8, 0.6, 0, 0, 0), hz=700, res=0.35, env=0.32, mode="LP"), adsr(1, 0.001, 0.3, 0.0, 0.15),
       vca(1, levels=(0.0, 0.85, 0.0)), {"vcf1_lvl1": 0.0}, out(rev=0.15), settings=POLY6,
       demo="60:0.25,63:0.25,67:0.25,63:0.25,65:0.25,68:0.25,72:0.5")

# ───────── Sequences (717A) ─────────
BERLIN = st(0, 12, 7, 10, 0, 12, 3, 10, 0, 12, 7, 15)
preset("Berlin Twelve", "Sequence",
       vco(1, "saw", rng="16", key=False, mods=(0.0, 1.0)), vco(2, "saw", rng="16", st=0, ct=8, key=False, mods=(0, 0, 1.0)),
       vcf(1, (0.7, 0.6, 0, 0, 0), hz=110, res=0.42, key=0.0, env=0.4, ext=0.12), adsr(1, 0.002, 0.22, 0.0, 0.18, mode="EXT"),
       vca(1, gain=0.0), out(rev=0.35, rev_in=(0.35, 0, 0)), lfo(1, 0.08, "tri"), {"vcf1_mod3": 0.06, "vco1_att2": 1, "vco2_att3": 1},
       seq(BERLIN, [6, 2, 3, 2, 8, 2, 3, 2, 6, 3, 4, 9], rate=128, div="1/16", rng=("2V", "10V", "10V")),
       cables=[cable("par_a1", "vco1_ext", "red"), cable("par_a2", "mult1_1", "red"), cable("mult1_2", "vco2_ext", "red"),
               cable("clk_out1", "adsr1_gate", "yellow"), cable("par_b1", "vcf1_cv", "blue")],
       run=True)
preset("Transposed Sequence", "Sequence",
       vco(1, "pulse", pw=0.35, key=True, mods=(0.0, 1.0)), vco(2, "saw", st=12, ct=-6, key=True, mods=(0, 0, 1.0)),
       vcf(1, (0.7, 0.4, 0, 0, 0), hz=180, res=0.3, key=1.0, env=0.3), adsr(1, 0.002, 0.3, 0.15, 0.2, mode="EXT"),
       vca(1), out(rev=0.3), {"vco1_att2": 1, "vco2_att3": 1},
       seq(st(0, 7, 12, 7, 3, 10, 15, 10), rate=120, div="1/16", n=8),
       cables=[cable("par_a1", "vco1_ext", "red"), cable("par_a2", "vco2_ext", "red"), cable("clk_out1", "adsr1_gate", "yellow")],
       run=True, demo="48:4,53:4,51:4,55:4")
preset("Thirty-Six Steps", "Sequence",
       vco(1, "tri", key=False, mods=(0.0, 1.0)), vco(2, "pulse", st=12, pw=0.3, key=False, mods=(0, 0, 1.0)),
       vcf(1, (0.7, 0.35, 0, 0, 0), hz=600, res=0.2, key=0.0, env=0.2), adsr(1, 0.002, 0.35, 0.2, 0.3, mode="EXT"),
       vca(1), out(rev=0.4, rev_in=(0.4, 0, 0)), {"vco1_att2": 1, "vco2_att3": 1},
       seq(st(12, 14, 15, 19, 17, 15, 14, 12, 10, 12, 14, 15), st(19, 17, 15, 14, 15, 17, 19, 22, 20, 19, 17, 15),
           st(14, 12, 10, 12, 7, 10, 12, 14, 15, 14, 12, 10), rate=150, div="1/16", rng=("2V", "2V", "2V"), mode="A-B-C"),
       cables=[cable("ser_out1", "vco1_ext", "red"), cable("ser_out2", "vco2_ext", "red"), cable("clk_out1", "adsr1_gate", "yellow")],
       run=True)
preset("Sequenced Filter", "Sequence",
       vco(1, "saw", rng="16"), vco(2, "saw", rng="16", ct=7), vco(3, "pulse", rng="32", pw=0.45),
       vcf(1, (0.7, 0.7, 0.6, 0, 0), hz=110, res=0.4, key=1.0, env=0.0, ext=0.5), adsr(1, 0.002, 0.3, 0.6, 0.2),
       vca(1, gain=0.0, env=1.0), out(rev=0.3),
       seq([1.0, 4.0, 2.0, 6.0, 1.0, 5.0, 3.0, 8.0], rate=120, div="1/16", n=8, rng=("10V", "10V", "10V")),
       cables=[cable("par_a1", "vcf1_cv", "blue")], run=True, settings=MONO, demo="36:4,39:4,34:4,36:4")
preset("Gate Patterns", "Sequence",
       vco(1, "pulse", rng="8", pw=0.25, key=False, mods=(0.0, 1.0)), vco(2, "saw", st=-12, key=False, mods=(0, 0, 1.0)),
       vcf(2, (0.8, 0.8, 0, 0, 0), hz=500, res=0.6, key=0.0, env=0.35, mode="BP"), adsr(1, 0.001, 0.22, 0.0, 0.1, mode="EXT"),
       vca(1, levels=(0, 0.85, 0)), {"vcf1_lvl1": 0.0, "vco1_att2": 1, "vco2_att3": 1}, out(rev=0.25),
       seq(st(0, 0, 12, 0, 7, 0, 10, 12, 0, 3, 0, 15), rate=132, div="1/16", hold=(2, 6, 9, 11)),
       cables=[cable("par_a1", "vco1_ext", "red"), cable("par_a2", "vco2_ext", "red"), cable("clk_out1", "adsr1_gate", "yellow")],
       run=True)

# ───────── S&H, FX ─────────
preset("Random Computer", "FX",
       vco(1, "sine", rng="4", key=False, mods=(0.0, 0.0)), vco(3, "sine", rng="8", key=False, mods=(0.0, 0.0, 0.0)),
       {"vco3_mod2": 0.4, "vco3_att2": 1},
       vcf(1, (0.0, 0.0, 1.0, 0, 0), hz=4000, res=0.1, key=0.0, env=0.0), adsr(1, 0.002, 0.1, 0.0, 0.06, mode="EXT"),
       vca(1), {"sh_mode": 1, "sh_clock": 1, "sh_rate": sh_hz(9.0)}, out(rev=0.35, rev_in=(0.35, 0, 0)),
       cables=[cable("sh_clkout", "adsr1_gate", "yellow")])
preset("S&H Filter", "FX",
       vco(1, "saw"), vco(2, "saw", ct=9),
       vcf(1, (0.7, 0.7, 0, 0, 0), hz=200, res=0.65, env=0.0, ext=0.35), adsr(1, 0.01, 0.5, 1.0, 0.5),
       vca(1), {"sh_mode": 1, "sh_clock": 1, "sh_rate": sh_hz(7.0)}, out(rev=0.35),
       cables=[cable("sh_out", "vcf1_cv", "blue")], settings=MONO, demo="45:3,48:3,43:3")
preset("Surf and Wind", "FX",
       vcf(1, (0.0, 0.0, 0.0, 0.0, 1.0), hz=700, res=0.35, key=0.0, env=0.0, lfo=0.32),
       vca(1, gain=1.0, env=0.0), lfo(1, 0.09, "tri"), lfo(2, 0.13, "sine"),
       vcf(2, (0.0, 0.0, 0.0, 0.0, 0.0), hz=900, res=0.3), out(rev=0.6, rev_in=(0.6, 0, 0)),
       cables=[cable("noise_pink", "vcf1_in", "white")])
preset("Laser Zaps", "FX",
       vco(1, "pulse", rng="4", pw=0.45, mods=(0.0, 1.0)), {"vco1_att2": 1},
       vcf(1, (1.0, 0, 0, 0, 0), hz=4000, res=0.2, key=0.0, env=0.0), adsr(1, 0.001, 0.4, 0.0, 0.3), adsr(2, 0.001, 0.2, 0.0, 0.15),
       vca(1), {"mix_lvl1": 0.35}, out(rev=0.45, rev_in=(0.5, 0, 0)),
       cables=[cable("adsr2_out", "mix_in1", "green"), cable("mix_out", "vco1_ext", "green")],
       settings=MONO, demo="72:0.25,76:0.25,72:0.25,79:0.5,84:0.5")
preset("Thunder", "FX",
       vcf(1, (0, 0, 0, 0.0, 1.0), hz=90, res=0.15, key=0.0, env=0.45), adsr(1, 0.02, 3.5, 0.0, 4.0), vca(1, levels=(1.0, 0, 0), env=1.0), vca(2, levels=(1.0, 0, 0), env=1.0),
       out(p2=1.0, rev=0.65, rev_in=(0.7, 0, 0), pos=(0.35, 0.65, 0.5)), cables=[cable("noise_pink", "vcf1_in", "white")], settings=MONO, demo="36:4")
preset("Space Drone", "Drone",
       vco(1, "saw", rng="16", key=False, st=0), vco(2, "saw", rng="16", key=False, st=7, ct=-11), vco(3, "sine", rng="LO", key=False, st=-6),
       {"vco1_mod2": 0.05, "vco1_att2": 1},
       vcf(1, (0.6, 0.6, 0, 0.05, 0), hz=240, res=0.6, key=0.0, env=0.0, lfo=0.15), lfo(1, 0.06, "tri"),
       vca(1, gain=1.0, env=0.0), out(rev=0.6, rev_in=(0.6, 0, 0), ps={"in1": 0.8, "res": 0.4, "hz": 0.07, "int": 0.8}, p1=0.5, p2=0.8, pos=(0.3, 0.7, 0.5)),
       cables=[cable("vco3_sine", "vco1_ext", "white"), cable("ps_out", "pan_in2", "yellow")], settings=MONO)
preset("Spring Drips", "FX",
       vco(1, "sine", rng="2", key=False, mods=(0, 0)), {"vco1_mod2": 0.3, "vco1_att2": 1},
       vcf(1, (0.8, 0, 0, 0, 0), hz=8000, key=0.0, env=0.0), adsr(1, 0.001, 0.05, 0.0, 0.04, mode="EXT"),
       vca(1, levels=(0.8, 0, 0)), {"sh_mode": 1, "sh_clock": 1, "sh_rate": sh_hz(3.5), "rev_lvl1": 1.0},
       out(p1=0.25, rev=0.85, rev_in=(1.0, 0, 0)),
       cables=[cable("sh_out", "vco1_ext", "blue"), cable("sh_clkout", "adsr1_gate", "yellow")])

# ───────── Percussion ─────────
preset("Kick", "Drums",
       vcf(1, (0, 0, 0, 0, 0), hz=42, res=0.9, key=0.0, env=0.3), adsr(1, 0.001, 0.09, 0.0, 0.08, fast=False),
       vca(1, levels=(1.0, 0, 0), env=1.0), vca(2, levels=(1.0, 0, 0), env=1.0, adsr=2), adsr(2, 0.001, 0.35, 0.0, 0.3), {"vca1_adsr": 1}, out(p1=1.0, p2=1.0, rev=0.05),
       settings={"voices": 1, "vcf1_model": 1, "vcf1_mode": 2}, demo="36:0.5,36:0.5,36:0.25,36:0.75")
preset("Snare", "Drums",
       vco(1, "tri", rng="8", key=False, st=-7), vcf(2, (0.5, 0, 0, 0, 0.85), hz=1800, res=0.3, key=0.0, env=0.25, mode="BP"),
       adsr(1, 0.001, 0.16, 0.0, 0.14), vca(1, levels=(0, 0.9, 0)), {"vcf1_lvl1": 0.0}, out(rev=0.25, rev_in=(0.3, 0, 0)),
       settings=MONO, cables=[cable("noise_white", "vcf2_in", "white")], demo="38:0.5,38:0.5,38:0.25,38:0.25,38:0.5")
preset("Hi-Hat", "Drums",
       vcf(2, (0, 0, 0, 0, 0.9), hz=7000, res=0.2, key=0.0, env=0.0, mode="HP"), adsr(1, 0.001, 0.05, 0.0, 0.05),
       vca(1, levels=(0, 0.9, 0)), {"vcf1_lvl1": 0.0}, out(rev=0.1), settings=MONO, cables=[cable("noise_white", "vcf2_in", "white")], demo="42:0.25,42:0.25,42:0.25,42:0.25,42:0.5,42:0.5")

def write():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, p in enumerate(presets):
        slug = "".join(c.lower() if c.isalnum() else "_" for c in p["name"]).strip("_")
        while "__" in slug: slug = slug.replace("__", "_")
        json.dump(p, open(os.path.join(OUT, f"{i:02d}_{slug}.json"), "w"), indent=1)
    print(len(presets), "presets")

if __name__ == "__main__":
    write()
