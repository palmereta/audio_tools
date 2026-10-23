#!/usr/bin/env python3
"""Imágenes de la web del AT330: las fotos de Blender (hero.jpg, close.jpg, top.jpg en audiotools-private/at330/photos-final)
y las capturas del editor que saca la app independiente con AT330_SNAPSHOT (3200 × 1920) y quedan en
/tmp/claude-501/at330-shots: overview.png (Rain on Neon, panel), extended.png (Rain on Neon, página Extended) y
vocoder.png (Vocoder Ensemble, panel). Escribe web/assets/img/at330/ en los tamaños que espera scripts/build-web.py
(jpg + webp) y la miniatura del menú.

    AT330_SNAPSHOT=/tmp/claude-501/at330-shots/overview.png AT330_PRESET=18 AT330_PAGE=panel AT330_KEYS=45,52,57,60,64,69 \\
        HOME=<home de pruebas> build-at330/plugins/at330/AT330_artefacts/Release/Standalone/AT330.app/Contents/MacOS/AT330
    python3 plugins/at330/tools/web_images.py <carpeta con hero.jpg close.jpg top.jpg>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "at330")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")
SHOTS = "/tmp/claude-501/at330-shots"


def save(im, name, q=84, out=OUT):
    im = im.convert("RGB")
    im.save(os.path.join(out, name + ".jpg"), quality=q, optimize=True, progressive=True)
    im.save(os.path.join(out, name + ".webp"), quality=q - 4, method=6)


def sized(im, name):
    for w in (1200, 2400):
        k = min(w, im.width) / im.width
        save(im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS), f"{name}-{w}")


def crop_frac(im, x, y, w, h):
    W, H = im.size
    return im.crop((round(x * W), round(y * H), round(min(1, x + w) * W), round(min(1, y + h) * H)))


def cover(im, w, h, ax=0.5, ay=0.5):
    k = max(w / im.width, h / im.height)
    r = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
    x, y = round((r.width - w) * ax), round((r.height - h) * ay)
    return r.crop((x, y, x + w, y + h))


def main():
    src = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    photo = lambda n: Image.open(os.path.join(src, n + ".jpg"))
    shot = lambda n: Image.open(os.path.join(SHOTS, n + ".png"))
    hero = photo("hero")
    sized(hero, "hero")
    save(cover(hero, 1200, 630, ay=0.75), "og-image")
    save(cover(crop_frac(hero, 0.0, 0.18, 1.0, 0.82), 480, 300, ay=0.6), "at330", out=NAV)
    top, close = photo("top"), photo("close")
    save(cover(crop_frac(top, 0.10, 0.14, 0.55, 0.66), 1200, 900), "hl-top")
    save(cover(close, 1200, 900), "hl-close")
    save(cover(crop_frac(hero, 0.05, 0.3, 0.55, 0.7), 1200, 900, ax=0.0, ay=0.6), "hl-keys")
    ov, ex, vo = shot("overview"), shot("extended"), shot("vocoder")
    sized(ov, "ui-overview")
    sized(ex, "ui-extended")
    sized(crop_frac(vo, 0.62, 0.08, 0.38, 0.48), "ui-vocoder")
    save(cover(crop_frac(ov, 0.29, 0.05, 0.5, 0.6), 1200, 900), "hl-ui")
    save(cover(crop_frac(ex, 0.0, 0.1, 0.72, 0.9), 1200, 900), "hl-extended")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
