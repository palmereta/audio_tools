#!/usr/bin/env python3
"""Presets de fábrica del AT330: escribe assets/presets/pNN_nombre.json.

Cada preset guarda sólo lo que difiere de los valores por defecto (Source/Params330.h), en las unidades del
parámetro (segundos, %, dB, semitonos; las llaves en 0/1). Además trae, para las pruebas y las demos:
  - "demo_chords": la progresión con la que se escucha (at330render --chords);
  - "voice": true si necesita una voz en la entrada (vocoder).
El volumen de cada uno lo ajusta tools/headroom.py (tools/preset_levels.json) para que ninguno pase de -1 dBFS.
Ningún nombre cita marcas, películas, artistas ni temas.

    python3 plugins/at330/tools/presets.py
"""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "presets")
LEVELS = os.path.join(HERE, "preset_levels.json")

P = []
# Las llaves por defecto: cuerdas arriba y abajo prendidas, todo lo demás apagado.
STR_OFF = dict(str_upper=0, str_lower=0)
CHOIR_ALL = dict(ch_female_up=1, ch_male8_up=1, ch_male4_lo=1, ch_male8_lo=1)
VOC = dict(voc_upper=1, voc_lower=1)


def preset(cat, name, desc, chords="Am F C G", voice=False, **params):
    for k in params:
        assert re.fullmatch(r"[a-z0-9_]+", k), (name, k)
    if voice: name += " (Audio In)"           # como en el AT20V: sin voz por la entrada no suena
    P.append({"name": name, "category": cat, "description": desc, "order": len(P), "params": params,
              "demo_chords": chords, "voice": voice})


# ── Cuerdas: la sección de 4' con su ensemble ──
S = "Strings"
preset(S, "Ensemble Strings", "The string section as it leaves the factory: both halves at 4', medium attack, the ensemble always on.")
preset(S, "Slow Swell Strings", "A long attack and a long release: chords that rise and hang.", "Dm Bb F C", str_attack=2.2, release=4.5)
preset(S, "Dark Strings", "Tone almost closed: a soft, muffled section for low pads.", "Em C G D", str_tone=12, release=2.5)
preset(S, "Bright Strings", "Tone open: the full buzz of the divider sawtooth through the ensemble.", "C Am F G", str_tone=95, str_attack=0.08)
preset(S, "Short Strings", "Fast attack and short release for stabs and rhythmic chords.", "Am Am F G", str_attack=0.01, release=0.18, str_tone=70)
preset(S, "Vintage Strings", "The original's single envelope (Classic): each new chord dips and attacks again, the way the machine plays.", "Am F C G",
       keying=0, str_attack=0.15, release=1.4)
preset(S, "Dry Strings", "The ensemble switched off (extra): the bare 4' sawtooth, to hear what the ensemble adds.", "Am F C G", str_ens=0, str_tone=60)
preset(S, "Wide Strings", "Ensemble depth pushed past the original for a wider, more seasick chorus.", "Dm Am Bb F", ens_depth=150, release=2.0)
preset(S, "Upper Strings Only", "Strings only above the split: the lower half is free for a bass from another instrument.", "Am F C G", str_lower=0, release=1.6)

# ── Coro: las tres voces y el banco de formantes ──
C = "Choir"
preset(C, "Male Choir 8'", "Male voices at 8' over the whole keyboard, vibrato after a short delay.", "Am F C G",
       **STR_OFF, ch_male8_up=1, ch_male8_lo=1, ch_attack=0.2, release=1.8)
preset(C, "Female Choir", "Female 4' on the upper half, male 4' below: the classic split choir.", "C Em F G",
       **STR_OFF, ch_female_up=1, ch_male4_lo=1, ch_attack=0.25, release=2.0)
preset(C, "Full Choir", "All four choir tablets on: male 8' and female 4' above, male 4' and 8' below.", "Dm Bb F C",
       **STR_OFF, **CHOIR_ALL, ch_attack=0.3, release=2.4)
preset(C, "Low Monks", "Male 8' alone, slow vibrato, long release: a low drone of voices.", "Em Em C D",
       **STR_OFF, ch_male8_up=1, ch_male8_lo=1, vib_rate=4.8, vib_depth=20, release=3.5, ch_attack=0.5)
preset(C, "Cathedral Voices", "The full choir in a large space (extra reverb).", "Am Dm Em Am",
       **STR_OFF, **CHOIR_ALL, ch_attack=0.4, release=3.5, space_mix=35, space_size=85)
preset(C, "Choir Vibrato", "Deeper, faster vibrato through the BBD, no delay before it starts.", "F C Dm Bb",
       **STR_OFF, ch_female_up=1, ch_male8_up=1, ch_male4_lo=1, vib_depth=65, vib_rate=7.5, vib_delay=0)
preset(C, "Distant Voices", "Choir with the attack stretched and the space wet: voices far away.", "Am Em F C",
       **STR_OFF, ch_female_up=1, ch_male4_lo=1, ch_attack=1.2, release=5.0, space_mix=50, space_size=90, ch_level=70)
preset(C, "Dry Choir", "Choir with its ensemble tablet off: the formant filters on their own.", "Am F C G",
       **STR_OFF, **CHOIR_ALL, ch_ens=0, vib_depth=15)

# ── Capas: cuerdas y coro juntos ──
L = "Layers"
preset(L, "Strings and Choir", "Strings and the full choir together, the sound most people know this machine for.", "Am F C G",
       **CHOIR_ALL, str_level=70, ch_level=75, release=2.5, ch_attack=0.3)
preset(L, "Rain on Neon", "Slow strings with male and female voices, long release, a wet hall: slow, wide minor chords.", "Fm Db Ab Eb",
       ch_female_up=1, ch_male8_up=1, ch_male8_lo=1, str_attack=1.4, ch_attack=0.8, release=5.5, space_mix=40, space_size=90, str_tone=45)
preset(L, "Night Shift Pad", "Dark strings under a quiet male choir: a bed for dark wave and synthwave.", "Em C Am B",
       ch_male8_up=1, ch_male8_lo=1, str_tone=30, ch_level=55, release=2.8, str_attack=0.6)
preset(L, "Glass Hall", "Bright strings and the female voice, each note swelling on its own.", "Cmaj7 Am7 Fmaj7 G",
       ch_female_up=1, str_tone=85, str_attack=0.9, ch_attack=0.9, release=3.0, space_mix=30)
preset(L, "Cold Horizon", "Wide ensemble, all voices, slow attack: an endless horizon of sound.", "Dm Am Bb C",
       **CHOIR_ALL, ens_depth=140, str_attack=2.0, ch_attack=1.0, release=6.0, str_tone=40)
preset(L, "Split Choir over Strings", "Strings below the split, choir above: play the left hand as an orchestra and the right as a choir.", "Am F C G",
       str_upper=0, ch_female_up=1, ch_male8_up=1, release=2.0)
preset(L, "Frozen Cathedral", "Everything on with the longest release and a huge space.", "Em Cmaj7 Am D",
       **CHOIR_ALL, release=8.0, str_attack=1.5, ch_attack=1.0, space_mix=55, space_size=100, ens_depth=120)
preset(L, "Velvet Poly", "Velocity (extra): soft notes stay quiet and every note keeps its own attack.", "Dm7 G7 Cmaj7 Am7",
       ch_male8_up=1, ch_male4_lo=1, velocity=70, str_attack=0.2, ch_attack=0.25, release=1.5)

# ── Vocoder: la voz por la entrada (sidechain) ──
V = "Vocoder"
preset(V, "Vocoder Ensemble", "Both vocoder tablets with the ensemble: the classic, wide talking chord.", "Am F C G", True,
       **STR_OFF, **VOC, release=0.6)
preset(V, "Vocoder Dry", "The ten bands without ensemble or vibrato: the plain vocoder.", "Am F C G", True,
       **STR_OFF, **VOC, voc_ens=0, vib_depth=0, release=0.4)
preset(V, "Machine Voice", "Fast response, bright tone, no ensemble: a hard, robotic voice.", "Em Em G D", True,
       **STR_OFF, **VOC, voc_ens=0, vib_depth=0, voc_response=8, voc_tone=90, consonants=70, release=0.15)
preset(V, "Vocoder with Choir", "The vocoder over the choir: the voice speaks, the choir holds the chord.", "Dm Bb F C", True,
       str_upper=0, str_lower=0, **VOC, ch_male8_up=1, ch_male8_lo=1, ch_level=45, release=1.5)
preset(V, "Vocoder with Strings", "Vocoder on top of the strings, a little direct voice mixed in.", "Am F C G", True,
       **VOC, str_level=55, direct_mic=12, release=1.2)
preset(V, "Whisper Bands", "Consonants turned up and the tone open: every s and t comes through.", "Cm Ab Eb Bb", True,
       **STR_OFF, **VOC, consonants=100, voc_tone=85, release=0.5)
preset(V, "Hold Pad", "Vocoder Hold on: each band holds its level for seconds after the voice stops.", "Am Em F C", True,
       **STR_OFF, **VOC, hold=1, release=3.0, space_mix=25)
preset(V, "Formant Freeze", "Freeze (extra): the vowel caught by the bands stays while you play the chords.", "Dm Am Bb F", True,
       **STR_OFF, **VOC, hold=2, release=2.0, vib_depth=20)
preset(V, "Deep Formants", "Formants shifted down (extra): the voice sounds bigger and lower than the singer.", "Em C G D", True,
       **STR_OFF, **VOC, formant_shift=-5, voc_tone=50, release=0.8)
preset(V, "Small Formants", "Formants shifted up (extra): a small, high voice on the same notes.", "C G Am F", True,
       **STR_OFF, **VOC, formant_shift=6, voc_tone=80, release=0.6)
preset(V, "Slow Response", "The band detectors slowed down (extra): a smeared, ghostly voice.", "Fm Db Ab Eb", True,
       **STR_OFF, **VOC, voc_response=220, release=1.8, space_mix=30)
preset(V, "External Carrier", "The carrier comes from the right input channel (extra), the voice from the left.", "Am F C G", True,
       **STR_OFF, carrier=1, voc_ens=1, release=0.5)

# ── Pitch Shift y movimiento ──
M = "Pitch and Motion"
preset(M, "Rising Choir", "Pitch Shift in Auto: every chord starts a fifth below and glides up.", "Am F C G",
       **STR_OFF, **CHOIR_ALL, pitch_mode=1, pitch_set=7, pitch_time=0.35, release=2.0)
preset(M, "Siren Strings", "Auto Pitch Shift a full octave below with a long glide.", "Em Em C D",
       pitch_mode=1, pitch_set=12, pitch_time=1.2, release=1.5, str_tone=70)
preset(M, "Scoop Voices", "A short scoop up into each note, like a singer reaching for it.", "Dm Bb F C",
       **STR_OFF, ch_female_up=1, ch_male4_lo=1, pitch_mode=1, pitch_set=2, pitch_time=0.08, release=1.6)
preset(M, "Sinking Pad", "Manual Pitch Shift half way down: the whole pad detuned below the keys (move Pitch Shift to bend it).", "Am F C G",
       **CHOIR_ALL, pitch_mode=2, pitch_amount=50, pitch_set=1, release=2.5)
preset(M, "Wobbling Tape", "Deep, slow vibrato on choir and vocoder with a wide ensemble: an old tape wobbling.", "Cm Ab Eb Bb",
       **STR_OFF, ch_female_up=1, ch_male8_up=1, ch_male4_lo=1, vib_rate=4.5, vib_depth=100, vib_delay=0, ens_depth=150, release=2.0)
preset(M, "Classic Retrigger", "The original's single envelope with a slow attack: every new key re-attacks the whole chord.", "Am F C G",
       **CHOIR_ALL, keying=0, str_attack=0.6, ch_attack=0.6, release=2.0)
preset(M, "Poly Swells", "Poly envelopes (extra, the default) with a slow attack: each note swells on its own, unlike Classic Retrigger.", "Am F C G",
       **CHOIR_ALL, str_attack=0.6, ch_attack=0.6, release=2.0)
preset(M, "Bend Choir", "Pitch bend up and down a full fifth (extra) over a full choir; the mod wheel adds vibrato.", "Am F C G",
       **STR_OFF, **CHOIR_ALL, bend_range=7, mod_vib=100, vib_depth=10)

assert len(P) >= 40, len(P)


def main():
    levels = json.load(open(LEVELS)) if os.path.exists(LEVELS) else {}
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, p in enumerate(P):
        if p["name"] in levels: p["params"]["volume"] = levels[p["name"]]
        slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
        json.dump(p, open(os.path.join(OUT, f"p{i:02d}_{slug}.json"), "w"), indent=1)
    print(f"{len(P)} presets en {os.path.relpath(OUT)}")


if __name__ == "__main__":
    main()
