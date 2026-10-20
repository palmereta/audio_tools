"""Escena Blender del AT101V (vista cenital del instrumento completo).

Corre en pc50-wsl:  blender -b -P scene101.py -- --layout layout101.json --silk silkscreen101.png \
                         --out out101 --mode bg,keys,sprites --samples 256 [--colors grey,blue,red]
Film de la web (filmkit, ver film2):  blender -b --factory-startup -P scene101.py -- --mode film2 \
                         --quality draft|final --audio acid-line.wav --out <dir> [--shot open,seq]

1 unidad de Blender = 10 mm. Punto del panel (x, y) en mm, y hacia el frente → (x/10, −y/10, z/10).
Cámara ortográfica inclinada THETA: en pantalla sx = x, sy = y·cosθ − z·sinθ (mm).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Euler, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d

# Sin --layout/--silk se buscan en el directorio actual y, si no están, en plugins/at101v/assets (film2 desde el repo).
HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "..", "..", "assets")
def _default(name, sub): return name if os.path.exists(name) else os.path.join(ASSETS, sub)
L = json.load(open(arg("--layout", _default("layout101.json", "layout101.json"))))
SILK = os.path.abspath(arg("--silk", _default("silkscreen101.png", os.path.join("render", "silkscreen101.png"))))
OUT = os.path.abspath(arg("--out", "out101"))
SAMPLES = int(arg("--samples", "256"))
COLORS = arg("--colors", "grey,blue,red").split(",")
os.makedirs(OUT, exist_ok=True)

THETA = math.radians(8.0)
PXMM = float(arg("--pxmm", "5.2"))  # px por mm (2x de la UI)
MARGIN = 8.0                    # mm de gabinete alrededor del panel
W, H = L["size"]
BODY = {"grey": (0.098, 0.096, 0.090), "blue": (0.018, 0.115, 0.40), "red": (0.52, 0.03, 0.03)}

def P(x, y, z=0.0): return Vector((x / 10.0, -y / 10.0, z / 10.0))

# ───────── escena ─────────
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for t in ("CUDA",):                    # OptiX no inicia en WSL y Cycles cae a CPU sin avisar
        try:
            prefs.compute_device_type = t; prefs.get_devices()
            if any(d.type == t for d in prefs.devices):
                for d in prefs.devices: d.use = d.type == t
                break
        except Exception: pass
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    if hasattr(sc.cycles, "denoising_use_gpu"): sc.cycles.denoising_use_gpu = True   # OIDN en la GPU: en CPU era el cuello de botella
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.6, 0.62, 0.66, 1); bg.inputs["Strength"].default_value = 0.08
    return sc

def lights():
    def sun(name, rot, e, ang, col):
        l = bpy.data.lights.new(name, "SUN"); l.energy = e; l.angle = math.radians(ang); l.color = col
        o = bpy.data.objects.new(name, l); bpy.context.collection.objects.link(o); o.rotation_euler = Euler(rot, "XYZ")
    sun("Key", (math.radians(34), math.radians(-22), math.radians(-18)), 3.2, 16, (1.0, 0.97, 0.93))
    sun("Fill", (math.radians(-18), math.radians(28), 0), 0.6, 38, (0.85, 0.9, 1.0))
    sun("Rim", (math.radians(-58), 0, math.radians(12)), 0.5, 10, (1, 1, 1))

def camera(x0, y0, x1, y1):
    """Encuadre del rectángulo de panel [x0,x1]×[y0,y1] (mm) en el plano z=0."""
    sc = bpy.context.scene
    w_mm, h_mm = x1 - x0, (y1 - y0) * math.cos(THETA)
    cam = bpy.data.cameras.new("Cam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = w_mm / 10.0
    o = bpy.data.objects.new("Cam", cam); bpy.context.collection.objects.link(o)
    view = Vector((0, math.sin(THETA), -math.cos(THETA)))
    cam.clip_start = 0.1; cam.clip_end = 1000
    o.location = P((x0 + x1) / 2, (y0 + y1) / 2) - view * 300.0
    o.rotation_euler = Euler((THETA, 0, 0), "XYZ"); sc.camera = o
    sc.render.resolution_x = int(round(w_mm * PXMM)); sc.render.resolution_y = int(round(h_mm * PXMM))
    return sc.render.resolution_x, sc.render.resolution_y

def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)

# ───────── materiales ─────────
def mat(name, col, rough, metal=0.0, coat=0.0, spec=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    return m

def body_material(color):
    """Plástico del gabinete (textura de molde) con la serigrafía blanca encima."""
    m = bpy.data.materials.new("Body"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(SILK); tex.extension = "CLIP"; tex.interpolation = "Cubic"
    uv = N.new("ShaderNodeTexCoord"); Lk.new(uv.outputs["UV"], tex.inputs["Vector"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 1400.0; nz.inputs["Detail"].default_value = 8.0
    Lk.new(uv.outputs["Object"], nz.inputs["Vector"])
    mix = N.new("ShaderNodeMix"); mix.data_type = "RGBA"
    Lk.new(tex.outputs["Alpha"], mix.inputs["Factor"]); mix.inputs["A"].default_value = (*BODY[color], 1)
    mix.inputs["B"].default_value = (0.86, 0.85, 0.82, 1)
    Lk.new(mix.outputs["Result"], b.inputs["Base Color"])
    r = N.new("ShaderNodeMapRange"); Lk.new(nz.outputs["Fac"], r.inputs["Value"])
    r.inputs["To Min"].default_value = 0.42; r.inputs["To Max"].default_value = 0.6
    Lk.new(r.outputs["Result"], b.inputs["Roughness"])
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.12; bump.inputs["Distance"].default_value = 0.002
    h = N.new("ShaderNodeMath"); h.operation = "MULTIPLY_ADD"; Lk.new(nz.outputs["Fac"], h.inputs[0]); h.inputs[1].default_value = 0.2
    Lk.new(tex.outputs["Alpha"], h.inputs[2]); Lk.new(h.outputs["Value"], bump.inputs["Height"]); Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = 0.4
    return m

M = {}
def materials(color):
    M["body"] = body_material(color)
    M["plastic"] = mat("BodyPlain", BODY[color], 0.55, spec=0.4)
    M["frame"] = mat("Frame", tuple(min(1, c * 1.35 + 0.02) for c in BODY[color]), 0.5, spec=0.4)
    M["slot"] = mat("Slot", (0.012, 0.011, 0.013), 0.85)
    M["cap"] = mat("Cap", (0.01, 0.01, 0.011), 0.42, coat=0.15)
    M["knob"] = mat("Knob", (0.01, 0.01, 0.011), 0.4, coat=0.2)
    M["green"] = mat("LineGreen", (0.05, 0.55, 0.12), 0.5)
    M["orange"] = mat("LineOrange", (0.95, 0.30, 0.03), 0.5)
    M["yellow"] = mat("LineYellow", (0.95, 0.70, 0.05), 0.5)
    M["button"] = mat("Button", (0.62, 0.57, 0.50), 0.45, coat=0.1)
    M["black"] = mat("Black", (0.012, 0.012, 0.013), 0.5)
    M["chrome"] = mat("Chrome", (0.85, 0.85, 0.85), 0.15, metal=1.0)
    M["nickel"] = mat("Nickel", (0.6, 0.6, 0.58), 0.3, metal=1.0)
    M["led"] = mat("LedOff", (0.20, 0.03, 0.04), 0.2, coat=0.6)
    M["white_key"] = mat("WhiteKey", (0.86, 0.85, 0.82), 0.28, coat=0.35)
    M["black_key"] = mat("BlackKey", (0.01, 0.01, 0.011), 0.22, coat=0.6)

# ───────── geometría ─────────
def link(o): bpy.context.collection.objects.link(o); return o
def mesh(name, bm, m, smooth=True):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    if smooth:
        for p in me.polygons: p.use_smooth = True
    me.materials.append(m); o = link(bpy.data.objects.new(name, me))
    if smooth:
        # Normales ponderadas por área: las caras grandes quedan planas y sólo el bisel se redondea. Sin esto,
        # el sombreado suave interpolaba las normales a lo largo de las caras planas y los brillos se doblaban
        # (las teclas negras se veían torcidas y las blancas abultadas).
        wn = o.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL"); wn.mode = "FACE_AREA"; wn.keep_sharp = True
    return o

def box(name, x0, y0, x1, y1, z0, z1, m, bevel=0.3):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    c = P((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    for v in bm.verts:
        v.co.x = c.x + v.co.x * (x1 - x0) / 10; v.co.y = c.y + v.co.y * (y1 - y0) / 10; v.co.z = c.z + v.co.z * (z1 - z0) / 10
    if bevel > 0: bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel / 10, segments=3, affect="EDGES")
    return mesh(name, bm, m)

# Vistas en perspectiva (hero, film): la tecla negra con su forma real, más angosta arriba y con el frente
# inclinado. Como caja recta, vista de costado y de cerca parecía torcida. La vista cenital del plugin sigue con cajas.
REAL_KEYS = False

def black_key(name, x0, y0, x1, y1, z0, z1, m, bevel=0.9):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    c = P((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    recede = min(0.45, 4.0 / max(1.0, y1 - y0))          # el frente de arriba retrocede 4 mm
    for v in bm.verts:
        if v.co.z > 0:
            v.co.x *= 0.78                                 # arriba ~78 % del ancho de la base
            if v.co.y < 0: v.co.y += recede
        v.co.x = c.x + v.co.x * (x1 - x0) / 10; v.co.y = c.y + v.co.y * (y1 - y0) / 10; v.co.z = c.z + v.co.z * (z1 - z0) / 10
    if bevel > 0: bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel / 10, segments=3, affect="EDGES")
    return mesh(name, bm, m)

def lathe(profile, segs=96, radial=None):
    bm = bmesh.new(); rings = []
    for r, z in profile:
        ring = []
        for i in range(segs):
            t = 2 * math.pi * i / segs; rr = radial(t, r, z) if radial else r
            ring.append(bm.verts.new((rr * math.cos(t) / 10, rr * math.sin(t) / 10, z / 10)))
        rings.append(ring)
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(segs): j = (i + 1) % segs; bm.faces.new((a[i], a[j], b[j], b[i]))
    if profile[-1][0] > 0:
        c = bm.verts.new((0, 0, profile[-1][1] / 10))
        for i in range(segs): bm.faces.new((rings[-1][i], rings[-1][(i + 1) % segs], c))
    return bm

def top_plane():
    """Superficie superior (sin el hueco del teclado) con UV = mm/(W,H) para la serigrafía."""
    kb = L["keyboard"]; bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    for x0, y0, x1, y1 in ((0, 0, W, kb["y0"]), (0, kb["y0"], kb["x0"], H), (kb["x1"], kb["y0"], W, H)):
        vs = [bm.verts.new(P(x, y, 0.0)) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
        f = bm.faces.new(vs)
        for lp in f.loops:
            co = lp.vert.co; lp[uvl].uv = (co.x * 10 / W, 1.0 + co.y * 10 / H)
    return mesh("Top", bm, M["body"], smooth=False)

def frame_rect(name, x0, y0, x1, y1, w=0.9, h=0.7):
    """Marco en relieve (los rectángulos moldeados que separan las secciones)."""
    box(name + "T", x0, y0 - w / 2, x1, y0 + w / 2, 0, h, M["frame"], 0.15)
    box(name + "B", x0, y1 - w / 2, x1, y1 + w / 2, 0, h, M["frame"], 0.15)
    box(name + "L", x0 - w / 2, y0, x0 + w / 2, y1, 0, h, M["frame"], 0.15)
    box(name + "R", x1 - w / 2, y0, x1 + w / 2, y1, 0, h, M["frame"], 0.15)

def build_static(color, keys_down=False):
    reset(); materials(color); lights()
    top_plane()
    # Gabinete: bordes redondeados alrededor del panel y fondo bajo el teclado.
    kb = L["keyboard"]
    box("CaseL", -6, -6, 1.0, H + 6, -40, 0.6, M["plastic"], 3.0)
    box("CaseR", W - 1.0, -6, W + 6, H + 6, -40, 0.6, M["plastic"], 3.0)
    box("CaseB", -6, -6, W + 6, 1.0, -40, 0.6, M["plastic"], 3.0)
    box("CaseF", kb["x0"] - 2, H - 4.0, kb["x1"] + 2, H + 6, -40, -8.0, M["plastic"], 2.5)
    box("CaseFL", -6, H - 4.0, kb["x0"], H + 6, -40, 0.6, M["plastic"], 3.0)
    box("CaseFR", kb["x1"], H - 4.0, W + 6, H + 6, -40, 0.6, M["plastic"], 3.0)
    box("KbWall", kb["x0"] - 0.5, kb["y0"] - 1.0, kb["x1"] + 0.5, kb["y0"] + 0.4, -14, 0.0, M["plastic"], 0.4)
    # Tira de jacks: escalón elevado al fondo.
    box("Step", 6, 2, W - 6, 33.5, 0, 2.6, M["plastic"], 0.6)
    box("StepFront", 6, 33.0, W - 6, 34.0, -0.5, 2.4, M["frame"], 0.2)
    for j in L["jacks"]:
        d = j["d"]
        o = mesh("Jack", lathe([(d * 0.75, 2.4), (d * 0.75, 3.4), (d * 0.62, 3.7), (d * 0.45, 3.7), (d * 0.42, 1.0), (0, 1.0)], 64), M["nickel"])
        o.location = P(j["x"], j["y"]); hole = mesh("JackHole", lathe([(d * 0.4, 1.2), (0, 1.2)], 32), M["slot"]); hole.location = P(j["x"], j["y"])
    # Marcos de sección.
    hy0, hy1, cy1 = L["headerY"]
    for s in L["sections"]:
        frame_rect("Hdr" + s["title"], s["x0"], hy0, s["x1"], hy1)
        frame_rect("Ctl" + s["title"], s["x0"], hy1, s["x1"], cy1)
    frame_rect("Func", 12.0, cy1, 554.0, 160.5)
    frame_rect("Perf", 4.0, 162.0, 86.5, 307.0)
    # Pozos de faders y selectores deslizantes: marco + ranura oscura.
    for s in L["sliders"]:
        x = s["x"]; short = s.get("short", False)
        y0, y1 = (s["y10"] - 9.0, s["y0"] + 9.0)
        frame_rect("Well" + s["id"], x - 4.6, y0, x + 4.6, y1, 0.7, 0.9)
        box("Slot" + s["id"], x - 1.3, s["y10"] - 3.0, x + 1.3, s["y0"] + 3.0, -2.0, 0.05, M["slot"], 0.2)
    for s in L["switches"]:
        x = s["x"]; ys = s["ys"]
        frame_rect("SwWell" + s["id"], x - 4.6, ys[0] - 10.0, x + 4.6, ys[-1] + 10.0, 0.7, 0.9)
        box("SwSlot" + s["id"], x - 1.3, ys[0] - 4.0, x + 1.3, ys[-1] + 4.0, -2.0, 0.05, M["slot"], 0.2)
    for s in L["mini"]:
        y = s["y"]; xs = s["xs"]
        box("MiniSlot" + s["id"], xs[0] - 5.5, y - 3.0, xs[-1] + 5.5, y + 3.0, -1.0, 0.9, M["black"], 2.6)
    # Botones crema, LEDs y POWER.
    for b in L["buttons"]:
        box("Btn" + b["id"], b["x"] - 6.2, b["y"] - 2.8, b["x"] + 6.2, b["y"] + 2.8, 0, 3.0, M["button"], 0.5)
        box("Led" + b["id"], b["led"][0] - 1.9, b["led"][1] - 0.8, b["led"][0] + 1.9, b["led"][1] + 0.8, 0, 0.6, M["led"], 0.2)
    pw = L["power"]
    box("Power", pw["x"] - 7.8, pw["y"] - 4.2, pw["x"] + 7.8, pw["y"] + 4.2, 0, 3.0, M["black"], 0.6)
    box("PowerLed", pw["led"][0] - 1.9, pw["led"][1] - 0.8, pw["led"][0] + 1.9, pw["led"][1] + 0.8, 0, 0.6, M["led"], 0.2)
    rl = L["rateLed"]; box("RateLed", rl[0] - 2.4, rl[1] - 0.9, rl[0] + 2.4, rl[1] + 0.9, 0, 0.6, M["led"], 0.2)
    bd = L["bender"]
    box("BenderSlot", bd["cx"] - bd["w"] / 2, bd["cy"] - bd["h"] / 2, bd["cx"] + bd["w"] / 2, bd["cy"] + bd["h"] / 2, -3.0, 0.4, M["slot"], 1.0)
    # Teclado: las teclas siguen por debajo del panel; el fondo queda en sombra.
    box("KeyBed", kb["x0"] - 1, kb["y0"] - 10, kb["x1"] + 1, H - 2, -24, -14.5, M["black"], 0.2)
    keys = []
    M_ = __import__("mathutils")
    for k in kb["keys"]:
        top = -0.6 if k["black"] else -6.0
        z0 = -13.0
        x0, x1 = k["x0"] + (0.0 if k["black"] else 0.35), k["x1"] - (0.0 if k["black"] else 0.35)
        if k["black"] and REAL_KEYS:
            o = black_key("Key%d" % k["note"], x0, k["y0"] - 9.0, x1, k["y1"], z0, top, M["black_key"])
        else:
            o = box("Key%d" % k["note"], x0, k["y0"] - 9.0, x1, k["y1"], z0, top,
                    M["black_key"] if k["black"] else M["white_key"], 0.9 if k["black"] else 0.7)
        keys.append((k, o))
        if keys_down:
            # Tecla apretada: gira alrededor del extremo trasero (baja ~5 mm adelante).
            piv = P((k["x0"] + k["x1"]) / 2, k["y0"] - 9.0, top)
            ang = math.atan2(5.0, k["y1"] - k["y0"] + 9.0)
            T = M_.Matrix.Translation(piv) @ M_.Matrix.Rotation(ang, 4, "X") @ M_.Matrix.Translation(-piv)
            o.data.transform(T)
    return keys

def frame_full():
    return -MARGIN, -MARGIN, W + MARGIN, H + MARGIN

def mode_bg():
    for color in COLORS:
        build_static(color)
        bpy.context.scene.render.film_transparent = True
        rx, ry = camera(*frame_full())
        render(os.path.join(OUT, f"bg_{color}.png"))
    build_static("grey", keys_down=True)
    bpy.context.scene.render.film_transparent = True
    camera(*frame_full())
    render(os.path.join(OUT, "keysdown.png"))
    x0, y0, x1, y1 = frame_full()
    meta = dict(width=rx, height=ry, pxmm=PXMM, x0=x0, y0=y0, theta=math.degrees(THETA),
                cosT=math.cos(THETA), sinT=math.sin(THETA), whiteTop=-6.0, blackTop=-0.6, keyBottom=-13.0)
    json.dump(meta, open(os.path.join(OUT, "bg.json"), "w"), indent=1)

def mode_preview():
    build_static(COLORS[0])
    camera(*frame_full())
    render(os.path.join(OUT, f"preview_{COLORS[0]}.png"))

# ───────── sprites (con su sombra, sobre un plano shadow catcher) ─────────
def sprite_scene():
    reset(); materials("grey"); lights()
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=50.0)
    catcher = mesh("Catcher", bm, M["plastic"], smooth=False); catcher.is_shadow_catcher = True
    bpy.context.scene.render.film_transparent = True

def sprite_camera(cx, cy, size_mm, lift_mm=0.0):
    """Cuadro cuadrado centrado en (cx, cy − lift) mm; el ancla del objeto queda lift·cosθ·px abajo del centro."""
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("SpCam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = size_mm / 10.0
    cam.clip_start = 0.1; cam.clip_end = 1000
    o = bpy.data.objects.new("SpCam", cam); link(o)
    view = Vector((0, math.sin(THETA), -math.cos(THETA)))
    o.location = P(cx, cy - lift_mm) - view * 300.0; o.rotation_euler = Euler((THETA, 0, 0), "XYZ"); sc.camera = o
    res = int(round(size_mm * PXMM)); sc.render.resolution_x = sc.render.resolution_y = res
    return res

def fader_cap(line_mat, w=10.0, h=13.6, tall=7.0):
    """Capuchón del 101: bloque negro con lomo moldeado y una línea de color sobre el frente."""
    o = box("Cap", -w / 2, -h / 2, w / 2, h / 2, 0.0, tall, M["cap"], 0.9)
    r = box("CapRidge", -w / 2 + 0.4, -h / 2 + 0.2, w / 2 - 0.4, -h / 2 + h * 0.42, tall, tall + 1.4, M["cap"], 0.6)
    ln = box("CapLine", -w / 2 + 0.6, h * 0.08, w / 2 - 0.6, h * 0.08 + 1.0, tall + 0.02, tall + 0.14, line_mat, 0.0)
    # Lomo y línea cuelgan del capuchón: sin esto, en las vistas en perspectiva (populate mueve sólo el capuchón)
    # quedaban todos amontonados en el origen, un bloque negro flotando en la esquina trasera izquierda.
    for c in (r, ln): c.parent = o
    return o

def knob_obj():
    k = mesh("Knob", lathe([(10.0, 0.0), (10.0, 8.2), (9.4, 9.8), (8.6, 10.6), (0.0, 10.9)], 192,
                           radial=lambda t, r, z: r - (0.32 if (z < 8.3 and math.cos(t * 40) > 0.0) else 0.0)), M["knob"])
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co.x *= 0.11; v.co.y = v.co.y * 0.42 + 0.62; v.co.z = v.co.z * 0.02 + 1.09
    ptr = mesh("Pointer", bm, M["orange"], smooth=False); ptr.parent = k
    return k

def render_sprite(name, size_mm, lift_mm, key, extra=None):
    res = sprite_camera(0, 0, size_mm, lift_mm)
    render(os.path.join(OUT, name + ".png"))
    m = dict(size=res, anchorX=res / 2, anchorY=res / 2 + lift_mm * math.cos(THETA) * PXMM)
    if extra: m.update(extra)
    return m

def mode_sprites():
    meta = {}
    for col in ("green", "orange"):
        sprite_scene(); fader_cap(M[col])
        meta["cap"] = render_sprite(f"cap_{col}", 26.0, 3.0, "cap")
    sprite_scene(); fader_cap(M["yellow"], 9.0, 9.0, 6.0)
    meta["swcap"] = render_sprite("swcap_yellow", 22.0, 2.5, "swcap")
    # Perilla: 128 cuadros de una vuelta completa (puntero arriba en el cuadro 0, sentido horario).
    sprite_scene(); k = knob_obj(); res = sprite_camera(0, 0, 32.0, 2.4)
    os.makedirs(os.path.join(OUT, "knob"), exist_ok=True)
    for f in range(128):
        k.rotation_euler.z = -math.radians(360.0 * f / 128)
        render(os.path.join(OUT, "knob", f"{f:03d}.png"))
    meta["knob"] = dict(size=res, frames=128, anchorX=res / 2, anchorY=res / 2 + 2.4 * math.cos(THETA) * PXMM, radiusMm=10.0)
    # Perilla metálica de los selectores de PORTAMENTO y TRANSPOSE.
    sprite_scene(); mesh("Nub", lathe([(2.4, 0.0), (2.4, 1.6), (2.1, 2.6), (1.3, 3.2), (0.0, 3.35)], 96), M["chrome"])
    meta["nub"] = render_sprite("nub", 10.0, 1.2, "nub")
    # Palanca del bender: paleta negra que pivota en el fondo de la ranura.
    sprite_scene()
    lever = box("Lever", -15.0, -2.2, 15.0, 2.2, -10.0, 5.0, M["black"], 1.6)
    grip = box("LeverGrip", -3.0, -2.8, 3.0, 2.8, 3.0, 6.2, M["black"], 1.2)
    # Pivote 10 mm debajo del panel: se corre la malla para que el origen quede en el eje.
    for o in (lever, grip): o.data.transform(__import__("mathutils").Matrix.Translation(Vector((0, 0, 1.0))))
    grip.parent = lever; lever.location = P(0, 0, -10.0)
    os.makedirs(os.path.join(OUT, "lever"), exist_ok=True)
    res = sprite_camera(0, 0, 44.0, 2.0)
    for f in range(21):
        ang = -20.0 + 2.0 * f
        lever.rotation_euler = Euler((0, math.radians(ang), 0), "XYZ")
        render(os.path.join(OUT, "lever", f"{f:03d}.png"))
    meta["lever"] = dict(size=res, frames=21, degPerFrame=2.0, anchorX=res / 2, anchorY=res / 2 + 2.0 * math.cos(THETA) * PXMM)
    # Botón crema y POWER apretados.
    sprite_scene(); box("BtnDown", -6.2, -2.8, 6.2, 2.8, 0, 1.4, M["button"], 0.5)
    meta["button"] = render_sprite("button_down", 18.0, 0.5, "button")
    sprite_scene(); box("PwDown", -7.8, -4.2, 7.8, 4.2, 0, 1.4, M["black"], 0.6)
    meta["power"] = render_sprite("power_down", 22.0, 0.5, "power")
    # LED encendido (rojo), con halo.
    sprite_scene()
    em = bpy.data.materials.new("LedOn"); em.use_nodes = True; bb = em.node_tree.nodes["Principled BSDF"]
    bb.inputs["Base Color"].default_value = (1, 0.1, 0.06, 1); bb.inputs["Emission Color"].default_value = (1, 0.06, 0.02, 1)
    bb.inputs["Emission Strength"].default_value = 30.0
    box("LedOn", -1.9, -0.8, 1.9, 0.8, 0, 0.6, em, 0.2)
    meta["led"] = render_sprite("led_on", 8.0, 0.0, "led")
    json.dump(meta, open(os.path.join(OUT, "sprites.json"), "w"), indent=1)

# ───────── vistas en perspectiva (web y video) ─────────
def populate(preset_path):
    """Pone los controles en 3D con los valores de un preset (sliders, selectores, perillas)."""
    pv = json.load(open(preset_path))["panel"] if preset_path else {}
    get = lambda k, d: pv.get(k, d)
    caps = {}                                  # capuchón de cada fader por id (el film los mueve)
    for s in L["sliders"]:
        v = get(s["id"], 0.5)
        o = fader_cap(M["green"] if s["color"] == "green" else M["orange"])
        o.location = P(s["x"], s["y0"] + (s["y10"] - s["y0"]) * v); caps[s["id"]] = o
    for s in L["switches"]:
        i = int(round(get(s["id"], 1)))
        o = fader_cap(M["yellow"], 9.0, 9.0, 6.0); o.location = P(s["x"], s["ys"][min(i, len(s["ys"]) - 1)])
    for s in L["mini"]:
        i = int(round(get(s["id"], 1)))
        n = mesh("Nub", lathe([(2.4, 0.0), (2.4, 1.6), (2.1, 2.6), (1.3, 3.2), (0.0, 3.35)], 64), M["chrome"])
        n.location = P(s["xs"][min(i, len(s["xs"]) - 1)], s["y"], 0.9)
    for k in L["knobs"]:
        v = get(k["id"], 0.5)
        a0, a1 = (-55, 55) if k["kind"] == "wave" else ((-50, 50) if k["kind"] == "range" else (-150, 150))
        t = v / max(1, k["steps"] - 1) if k["steps"] > 0 else v
        o = knob_obj(); o.scale = (k["r"] / 10.0,) * 3
        o.rotation_euler.z = -math.radians(a0 + (a1 - a0) * t); o.location = P(k["x"], k["y"])
    bd = L["bender"]
    lever = box("Lever", -15.0, -2.2, 15.0, 2.2, -10.0, 5.0, M["black"], 1.6); lever.location = P(bd["cx"], bd["cy"])
    em = bpy.data.materials.new("LedOn"); em.use_nodes = True; bb = em.node_tree.nodes["Principled BSDF"]
    bb.inputs["Base Color"].default_value = (1, 0.1, 0.06, 1); bb.inputs["Emission Color"].default_value = (1, 0.06, 0.02, 1)
    bb.inputs["Emission Strength"].default_value = 18.0
    lit = ["seq_play"] if get("seq_play", 0) > 0.5 else []
    for b in L["buttons"]:
        if b["id"] in lit: box("LedOn" + b["id"], b["led"][0] - 1.9, b["led"][1] - 0.8, b["led"][0] + 1.9, b["led"][1] + 0.8, 0, 0.7, em, 0.2)
    pw = L["power"]; box("PowerLedOn", pw["led"][0] - 1.9, pw["led"][1] - 0.8, pw["led"][0] + 1.9, pw["led"][1] + 0.8, 0, 0.7, em, 0.2)
    return caps

def stage():
    """Piso oscuro y luz de estudio para las vistas en perspectiva."""
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=400.0)
    floor = mesh("Floor", bm, mat("FloorMat", (0.012, 0.012, 0.013), 0.55), smooth=False); floor.location = Vector((28.5, -15.5, -4.05))
    a = bpy.data.lights.new("Soft", "AREA"); a.energy = 90000.0; a.size = 60.0; a.color = (1.0, 0.96, 0.92)
    o = bpy.data.objects.new("Soft", a); link(o); o.location = Vector((-20, -60, 80))
    o.rotation_euler = (Vector((28.5, -15.5, 0)) - o.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.02

def persp_camera(loc, target, lens=50.0, fstop=0.04, w=2400, h=1100):
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("PCam"); cam.lens = lens; cam.clip_start = 0.1; cam.clip_end = 2000
    cam.dof.use_dof = True; cam.dof.aperture_fstop = fstop
    o = bpy.data.objects.new("PCam", cam); link(o); o.location = Vector(loc)
    tgt = bpy.data.objects.new("Target", None); link(tgt); tgt.location = Vector(target)
    c = o.constraints.new("TRACK_TO"); c.target = tgt; c.track_axis = "TRACK_NEGATIVE_Z"; c.up_axis = "UP_Y"
    cam.dof.focus_object = tgt
    sc.camera = o; sc.render.resolution_x = w; sc.render.resolution_y = h
    return o, tgt

HERO_PRESET = arg("--preset")
def mode_hero():
    # Cámaras lejos y con lente larga: con 38-42 mm desde cerca y de costado, las teclas de un extremo y del otro
    # fugaban en direcciones muy distintas y se veían torcidas.
    global REAL_KEYS; REAL_KEYS = True
    shots = [("grey", (16.0, -150.0, 76.0), (31.0, -15.0, 0.0), 70.0),
             ("blue", (58.0, -132.0, 56.0), (33.0, -13.0, 0.0), 80.0),
             ("red", (4.0, -118.0, 34.0), (24.0, -17.0, 0.0), 72.0)]
    only = arg("--shots", "grey,blue,red").split(",")
    for color, loc, target, lens in shots:
        if color not in only: continue
        build_static(color); populate(HERO_PRESET); stage()
        persp_camera(loc, target, lens, 0.04, int(arg("--w", "2400")), int(arg("--h", "1100")))
        render(os.path.join(OUT, f"hero_{color}.png"))

def mode_anim():
    """Paneo lento sobre el instrumento gris para el video de la web (frames PNG)."""
    build_static(arg("--color", "grey")); populate(HERO_PRESET); stage()
    sc = bpy.context.scene
    frames = int(arg("--frames", "192"))
    o, tgt = persp_camera((-10.0, -70.0, 34.0), (24.0, -14.0, 0.0), 45.0, 0.05, 1280, 720)
    sc.frame_start, sc.frame_end = 1, frames
    o.location = Vector((-10.0, -70.0, 34.0)); o.keyframe_insert("location", frame=1)
    o.location = Vector((62.0, -62.0, 22.0)); o.keyframe_insert("location", frame=frames)
    tgt.location = Vector((22.0, -14.0, 0.0)); tgt.keyframe_insert("location", frame=1)
    tgt.location = Vector((38.0, -12.0, 0.0)); tgt.keyframe_insert("location", frame=frames)
    # Un fader se mueve (FREQ del VCF), como si alguien lo tocara.
    cut = next(s for s in L["sliders"] if s["id"] == "vcf_freq")
    capobj = min((ob for ob in bpy.data.objects if ob.name.startswith("Cap") and ob.parent is None),
                 key=lambda ob: (ob.location - P(cut["x"], (cut["y0"] + cut["y10"]) / 2)).length)
    for f, v in ((1, 0.25), (frames // 2, 0.7), (frames, 0.35)):
        capobj.location = P(cut["x"], cut["y0"] + (cut["y10"] - cut["y0"]) * v); capobj.keyframe_insert("location", frame=f)
    os.makedirs(os.path.join(OUT, "anim"), exist_ok=True)
    sc.render.filepath = os.path.join(OUT, "anim", "f_")
    sc.render.image_settings.color_mode = "RGB"
    sc.render.use_persistent_data = True     # la escena se sincroniza una vez, no en cada cuadro
    bpy.ops.render.render(animation=True)


def mode_film():
    """Toma del film de la web (--shot wide|...): --frames cuadros a --w×--h; --probe N renderiza sólo el cuadro N."""
    SHOTS = {
        # wide y keys: lente más larga desde más lejos (mismo encuadre), así las teclas no fugan torcidas.
        "wide": ([(-20.7, -88.7, 45.3), (70.0, -78.7, 29.3)], [(22.0, -14.0, 0.0), (38.0, -12.0, 0.0)], 60.0, 0.05),
        "sliders": ([(30.0, -32.0, 11.0), (52.0, -31.0, 10.0)], [(33.0, -9.0, 0.5), (50.0, -9.0, 0.5)], 50.0, 0.14),
        "keys": ([(2.7, -74.9, 20.2), (51.1, -74.9, 20.2)], [(12.0, -22.0, 0.0), (48.0, -22.0, 0.0)], 70.0, 0.08),
    }
    global REAL_KEYS; REAL_KEYS = True
    shot = arg("--shot", "wide")
    cams, tgts, lens, fstop = SHOTS[shot]
    build_static(arg("--color", "grey")); populate(HERO_PRESET); stage()
    sc = bpy.context.scene
    frames = int(arg("--frames", "120"))
    o, tgt = persp_camera(cams[0], tgts[0], lens, fstop, int(arg("--w", "1920")), int(arg("--h", "1080")))
    sc.frame_start, sc.frame_end = 1, frames
    for f, c, t in ((1, cams[0], tgts[0]), (frames, cams[1], tgts[1])):
        o.location = Vector(c); o.keyframe_insert("location", frame=f)
        tgt.location = Vector(t); tgt.keyframe_insert("location", frame=f)
    if arg("--probe"): sc.frame_start = sc.frame_end = int(arg("--probe"))
    os.makedirs(os.path.join(OUT, "film_" + shot), exist_ok=True)
    sc.render.filepath = os.path.join(OUT, "film_" + shot, "f_")
    sc.render.image_settings.color_mode = "RGB"
    sc.render.use_persistent_data = True     # la escena se sincroniza una vez, no en cada cuadro
    bpy.ops.render.render(animation=True)


# ───────── film de la web (filmkit) ─────────
# Música: web/assets/samples/at101v/acid-line.m4a (preset 16_acid_line, 120 bpm). El secuenciador da un paso cada
# semicorchea (0,125 s, 16 pasos = 2 s) desde t = 0; el teclado lo transpone en 4, 8, 12 y 17,75 s (+3, +5, −2, 0).
# Verificado sobre el audio: inicios cada 0,125 s y la altura del paso 0 en cada tramo (36, 39, 41, 34, 36).
BPM = 120.0
BEAT = 60.0 / BPM
STEP = BEAT / 4
TRANSPOSE = [(0.0, 0), (4.0, 3), (8.0, 5), (12.0, -2), (17.75, 0)]
# (toma, comienzo en el film en s, duración en tiempos, color del gabinete)
FILM_SHOTS = [("open", 0.0, 8, "grey"), ("seq", 4.0, 6, "blue"), ("filter", 7.0, 8, "red"), ("keys", 11.0, 6, "grey"),
              ("trio1", 14.0, 2, "blue"), ("trio2", 15.0, 2, "red"), ("trio3", 16.0, 2, "grey"), ("end", 17.0, 8, "grey")]
FILM_PRESET = os.path.join(ASSETS, "presets", "16_acid_line.json")

def note_num(name):
    """'D#2' → 39 (C4 = 60)."""
    i = 2 if len(name) > 1 and name[1] == "#" else 1
    return 12 * (int(name[i:]) + 1) + "C D EF G A B".index(name[0]) + (1 if i == 2 else 0)

def key_events(seq, until=24.0):
    """Teclas que bajan con cada paso: (nota, desde, hasta, acento). El secuenciador suena en las octavas 1-3; en el
    teclado (F3-C6) se muestra dos octavas arriba, con la transposición del momento. Ligadura: la tecla sigue abajo
    hasta que baja la del paso siguiente (legato)."""
    ev = []
    for k in range(int(until / STEP)):
        t = k * STEP; s = seq[k % len(seq)].split()
        if not s or s[0] == "-": continue
        tr = [d for (t0, d) in TRANSPOSE if t0 <= t][-1]
        off = t + (STEP + 0.03 if "slide" in s else 0.62 * STEP)
        ev.append((note_num(s[0]) + 24 + tr, t, off, "acc" in s))
    return ev

def key_depth(ev, note, t):
    """0..1: la tecla se adelanta 15 ms al sonido (se lee en sincronía), baja en 30 ms y sube en 50 ms."""
    d = 0.0
    for (n, a, b, acc) in ev:
        if n != note or t < a - 0.04 or t > b + 0.08: continue
        down = fk_ease((t - (a - 0.015)) / 0.03) if t < b else 1.0 - fk_ease((t - b) / 0.05)
        d = max(d, down * (1.0 if acc else 0.82))
    return d

def fk_ease(u):
    u = min(1.0, max(0.0, u)); return u * u * (3 - 2 * u)

def automation(t):
    """Lo que la mano hace en los faders a lo largo del film (s): abre el corte y sube la envolvente en la toma del
    filtro, y después lo devuelve a medio camino."""
    freq = 0.24 + 0.24 * fk_ease((t - 7.3) / 3.4) - 0.10 * fk_ease((t - 12.5) / 3.5)
    env = 0.60 + 0.14 * fk_ease((t - 8.1) / 2.8) - 0.06 * fk_ease((t - 13.0) / 3.0)
    res = 0.82 - 0.06 * fk_ease((t - 9.0) / 2.0)
    return {"vcf_freq": freq, "vcf_env": env, "vcf_res": res}

def film_lights():
    """Estudio oscuro: llave cálida arriba a la izquierda, relleno frío bajo a la derecha, contraluz que dibuja los
    bordes y un cenital suave para la serigrafía. Sin los soles de la vista cenital (aclaraban el piso a gris)."""
    for n in ("Key", "Fill", "Rim", "Soft"):
        o = bpy.data.objects.get(n)
        if o: bpy.data.objects.remove(o, do_unlink=True)
    C = Vector((28.5, -15.5, 0.0))
    def area(name, loc, look, size, energy, col, spread=None, rect=None):
        l = bpy.data.lights.new(name, "AREA"); l.energy = energy; l.color = col
        if rect: l.shape = "RECTANGLE"; l.size, l.size_y = rect
        else: l.size = size
        if spread: l.spread = math.radians(spread)
        o = link(bpy.data.objects.new(name, l)); o.location = Vector(loc)
        o.rotation_euler = (Vector(look) - o.location).to_track_quat("-Z", "Y").to_euler()
        return o
    area("FKey", (-18, -62, 72), C, 45, 80000, (1.0, 0.95, 0.89), spread=75)
    area("FFill", (100, -58, 22), C, 60, 9000, (0.82, 0.88, 1.0))
    area("FRim", (22, 58, 38), C + Vector((0, -6, 0)), 70, 28000, (1.0, 0.97, 0.93))
    area("FTop", (28.5, -18, 85), C, 70, 6000, (1.0, 1.0, 1.0), spread=60)
    # Barrido: una tira de luz detrás y arriba que cruza de izquierda a derecha (el brillo corre por los faders).
    return area("Sweep", (-20, 30, 26), (-20, -12, 0), 0, 0.0, (1.0, 0.92, 0.82), rect=(3.0, 60.0))

def led(name, xy, energy=0.0):
    """LED con su propio material (la intensidad se anima): apagado se ve como los demás, rojo oscuro."""
    m = bpy.data.materials.new("Led" + name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.35, 0.04, 0.04, 1); b.inputs["Roughness"].default_value = 0.2
    b.inputs["Emission Color"].default_value = (1, 0.07, 0.025, 1); b.inputs["Emission Strength"].default_value = energy
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = 0.6
    box("FLed" + name, xy[0] - 1.9, xy[1] - 0.8, xy[0] + 1.9, xy[1] + 0.8, 0, 0.75, m, 0.2)
    return b.inputs["Emission Strength"]

def film2():
    """Film de la web del AT101V: 21 s, 42 tiempos a 120 bpm sobre "Acid Line"; el instrumento toca la línea.

    Guion (comienzo s, tiempos, gabinete):
      open    0,0  8  gris  Apertura con el título abajo a la izquierda (esa zona es piso oscuro): 3/4 desde la
                            derecha y un poco abajo, acercándose despacio. Las teclas bajan con cada paso de la
                            secuencia, el LED de RATE late en cada tiempo. Foco en las teclas que tocan.
      seq     4,0  6  azul  Macro de la fila de botones del secuenciador, cámara baja: foco en el LED de RATE que
                            late al tempo y cambio de foco al LED de PLAY (titila con cada paso, más en los acentos).
                            Travelling lateral lento, cámara en mano muy sutil.
      filter  7,0  8  rojo  Los faders del VCF: el de corte sube de 0,24 a 0,48 y el de envolvente de 0,60 a 0,74
                            (como una mano); una tira de luz barre el panel de izquierda a derecha. Acercamiento
                            frontal bajo con el foco en el capuchón del corte.
      keys   11,0  6  gris  Rasante sobre el teclado desde la derecha: las teclas de la línea bajan (con la
                            transposición a −2 desde 12 s), travelling a lo largo de las teclas, poca profundidad.
      trio1  14,0  2  azul  Los tres colores: un mismo giro lento en 3/4 desde la izquierda, cortado en cada medio
      trio2  15,0  2  rojo  compás (azul, rojo, gris) sin interrumpir el movimiento de cámara.
      trio3  16,0  2  gris
      end    17,0  8  gris  De frente y algo alto, se aleja despacio; film.py lo oscurece y pone el cierre.

    --mode film2 [--shot a,b] [--quality draft|final] [--audio wav] [--out dir] [--probe N: un cuadro cada N]
    """
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    global REAL_KEYS; REAL_KEYS = True
    only = arg("--shot"); quality = arg("--quality", "final"); probe = int(arg("--probe", "0"))
    env = fk.envelope(arg("--audio")) if arg("--audio") else []
    preset = json.load(open(FILM_PRESET))
    ev = key_events(preset["sequence"])
    fps = fk.FPS
    sl = {s["id"]: s for s in L["sliders"]}
    rl, bt = L["rateLed"], {b["id"]: b for b in L["buttons"]}
    C = Vector((28.5, -15.5, 0.0))
    def orbit(deg, r, z, look_z=-2.0, cx=28.5, cy=-15.5):
        a = math.radians(deg); return (cx + r * math.cos(a), cy + r * math.sin(a), z)
    trio = fk.Path([(0.0, orbit(-128, 112, 40), (27.0, -15.5, -2.5), 62, 0.10),
                    (1.0, orbit(-108, 108, 34), (30.0, -15.5, -2.5), 62, 0.10)], easing="linear")
    # f-stop dividido por 100: 1 unidad = 10 mm, así la profundidad de campo es la de una cámara real.
    paths = {
        "open":   fk.Path([(0, (116.0, -95.0, 29.0), (30.0, -12.5, -6.0), 58, 0.08),
                           (1, (102.0, -81.0, 23.0), (29.5, -12.0, -6.0), 58, 0.08)]),
        "seq":    fk.Path([(0, (23.5, -24.0, 10.0), (12.0, -10.0, 0.3), 70, 0.06),
                           (1, (20.0, -24.5, 9.0), (12.5, -10.8, 0.3), 70, 0.06)]),
        "filter": fk.Path([(0, (42.0, -27.0, 11.0), (37.0, -8.4, 0.4), 70, 0.05),
                           (1, (39.8, -22.5, 9.0), (36.6, -8.6, 0.4), 70, 0.05)]),
        "keys":   fk.Path([(0, (55.0, -47.0, 10.0), (31.0, -25.0, -1.0), 60, 0.07),
                           (1, (49.0, -48.0, 9.0), (26.0, -25.0, -1.0), 60, 0.07)]),
        "end":    fk.Path([(0, (28.5, -96.0, 42.0), (28.5, -14.5, -2.5), 50, 0.10),
                           (1, (28.5, -128.0, 56.0), (28.5, -14.5, -2.5), 50, 0.10)], easing="out"),
    }
    t_trio, d_trio = 14.0, 3.0
    for (name, t0, beats, color) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        keys = build_static(color); caps = populate(FILM_PRESET); stage()
        for o in list(bpy.data.objects):
            if o.name.startswith("LedOnseq_play"): bpy.data.objects.remove(o, do_unlink=True)
        sweep = film_lights()
        fk.setup(bpy.context.scene, quality)
        sc = bpy.context.scene
        sc.render.image_settings.color_mode = "RGB"; sc.render.use_persistent_data = True
        # Los primeros planos ven el panel casi de frente a la luz: un poco menos de exposición para que el rojo y
        # el azul no se laven hacia el rosa y el celeste.
        sc.view_settings.exposure = {"seq": -0.35, "filter": -0.6}.get(name, 0.0)
        cam = fk.camera(sc); cam.data.clip_start = 0.1; cam.data.clip_end = 3000
        # Teclas: el origen pasa al eje trasero para girarlas como al apretarlas (8 mm abajo en el frente, se lee de lejos).
        kobj = []
        for k, o in keys:
            top = -0.6 if k["black"] else -6.0
            piv = P((k["x0"] + k["x1"]) / 2, k["y0"] - 9.0, top)
            o.data.transform(Matrix.Translation(-piv)); o.location = piv
            kobj.append((k["note"], o, math.atan2(8.0, k["y1"] - k["y0"] + 9.0)))
        rate = led("Rate", rl); play = led("Play", bt["seq_play"]["led"]); trn = led("Trans", bt["key_transpose"]["led"])
        path = paths.get(name, trio)
        frames = int(round(beats * BEAT * fps)); f0 = int(round(t0 * fps))

        def active_key(T):
            """Centro de la tecla que suena en T (para que el foco la siga en la toma del teclado)."""
            cur = [e for e in ev if e[1] <= T][-1]
            k = next(k for k in L["keyboard"]["keys"] if k["note"] == cur[0])
            return P((k["x0"] + k["x1"]) / 2, (k["y0"] + k["y1"]) / 2, -6.0)
        foc = {"T": None}

        def pose(f, t, u, name=name, path=path, t0=t0):
            T = t0 + t
            moved = []
            for note, o, amax in kobj:
                o.rotation_euler.x = amax * key_depth(ev, note, T); moved.append(o)
            for sid, v in automation(T).items():
                s = sl[sid]; caps[sid].location = P(s["x"], s["y0"] + (s["y10"] - s["y0"]) * v); moved.append(caps[sid])
            uu, shake, focus = u, 0.0, None
            if name.startswith("trio"):
                uu = (T - t_trio) / d_trio; focus = Vector((28.0, -18.0, -1.0))
            elif name == "open":
                focus = Vector((27.0, -22.0, -0.6))
            elif name == "seq":                     # del LED de RATE al de PLAY
                k = fk.rack(u, 0.32, 0.68, 0.0, 1.0)
                focus = P(*rl, 0.6).lerp(P(*bt["seq_play"]["led"], 0.6), k); shake = 0.10
            elif name == "filter":
                focus = caps["vcf_freq"].location + Vector((0, 0, 0.7)); shake = 0.08
            elif name == "keys":                    # el foco sigue la tecla que suena, suavizado
                tgt = active_key(T)
                foc["T"] = tgt if foc["T"] is None else foc["T"].lerp(tgt, 0.18)
                focus = foc["T"]; shake = 0.06
            elif name == "end":
                focus = Vector((28.5, -16.0, -1.0))
            path.apply(cam, uu, focus=focus, shake=shake, t=T)
            return moved

        def extra(f, name=name, f0=f0):
            T = t0 + (f - 1) / fps; g = f0 + f - 1
            e = env[min(g, len(env) - 1)] if env else 0.5
            ph = (T % BEAT) / BEAT
            rate.default_value = 12.0 if ph < 0.22 else 0.0; rate.keyframe_insert("default_value")
            play.default_value = 3.0 + 9.0 * e; play.keyframe_insert("default_value")
            trn.default_value = 4.0; trn.keyframe_insert("default_value")
            # El barrido cruza el panel durante la toma del filtro y vuelve, más tenue, en el trío.
            if name == "filter": k = (T - 7.0) / 4.0
            elif name.startswith("trio"): k = (T - t_trio) / d_trio
            else: k = -1.0
            sweep.location.x = -20.0 + 100.0 * fk_ease(k)
            sweep.data.energy = (22000.0 if name == "filter" else 30000.0) * math.sin(math.pi * min(1, max(0, k)))
            sweep.keyframe_insert("location"); sweep.data.keyframe_insert("energy")

        if probe: fk.render_shot(sc, name, frames, pose, OUT, step=probe, extra_keys=extra)
        else: fk.render_shot(sc, name, frames, pose, OUT, extra_keys=extra)

if __name__ == "__main__":
    modes = {"bg": mode_bg, "sprites": mode_sprites, "preview": mode_preview, "hero": mode_hero, "anim": mode_anim, "film": mode_film,
             "film2": film2}
    for m in arg("--mode", "bg,sprites").split(","):
        print("== modo", m, flush=True); modes[m]()
