#!/usr/bin/env python3
"""Biblioteca del Sample Pack: render con los motores reales, one-shots, loops sin costura y efectos.

Todo el audio sale de los plugins de Audio Tools (render offline sin interfaz). Esta biblioteca no sabe nada del
contenido: lo declara scripts/samplepack_content.py y lo orquesta scripts/samplepack.py.
Se corre con el Python del venv de audio (numpy, soundfile, scipy).
"""
import json, math, os, re, subprocess, sys, tempfile
import numpy as np
import soundfile as sf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRIV = os.path.join(os.path.dirname(ROOT), "audiotools-private", "samplepack")
WORK = os.path.join(PRIV, "work")
SR = 44100
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import samples as S            # Song (MIDI) y m()
from samples import Song, m

NICE = ["nice", "-n", "19"]
TP_CAP = -1.2        # pico real maximo (dBTP) de todo lo que sale

def tool(p):
    for d in ("build", "build-rel", "build-devicenti-rel"):
        t = os.path.join(ROOT, d, "plugins", p, f"{p}render_artefacts", "Release", f"{p}render")
        if os.path.exists(t): return t
    raise SystemExit(f"no hay {p}render")

def presetfile(p, key):
    d = os.path.join(ROOT, "plugins", p, "assets", "presets")
    for f in sorted(os.listdir(d)):
        if f.endswith(".json") and key in f: return os.path.join(d, f)
    raise SystemExit(f"preset {p}/{key}")

def preset_name(p, key):
    return json.load(open(presetfile(p, key)))["name"]

# ───────── lectura / escritura ─────────
def rd(path):
    x, sr = sf.read(path, dtype="float32", always_2d=True)
    if sr != SR:
        raise SystemExit(f"{path}: {sr} Hz")
    if x.shape[1] == 1: x = np.repeat(x, 2, axis=1)
    return x[:, :2].copy()

def wr(path, x):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sf.write(path, np.clip(x, -1.0, 1.0), SR, subtype="PCM_24")

def peak_db(x): return 20 * math.log10(max(1e-9, float(np.abs(x).max())))

def tp_db(x):
    """Pico real (sobremuestreo x4), para que los picos entre muestras tampoco pasen de -1 dBTP."""
    from scipy.signal import resample_poly
    return 20 * math.log10(max(1e-9, float(np.abs(resample_poly(x, 4, 1, axis=0)).max())))

def lufs(x):
    """Sonoridad integrada (ffmpeg ebur128) de un arreglo."""
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, "a.wav"); sf.write(p, np.clip(x, -1, 1), SR, subtype="FLOAT")
        r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", p, "-af", "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True)
    mm = re.findall(r"\n\s+I:\s+(-?[\d.]+) LUFS", r.stderr)
    return float(mm[-1]) if mm else -70.0

def gain_db(x, db): return x * np.float32(10 ** (db / 20))

# ───────── MIDI → render ─────────
MIDI_SYNTHS = ("at20v", "at101v", "at42", "at700", "atfs1", "atmd")

def run(cmd):
    r = subprocess.run(NICE + cmd, capture_output=True, text=True)
    return r

def render_midi(slug, preset, song, secs, extra=(), bpm=None):
    """Toca un Song con el motor de `slug` y devuelve (n, 2) a 44,1 kHz con `secs` de largo."""
    with tempfile.TemporaryDirectory(dir=WORK) as td:
        mid, out = os.path.join(td, "s.mid"), os.path.join(td, "o.wav")
        song.write(mid)
        bpm = bpm or song.bpm
        if slug in MIDI_SYNTHS:
            cmd = [tool(slug), presetfile(slug, preset), out, "--midi", mid, "--secs", str(secs), "--sr", str(SR)]
            if slug in ("at101v", "atmd"): cmd += ["--bpm", str(bpm)]
            if slug == "atmd": cmd += ["--tail", "1"]
        else:       # at330, devicenti, thrum, pallor: por nombre de preset
            cmd = [tool(slug), out, "--preset", preset, "--midi", mid, "--bpm", str(bpm), "--seconds", str(secs), "--sr", str(SR)]
        r = run(cmd + list(extra))
        if not os.path.exists(out): raise SystemExit(f"falló {slug}/{preset}: {r.stdout[-300:]} {r.stderr[-300:]}")
        x = rd(out)
    n = int(secs * SR)
    return x[:n] if len(x) >= n else np.vstack([x, np.zeros((n - len(x), 2), np.float32)])

def render_cli(slug, args, secs):
    """Motores con secuenciador propio (AT303, Pallor, DeviceNTI, Thrum con --chords)."""
    with tempfile.TemporaryDirectory(dir=WORK) as td:
        out = os.path.join(td, "o.wav")
        r = run([tool(slug), out] + [str(a) for a in args] + ["--sr", str(SR)])
        if not os.path.exists(out): raise SystemExit(f"falló {slug} {args}: {r.stdout[-300:]} {r.stderr[-300:]}")
        x = rd(out)
    n = int(secs * SR)
    return x[:n] if len(x) >= n else np.vstack([x, np.zeros((n - len(x), 2), np.float32)])

# ───────── efectos (Halation, Tarnish: audio → audio; Murmur: con preset .json) ─────────
def fx_process(kind, x, preset, tail=4.0, bpm=None, extra=()):
    """Pasa un arreglo por Halation / Tarnish / Murmur y devuelve el resultado (con la cola)."""
    with tempfile.TemporaryDirectory(dir=WORK) as td:
        i, o = os.path.join(td, "i.wav"), os.path.join(td, "o.wav")
        sf.write(i, np.clip(x, -1, 1), SR, subtype="FLOAT")
        if kind == "murmur":
            cmd = [tool("murmur"), presetfile("murmur", preset), i, o, "--secs", f"{len(x) / SR:.3f}", "--tail", str(tail)]
        else:
            cmd = [tool(kind), i, o, "--preset", preset_name(kind, preset), "--tail", str(tail)]
        if bpm and kind != "murmur": cmd += ["--bpm", str(bpm)]
        elif bpm: cmd += ["--bpm", str(bpm)]
        r = run(cmd + list(extra))
        if not os.path.exists(o): raise SystemExit(f"falló {kind}/{preset}: {r.stdout[-300:]} {r.stderr[-300:]}")
        y = sf.read(o, dtype="float32", always_2d=True)
        if y[1] != SR: raise SystemExit(f"{kind}: salida a {y[1]} Hz")
        y = y[0]
    if y.shape[1] == 1: y = np.repeat(y, 2, axis=1)
    return y[:, :2].copy()

# ───────── loops sin costura ─────────
def fold_loop(x, L):
    """Plega la cola que sobra después de L muestras sobre el comienzo: el loop cierra con sus reverberaciones."""
    y = x[:L].copy()
    t = x[L:L + L]
    y[:len(t)] += t
    return y

def loop_len(bpm, bars, beats=4):
    return int(round(bars * beats * 60.0 / bpm * SR))

def close_seam(y, ms=3.0):
    """Cierra la costura del loop: el salto entre la ultima y la primera muestra se reparte en una rampa de unos milisegundos
    al final, asi el valor es continuo cuando el loop vuelve a empezar (sin clic ni caida de nivel)."""
    n = int(ms * SR / 1000)
    j = y[0] - y[-1]
    if float(np.abs(j).max()) < 1e-5: return y
    z = y.copy()
    ramp = (np.linspace(0, 1, n, dtype=np.float32) ** 2)[:, None]
    z[-n:] += ramp * j[None, :]
    return z

def wet_loop(kind, dry, preset, bpm, reps=4, tail=3.0, extra=()):
    """Versión mojada de un loop sin costura: se repite `reps` veces, se procesa y se toma una vuelta del medio con un
    cruce corto, así las reverberaciones y los ecos del final de la vuelta anterior entran al comienzo."""
    L = len(dry)
    y = fx_process(kind, np.vstack([dry] * reps), preset, tail=tail, bpm=bpm, extra=extra)
    k = reps - 2
    seg = y[k * L:(k + 1) * L + int(0.02 * SR)].copy()
    n = int(0.012 * SR)
    # cruce de potencia constante entre el comienzo y lo que seguiría después del final
    nxt = seg[L:L + n]
    head = seg[:n]
    t = np.linspace(0, math.pi / 2, n, dtype=np.float32)[:, None]
    seg[:n] = head * np.sin(t) + nxt * np.cos(t)
    return seg[:L]

# ───────── recorte, normalización ─────────
def trim_tail(x, thresh_db=-64.0, min_len=0.1, fade=0.03):
    a = np.abs(x).max(1)
    th = 10 ** (thresh_db / 20) * max(1e-6, a.max())
    idx = np.where(a > th)[0]
    end = int(idx[-1]) + 1 if len(idx) else len(x)
    if end >= len(x) - int(0.03 * SR): fade = max(fade, 0.4)      # la nota seguia sonando: cola mas larga
    end = min(len(x), end + int(0.02 * SR))
    y = x[:max(end, int(min_len * SR))].copy()
    f = min(len(y), int(fade * SR))
    y[-f:] *= np.linspace(1, 0, f, dtype=np.float32)[:, None]
    return y

def trim_head(x, thresh_db=-50.0, keep=0.002):
    a = np.abs(x).max(1)
    th = 10 ** (thresh_db / 20) * max(1e-6, a.max())
    idx = np.where(a > th)[0]
    s = max(0, int(idx[0]) - int(keep * SR)) if len(idx) else 0
    return x[s:]

def fade_in(x, ms=2.0):
    n = min(len(x), int(ms * SR / 1000))
    y = x.copy(); y[:n] *= np.linspace(0, 1, n, dtype=np.float32)[:, None]
    return y

def remove_dc(x):
    return x - x.mean(0, keepdims=True) if len(x) > SR else x

import numba

@numba.njit(cache=True)
def _release(g, rel):
    out = g.copy()
    for i in range(1, len(g)):
        up = out[i - 1] + rel
        if out[i] > up: out[i] = up
    return out

def limit(x, ceil_db=-1.5, look_ms=2.0, rel_ms=80.0):
    """Limitador de pico con anticipacion (para subir loops muy picudos sin recortar)."""
    from scipy.ndimage import minimum_filter1d, uniform_filter1d
    c = 10 ** (ceil_db / 20)
    env = np.abs(x).max(1)
    greq = np.minimum(1.0, c / np.maximum(env, 1e-9)).astype(np.float64)
    n = max(3, int(look_ms * SR / 1000))
    g = uniform_filter1d(minimum_filter1d(greq, size=n, mode="nearest"), size=n, mode="nearest")
    g = _release(g, 1.0 / (rel_ms * SR / 1000))
    return (x * g[:, None]).astype(np.float32)

def norm_loop(x, target_lufs=-16.0, peak_cap=-1.5, max_limit_db=4.0):
    """Ganancia fija a -16 LUFS integrados con pico maximo de -1,5 dBFS; si el pico no deja llegar, hasta 4 dB de limitador
    (aplicado sobre el loop repetido, para que la costura no cambie)."""
    g = target_lufs - lufs(x)
    gcap = peak_cap - peak_db(x)
    if g <= gcap:
        y = gain_db(x, g)
    else:
        g = min(g, gcap + max_limit_db)
        L = len(x)
        y = limit(gain_db(np.vstack([x, x, x]), g), peak_cap)[L:2 * L]
    t = tp_db(y)
    if t > TP_CAP:                      # picos entre muestras: baja el conjunto hasta -1,2 dBTP
        y = gain_db(y, TP_CAP - t); g += TP_CAP - t
    return y, g

def norm_peak(x, peak=-3.0):
    g = min(peak - peak_db(x), TP_CAP - tp_db(x))
    return gain_db(x, g), g

# ───────── notación de patrones ─────────
_TOK = re.compile(r"([A-G][#b]?)(-?\d)")

def transpose(pat, n):
    return _TOK.sub(lambda mo: _name(m(mo.group(0)) + n), pat)

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
def _name(n): return f"{NAMES[n % 12]}{n // 12 - 1}"

def song_from(bpm, bars, swing=0.0, gate=0.9, vel=100, spb=16):
    """Compases escritos como cuadrículas de 16 pasos (semicorcheas).
    Cada paso: '.' silencio, '-' continúa la nota anterior, 'A1' nota, 'A1!' acento, 'A1~' suave, 'A1:2' largo en pasos,
    'C3+E3+G3' acorde. `swing` retrasa los pasos impares (0 a 0,5 de un paso)."""
    s = Song(bpm)
    step = 1.0 / (spb // 4)
    for bi, bar in enumerate(bars):
        toks = bar.split()
        assert len(toks) == spb, f"compás {bi}: {len(toks)} pasos: {bar}"
        for i, t in enumerate(toks):
            if t in (".", "-"): continue
            v = vel
            if t.endswith("!"): v, t = 118, t[:-1]
            elif t.endswith("~"): v, t = 64, t[:-1]
            ln = None
            if ":" in t: t, ln = t.split(":"); ln = int(ln)
            else:
                ln = 1
                j = i + 1
                while j < spb and toks[j] == "-": ln += 1; j += 1
            beat = bi * 4 + i * step + (swing * step if i % 2 else 0.0)
            for nn in t.split("+"):
                s.note(beat, max(0.05, ln * step * gate - (swing * step if i % 2 else 0.0) * 0), m(nn), v)
    return s

def rep(bar, n): return [bar] * n

def mix(*parts):
    n = max(len(p) for p, _ in parts)
    y = np.zeros((n, 2), np.float32)
    for p, g in parts: y[:len(p)] += gain_db(p, g)
    return y

# ───────── análisis de calidad ─────────
def seam_click(y):
    """Salto en la costura de un loop respecto del salto típico entre muestras vecinas (≈1 = sin clic)."""
    d = np.abs(np.diff(y, axis=0)).max(1)
    jump = float(np.abs(y[0] - y[-1]).max())
    ref = float(np.percentile(d, 99.5)) + 1e-6
    return jump / ref

def clip_count(y): return int((np.abs(y) >= 0.9999).sum())

def lead_silence(y, thr_db=-50.0):
    a = np.abs(y).max(1); th = 10 ** (thr_db / 20) * max(1e-6, float(a.max()))
    idx = np.where(a > th)[0]
    return (idx[0] / SR) if len(idx) else 99.0

def crackle_score(y):
    """Cantidad de chasquidos aislados: picos de la segunda diferencia muy por encima de su vecindad (sólo tonal)."""
    x = y.mean(1)
    d2 = np.abs(np.diff(x, 2))
    w = 441
    k = np.ones(w) / w
    env = np.convolve(d2, k, mode="same") + 1e-7
    r = d2 / env
    # un chasquido es un pico aislado: muy alto frente a su ventana y sin vecinos altos
    cand = np.where(r > 40)[0]
    n = 0; last = -10000
    for c in cand:
        if c - last > 441: n += 1
        last = c
    return n
