#!/usr/bin/env python3
"""Capturas de los manuales: lee plugins/<p>/docs/manual/shots.json, abre el standalone de cada producto con las
variables de captura de su editor, espera la imagen, la recorta y la guarda en docs/manual/img/ (JPEG, 1600 px de
ancho como mucho). Sólo macOS (usa los .app de build/).

    python3 scripts/manual-shots.py                 # todos
    python3 scripts/manual-shots.py murmur at101v   # algunos
    python3 scripts/manual-shots.py murmur --only flock-overview.jpg
"""
import json, os, subprocess, sys, time
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRODUCTS = {   # carpeta → (target de CMake, prefijo de las variables[, nombre del .app si no es el del target])
    "at20v": ("AT20V", "AT20V"), "at101v": ("AT101V", "AT101V"), "at42": ("AT42", "AT42"), "at700": ("AT700", "AT700"),
    "atfs1": ("ATFS1", "ATFS1"), "orrery": ("Orrery", "ORRERY"), "murmur": ("Murmur", "MURMUR"),
    "atmd": ("ATMD", "ATMD"), "lacquer": ("Lacquer", "LACQUER"), "halation": ("Halation", "HALATION"), "tarnish": ("Tarnish", "TARNISH"), "at303": ("AT303", "AT303"),
    "plumb": ("Plumb", "PLUMB"),
    "pallor": ("Pallor", "PALLOR"),
    "thrum": ("Thrum", "THRUM"),
    "at330": ("AT330", "AT330"),
    "devicenti": ("DeVicenti", "DV", "DeviceNTI V"),
}
TMP = "/tmp/claude-501/manual-shots"
DEMO_INPUT = os.path.join(TMP, "murmur-demo-input.wav")


def app_binary(folder, name, app=None):
    # BUILD=<carpeta> usa otro build (por defecto build/).
    app = app or name
    return os.path.join(ROOT, os.environ.get("BUILD", "build"), "plugins", folder, f"{name}_artefacts", "Release", "Standalone", f"{app}.app", "Contents", "MacOS", app)


def demo_input():
    """Entrada de prueba de Murmur: un clip propio de la serie (ATFS1 cantando vocales)."""
    if not os.path.exists(DEMO_INPUT):
        src = os.path.join(ROOT, "web", "assets", "samples", "atfs1", "formant-lead.m4a")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-ar", "48000", "-ac", "2", DEMO_INPUT], check=True)
    return DEMO_INPUT


def demo_input_halation():
    """Entrada de prueba de Halation: un piano eléctrico propio (ATFS1), con ataques claros para ver los ecos."""
    path = os.path.join(TMP, "halation-demo-input.wav")
    if not os.path.exists(path):
        src = os.path.join(ROOT, "web", "assets", "samples", "atfs1", "tine-piano.m4a")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-ar", "48000", "-ac", "2", path], check=True)
    return path


def demo_input_plumb():
    """Entrada de prueba de Plumb: la voz sintética propia de tools/demo_voice.py, cantando desafinada."""
    path = os.path.join(TMP, "plumb-demo-input.wav")
    if not os.path.exists(path):
        subprocess.run([sys.executable, os.path.join(ROOT, "plugins", "plumb", "tools", "demo_voice.py"), path], check=True)
    return path


def demo_samples_pallor():
    """Carpeta de muestras para la página SAMPLES de Pallor: archivos propios generados acá (no los del usuario)."""
    import math, struct, wave
    d = os.path.join(TMP, "pallor-samples", "Cold Kit")
    os.makedirs(os.path.join(TMP, "pallor-samples", "Cold Kit", "Extras"), exist_ok=True)
    names = ["kick long.wav", "kick short.wav", "snare plate.wav", "snare gated.wav", "clap room.wav", "hat closed.wav",
             "hat open.wav", "ride bell.wav", "tom low.wav", "tom high.wav", "metal hit.wav", "noise sweep.wav"]
    for k, n in enumerate(names):
        p = os.path.join(d, n)
        if os.path.exists(p): continue
        sr, dur = 44100, 0.25 + 0.05 * k
        with wave.open(p, "w") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
            f0 = 60 + 40 * k
            w.writeframes(b"".join(struct.pack("<h", int(20000 * math.exp(-i / (sr * 0.08)) * math.sin(2 * math.pi * f0 * i / sr))) for i in range(int(sr * dur))))
    return d


def take(folder, shot):
    name, prefix, *app = PRODUCTS[folder]
    binary = app_binary(folder, name, *app)
    png = os.path.join(TMP, f"{folder}-{shot['file']}.png")
    if os.path.exists(png): os.remove(png)
    env = dict(os.environ)
    env.update({k: str(v) for k, v in shot.get("env", {}).items()})
    env[f"{prefix}_SNAPSHOT"] = png
    if folder == "devicenti":
        # Settings in a scratch home, never the real one; no DV_PLAY (it would play through the speakers).
        env["HOME"] = env.get("DV_HOME", os.path.join(os.path.dirname(ROOT), "audiotools-private", "qa", "scratch-home-devicenti"))
        os.makedirs(env["HOME"], exist_ok=True)
    if folder == "murmur": env["MURMUR_DEMO_INPUT"] = demo_input()
    if folder == "halation": env["HALATION_DEMO_INPUT"] = demo_input_halation()
    if folder == "tarnish": env["TARNISH_DEMO_INPUT"] = demo_input_halation()
    if folder == "plumb": env["PLUMB_DEMO_INPUT"] = demo_input_plumb()
    if folder == "pallor" and env.get("PALLOR_SAMPLES_DIR") == "demo": env["PALLOR_SAMPLES_DIR"] = demo_samples_pallor()
    if folder == "lacquer":
        # La mezcla de demostración (instrumentos propios); "LACQUER_FILE": "none" deja el modo en vivo.
        demo = os.path.join(ROOT, "build", "lacquer-demo", "demo-mix.wav")
        if not os.path.exists(demo): subprocess.run([sys.executable, os.path.join(ROOT, "plugins", "lacquer", "tools", "demo_mix.py")], check=True)
        for k in ("LACQUER_FILE", "LACQUER_LEARN_FILE"):
            if env.get(k) == "demo": env[k] = demo
        if env.get("LACQUER_FILE") == "none": env.pop("LACQUER_FILE")
    wait = float(shot.get("wait", 4))
    if f"{prefix}_WAIT" in env: wait = max(wait, float(env[f"{prefix}_WAIT"]))
    p = subprocess.Popen([binary], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.time() + wait + 25
    last = -1
    while time.time() < deadline:
        time.sleep(0.5)
        if os.path.exists(png):
            size = os.path.getsize(png)
            if size > 0 and size == last: break
            last = size
    p.kill(); p.wait()
    time.sleep(0.4)
    if not os.path.exists(png): raise SystemExit(f"{folder}/{shot['file']}: no se generó la captura")
    im = Image.open(png).convert("RGB")
    x, y, w, h = shot.get("crop", [0, 0, 1, 1])
    W, H = im.size
    box = (round(x * W), round(y * H), round(min(1.0, x + w) * W), round(min(1.0, y + h) * H))
    im = im.crop(box)
    if im.width > 1600: im = im.resize((1600, round(im.height * 1600 / im.width)), Image.LANCZOS)
    out = os.path.join(ROOT, "plugins", folder, "docs", "manual", "img", shot["file"])
    os.makedirs(os.path.dirname(out), exist_ok=True)
    im.save(out, quality=84, optimize=True, progressive=True)
    return out, im.size


def main():
    os.makedirs(TMP, exist_ok=True)
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
    folders = [a for a in args if a in PRODUCTS] or list(PRODUCTS)
    for folder in folders:
        spec = os.path.join(ROOT, "plugins", folder, "docs", "manual", "shots.json")
        if not os.path.exists(spec): print(f"{folder}: sin shots.json"); continue
        shots = json.load(open(spec))
        for shot in shots:
            if only and shot["file"] != only: continue
            out, size = take(folder, shot)
            print(f"{folder:7s} {shot['file']:34s} {size[0]}x{size[1]}")


if __name__ == "__main__":
    main()
