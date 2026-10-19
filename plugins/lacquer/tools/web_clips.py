#!/usr/bin/env python3
"""Clips de la web de Lacquer: la mezcla de demostración (tools/demo_mix.py) y sus masters en varios estilos,
renderizados con lacquerrender. Todos se llevan a −16 LUFS con una ganancia fija (nada de normalización dinámica,
que cambiaría justamente lo que se quiere escuchar), así el A/B compara sonido y no volumen, como lo haría una
plataforma de streaming. Escribe web/assets/samples/lacquer/<id>.m4a y sus entradas en samples.json.

    python3 plugins/lacquer/tools/web_clips.py
"""
import json, os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
DEMO = os.path.join(ROOT, "build", "lacquer-demo")
OUT = os.path.join(ROOT, "web", "assets", "samples", "lacquer")
RENDER = os.path.join(ROOT, "build", "plugins", "lacquer", "lacquerrender")
sys.path.insert(0, os.path.join(ROOT, "scripts"))
from samples import peaks  # noqa: E402

CLIPS = [
    ("the-mix", None, {"en": "The mix", "es": "La mezcla"},
     {"en": "The unmastered demo mix (our own ATMD, AT700 and ATFS1), raised to the same loudness as the masters.",
      "es": "La mezcla de demostración sin masterizar (nuestros ATMD, AT700 y ATFS1), subida a la misma sonoridad que los masters."}),
    ("balanced", ["--style", "balanced"], {"en": "Balanced", "es": "Balanced"},
     {"en": "The default: an even tone, gentle glue, -14 LUFS. Listen to the low mids clearing and the top opening up.",
      "es": "El de fábrica: tono parejo, glue suave, -14 LUFS. Escuchá cómo se limpian los medios graves y se abren los agudos."}),
    ("warm", ["--style", "warm"], {"en": "Warm", "es": "Warm"},
     {"en": "Rounder low mids, a softer top and light tape saturation.", "es": "Medios graves más redondos, agudos más suaves y una saturación de cinta leve."}),
    ("bright", ["--style", "bright"], {"en": "Bright", "es": "Bright"},
     {"en": "More air above 10 kHz and less mud around 300 Hz.", "es": "Más aire arriba de 10 kHz y menos barro cerca de 300 Hz."}),
    ("punchy", ["--style", "punchy", "--intensity", "70"], {"en": "Punchy", "es": "Punchy"},
     {"en": "Slower glue so the kick and snare come through, with a soft clipper on the peaks.", "es": "Glue más lento para que pasen bombo y redoblante, con un recortador suave en los picos."}),
    ("loud-club", ["--style", "loud", "--intensity", "75", "--target", "club"], {"en": "Loud, Club target", "es": "Loud, objetivo Club"},
     {"en": "Mastered to -8 LUFS, then turned down to the same loudness as the others: what extra loudness costs on streaming.",
      "es": "Masterizado a -8 LUFS y bajado a la misma sonoridad que los demás: lo que cuesta la sonoridad extra en streaming."}),
]


def lufs(path):
    r = subprocess.run(["ffmpeg", "-nostats", "-i", path, "-af", "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True).stderr
    return float(re.findall(r"I:\s+(-?[\d.]+) LUFS", r)[-1])


def main():
    os.makedirs(OUT, exist_ok=True)
    mix = os.path.join(DEMO, "demo-mix.wav")
    if not os.path.exists(mix):
        subprocess.run([sys.executable, os.path.join(ROOT, "plugins", "lacquer", "tools", "demo_mix.py")], check=True)
    meta_path = os.path.join(ROOT, "web", "assets", "samples", "samples.json")
    meta = json.load(open(meta_path))
    order = [k for k in meta["_order"] if not k.startswith("lacquer/")]
    for cid, args, title, desc in CLIPS:
        src = mix
        if args:
            src = os.path.join(DEMO, f"master-{cid}.wav")
            subprocess.run([RENDER, mix, src, *args], check=True, stdout=subprocess.DEVNULL)
        gain = -16.0 - lufs(src)
        level = os.path.join(DEMO, f"web-{cid}.wav")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-af", f"volume={gain:.3f}dB", "-c:a", "pcm_s24le", level], check=True)
        m4a = os.path.join(OUT, f"{cid}.m4a")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", level, "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", m4a], check=True)
        key = f"lacquer/{cid}"
        secs = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", m4a], capture_output=True, text=True).stdout)
        meta[key] = {"plugin": "lacquer", "id": cid, "preset": cid, "title": title, "desc": desc, "secs": round(secs, 1),
                     "peaks": peaks(level), "bpm": 104, "kb": os.path.getsize(m4a) // 1024, "nomidi": True}
        order.append(key)
        print(f"{cid:10s} gain {gain:+.2f} dB  {os.path.getsize(m4a) // 1024} KB")
    meta["_order"] = order
    json.dump(meta, open(meta_path, "w"), ensure_ascii=False, indent=1)


if __name__ == "__main__":
    main()
