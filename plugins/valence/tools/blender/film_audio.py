#!/usr/bin/env python3
"""Audio of the Valence film: the cuts of tools/film.json ("edit") taken from the main demo and joined with a 20 ms
equal-power crossfade centred on each cut, so the film keeps the demo's timing to the sample. scripts/film.py calls it
(audio.cmd) and brings the result to -16 LUFS in two passes.

    plugins/valence/tools/blender/film_audio.py <out.wav>

The demo is read from ../audiotools-private/valence/demos/ next to the repo (VALENCE_DEMOS overrides it)."""
import json, os, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
XF = 0.02


def main():
    out = sys.argv[1]
    ed = json.load(open(os.path.join(ROOT, "plugins", "valence", "tools", "film.json")))["edit"]
    demos = os.environ.get("VALENCE_DEMOS", os.path.join(ROOT, "..", "audiotools-private", "valence", "demos"))
    src = os.path.join(demos, ed["demo"])
    cuts = ed["cuts"]
    cmd, fc = ["ffmpeg", "-y", "-loglevel", "error"], []
    for i, (g0, g1) in enumerate(cuts):
        a = g0 - (XF / 2 if i > 0 else 0); b = g1 + (XF / 2 if i < len(cuts) - 1 else 0)
        cmd += ["-ss", f"{a:.6f}", "-t", f"{b - a:.6f}", "-i", src]
    last = "0:a"
    for i in range(1, len(cuts)):
        fc.append(f"[{last}][{i}:a]acrossfade=d={XF}:c1=qsin:c2=qsin[x{i}]"); last = f"x{i}"
    fc.append(f"[{last}]aresample=48000[a]")
    subprocess.run(cmd + ["-filter_complex", ";".join(fc), "-map", "[a]", "-c:a", "pcm_s24le", out], check=True)


if __name__ == "__main__":
    main()
