#!/usr/bin/env python3
"""Listen-check by analysis of the finished Valence demos: spectral balance by band, crest factor, stereo, mono low
end, and the fusions (every layer on its step 0 at the same instant) where a demo claims them.

    python plugins/valence/tools/demos/analyze.py OUTDIR
"""
import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vdlib import *

BANDS = [("sub", 20, 60), ("low", 60, 250), ("lowmid", 250, 800), ("mid", 800, 2500), ("presence", 2500, 6000), ("air", 6000, 16000)]

def bands(x):
    mono = x.mean(axis=1); n = 1 << 16; hop = n
    acc = np.zeros(n // 2 + 1)
    for i in range(0, len(mono) - n, hop): acc += np.abs(np.fft.rfft(mono[i:i + n] * np.hanning(n))) ** 2
    f = np.fft.rfftfreq(n, 1 / SR); tot = acc[(f >= 20) & (f < 16000)].sum()
    return {k: round(10 * np.log10(acc[(f >= a) & (f < b)].sum() / tot), 1) for k, a, b in BANDS}

def stereo(x):
    lo = lp(x, 120, 4); side = (lo[:, 0] - lo[:, 1]) / 2; mid = (lo[:, 0] + lo[:, 1]) / 2
    corr = float(np.corrcoef(x[:, 0], x[:, 1])[0, 1])
    return {"lr_correlation": round(corr, 3), "low_side_to_mid_db": round(20 * np.log10(np.sqrt(np.mean(side ** 2)) / max(np.sqrt(np.mean(mid ** 2)), 1e-12)), 1)}

def fusions(wd, bpm, bars):
    """For each claimed fusion bar: the layers with a note exactly on its first tick."""
    notes = json.load(open(os.path.join(wd, "notes.json")))["notes"]
    layers = sorted({e["layer"] for e in notes})
    out = {}
    for b in bars:
        t = b * 4
        out[b] = sorted({e["layer"] for e in notes if abs(e["beat"] - t) < 1e-3})
    eng = json.load(open(os.path.join(wd, "notes.json"))).get("fusions")
    res = {"layers": layers, "on_first_tick": out}
    if eng is not None: res["engine_fusion_bars"] = [round(f["beat"] / 4, 3) for f in eng]
    return res

def main():
    out = sys.argv[1]
    idx = json.load(open(os.path.join(out, "index.json")))
    for name, info in idx.items():
        y = load(os.path.join(out, name, name + ".wav"))
        crest = peak_db(y) - 10 * np.log10(np.mean(y ** 2))
        print(f"{name}: bands {bands(y)} crest {crest:.1f} dB {stereo(y)}")
        if "fusions_bars" in info: print("   fusions", fusions(os.path.join(out, name), info["bpm"], info["fusions_bars"]))
        for sec, s in info.get("sections", {}).items():
            print(f"   {sec} fusions", fusions(os.path.join(out, name, sec), s["bpm"], s["fusions_bars"]))

if __name__ == "__main__":
    main()
