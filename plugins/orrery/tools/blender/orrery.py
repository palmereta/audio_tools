"""Un orrery de bronce para la web de Orrery: sol encendido, ocho planetas (los colores de los carriles)
en brazos de distinto largo y altura, anillos finos que marcan las órbitas, sobre un pedestal oscuro.

Corre en pc50-wsl:
  blender -b -P orrery.py -- --mode hero --out out --samples 160
  blender -b -P orrery.py -- --mode film --shot wide --frames 120 --out out --samples 40
Unidades: 1 = 10 cm.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "160"))
LANES = [(0.498, 0.820, 0.780), (0.663, 0.612, 0.941), (0.941, 0.612, 0.718), (0.549, 0.753, 0.961),
         (0.722, 0.890, 0.627), (0.949, 0.627, 0.482), (0.847, 0.706, 0.973), (0.961, 0.886, 0.478)]
PERIODS = [13, 17, 19, 23, 29, 31, 37, 41]      # vueltas lentas y distintas, como los lazos de los carriles
SIZES = [0.17, 0.21, 0.15, 0.19, 0.14, 0.24, 0.16, 0.13]
RK = [1.55 + 0.42 * k for k in range(8)]         # largo de cada brazo
ZK = [1.2 + 0.28 * k for k in range(8)]          # altura de cada brazo (los de afuera, más altos)
HK = [ZK[k] + 0.5 + SIZES[k] for k in range(8)]  # altura del centro de cada planeta

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    try:
        prefs.compute_device_type = "CUDA"; prefs.get_devices()
        for d in prefs.devices: d.use = d.type == "CUDA"
    except Exception: pass
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    if hasattr(sc.cycles, "denoising_use_gpu"): sc.cycles.denoising_use_gpu = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.006, 0.007, 0.016, 1); bg.inputs["Strength"].default_value = 1.0
    return sc

def mat(name, col, rough, metal=0.0, emit=None, coat=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if emit:
        b.inputs["Emission Color"].default_value = (*emit[0], 1); b.inputs["Emission Strength"].default_value = emit[1]
    return m

def link(o): bpy.context.collection.objects.link(o); return o
def mesh_obj(name, bm, m, smooth=True):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    if smooth:
        for p in me.polygons: p.use_smooth = True
    me.materials.append(m); return link(bpy.data.objects.new(name, me))
def sphere(name, r, m, loc, seg=64):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=seg // 2, radius=r)
    o = mesh_obj(name, bm, m); o.location = loc; return o
BEVEL = [False]                                     # en el film: cantos biselados y tapas planas
def cyl(name, r, h, m, loc, seg=48, rot=None):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=h)
    o = mesh_obj(name, bm, m); o.location = loc
    if rot: o.rotation_euler = rot
    if BEVEL[0]:
        for p in o.data.polygons:
            if abs(p.normal.z) > 0.9: p.use_smooth = False
        b = o.modifiers.new("Bevel", "BEVEL"); b.width = min(0.012, r * 0.3, h * 0.3); b.segments = 3
        b.limit_method = "ANGLE"; b.harden_normals = True
    return o
def torus(name, R, r, m, z):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=192, minor_segments=12, location=(0, 0, z))
    o = bpy.context.active_object; o.name = name; o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    return o

M = {}
def build():
    sc = reset()
    M["brass"] = mat("Brass", (0.78, 0.56, 0.26), 0.28, metal=1.0)
    M["brassDark"] = mat("BrassDark", (0.45, 0.32, 0.15), 0.4, metal=1.0)
    M["base"] = mat("Base", (0.012, 0.012, 0.016), 0.35, coat=0.6)
    M["floor"] = mat("Floor", (0.008, 0.009, 0.013), 0.75)
    M["sun"] = mat("Sun", (1.0, 0.5, 0.12), 0.35, emit=((1.0, 0.46, 0.1), 1.6))
    # Piso y pedestal.
    cyl("Floor", 60.0, 0.1, M["floor"], (0, 0, -0.05), seg=96)
    cyl("Plinth", 3.4, 0.6, M["base"], (0, 0, 0.3), seg=128)
    torus("PlinthRing", 3.38, 0.05, M["brass"], 0.6)
    cyl("Column", 0.16, 3.2, M["brass"], (0, 0, 2.2), seg=48)
    sphere("Sun", 0.75, M["sun"], (0, 0, 3.9))
    l = bpy.data.lights.new("SunLight", "POINT"); l.energy = 450; l.color = (1.0, 0.75, 0.45); l.shadow_soft_size = 0.7
    link(bpy.data.objects.new("SunLight", l)).location = (0, 0, 3.9)
    pivots = []
    for k in range(8):
        r = 1.55 + 0.42 * k; z = 1.2 + 0.28 * k
        piv = link(bpy.data.objects.new(f"Pivot{k}", None)); piv.location = (0, 0, 0)
        collar = cyl(f"Collar{k}", 0.24, 0.12, M["brassDark"], (0, 0, z)); collar.parent = piv
        arm = cyl(f"Arm{k}", 0.025, r, M["brass"], (r / 2, 0, z), seg=16, rot=(0, math.pi / 2, 0)); arm.parent = piv
        post = cyl(f"Post{k}", 0.02, 0.5, M["brass"], (r, 0, z + 0.25), seg=12); post.parent = piv
        sat = tuple(max(0.0, min(1.0, (c - 0.55) * 2.2 + 0.3)) for c in LANES[k])
        pm = mat(f"Planet{k}", sat, 0.3, emit=(sat, 0.12), coat=0.5)
        size = SIZES[k]
        pl = sphere(f"Planet{k}", size, pm, (r, 0, z + 0.5 + size)); pl.parent = piv
        piv.rotation_euler.z = k * 2.399      # ángulo dorado: arrancan repartidos
        pivots.append(piv)
        torus(f"Orbit{k}", r, 0.004, M["brassDark"], z + 0.5 + size)
    # Luces de estudio.
    for nm, loc, e, size, col in (("Key", (-9, -7, 9), 2600, 6, (1.0, 0.95, 0.88)), ("Rim", (8, 9, 5), 1600, 5, (0.55, 0.65, 1.0)),
                                  ("Fill", (6, -9, 2), 500, 8, (0.7, 0.75, 1.0))):
        li = bpy.data.lights.new(nm, "AREA"); li.energy = e; li.size = size; li.color = col
        o = link(bpy.data.objects.new(nm, li)); o.location = loc
        o.rotation_euler = (Vector((0, 0, 2.5)) - o.location).to_track_quat("-Z", "Y").to_euler()
    return pivots

def camera(loc, target, lens=45, res=(2400, 1100), fstop=4.0):
    sc = bpy.context.scene
    for o in [o for o in bpy.data.objects if o.type == "CAMERA"]: bpy.data.objects.remove(o)
    cam = bpy.data.cameras.new("Cam"); cam.lens = lens; cam.dof.use_dof = True; cam.dof.aperture_fstop = fstop
    o = link(bpy.data.objects.new("Cam", cam)); sc.camera = o
    o.location = loc; t = Vector(target)
    o.rotation_euler = (t - o.location).to_track_quat("-Z", "Y").to_euler()
    cam.dof.focus_distance = (t - o.location).length
    sc.render.resolution_x, sc.render.resolution_y = res
    return o

def render(path): bpy.context.scene.render.filepath = path; bpy.ops.render.render(write_still=True)

def mode_hero():
    pivots = build()
    shots = {"hero": ((13.5, -10.5, 6.4), (0.4, 0, 2.2), 40, (2400, 1100)),
             "close": ((7.6, -6.2, 5.0), (0.6, 0, 3.0), 50, (1600, 1200)),     # 4:3, las tarjetas de la web
             "top": ((0.01, -1.5, 16.0), (0, 0, 2.0), 40, (1600, 1200))}
    for nm in (arg("--shots") or "hero,close,top").split(","):
        loc, tgt, lens, res = shots[nm]
        camera(Vector(loc), tgt, lens, res)
        render(os.path.join(OUT, f"orrery_{nm}.png"))

def mode_film():
    pivots = build()
    shot = arg("--shot", "wide"); frames = int(arg("--frames", "120"))
    os.makedirs(os.path.join(OUT, f"film_{shot}"), exist_ok=True)
    for f in range(frames):
        t = f / 24.0
        for k, piv in enumerate(pivots): piv.rotation_euler.z = k * 2.399 + t * 2 * math.pi / PERIODS[k] * 1.6
        e = f / max(1, frames - 1); e = e * e * (3 - 2 * e)
        if shot == "wide":
            a = -0.6 + 0.5 * e; d = 13.5 - 1.5 * e
            loc = (d * math.cos(a), d * math.sin(a), 6.0 - 0.8 * e); tgt = (0, 0, 2.6); lens = 40
        elif shot == "close":
            a = -0.9 + 0.35 * e
            loc = (7.4 * math.cos(a), 7.4 * math.sin(a), 5.4 + 0.3 * e); tgt = (0.3, 0, 3.2); lens = 38
        else:
            loc = (0.6 * math.sin(e), -1.0 - 1.5 * e, 15.5 - 2.0 * e); tgt = (0, 0, 2.0); lens = 38
        camera(Vector(loc), tgt, lens, (1920, 1080), 5.6)
        render(os.path.join(OUT, f"film_{shot}", f"f_{f + 1:04d}.png"))


# ── Film de la web (filmkit, --mode film2): el movimiento sale de la música ──
# El audio es first-light.m4a desde FILM_START (el mismo "start" de film.json) y su .mid, que es la misma toma: cada
# carril toca en su canal (canal 1 = carril 0), así cada planeta se enciende con sus propias notas. Verificado: la
# correlación entre las notas del .mid y los ataques del audio tiene su pico en desfase 0.
FPS = 24
FILM_START = 43.8
FILM_SHOTS = [("open", 0.0, 4.8), ("lanes", 4.8, 4.2), ("sun", 9.0, 3.4), ("top", 12.4, 5.0), ("wide", 17.4, 3.2), ("end", 20.6, 3.4)]
FILM_LEN = 24.0

def midi_notes(path):
    """Notas (inicio s, fin s, canal 0-15, velocidad) de un .mid de tipo 0 o 1, con el mapa de tempo."""
    d = open(path, "rb").read(); div = int.from_bytes(d[12:14], "big"); i = 14; ev = []
    def vlq(j):
        v = 0
        while True:
            b = d[j]; j += 1; v = (v << 7) | (b & 0x7F)
            if not b & 0x80: return v, j
    while d[i:i + 4] == b"MTrk":
        end = i + 8 + int.from_bytes(d[i + 4:i + 8], "big"); j = i + 8; tick = 0; st = 0
        while j < end:
            dt, j = vlq(j); tick += dt
            if d[j] == 0xFF:
                ln, k = vlq(j + 2)
                if d[j + 1] == 0x51: ev.append((tick, 0, "tempo", int.from_bytes(d[k:k + ln], "big")))
                j = k + ln; continue
            if d[j] in (0xF0, 0xF7):
                ln, k = vlq(j + 1); j = k + ln; continue
            if d[j] & 0x80: st = d[j]; j += 1
            hi, ch = st & 0xF0, st & 0x0F; n = 1 if hi in (0xC0, 0xD0) else 2; a = d[j:j + n]; j += n
            if hi == 0x90 and a[1] > 0: ev.append((tick, 2, "on", ch, a[0], a[1]))
            elif hi == 0x80 or hi == 0x90: ev.append((tick, 1, "off", ch, a[0]))
        i = end
    ev.sort(key=lambda e: (e[0], e[1]))
    t, last, tempo, held, notes = 0.0, 0, 500000, {}, []
    for e in ev:
        t += (e[0] - last) * tempo / 1e6 / div; last = e[0]
        if e[2] == "tempo": tempo = e[3]
        elif e[2] == "on":
            if (e[3], e[4]) in held: notes.append(held.pop((e[3], e[4])) + (t,))
            held[(e[3], e[4])] = (t, e[3], e[5])
        elif (e[3], e[4]) in held: notes.append(held.pop((e[3], e[4])) + (t,))
    notes += [h + (t,) for h in held.values()]
    return [(a, b, ch, v) for (a, ch, v, b) in notes]

def film_motion(env, notes, n):
    """Por cuadro del film (g = 0..n-1): brillo de cada planeta y de su órbita con las notas de su carril, ángulo de
    cada brazo (velocidad propia, más lenta hacia afuera, con un empujón en cada nota) y la respiración
    del sol (la envolvente del audio, lenta). Las fases de arranque se eligen para que ningún brazo atraviese el
    poste o el planeta de otro durante el film (los brazos de afuera van más altos y barren los radios de adentro)."""
    import random
    pulse = [[0.0] * n for _ in range(8)]; ring = [[0.0] * n for _ in range(8)]
    for (t0, t1, ch, vel) in notes:
        a0, a1 = t0 - FILM_START, t1 - FILM_START
        if ch > 7 or a1 < -4 or a0 > n / FPS: continue
        peak = min(1.0, 0.45 + vel / 90.0)
        for g in range(max(0, int(a0 * FPS)), n):
            T = g / FPS; dt = T - a0
            if dt < 0: continue
            att = min(1.0, dt * FPS / 2.0 + 0.5)                    # ataque de un cuadro y medio
            hold = 1.0 if T < a1 else math.exp(-(T - a1) / 0.4)     # mientras la nota suena queda una brasa
            pulse[ch][g] = max(pulse[ch][g], peak * att * (0.7 * math.exp(-dt / 0.45) + 0.3 * hold))
            ring[ch][g] = max(ring[ch][g], peak * att * (0.6 * math.exp(-dt / 1.1) + 0.25 * hold))
            if T > a1 + 4 and dt > 6: break
    w0 = [0.30 * (RK[0] / RK[k]) ** 0.8 for k in range(8)]          # rad/s: 21 s la vuelta de adentro, 49 s la de afuera
    acc = []
    for k in range(8):
        a, row = 0.0, []
        for g in range(n): row.append(a); a += w0[k] * (1 + 0.45 * pulse[k][g]) / FPS
        acc.append(row)
    pairs = [(k, j) for k in range(8) for j in range(k + 1, min(8, k + 4))]
    span = {(k, j): (min(acc[j][g] - acc[k][g] for g in range(n)), max(acc[j][g] - acc[k][g] for g in range(n))) for (k, j) in pairs}
    def clearance(ph):
        worst = 9.0
        for (k, j) in pairs:
            lo, hi = span[(k, j)]; lo += ph[j] - ph[k]; hi += ph[j] - ph[k]
            m = math.ceil(lo / (2 * math.pi)) * 2 * math.pi            # primer múltiplo de 2π a partir de lo
            c = 0.0 if m <= hi else min(m - hi, lo - (m - 2 * math.pi))
            if ZK[j] - 0.025 > ZK[k] + 0.5 + 2 * SIZES[k] + 0.01: continue   # el brazo pasa por encima del planeta
            worst = min(worst, c * RK[k] - (SIZES[k] + 0.11))
        return worst
    rnd = random.Random(11); best = None
    for _ in range(4000):                                           # sorteo y después ajuste fino de a un brazo
        ph = [rnd.uniform(0, 2 * math.pi) for _ in range(8)]
        c = clearance(ph)
        if best is None or c > best[0]: best = (c, ph)
    for step in (0.2, 0.1, 0.05, 0.02) * 3:
        for k in range(8):
            for d in (-step, step):
                ph = list(best[1]); ph[k] += d; c = clearance(ph)
                if c > best[0]: best = (c, ph)
    ph = best[1]
    ang = [[ph[k] + acc[k][g] for g in range(n)] for k in range(8)]
    es, sun = 0.0, []
    for g in range(n):
        x = env[min(g, len(env) - 1)]; es += (x - es) * (0.22 if x > es else 0.05); sun.append(es)
    lo, hi = min(sun), max(sun)
    sun = [(v - lo) / max(1e-6, hi - lo) for v in sun]
    return dict(pulse=pulse, ring=ring, ang=ang, sun=sun, clearance=best[0])

def planet_at(k, th):
    return (RK[k] * math.cos(th), RK[k] * math.sin(th), HK[k])

# ── Escena del film: el mismo orrery con más oficio de cerca ──
def node(nt, kind, **kw):
    n = nt.nodes.new(kind)
    for k, v in kw.items(): n.inputs[k].default_value = v
    return n

def scuff(m, lo, hi, scale=38.0):
    """Rugosidad que varía apenas (huellas, pulido desparejo): el metal deja de verse de plástico."""
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    nz = node(nt, "ShaderNodeTexNoise", Scale=scale, Detail=6.0)
    mr = node(nt, "ShaderNodeMapRange", **{"To Min": lo, "To Max": hi})
    nt.links.new(nz.outputs["Fac"], mr.inputs["Value"]); nt.links.new(mr.outputs["Result"], b.inputs["Roughness"])

def vivid(c, s_mul=1.7):
    import colorsys
    h, s, v = colorsys.rgb_to_hsv(*c); return colorsys.hsv_to_rgb(h, min(1.0, s * s_mul + 0.12), 1.0)

def sun_material():
    """Sol: emisión con granulado (ruido) y el borde un poco más oscuro; la fuerza la lleva el nodo Breath."""
    m = bpy.data.materials.new("SunFilm"); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear(); L = nt.links.new
    out = nt.nodes.new("ShaderNodeOutputMaterial"); em = nt.nodes.new("ShaderNodeEmission")
    tc = nt.nodes.new("ShaderNodeTexCoord"); nz = node(nt, "ShaderNodeTexNoise", Scale=8.0, Detail=10.0, Roughness=0.62)
    L(tc.outputs["Object"], nz.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); e = ramp.color_ramp.elements
    e[0].position = 0.30; e[0].color = (1.0, 0.11, 0.01, 1); e[1].position = 0.74; e[1].color = (1.0, 0.46, 0.10, 1)
    L(nz.outputs["Fac"], ramp.inputs["Fac"]); L(ramp.outputs["Color"], em.inputs["Color"])
    lw = node(nt, "ShaderNodeLayerWeight", Blend=0.4)
    limb = nt.nodes.new("ShaderNodeMath"); limb.operation = "MULTIPLY_ADD"; limb.inputs[1].default_value = -0.62; limb.inputs[2].default_value = 1.0
    L(lw.outputs["Facing"], limb.inputs[0])
    br = nt.nodes.new("ShaderNodeValue"); br.name = "Breath"; br.outputs[0].default_value = 4.0
    mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"; L(limb.outputs[0], mul.inputs[0]); L(br.outputs[0], mul.inputs[1])
    L(mul.outputs[0], em.inputs["Strength"]); L(em.outputs[0], out.inputs["Surface"])
    return m

def planet_material(k):
    """Planeta: laca con bandas suaves del color del carril y una emisión que las notas encienden."""
    base = tuple(max(0.0, min(1.0, (c - 0.55) * 2.2 + 0.3)) for c in LANES[k])
    m = mat(f"PlanetFilm{k}", base, 0.34, emit=(vivid(LANES[k]), 0.25), coat=0.9)
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]; b.inputs["Coat Roughness"].default_value = 0.05
    tc = nt.nodes.new("ShaderNodeTexCoord")
    wv = node(nt, "ShaderNodeTexWave", Scale=2.2 + 0.3 * k, Distortion=5.0, Detail=3.0); wv.bands_direction = "Z"
    nt.links.new(tc.outputs["Object"], wv.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); e = ramp.color_ramp.elements
    e[0].color = (*[c * 0.62 for c in base], 1); e[1].color = (*[min(1.0, c * 1.08) for c in base], 1)
    nt.links.new(wv.outputs["Fac"], ramp.inputs["Fac"]); nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    return m

def build_film():
    """El orrery de las fotos, con lo que pide una toma de cerca: cantos biselados, bronce con huellas, un sol que
    brilla de verdad (ilumina los brazos), planetas laqueados que se encienden, órbitas que se iluminan con su carril,
    contrapesos y rótulas en los brazos, una copa que sostiene el sol y la escala grabada en el plato."""
    BEVEL[0] = True
    pivots = build()
    sc = bpy.context.scene
    scuff(M["brass"], 0.2, 0.34); scuff(M["brassDark"], 0.3, 0.48, 24.0)
    sun = bpy.data.objects["Sun"]; sun.data.materials[0] = sun_material(); sun.visible_shadow = False
    sunlight = bpy.data.objects["SunLight"]; sunlight.data.shadow_soft_size = 0.6; sunlight.visible_camera = False
    cup = bmesh.new(); bmesh.ops.create_cone(cup, cap_ends=True, segments=64, radius1=0.10, radius2=0.45, depth=0.11)
    o = mesh_obj("SunCup", cup, M["brass"]); o.location = (0, 0, 3.29)
    b = o.modifiers.new("Bevel", "BEVEL"); b.width = 0.01; b.segments = 3; b.harden_normals = True
    torus("SunCupLip", 0.45, 0.018, M["brass"], 3.345)
    planets, glows, rings = [], [], []
    for k, piv in enumerate(pivots):
        r, z, s = RK[k], ZK[k], SIZES[k]
        pl = bpy.data.objects[f"Planet{k}"]; pl.data.materials[0] = planet_material(k); pl.visible_shadow = False
        sphere(f"Elbow{k}", 0.042, M["brass"], (r, 0, z), seg=24).parent = piv
        cyl(f"Cup{k}", 0.055, 0.03, M["brass"], (r, 0, z + 0.5 - 0.012), seg=32).parent = piv
        cyl(f"Counter{k}", 0.02, 0.5, M["brass"], (-0.49, 0, z), seg=16, rot=(0, math.pi / 2, 0)).parent = piv
        sphere(f"Weight{k}", 0.075, M["brassDark"], (-0.78, 0, z), seg=32).parent = piv
        li = bpy.data.lights.new(f"Glow{k}", "POINT"); li.color = vivid(LANES[k]); li.shadow_soft_size = s; li.energy = 0.0
        g = link(bpy.data.objects.new(f"Glow{k}", li)); g.location = (r, 0, HK[k]); g.parent = piv
        old = bpy.data.objects[f"Orbit{k}"]; bpy.data.objects.remove(old)
        rm = mat(f"OrbitFilm{k}", (0.45, 0.32, 0.15), 0.35, metal=1.0, emit=(vivid(LANES[k]), 0.0))
        rings.append(torus(f"Orbit{k}", r, 0.0065, rm, HK[k]))
        planets.append(pl); glows.append(li)
    # La escala grabada en el plato: un anillo fino y 72 marcas de bronce (una larga cada 30°).
    bm = bmesh.new()
    for i in range(72):
        a = i * 2 * math.pi / 72; r1 = 3.2 if i % 6 == 0 else 3.08; w = 0.016 if i % 6 == 0 else 0.01
        mtx = Matrix.Rotation(a, 4, "Z") @ Matrix.Translation(((2.96 + r1) / 2, 0, 0.6015)) @ Matrix.Diagonal((r1 - 2.96, w, 0.003, 1))
        bmesh.ops.create_cube(bm, size=1.0, matrix=mtx)
    mesh_obj("Scale", bm, M["brass"], smooth=False)
    ring = torus("ScaleRing", 2.92, 0.012, M["brass"], 0.6); ring.scale.z = 0.3
    # Luz que barre el bronce (se mueve y se enciende en algunas tomas).
    bpy.data.objects["Fill"].visible_glossy = False        # su reflejo lechoso tapaba el plato oscuro
    rim = bpy.data.objects["Rim"]; rim.location = (8, 10, 11)   # más alto: su reflejo cae antes del plato en las tomas bajas
    rim.rotation_euler = (Vector((0, 0, 2.5)) - rim.location).to_track_quat("-Z", "Y").to_euler()
    pb = M["base"].node_tree.nodes["Principled BSDF"]           # plato: laca más fina, el contraluz no lo lava
    pb.inputs["Coat Weight"].default_value = 0.22; pb.inputs["Roughness"].default_value = 0.5
    li = bpy.data.lights.new("Sweep", "AREA"); li.energy = 0.0; li.size = 1.6; li.color = (1.0, 0.86, 0.66)
    sweep = link(bpy.data.objects.new("Sweep", li))
    return dict(sc=sc, pivots=pivots, planets=planets, glows=glows, rings=rings, sun=sun, sunlight=sunlight, sweep=sweep,
                breath=sun.data.materials[0].node_tree.nodes["Breath"])

def aim(cam, path, u, rz=0.0, focus=None, shake=0.0, t=0.0):
    """Como Path.apply de filmkit, pero el recorrido puede girar alrededor del eje del orrery (rz): así la cámara
    acompaña a un planeta en su órbita."""
    import filmkit as fk
    pos, look, lens, fs = path.at(u)
    R = Matrix.Rotation(rz, 3, "Z"); pos = R @ pos; look = R @ look
    if shake > 0:
        pos = pos + Vector((fk.noise1(t, 1), fk.noise1(t, 2), fk.noise1(t, 3))) * shake
        look = look + Vector((fk.noise1(t, 4), fk.noise1(t, 5), fk.noise1(t, 6))) * shake * 0.6
    cam.location = pos; cam.rotation_euler = (look - pos).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    if fs is not None: cam.data.dof.aperture_fstop = fs
    target = Vector(focus) if focus is not None else look
    cam.data.dof.focus_distance = max(0.01, (target - pos).dot((look - pos).normalized()))

def polar(az, rad, z): a = math.radians(az); return (rad * math.cos(a), rad * math.sin(a), z)

def off_look(cam, target, yaw, pitch):
    """Punto de mira corrido para que target quede fuera del centro: yaw grados a la derecha del cuadro, pitch arriba."""
    c, t = Vector(cam), Vector(target); d = t - c
    d = Matrix.Rotation(math.radians(yaw), 3, "Z") @ d
    h = math.hypot(d.x, d.y); d.z -= h * math.tan(math.radians(pitch))
    return tuple(c + d)

def film2():
    """Film de la web, 24 s sobre first-light (desde 43,8 s), seis tomas. Escala: 1 = 10 cm, por eso los f-stop de
    la escena son los de una cámara real divididos por 10 (f/0.4 aquí se ve como f/4).

      open   0,0–4,8   Contrapicado al ras del plato que avanza y sube hacia el sol (arriba a la derecha); los
                       planetas cruzan por delante. El tercio de abajo a la izquierda queda oscuro para el título.
                       El planeta del carril 3 (la melodía) se enciende en cada nota.
      lanes  4,8–9,0   Macro entre los brazos con tele: la cámara acompaña al planeta del carril 3 en su órbita;
                       entra el acorde (carriles 1, 2, 3, 5, 6 se encienden y se aceleran) y el foco pasa del
                       planeta 3 al 2, que toca enseguida.
      sun    9,0–12,4  Desde abajo de los brazos, la columna de collares hacia el sol que respira con la envolvente;
                       foco de los collares al sol, una luz cálida barre el bronce.
      top   12,4–17,4  Revelación cenital: de tres cuartos a vista desde arriba, girando; se leen las órbitas, la
                       escala grabada y los planetas a velocidades distintas (en el silencio de la frase).
      wide  17,4–20,6  Plano general bajo con arco lento; vuelve la melodía y una luz barre el plato.
      end   20,6–24,0  Se aleja despacio desde arriba; el último acorde enciende todo bajo el cierre.
    Todo lo que se mueve sale del audio y de su .mid: brillo y empujón de cada planeta y su órbita con las notas de
    su carril, el sol (emisión, luz y un leve pulso de tamaño) con la envolvente. Las fases de arranque de los brazos
    se eligen para que ninguno atraviese el poste o el planeta de otro (ver film_motion)."""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final"); step = int(arg("--step", "1"))
    wav = arg("--audio"); mid = arg("--midi") or os.path.splitext(wav)[0] + ".mid"
    env = fk.envelope(wav, start=FILM_START)
    n = int(round(FILM_LEN * FPS)) + 4
    mo = film_motion(env, midi_notes(mid), n)
    print(f"orrery film2: holgura mínima entre brazos {mo['clearance']:.2f}", flush=True)
    ang, pulse, ring, sun = mo["ang"], mo["pulse"], mo["ring"], mo["sun"]
    P = lambda k, g: Vector(planet_at(k, ang[k][min(n - 1, g)]))
    for (name, t0, dur) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        S = build_film(); sc = S["sc"]
        fk.setup(sc, quality)
        cam = fk.camera(sc)
        frames = int(round(dur * FPS)); f0 = int(round(t0 * FPS))
        gm = f0 + frames // 2
        th5 = lambda g: ang[5][min(n - 1, g)]
        paths = {
            "open":  fk.Path([(0, polar(-55, 10.5, 0.8), off_look(polar(-55, 10.5, 0.8), (0, 0, 3.9), 14, 6), 40, 0.45),
                              (1, polar(-49, 8.4, 1.2), off_look(polar(-49, 8.4, 1.2), (0, 0, 3.9), 15, 6), 42, 0.45)]),
            # en el marco del planeta 6 (carril 5) puesto en el ángulo 0: él cerca arriba a la derecha, el 3 lejos abajo
            "lanes": fk.Path([(0, (5.0, -2.2, 3.6), (3.0, 0.3, 3.1), 75, 0.2),
                              (1, (4.85, -1.95, 3.7), (2.95, 0.35, 2.85), 75, 0.2)], easing="smooth"),
            "sun":   fk.Path([(0, polar(-40, 3.0, 0.95), (0.0, 0.0, 3.0), 38, 0.35),
                              (1, polar(-28, 2.6, 1.35), (0.0, 0.0, 3.35), 40, 0.35)]),
            "top":   fk.Path([(0, polar(-75, 9.5, 6.6), (0.0, 0.0, 1.6), 34, 0.9),
                              (0.55, polar(-45, 4.6, 12.0), (0.0, 0.0, 1.6), 34, 0.9),
                              (1, polar(-20, 0.25, 15.2), (0.0, 0.0, 1.6), 36, 0.9)]),
            "wide":  fk.Path([(0, polar(-118, 12.5, 3.0), (0.2, 0.0, 2.3), 40, 0.6),
                              (1, polar(-96, 12.0, 3.4), (0.1, 0.0, 2.35), 40, 0.6)], easing="smooth"),
            "end":   fk.Path([(0, polar(-72, 9.8, 5.6), (0.0, 0.0, 2.3), 40, 0.7),
                              (1, polar(-68, 13.2, 6.9), (0.0, 0.0, 2.2), 40, 0.7)], easing="out"),
        }
        path = paths[name]
        sw = S["sweep"]

        def pose(f, t, u, name=name, path=path, f0=f0, gm=gm):
            g = f0 + f - 1
            for k, piv in enumerate(S["pivots"]): piv.rotation_euler.z = ang[k][g]
            S["sun"].scale = [1.0 + 0.014 * sun[g]] * 3
            rz, focus, shake = 0.0, None, 0.0
            SUN = Vector((0, 0, 3.9)); front = SUN + (path.at(u)[0] - SUN).normalized() * 0.75     # cara del sol que mira a cámara
            if name == "open": focus = front; shake = 0.008
            if name == "lanes":
                rz = th5(gm) + 0.8 * (th5(g) - th5(gm)); shake = 0.006       # el acorde (6 s) con el 6 nítido; el 3 toca a los 7,8 s
                focus = P(5, g).lerp(P(2, g), fk.rack(u, 0.45, 0.68, 0.0, 1.0))
            if name == "sun":
                focus = Vector((0, 0, 2.9)).lerp(front, fk.rack(u, 0.25, 0.7, 0.0, 1.0)); shake = 0.006
                sw.location = Vector(polar(-80 + 70 * u, 2.4, 2.2 + 1.2 * u))
                sw.rotation_euler = (Vector((0, 0, 2.8)) - sw.location).to_track_quat("-Z", "Y").to_euler()
            if name == "wide":
                sw.location = Vector(polar(-170 + 110 * fk.ease(u), 5.5, 2.4))
                sw.rotation_euler = (Vector((0, 0, 0.6)) - sw.location).to_track_quat("-Z", "Y").to_euler()
            if name == "top": focus = Vector((0, 0, 2.6))
            aim(cam, path, u, rz=rz, focus=focus, shake=shake, t=t0 + t)
            return list(S["pivots"]) + [S["sun"], sw]

        def keys(f, name=name, f0=f0, frames=frames):
            g = f0 + f - 1; u = (f - 1) / max(1, frames - 1)
            S["breath"].outputs[0].default_value = 1.1 + 1.9 * sun[g]; S["breath"].outputs[0].keyframe_insert("default_value")
            S["sunlight"].data.energy = 240 + 380 * sun[g]; S["sunlight"].data.keyframe_insert("energy")
            for k in range(8):
                b = S["planets"][k].data.materials[0].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
                b.default_value = 0.12 + 9.0 * pulse[k][g]; b.keyframe_insert("default_value")
                S["glows"][k].energy = 45.0 * pulse[k][g]; S["glows"][k].keyframe_insert("energy")
                r = S["rings"][k].data.materials[0].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
                r.default_value = 6.0 * ring[k][g]; r.keyframe_insert("default_value")
            e = {"sun": 160.0 * math.sin(math.pi * u), "wide": 90.0 * math.sin(math.pi * u)}.get(name, 0.0)
            sw.data.energy = e; sw.data.keyframe_insert("energy")

        fk.render_shot(sc, name, frames, pose, OUT, step=step, extra_keys=keys)

{"hero": mode_hero, "film": mode_film, "film2": film2}[arg("--mode", "hero")]()
