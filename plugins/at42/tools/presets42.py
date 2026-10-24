#!/usr/bin/env python3
"""Genera los presets de fábrica del AT42 en assets/presets/NN_nombre.json.

Los drones se afinan en notas reales: la frecuencia de cada oscilador sigue f = 1/(a + b·(1 − x))
(el pote de TUNE en el circuito de carga, ver Source/dsp/Drone42.h) y VOLT la escala; acá se
invierte esa curva para pedir notas. Valores del panel 0..1 (selectores como índice).
"""
import json, math, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")
LO = [16.35, 61.74, 146.83, 329.63, 783.99]
HI = [98.0, 196.0, 493.88, 1046.5, 2349.3]
NOTES = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
CARTS = ["Cathedral", "Magic", "Time", "Vibrotrem", "Filter", "Vibe", "Pitch Shifter", "Infinity", "String Ringer", "Syntex-1", "Digital", "Generator", "Ochre"]

def hz(note):
    m = re.match(r"([A-G]#?)(-?\d)", note)
    return 440.0 * 2 ** ((NOTES[m.group(1)] + 12 * (int(m.group(2)) + 1) - 69) / 12)

def volt_scale(volt):
    vs = 14.0 - 4.4 * volt
    r = (vs - 9.0) / (vs - 1.5)
    return math.log(1 / 0.4) / math.log(1 / r)

def ldr_mul(ambient):
    """Con MOD encendido la luz ambiente sube el oscilador (Drone42.h: 1 + 0,65·luz^0,8): +264 centésimos con la de fábrica."""
    return 1.0 + 0.65 * ambient ** 0.8

def tune_for(i, note, volt, mul=1.0):
    f = hz(note) / volt_scale(volt) / mul
    if not (LO[i] * 1.0001 <= f <= HI[i] * 0.9999): print(f"  ojo: {note} fuera del rango del oscilador {i + 1}, queda al borde")
    f = min(max(f, LO[i] * 1.0001), HI[i] * 0.9999)
    a, b = 1 / HI[i], 1 / LO[i] - 1 / HI[i]
    return round(1.0 - (1.0 / f - a) / b, 4)

presets = []

def drone(v, notes, volt=0.15, mute=(), mod=(), att=0.3, rls=0.55, cv=0.5, hold=True, orb=(), ambient=0.18):
    """mod: MOD encendido tal cual (la luz sube esos osciladores). orb: MOD encendido con TUNE compensado, así con la luz
    ambiente suena la nota pedida y la mano sobre el orbe la baja (1.0.8)."""
    d = {f"d{v}_volt": volt, f"d{v}_att": att, f"d{v}_rls": rls, f"d{v}_cv": cv, f"d{v}_hold": 1 if hold else 0}
    for i, n in enumerate(notes):
        if n is None: d[f"d{v}_mute{i+1}"] = 1; continue
        d[f"d{v}_tune{i+1}"] = tune_for(i, n, volt, ldr_mul(ambient) if (i + 1) in orb else 1.0)
        d[f"d{v}_mute{i+1}"] = 1 if (i + 1) in mute else 0
        d[f"d{v}_mod{i+1}"] = 1 if (i + 1) in mod or (i + 1) in orb else 0
    return d

def mix(**vols):
    names = ["d1", "d2", "d3", "ext", "vcoa", "vcob", "pre", "d4", "d5", "d6"]
    d = {f"mix_vol{i+1}": 0.0 for i in range(10)}
    for k, v in vols.items():
        if isinstance(v, tuple): d[f"mix_vol{names.index(k)+1}"], d[f"mix_pan{names.index(k)+1}"] = v
        else: d[f"mix_vol{names.index(k)+1}"] = v
    return d

def fx(cl, pl, cr=None, pr=None, x=0.5, y=0.5, z=0.5, blend=0.4):
    return {"fx_cart_l": CARTS.index(cl), "fx_prog_l": pl - 1, "fx_cart_r": CARTS.index(cr or cl), "fx_prog_r": (pr or pl) - 1,
            "fx_x": x, "fx_y": y, "fx_z": z, "fx_blend": blend}

def cable(src, dst, color="red"): return {"src": src, "dst": dst, "color": color}

def preset(name, cat, panel=None, settings=None, cables=None, keyseq=None, demo_keys="1,2", demo_notes="", **extra):
    p = {}
    for part in (panel or []): p.update(part)
    p.update(extra)
    d = dict(name=name, category=cat, panel=p, settings=settings or {}, cables=cables or [], keyseq=keyseq or [], demoKeys=demo_keys, demoNotes=demo_notes)
    presets.append(d)

# ───────── Init ─────────
# El teclado toca VCO A por su camino normalizado (CV y EG A sin cables): con sólo los drones en el mezclador, Init
# quedaba mudo con MIDI (QA en DAWs 2026-10-22, C1).
# Las seis teclas drone suenan (1.0.8): antes 3 a 6 tenían el mezclador en cero y no se oía nada.
preset("Init", "Init", [mix(d1=0.6, d2=0.6, vcoa=0.6, d3=0.45, d4=0.6, d5=0.6, d6=0.45)], demo_keys="1")

# ───────── Drones ─────────
preset("Cathedral of A", "Drone",
       [drone(1, ["A1", "E2", "A3", "C5", "E6"], orb=(4,)), drone(2, ["A1", "B2", "E3", "G4", "B5"], mute=(5,), orb=(4,)),
        mix(d1=(0.62, 0.3), d2=(0.55, 0.7)), fx("Cathedral", 1, x=0.55, y=0.35, z=0.8, blend=0.55)],
       flt_freq_l=0.8, flt_freq_r=0.8)
preset("Low Choir", "Drone",
       [drone(1, ["D1", "A1", "D3", "F4", "A5"], volt=0.25, att=0.55, rls=0.7, orb=(4,)), drone(4, ["D1", "D2", "A3", "D5", "F6"], volt=0.25, att=0.6, rls=0.7, mute=(1,), orb=(4,)),
        mix(d1=(0.6, 0.25), d4=(0.6, 0.75)), fx("Infinity", 1, x=0.4, y=0.3, z=0.75, blend=0.5)],
       flt_freq_l=0.62, flt_freq_r=0.66, flt_res_l=0.2, flt_res_r=0.2, demo_keys="1,4")
preset("Swarm", "Drone",
       [drone(1, ["E1", "B1", "E3", "G#4", "B5"], volt=0.62), drone(2, ["E1", "B1", "E3", "B4", "E6"], volt=0.68),
        mix(d1=(0.55, 0.2), d2=(0.55, 0.8)), fx("Time", 2, x=0.45, y=0.2, z=0.6, blend=0.35)],
       flt_freq_l=0.7, flt_freq_r=0.72, flt_res_l=0.35, flt_res_r=0.35)
preset("Fifth Machine", "Drone",
       [drone(1, ["C1", "G1", "C3", "G4", "C6"]), drone(2, ["G1", "D2", "G3", "D5", "G6"]), drone(4, ["C2", "G2", "D4", "A4", "E6"], mute=(1, 2)),
        mix(d1=(0.5, 0.35), d2=(0.45, 0.65), d4=(0.4, 0.5)), fx("Cathedral", 3, x=0.4, y=0.4, z=0.75, blend=0.45)],
       demo_keys="1,2,4")
preset("Microtonal Fog", "Drone",
       [drone(1, ["F#1", "C#2", "F#3", "A#4", "D#6"], volt=0.2, mod=(3, 4, 5)), drone(5, ["F#1", "C#2", "G3", "B4", "E6"], volt=0.22, mod=(2, 3, 4)),
        mix(d1=(0.55, 0.3), d5=(0.55, 0.7)), fx("Infinity", 3, x=0.6, y=0.5, z=0.35, blend=0.45)],
       settings={"ambient": 0.35}, cables=[cable("lfoa_out", "d1_cvin", "red"), cable("lfob_out", "d5_cvin", "yellow")],
       lfoa_rate=0.25, lfob_rate=0.2, lfob_wave=0.0, d1_cv=0.6, d5_cv=0.5, demo_keys="1,5")
preset("Volt Storm", "Drone",
       [drone(1, ["A0", "E1", "A2", "E4", "A5"], volt=0.92), drone(2, ["C1", "G1", "C3", "G4", "C6"], volt=0.86),
        mix(d1=(0.5, 0.15), d2=(0.5, 0.85)), fx("Digital", 1, x=0.35, y=0.55, z=0.4, blend=0.3)],
       flt_freq_l=0.66, flt_freq_r=0.6, flt_res_l=0.45, flt_res_r=0.4, flt_dist=0.35, flt_gain=0.4)

# ───────── Ambient ─────────
preset("Shimmer Garden", "Ambient",
       [drone(1, ["G1", "D2", "G3", "B4", "F#6"], att=0.7, rls=0.8, orb=(4, 5)), drone(2, ["E1", "B1", "D4", "F#4", "A5"], att=0.75, rls=0.8, orb=(5,)),
        mix(d1=(0.5, 0.25), d2=(0.5, 0.75)), fx("Cathedral", 1, x=0.75, y=0.25, z=0.9, blend=0.7)],
       flt_freq_l=0.74, flt_freq_r=0.76, settings={"long_rel": 1})
preset("Breathing Orbs", "Ambient",
       [drone(1, ["D1", "A1", "D3", "F#4", "A5"], mod=(2, 3, 4, 5)), drone(2, ["A1", "E2", "A3", "C#5", "E6"], mod=(3, 4)),
        mix(d1=(0.55, 0.3), d2=(0.5, 0.7)), fx("Infinity", 1, x=0.45, y=0.35, z=0.8, blend=0.55)],
       cables=[cable("lfoa_out", "d1_cvin"), cable("lfoa_out", "d2_cvin", "yellow")], lfoa_rate=0.12, lfoa_wave=0.0, d1_cv=0.55, d2_cv=0.45)
preset("Tape Memory", "Ambient",
       [drone(4, ["C2", "G2", "E3", "B4", "D6"], att=0.6, rls=0.75, orb=(4,)), drone(5, ["A1", "E2", "C4", "G4", "E6"], att=0.6, rls=0.75, orb=(5,)),
        mix(d4=(0.55, 0.3), d5=(0.55, 0.7)), fx("Time", 3, x=0.55, y=0.2, z=0.45, blend=0.5)],
       flt_freq_l=0.6, flt_freq_r=0.62, flt_res_l=0.1, settings={"vintage": 0.4}, demo_keys="4,5")
preset("Ocean of Noise", "Ambient",
       [mix(d3=(0.6, 0.3), d6=(0.6, 0.7)), fx("Cathedral", 3, x=0.5, y=0.5, z=0.9, blend=0.6)],
       d3_noise=1.0, d3_pitch=0.0, d3_fmamt=1.0, d3_div=1.0, d3_hold=1, d6_noise=1.0, d6_pitch=0.0, d6_fmamt=1.0, d6_div=1.0, d6_hold=1,
       flt_freq_l=0.55, flt_freq_r=0.6, flt_res_l=0.55, flt_res_r=0.5,
       cables=[cable("lfoa_out", "flt_cvl", "blue"), cable("lfob_out", "flt_cvr", "white")], lfoa_rate=0.15, lfob_rate=0.05, lfob_speed=0,
       flt_mod_l=0.25, flt_mod_r=0.25, demo_keys="3,6")
preset("Reverse Hymn", "Ambient",
       [drone(1, ["F1", "C2", "F3", "A4", "C6"], att=0.5, rls=0.6, orb=(4,)), drone(2, ["D#1", "A#1", "G3", "D5", "G6"], att=0.5, rls=0.6, orb=(4,)),
        mix(d1=(0.55, 0.2), d2=(0.5, 0.8)), fx("Ochre", 3, x=0.7, y=0.55, z=0.3, blend=0.6)])

# ───────── Sci-fi (Papa Srapa) ─────────
preset("Siren Tower", "Sci-Fi", [mix(d3=(0.65, 0.4), d6=(0.6, 0.6)), fx("Time", 1, x=0.4, y=0.35, z=0.5, blend=0.35)],
       d3_rate=0.45, d3_fmamt=0.6, d3_div=0.8, d3_pitch=0.55, d3_hold=1, d6_rate=0.3, d6_fmamt=0.5, d6_div=0.9, d6_pitch=0.65, d6_hi=1, d6_hold=1,
       demo_keys="3,6")
preset("Birds at Dusk", "Sci-Fi", [mix(d3=(0.6, 0.3), d6=(0.55, 0.7)), fx("Cathedral", 3, x=0.3, y=0.3, z=0.7, blend=0.45)],
       d3_rate=0.7, d3_fmamt=0.8, d3_div=0.15, d3_pitch=0.7, d3_hi=1, d3_am=1, d3_hold=1,
       d6_rate=0.62, d6_fmamt=0.7, d6_div=0.2, d6_pitch=0.78, d6_hi=1, d6_am=1, d6_hold=1, demo_keys="3,6")
preset("Space Monster", "Sci-Fi", [mix(d3=(0.65, 0.5)), fx("Magic", 3, x=0.55, y=0.4, z=0.3, blend=0.45)],
       d3_rate=0.55, d3_x10=1, d3_fmamt=0.85, d3_div=0.35, d3_pitch=0.25, d3_noise=0.2, d3_hold=1,
       flt_freq_l=0.55, flt_freq_r=0.55, flt_res_l=0.6, flt_res_r=0.6, flt_dist=0.4, demo_keys="3")
preset("Volleys", "Sci-Fi", [mix(d3=(0.6, 0.3), d6=(0.6, 0.7)), fx("Vibe", 2, x=0.6, y=0.3, z=0.4, blend=0.3)],
       d3_rate=0.62, d3_am=1, d3_fm=0, d3_noise=0.85, d3_pitch=0.2, d3_hold=1, d6_rate=0.5, d6_am=1, d6_fm=0, d6_noise=0.9, d6_pitch=0.1, d6_hold=1,
       demo_keys="3,6")
preset("Random Robot", "Sci-Fi", [mix(vcoa=(0.6, 0.45), vcob=(0.5, 0.55)), fx("String Ringer", 3, x=0.4, y=0.6, z=0.5, blend=0.4)],
       vcoa_wave=3, vcoa_oct=5, ega_loop=1, ega_a=0.0, ega_d=0.2, ega_s=0.0, vcob_wave=4, vcob_oct=4, vcob_fm=0.4, egb_hold=1,
       cables=[cable("d3_shout", "vcoa_voct", "yellow"), cable("seq_clkout", "d3_shclk", "red")], seq_pulser=0.62, demo_keys="")

# ───────── Sequences (con cables) ─────────
preset("Berlin Five", "Sequence", [mix(vcoa=(0.7, 0.4), vcob=(0.55, 0.6), d1=(0.3, 0.5)), drone(1, ["C1", "G1", "C3", None, None], rls=0.5),
       fx("Time", 1, x=0.5, y=0.38, z=0.3, blend=0.4)],
       vcoa_wave=2, vcoa_oct=3, vcob_wave=3, vcob_oct=4, vcob_pw=0.3, ega_a=0.0, ega_d=0.3, ega_s=0.0, ega_r=0.25, egb_hold=0,
       egb_a=0.0, egb_d=0.35, egb_s=0.1, egb_r=0.3, seq_pulser=0.58, seq_stages=2,
       seq_step1=0.0, seq_step2=0.4, seq_step3=0.6, seq_step4=0.2, seq_step5=0.8,
       flt_freq_l=0.58, flt_freq_r=0.6, flt_res_l=0.45, flt_res_r=0.45,
       cables=[cable("seq_cvout", "vcoa_voct", "red"), cable("seq_gateout", "ega_gate", "yellow"), cable("seq_cvout", "vcob_voct", "blue"),
               cable("seq_gateout", "egb_gate", "green"), cable("ega_out", "flt_cvl", "white")], flt_mod_l=0.12, demo_keys="1")
preset("Pulser Ritual", "Sequence", [mix(vcoa=(0.65, 0.5), d3=(0.4, 0.3), d6=(0.4, 0.7)), fx("Infinity", 2, x=0.45, y=0.5, z=0.5, blend=0.35)],
       vcoa_wave=5, vcoa_pw=0.3, vcoa_oct=4, ega_d=0.25, ega_s=0.0, seq_pulser=0.5, seq_stages=1,
       seq_step1=0.0, seq_step2=0.25, seq_step3=0.5, seq_step4=0.85,
       d3_noise=0.7, d3_pitch=0.1, d3_rls=0.2, d6_noise=0.5, d6_pitch=0.3, d6_rls=0.15,
       cables=[cable("seq_cvout", "vcoa_voct"), cable("seq_gateout", "ega_gate", "yellow"), cable("seq_gateout", "d3_gate", "white"),
               cable("seq_clkout", "d6_gate", "green")], demo_keys="")
preset("Techno Monster", "Sequence", [mix(vcoa=(0.75, 0.5), d3=(0.45, 0.5)), fx("Syntex-1", 3, x=0.4, y=0.6, z=0.5, blend=0.25)],
       vcoa_wave=3, vcoa_pw=0.4, vcoa_oct=3, ega_a=0.0, ega_d=0.22, ega_s=0.0, ega_r=0.15, seq_pulser=0.66, seq_stages=2,
       seq_step1=0.0, seq_step2=0.0, seq_step3=0.6, seq_step4=0.0, seq_step5=0.4, seq_gate3=0,
       d3_noise=1.0, d3_pitch=0.0, d3_att=0.0, d3_rls=0.1,
       flt_freq_l=0.5, flt_freq_r=0.5, flt_res_l=0.65, flt_res_r=0.65, flt_dist=0.5, flt_gain=0.5,
       cables=[cable("seq_cvout", "vcoa_voct"), cable("seq_gateout", "ega_gate", "yellow"), cable("ega_out", "flt_cvl", "white"),
               cable("ega_out", "flt_cvr", "blue"), cable("seq_clkout", "d3_gate", "green")], flt_mod_l=0.35, flt_mod_r=0.35, demo_keys="")

# ───────── Keys (teclado táctil, arpegiador y secuenciador) ─────────
preset("Flute Lead", "Keys", [mix(vcoa=(0.7, 0.5)), fx("Cathedral", 3, x=0.4, y=0.3, z=0.6, blend=0.35)],
       vcoa_wave=5, vcoa_pw=0.2, vcoa_oct=5, ega_a=0.25, ega_d=0.4, ega_s=0.7, ega_r=0.45,
       settings={"kb_scale": 11, "kb_porta": 0.25, "kb_vib": 0.35}, demo_keys="", demo_notes="0:0.6,3:0.6,5:0.6,7:1.2,10:0.6,7:1.6")
preset("Arp Constellation", "Keys", [mix(vcoa=(0.65, 0.35), vcob=(0.5, 0.65), d1=(0.3, 0.5)), drone(1, ["A1", "E2", None, None, None]),
       fx("Magic", 1, x=0.55, y=0.4, z=0.75, blend=0.45)],
       vcoa_wave=1, vcoa_oct=5, vcob_wave=4, vcob_oct=4, vcob_pw=0.8, ega_d=0.35, ega_s=0.0, egb_d=0.4, egb_s=0.0,
       settings={"kb_mode": 1, "kb_scale": 6, "kb_root": 9, "kb_dir": 2, "kb_hold": 1, "kb_bpm": 104}, demo_keys="1", demo_notes="0+3+7+10:8")
preset("Binaural VCOs", "Keys", [mix(vcoa=(0.65, 0.0), vcob=(0.65, 1.0)), fx("Vibrotrem", 3, x=0.3, y=0.2, z=0.5, blend=0.3)],
       vcoa_wave=2, vcoa_oct=4, vcob_wave=2, vcob_oct=4, vcob_tune=0.505, vcob_fm=0.0, ega_a=0.3, ega_s=0.8, egb_a=0.3, egb_s=0.8,
       flt_freq_l=0.62, flt_freq_r=0.62, flt_res_l=0.35, flt_res_r=0.35, width=1.0, demo_keys="", demo_notes="0+7:3,-2+5:3,-4+3:3")
preset("Pressure Pad", "Keys", [mix(vcoa=(0.6, 0.4), vcob=(0.55, 0.6)), fx("Infinity", 1, x=0.4, y=0.4, z=0.7, blend=0.5)],
       vcoa_wave=4, vcoa_pw=0.5, vcob_wave=4, vcob_pw=0.7, vcob_oct=4, ega_a=0.5, ega_s=0.9, ega_r=0.6, egb_a=0.55, egb_s=0.9, egb_r=0.6,
       cables=[cable("kb_press", "flt_cvl", "yellow"), cable("kb_press", "flt_cvr", "blue")], flt_freq_l=0.45, flt_freq_r=0.45, flt_mod_l=0.5, flt_mod_r=0.5,
       settings={"kb_press": 1, "kb_rise": 0.6, "kb_fall": 0.6, "kb_scale": 1}, demo_keys="", demo_notes="0+4+7:3,5+9+12:3")
preset("Sequenced Plates", "Keys", [mix(vcoa=(0.7, 0.45), vcob=(0.4, 0.55)), fx("Time", 1, x=0.55, y=0.38, z=0.35, blend=0.4)],
       vcoa_wave=3, vcoa_pw=0.35, vcoa_oct=4, ega_d=0.3, ega_s=0.0, ega_r=0.25, vcob_fm=0.3, vcob_oct=5, egb_d=0.3, egb_s=0.0,
       settings={"kb_mode": 2, "kb_scale": 11, "kb_bpm": 118}, keyseq=[0, 3, 7, 10, 12, 10, 7, 3, 0, 5, 7, 12, 15, 12, 7, 5],
       demo_keys="", demo_notes="0:4,5:4")

# ───────── Experimental ─────────
preset("Piezo Percussion", "Experimental", [mix(pre=(0.6, 0.5), d3=(0.4, 0.4)), fx("Filter", 1, x=0.6, y=0.7, z=0.4, blend=0.5)],
       pre_gain=0.6, d3_noise=0.8, d3_pitch=0.2, d3_att=0.0, d3_rls=0.15,
       cables=[cable("ef_gate", "d3_gate", "yellow"), cable("ef_env", "flt_cvl", "white")], flt_mod_l=0.3, demo_keys="")
preset("Joystick Space", "Experimental",
       [drone(1, ["B0", "F#1", "B2", "D#4", "F#5"]), drone(2, ["E1", "B1", "E3", "G#4", "B5"]),
        mix(d1=(0.55, 0.3), d2=(0.55, 0.7)), fx("Infinity", 2, x=0.4, y=0.4, z=0.5, blend=0.5)],
       cables=[cable("joy_xout", "fx_cvx", "red"), cable("joy_yout", "flt_cvl", "blue"), cable("joy_yout", "flt_cvr", "blue")],
       flt_mod_l=0.3, flt_mod_r=0.3, joy_x=0.7, joy_y=0.35)
preset("Ring Bells", "Experimental", [drone(4, ["G1", "D2", "G3", "B4", "D6"], att=0.0, rls=0.45, hold=False), mix(d4=(0.6, 0.5)),
       fx("String Ringer", 1, x=0.5, y=0.6, z=0.2, blend=0.6)],
       cables=[cable("seq_gateout", "d4_gate", "yellow")], seq_pulser=0.4, demo_keys="")
preset("Crushed Choir", "Experimental", [drone(1, ["C1", "G1", "D#3", "G4", "C6"], volt=0.3), drone(5, ["G1", "D2", "G3", "A#4", "D6"], volt=0.3),
       mix(d1=(0.55, 0.3), d5=(0.55, 0.7)), fx("Digital", 2, x=0.45, y=0.3, z=0.6, blend=0.55)], demo_keys="1,5")

os.makedirs(OUT, exist_ok=True)
# MASTER lo fija tools/level_presets.py: se conserva al regenerar (si no, todos volvían a 7,5 y había que nivelar de nuevo).
masters = {}
for f in os.listdir(OUT):
    if f.endswith(".json"):
        d = json.load(open(os.path.join(OUT, f)))
        if "master" in d.get("panel", {}): masters[d["name"]] = d["panel"]["master"]
        os.remove(os.path.join(OUT, f))
for i, p in enumerate(presets):
    slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
    if p["name"] in masters and "master" not in p["panel"]: p["panel"]["master"] = masters[p["name"]]
    json.dump(p, open(os.path.join(OUT, f"{i:02d}_{slug}.json"), "w"), indent=1)
print(len(presets), "presets")
