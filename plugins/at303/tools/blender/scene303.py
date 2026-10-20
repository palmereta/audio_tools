"""El AT303 renderizado: un instrumento propio (no el panel del original) de aluminio grafito, visto desde arriba
con cámara ortográfica. Genera lo que la interfaz dibuja (docs/research.md §6, dirección "grafito + esquema"):

  out/bg.png          el instrumento entero, con las perillas sin tapa, los pads apagados y las teclas arriba
  out/keysdown.png    el mismo encuadre con todas las teclas y botones apretados (la interfaz recorta cada uno)
  out/knob_big.png, out/knob_small.png   tapas de perilla sin sombra ni índice (fondo transparente); la interfaz dibuja el índice rotado
  out/layout.json     posición en píxeles (de bg.png) de cada perilla, pad, tecla, botón y del display
Fotos y film de la web: --mode photos [--shots hero,close,top] y --mode film --shot orbit|knobs|pads (144 cuadros).
Film nuevo de la web (filmkit, seis tomas a 128 BPM): --mode film2, ver film2() más abajo.

Sin marcas: la placa dice "AT303 / Audio Tools". Unidades: mm; el origen es la esquina superior izquierda de la placa,
x a la derecha, y hacia abajo (en Blender, y se invierte). Escala: 6 px/mm (la interfaz trabaja a la mitad).

  B=/Volumes/Docs/Applications/blender/Blender.app/Contents/MacOS/Blender      (o el de pc50-wsl, bajo el lock)
  $B -b --factory-startup -P scene303.py -- --out ../../assets/render/out --samples 256 [--body graphite|alu|olive]
"""
import math, os, sys, json
try:
    import bpy, bmesh
    from mathutils import Vector
except ImportError:                 # fuera de Blender sólo sirve --mode filmjson (escribe tools/film.json)
    bpy = None

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out"))
if bpy: os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "192"))
BODY = arg("--body", "graphite")
HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, "..", "..", "..", "lacquer", "assets", "fonts", "BarlowCondensed-SemiBold.ttf")
PX_PER_MM = 6.0
W, H = 400.0, 214.0                 # la placa
MARGIN = 10.0

# ── Disposición (mm) ──
BIG = [("cutoff", "Cutoff"), ("resonance", "Resonance"), ("envmod", "Env Mod"), ("decay", "Decay"), ("accent", "Accent")]
SMALL = [("tuning", "Tuning"), ("tempo", "Tempo"), ("volume", "Volume")]
LAYOUT = {
    "big": [{"id": k, "label": l, "x": 132 + 40 * i, "y": 44, "r": 11.5} for i, (k, l) in enumerate(BIG)],
    "small": [{"id": k, "label": l, "x": 34 + 25 * i, "y": 92, "r": 7.5} for i, (k, l) in enumerate(SMALL)],
    "wave": {"x": 352, "y": 44},
    "display": {"x": 22, "y": 38, "w": 62, "h": 22},
    "sweep": {"x": 288, "y": 66, "w": 46, "h": 4},
    "pads": [{"x": 118 + 16.75 * i, "y": 104, "w": 12.5, "h": 12.5} for i in range(16)],
    "keys": [],
    "buttons": [],
}
# Teclado de una octava en dos filas de teclas cuadradas propias: abajo las 8 naturales, arriba las 5 alteradas.
naturals = ["C", "D", "E", "F", "G", "A", "B", "C2"]
nat_semis = [0, 2, 4, 5, 7, 9, 11, 12]
kx0, kw, kgap = 118.0, 31.0, 2.6
for i, (n, s) in enumerate(zip(naturals, nat_semis)):
    LAYOUT["keys"].append({"semi": s, "name": n, "x": kx0 + i * (kw + kgap), "y": 172, "w": kw, "h": 30, "sharp": False})
for after, s in ((0, 1), (1, 3), (3, 6), (4, 8), (5, 10)):
    LAYOUT["keys"].append({"semi": s, "name": ["C#", "D#", "F#", "G#", "A#"][[1, 3, 6, 8, 10].index(s)],
                           "x": kx0 + after * (kw + kgap) + kw * 0.5 + kgap * 0.5, "y": 136, "w": 24, "h": 28, "sharp": True})
# Botones de función: cuatro filas de dos, a la izquierda del teclado.
for i, (bid, lab) in enumerate((("pitch", "Pitch"), ("time", "Time"), ("run", "Run"), ("clear", "Clear"),
                                ("down", "Down"), ("up", "Up"), ("acc", "Accent"), ("slide", "Slide"))):
    LAYOUT["buttons"].append({"id": bid, "label": lab, "x": 22 + (i % 2) * 33, "y": 130 + (i // 2) * 20, "w": 28, "h": 12})

COLORS = {
    "graphite": ((0.055, 0.058, 0.064), (0.03, 0.031, 0.034)),
    "alu": ((0.62, 0.63, 0.64), (0.34, 0.35, 0.36)),
    "olive": ((0.11, 0.12, 0.085), (0.05, 0.055, 0.04)),
}

def bl(x, y, z=0.0):
    """mm de la placa → metros de Blender (y invertida)."""
    return Vector((x * 0.001, -y * 0.001, z * 0.001))

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("OPTIX", "CUDA", "METAL"):
        try:
            prefs.compute_device_type = kind; prefs.get_devices()
            if any(d.type == kind for d in prefs.devices):
                for d in prefs.devices: d.use = d.type == kind
                break
        except Exception: pass
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True; sc.cycles.denoising_use_gpu = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Base Contrast"
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    return sc

def mat(name, base, rough, metal=0.0, aniso=0.0, coat=0.0, emit=None, strength=0.0, alpha=1.0, trans=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if aniso: b.inputs["Anisotropic"].default_value = aniso
    if coat: b.inputs["Coat Weight"].default_value = coat
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    if trans: b.inputs["Transmission Weight"].default_value = trans
    return m

def rounded_box(name, x, y, w, h, depth, z0, r, m, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=bl(x + w / 2, y + h / 2, z0 + depth / 2))
    o = bpy.context.active_object; o.name = name
    o.scale = (w * 0.001, h * 0.001, depth * 0.001); bpy.ops.object.transform_apply(scale=True)
    if r > 0:
        md = o.modifiers.new("b", "BEVEL"); md.width = r * 0.001; md.segments = 6; md.limit_method = "ANGLE"
    if bevel > 0:
        md2 = o.modifiers.new("b2", "BEVEL"); md2.width = bevel * 0.001; md2.segments = 3
    o.data.materials.append(m); bpy.ops.object.shade_smooth()
    return o

# --labels 0: sin la serigrafía (la interfaz la dibuja como texto vectorial, legible a cualquier tamaño).
LABELS = arg("--labels", "1") == "1"

def text(s, x, y, size, m, align="LEFT", z=0.02, force=False):
    if not LABELS and not force: return None
    cu = bpy.data.curves.new("t", "FONT"); cu.body = s; cu.size = size * 0.001; cu.align_x = align; cu.align_y = "CENTER"
    if os.path.exists(FONT): cu.font = bpy.data.fonts.load(FONT, check_existing=True)
    cu.extrude = 0.00002
    o = bpy.data.objects.new("t", cu); bpy.context.scene.collection.objects.link(o)
    o.location = bl(x, y, z); cu.materials.append(m)
    return o

def knob_cap(name, r, color):
    """Tapa vista desde arriba: pollera grafito con nervaduras radiales y una tapa de aluminio biselada."""
    skirt_m = mat(name + "_skirt", (0.035, 0.036, 0.04), 0.5, metal=0.3)
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=r * 0.001, depth=r * 0.0004, location=(0, 0, r * 0.0002))
    c = bpy.context.active_object; c.name = name; c.data.materials.append(skirt_m)
    md = c.modifiers.new("b", "BEVEL"); md.width = r * 0.00006; md.segments = 3
    rib_m = mat(name + "_rib", (0.09, 0.092, 0.1), 0.35, metal=0.5)
    for k in range(36):
        a = 2 * math.pi * k / 36
        bpy.ops.mesh.primitive_cube_add(size=1, location=(math.cos(a) * r * 0.00087, math.sin(a) * r * 0.00087, r * 0.00042))
        g = bpy.context.active_object; g.scale = (r * 0.00024, r * 0.00005, r * 0.00008); g.rotation_euler = (0, 0, a)
        g.data.materials.append(rib_m); g.parent = c
    alu = mat(name + "_alu", (0.62, 0.62, 0.64), 0.32, metal=1.0, aniso=0.6)
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=r * 0.00074, depth=r * 0.0009, location=(0, 0, r * 0.00085))
    top = bpy.context.active_object; top.data.materials.append(alu)
    md = top.modifiers.new("b", "BEVEL"); md.width = r * 0.00012; md.segments = 6; md.limit_method = "ANGLE"
    bpy.ops.object.shade_smooth()
    # Sin índice: la interfaz dibuja la tapa quieta (la luz no gira) y el índice rotado encima.
    top.parent = c
    return c

def build(keys_down=False, only_cap=None):
    sc = reset()
    top_c, cap_c = COLORS.get(BODY, COLORS["graphite"])
    plate = mat("plate", top_c, 0.55, metal=0.7, aniso=0.15)
    caps = mat("caps", cap_c, 0.75, metal=0.5)
    ink = mat("ink", (0.70, 0.71, 0.72) if BODY != "alu" else (0.08, 0.08, 0.09), 0.6)
    accent_ink = mat("accent", (0.95, 0.55, 0.15), 0.5)
    world = bpy.data.worlds.new("w"); sc.world = world; world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.012, 0.012, 0.013, 1)
    if only_cap is None:
        # Mesa.
        bpy.ops.mesh.primitive_plane_add(size=2, location=bl(W / 2, H / 2, -46)); t = bpy.context.active_object
        t.data.materials.append(mat("table", (0.025, 0.024, 0.023), 0.6))
        # Cuerpo: placa superior entre dos tapas laterales arenadas.
        rounded_box("body", 14, 0, W - 28, H, 45, -45, 2.5, plate)
        rounded_box("capL", 0, -1, 15, H + 2, 48, -46, 4.5, caps)
        rounded_box("capR", W - 15, -1, 15, H + 2, 48, -46, 4.5, caps)
        # Grabados.
        text("AT303", 22, 20, 11, ink, z=0.01)
        text("Audio Tools", 22, 29.5, 3.6, ink, z=0.01)
        for k in LAYOUT["big"]: text(k["label"], k["x"], k["y"] + k["r"] + 7, 3.4, ink, "CENTER", z=0.01)
        for k in LAYOUT["small"]: text(k["label"], k["x"], k["y"] + k["r"] + 5.5, 3.0, ink, "CENTER", z=0.01)
        text("Waveform", LAYOUT["wave"]["x"], LAYOUT["wave"]["y"] + 18.5, 3.4, ink, "CENTER", z=0.01)
        text("Accent charge", LAYOUT["sweep"]["x"] + LAYOUT["sweep"]["w"] / 2, LAYOUT["sweep"]["y"] + 7.5, 2.8, ink, "CENTER", z=0.01)
        for i in range(16): text(str(i + 1), LAYOUT["pads"][i]["x"] + 6.25, 100, 2.6, ink, "CENTER", z=0.01)
        # Escala de las perillas grandes: 11 marcas.
        for k in LAYOUT["big"] + LAYOUT["small"]:
            rr = k["r"] + 2.6
            for j in range(11):
                a = math.radians(-135 + 27 * j)
                x, y = k["x"] + math.sin(a) * rr, k["y"] - math.cos(a) * rr
                bpy.ops.mesh.primitive_cube_add(size=1, location=bl(x, y, 0.01)); m_ = bpy.context.active_object
                m_.scale = (0.00035, 0.0016 if j % 5 == 0 else 0.0009, 0.00002); m_.rotation_euler = (0, 0, -a); m_.data.materials.append(ink)
        # Ejes de las perillas (la tapa la dibuja la interfaz).
        shaft = mat("shaft", (0.02, 0.02, 0.022), 0.5, metal=0.4)
        for k in LAYOUT["big"] + LAYOUT["small"]:
            bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.003, depth=0.004, location=bl(k["x"], k["y"], 1.0))
            bpy.context.active_object.data.materials.append(shaft)
        # Llave de forma de onda (palanca hacia "saw").
        wv = LAYOUT["wave"]
        rounded_box("waveplate", wv["x"] - 7, wv["y"] - 11, 14, 22, 1.2, 0, 1.2, mat("wp", (0.02, 0.02, 0.022), 0.4, metal=0.6))
        text("Saw", wv["x"] + 9, wv["y"] - 7, 2.8, ink, z=0.01); text("Sq", wv["x"] + 9, wv["y"] + 7, 2.8, ink, z=0.01)
        # Display: vidrio oscuro con un marco.
        d = LAYOUT["display"]
        rounded_box("dispframe", d["x"] - 1.5, d["y"] - 1.5, d["w"] + 3, d["h"] + 3, 0.6, 0, 1.0, mat("frame", (0.015, 0.015, 0.016), 0.3, metal=0.7))
        rounded_box("dispglass", d["x"], d["y"], d["w"], d["h"], 0.8, 0.0, 0.5, mat("glass", (0.012, 0.005, 0.002), 0.45, coat=0.05))
        # Barra de carga del acento (vidrio negro; la luz la dibuja la interfaz).
        s_ = LAYOUT["sweep"]
        rounded_box("sweepbar", s_["x"], s_["y"], s_["w"], s_["h"], 0.6, 0, 0.8, mat("sw", (0.02, 0.008, 0.004), 0.1, coat=1.0))
        # Pads: tapas translúcidas.
        padm = mat("pad", (0.55, 0.56, 0.58), 0.35, trans=0.35)
        for p in LAYOUT["pads"]:
            rounded_box("pad", p["x"], p["y"], p["w"], p["h"], 3.0 if not keys_down else 1.8, 0, 1.6, padm)
        # Teclas: naturales claras, alteradas grafito; botones de función.
        keym = mat("key", (0.70, 0.70, 0.68), 0.45)
        sharpm = mat("sharp", (0.09, 0.095, 0.1), 0.4, metal=0.5)
        for kk in LAYOUT["keys"]:
            h_ = 6.0 if not keys_down else 3.5
            rounded_box("key", kk["x"], kk["y"], kk["w"], kk["h"], h_, -1.0, 1.4, sharpm if kk["sharp"] else keym)
        btnm = mat("btn", (0.16, 0.165, 0.17), 0.5, metal=0.3)
        for b in LAYOUT["buttons"]:
            rounded_box("btn", b["x"], b["y"], b["w"], b["h"], 4.0 if not keys_down else 2.2, -1.0, 1.2, btnm)
            text(b["label"], b["x"] + b["w"] / 2, b["y"] - 2.6, 2.6, ink, "CENTER", z=0.01)
    else:
        # Sólo una tapa, sobre un piso que atrapa la sombra.
        r = 11.5 if only_cap == "big" else 7.5
        knob_cap("cap", r, (0.95, 0.55, 0.15))
        # Sin sombra: la interfaz rota la tapa y dibuja la sombra fija aparte.
        sc.render.film_transparent = True
    # Luz: caja grande arriba a la izquierda (la sombra cae hacia abajo a la derecha) y un relleno frío.
    def area(name, loc, size, energy, color):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = energy; l.color = color
        o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (Vector((W / 2 * 0.001, -H / 2 * 0.001, 0)) - Vector(loc)).to_track_quat("-Z", "X").to_euler() if only_cap is None else (Vector((0, 0, 0)) - Vector(loc)).to_track_quat("-Z", "X").to_euler()
        return o
    if only_cap is None:
        area("key", (-0.05, 0.12, 0.55), 0.5, 5.0, (1.0, 0.97, 0.92))
        area("fill", (0.55, -0.35, 0.35), 0.6, 1.2, (0.75, 0.85, 1.0))
    else:
        area("key", (-0.05, 0.12 + 0.1, 0.55), 0.5, 0.9, (1.0, 0.97, 0.92))
        area("fill", (0.2, -0.2, 0.3), 0.3, 0.08, (0.75, 0.85, 1.0))
    # Cámara ortográfica cenital.
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); sc.camera = cam
    if only_cap is None:
        cd.ortho_scale = (W + 2 * MARGIN) * 0.001
        cam.location = bl(W / 2, H / 2, 600)
        sc.render.resolution_x = int((W + 2 * MARGIN) * PX_PER_MM); sc.render.resolution_y = int((H + 2 * MARGIN) * PX_PER_MM)
    else:
        r = 11.5 if only_cap == "big" else 7.5
        size = (r * 2 + 8)
        cd.ortho_scale = size * 0.001; cam.location = (0, 0, 0.6)
        sc.render.resolution_x = sc.render.resolution_y = int(size * PX_PER_MM)
    sc.render.resolution_percentage = 100
    return sc

def px(x, y): return [round((x + MARGIN) * PX_PER_MM, 1), round((y + MARGIN) * PX_PER_MM, 1)]

def write_layout():
    L = {"pxPerMm": PX_PER_MM, "margin": MARGIN, "size": [int((W + 2 * MARGIN) * PX_PER_MM), int((H + 2 * MARGIN) * PX_PER_MM)],
         "knobs": [], "pads": [], "keys": [], "buttons": []}
    for k in LAYOUT["big"]: L["knobs"].append({"id": k["id"], "c": px(k["x"], k["y"]), "r": k["r"] * PX_PER_MM, "size": "big"})
    for k in LAYOUT["small"]: L["knobs"].append({"id": k["id"], "c": px(k["x"], k["y"]), "r": k["r"] * PX_PER_MM, "size": "small"})
    for p in LAYOUT["pads"]: L["pads"].append(px(p["x"], p["y"]) + [p["w"] * PX_PER_MM, p["h"] * PX_PER_MM])
    for k in LAYOUT["keys"]: L["keys"].append({"semi": k["semi"], "rect": px(k["x"], k["y"]) + [k["w"] * PX_PER_MM, k["h"] * PX_PER_MM], "sharp": k["sharp"]})
    for b in LAYOUT["buttons"]: L["buttons"].append({"id": b["id"], "rect": px(b["x"], b["y"]) + [b["w"] * PX_PER_MM, b["h"] * PX_PER_MM]})
    d = LAYOUT["display"]; L["display"] = px(d["x"], d["y"]) + [d["w"] * PX_PER_MM, d["h"] * PX_PER_MM]
    s = LAYOUT["sweep"]; L["sweep"] = px(s["x"], s["y"]) + [s["w"] * PX_PER_MM, s["h"] * PX_PER_MM]
    wv = LAYOUT["wave"]; L["wave"] = px(wv["x"] - 7, wv["y"] - 11) + [14 * PX_PER_MM, 22 * PX_PER_MM]
    json.dump(L, open(os.path.join(OUT, "layout.json"), "w"), indent=1)

# ── Fotos y film de la web: el instrumento completo en perspectiva, con tapas, pads encendidos y display ──
# Patrón de Squelch Line (assets/presets): acentos en los pasos 2, 8 y 12, slides en 4 y 10; 128 BPM.
FILM_BPM, FPS = 128.0, 24
ACC_STEPS, SLIDE_STEPS = {1, 7, 11}, {3, 9}
KNOB_VALUES = {"cutoff": 0.42, "resonance": 0.86, "envmod": 0.6, "decay": 0.45, "accent": 0.75, "tuning": 0.5, "tempo": 0.55, "volume": 0.8}

def emit_mat(name, color, strength):
    m = mat(name, (0.05, 0.05, 0.05), 0.4, emit=color, strength=strength)
    return m

def photo_scene():
    sc = build(False)
    # Tapas de perilla con su índice ámbar.
    caps = {}
    for k in LAYOUT["big"] + LAYOUT["small"]:
        c = knob_cap("cap_" + k["id"], k["r"], (0.95, 0.55, 0.15))
        c.location = bl(k["x"], k["y"], 0.0)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, k["r"] * 0.00043, k["r"] * 0.00131))
        ind = bpy.context.active_object; ind.scale = (k["r"] * 0.0001, k["r"] * 0.00038, 0.00005)
        ind.data.materials.append(emit_mat("ind", (1.0, 0.6, 0.2), 1.5)); ind.parent = c
        caps[k["id"]] = c
    # Pads: un material propio por pad para encenderlos cuadro a cuadro.
    pads = sorted([o for o in sc.objects if o.name.startswith("pad") and o.type == "MESH"], key=lambda o: o.location.x)
    pad_mats = []
    for i, o in enumerate(pads):
        m = mat(f"pad{i}", (0.55, 0.56, 0.58), 0.35, trans=0.35, emit=(1.0, 0.55, 0.15), strength=0.0)
        o.data.materials.clear(); o.data.materials.append(m); pad_mats.append(m)
    # Display: el tempo en ámbar; barra de carga del acento.
    d = LAYOUT["display"]
    t = text("128", d["x"] + d["w"] * 0.5, d["y"] + d["h"] * 0.5, 13.0, emit_mat("seg", (1.0, 0.55, 0.12), 6.0), "CENTER", z=0.9, force=True)
    s_ = LAYOUT["sweep"]
    bpy.ops.mesh.primitive_cube_add(size=1, location=bl(s_["x"] + s_["w"] * 0.25, s_["y"] + s_["h"] * 0.5, 0.7))
    bar = bpy.context.active_object; bar.scale = (s_["w"] * 0.5 * 0.001, s_["h"] * 0.6 * 0.001, 0.0002)
    bar_m = emit_mat("bar", (1.0, 0.55, 0.12), 5.0); bar.data.materials.append(bar_m)
    # Luz de recorte desde atrás para separar el borde de la placa en perspectiva.
    l = bpy.data.lights.new("rim", "AREA"); l.size = 0.6; l.energy = 6.0; l.color = (0.8, 0.88, 1.0)
    o = bpy.data.objects.new("rim", l); o.location = (0.2, 0.45, 0.35); sc.collection.objects.link(o)
    o.rotation_euler = (Vector((0.2, -0.1, 0)) - o.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.color_mode = "RGB"; sc.render.image_settings.quality = 92
    sc.render.film_transparent = False
    return sc, caps, pad_mats, bar, bar_m

def knob_angle(v): return -math.radians(-135 + 270 * v)

def set_time(t, caps, pad_mats, bar, cutoff=None):
    step = int(t * FILM_BPM / 60.0 * 4) % 16
    for i, m in enumerate(pad_mats):
        b = m.node_tree.nodes["Principled BSDF"]
        col = (1.0, 0.22, 0.12) if i in ACC_STEPS else (1.0, 0.55, 0.15)
        b.inputs["Emission Color"].default_value = (*col, 1)
        b.inputs["Emission Strength"].default_value = (9.0 if i == step else 1.6)
    vals = dict(KNOB_VALUES)
    if cutoff is not None: vals["cutoff"] = cutoff
    for k, c in caps.items(): c.rotation_euler = (0, 0, knob_angle(vals[k]))
    # La carga del acento sube con cada acento y baja despacio.
    charge = 0.0
    for j in range(16 * 4):
        tj = j * 60.0 / FILM_BPM / 4
        if tj > t: break
        charge *= math.exp(-(60.0 / FILM_BPM / 4) / 0.45)
        if j % 16 in ACC_STEPS: charge = charge + (1 - charge) * 0.55
    bar.scale.x = max(0.0005, LAYOUT["sweep"]["w"] * charge * 0.001)
    bar.location.x = bl(LAYOUT["sweep"]["x"] + LAYOUT["sweep"]["w"] * charge * 0.5, 0)[0]

PSHOTS = {   # posición de cámara, mira, lente, f, resolución
    "hero":  ((0.54, -0.58, 0.36), (0.205, -0.112, -0.004), 44, 9.0, (2400, 1100)),
    "close": ((0.11, -0.17, 0.075), (0.25, -0.052, 0.004), 58, 2.8, (1600, 1100)),
    "top":   ((0.21, -0.12, 0.66), (0.21, -0.108, 0.0), 50, 11.0, (1400, 1100)),
}

def pcamera(sc, loc, look, lens, fstop):
    cd = bpy.data.cameras.new("pcam"); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    cam = bpy.data.objects.new("pcam", cd); sc.collection.objects.link(cam); sc.camera = cam
    paim(cam, loc, look); return cam

def paim(cam, loc, look):
    cam.location = Vector(loc); d = Vector(look) - Vector(loc)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); cam.data.dof.focus_distance = d.length

def photos():
    for name in arg("--shots", "hero,close,top").split(","):
        sc, caps, pads, bar, _ = photo_scene()
        loc, look, lens, fstop, res = PSHOTS[name]
        pcamera(sc, loc, look, lens, fstop); sc.render.resolution_x, sc.render.resolution_y = res
        set_time(1.0, caps, pads, bar)
        sc.render.filepath = os.path.join(OUT, name + ".jpg")
        bpy.ops.render.render(write_still=True)

def film():
    shot = arg("--shot", "orbit"); frames = int(arg("--frames", "144"))
    first = int(arg("--first", "1")); step = int(arg("--step", "1"))
    sc, caps, pads, bar, _ = photo_scene()
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    cam = pcamera(sc, (0.6, -0.6, 0.4), (0.2, -0.1, 0), 42, 9.0)
    for f in range(first, frames + 1, step):
        t = (f - 1) / FPS; u = (f - 1) / (frames - 1)
        cutoff = 0.42
        if shot == "orbit":
            a = math.radians(-70 + 40 * u)
            loc = (0.205 + 0.62 * math.cos(a), -0.112 + 0.62 * math.sin(a), 0.36 - 0.04 * u); look = (0.205, -0.112, -0.004)
            cam.data.lens = 42; cam.data.dof.aperture_fstop = 9.0
        elif shot == "knobs":
            # A lo largo de la fila de perillas; Cutoff abre y cierra.
            loc = (0.10 + 0.16 * u, -0.17, 0.075); look = (0.20 + 0.14 * u, -0.052, 0.004)
            cam.data.lens = 58; cam.data.dof.aperture_fstop = 2.8
            cutoff = 0.42 + 0.3 * math.sin(2 * math.pi * t / 6.0)
        else:
            # Bajo sobre los pads mientras corre el patrón.
            loc = (0.17 + 0.14 * u, -0.20, 0.09); look = (0.24 + 0.14 * u, -0.118, 0.0)
            cam.data.lens = 50; cam.data.dof.aperture_fstop = 3.5
        paim(cam, loc, look)
        set_time(t, caps, pads, bar, cutoff)
        sc.render.filepath = os.path.join(OUT, f"{shot}_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)

# ── Film de la web (filmkit) ──
# Guion (128 BPM: un tiempo = 0,469 s; 4 tiempos = 45 cuadros justos). El sonido es el AT303 tocando Squelch Line,
# renderizado por at303render con la MISMA automatización que mueven las perillas (film_auto: --at cada 0,1 s en
# tools/film.json, que escribe `python3 scene303.py --mode filmjson`). El cuadro f de una toma es el instante
# (f0 + f − 1) / 24 del film y del audio, así los pads, las lámparas y las teclas caen con las notas.
#
#   1 open   8 t  3,75 s  Macro bajo entre la fila de perillas y la de pads, mirando a lo largo de los pads: la
#                         secuencia corre hacia el fondo, los pasos con acento se encienden rojos. Arranca en
#                         penumbra (los pads son la única luz) y el estudio sube; dolly lento hacia adelante.
#                         El tercio inferior izquierdo es placa oscura: ahí entra el título.
#   2 knob   8 t  3,75 s  La perilla Cutoff en macro (100 mm, f/2,2): foco en la escala y la serigrafía, rack al
#                         índice de la tapa justo cuando la perilla gira grande (36 → 86) y el filtro abre.
#                         Arco corto alrededor de la perilla, cámara en mano muy sutil.
#   3 lamps  6 t  2,81 s  Las lámparas de Accent y Slide en los botones parpadean con los pasos marcados; rack
#                         desde las lámparas al display del fondo, donde el punto del pulso late en cada tiempo.
#   4 keys   6 t  2,81 s  Rasante sobre el teclado desde la derecha: cada nota de la línea baja su tecla (la del
#                         slide se queda abajo hasta la nota siguiente); travelling lento a la izquierda.
#   5 hero  12 t  5,63 s  El instrumento entero: grúa en arco desde adelante a la izquierda que sube a tres cuartos
#                         de la derecha; un barrido de luz (tira rectangular, sólo reflejos) cruza la placa;
#                         Cutoff y Env Mod giran con la automatización.
#   6 end   10 t  4,69 s  Fundido de medio tiempo; tres cuartos de frente que se aleja despacio (el filtro se va
#                         cerrando); film.py oscurece y pone el cierre encima.
FILM_SHOTS = [("open", 8), ("knob", 8), ("lamps", 6), ("keys", 6), ("hero", 12), ("end", 10)]
BEAT = 60.0 / FILM_BPM
STEP16 = BEAT / 4
NOTES = [0, 12, 0, 3, 5, 0, 12, 10, 0, 7, 0, 3, 12, 0, 5, 3]     # Squelch Line, semitonos de cada paso
PRESET = {"cutoff": 42, "resonance": 88, "envmod": 62, "decay": 48, "accent": 78, "tuning": 50, "tempo": 55, "volume": 80}
AUTO_DT = 0.1

def shot_table():
    """(nombre, primer cuadro del film, cuadros) con el mismo redondeo que scripts/film.py."""
    res, f0 = [], 0
    for name, beats in FILM_SHOTS:
        n = int(round(beats * BEAT * FPS)); res.append((name, f0, n)); f0 += n
    return res

def _sm(a, b, t0, t1, t):
    u = min(1.0, max(0.0, (t - t0) / (t1 - t0))); u = u * u * (3 - 2 * u)
    return a + (b - a) * u

def film_auto(t):
    """Valores (0..100) de las perillas automatizadas en el instante t del film. Cutoff: casi cerrado en la
    apertura, giro grande en la toma de la perilla, vuelve a la mitad, sube con el héroe y se cierra en el final.
    Env Mod: sube durante el héroe y vuelve."""
    if t < 4.2: c = 34 + 2 * t / 4.2
    elif t < 7.6: c = _sm(36, 86, 4.2, 6.9, t)
    elif t < 12.8: c = _sm(86, 60, 7.6, 12.8, t)
    elif t < 19.2: c = _sm(60, 74, 12.8, 18.0, t)
    else: c = _sm(74, 48, 19.2, 23.4, t)
    e = _sm(62, 84, 13.0, 17.5, t) if t < 19.5 else _sm(84, 62, 19.5, 23.0, t)
    return {"cutoff": round(c, 1), "envmod": round(e, 1)}

def film_auto_points(total):
    return [(round(k * AUTO_DT, 2), film_auto(k * AUTO_DT)) for k in range(int(total / AUTO_DT) + 20)]

def knob_values(t):
    """Valores 0..1 de todas las perillas en t, interpolando los mismos puntos que recibe at303render."""
    v = {k: x / 100.0 for k, x in PRESET.items()}
    k = t / AUTO_DT; i = int(k); fr = k - i
    a, b = film_auto(i * AUTO_DT), film_auto((i + 1) * AUTO_DT)
    for p in a: v[p] = (a[p] + (b[p] - a[p]) * fr) / 100.0
    return v

def write_film_json():
    """tools/film.json: títulos, tomas en tiempos y el comando de audio con la automatización de film_auto (un --at
    cada vez que un valor cambia, en pasos de 0,1 s). Se corre fuera de Blender: python3 scene303.py --mode filmjson"""
    total = sum(n for _, _, n in shot_table()) / FPS
    head = ["build/plugins/at303/at303render_artefacts/Release/at303render", "{out}", "--preset", "Squelch Line",
            "--bpm", "%d" % FILM_BPM, "--seconds", "{seconds}"]
    last = film_auto(0.0)
    for p_, x in last.items(): head += ["--set", "%s=%g" % (p_, x)]
    ats = []
    for tt, vals in film_auto_points(total + 1.5):
        for p_, x in vals.items():
            if x != last[p_]: ats.append(["--at", "%.2f" % tt, "%s=%g" % (p_, x)]); last[p_] = x
    J = lambda xs: ", ".join(json.dumps(x) for x in xs)
    lines = ["{", '  "title": "AT303",', '  "tagline": "The squelch, from the circuit up.",',
             '  "fps": %d,' % FPS, '  "bpm": %d,' % FILM_BPM, '  "shots": [']
    sh = [json.dumps(dict({"name": n, "beats": b_}, **({"in": "dissolve"} if n == "end" else {}))) for n, b_ in FILM_SHOTS]
    lines += ["    " + x + ("," if i < len(sh) - 1 else "") for i, x in enumerate(sh)]
    lines += ["  ],", '  "poster": {"shot": "hero", "frame": 96},', '  "audio": {"cmd": [', "    " + J(head) + ","]
    rows = [J(sum(ats[i:i + 6], [])) for i in range(0, len(ats), 6)]
    lines += ["    " + r + ("," if i < len(rows) - 1 else "") for i, r in enumerate(rows)]
    lines += ["  ]}", "}"]
    p = os.path.join(HERE, "..", "film.json")
    open(p, "w").write("\n".join(lines) + "\n"); json.load(open(p))
    print(os.path.normpath(p), "%.2f s, %d cambios de perilla" % (total, len(ats)))

def step_state(t):
    """Paso que suena en t, cuánto hace que empezó, y el tiempo de inicio de cada paso la última vez que sonó."""
    k = int(t / STEP16 + 1e-6)
    return k % 16, t - k * STEP16, k

def film_scene():
    """La escena de las fotos con lo que el film anima: pads con brillo propio, lámparas de Accent y Slide en sus
    botones, el punto del pulso en el display, las teclas sueltas y una tira de luz para el barrido."""
    sc, caps, pad_mats, bar, bar_m = photo_scene()
    # Estudio más oscuro: mesa casi negra y mate; el aire un poco más negro para que la placa se recorte.
    sc.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.004, 0.004, 0.0045, 1)
    tb = bpy.data.materials["table"].node_tree.nodes["Principled BSDF"]
    tb.inputs["Base Color"].default_value = (0.012, 0.012, 0.013, 1); tb.inputs["Roughness"].default_value = 0.42
    # Pads: base esmerilada más oscura (apagados no deben parecer encendidos).
    for i, m in enumerate(pad_mats):
        b = m.node_tree.nodes["Principled BSDF"]
        b.inputs["Base Color"].default_value = (0.11, 0.105, 0.10, 1); b.inputs["Roughness"].default_value = 0.3
        b.inputs["Coat Weight"].default_value = 0.6
        b.inputs["Emission Color"].default_value = (1.0, 0.2, 0.1, 1) if i in ACC_STEPS else (1.0, 0.55, 0.15, 1)
    # Índice de las perillas un poco más largo y brillante: en macro tiene que leerse el giro.
    for c in caps.values():
        for ch in c.children:
            if ch.data.materials and ch.data.materials[0].name.startswith("ind"):
                r = ch.scale.y / 0.38
                ch.scale.y = r * 0.46; ch.scale.x *= 1.25; ch.location.y = r * 0.39
                ch.data.materials[0].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 3.0
    # Lámparas: lente redonda con domo, embutida en el extremo izquierdo de los botones Accent y Slide.
    lamps = {}
    for bid, col in (("acc", (1.0, 0.16, 0.07)), ("slide", (1.0, 0.55, 0.12))):
        bt = [b for b in LAYOUT["buttons"] if b["id"] == bid][0]
        m = mat("lamp_" + bid, (0.25 * col[0], 0.25 * col[1], 0.25 * col[2]), 0.12, emit=col, strength=0.0, coat=1.0)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=0.0019, location=bl(bt["x"] + 5.0, bt["y"] + bt["h"] / 2, 3.0))
        o = bpy.context.active_object; o.scale.z = 0.45; o.data.materials.append(m); bpy.ops.object.shade_smooth()
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.0025, depth=0.0006, location=bl(bt["x"] + 5.0, bt["y"] + bt["h"] / 2, 2.85))
        r = bpy.context.active_object; r.data.materials.append(bpy.data.materials["frame"])
        lamps[bid] = m
    # Punto del pulso en el display (abajo a la derecha del número).
    d = LAYOUT["display"]
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.0011, depth=0.0002, location=bl(d["x"] + d["w"] - 6, d["y"] + d["h"] - 5, 0.9))
    dot_m = emit_mat("dot", (1.0, 0.55, 0.12), 0.0); bpy.context.active_object.data.materials.append(dot_m)
    # Teclas: cada objeto con su semitono, por el centro de su malla (rounded_box deja el objeto en el origen).
    keys = {}
    for o in sc.objects:
        if o.type == "MESH" and o.name.startswith("key"):
            m_ = sum((v.co for v in o.data.vertices), Vector()) / len(o.data.vertices)
            for kk in LAYOUT["keys"]:
                c = bl(kk["x"] + kk["w"] / 2, kk["y"] + kk["h"] / 2)
                if abs(m_.x - c.x) < 0.002 and abs(m_.y - c.y) < 0.002: keys[kk["semi"]] = (o, o.location.z)
    # Tira de luz del barrido: rectángulo largo y angosto que mira hacia abajo (apagada salvo en el héroe).
    # Sólo reflejos: no ilumina en difuso (nada de charco en la mesa ni teclas quemadas), se ve como un brillo que
    # corre por el aluminio de las tapas, el canto de las teclas y la placa.
    l = bpy.data.lights.new("sweep", "AREA"); l.shape = "RECTANGLE"; l.size = 0.03; l.size_y = 0.20; l.energy = 0.0
    l.spread = math.radians(40); l.color = (1.0, 0.95, 0.88)
    sweep = bpy.data.objects.new("sweep", l); sc.collection.objects.link(sweep); sweep.location = (-0.2, -0.107, 0.12)
    sweep.visible_diffuse = False; sweep.visible_transmission = False; sweep.visible_volume_scatter = False
    # Enlace de luz: la tira alumbra el instrumento y no la mesa (su reflejo dejaba un brillo en el piso).
    rx = bpy.data.collections.new("sweep_rx")
    for o in sc.objects:
        if o.type in ("MESH", "CURVE", "FONT") and not any(m and m.name == "table" for m in o.data.materials): rx.objects.link(o)
    sweep.light_linking.receiver_collection = rx
    return sc, caps, pad_mats, bar, lamps, dot_m, keys, sweep

def film_state(t):
    """Brillo de cada pad, de las lámparas y del punto, y la tecla apretada en el instante t."""
    s, age, k = step_state(t)
    pads = []
    for i in range(16):
        # Último arranque del paso i (hasta t): un destello que decae, con un brillo bajo de "programado".
        back = (s - i) % 16; t_on = (k - back) * STEP16
        a = t - t_on if t_on >= 0 else 99.0
        pk = 16.0 if i in ACC_STEPS else 9.0
        pads.append(0.06 + pk * math.exp(-a / 0.11))
    acc = 30.0 * math.exp(-age / 0.09) if s in ACC_STEPS else 0.0
    sl = 0.0
    if s in SLIDE_STEPS: sl = 26.0
    elif (s - 1) % 16 in SLIDE_STEPS: sl = 26.0 * math.exp(-age / 0.06)     # el slide se apaga al llegar a la nota
    beat_age = t % BEAT
    dot = 18.0 * math.exp(-beat_age / 0.12)
    # Tecla: la nota del paso, apretada el 60 % del paso (o el paso entero si liga con slide).
    gate = STEP16 * (1.0 if s in SLIDE_STEPS else 0.6)
    down = {NOTES[s]: 1.0 if age < gate else max(0.0, 1 - (age - gate) / 0.03)}
    if (s - 1) % 16 in SLIDE_STEPS and age < 0.02: down[NOTES[(s - 1) % 16]] = 1 - age / 0.02
    return pads, acc, sl, dot, down

def accent_charge(t):
    charge = 0.0
    for j in range(int(t / STEP16) + 1):
        charge *= math.exp(-STEP16 / 0.45)
        if j % 16 in ACC_STEPS: charge = charge + (1 - charge) * 0.55
    # entre pasos decae
    return charge * math.exp(-(t - int(t / STEP16) * STEP16) / 0.45)

def film2():
    """Seis tomas con filmkit (guion arriba de FILM_SHOTS).
      blender -b --factory-startup -P scene303.py -- --mode film2 --audio <wav> --out <dir> [--shot a,b]
            [--quality draft|final] [--every N]   (--every: un cuadro de cada N, para mirar encuadres)"""
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final"); every = int(arg("--every", "1"))
    env = fk.envelope(arg("--audio")) if arg("--audio") else []
    V = lambda x, y, z: Vector((x, y, z))
    PAD = lambda i: V(0.12425 + 0.01675 * i, -0.1104, 0.003)
    KNOB = {k["id"]: V(k["x"] * 0.001, -k["y"] * 0.001, 0.0) for k in LAYOUT["big"] + LAYOUT["small"]}
    C = V(0.205, -0.107, 0.0)
    lampc = V(0.0525, -0.196, 0.003); disp = V(0.053, -0.049, 0.001)
    def arc(cx, a, r, z): return cx + V(math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r, z)
    cut = KNOB["cutoff"]
    paths = {
        "open":  fk.Path([(0, V(0.098, -0.074, 0.052), V(0.300, -0.118, -0.012), 55, 2.8),
                          (1, V(0.126, -0.077, 0.047), V(0.320, -0.118, -0.010), 58, 2.8)]),
        "knob":  fk.Path([(0, arc(cut, -72, 0.150, 0.085), cut + V(0, -0.004, 0.006), 100, 2.4),
                          (1, arc(cut, -50, 0.138, 0.074), cut + V(0, -0.003, 0.007), 100, 2.4)]),
        "lamps": fk.Path([(0, V(0.076, -0.330, 0.110), V(0.054, -0.170, 0.000), 85, 2.4),
                          (1, V(0.050, -0.324, 0.104), V(0.053, -0.110, 0.000), 85, 2.4)]),
        "keys":  fk.Path([(0, V(0.400, -0.310, 0.100), V(0.262, -0.176, 0.000), 60, 5.0),
                          (1, V(0.360, -0.322, 0.094), V(0.222, -0.172, 0.000), 60, 5.0)]),
        "hero":  fk.Path([(0, arc(C, -122, 0.60, 0.13), C + V(0, 0, -0.008), 38, 5.6),
                          (0.5, arc(C, -95, 0.60, 0.20), C + V(0, 0, -0.006), 39, 5.6),
                          (1, arc(C, -68, 0.62, 0.29), C + V(0, 0, -0.004), 40, 5.6)]),
        "end":   fk.Path([(0, C + V(0.20, -0.44, 0.25), C + V(0, 0, -0.004), 45, 5.6),
                          (1, C + V(0.28, -0.64, 0.38), C + V(0, 0, -0.004), 45, 5.6)], easing="out"),
    }
    shake = {"open": 0.0006, "knob": 0.0008, "lamps": 0.0006, "keys": 0.0006, "hero": 0.0, "end": 0.0}
    for (name, f0, frames) in shot_table():
        if only and name not in only.split(","): continue
        sc, caps, pad_mats, bar, lamps, dot_m, keys, sweep = film_scene()
        fk.setup(sc, quality)
        cam = fk.camera(sc); path = paths[name]
        hero_fr = frames

        def pose(f, t, u, name=name, path=path, f0=f0):
            T = (f0 + f - 1) / FPS
            vals = knob_values(T)
            for k, c in caps.items(): c.rotation_euler = (0, 0, knob_angle(vals[k]))
            ch = accent_charge(T); sw_ = LAYOUT["sweep"]
            bar.scale.x = max(0.0005, sw_["w"] * ch * 0.001)
            bar.location.x = bl(sw_["x"] + sw_["w"] * ch * 0.5, 0)[0]
            _, _, _, _, down = film_state(T)
            for semi, (o, z0) in keys.items(): o.location.z = z0 - 0.003 * down.get(semi, 0.0)
            focus = None
            if name == "open": focus = PAD(6)
            elif name == "knob":   # de la escala (adelante) al índice de la tapa cuando empieza a girar
                focus = (cut + V(0, -0.0185, 0.0)).lerp(cut + V(0, 0, 0.014), fk.rack(u, 0.08, 0.30, 0.0, 1.0))
            elif name == "lamps":  # de las lámparas al display
                focus = lampc.lerp(disp, fk.rack(u, 0.42, 0.78, 0.0, 1.0))
            elif name == "keys": focus = path.at(u)[1] + V(0, -0.006, 0.004)
            path.apply(cam, u, focus=focus, shake=shake[name], t=T)
            moved = list(caps.values()) + [bar] + [o for o, _ in keys.values()]
            if name == "hero":     # la tira cruza la placa de izquierda a derecha, con aceleración suave
                sweep.location = (fk.rack(u, 0.10, 0.92, -0.10, 0.52), -0.107, 0.12)
                moved.append(sweep)
            return moved

        studio = [(o.data, o.data.energy) for o in sc.objects if o.type == "LIGHT" and o.name in ("key", "fill", "rim")]

        def keys_(f, name=name, f0=f0, frames=frames):
            T = (f0 + f - 1) / FPS; u = (f - 1) / max(1, frames - 1)
            if name == "open":     # se abre en penumbra, con los pads como única luz, y el estudio sube despacio
                for ld, e0 in studio:
                    ld.energy = e0 * fk.rack(u, 0.0, 0.9, 0.18, 1.0); ld.keyframe_insert("energy")
            pads, acc, sl, dot, _ = film_state(T)
            for m, e in zip(pad_mats, pads):
                m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = e
                m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].keyframe_insert("default_value")
            for m, e in ((lamps["acc"], acc), (lamps["slide"], sl), (dot_m, dot)):
                m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = e
                m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].keyframe_insert("default_value")
            if name == "hero":
                sweep.data.energy = 6.0 * math.sin(math.pi * min(1.0, max(0.0, (u - 0.08) / 0.88))) ** 0.6
                sweep.data.keyframe_insert("energy")

        if every > 1:
            sc.frame_step = every
        fk.render_shot(sc, name, frames, pose, OUT, step=every, extra_keys=keys_)


if __name__ == "__main__" and arg("--mode", "ui") == "filmjson":
    write_film_json(); raise SystemExit
if __name__ == "__main__" and arg("--mode", "ui") == "film2":
    film2(); raise SystemExit
if __name__ == "__main__" and arg("--mode", "ui") == "photos":
    photos(); raise SystemExit
if __name__ == "__main__" and arg("--mode", "ui") == "film":
    film(); raise SystemExit

if __name__ == "__main__":
    write_layout()
    what = arg("--only", "all")
    if what in ("all", "bg"):
        sc = build(False); sc.render.filepath = os.path.join(OUT, "bg.png"); bpy.ops.render.render(write_still=True)
    if what in ("all", "down"):
        sc = build(True); sc.render.filepath = os.path.join(OUT, "keysdown.png"); bpy.ops.render.render(write_still=True)
    if what in ("all", "caps"):
        for c in ("big", "small"):
            sc = build(only_cap=c); sc.render.filepath = os.path.join(OUT, f"knob_{c}.png"); bpy.ops.render.render(write_still=True)
