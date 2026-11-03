#!/usr/bin/env python3
"""Sample Pack de Audio Tools: render, normalización, QA, zips y previews.

    python3 scripts/samplepack.py build at20v [at101v ...]   renderiza una carpeta por sinte
    python3 scripts/samplepack.py qa                         mide todo lo renderizado
    python3 scripts/samplepack.py zip                        un zip por sinte y el pack completo
    python3 scripts/samplepack.py previews                   m4a de preescucha y web/assets/samplepack/pack.json

Se corre con el Python del venv de audio (numpy, soundfile). Todo sale de nuestros plugins; ver samplepack_content.py.
"""
import hashlib, json, os, shutil, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from samplepack_lib import *
import samplepack_content as C

PACK = os.path.join(PRIV, "pack")
VERSION = "1.0"
LICENSE_TXT = {
 "en": ("These samples are released under CC0 1.0 (public domain dedication): use them in any project, commercial or not, "
        "with no credit needed. Every sound was generated with Audio Tools plugins. No third-party samples or melodies."),
 "es": ("Estas muestras se publican bajo CC0 1.0 (dedicación al dominio público): usalas en cualquier proyecto, comercial o no, "
        "sin necesidad de dar crédito. Todos los sonidos se generaron con plugins de Audio Tools. No hay samples ni melodías de terceros."),
}

def S_(spec): return spec["folder"]
def upper(spec): return spec["folder"].upper()

def base_name(spec, lp, idx):
    key = f"_{lp['key']}" if lp["key"] else ""
    return f"{spec['folder']}_{lp['id']}_{lp['bpm']}bpm{key}_{idx:02d}"

def merge_songs(songs):
    out = Song(songs[0].bpm)
    for s in songs: out.ev.extend(s.ev)
    return out

# ───────── one-shots ─────────
def build_oneshots(spec, out_dir, log):
    groups = {}
    for o in spec["oneshots"]:
        g = groups.setdefault(o["label"], dict(preset=o["preset"], hold=o["hold"], tail=o["tail"], extra=o["extra"], combos=[]))
        g["plain"] = o.get("plain", False)
        for n in o["notes"]:
            for v in o["vels"]:
                if (n, v) not in g["combos"]: g["combos"].append((n, v))
    made = []
    for label, g in groups.items():
        slot = g["hold"] + g["tail"]
        s = Song(120)
        spb = 2.0      # negras por segundo a 120 BPM
        for i, (n, v) in enumerate(g["combos"]):
            for nn in n.split("+"): s.note(i * slot * spb, g["hold"] * spb, m(nn), v)
        total = len(g["combos"]) * slot + 0.5
        x = render_midi(spec["slug"], g["preset"], s, total, extra=g["extra"], bpm=120)
        parts = []
        for i, (n, v) in enumerate(g["combos"]):
            seg = x[int(i * slot * SR):int((i + 1) * slot * SR)]
            parts.append((n, v, seg))
        pk = max(peak_db(seg) for _, _, seg in parts)
        gdb = min(-3.0 - pk, TP_CAP - max(tp_db(seg) for _, _, seg in parts))
        for n, v, seg in parts:
            y = trim_tail(gain_db(seg, gdb), thresh_db=-54, min_len=0.12, fade=0.12)
            y = fade_in(remove_dc(y), 1.0)
            name = f"{spec['folder']}_{label}_v{v}" if g["plain"] else f"{spec['folder']}_{label}_{n.replace('+', '')}_v{v}"
            wr(os.path.join(out_dir, "OneShots", name + ".wav"), y)
            made.append(name)
        log(f"  one-shots {label}: {len(parts)}  pico {pk:+.1f} -> -3.0")
    return made

# ───────── loops ─────────
def render_loop(spec, lp):
    L = loop_len(lp["bpm"], lp["nbars"])
    secs = L / SR + lp["tail"]
    if lp["mode"] == "midi":
        acc, songs = None, []
        for preset, bars, gdb in lp["parts"]:
            s = song_from(lp["bpm"], bars, lp["swing"], lp["gate"], lp["vel"])
            songs.append(s)
            x = gain_db(render_midi(spec["slug"], preset, s, secs, extra=lp["extra"], bpm=lp["bpm"]), gdb)
            acc = x if acc is None else acc + x
        return fold_loop(acc, L), merge_songs(songs)
    args = list(lp["args"]) + ["--seconds", f"{secs:.3f}", "--bpm", str(lp["bpm"])]
    x = render_cli(spec["slug"], args, secs)
    return fold_loop(x, L), None

def build_loops(spec, out_dir, log, store):
    for i, lp in enumerate(spec["loops"], 1):
        y, song = render_loop(spec, lp)
        y = close_seam(remove_dc(y))
        y, g = norm_loop(y)
        y = remove_dc(y)
        name = base_name(spec, lp, i)
        wr(os.path.join(out_dir, "Loops", name + ".wav"), y)
        if song is not None:
            os.makedirs(os.path.join(out_dir, "MIDI"), exist_ok=True)
            song.write(os.path.join(out_dir, "MIDI", name + ".mid"))
        store[lp["id"]] = (name, y, lp)
        log(f"  loop {name}: {len(y) / SR:5.2f} s  {lufs(y):6.1f} LUFS  pico {peak_db(y):+.1f}  costura {seam_click(y):.2f}  silencio inicial {lead_silence(y):.3f}")

# ───────── efectos ─────────
def build_fx(spec, out_dir, log, store):
    for f in spec["fx"]:
        kind, src = f["src"]
        if kind == "loop":
            name, dry, lp = store[src]
            w = wet_loop(f["kind"], dry, f["preset"], lp["bpm"])
            w = close_seam(remove_dc(w))
            w, g = norm_loop(w)
            w = remove_dc(w)
            fn = f"{name}_{f['tag']}"
            wr(os.path.join(out_dir, "Fx_Wet", fn + ".wav"), w)
            f["file"], f["loop"] = fn, True
            log(f"  fx {fn}: {len(w) / SR:5.2f} s  {lufs(w):6.1f} LUFS  pico {peak_db(w):+.1f}  costura {seam_click(w):.2f}")
        elif kind == "os":
            dry = rd(os.path.join(out_dir, "OneShots", src + ".wav"))
            y = fx_process(f["kind"], dry, f["preset"], tail=6.0)
            y = fade_in(trim_tail(y, thresh_db=-54, min_len=0.5, fade=0.4)[:int(9 * SR)], 1.0)
            y, g = norm_peak(remove_dc(y), -3.0)
            fn = f"{src}_{f['tag']}"
            wr(os.path.join(out_dir, "Fx_Wet", fn + ".wav"), y)
            f["file"], f["loop"] = fn, False
            log(f"  fx {fn}: {len(y) / SR:5.2f} s  pico {peak_db(y):+.1f}")
        else:   # textura granular de un loop
            name, dry, lp = store[src]
            y = fx_process(f["kind"], np.vstack([dry, dry]), f["preset"], tail=5.0, bpm=lp["bpm"])
            y = trim_tail(trim_head(y), thresh_db=-52, min_len=2.0, fade=1.5)[:int(14 * SR)]
            y[-int(1.8 * SR):] *= np.linspace(1, 0, int(1.8 * SR), dtype=np.float32)[:, None]
            y = fade_in(y, 40.0)
            y, g = norm_loop(remove_dc(y))
            key = f"_{lp['key']}" if lp["key"] else ""
            fn = f"{spec['folder']}_{src}_{f['tag']}{key}_Texture"
            wr(os.path.join(out_dir, "Fx_Wet", fn + ".wav"), y)
            f["file"], f["loop"] = fn, False
            log(f"  fx {fn}: {len(y) / SR:5.2f} s  {lufs(y):6.1f} LUFS  pico {peak_db(y):+.1f}")

# ───────── README ─────────
def write_readme(spec, out_dir):
    files = {d: sorted(os.listdir(os.path.join(out_dir, d))) for d in ("OneShots", "Loops", "Fx_Wet", "MIDI") if os.path.isdir(os.path.join(out_dir, d))}
    fxm = os.path.join(PRIV, f"fxmeta-{spec['slug']}.json")
    fxkind = {f["file"]: ("loop" if f.get("loop") else "other") for f in json.load(open(fxm))["fx"]} if os.path.exists(fxm) else {}
    if fxkind: pass
    lines = []
    for lang, title in (("en", "SAMPLE PACK"), ("es", "SAMPLE PACK")):
        t = spec["tag"][lang]
        lines += [f"AUDIO TOOLS {title} - {spec['name']}", "=" * 60, t, "",
                  ("License: " if lang == "en" else "Licencia: ") + LICENSE_TXT[lang], "",
                  ("Format: WAV, 24 bit, 44.1 kHz, stereo. Loops close without clicks (tails folded into the start); each loop is "
                   "named with its tempo and key. Wet files were processed by Halation (echo and rooms), Tarnish (tape, cassette, vinyl) "
                   "and Murmur (granular). The ones marked (loop) in the list close without a click." if lang == "en" else
                   "Formato: WAV, 24 bits, 44,1 kHz, estéreo. Los loops cierran sin clics (las colas se pliegan sobre el comienzo); cada loop lleva "
                   "su tempo y su tonalidad en el nombre. Los archivos mojados pasaron por Halation (ecos y salas), Tarnish (cinta, casete, vinilo) "
                   "y Murmur (granular). Los marcados (loop) en la lista cierran sin clic."), "",
                  ("Folders: OneShots/ single notes and hits, Loops/ dry loops, Fx_Wet/ the same material with effects" + (", MIDI/ the MIDI of each loop." if "MIDI" in files else ".")
                   if lang == "en" else
                   "Carpetas: OneShots/ notas y golpes sueltos, Loops/ loops secos, Fx_Wet/ el mismo material con efectos" + (", MIDI/ el MIDI de cada loop." if "MIDI" in files else ".")), ""]
        lines.append(("CONTENTS (loop = closes without a click; texture = free-running, no tempo)" if lang == "en" else "CONTENIDO (loop = cierra sin clic; textura = libre, sin tempo)"))
        for d, fl in files.items():
            lines.append(f"  {d}/ ({len([f for f in fl if f.endswith('.wav') or f.endswith('.mid')])})")
            for f in fl:
                tag = ""
                if d == "Fx_Wet":
                    tag = "   (loop)" if fxkind.get(f[:-4]) == "loop" else ("   (" + ("textura" if lang == "es" else "texture") + ")" if f.endswith("Texture.wav") else "")
                elif d == "Loops": tag = "   (loop)"
                lines.append(f"    {f}{tag}")
        lines += ["", "-" * 60, ""]
    open(os.path.join(out_dir, "README.txt"), "w").write("\n".join(lines))

# ───────── build ─────────
def build(slug):
    spec = next(s for s in C.SYNTHS if s["slug"] == slug)
    out_dir = os.path.join(PACK, spec["folder"])
    if os.path.isdir(out_dir): shutil.rmtree(out_dir)
    os.makedirs(WORK, exist_ok=True)
    logf = open(os.path.join(PRIV, f"build-{slug}.log"), "w")
    def log(s): print(s); logf.write(s + "\n"); logf.flush()
    log(f"== {spec['name']}")
    store = {}
    build_oneshots(spec, out_dir, log)
    build_loops(spec, out_dir, log, store)
    build_fx(spec, out_dir, log, store)
    write_readme(spec, out_dir)
    json.dump(dict(slug=slug, fx=spec["fx"]), open(os.path.join(PRIV, f"fxmeta-{slug}.json"), "w"), indent=1)
    wavs = [f for _, _, fs in os.walk(out_dir) for f in fs if f.endswith(".wav")]
    log(f"  total: {len(wavs)} WAV, {sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(out_dir) for f in fs) / 1e6:.1f} MB")

# ───────── QA ─────────
def estimate_key(y):
    import librosa
    mono = y.mean(1)
    ch = librosa.feature.chroma_cqt(y=mono, sr=SR).mean(1)
    maj = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
    mnr = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])
    best = (-2, "")
    for k in range(12):
        for prof, suf in ((maj, ""), (mnr, "m")):
            c = float(np.corrcoef(np.roll(prof, k), ch)[0, 1])
            if c > best[0]: best = (c, NAMES[k] + suf)
    return best[1], best[0]

def qa():
    rows, flags = [], []
    for folder in sorted(os.listdir(PACK)):
        d = os.path.join(PACK, folder)
        if not os.path.isdir(d): continue
        spec = next((s for s in C.SYNTHS if s["folder"] == folder), None)
        for sub in ("OneShots", "Loops", "Fx_Wet"):
            sd = os.path.join(d, sub)
            if not os.path.isdir(sd): continue
            for f in sorted(os.listdir(sd)):
                if not f.endswith(".wav"): continue
                x = rd(os.path.join(sd, f))
                info = sf.info(os.path.join(sd, f))
                is_loop = sub != "OneShots" and "bpm" in f and "Texture" not in f
                r = dict(synth=folder, sub=sub, file=f, secs=round(len(x) / SR, 2), peak=round(peak_db(x), 2), clip=clip_count(x),
                         lead=round(lead_silence(x), 3), dc=round(float(np.abs(x.mean(0)).max()), 5), sr=info.samplerate, subtype=info.subtype)
                if sub != "OneShots": r["lufs"] = round(lufs(x), 1)
                if is_loop: r["seam"] = round(seam_click(x), 2)
                end = float(np.abs(x[-int(0.01 * SR):]).max())
                r["end"] = round(20 * math.log10(max(end, 1e-9)), 1)
                rows.append(r)
                fl = []
                if r["peak"] > -1.0: fl.append("pico")
                if r["clip"]: fl.append("clip")
                if r["peak"] < -40: fl.append("silencio")
                if sub == "OneShots" and r["lead"] > 0.03: fl.append("arranque tardio")
                if sub != "OneShots" and r["lead"] > 0.6: fl.append("silencio inicial")
                if r.get("seam", 0) > 1.5: fl.append("costura")
                if r["dc"] > 0.003: fl.append("DC")
                if not re.fullmatch(r"[A-Za-z0-9_.#+-]+", f): fl.append("nombre no ASCII")
                if r["sr"] != SR or r["subtype"] != "PCM_24": fl.append("formato")
                if sub != "OneShots" and r.get("lufs", -16) < -21: fl.append("bajo")
                if sub == "OneShots" and r["end"] > -50: fl.append("corte")
                if fl: flags.append((folder, sub, f, fl))
    json.dump(rows, open(os.path.join(PRIV, "qa.json"), "w"), indent=1)
    print(len(rows), "archivos;", len(flags), "con avisos")
    for fl in flags: print("  ", *fl)
    return rows

def keys():
    """Compara la tonalidad declarada de cada loop con la que estima el analisis de croma."""
    for spec in C.SYNTHS:
        for i, lp in enumerate(spec["loops"], 1):
            if not lp["key"]: continue
            p = os.path.join(PACK, spec["folder"], "Loops", base_name(spec, lp, i) + ".wav")
            if not os.path.exists(p): continue
            k, c = estimate_key(rd(p))
            flag = "" if k == lp["key"] or (k.rstrip("m") == lp["key"].rstrip("m")) else "   <-- distinto"
            print(f"{spec['folder']:10s} {lp['id']:14s} declarada {lp['key']:4s} estimada {k:4s} ({c:.2f}){flag}")

# ───────── previews, zips y metadatos para la web ─────────
WEBDL = os.path.join(ROOT, "web", "downloads")
WEBPREV = os.path.join(ROOT, "web", "assets", "samplepack")
DEMOS_SRC = os.path.join(ROOT, "web", "assets", "samples")
ZIP_DATE = (2026, 10, 31, 0, 0, 0)

def pretty_fx(tag):
    eng = {"Hal": "Halation", "Tar": "Tarnish", "Mur": "Murmur"}[tag[:3]]
    words = re.sub(r"(?<=[a-z])(?=[A-Z0-9])|(?<=[0-9])(?=[A-Z])", " ", tag[3:]).strip()
    return eng, words

def short(desc):
    t = re.split(r"[,.]", desc)[0].strip()
    return t[0].upper() + t[1:] if t else desc

def preview_encode(x, out_m4a, tmp_wav):
    """AAC 160 kbps a -16 LUFS (pico maximo -1,5 dB), fundido de 30 ms al final; devuelve (segundos, picos)."""
    pad = x if len(x) > 2 * SR else np.vstack([x, np.zeros((2 * SR - len(x), 2), np.float32)])
    g = -16.0 - lufs(pad)
    g = min(g, -1.5 - tp_db(x))
    y = gain_db(x, g)
    n = int(0.03 * SR); y[-n:] *= np.linspace(1, 0, n, dtype=np.float32)[:, None]
    sf.write(tmp_wav, np.clip(y, -1, 1), SR, subtype="PCM_24")
    os.makedirs(os.path.dirname(out_m4a), exist_ok=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp_wav, "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", out_m4a], check=True)
    return round(len(y) / SR, 1), S.peaks(tmp_wav)

def count_files(d):
    return {sub: len([f for f in os.listdir(os.path.join(d, sub)) if f.endswith(".wav")]) for sub in ("OneShots", "Loops", "Fx_Wet") if os.path.isdir(os.path.join(d, sub))}

def build_previews():
    """Elige 6 muestras por sinte, las codifica en web/assets/samplepack/<slug>/ y devuelve su lista."""
    out = {}
    for spec in C.SYNTHS:
        d = os.path.join(PACK, spec["folder"])
        fx = json.load(open(os.path.join(PRIV, f"fxmeta-{spec['slug']}.json")))["fx"]
        n = len(spec["loops"])
        idx = sorted({0, n // 3, (2 * n) // 3})
        items = []
        for i in idx:
            lp = spec["loops"][i]; name = base_name(spec, lp, i + 1)
            key = f" - {lp['key']}" if lp["key"] else ""
            te, ts = C.TITLES[f"{spec['slug']}/{lp['id']}"]
            items.append(dict(kind="loop", file=name, path=os.path.join(d, "Loops", name + ".wav"), tile=0,
                              title=dict(en=f"{te} · {lp['bpm']} BPM{key}", es=f"{ts} · {lp['bpm']} BPM{key}"),
                              desc=dict(en="Dry loop: " + lp["en"], es="Loop seco: " + lp["es"])))
        wet = [f for f in fx if f["loop"]]
        pick = [wet[0]] + ([wet[len(wet) // 2]] if len(wet) > 2 else wet[1:2])
        for f in pick:
            eng, words = pretty_fx(f["tag"])
            src = next(l for k, l in ((base_name(spec, l, j + 1), l) for j, l in enumerate(spec["loops"])) if f["file"].startswith(k))
            key = f" - {src['key']}" if src["key"] else ""
            te, ts = C.TITLES[f"{spec['slug']}/{src['id']}"]
            items.append(dict(kind="wet", file=f["file"], path=os.path.join(d, "Fx_Wet", f["file"] + ".wav"), tile=0,
                              title=dict(en=f"{te} + {eng} {words} · {src['bpm']} BPM{key}", es=f"{ts} + {eng} {words} · {src['bpm']} BPM{key}"),
                              desc=dict(en="With effect: " + f["en"], es="Con efecto: " + f["es"])))
        tex = [f for f in fx if not f["loop"] and f["file"].endswith("Texture")]
        if tex:
            f = tex[0]; eng, words = pretty_fx(f["tag"])
            items.append(dict(kind="texture", file=f["file"], path=os.path.join(d, "Fx_Wet", f["file"] + ".wav"), tile=1,
                              title=dict(en=f"{eng} {words}: texture", es=f"{eng} {words}: textura"), desc=dict(en="Granular texture. " + f["en"], es="Textura granular. " + f["es"])))
        else:
            ol = sorted(os.listdir(os.path.join(d, "OneShots")))
            f = C.PREVIEW_OS.get(spec["slug"]) or ol[len(ol) // 2][:-4]
            nm = re.sub(r"(?<=[a-z])(?=[A-Z0-9])", " ", re.sub(r"_v\d+$", "", f.split("_", 1)[1]).replace("_", " "))
            items.append(dict(kind="oneshot", file=f, path=os.path.join(d, "OneShots", f + ".wav"), tile=1,
                              title=dict(en="One-shot: " + nm, es="One-shot: " + nm),
                              desc=dict(en="A single note or hit from the OneShots folder.", es="Una nota o golpe suelto de la carpeta OneShots.")))
        res = []
        with tempfile.TemporaryDirectory(dir=WORK) as td:
            for it in items:
                x = rd(it["path"])
                tile = it["tile"] or (2 if len(x) < 10 * SR else 1)       # un loop corto se oye dos veces
                if tile > 1: x = np.vstack([x] * tile)
                m4a = os.path.join(WEBPREV, spec["slug"], it["file"] + ".m4a")
                secs, pk = preview_encode(x, m4a, os.path.join(td, "p.wav"))
                res.append(dict(kind=it["kind"], file=it["file"], src=f"{spec['slug']}/{it['file']}.m4a", title=it["title"], desc=it["desc"], secs=secs, peaks=pk,
                                kb=round(os.path.getsize(m4a) / 1024)))
        out[spec["slug"]] = res
        print(spec["folder"], [r["secs"] for r in res])
    return out

def readme_all(synths, demos):
    L = []
    for lang in ("en", "es"):
        L += [("AUDIO TOOLS SAMPLE PACK" if lang == "en" else "AUDIO TOOLS SAMPLE PACK") + f" {VERSION}", "=" * 60,
              ("Original one-shots, loops and effected variations made only with the Audio Tools plugins: " if lang == "en" else
               "One-shots, loops y variaciones con efectos originales, hechas solo con los plugins de Audio Tools: ") + ", ".join(s["name"] for s in synths) +
              (", with Halation, Tarnish and Murmur for the effects." if lang == "en" else ", con Halation, Tarnish y Murmur para los efectos."), "",
              ("License: " if lang == "en" else "Licencia: ") + LICENSE_TXT[lang], "",
              ("Format: WAV 24 bit / 44.1 kHz stereo (loops at about -16 LUFS, peaks below -1 dBFS). Each folder has OneShots/, Loops/, Fx_Wet/ and, when the loops are played from MIDI, MIDI/. "
               "Loop names carry tempo and key; they close without clicks." if lang == "en" else
               "Formato: WAV 24 bits / 44,1 kHz estéreo (loops a unos -16 LUFS, picos por debajo de -1 dBFS). Cada carpeta tiene OneShots/, Loops/, Fx_Wet/ y, si los loops se tocan desde MIDI, MIDI/. "
               "Los nombres de los loops llevan tempo y tonalidad; cierran sin clics."), "",
              ("Demos/ has the demo recordings from the Audio Tools site (AAC 160 kbps) with their MIDI files." if lang == "en" else
               "Demos/ trae las grabaciones de demostración del sitio de Audio Tools (AAC 160 kbps) con sus archivos MIDI."), "",
              ("FOLDERS" if lang == "en" else "CARPETAS")]
        for s in synths: L.append(f"  {s['folder']}/  {s['tag'][lang]}")
        L += ["", "https://audiotools.com.ar/samplepack/" if lang == "en" else "https://audiotools.com.ar/es/samplepack/", "", "-" * 60, ""]
    return "\n".join(L)

def zip_dir(zf, src, arc_root):
    for r, _, fs in os.walk(src):
        for f in sorted(fs):
            if f.startswith("."): continue
            full = os.path.join(r, f)
            rel = os.path.join(arc_root, os.path.relpath(full, src))
            zi = zipfile.ZipInfo(rel.replace(os.sep, "/"), ZIP_DATE); zi.compress_type = zipfile.ZIP_DEFLATED; zi.external_attr = 0o644 << 16
            with open(full, "rb") as fh, zf.open(zi, "w", force_zip64=True) as w: shutil.copyfileobj(fh, w, 1 << 20)

def zip_text(zf, arc, text):
    zi = zipfile.ZipInfo(arc, ZIP_DATE); zi.compress_type = zipfile.ZIP_DEFLATED; zi.external_attr = 0o644 << 16
    zf.writestr(zi, text)

def build_zips():
    os.makedirs(WEBDL, exist_ok=True)
    info = {}
    for spec in C.SYNTHS:
        name = f"AudioTools-SamplePack-{spec['folder']}.zip"
        path = os.path.join(WEBDL, name)
        with zipfile.ZipFile(path, "w") as zf:
            zip_dir(zf, os.path.join(PACK, spec["folder"]), f"AudioTools-SamplePack-{spec['folder']}")
        info[spec["slug"]] = dict(zip=name)
        print(name, round(os.path.getsize(path) / 1e6, 1), "MB")
    # pack completo: todas las carpetas + Demos/
    name = "AudioTools-SamplePack-All.zip"
    path = os.path.join(WEBDL, name)
    root = "AudioTools-SamplePack"
    ndemo = 0
    with zipfile.ZipFile(path, "w") as zf:
        zip_text(zf, f"{root}/README.txt", readme_all(C.SYNTHS, True))
        for spec in C.SYNTHS: zip_dir(zf, os.path.join(PACK, spec["folder"]), f"{root}/{spec['folder']}")
        for plug in sorted(os.listdir(DEMOS_SRC)):
            d = os.path.join(DEMOS_SRC, plug)
            if not os.path.isdir(d): continue
            for f in sorted(os.listdir(d)):
                if f.endswith(".m4a") or f.endswith(".mid"):
                    zi = zipfile.ZipInfo(f"{root}/Demos/{plug}/{f}", ZIP_DATE); zi.compress_type = zipfile.ZIP_STORED if f.endswith(".m4a") else zipfile.ZIP_DEFLATED; zi.external_attr = 0o644 << 16
                    with open(os.path.join(d, f), "rb") as fh, zf.open(zi, "w", force_zip64=True) as w: shutil.copyfileobj(fh, w, 1 << 20)
                    ndemo += 1
        zip_text(zf, f"{root}/Demos/README.txt", "Demo recordings from the Audio Tools site (AAC 160 kbps, about -16 LUFS) and the MIDI files that drive them, "
                 "one folder per plugin. They were made only with Audio Tools plugins and are released under CC0 1.0 like the rest of the pack.\n\n"
                 "Grabaciones de demostración del sitio de Audio Tools (AAC 160 kbps, unos -16 LUFS) y los archivos MIDI que las tocan, una carpeta por plugin. "
                 "Se hicieron solo con plugins de Audio Tools y se publican bajo CC0 1.0 como el resto del pack.\n")
    info["all"] = dict(zip=name, demos=ndemo)
    print(name, round(os.path.getsize(path) / 1e6, 1), "MB", ndemo, "demos")
    return info

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

def assemble():
    prev = build_previews()
    if "--keep-zips" in sys.argv:       # solo previews y metadatos: reusa los zips ya armados
        zi = {x["slug"]: dict(zip=f"AudioTools-SamplePack-{x['folder']}.zip") for x in C.SYNTHS}
        zi["all"] = dict(zip="AudioTools-SamplePack-All.zip", demos=len([1 for _, _, fs in os.walk(DEMOS_SRC) for f in fs if f.endswith((".m4a", ".mid"))]))
    else:
        zi = build_zips()
    packs = []
    total_files = 0
    for spec in C.SYNTHS:
        d = os.path.join(PACK, spec["folder"])
        cnt = count_files(d)
        nmidi = len(os.listdir(os.path.join(d, "MIDI"))) if os.path.isdir(os.path.join(d, "MIDI")) else 0
        z = os.path.join(WEBDL, zi[spec["slug"]]["zip"])
        total_files += sum(cnt.values())
        packs.append(dict(slug=spec["slug"], name=spec["name"], folder=spec["folder"], tag=spec["tag"], genres=spec["genres"], counts=cnt, midi=nmidi,
                          zip=zi[spec["slug"]]["zip"], bytes=os.path.getsize(z), sha256=sha256(z), previews=prev[spec["slug"]]))
    z = os.path.join(WEBDL, zi["all"]["zip"])
    meta = dict(version=VERSION, packs=packs, all=dict(zip=zi["all"]["zip"], bytes=os.path.getsize(z), sha256=sha256(z), samples=total_files, demos=zi["all"]["demos"],
                midi=sum(p["midi"] for p in packs)))
    os.makedirs(WEBPREV, exist_ok=True)
    json.dump(meta, open(os.path.join(WEBPREV, "pack.json"), "w"), indent=1, ensure_ascii=False)
    print("samples:", total_files, "pack completo:", round(meta["all"]["bytes"] / 1e6, 1), "MB")

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "build":
        for s in sys.argv[2:] or [x["slug"] for x in C.SYNTHS]: build(s)
    elif cmd == "readmes":
        for sp in C.SYNTHS: write_readme(sp, os.path.join(PACK, sp["folder"]))
    elif cmd == "assemble": assemble()
    elif cmd == "previews": build_previews()
    elif cmd == "qa": qa()
    elif cmd == "keys": keys()
    else:
        raise SystemExit(__doc__)
