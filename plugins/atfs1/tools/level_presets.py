#!/usr/bin/env python3
"""Nivela los presets de fábrica: renderiza cada uno con atfs1render (sus demoNotes) y ajusta "volume"
para que todos queden parecidos (RMS de la parte que suena ≈ −18 dBFS, pico ≤ −1,5 dBFS).
Guarda los volúmenes en tools/levelsfs1.json, que presetsfs1.py aplica al regenerar.

    python3 tools/level_presets.py ../../build/plugins/atfs1/atfs1render_artefacts/Release/atfs1render
"""
import json, math, os, subprocess, sys, tempfile, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS = os.path.join(ROOT, "assets", "presets")
LEVELS = os.path.join(ROOT, "tools", "levelsfs1.json")
RENDER = os.path.abspath(sys.argv[1])
TARGET_RMS, MAX_PEAK = -18.0, -1.5
SKIP = {"Formant Scratch"}          # suena sólo con la rueda: copia el volumen de la voz de habla

def measure(path):
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "o.wav")
        subprocess.run([RENDER, path, out, "--secs", "8"], check=True, capture_output=True)
        with wave.open(out) as w:
            raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3)
    x = (raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16))
    x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    win = 2400
    frames = x[: len(x) // win * win].reshape(-1, win)
    rms = np.sqrt((frames ** 2).mean(axis=1))
    active = rms[rms > 10 ** (-50 / 20)]
    r = 20 * math.log10(np.sqrt((active ** 2).mean()) + 1e-9) if len(active) else -120.0
    return r, 20 * math.log10(np.abs(x).max() + 1e-9)

levels = json.load(open(LEVELS)) if os.path.exists(LEVELS) else {}
for f in sorted(os.listdir(PRESETS)):
    if not f.endswith(".json"): continue
    path = os.path.join(PRESETS, f)
    d = json.load(open(path))
    if d["name"] in SKIP: continue
    for it in range(4):
        d = json.load(open(path))
        rms, peak = measure(path)
        vol = d["params"].get("volume", 0.75)
        gain_db = min(TARGET_RMS - rms, MAX_PEAK - peak)
        if abs(gain_db) < 0.6: break
        new = min(1.0, max(0.15, vol * 10 ** (gain_db / 40)))        # ganancia ∝ volume²
        if abs(new - vol) < 0.004: break
        d["params"]["volume"] = round(new, 3)
        json.dump(d, open(path, "w"), indent=1)
    rms, peak = measure(path)
    levels[d["name"]] = d["params"].get("volume", 0.75)
    print(f"{d['name']:22s} vol {levels[d['name']]:.3f}  rms {rms:6.1f}  pico {peak:6.1f}")
if "Talking Formants" in levels:
    for n in SKIP: levels[n] = levels["Talking Formants"]
json.dump(levels, open(LEVELS, "w"), indent=1, sort_keys=True)
