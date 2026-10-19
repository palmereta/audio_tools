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
from mathutils import Vector, Euler, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d

L = json.load(open(arg("--layout", "layout42.json")))
SILK = os.path.abspath(arg("--silk", "silkscreen42.png"))
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
def populate(preset_path, color):
    pv = json.load(open(preset_path))["panel"] if preset_path else {}
    for k in L["knobs"]:
        v = pv.get(k["id"], 0.5)
        o = knob_obj(k["size"], k["color"])
        t = (v / max(1, k["steps"] - 1)) if k["steps"] > 0 else v
        o.rotation_euler.z = -math.radians(k["a0"] + (k["a1"] - k["a0"]) * t)
        at(o, k["x"], k["y"], 1.0 if k["id"] == "kb_enc" else 0.0)
    for t in L["toggles"]:
        i = int(round(pv.get(t["id"], 1 if t["positions"] == 3 else 0)))
        bat = mesh("Bat", lathe([(1.0, 0.0), (1.0, 5.5), (1.4, 7.6), (1.1, 8.8), (0.0, 9.0)], 32), M["chrome"])
        ang = (30, 0, -30)[i] if t["positions"] == 3 else (30 if i == 0 else -30)
        bat.rotation_euler = Euler((math.radians(ang), 0, 0), "XYZ"); at(bat, t["x"], t["y"], 3.6)
    j = L["joystick"]
    cap = mesh("JCap", lathe([(5.05, 0.0), (5.05, 2.6), (4.4, 3.6), (0.0, 3.9)], 64), M["skirt"]); at(cap, j["x"], j["y"], 9.0)
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

if __name__ == "__main__":
    modes = {"bg": mode_bg, "sprites": mode_sprites, "preview": mode_preview, "hero": mode_hero, "anim": mode_anim, "film": mode_film}
    for m in arg("--mode", "bg,sprites").split(","):
        print("== modo", m, flush=True); modes[m]()
