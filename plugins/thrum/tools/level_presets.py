#!/usr/bin/env python3
"""Nivelar los presets de Thrum: cada uno toca su frase de demostración (thrumrender --preset, 4 compases) y se mide su
sonoridad integrada (BS.1770) y su pico; el nivel de salida (out_level) que lo lleva a −20 LUFS sin pasar de −3 dBFS de
pico queda en tools/preset_levels.json, que presets.py suma a cada preset.

    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/thrum/tools/level_presets.py [build-dir]

Se corre después de presets.py y de compilar thrumrender; se repite (presets.py → compilar → level) hasta que no cambie.
"""
import json, os, subprocess, sys, tempfile
import numpy as np, soundfile as sf, pyloudnorm as pyln

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
BUILD = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build-guitar")
R = os.path.join(BUILD, "plugins", "thrum", "thrumrender_artefacts", "Release", "thrumrender")
TARGET, PEAK = -20.0, -3.0
LEVELS = os.path.join(HERE, "preset_levels.json")

def main():
    pdir = os.path.join(HERE, "..", "assets", "presets")
    presets = [json.load(open(os.path.join(pdir, f))) for f in sorted(os.listdir(pdir)) if f.endswith(".json")]
    names = [p["name"] for p in presets]
    # El nivel actual es el que tiene el preset compilado (assets), no el del último json: así repetir no corrige dos veces.
    old = {p["name"]: float(p["params"].get("out_level", 0.0)) for p in presets}
    out = {}
    with tempfile.TemporaryDirectory() as tmp:
        for n in names:
            wav = os.path.join(tmp, "x.wav")
            subprocess.run([R, wav, "--preset", n, "--bars", "4", "--tail", "1"], check=True, capture_output=True)
            x, sr = sf.read(wav, always_2d=True)
            lufs = pyln.Meter(sr).integrated_loudness(x)
            pk = 20 * np.log10(np.abs(x).max() + 1e-12)
            cur = old.get(n, 0.0)
            g = TARGET - lufs
            g = min(g, PEAK - pk)
            new = round(max(-24.0, min(12.0, cur + g)), 1)
            out[n] = new
            print(f"{n:30s} {lufs:6.1f} LUFS, pico {pk:5.1f} dBFS → out_level {new:+.1f} dB")
    json.dump(out, open(LEVELS, "w"), indent=1, ensure_ascii=False)
    print(f"{LEVELS}: {len(out)} niveles")

if __name__ == "__main__":
    main()
