"""Escena Blender del ATMD (instrumento completo: panel inclinado, listón, teclado y controlador izquierdo).

Corre en pc50-wsl (ver docs/ui-assets.md):
  flock ~/ai/prerendered/.gpu.lock blender -b -P scenemd.py -- --layout layoutmd.json --silk silkscreenmd.png \
        --lhc lhcmd.png --plate nameplatemd.png --out out --mode bg,sprites [--samples 512]

Modos: bg (fondo del editor + teclas abajo), sprites (perillas, teclas basculantes, lámparas, ruedas),
       hero (tres tomas 2400x1100), film (--shot wide|panel|keys --frames N --w --h), preview (rápido).

Unidades: 1 unidad de Blender = 10 mm. Marco del gabinete ("deck"): x a la derecha desde la cara exterior
de la mejilla izquierda, y hacia el músico, z hacia arriba → (x/10, −y/10, z/10).
Marco del panel: (px, py) mm de layoutmd.json (y hacia abajo de la cara) y h mm sobre la cara; gira
TILT alrededor de la bisagra: y = HY + (py−HPY)·cosT + h·sinT,  z = HZ − (py−HPY)·sinT + h·cosT.

Cámara del editor: ortográfica, mirando a lo largo de la bisectriz entre la normal del panel y la vertical
(PHI = TILT/2 desde la vertical). Los dos planos quedan con el mismo escorzo cos(PHI). En pantalla (mm):
  sx = x − X0,   sy = y·cosPHI − z·sinPHI − S0         (deck)
  sx = px + PX_OFF − X0,  sy = py·cos(T−PHI) + h·sin(T−PHI) + C_PANEL − S0   (panel)
"""
import bpy, bmesh, json, math, os, sys, time
from mathutils import Vector, Euler, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.normpath(os.path.join(HERE, "..", "..", "assets"))
def asset(n):
    """Archivo de entrada: el del directorio actual (como siempre) o, si no está, el de plugins/atmd/assets."""
    return n if os.path.exists(n) or not os.path.exists(os.path.join(ASSETS, n)) else os.path.join(ASSETS, n)

L = json.load(open(arg("--layout", asset("layoutmd.json"))))
SILK = os.path.abspath(arg("--silk", asset("silkscreenmd.png")))
LHC_TEX = os.path.abspath(arg("--lhc", asset("lhcmd.png")))
PLATE_TEX = os.path.abspath(arg("--plate", asset("nameplatemd.png")))
OUT = os.path.abspath(arg("--out", "out"))
SAMPLES = int(arg("--samples", "512"))
PXMM = float(arg("--pxmm", "4.0"))           # px por mm de las imágenes (2x de la UI)
os.makedirs(OUT, exist_ok=True)

# ───────── geometría del layout ─────────
PANEL = L["panel"]; CASE = L["case"]; KB = L["keyboard"]; LHC = L["deck"]["lhc"]; LOOK = L["look"]
TILT = math.radians(PANEL["tilt_deg"]["default"])
PHI = TILT / 2.0
PW_, PH_ = PANEL["size"]
HPY = PANEL["hinge"]["y"]
HY, HZ = CASE["hinge"]["y"], CASE["hinge"]["z"]
CW, CD, CH = CASE["size"]
RAIL = PANEL["rail"]
PX_OFF = CW / 2.0 - (RAIL["x0"] + RAIL["x1"]) / 2.0     # panel centrado entre las mejillas (27,4 mm)
SIDE_W = PANEL["side_strips"]["w"]
NB = CASE["name_board"]
NB_Z_FRONT = 88.0        # layout dice 78 (±10, estimado): con 78 las negras (tope 82) atravesarían el listón
NB_FACE_BOTTOM = 75.0    # canto frontal del listón: tapa la cola de las teclas
WHITE_TOP, BLACK_TOP = KB["z_white_top"], KB["z_black_top"]
WHITE_BOTTOM = 59.0
LIP = CASE["front_lip"]
KEY_PIVOT_Y = 150.0      # las teclas pivotan bajo el panel; adelante bajan KEY_DIP mm
KEY_DIP = 9.0
LHC_Z = LHC["z_top"]
COS_P, SIN_P = math.cos(PHI), math.sin(PHI)


def srgb(h):
    h = h.lstrip("#"); c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


def D(x, y, z=0.0): return Vector((x / 10.0, -y / 10.0, z / 10.0))


def panel_to_case(px, py, h=0.0):
    u = py - HPY
    return (px + PX_OFF, HY + u * math.cos(TILT) + h * math.sin(TILT), HZ - u * math.sin(TILT) + h * math.cos(TILT))


def screen_mm(x, y, z):
    """Coordenadas de pantalla (mm, sin origen) de un punto del gabinete."""
    return x, y * COS_P - z * SIN_P


class Frame:
    """Marco local: (a, b, c) mm con b hacia el músico/abajo de la cara → (a/10, −(b−b0)/10, c/10) en su raíz."""
    def __init__(self, b0=0.0, root=None):
        self.b0 = b0; self.root = root
    def v(self, a, b, c=0.0): return Vector((a / 10.0, -(b - self.b0) / 10.0, c / 10.0))
    def own(self, o):
        if self.root is not None: o.parent = self.root
        return o


# ───────── escena ─────────
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    try:                                   # OptiX no inicia en WSL y Cycles cae a CPU sin avisar
        prefs.compute_device_type = "CUDA"; prefs.get_devices()
        for d in prefs.devices: d.use = d.type == "CUDA"
    except Exception as e: print("CUDA?", e)
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    if hasattr(sc.cycles, "denoising_use_gpu"): sc.cycles.denoising_use_gpu = True
    sc.cycles.max_bounces = 10; sc.cycles.transparent_max_bounces = 16; sc.cycles.caustics_reflective = False
    sc.cycles.caustics_refractive = False; sc.cycles.blur_glossy = 0.5
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    sc.render.image_settings.color_depth = "8"
    sc.render.film_transparent = True
    world()
    return sc


def world(strength=1.0):
    """Cúpula de estudio procedural (sólo depende de la dirección: igual para el fondo y los sprites)."""
    w = bpy.data.worlds.new("W"); bpy.context.scene.world = w; w.use_nodes = True
    N = w.node_tree.nodes; Lk = w.node_tree.links
    bg = N["Background"]
    tc = N.new("ShaderNodeTexCoord")
    dirv = N.new("ShaderNodeVectorMath"); dirv.operation = "NORMALIZE"; Lk.new(tc.outputs["Generated"], dirv.inputs[0])
    sep = N.new("ShaderNodeSeparateXYZ"); Lk.new(dirv.outputs["Vector"], sep.inputs[0])
    # Base: piso oscuro, cielo gris; caja de luz grande arriba-adelante-izquierda.
    up = N.new("ShaderNodeMapRange"); Lk.new(sep.outputs["Z"], up.inputs["Value"])
    up.inputs["From Min"].default_value = -0.2; up.inputs["From Max"].default_value = 1.0
    up.inputs["To Min"].default_value = 0.015; up.inputs["To Max"].default_value = 0.22
    box = N.new("ShaderNodeVectorMath"); box.operation = "DOT_PRODUCT"; Lk.new(dirv.outputs["Vector"], box.inputs[0])
    box.inputs[1].default_value = Vector((-0.30, -0.45, 0.84)).normalized()
    soft = N.new("ShaderNodeMapRange"); Lk.new(box.outputs["Value"], soft.inputs["Value"])
    soft.inputs["From Min"].default_value = 0.80; soft.inputs["From Max"].default_value = 0.96
    soft.inputs["To Min"].default_value = 0.0; soft.inputs["To Max"].default_value = 2.4
    soft.interpolation_type = "SMOOTHSTEP"
    rim = N.new("ShaderNodeVectorMath"); rim.operation = "DOT_PRODUCT"; Lk.new(dirv.outputs["Vector"], rim.inputs[0])
    rim.inputs[1].default_value = Vector((0.55, 0.35, 0.76)).normalized()
    rimr = N.new("ShaderNodeMapRange"); Lk.new(rim.outputs["Value"], rimr.inputs["Value"])
    rimr.inputs["From Min"].default_value = 0.86; rimr.inputs["From Max"].default_value = 0.97
    rimr.inputs["To Min"].default_value = 0.0; rimr.inputs["To Max"].default_value = 0.9
    rimr.interpolation_type = "SMOOTHSTEP"
    a1 = N.new("ShaderNodeMath"); a1.operation = "ADD"; Lk.new(up.outputs["Result"], a1.inputs[0]); Lk.new(soft.outputs["Result"], a1.inputs[1])
    a2 = N.new("ShaderNodeMath"); a2.operation = "ADD"; Lk.new(a1.outputs["Value"], a2.inputs[0]); Lk.new(rimr.outputs["Result"], a2.inputs[1])
    m = N.new("ShaderNodeMath"); m.operation = "MULTIPLY"; Lk.new(a2.outputs["Value"], m.inputs[0]); m.inputs[1].default_value = strength
    bg.inputs["Color"].default_value = (0.95, 0.96, 1.0, 1)
    Lk.new(m.outputs["Value"], bg.inputs["Strength"])


def sun(name, travel, energy, angle, color=(1, 1, 1)):
    l = bpy.data.lights.new(name, "SUN"); l.energy = energy; l.angle = math.radians(angle); l.color = color
    o = bpy.data.objects.new(name, l); bpy.context.collection.objects.link(o)
    o.rotation_euler = Vector(travel).normalized().to_track_quat("-Z", "Y").to_euler()
    return o


def lights():
    sun("Key", (0.35, 0.40, -0.85), 2.6, 14, (1.0, 0.965, 0.92))
    sun("Fill", (-0.62, 0.25, -0.74), 0.45, 40, (0.86, 0.91, 1.0))
    sun("Rim", (-0.15, -0.55, -0.82), 0.7, 8, (1, 1, 1))


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


# ───────── materiales ─────────
M = {}


def mix_rgba(N):
    """Nodo Mix en modo color; devuelve (nodo, factor, A, B, salida) buscando los sockets por tipo."""
    m = N.new("ShaderNodeMix"); m.data_type = "RGBA"
    ins = [s for s in m.inputs if s.type == "RGBA"]
    out = next(s for s in m.outputs if s.type == "RGBA")
    return m, m.inputs[0], ins[0], ins[1], out


def principled(name, col, rough, metal=0.0, coat=0.0, spec=0.5, aniso=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Specular IOR Level"].default_value = spec
    if aniso: b.inputs["Anisotropic"].default_value = aniso
    return m


def bump_noise(m, scale, strength, distance=0.002, coords="Object", stretch=None):
    nt = m.node_tree; N = nt.nodes; Lk = nt.links; b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord"); nz = N.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = scale; nz.inputs["Detail"].default_value = 6.0
    if stretch:
        mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = stretch
        Lk.new(tc.outputs[coords], mp.inputs["Vector"]); Lk.new(mp.outputs["Vector"], nz.inputs["Vector"])
    else:
        Lk.new(tc.outputs[coords], nz.inputs["Vector"])
    bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = strength; bp.inputs["Distance"].default_value = distance
    Lk.new(nz.outputs["Fac"], bp.inputs["Height"]); Lk.new(bp.outputs["Normal"], b.inputs["Normal"])
    return nz


def printed(name, base_hex, tex_path, size_mm, flip=False):
    """Pintura negra satinada con serigrafía (imagen RGBA, tinta blanca) mapeada por coordenadas de objeto.
    El origen del objeto es la esquina superior izquierda de la imagen; size_mm = (ancho, alto)."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord")
    mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (10.0 / size_mm[0], 10.0 / size_mm[1], 1.0)
    mp.inputs["Location"].default_value = (0.0, 1.0, 0.0)
    Lk.new(tc.outputs["Object"], mp.inputs["Vector"])
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(tex_path); tex.extension = "CLIP"
    tex.interpolation = "Cubic"; Lk.new(mp.outputs["Vector"], tex.inputs["Vector"])
    mix, fac, ma, mb, mo = mix_rgba(N)
    Lk.new(tex.outputs["Alpha"], fac)
    ma.default_value = (*srgb(base_hex), 1)
    mb.default_value = (*srgb(LOOK["legend_color"]), 1)
    Lk.new(mo, b.inputs["Base Color"])
    rr = N.new("ShaderNodeMapRange"); Lk.new(tex.outputs["Alpha"], rr.inputs["Value"])
    rr.inputs["To Min"].default_value = 0.46; rr.inputs["To Max"].default_value = 0.38
    Lk.new(rr.outputs["Result"], b.inputs["Roughness"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 900.0; nz.inputs["Detail"].default_value = 4.0
    Lk.new(tc.outputs["Object"], nz.inputs["Vector"])
    hsum = N.new("ShaderNodeMath"); hsum.operation = "MULTIPLY_ADD"
    Lk.new(nz.outputs["Fac"], hsum.inputs[0]); hsum.inputs[1].default_value = 0.25; Lk.new(tex.outputs["Alpha"], hsum.inputs[2])
    bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.10; bp.inputs["Distance"].default_value = 0.0015
    Lk.new(hsum.outputs["Value"], bp.inputs["Height"]); Lk.new(bp.outputs["Normal"], b.inputs["Normal"])
    b.inputs["Specular IOR Level"].default_value = 0.45; b.inputs["Coat Weight"].default_value = 0.05
    return m


def nameplate_mat():
    m = bpy.data.materials.new("Nameplate"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]; p = NB["nameplate"]
    tc = N.new("ShaderNodeTexCoord")
    mp = N.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (10.0 / (p["x1"] - p["x0"]), 10.0 / (p["y1"] - p["y0"]), 1.0)
    mp.inputs["Location"].default_value = (0.0, 1.0, 0.0)
    Lk.new(tc.outputs["Object"], mp.inputs["Vector"])
    tex = N.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(PLATE_TEX); tex.extension = "EXTEND"
    Lk.new(mp.outputs["Vector"], tex.inputs["Vector"]); Lk.new(tex.outputs["Color"], b.inputs["Base Color"])
    sep = N.new("ShaderNodeSeparateColor"); Lk.new(tex.outputs["Color"], sep.inputs[0])
    mt = N.new("ShaderNodeMapRange"); Lk.new(sep.outputs[0], mt.inputs["Value"])
    mt.inputs["From Min"].default_value = 0.05; mt.inputs["From Max"].default_value = 0.4
    Lk.new(mt.outputs["Result"], b.inputs["Metallic"])
    b.inputs["Roughness"].default_value = 0.32
    bump_noise(m, 60.0, 0.05, stretch=(1.0, 40.0, 1.0))
    return m


def wood(name, axis="X", tint=1.0):
    """Nogal procedural: tono de fondo que varía lento, veta fina a lo largo del eje y poros; satinado."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord")
    st = {"X": (0.07, 1.0, 1.0), "Y": (1.0, 0.07, 1.0)}[axis]
    mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = st
    mp.inputs["Location"].default_value = (3.7, 1.3, -2.1)
    Lk.new(tc.outputs["Object"], mp.inputs["Vector"])
    # Veta: anillos alrededor del eje con distorsión de dos escalas (espaciado irregular, figura ondulada).
    n1 = N.new("ShaderNodeTexNoise"); n1.inputs["Scale"].default_value = 0.25; n1.inputs["Detail"].default_value = 2.0
    Lk.new(mp.outputs["Vector"], n1.inputs["Vector"])
    n2 = N.new("ShaderNodeTexNoise"); n2.inputs["Scale"].default_value = 2.2; n2.inputs["Detail"].default_value = 3.0
    Lk.new(mp.outputs["Vector"], n2.inputs["Vector"])
    d1 = N.new("ShaderNodeVectorMath"); d1.operation = "MULTIPLY_ADD"
    Lk.new(n1.outputs["Color"], d1.inputs[0]); d1.inputs[1].default_value = (2.6, 2.6, 2.6); Lk.new(mp.outputs["Vector"], d1.inputs[2])
    d2 = N.new("ShaderNodeVectorMath"); d2.operation = "MULTIPLY_ADD"
    Lk.new(n2.outputs["Color"], d2.inputs[0]); d2.inputs[1].default_value = (0.25, 0.25, 0.25); Lk.new(d1.outputs["Vector"], d2.inputs[2])
    wv = N.new("ShaderNodeTexWave"); wv.wave_type = "RINGS"; wv.rings_direction = axis
    wv.inputs["Scale"].default_value = 0.5; wv.inputs["Distortion"].default_value = 0.0
    wv.inputs["Detail"].default_value = 0.0
    Lk.new(d2.outputs["Vector"], wv.inputs["Vector"])
    pw = N.new("ShaderNodeMath"); pw.operation = "POWER"; Lk.new(wv.outputs["Fac"], pw.inputs[0]); pw.inputs[1].default_value = 5.0
    # Variación lenta del tono.
    big = N.new("ShaderNodeTexNoise"); big.inputs["Scale"].default_value = 0.35; big.inputs["Detail"].default_value = 2.0
    Lk.new(mp.outputs["Vector"], big.inputs["Vector"])
    # Poros: ruido fino muy estirado a lo largo de la veta.
    mpp = N.new("ShaderNodeMapping"); mpp.inputs["Scale"].default_value = tuple(v * 1.0 for v in ({"X": (0.02, 1.0, 1.0), "Y": (1.0, 0.02, 1.0)}[axis]))
    Lk.new(tc.outputs["Object"], mpp.inputs["Vector"])
    pore = N.new("ShaderNodeTexNoise"); pore.inputs["Scale"].default_value = 90.0; pore.inputs["Detail"].default_value = 4.0
    Lk.new(mpp.outputs["Vector"], pore.inputs["Vector"])
    base = srgb(CASE["wood"]["color"])
    ramp = N.new("ShaderNodeValToRGB"); Lk.new(big.outputs["Fac"], ramp.inputs["Fac"])
    ramp.color_ramp.elements[0].position = 0.3; ramp.color_ramp.elements[0].color = (*(c * 0.62 * tint for c in base), 1)
    ramp.color_ramp.elements[1].position = 0.75; ramp.color_ramp.elements[1].color = (*(min(1, c * 1.12 * tint) for c in base), 1)
    g = N.new("ShaderNodeMath"); g.operation = "MULTIPLY_ADD"; Lk.new(pw.outputs["Value"], g.inputs[0]); g.inputs[1].default_value = 0.42
    Lk.new(pore.outputs["Fac"], g.inputs[2])
    gm = N.new("ShaderNodeMapRange"); Lk.new(g.outputs["Value"], gm.inputs["Value"])
    gm.inputs["From Min"].default_value = 0.3; gm.inputs["From Max"].default_value = 1.0
    gm.inputs["To Min"].default_value = 1.0; gm.inputs["To Max"].default_value = 0.55
    mul, fac, ma, mb, mo = mix_rgba(N); mul.blend_type = "MULTIPLY"; fac.default_value = 1.0
    Lk.new(ramp.outputs["Color"], ma)
    cmb = N.new("ShaderNodeCombineColor"); Lk.new(gm.outputs["Result"], cmb.inputs[0]); Lk.new(gm.outputs["Result"], cmb.inputs[1]); Lk.new(gm.outputs["Result"], cmb.inputs[2])
    Lk.new(cmb.outputs[0], mb)
    Lk.new(mo, b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.55; b.inputs["Coat Weight"].default_value = 0.12
    b.inputs["Coat Roughness"].default_value = 0.35; b.inputs["Specular IOR Level"].default_value = 0.35
    bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.05; bp.inputs["Distance"].default_value = 0.002
    Lk.new(g.outputs["Value"], bp.inputs["Height"]); Lk.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m


def brushed(name, col=(0.80, 0.81, 0.82), rough=0.26, axis="Y", radial=False):
    m = principled(name, col, rough, metal=1.0, aniso=0.75)
    nt = m.node_tree; N = nt.nodes; Lk = nt.links; b = N["Principled BSDF"]
    tg = N.new("ShaderNodeTangent")
    if radial: tg.direction_type = "RADIAL"; tg.axis = "Z"
    else: tg.direction_type = "RADIAL"; tg.axis = axis
    Lk.new(tg.outputs["Tangent"], b.inputs["Tangent"])
    tc = N.new("ShaderNodeTexCoord")
    if radial:     # anillos concéntricos del torneado
        wv = N.new("ShaderNodeTexWave"); wv.wave_type = "RINGS"; wv.rings_direction = "SPHERICAL"
        wv.inputs["Scale"].default_value = 60.0; wv.inputs["Distortion"].default_value = 3.0; wv.inputs["Detail"].default_value = 6.0
        Lk.new(tc.outputs["Object"], wv.inputs["Vector"]); src = wv.outputs["Fac"]
    else:
        nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 30.0; nz.inputs["Detail"].default_value = 10.0
        mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (1.0, 1.0, 1.0)
        st = {"X": (1.0, 60.0, 60.0), "Y": (60.0, 1.0, 60.0), "Z": (60.0, 60.0, 1.0)}[axis]
        mp.inputs["Scale"].default_value = st
        Lk.new(tc.outputs["Object"], mp.inputs["Vector"]); Lk.new(mp.outputs["Vector"], nz.inputs["Vector"]); src = nz.outputs["Fac"]
    bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.06; bp.inputs["Distance"].default_value = 0.001
    Lk.new(src, bp.inputs["Height"]); Lk.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m


def knob_plastic(name, pointer_r0=None, pointer_r1=None, pointer_w=0.45):
    """Plástico negro brillante; con pointer_r0/1 pinta la raya blanca del índice sobre la pollera (en coords de objeto)."""
    m = principled(name, (0.0065, 0.0065, 0.0072), 0.24, coat=0.35, spec=0.55)
    nt = m.node_tree; N = nt.nodes; Lk = nt.links; b = N["Principled BSDF"]
    b.inputs["Coat Roughness"].default_value = 0.08
    if pointer_r0 is None: return m
    tc = N.new("ShaderNodeTexCoord"); sep = N.new("ShaderNodeSeparateXYZ"); Lk.new(tc.outputs["Object"], sep.inputs[0])
    ax = N.new("ShaderNodeMath"); ax.operation = "ABSOLUTE"; Lk.new(sep.outputs["X"], ax.inputs[0])
    c1 = N.new("ShaderNodeMath"); c1.operation = "LESS_THAN"; Lk.new(ax.outputs["Value"], c1.inputs[0]); c1.inputs[1].default_value = pointer_w / 20.0
    c2 = N.new("ShaderNodeMath"); c2.operation = "GREATER_THAN"; Lk.new(sep.outputs["Y"], c2.inputs[0]); c2.inputs[1].default_value = pointer_r0 / 10.0
    c3 = N.new("ShaderNodeMath"); c3.operation = "LESS_THAN"; Lk.new(sep.outputs["Y"], c3.inputs[0]); c3.inputs[1].default_value = pointer_r1 / 10.0
    m1 = N.new("ShaderNodeMath"); m1.operation = "MULTIPLY"; Lk.new(c1.outputs["Value"], m1.inputs[0]); Lk.new(c2.outputs["Value"], m1.inputs[1])
    m2 = N.new("ShaderNodeMath"); m2.operation = "MULTIPLY"; Lk.new(m1.outputs["Value"], m2.inputs[0]); Lk.new(c3.outputs["Value"], m2.inputs[1])
    mix, fac, ma, mb, mo = mix_rgba(N); Lk.new(m2.outputs["Value"], fac)
    ma.default_value = (0.0065, 0.0065, 0.0072, 1); mb.default_value = (0.80, 0.79, 0.74, 1)
    Lk.new(mo, b.inputs["Base Color"])
    return m


def glass(name, col, emit=0.0, emit_col=None):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = 0.04
    b.inputs["Transmission Weight"].default_value = 1.0; b.inputs["IOR"].default_value = 1.52
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*(emit_col or col), 1); b.inputs["Emission Strength"].default_value = emit
    return m


def emissive(name, col, strength):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Emission Color"].default_value = (*col, 1)
    b.inputs["Emission Strength"].default_value = strength
    return m


def materials():
    M.clear()
    M["panel"] = printed("Panel", LOOK["panel_color"], SILK, (PW_, PH_))
    lw, lh = LHC["x1"] - LHC["x0"], LHC["y1"] - LHC["y0"]
    M["lhc"] = printed("LhcPlate", LOOK["panel_color"], LHC_TEX, (lw, lh))
    M["lhc"].node_tree.nodes["Principled BSDF"].inputs["Coat Weight"].default_value = 0.0
    M["paint"] = principled("PanelPlain", srgb(LOOK["panel_color"]), 0.46, spec=0.45)
    M["plate"] = nameplate_mat()
    M["wood_x"] = wood("WoodX", "X"); M["wood_y"] = wood("WoodY", "Y", 0.95)
    M["alu"] = brushed("SidePlate", (0.78, 0.79, 0.80), 0.3, axis="Z")
    M["strip"] = brushed("SideStrip", (0.80, 0.81, 0.82), 0.24, axis="Y")
    M["insert"] = brushed("Insert", (0.80, 0.80, 0.81), 0.30, radial=True)
    M["chrome"] = principled("Chrome", (0.88, 0.88, 0.88), 0.08, metal=1.0)
    M["nickel"] = principled("Nickel", (0.72, 0.71, 0.68), 0.22, metal=1.0)
    M["screw"] = principled("Screw", (0.16, 0.16, 0.16), 0.35, metal=1.0)
    M["hole"] = principled("Hole", (0.003, 0.003, 0.003), 0.9, spec=0.05)
    M["black"] = principled("BlackPlastic", (0.010, 0.010, 0.011), 0.45)
    M["dark"] = principled("Dark", (0.02, 0.02, 0.021), 0.6)
    M["felt"] = principled("Felt", (0.012, 0.010, 0.010), 0.95, spec=0.1)
    M["knob"] = knob_plastic("Knob")
    M["knob_std"] = knob_plastic("KnobStd", 10.0, 12.4)
    M["knob_big"] = knob_plastic("KnobBig", 12.1, 14.9, 0.5)
    M["white_paint"] = principled("PointerLine", (0.80, 0.79, 0.74), 0.4)
    wk = principled("WhiteKey", srgb(LOOK["white_key"]), 0.2, coat=0.35, spec=0.5)
    b = wk.node_tree.nodes["Principled BSDF"]; b.inputs["Subsurface Weight"].default_value = 0.12
    b.inputs["Subsurface Radius"].default_value = (0.2, 0.15, 0.1); b.inputs["Subsurface Scale"].default_value = 0.05
    M["white_key"] = wk
    M["black_key"] = principled("BlackKey", srgb(LOOK["black_key"]), 0.16, coat=0.5, spec=0.55)
    for c, h in LOOK["rocker"].items():
        M["rocker_" + c] = principled("Rocker_" + c, srgb(h), 0.34 if c != "black" else 0.3, coat=0.12, spec=0.5)
    M["wheel"] = principled("Wheel", tuple(c * 0.55 for c in srgb(LOOK["rocker"]["cream"])), 0.5, spec=0.4)
    M["floor"] = principled("Floor", (0.016, 0.016, 0.017), 0.5, spec=0.3)


# ───────── utilidades de malla ─────────
def link(o): bpy.context.collection.objects.link(o); return o


def mesh(name, bm, m, smooth=True, mats=None, wn=False):
    """wn: normales ponderadas por área (caras grandes planas, biseles redondeados). Sin esto, las cajas
    biseladas con sombreado suave se ven como troncos y las teclas con reflejos en gota."""
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    if smooth:
        for p in me.polygons: p.use_smooth = True
    me.materials.append(m)
    for extra in (mats or []): me.materials.append(extra)
    o = link(bpy.data.objects.new(name, me))
    if smooth and wn:
        # Aristas vivas (sin bisel) marcadas como duras: si no, la cara frontal de las blancas promediaba con
        # la base (área enorme) y salía con triángulos oscuros.
        me.set_sharp_from_angle(angle=math.radians(50))
        md = o.modifiers.new("WN", "WEIGHTED_NORMAL"); md.mode = "FACE_AREA"; md.weight = 100; md.keep_sharp = True
    return o


def box(fr, name, a0, b0, a1, b1, c0, c1, m, bevel=0.3, segs=3):
    """Caja en el marco fr (mm). a: derecha, b: hacia el músico/abajo de la cara, c: altura."""
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    lo, hi = fr.v(a0, b1, c0), fr.v(a1, b0, c1)
    for v in bm.verts:
        v.co = Vector((lo.x + (v.co.x + 0.5) * (hi.x - lo.x), lo.y + (v.co.y + 0.5) * (hi.y - lo.y), lo.z + (v.co.z + 0.5) * (hi.z - lo.z)))
    if bevel > 0: bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel / 10, segments=segs, affect="EDGES", clamp_overlap=True)
    return fr.own(mesh(name, bm, m, wn=True))


def prism(fr, name, poly_bc, a0, a1, m, bevel=0.0, segs=3, smooth=True):
    """Extrusión en a (ancho) de un polígono en el plano (b, c) mm."""
    bm = bmesh.new()
    va = [bm.verts.new(fr.v(a0, b, c)) for b, c in poly_bc]
    vb = [bm.verts.new(fr.v(a1, b, c)) for b, c in poly_bc]
    n = len(poly_bc)
    f0 = bm.faces.new(va); f1 = bm.faces.new(vb[::-1])
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((va[i], va[j], vb[j], vb[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    if bevel > 0: bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel / 10, segments=segs, affect="EDGES", clamp_overlap=True)
    return fr.own(mesh(name, bm, m, smooth, wn=True))


def lathe(profile, segs=96, radial=None):
    """Sólido de revolución alrededor de Z local (mm): profile = [(r, z), ...] de abajo hacia arriba."""
    bm = bmesh.new(); rings = []
    for r, z in profile:
        ring = []
        for i in range(segs):
            t = 2 * math.pi * i / segs; rr = radial(t, r, z) if radial else r
            ring.append(bm.verts.new((rr * math.sin(t) / 10, rr * math.cos(t) / 10, z / 10)))
        rings.append(ring)
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(segs):
            j = (i + 1) % segs; bm.faces.new((a[i], b[i], b[j], a[j]))
    if profile[-1][0] > 0:
        c = bm.verts.new((0, 0, profile[-1][1] / 10))
        for i in range(segs): bm.faces.new((rings[-1][i], c, rings[-1][(i + 1) % segs]))
    if profile[0][0] > 0:
        c = bm.verts.new((0, 0, profile[0][1] / 10))
        for i in range(segs): bm.faces.new((rings[0][(i + 1) % segs], c, rings[0][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def place(fr, o, a, b, c=0.0, rot_z=0.0):
    o.location = fr.v(a, b, c); o.rotation_euler = Euler((0, 0, rot_z), "XYZ"); fr.own(o); return o


def plate_with_holes(fr, name, a0, b0, a1, b1, c0, c1, holes, m, bevel=0.5):
    """Placa con agujeros rectangulares (modificador booleano exacto, sin aplicar)."""
    o = box(fr, name, a0, b0, a1, b1, c0, c1, m, bevel)
    for i, (ha0, hb0, ha1, hb1) in enumerate(holes):
        cut = box(fr, f"{name}_cut{i}", ha0, hb0, ha1, hb1, c0 - 5, c1 + 5, m, 0.0)
        cut.hide_render = True; cut.hide_viewport = True
        md = o.modifiers.new(f"hole{i}", "BOOLEAN"); md.operation = "DIFFERENCE"; md.object = cut; md.solver = "EXACT"
    # Las normales ponderadas van después del booleano (antes, las caras nuevas salían con rayas).
    for md in [md for md in o.modifiers if md.type == "WEIGHTED_NORMAL"]: o.modifiers.remove(md)
    md = o.modifiers.new("WN", "WEIGHTED_NORMAL"); md.mode = "FACE_AREA"; md.weight = 100; md.keep_sharp = True
    return o


def slot_well(fr, name, cx, cy, sw, sl, z_top, depth=14.0):
    """Pozo oscuro bajo la abertura de una rueda: cuatro paredes finas y piso."""
    t = 0.8; a0, a1, b0, b1 = cx - sw / 2, cx + sw / 2, cy - sl / 2, cy + sl / 2; zb = z_top - depth
    for i, (x0, y0, x1, y1) in enumerate(((a0 - t, b0 - t, a0, b1 + t), (a1, b0 - t, a1 + t, b1 + t),
                                          (a0, b0 - t, a1, b0), (a0, b1, a1, b1 + t))):
        box(fr, f"{name}W{i}", x0, y0, x1, y1, zb, z_top - 1.0, M["hole"], 0.0)
    box(fr, f"{name}F", a0 - t, b0 - t, a1 + t, b1 + t, zb - 1.0, zb, M["hole"], 0.0)


# ───────── marcos ─────────
def panel_frame(at_origin=False):
    root = bpy.data.objects.new("PanelRoot", None); link(root)
    root.location = Vector((0, 0, 0)) if at_origin else D(PX_OFF, HY, HZ)
    root.rotation_euler = Euler((TILT, 0, 0), "XYZ")
    return Frame(0.0 if at_origin else HPY, root)


DECK = Frame(0.0, None)
LOCAL = Frame(0.0, None)      # marco sin raíz para piezas centradas en el origen


# ───────── controles (centrados en el origen de su marco) ─────────
def knob_standard(big=False):
    s = 1.2 if big else 1.0
    r = 12.5 * s; ri = (9.3 if big else 7.8)
    prof = [(r, 0.0), (r, 0.9), (r - 0.4, 1.6), (r - 1.5, 2.6), (r - 2.5 * s, 3.4), (9.7 * s, 3.9), (9.6 * s, 4.2),
            (9.25 * s, 13.6), (9.05 * s, 14.3), (8.55 * s, 14.8), (ri + 0.05, 14.95), (ri + 0.05, 14.6)]
    flute = lambda t, rr, z: rr - (0.11 * (0.5 + 0.5 * math.cos(t * 44)) if 4.6 < z < 13.4 else 0.0)
    k = mesh("Knob", lathe(prof, 176, flute), M["knob_big" if big else "knob_std"])
    ins = mesh("Insert", lathe([(ri, 14.4), (ri, 15.0), (ri - 0.3, 15.25), (0.0, 15.3)], 128), M["insert"])
    ins.parent = k
    return k


def knob_pointer():
    """Perilla de los selectores RANGE/WAVEFORM: cuerpo negro, inserto chico y aleta-índice con raya blanca."""
    k = mesh("PKnob", lathe([(11.0, 0.0), (11.0, 0.9), (10.4, 1.9), (9.0, 3.0), (7.6, 3.8), (7.3, 4.4),
                             (7.05, 11.8), (6.8, 12.6), (6.3, 13.0), (5.9, 13.05), (5.9, 12.7)], 160), M["knob"])
    ins = mesh("PInsert", lathe([(5.85, 12.6), (5.85, 13.1), (5.6, 13.3), (0.0, 13.35)], 96), M["insert"]); ins.parent = k
    # Aleta: de r=5 a r=16 (pointer_len), alta junto al cuerpo y baja en la punta.
    bm = bmesh.new()
    sec = [(4.5, 1.7, 11.0), (12.0, 1.15, 4.4), (15.8, 0.8, 2.4)]
    rows = []
    for y, hw, top in sec:
        rows.append([bm.verts.new((-hw / 10, y / 10, 0.0)), bm.verts.new((hw / 10, y / 10, 0.0)),
                     bm.verts.new((hw * 0.55 / 10, y / 10, top / 10)), bm.verts.new((-hw * 0.55 / 10, y / 10, top / 10))])
    for a, b in zip(rows[:-1], rows[1:]):
        for i in range(4):
            j = (i + 1) % 4; bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(rows[0][::-1]); bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.035, segments=2, affect="EDGES", clamp_overlap=True)
    fin = mesh("PFin", bm, M["knob"]); fin.parent = k
    # Raya blanca sobre el lomo de la aleta.
    bm = bmesh.new(); pts = []
    for y, hw, top in sec:
        pts.append((y, top))
    strip = []
    for y, top in pts:
        strip.append((bm.verts.new((-0.035, y / 10, (top + 0.06) / 10)), bm.verts.new((0.035, y / 10, (top + 0.06) / 10))))
    for (a0, a1), (b0, b1) in zip(strip[:-1], strip[1:]): bm.faces.new((a0, a1, b1, b0))
    ln = mesh("PLine", bm, M["white_paint"], smooth=False); ln.parent = k
    return k


def rocker_cap(color, w, h, orient, state, frame_deck=False):
    """Tecla basculante: perfil en V (extremos altos); index 0 = izquierda/abajo hundida, 1 = derecha/arriba."""
    hw = w / 2.0
    prof = [(-hw, 0.0), (hw, 0.0), (hw, 5.2), (hw - 0.8, 5.35), (1.6, 3.35), (0.0, 3.2), (-1.6, 3.35), (-hw + 0.8, 5.35), (-hw, 5.2)]
    bm = bmesh.new(); n = len(prof)
    va = [bm.verts.new((u / 10, -h / 20, z / 10 - 0.1)) for u, z in prof]
    vb = [bm.verts.new((u / 10, h / 20, z / 10 - 0.1)) for u, z in prof]
    bm.faces.new(va[::-1]); bm.faces.new(vb)
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((va[i], va[j], vb[j], vb[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.05, segments=3, affect="EDGES", clamp_overlap=True)
    cap = mesh("Rocker", bm, M["rocker_" + color], wn=True)
    # Pivote 4 mm debajo de la cara.
    piv = Vector((0, 0, -0.4)); ang = math.radians(10.0) * (1 if state else -1)
    R = Matrix.Translation(piv) @ Matrix.Rotation(ang, 4, "Y") @ Matrix.Translation(-piv)
    cap.data.transform(R)
    if orient == "v": cap.data.transform(Matrix.Rotation(math.radians(90), 4, "Z"))   # derecha → arriba
    return cap


def lamp(kind, on):
    """Lámpara joya: bisel + lente facetada; encendida, lente y foco emiten."""
    lp = next(l for l in L["lamps"] if l["id"] == kind)
    rb, rl = lp["bezel_d"] / 2, lp["lens_d"] / 2
    amber = lp["color"] == "amber"
    if amber:      # bisel negro moleteado (OVERLOAD)
        knurl = lambda t, r, z: r - (0.18 if (z < 3.6 and math.cos(t * 60) > 0) else 0.0)
        bz = mesh("Bezel", lathe([(rb, 0.0), (rb, 3.6), (rb - 0.6, 4.2), (rl + 0.9, 4.4), (rl + 0.5, 3.0), (rl + 0.4, 3.0)], 180, knurl), M["black"])
    else:          # bisel cromado (POWER)
        bz = mesh("Bezel", lathe([(rb, 0.0), (rb, 1.2), (rb - 0.8, 2.4), (rl + 0.7, 2.8), (rl + 0.4, 2.0), (rl + 0.3, 2.0)], 128), M["chrome"])
    top = 4.2 if amber else 2.6
    col = (1.0, 0.42, 0.05) if amber else (0.95, 0.03, 0.02)
    lens_m = glass("Lens", tuple(c * (0.9 if on else 0.55) for c in col), (2.2 if amber else 1.6) if on else 0.0, col)
    lens = mesh("Lens", lathe([(rl, top - 1.5), (rl, top + 0.6), (rl * 0.82, top + 1.9), (rl * 0.45, top + 2.7), (0.0, top + 2.9)], 12), lens_m, smooth=False)
    lens.parent = bz
    bulb = mesh("Bulb", lathe([(rl * 0.55, top - 3.0), (rl * 0.55, top - 1.2), (0.0, top - 0.6)], 24),
                emissive("Filament", col, 6.0 if amber else 4.0) if on else principled("BulbOff", (0.5, 0.45, 0.35), 0.3))
    bulb.parent = bz
    cav = mesh("LampCav", lathe([(rl + 0.4, top - 3.4), (rl + 0.4, top - 1.4), (0.0, top - 1.4)], 24), M["dark"]); cav.parent = bz
    cav.location.z = 0.0
    return bz


def wheel_obj(kind, value):
    """Rueda (disco r=38, ancho 12, eje en x) con estrías; la de PITCH lleva la muesca del centro."""
    wl = next(w for w in LHC["wheels"] if w["id"] == kind)
    R, Wd = wl["radius"], wl["wheel_w"]
    depth = math.sqrt(R * R - (wl["slot_len"] / 2) ** 2)      # centro bajo la placa
    bm = bmesh.new(); segs = 360; rings = []
    pitch = kind == "pitch_wheel"
    for s_ in range(4):
        x = (-Wd / 2, -Wd / 2 + 0.8, Wd / 2 - 0.8, Wd / 2)[s_]
        ring = []
        for i in range(segs):
            t = 2 * math.pi * i / segs
            rr = R - (0.0 if 0 < s_ < 3 else 0.7)
            if 0 < s_ < 3:
                rr -= 0.75 * (0.5 + 0.5 * math.cos(t * 120))       # estrías cada 3°
                if pitch and abs(((t + math.pi) % (2 * math.pi)) - math.pi) < math.radians(3.2): rr -= 2.0
            ring.append(bm.verts.new((x / 10, rr * math.sin(t) / 10, rr * math.cos(t) / 10)))
        rings.append(ring)
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(segs):
            j = (i + 1) % segs; bm.faces.new((a[i], a[j], b[j], b[i]))
    for ring, rev in ((rings[0], True), (rings[-1], False)):
        c = bm.verts.new((ring[0].co.x, 0, 0))
        for i in range(segs):
            j = (i + 1) % segs; bm.faces.new((ring[j], ring[i], c) if rev else (ring[i], ring[j], c))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = mesh("Wheel_" + kind, bm, M["wheel"])
    if pitch: ang = -value * wl["travel_deg"] / 2.0
    else: ang = -(value - 0.5) * wl["travel_deg"]
    o.rotation_euler = Euler((math.radians(ang), 0, 0), "XYZ")
    o.location = Vector((0, 0, -depth / 10))
    holder = bpy.data.objects.new("WheelHolder_" + kind, None); link(holder); o.parent = holder
    return holder


# ───────── estático: gabinete, panel, teclado ─────────
def black_rocker_well(fr, name, x, y, w, h, orient):
    """Abertura oscura de la tecla basculante (la tecla es un sprite)."""
    ww, hh = w, h                                     # w, h del layout ya son extensión en x, y
    box(fr, name, x - ww / 2 - 0.7, y - hh / 2 - 0.7, x + ww / 2 + 0.7, y + hh / 2 + 0.7, -3.0, 0.02, M["hole"], 0.3)


def jack(fr, name, x, y, nut_d, hole_d, c0=0.0):
    r = nut_d / 2
    nut = mesh(name, lathe([(r, 0.0), (r, 1.6), (r - 0.6, 2.2), (hole_d / 2 + 1.2, 2.4), (hole_d / 2 + 0.6, 2.0), (hole_d / 2, 2.0), (hole_d / 2, -3.0), (0, -3.0)], 96,
                           lambda t, rr, z: rr - (0.25 if (z < 1.7 and rr > r - 0.1 and math.cos(t * 50) > 0) else 0.0)), M["nickel"])
    place(fr, nut, x, y, c0)
    hole = mesh(name + "Hole", lathe([(hole_d / 2 - 0.05, -2.9), (hole_d / 2 - 0.05, 1.9), (0.0, 1.9)], 48), M["hole"])
    place(fr, hole, x, y, c0 - 0.4)


def screw(fr, name, x, y, d, c0=0.0, m=None):
    r = d / 2
    s = mesh(name, lathe([(r, 0.0), (r, 0.5), (r * 0.8, 1.1), (r * 0.4, 1.35), (0.0, 1.4)], 32), m or M["screw"])
    place(fr, s, x, y, c0)
    for a in (0, 90):          # ranura Phillips
        sl = box(LOCAL, name + f"s{a}", -r * 0.75, -0.25, r * 0.75, 0.25, 0.9, 1.6, M["hole"], 0.0)
        sl.parent = s; sl.location = Vector((0, 0, 0)); sl.rotation_euler = Euler((0, 0, math.radians(a)), "XYZ")


def build_panel(fr):
    """Cara impresa, listón superior de madera, placas laterales de aluminio, aberturas de teclas y jack."""
    face = box(fr, "Face", 0.0, RAIL["y0"], PW_, HPY, -1.6, 0.0, M["panel"], 0.15, 2)
    # La textura usa coordenadas de objeto con origen en la esquina impresa: recentrar la malla.
    off = fr.v(0.0, 0.0, 0.0)
    face.data.transform(Matrix.Translation(-off)); face.location = off
    # Listón superior con la ranura donde entra la cara.
    rail_back = -(RAIL["depth"] - RAIL["slot"]["w"]) / 2 - 1.6
    rail_front = rail_back + RAIL["depth"]
    box(fr, "Rail", RAIL["x0"], RAIL["y0"], RAIL["x1"], RAIL["y1"], rail_back, rail_front, M["wood_x"], 1.6, 4)
    for i, s in enumerate(RAIL["screws"]):
        screw(fr, f"RailScrew{i}", s["x"], s["y"] - 6.0, s["d"] + 2.0, rail_front - 0.3)
    # Placas laterales: trapecio desde la cara hasta el piso del gabinete.
    for side, (a0, a1) in enumerate(((-SIDE_W, 0.0), (PW_, PW_ + SIDE_W))):
        # (b, c) en el marco del panel: b = py, c = h
        ub = lambda y, z: (math.cos(TILT) * (y - HY) - math.sin(TILT) * (z - HZ) + HPY,
                           math.sin(TILT) * (y - HY) + math.cos(TILT) * (z - HZ))
        poly = [(RAIL["y1"] - 1.0, 0.6), (HPY, 0.6), ub(100.0, HZ), (RAIL["y1"] - 1.0, -42.0)]
        prism(fr, f"SidePlate{side}", poly, a0, a1, M["alu"], 0.4, 2)
    # Fondo de la caja del panel (oscuro) para las tomas de costado.
    box(fr, "PanelBack", 0.0, RAIL["y1"], PW_, HPY, -42.0, -40.0, M["dark"], 0.2)
    for s in L["switches"]:
        black_rocker_well(fr, "Well_" + s["id"], s["x"], s["y"], s["w"], s["h"], s["orient"])
    for j in L["jacks"]:
        jack(fr, "Jack_" + j["id"], j["x"], j["y"], j["nut_d"], j["hole_d"])


def build_case():
    fr = DECK
    pr = CASE["cheeks"]["profile_yz"]; t = CASE["cheeks"]["t"]
    prism(fr, "CheekL", [(y, z) for y, z in pr], 0.0, t, M["wood_y"], 2.5, 3)
    prism(fr, "CheekR", [(y, z) for y, z in pr], CW - t, CW, M["wood_y"], 2.5, 3)
    # Frente: tabla bajo las teclas (z 0..58) y fondo del teclado.
    box(fr, "FrontLip", LIP["x0"], LIP["y0"], LIP["x1"], LIP["y1"], 0.0, LIP["z1"], M["wood_x"], 2.0, 3)
    kbd = CASE["key_bed"]
    box(fr, "KeyBed", kbd["x0"], kbd["y0"] - 30, kbd["x1"], kbd["y1"], 30.0, kbd["z"], M["felt"], 0.0)
    box(fr, "Bottom", t, 10.0, CW - t, CD - 2.0, 0.0, 30.0, M["dark"], 0.0)
    eb = CASE["electronics_box"]
    box(fr, "ElecBox", t, eb["y0"], CW - t, eb["y1"], 0.0, eb["z1"] - 1.0, M["dark"], 0.5)
    box(fr, "Back", t - 2, 0.0, CW - t + 2, eb["y0"], 0.0, 120.0, M["wood_x"], 1.5)
    # Listón con placa propia: plano inclinado de z_back (bisagra) a NB_Z_FRONT, canto frontal redondeado.
    poly = [(NB["y0"], NB["z_back"]), (NB["y1"] - 3.0, NB_Z_FRONT), (NB["y1"], NB_Z_FRONT - 3.0),
            (NB["y1"], NB_FACE_BOTTOM), (NB["y1"] - 9.0, NB_FACE_BOTTOM), (NB["y0"], NB["z_back"] - 16.0)]
    prism(fr, "NameBoard", poly, NB["x0"], NB["x1"], M["wood_x"], 1.2, 3)
    p = NB["nameplate"]
    zf = lambda y: NB["z_back"] + (NB_Z_FRONT - NB["z_back"]) * (y - NB["y0"]) / (NB["y1"] - 3.0 - NB["y0"])
    slope = math.atan2(NB["z_back"] - NB_Z_FRONT, NB["y1"] - 3.0 - NB["y0"])
    # Placa: caja delgada sobre el plano inclinado, con origen en su esquina trasera izquierda (para la textura).
    plen = p["y1"] - p["y0"]               # medida sobre la pendiente (la textura usa la misma)
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector(((v.co.x + 0.5) * (p["x1"] - p["x0"]) / 10, -(v.co.y + 0.5) * plen / 10, (v.co.z - 0.5) * 0.08))
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.03, segments=2, affect="EDGES", clamp_overlap=True)
    pl = mesh("Nameplate", bm, M["plate"])
    pl.location = D(p["x0"], p["y0"], zf(p["y0"]) + 0.8); pl.rotation_euler = Euler((slope, 0, 0), "XYZ")
    # Controlador izquierdo: caja, placa con serigrafía y agujeros de las ruedas.
    x0, x1, y0, y1 = LHC["x0"], LHC["x1"], LHC["y0"], LHC["y1"]
    for i, (a0, b0, a1, b1) in enumerate(((x0, y0, x0 + 1.5, y1), (x1 - 1.5, y0, x1, y1), (x0, y1 - 1.5, x1, y1))):
        box(fr, f"LhcWall{i}", a0, b0, a1, b1, kbd["z"], LHC_Z - 1.5, M["dark"], 0.0)
    holes = [(w["x"] - w["slot_w"] / 2, w["y"] - w["slot_len"] / 2, w["x"] + w["slot_w"] / 2, w["y"] + w["slot_len"] / 2) for w in LHC["wheels"]]
    plate = plate_with_holes(fr, "LhcPlate", x0 + 0.3, y0 + 0.3, x1 - 0.3, y1 - 0.3, LHC_Z - 1.5, LHC_Z, holes, M["lhc"], 0.5)
    off = D(x0, y0, LHC_Z)
    plate.data.transform(Matrix.Translation(-off)); plate.location = off
    for c in [o for o in bpy.data.objects if o.name.startswith("LhcPlate_cut")]:
        c.data.transform(Matrix.Translation(-off)); c.location = off
    for w in LHC["wheels"]:
        slot_well(fr, "Slot_" + w["id"], w["x"], w["y"], w["slot_w"], w["slot_len"], LHC_Z)
    for s in LHC["switches"]:
        black_rocker_well(fr, "Well_" + s["id"], s["x"], s["y"], s["w"], s["h"], "h")
        bpy.data.objects["Well_" + s["id"]].location.z += LHC_Z / 10
    for j in LHC["jacks"]:
        jack(fr, "Jack_" + j["id"], j["x"], j["y"], j["nut_d"], j["nut_d"] * 0.52, LHC_Z)
    for i, s in enumerate(LHC["screws"]):
        screw(fr, f"LhcScrew{i}", s["x"], s["y"], s["d"] + 1.0, LHC_Z, M["screw"])


def white_outline(k, keys_by_note):
    """Contorno superior de una blanca (mm del deck): cabeza ancha adelante, cola angosta entre las negras."""
    x0, x1, y0, y1 = k["x0"], k["x1"], k["y0"], k["y1"]
    lb = keys_by_note.get(k["note"] - 1); rb = keys_by_note.get(k["note"] + 1)
    gap = 0.9
    xl = max(x0, lb["x1"] + gap) if lb and lb["black"] else x0
    xr = min(x1, rb["x0"] - gap) if rb and rb["black"] else x1
    ys = KB["y_black"] + 1.4
    pts = [(xl, y0), (xr, y0)]
    if xr < x1: pts += [(xr, ys), (x1, ys)]
    pts += [(x1, y1), (x0, y1)]
    if xl > x0: pts += [(x0, ys), (xl, ys)]
    return pts


def white_key(name, outline, m):
    bm = bmesh.new()
    top = [bm.verts.new(D(x, y, WHITE_TOP)) for x, y in outline]
    bot = [bm.verts.new(D(x, y, WHITE_BOTTOM)) for x, y in outline]
    ft = bm.faces.new(top); bm.faces.new(bot[::-1])
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((top[j], top[i], bot[i], bot[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    edges = [e for e in bm.edges if all(abs(v.co.z - WHITE_TOP / 10) < 1e-6 for v in e.verts)]
    bmesh.ops.bevel(bm, geom=edges, offset=0.08, segments=3, affect="EDGES", clamp_overlap=True)
    return mesh(name, bm, m, wn=True)


def black_key(name, k, m):
    """Negra real: base recta hasta el nivel de las blancas, laterales que se cierran al 74 % y frente inclinado."""
    x0, x1, y0, y1 = k["x0"], k["x1"], NB["y1"] + 1.5, k["y1"]
    cx = (x0 + x1) / 2; hw = (x1 - x0) / 2
    levels = [(62.0, 1.0, 0.0), (WHITE_TOP + 1.0, 1.0, 0.0), (BLACK_TOP, 0.74, 5.5)]
    bm = bmesh.new(); rings = []
    for z, sc, rec in levels:
        rings.append([bm.verts.new(D(cx - hw * sc, y0, z)), bm.verts.new(D(cx + hw * sc, y0, z)),
                      bm.verts.new(D(cx + hw * sc, y1 - rec, z)), bm.verts.new(D(cx - hw * sc, y1 - rec, z))])
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(4):
            j = (i + 1) % 4; bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(rings[0][::-1]); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=0.07, segments=3, affect="EDGES", clamp_overlap=True)
    return mesh(name, bm, m, wn=True)


def build_keys(keys_down=False):
    by_note = {k["note"]: k for k in KB["keys"]}
    out = []
    for k in KB["keys"]:
        if k["black"]: o = black_key("Key%d" % k["note"], k, M["black_key"])
        else: o = white_key("Key%d" % k["note"], white_outline(k, by_note), M["white_key"])
        if keys_down:
            front = k["y1"]
            piv = D(0, KEY_PIVOT_Y, (BLACK_TOP if k["black"] else WHITE_TOP))
            ang = math.atan2(KEY_DIP, KB["y_white"] - KEY_PIVOT_Y)
            o.data.transform(Matrix.Translation(piv) @ Matrix.Rotation(ang, 4, "X") @ Matrix.Translation(-piv))
        out.append(o)
    return out


def build_static(keys_down=False):
    sc = reset(); materials(); lights()
    fr = panel_frame()
    build_panel(fr); build_case(); build_keys(keys_down)
    return fr


# ───────── cámara ortográfica del editor ─────────
X0_MM, X1_MM = -12.0, CW + 12.0
def frame_bounds():
    """Rectángulo de pantalla (mm): del tope del listón superior al pie del frente del gabinete."""
    tops = []
    for py, h in ((RAIL["y0"], -(RAIL["depth"] - RAIL["slot"]["w"]) / 2 - 1.6), (RAIL["y0"], 8.0)):
        x, y, z = panel_to_case(0, py, h); tops.append(screen_mm(x, y, z)[1])
    s0 = min(tops) - 7.0
    s1 = screen_mm(0, CD, 0.0)[1] + 6.0
    return X0_MM, s0, X1_MM, s1


def ortho_camera(x0, s0, x1, s1):
    sc = bpy.context.scene
    rx = int(round((x1 - x0) * PXMM)); ry = int(round((s1 - s0) * PXMM))
    w_mm = rx / PXMM; h_mm = ry / PXMM
    cam = bpy.data.cameras.new("Cam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL"; cam.ortho_scale = w_mm / 10.0
    cam.clip_start = 1.0; cam.clip_end = 2000
    o = bpy.data.objects.new("Cam", cam); link(o)
    cx, cy = x0 + w_mm / 2, s0 + h_mm / 2
    p0 = Vector((cx / 10.0, -(cy / COS_P) / 10.0, 0.0))            # punto del plano z=0 con esa pantalla
    view = Vector((0, SIN_P, -COS_P))
    o.location = p0 - view * 400.0
    o.rotation_euler = Euler((PHI, 0, 0), "XYZ"); sc.camera = o
    sc.render.resolution_x = rx; sc.render.resolution_y = ry; sc.render.resolution_percentage = 100
    return rx, ry, x0, s0


def check_projection(rx, ry, x0, s0):
    """Compara la fórmula analítica con la proyección real de la cámara (px)."""
    from bpy_extras.object_utils import world_to_camera_view
    sc = bpy.context.scene; err = 0.0
    bpy.context.view_layer.update()
    pts = [("deck", (100, 300, 70)), ("deck", (700, 430, 58)), ("panel", (44.3, 66.5, 0)), ("panel", (661.5, 133.2, 10))]
    for fr, (a, b, c) in pts:
        x, y, z = (a, b, c) if fr == "deck" else panel_to_case(a, b, c)
        co = world_to_camera_view(sc, sc.camera, D(x, y, z))
        px, py = co.x * rx, (1 - co.y) * ry
        sx, sy = screen_mm(x, y, z)
        ex, ey = (sx - x0) * PXMM, (sy - s0) * PXMM
        err = max(err, abs(px - ex), abs(py - ey))
    print(f"proyección: error máx {err:.3f} px", flush=True)
    return err


def projection_meta(rx, ry, x0, s0):
    k = PXMM
    cT, sT = math.cos(TILT - PHI), math.sin(TILT - PHI)
    # Panel: sy = (HY + (py−HPY)cosT + h sinT)cosP − (HZ − (py−HPY) sinT + h cosT) sinP
    c_panel = HY * COS_P - HZ * SIN_P - HPY * cT
    return dict(
        pxPerMm=k, width=rx, height=ry, x0Mm=x0, s0Mm=s0,
        tiltDeg=math.degrees(TILT), viewFromVerticalDeg=math.degrees(PHI), panelXOffsetMm=PX_OFF,
        panel=dict(affine=[[k, 0.0, k * (PX_OFF - x0)], [0.0, k * cT, k * (c_panel - s0)]], perMmHeight=[0.0, k * sT],
                   note="px = a00*x + a02 ; py = a11*y + a12 + perMmHeight[1]*h   (x, y: layoutmd.json panel mm; h: mm above the face)"),
        deck=dict(affine=[[k, 0.0, -k * x0], [0.0, k * COS_P, -k * s0]], perMmHeight=[0.0, -k * SIN_P],
                  note="px = a00*x + a02 ; py = a11*y + a12 + perMmHeight[1]*z   (x, y: deck/case mm; z: mm above the case bottom)"),
        heights=dict(whiteKeyTop=WHITE_TOP, blackKeyTop=BLACK_TOP, lhcPlate=LHC_Z, keyDipMm=KEY_DIP, keyPivotY=KEY_PIVOT_Y,
                     whiteKeyBottom=WHITE_BOTTOM, lipTop=LIP["z1"], nameBoardFront=[NB["y1"], NB_Z_FRONT, NB_FACE_BOTTOM]),
    )


def keys_crop(rx, ry, x0, s0):
    """Rectángulo (px) del teclado para el render de teclas apretadas."""
    sx0 = KB["x0"] - 3.0; sx1 = KB["x1"] + 3.0
    sy0 = screen_mm(0, NB["y1"], NB_FACE_BOTTOM)[1] - 2.0
    sy1 = screen_mm(0, LIP["y0"] + 1.0, LIP["z1"])[1] + 1.0
    c = [int(math.floor((sx0 - x0) * PXMM)), int(math.floor((sy0 - s0) * PXMM)),
         int(math.ceil((sx1 - x0) * PXMM)), int(math.ceil((sy1 - s0) * PXMM))]
    return [max(0, c[0]), max(0, c[1]), min(rx, c[2]), min(ry, c[3])]


def mode_bg():
    t0 = time.time()
    build_static(False)
    rx, ry, x0, s0 = ortho_camera(*frame_bounds())
    err = check_projection(rx, ry, x0, s0)
    render(os.path.join(OUT, "bg.png"))
    t1 = time.time()
    meta = projection_meta(rx, ry, x0, s0); meta["projectionCheckPx"] = round(err, 4)
    crop = keys_crop(rx, ry, x0, s0)
    build_static(True)
    ortho_camera(*frame_bounds())
    sc = bpy.context.scene
    sc.render.use_border = True; sc.render.use_crop_to_border = True
    sc.render.border_min_x = crop[0] / rx; sc.render.border_max_x = crop[2] / rx
    sc.render.border_min_y = 1 - crop[3] / ry; sc.render.border_max_y = 1 - crop[1] / ry
    render(os.path.join(OUT, "keysdown.png"))
    t2 = time.time()
    by_note = {k["note"]: k for k in KB["keys"]}
    keys = []
    for k in KB["keys"]:
        d = dict(note=k["note"], black=k["black"], x0=k["x0"], x1=k["x1"], y1=k["y1"])
        if not k["black"]: d["outline"] = white_outline(k, by_note)
        keys.append(d)
    meta.update(keysCrop=crop, keys=keys, samples=SAMPLES,
                seconds=dict(bg=round(t1 - t0, 1), keysdown=round(t2 - t1, 1)))
    json.dump(meta, open(os.path.join(OUT, "bg.json"), "w"), indent=1)
    print(f"bg {t1 - t0:.0f} s, keysdown {t2 - t1:.0f} s", flush=True)


def mode_preview():
    global SAMPLES
    SAMPLES = int(arg("--samples", "48"))
    build_static(False)
    rx, ry, x0, s0 = ortho_camera(*frame_bounds())
    check_projection(rx, ry, x0, s0)
    bpy.context.scene.render.resolution_percentage = int(arg("--pct", "50"))
    if "--nopop" not in argv: populate()
    render(os.path.join(OUT, "preview_bg.png"))


# ───────── sprites ─────────
def sprite_scene(plane):
    """Escena vacía con las mismas luces; marco del panel (girado) o del deck en el origen; atrapa-sombras."""
    reset(); materials(); lights()
    fr = panel_frame(at_origin=True) if plane == "panel" else Frame(0.0, None)
    # Atrapa-sombras de 2 m: tapa, como el panel real, la luz que viene de detrás del plano (con uno chico
    # la luz de contorno iluminaba las perillas del sprite y no las de la escena completa).
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=100.0)
    catcher = fr.own(mesh("Catcher", bm, M["paint"], smooth=False)); catcher.is_shadow_catcher = True
    return fr, catcher


def sprite_camera(w_px, h_px, ax, ay):
    """Cámara del editor (misma dirección) con el origen del mundo en el píxel (ax, ay) de un cuadro w×h."""
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("SpCam"); cam.type = "ORTHO"; cam.sensor_fit = "HORIZONTAL" if w_px >= h_px else "VERTICAL"
    cam.ortho_scale = max(w_px, h_px) / PXMM / 10.0; cam.clip_start = 1.0; cam.clip_end = 2000
    o = bpy.data.objects.new("SpCam", cam); link(o)
    cx = (w_px / 2 - ax) / PXMM; cy = (h_px / 2 - ay) / PXMM          # centro de pantalla (mm) respecto del origen
    p0 = Vector((cx / 10.0, -(cy / COS_P) / 10.0, 0.0))
    o.location = p0 - Vector((0, SIN_P, -COS_P)) * 400.0
    o.rotation_euler = Euler((PHI, 0, 0), "XYZ"); sc.camera = o
    sc.render.resolution_x = w_px; sc.render.resolution_y = h_px; sc.render.resolution_percentage = 100
    sc.render.use_persistent_data = True


def frames_dir(name):
    d = os.path.join(OUT, name); os.makedirs(d, exist_ok=True); return d


SPRITE_T = {}


def sprite_knob(name, builder, w, h, ax, ay, frames=128):
    t0 = time.time()
    fr, _ = sprite_scene("panel")
    k = builder(); fr.own(k)
    sprite_camera(w, h, ax, ay)
    d = frames_dir(name)
    for f in range(frames):
        k.rotation_euler = Euler((0, 0, -math.radians(360.0 * f / frames)), "XYZ")
        render(os.path.join(d, f"{f:03d}.png"))
    SPRITE_T[name] = round(time.time() - t0, 1)
    return dict(w=w, h=h, frames=frames, anchor=[ax, ay], plane="panel", degPerFrame=360.0 / frames,
                note="frame 0 = pointer at 12 o'clock on the panel, clockwise")


def sprite_single(name, plane, build, w, h, ax, ay):
    t0 = time.time()
    fr, _ = sprite_scene(plane)
    o = build(); fr.own(o)
    if plane == "deck": o.location = Vector((0, 0, 0))
    sprite_camera(w, h, ax, ay)
    render(os.path.join(OUT, name + ".png"))
    SPRITE_T[name] = round(time.time() - t0, 1)
    return dict(w=w, h=h, frames=1, anchor=[ax, ay], plane=plane)


def sprite_wheel(kind, frames, w, h, ax, ay):
    t0 = time.time()
    reset(); materials(); lights()
    fr = Frame(0.0, None)
    wl = next(x for x in LHC["wheels"] if x["id"] == kind)
    sw, sl = wl["slot_w"], wl["slot_len"]
    # Placa con el agujero (atrapa-sombras) y el pozo oscuro debajo: el sprite tapa entero el hueco del fondo.
    pad = 1000.0
    cat = plate_with_holes(fr, "Catcher", -pad, -pad, pad, pad, -1.5, 0.0, [(-sw / 2, -sl / 2, sw / 2, sl / 2)], M["paint"], 0.0)
    cat.is_shadow_catcher = True
    slot_well(fr, "Slot", 0.0, 0.0, sw, sl, 0.0)
    holder = wheel_obj(kind, 0.0)
    whl = holder.children[0]
    sprite_camera(w, h, ax, ay)
    d = frames_dir(kind)
    tr = wl["travel_deg"]
    for f in range(frames):
        t = f / (frames - 1)
        v = -1.0 + 2.0 * t if kind == "pitch_wheel" else t
        ang = -v * tr / 2.0 if kind == "pitch_wheel" else -(v - 0.5) * tr
        whl.rotation_euler = Euler((math.radians(ang), 0, 0), "XYZ")
        render(os.path.join(d, f"{f:03d}.png"))
    SPRITE_T[kind] = round(time.time() - t0, 1)
    return dict(w=w, h=h, frames=frames, anchor=[ax, ay], plane="deck", planeZ=LHC_Z,
                values=[-1.0, 1.0] if kind == "pitch_wheel" else [0.0, 1.0],
                note="frame f ↔ value = v0 + (v1 − v0)·f/(frames−1); anchor = wheel centre on the plate (x, y, z=lhcPlate)")


def mode_sprites():
    only = arg("--only")
    want = lambda n: only is None or any(n.startswith(o) for o in only.split(","))
    meta_path = os.path.join(OUT, "sprites.json")
    meta = json.load(open(meta_path)) if os.path.exists(meta_path) else {}
    k = PXMM
    # Perillas: cuadro amplio (el empaquetador recorta al contenido); ancla un poco arriba-izquierda (la sombra cae abajo-derecha).
    if want("knob_std"): meta["knob_std"] = sprite_knob("knob_std", lambda: knob_standard(False), int(48 * k), int(48 * k), int(20 * k), int(19 * k))
    if want("knob_big"): meta["knob_big"] = sprite_knob("knob_big", lambda: knob_standard(True), int(54 * k), int(54 * k), int(23 * k), int(22 * k))
    if want("knob_ptr"): meta["knob_ptr"] = sprite_knob("knob_ptr", knob_pointer, int(50 * k), int(50 * k), int(23 * k), int(22 * k))
    # Teclas basculantes del panel: (color, orientación) en los dos estados.
    combos = sorted({(s["color"], s["orient"]) for s in L["switches"]})
    for col, ori in combos:
        sw = next(s for s in L["switches"] if s["color"] == col and s["orient"] == ori)
        for st in (0, 1):
            n = f"rocker_{col}_{ori}_{st}"
            if not want(n): continue
            W_, H_ = (int(40 * k), int(26 * k)) if ori == "h" else (int(26 * k), int(40 * k))
            meta[n] = sprite_single(n, "panel", lambda: rocker_cap(col, max(sw["w"], sw["h"]), min(sw["w"], sw["h"]), ori, st),
                                    W_, H_, W_ // 2 - int(2 * k), H_ // 2 - int(2 * k))
    for s in LHC["switches"][:1]:
        for st in (0, 1):
            n = f"rocker_cream_h_{st}"
            if not want(n): continue
            W_, H_ = int(42 * k), int(28 * k)
            meta[n] = sprite_single(n, "deck", lambda: rocker_cap("cream", s["w"], s["h"], "h", st), W_, H_, W_ // 2 - int(2 * k), H_ // 2)
            meta[n]["planeZ"] = LHC_Z
    for lp in L["lamps"]:
        for on in (0, 1):
            n = f"lamp_{lp['id']}_{on}"
            if not want(n): continue
            sz = int((lp["bezel_d"] + 16) * k)
            meta[n] = sprite_single(n, "panel", lambda: lamp(lp["id"], on), sz, sz, sz // 2 - int(1.5 * k), sz // 2 - int(1.5 * k))
    if want("pitch_wheel"): meta["pitch_wheel"] = sprite_wheel("pitch_wheel", 33, int(34 * k), int(86 * k), int(15 * k), int(46 * k))
    if want("mod_wheel"): meta["mod_wheel"] = sprite_wheel("mod_wheel", 33, int(34 * k), int(86 * k), int(15 * k), int(46 * k))
    for n, t in SPRITE_T.items():
        if n in meta: meta[n]["seconds"] = t
    json.dump(meta, open(meta_path, "w"), indent=1)
    print("sprites", SPRITE_T, flush=True)


# ───────── vistas en perspectiva (web y video) ─────────
def knob_angle(kn, v):
    if kn["kind"] == "rotary-switch": return kn["detents"][int(v)]
    nums = []
    for s, a in kn["legends"]:
        try: nums.append((float(s), a))
        except ValueError: pass
    mono = all(b[0] > a[0] and b[1] > a[1] for a, b in zip(nums[:-1], nums[1:]))
    if mono and len(nums) >= 2 and nums[0][0] <= v <= nums[-1][0] and not (nums[0][0] == 0 and nums[-1][0] == 10):
        (v0, a0), (v1, a1) = nums[0], nums[-1]
        return a0 + (v - v0) * (a1 - a0) / (v1 - v0)
    return kn["a0"] + (kn["a1"] - kn["a0"]) * v / 10.0


def populate(values=None):
    """Controles en 3D en sus posiciones del layout (valores por defecto o los de un preset)."""
    values = values or {}
    fr = next(Frame(HPY, o) for o in bpy.data.objects if o.name.startswith("PanelRoot"))
    for kn in L["knobs"]:
        v = values.get(kn["id"], kn["default"])
        o = knob_pointer() if kn["kind"] == "rotary-switch" else knob_standard(kn["kind"] == "big")
        place(fr, o, kn["x"], kn["y"], 0.0, -math.radians(knob_angle(kn, v)))
    for s in L["switches"]:
        st = int(values.get(s["id"], s["default"]))
        o = rocker_cap(s["color"], max(s["w"], s["h"]), min(s["w"], s["h"]), s["orient"], st)
        place(fr, o, s["x"], s["y"], 0.0)
    for lp in L["lamps"]:
        on = 1 if lp["id"] == "power_led" else int(values.get(lp["id"], 0))
        place(fr, lamp(lp["id"], on), lp["x"], lp["y"], 0.0)
    for s in LHC["switches"]:
        st = int(values.get(s["id"], s["default"]))
        o = rocker_cap("cream", s["w"], s["h"], "h", st); place(DECK, o, s["x"], s["y"], LHC_Z)
    for w in LHC["wheels"]:
        h = wheel_obj(w["id"], values.get(w["id"], w["default"])); h.location = D(w["x"], w["y"], LHC_Z)


def stage(world_strength=0.35):
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=600.0)
    floor = mesh("Floor", bm, M["floor"], smooth=False); floor.location = Vector((CW / 20, -CD / 20, -0.02))
    def area(name, loc, target, energy, size, col=(1, 1, 1)):
        a = bpy.data.lights.new(name, "AREA"); a.energy = energy; a.size = size; a.color = col; a.shape = "DISK"
        o = bpy.data.objects.new(name, a); link(o); o.location = Vector(loc)
        o.rotation_euler = (Vector(target) - o.location).to_track_quat("-Z", "Y").to_euler()
    ctr = (CW / 20, -CD / 20, 8.0)
    area("Soft", (-30, -150, 160), ctr, float(arg("--soft", "3.2e5")), 120.0, (1.0, 0.96, 0.92))
    area("Rim", (140, 90, 90), ctr, 2.0e5, 80.0, (0.9, 0.94, 1.0))
    area("Kick", (150, -120, 30), ctr, 8.0e4, 60.0, (1.0, 0.92, 0.85))
    for o in [o for o in bpy.data.objects if o.type == "LIGHT" and o.data.type == "SUN"]:
        o.data.energy *= 0.35
    m = [n for n in bpy.context.scene.world.node_tree.nodes if n.type == "MATH" and n.operation == "MULTIPLY"][0]
    m.inputs[1].default_value = world_strength
    bpy.context.scene.render.film_transparent = False


def persp_camera(loc, target, lens, fstop, w, h):
    sc = bpy.context.scene
    cam = bpy.data.cameras.new("PCam"); cam.lens = lens; cam.clip_start = 1.0; cam.clip_end = 5000
    cam.dof.use_dof = fstop > 0; cam.dof.aperture_fstop = max(fstop, 0.01)
    o = bpy.data.objects.new("PCam", cam); link(o); o.location = Vector(loc)
    tgt = bpy.data.objects.new("Target", None); link(tgt); tgt.location = Vector(target)
    c = o.constraints.new("TRACK_TO"); c.target = tgt; c.track_axis = "TRACK_NEGATIVE_Z"; c.up_axis = "UP_Y"
    cam.dof.focus_object = tgt
    sc.camera = o; sc.render.resolution_x = w; sc.render.resolution_y = h; sc.render.resolution_percentage = 100
    return o, tgt


# Tomas en perspectiva: lentes largas desde lejos. Con 35-50 mm de cerca, las teclas de un extremo y del otro
# fugan en direcciones muy distintas y se ven torcidas (lección del AT101V). Coordenadas en unidades de Blender.
HERO = {
    "front": ((-61.0, -376.0, 234.0), (38.0, -22.0, 11.0), 135.0, 0.25),
    "panel": ((165.0, -235.0, 150.0), (47.0, -12.0, 18.0), 200.0, 0.12),
    "keys":  ((232.0, -165.0, 38.0), (37.0, -35.0, 7.0), 150.0, 0.12),
}
FILM = {
    "wide":  ([(-70.0, -320.0, 200.0), (120.0, -320.0, 190.0)], [(33.0, -24.0, 9.0), (41.0, -24.0, 9.0)], 135.0, 0.25),
    "panel": ([(-40.0, -215.0, 150.0), (60.0, -215.0, 150.0)], [(14.0, -12.0, 19.0), (46.0, -12.0, 19.0)], 200.0, 0.10),
    "keys":  ([(225.0, -150.0, 36.0), (225.0, -120.0, 30.0)], [(42.0, -37.0, 6.5), (18.0, -36.0, 6.5)], 150.0, 0.12),
}


def hero_scene(preset_path=None):
    vals = json.load(open(preset_path)).get("values", {}) if preset_path else {}
    build_static(False); populate(vals); stage()


def mode_hero():
    only = arg("--shots", ",".join(HERO)).split(",")
    w, h = int(arg("--w", "2400")), int(arg("--h", "1100"))
    hero_scene(arg("--preset"))
    times = {}
    for name in only:
        loc, tgt, lens, fs = HERO[name]
        t0 = time.time()
        for o in [o for o in bpy.data.objects if o.name.startswith(("PCam", "Target"))]: bpy.data.objects.remove(o)
        persp_camera(loc, tgt, lens, fs, w, h)
        bpy.context.scene.render.image_settings.color_mode = "RGB"
        render(os.path.join(OUT, f"hero_{name}.png"))
        times[name] = round(time.time() - t0, 1)
    print("hero", times, flush=True)
    json.dump(times, open(os.path.join(OUT, "hero_times.json"), "w"))


def mode_film():
    """--shot wide|panel|keys: --frames cuadros a --w×--h; --probe N renderiza sólo el cuadro N."""
    shot = arg("--shot", "wide")
    cams, tgts, lens, fs = FILM[shot]
    hero_scene(arg("--preset"))
    sc = bpy.context.scene
    frames = int(arg("--frames", "120"))
    o, tgt = persp_camera(cams[0], tgts[0], lens, fs, int(arg("--w", "1920")), int(arg("--h", "1080")))
    sc.frame_start, sc.frame_end = 1, frames
    for f, c, t in ((1, cams[0], tgts[0]), (frames, cams[1], tgts[1])):
        o.location = Vector(c); o.keyframe_insert("location", frame=f)
        tgt.location = Vector(t); tgt.keyframe_insert("location", frame=f)
    for fc in (o.animation_data.action.fcurves if hasattr(o.animation_data.action, "fcurves") else []):
        for kp in fc.keyframe_points: kp.interpolation = "SINE"
    if arg("--probe"): sc.frame_start = sc.frame_end = int(arg("--probe"))
    os.makedirs(os.path.join(OUT, "film_" + shot), exist_ok=True)
    sc.render.filepath = os.path.join(OUT, "film_" + shot, "f_")
    sc.render.image_settings.color_mode = "RGB"
    sc.render.use_persistent_data = True
    bpy.ops.render.render(animation=True)


# ───────── film de la web (filmkit, --mode film2) ─────────
# Seis tomas sobre la demo "Fat Bass" (100 BPM, un tiempo = 0,6 s; cortes secos en tiempos del compás). El inicio
# de cada toma en el film es el instante del audio (la envolvente y las notas del MIDI se leen desde ahí).
FILM_SHOTS = [("open", 0.0, 4.2), ("keys", 4.2, 3.6), ("filter", 7.8, 3.6), ("wheels", 11.4, 3.0),
              ("sweep", 14.4, 3.0), ("end", 17.4, 4.2)]
NOTE_UP = 24          # la línea suena dos octavas abajo (pies 16'/32'): se toca en el centro del teclado


def midi_notes(path):
    """Notas (t_on, t_off, nota) de un MIDI de un canal (lector mínimo: tempo único, formato 0 o 1)."""
    import struct
    b = open(path, "rb").read()
    ntr, div = struct.unpack(">H", b[10:12])[0], struct.unpack(">H", b[12:14])[0]
    def vlq(i):
        v = 0
        while True:
            c = b[i]; i += 1; v = (v << 7) | (c & 0x7F)
            if c < 0x80: return v, i
    i, ev, tempo = 14, [], 500000
    for _ in range(ntr):
        ln = struct.unpack(">I", b[i + 4:i + 8])[0]; j = i + 8; end = j + ln; t = 0; rs = 0
        while j < end:
            d, j = vlq(j); t += d; s = b[j]
            if s == 0xFF:
                ty = b[j + 1]; l, k = vlq(j + 2)
                if ty == 0x51: tempo = int.from_bytes(b[k:k + l], "big")
                j = k + l
            elif s in (0xF0, 0xF7): l, k = vlq(j + 1); j = k + l
            else:
                if s & 0x80: rs = s; j += 1
                n = 1 if rs & 0xF0 in (0xC0, 0xD0) else 2
                ev.append((t, rs & 0xF0, b[j:j + n])); j += n
        i = end
    sec = lambda tk: tk / div * tempo / 1e6
    out, on = [], {}
    for tk, kind, d in sorted(ev, key=lambda e: e[0]):
        if kind == 0x90 and d[1] > 0: on[d[0]] = sec(tk)
        elif kind in (0x80, 0x90) and d[0] in on: out.append((on.pop(d[0]), sec(tk), d[0]))
    return sorted(out)


def smooth_seg(t, pts):
    """Valor por tramos [(t, v), ...] con aceleración suave entre puntos (gesto de una mano sobre un control)."""
    if t <= pts[0][0]: return pts[0][1]
    for (t0, v0), (t1, v1) in zip(pts[:-1], pts[1:]):
        if t <= t1:
            u = (t - t0) / max(1e-6, t1 - t0); u = u * u * (3 - 2 * u)
            return v0 + (v1 - v0) * u
    return pts[-1][1]


# Gestos del músico (tiempo del film en s). El corte sube en la toma "filter" y baja en "sweep"; la rueda de
# modulación sube en "wheels" y la de altura hace un bend hacia arriba y vuelve antes del cambio a Do (14,4 s).
CUTOFF = [(0.0, -1.5), (8.2, -1.5), (11.0, 2.2), (14.9, 2.2), (17.2, 0.4), (30.0, 0.4)]
EMPH = [(0.0, 2.5), (14.6, 2.5), (16.9, 4.6), (30.0, 4.6)]
CONTOUR = [(0.0, 6.5), (9.0, 6.5), (10.8, 5.2), (30.0, 5.2)]
MODW = [(0.0, 0.0), (11.7, 0.0), (13.1, 0.62), (16.5, 0.62), (18.6, 0.25), (30.0, 0.25)]
PITCHW = [(0.0, 0.0), (12.75, 0.0), (13.05, 0.55), (13.5, 0.55), (13.95, 0.0), (30.0, 0.0)]


def populate_film(values):
    """Como populate(), pero devuelve los objetos que el film anima: perillas por id, lámparas y ruedas."""
    fr = next(Frame(HPY, o) for o in bpy.data.objects if o.name.startswith("PanelRoot"))
    knobs, lamps, wheels = {}, {}, {}
    for kn in L["knobs"]:
        v = values.get(kn["id"], kn["default"])
        o = knob_pointer() if kn["kind"] == "rotary-switch" else knob_standard(kn["kind"] == "big")
        knobs[kn["id"]] = (place(fr, o, kn["x"], kn["y"], 0.0, -math.radians(knob_angle(kn, v))), kn)
    for s in L["switches"]:
        st = int(values.get(s["id"], s["default"]))
        place(fr, rocker_cap(s["color"], max(s["w"], s["h"]), min(s["w"], s["h"]), s["orient"], st), s["x"], s["y"], 0.0)
    for lp in L["lamps"]:
        on = 1 if lp["id"] in ("power_led", "overload") else 0        # la de OVERLOAD se anima (emisión con claves)
        lamps[lp["id"]] = place(fr, lamp(lp["id"], on), lp["x"], lp["y"], 0.0)
    for s in LHC["switches"]:
        place(DECK, rocker_cap("cream", s["w"], s["h"], "h", int(values.get(s["id"], s["default"]))), s["x"], s["y"], LHC_Z)
    for w in LHC["wheels"]:
        h = wheel_obj(w["id"], 0.0); h.location = D(w["x"], w["y"], LHC_Z); wheels[w["id"]] = (h.children[0], w)
    return knobs, lamps, wheels


def P(px, py, h=0.0):
    """Punto del panel (mm del layout, h sobre la cara) en coordenadas del mundo."""
    return D(*panel_to_case(px, py, h))


def key_x(note):
    k = next(k for k in KB["keys"] if k["note"] == note); return (k["x0"] + k["x1"]) / 20.0


def film_scene(values):
    """El instrumento con el estudio de las fotos, más oscuro, con piso infinito y la luz de barrido."""
    build_static(False)
    knobs, lamps, wheels = populate_film(values)
    stage(world_strength=0.22)
    fl = bpy.data.objects["Floor"]; fl.scale = (12.0, 12.0, 1.0)      # sin borde visible en las tomas bajas
    # Teclas con el origen en su pivote: bajan girando como las de verdad (9 mm adelante).
    keys = {}
    for k in KB["keys"]:
        o = bpy.data.objects["Key%d" % k["note"]]
        piv = D((k["x0"] + k["x1"]) / 2, KEY_PIVOT_Y, BLACK_TOP if k["black"] else WHITE_TOP)
        o.data.transform(Matrix.Translation(-piv)); o.location = piv
        keys[k["note"]] = o
    # Luz de barrido: tira alta y angosta que cruza delante del instrumento (reflejo que corre por la madera y
    # por las perillas). Energía y posición por toma.
    sw = bpy.data.lights.new("Sweep", "AREA"); sw.shape = "RECTANGLE"; sw.size = 3.0; sw.size_y = 160.0
    sw.color = (1.0, 0.93, 0.84); sw.energy = 0.0
    swo = bpy.data.objects.new("Sweep", sw); link(swo)
    # Brillo de la lámpara OVERLOAD sobre el panel: una luz puntual dentro de la lente.
    ov = next(l for l in L["lamps"] if l["id"] == "overload")
    gl = bpy.data.lights.new("OvlGlow", "POINT"); gl.shadow_soft_size = 0.25; gl.color = (1.0, 0.45, 0.08); gl.energy = 0.0
    glo = bpy.data.objects.new("OvlGlow", gl); link(glo); glo.location = P(ov["x"], ov["y"], 7.5)
    lamp_mats = []
    for ch in lamps["overload"].children:
        for m in ch.data.materials:
            b = m.node_tree.nodes.get("Principled BSDF")
            if b and b.inputs["Emission Strength"].default_value > 0: lamp_mats.append((b, b.inputs["Emission Strength"].default_value))
    return dict(knobs=knobs, lamps=lamps, wheels=wheels, keys=keys, sweep=swo, glow=glo, lamp_mats=lamp_mats)


def film_paths():
    """Recorridos de cámara por toma (unidades de Blender = 10 mm; f-stop dividido por 10 por la escala)."""
    from mathutils import Vector as V
    import filmkit as fk
    cut, ovl = P(453.6, 44.3), P(383.8, 66.5)
    whl = D(66.8, 372.0, LHC_Z)                      # entre las dos ruedas
    return {
        # Apertura: el instrumento arriba a la derecha, piso oscuro abajo a la izquierda (ahí va el título);
        # avance lento con una grúa que baja apenas.
        "open":   fk.Path([(0, V((-50.0, -266.0, 130.0)), V((27.0, -18.0, 6.0)), 86, 0.6),
                           (1, V((-41.0, -246.0, 119.0)), V((29.5, -18.0, 7.0)), 86, 0.6)]),
        # Teclado al ras: travelling lateral hacia la izquierda sobre las teclas que tocan la línea.
        "keys":   fk.Path([(0, V((74.0, -70.0, 15.5)), V((44.0, -40.0, 6.8)), 85, 0.5),
                           (1, V((60.0, -73.0, 14.5)), V((30.0, -40.0, 6.8)), 85, 0.5)]),
        # Filtro: la perilla de corte gira en primer plano; el foco pasa a la lámpara OVERLOAD que destella.
        "filter": fk.Path([(0, cut + V((10.5, -30.0, 17.0)), cut + V((-1.0, 0.0, 0.0)), 100, 0.55),
                           (1, cut + V((6.5, -26.0, 14.5)), (cut + ovl) / 2 + V((0.0, 0.0, 0.0)), 100, 0.55)]),
        # Ruedas: ángulo bajo desde adelante a la izquierda, arco suave hacia el teclado.
        "wheels": fk.Path([(0, whl + V((-22.0, -36.0, 16.0)), whl + V((1.5, 0.0, -0.5)), 90, 0.5),
                           (1, whl + V((-15.0, -40.0, 19.0)), whl + V((3.0, 0.0, -0.5)), 90, 0.5)]),
        # Barrido: travelling a lo largo del panel (mezclador → filtro) con la luz que cruza en sentido contrario.
        "sweep":  fk.Path([(0, V((22.0, -82.0, 58.0)), V((30.0, -14.0, 19.0)), 85, 0.6),
                           (1, V((44.0, -84.0, 56.0)), V((50.0, -14.0, 19.0)), 85, 0.6)], easing="linear"),
        # Cierre: de frente, se aleja despacio (film.py lo oscurece y pone el cierre al centro).
        "end":    fk.Path([(0, V((30.0, -250.0, 140.0)), V((36.5, -20.0, 10.0)), 100, 0.7),
                           (1, V((27.0, -320.0, 175.0)), V((36.5, -21.0, 9.0)), 100, 0.7)], easing="out"),
    }


# Luz de barrido por toma: (desde, hasta, mira, energía máxima); el centro de la tira va de un punto al otro.
SWEEPS = {
    "open":  ((-60.0, -130.0, 70.0), (120.0, -120.0, 80.0), (36.0, -20.0, 8.0), 4.0e4),
    "sweep": ((110.0, -95.0, 70.0), (-20.0, -95.0, 70.0), (40.0, -14.0, 18.0), 3.0e4),
    "end":   ((-40.0, -140.0, 90.0), (110.0, -140.0, 90.0), (36.0, -20.0, 8.0), 1.5e4),
}


def film2():
    """Film de la web del ATMD: 21,6 s, seis tomas, sobre "Fat Bass" (web/assets/samples/atmd/fat-bass.m4a).

    Storyboard (film.json: 100 BPM, cortes secos en tiempos; la línea suena en el teclado dos octavas arriba):
      1. open    0,0–4,2 s (7 t)  Plano general 3/4 desde la izquierda, alto: el instrumento arriba a la derecha y
                                  el piso oscuro abajo a la izquierda, donde entra el título. Avance lento; una luz
                                  de barrido cruza la madera de nogal; las teclas ya tocan la línea de bajo.
      2. keys    4,2–7,8 s (6 t)  Al ras del teclado, de la derecha: travelling lateral sobre las teclas que bajan
                                  con cada nota (Mi, Sol, La, Si…); foco que sigue la última tecla tocada.
      3. filter  7,8–11,4 s (6 t) Macro del filtro: la perilla CUTOFF gira de −1,5 a +2,2 y AMOUNT OF CONTOUR
                                  baja; el foco pasa de la perilla a la lámpara OVERLOAD, que destella en los picos.
      4. wheels 11,4–14,4 s (5 t) Ángulo bajo sobre el controlador izquierdo: la rueda MOD sube, la PITCH hace un
                                  bend y vuelve al centro; detrás, el teclado que sigue tocando.
      5. sweep  14,4–17,4 s (5 t) Travelling a lo largo del panel (mezclador → filtro) con una tira de luz que cruza
                                  al revés: el reflejo corre por las perillas estriadas y el listón; EMPHASIS sube,
                                  CUTOFF baja; la lámpara sigue los picos.
      6. end    17,4–21,6 s (7 t) De frente, se aleja despacio; film.py lo oscurece y pone el cierre al centro.
    """
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final"); step = int(arg("--step", "1"))
    env = fk.envelope(arg("--audio"))
    midi = arg("--midi", os.path.join(HERE, "..", "..", "..", "..", "web", "assets", "samples", "atmd", "fat-bass.mid"))
    notes = [(a, b, n + NOTE_UP) for a, b, n in midi_notes(midi)]
    preset = arg("--preset", os.path.join(ASSETS, "presets", "p01_fat_bass.json"))
    values = dict(json.load(open(preset)).get("params", {}))
    # Lámpara: se enciende cuando la envolvente pasa del 70 % (los golpes fuertes del compás) y se apaga en ~0,15 s.
    lamp_k = []
    g = 0.0
    for e in env:
        x = max(0.0, min(1.0, (e - 0.70) / 0.22)); g = x if x > g else g * 0.62; lamp_k.append(g)
    def env_at(T, arr): return arr[min(len(arr) - 1, max(0, int(round(T * FPS_FILM))))]

    def key_depth(note, T):
        """Hundimiento 0..1 de una tecla en el instante T: baja en 25 ms, sube en 60 ms."""
        d = 0.0
        for a, b, n in notes:
            if n != note or T < a - 0.03 or T > b + 0.1: continue
            if T < a: d = max(d, 0.0)
            elif T < a + 0.025: d = max(d, (T - a) / 0.025)
            elif T < b: d = 1.0
            else: d = max(d, 1.0 - (T - b) / 0.06)
        return max(0.0, min(1.0, d))
    played = sorted({n for _, _, n in notes})
    key_ang = math.atan2(KEY_DIP, KB["y_white"] - KEY_PIVOT_Y)

    for (name, t0, dur) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        S = film_scene(values)
        sc = bpy.context.scene
        fk.setup(sc, quality)
        sc.render.image_settings.color_mode = "RGB"
        cam = fk.camera(sc); cam.data.clip_start = 0.5; cam.data.clip_end = 6000.0
        path = film_paths()[name]
        frames = int(round(dur * FPS_FILM))
        knobs, wheels, keys = S["knobs"], S["wheels"], S["keys"]

        def knob_set(kid, v):
            o, kn = knobs[kid]; o.rotation_euler.z = -math.radians(knob_angle(kn, v)); return o

        def wheel_set(wid, v):
            o, wl = wheels[wid]
            ang = -v * wl["travel_deg"] / 2.0 if wid == "pitch_wheel" else -(v - 0.5) * wl["travel_deg"]
            o.rotation_euler.x = math.radians(ang); return o

        def pose(f, t, u, name=name, t0=t0, path=path):
            T = t0 + t
            moved = []
            for n in played:
                o = keys[n]; o.rotation_euler.x = key_ang * key_depth(n, T); moved.append(o)
            hand = 0.025 * fk.noise1(T, 7, rate=0.9)          # la mano no queda quieta del todo
            moved.append(knob_set("filter_cutoff", smooth_seg(T, CUTOFF) + (hand if 8.2 < T < 17.2 else 0.0)))
            moved.append(knob_set("filter_emphasis", smooth_seg(T, EMPH)))
            moved.append(knob_set("filter_contour", smooth_seg(T, CONTOUR)))
            moved.append(wheel_set("mod_wheel", smooth_seg(T, MODW)))
            moved.append(wheel_set("pitch_wheel", smooth_seg(T, PITCHW)))
            # Foco por toma.
            focus = None
            if name == "keys":                              # sigue (suavizado) la última tecla tocada
                xs = [key_x(n) for a, b, n in notes if a <= T + 0.05]
                recent = [key_x(n) for a, b, n in notes if T - 0.9 <= a <= T + 0.05] or xs[-1:] or [key_x(52)]
                focus = Vector((sum(recent) / len(recent), -40.5, 7.0))
            elif name == "filter":                          # de la perilla de corte a la lámpara OVERLOAD
                k = fk.rack(u, 0.45, 0.75, 0.0, 1.0)
                focus = P(453.6, 44.3, 14.0).lerp(P(383.8, 66.5, 4.0), k)
            elif name == "wheels":
                focus = D(46.4, 372.0, LHC_Z + 20.0).lerp(D(87.2, 372.0, LHC_Z + 20.0), fk.rack(u, 0.2, 0.6, 0.0, 1.0))
            elif name == "sweep":
                focus = path.at(u)[1]
            shake = {"keys": 0.10, "filter": 0.08, "wheels": 0.10}.get(name, 0.0)
            path.apply(cam, u, focus=focus, shake=shake, t=T)
            sw = SWEEPS.get(name)
            if sw:
                a, b, look, _ = sw
                p = Vector(a).lerp(Vector(b), fk.ease(u, "smooth"))
                S["sweep"].location = p
                S["sweep"].rotation_euler = (Vector(look) - p).to_track_quat("-Z", "Y").to_euler()
                moved.append(S["sweep"])
            return moved

        def keys_fn(f, name=name, t0=t0, frames=frames):
            u = (f - 1) / max(1, frames - 1); T = t0 + (f - 1) / FPS_FILM
            sw = SWEEPS.get(name)
            S["sweep"].data.energy = sw[3] * math.sin(math.pi * u) ** 0.7 if sw else 0.0
            S["sweep"].data.keyframe_insert("energy")
            k = env_at(T, lamp_k)
            S["glow"].data.energy = 60.0 * k; S["glow"].data.keyframe_insert("energy")
            for b, e0 in S["lamp_mats"]:
                b.inputs["Emission Strength"].default_value = e0 * (0.04 + 1.6 * k)
                b.inputs["Emission Strength"].keyframe_insert("default_value")

        t_start = time.time()
        fk.render_shot(sc, name, frames, pose, OUT, step=step, extra_keys=keys_fn)
        print(f"toma {name}: {frames} cuadros, {time.time() - t_start:.0f} s", flush=True)


FPS_FILM = 24


if __name__ == "__main__":
    modes = {"bg": mode_bg, "sprites": mode_sprites, "preview": mode_preview, "hero": mode_hero, "film": mode_film,
             "film2": film2}
    for m in arg("--mode", "bg,sprites").split(","):
        t0 = time.time()
        print("== modo", m, flush=True); modes[m]()
        print(f"== modo {m}: {time.time() - t0:.1f} s", flush=True)
