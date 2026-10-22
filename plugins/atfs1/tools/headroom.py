#!/usr/bin/env python3
"""Deja margen en los presets que llegan a 0 dBFS con el material MIDI de la QA en DAWs (2026-10-22: acorde de 2 s,
escala con velocidades distintas y una nota larga a 127, 10 s; harness/material.lua de la QA en REAPER). Sólo baja
"volume" de los que pasan de -1 dBFS de pico, hasta -1,5; el resto no se toca (el carácter no cambia, sólo el nivel).
Guarda el volumen también en tools/levelsfs1.json (presetsfs1.py lo aplica al regenerar).

    python3 tools/headroom.py <atfs1render> [--apply]
"""
import json, math, os, struct, subprocess, sys, tempfile, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS = os.path.join(ROOT, "assets", "presets")
LEVELS = os.path.join(ROOT, "tools", "levelsfs1.json")
RENDER = os.path.abspath(sys.argv[1])
APPLY = "--apply" in sys.argv
MAX_PEAK, PEAK_TARGET = -1.0, -1.5

SYNTH = [(0.0, 2.0, 48, 100), (0.0, 2.0, 52, 100), (0.0, 2.0, 55, 100),
         (2.2, 0.2, 60, 90), (2.45, 0.2, 62, 70), (2.7, 0.2, 64, 110), (2.95, 0.2, 65, 60),
         (3.2, 0.2, 67, 90), (3.45, 0.2, 69, 127), (3.7, 0.2, 71, 80), (3.95, 0.2, 72, 100),
         (4.5, 1.0, 57, 127)]

def write_midi(path):
    ppq, tempo = 960, 500000                       # 120 bpm: 1 s = 1920 ticks
    ev = []
    for t, d, n, v in SYNTH:
        ev.append((round(t * 1920), 1, n, v)); ev.append((round((t + d) * 1920), 0, n, 0))
    ev.sort(key=lambda e: (e[0], e[1]))
    def vlq(x):
        b = [x & 0x7F]; x >>= 7
        while x: b.insert(0, (x & 0x7F) | 0x80); x >>= 7
        return bytes(b)
    trk, last = bytearray(b"\x00\xFF\x51\x03" + tempo.to_bytes(3, "big")), 0
    for tick, on, n, v in ev:
        trk += vlq(tick - last) + bytes([0x90 if on else 0x80, n, v]); last = tick
    trk += b"\x00\xFF\x2F\x00"
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, ppq) + b"MTrk" + struct.pack(">I", len(trk)) + trk)

def measure(path, mid):
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "o.wav")
        subprocess.run([RENDER, path, out, "--midi", mid, "--secs", "10"], check=True, capture_output=True)
        with wave.open(out) as w:
            raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3)
    x = raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16)
    x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    return 20 * math.log10(np.abs(x).max() + 1e-12), 20 * math.log10(math.sqrt(np.mean(x ** 2)) + 1e-12)

td = tempfile.mkdtemp(); mid = os.path.join(td, "qa.mid"); write_midi(mid)
levels = json.load(open(LEVELS))
for f in sorted(os.listdir(PRESETS)):
    if not f.endswith(".json"): continue
    path = os.path.join(PRESETS, f)
    peak, rms = measure(path, mid)
    tag = ""
    if peak > MAX_PEAK and APPLY:
        for _ in range(5):                         # el limitador no es lineal: se itera
            d = json.load(open(path))
            vol = d["params"].get("volume", 0.75)
            new = round(vol * 10 ** ((PEAK_TARGET - peak) / 40), 3)   # ganancia ∝ volume²
            d["params"]["volume"] = new
            json.dump(d, open(path, "w"), indent=1)
            levels[d["name"]] = new
            p2, r2 = measure(path, mid)
            tag = f"  ->  vol {new:.3f}  pico {p2:6.1f}  rms {r2:6.1f}"
            peak, rms = p2, r2
            if peak <= MAX_PEAK: break
    print(f"{'HOT ' if tag or peak > MAX_PEAK else '    '}{f:28s} pico {peak:6.1f}  rms {rms:6.1f}{tag}")
if APPLY: json.dump(levels, open(LEVELS, "w"), indent=1, sort_keys=True)
