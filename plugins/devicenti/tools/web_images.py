#!/usr/bin/env python3
"""Website images for DeVicenti: the Blender photos (hero.jpg 2400x1350, top.jpg, close.jpg, keys.jpg, filter.jpg, made by
tools/blender/devicenti.py --mode photos) and the editor screenshots taken from the standalone with DV_SNAPSHOT at scale 3
(3360 x 2490), which are left in /tmp/claude-501/devicenti-shots: overview.png (Basement Boom, the unit), seq.png (the SEQ
page) and fx.png (the FX page). Writes web/assets/img/devicenti/ in the sizes scripts/build-web.py expects (jpg + webp) and
the menu thumbnail.

    HOME=<scratch home> DV_SNAPSHOT=/tmp/claude-501/devicenti-shots/overview.png DV_SNAP_SCALE=3 DV_SNAP_QUIT=1 DV_PRESET=2 \\
        build-devicenti/plugins/devicenti/DeVicenti_artefacts/Release/Standalone/DeVicenti.app/Contents/MacOS/DeVicenti
        (the same with DV_PAGE=SEQ DV_TRACK=1 for seq.png, and DV_PAGE=FX DV_TRACK=2 for fx.png)
    python3 plugins/devicenti/tools/web_images.py <folder with hero.jpg top.jpg close.jpg keys.jpg filter.jpg>
"""
import os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "web", "assets", "img", "devicenti")
NAV = os.path.join(ROOT, "web", "assets", "img", "nav")
SHOTS = "/tmp/claude-501/devicenti-shots"


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
    shot = lambda n: Image.open(os.path.join(SHOTS, n + ".png")).convert("RGB")
    hero = photo("hero")
    sized(hero, "hero")
    save(cover(hero, 1200, 630, ay=0.55), "og-image")
    save(cover(crop_frac(hero, 0.1, 0.1, 0.85, 0.9), 480, 300, ay=0.5), "devicenti", out=NAV)
    save(cover(photo("top"), 1200, 900), "hl-top")
    save(cover(photo("close"), 1200, 900), "hl-close")
    save(cover(photo("keys"), 1200, 900), "hl-keys")
    save(cover(photo("filter"), 1200, 900), "hl-filter")
    ov, sq, fx = shot("overview"), shot("seq"), shot("fx")
    sized(ov, "ui-overview")
    sized(sq, "ui-seq")
    sized(fx, "ui-fx")
    save(cover(crop_frac(ov, 0.0, 0.04, 1.0, 0.96), 1200, 900), "hl-ui")
    save(cover(crop_frac(sq, 0.0, 0.0, 1.0, 0.55), 1200, 900, ax=0.0, ay=0.0), "hl-pages")
    for f in sorted(os.listdir(OUT)): print(f, Image.open(os.path.join(OUT, f)).size if f.endswith(".jpg") else "")


if __name__ == "__main__":
    main()
