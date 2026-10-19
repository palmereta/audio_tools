#!/usr/bin/env python3
"""Presets de fábrica del ATFS1 (assets/presets/pNN_*.json). Sonidos propios: ninguno copia una voz
de Yamaha. Cada voz se escribe corta: lo que no se nombra queda en su valor por defecto (PresetFs.h).

Formas: 0 sine, 1 all1, 2 all2, 3 odd1, 4 odd2, 5 res1, 6 res2, 7 frmt.
Frecuencia fija: 440 Hz x 2^(coarse + fine/128 - 16)  → hz(f) da (coarse, fine).
"""
import json, math, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")
SINE, ALL1, ALL2, ODD1, ODD2, RES1, RES2, FRMT = range(8)

def hz(f):
    """Frecuencia fija → (coarse, fine) del FS1R."""
    x = math.log2(f / 440.0) + 16.0
    c = int(math.floor(x)); fine = int(round((x - c) * 128))
    if fine == 128: c += 1; fine = 0
    return max(0, min(21, c)), max(0, min(127, fine))

def eg(levels=(99, 99, 99, 0), times=(0, 20, 20, 30), hold=0, ts=0):
    return {"level": list(levels), "time": list(times), "hold": hold, "timeScale": ts}

def op(form=SINE, ratio=None, fixed_hz=None, level=99, skirt=0, bw=0, egv=None, **kw):
    o = {"form": form, "level": level, "skirt": skirt, "bwRes": bw}
    if fixed_hz is not None:
        c, f = hz(fixed_hz); o.update(fixed=True, coarse=c, fine=f)
    elif ratio is not None:
        if ratio == 0.5: o.update(coarse=0, fine=0)
        else:
            c = max(1, int(math.floor(ratio + 1e-6)))
            o.update(coarse=c, fine=int(round((ratio / c - 1.0) * 100)))
    if egv: o["eg"] = egv
    o.update(kw)
    return o

def nop(center=None, level=60, bw=30, egv=None, **kw):
    u = {"level": level, "bandwidth": bw}
    if center is not None:
        c, f = hz(center); u.update(coarse=c, fine=f)
    if egv: u["eg"] = egv
    u.update(kw)
    return u

def voice(alg=0, fb=0, ops=(), nops=(), **common):
    v = {"algorithm": alg, "feedback": fb}
    ops = list(ops) + [op(level=0)] * (8 - len(ops))
    nops = list(nops) + [nop(level=0)] * (8 - len(nops))
    v["ops"] = ops; v["nops"] = nops
    v.update(common)
    return v

def knob(dest, op_, depth, unvoiced=0): return [dest, unvoiced, op_, depth]   # dest 1 level, 2 freq, 3 width

PRESETS = []
def preset(name, cat, v, params=None, fseq=None, demo=None):
    p = {"name": name, "category": cat, "voice": dict(v, name=name[:10]), "params": params or {}}
    if fseq: p["fseq"] = fseq
    if demo: p["demoNotes"] = demo
    PRESETS.append(p)

# ───────── voces con Formant Sequence ─────────
def fseq_voice(levels=None, bw=(40, 44, 48, 52, 56, 60, 64, 68), skirt=1, n_level=70, release=34, pmod=4, attack=6):
    """La base de todas las voces de Fseq: 8 formantes fijos y 8 ruidos, cada par en su pista."""
    ops = [op(FRMT, fixed_hz=500 * (k + 1), level=99, skirt=skirt, bw=bw[k], fseq=True, fseqTrack=k, pitchMod=pmod,
              egv=eg((99, 99, 99, 0), (attack, 30, 30, release))) for k in range(8)]
    nops = [nop(2000 + 900 * k, level=n_level, bw=44, fseq=True, egv=eg((99, 99, 99, 0), (attack, 30, 30, release))) for k in range(8)]
    return dict(alg=0, ops=ops, nops=nops, lfo1Wave=4, lfo1Speed=62, lfo1Delay=40, lfo1Pmd=14)

FS = {"fseq": 1}
preset("Talking Formants", "Vo", voice(**fseq_voice()), dict(FS, rev_type=3, rev_send=0.18, var_type=0), "Audio Tools", "48:1.8,43:1.8")
preset("Shoo Bee Doo", "Vo", voice(**fseq_voice(pmod=5)), dict(FS, rev_type=6, rev_send=0.22, var_type=1, var_send=0.15), "Shoo Bee Doo", "50:1.9,53:1.9")
preset("Uno Dos Tres", "Vo", voice(**fseq_voice()), dict(FS, rev_type=3, rev_send=0.15, var_type=0), "Uno Dos Tres", "45:2.2")
preset("Robot Voice", "Vo", voice(**fseq_voice(pmod=0)), dict(FS, fseq_pitch=1, ins_type=7, ins_amount=0.35, ins_mix=0.7, var_type=4, var_send=0.25, rev_send=0.1),
       "I Am A Robot", "45:2.0,40:2.0")
preset("Vowel Choir", "Vo", voice(**fseq_voice(skirt=2, attack=40, release=48, pmod=6)),
       dict(FS, fseq_pitch=1, fseq_loop=2, fseq_lp_start=150, fseq_lp_end=301, fseq_speed=60, voices=24, attack=8,
            var_type=4, var_send=0.35, var_depth=0.7, rev_type=2, rev_send=0.35, rev_time=0.7), "Choir Ah", "48+55+60+64:4")
preset("High Choir", "Vo", voice(**fseq_voice(skirt=2, attack=46, release=50, pmod=6)),
       dict(FS, fseq_pitch=1, fseq_loop=2, fseq_lp_start=150, fseq_lp_end=301, fseq_speed=45, voices=24,
            var_type=4, var_send=0.35, rev_type=2, rev_send=0.4, rev_time=0.75), "Choir Oo", "60+64+67+72:4")
preset("La La Lead", "Vo", voice(**fseq_voice(pmod=6, release=30)), dict(FS, mono=1, porta=1, porta_time=30, var_type=9, var_send=0.18, var_rate=0.55, rev_send=0.2),
       "La La La", "60:0.6,62:0.6,64:0.6,67:1.2")
preset("Whisper Pad", "Vo", voice(**fseq_voice(attack=50, release=52, n_level=92)),
       dict(FS, fseq_pitch=1, vn_balance=40, fseq_loop=2, fseq_speed=50, var_type=1, var_send=0.3, rev_type=2, rev_send=0.45, rev_time=0.8), "Whisper Hey", "48+55+60:4")
preset("Vowel Morph Pad", "Pd", voice(**fseq_voice(skirt=2, attack=52, release=55)),
       dict(FS, fseq_pitch=1, fseq_loop=2, fseq_lp_start=0, fseq_lp_end=225, fseq_speed=25, fseq_smooth=1, voices=24,
            var_type=4, var_send=0.4, rev_type=2, rev_send=0.4, rev_time=0.8), "Ooh Aah", "45+52+57+60:5")

# ───────── secuencias rítmicas ─────────
preset("Yah Yah Riff", "Sq", voice(**fseq_voice(release=26)), dict(FS, fseq_pitch=1, fseq_loop=1, fseq_lp_start=0, fseq_lp_end=191, var_type=8, var_send=0.25, var_rate=0.45, rev_send=0.12),
       "Yah Yah", "45:1.5,48:1.5,43:1.5")
preset("Wah Bass", "Bs", voice(**fseq_voice(release=20, bw=(36, 40, 44, 48, 52, 56, 60, 64))),
       dict(FS, fseq_pitch=1, fseq_loop=1, fseq_lp_start=0, fseq_lp_end=159, fseq_speed=120, mono=1, ins_type=2, ins_amount=0.35, ins_mix=0.6, rev_send=0.05, var_type=0),
       "Wah Wah", "33:1.2,36:0.6,31:1.2")
preset("Robot Talk Seq", "Sq", voice(**fseq_voice(release=22)), dict(FS, fseq_pitch=1, fseq_loop=1, fseq_lp_start=0, fseq_lp_end=111, fseq_speed=80,
       var_type=10, var_send=0.25, var_rate=0.5, var_depth=0.4, rev_send=0.1), "Robot Talk", "52:2,55:2")

# ───────── pads de formantes (sin Fseq) ─────────
def vowel_ops(f, levels, bw=46, skirt=1, eg_=None, pmod=5, **kw):
    """Formantes fijos en las frecuencias de una vocal: el núcleo de los pads y coros sin Fseq."""
    return [op(FRMT, fixed_hz=fr, level=lv, skirt=skirt, bw=bw + 4 * k, pitchMod=pmod, egv=eg_, **kw) for k, (fr, lv) in enumerate(zip(f, levels))]

slow = eg((99, 96, 92, 0), (52, 60, 60, 54))
preset("Formant Strings", "Pd", voice(0, 0, vowel_ops((700, 1150, 2450, 3300), (99, 92, 80, 70), bw=50, skirt=2, eg_=slow, pmod=4) +
                                      [op(ALL2, ratio=1.0, level=78, skirt=3, egv=slow, pitchMod=4, detune=6), op(ALL2, ratio=1.0, level=76, skirt=3, egv=slow, pitchMod=4, detune=-6)],
                                      lfo1Wave=4, lfo1Speed=58, lfo1Pmd=10, lfo1Delay=50,
                                      formantKnob=[knob(2, 0, 30), knob(2, 1, 30), knob(2, 2, 20), [0, 0, 0, 0], [0, 0, 0, 0]],
                                      fmKnob=[knob(1, 4, 40), knob(1, 5, 40), [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]),
       dict(voices=24, var_type=5, var_send=0.4, rev_type=2, rev_send=0.35, rev_time=0.7), demo="48+55+60+64:4")
preset("Glass Choir", "Pd", voice(0, 0, vowel_ops((330, 870, 2250, 3000), (99, 88, 74, 66), bw=40, skirt=1, eg_=slow, pmod=5) +
                                  [op(ALL1, ratio=2.0, level=72, skirt=2, egv=eg((99, 80, 60, 0), (40, 60, 70, 55))), op(SINE, ratio=4.0, level=58, egv=eg((99, 70, 50, 0), (30, 60, 70, 55)))],
                                  lfo1Wave=4, lfo1Speed=55, lfo1Pmd=9, lfo1Delay=40,
                                  formantKnob=[knob(2, 0, 40), knob(2, 1, 40), [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]),
       dict(voices=24, var_type=1, var_send=0.35, rev_type=6, rev_send=0.45, rev_time=0.8), demo="60+64+67+71:4")
preset("Noise Wind", "Fx", voice(0, 0, [op(level=0)] * 8,
                                 [nop(800, 92, 18, egv=slow, freqMod=6, resonance=4), nop(1900, 86, 22, egv=slow, freqMod=5), nop(4200, 78, 30, egv=slow, freqMod=7),
                                  nop(7500, 66, 40, egv=slow, freqMod=4)],
                                 lfo1Wave=5, lfo1Speed=30, lfo1Fmd=60),
       dict(var_type=3, var_send=0.35, var_depth=0.6, rev_type=7, rev_send=0.5, rev_time=0.85), demo="60:5")
preset("Resonant Sweep Pad", "Pd", voice(0, 0, [op(RES1, ratio=1.0, level=96, bw=3, skirt=2, egv=slow, pitchMod=3), op(RES2, ratio=1.0, level=92, bw=6, skirt=3, egv=slow, pitchMod=3, detune=5),
                                                op(RES1, ratio=0.5, level=84, bw=9, skirt=1, egv=slow, pitchMod=3), op(ALL2, ratio=1.0, level=70, skirt=4, egv=slow)],
                                         lfo1Wave=0, lfo1Speed=30, lfo1Pmd=6, lfo1Amd=12,
                                         filterType=2, cutoff=88, resonance=40, fegLevel=[30, 10, 0, -10], fegTime=[60, 70, 70, 60], fegDepth=40, lfo2Speed=20, lfo2FilterDepth=30),
       dict(filter=1, voices=16, var_type=4, var_send=0.35, rev_type=2, rev_send=0.4), demo="45+52+57+64:5")
preset("Odd Pad", "Pd", voice(0, 0, [op(ODD2, ratio=1.0, level=94, skirt=3, egv=slow, pitchMod=4), op(ODD2, ratio=1.0, level=92, skirt=3, egv=slow, pitchMod=4, detune=7),
                                     op(ODD1, ratio=2.0, level=80, skirt=2, egv=slow, pitchMod=4, detune=-6)],
                             lfo1Wave=4, lfo1Speed=50, lfo1Pmd=8, filterType=0, cutoff=92, resonance=10, fegLevel=[20, 0, -6, -10], fegTime=[50, 60, 60, 60], fegDepth=30),
       dict(filter=1, var_type=1, var_send=0.4, rev_type=2, rev_send=0.35), demo="48+55+62+67:4")

# ───────── leads ─────────
lead_eg = eg((99, 95, 90, 0), (2, 40, 50, 28))
preset("Formant Lead", "Ld", voice(0, 0, vowel_ops((640, 1190, 2390), (99, 94, 84), bw=40, skirt=1, eg_=lead_eg, pmod=6) +
                                   [op(SINE, ratio=1.0, level=86, egv=lead_eg, pitchMod=6)],
                                   lfo1Wave=4, lfo1Speed=64, lfo1Delay=45, lfo1Pmd=16,
                                   formantKnob=[knob(2, 0, 50), knob(2, 1, 63), knob(2, 2, 40), [0, 0, 0, 0], [0, 0, 0, 0]]),
       dict(mono=1, porta=1, porta_time=28, at_dest=6, at_depth=40, mw_dest=6, mw_depth=50, var_type=9, var_send=0.2, var_rate=0.5, rev_send=0.18), demo="60:0.5,62:0.5,64:0.5,67:1.5")
preset("Odd Square Lead", "Ld", voice(7, 3, [op(SINE, ratio=2.0, level=62, egv=eg((99, 70, 50, 0), (0, 40, 60, 30))), op(ODD1, ratio=1.0, level=99, skirt=3, egv=lead_eg, pitchMod=6),
                                             op(ODD2, ratio=1.0, level=0), op(ODD2, ratio=1.0, level=0)] ,
                                     lfo1Wave=4, lfo1Speed=66, lfo1Delay=40, lfo1Pmd=14,
                                     fmKnob=[knob(1, 0, 63), [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]),
       dict(mono=1, porta=1, porta_time=20, var_type=8, var_send=0.18, rev_send=0.15), demo="57:0.4,60:0.4,64:0.4,69:1.2")
preset("Res Brass", "Ld", voice(0, 0, [op(RES2, ratio=1.0, level=99, bw=4, skirt=2, egv=eg((99, 92, 86, 0), (14, 40, 50, 30)), pitchMod=5),
                                       op(RES1, ratio=1.0, level=92, bw=2, skirt=3, egv=eg((99, 90, 80, 0), (20, 40, 50, 30)), pitchMod=5, detune=4),
                                       op(ALL2, ratio=1.0, level=80, skirt=4, egv=eg((99, 90, 80, 0), (16, 40, 50, 30)), pitchMod=5)],
                               pegLevel=[-8, 0, 0, 0, 0], pegTime=[30, 30, 30, 30], lfo1Wave=4, lfo1Speed=60, lfo1Delay=50, lfo1Pmd=10,
                               filterType=1, cutoff=80, resonance=8, fegLevel=[40, 20, 10, 0], fegTime=[30, 50, 60, 40], fegDepth=45),
       dict(filter=1, voices=12, var_type=4, var_send=0.25, rev_send=0.25), demo="53+57+60:1.2,55+58+62:1.6")

# ───────── teclados y campanas (FM con las formas del FS1R) ─────────
ep_mod = eg((99, 60, 0, 0), (0, 58, 70, 40))
ep_car = eg((99, 80, 0, 0), (0, 62, 78, 42), ts=3)
# Algoritmo 6 (índice 5): dos pilas 1>2 y 3>4, portadoras 2 y 4.
preset("Tine Piano", "Ky", voice(5, 3, [op(SINE, ratio=14.0, level=56, egv=eg((99, 40, 0, 0), (0, 40, 60, 40)), ampVel=4),
                                        op(SINE, ratio=1.0, level=99, egv=ep_car, ampVel=3, pitchMod=1),
                                        op(SINE, ratio=1.0, level=70, egv=ep_mod, ampVel=5),
                                        op(SINE, ratio=1.0, level=96, egv=ep_car, ampVel=3, detune=3)]),
       dict(voices=16, var_type=6, var_rate=0.35, var_depth=0.35, var_send=0.6, rev_send=0.2), demo="60+64+67:1.5,62+65+69:1.5")
preset("Formant Bells", "Ky", voice(0, 0, [op(SINE, ratio=1.0, level=96, egv=eg((99, 70, 0, 0), (0, 70, 82, 60), ts=3)),
                                           op(SINE, ratio=3.5, level=84, egv=eg((99, 50, 0, 0), (0, 60, 74, 55), ts=3)),
                                           op(ALL1, ratio=5.0, level=72, skirt=1, egv=eg((99, 30, 0, 0), (0, 50, 64, 50), ts=3)),
                                           op(FRMT, fixed_hz=2600, level=70, bw=40, egv=eg((99, 20, 0, 0), (0, 44, 60, 46)))]),
       dict(voices=24, var_type=0, rev_type=6, rev_send=0.45, rev_time=0.75), demo="72:1,76:1,79:1,84:2")
preset("Glass Marimba", "Ky", voice(0, 0, [op(ALL1, ratio=1.0, level=97, skirt=1, egv=eg((99, 0, 0, 0), (0, 52, 60, 46), ts=4)),
                                           op(SINE, ratio=4.0, level=78, egv=eg((99, 0, 0, 0), (0, 44, 50, 40), ts=4)),
                                           op(RES1, ratio=1.0, level=70, bw=9, egv=eg((99, 0, 0, 0), (0, 40, 50, 40), ts=4))]),
       dict(voices=16, rev_type=3, rev_send=0.3, var_type=0), demo="60:0.4,64:0.4,67:0.4,72:0.4,71:0.4,67:0.4,64:0.8")
preset("Formant Clav", "Ky", voice(0, 0, [op(ODD1, ratio=1.0, level=96, skirt=4, egv=eg((99, 60, 0, 0), (0, 50, 66, 22)), ampVel=4),
                                          op(FRMT, fixed_hz=1400, level=86, bw=34, skirt=0, egv=eg((99, 50, 0, 0), (0, 44, 60, 20)), ampVel=5),
                                          op(FRMT, fixed_hz=2800, level=72, bw=40, egv=eg((99, 40, 0, 0), (0, 40, 56, 20)), ampVel=6)],
                                  formantKnob=[knob(2, 1, 40), knob(2, 2, 40), [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]),
       dict(voices=12, ins_type=4, ins_amount=0.5, ins_mix=0.8, var_type=0, rev_send=0.12), demo="48:0.25,51:0.25,55:0.25,58:0.5,55:0.25,51:0.5")
preset("Formant Organ", "Ky", voice(0, 0, [op(ALL2, ratio=1.0, level=92, skirt=1, egv=eg((99, 99, 99, 0), (0, 0, 0, 14))),
                                           op(SINE, ratio=2.0, level=86, egv=eg((99, 99, 99, 0), (0, 0, 0, 14))),
                                           op(SINE, ratio=4.0, level=72, egv=eg((99, 99, 99, 0), (0, 0, 0, 14))),
                                           op(FRMT, fixed_hz=1800, level=74, bw=42, egv=eg((99, 99, 99, 0), (0, 0, 0, 14)))]),
       dict(voices=24, ins_type=6, ins_amount=0.8, ins_mix=1.0, var_type=0, rev_send=0.2), demo="48+55+60+64:2,50+57+62+65:2")

# ───────── bajos ─────────
preset("FM Bass", "Bs", voice(7, 6, [op(SINE, ratio=1.0, level=70, egv=eg((99, 50, 30, 0), (0, 40, 60, 24))),
                                     op(SINE, ratio=1.0, level=98, egv=eg((99, 90, 80, 0), (0, 50, 60, 24)))]),
       dict(mono=1, var_type=0, rev_send=0.04), demo="36:0.3,36:0.3,48:0.3,36:0.3,43:0.3,46:0.6")
preset("Formant Bass", "Bs", voice(0, 0, [op(SINE, ratio=1.0, level=99, egv=eg((99, 92, 88, 0), (0, 40, 50, 22))),
                                          op(FRMT, fixed_hz=420, level=90, bw=30, egv=eg((99, 80, 60, 0), (0, 40, 50, 20))),
                                          op(FRMT, fixed_hz=900, level=80, bw=34, egv=eg((99, 60, 40, 0), (0, 36, 50, 20)))],
                                  formantKnob=[knob(2, 1, 50), knob(2, 2, 50), [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]),
       dict(mono=1, var_type=0, rev_send=0.03, ins_type=2, ins_amount=0.2, ins_mix=0.5), demo="33:0.4,33:0.2,40:0.4,43:0.6")
preset("Odd Sub Bass", "Bs", voice(0, 0, [op(ODD1, ratio=0.5, level=99, skirt=2, egv=eg((99, 95, 90, 0), (0, 40, 50, 20))),
                                          op(SINE, ratio=1.0, level=82, egv=eg((99, 70, 50, 0), (0, 40, 50, 20)))],
                                  filterType=0, cutoff=70, resonance=30, fegLevel=[40, 0, -10, -10], fegTime=[0, 40, 50, 30], fegDepth=50),
       dict(filter=1, mono=1, var_type=0, rev_send=0.03), demo="36:0.3,39:0.3,43:0.3,46:0.3,48:0.6")

# ───────── efectos ─────────
preset("Space Talk", "Fx", voice(**fseq_voice(release=60)), dict(FS, fseq_speed=70, var_type=9, var_send=0.45, var_rate=0.3, var_depth=0.6, rev_type=7, rev_send=0.55, rev_time=0.9),
       "Synthesizer", "40:3")
preset("Formant Scratch", "Fx", voice(**fseq_voice()), dict(FS, mw_dest=10, mw_depth=63, rev_send=0.15, var_type=0), "One Two", "48:3")
preset("Breath Flute", "Ld", voice(0, 0, [op(SINE, ratio=1.0, level=96, egv=eg((99, 95, 92, 0), (10, 40, 50, 30)), pitchMod=5),
                                          op(SINE, ratio=2.0, level=60, egv=eg((99, 90, 80, 0), (12, 40, 50, 30)), pitchMod=5)],
                                  [nop(level=74, bw=24, mode=1, transpose=12, egv=eg((99, 60, 40, 0), (4, 40, 50, 30))),
                                   nop(level=62, bw=40, mode=1, transpose=24, egv=eg((99, 40, 20, 0), (4, 30, 50, 30)))],
                                  lfo1Wave=4, lfo1Speed=62, lfo1Delay=50, lfo1Pmd=12),
       dict(mono=1, porta=1, porta_time=16, var_type=0, rev_type=3, rev_send=0.3), demo="72:0.5,74:0.5,76:0.5,79:1.5")
preset("Init Formant", "Ky", voice(0, 0, [op(FRMT, fixed_hz=800, level=99, bw=44, skirt=1, egv=eg((99, 99, 99, 0), (0, 20, 20, 30)))]),
       dict(var_type=0, rev_send=0.1), demo="48:1,55:1")

def main():
    levels = os.path.join(ROOT, "tools", "levelsfs1.json")      # tools/level_presets.py
    lv = json.load(open(levels)) if os.path.exists(levels) else {}
    for p in PRESETS:
        if p["name"] in lv: p["params"]["volume"] = lv[p["name"]]
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, p in enumerate(PRESETS):
        fn = f"p{i + 1:02d}_{p['name'].lower().replace(' ', '_').replace('/', '_')}.json"
        json.dump(p, open(os.path.join(OUT, fn), "w"), indent=1)
    print(len(PRESETS), "presets")

if __name__ == "__main__":
    main()
