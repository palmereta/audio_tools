#!/usr/bin/env python3
"""Audio del film de la web del AT20V y su partitura: una línea acid de 12 compases a 124 BPM con el filtro que se abre
hasta gritar, un cable que entra en el CV del pasa bajos (el filtro empieza a oscilar al tiempo) y un bend con la rueda.

    plugins/at20v/tools/blender/film_audio.py <out.wav>          (lo llama scripts/film.py desde film.json)

El render CLI (at20vrender) no automatiza perillas: se renderiza la misma frase con K ajustes del filtro (corte y pico
de menor a mayor) y se pasa de uno a otro muestra a muestra según la curva x(t). Como el motor es determinista y las
notas son las mismas, las K versiones están en fase y el cruce suena a un barrido del corte (verificado: correlación
entre versiones vecinas cerca de 1).

La escena (scene.py --mode film2) importa este módulo para mover lo mismo que suena: NOTES (teclas), x_at (perillas de
corte y pico), bend_at (rueda), PLUG_IN (la ficha que entra) y wobble_at (lo que hace el cable en el filtro).
"""
import math, os, struct, subprocess, sys, tempfile, wave

BPM = 124.0
BEAT = 60.0 / BPM
BARS = 12
PLUG_IN = 26.0                    # tiempo (en tiempos de compás) en que la ficha llega al fondo del jack
HOLD = (30.0, 33.75)              # nota larga del bend (la toma de la rueda)

def m(name):
    N = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
    i = 2 if len(name) > 2 and name[1] == "#" else 1
    return N[name[:i]] + 12 * (int(name[i:]) + 1)

def _notes():
    """(tiempo de inicio, duración, nota MIDI, velocidad) en tiempos de compás."""
    pat = ["A1", "A1", "A2", "A1", "-", "A1", "C2", "A1", "D2", "A1", "-", "E2", "A1", "G2", "A1", "A2"]
    roots = [0, 0, 3, 5, 0, 0, -2, 3, 0, 0, 5, 3]
    out = []
    for bar in range(BARS - 1):
        for i, p in enumerate(pat):
            b = bar * 4 + i * 0.25
            if p == "-" or HOLD[0] <= b < HOLD[1] + 0.25: continue
            out.append((b, 0.2 if i % 4 else 0.24, m(p) + roots[bar], 120 if i % 4 == 0 else 84))
    out.append((HOLD[0], HOLD[1] - HOLD[0], m("A2") + 3, 120))      # la nota que se dobla con la rueda (Do)
    out.append(((BARS - 1) * 4, 3.6, m("A1"), 120))                 # cierre: La largo mientras el filtro se cierra
    return sorted(out)

NOTES = _notes()

def _smooth(u):
    u = min(1.0, max(0.0, u)); return u * u * (3 - 2 * u)

def x_at(b):
    """Posición del filtro (0 = cerrado y manso, 1 = abierto y gritando) en el tiempo b (en tiempos de compás)."""
    pts = [(0, 0.06), (8, 0.16), (14, 0.28), (22, 0.86), (30, 0.80), (34, 0.92), (40, 1.0), (41, 1.0), (46.5, 0.12), (60, 0.12)]
    for (b0, v0), (b1, v1) in zip(pts[:-1], pts[1:]):
        if b0 <= b <= b1: return v0 + (v1 - v0) * _smooth((b - b0) / (b1 - b0))
    return pts[-1][1] if b > pts[-1][0] else pts[0][1]

def wobble_at(b):
    """Modulación del cable MG → CV del corte: un triángulo por tiempo desde que entra la ficha; se apaga al final."""
    amp = 0.16 * _smooth((b - PLUG_IN) / 1.0) * (1 - _smooth((b - 43.0) / 3.0))
    ph = (b - PLUG_IN) % 1.0
    tri = 4 * ph - 1 if ph < 0.5 else 3 - 4 * ph
    return amp * tri

def bend_at(b):
    """Rueda (−1..1) durante la nota larga: sube un cuarto, se sostiene y vuelve."""
    return _smooth((b - 30.5) / 0.75) * (1 - _smooth((b - 32.75) / 0.75))

def lfo_led(b):
    """LED del MG: sigue al triángulo (prende en la cresta)."""
    ph = (b - PLUG_IN) % 1.0
    return max(0.0, 1 - abs(ph - 0.25) * 4)

K = 25                                                              # versiones del filtro

def knob_values(x):
    """x → (corte, pico) del pasa bajos en 0..1 (lo que muestran las perillas)."""
    return 0.12 + 0.52 * x, 0.50 + 0.43 * x

# ───────── render (sólo fuera de Blender) ─────────
def _vlq(n):
    out = [n & 0x7F]; n >>= 7
    while n: out.insert(0, (n & 0x7F) | 0x80); n >>= 7
    return bytes(out)

def write_midi(path):
    tpq = 480; ev = []
    for (b, d, n, v) in NOTES:
        ev.append((b, 1, bytes([0x90, n, v]))); ev.append((b + d, 0, bytes([0x80, n, 0])))
    b = HOLD[0]
    while b <= HOLD[1]:
        x = max(0, min(16383, int(8192 + bend_at(b) * 8191))); ev.append((b, 2, bytes([0xE0, x & 0x7F, x >> 7]))); b += 1 / 32
    data = bytearray(b"\x00\xFF\x51\x03" + struct.pack(">I", int(60_000_000 / BPM))[1:]); last = 0
    for (b, _, msg) in sorted(ev, key=lambda e: (e[0], e[1])):
        t = int(round(b * tpq)); data += _vlq(t - last) + msg; last = t
    data += b"\x00\xFF\x2F\x00"
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, tpq) + b"MTrk" + struct.pack(">I", len(data)) + data)

def render(out):
    import json
    import numpy as np
    root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
    exe = os.path.join(root, "build", "plugins", "at20v", "at20vrender_artefacts", "Release", "at20vrender")
    base = json.load(open(os.path.join(root, "plugins", "at20v", "assets", "presets", "15_acid_bass.json")))
    tmp = tempfile.mkdtemp(prefix="at20v-film-", dir="/tmp/claude-501" if os.path.isdir("/tmp/claude-501") else None)
    mid = os.path.join(tmp, "film.mid"); write_midi(mid)
    secs = BARS * 4 * BEAT + 2.0
    vers = []
    for k in range(K):
        c, p = knob_values(k / (K - 1))
        pr = json.loads(json.dumps(base)); pr["knobs"]["lpf_cutoff"] = c; pr["knobs"]["lpf_peak"] = p
        pr["settings"] = {"bend_range": 5}
        pj = os.path.join(tmp, f"v{k}.json"); json.dump(pr, open(pj, "w"))
        wv = os.path.join(tmp, f"v{k}.wav")
        r = subprocess.run([exe, pj, wv, "--midi", mid, "--secs", f"{secs:.2f}"], capture_output=True, text=True)
        if r.returncode: sys.exit(r.stderr)
        w = wave.open(wv); sr = w.getframerate(); n = w.getnframes(); raw = w.readframes(n); w.close()
        a = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        v = (a[:, 0].astype(np.int32) | (a[:, 1].astype(np.int32) << 8) | (a[:, 2].astype(np.int32) << 16))
        v = np.where(v >= 1 << 23, v - (1 << 24), v).astype(np.float32) / (1 << 23)
        vers.append(v.reshape(-1, 2))
    vers = np.stack(vers)                                            # K × muestras × 2
    n = vers.shape[1]
    b = np.arange(n) / sr / BEAT
    xs = np.array([x_at(bb) + wobble_at(bb) for bb in b[::64]])
    xs = np.interp(np.arange(n), np.arange(0, n, 64), xs)
    pos = np.clip(xs, 0, 1) * (K - 1)
    i0 = np.minimum(np.floor(pos).astype(int), K - 2); fr = (pos - i0)[:, None]
    idx = np.arange(n)
    y = vers[i0, idx] * (1 - fr) + vers[i0 + 1, idx] * fr
    y = y / max(1e-6, np.abs(y).max()) * 0.89
    pcm = (np.clip(y, -1, 1) * 32767).astype("<i2")
    w = wave.open(out, "wb"); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr); w.writeframes(pcm.tobytes()); w.close()
    cc = [float(np.corrcoef(vers[k, :, 0], vers[k + 1, :, 0])[0, 1]) for k in range(0, K - 1, 6)]
    print(f"{out}: {n / sr:.2f} s, correlación entre versiones vecinas {', '.join(f'{c:.3f}' for c in cc)}")

if __name__ == "__main__":
    render(os.path.abspath(sys.argv[1]))
