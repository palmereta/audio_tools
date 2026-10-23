#!/usr/bin/env python3
"""Deja margen en los presets que llegan a 0 dBFS (QA en REAPER 2026-10-22: 25 presets con el pico justo en 0 dBFS, el
techo suave del master). Mide cada preset con dos materiales:
  - su patrón, 8 compases a su tempo (lo que suena al cargarlo y darle Play);
  - el material de la QA en DAWs (harness/material.lua): el patrón a 120 BPM con, encima, cada sonido del mapa GM a
    velocidad 110 y un groove de bombo, caja y hi-hat (10 s).
Si el pico pasa de -1 dBFS baja sólo el volumen maestro (m_volume, en tools/preset_levels.json) hasta -1,5 dBFS de pico;
el resto no se toca: el carácter es el mismo, sólo que el techo deja de recortar. Después hay que regenerar los presets
(presets.py) y recompilar.

    python3 plugins/pallor/tools/headroom.py <pallorrender> [--apply]
"""
import json, math, os, struct, subprocess, sys, tempfile, wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PRESETS = os.path.join(HERE, "..", "assets", "presets")
LEVELS = os.path.join(HERE, "preset_levels.json")
RENDER = os.path.abspath(sys.argv[1])
APPLY = "--apply" in sys.argv
MAX_PEAK, PEAK_TARGET = -1.0, -1.5

# harness/material.lua de la QA en REAPER: M.set_drum_map(...) y el groove de 2 compases a 120.
DRUM_MAP = [36, 38, 42, 46, 39, 37, 49, 51, 41, 45, 48, 50, 56, 75, 54, 52]
def qa_notes():
    ev = [(0.25 * i, 0.1, n, 110) for i, n in enumerate(DRUM_MAP)]
    t0 = max(0.25 * len(DRUM_MAP), 4.0)
    for b in range(8):
        tb = t0 + b * 0.5
        if tb > 7.9: break
        ev.append((tb, 0.1, 36 if b % 2 == 0 else 38, 110)); ev.append((tb + 0.25, 0.1, 42, 80))
    return ev

def write_midi(path, notes):
    ppq, tempo = 960, 500000                       # 120 bpm: 1 s = 1920 ticks
    ev = []
    for t, d, n, v in notes:
        ev.append((round(t * 1920), 1, n, v)); ev.append((round((t + d) * 1920), 0, n, 0))
    ev.sort(key=lambda e: (e[0], e[1]))
    def vlq(x):
        b = [x & 0x7F]; x >>= 7
        while x: b.insert(0, (x & 0x7F) | 0x80); x >>= 7
        return bytes(b)
    trk, last = bytearray(b"\x00\xFF\x51\x03" + tempo.to_bytes(3, "big")), 0
    for tick, on, n, v in ev:
        trk += vlq(tick - last) + bytes([0x99 if on else 0x89, n, v]); last = tick
    trk += b"\x00\xFF\x2F\x00"
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, ppq) + b"MTrk" + struct.pack(">I", len(trk)) + trk)

def read24(path):
    with wave.open(path) as w:
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3)
    x = raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16)
    return np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)

def peak_db(name, volume, mid, td):
    worst = -240.0
    for extra in (["--bars", "8"], ["--midi", mid, "--with-pattern", "--bpm", "120", "--seconds", "10"]):
        out = os.path.join(td, "o.wav")
        subprocess.run([RENDER, out, "--preset", name, "--set", f"m_volume={volume}"] + extra, check=True, capture_output=True)
        worst = max(worst, 20 * math.log10(np.abs(read24(out)).max() + 1e-12))
    return worst

def main():
    td = tempfile.mkdtemp(); mid = os.path.join(td, "qa.mid"); write_midi(mid, qa_notes())
    levels = json.load(open(LEVELS))
    changed = 0
    for f in sorted(os.listdir(PRESETS)):
        if not f.endswith(".json"): continue
        d = json.load(open(os.path.join(PRESETS, f)))
        name = d["name"]; vol = float(d["params"].get("m_volume", 0.0))
        pk = peak_db(name, vol, mid, td); first = pk; v = vol
        if pk > MAX_PEAK and APPLY:
            for _ in range(8):                         # el techo no es lineal: se itera
                v = round(v - (pk - PEAK_TARGET) - 0.05, 1)
                pk = peak_db(name, v, mid, td)
                if pk <= MAX_PEAK: break
            levels[name] = v; changed += 1
        print(f"{name:28s} volumen {vol:+5.1f} dB  pico {first:6.2f} dBFS" + (f"  ->  {v:+5.1f} dB, pico {pk:6.2f}" if v != vol else ""))
    if APPLY and changed:
        json.dump(levels, open(LEVELS, "w"), indent=1, ensure_ascii=False)
        print(f"{changed} presets con menos volumen; ahora: python3 plugins/pallor/tools/presets.py")

if __name__ == "__main__":
    main()
