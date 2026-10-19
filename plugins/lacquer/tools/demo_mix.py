#!/usr/bin/env python3
"""Mezcla de demostración de Lacquer, hecha sólo con instrumentos propios (nada de samples ajenos).

Escribe los MIDI de cada parte, los renderiza con los renders de los plugins (batería y bajo del ATMD, cuerdas del
AT700, piano del ATFS1) y los mezcla como una mezcla casera sin masterizar: unos −20 LUFS, picos cerca de −6 dBFS,
algo opaca y con una intro sin batería. Es el material de las capturas del manual y de los A/B de la web.

    python3 plugins/lacquer/tools/demo_mix.py [carpeta]     (por defecto build/lacquer-demo)
"""
import os, struct, subprocess, sys, wave
import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(ROOT, "build", "lacquer-demo")
SR = 48000
BPM = 104
BEAT = 60.0 / BPM
BARS = 16


def render_bin(name):
    for d in ("build/plugins/{0}/{0}render_artefacts/Release/{0}render", "build/plugins/{0}/{0}render_artefacts/{0}render"):
        p = os.path.join(ROOT, d.format(name))
        if os.path.exists(p):
            return p
    sys.exit("missing renderer: " + name)


def write_midi(path, notes):
    """notes: (beat, dur_beats, note, vel). Formato 0, 480 ticks por negra, tempo BPM."""
    tpq = 480
    ev = []
    for b, d, n, v in notes:
        ev.append((int(round(b * tpq)), 1, n, v))
        ev.append((int(round((b + d) * tpq)), 0, n, 0))
    ev.sort(key=lambda e: (e[0], e[1]))
    data = bytearray()

    def vlq(x):
        out = [x & 0x7F]
        x >>= 7
        while x:
            out.insert(0, (x & 0x7F) | 0x80)
            x >>= 7
        return bytes(out)

    tempo = int(60_000_000 / BPM)
    data += vlq(0) + bytes([0xFF, 0x51, 3]) + tempo.to_bytes(3, "big")
    t = 0
    for tick, on, n, v in ev:
        data += vlq(tick - t) + bytes([0x90 if on else 0x80, n, v])
        t = tick
    data += vlq(0) + bytes([0xFF, 0x2F, 0])
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, tpq) + b"MTrk" + struct.pack(">I", len(data)) + data)


def read_wav(path):
    with wave.open(path) as w:
        n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(n)
    if sw == 2:
        x = np.frombuffer(raw, "<i2").astype(np.float64) / 32768.0
    elif sw == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) << 8 | b[:, 1].astype(np.int32) << 16 | b[:, 2].astype(np.int32) << 24) >> 8) / 8388608.0
    else:
        x = np.frombuffer(raw, "<f4").astype(np.float64)
    x = x.reshape(-1, ch)
    return x if ch == 2 else np.repeat(x, 2, axis=1)


def write_wav(path, x):
    y = np.clip(np.round(x * 8388607.0), -8388608, 8388607).astype(np.int32)
    b = np.zeros((y.size, 3), np.uint8)
    f = y.reshape(-1)
    b[:, 0] = f & 0xFF; b[:, 1] = (f >> 8) & 0xFF; b[:, 2] = (f >> 16) & 0xFF
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(3); w.setframerate(SR)
        w.writeframes(b.tobytes())


def part(name, plugin, preset, notes, extra=()):
    mid = os.path.join(OUT, name + ".mid")
    wav = os.path.join(OUT, name + ".wav")
    write_midi(mid, notes)
    secs = BARS * 4 * BEAT + 3.0
    cmd = [render_bin(plugin), os.path.join(ROOT, "plugins", plugin, "assets", "presets", preset), wav, "--midi", mid, "--secs", "%.2f" % secs, "--sr", str(SR), *extra]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    return read_wav(wav)


def main():
    os.makedirs(OUT, exist_ok=True)
    intro = 4                                    # compases sin batería
    kick, snare, hat, bass, chords, keys = [], [], [], [], [], []
    prog = [(45, [57, 60, 64]), (41, [57, 60, 65]), (48, [55, 60, 64]), (43, [55, 59, 62])]   # Am F C G
    for bar in range(BARS):
        root, ch = prog[bar % 4]
        b0 = bar * 4
        chords.append((b0, 3.9, ch[0], 80)); chords.append((b0, 3.9, ch[1], 76)); chords.append((b0, 3.9, ch[2], 76))
        if bar >= intro:
            for k in (0, 2.5): kick.append((b0 + k, 0.4, 36, 120))
            if bar % 2: kick.append((b0 + 3.75, 0.2, 36, 100))
            for s in (1, 3): snare.append((b0 + s, 0.3, 38, 115))
            for h in range(8): hat.append((b0 + h * 0.5, 0.2, 42, 96 if h % 2 == 0 else 70))
            line = [(0, root), (1.5, root), (2, root + 12), (2.5, root), (3, root + 7)]
            for t, n in line: bass.append((b0 + t, 0.45, n - 12, 110))
        mel = [(0, 76), (1, 74), (1.5, 72), (2, 74), (3, 72)] if bar % 2 == 0 else [(0, 69), (0.5, 72), (1.5, 71), (2, 67), (3, 69)]
        if bar >= 2:
            for t, n in mel: keys.append((b0 + t, 0.45, n, 92))
    tracks = [
        ("kick", part("kick", "atmd", "p22_kick.json", kick), 0.95, 0.0),
        ("snare", part("snare", "atmd", "p23_snare.json", snare), 0.55, 0.05),
        ("hat", part("hat", "atmd", "p24_hi_hat.json", hat), 0.07, 0.35),
        ("bass", part("bass", "atmd", "p01_fat_bass.json", bass), 0.55, 0.0),
        ("chords", part("chords", "at700", "03_ensemble_strings.json", chords), 0.35, -0.3),
        ("keys", part("keys", "atfs1", "p21_tine_piano.json", keys), 0.40, 0.25),
    ]
    n = max(len(t[1]) for t in tracks)
    mix = np.zeros((n, 2))
    for name, x, g, pan in tracks:
        x = x / (np.max(np.abs(x)) + 1e-9)
        gl, gr = np.cos((pan + 1) * np.pi / 4) * np.sqrt(2), np.sin((pan + 1) * np.pi / 4) * np.sqrt(2)
        mix[: len(x), 0] += g * x[:, 0] * gl
        mix[: len(x), 1] += g * x[:, 1] * gr
    # Mezcla casera: algo opaca (pasa bajos suave de 1 polo a 9 kHz) y con margen.
    a = np.exp(-2 * np.pi * 9000 / SR)
    for c in range(2):
        y = np.zeros(n); s = 0.0
        col = mix[:, c]
        for i in range(n):
            s = (1 - a) * col[i] + a * s
            y[i] = s
        mix[:, c] = y
    mix *= 10 ** (-6.0 / 20) / np.max(np.abs(mix))
    end = int((BARS * 4 * BEAT + 1.5) * SR)
    mix = mix[:end]
    fade = int(1.0 * SR)
    mix[-fade:] *= np.linspace(1, 0, fade)[:, None]
    path = os.path.join(OUT, "demo-mix.wav")
    write_wav(path, mix)
    print(path, "%.1f s" % (len(mix) / SR))


if __name__ == "__main__":
    main()
