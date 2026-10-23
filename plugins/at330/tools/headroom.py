#!/usr/bin/env python3
"""Nivel de los presets del AT330: los deja parejos y con margen. Mide cada preset con dos materiales:
  - su progresión de demostración ("demo_chords"), un acorde por compás a 96 BPM;
  - el acorde de prueba de at330render (A menor de tres octavas, 6 s), el mismo tipo de material de at330test T16.
Los de vocoder llevan la voz sintética (tools/demo_voice.py) en la entrada.
Ajusta sólo el volumen del preset (volume, en tools/preset_levels.json): lleva el RMS hacia -16 dBFS sin que el pico
pase de -1,5 dBFS. Después hay que regenerar los presets (presets.py) y recompilar.

    python3 plugins/at330/tools/headroom.py <at330render> [--apply]
"""
import json, math, os, subprocess, sys, tempfile, wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PRESETS = os.path.join(HERE, "..", "assets", "presets")
LEVELS = os.path.join(HERE, "preset_levels.json")
RENDER = os.path.abspath(sys.argv[1])
APPLY = "--apply" in sys.argv
RMS_TARGET, PEAK_MAX = -16.0, -1.5


def read24(path):
    with wave.open(path) as w:
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).reshape(-1, 3)
    x = raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16)
    return np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)


def measure(d, volume, voice, td):
    peak, rms = -240.0, []
    for extra in (["--chords", d["demo_chords"], "--bpm", "96"], []):
        out = os.path.join(td, "o.wav")
        cmd = [RENDER, out, "--preset", d["name"], "--set", f"volume={volume}"] + extra
        if d.get("voice"): cmd += ["--voice", voice]
        subprocess.run(cmd, check=True, capture_output=True)
        x = read24(out)
        peak = max(peak, 20 * math.log10(np.abs(x).max() + 1e-12))
        # RMS de la parte que suena (sin la cola ni el silencio inicial).
        n = len(x) // 2
        body = x[int(0.3 * 48000) * 2: n * 2 - 3 * 48000 * 2] if n > 4 * 48000 else x
        rms.append(10 * math.log10(np.mean(body ** 2) + 1e-20))
    return peak, max(rms)


def main():
    td = tempfile.mkdtemp()
    voice = os.path.join(td, "voice.wav")
    subprocess.run([sys.executable, os.path.join(HERE, "demo_voice.py"), voice, "--script", "night"], check=True, capture_output=True)
    levels = json.load(open(LEVELS)) if os.path.exists(LEVELS) else {}
    for f in sorted(os.listdir(PRESETS)):
        if not f.endswith(".json"): continue
        d = json.load(open(os.path.join(PRESETS, f)))
        vol = float(d["params"].get("volume", 0.0))
        pk, rms = measure(d, vol, voice, td)
        v = vol
        if APPLY:
            for _ in range(6):                               # el ensemble y la reverb no son lineales en el pico: se itera
                want = min(RMS_TARGET - rms, PEAK_MAX - pk)
                if abs(want) < 0.3: break
                v = round(max(-36.0, min(6.0, v + want)), 1)
                pk, rms = measure(d, v, voice, td)
            levels[d["name"]] = v
        print(f"{d['name']:28s} volumen {vol:+5.1f} -> {v:+5.1f} dB  pico {pk:6.2f} dBFS  RMS {rms:6.1f} dBFS")
    if APPLY:
        json.dump(levels, open(LEVELS, "w"), indent=1, ensure_ascii=False)
        print("ahora: python3 plugins/at330/tools/presets.py y recompilar")


if __name__ == "__main__":
    main()
