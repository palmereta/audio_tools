#!/usr/bin/env python3
"""Imágenes de la web de Pallor: las fotos del instrumento (tools/blender/pallor.py --mode photos) y las capturas de
la interfaz que deja scripts/manual-shots.py en /tmp/claude-501/manual-shots (2880 × 1880). Escribe
web/assets/img/pallor/ en los tamaños que espera scripts/build-web.py (jpg + webp) y la miniatura del menú.

    python3 scripts/manual-shots.py pallor
    python3 plugins/pallor/tools/web_images.py <carpeta con hero.jpg close.jpg knobs.jpg side.jpg top.jpg>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "pallor")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")
SHOTS = "/tmp/claude-501/manual-shots"
SCREEN = (0.0239, 0.1217, 0.9522, 0.5183)          # el vidrio de la pantalla, en fracciones del editor


def save(im, name, q=84, out=OUT):
    im = im.convert("RGB")
    im.save(os.path.join(out, name + ".jpg"), quality=q, optimize=True, progressive=True)
    im.save(os.path.join(out, name + ".webp"), quality=q - 4, method=6)


def sized(im, name):
    """Variantes -1200 y -2400 (la de 2400 no se agranda más allá del original)."""
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
    shot = lambda n: Image.open(os.path.join(SHOTS, f"pallor-{n}.jpg.png"))
    hero = photo("hero")
    sized(hero, "hero")
    save(cover(hero, 1200, 630), "og-image")
    save(cover(hero, 480, 300), "pallor", out=NAV)
    save(cover(photo("close"), 1200, 900), "hl-close")
    save(cover(photo("knobs"), 1200, 900), "hl-knobs")
    save(cover(photo("side"), 1200, 900), "hl-side")
    save(cover(photo("top"), 1200, 900), "hl-top")
    ov = shot("overview")
    sized(ov, "ui-overview")
    sized(crop_frac(shot("pattern"), *SCREEN), "ui-pattern")
    sized(crop_frac(shot("fx"), *SCREEN), "ui-fx")
    sized(crop_frac(shot("voice"), *SCREEN), "ui-voice")
    save(cover(crop_frac(ov, 0.0, 0.0, 1.0, 1.0), 1200, 900, ay=1.0), "hl-ui")
    save(cover(crop_frac(shot("lanes"), 0.17, 0.25, 0.83, 0.42), 1200, 900, ax=0.0), "hl-pattern")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
