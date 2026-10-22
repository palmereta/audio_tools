#!/usr/bin/env python3
"""Mide (y con --apply corrige) el nivel de los presets de fábrica con el mismo material que la QA en REAPER
(2026-10-22): 10 s, acorde C-E-G de 2 s, una escala corta, una nota larga y silencio para las colas. Pico y RMS
sobre los 10 s enteros, como `analyze.py` de la QA.

Sólo toca los que se salen (no nivela todo, para no cambiar el carácter de los presets que ya están bien):
  - pico por encima de -1,0 dBFS (el limitador suave del motor empieza en 0,9 y llega a 1,0): se baja hasta -1,5;
  - RMS más de 12 dB por debajo de la mediana: se sube hacia la mediana - 6 dB, sin pasar el pico de -1,5.
La ganancia se aplica con la perilla VOLUME (ganancia = 2·v², así que +6 dB como mucho). Lo que no alcanza (Voice,
Steam Locomotive y MS Hi-Hat estaban 30 dB abajo) se corrigió a mano con el filtro y la envolvente (1.0.7).

    python3 tools/level_presets.py <at20vrender> [--apply]
Los presets de audio in (vocoder, guitar synth, FX) no se miden: sin entrada están en silencio a propósito.
"""
import json, math, os, struct, subprocess, sys, tempfile, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS = os.path.join(ROOT, "assets", "presets")
RENDER = os.path.abspath(sys.argv[1])
APPLY = "--apply" in sys.argv
MAX_PEAK, PEAK_TARGET, QUIET_BELOW = -1.0, -1.5, 12.0

# Material de la QA (harness/material.lua, M.synth): (inicio, duración, nota, velocidad).
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

def read24(path):
    with wave.open(path) as w:
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3)
    x = raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16)
    return np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)

def measure(path, mid):
    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "o.wav")
        subprocess.run([RENDER, path, out, "--midi", mid, "--secs", "10"], check=True, capture_output=True)
        x = read24(out)
    db = lambda v: 20 * math.log10(v + 1e-12)
    return db(np.abs(x).max()), db(math.sqrt(np.mean(x ** 2))), int((np.abs(x) >= 0.999).sum())

def needs_audio(d):
    return d.get("category") in ("Vocoder", "Effect") or d["name"].startswith("Guitar Synth")

def main():
    td = tempfile.mkdtemp()
    mid = os.path.join(td, "qa.mid"); write_midi(mid)
    files = sorted(f for f in os.listdir(PRESETS) if f.endswith(".json"))
    res = {}
    for f in files:
        d = json.load(open(os.path.join(PRESETS, f)))
        if not needs_audio(d): res[f] = measure(os.path.join(PRESETS, f), mid)
    med = float(np.median([r[1] for r in res.values()]))
    print(f"mediana RMS {med:.1f} dBFS ({len(res)} presets sin audio in)")
    for f in files:
        if f not in res: continue
        path = os.path.join(PRESETS, f)
        peak, rms, overs = res[f]
        want = 0.0
        if peak > MAX_PEAK: want = PEAK_TARGET - peak
        elif rms < med - QUIET_BELOW: want = min(med - 6.0 - rms, PEAK_TARGET - peak)
        tag, peak2, rms2, overs2 = "", peak, rms, overs
        if APPLY and abs(want) > 0.1:
            for _ in range(4):                     # el limitador y las colas no son lineales: se itera
                d = json.load(open(path))
                vol = d["knobs"]["volume"]
                new_vol = min(1.0, vol * 10 ** (want / 40))
                if abs(new_vol - vol) < 0.002: break
                d["knobs"]["volume"] = round(new_vol, 3)
                open(path, "w").write(json.dumps(d, indent=1, ensure_ascii=False))
                peak2, rms2, overs2 = measure(path, mid)
                want = 0.0
                if peak2 > MAX_PEAK: want = PEAK_TARGET - peak2
                elif rms2 < med - QUIET_BELOW: want = min(med - 6.0 - rms2, PEAK_TARGET - peak2)
                if abs(want) <= 0.1: break
            tag = f"  ->  pico {peak2:6.1f}  rms {rms2:6.1f}  >=0.999 {overs2}"
        flag = "HOT  " if peak > MAX_PEAK else ("QUIET" if rms < med - QUIET_BELOW else "     ")
        print(f"{flag} {f:32s} pico {peak:6.1f}  rms {rms:6.1f}  >=0.999 {overs:6d}{tag}")

main()
