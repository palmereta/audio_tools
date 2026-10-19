#!/usr/bin/env python3
"""Imágenes de la web de Halation: los renders de la maqueta (tools/blender/maquette.py, bajados de pc50 a una carpeta)
y las capturas de la interfaz que deja scripts/manual-shots.py en /tmp/claude-501/manual-shots. Escribe
web/assets/img/halation/ en los tamaños que espera scripts/build-web.py (jpg + webp) y la miniatura del menú.

    python3 plugins/halation/tools/web_images.py <carpeta con hero.jpg close.jpg top.jpg freeze.jpg>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "halation")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")
SHOTS = "/tmp/claude-501/manual-shots"
BG = (31, 29, 27)                   # el fondo del editor


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


def cover(im, w, h):
    k = max(w / im.width, h / im.height)
    r = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
    x, y = (r.width - w) // 2, (r.height - h) // 2
    return r.crop((x, y, x + w, y + h))


def on_bg(im, w, h, fill=0.92):
    k = min(w * fill / im.width, h * fill / im.height)
    r = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
    c = Image.new("RGB", (w, h), BG)
    c.paste(r, ((w - r.width) // 2, (h - r.height) // 2))
    return c


def main():
    src = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    hero = Image.open(os.path.join(src, "hero.jpg"))
    sized(hero, "hero")
    save(cover(hero, 1200, 630), "og-image")
    save(cover(hero, 480, 300), "halation", out=NAV)
    save(cover(Image.open(os.path.join(src, "close.jpg")), 1200, 900), "hl-close")
    save(cover(Image.open(os.path.join(src, "top.jpg")), 1200, 900), "hl-top")
    save(cover(Image.open(os.path.join(src, "freeze.jpg")), 1200, 900), "hl-freeze")
    shot = lambda n: Image.open(os.path.join(SHOTS, f"halation-{n}.jpg.png"))
    ov = shot("overview")
    sized(ov, "ui-overview")
    sized(crop_frac(shot("room-space"), 0.0, 0.061, 1.0, 0.4634), "ui-room")
    sized(crop_frac(shot("room-frozen"), 0.0, 0.061, 1.0, 0.4634), "ui-frozen")
    save(cover(crop_frac(ov, 0.22, 0.09, 0.56, 0.42), 1200, 900), "hl-ui")
    save(on_bg(crop_frac(shot("modules"), 0.0, 0.77, 1.0, 0.193), 1200, 900, 0.96), "hl-mods")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
