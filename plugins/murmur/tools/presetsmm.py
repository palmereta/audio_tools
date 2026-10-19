#!/usr/bin/env python3
"""Presets de fábrica de Murmur (assets/presets/pNN_*.json). Cada preset nombra sólo lo que cambia: el resto queda
en el valor por defecto de ParamsMm.h. Freeze nunca viene encendido (congelaría un búfer vacío): los presets de la
categoría Stretch esperan que el usuario apriete Freeze."""
import json, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")
# Índices de las elecciones (ParamsMm.h).
DIV = {"off": 0, "bar": 1, "1/2": 2, "1/4": 3, "1/8": 4, "1/16": 5, "1/32": 6, "1/4T": 7, "1/8T": 8, "1/4D": 9, "1/8D": 10}
Q = {"free": 0, "semis": 1, "oct": 2, "fifths": 3, "major": 4, "minor": 5, "penta": 6, "whole": 7}
SHAPE = {"sine": 0, "tri": 1, "ramp": 2, "drift": 3, "step": 4}
T = {"off": 0, "position": 1, "spray": 2, "size": 3, "density": 4, "pitch": 5, "pan": 6, "tone": 7, "mix": 8, "feedback": 9}

PRESETS = []
def preset(name, cat, **p):
    for k in list(p):
        if k in ("sync", "delay_sync", "lfo1_sync", "lfo2_sync") and isinstance(p[k], str): p[k] = DIV[p[k]]
        if k == "quant" and isinstance(p[k], str): p[k] = Q[p[k]]
        if k.endswith("_shape") and isinstance(p[k], str): p[k] = SHAPE[p[k]]
        if k.endswith("_target") and isinstance(p[k], str): p[k] = T[p[k]]
    PRESETS.append({"name": name, "category": cat, "params": p})

# ── Clouds
preset("First Murmur", "Clouds", size=180, density=18, spray=0.35, pan=0.6, rev_mix=0.25, mix=0.5)
preset("Slow Fog", "Clouds", size=900, density=8, spray=1.5, shape=1.0, high_cut=6000, rev_mix=0.5, rev_size=0.8, mix=0.6)
preset("Starling Cloud", "Clouds", size=120, density=60, spray=2.0, pan=0.9, pitch_rand=0.3, rev_mix=0.3, mix=0.55)
preset("Dust Motes", "Clouds", size=40, density=40, density_rand=1.0, spray=3.0, buffer=10, reverse=0.3, high_cut=8000, rev_mix=0.4, mix=0.5)
# ── Shimmer
preset("Choir Rising", "Shimmer", size=300, density=20, spray=0.5, feedback=0.7, shimmer=0.5, rev_mix=0.6, rev_size=0.8, mix=0.55)
preset("Glass Halo", "Shimmer", pitch=12, pitch_rand=12, quant="oct", feedback=0.5, shimmer=0.3, size=250, density=22, rev_mix=0.7, mix=0.5)
preset("Cathedral Bloom", "Shimmer", size=700, density=14, spray=1.0, feedback=0.8, shimmer=0.35, sub=0.1, shape=1.0, rev_mix=0.8, rev_size=0.95, duck=0.4, mix=0.6)
# ── Stretch (apretar Freeze)
preset("Stretch Ten Times", "Stretch", size=400, density=30, spray=0.05, scan=0.1, buffer=8, position=0.2, shape=1.0, rev_mix=0.3, mix=0.8)
preset("Reverse Glacier", "Stretch", size=800, density=18, reverse=1.0, scan=-0.05, spray=0.1, shape=1.0, rev_mix=0.6, mix=0.8)
preset("Held Chord", "Stretch", position=0.5, spray=0.1, size=600, density=25, pitch_rand=7, quant="fifths", shimmer=0.2, shape=1.0, rev_mix=0.5, mix=0.8)
# ── Rhythm
preset("Eighth Notes", "Rhythm", sync="1/8", size=90, shape=0.2, skew=-0.6, spray=0.5, delay_sync="1/8D", delay_mix=0.25, delay_fb=0.3, mix=0.6)
preset("Sixteenth Stutter", "Rhythm", sync="1/16", chance=0.7, size=60, shape=0.1, pitch_rand=12, quant="oct", reverse=0.3, spray=0.2, mix=0.6)
preset("Triplet Rain", "Rhythm", sync="1/8T", chance=0.6, size=150, pitch_rand=7, quant="major", spray=1.0, delay_sync="1/4T", delay_mix=0.2, rev_mix=0.3, mix=0.55)
preset("Gated Swells", "Rhythm", sync="1/4", size=450, skew=0.8, reverse=0.5, spray=0.6, rev_mix=0.35, mix=0.6)
# ── Pitch
preset("Octave Shower", "Pitch", pitch_rand=24, quant="oct", density=30, size=150, spray=0.8, rev_mix=0.4, mix=0.55)
preset("Major Sparkle", "Pitch", pitch_rand=12, quant="major", density=25, size=200, shimmer=0.2, spray=0.6, rev_mix=0.4, mix=0.55)
preset("Minor Mist", "Pitch", pitch_rand=12, quant="minor", size=400, density=15, high_cut=7000, spray=1.0, rev_mix=0.5, mix=0.6)
preset("Pentatonic Wind", "Pitch", pitch_rand=12, quant="penta", density=35, size=120, pan=1.0, spray=1.2, rev_mix=0.35, mix=0.55)
preset("Fifths Drone", "Pitch", pitch_rand=19, quant="fifths", size=1200, density=6, shape=1.0, spray=0.8, rev_mix=0.6, mix=0.65)
preset("Down Below", "Pitch", pitch=-12, sub=0.3, size=600, density=14, high_cut=3000, rev_mix=0.4, mix=0.6)
# ── Texture
preset("Tape Scatter", "Texture", size=70, density=50, density_rand=1.0, spray=4.0, buffer=12, reverse=0.5, high_cut=9000,
       lfo1_target="position", lfo1_shape="drift", lfo1_rate=0.2, lfo1_amount=0.4, mix=0.55)
preset("Reverse Swell", "Texture", reverse=1.0, skew=0.9, size=500, density=12, spray=0.8, rev_mix=0.4, mix=0.6)
preset("Granular Wash", "Texture", size=1500, density=10, shape=1.0, spray=3.0, buffer=12, rev_mix=0.7, rev_size=0.9, mix=0.7)
preset("Underwater", "Texture", low_cut=60, high_cut=1200, size=300, density=25, spray=1.0,
       lfo1_target="tone", lfo1_shape="drift", lfo1_rate=0.15, lfo1_amount=0.6, rev_mix=0.4, mix=0.6)
preset("Breathing Cloud", "Texture", size=250, density=20, spray=1.0, lfo1_target="density", lfo1_shape="sine", lfo1_rate=0.1, lfo1_amount=0.8,
       lfo2_target="pan", lfo2_shape="drift", lfo2_rate=0.3, lfo2_amount=0.6, rev_mix=0.45, mix=0.6)
# ── Glitch
preset("Bit Swarm", "Glitch", size=8, density=150, spray=1.0, pitch_rand=24, quant="semis", shape=0.0, pan=1.0, mix=0.5)
preset("Broken Radio", "Glitch", size=25, density=80, density_rand=1.0, reverse=0.5, high_cut=4000, low_cut=400,
       lfo1_target="pitch", lfo1_shape="step", lfo1_rate=4.0, lfo1_amount=0.5, mix=0.55)
preset("Skipping CD", "Glitch", sync="1/16", size=120, spray=0.0, position=0.25, chance=0.5, shape=0.0, mix=0.7)
# ── MIDI
preset("Play the Cloud", "MIDI", midi_mode=1, size=250, density=30, spray=0.6, rev_mix=0.4, mix=0.7)
preset("Gated Choir", "MIDI", midi_mode=2, size=300, density=40, shimmer=0.2, feedback=0.3, spray=0.8, rev_mix=0.5, mix=0.8)

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, p in enumerate(PRESETS):
        slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
        with open(os.path.join(OUT, f"p{i + 1:02d}_{slug}.json"), "w") as fh:
            json.dump(p, fh, indent=1)
    print(f"{len(PRESETS)} presets")

if __name__ == "__main__":
    main()
