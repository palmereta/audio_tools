#!/usr/bin/env python3
"""Fotos de la web de Murmur: las dos imágenes de tools/blender/murmur.py --mode hero (murmur_hero.png, 2400 × 1100,
y murmur_flock.png, 1600 × 1200). Escribe web/assets/img/murmur/ en los tamaños que espera scripts/build-web.py
(jpg + webp) y la miniatura del menú. Las capturas de la interfaz (ui-*, hl-simple, hl-freeze...) no se tocan.

    python3 plugins/murmur/tools/web_images.py <carpeta con murmur_hero.png y murmur_flock.png>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "murmur")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")


def save(im, name, q=84, out=OUT, webp=True):
    im = im.convert("RGB")
    im.save(os.path.join(out, name + ".jpg"), quality=q, optimize=True, progressive=True)
    if webp: im.save(os.path.join(out, name + ".webp"), quality=q - 4, method=6)


def cover(im, w, h, fx=0.5, fy=0.5):
    """Recorta al formato w × h; fx, fy eligen qué parte queda (0,5 = el centro)."""
    k = max(w / im.width, h / im.height)
    r = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
    x, y = round((r.width - w) * fx), round((r.height - h) * fy)
    return r.crop((x, y, x + w, y + h))


def main():
    src = sys.argv[1]
    hero = Image.open(os.path.join(src, "murmur_hero.png")).convert("RGB")
    for w in (1200, 2400):
        k = min(w, hero.width) / hero.width
        save(hero.resize((round(hero.width * k), round(hero.height * k)), Image.LANCZOS), f"hero-{w}")
    save(cover(hero, 1200, 630), "og-image", webp=False)
    save(cover(hero, 480, 300, fx=0.62), "murmur", out=NAV)
    save(cover(Image.open(os.path.join(src, "murmur_flock.png")).convert("RGB"), 1200, 900), "hl-flock")
    print("listo:", OUT)


if __name__ == "__main__":
    main()
