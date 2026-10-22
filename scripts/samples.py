#!/usr/bin/env python3
"""Samples de la web: MIDI compuesto para cada preset → render con el motor real → AAC + forma de onda.

    python3 scripts/samples.py [--only at42]

Para cada sample escribe web/assets/samples/<plugin>/<id>.mid (el MIDI, descargable), lo renderiza
con at20vrender / at101vrender / at42render (build/), normaliza a −16 LUFS con ffmpeg, codifica
<id>.m4a (AAC 160 kbps) y guarda los picos de la forma de onda en web/assets/samples/samples.json,
que lee scripts/build-web.py. Las piezas son originales (no hay melodías de terceros).
"""
import json, math, os, struct, subprocess, sys, tempfile, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "web", "assets", "samples")
RENDER = {p: os.path.join(ROOT, "build", "plugins", p, f"{p}render_artefacts", "Release", f"{p}render") for p in ("at20v", "at101v", "at42", "at700", "atfs1", "atmd")}
PRESETS = {p: os.path.join(ROOT, "plugins", p, "assets", "presets") for p in ("at20v", "at101v", "at42", "at700", "atfs1", "atmd")}

# ───────── MIDI (SMF tipo 0, 480 ticks por negra) ─────────
TPQ = 480
class Song:
    def __init__(self, bpm): self.bpm = bpm; self.ev = []      # (beat, prioridad, bytes)
    def note(self, beat, dur, n, vel=96):
        self.ev.append((beat, 1, bytes([0x90, n, vel]))); self.ev.append((beat + dur, 0, bytes([0x80, n, 0])))
    def chord(self, beat, dur, ns, vel=88):
        for n in ns: self.note(beat, dur, n, vel)
    def bend(self, beat, v):                       # v en −1..1
        x = max(0, min(16383, int(8192 + v * 8191)))
        self.ev.append((beat, 2, bytes([0xE0, x & 0x7F, x >> 7])))
    def cc(self, beat, num, val): self.ev.append((beat, 2, bytes([0xB0, num, val])))
    def write(self, path):
        data = bytearray(b"\x00\xFF\x51\x03" + struct.pack(">I", int(60_000_000 / self.bpm))[1:])
        last = 0
        for beat, _, msg in sorted(self.ev, key=lambda e: (e[0], e[1])):
            t = int(round(beat * TPQ)); data += vlq(t - last) + msg; last = t
        data += b"\x00\xFF\x2F\x00"
        with open(path, "wb") as f:
            f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ) + b"MTrk" + struct.pack(">I", len(data)) + data)
    def seconds(self): return max(b for b, _, _ in self.ev) * 60.0 / self.bpm

def vlq(n):
    out = [n & 0x7F]; n >>= 7
    while n: out.insert(0, (n & 0x7F) | 0x80); n >>= 7
    return bytes(out)

N = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}
def m(name):
    """'A2' → 45 (C4 = 60)."""
    i = 2 if len(name) > 2 and name[1] in "#b" else 1
    return N[name[:i]] + 12 * (int(name[i:]) + 1)

# ───────── piezas ─────────
def acid_bass():                                   # AT20V: línea acid en La menor, semicorcheas
    s = Song(124)
    pat = ["A1", "A1", "A2", "A1", "-", "A1", "C2", "A1", "D2", "A1", "-", "E2", "A1", "G2", "A1", "A2"]
    for bar in range(8):
        root = [0, 0, 3, 5, 0, 0, -2, 3][bar]
        for i, p in enumerate(pat):
            if p == "-": continue
            s.note(bar * 4 + i * 0.25, 0.2 if i % 4 else 0.24, m(p) + root, 120 if i % 4 == 0 else 84)
    return s

def scream_lead():                                 # AT20V: frase de lead en Mi menor, notas largas
    s = Song(96)
    ph = [("E4", 0, 1.5), ("G4", 1.5, 0.5), ("B4", 2, 2), ("A4", 4, 1), ("G4", 5, 1), ("F#4", 6, 2),
          ("E4", 8, 1.5), ("D4", 9.5, 0.5), ("E4", 10, 1), ("G4", 11, 1), ("B4", 12, 1.5), ("D5", 13.5, 0.5), ("E5", 14, 4)]
    for n, b, d in ph: s.note(b, d * 0.96, m(n), 110)
    return s

def hoover_riff():                                 # AT20V: riff de octavas y quintas, 140 BPM
    s = Song(140)
    for bar in range(8):
        r = [m("D2"), m("D2"), m("F2"), m("C2")][bar % 4]
        for i, (b, d, iv) in enumerate([(0, 0.75, 0), (0.75, 0.25, 12), (1.5, 0.5, 7), (2, 0.75, 0), (3, 0.5, 12), (3.5, 0.5, 10)]):
            s.note(bar * 4 + b, d * 0.9, r + iv, 118)
    return s

def cathedral_pads(base=57):                       # pads: Am9 – Fmaj7 – Cmaj7 – G6sus, a 70 BPM
    s = Song(70)
    chords = [[57, 60, 64, 67, 71], [53, 57, 60, 64, 69], [48, 55, 59, 64, 67], [55, 57, 62, 64, 71]]
    for i, c in enumerate(chords): s.chord(i * 4, 3.9, [n - 57 + base for n in c], 80)
    return s

def held(note, secs, bpm=120):                     # una nota sostenida (presets que se tocan solos)
    s = Song(bpm); s.note(0, secs * bpm / 60.0, note, 100); return s

def organ_comp():                                  # AT20V: comping de órgano, progresión gospel en Fa
    s = Song(96)
    prog = [[53, 57, 60, 65], [58, 62, 65, 70], [57, 60, 64, 69], [55, 58, 62, 67], [53, 57, 60, 65], [58, 62, 65, 70], [60, 64, 67, 70], [53, 57, 60, 65]]
    for bar, c in enumerate(prog):
        for b, d in ((0, 1.4), (1.5, 0.4), (2, 1.4), (3.5, 0.4)): s.chord(bar * 4 + b, d, c, 96)
    return s

def house_bass():                                  # AT101V: bajo house con octavas, 122 BPM
    s = Song(122)
    for bar in range(8):
        r = [m("F1"), m("F1"), m("Ab1"), m("Eb1")][bar % 4]
        for b, d, iv in ((0, 0.4, 0), (0.75, 0.2, 12), (1.5, 0.4, 0), (2, 0.2, 12), (2.5, 0.4, 0), (3.25, 0.2, 7), (3.5, 0.4, 12)):
            s.note(bar * 4 + b, d, r + iv, 110)
    return s

def legato_lead():                                 # AT101V: melodía con portamento (notas solapadas) y vibrato por rueda
    s = Song(92)
    ph = [("C4", 0, 1), ("Eb4", 1, 1), ("G4", 2, 1.5), ("Bb4", 3.5, 0.5), ("C5", 4, 3), ("Bb4", 7, 0.5), ("G4", 7.5, 0.5),
          ("Ab4", 8, 1.5), ("G4", 9.5, 0.5), ("F4", 10, 1), ("Eb4", 11, 1), ("D4", 12, 1), ("C4", 13, 3)]
    for n, b, d in ph: s.note(b, d + 0.06, m(n), 100)               # solapadas: AUTO desliza
    for b in (4.5, 13.5): s.cc(b, 1, 0); s.cc(b + 0.8, 1, 90); s.cc(b + 2.4, 1, 0)
    return s

def arp_chords():                                  # AT101V: acordes para el arpegiador (Cm – Ab – Eb – Bb)
    s = Song(120)
    for i, c in enumerate([[48, 51, 55, 60], [44, 48, 51, 56], [51, 55, 58, 63], [46, 50, 53, 58]]): s.chord(i * 4, 3.95, c, 96)
    return s

def transposer(notes, bars, bpm):                  # AT101V: con PLAY, cada tecla transpone la secuencia
    s = Song(bpm)
    for i, n in enumerate(notes): s.note(i * bars * 4, 0.5, n, 90)
    s.note(len(notes) * bars * 4 + 3.5, 0.1, 60, 1)                 # marca el final
    return s

def whistle_melody():                              # AT101V: melodía lenta para el filtro autooscilante
    s = Song(76)
    for n, b, d in [("G4", 0, 2), ("D5", 2, 2), ("C5", 4, 1), ("B4", 5, 1), ("A4", 6, 2), ("G4", 8, 1.5), ("A4", 9.5, 0.5), ("B4", 10, 2), ("D5", 12, 4)]:
        s.note(b, d + 0.05, m(n), 96)
    return s

def drone_keys(keys, secs, bpm=60):                # AT42: teclas drone (C1 = tecla 1 … F1 = tecla 6)
    s = Song(bpm)
    for k, start in keys: s.note(start, secs - start - 1, 23 + k, 100)
    return s

def flute_phrase():                                # AT42: frase de flauta en el teclado táctil, pentatónica menor
    s = Song(84)
    ph = [("A3", 0, 1.5), ("C4", 1.5, 0.5), ("D4", 2, 1), ("E4", 3, 1), ("G4", 4, 2), ("E4", 6, 1), ("D4", 7, 1),
          ("C4", 8, 1.5), ("D4", 9.5, 0.5), ("E4", 10, 2), ("A4", 12, 4)]
    for n, b, d in ph: s.note(b, d * 0.97, m(n), 100)
    s.bend(12.5, 0.0); s.bend(13.2, 0.35); s.bend(14.0, 0.0)
    return s

def pent_chords():                                 # AT42: acordes para el arpegiador del teclado (La menor)
    s = Song(104)
    for i, c in enumerate([[57, 60, 64, 67], [53, 57, 60, 65], [55, 59, 62, 67], [52, 55, 59, 64]]): s.chord(i * 4, 3.9, c, 96)
    return s

def brass_fanfare():                               # AT700: fanfarria lenta en Si bemol, acordes con la melodía arriba
    s = Song(72)
    prog = [(0, 2, [46, 58, 62, 65]), (2, 2, [45, 57, 60, 65]), (4, 2, [43, 58, 62, 67]), (6, 4, [39, 55, 58, 63, 67]),
            (10, 2, [41, 57, 60, 65, 69]), (12, 2, [43, 58, 62, 67, 70]), (14, 6, [46, 58, 62, 65, 70])]
    for b, d, c in prog: s.chord(b, d * 0.97, c, 92)
    return s

def tomita_whistle():                              # AT700: melodía lenta y ligada para el filtro autooscilante
    s = Song(66)
    ph = [("A4", 0, 2), ("D5", 2, 1), ("E5", 3, 1), ("F#5", 4, 2.5), ("E5", 6.5, 0.5), ("D5", 7, 1), ("B4", 8, 2),
          ("A4", 10, 1), ("B4", 11, 1), ("D5", 12, 3), ("C#5", 15, 1), ("D5", 16, 4)]
    for n, b, d in ph: s.note(b, d + 0.08, m(n), 100)
    return s

def bell_motif():                                  # AT700: motivo de campanas, notas sueltas y un acorde final
    s = Song(84)
    for n, b, d in [("C5", 0, 1.5), ("G5", 1.5, 1.5), ("E5", 3, 1), ("D5", 4, 2), ("A4", 6, 1), ("C5", 7, 1), ("G5", 8, 1.5),
                    ("E6", 9.5, 0.5), ("D6", 10, 2)]:
        s.note(b, d * 0.9, m(n), 104)
    s.chord(12, 4, [m("C5"), m("G5"), m("E6")], 96)
    return s

def sync_phrase():                                 # AT700: frase de lead en La menor; cada nota vuelve a barrer el sync
    s = Song(108)
    for n, b, d in [("A4", 0, 1), ("C5", 1, 1), ("D5", 2, 0.5), ("E5", 2.5, 1.5), ("G5", 4, 1), ("E5", 5, 1), ("D5", 6, 2),
                    ("C5", 8, 1), ("D5", 9, 0.5), ("E5", 9.5, 0.5), ("A4", 10, 4)]:
        s.note(b, d * 0.95, m(n), 100)
    return s

def string_chords():                               # AT700: cuerdas, Rem9 – Sib maj7 – Fa/La – Do sus
    s = Song(66)
    for i, c in enumerate([[50, 57, 64, 65, 72], [46, 57, 62, 65, 69], [45, 53, 60, 65, 69], [48, 55, 60, 65, 67]]):
        s.chord(i * 4, 3.95, c, 84)
    return s

def synthpop_bass():                               # AT700: bajo de corcheas, synth-pop en Mi menor
    s = Song(118)
    pat = ["E2", "E2", "E3", "E2", "G2", "A2", "E2", "B2"]
    for bar in range(8):
        root = [0, 0, -4, -2, 0, 0, -7, -5][bar]
        for i, n in enumerate(pat): s.note(bar * 4 + i * 0.5, 0.42, m(n) + root, 112 if i % 2 == 0 else 90)
    return s

def run_only(beats, bpm=120):                       # AT700: el secuenciador toca solo; el MIDI sólo fija la duración
    s = Song(bpm); s.cc(beats, 64, 0)
    return s

def talking_phrase():                              # ATFS1: la frase del Fseq dicha en cuatro alturas
    s = Song(60)
    for i, n in enumerate(["C3", "Eb3", "G3", "F3", "Ab2", "C3"]):
        s.note(i * 2, 1.9, m(n), 100)
    return s

def choir_chords():                                # ATFS1: coro de vocales, Re menor
    s = Song(60)
    for i, ch in enumerate([[50, 57, 60, 65, 69], [46, 53, 57, 62, 69], [41, 48, 57, 60, 64], [48, 55, 60, 64, 67]]):
        s.chord(i * 4, 3.8, ch, 84)
    return s

def scat_line():                                   # ATFS1: "shoo bee doo wah" en una línea de swing
    s = Song(64)
    for i, n in enumerate(["G3", "Bb3", "C4", "D4", "F3", "G3"]):
        s.note(i * 2, 1.95, m(n), 104)
    return s

def formant_strings():                             # ATFS1: acordes con la perilla FORMANT (CC 80) moviéndose
    s = Song(66)
    for i, ch in enumerate([[45, 52, 57, 60, 64], [41, 48, 53, 57, 64], [43, 50, 55, 59, 62], [40, 47, 52, 55, 59]]):
        s.chord(i * 4, 3.9, ch, 86)
    for k in range(65):
        v = int(64 + 50 * math.sin(k / 64 * math.pi * 2))
        s.cc(k * 0.25, 80, max(0, min(127, v)))
    return s

def tine_comp():                                   # ATFS1: piano de púas, acordes de jazz
    s = Song(92)
    prog = [[50, 53, 57, 60], [55, 59, 62, 65], [48, 52, 55, 59], [45, 48, 52, 55]]
    for bar, ch in enumerate(prog * 2):
        s.chord(bar * 4, 1.4, ch, 90); s.chord(bar * 4 + 1.5, 0.45, ch, 70); s.chord(bar * 4 + 2.5, 1.3, [n + (12 if j == 3 else 0) for j, n in enumerate(ch)], 82)
    return s

def formant_lead_line():                           # ATFS1: melodía con la rueda moviendo los formantes
    s = Song(84)
    ph = [("A4", 0, 1), ("C5", 1, 1), ("E5", 2, 2), ("D5", 4, 1), ("C5", 5, 0.5), ("B4", 5.5, 0.5), ("A4", 6, 2), ("G4", 8, 1), ("A4", 9, 1), ("C5", 10, 3)]
    for n, b, d in ph: s.note(b, d * 0.98, m(n), 104)
    for k in range(53):
        s.cc(k * 0.25, 1, int(63 + 60 * math.sin(k / 52 * math.pi * 3)))
    return s

def yah_riff():                                    # ATFS1: el Fseq rítmico, una nota por compás
    s = Song(96)
    for i, n in enumerate(["A2", "A2", "C3", "G2", "A2", "A2", "D3", "E3"]):
        s.note(i * 2, 1.95, m(n), 108)
    return s

def scratch_moves():                               # ATFS1: la rueda recorre el Fseq ("formant scratch")
    s = Song(90)
    s.note(0, 15.5, m("C3"), 100); s.note(8, 7.5, m("G3"), 90)
    pts = [(0, 0), (1, 70), (1.5, 30), (2.5, 110), (3, 60), (4, 127), (5, 0), (5.5, 50), (6, 20), (7, 90), (8, 40), (9, 127), (10, 64), (11, 100), (12, 10), (13, 80), (14, 30), (15, 127)]
    for (b0, v0), (b1, v1) in zip(pts[:-1], pts[1:]):
        steps = int((b1 - b0) * 16)
        for k in range(steps): s.cc(b0 + (b1 - b0) * k / steps, 1, int(v0 + (v1 - v0) * k / steps))
    return s

def find(plugin, slug):
    for f in sorted(os.listdir(PRESETS[plugin])):
        if f.endswith(".json") and f.split("_", 1)[1][:-5] == slug: return os.path.join(PRESETS[plugin], f)
    raise SystemExit(f"no está el preset {plugin}/{slug}")

# (id, plugin, preset, pieza, extra args, título EN, título ES, descripción EN, descripción ES)

# ───────── ATMD (Model D) ─────────
def funk_bass():                                   # ATMD: bajo funk en Mi menor, 100 BPM
    s = Song(100)
    pat = [(0, 0.35, 0, 118), (0.75, 0.2, 12, 90), (1.0, 0.3, 0, 104), (1.5, 0.2, 10, 92), (2.0, 0.45, 7, 110), (2.75, 0.2, 0, 92),
           (3.0, 0.3, 3, 100), (3.5, 0.4, 5, 104)]
    for bar in range(8):
        r = [m("E1"), m("E1"), m("G1"), m("A1"), m("E1"), m("E1"), m("C2"), m("D2")][bar]
        for b, d, iv, v in pat: s.note(bar * 4 + b, d, r + iv, v)
    return s

def prog_lead():                                   # ATMD: solo legato con glide y vibrato por la rueda (osc 3 como LFO)
    s = Song(84)
    ph = [("A3", 0, 1), ("C4", 1, 0.5), ("E4", 1.5, 0.5), ("A4", 2, 2), ("G4", 4, 0.5), ("E4", 4.5, 0.5), ("D4", 5, 1), ("E4", 6, 2),
          ("C5", 8, 1), ("B4", 9, 0.5), ("A4", 9.5, 0.5), ("G4", 10, 1), ("A4", 11, 1), ("E5", 12, 4)]
    for n, b, d in ph: s.note(b, d + 0.05, m(n), 104)          # solapadas: el disparo único no redispara, el glide desliza
    for b in (2.6, 6.5, 12.6): s.cc(b, 1, 0); s.cc(b + 0.7, 1, 58); s.cc(b + 1.4, 1, 0)     # la rueda es casi cúbica: 58 ≈ ±0.7 st
    return s

def taurus_line():                                 # ATMD: notas largas y graves, con glide
    s = Song(72)
    for b, n, d in [(0, "C2", 3.9), (4, "G1", 3.9), (8, "Ab1", 3.9), (12, "Bb1", 1.95), (14, "Eb2", 2.5)]: s.note(b, d, m(n), 110)
    return s

def squelch_line():                                # ATMD: semicorcheas con acentos (el contorno del filtro con resonancia)
    s = Song(126)
    pat = [0, 0, 12, 0, 3, 0, 15, 0, 0, 7, 0, 12, 10, 0, 3, 5]
    for bar in range(8):
        r = [m("C2"), m("C2"), m("Eb2"), m("Bb1")][bar % 4]
        for i, iv in enumerate(pat): s.note(bar * 4 + i * 0.25, 0.18, r + iv, 122 if i % 4 == 0 else 84)
    return s

def brass_line():                                  # ATMD: fanfarria monofónica
    s = Song(104)
    ph = [("G3", 0, 0.5), ("C4", 0.5, 0.5), ("E4", 1, 1), ("G4", 2, 1.5), ("E4", 3.5, 0.5), ("F4", 4, 1), ("D4", 5, 1), ("G4", 6, 2),
          ("A4", 8, 0.75), ("G4", 8.75, 0.25), ("F4", 9, 1), ("E4", 10, 1), ("D4", 11, 1), ("C4", 12, 4)]
    for n, b, d in ph: s.note(b, d * 0.92, m(n), 110)
    return s

def zap_hits():                                    # ATMD: disparos de efecto sobre distintas alturas
    s = Song(90)
    for i, n in enumerate(["C5", "G4", "C5", "E5", "C4", "G4", "A4", "C6"]): s.note(i * 1.0, 0.25, m(n), 120)
    return s

SAMPLES = [
    ("acid-bass", "at20v", "acid_bass", acid_bass(), [], "Acid Bass", "Acid Bass",
     "A 124 BPM acid line through the Korg35 low-pass.", "Una línea acid a 124 BPM por el pasa bajos Korg35."),
    ("scream-lead", "at20v", "scream_lead", scream_lead(), [], "Scream Lead", "Scream Lead",
     "A slow phrase with the resonance pushed into the scream.", "Una frase lenta con la resonancia llevada al grito."),
    ("hoover", "at20v", "hoover_square", hoover_riff(), [], "Hoover Square", "Hoover Square",
     "An octave riff at 140 BPM.", "Un riff de octavas a 140 BPM."),
    ("glass-cathedral", "at20v", "glass_cathedral", cathedral_pads(), [], "Glass Cathedral", "Glass Cathedral",
     "Four polyphonic chords with delay and reverb.", "Cuatro acordes polifónicos con delay y reverb."),
    ("hammond", "at20v", "hammond_organ", organ_comp(), [], "Hammond Organ", "Hammond Organ",
     "A factory chart from the 1978 manual, comping in F.", "Una carta de fábrica del manual de 1978, acompañando en Fa."),
    ("random-bleeps", "at20v", "random_bleeps", held(48, 16), [], "Random Bleeps", "Random Bleeps",
     "One key held: the patch bay plays the rest.", "Una tecla apretada: la patchera toca el resto."),

    ("acid-line", "at101v", "acid_line", transposer([60, 63, 65, 58], 2, 120), [], "Acid Line", "Acid Line",
     "The step sequencer, transposed from the keys every two bars.", "El secuenciador de pasos, transpuesto desde las teclas cada dos compases."),
    ("house-bass", "at101v", "101_bass", house_bass(), [], "101 Bass", "101 Bass",
     "A house bass line with octave jumps.", "Una línea de bajo house con saltos de octava."),
    ("classic-lead", "at101v", "classic_lead", legato_lead(), [], "Classic Lead", "Classic Lead",
     "A legato melody: Auto portamento slides, the mod wheel adds vibrato.", "Una melodía legato: el portamento Auto desliza y la rueda suma vibrato."),
    ("arp-up", "at101v", "arp_up", arp_chords(), [], "Arp Up", "Arp Up",
     "Four chords through the arpeggiator, clocked by the modulator.", "Cuatro acordes por el arpegiador, con el modulador como reloj."),
    ("soft-pad", "at101v", "soft_pad", cathedral_pads(57), [], "Soft Pad", "Soft Pad",
     "Six voices, PWM and chorus: the SH-101 as a pad.", "Seis voces, PWM y chorus: el SH-101 como pad."),
    ("whistle", "at101v", "whistle", whistle_melody(), [], "Whistle", "Whistle",
     "No oscillator at all: the filter self-oscillates and follows the keys.", "Ningún oscilador: el filtro autooscila y sigue al teclado."),

    ("cathedral-a", "at42", "cathedral_of_a", drone_keys([(1, 0), (2, 3)], 22), ["--keys", "1,2"], "Cathedral of A", "Cathedral of A",
     "Two drone voices tuned to A, through the shimmer cartridge.", "Dos voces drone afinadas en La, por el cartucho shimmer."),
    ("microtonal-fog", "at42", "microtonal_fog", drone_keys([(1, 0), (5, 2)], 22), ["--keys", "1,5"], "Microtonal Fog", "Microtonal Fog",
     "The LFOs light the orbs; the drones drift with the light.", "Los LFO encienden los orbes; los drones derivan con la luz."),
    ("birds-dusk", "at42", "birds_at_dusk", drone_keys([(3, 0), (6, 1)], 18), ["--keys", "3,6"], "Birds at Dusk", "Birds at Dusk",
     "The two noise voices with FM and AM: chirps and calls.", "Las dos voces de ruido con FM y AM: trinos y llamados."),
    ("berlin-five", "at42", "berlin_five", drone_keys([(1, 0)], 18), ["--keys", "1"], "Berlin Five", "Berlin Five",
     "The 5-step sequencer drives both VCOs and opens the filter.", "El secuenciador de 5 pasos maneja los dos VCO y abre el filtro."),
    ("flute-lead", "at42", "flute_lead", flute_phrase(), [], "Flute Lead", "Flute Lead",
     "VCO A on the touch keyboard, in the minor pentatonic scale.", "El VCO A en el teclado táctil, en la escala pentatónica menor."),
    ("arp-constellation", "at42", "arp_constellation", pent_chords(), ["--keys", "1"], "Arp Constellation", "Arp Constellation",
     "Chords through the touch keyboard's arpeggiator, over a drone.", "Acordes por el arpegiador del teclado táctil, sobre un drone."),
    ("ocean-noise", "at42", "ocean_of_noise", drone_keys([(3, 0), (6, 0)], 20), ["--keys", "3,6"], "Ocean of Noise", "Ocean of Noise",
     "Filtered noise swept by two slow LFOs.", "Ruido filtrado barrido por dos LFO lentos."),

    ("brass-fanfare", "at700", "tomita_brass", brass_fanfare(), [], "Tomita Brass", "Tomita Brass",
     "Three saws per voice through the 24 dB filter, six voices, spring reverb.", "Tres sierras por voz por el filtro de 24 dB, seis voces, reverb de resorte."),
    ("berlin-twelve", "at700", "berlin_twelve", run_only(36, 128), [], "Berlin Twelve", "Berlin Twelve",
     "The 717A plays twelve steps; channel B opens the filter on the accents.", "El 717A toca doce pasos; el canal B abre el filtro en los acentos."),
    ("tomita-whistle", "at700", "tomita_whistle", tomita_whistle(), [], "Tomita Whistle", "Tomita Whistle",
     "No oscillator: the filter self-oscillates, follows the keys and glides.", "Ningún oscilador: el filtro autooscila, sigue al teclado y desliza."),
    ("sync-sweep", "at700", "sync_sweep", sync_phrase(), [], "Sync Sweep", "Sync Sweep",
     "VCO-2 synced to VCO-1; ADSR-1 sweeps its pitch on every note.", "El VCO-2 sincronizado al VCO-1; el ADSR-1 barre su afinación en cada nota."),
    ("ring-bells", "at700", "ring_bells", bell_motif(), [], "Ring Bells", "Ring Bells",
     "Two sine VCOs into the ring modulator, a long decay and the spring.", "Dos VCO en seno al modulador en anillo, un decaimiento largo y el resorte."),
    ("ensemble-strings", "at700", "ensemble_strings", string_chords(), [], "Ensemble Strings", "Ensemble Strings",
     "PWM from LFO-2 through a multiple, then the 8-stage phaser of the output module.", "PWM del LFO-2 por un múltiple y el phaser de 8 etapas del módulo de salida."),
    ("synthpop-bass", "at700", "sheffield_bass", synthpop_bass(), [], "Sheffield Bass", "Sheffield Bass",
     "Two detuned saws and a sub pulse, eighth notes at 118 BPM.", "Dos sierras desafinadas y un pulso sub, en corcheas a 118 BPM."),
    ("thirty-six-steps", "at700", "thirty_six_steps", run_only(40, 150), [], "Thirty-Six Steps", "Thirty-Six Steps",
     "Channels A, B and C in series: one melody of 36 steps.", "Los canales A, B y C en serie: una melodía de 36 pasos."),

    ("talking-formants", "atfs1", "talking_formants", talking_phrase(), [], "Talking Formants", "Talking Formants",
     "A Formant Sequence analyzed from a spoken phrase, played at six pitches.", "Un Formant Sequence analizado de una frase hablada, tocado en seis alturas."),
    ("vowel-choir", "atfs1", "vowel_choir", choir_chords(), [], "Vowel Choir", "Vowel Choir",
     "Eight formant operators per voice follow a looping o-a vowel, with symphonic chorus.", "Ocho operadores de formante por voz siguen una vocal o-a en lazo, con chorus sinfónico."),
    ("shoo-bee-doo", "atfs1", "shoo_bee_doo", scat_line(), [], "Shoo Bee Doo", "Shoo Bee Doo",
     "A scat phrase turned into formant tracks and sung back by the synth.", "Una frase de scat convertida en pistas de formantes y cantada por el sinte."),
    ("formant-strings", "atfs1", "formant_strings", formant_strings(), [], "Formant Strings", "Formant Strings",
     "The FORMANT knob (CC 80) moves three formant centres while the chords play.", "La perilla FORMANT (CC 80) mueve tres centros de formante mientras suenan los acordes."),
    ("tine-piano", "atfs1", "tine_piano", tine_comp(), [], "Tine Piano", "Tine Piano",
     "Two FM stacks with a 14:1 modulator for the tine, through the tremolo.", "Dos pilas FM con un modulador 14:1 para la púa, por el trémolo."),
    ("formant-lead", "atfs1", "formant_lead", formant_lead_line(), [], "Formant Lead", "Formant Lead",
     "A mono lead whose mod wheel slides the formants from o to e.", "Un lead mono: la rueda desliza los formantes de o a e."),
    ("yah-yah-riff", "atfs1", "yah_yah_riff", yah_riff(), [], "Yah Yah Riff", "Yah Yah Riff",
     "A rhythmic vowel sequence on a loop, one note per bar.", "Una secuencia rítmica de vocales en lazo, una nota por compás."),
    ("formant-scratch", "atfs1", "formant_scratch", scratch_moves(), [], "Formant Scratch", "Formant Scratch",
     "The mod wheel scrubs through a spoken Fseq, back and forth.", "La rueda recorre un Fseq hablado, para adelante y para atrás."),

    ("fat-bass", "atmd", "fat_bass", funk_bass(), [], "Fat Bass", "Fat Bass",
     "Three sawtooths, one an octave down, a funk line through the ladder filter.", "Tres sierras, una una octava abajo, una línea funk por el filtro escalera."),
    ("lucky-lead", "atmd", "lucky_lead", prog_lead(), [], "Lucky Lead", "Lucky Lead",
     "A legato solo: single trigger, glide, and vibrato from the mod wheel with oscillator 3 as LFO.", "Un solo legato: disparo único, glide y vibrato con la rueda, el osc 3 como LFO."),
    ("taurus-low", "atmd", "taurus_low", taurus_line(), [], "Taurus Low", "Taurus Low",
     "Long notes in the 32' range, gliding from one to the next.", "Notas largas en el rango de 32', deslizando de una a otra."),
    ("squelch", "atmd", "squelch_bass", squelch_line(), [], "Squelch Bass", "Squelch Bass",
     "Sixteenths with the emphasis high and the filter contour snapping open.", "Semicorcheas con el énfasis alto y el contorno del filtro abriendo de golpe."),
    ("brass", "atmd", "brass_section", brass_line(), [], "Brass Section", "Brass Section",
     "Three detuned oscillators and a slow filter attack.", "Tres osciladores desafinados y un ataque lento del filtro."),
    ("singing-filter", "atmd", "singing_filter", whistle_melody(), [], "Singing Filter", "Singing Filter",
     "The emphasis past 8: the filter sings on top of a quiet sawtooth.", "El énfasis pasado de 8: el filtro canta sobre una sierra suave."),
    ("poly-strings", "atmd", "poly_strings", cathedral_pads(57), [], "Poly Strings", "Poly Strings",
     "The polyphonic extension: six voices of the Model D with reverb.", "La extensión polifónica: seis voces del Model D con reverb."),
    ("laser-zap", "atmd", "laser_zap", zap_hits(), [], "Laser Zap", "Laser Zap",
     "Oscillator 3's reverse sawtooth sweeping the pitch of oscillator 1.", "La sierra invertida del osc 3 barriendo la afinación del osc 1."),
]

# ───────── Orrery: el generador tocando nuestros sintes ─────────
# (id, preset de Orrery, {carril: (plugin, preset del sinte, ganancia)}, títulos y descripciones). Cada carril se
# renderiza con el motor de su sinte desde el MIDI que generó Orrery y se mezcla; "preview" usa su propio sonido.
ORRERY = [
    ("first-light", "first_light", {1: ("at700", "space_drone", 0.6), 2: ("atfs1", "formant_strings", 0.8), 3: ("atfs1", "breath_flute", 0.8),
                                    5: ("atfs1", "formant_bells", 0.55), 6: ("at101v", "sub_rumble", 0.6)},
     "First Light", "First Light", "Ambient: drone on AT700, pad, flute and bells on ATFS1, sub bass on AT101V.",
     "Ambient: drone en el AT700, pad, flauta y campanas en el ATFS1, bajo en el AT101V."),
    ("tape-loops", "tape_loops", {1: ("atfs1", "glass_marimba", 0.7), 2: ("atfs1", "glass_marimba", 0.7), 3: ("atfs1", "formant_bells", 0.6),
                                  4: ("atfs1", "formant_bells", 0.5), 5: ("at700", "tine_keys", 0.6), 6: ("at20v", "breathing_strings", 0.6)},
     "Tape Loops", "Tape Loops", "Five loops of 17, 21, 23, 29 and 31 beats, Eno style, over a soft pad.",
     "Cinco lazos de 17, 21, 23, 29 y 31 pulsos, a lo Eno, sobre un pad suave."),
    ("undertow", "undertow", {1: ("at20v", "deep_ocean_drone", 0.6), 2: ("at700", "phaser_pad", 0.7), 5: ("at20v", "lunar_bells", 0.5), 6: ("at101v", "dark_drone", 0.55)},
     "Undertow", "Undertow", "Dark ambient in C Phrygian with drifting pitch and a filter on a random walk.",
     "Dark ambient en Do frigio, con la afinación a la deriva y un filtro que camina al azar."),
    ("tanpura", "tanpura", {1: ("at20v", "endless_drift", 0.6), 3: ("atfs1", "glass_marimba", 0.65), 2: ("atfs1", "vowel_choir", 0.6)},
     "Tanpura", "Tanpura", "A drone with a four-note string pattern that never changes key.",
     "Un drone con un patrón de cuatro notas que nunca cambia de centro."),
    ("breath-six", "breath_six", {1: ("at101v", "soft_pad", 0.55), 2: ("atfs1", "vowel_choir", 0.7), 5: ("atfs1", "formant_bells", 0.5)},
     "Breath Six", "Breath Six", "Meditation at six breaths per minute: the choir swells as you breathe in.",
     "Meditación a seis respiraciones por minuto: el coro crece al inspirar."),
    ("orbiting", "orbiting", {4: ("at101v", "glass_bells", 0.55), 5: ("at20v", "lunar_bells", 0.5), 2: ("atfs1", "glass_choir", 0.7), 6: ("at700", "sub_pulse", 0.5)},
     "Orbiting", "Orbiting", "Celestial in E Lydian: a Euclidean arpeggio, high bells and a glass choir.",
     "Celestial en Mi lidio: un arpegio euclidiano, campanas agudas y un coro de cristal."),
    ("night-garden", "night_garden", "preview", "Night Garden", "Night Garden", "Sleep mood with Orrery's own preview sound, gentle mode on.",
     "Clima de dormir con el sonido de prueba de Orrery y el modo suave encendido."),
]

def orrery_samples(meta):
    import shutil
    tool = os.path.join(ROOT, "build", "plugins", "orrery", "orreryrender_artefacts", "Release", "orreryrender")
    pdir = os.path.join(ROOT, "plugins", "orrery", "assets", "presets")
    d = os.path.join(OUT, "orrery"); os.makedirs(d, exist_ok=True)
    keys = []
    for sid, preset, lanes, ten, tes, den, des in ORRERY:
        src = next(os.path.join(pdir, f) for f in sorted(os.listdir(pdir)) if f.split("_", 1)[1][:-5] == preset)
        with tempfile.TemporaryDirectory() as td:
            pr = json.load(open(src))
            pr["params"]["fade_in"] = 3; pr["params"]["fade_cc7"] = 0; pr["params"]["session"] = 0   # el clip arranca sonando
            pj = os.path.join(td, "p.json"); json.dump(pr, open(pj, "w"))
            gen = os.path.join(td, "gen")
            r = subprocess.run([tool, pj, gen, "--secs", "70"], capture_output=True, text=True)
            if r.returncode != 0: raise SystemExit(f"falló orreryrender en {sid}: {r.stdout} {r.stderr}")
            mix = os.path.join(td, "mix.wav")
            if lanes == "preview":
                shutil.copy(os.path.join(gen, "preview.wav"), mix)
            else:
                ins, filt = [], []
                for k, (plugin, ppreset, gain) in enumerate(lanes.values()):
                    lane = list(lanes.keys())[k]
                    out = os.path.join(td, f"l{lane}.wav")
                    rr = subprocess.run([RENDER[plugin], find(plugin, ppreset), out, "--midi", os.path.join(gen, f"lane{lane}.mid"), "--secs", "74"], capture_output=True, text=True)
                    if rr.returncode != 0 or not os.path.exists(out): raise SystemExit(f"falló {plugin}/{ppreset} en {sid}: {rr.stdout} {rr.stderr}")
                    ins += ["-i", out]; filt.append(f"[{k}:a]volume={gain}[a{k}]")
                n = len(filt)
                fc = ";".join(filt) + ";" + "".join(f"[a{k}]" for k in range(n)) + f"amix=inputs={n}:normalize=0[m]"
                subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *ins, "-filter_complex", fc, "-map", "[m]", "-c:a", "pcm_s24le", mix], check=True)
            loud = os.path.join(td, "loud.wav")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", mix, "-af", "atrim=0:68,loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000",
                            "-c:a", "pcm_s24le", loud], check=True)
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", loud], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", loud, "-af", f"afade=t=in:d=1.5,afade=t=out:st={max(0.0, dur - 3.0):.2f}:d=3",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            shutil.copy(os.path.join(gen, "all.mid"), os.path.join(d, sid + ".mid"))
            meta[f"orrery/{sid}"] = dict(plugin="orrery", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                         secs=round(dur, 1), peaks=peaks(loud), bpm=pr["params"].get("tempo", 60), kb=round(os.path.getsize(m4a) / 1024))
            keys.append(f"orrery/{sid}")
            print(f"orrery  {sid:18s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB")
    return keys

def peaks(wav_path, n=96):
    with wave.open(wav_path) as w:
        ch, sw, fr = w.getnchannels(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(fr)
    if sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    else:
        x = np.frombuffer(raw, dtype=np.int16) / 32768.0
    x = np.abs(x.reshape(-1, ch).mean(axis=1))
    seg = np.array_split(x, n)
    v = np.array([math.sqrt(float((s ** 2).mean())) if len(s) else 0.0 for s in seg])
    v = v / (v.max() + 1e-9)
    return [round(float(a), 3) for a in np.maximum(v, 0.04)]

# Murmur: un clip propio de la serie como fuente, pasado por un preset (murmurrender). Las descripciones dicen de
# dónde sale el sonido seco.
MURMUR = [
    ("vocal-cloud", "starling_cloud", "atfs1/formant-lead", [], "Vocal Cloud", "Nube de voz",
     "Starling Cloud on ATFS1 singing vowels: the phrase scattered into a flock.", "Starling Cloud sobre el ATFS1 cantando vocales: la frase dispersa en una bandada."),
    ("shimmer-choir", "choir_rising", "at101v/soft-pad", [], "Shimmer Choir", "Coro shimmer",
     "Choir Rising on an AT101V pad: feedback and octave grains that keep rising.", "Choir Rising sobre un pad del AT101V: feedback y granos una octava arriba que siguen subiendo."),
    ("stretched-chord", "stretch_ten_times", "at20v/glass-cathedral", ["--freeze-at", "4"], "Stretched Chord", "Acorde estirado",
     "Stretch Ten Times: Freeze at 4 s and Scan at 0.1 hold an AT20V chord and stretch it.", "Stretch Ten Times: Freeze a los 4 s y Scan en 0,1 retienen un acorde del AT20V y lo estiran."),
    ("stutter", "sixteenth_stutter", "at101v/acid-line", ["--bpm", "124"], "Sixteenth Stutter", "Tartamudeo en semicorcheas",
     "Grains on the sixteenth-note grid, jumping octaves, over an AT101V acid line.", "Granos en la grilla de semicorcheas, saltando de octava, sobre una línea ácida del AT101V."),
    ("octave-shower", "octave_shower", "at42/flute-lead", [], "Octave Shower", "Lluvia de octavas",
     "An AT42 flute line broken into grains spread over four octaves.", "Una línea de flauta del AT42 partida en granos repartidos en cuatro octavas."),
    ("tape-scatter", "tape_scatter", "at20v/hammond", [], "Tape Scatter", "Cinta dispersa",
     "Short grains from twelve seconds of an AT20V organ, half of them backwards.", "Granos cortos de doce segundos de un órgano del AT20V, la mitad al revés."),
    ("grain-chords", "play_the_cloud", "atfs1/formant-lead", ["--notes", "57+60+64:5,53+57+60:5,55+59+62:5,52+55+60:5"], "Chords of Grains", "Acordes de granos",
     "MIDI Pitch mode: four chords played into Murmur set the pitches of the grains.", "Modo MIDI Pitch: cuatro acordes tocados a Murmur fijan las alturas de los granos."),
]

def murmur_samples(meta):
    tool = os.path.join(ROOT, "build", "plugins", "murmur", "murmurrender_artefacts", "Release", "murmurrender")
    pdir = os.path.join(ROOT, "plugins", "murmur", "assets", "presets")
    d = os.path.join(OUT, "murmur"); os.makedirs(d, exist_ok=True)
    keys = []
    for sid, preset, source, extra, ten, tes, den, des in MURMUR:
        src = next(os.path.join(pdir, f) for f in sorted(os.listdir(pdir)) if f.split("_", 1)[1][:-5] == preset)
        with tempfile.TemporaryDirectory() as td:
            dry, raw, loud = os.path.join(td, "dry.wav"), os.path.join(td, "raw.wav"), os.path.join(td, "loud.wav")
            # La fuente se repite hasta cubrir los 40 s (los clips de los sintes duran 10 a 20 s).
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-stream_loop", "-1", "-i", os.path.join(OUT, source + ".m4a"), "-t", "40",
                            "-ar", "48000", "-ac", "2", "-c:a", "pcm_s24le", dry], check=True)
            r = subprocess.run([tool, src, dry, raw, "--secs", "40", "--tail", "6"] + extra, capture_output=True, text=True)
            if r.returncode != 0 or not os.path.exists(raw): raise SystemExit(f"falló murmurrender en {sid}: {r.stdout} {r.stderr}")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af",
                            "areverse,silenceremove=start_periods=1:start_threshold=-55dB,areverse,loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000",
                            "-c:a", "pcm_s24le", loud], check=True)      # sólo el silencio final: los huecos del ritmo quedan
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", loud], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", loud, "-af", f"afade=t=in:d=0.3,afade=t=out:st={max(0.0, dur - 2.5):.2f}:d=2.5",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            meta[f"murmur/{sid}"] = dict(plugin="murmur", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                         secs=round(dur, 1), peaks=peaks(loud), bpm=120, kb=round(os.path.getsize(m4a) / 1024), nomidi=True)
            keys.append(f"murmur/{sid}")
            print(f"murmur  {sid:18s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {r.stdout.strip()[:70]}")
    return keys

# Halation: un clip propio de la serie, seco, pasado por un preset de fábrica (halationrender). La fuente se repite
# hasta `secs` segundos y después se deja sonar la cola; `extra` lleva tempo y cambios en el tiempo (--at).
HALATION = [
    ("cathedral-tines", "Cathedral of Glass", "atfs1/tine-piano", 24, ["--bpm", "92", "--tail", "12"], "Cathedral of Glass", "Catedral de vidrio",
     "An ATFS1 electric piano in a huge bright hall, with a fifth of shimmer rising out of the tail.",
     "Un piano eléctrico del ATFS1 en una sala enorme y brillante, con una quinta de shimmer que sube desde la cola."),
    ("tape-flute", "Tape Echo 1974", "at42/flute-lead", 15, ["--bpm", "84", "--tail", "6"], "Tape Echo 1974", "Eco de cinta 1974",
     "An AT42 flute through a worn tape echo: the repeats wobble, saturate and darken.",
     "Una flauta del AT42 por un eco de cinta gastado: las repeticiones se ondulan, saturan y oscurecen."),
    ("ping-pong-arp", "Ping-Pong Machine", "at101v/arp-up", 18, ["--bpm", "120", "--tail", "5"], "Ping-Pong Machine", "Máquina de ping-pong",
     "An AT101V arpeggio with dotted-eighth echoes bouncing left and right (headphones help).",
     "Un arpegio del AT101V con ecos en corchea con puntillo que rebotan de izquierda a derecha (mejor con auriculares)."),
    ("rising-pad", "Rising Choir", "at101v/soft-pad", 33, ["--bpm", "70", "--tail", "14"], "Rising Choir", "Coro que sube",
     "An AT101V pad whose tail climbs an octave at a time, like a choir lifting off.",
     "Un pad del AT101V cuya cola sube de a una octava, como un coro que se eleva."),
    ("frozen-bells", "Slow Tide", "at700/ring-bells", 14, ["--bpm", "84", "--tail", "16", "--at", "9", "on_freeze=1"], "Frozen Bells", "Campanas congeladas",
     "AT700 bells in a slow, breathing hall; at 9 s Freeze holds the room and the bells hang in the air after the playing stops.",
     "Campanas del AT700 en una sala lenta que respira; a los 9 s Freeze retiene la sala y las campanas quedan suspendidas cuando ya no se toca."),
    ("reverse-voice", "Reverse Shimmer", "atfs1/formant-lead", 26, ["--bpm", "84", "--tail", "10"], "Reverse Shimmer", "Shimmer al revés",
     "ATFS1 singing vowels: the tail plays backwards an octave up and swells into each new phrase.",
     "El ATFS1 cantando vocales: la cola suena al revés una octava arriba y crece hacia cada frase nueva."),
    ("spectral-chord", "Spectral Ice", "at20v/glass-cathedral", 17, ["--bpm", "70", "--tail", "14"], "Spectral Ice", "Hielo espectral",
     "An AT20V chord smeared into a still, glassy cloud with its phases scattered.",
     "Un acorde del AT20V corrido en una nube quieta y vidriosa, con las fases dispersas."),
    ("tuned-bleeps", "Tuned Abyss", "at20v/random-bleeps", 19, ["--bpm", "120", "--tail", "12"], "Tuned Abyss", "Abismo afinado",
     "Random AT20V bleeps fall into a dark room tuned to a minor chord: the tail rings in key.",
     "Bleeps al azar del AT20V caen en una sala oscura afinada en un acorde menor: la cola suena en tono."),
]

# Tarnish: lo mismo, por sus máquinas.
TARNISH = [
    ("dusty-keys", "Dusty Cassette", "atfs1/tine-piano", 23, ["--tail", "1.5", "--bpm", "92"], "Dusty Cassette", "Casete polvorienta",
     "An ATFS1 electric piano on a used ferric cassette: wobble, hiss, a tilted head and dropouts.",
     "Un piano eléctrico del ATFS1 en una casete ferric usada: vaivén, soplido, cabeza inclinada y caídas."),
    ("warm-lead", "Warm Studio Tape", "at101v/classic-lead", 12, ["--tail", "1.5", "--bpm", "92"], "Warm Studio Tape", "Cinta de estudio tibia",
     "An AT101V lead through a studio machine at 15 ips: saturation, head bump, almost no wobble.",
     "Un lead del AT101V por una máquina de estudio a 15 ips: saturación, golpe de cabeza, casi nada de vaivén."),
    ("beat-tape-pad", "Lo-Fi Beat Tape", "at101v/soft-pad", 17, ["--tail", "1.5", "--bpm", "70"], "Lo-Fi Beat Tape", "Cinta de beats",
     "An AT101V pad on a soft, wobbly, saturated cassette.",
     "Un pad del AT101V en una casete blanda, con vaivén y saturada."),
    ("worn-vinyl-organ", "Worn Vinyl", "at20v/hammond", 21, ["--tail", "1.5", "--bpm", "96"], "Worn Vinyl", "Vinilo gastado",
     "An AT20V organ on a loved LP: crackle, clicks and two scratches that pop once per turn.",
     "Un órgano del AT20V en un LP muy escuchado: crujido, clics y dos rayas que suenan una vez por vuelta."),
    ("old-radio-voice", "Old Radio", "atfs1/formant-lead", 13, ["--tail", "1.5", "--bpm", "84"], "Old Radio", "Radio vieja",
     "ATFS1 singing vowels on an old AM set: a squeezed station, fading, static, a small speaker.",
     "El ATFS1 cantando vocales en una radio de AM vieja: emisora comprimida, desvanecimiento, estática, parlante chico."),
    ("mobile-voice", "Mobile Call Breakup", "atfs1/formant-lead", 13, ["--tail", "1.5", "--bpm", "84"], "Mobile Call Breakup", "Llamada que se corta",
     "The same voice on a weak mobile call: grit and frames that drop out in bursts.",
     "La misma voz en una llamada de celular débil: aspereza y tramas que se pierden en ráfagas."),
    ("broken-vhs-pad", "Broken VHS", "at20v/glass-cathedral", 17, ["--tail", "1.5", "--bpm", "70"], "Broken VHS", "VHS rota",
     "An AT20V chord from a rental tape that lost its tracking: servo wobble, hum, edge damage.",
     "Un acorde del AT20V de una cinta de videoclub que perdió el tracking: vaivén del servo, zumbido, bordes dañados."),
    ("tape-stop-acid", "Tape Stop Drop", "at101v/acid-line", 16, ["--tail", "1.5", "--bpm", "120", "--at", "6", "stop=1", "--at", "8.5", "stop=0"], "Tape Stop Drop", "Tape stop",
     "An AT101V acid line: at 6 s the tape stops in one bar and starts again.",
     "Una línea ácida del AT101V: a los 6 s la cinta se para en un compás y vuelve a arrancar."),
    ("half-speed-flute", "Half-Speed Dream", "at42/flute-lead", 15, ["--tail", "1.5", "--bpm", "84"], "Half-Speed Dream", "Sueño a media velocidad",
     "An AT42 flute on a chrome cassette slowed down an octave.",
     "Una flauta del AT42 en una casete chrome bajada una octava."),
]

def ride(pid, secs, lo, hi, period, step=0.25):
    """Automatización de una perilla en --at: un vaivén lento entre lo y hi, como se toca una línea ácida."""
    out = []
    t = 0.0
    while t < secs:
        v = lo + (hi - lo) * 0.5 * (1 - math.cos(2 * math.pi * t / period))
        out += ["--at", f"{t:.2f}", f"{pid}={v:.1f}"]; t += step
    return out

# AT303: (id, preset, segundos, bpm, extra, título en, título es, desc en, desc es). Cada preset toca su propio patrón.
AT303 = [
    ("squelch-line", "Squelch Line", 16, 132, ride("cutoff", 16, 25, 70, 16), "Squelch Line", "Squelch Line",
     "Its own pattern with the cutoff ridden by hand across sixteen seconds: the classic squelch.",
     "Su propio patrón con el corte movido a mano durante dieciséis segundos: el chillido clásico."),
    ("accent-climb", "Accent Climb", 12, 125, [], "Accent Climb", "Accent Climb",
     "Four accents in a row: each one charges the capacitor further, and the fourth screams.",
     "Cuatro acentos seguidos: cada uno carga más el capacitor, y el cuarto grita."),
    ("night-bus", "Night Bus", 16, 128, ride("resonance", 16, 55, 92, 8), "Night Bus", "Night Bus",
     "A rolling line with ties and slides; the resonance rises and falls.",
     "Una línea que rueda, con ligaduras y slides; la resonancia sube y baja."),
    ("wet-concrete", "Wet Concrete", 16, 126, ride("envmod", 16, 40, 90, 16), "Wet Concrete", "Wet Concrete",
     "Dark and wide, Env Mod opening up: deeper and more vocal at the same time.",
     "Oscuro y amplio, con Env Mod abriéndose: más grave y más vocal a la vez."),
    ("deep-root", "Deep Root", 12, 122, [], "Deep Root", "Deep Root",
     "Low and round: little resonance, the octave down, for under a track.",
     "Grave y redondo: poca resonancia, la octava de abajo, para debajo de un tema."),
    ("hot-box", "Hot Box", 12, 134, ride("cutoff", 12, 30, 60, 12), "Hot Box", "Hot Box",
     "Soft clip after the filter: thick, compressed and warm.",
     "Recorte suave después del filtro: grueso, comprimido y cálido."),
    ("triplet-roll", "Triplet Roll", 12, 120, [], "Triplet Roll", "Triplet Roll",
     "Three steps per beat: a twelve-step loop that rolls.",
     "Tres pasos por tiempo: un loop de doce pasos que rueda."),
    ("screamer", "Screamer", 12, 134, ride("cutoff", 12, 30, 65, 12), "Screamer", "Screamer",
     "The extended page: high resonance, a higher cutoff top and FM from the output.",
     "La página extendida: resonancia alta, el tope del corte más arriba y FM desde la salida."),
    ("dub-pulse", "Dub Pulse", 16, 118, [], "Dub Pulse", "Dub Pulse",
     "Sparse notes into the dotted ping-pong delay.",
     "Notas sueltas hacia el delay ping-pong con puntillo."),
]

def at303_samples(meta):
    """Clips del AT303: cada preset tocando su patrón con el reloj de un host, a −16 LUFS."""
    tool = os.path.join(ROOT, "build", "plugins", "at303", "at303render_artefacts", "Release", "at303render")
    d = os.path.join(OUT, "at303"); os.makedirs(d, exist_ok=True)
    keys = []
    for sid, preset, secs, bpm, extra, ten, tes, den, des in AT303:
        with tempfile.TemporaryDirectory() as td:
            raw, loud = os.path.join(td, "raw.wav"), os.path.join(td, "loud.wav")
            r = subprocess.run([tool, raw, "--preset", preset, "--seconds", str(secs + 1.5), "--bpm", str(bpm)] + extra, capture_output=True, text=True)
            if r.returncode != 0 or not os.path.exists(raw): raise SystemExit(f"falló at303render en {sid}: {r.stdout} {r.stderr}")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af",
                            f"atrim=0:{secs + 1.5},loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000", "-c:a", "pcm_s24le", loud], check=True)
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", loud], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", loud, "-af", f"afade=t=out:st={max(0.0, dur - 1.5):.2f}:d=1.5",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            meta[f"at303/{sid}"] = dict(plugin="at303", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                        secs=round(dur, 1), peaks=peaks(loud), bpm=bpm, kb=round(os.path.getsize(m4a) / 1024), nomidi=True)
            keys.append(f"at303/{sid}")
            print(f"at303 {sid:17s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {r.stdout.strip()[:70]}")
    return keys

# Plumb: la voz sintética propia de plugins/plumb/tools/demo_voice.py, cantando desafinada. Cada clip la pasa dos
# veces: primero como se cantó (Mix en 0: la voz seca, alineada) y después corregida por el preset, en La menor.
# (id, preset, melodía, extra, título en, título es, desc en, desc es)
PLUMB = [
    ("natural-lead", "Natural Lead", "lead", [], "Natural Lead", "Natural Lead",
     "A synthetic voice singing off pitch, first as sung, then through Natural Lead: the notes land, the vibrato and the scoops stay.",
     "Una voz sintética que canta desafinada, primero como se cantó y después por Natural Lead: las notas caen en su lugar, el vibrato y las entradas quedan."),
    ("hard-tune", "Hard Tune", "lead", [], "Hard Tune", "Hard Tune",
     "The same phrase, then Hard Tune: every note exactly on pitch, instant steps.",
     "La misma frase, después Hard Tune: cada nota exactamente afinada, saltos instantáneos."),
    ("soft-robot", "Soft Robot", "lead", [], "Soft Robot", "Soft Robot",
     "Hard steps with a short glide between the notes.",
     "Escalones duros con un deslizamiento corto entre las notas."),
    ("low-voice", "Low Voice", "low", [], "Low Voice", "Voz grave",
     "A low voice, from 110 Hz: the detector listens from 65 Hz.",
     "Una voz grave, desde 110 Hz: el detector escucha desde 65 Hz."),
    ("high-voice", "High Voice", "high", [], "High Voice", "Voz aguda",
     "A high voice, up to 880 Hz.",
     "Una voz aguda, hasta 880 Hz."),
    ("octave-down", "Octave Down", "lead", [], "Octave Down", "Octava abajo",
     "Corrected and one octave down with the formants kept: a second, deeper voice rather than a slowed-down one.",
     "Corregida y una octava abajo con los formantes en su lugar: una segunda voz más grave, no una voz ralentizada."),
    ("throat-down", "Throat Down", "lead", [], "Throat Down", "Garganta más larga",
     "Corrected, with the formants moved down three semitones: a darker, bigger voice at the same pitch.",
     "Corregida, con los formantes tres semitonos más abajo: una voz más oscura y más grande a la misma altura."),
]

def plumb_samples(meta):
    """Clips de Plumb: la frase dos veces (seca, después corregida), a −16 LUFS."""
    tool = os.path.join(ROOT, "build", "plugins", "plumb", "plumbrender_artefacts", "Release", "plumbrender")
    voice = os.path.join(ROOT, "plugins", "plumb", "tools", "demo_voice.py")
    d = os.path.join(OUT, "plumb"); os.makedirs(d, exist_ok=True)
    keys = []
    for sid, preset, melody, extra, ten, tes, den, des in PLUMB:
        with tempfile.TemporaryDirectory() as td:
            one, dry, raw, loud = (os.path.join(td, f) for f in ("one.wav", "dry.wav", "raw.wav", "loud.wav"))
            subprocess.run([sys.executable, voice, one, "--melody", melody], check=True, capture_output=True)
            with wave.open(one) as w: fs, x = w.getframerate(), np.frombuffer(w.readframes(w.getnframes()), "<i2")
            gap = np.zeros(int(0.8 * fs), "<i2")
            with wave.open(dry, "wb") as w:
                w.setnchannels(1); w.setsampwidth(2); w.setframerate(fs); w.writeframes(np.concatenate([x, gap, x]).tobytes())
            second = (len(x) + len(gap) * 0.5) / fs
            r = subprocess.run([tool, dry, raw, "--preset", preset, "--set", "key=9", "--set", "scale=2", "--set", "mix=0",
                                "--at", f"{second:.2f}", "mix=100", "--tail", "0.5"] + extra, capture_output=True, text=True)
            if r.returncode != 0 or not os.path.exists(raw): raise SystemExit(f"falló plumbrender en {sid}: {r.stdout} {r.stderr}")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af",
                            "areverse,silenceremove=start_periods=1:start_threshold=-60dB,areverse,loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000",
                            "-c:a", "pcm_s24le", loud], check=True)
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", loud], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", loud, "-af", f"afade=t=out:st={max(0.0, dur - 0.5):.2f}:d=0.5",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            meta[f"plumb/{sid}"] = dict(plugin="plumb", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                        secs=round(dur, 1), peaks=peaks(loud), bpm=100, kb=round(os.path.getsize(m4a) / 1024), nomidi=True,
                                        split=round(second, 2))
            keys.append(f"plumb/{sid}")
            print(f"plumb {sid:17s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {r.stdout.strip()[:70]}")
    return keys

# Pallor: los clips de su herramienta de demos (plugins/pallor/tools/demos.py, que ya los deja a −16 LUFS con un
# limitador de pico de ≤ 3 dB): un preset por género, dos recorridos y dos bocetos con nuestros otros instrumentos.
# (id, id del demo, preset, título en, título es, desc en, desc es)
PALLOR = [
    ("darkwave", "01_darkwave", "Night Shift Mercury", "Dark wave", "Dark wave",
     "Night Shift Mercury: bridged-T kick, gated 8-bit snare and chorused hats, its song with A, B and fills.",
     "Night Shift Mercury: bombo de puente en T, caja de 8 bits con gate y charles con chorus, su canción con A, B y fills."),
    ("coldwave", "02_coldwave", "Thin Ice Lines", "Coldwave", "Coldwave",
     "Thin Ice Lines: the hexagonal kit at 8 bits, flanged hats, a cold nonlinear room.",
     "Thin Ice Lines: el kit hexagonal a 8 bits, charles con flanger y una sala no lineal fría."),
    ("postpunk", "03_postpunk", "Floor Tom Sermon", "Post-punk", "Post-punk",
     "Floor Tom Sermon: tribal floor toms around the backbeat, room and tape, a fill every four bars.",
     "Floor Tom Sermon: toms de piso tribales alrededor del backbeat, sala y cinta, un fill cada cuatro compases."),
    ("ebm", "05_ebm", "Sweat Engine", "EBM", "EBM",
     "Sweat Engine: dark kick, clap backbeat, sixteenth hats and a clanking cowbell.",
     "Sweat Engine: bombo oscuro, palmas en el backbeat, charles en semicorcheas y un cencerro metálico."),
    ("synthpop", "06_synthpop", "Neon Promenade", "Synthpop", "Synthpop",
     "Neon Promenade: the 1982-style acoustic sample kit with claps and tambourine.",
     "Neon Promenade: el kit de muestras acústicas al estilo de 1982, con palmas y pandereta."),
    ("industrial", "08_industrial", "Broken Conveyor", "Industrial", "Industrial",
     "Broken Conveyor: the foundry kit, struck sheets, pipes and chains, through a jet flanger.",
     "Broken Conveyor: el kit de fundición, chapas, caños y cadenas golpeados, por un jet flanger."),
    ("italo", "10_italo", "Hi-Energy Ghost", "Italo and hi-NRG", "Italo y hi-NRG",
     "Hi-Energy Ghost: four on the floor, off-beat open hats and hexagonal toms going pew.",
     "Hi-Energy Ghost: bombo en negras, charles abiertos a contratiempo y toms hexagonales con su pew."),
    ("witchhouse", "11_witchhouse", "Triangle Static", "Witch house", "Witch house",
     "Triangle Static: 70 BPM, hat rolls by ratchet, sub drops, a reverse room and an 11 kHz crush.",
     "Triangle Static: 70 BPM, redobles de charles con ratchet, caídas sub, una sala invertida y reducción a 11 kHz."),
    ("gated-rooms", "14_gated_tour", "Night Shift Mercury", "Four 80s rooms", "Cuatro salas de los 80",
     "Kick and snare only, the snare dry, then through the gated, nonlinear and reverse rooms, four bars each.",
     "Sólo bombo y caja: la caja seca y después por la sala con gate, la no lineal y la invertida, cuatro compases cada una."),
    ("kit-tour", "15_kit_tour", "Neon Promenade", "Eight kits", "Ocho kits",
     "One pattern through the eight built-in digital kits, two bars each.",
     "Un mismo patrón por los ocho kits digitales incorporados, dos compases cada uno."),
    ("sketch-sodium", "20_sketch_sodium", "Night Shift Mercury", "Sketch: Sodium", "Boceto: Sodium",
     "A dark wave sketch in A minor: Pallor with a rubber bass from ATMD and a glass choir from ATFS1 through Halation, Tarnish and Lacquer on the mix.",
     "Un boceto de dark wave en La menor: Pallor con un bajo gomoso del ATMD y un coro de vidrio del ATFS1 por Halation, Tarnish y Lacquer en la mezcla."),
    ("sketch-steel", "21_sketch_steel", "Sweat Engine", "Sketch: Steel Parade", "Boceto: Steel Parade",
     "An EBM sketch in D minor: Pallor under a sixteenth-note squelch bass from ATMD and resonant brass from ATFS1.",
     "Un boceto de EBM en Re menor: Pallor bajo un bajo ácido en semicorcheas del ATMD y bronces resonantes del ATFS1."),
]
PYAUDIO = "/Volumes/Docs/Applications/pyaudio-venv/bin/python"

def pallor_samples(meta):
    """Clips de Pallor: renderiza los demos elegidos (ya a −16 LUFS) y los codifica en AAC con su forma de onda."""
    d = os.path.join(OUT, "pallor"); os.makedirs(d, exist_ok=True)
    keys = []
    with tempfile.TemporaryDirectory() as td:
        ids = ",".join(x[1] for x in PALLOR)
        subprocess.run([PYAUDIO, os.path.join(ROOT, "plugins", "pallor", "tools", "demos.py"), td, "--only", ids], check=True, capture_output=True)
        clips = json.load(open(os.path.join(td, "clips.json")))
        for sid, did, preset, ten, tes, den, des in PALLOR:
            wav = os.path.join(td, did + ".wav")
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", wav], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-af", f"afade=t=out:st={max(0.0, dur - 1.5):.2f}:d=1.5",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            bpm = int(round(float(subprocess.run([os.path.join(ROOT, "build", "plugins", "pallor", "pallorrender_artefacts", "Release", "pallorrender"), "--list"],
                                                 capture_output=True, text=True).stdout.split(preset + "  (")[1].split(" ")[0])))
            meta[f"pallor/{sid}"] = dict(plugin="pallor", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                         secs=round(dur, 1), peaks=peaks(wav), bpm=bpm, kb=round(os.path.getsize(m4a) / 1024), nomidi=True)
            keys.append(f"pallor/{sid}")
            print(f"pallor {sid:16s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {clips.get(did, {}).get('lufs')} LUFS")
    return keys

# Thrum: los clips de su herramienta de demos (plugins/thrum/tools/demos.py, que ya los deja a −16 LUFS con un limitador
# de pico donde hace falta), renderizados con build-guitar (thrumrender, y pallorrender y at101vrender para el dark wave).
# (id, id del demo, preset principal, título en, título es, desc en, desc es)
THRUM = [
    ("clean", "01_clean", "Glass Arpeggio", "Clean", "Limpio",
     "Glass Arpeggio picking the chords, and Clean Picked Lead on top: legato where the notes overlap, a late vibrato, a bend with the wheel.",
     "Glass Arpeggio punteando los acordes y Clean Picked Lead arriba: ligados donde las notas se pisan, un vibrato tardío, un bend con la rueda."),
    ("crunch-blues", "02_crunch_blues", "Texas Shuffle", "Crunch blues", "Blues con crunch",
     "A shuffle in A through a green overdrive and the small combo, and a lead with wheel bends and the bend-in keyswitch.",
     "Un shuffle en La por un overdrive verde y el combo chico, y un punteo con bends de la rueda y el keyswitch de bend de entrada."),
    ("high-gain", "03_high_gain", "Modern Chug", "High gain", "Alta ganancia",
     "The seven-string in drop tuning, palm-muted through the gate and the high-gain amp, and a legato lead with a pinch harmonic.",
     "La siete cuerdas en afinación drop, con palm mute por el gate y el amplificador de alta ganancia, y un punteo ligado con un pinch."),
    ("strummed-chords", "04_strummed_chords", "Sparkle Pop Strum", "Four strum patterns", "Cuatro patrones de rasgueo",
     "One progression per pattern from the library: pop, funk scratch with ghost notes, a reggae skank and a country boom-chuck, four bars each.",
     "Una progresión por patrón de la biblioteca: pop, rasgueo funk con ghost notes, un skank de reggae y un boom-chuck country, cuatro compases cada uno."),
    ("lead-bends", "05_lead_bends", "Blues Lead Bends", "Lead with bends", "Punteo con bends",
     "Whole, half, pre-bends and releases with the pitch wheel, slide-in and bend-in keyswitches, over chords from the small combo.",
     "Bends de un tono, de medio, pre-bends y vueltas con la rueda, keyswitches de slide y bend de entrada, sobre acordes del combo chico."),
    ("darkwave", "06_darkwave_pallor_at101v", "Night Drive Strum", "Dark wave", "Dark wave",
     "Night Drive Strum and Gothic Lead with chorus, echo and hall, over Pallor's Night Shift Mercury and an AT101V bass.",
     "Night Drive Strum y Gothic Lead con chorus, eco y hall, sobre Night Shift Mercury de Pallor y un bajo del AT101V."),
    ("fingerpicking", "07_fingerpicking", "Fingerstyle Semi", "Fingerpicking", "Fingerpicking",
     "Travis picking on the semi-hollow: the thumb alternates the bass while the fingers play on top.",
     "Travis picking en la semi-hollow: el pulgar alterna el bajo mientras los dedos tocan arriba."),
    ("ambient", "08_ambient_feedback", "Ambient Freeze", "Ambient and feedback", "Ambient y acople",
     "Fingers through tape echo and a long hall, and a held E that blooms into its octave as amplifier feedback.",
     "Dedos por un eco de cinta y un hall largo, y un Mi sostenido que se abre a su octava como acople del amplificador."),
]

def thrum_samples(meta):
    """Clips de Thrum: renderiza los demos (ya a −16 LUFS) y los codifica en AAC con su forma de onda."""
    d = os.path.join(OUT, "thrum"); os.makedirs(d, exist_ok=True)
    keys = []
    with tempfile.TemporaryDirectory() as td:
        ids = ",".join(x[1] for x in THRUM)
        subprocess.run([PYAUDIO, os.path.join(ROOT, "plugins", "thrum", "tools", "demos.py"), td, "--only", ids, "--build", os.path.join(ROOT, "build-guitar")], check=True, capture_output=True)
        index = json.load(open(os.path.join(td, "index.json")))
        for sid, did, preset, ten, tes, den, des in THRUM:
            wav = os.path.join(td, did + ".wav")
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", wav], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-af", f"afade=t=out:st={max(0.0, dur - 1.5):.2f}:d=1.5",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            meta[f"thrum/{sid}"] = dict(plugin="thrum", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                        secs=round(dur, 1), peaks=peaks(wav), bpm=index[did]["bpm"], kb=round(os.path.getsize(m4a) / 1024), nomidi=True)
            keys.append(f"thrum/{sid}")
            print(f"thrum {sid:16s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {index[did].get('lufs')} LUFS")
    return keys

def fx_samples(meta, plugin, items):
    """Clips de un efecto: la fuente propia repetida hasta `secs`, por un preset, a −16 LUFS."""
    tool = os.path.join(ROOT, "build", "plugins", plugin, f"{plugin}render_artefacts", "Release", f"{plugin}render")
    d = os.path.join(OUT, plugin); os.makedirs(d, exist_ok=True)
    keys = []
    for sid, preset, source, secs, extra, ten, tes, den, des in items:
        with tempfile.TemporaryDirectory() as td:
            dry, raw, loud = os.path.join(td, "dry.wav"), os.path.join(td, "raw.wav"), os.path.join(td, "loud.wav")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-stream_loop", "-1", "-i", os.path.join(OUT, source + ".m4a"), "-t", str(secs),
                            "-af", f"afade=t=out:st={secs - 0.4}:d=0.4", "-ar", "48000", "-ac", "2", "-c:a", "pcm_s24le", dry], check=True)
            r = subprocess.run([tool, dry, raw, "--preset", preset] + extra, capture_output=True, text=True)
            if r.returncode != 0 or not os.path.exists(raw): raise SystemExit(f"falló {plugin}render en {sid}: {r.stdout} {r.stderr}")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af",
                            "areverse,silenceremove=start_periods=1:start_threshold=-60dB,areverse,loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000",
                            "-c:a", "pcm_s24le", loud], check=True)      # sólo el silencio final: la cola larga queda
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", loud], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", loud, "-af", f"afade=t=out:st={max(0.0, dur - 3.0):.2f}:d=3.0",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            meta[f"{plugin}/{sid}"] = dict(plugin=plugin, id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                           secs=round(dur, 1), peaks=peaks(loud), bpm=int(extra[extra.index("--bpm") + 1]),
                                           kb=round(os.path.getsize(m4a) / 1024), nomidi=True)
            keys.append(f"{plugin}/{sid}")
            print(f"{plugin} {sid:17s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {r.stdout.strip()[:70]}")
    return keys

def main():
    only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
    meta_path = os.path.join(OUT, "samples.json")
    meta = json.load(open(meta_path)) if os.path.exists(meta_path) else {}
    orrery_keys = [k for k in meta.get("_order", []) if k.startswith("orrery/")]
    if only in (None, "orrery"): orrery_keys = orrery_samples(meta)
    murmur_keys = [k for k in meta.get("_order", []) if k.startswith("murmur/")]
    if only in (None, "murmur"): murmur_keys = murmur_samples(meta)
    halation_keys = [k for k in meta.get("_order", []) if k.startswith("halation/")]
    if only in (None, "halation"): halation_keys = fx_samples(meta, "halation", HALATION)
    tarnish_keys = [k for k in meta.get("_order", []) if k.startswith("tarnish/")]
    if only in (None, "tarnish"): tarnish_keys = fx_samples(meta, "tarnish", TARNISH)
    at303_keys = [k for k in meta.get("_order", []) if k.startswith("at303/")]
    if only in (None, "at303"): at303_keys = at303_samples(meta)
    plumb_keys = [k for k in meta.get("_order", []) if k.startswith("plumb/")]
    if only in (None, "plumb"): plumb_keys = plumb_samples(meta)
    pallor_keys = [k for k in meta.get("_order", []) if k.startswith("pallor/")]
    if only in (None, "pallor"): pallor_keys = pallor_samples(meta)
    thrum_keys = [k for k in meta.get("_order", []) if k.startswith("thrum/")]
    if only in (None, "thrum"): thrum_keys = thrum_samples(meta)
    for sid, plugin, preset, song, extra, ten, tes, den, des in SAMPLES:
        if only and plugin != only: continue
        d = os.path.join(OUT, plugin); os.makedirs(d, exist_ok=True)
        mid = os.path.join(d, sid + ".mid"); song.write(mid)
        with tempfile.TemporaryDirectory() as td:
            raw, loud = os.path.join(td, "raw.wav"), os.path.join(td, "loud.wav")
            r = subprocess.run([RENDER[plugin], find(plugin, preset), raw, "--midi", mid] + extra, capture_output=True, text=True)
            if r.returncode != 0 or not os.path.exists(raw): raise SystemExit(f"falló el render de {sid}: {r.stdout} {r.stderr}")
            # −16 LUFS integrados, pico real −1,5 dB; corta el silencio final y hace un fade de salida.
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af",
                            "silenceremove=stop_periods=-1:stop_duration=0.5:stop_threshold=-50dB,loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000",
                            "-c:a", "pcm_s24le", loud], check=True)
            dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", loud], capture_output=True, text=True).stdout)
            m4a = os.path.join(d, sid + ".m4a")
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", loud, "-af", f"afade=t=out:st={max(0.0, dur - 0.8):.2f}:d=0.8",
                            "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
            meta[f"{plugin}/{sid}"] = dict(plugin=plugin, id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                           secs=round(dur, 1), peaks=peaks(loud), bpm=song.bpm, kb=round(os.path.getsize(m4a) / 1024))
            print(f"{plugin:7s} {sid:18s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {r.stdout.strip()[:60]}")
    meta["_order"] = [f"{p}/{s}" for s, p, *_ in SAMPLES] + orrery_keys + murmur_keys + halation_keys + tarnish_keys + at303_keys + plumb_keys + pallor_keys + thrum_keys
    json.dump(meta, open(meta_path, "w"), indent=1, ensure_ascii=False)

if __name__ == "__main__":
    main()
