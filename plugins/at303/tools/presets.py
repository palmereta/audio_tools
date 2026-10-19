#!/usr/bin/env python3
"""Presets de fábrica del AT303: escribe assets/presets/pNN_nombre.json.

Cada preset trae el sonido (sólo lo que difiere de los valores por defecto, Source/Params303.h) y un patrón propio,
escrito como en el original: la lista de alturas por orden y la lista de tiempos por paso. Todas las líneas son
originales; ninguna copia un tema conocido.

Alturas: "nota[u][d][a][s]" separadas por espacios, nota 0-12 (C a C de arriba), u/d = octava arriba/abajo,
a = acento, s = slide. Tiempos: N = nota, T = ligadura (sigue la anterior), R = silencio.

    python3 plugins/at303/tools/presets.py
"""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "presets")

P = []
def preset(cat, name, desc, pitch, time, tempo=125, length=16, triplet=False, **params):
    toks = pitch.split()
    assert len(toks) <= 16 and all(re.fullmatch(r"(1[0-2]|[0-9])[udas]*", t) for t in toks), (name, pitch)
    assert re.fullmatch(r"[NTR]{1,16}", time), (name, time)
    params["tempo"] = tempo
    P.append({"name": name, "category": cat, "description": desc, "order": len(P),
              "params": params,
              "pattern": {"pitch": " ".join(toks + ["0"] * (16 - len(toks))), "time": time.ljust(16, "R"),
                          "length": length, "triplet": triplet}})

# ── Clásicos: el circuito de fábrica ──
C = "Classic"
preset(C, "First Squelch", "The default voice with a simple line: resonance high, envelope at half, one accent per beat.",
       "0a 0 12s 0 3a 0 7 0 0a 0 10s 7 5a 3 0 0", "NNNNNNNNNNNNNNNN", cutoff=40, resonance=72, envmod=55, decay=45, accent=65)
preset(C, "Rubber Root", "One note and its octave; all the movement is the filter and the accent capacitor.",
       "0 0 12u 0 0a 0 12a 0 0 0a 12s 0 0a 0 12 0", "NNNNNNNNNNNNNNNN", cutoff=30, resonance=80, envmod=65, decay=55, accent=75)
preset(C, "Sliding Fifths", "Fifths and octaves tied with slides: the 22 ms glide of the original circuit.",
       "0 7s 12 7 0a 5s 7 0 3 7s 10 7a", "NNTNNNTNNNNNRNNR", cutoff=45, resonance=70, envmod=50, decay=50, accent=55)
preset(C, "Square Pulse", "The square wave: hollower, with the waveshaper's soft corners.",
       "0a 0 3 0 5 0 3s 0 0a 0 7 5 3 0", "NNNNNNNNNNNNNNRR", waveform=1, cutoff=38, resonance=66, envmod=58, decay=40, accent=60)
preset(C, "Accent Climb", "Four accents in a row: hear the fourth scream as C13 charges.",
       "0a 0a 0a 0a 0 0 0 0 3a 3a 3a 3a 0 0 0 0", "NNNNNNNNNNNNNNNN", cutoff=35, resonance=85, envmod=45, decay=40, accent=90)
preset(C, "Low Decay Chug", "Decay at minimum: every note is a short pluck.",
       "0 0 0 12 0 0 3 0 0 0 5 0 3 0 12d 0", "NNRNNRNNNRNNNRNN", cutoff=25, resonance=55, envmod=70, decay=0, accent=50)
preset(C, "Long Sweep", "Decay at maximum and Env Mod up: slow, wide filter sweeps.",
       "0a 3 7 10 12 10 7 3", "NTTTNTTTNTTTNTTT", tempo=118, cutoff=20, resonance=70, envmod=85, decay=100, accent=40)
preset(C, "Minor Walk", "A walking minor line, no accents: the plain voice.",
       "0 3 5 7 8 7 5 3 0 3 5 7 10 7 5 3", "NNNNNNNNNNNNNNNN", cutoff=50, resonance=45, envmod=40, decay=35, accent=0)

# ── Ácido: resonancia alta y mucho movimiento ──
A = "Acid"
preset(A, "Squelch Line", "Resonance near the top, accents and slides off the beat.",
       "0 12a 0 3s 5 0 12 10a 0 7s 0 3a 12 0 5 3", "NNNNNNNNNNNNNNNN", tempo=132, cutoff=42, resonance=88, envmod=62, decay=48, accent=78)
preset(A, "Night Bus", "A rolling line with ties that let the slides breathe.",
       "0 0a 10d 0 3s 0 7 5a 3 0", "NNTNNTNNNTNNRNNT", tempo=128, cutoff=36, resonance=82, envmod=58, decay=60, accent=70)
preset(A, "Wet Concrete", "Dark and wide: low cutoff, long decay, accents that pop through.",
       "0 0 0a 12 0 0 3a 0 0 0 10a 0 0 5 0 3", "NNNNNNNNNNNNNNNN", tempo=126, cutoff=18, resonance=86, envmod=72, decay=70, accent=82)
preset(A, "Glass Eel", "High and quick, with slides on every other note.",
       "12 7s 12u 10 7s 12 3s 7 12 5s 10 7 12u 10s 7 3", "NNNNNNNNNNNNNNNN", tempo=136, cutoff=55, resonance=90, envmod=50, decay=30, accent=60)
preset(A, "Twelve O'Clock", "Every knob at noon. A good place to start turning.",
       "0 3 0 7a 0 12s 10 0 0 3 0 5a 0 12 7s 0", "NNNNNNNNNNNNNNNN", cutoff=50, resonance=50, envmod=50, decay=50, accent=50)
preset(A, "Pressure Drop", "The filter opens across the bar as the accents stack up.",
       "0 0 0 0a 0 0 0a 0 0 0a 0 0a 0a 0a 0a 0a", "NNNNNNNNNNNNNNNN", tempo=130, cutoff=28, resonance=84, envmod=60, decay=42, accent=95)
preset(A, "Bubble Square", "The square wave with high resonance: rounder bubbles.",
       "0 12 7 12a 0 10s 12 5 0 12 3s 7a 0 12 10 7", "NNNNNNNNNNNNNNNN", waveform=1, tempo=128, cutoff=40, resonance=84, envmod=60, decay=38, accent=70)
preset(A, "Off The Grid", "Rests in odd places: the line pushes and pulls against the beat.",
       "0a 3 12s 0 7 5a 0 10", "NRNNRNTNRNNRNRNN", tempo=124, cutoff=44, resonance=80, envmod=55, decay=52, accent=72)

# ── Graves: hondos, para sostener un tema ──
B = "Bass"
preset(B, "Deep Root", "Low and round: little resonance, the octave down, no slides.",
       "0d 0d 0d 0 0d 0d 3d 0d", "NTRNNTRNNTRNNTNR", tempo=122, cutoff=22, resonance=25, envmod=30, decay=35, accent=30, drive=10)
preset(B, "Dub Pulse", "Sparse notes with the dotted delay: room to breathe.",
       "0d 0 7d 5d", "NTTRRRNRNTTRRNRR", tempo=118, cutoff=30, resonance=40, envmod=35, decay=55, accent=35, delay_mix=30, delay_time=2, delay_fb=45)
preset(B, "Sub Ladder", "Full bass mod on: the ladder keeps its low end at high resonance.",
       "0d 0d 12d 0d 3d 0d 7d 5d", "NNNNNNNNNNNNNNNN", tempo=124, cutoff=26, resonance=70, envmod=40, decay=40, accent=40, bass_fix=1)
preset(B, "Funk Thumb", "Syncopated sixteenths with octave jumps.",
       "0 12 0 10 7 0 12a 5", "NRNNRNNRNRNNRNNR", tempo=112, cutoff=38, resonance=50, envmod=45, decay=30, accent=55)
preset(B, "Tied Drone", "One long note per bar, tied, with the filter breathing on Env Mod.",
       "0d 3d", "NTTTTTTTNTTTTTTT", tempo=120, cutoff=24, resonance=72, envmod=68, decay=90, accent=20)
preset(B, "Pocket", "A tight two-bar feel squeezed into one: rests on the one and the three.",
       "0 0 3 0 5 3 0 0 7 5", "RNNNRNNNRNNNRNNN", tempo=120, cutoff=34, resonance=48, envmod=42, decay=28, accent=45)

# ── Distorsión: la salida empujada ──
D = "Driven"
preset(D, "Hot Box", "Soft clip into the delay: thick, compressed and warm.",
       "0 12a 0 3s 0 7 12 0 0a 10 0 5s 3 0 12 7", "NNNNNNNNNNNNNNNN", tempo=134, cutoff=40, resonance=82, envmod=60, decay=45, accent=75, drive_type=1, drive=70)
preset(D, "Broken Speaker", "Hard clip with the tone low: a small speaker pushed too far.",
       "0 0 0a 0 3 0 0 12s 0 0 7a 0 5 0 3 0", "NNNNNNNNNNNNNNNN", tempo=138, cutoff=36, resonance=78, envmod=58, decay=40, accent=80, drive_type=2, drive=80, drive_tone=30)
preset(D, "Fuzz Worm", "Fuzz after the filter: the resonance turns into a buzzing worm.",
       "0 3s 5 7s 10 12s 10 7s", "NNNNNNNNNNNNNNNN", tempo=128, cutoff=30, resonance=88, envmod=50, decay=55, accent=60, drive_type=3, drive=65, drive_tone=55)
preset(D, "Overdriven Ladder", "The extended overdrive: the ladder itself driven harder, before any output stage.",
       "0a 0 12 0 0 3a 0 10 0 0a 7 0 5 0 3a 0", "NNNNNNNNNNNNNNNN", tempo=130, cutoff=42, resonance=80, envmod=60, decay=44, accent=78, overdrive=70, drive=20)

# ── Tresillos ──
T = "Triplets"
preset(T, "Triplet Roll", "Three steps per beat: a rolling twelve-step loop.",
       "0 0 12a 0 3 7s 0 0 10a 0 5 3s", "NNNNNNNNNNNN", tempo=120, length=12, triplet=True, cutoff=40, resonance=80, envmod=58, decay=46, accent=70)
preset(T, "Shuffle Gate", "Triplets with rests: a swung, gated line.",
       "0 12 7a 3 0 10s", "NRNNRNNRNNRN", tempo=116, length=12, triplet=True, cutoff=46, resonance=74, envmod=52, decay=34, accent=62)
preset(T, "Six Against Four", "Fifteen triplet steps over a four-beat bar: the line drifts across the beat.",
       "0a 3 7 12s 7 3 0 5a 10 12s 10 5 0 7 3", "NNNNNNNNNNNNNNN", tempo=124, length=15, triplet=True, cutoff=42, resonance=82, envmod=56, decay=40, accent=72)

# ── Extendido: las modificaciones clásicas del circuito ──
E = "Extended"
preset(E, "Long Accent", "Accent decay stretched: accented notes sweep as slowly as normal ones.",
       "0a 0 0 12 0a 0 3 0 0a 0 7 0 0a 10 5 3", "NNNNNNNNNNNNNNNN", tempo=126, cutoff=34, resonance=80, envmod=60, decay=60, accent=72, accent_decay=900)
preset(E, "Screamer", "High resonance mod, cutoff top raised and FM from the oscillator: a much wilder filter.",
       "0 12a 0 7s 0 10 12a 3 0 12 5s 0 7a 0 12 10", "NNNNNNNNNNNNNNNN", tempo=134, cutoff=45, resonance=78, envmod=55, decay=40, accent=70, high_res=1, cutoff_top=4200, filter_fm=35)
preset(E, "Soft Attack", "The VCA click softened and a longer slide: a smoother, rounder line.",
       "0 3s 7 10s 12 10s 7 3s", "NNTNNTNNNTNNNTNN", tempo=118, cutoff=40, resonance=66, envmod=50, decay=55, accent=40, soft_attack=12, vca_click=40, slide_time=180)
preset(E, "Tracking Lead", "Filter tracking at full: the tone follows the keys, so it plays as a lead.",
       "12 3u 7u 3u 12u 7u 10u 12u", "NNNNNNNNNNNNNNNN", tempo=128, cutoff=36, resonance=70, envmod=40, decay=40, accent=55, tracking=100)
preset(E, "Muffled Box", "The muffler on hard: the VCA rounds every edge.",
       "0 0a 3 0 7 0a 5 3 0 0a 10 0 12 7a 5 3", "NNNNNNNNNNNNNNNN", tempo=124, cutoff=48, resonance=76, envmod=56, decay=42, accent=68, muffler=2)

assert len(P) == 34, len(P)

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
