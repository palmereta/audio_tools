"""Shared pieces of the Valence demos (tools/demos.py): MIDI in and out, our render CLIs, effects, mixing and
measurement. Everything renders at 48 kHz, stereo, float.
"""
import json, os, struct, subprocess
import numpy as np, soundfile as sf, pyloudnorm as pyln
from scipy.signal import butter, sosfilt, resample_poly

SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
BUILD = os.environ.get("VALENCE_BUILD", os.path.join(ROOT, "build"))
PRESETS = os.path.join(ROOT, "plugins")


def tool(plugin, name=None):
    name = name or plugin + "render"
    for cand in (os.path.join(BUILD, "plugins", plugin, f"{name}_artefacts", "Release", name),
                 os.path.join(BUILD, "plugins", plugin, name), os.path.join(BUILD, f"{name}_artefacts", "Release", name)):
        if os.path.exists(cand): return cand
    raise FileNotFoundError(f"{name} not built in {BUILD}")


def run(cmd, cwd=None):
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True, cwd=cwd)
    if r.returncode != 0:
        raise RuntimeError(" ".join(map(str, cmd)) + "\n" + r.stdout + r.stderr)
    return r.stdout


# ── MIDI (SMF type 0, 960 ticks per quarter) ──
NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}
def m(name):
    if not isinstance(name, str): return name
    i = 2 if len(name) > 2 and name[1] in "#b" else 1
    return NOTE[name[:i]] + 12 * (int(name[i:]) + 1)

def vlq(n):
    out = [n & 0x7F]; n >>= 7
    while n: out.insert(0, (n & 0x7F) | 0x80); n >>= 7
    return bytes(out)

TPQ = 960
GM = {"kick": 36, "rim": 37, "snare": 38, "clap": 39, "chh": 42, "ohh": 46, "crash": 49, "ride": 51, "tom1": 41, "tom2": 45,
      "tom3": 48, "tom4": 50, "cowbell": 56, "clave": 75, "tamb": 54, "fx": 52}

class Song:
    """Notes and controllers on a beat grid (quarter notes), written as a type-0 file with one tempo."""
    def __init__(self, bpm): self.bpm = bpm; self.ev = []
    def note(self, beat, dur, n, vel=96, ch=1):
        n = m(n); vel = int(max(1, min(127, round(vel))))
        self.ev.append((beat, 2, bytes([0x90 | (ch - 1), n, vel]))); self.ev.append((beat + dur, 0, bytes([0x80 | (ch - 1), n, 0])))
    def chord(self, beat, dur, notes, vel=96, ch=1):
        for n in notes: self.note(beat, dur, n, vel, ch)
    def hit(self, beat, name, vel=100, dur=0.1): self.note(beat, dur, GM[name] if isinstance(name, str) else name, vel, 10)
    def cc(self, beat, num, val, ch=1): self.ev.append((beat, 1, bytes([0xB0 | (ch - 1), num, int(max(0, min(127, val)))])))
    def end(self): return max((b for b, _, _ in self.ev), default=0.0)
    def secs(self, beats): return beats * 60.0 / self.bpm
    def write(self, path):
        data = bytearray(b"\x00\xFF\x51\x03" + struct.pack(">I", int(round(60_000_000 / self.bpm)))[1:])
        last = 0
        for beat, _, msg in sorted(self.ev, key=lambda e: (e[0], e[1])):
            t = int(round(beat * TPQ)); data += vlq(t - last) + msg; last = t
        data += b"\x00\xFF\x2F\x00"
        with open(path, "wb") as f:
            f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ) + b"MTrk" + struct.pack(">I", len(data)) + data)
        return path


def read_mid(path):
    """Every channel event of a .mid as (seconds, status, d1, d2), tempo changes honoured."""
    b = open(path, "rb").read()
    assert b[:4] == b"MThd"
    _, fmt, ntr, div = struct.unpack(">IHHH", b[4:14])
    p = 14; raw = []
    for _ in range(ntr):
        assert b[p:p + 4] == b"MTrk"; ln = struct.unpack(">I", b[p + 4:p + 8])[0]; q = p + 8; e = q + ln; t = 0; run_st = 0
        while q < e:
            d = 0
            while True:
                c = b[q]; q += 1; d = (d << 7) | (c & 0x7F)
                if not c & 0x80: break
            t += d; st = b[q]
            if st == 0xFF:
                typ = b[q + 1]; q += 2; l = 0
                while True:
                    c = b[q]; q += 1; l = (l << 7) | (c & 0x7F)
                    if not c & 0x80: break
                if typ == 0x51: raw.append((t, -1, int.from_bytes(b[q:q + 3], "big"), 0))
                q += l; continue
            if st in (0xF0, 0xF7):
                q += 1; l = 0
                while True:
                    c = b[q]; q += 1; l = (l << 7) | (c & 0x7F)
                    if not c & 0x80: break
                q += l; continue
            if st & 0x80: run_st = st; q += 1
            else: st = run_st
            n = 1 if (st & 0xF0) in (0xC0, 0xD0) else 2
            d1 = b[q]; d2 = b[q + 1] if n == 2 else 0; q += n
            raw.append((t, st, d1, d2))
        p = e
    raw.sort(key=lambda r: (r[0], r[1] != -1))
    out = []; tempo = 500000; last_t = 0; sec = 0.0
    for t, st, d1, d2 in raw:
        sec += (t - last_t) * tempo / 1e6 / div; last_t = t
        if st == -1: tempo = d1; continue
        out.append((sec, st, d1, d2))
    return out


def note_spans(events):
    """(start, end, ch, note, vel) for every note; raises if a note-on repeats before its note-off (same channel)."""
    open_ = {}; spans = []; dup = []
    for sec, st, d1, d2 in events:
        kind, ch = st & 0xF0, st & 0x0F
        if kind == 0x90 and d2 > 0:
            if (ch, d1) in open_: dup.append((sec, ch, d1))
            open_[(ch, d1)] = (sec, d2)
        elif kind == 0x80 or (kind == 0x90 and d2 == 0):
            if (ch, d1) in open_:
                s, v = open_.pop((ch, d1)); spans.append((s, sec, ch, d1, v))
    return spans, list(open_.items()), dup


# ── Renders (one instrument, one stem) ──
def preset_path(plugin, stem):
    d = os.path.join(PRESETS, plugin, "assets", "presets")
    for f in sorted(os.listdir(d)):
        if f.endswith(".json") and stem in f: return os.path.join(d, f)
    raise FileNotFoundError(f"{plugin}: no preset matching {stem}")

class Audio(np.ndarray):
    """A rendered stem that remembers the .mid it was played from (lost on any new array, on purpose)."""
    mid = None

def tag(x, mid):
    y = np.asarray(x).view(Audio); y.mid = mid
    return y

def load(path):
    x, sr = sf.read(path, always_2d=True, dtype="float64")
    assert sr == SR, (path, sr)
    if x.shape[1] == 1: x = np.repeat(x, 2, axis=1)
    return x[:, :2]

def render(kind, mid, out, preset, bpm, tail=3.0, sets=(), cc=(), focus=None, extra=()):
    """One instrument playing one .mid to out (a WAV). kind is the plugin; preset a factory name (processor-based
    renders) or a preset-file fragment (engine-based renders)."""
    s = [f"{k}={v}" for k, v in dict(sets).items()]
    if kind == "pallor":
        cmd = [tool("pallor"), out, "--preset", preset, "--midi", mid, "--bpm", bpm, "--tail", tail] + sum((["--set", x] for x in s), [])
    elif kind == "devicenti":
        cmd = [tool("devicenti"), out] + (["--project", preset] if preset.endswith(".json") else ["--preset", preset]) + ["--midi", mid, "--bpm", bpm, "--tail", tail] + (["--focus", focus] if focus else []) + sum((["--set", x] for x in s), [])
    elif kind == "at303":
        cmd = [at303midi(), out, "--midi", mid, "--preset", preset, "--tail", tail] + sum((["--set", x] for x in s), []) + sum((["--cc", c] for c in cc), [])
    elif kind == "atmd":
        cmd = [atmdmidi(), preset_path("atmd", preset), out, "--midi", mid, "--tail", tail, "--bpm", bpm] + sum((["--set", x] for x in s), []) + sum((["--cc", c] for c in cc), [])
    elif kind in ("at101v", "at20v", "at700", "atfs1"):
        cmd = [tool(kind), patched(kind, preset, dict(sets), out), out, "--midi", mid] + (["--bpm", bpm] if kind == "at101v" else [])
    elif kind == "at330":
        cmd = [tool("at330"), out, "--preset", preset, "--midi", mid, "--bpm", bpm, "--tail", tail] + sum((["--set", x] for x in s), [])
    else:
        raise ValueError(kind)
    run(list(cmd) + list(extra))
    x = load(out)
    n = int((np.abs(x) >= 0.9999).sum())
    if n: CLIPPED[os.path.basename(out)] = n           # a 24-bit render hit full scale: lower that instrument's output
    return x

CLIPPED = {}

def patched(kind, preset, sets, out):
    """A copy of an engine-based preset (AT101V, AT20V, AT700, ATFS1) with values changed, next to out. The arpeggio
    and sequencer of the AT101V and the AT700 are always turned off: Valence plays the notes."""
    d = json.load(open(preset_path(kind, preset)))
    if kind == "at101v": sets = dict({"arp_mode": 0, "seq_play": 0, "hold": 0}, **sets)
    if kind == "at700": d["run"] = False
    for k, v in sets.items():
        sec = next((x for x in ("panel", "knobs", "params", "settings") if isinstance(d.get(x), dict) and k in d[x]), "settings")
        d.setdefault(sec, {})[k] = v
    path = out[:-4] + "_preset.json"
    json.dump(d, open(path, "w"), indent=1)
    return path

def dv_project(name, out_dir, **fx):
    """A DeviceNTI V factory preset (by name) as a project file with its fx values changed, for --project. tape_noise=0
    keeps TAPE (wow, flutter, saturation), AGE and the sends but drops the master crackle: the ticks heard in the renders
    are that crackle, by design (dsp/Master.h), not a render artifact."""
    d = os.path.join(PRESETS, "devicenti", "assets", "presets")
    for f in sorted(os.listdir(d)):
        if not f.endswith(".json"): continue
        v = json.load(open(os.path.join(d, f)))
        if v.get("name") == name:
            v.setdefault("fx", {}).update(fx)
            path = os.path.join(out_dir, f"dv_{f[:-5]}_" + "_".join(f"{k}{x}" for k, x in sorted(fx.items())) + ".json")
            json.dump(v, open(path, "w"))
            return path
    raise FileNotFoundError(f"devicenti: no preset named {name}")

def at303midi(): return _extra("at303midirender")
def atmdmidi(): return _extra("atmdmidirender")
def _extra(name):
    for cand in (os.path.join(BUILD, f"{name}_artefacts", "Release", name), os.path.join(BUILD, f"{name}_artefacts", name)):
        if os.path.exists(cand): return cand
    raise FileNotFoundError(f"{name}: build it with plugins/valence/tools/demos/extra.cmake")


def fx(kind, x, tmp, name, preset, bpm, tail=6.0, sets=()):
    """x through Halation, Tarnish or Murmur (their render CLIs, a factory preset, --set overrides)."""
    i = os.path.join(tmp, name + "_in.wav"); o = os.path.join(tmp, name + "_out.wav")
    sf.write(i, x.astype(np.float32), SR, subtype="FLOAT")
    if kind == "murmur":
        cmd = [tool(kind), preset_path("murmur", preset), i, o, "--bpm", bpm, "--tail", tail]
    else:
        cmd = [tool(kind), i, o, "--preset", preset, "--bpm", bpm, "--tail", tail] + sum((["--set", f"{k}={v}"] for k, v in dict(sets).items()), [])
    run(cmd)
    y = load(o)
    if kind == "tarnish":
        # tarnishrender does not remove the processor's latency (the plugin reports it to the host, 1255 samples at
        # 48 kHz; the tool never reads it): find it against the input and take it out.
        lag = delay_of(x, y)
        y = y[lag:]
    return y

def delay_of(x, y, max_ms=200):
    """Delay of y against x (samples), by cross-correlation of the first 20 s of their mono sums."""
    from scipy.signal import correlate
    n = min(len(x), len(y), 20 * SR); a = x[:n].mean(axis=1); b = y[:n].mean(axis=1)
    c = correlate(b, a, mode="full", method="fft")[n - 1: n - 1 + int(max_ms * 1e-3 * SR)]
    return int(np.argmax(c))

def lacquer(x, tmp, name, style="punchy", intensity=60, target="loud", ceiling=-1.0, lufs=None):
    i = os.path.join(tmp, name + "_pre.wav"); o = os.path.join(tmp, name + "_lq.wav"); rep = os.path.join(tmp, name + "_lq.json")
    sf.write(i, x.astype(np.float32), SR, subtype="FLOAT")
    cmd = [tool("lacquer"), i, o, "--style", style, "--intensity", intensity, "--target", target, "--ceiling", ceiling, "--bits", 32, "--report", rep]
    if lufs is not None: cmd += ["--lufs", lufs]
    run(cmd)
    return load(o), json.load(open(rep))


# ── Mixing ──
def fit(x, n):
    return x[:n] if len(x) >= n else np.vstack([x, np.zeros((n - len(x), 2))])

def db(v): return 10 ** (v / 20)

def lufs(x):
    v = pyln.Meter(SR).integrated_loudness(x)
    return v if np.isfinite(v) else -99.0

def at_lufs(x, target):
    l = lufs(x)
    return x * db(target - l) if l > -90 else x

def hp(x, f, order=2):
    return sosfilt(butter(order, f, "highpass", fs=SR, output="sos"), x, axis=0)

def lp(x, f, order=2):
    return sosfilt(butter(order, f, "lowpass", fs=SR, output="sos"), x, axis=0)

def pan(x, p):
    """Constant-power pan of a stereo stem, p in -1..1."""
    a = (p + 1) * np.pi / 4
    return np.stack([x[:, 0] * np.cos(a) * np.sqrt(2), x[:, 1] * np.sin(a) * np.sqrt(2)], axis=1)

def width(x, w):
    mid = (x[:, 0] + x[:, 1]) / 2; side = (x[:, 0] - x[:, 1]) / 2 * w
    return np.stack([mid + side, mid - side], axis=1)

def duck(x, beats_sec, depth_db=6.0, release=0.18, attack=0.004, smooth=0.003):
    """Sidechain-style ducking on the given kick times (seconds). The release runs to 1e-4 of the depth (cut at three
    release times it left a small step) and a one-pole of a few ms on the gain rounds the corners where two kicks'
    curves meet: a kink in the gain of a sub reads as a click."""
    from scipy.signal import lfilter
    g = np.ones(len(x)); d = 1 - db(-depth_db)
    t = np.arange(int(release * 9.3 * SR)) / SR
    shape = d * np.where(t < attack, t / attack, np.exp(-(t - attack) / release))
    for s in beats_sec:
        i = int(s * SR)
        if i >= len(g): continue
        k = min(len(shape), len(g) - i)
        g[i:i + k] = np.minimum(g[i:i + k], 1 - shape[:k])
    a = np.exp(-1.0 / (smooth * SR))
    g = lfilter([1 - a], [1, -a], g - 1.0) + 1.0
    return x * g[:, None]

def mix(n, *parts):
    out = np.zeros((n, 2))
    for p in parts: out += fit(p, n)
    return out

def fade(x, fin=0.0, fout=0.0):
    y = x.copy()
    if fin > 0: k = int(fin * SR); y[:k] *= np.linspace(0, 1, k)[:, None]
    if fout > 0: k = int(fout * SR); y[-k:] *= np.linspace(1, 0, k)[:, None] ** 2
    return y


# ── Measurement and the finish ──
def true_peak_db(x):
    up = resample_poly(x, 4, 1, axis=0)
    return 20 * np.log10(max(np.abs(up).max(), 1e-12))

def peak_db(x): return 20 * np.log10(max(np.abs(x).max(), 1e-12))

def limit(x, ceiling, la_ms=3.0, rel_ms=80.0):
    """Look-ahead peak limiter on the true peak (4x oversampled, linked); returns (y, max gain reduction dB)."""
    from scipy.ndimage import minimum_filter1d
    la, rel = int(la_ms * 1e-3 * SR), np.exp(-1.0 / (rel_ms * 1e-3 * SR))
    up = np.abs(resample_poly(x, 4, 1, axis=0)).max(axis=1)
    pk = np.maximum(np.abs(x).max(axis=1), up[: 4 * len(x)].reshape(len(x), 4).max(axis=1))
    need = np.minimum(1.0, ceiling / np.maximum(pk, 1e-12))
    g_target = minimum_filter1d(need, size=2 * la + 1, origin=-la if la else 0)
    g = np.empty_like(g_target); cur = 1.0
    for i, t in enumerate(g_target):
        cur = t if t < cur else t + rel * (cur - t)
        g[i] = cur
    k = np.ones(la) / la if la > 1 else np.ones(1)
    gp = np.pad(g, (len(k) // 2, len(k) - 1 - len(k) // 2), mode="edge")
    g = np.minimum(g, np.convolve(gp, k, mode="valid"))
    return x * g[:, None], float(20 * np.log10(g.min()))

def to_target(x, target=-16.0, ceiling_db=-1.0, max_limit=5.0):
    """Gain to the target integrated loudness; a look-ahead limiter only if the true peak would pass the ceiling."""
    gain = target - lufs(x); gr = 0.0; y = x
    ceil = db(ceiling_db - 0.15)
    for _ in range(5):
        y = x * db(gain); gr = 0.0
        if true_peak_db(y) > ceiling_db: y, gr = limit(y, ceil)
        err = target - lufs(y)
        if abs(err) < 0.05: break
        gain += err
    while gr < -max_limit:
        gain -= 0.5; y = x * db(gain)
        y, gr = limit(y, ceil) if true_peak_db(y) > ceiling_db else (y, 0.0)
    return y, gr

def measure(x):
    return {"lufs": round(lufs(x), 1), "true_peak": round(true_peak_db(x), 2), "sample_peak": round(peak_db(x), 2), "secs": round(len(x) / SR, 1)}

def write(x, base, mp3=True, aac=False):
    sf.write(base + ".wav", x.astype(np.float32), SR, subtype="PCM_24")
    if mp3: run(["ffmpeg", "-y", "-loglevel", "error", "-i", base + ".wav", "-codec:a", "libmp3lame", "-q:a", "2", base + ".mp3"])
    if aac: run(["ffmpeg", "-y", "-loglevel", "error", "-i", base + ".wav", "-c:a", "aac", "-b:a", "256k", base + ".m4a"])


def clicks(x, onsets=None, thresh=12.0):
    """Spikes of the second difference far above its local level (a cut note, a jump) outside the first 30 ms of
    each note-on: count and first times. Without onsets (effect returns) it is not measured."""
    if onsets is None: return None, []
    mono = x.mean(axis=1)
    d2 = np.abs(np.diff(mono, 2))
    loc = np.sqrt(np.convolve(d2 ** 2, np.ones(480) / 480, mode="same")) + 1e-6
    lvl = np.sqrt(np.convolve(mono ** 2, np.ones(4800) / 4800, mode="same"))[1:-1]
    hit = np.where((d2 > thresh * loc) & (d2 > 0.02 * (lvl + 1e-3)) & (d2 > 1e-3))[0]
    ok = np.ones(len(d2), bool)
    for t in onsets:
        a = max(0, int((t - 0.003) * SR)); ok[a:a + int(0.033 * SR)] = False
    hit = hit[ok[hit]]
    if len(hit) == 0: return 0, []
    groups = [hit[0]] + [h for a, h in zip(hit, hit[1:]) if h - a > 480]
    return len(groups), [round(g / SR, 3) for g in groups[:6]]

def stem_report(x, onsets=None):
    mono = x.mean(axis=1); n = len(mono)
    spec = np.abs(np.fft.rfft(mono[: min(n, 1 << 22)])) ** 2; f = np.fft.rfftfreq(min(n, 1 << 22), 1 / SR)
    cen = float((spec * f).sum() / max(spec.sum(), 1e-20))
    c, at = clicks(x, onsets)
    extra = None
    if onsets is not None:
        # Onsets in the audio with no note-on near them (a synth playing its own pattern, a retrigger).
        env = np.sqrt(np.convolve(mono ** 2, np.ones(96) / 96, mode="same"))
        floor = max(env.max() * 0.1, 1e-4)
        d = np.where((env[96:] > 6 * env[:-96]) & (env[96:] > floor))[0] + 96
        if len(d):
            g = [d[0]] + [b for a, b in zip(d, d[1:]) if b - a > 2400]
            on = np.concatenate([[-1e9], np.sort(np.array(onsets, float)), [1e9]])
            t = np.array(g) / SR; k = np.searchsorted(on, t)
            # flagged: more than 120 ms after the last note-on and more than 40 ms before the next one
            extra = int(((t - on[k - 1] > 0.12) & (on[k] - t > 0.04)).sum())
    return {"lufs": round(float(lufs(x)), 1), "peak": round(float(peak_db(x)), 1), "centroid_hz": round(cen), "clicks": c, "click_at": [float(a) for a in at], "onsets_without_note": extra}
