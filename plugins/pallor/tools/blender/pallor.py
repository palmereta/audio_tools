"""Pallor renderizado: una caja de ritmos propia (no la de ningún fabricante), vista desde arriba con cámara ortográfica
para la interfaz, y en perspectiva para las fotos y el film de la web.

Diseño: una losa de aluminio anodizado negro, de cantos gruesos y chaflanados; arriba una pantalla grande de vidrio
oscuro en un bisel de aluminio cepillado; abajo una canaleta fresada revestida de cerámica color hueso con 16 pads de
silicona pálida (la luz rosa sale de adentro); entre las dos, la fila de ocho perillas de la voz elegida (pollera
moleteada grafito, tapa de cerámica hueso) y teclas de goma con su lente de LED. Cuatro tornillos Allen en las esquinas.
Sin marcas: la serigrafía dice "PALLOR" y "Audio Tools".

Salidas del modo ui (lo que dibuja la interfaz, 8 px/mm; la interfaz trabaja a 4 px/mm):
  bg.jpg       el panel entero, sin tapas de perilla, pads apagados, teclas arriba, pantalla vacía, sin serigrafía
  lit.jpg      el mismo encuadre con los pads y las lentes encendidos (la interfaz recorta cada pad con su brillo)
  down.jpg     el mismo encuadre con pads y teclas apretados
  cap_big.png, cap_small.png   tapas de perilla (fondo transparente); la interfaz dibuja el índice y el arco
  layout.json  dónde está cada cosa (px de bg.jpg) y la serigrafía (texto, tamaño, alineación)

  B=/Volumes/Docs/Applications/blender/Blender.app/Contents/MacOS/Blender      (o el de pc50-wsl, bajo el lock)
  $B -b --factory-startup -P pallor.py -- --out <dir> [--samples 256] [--only bg,lit,down,caps]
  $B -b --factory-startup -P pallor.py -- --mode photos --screen <ui-screen.png> --out <dir> [--shots hero,close,top,knobs,side]
  $B -b --factory-startup -P pallor.py -- --mode film2 --screen <png> --audio <wav> --out <dir> [--quality final] [--shot a,b]
  python3 pallor.py --mode filmjson      (fuera de Blender: escribe tools/film.json)

Unidades: mm; el origen es la esquina superior izquierda del panel, x a la derecha, y hacia abajo (en Blender y se
invierte).
"""
import math, os, sys, json
try:
    import bpy
    from mathutils import Vector
except ImportError:                 # fuera de Blender sólo sirve --mode filmjson
    bpy = None

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out"))
if bpy: os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "192"))
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
FONT = os.path.join(ROOT, "plugins", "lacquer", "assets", "fonts", "BarlowCondensed-SemiBold.ttf")
FONT_M = os.path.join(ROOT, "plugins", "lacquer", "assets", "fonts", "BarlowCondensed-Medium.ttf")
PX_PER_MM = 8.0
W, H = 360.0, 235.0

# ── Disposición (mm) ──
VOICES = ["Kick", "Snare", "Clap", "Rim", "Closed Hat", "Open Hat", "Crash", "Ride",
          "Tom 1", "Tom 2", "Tom 3", "Tom 4", "Cowbell", "Clave", "Tamb", "FX"]
KNOBS = [("tune", "Tune"), ("decay", "Decay"), ("tone", "Tone"), ("char1", "Click"), ("char2", "Glide"),
         ("drive", "Drive"), ("cutoff", "Cutoff"), ("level", "Level")]
L = {
    "display": {"x": 98.0, "y": 6.0, "w": 150.0, "h": 13.0},
    "screen": {"x": 7.0, "y": 27.0, "w": 346.0, "h": 125.0, "bezel": 1.6},
    "header_knobs": [{"id": "tempo", "label": "Tempo", "x": 299.0, "y": 12.0, "r": 6.0},
                     {"id": "swing", "label": "Swing", "x": 320.0, "y": 12.0, "r": 6.0},
                     {"id": "m_volume", "label": "Volume", "x": 341.0, "y": 12.0, "r": 6.0}],
    "knobs": [{"id": k, "label": lab, "x": 70.0 + 32.5 * i, "y": 171.5, "r": 8.5} for i, (k, lab) in enumerate(KNOBS)],
    "keys": [{"id": "prev", "label": "<", "x": 85.0, "y": 7.5, "w": 10.0, "h": 10.0},
             {"id": "next", "label": ">", "x": 251.0, "y": 7.5, "w": 10.0, "h": 10.0},
             {"id": "manual", "label": "Manual", "x": 266.0, "y": 8.0, "w": 20.0, "h": 9.0},
             {"id": "run", "label": "Run", "x": 9.0, "y": 164.0, "w": 20.0, "h": 14.0},
             {"id": "fill", "label": "Fill", "x": 32.0, "y": 164.0, "w": 20.0, "h": 14.0},
             {"id": "mute", "label": "Mute", "x": 310.0, "y": 164.0, "w": 20.0, "h": 14.0},
             {"id": "solo", "label": "Solo", "x": 333.0, "y": 164.0, "w": 20.0, "h": 14.0}],
    "trough": {"x": 6.0, "y": 194.0, "w": 348.0, "h": 34.0},
    "pads": [],
}
PITCH = 346.0 / 16
for i in range(16):
    L["pads"].append({"x": 7.0 + PITCH * i + (PITCH - 18.6) / 2, "y": 197.5, "w": 18.6, "h": 18.6, "name": VOICES[i]})

# Serigrafía estática (la interfaz la dibuja como texto vectorial; las fotos la graban en el panel).
INK, DIM = "ink", "dim"
SILK = [
    {"t": "PALLOR", "x": 9.0, "y": 12.6, "size": 9.0, "align": "LEFT", "c": INK, "track": 0.18, "font": "semi"},
    {"t": "AUDIO TOOLS  /  DRUM MACHINE", "x": 9.3, "y": 20.0, "size": 2.6, "align": "LEFT", "c": DIM, "track": 0.18},
    {"t": "VOICE", "x": 70.0 - 8.5, "y": 158.6, "size": 2.5, "align": "LEFT", "c": DIM, "track": 0.2},
    {"t": "TRANSPORT", "x": 9.0, "y": 158.6, "size": 2.5, "align": "LEFT", "c": DIM, "track": 0.2},
    {"t": "CHANNEL", "x": 310.0, "y": 158.6, "size": 2.5, "align": "LEFT", "c": DIM, "track": 0.2},
]
for k in L["header_knobs"]:
    SILK.append({"t": k["label"].upper(), "x": k["x"], "y": k["y"] + k["r"] + 3.4, "size": 2.4, "align": "CENTER", "c": DIM, "track": 0.14})
for k in L["keys"]:
    if k["id"] in ("prev", "next"): continue
    SILK.append({"t": k["label"].upper(), "x": k["x"] + k["w"] / 2, "y": k["y"] + k["h"] * 0.62, "size": 2.6, "align": "CENTER", "c": INK, "track": 0.14, "on": "key"})

COL = {
    "plate": (0.0105, 0.0112, 0.0128), "edge": (0.05, 0.052, 0.058), "bezel": (0.36, 0.365, 0.375),
    "bone": (0.44, 0.415, 0.37), "capbone": (0.66, 0.625, 0.56), "pad": (0.70, 0.655, 0.63), "rose": (1.0, 0.13, 0.36), "padglow": (1.0, 0.05, 0.24), "ink": (0.78, 0.75, 0.69),
    "dim": (0.40, 0.395, 0.38), "rubber": (0.022, 0.022, 0.024), "glass": (0.004, 0.0045, 0.006),
}

def bl(x, y, z=0.0):
    """mm del panel → metros de Blender (y invertida)."""
    return Vector((x * 0.001, -y * 0.001, z * 0.001))

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("CUDA", "METAL", "HIP"):                  # OptiX no arranca en WSL
        try:
            prefs.compute_device_type = kind; prefs.get_devices()
        except TypeError:
            continue
        if any(d.type == kind for d in prefs.devices):
            for d in prefs.devices: d.use = d.type == kind
            break
    sc.cycles.device = "GPU" if arg("--cpu", "0") != "1" else "CPU"
    sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    try: sc.cycles.denoising_use_gpu = True
    except AttributeError: pass
    sc.cycles.use_adaptive_sampling = True; sc.cycles.adaptive_threshold = 0.01
    sc.view_settings.view_transform = "AgX"
    try: sc.view_settings.look = "AgX - Base Contrast"
    except TypeError: pass
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    return sc

def mat(name, base, rough, metal=0.0, aniso=0.0, coat=0.0, emit=None, strength=0.0, trans=0.0, sss=0.0, spec=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if aniso: b.inputs["Anisotropic"].default_value = aniso
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.08
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    if trans: b.inputs["Transmission Weight"].default_value = trans
    if sss:
        b.inputs["Subsurface Weight"].default_value = sss; b.inputs["Subsurface Radius"].default_value = (2.0, 1.2, 1.0)
        b.inputs["Subsurface Scale"].default_value = 0.0015
    b.inputs["Specular IOR Level"].default_value = spec
    return m

def brushed(name, base, rough, aniso):
    """Aluminio cepillado: ruido estirado en x que mueve la rugosidad (el cepillado se ve en los reflejos)."""
    m = mat(name, base, rough, metal=1.0, aniso=aniso)
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (60.0, 6000.0, 60.0)
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 1.0; nz.inputs["Detail"].default_value = 8.0
    mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs["To Min"].default_value = rough * 0.75; mr.inputs["To Max"].default_value = rough * 1.3
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"]); nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
    nt.links.new(nz.outputs["Fac"], mr.inputs["Value"]); nt.links.new(mr.outputs["Result"], b.inputs["Roughness"])
    return m

def box(name, x, y, w, h, depth, z0, r, m, segs=6):
    """Caja de esquinas redondeadas (r en mm) de z0 a z0+depth."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=bl(x + w / 2, y + h / 2, z0 + depth / 2))
    o = bpy.context.active_object; o.name = name
    o.scale = (w * 0.001, h * 0.001, depth * 0.001); bpy.ops.object.transform_apply(scale=True)
    if r > 0:
        md = o.modifiers.new("b", "BEVEL"); md.width = min(r, depth * 0.49, w * 0.49, h * 0.49) * 0.001; md.segments = segs; md.limit_method = "ANGLE"
    o.data.materials.append(m); bpy.ops.object.shade_smooth()
    return o

def cut(target, cutter):
    md = target.modifiers.new("cut", "BOOLEAN"); md.object = cutter; md.operation = "DIFFERENCE"; md.solver = "EXACT"
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier="cut")
    bpy.data.objects.remove(cutter, do_unlink=True)
    # Sombreado suave por ángulo: el suave de todo el objeto deja degradés falsos en los triángulos largos del booleano.
    for o in bpy.context.selected_objects: o.select_set(False)
    target.select_set(True); bpy.context.view_layer.objects.active = target
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(35))

def cyl(name, x, y, r, depth, z0, m, verts=96, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r * 0.001, depth=depth * 0.001, location=bl(x, y, z0 + depth / 2))
    o = bpy.context.active_object; o.name = name; o.data.materials.append(m)
    if bevel > 0:
        md = o.modifiers.new("b", "BEVEL"); md.width = bevel * 0.001; md.segments = 4; md.limit_method = "ANGLE"
    bpy.ops.object.shade_smooth()
    return o

LABELS = arg("--labels", "0") == "1"      # ui: sin serigrafía (la dibuja la interfaz); fotos y film: con

def text(s, x, y, size, m, align="LEFT", z=0.02, track=0.0, font=FONT):
    cu = bpy.data.curves.new("t", "FONT"); cu.body = s; cu.size = size * 0.001 * 1.38; cu.align_x = align; cu.align_y = "CENTER"
    if os.path.exists(font): cu.font = bpy.data.fonts.load(font, check_existing=True)
    cu.space_character = 1.0 + track; cu.extrude = 0.000012
    o = bpy.data.objects.new("t", cu); bpy.context.scene.collection.objects.link(o)
    o.location = bl(x, y, z); cu.materials.append(m)
    return o

def silkscreen(ink, dim, key_ink, knob_labels=None):
    dark = bpy.data.materials.get("darkink") or mat("darkink", (0.07, 0.065, 0.06), 0.6)
    for s in SILK:
        m = key_ink if s.get("on") == "key" else (ink if s["c"] == INK else dim)
        z = 0.01 if s.get("on") != "key" else 3.22
        text(s["t"], s["x"], s["y"], s["size"], m, s["align"], z=z, track=s.get("track", 0.0), font=FONT if s.get("font") == "semi" else FONT_M)
    for k in L["knobs"]:
        lab = (knob_labels or {}).get(k["id"], k["label"])
        text(lab.upper(), k["x"], k["y"] + k["r"] + 5.0, 2.7, ink, "CENTER", z=0.01, track=0.14, font=FONT_M)
    for p in L["pads"]:
        text(p["name"].upper(), p["x"] + p["w"] / 2, p["y"] + p["h"] + 4.6, 2.3, dark, "CENTER", z=0.01 - 1.88, track=0.12, font=FONT_M)
        text("%02d" % (L["pads"].index(p) + 1), p["x"] + 1.0, p["y"] - 1.6, 1.9, dark, "LEFT", z=0.01 - 1.88, track=0.1, font=FONT_M)

def knob_cap(name, r):
    """Perilla: pollera moleteada grafito (0 a 0,75 r), anillo de aluminio cepillado (hasta 0,95 r) y una tapa de
    cerámica color hueso apenas abombada. Todo cuelga de un vacío en el eje (girarlo gira la perilla)."""
    root = bpy.data.objects.new(name, None); bpy.context.scene.collection.objects.link(root)
    skirt = mat(name + "_skirt", (0.028, 0.029, 0.032), 0.42, metal=0.6)
    parts = [cyl(name + "_s", 0, 0, r, r * 0.75, 0, skirt, verts=144, bevel=r * 0.05)]
    for k in range(48):
        a = 2 * math.pi * k / 48
        bpy.ops.mesh.primitive_cube_add(size=1, location=(math.cos(a) * r * 0.00099, math.sin(a) * r * 0.00099, r * 0.00036))
        g = bpy.context.active_object; g.scale = (r * 0.00012, r * 0.000055, r * 0.00066); g.rotation_euler = (0, 0, a)
        g.data.materials.append(skirt); parts.append(g)
    ring = mat(name + "_ring", (0.55, 0.555, 0.565), 0.22, metal=1.0, aniso=0.5)
    parts.append(cyl(name + "_r", 0, 0, r * 0.80, r * 0.95, 0, ring, verts=144, bevel=r * 0.035))
    top = mat(name + "_top", COL["capbone"], 0.5, sss=0.05)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=r * 0.00074, location=(0, 0, r * 0.00095))
    t = bpy.context.active_object; t.scale.z = 0.11; t.data.materials.append(top); bpy.ops.object.shade_smooth(); parts.append(t)
    for o in parts: o.parent = root
    return root
KNOB_TOP = 1.03        # altura de la tapa en radios (el índice va encima)

PRESSED = False

def build(state="bg"):
    """state: bg (todo apagado), lit (pads y lentes encendidos), down (pads y teclas apretados), photo."""
    sc = reset()
    plate = brushed("plate", COL["plate"], 0.34, 0.35)
    edge = mat("edge", COL["edge"], 0.3, metal=1.0)
    bezel = brushed("bezel", COL["bezel"], 0.22, 0.7)
    glass = mat("glass", COL["glass"], 0.32, coat=0.35, spec=0.25)
    glass.node_tree.nodes["Principled BSDF"].inputs["Coat Roughness"].default_value = 0.22
    bone = mat("bone", COL["bone"], 0.62, sss=0.08)
    rubber = mat("rubber", COL["rubber"], 0.72)
    ink = mat("ink", COL["ink"], 0.55); dim = mat("dim", COL["dim"], 0.6); key_ink = mat("keyink", (0.62, 0.6, 0.56), 0.6)
    lit = state in ("lit",)
    down = state == "down"
    world = bpy.data.worlds.new("w"); sc.world = world; world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.006, 0.006, 0.007, 1)
    # Mesa (sólo se ve en las fotos).
    bpy.ops.mesh.primitive_plane_add(size=3, location=bl(W / 2, H / 2, -52)); t = bpy.context.active_object
    t.data.materials.append(mat("table", (0.009, 0.009, 0.010), 0.42))
    # Cuerpo: losa de 50 mm con canto chaflanado; el panel superior a z = 0.
    body = box("body", -6, -6, W + 12, H + 12, 50, -50, 5.0, plate, segs=3)
    lip = box("lip", -6.6, -6.6, W + 13.2, H + 13.2, 36, -50, 6.0, edge, segs=4)
    # Pantalla: hueco fresado, bisel de aluminio y vidrio.
    s = L["screen"]
    cut(body, box("c1", s["x"], s["y"], s["w"], s["h"], 6, -3.0, 2.2, plate))
    bz = box("bezel", s["x"], s["y"], s["w"], s["h"], 2.2, -3.0, 2.0, bezel, segs=5)
    cut(bz, box("c2", s["x"] + s["bezel"], s["y"] + s["bezel"], s["w"] - 2 * s["bezel"], s["h"] - 2 * s["bezel"], 6, -4.0, 1.2, bezel))
    gl = box("glass", s["x"] + s["bezel"], s["y"] + s["bezel"], s["w"] - 2 * s["bezel"], s["h"] - 2 * s["bezel"], 1.0, -2.1, 0.8, glass)
    # Display del encabezado: vidrio negro con marco.
    d = L["display"]
    cut(body, box("c3", d["x"] - 1.2, d["y"] - 1.2, d["w"] + 2.4, d["h"] + 2.4, 4, -1.2, 1.6, plate))
    box("dframe", d["x"] - 1.2, d["y"] - 1.2, d["w"] + 2.4, d["h"] + 2.4, 0.9, -1.2, 1.4, bezel)
    box("dglass", d["x"], d["y"], d["w"], d["h"], 0.9, -0.95, 0.8, glass)
    # Canaleta de los pads, revestida de cerámica hueso.
    tr = L["trough"]
    cut(body, box("c4", tr["x"], tr["y"], tr["w"], tr["h"], 6, -2.6, 4.0, plate))
    box("liner", tr["x"], tr["y"], tr["w"], tr["h"], 0.7, -2.6, 4.0, bone)
    # Ranuras fresadas que separan las zonas.
    for yy in (154.6,):
        cut(body, box("g", 7.0, yy - 0.35, W - 14.0, 0.7, 2, -0.5, 0.3, plate))
    # Tornillos Allen.
    screw = mat("screw", (0.2, 0.2, 0.21), 0.3, metal=1.0)
    hexm = mat("hex", (0.004, 0.004, 0.004), 0.8)
    for (x, y) in ((3.4, 3.4), (W - 3.4, 3.4), (3.4, H - 3.4), (W - 3.4, H - 3.4)):
        cyl("screw", x, y, 1.75, 0.5, -0.3, screw, verts=64, bevel=0.25)
        bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.0008, depth=0.0006, location=bl(x, y, 0.15))
        bpy.context.active_object.data.materials.append(hexm)
    # Escalas de las perillas (marcas grabadas) y ejes.
    shaft = mat("shaft", (0.03, 0.03, 0.033), 0.4, metal=0.8)
    for k in L["knobs"] + L["header_knobs"]:
        rr = k["r"] + 1.9
        for j in range(11):
            a = math.radians(-135 + 27 * j)
            x, y = k["x"] + math.sin(a) * rr, k["y"] - math.cos(a) * rr
            bpy.ops.mesh.primitive_cube_add(size=1, location=bl(x, y, 0.004)); mk = bpy.context.active_object
            big = j in (0, 5, 10)
            mk.scale = (0.00028, (0.0014 if big else 0.0008) * (k["r"] / 8.5) ** 0.5, 0.000008); mk.rotation_euler = (0, 0, -a); mk.data.materials.append(dim)
        cyl("shaft", k["x"], k["y"], 3.0 if k["r"] > 7 else 2.4, 3.0, 0, shaft, verts=48)
        cyl("nut", k["x"], k["y"], 4.6 if k["r"] > 7 else 3.6, 0.6, 0, edge, verts=6)
    # Teclas: goma con lente de LED.
    lens_off = mat("lens", (0.10, 0.06, 0.07), 0.15, coat=1.0, trans=0.4)
    lens_on = mat("lens_on", (0.5, 0.2, 0.3), 0.15, coat=1.0, emit=COL["rose"], strength=5.0)
    for k in L["keys"]:
        hh = 2.2 if down else 3.2
        cut(body, box("kc", k["x"] - 0.6, k["y"] - 0.6, k["w"] + 1.2, k["h"] + 1.2, 4, -2.0, 1.4, plate))
        box("key_" + k["id"], k["x"], k["y"], k["w"], k["h"], hh + 2.0, -2.0, 1.3, rubber)
        if k["id"] in ("run", "fill", "mute", "solo"):
            box("lens_" + k["id"], k["x"] + k["w"] / 2 - 3.0, k["y"] + 1.6, 6.0, 1.5, 0.3, hh, 0.6, lens_on if lit else lens_off)
        if k["id"] in ("prev", "next"):
            tri = mat("tri", COL["dim"], 0.6)
            x0 = k["x"] + k["w"] / 2; y0 = k["y"] + k["h"] / 2
            bpy.ops.mesh.primitive_cylinder_add(vertices=3, radius=0.0021, depth=0.0002, location=bl(x0, y0, hh + 0.1))
            o = bpy.context.active_object; o.rotation_euler = (0, 0, math.pi / 2 if k["id"] == "prev" else -math.pi / 2); o.data.materials.append(tri)
    # Pads: silicona pálida translúcida, con la luz rosa adentro (lit).
    for i, p in enumerate(L["pads"]):
        hh = 3.6 if not down else 2.4
        m = mat("pad%d" % i, COL["pad"] if not lit else (0.22, 0.12, 0.15), 0.42, sss=0.35, emit=COL["padglow"], strength=(1.15 if lit else 0.0), spec=0.35)
        o = box("pad%d" % i, p["x"], p["y"], p["w"], p["h"], hh + 1.9, -1.9, 2.6, m, segs=8)
    if LABELS: silkscreen(ink, dim, key_ink)
    # Luz: caja grande arriba a la izquierda y un relleno frío desde abajo a la derecha.
    def area(name, loc, size, energy, color, look=(W / 2, H / 2)):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = energy; l.color = color
        o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (bl(*look) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        return o
    area("key", (-0.10, 0.20, 0.62), 0.55, 6.0, (1.0, 0.96, 0.92))
    area("fill", (0.62, -0.42, 0.42), 0.7, 1.4, (0.72, 0.82, 1.0))
    return sc

def ortho_camera(sc):
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"; cd.ortho_scale = W * 0.001
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); sc.camera = cam
    cam.location = bl(W / 2, H / 2, 600)
    sc.render.resolution_x = int(W * PX_PER_MM); sc.render.resolution_y = int(H * PX_PER_MM); sc.render.resolution_percentage = 100
    return cam

def cap_scene(r):
    sc = reset()
    knob_cap("cap", r)
    sc.render.film_transparent = True
    for name, loc, size, e, c in (("key", (-0.10, 0.20, 0.62), 0.55, 6.0, (1.0, 0.96, 0.92)), ("fill", (0.62, -0.42, 0.42), 0.7, 1.4, (0.72, 0.82, 1.0))):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = e; l.color = c
        o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    size = 2 * r + 8
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"; cd.ortho_scale = size * 0.001
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); sc.camera = cam; cam.location = (0, 0, 0.6)
    sc.render.resolution_x = sc.render.resolution_y = int(size * PX_PER_MM); sc.render.resolution_percentage = 100
    return sc

def px(x, y): return [round(x * PX_PER_MM, 1), round(y * PX_PER_MM, 1)]
def rect(o): return px(o["x"], o["y"]) + [round(o["w"] * PX_PER_MM, 1), round(o["h"] * PX_PER_MM, 1)]

def write_layout():
    s = L["screen"]; b = s["bezel"]
    J = {"pxPerMm": PX_PER_MM, "size": [int(W * PX_PER_MM), int(H * PX_PER_MM)],
         "display": rect(L["display"]),
         "screen": px(s["x"] + b, s["y"] + b) + [round((s["w"] - 2 * b) * PX_PER_MM, 1), round((s["h"] - 2 * b) * PX_PER_MM, 1)],
         "knobs": [{"id": k["id"], "c": px(k["x"], k["y"]), "r": k["r"] * PX_PER_MM, "size": "big"} for k in L["knobs"]]
                  + [{"id": k["id"], "c": px(k["x"], k["y"]), "r": k["r"] * PX_PER_MM, "size": "small"} for k in L["header_knobs"]],
         "keys": [{"id": k["id"], "rect": rect(k), "led": k["id"] in ("run", "fill", "mute", "solo")} for k in L["keys"]],
         "pads": [rect(p) for p in L["pads"]],
         "trough": rect(L["trough"]),
         "silk": [{"t": x["t"], "c": px(x["x"], x["y"]), "size": round(x["size"] * PX_PER_MM * 1.38, 1), "align": x["align"].lower(),
                   "ink": x["c"], "track": x.get("track", 0.0), "semi": x.get("font") == "semi"} for x in SILK],
         "knobLabelDy": 5.0 * PX_PER_MM, "padLabelDy": 4.6 * PX_PER_MM}
    json.dump(J, open(os.path.join(OUT, "layout.json"), "w"), indent=1)

# ── Fotos y film: el instrumento completo con tapas, pantalla encendida (la captura de la interfaz) y pads ──
FILM_BPM, FPS = 120.0, 24
BEAT = 60.0 / FILM_BPM; STEP16 = BEAT / 4
# Patrón de los pads en el film y las fotos (Night Shift Mercury, compás A): bombo, caja, charles, palmas.
PAT = {0: [0, 6, 8, 14], 1: [4, 12], 2: [4, 12], 4: [0, 2, 4, 6, 8, 10, 12, 14], 14: [2, 6, 10, 14]}
KNOB_VALUES = {"tune": 0.5, "decay": 0.62, "tone": 0.45, "char1": 0.38, "char2": 0.55, "drive": 0.2, "cutoff": 0.7, "level": 0.8,
               "tempo": 0.31, "swing": 0.22, "m_volume": 0.75}
SCREEN = arg("--screen")      # captura de la pantalla de la interfaz (PALLOR_SNAPSHOT, recortada al vidrio)

def screen_material(sc, glass_obj, image=None, strength=2.3):
    """El vidrio con la captura de la interfaz como luz propia, detrás de la capa de vidrio que refleja."""
    image = image or SCREEN
    m = bpy.data.materials.new("screen"); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    # Vidrio poco reflectante: la captura tiene que leerse como una pantalla encendida, no como un velo.
    b.inputs["Base Color"].default_value = (0.0, 0.0, 0.0, 1); b.inputs["Roughness"].default_value = 0.4
    b.inputs["Specular IOR Level"].default_value = 0.08
    b.inputs["Coat Weight"].default_value = 0.1; b.inputs["Coat Roughness"].default_value = 0.2
    if image and os.path.exists(image):
        tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(image); tex.interpolation = "Cubic"
        tex.extension = "EXTEND"
        # Las coordenadas generadas van de 0 a 1 en la caja del vidrio: x a la derecha, y hacia arriba (como la imagen).
        tc = nt.nodes.new("ShaderNodeTexCoord")
        nt.links.new(tc.outputs["Generated"], tex.inputs["Vector"])
        nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"])
        b.inputs["Emission Strength"].default_value = strength
    glass_obj.data.materials.clear(); glass_obj.data.materials.append(m)
    return m

def photo_scene():
    sc = build("photo")
    global LABELS
    if not LABELS:
        silkscreen(bpy.data.materials["ink"], bpy.data.materials["dim"], bpy.data.materials["keyink"])
    # El vidrio de la pantalla en coordenadas del mundo (origen en la esquina del panel) para que el mapeo funcione.
    gl = bpy.data.objects["glass"]
    screen_material(sc, gl)
    if arg("--display"): screen_material(sc, bpy.data.objects["dglass"], arg("--display"), 1.4)
    caps = {}
    for k in L["knobs"] + L["header_knobs"]:
        c = knob_cap("cap_" + k["id"], k["r"])
        c.location = bl(k["x"], k["y"], 0.0)
        # Índice: una línea de tinta en la tapa de cerámica.
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, k["r"] * 0.00042, k["r"] * 0.001 * KNOB_TOP))
        ind = bpy.context.active_object; ind.scale = (k["r"] * 0.00008, k["r"] * 0.00042, 0.00003)
        ind.data.materials.append(mat("ind", (0.03, 0.025, 0.03), 0.5)); ind.parent = c
        caps[k["id"]] = c
    pads = [bpy.data.materials["pad%d" % i] for i in range(16)]
    # Arco de valor: la interfaz lo dibuja; en las fotos, una línea de luz rosa grabada alrededor de cada perilla.
    arcs = {}
    for k in L["knobs"]:
        arcs[k["id"]] = value_arc(k, KNOB_VALUES[k["id"]])
    ARCS.clear(); ARCS.update(arcs)
    # Lentes de las teclas: material propio para animarlas.
    lenses = {}
    for kid in ("run", "fill", "mute", "solo"):
        o = bpy.data.objects.get("lens_" + kid)
        m = mat("lensm_" + kid, (0.4, 0.15, 0.22), 0.15, coat=1.0, emit=COL["rose"], strength=0.0)
        o.data.materials.clear(); o.data.materials.append(m); lenses[kid] = m
    lenses["run"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 5.0
    # Luz de recorte desde atrás para separar el canto de la losa en perspectiva.
    l = bpy.data.lights.new("rim", "AREA"); l.size = 0.8; l.energy = 9.0; l.color = (0.78, 0.86, 1.0)
    o = bpy.data.objects.new("rim", l); o.location = (0.18, 0.42, 0.30); sc.collection.objects.link(o)
    o.rotation_euler = (bl(W / 2, H / 2) - o.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.color_mode = "RGB"; sc.render.image_settings.quality = 93
    sc.render.film_transparent = False
    return sc, caps, pads, lenses

ARCS = {}

def value_arc(k, v):
    """Arco de luz alrededor de la perilla, de −135° al valor (curva con bisel, emisiva)."""
    cu = bpy.data.curves.new("arc", "CURVE"); cu.dimensions = "2D"; cu.bevel_depth = 0.00025
    # El arco recorre toda la escala y bevel_factor_end lo corta en el valor (así se puede animar).
    sp = cu.splines.new("POLY"); n = 96
    a0, a1 = -135.0, 135.0
    cu.bevel_factor_mapping_end = "SPLINE"; cu.bevel_factor_end = max(0.02, v)
    sp.points.add(n - 1)
    rr = k["r"] + 3.6
    for i in range(n):
        a = math.radians(a0 + (a1 - a0) * i / (n - 1))
        p = bl(k["x"] + math.sin(a) * rr, k["y"] - math.cos(a) * rr, 0.05)
        sp.points[i].co = (p.x, p.y, p.z, 1)
    o = bpy.data.objects.new("arc_" + k["id"], cu); bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat("arcm", (0.2, 0.05, 0.1), 0.4, emit=COL["rose"], strength=6.0))
    return o

def knob_angle(v): return -math.radians(-135 + 270 * v)

def pad_levels(t):
    """Brillo de cada pad en t: un destello que decae desde el último golpe del patrón."""
    k = int(t / STEP16 + 1e-6); out = []
    for i in range(16):
        steps = PAT.get(i, [])
        last = None
        for back in range(16):
            if (k - back) % 16 in steps and k - back >= 0: last = (k - back) * STEP16; break
        a = t - last if last is not None else 99.0
        out.append(0.02 + 2.2 * math.exp(-a / 0.12))
    return out

def set_pads(pads, t):
    for m, e in zip(pads, pad_levels(t)):
        m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = e

PSHOTS = {   # cámara, mira, lente, f, resolución
    "hero":  ((0.43, -0.56, 0.42), (0.180, -0.120, -0.006), 46, 8.0, (2400, 1250)),
    "close": ((0.010, -0.330, 0.075), (0.083, -0.207, 0.003), 75, 2.8, (1600, 1100)),
    "top":   ((0.18, -0.1175, 0.75), (0.18, -0.1175, 0.0), 50, 11.0, (1800, 1180)),
    "knobs": ((0.33, -0.255, 0.075), (0.1025, -0.1715, 0.008), 85, 3.2, (1600, 1100)),
    "side":  ((-0.24, -0.46, 0.17), (0.17, -0.120, -0.01), 50, 6.0, (1600, 1100)),
}

def pcamera(sc, loc, look, lens, fstop):
    cd = bpy.data.cameras.new("pcam"); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop; cd.clip_start = 0.005
    cam = bpy.data.objects.new("pcam", cd); sc.collection.objects.link(cam); sc.camera = cam
    cam.location = Vector(loc); d = Vector(look) - Vector(loc)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); cd.dof.focus_distance = d.length
    return cam

def photos():
    for name in arg("--shots", "hero,close,top,knobs,side").split(","):
        sc, caps, pads, lenses = photo_scene()
        loc, look, lens, fstop, res = PSHOTS[name]
        pcamera(sc, loc, look, lens, fstop); sc.render.resolution_x, sc.render.resolution_y = res; sc.render.resolution_percentage = int(arg("--pct", "100"))
        for k, c in caps.items(): c.rotation_euler = (0, 0, knob_angle(KNOB_VALUES[k]))
        set_pads(pads, 12 * STEP16 + 0.03)
        sc.render.filepath = os.path.join(OUT, name + ".jpg")
        bpy.ops.render.render(write_still=True)

# ── Film (filmkit): seis tomas a 120 BPM ──
FILM_SHOTS = [("open", 8), ("pads", 8), ("knob", 8), ("screen", 6), ("hero", 12), ("end", 8)]
TAGLINE = "The cold drum machine."
PRESET = "Night Shift Mercury"
AUTO_DT = 0.1

def shot_table():
    res, f0 = [], 0
    for name, beats in FILM_SHOTS:
        n = int(round(beats * BEAT * FPS)); res.append((name, f0, n)); f0 += n
    return res

def _sm(a, b, t0, t1, t):
    u = min(1.0, max(0.0, (t - t0) / (t1 - t0))); u = u * u * (3 - 2 * u)
    return a + (b - a) * u

def film_auto(t):
    """Decay de la caja (en %, 0..100): corto al abrir, se abre en la toma de la perilla, queda largo en el héroe."""
    d = _sm(32, 82, 8.4, 11.6, t) if t < 14.0 else _sm(82, 55, 20.0, 24.0, t)
    return {"sd_decay": round(d, 1)}

def write_film_json():
    total = sum(n for _, _, n in shot_table()) / FPS
    head = ["build/plugins/pallor/pallorrender_artefacts/Release/pallorrender", "{out}", "--preset", PRESET,
            "--bpm", "%d" % FILM_BPM, "--seconds", "{seconds}"]
    last = film_auto(0.0)
    for p_, x in last.items(): head += ["--set", "%s=%g" % (p_, x)]
    ats = []
    for k in range(int((total + 1.5) / AUTO_DT) + 1):
        tt = k * AUTO_DT
        for p_, x in film_auto(tt).items():
            if x != last[p_]: ats.append(["--at", "%.2f" % tt, "%s=%g" % (p_, x)]); last[p_] = x
    J = lambda xs: ", ".join(json.dumps(x) for x in xs)
    lines = ["{", '  "title": "Pallor",', '  "tagline": "%s",' % TAGLINE, '  "fps": %d,' % FPS, '  "bpm": %d,' % FILM_BPM, '  "shots": [']
    sh = [json.dumps(dict({"name": n, "beats": b_}, **({"in": "dissolve"} if n == "end" else {}))) for n, b_ in FILM_SHOTS]
    lines += ["    " + x + ("," if i < len(sh) - 1 else "") for i, x in enumerate(sh)]
    lines += ["  ],", '  "poster": {"shot": "hero", "frame": 120},', '  "audio": {"cmd": [', "    " + J(head) + ("," if ats else "")]
    rows = [J(sum(ats[i:i + 6], [])) for i in range(0, len(ats), 6)]
    lines += ["    " + r + ("," if i < len(rows) - 1 else "") for i, r in enumerate(rows)]
    lines += ["  ]}", "}"]
    p = os.path.join(HERE, "..", "film.json")
    open(p, "w").write("\n".join(lines) + "\n"); json.load(open(p))
    print(os.path.normpath(p), "%.2f s, %d cambios" % (total, len(ats)))

def film2():
    sys.path.insert(0, os.path.join(ROOT, "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final"); every = int(arg("--every", "1"))
    V = lambda x, y, z: Vector((x, y, z))
    P = lambda x, y, z=0.0: bl(x, y, z)
    pad = lambda i: P(L["pads"][i]["x"] + 9.3, L["pads"][i]["y"] + 9.3, 3.6)
    kn = {k["id"]: P(k["x"], k["y"], 4.0) for k in L["knobs"]}
    C = P(W / 2, H / 2 + 6, 0)
    scr = P(212, 82, 0)
    def arc(cx, a, r, z): return cx + V(math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r, z)
    paths = {
        "open":   fk.Path([(0, P(30, 262, 72), P(55, 207, 0), 50, 2.8), (1, P(78, 262, 66), P(103, 207, 0), 52, 2.8)]),
        "pads":   fk.Path([(0, P(-30, 230, 28), P(120, 206, 1), 40, 2.8), (1, P(-10, 236, 24), P(170, 206, 1), 42, 2.8)]),
        "knob":   fk.Path([(0, arc(kn["decay"], -70, 0.085, 0.050), kn["decay"] + V(0, 0, -0.001), 100, 2.6),
                           (1, arc(kn["decay"], -55, 0.080, 0.046), kn["decay"] + V(0, 0, -0.001), 100, 2.6)]),
        "screen": fk.Path([(0, P(185, 162, 132), P(185, 80, 0), 50, 4.0), (1, P(228, 160, 126), P(228, 80, 0), 52, 4.0)]),
        "hero":   fk.Path([(0, arc(C, -128, 0.56, 0.12), C + V(0, 0, -0.01), 40, 6.0),
                           (0.5, arc(C, -98, 0.56, 0.19), C + V(0, 0, -0.008), 41, 6.0),
                           (1, arc(C, -66, 0.58, 0.27), C + V(0, 0, -0.006), 42, 6.0)]),
        "end":    fk.Path([(0, C + V(0.18, -0.42, 0.26), C + V(0, 0, -0.006), 45, 6.0),
                           (1, C + V(0.26, -0.62, 0.38), C + V(0, 0, -0.006), 45, 6.0)], easing="out"),
    }
    shake = {"open": 0.0005, "pads": 0.0006, "knob": 0.0006, "screen": 0.0005, "hero": 0.0, "end": 0.0}
    for (name, f0, frames) in shot_table():
        if only and name not in only.split(","): continue
        sc, caps, pads, lenses = photo_scene()
        sc.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.003, 0.003, 0.0035, 1)
        fk.setup(sc, quality)
        cam = fk.camera(sc); path = paths[name]
        # Cabezal de la grilla: una franja de luz que recorre las columnas de la pantalla.
        s = L["screen"]
        bpy.ops.mesh.primitive_plane_add(size=1, location=bl(0, 0, -0.95))
        head = bpy.context.active_object; head.scale = (0.0146, 0.070, 1)
        hm = bpy.data.materials.new("head"); hm.use_nodes = True; nt = hm.node_tree; nt.nodes.clear()
        em = nt.nodes.new("ShaderNodeEmission"); em.inputs["Color"].default_value = (0.75, 0.55, 0.65, 1)
        tp = nt.nodes.new("ShaderNodeBsdfTransparent"); ad = nt.nodes.new("ShaderNodeAddShader"); ou = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(em.outputs[0], ad.inputs[0]); nt.links.new(tp.outputs[0], ad.inputs[1]); nt.links.new(ad.outputs[0], ou.inputs[0])
        head.data.materials.append(hm)
        head.visible_shadow = False
        GX0, GW, GY = 98.1, 250.3, 83.5               # zona de la grilla de la página PATTERN en la captura (mm)

        def pose(f, t, u, name=name, path=path, f0=f0):
            T = (f0 + f - 1) / FPS
            dec = film_auto(T)["sd_decay"] / 100.0
            for k, c in caps.items(): c.rotation_euler = (0, 0, knob_angle(dec if k == "decay" else KNOB_VALUES[k]))
            step = int(T / STEP16) % 16
            head.location = bl(GX0 + GW * (step + 0.5) / 16.0, GY, -0.95)
            focus = None
            if name == "open": focus = path.at(u)[1] + V(0, 0, 0.004)
            elif name == "pads": focus = pad(int(2 + u * 8))
            elif name == "knob": focus = kn["decay"]
            elif name == "screen": focus = path.at(u)[1]
            path.apply(cam, u, focus=focus, shake=shake[name], t=T)
            return list(caps.values()) + [head]

        studio = [(o.data, o.data.energy) for o in sc.objects if o.type == "LIGHT"]

        def keys_(f, name=name, f0=f0, frames=frames):
            T = (f0 + f - 1) / FPS; u = (f - 1) / max(1, frames - 1)
            if name == "open":
                for ld, e0 in studio:
                    ld.energy = e0 * fk.rack(u, 0.0, 0.85, 0.12, 1.0); ld.keyframe_insert("energy")
            for m, e in zip(pads, pad_levels(T)):
                inp = m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]; inp.default_value = e; inp.keyframe_insert("default_value")
            step = int(T / STEP16) % 16
            inp = em.inputs["Strength"]
            inp.default_value = 0.35 + 0.25 * math.exp(-(T - int(T / STEP16) * STEP16) / 0.06); inp.keyframe_insert("default_value")
            if "decay" in ARCS:
                cu = ARCS["decay"].data; cu.bevel_factor_end = max(0.02, film_auto(T)["sd_decay"] / 100.0); cu.keyframe_insert("bevel_factor_end")
            fl = lenses["fill"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
            fl.default_value = 5.0 if (int(T / (4 * BEAT)) % 4 == 3) else 0.0; fl.keyframe_insert("default_value")

        if every > 1: sc.frame_step = every
        fk.render_shot(sc, name, frames, pose, OUT, step=every, extra_keys=keys_)


if __name__ == "__main__" and arg("--mode", "ui") == "filmjson":
    write_film_json(); raise SystemExit
if __name__ == "__main__" and arg("--mode", "ui") == "film2":
    film2(); raise SystemExit
if __name__ == "__main__" and arg("--mode", "ui") == "photos":
    photos(); raise SystemExit

if __name__ == "__main__":
    write_layout()
    what = arg("--only", "bg,lit,down,caps").split(",")
    for st in ("bg", "lit", "down"):
        if st in what:
            sc = build(st); ortho_camera(sc)
            sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.color_mode = "RGB"; sc.render.image_settings.quality = 90
            sc.render.filepath = os.path.join(OUT, st + ".jpg"); bpy.ops.render.render(write_still=True)
    if "caps" in what:
        for nm, r in (("big", 8.5), ("small", 6.0)):
            sc = cap_scene(r); sc.render.filepath = os.path.join(OUT, f"cap_{nm}.png"); bpy.ops.render.render(write_still=True)
