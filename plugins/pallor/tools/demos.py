#!/usr/bin/env python3
"""Clips de demostración de Pallor para escuchar (y, en la fase 2, para la web).

    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/pallor/tools/demos.py OUTDIR [--only id,id]

1. Un clip por género: un preset de fábrica (con su cadena de canción o su fill automático) por pallorrender.
2. Dos recorridos: la caja por los cuatro tipos de sala de los 80 (seca, gated, no lineal, al revés) y un mismo
   patrón pasando por los ocho kits digitales.
3. Tres bocetos de 60-90 s: Pallor con bajo y pads de los otros instrumentos de Audio Tools (atmdrender,
   atfs1render, por MIDI), los pads por Halation, la mezcla por Tarnish y masterizada con Lacquer.

Los clips se llevan a −16 LUFS (BS.1770) con una ganancia y, si el pico pasara de −1 dBFS, un limitador de pico
con anticipación (lo que recorta queda anotado). Escribe WAV (24 bits) y MP3 (LAME V2) e index.md con lo medido.
Necesita numpy, soundfile, pyloudnorm, ffmpeg y los render CLI compilados (cmake --build build --target
pallorrender atmdrender atfs1render halationrender tarnishrender lacquerrender).
"""
import json, os, struct, subprocess, sys, tempfile
import numpy as np, soundfile as sf, pyloudnorm as pyln

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
B = os.path.join(ROOT, "build", "plugins")
VR = os.path.join(B, "pallor", "pallorrender_artefacts", "Release", "pallorrender")
ATMD = os.path.join(B, "atmd", "atmdrender_artefacts", "Release", "atmdrender")
ATFS1 = os.path.join(B, "atfs1", "atfs1render_artefacts", "Release", "atfs1render")
HAL = os.path.join(B, "halation", "halationrender_artefacts", "Release", "halationrender")
TAR = os.path.join(B, "tarnish", "tarnishrender_artefacts", "Release", "tarnishrender")
LAC = os.path.join(B, "lacquer", "lacquerrender")
PRE = lambda plug, f: os.path.join(ROOT, "plugins", plug, "assets", "presets", f)
SR = 48000
TARGET = -16.0


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(" ".join(cmd) + "\n" + r.stdout + r.stderr)
    return r.stdout


def pallor(out, preset, bars, extra=()):
    run([VR, out, "--preset", preset, "--bars", str(bars), "--tail", "2.5", *extra])


# ── Nivel: −16 LUFS, pico ≤ −1 dBFS ──
def limit(x, ceiling, sr):
    """Limitador de pico con 3 ms de anticipación y 80 ms de liberación (ganancia por muestra, enlazada)."""
    la, rel = int(0.003 * sr), np.exp(-1.0 / (0.08 * sr))
    pk = np.max(np.abs(x), axis=1)
    need = np.minimum(1.0, ceiling / np.maximum(pk, 1e-12))
    # mínimo móvil hacia adelante (la ganancia baja antes del pico)
    from scipy.ndimage import minimum_filter1d
    g_target = minimum_filter1d(need, size=2 * la + 1, origin=-la if la else 0)
    g = np.empty_like(g_target); cur = 1.0
    for i, t in enumerate(g_target):
        cur = t if t < cur else t + rel * (cur - t)
        g[i] = cur
    # suavizado del ataque (la ventana de anticipación como rampa)
    k = np.ones(la) / la if la > 1 else np.ones(1)
    gp = np.pad(g, (len(k) // 2, len(k) - 1 - len(k) // 2), mode="edge")   # bordes con su propio valor, no con ceros
    g = np.minimum(g, np.convolve(gp, k, mode="valid"))
    return x * g[:, None], float(20 * np.log10(g.min()))


def finish(src, dst_base, sr=SR):
    x, sr = sf.read(src, always_2d=True)
    m = pyln.Meter(sr)
    lufs0 = m.integrated_loudness(x)
    # Ganancia y limitador, repetidos hasta que el resultado (ya limitado) quede en el objetivo.
    gain, gr, y = TARGET - lufs0, 0.0, x
    for _ in range(4):
        y = x * 10 ** (gain / 20)
        gr = 0.0
        if np.abs(y).max() > 10 ** (-1 / 20):
            y, gr = limit(y, 10 ** (-1.05 / 20), sr)
        err = TARGET - m.integrated_loudness(y)
        if abs(err) < 0.05: break
        gain += err
    # Los transitorios no se aplastan más de 3 dB: un clip muy ralo queda un poco por debajo de −16 LUFS.
    while gr < -3.0:
        gain -= 0.5
        y = x * 10 ** (gain / 20)
        y, gr = limit(y, 10 ** (-1.05 / 20), sr) if np.abs(y).max() > 10 ** (-1 / 20) else (y, 0.0)
    x = y
    lufs = m.integrated_loudness(x)
    peak = 20 * np.log10(np.abs(x).max())
    sf.write(dst_base + ".wav", x, sr, subtype="PCM_24")
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", dst_base + ".wav", "-codec:a", "libmp3lame", "-q:a", "2", dst_base + ".mp3"])
    return {"lufs": round(float(lufs), 1), "peak": round(float(peak), 1), "limit": round(float(gr), 1), "secs": round(len(x) / sr, 1), "raw_lufs": round(float(lufs0), 1)}


# ── MIDI (SMF tipo 0, 480 ticks por negra) ──
class Song:
    def __init__(self, bpm): self.bpm = bpm; self.ev = []
    def note(self, beat, dur, n, vel=96):
        self.ev.append((beat, 1, bytes([0x90, n, vel]))); self.ev.append((beat + dur, 0, bytes([0x80, n, 0])))
    def chord(self, beat, dur, ns, vel=80):
        for n in ns: self.note(beat, dur, n, vel)
    def write(self, path):
        data = bytearray(b"\x00\xFF\x51\x03" + struct.pack(">I", int(60_000_000 / self.bpm))[1:])
        last = 0
        for beat, _, msg in sorted(self.ev, key=lambda e: (e[0], e[1])):
            t = int(round(beat * 480)); data += vlq(t - last) + msg; last = t
        data += b"\x00\xFF\x2F\x00"
        with open(path, "wb") as f:
            f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(data)) + data)


def vlq(n):
    out = [n & 0x7F]; n >>= 7
    while n: out.insert(0, (n & 0x7F) | 0x80); n >>= 7
    return bytes(out)


NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}
def m(name):
    i = 2 if len(name) > 2 and name[1] in "#b" else 1
    return NOTE[name[:i]] + 12 * (int(name[i:]) + 1)


def load(path, n=None):
    x, sr = sf.read(path, always_2d=True)
    assert sr == SR, (path, sr)
    if x.shape[1] == 1: x = np.repeat(x, 2, axis=1)
    if n is not None:
        x = x[:n] if len(x) >= n else np.vstack([x, np.zeros((n - len(x), 2))])
    return x


def lufs_of(x): return pyln.Meter(SR).integrated_loudness(x)


def at_lufs(x, target):
    l = lufs_of(x)
    return x * 10 ** ((target - l) / 20) if np.isfinite(l) else x


# ── Los clips por género ──
GENRES = [
    ("01_darkwave", "Night Shift Mercury", 16, "Dark wave: bridged-T kick, gated 8-bit snare, chorused hats; song chain A/B with fills."),
    ("02_coldwave", "Thin Ice Lines", 16, "Coldwave: the hexagonal kit at 8 bits, flanged hats, cold nonlinear room."),
    ("03_postpunk", "Floor Tom Sermon", 16, "Post-punk: tribal floor toms around the backbeat, room and tape, auto fill every 4."),
    ("04_minimalwave", "Preset Rhumba Ghost", 16, "Minimal wave: the cheap analog box, bossa into rock, through the 12-bit sampler."),
    ("05_ebm", "Sweat Engine", 16, "EBM: dark kick, clap backbeat, sixteenth hats, clanking cowbell; chain with fills."),
    ("06_synthpop", "Neon Promenade", 16, "Synthpop: the 1982-style acoustic sample kit with claps and tambourine."),
    ("07_gothrock", "Vigil Machine", 16, "Goth rock: VCO kit, gallop kick, gated toms in the fills."),
    ("08_industrial", "Broken Conveyor", 16, "Industrial: the foundry kit (sheets, pipes, chains) with a jet flanger."),
    ("09_newbeat", "Dense Smoke 108", 16, "New beat: 12-bit 26 kHz kit at 108 BPM, gated clap, noise sweeps."),
    ("10_italo", "Hi-Energy Ghost", 16, "Italo / hi-NRG: four on the floor, offbeat open hats, hexagonal 'pew' toms."),
    ("11_witchhouse", "Triangle Static", 8, "Witch house: 70 BPM, hat rolls by ratchet, sub drops, reverse room, 11 kHz crush."),
    ("12_triphop", "Rainy Breakbeat", 8, "90s darkwave / trip-hop: swung break on the 12-bit sampler kit with ghost notes."),
    ("13_darkambient", "Catacomb Drift", 8, "Dark ambient: chance-driven ride and metal, tape echo and a hall."),
]


def genre_clips(out, tmp, only):
    rows = []
    for cid, preset, bars, desc in GENRES:
        if only and cid not in only: continue
        raw = os.path.join(tmp, cid + ".wav")
        pallor(raw, preset, bars)
        r = finish(raw, os.path.join(out, cid))
        rows.append((cid, preset, desc, r))
        print(cid, r)
    return rows


def tours(out, tmp, only):
    rows = []
    if not only or "14_gated_tour" in only:
        # La caja de "Night Shift Mercury" en cuatro salas, 4 compases cada una: seca, gated, no lineal, al revés.
        raw = os.path.join(tmp, "g.wav")
        spb = 60.0 / 120 * 4
        extra = ["--set", "mode=0", "--set", "sd_senda=70", "--set", "a_return=-60", "--set", "cp_senda=0"]
        for vo in ("ch", "oh", "tb", "cp", "t1", "t2", "t3", "t4"):
            extra += ["--set", f"{vo}_mute=1"]
        for i, (typ, ret) in enumerate([(0, -60), (0, 0), (1, 0), (2, 0)]):
            t = f"{i * 4 * spb:.4f}"
            extra += ["--at", t, f"a_type={typ}", "--at", t, f"a_return={ret}"]
        extra += ["--at", "0", "a_gate=280", "--at", f"{8 * spb:.4f}", "a_gate=320", "--at", f"{12 * spb:.4f}", "a_gate=300"]
        pallor(raw, "Night Shift Mercury", 16, extra)
        r = finish(raw, os.path.join(out, "14_gated_tour"))
        print("14_gated_tour", r)
        rows.append(("14_gated_tour", "Night Shift Mercury", "Kick and snare only; the snare through the four 80s rooms, 4 bars each: dry, gated (280 ms), nonlinear (320 ms), reverse.", r))
    if not only or "15_kit_tour" in only:
        # El patrón de "Neon Promenade" pasando por los ocho kits digitales, 2 compases cada uno.
        raw = os.path.join(tmp, "k.wav")
        spb = 60.0 / 118 * 4
        extra = ["--set", "mode=0"]
        for k in range(8):
            t = f"{k * 2 * spb:.4f}"
            for vo in ("bd", "sd", "cp", "ch", "oh", "tb", "cb", "t1", "t2", "t3", "t4", "rs"):
                extra += ["--at", t, f"{vo}_kit={k}", "--at", t, f"{vo}_engine=1"]
        pallor(raw, "Neon Promenade", 16, extra)
        r = finish(raw, os.path.join(out, "15_kit_tour"))
        print("15_kit_tour", r)
        rows.append(("15_kit_tour", "Neon Promenade", "One pattern through the eight built-in digital kits, 2 bars each: Studio 82, Pop 84, Electro 81, Hexa 84, Crunch 88, Studio 91, Foundry, FM 85.", r))
    if not only or "16_analog_tour" in only:
        # Los modelos analógicos del bombo y la caja: un compás cada combinación.
        raw = os.path.join(tmp, "a.wav")
        spb = 60.0 / 124 * 4
        extra = ["--set", "mode=0"]
        for i in range(5):
            t = f"{i * 2 * spb:.4f}"
            extra += ["--at", t, f"bd_model={i}", "--at", t, f"sd_model={i}", "--at", t, "bd_engine=0", "--at", t, "sd_engine=0"]
        pallor(raw, "Steel Boots", 10, extra)
        r = finish(raw, os.path.join(out, "16_analog_tour"))
        print("16_analog_tour", r)
        rows.append(("16_analog_tour", "Steel Boots", "Analog kick and snare models, 2 bars each: bridged-T, VCO punch, compact box, hexagonal, dark (kick) / crack (snare).", r))
    return rows


# ── Bocetos ──
def sketch(out, tmp, cid, title, desc, preset, bpm, bars, bass, pad, chords, bass_pattern, mute_kick=(), tape="Warm Studio Tape", hal="Cathedral of Glass"):
    beat = 60.0 / bpm
    n = int((bars * 4 * beat + 3.0) * SR)
    # Batería: el preset en modo canción, con el bombo fuera en el puente.
    drums = os.path.join(tmp, cid + "_drums.wav")
    extra = []
    for a, b in mute_kick:
        extra += ["--at", f"{a * 4 * beat:.4f}", "bd_mute=1", "--at", f"{b * 4 * beat:.4f}", "bd_mute=0"]
    pallor(drums, preset, bars, extra)
    # Bajo y pad por MIDI.
    sb, sp = Song(bpm), Song(bpm)
    for bar in range(bars):
        ch = chords[(bar // 2) % len(chords)]
        root = m(ch[0])
        if bar >= 4:
            for i, (step, octv, dur, vel) in enumerate(bass_pattern):
                sb.note(bar * 4 + step * 0.25, dur * 0.25 * 0.9, root + 12 * octv, vel)
        if bar >= 8 and bar % 2 == 0 and not any(a <= bar < b for a, b in mute_kick[:0]):
            sp.chord(bar * 4, 7.8, [m(x) for x in ch[1]], 70)
    mb, mp = os.path.join(tmp, cid + "_bass.mid"), os.path.join(tmp, cid + "_pad.mid")
    sb.write(mb); sp.write(mp)
    wb, wp, wpr = os.path.join(tmp, cid + "_bass.wav"), os.path.join(tmp, cid + "_pad.wav"), os.path.join(tmp, cid + "_padrev.wav")
    run([ATMD, PRE("atmd", bass), wb, "--midi", mb, "--bpm", str(bpm), "--tail", "3"])
    if pad[0] == "atfs1": run([ATFS1, PRE("atfs1", pad[1]), wp, "--midi", mp])
    else: run([ATMD, PRE("atmd", pad[1]), wp, "--midi", mp, "--bpm", str(bpm), "--tail", "4"])
    run([HAL, wp, wpr, "--preset", hal, "--tail", "3", "--bpm", str(bpm)])
    D, Bs, Pd = load(drums, n), load(wb, n), load(wpr, n)
    mix = at_lufs(D, -17) + at_lufs(Bs, -22) + at_lufs(Pd, -27)
    mix *= 0.5 / max(1e-9, np.abs(mix).max())
    pre = os.path.join(tmp, cid + "_mix.wav"); sf.write(pre, mix, SR, subtype="FLOAT")
    taped = os.path.join(tmp, cid + "_tape.wav")
    run([TAR, pre, taped, "--preset", tape, "--tail", "0"])
    mastered = os.path.join(tmp, cid + "_master.wav")
    run([LAC, taped, mastered, "--style", "gentle", "--target", "apple", "--ceiling", "-1"])
    r = finish(mastered, os.path.join(out, cid))
    print(cid, r)
    return (cid, preset, f"{title}: {desc}", r)


def sketches(out, tmp, only):
    rows = []
    S = [
        dict(cid="20_sketch_sodium", title="Sodium", preset="Night Shift Mercury", bpm=120, bars=36, bass="p05_rubber_bass.json", pad=("atfs1", "p14_glass_choir.json"),
             chords=[("A1", ["A3", "C4", "E4"]), ("F1", ["A3", "C4", "F4"]), ("C2", ["G3", "C4", "E4"]), ("G1", ["G3", "B3", "D4"])],
             bass_pattern=[(0, 0, 2, 110), (2, 1, 2, 80), (4, 0, 2, 100), (6, 0, 2, 80), (8, 0, 2, 105), (10, 1, 2, 80), (12, 0, 2, 100), (14, 0, 2, 80)],
             mute_kick=[(20, 24)], desc="dark wave sketch, A minor, 120 BPM. Pallor 'Night Shift Mercury' (song chain) with a rubber bass from ATMD and a glass choir from ATFS1 through Halation; Tarnish tape on the mix, Lacquer master. Kick drops out for bars 21-24."),
        dict(cid="21_sketch_steel", title="Steel Parade", preset="Sweat Engine", bpm=124, bars=32, bass="p04_squelch_bass.json", pad=("atfs1", "p20_res_brass.json"),
             chords=[("D1", ["D3", "F3", "A3"]), ("D1", ["D3", "F3", "A3"]), ("Bb0", ["D3", "F3", "Bb3"]), ("C1", ["E3", "G3", "C4"])],
             bass_pattern=[(i, 1 if i % 4 == 3 else 0, 1, 115 if i % 4 == 0 else 80) for i in range(16)],
             mute_kick=[(16, 18)], desc="EBM sketch, D minor, 124 BPM. Pallor 'Sweat Engine' under a sixteenth-note squelch bass from ATMD and resonant brass from ATFS1; Tarnish, Lacquer."),
        dict(cid="22_sketch_nave", title="Graveyard Nave", preset="Vigil Machine", bpm=138, bars=40, bass="p03_plucked_bass.json", pad=("atmd", "p28_poly_strings.json"),
             chords=[("E1", ["E3", "G3", "B3"]), ("C2", ["E3", "G3", "C4"]), ("A1", ["E3", "A3", "C4"]), ("B1", ["D#3", "F#3", "B3"])],
             bass_pattern=[(0, 0, 2, 110), (2, 0, 2, 90), (4, 0, 2, 100), (6, 1, 2, 90), (8, 0, 2, 105), (10, 0, 2, 90), (12, 0, 2, 100), (14, 1, 2, 90)],
             mute_kick=[(24, 26)], hal="Concert Hall", desc="goth rock / post-punk sketch, E minor, 138 BPM. Pallor 'Vigil Machine' with a plucked ATMD bass and ATMD poly strings in Halation's hall; Tarnish, Lacquer."),
    ]
    for s in S:
        if only and s["cid"] not in only: continue
        rows.append(sketch(out, tmp, **s))
    return rows


def main():
    out = sys.argv[1]
    only = set(sys.argv[sys.argv.index("--only") + 1].split(",")) if "--only" in sys.argv else set()
    os.makedirs(out, exist_ok=True)
    rows = []
    with tempfile.TemporaryDirectory(dir=out) as tmp:
        rows += genre_clips(out, tmp, only)
        rows += tours(out, tmp, only)
        rows += sketches(out, tmp, only)
    idx = os.path.join(out, "clips.json")
    old = json.load(open(idx)) if os.path.exists(idx) else {}
    for cid, preset, desc, r in rows:
        old[cid] = {"preset": preset, "desc": desc, **r}
    json.dump(old, open(idx, "w"), indent=1)
    with open(os.path.join(out, "index.md"), "w") as f:
        f.write("# Pallor demos\n\nRendered with `plugins/pallor/tools/demos.py` (pallorrender, 48 kHz). Every clip is levelled to −16 LUFS "
                "(BS.1770) by gain; where the peak would pass −1 dBFS a look-ahead peak limiter catches it (column *Limit*, never more than 3 dB: "
                "very sparse clips stay a little under −16 LUFS rather than have their transients flattened).\n\n")
        f.write("| Clip | Preset | What it is | LUFS | Peak dBFS | Limit dB | Length | Raw LUFS |\n|---|---|---|---|---|---|---|---|\n")
        for cid in sorted(old):
            r = old[cid]
            f.write(f"| `{cid}` (.wav, .mp3) | {r['preset']} | {r['desc']} | {r['lufs']} | {r['peak']} | {r['limit']} | {r['secs']} s | {r['raw_lufs']} |\n")
    print("index:", os.path.join(out, "index.md"))


if __name__ == "__main__":
    main()
