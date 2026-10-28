#!/usr/bin/env python3
"""Demos of Valence: the arpeggiator's layers played by our own synths, with drums, bass and pads, mixed in Python.

    VALENCE_BUILD=<build dir> python plugins/valence/tools/demos.py OUTDIR [--only name,name] [--sketch] [--stems DIR]

Each demo writes into OUTDIR/<name>/: the input the player "plays" (input.mid), the Valence preset (preset.json), one
.mid per layer and one with all of them (from valencerender, or from tools/demos/sketch.py with --sketch), the note
list (notes.json), the final mix (<name>.wav, 24-bit 48 kHz, and .mp3) and a check report (checks.json). Stems go to
--stems (default OUTDIR/<name>/stems). The main demo also gets a louder master through Lacquer. index.md and
index.json sum it up.

Everything is our own material: drums from Pallor (MIDI written here), bass, keys and stabs from AT303, ATMD, AT101V,
AT20V, AT700, ATFS1, AT330 and DeviceNTI V, effects from Halation, Tarnish and Murmur, the loud master from Lacquer.
Needs numpy, scipy, soundfile, pyloudnorm, ffmpeg and the render CLIs (the AT303/ATMD MIDI renders come from
tools/demos/extra.cmake). Run it on the build machine (pc50), not on the Mac.
"""
import json, os, shutil, sys, tempfile
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "demos"))
from vdlib import *            # noqa: E402
import sketch                   # noqa: E402

ARGS = sys.argv[1:]
OUT = os.path.abspath(ARGS[0]) if ARGS and not ARGS[0].startswith("--") else os.path.abspath(os.path.join(ROOT, "..", "audiotools-private", "valence", "demos"))
def _opt(name, default=None):
    return ARGS[ARGS.index(name) + 1] if name in ARGS and ARGS.index(name) + 1 < len(ARGS) else default
ONLY = set(_opt("--only").split(",")) if _opt("--only") else None
INDEX_ONLY = "--index-only" in ARGS
SKETCH = "--sketch" in ARGS
STEMS = _opt("--stems")


# ── Valence: the layers of a demo ──
# The demos describe their layers in a compact form (one dict per step); to_valence() turns that into a Valence preset
# (params + rows, the format of assets/presets/ and tools/presets.py) that valencerender plays.
FIGS = ["1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D", "1/16", "1/16T", "1/16D", "1/32", "1/32T", "1/32D"]
PATS = ["up", "down", "updown", "updownrep", "converge", "diverge", "played", "random", "walk", "chord", "pedal", "finger"]
SCALES = ["major", "minor", "dorian", "phrygian"]
KEYS = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
COMPLETE = ["off", "triad", "seventh", "ninth", "octaves"]
ACCENT = 24

def to_valence(p):
    params = {"tempo": p["bpm"], "key": KEYS.index(p.get("key", "A")), "scale": SCALES.index(p.get("scale", "minor")),
              "seed": p.get("seed", 1), "complete": COMPLETE.index(p.get("complete", "off")), "accent": ACCENT, "order": 1,
              "out_mode": 1}       # each layer on its own channel: two layers on the same note neither merge nor share a note-off
    rows = {}
    for n in range(1, 5): params[f"l{n}_on"] = 0
    for i, L in enumerate(p["layers"]):
        n = i + 1; length = L.get("length", 16)
        steps = [dict(sketch.STEP, **(L.get("steps", [])[k] if k < len(L.get("steps", [])) else {})) for k in range(length)]
        pat = L.get("pattern", "up"); octaves = L.get("octaves", 1); oct_mode = 0
        if pat == "octaves_alt": pat, octaves, oct_mode = "up", max(2, octaves), 1
        octs = {s["oct"] for s in steps}
        register = octs.pop() if len(octs) == 1 else 0
        params.update({f"l{n}_on": 1, f"l{n}_channel": n, f"l{n}_rate": FIGS.index(L.get("figure", "1/16")), f"l{n}_length": length,
                       f"l{n}_pattern": PATS.index(pat), f"l{n}_octaves": octaves, f"l{n}_oct_mode": oct_mode,
                       f"l{n}_octave": register, f"l{n}_gate": L.get("gate", 0.5), f"l{n}_swing": L.get("swing", 50),
                       f"l{n}_shift": L.get("offset", 0), f"l{n}_vel_mode": 2, f"l{n}_restart": 0,
                       f"l{n}_mutate": {0: 0.0, 2: 0.5, 3: 0.35}.get(L.get("mutate", 0), 0.3),
                       f"l{n}_cc": L.get("cc_num", 74) if any(s["cc"] >= 0 for s in steps) else -1})
        r = {"vel": [s["vel"] - ACCENT if s["vel"] >= 110 else s["vel"] for s in steps],
             "accent": [int(s["vel"] >= 110) for s in steps], "on": [s["on"] for s in steps], "tie": [s["tie"] for s in steps],
             "ratchet": [s["ratchet"] for s in steps], "prob": [s["prob"] for s in steps],
             "oct": [s["oct"] - register for s in steps], "cc": [s["cc"] for s in steps]}
        if any("finger" in s for s in steps): r["finger"] = [s.get("finger", k % 8) for k, s in enumerate(steps)]   # the chord index of each step (pattern "finger")
        defaults = {"accent": 0, "on": 1, "tie": 0, "ratchet": 1, "prob": 100, "oct": 0, "cc": -1}
        rows[f"l{n}"] = {k: v for k, v in r.items() if k in ("vel", "finger") or any(x != defaults[k] for x in v)}
    return {"name": p["name"], "category": "Demo", "params": params, "rows": rows}


def valence_layers(name, preset, inp, beats, wd):
    """Write input.mid and preset.json, get one .mid per layer plus notes.json. Returns [(layer_index, path)]."""
    inp_path = inp.write(os.path.join(wd, "input.mid"))
    vp = to_valence(preset)
    json.dump(vp, open(os.path.join(wd, "preset.json"), "w"), indent=1)
    vr = None
    if not SKETCH:
        try: vr = tool("valence")
        except FileNotFoundError: vr = None
    if vr is not None:
        run([vr, os.path.join(wd, "preset.json"), inp_path, wd, "--secs", beats * 60 / preset["bpm"], "--bpm", preset["bpm"]])
        ev = json.load(open(os.path.join(wd, "events.json")))
        json.dump({"source": "valencerender", "notes": ev["notes"], "fusions": ev.get("fusions", [])}, open(os.path.join(wd, "notes.json"), "w"))
        return [(i, os.path.join(wd, f"layer{i + 1}.mid")) for i in range(4) if os.path.exists(os.path.join(wd, f"layer{i + 1}.mid"))]
    json.dump(preset, open(os.path.join(wd, "sketch_preset.json"), "w"), indent=1)
    chords = held_chords(inp)
    layers = sketch.play(preset, chords, beats)
    songs = sketch.to_songs(preset, layers, Song)
    paths = []; notes = []; allsong = Song(preset["bpm"])
    for li, (s, evs) in enumerate(zip(songs, layers)):
        if not any(cc is None for *_, cc in evs): continue
        paths.append((li, s.write(os.path.join(wd, f"layer{li + 1}.mid"))))
        allsong.ev += s.ev
        notes += [{"layer": li + 1, "beat": round(t, 4), "t": round(t * 60 / preset["bpm"], 5), "note": n, "vel": v, "dur": round(d * 60 / preset["bpm"], 5),
                   "nucleon": "neutron" if nu else "proton"} for t, d, n, v, nu, cc in evs if cc is None]
    allsong.write(os.path.join(wd, "all.mid"))
    json.dump({"source": "sketch", "notes": sorted(notes, key=lambda e: (e["beat"], e["layer"]))}, open(os.path.join(wd, "notes.json"), "w"))
    return paths


def held_chords(inp):
    """The chords the input holds over time: [(start_beat, end_beat, notes)] from its note-ons and note-offs."""
    edges = sorted({b for b, _, _ in inp.ev})
    on = {}; spans = []
    for b, p, msg in sorted(inp.ev, key=lambda e: (e[0], e[1])):
        if msg[0] & 0xF0 == 0x90: on[msg[1]] = b
        else: spans.append((on.pop(msg[1]), b, msg[1]))
    out = []
    for a, z in zip(edges, edges[1:]):
        ns = sorted(n for s, e, n in spans if s <= a and e >= z)
        if ns: out.append((a, z, ns))
    return out


# ── Drums: 16-step strings per bar ──
VEL = {"X": 122, "x": 100, "o": 72, "-": 44}
def drums(song, bar, parts, swing=50, steps=16):
    """parts: {voice: "x...x...", ...} for one bar starting at bar (4/4). swing delays the odd 16ths."""
    for voice, pat in parts.items():
        pat = pat.replace(" ", "")
        for i, c in enumerate(pat[:steps]):
            if c in VEL:
                t = bar * 4 + i * 4 / steps + ((swing - 50) / 50 * (4 / steps) if i % 2 else 0)
                song.hit(t, voice, VEL[c])

def kicks(song):
    return sorted(song.secs(b) for b, p, msg in song.ev if p == 2 and msg[0] == 0x99 and msg[1] == GM["kick"])


# ── Stems and the mix ──
# Level trims on top of each demo's targets, from the first analysis of the mixes (analyze.py): 20-60 Hz carried about
# two thirds of the energy and 2.5-6 kHz sat 20 dB under it, so kicks and subs come down and hats go up.
TRIM = {"Kick": -1.5, "sub": -1.0, "bass": -1.0, "reese": -1.0, "Hats": 1.0}
class Mix:
    def __init__(self, name, bpm, bars, tmp, wd, tail=4.0):
        self.name, self.bpm, self.tmp, self.wd = name, bpm, tmp, wd
        self.n = int((bars * 4 * 60 / bpm + tail) * SR)
        self.stems = {}; self.sends = {}; self.mids = {}
        self.stemdir = os.path.join(STEMS, os.path.basename(wd)) if STEMS else os.path.join(wd, "stems")
        os.makedirs(self.stemdir, exist_ok=True)

    def add(self, key, x, lufs_target, pan_=0.0, hpf=None, lpf=None, wid=None, send=None, duck_on=None, duck_db=0.0, mid=None):
        mid = mid or getattr(x, "mid", None)
        if mid: self.mids[key] = mid
        x = np.asarray(x)
        x = fit(x, self.n)
        x = x - x.mean(axis=0)                      # DC
        if hpf: x = hp(x, hpf)
        if lpf: x = lp(x, lpf)
        if wid is not None: x = width(x, wid)
        if pan_: x = pan(x, pan_)
        x = at_lufs(x, lufs_target + sum(v for k, v in TRIM.items() if k in key))
        if duck_on is not None and duck_db: x = duck(x, duck_on, duck_db)
        self.stems[key] = x
        for bus, amt in (send or {}).items():
            self.sends.setdefault(bus, []).append(x * db(amt))
        return x

    def bus(self, key, kind, preset, lufs_target, sets=(), hpf=150, duck_on=None, duck_db=0.0, tail=6.0):
        if key not in self.sends: return
        dry = sum(self.sends.pop(key))
        wet = fx(kind, dry, self.tmp, f"{self.name}_{key}", preset, self.bpm, tail=tail, sets=dict(sets, mix=100) if kind == "halation" else sets)
        self.add(key, wet, lufs_target, hpf=hpf, duck_on=duck_on, duck_db=duck_db)

    def sum(self):
        for k, x in self.stems.items():
            sf.write(os.path.join(self.stemdir, f"{k}.wav"), x.astype(np.float32), SR, subtype="FLOAT")
        json.dump({k: os.path.basename(v) for k, v in self.mids.items()}, open(os.path.join(self.stemdir, "sources.json"), "w"), indent=1)
        return sum(self.stems.values())


def render_layer(tmp, wd, paths, li, kind, preset, bpm, **kw):
    mid = dict(paths).get(li)
    if mid is None: return np.zeros((SR, 2))
    return tag(render(kind, mid, os.path.join(tmp, f"{os.path.basename(wd)}_L{li + 1}_{kind}.wav"), preset, bpm, **kw), mid)

def dv(focus, **extra):
    """--set values for a DeviceNTI V track played from MIDI: tape, AGE and the track sends off, the rest as the preset (the
    first demos; the crackle they avoided is the master tape noise alone, see vdlib.dv_project)."""
    return dict({"tape": 0, f"t{focus}_age": 0, f"t{focus}_delay": 0, f"t{focus}_reverb": 0}, **extra)

def part(tmp, wd, song, key, kind, preset, **kw):
    mid = song.write(os.path.join(wd, f"{key}.mid"))
    return tag(render(kind, mid, os.path.join(tmp, f"{os.path.basename(wd)}_{key}.wav"), preset, song.bpm, **kw), mid)

def pallor(tmp, wd, song, preset, sets=(), key="drums"):
    """Pallor playing the drum part; returns {bus: audio} from its per-voice outputs (Kick, Snare-Clap, Hats-Cymbals,
    Toms, Perc, FX, and Main for the master returns)."""
    mid = song.write(os.path.join(wd, f"{key}.mid"))
    sd = os.path.join(tmp, f"{os.path.basename(wd)}_{key}_stems"); os.makedirs(sd, exist_ok=True)
    run([tool("pallor"), os.path.join(tmp, f"{os.path.basename(wd)}_{key}.wav"), "--preset", preset, "--midi", mid, "--bpm", song.bpm,
         "--tail", 2.0, "--stems", sd] + sum((["--set", f"{k}={v}"] for k, v in dict(sets).items()), []))
    out = {}
    for f in sorted(os.listdir(sd)):
        x = load(os.path.join(sd, f))
        if np.abs(x).max() > 1e-5: out[f.split("_", 1)[1][:-4]] = tag(x, mid)
    return out


# Tarnish for its saturation and wear only: no dropouts, crackle or scrape (they read as clicks in a demo).
TAPE_CLEAN = {"dropouts": 0, "crackle": 0, "scrape": 0}

# The ATMD's Detuned Saws reworked as a reese, at the written pitch (8'; the factory ranges put the notes of these layers
# under 25 Hz): two sawtooths 20 cents apart, so F1 phases at about half a hertz and its harmonics faster, a third saw an
# octave up for the growl, two unison voices for width, every oscillator high in the mixer to overload the filter,
# emphasis for the CC 74 row to talk through, multi trigger so the ratchets restart the contours.
REESE = {"osc1_range": 3, "osc2_range": 3, "osc3_range": 4, "osc1_wave": 2, "osc2_wave": 2, "osc3_wave": 2, "osc2_freq": 0.2, "osc3_freq": -0.12,
         "mix_osc1": 10, "mix_osc2": 10, "mix_osc3": 6, "mix_osc1_on": 1, "mix_osc2_on": 1, "mix_osc3_on": 1, "filter_emphasis": 4.5,
         "filter_contour": 1.5, "f_attack": 0, "f_decay": 3, "f_sustain": 6, "kbd_ctrl1": 1, "kbd_ctrl2": 0, "l_attack": 0, "l_decay": 3.5,
         "l_sustain": 10, "decay_on": 1, "trigger": 1, "priority": 1, "glide_on": 0, "unison": 2, "detune": 8, "spread": 0.6, "analog": 0.5,
         "output": -10}
REESE_CC = ["74=filter_cutoff:-3.0:2.0"]             # about 90 Hz closed to 3.5 kHz open
REESE_TAPE = dict(TAPE_CLEAN, drive=20, saturation=100, noise=0, wow=0, flutter=0, highcut=22000, mix=100)   # studio tape pushed into distortion

def reese(tmp, wd, paths, li, bpm, **sets):
    """The reese layer: the ATMD, the band over 100 Hz through Tarnish driven hard (the distortion), the band under it
    clean and mono (the sub stays steady under the growl)."""
    x = render_layer(tmp, wd, paths, li, "atmd", "detuned_saws", bpm, tail=1.0, cc=REESE_CC, sets=dict(REESE, **sets))
    top = hp(x, 100, 4)
    dist = fit(fx("tarnish", top, tmp, os.path.basename(wd) + "_reese_tape", "Warm Studio Tape", bpm, tail=0.5, sets=REESE_TAPE), len(x))
    sub = lp(x, 100, 4).mean(axis=1, keepdims=True).repeat(2, axis=1)
    return tag(sub + at_lufs(dist, lufs(top) + 4.5), x.mid)

# Sub basses written in the demos: the root of the chord between E1 and D#2 (41-78 Hz), whatever octave it was held in.
def sub_note(name): return 28 + (m(name) - 28) % 12

# ATMD presets for a sub at the written pitch (their factory ranges sit one or two octaves lower).
SUB_8 = {"osc1_range": 3, "osc2_range": 3, "osc3_range": 3}
# The AT101V Rubber Bass with its sub oscillator (one octave down) on the written pitch: the saw at 4', an octave up.
# The factory 16' put the saw an octave and the sub two octaves under the note (16-25 Hz on these layers).
AT101_AT_PITCH = {"vco_range": 2}

# Kits: Pallor's analog voices set for each genre on top of a factory preset (its effects turned down).
CLEAN = {"m_lofi_mix": 0, "a_return": -60, "b_return": -60, "m_tape_drive": 0, "m_exciter": 0}
TECHNO_KIT = dict(CLEAN, bd_model=0, bd_decay=55, bd_tune=0, bd_tone=58, bd_drive=25, sd_model=4, cp_model=0, cp_level=-3,
                  ch_model=0, ch_decay=18, ch_tone=70, oh_model=0, oh_decay=40, rd_model=0, rs_model=0, cb_model=1, cl_model=1,
                  m_comp=35)
HOUSE_KIT = dict(CLEAN, bd_model=0, bd_decay=50, bd_tone=55, bd_tune=1, cp_model=1, cp_level=-2, ch_model=0, ch_decay=14, oh_model=0, oh_decay=55,
                 tb_model=2, tb_level=-6, rs_model=1, cb_model=2, m_comp=30)
BREAK_KIT = dict(CLEAN, bd_engine=1, bd_kit=5, sd_engine=1, sd_kit=5, ch_engine=1, ch_kit=5, oh_engine=1, oh_kit=5, rd_engine=1, rd_kit=5,
                 cy_engine=1, cy_kit=5, rs_engine=1, rs_kit=5, tb_model=2, m_comp=45)
HEAVY_BREAK_KIT = dict(BREAK_KIT, bd_kit=4, sd_kit=4, ch_kit=4, oh_kit=4, rd_kit=4, cy_kit=4, rs_kit=4, bd_drive=30, sd_drive=25, m_comp=55)
# A rolling break: the roomy acoustic kit with the ghost notes really soft (velocity sensitivity up), a brighter snare,
# hats through the clean chain so the shuffle stays crisp.
LIQUID_KIT = dict(BREAK_KIT, sd_velsens=95, rs_velsens=90, ch_velsens=90, oh_velsens=80, sd_tune=2, sd_decay=45, ch_decay=35,
                  ch_chain=4, oh_chain=4, rd_chain=4, ch_tone=62, m_comp=40)
# The hard break: Crunch 88 driven, the snare cracking, a tighter kick.
HARD_KIT = dict(HEAVY_BREAK_KIT, sd_velsens=90, sd_drive=40, sd_tone=62, bd_drive=40, bd_decay=40, ch_velsens=85, m_comp=60)


# ── The demos ──
def demo_phase_drift(tmp, wd):
    bpm, bars = 128, 64
    inp = Song(bpm); inp.chord(0, bars * 4, ["A3", "C4", "E4", "G4"], 100)          # one chord held for the whole demo
    cc = [40, 52, 64, 76, 88, 90, 80, 68, 56, 46, 40, 48, 60, 72, 84, 70]
    acc = {0: 112, 4: 104, 10: 110}
    preset = {"name": "Phase Drift", "bpm": bpm, "key": "A", "scale": "minor", "seed": 7, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "updown", "gate": 0.35, "swing": 52, "cc_num": 74,
         "steps": [{"vel": acc.get(i, 86), "cc": cc[i]} for i in range(16)]},
        {"ch": 2, "figure": "1/16", "length": 15, "pattern": "played", "gate": 0.3, "swing": 52,
         "steps": [{"on": int(i in (0, 2, 5, 7, 9, 12)), "vel": 92 if i % 3 == 0 else 74, "prob": 100 if i in (0, 5, 9) else 70, "oct": 1} for i in range(15)]},
        {"ch": 3, "figure": "1/16", "length": 12, "pattern": "random", "gate": 0.3, "swing": 52,
         "steps": [{"on": int(i in (0, 3, 5, 8, 10)), "vel": 84, "oct": 1 if i % 2 == 0 else 0} for i in range(12)]},
    ]}
    paths = valence_layers("phase_drift", preset, inp, bars * 4, wd)
    # Drums change on every fusion (all three layers back on step 0 every 240 sixteenths = 15 bars).
    d = Song(bpm)
    for b in range(bars):
        sec = b // 15
        p = {"kick": "x...x...x...x...", "chh": "..x...x...x...x."}
        if sec >= 1: p.update(clap="....x.......x...", chh="..x.-.x...x.-.xo")
        if sec >= 2: p.update(ohh="..x...x...x...x.", chh="o.x.o.x.o.x.o.xo", rim="...x..x....x..x.")
        if sec == 3: p = {"chh": "..x...x...x...x." if b % 15 > 7 else "", "rim": "...x..x....x..x."}   # the breakdown
        if b >= 60: p = {"kick": "x...x...x...x...", "clap": "....x.......x...", "ohh": "..x...x...x...x.", "chh": "o.x.o.x.o.x.o.xo", "rim": "...x..x....x..x."}
        if b % 15 == 0 and b: p["crash"] = "x..............."
        if b == 0: p.pop("chh", None)
        drums(d, b, p)
    bass = Song(bpm)
    for b in range(15, bars):
        if 45 <= b < 60: continue
        for i in (2, 6, 10, 14): bass.note(b * 4 + i * 0.25, 0.2, "A1", 100 if i == 2 else 88)
    mx = Mix("phase_drift", bpm, bars, tmp, wd)
    ks = kicks(d)
    dr = pallor(tmp, wd, d, "Pressure Plant", TECHNO_KIT)
    for k, x in dr.items(): mx.add("drums_" + k, x, {"Kick": -19, "Snare-Clap": -25, "Hats-Cymbals": -26, "Perc": -29}.get(k, -30), send={"verb": -14} if k != "Kick" else None)
    mx.add("bass", part(tmp, wd, bass, "bass", "atmd", "rubber_bass", tail=3.0, sets=SUB_8), -24, lpf=900, duck_on=ks, duck_db=5)
    l1 = render_layer(tmp, wd, paths, 0, "devicenti", "Rainy Pluck", bpm, focus=4, tail=3.0, sets=dv(4))
    l2 = render_layer(tmp, wd, paths, 1, "devicenti", "Glass Pluck", bpm, focus=4, tail=3.0, sets=dv(4))
    l3 = render_layer(tmp, wd, paths, 2, "atfs1", "glass_marimba", bpm)
    mx.add("layer1", l1, -22, pan_=-0.3, hpf=180, send={"delay": -8, "verb": -14}, duck_on=ks, duck_db=3)
    mx.add("layer2", l2, -24, pan_=0.4, hpf=250, send={"delay": -6, "verb": -12}, duck_on=ks, duck_db=3)
    mx.add("layer3", l3, -25, pan_=0.1, hpf=250, send={"verb": -10}, duck_on=ks, duck_db=3)
    mx.bus("delay", "halation", "Dotted Rain", -26, duck_on=ks, duck_db=4)
    mx.bus("verb", "halation", "Small Room", -29)
    return mx.sum(), {"title": "Phase Drift", "genre": "Minimal techno", "bpm": bpm, "preset": "Phase Drift",
            "point": "One chord held for two minutes that never repeats: layers of 16, 15 and 12 steps realign every 15 bars, and the drums change on each realignment.",
            "instruments": "DeviceNTI V Rainy Pluck (layer 1, CC 74 row on its cutoff) and Glass Pluck (layer 2), ATFS1 Glass Marimba (layer 3), Pallor kit, ATMD Rubber Bass at 8', Halation Dotted Rain and Small Room",
            "fusions_bars": [15, 30, 45, 60]}


def demo_acid_shell(tmp, wd):
    bpm, bars = 130, 32
    inp = Song(bpm)
    prog = [["A1", "C2", "E2"]] * 4 + [["F1", "A1", "C2"], ["G1", "B1", "D2"], ["A1", "C2", "E2"], ["A1", "C2", "E2"]]
    for k, c in enumerate(prog): inp.chord(k * 16, 16, c, 100)
    cc = [30, 36, 44, 52, 60, 66, 70, 74, 76, 74, 68, 60, 52, 44, 38, 34]
    steps1 = [{"vel": 120 if i in (0, 3, 6, 10) else 96, "tie": int(i in (5, 13)), "ratchet": 3 if i == 15 else 1, "oct": 1 if i in (2, 9) else 0, "cc": cc[i]} for i in range(16)]
    preset = {"name": "Acid Shell", "bpm": bpm, "key": "A", "scale": "minor", "seed": 11, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "pedal", "gate": 0.45, "swing": 54, "cc_num": 74, "steps": steps1},
        {"ch": 2, "figure": "1/16", "length": 7, "pattern": "chord", "gate": 0.25, "swing": 54,
         "steps": [{"on": int(i == 0), "prob": 85, "vel": 100, "oct": 2} for i in range(7)]},
    ]}
    paths = valence_layers("acid_shell", preset, inp, bars * 4, wd)
    d = Song(bpm)
    for b in range(bars):
        p = {"kick": "x...x...x...x...", "chh": "..x...x...x...x."}
        if b >= 8: p.update(clap="....x.......x...", chh="o.x.o.x.o.x.o.x-")
        if b >= 12: p["ohh"] = "..x...x...x...x."
        if 16 <= b < 20: p = {"chh": "o.x.o.x.o.x.o.x-", "clap": "....x.......x..." if b >= 18 else ""}
        if b in (8, 20): p["crash"] = "x..............."
        if b % 8 == 7 and b != 15: p["clap"] = "....x.......x.xx"
        drums(d, b, p)
    mx = Mix("acid_shell", bpm, bars, tmp, wd)
    ks = kicks(d)
    for k, x in pallor(tmp, wd, d, "Body Discipline", TECHNO_KIT).items():
        mx.add("drums_" + k, x, {"Kick": -18.5, "Snare-Clap": -24, "Hats-Cymbals": -26}.get(k, -30), send={"verb": -14} if k == "Snare-Clap" else None)
    acid = render_layer(tmp, wd, paths, 0, "at303", "Squelch Line", bpm, tail=1.0, cc=["74=cutoff:18:72"])
    stab = render_layer(tmp, wd, paths, 1, "at101v", "techno_stab", bpm, sets={"voices": 2, "dly_on": 0, "rev_on": 0})
    mx.add("layer1_acid", acid, -20, send={"delay": -12}, duck_on=ks, duck_db=3)
    mx.add("layer2_stab", stab, -25, pan_=0.2, hpf=200, send={"delay": -6, "verb": -10}, duck_on=ks, duck_db=4)
    mx.bus("delay", "halation", "Dotted Rain", -28, duck_on=ks, duck_db=5)
    mx.bus("verb", "halation", "Small Room", -30)
    return mx.sum(), {"title": "Acid Shell", "genre": "Minimal techno", "bpm": bpm, "preset": "Acid Shell",
            "point": "Accents, slides (tied steps), a ratchet on the last step and a CC 74 row driving the AT303 cutoff, with a chord stab on a 7-step layer against it.",
            "instruments": "AT303 Squelch Line (layer 1, MIDI with CC 74 on cutoff), AT101V Techno Stab (layer 2), Pallor kit, Halation Dotted Rain and Small Room"}


def demo_dub_chord(tmp, wd):
    bpm, bars = 124, 32
    inp = Song(bpm)
    for k in range(bars // 8):
        inp.chord(k * 32, 32, ["C3", "Eb3", "G3", "Bb3", "D4"] if k % 2 == 0 else ["F3", "Ab3", "C4", "Eb4", "G4"], 100)
    preset = {"name": "Dub Chord", "bpm": bpm, "key": "C", "scale": "minor", "seed": 5, "layers": [
        {"ch": 1, "figure": "1/8", "length": 16, "pattern": "chord", "gate": 0.2, "swing": 50, "mutate": 3,
         "steps": [{"on": int(i in (1, 4, 6, 9, 11, 14)), "prob": 100 if i in (1, 9) else 60, "vel": 100 if i in (1, 9) else 84} for i in range(16)]},
        {"ch": 2, "figure": "1/4", "length": 4, "pattern": "finger", "gate": 0.9, "swing": 50,       # every step on chord index 0: the root
         "steps": [{"oct": -2, "vel": 100, "finger": 0} for _ in range(4)]},
    ]}
    paths = valence_layers("dub_chord", preset, inp, bars * 4, wd)
    d = Song(bpm)
    for b in range(bars):
        p = {"kick": "x...x...x...x...", "tamb": "-.o.-.o.-.o.-.o-"}
        if b >= 4: p["chh"] = "..x...x...x...x."
        if b >= 8: p["rim"] = "...o..x....o..x." if b % 2 else "......x.......x."
        if b >= 16: p["ohh"] = "..o...o...o...o."
        if 24 <= b < 26: p.pop("kick")
        drums(d, b, p)
    mx = Mix("dub_chord", bpm, bars, tmp, wd)
    ks = kicks(d)
    for k, x in pallor(tmp, wd, d, "Sodium Streetlight", dict(TECHNO_KIT, bd_decay=70, bd_tone=30)).items():
        mx.add("drums_" + k, x, {"Kick": -19, "Hats-Cymbals": -28, "Perc": -27, "Snare-Clap": -28}.get(k, -31), send={"dub": -16} if k == "Perc" else None)
    # The stab is Dub Throws whole: its tape, AGE and its own dub delay (dotted eighth) and hall, with the master
    # crackle off (dv_project). A filter sweep over 8 bars and delay throws are automated on the track: the last stab
    # of every fourth bar goes into the delay at full send with the feedback up, then both come back.
    spb = 60 / bpm; ons = sorted(t for t, st_, d1, d2 in read_mid(dict(paths)[0]) if st_ & 0xF0 == 0x90 and d2 > 0)
    at = []
    for k in range(bars * 8):
        at += ["--at", round(k * spb / 2, 4), f"t1_cutoff={round(1500 * 2 ** (1.1 * np.sin(2 * np.pi * k / 64 - np.pi / 2)))}"]
    throws = []
    for b in range(3, bars, 4):
        last = [t for t in ons if b * 4 * spb <= t < (b + 1) * 4 * spb]
        if last: throws.append(last[-1])
    for t in throws:
        at += ["--at", round(t - 0.01, 4), "t1_delay=0.95", "--at", round(t - 0.01, 4), "delay_fb=0.72",
               "--at", round(t + spb / 2, 4), "t1_delay=0.5", "--at", round(t + 3 * spb, 4), "delay_fb=0.55"]
    st = render_layer(tmp, wd, paths, 0, "devicenti", dv_project("Dub Throws", tmp, tape_noise=0), bpm, focus=1, tail=6.0,
                      sets={"delay_time": 4}, extra=at)
    sub = render_layer(tmp, wd, paths, 1, "atmd", "taurus_low", bpm, tail=3.0, sets=dict(SUB_8, osc3_range=4))
    mx.add("layer1_chord", st, -22, hpf=140, send={"verb": -12}, duck_on=ks, duck_db=4)
    mx.add("layer2_sub", sub, -20.5, lpf=160, duck_on=ks, duck_db=6)
    mx.bus("dub", "halation", "Ducked Dub", -27, duck_on=ks, duck_db=3, tail=8)
    mx.bus("verb", "halation", "Concert Hall", -28, tail=8)
    return mx.sum(), {"title": "Dub Chord", "genre": "Dub techno", "bpm": bpm, "preset": "Dub Chord",
            "point": "The full-chord pattern with silences and probability; mutation moves one step every three cycles so the stab rhythm drifts, while a second layer holds the chord root as the sub in quarters (finger pattern on index 0).",
            "instruments": "DeviceNTI V Dub Throws (layer 1, its own tape, AGE and dotted-eighth dub delay, filter sweep and delay throws automated), ATMD Taurus Low at 8' (layer 2, sub), Pallor kit, Halation Ducked Dub (rim) and Concert Hall"}


def demo_bubble(tmp, wd):
    bpm, bars = 122, 32
    inp = Song(bpm)
    prog = [["G3", "Bb3", "D4", "F4"], ["Eb3", "G3", "Bb3", "D4"], ["C3", "Eb3", "G3", "Bb3"], ["D3", "F#3", "A3", "C4"]]
    for k in range(bars // 2): inp.chord(k * 8, 8, prog[k % 4], 100)
    vel = [115, 85, 100, 90, 110, 85, 95, 88, 112, 86, 100, 92, 108, 85, 98, 90]
    preset = {"name": "Bubble", "bpm": bpm, "key": "G", "scale": "minor", "seed": 3, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "pedal", "gate": 0.4, "swing": 58,      # root low, the other notes an octave up
         "steps": [{"vel": vel[i], "oct": -2 if i % 2 == 0 else -1, "on": int(i not in (3, 11))} for i in range(16)]},
        {"ch": 2, "figure": "1/8T", "length": 12, "pattern": "up", "offset": 1, "gate": 0.35,
         "steps": [{"vel": 96 if i % 3 == 0 else 80, "on": int(i not in (5, 10))} for i in range(12)]},
    ]}
    paths = valence_layers("bubble", preset, inp, bars * 4, wd)
    d = Song(bpm)
    for b in range(bars):
        p = {"kick": "x...x...x...x...", "tamb": "o-x-o-x-o-x-o-x-"}
        if b >= 4: p["ohh"] = "..x...x...x...x."
        if b >= 8: p["clap"] = "....x.......x..."
        if b >= 12: p["rim"] = "...x.....x....x."
        if 20 <= b < 22: p.pop("kick")
        drums(d, b, p, swing=60)
    mx = Mix("bubble", bpm, bars, tmp, wd)
    ks = kicks(d)
    for k, x in pallor(tmp, wd, d, "Satin Arcade", HOUSE_KIT).items():
        mx.add("drums_" + k, x, {"Kick": -18.5, "Snare-Clap": -25, "Hats-Cymbals": -27, "Perc": -27}.get(k, -31), send={"verb": -14} if k == "Snare-Clap" else None)
    bass = render_layer(tmp, wd, paths, 0, "at101v", "rubber_bass", bpm, sets=AT101_AT_PITCH)
    mar = render_layer(tmp, wd, paths, 1, "atfs1", "glass_marimba", bpm)
    mx.add("layer1_bass", bass, -20.5, duck_on=ks, duck_db=5)
    mx.add("layer2_marimba", mar, -23, pan_=0.25, hpf=220, send={"delay": -10, "verb": -12}, duck_on=ks, duck_db=2)
    mx.bus("delay", "halation", "Swing Taps", -28)
    mx.bus("verb", "halation", "Small Room", -29)
    return mx.sum(), {"title": "Bubble", "genre": "Minimal house", "bpm": bpm, "preset": "Bubble",
            "point": "Swing and short gates: an octave-jumping bass (pedal pattern, per-step octave row) at 58 % swing and a marimba in eighth-note triplets reading the chord one note later (index offset).",
            "instruments": "AT101V Rubber Bass at 4' with its sub on the note (layer 1), ATFS1 Glass Marimba (layer 2), Pallor kit, Halation Swing Taps and Small Room"}


def demo_jack_groove(tmp, wd):
    bpm, bars = 124, 32
    inp = Song(bpm)
    dy = [["E3", "B3"], ["E3", "B3"], ["A3", "E4"], ["G3", "D4"]]
    for k in range(bars // 2): inp.chord(k * 8, 8, dy[k % 4], 100)
    ghosts = (2, 5, 7, 10, 13)
    preset = {"name": "Jack Groove", "bpm": bpm, "key": "E", "scale": "minor", "seed": 21, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "pedal", "gate": 0.5, "swing": 56,
         "steps": [{"vel": 38 if i in ghosts else (118 if i in (0, 8) else 96), "prob": 40 if i in ghosts else 100, "oct": -1,
                    "on": int(i not in (4, 12))} for i in range(16)]},
        {"ch": 2, "figure": "1/8", "length": 8, "pattern": "updown", "gate": 0.45, "swing": 56,
         "steps": [{"vel": 100 if i % 2 else 84, "on": int(i not in (0, 4))} for i in range(8)]},
    ]}
    paths = valence_layers("jack_groove", preset, inp, bars * 4, wd)
    d = Song(bpm)
    for b in range(bars):
        p = {"kick": "x...x...x...x...", "tamb": "-ox--ox--ox--ox-"}
        if b >= 4: p["clap"] = "....x.......x..."
        if b >= 8: p["chh"] = "x.x.x.x.x.x.x.x." if b % 4 != 3 else "x.x.x.x.x.x.xxxx"
        if b >= 16: p["ohh"] = "..o...o...o...o."
        if 24 <= b < 26: p = {"clap": "....x.......x...", "tamb": "-ox--ox--ox--ox-"}
        drums(d, b, p, swing=56)
    mx = Mix("jack_groove", bpm, bars, tmp, wd)
    ks = kicks(d)
    for k, x in pallor(tmp, wd, d, "Satin Arcade", dict(HOUSE_KIT, cp_model=2)).items():
        mx.add("drums_" + k, x, {"Kick": -18.5, "Snare-Clap": -24, "Hats-Cymbals": -27, "Perc": -27}.get(k, -31), send={"verb": -12} if k == "Snare-Clap" else None)
    bass = render_layer(tmp, wd, paths, 0, "atmd", "plucked_bass", bpm, tail=1.0, sets={"vel_amp": 1})
    org = render_layer(tmp, wd, paths, 1, "devicenti", "Sunday Organ", bpm, focus=3, tail=3.0, sets=dv(3))
    mx.add("layer1_bass", bass, -20, duck_on=ks, duck_db=4)
    mx.add("layer2_organ", org, -24, pan_=-0.15, hpf=200, send={"verb": -12}, duck_on=ks, duck_db=3)
    mx.bus("verb", "halation", "Small Room", -29)
    return mx.sum(), {"title": "Jack Groove", "genre": "Minimal house", "bpm": bpm, "preset": "Jack Groove",
            "point": "Ghost notes by probability and per-step velocity: the bass plays its weak steps at velocity 38 and 40 % chance, so the groove breathes; a two-note organ goes up and down in eighths.",
            "instruments": "ATMD Plucked Bass (layer 1, velocity to loudness on), DeviceNTI V Sunday Organ (layer 2), Pallor kit, Halation Small Room"}


def demo_late_shuffle(tmp, wd):
    bpm, bars = 120, 32
    inp = Song(bpm)
    roots = ["A3", "F3", "D3", "E3"]
    for k in range(bars // 2): inp.note(k * 8, 8, roots[k % 4], 100)              # one note at a time
    preset = {"name": "Late Shuffle", "bpm": bpm, "key": "A", "scale": "minor", "complete": "seventh", "seed": 9, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "up", "gate": 0.4, "swing": 60,
         "steps": [{"vel": 104 if i % 4 == 0 else 82, "on": int(i not in (6, 14))} for i in range(16)]},
        {"ch": 2, "figure": "1/8", "length": 12, "pattern": "pedal", "gate": 0.6, "swing": 60,
         "steps": [{"oct": -2, "vel": 100, "on": int(i not in (3, 9))} for i in range(12)]},
    ]}
    paths = valence_layers("late_shuffle", preset, inp, bars * 4, wd)
    d = Song(bpm)
    for b in range(bars):
        p = {"kick": "x...x...x...x...", "chh": "x.x.x.x.x.x.x.x."}
        if b >= 4: p["clap"] = "....x.......x..."
        if b >= 8: p.update(ohh="..x...x...x...x.", rim="..x....x..x...x.")
        if b >= 16: p["tamb"] = "-o-o-o-o-o-o-o-o"
        drums(d, b, p, swing=62)
    mx = Mix("late_shuffle", bpm, bars, tmp, wd)
    ks = kicks(d)
    for k, x in pallor(tmp, wd, d, "Satin Arcade", HOUSE_KIT).items():
        mx.add("drums_" + k, x, {"Kick": -18.5, "Snare-Clap": -25, "Hats-Cymbals": -27, "Perc": -28}.get(k, -31), send={"verb": -14} if k == "Snare-Clap" else None)
    keys = render_layer(tmp, wd, paths, 0, "at700", "tine_keys", bpm)
    bass = render_layer(tmp, wd, paths, 1, "atmd", "fat_bass", bpm, tail=3.0, sets=SUB_8)
    mx.add("layer1_keys", keys, -22, pan_=0.15, hpf=180, send={"delay": -10, "verb": -12}, duck_on=ks, duck_db=2)
    mx.add("layer2_bass", bass, -20.5, duck_on=ks, duck_db=4)
    mx.bus("delay", "halation", "Swing Taps", -29)
    mx.bus("verb", "halation", "Small Room", -29)
    return mx.sum(), {"title": "Late Shuffle", "genre": "Minimal house", "bpm": bpm, "preset": "Late Shuffle",
            "point": "Chord completion: the player holds one note at a time and Valence builds the seventh chord in A minor (the added notes are the neutrons), at 60 % swing.",
            "instruments": "AT700 Tine Keys (layer 1), ATMD Fat Bass at 8' (layer 2), Pallor kit, Halation Swing Taps and Small Room"}


def dnb_break(d, b, kind="two_step", fill=False):
    if kind == "two_step":
        p = {"kick": "x.........x.....", "snare": "....x.......x...", "chh": "x.x.x.x.x.x.x.x.", "rim": "......o.......o."}
        if b % 2: p["kick"] = "x.........x..x.."; p["snare"] = "....x..o....x..."
    elif kind == "heavy":
        p = {"kick": "X.........x.....", "snare": "....X..-.-..X...", "chh": "x.xox.xox.xox.xo", "ohh": "..............x."}
        if b % 2: p["kick"] = "X.x.......x....."; p["snare"] = "....X......-X.-."
    else:  # halftime: one snare per bar on beat 3
        p = {"kick": "x.........x.....", "snare": "........X.......", "chh": "x...x...x.o.x...", "rim": "..............o."}
        if b % 2: p["kick"] = "x......x..x....."
    if fill: p["snare"] = p["snare"][:12] + "xoxx"
    drums(d, b, p)


def rolling_break(d, b, fill=0, swing=56):
    """A two-step that rolls: kick on 1 and on the and of 3 (a pickup kick every other bar), snare on 2 and 4 with ghost
    notes around it, 16th hats with an accent shape and a shuffle (swing on the off 16ths, ghosts included), an open hat
    on the last off-beat every other bar. fill=1: a short snare run on beat 4; fill=2: a 32nd-note roll that builds
    over beats 3 and 4, the kick dropped."""
    odd = b % 2
    p = {"kick": "x.........x..x.." if odd else "x.........x.....",
         "snare": "....X..-.-..X.-." if odd else "....X..-.-..X..-",
         "chh": "x-o-x-oxx-o-x-.-" if odd else "x-o-x-oxx-o-x-ox",
         "ohh": "..............o." if odd else "",
         "rim": "..-........-...." if odd else ""}
    if fill == 1: p["snare"] = p["snare"][:12] + "Xoxo"
    if fill == 2:
        p["kick"] = "x.........x....."; p["snare"] = "....X..-"; p["chh"] = "x-o-x-oxx-o-o-o-"; p.pop("ohh"); p.pop("rim")
        drums(d, b, {"snare": "." * 16 + "-.-.o-o-o.x-xoxx"}, steps=32)
    drums(d, b, p, swing=swing)


def demo_liquid_rolls(tmp, wd):
    bpm, bars = 174, 48
    inp = Song(bpm)
    prog = [["F3", "A3", "C4", "E4"], ["E3", "G3", "B3", "D4"], ["D3", "F3", "A3", "C4", "E4"], ["C3", "E3", "G3", "B3"]]
    for k in range(bars // 4): inp.chord(k * 16, 16, prog[k % 4], 100)
    preset = {"name": "Liquid Rolls", "bpm": bpm, "key": "C", "scale": "major", "seed": 13, "layers": [
        {"ch": 1, "figure": "1/16", "length": 7, "pattern": "updown", "octaves": 2, "gate": 0.3, "swing": 52,
         "steps": [{"vel": 96 if i == 0 else 78} for i in range(7)]},
        {"ch": 2, "figure": "1/8T", "length": 11, "pattern": "random", "gate": 0.3,
         "steps": [{"prob": 75, "ratchet": 2 if i == 10 else 1, "vel": 86, "oct": 1} for i in range(11)]},
    ]}
    paths = valence_layers("liquid_rolls", preset, inp, bars * 4, wd)
    # The break rolls from bar 8: ghost snares, shuffled hats, fills every 4 bars and a 32nd-note roll every 8; the
    # ride joins from bar 16; bars 32-35 drop to hats and rim only, the roll before bar 36 brings the break back.
    d = Song(bpm)
    for b in range(8, bars):
        if 32 <= b < 35:
            drums(d, b, {"chh": "x-o-x-o-x-o-x-o-", "rim": "..-........-...."}, swing=56); continue
        rolling_break(d, b, fill=2 if b % 8 == 7 or b == 35 else (1 if b % 4 == 3 else 0))
        if b % 8 == 0 or b == 36: d.hit(b * 4, "crash", 100)
        if b >= 16: drums(d, b, {"ride": "x.o.x.o.x.o.x.o."}, swing=56)
    pad = Song(bpm)
    for k in range(bars // 4): pad.chord(k * 16, 15.8, prog[k % 4], 80)
    sub = Song(bpm)
    for k in range(bars // 4):
        r = sub_note(prog[k % 4][0])
        for bb in range(4):
            if k * 4 + bb < 8: continue
            sub.note((k * 4 + bb) * 4, 2.4, r, 100); sub.note((k * 4 + bb) * 4 + 2.5, 1.3, r, 90)
    mx = Mix("liquid_rolls", bpm, bars, tmp, wd)
    ks = kicks(d)
    # The ghost notes pull the snare bus loudness down: its target sits 2 dB under the old break's so the backbeat
    # lands where it was instead of 4 dB over it.
    for k, x in pallor(tmp, wd, d, "Rainy Breakbeat", LIQUID_KIT).items():
        mx.add("drums_" + k, x, {"Kick": -20, "Snare-Clap": -23.5, "Hats-Cymbals": -26, "Perc": -28}.get(k, -29), send={"verb": -12} if k == "Snare-Clap" else None)
    l1 = render_layer(tmp, wd, paths, 0, "atfs1", "glass_marimba", bpm)
    l2 = render_layer(tmp, wd, paths, 1, "devicenti", dv_project("Soft Chime", tmp, tape_noise=0), bpm, focus=4, tail=3.0)
    mx.add("layer1_bells", l1, -23, pan_=-0.25, hpf=250, send={"delay": -10, "verb": -8})
    mx.add("layer2_chime", l2, -25, pan_=0.35, hpf=300, send={"verb": -9})
    mx.add("pad", part(tmp, wd, pad, "pad", "at330", "Glass Hall", tail=4.0), -25, hpf=150, wid=1.3, duck_on=ks, duck_db=3)
    mx.add("sub", part(tmp, wd, sub, "sub", "atmd", "rubber_bass", tail=3.0, sets=SUB_8), -21, lpf=180, duck_on=ks, duck_db=3)
    mx.bus("delay", "halation", "Dotted Rain", -29)
    mx.bus("verb", "halation", "Concert Hall", -26, tail=8)
    return mx.sum(), {"title": "Liquid Rolls", "genre": "Drum and bass (liquid)", "bpm": bpm, "preset": "Liquid Rolls",
            "point": "Fast, soft layers over a rolling break: a 7-step bell line across two octaves against an 11-step eighth-triplet chime at 75 % chance with a ratchet on its last step.",
            "instruments": "ATFS1 Glass Marimba (layer 1), DeviceNTI V Soft Chime with its tape, AGE and sends (layer 2), AT330 Glass Hall pad, ATMD Rubber Bass sub at 8', Pallor rolling break (Studio 91 kit: ghost snares, shuffled hats, fills), Halation Dotted Rain and Concert Hall"}


def demo_reese_engine(tmp, wd):
    bpm, bars = 172, 48
    inp = Song(bpm)
    roots = [["F1", "C2"], ["F1", "C2"], ["Db1", "Ab1"], ["Eb1", "Bb1"]]
    for k in range(bars // 4): inp.chord(k * 16, 16, roots[k % 4], 100)
    cc = [20, 34, 50, 70, 90, 100, 84, 60, 40, 56, 76, 96, 110, 92, 64, 40]
    preset = {"name": "Reese Engine", "bpm": bpm, "key": "F", "scale": "minor", "complete": "triad", "seed": 17, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "pedal", "gate": 1.0, "swing": 50, "cc_num": 74,
         "steps": [{"vel": 110 if i in (0, 10) else 96, "ratchet": 4 if i in (7, 15) else 1, "cc": cc[i], "on": int(i not in (3, 13))} for i in range(16)]},
        {"ch": 2, "figure": "1/16", "length": 5, "pattern": "chord", "gate": 0.15, "swing": 50,
         "steps": [{"on": int(i == 0), "vel": 104, "oct": 2} for i in range(5)]},
    ]}
    paths = valence_layers("reese_engine", preset, inp, bars * 4, wd)
    d = Song(bpm)
    for b in range(bars):
        if b < 8: drums(d, b, {"chh": "x.x.x.x.x.x.x.x." if b >= 4 else "", "snare": "............xxxx" if b == 7 else ""}); continue
        if 32 <= b < 34: drums(d, b, {"chh": "x.xox.xox.xox.xo", "snare": "........" + ("-.-.o-o-" if b == 32 else "o.x.xoxX")}); continue
        dnb_break(d, b, "heavy", fill=(b % 8 == 7))
        if b % 8 == 0 or b == 34: d.hit(b * 4, "crash", 110)
    mx = Mix("reese_engine", bpm, bars, tmp, wd)
    ks = kicks(d)
    for k, x in pallor(tmp, wd, d, "Rainy Breakbeat", HARD_KIT).items():
        mx.add("drums_" + k, x, {"Kick": -19, "Snare-Clap": -19.5, "Hats-Cymbals": -25.5}.get(k, -29), send={"verb": -14} if k == "Snare-Clap" else None)
    rs = reese(tmp, wd, paths, 0, bpm)
    stab = render_layer(tmp, wd, paths, 1, "devicenti", dv_project("Hammer Stab", tmp, tape_noise=0), bpm, focus=3, tail=3.0)
    mx.add("layer1_reese", rs, -19, duck_on=ks, duck_db=4)
    mx.add("layer2_stab", stab, -24.5, pan_=0.2, hpf=250, send={"delay": -8, "verb": -10}, duck_on=ks, duck_db=2)
    mx.bus("delay", "halation", "Ping-Pong Machine", -29)
    mx.bus("verb", "halation", "Small Room", -29)
    return mx.sum(), {"title": "Reese Engine", "genre": "Drum and bass", "bpm": bpm, "preset": "Reese Engine",
            "point": "A reese bass from a pedal pattern with a CC 74 row opening and closing its filter and four-note ratchets on steps 8 and 16, against a short stab every five steps.",
            "instruments": "ATMD reese (layer 1: two saws 20 cents apart and one an octave up, overloaded mixer, CC 74 on cutoff), its top through Tarnish studio tape driven hard over a clean mono sub, DeviceNTI V Hammer Stab with its tape and AGE (layer 2), Pallor hard break (Crunch 88 kit, driven), Halation Ping-Pong Machine and Small Room"}


def demo_halftime(tmp, wd):
    bpm, bars = 172, 48
    inp = Song(bpm)
    prog = [["D3", "F3", "A3", "C4", "E4"], ["Bb2", "D3", "F3", "A3"], ["G2", "Bb2", "D3", "F3"], ["A2", "C#3", "E3", "G3"]]
    for k in range(bars // 8): inp.chord(k * 32, 32, prog[k % 4], 100)
    preset = {"name": "Halftime", "bpm": bpm, "key": "D", "scale": "minor", "seed": 23, "layers": [
        {"ch": 1, "figure": "1/8", "length": 23, "pattern": "walk", "gate": 0.55, "mutate": 2,
         "steps": [{"on": int(i % 3 != 2), "vel": 92} for i in range(23)]},
        {"ch": 2, "figure": "1/8", "length": 29, "pattern": "up", "octaves": 2, "gate": 0.55,
         "steps": [{"on": int(i % 4 in (0, 1)), "vel": 60 + (i * 7) % 31} for i in range(29)]},
    ]}
    paths = valence_layers("halftime", preset, inp, bars * 4, wd)
    d = Song(bpm)
    for b in range(4, bars):
        if 28 <= b < 30: continue
        dnb_break(d, b, "halftime", fill=(b % 8 == 7))
    sub = Song(bpm)
    for k in range(bars // 8):
        r = sub_note(prog[k % 4][0])
        for bb in range(8):
            if k * 8 + bb < 4: continue
            sub.note((k * 8 + bb) * 4, 1.8, r, 100); sub.note((k * 8 + bb) * 4 + 2.75, 0.9, r, 80)
    mx = Mix("halftime", bpm, bars, tmp, wd)
    ks = kicks(d)
    for k, x in pallor(tmp, wd, d, "Rainy Breakbeat", dict(HEAVY_BREAK_KIT, sd_tune=-3)).items():
        mx.add("drums_" + k, x, {"Kick": -19.5, "Snare-Clap": -20.5, "Hats-Cymbals": -27}.get(k, -29), send={"verb": -10} if k == "Snare-Clap" else None)
    # Two instruments in two places: a dark analog pluck, low-passed, left and close with the delay; a glassy FM bell
    # (inharmonic partials at 3.5 and 1.41), high-passed, right and wide, far back in its own hall and the big one.
    l1 = render_layer(tmp, wd, paths, 0, "at20v", "reso_pluck", bpm)
    l2 = render_layer(tmp, wd, paths, 1, "devicenti", dv_project("Glass Rain", tmp, tape_noise=0), bpm, focus=4, tail=6.0)
    mx.add("layer1_pluck", l1, -22.5, pan_=-0.25, hpf=120, lpf=1800, send={"delay": -8, "verb": -14}, duck_on=ks, duck_db=2)
    mx.add("layer2_bell", l2, -25, pan_=0.3, hpf=450, wid=1.4, send={"verb": -5}, duck_on=ks, duck_db=1)
    mx.add("sub", part(tmp, wd, sub, "sub", "atmd", "rubber_bass", tail=3.0, sets=SUB_8), -20.5, lpf=160)
    mx.bus("delay", "halation", "Dotted Rain", -28)
    mx.bus("verb", "halation", "Concert Hall", -25.5, tail=8)
    return mx.sum(), {"title": "Halftime", "genre": "Drum and bass (halftime)", "bpm": bpm, "preset": "Halftime",
            "point": "Slow, spaced layers of 23 and 29 eighths (a random walk that mutates, and a two-octave climb) that only realign every 667 steps, over a half-time beat.",
            "instruments": "AT20V Reso Pluck low-passed (layer 1, the dark pluck), DeviceNTI V Glass Rain bell with its tape, AGE and hall (layer 2), ATMD Rubber Bass sub at 8', Pallor break, Halation Dotted Rain and Concert Hall"}


def section(tmp, wd, name, preset, inp, bars, tail=6.0):
    sub = os.path.join(wd, name); os.makedirs(sub, exist_ok=True)
    return sub, valence_layers(name, preset, inp, bars * 4, sub), Mix(name, preset["bpm"], bars, tmp, sub, tail)

def gate_bars(x, bpm, keep, ramp=0.02):
    """Keep a stem only in the given bar ranges [(from, to)], with short ramps (a mute automation in the mix)."""
    g = np.zeros(len(x)); spb = 4 * 60 / bpm
    for a, b in keep:
        i, j = int(a * spb * SR), min(len(x), int(b * spb * SR)); g[i:j] = 1
    k = max(1, int(ramp * SR)); g = np.convolve(g, np.ones(k) / k, mode="same")
    return x * g[:, None]

def demo_main(tmp, wd):
    """Three genres in one piece: minimal techno into minimal house at 125, a bridge, drum and bass at 174. Every
    section change and every drum change sits on a fusion (all layers back on step 0)."""
    # A: minimal techno, 45 bars. Layers of 16, 15 and 12 sixteenths: fusion every 15 bars (15, 30, 45).
    bpmA, barsA = 125, 45
    inp = Song(bpmA); inp.chord(0, barsA * 4, ["A3", "C4", "E4", "G4"], 100)
    cc = [40, 52, 64, 76, 88, 90, 80, 68, 56, 46, 40, 48, 60, 72, 84, 70]
    pA = {"name": "Main A (techno)", "bpm": bpmA, "key": "A", "scale": "minor", "seed": 31, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "updown", "gate": 0.35, "swing": 52, "cc_num": 74,
         "steps": [{"vel": {0: 112, 4: 104, 10: 110}.get(i, 86), "cc": cc[i]} for i in range(16)]},
        {"ch": 2, "figure": "1/16", "length": 15, "pattern": "played", "gate": 0.3, "swing": 52,
         "steps": [{"on": int(i in (0, 2, 5, 7, 9, 12)), "vel": 90, "prob": 100 if i in (0, 5, 9) else 70, "oct": 1} for i in range(15)]},
        {"ch": 3, "figure": "1/16", "length": 12, "pattern": "random", "gate": 0.3, "swing": 52,
         "steps": [{"on": int(i in (0, 3, 5, 8, 10)), "vel": 84, "oct": 1 if i % 2 == 0 else 0} for i in range(12)]},
    ]}
    subA, pathsA, mA = section(tmp, wd, "A_techno", pA, inp, barsA)
    d = Song(bpmA)
    for b in range(barsA):
        p = {"kick": "x...x...x...x...", "chh": "..x...x...x...x."} if b >= 4 else {"chh": "..x...x...x...x." if b >= 2 else ""}
        if b >= 15: p.update(clap="....x.......x...", chh="..x.-.x...x.-.xo")
        if b >= 30: p.update(ohh="..x...x...x...x.", chh="o.x.o.x.o.x.o.xo", rim="...x..x....x..x.")
        if b in (15, 30): p["crash"] = "x..............."
        drums(d, b, p)
    bass = Song(bpmA)
    for b in range(15, barsA):
        for i in (2, 6, 10, 14): bass.note(b * 4 + i * 0.25, 0.2, "A1", 100 if i == 2 else 88)
    ks = kicks(d)
    for k, x in pallor(tmp, subA, d, "Pressure Plant", TECHNO_KIT).items():
        mA.add("drums_" + k, x, {"Kick": -19, "Snare-Clap": -25, "Hats-Cymbals": -26}.get(k, -29), send={"verb": -14} if k != "Kick" else None)
    mA.add("bass", part(tmp, subA, bass, "bass", "atmd", "rubber_bass", tail=3.0, sets=SUB_8), -24, lpf=900, duck_on=ks, duck_db=5)
    mA.add("layer1", render_layer(tmp, subA, pathsA, 0, "devicenti", "Rainy Pluck", bpmA, focus=4, tail=3.0, sets=dv(4)), -22, pan_=-0.3, hpf=180, send={"delay": -8, "verb": -14}, duck_on=ks, duck_db=3)
    mA.add("layer2", render_layer(tmp, subA, pathsA, 1, "devicenti", "Glass Pluck", bpmA, focus=4, tail=3.0, sets=dv(4)), -24, pan_=0.4, hpf=250, send={"delay": -6, "verb": -12}, duck_on=ks, duck_db=3)
    mA.add("layer3", render_layer(tmp, subA, pathsA, 2, "atfs1", "glass_marimba", bpmA), -25, pan_=0.1, hpf=250, send={"verb": -10}, duck_on=ks, duck_db=3)
    mA.bus("delay", "halation", "Dotted Rain", -26, duck_on=ks, duck_db=4)
    mA.bus("verb", "halation", "Small Room", -29)
    a = mA.sum()

    # B: minimal house at the same tempo, 32 bars from bar 45. Layers of 16 sixteenths, 12 eighth-triplets and 7
    # eighths: fusion every 7 bars (7, 14, 21, 28); bars 28-31 are the bridge into drum and bass.
    bpmB, barsB = 125, 32
    prog = [["A3", "C4", "E4", "G4"], ["F3", "A3", "C4", "E4"], ["D3", "F3", "A3", "C4"], ["E3", "G3", "B3", "D4"]]
    inp = Song(bpmB)
    for k in range(barsB // 2): inp.chord(k * 8, 8, prog[k % 4], 100)
    vel = [115, 85, 100, 90, 110, 85, 95, 88, 112, 86, 100, 92, 108, 85, 98, 90]
    pB = {"name": "Main B (house)", "bpm": bpmB, "key": "A", "scale": "minor", "seed": 32, "layers": [
        {"ch": 1, "figure": "1/16", "length": 16, "pattern": "pedal", "gate": 0.4, "swing": 58,      # root low, the other notes an octave up
         "steps": [{"vel": vel[i], "oct": -2 if i % 2 == 0 else -1, "on": int(i not in (3, 11))} for i in range(16)]},
        {"ch": 2, "figure": "1/8T", "length": 12, "pattern": "up", "offset": 1, "gate": 0.35,
         "steps": [{"vel": 96 if i % 3 == 0 else 80, "on": int(i not in (5, 10))} for i in range(12)]},
        {"ch": 3, "figure": "1/8", "length": 7, "pattern": "chord", "gate": 0.2, "swing": 58,
         "steps": [{"on": int(i in (0, 4)), "vel": 96, "prob": 100 if i == 0 else 90} for i in range(7)]},
    ]}
    subB, pathsB, mB = section(tmp, wd, "B_house", pB, inp, barsB)
    d = Song(bpmB)
    for b in range(barsB):
        p = {"kick": "x...x...x...x...", "tamb": "o-x-o-x-o-x-o-x-", "ohh": "..x...x...x...x."}
        if b >= 14: p.update(clap="....x.......x...", rim="...x.....x....x.")
        if b == 14: p["crash"] = "x..............."
        if b >= 28: p = {"tamb": "o-x-o-x-o-x-o-x-" if b < 30 else ""}
        drums(d, b, p, swing=60)
    ks = kicks(d)
    for k, x in pallor(tmp, subB, d, "Satin Arcade", HOUSE_KIT).items():
        mB.add("drums_" + k, x, {"Kick": -18.5, "Snare-Clap": -25, "Hats-Cymbals": -27, "Perc": -27}.get(k, -31), send={"verb": -14} if k == "Snare-Clap" else None)
    l1 = render_layer(tmp, subB, pathsB, 0, "at101v", "rubber_bass", bpmB, sets=AT101_AT_PITCH)
    l1 = tag(gate_bars(l1, bpmB, [(0, 28)]), l1.mid)
    l2 = render_layer(tmp, subB, pathsB, 1, "atfs1", "glass_marimba", bpmB)
    l3 = render_layer(tmp, subB, pathsB, 2, "devicenti", "Dub Throws", bpmB, focus=1, tail=3.0, sets=dv(1))
    mB.add("layer1_bass", l1, -20.5, duck_on=ks, duck_db=5)
    mB.add("layer2_marimba", l2, -23, pan_=0.25, hpf=220, send={"delay": -10, "verb": -12}, duck_on=ks, duck_db=2)
    mB.add("layer3_stab", l3, -25, pan_=-0.2, hpf=200, send={"dub": -4, "verb": -10}, duck_on=ks, duck_db=3)
    mB.bus("delay", "halation", "Swing Taps", -28)
    mB.bus("dub", "halation", "Ducked Dub", -27, tail=8)
    mB.bus("verb", "halation", "Small Room", -29)
    b_mix = mB.sum()
    # The bridge: the last four bars of the house layers through Murmur, swelling into the cut.
    spbB = 4 * 60 / bpmB
    src = b_mix[int(26 * spbB * SR): int(30 * spbB * SR)]
    cloud = fx("murmur", src, tmp, "main_bridge", "reverse_swell", bpmB, tail=2.0)
    cloud = at_lufs(hp(cloud, 200), -24) * np.linspace(0.3, 1.0, len(cloud))[:, None] ** 2

    # C: drum and bass at 174, 49 bars. Layers of 7 and 16 sixteenths and 12 eighth-triplets: fusion every 7 bars.
    # The hats come in on the first fusion (7), the break and the reese drop on the second (14), the breakdown on 35
    # and the last drop on 42.
    bpmC, barsC = 174, 49
    progC = [["F3", "A3", "C4", "E4"], ["E3", "G3", "B3", "D4"], ["D3", "F3", "A3", "C4", "E4"], ["A2", "C3", "E3", "G3", "B3"]]
    inp = Song(bpmC)
    for k in range((barsC + 3) // 4): inp.chord(k * 16, min(16, barsC * 4 - k * 16), progC[k % 4], 100)
    ccC = [20, 34, 50, 70, 90, 100, 84, 60, 40, 56, 76, 96, 110, 92, 64, 40]
    pC = {"name": "Main C (drum and bass)", "bpm": bpmC, "key": "A", "scale": "minor", "seed": 33, "layers": [
        {"ch": 1, "figure": "1/16", "length": 7, "pattern": "updown", "octaves": 2, "gate": 0.3, "swing": 52,
         "steps": [{"vel": 96 if i == 0 else 78} for i in range(7)]},
        {"ch": 2, "figure": "1/16", "length": 16, "pattern": "pedal", "gate": 1.0, "cc_num": 74,
         "steps": [{"vel": 110 if i in (0, 10) else 96, "ratchet": 4 if i == 15 else 1, "cc": ccC[i], "oct": -2, "on": int(i not in (3, 13))} for i in range(16)]},
        {"ch": 3, "figure": "1/8T", "length": 12, "pattern": "random", "gate": 0.3,
         "steps": [{"prob": 100 if i == 0 else 75, "ratchet": 2 if i == 11 else 1, "vel": 86, "oct": 1} for i in range(12)]},
    ]}
    subC, pathsC, mC = section(tmp, wd, "C_dnb", pC, inp, barsC, tail=8.0)
    d = Song(bpmC)
    for b in range(barsC):
        if b < 7: continue
        if b < 14 or 35 <= b < 42:
            drums(d, b, {"chh": "x.x.x.x.x.x.x.x.", "snare": "............xxxx" if b in (13, 41) else ""}); continue
        if b < 28: rolling_break(d, b, fill=2 if b % 7 == 6 else 0)
        else: dnb_break(d, b, "heavy", fill=(b % 7 == 6))
        if b in (14, 21, 28, 42): d.hit(b * 4, "crash", 110)
        if b >= 21: drums(d, b, {"ride": "x.o.x.o.x.o.x.o."}, swing=56 if b < 28 else 50)
    pad = Song(bpmC)
    for k in range((barsC + 3) // 4): pad.chord(k * 16, min(15.8, barsC * 4 - k * 16), progC[k % 4], 80)
    # A clean mono sub under the reese (which plays an octave up): the chord root between D1 and C#2 (37-69 Hz), on the
    # two-step kick (1 and the and of 3), only in the bars where the reese plays.
    subs = Song(bpmC)
    for b in list(range(14, 35)) + list(range(42, barsC)):
        r = 26 + (m(progC[(b // 4) % 4][0]) - 26) % 12
        subs.note(b * 4, 2.4, r, 100); subs.note(b * 4 + 2.5, 1.4, r, 90)
    ks = kicks(d)
    for k, x in pallor(tmp, subC, d, "Rainy Breakbeat", LIQUID_KIT).items():
        mC.add("drums_" + k, x, {"Kick": -20, "Snare-Clap": -22.5, "Hats-Cymbals": -26, "Perc": -28}.get(k, -29), send={"verb": -12} if k == "Snare-Clap" else None)
    rs = reese(tmp, subC, pathsC, 1, bpmC, osc1_range=4, osc2_range=4, osc3_range=5)     # its notes reach A0: one octave up (55-87 Hz)
    rs = tag(gate_bars(rs, bpmC, [(14, 35), (42, barsC)]), rs.mid)
    mC.add("layer1_bells", render_layer(tmp, subC, pathsC, 0, "atfs1", "glass_marimba", bpmC), -23, pan_=-0.25, hpf=250, send={"delay": -10, "verb": -8})
    mC.add("layer2_reese", rs, -20, duck_on=ks, duck_db=4)
    mC.add("sub", part(tmp, subC, subs, "sub", "atmd", "rubber_bass", tail=3.0, sets=SUB_8), -21, lpf=100, wid=0.0, duck_on=ks, duck_db=3)
    mC.add("layer3_chime", render_layer(tmp, subC, pathsC, 2, "devicenti", "Soft Chime", bpmC, focus=4, tail=3.0, sets=dv(4)), -25, pan_=0.35, hpf=300, send={"verb": -6})
    mC.add("pad", part(tmp, subC, pad, "pad", "at330", "Glass Hall", tail=4.0), -24, hpf=150, wid=1.3, duck_on=ks, duck_db=3)
    mC.bus("delay", "halation", "Dotted Rain", -29)
    mC.bus("verb", "halation", "Concert Hall", -26, tail=8)
    c = mC.sum()

    # Placement: A from 0, B on A's last fusion (bar 45), the bridge over B's bars 26-30, C on B's bar 32.
    tA = 0; tB = int(barsA * 4 * 60 / bpmA * SR); tBr = tB + int(26 * spbB * SR); tC = tB + int(barsB * spbB * SR)
    n = tC + len(c)
    out = np.zeros((n, 2))
    out[tA:tA + len(a)] += fit(a, min(len(a), n - tA)) if len(a) <= n - tA else a[:n - tA]
    out[tB:tB + len(b_mix)] += b_mix[:n - tB]
    out[tBr:tBr + len(cloud)] += cloud[:n - tBr]
    out[tC:tC + len(c)] += c
    info = {"title": "Valence main demo", "genre": "Minimal techno, minimal house, drum and bass", "bpm": "125 / 174", "preset": "Main A, Main B, Main C",
            "point": "Three genres in one piece: a held chord drifting in techno, house layers with swing and a 7-bar cycle, a Murmur bridge, then drum and bass where the break and the reese drop exactly on a fusion.",
            "instruments": "A: DeviceNTI V Rainy Pluck and Glass Pluck, ATFS1 Glass Marimba, ATMD bass, Pallor techno kit; B: AT101V Rubber Bass, ATFS1 Glass Marimba, DeviceNTI V Dub Throws, Pallor house kit; bridge: Murmur Reverse Swell; C: ATFS1 Glass Marimba, ATMD reese with CC 74, its top through Tarnish driven hard, an ATMD mono sub on the chord root, DeviceNTI V Soft Chime, AT330 Glass Hall, Pallor rolling break then hard break; Halation throughout; loud master by Lacquer",
            "sections": {"A_techno": {"start_s": 0.0, "bars": barsA, "bpm": bpmA, "fusions_bars": [15, 30, 45]},
                         "B_house": {"start_s": round(tB / SR, 3), "bars": barsB, "bpm": bpmB, "fusions_bars": [7, 14, 21, 28]},
                         "C_dnb": {"start_s": round(tC / SR, 3), "bars": barsC, "bpm": bpmC, "fusions_bars": [7, 14, 21, 28, 35, 42],
                                   "drop_s": round((tC + 14 * 4 * 60 / bpmC * SR) / SR, 3)}}}
    return out, info


DEMOS = [("01_phase_drift", demo_phase_drift), ("02_acid_shell", demo_acid_shell), ("03_dub_chord", demo_dub_chord),
         ("04_bubble", demo_bubble), ("05_jack_groove", demo_jack_groove), ("06_late_shuffle", demo_late_shuffle),
         ("07_liquid_rolls", demo_liquid_rolls), ("08_reese_engine", demo_reese_engine), ("09_halftime", demo_halftime)]


# ── Checks by analysis ──
def check_midi(wd):
    out = {}
    for f in sorted(os.listdir(wd)):
        if not (f.startswith("layer") and f.endswith(".mid")): continue
        spans, hanging, dup = note_spans(read_mid(os.path.join(wd, f)))
        out[f] = {"notes": len(spans), "hanging": len(hanging), "double_note_on": len(dup)}
    return out

def stem_reports(sd, wd):
    """Loudness, peak and brightness of every stem, and clicks away from its own note-ons (a cut note, a jump)."""
    src = json.load(open(os.path.join(sd, "sources.json")))
    out = {}
    for f in sorted(os.listdir(sd)):
        if not f.endswith(".wav"): continue
        k = f[:-4]; x = load(os.path.join(sd, f))
        on = [t for t, st, d1, d2 in read_mid(os.path.join(wd, src[k])) if st & 0xF0 == 0x90 and d2 > 0] if k in src else None
        out[k] = stem_report(x, on)
    return out


def check_bars(wd, bpm, beats_per_bar=4):
    """Polymeter by the notes: the bars of the layers (step position, layer, note) and how many are distinct before one
    repeats an earlier bar exactly."""
    notes = json.load(open(os.path.join(wd, "notes.json")))["notes"]
    bars = {}
    for e in notes:
        b = int(e["beat"] // beats_per_bar)
        bars.setdefault(b, []).append((round(e["beat"] - b * beats_per_bar, 3), e["layer"], e["note"]))
    keys = [tuple(sorted(bars.get(b, []))) for b in range(max(bars) + 1)] if bars else []
    seen = {}; first_repeat = None
    for b, k in enumerate(keys):
        if k in seen and k and first_repeat is None: first_repeat = (seen[k], b)
        seen.setdefault(k, b)
    return {"bars": len(keys), "distinct_bars": len(set(k for k in keys if k)), "first_exact_repeat": first_repeat}


def check_repeat(x, bpm, bars_from=0, bars_to=None, bars_per=1):
    """How alike consecutive bars are (correlation of their envelopes in 16 bands): 1.0 = identical loop."""
    spb = int(4 * 60 / bpm * SR) * bars_per
    mono = x.mean(axis=1)
    n = (len(mono) // spb) if bars_to is None else bars_to // bars_per
    feats = []
    for k in range(bars_from // bars_per, n):
        seg = mono[k * spb:(k + 1) * spb]
        if len(seg) < spb: break
        w = spb // 64
        spec = np.abs(np.fft.rfft(seg[:64 * w].reshape(64, w) * np.hanning(w), axis=1))
        bands = np.array_split(spec, 16, axis=1)
        feats.append(np.log1p(np.stack([b.sum(axis=1) for b in bands], axis=1)).ravel())
    sims = [float(np.corrcoef(a, b)[0, 1]) for a, b in zip(feats, feats[1:])]
    return {"bar_to_bar_similarity_mean": round(float(np.mean(sims)), 4), "max": round(float(np.max(sims)), 4), "n": len(sims)} if sims else {}


def checks_for(wd, sd, bpm):
    layers = [load(os.path.join(sd, f)) for f in sorted(os.listdir(sd)) if f.startswith("layer") and f.endswith(".wav")]
    n = max(len(x) for x in layers)
    return {"midi": check_midi(wd), "bars": check_bars(wd, bpm), "layers_repeat": check_repeat(sum(fit(x, n) for x in layers), bpm),
            "stems": stem_reports(sd, wd)}

def report(name, info, checks):
    print(f"{name}: {info['secs']} s, {info['lufs']} LUFS, true peak {info['true_peak']} dBTP, limiter {info['limiter_db']} dB", flush=True)
    for sec, c in checks.items():
        print(f"  [{sec}] {c['bars']} {c['layers_repeat']}")
        for k, v in c["stems"].items(): print(f"    {k:24s} {v}")
        print("    midi", c["midi"], flush=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    idx_path = os.path.join(OUT, "index.json")
    index = json.load(open(idx_path)) if os.path.exists(idx_path) else {}
    if INDEX_ONLY: return write_index(index)
    with tempfile.TemporaryDirectory() as tmp:
        for name, fn in DEMOS + [("10_main", demo_main)]:
            if ONLY and name not in ONLY: continue
            wd = os.path.join(OUT, name); os.makedirs(wd, exist_ok=True)
            x, info = fn(tmp, wd)
            x = hp(x, 25, 4)                        # rumble under the kick
            pre = measure(x)
            y, gr = to_target(x, -16.0, -1.0)
            y = fade(y, 0.0, 1.5)
            write(y, os.path.join(wd, name), aac=(name == "10_main"))
            info.update(measure(y)); info["limiter_db"] = round(gr, 1); info["mix_peak_before"] = pre["true_peak"]
            if name == "10_main":
                # The loud version: Lacquer's punchy style to its loud target, then the same true-peak guard.
                loud, rep = lacquer(x * db(-16.0 - lufs(x)), tmp, name, style="punchy", intensity=65, target="loud", ceiling=-1.0)
                loud, lgr = to_target(loud, lufs(loud), -1.0)
                loud = fade(loud, 0.0, 1.5)
                write(loud, os.path.join(wd, name + "_loud"), aac=True)
                info["loud"] = dict(measure(loud), lacquer=rep.get("result"))
                secs = [(k, os.path.join(wd, k), os.path.join(STEMS, k) if STEMS else os.path.join(wd, k, "stems"), info["sections"][k]["bpm"]) for k in info["sections"]]
                checks = {k: checks_for(w, sd, bpm) for k, w, sd, bpm in secs}
            else:
                sd = os.path.join(STEMS, name) if STEMS else os.path.join(wd, "stems")
                checks = {name: checks_for(wd, sd, info["bpm"])}
            info["checks"] = checks
            if CLIPPED: info["clipped_renders"] = dict(CLIPPED); print("  CLIPPED renders:", CLIPPED); CLIPPED.clear()
            info["layers_from"] = sorted({json.load(open(os.path.join(r, f)))["source"] for r, _, fs in os.walk(wd) for f in fs if f == "notes.json"})
            if os.environ.get("VALENCE_COMMIT"): info["valence_commit"] = os.environ["VALENCE_COMMIT"]
            json.dump(checks, open(os.path.join(wd, "checks.json"), "w"), indent=1)
            index[name] = info
            report(name, info, checks)
            json.dump(index, open(idx_path, "w"), indent=1, ensure_ascii=False)
    write_index(index)


def commits(index):
    """ (Valence at commit X; A and B at commit Y)." from each demo's valence_commit, the most common first."""
    by = {}
    for name, _ in DEMOS + [("10_main", None)]:
        if name in index and index[name].get("valence_commit"): by.setdefault(index[name]["valence_commit"], []).append(index[name]["title"])
    if not by: return "."
    order = sorted(by, key=lambda c: -len(by[c]))
    return f" (Valence at commit {order[0]}" + "".join(f"; {', '.join(by[c])} at commit {c}" for c in order[1:]) + ")."

def write_index(index):
    lines = ["# Valence: demos for listening (phase 1)", "",
             "Every note of the arpeggio comes from Valence: an input file (what the player holds) and a preset go through",
             "`valencerender`, which writes one MIDI file per layer; each layer is then played by one of our synths with its",
             "render tool, mixed in Python with drums from Pallor (MIDI written in `plugins/valence/tools/demos.py`), bass and",
             "pads, and effects from Halation and Tarnish. Loudness −16 LUFS integrated (BS.1770), true peak under −1 dBTP",
             "(a look-ahead limiter only where the peak would pass; the column says how much). WAV 24-bit 48 kHz and MP3.",
             "Each folder keeps `input.mid`, `preset.json`, `layerN.mid`, `all.mid`, `events.json` (every note with its layer,",
             "time, velocity and source nucleon, the CCs, chords and fusions, as valencerender wrote it), `notes.json` and",
             "`checks.json`; the stems (one WAV per layer, part and effect return, 32-bit float) stay on the build machine.",
             "The Valence preset of each demo is its own `preset.json` (values from `docs/research.md` 2.10, every layer on its own",
             "channel and with New Chord = Keep phase so the polymeter survives chord changes); factory presets with the same",
             "name may differ. Stems: `~/valence-demos/out/stems/` on pc50-wsl.",
             f"Layers from: {', '.join(sorted({x for i in index.values() for x in i.get('layers_from', [])}))}" + commits(index), "",
             "| Demo | Genre | BPM | Valence preset | Length | LUFS | True peak | Sample peak | Limiter | What it shows |",
             "|---|---|---|---|---|---|---|---|---|---|"]
    for name, _ in DEMOS + [("10_main", None)]:
        if name not in index: continue
        i = index[name]
        lines.append(f"| `{name}/{name}.wav` | {i['genre']} | {i['bpm']} | {i['preset']} | {i['secs']} s | {i['lufs']} | {i['true_peak']} dBTP | {i['sample_peak']} dBFS | {i['limiter_db']} dB | {i['point']} |")
    if "10_main" in index:
        i = index["10_main"]; sec = i["sections"]
        mmss = lambda t: f"{int(t // 60)}:{t % 60:04.1f}"
        lines += ["", "## Main demo", "",
                  f"- Minimal techno at {sec['A_techno']['bpm']} BPM from 0:00 ({sec['A_techno']['bars']} bars; layers of 16, 15 and 12 sixteenths, fusions on bars "
                  + ", ".join(map(str, sec['A_techno']['fusions_bars'])) + "; the clap and the open hats enter on the first two, the house starts on the third).",
                  f"- Minimal house at {sec['B_house']['bpm']} BPM from {mmss(sec['B_house']['start_s'])} ({sec['B_house']['bars']} bars; layers of 16 sixteenths, 12 eighth-triplets"
                  " and 7 eighths, fusions every 7 bars; bars 28-31 are the bridge, the house layers through Murmur).",
                  f"- Drum and bass at {sec['C_dnb']['bpm']} BPM from {mmss(sec['C_dnb']['start_s'])} ({sec['C_dnb']['bars']} bars; layers of 7 and 16 sixteenths and 12"
                  f" eighth-triplets, fusions every 7 bars: hats on bar 7, the break and the reese drop on bar 14 at {mmss(sec['C_dnb']['drop_s'])}, breakdown on 35, last drop on 42).",
                  f"- Loud version `10_main/10_main_loud.wav` (also .mp3 and .m4a): Lacquer, punchy style, loud target: {i['loud']['lufs']} LUFS,"
                  f" true peak {i['loud']['true_peak']} dBTP, sample peak {i['loud']['sample_peak']} dBFS. The −16 LUFS version also has an .m4a."]
    lines += ["", "## Instruments", ""]
    for name, _ in DEMOS + [("10_main", None)]:
        if name in index: lines.append(f"- **{index[name]['title']}**: {index[name]['instruments']}.")
    open(os.path.join(OUT, "index.md"), "w").write("\n".join(lines) + "\n")
    print(os.path.join(OUT, "index.md"))


if __name__ == "__main__":
    main()
