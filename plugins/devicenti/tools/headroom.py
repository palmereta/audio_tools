#!/usr/bin/env python3
"""DeVicenti preset levels: balance the four tracks of every factory preset and check the mix headroom.

    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/devicenti/tools/headroom.py [--build build-devicenti] [--apply]

Renders each preset (8 bars, its song chain) with the four track outputs on, measures each track's loudness
(BS.1770) and computes the level change that brings it to its genre's target (the balance found by ear-proxy on the
demos: kit and bass forward in trip hop and heavy, pads forward in ambient, the drone forward in dark ambient). The
changes go to preset_levels.json, which presets.py adds to the track levels. With --apply it then regenerates the
presets. The full mix is measured too: integrated loudness and sample peak (the master's soft ceiling keeps it at or
below −0.5 dBFS; the goal is ≤ −1 dBFS before the LIMIT stage does much).
"""
import json, os, subprocess, sys, tempfile
import numpy as np, soundfile as sf, pyloudnorm as pyln

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
TARGET = {
    "Trip Hop": [-17.0, -16.5, -18.0, -21.0], "Downtempo": [-18.0, -17.5, -19.5, -20.0], "Acid Jazz": [-17.0, -18.0, -19.0, -22.0],
    "Ambient": [-30.0, -20.0, -13.0, -21.0], "Dark Ambient": [-24.0, -27.0, -21.0, -19.0], "Heavy": [-12.5, -12.0, -16.0, -20.0],
}


def main():
    build = sys.argv[sys.argv.index("--build") + 1] if "--build" in sys.argv else "build-devicenti"
    R = os.path.join(ROOT, build, "plugins", "devicenti", "devicentirender_artefacts", "Release", "devicentirender")
    pdir = os.path.join(HERE, "..", "assets", "presets")
    levels_path = os.path.join(HERE, "preset_levels.json")
    old = json.load(open(levels_path)) if os.path.exists(levels_path) else {}
    out = dict(old)
    meter = pyln.Meter(48000)
    tmp = tempfile.mkdtemp()
    rows = []
    for f in sorted(os.listdir(pdir)):
        p = json.load(open(os.path.join(pdir, f)))
        cat = p["category"]
        if cat not in TARGET:
            continue
        r = subprocess.run([R, os.path.join(tmp, "m.wav"), "--preset", p["name"], "--bars", "8", "--song", "--tail", "1", "--stems", tmp], capture_output=True, text=True)
        if r.returncode != 0:
            print("render failed", p["name"], r.stdout, r.stderr); continue
        adj = list(out.get(p["name"], [0.0, 0.0, 0.0, 0.0]))
        meas = []
        for k in range(4):
            x, sr = sf.read(os.path.join(tmp, f"track{k + 1}.wav"))
            L = meter.integrated_loudness(x)
            meas.append(L)
            if np.isfinite(L) and L > -70:
                adj[k] = float(np.clip(adj[k] + (TARGET[cat][k] - L), -15.0, 9.0))
        out[p["name"]] = [round(a, 1) for a in adj]
        rows.append((p["name"], cat, meas))
        print(f"{p['name']:22s} {cat:13s} " + "  ".join(f"T{k + 1} {m:6.1f}" for k, m in enumerate(meas)) + "   adj " + " ".join(f"{a:+5.1f}" for a in out[p['name']]))
    json.dump(out, open(levels_path, "w"), indent=1, sort_keys=True)
    if "--apply" in sys.argv:
        subprocess.run([sys.executable, os.path.join(HERE, "presets.py")], check=True)
    # Mix check after the change (only meaningful once the presets are rebuilt into the plugin).
    if "--check" in sys.argv:
        for f in sorted(os.listdir(pdir)):
            p = json.load(open(os.path.join(pdir, f)))
            subprocess.run([R, os.path.join(tmp, "m.wav"), "--preset", p["name"], "--bars", "8", "--song", "--tail", "1"], capture_output=True)
            x, sr = sf.read(os.path.join(tmp, "m.wav"))
            print(f"{p['name']:22s} mix {meter.integrated_loudness(x):6.1f} LUFS, peak {20 * np.log10(np.abs(x).max() + 1e-9):5.1f} dBFS")


if __name__ == "__main__":
    main()
