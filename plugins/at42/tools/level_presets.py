#!/usr/bin/env python3
"""Nivela los presets de fábrica: renderiza cada uno con at42render y ajusta MASTER para que
todos queden parecidos (RMS de la parte que suena ≈ −17 dBFS, pico ≤ −1,5 dBFS).

    python3 tools/level_presets.py ../../build/plugins/at101v/at42render_artefacts/Release/at42render
"""
import json, math, os, subprocess, sys, tempfile, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS = os.path.join(ROOT, "assets", "presets")
RENDER = os.path.abspath(sys.argv[1])
TARGET_RMS, MAX_PEAK = -17.0, -1.5

def measure(path):
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "o.wav")
        subprocess.run([RENDER, path, out, "--secs", "10"], check=True, capture_output=True)
        with wave.open(out) as w:
            raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3)
    x = (raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16))
    x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    # RMS sobre ventanas de 50 ms que suenan (descarta silencios entre notas).
    win = 2400
    frames = x[: len(x) // win * win].reshape(-1, win)
    rms = np.sqrt((frames ** 2).mean(axis=1))
    active = rms[rms > 10 ** (-50 / 20)]
    r = 20 * math.log10(np.sqrt((active ** 2).mean()) + 1e-9) if len(active) else -120.0
    return r, 20 * math.log10(np.abs(x).max() + 1e-9)

for f in sorted(os.listdir(PRESETS)):
    if not f.endswith(".json"): continue
    path = os.path.join(PRESETS, f)
    for it in range(3):
        d = json.load(open(path))
        rms, peak = measure(path)
        # Sin entrada de audio no suena (Piezo Percussion): se deja como está (antes lo subía a MASTER 10).
        if rms < -100.0: break
        vol = d["panel"].get("master", 0.75)
        # Ganancia ∝ volume²: cuánto mover el slider para llegar al objetivo.
        gain_db = min(TARGET_RMS - rms, MAX_PEAK - peak)
        if abs(gain_db) < 0.6: break
        new = min(1.0, max(0.2, vol * 10 ** (gain_db / 40)))
        if abs(new - vol) < 0.005: break
        d["panel"]["master"] = round(new, 3)
        json.dump(d, open(path, "w"), indent=1)
    rms, peak = measure(path)
    print(f"{d['name']:22s} vol {d["panel"].get("master", 0.75)*10:4.1f}  rms {rms:6.1f}  pico {peak:6.1f}")
