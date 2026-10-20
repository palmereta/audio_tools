"""Escena Blender del AT700: los tres gabinetes en vista cenital (fondo del editor), sprites de los
controles y vistas en perspectiva para la web.

Corre en pc50-wsl:
  blender -b -P scene700.py -- --layout layout700.json --silk silkscreen700.png --out out700 --mode bg,sprites --samples 160
  blender -b -P scene700.py -- --mode hero --preset preset.json --out out700
  blender -b -P scene700.py -- --mode film --shot wide --frames 120 --preset preset.json --out out700

1 unidad de Blender = 10 mm. Punto del lienzo (x, y) mm → (x/10, −y/10, z/10).
Cámara ortográfica inclinada THETA: en pantalla sx = x − x0, sy = (y − y0)·cosθ − z·sinθ (mm).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Euler, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d

HERE = os.path.dirname(os.path.abspath(__file__))
def asset(flag, name, sub=""):
    """Ruta de un insumo: la del argumento; si no, la del directorio actual (pc50: ~/ai/at700); si no, la del repo."""
    if arg(flag): return os.path.abspath(arg(flag))
    if os.path.exists(name): return os.path.abspath(name)
    return os.path.join(HERE, "..", "..", "assets", sub, name)
L = json.load(open(asset("--layout", "layout700.json")))
SILK = asset("--silk", "silkscreen700.png", "render")
VUFACE = asset("--vu", "vu_face.png", "render")
OUT = os.path.abspath(arg("--out", "out700"))
SAMPLES = int(arg("--samples", "160"))
os.makedirs(OUT, exist_ok=True)

THETA = math.radians(8.0)
PXMM = float(arg("--pxmm", "4.0"))
W, H = L["size"]
CAB = L["cabinets"]
KSIZE = {"KS": 12.5, "KM": 15.0, "KL": 21.0, "KP": 25.0, "KF": 9.5, "KQ": 12.5}

def P(x, y, z=0.0): return Vector((x / 10.0, -y / 10.0, z / 10.0))

# ───────── escena ─────────
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    try:
        prefs.compute_device_type = "CUDA"; prefs.get_devices()        # OptiX no inicia en WSL
        for d in prefs.devices: d.use = d.type == "CUDA"
    except Exception: pass
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    if hasattr(sc.cycles, "denoising_use_gpu"): sc.cycles.denoising_use_gpu = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.6, 0.62, 0.66, 1); bg.inputs["Strength"].default_value = 0.1
    return sc

def lights(scale=1.0):
    def sun(name, rot, e, ang, col):
        l = bpy.data.lights.new(name, "SUN"); l.energy = e * scale; l.angle = math.radians(ang); l.color = col
        o = bpy.data.objects.new(name, l); bpy.context.collection.objects.link(o); o.rotation_euler = Euler(rot, "XYZ")
    sun("Key", (math.radians(34), math.radians(-22), math.radians(-18)), 3.0, 18, (1.0, 0.97, 0.93))
    sun("Fill", (math.radians(-18), math.radians(28), 0), 0.7, 40, (0.85, 0.9, 1.0))
    sun("Rim", (math.radians(-58), 0, math.radians(12)), 0.5, 10, (1, 1, 1))

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
def mat(name, col, rough, metal=0.0, coat=0.0, spec=0.5, emit=None, aniso=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    if aniso and "Anisotropic" in b.inputs: b.inputs["Anisotropic"].default_value = aniso
    if emit:
        b.inputs["Emission Color"].default_value = (*emit[0], 1); b.inputs["Emission Strength"].default_value = emit[1]
    return m

def panel_material():
    """Aluminio anodizado negro con serigrafía blanca (la textura es una máscara de tinta)."""
    m = bpy.data.materials.new("Panel"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(SILK); tex.image.colorspace_settings.name = "Non-Color"
    tex.extension = "CLIP"; tex.interpolation = "Cubic"
    uv = N.new("ShaderNodeTexCoord"); Lk.new(uv.outputs["UV"], tex.inputs["Vector"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 3000.0; nz.inputs["Detail"].default_value = 4.0
    Lk.new(uv.outputs["Object"], nz.inputs["Vector"])
    mix = N.new("ShaderNodeMix"); mix.data_type = "RGBA"
    Lk.new(tex.outputs["Color"], mix.inputs["Factor"])
    mix.inputs["A"].default_value = (0.0075, 0.0075, 0.0082, 1); mix.inputs["B"].default_value = (0.78, 0.78, 0.76, 1)
    Lk.new(mix.outputs["Result"], b.inputs["Base Color"])
    r = N.new("ShaderNodeMapRange"); Lk.new(nz.outputs["Fac"], r.inputs["Value"])
    r.inputs["To Min"].default_value = 0.36; r.inputs["To Max"].default_value = 0.5
    rm = N.new("ShaderNodeMix"); rm.data_type = "FLOAT"
    Lk.new(tex.outputs["Color"], rm.inputs["Factor"]); Lk.new(r.outputs["Result"], rm.inputs["A"]); rm.inputs["B"].default_value = 0.62
    Lk.new(rm.outputs["Result"], b.inputs["Roughness"])
    b.inputs["Metallic"].default_value = 0.25
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08; bump.inputs["Distance"].default_value = 0.001
    Lk.new(nz.outputs["Fac"], bump.inputs["Height"]); Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = 0.5
    return m

def tolex_material():
    m = bpy.data.materials.new("Tolex"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]; b.inputs["Base Color"].default_value = (0.012, 0.012, 0.012, 1); b.inputs["Roughness"].default_value = 0.72
    uv = N.new("ShaderNodeTexCoord")
    v = N.new("ShaderNodeTexVoronoi"); v.inputs["Scale"].default_value = 260.0; Lk.new(uv.outputs["Object"], v.inputs["Vector"])
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.35; bump.inputs["Distance"].default_value = 0.003
    Lk.new(v.outputs["Distance"], bump.inputs["Height"]); Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

def vu_material():
    m = bpy.data.materials.new("VU"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tex = N.new("ShaderNodeTexImage")
    if os.path.exists(VUFACE): tex.image = bpy.data.images.load(VUFACE)
    uv = N.new("ShaderNodeTexCoord"); Lk.new(uv.outputs["UV"], tex.inputs["Vector"])
    Lk.new(tex.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.5
    if "Emission Color" in b.inputs:
        Lk.new(tex.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.08
    return m

M = {}
def materials():
    M["panel"] = panel_material()
    M["tolex"] = tolex_material()
    M["vu"] = vu_material()
    M["back"] = mat("Back", (0.004, 0.004, 0.004), 0.8)
    M["slot"] = mat("Slot", (0.002, 0.002, 0.002), 0.9)
    M["nut"] = mat("Nut", (0.62, 0.62, 0.63), 0.28, metal=1.0)
    M["swplate"] = mat("SwPlate", (0.08, 0.08, 0.085), 0.35, metal=0.8)
    M["chrome"] = mat("Chrome", (0.86, 0.86, 0.86), 0.1, metal=1.0)
    M["corner"] = mat("Corner", (0.75, 0.75, 0.76), 0.18, metal=1.0)
    M["hole"] = mat("Hole", (0.003, 0.003, 0.003), 0.9)
    M["skirt"] = mat("Skirt", (0.02, 0.02, 0.021), 0.4)
    M["cap"] = mat("Cap", (0.70, 0.70, 0.71), 0.22, metal=1.0, aniso=0.6)
    M["pointer_w"] = mat("PointerW", (0.85, 0.85, 0.85), 0.4)
    M["pointer_k"] = mat("PointerK", (0.01, 0.01, 0.01), 0.4)
    M["fader"] = mat("Fader", (0.015, 0.015, 0.016), 0.45)
    M["cream"] = mat("Cream", (0.75, 0.70, 0.58), 0.35, coat=0.3)
    M["glass"] = mat("Glass", (0.9, 0.9, 0.9), 0.05, coat=1.0)
    M["white_key"] = mat("WhiteKey", (0.78, 0.77, 0.73), 0.32, coat=0.4)
    M["black_key"] = mat("BlackKey", (0.012, 0.012, 0.012), 0.28, coat=0.5)
    M["kbcase"] = mat("KbCase", (0.02, 0.02, 0.022), 0.5, metal=0.2)
    M["felt"] = mat("Felt", (0.12, 0.02, 0.02), 0.9)
    for k, c in (("red", (0.62, 0.03, 0.02)), ("white", (0.78, 0.77, 0.74)), ("orange", (0.85, 0.22, 0.03))):
        M["btn_" + k] = mat("Btn" + k, c, 0.38, coat=0.3)
    for k, c in (("red", (0.25, 0.02, 0.01)), ("green", (0.03, 0.14, 0.02))):
        M["led_" + k] = mat("Led" + k, c, 0.12, coat=0.8)

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

def textured_plane(name, x0, y0, x1, y1, z, m):
    """Plano con UV en coordenadas del lienzo (la serigrafía cubre todo el lienzo)."""
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    vs = [bm.verts.new(P(x, y, z)) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
    f = bm.faces.new(vs)
    for lp in f.loops:
        co = lp.vert.co; lp[uvl].uv = (co.x * 10 / W, 1.0 + co.y * 10 / H)
    return mesh(name, bm, m, smooth=False)

def jack(x, y):
    nut = mesh("Nut", lathe([(5.4, 0.0), (5.4, 1.8), (4.9, 2.3), (0.0, 2.3)], 6), M["nut"]); at(nut, x, y)
    nut.rotation_euler.z = math.radians(30)
    ring = mesh("Ring", lathe([(4.3, 2.3), (4.3, 3.3), (3.7, 3.7), (3.3, 3.7), (3.25, 1.0), (0.0, 1.0)], 48), M["nut"]); at(ring, x, y)
    hole = mesh("Hole", lathe([(3.25, 1.2), (0.0, 1.2)], 32), M["hole"]); at(hole, x, y)

def slot(x0, y0, x1, y1):
    box("SlotFrame", x0 - 0.5, y0 - 0.5, x1 + 0.5, y1 + 0.5, -0.05, 0.12, M["skirt"], 0.3)
    box("Slot", x0, y0, x1, y1, -0.6, 0.14, M["slot"], 0.2)

def fader_slot(s):
    if s["horiz"]: slot(s["x0"] - 4.5, s["y"] - 1.3, s["x1"] + 4.5, s["y"] + 1.3)
    else: slot(s["x"] - 1.3, s["y0"] - 2.6, s["x"] + 1.3, s["y1"] + 2.6)

def switch_slot(s):
    l = s["length"]
    if s["orient"] == "v": box("SwPlate", s["x"] - 2.6, s["y"] - l / 2 - 1.2, s["x"] + 2.6, s["y"] + l / 2 + 1.2, 0.0, 0.35, M["swplate"], 0.3); slot(s["x"] - 1.4, s["y"] - l / 2, s["x"] + 1.4, s["y"] + l / 2)
    else: box("SwPlate", s["x"] - l / 2 - 1.2, s["y"] - 2.6, s["x"] + l / 2 + 1.2, s["y"] + 2.6, 0.0, 0.35, M["swplate"], 0.3); slot(s["x"] - l / 2, s["y"] - 1.4, s["x"] + l / 2, s["y"] + 1.4)

def lever_slot(lv):
    slot(lv["x"] - 1.6, lv["y0"] - 2.4, lv["x"] + 1.6, lv["y1"] + 2.4)

def toggle_base(t):
    m1 = mesh("Bush", lathe([(3.0, 0.0), (3.0, 0.8), (2.5, 1.1), (2.5, 2.5), (0.0, 2.5)], 6), M["nut"]); at(m1, t["x"], t["y"])
    m2 = mesh("Thread", lathe([(2.3, 0.0), (2.3, 3.3), (0.0, 3.3)], 32), M["chrome"]); at(m2, t["x"], t["y"])

def button_base(b):
    if b["shape"] == "square":
        box("BtnFrame", b["x"] - b["w"] / 2 - 1.2, b["y"] - b["h"] / 2 - 1.2, b["x"] + b["w"] / 2 + 1.2, b["y"] + b["h"] / 2 + 1.2, 0.0, 0.8, M["skirt"], 0.4)
        box("Btn", b["x"] - b["w"] / 2, b["y"] - b["h"] / 2, b["x"] + b["w"] / 2, b["y"] + b["h"] / 2, 0.0, 6.0, M["btn_" + b["color"]], 1.0)
    else:
        d = b["d"]
        bez = mesh("Bezel", lathe([(d / 2 + 1.3, 0.0), (d / 2 + 1.3, 0.8), (d / 2 + 0.4, 1.1), (0.0, 1.1)], 48), M["skirt"]); at(bez, b["x"], b["y"])
        cap = mesh("Btn", lathe([(d / 2, 0.0), (d / 2, 4.2), (d / 2 - 0.6, 4.9), (0.0, 5.1)], 48), M["btn_" + b["color"]]); at(cap, b["x"], b["y"])

def led(l):
    o = mesh("Led", lathe([(1.6, 0.0), (1.6, 0.6), (1.3, 1.3), (0.0, 1.6)], 32), M["led_" + l["color"]]); at(o, l["x"], l["y"])
    o["cid"] = l["id"]                                                  # el film enciende cada LED por su id
    r = mesh("LedRing", lathe([(2.2, 0.0), (2.2, 0.4), (1.7, 0.5), (0.0, 0.5)], 32), M["nut"]); at(r, l["x"], l["y"])

def screw(x, y, z=0.0):
    o = mesh("Screw", lathe([(1.7, 0.0), (1.7, 0.4), (1.4, 0.8), (0.0, 0.95)], 24), M["chrome"]); at(o, x, y, z)
    box("Cross1", x - 1.0, y - 0.18, x + 1.0, y + 0.18, z + 0.6, z + 1.0, M["hole"], 0.0)
    box("Cross2", x - 0.18, y - 1.0, x + 0.18, y + 1.0, z + 0.6, z + 1.0, M["hole"], 0.0)

def meter_body(m):
    x0, y0, x1, y1 = m["x"] - m["w"] / 2, m["y"] - m["h"] / 2, m["x"] + m["w"] / 2, m["y"] + m["h"] / 2
    for bx in ((x0 - 1.4, y0 - 1.4, x1 + 1.4, y0 + 0.6), (x0 - 1.4, y1 - 0.6, x1 + 1.4, y1 + 1.4),
               (x0 - 1.4, y0 - 1.4, x0 + 0.6, y1 + 1.4), (x1 - 0.6, y0 - 1.4, x1 + 1.4, y1 + 1.4)):
        box("MeterBezel", *bx, 0.0, 1.8, M["skirt"], 0.4)
    box("MeterBack", x0, y0, x1, y1, -1.0, 0.2, M["back"], 0.0)
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    vs = [bm.verts.new(P(x, y, 0.6)) for x, y in ((x0 + 0.5, y0 + 0.5), (x1 - 0.5, y0 + 0.5), (x1 - 0.5, y1 - 0.5), (x0 + 0.5, y1 - 0.5))]
    f = bm.faces.new(vs)
    for lp, uv in zip(f.loops, ((0, 1), (1, 1), (1, 0), (0, 0))): lp[uvl].uv = uv
    mesh("MeterFace", bm, M["vu"], smooth=False)

def corner(x, y, z, sx, sy):
    box("Corner", x, y, x + sx * 14.0, y + sy * 14.0, z, z + 2.0, M["corner"], 1.6)

def cabinet(c, depth=-60.0, lip=9.0, tray=None):
    x0, y0, x1, y1 = c["x0"], c["y0"], c["x1"], c["y1"]
    t = 9.0
    box("CabL", x0, y0, x0 + t, y1, depth, lip, M["tolex"], 1.6)
    box("CabR", x1 - t, y0, x1, y1, depth, lip, M["tolex"], 1.6)
    box("CabT", x0, y0, x1, y0 + t, depth, lip, M["tolex"], 1.6)
    box("CabB", x0, y1 - t, x1, y1, depth, lip, M["tolex"], 1.6)
    for (x, y, sx, sy) in ((x0 - 0.5, y0 - 0.5, 1, 1), (x1 + 0.5, y0 - 0.5, -1, 1), (x0 - 0.5, y1 + 0.5, 1, -1), (x1 + 0.5, y1 + 0.5, -1, -1)):
        corner(x, y, lip - 0.6, sx, sy)
    box("CabBack", x0 + t, y0 + t, x1 - t, y1 - t, depth, -2.5, M["back"], 0.0)

def build_static():
    reset(); materials(); lights()
    cabinet(CAB["seq"]); cabinet(CAB["console"])
    # Bandeja de la consola (hueco bajo las filas de módulos).
    T = CAB["tray"]
    box("TrayFloor", T["x0"], T["y0"] + 4.0, T["x1"], T["y1"], -34.0, -32.0, M["tolex"], 0.0)
    box("TrayLip", T["x0"], T["y0"], T["x1"], T["y0"] + 4.0, -32.0, 0.0, M["tolex"], 0.8)
    box("TrayFront", T["x0"], T["y1"] - 4.0, T["x1"], T["y1"], -32.0, -6.0, M["tolex"], 0.8)
    # Paneles: una placa por módulo, con una junta fina entre placas.
    for p in L["panels"]:
        x0, y0, x1, y1 = p["x0"] + 0.18, p["y0"] + 0.18, p["x1"] - 0.18, p["y1"] - 0.18
        box("PanelBody", x0, y0, x1, y1, -2.0, -0.02, M["panel"], 0.25)
        textured_plane("PanelTop", x0, y0, x1, y1, 0.0, M["panel"])
        sx = 4.2 if (p["x1"] - p["x0"]) > 40 else 3.6
        for (x, y) in ((x0 + sx, y0 + 3.4), (x1 - sx, y0 + 3.4), (x0 + sx, y1 - 3.4), (x1 - sx, y1 - 3.4)): screw(x, y)
    for j in L["jacks"]: jack(j["x"], j["y"])
    for s in L["sliders"]: fader_slot(s)
    for s in L["switches"]: switch_slot(s)
    for lv in L["levers"]: lever_slot(lv)
    for t in L["toggles"]: toggle_base(t)
    for b in L["buttons"]: button_base(b)
    for l in L["leds"]: led(l)
    for m in L["meters"]: meter_body(m)
    # Auriculares y DIN del 712A (decorativos).
    ph = L["phones"]
    mesh("PhonesNut", lathe([(5.6, 0.0), (5.6, 2.0), (5.0, 2.5), (0.0, 2.5)], 6), M["nut"]).location = P(ph["x"], ph["y"])
    at(mesh("PhonesHole", lathe([(3.4, 1.4), (0.0, 1.4)], 32), M["hole"]), ph["x"], ph["y"])
    dn = L["din"]
    at(mesh("Din", lathe([(8.0, 0.0), (8.0, 1.5), (7.0, 2.0), (6.6, 2.0), (6.5, 0.6), (0.0, 0.6)], 64), M["nut"]), dn["x"], dn["y"])
    for i in range(5):
        a = math.radians(-90 + i * 45 - 90)
        at(mesh("DinPin", lathe([(0.7, 0.0), (0.7, 0.9), (0.0, 0.9)], 12), M["hole"]), dn["x"] + math.cos(a) * 3.6, dn["y"] - math.sin(a) * 3.6)
    keyboard_case()

def keyboard_case(pressed=()):
    K = CAB["keyboard"]; KS = CAB["keys"]
    box("KbBody", K["x0"], K["y0"], K["x1"], K["y1"], -40.0, -0.5, M["kbcase"], 3.0)
    box("KbCheekL", K["x0"], K["y0"], K["x0"] + 8.0, K["y1"], -40.0, 16.0, M["kbcase"], 2.4)
    box("KbCheekR", K["x1"] - 8.0, K["y0"], K["x1"], K["y1"], -40.0, 16.0, M["kbcase"], 2.4)
    box("KbBack", K["x0"] + 8.0, K["y0"], K["x1"] - 8.0, K["y0"] + 12.0, -0.5, 9.0, M["kbcase"], 1.6)
    box("KbFelt", KS["x0"] - 1.0, KS["y0"] - 2.0, KS["x1"] + 1.0, KS["y0"] + 1.0, 0.0, 4.0, M["felt"], 0.3)
    keys(pressed)

def keys(pressed=()):
    KS = CAB["keys"]
    for k in L["keylist"]:
        dn = k["i"] in pressed
        if not k["black"]:
            box("WKey", k["x0"] + 0.35, KS["y0"], k["x1"] - 0.35, KS["y1"], 2.0, (11.0 if not dn else 9.0), M["white_key"], 0.8)
        else:
            box("BKey", k["x0"], KS["y0"], k["x1"], KS["y0"] + (KS["y1"] - KS["y0"]) * 0.62, 6.0, (21.0 if not dn else 18.5), M["black_key"], 1.0)

def frame_full(): return 0.0, 0.0, W, H

def mode_bg():
    build_static()
    bpy.context.scene.render.film_transparent = True
    rx, ry = camera(*frame_full())
    render(os.path.join(OUT, "bg.png"))
    x0, y0, x1, y1 = frame_full()
    json.dump(dict(width=rx, height=ry, pxmm=PXMM, x0=x0, y0=y0, theta=math.degrees(THETA),
                   cosT=math.cos(THETA), sinT=math.sin(THETA)), open(os.path.join(OUT, "bg.json"), "w"), indent=1)

def mode_preview():
    """Una franja de la consola a baja resolución para revisar materiales."""
    global PXMM
    PXMM = 2.0
    build_static(); camera(40.0, 330.0, 480.0, 640.0); render(os.path.join(OUT, "preview.png"))

# ───────── sprites (sombra sobre un plano shadow catcher) ─────────
def sprite_scene():
    reset(); materials(); lights()
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=50.0)
    catcher = mesh("Catcher", bm, M["back"], smooth=False); catcher.is_shadow_catcher = True
    bpy.context.scene.render.film_transparent = True

def sprite_camera(size_mm, lift_mm=0.0, w_mm=None):
    sc = bpy.context.scene
    w_mm = w_mm or size_mm
    cam = bpy.data.cameras.new("SpCam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = w_mm / 10.0
    cam.clip_start = 0.1; cam.clip_end = 1000
    o = bpy.data.objects.new("SpCam", cam); link(o)
    view = Vector((0, math.sin(THETA), -math.cos(THETA)))
    o.location = P(0, -lift_mm) - view * 300.0; o.rotation_euler = Euler((THETA, 0, 0), "XYZ"); sc.camera = o
    sc.render.resolution_x = int(round(w_mm * PXMM * 2)); sc.render.resolution_y = int(round(size_mm * PXMM * 2))
    return sc.render.resolution_x, sc.render.resolution_y

def sprite_meta(rx, ry, lift):
    return dict(w=rx, h=ry, size=rx, anchorX=rx / 2, anchorY=ry / 2 + lift * math.cos(THETA) * PXMM * 2)

def knob_obj(kind):
    """Perilla del 700: falda negra moleteada, tapa de aluminio con línea negra y raya blanca en la falda."""
    d = KSIZE[kind]; r = d / 2
    if kind == "KF":
        k = mesh("Knob", lathe([(r, 0.0), (r, 9.0), (r - 0.4, 9.5), (0.0, 9.6)], 96,
                               radial=lambda t, rr, z: rr - (0.18 if (z < 8.8 and math.cos(t * 24) > 0) else 0)), M["skirt"])
        cap = mesh("Cap", lathe([(r - 0.9, 9.55), (r - 0.9, 9.75), (0.0, 9.8)], 64), M["cap"]); cap.parent = k
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts: v.co.x *= 0.07; v.co.y = v.co.y * (r - 1.0) * 0.1 * 0.9 + (r - 1.0) * 0.05; v.co.z = v.co.z * 0.02 + 0.985
        ptr = mesh("Pointer", bm, M["pointer_k"], smooth=False); ptr.parent = k
        return k
    h = {"KS": 11.0, "KM": 12.0, "KL": 13.5, "KP": 7.0, "KQ": 11.0}[kind]
    capr = {"KS": r * 0.66, "KM": r * 0.64, "KL": r * 0.6, "KP": r * 0.78, "KQ": r * 0.66}[kind]
    skirt_top = h * 0.32
    prof = [(r, 0.0), (r, skirt_top), (r - 0.4, skirt_top + 0.4), (capr + 0.9, skirt_top + 0.8), (capr + 0.6, h - 0.8), (capr, h - 0.2), (0.0, h)]
    if kind == "KP":
        prof = [(r, 0.0), (r, 4.0), (r - 0.5, 4.6), (capr + 0.3, 5.4), (capr + 0.1, h), (0.0, h)]
    k = mesh("Knob", lathe(prof, 128, radial=lambda t, rr, z: rr - (0.22 if (z > skirt_top + 0.6 and rr > capr + 0.3 and math.cos(t * 36) > 0) else 0)), M["skirt"])
    cap = mesh("Cap", lathe([(capr - 0.2, h - 0.15), (capr - 0.2, h + 0.05), (0.0, h + 0.1)], 96), M["cap"]); cap.parent = k
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co.x *= 0.08; v.co.y = v.co.y * capr * 0.085 + capr * 0.055; v.co.z = v.co.z * 0.02 + (h + 0.1) / 10
    ptr = mesh("Pointer", bm, M["pointer_k"], smooth=False); ptr.parent = k
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co.x *= 0.07; v.co.y = v.co.y * (r - capr) * 0.06 + (capr + r) * 0.05; v.co.z = v.co.z * 0.02 + (skirt_top + 0.45) / 10
    ln = mesh("SkirtLine", bm, M["pointer_w"], smooth=False); ln.parent = k
    return k

def fader_cap():
    c = box("FaderCap", -4.25, -2.3, 4.25, 2.3, 4.0, 9.5, M["fader"], 0.8)
    stem = box("Stem", -0.9, -1.0, 0.9, 1.0, 0.0, 4.2, M["fader"], 0.2); stem.parent = c
    ln = box("FaderLine", -4.3, -0.22, 4.3, 0.22, 9.4, 9.56, M["pointer_w"], 0.0); ln.parent = c
    return c

def mode_sprites():
    meta = {"knobs": {}, "buttons": {}}
    for kind in sorted({k["size"] for k in L["knobs"]}):
        sprite_scene(); k = knob_obj(kind)
        d = KSIZE[kind]; sz = d + 8.0; lift = 3.0
        rx, ry = sprite_camera(sz, lift)
        name = f"knob_{kind}"
        os.makedirs(os.path.join(OUT, name), exist_ok=True)
        for f in range(128):
            k.rotation_euler.z = -math.radians(360.0 * f / 128)
            render(os.path.join(OUT, name, f"{f:03d}.png"))
        meta["knobs"][kind] = dict(sprite_meta(rx, ry, lift), frames=128, d=d)
    # Tapas de fader (vertical y horizontal).
    for nm, rot in (("fader_v", 0), ("fader_h", 90)):
        sprite_scene(); c = fader_cap(); c.rotation_euler.z = math.radians(rot)
        rx, ry = sprite_camera(14.0, 3.5); render(os.path.join(OUT, nm + ".png")); meta[nm] = sprite_meta(rx, ry, 3.5)
    # Botón de las llaves deslizantes y bola de los selectores de onda.
    sprite_scene(); box("Nub", -1.1, -1.6, 1.1, 1.6, 0.0, 3.2, M["skirt"], 0.4)
    rx, ry = sprite_camera(8.0, 1.5); render(os.path.join(OUT, "nub_v.png")); meta["nub_v"] = sprite_meta(rx, ry, 1.5)
    sprite_scene(); box("Nub", -1.6, -1.1, 1.6, 1.1, 0.0, 3.2, M["skirt"], 0.4)
    rx, ry = sprite_camera(8.0, 1.5); render(os.path.join(OUT, "nub_h.png")); meta["nub_h"] = sprite_meta(rx, ry, 1.5)
    sprite_scene()
    stem = mesh("LeverStem", lathe([(1.0, 0.0), (1.0, 5.0), (0.0, 5.0)], 24), M["chrome"])
    ball = mesh("LeverBall", lathe([(2.6 * math.sin(a), 6.2 - 2.6 * math.cos(a)) for a in [i * math.pi / 16 for i in range(17)]], 48), M["skirt"]); ball.parent = stem
    rx, ry = sprite_camera(10.0, 3.0); render(os.path.join(OUT, "lever.png")); meta["lever"] = sprite_meta(rx, ry, 3.0)
    # Palanca de los toggles (punta crema): arriba/centro/abajo e izquierda/derecha.
    sprite_scene()
    bat = mesh("Bat", lathe([(0.9, 0.0), (0.9, 4.5), (1.2, 6.2), (0.9, 7.3), (0.0, 7.5)], 32), M["chrome"])
    tip = mesh("Tip", lathe([(1.5, 6.0), (1.7, 7.4), (1.3, 8.6), (0.0, 8.9)], 32), M["cream"]); tip.parent = bat
    bat.location = P(0, 0, 3.3)
    rx, ry = sprite_camera(16.0, 2.5)
    for nm, rot in (("up", (30, 0)), ("mid", (0, 0)), ("down", (-30, 0)), ("left", (0, -30)), ("right", (0, 30))):
        bat.rotation_euler = Euler((math.radians(rot[0]), math.radians(rot[1]), 0), "XYZ")
        render(os.path.join(OUT, f"bat_{nm}.png"))
    meta["bat"] = sprite_meta(rx, ry, 2.5)
    # Botones apretados.
    for b in {(b["shape"], b["color"], b["d"], b["w"], b["h"]) for b in L["buttons"]}:
        shape, color, d, w_, h_ = b
        sprite_scene()
        if shape == "square": box("Btn", -w_ / 2, -h_ / 2, w_ / 2, h_ / 2, 0.0, 3.6, M["btn_" + color], 1.0); sz = max(w_, h_) + 7.0
        else: mesh("Btn", lathe([(d / 2, 0.0), (d / 2, 2.4), (d / 2 - 0.6, 3.1), (0.0, 3.3)], 48), M["btn_" + color]); sz = d + 7.0
        rx, ry = sprite_camera(sz, 1.0)
        nm = f"btn_{shape}_{color}_{int(round(w_ * 10))}"
        render(os.path.join(OUT, nm + ".png"))
        meta["buttons"][f"{shape}_{color}_{int(round(w_ * 10))}"] = dict(sprite_meta(rx, ry, 1.0), file=nm)
    # Fichas de 1/4" para los cables.
    os.makedirs(os.path.join(OUT, "plugs"), exist_ok=True)
    for cname, col in (("red", (0.62, 0.04, 0.03)), ("yellow", (0.8, 0.52, 0.02)), ("blue", (0.02, 0.13, 0.55)),
                       ("green", (0.03, 0.38, 0.08)), ("white", (0.78, 0.78, 0.76)), ("black", (0.015, 0.015, 0.015))):
        sprite_scene()
        body = mat("PlugBody" + cname, col, 0.4, coat=0.25)
        sleeve = mesh("Sleeve", lathe([(4.4, 0.0), (4.4, 2.0), (4.0, 2.6), (4.0, 7.0)], 48), M["chrome"]); sleeve.location = P(0, 0, 2.5)
        boot = mesh("Boot", lathe([(4.6, 7.0), (4.6, 14.0), (4.2, 18.0), (3.0, 22.0), (2.4, 24.0), (0.0, 24.2)], 48,
                                  radial=lambda t, rr, z: rr - (0.15 if (9.0 < z < 17.0 and math.cos(t * 16) > 0) else 0)), body)
        boot.parent = sleeve
        rx, ry = sprite_camera(24.0, 9.0)
        render(os.path.join(OUT, "plugs", f"{cname}.png"))
    meta["plugs"] = dict(sprite_meta(rx, ry, 9.0), topZ=26.5)
    json.dump(meta, open(os.path.join(OUT, "sprites.json"), "w"), indent=1)

# ───────── vistas en perspectiva (web y film) ─────────
def populate(preset_path, pressed=()):
    pr = json.load(open(preset_path)) if preset_path else {}
    pv = pr.get("panel", {})
    for k in L["knobs"]:
        v = pv.get(k["id"], 0.5)
        o = knob_obj(k["size"])
        t = (v / max(1, k["steps"] - 1)) if k["steps"] > 0 else v
        o.rotation_euler.z = -math.radians(k["a0"] + (k["a1"] - k["a0"]) * t)
        at(o, k["x"], k["y"], 7.2 if k["size"] == "KF" else 0.0)
        o["cid"] = k["id"]
    for s in L["sliders"]:
        v = pv.get(s["id"], 0.5 if s.get("detent") else 0.0)
        c = fader_cap()
        if s["horiz"]: c.rotation_euler.z = math.radians(90); at(c, s["x0"] + (s["x1"] - s["x0"]) * v, s["y"])
        else: at(c, s["x"], s["y1"] - (s["y1"] - s["y0"]) * v)
        c["cid"] = s["id"]
    for sw in L["switches"]:
        i = int(round(pv.get(sw["id"], 0))); n = sw["positions"]; l = sw["length"] - 4.0
        o = box("Nub", -1.1, -1.6, 1.1, 1.6, 0.0, 3.2, M["skirt"], 0.4) if sw["orient"] == "v" else box("Nub", -1.6, -1.1, 1.6, 1.1, 0.0, 3.2, M["skirt"], 0.4)
        f = i / max(1, n - 1)
        if sw["orient"] == "v": at(o, sw["x"], sw["y"] - l / 2 + l * f)
        else: at(o, sw["x"] - l / 2 + l * f, sw["y"])
    for lv in L["levers"]:
        i = int(round(pv.get(lv["id"], 0)))
        stem = mesh("LeverStem", lathe([(1.0, 0.0), (1.0, 5.0), (0.0, 5.0)], 24), M["chrome"])
        ball = mesh("LeverBall", lathe([(2.6 * math.sin(a), 6.2 - 2.6 * math.cos(a)) for a in [j * math.pi / 16 for j in range(17)]], 48), M["skirt"]); ball.parent = stem
        at(stem, lv["x"], lv["y0"] + (lv["y1"] - lv["y0"]) * i / (lv["positions"] - 1))
    for t in L["toggles"]:
        i = int(round(pv.get(t["id"], 0)))
        bat = mesh("Bat", lathe([(0.9, 0.0), (0.9, 4.5), (1.2, 6.2), (0.9, 7.3), (0.0, 7.5)], 24), M["chrome"])
        tip = mesh("Tip", lathe([(1.5, 6.0), (1.7, 7.4), (1.3, 8.6), (0.0, 8.9)], 24), M["cream"]); tip.parent = bat
        horiz = t.get("lpos") == "horizontal"
        ang = (30, 0, -30)[i] if t["positions"] == 3 else (30 if i == 0 else -30)
        bat.rotation_euler = Euler((0, math.radians(-ang), 0) if horiz else (math.radians(ang), 0, 0), "XYZ")
        at(bat, t["x"], t["y"], 3.3)
    return pr.get("cables", [])

def stand_up(angle=84.0):
    """Para las vistas en perspectiva: los gabinetes se paran detrás del teclado (que queda horizontal).
    Gira todo lo que está por encima del borde inferior de la consola alrededor de ese borde."""
    edge = -CAB["console"]["y1"] / 10.0
    piv = bpy.data.objects.new("Pivot", None); link(piv); piv.location = Vector((0.0, edge, 0.0))
    bpy.context.view_layer.update()                                     # matrix_world de los objetos recién creados
    for o in list(bpy.data.objects):
        if o.type not in ("MESH", "CURVE") or o.parent is not None or o.name.startswith(("Floor", "Wall")): continue
        if o.type == "MESH" and len(o.data.vertices):
            c = o.matrix_world @ (sum((v.co for v in o.data.vertices), Vector()) / len(o.data.vertices))
        else: c = o.location
        if c.y < edge - 0.05: continue                                  # teclado
        o.parent = piv; o.matrix_parent_inverse = Matrix.Translation(-piv.location)
    piv.rotation_euler.x = math.radians(angle)
    bpy.context.view_layer.update()
    return piv

def world_jack(piv, x, y, z):
    return piv.matrix_world @ (P(x, y, z) - piv.location) if piv else P(x, y, z)

def add_cables(cables_, piv):
    """Cables del preset: fichas en los jacks y curvas que cuelgan con la gravedad del mundo."""
    jpos = {j["id"]: (j["x"], j["y"]) for j in L["jacks"]}
    cols = {"red": (0.62, 0.04, 0.03), "yellow": (0.8, 0.52, 0.02), "blue": (0.02, 0.13, 0.55), "green": (0.03, 0.38, 0.08),
            "white": (0.78, 0.78, 0.76), "black": (0.015, 0.015, 0.015)}
    for i, c in enumerate(cables_):
        if c["src"] not in jpos or c["dst"] not in jpos: continue
        col = cols.get(c.get("color", "red"), cols["red"])
        cm = mat(f"CableMat{i}", col, 0.45, coat=0.2)
        for jid in (c["src"], c["dst"]):
            x, y = jpos[jid]
            sleeve = mesh("Sleeve", lathe([(4.4, 0.0), (4.4, 2.0), (4.0, 2.6), (4.0, 7.0)], 32), M["chrome"]); at(sleeve, x, y, 2.5)
            boot = mesh("Boot", lathe([(4.6, 7.0), (4.6, 14.0), (4.2, 18.0), (3.0, 22.0), (2.4, 24.0), (0.0, 24.2)], 32), cm); boot.parent = sleeve
            if piv: sleeve.parent = piv; sleeve.matrix_parent_inverse = Matrix.Translation(-piv.location)   # coordenadas del lienzo, giradas con los gabinetes
        pa = world_jack(piv, jpos[c["src"]][0], jpos[c["src"]][1], 26.0)
        pb = world_jack(piv, jpos[c["dst"]][0], jpos[c["dst"]][1], 26.0)
        out_dir = (piv.matrix_world.to_3x3() @ Vector((0, 0, 1))) if piv else Vector((0, 0, 1))
        cable_curve(f"Cable{i}", pa, pb, cm, out_dir)

def cable_curve(name, pa, pb, m, out_dir=Vector((0, 0, 1))):
    d = (pb - pa).length
    sag = min(14.0, 2.0 + d * 0.16)
    mid = (pa + pb) / 2 + out_dir * 2.0 + Vector((0, 0, -sag * 0.6))
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"
    cu.bevel_depth = 0.28; cu.bevel_resolution = 4; cu.resolution_u = 24
    sp = cu.splines.new("BEZIER"); sp.bezier_points.add(2)
    for bp, co in zip(sp.bezier_points, (pa, mid, pb)):
        bp.co = co; bp.handle_left_type = bp.handle_right_type = "AUTO"
    sp.bezier_points[0].handle_right_type = "FREE"; sp.bezier_points[0].handle_right = pa + out_dir * 2.4
    sp.bezier_points[0].handle_left_type = "FREE"; sp.bezier_points[0].handle_left = pa - out_dir * 2.4
    sp.bezier_points[2].handle_left_type = "FREE"; sp.bezier_points[2].handle_left = pb + out_dir * 2.4
    sp.bezier_points[2].handle_right_type = "FREE"; sp.bezier_points[2].handle_right = pb - out_dir * 2.4
    o = bpy.data.objects.new(name, cu); cu.materials.append(m); link(o)
    return o

def stage():
    # Ciclorama: piso que sube en curva detrás de los gabinetes (sin horizonte).
    bm = bmesh.new(); prof = []
    for i in range(25):
        a = i / 24 * math.pi / 2
        prof.append((-60.0 + 260.0 * (1 - math.cos(a)) * 0 + 0, 0))
    verts = []
    for yi in range(41):
        t = yi / 40
        y = -600.0 + t * 900.0                                    # de adelante hacia atrás
        z = -4.2 if y < 60.0 else -4.2 + (1 - math.cos(min(1.0, (y - 60.0) / 240.0) * math.pi / 2)) * 240.0
        if y > 300.0: z = -4.2 + 240.0 + (y - 300.0)
        verts.append((y, z))
    rows = []
    for y, z in verts:
        rows.append([bm.verts.new((x, y, z)) for x in (-1500.0, 1600.0)])
    for a, b in zip(rows[:-1], rows[1:]): bm.faces.new((a[0], a[1], b[1], b[0]))
    floor = mesh("Floor", bm, mat("FloorMat", (0.006, 0.006, 0.0065), 0.55), smooth=True)
    a = bpy.data.lights.new("Soft", "AREA"); a.energy = 240000.0; a.size = 90.0; a.color = (1.0, 0.96, 0.92)
    o = bpy.data.objects.new("Soft", a); link(o); o.location = Vector((5, -230, 140))
    o.rotation_euler = (Vector((45.0, -85.0, 40.0)) - o.location).to_track_quat("-Z", "Y").to_euler()
    b = bpy.data.lights.new("Rim", "AREA"); b.energy = 60000.0; b.size = 60.0; b.color = (0.85, 0.9, 1.0)
    o = bpy.data.objects.new("RimL", b); link(o); o.location = Vector((120, -150, 90))
    o.rotation_euler = (Vector((45.0, -85.0, 30.0)) - o.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.02

def persp_camera(loc, target, lens=50.0, fstop=0.05, w=2400, h=1100):
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("PCam"); cam.lens = lens; cam.clip_start = 0.1; cam.clip_end = 3000
    cam.dof.use_dof = True; cam.dof.aperture_fstop = fstop
    o = bpy.data.objects.new("PCam", cam); link(o); o.location = Vector(loc)
    tgt = bpy.data.objects.new("Target", None); link(tgt); tgt.location = Vector(target)
    c = o.constraints.new("TRACK_TO"); c.target = tgt; c.track_axis = "TRACK_NEGATIVE_Z"; c.up_axis = "UP_Y"
    cam.dof.focus_object = tgt
    sc.camera = o; sc.render.resolution_x = w; sc.render.resolution_y = h
    return o, tgt

PRESET = arg("--preset")
SHOTS = {
    # (cámara inicio, cámara fin), (objetivo inicio, objetivo fin), lente, f-stop — con los gabinetes parados.
    "wide":  ([(16.0, -330.0, 70.0), (74.0, -318.0, 60.0)], [(45.0, -86.0, 41.0), (46.0, -86.0, 39.0)], 42.0, 0.14),
    "vcos":  ([(2.0, -128.0, 30.0), (24.0, -124.0, 28.0)], [(10.0, -86.0, 30.0), (26.0, -86.0, 28.0)], 55.0, 0.09),
    "seq":   ([(24.0, -150.0, 72.0), (66.0, -150.0, 70.0)], [(26.0, -86.0, 66.0), (64.0, -86.0, 64.0)], 50.0, 0.08),
    "filters": ([(30.0, -130.0, 28.0), (54.0, -128.0, 27.0)], [(36.0, -86.0, 26.0), (54.0, -86.0, 26.0)], 55.0, 0.08),
    "keys":  ([(30.0, -150.0, 22.0), (60.0, -148.0, 20.0)], [(34.0, -98.0, 2.0), (58.0, -98.0, 2.0)], 45.0, 0.08),
}

def scene_persp():
    build_static(); cables_ = populate(PRESET)
    piv = stand_up(float(arg("--angle", "84")))
    add_cables(cables_, piv); stage()

def mode_hero():
    shots = [("hero", (24.0, -352.0, 66.0), (45.0, -86.0, 43.0), 42.0, 2400, 1100),
             ("hero_seq", (22.0, -150.0, 76.0), (40.0, -86.0, 66.0), 42.0, 2400, 1100),
             ("hero_console", (6.0, -140.0, 34.0), (26.0, -86.0, 28.0), 42.0, 2400, 1100),
             ("hero_keys", (22.0, -170.0, 30.0), (40.0, -95.0, 4.0), 40.0, 2400, 1100)]
    only = arg("--only")
    for nm, loc, tgt, lens, w, h in shots:
        if only and nm not in only.split(","): continue
        scene_persp()
        persp_camera(loc, tgt, lens, 0.12, w, h)
        render(os.path.join(OUT, f"{nm}.png"))

def mode_film():
    shot = arg("--shot", "wide")
    cams, tgts, lens, fstop = SHOTS[shot]
    scene_persp()
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
    sc.render.use_persistent_data = True
    bpy.ops.render.render(animation=True)

# ───────── film de la web (filmkit, --mode film2) ─────────
FILM_BPM = 128.0                       # Berlin Twelve: CLOCK RATE 32 con DIVIDE 1/16 = semicorcheas a 128 BPM
STEP = 60.0 / FILM_BPM / 4             # 117,2 ms por paso; el 717A arranca en el paso 1 en t = 0 (medido en el audio)
FILM_SHOTS = [("open", 9), ("grid", 6), ("truck", 9), ("vu", 6), ("reveal", 9), ("end", 9)]   # en tiempos; 3 = una vuelta de 12 pasos
FPS = 24

def film2():
    """Film de la web: la máquina tocando Berlin Twelve (el preset 17, el mismo audio que arma scripts/film.py con
    at700render). Seis tomas cortadas al tiempo de 128 BPM y siempre en el paso 1 de la secuencia (cada 3 tiempos):

      open    9 t  4,2 s  macro rasante a lo largo de la fila de lámparas del 717A: la luz roja corre por los 12 pasos
                          (con su resplandor), la cámara avanza despacio y el foco va de la lámpara 1 a
                          la 5; abajo a la izquierda queda panel desenfocado y oscuro para el título.
      grid    6 t  2,8 s  la matriz de 3 × 12 perillas del 717A en tres cuartos, travelling lateral corto; una luz
                          rasante barre las tapas de aluminio; el reloj (CLOCK) parpadea con cada paso.
      truck   9 t  4,2 s  travelling a lo largo de la fila de arriba de la consola (702A → 703E) con los cables
                          balanceándose delante; termina en el 703E, donde el fader de CUTOFF sube y baja con el
                          brillo del sonido y el LED de señal del filtro late con cada nota; el foco pasa al fader.
      vu      6 t  2,8 s  los dos VU del 711A de cerca, con las agujas siguiendo el nivel (balística de VU por canal),
                          esferas apenas iluminadas; foco de la aguja izquierda a la derecha.
      reveal  9 t  4,2 s  desde abajo, casi sobre el teclado, la grúa sube y retrocede hasta mostrar la pared entera
                          con las lámparas corriendo arriba.
      end     9 t  4,2 s  plano general en tres cuartos desde la derecha que se aleja despacio (film.py lo oscurece y pone el cierre);
                          entra fundido.

    Se mueven: lámparas de paso y reloj (con la grilla exacta de 117,2 ms), LED de señal del VCF1 y del VCA1 (con la
    envolvente), fader de CUTOFF del VCF1, agujas de los VU, los cinco cables (balanceo lento) y la luz que barre.
    El teclado no se toca: Berlin Twelve lo toca el secuenciador.

        blender -b --factory-startup -P scene700.py -- --mode film2 [--shot open,vu] --quality draft|final
                --audio berlin.wav --out <dir>
    El WAV es el de at700render (16 bits): at700render presets/17_berlin_twelve.json x.wav --secs 24.
    """
    import re, tempfile
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final")
    wav = arg("--audio")
    env = fk.envelope(wav)
    vul, vur = vu_needles(wav)
    slow = smooth(env, 0.5)
    m_ = sum(slow) / len(slow); sd = (sum((v - m_) ** 2 for v in slow) / len(slow)) ** 0.5 or 1.0
    # Preset completo: los valores por defecto del panel y encima los del preset (populate necesita todos).
    pr = json.load(open(os.path.join(HERE, "..", "..", "assets", "presets", "17_berlin_twelve.json")))
    hdr = open(os.path.join(HERE, "..", "..", "Source", "dsp", "Params700.generated.h")).read()
    panel = {m.group(1): float(m.group(2)) for m in re.finditer(r'\{\s*"(\w+)",\s*"[^"]*",\s*-?\d+,\s*(-?[\d.]+)f', hdr)}
    panel.update(pr["panel"]); pr["panel"] = panel
    ppath = os.path.join(tempfile.gettempdir(), "at700_film_preset.json"); json.dump(pr, open(ppath, "w"))
    cut0 = pr["panel"]["vcf1_cutoff"]

    f0 = 0
    for name, beats in FILM_SHOTS:
        n = int(round(beats * 60.0 / FILM_BPM * FPS))           # igual que scripts/film.py
        start, f0 = f0, f0 + n
        frames = n + (12 if name == "reveal" else 0)             # cola para el fundido hacia "end"
        if only and name not in only.split(","): continue
        sc, piv, ctl = film_scene(ppath)
        fk.setup(sc, quality)
        sc.render.image_settings.color_mode = "RGB"
        bloom(sc)
        Wc = lambda x, y, z=0.0: world_jack(piv, x, y, z)       # punto del lienzo (mm) parado, z = distancia al panel
        cam = fk.camera(sc); cam.data.clip_start = 0.05; cam.data.clip_end = 5000.0
        sweep = None
        if name == "grid":                                       # luz rasante que barre las tapas de aluminio
            l = bpy.data.lights.new("Sweep", "AREA"); l.shape = "RECTANGLE"; l.size = 2.0; l.size_y = 40.0
            l.color = (1.0, 0.93, 0.85); l.energy = 2500.0
            sweep = bpy.data.objects.new("Sweep", l); link(sweep)
        lamp = lambda i: Wc(246.0 + 42.25 * i, 238.0)              # i fraccionario: entre lámparas
        paths = {
            "open":  fk.Path([(0, Wc(118, 240, 74), Wc(400, 246, 0), 50, 0.05),
                              (1, Wc(150, 238, 62), Wc(440, 245, 0), 50, 0.05)]),
            "grid":  fk.Path([(0, Wc(380, 330, 380), Wc(470, 172, 0), 40, 0.06),
                              (1, Wc(470, 320, 360), Wc(508, 172, 0), 40, 0.06)]),
            "truck": fk.Path([(0, Wc(70, 580, 170), Wc(110, 470, 0), 45, 0.08),
                              (0.55, Wc(210, 575, 165), Wc(250, 470, 0), 45, 0.08),
                              (1, Wc(320, 565, 150), Wc(360, 490, 0), 48, 0.07)], easing="smooth"),
            "vu":    fk.Path([(0, Wc(640, 600, 175), Wc(735, 556, 0), 40, 0.035),
                              (1, Wc(662, 592, 150), Wc(742, 556, 0), 42, 0.035)]),
            "reveal": fk.Path([(0, Vector((30.0, -121.0, 3.5)), Vector((42.0, -86.0, 30.0)), 24, 0.06),
                               (0.5, Vector((40.0, -170.0, 18.0)), Vector((44.0, -86.0, 38.0)), 27, 0.09),
                               (1, Vector((50.0, -238.0, 32.0)), Vector((45.0, -86.0, 41.0)), 30, 0.12)]),
            "end":   fk.Path([(0, Vector((92.0, -258.0, 32.0)), Vector((45.0, -86.0, 41.0)), 34, 0.14),
                              (1, Vector((100.0, -296.0, 37.0)), Vector((45.0, -86.0, 40.0)), 34, 0.14)], easing="out"),
        }
        path = paths[name]
        shake = {"open": 0.10, "vu": 0.10, "truck": 0.05}.get(name, 0.0)
        fader = ctl["sl"]["vcf1_cutoff"]
        fs = [s_ for s_ in L["sliders"] if s_["id"] == "vcf1_cutoff"][0]

        def pose(f, t, u, name=name, path=path, start=start):
            g = start + f - 1; ta = g / FPS
            k = max(-2.0, min(2.0, (slow[min(g, len(slow) - 1)] - m_) / sd))
            v = max(0.0, min(1.0, cut0 + 0.07 * k))               # la mano acompaña el brillo del sonido
            fader.location = P(fs["x"], fs["y1"] - (fs["y1"] - fs["y0"]) * v)
            for nd, ang in ((ctl["vu"][0], vul), (ctl["vu"][1], vur)):
                nd.rotation_euler.z = -math.radians(ang[min(g, len(ang) - 1)])
            focus = None
            if name == "open": focus = lamp(fk.rack(u, 0.2, 0.85, 0.0, 4.0))             # de la lámpara 1 a la 5
            if name == "truck": focus = path.at(u)[1].lerp(Wc(365.0, 495.0), fk.rack(u, 0.6, 0.9, 0.0, 1.0))
            if name == "vu": focus = Wc(688.5, 556.0).lerp(Wc(794.0, 556.0), fk.rack(u, 0.35, 0.8, 0.0, 1.0))
            path.apply(cam, u, focus=focus, shake=shake, t=ta)
            moved = [fader] + list(ctl["vu"])
            if sweep is not None:
                x = 18.0 + 60.0 * fk.ease(u, "smooth")                # entra y sale del cuadro por los costados
                sweep.location = Vector((x, -102.0, 104.0))
                sweep.rotation_euler = (Wc(x * 10.0, 160.0) - sweep.location).to_track_quat("-Z", "Y").to_euler()
                moved.append(sweep)
            return moved

        def keys(f, start=start, frames=frames):
            # Lo que no es transformación se graba en propiedades del objeto (las lee el material con un nodo
            # Attribute): las claves en el árbol de nodos de un material copiado no llegaban al render en Blender 5.2.
            g = start + f - 1; ta = g / FPS
            e = env[min(g, len(env) - 1)]
            lv = {"step%d" % (i + 1): lamp_level(ta, i) for i in range(12)}
            lv.update(seq_clock=clock_level(ta), vcf1_sig=e ** 1.5, vca1_sig=e ** 1.5)
            for lid, val in lv.items():
                o = ctl["leds"][lid]; o["glow"] = val; o.keyframe_insert('["glow"]')
            for (bp, base, nrm, lat, i) in ctl["cables"]:            # balanceo lento, distinto en cada cable
                bp.co = base + nrm * (0.35 * fk.noise1(ta, 11 + i, 0.32)) + lat * (0.5 * fk.noise1(ta, 23 + i, 0.27))
                bp.keyframe_insert("co")

        if arg("--dry"):                                         # --dry 1: arma la escena y prueba las poses sin renderizar
            for f in (1, frames // 2, frames):
                sc.frame_set(f); pose(f, (f - 1) / FPS, (f - 1) / max(1, frames - 1)); keys(f)
                print("dry", name, f, tuple(round(c, 1) for c in cam.location), round(cam.data.lens), flush=True)
            if arg("--dry") == "3":                              # un cuadro en la CPU, chico, para revisar luces y encuadre
                fr = int(arg("--frame", "1")); sc.frame_set(fr)
                pose(fr, (fr - 1) / FPS, (fr - 1) / max(1, frames - 1)); keys(fr); sc.frame_set(fr)
                sc.cycles.device = "CPU"; sc.cycles.samples = 16; sc.render.resolution_percentage = 30
                sc.render.filepath = os.path.join(OUT, f"cpu_{name}_{fr:04d}.jpg"); bpy.ops.render.render(write_still=True)
            continue
        fk.render_shot(sc, name, frames, pose, OUT, extra_keys=keys, step=int(arg("--step", "1")))   # --step: vistazo rápido

def bloom(sc):
    """Resplandor suave de lo que pasa del umbral (lámparas, LED y brillos del cromo), en el compositor del render."""
    t = bpy.data.node_groups.new("FilmComp", "CompositorNodeTree")
    t.interface.new_socket(name="Image", in_out="OUTPUT", socket_type="NodeSocketColor")
    rl = t.nodes.new("CompositorNodeRLayers"); gl = t.nodes.new("CompositorNodeGlare"); out = t.nodes.new("NodeGroupOutput")
    gl.inputs["Type"].default_value = "Bloom"; gl.inputs["Quality"].default_value = "High"
    gl.inputs["Threshold"].default_value = 4.0; gl.inputs["Strength"].default_value = 0.5; gl.inputs["Size"].default_value = 0.45
    t.links.new(rl.outputs["Image"], gl.inputs["Image"]); t.links.new(gl.outputs["Image"], out.inputs[0])
    sc.compositing_node_group = t
    try: sc.render.compositor_device = "CPU"                    # en WSL el compositor en la GPU (EGL) no arranca
    except (AttributeError, TypeError): pass

def lamp_level(t, i, glow=0.05):
    """Lámpara del paso i (0..11): encendida mientras es el paso actual, con el resplandor que se apaga en ~50 ms;
    promediada en el tiempo del obturador (medio cuadro) para que el parpadeo no dependa de dónde cae el cuadro."""
    acc = 0.0
    for j in range(8):
        tt = t + (j / 7.0 - 0.5) * 0.5 / FPS
        if tt < 0: continue
        k = int(tt / STEP); d = (k - i) % 12
        if d == 0: acc += 1.0
        elif k - d >= 0: acc += math.exp(-(tt - (k - d + 1) * STEP) / glow)
    return acc / 8

def clock_level(t):
    """LED del reloj: alto la primera mitad de cada paso (CLOCK PW 0,5)."""
    acc = 0.0
    for j in range(8):
        tt = t + (j / 7.0 - 0.5) * 0.5 / FPS
        acc += 1.0 if (tt / STEP) % 1.0 < 0.5 else 0.0
    return acc / 8

def smooth(x, tau):
    """Suavizado de ida y vuelta (sin retraso) con constante tau (s)."""
    a = math.exp(-1.0 / (tau * FPS)); y = list(x); s = y[0]
    for i in range(len(y)): s = a * s + (1 - a) * y[i]; y[i] = s
    s = y[-1]
    for i in range(len(y) - 1, -1, -1): s = a * s + (1 - a) * y[i]; y[i] = s
    return y

def vu_needles(path):
    """Ángulo de cada aguja (grados, −48 = reposo, +48 = +3 VU) por cuadro: RMS por canal y una aguja de masa y
    resorte de 2,1 Hz con amortiguamiento 0,8 (unos 300 ms hasta la lectura, como un VU)."""
    import wave, array
    w = wave.open(path); sr = w.getframerate(); ch = w.getnchannels(); n = w.getnframes()
    if w.getsampwidth() != 2: raise ValueError("WAV de 16 bits para los VU")
    a = array.array("h"); a.frombytes(w.readframes(n)); w.close()
    per = sr / FPS; nf = int(n / per)
    rms = []
    for c in range(min(2, ch)):
        x = a[c::ch]; r = []
        for i in range(nf):
            seg = x[int(i * per):int((i + 1) * per)]
            r.append(math.sqrt(sum(v * v for v in seg) / max(1, len(seg))) / 32768.0)
        rms.append(r)
    if len(rms) == 1: rms.append(rms[0])
    ref = sorted(rms[0] + rms[1])[int(0.8 * 2 * nf)] or 1.0          # 0 VU ≈ el nivel de las notas fuertes
    res = []
    for r in rms:
        w0 = 2 * math.pi * 2.1; z = 0.8; th, v, out = -48.0, 0.0, []
        for lv in r:
            tgt = -48.0 + 96.0 * min(1.05, (lv / ref) / 1.4125)
            for _ in range(8):
                h = 1.0 / (FPS * 8); v += (w0 * w0 * (tgt - th) - 2 * z * w0 * v) * h; th += v * h
            th = max(-50.0, min(50.0, th)); out.append(th)
        res.append(out)
    return res

def vu_needle(m):
    """Aguja del VU: gira en el pivote de la esfera (abajo al centro, fuera de la ventana, como en vu_face.png) y
    nace dentro del marco inferior, así el eje no se ve. Origen en el pivote; rotation z = −ángulo."""
    y0 = m["y"] - m["h"] / 2; fh = m["h"] - 1.0
    py = y0 + 0.5 + 1.18 * fh; R = 0.98 * fh
    r0, r1 = 4.2, 0.86 * R
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= 0.05; v.co.y = (r0 + (r1 - r0) * (v.co.y + 0.5)) / 10; v.co.z = v.co.z * 0.015 + 0.10
    o = mesh("VuNeedle", bm, M["pointer_k"], smooth=False); o.location = P(m["x"], py)
    return o

def film_scene(ppath):
    """La escena de las vistas en perspectiva más lo que el film anima: agujas de los VU, LED con emisión propia y
    su resplandor, y los cables preparados para balancearse."""
    build_static(); cables_ = populate(ppath)
    needles = [vu_needle(m) for m in L["meters"]]
    piv = stand_up(float(arg("--angle", "84")))
    add_cables(cables_, piv); stage()
    sc = bpy.context.scene
    bpy.data.objects["Key"].data.energy = 1.8                    # el sol de las fotos aclaraba el negro en los planos rasantes
    M["vu"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 0.12   # esferas apenas iluminadas
    ctl = {"vu": needles, "sl": {}, "leds": {}, "cables": []}
    glow = {}
    for key, col, gain in (("red", (1.0, 0.045, 0.01), 40.0), ("green", (0.2, 1.0, 0.12), 12.0)):
        mm = M["led_" + key].copy(); mm.name = "LedGlow_" + key; nt = mm.node_tree
        at_ = nt.nodes.new("ShaderNodeAttribute"); at_.attribute_type = "OBJECT"; at_.attribute_name = "glow"
        mu = nt.nodes.new("ShaderNodeMath"); mu.operation = "MULTIPLY"; mu.inputs[1].default_value = gain
        b = nt.nodes["Principled BSDF"]; b.inputs["Emission Color"].default_value = (*col, 1)
        nt.links.new(at_.outputs["Fac"], mu.inputs[0]); nt.links.new(mu.outputs["Value"], b.inputs["Emission Strength"])
        glow[key] = mm
    for o in bpy.data.objects:
        cid = o.get("cid")
        if cid is None: continue
        if o.name.startswith("FaderCap"): ctl["sl"][cid] = o
        if o.name.startswith("Led"):
            o.data.materials[0] = glow["red" if "red" in o.data.materials[0].name.lower() else "green"]
            o["glow"] = 0.8 if cid in ("power", "seq_start") else 0.0      # encendido y secuenciador en marcha
            ctl["leds"][cid] = o
    nrm = piv.matrix_world.to_3x3() @ Vector((0, 0, 1))
    for o in bpy.data.objects:
        if o.type == "CURVE" and o.name.startswith("Cable"):
            sp = o.data.splines[0]; bp = sp.bezier_points[1]
            chord = (sp.bezier_points[2].co - sp.bezier_points[0].co); lat = chord.cross(nrm).normalized()
            ctl["cables"].append((bp, bp.co.copy(), nrm, lat, len(ctl["cables"])))
    return sc, piv, ctl

if __name__ == "__main__":
    modes = {"bg": mode_bg, "sprites": mode_sprites, "preview": mode_preview, "hero": mode_hero, "film": mode_film, "film2": film2}
    for m in arg("--mode", "bg,sprites").split(","):
        print("== modo", m, flush=True); modes[m]()
