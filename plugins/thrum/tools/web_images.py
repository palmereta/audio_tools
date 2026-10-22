#!/usr/bin/env python3
"""Imágenes de la web de Thrum: las fotos (tools/blender/thrum.py --mode photos: hero, guitar, pedals, amp, lineup) y
las capturas de la interfaz que deja scripts/manual-shots.py en /tmp/claude-501/manual-shots (2720 × 1700). Escribe
web/assets/img/thrum/ en los tamaños que espera scripts/build-web.py (jpg + webp) y la miniatura del menú.

    BUILD=build-guitar python3 scripts/manual-shots.py thrum
    python3 plugins/thrum/tools/web_images.py <carpeta con hero.jpg guitar.jpg pedals.jpg amp.jpg lineup.jpg>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "thrum")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")
SHOTS = "/tmp/claude-501/manual-shots"
PAGE = (0.0, 0.3, 1.0, 0.7)                     # la página (debajo del diapasón), en fracciones del editor


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
    shot = lambda n: Image.open(os.path.join(SHOTS, f"thrum-{n}.jpg.png"))
    hero = photo("hero")
    sized(hero, "hero")
    save(cover(hero, 1200, 630), "og-image")
    save(cover(hero, 480, 300), "thrum", out=NAV)
    save(cover(photo("guitar"), 1200, 900), "hl-guitar")
    save(cover(photo("pedals"), 1200, 900), "hl-pedals")
    save(cover(photo("amp"), 1200, 900), "hl-amp")
    save(cover(photo("lineup"), 1200, 900), "hl-lineup")
    ov = shot("overview")
    sized(ov, "ui-overview")
    sized(crop_frac(shot("play"), *PAGE), "ui-play")
    sized(crop_frac(shot("amp"), *PAGE), "ui-amp")
    sized(crop_frac(shot("pedals"), *PAGE), "ui-pedals")
    # El diapasón en vivo con la guitarra debajo (la mitad izquierda del editor).
    save(cover(crop_frac(shot("fretboard"), 0.0, 0.06, 0.62, 0.62), 1200, 900, ax=0.0, ay=0.0), "hl-fretboard")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
