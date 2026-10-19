#!/usr/bin/env python3
"""Presets de fábrica de Plumb: escribe assets/presets/pNN_nombre.json.

Un preset fija el carácter de la corrección (Amount, Speed, Style, el carácter propio, formantes, el modo de
latencia cuando importa) y nunca la tonalidad, la escala ni la referencia: esos son de la canción.

    python3 plugins/plumb/tools/presets.py
"""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "presets")
P = []

def preset(cat, name, desc, **params):
    P.append({"name": name, "category": cat, "description": desc, "order": len(P), "params": params})

N = "Natural"
preset(N, "Natural Lead", "A lead vocal kept alive: the note centre is corrected, vibrato and scoops stay.", amount=100, speed=20, style=20)
preset(N, "Gentle Polish", "Barely there: only notes that drift a little are nudged, slowly.", amount=70, speed=60, style=10)
preset(N, "Pop Lead", "Modern pop: tighter, faster, still with the vibrato.", amount=100, speed=12, style=45)
preset(N, "Ballad", "Long notes breathe: humanize keeps sustained notes loose, starts are corrected.", amount=100, speed=30, style=15,
       custom_character=1, transition=80, humanize=70, flex=70, hysteresis=15, keep_vibrato=100, keep_drift=40)
preset(N, "Backing Vocals", "Stacks and doubles sit together: firm correction, a little vibrato.", amount=100, speed=10, style=60)
preset(N, "Choir Section", "Many takes of one part: every note on centre, the vibrato kept.", amount=100, speed=8, style=40,
       custom_character=1, transition=40, humanize=20, flex=90, hysteresis=10, keep_vibrato=80, keep_drift=0)

H = "Hard"
preset(H, "Hard Tune", "The effect: every note exactly on pitch, instant steps.", amount=100, speed=0, style=100)
preset(H, "Hard Tune, Live", "The effect for the stage: Live mode, 0.7 ms.", amount=100, speed=0, style=100, mode=0)
preset(H, "Soft Robot", "Hard steps with a short glide between notes.", amount=100, speed=6, style=95)
preset(H, "Stepped Glide", "Hard on the notes, a slow slide from one to the next.", amount=100, speed=0, style=80,
       custom_character=1, transition=120, humanize=0, flex=200, hysteresis=0, keep_vibrato=0, keep_drift=0)
preset(H, "Warble", "Hard tune with no hysteresis: vibrato flips between notes on purpose.", amount=100, speed=0, style=100,
       custom_character=1, transition=0, humanize=0, flex=200, hysteresis=0, keep_vibrato=0, keep_drift=0)

L = "Live"
preset(L, "Stage Natural", "For in-ear monitoring: Live mode with a natural correction.", amount=90, speed=25, style=20, mode=0)
preset(L, "Stage Firm", "Live mode, firmer: for a loud stage.", amount=100, speed=12, style=50, mode=0)
preset(L, "Tracking", "The default for recording: Tight mode, natural, fast enough to sing with.", amount=100, speed=18, style=25, mode=1)

V = "Voice"
preset(V, "Low Voice", "Bass and baritone: the detector listens from 65 Hz.", amount=100, speed=22, style=25, voice=0)
preset(V, "High Voice", "Soprano and high tenor: the detector listens up to 1.4 kHz.", amount=100, speed=18, style=25, voice=2)
preset(V, "Rap Pitch Lock", "Spoken and rapped lines keep their inflection; only held notes are caught.", amount=60, speed=40, style=10,
       custom_character=1, transition=60, humanize=80, flex=40, hysteresis=25, keep_vibrato=100, keep_drift=80)
preset(V, "Instrument", "Monophonic instruments (sax, violin, bass): the widest range, natural.", amount=100, speed=25, style=20, voice=3)

C = "Character"
preset(C, "Throat Down", "A longer vocal tract: darker, bigger, the pitch unchanged.", amount=100, speed=20, style=25, throat=-3)
preset(C, "Throat Up", "A shorter vocal tract: brighter, younger, the pitch unchanged.", amount=100, speed=20, style=25, throat=3)
preset(C, "Octave Down", "One octave down with the formants kept: a second, deeper voice.", amount=100, speed=20, style=30, transpose=-12)
preset(C, "Octave Up", "One octave up with the formants kept.", amount=100, speed=20, style=30, transpose=12)
preset(C, "Chipmunk", "Up a fifth with the formants following the pitch.", amount=100, speed=10, style=50, transpose=7, formants=0)
preset(C, "Added Vibrato", "A steady synthetic vibrato on every held note.", amount=100, speed=0, style=100, vib_depth=35, vib_rate=5.5, vib_delay=180)
preset(C, "MIDI Note Lock", "The note you hold on a MIDI keyboard is the target.", amount=100, speed=8, style=70, midi_mode=2)

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, p in enumerate(P):
        slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
        json.dump(p, open(os.path.join(OUT, f"p{i:02d}_{slug}.json"), "w"), indent=1)
    print(f"{len(P)} presets en {os.path.relpath(OUT)}")

if __name__ == "__main__":
    main()
