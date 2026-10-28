"""A small Python model of what the Valence engine plays, for composing the demos before valencerender existed and
for checking its output against what each demo expects. It is not the engine: the demos render from valencerender
whenever it is built (demos.py --sketch forces this model).

A demo preset is a dict: {"bpm", "key", "scale", "complete", "seed", "layers": [layer, ...]}; a layer is
{"ch", "figure", "length", "pattern", "octaves", "offset", "gate", "swing", "vel", "cc", "mutate", "steps": [...]}
with steps of {"on", "vel", "tie", "ratchet", "prob", "oct", "cc"} (missing keys take the defaults below).
"""
import random

FIG = {"1/4": 1.0, "1/8": 0.5, "1/16": 0.25, "1/32": 0.125, "1/8T": 1 / 3, "1/16T": 1 / 6, "1/4T": 2 / 3, "1/8D": 0.75, "1/16D": 0.375}
SCALES = {"minor": [0, 2, 3, 5, 7, 8, 10], "major": [0, 2, 4, 5, 7, 9, 11], "dorian": [0, 2, 3, 5, 7, 9, 10], "phrygian": [0, 1, 3, 5, 7, 8, 10]}
KEYS = {"C": 0, "C#": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}
STEP = {"on": 1, "vel": 100, "tie": 0, "ratchet": 1, "prob": 100, "oct": 0, "cc": -1}


def complete(notes, mode, key, scale):
    """The held notes plus the neutrons that complete the chord in the scale (stacked thirds on the lowest note)."""
    if mode in (None, "off") or not notes: return [(n, 0) for n in notes]
    want = {"triad": 3, "seventh": 4, "ninth": 5}.get(mode, 0)
    sc = SCALES[scale]; tonic = KEYS[key]
    root = min(notes)
    deg = min(range(7), key=lambda d: (root - tonic - sc[d]) % 12)        # the scale degree at or just under the root
    base = root - (root - tonic - sc[deg]) % 12
    out = [(n, 0) for n in sorted(notes)]
    for k in range(want):
        d = deg + 2 * k
        n = base + sc[d % 7] - sc[deg] + 12 * (d // 7)
        if all(n % 12 != o % 12 for o, _ in out): out.append((n, 1))
    if mode == "octaves": out += [(n + 12, 1) for n in notes]
    return sorted(out)


def order(pool, pattern, octaves, rng):
    base = [p for o in range(octaves) for p in [(n + 12 * o, nu) for n, nu in pool]]
    if pattern == "down": return base[::-1]
    if pattern == "updown": return base + base[-2:0:-1]
    if pattern == "updown_rep": return base + base[::-1]
    if pattern == "converge":
        out = []; lo, hi = 0, len(base) - 1
        while lo <= hi: out.append(base[lo]); (lo != hi) and out.append(base[hi]); lo += 1; hi -= 1
        return out
    if pattern == "pedal":
        if len(base) < 2: return base
        out = []
        for p in base[1:]: out += [base[0], p]
        return out
    if pattern == "octaves_alt":
        return [p for n in base for p in (n, (n[0] + 12, n[1]))]
    return base                                      # up, played, chord, random, walk use the plain order


def play(preset, chords, beats, mute=None):
    """chords: [(start_beat, end_beat, [notes])]. Returns per-layer events: (beat, dur, note, vel, nucleon, cc_or_None)."""
    rng_g = random.Random(preset.get("seed", 1))
    layers_out = []
    for li, L in enumerate(preset["layers"]):
        rng = random.Random(rng_g.random())
        dur = FIG[L.get("figure", "1/16")] * L.get("mult", 1)
        n_steps = L.get("length", 16)
        steps = [dict(STEP, **(L.get("steps", [{}] * n_steps)[i] if i < len(L.get("steps", [])) else {})) for i in range(n_steps)]
        sw = (L.get("swing", 50) - 50) / 50.0
        out = []; c = 0; walk = 0; k = 0
        while k * dur < beats:
            t0 = k * dur
            s = steps[k % n_steps]
            if L.get("mutate") and k and k % (n_steps * L["mutate"]) == 0:
                j = rng.randrange(1, n_steps); steps[j] = dict(steps[j], on=1 - steps[j]["on"])
            held = [ch for ch in chords if ch[0] <= t0 + 1e-9 < ch[1]]
            notes = held[-1][2] if held else []
            t = t0 + (sw * dur if (k % 2 == 1 and "T" not in L.get("figure", "")) else 0.0)
            if mute and mute(li, t0): k += 1; continue
            if s["cc"] >= 0: out.append((t, 0, None, None, None, s["cc"]))
            if not notes or not s["on"] or rng.random() * 100 >= s["prob"]:
                k += 1; continue
            pool = complete(notes, preset.get("complete"), preset.get("key", "A"), preset.get("scale", "minor"))
            seq = order(pool, L.get("pattern", "up"), L.get("octaves", 1), rng)
            vel = s["vel"] if L.get("vel") is None else L["vel"]
            r = max(1, s["ratchet"]); g = L.get("gate", 0.5)
            if L.get("pattern") == "chord": picks = [seq[:len(pool)]]
            elif L.get("pattern") == "random": picks = [[rng.choice(seq)]]
            elif L.get("pattern") == "walk":
                walk = max(0, min(len(seq) - 1, walk + rng.choice((-1, 1)))); picks = [[seq[walk]]]
            else: picks = [[seq[(c + L.get("offset", 0)) % len(seq)]]]
            c += 1
            for q in range(r):
                tq = t + q * dur / r
                d = dur * 1.02 if (s["tie"] and r == 1) else dur / r * (min(g, 0.9) if r > 1 else g)
                for n, nu in picks[0]:
                    out.append((tq, d, n + 12 * s["oct"], int(vel * (1 - 0.18 * q) if r > 1 else vel), nu, None))
            k += 1
        layers_out.append(out)
    return layers_out


def to_songs(preset, layers, Song, ccnum=74):
    songs = []
    for li, (L, evs) in enumerate(zip(preset["layers"], layers)):
        s = Song(preset["bpm"]); ch = L.get("ch", li + 1)
        for t, d, n, v, nu, cc in evs:
            if cc is not None: s.cc(t, L.get("cc_num", ccnum), cc, ch)
            else: s.note(t, d, n, v, ch)
        songs.append(s)
    return songs
