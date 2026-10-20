"""El grabador de carretes de Tarnish (Blender 5.2, Cycles), para las fotos de la web y el film. Genérico: ninguna
forma ni marca de un fabricante.

Una máquina de estudio de dos pistas acostada en su mueble: placa de transporte de aluminio cepillado entre dos
laterales de nogal, dos carretes de 10,5" de aluminio torneado (tres rayos con ventanas redondeadas, cubo con su
traba de tres orejas), las bobinas de cinta marrón (la que entrega llena, la que recibe por la mitad), brazos de
tensión con rodillos, guías con pestañas, el bloque de tres cabezas (borrado, grabación, reproducción) con la cinta
enhebrada por su cara, el cabrestante y el rodillo de presión de goma. Adelante, las teclas del transporte con sus
pilotos y, en el frente inclinado, dos VU de cara crema iluminada con aguja, cuatro perillas moleteadas y una llave.
Estudio oscuro: luz principal suave arriba a la izquierda, relleno frío, contraluz y una tira que dibuja el metal.

Corre en pc50-wsl, siempre bajo el lock de GPU compartido:
  B=~/ai/blender/blender-5.2.2-linux-x64/blender; L=~/ai/prerendered/.gpu.lock
  flock $L $B -b --factory-startup -P deck.py -- --mode stills --out out [--shots hero,close,top] [--samples 256] [--pct 50]
  flock $L $B -b --factory-startup -P deck.py -- --mode film2 --quality draft|final --audio <wav> --out <dir> [--shot a,b]
Las fotos van a la web con plugins/tarnish/tools/web_images.py; el film, con scripts/film.py tarnish <dir>.
Pruebas de encuadre y luz sin esperar el lock: --cpu --pct 25 (y --step mid en film2: primer, medio y último cuadro de
cada toma); --shots cas,vu son encuadres de prueba de las casetes y del VU.
Unidades: 1 = 1 m. La placa mide 0,66 x 0,45 m; el frente mira hacia -y; los carretes giran en sentido antihorario
vistos desde arriba (como una máquina vertical vista de frente).
"""
import bpy, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "256"))
PCT = int(arg("--pct", "100"))
CPU = "--cpu" in argv                           # pruebas de aspecto chicas sin esperar el lock de la GPU
FPS = 24
IPS15 = 0.381                                   # m/s: 15 pulgadas por segundo

# ── Medidas (m) ──
RS, RT = Vector((-0.165, 0.10)), Vector((0.165, 0.10))   # centros de los carretes
REEL_R = 0.1335                                 # 10,5"
HUB_R = 0.0508                                  # cubo NAB de 4"
ZF0, ZF1 = 0.0100, 0.0228                       # cara inferior de la brida de abajo, cara superior de la de arriba
FT = 0.0030                                     # espesor de brida
ZT = 0.5 * (ZF0 + FT + ZF1 - FT)                # centro de la cinta (entre bridas)
TW = 0.00635                                    # cinta de 1/4"
YB = -0.112                                     # línea de la cinta frente a las cabezas
HEADS_X = (-0.066, -0.036, -0.006)              # borrado, grabación, reproducción
CAP_X, CAP_R, PINCH_R = 0.032, 0.0048, 0.0125
STATE = {"packS": 0.117, "packT": 0.083}
SLOPE_A = math.radians(35.8)                    # el frente inclinado: de (y -0,200, z 0) a (y -0,304, z -0,075)
SLOPE_C = Vector((0.0, -0.252, -0.0375))

def slope_pt(x, y, z=0.0):
    """Punto del marco del frente inclinado (x a lo ancho, y subiendo por la pendiente, z hacia afuera) en el mundo."""
    return SLOPE_C + Vector((x, y * math.cos(SLOPE_A) - z * math.sin(SLOPE_A), y * math.sin(SLOPE_A) + z * math.cos(SLOPE_A)))


# ── Escena y materiales ──
def reset():
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
    sc.cycles.device = "CPU" if CPU else "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    try: sc.cycles.denoising_use_gpu = not CPU
    except AttributeError: pass
    sc.render.resolution_percentage = PCT
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 94
    return sc

def principled(name):
    m = bpy.data.materials.new(name); m.use_nodes = True
    return m, m.node_tree, m.node_tree.nodes["Principled BSDF"]

def mat(name, base, rough, metal=0.0, coat=0.0, emit=None, strength=0.0, spec=0.5):
    m, nt, b = principled(name)
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    b.inputs["Specular IOR Level"].default_value = spec
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.08
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    return m

def _coord(nt, kind="Object"):
    return nt.nodes.new("ShaderNodeTexCoord").outputs[kind]

def _math(nt, op, a, b=None, val=None):
    n = nt.nodes.new("ShaderNodeMath"); n.operation = op
    nt.links.new(a, n.inputs[0])
    if b is not None: nt.links.new(b, n.inputs[1])
    elif val is not None: n.inputs[1].default_value = val
    return n.outputs[0]

def _ramp(nt, src, a, b, pa=0.0, pb=1.0):
    r = nt.nodes.new("ShaderNodeValToRGB")
    r.color_ramp.elements[0].position = pa; r.color_ramp.elements[1].position = pb
    r.color_ramp.elements[0].color = a if len(a) == 4 else (*a, 1); r.color_ramp.elements[1].color = b if len(b) == 4 else (*b, 1)
    nt.links.new(src, r.inputs["Fac"]); return r.outputs["Color"]

def _bump(nt, b, height, strength, dist=0.0002):
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = strength; bp.inputs["Distance"].default_value = dist
    nt.links.new(height, bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])

def brushed(name, base=(0.80, 0.80, 0.78), rough=(0.20, 0.32), aniso=0.75, scale=1400.0):
    """Aluminio cepillado en línea: anisotropía a lo largo de la u de la malla (los cubos traen UV) y rayas finas de
    rugosidad estiradas en la misma dirección."""
    m, nt, b = principled(name)
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Metallic"].default_value = 1.0
    b.inputs["Anisotropic"].default_value = aniso
    t = nt.nodes.new("ShaderNodeTangent"); t.direction_type = "UV_MAP"; t.uv_map = "UVMap"
    nt.links.new(t.outputs["Tangent"], b.inputs["Tangent"])
    mp = nt.nodes.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (6.0, scale, 1.0)
    nt.links.new(_coord(nt, "UV"), mp.inputs["Vector"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 1.0; nz.inputs["Detail"].default_value = 6.0
    nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
    r = _ramp(nt, nz.outputs["Fac"], (rough[0],) * 3, (rough[1],) * 3, 0.3, 0.7)
    nt.links.new(r, b.inputs["Roughness"])
    _bump(nt, b, nz.outputs["Fac"], 0.04)
    return m

def turned(name, base=(0.86, 0.85, 0.83), rough=0.22, rings=2600.0, bump=0.10):
    """Aluminio torneado: brillo anisótropo en círculos (tangente radial alrededor de z) y los anillos finos del
    torno en la rugosidad y el relieve. Se usa en bridas, cubos y perillas (objetos con el origen en su eje)."""
    m, nt, b = principled(name)
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Metallic"].default_value = 1.0
    b.inputs["Anisotropic"].default_value = 0.8
    t = nt.nodes.new("ShaderNodeTangent"); t.direction_type = "RADIAL"; t.axis = "Z"
    nt.links.new(t.outputs["Tangent"], b.inputs["Tangent"])
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(_coord(nt), sep.inputs[0])
    cxy = nt.nodes.new("ShaderNodeCombineXYZ"); nt.links.new(sep.outputs[0], cxy.inputs[0]); nt.links.new(sep.outputs[1], cxy.inputs[1])
    ln = nt.nodes.new("ShaderNodeVectorMath"); ln.operation = "LENGTH"; nt.links.new(cxy.outputs[0], ln.inputs[0])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.noise_dimensions = "1D"; nz.inputs["Scale"].default_value = 1.0
    nz.inputs["Detail"].default_value = 3.0
    nt.links.new(_math(nt, "MULTIPLY", ln.outputs["Value"], val=rings), nz.inputs["W"])
    nt.links.new(_ramp(nt, nz.outputs["Fac"], (rough * 0.7,) * 3, (rough * 1.5,) * 3, 0.3, 0.7), b.inputs["Roughness"])
    _bump(nt, b, nz.outputs["Fac"], bump)
    return m

def knurled(name):
    """Aluminio moleteado: estrías verticales finas alrededor del eje (gradiente radial → seno → relieve)."""
    m = turned(name, rough=0.22, bump=0.04)
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    g = nt.nodes.new("ShaderNodeTexGradient"); g.gradient_type = "RADIAL"; nt.links.new(_coord(nt), g.inputs["Vector"])
    s = _math(nt, "SINE", _math(nt, "MULTIPLY", g.outputs["Fac"], val=2 * math.pi * 90))
    _bump(nt, b, s, 0.35, 0.0004)
    return m

def walnut(name):
    """Nogal aceitado: vetas en bandas a lo largo del lateral (x local) torcidas con ruido, poros finos y una laca
    satinada."""
    m, nt, b = principled(name)
    co = _coord(nt)
    mp = nt.nodes.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (0.12, 1.0, 1.0); nt.links.new(co, mp.inputs["Vector"])
    w = nt.nodes.new("ShaderNodeTexWave"); w.wave_type = "BANDS"; w.bands_direction = "Y"
    w.inputs["Scale"].default_value = 18.0; w.inputs["Distortion"].default_value = 6.0; w.inputs["Detail"].default_value = 6.0
    w.inputs["Detail Scale"].default_value = 2.5
    nt.links.new(mp.outputs["Vector"], w.inputs["Vector"])
    col = _ramp(nt, w.outputs["Fac"], (0.045, 0.021, 0.010), (0.090, 0.046, 0.022), 0.2, 0.8)
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 900.0; nt.links.new(co, nz.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.blend_type = "MULTIPLY"; mix.inputs[0].default_value = 0.35
    nt.links.new(col, mix.inputs[6]); nt.links.new(nz.outputs["Color"], mix.inputs[7])
    nt.links.new(mix.outputs[2], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.42
    b.inputs["Coat Weight"].default_value = 0.6; b.inputs["Coat Roughness"].default_value = 0.22
    _bump(nt, b, nz.outputs["Fac"], 0.05)
    return m

def tape_material():
    """Cinta: la cara de óxido (la de adentro, la que toca las cabezas) marrón satinada; la de afuera, base de
    poliéster más oscura y brillante. Rayas longitudinales y motas que corren con la cinta: el mapeo 'scroll' se
    desplaza con el avance (u = metros de cinta x 10)."""
    m, nt, b = principled("tape")
    uv = _coord(nt, "UV")
    mp = nt.nodes.new("ShaderNodeMapping"); mp.name = "scroll"; nt.links.new(uv, mp.inputs["Vector"])
    st = nt.nodes.new("ShaderNodeMapping"); st.inputs["Scale"].default_value = (1.5, 90.0, 1.0); nt.links.new(mp.outputs["Vector"], st.inputs["Vector"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 2.0; nz.inputs["Detail"].default_value = 3.0
    nt.links.new(st.outputs["Vector"], nz.inputs["Vector"])
    vo = nt.nodes.new("ShaderNodeTexVoronoi"); vo.inputs["Scale"].default_value = 60.0; nt.links.new(mp.outputs["Vector"], vo.inputs["Vector"])
    spots = _ramp(nt, vo.outputs["Distance"], (1, 1, 1), (0, 0, 0), 0.0, 0.05)
    oxide = _ramp(nt, nz.outputs["Fac"], (0.050, 0.027, 0.014), (0.105, 0.060, 0.030), 0.35, 0.7)
    mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.inputs[7].default_value = (0.20, 0.12, 0.06, 1)
    nt.links.new(_math(nt, "MULTIPLY", spots, val=0.6), mx.inputs[0]); nt.links.new(oxide, mx.inputs[6])
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    side = nt.nodes.new("ShaderNodeMix"); side.data_type = "RGBA"; side.inputs[6].default_value = (0.030, 0.020, 0.013, 1)
    nt.links.new(geo.outputs["Backfacing"], side.inputs[0]); nt.links.new(mx.outputs[2], side.inputs[7])
    nt.links.new(side.outputs[2], b.inputs["Base Color"])
    rr = nt.nodes.new("ShaderNodeMix"); rr.data_type = "FLOAT"; rr.inputs[2].default_value = 0.22; rr.inputs[3].default_value = 0.42
    nt.links.new(geo.outputs["Backfacing"], rr.inputs[0]); nt.links.new(rr.outputs[0], b.inputs["Roughness"])
    return m

def pack_material():
    """La bobina vista de canto y por las ventanas: espiras finas que brillan distinto (ruido en función del radio)
    y un poco de anisotropía circular."""
    m, nt, b = principled("pack")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(_coord(nt), sep.inputs[0])
    cxy = nt.nodes.new("ShaderNodeCombineXYZ"); nt.links.new(sep.outputs[0], cxy.inputs[0]); nt.links.new(sep.outputs[1], cxy.inputs[1])
    ln = nt.nodes.new("ShaderNodeVectorMath"); ln.operation = "LENGTH"; nt.links.new(cxy.outputs[0], ln.inputs[0])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.noise_dimensions = "1D"; nz.inputs["Scale"].default_value = 1.0; nz.inputs["Detail"].default_value = 4.0
    nt.links.new(_math(nt, "MULTIPLY", ln.outputs["Value"], val=900.0), nz.inputs["W"])
    nt.links.new(_ramp(nt, nz.outputs["Fac"], (0.022, 0.013, 0.008), (0.050, 0.029, 0.016), 0.3, 0.7), b.inputs["Base Color"])
    nt.links.new(_ramp(nt, nz.outputs["Fac"], (0.16,) * 3, (0.34,) * 3, 0.3, 0.7), b.inputs["Roughness"])
    b.inputs["Anisotropic"].default_value = 0.6
    t = nt.nodes.new("ShaderNodeTangent"); t.direction_type = "RADIAL"; t.axis = "Z"; nt.links.new(t.outputs["Tangent"], b.inputs["Tangent"])
    return m

def glass_material(name="glass"):
    """Vidrio de instrumento sin refracción (deja pasar las sombras): transparente + reflejo dieléctrico."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; N = nt.nodes
    out = N["Material Output"]; b = N["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0, 0, 0, 1); b.inputs["Roughness"].default_value = 0.03
    b.inputs["Specular IOR Level"].default_value = 0.35
    tr = N.new("ShaderNodeBsdfTransparent"); tr.inputs[0].default_value = (0.96, 0.96, 0.95, 1)
    add = N.new("ShaderNodeAddShader"); nt.links.new(tr.outputs[0], add.inputs[0]); nt.links.new(b.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], out.inputs["Surface"])
    return m

def floor_material():
    m, nt, b = principled("floor")
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 3.0; nz.inputs["Detail"].default_value = 8.0
    nt.links.new(_coord(nt), nz.inputs["Vector"])
    nt.links.new(_ramp(nt, nz.outputs["Fac"], (0.008, 0.008, 0.0085), (0.014, 0.0135, 0.0135), 0.3, 0.7), b.inputs["Base Color"])
    nt.links.new(_ramp(nt, nz.outputs["Fac"], (0.28,) * 3, (0.45,) * 3, 0.3, 0.7), b.inputs["Roughness"])
    return m


# ── Geometría ──
def link(o, sc=None):
    (sc or bpy.context.scene).collection.objects.link(o); return o

def add(o, m):
    o.data.materials.append(m); return o

def finish(o, bevel=0.0, seg=3, angle=35):
    """Bisela las aristas vivas, aplica y suaviza por ángulo: metal con cantos que toman la luz."""
    bpy.context.view_layer.objects.active = o
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True)
    if bevel:
        md = o.modifiers.new("b", "BEVEL"); md.width = bevel; md.segments = seg; md.limit_method = "ANGLE"
        md.angle_limit = math.radians(30)
        bpy.ops.object.modifier_apply(modifier=md.name)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle))
    return o

def cyl(r, h, loc, m, verts=96, bevel=0.0, rot=(0, 0, 0), seg=3):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=h, location=loc, rotation=rot)
    return add(finish(bpy.context.active_object, bevel, seg), m)

def box(size, loc, m, bevel=0.0, rot=(0, 0, 0), seg=3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot); o = bpy.context.active_object
    o.scale = size; bpy.ops.object.transform_apply(scale=True)
    return add(finish(o, bevel, seg), m)

def sphere(r, loc, m, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=r, location=loc); o = bpy.context.active_object
    o.scale = scale; bpy.ops.object.transform_apply(scale=True); bpy.ops.object.shade_smooth()
    return add(o, m)

def shape(name, loops, depth, m, bevel=0.0, loc=(0, 0, 0), res=3):
    """Placa plana a partir de contornos 2D (el primero es el borde, los demás agujeros: relleno par-impar),
    extruida ±depth en z y con bisel redondo. Se convierte a malla para suavizar por ángulo."""
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "2D"; cu.fill_mode = "BOTH"
    cu.extrude = depth; cu.bevel_depth = bevel; cu.bevel_resolution = res
    for pts in loops:
        sp = cu.splines.new("POLY"); sp.points.add(len(pts) - 1); sp.use_cyclic_u = True
        for p, c in zip(sp.points, pts): p.co = (c[0], c[1], 0.0, 1.0)
    o = link(bpy.data.objects.new(name, cu)); o.location = loc
    for x in bpy.context.selected_objects: x.select_set(False)
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.convert(target="MESH")
    o = bpy.context.active_object
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    return add(o, m)

def circle_pts(r, n=128, c=(0, 0), a0=0.0, a1=2 * math.pi):
    return [(c[0] + r * math.cos(a0 + (a1 - a0) * i / n), c[1] + r * math.sin(a0 + (a1 - a0) * i / n)) for i in range(n)]

def rrect(w, h, r, n=8, c=(0, 0)):
    """Rectángulo de esquinas redondeadas (centro c)."""
    pts = []
    for (cx, cy, a0) in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n); pts.append((c[0] + cx + r * math.cos(a), c[1] + cy + r * math.sin(a)))
    return pts

def window_pts(a_mid, span, r1, r2, half_spoke, rho, n=14):
    """Ventana de brida: entre los radios r1 y r2, limitada por dos rayos de ancho constante (bordes paralelos al eje
    de cada rayo, a half_spoke de él) y con las cuatro esquinas redondeadas con radio rho."""
    def rot(p, a): return (p[0] * math.cos(a) - p[1] * math.sin(a), p[0] * math.sin(a) + p[1] * math.cos(a))
    def fillet(R, outer, side):
        # Marco del rayo: eje del rayo en +x, la ventana del lado y > half_spoke (side=+1) o y < -half_spoke (side=-1).
        cy = half_spoke + rho
        rc = R - rho if outer else R + rho
        cx = math.sqrt(max(1e-12, rc * rc - cy * cy))
        c = (cx, side * cy)
        p_line = (cx, side * half_spoke)
        p_circ = (c[0] * R / rc, c[1] * R / rc)
        a_line, a_circ = math.atan2(p_line[1] - c[1], p_line[0] - c[0]), math.atan2(p_circ[1] - c[1], p_circ[0] - c[0])
        return c, a_line, a_circ
    pts = []
    sa, sb = a_mid - span / 2, a_mid + span / 2          # ejes de los dos rayos que limitan la ventana
    # Esquina rayo A (side +1 respecto de A) / círculo interior → borde de A → esquina A / exterior → arco exterior
    # → esquina B / exterior → borde de B → esquina B / interior → arco interior (de vuelta).
    corners = []
    for (ax, side, R, outer) in ((sa, +1, r1, False), (sa, +1, r2, True), (sb, -1, r2, True), (sb, -1, r1, False)):
        c, al, ac = fillet(R, outer, side)
        corners.append((ax, c, al, ac, R, outer))
    def arc(c, a0, a1, ax, k=n):
        d = (a1 - a0 + math.pi) % (2 * math.pi) - math.pi
        return [rot((c[0] + rho * math.cos(a0 + d * i / k), c[1] + rho * math.sin(a0 + d * i / k)), ax) for i in range(k + 1)]
    # A/interior: de la tangente con el círculo interior a la tangente con el borde del rayo.
    ax, c, al, ac, R, _ = corners[0]; pts += arc(c, ac, al, ax)
    ax, c, al, ac, R, _ = corners[1]; pts += arc(c, al, ac, ax)
    pa = math.atan2(pts[-1][1], pts[-1][0])
    ax, c, al, ac, R, _ = corners[2]; q = arc(c, ac, al, ax); pb = math.atan2(q[0][1], q[0][0])
    m = 48
    d = (pb - pa) % (2 * math.pi)
    pts += [(r2 * math.cos(pa + d * i / m), r2 * math.sin(pa + d * i / m)) for i in range(1, m)]
    pts += q
    ax, c, al, ac, R, _ = corners[3]; q = arc(c, al, ac, ax)
    pts += q
    pa = math.atan2(q[-1][1], q[-1][0]); pb = math.atan2(pts[0][1], pts[0][0])
    d = (pa - pb) % (2 * math.pi)
    pts += [(r1 * math.cos(pa - d * i / m), r1 * math.sin(pa - d * i / m)) for i in range(1, m)]
    return pts

def reel(name, c, pack_r, M):
    """Un carrete NAB de 10,5": dos bridas de aluminio torneado con tres rayos, la bobina, el cubo, el plato del
    motor abajo y la traba de tres orejas arriba. Todo hijo de un vacío en el eje (se gira ese)."""
    parent = link(bpy.data.objects.new(name, None)); parent.location = (c.x, c.y, 0.0)
    objs = []
    loops = [circle_pts(REEL_R, 256)]
    for k in range(3):
        loops.append(window_pts(math.radians(90 + 120 * k), math.radians(120), 0.060, 0.1185, 0.0115, 0.0075))
    loops.append(list(reversed(circle_pts(0.0381, 96))))
    for z in (ZF0 + FT / 2, ZF1 - FT / 2):
        f = shape(name + "_flange", loops, FT / 2 - 0.0004, M["reel"], bevel=0.0004, loc=(0, 0, z)); objs.append(f)
    objs.append(cyl(pack_r, TW, (0, 0, ZT), M["pack"], verts=192))
    objs.append(cyl(HUB_R, ZF1 - ZF0 - 2 * FT + 0.0002, (0, 0, ZT), M["hub"], verts=96))
    objs.append(cyl(0.056, 0.008, (0, 0, ZF0 - 0.0045), M["anod"], verts=96, bevel=0.0008))          # plato
    objs.append(cyl(0.0379, ZF1 - ZF0 + 0.004, (0, 0, (ZF0 + ZF1) / 2 + 0.001), M["anod"], verts=96, bevel=0.0006))
    objs.append(cyl(0.020, 0.006, (0, 0, ZF1 + 0.005), M["knob"], verts=96, bevel=0.0012))           # tapa de la traba
    objs.append(cyl(0.0075, 0.003, (0, 0, ZF1 + 0.0085), M["anod"], verts=48, bevel=0.0006))
    for k in range(3):                                                                              # orejas
        a = math.radians(30 + 120 * k)
        e = box((0.022, 0.009, 0.0026), (math.cos(a) * 0.026, math.sin(a) * 0.026, ZF1 + 0.0028), M["knob"], bevel=0.0009,
                rot=(0, 0, a))
        objs.append(e)
    for o in objs: o.parent = parent
    return parent

def ribbon(name, pts, z0, z1, m):
    """Cinta como cinta de malla vertical a lo largo de pts (2D) con UV u = metros x 10, v = ancho."""
    verts, faces, us = [], [], [0.0]
    for i in range(1, len(pts)): us.append(us[-1] + (Vector(pts[i]) - Vector(pts[i - 1])).length)
    for p in pts: verts += [(p[0], p[1], z0), (p[0], p[1], z1)]
    for i in range(len(pts) - 1): faces.append((2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1))
    me = bpy.data.meshes.new(name); me.from_pydata(verts, [], faces); me.update()
    uvl = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            vi = me.loops[li].vertex_index
            uvl.data[li].uv = (us[vi // 2] * 10.0, float(vi % 2))
    for poly in me.polygons: poly.use_smooth = True
    o = link(bpy.data.objects.new(name, me)); me.materials.append(m)
    return o

def belt(circles):
    """Recorrido de la cinta: círculos (centro, radio, sentido) con s=+1 si la cinta los rodea en sentido
    antihorario. Devuelve la polilínea 2D (tangentes exteriores/interiores y arcos), del primero al último."""
    tang = []
    for (c1, r1, s1), (c2, r2, s2) in zip(circles[:-1], circles[1:]):
        D = c2 - c1; k = r1 * s1 - r2 * s2
        phi = math.atan2(D.y, D.x) + math.asin(max(-1.0, min(1.0, k / D.length)))
        d = Vector((math.cos(phi), math.sin(phi))); N = Vector((d.y, -d.x))
        tang.append((c1 + N * (r1 * s1), c2 + N * (r2 * s2)))
    pts = [tang[0][0]]
    for i in range(1, len(circles) - 1):
        c, r, s = circles[i]
        a_in, a_out = math.atan2(*(tang[i - 1][1] - c).yx), math.atan2(*(tang[i][0] - c).yx)
        sweep = (a_out - a_in) % (2 * math.pi) if s > 0 else -((a_in - a_out) % (2 * math.pi))
        if abs(sweep) > 1.9 * math.pi: sweep = 0.0
        n = max(2, int(abs(sweep) / math.radians(3)))
        pts += [c + Vector((math.cos(a_in + sweep * j / n), math.sin(a_in + sweep * j / n))) * r for j in range(n + 1)]
    pts.append(tang[-1][1])
    out = [pts[0]]
    for p in pts[1:]:
        if (p - out[-1]).length > 1e-5: out.append(p)
    return out

def head(x, M, kind):
    """Una cabeza: cuerpo de mu-metal en D (frente redondeado hacia la cinta), los polos de las dos pistas con su
    entrehierro, la base con dos tornillos y los cables por detrás."""
    rf = 0.016 if kind else 0.013                 # radio del frente
    cy = YB - 0.0012 + rf
    hw = 0.0078 if kind else 0.0085
    a = math.asin(hw / rf)
    front = [(x + rf * math.cos(-math.pi / 2 - a + 2 * a * i / 24), cy + rf * math.sin(-math.pi / 2 - a + 2 * a * i / 24)) for i in range(25)]
    back = cy + 0.0035
    o = shape("head", [front + [(x + hw, back), (x - hw, back)]], 0.0105, M["head"], bevel=0.0009, loc=(0, 0, 0.0155))
    # Tapa superior negra (resina) con dos tornillos de ajuste de azimut.
    shape("headtop", [[(px, py - 0.0006 if py < cy else py - 0.0006) for (px, py) in [(x + rf * 0.92 * math.cos(-math.pi / 2 - a + 2 * a * i / 24), cy + rf * 0.92 * math.sin(-math.pi / 2 - a + 2 * a * i / 24)) for i in range(25)] + [(x + hw - 0.0008, back - 0.0008), (x - hw + 0.0008, back - 0.0008)]]],
          0.0004, M["anod"], bevel=0.0003, loc=(0, 0, 0.0275))
    for dx in (-0.0042, 0.0042):
        cyl(0.0011, 0.0008, (x + dx, cy - 0.0005, 0.0283), M["screw"], verts=24, bevel=0.0003)
    yf = YB - 0.0012
    for dz in (-0.0017, 0.0017):                                  # polos de las dos pistas
        box((0.0013, 0.0008, 0.0021), (x, yf + 0.0005, ZT + dz), M["pole"], bevel=0.0001)
        box((0.00016, 0.0006, 0.0021), (x, yf + 0.0004, ZT + dz), M["gap"])
    if kind == 0:                                                 # borrado: una ranura ancha
        box((0.0032, 0.0006, 0.0055), (x, yf + 0.0005, ZT), M["pole"], bevel=0.0001)
    box((0.020, 0.022, 0.0042), (x, cy - 0.003, 0.0021), M["anod"], bevel=0.0007)
    for dx in (-0.0068, 0.0068):
        cyl(0.0016, 0.0012, (x + dx, cy + 0.0045, 0.0048), M["screw"], verts=24, bevel=0.0004)
    cols = (M["wire_r"], M["wire_w"], M["wire_g"])
    for i, cm in enumerate(cols):
        dx = (i - 1) * 0.0022
        cu = bpy.data.curves.new("wire", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = 0.00045; cu.bevel_resolution = 2
        sp = cu.splines.new("BEZIER"); sp.bezier_points.add(1)
        p0, p1 = Vector((x + dx, back - 0.0005, 0.022 - i * 0.002)), Vector((x + dx * 2.2 + 0.004, back + 0.03, 0.0015))
        sp.bezier_points[0].co = p0; sp.bezier_points[0].handle_left = p0; sp.bezier_points[0].handle_right = p0 + Vector((0, 0.016, 0.004))
        sp.bezier_points[1].co = p1; sp.bezier_points[1].handle_left = p1 + Vector((0, -0.012, 0.012)); sp.bezier_points[1].handle_right = p1
        wo = link(bpy.data.objects.new("wire", cu)); cu.materials.append(cm)
    return o

def guide(c, M, flange=0.0072, r=0.0035):
    cyl(r, 0.030, (c.x, c.y, 0.015), M["steel"], verts=48, bevel=0.0004)
    for dz in (-TW / 2 - 0.0006, TW / 2 + 0.0006):
        cyl(flange, 0.0009, (c.x, c.y, ZT + dz), M["steel"], verts=64, bevel=0.0003)
    cyl(0.0085, 0.004, (c.x, c.y, 0.002), M["anod"], verts=64, bevel=0.0008)
    cyl(0.0026, 0.0012, (c.x, c.y, 0.0306), M["anod"], verts=32, bevel=0.0003)

def screw(x, y, M, z=0.0, r=0.0026):
    cyl(r, 0.0014, (x, y, z + 0.0005), M["screw"], verts=32, bevel=0.0006)
    box((r * 1.5, 0.00045, 0.0008), (x, y, z + 0.0013), M["gap"])

def build(scene_state=STATE, props=False):
    """Arma el grabador entero. Devuelve los objetos que se animan."""
    sc = reset()
    M = {
        "plate": brushed("plate", base=(0.62, 0.62, 0.61), rough=(0.26, 0.40)), "reel": turned("reel"), "hub": turned("hub", base=(0.70, 0.70, 0.69), rough=0.3),
        "knob": turned("knob", base=(0.82, 0.81, 0.79), rough=0.2), "knurl": knurled("knurl"),
        "anod": mat("anod", (0.018, 0.018, 0.020), 0.32, metal=1.0), "black": mat("black", (0.012, 0.012, 0.013), 0.55),
        "steel": mat("steel", (0.86, 0.86, 0.87), 0.07, metal=1.0), "screw": mat("screw", (0.75, 0.75, 0.76), 0.2, metal=1.0),
        "head": mat("head", (0.70, 0.70, 0.69), 0.16, metal=1.0), "pole": mat("pole", (0.92, 0.86, 0.70), 0.06, metal=1.0),
        "gap": mat("gap", (0.01, 0.01, 0.01), 0.5), "rubber": mat("rubber", (0.014, 0.014, 0.015), 0.62, spec=0.35),
        "wood": walnut("wood"), "pack": pack_material(), "tape": tape_material(), "glass": glass_material(),
        "wire_r": mat("wire_r", (0.45, 0.04, 0.03), 0.4), "wire_w": mat("wire_w", (0.7, 0.68, 0.62), 0.4),
        "wire_g": mat("wire_g", (0.05, 0.25, 0.08), 0.4),
        "face": mat("face", (0.74, 0.63, 0.43), 0.55, emit=(1.0, 0.76, 0.45), strength=0.30),
        "ink": mat("ink", (0.015, 0.013, 0.012), 0.6), "red": mat("redink", (0.55, 0.05, 0.03), 0.5),
        "needle": mat("needle", (0.02, 0.02, 0.02), 0.4), "print": mat("print", (0.75, 0.72, 0.64), 0.5),
        "keyal": brushed("keyal", rough=(0.18, 0.28), scale=600.0),
    }
    # Piso del estudio.
    bpy.ops.mesh.primitive_plane_add(size=12, location=(0, 0, -0.135)); add(bpy.context.active_object, floor_material())

    # ── Mueble: placa cepillada, laterales de nogal, frente inclinado negro, cuerpo ──
    box((0.66, 0.45, 0.008), (0, 0.025, -0.004), M["plate"], bevel=0.0015)
    box((0.66, 0.44, 0.118), (0, 0.025, -0.067), M["black"], bevel=0.002)                       # cuerpo (debajo)
    box((0.66, 0.11, 0.052), (0, -0.248, -0.101), M["black"], bevel=0.002)                      # bajo el frente
    prof = [(0.26, -0.132), (0.26, 0.009), (-0.200, 0.009), (-0.304, -0.066), (-0.304, -0.132)]
    for sx in (-1, 1):
        cu_pts = [(p[0], p[1]) for p in prof]
        side = shape("cheek", [cu_pts], 0.011 - 0.0015, M["wood"], bevel=0.0015, loc=(sx * 0.3415, 0, 0))
        side.rotation_euler = (math.radians(90), 0, math.radians(90))                     # el perfil (y, z) al plano yz
    # Frente inclinado: un marco local con x a lo ancho, y subiendo por la pendiente, z hacia afuera.
    slope = link(bpy.data.objects.new("slope", None)); slope.location = SLOPE_C; slope.rotation_euler = (SLOPE_A, 0, 0)
    sl = math.hypot(0.104, 0.075)
    holes = [rrect(0.104, 0.064, 0.004, c=(sx * 0.19, 0.004)) for sx in (-1, 1)]
    pn = shape("slopepanel", [rrect(0.66, sl, 0.002)] + holes, 0.002, M["anod"], bevel=0.0006); pn.parent = slope; pn.location = (0, 0, -0.002)
    box((0.66, 0.06, 0.004), (0, -0.30, -0.10), M["anod"]).rotation_euler = (math.radians(90), 0, 0)
    meters = []
    for k, sx in enumerate((-1, 1)):
        meters.append(vu_meter(slope, sx * 0.19, M, k))
    for i, x in enumerate((-0.075, -0.025, 0.025, 0.075)):                                   # perillas
        kb = cyl(0.0115, 0.013, (x, -0.004, 0.0065), M["knurl"], verts=128, bevel=0.0012); kb.parent = slope
        cap = cyl(0.0098, 0.002, (x, -0.004, 0.0135), M["knob"], verts=96, bevel=0.0008); cap.parent = slope
        a = math.radians((-40, 25, -10, 60)[i])
        ln = box((0.0009, 0.007, 0.0006), (x + math.sin(-a) * 0.005, -0.004 + math.cos(a) * 0.005, 0.0146), M["ink"], rot=(0, 0, a)); ln.parent = slope
        sk = cyl(0.014, 0.002, (x, -0.004, 0.001), M["anod"], verts=96, bevel=0.0007); sk.parent = slope
        for j in range(11):                                                                    # escala de la perilla
            b = math.radians(-135 + 27 * j)
            d = box((0.0006, 0.0022, 0.0002), (x - math.sin(b) * 0.0175, -0.004 + math.cos(b) * 0.0175, 0.0002), M["print"], rot=(0, 0, b)); d.parent = slope
    # Llave de velocidad.
    tb = cyl(0.0045, 0.003, (0.0, 0.026, 0.0015), M["knob"], verts=48, bevel=0.0007); tb.parent = slope
    tg = cyl(0.0017, 0.016, (0.0, 0.029, 0.009), M["steel"], verts=32, rot=(math.radians(-25), 0, 0)); tg.parent = slope
    tip = sphere(0.0026, (0.0, 0.0324, 0.0162), M["steel"]); tip.parent = slope

    # ── Carretes ──
    rS = reel("supply", RS, scene_state["packS"], M)
    rT = reel("takeup", RT, scene_state["packT"], M)

    # ── Recorrido de la cinta ──
    tS, tT = Vector((-0.272, -0.058)), Vector((0.272, -0.058))     # rodillos de los brazos de tensión
    gL, gR = Vector((-0.104, YB + 0.0035)), Vector((0.090, YB + 0.0035))
    e = 0.00015                                                    # la cinta va apenas por fuera de cada superficie
    circles = [(RS, scene_state["packS"], 1), (tS, 0.009 + e, 1), (gL, 0.0035 + e, 1)]
    for i, x in enumerate(HEADS_X):
        rf = 0.016 if i else 0.013
        circles.append((Vector((x, YB - 0.0012 + rf)), rf + e, 1))
    capc = Vector((CAP_X, YB - 0.0004 + CAP_R))
    circles += [(capc, CAP_R + e, 1), (gR, 0.0035 + e, 1), (tT, 0.009 + e, 1), (RT, scene_state["packT"], 1)]
    path = belt(circles)
    tape = ribbon("tape", path, ZT - TW / 2, ZT + TW / 2, M["tape"])
    # Bloque de cabezas.
    shape("headblock", [rrect(0.150, 0.052, 0.006, c=(-0.034, -0.083))], 0.0018, M["anod"], bevel=0.0006, loc=(0, 0, 0.0018))
    for i, x in enumerate(HEADS_X): head(x, M, i)
    for (x, y) in ((-0.103, -0.064), (0.035, -0.064), (-0.103, -0.1), (0.035, -0.07)): screw(x, y, M, 0.0036)
    for g in (gL, gR): guide(g, M)
    # Brazos de tensión: rodillo con pestañas, brazo y pivote.
    arms = []
    for t, piv in ((tS, Vector((-0.300, 0.012))), (tT, Vector((0.300, 0.012)))):
        cyl(0.009, 0.020, (t.x, t.y, ZT), M["steel"], verts=64, bevel=0.0006)
        for dz in (-TW / 2 - 0.0007, TW / 2 + 0.0007): cyl(0.0115, 0.001, (t.x, t.y, ZT + dz), M["steel"], verts=64, bevel=0.0003)
        cyl(0.0045, 0.003, (t.x, t.y, ZT + 0.0115), M["anod"], verts=48, bevel=0.0006)
        d = piv - t; L = d.length; mid = (piv + t) / 2
        arms.append(box((L, 0.007, 0.003), (mid.x, mid.y, 0.0035), M["knob"], bevel=0.0012, rot=(0, 0, math.atan2(d.y, d.x))))
        cyl(0.0085, 0.008, (piv.x, piv.y, 0.004), M["knob"], verts=64, bevel=0.0012)
        cyl(0.003, 0.002, (piv.x, piv.y, 0.0085), M["anod"], verts=32, bevel=0.0005)
    # Cabrestante y rodillo de presión (del lado de afuera, contra la cinta).
    cyl(0.012, 0.004, (capc.x, capc.y, 0.002), M["anod"], verts=64, bevel=0.0008)
    cyl(CAP_R, 0.032, (capc.x, capc.y, 0.016), M["steel"], verts=64, bevel=0.0004)
    pc = Vector((CAP_X, YB - 0.0004 - 2 * e - 0.0001 - PINCH_R))
    pinch = link(bpy.data.objects.new("pinch", None)); pinch.location = (pc.x, pc.y, 0.0)
    for o in (cyl(PINCH_R, 0.0160, (0, 0, ZT), M["rubber"], verts=128, bevel=0.0012),
              cyl(0.0068, 0.0025, (0, 0, ZT + 0.0090), M["knob"], verts=64, bevel=0.0007),
              cyl(0.0022, 0.0012, (0, 0, ZT + 0.0108), M["anod"], verts=32, bevel=0.0003),
              cyl(0.0009, 0.0006, (0.0045, 0, ZT + 0.0104), M["anod"], verts=16)):
        o.parent = pinch
    parm = Vector((CAP_X + 0.042, YB - 0.030))
    d = parm - pc; L = d.length
    box((L, 0.009, 0.004), ((pc + parm).x / 2, (pc + parm).y / 2, ZT - 0.0105), M["knob"], bevel=0.0014, rot=(0, 0, math.atan2(d.y, d.x)))
    cyl(0.009, ZT - 0.008, (parm.x, parm.y, (ZT - 0.008) / 2), M["knob"], verts=64, bevel=0.0012)
    cyl(0.004, ZT - 0.007, (pc.x, pc.y, (ZT - 0.0105) / 2), M["knob"], verts=48, bevel=0.0008)
    # ── Teclas del transporte: rebobinar, avanzar, reproducir, parar, grabar, con sus pilotos ──
    keys, lamps = {}, {}
    names = ("rew", "ff", "play", "stop", "rec")
    for i, nm in enumerate(names):
        x = -0.096 + 0.048 * i
        box((0.040, 0.030, 0.003), (x, -0.172, 0.0015), M["anod"], bevel=0.0008)                  # zócalo
        k = box((0.034, 0.024, 0.011), (x, -0.172, 0.0085), M["keyal"], bevel=0.0018); keys[nm] = k
        lm = mat("lamp_" + nm, {"play": (0.20, 0.55, 0.26), "stop": (0.60, 0.10, 0.05), "rec": (0.95, 0.08, 0.05)}.get(nm, (1.0, 0.65, 0.25)), 0.15,
                 emit={"play": (0.35, 1.0, 0.45), "stop": (1.0, 0.18, 0.06), "rec": (1.0, 0.06, 0.03)}.get(nm, (1.0, 0.6, 0.2)), strength=0.0)
        # PLAY y STOP tienen el signo iluminado (se ve desde cualquier ángulo); además, un piloto detrás de cada tecla.
        symbol(nm, 0.0, 0.0, 0.0056, M, lm if nm in ("play", "stop") else None).parent = k
        cyl(0.0042, 0.002, (x, -0.150, 0.001), M["knob"], verts=48, bevel=0.0006)
        sphere(0.0032, (x, -0.150, 0.0024), lm, scale=(1, 1, 0.7)); lamps[nm] = lm
    for (x, y) in ((-0.315, -0.19), (0.315, -0.19), (-0.315, 0.238), (0.315, 0.238), (-0.135, -0.19), (0.135, -0.19)):
        screw(x, y, M)
    if props: cassettes(M)
    lights(sc)
    return dict(sc=sc, rS=rS, rT=rT, tape=M["tape"], pinch=pinch, keys=keys, lamps=lamps, meters=meters, M=M)

def symbol(nm, x, y, z, M, lit=None):
    """Los signos grabados de cada tecla (mallas planas, sin texto)."""
    tris = []
    def tri(cx, w, h, d=1):
        return [(cx - d * w / 2, -h / 2), (cx + d * w / 2, 0), (cx - d * w / 2, h / 2)]
    if nm == "rew": tris = [tri(-0.0022, 0.0045, 0.006, -1), tri(0.0022, 0.0045, 0.006, -1)]
    elif nm == "ff": tris = [tri(-0.0022, 0.0045, 0.006), tri(0.0022, 0.0045, 0.006)]
    elif nm == "play": tris = [tri(0.0, 0.0055, 0.0065)]
    elif nm == "stop": tris = [[(-0.0028, -0.0028), (0.0028, -0.0028), (0.0028, 0.0028), (-0.0028, 0.0028)]]
    else: tris = [circle_pts(0.0031, 32)]
    verts, faces = [], []
    for t in tris:
        n0 = len(verts); verts += [(x + p[0], y + p[1], z) for p in t]; faces.append(tuple(range(n0, n0 + len(t))))
    me = bpy.data.meshes.new(nm); me.from_pydata(verts, [], faces)
    o = link(bpy.data.objects.new("sym_" + nm, me)); me.materials.append(lit or (M["red"] if nm == "rec" else M["ink"]))
    return o

def vu_meter(slope, x, M, k):
    """Un VU: caja negra, cara crema iluminada por detrás con la escala impresa (−20 a +3, la zona roja), aguja
    negra con su pivote escondido bajo la visera, vidrio y marco de aluminio. Devuelve el pivote de la aguja."""
    def child(o): o.parent = slope; return o
    W, H = 0.104, 0.064; cx, cy = x, 0.004
    child(box((W, H, 0.004), (cx, cy, -0.0135), M["black"]))                                 # fondo de la caja
    child(shape("well", [rrect(W + 0.006, H + 0.006, 0.004, c=(cx, cy)), list(reversed(rrect(W, H, 0.004, c=(cx, cy))))],
                0.004, M["black"], loc=(0, 0, -0.008)))
    child(box((W - 0.004, H - 0.004, 0.001), (cx, cy, -0.0105), M["face"]))                  # cara
    child(shape("bezel", [rrect(W + 0.008, H + 0.008, 0.006, c=(cx, cy)), list(reversed(rrect(W - 0.002, H - 0.002, 0.003, c=(cx, cy))))],
                0.0012, M["knob"], bevel=0.0007, loc=(0, 0, 0.0012)))
    child(box((W - 0.004, 0.012, 0.004), (cx, cy - H / 2 + 0.008, -0.006), M["black"], bevel=0.001))   # visera
    child(box((W - 0.002, H - 0.002, 0.0004), (cx, cy, -0.0012), M["glass"]))
    piv = (cx, cy - 0.032, -0.0085)
    R = 0.050
    def ang(db): return math.radians(-46 + 92 * min(1.03, 0.708 * 10 ** (db / 20)) / 1.0)
    def at(a, r): return (piv[0] + math.sin(a) * r, piv[1] + math.cos(a) * r)
    zf = -0.00995
    # Arco de la escala (negro hasta 0, rojo después) y marcas.
    for (d0, d1, m, w) in ((-20, 0, M["ink"], 0.0008), (0, 3, M["red"], 0.0022)):
        a0, a1 = ang(d0), ang(d1); n = 48
        inner = [at(a0 + (a1 - a0) * i / n, R) for i in range(n + 1)]
        outer = [at(a1 - (a1 - a0) * i / n, R + w) for i in range(n + 1)]
        me = bpy.data.meshes.new("arc"); me.from_pydata([(p[0], p[1], zf) for p in inner + outer], [], [tuple(range(2 * n + 2))])
        o = child(link(bpy.data.objects.new("arc", me))); me.materials.append(m)
    marks = [(-20, "-20"), (-10, "-10"), (-7, "-7"), (-5, "-5"), (-3, "-3"), (-2, ""), (-1, ""), (0, "0"), (1, ""), (2, "+2"), (3, "+3")]
    for db, label in marks:
        a = ang(db); p = at(a, R + 0.0022); big = bool(label)
        o = child(box((0.0009, 0.0050 if big else 0.0032, 0.0001), (p[0], p[1], zf), M["red"] if db > 0 else M["ink"], rot=(0, 0, -a)))
        if label:
            q = at(a, R + 0.0080)
            t = text(label, (q[0], q[1], zf), 0.0052, M["red"] if db > 0 else M["ink"], rot=-a); child(t)
    for pc in (0, 20, 40, 60, 80, 100):                                                      # escala de porcentaje
        a = math.radians(-46 + 92 * 0.708 * pc / 100.0); p = at(a, R - 0.0035)
        child(box((0.0004, 0.0018, 0.0001), (p[0], p[1], zf), M["ink"], rot=(0, 0, -a)))
    child(text("VU", (cx, cy - 0.005, zf), 0.011, M["ink"]))
    pv = link(bpy.data.objects.new(f"needle{k}", None)); pv.parent = slope; pv.location = piv
    nd = box((0.0008, 0.054, 0.0003), (0, 0.027, 0), M["needle"]); nd.parent = pv
    tipm = box((0.0006, 0.010, 0.00031), (0, 0.049, 0), M["red"]); tipm.parent = pv
    pv.rotation_euler = (0, 0, -ang(-20) - math.radians(2))
    # Piloto de pico a la derecha de cada VU.
    pm = mat(f"peak{k}", (0.9, 0.12, 0.05), 0.2, emit=(1.0, 0.12, 0.04), strength=0.0)
    child(cyl(0.0034, 0.002, (cx + W / 2 + 0.012, cy + 0.020, 0.001), M["knob"], verts=48, bevel=0.0006))
    s = sphere(0.0024, (cx + W / 2 + 0.012, cy + 0.020, 0.0022), pm, scale=(1, 1, 0.6)); child(s)
    return dict(pivot=pv, ang=ang, peak=pm)

def text(s, loc, size, m, rot=0.0):
    cu = bpy.data.curves.new("txt", "FONT"); cu.body = s; cu.size = size; cu.align_x = "CENTER"; cu.align_y = "CENTER"
    o = link(bpy.data.objects.new("txt", cu)); o.location = loc; o.rotation_euler = (0, 0, rot); cu.materials.append(m)
    return o

def cassettes(M):
    """Dos casetes genéricas apiladas en la mesa, a la izquierda del frente: carcasa ahumada, etiqueta crema con
    renglones y, en su ventana, las bobinas marrones y los cubos dentados bajo el vidrio (capas finas sobre la
    carcasa: de lejos se lee como la ventana de verdad)."""
    shell = mat("shell", (0.03, 0.03, 0.032), 0.22, coat=0.4)
    label = mat("label", (0.78, 0.74, 0.64), 0.6)
    hubm = mat("chub", (0.85, 0.84, 0.8), 0.35)
    win = mat("cwin", (0.006, 0.006, 0.007), 0.1)
    for i, (x, y, a) in enumerate(((-0.47, -0.27, 28), (-0.462, -0.276, 16))):
        z = -0.135 + 0.0049 + i * 0.0098
        root = link(bpy.data.objects.new("cassette", None)); root.location = (x, y, z); root.rotation_euler = (0, 0, math.radians(a))
        top = 0.0048
        parts = [shape("cshell", [rrect(0.1004, 0.0638, 0.003)], 0.0042, shell, bevel=0.0006)]
        parts.append(box((0.060, 0.019, 0.0001), (0, 0.002, top + 0.00005), win))
        for sx, rp in ((-1, 0.0105), (1, 0.0150)):
            parts.append(cyl(rp, 0.0001, (sx * 0.021, 0.002, top + 0.00015), M["pack"], verts=64))
            parts.append(cyl(0.0058, 0.0001, (sx * 0.021, 0.002, top + 0.00025), hubm, verts=12))
            parts.append(cyl(0.0032, 0.0001, (sx * 0.021, 0.002, top + 0.00035), win, verts=24))
        parts.append(shape("clabel", [rrect(0.088, 0.044, 0.002, c=(0, 0.004)), list(reversed(rrect(0.058, 0.017, 0.003, c=(0, 0.002))))],
                           0.0001, label, loc=(0, 0, top + 0.0004)))
        for j, yy in enumerate((0.0215, 0.0145, -0.0105)):
            parts.append(box((0.07 if j < 2 else 0.05, 0.0004, 0.00005), (0, yy, top + 0.00053), M["ink"]))
        parts.append(box((0.058, 0.017, 0.0001), (0, 0.002, top + 0.0006), M["glass"]))
        for p in parts: p.parent = root

def lights(sc):
    def area(name, loc, look, size, energy, color, size_y=None):
        l = bpy.data.lights.new(name, "AREA"); l.energy = energy; l.color = color
        if size_y: l.shape = "RECTANGLE"; l.size = size; l.size_y = size_y
        else: l.size = size
        o = link(bpy.data.objects.new(name, l)); o.location = loc
        o.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o
    area("key", (-0.75, -0.55, 0.95), (0, 0.0, 0), 0.9, 26.0, (1.0, 0.93, 0.84), 0.6)
    area("fill", (0.95, -0.75, 0.30), (0, 0, 0), 1.2, 4.0, (0.72, 0.82, 1.0))
    area("rim", (0.25, 1.0, 0.40), (0, 0.05, 0), 1.0, 28.0, (1.0, 0.86, 0.70), 0.35)
    area("strip", (0.0, -0.10, 0.80), (0, -0.02, 0), 1.4, 6.0, (1.0, 0.97, 0.94), 0.06)
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
    w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.0025, 0.0024, 0.0023, 1)


# ── Estado en el tiempo ──
def stop_shape(x, curve=0.5):
    """La misma curva de frenado que Transport.h (de lineal a exponencial)."""
    x = min(1.0, max(0.0, x)); lin = 1.0 - x
    a = 1.0 + 6.0 * curve; ex = (math.exp(-a * x) - math.exp(-a)) / (1.0 - math.exp(-a))
    return lin + (ex - lin) * curve

def travel_table(t_stop=None, stop_s=1.8, total=40.0, dt=0.001):
    """Metros de cinta recorridos desde t=0 (a 15 ips, con el tape stop en t_stop)."""
    xs, s, x = [0.0], 0.0, 0.0
    n = int(total / dt)
    for i in range(1, n + 1):
        t = i * dt
        v = 1.0 if t_stop is None or t < t_stop else stop_shape((t - t_stop) / stop_s)
        x += IPS15 * v * dt; xs.append(x)
    return lambda t: xs[min(n, max(0, int(round(t / dt))))]

def set_transport(rig, x):
    """Carretes, rodillo de presión y textura de la cinta para x metros recorridos (sentido antihorario)."""
    rig["rS"].rotation_euler = (0, 0, x / STATE["packS"] + 0.4)
    rig["rT"].rotation_euler = (0, 0, x / STATE["packT"] + 1.3)
    rig["pinch"].rotation_euler = (0, 0, -x / PINCH_R)
    rig["tape"].node_tree.nodes["scroll"].inputs["Location"].default_value = (-x * 10.0, 0, 0)

def set_needle(rig, k, frac):
    m = rig["meters"][k]
    m["pivot"].rotation_euler = (0, 0, -math.radians(-46 + 92 * min(1.03, max(-0.02, frac))))


# ── Fotos de la web ──
SHOTS = {   # posición, mira, lente, f-stop, resolución
    "hero":  ((-0.80, -1.40, 0.50), (-0.075, -0.03, -0.050), 46, 8.0, (2400, 1100)),
    "close": ((0.105, -0.265, 0.095), (-0.022, -0.104, 0.012), 60, 4.5, (1600, 1200)),
    "top":   ((0.30, -0.62, 0.70), (0.0, 0.0, -0.01), 50, 9.0, (1600, 1200)),
}

def camera(sc, loc, look, lens, fstop):
    cd = bpy.data.cameras.new("cam"); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop; cd.clip_start = 0.005
    cam = link(bpy.data.objects.new("cam", cd)); sc.camera = cam
    cam.location = Vector(loc); d = Vector(look) - Vector(loc)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); cd.dof.focus_distance = d.length
    return cam

def stills():
    for name in arg("--shots", "hero,close,top").split(","):
        rig = build(STATE, props=(name == "cas"))
        sc = rig["sc"]
        if name == "hero":
            # Un reflejo largo y tibio en las bridas de los carretes: tira alta detrás, que el aluminio torneado abre en arco.
            l = bpy.data.lights.new("glint", "AREA"); l.shape = "RECTANGLE"; l.size = 0.9; l.size_y = 0.10; l.energy = 10.0
            l.color = (1.0, 0.9, 0.78); g = link(bpy.data.objects.new("glint", l), sc); g.location = (0.15, 0.75, 0.55)
            g.rotation_euler = (Vector((0.0, 0.08, 0.0)) - g.location).to_track_quat("-Z", "Y").to_euler()
        loc, look, lens, fstop, res = SHOTS[name]
        camera(sc, loc, look, lens, fstop); sc.render.resolution_x, sc.render.resolution_y = res
        set_transport(rig, 1.1)
        set_needle(rig, 0, 0.55); set_needle(rig, 1, 0.62)
        rig["lamps"]["play"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 14.0
        rig["keys"]["play"].location.z -= 0.003
        sc.render.filepath = os.path.join(OUT, name + ".jpg")
        bpy.ops.render.render(write_still=True)


# ── Film de la web (filmkit) ──
def film2():
    """Film de Tarnish: un piano eléctrico (tine-piano del ATFS1) por la máquina de estudio a 15 ips (preset Warm
    Studio Tape, 92 bpm) y, en el último acorde, el tape stop: la cinta frena en 1,8 s con la curva del plugin
    (tarnishrender --at 18.913 stop=1 --set stop_time=1.8, el mismo comando que lee film.json). Tomas en tiempos de
    92 bpm (0,652 s); t es el tiempo del film = el del audio.

      1 open     6 tiempos  0,00-3,91 s  Macro rasante sobre el carrete que entrega: la brida torneada gira en
                                          primer plano arriba a la derecha, la cinta sale de la bobina; la cámara
                                          avanza despacio y el foco va de la brida al rodillo de tensión. El tercio
                                          de abajo a la izquierda queda en sombra para el título.
      2 heads    6 tiempos  3,91-7,83 s  Travelling lateral a la altura de la cinta frente al bloque de cabezas, de
                                          borrado a reproducción, 85 mm; foco que sigue a cada cabeza; la cinta
                                          corre con sus rayas y motas.
      3 capstan  4 tiempos  7,83-10,43   Cabrestante y rodillo de presión girando (la marca del rodillo), la cinta
                                          por el pellizco; empuje lento, cámara en mano muy sutil.
      4 vu       4 tiempos 10,43-13,04   Los dos VU de frente: las agujas siguen la música (balística de VU de
                                          300 ms, un canal cada uno), el piloto de pico salta en los acordes fuertes;
                                          una luz cálida barre el vidrio.
      5 hero     7 tiempos 13,04-17,61   Plano general bajo que sube y gira de la izquierda al frente: la máquina
                                          entera, los dos carretes a sus velocidades (ω = v/r: el que recibe, más
                                          chico, gira más rápido).
      6 stop     5 tiempos 17,61-20,87   Las teclas en primer plano y los carretes detrás: a los 18,91 s se hunde
                                          STOP, se apaga el piloto de PLAY y se enciende el rojo; el foco pasa a los
                                          carretes, que frenan con la curva del audio y quedan quietos; las agujas
                                          caen.
      7 end      5 tiempos 20,87-24,13   (fundido) La máquina quieta desde arriba y adelante, la cámara se aleja;
                                          film.py oscurece y pone el cierre.
    """
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final")
    wav = arg("--audio")
    vu = vu_levels(wav)
    env = fk.envelope(wav)
    hits = set(fk.onsets(env, thresh=0.22, gap=6))
    travel = travel_table(T_STOP, STOP_S)
    t_cursor = 0.0
    starts = {}
    for (name, beats, kind) in FILM_SHOTS:
        dur = round(beats * BEAT * FPS) / FPS
        starts[name] = (t_cursor - (0.5 * BEAT if kind == "dissolve" else 0.0), dur)
        t_cursor += dur
    for (name, beats, kind) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        t0, dur = starts[name]
        rig = build(STATE, props=(name == "end"))
        sc = rig["sc"]; fk.setup(sc, quality)
        if CPU: sc.cycles.device = "CPU"; sc.cycles.denoising_use_gpu = False
        if arg("--pct"): sc.render.resolution_percentage = PCT
        cam = fk.camera(sc)
        # En los planos cercanos el contraluz se refleja en la placa como un fondo blanco: se baja.
        rim = bpy.data.objects["rim"].data; rim.energy *= {"open": 0.5, "heads": 0.25, "capstan": 0.4, "stop": 0.35}.get(name, 1.0)
        if name == "open":                       # el tercio de abajo a la izquierda (el título) queda en penumbra
            bpy.data.objects["key"].data.energy *= 0.4; bpy.data.objects["strip"].data.energy = 0.0
            bpy.data.objects["key"].location = (-0.45, -0.25, 0.75)
        frames = int(round(dur * FPS))
        sweep = None
        if name == "vu":
            l = bpy.data.lights.new("sweep", "AREA"); l.shape = "RECTANGLE"; l.size = 0.03; l.size_y = 0.5; l.energy = 0.0
            l.color = (1.0, 0.85, 0.65); sweep = link(bpy.data.objects.new("sweep", l))
        path, focus, shake = film_path(name, rig)

        def pose(f, t, u, name=name, path=path, focus=focus, shake=shake, t0=t0):
            T = t0 + t
            set_transport(rig, travel(T))
            for k in (0, 1): set_needle(rig, k, vu[k](T))
            moved = [rig["rS"], rig["rT"], rig["pinch"]] + [m["pivot"] for m in rig["meters"]]
            # Teclas: PLAY hundida mientras corre; STOP se hunde 0,25 s al pararse.
            run = T < T_STOP
            rig["keys"]["play"].location.z = 0.0085 - (0.003 if run else 0.0)
            ds = T - T_STOP
            rig["keys"]["stop"].location.z = 0.0085 - 0.0035 * (fk.ease(ds / 0.06, "out") if 0 <= ds < 0.30 else (max(0.0, 1 - (ds - 0.30) / 0.12) if ds >= 0.30 else 0.0))
            moved += [rig["keys"]["play"], rig["keys"]["stop"]]
            if sweep is not None:
                a = fk.ease(u, "smooth")
                sweep.location = (-0.32 + 0.64 * a, -0.42, 0.06); sweep.rotation_euler = (math.radians(62), 0, math.radians(-8)); moved.append(sweep)
            fp = focus(u, T) if callable(focus) else focus
            path.apply(cam, u, focus=fp, shake=shake, t=T)
            return moved

        def keys(f, name=name, t0=t0):
            T = t0 + (f - 1) / FPS
            g = int(round(T * FPS))
            run = T < T_STOP
            L = rig["lamps"]
            for nm, val in (("play", 8.0 if run else 0.0), ("stop", 0.0 if run else 12.0)):
                s = L[nm].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]; s.default_value = val; s.keyframe_insert("default_value")
            for k in (0, 1):
                hot = run and any(0 <= g - h < 3 for h in hits) and vu[k](T) > 0.62
                s = rig["meters"][k]["peak"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
                s.default_value = 9.0 if hot else 0.0; s.keyframe_insert("default_value")
            sl = rig["tape"].node_tree.nodes["scroll"].inputs["Location"]; sl.keyframe_insert("default_value")
            if sweep is not None:
                u = (f - 1) / max(1, frames - 1)
                sweep.data.energy = 3.0 * math.sin(math.pi * min(1.0, max(0.0, u))) ** 2; sweep.data.keyframe_insert("energy")

        step = int(arg("--step", "1")) if arg("--step") != "mid" else max(1, (frames - 1) // 2)
        fk.render_shot(sc, name, frames, pose, OUT, step=step, extra_keys=keys)

BPM = 92.0
BEAT = 60.0 / BPM
T_STOP = 29 * BEAT                       # 18,913 s: el tape stop, en el tiempo 29
STOP_S = 1.8
FILM_SHOTS = [("open", 6, "cut"), ("heads", 6, "cut"), ("capstan", 4, "cut"), ("vu", 4, "cut"),
              ("hero", 7, "cut"), ("stop", 5, "cut"), ("end", 5, "dissolve")]

def film_path(name, rig):
    """Recorrido de cámara, foco y cámara en mano de cada toma."""
    import filmkit as fk
    V = lambda *a: Vector(a)
    S3 = V(RS.x, RS.y, ZT)
    if name == "open":
        # El carrete arriba a la derecha, la cinta saliendo hacia el rodillo de tensión abajo a la izquierda (en sombra).
        p = fk.Path([(0, S3 + V(0.215, -0.250, 0.150), S3 + V(-0.085, -0.095, -0.020), 50, 5.0),
                     (1, S3 + V(0.170, -0.205, 0.120), S3 + V(-0.075, -0.085, -0.020), 52, 5.0)])
        near = S3 + V(0.07, -0.09, 0.006)
        return p, (lambda u, T: near), 0.0010
    if name == "heads":
        hx = lambda u: HEADS_X[0] + (HEADS_X[2] - HEADS_X[0]) * u
        p = fk.Path([(0, V(HEADS_X[0] - 0.050, -0.250, 0.085), V(HEADS_X[0] + 0.006, YB + 0.004, ZT), 70, 4.0),
                     (1, V(HEADS_X[2] + 0.002, -0.250, 0.078), V(HEADS_X[2] + 0.006, YB + 0.004, ZT), 70, 4.0)])
        return p, (lambda u, T: V(hx(fk.rack(u, 0.1, 0.9, 0.0, 1.0)), YB - 0.001, ZT)), 0.0006
    if name == "capstan":
        c = V(CAP_X, YB, ZT)
        p = fk.Path([(0, c + V(0.115, -0.150, 0.075), c + V(-0.010, 0.000, -0.002), 85, 4.5),
                     (1, c + V(0.095, -0.125, 0.062), c + V(-0.008, 0.000, -0.002), 85, 4.5)])
        return p, c + V(0, -0.002, 0.002), 0.0008
    if name == "vu":
        n = V(0, -math.sin(SLOPE_A), math.cos(SLOPE_A))
        mL = slope_pt(-0.19, 0.004)
        p = fk.Path([(0, mL + n * 0.23 + V(-0.045, 0, 0.012), mL + V(0.012, 0, 0), 65, 5.0),
                     (1, mL + n * 0.20 + V(0.025, 0, 0.006), mL + V(0.018, 0, 0), 65, 5.0)])
        return p, mL, 0.0
    if name == "hero":
        def P(a, r, z): return V(r * math.cos(math.radians(a)), r * math.sin(math.radians(a)), z)
        p = fk.Path([(0, P(-132, 1.05, 0.13), V(0.0, 0.0, -0.035), 40, 6.0),
                     (0.5, P(-114, 1.00, 0.24), V(0.0, 0.0, -0.035), 40, 6.0),
                     (1, P(-97, 0.96, 0.35), V(0.0, 0.0, -0.035), 40, 6.0)])
        return p, V(0.0, -0.06, 0.0), 0.0
    if name == "stop":
        k = V(0.048, -0.172, 0.012)
        p = fk.Path([(0, V(0.120, -0.470, 0.215), V(0.004, -0.030, 0.000), 45, 3.2),
                     (1, V(0.105, -0.450, 0.205), V(0.000, -0.020, 0.000), 45, 3.2)], easing="out")
        reels = V(0.0, 0.07, ZT)
        def focus(u, T):
            return k.lerp(reels, fk.rack(T, T_STOP + 0.35, T_STOP + 1.2, 0.0, 1.0))
        return p, focus, 0.0006
    # end
    p = fk.Path([(0, V(0.0, -0.78, 0.62), V(0.0, -0.02, -0.03), 50, 8.0),
                 (1, V(0.0, -1.02, 0.80), V(0.0, -0.02, -0.03), 50, 8.0)], easing="out")
    return p, V(0.0, -0.05, 0.0), 0.0

def vu_levels(path):
    """Agujas de los VU por canal: valor rectificado medio con la balística de un VU (2.º orden, ~300 ms para llegar,
    poco sobrepaso), calibrado para que los acordes fuertes lleguen a 0/+1 VU. Devuelve dos funciones t → fracción
    de la escala (0,708 = 0 VU)."""
    import wave, numpy as np
    w = wave.open(path); sr = w.getframerate(); ch = w.getnchannels(); sw = w.getsampwidth(); n = w.getnframes()
    raw = w.readframes(n); w.close()
    if sw == 2: a = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
    else:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        a = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8).astype(np.float32) / 8388608.0
    a = a.reshape(-1, ch)
    rate = 240; hop = sr // rate
    out = []
    for c in range(min(2, ch)):
        x = np.abs(a[:, c]); m = len(x) // hop
        mean = x[: m * hop].reshape(m, hop).mean(axis=1)
        wn, z = 2 * math.pi * 2.1, 0.78; y, v, ys = 0.0, 0.0, []
        for s in mean:
            acc = wn * wn * (s - y) - 2 * z * wn * v; v += acc / rate; y += v / rate; ys.append(y)
        ys = np.array(ys)
        ref = np.percentile(ys[ys > 1e-4], 97) if np.any(ys > 1e-4) else 1.0
        ys = 0.708 * 1.12 * ys / ref
        out.append(ys)
    if len(out) == 1: out.append(out[0])
    def f(ys):
        return lambda t: float(ys[min(len(ys) - 1, max(0, int(t * rate)))]) if t >= 0 else 0.0
    return f(out[0]), f(out[1])


mode = arg("--mode", "stills")
if mode == "film2": film2()
else: stills()
