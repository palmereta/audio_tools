#!/usr/bin/env python3
"""Formant Sequences de fábrica del ATFS1 (assets/fseq/fNN_*.json).

Dos fuentes, ninguna con datos de Yamaha:
  - vocales sintéticas: formantes promedio de Peterson y Barney (1952), con deslizamientos entre vocales;
  - frases dichas por espeak-ng (síntesis de voz libre, corre en pc50-wsl) y analizadas por el ATFS1
    (altura por autocorrelación, formantes por LPC). Los WAV quedan fuera del repo; el Fseq es el análisis.

Uso: python3 tools/fseqlib.py [--tts carpeta_con_los_wav]
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(ROOT))
TOOL = os.path.join(REPO, "build", "plugins", "atfs1", "atfs1render_artefacts", "Release", "atfs1render")
OUT = os.path.join(ROOT, "assets", "fseq")
TTS = sys.argv[sys.argv.index("--tts") + 1] if "--tts" in sys.argv else "/tmp/claude-501/tts"

VOWELS = [
    # nombre, vocales, segundos por vocal, altura Hz, voz aguda, vibrato (semitonos), aliento
    ("Vowels AEIOU", "aeiou", 0.42, 110, False, 0.2, 0.0),
    ("Vowels High", "aeiou", 0.42, 220, True, 0.25, 0.0),
    ("Ooh Aah", "ua", 0.9, 98, False, 0.15, 0.0),
    ("Ee Oh", "io", 0.8, 130, False, 0.2, 0.0),
    ("Choir Ah", "oa", 1.2, 147, False, 0.3, 0.05),
    ("Choir Oo", "uo", 1.2, 196, True, 0.3, 0.05),
    ("Hum Mm", "ma", 1.0, 110, False, 0.1, 0.0),
    ("Yah Yah", "ya-ya-ya-ya-", 0.13, 123, False, 0.0, 0.0),
    ("Wah Wah", "uauauaua", 0.16, 110, False, 0.0, 0.0),
    ("Robot Talk", "aeoiuoea", 0.11, 98, False, 0.0, 0.0),
    ("Whisper Hey", "-e-i", 0.5, 130, False, 0.0, 1.0),
    ("Breathy Ah", "a", 1.5, 165, True, 0.3, 0.3),
]
PHRASES = [
    # nombre, archivo, lazo (inicio, fin, modo) o None
    ("Audio Tools", "audio_tools.wav", None),
    ("One Two", "one_two.wav", None),
    ("Formant", "formant.wav", None),
    ("Shoo Bee Doo", "shooby.wav", None),
    ("Uno Dos Tres", "uno_dos.wav", None),
    ("Yeah", "yeah.wav", None),
    ("Synthesizer", "synthesizer.wav", None),
    ("La La La", "lalala.wav", None),
    ("I Am A Robot", "robot.wav", None),
]

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    n = 1
    for name, txt, secs, hz, high, vib, breath in VOWELS:
        fn = os.path.join(OUT, f"f{n:02d}_{name.lower().replace(' ', '_')}.json")
        cmd = [TOOL, "--vowels", fn, name, txt, "--secs-per", str(secs), "--pitch", str(hz), "--vibrato", str(vib), "--breath", str(breath)]
        if high: cmd.append("--female")
        print(subprocess.run(cmd, capture_output=True, text=True).stdout.strip()); n += 1
    for name, wav, loop in PHRASES:
        src = os.path.join(TTS, wav)
        if not os.path.exists(src): print("falta", src); continue
        fn = os.path.join(OUT, f"f{n:02d}_{name.lower().replace(' ', '_')}.json")
        cmd = [TOOL, "--analyze", src, fn, name]
        if loop: cmd += ["--loop", str(loop[0]), str(loop[1]), loop[2]]
        print(subprocess.run(cmd, capture_output=True, text=True).stdout.strip()); n += 1

if __name__ == "__main__":
    main()
