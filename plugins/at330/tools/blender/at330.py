"""El AT330 renderizado, dirección "Studio 1979" (elegida el 2026-10-23 entre tres; las pruebas están en
audiotools-private/at330/ui-directions/): un instrumento propio de estudio de fines de los 70, no el panel del original.
Panel marrón carbón mate, mejillas de nogal, llaves basculantes en tres colores apagados (ocre las cuerdas, óxido el
coro, petróleo el vocoder, crema los ensembles), deslizadores negros, un visor ámbar y teclas de marfil.

  --mode ui        lo que la interfaz dibuja (plugins/at330/assets/ui/), sin serigrafía (la dibuja la interfaz):
                     bg.jpg        el panel y el teclado con las llaves apagadas, sin tapas ni palancas
                     on.jpg        el mismo encuadre con todas las llaves prendidas (la interfaz recorta cada una)
                     keysdown.jpg  todas las teclas apretadas
                     slider.png, knob.png, lever.png   tapa de deslizador, tapa de perilla (sin índice) y palanca,
                                   con fondo transparente; la interfaz las ubica y dibuja la sombra y el índice
                     layout.json   dónde está cada cosa, en píxeles de la ventana a 1x
  --mode photos    las fotos de la web (hero, close, top), con serigrafía, teclas de tamaño real y el visor encendido
  --mode film2     las tomas del film de la web (scripts/film.py y tools/film.json)

Sin marcas: la placa dice "AT330 / Audio Tools". Unidades: mm; el origen es la esquina superior izquierda del panel, x a
la derecha, y hacia abajo (en Blender y se invierte). La disposición es la de la dirección elegida.

  B=/Volumes/Docs/Applications/blender/Blender.app/Contents/MacOS/Blender      (o el de pc50-wsl, bajo el lock de la GPU)
  $B -b --factory-startup -P plugins/at330/tools/blender/at330.py -- --mode ui [--samples 128]
"""
import json, math, os, sys
import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
MODE = arg("--mode", "ui"); SAMPLES = int(arg("--samples", "128"))
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
UI = os.path.abspath(os.path.join(HERE, "..", "..", "assets", "ui"))
OUT = os.path.abspath(arg("--out", UI if MODE == "ui" else "/tmp/claude-501/at330-photos"))
FONT = os.path.join(REPO, "plugins", "lacquer", "assets", "fonts", "BarlowCondensed-SemiBold.ttf")
FONT_M = os.path.join(REPO, "plugins", "lacquer", "assets", "fonts", "BarlowCondensed-Medium.ttf")
MM = 0.001

# ── Disposición (mm) ──
PW, PH = 1040.0, 350.0              # el panel
KEYS_Y, KEYS_H = PH, 182.0          # en la interfaz el teclado ocupa todo el ancho debajo del panel
WIN_W, TOP = 1600, 44               # la ventana a 1x: barra de 44 px, panel + teclado 1600 x 880, barra de 36 px
# El encuadre de la interfaz incluye las mejillas de nogal (40 mm a cada lado), el listón de arriba y el frente.
VX0, VY0 = -40.0, -34.0
VIEW_W = PW + 80.0
PX = WIN_W / VIEW_W                 # px por mm a 1x
SCALE = 2                           # las imágenes van a 2x
YA, YB, TRAVEL = 95.0, 232.0, 92.0
SECTIONS = [("pitch", "PITCH SHIFT", 14, 156), ("vib", "VIBRATO", 164, 292), ("str", "STRINGS", 300, 436),
            ("choir", "CHOIR", 444, 662), ("voc", "VOCODER", 670, 952), ("master", "MASTER", 960, 1028)]
def C(id, kind, x, y, label, sec, color=None, sub=""): return dict(id=id, kind=kind, x=x, y=y, label=label, sec=sec, color=color, sub=sub)
CONTROLS = [
    C("pitch_mode", "switch3", 32, YA, "MODE", "pitch"), C("pitch_set", "knob", 92, YA, "DEPTH", "pitch"),
    C("pitch_time", "knob", 134, YA, "GLIDE TIME", "pitch"), C("pitch_amount", "knob", 88, YB, "BEND DOWN", "pitch"),
    C("vib_rate", "knob", 186, YB, "RATE", "vib"), C("vib_depth", "knob", 228, YB, "DEPTH", "vib"),
    C("vib_delay", "knob", 270, YB, "DELAY", "vib"), C("vib_led", "led", 228, YA, "", "vib"),
    C("str_upper", "tablet", 326, YA, "UPPER", "str", "str", "4'"), C("str_lower", "tablet", 368, YA, "LOWER", "str", "str", "4'"),
    C("str_ens", "tablet", 410, YA, "ENSEMBLE", "str", "ens", "CHORUS"),
    C("str_attack", "knob", 326, YB, "ATTACK", "str"), C("str_tone", "knob", 368, YB, "TONE", "str"), C("str_level", "knob", 410, YB, "LEVEL", "str"),
    C("ch_female_up", "tablet", 470, YA, "FEMALE", "choir", "choir", "4' UPPER"), C("ch_male8_up", "tablet", 512, YA, "MALE", "choir", "choir", "8' UPPER"),
    C("ch_male4_lo", "tablet", 554, YA, "MALE", "choir", "choir", "4' LOWER"), C("ch_male8_lo", "tablet", 596, YA, "MALE", "choir", "choir", "8' LOWER"),
    C("ch_ens", "tablet", 638, YA, "ENSEMBLE", "choir", "ens", "CHORUS"),
    C("ch_attack", "knob", 512, YB, "ATTACK", "choir"), C("ch_level", "knob", 596, YB, "LEVEL", "choir"),
    C("voc_upper", "tablet", 696, YA, "UPPER", "voc", "voc", "8'"), C("voc_lower", "tablet", 738, YA, "LOWER", "voc", "voc", "8'"),
    C("voc_ens", "tablet", 780, YA, "ENSEMBLE", "voc", "ens", "CHORUS"), C("hold", "tablet", 822, YA, "HOLD", "voc", "hold", "VOWELS"),
    C("mic_level", "knob", 696, YB, "MIC GAIN", "voc"), C("voc_level", "knob", 738, YB, "LEVEL", "voc"),
    C("voc_tone", "knob", 780, YB, "TONE", "voc"), C("consonants", "knob", 822, YB, "CONSONANTS", "voc"),
    C("direct_mic", "knob", 864, YB, "DIRECT MIC", "voc"), C("mic_led", "led", 906, YB - 26, "", "voc"),
    C("release", "knob", 994, YB, "RELEASE", "master"), C("tune", "knob", 994, YA - 4, "TUNE", "master"),
    C("volume", "knob", 994, YA + 62, "VOLUME", "master"),
]
BANDS = dict(x0=850.0, x1=940.0, y0=YA - 34.0, y1=YA + 34.0)
PLATE_Y0, PLATE_Y1 = 6.0, 296.0     # las placas de las secciones
NAME_Y0, NAME_Y1 = 302.0, 344.0     # la franja de nogal con el nombre
TAB_W, TAB_H, CAP_W, CAP_H, KNOB_R = 30.0, 48.0, 14.0, 22.0, 13.0
KW_UI = PW / 29                     # ancho de la blanca en la interfaz
WHITES = [0, 2, 4, 5, 7, 9, 11]

# Valores para las fotos y el film (la interfaz los dibuja ella).
PHOTO = {"pitch_mode": 1, "pitch_set": 0.45, "pitch_time": 0.3, "pitch_amount": 0.0, "vib_rate": 0.25, "vib_depth": 0.3, "vib_delay": 0.3,
         "str_attack": 0.3, "str_tone": 0.55, "str_level": 0.8, "ch_attack": 0.35, "ch_level": 0.75, "mic_level": 0.5, "voc_level": 0.8,
         "voc_tone": 0.6, "consonants": 0.5, "direct_mic": 0.0, "release": 0.45, "tune": 0.5, "volume": 0.75,
         "on": {"str_upper", "str_lower", "str_ens", "ch_female_up", "ch_male4_lo", "ch_ens", "voc_upper", "voc_lower", "voc_ens"}}
BAND_LEVELS = [0.35, 0.62, 0.88, 0.7, 0.52, 0.66, 0.4, 0.3, 0.22, 0.15]

def bl(x, y, z=0.0): return Vector((x * MM, -y * MM, z * MM))

# ── Escena ──
def reset(res):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("CUDA", "METAL", "OPTIX"):
        try:
            prefs.compute_device_type = kind; prefs.get_devices()
            if any(d.type == kind for d in prefs.devices):
                for d in prefs.devices: d.use = d.type == kind
                break
        except Exception: pass
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    try: sc.cycles.denoising_use_gpu = True
    except Exception: pass
    sc.cycles.seed = 7                              # bg y on con el mismo ruido: los recortes no se notan
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Base Contrast"
    sc.render.resolution_x, sc.render.resolution_y = res; sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    return sc

MATS = {}
def mat(name, base, rough=0.5, metal=0.0, coat=0.0, emit=None, strength=0.0, noise=0.0, wood=False, brushed=False):
    if name in MATS: return MATS[name]
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.06
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    tc = None
    if noise or wood or brushed: tc = nt.nodes.new("ShaderNodeTexCoord")
    if wood:
        # Nogal: vetas largas casi rectas a lo largo de la pieza ("x" o "y") hechas con ruido estirado, una figura lenta
        # y suave encima, poco contraste entre tonos y poro fino como relieve.
        along_x = wood == "x"
        mps = nt.nodes.new("ShaderNodeMapping"); mps.inputs["Scale"].default_value = (3.0, 140.0, 3.0) if along_x else (140.0, 3.0, 3.0)
        streak = nt.nodes.new("ShaderNodeTexNoise"); streak.inputs["Scale"].default_value = 1.0; streak.inputs["Detail"].default_value = 7.0
        streak.inputs["Roughness"].default_value = 0.62
        mpf = nt.nodes.new("ShaderNodeMapping"); mpf.inputs["Scale"].default_value = (1.0, 4.0, 1.0) if along_x else (4.0, 1.0, 1.0)
        fig = nt.nodes.new("ShaderNodeTexWave"); fig.wave_type = "BANDS"; fig.bands_direction = "Y" if along_x else "X"
        fig.inputs["Scale"].default_value = 9.0; fig.inputs["Distortion"].default_value = 5.0; fig.inputs["Detail"].default_value = 2.0
        pore = nt.nodes.new("ShaderNodeTexNoise"); pore.inputs["Scale"].default_value = 2500.0; pore.inputs["Detail"].default_value = 1.0
        m1 = nt.nodes.new("ShaderNodeMath"); m1.operation = "MULTIPLY"; m1.inputs[1].default_value = 0.25
        m2 = nt.nodes.new("ShaderNodeMath"); m2.operation = "MULTIPLY_ADD"; m2.inputs[1].default_value = 0.75
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.30; ramp.color_ramp.elements[0].color = (0.075, 0.037, 0.018, 1)
        ramp.color_ramp.elements[1].position = 0.70; ramp.color_ramp.elements[1].color = (0.20, 0.10, 0.048, 1)
        for src, dst in ((tc.outputs["Object"], mps.inputs["Vector"]), (mps.outputs["Vector"], streak.inputs["Vector"]),
                         (tc.outputs["Object"], mpf.inputs["Vector"]), (mpf.outputs["Vector"], fig.inputs["Vector"]),
                         (tc.outputs["Object"], pore.inputs["Vector"]), (fig.outputs["Fac"], m1.inputs[0]),
                         (streak.outputs["Fac"], m2.inputs[0]), (m1.outputs[0], m2.inputs[2]), (m2.outputs[0], ramp.inputs["Fac"]),
                         (ramp.outputs["Color"], b.inputs["Base Color"])):
            nt.links.new(src, dst)
        bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.05
        nt.links.new(pore.outputs["Fac"], bump.inputs["Height"]); nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    if brushed:
        # Aluminio anodizado cepillado: ruido estirado a lo largo de x, como relieve y un poco en el color.
        mp2 = nt.nodes.new("ShaderNodeMapping"); mp2.inputs["Scale"].default_value = (6.0, 900.0, 6.0)
        nz2 = nt.nodes.new("ShaderNodeTexNoise"); nz2.inputs["Scale"].default_value = 6.0; nz2.inputs["Detail"].default_value = 8.0
        bump2 = nt.nodes.new("ShaderNodeBump"); bump2.inputs["Strength"].default_value = 0.12
        mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.inputs["Factor"].default_value = 0.35
        mix.inputs[6].default_value = (*base, 1); mix.inputs[7].default_value = tuple(c * 1.35 for c in base) + (1,)
        nt.links.new(tc.outputs["Object"], mp2.inputs["Vector"]); nt.links.new(mp2.outputs["Vector"], nz2.inputs["Vector"])
        nt.links.new(nz2.outputs["Fac"], bump2.inputs["Height"]); nt.links.new(bump2.outputs["Normal"], b.inputs["Normal"])
        nt.links.new(nz2.outputs["Fac"], mix.inputs["Factor"]); nt.links.new(mix.outputs[2], b.inputs["Base Color"])
    if noise:
        nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 900.0; nz.inputs["Detail"].default_value = 2.0
        bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = noise
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"]); nt.links.new(nz.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    MATS[name] = m
    return m

TAB = {"str": (0.78, 0.52, 0.12), "choir": (0.60, 0.20, 0.10), "voc": (0.10, 0.40, 0.44), "ens": (0.86, 0.82, 0.70), "hold": (0.42, 0.42, 0.44)}
def materials():
    MATS.clear()
    return dict(
        panel=mat("panel", (0.045, 0.040, 0.036), 0.62, noise=0.08), cheek=walnut("cheek", False, 0.13),
        rail=walnut("rail", True, 0.41), board=walnut("board", True, 0.77), front=walnut("front", True, 0.29), base=mat("base", (0.012, 0.011, 0.010), 0.7),
        plate=mat("plate", (0.052, 0.046, 0.041), 0.48, brushed=True), screw=mat("screw", (0.5, 0.48, 0.45), 0.35, metal=1.0),
        slot=mat("slot", (0.006, 0.006, 0.006), 0.85), cap=mat("cap", (0.02, 0.02, 0.02), 0.35), capline=mat("capline", (0.85, 0.82, 0.74), 0.4),
        knob=mat("knob", (0.025, 0.025, 0.025), 0.3), knobtop=mat("knobtop", (0.62, 0.62, 0.64), 0.28, metal=1.0),
        trim=mat("trim", (0.55, 0.55, 0.57), 0.32, metal=1.0), ivory=mat("ivory", (0.80, 0.77, 0.69), 0.32), ebony=mat("ebony", (0.014, 0.014, 0.014), 0.3, coat=0.3),
        well=mat("well", (0.01, 0.01, 0.01), 0.7), window=mat("window", (0.01, 0.009, 0.008), 0.08, coat=1.0),
        silk=mat("silk", (0.92, 0.87, 0.76), 0.5), accent=mat("accent", (0.85, 0.55, 0.17), 0.5),
        led_on=mat("led_on", (1, 0.12, 0.05), 0.3, emit=(1, 0.12, 0.05), strength=8), led_off=mat("led_off", (0.16, 0.02, 0.02), 0.35),
        bar_on=mat("bar_on", (1, 0.45, 0.08), 0.3, emit=(1, 0.42, 0.06), strength=2.6), bar_off=mat("bar_off", (0.05, 0.035, 0.02), 0.5),
        floor=mat("floor", (0.12, 0.10, 0.085), 0.55),
    )
TEX = os.environ.get("AT330_TEX", os.path.join(REPO, "..", "audiotools-private", "at330-ref", "textures"))
def walnut(name, along_x, offset):
    """Nogal aceitado y laqueado: la textura de nogal negro (Poly Haven, CC0; tools/blender/fetch_textures.sh),
    oscurecida y entibiada, rugosidad y normales de la misma textura y una capa de laca satinada. Cada pieza toma otra
    parte de la textura (offset). Sin las texturas, el nogal procedural."""
    files = {k: os.path.join(TEX, f"black_walnut_veneer_02_{k}_4k.jpg") for k in ("diff", "rough", "nor_gl")}
    if not all(os.path.exists(f) for f in files.values()):
        return mat(name, (0.2, 0.1, 0.05), 0.42, coat=0.35, wood="x" if along_x else "y")
    if name in MATS: return MATS[name]
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.1, 1.1, 1.1)                 # un tile cubre ~0,9 m
    mp.inputs["Location"].default_value = (offset, offset * 0.37, 0.0)
    if along_x: nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    else:      # veta a lo largo de y: se cambian x e y antes de proyectar
        sep = nt.nodes.new("ShaderNodeSeparateXYZ"); com = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs["Vector"])
        nt.links.new(sep.outputs["X"], com.inputs["Y"]); nt.links.new(sep.outputs["Y"], com.inputs["X"]); nt.links.new(sep.outputs["Z"], com.inputs["Z"])
        nt.links.new(com.outputs["Vector"], mp.inputs["Vector"])
    def tex(key, colour):
        t = nt.nodes.new("ShaderNodeTexImage"); t.image = bpy.data.images.load(files[key], check_existing=True)
        t.projection = "BOX"; t.projection_blend = 0.15
        if not colour: t.image.colorspace_settings.name = "Non-Color"
        nt.links.new(mp.outputs["Vector"], t.inputs["Vector"]); return t
    dif, rough, nor = tex("diff", True), tex("rough", False), tex("nor_gl", False)
    # Aceite: más oscuro, más cálido y con más contraste entre veta y fondo.
    gam = nt.nodes.new("ShaderNodeGamma"); gam.inputs["Gamma"].default_value = 1.9
    tint = nt.nodes.new("ShaderNodeMix"); tint.data_type = "RGBA"; tint.blend_type = "MULTIPLY"; tint.inputs["Factor"].default_value = 1.0
    tint.inputs[7].default_value = (0.78, 0.50, 0.30, 1)
    nt.links.new(dif.outputs["Color"], gam.inputs["Color"]); nt.links.new(gam.outputs["Color"], tint.inputs[6])
    nt.links.new(tint.outputs[2], b.inputs["Base Color"])
    rr = nt.nodes.new("ShaderNodeMapRange"); rr.inputs["To Min"].default_value = 0.30; rr.inputs["To Max"].default_value = 0.55
    nt.links.new(rough.outputs["Color"], rr.inputs["Value"]); nt.links.new(rr.outputs["Result"], b.inputs["Roughness"])
    nm = nt.nodes.new("ShaderNodeNormalMap"); nm.inputs["Strength"].default_value = 0.7
    nt.links.new(nor.outputs["Color"], nm.inputs["Color"]); nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
    b.inputs["Coat Weight"].default_value = 0.45; b.inputs["Coat Roughness"].default_value = 0.18
    MATS[name] = m
    return m

def tablet_mat(kind, on):
    # Apagada: el color bastante más oscuro (que el estado se lea sin la luz de la habitación); prendida, el color pleno.
    k = 1.0 if on else 0.30
    return mat(f"tab_{kind}_{int(on)}", tuple(c * k for c in TAB[kind]), 0.34 if on else 0.5)

def box(name, x, y, z, sx, sy, sz, m, bevel=0.0, rot=None, segs=3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=bl(x, y, z))
    o = bpy.context.active_object; o.name = name; o.scale = (sx * MM, sy * MM, sz * MM)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if rot: o.rotation_euler = rot
    if bevel:
        b = o.modifiers.new("bv", "BEVEL"); b.width = bevel * MM; b.segments = segs
    o.data.materials.append(m); return o

def cyl(name, x, y, z, r, h, m, verts=64, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r * MM, depth=h * MM, location=bl(x, y, z))
    o = bpy.context.active_object; o.name = name
    if bevel:
        b = o.modifiers.new("bv", "BEVEL"); b.width = bevel * MM; b.segments = 3
    bpy.ops.object.shade_smooth()
    o.data.materials.append(m); return o

def text(s, x, y, size, m, align="CENTER", z=0.05, font=FONT):
    cu = bpy.data.curves.new("t", "FONT"); cu.body = s; cu.size = size * MM; cu.align_x = align; cu.align_y = "CENTER"
    if os.path.exists(font): cu.font = bpy.data.fonts.load(font, check_existing=True)
    cu.extrude = 0.00001
    o = bpy.data.objects.new("t", cu); bpy.context.scene.collection.objects.link(o)
    o.location = bl(x, y, z); cu.materials.append(m)
    return o

# ── Piezas ──
def slider_cap(M, x, y, z=6.0):
    o = box("cap", x, y, z, CAP_W, CAP_H, 12, M["cap"], bevel=2.2)
    box("capline", x, y, z + 6.2, CAP_W - 1, 1.6, 0.6, M["capline"])
    return o

def knob_cap(M, x, y):
    cyl("knob", x, y, 9, KNOB_R, 16, M["knob"], verts=96, bevel=1.2)
    # Nervaduras alrededor de la pollera.
    for k in range(32):
        a = 2 * math.pi * k / 32
        box("rib", x + math.cos(a) * (KNOB_R - 0.2), y + math.sin(a) * (KNOB_R - 0.2), 7, 1.2, 0.9, 11, M["knob"], rot=(0, 0, -a))
    cyl("knobtop", x, y, 17.1, 8.3, 0.6, M["knobtop"], verts=96)

def lever(M, x, y):
    box("lever", x, y, 7, 5, 6, 12, M["ivory"], bevel=1.5)

def tablet(M, k, on):
    tilt = math.radians(-6 if on else 6)
    box("tab", k["x"], k["y"], 5, TAB_W, TAB_H, 9, tablet_mat(k["color"], on), bevel=2.5, rot=(tilt, 0, 0))

def build_panel(M, state, ui=True):
    """state: 'off' (todas apagadas, sin tapas), 'on' (todas prendidas, sin tapas) o 'photo' (con tapas y valores)."""
    box("panel", PW / 2, PH / 2, -12, PW, PH, 20, M["base"], bevel=1.5)
    for sid, title, x0, x1 in SECTIONS:
        box("plate_" + sid, (x0 + x1) / 2, (PLATE_Y0 + PLATE_Y1) / 2, -1.5, x1 - x0 + 6, PLATE_Y1 - PLATE_Y0, 3, M["plate"], bevel=1.4)
        for sx in (x0 + 1, x1 - 1):
            for sy in (PLATE_Y0 + 6, PLATE_Y1 - 6):
                cyl("screw", sx, sy, 0.3, 2.2, 1.2, M["screw"], verts=32, bevel=0.4)
                box("slot", sx, sy, 0.95, 3.2, 0.5, 0.3, M["base"])
    box("rail", PW / 2, -17, 4, PW + 2, 32, 16, M["rail"], bevel=3)
    box("nameboard", PW / 2, (NAME_Y0 + NAME_Y1) / 2, -2, PW, NAME_Y1 - NAME_Y0, 6, M["board"], bevel=2)
    box("lip", PW / 2, PH + 2, -3, PW, 4, 14, M["trim"], bevel=1)
    for c in CONTROLS:
        x, y = c["x"], c["y"]
        if c["kind"] == "slider":
            box("slot", x, y, -1.5, 4, TRAVEL + 14, 3, M["slot"])
            if state == "photo": slider_cap(M, x, y + TRAVEL / 2 - PHOTO[c["id"]] * TRAVEL)
        elif c["kind"] == "knob":
            cyl("skirt", x, y, 0.8, KNOB_R + 3, 1.6, M["trim"], verts=96)
            if state == "photo":
                knob_cap(M, x, y)
                a = math.radians(-135 + 270 * PHOTO[c["id"]])
                box("ptr", x + 5 * math.sin(a), y - 5 * math.cos(a), 17.6, 1.4, 6.5, 0.5, M["silk"], rot=(0, 0, -a))
        elif c["kind"] == "tablet":
            box("well", x, y, -1, TAB_W + 4, TAB_H + 4, 2, M["well"])
            on = state == "on" or (state == "photo" and c["id"] in PHOTO["on"])
            tablet(M, c, on)
        elif c["kind"] == "switch3":
            box("plate", x, y, 0.5, 18, 40, 1, M["trim"], bevel=1)
            box("slot3", x, y, 1.1, 5, 30, 0.3, M["slot"])
            if state == "photo": lever(M, x, y - 10 + 10 * PHOTO["pitch_mode"])
        elif c["kind"] == "led":
            cyl("ledring", x, y, 0.6, 4.2, 1.2, M["trim"], verts=48)
            cyl("led", x, y, 1.6, 3, 2.5, M["led_on"] if state == "photo" else M["led_off"], verts=48)
    # El visor de las bandas: una ventana oscura; en las fotos, los segmentos encendidos.
    B = BANDS
    box("window", (B["x0"] + B["x1"]) / 2, (B["y0"] + B["y1"]) / 2, -0.4, B["x1"] - B["x0"] + 10, B["y1"] - B["y0"] + 10, 1.6, M["window"], bevel=1)
    box("bezel", (B["x0"] + B["x1"]) / 2, (B["y0"] + B["y1"]) / 2, -0.8, B["x1"] - B["x0"] + 14, B["y1"] - B["y0"] + 14, 1.4, M["trim"], bevel=1)
    if state == "photo":
        w = (B["x1"] - B["x0"]) / 10
        for i, lv in enumerate(BAND_LEVELS):
            for s in range(12):
                yy = B["y1"] - 3 - s * (B["y1"] - B["y0"] - 6) / 12
                box("seg", B["x0"] + w * (i + 0.5), yy, 0.5, w * 0.62, 3.2, 0.4, M["bar_on"] if s < lv * 12 else M["bar_off"])
        silkscreen(M)

SEC_COL = {"str": (0.78, 0.52, 0.12), "choir": (0.69, 0.27, 0.16), "voc": (0.17, 0.56, 0.58)}
def silkscreen(M):
    """La serigrafía de las fotos y el film, como la dibuja la interfaz: títulos con la marca del color de su sección,
    etiquetas en seminegrita, escalas 0-10 alrededor de las perillas y las llaves en dos renglones."""
    for sid, title, x0, x1 in SECTIONS:
        cx = (x0 + x1) / 2; tw = len(title) * 3.6 + 4
        text(title, cx + 4, 21, 9.0, M["silk"])
        box("chip", cx - tw - 4, 21, 0.06, 5, 5, 0.12, mat("chip_" + sid, SEC_COL.get(sid, (0.85, 0.80, 0.70)), 0.5))
        box("rule", (x0 + 5 + cx - tw - 10) / 2, 21, 0.05, max(1, cx - tw - 10 - x0 - 5), 0.6, 0.1, M["silk"])
        box("rule", (cx + tw + 8 + x1 - 5) / 2, 21, 0.05, max(1, x1 - 5 - cx - tw - 8), 0.6, 0.1, M["silk"])
    for c in CONTROLS:
        if not c["label"]: continue
        if c["kind"] == "tablet":
            text(c["label"], c["x"], c["y"] + TAB_H / 2 + 7.5, 5.0, M["silk"], font=FONT)
            if c["sub"]: text(c["sub"], c["x"], c["y"] + TAB_H / 2 + 13.5, 3.6, M["silk"], font=FONT_M)
        elif c["kind"] == "knob":
            text(c["label"], c["x"], c["y"] + KNOB_R + 12, 5.0, M["silk"], font=FONT)
            for i in range(11):
                a = math.radians(-135 + 27 * i); rr = KNOB_R + 6.5
                box("tick", c["x"] + rr * math.sin(a), c["y"] - rr * math.cos(a), 0.05, 0.6, 3.6 if i % 5 == 0 else 2.2, 0.1, M["silk"], rot=(0, 0, -a))
                if i % 5 == 0:
                    rn = KNOB_R + 12.5
                    text(str(i), c["x"] + rn * math.sin(a), c["y"] - rn * math.cos(a), 3.4, M["silk"], font=FONT_M)
        elif c["kind"] == "switch3":
            for i, t in enumerate(("OFF", "AUTO", "MAN")): text(t, c["x"] + 12, c["y"] - 10 + 10 * i, 4.0, M["silk"], "LEFT", font=FONT)
    ny = (NAME_Y0 + NAME_Y1) / 2
    text("AT330", 28, ny, 18.0, M["silk"], "LEFT", z=1.2)
    text("STRINGS  ·  CHOIR  ·  VOCODER", 108, ny, 6.5, M["accent"], "LEFT", font=FONT, z=1.2)
    text("Audio Tools", PW - 28, ny, 8.0, M["silk"], "RIGHT", font=FONT_M, z=1.2)

def build_keys(M, y0, z0, kw, x0=0.0, down=False, length=150.0):
    wi = 0; made = []
    for n in range(49):
        pc = n % 12
        dz = -2.2 if down else 0.0
        if pc in WHITES:
            made.append((36 + n, box("w", x0 + kw * (wi + 0.5), y0 + length / 2, z0 + dz, kw - 1.0, length, 18, M["ivory"], bevel=1.2))); wi += 1
        else:
            made.append((36 + n, box("b", x0 + kw * wi, y0 + length * 0.32, z0 + 10 + dz, kw * 0.58, length * 0.64, 18, M["ebony"], bevel=1.4)))
    box("keybed", x0 + kw * wi / 2, y0 + length / 2 + 6, z0 - 12, kw * wi + 8, length + 24, 8, M["trim"])
    return made

def lights(sc, rim=False):
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.05, 0.045, 0.04, 1); bg.inputs["Strength"].default_value = 0.25
    specs = [("key", (-0.3, 0.6, 1.4), 13, (1.0, 0.86, 0.7), 1.4), ("fill", (1.4, -0.2, 1.0), 4, (0.8, 0.85, 1.0), 1.0)]
    if rim: specs.append(("rim", (0.5, 0.9, 0.35), 18, (0.8, 0.88, 1.0), 0.8))
    for name, loc, power, col, size in specs:
        ld = bpy.data.lights.new(name, "AREA"); ld.energy = power; ld.color = col; ld.size = size
        o = bpy.data.objects.new(name, ld); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (Vector((PW / 2 * MM, -PH / 2 * MM, 0)) - o.location).to_track_quat("-Z", "Y").to_euler()

def ortho(sc, cx, cy, width_mm, z=2.0):
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"; cd.ortho_scale = width_mm * MM
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); cam.location = bl(cx, cy, z * 1000); sc.camera = cam

# ── Modo ui ──
VIEW_H = 880.0 / PX                  # el alto del encuadre (la imagen mide 1600 x 880 a 1x)
UI_H_MM = VIEW_H
KEY_LEN = VY0 + VIEW_H - 14 - KEYS_Y - 6           # las teclas llegan hasta el listón del frente
def ui_scene(state, keys_down=False):
    sc = reset((WIN_W * SCALE, 880 * SCALE))
    M = materials(); build_panel(M, state)
    build_keys(M, KEYS_Y + 6, -2, KW_UI, down=keys_down, length=KEY_LEN)
    box("keyback", PW / 2, KEYS_Y + 3, -8, PW, 6, 12, M["panel"])
    for x in (VX0 / 2, PW - VX0 / 2): box("cheek", x, (VY0 + VY0 + VIEW_H) / 2, 12, -VX0 + 0.5, VIEW_H + 2, 40, M["cheek"], bevel=4)
    box("frontrail", PW / 2, VY0 + VIEW_H - 5, 4, PW, 12, 18, M["front"], bevel=2)
    lights(sc); ortho(sc, VX0 + VIEW_W / 2, VY0 + VIEW_H / 2, VIEW_W)
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.color_mode = "RGB"; sc.render.image_settings.quality = 90
    return sc

def sprite_scene(kind):
    """Una pieza sola sobre fondo transparente, con la misma luz y escala que el panel."""
    sizes = {"slider": (CAP_W + 6, CAP_H + 6), "knob": (2 * KNOB_R + 6, 2 * KNOB_R + 6), "lever": (11, 12)}
    wmm, hmm = sizes[kind]
    sc = reset((int(round(wmm * PX * SCALE)), int(round(hmm * PX * SCALE))))
    M = materials()
    if kind == "slider": slider_cap(M, 0, 0)
    elif kind == "knob": knob_cap(M, 0, 0)
    else: lever(M, 0, 0)
    lights(sc)
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"; cd.ortho_scale = max(wmm, hmm) * MM
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); cam.location = (0, 0, 2.0); sc.camera = cam
    sc.render.film_transparent = True
    return sc

def X(x): return round((x - VX0) * PX, 2)
def Y(y): return round(TOP + (y - VY0) * PX, 2)
def p1(x, y): return [X(x), Y(y)]          # mm del panel → px de la ventana a 1x

def write_layout():
    L = {"window": [WIN_W, 960], "top": TOP, "pxPerMm": PX, "scale": SCALE, "image": [0, TOP, WIN_W, 880],
         "sections": [], "controls": [], "keys": [], "bands": None,
         "nameboard": [X(0), Y(NAME_Y0), round(PW * PX, 2), round((NAME_Y1 - NAME_Y0) * PX, 2)]}
    for sid, title, x0, x1 in SECTIONS: L["sections"].append({"id": sid, "title": title, "x0": X(x0 - 3), "x1": X(x1 + 3), "y": Y(22)})
    for c in CONTROLS:
        e = {"id": c["id"], "kind": c["kind"], "label": c["label"], "sub": c["sub"], "c": p1(c["x"], c["y"])}
        if c["kind"] == "slider": e.update(top=Y(c["y"] - TRAVEL / 2), bottom=Y(c["y"] + TRAVEL / 2), cap=[CAP_W * PX, CAP_H * PX])
        elif c["kind"] == "tablet": e.update(rect=[X(c["x"] - TAB_W / 2 - 2), Y(c["y"] - TAB_H / 2 - 2), round((TAB_W + 4) * PX, 2), round((TAB_H + 4) * PX, 2)])
        elif c["kind"] == "knob": e.update(r=round(KNOB_R * PX, 2))
        elif c["kind"] == "switch3": e.update(step=round(10 * PX, 2))
        L["controls"].append(e)
    B = BANDS
    L["bands"] = [X(B["x0"]), Y(B["y0"]), round((B["x1"] - B["x0"]) * PX, 2), round((B["y1"] - B["y0"]) * PX, 2)]
    # Teclas: C2..C6 (MIDI 36..84); las blancas como polígono (la parte de abajo entera y la de arriba entre negras).
    y0, yb, y1 = Y(KEYS_Y + 6), Y(KEYS_Y + 6 + KEY_LEN * 0.64), Y(KEYS_Y + 6 + KEY_LEN)
    wi = 0; bw = KW_UI * 0.58
    for n in range(49):
        pc = n % 12
        if pc in WHITES:
            x0, x1 = wi * KW_UI, (wi + 1) * KW_UI
            l = x0 + (bw / 2 if pc in (2, 4, 7, 9, 11) else 0); r = x1 - (bw / 2 if pc in (0, 2, 5, 7, 9) and n < 48 else 0)
            poly = [[X(l), y0], [X(r), y0], [X(r), yb], [X(x1), yb], [X(x1), y1], [X(x0), y1], [X(x0), yb], [X(l), yb]]
            L["keys"].append({"note": 36 + n, "black": False, "poly": poly}); wi += 1
        else:
            x = wi * KW_UI
            L["keys"].append({"note": 36 + n, "black": True, "poly": [[X(x - bw / 2), y0], [X(x + bw / 2), y0], [X(x + bw / 2), yb], [X(x - bw / 2), yb]]})
    os.makedirs(OUT, exist_ok=True)
    json.dump(L, open(os.path.join(OUT, "layout.json"), "w"), indent=1)

def mode_ui():
    write_layout()
    only = arg("--only", "all")
    jobs = [("bg", lambda: ui_scene("off")), ("on", lambda: ui_scene("on")), ("keysdown", lambda: ui_scene("off", keys_down=True))]
    for name, make in jobs:
        if only in ("all", name):
            sc = make(); sc.render.filepath = os.path.join(OUT, name + ".jpg"); bpy.ops.render.render(write_still=True)
    for kind in ("knob", "lever"):
        if only in ("all", "sprites", kind):
            sc = sprite_scene(kind); sc.render.filepath = os.path.join(OUT, kind + ".png"); bpy.ops.render.render(write_still=True)

# ── Fotos ──
REAL_KW = 23.5
def photo_scene(res):
    sc = reset(res)
    M = materials(); build_panel(M, "photo")
    kx0 = PW - 29 * REAL_KW - 12
    M["_keys"] = build_keys(M, PH + 8, -10, REAL_KW, x0=kx0)
    # El frente de nogal, el bloque de la izquierda (una placa más, con tornillos) y las mejillas de nogal.
    box("front", PW / 2, PH + 175, -18, PW, 30, 32, M["front"], bevel=3)
    box("leftblock", kx0 / 2, PH + 85, -10, kx0 - 4, 170, 20, M["base"], bevel=1.5)
    box("leftplate", kx0 / 2, PH + 85, 0.5, kx0 - 16, 156, 3, M["plate"], bevel=1.4)
    for sx in (12, kx0 - 12):
        for sy in (PH + 13, PH + 157):
            cyl("screw", sx, sy, 2.3, 2.2, 1.2, M["screw"], verts=32, bevel=0.4)
    for x in (-16, PW + 16): box("cheek", x, (PH + 190 - 34) / 2, 2, 32, PH + 224, 70, M["cheek"], bevel=5)
    for i, t in enumerate(("STRINGS 4'", "CHOIR 8' 4'", "VOCODER 10 BANDS", "ENSEMBLE 4 x BBD")):
        text(t, 26, PH + 44 + 26 * i, 9.0, M["silk"], "LEFT", z=2.1, font=FONT)
    bpy.ops.mesh.primitive_plane_add(size=8, location=(0.5, -0.4, -0.036)); bpy.context.active_object.data.materials.append(M["floor"])
    lights(sc, rim=True)
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.color_mode = "RGB"; sc.render.image_settings.quality = 92
    return sc, M

PSHOTS = {   # posición, mira (m), lente, f, resolución
    "hero": ((PW * 0.30 * MM, -(PH + 1050) * MM, 0.95), (PW * 0.52 * MM, -(PH * 0.62) * MM, -0.02), 42, 8.0, (2400, 1350)),
    "close": ((0.58 * PW * MM, -0.46, 0.20), (0.76 * PW * MM, -(YA + 45) * MM, 0.004), 40, 5.6, (1600, 1200)),
    "top": ((PW / 2 * MM, -255 * MM, 1.45), (PW / 2 * MM, -255 * MM, 0.0), 40, 16.0, (2000, 1300)),
}
def pcamera(sc, loc, look, lens, fstop):
    cd = bpy.data.cameras.new("pcam"); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    cam = bpy.data.objects.new("pcam", cd); sc.collection.objects.link(cam); sc.camera = cam
    cam.location = Vector(loc); d = Vector(look) - Vector(loc)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); cd.dof.focus_distance = d.length
    return cam

def mode_photos():
    for name in arg("--shots", "hero,close,top").split(","):
        loc, look, lens, fstop, res = PSHOTS[name]
        sc, M = photo_scene(res); pcamera(sc, loc, look, lens, fstop)
        sc.render.filepath = os.path.join(OUT, name + ".jpg"); bpy.ops.render.render(write_still=True)


# ── Film de la web (scripts/film.py y tools/film.json) ──
# Seis tomas a 64 BPM sobre "Rain on Neon" tocando Fa menor, Re bemol, La bemol y Mi bemol (un acorde cada 8 tiempos, como
# at330render --chords "Fm Db Ab Eb" --beats-per-chord 8). Las teclas bajan con los acordes, las llaves del coro se
# prenden a tiempo y el visor de bandas sigue el audio (--audio: el mismo render que suena en el film).
#   blender -b --factory-startup -P at330.py -- --mode film2 --audio <wav> --out <dir> [--shot a,b] [--quality draft|preview|final]
FILM_BPM, FPS = 64.0, 24
BEAT = 60.0 / FILM_BPM
FILM_SHOTS = [("open", 6), ("tablets", 4), ("bands", 4), ("keys", 4), ("hero", 8), ("end", 6)]
FILM_CHORDS = [(5, True), (1, False), (8, False), (3, False)]           # Fm Db Ab Eb (raíz, menor)

def shot_table():
    out, f0 = [], 0
    for name, beats in FILM_SHOTS:
        n = int(round(beats * BEAT * FPS)); out.append((name, f0, n)); f0 += n
    return out

def chord_notes(t):
    """Las notas que suenan en el segundo t (mismo voicing que at330render --chords)."""
    k = int(t / (8 * BEAT)) % len(FILM_CHORDS)
    root, minor = FILM_CHORDS[k]
    r = 57 + (root + 3) % 12
    return {r, r + (3 if minor else 4), r + 7, 45 + (root + 3) % 12}

def band_levels(path):
    """Por cuadro, el nivel (0..12 segmentos) de cada una de las diez bandas del vocoder en el audio del film."""
    import wave, numpy as np
    w = wave.open(path); sr = w.getframerate(); ch = w.getnchannels(); sw = w.getsampwidth(); raw = w.readframes(w.getnframes()); w.close()
    if sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        x = (b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)); x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    else: x = np.frombuffer(raw, dtype="<i2") / 32768.0
    x = x.reshape(-1, ch).mean(axis=1)
    hz = [200, 280, 400, 600, 900, 1300, 2000, 2800, 4000, 6000]
    n, hop = 4096, int(sr / FPS); win = np.hanning(n); f = np.fft.rfftfreq(n, 1 / sr)
    frames = []
    for i in range(0, max(1, len(x) - n), hop):
        S = np.abs(np.fft.rfft(x[i:i + n] * win)) ** 2
        frames.append([10 * np.log10(S[(f > c / 1.19) & (f < c * 1.19)].sum() + 1e-12) for c in hz])
    a = np.array(frames); top = a.max(axis=0)
    top = np.percentile(a, 98, axis=0)
    lv = np.clip((a - (top - 24.0)) / 24.0 * 12.0, 0, 12)
    for i in range(1, len(lv)): lv[i] = np.maximum(lv[i], lv[i - 1] - 1.2)       # caída como un medidor
    return lv

def film2():
    sys.path.insert(0, os.path.join(REPO, "shared", "film"))
    import filmkit as fk
    out = os.path.abspath(arg("--out", "film")); only = arg("--shot"); quality = arg("--quality", "final")
    levels = band_levels(arg("--audio")) if arg("--audio") else None
    def V(*a): return Vector(a[0] if len(a) == 1 else a)
    for (name, f0, frames) in shot_table():
        if only and name not in only.split(","): continue
        sc, M = photo_scene((1920, 1080))
        fk.setup(sc, quality)
        cam = fk.camera(sc)
        keys = M["_keys"]
        # Llaves del coro y del vocoder en sus dos estados (se cambia cuál se ve) y los segmentos encendidos del visor.
        tabs = []
        for c in CONTROLS:
            if c["kind"] == "tablet" and c["sec"] in ("choir", "voc"):
                for o in [o for o in sc.objects if o.name.startswith("tab") and abs(o.location.x - c["x"] * MM) < 1e-4]:
                    bpy.data.objects.remove(o, do_unlink=True)
                bpy.ops.object.select_all(action="DESELECT")
                on = box("tabon", c["x"], c["y"], 5, TAB_W, TAB_H, 9, tablet_mat(c["color"], True), bevel=2.5, rot=(math.radians(-6), 0, 0))
                off = box("taboff", c["x"], c["y"], 5, TAB_W, TAB_H, 9, tablet_mat(c["color"], False), bevel=2.5, rot=(math.radians(6), 0, 0))
                tabs.append((c, on, off))
        segs = []
        B = BANDS; w = (B["x1"] - B["x0"]) / 10
        for o in [o for o in sc.objects if o.name.startswith("seg")]: bpy.data.objects.remove(o, do_unlink=True)
        for i in range(10):
            for k in range(12):
                yy = B["y1"] - 3 - k * (B["y1"] - B["y0"] - 6) / 12
                box("segoff", B["x0"] + w * (i + 0.5), yy, 0.5, w * 0.62, 3.2, 0.4, M["bar_off"])
                segs.append((i, k, box("segon", B["x0"] + w * (i + 0.5), yy, 0.55, w * 0.62, 3.2, 0.42, M["bar_on"])))
        P = lambda x, y, z: V(x, y, z)
        if name == "open":
            path = fk.Path([(0, P(0.08, -0.56, 0.10), P(0.12, -0.323, 0.0), 50, 2.8), (1, P(0.30, -0.62, 0.17), P(0.30, -0.30, 0.0), 50, 2.8)])
        elif name == "tablets":
            path = fk.Path([(0, P(0.44, -0.30, 0.10), P(0.48, -0.10, 0.0), 60, 3.2), (1, P(0.70, -0.30, 0.11), P(0.66, -0.10, 0.0), 60, 3.2)])
        elif name == "bands":
            path = fk.Path([(0, P(0.78, -0.36, 0.16), P(0.895, -0.095, 0.0), 60, 4.0), (1, P(0.86, -0.32, 0.14), P(0.895, -0.095, 0.0), 62, 4.0)])
        elif name == "keys":
            path = fk.Path([(0, P(0.30, -0.66, 0.06), P(0.70, -0.43, -0.01), 45, 3.5), (1, P(0.58, -0.68, 0.07), P(0.86, -0.43, -0.01), 45, 3.5)])
        elif name == "hero":
            path = fk.Path([(0, P(0.10, -1.30, 0.75), P(0.52, -0.26, -0.02), 40, 8.0), (0.5, P(0.52, -1.45, 0.95), P(0.52, -0.26, -0.02), 40, 8.0),
                            (1, P(0.98, -1.28, 0.80), P(0.52, -0.26, -0.02), 40, 8.0)])
        else:
            path = fk.Path([(0, P(0.45, -1.20, 0.90), P(0.52, -0.25, 0.0), 40, 8.0), (1, P(0.45, -1.95, 1.60), P(0.52, -0.25, 0.0), 40, 8.0)], easing="out")
        rest = {o.name: o.location.z for _, o in keys}

        def pose(f, t, u, name=name, path=path, f0=f0):
            T = (f0 + f - 1) / FPS
            path.apply(cam, u, shake=0.0005 if name in ("open", "tablets", "bands", "keys") else 0.0, t=T)
            down = chord_notes(T)
            for note, o in keys:
                o.location.z = rest[o.name] - (2.2 * MM if note in down else 0.0)
            return [o for _, o in keys]

        def keys_(f, name=name, f0=f0, frames=frames):
            T = (f0 + f - 1) / FPS; u = (f - 1) / max(1, frames - 1)
            if name == "open":
                for o in sc.objects:
                    if o.type == "LIGHT":
                        if "e0" not in o: o["e0"] = o.data.energy
                        o.data.energy = o["e0"] * fk.rack(u, 0.0, 0.8, 0.08, 1.0); o.data.keyframe_insert("energy")
            # Las llaves del coro se prenden de a una, a tiempo, en la toma "tablets"; antes, apagadas; después, el estado del preset.
            for j, (c, on, off) in enumerate(tabs):
                lit = c["id"] in PHOTO["on"]
                if name == "open": lit = False
                if name == "tablets": lit = (f - 1) / FPS > (j + 1) * BEAT * 0.45 and c["id"] in PHOTO["on"] | {"ch_male8_up", "ch_male8_lo"}
                on.hide_render = not lit; off.hide_render = lit
                on.keyframe_insert("hide_render"); off.keyframe_insert("hide_render")
            fi = min(int(T * FPS), len(levels) - 1) if levels is not None else 0
            for i, k, o in segs:
                o.hide_render = levels is None or not (k < levels[fi][i]) or name == "open"
                o.keyframe_insert("hide_render")
        fk.render_shot(sc, name, frames, pose, out, extra_keys=keys_)

if MODE == "ui": mode_ui()
elif MODE == "photos": mode_photos()
elif MODE == "film2": film2()
else: raise SystemExit("modo desconocido: " + MODE)
