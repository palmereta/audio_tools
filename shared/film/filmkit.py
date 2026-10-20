"""Herramientas comunes para los films de la web (Blender 5.x, Cycles). Se importa desde la escena de cada plugin:

    import sys; sys.path.insert(0, "<repo>/shared/film"); import filmkit as fk

La escena arma el instrumento como para las fotos y después, por cada toma:

    fk.setup(sc, quality="final")                       # Cycles en la GPU, AgX, desenfoque de movimiento
    cam = fk.camera(sc)
    path = fk.Path([(0.0, (x, y, z), (lx, ly, lz), 50), (1.0, ...)])   # u, posición, mira, lente
    def pose(f, t, u):                                  # estado de la escena en el cuadro f (t en s, u de 0 a 1)
        path.apply(cam, u, focus=bob.location)          # cámara con aceleración suave y foco que sigue al objeto
        knob.rotation_euler.z = ...
        return [knob]                                   # objetos a los que se les graba el cuadro
    fk.render_shot(sc, "swing", frames=192, pose=pose, out=OUT)

render_shot no pinta cuadro por cuadro con el estado puesto a mano: graba keyframes de todo lo que se movió y
renderiza la animación, así el desenfoque de movimiento sale de verdad (la cámara y las perillas se ven moverse).
Escribe <out>/<toma>_0001.jpg… que scripts/film.py arma con los demás.
"""
import math, os, struct, wave

import bpy
from mathutils import Vector

FPS = 24


# ── Render ──
def setup(sc, quality="final", res=(1920, 1080), blur=0.5):
    """Cycles en la GPU (CUDA en pc50, Metal en la Mac), AgX con más contraste, desenfoque de movimiento."""
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("OPTIX", "CUDA", "METAL", "HIP"):
        try:
            prefs.compute_device_type = kind; prefs.get_devices()
        except TypeError:
            continue
        if any(d.type == kind for d in prefs.devices) and kind != "OPTIX":       # OptiX no arranca en WSL
            for d in prefs.devices: d.use = d.type == kind
            break
    sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"
    q = {"draft": (24, 0.5), "preview": (64, 0.75), "final": (192, 1.0)}[quality]
    sc.cycles.samples = q[0]; sc.cycles.use_denoising = True
    try: sc.cycles.denoising_use_gpu = True
    except AttributeError: pass
    sc.cycles.use_adaptive_sampling = True; sc.cycles.adaptive_threshold = 0.02
    sc.render.resolution_x, sc.render.resolution_y = res; sc.render.resolution_percentage = int(100 * q[1])
    sc.render.fps = FPS
    sc.render.use_motion_blur = blur > 0; sc.render.motion_blur_shutter = blur
    sc.view_settings.view_transform = "AgX"
    try: sc.view_settings.look = "AgX - Medium High Contrast"
    except TypeError: pass
    sc.render.film_transparent = False
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 94


def camera(sc, lens=50, fstop=2.8, name="filmcam"):
    cd = bpy.data.cameras.new(name); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    cd.sensor_width = 36; cd.clip_start = 0.005
    cam = bpy.data.objects.new(name, cd); sc.collection.objects.link(cam); sc.camera = cam
    return cam


# ── Movimiento ──
def ease(u, kind="inout"):
    u = min(1.0, max(0.0, u))
    if kind == "in": return u * u * u
    if kind == "out": return 1 - (1 - u) ** 3
    if kind == "linear": return u
    return u * u * (3 - 2 * u) if kind == "smooth" else (4 * u ** 3 if u < 0.5 else 1 - (-2 * u + 2) ** 3 / 2)


def _cr(p0, p1, p2, p3, t):
    """Catmull-Rom centrípeta simplificada (uniforme) entre p1 y p2."""
    t2, t3 = t * t, t * t * t
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)


class Path:
    """Recorrido de cámara por puntos (u, posición, mira, lente[, f-stop]); se recorre con una sola aceleración
    suave de punta a punta (sin frenar en los puntos intermedios), como un travelling con grúa."""

    def __init__(self, keys, easing="inout"):
        self.k = [(u, Vector(p), Vector(l), lens, (rest[0] if rest else None)) for (u, p, l, lens, *rest) in keys]
        self.easing = easing

    def at(self, u):
        u = ease(u, self.easing); k = self.k
        if u <= k[0][0]: return k[0][1:]
        if u >= k[-1][0]: return k[-1][1:]
        i = max(j for j in range(len(k) - 1) if k[j][0] <= u)
        a, b = k[i], k[i + 1]; t = (u - a[0]) / (b[0] - a[0])
        p0, p3 = k[max(0, i - 1)], k[min(len(k) - 1, i + 2)]
        pos = _cr(p0[1], a[1], b[1], p3[1], t); look = _cr(p0[2], a[2], b[2], p3[2], t)
        lens = a[3] + (b[3] - a[3]) * ease(t, "smooth")
        fs = a[4] if a[4] is None or b[4] is None else a[4] + (b[4] - a[4]) * ease(t, "smooth")
        return pos, look, lens, fs

    def apply(self, cam, u, focus=None, shake=0.0, t=0.0):
        pos, look, lens, fs = self.at(u)
        if shake > 0:                                  # cámara en mano muy sutil: ruido suave, no temblor
            pos = pos + Vector((noise1(t, 1), noise1(t, 2), noise1(t, 3))) * shake
            look = look + Vector((noise1(t, 4), noise1(t, 5), noise1(t, 6))) * shake * 0.6
        cam.location = pos
        cam.rotation_euler = (look - pos).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        if fs is not None: cam.data.dof.aperture_fstop = fs
        target = Vector(focus) if focus is not None else look
        cam.data.dof.focus_distance = max(0.01, (target - pos).dot((look - pos).normalized()))


def noise1(t, seed, rate=0.35):
    """Ruido suave (suma de senos inconmensurables), de −1 a 1."""
    s = seed * 12.9898
    return (math.sin(2 * math.pi * rate * t * 1.0 + s) * 0.5 + math.sin(2 * math.pi * rate * t * 2.17 + 2 * s) * 0.3
            + math.sin(2 * math.pi * rate * t * 3.71 + 3 * s) * 0.2)


def rack(u, u0, u1, a, b):
    """Valor que pasa de a a b entre u0 y u1 con aceleración suave (para foco, intensidades, etc.)."""
    return a + (b - a) * ease((u - u0) / max(1e-6, u1 - u0), "smooth")


# ── Audio ──
def envelope(path, fps=FPS, attack=0.005, release=0.12, start=0.0):
    """Envolvente por cuadro (0..1) de un WAV de 16 o 24 bits: para encender luces o mover medidores al ritmo del
    sonido. Seguidor de picos con ataque y caída, normalizado a su máximo."""
    w = wave.open(path); sr = w.getframerate(); ch = w.getnchannels(); sw = w.getsampwidth(); n = w.getnframes()
    raw = w.readframes(n); w.close()
    if sw == 2: vals = struct.unpack("<%dh" % (len(raw) // 2), raw); scale = 32768.0
    elif sw == 3:
        vals = [int.from_bytes(raw[i:i + 3], "little", signed=True) for i in range(0, len(raw), 3)]; scale = 8388608.0
    else: raise ValueError("WAV de 16 o 24 bits")
    mono = [abs(sum(vals[i:i + ch])) / (ch * scale) for i in range(0, len(vals), ch)]
    a, r = math.exp(-1 / (attack * sr)), math.exp(-1 / (release * sr))
    e, out, per = 0.0, [], sr / fps
    acc, k = 0.0, 0
    s0 = int(start * sr)
    for i, x in enumerate(mono[s0:]):
        e = a * e + (1 - a) * x if x > e else r * e
        acc = max(acc, e); k += 1
        if k >= per: out.append(acc); acc, k = 0.0, 0
    m = max(out) if out else 1.0
    return [x / m for x in out] if m > 0 else out


def onsets(env, thresh=0.25, gap=4):
    """Cuadros donde la envolvente sube de golpe (notas, golpes): para disparar destellos o movimientos."""
    res, last = [], -gap
    for i in range(1, len(env)):
        if env[i] - env[i - 1] > thresh and i - last >= gap: res.append(i); last = i
    return res


# ── Render de una toma ──
def _key(o):
    o.keyframe_insert("location"); o.keyframe_insert("rotation_euler"); o.keyframe_insert("scale")
    if o.type == "CAMERA":
        o.data.keyframe_insert("lens"); o.data.dof.keyframe_insert("focus_distance"); o.data.dof.keyframe_insert("aperture_fstop")


def render_shot(sc, name, frames, pose, out, first=1, step=1, extra_keys=None):
    """Graba keyframes de la toma con pose(f, t, u) → objetos movidos (la cámara siempre) y la renderiza.
    extra_keys(f): para claves que no son de transformación (intensidades de luz, materiales)."""
    os.makedirs(out, exist_ok=True)
    for o in sc.objects: o.animation_data_clear()
    for f in range(1, frames + 1):
        sc.frame_set(f)
        t, u = (f - 1) / FPS, (f - 1) / max(1, frames - 1)
        moved = pose(f, t, u) or []
        for o in set([sc.camera] + list(moved)): _key(o)
        if extra_keys: extra_keys(f)
    for o in sc.objects:                               # entre claves, lineal: el movimiento ya viene suavizado
        ad = o.animation_data
        if ad and ad.action:
            for fc in _fcurves(ad.action):
                for kp in fc.keyframe_points: kp.interpolation = "LINEAR"
    sc.frame_start, sc.frame_end, sc.frame_step = first, frames, step
    sc.render.filepath = os.path.join(out, f"{name}_####")
    bpy.ops.render.render(animation=True)


def _fcurves(action):
    if hasattr(action, "fcurves"): return list(action.fcurves)
    res = []                                           # Blender 5: acciones por capas
    for layer in getattr(action, "layers", []):
        for strip in layer.strips:
            for bag in getattr(strip, "channelbags", []): res += list(bag.fcurves)
    return res
