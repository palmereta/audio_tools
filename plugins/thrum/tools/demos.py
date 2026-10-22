#!/usr/bin/env python3
"""Demos de Thrum para escuchar antes de la interfaz final (fase 1).

    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/thrum/tools/demos.py OUTDIR [--only id,id] [--build build-guitar]

Cada demo es MIDI escrito acá (melodías, progresiones, bends con la rueda, keyswitches) tocado por presets de fábrica
con thrumrender; las que tienen varias partes (base y punteo) se mezclan, y la de dark wave suma Pallor (pallorrender) y
el AT101V (at101vrender). Todo a −16 LUFS (BS.1770) con una ganancia y, si el pico pasara de −1 dBFS, un limitador de
pico con anticipación. Escribe WAV (24 bits), MP3 (LAME V2) e index.md con lo medido.
Necesita numpy, soundfile, pyloudnorm, scipy, ffmpeg y los render compilados:
    cmake --build build-guitar --target thrumrender pallorrender at101vrender
"""
import json, os, struct, subprocess, sys, tempfile
import numpy as np, soundfile as sf, pyloudnorm as pyln

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SR = 48000
TARGET = -16.0


def args():
    a = sys.argv[1:]
    out = a[0] if a and not a[0].startswith("--") else os.path.join(ROOT, "..", "audiotools-private", "thrum", "demos")
    only = None; build = os.path.join(ROOT, "build-guitar")
    for i, x in enumerate(a):
        if x == "--only": only = set(a[i + 1].split(","))
        if x == "--build": build = a[i + 1]
    return os.path.abspath(out), only, build


OUT, ONLY, BUILD = args()
TR = os.path.join(BUILD, "plugins", "thrum", "thrumrender_artefacts", "Release", "thrumrender")
VR = os.path.join(BUILD, "plugins", "pallor", "pallorrender_artefacts", "Release", "pallorrender")
AR = os.path.join(BUILD, "plugins", "at101v", "at101vrender_artefacts", "Release", "at101vrender")


def run(cmd, cwd=None):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)
    if r.returncode != 0:
        raise RuntimeError(" ".join(cmd) + "\n" + r.stdout + r.stderr)
    return r.stdout


# ── MIDI (SMF tipo 0, 480 ticks por negra) ──
NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}
def m(name):
    i = 2 if len(name) > 2 and name[1] in "#b" else 1
    return NOTE[name[:i]] + 12 * (int(name[i:]) + 1)

def vlq(n):
    out = [n & 0x7F]; n >>= 7
    while n: out.insert(0, (n & 0x7F) | 0x80); n >>= 7
    return bytes(out)

class Song:
    def __init__(self, bpm): self.bpm = bpm; self.ev = []
    def note(self, beat, dur, n, vel=96, ch=1):
        n = m(n) if isinstance(n, str) else n
        self.ev.append((beat, 2, bytes([0x90 | (ch - 1), n, vel]))); self.ev.append((beat + dur, 0, bytes([0x80 | (ch - 1), n, 0])))
    def chord(self, beat, dur, notes, vel=96):
        for n in notes: self.note(beat, dur, n, vel)
    def ks(self, beat, k, base=12):                     # keyswitch (articulación): k = 0 sustain, 1 palm, 3 muerta, 7 bend in…
        self.note(beat, 0.05, base + k, 100)
    def bend(self, beat, semis, rng=2.0):                # rueda de pitch bend (rango del preset, 2 semitonos por defecto)
        v = int(round(8192 + max(-1.0, min(1.0, semis / rng)) * (8191 if semis > 0 else 8192)))
        v = max(0, min(16383, v))
        self.ev.append((beat, 1, bytes([0xE0, v & 127, v >> 7])))
    def bend_curve(self, b0, b1, s0, s1, steps=12, rng=2.0):
        for i in range(steps + 1):
            u = i / steps; u = u * u * (3 - 2 * u)
            self.bend(b0 + (b1 - b0) * i / steps, s0 + (s1 - s0) * u, rng)
    def cc(self, beat, num, val): self.ev.append((beat, 1, bytes([0xB0, num, val])))
    def write(self, path):
        data = bytearray(b"\x00\xFF\x51\x03" + struct.pack(">I", int(60_000_000 / self.bpm))[1:])
        last = 0
        for beat, _, msg in sorted(self.ev, key=lambda e: (e[0], e[1])):
            t = int(round(beat * 480)); data += vlq(t - last) + msg; last = t
        data += b"\x00\xFF\x2F\x00"
        with open(path, "wb") as f:
            f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(data)) + data)

CHORD = {
    "": [0, 4, 7], "m": [0, 3, 7], "7": [0, 4, 7, 10], "maj7": [0, 4, 7, 11], "m7": [0, 3, 7, 10], "5": [0, 7], "9": [0, 4, 7, 10, 14], "sus4": [0, 5, 7],
}
def chord_notes(name, octave=3):
    i = 2 if len(name) > 1 and name[1] in "#b" else 1
    root = NOTE[name[:i]] + 12 * (octave + 1); q = name[i:]
    return [root + iv for iv in CHORD[q]]

def progression(song, chords, beats_per=4, start=0, vel=100, octave=3):
    for k, c in enumerate(chords):
        song.chord(start + k * beats_per, beats_per - 0.02, chord_notes(c, octave), vel)


# ── Render ──
def thrum(tmp, name, preset, song, extra=(), tail=3.0):
    mid = os.path.join(tmp, name + ".mid"); wav = os.path.join(tmp, name + ".wav")
    song.write(mid)
    run([TR, wav, "--preset", preset, "--midi", mid, "--bpm", str(song.bpm), "--tail", str(tail), *extra])
    return load(wav)

def load(path, n=None):
    x, sr = sf.read(path, always_2d=True)
    assert sr == SR, (path, sr)
    if x.shape[1] == 1: x = np.repeat(x, 2, axis=1)
    if n is not None: x = x[:n] if len(x) >= n else np.vstack([x, np.zeros((n - len(x), 2))])
    return x

def lufs_of(x): return pyln.Meter(SR).integrated_loudness(x)
def at_lufs(x, target):
    l = lufs_of(x)
    return x * 10 ** ((target - l) / 20) if np.isfinite(l) else x

def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros((n, 2))
    for p in parts: out[: len(p)] += p
    return out

def limit(x, ceiling, sr):
    """Limitador de pico con 3 ms de anticipación y 80 ms de liberación (ganancia por muestra, enlazada)."""
    from scipy.ndimage import minimum_filter1d
    la, rel = int(0.003 * sr), np.exp(-1.0 / (0.08 * sr))
    need = np.minimum(1.0, ceiling / np.maximum(np.max(np.abs(x), axis=1), 1e-12))
    g_target = minimum_filter1d(need, size=2 * la + 1, origin=-la if la else 0)
    g = np.empty_like(g_target); cur = 1.0
    for i, t in enumerate(g_target):
        cur = t if t < cur else t + rel * (cur - t)
        g[i] = cur
    k = np.ones(la) / la if la > 1 else np.ones(1)
    gp = np.pad(g, (len(k) // 2, len(k) - 1 - len(k) // 2), mode="edge")
    g = np.minimum(g, np.convolve(gp, k, mode="valid"))
    return x * g[:, None], float(20 * np.log10(g.min()))

def finish(x, base, maxLimit=3.0):
    meter = pyln.Meter(SR)
    lufs0 = meter.integrated_loudness(x)
    gain, gr, y = TARGET - lufs0, 0.0, x
    for _ in range(4):
        y = x * 10 ** (gain / 20); gr = 0.0
        if np.abs(y).max() > 10 ** (-1 / 20): y, gr = limit(y, 10 ** (-1.05 / 20), SR)
        err = TARGET - meter.integrated_loudness(y)
        if abs(err) < 0.05: break
        gain += err
    while gr < -maxLimit:
        gain -= 0.5
        y = x * 10 ** (gain / 20)
        y, gr = limit(y, 10 ** (-1.05 / 20), SR) if np.abs(y).max() > 10 ** (-1 / 20) else (y, 0.0)
    # Fundido de salida corto (el final de las colas).
    f = min(len(y), int(0.5 * SR)); y[-f:] *= np.linspace(1, 0, f)[:, None]
    lufs = meter.integrated_loudness(y); peak = 20 * np.log10(np.abs(y).max())
    sf.write(base + ".wav", y, SR, subtype="PCM_24")
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", base + ".wav", "-codec:a", "libmp3lame", "-q:a", "2", base + ".mp3"])
    return {"lufs": round(float(lufs), 1), "peak": round(float(peak), 1), "limit": round(float(gr), 1), "secs": round(len(y) / SR, 1)}


# ── Las demos ──
def demo_clean(tmp):
    bpm = 96
    prog = ["Cmaj7", "Am7", "Fmaj7", "G"] * 4
    rh = Song(bpm); progression(rh, prog, 4, 0, 92)
    a = thrum(tmp, "clean_arp", "Glass Arpeggio", rh)
    ld = Song(bpm); b = 16.0
    # Una melodía cantable: ligados donde las notas se pisan, una nota larga con vibrato, un bend de medio tono.
    line = [("E5", 0, 2), ("D5", 2, 1), ("C5", 3, 1), ("A4", 4, 3), ("G4", 7, 1), ("A4", 8, 1), ("C5", 9, 1), ("D5", 10, 1), ("E5", 11, 1), ("D5", 12, 4),
            ("G5", 16, 1.5), ("E5", 17.5, 0.5), ("D5", 18, 1), ("C5", 19, 1), ("D5", 20, 1), ("E5", 21, 1), ("C5", 22, 1), ("A4", 23, 1),
            ("F5", 24, 1), ("E5", 25, 1), ("D5", 26, 1), ("C5", 27, 1), ("D5", 28, 4),
            ("E5", 32, 1), ("G5", 33, 1), ("A5", 34, 2), ("G5", 36, 1), ("E5", 37, 1), ("D5", 38, 2), ("C5", 40, 1), ("D5", 41, 1), ("E5", 42, 2), ("C5", 44, 4)]
    for i, (n, s, d) in enumerate(line):
        legato = 0.08 if i % 3 == 1 else -0.05            # algunas se pisan (ligado), otras se separan
        ld.note(b + s, d + legato, n, 88 + (i * 7) % 20)
    ld.bend_curve(b + 13.0, b + 13.4, 0, 1.0); ld.bend_curve(b + 14.5, b + 15.0, 1.0, 0)
    l = thrum(tmp, "clean_lead", "Clean Picked Lead", ld)
    return mix(at_lufs(a, -22), at_lufs(l, -21)), "Clean: picked chords and a lead", "Glass Arpeggio (fingerpicking, Ballad arpeggio) + Clean Picked Lead (melody with legato, vibrato and a pitch-wheel bend)", bpm

def demo_crunch(tmp):
    bpm = 100
    twelve = ["A7", "A7", "A7", "A7", "D7", "D7", "A7", "A7", "E7", "D7", "A7", "E7"]
    rh = Song(bpm); progression(rh, twelve + ["A7"], 4, 0, 96, octave=2)
    r = thrum(tmp, "crunch_rhythm", "Texas Shuffle", rh)
    ld = Song(bpm); b = 8.0
    # Pentatónica de La: bends de un tono con la rueda, notas que entran bendeadas (keyswitch "bend in"), vibrato.
    ph = [("C5", 0, 0.5), ("A4", 0.5, 0.5), ("C5", 1, 0.5), ("D5", 1.5, 2.5), ("E5", 4, 0.75), ("D5", 4.75, 0.25), ("C5", 5, 0.5), ("A4", 5.5, 2.5),
          ("G4", 8, 0.5), ("A4", 8.5, 0.5), ("C5", 9, 1), ("A4", 10, 2), ("E5", 16, 0.5), ("G5", 16.5, 0.5), ("A5", 17, 3)]
    for i, (n, s, d) in enumerate(ph): ld.note(b + s, d, n, 100 + (i * 5) % 20)
    ld.bend_curve(b + 1.6, b + 2.2, 0, 2.0); ld.bend_curve(b + 3.2, b + 3.8, 2.0, 0)          # D5 → E5 y vuelta
    ld.ks(b + 11.9, 7)                                                                      # bend in desde acá
    for i, (n, s, d) in enumerate([("A5", 12, 1.5), ("G5", 13.5, 0.5), ("E5", 14, 2)]): ld.note(b + s, d, n, 108)
    ld.ks(b + 15.9, 0)
    ld.bend_curve(b + 17.5, b + 18.2, 0, 1.0); ld.bend_curve(b + 19.5, b + 20.0, 1.0, 0)
    ph2 = [("C6", 22, 0.5), ("A5", 22.5, 0.5), ("G5", 23, 0.5), ("E5", 23.5, 0.5), ("D5", 24, 0.5), ("C5", 24.5, 0.5), ("A4", 25, 3)]
    for i, (n, s, d) in enumerate(ph2): ld.note(b + s, d, n, 104)
    ld.bend_curve(b + 25.5, b + 26.3, 0, 0.5); ld.bend_curve(b + 27.3, b + 27.8, 0.5, 0)
    for k, (n, s, d) in enumerate([("E5", 32, 0.5), ("D5", 32.5, 0.5), ("C5", 33, 0.5), ("A4", 33.5, 0.5), ("C5", 34, 1), ("A4", 35, 4)]): ld.note(b + s, d, n, 106)
    l = thrum(tmp, "crunch_lead", "Blues Lead Bends", ld, ["--set", "p_art=0"])
    return mix(at_lufs(r, -22), at_lufs(l, -19.5)), "Crunch: a shuffle blues with a bending lead", "Texas Shuffle (green overdrive into the small combo, shuffle strum) + Blues Lead Bends (crunch amp, wheel bends, bend-in keyswitch, vibrato)", bpm

def demo_high(tmp):
    bpm = 140
    rh = Song(bpm); prog = ["A5", "A5", "C5", "G5", "A5", "A5", "F5", "G5"] * 2
    for k, c in enumerate(prog):
        root = NOTE[c[0]] + 12 * 3
        rh.chord(k * 4, 3.96, [root - 12 if root > 48 else root, root + 7 - (12 if root > 48 else 0)], 110)
    r = thrum(tmp, "high_rhythm", "Modern Chug", rh)
    ld = Song(bpm); b = 32.0
    # Corrida en semicorcheas con ligados (las notas se pisan: hammer-on/pull-off), un pinch fuerte y un bend final.
    run_notes = ["A4", "B4", "C5", "B4", "A4", "G4", "A4", "C5", "D5", "E5", "D5", "C5", "D5", "E5", "G5", "A5"]
    for i, n in enumerate(run_notes * 2): ld.note(b + i * 0.25, 0.3, n, 96 + (i % 4 == 0) * 20)
    ld.note(b + 8, 2.0, "E5", 120)                                                         # pinch (velocity ≥ 112)
    ld.note(b + 10, 1.0, "D5", 100); ld.note(b + 11, 1.0, "C5", 100)
    for i, n in enumerate(["A5", "G5", "E5", "D5", "E5", "G5", "A5", "C6"]): ld.note(b + 12 + i * 0.5, 0.55, n, 104)
    ld.note(b + 16, 8.0, "B5", 112)
    ld.bend_curve(b + 16.5, b + 17.2, 0, 1.0); ld.bend_curve(b + 22.0, b + 23.0, 1.0, 0)
    l = thrum(tmp, "high_lead", "Metal Lead", ld)
    return mix(at_lufs(r, -20.5), at_lufs(l, -20)), "High gain: palm-muted riff and a legato lead", "Modern Chug (seven-string, drop tuning, gate, muted-chug pattern) + Metal Lead (hammer-on legato, pinch on hard notes, wheel bend)", bpm

def demo_strum(tmp):
    bpm = 100
    parts = [("Sparkle Pop Strum", ["G", "D", "Em", "C"]), ("Funk Scratch", ["E9", "A9", "E9", "A9"]), ("Reggae Skank", ["Am", "Dm", "Em", "Am"]), ("Boom Chuck", ["G", "C", "D", "G"])]
    out = []
    for name, chords in parts:
        s = Song(bpm); progression(s, chords + chords[:2] + chords[2:], 4, 0, 100)
        out.append((name, at_lufs(thrum(tmp, "strum_" + name.replace(" ", "_"), name, s, tail=1.5), -20)))
    seg = int(4 * 4 * 60 / bpm * SR)                                                       # 4 compases por sección
    x = np.zeros((seg * len(out) + int(2 * SR), 2))
    fade = int(0.4 * SR)
    for i, (_, y) in enumerate(out):
        y = y[: seg + fade].copy()
        y[-fade:] *= np.linspace(1, 0, fade)[:, None]
        x[i * seg: i * seg + len(y)] += y
    return x, "Strummed chords: four patterns from the library", " → ".join(n for n, _ in out) + " (pop island, funk scratch with ghost notes, reggae one drop, country train beat), four bars each", bpm

def demo_bends(tmp):
    bpm = 84
    rh = Song(bpm); progression(rh, ["Am", "F", "C", "G"] * 2, 4, 0, 84)
    r = thrum(tmp, "bends_backing", "Small Combo Breakup", rh)
    ld = Song(bpm)
    # Bends de distinta forma: tono entero lento, medio tono rápido, pre-bend que baja, bend y vibrato, slide de entrada.
    ld.note(0, 1.5, "E5", 100); ld.bend_curve(0.3, 1.0, 0, 2.0); ld.note(1.5, 0.5, "D5", 96); ld.bend_curve(1.45, 1.5, 2.0, 0, 2)
    ld.note(2, 1, "C5", 96); ld.note(3, 1, "A4", 100)
    ld.note(4, 2.5, "C5", 104); ld.bend_curve(4.2, 4.5, 0, 1.0); ld.bend_curve(6.0, 6.4, 1.0, 0)
    ld.note(6.5, 0.5, "A4", 92); ld.note(7, 1, "G4", 96)
    ld.bend(7.9, 2.0); ld.note(8, 2, "D5", 104); ld.bend_curve(8.4, 9.4, 2.0, 0)             # pre-bend que baja
    ld.note(10, 0.5, "C5", 96); ld.note(10.5, 0.5, "A4", 96); ld.note(11, 1, "C5", 100)
    ld.ks(11.9, 8)                                                                          # slide de entrada
    ld.note(12, 1, "E5", 104); ld.ks(12.95, 0)
    ld.note(13, 3, "G5", 110); ld.bend_curve(13.3, 13.8, 0, 2.0)
    ld.note(16, 0.5, "A5", 104); ld.note(16.5, 0.5, "G5", 100); ld.note(17, 0.5, "E5", 100); ld.note(17.5, 0.5, "D5", 100)
    ld.note(18, 2, "E5", 106); ld.bend_curve(18.2, 18.6, 0, 1.0); ld.bend_curve(19.6, 20.0, 1.0, 0)
    ld.note(20, 1, "C5", 100); ld.note(21, 0.5, "D5", 100); ld.note(21.5, 0.5, "C5", 96); ld.note(22, 2, "A4", 100)
    ld.ks(23.9, 7)                                                                          # bend in
    ld.note(24, 2, "C5", 108); ld.note(26, 2, "D5", 108); ld.ks(27.9, 0)
    ld.note(28, 4, "E5", 112); ld.bend_curve(28.3, 29.0, 0, 2.0)
    l = thrum(tmp, "bends_lead", "Blues Lead Bends", ld, ["--set", "p_art=0"])
    return mix(at_lufs(r, -24), at_lufs(l, -19)), "Lead with bends", "Blues Lead Bends over Small Combo Breakup chords: wheel bends (whole, half, pre-bend and release), bend-in and slide-in keyswitches, late vibrato", bpm

def demo_darkwave(tmp):
    bpm = 120; bars = 16
    vw = os.path.join(tmp, "pallor.wav")
    run([VR, vw, "--preset", "Night Shift Mercury", "--bars", str(bars), "--bpm", str(bpm), "--tail", "2.5"])
    drums = load(vw)
    # AT101V: bajo en corcheas sobre Em C G D.
    roots = {"Em": 40, "C": 36, "G": 43, "D": 38}
    eighth = 60 / bpm / 2
    seq = []
    for k in range(bars):
        r0 = roots[["Em", "C", "G", "D"][k % 4]]
        for i in range(8): seq.append(f"{r0 + (12 if i in (3, 6) else 0)}:{eighth:.4f}")
    bw = os.path.join(tmp, "bass.wav")
    preset = os.path.join(ROOT, "plugins", "at101v", "assets", "presets", "02_pulse_bass.json")
    run([AR, preset, bw, "--seq", ",".join(seq), "--bpm", str(bpm), "--secs", str(bars * 4 * 60 / bpm + 2)])
    bass = load(bw)
    rh = Song(bpm); progression(rh, ["Em", "C", "G", "D"] * (bars // 4), 4, 0, 96)
    g1 = thrum(tmp, "dw_rhythm", "Night Drive Strum", rh)
    ld = Song(bpm); b = 32.0
    mel = [("B4", 0, 3), ("G4", 3, 1), ("C5", 4, 3), ("B4", 7, 1), ("D5", 8, 2), ("B4", 10, 2), ("A4", 12, 4),
           ("E5", 16, 3), ("D5", 19, 1), ("C5", 20, 2), ("B4", 22, 2), ("G4", 24, 2), ("A4", 26, 2), ("B4", 28, 4)]
    for i, (n, s, d) in enumerate(mel): ld.note(b + s, d - 0.05, n, 96)
    g2 = thrum(tmp, "dw_lead", "Gothic Lead", ld)
    x = mix(at_lufs(drums, -19), at_lufs(bass, -21.5), at_lufs(g1, -22), at_lufs(g2, -22.5))
    return x, "Dark wave with Pallor and the AT101V", "Pallor 'Night Shift Mercury' (drums) + AT101V 'Pulse Bass' (eighth-note bass) + Thrum 'Night Drive Strum' (chorus and echo, triads) and 'Gothic Lead' (flanger, tape echo, hall) from bar 9", bpm

def demo_fingerpick(tmp):
    bpm = 92
    s = Song(bpm); progression(s, ["C", "Am", "F", "G7"] * 3, 4, 0, 90)
    return thrum(tmp, "fingerpick", "Fingerstyle Semi", s), "Fingerpicking (extra)", "Fingerstyle Semi: Travis picking (thumb alternates the bass, fingers on top), semi-hollow neck pickup", bpm

def demo_ambient(tmp):
    bpm = 72
    s = Song(bpm); progression(s, ["Cmaj7", "Em7", "Fmaj7", "G"] * 2, 4, 0, 80)
    pad = thrum(tmp, "ambient_pad", "Ambient Freeze", s, tail=8)
    fb = Song(bpm); fb.note(16, 14, "E4", 100); fb.note(30, 2, "D4", 90)
    drone = thrum(tmp, "ambient_fb", "Feedback Drone", fb, tail=4)
    return mix(at_lufs(pad, -20), at_lufs(drone, -23)), "Ambient and feedback (extra)", "Ambient Freeze (fingers, tape echo, long hall) with Feedback Drone: a held E that blooms into its octave", bpm

DEMOS = [
    ("01_clean", demo_clean), ("02_crunch_blues", demo_crunch), ("03_high_gain", demo_high), ("04_strummed_chords", demo_strum),
    ("05_lead_bends", demo_bends), ("06_darkwave_pallor_at101v", demo_darkwave), ("07_fingerpicking", demo_fingerpick), ("08_ambient_feedback", demo_ambient),
]


def main():
    os.makedirs(OUT, exist_ok=True)
    idx_path = os.path.join(OUT, "index.json")
    index = json.load(open(idx_path)) if os.path.exists(idx_path) else {}
    with tempfile.TemporaryDirectory() as tmp:
        for name, fn in DEMOS:
            if ONLY and name not in ONLY: continue
            x, title, what, bpm = fn(tmp)
            # Los rasgueos limpios son muy picudos (factor de cresta ~20 dB, como una guitarra real sin compresor): se deja
            # limitar hasta 8 dB para llegar a −16 LUFS.
            info = finish(x, os.path.join(OUT, name), 8.0 if name == "04_strummed_chords" else 3.0)
            info.update({"title": title, "what": what, "bpm": bpm})
            index[name] = info
            print(f"{name}: {info['secs']} s, {info['lufs']} LUFS, pico {info['peak']} dBFS, limitador {info['limit']} dB")
    json.dump(index, open(idx_path, "w"), indent=1, ensure_ascii=False)
    lines = ["# Thrum: demos for listening (phase 1)", "",
             "Rendered offline with `thrumrender` (and `pallorrender` / `at101vrender` for the dark wave clip) from MIDI written in",
             "`plugins/thrum/tools/demos.py`, every part played by a factory preset. Loudness −16 LUFS integrated (BS.1770),",
             "peaks under −1 dBFS (a look-ahead peak limiter only where the peak would pass; the column says how much).",
             "WAV 24-bit 48 kHz and MP3 (LAME V2). Pallor is still in development: its renderer was built from the working",
             "tree of the day (and the AT101V renderer from the released code), both in `build-guitar`.", "",
             "| File | What it is | Presets | BPM | Length | LUFS | Peak | Limiter |", "|---|---|---|---|---|---|---|---|"]
    for name, _ in DEMOS:
        if name not in index: continue
        i = index[name]
        lines.append(f"| `{name}.wav` / `.mp3` | {i['title']} | {i['what']} | {i['bpm']} | {i['secs']} s | {i['lufs']} | {i['peak']} dBFS | {i['limit']} dB |")
    lines += ["", "What to listen for: the attack and decay of single notes (no two picks identical), the strum spread and the",
              "up-strokes hitting fewer strings, legato and slides without re-picking, the bend curves and the late vibrato,",
              "the hand noises (pick, finger squeak on position shifts, release), the amp from clean to high gain, the cabinet."]
    open(os.path.join(OUT, "index.md"), "w").write("\n".join(lines) + "\n")
    print(os.path.join(OUT, "index.md"))

if __name__ == "__main__":
    main()
