#!/usr/bin/env python3
"""Film de la web de un plugin: arma las tomas renderizadas con un mismo acabado para todos.

    python3 scripts/film.py <plugin> <carpeta de cuadros> [--draft] [--no-render-audio]

Lee plugins/<plugin>/tools/film.json:

    {
      "title": "Plumb", "tagline": "Sing in tune. Feel no delay.",
      "fps": 24,
      "bpm": 120,                     # opcional: los cortes caen en tiempos del compás
      "audio": {"file": "web/assets/samples/plumb/natural-lead.m4a", "start": 0.0}
            o {"cmd": ["build/...render", "{out}", "--preset", "X", ...]},   # {out} = wav a escribir
      "shots": [                      # en orden; la duración en tiempos (con bpm) o en segundos
        {"name": "hero", "beats": 8}, {"name": "close", "seconds": 3.5, "in": "dissolve"}
      ],
      "poster": {"shot": "hero", "frame": 40}
    }

Cada toma son cuadros JPEG o PNG `<carpeta>/<name>_0001.jpg`… a `fps`; si la toma tiene menos cuadros que su
duración, se congela el último. El acabado:
  - Apertura: el nombre (Archivo condensada, como en la web) y la frase entran sobre la primera toma, abajo a la
    izquierda, y se van antes del primer corte.
  - Cortes secos en los tiempos del compás; "in": "dissolve" pide un fundido de medio tiempo.
  - Cierre: la última toma se oscurece y aparecen el nombre, "Free and open source" y audiotools.com.ar.
  - Color: curva en S suave, viñeta y grano fino iguales en todos los films; audio a −16 LUFS con fundidos.
Escribe web/assets/video/<plugin>.mp4 (H.264, 1080p) y web/assets/img/<plugin>/film-poster.jpg.
"""
import json, os, re, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "shared", "film", "fonts", "Archivo[wdth,wght].ttf")
W, H = 1920, 1080


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        sys.exit(f"falló: {' '.join(map(str, cmd))[:300]}\n{r.stderr[-2000:]}")
    return r.stdout


def accent_of(slug):
    src = open(os.path.join(ROOT, "scripts", "build-web.py"), encoding="utf-8").read()
    m = re.search(r'"%s":\s*"(#[0-9a-fA-F]{6})"' % re.escape(slug), src[src.index("ACCENT = "):])
    return m.group(1) if m else "#e0e0e0"


def font(size, weight, width):
    from PIL import ImageFont
    f = ImageFont.truetype(FONT, size)
    f.set_variation_by_axes([weight, width])
    return f


def text_layer(path, lines, anchor):
    """PNG transparente de 1920x1080 con líneas (texto, tamaño, peso, ancho, color, espacio antes). anchor:
    "lower-left" (apertura) o "center" (cierre). Sombra suave para leerse sobre cualquier fondo."""
    from PIL import Image, ImageDraw, ImageFilter
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d, ds = ImageDraw.Draw(img), ImageDraw.Draw(shadow)
    blocks = []
    for (s, size, wght, wdth, col, gap) in lines:
        f = font(size, wght, wdth)
        x0, y0, x1, y1 = d.textbbox((0, 0), s, font=f)
        blocks.append((s, f, col, gap, x1 - x0, y1 - y0, y0))
    total = sum(b[3] + b[5] for b in blocks)
    y = (H - 120 - total) if anchor == "lower-left" else (H - total) / 2
    for (s, f, col, gap, w, h, oy) in blocks:
        y += gap
        x = 120 if anchor == "lower-left" else (W - w) / 2
        ds.text((x, y - oy), s, font=f, fill=(0, 0, 0, 170))
        d.text((x, y - oy), s, font=f, fill=col)
        y += h
    shadow = shadow.filter(ImageFilter.GaussianBlur(18))
    Image.alpha_composite(shadow, img).save(path)


def main():
    args = sys.argv[1:]
    if len(args) < 2: sys.exit(__doc__)
    slug, frames = args[0], os.path.abspath(args[1])
    draft = "--draft" in args
    spec = json.load(open(os.path.join(ROOT, "plugins", slug, "tools", "film.json"), encoding="utf-8"))
    fps = spec.get("fps", 24); bpm = spec.get("bpm")
    beat = 60.0 / bpm if bpm else 0.5
    acc = accent_of(slug)
    tmp = tempfile.mkdtemp(prefix=f"film-{slug}-", dir="/tmp/claude-501" if os.path.isdir("/tmp/claude-501") else None)

    # ── Duraciones: en tiempos de compás si hay bpm, redondeadas a cuadros ──
    shots = []
    for s in spec["shots"]:
        dur = s["beats"] * beat if "beats" in s else s["seconds"]
        shots.append(dict(s, dur=round(dur * fps) / fps))
    total = sum(s["dur"] for s in shots)

    # ── Cada toma a un clip de la duración exacta (congela el último cuadro si faltan) ──
    clips = []
    for s in shots:
        pat = None
        for ext in ("jpg", "png"):
            if os.path.exists(os.path.join(frames, f"{s['name']}_0001.{ext}")): pat = os.path.join(frames, f"{s['name']}_%04d.{ext}")
        if pat is None: sys.exit(f"faltan los cuadros de la toma {s['name']} en {frames}")
        n = int(round(s["dur"] * fps)) + (int(round(beat * fps)) if s is not shots[-1] else 0)   # cola para el fundido
        out = os.path.join(tmp, f"{s['name']}.mp4")
        run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(fps), "-start_number", str(s.get("first", 1)), "-i", pat,
             "-vf", f"scale={W}:{H}:flags=lanczos,tpad=stop_mode=clone:stop=-1,format=yuv420p", "-frames:v", str(n),
             "-c:v", "libx264", "-crf", "12" if not draft else "22", "-preset", "slow" if not draft else "veryfast", out])
        clips.append(out)

    # ── Montaje: cortes secos o fundidos de medio tiempo, sobre la grilla del compás ──
    inputs, fc, last, t = [], [], "0:v", 0.0
    for i, c in enumerate(clips): inputs += ["-i", c]
    for i in range(1, len(clips)):
        t += shots[i - 1]["dur"]
        kind = shots[i].get("in", "cut")
        dd = 0.5 * beat if kind == "dissolve" else 1.0 / fps
        # El fundido empieza en el corte y se come la cola de la toma anterior (cada clip trae un tiempo de más), así
        # las tomas que siguen no se adelantan respecto del audio. Para que eso cierre, el xfade tiene que recibir la
        # toma anterior ya recortada a su duración más el fundido.
        fc.append(f"[{last}]trim=0:{t + dd:.4f},setpts=PTS-STARTPTS[p{i}];"
                  f"[p{i}][{i}:v]xfade=transition=fade:duration={dd:.4f}:offset={t:.4f}[x{i}]")
        last = f"x{i}"
    # Acabado de color: curva en S suave, un poco de saturación, viñeta y grano fino.
    grade = ("curves=all='0/0 0.06/0.04 0.5/0.5 0.94/0.96 1/1',eq=saturation=1.04,vignette=angle=PI/5:mode=forward,"
             "noise=alls=3:allf=t,unsharp=5:5:0.35")
    fc.append(f"[{last}]trim=0:{total:.4f},setpts=PTS-STARTPTS,{grade}[g]")

    # ── Textos: apertura abajo a la izquierda y cierre centrado ──
    title_png, end_png = os.path.join(tmp, "title.png"), os.path.join(tmp, "end.png")
    text_layer(title_png, [(spec["title"], 150, 700, 62, (245, 243, 238, 255), 0),
                           (spec["tagline"], 54, 600, 74, (245, 243, 238, 235), 14)], "lower-left")
    text_layer(end_png, [(spec["title"], 190, 700, 62, (245, 243, 238, 255), 0),
                         ("Free and open source", 46, 600, 74, (245, 243, 238, 230), 26),
                         ("audiotools.com.ar", 34, 600, 100, tuple(int(acc[i:i + 2], 16) for i in (1, 3, 5)) + (255,), 34)], "center")
    t_in, t_out = 0.6, min(shots[0]["dur"] - 0.4, 3.6)
    e_in = max(total - 3.2, total * 0.7)
    n = len(clips)
    inputs += ["-loop", "1", "-t", f"{total:.3f}", "-i", title_png, "-loop", "1", "-t", f"{total:.3f}", "-i", end_png]
    fc.append(f"[{n}:v]format=rgba,fade=t=in:st={t_in}:d=0.7:alpha=1,fade=t=out:st={t_out - 0.5:.3f}:d=0.5:alpha=1[ti]")
    fc.append(f"[{n + 1}:v]format=rgba,fade=t=in:st={e_in + 0.5:.3f}:d=0.8:alpha=1[en]")
    # La última toma se oscurece bajo el cierre.
    fc.append(f"[g]split[ga][gb];[gb]trim=start={e_in:.3f},setpts=PTS-STARTPTS,boxblur=8:1,colorlevels=romax=0.32:gomax=0.32:bomax=0.32,"
              f"format=yuva420p,fade=t=in:st=0:d=1.2:alpha=1,setpts=PTS+{e_in:.3f}/TB[dark];[ga][dark]overlay=eof_action=pass[gd]")
    fc.append("[gd][ti]overlay=y='18*(1-min(1,max(0,(t-%.2f)/0.7)))':shortest=1[t1]" % t_in)
    fc.append("[t1][en]overlay=shortest=1,fade=t=in:st=0:d=0.5,fade=t=out:st=%.3f:d=0.6[v]" % (total - 0.6))

    # ── Audio ──
    a = spec["audio"]
    wav = os.path.join(tmp, "audio.wav")
    if "cmd" in a:
        cmd = [x.replace("{out}", wav).replace("{seconds}", f"{total + 1.5:.2f}") for x in a["cmd"]]
        cmd[0] = os.path.join(ROOT, cmd[0]) if not os.path.isabs(cmd[0]) else cmd[0]
        run(cmd, cwd=ROOT)
        src = wav
    else:
        src = os.path.join(ROOT, a["file"])
    # −16 LUFS en dos pasadas: se mide el tramo ya recortado y con sus fundidos, y después se aplica una ganancia fija
    # (loudnorm lineal). En una sola pasada loudnorm adivina y erraba hasta 1,2 LU con material ya masterizado.
    cut, norm = os.path.join(tmp, "audio-cut.wav"), os.path.join(tmp, "audio16.wav")
    run(["ffmpeg", "-y", "-loglevel", "error", "-ss", str(a.get("start", 0)), "-i", src, "-t", f"{total:.3f}",
         "-af", f"aresample=48000,afade=t=in:st=0:d=0.25,afade=t=out:st={total - 1.4:.3f}:d=1.4", cut])
    target = "I=-16:TP=-1.5:LRA=11"
    r = subprocess.run(["ffmpeg", "-hide_banner", "-i", cut, "-af", f"loudnorm={target}:print_format=json", "-f", "null", "-"],
                       capture_output=True, text=True)
    m = json.loads(r.stderr[r.stderr.rindex("{"):r.stderr.rindex("}") + 1])
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", cut, "-af",
         f"loudnorm={target}:measured_I={m['input_i']}:measured_TP={m['input_tp']}:measured_LRA={m['input_lra']}:"
         f"measured_thresh={m['input_thresh']}:offset={m['target_offset']}:linear=true,aresample=48000", norm])
    inputs += ["-i", norm]

    out = os.path.join(ROOT, "web", "assets", "video", f"{slug}.mp4")
    if draft: out = os.path.join(tmp, f"{slug}-draft.mp4")
    run(["ffmpeg", "-y", "-loglevel", "error"] + inputs + ["-filter_complex", ";".join(f for f in fc if f),
         "-map", "[v]", "-map", f"{n + 2}:a", "-c:v", "libx264", "-preset", "slow" if not draft else "veryfast",
         "-b:v", "4200k", "-maxrate", "6000k", "-bufsize", "9000k", "-pix_fmt", "yuv420p", "-r", str(fps),
         "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", "-t", f"{total:.3f}", out])
    p = spec.get("poster")
    if p and not draft:
        os.makedirs(os.path.join(ROOT, "web", "assets", "img", slug), exist_ok=True)
        src = None
        for ext in ("jpg", "png"):
            f = os.path.join(frames, f"{p['shot']}_{p['frame']:04d}.{ext}")
            if os.path.exists(f): src = f
        run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-vf",
             f"scale={W}:{H}:flags=lanczos,curves=all='0/0 0.06/0.04 0.5/0.5 0.94/0.96 1/1',vignette=angle=PI/5", "-q:v", "3",
             os.path.join(ROOT, "web", "assets", "img", slug, "film-poster.jpg")])
    print(f"{out}: {total:.1f} s, {os.path.getsize(out) / 1e6:.1f} MB")
    if not draft: shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
