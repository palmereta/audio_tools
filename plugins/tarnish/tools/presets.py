#!/usr/bin/env python3
"""Presets de fábrica de Tarnish (docs/research.md §7): escribe assets/presets/pNN_nombre.json.

El ruido se escribe en dB respecto del nivel de referencia, como en la investigación, y se convierte a la perilla
Noise (50 % = el ruido propio de la máquina, ±20 dB en los extremos). La copia (print-through) en dB se convierte a
su perilla (0 = apagada, 1-100 = −70 a −40 dB).

    python3 plugins/tarnish/tools/presets.py
"""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "presets")

# Ruido propio de cada máquina y variante (Source/dsp/Machines.h).
HISS = {("studio", 0): -72, ("studio", 1): -68, ("studio", 2): -63, ("studio", 3): -58,
        ("cassette", 0): -52, ("cassette", 1): -56, ("cassette", 2): -57, ("cassette", 3): -46,
        ("video", 0): -44, ("video", 1): -40, ("vinyl", 0): -60, ("vinyl", 1): -60, ("vinyl", 2): -38,
        ("radio", 0): -50, ("radio", 1): -50, ("phone", 0): -50, ("phone", 1): -56}
MACHINES = ["studio", "cassette", "video", "vinyl", "radio", "phone"]
VARIANT_ID = {"studio": "speed", "cassette": "formulation", "video": "video", "vinyl": "rpm", "radio": "band", "phone": "line"}

def preset(cat, name, desc, machine, variant=0, noise_db=None, print_db=None, **kw):
    p = {"machine": MACHINES.index(machine), VARIANT_ID[machine]: variant}
    if noise_db is not None:
        p["noise"] = max(0.0, min(100.0, round(50 + (noise_db - HISS[(machine, variant)]) * 2.5, 1)))
    if print_db is not None:
        p["print"] = max(1.0, min(100.0, round((print_db + 70) / 0.3, 1)))
    p.update(kw)
    return {"name": name, "category": cat, "description": desc, "params": p}

S, C, V, VI, R, L = "Studio tape", "Cassette", "Video tape", "Vinyl", "Radio and phone", "Lo-fi and effects"
PRESETS = [
    preset(S, "Warm Studio Tape", "15 ips, a little drive: the everyday glue of tape on a mix or a bus.", "studio", 1, noise_db=-70,
           drive=4, saturation=55, wow=0.03, flutter=0.02, scrape=15),
    preset(S, "30 ips Master", "The fast, clean machine for a mix bus: open top, low bump, almost no hiss.", "studio", 0, noise_db=-72,
           drive=2, bias=20, wow=0.02, flutter=0.015, scrape=10),
    preset(S, "7.5 ips Demo Reel", "Half the speed: the bump moves down, the top rolls off, the hiss rises.", "studio", 2, noise_db=-63,
           drive=6, saturation=60, wow=0.08, flutter=0.05, scrape=25, eq=1),
    preset(S, "Hot Drum Bus", "Driven hard with a little grit; auto gain keeps the level, the mix keeps the punch.", "studio", 1, noise_db=-68,
           drive=12, saturation=75, bias=-20, wow=0.03, flutter=0.02, mix=70),
    preset(S, "Bounced Four Times", "Generation loss in one click: worn, a tilted head and a faint echo of the layer next door.", "studio", 2, noise_db=-58, print_db=-55,
           drive=5, wear=25, azimuth=20, wow=0.1, flutter=0.06, scrape=30),
    preset(S, "Under-Biased Grit", "The magnetic loop wide open: fuzz on the quiet notes, a brighter, rougher top.", "studio", 1, noise_db=-66,
           bias=-80, drive=6, wow=0.03, flutter=0.02),
    preset(C, "Dusty Cassette", "A ferric tape that has seen some use: wobble, dropouts, a tilted head, hiss.", "cassette", 0, noise_db=-50,
           drive=3, wow=0.25, flutter=0.12, wear=45, dropouts=30, azimuth=25, scrape=30),
    preset(C, "Chrome Mixtape", "The good copy: chrome tape with mild noise reduction, a gentle wobble.", "cassette", 1, noise_db=-58,
           nr=1, drive=2, wow=0.1, flutter=0.06, scrape=15),
    preset(C, "Pocket Player Jog", "A personal player on the move: strong wow from the reels, a closed top.", "cassette", 0, noise_db=-52,
           wow=0.6, flutter=0.2, wear=30, highcut=10000, wobble=1),
    preset(C, "Fourth-Generation Dub", "A copy of a copy of a copy: noise reduction that breathes, a dull, hissy top.", "cassette", 0, noise_db=-46,
           azimuth=45, wear=50, nr=2, mistrack=4, wow=0.2, flutter=0.1, dropouts=15),
    preset(C, "Answering Machine", "Half-speed tape, a narrow band, a voice from another room.", "cassette", 3, noise_db=-42,
           wow=0.8, flutter=0.3, drive=8, lowcut=300, highcut=4500),
    preset(C, "Chewed Tape", "The deck ate it: deep wobble, dropout clusters, badly worn.", "cassette", 0, noise_db=-48,
           wow=1.5, flutter=0.4, dropouts=70, wear=80, scrape=50),
    preset(C, "Hot Ferric Saturation", "Ferric tape recorded hot: the top end closes as the level rises.", "cassette", 0, noise_db=-52,
           drive=10, saturation=60, wow=0.1, flutter=0.06),
    preset(C, "Forgotten in the Car", "A summer on the dashboard: a warped shell, worn oxide, a ghost echo, 6 kHz of top.", "cassette", 0, noise_db=-48, print_db=-45,
           wear=70, wow=0.9, flutter=0.2, highcut=6000, dropouts=35),
    preset(C, "Lo-Fi Beat Tape", "A soft, wobbly cassette for beats: saturated, hissy, never harsh.", "cassette", 0, noise_db=-50,
           drive=6, saturation=60, wow=0.35, flutter=0.12, wear=20, highcut=11000, scrape=20),
    preset(V, "Broken VHS", "A rental copy that has lost its tracking: servo wobble, edge damage, hum.", "video", 0, noise_db=-44,
           wow=0.6, flutter=0.2, dropouts=50, hum=45, wear=40),
    preset(V, "Rental Copy, EP Mode", "Long play: 4.5 kHz of top, more hiss, a constant wobble.", "video", 1, noise_db=-40,
           wow=0.35, flutter=0.15, wear=40),
    preset(V, "Tracking Error", "Dropouts everywhere and the field-rate buzz in the audio.", "video", 0, noise_db=-42,
           dropouts=60, wear=60, hum=65, wow=0.4, flutter=0.15),
    preset(VI, "Worn Vinyl", "A loved LP: crackle, clicks, two scratches, the stylus moving in as the song plays.", "vinyl", 0, noise_db=-52,
           crackle=40, scratches=2, wow=0.12, flutter=0.03),
    preset(VI, "Clean Pressing", "Just the air of a record: a faint surface, a little crackle.", "vinyl", 0, noise_db=-66,
           crackle=5, wow=0.05, flutter=0.02),
    preset(VI, "Thrift Store 45", "A 45 from the bargain bin: busy crackle, one deep scratch, inner-groove loss.", "vinyl", 1, noise_db=-54,
           crackle=60, scratches=1, groove=50, groove_follow=0, wow=0.1),
    preset(VI, "Shellac 78", "A 78: mono, 5 kHz of top, the roar of shellac.", "vinyl", 2, noise_db=-38,
           crackle=70, saturation=40, wow=0.15),
    preset(VI, "Warped LP", "A warped record: a slow pitch wobble at the turn rate and a subsonic sway.", "vinyl", 0, noise_db=-58,
           wow=0.6, crackle=20),
    preset(VI, "Inner Groove Blues", "The last track of a side: dull top, tracing distortion, crackle.", "vinyl", 0, noise_db=-56,
           groove=100, groove_follow=0, saturation=60, crackle=25),
    preset(R, "Old Radio", "A narrow AM set: a squeezed station, a little static and fading, a small speaker.", "radio", 0, noise_db=-50,
           compress=60, crackle=25, fading=20),
    preset(R, "Distant Station", "Barely there: deep selective fading, static, a whistle, slightly off the station.", "radio", 0, noise_db=-46,
           compress=70, fading=70, crackle=60, whistle=30, tuning=20),
    preset(R, "Telephone", "A landline: 300-3400 Hz, mu-law, the handset's bump.", "phone", 0, noise_db=-50),
    preset(R, "Mobile Call Breakup", "A weak mobile call: grit, frames dropping out in bursts.", "phone", 1, noise_db=-56,
           codec=60, frameloss=40),
    preset(R, "Intercom", "A wall intercom: narrow, hard-driven, with mains hum.", "phone", 0, noise_db=-48,
           drive=10, hum=55, lowcut=400, highcut=3000),
    preset(L, "Tape Stop Drop", "Clean tape with a one-bar stop on the button or any MIDI note.", "studio", 1,
           stop_sync=5, stop_curve=60, start_time=0.4),
    preset(L, "Half-Speed Dream", "Chrome cassette slowed down an octave: dark, slow, dreamy.", "cassette", 1, noise_db=-56,
           varispeed=-12, wow=0.2, flutter=0.08),
    preset(L, "12-bit Sampler", "The late-80s sampler sheen: 12 bits at 26 kHz, clean filters.", "studio", 0,
           bits=12, rate=26000, drive=3),
    preset(L, "Toy Sampler", "8 bits at 8 kHz with the aliasing left in, a wobbly cassette under it.", "cassette", 0, noise_db=-48,
           bits=8, rate=8000, crush_mode=1, jitter=20, wow=0.3, flutter=0.1),
]

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    for i, p in enumerate(PRESETS):
        p["order"] = i
        slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
        with open(os.path.join(OUT, f"p{i:02d}_{slug}.json"), "w") as fh:
            json.dump(p, fh, indent=1, ensure_ascii=False)
    print(f"{len(PRESETS)} presets en {os.path.normpath(OUT)}")

if __name__ == "__main__":
    main()
