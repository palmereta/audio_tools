"""Escena Blender del AT42 (vista cenital del panel completo, y vistas en perspectiva para la web).

Corre en pc50-wsl:
  blender -b -P scene42.py -- --layout layout42.json --silk silkscreen42.png --out out42 \
          --mode bg,sprites --samples 200 [--colors cream,black,maize,blue]
  blender -b -P scene42.py -- --mode hero --preset preset.json --out out_hero
  blender -b -P scene42.py -- --mode anim --frames 192 --preset preset.json --out out_hero

1 unidad de Blender = 10 mm. Punto del panel (x, y) mm → (x/10, −y/10, z/10).
Cámara ortográfica inclinada THETA: en pantalla sx = x − x0, sy = (y − y0)·cosθ − z·sinθ (mm).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Euler, Matrix, Quaternion

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d

HERE = os.path.dirname(os.path.abspath(__file__))
def _asset(a, name, rel):
    """Archivo pasado por argumento; si no, el del directorio actual y si no existe, el del repo (assets/)."""
    if arg(a): return arg(a)
    return name if os.path.exists(name) else os.path.join(HERE, "..", "..", "assets", rel)
L = json.load(open(_asset("--layout", "layout42.json", "layout42.json")))
SILK = os.path.abspath(_asset("--silk", "silkscreen42.png", os.path.join("render", "silkscreen42.png")))
OUT = os.path.abspath(arg("--out", "out42"))
SAMPLES = int(arg("--samples", "200"))
COLORS = arg("--colors", "cream,black,maize,blue").split(",")
os.makedirs(OUT, exist_ok=True)

THETA = math.radians(8.0)
PXMM = float(arg("--pxmm", "6.0"))
MARGIN = 7.0
W, H = L["size"]
# Pintura en polvo del panel y tinta de la serigrafía (lineal).
PANEL = {"cream": ((0.80, 0.69, 0.55), (0.012, 0.012, 0.012)),
         "black": ((0.016, 0.016, 0.018), (0.80, 0.79, 0.76)),
         "maize": ((0.74, 0.36, 0.012), (0.012, 0.012, 0.012)),
         "blue": ((0.024, 0.105, 0.25), (0.80, 0.79, 0.76))}
CAP = {"orange": (0.82, 0.26, 0.05), "teal": (0.0, 0.11, 0.19), "green": (0.0, 0.11, 0.045), "grey": (0.55, 0.55, 0.55),
       "black": (0.012, 0.012, 0.013), "red": (0.69, 0.035, 0.024)}
BTN = {"black": (0.012, 0.012, 0.013), "grey": (0.27, 0.30, 0.34), "light": (0.56, 0.60, 0.62), "red": (0.69, 0.035, 0.024)}

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
    sun("Key", (math.radians(34), math.radians(-22), math.radians(-18)), 2.6, 18, (1.0, 0.97, 0.93))
    sun("Fill", (math.radians(-18), math.radians(28), 0), 0.55, 40, (0.85, 0.9, 1.0))
    sun("Rim", (math.radians(-58), 0, math.radians(12)), 0.45, 10, (1, 1, 1))

def camera(x0, y0, x1, y1):
    sc = bpy.context.scene
    w_mm, h_mm = x1 - x0, (y1 - y0) * math.cos(THETA)
    cam = bpy.data.cameras.new("Cam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = w_mm / 10.0
    cam.clip_start = 0.1; cam.clip_end = 1000
    o = bpy.data.objects.new("Cam", cam); bpy.context.collection.objects.link(o)
    view = Vector((0, math.sin(THETA), -math.cos(THETA)))
    o.location = P((x0 + x1) / 2, (y0 + y1) / 2) - view * 300.0
    o.rotation_euler = Euler((THETA, 0, 0), "XYZ"); sc.camera = o
    sc.render.resolution_x = int(round(w_mm * PXMM)); sc.render.resolution_y = int(round(h_mm * PXMM))
    return sc.render.resolution_x, sc.render.resolution_y

def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)

# ───────── materiales ─────────
def mat(name, col, rough, metal=0.0, coat=0.0, spec=0.5, sss=0.0, emit=None):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    if sss > 0 and "Subsurface Weight" in b.inputs:
        b.inputs["Subsurface Weight"].default_value = sss; b.inputs["Subsurface Radius"].default_value = (0.3, 0.3, 0.3)
    if emit:
        b.inputs["Emission Color"].default_value = (*emit[0], 1); b.inputs["Emission Strength"].default_value = emit[1]
    return m

def panel_material(color):
    """Pintura en polvo (textura fina) con la serigrafía: R = tinta, G = pestañas (texto calado)."""
    body, ink = PANEL[color]
    m = bpy.data.materials.new("Panel"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(SILK); tex.image.colorspace_settings.name = "Non-Color"
    tex.extension = "CLIP"; tex.interpolation = "Cubic"
    uv = N.new("ShaderNodeTexCoord"); Lk.new(uv.outputs["UV"], tex.inputs["Vector"])
    sep = N.new("ShaderNodeSeparateColor"); Lk.new(tex.outputs["Color"], sep.inputs["Color"])
    mx = N.new("ShaderNodeMath"); mx.operation = "MAXIMUM"; Lk.new(sep.outputs[0], mx.inputs[0]); Lk.new(sep.outputs[1], mx.inputs[1])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 2600.0; nz.inputs["Detail"].default_value = 6.0
    Lk.new(uv.outputs["Object"], nz.inputs["Vector"])
    mix = N.new("ShaderNodeMix"); mix.data_type = "RGBA"
    Lk.new(mx.outputs["Value"], mix.inputs["Factor"]); mix.inputs["A"].default_value = (*body, 1); mix.inputs["B"].default_value = (*ink, 1)
    Lk.new(mix.outputs["Result"], b.inputs["Base Color"])
    r = N.new("ShaderNodeMapRange"); Lk.new(nz.outputs["Fac"], r.inputs["Value"])
    r.inputs["To Min"].default_value = 0.48; r.inputs["To Max"].default_value = 0.68
    Lk.new(r.outputs["Result"], b.inputs["Roughness"])
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.18; bump.inputs["Distance"].default_value = 0.0015
    Lk.new(nz.outputs["Fac"], bump.inputs["Height"]); Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = 0.42
    return m

M = {}
def materials(color):
    M["panel"] = panel_material(color)
    M["case"] = mat("Case", (0.035, 0.035, 0.038), 0.55, metal=0.3)
    M["blackpanel"] = mat("BlackPanel", (0.008, 0.008, 0.009), 0.62)
    M["plate"] = mat("Plate", (0.80, 0.80, 0.78), 0.4)
    M["stripe"] = mat("Stripe", (0.01, 0.01, 0.01), 0.5)
    M["nut"] = mat("Nut", (0.55, 0.55, 0.56), 0.32, metal=1.0)
    M["hole"] = mat("Hole", (0.004, 0.004, 0.004), 0.9)
    M["chrome"] = mat("Chrome", (0.86, 0.86, 0.86), 0.12, metal=1.0)
    M["skirt"] = mat("Skirt", (0.035, 0.035, 0.035), 0.42)
    M["pointer"] = mat("Pointer", (0.85, 0.85, 0.85), 0.4)
    M["orb"] = mat("Orb", (0.60, 0.58, 0.54), 0.45, sss=0.6)
    M["glass"] = mat("Glass", (0.01, 0.025, 0.07), 0.3, coat=0.3)
    M["key"] = mat("Key", (0.72, 0.71, 0.68), 0.38, coat=0.2)
    M["window"] = mat("Window", (0.02, 0.02, 0.02), 0.2, coat=0.6)
    M["screw"] = mat("Screw", (0.4, 0.4, 0.41), 0.35, metal=1.0)
    for k, c in CAP.items(): M["cap_" + k] = mat("Cap" + k, c, 0.38, coat=0.25)
    for k, c in BTN.items(): M["btn_" + k] = mat("Btn" + k, c, 0.4, coat=0.15)
    for k, c in (("amber", (0.25, 0.12, 0.0)), ("red", (0.25, 0.02, 0.01)), ("blue", (0.0, 0.05, 0.14)), ("green", (0.03, 0.12, 0.0))):
        M["led_" + k] = mat("Led" + k, c, 0.15, coat=0.8)

# ───────── geometría ─────────
def link(o): bpy.context.collection.objects.link(o); return o
def mesh(name, bm, m, smooth=True):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    if smooth:
        for p in me.polygons: p.use_smooth = True
    me.materials.append(m); return link(bpy.data.objects.new(name, me))

def box(name, x0, y0, x1, y1, z0, z1, m, bevel=0.3):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    c = P((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    for v in bm.verts:
        v.co.x = c.x + v.co.x * (x1 - x0) / 10; v.co.y = c.y + v.co.y * (y1 - y0) / 10; v.co.z = c.z + v.co.z * (z1 - z0) / 10
    if bevel > 0: bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel / 10, segments=3, affect="EDGES")
    return mesh(name, bm, m)

def lathe(profile, segs=64, radial=None):
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

def at(o, x, y, z=0.0): o.location = P(x, y, z); return o

def top_plane():
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    vs = [bm.verts.new(P(x, y, 0.0)) for x, y in ((0, 0), (W, 0), (W, H), (0, H))]
    f = bm.faces.new(vs)
    for lp in f.loops:
        co = lp.vert.co; lp[uvl].uv = (co.x * 10 / W, 1.0 + co.y * 10 / H)
    return mesh("Top", bm, M["panel"], smooth=False)

def jack(x, y, dark=False):
    nut = mesh("Nut", lathe([(4.55, 0.0), (4.55, 1.6), (4.1, 2.0), (0.0, 2.0)], 6), M["nut"]); at(nut, x, y)
    nut.rotation_euler.z = math.radians(30)
    ring = mesh("Ring", lathe([(3.5, 2.0), (3.5, 3.1), (2.8, 3.4), (2.2, 3.4), (2.15, 1.0), (0.0, 1.0)], 48), M["nut"]); at(ring, x, y)
    hole = mesh("Hole", lathe([(2.15, 1.2), (0.0, 1.2)], 32), M["hole"]); at(hole, x, y)

def button_up(b, z=0.0):
    d = b["d"]
    bez = mesh("Bezel", lathe([(d / 2 + 1.2, 0.0), (d / 2 + 1.2, 0.6), (d / 2 + 0.4, 0.9), (0.0, 0.9)], 48), M["skirt"]); at(bez, b["x"], b["y"], z)
    cap = mesh("Btn", lathe([(d / 2, 0.0), (d / 2, 3.4), (d / 2 - 0.5, 4.0), (0.0, 4.1)], 48), M["btn_" + b["color"]]); at(cap, b["x"], b["y"], z)
    return cap

def toggle_base(t):
    m1 = mesh("Bush", lathe([(3.4, 0.0), (3.4, 0.9), (2.75, 1.2), (2.75, 2.8), (0.0, 2.8)], 6 if True else 48), M["nut"]); at(m1, t["x"], t["y"])
    m2 = mesh("Thread", lathe([(2.6, 0.0), (2.6, 3.6), (0.0, 3.6)], 32), M["chrome"]); at(m2, t["x"], t["y"])

def led(l):
    o = mesh("Led", lathe([(1.75, 0.0), (1.75, 0.6), (1.4, 1.3), (0.0, 1.6)], 32), M["led_" + l["color"]]); at(o, l["x"], l["y"])
    r = mesh("LedRing", lathe([(2.3, 0.0), (2.3, 0.4), (1.8, 0.5), (0.0, 0.5)], 32), M["skirt"]); at(r, l["x"], l["y"])

def screw(x, y, z=0.0):
    o = mesh("Screw", lathe([(1.6, 0.0), (1.6, 0.4), (1.3, 0.8), (0.0, 0.9)], 24), M["screw"]); at(o, x, y, z)
    box("Socket", x - 0.6, y - 0.6, x + 0.6, y + 0.6, z + 0.5, z + 0.95, M["hole"], 0.0)

def build_static(color):
    reset(); materials(color); lights()
    top_plane()
    box("Panel", 0, 0, W, H, -1.6, -0.02, M["panel"], 0.6)
    box("Case", -1.8, -1.8, W + 1.8, H + 1.8, -29.0, -1.5, M["case"], 2.2)
    for x, y in ((3.0, 3.0), (W - 3.0, 3.0), (3.0, H - 3.0), (W - 3.0, H - 3.0), (W / 2, 3.0), (W / 2, H - 3.0)): screw(x, y)
    for j in L["jacks"]:
        jack(j["x"], j["y"], j.get("dark", False))
    for b in L["buttons"]:
        button_up(b, 1.0 if b["color"] == "red" else 0.0)
    for t in L["toggles"]: toggle_base(t)
    for l in L["leds"]: led(l)
    for o in L["orbs"]:
        bez = mesh("OrbBezel", lathe([(o["d"] / 2 + 1.4, 0.0), (o["d"] / 2 + 1.4, 1.2), (o["d"] / 2 + 0.3, 1.6), (0.0, 1.6)], 96), M["skirt"]); at(bez, o["x"], o["y"])
        prof = [(o["d"] / 2 * math.cos(a), 1.6 + o["d"] / 2 * 0.42 * math.sin(a)) for a in [i * math.pi / 2 / 16 for i in range(17)]]
        dome = mesh("Orb", lathe(prof, 96), M["orb"]); at(dome, o["x"], o["y"])
    for bar in L["bars"]:
        for i in range(5):
            x0 = bar["x0"] + 1.2 + i * (bar["x1"] - bar["x0"] - 2.4) / 5
            box("Seg", x0 + 0.25, bar["y0"] + 1.2, x0 + (bar["x1"] - bar["x0"] - 2.4) / 5 - 0.25, bar["y1"] - 1.2, 0.0, 0.25, M["window"], 0.1)
    s = L["slot"]
    box("SlotFrame", s["x0"], s["y0"], s["x1"], s["y1"], 0.0, 1.0, M["nut"], 0.5)
    box("SlotHole", s["x0"] + 2.0, s["y0"] + 2.0, s["x1"] - 2.0, s["y1"] - 2.0, 0.6, 1.05, M["hole"], 0.2)
    # Teclado táctil: panel negro, placas con rayas, pantalla, botones de transposición.
    T = L["touch"]
    box("Touch", T["x0"], T["y0"], T["x1"], T["y1"], 0.0, 1.0, M["blackpanel"], 0.8)
    for x in (76.2, 162.5, 247.5, 332.5, 418.7):
        screw(x, 223.8); screw(x, 311.2)
    for p in L["plates"]:
        x0, y0, x1, y1 = p["x"] - p["w"] / 2, p["y"] - p["h"] / 2, p["x"] + p["w"] / 2, p["y"] + p["h"] / 2
        box("Plate", x0, y0, x1, y1, 1.0, 1.35, M["plate"], 0.6)
        n = 9
        for i in range(n):
            yy = y0 + 4.0 + i * (p["h"] - 8.0) / (n - 1)
            box("Stripe", x0 + 2.6, yy - 0.9, x1 - 2.6, yy + 0.9, 1.35, 1.42, M["stripe"], 0.0)
    D = L["display"]
    box("Display", D["x0"], D["y0"], D["x1"], D["y1"], 1.0, 1.25, M["glass"], 0.3)
    # Teclas drone y joystick.
    for k in L["dronekeys"]:
        s2 = k["s"] / 2
        box("DKey", k["x"] - s2, k["y"] - s2, k["x"] + s2, k["y"] + s2, 0.0, 6.0, M["key"], 1.6)
        box("DKeyWin", k["x"] - 3.2, k["y"] - s2 + 1.8, k["x"] + 3.2, k["y"] - s2 + 3.6, 6.0, 6.08, M["window"], 0.2)
    for x, y in ((433.5, 223.8), (433.5, 267.5), (433.5, 311.2), (480.5, 223.8), (480.5, 267.5), (480.5, 311.2)): screw(x, y)
    j = L["joystick"]
    base = mesh("JoyBase", lathe([(j["d"] / 2, 0.0), (j["d"] / 2, 1.8), (j["d"] / 2 - 2.0, 3.0), (5.0, 3.6), (0.0, 3.6)], 96), M["skirt"]); at(base, j["x"], j["y"])
    for x, y in ((24.2, 250.7), (50.1, 250.7), (24.2, 284.2), (50.1, 284.2)): screw(x, y)

def frame_full():
    return -MARGIN, -MARGIN, W + MARGIN, H + MARGIN

def mode_bg():
    rx = ry = 0
    for color in COLORS:
        build_static(color)
        bpy.context.scene.render.film_transparent = True
        rx, ry = camera(*frame_full())
        render(os.path.join(OUT, f"bg_{color}.png"))
    x0, y0, x1, y1 = frame_full()
    json.dump(dict(width=rx, height=ry, pxmm=PXMM, x0=x0, y0=y0, theta=math.degrees(THETA),
                   cosT=math.cos(THETA), sinT=math.sin(THETA)), open(os.path.join(OUT, "bg.json"), "w"), indent=1)

def mode_preview():
    build_static(COLORS[0]); camera(*frame_full()); render(os.path.join(OUT, f"preview_{COLORS[0]}.png"))

# ───────── sprites (con sombra sobre un plano shadow catcher) ─────────
def sprite_scene():
    reset(); materials("cream"); lights()
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=50.0)
    catcher = mesh("Catcher", bm, M["case"], smooth=False); catcher.is_shadow_catcher = True
    bpy.context.scene.render.film_transparent = True

def sprite_camera(size_mm, lift_mm=0.0):
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("SpCam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = size_mm / 10.0
    cam.clip_start = 0.1; cam.clip_end = 1000
    o = bpy.data.objects.new("SpCam", cam); link(o)
    view = Vector((0, math.sin(THETA), -math.cos(THETA)))
    o.location = P(0, -lift_mm) - view * 300.0; o.rotation_euler = Euler((THETA, 0, 0), "XYZ"); sc.camera = o
    res = int(round(size_mm * PXMM)); sc.render.resolution_x = sc.render.resolution_y = res
    return res

def sprite_meta(res, lift):
    return dict(size=res, anchorX=res / 2, anchorY=res / 2 + lift * math.cos(THETA) * PXMM)

def knob_obj(size, color):
    """Perilla: falda moleteada oscura + tapa de color + línea blanca (o disco con punto, KS)."""
    d = {"KS": 8.0, "KM": 12.1, "KL": 14.0, "KXL": 17.9, "RSW": 18.8, "RSO": 14.0}[size]
    r = d / 2
    if size == "KS":
        k = mesh("Knob", lathe([(r, 0.0), (r, 5.6), (r - 0.5, 6.3), (0.0, 6.4)], 96, radial=lambda t, rr, z: rr - (0.12 if (z < 5.5 and math.cos(t * 30) > 0) else 0)), M["cap_black"])
        dot = mesh("Dot", lathe([(0.55, 6.38), (0.0, 6.45)], 24), M["pointer"]); dot.location = Vector((0, r * 0.62 / 10, 0)); dot.parent = k
        return k
    capr = {"KM": 4.75, "KL": 4.75, "KXL": 6.05, "RSW": 6.4, "RSO": 5.4}[size]
    k = mesh("Knob", lathe([(r, 0.0), (r, 7.0), (r - 0.6, 7.6), (capr + 0.3, 7.7), (0.0, 7.7)], 128,
                           radial=lambda t, rr, z: rr - (0.3 if (z < 6.9 and rr > capr + 0.5 and math.cos(t * 36) > 0) else 0)), M["skirt"])
    capm = M["cap_" + color] if color in CAP else M["cap_black"]
    cap = mesh("Cap", lathe([(capr, 7.6), (capr, 9.6), (capr - 0.5, 10.2), (0.0, 10.3)], 96), capm); cap.parent = k
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co.x *= 0.09; v.co.y = v.co.y * capr * 0.08 + capr * 0.055; v.co.z = v.co.z * 0.02 + 1.03
    ptr = mesh("Pointer", bm, M["pointer"], smooth=False); ptr.parent = k
    return k

def mode_sprites():
    meta = {"knobs": {}, "buttons": {}}
    combos = sorted({(k["size"], k["color"]) for k in L["knobs"]})
    for size, color in combos:
        sprite_scene(); k = knob_obj(size, color)
        d = {"KS": 8.0, "KM": 12.1, "KL": 14.0, "KXL": 17.9, "RSW": 18.8, "RSO": 14.0}[size]
        sz = d + 9.0; lift = 2.2
        res = sprite_camera(sz, lift)
        name = f"knob_{size}_{color}"
        os.makedirs(os.path.join(OUT, name), exist_ok=True)
        for f in range(128):
            k.rotation_euler.z = -math.radians(360.0 * f / 128)
            render(os.path.join(OUT, name, f"{f:03d}.png"))
        meta["knobs"][f"{size}_{color}"] = dict(sprite_meta(res, lift), frames=128, d=d)
    # Botones apretados (latch o momentáneos).
    for color, d in sorted({(b["color"], b["d"]) for b in L["buttons"]}):
        sprite_scene()
        cap = mesh("Btn", lathe([(d / 2, 0.0), (d / 2, 2.2), (d / 2 - 0.5, 2.8), (0.0, 2.9)], 48), M["btn_" + color])
        res = sprite_camera(d + 6.0, 0.8)
        nm = f"btn_{color}_{d:.1f}".replace(".", "_")
        render(os.path.join(OUT, nm + ".png"))
        meta["buttons"][f"{color}_{d:.1f}"] = dict(sprite_meta(res, 0.8), file=nm)
    # Palanca de los toggles: arriba, centro, abajo.
    sprite_scene()
    bat = mesh("Bat", lathe([(1.0, 0.0), (1.0, 5.5), (1.4, 7.6), (1.1, 8.8), (0.0, 9.0)], 48), M["chrome"])
    bat.location = P(0, 0, 3.6)
    res = sprite_camera(18.0, 2.0)
    for nm, ang in (("up", 30), ("mid", 0), ("down", -30)):
        bat.rotation_euler = Euler((math.radians(ang), 0, 0), "XYZ")
        render(os.path.join(OUT, f"bat_{nm}.png"))
    meta["bat"] = sprite_meta(res, 2.0)
    # Tecla drone apretada.
    sprite_scene(); box("DKey", -8.5, -8.5, 8.5, 8.5, 0.0, 4.2, M["key"], 1.6)
    box("DKeyWin", -3.2, -6.7, 3.2, -4.9, 4.2, 4.28, M["window"], 0.2)
    res = sprite_camera(24.0, 1.6); render(os.path.join(OUT, "dkey_down.png")); meta["dkey"] = sprite_meta(res, 1.6)
    # Capuchón del joystick.
    sprite_scene()
    shaft = mesh("Shaft", lathe([(1.6, 0.0), (1.6, 6.0), (0.0, 6.0)], 32), M["chrome"]); shaft.location = P(0, 0, 3.6)
    cap = mesh("JCap", lathe([(5.05, 0.0), (5.05, 2.6), (4.4, 3.6), (0.0, 3.9)], 64, radial=lambda t, rr, z: rr - (0.2 if (z < 2.5 and math.cos(t * 24) > 0) else 0)), M["skirt"])
    cap.location = P(0, 0, 9.0)
    res = sprite_camera(20.0, 5.0); render(os.path.join(OUT, "joy_cap.png")); meta["joy"] = sprite_meta(res, 5.0)
    # Fichas de 3,5 mm para los cables.
    os.makedirs(os.path.join(OUT, "plugs"), exist_ok=True)
    for cname, col in (("red", (0.7, 0.04, 0.03)), ("yellow", (0.8, 0.5, 0.02)), ("blue", (0.02, 0.15, 0.6)),
                       ("green", (0.03, 0.4, 0.08)), ("white", (0.8, 0.8, 0.78)), ("black", (0.015, 0.015, 0.015))):
        sprite_scene()
        body = mat("PlugBody" + cname, col, 0.35, coat=0.3)
        pl = mesh("Plug", lathe([(3.4, 0.0), (3.4, 1.2), (3.0, 1.6), (3.0, 9.0), (2.4, 10.2), (1.6, 11.0), (0.0, 11.2)], 48,
                                radial=lambda t, rr, z: rr - (0.12 if (1.8 < z < 8.5 and math.cos(t * 20) > 0) else 0)), body)
        pl.location = P(0, 0, 2.0)
        res = sprite_camera(18.0, 5.0)
        render(os.path.join(OUT, "plugs", f"{cname}.png"))
    meta["plugs"] = dict(sprite_meta(res, 5.0), topZ=13.0)
    json.dump(meta, open(os.path.join(OUT, "sprites.json"), "w"), indent=1)

# ───────── vistas en perspectiva (web y video) ─────────
KN = {}                                      # id → (perilla, ángulo del preset en grados): el film las mueve
JOY = {}
def populate(preset_path, color):
    pv = json.load(open(preset_path))["panel"] if preset_path else {}
    for k in L["knobs"]:
        v = pv.get(k["id"], 0.5)
        o = knob_obj(k["size"], k["color"])
        t = (v / max(1, k["steps"] - 1)) if k["steps"] > 0 else v
        a = k["a0"] + (k["a1"] - k["a0"]) * t
        o.rotation_euler.z = -math.radians(a); KN[k["id"]] = (o, a)
        at(o, k["x"], k["y"], 1.0 if k["id"] == "kb_enc" else 0.0)
    for t in L["toggles"]:
        i = int(round(pv.get(t["id"], 1 if t["positions"] == 3 else 0)))
        bat = mesh("Bat", lathe([(1.0, 0.0), (1.0, 5.5), (1.4, 7.6), (1.1, 8.8), (0.0, 9.0)], 32), M["chrome"])
        ang = (30, 0, -30)[i] if t["positions"] == 3 else (30 if i == 0 else -30)
        bat.rotation_euler = Euler((math.radians(ang), 0, 0), "XYZ"); at(bat, t["x"], t["y"], 3.6)
    j = L["joystick"]
    cap = mesh("JCap", lathe([(5.05, 0.0), (5.05, 2.6), (4.4, 3.6), (0.0, 3.9)], 64), M["skirt"]); at(cap, j["x"], j["y"], 9.0)
    # Fuelle de goma y vástago cromado: sin ellos el capuchón quedaba flotando sobre la base.
    boot = mesh("JBoot", lathe([(7.4, 3.2), (6.8, 4.2), (4.6, 5.2), (2.9, 5.8), (2.4, 5.9), (2.4, 5.0), (0.0, 5.0)], 64), M["skirt"]); at(boot, j["x"], j["y"])
    shaft = mesh("JShaft", lathe([(1.6, 2.0), (1.6, 9.4), (0.0, 9.4)], 32), M["chrome"]); at(shaft, j["x"], j["y"])
    JOY.update(cap=cap, shaft=shaft, boot=boot)
    em = mat("OrbGlow", (0.9, 0.2, 0.15), 0.4, sss=0.5, emit=((1.0, 0.08, 0.04), 2.5))
    for o in L["orbs"][:2]:
        prof = [(o["d"] / 2 * 0.98 * math.cos(a), 1.7 + o["d"] / 2 * 0.6 * math.sin(a)) for a in [i * math.pi / 2 / 16 for i in range(17)]]
        g = mesh("OrbOn", lathe(prof, 96), em); at(g, o["x"], o["y"])

def stage():
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=400.0)
    floor = mesh("Floor", bm, mat("FloorMat", (0.012, 0.012, 0.013), 0.55), smooth=False); floor.location = Vector((24.75, -16.0, -2.95))
    a = bpy.data.lights.new("Soft", "AREA"); a.energy = 90000.0; a.size = 60.0; a.color = (1.0, 0.96, 0.92)
    o = bpy.data.objects.new("Soft", a); link(o); o.location = Vector((-20, -60, 80))
    o.rotation_euler = (Vector((24.75, -16.0, 0)) - o.location).to_track_quat("-Z", "Y").to_euler()
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

PRESET = arg("--preset")
def mode_hero():
    shots = [("cream", (4.0, -72.0, 42.0), (25.0, -16.0, 0.0), 40.0),
             ("black", (52.0, -50.0, 22.0), (20.0, -12.0, 0.0), 55.0),
             ("maize", (-6.0, -44.0, 16.0), (18.0, -18.0, 0.0), 36.0),
             ("blue", (30.0, -58.0, 30.0), (26.0, -14.0, 0.0), 46.0)]
    for color, loc, target, lens in shots:
        if color not in COLORS: continue
        build_static(color); populate(PRESET, color); stage()
        persp_camera(loc, target, lens)
        render(os.path.join(OUT, f"hero_{color}.png"))

def mode_anim():
    build_static(arg("--color", "cream")); populate(PRESET, "cream"); stage()
    sc = bpy.context.scene
    frames = int(arg("--frames", "192"))
    o, tgt = persp_camera((-8.0, -66.0, 36.0), (20.0, -14.0, 0.0), 42.0, 0.05, 1280, 720)
    sc.frame_start, sc.frame_end = 1, frames
    o.location = Vector((-8.0, -66.0, 36.0)); o.keyframe_insert("location", frame=1)
    o.location = Vector((56.0, -58.0, 24.0)); o.keyframe_insert("location", frame=frames)
    tgt.location = Vector((18.0, -14.0, 0.0)); tgt.keyframe_insert("location", frame=1)
    tgt.location = Vector((32.0, -12.0, 0.0)); tgt.keyframe_insert("location", frame=frames)
    os.makedirs(os.path.join(OUT, "anim"), exist_ok=True)
    sc.render.filepath = os.path.join(OUT, "anim", "f_")
    sc.render.image_settings.color_mode = "RGB"
    sc.render.use_persistent_data = True     # la escena se sincroniza una vez, no en cada cuadro
    bpy.ops.render.render(animation=True)


def mode_film():
    """Toma del film de la web (--shot wide|...): --frames cuadros a --w×--h; --probe N renderiza sólo el cuadro N."""
    SHOTS = {
        "wide": ([(-8.0, -66.0, 36.0), (56.0, -58.0, 24.0)], [(18.0, -14.0, 0.0), (32.0, -12.0, 0.0)], 42.0, 0.05),
        "orbs": ([(1.0, -29.0, 11.0), (21.0, -28.0, 10.0)], [(7.0, -10.0, 0.6), (16.0, -10.0, 0.6)], 50.0, 0.14),
        "keys": ([(6.0, -53.0, 15.0), (44.0, -53.0, 15.0)], [(10.0, -27.0, 0.0), (40.0, -27.0, 0.0)], 45.0, 0.1),
    }
    shot = arg("--shot", "wide")
    cams, tgts, lens, fstop = SHOTS[shot]
    build_static(arg("--color", "cream")); populate(PRESET, arg('--color', 'cream')); stage()
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
FILM_SHOTS = [                                 # (toma, cuadro de inicio en el audio, cuadros): los cortes caen en las
    ("open", 0, 113), ("orbs", 113, 102), ("knobs", 215, 71), ("touch", 286, 65),   # crestas del oleaje del drone
    ("joy", 351, 70), ("wide", 421, 70), ("end", 491, 85)]                          # (cathedral-a, ~1,43 s)
BANDS = ((30, 150), (150, 400), (400, 1200), (1200, 6000))
ORB_COL = ((1.0, 0.20, 0.05), (1.0, 0.16, 0.04), (1.0, 0.26, 0.07), (1.0, 0.21, 0.06))
ORB_LEVEL = (1.0, 1.0, 0.55, 0.45)             # el preset toca los drones 1 y 2; los otros dos respiran más bajo
PHRASE = (2, 4, 7, 9, 7, 4, 3, 8, 10, 5, 1, 6)   # orden en que se encienden las placas táctiles
CABLES = (("joy_xout", "vcoa_fm", (0.80, 0.50, 0.02), 2.9), ("joy_yout", "d3_shin", (0.70, 0.04, 0.03), 3.5),
          ("lfoa_out", "d3_shclk", (0.02, 0.15, 0.60), 2.3))
LED_COL = {"amber": (1.0, 0.42, 0.04), "red": (1.0, 0.05, 0.02), "blue": (0.08, 0.32, 1.0), "green": (0.15, 1.0, 0.08)}


def fs(n): return n / 100.0                     # f-stop real → el de la escena (1 unidad = 10 mm, cien veces más grande)


def band_envelopes(path, fps=24):
    """Envolvente por cuadro de cuatro bandas del WAV (0..1, suavizada ~0,45 s y normalizada a su propio rango):
    filmkit.envelope da el golpe general, esto da la respiración lenta de cada parte del drone."""
    import numpy as np, wave
    w = wave.open(path); sr = w.getframerate(); ch = w.getnchannels(); n = w.getnframes()
    if w.getsampwidth() != 2: raise ValueError("WAV de 16 bits")
    x = np.frombuffer(w.readframes(n), dtype=np.int16).reshape(-1, ch).mean(1) / 32768.0; w.close()
    win = 8192; hop = sr / fps; xp = np.pad(x, (win // 2, win // 2)); han = np.hanning(win)
    S = np.array([np.abs(np.fft.rfft(xp[int(i * hop):int(i * hop) + win] * han)) ** 2 for i in range(int(len(x) / hop))])
    f = np.fft.rfftfreq(win, 1 / sr); k = np.hanning(11); k /= k.sum()
    out = []
    for a, b in BANDS:
        e = 10 * np.log10(S[:, (f >= a) & (f < b)].sum(1) + 1e-12)
        e = np.convolve(np.pad(e, 5, mode="edge"), k, "valid")
        lo, hi = np.percentile(e, 5), np.percentile(e, 98)
        out.append([float(v) for v in np.clip((e - lo) / max(1e-6, hi - lo), 0, 1)])
    return out


class Drone:
    """Estado del instrumento en función del cuadro g del audio (g/24 s): orbes, medidores, placas, LEDs."""

    def __init__(self, wav, fk):
        self.fk = fk; self.b = band_envelopes(wav); self.env = fk.envelope(wav)
        m = self.b[2]; pk = []
        for i in range(2, len(m) - 2):                    # crestas del oleaje (banda media)
            if m[i] == max(m[i - 2:i + 3]) and m[i] > 0.45 and (not pk or i - pk[-1] >= 20): pk.append(i)
        self.crests = pk
        ev = []                                           # cada cresta dispara un arpegio de cuatro placas
        for c in pk:
            for j in range(4): ev.append((c - 5 + int(round(j * 8.6)), 1.0 if j == 0 else 0.55))
        self.events = sorted(ev)

    def _at(self, a, g): return a[max(0, min(len(a) - 1, g))]
    def breath(self, g): return self._at(self.b[2], g)
    def ignite(self, g): return self.fk.rack(g / 24.0, 0.25, 2.4, 0.0, 1.0)
    def studio(self, g): return self.fk.rack(g / 24.0, 0.1, 2.9, 0.12, 1.0)

    def orb(self, i, g):
        return self.ignite(g) * ORB_LEVEL[i] * (0.03 + 0.97 * (0.6 * self.breath(g) + 0.4 * self._at(self.b[i], g)) ** 2.0)

    def plates(self, g):
        res = [0.0] * 12
        for k, (e, amp) in enumerate(self.events):
            if e > g: break
            dt = (g - e) / 24.0
            v = amp * min(1.0, dt / 0.12) * math.exp(-max(0.0, dt - 0.12) / 0.45)
            i = PHRASE[k % len(PHRASE)]; res[i] = max(res[i], v)
        return res

    def step(self, g):                                    # paso del secuenciador de 5 pasos (avanza con cada evento)
        n = sum(1 for (e, _) in self.events if e <= g)
        return (n - 1) % 5 if n else -1

    def joy(self, g):                                     # el joystick dibuja una figura lenta (Lissajous)
        T = g / 24.0
        return math.sin(2 * math.pi * T / 4.4), 0.8 * math.sin(2 * math.pi * T / 3.1 + 0.8)


def glow_material(base, name, emit, maxs, facing=0.0):
    """Copia de un material con emisión gobernada por un nodo Valor "Glow" (0..1, se graba por cuadro).
    facing > 0: más intensa en el centro de la cúpula que en el borde (el núcleo encendido de un orbe)."""
    m = base.copy(); m.name = name; nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    b.inputs["Emission Color"].default_value = (*emit, 1)
    v = nt.nodes.new("ShaderNodeValue"); v.name = "Glow"; v.outputs[0].default_value = 0.0
    mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"; mul.inputs[1].default_value = maxs
    nt.links.new(v.outputs[0], mul.inputs[0])
    if facing > 0:
        lw = nt.nodes.new("ShaderNodeLayerWeight"); lw.inputs["Blend"].default_value = 0.5
        mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs["To Min"].default_value = 1.0; mr.inputs["To Max"].default_value = 1.0 - facing
        nt.links.new(lw.outputs["Facing"], mr.inputs["Value"])
        m2 = nt.nodes.new("ShaderNodeMath"); m2.operation = "MULTIPLY"
        nt.links.new(mul.outputs[0], m2.inputs[0]); nt.links.new(mr.outputs["Result"], m2.inputs[1]); mul = m2
    nt.links.new(mul.outputs[0], b.inputs["Emission Strength"])
    return m, v


def set_glow(v, x):
    v.outputs[0].default_value = x; v.outputs[0].keyframe_insert("default_value")


def numbered(prefix):
    """Objetos "Prefijo", "Prefijo.001"… en orden de creación (el mismo orden del layout)."""
    import re
    rx = re.compile("^%s(\\.\\d+)?$" % prefix)
    return sorted([o for o in bpy.data.objects if rx.match(o.name)], key=lambda o: o.name)


def plug_and_cable(a, b, col, height):
    """Ficha de 3,5 mm en cada jack y el cable entre las dos, que sube en arco y cae. El cable cuelga de un vacío
    sobre la línea entre las fichas: girarlo hace que el arco se balancee sin mover las puntas."""
    body = mat("PlugBody", col, 0.35, coat=0.3); rubber = mat("Cable", tuple(c * 0.85 for c in col), 0.48, coat=0.1)
    tops = []
    for j in (a, b):
        pl = mesh("Plug", lathe([(3.4, 0.0), (3.4, 1.2), (3.0, 1.6), (3.0, 9.0), (2.4, 10.2), (1.6, 11.0), (0.0, 11.2)], 48,
                                radial=lambda t, rr, z: rr - (0.12 if (1.8 < z < 8.5 and math.cos(t * 20) > 0) else 0)), body)
        at(pl, j["x"], j["y"], 3.0); tops.append(P(j["x"], j["y"], 13.6))
    A, B = tops; mid = (A + B) / 2; d = (B - A)
    q0 = d.normalized().to_track_quat("X", "Z")
    piv = bpy.data.objects.new("CablePivot", None); link(piv); piv.location = mid; piv.rotation_euler = q0.to_euler()
    inv = (Matrix.Translation(mid) @ q0.to_matrix().to_4x4()).inverted()
    up = Vector((0, 0, 1)); h = height
    pts = [(A, A + up * h * 0.55), (mid + up * h, None), (B, B + up * h * 0.55)]
    cu = bpy.data.curves.new("Cable", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = 0.16; cu.bevel_resolution = 4
    cu.resolution_u = 24; sp = cu.splines.new("BEZIER"); sp.bezier_points.add(2)
    bp = sp.bezier_points
    bp[0].co = inv @ A; bp[0].handle_left = inv @ (A - up * 0.5); bp[0].handle_right = inv @ (A + up * h * 0.6)
    bp[2].co = inv @ B; bp[2].handle_right = inv @ (B - up * 0.5); bp[2].handle_left = inv @ (B + up * h * 0.6)
    hd = d.normalized() * d.length * 0.30
    bp[1].co = inv @ (mid + up * h); bp[1].handle_left = inv @ (mid + up * h - hd); bp[1].handle_right = inv @ (mid + up * h + hd)
    o = bpy.data.objects.new("Cable", cu); link(o); o.data.materials.append(rubber); o.parent = piv
    return piv, q0


def film_build(drone):
    """El instrumento del film: el de las fotos (preset Cathedral of A) con los orbes, medidores, LEDs, placas y
    pantalla con emisión animable, el joystick articulado, cables en el patch y la luz de estudio del film."""
    preset = PRESET or os.path.join(HERE, "..", "..", "assets", "presets", "01_cathedral_of_a.json")
    build_static("cream"); populate(preset, "cream"); stage()
    sc = bpy.context.scene
    for o in list(bpy.data.objects):                    # luces de las fotos fuera: el film tiene las suyas
        if o.type == "LIGHT" or o.name.startswith("OrbOn") or o.name == "Orb" or o.name.startswith("Orb."):
            bpy.data.objects.remove(o)
    R = dict(orbs=[], segs=[], leds=[], plates=[], halos=[], cables=[])
    # Orbes: cúpula de acrílico esmerilado (SSS) con el núcleo encendido.
    orbm = mat("OrbFilm", (0.58, 0.42, 0.33), 0.32, coat=0.6, sss=1.0)
    orbm.node_tree.nodes["Principled BSDF"].inputs["Subsurface Scale"].default_value = 0.35
    orbm.node_tree.nodes["Principled BSDF"].inputs["Subsurface Radius"].default_value = (1.0, 0.45, 0.25)
    for i, ob in enumerate(L["orbs"]):
        r = ob["d"] / 2
        prof = [(r * math.cos(a), 1.6 + r * 0.55 * math.sin(a)) for a in [k * math.pi / 2 / 20 for k in range(21)]]
        m, v = glow_material(orbm, "OrbGlow%d" % i, ORB_COL[i], 7.5, facing=0.7)
        dome = mesh("OrbDome", lathe(prof, 96), m); at(dome, ob["x"], ob["y"]); R["orbs"].append(v)
        dome.visible_shadow = False                              # deja pasar la luz del LED de adentro
        pl = bpy.data.lights.new("OrbLed%d" % i, "POINT"); pl.shadow_soft_size = 0.7; pl.color = (1.0, 0.36, 0.12); pl.energy = 0.0
        po = bpy.data.objects.new("OrbLed%d" % i, pl); link(po); po.location = P(ob["x"], ob["y"], 3.0)
        R.setdefault("orbled", []).append(pl)
    # Medidores de barra: cinco segmentos por drone, de ámbar a rojo.
    for s, o in enumerate(numbered("Seg")):
        col = (1.0, 0.42, 0.05) if s % 5 < 4 else (1.0, 0.08, 0.02)
        m, v = glow_material(o.data.materials[0], "SegGlow%d" % s, col, 9.0); o.data.materials[0] = m; R["segs"].append(v)
    for l, o in zip(L["leds"], numbered("Led")):
        m, v = glow_material(o.data.materials[0], "LedGlow_" + l["id"], LED_COL[l["color"]], 14.0)
        o.data.materials[0] = m; R["leds"].append((l["id"], v))
    # Placas táctiles: un halo de luz por debajo (asoma 1,4 mm alrededor) y la placa que se entibia al tocarla.
    for p, o in zip(L["plates"], numbered("Plate")):
        m, v = glow_material(o.data.materials[0], "PlateGlow%d" % p["i"], (1.0, 0.72, 0.45), 0.9)
        o.data.materials[0] = m
        x0, y0, x1, y1 = p["x"] - p["w"] / 2, p["y"] - p["h"] / 2, p["x"] + p["w"] / 2, p["y"] + p["h"] / 2
        hm, hv = glow_material(M["blackpanel"], "HaloGlow%d" % p["i"], (1.0, 0.50, 0.18), 22.0)
        box("Halo", x0 - 1.4, y0 - 1.4, x1 + 1.4, y1 + 1.4, 1.0, 1.12, hm, 0.4)
        R["plates"].append(v); R["halos"].append(hv)
    disp = numbered("Display")[0]
    m, R["display"] = glow_material(disp.data.materials[0], "DisplayGlow", (0.10, 0.62, 0.85), 3.0); disp.data.materials[0] = m
    # Teclas drone 1 y 2 apretadas (el preset las tiene sostenidas): bajan 1,8 mm y su ventanita se enciende.
    keys, wins = numbered("DKey"), numbered("DKeyWin")
    kwm, kwv = glow_material(M["window"], "DKeyGlow", (1.0, 0.10, 0.03), 7.0)
    for k, o, w in zip(L["dronekeys"], keys, wins):
        if k["key"] in (1, 2):
            o.location.z -= 0.18; w.location.z -= 0.18; w.data.materials[0] = kwm
    R["dkey"] = kwv
    # Joystick: capuchón y vástago cuelgan de un pivote en la base.
    j = L["joystick"]
    piv = bpy.data.objects.new("JoyPivot", None); link(piv); piv.location = P(j["x"], j["y"], 2.0)
    for o in (JOY["cap"], JOY["shaft"]):
        o.location = o.location - piv.location; o.parent = piv
    R["joy"] = piv
    jk = {x["id"]: x for x in L["jacks"]}
    for a, b, col, h in CABLES:
        R["cables"].append(plug_and_cable(jk[a], jk[b], col, h))
    # Estudio oscuro: llave cálida arriba a la izquierda, relleno frío y amplio, contraluz que recorta perillas,
    # y una franja de luz que barre el panel en algunas tomas. Potencias por irradiancia (escena ×100).
    C = Vector((24.75, -16.0, 0.0))
    def area(name, loc, size, E, col, sy=None, look=C):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.color = col
        if sy: l.shape = "RECTANGLE"; l.size_y = sy
        o = bpy.data.objects.new(name, l); link(o); o.location = Vector(loc)
        o.rotation_euler = (Vector(look) - o.location).to_track_quat("-Z", "Y").to_euler()
        o["E"] = E * math.pi * (Vector(look) - o.location).length_squared; l.energy = o["E"]
        o.visible_camera = False                                # la fuente no se ve en cuadro, sólo su luz
        return o
    R["key"] = area("FKey", (-16, -64, 56), 40, 1.9, (1.0, 0.94, 0.86))
    R["fill"] = area("FFill", (80, -46, 20), 70, 0.35, (0.80, 0.87, 1.0))
    R["rim"] = area("FRim", (34, 48, 26), 50, 1.4, (1.0, 0.97, 0.94))
    R["sweep"] = area("FSweep", (0, 10, 20), 2.5, 0.0, (1.0, 0.92, 0.82), sy=80, look=(0, -20, 0))
    sc.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.012
    fl = bpy.data.objects["Floor"].data.materials[0].node_tree.nodes["Principled BSDF"]
    fl.inputs["Roughness"].default_value = 0.72                    # el piso refleja apenas el brillo de los orbes
    return sc, R


# Perillas que se mueven: (amplitud °, período s, fase, cuánto siguen la respiración °).
KNOB_DRIFT = {"d1_tune2": (14, 7.0, 0.3, 6), "d1_tune4": (10, 9.0, 1.9, 4), "d1_volt": (8, 11, 0.7, 10), "d1_cv": (12, 8.0, 2.4, 6),
              "d2_tune3": (16, 6.5, 1.1, 6), "d2_tune5": (10, 8.5, 2.9, 4), "d2_volt": (9, 10, 0.2, 10), "d2_cv": (12, 7.5, 1.6, 6),
              "d2_att": (6, 12, 0.9, 4), "d1_rls": (6, 13, 2.2, 4), "vcoa_pw": (14, 7.0, 0.4, 0), "vcoa_shape": (10, 9.0, 2.0, 0),
              "fx_x": (18, 9.0, 0.5, 6), "fx_y": (14, 7.0, 2.6, 6), "fx_blend": (8, 11, 1.2, 0), "master": (4, 14, 0.0, 3)}


def knob_angle(kid, base, T, br):
    from filmkit import ease
    if kid == "vcoa_tune": return base - 40 + 80 * ease((T - 8.5) / 3.8, "smooth")
    if kid == "vcoa_fm": return base + 25 - 55 * ease((T - 9.2) / 3.0, "smooth")
    a, per, ph, eb = KNOB_DRIFT[kid]
    return base + a * math.sin(2 * math.pi * T / per + ph) + eb * (br - 0.5)


def film2():
    """Film de la web del AT42 (≈ 24 s, audio cathedral-a: drones 1 y 2 sostenidos, un oleaje de ~1,43 s).
    El drone respira: los orbes laten con su banda del audio, los medidores suben y bajan, las perillas giran despacio.

      open   0,00–4,71  Estudio a oscuras; el orbe del drone 2 se enciende y empieza a respirar, su luz cálida baña las
                        perillas vecinas mientras la llave sube. Macro bajo de frente-derecha, empuje lento, 65 mm f/2,
                        foco en la cúpula; abajo a la izquierda queda el panel en sombra para el título.
      orbs   4,71–8,96  Macro rasante a lo largo de la fila de orbes: de cerca el orbe 1, al fondo el 2; traveling lateral
                        lento y rack focus de uno al otro. Laten cada uno con su banda; los medidores de barra acompañan.
      knobs  8,96–11,92 Primer plano de las perillas verdes del VCO A: TUNE gira 80°, FM vuelve 55°; una franja de luz
                        cruza las tapas. Arco corto, rack focus de TUNE a WAVE.
      touch 11,92–14,62 Desde arriba en 3/4, traveling por el teclado táctil: las placas se encienden en arpegio con cada
                        cresta del drone (halo ámbar debajo), la pantalla late y el secuenciador de 5 pasos avanza.
      joy   14,62–17,54 Contrapicado al joystick que dibuja una figura lenta; los LEDs X/Y lo siguen y los cables del
                        patch se mecen detrás. Empuje lento, 60 mm.
      wide  17,54–20,46 Revelado: la cámara sube desde el ras del panel y descubre la máquina entera con los cuatro orbes
                        respirando; la franja de luz barre el panel de izquierda a derecha. 35 mm.
      end   20,46–24,00 (fundido) Vista alta de frente que se aleja despacio; film.py la oscurece y pone el cierre.
    """
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final"); probe = arg("--probe")
    drone = Drone(arg("--audio"), fk)
    print("crestas", [round(c / 24, 2) for c in drone.crests], flush=True)
    jd = L["joystick"]
    for (name, f0, frames) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        KN.clear(); JOY.clear()
        sc, R = film_build(drone)
        fk.setup(sc, quality); sc.render.use_persistent_data = True
        cam = fk.camera(sc); cam.data.clip_end = 3000
        O = [P(o["x"], o["y"], 5.0) for o in L["orbs"]]            # centro de cada cúpula
        K = {k: P(KN[k][0].location.x * 10, -KN[k][0].location.y * 10, 9.0) for k in ("vcoa_tune", "vcoa_fm", "vcoa_wave")}
        JC = P(jd["x"], jd["y"], 10.0)
        paths = {
            "open":  fk.Path([(0, O[1] + Vector((5.6, -12.8, 2.9)), O[1] + Vector((-0.9, -1.2, -0.35)), 65, fs(3.2)),
                              (1, O[1] + Vector((4.3, -10.0, 2.3)), O[1] + Vector((-0.8, -1.0, -0.3)), 65, fs(3.2))], easing="out"),
            "orbs":  fk.Path([(0, O[0] + Vector((-9.5, -4.8, 1.9)), O[0] + Vector((4.0, 0.0, -0.1)), 60, fs(4.0)),
                              (1, O[0] + Vector((-7.0, -5.6, 1.8)), O[0] + Vector((5.5, 0.0, -0.1)), 60, fs(4.0))]),
            "knobs": fk.Path([(0, K["vcoa_tune"] + Vector((5.5, -8.0, 3.2)), K["vcoa_tune"] + Vector((-2.2, 0.0, -0.4)), 70, fs(2.4)),
                              (1, K["vcoa_tune"] + Vector((3.2, -8.8, 2.8)), K["vcoa_tune"] + Vector((-2.6, -0.6, -0.4)), 70, fs(2.4))]),
            "touch": fk.Path([(0, (9.0, -42.0, 14.0), (15.0, -26.4, 0.0), 50, fs(4.0)),
                              (1, (19.0, -42.5, 13.0), (25.0, -26.4, 0.0), 50, fs(4.0))], easing="linear"),
            "joy":   fk.Path([(0, JC + Vector((5.4, -10.2, 3.6)), JC + Vector((0.3, 2.6, -0.9)), 60, fs(3.5)),
                              (1, JC + Vector((4.0, -7.6, 2.9)), JC + Vector((0.3, 2.6, -0.8)), 60, fs(3.5))], easing="out"),
            "wide":  fk.Path([(0, (32.0, -74.0, 7.0), (25.5, -14.0, 0.0), 35, fs(5.6)),
                              (0.5, (26.0, -72.0, 13.0), (25.0, -14.0, 0.0), 35, fs(5.6)),
                              (1, (17.0, -66.0, 22.0), (24.0, -15.0, 0.0), 35, fs(5.6))]),
            "end":   fk.Path([(0, (24.75, -56.0, 38.0), (24.75, -15.5, 0.0), 40, fs(6.0)),
                              (1, (24.75, -70.0, 50.0), (24.75, -15.5, 0.0), 40, fs(6.0))], easing="out"),
        }
        path = paths[name]

        def pose(f, t, u, name=name, path=path, f0=f0):
            g = f0 + f - 1; T = g / 24.0; br = drone.breath(g)
            moved = []
            for kid, (o, base) in KN.items():
                if kid in KNOB_DRIFT or kid in ("vcoa_tune", "vcoa_fm"):
                    o.rotation_euler.z = -math.radians(knob_angle(kid, base, T, br)); moved.append(o)
            jx, jy = drone.joy(g)
            R["joy"].rotation_euler = (math.radians(-15 * jy), math.radians(15 * jx), 0); moved.append(R["joy"])
            for (piv, q0), seed in zip(R["cables"], (3, 5, 7)):
                th = math.radians(2.5 * fk.noise1(T, seed, 0.3) + 2.0 * (br - 0.5))
                piv.rotation_euler = (q0 @ Quaternion((1, 0, 0), th)).to_euler(); moved.append(piv)
            focus = None
            if name == "open": focus = O[1] + Vector((0, 0, 0.2))
            if name == "orbs": focus = O[0].lerp(O[1], fk.rack(u, 0.3, 0.72, 0.0, 1.0))
            if name == "knobs": focus = K["vcoa_tune"].lerp(K["vcoa_wave"], fk.rack(u, 0.45, 0.85, 0.0, 1.0))
            if name == "joy": focus = JC
            shake = {"open": 0.10, "orbs": 0.08, "knobs": 0.10, "joy": 0.08}.get(name, 0.0)
            path.apply(cam, u, focus=focus, shake=shake, t=T)
            return moved

        def keys(f, name=name, f0=f0, frames=frames):
            g = f0 + f - 1; T = g / 24.0; u = (f - 1) / max(1, frames - 1); br = drone.breath(g)
            for i, v in enumerate(R["orbs"]):
                o = drone.orb(i, g); set_glow(v, o)
                R["orbled"][i].energy = 200.0 * o; R["orbled"][i].keyframe_insert("energy")
            for s, v in enumerate(R["segs"]):
                set_glow(v, max(0.0, min(1.0, drone.orb(s // 5, g) * 5.4 - s % 5)))
            pl = drone.plates(g)
            for i in range(12): set_glow(R["plates"][i], pl[i]); set_glow(R["halos"][i], pl[i])
            set_glow(R["display"], drone.ignite(g) * (0.35 + 0.25 * br + 0.4 * max(pl)))
            set_glow(R["dkey"], drone.ignite(g))
            jx, jy = drone.joy(g); st = drone.step(g); ig = drone.ignite(g)
            for lid, v in R["leds"]:
                x = 0.0
                if lid in ("d1_gate", "d2_gate"): x = ig
                elif lid in ("d4_gate", "d5_gate", "ega", "egb", "ef_env"): x = ig * br
                elif lid.startswith("seq_step"): x = ig * (1.0 if int(lid[-1]) - 1 == st else 0.0)
                elif lid == "joy_x": x = ig * abs(jx)
                elif lid == "joy_y": x = ig * abs(jy)
                elif lid in ("lfoa", "lfob"): x = ig * (0.5 + 0.5 * math.sin(2 * math.pi * T * 0.7 + (0 if lid == "lfoa" else 2)))
                elif lid in ("d3_sh", "d6_sh"): x = ig * (1.0 if drone.step(g) % 2 == (lid == "d6_sh") else 0.0)
                elif lid == "ef_gate": x = ig * (1.0 if br > 0.55 else 0.0)
                set_glow(v, x)
            s = drone.studio(g)
            for k in ("key", "fill", "rim"):
                R[k].data.energy = R[k]["E"] * s; R[k].data.keyframe_insert("energy")
            sw = R["sweep"]                                        # la franja cruza el panel en knobs y wide
            if name in ("knobs", "wide"):
                x0, x1 = {"knobs": (6.0, 20.0), "wide": (-4.0, 54.0)}[name]
                sw.location.x = x0 + (x1 - x0) * fk.ease(u, "inout")
                sw.data.energy = 1.0 * math.pi * 30.0 ** 2 * math.sin(math.pi * min(1.0, max(0.0, u))) ** 0.7
            else:
                sw.data.energy = 0.0
            sw.keyframe_insert("location"); sw.data.keyframe_insert("energy")

        if probe:                                                  # cuadros sueltos para ajustar encuadre y luz
            for fr in [max(1, min(frames, int(round(float(x) * (frames - 1))) + 1)) for x in probe.split(",")]:
                sc.frame_set(fr); t, u = (fr - 1) / 24.0, (fr - 1) / max(1, frames - 1)
                pose(fr, t, u); keys(fr); bpy.context.view_layer.update()
                sc.render.filepath = os.path.join(OUT, f"{name}_p{fr:04d}.jpg"); bpy.ops.render.render(write_still=True)
            continue
        fk.render_shot(sc, name, frames, pose, OUT, extra_keys=keys)

if __name__ == "__main__":
    modes = {"bg": mode_bg, "sprites": mode_sprites, "preview": mode_preview, "hero": mode_hero, "anim": mode_anim, "film": mode_film, "film2": film2}
    for m in arg("--mode", "bg,sprites").split(","):
        print("== modo", m, flush=True); modes[m]()
