#!/usr/bin/env python3
"""Imágenes de la web de Tarnish: los renders del grabador (tools/blender/deck.py, bajados de pc50 a una carpeta)
y las capturas de la interfaz que deja scripts/manual-shots.py en /tmp/claude-501/manual-shots. Escribe
web/assets/img/tarnish/ en los tamaños que espera scripts/build-web.py (jpg + webp) y la miniatura del menú.

    python3 plugins/tarnish/tools/web_images.py <carpeta con hero.jpg close.jpg top.jpg>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "tarnish")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")
SHOTS = "/tmp/claude-501/manual-shots"
BG = (36, 25, 18)                   # el fondo del editor (óxido)


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
    save(cover(hero, 480, 300), "tarnish", out=NAV)
    save(cover(Image.open(os.path.join(src, "close.jpg")), 1200, 900), "hl-close")
    save(cover(Image.open(os.path.join(src, "top.jpg")), 1200, 900), "hl-top")
    shot = lambda n: Image.open(os.path.join(SHOTS, f"tarnish-{n}.jpg.png"))
    ov = shot("overview")
    sized(ov, "ui-overview")
    sized(crop_frac(shot("deck-vinyl"), 0.0, 0.063, 0.758, 0.494), "ui-vinyl")
    sized(on_bg(crop_frac(shot("service"), 0.758, 0.063, 0.242, 0.494), 900, 1100, 0.94), "ui-service")
    save(cover(crop_frac(ov, 0.0, 0.063, 0.758, 0.494), 1200, 900), "hl-ui")
    save(cover(crop_frac(shot("deck-vinyl"), 0.0, 0.063, 0.758, 0.494), 1200, 900), "hl-vinyl")
    save(on_bg(crop_frac(shot("service"), 0.758, 0.063, 0.242, 0.494), 1200, 900, 0.94), "hl-loop")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
