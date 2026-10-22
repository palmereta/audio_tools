#!/usr/bin/env python3
"""Nivela los presets de fábrica: renderiza cada uno (8 compases con pallorrender), mide la sonoridad integrada
(BS.1770) y guarda en preset_levels.json el ajuste de volumen maestro que lo lleva a TARGET LUFS. presets.py lo aplica.

    python3 plugins/pallor/tools/presets.py && cmake --build build --target pallorrender
    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/pallor/tools/level_presets.py
    python3 plugins/pallor/tools/presets.py && cmake --build build --target Pallor_All pallorrender

Necesita numpy, soundfile y pyloudnorm.
"""
import glob, json, os, subprocess, tempfile
import numpy as np, soundfile as sf, pyloudnorm as pyln

TARGET = -18.0
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
RENDER = os.path.join(ROOT, "build", "plugins", "pallor", "pallorrender_artefacts", "Release", "pallorrender")
LEVELS = os.path.join(HERE, "preset_levels.json")

def main():
    old = json.load(open(LEVELS)) if os.path.exists(LEVELS) else {}
    out = {}
    with tempfile.TemporaryDirectory() as tmp:
        for f in sorted(glob.glob(os.path.join(HERE, "..", "assets", "presets", "*.json"))):
            name = json.load(open(f))["name"]
            wav = os.path.join(tmp, "p.wav")
            subprocess.run([RENDER, wav, "--preset", name, "--bars", "8", "--tail", "1"], check=True, capture_output=True)
            x, sr = sf.read(wav)
            lufs = pyln.Meter(sr).integrated_loudness(x)
            trim = round(old.get(name, 0.0) + TARGET - lufs, 1)
            out[name] = max(-24.0, min(6.0, trim))
            print(f"{name:30s} {lufs:6.1f} LUFS → volumen {out[name]:+.1f} dB")
    json.dump(out, open(LEVELS, "w"), indent=1, ensure_ascii=False)

if __name__ == "__main__":
    main()
