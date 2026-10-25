#!/usr/bin/env python3
"""DeVicenti demo clips (1.0.0): short and sound-first, one sound or effect each (clips.py). `--full` renders the
phase 1 song demos instead (demo_songs.py, kept in audiotools-private/vicenti/demos/2026-10-25-full/).

    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/devicenti/tools/demos.py OUTDIR [--only 01,04] [--build build-devicenti]

1. clips.py (or demo_songs.py with --full) writes the projects (tools/demo_projects/*.json).
2. devicentirender plays each one (the song chain with a simulated host clock; the jam as MIDI into the plugin).
3. Mastering: a fixed gain to the genre's loudness target (−16 LUFS; −20 LUFS for ambient and dark ambient, which need
   the dynamic range) and, only if the true peak would pass −1 dBTP, a look-ahead true-peak limiter (what it took is
   written down). WAV 24-bit and MP3 192k (LAME), both measured again.
4. Checks per clip: loudness, true peak (WAV and MP3), sample peak, clipped samples, DC per channel, HF-burst click
   candidates, and an image (waveform, spectrogram, zoom on the strongest candidate). index.md lists everything.
"""
import json, os, subprocess, sys
import numpy as np, soundfile as sf, pyloudnorm as pyln
from scipy.signal import resample_poly, butter, sosfilt
from scipy.ndimage import minimum_filter1d, uniform_filter1d

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
SR = 48000
CEIL = -1.0


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(" ".join(cmd) + "\n" + r.stdout + r.stderr)
    return r.stdout


def lufs(x):
    return pyln.Meter(SR).integrated_loudness(x.T)


def true_peak(x):
    return 20 * np.log10(np.abs(resample_poly(x, 4, 1, axis=1)).max() + 1e-12)


def release_follow(g, rel):
    out = np.empty_like(g); cur = 1.0
    for i, v in enumerate(g):
        cur = v if v < cur else min(v, cur + (1.0 - cur) * rel)
        out[i] = cur
    return out


def tp_limit(x, ceil_db, look_ms=2.5, rel_ms=120.0):
    ceil = 10 ** (ceil_db / 20)
    up = np.abs(resample_poly(x, 4, 1, axis=1)).max(axis=0)
    pk = up[: (len(up) // 4) * 4].reshape(-1, 4).max(axis=1)
    pk = np.concatenate([pk, np.zeros(max(0, x.shape[1] - len(pk)))])[: x.shape[1]]
    g = np.minimum(1.0, ceil / np.maximum(pk, 1e-9))
    w = int(look_ms * SR / 1000)
    g = minimum_filter1d(g, size=2 * w + 1, mode="nearest")
    g = uniform_filter1d(g, size=w, mode="nearest")
    g = minimum_filter1d(g, size=w, mode="nearest")
    g = release_follow(g, 1.0 / (rel_ms * SR / 1000))
    return x * g[None, :], float(1 - g.min())


def trim_tail(x, thresh_db=-72.0, fade_s=0.6):
    env = np.abs(x).max(axis=0)
    idx = np.where(env > 10 ** (thresh_db / 20))[0]
    end = min(x.shape[1], (idx[-1] if len(idx) else x.shape[1]) + int(0.25 * SR))
    x = x[:, :end].copy()
    nf = min(int(fade_s * SR), x.shape[1])
    x[:, -nf:] *= np.linspace(1, 0, nf) ** 2
    return x


def master(x, target):
    x = trim_tail(x)
    gain = target - lufs(x)
    x = x * 10 ** (gain / 20)
    gr = 0.0
    for _ in range(5):
        if true_peak(x) <= CEIL - 0.05:
            break
        x, g = tp_limit(x, CEIL - 0.15)
        gr = max(gr, g)
        x = x * 10 ** ((target - lufs(x)) / 20)
    return x, round(gain, 2), round(-20 * np.log10(max(1 - gr, 1e-9)), 2)


def decode_mp3(path):
    out = subprocess.run(["ffmpeg", "-v", "quiet", "-i", path, "-f", "f32le", "-ac", "2", "-ar", str(SR), "-"], capture_output=True, check=True).stdout
    return np.frombuffer(out, dtype=np.float32).reshape(-1, 2).T.astype(np.float64)


def click_candidates(x, top=5):
    m = x.mean(axis=0)
    h = sosfilt(butter(4, 9000, "hp", fs=SR, output="sos"), m)
    fr = int(0.001 * SR); nfr = len(h) // fr
    e = (h[: nfr * fr].reshape(nfr, fr) ** 2).mean(axis=1) + 1e-14
    loc = uniform_filter1d(e, size=151, mode="nearest")
    ratio = 10 * np.log10(e / loc); ratio[10 * np.log10(e) < -70] = 0
    out = []
    for i in np.argsort(ratio)[::-1]:
        if all(abs(i - j) > 50 for j, _ in out):
            out.append((int(i), float(ratio[i])))
        if len(out) >= top:
            break
    return [(round(i * 0.001, 3), round(r, 1)) for i, r in out]


def image(x, name, outdir, res):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(3, 1, figsize=(14, 9), gridspec_kw=dict(height_ratios=[1, 1.6, 1]))
    t = np.arange(x.shape[1]) / SR; dec = max(1, x.shape[1] // 20000)
    ax[0].plot(t[::dec], x[0, ::dec], lw=0.4, color="#c0502a"); ax[0].plot(t[::dec], -np.abs(x[1, ::dec]), lw=0.4, color="#2a6fc0", alpha=0.6)
    ax[0].set_xlim(0, t[-1]); ax[0].set_ylim(-1, 1); ax[0].set_title(f"{name} | {res['lufs']} LUFS {res['true_peak_dbtp']} dBTP")
    ax[1].specgram(x.mean(axis=0), NFFT=2048, Fs=SR, noverlap=1536, cmap="magma", vmin=-140, vmax=-20, scale="dB"); ax[1].set_ylim(0, 24000)
    c = res["click_candidates"]; tc = c[0][0] if c else 1.0
    i0, i1 = max(0, int((tc - 0.02) * SR)), min(x.shape[1], int((tc + 0.02) * SR))
    ax[2].plot(t[i0:i1], x[0, i0:i1], lw=0.7, color="#c0502a"); ax[2].plot(t[i0:i1], x[1, i0:i1], lw=0.7, color="#2a6fc0", alpha=0.6)
    ax[2].set_title(f"zoom on the strongest HF-burst candidate t={tc:.3f} s ({c[0][1] if c else 0} dB)")
    fig.tight_layout(); fig.savefig(f"{outdir}/{name}.png", dpi=80); plt.close(fig)


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 1
    outdir = os.path.abspath(sys.argv[1]); os.makedirs(outdir, exist_ok=True)
    only = None
    if "--only" in sys.argv:
        only = sys.argv[sys.argv.index("--only") + 1].split(",")
    build = sys.argv[sys.argv.index("--build") + 1] if "--build" in sys.argv else "build-devicenti"
    R = os.path.join(ROOT, build, "plugins", "devicenti", "devicentirender_artefacts", "Release", "devicentirender")
    if "--full" in sys.argv:
        import demo_songs
        demos = [d + (None,) for d in demo_songs.write_all()]
    else:
        import clips
        demos = clips.write_all()
    rows = []
    for slug, path, secs, target, proj, tail_s in demos:
        if only and not any(slug.startswith(o) for o in only):
            continue
        raw = os.path.join(outdir, f"_raw_{slug}.wav")
        if slug == "07-controller-jam":
            mid = os.path.join(outdir, "07-controller-jam.mid")
            secs = demo_songs.jam_midi(mid)
            out = run([R, raw, "--project", path, "--midi", mid, "--midi-channels", "--tail", "4"])
        else:
            tail = tail_s if tail_s is not None else (14.0 if target <= -20 else 5.0)
            out = run([R, raw, "--project", path, "--song", "--seconds", f"{secs:.3f}", "--tail", str(tail)])
        cpu = float(out.split("CPU ")[1].split(" %")[0])
        x, _ = sf.read(raw); x = x.T.astype(np.float64)
        raw_peak = 20 * np.log10(np.abs(x).max() + 1e-12)
        m, gain, gr = master(x, target)
        wav = os.path.join(outdir, slug + ".wav"); mp3 = os.path.join(outdir, slug + ".mp3")
        sf.write(wav, m.T, SR, subtype="PCM_24")
        subprocess.run(["lame", "--quiet", "-b", "192", "--cbr", "-q", "2", "--noreplaygain", wav, mp3], check=True)
        y = decode_mp3(mp3)
        res = dict(name=slug, title=proj["name"], description=proj["description"], seconds=round(m.shape[1] / SR, 1), lufs=round(lufs(m), 1),
                   true_peak_dbtp=round(true_peak(m), 2), mp3_lufs=round(lufs(y), 1), mp3_true_peak_dbtp=round(true_peak(y), 2),
                   sample_peak_dbfs=round(20 * np.log10(np.abs(m).max() + 1e-12), 2), clipped=int((np.abs(m) >= 0.999).sum()),
                   dc=[round(float(v), 6) for v in m.mean(axis=1)], raw_peak_dbfs=round(raw_peak, 2), gain_db=gain, limiter_gr_db=gr,
                   cpu_pct=cpu, target_lufs=target, click_candidates=click_candidates(m))
        image(m, slug, outdir, res)
        with open(os.path.join(outdir, slug + ".json"), "w") as f:
            json.dump(res, f, indent=1)
        os.remove(raw)
        rows.append(res)
        print(f"{slug}: {res['seconds']} s, {res['lufs']} LUFS (target {target}), {res['true_peak_dbtp']} dBTP, MP3 {res['mp3_true_peak_dbtp']} dBTP, "
              f"raw peak {raw_peak:.1f} dBFS, limiter {gr} dB, CPU {cpu:.1f} %, clicks? {res['click_candidates'][:2]}")
    # index.md (merged with the rows of earlier runs).
    idx = os.path.join(outdir, "index.json")
    allrows = {}
    if os.path.exists(idx):
        for r in json.load(open(idx)):
            allrows[r["name"]] = r
    for r in rows:
        allrows[r["name"]] = r
    json.dump([allrows[k] for k in sorted(allrows)], open(idx, "w"), indent=1)
    with open(os.path.join(outdir, "index.md"), "w") as f:
        f.write("# DeVicenti demo clips (1.0.0)\n\nShort, sound-first clips: each shows one DeVicenti sound or effect, and each is also a factory preset of "
                "the same name. Rendered by `devicentirender` from `plugins/devicenti/tools/demo_projects/` (written by `clips.py`). The phase 1 song demos are "
                "in `2026-10-25-full/`. 48 kHz / 24-bit WAV and MP3 192k. Loudness by a fixed gain to the target; a true-peak limiter only where the "
                "peak would pass −1 dBTP (column *Limiter*).\n\n")
        f.write("| Clip | Length | LUFS (target) | True peak WAV / MP3 | Raw peak | Limiter | CPU | What it shows |\n|---|---|---|---|---|---|---|---|\n")
        for k in sorted(allrows):
            r = allrows[k]
            f.write(f"| `{r['name']}` | {r['seconds']} s | {r['lufs']} ({r['target_lufs']}) | {r['true_peak_dbtp']} / {r['mp3_true_peak_dbtp']} dBTP | {r['raw_peak_dbfs']} dBFS | "
                    f"{r['limiter_gr_db']} dB | {r['cpu_pct']} % | {r['description']} |\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
