#!/usr/bin/env python3
"""Imágenes de la web de Lacquer: los renders del torno (tools/blender/lathe.py, bajados de pc50 a una carpeta) y las
capturas de la interfaz que deja scripts/manual-shots.py en /tmp/claude-501/manual-shots. Escribe
web/assets/img/lacquer/ en los tamaños que espera scripts/build-web.py (jpg + webp).

    python3 plugins/lacquer/tools/web_images.py <carpeta con hero.jpg close.jpg top.jpg [poster.jpg]>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "lacquer")
SHOTS = "/tmp/claude-501/manual-shots"
ENAMEL = (196, 198, 186)


def save(im, name, q=84):
    im = im.convert("RGB")
    im.save(os.path.join(OUT, name + ".jpg"), quality=q, optimize=True, progressive=True)
    im.save(os.path.join(OUT, name + ".webp"), quality=q - 4, method=6)


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


def on_enamel(im, w, h, fill=0.9):
    k = min(w * fill / im.width, h * fill / im.height)
    r = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
    c = Image.new("RGB", (w, h), ENAMEL)
    c.paste(r, ((w - r.width) // 2, (h - r.height) // 2))
    return c


def main():
    src = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    hero = Image.open(os.path.join(src, "hero.jpg"))
    sized(hero, "hero")
    save(cover(hero, 1200, 630), "og-image")
    save(cover(Image.open(os.path.join(src, "close.jpg")), 1200, 900), "hl-close")
    save(cover(Image.open(os.path.join(src, "top.jpg")), 1200, 900), "hl-top")
    if os.path.exists(os.path.join(src, "poster.jpg")): save(Image.open(os.path.join(src, "poster.jpg")), "film-poster")
    ov = Image.open(os.path.join(SHOTS, "lacquer-overview.jpg.png"))
    loud = Image.open(os.path.join(SHOTS, "lacquer-loud-club.jpg.png"))
    sized(ov, "ui-overview")
    sized(crop_frac(loud, 0.405, 0.47, 0.595, 0.53), "ui-loud")
    sized(crop_frac(ov, 0.405, 0.47, 0.595, 0.27), "ui-readouts")
    save(on_enamel(crop_frac(ov, 0.0, 0.06, 0.41, 0.62), 1200, 900, 0.96), "hl-ui")
    save(cover(crop_frac(ov, 0.405, 0.40, 0.595, 0.60), 1200, 900), "hl-read")
    save(on_enamel(crop_frac(loud, 0.61, 0.735, 0.39, 0.265), 1200, 900, 0.92), "hl-mod")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
