"""Escena Blender del ATFS1: el frente de rack 1U (fondo del editor), sprites de los controles y
vistas en perspectiva para la web.

Corre en pc50-wsl:
  blender -b -P scenefs1.py -- --layout layoutfs1.json --silk silkscreenfs1.png --out outfs1 --mode bg,sprites --samples 160
  blender -b -P scenefs1.py -- --mode hero --out outfs1
  blender -b -P scenefs1.py -- --mode film --shot pan --frames 120 --out outfs1

1 unidad de Blender = 10 mm. Punto del frente (x, y) mm → (x/10, −y/10, z/10); z sale del frente.
Cámara ortográfica inclinada THETA: en pantalla sx = x − x0, sy = (y − y0)·cosθ − z·sinθ (mm).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Euler

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d

L = json.load(open(arg("--layout", "layoutfs1.json")))
SILK = os.path.abspath(arg("--silk", "silkscreenfs1.png"))
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

for m in (arg("--mode", "bg")).split(","):
    {"bg": mode_bg, "sprites": mode_sprites, "hero": mode_hero, "film": mode_film}[m]()
