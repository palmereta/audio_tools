#!/usr/bin/env python3
"""Nivela los presets de fábrica: renderiza cada uno con atmdrender y ajusta VOLUME para que todos queden
parecidos (RMS de la parte que suena ≈ −17 dBFS, pico ≤ −1,5 dBFS). Se corre después de presetsmd.py.

    python3 plugins/atmd/tools/level_presets.py build/plugins/atmd/atmdrender_artefacts/Release/atmdrender
"""
import json, math, os, subprocess, sys, tempfile, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS = os.path.join(ROOT, "assets", "presets")
RENDER = os.path.abspath(sys.argv[1])
TARGET_RMS, MAX_PEAK = -17.0, -1.5
K = 4.6     # el mismo potenciómetro logarítmico que audioTaper() del motor

def taper(d): return (math.exp(K * d / 10) - 1) / (math.exp(K) - 1)
def untaper(t): return 10 * math.log(t * (math.exp(K) - 1) + 1) / K

def measure(path):
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "o.wav")
        subprocess.run([RENDER, path, out, "--tail", "2"], check=True, capture_output=True)
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

for f in sorted(os.listdir(PRESETS)):
    if not f.endswith(".json"): continue
    path = os.path.join(PRESETS, f)
    for it in range(4):
        d = json.load(open(path))
        rms, peak = measure(path)
        vol = d["params"].get("volume", 6.0)
        gain_db = min(TARGET_RMS - rms, MAX_PEAK - peak)
        if abs(gain_db) < 0.5: break
        new = min(10.0, max(2.0, untaper(taper(vol) * 10 ** (gain_db / 20))))
        if abs(new - vol) < 0.01: break
        d["params"]["volume"] = round(new, 2)
        json.dump(d, open(path, "w"), indent=1)
    rms, peak = measure(path)
    print(f"{d['name']:22s} volume {d['params'].get('volume', 6.0):5.2f}  rms {rms:6.1f}  pico {peak:6.1f}")
