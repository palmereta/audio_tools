"""DeviceNTI V: the demo clips for 1.0.0, short and sound-first (each shows one sound or one effect, like the other
plugins' showcase clips). Seven projects written to tools/demo_projects/ and rendered by demos.py; each is also a
factory preset (presets.py appends them to their banks), so what the clip plays is one click away in the plugin.

The trip hop clips follow one direction: slow, minor and menacing, a heavy distorted bass that pulses and swells, slow
dark pads, filtered noise and breath, sparse metallic hits far back in the reverb, tape and dub delay. Where there are
drums they are half-time and lazy: a soft deep kick and a big snare far back in the room, no hi-hats.
"""
import json, os
from presets import *  # noqa: F401,F403

HERE = os.path.dirname(os.path.abspath(__file__))
DEMO_DIR = os.path.join(HERE, "demo_projects")
TIE = -1


def glock(P, t, i, steps, **locks):
    """A lock-only step whose gate lasts `steps` (global effect locks hold for the gate of their trig)."""
    st = P.lock(t, i, **locks)
    st["len"] = int(steps * 12)
    return st


# ───────────────────────── the sounds ─────────────────────────
def bass_pulse(drive=0.6, fold=0.18, cutoff=320.0, pulse="1/8"):
    """Heavy, analog-like bass: a feedback operator for the saw edge, drive and fold over a clean sub, a resonant
    low-pass that pulses with an LFO on the beat and opens slowly with the filter envelope (the swell)."""
    s = bass_heavy_detuned(drive, fold, cutoff)
    s.update({"b_fb": 0.55, "c_fb": 0.2, "reso": 0.42, "ktrk": 0.3, "fenv": 0.42, "f_att": 0.9, "f_dec": 3.0, "f_sus": 0.35, "f_rel": 1.2,
              "amp_att": 0.04, "amp_rel": 0.7, "age": 0.7, "ins_type": INS["drive"], "ins_amt": 0.3, "ins_p1": 0.35, "ins_p2": 0.25,
              "send_delay": 0.06, "send_reverb": 0.1, "level": -10.0, "vmode": 1, "glide": 0.09, "sub_level": 0.7})
    s.update(lfo(1, "cutoff", 0.22, sync=pulse, shape="tri", mode="trig"))
    s.update(lfo(2, "drive", 0.2, sync="2bars", shape="smooth"))
    return s


def pad_dark(cutoff=650.0):
    """Slow dark pad: Diamond with detuned operators, three unison voices spread wide, a slow wandering low-pass."""
    s = pad_cluster()
    s.update({"cutoff": cutoff, "reso": 0.18, "unison": 3, "uni_detune": 14.0, "uni_spread": 1.0, "age": 0.65, "ins_type": INS["chorus"],
              "ins_amt": 0.25, "ins_p1": 0.15, "ins_p2": 0.4, "send_reverb": 0.55, "send_delay": 0.12, "level": -5.0, "amp_att": 2.5, "amp_rel": 4.0})
    s.update(lfo(2, "cutoff", 0.12, rate=0.05, shape="smooth"))
    s.update(lfo(1, "index", 0.25, sync="4bars", shape="smooth"))
    return s


def breath():
    """Filtered noise like breath: band-passed noise that follows the amp, its centre drifting slowly."""
    s = snd("CHOIR", op("a", 1.0, 0.12, 2.0, 3.0, 1.0, 3.0), ftype="LP12", cutoff=7000.0, reso=0.1, noise_level=0.85, noise_decay=10.0,
            noise_freq=2600.0, noise_q=1.2, noise_mode=1, noise_env=1, age=0.4, voices=4, **amp(1.8, 0.5, 1.0, 3.0), level=-2.0,
            send_reverb=0.6, send_delay=0.25, pan=-0.35)
    s.update(lfo(1, "noise_freq", 0.3, rate=0.09, shape="smooth"))
    s.update(lfo(2, "amp", 0.25, rate=0.17, shape="smooth"))
    return s


def metal_hit():
    """A sparse metallic hit: inharmonic fixed partials, a long ring, far back in the reverb and the dub delay."""
    s = bell_rite()
    s.update({"b_fixed": 0.0, "cutoff": 9000.0, "reso": 0.12, "age": 0.4, "send_reverb": 0.55, "send_delay": 0.4, "level": 8.0,
              "pan": 0.35, "a_dec": 3.5, "c_dec": 2.6, "amp_rel": 2.5, "ins_type": INS["ring"], "ins_amt": 0.25, "ins_p1": 0.3, "ins_p2": 0.0})
    return s


def dub_stab():
    """A dark chord stab for delay throws: short tine-like stack, lo-fi insert, high delay send."""
    s = ep_tine(0.3, 2400.0, "lofi", 0.3)
    s.update({"amp_dec": 0.35, "amp_sus": 0.0, "amp_rel": 0.25, "a_dec": 0.4, "a_sus": 0.0, "b_dec": 0.25, "send_delay": 0.5,
              "send_reverb": 0.35, "age": 0.6, "level": -6.0, "ins_mix": 0.6, "pan": -0.3, "unison": 2, "uni_spread": 0.8, "uni_detune": 8.0})
    return s


def kit_halftime():
    """Half-time, lazy: a soft deep kick, a big dark snare far back in the reverb, a quiet shaker. No hi-hats."""
    kick = k_kick(pitch=28.0, sweep=18.0, sweep_t=0.05, decay=0.75, click=0.03, fold=0.0, drive=0.08, sub=True)
    kick.update({"level": 3.0, "send_reverb": 0.06})
    snare = k_snare(pitch=49.0, tone_dec=0.26, noise=0.5, noise_dec=0.5, base=190.0, rev=0.8, drive=0.12, level=1.0)
    snare.update({"cutoff": 3200.0, "ftype": FTYPE["LP12"], "reso": 0.05, "send_delay": 0.1, "hum_time": 0.4})
    shaker = k_shaker(-14.0); shaker.update({"hum_time": 0.6, "hum_vel": 0.5, "send_reverb": 0.35})
    return kit({1: kick, 2: snare, 7: shaker},
               {"age": 0.7, "cutoff": 6500.0, "send_reverb": 0.12})


def trip_fx(**kw):
    """The trip hop room: dub delay with wow, a long dark hall, plenty of tape."""
    d = dict(delay_tone=0.38, delay_wow=0.45, reverb_damp=0.6, reverb_mod=0.4, tape_noise=0.08, delay_to_reverb=0.35)
    d.update(kw)
    return fxs("dub", "hall", **d)


def trip_params(**kw):
    p = {"delay_time": 7, "delay_fb": 0.55, "reverb_size": 0.8, "reverb_decay": 4.5, "tape": 0.45, "glue": 0.3, "limit": 0.2}
    p.update(kw)
    return p


def bars(n, bpm):
    return n * 4 * 60.0 / bpm


# ───────────────────────── the clips ─────────────────────────
def low_swell():
    """Trip hop, no drums: the pulsing bass swells under a dark pad, breath and two far metallic hits."""
    root, sc, bpm = NOTE["C"], SCALE["minor"], 76
    pats = []
    for k in range(2):
        P = Pattern(k, 64, root, sc)
        line = [(0, 0, 2), (2, 5, 1), (3, 4, 1)] if k == 0 else [(0, 0, 2), (2, 2, 1), (3, 1, 1)]   # (bar, degree, bars)
        for (b, deg, ln) in line:
            P.trig(0, 16 * b, notes=degree_note(root, sc, deg, 1), vel=112, len=ln * 16 * 12 - 6)
        for b, c in enumerate([300, 380, 470, 560] if k == 0 else [520, 650, 800, 620]):
            P.lock(0, 16 * b, cutoff=float(c))
        P.chord(1, 0, 0, 3, 3, CT["sus2"] if k == 0 else 0, VC["spread"], len=64 * 12 - 6, vel=78)
        if k == 0:
            P.trig(2, 0, notes=degree_note(root, sc, 4, 3), vel=90, len=TIE)
        P.trig(3, 22 if k == 0 else 44, notes=degree_note(root, sc, 0, 4), vel=90, len=8 * 12)
        if k == 1:
            P.lock(3, 44, send_delay=0.7, g_delay_fb=0.72)
        pats.append(P)
    tracks = [tone(bass_pulse()), tone(pad_dark()), tone(breath()), tone(metal_hit())]
    proj = project("Low Swell", "Trip Hop", 140, "No drums, C minor, 76 BPM: a heavy driven bass that pulses on the eighths and swells open, a dark pad, breath and far metallic hits in the dub delay.",
                   bpm, 50, tracks, pats, chain=[(0, 1), (1, 1)], fx=trip_fx(),
                   params=trip_params(t1_level=-1.0, t2_level=-4.0, t3_level=-3.0, t4_level=-4.0))
    return proj, bars(8, bpm), 5.0


def half_light():
    """Trip hop with sparse drums: half-time soft kick, a big snare far back, the pulsing bass, a pad, metallic throws."""
    root, sc, bpm = NOTE["F"], SCALE["minor"], 80
    pats = []
    for k in range(2):
        P = Pattern(k, 64, root, sc)
        for b in range(4):
            P.trig(0, 16 * b, lane=1, vel=96)
            if b % 2 == 0:
                P.trig(0, 16 * b + 10, lane=1, vel=74)
            P.trig(0, 16 * b + 8, lane=2, vel=104, micro=6)
            for s in range(0, 16, 2):
                if (s // 2) % 2 == 1 or k == 1:
                    P.trig(0, 16 * b + s, lane=7, vel=44 if s % 4 else 30, prob=80)
        line = [(0, 0, 2), (2, 5, 1), (3, 4, 1)] if k == 0 else [(0, 0, 1), (1, 2, 1), (2, 5, 1), (3, 4, 1)]
        for (b, deg, ln) in line:
            P.trig(1, 16 * b, notes=degree_note(root, sc, deg, 1), vel=110, len=ln * 16 * 12 - 6)
        P.chord(2, 0, 0, 3, 4, 0, VC["spread"], len=32 * 12 - 6, vel=70)
        P.chord(2, 32, 5, 3, 4, 0, VC["spread"], len=32 * 12 - 6, vel=70)
        if k == 1:
            P.trig(3, 14, notes=degree_note(root, sc, 0, 4), vel=88, len=8 * 12); P.lock(3, 14, send_delay=0.65)
            P.trig(3, 46, notes=degree_note(root, sc, 4, 4), vel=84, len=12 * 12); P.lock(3, 46, send_delay=0.75, g_delay_fb=0.7)
        pats.append(P)
    tracks = [kit_track(kit_halftime()), tone(bass_pulse(0.55, 0.15, 340.0, "1/4")), tone(pad_dark(800.0)), tone(metal_hit())]
    proj = project("Half-Light", "Trip Hop", 141, "F minor, 80 BPM, half-time: a soft deep kick and a big snare far back in the hall, the pulsing driven bass, a slow dark pad, metallic hits thrown into the dub delay.",
                   bpm, 50, tracks, pats, chain=[(0, 1), (1, 1)], fx=trip_fx(),
                   params=trip_params(t1_level=-4.0, t2_level=-1.0, t3_level=-4.0, t4_level=-4.0))
    return proj, bars(8, bpm), 5.0


def dub_throws():
    """The dub delay: a dark stab, then throws with more send and feedback until the echoes run away, then a cut."""
    root, sc, bpm = NOTE["A"], SCALE["minor"], 82
    P = Pattern(0, 64, root, sc)
    hits = [(0, 0, 0.35, 0.5), (22, 0, 0.45, 0.55), (38, 5, 0.55, 0.65), (48, 3, 0.7, 0.8), (54, 3, 0.85, 1.02)]
    for k, (i, deg, sd, fb) in enumerate(hits):
        P.chord(0, i, deg, 3, 3, 0, VC["close"], len=(10 if k == len(hits) - 1 else 4) * 12, vel=104)
        P.lock(0, i, send_delay=sd, g_delay_fb=fb)
    P.trig(1, 0, notes=degree_note(root, sc, 0, 1), vel=100, len=TIE)
    P2 = Pattern(1, 32, root, sc)                       # the echoes ring out, then the tail is pulled down
    glock(P2, 0, 0, 12, g_delay_fb=1.05); glock(P2, 0, 12, 20, g_delay_fb=0.3)
    P2.trig(1, 0, notes=degree_note(root, sc, 0, 1), vel=90, len=24 * 12)
    tracks = [tone(dub_stab()), tone(bass_pulse(0.45, 0.1, 280.0, "1/4")), tone(pad_dark()), tone(breath())]
    proj = project("Dub Throws", "Trip Hop", 142, "A minor, 82 BPM: a dark lo-fi stab thrown into the dub delay with more send and feedback each time, until the echoes run away; a low bass underneath.",
                   bpm, 50, tracks, [P, P2], chain=[(0, 1), (1, 1)], fx=trip_fx(delay_wow=0.55),
                   params=trip_params(delay_time=7, t1_level=-2.0, t2_level=-6.0, t3_level=-60.0, t4_level=-60.0))
    return proj, bars(6, bpm), 4.0


def slow_weight():
    """Dark ambient: the heavy pad alone, opening and closing over minutes compressed to half a minute, sub and breath."""
    root, sc, bpm = NOTE["D"], SCALE["phrygian"], 60
    P = Pattern(0, 64, root, sc)
    P.chord(0, 0, 0, 3, 4, CT["cluster"], VC["spread"], len=TIE, vel=84)
    for b, c in enumerate([420, 700, 1200, 1700]):
        P.lock(0, 16 * b, cutoff=float(c))
    P.trig(1, 0, notes=degree_note(root, sc, 0, 1), vel=90, len=TIE)
    P.trig(2, 8, notes=degree_note(root, sc, 4, 3), vel=88, len=TIE)
    P2 = Pattern(1, 48, root, sc)
    for b, c in enumerate([1300, 800, 450]):
        P2.lock(0, 16 * b, cutoff=float(c))
    P2.trig(3, 4, notes=degree_note(root, sc, 1, 3), vel=86)
    pad = pad_dark(500.0); pad.update({"unison": 4, "uni_detune": 18.0, "amp_rel": 6.0, "send_reverb": 0.65})
    tracks = [tone(pad), tone(sub_rumble()), tone(breath()), tone(metal_hit())]
    proj = project("Slow Weight", "Dark Ambient", 160, "D Phrygian, no pulse: a cluster pad with four unison voices opens from 420 Hz to 1.7 kHz and closes again over a sub rumble and breath; one metallic hit.",
                   bpm, 50, tracks, [P, P2], chain=[(0, 1), (1, 1)],
                   fx=fxs("pingpong", "void", delay_tone=0.4, reverb_damp=0.5, reverb_mod=0.5, tape_noise=0.3, delay_to_reverb=0.5),
                   params={"t1_level": -2.0, "t2_level": -6.0, "t3_level": -4.0, "t4_level": -6.0, "delay_time": 8, "delay_fb": 0.5,
                           "reverb_size": 0.9, "reverb_decay": 9.0, "tape": 0.4, "glue": 0.2})
    return proj, bars(7, bpm), 8.0


def frozen_tail():
    """FREEZE and SHIMMER: a chord into the hall, the shimmer rises, the tail freezes and glass bells play over it."""
    root, sc, bpm = NOTE["D"], SCALE["minor"], 70
    P = Pattern(0, 64, root, sc)
    P.chord(0, 0, 0, 4, 4, CT["add9"], VC["spread"], len=6 * 12, vel=100)
    P.lock(0, 0, g_reverb_shimmer=0.0)
    P.chord(0, 16, 5, 3, 4, CT["add9"], VC["spread"], len=6 * 12, vel=96)
    P.lock(0, 16, g_reverb_shimmer=0.5)
    P.chord(0, 32, 3, 3, 4, CT["sus2"], VC["spread"], len=6 * 12, vel=96)
    P.lock(0, 32, g_reverb_shimmer=0.8)
    glock(P, 2, 44, 20, g_reverb_freeze=1.0, g_reverb_shimmer=0.8)
    for (i, deg) in [(48, 4), (52, 6), (56, 7), (60, 9)]:
        P.trig(1, i, notes=degree_note(root, sc, deg, 5), vel=82)
    P2 = Pattern(1, 32, root, sc)
    for (i, deg) in [(0, 11), (6, 9), (12, 7)]:
        P2.trig(1, i, notes=degree_note(root, sc, deg, 5), vel=74)
    glock(P2, 2, 0, 24, g_reverb_freeze=1.0, g_reverb_shimmer=0.8)
    keys = ep_tine(0.18, 3200.0, "chorus", 0.3)
    keys.update({"send_reverb": 0.85, "send_delay": 0.1, "amp_rel": 1.2, "level": -6.0})
    bells = bell_glass(True); bells.update({"send_reverb": 0.4, "send_delay": 0.3, "level": -10.0})
    tracks = [tone(keys), tone(bells), tone(pad_warm()), tone(breath())]
    proj = project("Frozen Tail", "Ambient", 150, "D minor, 70 BPM: chords into a long hall while the shimmer rises, then FREEZE holds the tail and glass bells play over the frozen room.",
                   bpm, 50, tracks, [P, P2], chain=[(0, 1), (1, 1)],
                   fx=fxs("pingpong", "hall", delay_tone=0.5, reverb_damp=0.35, reverb_mod=0.5, tape_noise=0.15, delay_to_reverb=0.4),
                   params={"t1_level": -1.0, "t2_level": -3.0, "t3_level": -60.0, "t4_level": -60.0, "delay_time": 8, "delay_fb": 0.45,
                           "reverb_size": 0.85, "reverb_decay": 8.0, "tape": 0.25})
    return proj, bars(6, bpm), 9.0


def drive_and_fold():
    """DRIVE then FOLD on one bass line: clean, driven harder bar by bar, then folded; the sub stays clean; a slow kick."""
    root, sc, bpm = NOTE["E"], SCALE["phrygian"], 75
    pats = []
    stages = [(0.0, 0.0), (0.25, 0.0), (0.5, 0.0), (0.8, 0.0), (0.8, 0.3), (0.8, 0.55), (0.9, 0.8), (0.9, 0.8)]
    for k in range(2):
        P = Pattern(k, 64, root, sc)
        for b in range(4):
            dr, fo = stages[4 * k + b]
            for (s, deg, ln) in [(0, 0, 5), (6, 0, 2), (10, 1, 3), (14, 0, 2)]:
                P.trig(1, 16 * b + s, notes=degree_note(root, sc, deg, 1), vel=110 if s == 0 else 96, len=ln * 12)
                P.lock(1, 16 * b + s, drive=dr, fold=fo)
            P.trig(0, 16 * b, lane=1, vel=92)
        pats.append(P)
    bass = bass_growl(0.0, 0.0, 5200.0)
    bass.update({"ins_type": INS["off"], "lfo1_depth": 0.0, "unison": 1, "level": -4.0, "send_delay": 0.05, "send_reverb": 0.06,
                 "reso": 0.2, "sub_level": 0.4, "fenv": 0.15})
    k = kit_halftime()
    tracks = [kit_track(k), tone(bass), tone(pad_dark()), tone(metal_hit())]
    proj = project("Drive and Fold", "Heavy", 170, "E Phrygian, 75 BPM: one bass line, clean at first, DRIVE turned up bar by bar, then FOLD on top; the sub under it stays clean. A soft kick on the one.",
                   bpm, 50, tracks, pats, chain=[(0, 1), (1, 1)], fx=trip_fx(),
                   params=trip_params(t1_level=-6.0, t2_level=0.0, t3_level=-60.0, t4_level=-60.0, tape=0.3))
    return proj, bars(8, bpm), 3.0


def tape_chords():
    """Chord memory and AGE: four stored shapes on a tine piano, AGE rising from clean to worn tape over eight bars."""
    root, sc, bpm = NOTE["D"], SCALE["chromatic"], 88          # chromatic: the stored shapes play exactly (no scale snapping)
    mem = [[0, 3, 7, 10, 14], [0, 4, 7, 11, 14], [0, 3, 7, 10, 17], [0, 4, 7, 10, 13]]   # min9, maj9, min11, 7(b9)
    pats = []
    prog = [(n("D3"), 0), (n("G3"), 3), (n("C3"), 1), (n("A2"), 3)]
    for k in range(2):
        P = Pattern(k, 64, root, sc)
        for b, (rn, slot) in enumerate(prog):
            age = (4 * k + b) / 7.0
            st = P.chord(0, 16 * b, rn, len=10 * 12, vel=86, memory=slot)
            P.lock(0, 16 * b, age=round(age, 2), vintage=1.0 if age > 0.8 else 0.0)
            P.chord(0, 16 * b + 11, rn, len=4 * 12, vel=66, memory=slot)
            P.lock(0, 16 * b + 11, age=round(age, 2))
            P.trig(1, 16 * b, notes=rn - 12, vel=96, len=7 * 12)
            P.trig(1, 16 * b + 10, notes=rn - 12 + 7, vel=80, len=4 * 12)
        pats.append(P)
    ep = ep_tine(0.25, 3600.0, "chorus", 0.25)
    ep.update({"age": 0.0, "send_reverb": 0.3, "send_delay": 0.08, "strum": 0.012, "level": -4.0})
    tracks = [tone(ep), tone(bass_fretless()), tone(pad_warm()), tone(breath())]
    proj = project("Tape Chords", "Acid Jazz", 130, "D Dorian, 88 BPM: four chord-memory shapes (minor 9, major 9, minor 11, 7 flat 9) on a tine piano over a fretless bass, with AGE rising from clean to worn tape.",
                   bpm, 50, tracks, pats, chain=[(0, 1), (1, 1)],
                   fx=fxs("tape", "plate", delay_tone=0.5, reverb_damp=0.45, tape_noise=0.25),
                   params={"t1_level": -1.0, "t2_level": -5.0, "t3_level": -60.0, "t4_level": -60.0, "delay_time": 7, "delay_fb": 0.3,
                           "reverb_size": 0.5, "reverb_decay": 2.2, "tape": 0.35, "glue": 0.3}, chord_mem=mem)
    return proj, bars(8, bpm), 4.0


# (slug, builder, loudness target in LUFS)
CLIPS = [("01-low-swell", low_swell, -16.0), ("02-half-light", half_light, -16.0), ("03-dub-throws", dub_throws, -16.0),
         ("04-slow-weight", slow_weight, -20.0), ("05-frozen-tail", frozen_tail, -20.0), ("06-drive-and-fold", drive_and_fold, -16.0),
         ("07-tape-chords", tape_chords, -16.0)]


def presets():
    """The clips as factory presets (presets.py)."""
    return [fn()[0] for _, fn, _ in CLIPS]


def write_all():
    """[(slug, project path, seconds, loudness target, project, tail seconds)] for demos.py."""
    os.makedirs(DEMO_DIR, exist_ok=True)
    out = []
    for slug, fn, target in CLIPS:
        proj, secs, tail = fn()
        path = os.path.join(DEMO_DIR, slug + ".json")
        with open(path, "w") as f:
            json.dump(proj, f, indent=1)
        out.append((slug, path, secs, target, proj, tail))
    # The web film's soundtrack (tools/film.json): Half-Light played for 12 bars (36 s at 80 BPM).
    film, _, _ = half_light()
    film["chain"] = [[0, 1, 0, 0], [1, 2, 0, 0]]
    with open(os.path.join(DEMO_DIR, "film-half-light.json"), "w") as f:
        json.dump(film, f, indent=1)
    return out


if __name__ == "__main__":
    for slug, path, secs, target, proj, tail in write_all():
        print(f"{slug:22s} {secs:5.1f} s + {tail} s tail, {target} LUFS")
