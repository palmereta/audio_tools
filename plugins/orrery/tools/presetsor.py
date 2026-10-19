#!/usr/bin/env python3
"""Presets de fábrica de Orrery (assets/presets/pNN_*.json): tres por clima.

Cada preset nombra sólo lo que cambia (el resto queda en el valor por defecto de ParamsOr.h).
Roles: 0 Drone, 1 Pad, 2 Melody, 3 Arp, 4 Cloud, 5 Bass, 6 Pulse. Escalas: índice en Theory.h.
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")
DRONE, PAD, MELODY, ARP, CLOUD, BASS, PULSE = range(7)
SC = {"major": 0, "minor": 1, "dorian": 2, "phrygian": 3, "lydian": 4, "mixolydian": 5, "locrian": 6, "harmonic minor": 7,
      "phrygian dominant": 8, "double harmonic": 9, "major penta": 10, "minor penta": 11, "hirajoshi": 12, "in-sen": 13,
      "whole tone": 14, "lydian aug": 15}
KEY = {"C": 0, "C#": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}

def lane(n, role, **kw):
    p = {f"l{n}_on": 1, f"l{n}_role": role, f"l{n}_channel": n}
    for k, v in kw.items(): p[f"l{n}_{k}"] = v
    return p

PRESETS = []
def preset(name, mood, key, scale, lanes, **g):
    params = {f"l{n}_on": 0 for n in range(1, 9)}
    for l in lanes: params.update(l)
    params.update({"key": KEY[key], "scale": SC[scale]})
    params.update(g)
    PRESETS.append({"name": name, "category": mood, "params": params})

# ───────── Ambient ─────────
preset("First Light", "Ambient", "D", "dorian", [
    lane(1, DRONE, density=0.4, velocity=0.5), lane(2, PAD, voices=4, density=0.55, length=0.6),
    lane(3, MELODY, density=0.35, loop=1, loop_beats=17, echoes=1, echo_time=0.75),
    lane(5, CLOUD, density=0.3, echoes=3, echo_time=1.5, velocity=0.45), lane(6, BASS, density=0.4)],
    tempo=62, chord_beats=16, color=0.55, motion=0.45, space=0.6)
preset("Tape Loops", "Ambient", "F", "lydian", [
    lane(1, MELODY, density=0.15, loop=1, loop_beats=17, range=12, octave=0, mutate=0.05),
    lane(2, MELODY, density=0.15, loop=1, loop_beats=21, range=12, octave=-1, mutate=0.05),
    lane(3, CLOUD, density=0.2, loop=1, loop_beats=23, octave=-1, echoes=1),
    lane(4, CLOUD, density=0.15, loop=1, loop_beats=29, echoes=2, echo_time=2.0),
    lane(5, MELODY, density=0.12, loop=1, loop_beats=31, octave=1, range=10, mutate=0.05),
    lane(6, PAD, voices=3, density=0.3, velocity=0.4)],
    tempo=58, progression=1, color=0.3, motion=0.3, space=0.7)
preset("Slow Tide", "Ambient", "A", "minor penta", [
    lane(2, PAD, voices=4, density=0.5, length=0.7),
    lane(3, ARP, loop=1, loop_beats=13, rate=1, eu_steps=8, eu_pulses=3, arp=3, octave=0, velocity=0.45, echoes=2, echo_time=1.5),
    lane(5, CLOUD, density=0.25, echoes=2), lane(6, BASS, density=0.3)],
    tempo=60, chord_beats=20, color=0.5, motion=0.4, space=0.65)

# ───────── Dark Ambient ─────────
preset("Undertow", "Dark Ambient", "C", "phrygian", [
    lane(1, DRONE, density=0.8, octave=-1, drift=12), lane(2, PAD, voices=3, octave=-1, density=0.4, length=0.8, cc=74, cc_shape=1, cc_depth=0.5),
    lane(5, CLOUD, density=0.18, octave=-1, echoes=4, echo_time=2.0, velocity=0.4), lane(6, BASS, density=0.3, octave=-1)],
    tempo=52, tension=0.6, brightness=0.25, chord_beats=24, color=0.4, motion=0.35, space=0.75)
preset("Catacomb", "Dark Ambient", "D", "harmonic minor", [
    lane(1, DRONE, density=0.6, octave=-1), lane(2, PAD, voices=3, density=0.4, octave=-1),
    lane(7, PULSE, rate=1, eu_steps=8, eu_pulses=2, octave=-2, velocity=0.5, length=0.3),
    lane(3, MELODY, density=0.2, octave=-1, loop=1, loop_beats=19, echoes=2, echo_time=1.5)],
    tempo=56, tension=0.55, brightness=0.3, progression=2, chord_beats=16, motion=0.3, space=0.7)
preset("Black Lake", "Dark Ambient", "E", "phrygian dominant", [
    lane(1, DRONE, density=0.7, octave=-1, drift=20), lane(2, PAD, voices=4, density=0.35, cc=1, cc_shape=0, cc_beats=32, cc_depth=0.6),
    lane(5, CLOUD, density=0.2, echoes=5, echo_time=2.5, echo_decay=0.7, velocity=0.35)],
    tempo=48, tension=0.7, brightness=0.2, chord_beats=32, motion=0.3, space=0.85)

# ───────── Drone ─────────
preset("Om Field", "Drone", "C", "major penta", [
    lane(1, DRONE, density=0.75, octave=-1), lane(2, DRONE, density=0.4, octave=0, channel=2),
    lane(5, CLOUD, density=0.12, echoes=3, echo_time=2.0, velocity=0.35)],
    tempo=50, progression=1, color=0.2, motion=0.2, space=0.8)
preset("Tanpura", "Drone", "D", "major", [
    lane(1, DRONE, density=0.8, octave=-1),
    lane(3, ARP, follow=2, arp=0, rate=1, eu_steps=4, eu_pulses=4, octave=-1, range=13, loop=1, loop_beats=4, mutate=0.0, velocity=0.5, length=0.9),
    lane(2, PAD, voices=2, density=0.25, velocity=0.35)],
    tempo=60, progression=1, color=0.1, motion=0.15, space=0.7)
preset("Monolith", "Drone", "B", "locrian", [
    lane(1, DRONE, density=0.9, octave=-2, drift=25), lane(2, DRONE, density=0.5, octave=-1, cc=74, cc_shape=1, cc_depth=0.7, cc_center=0.4)],
    tempo=40, progression=1, tension=0.4, brightness=0.15, motion=0.2, space=0.9)

# ───────── Sleep ─────────
preset("Night Garden", "Sleep", "G", "major penta", [
    lane(2, PAD, voices=3, density=0.35, length=0.7, velocity=0.45), lane(5, CLOUD, density=0.15, echoes=2, echo_time=2.0, velocity=0.35),
    lane(6, BASS, density=0.2, velocity=0.4)],
    tempo=50, chord_beats=24, color=0.4, motion=0.25, space=0.75, gentle=1, session=45, fade_in=60, fade_out=300, density=0.35)
preset("Deep Sleep", "Sleep", "F", "major", [
    lane(1, DRONE, density=0.4, octave=-1, velocity=0.4), lane(2, PAD, voices=3, density=0.3, velocity=0.4, length=0.8)],
    tempo=46, chord_beats=32, color=0.35, motion=0.2, space=0.85, brightness=0.3, gentle=1, session=60, fade_in=90, fade_out=300, density=0.3)
preset("Lullaby Stars", "Sleep", "C", "lydian", [
    lane(3, MELODY, density=0.2, velocity=0.4, loop=1, loop_beats=23, echoes=1), lane(4, ARP, rate=1, eu_steps=8, eu_pulses=2, octave=1, velocity=0.35, echoes=2),
    lane(2, PAD, voices=3, density=0.3, velocity=0.4)],
    tempo=52, chord_beats=16, color=0.5, motion=0.3, space=0.7, gentle=1, session=30, fade_in=45, fade_out=240, density=0.35)

# ───────── Meditation ─────────
preset("Breath Six", "Meditation", "D", "major penta", [
    lane(1, DRONE, density=0.5, octave=-1, velocity=0.45), lane(2, PAD, voices=4, density=0.45, cc=2, cc_shape=2, cc_depth=0.8, cc_center=0.5),
    lane(5, CLOUD, density=0.15, echoes=2, velocity=0.35)],
    tempo=60, breath=6, gentle=1, session=20, fade_in=30, fade_out=90, motion=0.3, space=0.7)
preset("Slow Breath", "Meditation", "A", "dorian", [
    lane(2, PAD, voices=3, density=0.4, length=0.8, cc=11, cc_shape=2, cc_depth=0.7), lane(3, MELODY, density=0.2, velocity=0.4, echoes=1),
    lane(6, BASS, density=0.2, velocity=0.4)],
    tempo=56, breath=4, gentle=1, session=15, fade_in=30, fade_out=90, motion=0.25, space=0.75)
preset("Singing Bowls", "Meditation", "D", "major penta", [
    lane(5, CLOUD, density=0.15, octave=-1, length=1.0, echoes=4, echo_time=3.0, echo_decay=0.65, velocity=0.5),
    lane(8, CLOUD, density=0.1, octave=0, length=1.0, echoes=3, echo_time=4.0, velocity=0.4, loop=1, loop_beats=37),
    lane(1, DRONE, density=0.4, octave=-1, velocity=0.35)],
    tempo=60, breath=5, progression=1, gentle=1, session=20, fade_in=20, fade_out=120, space=0.85)

# ───────── Focus ─────────
preset("Deep Work", "Focus", "E", "minor", [
    lane(2, PAD, voices=4, density=0.45, velocity=0.45), lane(7, PULSE, rate=2, eu_steps=8, eu_pulses=5, velocity=0.35, length=0.25, octave=0),
    lane(6, BASS, density=0.4, velocity=0.45)],
    tempo=70, chord_beats=16, tension=0.1, motion=0.3, space=0.5, gentle=1)
preset("Study Rain", "Focus", "Bb", "major", [
    lane(5, CLOUD, density=0.55, octave=1, velocity=0.3, length=0.2), lane(8, CLOUD, density=0.4, octave=0, velocity=0.3, length=0.3),
    lane(2, PAD, voices=3, density=0.35, velocity=0.4)],
    tempo=66, chord_beats=24, tension=0.1, motion=0.25, brightness=0.4, gentle=1)
preset("Flow State", "Focus", "G", "mixolydian", [
    lane(3, ARP, loop=1, loop_beats=16, rate=2, eu_steps=16, eu_pulses=7, arp=2, velocity=0.4), lane(6, BASS, density=0.5, velocity=0.5),
    lane(2, PAD, voices=3, density=0.35, velocity=0.4)],
    tempo=74, chord_beats=16, tension=0.15, motion=0.3, space=0.4)

# ───────── Celestial ─────────
preset("Orbiting", "Celestial", "E", "lydian", [
    lane(4, ARP, rate=2, eu_steps=8, eu_pulses=5, arp=0, octave=1, velocity=0.45, echoes=2, echo_time=0.75),
    lane(5, CLOUD, density=0.35, octave=1, echoes=3), lane(2, PAD, voices=4, density=0.5, octave=1), lane(6, BASS, density=0.3)],
    tempo=64, brightness=0.75, color=0.7, chord_beats=16, motion=0.45, space=0.65)
preset("Aurora", "Celestial", "Ab", "lydian aug", [
    lane(2, PAD, voices=5, density=0.55, cc=74, cc_shape=0, cc_beats=24, cc_depth=0.6), lane(5, CLOUD, density=0.3, echoes=4, echo_time=1.5),
    lane(3, MELODY, density=0.25, octave=1, loop=1, loop_beats=29, echoes=2)],
    tempo=58, brightness=0.7, color=0.8, chord_beats=20, motion=0.4, space=0.75)
preset("Starfield", "Celestial", "C", "whole tone", [
    lane(5, CLOUD, density=0.3, octave=1, echoes=3, echo_time=1.25), lane(8, CLOUD, density=0.25, octave=2, echoes=4, echo_time=2.0, velocity=0.4),
    lane(4, ARP, rate=3, eu_steps=16, eu_pulses=3, arp=3, octave=1, velocity=0.35, loop=1, loop_beats=11), lane(1, DRONE, density=0.4, octave=-1)],
    tempo=60, brightness=0.8, chord_beats=24, progression=1, motion=0.35, space=0.7)

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, p in enumerate(PRESETS):
        fn = f"p{i + 1:02d}_{p['name'].lower().replace(' ', '_')}.json"
        json.dump(p, open(os.path.join(OUT, fn), "w"), indent=1)
    print(len(PRESETS), "presets")

if __name__ == "__main__":
    main()
