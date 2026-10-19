#!/usr/bin/env python3
"""Presets de fábrica del ATMD: escribe plugins/atmd/assets/presets/pNN_nombre.json.

Cada preset lista sólo lo que cambia respecto del valor por defecto de la tabla (Source/ParamsMd.h).
Rangos: 0 LO, 1 32', 2 16', 3 8', 4 4', 5 2'. Formas: 0 tri, 1 tri-saw (osc 3: sierra invertida),
2 sierra, 3 cuadrada, 4 pulso ancho, 5 pulso angosto. Perillas en números del dial.

    python3 plugins/atmd/tools/presetsmd.py
"""
import json, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")

ON, OFF = 1, 0

def three(r1=3, r2=3, r3=3, w1=2, w2=2, w3=2, f2=0.0, f3=0.0, m1=7, m2=0, m3=0):
    d = dict(osc1_range=r1, osc2_range=r2, osc3_range=r3, osc1_wave=w1, osc2_wave=w2, osc3_wave=w3,
             osc2_freq=f2, osc3_freq=f3, mix_osc1=m1, mix_osc2=m2, mix_osc3=m3,
             mix_osc1_on=ON if m1 > 0 else OFF, mix_osc2_on=ON if m2 > 0 else OFF, mix_osc3_on=ON if m3 > 0 else OFF)
    return d

def filt(cut=0.0, emph=0.0, amt=5.0, a=0.0, d=4.0, s=5.0, k1=OFF, k2=OFF):
    return dict(filter_cutoff=cut, filter_emphasis=emph, filter_contour=amt, f_attack=a, f_decay=d, f_sustain=s, kbd_ctrl1=k1, kbd_ctrl2=k2)

def loud(a=0.0, d=4.0, s=8.0):
    return dict(l_attack=a, l_decay=d, l_sustain=s)

P = []
def add(name, cat, *parts, **kw):
    d = {}
    for p in parts: d.update(p)
    d.update(kw)
    P.append((name, cat, d))

# ── Init ──
add("Init", "Init")

# ── Bass ──
add("Fat Bass", "Bass", three(2, 2, 1, 2, 2, 3, 0.08, 0, 8, 7, 5), filt(-1.5, 2.5, 6.5, 0, 3.6, 2.5, ON), loud(0, 4.5, 8))
add("Taurus Low", "Bass", three(1, 1, 2, 2, 3, 2, -0.06, 0, 8, 6, 4), filt(-2.8, 1.5, 6, 0, 4.2, 3), loud(0, 5, 9), glide_on=ON, glide=3.2)
add("Plucked Bass", "Bass", three(2, 2, 3, 3, 2, 2, 0, 0.1, 7, 6, 0), filt(-3.0, 4.0, 7.5, 0, 2.6, 0, ON), loud(0, 3.4, 4), decay_on=OFF)
add("Squelch Bass", "Bass", three(2, 2, 3, 2, 4, 2, 0, 0, 8, 5, 0), filt(-3.4, 7.2, 8, 0, 2.8, 1, ON), loud(0, 3.6, 6))
add("Rubber Bass", "Bass", three(2, 2, 2, 0, 3, 2, 0.04, 0, 9, 6, 0), filt(-2.2, 3.0, 5, 0, 3.0, 4), loud(0, 4, 8), glide_on=ON, glide=1.8)
add("Moog Thump", "Bass", three(2, 1, 3, 3, 3, 0, 0, 0, 8, 7, 0), filt(-3.6, 0.0, 7, 0, 1.8, 0), loud(0, 2.3, 0), decay_on=ON)
add("Funk Clav Bass", "Bass", three(3, 2, 3, 5, 4, 2, 0, 0, 7, 5, 0), filt(-1.8, 3.5, 6, 0, 2.2, 1.5, ON), loud(0, 2.6, 3), decay_on=OFF)
add("Sub Drop", "Bass", three(1, 1, 0, 0, 2, 0, 0, 0, 9, 3, 0), filt(-1.0, 0, 3, 0, 5, 7), loud(0, 5.5, 9),
    osc3_ctrl=OFF, osc_mod_on=ON, mod_mix=0, osc3_range=0, osc3_freq=-6.0)

# ── Lead ──
add("Lucky Lead", "Lead", three(3, 3, 4, 2, 2, 3, 0.12, 0, 7, 7, 4), filt(-0.2, 3.0, 4.5, 0.6, 5.0, 6, ON, ON), loud(0.3, 5, 9),
    glide_on=ON, glide=3.6, osc_mod_on=ON, mod_mix=0, osc3_range=0, osc3_wave=0, osc3_ctrl=OFF, osc3_freq=1.2, mix_osc3=0, mix_osc3_on=OFF)
add("Prog Solo", "Lead", three(3, 3, 3, 2, 2, 2, 0.15, -0.12, 7, 7, 6), filt(0.3, 4.0, 4, 0.3, 5.5, 6, ON, ON), loud(0, 5, 9), glide_on=ON, glide=2.6)
add("Square Whistle", "Lead", three(4, 4, 3, 3, 0, 2, 0.04, 0, 6, 5, 0), filt(0.8, 1.0, 2, 0, 5, 8, ON, ON), loud(0.4, 5, 9),
    osc_mod_on=ON, osc3_ctrl=OFF, osc3_range=0, osc3_wave=0, osc3_freq=2.0, mod_mix=0)
add("Singing Filter", "Lead", three(3, 3, 3, 2, 2, 2, 0, 0, 3, 0, 0), filt(-0.5, 8.6, 3.5, 0.8, 5.5, 6, ON, ON), loud(0.2, 5, 9), glide_on=ON, glide=3.0)
add("Fifth Lead", "Lead", three(3, 3, 3, 2, 2, 2, 7.0, 0, 7, 6, 0), filt(0.0, 2.5, 4.5, 0.2, 4.6, 5, ON, ON), loud(0, 5, 9), glide_on=ON, glide=2.2)
add("Thin Pulse Lead", "Lead", three(3, 4, 3, 5, 5, 2, -0.08, 0, 7, 6, 0), filt(1.0, 2.0, 3, 0, 4.5, 6, ON, ON), loud(0, 4.5, 9))
add("Screamer", "Lead", three(3, 3, 3, 2, 3, 2, 0.1, 0, 10, 10, 0), filt(-0.3, 6.5, 5, 0, 5, 5, ON, ON), loud(0, 5, 9), mix_noise=2, mix_noise_on=ON)
add("Detuned Saws", "Lead", three(3, 3, 2, 2, 2, 2, 0.22, -0.18, 7, 7, 6), filt(0.6, 2.0, 3.5, 0.4, 5, 7, ON, ON), loud(0.2, 5, 9))

# ── Brass and keys ──
add("Brass Section", "Brass and keys", three(3, 3, 2, 2, 2, 2, 0.14, -0.1, 7, 7, 5), filt(-1.2, 1.5, 5.5, 3.0, 4.6, 5, ON), loud(1.2, 4.5, 9))
add("Soft Horn", "Brass and keys", three(3, 3, 3, 1, 2, 2, 0.05, 0, 7, 5, 0), filt(-1.6, 1.0, 4.5, 3.6, 4.4, 5, ON), loud(2.0, 4.5, 9))
add("Organ Keys", "Brass and keys", three(3, 4, 5, 3, 3, 3, 0, 0, 7, 5, 4), filt(1.5, 0, 0.5, 0, 4, 10, ON, ON), loud(0, 2.8, 10), decay_on=OFF)
add("Electric Piano", "Brass and keys", three(3, 4, 3, 0, 0, 2, 0, 0, 8, 4, 0), filt(-0.4, 0.5, 4, 0, 4.8, 1, ON), loud(0, 5.4, 0), decay_on=ON, vel_amp=0.5, vel_filter=0.4)
add("Harpsichord", "Brass and keys", three(4, 3, 3, 5, 4, 2, 0, 0, 7, 6, 0), filt(0.2, 1.5, 5, 0, 3.6, 0, ON, ON), loud(0, 4.4, 0), decay_on=ON)

# ── Effects and percussion ──
add("Kick", "Effects and percussion", three(3, 3, 3, 0, 2, 2, 0, 0, 0, 0, 0), filt(-4.0, 10.0, 7, 0, 2.6, 0, ON, ON), loud(0, 3.2, 0), decay_on=ON)
add("Snare", "Effects and percussion", three(3, 3, 3, 0, 2, 2, 0, 0, 3, 0, 0), filt(1.0, 1.5, 3, 0, 2.4, 0, ON), loud(0, 2.6, 0),
    mix_noise=8, mix_noise_on=ON, decay_on=ON)
add("Hi-Hat", "Effects and percussion", three(3, 3, 3, 2, 2, 2, 0, 0, 0, 0, 0), filt(4.0, 2.0, 1, 0, 1.6, 0), loud(0, 1.8, 0),
    mix_noise=10, mix_noise_on=ON, decay_on=ON)
add("Wind", "Effects and percussion", three(3, 3, 3, 2, 2, 2, 0, 0, 0, 0, 0), filt(-1.0, 7.0, 0, 0, 4, 10), loud(4.5, 5, 10),
    mix_noise=8, mix_noise_on=ON, noise_color=1, filter_mod_on=ON, mod_mix=0, osc3_ctrl=OFF, osc3_range=0, osc3_wave=0, osc3_freq=-6.0)
add("Laser Zap", "Effects and percussion", three(4, 3, 3, 2, 2, 2, 0, 0, 7, 0, 0), filt(1.0, 6.0, 8, 0, 2.6, 0, ON, ON), loud(0, 3.0, 0),
    osc_mod_on=ON, mod_mix=0, osc3_ctrl=OFF, osc3_range=0, osc3_wave=1, osc3_freq=7.0, decay_on=ON)
add("Bird Call", "Effects and percussion", three(5, 3, 3, 0, 2, 2, 0, 0, 7, 0, 0), filt(3.0, 0, 0, 0, 4, 10), loud(0.5, 4, 9),
    osc_mod_on=ON, mod_mix=0, osc3_ctrl=OFF, osc3_range=0, osc3_wave=0, osc3_freq=5.5)

# ── Drones and pads (extensión polifónica) ──
add("Poly Strings", "Drones and pads", three(3, 3, 2, 2, 2, 2, 0.12, -0.1, 7, 7, 4), filt(-0.6, 1.0, 2.5, 4.2, 5.6, 6, ON), loud(4.5, 6.2, 9),
    voices=6, unison=1, analog=0.5, spread=0.7, rev_on=ON, rev_mix=0.25)
add("Unison Stack", "Drones and pads", three(2, 3, 3, 2, 2, 3, 0.1, 0, 8, 6, 0), filt(-0.8, 3.0, 5, 0, 4.6, 5, ON), loud(0, 4.8, 9),
    unison=4, detune=14, spread=0.8)
add("Slow Sweep Pad", "Drones and pads", three(3, 3, 3, 2, 3, 0, 0.18, 0, 7, 6, 0), filt(-2.0, 4.5, 5, 6.0, 7.0, 6, ON), loud(5.0, 6.5, 9),
    voices=6, analog=0.6, dly_on=ON, dly_mix=0.2, rev_on=ON, rev_mix=0.3)
add("Drone Machine", "Drones and pads", three(1, 2, 0, 2, 2, 0, 0.05, 0, 8, 6, 0), filt(-1.5, 6.0, 0, 0, 4, 10), loud(6.0, 7, 10),
    filter_mod_on=ON, mod_mix=0, osc3_ctrl=OFF, osc3_wave=0, osc3_freq=-7.0, rev_on=ON, rev_size=0.85, rev_mix=0.35)

# Presets cuyo sonido es la modulación: la rueda MOD arranca levantada (campo "mod_wheel" del preset).
WHEEL = {"Sub Drop": 0.75, "Laser Zap": 0.85, "Bird Call": 0.8, "Wind": 0.9, "Drone Machine": 0.7}

def slug(s): return re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, (name, cat, d) in enumerate(P):
        with open(os.path.join(OUT, f"p{i:02d}_{slug(name)}.json"), "w") as fh:
            doc = {"name": name, "category": cat, "params": d}
            if name in WHEEL: doc["mod_wheel"] = WHEEL[name]
            json.dump(doc, fh, indent=1)
    print(f"{len(P)} presets en {OUT}")

if __name__ == "__main__":
    main()
