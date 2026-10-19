#!/usr/bin/env python3
"""Voz de demostración de Plumb, sintética y propia (ninguna voz grabada): un pulso glótico con jitter y shimmer por
formantes de vocal que cambian, cantando una melodía con notas desafinadas, entradas desde abajo (scoops) y vibrato.

    python3 plugins/plumb/tools/demo_voice.py out.wav [--melody lead|low|high|rap] [--seed 1]
"""
import sys, wave, math
import numpy as np

FS = 48000
VOWELS = {  # F1-F4 (Hz) y anchos de banda
    "a": ([730, 1090, 2440, 3400], [80, 90, 120, 150]), "e": ([530, 1840, 2480, 3500], [60, 100, 120, 150]),
    "i": ([300, 2250, 3000, 3600], [50, 100, 120, 150]), "o": ([500, 850, 2400, 3300], [70, 80, 120, 150]),
    "u": ([330, 870, 2250, 3200], [60, 80, 120, 150]),
}
MELODIES = {
    # (inicio s, nota MIDI, desafinación en cents, vocal, duración s)
    "lead": [(0.0, 64, +28, "a", 0.55), (0.6, 67, -35, "e", 0.45), (1.1, 69, +18, "a", 0.9), (2.1, 67, -22, "o", 0.45),
             (2.6, 64, +38, "a", 0.5), (3.15, 62, -30, "e", 0.45), (3.65, 64, +15, "a", 1.2), (5.0, 69, -40, "i", 0.6),
             (5.65, 71, +25, "a", 0.5), (6.2, 72, -18, "o", 1.0), (7.3, 69, +30, "a", 1.3)],
    "low": [(0.0, 45, +30, "o", 0.8), (0.9, 47, -28, "a", 0.6), (1.6, 48, +22, "o", 1.0), (2.8, 45, -35, "u", 1.2)],
    "high": [(0.0, 76, -30, "a", 0.6), (0.7, 79, +25, "i", 0.6), (1.4, 81, -20, "a", 1.0), (2.5, 79, +35, "e", 1.0)],
}

def synth(melody, seed=1):
    rng = np.random.default_rng(seed)
    end = max(t + d for t, _, _, _, d in melody) + 0.4
    N = int(end * FS)
    f0 = np.zeros(N); vowel_at = ["a"] * N; amp = np.zeros(N)
    for (t0, note, cents, v, dur) in melody:
        a, b = int(t0 * FS), int((t0 + dur) * FS)
        t = np.arange(b - a) / FS
        base = note + cents / 100.0
        scoop = -0.9 * np.exp(-t / 0.07)                                  # entrada desde abajo
        vib = 0.35 * np.sin(2 * np.pi * 5.6 * t + rng.uniform(0, 6)) * np.clip((t - 0.25) / 0.3, 0, 1)
        drift = 0.08 * np.sin(2 * np.pi * 0.7 * t + rng.uniform(0, 6))
        f0[a:b] = 440.0 * 2 ** ((base + scoop + vib + drift - 69) / 12.0)
        env = np.clip(t / 0.03, 0, 1) * np.clip((dur - t) / 0.06, 0, 1)
        amp[a:b] = np.maximum(amp[a:b], env)
        for i in range(a, b): vowel_at[i] = v
    # Pulso glótico (Rosenberg) con jitter y shimmer; la derivada del flujo.
    out = np.zeros(N); ph = 0.0; prev = 0.0; jit = 0.0; shim = 1.0
    states = {}
    for n in range(N):
        if f0[n] <= 0: out[n] = 0; prev = 0; continue
        ph += f0[n] * (1 + jit) / FS
        if ph >= 1: ph -= 1; jit = 0.004 * rng.normal(); shim = 1 + 0.03 * rng.normal()
        u = 0.5 * (1 - math.cos(math.pi * ph / 0.4)) if ph < 0.4 else (math.cos(0.5 * math.pi * (ph - 0.4) / 0.16) if ph < 0.56 else 0.0)
        x = (u - prev) * shim * amp[n] + 0.003 * amp[n] * rng.normal(); prev = u
        out[n] = x
    # Formantes: resonadores de dos polos, la vocal interpolada en 40 ms.
    y = np.zeros(N)
    fcur = np.array(VOWELS["a"][0], float); bw = np.array(VOWELS["a"][1], float)
    s1 = np.zeros(4); s2 = np.zeros(4)
    for n in range(N):
        tf, tb = VOWELS[vowel_at[n]]
        fcur += (np.array(tf) - fcur) * (1 / (0.04 * FS)) * 6; bw += (np.array(tb) - bw) * 0.001
        x = out[n]
        for k in range(4):
            r = math.exp(-math.pi * bw[k] / FS); th = 2 * math.pi * fcur[k] / FS
            yk = (1 - r) * x + 2 * r * math.cos(th) * s1[k] - r * r * s2[k]
            s2[k] = s1[k]; s1[k] = yk; x = yk
        y[n] = x
    y *= 0.3 / (np.max(np.abs(y)) + 1e-9)
    return y

def main():
    path = sys.argv[1]
    mel = sys.argv[sys.argv.index("--melody") + 1] if "--melody" in sys.argv else "lead"
    seed = int(sys.argv[sys.argv.index("--seed") + 1]) if "--seed" in sys.argv else 1
    y = synth(MELODIES[mel], seed)
    pcm = (np.clip(y, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(FS); w.writeframes(pcm.tobytes())
    print(f"{path}: {len(y) / FS:.1f} s")

if __name__ == "__main__":
    main()
