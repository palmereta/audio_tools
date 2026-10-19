#!/usr/bin/env python3
"""Presets de fábrica de Halation → assets/presets/*.json (los lee el plugin desde BinaryData).
Cada preset fija sólo lo que se aparta del valor por defecto. Divisiones: índice en ParamsSp.h (6 = 1/8D, 8 = 1/4). Length es un T60: 0.2 s · 300^(length/100); 100 = infinito."""
import json, os, re

D = {"1/32": 0, "1/16T": 1, "1/16": 2, "1/16D": 3, "1/8T": 4, "1/8": 5, "1/8D": 6, "1/4T": 7, "1/4": 8, "1/4D": 9, "1/2T": 10, "1/2": 11, "1/2D": 12, "1 bar": 13, "2 bars": 14, "4 bars": 15}
CH = {"digital": 0, "tape": 1, "bbd": 2, "spring": 3}
P = []
def p(cat, name, desc, scene_b=None, **kw):
    P.append(dict(name=name, category=cat, description=desc, params=kw, scene_b=scene_b))

# ── Clásicos ──
p("Classic", "Tape Echo 1974", "A worn tape echo: wobble, warm saturation and repeats that darken.", space=8, division=D["1/8D"], length=48, tone=45, character=CH["tape"], wear=35, mod_depth=40, mix=30, pingpong=0)
p("Classic", "Analog Bucket", "Dark bucket-brigade delay, the repeats sink into the mix.", space=5, division=D["1/4"], length=52, tone=35, character=CH["bbd"], mix=28, pingpong=0)
p("Classic", "Clean Digital Quarter", "A clean quarter-note delay, every repeat the same colour.", space=0, division=D["1/4"], length=40, tone=80, mix=25, pingpong=0, mod_depth=0)
p("Classic", "Ping-Pong Machine", "Dotted eighths bouncing left and right.", space=6, division=D["1/8D"], length=55, tone=65, mix=32, pingpong=1)
p("Classic", "Plate Vocal", "Bright, dense plate for vocals: short pre-delay, smooth tail.", space=100, room=2, size=100, length=40, tone=78, predelay=25, diffusion=90, mix=24, mod_depth=20)
p("Classic", "Small Room", "A wooden room around the sound, short and natural.", space=100, room=0, size=100, length=22, tone=55, diffusion=80, mix=22)
p("Classic", "Concert Hall", "A large hall with a long, warm tail.", space=100, room=1, size=150, length=55, tone=52, predelay=30, diffusion=85, mix=28, mod_depth=30)
p("Classic", "Spring Surf", "A drippy spring tank, like a guitar amp's reverb.", space=92, room=3, size=60, length=45, tone=60, character=CH["spring"], mix=30, diffusion=40)
# ── Ambient ──
p("Ambient", "Cathedral of Glass", "An enormous bright space with a slow octave shimmer.", space=100, size=320, length=85, tone=80, predelay=60, mix=45, on_shimmer=1, shimmer=35, interval=0, mod_depth=40)
p("Ambient", "Endless Pad", "A tail that never ends: play a chord and it stays.", space=100, size=250, length=100, tone=60, mix=50, mod_depth=45)
p("Ambient", "Breathing Room", "The walls move slowly in and out, the tail rises and falls like breath.", space=95, size=160, length=72, tone=58, mix=40, on_breathe=1, breathe_mode=0, breathe_depth=45, breathe_rate=0.08)
p("Ambient", "Rising Choir", "Each repeat climbs a fifth and an octave: a slowly rising choir.", space=88, size=200, length=80, tone=70, mix=45, on_shimmer=1, shimmer=55, interval=2)
p("Ambient", "Warm Blanket", "A soft, dark, wide space that wraps the sound.", space=100, room=3, size=180, length=62, tone=28, mix=40, width=140, mod_depth=35)
p("Ambient", "Slow Tide", "Long tape-coloured repeats that melt into a hall.", space=55, division=D["1/2"], length=78, tone=45, character=CH["tape"], wear=20, mix=40, mod_depth=55)
p("Ambient", "Tuned Abyss", "A deep tail that resonates on the notes you hold (MIDI) or on A minor.", space=100, size=260, length=82, tone=40, mix=45, on_resonator=1, resonance=55, chord=3, res_root=45)
p("Ambient", "Spectral Ice", "The tail freezes into a glassy, still cloud.", space=100, size=200, length=75, tone=82, mix=50, on_spectral=1, spectral=85)
# ── Rítmicos ──
p("Rhythmic", "Dotted Rain", "A pattern of repeats falling like rain, on the beat.", space=12, division=D["1/4"], pattern=4, length=50, tone=62, mix=32, pingpong=1)
p("Rhythmic", "Stutter Grid", "Sixteenth-note stutters that catch the end of a phrase.", space=15, division=D["1/8"], length=35, mix=35, on_stutter=1, stutter=45, stutter_div=D["1/16"])
p("Rhythmic", "Gallop", "A galloping two-tap pattern for picked guitars and synths.", space=5, division=D["1/4"], pattern=5, length=40, tone=65, mix=30)
p("Rhythmic", "Ducked Dub", "Dub delay that steps back while you play and blooms in the gaps.", space=20, division=D["1/4D"], length=72, tone=40, character=CH["tape"], wear=25, mix=40, on_duck=1, duck=65)
p("Rhythmic", "Gated 1984", "The big drum reverb that stops dead.", space=100, size=150, length=60, tone=65, mix=40, on_gate=1, gate=95, gate_hold=280)
p("Rhythmic", "Swing Taps", "Swung repeats that lean back into the groove.", space=10, division=D["1/4"], pattern=7, length=45, mix=30, pingpong=1)
# ── Experimentales ──
p("Experimental", "Ghost Before You", "Reversed swells that rise into each beat.", space=80, size=150, length=60, mix=50, on_reverse=1, reverse=85, reverse_div=D["1/4"])
p("Experimental", "Broken Satellite", "Grains and tape wear: a transmission breaking up in orbit.", space=60, division=D["1/8T"], length=68, character=CH["tape"], wear=80, mix=45, on_grains=1, grains=60, grain_ms=60)
p("Experimental", "Collapsing Hall", "While you play, the hall closes in and the tail climbs.", space=90, size=260, length=70, mix=45, on_breathe=1, breathe_mode=1, breathe_depth=70, breathe_rate=0.25)
p("Experimental", "Self-Oscillation", "Feedback at the edge: the repeats feed themselves (the limiter keeps it safe).", space=25, division=D["1/4D"], length=100, tone=50, character=CH["tape"], wear=30, mix=40)
p("Experimental", "Reverse Shimmer", "Reversed swells that climb an octave as they repeat.", space=85, size=200, length=72, mix=50, on_reverse=1, reverse=70, reverse_div=D["1/2"], on_shimmer=1, shimmer=40)
p("Experimental", "Frozen Lows", "Hold the low end of the tail while the rest keeps moving.", space=100, size=180, length=70, mix=45, on_freeze=1, freeze_mode=1)
p("Experimental", "Delay to Cathedral", "Morph from a clean delay (A) to a vast shimmering space (B).", scene_b={"space": 100, "size": 300, "length": 88, "shimmer": 50, "tone": 75, "mix": 50},
  space=0, division=D["1/8D"], length=45, mix=30, on_shimmer=1, shimmer=0, morph=0)
p("Experimental", "Grain Storm", "The repeats shatter into a swarm of grains.", space=50, division=D["1/16"], length=75, mix=50, on_grains=1, grains=90, grain_ms=40, mod_depth=60)

os.makedirs("assets/presets", exist_ok=True)
for f in os.listdir("assets/presets"):
    if f.endswith(".json"): os.remove(os.path.join("assets/presets", f))
for i, pr in enumerate(P):
    pr["order"] = i
    if pr["scene_b"] is None: del pr["scene_b"]
    slug = re.sub(r"[^a-z0-9]+", "_", pr["name"].lower()).strip("_")
    json.dump(pr, open(f"assets/presets/p{i:02d}_{slug}.json", "w"), indent=1)
print(len(P), "presets")
