#!/usr/bin/env python3
"""Imágenes de la web de Valence, desde las fotos del átomo (tools/blender/valence.py --mode still, en pc50-wsl): el
héroe 1200/2400, la og-image 1200 × 630, la miniatura del menú 480 × 300 y los highlights 1200 × 900, en jpg + webp,
en los tamaños que espera scripts/build-web.py (los mismos que Thrum y Orrery).

    python3 plugins/valence/tools/web_images.py <carpeta con hero.jpg tight.jpg hl-*.jpg> [salida]

hero.jpg es el héroe (16:9, 2400 de ancho o más); tight.jpg el mismo átomo encuadrado más cerca, del que salen la
og-image y la miniatura (en el héroe el átomo queda chico a 480 de ancho). Cada hl-<nombre>.jpg da hl-<nombre>.
Sin [salida] escribe web/assets/img/valence/ y web/assets/img/nav/valence.*.
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
ZOOM = {"og-image": 1.3, "nav": 1.8, "hl-atom": 1.4, "hl-dnb": 1.25}    # centre crop before sizing: the atom renders small


def save(im, out, name, q=84):
    im = im.convert("RGB")
    im.save(os.path.join(out, name + ".jpg"), quality=q, optimize=True, progressive=True)
    im.save(os.path.join(out, name + ".webp"), quality=q - 4, method=6)


def cover(im, w, h, ax=0.5, ay=0.5):
    k = max(w / im.width, h / im.height)
    r = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
    x, y = round((r.width - w) * ax), round((r.height - h) * ay)
    return r.crop((x, y, x + w, y + h))


def zoom(im, k):
    w, h = round(im.width / k), round(im.height / k)
    return im.crop(((im.width - w) // 2, (im.height - h) // 2, (im.width - w) // 2 + w, (im.height - h) // 2 + h))


def main():
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "web", "assets", "img", "valence")
    nav = out if len(sys.argv) > 2 else os.path.join(ROOT, "web", "assets", "img", "nav")
    os.makedirs(out, exist_ok=True)
    photo = lambda n: Image.open(os.path.join(src, n))
    hero = photo("hero.jpg")
    for w in (1200, 2400):
        save(hero.resize((w, round(hero.height * w / hero.width)), Image.LANCZOS), out, f"hero-{w}")
    tight = photo("tight.jpg")
    save(cover(zoom(tight, ZOOM["og-image"]), 1200, 630), out, "og-image")
    save(cover(zoom(tight, ZOOM["nav"]), 480, 300), nav, "valence" if nav != out else "nav-valence")
    for f in sorted(os.listdir(src)):
        if f.startswith("hl-") and f.endswith(".jpg"): save(cover(zoom(photo(f), ZOOM.get(f[:-4], 1.0)), 1200, 900), out, f[:-4])
    for f in sorted(os.listdir(out)):
        if f.endswith(".jpg"): print(f, Image.open(os.path.join(out, f)).size, os.path.getsize(os.path.join(out, f)) // 1024, "KB")


if __name__ == "__main__":
    main()
