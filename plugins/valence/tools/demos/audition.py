#!/usr/bin/env python3
"""Audition candidate instrument presets for the Valence demos by analysis: each one plays the same short phrase and
the script prints loudness, peak, brightness, low-end share, decay (pluck or sustain), echo tail and width.

    VALENCE_BUILD=~/valence-demos/build python plugins/valence/tools/demos/audition.py OUTDIR [role ...]

roles: keys (a 1/16 arpeggio of A minor 7, then the chord held), bass (eighths on A1 and A2), stab (chord stabs).
The WAVs stay in OUTDIR for a listen.
"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vdlib import *

BPM = 124

def phrase(role):
    s = Song(BPM)
    if role == "keys":
        notes = [57, 60, 64, 67]
        for k in range(48): s.note(k * 0.25, 0.1, notes[k % 4] + (12 if (k // 4) % 2 else 0), 100 if k % 4 == 0 else 84)
        s.chord(12, 7.5, notes, 90)
    elif role == "bass":
        for k in range(32): s.note(k * 0.5, 0.3, 33 + (12 if k % 4 == 2 else 0) + (5 if k >= 16 else 0), 110 if k % 4 == 0 else 90)
    elif role == "stab":
        for k in range(8): s.chord(k * 1.5, 0.15, [57, 60, 64, 67], 100)
        s.chord(14, 0.2, [57, 60, 64, 67], 100)
    return s

CANDIDATES = {
    "keys": [("devicenti", "Glass Pluck", f) for f in (2, 3, 4)] + [("devicenti", "Rainy Pluck", f) for f in (2, 3, 4)]
            + [("devicenti", "Soft Chime", f) for f in (2, 3, 4)] + [("devicenti", "Vibe Mallet", f) for f in (2, 3, 4)]
            + [("devicenti", "Bamboo", 4), ("devicenti", "Sunday Organ", 2), ("devicenti", "Sunday Organ", 3), ("devicenti", "Sunday Organ", 4)]
            + [("atfs1", p, None) for p in ("glass_marimba", "formant_bells", "tine_piano", "formant_clav", "formant_organ")]
            + [("at700", p, None) for p in ("tine_keys", "clavi", "ring_bells", "phaser_pad", "ensemble_strings")]
            + [("at101v", p, None) for p in ("poly_pluck", "glass_bells", "poly_keys", "techno_stab")]
            + [("at20v", p, None) for p in ("reso_pluck", "ring_bells")]
            + [("at330", p, None) for p in ("Glass Hall", "Night Shift Pad", "Cold Horizon", "Dark Strings")],
    "bass": [("atmd", p, None) for p in ("fat_bass", "sub_drop", "plucked_bass", "rubber_bass", "moog_thump", "detuned_saws", "taurus_low")]
            + [("at101v", p, None) for p in ("101_bass", "pulse_bass", "rubber_bass", "sub_rumble", "minimal_pulse", "gate_bass")]
            + [("at303", p, None) for p in ("First Squelch", "Squelch Line", "Night Bus", "Wet Concrete", "Dub Pulse", "Deep Root", "Pressure Drop")]
            + [("atfs1", p, None) for p in ("fm_bass", "odd_sub_bass")] + [("at700", p, None) for p in ("700_bass", "sub_pulse")]
            + [("devicenti", "Concrete Sub", f) for f in (2, 3, 4)] + [("devicenti", "Iron Growl", f) for f in (2, 3)],
    "stab": [("devicenti", p, f) for p in ("Hammer Stab", "Short Horn", "Dub Throws", "Tape Chords") for f in (1, 2, 3, 4)]
            + [("at101v", "techno_stab", None), ("atmd", "organ_keys", None), ("at700", "clavi", None)],
}

def analyze(x, s):
    mono = x.mean(axis=1); n = len(mono)
    spec = np.abs(np.fft.rfft(mono * np.hanning(n))) ** 2; f = np.fft.rfftfreq(n, 1 / SR)
    cen = float((spec * f).sum() / max(spec.sum(), 1e-20)); low = float(spec[f < 120].sum() / max(spec.sum(), 1e-20))
    env = np.sqrt(np.convolve(mono ** 2, np.ones(480) / 480, mode="same"))
    on = [int(s.secs(b) * SR) for b, p, msg in s.ev if p == 2 and msg[2] > 0][:24]
    dec = [20 * np.log10(max(env[min(i + int(0.12 * SR), n - 1)], 1e-9) / max(env[i:i + 960].max(), 1e-9)) for i in on]
    end = int(s.secs(s.end()) * SR)
    tail = 20 * np.log10(max(env[min(end + int(0.5 * SR), n - 1)], 1e-9) / max(env[:end].max(), 1e-9))
    side = np.sqrt(np.mean(((x[:, 0] - x[:, 1]) / 2) ** 2)); mid = np.sqrt(np.mean(((x[:, 0] + x[:, 1]) / 2) ** 2))
    return lufs(x), peak_db(x), cen, low, float(np.median(dec)), tail, side / max(mid, 1e-9), float(np.abs(x.mean(axis=0)).max())

def main():
    out = sys.argv[1]; roles = sys.argv[2:] or list(CANDIDATES)
    os.makedirs(out, exist_ok=True)
    for role in roles:
        s = phrase(role); mid = s.write(os.path.join(out, f"_{role}.mid"))
        print(f"\n## {role}\n{'instrument':44s} LUFS   peak  centroid  low<120  decay120ms  tail+0.5s  side/mid  dc")
        for kind, preset, focus in CANDIDATES[role]:
            tag = f"{kind}:{preset}" + (f":t{focus}" if focus else "")
            wav = os.path.join(out, f"{role}_{tag.replace(':', '_').replace(' ', '_')}.wav")
            try:
                x = render(kind, mid, wav, preset, BPM, tail=2.0, focus=focus)
            except Exception as e:
                print(f"{tag:44s} FAILED {str(e).splitlines()[-1][:80]}"); continue
            l, p, c, lo, d, t, w, dc = analyze(x, s)
            print(f"{tag:44s} {l:6.1f} {p:6.1f} {c:8.0f} {lo:8.2f} {d:10.1f} {t:10.1f} {w:9.2f} {dc:.4f}")

if __name__ == "__main__":
    main()
