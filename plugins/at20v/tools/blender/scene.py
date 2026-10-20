"""Escena Blender del DH-20: renderiza el fondo del panel y los sprites de la interfaz.

Corre en pc50-wsl (RTX 3090, Cycles + OptiX):
  blender -b -P scene.py -- --layout layout.json --silk silkscreen.png --out out/ --mode all
  blender -b --factory-startup -P scene.py -- --mode film2 [--shot open,keys] [--quality draft|final]
          --audio film.wav --out out/      (film de la web; ver film2 más abajo)

Convenciones:
  - 1 unidad de Blender (BU) = 10 unidades de panel. Panel (u, v) con v hacia abajo → (u/10, -v/10, 0).
  - Cámara ortográfica inclinada THETA alrededor de X: un punto (x, y, z) cae en pantalla en
    sx = x, sy = y·cosθ + z·sinθ. Todo (fondo y sprites) usa la misma orientación y la misma luz
    direccional, así un único filmstrip de perilla sirve para cualquier posición del panel.
  - Resolución: PX_PER_UNIT píxeles por unidad de panel (2 = retina del tamaño 1x).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Euler, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default

# Sin --layout/--silk (film2) se toman los del repo, al lado de este script.
_ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets")
def _default(name, repo):
    return name if os.path.exists(name) else os.path.join(_ASSETS, repo)
LAYOUT = json.load(open(arg("--layout", _default("layout.json", "layout.json"))))
SILK = os.path.abspath(arg("--silk", _default("silkscreen.png", os.path.join("render", "silkscreen.png"))))
OUT = os.path.abspath(arg("--out", "out"))
MODE = arg("--mode", "all")
SAMPLES = int(arg("--samples", "256"))
FRAMES = int(arg("--frames", "128"))
WHEEL_ONLY = arg("--wheel-only", "0") == "1"      # modo keys: sólo los cuadros de la rueda (out/keys/wheel_*.png)
os.makedirs(OUT, exist_ok=True)

THETA = math.radians(12.0)
PX_PER_UNIT = 2.0
PW, PH = LAYOUT["panel"]["w"], LAYOUT["panel"]["h"]
# Marco del render de fondo (unidades de panel): gabinete alrededor del panel.
CASE_L, CASE_R, CASE_T, CASE_B = 46.0, 46.0, 58.0, 26.0

def P(u, v, z=0.0):
    return Vector((u / 10.0, -v / 10.0, z / 10.0))

# ───────────────────────── escena base ─────────────────────────
def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for dev_type in ("CUDA",):             # OptiX no inicia en WSL y Cycles cae a CPU sin avisar
        try:
            prefs.compute_device_type = dev_type
            prefs.get_devices()
            if any(d.type == dev_type for d in prefs.devices):
                for d in prefs.devices: d.use = d.type == dev_type
                break
        except Exception:
            pass
    sc.cycles.device = "GPU"
    if hasattr(sc.cycles, "denoising_use_gpu"): sc.cycles.denoising_use_gpu = True   # OIDN en la GPU: en CPU era el cuello de botella
    print("cycles:", prefs.compute_device_type, [(d.name, d.type, d.use) for d in prefs.devices], flush=True)
    sc.cycles.samples = SAMPLES
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 8
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.look = "AgX - Medium High Contrast"
    sc.view_settings.exposure = 0.0
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.render.image_settings.color_depth = "8"
    world = bpy.data.worlds.new("W"); sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.55, 0.57, 0.62, 1.0)
    bg.inputs["Strength"].default_value = 0.06
    return sc

def add_lights():
    # Luz principal: arriba a la izquierda, algo hacia el frente (como la foto de referencia).
    key = bpy.data.lights.new("Key", "SUN"); key.energy = 3.4; key.angle = math.radians(18)
    key.color = (1.0, 0.97, 0.93)
    k = bpy.data.objects.new("Key", key); bpy.context.collection.objects.link(k)
    k.rotation_euler = Euler((math.radians(38), math.radians(-24), math.radians(-20)), "XYZ")
    fill = bpy.data.lights.new("Fill", "SUN"); fill.energy = 0.55; fill.angle = math.radians(40)
    fill.color = (0.85, 0.9, 1.0)
    f = bpy.data.objects.new("Fill", fill); bpy.context.collection.objects.link(f)
    f.rotation_euler = Euler((math.radians(-20), math.radians(30), 0), "XYZ")
    rim = bpy.data.lights.new("Rim", "SUN"); rim.energy = 0.8; rim.angle = math.radians(10)
    r = bpy.data.objects.new("Rim", rim); bpy.context.collection.objects.link(r)
    r.rotation_euler = Euler((math.radians(-60), 0, math.radians(15)), "XYZ")

def add_camera(cx_u, cy_u, width_u, height_u, res_x, res_y, theta=None):
    """Cámara ortográfica inclinada centrada en el punto de panel (cx_u, cy_u) a z=0."""
    theta = THETA if theta is None else theta
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("Cam"); cam.type = "ORTHO"
    cam.sensor_fit = "HORIZONTAL"
    cam.ortho_scale = width_u / 10.0
    cam.clip_start = 0.1; cam.clip_end = 500
    obj = bpy.data.objects.new("Cam", cam); bpy.context.collection.objects.link(obj)
    target = P(cx_u, cy_u)
    view = Vector((0, math.sin(theta), -math.cos(theta)))   # mirando hacia abajo y hacia +y
    obj.location = target - view * 200.0
    obj.rotation_euler = Euler((theta, 0, 0), "XYZ")       # 0 = mirando −Z; inclinar hacia +Y
    sc.camera = obj
    sc.render.resolution_x = res_x; sc.render.resolution_y = res_y
    sc.render.resolution_percentage = 100
    return obj

# ───────────────────────── materiales ─────────────────────────
def principled(name, color, rough, metal=0.0, coat=0.0, spec=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    return m

def panel_material():
    """Aluminio pintado gris carbón con textura de pintura en polvo y serigrafía blanca."""
    m = bpy.data.materials.new("Panel"); m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(SILK)
    tex.interpolation = "Cubic"; tex.extension = "CLIP"
    uv = N.new("ShaderNodeTexCoord")
    Lk.new(uv.outputs["UV"], tex.inputs["Vector"])
    noise = N.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value = 900.0
    noise.inputs["Detail"].default_value = 6.0
    Lk.new(uv.outputs["Object"], noise.inputs["Vector"])
    big = N.new("ShaderNodeTexNoise"); big.inputs["Scale"].default_value = 3.0
    big.inputs["Detail"].default_value = 3.0
    Lk.new(uv.outputs["Object"], big.inputs["Vector"])
    # Pintura: gris carbón con variación muy leve (manchas grandes) para que no sea plano.
    paint = N.new("ShaderNodeMix"); paint.data_type = "RGBA"
    paint.inputs["A"].default_value = (0.021, 0.021, 0.023, 1)
    paint.inputs["B"].default_value = (0.029, 0.029, 0.031, 1)
    Lk.new(big.outputs["Fac"], paint.inputs["Factor"])
    ink = N.new("ShaderNodeMix"); ink.data_type = "RGBA"
    Lk.new(tex.outputs["Alpha"], ink.inputs["Factor"])
    Lk.new(paint.outputs["Result"], ink.inputs["A"])
    ink.inputs["B"].default_value = (0.70, 0.69, 0.66, 1)
    Lk.new(ink.outputs["Result"], b.inputs["Base Color"])
    rough = N.new("ShaderNodeMapRange")
    Lk.new(noise.outputs["Fac"], rough.inputs["Value"])
    rough.inputs["To Min"].default_value = 0.52; rough.inputs["To Max"].default_value = 0.68
    rmix = N.new("ShaderNodeMix"); rmix.data_type = "FLOAT"
    Lk.new(tex.outputs["Alpha"], rmix.inputs["Factor"])
    Lk.new(rough.outputs["Result"], rmix.inputs["A"]); rmix.inputs["B"].default_value = 0.42
    Lk.new(rmix.outputs["Result"], b.inputs["Roughness"])
    # Relieve: grano de la pintura + la tinta apenas sobresale.
    hsum = N.new("ShaderNodeMath"); hsum.operation = "MULTIPLY_ADD"
    Lk.new(noise.outputs["Fac"], hsum.inputs[0]); hsum.inputs[1].default_value = 0.15
    Lk.new(tex.outputs["Alpha"], hsum.inputs[2])
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08
    bump.inputs["Distance"].default_value = 0.002
    Lk.new(hsum.outputs["Value"], bump.inputs["Height"])
    Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = 0.35
    return m

MAT = {}
def materials():
    MAT["panel"] = panel_material()
    MAT["knob"] = principled("KnobPlastic", (0.012, 0.012, 0.013), 0.32, coat=0.15, spec=0.55)
    MAT["knob_skirt"] = principled("KnobSkirt", (0.010, 0.010, 0.011), 0.4, spec=0.5)
    MAT["indicator"] = principled("Indicator", (0.80, 0.79, 0.76), 0.45)
    MAT["nickel"] = principled("Nickel", (0.72, 0.70, 0.66), 0.28, metal=1.0)
    MAT["nickel_dull"] = principled("NickelDull", (0.55, 0.54, 0.52), 0.42, metal=1.0)
    MAT["hole"] = principled("Hole", (0.004, 0.004, 0.004), 0.9)
    MAT["screw"] = principled("Screw", (0.08, 0.08, 0.085), 0.35, metal=1.0)
    MAT["case"] = principled("Case", (0.018, 0.018, 0.019), 0.62)
    MAT["cheek"] = principled("Cheek", (0.022, 0.021, 0.021), 0.75)
    MAT["led_off"] = principled("LedOff", (0.18, 0.02, 0.02), 0.15, coat=0.6)
    MAT["trim_hole"] = principled("TrimHole", (0.006, 0.006, 0.006), 0.6)

# ───────────────────────── geometría ─────────────────────────
def link(obj):
    bpy.context.collection.objects.link(obj); return obj

def mesh_obj(name, bm, mat, smooth=True):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    if smooth:
        for p in me.polygons: p.use_smooth = True
    me.materials.append(mat)
    o = link(bpy.data.objects.new(name, me))
    if smooth:
        # Normales ponderadas por área: las caras grandes quedan planas y sólo el bisel se redondea. Sin esto,
        # el sombreado suave doblaba los brillos de las teclas (se veían torcidas, con reflejos en horquilla).
        wn = o.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL"); wn.mode = "FACE_AREA"; wn.keep_sharp = True
    return o

def lathe(profile, segs=96, radial=None):
    """Sólido de revolución. profile: lista (r, z) en unidades de panel, de abajo hacia arriba.
    radial(theta, r, z) → r modulado (estrías)."""
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        ring = []
        for i in range(segs):
            t = 2 * math.pi * i / segs
            rr = radial(t, r, z) if radial else r
            ring.append(bm.verts.new((rr * math.cos(t) / 10, rr * math.sin(t) / 10, z / 10)))
        rings.append(ring)
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(segs):
            j = (i + 1) % segs
            bm.faces.new((a[i], a[j], b[j], b[i]))
    top = rings[-1]
    if profile[-1][0] > 0:
        c = bm.verts.new((0, 0, profile[-1][1] / 10))
        for i in range(segs):
            bm.faces.new((top[i], top[(i + 1) % segs], c))
    return bm

def knob_object(kind, name="Knob"):
    """Perilla del MS-20: faldón ancho, cuerpo estriado algo cónico, tapa apenas abombada y
    línea indicadora blanca que cruza la tapa y baja por el costado."""
    s = 1.0 if kind == "small" else 1.36
    R = LAYOUT["knobs"][0]["radius"]  # sólo para referencia
    rs = 25.0 * s           # radio del faldón
    rb0, rb1 = 18.6 * s, 17.4 * s   # cuerpo: abajo / arriba
    hs, hb = 4.0 * s, 40.0 * s      # alto del faldón / del cuerpo
    flutes = 26 if kind == "small" else 30
    depth = 0.9 * s

    def radial(t, r, z):
        if z <= hs * 1.6 or z >= hb - 1.2 * s: return r
        f = 0.5 + 0.5 * math.cos(t * flutes)
        return r - depth * (f ** 6) * 2.0

    prof = [(rs, 0.0), (rs, hs * 0.55), (rs - 0.6 * s, hs), (rb0 + 2.2 * s, hs + 1.2 * s),
            (rb0, hs * 1.6), (rb0 - 0.05, hs * 2.2)]
    steps = 14
    for i in range(1, steps + 1):
        z = hs * 2.2 + (hb - 1.5 * s - hs * 2.2) * i / steps
        prof.append((rb0 + (rb1 - rb0) * i / steps, z))
    prof += [(rb1 - 0.5 * s, hb - 0.5 * s), (rb1 - 1.3 * s, hb), (rb1 * 0.6, hb + 0.5 * s), (0.0, hb + 0.7 * s)]
    body = mesh_obj(name, lathe(prof, segs=160, radial=radial), MAT["knob"])
    body.data.materials.append(MAT["knob_skirt"])
    # Faldón con otro material (más mate): caras por debajo de hs.
    for p in body.data.polygons:
        if p.center.z < hs / 10 * 1.05: p.material_index = 1
    # Línea indicadora: tira sobre la tapa (hacia +Y = arriba del panel) y por el costado.
    bm = bmesh.new()
    w = 1.25 * s
    top_z = hb + 0.62 * s
    def quad(a, b, c, d):
        vs = [bm.verts.new(Vector(v) / 10) for v in (a, b, c, d)]
        bm.faces.new(vs)
    # Sobre la tapa: sigue el abombado (z de la tapa + 0.12) en tramos.
    def cap_z(r):
        pts = [(0.0, hb + 0.7 * s), (rb1 * 0.6, hb + 0.5 * s), (rb1 - 1.3 * s, hb)]
        for (r0, z0), (r1, z1) in zip(pts[:-1], pts[1:]):
            if r <= r1: return z0 + (z1 - z0) * (r - r0) / (r1 - r0)
        return hb
    rs_ = [2.2 * s + (rb1 - 1.3 * s - 2.2 * s) * i / 6 for i in range(7)]
    for r0, r1 in zip(rs_[:-1], rs_[1:]):
        quad((-w / 2, r0, cap_z(r0) + 0.12), (w / 2, r0, cap_z(r0) + 0.12),
             (w / 2, r1, cap_z(r1) + 0.12), (-w / 2, r1, cap_z(r1) + 0.12))
    quad((-w / 2, rb1 - 1.3 * s, hb + 0.12), (w / 2, rb1 - 1.3 * s, hb + 0.12),
         (w / 2, rb1 - 1.0 * s, hb + 0.15), (-w / 2, rb1 - 1.0 * s, hb + 0.15))
    # costado: de la tapa al faldón, siguiendo la conicidad
    quad((-w / 2, rb1 - 1.0 * s, hb + 0.15), (w / 2, rb1 - 1.0 * s, hb + 0.15),
         (w / 2, rb1 + 0.12, hb - 1.2 * s), (-w / 2, rb1 + 0.12, hb - 1.2 * s))
    quad((-w / 2, rb1 + 0.12, hb - 1.2 * s), (w / 2, rb1 + 0.12, hb - 1.2 * s),
         (w / 2, rb0 + 0.12, hs * 2.2), (-w / 2, rb0 + 0.12, hs * 2.2))
    ind = mesh_obj(name + "_ind", bm, MAT["indicator"], smooth=False)
    ind.parent = body
    return body

def jack_object(name="Jack"):
    """Jack del MS-20: arandela, tuerca hexagonal niquelada y buje con el agujero negro."""
    R = LAYOUT["jackRadius"]
    objs = []
    washer = mesh_obj(name + "_w", lathe([(R * 1.0, 0.0), (R * 1.0, 0.8), (R * 0.95, 1.0), (0.0, 1.0)], 64), MAT["nickel_dull"])
    nut = mesh_obj(name + "_n", lathe([(R * 0.98, 1.0), (R * 0.98, 6.0), (R * 0.90, 7.0), (0.0, 7.0)], 6), MAT["nickel"], smooth=False)
    nut.rotation_euler.z = math.radians(30)
    bush = mesh_obj(name + "_b", lathe([(R * 0.62, 6.5), (R * 0.62, 10.5), (R * 0.56, 11.5), (R * 0.36, 11.5),
                                        (R * 0.33, 11.0), (R * 0.33, 3.0), (0.0, 3.0)], 64), MAT["nickel"])
    hole = mesh_obj(name + "_h", lathe([(R * 0.33, 3.2), (0.0, 3.2)], 48), MAT["hole"])
    for o in (nut, bush, hole): o.parent = washer
    return washer

def screw_object(name="Screw"):
    s = mesh_obj(name, lathe([(9.0, 0), (9.0, 1.2), (8.0, 3.2), (5.0, 4.4), (0.0, 4.8)], 48), MAT["screw"])
    # Ranura Phillips: dos cajas oscuras.
    for ang in (0, 90):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x *= 1.2; v.co.y *= 0.18; v.co.z *= 0.15
            v.co.z += 0.47
        c = mesh_obj(name + f"_x{ang}", bm, MAT["hole"], smooth=False)
        c.rotation_euler.z = math.radians(ang + 20); c.parent = s
    return s

def led_object(name="Led", mat=None):
    return mesh_obj(name, lathe([(4.2, 0), (4.2, 1.5), (3.2, 3.6), (0, 4.4)], 48), mat or MAT["led_off"])

def trim_hole_object(name="Trim"):
    o = mesh_obj(name, lathe([(10.5, 0.0), (10.5, 0.4), (8.4, 0.4), (8.4, -2.0), (0.0, -2.0)], 64), MAT["trim_hole"])
    return o

def panel_object():
    bm = bmesh.new()
    verts = [bm.verts.new(P(u, v, -0.0)) for u, v in ((0, 0), (PW, 0), (PW, PH), (0, PH))]
    f = bm.faces.new(verts)
    uvl = bm.loops.layers.uv.new()
    for loop in f.loops:
        co = loop.vert.co
        loop[uvl].uv = (co.x * 10 / PW, 1.0 + co.y * 10 / PH)
    return mesh_obj("Panel", bm, MAT["panel"], smooth=False)

def box(name, u0, v0, u1, v1, z0, z1, mat):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    c = (P((u0 + u1) / 2, (v0 + v1) / 2, (z0 + z1) / 2))
    for v in bm.verts:
        v.co.x = c.x + v.co.x * (u1 - u0) / 10
        v.co.y = c.y + v.co.y * (v1 - v0) / 10
        v.co.z = c.z + v.co.z * (z1 - z0) / 10
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.12, segments=3, affect="EDGES")
    return mesh_obj(name, bm, mat)

def case_objects():
    # Mejillas laterales (más altas que el panel) y labio superior.
    box("CheekL", -CASE_L, -CASE_T - 10, -2, PH + CASE_B + 10, -6, 18, MAT["cheek"])
    box("CheekR", PW + 2, -CASE_T - 10, PW + CASE_R, PH + CASE_B + 10, -6, 18, MAT["cheek"])
    box("Lip", -2, -CASE_T - 10, PW + 2, -3, -4, 10, MAT["case"])
    box("Bottom", -2, PH + 3, PW + 2, PH + CASE_B + 10, -6, 4, MAT["case"])
    # Tacos de goma del labio superior (se ven en la foto de referencia).
    rubber = principled("Rubber", (0.012, 0.012, 0.013), 0.85)
    for u in (190.0, 570.0, 965.0, 1208.0):
        box(f"Foot{int(u)}", u - 34, -CASE_T + 8, u + 34, -CASE_T + 40, 10, 13, rubber)

# ───────────────────────── modos de render ─────────────────────────
def place(obj, u, v, rot_deg=0.0):
    obj.location = P(u, v)
    obj.rotation_euler.z = -math.radians(rot_deg)

def default_angle(k):
    # Posición de reposo para el fondo: no importa (la perilla tapa su propia sombra rotada),
    # pero la línea va a 0 para que la sombra del indicador no aparezca corrida.
    return k["startDeg"]

def build_panel_scene(knobs_visible=True):
    reset_scene(); materials(); add_lights()
    panel_object(); case_objects()
    proto = {"small": knob_object("small", "KnobSmall"), "big": knob_object("big", "KnobBig")}
    proto["small"].hide_render = proto["big"].hide_render = True
    for ch in proto["small"].children + proto["big"].children: ch.hide_render = True
    knobs = []
    for k in LAYOUT["knobs"]:
        o = proto[k["kind"]].copy(); link(o); o.hide_render = False
        ind = proto[k["kind"]].children[0].copy(); link(ind); ind.parent = o; ind.hide_render = False
        place(o, *k["pos"], default_angle(k))
        for x in (o, ind):
            x.visible_camera = knobs_visible
        knobs.append(o)
    jp = jack_object("JackProto")
    for j in LAYOUT["jacks"]:
        o = jp.copy(); link(o)
        for ch in jp.children:
            c2 = ch.copy(); link(c2); c2.parent = o
        place(o, *j["pos"])
    for ch in jp.children: ch.hide_render = True
    jp.hide_render = True
    for i, (u, v) in enumerate(LAYOUT["screws"]):
        s = screw_object(f"Screw{i}"); place(s, u, v, 20 * i)
    for i, (u, v) in enumerate(LAYOUT["holes"]):
        t = trim_hole_object(f"Trim{i}"); place(t, u, v)
    for l in LAYOUT["leds"]:
        o = led_object("Led_" + l["id"]); place(o, *l["pos"])
    return knobs

def render_to(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)

def bg_frame():
    u0, u1 = -CASE_L, PW + CASE_R
    v0, v1 = -CASE_T, PH + CASE_B
    w_u, h_u = u1 - u0, (v1 - v0) * math.cos(THETA)
    res_x = int(round(w_u * PX_PER_UNIT)); res_y = int(round(h_u * PX_PER_UNIT))
    return u0, v0, u1, v1, w_u, h_u, res_x, res_y

def mode_bg():
    build_panel_scene(knobs_visible=False)
    u0, v0, u1, v1, w_u, h_u, rx, ry = bg_frame()
    add_camera((u0 + u1) / 2, (v0 + v1) / 2, w_u, h_u, rx, ry)
    render_to(os.path.join(OUT, "background.png"))
    meta = dict(theta=math.degrees(THETA), pxPerUnit=PX_PER_UNIT, u0=u0, v0=v0, u1=u1, v1=v1,
                width=rx, height=ry, cosTheta=math.cos(THETA), sinTheta=math.sin(THETA))
    json.dump(meta, open(os.path.join(OUT, "background.json"), "w"), indent=1)

def mode_preview():
    """Fondo con perillas visibles (para revisar el look completo)."""
    build_panel_scene(knobs_visible=True)
    u0, v0, u1, v1, w_u, h_u, rx, ry = bg_frame()
    add_camera((u0 + u1) / 2, (v0 + v1) / 2, w_u, h_u, rx, ry)
    render_to(os.path.join(OUT, "preview.png"))

def sprite_setup(center_u, center_v, size_u):
    """Encuadre cuadrado alrededor de un punto del panel. El panel queda como holdout: aporta
    luz rebotada y oclusión pero sale transparente."""
    sc = bpy.context.scene
    sc.render.film_transparent = True
    res = int(round(size_u * PX_PER_UNIT))
    add_camera(center_u, center_v, size_u, size_u, res, res)
    for o in bpy.data.objects:
        if o.name.startswith(("Panel", "Cheek", "Lip", "Bottom")): o.is_holdout = True
    return res

def mode_knobs():
    for kind in ("small", "big"):
        reset_scene(); materials(); add_lights(); panel_object()
        o = knob_object(kind, "Knob")
        cu, cv = PW / 2, PH / 2
        place(o, cu, cv)
        size_u = 84.0 if kind == "small" else 112.0
        # La perilla se ve corrida hacia arriba (su altura proyectada): centrar el cuadro un poco
        # más arriba. El ancla (base de la perilla) queda documentada en el json.
        lift = 18.0 if kind == "small" else 24.0
        res = sprite_setup(cu, cv - lift / math.cos(THETA), size_u)
        sc = bpy.context.scene
        sc.frame_start = 0; sc.frame_end = FRAMES - 1
        o.rotation_mode = "XYZ"
        for f in range(FRAMES):
            sc.frame_set(f)
            # Fotograma f cubre 0..360° en pasos iguales: el editor elige el más cercano al ángulo.
            o.rotation_euler.z = -math.radians(360.0 * f / FRAMES)
            render_to(os.path.join(OUT, f"knob_{kind}", f"{f:03d}.png"))
        anchor_y = res / 2 + lift * PX_PER_UNIT   # base de la perilla en px dentro del cuadro
        json.dump(dict(size=res, frames=FRAMES, anchorX=res / 2, anchorY=anchor_y, degPerFrame=360.0 / FRAMES),
                  open(os.path.join(OUT, f"knob_{kind}.json"), "w"), indent=1)

def plug_object(color, name="Plug"):
    """Ficha de patcheo de 6,35 mm vista enchufada: virola niquelada, capuchón de color con
    estrías de agarre, alivio de tensión y el arranque del cable (el resto lo dibuja el editor)."""
    body = principled("PlugBody", color, 0.36, coat=0.35)
    dark = principled("PlugRelief", tuple(c * 0.55 for c in color), 0.5, coat=0.1)
    ferrule = mesh_obj(name + "_f", lathe([(11.8, 10.5), (12.2, 12.0), (12.2, 17.0), (11.4, 18.0), (0.0, 18.0)], 64),
                       MAT["nickel"])
    grip = lambda t, r, z: r - (0.55 if (22.0 < z < 46.0 and math.cos(t * 16) > 0.55) else 0.0)
    boot = mesh_obj(name, lathe([(13.6, 17.5), (15.2, 19.5), (15.4, 22.0), (15.0, 46.0), (14.0, 49.0), (11.0, 51.0),
                                 (0.0, 51.0)], 96, radial=grip), body)
    relief = mesh_obj(name + "_r", lathe([(9.6, 50.5), (9.0, 54.0), (7.6, 62.0), (6.2, 68.0), (5.6, 70.0),
                                          (0.0, 70.0)], 64), dark)
    ferrule.parent = boot; relief.parent = boot
    return boot

PLUG_COLORS = {
    "black": (0.015, 0.015, 0.016), "red": (0.50, 0.02, 0.015), "blue": (0.02, 0.10, 0.45),
    "yellow": (0.70, 0.45, 0.02), "green": (0.03, 0.30, 0.06), "white": (0.70, 0.70, 0.68),
}

def mode_plugs():
    for cname, col in PLUG_COLORS.items():
        reset_scene(); materials(); add_lights(); panel_object()
        j = jack_object("Jack"); cu, cv = PW / 2, PH / 2; place(j, cu, cv)
        for o in [j] + list(j.children): o.is_holdout = True
        p = plug_object(col); place(p, cu, cv)
        size_u = 84.0; lift = 18.0
        res = sprite_setup(cu, cv - lift / math.cos(THETA), size_u)
        render_to(os.path.join(OUT, "plugs", f"{cname}.png"))
        json.dump(dict(size=res, anchorX=res / 2, anchorY=res / 2 + lift * PX_PER_UNIT,
                       topZ=68.0, sinTheta=math.sin(THETA)),
                  open(os.path.join(OUT, "plugs.json"), "w"), indent=1)

def mode_leds():
    reset_scene(); materials(); add_lights(); panel_object()
    em = bpy.data.materials.new("LedOn"); em.use_nodes = True
    b = em.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (1.0, 0.08, 0.05, 1)
    b.inputs["Emission Color"].default_value = (1.0, 0.06, 0.03, 1)
    b.inputs["Emission Strength"].default_value = 25.0
    o = led_object("Led", em); cu, cv = PW / 2, PH / 2; place(o, cu, cv)
    res = sprite_setup(cu, cv, 30.0)
    bpy.context.scene.view_layers[0].use_pass_emit = True
    render_to(os.path.join(OUT, "led_on.png"))


# ───────────────────────── teclado (estilo MS-20 / MS-20 V) ─────────────────────────
KEY_THETA = math.radians(24.0)
KH = 300.0                     # profundidad de la sección de teclado (unidades de panel)
KEY_U0, KEY_U1 = 132.0, 1598.0
KEY_V0, KEY_LEN, BLACK_LEN = 52.0, 232.0, 150.0

def textured_box(name, u0, v0, u1, v1, z0, z1, img_path):
    """Caja cuya cara superior lleva una textura (el logo de la franja)."""
    o = box(name, u0, v0, u1, v1, z0, z1, MAT["case"])
    m = bpy.data.materials.new(name + "Mat"); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(img_path); tex.extension = "CLIP"
    co = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
    # Coordenadas generadas (0..1 sobre la caja) → x a lo ancho, y a lo profundo.
    nt.links.new(co.outputs["Generated"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], tex.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"
    mix.inputs["A"].default_value = (0.016, 0.016, 0.017, 1)
    nt.links.new(tex.outputs["Alpha"], mix.inputs["Factor"]); nt.links.new(tex.outputs["Color"], mix.inputs["B"])
    nt.links.new(mix.outputs["Result"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.55
    o.data.materials.clear(); o.data.materials.append(m)
    return o

def key_mesh(name, u0, u1, v0, v1, z0, z1, mat, bevel=0.6):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    cu, cv, cz = (u0 + u1) / 2, (v0 + v1) / 2, (z0 + z1) / 2
    for v in bm.verts:
        v.co.x = (v.co.x * (u1 - u0)) / 10; v.co.y = (-v.co.y * (v1 - v0)) / 10 - (cv - v0) / 10 * 0; v.co.z = v.co.z * (z1 - z0) / 10
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel / 10, segments=3, affect="EDGES")
    o = mesh_obj(name, bm, mat)
    # Origen en el borde trasero superior: la tecla gira alrededor de ese eje al apretarse.
    o.location = P(cu, cv, cz)
    me = o.data
    shift = Vector((0, -(cv - v0) / 10, -(z1 - cz) / 10))   # del centro al pivote (atrás, arriba)
    for vert in me.vertices: vert.co += shift      # local: y ∈ [−largo, 0], z ∈ [−alto, 0]
    o.location = P(cu, v0, z1)
    return o

def build_keyboard(pressed=False):
    white = principled("KeyWhite", (0.84, 0.81, 0.74), 0.3, coat=0.35)
    black = principled("KeyBlack", (0.008, 0.008, 0.009), 0.22, coat=0.6)
    box("KbBase", -CASE_L, -4, PW + CASE_R, KH, -40, 0, MAT["case"])
    textured_box("KbStrip", -CASE_L, -4, PW + CASE_R, KEY_V0 - 4, 0, 30, os.path.join(os.path.dirname(SILK), "strip_logo.png"))
    box("KbLeft", -CASE_L, KEY_V0 - 4, KEY_U0 - 6, KH, 0, 26, MAT["cheek"])
    box("KbRight", KEY_U1 + 4, KEY_V0 - 4, PW + CASE_R, KH, 0, 30, MAT["cheek"])
    box("KbFront", KEY_U0 - 6, KEY_V0 + KEY_LEN + 4, KEY_U1 + 4, KH, -12, 10, MAT["case"])
    keys = []
    whites = [n for n in range(37) if (n % 12) in (0, 2, 4, 5, 7, 9, 11)]
    W = (KEY_U1 - KEY_U0) / len(whites)
    for i, n in enumerate(whites):
        o = key_mesh(f"Key{n}", KEY_U0 + i * W + 1.4, KEY_U0 + (i + 1) * W - 1.4, KEY_V0, KEY_V0 + KEY_LEN, 0, 22, white, 1.2)
        keys.append((n, False, o))
    for n in range(37):
        if (n % 12) not in (1, 3, 6, 8, 10): continue
        left = sum(1 for m in whites if m < n)
        cx = KEY_U0 + left * W
        # Desplazamiento real de las negras dentro de cada grupo (no centradas en la juntura).
        off = {1: -0.12, 3: 0.12, 6: -0.15, 8: 0.0, 10: 0.15}[n % 12] * W
        bw = W * 0.58
        o = key_mesh(f"Key{n}", cx + off - bw / 2, cx + off + bw / 2, KEY_V0, KEY_V0 + BLACK_LEN, 22, 42, black, 2.0)
        keys.append((n, True, o))
    if pressed:
        for n, isb, o in keys:
            o.rotation_euler.x = math.radians(7.5 if isb else 6.0)
    # Rueda de control (gira sobre el eje U) en su ranura, y pulsador momentáneo debajo.
    box("WheelSlot", 12, 70, 68, 228, 20, 27, MAT["hole"])
    wheel = mesh_obj("Wheel", lathe([(0.0, -16.0), (56.0, -16.0), (59.0, -13.0), (59.0, 13.0), (56.0, 16.0), (0.0, 16.0)], 128,
                                    radial=lambda t, r, z: r - (2.2 if (abs(z) < 13.5 and math.cos(t * 56) > 0.2) else 0.0)),
                     principled("WheelRubber", (0.022, 0.022, 0.024), 0.55))
    # El torno deja el eje de la rueda en Z; se acuesta sobre U con Y = 90°. En orden ZYX el giro en Z se aplica
    # primero: la rueda gira sobre su propio eje (en XYZ, girar en X la volcaba de costado).
    wheel.rotation_mode = "ZYX"
    wheel.rotation_euler = Euler((0, math.radians(90), 0), "ZYX")
    wheel.location = P(40, 149, 26 + 18 - 59)
    ring = mesh_obj("MswRing", lathe([(19.0, 26.0), (19.0, 29.0), (16.5, 30.0), (0.0, 30.0)], 64), MAT["nickel"])
    ring.location = P(40, 260, 0)
    cap = mesh_obj("MswCap", lathe([(14.0, 26.0), (14.0, 33.0), (12.0, 35.0), (0.0, 35.5)], 64), principled("Cap", (0.02, 0.02, 0.022), 0.35, coat=0.4))
    cap.location = P(40, 260, 0)
    return keys, wheel, cap

def key_frame():
    w_u = PW + CASE_L + CASE_R
    h_u = KH * math.cos(KEY_THETA) + 50.0 * math.sin(KEY_THETA)
    rx = int(round(w_u * PX_PER_UNIT)); ry = int(round(h_u * PX_PER_UNIT))
    return (-CASE_L + PW + CASE_R) / 2 - CASE_L / 2 + CASE_L / 2, w_u, h_u, rx, ry

def convex_hull(pts):
    pts = sorted(set(pts))
    if len(pts) < 3: return pts
    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, hi = [], []
    for p_ in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p_) <= 0: lo.pop()
        lo.append(p_)
    for p_ in reversed(pts):
        while len(hi) >= 2 and cross(hi[-2], hi[-1], p_) <= 0: hi.pop()
        hi.append(p_)
    return lo[:-1] + hi[:-1]

def mode_keys():
    from bpy_extras.object_utils import world_to_camera_view
    os.makedirs(os.path.join(OUT, "keys"), exist_ok=True)
    meta = {}
    hulls = {}
    for pressed in ((False,) if WHEEL_ONLY else (False, True)):
        reset_scene(); materials(); add_lights()
        keys, wheel, cap = build_keyboard(pressed)
        _, w_u, h_u, rx, ry = key_frame()
        cu = (-CASE_L + PW + CASE_R) / 2
        cv = KH / 2 - 25.0 * math.sin(KEY_THETA)
        add_camera(cu, cv, w_u, h_u, rx, ry, theta=KEY_THETA)
        sc = bpy.context.scene
        bpy.context.view_layer.update()
        cam = sc.camera
        for n, isb, o in keys:
            pts = []
            for v in o.data.vertices:
                c = world_to_camera_view(sc, cam, o.matrix_world @ v.co)
                pts.append((round(c.x * rx, 1), round((1.0 - c.y) * ry, 1)))
            hulls.setdefault(n, {"black": isb, "pts": []})["pts"] += pts
        if not WHEEL_ONLY: render_to(os.path.join(OUT, "keys", "keys_down.png" if pressed else "keys_up.png"))
        if not pressed:
            meta.update(width=rx, height=ry, theta=math.degrees(KEY_THETA))
            # Rueda: filmstrip con todo lo demás como holdout.
            sc.render.film_transparent = True
            for o in bpy.data.objects:
                if o.type == "MESH" and o.name not in ("Wheel",): o.is_holdout = True
            wc = world_to_camera_view(sc, cam, wheel.matrix_world.translation)
            frames = 33
            # Render con recorte alrededor de la rueda (mucho más rápido que el cuadro entero).
            bx0, bx1 = (wc.x * rx - 60) / rx, (wc.x * rx + 60) / rx
            by0, by1 = wc.y - 150.0 / ry, wc.y + 150.0 / ry
            sc.render.use_border = True; sc.render.use_crop_to_border = True
            sc.render.border_min_x, sc.render.border_max_x = max(0, bx0), min(1, bx1)
            sc.render.border_min_y, sc.render.border_max_y = max(0, by0), min(1, by1)
            meta["wheelCrop"] = [round(max(0, bx0) * rx), round((1 - min(1, by1)) * ry), round((min(1, bx1) - max(0, bx0)) * rx), round((min(1, by1) - max(0, by0)) * ry)]
            base_rot = wheel.rotation_euler.copy()
            # Recorte alrededor de la rueda con una cámara auxiliar del mismo encuadre: se renderiza
            # el cuadro completo y se recorta después (más simple y exacto).
            meta["wheel"] = dict(cx=round(wc.x * rx, 1), cy=round((1.0 - wc.y) * ry, 1), frames=frames, range=40.0)
            for f in range(frames):
                # ±40°: 2,5° por cuadro, menos de media estría (56 estrías = 6,4°), así el giro no se ve al revés.
                a = -40.0 + 80.0 * f / (frames - 1)
                wheel.rotation_euler = Euler((0, math.radians(90), math.radians(a)), "ZYX")
                render_to(os.path.join(OUT, "keys", f"wheel_{f:03d}.png"))
            wheel.rotation_euler = base_rot
            if WHEEL_ONLY: return
            # Pulsador apretado.
            for o in bpy.data.objects:
                if o.type == "MESH": o.is_holdout = o.name not in ("MswCap",)
            cap.location.z -= 0.45
            cc = world_to_camera_view(sc, cam, cap.matrix_world.translation)
            sx0, sx1 = (cc.x * rx - 50) / rx, (cc.x * rx + 50) / rx
            sy0, sy1 = cc.y - 50.0 / ry, cc.y + 50.0 / ry
            sc.render.border_min_x, sc.render.border_max_x = sx0, sx1
            sc.render.border_min_y, sc.render.border_max_y = sy0, sy1
            meta["switchCrop"] = [round(sx0 * rx), round((1 - sy1) * ry), round((sx1 - sx0) * rx), round((sy1 - sy0) * ry)]
            render_to(os.path.join(OUT, "keys", "switch_down.png"))
            sc.render.use_border = False
            meta["switch"] = dict(cx=round(cc.x * rx, 1), cy=round((1.0 - cc.y) * ry, 1), r=round(20 * PX_PER_UNIT, 1))
    meta["keys"] = [dict(note=n, black=h["black"], hull=convex_hull(h["pts"])) for n, h in sorted(hulls.items())]
    json.dump(meta, open(os.path.join(OUT, "keys.json"), "w"), indent=1)

# ───────────────────────── hero: animación promocional ─────────────────────────
def preset_angles(path):
    pr = json.load(open(path))
    ang = {}
    for k in LAYOUT["knobs"]:
        v = pr["knobs"].get(k["id"], 0.0)
        if k["steps"] > 0:
            ang[k["id"]] = k["startDeg"] + (k["endDeg"] - k["startDeg"]) * v / (k["steps"] - 1)
        else:
            ang[k["id"]] = k["startDeg"] + (k["endDeg"] - k["startDeg"]) * v
    return pr, ang

def cable_curve(name, a, b, color, lift_a=0.0, lift_b=0.0):
    """Cable 3D entre dos jacks (coordenadas de panel): sale de la ficha, cae hacia abajo del panel
    y vuelve a subir. Curva Bézier con bisel = sección redonda."""
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"
    cu.bevel_depth = 0.34; cu.bevel_resolution = 6; cu.resolution_u = 24
    sp = cu.splines.new("BEZIER"); sp.bezier_points.add(2)
    pa, pb = P(*a, 70.0 + lift_a), P(*b, 70.0 + lift_b)
    span = (pa - pb).length
    mid = (pa + pb) / 2 + Vector((0, -0.42 * span - 2.0, 0)); mid.z = 1.6
    for bp, co in zip(sp.bezier_points, (pa, mid, pb)):
        bp.co = co; bp.handle_left_type = bp.handle_right_type = "AUTO"
    # Que salga vertical de la ficha.
    sp.bezier_points[0].handle_right_type = "FREE"; sp.bezier_points[0].handle_right = pa + Vector((0, -1.0, 5.0))
    sp.bezier_points[0].handle_left_type = "FREE"; sp.bezier_points[0].handle_left = pa + Vector((0, 1.0, -5.0))
    sp.bezier_points[2].handle_left_type = "FREE"; sp.bezier_points[2].handle_left = pb + Vector((0, -1.0, 5.0))
    sp.bezier_points[2].handle_right_type = "FREE"; sp.bezier_points[2].handle_right = pb + Vector((0, 1.0, -5.0))
    o = bpy.data.objects.new(name, cu); link(o)
    o.data.materials.append(principled("Cable" + name, color, 0.42, coat=0.25))
    return o

def keyboard_objects(y0):
    """Teclado de 37 teclas (C–C) delante del panel, en el plano de la mesa (z = 0)."""
    white = principled("KeyWhite", (0.78, 0.76, 0.70), 0.3, coat=0.4)
    black = principled("KeyBlack", (0.01, 0.01, 0.011), 0.25, coat=0.5)
    W, L = 6.55, 41.0                     # tecla blanca: ancho y largo (BU)
    whites = [n for n in range(37) if (n % 12) in (0, 2, 4, 5, 7, 9, 11)]
    x0 = (PW / 10 - len(whites) * W) / 2 + 2.0
    objs = []
    for i, n in enumerate(whites):
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x = x0 + i * W + W / 2 + v.co.x * (W - 0.18); v.co.y = y0 - L / 2 + v.co.y * L; v.co.z = 1.1 + v.co.z * 2.2
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.12, segments=3, affect="EDGES")
        objs.append(mesh_obj(f"KeyW{n}", bm, white))
    for n in range(37):
        if (n % 12) not in (1, 3, 6, 8, 10): continue
        left_white = sum(1 for m in whites if m < n)
        cx = x0 + left_white * W
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x = cx + v.co.x * W * 0.56; v.co.y = y0 - 12.5 + v.co.y * 25.0; v.co.z = 2.9 + v.co.z * 1.7
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.15, segments=3, affect="EDGES")
        objs.append(mesh_obj(f"KeyB{n}", bm, black))
    # Cama del teclado y mejillas.
    bed = box("KeyBed", -CASE_L, -y0 * 10 + 6, PW + CASE_R, -y0 * 10 + L * 10 + 30, -30, 0, MAT["case"])
    return objs

def mode_hero():
    preset = arg("--preset", "preset.json")
    pr, ang = preset_angles(preset)
    reset_scene(); materials()
    sc = bpy.context.scene
    sc.render.film_transparent = False
    sc.cycles.samples = SAMPLES
    panel_object(); case_objects()
    proto = {"small": knob_object("small", "KnobSmall"), "big": knob_object("big", "KnobBig")}
    for k in LAYOUT["knobs"]:
        o = proto[k["kind"]].copy(); link(o)
        ind = proto[k["kind"]].children[0].copy(); link(ind); ind.parent = o
        place(o, *k["pos"], ang[k["id"]])
    for pz in proto.values():
        pz.hide_render = True
        for ch in pz.children: ch.hide_render = True
    jp = jack_object("JackProto")
    for j in LAYOUT["jacks"]:
        o = jp.copy(); link(o)
        for ch in jp.children:
            c2 = ch.copy(); link(c2); c2.parent = o
        place(o, *j["pos"])
    for ch in jp.children: ch.hide_render = True
    jp.hide_render = True
    for i, (u, v) in enumerate(LAYOUT["screws"]):
        place(screw_object(f"Screw{i}"), u, v, 20 * i)
    for i, (u, v) in enumerate(LAYOUT["holes"]):
        place(trim_hole_object(f"Trim{i}"), u, v)
    for l in LAYOUT["leds"]:
        place(led_object("Led_" + l["id"]), *l["pos"])
    jpos = {j["id"]: j["pos"] for j in LAYOUT["jacks"]}
    stack = {}
    for i, c in enumerate(pr.get("cables", [])):
        col = PLUG_COLORS.get(c.get("color", "red"), PLUG_COLORS["red"])
        lift = {}
        for end in ("src", "dst"):
            # Fichas apiladas: la segunda se enchufa en la parte de atrás de la primera.
            n = stack.get(c[end], 0); stack[c[end]] = n + 1
            lift[end] = n * 58.0
            o = plug_object(col, f"Plug{i}{end}"); place(o, *jpos[c[end]]); o.location.z += lift[end] / 10
        cable_curve(f"Cable{i}", jpos[c["src"]], jpos[c["dst"]], col, lift["src"], lift["dst"])

    # Inclinar todo el panel (como el MS-20 real) alrededor de su borde inferior.
    pivot = bpy.data.objects.new("Pivot", None); link(pivot)
    pivot.location = Vector((0, -(PH + CASE_B) / 10, 0))
    bpy.context.view_layer.update()          # sin esto matrix_world del pivote sigue en identidad
    for o in list(bpy.data.objects):
        if o is pivot or o.parent is not None or o.type in ("CAMERA", "LIGHT"): continue
        o.parent = pivot
        o.matrix_parent_inverse = pivot.matrix_world.inverted()
    pivot.rotation_euler.x = math.radians(52)
    bpy.context.view_layer.update()
    pnl = bpy.data.objects["Panel"]
    print("panel bbox:", [tuple(round(c, 1) for c in (pnl.matrix_world @ Vector(v))) for v in pnl.bound_box][:4], flush=True)
    keyboard_objects(-(PH + CASE_B) / 10 - 1.0)
    # Mesa oscura con reflejo suave.
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=400.0)
    for v in bm.verts: v.co.z = -3.05
    mesh_obj("Floor", bm, principled("Floor", (0.012, 0.012, 0.014), 0.28), smooth=False)

    # Luces de estudio: clave suave arriba a la izquierda, contra cálida, relleno frío.
    def area(name, loc, rot, size, energy, color):
        L = bpy.data.lights.new(name, "AREA"); L.size = size; L.energy = energy; L.color = color
        o = bpy.data.objects.new(name, L); link(o); o.location = loc; o.rotation_euler = Euler(rot, "XYZ")
    area("Key", (-40, -120, 140), (math.radians(40), math.radians(-12), 0), 90, 9e6, (1.0, 0.96, 0.9))
    area("Rim", (120, 60, 90), (math.radians(-35), math.radians(55), 0), 60, 3e6, (1.0, 0.72, 0.45))
    area("Fill", (200, -160, 40), (math.radians(75), 0, math.radians(50)), 120, 1.5e6, (0.6, 0.75, 1.0))
    sc.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.015
    sc.view_settings.exposure = -2.6

    cam = bpy.data.cameras.new("HeroCam"); cam.lens = 38; cam.dof.use_dof = True
    cam.dof.aperture_fstop = 4.0
    co = bpy.data.objects.new("HeroCam", cam); link(co); sc.camera = co
    focus = bpy.data.objects.new("Focus", None); link(focus)
    cam.dof.focus_object = focus
    track = co.constraints.new("TRACK_TO"); track.target = focus
    track.track_axis = "TRACK_NEGATIVE_Z"; track.up_axis = "UP_Y"

    W = PW / 10
    still = MODE_STILL[0]
    sc.render.resolution_x, sc.render.resolution_y = (3840, 2160) if still else (1920, 1080)
    keys = [  # (frame, cámara, foco)
        (1,   (W * 0.10, -128, 112), (W * 0.22, -24, 26)),
        (72,  (W * 0.48, -182, 150), (W * 0.50, -26, 22)),
        (144, (W * 0.92, -122, 104), (W * 0.80, -22, 27)),
    ]
    # Tomas del film de la web (--shot): primer plano de la patchera y paneo bajo sobre el teclado.
    SHOTS = {
        "patch": ([(1, (W * 0.66, -70, 64), (W * 0.76, -18, 27)), (FILM_FRAMES, (W * 0.98, -64, 58), (W * 0.90, -18, 27))], 55, 2.8),
        "keys": ([(1, (W * 0.02, -112, 30), (W * 0.18, -34, 4)), (FILM_FRAMES, (W * 0.70, -112, 30), (W * 0.82, -34, 4))], 42, 4.0),
        "wide": ([(1, (W * 0.10, -128, 112), (W * 0.22, -24, 26)), (FILM_FRAMES, (W * 0.92, -122, 104), (W * 0.80, -22, 27))], 38, 4.0),
    }
    shot = arg("--shot")
    if shot:
        keys, cam.lens, cam.dof.aperture_fstop = SHOTS[shot]
        sc.render.resolution_x, sc.render.resolution_y = int(arg("--w", "1920")), int(arg("--h", "1080"))
    if still:
        # Encuadre por argumentos (--camloc/--camfocus en BU, --lens, --name) o el plano general.
        def vec(name, default):
            v = arg(name); return Vector(tuple(float(c) for c in v.split(","))) if v else Vector(default)
        co.location = vec("--camloc", (W * 0.44, -178, 152)); focus.location = vec("--camfocus", (W * 0.5, -27, 21))
        cam.lens = float(arg("--lens", str(cam.lens)))
        if arg("--fstop"): cam.dof.aperture_fstop = float(arg("--fstop"))
        sc.frame_set(1)
        render_to(os.path.join(OUT, arg("--name", "hero_4k") + ".png"))
        return
    sc.frame_start, sc.frame_end = 1, (FILM_FRAMES if shot else 144)
    if arg("--probe"): sc.frame_start = sc.frame_end = int(arg("--probe"))
    sc.render.fps = 24
    for f, c, t in keys:
        co.location = Vector(c); co.keyframe_insert("location", frame=f)
        focus.location = Vector(t); focus.keyframe_insert("location", frame=f)
    for obj in (co, focus):
        if obj.animation_data and obj.animation_data.action:
            for fc in getattr(obj.animation_data.action, "fcurves", []):
                for kp in fc.keyframe_points: kp.interpolation = "BEZIER"
    sc.render.filepath = os.path.join(OUT, ("film_" + shot) if shot else "hero", "f_")
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGB"
    sc.render.use_persistent_data = True     # la escena se sincroniza una vez, no en cada cuadro
    bpy.ops.render.render(animation=True)

MODE_STILL = [False]
FILM_FRAMES = int(arg("--frames", "120"))
def mode_hero_still():
    MODE_STILL[0] = True
    mode_hero()

# ───────────────────────── film de la web (filmkit, --mode film2) ─────────────────────────
FS = 1 / 200.0               # 1 BU ≈ 5 mm, pero Cycles calcula el desenfoque como si fuera 1 m: f-stop real × FS
TILT = math.radians(52)
PIV = Vector((0, -(PH + CASE_B) / 10, 0))
FILM_SHOTS = [("open", 0, 8), ("keys", 8, 6), ("filter", 14, 8), ("plug", 22, 8), ("wheel", 30, 4), ("hero", 34, 6),
              ("end", 40, 8)]                    # (toma, tiempo de compás en que empieza, tiempos que dura)
FILM_CABLES = [("white_out", "sh_in", "red"), ("mg_sq_out", "sh_clock_in", "green"), ("sh_out", "hpf_cut_in", "blue")]
NEW_CABLE = ("mg_tri_out", "lpf_cut_in", "yellow")   # el que se enchufa en la toma "plug"

def pv(u, v, h=0.0):
    """Punto del mundo sobre el panel inclinado: (u, v) de panel y h en BU a lo largo de su normal."""
    return PIV + Matrix.Rotation(TILT, 3, "X") @ (P(u, v) + Vector((0, 0, h)) - PIV)

def plug_shaft(plug, name):
    """Vástago de 6,35 mm bajo la virola: manga niquelada, anillo aislante y punta con cuello. Enchufada no se ve (queda
    dentro del jack); en la toma del enchufe se ve entrar."""
    sleeve = mesh_obj(name + "_s", lathe([(6.3, -30.0), (6.3, 10.6), (0.0, 10.6)], 48), MAT["nickel"])
    ins = mesh_obj(name + "_i", lathe([(6.15, -34.0), (6.15, -30.0)], 48), principled("Insul", (0.01, 0.01, 0.011), 0.35))
    tip = mesh_obj(name + "_t", lathe([(0.0, -56.0), (3.5, -55.4), (5.6, -53.6), (6.3, -51.0), (6.3, -45.0), (4.4, -43.0),
                                       (4.4, -38.0), (5.6, -36.5), (5.6, -34.0), (0.0, -34.0)], 48), MAT["nickel"])
    for o in (sleeve, ins, tip): o.parent = plug
    return [sleeve, ins, tip]

def cable_keys(c, new=False):
    """Claves de forma del cable: "sway" mueve el punto medio de costado (el cable se mece); el cable nuevo tiene además
    lx/ly/lz, que corren su extremo 1 BU por unidad (sigue a la ficha mientras entra)."""
    c.shape_key_add(name="Basis", from_mix=False)
    sw = c.shape_key_add(name="sway", from_mix=False); sw.slider_min, sw.slider_max = -2, 2
    d = Vector((0.8, -0.45, 0.0))
    pt = sw.data[1]; pt.co += d; pt.handle_left += d; pt.handle_right += d
    if new:
        for ax, d in (("lx", Vector((1, 0, 0))), ("ly", Vector((0, 1, 0))), ("lz", Vector((0, 0, 1)))):
            k = c.shape_key_add(name=ax, from_mix=False); k.slider_min, k.slider_max = -40, 40
            pt = k.data[2]; pt.co += d; pt.handle_left += d; pt.handle_right += d

def film_keyboard(y0):
    """Teclado del film: las 37 teclas (algo más angostas que en las fotos) giran sobre su borde trasero, y a la
    izquierda la rueda de control en su ranura con el pulsador."""
    white = principled("KeyWhite", (0.66, 0.64, 0.58), 0.3, coat=0.4)       # algo más gris que en las fotos: en macro se quemaba
    black = principled("KeyBlack", (0.01, 0.01, 0.011), 0.25, coat=0.5)
    W, L = 6.3, 41.0
    whites = [n for n in range(37) if (n % 12) in (0, 2, 4, 5, 7, 9, 11)]
    x0 = (PW + CASE_R) / 10 - 2.0 - len(whites) * W
    keys = {}
    for i, n in enumerate(whites):
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x *= W - 0.18; v.co.y = -L / 2 + v.co.y * L; v.co.z = -1.1 + v.co.z * 2.2
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.12, segments=3, affect="EDGES")
        o = mesh_obj(f"KeyW{n}", bm, white); o.location = (x0 + i * W + W / 2, y0, 2.2); keys[n] = (o, False)
    for n in range(37):
        if (n % 12) not in (1, 3, 6, 8, 10): continue
        cx = x0 + sum(1 for m in whites if m < n) * W + {1: -0.12, 3: 0.12, 6: -0.15, 8: 0.0, 10: 0.15}[n % 12] * W
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x *= W * 0.56; v.co.y = -12.5 + v.co.y * 25.0; v.co.z = -1.7 + v.co.z * 3.4
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.15, segments=3, affect="EDGES")
        o = mesh_obj(f"KeyB{n}", bm, black); o.location = (cx, y0, 4.6); keys[n] = (o, True)
    vb0, vb1 = -y0 * 10 + 6, -y0 * 10 + L * 10 + 30
    box("KeyBed", -CASE_L, vb0, PW + CASE_R, vb1, -30, 0, MAT["case"])
    box("KbCheekR", (x0 + len(whites) * W) * 10 + 4, vb0, PW + CASE_R, vb1, 0, 52, MAT["cheek"])
    # Cubierta de la rueda: pintura en polvo oscura con grano (la de las mejillas, en macro, se veía gris y plana).
    paint = principled("DeckPaint", (0.010, 0.010, 0.011), 0.6, spec=0.18)
    nt = paint.node_tree; bs = nt.nodes["Principled BSDF"]
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 700.0; nz.inputs["Detail"].default_value = 6.0
    tc = nt.nodes.new("ShaderNodeTexCoord"); nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    rr = nt.nodes.new("ShaderNodeMapRange"); rr.inputs["To Min"].default_value = 0.55; rr.inputs["To Max"].default_value = 0.75
    nt.links.new(nz.outputs["Fac"], rr.inputs["Value"]); nt.links.new(rr.outputs["Result"], bs.inputs["Roughness"])
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.05; bp.inputs["Distance"].default_value = 0.002
    nt.links.new(nz.outputs["Fac"], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], bs.inputs["Normal"])
    box("WheelDeck", -CASE_L, vb0, x0 * 10 - 4, vb1, 0, 18, paint)
    wy = y0 - 16.0
    box("WheelSlot", 52, -wy * 10 - 72, 88, -wy * 10 + 72, 17.6, 18.3, principled("SlotBlack", (0.003, 0.003, 0.003), 0.9, spec=0.1))
    # Estrías redondeadas con 8 segmentos cada una (con 128 se veían escalones en macro).
    wheel = mesh_obj("FilmWheel", lathe([(0.0, -16.0), (56.0, -16.0), (59.0, -13.0), (59.0, -12.0), (59.0, 12.0), (59.0, 13.0), (56.0, 16.0), (0.0, 16.0)], 448,
                                        radial=lambda t, r, z: r - (1.6 * max(0.0, math.cos(t * 56)) ** 2 if abs(z) < 12.5 else 0.0)),
                     principled("FilmWheelRubber", (0.008, 0.008, 0.009), 0.4, spec=0.35))
    wheel.rotation_mode = "ZYX"; wheel.rotation_euler = Euler((0, math.radians(90), 0), "ZYX")
    wheel.location = Vector((7.0, wy, 1.8 + 1.7 - 5.9))
    ring = mesh_obj("FilmMswRing", lathe([(19.0, 0.0), (19.0, 3.0), (16.5, 4.0), (0.0, 4.0)], 64), MAT["nickel"])
    cap = mesh_obj("FilmMswCap", lathe([(14.0, 0.0), (14.0, 7.0), (12.0, 9.0), (0.0, 9.5)], 64), principled("Cap", (0.02, 0.02, 0.022), 0.35, coat=0.4))
    for o in (ring, cap): o.location = Vector((17.0, wy, 1.8))
    return keys, wheel

def film_scene(pr, ang, with_new=True):
    """El instrumento como en las fotos (mode_hero), con lo que el film anima a mano: perillas por id, LEDs con su
    material, cables con claves de forma, la ficha que se enchufa (con su vástago) y el teclado con la rueda."""
    reset_scene(); materials()
    sc = bpy.context.scene
    panel_object(); case_objects()
    proto = {"small": knob_object("small", "KnobSmall"), "big": knob_object("big", "KnobBig")}
    knobs = {}
    for k in LAYOUT["knobs"]:
        o = proto[k["kind"]].copy(); link(o)
        ind = proto[k["kind"]].children[0].copy(); link(ind); ind.parent = o
        place(o, *k["pos"], ang[k["id"]]); knobs[k["id"]] = o
    for pz in proto.values():
        pz.hide_render = True
        for ch in pz.children: ch.hide_render = True
    jp = jack_object("JackProto")
    for j in LAYOUT["jacks"]:
        o = jp.copy(); link(o)
        for ch in jp.children:
            c2 = ch.copy(); link(c2); c2.parent = o
        place(o, *j["pos"])
    for ch in jp.children: ch.hide_render = True
    jp.hide_render = True
    for i, (u, v) in enumerate(LAYOUT["screws"]):
        place(screw_object(f"Screw{i}"), u, v, 20 * i)
    for i, (u, v) in enumerate(LAYOUT["holes"]):
        place(trim_hole_object(f"Trim{i}"), u, v)
    leds = {}
    for l in LAYOUT["leds"]:
        m = principled("Led_" + l["id"], (0.18, 0.02, 0.02), 0.15, coat=0.6)
        b = m.node_tree.nodes["Principled BSDF"]
        b.inputs["Emission Color"].default_value = (1.0, 0.07, 0.03, 1); b.inputs["Emission Strength"].default_value = 0.0
        o = led_object("Led_" + l["id"], m); place(o, *l["pos"]); leds[l["id"]] = b
    jpos = {j["id"]: j["pos"] for j in LAYOUT["jacks"]}
    cables = []
    for i, (a, b_, cn) in enumerate(FILM_CABLES + ([NEW_CABLE] if with_new else [])):
        col = PLUG_COLORS[cn]; plugs = []
        for end in (a, b_):
            o = plug_object(col, f"Plug{i}{end}"); place(o, *jpos[end]); plugs.append(o)
        c = cable_curve(f"Cable{i}", jpos[a], jpos[b_], col)
        cable_keys(c, new=(cn == NEW_CABLE[2] and i == len(FILM_CABLES)))
        cables.append((c, plugs))
    newplug = None
    if with_new:
        newplug = cables[-1][1][1]; plug_shaft(newplug, "NewShaft")
    pivot = bpy.data.objects.new("Pivot", None); link(pivot)
    pivot.location = PIV.copy()
    bpy.context.view_layer.update()
    for o in list(bpy.data.objects):
        if o is pivot or o.parent is not None or o.type in ("CAMERA", "LIGHT"): continue
        o.parent = pivot; o.matrix_parent_inverse = pivot.matrix_world.inverted()
    pivot.rotation_euler.x = TILT
    bpy.context.view_layer.update()
    keys, wheel = film_keyboard(PIV.y - 1.0)
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=400.0)
    for v in bm.verts: v.co.z = -3.05
    mesh_obj("Floor", bm, principled("Floor", (0.012, 0.012, 0.014), 0.28), smooth=False)
    def area(name, loc, rot, size, energy, color):
        L_ = bpy.data.lights.new(name, "AREA"); L_.size = size; L_.energy = energy; L_.color = color
        o = bpy.data.objects.new(name, L_); link(o); o.location = loc; o.rotation_euler = Euler(rot, "XYZ"); return o
    area("Key", (-40, -120, 140), (math.radians(40), math.radians(-12), 0), 90, 9e6, (1.0, 0.96, 0.9))
    area("Rim", (120, 60, 90), (math.radians(-35), math.radians(55), 0), 60, 3e6, (1.0, 0.72, 0.45))
    area("Fill", (200, -160, 40), (math.radians(75), 0, math.radians(50)), 120, 1.5e6, (0.6, 0.75, 1.0))
    sweep = area("Sweep", (0, -80, 60), (0, 0, 0), 14, 0.0, (1.0, 0.86, 0.68))
    sc.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.015
    sc.view_settings.exposure = -2.6
    return dict(sc=sc, knobs=knobs, leds=leds, cables=cables, newplug=newplug, keys=keys, wheel=wheel, sweep=sweep)

def aim(o, target):
    o.rotation_euler = (Vector(target) - o.location).to_track_quat("-Z", "Y").to_euler()

def film2():
    """Film de la web del AT20V: 48 tiempos a 124 BPM (23,2 s), siete tomas cortadas en el tiempo. El audio es
    film_audio.py: una línea acid en La menor cuyo filtro se abre hasta gritar; las perillas de corte y pico giran con
    lo que suena, las teclas bajan en cada nota, un cable entra en el CV del pasa bajos y el filtro empieza a oscilar
    a tempo, la rueda dobla una nota larga.

    Storyboard (tiempo de compás de inicio · tiempos · segundos):
      open   0 · 8 · 3,9 s  Rasante desde el extremo izquierdo: las perillas de los VCO desenfocadas adelante (abajo
                            a la izquierda, oscuro y calmo: ahí va el título) y el panel que se aleja hacia la patchera.
                            Travelling lento hacia la derecha; un barrido de luz cálida cruza el panel. LED del EG en
                            cada nota.
      keys   8 · 6 · 2,9 s  Macro bajo a ras del teclado: las teclas bajan con la línea acid; foco que pasa de las
                            teclas a las perillas del filtro detrás. Cámara en mano muy sutil.
      filter 14 · 8 · 3,9 s Macro de las perillas del pasa bajos: el corte y el pico giran mientras el filtro se abre
                            (lo que suena); foco del corte al pico. Desplazamiento lateral lento.
      plug   22 · 8 · 3,9 s Macro rasante de la fila superior de la patchera: la ficha amarilla baja desde arriba, se
                            alinea y entra en CUTOFF FREQ (llega al fondo en el tiempo 26); desde ahí el filtro oscila
                            a tempo y el cable se mece.
      wheel  30 · 4 · 1,9 s Macro a ras de la rueda, contra el teclado y el panel: la nota larga se dobla una cuarta y
                            vuelve, la rueda gira con ella y un brillo chico recorre las estrías.
      hero   34 · 6 · 2,9 s Grúa amplia sobre el instrumento entero, el filtro gritando: todo se mueve a la vez.
      end    40 · 8 · 3,9 s Fundido; alejamiento frontal lento y alto mientras el filtro se cierra (film.py lo oscurece
                            y pone el cierre encima).
    """
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "shared", "film"))
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import filmkit as fk
    import film_audio as fa
    only = arg("--shot"); quality = arg("--quality", "final"); probe = arg("--probe")
    # Las notas, el filtro y la rueda salen de la partitura de film_audio.py (la misma que suena): más exacto que
    # leer la envolvente del WAV. --audio se acepta por compatibilidad con los demás films, no hace falta.
    B = fa.BEAT
    pr, ang = preset_angles(os.path.join(_ASSETS, "presets", "15_acid_bass.json"))
    kn = {k["id"]: k for k in LAYOUT["knobs"]}
    deg = lambda kid, v: kn[kid]["startDeg"] + (kn[kid]["endDeg"] - kn[kid]["startDeg"]) * v
    by_key = {}
    for (s, d, n, v) in fa.NOTES: by_key.setdefault(n - 24, []).append((s, s + d))
    starts = [s for (s, d, n, v) in fa.NOTES]

    def press(n, b):
        p = 0.0
        for (s, e) in by_key.get(n, ()):
            if s <= b <= e + 0.2:
                p = max(p, min(1.0, (b - s) / 0.06) if b <= e else min(1.0, (e - s) / 0.06) * max(0.0, 1 - (b - e) / 0.14))
        return fk.ease(p, "smooth")

    def eg_led(b):
        past = [s for s in starts if s <= b]
        return math.exp(-(b - past[-1]) / 0.35) if past else 0.0

    idx = {n: i for i, (n, _, _) in enumerate(FILM_SHOTS)}
    for (name, b0, nb) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        S = film_scene(pr, ang, with_new=idx[name] >= idx["plug"])
        sc = S["sc"]; fk.setup(sc, quality); sc.view_settings.exposure = -2.6; sc.render.image_settings.color_mode = "RGB"
        cam = fk.camera(sc)
        if name == "wheel": S["sweep"].data.size = 2.5
        frames = int(round(nb * B * fk.FPS))
        W_ = PW / 10
        k = lambda u, pos, look, lens, fs: (u, Vector(pos), Vector(look), lens, fs * FS)
        paths = {
            "open":   fk.Path([k(0, pv(-160, 300, 25), pv(700, 220, 0), 35, 5.6), k(1, pv(-70, 300, 21), pv(800, 210, 0), 36, 5.6)]),
            "keys":   fk.Path([k(0, (40, -120, 14), (95, -80, 2), 40, 5.6), k(1, (54, -118, 13), (108, -78, 2.5), 40, 5.6)]),
            "filter": fk.Path([k(0, pv(520, 210, 55), pv(538, 160, 0), 50, 2.8), k(1, pv(545, 200, 46), pv(548, 160, 0), 50, 2.8)]),
            "plug":   fk.Path([k(0, pv(1460, 190, 14), pv(1263, 112, 5), 45, 4.0), k(1, pv(1440, 175, 12), pv(1263, 110, 3.5), 45, 4.0)]),
            # Rueda: macro bajo, casi a ras de la cubierta (poca cubierta en cuadro, la rueda contra el teclado y el panel);
            # mira 2 BU a la derecha de la rueda, así queda en el tercio izquierdo-central durante todo el leve arco.
            "wheel":  fk.Path([k(0, (-4, -88, 4.6), (10.5, -70.5, 2.6), 55, 40.0), k(1, (-1, -89.5, 4.4), (11.0, -70.5, 2.6), 55, 40.0)]),
            "hero":   fk.Path([k(0, (35, -195, 92), (80, -38, 18), 35, 8.0), k(1, (125, -192, 112), (84, -38, 18), 35, 8.0)]),
            "end":    fk.Path([k(0, (W_ * 0.5, -165, 118), (W_ * 0.5, -38, 18), 38, 8.0),
                               k(1, (W_ * 0.5, -215, 158), (W_ * 0.5, -38, 18), 38, 8.0)], easing="out"),
        }
        path = paths[name]
        cut, peak = S["knobs"]["lpf_cutoff"], S["knobs"]["lpf_peak"]
        newplug = S["newplug"]
        cables = S["cables"]

        def plug_state(b):
            """(dx, dy, h, inclinación) de la ficha nueva respecto del jack, en BU."""
            if b >= 26.0: return 0.0, 0.0, 0.0, 0.0
            a = fk.ease((b - 22.0) / 2.8, "out")
            h = 24.0 + (7.4 - 24.0) * a
            dx, dy, tl = 1.0 * (1 - a), -2.5 * (1 - a), math.radians(10) * (1 - a)
            if b > 24.8: h = 7.4 - 0.4 * fk.ease((b - 24.8) / 0.4, "smooth")
            if b > 25.2: h = 7.0 - 6.6 * fk.ease((b - 25.2) / 0.65, "inout")
            if b > 25.85: h = 0.4 * (1 - fk.ease((b - 25.85) / 0.15, "in"))
            return dx, dy, h, tl

        def pose(f, t, u, name=name, path=path, b0=b0):
            b = b0 + t / B
            x = fa.x_at(b); cv, pk = fa.knob_values(x)
            cut.rotation_euler.z = -math.radians(deg("lpf_cutoff", cv))
            peak.rotation_euler.z = -math.radians(deg("lpf_peak", pk))
            moved = [cut, peak]
            for n, (o, isb) in S["keys"].items():
                o.rotation_euler.x = math.radians(3.6 if isb else 3.2) * press(n, b); moved.append(o)
            S["wheel"].rotation_euler = Euler((0, math.radians(90), math.radians(-34) * fa.bend_at(b)), "ZYX"); moved.append(S["wheel"])
            if newplug is not None:
                dx, dy, h, tl = plug_state(b)
                jx, jy = [j["pos"] for j in LAYOUT["jacks"] if j["id"] == NEW_CABLE[1]][0]
                newplug.location = P(jx, jy) + Vector((dx, dy, h)); newplug.rotation_euler.x = tl; moved.append(newplug)
            sw = S["sweep"]
            if name == "open":
                sw.location = Vector((-40 + 260 * fk.ease(u, "inout"), -95, 62)); aim(sw, (sw.location.x + 10, -30, 20))
            elif name == "hero":
                sw.location = Vector((170 - 200 * u, -105, 75)); aim(sw, (sw.location.x - 10, -30, 20))
            elif name == "wheel":                # brillo chico que recorre las estrías de adelante hacia atrás
                sw.location = Vector((3.0, -84 + 22 * fk.ease(u, "inout"), 8.5)); aim(sw, (7.0, -72, 2.0))
            moved.append(sw)
            focus = None
            if name == "keys": focus = Vector((84, -92, 2.5)).lerp(pv(538.6, 224.0, 4), fk.rack(u, 0.55, 0.9, 0, 1))
            if name == "filter": focus = pv(537.7, 97.1, 5.5).lerp(pv(538.6, 224.0, 4.2), fk.rack(u, 0.35, 0.75, 0, 1))
            if name == "plug": focus = pv(1263.3, 109.9, 4.0)
            if name == "wheel": focus = Vector((7.0, -74.5, 2.9))       # estrías del frente de la rueda
            shake = {"keys": 0.2, "filter": 0.18, "plug": 0.15, "wheel": 0.06}.get(name, 0.0)   # BU: ≈ 1 mm reales
            path.apply(cam, u, focus=focus, shake=shake, t=b0 * B + t)
            return moved

        def keys_(f, name=name, b0=b0, frames=frames):
            b = b0 + (f - 1) / fk.FPS / B
            S["leds"]["eg_led"].inputs["Emission Strength"].default_value = 70.0 * eg_led(b)
            S["leds"]["mg_led"].inputs["Emission Strength"].default_value = 55.0 * fa.lfo_led(b)
            for lid in ("eg_led", "mg_led"): S["leds"][lid].inputs["Emission Strength"].keyframe_insert("default_value")
            sw = S["sweep"].data
            sw.energy = {"open": 1.4e6, "hero": 0.9e6, "wheel": 3.0e4}.get(name, 0.0); sw.keyframe_insert("energy")
            T = b * B
            for i, (c, _) in enumerate(cables):
                kb = c.data.shape_keys.key_blocks
                v = 0.55 * fk.noise1(T, 7 + i, rate=0.28)
                if newplug is not None and i == len(cables) - 1:
                    dx, dy, h, tl = plug_state(b)
                    kb["lx"].value = dx; kb["ly"].value = dy - 7.0 * math.sin(tl); kb["lz"].value = h + 7.0 * (math.cos(tl) - 1)
                    for ax in ("lx", "ly", "lz"): kb[ax].keyframe_insert("value")
                    if b > 26: v += 1.1 * math.exp(-(b - 26) * B / 0.9) * math.sin(2 * math.pi * 1.4 * (b - 26) * B)
                kb["sway"].value = v; kb["sway"].keyframe_insert("value")

        if probe:
            os.makedirs(OUT, exist_ok=True)
            for pu in [float(x) for x in probe.split(",")]:
                f = 1 + int(round(pu * (frames - 1))); sc.frame_set(f)
                pose(f, (f - 1) / fk.FPS, (f - 1) / max(1, frames - 1)); keys_(f)
                sc.render.filepath = os.path.join(OUT, f"probe_{name}_{f:04d}.jpg"); bpy.ops.render.render(write_still=True)
            continue
        if arg("--cams"):                  # pruebas de encuadre: {"name", "shot", "u", "pos", "look", "lens", "fstop", "focus"}
            vec = lambda c: pv(*c[1:]) if isinstance(c[0], str) else Vector(c)      # ["pv", u, v, h] o [x, y, z]
            for c in json.load(open(arg("--cams"))):
                if c["shot"] != name: continue
                f = 1 + int(round(c.get("u", 0.0) * (frames - 1))); sc.frame_set(f)
                pose(f, (f - 1) / fk.FPS, (f - 1) / max(1, frames - 1)); keys_(f)
                pth = fk.Path([(0, vec(c["pos"]), vec(c["look"]), c["lens"], c["fstop"] * FS), (1, vec(c["pos"]), vec(c["look"]), c["lens"], c["fstop"] * FS)])
                pth.apply(cam, 0.0, focus=vec(c["focus"]) if "focus" in c else None)
                sc.render.filepath = os.path.join(OUT, f"cam_{c['name']}.jpg"); bpy.ops.render.render(write_still=True)
            continue
        fk.render_shot(sc, name, frames, pose, OUT, extra_keys=keys_)


if __name__ == "__main__":
    modes = {"bg": mode_bg, "preview": mode_preview, "knobs": mode_knobs, "plugs": mode_plugs, "leds": mode_leds,
             "hero": mode_hero, "herostill": mode_hero_still, "keys": mode_keys, "film2": film2}
    for m in (["bg", "knobs", "plugs", "leds"] if MODE == "all" else MODE.split(",")):
        print("== modo", m, flush=True)
        modes[m]()
