#!/usr/bin/env python3
"""El sonido del film de Halation y su partitura, la misma para el audio y para la imagen (maquette.py --mode film2).

Un arpegio seco del AT101V (web/assets/samples/at101v/arp-up) cortado en notas sueltas y vuelto a poner en negras a
120 BPM, pasado por halationrender con el preset Ping-Pong Machine: cada nota rebota en corchea con puntillo de un lado
al otro. A los 10 s el control Delay / Space sube despacio hasta 100 (los ecos se funden en una sala) y a los 16 s un
acorde queda sonando en la sala abierta. Como las notas las ponemos nosotros, la imagen sabe el tiempo exacto de cada
nota y de cada eco, sin adivinarlo del sonido.

    python3 plugins/halation/tools/blender/film_audio.py      # escribe film-audio.m4a (y film-audio.wav en /tmp)

El tiempo del film (0 = primer cuadro) va PRE segundos detrás del audio: el audio arranca antes para que el film no
empiece con la sala vacía (film.json lo recorta con "start").
"""
import math, os, subprocess, sys, tempfile

BPM = 120.0
BEAT = 60.0 / BPM
ECHO = 0.75 * BEAT                       # corchea con puntillo
PRE = 2.0                                # segundos de audio antes del primer cuadro
LENGTH_A, LENGTH_B = 55.0, 84.0          # Length del preset y de la sala final
FB = 10 ** (-3 * ECHO / (0.2 * 300 ** (LENGTH_A / 100)))   # ganancia de cada repetición (T60 de Length)
SPACE0, SPACE1 = 10.0, 16.0              # Delay / Space sube entre estos segundos del film
CHORD_T = 16.0                           # el acorde final
LAST_NOTE = 15.5
TOTAL = 24.0

# Arpegio fuente: un acorde cada 2 s, 13 notas por acorde a 2/13 s (como sale del AT101V).
SRC_T0, SRC_STEP, SRC_BAR = 0.03, 2.0 / 13.0, 2.0
PATTERN = [0, 3, 6, 9]                   # qué nota del arpegio va en cada negra del compás (sube)


def space_at(t):
    """Delay / Space (0..100) en el segundo t del film."""
    u = min(1.0, max(0.0, (t - SPACE0) / (SPACE1 - SPACE0)))
    return 6.0 + 94.0 * u * u * (3 - 2 * u)


def notes():
    """(t en el film, acorde 0..3, nota del arpegio, es el acorde final). Negras desde −PRE hasta LAST_NOTE; las
    notas pares entran cargadas a la izquierda y las impares a la derecha (side() da el lado)."""
    res = []
    b = 0
    t = -PRE
    while t <= LAST_NOTE + 1e-6:
        bar = int(math.floor((t + 1e-6) / (4 * BEAT)))
        res.append((round(t, 4), bar % 4, PATTERN[b % 4], False))
        b += 1; t += BEAT
    res.append((CHORD_T, 0, 0, True))
    return res


def side(i):
    """Lado de la nota i (−1 izquierda, +1 derecha). El ping-pong del plugin cruza los canales en cada vuelta: el
    primer eco sale del mismo lado que la nota, el segundo del otro, y así."""
    return -1 if i % 2 == 0 else 1


def echoes(t_end=TOTAL + 1.0):
    """Cada repetición: (t, lado −1/+1, nivel relativo, índice de la nota, número de eco). Con Space alto las
    repeticiones se funden (la imagen las convierte en sala); acá siempre se listan."""
    res = []
    for i, (tn, c, k, chord) in enumerate(notes()):
        if chord: continue
        for j in range(1, 9):
            te = tn + j * ECHO
            if te > t_end: break
            lvl = FB ** j
            if lvl < 0.03: break
            res.append((te, side(i) * (1 if j % 2 else -1), lvl, i, j))
    return res


def build(root):
    src = os.path.join(root, "web/assets/samples/at101v/arp-up.m4a")
    tool = os.path.join(root, "build/plugins/halation/halationrender_artefacts/Release/halationrender")
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "film-audio.m4a")
    td = tempfile.mkdtemp(prefix="halation-audio-", dir="/tmp/claude-501" if os.path.isdir("/tmp/claude-501") else None)
    arp = os.path.join(td, "arp.wav")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-ac", "2", "-ar", "48000", "-c:a", "pcm_s16le", arp], check=True)
    # Cada nota: un corte de 0,15 s con ataque de 3 ms y salida de 60 ms; el acorde final son las cuatro primeras notas
    # del arpegio a la vez y más largas (la cola del sintetizador queda).
    parts, filt = [], []
    for n, (tn, c, k, chord) in enumerate(notes()):
        s0 = SRC_T0 + c * SRC_BAR + k * SRC_STEP - 0.004
        at = int(round((tn + PRE) * 1000))
        if chord:
            for m in range(4):
                s = SRC_T0 + m * SRC_STEP - 0.004
                lab = f"c{m}"
                filt.append(f"[0:a]atrim={s:.4f}:{s + 1.2:.4f},asetpts=PTS-STARTPTS,afade=t=in:d=0.004,"
                            f"afade=t=out:st=0.5:d=0.7,volume=0.42,adelay={at + 18 * m}|{at + 18 * m}[{lab}]")
                parts.append(lab)
            continue
        lab = f"n{n}"
        pl, pr = (1.0, 0.4) if side(n) < 0 else (0.4, 1.0)
        filt.append(f"[0:a]atrim={s0:.4f}:{s0 + 0.15:.4f},asetpts=PTS-STARTPTS,afade=t=in:d=0.003,"
                    f"afade=t=out:st=0.09:d=0.06,pan=stereo|c0={pl:.2f}*c0|c1={pr:.2f}*c1,adelay={at}|{at}[{lab}]")
        parts.append(lab)
    filt.append("".join(f"[{p}]" for p in parts) + f"amix=inputs={len(parts)}:normalize=0,apad=whole_dur={TOTAL + PRE + 1}[o]")
    dry = os.path.join(td, "dry.wav")
    fs = os.path.join(td, "filter.txt"); open(fs, "w").write(";\n".join(filt))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", arp, "-filter_complex_script", fs, "-map", "[o]",
                    "-t", f"{TOTAL + PRE:.2f}", "-ar", "48000", "-c:a", "pcm_s24le", dry], check=True)
    # Automatización: Space, Size, Length y Mix suben juntos entre SPACE0 y SPACE1 (pasos de 0,1 s).
    ats = []
    t = SPACE0 - 0.1
    while t <= SPACE1 + 0.05:
        u = min(1.0, max(0.0, (t - SPACE0) / (SPACE1 - SPACE0))); s = u * u * (3 - 2 * u)
        tt = f"{t + PRE:.2f}"
        ats += ["--at", tt, f"space={space_at(t):.1f}", "--at", tt, f"size={100 + 200 * s:.1f}",
                "--at", tt, f"length={LENGTH_A + (LENGTH_B - LENGTH_A) * s:.1f}", "--at", tt, f"mix={32 + 20 * s:.1f}"]
        t += 0.1
    # El shimmer (una octava arriba) entra con el acorde: la sala abierta brilla y sube.
    ats += ["--at", f"{CHORD_T + PRE - 0.5:.2f}", "on_shimmer=1", "--at", f"{CHORD_T + PRE - 0.5:.2f}", "interval=0",
            "--at", f"{CHORD_T + PRE - 0.5:.2f}", "shimmer=28"]
    wet = os.path.join(td, "wet.wav")
    subprocess.run([tool, dry, wet, "--preset", "Ping-Pong Machine", "--bpm", f"{BPM:g}", "--tail", "1"] + ats, check=True, cwd=root)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wet, "-t", f"{TOTAL + PRE + 0.5:.2f}",
                    "-af", "loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000", "-c:a", "aac", "-b:a", "192k", out], check=True)
    print(out, td)


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
