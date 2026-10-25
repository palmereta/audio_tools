"""DeviceNTI V: the seven demos of the phase 1 checkpoint (plan §13.4; genre-recipes.md blueprints §1.6 … §6.6).

Each demo is a project file (patterns + a song chain) written to tools/demo_projects/, rendered by the C++ engine
through devicentirender (demos.py). The seventh, the controller jam, is a MIDI performance (notes, sustain, knob CCs,
pitch bend, mod wheel) played into the same engine, with no sequencer.
"""
import json, os
from presets import *  # noqa: F401,F403

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO_DIR = os.path.join(HERE, "demo_projects")


def copy_bars(src, dst, t, bars, at_bar, lanes=None):
    """Copy the steps of `bars` bars of track t of a 16-step template into dst starting at bar `at_bar`."""
    for (lane, i), st in src.tracks[t]["steps"].items():
        if lanes is not None and lane not in lanes:
            continue
        for b in range(bars):
            s2 = json.loads(json.dumps(st)); s2["i"] = i + 16 * (at_bar + b)
            dst.tracks[t]["steps"][(lane, s2["i"])] = s2


def grid_bar(fn, variant):
    p = Pattern(99, 16); fn(p, 0, variant); return p


# ─────────────────────────── 1. Trip hop ───────────────────────────
def trip_hop():
    root, sc = NOTE["A"], SCALE["minor"]
    kitd = kit_bristol(0.6)
    bass = bass_heavy_detuned(0.42, 0.1, 440.0)
    ep = ep_tine(0.25, 3400.0, "lofi", 0.3)
    ep.update({"ins_p1": 0.25, "ins_p2": 0.2, "ins_mix": 0.55, "send_reverb": 0.42, "send_delay": 0.12})
    pad = pad_dusty(True)
    lead = lead_theremin()
    ep_dark = dict(ep); ep_dark.update({"cutoff": 2200.0, "b_level": 0.14, "age": 0.85})
    pats = []
    prog_v = [(0, 5), (0, 5), (5, 4), (4, 4, "b9")]          # Am9 Am9 Fmaj7 E7b9
    prog_h = [(3, 5), (3, 5), (5, 4), (4, 4, "b9")]          # Dm9 Dm9 Fmaj7 E7b9
    # P0 intro (4 bars): hats + rim, kick from bar 3; EP once per bar; pad swell opening by cutoff locks.
    P = Pattern(0, 64, root, sc)
    for b in range(4):
        P.grid(0, 3, "x . o . x . o . x . o . x . o .", offset=16 * b, vel=70)
        P.grid(0, 2, ". . . . . . . . . . . . . . o .", offset=16 * b)
        if b >= 2:
            P.grid(0, 0, "X . . . . . . x . . X . . . . .", offset=16 * b)
        P.chord(2, 16 * b, 0, 4, 5, 0, VC["drop2"], len=15 * 12, vel=72)
    P.chord(3, 0, 0, 3, 3, 0, VC["spread"], len=64 * 12 - 6, vel=70)
    for b, c in enumerate([700, 1100, 1600, 2200]):
        P.lock(3, 16 * b, cutoff=float(c))
    pats.append(P)
    # P1 verse (4 bars, played twice): TH-A, 2-bar variation by A:B, fill on the second pass's last bar.
    P = Pattern(1, 64, root, sc)
    for b in range(4):
        copy_bars(grid_bar(drums_th, "A"), P, 0, 1, b)
    P.trig(0, 16 + 6, lane=0, vel=96, cond=("ab", 2, 2)); P.trig(0, 48 + 15, lane=1, vel=46, cond=("ab", 2, 2))
    P.trig(0, 61, lane=1, vel=70, retrig=2, retrigCount=3, retrigRamp=45, cond="fill")
    for b, c in enumerate(prog_v):
        P.trig(1, 16 * b + 0, notes=degree_note(root, sc, c[0], 1), vel=112, len=5 * 12)
        P.trig(1, 16 * b + 6, notes=degree_note(root, sc, c[0], 1), vel=96, len=3 * 12)
        P.trig(1, 16 * b + 10, notes=degree_note(root, sc, c[0] + 2, 1), vel=100, len=5 * 12)
        P.chord(2, 16 * b, c[0], 4, c[1], 0, VC["drop2"], flags=CF.get(c[2], 0) if len(c) > 2 else 0, len=14 * 12, vel=80)
        P.chord(2, 16 * b + 11, c[0], 4, c[1], 0, VC["drop2"], flags=CF.get(c[2], 0) if len(c) > 2 else 0, len=4 * 12, vel=58)
    P.chord(3, 0, 0, 3, 3, 0, VC["spread"], len=32 * 12 - 6, vel=54); P.chord(3, 32, 5, 3, 3, 0, VC["spread"], len=32 * 12 - 6, vel=54)
    P.lock(3, 0, level=-14.0); P.lock(3, 32, level=-14.0)
    pats.append(P)
    # P2 heavy (4 bars ×2): TH-C half-time, the bass driven harder (locks), EP with more delay, lead on the 2nd pass.
    P = Pattern(2, 64, root, sc)
    for b in range(4):
        copy_bars(grid_bar(drums_th, "C"), P, 0, 1, b)
    P.trig(0, 0, lane=7, vel=100)                                   # vinyl pop at the section start
    for b, c in enumerate(prog_h):
        for (i, ln, v, dd) in [(0, 4, 118, 0), (6, 2, 100, 0), (10, 2, 104, -1), (13, 3, 100, 0)]:
            P.trig(1, 16 * b + i, notes=degree_note(root, sc, c[0] + dd, 1), vel=v, len=ln * 12)
            P.lock(1, 16 * b + i, drive=0.62, fold=0.22)
        P.chord(2, 16 * b, c[0], 4, c[1], 0, VC["drop2"], flags=CF.get(c[2], 0) if len(c) > 2 else 0, len=15 * 12, vel=78)
        P.lock(2, 16 * b, send_delay=0.32)
    for (i, deg, ln) in [(32, 4, 3), (35, 6, 3), (38, 8, 4), (44, 7, 4), (48, 4, 6), (56, 3, 4), (60, 4, 4)]:
        P.trig(3, i, notes=degree_note(root, sc, deg, 4), vel=96, len=int(ln * 12), cond=("ab", 2, 2))
    pats.append(P)
    # P3 return (4 bars): TH-B, bass every second bar, EP on the darker patch (patch swap).
    P = Pattern(3, 64, root, sc)
    for b in range(4):
        copy_bars(grid_bar(drums_th, "B"), P, 0, 1, b)
    for b, c in enumerate(prog_v):
        if b % 2 == 0:
            P.trig(1, 16 * b, notes=degree_note(root, sc, c[0], 1), vel=110, len=8 * 12)
        s = P.chord(2, 16 * b, c[0], 4, c[1], 0, VC["drop2"], flags=CF.get(c[2], 0) if len(c) > 2 else 0, len=15 * 12, vel=80); s["patch"] = 0
    pats.append(P)
    # P4 outro (2 bars): hats only, the last chord rings into the reverb, the tape gets noisier.
    P = Pattern(4, 32, root, sc)
    for b in range(2):
        P.grid(0, 3, "x . o . x . o . x . o . x . o .", offset=16 * b, vel=62)
    s = P.chord(2, 0, 0, 4, 5, 0, VC["drop2"], len=30 * 12, vel=76); s["patch"] = 0
    P.lock(2, 0, send_reverb=0.75)
    P.lock(0, 0, lane=3, g_tape_noise=0.8)
    pats.append(P)
    tracks = [kit_track(kitd), tone(bass), tone(ep), tone(pad)]
    proj = project("Demo 1: Trip Hop", "Demo", 900, "84 BPM, A minor: dusty kit, heavy detuned FM bass with sub, tine EP through lo-fi and spring, dusty pad, a whistling lead; vinyl.",
                   84, 58, tracks, pats, chain=[(0, 1), (1, 2, 1), (2, 2), (3, 1), (4, 1)],
                   fx=fxs("tape", "spring", delay_tone=0.42, delay_wow=0.35, reverb_damp=0.55, tape_noise=0.5, delay_to_reverb=0.25),
                   params={"t1_level": 1.5, "t2_level": -4.5, "t3_level": -1.0, "t4_level": -5.0, "delay_time": 7, "delay_fb": 0.4,
                           "reverb_size": 0.45, "reverb_decay": 2.4, "tape": 0.5, "glue": 0.35, "limit": 0.25},
                   patches=[ep_dark, lead])
    # T4 plays the pad in P0/P1 and the lead (patch 1) in P2: a patch swap per step.
    for st in proj["patterns"][2]["tracks"][3]["steps"]:
        st["patch"] = 1
    return proj, 26 * 4 * 60 / 84


# ─────────────────────────── 2. Downtempo ───────────────────────────
def downtempo():
    root, sc = NOTE["F"], SCALE["major"]
    kitd = kit_round()
    kitd["0"].update({"age": 0.3})
    bass = bass_rolling()
    pad = pad_warm()
    pluck = pluck_dub(); pluck.update({"send_delay": 0.4, "level": -2.0})
    prog = [(0, 5), (2, 4), (5, 5), (4, 3, "sus4")]               # Fmaj9 Am7 Dm9 Csus4
    prog_b = [(3, 5), (2, 4), (5, 5), (4, 4)]                     # Bbmaj9 Am7 Dm9 C7
    pats = []

    def bassline(P, prg, drive=None):
        for b, c in enumerate(prg):
            for (i, ln, v, dd) in [(0, 1.5, 112, 0), (3, 0.8, 78, 0), (6, 1.5, 100, 0), (10, 1.0, 92, 2), (12, 2.5, 100, 4)]:
                P.trig(1, 16 * b + i, notes=degree_note(root, sc, c[0] + dd, 1), vel=v, len=int(ln * 12), slide=1 if i == 10 else 0)
                if drive:
                    P.lock(1, 16 * b + i, drive=drive)

    def pads(P, prg, vel=74):
        for b, c in enumerate(prg):
            P.chord(2, 16 * b, c[0], 3, c[1], CT.get(c[2], 0) if len(c) > 2 else 0, VC["spread"], len=16 * 12 - 4, vel=vel)

    P = Pattern(0, 64, root, sc)                                   # intro: shaker + rim, pad swelling open
    for b in range(4):
        copy_bars(grid_bar(drums_dt, "A"), P, 0, 1, b, lanes={6})
        P.grid(0, 2, ". . . . x . . . . . . . x . . .", offset=16 * b, vel=70)
        P.lock(2, 16 * b, cutoff=float([600, 1000, 1600, 2400][b]))
    P.chord(2, 0, 0, 3, 5, 0, VC["spread"], len=64 * 12 - 6, vel=70)
    pats.append(P)
    P = Pattern(1, 64, root, sc)                                   # A: groove, bass, pads; the pluck motif on the second pass
    for b in range(4):
        copy_bars(grid_bar(drums_dt, "A"), P, 0, 1, b)
    P.trig(0, 61, lane=1, vel=70, retrig=3, retrigCount=3, retrigRamp=40, cond="fill")
    bassline(P, prog); pads(P, prog)
    for (i, deg, ln, v) in [(0, 4, 1.5, 100), (3, 2, 1, 84), (6, 4, 2, 96), (16, 5, 1.5, 96), (19, 4, 1, 80), (22, 2, 3, 92),
                            (32, 4, 1.5, 100), (35, 2, 1, 84), (38, 4, 2, 96), (48, 6, 1.5, 100), (51, 4, 1, 84), (54, 2, 4, 90)]:
        P.trig(3, i, notes=degree_note(root, sc, deg, 4), vel=v, len=int(ln * 12), cond=("ab", 2, 2))
    pats.append(P)
    P = Pattern(2, 64, root, sc)                                   # B: ride, bass a little driven, motif an octave up
    for b in range(4):
        copy_bars(grid_bar(drums_dt, "B"), P, 0, 1, b)
    bassline(P, prog_b, drive=0.42); pads(P, prog_b, 80)
    for b in range(4):
        P.lock(2, 16 * b, send_reverb=0.55)
    for (i, deg, ln, v) in [(0, 7, 1.5, 100), (3, 6, 1, 84), (6, 4, 2, 96), (16, 9, 1.5, 96), (19, 7, 1, 80), (22, 6, 3, 92),
                            (32, 7, 1.5, 100), (35, 6, 1, 84), (38, 4, 2, 96), (48, 8, 2, 100), (52, 7, 1, 84), (54, 4, 4, 90)]:
        P.trig(3, i, notes=degree_note(root, sc, deg, 4), vel=v, len=int(ln * 12))
        if i in (16, 48):
            P.lock(3, i, send_delay=0.6)
    pats.append(P)
    P = Pattern(3, 64, root, sc)                                   # out: no kick, bass stops, the last chord opens, a delay throw
    for b in range(4):
        copy_bars(grid_bar(drums_dt, "A"), P, 0, 1, b, lanes={1, 3, 6, 7})
    bassline(P, prog[:2])
    P.chord(2, 0, 0, 3, 5, 0, VC["spread"], len=64 * 12 - 6, vel=78); P.lock(2, 0, send_reverb=0.7)
    for (i, deg) in [(0, 4), (3, 2), (6, 4)]:
        P.trig(3, i, notes=degree_note(root, sc, deg, 4), vel=96, len=18)
    P.trig(3, 38, notes=degree_note(root, sc, 4, 5), vel=100, len=24)
    P.lock(3, 38, send_delay=0.75, g_delay_fb=0.88)
    pats.append(P)
    tracks = [kit_track(kitd), tone(bass), tone(pad), tone(pluck)]
    proj = project("Demo 2: Downtempo", "Demo", 901, "92 BPM, F major: round kit with shaker, rolling bass over a sub, warm fan pads with AGE, an FM pluck into the dub delay.",
                   92, 55, tracks, pats, chain=[(0, 1), (1, 2, 1), (2, 2), (3, 1)],
                   fx=fxs("dub", "hall", delay_tone=0.42, delay_return=0.7, reverb_return=0.55, reverb_damp=0.45, tape_noise=0.2, delay_to_reverb=0.3),
                   params={"t1_level": 1.5, "t2_level": -2.0, "t3_level": -8.5, "t4_level": 2.0, "delay_time": 7, "delay_fb": 0.58,
                           "reverb_size": 0.6, "reverb_decay": 3.8, "tape": 0.3, "glue": 0.3, "limit": 0.15})
    return proj, 24 * 4 * 60 / 92


# ─────────────────────────── 3. Acid jazz ───────────────────────────
def acid_jazz():
    root, sc = NOTE["D"], SCALE["dorian"]
    kitd = kit_club()
    kitd["6"] = k_ride(-10.0); kitd["3"] = k_brush(-4.0) | {"lane_note": 39}
    bass = bass_slap()
    ep = ep_tine(0.32, 4200.0, "phaser", 0.22)
    organ = organ_drawbar(0.25)
    organ.update({"level": -11.0})
    brass = brass_stab(True)
    pats = []
    prog = [(0, 5), (3, 6), (6, 5), (4, 4, "b9")]                 # Dm9 G13 Cmaj9 A7b9
    comp = [(3, 1.0, 92), (6, 0.8, 78), (10, 1.0, 88), (13, 0.8, 72)]

    def comping(P, prg):
        for b, c in enumerate(prg):
            for (i, ln, v) in comp:
                P.chord(2, 16 * b + i, c[0], 4, c[1], 0, VC["drop2"], flags=CF.get(c[2], 0) if len(c) > 2 else 0, len=int(ln * 12), vel=v)

    P = Pattern(0, 64, root, sc)                                   # intro: brushes and ride, EP rubato Dm9 → G13
    for b in range(4):
        copy_bars(grid_bar(drums_aj, "C"), P, 0, 1, b)
        P.grid(0, 2, ". . o . . . o . . . o . X . o .", offset=16 * b, vel=56)
    P.chord(2, 0, 0, 4, 5, 0, VC["drop2"], len=30 * 12, vel=80); P.chord(2, 32, 3, 4, 6, 0, VC["drop2"], len=30 * 12, vel=78)
    for st in P.tracks[2]["steps"].values():
        st["micro"] = 4
    pats.append(P)
    P = Pattern(1, 64, root, sc)                                   # head
    for b in range(4):
        copy_bars(grid_bar(drums_aj, "A"), P, 0, 1, b)
    P.trig(0, 16 + 15, lane=1, vel=120, cond=("ab", 2, 2)); P.trig(0, 48 + 7, lane=7, vel=96, cond=("ab", 2, 2))
    for b, c in enumerate(prog):
        for (i, ln, v, dd) in [(0, 0.8, 118, 0), (3, 0.5, 86, 7), (6, 0.8, 102, 0), (8, 0.4, 64, 2), (10, 0.8, 106, 4), (14, 0.5, 90, 6)]:
            P.trig(1, 16 * b + i, notes=degree_note(root, sc, c[0] + dd, 2), vel=v, len=int(ln * 12))
    comping(P, prog)
    P.chord(3, 0, 0, 3, 6, CT["quartal"], VC["close"], len=64 * 12 - 6, vel=70)
    pats.append(P)
    P = Pattern(2, 64, root, sc)                                   # solo: ride, walking bass, stabs, brass on the second pass
    for b in range(4):
        copy_bars(grid_bar(drums_aj, "B"), P, 0, 1, b)
    walk = [0, 1, 2, 3, 4, 3, 2, 1]
    for b, c in enumerate(prog):
        for k in range(8):
            P.trig(1, 16 * b + 2 * k, notes=degree_note(root, sc, c[0] + walk[k], 2), vel=104 if k % 2 == 0 else 88, len=20)
    comping(P, prog)
    for b, c in enumerate(prog):
        for i in (2, 9):
            s = P.chord(3, 16 * b + i, c[0], 4, 3, 0, VC["close"], len=6, vel=110, cond=("ab", 2, 2)); s["patch"] = 0
    pats.append(P)
    P = Pattern(3, 64, root, sc)                                   # out: the groove thins to hats, the last chord rings
    for b in range(2):
        copy_bars(grid_bar(drums_aj, "A"), P, 0, 1, b)
    for b in range(2, 4):
        P.grid(0, 3, "X o x o X o x o X o x o X o x o", offset=16 * b, vel=72)
    for b, c in enumerate(prog[:2]):
        for (i, ln, v, dd) in [(0, 0.8, 116, 0), (6, 0.8, 100, 0), (10, 0.8, 104, 4)]:
            P.trig(1, 16 * b + i, notes=degree_note(root, sc, c[0] + dd, 2), vel=v, len=int(ln * 12))
    P.chord(2, 32, 6, 4, 5, 0, VC["drop2"], len=30 * 12, vel=84); P.lock(2, 32, send_reverb=0.62)
    pats.append(P)
    tracks = [kit_track(kitd), tone(bass), tone(ep), tone(organ)]
    proj = project("Demo 3: Acid Jazz", "Demo", 902, "100 BPM, D Dorian: live-feeling kit with ride and brushes, slap FM bass, tine EP comping ninths and thirteenths from chord mode, an FM organ, brass stabs.",
                   100, 60, tracks, pats, chain=[(0, 1), (1, 2), (2, 2), (3, 1)],
                   fx=fxs("pingpong", "plate", delay_tone=0.6, reverb_damp=0.4, tape_noise=0.12),
                   params={"t1_level": 1.5, "t2_level": -2.0, "t3_level": -3.5, "t4_level": -8.0, "delay_time": 4, "delay_fb": 0.28,
                           "reverb_size": 0.35, "reverb_decay": 1.7, "glue": 0.45, "limit": 0.2},
                   patches=[brass])
    return proj, 24 * 4 * 60 / 100


# ─────────────────────────── 4. Ambient ───────────────────────────
def ambient():
    root, sc = NOTE["D"], SCALE["major"]
    pad = pad_evolving()
    pad.update({"amp_att": 2.5, "level": -9.0})
    drone = choir_drone()
    bells = bell_glass(); bells.update({"send_delay": 0.3})
    kitd = kit_tick()
    tex = texture_ring()
    pats = []
    P = Pattern(0, 64, root, sc)                                   # emergence: Dsus2 swelling open, ticks
    P.chord(0, 0, 0, 4, 3, CT["sus2"], VC["spread"], len=64 * 12 - 6, vel=80)
    for b, c in enumerate([500, 900, 1400, 2000]):
        P.lock(0, 16 * b, cutoff=float(c))
    P.grid(3, 7, ". . . . . . . . x . . . . . . .", offset=0, vel=40, prob=40)
    P.grid(3, 7, ". . . . x . . . . . . . x . . .", offset=32, vel=36, prob=40)
    pats.append(P)
    P = Pattern(1, 64, root, sc)                                   # bloom: Gmaj7#11, drone D+A, bells on 11 and 13 steps
    P.chord(0, 0, 3, 4, 4, 0, VC["spread"], flags=CF["#11"], len=64 * 12 - 6, vel=80)
    P.chord(1, 0, 0, 2, 2, 0, VC["close"], len=64 * 12 - 6, vel=70)
    P.tracks[2]["length"] = 11; P.tracks[2]["speed"] = 2
    for (i, deg) in [(0, 4), (5, 8), (8, 6)]:
        P.trig(2, i, notes=degree_note(root, sc, deg, 5), vel=72, len=40, prob=60)
    P.grid(3, 0, "X . . . . . . . . . . . . . . .", offset=0, vel=60, cond=("ab", 1, 2))
    P.grid(3, 0, "X . . . . . . . . . . . . . . .", offset=32, vel=60, cond=("ab", 1, 2))
    for o in (8, 24, 40, 56):
        P.trig(3, o, lane=7, vel=34, prob=40)
    pats.append(P)
    P = Pattern(2, 64, root, sc)                                   # peak: Dsus2 | Asus4, wider, shimmer up; noise texture
    P.chord(0, 0, 0, 4, 3, CT["sus2"], VC["spread"], len=32 * 12 - 6, vel=84); P.chord(0, 32, 4, 4, 3, CT["sus4"], VC["spread"], len=32 * 12 - 6, vel=84)
    P.lock(0, 0, unison=4, g_reverb_shimmer=0.6); P.lock(0, 32, unison=4, g_reverb_shimmer=0.6)
    P.chord(1, 0, 0, 2, 2, 0, VC["close"], len=64 * 12 - 6, vel=74)
    P.tracks[2]["length"] = 13; P.tracks[2]["speed"] = 2
    for (i, deg) in [(0, 9), (4, 7), (7, 11), (10, 8)]:
        P.trig(2, i, notes=degree_note(root, sc, deg, 5), vel=70, len=40, prob=55)
    pats.append(P)
    P = Pattern(3, 64, root, sc)                                   # decay: the last chord with a long release, bells alone, freeze
    P.chord(0, 0, 0, 4, 3, CT["sus2"], VC["spread"], len=20 * 12, vel=76)
    P.lock(0, 0, amp_rel=6.0)
    P.tracks[2]["length"] = 11; P.tracks[2]["speed"] = 2
    for (i, deg) in [(0, 4), (6, 2)]:
        P.trig(2, i, notes=degree_note(root, sc, deg, 5), vel=64, len=40, prob=70)
    P.trig(3, 16, lane=7, vel=30, len=40 * 12)
    P.lock(3, 16, lane=7, g_reverb_freeze=1.0)
    pats.append(P)
    # T4 is a KIT for the ticks and kicks; the noise texture plays as a patch on T2's slot in the peak (patch swap).
    tracks = [tone(pad), tone(drone), tone(bells), kit_track(kitd)]
    proj = project("Demo 4: Ambient", "Demo", 903, "70 BPM, D major: an evolving fan pad, an additive drone, glass bells on 11- and 13-step loops with probability, ticks; shimmer and a long hall, freeze at the end.",
                   70, 50, tracks, pats, chain=[(0, 1), (1, 2), (2, 1), (3, 1)],
                   fx=fxs("pingpong", "hall", delay_tone=0.5, reverb_shimmer=0.3, reverb_damp=0.35, reverb_mod=0.5, tape_noise=0.15, delay_to_reverb=0.4),
                   params={"t1_level": -3.0, "t2_level": 1.0, "t3_level": 3.0, "t4_level": -6.0, "delay_time": 8, "delay_fb": 0.5,
                           "reverb_size": 0.85, "reverb_decay": 10.0, "tape": 0.25},
                   patches=[tex])
    # The texture (patch 0) joins on T2 in the peak, an octave up, quietly.
    pats2 = proj["patterns"][2]["tracks"][1]["steps"]
    pats2.append({"lane": 0, "i": 32, "notes": [degree_note(root, sc, 4, 4)], "vel": 60, "len": 32 * 12 - 6, "patch": 0})
    return proj, 20 * 4 * 60 / 70


# ─────────────────────────── 5. Dark ambient ───────────────────────────
def dark_ambient():
    root, sc = NOTE["D"], SCALE["phrygian"]
    drone = resonator_drone()
    rumble = sub_rumble()
    pad = pad_cluster(); pad.update({"send_reverb": 0.65})
    kitd = kit_dark()
    kitd["0"].update({"hum_time": 1.0})
    for l in "12345678":
        if l in kitd:
            kitd[l]["hum_time"] = 1.0
    pats = []
    # Bars at 60 BPM are 4 s. Patterns of 4 bars (64 steps), and humanised timing on the kit.
    P = Pattern(0, 48, root, sc)                                   # hum (3 bars): the drone fades in
    P.chord(0, 0, 0, 2, 2, CT["fifths"], VC["close"], len=48 * 12, vel=80)
    for b, lv in enumerate([-30.0, -18.0, -9.0]):
        P.lock(0, 16 * b, level=lv)
    pats.append(P)
    P = Pattern(1, 64, root, sc)                                   # descent (4 bars): sub from bar 2, impacts, metal 50 %
    P.chord(0, 0, 0, 2, 2, CT["fifths"], VC["close"], len=64 * 12, vel=80)
    P.lock(0, 0, level=-9.0)
    P.trig(1, 16, notes=n("D1"), vel=100, len=48 * 12)
    P.trig(3, 16, lane=1, vel=120)
    for o in (11, 27, 43, 59):
        P.trig(3, o, lane=6, vel=90, prob=50, micro=-9)
    pats.append(P)
    P = Pattern(2, 64, root, sc)                                   # presence (4 bars ×1.5): fifth via chord mode, rumble driven, cluster pad, heartbeat
    P.chord(0, 0, 0, 2, 3, CT["fifths"], VC["close"], len=64 * 12, vel=84)
    P.lock(0, 0, level=-8.0)
    P.trig(1, 0, notes=n("D1"), vel=104, len=64 * 12); P.lock(1, 0, drive=0.45)
    P.chord(2, 0, 0, 3, 3, CT["cluster"], VC["close"], len=64 * 12, vel=80)
    for b in range(4):
        P.trig(3, 16 * b, lane=0, vel=70, micro=3); P.trig(3, 16 * b + 2, lane=0, vel=50, micro=-2, prob=70)
        P.trig(3, 16 * b + 11, lane=6, vel=86, prob=50)
    pats.append(P)
    P = Pattern(3, 48, root, sc)                                   # climax (3 bars): resonator feedback up, fold, cutoff opens, impact + metal each bar
    P.chord(0, 0, 0, 2, 3, CT["fifths"], VC["close"], len=48 * 12, vel=90)
    P.lock(0, 0, level=-6.0, ins_amt=0.97, cutoff=1200.0)
    P.trig(1, 0, notes=n("D1"), vel=110, len=48 * 12); P.lock(1, 0, drive=0.62, fold=0.25)
    s = P.chord(2, 0, 1, 3, 3, CT["cluster"], VC["close"], len=48 * 12, vel=86); P.lock(2, 0, cutoff=1200.0)
    for b in range(3):
        P.trig(3, 16 * b, lane=1, vel=120); P.trig(3, 16 * b + 8, lane=6, vel=100); P.trig(3, 16 * b + 12, lane=7, vel=90)
        P.trig(3, 16 * b, lane=0, vel=86)
    pats.append(P)
    P = Pattern(4, 32, root, sc)                                   # void (2 bars): freeze, everything else out, one far hit
    P.trig(3, 0, lane=4, vel=60, len=32 * 12); P.lock(3, 0, lane=4, g_reverb_freeze=1.0)
    P.trig(3, 20, lane=1, vel=70)
    pats.append(P)
    tracks = [tone(drone), tone(rumble), tone(pad), kit_track(kitd)]
    proj = project("Demo 5: Dark Ambient", "Demo", 904, "60 BPM grid, no groove, D Phrygian: a feedback drone through a comb tuned to the root, a sub rumble, a cluster pad, impacts and metallic FM hits; void reverb, freeze.",
                   60, 50, tracks, pats, chain=[(0, 1), (1, 1), (2, 2), (3, 1), (4, 1)],
                   fx=fxs("dub", "void", delay_tone=0.3, reverb_damp=0.7, reverb_mod=0.55, tape_noise=0.35, delay_to_reverb=0.45),
                   params={"t1_level": 3.0, "t2_level": -3.0, "t3_level": -8.0, "t4_level": -3.0, "delay_time": 6, "delay_fb": 0.72,
                           "reverb_size": 0.9, "reverb_decay": 22.0, "tape": 0.35, "duck": 0.2})
    return proj, (3 + 4 + 8 + 3 + 2) * 4.0


# ─────────────────────────── 6. Heavy ───────────────────────────
def heavy():
    root, sc = NOTE["E"], SCALE["phrygian"]
    kitd = kit_anvil()
    bass = bass_growl(0.6, 0.25, 680.0)
    stab = stab_hard(True)
    sub = bass_sub(); sub.update({"level": -2.0})
    pats = []
    riff = [(0, 2.5, 120, 0), (6, 1.5, 100, 2), (9, 1.5, 110, 0), (12, 2, 105, 3)]       # E . . . . G . . E . . A
    P = Pattern(0, 32, root, sc)                                   # hit (2 bars): impact, then a kick; sub held
    P.trig(0, 0, lane=7, vel=120); P.trig(0, 16, lane=0, vel=127)
    P.trig(3, 0, notes=n("E1"), vel=110, len=32 * 12)
    pats.append(P)
    P = Pattern(1, 64, root, sc)                                   # riff (4 bars): HV-A, the bass riff, the sub follows
    for b in range(4):
        copy_bars(grid_bar(drums_hv, "A"), P, 0, 1, b)
        for (i, ln, v, dd) in riff:
            P.trig(1, 16 * b + i, notes=degree_note(root, sc, dd, 1), vel=v, len=int(ln * 12))
        P.trig(3, 16 * b, notes=n("E1"), vel=104, len=16 * 12 - 2)
    pats.append(P)
    P = Pattern(2, 64, root, sc)                                   # hard (4 bars): HV-B, growl pushed (locks), stabs on step 15, a delay throw in bar 2
    for b in range(4):
        copy_bars(grid_bar(drums_hv, "B"), P, 0, 1, b)
        for (i, ln, v, dd) in riff:
            P.trig(1, 16 * b + i, notes=degree_note(root, sc, dd, 1), vel=v, len=int(ln * 12))
            P.lock(1, 16 * b + i, drive=0.78, fold=0.38)
        P.chord(2, 16 * b + 14, 0, 3, 2, CT["power"], VC["close"], len=8, vel=120)
        P.trig(3, 16 * b, notes=n("E1"), vel=104, len=16 * 12 - 2)
    P.lock(2, 16 + 14, send_delay=0.5)
    pats.append(P)
    P = Pattern(3, 64, root, sc)                                   # break (4 bars): kick and metal; the bass out, back with a filter sweep; sub up an octave at the end
    for b in range(4):
        P.grid(0, 0, "X . . . . . . . . . X . . . . .", offset=16 * b)
        P.trig(0, 16 * b + 6, lane=6, vel=90, prob=70)
        P.chord(2, 16 * b, 0, 3, 2, CT["power"], VC["close"], len=8, vel=110)
    for b in (2, 3):
        for (i, ln, v, dd) in riff:
            P.trig(1, 16 * b + i, notes=degree_note(root, sc, dd, 1), vel=v, len=int(ln * 12))
            P.lock(1, 16 * b + i, cutoff=float(250 + 140 * (16 * (b - 2) + i)))
    P.trig(3, 0, notes=n("E1"), vel=100, len=48 * 12); P.trig(3, 48, notes=n("E1"), vel=104, len=16 * 12); P.lock(3, 48, tune=12.0)
    pats.append(P)
    P = Pattern(4, 32, root, sc)                                   # end (2 bars): impact, the last stab with a throw
    P.trig(0, 0, lane=7, vel=120); P.trig(0, 0, lane=0, vel=127)
    P.chord(2, 0, 0, 3, 2, CT["power"], VC["close"], len=10, vel=124); P.lock(2, 0, send_delay=0.6, g_delay_fb=0.8)
    P.trig(3, 0, notes=n("E1"), vel=100, len=24 * 12)
    pats.append(P)
    tracks = [kit_track(kitd), tone(bass), tone(stab), tone(sub)]
    proj = project("Demo 6: Heavy", "Demo", 905, "75 BPM half-time, E Phrygian: folded kick, crushed snare, a growl FM bass driven and folded over a clean sub, hard crushed stabs; dub delay throws.",
                   75, 52, tracks, pats, chain=[(0, 1), (1, 1), (2, 2), (3, 1), (1, 1), (4, 1)],
                   fx=fxs("dub", "plate", delay_tone=0.45, reverb_damp=0.5, tape_noise=0.2, delay_return=0.8),
                   params={"t1_level": -2.5, "t2_level": -0.5, "t3_level": -4.0, "t4_level": -2.0, "delay_time": 6, "delay_fb": 0.55,
                           "reverb_size": 0.3, "reverb_decay": 1.8, "tape": 0.4, "glue": 0.6, "limit": 0.5, "duck": 0.0})
    return proj, (2 + 4 + 8 + 4 + 4 + 2) * 4 * 60 / 75


# ─────────────────────────── 7. Controller jam (MIDI in) ───────────────────────────
def jam_project():
    """Four tracks on MIDI channels 1-4, no patterns: everything comes from the MIDI performance."""
    kitd = kit_bristol(0.5)
    bass = bass_heavy_detuned(0.38, 0.08, 520.0)
    ep = ep_tine(0.25, 3200.0, "chorus", 0.3)
    lead = lead_theremin(); lead.update(mod(1, "wheel", "pitch", 0.03)); lead.update({"lfo1_depth": 0.0})
    tracks = [kit_track(kitd, in_channel=1), tone(bass, in_channel=2), tone(ep, in_channel=3), tone(lead, in_channel=4)]
    return project("Demo 7: Controller Jam", "Demo", 906, "The same engine played from a controller: finger drums, bass, EP with sustain and knob moves, a lead with pitch bend and mod wheel.",
                   88, 50, tracks, [Pattern(0)], fx=fxs("tape", "spring", delay_tone=0.45, tape_noise=0.35),
                   params={"t1_level": -2.0, "t2_level": -3.0, "t3_level": -4.0, "t4_level": -6.0, "delay_time": 7, "delay_fb": 0.4,
                           "reverb_size": 0.45, "reverb_decay": 2.4, "tape": 0.4, "glue": 0.3, "limit": 0.2})


def jam_midi(path, bpm=88.0, bars=20, seed=7):
    """A 'recorded' performance: humanised timing (±12 ms) and velocities, sustain, CC 74/71/91/77 moves, bend, wheel."""
    import random
    import mido
    rnd = random.Random(seed)
    tpb = 480
    beat = 60.0 / bpm
    ev = []                                                   # (seconds, message)
    root = n("A1")

    def t_of(bar, step):
        return (bar * 16 + step) * beat / 4 + rnd.gauss(0, 0.006)

    def note(ch, nn, vel, t0, dur):
        ev.append((max(0.0, t0), mido.Message("note_on", channel=ch, note=nn, velocity=max(1, min(127, vel)))))
        ev.append((max(0.0, t0 + dur), mido.Message("note_off", channel=ch, note=nn, velocity=0)))

    chords = [[57, 60, 64, 67, 71], [57, 60, 64, 67, 71], [53, 57, 60, 64], [52, 56, 59, 62, 65]]   # Am9 Am9 Fmaj7 E7b9 (voiced)
    roots = [33, 33, 29, 28]
    for bar in range(bars):
        c = bar % 4
        intro = bar < 2
        # Finger drums (ch 1, GM-like notes): boom-bap with ghosts; enters fully at bar 2.
        for s in range(16):
            if s % 2 == 0:
                note(0, 42, rnd.randint(50, 80) if s % 4 else rnd.randint(70, 92), t_of(bar, s), 0.05)
            if not intro:
                if s in (0, 7, 10):
                    note(0, 36, rnd.randint(105, 125), t_of(bar, s), 0.1)
                if s == 8:
                    note(0, 38, rnd.randint(108, 124), t_of(bar, s) + 0.012, 0.1)
                if s in (11, 15) and rnd.random() < 0.6:
                    note(0, 38, rnd.randint(36, 54), t_of(bar, s), 0.05)
        if bar % 4 == 3 and not intro:
            for k, s in enumerate((13, 14, 15)):
                note(0, 38, 60 + 15 * k, t_of(bar, s), 0.05)
        # Bass (ch 2).
        if bar >= 2:
            r = roots[c]
            note(1, r, rnd.randint(100, 118), t_of(bar, 0), beat * 1.3)
            note(1, r, rnd.randint(88, 100), t_of(bar, 6), beat * 0.6)
            note(1, r + (3 if c < 2 else 4), rnd.randint(92, 106), t_of(bar, 10), beat * 1.2)
        # EP (ch 3) with the sustain pedal, chords rolled by hand.
        ev.append((t_of(bar, 0) - 0.02, mido.Message("control_change", channel=2, control=64, value=127)))
        for k, nn in enumerate(chords[c]):
            note(2, nn, rnd.randint(62, 84), t_of(bar, 0) + 0.011 * k, beat * 1.6)
        if bar % 2 == 1:
            for k, nn in enumerate(chords[c][1:]):
                note(2, nn, rnd.randint(48, 64), t_of(bar, 11) + 0.009 * k, beat * 0.5)
        ev.append((t_of(bar, 15), mido.Message("control_change", channel=2, control=64, value=0)))
        # Lead (ch 4) from bar 8: a phrase with bends and the wheel.
        if 8 <= bar < 16:
            ph = [(0, 76, 3), (6, 79, 2), (10, 83, 4), (16, 81, 6), (24, 79, 3), (28, 76, 4)]
            base = (bar - 8) % 2
            for (s, nn, ln) in ph:
                if (s >= 16) == bool(base):
                    note(3, nn, rnd.randint(88, 104), t_of(bar, s % 16), ln * beat / 4)
    # Knob moves (CC on the track channels): EP cutoff sweep, bass resonance, reverb on the EP, drive on the bass.
    T = bars * 4 * beat
    for k in range(200):
        t = k / 200 * T
        u = 0.5 - 0.5 * __import__("math").cos(2 * 3.14159 * t / (8 * 4 * beat))
        ev.append((t, mido.Message("control_change", channel=2, control=74, value=int(40 + 70 * u))))
        if k % 4 == 0:
            ev.append((t, mido.Message("control_change", channel=1, control=77, value=int(30 + 60 * (t / T)))))
    for k in range(40):
        t = (8 + k / 40 * 8) * 4 * beat
        ev.append((t, mido.Message("control_change", channel=2, control=91, value=int(50 + 50 * k / 40))))
    # Bends and wheel on the lead.
    for bar in range(8, 16):
        t0 = t_of(bar, 10)
        for k in range(12):
            ev.append((t0 + k * 0.01, mido.Message("pitchwheel", channel=3, pitch=int(-2000 + 2000 * k / 11))))
        for k in range(30):
            ev.append((t_of(bar, 0) + k * 0.03, mido.Message("control_change", channel=3, control=1, value=int(min(127, k * 5)) if bar % 2 else 0)))
    ev.sort(key=lambda e: e[0])
    mf = mido.MidiFile(ticks_per_beat=tpb)
    tr = mido.MidiTrack(); mf.tracks.append(tr)
    tr.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(bpm), time=0))
    last = 0
    for (t, m) in ev:
        tick = int(round(t / beat * tpb))
        m.time = max(0, tick - last); last = max(last, tick)
        tr.append(m)
    mf.save(path)
    return bars * 4 * beat


DEMOS = [("01-trip-hop", trip_hop, -16.0), ("02-downtempo", downtempo, -16.0), ("03-acid-jazz", acid_jazz, -16.0), ("04-ambient", ambient, -20.0),
         ("05-dark-ambient", dark_ambient, -20.0), ("06-heavy", heavy, -16.0)]


def write_all():
    os.makedirs(DEMO_DIR, exist_ok=True)
    out = []
    for slug, fn, target in DEMOS:
        proj, secs = fn()
        path = os.path.join(DEMO_DIR, slug + ".json")
        with open(path, "w") as f:
            json.dump(proj, f, indent=1)
        out.append((slug, path, secs, target, proj))
    jp = jam_project()
    path = os.path.join(DEMO_DIR, "07-controller-jam.json")
    with open(path, "w") as f:
        json.dump(jp, f, indent=1)
    out.append(("07-controller-jam", path, None, -16.0, jp))
    return out


if __name__ == "__main__":
    for slug, path, secs, target, proj in write_all():
        print(slug, path, secs)
