#!/usr/bin/env python3
"""Genera los presets de fábrica (assets/presets/*.json) a partir de las cartas de ajuste.

Los valores se escriben como en las cartas del manual: diales 0..10, PITCH y MASTER TUNE −5..+5,
selectores por nombre. Lo que no figura queda en el "Normal Setting" (OM p.4).
Fuente de los parches 01–14: docs/hardware-spec.md §6 (Owner's Manual y Setting Examples).
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")

W1 = {"tri": 0, "saw": 1, "pw": 2, "noise": 3}
S1 = {"32": 0, "16": 1, "8": 2, "4": 3}
W2 = {"saw": 0, "square": 1, "pulse": 2, "ring": 3}
S2 = {"16": 0, "8": 1, "4": 2, "2": 3}
BIPOLAR = {"vco2_pitch", "master_tune"}

NORMAL = dict(vco1_wave="saw", vco1_pw=0, vco1_scale="8", vco2_wave="saw", vco2_pitch=0, vco2_scale="8",
              portamento=0, master_tune=0, vco1_level=10, vco2_level=10, fm_mg=0, fm_eg1=0,
              hpf_cutoff=0, hpf_peak=0, hpf_mod_mg=0, hpf_mod_eg2=0,
              lpf_cutoff=10, lpf_peak=0, lpf_mod_mg=0, lpf_mod_eg2=0,
              mg_wave=5, mg_freq=5, eg1_delay=0, eg1_attack=0, eg1_release=0,
              eg2_hold=0, eg2_attack=0, eg2_decay=0, eg2_sustain=10, eg2_release=0,
              volume=7, esp_level=5, esp_lowcut=10, esp_highcut=0, esp_cvadj=5, esp_thresh=5)

def eg2(h, a, d, s, r): return dict(eg2_hold=h, eg2_attack=a, eg2_decay=d, eg2_sustain=s, eg2_release=r)
def mix(a, b): return dict(vco1_level=a, vco2_level=b)
def hpf(fc, pk, mg=0, eg=0): return dict(hpf_cutoff=fc, hpf_peak=pk, hpf_mod_mg=mg, hpf_mod_eg2=eg)
def lpf(fc, pk, mg=0, eg=0): return dict(lpf_cutoff=fc, lpf_peak=pk, lpf_mod_mg=mg, lpf_mod_eg2=eg)

PRESETS = [
    ("Synthe Bass", "Factory", dict(vco1_wave="tri", vco1_scale="32", **mix(10, 0), **hpf(3, 5, eg=5), **lpf(4, 5, eg=6),
                                    fm_mg=5, **eg2(0, 0, 2, 3, 1)), [("wheel_out", "total_in")], {}),
    ("Hammond Organ", "Factory", dict(vco1_wave="pw", vco1_pw=1.5, vco1_scale="16", vco2_wave="square", vco2_pitch=3,
                                      vco2_scale="4", **mix(8, 10), **lpf(3.5, 5, mg=7, eg=5), **eg2(0, 0, 0, 3, 1)),
     [("kbd_cv_out", "total_in")], {}),
    ("Parallel Fifths", "Factory", dict(vco1_wave="saw", vco1_scale="32", vco2_wave="saw", vco2_scale="16", vco2_pitch=3.5,
                                        **mix(10, 10), **hpf(3, 5, eg=8), **lpf(4, 5, eg=8), **eg2(0, 1.5, 4, 5, 2)), [], {}),
    ("Voice", "Factory", dict(vco1_wave="saw", vco1_scale="32", **mix(10, 0), **hpf(5, 5, mg=5, eg=8), **lpf(5, 5, mg=7, eg=3),
                              **eg2(0, 1, 2, 5, 0)), [("wheel_out", "total_in")], {}),
    ("Pitch Bend", "Factory", dict(vco2_wave="square", vco2_scale="16", **mix(0, 10), **hpf(4, 2, eg=5), **lpf(5, 5, eg=5),
                                   fm_eg1=7, **eg2(0, 0, 3, 5, 0)), [("wheel_out", "freq_in")], {}),
    ("Steam Locomotive", "Factory", dict(vco1_wave="noise", **mix(10, 0), **hpf(4, 2, eg=10), **lpf(4.5, 5, eg=3),
                                         mg_freq=4.5, **eg2(0, 1, 1, 0, 1)),
     [("wheel_out", "hpf_cut_in"), ("mg_sq_out", "kbd_trig_in")], {}),
    ("UFO", "Factory", dict(vco1_wave="tri", vco1_scale="8", **mix(5, 0), **hpf(6, 10, mg=5), **lpf(6, 10, mg=5), fm_mg=5,
                            mg_wave=8, mg_freq=2.5, portamento=10, **eg2(10, 5, 5, 5, 5)), [], {}),
    ("Horn", "Factory", dict(vco1_wave="saw", vco1_scale="16", **mix(10, 0), **lpf(3.5, 2, eg=10), fm_mg=1.5, mg_freq=4.5,
                             **eg2(0, 1, 4, 5, 1)), [], {}),
    ("Violin", "Factory", dict(vco1_wave="saw", vco1_scale="8", **mix(10, 0), **lpf(7, 5), fm_mg=1.5, mg_freq=4.5,
                               **eg2(0, 1.5, 5, 5, 1)), [], {}),
    ("Sample & Hold", "Factory", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="saw", vco2_scale="4", vco2_pitch=3,
                                      **mix(10, 10), **hpf(5, 5), **lpf(6, 5, eg=10), fm_mg=10, mg_wave=10, mg_freq=5,
                                      **eg2(5, 5, 10, 10, 10)),
     [("pink_out", "sh_in"), ("mg_sq_out", "sh_clock_in"), ("sh_out", "total_in")], {}),
    ("Guitar Synth I", "Factory", dict(vco1_wave="saw", vco1_scale="16", vco2_wave="saw", vco2_scale="4", vco2_pitch=3,
                                       **mix(10, 10), **hpf(0, 5), **lpf(5, 5, eg=8), **eg2(0, 1, 2, 5, 2),
                                       esp_level=9, esp_lowcut=0, esp_highcut=5.5, esp_cvadj=10, esp_thresh=3),
     [("esp_fv_out", "sh_in"), ("eg1_out", "sh_clock_in"), ("sh_out", "vco12_cv_in"), ("esp_trig_out", "kbd_trig_in")], {}),
    ("Guitar Synth II", "Factory", dict(vco1_wave="pw", vco1_pw=7, vco1_scale="32", vco2_wave="square", vco2_pitch=3,
                                        vco2_scale="16", **mix(10, 10), **hpf(3, 5, eg=7), **lpf(4, 5, eg=8), fm_mg=2,
                                        mg_wave=9, mg_freq=6, **eg2(0, 0, 2, 5, 0),
                                        esp_level=9, esp_lowcut=0, esp_highcut=5.5, esp_cvadj=10, esp_thresh=3),
     [("esp_fv_out", "vco12_cv_in"), ("esp_trig_out", "kbd_trig_in"), ("mg_sq_out", "sh_clock_in"),
      ("pink_out", "sh_in"), ("sh_out", "hpf_cut_in")], {}),
    ("Trumpet", "Factory", dict(vco1_wave="saw", vco1_scale="8", **mix(10, 0), **lpf(5, 3, eg=10), fm_eg1=1, mg_freq=3.5,
                                eg1_delay=2, eg1_attack=2, **eg2(0, 1, 2, 7, 1)),
     [("pink_out", "vca_in"), ("vca_out", "freq_in")], {}),
    ("Clarinet", "Factory", dict(vco1_wave="pw", vco1_pw=2, vco1_scale="16", **mix(10, 0), **lpf(6, 4, eg=7), fm_mg=1,
                                 mg_freq=4, eg1_delay=2, eg1_attack=2, **eg2(0, 1, 2, 5, 1)),
     [("mg_tri_out", "vca_in"), ("vca_out", "total_in")], {}),
    # ── Propios ──
    ("Acid Bass", "Bass", dict(vco1_wave="saw", vco1_scale="16", **mix(10, 0), **hpf(1.5, 3), **lpf(3, 8, eg=6),
                               portamento=2.5, **eg2(0, 0, 3, 1.5, 1.5)), [], {}),
    ("Fat Saw Bass", "Bass", dict(vco1_wave="saw", vco1_scale="32", vco2_wave="saw", vco2_scale="16", vco2_pitch=0.08,
                                  **mix(10, 9), **hpf(1, 4), **lpf(3.5, 5, eg=5), **eg2(0, 0, 3.5, 4, 2)), [], {}),
    ("Hoover Square", "Bass", dict(vco1_wave="pw", vco1_pw=3, vco1_scale="16", vco2_wave="square", vco2_scale="8",
                                   vco2_pitch=0.15, **mix(10, 8), **hpf(2, 6), **lpf(5.5, 6, eg=3), portamento=3,
                                   fm_mg=0.8, mg_freq=6, **eg2(0, 0.5, 4, 7, 3)), [], {}),
    ("Scream Lead", "Lead", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="saw", vco2_scale="8", vco2_pitch=0.12,
                                 **mix(10, 8), **hpf(2.5, 7.5), **lpf(4.5, 8.8, eg=4), fm_mg=1.2, mg_freq=5.5,
                                 portamento=1.5, **eg2(0, 1, 4, 7, 2)), [], {}),
    ("Reso Pluck", "Lead", dict(vco1_wave="pw", vco1_pw=2.5, vco1_scale="8", **mix(10, 0), **hpf(2, 5), **lpf(2.5, 8, eg=7),
                                **eg2(0, 0, 3.2, 0, 3)), [], {}),
    ("Delayed Vibrato Lead", "Lead", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="square", vco2_scale="4",
                                          **mix(10, 6), **lpf(5, 6, eg=3), fm_mg=3, mg_freq=5.5, eg1_delay=4.5,
                                          eg1_attack=5, eg1_release=2, **eg2(0, 1, 3, 8, 2)),
     [("mg_tri_out", "vca_in"), ("vca_out", "total_in")], {}),
    ("Ring Bells", "Keys", dict(vco1_wave="pw", vco1_pw=3, vco1_scale="8", vco2_wave="ring", vco2_scale="4", vco2_pitch=2.3,
                                **mix(0, 10), **hpf(3, 3), **lpf(7, 3, eg=3), **eg2(0, 0, 6, 0, 6)), [], {}),
    ("MS Kick", "Drums", dict(vco1_wave="tri", vco1_scale="32", **mix(6, 0), **hpf(0, 0), **lpf(1.5, 9.4, eg=6),
                              **eg2(0, 0, 3.6, 0, 3.6)), [], {}),
    ("MS Snare", "Drums", dict(vco1_wave="noise", vco2_wave="square", vco2_scale="4", **mix(10, 4), **hpf(5, 4, eg=2),
                               **lpf(7, 3, eg=3), **eg2(0, 0, 2.5, 0, 2.5)), [], {}),
    ("MS Hi-Hat", "Drums", dict(vco1_wave="noise", **mix(10, 0), **hpf(7.5, 5), **lpf(9, 3), **eg2(0, 0, 1.2, 0, 1.2)), [], {}),
    ("Howling Wind", "FX", dict(vco1_wave="noise", **mix(10, 0), **hpf(3, 6), **lpf(4, 9, mg=5), mg_wave=5, mg_freq=1.5,
                                **eg2(0, 6, 5, 10, 6)), [("pink_out", "total_in")], {}),
    ("Random Bloops", "FX", dict(vco1_wave="tri", vco1_scale="8", **mix(10, 0), **hpf(2, 3), **lpf(5, 9, mg=4, eg=2),
                                 fm_mg=6, mg_wave=10, mg_freq=6.5, **eg2(0, 0, 2.5, 0, 2)),
     [("white_out", "sh_in"), ("mg_sq_out", "sh_clock_in"), ("sh_out", "total_in"), ("mg_sq_out", "kbd_trig_in")], {}),
    ("Laser Zap", "FX", dict(vco1_wave="pw", vco1_pw=4, vco1_scale="4", **mix(10, 0), **lpf(8, 7), fm_eg1=10,
                             eg1_attack=0, eg1_release=2.5, **eg2(0, 0, 3, 0, 3)),
     [("eg1_rev_out", "freq_in")], {}),
]

# Divisiones del MG en modo TEMPO (mg_sync = 1): FREQUENCY elige el índice.
DIV = {"4bars": 0, "2bars": 1, "1bar": 2, "1/2": 3, "1/4": 4, "1/4T": 5, "1/8": 6, "1/8T": 7, "1/16": 8, "1/16T": 9, "1/32": 10}
def sync(div): return dict(mg_freq=DIV[div])          # dial 0..10 = índice
DLY = {"1/32": 0, "1/16T": 1, "1/16": 2, "1/16D": 3, "1/8T": 4, "1/8": 5, "1/8D": 6, "1/4T": 7, "1/4": 8, "1/4D": 9, "1/2": 10, "1bar": 11}
def delay(t, fb=0.4, mix=0.25, tone=0.6): return dict(dly_on=1, dly_time=DLY[t], dly_fb=fb, dly_mix=mix, dly_tone=tone)
def reverb(size=0.7, mix=0.3, damp=0.4): return dict(rev_on=1, rev_size=size, rev_mix=mix, rev_damp=damp)
def poly(n, uni=1, detune=12, spread=0.6):
    return dict(voices={1: 0, 2: 1, 4: 2, 6: 3, 8: 4, 12: 5}[n], unison=uni - 1, detune=detune, spread=spread)
CLOCK = [("mg_sq_out", "kbd_trig_in"), ("mg_sq_out", "sh_clock_in")]   # el MG dispara las EG y el S/H

PRESETS += [
    # ── Self-Playing: suenan solos (MG en TEMPO dispara TRIG IN). Una tecla apretada transpone. ──
    ("Random Bleeps", "Self-Playing", dict(vco1_wave="pw", vco1_pw=2, vco1_scale="8", **mix(10, 0), **hpf(2, 3), **lpf(6, 6, eg=3),
                                           fm_mg=6, mg_wave=5, **sync("1/16"), **eg2(0, 0, 2.5, 0, 2)),
     [("white_out", "sh_in"), ("sh_out", "total_in")] + CLOCK, dict(mg_sync=1, **delay("1/8D", 0.35, 0.22))),
    ("Robot Talk", "Self-Playing", dict(vco1_wave="saw", vco1_scale="16", vco2_wave="square", vco2_scale="8", vco2_pitch=0.1,
                                        **mix(10, 7), **hpf(3, 6, eg=0), **lpf(4, 8, eg=0), mg_wave=5, **sync("1/16T"),
                                        **eg2(0, 0, 4, 2, 1.5)),
     [("pink_out", "sh_in"), ("sh_out", "lpf_cut_in"), ("sh_out", "hpf_cut_in")][:2] + CLOCK,
     dict(mg_sync=1, **reverb(0.4, 0.15))),
    ("Drip Cave", "Self-Playing", dict(**mix(0, 0), **hpf(0, 0), **lpf(5, 9.6, eg=2), mg_wave=5, **sync("1/8"),
                                       **eg2(0, 0, 3, 0, 3)),
     [("white_out", "sh_in"), ("sh_out", "lpf_cut_in")] + CLOCK, dict(mg_sync=1, **delay("1/4D", 0.55, 0.35, 0.4), **reverb(0.9, 0.45))),
    ("Morse Station", "Self-Playing", dict(vco1_wave="pw", vco1_pw=0, vco1_scale="4", **mix(7, 0), **hpf(4, 2), **lpf(7, 3),
                                           mg_wave=1.5, **sync("1/16"), **eg2(0, 0, 0, 10, 0.5)),
     [("mg_sq_out", "kbd_trig_in")], dict(mg_sync=1, **delay("1/8", 0.3, 0.2), **reverb(0.5, 0.2))),
    ("Chaos Arp", "Self-Playing", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="pulse", vco2_scale="8", vco2_pitch=0.08,
                                       **mix(10, 6), **hpf(2, 3), **lpf(4, 7, eg=5), fm_mg=8, mg_wave=5, **sync("1/8"),
                                       **eg2(0, 0, 3, 1, 2.5), portamento=1.5),
     [("pink_out", "sh_in"), ("sh_out", "total_in")] + CLOCK, dict(mg_sync=1, **delay("1/8D", 0.45, 0.3))),
    ("Pulse Train Bass", "Self-Playing", dict(vco1_wave="saw", vco1_scale="32", vco2_wave="square", vco2_scale="16",
                                              **mix(10, 6), **hpf(1, 2), **lpf(2.5, 7.5, eg=6), mg_wave=5, **sync("1/16"),
                                              eg1_delay=0, eg1_attack=6, eg1_release=6, **eg2(0, 0, 3, 0, 2)),
     [("mg_sq_out", "kbd_trig_in"), ("eg1_out", "lpf_cut_in")], dict(mg_sync=1)),
    ("Wind Chimes", "Self-Playing", dict(vco1_wave="pw", vco1_pw=3, vco1_scale="4", vco2_wave="ring", vco2_scale="2", vco2_pitch=2.2,
                                         **mix(2, 10), **hpf(4, 2), **lpf(8, 2), fm_mg=7, mg_wave=5, **sync("1/8T"),
                                         **eg2(0, 0, 6, 0, 7)),
     [("white_out", "sh_in"), ("sh_out", "total_in")] + CLOCK, dict(mg_sync=1, **delay("1/4T", 0.5, 0.3, 0.7), **reverb(0.92, 0.5))),
    ("Clockwork Toy", "Self-Playing", dict(vco1_wave="tri", vco1_scale="8", vco2_wave="pulse", vco2_scale="4", **mix(10, 4),
                                           **hpf(3, 4), **lpf(6, 5, eg=2), fm_eg1=6, eg1_attack=0, eg1_release=3,
                                           mg_wave=5, **sync("1/8"), **eg2(0, 0, 2, 0, 2)),
     [("mg_sq_out", "kbd_trig_in"), ("eg1_rev_out", "freq_in")], dict(mg_sync=1, **reverb(0.5, 0.2))),
    ("Bubbling Swamp", "Self-Playing", dict(vco1_wave="tri", vco1_scale="16", **mix(10, 0), **hpf(1, 0), **lpf(4, 9, eg=0),
                                            fm_mg=0, mg_wave=5, mg_freq=7.5, **eg2(0, 2, 5, 10, 4)),
     [("white_out", "sh_in"), ("mg_sq_out", "sh_clock_in"), ("sh_out", "lpf_cut_in"), ("mg_tri_out", "gain_in")],
     dict(**reverb(0.6, 0.3))),
    ("Endless Drift", "Self-Playing", dict(vco1_wave="tri", vco1_scale="8", vco2_wave="saw", vco2_scale="8", vco2_pitch=0.05,
                                           **mix(10, 5), **hpf(1, 0), **lpf(4.5, 4, eg=3), fm_mg=3.5, mg_wave=5, **sync("1bar"),
                                           **eg2(0, 6, 7, 6, 7.5), portamento=5),
     [("pink_out", "sh_in"), ("sh_out", "total_in")] + CLOCK,
     dict(mg_sync=1, **poly(1, 3, 18, 0.9), **delay("1/4D", 0.6, 0.35, 0.45), **reverb(0.95, 0.55))),
    ("Space Telemetry", "Self-Playing", dict(vco1_wave="square" if False else "pw", vco1_pw=4, vco1_scale="4", **mix(8, 0), **hpf(5, 6),
                                             **lpf(8, 4), fm_mg=4, mg_wave=2, **sync("1/32"), **eg2(0, 0, 1.2, 0, 1)),
     [("white_out", "sh_in"), ("sh_out", "total_in")] + CLOCK, dict(mg_sync=1, **delay("1/16D", 0.6, 0.3, 0.8), **reverb(0.7, 0.3))),

    # ── Ambient: pads polifónicos, ataques lentos, delay y reverb ──
    ("Glass Cathedral", "Ambient", dict(vco1_wave="tri", vco1_scale="8", vco2_wave="tri" if False else "pulse", vco2_scale="4",
                                        **mix(10, 3), **hpf(2, 2), **lpf(6, 3), fm_mg=0.6, mg_freq=3,
                                        **eg2(0, 4.5, 6, 7, 6.5)),
     [], dict(**poly(6, 1, 8, 0.7), **delay("1/4D", 0.45, 0.25, 0.5), **reverb(0.93, 0.5))),
    ("Analog Fog", "Ambient", dict(vco1_wave="saw", vco1_scale="16", vco2_wave="saw", vco2_scale="8", vco2_pitch=0.08,
                                   **mix(10, 7), **hpf(1.5, 0), **lpf(4, 3, mg=2, eg=2), mg_freq=1.5,
                                   **eg2(0, 5, 6, 8, 6.5)),
     [], dict(**poly(4, 2, 16, 0.85), **reverb(0.85, 0.42), analog=0.7)),
    ("Frozen Horizon", "Ambient", dict(vco1_wave="pw", vco1_pw=4.5, vco1_scale="8", vco2_wave="saw", vco2_scale="8",
                                       vco2_pitch=0.03, **mix(10, 6), **hpf(3, 4), **lpf(5, 4, mg=3, eg=1), mg_freq=1,
                                       **eg2(0, 6, 7, 8, 7.5)),
     [], dict(**poly(6, 1, 10, 0.8), **delay("1/2", 0.5, 0.25, 0.4), **reverb(0.96, 0.55))),
    ("Deep Ocean Drone", "Ambient", dict(vco1_wave="saw", vco1_scale="32", vco2_wave="saw", vco2_scale="16", vco2_pitch=0.04,
                                         **mix(10, 9), **hpf(0.5, 0), **lpf(2.5, 7, mg=4), mg_freq=0.6, mg_wave=5,
                                         **eg2(0, 6.5, 6, 10, 7)),
     [("pink_out", "ext_sig_in")], dict(**poly(2, 2, 22, 1.0), **reverb(0.9, 0.4))),
    ("Aurora", "Ambient", dict(vco1_wave="pw", vco1_pw=2, vco1_scale="8", vco2_wave="ring", vco2_scale="4", vco2_pitch=1.2,
                               **mix(10, 4), **hpf(2.5, 3), **lpf(6, 4, mg=2), fm_mg=0.8, mg_freq=2.5,
                               **eg2(0, 5.5, 7, 6, 7)),
     [], dict(**poly(6, 1, 10, 0.8), **delay("1/4D", 0.55, 0.3, 0.55), **reverb(0.95, 0.5))),
    ("Distant Choir", "Ambient", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="saw", vco2_scale="8", vco2_pitch=0.06,
                                      **mix(10, 8), **hpf(4, 6), **lpf(5, 5), fm_mg=1.2, mg_freq=4.2,
                                      **eg2(0, 5, 6, 9, 6)),
     [], dict(**poly(6, 2, 14, 0.9), **reverb(0.9, 0.48))),
    ("Tape Memories", "Ambient", dict(vco1_wave="pw", vco1_pw=0, vco1_scale="8", vco2_wave="saw", vco2_scale="8", vco2_pitch=0.1,
                                      **mix(8, 8), **hpf(2, 0), **lpf(4.5, 2, eg=2), fm_mg=1.0, mg_freq=2,
                                      **eg2(0, 3, 6, 7, 6)),
     [], dict(**poly(6, 1, 10, 0.6), analog=0.95, **delay("1/2", 0.62, 0.35, 0.25), **reverb(0.8, 0.35))),
    ("Breathing Strings", "Ambient", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="saw", vco2_scale="16", **mix(10, 7),
                                          **hpf(2, 2), **lpf(5, 3, mg=1.5, eg=2), mg_freq=1.2, eg1_delay=3, eg1_attack=5,
                                          fm_eg1=0, **eg2(0, 4.5, 6, 8, 6)),
     [("mg_tri_out", "vca_in"), ("vca_out", "total_in")], dict(**poly(4, 3, 20, 0.9), **reverb(0.85, 0.4))),
    ("Lunar Bells", "Ambient", dict(vco1_wave="tri", vco1_scale="4", vco2_wave="ring", vco2_scale="4", vco2_pitch=3.6,
                                    **mix(6, 10), **hpf(3, 2), **lpf(7, 2), **eg2(0, 0, 7, 0, 7.5)),
     [], dict(**poly(8, 1, 6, 0.7), **delay("1/8D", 0.5, 0.3, 0.6), **reverb(0.94, 0.5))),

    # ── Vocoder: necesitan voz por la entrada de audio (sidechain o AT20V FX) ──
    ("Classic Robot Voice", "Vocoder", dict(vco1_wave="saw", vco1_scale="16", vco2_wave="square", vco2_scale="8", vco2_pitch=0.05,
                                            **mix(10, 8), **hpf(0, 0), **lpf(9, 0), **eg2(0, 0, 0, 10, 2)),
     [], dict(**poly(6, 1, 8, 0.5), voc_on=1, voc_bands=2, voc_mix=1.0, **reverb(0.5, 0.15))),
    ("Vocoder Choir", "Vocoder", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="saw", vco2_scale="8", vco2_pitch=0.08,
                                      **mix(10, 10), **hpf(1, 0), **lpf(8, 0), fm_mg=0.5, mg_freq=4.5, **eg2(0, 2, 5, 9, 4)),
     [], dict(**poly(6, 2, 16, 0.9), voc_on=1, voc_bands=3, voc_q=0.7, voc_shift=0.15, voc_release=0.45, **reverb(0.85, 0.4))),
    ("Whisper Machine", "Vocoder", dict(vco1_wave="noise", **mix(10, 0), **hpf(2, 0), **lpf(10, 0), **eg2(0, 0, 0, 10, 2)),
     [], dict(voc_on=1, voc_bands=3, voc_hiss=0.8, voc_q=0.6, **reverb(0.6, 0.25))),
    ("Talk Box Lead", "Vocoder", dict(vco1_wave="saw", vco1_scale="8", vco2_wave="pulse", vco2_scale="8", vco2_pitch=0.1,
                                      **mix(10, 7), **hpf(1, 0), **lpf(9, 2), portamento=2.5, **eg2(0, 0, 0, 10, 1.5)),
     [], dict(voc_on=1, voc_bands=1, voc_q=0.8, voc_attack=0.1, voc_release=0.2, **delay("1/8", 0.3, 0.18))),
    # ── Efecto: el audio de la pista entra por la ESP a los filtros (EXT SIGNAL IN) y el disparador de la ESP abre
    # el VCA, así suena sin MIDI. Los VCO quedan en cero. AT20V FX arranca con "Filter FX".
    ("Filter FX", "Effect", dict(**mix(0, 0), **hpf(1, 2), **lpf(6, 4), **eg2(0, 0, 0, 10, 3), volume=8,
                                 esp_level=6, esp_lowcut=0, esp_highcut=10, esp_thresh=0),
     [("esp_amp_out", "ext_sig_in"), ("esp_trig_out", "kbd_trig_in")], {}),
    ("Auto-Wah FX", "Effect", dict(**mix(0, 0), **hpf(0, 0), **lpf(2.5, 7, eg=8), **eg2(0, 0, 0, 10, 3), volume=8,
                                   esp_level=6, esp_lowcut=0, esp_highcut=10, esp_thresh=0),
     [("esp_amp_out", "ext_sig_in"), ("esp_trig_out", "kbd_trig_in"), ("esp_env_out", "lpf_cut_in")], {}),
    ("Screaming Filter FX", "Effect", dict(**mix(0, 0), **hpf(3, 6, mg=2), **lpf(5, 8, mg=4), mg_wave=5, mg_freq=4,
                                           **eg2(0, 0, 0, 10, 3), volume=7, esp_level=6, esp_lowcut=0, esp_highcut=10, esp_thresh=0),
     [("esp_amp_out", "ext_sig_in"), ("esp_trig_out", "kbd_trig_in")], {}),
    ("Envelope Gate FX", "Effect", dict(**mix(0, 0), **hpf(0, 0), **lpf(8, 2, eg=4), **eg2(0, 0, 3, 4, 1), volume=8,
                                        esp_level=6, esp_lowcut=0, esp_highcut=10, esp_thresh=1),
     [("esp_amp_out", "ext_sig_in"), ("esp_trig_out", "kbd_trig_in")], {}),
]

def to_norm(k, v):
    if k == "vco1_wave": return W1[v]
    if k == "vco1_scale": return S1[v]
    if k == "vco2_wave": return W2[v]
    if k == "vco2_scale": return S2[v]
    if k in BIPOLAR: return round((v + 5) / 10, 4)
    return round(v / 10, 4)

CABLE_COLORS = ["red", "blue", "yellow", "green", "white", "black"]

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    index = []
    for i, (name, cat, knobs, cables, settings) in enumerate(PRESETS, 1):
        k = dict(NORMAL); k.update(knobs)
        data = dict(name=name, category=cat,
                    knobs={kk: to_norm(kk, vv) for kk, vv in k.items()},
                    cables=[dict(src=s, dst=d, color=CABLE_COLORS[j % len(CABLE_COLORS)]) for j, (s, d) in enumerate(cables)],
                    settings=settings)
        fn = f"{i:02d}_{name.lower().replace(' ', '_').replace('&', 'and')}.json"
        json.dump(data, open(os.path.join(OUT, fn), "w"), indent=1)
        index.append(fn)
    print(len(index), "presets")

if __name__ == "__main__":
    main()
