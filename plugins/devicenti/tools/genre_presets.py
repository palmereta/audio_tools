"""DeVicenti factory banks: Trip Hop, Downtempo, Acid Jazz, Ambient, Dark Ambient, Heavy (genre-recipes.md §1-§6).

Each preset features one sound (its name) inside a short arrangement of its genre: two patterns of four bars (A and
B), a chain A A B A (the last B with its fill), the genre's kit, bass, keys and pad or lead. The featured sound takes
its role's track; the rest are the bank's standard parts, varied in key and tempo from preset to preset.
"""
from presets import *  # noqa: F401,F403


def chord_seq(p, t, prog, octave, size_default, voicing, length_steps, comp=None, vel=88, offset=0, bar=16):
    """One chord per bar (prog: (degree, size[, flags or 'type'])), held, or comped on `comp` steps."""
    for b, c in enumerate(prog):
        deg, size = c[0], c[1] if len(c) > 1 else size_default
        extra = c[2] if len(c) > 2 else None
        flags = CF[extra] if extra in CF else 0
        ctype = CT[extra] if extra in CT else 0
        if comp is None:
            p.chord(t, offset + b * bar, deg, octave, size, ctype, VC[voicing], flags=flags, len=length_steps * 12, vel=vel)
        else:
            for k, (i, ln, v) in enumerate(comp):
                p.chord(t, offset + b * bar + i, deg, octave, size, ctype, VC[voicing], flags=flags, len=int(ln * 12), vel=v)


def bass_seq(p, t, prog, root, scale, rhythm, octave=1, offset=0, bar=16):
    """Bass on each bar's chord root: rhythm = [(step, length, velocity, degree offset in the scale)]."""
    for b, c in enumerate(prog):
        for (i, ln, v, dd) in rhythm:
            p.trig(t, offset + b * bar + i, notes=degree_note(root, scale, c[0] + dd, octave), vel=v, len=-1 if ln < 0 else int(ln * 12))


def arrange(g, feature_role=None, feature=None, tempo=None, root=None, scale=None, name="", desc="", order=0, category="", extra_fx=None, extra_params=None):
    tempo = tempo or g["tempo"]
    root = NOTE[root] if isinstance(root, str) else (g["root"] if root is None else root)
    scale = SCALE[scale] if isinstance(scale, str) else (g["scale"] if scale is None else scale)
    parts = {"kit": g["kit"](), "bass": g["bass"](), "keys": g["keys"](), "t4": g["t4"]()}
    if feature_role:
        parts[feature_role] = feature
    A, B = Pattern(0, 64, root, scale), Pattern(1, 64, root, scale)
    for P, var in ((A, "A"), (B, "B")):
        for bar in range(4):
            sub = Pattern(99, 16)
            g["drums"] (sub, 0, var if bar < 3 else ("B" if var == "A" else "C"))
            for (lane, i), st in sub.tracks[0]["steps"].items():
                st2 = dict(st); st2["i"] = i + 16 * bar
                P.tracks[0]["steps"][(lane, i + 16 * bar)] = st2
        prog = g["prog"] if P is A else g.get("prog_b", g["prog"])
        bass_seq(P, 1, prog, root, scale, g["bassline"], g.get("bass_oct", 1))
        chord_seq(P, 2, prog, g.get("keys_oct", 4), g.get("keys_size", 4), g.get("voicing", "drop2"), 15, comp=g.get("comp"), vel=g.get("keys_vel", 84))
        g["t4_part"] (P, 3, prog, root, scale)
    # Fill in B's last bar: a snare roll.
    B.trig(0, 61, lane=1, vel=70, retrig=3, retrigCount=3, retrigRamp=40, cond="fill")
    tracks = [kit_track(parts["kit"]), tone(parts["bass"]), tone(parts["keys"]), tone(parts["t4"])]
    params = dict(g["params"]); params.update(extra_params or {})
    fx = dict(g["fx"]); fx.update(extra_fx or {})
    return project(name, category, order, desc, tempo, g["swing"], tracks, [A, B], chain=[(0, 2), (1, 1, 1), (0, 1)], fx=fx, params=params)


# ── T4 parts ──
def t4_pad(octave=3, size=3, hold=2):
    def f(P, t, prog, root, scale):
        for b in range(0, len(prog), hold):
            P.chord(t, b * 16, prog[b][0], octave, size, 0, VC["spread"], len=16 * hold * 12 - 6, vel=70)
    return f


def t4_lead(motif):
    def f(P, t, prog, root, scale):
        for (i, deg, ln, v) in motif:
            P.trig(t, i, notes=degree_note(root, scale, deg, 5), vel=v, len=int(ln * 12))
    return f


def t4_bells(prob=70):
    def f(P, t, prog, root, scale):
        for b, c in enumerate(prog):
            for k, (i, dd) in enumerate([(0, 4), (6, 2), (10, 7), (13, 9)]):
                P.trig(t, b * 16 + i, notes=degree_note(root, scale, c[0] + dd, 5), vel=80 - 8 * k, len=36, prob=prob)
    return f


G = {
    "Trip Hop": dict(tempo=84, swing=58, root=NOTE["A"], scale=SCALE["minor"], kit=kit_bristol, drums=drums_th, bass=bass_heavy_detuned, keys=ep_tine, t4=pad_dusty,
                     prog=[(0, 5), (0, 5), (5, 4), (4, 4, "b9")], prog_b=[(5, 4), (5, 4), (2, 4), (6, 4)],
                     bassline=[(0, 4, 110, 0), (6, 2, 96, 0), (10, 5, 100, 2)], keys_size=5, t4_part=t4_pad(3, 3, 2),
                     fx=fxs("tape", "spring", delay_tone=0.45, delay_wow=0.3, tape_noise=0.45, reverb_damp=0.55),
                     params={"t1_level": -2.0, "t2_level": -3.0, "t3_level": -4.0, "t4_level": -9.0, "delay_time": 6, "delay_fb": 0.38, "reverb_size": 0.4, "reverb_decay": 2.2, "tape": 0.45, "glue": 0.35, "limit": 0.2}),
    "Downtempo": dict(tempo=92, swing=55, root=NOTE["F"], scale=SCALE["major"], kit=kit_round, drums=drums_dt, bass=bass_rolling, keys=pad_warm, t4=pluck_dub,
                      prog=[(0, 5), (2, 4), (1, 5), (4, 5)], prog_b=[(5, 5), (3, 4), (1, 5), (4, 4, "sus4")],
                      bassline=[(0, 1.5, 110, 0), (3, 0.8, 80, 0), (6, 1.5, 100, 0), (10, 1.0, 96, 2), (12, 2, 100, 4)], keys_oct=3, keys_size=5, voicing="spread",
                      t4_part=t4_lead([(16 + 0, 4, 1.5, 100), (16 + 3, 2, 1, 84), (16 + 6, 4, 2, 96), (48 + 0, 6, 1.5, 100), (48 + 3, 4, 1, 84), (48 + 10, 2, 3, 90)]),
                      fx=fxs("dub", "hall", delay_tone=0.4, delay_return=0.8, reverb_damp=0.45, tape_noise=0.25),
                      params={"t1_level": -3.0, "t2_level": -3.0, "t3_level": -6.0, "t4_level": -6.0, "delay_time": 7, "delay_fb": 0.6, "reverb_size": 0.6, "reverb_decay": 3.5, "tape": 0.3, "glue": 0.25}),
    "Acid Jazz": dict(tempo=100, swing=60, root=NOTE["D"], scale=SCALE["dorian"], kit=kit_club, drums=drums_aj, bass=bass_slap, keys=ep_tine, t4=organ_drawbar,
                      prog=[(0, 5), (3, 6), (6, 5), (4, 4, "b9")], prog_b=[(0, 6), (0, 6), (1, 5), (4, 5)],
                      bassline=[(0, 0.8, 115, 0), (3, 0.5, 90, 7), (6, 0.8, 100, 0), (8, 0.4, 70, 2), (10, 0.8, 105, 4), (14, 0.5, 90, 6)],
                      comp=[(3, 1.0, 92), (6, 0.8, 80), (10, 1.0, 88), (13, 0.8, 76)], keys_size=5, voicing="drop2", t4_part=t4_pad(3, 4, 1),
                      fx=fxs("pingpong", "plate", delay_tone=0.6, reverb_damp=0.4, tape_noise=0.15),
                      params={"t1_level": -2.0, "t2_level": -3.0, "t3_level": -5.0, "t4_level": -12.0, "delay_time": 4, "delay_fb": 0.3, "reverb_size": 0.35, "reverb_decay": 1.6, "glue": 0.45, "limit": 0.2}),
    "Ambient": dict(tempo=70, swing=50, root=NOTE["D"], scale=SCALE["lydian"], kit=kit_tick, drums=lambda p, t, v: p.grid(t, 0, "X . . . . . . . . . . . . . . .", cond=("ab", 1, 2)).grid(t, 7, ". . . . . . . . x . . . . . . .", vel=40, prob=40),
                    bass=choir_drone, keys=pad_evolving, t4=bell_glass, prog=[(0, 3, "sus2"), (0, 3, "sus2"), (3, 4), (3, 4)], prog_b=[(0, 3, "sus2"), (4, 3, "sus4"), (0, 3, "add9"), (4, 3, "sus4")],
                    bassline=[(0, -1, 70, 0)], bass_oct=2, keys_oct=4, keys_size=4, voicing="spread", t4_part=t4_bells(55),
                    fx=fxs("pingpong", "hall", delay_tone=0.5, reverb_shimmer=0.35, reverb_damp=0.3, tape_noise=0.2),
                    params={"t1_level": -10.0, "t2_level": -8.0, "t3_level": -5.0, "t4_level": -8.0, "delay_time": 8, "delay_fb": 0.5, "reverb_size": 0.8, "reverb_decay": 9.0, "tape": 0.3}),
    "Dark Ambient": dict(tempo=60, swing=50, root=NOTE["D"], scale=SCALE["phrygian"], kit=kit_dark,
                         drums=lambda p, t, v: p.grid(t, 1, "X . . . . . . . . . . . . . . .", cond=("ab", 1, 2)).grid(t, 0, "x . . . . . . . . . x . . . . .", vel=70, prob=60).grid(t, 6, ". . . . . . . . . . . x . . . .", prob=50),
                         bass=sub_rumble, keys=pad_cluster, t4=resonator_drone, prog=[(0, 3, "cluster"), (0, 3, "cluster"), (1, 3, "fifths"), (0, 3, "cluster")],
                         bassline=[(0, -1, 90, 0)], bass_oct=1, keys_oct=3, keys_size=3, voicing="close", t4_part=t4_pad(2, 2, 4),
                         fx=fxs("dub", "void", delay_tone=0.3, reverb_damp=0.7, reverb_mod=0.5, tape_noise=0.4),
                         params={"t1_level": -6.0, "t2_level": -4.0, "t3_level": -7.0, "t4_level": -8.0, "delay_time": 6, "delay_fb": 0.7, "reverb_size": 0.8, "reverb_decay": 18.0, "tape": 0.4, "duck": 0.25}),
    "Heavy": dict(tempo=75, swing=52, root=NOTE["E"], scale=SCALE["phrygian"], kit=kit_anvil, drums=drums_hv, bass=bass_growl, keys=stab_hard, t4=bass_sub,
                  prog=[(0, 2, "power"), (0, 2, "power"), (1, 2, "power"), (0, 2, "power")],
                  bassline=[(0, 2.5, 120, 0), (6, 1.5, 100, 2), (9, 1.5, 110, 0), (12, 2, 105, 3)], keys_oct=3, keys_size=2, voicing="close",
                  comp=[(14, 0.5, 120)], t4_part=lambda P, t, prog, root, scale: [P.trig(t, b * 16, notes=degree_note(root, scale, c[0], 1), vel=110, len=15 * 12) for b, c in enumerate(prog)],
                  fx=fxs("dub", "plate", delay_tone=0.45, reverb_damp=0.5, tape_noise=0.2),
                  params={"t1_level": -1.0, "t2_level": -4.0, "t3_level": -6.0, "t4_level": -5.0, "delay_time": 6, "delay_fb": 0.55, "reverb_size": 0.3, "reverb_decay": 1.8, "tape": 0.35, "glue": 0.6, "limit": 0.45}),
}


def _v(s, **kw):
    s = dict(s); s.update(kw); return s


def genre_banks():
    out = []
    order = 10
    banks = {
        "Trip Hop": [
            ("Basement Boom", "bass", bass_heavy_detuned(0.4, 0.1, 450.0), "Heavy detuned FM bass over a clean sine and sub; dusty kit, Rhodes-like keys.", {}),
            ("Damp Hum", "bass", _v(bass_heavy_detuned(0.3, 0.12, 380.0), algo=ALGO["CROWN"], c_ratio=1.0, d_ratio=2.0, b_level=0.35), "Crown bass, low index, a little fold.", {"tempo": 80, "root": "D"}),
            ("Slow Sag", "bass", _v(bass_heavy_detuned(0.35, 0.0, 420.0), algo=ALGO["STACK"], glide=0.12, glide_mode=1, fenv=0.4, f_dec=0.4), "Stack bass with a long glide.", {"tempo": 78, "root": "C"}),
            ("Tine Haze", "keys", ep_tine(0.25, 3500.0), "Tine EP through spring reverb, minor ninths, drop-2.", {"root": "E"}),
            ("Velvet Tine", "keys", ep_tine(0.18, 2800.0, "chorus", 0.35), "Warmer tine, chorus.", {"tempo": 82, "root": "G"}),
            ("Dust Strings", "t4", pad_dusty(True), "Fan strings through the 12-bit insert, AGE 0.7.", {}),
            ("Thin Voice", "t4", lead_theremin(), "A whistling lead with a delayed vibrato and tape echo.", {"root": "D"}),
            ("Bristol Kit", "kit", kit_bristol(), "Boom-bap kick, dusty snare, rim, vinyl pops; lo-fi on the kit bus.", {"tempo": 86}),
            ("Sleepwalk Kit", "kit", kit_sleepwalk(), "Half-time feel, brushed snare, rim.", {"tempo": 76}),
            ("Cracked Vinyl Kit", "kit", _v_kit(kit_bristol(0.7), ins_amt=0.55, ins_p1=0.45), "Heavier crush on the kit bus.", {"tempo": 88}),
            ("Murk Pad", "t4", _v(pad_dusty(False), algo=ALGO["DIAMOND"], cutoff=900.0, send_reverb=0.4), "Diamond pad, dark low-pass.", {"root": "C"}),
            ("Rainy Pluck", "t4", pluck_dub(), "Short FM pluck into the dub delay.", {"tempo": 90}),
        ],
        "Downtempo": [
            ("Rolling Under", "bass", _v(bass_rolling(), algo=ALGO["STACK+SINE"]), "Rolling bass over a sub, warm pads, dub delay pluck.", {}),
            ("Warm Floor", "bass", _v(bass_rolling(), drive=0.2, cutoff=600.0), "Round bass with soft drive.", {"root": "C"}),
            ("Round Walk", "bass", _v(bass_rolling(), amp_rel=0.08, a_dec=0.25, send_delay=0.12), "Short bass with a tape slap.", {"tempo": 96}),
            ("Soft Vapor", "keys", pad_warm(), "Fan pad with chorus and AGE.", {"root": "Bb"}),
            ("Long Porch", "keys", _v(pad_warm(), algo=ALGO["DIAMOND"], lfo2_rate=0.04), "Diamond pad, slow movement.", {"root": "G", "tempo": 88}),
            ("Glass Pluck", "t4", pluck_dub(), "Fork pluck, dub delay, band-passed echoes.", {}),
            ("Bamboo", "t4", pluck_wood(), "Wooden pluck, ratios 2 and 4.", {"root": "D"}),
            ("Slow Choir", "keys", choir_fm(), "Additive choir with a moving band-pass (not a vocoder).", {"root": "A", "tempo": 86}),
            ("Soft Keys", "keys", ep_tine(0.12, 3000.0, "chorus", 0.25), "Low tine level, soft EP.", {"root": "Eb"}),
            ("Sunday Kit", "kit", kit_brush(), "Ride and rim, soft kick.", {"tempo": 94}),
            ("Round Kit", "kit", kit_round(), "Soft kick and snare, shaker.", {}),
            ("Dub Whistle", "t4", _v(lead_theremin(), send_delay=0.45), "Whistle lead in the dub delay.", {"root": "C"}),
        ],
        "Acid Jazz": [
            ("Funk Tine", "keys", ep_tine(0.32, 4500.0, "phaser", 0.2), "Bright tine EP comping ninths and elevenths.", {}),
            ("Wah Keys", "keys", clav(), "Clav-like FM through a band-pass with a wah LFO.", {"root": "E"}),
            ("Late Set EP", "keys", _v(ep_tine(0.2, 3000.0, "chorus", 0.2), lfo2_depth=0.15, lfo2_rate=4.5, lfo2_dest=dest("amp")), "Soft EP with tremolo.", {"tempo": 96}),
            ("Sunday Organ", "t4", organ_drawbar(0.2), "Drawbar-style additive organ, ensemble as a slow rotor.", {}),
            ("Church Drive", "t4", organ_drawbar(0.45, 0.25), "Organ driven and folded.", {"root": "G"}),
            ("Pocket Slap", "bass", bass_slap(), "Slap-like FM bass with a 7:1 click operator.", {}),
            ("Round Fretless", "bass", bass_fretless(), "Gliding fretless-style bass.", {"root": "A", "tempo": 98}),
            ("Horn Section", "t4", brass_stab(False), "Fork brass, three-voice unison.", {}),
            ("Short Horn", "t4", brass_stab(True), "Tight brass stab.", {"root": "C", "tempo": 104}),
            ("Vibe Mallet", "t4", vibes(), "FM vibraphone with a slow tremolo.", {"root": "F"}),
            ("Club Kit", "kit", kit_club(), "Tight kit with ride and cowbell.", {"tempo": 102}),
            ("Brush Kit", "kit", kit_brush(), "Brushes and rim.", {"tempo": 96}),
        ],
        "Ambient": [
            ("Slow Tide", "keys", pad_evolving(), "Fan pad with a looping modulator envelope, hall reverb with shimmer.", {}),
            ("Cold Dawn", "keys", _v(pad_evolving(), algo=ALGO["DIAMOND"], cutoff=1600.0), "Diamond pad, low-pass, slow.", {"root": "E"}),
            ("Wide Room", "keys", _v(pad_evolving(), unison=4, send_reverb=0.6), "Four-voice unison pad.", {"root": "C"}),
            ("Held Breath", "bass", choir_drone(), "Additive drone at 0.5 / 1 / 1.5 / 2.", {}),
            ("Glass Rain", "t4", bell_glass(), "Bells 1 : 3.5 and 1.41, probability.", {"root": "A"}),
            ("Soft Chime", "t4", bell_glass(True), "Short chime.", {"root": "F"}),
            ("Paper Piano", "t4", piano_soft(), "Soft piano-like pairs.", {"root": "G", "tempo": 66}),
            ("Warm Hum", "bass", _v(choir_drone(), sub_level=0.5, cutoff=900.0), "Drone with a sub octave.", {"root": "D"}),
            ("Aurora", "keys", _v(pad_evolving(), send_reverb=0.7), "Pad into the shimmer.", {"root": "B"}, {"reverb_shimmer": 0.6}),
            ("Distant Tape", "keys", _v(pad_dusty(True), age=0.95, ins_amt=0.45), "Worn-tape pad, lo-fi.", {"root": "Eb", "tempo": 64}),
            ("Tick Garden", "kit", kit_tick(), "Ticks and soft kicks, polymeter.", {}),
            ("Frozen Lake", "keys", pad_evolving(), "Pad with the reverb frozen.", {"root": "D"}, {"reverb_freeze": 1}),
        ],
        "Dark Ambient": [
            ("Black Cavern", "t4", resonator_drone(), "Feedback drone through a comb tuned to the root, void reverb.", {}),
            ("Iron Breath", "t4", _v(resonator_drone(), algo=ALGO["CROWN"], ins_type=INS["off"]), "Crown drone, feedback on B and C.", {"root": "C"}),
            ("Cold Furnace", "bass", sub_rumble(), "Sub rumble, driven, mono.", {}),
            ("Under Water", "keys", pad_cluster(), "Cluster pad, root + b2 + fifth.", {"root": "E"}),
            ("Rusted Choir", "keys", _v(choir_drone(), crush=0.4, ins_type=INS["lofi"], ins_amt=0.5), "Choir crushed to ~10 bits.", {"root": "F"}),
            ("Rite Bell", "t4", bell_rite(), "Bell at 1.41 with a long decay.", {"root": "D"}),
            ("Hollow Gong", "t4", _v(bell_rite(), b_ratio=2.76, a_dec=8.0), "Gong-like pairs at 2.76.", {"root": "C"}),
            ("Distant Clang", "t4", metal_drone(False), "Fixed-frequency metal, ring mod.", {}),
            ("Dead Signal", "t4", texture_ring(), "Noise through a ring modulator, moving band-pass.", {"root": "A"}),
            ("Crypt EP", "keys", ep_tine(0.12, 1600.0, "off", 0.0, dark=True), "Dark EP with slow tremolo, very slow.", {"tempo": 56}),
            ("Heartbeat Kit", "kit", kit_dark(), "Sub kick heartbeat, impacts, metal.", {}),
            ("Pressure", "bass", _v(sub_rumble(), drive=0.6), "Driven sub, mono.", {"root": "C"}),
            ("Last Light", "keys", pad_cluster(), "Cluster pad, reverb frozen.", {"root": "D"}, {"reverb_freeze": 1}),
        ],
        "Heavy": [
            ("Iron Growl", "bass", bass_growl(), "Crown growl bass, fold and drive over a clean sub; half-time kit.", {}),
            ("Basalt", "bass", bass_growl(0.7, 0.45, 600.0), "More fold.", {"root": "D"}),
            ("Low Press", "bass", _v(bass_heavy_detuned(0.55, 0.2, 380.0), sub_level=0.7), "Stack+sine bass, heavy sub.", {"root": "F"}),
            ("Concrete Sub", "t4", bass_sub(), "Clean sine sub.", {}),
            ("Hammer Stab", "keys", stab_hard(False), "Fork stab, driven, three-voice unison.", {}),
            ("Slag Stab", "keys", stab_hard(True), "Stab crushed to ~10 bits.", {"root": "C"}),
            ("Fold Lead", "keys", lead_fold(), "Feedback stack folded hard.", {"root": "A"}),
            ("Heavy Pad", "t4", _v(pad_warm(), drive=0.5, cutoff=1200.0, ftype=FTYPE["LP24"]), "Distorted pad, ladder.", {"root": "E"}),
            ("Wobble Drive", "bass", _v(bass_growl(), lfo1_shape=LFO_SHAPE["sh"], lfo1_depth=0.25, lfo1_sync=DIV["1/8"]), "Growl with a sample-and-hold on the modulator.", {"tempo": 72}),
            ("Anvil Kit", "kit", kit_anvil(), "Half-time kit, folded kick, crushed snare.", {}),
            ("Grit Kit", "kit", _v_kit(kit_anvil(), ins_type=INS["lofi"], ins_amt=0.5, ins_p1=0.4), "The kit through the bitcrusher.", {"tempo": 78}),
            ("Dub Slab", "bass", _v(bass_growl(0.5, 0.15, 650.0), send_delay=0.3), "Growl into the dub delay.", {"root": "G"}),
        ],
    }
    for cat, items in banks.items():
        g = G[cat]
        for it in items:
            name, role, sound, desc, over = it[0], it[1], it[2], it[3], it[4]
            fx = it[5] if len(it) > 5 else None
            out.append(arrange(g, role, sound, tempo=over.get("tempo"), root=over.get("root"), scale=over.get("scale"), name=name, desc=desc, order=order, category=cat, extra_fx=fx))
            order += 1
    return out


def _v_kit(k, **bus):
    k = dict(k); b = dict(k["0"]); b.update(bus); k["0"] = b
    return k
