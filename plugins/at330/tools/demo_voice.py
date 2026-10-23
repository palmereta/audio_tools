#!/usr/bin/env python3
"""Voz de demostración para el vocoder del AT330, sintética y propia (ninguna voz grabada).

A diferencia de la de Plumb (sólo vocales cantadas), ésta habla: vocales, nasales y líquidas con su pulso glótico,
y consonantes sordas (s, sh, f, h, t, k, p) hechas con ruido filtrado. Las sordas son las que el detector de
sonoras/sordas del vocoder tiene que reconocer y mandar al ruido; sin ellas la demo no las mostraría.

El texto se escribe en fonemas separados por espacios, con "|" como pausa corta y "_" como pausa larga; cada frase
empieza en un tiempo dado (segundos) para caer sobre el compás de la pista:

    python3 plugins/at330/tools/demo_voice.py out.wav [--script night|count|whisper] [--bpm 96] [--f0 110] [--seed 1]

Fonemas: a e i o u ae er (vocales), m n l r w y (sonoras), s sh f h (fricativas sordas), t k p (oclusivas sordas),
z v (fricativas sonoras: ruido más pulso).
"""
import sys, wave, math
import numpy as np
from scipy.signal import butter, lfilter, lfilter_zi

FS = 48000

# F1-F4 (Hz), anchos de banda y amplitud de la voz por fonema sonoro (tablas de Peterson y Barney para vocales de voz
# masculina; nasales y líquidas aproximadas).
VOICED = {
    "a": ([730, 1090, 2440, 3400], [90, 110, 140, 180], 1.0), "e": ([530, 1840, 2480, 3500], [70, 100, 140, 180], 0.95),
    "i": ([270, 2290, 3010, 3700], [60, 100, 140, 180], 0.85), "o": ([570, 840, 2410, 3300], [80, 90, 140, 180], 0.95),
    "u": ([300, 870, 2240, 3300], [70, 90, 140, 180], 0.85), "ae": ([660, 1720, 2410, 3400], [90, 110, 140, 180], 1.0),
    "er": ([490, 1350, 1690, 3300], [80, 110, 120, 180], 0.9),
    "m": ([280, 1000, 2200, 3300], [60, 200, 300, 400], 0.35), "n": ([280, 1700, 2600, 3400], [60, 200, 300, 400], 0.35),
    "l": ([360, 1300, 2700, 3400], [80, 150, 200, 250], 0.55), "r": ([420, 1300, 1600, 3200], [80, 120, 150, 250], 0.6),
    "w": ([300, 700, 2200, 3200], [70, 100, 200, 250], 0.55), "y": ([280, 2200, 2900, 3600], [60, 150, 200, 250], 0.55),
    "z": ([300, 1700, 2600, 3500], [80, 200, 300, 400], 0.25), "v": ([300, 1100, 2300, 3300], [80, 200, 300, 400], 0.25),
}
# Ruido de las sordas: (tipo de filtro, frecuencias, nivel)
NOISE = {
    "s": ("hp", 4500, 0.5), "sh": ("bp", (1800, 5500), 0.55), "f": ("bp", (1200, 9000), 0.18), "h": ("bp", (500, 3500), 0.12),
    "z": ("hp", 4200, 0.3), "v": ("bp", (1200, 8000), 0.1),
    "t": ("hp", 3200, 0.9), "k": ("bp", (1400, 3200), 0.9), "p": ("bp", (300, 1600), 0.8),
}
DUR = {"vowel": 0.15, "son": 0.075, "fric": 0.12, "stop": 0.085}
VOWELS = {"a", "e", "i", "o", "u", "ae", "er"}
STOPS = {"t", "k", "p"}
FRICS = {"s", "sh", "f", "h"}

SCRIPTS = {
    # (inicio en tiempos, fonemas, variación de f0 en semitonos a lo largo de la frase)
    "night": [
        (0, "w i | ae r | th_ e | n ai t", -2), (8, "w i | f a l | th_ r u | th_ e | s t a r z", -3),
        (16, "e k o z | o v | s i t i z | th_ ae t | n e v er | s l i p", -2), (28, "s t i l | h ie r", -4),
    ],
    "count": [
        (0, "w a n", -1), (2, "t u", -1), (4, "th_ r i", -1), (6, "f o r", -1),
        (8, "f ai v", -1), (10, "s i k s", -1), (12, "s e v e n", -1), (14, "ei t", -2),
    ],
    "whisper": [
        (0, "s i k r e t | s i g n a l z", 0), (6, "h i s | th_ r u | th_ e | w ai er", 0), (12, "f a s t er | f a s t er", 0),
    ],
}
# Diptongos y "th" se arman con lo que hay.
ALIAS = {"ai": ["a", "i"], "ei": ["e", "i"], "ie": ["i", "er"], "th_": ["f"], "g": ["k"]}


def expand(text):
    out = []
    for tok in text.split():
        out += ALIAS.get(tok, [tok])
    return out


def bandnoise(rng, n, kind, f):
    x = rng.standard_normal(n)
    if kind == "hp":
        b, a = butter(4, f / (FS / 2), "high")
    else:
        b, a = butter(2, [f[0] / (FS / 2), f[1] / (FS / 2)], "band")
    return lfilter(b, a, x)


def synth(script, bpm=96.0, f0base=110.0, seed=1, whisper=False):
    rng = np.random.default_rng(seed)
    beat = 60.0 / bpm
    # Línea de tiempo: por muestra, f0, amplitud de voz y formantes (por cuadros de 5 ms), y el ruido sumado aparte.
    end = max(t * beat for t, _, _ in script) + 6.0
    N = int(end * FS)
    f0 = np.full(N, f0base); vamp = np.zeros(N); noise = np.zeros(N)
    frame = 240
    nf = N // frame + 2
    F = np.tile(np.array(VOICED["a"][0], float), (nf, 1)); BW = np.tile(np.array(VOICED["a"][1], float), (nf, 1))
    for start, text, fall in script:
        ph = expand(text)
        t = start * beat
        t0 = t
        segs = []
        for p in ph:
            if p == "|": t += 0.07; continue
            if p in VOWELS: d = DUR["vowel"] * (1.25 if p in ("a", "o", "ae") else 1.0)
            elif p in STOPS: d = DUR["stop"]
            elif p in FRICS: d = DUR["fric"]
            else: d = DUR["son"]
            segs.append((p, t, d)); t += d
        t1 = t
        # Entonación: arranca un poco arriba y cae al final de la frase.
        a, b = int(t0 * FS), min(N, int(t1 * FS) + 1)
        u = np.linspace(0, 1, b - a)
        f0[a:b] = f0base * 2 ** ((1.5 - (1.5 - fall) * u ** 1.5 + 0.4 * np.sin(2 * np.pi * u * 2.3)) / 12.0)
        for p, ts, d in segs:
            i0, i1 = int(ts * FS), min(N, int((ts + d) * FS))
            n = i1 - i0
            if n <= 0: continue
            ramp = np.minimum(1, np.minimum(np.arange(n) / (0.012 * FS), (n - np.arange(n)) / (0.015 * FS)))
            if p in VOICED:
                fr, bw, amp = VOICED[p]
                vamp[i0:i1] = np.maximum(vamp[i0:i1], amp * ramp)
                F[i0 // frame:i1 // frame + 1] = fr; BW[i0 // frame:i1 // frame + 1] = bw
            if p in NOISE:
                kind, f, lv = NOISE[p]
                if p in STOPS:
                    # Oclusiva: silencio (cierre), explosión de 12 ms y aspiración de 30 ms.
                    c = int(0.035 * FS); bn = int(0.012 * FS); an = int(0.03 * FS)
                    burst = bandnoise(rng, bn, kind, f) * np.exp(-np.arange(bn) / (0.004 * FS))
                    asp = bandnoise(rng, an, "bp", (800, 5000)) * 0.25 * np.exp(-np.arange(an) / (0.012 * FS))
                    s0 = i0 + c
                    noise[s0:s0 + bn] += lv * burst[:max(0, min(bn, N - s0))]
                    noise[s0 + bn:s0 + bn + an] += asp[:max(0, min(an, N - s0 - bn))]
                else:
                    noise[i0:i1] += lv * bandnoise(rng, n, kind, f) * ramp
    # Fuente glótica (Rosenberg) con jitter y shimmer; en susurro, ruido en lugar del pulso.
    src = np.zeros(N); ph = 0.0; prev = 0.0; jit = 0.0; shim = 1.0
    if whisper:
        src = rng.standard_normal(N) * 0.08 * vamp
    else:
        for n in range(N):
            if vamp[n] <= 0.0: prev = 0.0; continue
            ph += f0[n] * (1 + jit) / FS
            if ph >= 1: ph -= 1; jit = 0.006 * rng.normal(); shim = 1 + 0.05 * rng.normal()
            u = 0.5 * (1 - math.cos(math.pi * ph / 0.4)) if ph < 0.4 else (math.cos(0.5 * math.pi * (ph - 0.4) / 0.16) if ph < 0.56 else 0.0)
            src[n] = (u - prev) * shim * vamp[n] + 0.002 * vamp[n] * rng.normal(); prev = u
    # Formantes en cascada, por cuadros de 5 ms con el filtro de cada cuadro (estado continuo); las formantes se
    # deslizan hacia el blanco en ~25 ms, como la coarticulación.
    y = np.zeros(N)
    cur = F[0].copy(); cbw = BW[0].copy()
    zi = [np.zeros(2) for _ in range(4)]
    k = 1 - math.exp(-frame / (0.025 * FS))
    for fi in range(N // frame + 1):
        a0, a1 = fi * frame, min(N, (fi + 1) * frame)
        if a0 >= N: break
        cur += (F[fi] - cur) * k; cbw += (BW[fi] - cbw) * k
        x = src[a0:a1]
        for j in range(4):
            r = math.exp(-math.pi * cbw[j] / FS); th = 2 * math.pi * cur[j] / FS
            b = [1 - 2 * r * math.cos(th) + r * r]; a = [1, -2 * r * math.cos(th), r * r]
            x, zi[j] = lfilter(b, a, x, zi=zi[j])
        y[a0:a1] = x
    # Radiación por la boca (+6 dB/oct) y la mezcla con el ruido.
    y = np.append(y[0], np.diff(y)) * 6.0
    y = y / (np.max(np.abs(y)) + 1e-9) + noise / (np.max(np.abs(noise)) + 1e-9) * 1.1
    y *= 0.5 / (np.max(np.abs(y)) + 1e-9)
    return y


def main():
    path = sys.argv[1]
    arg = lambda k, d: sys.argv[sys.argv.index(k) + 1] if k in sys.argv else d
    name = arg("--script", "night")
    y = synth(SCRIPTS[name], float(arg("--bpm", "96")), float(arg("--f0", "110")), int(arg("--seed", "1")), whisper=name == "whisper")
    pcm = (np.clip(y, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(FS); w.writeframes(pcm.tobytes())
    print(f"{path}: {len(y) / FS:.1f} s")


if __name__ == "__main__":
    main()
