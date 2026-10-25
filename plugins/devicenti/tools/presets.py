#!/usr/bin/env python3
"""DeVicenti factory presets (plan §5.9, §13; audiotools-private/vicenti/genre-recipes.md).

    python3 plugins/devicenti/tools/presets.py            writes assets/presets/*.json

A preset is a whole project: four tracks (TONE, KIT or MIDI) with their sounds, patterns, a song chain, the effect
modes and the host parameters (tempo, swing, levels, sends' returns, master). Banks are named for the genres the
instrument is aimed at: Trip Hop, Downtempo, Acid Jazz, Ambient, Dark Ambient, Heavy. Every preset features one sound
of its bank (the name), set in a short arrangement with that genre's kit, bass and keys, so it plays on load.

The sound library, the kits and the pattern helpers in this file are also used by demo_songs.py (the demos of the
listening checkpoint). Parameter keys are the stable keys of Source/dsp/SoundParams.h. No brand or artist names.
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "presets")

# ───────────────────────── sounds ─────────────────────────
ALGO = {"STACK": 0, "FORK": 1, "PAIRS": 2, "FAN": 3, "CROWN": 4, "STACK+SINE": 5, "DIAMOND": 6, "CHOIR": 7}
HSHAPE = {"sine": 0, "saw": 1, "square": 2, "organ": 3, "reed": 4, "bell": 5}
FTYPE = {"LP24": 0, "LP12": 1, "BP": 2, "HP": 3, "NOTCH": 4, "PEAK": 5, "OFF": 6}
INS = {"off": 0, "drive": 1, "lofi": 2, "chorus": 3, "phaser": 4, "ring": 5, "comb": 6}
LFO_SHAPE = {"sine": 0, "tri": 1, "saw": 2, "square": 3, "sh": 4, "smooth": 5}
LFO_MODE = {"free": 0, "trig": 1, "hold": 2, "one": 3}
DIV = {"free": 0, "1/64": 1, "1/32T": 2, "1/32": 3, "1/16T": 4, "1/32D": 5, "1/16": 6, "1/8T": 7, "1/16D": 8, "1/8": 9, "1/4T": 10, "1/8D": 11,
       "1/4": 12, "1/4D": 13, "1/2": 14, "1/2D": 15, "1bar": 16, "2bars": 17, "4bars": 18, "8bars": 19, "12bars": 20, "16bars": 21}
MODSRC = {"vel": 1, "key": 2, "wheel": 3, "at": 4, "poly": 5, "bend": 6, "lfo1": 7, "lfo2": 8, "rand": 9, "breath": 10, "expr": 11}
DELAY_DIV = ["1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1bar"]

# Sound parameter order (for LFO / matrix destinations: 5 + index). Mirrors SoundParams.h.
_OPK = ["ratio", "detune", "level", "fb", "hshape", "hamt", "att", "dec", "sus", "rel", "curve", "delay", "loop", "vel", "ks", "fixed", "tscale"]
PARAM_KEYS = ["algo"] + [f"{o}_{k}" for o in "abcd" for k in _OPK] + [
    "penv_depth", "penv_time", "noise_level", "noise_decay", "noise_freq", "noise_q", "noise_mode", "noise_att", "noise_env",
    "drive", "fold", "crush", "ftype", "cutoff", "reso", "fenv", "f_att", "f_dec", "f_sus", "f_rel", "ktrk", "base", "width", "basscomp",
    "amp_att", "amp_dec", "amp_sus", "amp_rel", "vel_amp", "pan", "level", "sub_level", "sub_oct", "sub_shape", "unison", "uni_detune",
    "uni_spread", "vmode", "voices", "glide", "glide_mode", "tune", "fine", "bend", "age", "vintage", "send_delay", "send_reverb",
    "ins_type", "ins_amt", "ins_p1", "ins_p2", "ins_mix", "lowmono"]
PARAM_KEYS += [f"lfo{n}_{k}" for n in (1, 2) for k in ("shape", "rate", "sync", "mode", "fade", "phase", "dest", "depth")]
PARAM_KEYS += [f"mod{n}_{k}" for n in range(1, 9) for k in ("src", "dst", "amt")]
PARAM_KEYS += ["strum", "strum_dir", "hum_vel", "hum_time", "chord_hold", "arp_mode", "arp_rate", "arp_range", "kit_pitch", "lane_note", "choke"]
VIRTUAL_DEST = {"pitch": 1, "amp": 2, "index": 3, "pan": 4}


def dest(name):
    """LFO / matrix destination by name: a virtual one (pitch, amp, index, pan) or a sound parameter key."""
    if name in VIRTUAL_DEST:
        return VIRTUAL_DEST[name]
    return 5 + PARAM_KEYS.index(name)


def op(l, ratio=1.0, level=0.0, a=0.002, d=0.5, s=1.0, r=0.2, fb=0.0, vel=0.3, ks=0.0, detune=0.0, fixed=0.0, harm=None,
       curve=0.0, delay=0.0, loop=0, tscale=1.0):
    """One operator (l = 'a'..'d'). Level: carrier gain, or modulation index 10·L² rad for a modulator."""
    d_ = {f"{l}_ratio": ratio, f"{l}_level": level, f"{l}_att": a, f"{l}_dec": d, f"{l}_sus": s, f"{l}_rel": r, f"{l}_fb": fb,
          f"{l}_vel": vel, f"{l}_ks": ks, f"{l}_detune": detune, f"{l}_fixed": fixed, f"{l}_curve": curve, f"{l}_delay": delay,
          f"{l}_loop": loop, f"{l}_tscale": tscale}
    if harm:
        d_[f"{l}_hshape"] = HSHAPE[harm[0]]
        d_[f"{l}_hamt"] = harm[1]
    return d_


def lfo(n, dest_name, depth, rate=1.0, sync="free", shape="sine", mode="free", fade=0.0, phase=0.0):
    p = f"lfo{n}_"
    return {p + "dest": dest(dest_name), p + "depth": depth, p + "rate": rate, p + "sync": DIV[sync], p + "shape": LFO_SHAPE[shape],
            p + "mode": LFO_MODE[mode], p + "fade": fade, p + "phase": phase}


def mod(n, src, dest_name, amt):
    return {f"mod{n}_src": MODSRC[src], f"mod{n}_dst": dest(dest_name), f"mod{n}_amt": amt}


def snd(algo, *ops, ftype="LP24", ins="off", **kw):
    s = {"algo": ALGO[algo] if isinstance(algo, str) else algo, "ftype": FTYPE[ftype], "ins_type": INS[ins]}
    for o in ops:
        s.update(o)
    s.update(kw)
    for k in s:
        assert k in PARAM_KEYS, k
    return s


def amp(a=0.002, d=0.3, s=1.0, r=0.1):
    return {"amp_att": a, "amp_dec": d, "amp_sus": s, "amp_rel": r}


def fenv(amount, a=0.002, d=0.3, s=0.0, r=0.2):
    return {"fenv": amount, "f_att": a, "f_dec": d, "f_sus": s, "f_rel": r}


def merge(*ds, **kw):
    out = {}
    for d in ds:
        out.update(d)
    out.update(kw)
    return out


# ── TONE sounds by role (genre-recipes §1-§7) ──
def bass_heavy_detuned(drive=0.42, fold=0.1, cutoff=450.0):
    """Trip hop pillar: STACK+SINE, A is a clean sine, B carries the C/D growl, + SUB −1 oct."""
    return snd("STACK+SINE",
               op("a", 1.0, 0.8, 0.002, 0.25, 0.8, 0.15, vel=0.2), op("b", 1.0, 0.72, 0.003, 0.3, 0.6, 0.12, detune=9.0, vel=0.3),
               op("c", 1.0, 0.45, 0.002, 0.35, 0.4, 0.12, vel=0.6), op("d", 2.0, 0.3, 0.002, 0.2, 0.1, 0.1, vel=0.7),
               ftype="LP24", cutoff=cutoff, reso=0.3, ktrk=0.4, **fenv(0.3, d=0.18), drive=drive, fold=fold, sub_level=0.6, lowmono=1,
               vmode=2, glide=0.04, age=0.5, send_reverb=0.04, **amp(0.002, 0.3, 1.0, 0.12), basscomp=0.7, level=-2.0)


def bass_growl(drive=0.62, fold=0.28, cutoff=700.0):
    """Heavy: CROWN with feedback, folded and driven, clean sub under it."""
    return snd("CROWN",
               op("a", 1.0, 0.8, 0.001, 0.3, 0.7, 0.12), op("b", 1.0, 0.4, 0.001, 0.22, 0.4, 0.1, fb=0.5, vel=0.5),
               op("c", 2.0, 0.5, 0.001, 0.18, 0.3, 0.1, fb=0.2, vel=0.5), op("d", 3.0, 0.35, 0.001, 0.12, 0.1, 0.08, vel=0.6),
               ftype="LP24", cutoff=cutoff, reso=0.35, ktrk=0.3, **fenv(0.33, d=0.18), drive=drive, fold=fold, sub_level=0.65, lowmono=1,
               unison=2, uni_detune=8.0, uni_spread=0.3, vmode=1, age=0.3, ins="drive", ins_amt=0.35, ins_p1=0.55, ins_p2=0.2,
               **amp(0.001, 0.3, 1.0, 0.1), basscomp=0.8, level=-6.0,
               **lfo(1, "b_level", 0.12, sync="1/8", shape="tri", mode="trig"))


def bass_sub(cutoff=170.0):
    return snd("STACK+SINE", op("a", 1.0, 0.9, 0.003, 0.35, 0.85, 0.16, harm=("square", 0.0)), op("b", 1.0, 0.0),
               ftype="LP24", cutoff=cutoff, reso=0.05, ktrk=0.0, drive=0.1, sub_level=0.0, lowmono=1, vmode=2, glide=0.03,
               **amp(0.003, 0.35, 1.0, 0.16), level=-1.0)


def bass_rolling():
    return snd("STACK", op("a", 1.0, 0.85, 0.002, 0.4, 0.7, 0.2, fb=0.15), op("b", 1.0, 0.35, 0.002, 0.26, 0.2, 0.15, vel=0.5),
               op("c", 2.0, 0.18, 0.002, 0.2, 0.0, 0.1), op("d", 1.0, 0.0),
               ftype="LP24", cutoff=520.0, reso=0.18, ktrk=0.4, **fenv(0.35, d=0.22), drive=0.3, sub_level=0.5, lowmono=1,
               vmode=2, glide=0.035, age=0.3, send_delay=0.06, send_reverb=0.04, **amp(0.002, 0.3, 1.0, 0.15), level=-2.0)


def bass_slap():
    return snd("STACK", op("a", 1.0, 0.85, 0.001, 0.32, 0.4, 0.11), op("b", 1.0, 0.45, 0.001, 0.06, 0.0, 0.05, vel=0.8),
               op("c", 3.0, 0.28, 0.001, 0.09, 0.0, 0.06, vel=0.6), op("d", 7.0, 0.12, 0.001, 0.025, 0.0, 0.02),
               ftype="LP24", cutoff=1400.0, reso=0.15, ktrk=0.6, **fenv(0.2, d=0.12), drive=0.15, sub_level=0.25, lowmono=1,
               vmode=1, **amp(0.001, 0.3, 1.0, 0.08), penv_depth=2.0, penv_time=0.012, level=-2.0)


def bass_fretless():
    return snd("STACK", op("a", 1.0, 0.8, 0.008, 0.6, 0.8, 0.2), op("b", 1.0, 0.25, 0.01, 0.5, 0.4, 0.2, vel=0.5), op("c", 1.0, 0.1),
               ftype="LP24", cutoff=900.0, reso=0.12, ktrk=0.5, **fenv(0.15, d=0.4), vmode=2, glide=0.12, glide_mode=1, sub_level=0.3,
               lowmono=1, age=0.3, **amp(0.006, 0.5, 1.0, 0.2), level=-2.0)


def ep_tine(tine=0.25, cutoff=3500.0, ins="chorus", ins_amt=0.3, age=0.5, dark=False):
    """FM Rhodes-like: PAIRS, tine pair B>A (14:1 bark), body pair D>C."""
    t = tine * (0.5 if dark else 1.0)
    return snd("PAIRS",
               op("a", 1.0, 0.8, 0.002, 1.4, 0.0, 0.5, vel=0.15), op("b", 14.0, t, 0.001, 0.12, 0.0, 0.06, vel=0.4, ks=0.2),
               op("c", 1.0, 0.55, 0.002, 1.8, 0.0, 0.6, detune=4.0, vel=0.2), op("d", 1.0, 0.3, 0.002, 0.8, 0.15, 0.3, fb=0.05, vel=0.4),
               ftype="LP12", cutoff=(1800.0 if dark else cutoff), reso=0.08, ktrk=0.5, drive=0.12, age=(0.9 if dark else age), ins=ins, ins_amt=ins_amt,
               ins_p1=0.3, ins_p2=0.6 if ins == "chorus" else 0.4, ins_mix=0.8, voices=12, **amp(0.002, 0.3, 1.0, 0.45), vel_amp=0.7,
               strum=0.012, hum_vel=0.4, hum_time=0.3, level=-5.0, send_reverb=0.38, send_delay=0.1,
               **lfo(2, "amp", 0.12 if dark else 0.0, rate=4.5 if not dark else 0.6))


def ep_wurly():
    s = ep_tine(tine=0.18, cutoff=3000.0, ins="phaser", ins_amt=0.25)
    s.update({"b_ratio": 7.07, "d_fb": 0.15, "a_dec": 1.0, "c_dec": 1.2})
    return s


def organ_drawbar(drive=0.25, fold=0.0, ins="chorus"):
    return snd("CHOIR", op("a", 0.5, 0.45, 0.002, 0.1, 1.0, 0.04), op("b", 1.0, 0.7, 0.002, 0.1, 1.0, 0.04),
               op("c", 2.0, 0.42, 0.002, 0.1, 1.0, 0.04, harm=("organ", 0.3)), op("d", 3.0, 0.32, 0.001, 0.12, 0.0, 0.04),
               ftype="LP12", cutoff=8000.0, reso=0.05, drive=drive, fold=fold, ins=ins, ins_amt=0.55, ins_p1=0.45, ins_p2=0.7, voices=10,
               **amp(0.004, 0.3, 1.0, 0.05), level=-9.0, send_reverb=0.2, age=0.2)


def brass_stab(short=True):
    return snd("FORK", op("a", 1.0, 0.8, 0.02 if short else 0.04, 0.25 if short else 0.4, 0.0 if short else 0.7, 0.12),
               op("b", 1.0, 0.5, 0.03 if short else 0.06, 0.3, 0.3 if short else 0.5, 0.12, vel=0.6), op("c", 2.0, 0.25, 0.03, 0.2, 0.0, 0.1),
               op("d", 1.0, 0.18, 0.02, 0.4, 0.3, 0.12, fb=0.3),
               ftype="LP12", cutoff=2800.0, reso=0.15, **fenv(0.3, a=0.02, d=0.25), unison=3, uni_detune=10.0, uni_spread=0.5, drive=0.25,
               **amp(0.004, 0.3, 1.0, 0.12), level=-10.0, send_reverb=0.12, voices=12)


def stab_hard(crush=False):
    s = snd("FORK", op("a", 1.0, 0.8, 0.001, 0.25, 0.0, 0.1), op("b", 2.0, 0.55, 0.001, 0.2, 0.1, 0.1, vel=0.5),
            op("c", 1.0, 0.6, 0.001, 0.09, 0.0, 0.06, vel=0.7), op("d", 1.41, 0.3, 0.001, 0.15, 0.0, 0.08),
            ftype="LP12", cutoff=2800.0, reso=0.2, drive=0.55, unison=3, uni_detune=12.0, uni_spread=0.6,
            ins="lofi" if crush else "drive", ins_amt=0.5 if crush else 0.3, ins_p1=0.45 if crush else 0.5, ins_p2=0.2,
            **amp(0.001, 0.3, 1.0, 0.1), level=-9.0, send_reverb=0.15, voices=12)
    return s


def lead_fold():
    return snd("STACK", op("a", 1.0, 0.7, 0.002, 0.5, 0.6, 0.15), op("b", 1.0, 0.4, 0.002, 0.3, 0.4, 0.12), op("c", 2.0, 0.25, 0.002, 0.2, 0.2, 0.1),
               op("d", 1.0, 0.35, 0.002, 0.3, 0.3, 0.1, fb=0.8), ftype="LP12", cutoff=3000.0, reso=0.2, fold=0.5, drive=0.3,
               vmode=2, glide=0.05, **amp(0.002, 0.3, 1.0, 0.12), level=-10.0, send_delay=0.25, send_reverb=0.2)


def lead_theremin():
    return snd("STACK", op("a", 1.0, 0.75, 0.04, 1.0, 0.9, 0.3), op("b", 1.0, 0.25, 0.05, 0.6, 0.6, 0.3), op("c", 1.0, 0.1, 0.05, 0.6, 0.6, 0.3),
               ftype="LP12", cutoff=4000.0, reso=0.1, vmode=2, glide=0.08, glide_mode=1, age=0.4, **amp(0.04, 0.3, 1.0, 0.35),
               level=-8.0, send_delay=0.3, send_reverb=0.35, **lfo(1, "pitch", 0.017, rate=5.2, mode="trig", fade=0.4))


def pluck_dub():
    return snd("FORK", op("a", 1.0, 0.8, 0.001, 0.26, 0.0, 0.12), op("b", 2.0, 0.4, 0.001, 0.16, 0.0, 0.08, vel=0.8),
               op("c", 3.0, 0.25, 0.001, 0.09, 0.0, 0.06), op("d", 1.0, 0.0),
               ftype="LP12", cutoff=3200.0, reso=0.12, **amp(0.001, 0.3, 1.0, 0.15), level=-8.0, send_delay=0.35, send_reverb=0.2, age=0.3)


def pluck_wood():
    return snd("PAIRS", op("a", 1.0, 0.8, 0.001, 0.5, 0.0, 0.2), op("b", 2.0, 0.35, 0.001, 0.08, 0.0, 0.05), op("c", 4.0, 0.25, 0.001, 0.3, 0.0, 0.1),
               op("d", 1.0, 0.25, 0.001, 0.06, 0.0, 0.04), ftype="LP12", cutoff=4500.0, **amp(0.001, 0.3, 1.0, 0.2), level=-8.0, send_delay=0.2, send_reverb=0.25)


def pad_dusty(lofi=True):
    return snd("FAN", op("a", 1.0, 0.7, 0.35, 2.0, 0.9, 0.9, harm=("saw", 0.3)), op("b", 1.0, 0.6, 0.4, 2.0, 0.9, 0.9, detune=7.0),
               op("c", 2.0, 0.4, 0.45, 2.0, 0.8, 0.9, detune=-5.0), op("d", 1.0, 0.25, 0.6, 3.0, 0.6, 1.0, fb=0.2),
               ftype="LP12", cutoff=1800.0, reso=0.1, unison=2, uni_detune=12.0, uni_spread=0.7, age=0.7,
               ins="lofi" if lofi else "chorus", ins_amt=0.33, ins_p1=0.5, ins_p2=0.6, ins_mix=0.7, voices=12,
               **amp(0.35, 0.5, 1.0, 0.9), level=-9.0, send_reverb=0.35, send_delay=0.05)


def pad_warm():
    return snd("FAN", op("a", 1.0, 0.65, 0.7, 2.0, 0.9, 1.4), op("b", 2.0, 0.45, 0.8, 2.0, 0.9, 1.4), op("c", 1.0, 0.55, 0.75, 2.0, 0.9, 1.4, detune=8.0),
               op("d", 1.0, 0.22, 1.0, 3.0, 0.6, 1.5, fb=0.15), ftype="LP12", cutoff=2200.0, reso=0.08, unison=2, uni_detune=10.0, uni_spread=0.7,
               age=0.6, ins="chorus", ins_amt=0.4, ins_p1=0.3, ins_p2=0.6, voices=12, **amp(0.7, 0.5, 1.0, 1.4), level=-10.0, send_reverb=0.4,
               **lfo(2, "cutoff", 0.06, rate=0.08))


def pad_evolving(shimmer=False):
    return snd("FAN", op("a", 1.0, 0.7, 1.5, 3.0, 1.0, 3.0), op("b", 2.0, 0.4, 1.8, 3.0, 1.0, 3.0, detune=6.0),
               op("c", 1.0, 0.5, 1.6, 3.0, 1.0, 3.0, detune=-7.0), op("d", 3.0, 0.18, 2.0, 4.0, 0.3, 3.0, fb=0.1, loop=1),
               ftype="LP12", cutoff=2400.0, reso=0.06, unison=3, uni_detune=14.0, uni_spread=1.0, age=0.4, ins="chorus", ins_amt=0.3,
               ins_p1=0.2, ins_p2=0.3, voices=12, **amp(1.6, 0.5, 1.0, 4.0), level=-11.0, send_reverb=0.55, send_delay=0.2,
               **lfo(2, "cutoff", 0.08, rate=0.04), **lfo(1, "index", 0.3, sync="4bars", shape="smooth"))


def choir_drone():
    return snd("CHOIR", op("a", 0.5, 0.55, 3.0, 4.0, 1.0, 4.0, harm=("organ", 0.4)), op("b", 1.0, 0.7, 3.0, 4.0, 1.0, 4.0, harm=("reed", 0.3)),
               op("c", 1.5, 0.35, 3.5, 4.0, 1.0, 4.0), op("d", 2.0, 0.25, 4.0, 4.0, 1.0, 4.0),
               ftype="LP24", cutoff=1600.0, reso=0.12, age=0.5, voices=8, **amp(3.0, 0.5, 1.0, 4.0), level=-11.0, send_reverb=0.35,
               **lfo(2, "cutoff", 0.1, rate=0.05, shape="smooth"))


def bell_glass(short=False):
    return snd("PAIRS", op("a", 1.0, 0.75, 0.001, 1.2 if short else 3.0, 0.0, 1.5), op("b", 3.5, 0.4, 0.001, 0.8 if short else 1.8, 0.0, 0.9, vel=0.7, ks=0.3),
               op("c", 2.0, 0.45, 0.001, 1.0 if short else 2.2, 0.0, 1.0, detune=3.0), op("d", 1.41, 0.3, 0.001, 0.5 if short else 1.0, 0.0, 0.6, ks=0.3),
               ftype="LP12", cutoff=8000.0, reso=0.05, age=0.4, voices=10, **amp(0.001, 0.3, 1.0, 1.5), level=-10.0, send_reverb=0.45, send_delay=0.2)


def bell_rite():
    s = bell_glass()
    s.update({"b_ratio": 1.41, "d_ratio": 2.76, "a_dec": 6.0, "c_dec": 5.0, "amp_rel": 3.0})
    return s


def metal_drone(gate=False):
    return snd("PAIRS", op("a", 1.0, 0.6, 1.0 if gate else 0.001, 4.0, 0.7 if gate else 0.0, 3.0), op("b", 1.0, 0.45, 0.001, 3.0, 0.5, 2.0, fixed=313.0, fb=0.3),
               op("c", 1.0, 0.35, 1.0 if gate else 0.001, 4.0, 0.6 if gate else 0.0, 3.0, fixed=673.0), op("d", 1.0, 0.25, 0.001, 2.0, 0.3, 2.0, fixed=1190.0, fb=0.6),
               ftype="LP12", cutoff=3000.0, reso=0.15, age=0.6, ins="ring", ins_amt=0.5, ins_p1=0.4, ins_p2=0.0, voices=6,
               **amp(1.0 if gate else 0.001, 0.5, 1.0, 3.0), level=-12.0, send_reverb=0.5)


def resonator_drone():
    return snd("CHOIR", op("a", 0.25, 0.5, 2.0, 4.0, 1.0, 4.0, fb=0.3), op("b", 0.5, 0.55, 2.5, 4.0, 1.0, 4.0, fb=0.45),
               op("c", 1.0, 0.45, 2.0, 4.0, 1.0, 4.0, fb=0.4), op("d", 1.5, 0.25, 3.0, 4.0, 1.0, 4.0),
               ftype="LP24", cutoff=520.0, reso=0.25, unison=4, uni_detune=20.0, uni_spread=0.8, age=0.8, ins="comb", ins_amt=0.85,
               ins_p1=0.5, ins_p2=0.5, ins_mix=0.6, voices=8, **amp(2.0, 0.5, 1.0, 4.0), level=-9.0, send_reverb=0.4,
               **lfo(1, "cutoff", 0.12, rate=0.03))


def sub_rumble():
    return snd("STACK+SINE", op("a", 0.5, 0.8, 0.5, 2.0, 1.0, 2.0), op("b", 1.0, 0.3, 0.6, 2.0, 1.0, 2.0),
               op("c", 1.41, 0.35, 0.6, 2.0, 0.8, 2.0), op("d", 0.5, 0.3, 0.8, 2.0, 0.7, 2.0, fb=0.4),
               ftype="LP24", cutoff=140.0, reso=0.2, sub_level=0.6, sub_oct=1, lowmono=1, drive=0.2, age=0.4,
               **amp(0.6, 0.5, 1.0, 2.5), level=-3.0)


def pad_cluster():
    return snd("DIAMOND", op("a", 1.0, 0.7, 3.5, 3.0, 1.0, 3.0), op("b", 1.0, 0.4, 3.5, 3.0, 1.0, 3.0, detune=14.0),
               op("c", 1.0, 0.4, 3.5, 3.0, 1.0, 3.0, detune=-11.0), op("d", 1.5, 0.3, 4.0, 3.0, 0.8, 3.0),
               ftype="LP12", cutoff=700.0, reso=0.15, unison=2, uni_detune=16.0, uni_spread=0.9, age=0.7, voices=10,
               **amp(3.5, 0.5, 1.0, 4.0), level=-8.0, send_reverb=0.6)


def texture_ring():
    return snd("CHOIR", op("a", 1.0, 0.3, 1.0, 2.0, 1.0, 2.0), ftype="BP", cutoff=1500.0, reso=0.4, noise_level=0.6, noise_decay=10.0,
               noise_freq=1500.0, noise_q=1.5, noise_mode=1, noise_env=1, ins="ring", ins_amt=0.7, ins_p1=0.05, ins_p2=0.2,
               **amp(1.0, 0.5, 1.0, 2.0), level=-10.0, send_reverb=0.5, **lfo(1, "noise_freq", 0.25, rate=0.07, shape="smooth"))


def piano_soft():
    return snd("PAIRS", op("a", 1.0, 0.8, 0.002, 1.5, 0.0, 0.9), op("b", 1.0, 0.2, 0.001, 0.3, 0.0, 0.15, vel=0.6),
               op("c", 2.0, 0.4, 0.002, 1.2, 0.0, 0.7), op("d", 1.0, 0.1, 0.001, 0.4, 0.0, 0.2),
               ftype="LP12", cutoff=3000.0, reso=0.05, ktrk=0.5, age=0.3, voices=12, **amp(0.002, 0.3, 1.0, 0.9), vel_amp=0.8,
               level=-7.0, send_reverb=0.55, hum_vel=0.3, hum_time=0.3)


def vibes():
    return snd("PAIRS", op("a", 1.0, 0.8, 0.001, 0.9, 0.0, 0.5), op("b", 4.0, 0.3, 0.001, 0.18, 0.0, 0.1, vel=0.6), op("c", 1.0, 0.0),
               ftype="LP12", cutoff=7000.0, **amp(0.001, 0.3, 1.0, 0.5), level=-8.0, send_reverb=0.15,
               **lfo(1, "amp", 0.12, rate=5.0))


def clav():
    return snd("STACK", op("a", 1.0, 0.8, 0.001, 0.26, 0.0, 0.06), op("b", 3.0, 0.45, 0.001, 0.12, 0.0, 0.04), op("c", 1.0, 0.3, 0.001, 0.2, 0.0, 0.05, fb=0.5),
               ftype="BP", cutoff=1800.0, reso=0.3, ins="phaser", ins_amt=0.4, ins_p1=0.4, ins_p2=0.3, **amp(0.001, 0.3, 1.0, 0.06), level=-8.0,
               **lfo(1, "cutoff", 0.2, rate=2.0, shape="tri"))


def choir_fm():
    return snd("CHOIR", op("a", 1.0, 0.6, 0.4, 2.0, 1.0, 1.0, harm=("reed", 0.5)), op("b", 2.0, 0.35, 0.5, 2.0, 1.0, 1.0, harm=("reed", 0.4)),
               op("c", 3.0, 0.2, 0.5, 2.0, 1.0, 1.0), op("d", 4.0, 0.12, 0.6, 2.0, 1.0, 1.0),
               ftype="BP", cutoff=900.0, reso=0.4, ins="chorus", ins_amt=0.5, ins_p2=0.7, **amp(0.4, 0.3, 1.0, 1.0), level=-9.0, send_reverb=0.35,
               **lfo(1, "cutoff", 0.15, rate=0.12, shape="smooth"))


# ── KIT lanes: 1 kick, 2 snare, 3 clap/rim, 4 closed hat, 5 open hat, 6 low tom, 7 high tom, 8 perc ──
def lane(algo, *ops, note=36, pitch=48.0, ftype="OFF", choke=0, **kw):
    base = {"lane_note": note, "kit_pitch": pitch, "choke": choke, "vel_amp": 0.85}
    base.update(amp(0.0005, 0.3, 1.0, 0.08))
    base.update(kw)
    return snd(algo, *ops, ftype=ftype, **base)


def k_kick(pitch=31.0, sweep=26.0, sweep_t=0.035, decay=0.45, click=0.12, fold=0.05, drive=0.15, sub=False):
    """Boom-bap / sub kick: 2 ops + pitch sweep + click noise + fold."""
    return lane("PAIRS", op("a", 1.0, 0.85, 0.0008, decay, 0.0, 0.08, vel=0.3), op("b", 1.0, 0.25, 0.0005, 0.03, 0.0, 0.03, vel=0.6),
                note=36, pitch=pitch, penv_depth=sweep, penv_time=sweep_t, fold=fold, drive=drive, noise_level=click * 1.6, noise_decay=0.014,
                noise_att=0.0007, noise_freq=2600.0, noise_q=0.8, noise_mode=1, level=1.0 if not sub else 2.0, **amp(0.0008, 0.3, 1.0, decay * 0.25))


def k_snare(pitch=54.0, tone_dec=0.14, noise=0.55, noise_dec=0.2, base=110.0, rev=0.2, drive=0.25, crush=0.0, level=2.0):
    return lane("PAIRS", op("a", 1.0, 0.45, 0.0005, tone_dec, 0.0, 0.05), op("b", 1.6, 0.3, 0.0005, 0.05, 0.0, 0.04),
                op("c", 2.32, 0.25, 0.0005, 0.09, 0.0, 0.04), op("d", 1.0, 0.15, 0.0005, 0.03, 0.0, 0.03),
                note=38, pitch=pitch, penv_depth=7.0, penv_time=0.02, drive=drive, base=base, crush=crush,
                noise_level=noise, noise_decay=noise_dec, noise_freq=3800.0, noise_q=0.6, noise_mode=1, send_reverb=rev, level=level)


def k_brush(level=-2.0):
    """Brushed snare: noise with a soft attack, band-passed, no tone."""
    return lane("CHOIR", op("a", 1.0, 0.0), note=38, noise_level=0.8, noise_decay=0.18, noise_att=0.012, noise_freq=3000.0, noise_q=0.5,
                noise_mode=1, ftype="HP", cutoff=900.0, reso=0.0, send_reverb=0.15, level=level)


def k_clap(level=6.0):
    return lane("CHOIR", op("a", 1.0, 0.0), note=39, noise_level=0.9, noise_decay=0.13, noise_freq=1250.0, noise_q=1.1, noise_mode=1,
                ftype="HP", cutoff=600.0, reso=0.1, send_reverb=0.3, level=level)


def k_rim(level=-4.0):
    return lane("PAIRS", op("a", 1.0, 0.6, 0.0003, 0.04, 0.0, 0.02, fixed=1700.0), op("b", 1.0, 0.4, 0.0003, 0.015, 0.0, 0.01, fixed=520.0),
                note=37, noise_level=0.2, noise_decay=0.01, noise_freq=4000.0, noise_mode=2, send_reverb=0.25, level=level)


def _hat_ops(dec):
    return (op("a", 1.0, 0.3, 0.0005, dec, 0.0, 0.03, fixed=3410.0, vel=0.4), op("b", 1.0, 0.6, 0.0005, dec, 0.0, 0.03, fixed=4730.0),
            op("c", 1.0, 0.22, 0.0005, dec, 0.0, 0.03, fixed=6120.0, vel=0.4), op("d", 1.0, 0.5, 0.0005, dec, 0.0, 0.03, fixed=8650.0, fb=0.6))


def k_chat(dec=0.05, level=-7.0, cutoff=6500.0, lp=22000.0):
    return lane("PAIRS", *_hat_ops(dec), note=42, noise_level=0.35, noise_decay=dec * 0.9, noise_freq=8000.0, noise_mode=2,
                ftype="HP", cutoff=cutoff, reso=0.15, width=lp, choke=1, pan=0.15, level=level)


def k_ohat(dec=0.42, level=-9.0):
    return lane("PAIRS", *_hat_ops(dec), note=46, noise_level=0.35, noise_decay=dec, noise_freq=8000.0, noise_mode=2,
                ftype="HP", cutoff=6000.0, reso=0.15, choke=1, pan=0.15, level=level, send_delay=0.08, **amp(0.0005, 0.3, 1.0, dec * 0.5))


def k_ride(level=-11.0):
    """Ride: inharmonic fixed partials with feedback and a long, soft decay; the bell is the 2.3 kHz partial."""
    return lane("PAIRS", op("a", 1.0, 0.4, 0.0005, 1.6, 0.0, 0.8, fixed=2310.0), op("b", 1.0, 0.55, 0.0005, 1.2, 0.0, 0.6, fixed=3370.0, fb=0.4),
                op("c", 1.0, 0.25, 0.0005, 1.8, 0.0, 0.8, fixed=5260.0), op("d", 1.0, 0.45, 0.0005, 0.9, 0.0, 0.5, fixed=7890.0, fb=0.7),
                note=46, noise_level=0.12, noise_decay=0.6, noise_freq=9000.0, noise_mode=2, ftype="HP", cutoff=1800.0, choke=0,
                pan=-0.2, level=level, send_reverb=0.12, **amp(0.0005, 0.3, 1.0, 0.8))


def k_shaker(level=-12.0):
    return lane("CHOIR", op("a", 1.0, 0.0), note=70, noise_level=0.7, noise_decay=0.06, noise_att=0.008, noise_freq=7000.0, noise_q=0.9,
                noise_mode=1, ftype="HP", cutoff=4000.0, pan=-0.25, level=level)


def k_tom(note, pitch, pan, level=-3.0, decay=0.38):
    return lane("PAIRS", op("a", 1.0, 0.7, 0.0005, decay, 0.0, 0.1, vel=0.3), op("b", 1.5, 0.22, 0.0005, 0.05, 0.0, 0.04, vel=0.5),
                note=note, pitch=pitch, penv_depth=12.0, penv_time=0.06, drive=0.15, noise_level=0.08, noise_decay=0.03,
                noise_freq=2000.0, pan=pan, send_reverb=0.18, level=level)


def k_cowbell(level=-9.0):
    return lane("CHOIR", op("a", 1.0, 0.45, 0.0005, 0.16, 0.0, 0.06, fixed=562.0), op("b", 1.0, 0.35, 0.0005, 0.12, 0.0, 0.05, fixed=845.0),
                note=56, ftype="BP", cutoff=1100.0, reso=0.35, pan=-0.3, level=level, send_delay=0.15)


def k_impact(level=0.0):
    """Dark ambient impact: noise burst with a 2 s low-passed decay plus a sweeping sub body."""
    return lane("PAIRS", op("a", 1.0, 0.8, 0.002, 2.0, 0.0, 1.0), op("b", 1.0, 0.2, 0.002, 0.4, 0.0, 0.2),
                note=49, pitch=24.0, penv_depth=18.0, penv_time=0.25, noise_level=0.7, noise_decay=2.0, noise_freq=600.0, noise_q=0.5,
                noise_mode=0, fold=0.1, drive=0.2, send_reverb=0.5, level=level, **amp(0.002, 0.3, 1.0, 1.5))


def k_metal(level=-6.0, crush=0.0):
    """Metallic FM hit: fixed inharmonic partials 1 : 1.41 : 2.76 : 5.4 over 233 Hz, long decay."""
    return lane("CHOIR", op("a", 1.0, 0.45, 0.0005, 1.5, 0.0, 1.0, fixed=233.0), op("b", 1.0, 0.35, 0.0005, 1.2, 0.0, 0.8, fixed=328.5, fb=0.3),
                op("c", 1.0, 0.25, 0.0005, 1.0, 0.0, 0.6, fixed=643.0), op("d", 1.0, 0.2, 0.0005, 0.8, 0.0, 0.5, fixed=1258.0, fb=0.6),
                note=70, crush=crush, ftype="LP12", cutoff=6000.0, pan=0.3, send_reverb=0.5, send_delay=0.15, level=level, **amp(0.0005, 0.3, 1.0, 1.0))


def k_heart(level=-2.0):
    return k_kick(pitch=26.0, sweep=14.0, sweep_t=0.06, decay=1.2, click=0.02, fold=0.0, drive=0.05, sub=True) | {"level": level, "lane_note": 45}


def k_scrape(level=-8.0):
    s = lane("CHOIR", op("a", 1.0, 0.0), note=50, noise_level=0.8, noise_decay=1.2, noise_att=0.3, noise_freq=1200.0, noise_q=3.0, noise_mode=1,
             ftype="OFF", pan=-0.3, level=level, send_reverb=0.6, **amp(0.3, 0.3, 1.0, 0.8))
    s.update(lfo(1, "noise_freq", 0.35, rate=0.6, mode="trig", shape="saw"))
    return s


def k_vinylpop(level=-10.0):
    return lane("CHOIR", op("a", 1.0, 0.0), note=70, noise_level=0.9, noise_decay=0.004, noise_freq=2500.0, noise_q=0.7, noise_mode=1, level=level, crush=0.4)


def kit(lanes, bus=None):
    """lanes: dict lane number (1-8) → sound. bus: kit bus values (cutoff, reso, drive, age, sends, insert)."""
    b = {"cutoff": 20000.0, "ftype": FTYPE["OFF"], "a_level": 0.75, "b_level": 0.75, "c_level": 0.75, "d_level": 0.75}
    if bus:
        b.update(bus)
    s = {"0": b}
    for k, v in lanes.items():
        s[str(k)] = v
    return s


def kit_bristol(age=0.55):
    return kit({1: k_kick(31.0, 24.0, 0.035, 0.42), 2: k_snare(noise=0.6, noise_dec=0.22, crush=0.15, rev=0.25), 3: k_rim(),
                4: k_chat(0.045, -10.0, lp=9000.0), 5: k_ohat(0.4, -12.0), 6: k_tom(45, 40.0, -0.3), 7: k_shaker(-14.0), 8: k_vinylpop()},
               {"age": age, "ins_type": INS["lofi"], "ins_amt": 0.33, "ins_p1": 0.25, "ins_p2": 0.2, "ins_mix": 0.85, "cutoff": 9000.0, "send_reverb": 0.04})


def kit_sleepwalk():
    return kit({1: k_kick(29.0, 20.0, 0.04, 0.6), 2: k_brush(-1.0), 3: k_rim(-6.0), 4: k_chat(0.035, -12.0, lp=7000.0), 5: k_ohat(0.5, -14.0),
                6: k_tom(45, 38.0, -0.3, decay=0.5), 7: k_tom(50, 45.0, 0.3, decay=0.45), 8: k_shaker(-13.0)},
               {"age": 0.6, "cutoff": 8000.0, "ins_type": INS["lofi"], "ins_amt": 0.4, "ins_p1": 0.35, "ins_p2": 0.7})


def kit_round():
    return kit({1: k_kick(33.0, 18.0, 0.03, 0.38, click=0.06), 2: k_snare(56.0, noise=0.45, noise_dec=0.16, rev=0.18, level=0.0), 3: k_rim(-5.0),
                4: k_chat(0.04, -11.0), 5: k_ohat(0.38, -13.0), 6: k_tom(45, 41.0, -0.3), 7: k_shaker(-11.0), 8: k_cowbell(-12.0)},
               {"age": 0.25, "send_reverb": 0.06})


def kit_club():
    return kit({1: k_kick(34.0, 22.0, 0.025, 0.32, click=0.15, drive=0.2), 2: k_snare(57.0, noise=0.5, noise_dec=0.15, rev=0.12, level=1.0),
                3: k_snare(60.0, tone_dec=0.06, noise=0.3, noise_dec=0.06, rev=0.05, level=-8.0), 4: k_chat(0.035, -9.0), 5: k_ohat(0.32, -11.0),
                6: k_ride(-12.0), 7: k_tom(50, 47.0, 0.3), 8: k_cowbell(-11.0)},
               {"age": 0.15, "send_reverb": 0.08})


def kit_brush():
    return kit({1: k_kick(30.0, 16.0, 0.04, 0.4, click=0.04), 2: k_brush(0.0), 3: k_rim(-5.0), 4: k_chat(0.03, -14.0, lp=8000.0), 5: k_ohat(0.4, -15.0),
                6: k_ride(-11.0), 7: k_tom(50, 46.0, 0.3), 8: k_shaker(-14.0)}, {"age": 0.3, "send_reverb": 0.1})


def kit_tick():
    return kit({1: k_kick(28.0, 10.0, 0.06, 0.5, click=0.0) | {"level": -6.0}, 2: k_rim(-12.0), 3: k_rim(-14.0) | {"lane_note": 39},
                4: k_chat(0.02, -18.0) | {"pan": 0.4}, 5: k_ohat(0.6, -20.0), 6: k_metal(-14.0) | {"lane_note": 45},
                7: k_shaker(-18.0) | {"lane_note": 50}, 8: lane("CHOIR", op("a", 1.0, 0.3, 0.0005, 0.6, 0.0, 0.4, fixed=2600.0), note=70, level=-14.0, send_reverb=0.6, send_delay=0.3)},
               {"age": 0.4, "send_reverb": 0.2})


def kit_dark():
    return kit({1: k_heart(-3.0) | {"lane_note": 36}, 2: k_impact(-2.0) | {"lane_note": 38}, 3: k_rim(-10.0), 4: k_chat(0.03, -16.0, lp=6000.0),
                5: k_scrape(-9.0) | {"lane_note": 46, "choke": 0}, 6: k_heart(-4.0), 7: k_metal(-7.0, crush=0.3) | {"lane_note": 50},
                8: k_metal(-9.0) | {"lane_note": 70, "a_fixed": 177.0, "b_fixed": 249.6, "c_fixed": 488.5, "d_fixed": 955.8}},
               {"age": 0.5, "send_reverb": 0.15, "cutoff": 12000.0})


def kit_anvil():
    return kit({1: k_kick(28.0, 30.0, 0.06, 0.7, click=0.15, fold=0.25, drive=0.4, sub=True), 2: k_snare(52.0, noise=0.7, noise_dec=0.25, drive=0.5, crush=0.35, rev=0.12, level=3.0),
                3: k_clap(5.0), 4: k_chat(0.04, -10.0), 5: k_ohat(0.35, -12.0), 6: k_tom(45, 36.0, -0.3, decay=0.5), 7: k_metal(-9.0, crush=0.2) | {"lane_note": 50},
                8: k_impact(-6.0) | {"lane_note": 70}},
               {"age": 0.3, "drive": 0.2, "send_reverb": 0.04})


# ───────────────────────── patterns ─────────────────────────
VEL = {"X": 120, "x": 92, "o": 46}
COND = {"fill": "fill", "!fill": "!fill", "pre": "pre", "!pre": "!pre", "nei": "nei", "!nei": "!nei", "1st": "1st", "!1st": "!1st", "last": "last", "!last": "!last"}


class Pattern:
    def __init__(self, index, length=16, root=9, scale=1):
        self.index, self.length, self.root, self.scale = index, length, root, scale
        self.tracks = [dict(length=length, speed=4, swing=100, lane_len=None, steps={}) for _ in range(4)]

    def _step(self, t, lane, i):
        st = self.tracks[t]["steps"]
        return st.setdefault((lane, i), {"lane": lane, "i": i})

    def trig(self, t, i, lane=0, notes=None, vel=100, len=12, **kw):
        s = self._step(t, lane, i)
        s.pop("trig", None)
        if notes is not None:
            s["notes"] = notes if isinstance(notes, list) else [notes]
        if vel != 100:
            s["vel"] = vel
        if len != 12:
            s["len"] = len
        for k, v in kw.items():
            if k == "cond":
                if isinstance(v, tuple):
                    s["cond"], s["a"], s["b"] = v
                else:
                    s["cond"] = v
            else:
                s[k] = v
        return s

    def grid(self, t, lane, txt, offset=0, **kw):
        """'X . . . x . o .' → trigs on lane (x/X/o velocities); bars separated by '|' are concatenated."""
        cells = [c for c in txt.replace("|", " ").split() if c]
        vmap = dict(VEL)
        if "vel" in kw:
            b = kw.pop("vel")
            vmap = {"X": min(127, b + 25), "x": b, "o": max(1, b // 2)}
        for k, c in enumerate(cells):
            if c in vmap:
                self.trig(t, offset + k, lane=lane, vel=vmap[c], **kw)
        return self

    def notes(self, t, seq, offset=0, steps=1.0, vel=100, **kw):
        """seq: (step, note or [notes], length in steps (−1 tie) or None, velocity or None)."""
        for e in seq:
            i, nn = e[0], e[1]
            ln = e[2] if len(e) > 2 and e[2] is not None else steps
            v = e[3] if len(e) > 3 and e[3] is not None else vel
            self.trig(t, offset + i, notes=nn, vel=v, len=-1 if ln == -1 else int(round(ln * 12)), **kw)
        return self

    def chord(self, t, i, degree, octave=4, size=3, type=0, voicing=0, inversion=0, flags=0, len=12, vel=100, memory=None, **kw):
        s = self.trig(t, i, notes=[degree, octave] if memory is None else [degree], vel=vel, len=len, **kw)
        c = {"kind": "degree" if memory is None else "memory", "type": type if memory is None else memory, "size": size, "voicing": voicing}
        if inversion:
            c["inversion"] = inversion
        if flags:
            c["flags"] = flags
        s["chord"] = c
        return s

    def lock(self, t, i, lane=0, **locks):
        s = self._step(t, lane, i)
        if "notes" not in s and "vel" not in s and s.get("trig") is None and len(s) == 2:
            s["trig"] = 2
        s.setdefault("locks", {}).update(locks)
        return s

    def to_json(self):
        tr = []
        for t in self.tracks:
            d = {"length": t["length"], "steps": [t["steps"][k] for k in sorted(t["steps"])]}
            if t["speed"] != 4:
                d["speed"] = t["speed"]
            if t["swing"] != 100:
                d["swing"] = t["swing"]
            if t["lane_len"]:
                d["lane_len"] = t["lane_len"]
            tr.append(d)
        return {"index": self.index, "length": self.length, "root": self.root, "scale": self.scale, "tracks": tr}


SCALES = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 4, 6, 7, 9, 11], [0, 2, 4, 5, 7, 9, 10],
          [0, 1, 3, 5, 6, 8, 10], [0, 2, 3, 5, 7, 8, 11], [0, 1, 4, 5, 7, 8, 10], [0, 1, 4, 5, 7, 8, 11], [0, 2, 4, 7, 9], [0, 3, 5, 7, 10],
          [0, 2, 3, 7, 8], [0, 1, 5, 7, 10], [0, 2, 4, 6, 8, 10], [0, 2, 4, 6, 8, 9, 11], [0, 2, 3, 5, 7, 9, 11], [0, 2, 3, 5, 6, 8, 9, 11], list(range(12))]


def degree_note(root_pc, scale, degree, octave):
    """MIDI note of a scale degree (0 = root) at an octave (C4 = 60: octave 4)."""
    sc = SCALES[scale]
    o, k = divmod(degree, len(sc))
    return 12 * (octave + 1) + root_pc + 12 * o + sc[k]


# Scale indexes (SoundParams/Seq.h order: Orrery's 16 + 3).
SCALE = {"major": 0, "minor": 1, "dorian": 2, "phrygian": 3, "lydian": 4, "mixolydian": 5, "locrian": 6, "harmonic": 7,
         "phrygdom": 8, "dblharm": 9, "majpent": 10, "minpent": 11, "hirajoshi": 12, "insen": 13, "whole": 14, "lydaug": 15,
         "melodic": 16, "dim": 17, "chromatic": 18}
CT = {"diatonic": 0, "sus2": 1, "sus4": 2, "add9": 3, "quartal": 4, "cluster": 5, "fifths": 6, "shell": 7, "power": 8}
VC = {"close": 0, "drop2": 1, "drop3": 2, "spread": 3, "noroot": 4}
CF = {"rootbass": 1, "b9": 2, "#11": 4, "b5": 8}
NOTE = {n: i for i, n in enumerate(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}
NOTE.update({"Db": 1, "Eb": 3, "Gb": 6, "Ab": 8, "Bb": 10})


def n(name):
    """'E1' → MIDI 28 (C4 = 60)."""
    m = re.match(r"([A-G][b#]?)(-?\d)", name)
    return NOTE[m.group(1)] + 12 * (int(m.group(2)) + 1)


# ───────────────────────── projects ─────────────────────────
def project(name, category, order, desc, tempo, swing, tracks, patterns, chain=None, fx=None, params=None, patches=None, chord_mem=None):
    p = {"name": name, "category": category, "order": order, "description": desc}
    prm = {"tempo": tempo, "swing": swing, "pattern": 1}
    if params:
        prm.update(params)
    p["params"] = prm
    p["tracks"] = tracks
    p["patterns"] = [x.to_json() for x in patterns]
    if chain:
        p["chain"] = [list(c) + [0] * (4 - len(c)) for c in chain]
    p["fx"] = fx or {}
    if patches:
        p["patches"] = patches
    if chord_mem:
        p["chord_mem"] = chord_mem
    return p


def tone(sound, **kw):
    t = {"machine": "TONE", "sounds": {"0": sound}}
    t.update(kw)
    return t


def kit_track(k, **kw):
    t = {"machine": "KIT", "sounds": k}
    t.update(kw)
    return t


def fxs(delay="tape", reverb="spring", **kw):
    d = {"delay_mode": {"tape": 0, "dub": 1, "pingpong": 2}[delay], "reverb_mode": {"spring": 0, "plate": 1, "hall": 2, "void": 3}[reverb]}
    d.update(kw)
    return d


# ── per-genre arrangement templates for the factory presets ──
def drums_th(p, t=0, variant="A"):
    if variant == "A":
        p.grid(t, 0, "X . . . . . . x . . X . . . . .").grid(t, 1, ". . . . . . . . X . . o . . . .").grid(t, 3, "x . o . x . o . x . o . x . o .").grid(t, 2, ". . . . . . . . . . . . . . o .")
    elif variant == "B":
        p.grid(t, 0, "X . . . . . x . . . X . . . . x").grid(t, 1, ". . . . . . . . X . . . . o . .").grid(t, 3, "o . o . x . o . o . x . o . o x").grid(t, 4, ". . . . . . . . . . . . . . . x")
    else:
        p.grid(t, 0, "X . . . . . . . . . x . . . X .").grid(t, 1, ". . . . . . . . X . . . . . . o").grid(t, 3, "x . x . x . x . x . x . x . x .", vel=60).grid(t, 7, ". . . . . . . . . . . . . o . .")
    p.trig(t, 8, lane=1, vel=120, micro=8) if (1, 8) in p.tracks[t]["steps"] else None
    p.tracks[t]["swing"] = 100


def drums_dt(p, t=0, variant="A"):
    if variant == "A":
        p.grid(t, 0, "X . . . . . x . . . X . . . . .").grid(t, 1, ". . . . X . . . . . . . X . . o").grid(t, 3, "x . x . x . x . x . x . x . x .").grid(t, 6, "x o x o x o x o x o x o x o x o", vel=50).grid(t, 7, ". . . o . . . . . . o . . . . .")
    else:
        p.grid(t, 0, "X . . . . . . x . . X . . . x .").grid(t, 1, ". . . . X . . o . . . . X . . .").grid(t, 3, "x . . x . . x . x . . x . . x .").grid(t, 2, ". . . . x . . . . . . . x . . .")


def drums_aj(p, t=0, variant="A"):
    if variant == "A":
        p.grid(t, 0, "X . . . . . x . . x . . . . . .").grid(t, 1, ". . . . X . . o . o . . X . . o").grid(t, 3, "X o x o X o x o X o x o X o x o").grid(t, 4, ". . . . . . . . . . . . . . x .")
    elif variant == "B":
        p.grid(t, 0, "X . . . . . . . X . . x . . . .").grid(t, 1, ". . . . X . . . . . o . X . . .").grid(t, 5, "x . x x . x x . x . x x . x x .").grid(t, 3, ". . . . x . . . . . . . x . . .")
    else:
        p.grid(t, 0, "X . . . . . . X . . X . . . . .").grid(t, 1, ". . o . . . o . . . o . X . o .").grid(t, 5, "x . . x . . x . x . . x . . x .")


def drums_hv(p, t=0, variant="A"):
    if variant == "A":
        p.grid(t, 0, "X . . . . . . . . . X . . . . .").grid(t, 1, ". . . . . . . . X . . . . . . .").grid(t, 2, ". . . . . . . . X . . . . . . .").grid(t, 3, "x . x . x . x . x . x . x . x .")
    else:
        p.grid(t, 0, "X . . x . . . . . . X . . . x .").grid(t, 1, ". . . . . . . . X . . . . . . o").grid(t, 2, ". . . . . . . . X . . . . . . .").grid(t, 4, ". . . . . . . . . . . . . . x .").grid(t, 3, "x . x . x . x . x . x . x . . .")


def bank(defs, category, base_order):
    out = []
    for k, d in enumerate(defs):
        d = dict(d)
        d.setdefault("order", base_order + k)
        out.append(project(category=category, **d))
    return out


def make_all():
    presets = []
    presets.append(project("Init", "Init", 0, "Empty project: a KIT track and three TONE tracks, 88 BPM.", 88, 56,
                           [kit_track(kit_bristol()), tone(bass_heavy_detuned()), tone(ep_tine()), tone(pad_warm())],
                           [Pattern(0)], fx=fxs()))
    # A project to try MIDI out in a DAW: track 4 is a MIDI machine (channel 1) playing a bass line and chords with
    # a lock sent as CC 74; track 2 sounds and doubles its notes on channel 2.
    P = Pattern(0, 32, NOTE["A"], SCALE["minor"])
    drums_th(P, 0, "A")
    for b in range(2):
        P.trig(1, 16 * b, notes=degree_note(NOTE["A"], SCALE["minor"], 0 if b == 0 else 5, 1), vel=110, len=6 * 12)
        P.chord(3, 16 * b, 0 if b == 0 else 5, 4, 4, 0, VC["drop2"], len=8 * 12, vel=90)
        P.trig(3, 16 * b + 10, notes=degree_note(NOTE["A"], SCALE["minor"], 4, 3), vel=100, len=24)
        P.lock(3, 16 * b + 10, cutoff=600.0 if b == 0 else 3000.0)
    presets.append(project("MIDI Out", "Init", 1, "Track 4 is a MIDI machine on channel 1 (notes, chords, a lock as CC 74); track 2 sounds and doubles on channel 2. Route a DAW MIDI track's input from DeVicenti.",
                           84, 56, [kit_track(kit_bristol()), tone(bass_heavy_detuned(), double=1, out_channel=2), tone(ep_tine()), {"machine": "MIDI", "out_channel": 1, "sounds": {"0": {}}}],
                           [P], fx=fxs(), params={"t1_level": 0.0, "t2_level": -4.0}))
    from genre_presets import genre_banks
    presets += genre_banks()
    # The demo clips' projects (clips.py), at the end so the earlier program numbers stay where they were.
    from clips import presets as clip_presets
    presets += clip_presets()
    return presets


def main():
    sys.path.insert(0, HERE)
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"):
            os.remove(os.path.join(OUT, f))
    allp = make_all()
    # Track balance measured by headroom.py (dB added to each track's level).
    lp = os.path.join(HERE, "preset_levels.json")
    levels = json.load(open(lp)) if os.path.exists(lp) else {}
    for p in allp:
        for k, a in enumerate(levels.get(p["name"], [])):
            key = f"t{k + 1}_level"
            p["params"][key] = round(max(-60.0, min(6.0, p["params"].get(key, 0.0) + a)), 1)
    names = set()
    for k, p in enumerate(allp):
        assert p["name"] not in names, p["name"]
        names.add(p["name"])
        slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
        with open(os.path.join(OUT, f"p{k:02d}_{slug}.json"), "w") as fh:
            json.dump(p, fh, separators=(",", ":"), sort_keys=False)
    cats = {}
    for p in allp:
        cats[p["category"]] = cats.get(p["category"], 0) + 1
    print(f"{len(allp)} presets:", ", ".join(f"{c} {k}" for c, k in cats.items()))


if __name__ == "__main__":
    main()
