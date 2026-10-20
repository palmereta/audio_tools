"""Escena Blender del ATFS1: el frente de rack 1U (fondo del editor), sprites de los controles y
vistas en perspectiva para la web.

Corre en pc50-wsl:
  blender -b -P scenefs1.py -- --layout layoutfs1.json --silk silkscreenfs1.png --out outfs1 --mode bg,sprites --samples 160
  blender -b -P scenefs1.py -- --mode hero --out outfs1
  blender -b -P scenefs1.py -- --mode film --shot pan --frames 120 --out outfs1
  blender -b --factory-startup -P scenefs1.py -- --mode film2 [--shot open,knobs] [--quality draft|final]
          --audio <wav> --out <dir> [--step N]      film de la web (filmkit); el guion está en film2()

1 unidad de Blender = 10 mm. Punto del frente (x, y) mm → (x/10, −y/10, z/10); z sale del frente.
Cámara ortográfica inclinada THETA: en pantalla sx = x − x0, sy = (y − y0)·cosθ − z·sinθ (mm).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Euler

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d

HERE = os.path.dirname(os.path.abspath(__file__))
def _asset(name, default, *alt):
    """Archivo pedido por argumento; si no está en la carpeta actual, el del repo (plugins/atfs1/assets/...)."""
    p = arg(name, default)
    if arg(name) is None and not os.path.exists(p):
        q = os.path.join(HERE, "..", "..", "assets", *alt)
        if os.path.exists(q): return q
    return p
L = json.load(open(_asset("--layout", "layoutfs1.json", "layoutfs1.json")))
SILK = os.path.abspath(_asset("--silk", "silkscreenfs1.png", "render", "silkscreenfs1.png"))
OUT = os.path.abspath(arg("--out", "outfs1"))
SAMPLES = int(arg("--samples", "160"))
os.makedirs(OUT, exist_ok=True)

THETA = math.radians(-7.0)            # negativa: la cámara mira desde un poco arriba
PXMM = float(arg("--pxmm", "8.0"))
W, H = L["size"]
DEPTH = 330.0                                    # fondo del gabinete

def P(x, y, z=0.0): return Vector((x / 10.0, -y / 10.0, z / 10.0))

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
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.55, 0.6, 0.68, 1); bg.inputs["Strength"].default_value = 0.12
    return sc

def lights(scale=1.0):
    def sun(name, rot, e, ang, col):
        l = bpy.data.lights.new(name, "SUN"); l.energy = e * scale; l.angle = math.radians(ang); l.color = col
        o = bpy.data.objects.new(name, l); bpy.context.collection.objects.link(o); o.rotation_euler = Euler(rot, "XYZ")
    # El frente mira a +z (hacia la cámara): luz de arriba a la izquierda, rasante para el cepillado.
    # Rotación en x negativa = la luz viene de arriba (de −y en el frente) y las sombras caen hacia abajo.
    sun("Key", (math.radians(-36), math.radians(-22), 0), 3.2, 16, (1.0, 0.97, 0.93))
    sun("Fill", (math.radians(8), math.radians(30), 0), 0.7, 40, (0.85, 0.9, 1.0))
    sun("Top", (math.radians(-64), 0, 0), 0.8, 25, (1, 1, 1))

def camera(x0, y0, x1, y1):
    sc = bpy.context.scene
    w_mm, h_mm = x1 - x0, (y1 - y0) * math.cos(THETA)
    cam = bpy.data.cameras.new("Cam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = w_mm / 10.0
    cam.clip_start = 0.1; cam.clip_end = 2000
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
def mat(name, col, rough, metal=0.0, coat=0.0, spec=0.5, emit=None, aniso=0.0, transmission=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    if aniso and "Anisotropic" in b.inputs: b.inputs["Anisotropic"].default_value = aniso
    if transmission and "Transmission Weight" in b.inputs: b.inputs["Transmission Weight"].default_value = transmission
    if emit:
        b.inputs["Emission Color"].default_value = (*emit[0], 1); b.inputs["Emission Strength"].default_value = emit[1]
    return m

def panel_material():
    """Aluminio cepillado anodizado gris azulado, con la serigrafía (color y alfa) encima."""
    m = bpy.data.materials.new("Panel"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(SILK); tex.extension = "CLIP"; tex.interpolation = "Cubic"
    uv = N.new("ShaderNodeTexCoord"); Lk.new(uv.outputs["UV"], tex.inputs["Vector"])
    # Cepillado horizontal: ruido estirado en x.
    mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (2.0, 260.0, 1.0)
    Lk.new(uv.outputs["Object"], mp.inputs["Vector"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 30.0; nz.inputs["Detail"].default_value = 8.0
    Lk.new(mp.outputs["Vector"], nz.inputs["Vector"])
    base = N.new("ShaderNodeMix"); base.data_type = "RGBA"
    Lk.new(nz.outputs["Fac"], base.inputs["Factor"])
    base.inputs["A"].default_value = (0.31, 0.38, 0.47, 1); base.inputs["B"].default_value = (0.38, 0.45, 0.55, 1)
    mix = N.new("ShaderNodeMix"); mix.data_type = "RGBA"
    Lk.new(tex.outputs["Alpha"], mix.inputs["Factor"]); Lk.new(base.outputs["Result"], mix.inputs["A"]); Lk.new(tex.outputs["Color"], mix.inputs["B"])
    Lk.new(mix.outputs["Result"], b.inputs["Base Color"])
    r = N.new("ShaderNodeMapRange"); Lk.new(nz.outputs["Fac"], r.inputs["Value"])
    r.inputs["To Min"].default_value = 0.28; r.inputs["To Max"].default_value = 0.4
    rm = N.new("ShaderNodeMix"); rm.data_type = "FLOAT"
    Lk.new(tex.outputs["Alpha"], rm.inputs["Factor"]); Lk.new(r.outputs["Result"], rm.inputs["A"]); rm.inputs["B"].default_value = 0.55
    Lk.new(rm.outputs["Result"], b.inputs["Roughness"])
    mm = N.new("ShaderNodeMix"); mm.data_type = "FLOAT"
    Lk.new(tex.outputs["Alpha"], mm.inputs["Factor"]); mm.inputs["A"].default_value = 0.7; mm.inputs["B"].default_value = 0.0
    Lk.new(mm.outputs["Result"], b.inputs["Metallic"])
    if "Anisotropic" in b.inputs: b.inputs["Anisotropic"].default_value = 0.5
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.05; bump.inputs["Distance"].default_value = 0.001
    Lk.new(nz.outputs["Fac"], bump.inputs["Height"]); Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

def lcd_material():
    """Cristal del LCD con la luz de fondo amarillo verdosa (sin texto: el editor dibuja los caracteres)."""
    m = bpy.data.materials.new("Lcd"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    uv = N.new("ShaderNodeTexCoord")
    g = N.new("ShaderNodeTexGradient"); g.gradient_type = "QUADRATIC_SPHERE"
    mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (0.9, 2.6, 1.0); mp.inputs["Location"].default_value = (0, 0, 0)
    Lk.new(uv.outputs["Generated"], mp.inputs["Vector"])
    sub = N.new("ShaderNodeVectorMath"); sub.operation = "SUBTRACT"; sub.inputs[1].default_value = (0.5, 0.5, 0.0)
    Lk.new(uv.outputs["Generated"], sub.inputs[0])
    sc = N.new("ShaderNodeVectorMath"); sc.operation = "MULTIPLY"; sc.inputs[1].default_value = (1.3, 2.4, 1.0)
    Lk.new(sub.outputs["Vector"], sc.inputs[0]); Lk.new(sc.outputs["Vector"], g.inputs["Vector"])
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.28, 0.38, 0.02, 1); ramp.color_ramp.elements[1].color = (0.46, 0.6, 0.05, 1)
    Lk.new(g.outputs["Fac"], ramp.inputs["Fac"])
    Lk.new(ramp.outputs["Color"], b.inputs["Base Color"])
    if "Emission Color" in b.inputs:
        Lk.new(ramp.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.55
    b.inputs["Roughness"].default_value = 0.45
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = 0.15
    return m

M = {}
def materials():
    M["panel"] = panel_material()
    M["lcd"] = lcd_material()
    M["case"] = mat("Case", (0.03, 0.032, 0.036), 0.55, metal=0.4)
    M["black"] = mat("Black", (0.008, 0.008, 0.009), 0.5)
    M["hole"] = mat("Hole", (0.002, 0.002, 0.002), 0.9)
    M["bezel"] = mat("Bezel", (0.012, 0.012, 0.013), 0.35)
    M["knob"] = mat("Knob", (0.012, 0.012, 0.013), 0.42, coat=0.15)
    M["collar"] = mat("Collar", (0.09, 0.095, 0.1), 0.25, metal=0.9)
    M["white"] = mat("White", (0.85, 0.85, 0.84), 0.45)
    M["grey_btn"] = mat("GreyBtn", (0.3, 0.31, 0.32), 0.42, coat=0.2)
    M["black_btn"] = mat("BlackBtn", (0.012, 0.012, 0.013), 0.34, coat=0.25)
    M["screw"] = mat("Screw", (0.02, 0.02, 0.022), 0.3, metal=0.8)
    M["led_off"] = mat("LedOff", (0.18, 0.17, 0.17), 0.12, coat=1.0)
    M["led_on"] = mat("LedOn", (0.9, 0.06, 0.03), 0.15, coat=1.0, emit=((1.0, 0.08, 0.03), 9.0))
    M["rail"] = mat("Rail", (0.05, 0.05, 0.055), 0.38, metal=0.9)
    M["floor"] = mat("Floor", (0.018, 0.019, 0.021), 0.55)

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
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    vs = [bm.verts.new(P(x, y, z)) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]
    f = bm.faces.new(vs)
    for lp in f.loops:
        co = lp.vert.co; lp[uvl].uv = (co.x * 10 / W, 1.0 + co.y * 10 / H)
    return mesh(name, bm, m, smooth=False)

def frame(name, x0, y0, x1, y1, t, z0, z1, m, bevel=0.3):
    for bx in ((x0, y0, x1, y0 + t), (x0, y1 - t, x1, y1), (x0, y0 + t, x0 + t, y1 - t), (x1 - t, y0 + t, x1, y1 - t)):
        box(name, *bx, z0, z1, m, bevel)

def screw(x, y):
    o = mesh("Screw", lathe([(2.6, 0.0), (2.6, 0.5), (2.2, 1.2), (1.2, 1.55), (0.0, 1.6)], 32), M["screw"]); at(o, x, y)
    box("Cross1", x - 1.5, y - 0.25, x + 1.5, y + 0.25, 1.0, 1.7, M["hole"], 0.0)
    box("Cross2", x - 0.25, y - 1.5, x + 0.25, y + 1.5, 1.0, 1.7, M["hole"], 0.0)

def big_knob():
    """Perilla grande: collar cónico de metal oscuro y tapa negra moleteada con tres rayas blancas."""
    collar = mesh("Collar", lathe([(9.4, 0.0), (9.4, 0.6), (8.8, 1.4), (8.1, 3.6), (7.6, 3.8), (0.0, 3.8)], 128), M["collar"])
    cap = mesh("Cap", lathe([(7.25, 3.6), (7.25, 13.6), (6.9, 14.3), (5.0, 14.6), (0.0, 14.7)], 160,
                            radial=lambda t, rr, z: rr - (0.24 if (4.2 < z < 13.4 and math.cos(t * 30) > 0) else 0)), M["knob"])
    cap.parent = collar
    for i, w in enumerate((-0.55, 0.0, 0.55)):
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts: v.co.x = v.co.x * 0.03 + w / 10; v.co.y = v.co.y * 0.32 + 0.47; v.co.z = v.co.z * 0.02 + 1.468
        ln = mesh("Line", bm, M["white"], smooth=False); ln.parent = collar
    return collar

def small_knob():
    cap = mesh("Vol", lathe([(4.6, 0.0), (4.6, 9.0), (4.3, 9.6), (0.0, 9.8)], 96,
                            radial=lambda t, rr, z: rr - (0.16 if (0.6 < z < 8.6 and math.cos(t * 24) > 0) else 0)), M["knob"])
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co.x *= 0.05; v.co.y = v.co.y * 0.2 + 0.25; v.co.z = v.co.z * 0.02 + 0.979
    ln = mesh("VolLine", bm, M["white"], smooth=False); ln.parent = cap
    return cap

def button_cap(b, pressed=False):
    r = b["d"] / 2; h = 2.6 if not pressed else 1.6
    m = M["grey_btn"] if b["kind"] == "grey" else M["black_btn"]
    return mesh("Btn", lathe([(r, -1.0), (r, h - 0.6), (r - 0.4, h), (0.0, h + (0.25 if b["kind"] == "grey" else -0.15))], 64), m)

def build_static(case=False):
    reset(); materials(); lights()
    # Frente: placa con borde, serigrafía, orejas con ranuras.
    # Placa con dos ventanas (LCD y llave de encendido): franjas verticales alrededor de los huecos.
    c = L["lcd"]; pw = L["power"]
    holes = [(c["x0"] + 1.5, c["y0"] + 1.5, c["x1"] - 1.5, c["y1"] - 1.5), (pw["x0"] + 1.6, pw["y0"] + 1.6, pw["x1"] - 1.6, pw["y1"] - 1.6)]
    xs = sorted({0.0, W, *[h[0] for h in holes], *[h[2] for h in holes]})
    for xa, xb in zip(xs[:-1], xs[1:]):
        ys = [0.0]
        for h in holes:
            if h[0] <= xa and xb <= h[2]: ys += [h[1], h[3]]
        ys.append(H)
        for ya, yb in zip(ys[0::2], ys[1::2]):
            box("PanelBody", xa, ya, xb, yb, -2.6, -0.02, M["panel"], 0.0)
            textured_plane("PanelTop", xa, ya, xb, yb, 0.0, M["panel"])
    if case:
        box("Case", 6.0, 0.6, W - 6.0, H - 0.6, -DEPTH, -2.6, M["case"], 1.0)
    for s in L["screws"]: screw(s["x"], s["y"])
    # LCD: hueco con marco negro y cristal hundido.
    g = c["glass"]
    frame("LcdFrame", c["x0"], c["y0"], c["x1"], c["y1"], 2.1, -0.2, 0.9, M["bezel"], 0.4)
    box("LcdWell", c["x0"] + 1.5, c["y0"] + 1.5, c["x1"] - 1.5, c["y1"] - 1.5, -3.5, -2.2, M["bezel"], 0.0)
    frame("LcdSide", g["x0"] - 0.6, g["y0"] - 0.6, g["x1"] + 0.6, g["y1"] + 0.6, 0.6, -2.4, -0.2, M["bezel"], 0.0)
    box("LcdGlass", g["x0"], g["y0"], g["x1"], g["y1"], -2.0, -1.4, M["lcd"], 0.0)
    # Auriculares.
    j = L["jack"]
    at(mesh("JackRing", lathe([(4.7, 0.0), (4.7, 1.4), (4.2, 1.9), (3.3, 1.9), (3.2, 0.4), (0.0, 0.4)], 64), M["bezel"]), j["x"], j["y"])
    at(mesh("JackHole", lathe([(3.2, 0.5), (0.0, 0.5)], 32), M["hole"]), j["x"], j["y"])
    # Botones: agujero oscuro alrededor y la tapa (en el fondo, sin apretar).
    for b in L["buttons"]:
        if b["kind"] == "hidden": continue
        at(mesh("BtnHole", lathe([(b["d"] / 2 + 0.35, 0.02), (0.0, 0.02)], 48), M["hole"]), b["x"], b["y"])
        at(button_cap(b), b["x"], b["y"])
    for l in L["leds"]:
        at(mesh("Led", lathe([(l["d"] / 2, 0.0), (l["d"] / 2, 0.6), (l["d"] / 2 - 0.4, 1.1), (0.0, 1.25)], 48), M["led_off"]), l["x"], l["y"])
        at(mesh("LedHole", lathe([(l["d"] / 2 + 0.4, 0.02), (0.0, 0.02)], 48), M["hole"]), l["x"], l["y"])
    # Llave de encendido: marco negro y tecla basculante.
    frame("PowerFrame", pw["x0"], pw["y0"], pw["x1"], pw["y1"], 1.6, -0.2, 1.4, M["bezel"], 0.4)
    box("PowerWell", pw["x0"] + 1.6, pw["y0"] + 1.6, pw["x1"] - 1.6, pw["y1"] - 1.6, -3.0, -0.5, M["hole"], 0.0)
    rk = box("Rocker", pw["x0"] + 2.2, pw["y0"] + 2.4, pw["x1"] - 2.2, pw["y1"] - 2.4, -1.5, 1.2, M["black_btn"], 0.8)
    rk.rotation_euler.x = math.radians(-6)

def frame_full(): return 0.0, 0.0, W, H

def mode_bg():
    build_static()
    bpy.context.scene.render.film_transparent = True
    x0, y0, x1, y1 = frame_full()
    rx, ry = camera(x0, y0, x1, y1 + 1.0)
    render(os.path.join(OUT, "bg.png"))
    json.dump(dict(width=rx, height=ry, pxmm=PXMM, x0=x0, y0=y0, theta=math.degrees(THETA),
                   cosT=math.cos(THETA), sinT=math.sin(THETA)), open(os.path.join(OUT, "bg.json"), "w"), indent=1)

# ───────── sprites ─────────
def sprite_scene():
    reset(); materials(); lights()
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=50.0)
    catcher = mesh("Catcher", bm, M["hole"], smooth=False); catcher.is_shadow_catcher = True
    bpy.context.scene.render.film_transparent = True

def sprite_camera(size_mm, lift_mm=0.0):
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("SpCam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = size_mm / 10.0
    cam.clip_start = 0.1; cam.clip_end = 1000
    o = bpy.data.objects.new("SpCam", cam); link(o)
    view = Vector((0, math.sin(THETA), -math.cos(THETA)))
    o.location = P(0, -lift_mm) - view * 300.0; o.rotation_euler = Euler((THETA, 0, 0), "XYZ"); sc.camera = o
    sc.render.resolution_x = int(round(size_mm * PXMM * 2)); sc.render.resolution_y = int(round(size_mm * PXMM * 2))
    return sc.render.resolution_x, sc.render.resolution_y

def sprite_meta(rx, ry, lift):
    return dict(w=rx, h=ry, size=rx, anchorX=rx / 2, anchorY=ry / 2 + lift * math.cos(THETA) * PXMM * 2)

def mode_sprites():
    meta = {"knobs": {}, "buttons": {}}
    for kind, build, sz, lift in (("big", big_knob, 26.0, -1.0), ("small", small_knob, 15.0, -0.6)):
        sprite_scene(); k = build()
        rx, ry = sprite_camera(sz, lift)
        os.makedirs(os.path.join(OUT, f"knob_{kind}"), exist_ok=True)
        for f in range(128):
            k.rotation_euler.z = -math.radians(360.0 * f / 128)
            render(os.path.join(OUT, f"knob_{kind}", f"{f:03d}.png"))
        meta["knobs"][kind] = dict(sprite_meta(rx, ry, lift), frames=128)
    for kind, d in (("grey", 4.8), ("black", 6.4)):
        for pressed in (False, True):
            sprite_scene()
            b = dict(kind=kind, d=d)
            mesh("BtnHole", lathe([(d / 2 + 0.35, 0.02), (0.0, 0.02)], 48), M["hole"])
            button_cap(b, pressed)
            rx, ry = sprite_camera(d + 6.0, 1.0)
            nm = f"btn_{kind}_{'down' if pressed else 'up'}"
            render(os.path.join(OUT, nm + ".png"))
            meta["buttons"][nm] = sprite_meta(rx, ry, 1.0)
    sprite_scene()
    l = L["leds"][0]
    mesh("Led", lathe([(l["d"] / 2, 0.0), (l["d"] / 2, 0.6), (l["d"] / 2 - 0.4, 1.1), (0.0, 1.25)], 48), M["led_on"])
    rx, ry = sprite_camera(9.0, 0.5); render(os.path.join(OUT, "led_on.png")); meta["led_on"] = sprite_meta(rx, ry, 0.5)
    json.dump(meta, open(os.path.join(OUT, "sprites.json"), "w"), indent=1)

# ───────── vistas en perspectiva (web y film) ─────────
def populate(values=None):
    values = values or {}
    for k in L["knobs"]:
        o = big_knob() if k["kind"] == "big" else small_knob()
        v = values.get(k["id"], 0.5)
        o.rotation_euler.z = -math.radians(-150 + 300 * v)
        at(o, k["x"], k["y"])
    for l in L["leds"][:1]:
        at(mesh("LedOn", lathe([(l["d"] / 2, 0.0), (l["d"] / 2, 0.6), (l["d"] / 2 - 0.4, 1.1), (0.0, 1.3)], 48), M["led_on"]), l["x"], l["y"])

def lcd_text_plane(path):
    """Imagen del LCD con texto (generada por el editor) sobre el cristal."""
    if not path or not os.path.exists(path): return
    c = L["lcd"]["glass"]
    m = bpy.data.materials.new("LcdText"); m.use_nodes = True; N = m.node_tree.nodes; Lk = m.node_tree.links
    b = N["Principled BSDF"]
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(os.path.abspath(path))
    Lk.new(tex.outputs["Color"], b.inputs["Base Color"])
    if "Emission Color" in b.inputs:
        Lk.new(tex.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.55
    b.inputs["Roughness"].default_value = 0.08
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = 1.0
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    vs = [bm.verts.new(P(x, y, -1.38)) for x, y in ((c["x0"], c["y0"]), (c["x1"], c["y0"]), (c["x1"], c["y1"]), (c["x0"], c["y1"]))]
    f = bm.faces.new(vs)
    for lp, uv in zip(f.loops, ((0, 1), (1, 1), (1, 0), (0, 0))): lp[uvl].uv = uv
    mesh("LcdText", bm, m, smooth=False)

def stage():
    """Mesa oscura apenas brillante bajo la unidad y fondo azul noche (sin horizonte)."""
    bm = bmesh.new()
    vs = [bm.verts.new(P(x, H + 0.4, z)) for x, z in ((-2500.0, -2500.0), (3000.0, -2500.0), (3000.0, 1500.0), (-2500.0, 1500.0))]
    bm.faces.new(vs)
    desk = mat("Desk", (0.012, 0.014, 0.018), 0.42, coat=0.12)
    mesh("Desk", bm, desk, smooth=False)
    w = bpy.context.scene.world.node_tree.nodes["Background"]
    w.inputs["Color"].default_value = (0.012, 0.016, 0.026, 1); w.inputs["Strength"].default_value = 0.6

def persp_camera(loc_mm, target_mm, lens=50.0, res=(2400, 1100), fstop=5.6):
    """Coordenadas de la vista en perspectiva (ver orient()): x a la derecha, y hacia el fondo (el frente
    mira a −y), z hacia arriba; el frente va de z = 0 (borde de arriba) a z = −44 mm."""
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("PCam"); cam.lens = lens; cam.clip_start = 1; cam.clip_end = 5000
    cam.dof.use_dof = True; cam.dof.aperture_fstop = fstop
    o = bpy.data.objects.new("PCam", cam); link(o); sc.camera = o
    o.location = Vector(loc_mm) / 10.0
    t = Vector(target_mm) / 10.0
    o.rotation_euler = (t - o.location).to_track_quat("-Z", "Y").to_euler()
    cam.dof.focus_distance = (t - o.location).length
    sc.render.resolution_x, sc.render.resolution_y = res
    return o

def orient():
    """Para las vistas en perspectiva el rack se para: lo que estaba en el plano XY (frente hacia +z) pasa a
    mirar a −y con +z arriba, así la cámara usa el "arriba" del mundo."""
    root = bpy.data.objects.new("Root", None); link(root)
    bpy.context.view_layer.update()
    for o in list(bpy.data.objects):
        if o is root or o.parent is not None or o.type in ("CAMERA", "LIGHT"): continue
        o.parent = root; o.matrix_parent_inverse = root.matrix_world.inverted()
    root.rotation_euler.x = math.radians(90)
    bpy.context.view_layer.update()

def hero_lights():
    target = Vector((241.0, 0.0, -22.0)) / 10.0
    for nm, loc, e, size, col in (("Soft", (-150, -420, 380), 16000, 500, (1.0, 0.96, 0.9)), ("Rim", (720, 260, 160), 5000, 260, (0.6, 0.75, 1.0)),
                                  ("Kick", (260, -120, 620), 5000, 320, (1, 1, 1)), ("Floor", (240, -300, -260), 1500, 400, (0.75, 0.82, 1.0))):
        l = bpy.data.lights.new(nm, "AREA"); l.energy = e; l.size = size / 10; l.color = col
        o = bpy.data.objects.new(nm, l); link(o); o.location = Vector(loc) / 10.0
        o.rotation_euler = (target - o.location).to_track_quat("-Z", "Y").to_euler()

def build_view(lcd_img=None):
    build_static(case=True)
    for o in [o for o in bpy.data.objects if o.type == "LIGHT"]: bpy.data.objects.remove(o)
    populate({"k1": 0.35, "k2": 0.55, "k3": 0.62, "k4": 0.45, "volume": 0.7})
    lcd_text_plane(lcd_img)
    stage()
    orient()
    hero_lights()
    bpy.context.scene.render.film_transparent = False

SHOTS = {
    # cámara (mm), objetivo (mm), lente, resolución
    "hero": ((560.0, -300.0, 95.0), (230.0, 0.0, -24.0), 46.0, (2400, 1100)),
    "knobs": ((395.0, -175.0, 55.0), (345.0, 0.0, -22.0), 62.0, (1600, 1000)),
    "lcd": ((170.0, -190.0, 48.0), (150.0, 0.0, -21.0), 55.0, (1600, 1000)),
    "front": ((241.0, -620.0, 60.0), (241.0, 0.0, -22.0), 50.0, (2400, 1100)),
}

def mode_hero():
    lcd = arg("--lcd")
    which = (arg("--shots") or "hero,knobs,lcd").split(",")
    for nm in which:
        build_view(lcd)
        loc, tgt, lens, res = SHOTS[nm]
        persp_camera(loc, tgt, lens, res)
        render(os.path.join(OUT, f"hero_{nm}.png"))

def mode_film():
    lcd = arg("--lcd")
    shot = arg("--shot", "pan"); frames = int(arg("--frames", "120"))
    build_view(lcd)
    os.makedirs(os.path.join(OUT, f"film_{shot}"), exist_ok=True)
    for f in range(frames):
        t = f / max(1, frames - 1); e = t * t * (3 - 2 * t)
        if shot == "pan":      # recorrido lateral: del LCD a las perillas
            loc = (120.0 + 300.0 * e, -230.0 + 30.0 * e, 50.0); tgt = (150.0 + 210.0 * e, 0.0, -22.0); lens = 50.0
        elif shot == "knobs":  # acercamiento a las perillas
            loc = (430.0 - 40.0 * e, -260.0 + 90.0 * e, 80.0 - 25.0 * e); tgt = (350.0, 0.0, -22.0); lens = 60.0
        else:                  # vista general que se aleja
            loc = (520.0 - 160.0 * e, -280.0 - 200.0 * e, 90.0 + 30.0 * e); tgt = (240.0, 0.0, -22.0); lens = 42.0
        for o in [o for o in bpy.data.objects if o.name.startswith("PCam")]: bpy.data.objects.remove(o)
        persp_camera(loc, tgt, lens, (1920, 1080))
        render(os.path.join(OUT, f"film_{shot}", f"f_{f + 1:04d}.png"))

# ───────── film de la web (filmkit): --mode film2 ─────────
# Coordenadas del film en mm del mundo (después de orient): x a la derecha, y hacia el fondo (el frente está en
# y = 0 y mira a −y), z hacia arriba (borde de arriba del frente en z = 0, el de abajo en z = −44).
FILM_SHOTS = [("open", 0.0, 4.0), ("pixels", 4.0, 4.0), ("buttons", 8.0, 4.0), ("knobs", 12.0, 4.0),
              ("hero", 16.0, 4.0), ("end", 20.0, 4.0)]
NOTES = [2.0 + 2.0 * k for k in range(10)]              # la frase dicha en diez alturas, una cada dos tiempos (60 BPM)

def V(x, y, z): return Vector((x / 10.0, y / 10.0, z / 10.0))
def FS(f): return f / 100.0                               # f-stop real → escena (1 unidad = 10 mm: el diafragma ×100)

# Fuente de 5 × 7 del LCD (la misma del editor, Source/ui/LookFs.h): ASCII 32..126, 5 columnas, bit 0 = fila de arriba.
GLYPH_HEX = (
    "000000000000005F00000007000700147F147F14242A7F2A12231308646236495620500005030000001C2241000041221C00"
    "2A1C7F1C2A08083E080800503000000808080808006060000020100804023E5149453E00427F400042615149462141454B31"
    "1814127F1027454545393C4A49493001710905033649494936064949291E0036360000005636000008142241001414141414"
    "00412214080201510906324979413E7E1111117E7F494949363E414141227F4141221C7F494949417F090909013E4149497A"
    "7F0808087F00417F41002040413F017F081422417F404040407F020C027F7F0408107F3E4141413E7F090909063E4151215E"
    "7F09192946464949493101017F01013F4040403F1F2040201F3F4038403F631408146307087008076151494543007F414100"
    "02040810200041417F0004020102044040404040000102040020545454787F484444383844444420384444487F3854545418"
    "087E0901020C5252523E7F0804047800447D40002040443D007F1028440000417F40007C041804787C080404783844444438"
    "7C14141408081414187C7C080404084854545420043F4440203C4040207C1C2040201C3C4030403C44281028440C5050503C"
    "4464544C44000836410000007F000000413608000201020402")
GLYPHS = [bytes.fromhex(GLYPH_HEX[i * 10:(i + 1) * 10]) for i in range(95)]
LNX, LNY, ROW1, ROW2 = 146, 38, 6, 16                   # puntos del cristal (0,59 mm) y filas de las dos líneas

class LcdSeq:
    """El LCD del film: una imagen de un píxel por punto (R = tinta, G = luz de fondo) por cuadro; el material la
    convierte en puntos cuadrados nítidos a cualquier distancia, con la sombra de cada punto sobre el reflector."""

    def __init__(self, folder):
        import numpy as np
        self.np, self.folder = np, folder; os.makedirs(folder, exist_ok=True)
        self.img = bpy.data.images.new("LcdBuf", LNX, LNY, alpha=True)

    def write(self, f, line1, line2, marks, backlight, cursor=None):
        np = self.np
        a = np.zeros((LNY, LNX, 4), np.float32); a[..., 1] = backlight; a[..., 3] = 1.0
        def put(row, ci, cols):
            for c in range(5):
                for r in range(7):
                    a[LNY - 1 - (row + r), 1 + ci * 6 + c, 0] = 1.0 if (cols[c] >> r) & 1 else 0.07
        for row, line in ((ROW1, line1), (ROW2, line2)):
            line = list(line) + [" "] * 24
            for ci in range(24):
                it = line[ci]
                if isinstance(it, str):
                    o = ord(it); put(row, ci, GLYPHS[o - 32] if 32 <= o <= 126 else GLYPHS[0])
                else:                                   # carácter propio: barra de 0 a 7 filas desde abajo
                    n = max(0, min(7, int(round(it)))); put(row, ci, [sum(1 << r for r in range(7 - n, 7))] * 5)
        if cursor:                                      # subrayado del campo bajo el cursor (fila 8 del carácter)
            row, c0, c1 = cursor
            a[LNY - 1 - (row + 7), 1 + c0 * 6:1 + c1 * 6 - 1, 0] = 1.0
        for lab in L["lcdLabels"]:                      # triángulos sobre los rótulos impresos bajo el cristal
            g = L["lcd"]["glass"]; cx = int((lab["x"] - g["x0"]) / (g["x1"] - g["x0"]) * LNX)
            v = 1.0 if marks.get(lab["text"]) else 0.07
            for k, w in enumerate((2, 1, 0)):
                a[LNY - 1 - (33 + k), cx - w:cx + w + 1, 0] = v
        self.img.pixels.foreach_set(a.ravel())
        self.img.filepath_raw = os.path.join(self.folder, f"lcd_{f:04d}.png"); self.img.file_format = "PNG"
        self.img.save()

def _sock(n, ident):
    return next(s for s in n.inputs if s.identifier == ident)

def lcd_film_material(first, frames):
    m = bpy.data.materials.new("LcdFilm"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    img = bpy.data.images.load(first); img.source = "SEQUENCE"; img.colorspace_settings.name = "Non-Color"
    uv = N.new("ShaderNodeTexCoord").outputs["UV"]
    def link(x, sock):
        if isinstance(x, (int, float)): sock.default_value = x
        elif isinstance(x, tuple): sock.default_value = x
        else: Lk.new(x, sock)
    def mth(op, a, b=0.0):
        n = N.new("ShaderNodeMath"); n.operation = op; link(a, n.inputs[0]); link(b, n.inputs[1]); return n.outputs[0]
    def vadd(a, off):
        n = N.new("ShaderNodeVectorMath"); n.operation = "ADD"; link(a, n.inputs[0]); n.inputs[1].default_value = off; return n.outputs[0]
    def tex(vec):
        t = N.new("ShaderNodeTexImage"); t.image = img; t.interpolation = "Closest"; t.extension = "EXTEND"
        t.image_user.frame_duration = frames; t.image_user.frame_start = 1; t.image_user.frame_offset = 0
        t.image_user.use_auto_refresh = True; Lk.new(vec, t.inputs["Vector"])
        sp = N.new("ShaderNodeSeparateColor"); Lk.new(t.outputs["Color"], sp.inputs[0]); return sp
    def dots(vec):                                      # 1 dentro del punto, 0 en la separación entre puntos
        s = N.new("ShaderNodeSeparateXYZ"); Lk.new(vec, s.inputs[0])
        dx = mth("ABSOLUTE", mth("SUBTRACT", mth("FRACT", mth("MULTIPLY", s.outputs[0], float(LNX))), 0.5))
        dy = mth("ABSOLUTE", mth("SUBTRACT", mth("FRACT", mth("MULTIPLY", s.outputs[1], float(LNY))), 0.5))
        mr = N.new("ShaderNodeMapRange"); Lk.new(mth("MAXIMUM", dx, dy), mr.inputs["Value"]); mr.clamp = True
        mr.inputs["From Min"].default_value = 0.36; mr.inputs["From Max"].default_value = 0.43
        mr.inputs["To Min"].default_value = 1.0; mr.inputs["To Max"].default_value = 0.0
        return mr.outputs["Result"]
    def mix(fac, a, bb):
        n = N.new("ShaderNodeMix"); n.data_type = "RGBA"; n.clamp_factor = True
        link(fac, _sock(n, "Factor_Float")); link(a, _sock(n, "A_Color")); link(bb, _sock(n, "B_Color"))
        return next(o for o in n.outputs if o.identifier == "Result_Color")
    t1 = tex(uv); ink = mth("MULTIPLY", t1.outputs["Red"], dots(uv))
    uv2 = vadd(uv, (0.22 / LNX, -0.30 / LNY, 0.0)); t2 = tex(uv2)
    shadow = mth("MULTIPLY", mth("MULTIPLY", t2.outputs["Red"], dots(uv2)), 0.32)
    # Luz de fondo: más clara en el centro (como el editor: lcdHi al centro, lcdLo en las esquinas).
    c = N.new("ShaderNodeVectorMath"); c.operation = "SUBTRACT"; Lk.new(uv, c.inputs[0]); c.inputs[1].default_value = (0.5, 0.5, 0.0)
    sc_ = N.new("ShaderNodeVectorMath"); sc_.operation = "MULTIPLY"; Lk.new(c.outputs[0], sc_.inputs[0]); sc_.inputs[1].default_value = (1.93, 0.51, 0.0)
    ln = N.new("ShaderNodeVectorMath"); ln.operation = "LENGTH"; Lk.new(sc_.outputs[0], ln.inputs[0])
    ramp = N.new("ShaderNodeValToRGB"); Lk.new(ln.outputs["Value"], ramp.inputs["Fac"])
    ramp.color_ramp.elements[0].color = (0.451, 0.610, 0.056, 1); ramp.color_ramp.elements[1].color = (0.296, 0.423, 0.025, 1)
    bl = t1.outputs["Green"]
    lit = mix(bl, (0.030, 0.034, 0.018, 1), ramp.outputs["Color"])
    INK = (0.0116, 0.0222, 0.0033, 1)
    col = mix(ink, mix(shadow, lit, INK), INK)
    Lk.new(col, b.inputs["Base Color"])
    Lk.new(mix(bl, (0, 0, 0, 1), col), b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 1.0
    b.inputs["Roughness"].default_value = 0.4
    if "Coat Weight" in b.inputs:
        b.inputs["Coat Weight"].default_value = 1.0; b.inputs["Coat Roughness"].default_value = 0.04
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = 0.25
    return m, b

def lcd_film_plane(m):
    c = L["lcd"]["glass"]
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    vs = [bm.verts.new(P(x, y, -1.38)) for x, y in ((c["x0"], c["y0"]), (c["x1"], c["y0"]), (c["x1"], c["y1"]), (c["x0"], c["y1"]))]
    f = bm.faces.new(vs)
    for lp, uv in zip(f.loops, ((0, 1), (1, 1), (1, 0), (0, 0))): lp[uvl].uv = uv
    return mesh("LcdFilm", bm, m, smooth=False)

# ── El rack: mejillas de nogal, rieles, tapas y unidades ciegas arriba y abajo ──
U = 44.45
RACK = [                                     # (posición en U respecto del ATFS1, alto en U, tipo)
        (-3, 2, "blank2"), (-1, 1, "vent"), (1, 1, "blank"), (2, 2, "vent2")]
RACK_Y0, RACK_Y1 = -3 * U - 12.0, 4 * U + 12.0       # de la tapa de arriba a la de abajo (mm, y del frente)

def walnut():
    m = bpy.data.materials.new("Walnut"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord"); mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (0.5, 6.0, 0.5)
    Lk.new(tc.outputs["Object"], mp.inputs["Vector"])
    wv = N.new("ShaderNodeTexWave"); wv.wave_type = "BANDS"; wv.bands_direction = "Y"
    wv.inputs["Scale"].default_value = 1.2; wv.inputs["Distortion"].default_value = 7.0; wv.inputs["Detail"].default_value = 4.0
    Lk.new(mp.outputs["Vector"], wv.inputs["Vector"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 40.0; Lk.new(mp.outputs["Vector"], nz.inputs["Vector"])
    r = N.new("ShaderNodeValToRGB"); Lk.new(wv.outputs["Fac"], r.inputs["Fac"])
    r.color_ramp.elements[0].color = (0.028, 0.012, 0.006, 1); r.color_ramp.elements[1].color = (0.11, 0.05, 0.022, 1)
    Lk.new(r.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.38
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = 0.35; b.inputs["Coat Roughness"].default_value = 0.18
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08; bump.inputs["Distance"].default_value = 0.002
    Lk.new(nz.outputs["Fac"], bump.inputs["Height"]); Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

def brushed_black():
    """Aluminio anodizado negro cepillado (frentes ciegos)."""
    m = bpy.data.materials.new("BlackPanel"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord"); mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (2.0, 260.0, 1.0)
    Lk.new(tc.outputs["Object"], mp.inputs["Vector"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 30.0; nz.inputs["Detail"].default_value = 8.0
    Lk.new(mp.outputs["Vector"], nz.inputs["Vector"])
    r = N.new("ShaderNodeMapRange"); Lk.new(nz.outputs["Fac"], r.inputs["Value"])
    r.inputs["To Min"].default_value = 0.34; r.inputs["To Max"].default_value = 0.5; Lk.new(r.outputs["Result"], b.inputs["Roughness"])
    b.inputs["Base Color"].default_value = (0.04, 0.041, 0.045, 1); b.inputs["Metallic"].default_value = 0.6
    if "Anisotropic" in b.inputs: b.inputs["Anisotropic"].default_value = 0.5
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.05; bump.inputs["Distance"].default_value = 0.001
    Lk.new(nz.outputs["Fac"], bump.inputs["Height"]); Lk.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

def build_rack():
    """Rack de escritorio de 7 U: el ATFS1 en el medio, unidades ciegas (lisas y ventiladas) arriba y abajo."""
    blk, wal = brushed_black(), walnut()
    coat = mat("Powder", (0.012, 0.012, 0.013), 0.62)
    made = []
    def keep(o): made.append(o); return o
    for (pos, n, kind) in RACK:
        y0 = pos * U; y1 = y0 + n * U - 0.45
        keep(box("Blank", 0.0, y0, W, y1, -2.0, 0.0, blk, 0.35))
        keep(box("BlankCase", 6.0, y0 + 0.6, W - 6.0, y1 - 0.6, -260.0, -2.0, M["case"], 1.0))
        for sx in (8.9, 473.7, 31.4, 453.2):
            for sy in ((4.7, n * U - 6.0) if n > 1 else (4.7, 38.0)):
                if sx in (31.4, 453.2) and kind.startswith("blank"): continue
                b0 = len(bpy.data.objects); screw(sx, y0 + sy); made.extend(list(bpy.data.objects)[b0:])
        if kind.startswith("vent"):                     # ranuras de ventilación verticales
            for i in range(34):
                x = 78.0 + i * 9.6
                keep(box("Slot", x - 1.6, y0 + 9.0, x + 1.6, y1 - 9.0, -1.2, 0.08, M["hole"], 0.0))
    # Rieles (sólo se ven en las ranuras entre frentes) y fondo negro.
    for x0, x1 in ((-6.0, 15.9), (466.7, W + 6.0)):
        keep(box("Rail", x0, RACK_Y0 + 12.0, x1, RACK_Y1 - 12.0, -4.6, -2.05, M["rail"], 0.2))
    keep(box("Back", -6.0, RACK_Y0 + 12.0, W + 6.0, RACK_Y1 - 12.0, -420.0, -400.0, coat, 0.0))
    # Mejillas de nogal y tapas.
    for x0, x1 in ((-26.0, -6.0), (W + 6.0, W + 26.0)):
        keep(box("Cheek", x0, RACK_Y0, x1, RACK_Y1, -420.0, 18.0, wal, 2.0))
    keep(box("Top", -6.0, RACK_Y0, W + 6.0, RACK_Y0 + 12.0, -420.0, 6.0, coat, 1.0))
    keep(box("Bottom", -6.0, RACK_Y1 - 12.0, W + 6.0, RACK_Y1, -420.0, 6.0, coat, 1.0))
    return made

def film_world(sc):
    """Estudio oscuro: mesa apenas brillante bajo el rack y pared del fondo con un charco de luz."""
    zdesk = -(RACK_Y1 + 0.2) / 10.0
    bm = bmesh.new(); bm.faces.new([bm.verts.new((x / 10, y / 10, zdesk)) for x, y in ((-3000, -3000), (3600, -3000), (3600, 1600), (-3000, 1600))])
    mesh("Desk", bm, mat("Desk", (0.016, 0.017, 0.02), 0.36, coat=0.15), smooth=False)
    bm = bmesh.new(); bm.faces.new([bm.verts.new((x / 10, 1600 / 10, z / 10)) for x, z in ((-3000, -400), (3600, -400), (3600, 2600), (-3000, 2600))])
    mesh("Wall", bm, mat("Wall", (0.05, 0.052, 0.058), 0.8), smooth=False)
    w = sc.world.node_tree.nodes["Background"]
    w.inputs["Color"].default_value = (0.010, 0.012, 0.018, 1); w.inputs["Strength"].default_value = 0.5

def alight(name, loc, look, size, energy, col=(1, 1, 1), size_y=None, spread=None):
    l = bpy.data.lights.new(name, "AREA"); l.energy = energy; l.color = col
    if size_y is None: l.size = size / 10.0
    else: l.shape = "RECTANGLE"; l.size = size / 10.0; l.size_y = size_y / 10.0
    if spread is not None: l.spread = math.radians(spread)
    o = bpy.data.objects.new(name, l); link(o); o.location = V(*loc)
    o.rotation_euler = (V(*look) - o.location).to_track_quat("-Z", "Y").to_euler()
    return o

def film_lights(sc, ex=1.0):
    """Llave suave arriba a la izquierda, relleno frío bajo a la derecha, luz cenital que roza el frente (el
    cepillado y las tapas de las perillas), una rasante fina pegada al borde de arriba y un charco en la pared."""
    Lg = {}
    Lg["key"] = alight("Key", (-60, -620, 420), (241, 0, -30), 600, 60000 * ex, (1.0, 0.95, 0.88), size_y=380)
    Lg["fill"] = alight("Fill", (760, -460, -90), (241, 0, -22), 600, 9000 * ex, (0.72, 0.82, 1.0))
    Lg["top"] = alight("Top", (241, -140, 330), (241, -6, -22), 700, 8000 * ex, (1.0, 0.98, 0.95), size_y=160)
    Lg["graze"] = alight("Graze", (241, -14, 46), (241, 6, -60), 560, 900 * ex, (1.0, 0.93, 0.84), size_y=10, spread=50)
    Lg["sweep"] = alight("Sweep", (-200, -260, 40), (-120, 0, -22), 40, 0.0, (1.0, 0.9, 0.8), size_y=360, spread=35)
    # Contraluces altas detrás de las mejillas: el canto del nogal y la tapa (no llegan al frente).
    Lg["rimL"] = alight("RimL", (-320, 260, 420), (-16, 0, -40), 400, 26000 * ex, (1.0, 0.86, 0.7))
    Lg["rimR"] = alight("RimR", (800, 260, 420), (498, 0, -40), 400, 26000 * ex, (1.0, 0.86, 0.7))
    sp = bpy.data.lights.new("Pool", "SPOT"); sp.energy = 4.0e6 * ex; sp.spot_size = math.radians(42); sp.spot_blend = 1.0
    sp.color = (0.78, 0.85, 1.0); sp.shadow_soft_size = 30.0
    o = bpy.data.objects.new("Pool", sp); link(o); o.location = V(241, 300, 1500)
    o.rotation_euler = (V(241, 1600, 250) - o.location).to_track_quat("-Z", "Y").to_euler(); Lg["pool"] = o
    return Lg

def build_film(lcd_first, frames):
    """El ATFS1 con perillas, LEDs y LCD propios del film, metido en el rack y parado (orient) con dos raíces:
    la de la unidad (que puede deslizarse) y la del rack."""
    build_static(case=True)
    for o in [o for o in bpy.data.objects if o.type == "LIGHT"]: bpy.data.objects.remove(o)
    knobs = {}
    for k in L["knobs"]:
        o = big_knob() if k["kind"] == "big" else small_knob(); at(o, k["x"], k["y"]); knobs[k["id"]] = o
    leds = {}
    for l in L["leds"]:
        m = mat("Led_" + l["id"], (0.35, 0.03, 0.02), 0.15, coat=1.0, emit=((1.0, 0.08, 0.03), 0.0))
        o = at(mesh("LedOn", lathe([(l["d"] / 2, 0.0), (l["d"] / 2, 0.6), (l["d"] / 2 - 0.4, 1.1), (0.0, 1.3)], 48), m), l["x"], l["y"])
        leds[l["id"]] = m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
    btns = {}
    for o in bpy.data.objects:
        if o.name.startswith("Btn") and not o.name.startswith("BtnHole"):
            for b in L["buttons"]:
                if abs(o.location.x * 10 - b["x"]) < 0.05 and abs(-o.location.y * 10 - b["y"]) < 0.05: btns[b["id"]] = o
    lm, lb = lcd_film_material(lcd_first, frames); lcd_film_plane(lm)
    bpy.context.view_layer.update()
    unit = [o for o in bpy.data.objects if o.parent is None and o.type not in ("CAMERA", "LIGHT")]
    rack = build_rack()
    roots = {}
    for nm, objs in (("UnitRoot", unit), ("RackRoot", [o for o in rack if o.parent is None])):
        r = bpy.data.objects.new(nm, None); link(r)
        for o in objs: o.parent = r
        r.rotation_euler.x = math.radians(90); roots[nm] = r
    bpy.context.view_layer.update()
    return dict(knobs=knobs, leds=leds, btns=btns, unit=roots["UnitRoot"], lcd=lb)

# ── Lo que pasa en el tiempo (t en segundos del film = segundos del audio) ──
PRESSES = [(8.0, "voice"), (8.5, "val_up"), (9.0, "val_up"), (9.5, "val_up"), (10.0, "val_up"), (10.5, "cur_up"), (11.0, "val_up")]
CUR_T = 10.5                                            # CURSOR+: del campo ALG al FB
KNOB_MOVES = {                                           # (t0, t1, desde, hasta): giros a mano, con aceleración suave
    "k1": [(18.0, 19.1, 0.35, 0.47)],
    "k3": [(12.05, 13.3, 0.62, 0.81), (14.6, 15.8, 0.81, 0.72)],
    "k4": [(14.05, 15.2, 0.45, 0.69)],
}
KNOB_BASE = {"k1": 0.35, "k2": 0.55, "k3": 0.62, "k4": 0.45, "volume": 0.7}
KNOB_NAME = {"k1": "ATTACK", "k2": "RELEASE", "k3": "FORMANT", "k4": "FM"}

def knob_value(kid, t):
    v = KNOB_BASE[kid]
    for (t0, t1, a, b) in KNOB_MOVES.get(kid, []):
        if t >= t0: v = a + (b - a) * fk_ease((t - t0) / (t1 - t0))
    return v

def fk_ease(u):
    u = min(1.0, max(0.0, u)); return 4 * u ** 3 if u < 0.5 else 1 - (-2 * u + 2) ** 3 / 2

def press_depth(kid, t):
    """Hundimiento (mm) de un botón: baja en 2 cuadros, queda 4 y vuelve en 4."""
    d = 0.0
    for (tp, b) in PRESSES:
        if b != kid: continue
        x = t - tp
        if 0 <= x < 0.09: d = max(d, x / 0.09)
        elif 0.09 <= x < 0.25: d = max(d, 1.0)
        elif 0.25 <= x < 0.42: d = max(d, 1.0 - fk_ease((x - 0.25) / 0.17))
    return d * 1.0

def lcd_state(t, env, ons):
    """Texto del LCD en el segundo t: arranque, PLAY, la página del Fseq, la edición de la voz y lo que se gira."""
    fi = min(len(env) - 1, max(0, int(round(t * 24))))
    e = env[fi]
    last = max([o / 24.0 for o in ons if o / 24.0 <= t + 1e-6] or [-9.0])
    midi = (t - last) < 0.14
    marks = {"PART": True, "BANK/PGM#": True, "VOL": True, "REV": True, "MIDI": midi}
    voices = 1 if e > 0.04 else 0
    alg = 1 + sum(1 for (tp, b) in PRESSES if b == "val_up" and tp <= t and tp < CUR_T)
    fb = sum(1 for (tp, b) in PRESSES if b == "val_up" and tp <= t and tp > CUR_T)
    dirty = t >= 9.0
    l1 = "P001 Talking Formants" + ("*" if dirty else "")
    play2 = ("ALG%02d FB%d AUDIO TO" % (alg, fb)).ljust(19) + ("%2dV" % voices)
    if t < 0.95: return " ", " ", {}, 0.0, None
    if t < 1.5:                                         # al encender: la primera línea en bloques, con parpadeo
        k = int((t - 0.95) * 24)
        bl = [0.35, 0.0, 0.8, 0.25, 1.0, 0.7, 1.0][k] if k < 7 else 1.0
        return [7] * 24, " ", {}, bl, None
    if t < 2.0: return "      ATFS1", "FORMANT / FM SYNTHESIS", {}, 1.0, None
    if t < 4.0: return l1, play2, marks, 1.0, None
    if t < 8.0:                                         # página del Fseq: cuadro que avanza a 126 por segundo y las
        fr = min(190, int(max(0.0, t - last) * 126)) if last > 0 else 0      # ocho formantes con su respiración
        bars, ee = [], min(1.0, 1.8 * e)
        for i in range(8):
            s = 0.5 + 0.5 * math.sin(2.1 * i + 9.0 * (t - last) * (0.7 + 0.13 * i) + 1.7 * math.sin(3.3 * t + i))
            bars.append(7.0 * ee * (0.3 + 0.7 * s) * (1.0 - 0.05 * i))
        brt = 7.0 * ee * (0.4 + 0.6 * (0.5 + 0.5 * math.sin(17.0 * t)))
        return "FR%03d FSEQ AUDIO TOOLS" % fr, bars + [" ", "V"] + [7.0 * ee] + [" ", "N", brt], marks, 1.0, None
    if t < 12.0:                                        # EDIT VOICE: el algoritmo sube con VALUE+, después el feedback
        blink = int(t * 4) % 2 == 0
        cur = (ROW2, 0, 5) if t < CUR_T else (ROW2, 6, 9)
        return l1, ("ALG%02d FB%d" % (alg, fb)).ljust(13) + "EDIT VOICE", marks, 1.0, cur if blink else None
    # Desde 12 s: PLAY; mientras se gira una perilla, la segunda línea muestra su valor.
    for kid, mv in KNOB_MOVES.items():
        for (t0, t1, a, b) in mv:
            if t0 <= t <= t1 + 0.6:
                v = knob_value(kid, t)
                val = ("%+d" % int(round((v - 0.5) * 128))) if kid == "k3" else str(int(round(v * 127)))
                return l1, (KNOB_NAME[kid] + " ").ljust(18) + val.rjust(4), marks, 1.0, None
    return l1, play2, marks, 1.0, None

def film2():
    """Film de la web del ATFS1: 24 s a 60 BPM sobre la frase "Audio Tools" del Fseq, dicha en diez alturas (una
    cada dos tiempos, desde los 2 s). Todos los cortes caen sobre una sílaba. El ATFS1 vive en un rack de escritorio
    de 7 U con mejillas de nogal y unidades ciegas, sobre una mesa oscura.

    open    0–4 s   Oscuridad; una luz rasante pegada al borde de arriba despierta el cepillado de derecha a
                    izquierda. A los 0,95 s el LCD se enciende con su parpadeo y la línea de bloques, a 1,5 s
                    "ATFS1 / FORMANT / FM SYNTHESIS", y con la primera sílaba (2 s) la pantalla de PLAY. Cámara a la
                    derecha y arriba que se acerca despacio; el LCD queda arriba a la derecha y el tercio de abajo a
                    la izquierda en penumbra para el título.
    pixels  4–8 s   Macro de los puntos del LCD, en diagonal: la página del Fseq con el número de cuadro que corre a
                    126 por segundo y las ocho barras de formantes (más voz y ruido) que se mueven con la frase. Foco
                    que pasa del contador a las barras; cámara en mano muy sutil y un leve travelling.
    buttons 8–12 s  Contrapicado desde la derecha que se desliza hacia el LCD: VOICE (8 s), VALUE+ cuatro veces
                    (ALG01 → ALG05, con el cursor parpadeando bajo el campo), CURSOR+ y VALUE+ (FB1), cada medio
                    tiempo. El foco sigue al botón que se aprieta y al final pasa al LCD, que muestra "ALG05 FB1".
    knobs   12–16 s Contrapicado sobre las cuatro perillas: FORMANT gira con la sílaba de los 12 s, FM con la de los
                    14 s (el foco pasa de una a otra), FORMANT vuelve un poco. El LED de abajo late con cada nota.
    hero    16–20 s Vista heroica 3/4 baja que sube: la unidad termina de entrar al rack (deslizándose 40 mm) y una
                    luz recorre el frente de izquierda a derecha; ATTACK gira con la sílaba de los 18 s.
    end     20–24 s De frente, la cámara se aleja despacio y muestra el rack entero; PART+ ya cambió el preset.
                    film.py lo oscurece y pone el cierre.
    """
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final"); step = int(arg("--step", "1"))
    env = fk.envelope(arg("--audio"))
    ons = fk.onsets(env, thresh=0.2, gap=12)
    print("onsets (s):", [round(o / 24, 2) for o in ons])
    for (name, t0, dur) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        frames = int(round(dur * 24))
        lcd = LcdSeq(os.path.join(OUT, "_lcd", name))
        for f in range(1, frames + 1):
            l1, l2, mk, bl, cur = lcd_state(t0 + (f - 1) / 24.0, env, ons)
            lcd.write(f, l1, l2, mk, bl, cur)
        R = build_film(os.path.join(OUT, "_lcd", name, "lcd_0001.png"), frames)
        sc = bpy.context.scene
        film_world(sc)
        Lg = film_lights(sc)
        fk.setup(sc, quality)
        sc.view_settings.look = "AgX - Medium High Contrast"
        sc.view_settings.exposure = SHOT_EXPOSURE.get(name, 0.0)
        cam = fk.camera(sc)
        knobs, btns, unit = R["knobs"], R["btns"], R["unit"]
        paths = film_paths()
        path, focus_fn, shake = paths[name]

        def pose(f, t, u, name=name, t0=t0, path=path, focus_fn=focus_fn, shake=shake):
            T = t0 + t
            for k in L["knobs"]:
                knobs[k["id"]].rotation_euler.z = -math.radians(-150 + 300 * knob_value(k["id"], T))
            for b in L["buttons"]:
                if b["id"] in btns: btns[b["id"]].location.z = -press_depth(b["id"], T) / 10.0
            unit.location.y = 0.0
            if name == "hero": unit.location.y = -4.0 * (1.0 - fk.ease(t / 1.7, "out"))
            sw = Lg["sweep"]
            if name == "hero":
                x = -150 + 800 * fk.ease(u, "smooth")
                sw.location = V(x, -260, 30); sw.rotation_euler = (V(x + 90, 0, -22) - sw.location).to_track_quat("-Z", "Y").to_euler()
            path.apply(cam, u, focus=focus_fn(u, T), shake=shake, t=T)
            return list(knobs.values()) + list(btns.values()) + [unit, sw]

        def keys(f, name=name, t0=t0, frames=frames):
            T = t0 + (f - 1) / 24.0; u = (f - 1) / max(1, frames - 1)
            fi = min(len(env) - 1, int(round(T * 24)))
            last = max([o / 24.0 for o in ons if o / 24.0 <= T + 1e-6] or [-9.0])
            top = R["leds"]["led_top"]; bot = R["leds"]["led_bot"]
            boot = 1.0 if T >= 1.5 else (0.0 if T < 0.95 else float(int(T * 12) % 2))
            top.default_value = 7.0 * boot; top.keyframe_insert("default_value")
            bot.default_value = 0.4 + 16.0 * math.exp(-max(0.0, T - last) / 0.16) * (1.0 if last > 0 else 0.0)
            bot.keyframe_insert("default_value")
            g = Lg["graze"].data
            g.energy = 900.0 * (fk.ease(T / 1.8, "inout") if name == "open" else 1.0); g.keyframe_insert("energy")
            k = Lg["key"].data                          # en la apertura la llave también sube desde la penumbra
            k.energy = 60000.0 * (0.2 + 0.8 * fk.ease(T / 2.2, "inout") if name == "open" else 1.0); k.keyframe_insert("energy")
            s = Lg["sweep"].data
            s.energy = 1200.0 * math.sin(math.pi * min(1.0, max(0.0, u))) ** 2 if name == "hero" else 0.0
            s.keyframe_insert("energy")

        if arg("--check"):                              # sólo arma la escena y prueba la pose (sin GPU)
            for f in (1, frames): pose(f, (f - 1) / 24.0, (f - 1) / max(1, frames - 1)); keys(f)
            print("ok", name, len(bpy.data.objects), "objetos"); continue
        fk.render_shot(sc, name, frames, pose, OUT, step=step, extra_keys=keys)

SHOT_EXPOSURE = {"pixels": -0.45, "buttons": -0.45, "knobs": -0.4, "open": -0.1, "hero": 0.6, "end": 0.6}
L_k = {k["id"]: (k["x"], k["y"]) for k in L["knobs"]}

def film_paths():
    """Recorrido de cámara, foco y cámara en mano por toma (mm del mundo, ver la nota de arriba)."""
    import filmkit as fk
    g = L["lcd"]["glass"]
    def knob(kid, d=-12.0): return V(L_k[kid][0], d, -L_k[kid][1])
    def btn(bid):
        b = next(b for b in L["buttons"] if b["id"] == bid); return V(b["x"], -2.0, -b["y"])
    lcdc = V((g["x0"] + g["x1"]) / 2, 1.4, -(g["y0"] + g["y1"]) / 2)
    def lcdpt(col, row): return V(g["x0"] + (col + 0.5) / LNX * (g["x1"] - g["x0"]), 1.38, -(g["y0"] + (row + 0.5) / LNY * (g["y1"] - g["y0"])))
    def btn_focus(u, T):                                # VOICE, después VALUE+ y CURSOR+ y, al final, el LCD
        p = btn("voice").lerp(btn("val_up"), fk.rack(T, 8.15, 8.45, 0.0, 1.0))
        p = p.lerp(btn("cur_up"), fk.rack(T, 10.2, 10.45, 0.0, 1.0) - fk.rack(T, 10.7, 10.95, 0.0, 1.0))
        return p.lerp(lcdpt(40, ROW2 + 3), fk.rack(T, 11.25, 11.8, 0.0, 1.0))
    return {
        "open": (fk.Path([(0, V(272, -190, 22), V(122, 0, -31), 55, FS(4.0)),
                          (1, V(246, -158, 14), V(118, 0, -30), 58, FS(4.0))], easing="out"),
                 lambda u, T: lcdpt(60, 9), 0.035),
        "pixels": (fk.Path([(0, V(140, -80, -4), V(109, 1.4, -20.5), 50, FS(8.0)),
                            (1, V(131, -75, -9), V(114, 1.4, -21.0), 50, FS(8.0))], easing="inout"),
                   lambda u, T: lcdpt(16, ROW1 + 3).lerp(lcdpt(24, ROW2 + 3), fk.rack(u, 0.3, 0.6, 0.0, 1.0)), 0.012),
        "buttons": (fk.Path([(0, V(318, -128, -62), V(206, 0, -24), 54, FS(4.0)),
                             (1, V(296, -116, -52), V(192, 0, -23), 54, FS(4.0))], easing="inout"),
                    btn_focus, 0.03),
        "knobs": (fk.Path([(0, V(480, -160, -80), V(348, -6, -22), 60, FS(4.0)),
                           (1, V(466, -146, -72), V(342, -6, -22), 60, FS(4.0))], easing="inout"),
                  lambda u, T: knob("k3").lerp(knob("k4"), fk.rack((T - 12.0) / 4.0, 0.42, 0.58, 0.0, 1.0)), 0.03),
        "hero": (fk.Path([(0, V(570, -320, -75), V(250, 0, -27), 40, FS(5.6)),
                          (1, V(525, -285, 5), V(238, 0, -24), 40, FS(5.6))], easing="inout"),
                 lambda u, T: V(300, -8, -22), 0.0),
        "end": (fk.Path([(0, V(241, -700, -40), V(241, 0, -26), 42, FS(8.0)),
                         (1, V(241, -980, 10), V(241, 0, -22), 42, FS(8.0))], easing="out"),
                lambda u, T: V(241, 0, -22), 0.0),
    }

for m in (arg("--mode", "bg")).split(","):
    {"bg": mode_bg, "sprites": mode_sprites, "hero": mode_hero, "film": mode_film, "film2": film2}[m]()
