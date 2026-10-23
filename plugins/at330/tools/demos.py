#!/usr/bin/env python3
"""Clips de demostración del AT330 para escuchar (y, en la fase 2, para la web).

    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/at330/tools/demos.py OUTDIR [--build build-at330] [--only id,id]

1. Un clip por carácter: cuerdas con y sin ensemble, las voces del coro, la capa lenta de cuerdas y coro, el vocoder
   con la voz sintética propia (tools/demo_voice.py: habla, con consonantes), Hold y Freeze, Pitch Shift y la
   diferencia entre las envolventes clásicas y las poly.
2. Dos bocetos: dark wave con la batería de Pallor, la guitarra de Thrum y el AT330 (pad y vocoder), y ambient con
   Orrery tocando el AT330; masterizados con Lacquer.

Los clips se llevan a −16 LUFS (BS.1770) con una ganancia y, si el pico pasara de −1 dBFS, un limitador de pico con
anticipación (lo que recorta queda anotado, nunca más de 3 dB). Escribe WAV (24 bits), MP3 (LAME V2) e index.md.
Los render de los otros instrumentos se buscan en las carpetas de build que existan (--build primero).
"""
import json, os, struct, subprocess, sys, tempfile
import numpy as np, soundfile as sf, pyloudnorm as pyln

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SR = 48000
TARGET = -16.0
ARGS = sys.argv[1:]
BUILD = ARGS[ARGS.index("--build") + 1] if "--build" in ARGS else "build-at330"


def find(rel_paths):
    """El primer render compilado que exista, en el build pedido o en los otros de la raíz."""
    dirs = [BUILD] + sorted(d for d in os.listdir(ROOT) if d.startswith("build") and d != BUILD)
    for d in dirs:
        for rel in rel_paths:
            p = os.path.join(ROOT, d, rel)
            if os.path.exists(p): return p
    raise FileNotFoundError(rel_paths[0])


R330 = find(["plugins/at330/at330render_artefacts/Release/at330render"])
PAL = lambda: find(["plugins/pallor/pallorrender_artefacts/Release/pallorrender"])
THR = lambda: find(["plugins/thrum/thrumrender_artefacts/Release/thrumrender"])
ORR = lambda: find(["plugins/orrery/orreryrender_artefacts/Release/orreryrender"])
LAC = lambda: find(["plugins/lacquer/lacquerrender", "plugins/lacquer/lacquerrender_artefacts/Release/lacquerrender"])
PY = sys.executable
# El volumen de cada preset (tools/headroom.py): el clip con y sin ensemble usa el de cada versión.
LEVELS = json.load(open(os.path.join(HERE, "preset_levels.json"))) if os.path.exists(os.path.join(HERE, "preset_levels.json")) else {}


def run(cmd):
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(" ".join(map(str, cmd)) + "\n" + r.stdout + r.stderr)
    return r.stdout


# ── Nivel: −16 LUFS, pico ≤ −1 dBFS (el mismo método que plugins/pallor/tools/demos.py) ──
def limit(x, ceiling, sr):
    la, rel = int(0.003 * sr), np.exp(-1.0 / (0.08 * sr))
    need = np.minimum(1.0, ceiling / np.maximum(np.max(np.abs(x), axis=1), 1e-12))
    from scipy.ndimage import minimum_filter1d
    g_target = minimum_filter1d(need, size=2 * la + 1, origin=-la if la else 0)
    g = np.empty_like(g_target); cur = 1.0
    for i, t in enumerate(g_target):
        cur = t if t < cur else t + rel * (cur - t)
        g[i] = cur
    k = np.ones(la) / la if la > 1 else np.ones(1)
    gp = np.pad(g, (len(k) // 2, len(k) - 1 - len(k) // 2), mode="edge")
    g = np.minimum(g, np.convolve(gp, k, mode="valid"))
    return x * g[:, None], float(20 * np.log10(g.min()))


def finish(src, dst_base):
    x, sr = sf.read(src, always_2d=True)
    # Sin silencio largo al final: hasta 1,5 s después de la última muestra por encima de -60 dBFS.
    loud = np.nonzero(np.max(np.abs(x), axis=1) > 10 ** (-60 / 20))[0]
    if len(loud): x = x[: min(len(x), int(loud[-1] + 1.5 * sr))]
    m = pyln.Meter(sr)
    lufs0 = m.integrated_loudness(x)
    gain, gr, y = TARGET - lufs0, 0.0, x
    for _ in range(4):
        y = x * 10 ** (gain / 20); gr = 0.0
        if np.abs(y).max() > 10 ** (-1 / 20): y, gr = limit(y, 10 ** (-1.05 / 20), sr)
        err = TARGET - m.integrated_loudness(y)
        if abs(err) < 0.05: break
        gain += err
    while gr < -3.0:
        gain -= 0.5
        y = x * 10 ** (gain / 20)
        y, gr = limit(y, 10 ** (-1.05 / 20), sr) if np.abs(y).max() > 10 ** (-1 / 20) else (y, 0.0)
    lufs, peak = m.integrated_loudness(y), 20 * np.log10(np.abs(y).max())
    sf.write(dst_base + ".wav", y, sr, subtype="PCM_24")
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", dst_base + ".wav", "-codec:a", "libmp3lame", "-q:a", "2", dst_base + ".mp3"])
    return {"lufs": round(float(lufs), 1), "peak": round(float(peak), 1), "limit": round(float(gr), 1), "secs": round(len(y) / sr, 1), "raw_lufs": round(float(lufs0), 1)}


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
    def note(self, beat, dur, n, vel=96):
        self.ev.append((beat, 1, bytes([0x90, n, vel]))); self.ev.append((beat + dur, 0, bytes([0x80, n, 0])))
    def chord(self, beat, dur, names, vel=90):
        for x in names: self.note(beat, dur, m(x) if isinstance(x, str) else x, vel)
    def cc(self, beat, num, val): self.ev.append((beat, 2, bytes([0xB0, num, val])))
    def write(self, path):
        data = bytearray(b"\x00\xFF\x51\x03" + struct.pack(">I", int(60_000_000 / self.bpm))[1:])
        last = 0
        for beat, _, msg in sorted(self.ev, key=lambda e: (e[0], e[1])):
            t = int(round(beat * 480)); data += vlq(t - last) + msg; last = t
        data += b"\x00\xFF\x2F\x00"
        with open(path, "wb") as f:
            f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(data)) + data)


def progression(bpm, chords, beats=8, gap=0.15, vel=90, top=None):
    """Una progresión de acordes (listas de notas), uno cada `beats` tiempos, y una melodía opcional arriba."""
    s = Song(bpm)
    for i, ch in enumerate(chords): s.chord(i * beats, beats - gap, ch, vel)
    for beat, dur, n in (top or []): s.note(beat, dur, m(n), vel)
    return s


def voice(tmp, name, script, bpm, offset_beats=0.0, gain=1.0):
    """La voz sintética (tools/demo_voice.py), corrida `offset_beats` tiempos."""
    raw = os.path.join(tmp, name + "_raw.wav")
    run([PY, os.path.join(HERE, "demo_voice.py"), raw, "--script", script, "--bpm", bpm])
    x, sr = sf.read(raw)
    pad = np.zeros(int(round(offset_beats * 60.0 / bpm * SR)))
    out = os.path.join(tmp, name + ".wav")
    sf.write(out, np.concatenate([pad, x]) * gain, SR, subtype="FLOAT")
    return out


def at330(tmp, cid, preset, song=None, chords=None, voice_wav=None, bpm=96, tail=4, extra=()):
    out = os.path.join(tmp, cid + ".wav")
    cmd = [R330, out, "--preset", preset, "--bpm", bpm, "--tail", tail]
    if song is not None:
        mid = os.path.join(tmp, cid + ".mid"); song.write(mid); cmd += ["--midi", mid]
    if chords: cmd += ["--chords", chords]
    if voice_wav: cmd += ["--voice", voice_wav]
    run(cmd + list(extra))
    return out


# ── Voicings (cerrados, con el bajo abajo) ──
Am = ["A2", "E3", "A3", "C4", "E4"]; F = ["F2", "C3", "A3", "C4", "F4"]; C = ["C3", "G3", "C4", "E4", "G4"]; G = ["G2", "D3", "G3", "B3", "D4"]
Dm = ["D3", "A3", "D4", "F4", "A4"]; Bb = ["Bb2", "F3", "Bb3", "D4", "F4"]; Em = ["E2", "B2", "E3", "G3", "B3"]
Fm = ["F2", "C3", "F3", "Ab3", "C4", "F4"]; Db = ["Db2", "Ab2", "F3", "Ab3", "Db4", "F4"]; Ab = ["Ab2", "Eb3", "Ab3", "C4", "Eb4"]; Eb = ["Eb2", "Bb2", "G3", "Bb3", "Eb4", "G4"]
Cm = ["C3", "G3", "C4", "Eb4", "G4"]


def clips(out, tmp, only):
    rows = []
    def clip(cid, preset, desc, **kw):
        if only and cid not in only: return
        r = finish(at330(tmp, cid, preset, **kw), os.path.join(out, cid))
        rows.append((cid, preset, desc, r)); print(cid, r)

    clip("01_strings", "Ensemble Strings", "The string section (4', four-BBD ensemble) on a slow minor progression, 72 BPM.",
         song=progression(72, [Am, F, C, G, Am, F, C, G], beats=8), bpm=72)
    clip("02_ensemble_on_off", "Dry Strings", "The same chords twice: first dry, then with the ensemble switched on at bar 9. The ensemble is most of the sound.",
         song=progression(72, [Dm, Bb, F, C, Dm, Bb, F, C], beats=8), bpm=72, extra=["--at", f"{16 * 4 * 60 / 72:.3f}", "str_ens=1", "--at", f"{16 * 4 * 60 / 72:.3f}", f"volume={LEVELS.get('Ensemble Strings', 0.0)}"])
    clip("03_male_choir", "Male Choir 8'", "Male 8' voices through the seven-filter formant bank, vibrato after its delay.",
         song=progression(66, [Dm, Bb, F, C], beats=8), bpm=66)
    clip("04_split_choir", "Female Choir", "Female 4' above the split and male 4' below, a slow line on top.",
         song=progression(70, [["C3", "G3", "E4", "G4", "C5"], ["E3", "B3", "E4", "G4", "B4"], ["F3", "C4", "F4", "A4", "C5"], ["G3", "D4", "G4", "B4", "D5"]], beats=8,
                          top=[(0, 4, "E5"), (4, 4, "D5"), (8, 6, "B4"), (16, 4, "C5"), (20, 4, "A4"), (24, 8, "D5")]), bpm=70)
    clip("05_cathedral", "Cathedral Voices", "The full choir in the large space: all four choir tablets and the extra reverb.",
         song=progression(60, [Am, Dm, Em, Am], beats=8), bpm=60, tail=7)
    clip("06_rain_on_neon", "Rain on Neon", "Slow strings with male and female voices and a long release: wide minor chords with a melody above, the late-night film-score sound.",
         song=progression(64, [Fm, Db, Ab, Eb, Fm, Db, Ab, ["Eb2", "Bb2", "G3", "Bb3", "Eb4"]], beats=8,
                          top=[(2, 6, "C5"), (10, 3, "Db5"), (13, 3, "C5"), (18, 6, "Ab4"), (26, 6, "G4"), (34, 6, "C5"), (42, 3, "F5"), (45, 3, "Eb5"), (50, 6, "C5"), (58, 6, "Bb4")]),
         bpm=64, tail=7)
    if not only or any(c.startswith(("07", "08", "09", "10")) for c in only):
        v_night = voice(tmp, "night", "night", 96, offset_beats=1)
        v_count = voice(tmp, "count", "count", 96, offset_beats=1)
        v_whisper = voice(tmp, "whisper", "whisper", 96, offset_beats=1)
        clip("07_vocoder", "Vocoder Ensemble (Audio In)", "The ten-band vocoder with the ensemble, the synthetic speaking voice (no recorded voice) over Am F C G.",
             song=progression(96, [Am, F, C, G, Am, F, C, G, Am], beats=4), voice_wav=v_night, bpm=96)
        clip("08_machine_voice", "Machine Voice (Audio In)", "Fast band response, no ensemble: the voice counting on one held chord.",
             song=progression(96, [["E3", "B3", "E4"]] * 5, beats=4, gap=0.0), voice_wav=v_count, bpm=96)
        clip("09_whisper", "Whisper Bands (Audio In)", "A whispered voice: the high consonant path carries the s, t and k only while a key is down.",
             song=progression(96, [Cm, Ab, Eb, Bb, Cm], beats=4), voice_wav=v_whisper, bpm=96)
        clip("10_hold", "Hold Pad (Audio In)", "Vocoder Hold: each band keeps its level for seconds, so the vowels hang after the voice stops.",
             song=progression(96, [Am, Em, F, C, Am, Em, F, C, Am], beats=4), voice_wav=v_night, bpm=96)
    clip("11_rising_choir", "Rising Choir", "Pitch Shift in Auto: every chord starts a fifth below and glides up to pitch.",
         song=progression(80, [Am, F, C, G, Am, F, C, G], beats=4), bpm=80)
    # Clásico contra poly: un acorde arpegiado lento, primero con las envolventes del original, después las poly.
    if not only or "12_classic_vs_poly" in only:
        s = Song(70)
        for half in range(2):
            b0 = half * 16
            for i, n in enumerate(["A2", "E3", "A3", "C4", "E4", "A4"]): s.note(b0 + i * 0.75, 12 - i * 0.75, m(n), 90)
        sw = f"{16 * 60 / 70:.3f}"
        clip("12_classic_vs_poly", "Classic Retrigger", "One slow arpeggiated chord twice: with the original's single envelope every new key re-attacks the whole chord (first half); with the poly envelopes (extra) each note swells on its own (second half).",
             song=s, bpm=70, extra=["--at", sw, "keying=1"])
    return rows


# ── Bocetos ──
def mixdown(parts, n):
    """Suma de pistas (archivo, LUFS objetivo) cortadas a n muestras."""
    mix = np.zeros((n, 2))
    for path, target in parts:
        x, sr = sf.read(path, always_2d=True)
        assert sr == SR, path
        if x.shape[1] == 1: x = np.repeat(x, 2, axis=1)
        x = x[:n] if len(x) >= n else np.vstack([x, np.zeros((n - len(x), 2))])
        l = pyln.Meter(SR).integrated_loudness(x)
        mix += x * 10 ** ((target - l) / 20) if np.isfinite(l) else x
    return mix * (0.5 / max(1e-9, np.abs(mix).max()))


def sketch_darkwave(out, tmp):
    """Dark wave en A menor a 120 BPM: batería de Pallor, guitarra de Thrum, pad y vocoder del AT330."""
    bpm, bars = 120, 32
    n = int((bars * 4 * 60 / bpm + 4) * SR)
    drums = os.path.join(tmp, "dw_drums.wav")
    run([PAL(), drums, "--preset", "Night Shift Mercury", "--bars", bars, "--tail", "3"])
    # Guitarra: la misma progresión, un acorde por compás, desde el compás 5.
    prog = ["Am", "F", "C", "G"]
    guit = os.path.join(tmp, "dw_guitar.wav")
    run([THR(), guit, "--preset", "Cold Chorus Arpeggio", "--chords", " ".join(prog[i % 4] for i in range(bars)), "--bpm", bpm, "--bars", bars, "--tail", "3"])
    g, sr = sf.read(guit, always_2d=True); g[: int(4 * 4 * 60 / bpm * SR)] *= 0.0; sf.write(guit, g, sr, subtype="FLOAT")
    # Pad del AT330: dos compases por acorde desde el compás 9, y el vocoder con la voz desde el 17.
    pad = Song(bpm)
    voic = {"Am": ["A2", "E3", "A3", "C4", "E4"], "F": ["F2", "C3", "A3", "C4", "F4"], "C": ["C3", "G3", "C4", "E4", "G4"], "G": ["G2", "D3", "G3", "B3", "D4"]}
    for bar in range(8, bars, 2): pad.chord(bar * 4, 7.8, voic[prog[(bar // 2) % 4]], 85)
    w_pad = at330(tmp, "dw_pad", "Night Shift Pad", song=pad, bpm=bpm, tail=4)
    voc = Song(bpm)
    for bar in range(16, bars): voc.chord(bar * 4, 3.9, [x for x in voic[prog[bar % 4]]][1:], 90)
    vw = voice(tmp, "dw_voice", "night", bpm, offset_beats=16 * 4)
    w_voc = at330(tmp, "dw_voc", "Vocoder Ensemble (Audio In)", song=voc, voice_wav=vw, bpm=bpm, tail=4)
    mix = mixdown([(drums, -17), (guit, -21), (w_pad, -22), (w_voc, -19)], n)
    pre = os.path.join(tmp, "dw_mix.wav"); sf.write(pre, mix, SR, subtype="FLOAT")
    mastered = os.path.join(tmp, "dw_master.wav")
    run([LAC(), pre, mastered, "--style", "gentle", "--target", "apple", "--ceiling", "-1"])
    return finish(mastered, os.path.join(out, "20_sketch_darkwave"))


def sketch_ambient(out, tmp):
    """Ambient: Orrery genera (Black Lake) y el AT330 toca el pad y el drone; masterizado con Lacquer."""
    secs = 75
    od = os.path.join(tmp, "orrery"); os.makedirs(od, exist_ok=True)
    run([ORR(), os.path.join(ROOT, "plugins/orrery/assets/presets/p06_black_lake.json"), od, "--secs", secs, "--seed", "3"])
    lanes = {int(f[4:-4]): os.path.join(od, f) for f in os.listdir(od) if f.startswith("lane") and f.endswith(".mid")}
    parts = []
    if 2 in lanes: parts.append((at330(tmp, "amb_pad", "Frozen Cathedral", bpm=48, tail=10, extra=["--midi", lanes[2]]), -18))
    if 1 in lanes: parts.append((at330(tmp, "amb_drone", "Low Monks", bpm=48, tail=10, extra=["--midi", lanes[1]]), -22))
    if 5 in lanes: parts.append((at330(tmp, "amb_cloud", "Glass Hall", bpm=48, tail=10, extra=["--midi", lanes[5]]), -24))
    n = int((secs + 8) * SR)
    mix = mixdown(parts, n)
    pre = os.path.join(tmp, "amb_mix.wav"); sf.write(pre, mix, SR, subtype="FLOAT")
    mastered = os.path.join(tmp, "amb_master.wav")
    run([LAC(), pre, mastered, "--style", "gentle", "--target", "apple", "--ceiling", "-1"])
    return finish(mastered, os.path.join(out, "21_sketch_ambient"))


def main():
    out = os.path.abspath(ARGS[0])
    only = set(ARGS[ARGS.index("--only") + 1].split(",")) if "--only" in ARGS else set()
    os.makedirs(out, exist_ok=True)
    rows = []
    with tempfile.TemporaryDirectory(dir=out) as tmp:
        rows += clips(out, tmp, only)
        if not only or "20_sketch_darkwave" in only:
            r = sketch_darkwave(out, tmp); print("20", r)
            rows.append(("20_sketch_darkwave", "Night Shift Pad + Vocoder Ensemble", "Dark wave sketch, A minor, 120 BPM: Pallor 'Night Shift Mercury' drums, Thrum guitar (chorus arpeggio) from bar 5, AT330 pad from bar 9 and the vocoder with the synthetic voice from bar 17; Lacquer master.", r))
        if not only or "21_sketch_ambient" in only:
            r = sketch_ambient(out, tmp); print("21", r)
            rows.append(("21_sketch_ambient", "Frozen Cathedral, Low Monks, Glass Hall", "Ambient sketch: Orrery 'Black Lake' (48 BPM, E) generates the parts and the AT330 plays them: pad, low male drone and the high cloud; Lacquer master.", r))
    idx = os.path.join(out, "clips.json")
    old = json.load(open(idx)) if os.path.exists(idx) else {}
    for cid, preset, desc, r in rows: old[cid] = {"preset": preset, "desc": desc, **r}
    json.dump(old, open(idx, "w"), indent=1)
    with open(os.path.join(out, "index.md"), "w") as f:
        f.write("# AT330 demos\n\nRendered with `plugins/at330/tools/demos.py` (at330render, 48 kHz). Every clip is levelled to −16 LUFS (BS.1770) "
                "by gain; where the peak would pass −1 dBFS a look-ahead peak limiter catches it (column *Limit*, never more than 3 dB). "
                "The vocoder clips use a synthetic voice made by `tools/demo_voice.py` (no recorded voice).\n\n")
        f.write("| Clip | Preset | What it is | LUFS | Peak dBFS | Limit dB | Length | Raw LUFS |\n|---|---|---|---|---|---|---|---|\n")
        for cid in sorted(old):
            r = old[cid]
            f.write(f"| `{cid}` (.wav, .mp3) | {r['preset']} | {r['desc']} | {r['lufs']} | {r['peak']} | {r['limit']} | {r['secs']} s | {r['raw_lufs']} |\n")
    print("index:", os.path.join(out, "index.md"))


if __name__ == "__main__":
    main()
