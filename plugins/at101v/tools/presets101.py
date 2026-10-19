#!/usr/bin/env python3
"""Genera los presets de fábrica del AT101V en assets/presets/NN_nombre.json.

Valores del panel como en el SH-101: 0..10 para sliders y perillas; selectores como índice
(rango 0=16' 1=8' 2=4' 3=2'; onda LFO 0=tri 1=cuadrada 2=random 3=ruido; PWM 0=LFO 1=MAN 2=ENV;
sub 0=1 oct 1=2 oct 2=2 oct pulso; VCA 0=ENV 1=GATE; trig 0=G+T 1=GATE 2=LFO; porta 0=AUTO 1=OFF 2=ON;
transpose 0=L 1=M 2=H; arpegio 0=off 1=down 2=U&D 3=up).

Las secuencias son listas de pasos en texto: "C2", "D#2 acc", "-", "G2 slide", "C3 tie", "A2 50%".
"""
import json, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")

CHOICE = {"lfo_wave", "vco_range", "pwm_src", "sub_wave", "vca_mode", "env_trig", "porta_mode", "transpose", "arp_mode", "hold", "seq_play"}
INIT = dict(lfo_rate=5.5, lfo_wave=0, vco_mod=0, vco_range=1, pw=0, pwm_src=1, mix_pulse=0, mix_saw=10, mix_sub=0, sub_wave=0,
            mix_noise=0, vcf_freq=6.2, vcf_res=0, vcf_env=3, vcf_mod=0, vcf_kybd=10, vca_mode=0, env_trig=0, env_a=0, env_d=4.5,
            env_s=6, env_r=3, tune=5, volume=7.5, portamento=0, porta_mode=1, transpose=1, bend_vco=2, bend_vcf=0, bend_lfo=5,
            arp_mode=0, hold=0, seq_play=0)

def rate_for_hz(hz):
    """LFO/CLK RATE (0..10) para una frecuencia: f = 0.1·300^x."""
    import math
    return round(10 * math.log(hz / 0.1) / math.log(300), 2)

presets = []
def preset(name, cat, seq=None, settings=None, **panel) -> None:
    p = dict(INIT); p.update(panel)
    out = {k: (v if k in CHOICE else round(v / 10.0, 4)) for k, v in p.items()}
    d = dict(name=name, category=cat, panel=out, settings=settings or {})
    if seq: d["sequence"] = seq.split() if isinstance(seq, str) else seq
    presets.append(d)

def steps(text):
    """'C2 acc | - | D#2 slide' → lista de pasos."""
    return [s.strip() for s in text.split("|")]

HOST = dict(clock_src=1, clock_div=3)

# ───────── Init ─────────
preset("Init", "Init")

# ───────── Bass ─────────
preset("101 Bass", "Bass", vco_range=0, mix_saw=8, mix_sub=7, vcf_freq=3.4, vcf_res=3, vcf_env=5, vcf_kybd=6, env_d=3.5, env_s=2, env_r=1.5)
preset("Pulse Bass", "Bass", vco_range=0, mix_saw=0, mix_pulse=9, pw=4, mix_sub=6, vcf_freq=3.0, vcf_res=2, vcf_env=5.5, vcf_kybd=5, env_d=3.2, env_s=1, env_r=1.2)
preset("Sub Rumble", "Bass", vco_range=0, mix_saw=4, mix_sub=10, sub_wave=1, vcf_freq=2.4, vcf_res=1, vcf_env=3, vcf_kybd=4, env_d=5, env_s=7, env_r=2.5)
preset("Rubber Bass", "Bass", vco_range=0, mix_saw=9, mix_sub=5, vcf_freq=2.6, vcf_res=5.5, vcf_env=6.5, vcf_kybd=5, env_d=3.8, env_s=0, env_r=2,
       porta_mode=0, portamento=2.2, env_trig=1)
preset("Square Sub", "Bass", vco_range=0, mix_saw=0, mix_pulse=8, pw=0, mix_sub=8, sub_wave=2, vcf_freq=3.8, vcf_res=1, vcf_env=3, vcf_kybd=5, env_d=4, env_s=5, env_r=1.5)
preset("Gate Bass", "Bass", vco_range=0, mix_saw=10, mix_sub=6, vcf_freq=4.0, vcf_res=4, vcf_env=4, vcf_kybd=7, vca_mode=1, env_d=2.5, env_s=0, env_r=1)
preset("Funk Pluck", "Bass", vco_range=1, mix_saw=6, mix_pulse=6, pw=3, mix_sub=4, vcf_freq=2.8, vcf_res=4.5, vcf_env=6, vcf_kybd=6, env_d=2.6, env_s=0, env_r=1.6,
       settings=dict(vel_vcf=0.5))
preset("Drive Bass", "Bass", vco_range=0, mix_saw=10, mix_pulse=6, pw=2, mix_sub=8, vcf_freq=3.6, vcf_res=3.5, vcf_env=4.5, vcf_kybd=6, env_d=4, env_s=4, env_r=2,
       settings=dict(drive=0.55))

# ───────── Lead ─────────
preset("Classic Lead", "Lead", mix_saw=8, mix_pulse=6, pwm_src=0, pw=5, lfo_rate=4.8, vcf_freq=5.4, vcf_res=2.5, vcf_env=3.5, env_a=0.5, env_d=5, env_s=7, env_r=3,
       porta_mode=0, portamento=2.5, bend_lfo=6)
preset("Whistle", "Lead", mix_saw=0, vcf_res=10, vcf_freq=5.0, vcf_env=0, vcf_kybd=10, env_a=1, env_d=6, env_s=9, env_r=3.5, porta_mode=0, portamento=3,
       settings=dict(dly_on=1, dly_mix=0.2))
preset("Bright Saw", "Lead", vco_range=2, mix_saw=10, vcf_freq=7.2, vcf_res=2, vcf_env=2, env_d=5, env_s=8, env_r=3, porta_mode=0, portamento=1.5)
preset("PWM Solo", "Lead", mix_saw=0, mix_pulse=10, pwm_src=0, pw=7, lfo_rate=3.8, vcf_freq=6, vcf_res=1.5, vcf_env=2.5, env_a=1, env_d=5, env_s=8, env_r=3,
       settings=dict(chorus=1, chorus_mix=0.4))
preset("Singing Res", "Lead", mix_saw=7, vcf_freq=4.2, vcf_res=7.8, vcf_env=4, vcf_kybd=8, env_a=2, env_d=6, env_s=6, env_r=4, porta_mode=0, portamento=3.5,
       settings=dict(rev_on=1, rev_mix=0.2))
preset("Unison Lead", "Lead", mix_saw=10, mix_pulse=5, pw=2, vcf_freq=6.3, vcf_res=2, vcf_env=3, env_d=5, env_s=8, env_r=3, porta_mode=0, portamento=2,
       settings=dict(unison=3, detune=18, spread=0.8))
preset("Envelope PWM", "Lead", mix_pulse=10, mix_saw=0, pwm_src=2, pw=8, vcf_freq=5.5, vcf_res=2, vcf_env=3, env_a=0, env_d=6, env_s=3, env_r=3)

# ───────── Seq (secuencias que tocan solas) ─────────
acid = steps("C2 acc | C2 | C3 slide | C2 | D#2 acc | - | C2 | G2 slide | C2 | A#1 | C2 acc | C3 | - | D#2 | F2 slide | G2 acc")
preset("Acid Line", "Seq", seq=acid, settings=dict(**HOST, accent=0.7), seq_play=1, mix_saw=10, vcf_freq=2.4, vcf_res=8.2, vcf_env=6, vcf_kybd=4,
       env_d=3.4, env_s=0, env_r=1.5, env_trig=0)
preset("Acid Drive", "Seq", seq=steps("A1 acc | A1 | A2 slide | A1 | - | A1 acc | C2 | D2 slide | A1 | E2 acc | A1 | G2 slide | A1 | - | A2 acc | C3 slide"),
       settings=dict(**HOST, accent=0.8, drive=0.6, dly_on=1, dly_time=6, dly_mix=0.18),
       seq_play=1, mix_saw=10, vcf_freq=2.2, vcf_res=8.6, vcf_env=6.5, vcf_kybd=4, env_d=3, env_s=0, env_r=1.2)
preset("Berlin Night", "Seq", seq=steps("C2 | G2 | C3 | D#3 | C2 | G2 | C3 | F3 | C2 | G2 | C3 | D#3 | A#1 | F2 | A#2 | D3"),
       settings=dict(**HOST, gate_len=0.4, dly_on=1, dly_time=6, dly_fb=0.45, dly_mix=0.28, rev_on=1, rev_mix=0.15),
       seq_play=1, mix_saw=6, mix_pulse=6, pw=3, mix_sub=4, vcf_freq=3.6, vcf_res=5, vcf_env=4, vcf_mod=1.5, lfo_rate=0.8, env_d=3.2, env_s=0, env_r=2.5)
preset("Electro Funk", "Seq", seq=steps("E1 acc | - | E2 | E1 | - | E1 | G1 acc | - | A1 | - | A1 tie | A1 | B1 acc | D2 | - | E2 slide"),
       settings=dict(**HOST, gate_len=0.55, accent=0.6),
       seq_play=1, vco_range=1, mix_pulse=9, pw=4, mix_saw=0, mix_sub=6, vcf_freq=3.0, vcf_res=4, vcf_env=5.5, vcf_kybd=5, env_d=3, env_s=0, env_r=1.2)
preset("Minimal Pulse", "Seq", seq=steps("C2 acc | C2 | C2 | C2 acc | C2 | C2 | C3 | C2 | C2 acc | C2 | C2 | C2 | D#2 acc | C2 | C2 | A#1"),
       settings=dict(**HOST, gate_len=0.3, accent=0.5, rev_on=1, rev_mix=0.12),
       seq_play=1, mix_saw=0, mix_pulse=10, pw=6, vcf_freq=3.3, vcf_res=6, vcf_env=4.5, vcf_mod=2, lfo_rate=0.6, env_d=2.4, env_s=0, env_r=1)
preset("Italo Octaves", "Seq", seq=steps("A1 | A2 | A1 | A2 | A1 | A2 | A1 | A2 | F1 | F2 | F1 | F2 | G1 | G2 | G1 | G2"),
       settings=dict(**HOST, gate_len=0.45, chorus=2, chorus_mix=0.4, dly_on=1, dly_time=6, dly_mix=0.15),
       seq_play=1, mix_saw=10, mix_sub=4, vcf_freq=4.0, vcf_res=3, vcf_env=4, vcf_kybd=6, env_d=3, env_s=1, env_r=1.5)
preset("Motorik", "Seq", seq=steps("D2 | D2 | A2 | D2 | D2 | C3 | D2 | A2 | D2 | D2 | F2 | D2 | G2 | D2 | A2 | C3"),
       settings=dict(**HOST, gate_len=0.6, dly_on=1, dly_time=8, dly_fb=0.3, dly_mix=0.2),
       seq_play=1, mix_saw=7, mix_sub=8, sub_wave=1, vcf_freq=3.8, vcf_res=3.5, vcf_env=3.5, vcf_mod=1, lfo_rate=1.2, env_d=4, env_s=3, env_r=1.5)
preset("Techno Stab", "Seq", seq=steps("C3 acc | - | - | C3 | - | - | C3 acc | - | - | D#3 | - | C3 | - | - | G3 slide | -"),
       settings=dict(**HOST, gate_len=0.35, accent=0.8, rev_on=1, rev_size=0.5, rev_mix=0.25, dly_on=1, dly_time=6, dly_mix=0.22),
       seq_play=1, mix_saw=10, mix_pulse=8, pw=5, vcf_freq=4.4, vcf_res=6.5, vcf_env=5, env_d=2.6, env_s=0, env_r=2.5)
preset("Probability Bass", "Seq", seq=steps("C2 | C2 50% | G2 25% | C2 acc | A#1 50% | C2 | D#2 75% | C2 | C2 | F2 50% | C2 acc | G1 25% | C2 | C3 50% | A#1 | G2 slide 50%"),
       settings=dict(**HOST, accent=0.6, gate_len=0.5),
       seq_play=1, vco_range=1, mix_saw=9, mix_sub=6, vcf_freq=3.0, vcf_res=5.5, vcf_env=5, vcf_kybd=5, env_d=3, env_s=0, env_r=1.2)
preset("Hold Me", "Seq", seq=steps("C2 tie | C2 | G2 | A#2 tie | A#2 | G2 | F2 slide | D#2 | C2 tie | C2 | D#2 | G2 slide | F2 tie | F2 | D#2 | D2 slide"),
       settings=dict(**HOST, gate_len=0.7), seq_play=1, env_trig=1, mix_saw=8, mix_pulse=4, pw=2, vcf_freq=3.4, vcf_res=4.5, vcf_env=4, vcf_kybd=6, env_d=5, env_s=3, env_r=2)

# ───────── Arp (mantener un acorde; HOLD lo deja corriendo) ─────────
preset("Arp Up", "Arp", arp_mode=3, lfo_rate=rate_for_hz(6.5), mix_saw=8, mix_pulse=5, pw=3, vcf_freq=4.4, vcf_res=4, vcf_env=4.5, env_d=2.6, env_s=0, env_r=2,
       settings=dict(dly_on=1, dly_time=5, dly_mix=0.2))
preset("Arp U&D Host", "Arp", arp_mode=2, mix_saw=10, vcf_freq=4.6, vcf_res=5, vcf_env=4, env_d=2.4, env_s=0, env_r=2,
       settings=dict(**HOST, arp_oct=1, gate_len=0.5, dly_on=1, dly_time=6, dly_mix=0.22))
preset("Arp Down Bells", "Arp", arp_mode=1, lfo_rate=rate_for_hz(5), vco_range=2, mix_saw=0, mix_pulse=6, pw=1, vcf_freq=5, vcf_res=7, vcf_env=3, env_d=3.5, env_s=0, env_r=4,
       settings=dict(arp_oct=2, rev_on=1, rev_mix=0.3))
preset("Trance Gate Arp", "Arp", arp_mode=3, mix_saw=10, mix_sub=4, vcf_freq=5.6, vcf_res=3, vcf_env=2, env_d=1.6, env_s=0, env_r=0.8,
       settings=dict(**HOST, arp_oct=2, unison=2, detune=16, rev_on=1, rev_mix=0.18, dly_on=1, dly_time=6, dly_mix=0.25))

# ───────── Ambient ─────────
preset("Drift Sequence", "Ambient", seq=steps("C3 | - | G3 slide | - | D#3 50% | - | - | A#3 slide 50% | C4 | - | G3 | - | F3 25% | - | D#3 slide | -"),
       settings=dict(clock_src=1, clock_div=1, gate_len=0.8, dly_on=1, dly_time=9, dly_fb=0.6, dly_tone=0.4, dly_mix=0.35, rev_on=1, rev_size=0.9, rev_damp=0.5, rev_mix=0.45,
                     chorus=1, chorus_mix=0.4),
       seq_play=1, env_trig=1, mix_saw=0, mix_pulse=8, pwm_src=0, pw=6, lfo_rate=2.5, vcf_freq=4.6, vcf_res=3, vcf_env=2.5, env_a=5, env_d=7, env_s=6, env_r=7,
       portamento=2.5, porta_mode=1)
preset("Glass Bells", "Ambient", seq=steps("E4 | - | B4 50% | - | G4 | - | D5 25% | - | A4 | - | E5 50% | - | F#4 | - | C#5 25% | -"),
       settings=dict(clock_src=1, clock_div=1, gate_len=0.3, dly_on=1, dly_time=7, dly_fb=0.55, dly_mix=0.35, rev_on=1, rev_size=0.95, rev_mix=0.5),
       seq_play=1, mix_saw=0, vcf_res=9.6, vcf_freq=4.5, vcf_env=1.5, vcf_kybd=10, env_d=5.5, env_s=0, env_r=6)
preset("Dark Drone", "Ambient", mix_saw=8, mix_sub=10, sub_wave=1, mix_noise=1, vcf_freq=2.6, vcf_res=6, vcf_env=0, vcf_mod=2.5, lfo_rate=0.4, env_a=7, env_d=8, env_s=10, env_r=8,
       vco_range=0, settings=dict(unison=3, detune=22, spread=0.9, rev_on=1, rev_size=0.95, rev_mix=0.4, chorus=1))
preset("Ocean Wind", "Ambient", mix_saw=0, mix_noise=10, vcf_freq=4.2, vcf_res=7, vcf_env=0, vcf_mod=4.5, lfo_rate=0.2, vcf_kybd=10, env_a=8, env_d=8, env_s=10, env_r=8,
       settings=dict(rev_on=1, rev_size=0.9, rev_mix=0.45, chorus=2, chorus_mix=0.6))
preset("Soft Pad", "Ambient", mix_saw=6, mix_pulse=8, pwm_src=0, pw=6, lfo_rate=2.2, vcf_freq=4.8, vcf_res=1.5, vcf_env=2, env_a=6.5, env_d=7, env_s=8, env_r=7.5,
       settings=dict(voices=3, unison=1, detune=10, chorus=2, chorus_mix=0.6, rev_on=1, rev_size=0.85, rev_mix=0.35))
preset("Self-Playing Bleeps", "Ambient", seq=["C3"], seq_play=1, lfo_wave=2, lfo_rate=rate_for_hz(3), vco_mod=6.5, vcf_mod=3, mix_saw=0, mix_pulse=8, pw=3,
       vcf_freq=4.6, vcf_res=7, vcf_env=3, env_d=2.6, env_s=0, env_r=3,
       settings=dict(dly_on=1, dly_time=5, dly_fb=0.5, dly_mix=0.35, rev_on=1, rev_mix=0.3))

# ───────── Poly (extensión) ─────────
preset("Poly Brass", "Poly", mix_saw=10, mix_pulse=4, pw=2, vcf_freq=4.4, vcf_res=1.5, vcf_env=4.5, env_a=2.2, env_d=5, env_s=7, env_r=3.5,
       settings=dict(voices=3, detune=8, chorus=1, chorus_mix=0.35, rev_on=1, rev_mix=0.15, vel_vcf=0.4))
preset("Poly Keys", "Poly", mix_pulse=8, pw=4, mix_saw=4, vcf_freq=4.6, vcf_res=2, vcf_env=4, env_d=5.5, env_s=2, env_r=4,
       settings=dict(voices=3, vel_vcf=0.6, vel_vca=0.5, chorus=2, chorus_mix=0.5))
preset("Poly Pluck", "Poly", mix_saw=8, mix_pulse=6, pw=3, vcf_freq=3.6, vcf_res=4.5, vcf_env=5.5, env_d=3.4, env_s=0, env_r=3.6,
       settings=dict(voices=4, vel_vcf=0.5, dly_on=1, dly_time=6, dly_mix=0.25, rev_on=1, rev_mix=0.2))

# ───────── FX ─────────
preset("Sample & Hold", "FX", env_trig=2, lfo_wave=2, lfo_rate=rate_for_hz(7), vcf_mod=5, mix_saw=10, vcf_freq=3.8, vcf_res=7.5, vcf_env=2, env_d=2.5, env_s=0, env_r=2,
       settings=dict(dly_on=1, dly_time=5, dly_mix=0.2))
preset("Zap Tom", "FX", mix_saw=0, vcf_res=9.4, vcf_freq=2.0, vcf_env=7, vcf_kybd=6, env_d=3.4, env_s=0, env_r=3.4)
preset("Noise Snare", "FX", mix_saw=0, mix_noise=10, vcf_freq=6.2, vcf_res=2, vcf_env=3.5, env_d=2.6, env_s=0, env_r=2.6)
preset("Laser", "FX", mix_saw=0, mix_pulse=10, vco_range=3, vco_mod=10, lfo_wave=1, lfo_rate=8.5, vcf_freq=7, vcf_res=5, vcf_env=0, env_d=4, env_s=0, env_r=3,
       settings=dict(dly_on=1, dly_mix=0.3))
preset("Siren", "FX", mix_saw=8, vco_mod=8, lfo_rate=3.2, vcf_freq=6.5, vcf_res=3, vcf_env=0, env_s=10, env_r=4)

os.makedirs(OUT, exist_ok=True)
for f in os.listdir(OUT):
    if f.endswith(".json"): os.remove(os.path.join(OUT, f))
for i, p in enumerate(presets):
    slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
    json.dump(p, open(os.path.join(OUT, f"{i:02d}_{slug}.json"), "w"), indent=1)
print(len(presets), "presets")
