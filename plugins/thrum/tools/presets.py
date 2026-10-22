#!/usr/bin/env python3
"""Presets de fábrica de Thrum: escribe assets/presets/pNN_nombre.json.

    python3 plugins/thrum/tools/presets.py

Cada preset parte de los valores por defecto de los parámetros (Source/ParamsTh.h) y cambia sólo lo que dice acá, en las
unidades de cada parámetro (Gain 0-10, posición de púa en mm, mezclas en %). Las elecciones van por nombre. "pattern"
elige un patrón de la biblioteca (tools/patterns.py) por "Género: Nombre"; "strum"/"accents" escriben uno propio.
"demo_chords" y "demo_bpm" son la frase con que thrumrender --bench y las demos prueban el preset.
Sonidos originales: ningún nombre de producto, marca ni artista.
"""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "presets")

# ── Piezas reusables ──
S3, H2, SEMI, JAZZ, BARI, SEVEN = "Solid body, three single coils", "Solid body, two humbuckers", "Semi-hollow body, two humbuckers", "Hollow jazz body", "Baritone", "Seven-string"
BRIDGE, BM, MID, MN, NECK = "Bridge", "Bridge + middle", "Middle (both)", "Middle + neck", "Neck"
CLEAN, CRUNCH, HIGH, COMBO = "American clean", "British crunch", "Modern high gain", "Small combo"

def amp(model, gain, bass=5, mid=5, treble=5, presence=4, master=5, sag=5, depth=3, bright=False, level=0):
    return {"amp_model": model, "amp_gain": gain, "amp_bass": bass, "amp_mid": mid, "amp_treble": treble, "amp_presence": presence,
            "amp_master": master, "amp_sag": sag, "amp_depth": depth, "amp_bright": 1 if bright else 0, "amp_level": level}

def cab(speaker="British 12 (ceramic, mid bump)", box="4x12 closed back", mic_a="Dynamic", pos_a=30, dist_a=3, mic_b="Ribbon", pos_b=50, dist_b=25, mix=0, room=15):
    return {"cab_source": "Model", "cab_speaker": speaker, "cab_box": box, "cab_mic_a": mic_a, "cab_pos_a": pos_a, "cab_dist_a": dist_a,
            "cab_mic_b": mic_b, "cab_pos_b": pos_b, "cab_dist_b": dist_b, "cab_mix": mix, "cab_room": room}

CAB_412 = cab()
CAB_412_MOD = cab("Modern 12 (aggressive)", "4x12 closed back", "Dynamic", 25, 2, "Ribbon", 40, 5, 30, 10)
CAB_212 = cab("American 12 (clean, wide)", "2x12 open back", "Dynamic", 35, 4, "Condenser", 50, 40, 25, 30)
CAB_112 = cab("Vintage 12 (alnico)", "1x12 open back", "Dynamic", 30, 3, "Ribbon", 40, 12, 30, 20)
CAB_110 = cab("Small 10", "1x10 small combo", "Dynamic", 35, 3, "Condenser", 50, 45, 20, 25)

def chorus(rate=30, depth=50, mix=50, tone=60): return {"cho_on": 1, "cho_rate": rate, "cho_depth": depth, "cho_mix": mix, "cho_tone": tone}
def phaser(rate=25, depth=60, fb=40, mix=100): return {"pha_on": 1, "pha_rate": rate, "pha_depth": depth, "pha_fb": fb, "pha_mix": mix}
def flanger(rate=20, depth=65, fb=70, mix=70, manual=30): return {"fla_on": 1, "fla_rate": rate, "fla_depth": depth, "fla_fb": fb, "fla_mix": mix, "fla_manual": manual}
def trem(rate=45, depth=50, shape="Sine", sync=False): return {"trem_on": 1, "trem_rate": rate, "trem_depth": depth, "trem_shape": shape, "trem_sync": 1 if sync else 0}
def vibp(rate=40, depth=40): return {"vib_on": 1, "vib_rate": rate, "vib_depth": depth}
def delay(kind="Digital", div=None, time=45, fb=35, mix=25, tone=50, mod=20):
    d = {"dly_on": 1, "dly_type": kind, "dly_fb": fb, "dly_mix": mix, "dly_tone": tone, "dly_mod": mod}
    if div is not None: d.update({"dly_sync": 1, "dly_div": div})
    else: d.update({"dly_sync": 0, "dly_time": time})
    return d
# División del delay sincronizado: dly_div en % (1/32, 1/16T, 1/16, 1/8T, 1/16., 1/8, 1/4T, 1/8., 1/4, 1/4., 1/2)
DIV = {"1/16": 22, "1/8T": 31, "1/8": 50, "1/4T": 59, "1/8.": 68, "1/4": 77, "1/4.": 86, "1/2": 96}
def reverb(kind="Room", decay=50, tone=50, mix=25, pre=10, size=50, freeze=False):
    return {"rev_on": 1, "rev_type": kind, "rev_decay": decay, "rev_tone": tone, "rev_mix": mix, "rev_pre": pre, "rev_size": size, "rev_freeze": 1 if freeze else 0}
def comp(sus=50, att=30, lvl=50, mix=100): return {"comp_on": 1, "comp_sus": sus, "comp_att": att, "comp_lvl": lvl, "comp_mix": mix}
def od(drive=45, tone=50, lvl=60): return {"od_on": 1, "od_drive": drive, "od_tone": tone, "od_lvl": lvl}
def dist(drive=50, filt=40, lvl=60): return {"dist_on": 1, "dist_drive": drive, "dist_filter": filt, "dist_lvl": lvl}
def fuzz(kind="Silicon", amount=70, bias=70, tone=50, lvl=50): return {"fuzz_on": 1, "fuzz_type": kind, "fuzz_fuzz": amount, "fuzz_bias": bias, "fuzz_tone": tone, "fuzz_lvl": lvl}
def gate(thr=30, rel=30): return {"gate_on": 1, "gate_thr": thr, "gate_rel": rel}
def wah(mode="Auto", pos=50, sens=50, speed=50, rng=50, q=50): return {"wah_on": 1, "wah_mode": mode, "wah_pos": pos, "wah_sens": sens, "wah_speed": speed, "wah_range": rng, "wah_q": q}

def guitar(kind, sel, vol=10, tone=10, **kw):
    d = {"g_type": kind, "g_selector": sel, "g_volume": vol, "g_tone": tone}
    d.update(kw)
    return d

def melody(mono=False, legato="Auto", reach=4, vib=True, vib_depth=35, vib_delay=450, **kw):
    d = {"p_mode": "Melody (mono)" if mono else "Melody (poly)", "p_legato": legato, "p_legato_frets": reach,
         "p_vib_auto": 1 if vib else 0, "p_vib_depth": vib_depth, "p_vib_delay": vib_delay}
    d.update(kw)
    return d

def strum(pattern, style="Auto", region=0, speed=35, up=4, **kw):
    d = {"p_mode": "Strum pattern", "s_pattern": pattern, "c_style": style, "c_region": region, "c_strum": speed, "c_up": up}
    d.update(kw)
    return d

def chords(style="Auto", region=0, speed=35, **kw):
    d = {"p_mode": "Chords", "c_style": style, "c_region": region, "c_strum": speed}
    d.update(kw)
    return d

def picking(pattern, style="Auto", region=0, **kw):
    d = {"p_mode": "Fingerpicking", "a_pattern": pattern, "c_style": style, "c_region": region, "p_fingers": 1}
    d.update(kw)
    return d

def merge(*ds):
    out = {}
    for d in ds: out.update(d)
    return out

# ── Los presets ──
P = []
def add(cat, name, desc, params, chords_demo="C G Am F", bpm=100, chain=None, **extra):
    e = {"name": name, "category": cat, "description": desc, "params": params, "demo_chords": chords_demo, "demo_bpm": bpm}
    if chain: e["chain"] = chain
    e.update(extra)
    P.append(e)

PRE_AMP_MOD = "gate comp wah od dist fuzz chorus phaser flanger vib amp cab trem delay verb"     # modulación delante del amplificador

# Limpios: jazz, funk, pop.
add("Clean", "Glass Arpeggio", "Bridge and middle single coils into a clean amp, light chorus and a small room: picked chords that ring.",
    merge(guitar(S3, BM), amp(CLEAN, 4, treble=6, bright=True), CAB_212, chorus(25, 35, 30), reverb("Room", 40, 55, 18), picking("Ballad: Ballad arpeggio")), "Cmaj7 Am7 Fmaj7 G", 92)
add("Clean", "Warm Jazz Box", "Hollow body on the neck pickup with the tone rolled back, jazz voicings, a dark clean amp.",
    merge(guitar(JAZZ, NECK, tone=6.5), amp(CLEAN, 3.5, bass=6, mid=5, treble=3.5, presence=2), CAB_112, reverb("Room", 35, 35, 14), chords("Jazz", 5, 22), {"p_pick_hard": 35, "p_pick_pos": 120}),
    "Dm7 G7 Cmaj7 A7", 96)
add("Clean", "Funk Scratch", "Bridge and middle pickups, squeezed by the compressor, sixteenth scratches with ghost notes.",
    merge(guitar(S3, BM), amp(CLEAN, 4.5, mid=6, treble=6.5, bright=True), CAB_212, comp(60, 20, 55), strum("Funk: Scratch", "Triads", 7, 18, 3)), "E9 A9", 104)
add("Clean", "Sparkle Pop Strum", "Middle and neck single coils, compressed and chorused: bright pop strumming.",
    merge(guitar(S3, MN), amp(CLEAN, 4, treble=6, bright=True), CAB_212, comp(45, 35, 50), chorus(20, 30, 25), reverb("Room", 45, 55, 20), strum("Pop: Island")), "G D Em C", 98)
add("Clean", "Clean Picked Lead", "Neck single coil, clean amp, dotted-eighth echo: melodies with smart legato and a late vibrato.",
    merge(guitar(S3, NECK), amp(CLEAN, 4.5, treble=5.5), CAB_212, delay("Digital", DIV["1/8."], fb=30, mix=22), reverb("Room", 40, 50, 15), melody(True, "Auto", 4, True, 30, 500)), "A", 96)
add("Clean", "Fingerstyle Semi", "Semi-hollow on the neck pickup played with the fingers: alternating bass under the melody strings.",
    merge(guitar(SEMI, NECK, tone=8), amp(CLEAN, 3.5, bass=5.5, treble=4.5), CAB_112, reverb("Room", 40, 45, 16), picking("Folk: Travis", "Open", 0)), "C Am F G7", 92)
add("Clean", "Chicken Pickin'", "Bridge single coil with compression and a short tape slap: snappy country lines.",
    merge(guitar(S3, BRIDGE), amp(CLEAN, 5, mid=6, treble=6, bright=True), CAB_212, comp(55, 15, 55), delay("Tape", None, 18, 12, 22, 55, 25), melody(True, "Hammer-on / pull-off", 3, False), {"p_pick_hard": 80}), "E", 120)
add("Clean", "Bossa Night", "Hollow body, fingers, a bossa pattern with jazz voicings and a soft room.",
    merge(guitar(JAZZ, NECK, tone=7), amp(CLEAN, 3.5, bass=5.5, treble=4), CAB_112, reverb("Room", 35, 40, 14), strum("Latin: Bossa", "Jazz", 5, 20, 3), {"p_fingers": 1}), "Am7 D7 Gmaj7 Cmaj7", 120)

# Crunch y blues.
add("Crunch & Blues", "Small Combo Breakup", "A small class A combo at the edge of breakup: the attack crunches, soft picking cleans up.",
    merge(guitar(H2, NECK), amp(COMBO, 5.5, treble=6, sag=7), CAB_110, reverb("Spring", 45, 50, 18, 0, 50), chords("Open", 0, 30)), "A D E A", 100)
add("Crunch & Blues", "Blues Lead Bends", "Humbuckers on the neck into a crunch amp with a light boost: bends into the note and a late, wide vibrato.",
    merge(guitar(H2, NECK), amp(CRUNCH, 5, mid=6, treble=5), CAB_412, od(20, 55, 65), reverb("Room", 45, 45, 18), melody(True, "Auto", 4, True, 45, 350), {"p_art": "Bend in", "p_bendin": 2, "p_bendin_time": 200}), "A", 84)
add("Crunch & Blues", "Texas Shuffle", "Neck single coil, green overdrive into a small combo, shuffle strumming.",
    merge(guitar(S3, NECK), amp(COMBO, 4.5, treble=5.5, sag=6), CAB_110, od(55, 55, 60), reverb("Spring", 40, 50, 15), strum("Blues: Shuffle", "Barre", 5, 30, 3)), "E7 A7 E7 B7", 112)
add("Crunch & Blues", "Crunch Rhythm", "Bridge humbucker into the British amp at mid gain: rock rhythm with open chords.",
    merge(guitar(H2, BRIDGE), amp(CRUNCH, 6, mid=6, treble=6, presence=5), CAB_412, reverb("Room", 35, 50, 12), strum("Rock: Drive", "Open", 0, 28)), "G C D C", 120)
add("Crunch & Blues", "Edge of Breakup", "Semi-hollow into the combo: chords that bloom when you dig in.",
    merge(guitar(SEMI, BRIDGE), amp(COMBO, 6, treble=5.5, sag=7), CAB_110, reverb("Room", 45, 45, 18), chords("Barre", 5, 32)), "E A B A", 96)
add("Crunch & Blues", "Open G Slide", "Open G tuning, slides between the notes, a gritty combo.",
    merge(guitar(S3, BRIDGE, g_tuning="Open G"), amp(COMBO, 6.5, treble=6), CAB_110, reverb("Spring", 40, 50, 15), melody(True, "Slides", 7, True, 25, 300), {"p_slide_time": 140}), "G", 88)
add("Crunch & Blues", "Delta Fingerpick", "Open G, thumb on the bass and fingers on top, through a crunchy combo.",
    merge(guitar(SEMI, NECK, g_tuning="Open G"), amp(COMBO, 5.5), CAB_110, reverb("Room", 35, 40, 15), picking("Folk: Alternating bass", "Open", 0)), "G C G D", 96)
add("Crunch & Blues", "Green Drive Lead", "Green overdrive pushing a clean amp: smooth mids for singing leads.",
    merge(guitar(H2, NECK), amp(CLEAN, 6, mid=6), CAB_112, od(70, 55, 55), delay("Analog", DIV["1/4"], fb=30, mix=20), reverb("Room", 40, 45, 15), melody(True, "Auto", 4, True, 40, 400)), "D", 100)

# Rock clásico y alta ganancia.
add("Rock & High Gain", "Classic Stack Eighths", "Bridge humbucker into the British stack, power chords in eighth notes.",
    merge(guitar(H2, BRIDGE), amp(CRUNCH, 8, mid=6.5, treble=6, presence=5.5), CAB_412, strum("Rock: Eighths", "Power", 3, 20, 3)), "A5 G5 D5 E5", 132)
add("Rock & High Gain", "Arena Lead", "The stack boosted by the green overdrive, tape echo and a hall: long, singing notes.",
    merge(guitar(H2, NECK), amp(CRUNCH, 8.5, mid=7, treble=5.5), CAB_412, od(30, 50, 70), delay("Tape", DIV["1/4"], fb=35, mix=22, mod=30), reverb("Hall", 45, 45, 18), melody(True, "Auto", 5, True, 40, 300)), "E", 100)
add("Rock & High Gain", "Seventies Riff", "A cranked small combo with chords and power chords: loose and bright.",
    merge(guitar(SEMI, BRIDGE), amp(COMBO, 8, treble=6.5, sag=8), CAB_110, reverb("Room", 35, 50, 12), chords("Power", 0, 18)), "E5 G5 A5 G5", 120)
add("Rock & High Gain", "Modern Chug", "Seven-string, drop tuning, gate and high gain: palm-muted sixteenths.",
    merge(guitar(SEVEN, BRIDGE, g_tuning="Drop (lowest string down a tone)"), amp(HIGH, 6.5, bass=6, mid=4, treble=6, presence=6, depth=6), CAB_412_MOD, gate(45, 15),
          strum("Rock: Muted chug", "Power", 0, 10, 2), {"p_palm": 75}), "A5 C5 A5 F5", 140)
add("Rock & High Gain", "Baritone Doom", "Baritone with a silicon fuzz into the British amp: slow, heavy power chords.",
    merge(guitar(BARI, BRIDGE), amp(CRUNCH, 7, bass=7, mid=5, treble=5), CAB_412, fuzz("Silicon", 65, 80, 45, 45), reverb("Hall", 50, 40, 15), chords("Power", 0, 25)), "B5 D5 C5 A5", 70)
add("Rock & High Gain", "Metal Lead", "High gain with a gate, hammer-on legato and pinch harmonics on hard notes, echo and hall.",
    merge(guitar(H2, BRIDGE), amp(HIGH, 7, mid=6, treble=6, presence=6), CAB_412_MOD, gate(40, 20), delay("Digital", DIV["1/8."], fb=30, mix=18), reverb("Hall", 40, 45, 15),
          melody(True, "Hammer-on / pull-off", 5, True, 40, 300), {"p_pinch_above": 112}), "E", 120)
add("Rock & High Gain", "Thrash Palm Mute", "Downpicked palm-muted eighths on two humbuckers, tight high gain.",
    merge(guitar(H2, BRIDGE), amp(HIGH, 6, bass=6, mid=4.5, treble=6.5, presence=6), CAB_412_MOD, gate(50, 10), strum("Punk: Down eights", "Power", 0, 8, 2), {"p_palm": 80}), "E5 G5 A5 E5", 150)
add("Rock & High Gain", "Seven String Grid", "Seven-string riffing: power chords on the low B with chugs and stabs.",
    merge(guitar(SEVEN, BRIDGE), amp(HIGH, 7, bass=6.5, mid=4, treble=6, depth=7), CAB_412_MOD, gate(50, 12), strum("Rock: Anthem", "Power", 0, 10, 2)), "B5 G5 D5 A5", 110)
add("Rock & High Gain", "Stoner Fuzz", "Germanium fuzz into a small combo: woolly, sagging power chords.",
    merge(guitar(SEMI, BRIDGE), amp(COMBO, 6, bass=6, treble=5, sag=9), CAB_110, fuzz("Germanium", 75, 65, 40, 55), reverb("Room", 35, 40, 12), chords("Power", 0, 22)), "E5 G5 A5 C5", 80)
add("Rock & High Gain", "Pinch Squeal", "Every note a pinch harmonic: the pick and thumb at the node, high gain.",
    merge(guitar(H2, BRIDGE), amp(HIGH, 7.5, mid=6, treble=6), CAB_412_MOD, gate(40, 20), delay("Digital", DIV["1/4"], fb=25, mix=15), reverb("Hall", 40, 45, 15),
          melody(True, "Auto", 4, True, 45, 250), {"p_art": "Pinch harmonics", "p_pick_pos": 75}), "E", 100)

# Post-punk y dark wave (pensados para sonar junto a Pallor).
add("Post-punk & Dark wave", "Cold Chorus Arpeggio", "Neck single coil, deep chorus before a clean amp, dotted echo: picked minor chords in the cold.",
    merge(guitar(S3, NECK, tone=8), amp(CLEAN, 4.5, bass=4.5, treble=6), CAB_212, chorus(28, 70, 60, 55), delay("Digital", DIV["1/8."], fb=40, mix=28, tone=45), reverb("Hall", 50, 45, 22),
          picking("Dark wave: Night picking", "Triads", 7)), "Em C G D", 120, PRE_AMP_MOD)
add("Post-punk & Dark wave", "Night Drive Strum", "Bridge and middle pickups, chorus and echo, steady eighths: the dark wave rhythm guitar.",
    merge(guitar(S3, BM), amp(CLEAN, 5, mid=5, treble=6.5, bright=True), CAB_212, chorus(30, 60, 55), delay("Digital", DIV["1/8"], fb=25, mix=20), reverb("Room", 50, 50, 20),
          strum("Dark wave: Night drive", "Triads", 5, 22, 3)), "Am F C G", 128, PRE_AMP_MOD)
add("Post-punk & Dark wave", "Post-Punk Downstrokes", "Bridge single coil, light crunch and chorus: fast downstrokes on high triads.",
    merge(guitar(S3, BRIDGE), amp(CRUNCH, 4, mid=5, treble=6.5, presence=6), CAB_412, chorus(35, 45, 45), reverb("Room", 40, 55, 15), strum("Post-punk: Downstroke drive", "Triads", 7, 12, 3)),
    "Bm G D A", 140, PRE_AMP_MOD)
add("Post-punk & Dark wave", "Gothic Lead", "A flanged lead over a long tape echo and a hall.",
    merge(guitar(S3, NECK), amp(CRUNCH, 4.5, treble=6), CAB_412, flanger(15, 60, 55, 55, 30), delay("Tape", DIV["1/4."], fb=45, mix=28, mod=35), reverb("Hall", 60, 45, 25),
          melody(True, "Slides", 5, True, 30, 600)), "Em", 110, PRE_AMP_MOD)
add("Post-punk & Dark wave", "Ice Cathedral", "Cascading picked notes into a huge chorused hall.",
    merge(guitar(S3, MN), amp(CLEAN, 4, treble=6.5, bright=True), CAB_212, chorus(20, 55, 45), delay("Digital", DIV["1/4"], fb=45, mix=25), reverb("Hall", 80, 55, 40, 20, 80),
          picking("Post-rock: Cascade", "Triads", 7)), "Em Cmaj7 G D", 110)
add("Post-punk & Dark wave", "Wire Stabs", "Cut-off chord stabs on the off-beats through a jet flanger.",
    merge(guitar(S3, BRIDGE), amp(CRUNCH, 4.5, treble=6), CAB_412, flanger(25, 70, 75, 65, 25), reverb("Room", 30, 55, 12), strum("Post-punk: Cold stabs", "Triads", 5, 10, 3)),
    "Em D C D", 126, PRE_AMP_MOD)
add("Post-punk & Dark wave", "Baritone Shadows", "A baritone melody with chorus and slapback: the low twang of cold wave.",
    merge(guitar(BARI, NECK), amp(CLEAN, 5, bass=4.5, treble=6), CAB_212, chorus(25, 50, 45), delay("Tape", None, 22, 20, 25, 50, 25), reverb("Room", 50, 45, 20), melody(True, "Auto", 4, True, 25, 600)),
    "Em", 116, PRE_AMP_MOD)
add("Post-punk & Dark wave", "Coldwave Tremolo", "An optical tremolo synced to the tempo and a big room: pulsing chords.",
    merge(guitar(SEMI, NECK), amp(CLEAN, 4.5, treble=5.5), CAB_212, trem(50, 65, "Optical", True), reverb("Hall", 55, 45, 25), chords("Triads", 5, 25), {"c_letring": 1}), "Am Em F G", 118)

# Surf, shoegaze, ambient.
add("Surf, Shoegaze & Ambient", "Surf Spring Drip", "Bridge single coil, bright clean amp, a dripping spring tank, tremolo-picked melody.",
    merge(guitar(S3, BRIDGE), amp(CLEAN, 5, mid=4.5, treble=7, bright=True), CAB_212, reverb("Spring", 60, 60, 45, 0, 85), melody(True, "Auto", 4, False), {"p_art": "Tremolo picking", "p_trem_div": "1/16"}),
    "E", 160)
add("Surf, Shoegaze & Ambient", "Surf Rhythm", "Downpicked eighths drenched in spring.",
    merge(guitar(S3, BM), amp(CLEAN, 5, treble=6.5, bright=True), CAB_212, reverb("Spring", 55, 60, 40, 0, 75), strum("Rock: Eighths", "Barre", 3, 18, 3)), "E A B A", 156)
add("Surf, Shoegaze & Ambient", "Twang and Spring", "A clean twang with a sine tremolo and a short spring: country noir.",
    merge(guitar(S3, BRIDGE), amp(CLEAN, 5, treble=6.5), CAB_212, trem(40, 45, "Sine"), reverb("Spring", 45, 55, 30, 0, 60), melody(True, "Auto", 4, True, 30, 400)), "A", 96)
add("Surf, Shoegaze & Ambient", "Shoegaze Wall", "Silicon fuzz into the crunch amp, chorus and a long hall: a wall of strummed noise.",
    merge(guitar(H2, BRIDGE), amp(CRUNCH, 5, mid=4.5, treble=5), CAB_412, fuzz("Silicon", 75, 75, 55, 50), chorus(15, 55, 45), reverb("Hall", 80, 40, 45, 15, 85), strum("Shoegaze: Wall", "Open", 0, 25, 4)),
    "E C#m A B", 108)
add("Surf, Shoegaze & Ambient", "Haze Melody", "Vibrato pedal into a fuzz, long reverb: a warped, dreamy lead.",
    merge(guitar(SEMI, NECK), amp(CLEAN, 5.5), CAB_112, vibp(30, 45), fuzz("Germanium", 45, 80, 45, 50), reverb("Hall", 75, 40, 45, 10, 80), melody(True, "Slides", 5, True, 30, 600)), "D", 84, PRE_AMP_MOD)
add("Surf, Shoegaze & Ambient", "Ambient Freeze", "Fingers on a clean semi, long echoes and a hall you can freeze (Reverb freeze holds the chord).",
    merge(guitar(SEMI, NECK, tone=7.5), amp(CLEAN, 3.5, treble=5), CAB_212, delay("Tape", DIV["1/4."], fb=55, mix=35, mod=40), reverb("Hall", 90, 45, 55, 20, 90), picking("Ballad: Pinch and roll", "Open", 0)),
    "Cmaj7 Em7 Fmaj7 G", 72)
add("Surf, Shoegaze & Ambient", "Swell Pad", "Soft finger plucks, slow compression and a long hall: pad-like chords.",
    merge(guitar(SEMI, NECK, tone=6), amp(CLEAN, 3.5, treble=4.5), CAB_212, comp(70, 90, 50, 100), chorus(15, 40, 35), delay("Digital", DIV["1/2"], fb=50, mix=30), reverb("Hall", 90, 40, 60, 25, 90),
          chords("Open", 0, 60), {"p_fingers": 1, "c_letring": 1}), "Cmaj7 Am7 Fmaj7 G", 70)
add("Surf, Shoegaze & Ambient", "Feedback Drone", "Held notes bloom into the octave by controlled feedback, like an amp on the edge.",
    merge(guitar(H2, NECK), amp(CRUNCH, 7.5, treble=5.5), CAB_412, reverb("Hall", 70, 40, 35), melody(True, "Auto", 4, False), {"p_feedback": 75, "p_fb_harm": "Octave"}), "E", 70)
add("Surf, Shoegaze & Ambient", "Endless Sustain", "Clean-ish sustain that never dies: feedback on the fundamental, a soft room.",
    merge(guitar(S3, NECK), amp(COMBO, 4.5, treble=5), CAB_110, comp(55, 40, 55), reverb("Room", 55, 45, 25), melody(True, "Slides", 5, True, 25, 700), {"p_feedback": 60, "p_fb_harm": "Fundamental"}), "A", 76)

# Acordes con rasgueo.
add("Strummed Chords", "Campfire Strum", "Semi-hollow neck pickup, clean, folk strumming with the bass first.",
    merge(guitar(SEMI, NECK, tone=8), amp(CLEAN, 3.5, bass=5, treble=5.5), CAB_212, reverb("Room", 40, 50, 15), strum("Folk: Folk strum", "Open", 0, 32)), "G C D G", 100)
add("Strummed Chords", "Pop Punk Eighths", "Two humbuckers, crunch, steady eighths.",
    merge(guitar(H2, BRIDGE), amp(CRUNCH, 6.5, mid=5.5, treble=6), CAB_412, strum("Punk: Down eights", "Power", 0, 12, 3), {"p_palm": 40}), "C G Am F", 168)
add("Strummed Chords", "Reggae Skank", "Bright clean chops on two and four, cut short.",
    merge(guitar(S3, MN), amp(CLEAN, 4.5, bass=3.5, treble=7, bright=True), CAB_212, reverb("Spring", 35, 55, 15, 0, 40), strum("Reggae: One drop", "Triads", 5, 10, 3)), "Am Dm Em Am", 76)
add("Strummed Chords", "Funk Ghost Notes", "Muted sixteenths with accented chord hits, auto wah.",
    merge(guitar(S3, BM), amp(CLEAN, 4.5, mid=6, treble=6.5), CAB_212, comp(55, 15, 55), wah("Auto", 50, 55, 60, 55, 55), strum("Funk: Ghost sixteenths", "Triads", 7, 12, 3)), "E9 E9 A9 E9", 100)
add("Strummed Chords", "Boom Chuck", "Bass note then the chord: country train beat on a bright clean amp.",
    merge(guitar(S3, BRIDGE), amp(CLEAN, 4.5, treble=6.5, bright=True), CAB_212, delay("Tape", None, 20, 10, 20, 50, 20), strum("Country: Train", "Open", 0, 22)), "G C D G", 112)
add("Strummed Chords", "Waltz Strum", "Bass and two strums in three: a waltz on a clean semi.",
    merge(guitar(SEMI, NECK), amp(CLEAN, 3.5), CAB_212, reverb("Room", 40, 45, 18), strum("Other: Waltz", "Open", 0, 30)), "C G7 G7 C", 96)
add("Strummed Chords", "Six-Eight Ballad", "Triplet strumming for slow ballads.",
    merge(guitar(SEMI, NECK, tone=8), amp(CLEAN, 4), CAB_212, chorus(15, 30, 25), reverb("Hall", 55, 45, 25), strum("Ballad: Six-eight", "Open", 0, 40)), "C Am F G", 70)
add("Strummed Chords", "Anthem Strum", "Big open chords on the crunch amp, sixteenth anthem pattern.",
    merge(guitar(H2, NECK), amp(CRUNCH, 6, mid=6, treble=5.5), CAB_412, reverb("Hall", 45, 45, 20), strum("Rock: Anthem", "Open", 0, 30)), "D A Bm G", 92)
add("Strummed Chords", "Half Time Pop", "Half-time pop strumming with chorus.",
    merge(guitar(S3, MN), amp(CLEAN, 4.5, treble=6), CAB_212, chorus(20, 30, 25), reverb("Room", 45, 50, 18), strum("Pop: Half time")), "F C G Am", 84)

# Punteos con ligados y bends.
add("Leads", "Legato Runs", "High gain, hammer-ons and pull-offs over five frets: fast runs without picking.",
    merge(guitar(H2, NECK), amp(HIGH, 6, mid=6.5, treble=5.5), CAB_412_MOD, gate(35, 25), delay("Digital", DIV["1/8."], fb=30, mix=20), reverb("Hall", 40, 45, 15), melody(True, "Hammer-on / pull-off", 5, True, 35, 300)),
    "A", 120)
add("Leads", "Bend Into Note", "Every note bends up a whole step into pitch, crunch amp and room.",
    merge(guitar(S3, NECK), amp(CRUNCH, 6.5, mid=6, treble=5.5), CAB_412, reverb("Room", 45, 45, 18), melody(True, "Auto", 4, True, 45, 300), {"p_art": "Bend in", "p_bendin": 2, "p_bendin_time": 160}), "A", 90)
add("Leads", "Slide Lead", "Legato slides between notes and a slide into each phrase.",
    merge(guitar(S3, BRIDGE), amp(COMBO, 6.5, treble=6), CAB_110, delay("Analog", DIV["1/4"], fb=25, mix=18), reverb("Spring", 45, 50, 18), melody(True, "Slides", 7, True, 30, 400), {"p_slide_time": 110}), "E", 92)
add("Leads", "Fusion Lead", "Compressed, overdriven humbucker with a touch of chorus and echo: smooth legato.",
    merge(guitar(H2, NECK), amp(CLEAN, 6, mid=6.5), CAB_112, comp(45, 40, 55), od(55, 45, 55), chorus(20, 25, 20), delay("Digital", DIV["1/8."], fb=35, mix=22), reverb("Hall", 45, 45, 18),
          melody(True, "Auto", 5, True, 30, 450)), "D", 108)
add("Leads", "Trill Lead", "Mono legato: hold one key and tap another for trills; release returns to the held note.",
    merge(guitar(H2, BRIDGE), amp(CRUNCH, 7.5, mid=6.5), CAB_412, delay("Tape", DIV["1/8"], fb=25, mix=15), reverb("Room", 40, 45, 15), melody(True, "Hammer-on / pull-off", 5, True, 40, 300)), "E", 110)
add("Leads", "Whammy Dives", "High gain lead with a floating bridge: CC4 dives, the arm flutters when released.",
    merge(guitar(S3, BRIDGE), amp(HIGH, 6.5, mid=6, treble=6), CAB_412_MOD, gate(35, 25), delay("Digital", DIV["1/4"], fb=30, mix=18), reverb("Hall", 45, 45, 18), melody(True, "Auto", 4, True, 35, 350), {"p_whammy": 12}),
    "E", 110)
add("Leads", "Harmonics Bells", "Natural and artificial harmonics on a clean, chorused tone.",
    merge(guitar(S3, MN), amp(CLEAN, 4.5, treble=6.5, bright=True), CAB_212, chorus(20, 40, 35), delay("Digital", DIV["1/4."], fb=40, mix=28), reverb("Hall", 60, 55, 30), melody(False, "Off", 4, False), {"p_art": "Harmonics"}),
    "E", 90)
add("Leads", "Tremolo Picking Lead", "Fast alternate picking on every note, crunch and spring.",
    merge(guitar(S3, BRIDGE), amp(CRUNCH, 5.5, treble=6.5), CAB_412, reverb("Spring", 50, 55, 30, 0, 70), melody(True, "Auto", 4, False), {"p_art": "Tremolo picking", "p_trem_div": "1/16"}), "A", 140)

# Más colores.
add("Effects", "Auto Wah Funk", "Envelope wah on muted sixteenths: the pick opens the filter.",
    merge(guitar(S3, BM), amp(CLEAN, 5, mid=6, treble=6), CAB_212, comp(50, 15, 55), wah("Auto", 50, 65, 65, 60, 65), strum("Funk: Chop", "Triads", 7, 12, 3)), "E9 A9", 104)
add("Effects", "Wah Lead (CC 11)", "A crunch lead with the wah on the expression pedal (CC 11).",
    merge(guitar(H2, NECK), amp(CRUNCH, 7, mid=6), CAB_412, wah("MIDI CC 11", 50, 50, 50, 55, 60), delay("Analog", DIV["1/4"], fb=25, mix=18), reverb("Room", 40, 45, 15), melody(True, "Auto", 4, True, 40, 300)),
    "E", 96)
add("Effects", "Seventies Phaser", "Slow phaser on crunchy chords.",
    merge(guitar(S3, NECK), amp(CRUNCH, 5.5, treble=5.5), CAB_412, phaser(20, 65, 45, 100), reverb("Room", 40, 45, 15), strum("Rock: Drive", "Open", 0, 30), ), "D C G D", 96, PRE_AMP_MOD)
add("Effects", "Jet Flanger Chords", "Through-zero-ish jet flanger sweeps over held chords.",
    merge(guitar(H2, BRIDGE), amp(CRUNCH, 6.5, treble=6), CAB_412, flanger(10, 80, 80, 70, 15), reverb("Room", 40, 45, 15), chords("Barre", 5, 30)), "E A D A", 90)
add("Effects", "Tape Echo Twang", "Slapback and multi-head tape echo on a twangy bridge pickup.",
    merge(guitar(S3, BRIDGE), amp(CLEAN, 5, treble=6.5, bright=True), CAB_212, delay("Tape", DIV["1/8."], fb=45, mix=30, tone=45, mod=45), reverb("Spring", 40, 55, 20, 0, 60), melody(True, "Auto", 4, True, 25, 400)),
    "A", 100)
add("Effects", "Harmonic Tremolo Chords", "Lows and highs pulsing against each other.",
    merge(guitar(SEMI, NECK), amp(CLEAN, 4.5), CAB_212, trem(45, 60, "Harmonic", True), reverb("Room", 45, 45, 20), chords("Open", 0, 30), {"c_letring": 1}), "A D A E", 92)

# Directo (para reamplificar).
add("Direct", "DI Single Coils", "The guitar alone, no amp or cabinet: record this and reamp it later (the DI output carries the same).",
    merge(guitar(S3, BRIDGE), {"amp_on": 0, "cab_on": 0}, melody(False, "Auto", 4, True)), "A", 100)
add("Direct", "DI Humbuckers", "Two humbuckers straight to the desk.",
    merge(guitar(H2, NECK), {"amp_on": 0, "cab_on": 0}, chords("Open", 0, 30)), "G C D G", 100)
add("Direct", "DI Baritone", "The baritone straight to the desk.",
    merge(guitar(BARI, BRIDGE), {"amp_on": 0, "cab_on": 0}, melody(True, "Auto", 4, True)), "E", 100)

def slug(s): return re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")

def main():
    os.makedirs(OUT, exist_ok=True)
    levels_path = os.path.join(HERE, "preset_levels.json")
    levels = json.load(open(levels_path)) if os.path.exists(levels_path) else {}
    for p in P:
        if p["name"] in levels: p["params"]["out_level"] = levels[p["name"]]
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    names = set()
    for i, p in enumerate(P):
        assert p["name"] not in names, p["name"]; names.add(p["name"])
        data = dict(p); data["order"] = i
        with open(os.path.join(OUT, f"p{i:02d}_{slug(p['name'])}.json"), "w") as f:
            json.dump(data, f, indent=1, ensure_ascii=False)
            f.write("\n")
    cats = {}
    for p in P: cats[p["category"]] = cats.get(p["category"], 0) + 1
    print(f"{len(P)} presets: " + ", ".join(f"{k} {v}" for k, v in cats.items()))

if __name__ == "__main__":
    main()
