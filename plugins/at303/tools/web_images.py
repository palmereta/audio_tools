#!/usr/bin/env python3
"""Imágenes de la web del AT303: las fotos del instrumento (tools/blender/scene303.py --mode photos) y las capturas
de la interfaz que deja scripts/manual-shots.py en /tmp/claude-501/manual-shots. Escribe web/assets/img/at303/ en
los tamaños que espera scripts/build-web.py (jpg + webp) y la miniatura del menú.

    python3 plugins/at303/tools/web_images.py <carpeta con hero.jpg close.jpg top.jpg>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "at303")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")
SHOTS = "/tmp/claude-501/manual-shots"
BG = (28, 30, 32)                   # el panel del editor (grafito)


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
    save(cover(hero, 480, 300), "at303", out=NAV)
    save(cover(Image.open(os.path.join(src, "close.jpg")), 1200, 900), "hl-close")
    save(cover(Image.open(os.path.join(src, "top.jpg")), 1200, 900), "hl-top")
    shot = lambda n: Image.open(os.path.join(SHOTS, f"at303-{n}.jpg.png"))
    ov = shot("overview")
    sized(ov, "ui-overview")
    sized(on_bg(crop_frac(ov, 0.262, 0.672, 0.462, 0.205), 1600, 760, 0.94), "ui-editor")
    sized(crop_frac(shot("inspect"), 0.0, 0.0, 1.0, 0.669), "ui-inspect")
    save(cover(crop_frac(ov, 0.0, 0.0, 1.0, 0.669), 1200, 900), "hl-ui")
    save(on_bg(crop_frac(ov, 0.262, 0.672, 0.462, 0.205), 1200, 900, 0.94), "hl-editor")
    save(cover(crop_frac(shot("inspect"), 0.0, 0.03, 0.75, 0.6), 1200, 900), "hl-inspect")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
