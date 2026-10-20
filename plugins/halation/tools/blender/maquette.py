"""La maqueta de yeso de Halation, para la web y el film.

Una sala de yeso blanco en corte (sin techo y sin pared de frente, como una maqueta de arquitecto) sobre una mesa
oscura. Adentro, el sonido son partículas de luz: cada pulso sale de la fuente, viaja, rebota en las paredes y se
apaga con el tiempo de reverberación. Tamaño = nivel, color = brillo (graves tibios, agudos fríos; las colas se
entibian porque los agudos se apagan antes). Con Space alto cada rebote se parte en fragmentos y la sala se llena de
niebla; con Freeze todo queda suspendido como polvo y la sala se tiñe de violeta. Es la misma idea que dibuja el
plugin (Source/ui/RoomSim.h), simulada acá con su propio paso fijo y semilla: el resultado es determinista.

Las partículas son una sola malla que se rearma en cada cuadro, con el color (ya multiplicado por la intensidad) en
un atributo de vértice; el movimiento se dibuja estirando cada esfera hacia atrás (una cola corta), así no hace falta
animar cientos de objetos ni usar desenfoque de movimiento.

Corre en pc50-wsl (en ~/halation-render, siempre bajo el lock de GPU compartido):
  B=~/ai/blender/blender-5.2.2-linux-x64/blender; L=~/ai/prerendered/.gpu.lock
  flock $L $B -b -P maquette.py -- --mode stills --out out --samples 256
  for s in echo space freeze; do flock $L $B -b -P maquette.py -- --mode film --shot $s --out film --samples 96; done
El film de la web (filmkit, seis tomas sincronizadas con film_audio.py; ver film2()):
  flock $L $B -b --factory-startup -P maquette.py -- --mode film2 --quality draft|final --audio halation-audio.wav --out out
  [--shot open,macro] [--test 1,48,96]   y después en la Mac: python3 scripts/film.py halation <cuadros>
Cuadros en JPEG (C: de pc50 anda justo de espacio). Opciones: --pct 50 (pruebas), --shots hero,close,
--first N --step N, --cpu (render por CPU, sin lock: para cuando el estudio de generación usa la GPU).
Unidades: 1 = 1 m. La maqueta mide 0,9 x 0,6 m; el frente abierto mira hacia -y.
"""
import bpy, bmesh, math, os, sys, random
from array import array
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "160"))
PCT = int(arg("--pct", "100"))
FPS = 24.0

# Interior de la sala (la cara de adentro de las paredes).
X0, X1, Y0, Y1 = -0.34, 0.34, -0.20, 0.20
F, H, T = 0.012, 0.24, 0.018                   # piso, alto de las paredes, espesor
COLS = [(sx * 0.22, y) for sx in (-1, 1) for y in (-0.10, 0.0, 0.10)]
RC = 0.012                                      # radio de las columnas
SRC = Vector((0.0, -0.09, 0.075))              # la fuente: una esfera chica sobre un pedestal

COLD, WARM, FROZEN = Vector((0.10, 0.45, 1.0)), Vector((1.0, 0.43, 0.13)), Vector((0.58, 0.44, 1.0))

# ---------------------------------------------------------------------------------------------------------------
# Simulación

class Part:
    __slots__ = ("p", "v", "e", "w", "kind", "bounces")
    def __init__(s, p, v, e, w, kind):
        s.p, s.v, s.e, s.w, s.kind, s.bounces = p, v, e, w, kind, 0

class Sim:
    """Paso fijo de 1/96 s. `params(t)` da el estado del plugin en el tiempo t."""
    DT = 1.0 / 96.0

    def __init__(s, params, seed=7):
        s.params, s.rng, s.parts, s.t, s.next_pulse, s.k = params, random.Random(seed), [], None, 0.0, 0

    def run_to(s, t_end, t_start):
        if s.t is None: s.t = t_start; s.next_pulse = t_start
        while s.t < t_end - 1e-9: s.step()

    def emit_pulse(s, P):
        r = s.rng
        w = P["warm"][s.k % len(P["warm"])]
        side = (1 if s.k % 2 else -1) if P.get("ping") else 0
        org = SRC + Vector((0.11 * side, 0.0, 0.0))
        n = P["rays"]
        for i in range(n):
            a = 2 * math.pi * (i + r.random() * 0.6) / n
            el = (r.random() - 0.35) * 0.9
            d = Vector((math.cos(a) * math.cos(el), math.sin(a) * math.cos(el), math.sin(el)))
            if side: d.x = abs(d.x) * side if r.random() < 0.7 else d.x
            s.parts.append(Part(org.copy(), d.normalized() * P["speed"], P["level"] * (0.7 + 0.3 * r.random()),
                                min(1.0, max(0.0, w + (r.random() - 0.5) * 0.15)), 0))
        s.k += 1

    def step(s):
        P, dt, r = s.params(s.t), s.DT, s.rng
        if P["period"] > 0 and s.t >= s.next_pulse:
            if P.get("pulses", True): s.emit_pulse(P)
            s.next_pulse += P["period"]
        sp, frz = P["space"], P.get("freeze", 0.0)
        t60 = P["t60"]
        dec = math.exp(-6.91 * dt / t60) if frz < 1 else 1.0
        dec = dec ** (1.0 - frz)
        born = []
        for q in s.parts:
            if frz > 0:
                q.v *= (1.0 - 0.08 * frz)
                q.v += Vector((r.gauss(0, 1), r.gauss(0, 1), r.gauss(0, 1))) * 0.0006 * frz
            rise = P.get("rise", 0.0)
            if rise and q.kind != 2: q.v.z += rise * dt; q.w = max(0.0, q.w - 0.25 * dt * rise / 0.1)
            q.p += q.v * dt
            rad = radius(q)
            hit = None
            for ax, lo, hi in ((0, X0, X1), (1, Y0, Y1), (2, F, F + H)):
                if q.p[ax] < lo + rad: q.p[ax] = lo + rad; q.v[ax] = abs(q.v[ax]); hit = ax
                elif q.p[ax] > hi - rad: q.p[ax] = hi - rad; q.v[ax] = -abs(q.v[ax]); hit = ax
            for cx, cy in COLS:
                dx, dy = q.p.x - cx, q.p.y - cy
                d = math.hypot(dx, dy)
                if d < RC + rad and d > 1e-6:
                    nx, ny = dx / d, dy / d
                    q.p.x, q.p.y = cx + nx * (RC + rad), cy + ny * (RC + rad)
                    vn = q.v.x * nx + q.v.y * ny
                    if vn < 0: q.v.x -= 2 * vn * nx; q.v.y -= 2 * vn * ny
                    hit = 3
            if hit is not None and q.kind != 2:
                q.e *= P.get("absorb", 0.82)
                q.bounces += 1
                if q.kind == 0 and sp > 0.25 and q.bounces <= 3:
                    m = 1 + int(round(3 * sp))
                    for _ in range(m):
                        jit = Vector((r.gauss(0, 1), r.gauss(0, 1), r.gauss(0, 1))) * (0.25 + 0.5 * sp)
                        d = (q.v.normalized() + jit).normalized() * q.v.length * (0.75 + 0.3 * r.random())
                        born.append(Part(q.p.copy(), d, q.e * 0.42 / m * 1.6, q.w, 1))
                    q.e *= 0.6
            q.e *= dec
            if q.kind != 2:
                q.w += (0.92 - q.w) * dt * 0.22 * (1.0 - frz)         # los agudos se apagan antes: la cola se entibia
                if sp > 0.35 and r.random() < (sp - 0.35) * 5.0 * dt * (0.5 + q.e):
                    v = Vector((r.gauss(0, 1), r.gauss(0, 1), r.gauss(0, 1))) * 0.006
                    born.append(Part(q.p.copy(), v, min(0.5, q.e) * 0.35, q.w, 2))
        if frz > 0.3 and r.random() < frz * 40 * dt:                 # polvo suspendido
            p = Vector((r.uniform(X0, X1), r.uniform(Y0, Y1), r.uniform(F + 0.01, F + H * 0.9)))
            born.append(Part(p, Vector((0, 0, 0.002)), 0.05 + 0.08 * r.random(), r.random(), 2))
        s.parts.extend(born)
        s.parts = [q for q in s.parts if q.e > 0.006][-2400:]
        s.t += dt

def radius(q):
    if q.kind == 2: return 0.0011 + 0.0012 * min(1.0, q.e * 3)
    return 0.0022 + 0.0085 * math.sqrt(min(1.5, q.e))

# ---------------------------------------------------------------------------------------------------------------
# Escena

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    if "--cpu" not in argv:
        for kind in ("OPTIX", "CUDA", "METAL"):
            try:
                prefs.compute_device_type = kind; prefs.get_devices()
                if any(d.type == kind for d in prefs.devices):
                    for d in prefs.devices: d.use = d.type == kind
                    break
            except Exception: pass
    sc.cycles.device = "CPU" if "--cpu" in argv else "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    sc.cycles.denoising_use_gpu = "--cpu" not in argv     # en pc50-wsl, con OIDN por CPU un cuadro tarda 7-30 s
    sc.cycles.max_bounces = 6; sc.cycles.volume_bounces = 1; sc.cycles.transparent_max_bounces = 4
    sc.render.resolution_percentage = PCT
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.view_settings.exposure = -0.6                # el yeso en penumbra; las partículas compensan con GAIN
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 92
    return sc

def mat(name, base, rough, bump=0.0, sss=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough
    if sss: b.inputs["Subsurface Weight"].default_value = sss; b.inputs["Subsurface Scale"].default_value = 0.004
    if bump:                                       # el grano del yeso, apenas
        tc = nt.nodes.new("ShaderNodeTexCoord"); nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 420.0; nz.inputs["Detail"].default_value = 6.0
        bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = bump; bp.inputs["Distance"].default_value = 0.0004
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"]); nt.links.new(nz.outputs["Fac"], bp.inputs["Height"])
        nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m

def box(name, a, b, m):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2))
    o = bpy.context.active_object; o.name = name
    o.scale = (b[0] - a[0], b[1] - a[1], b[2] - a[2]); bpy.ops.object.transform_apply(scale=True)
    if m: o.data.materials.append(m)
    return o

def cut(o, cutter):
    md = o.modifiers.new("cut", "BOOLEAN"); md.operation = "DIFFERENCE"; md.object = cutter; md.solver = "EXACT"
    bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier="cut")
    bpy.data.objects.remove(cutter, do_unlink=True)

def arch_cutter(cx, cy, w, h, depth, axis):
    """Un vano de puerta con arco de medio punto, para restarle a una pared (axis 'y': pared de fondo)."""
    a = (cx - w / 2, cy - depth, F) if axis == "y" else (cx - depth, cy - w / 2, F)
    b = (cx + w / 2, cy + depth, F + h) if axis == "y" else (cx + depth, cy + w / 2, F + h)
    c = box("c", a, b, None)
    rot = (math.pi / 2, 0, 0) if axis == "y" else (math.pi / 2, 0, math.pi / 2)
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=w / 2, depth=2 * depth, location=(cx, cy, F + h), rotation=rot)
    cy_ = bpy.context.active_object
    bpy.context.view_layer.objects.active = c
    md = c.modifiers.new("u", "BOOLEAN"); md.operation = "UNION"; md.object = cy_; md.solver = "EXACT"
    bpy.ops.object.modifier_apply(modifier="u"); bpy.data.objects.remove(cy_, do_unlink=True)
    return c

def build():
    sc = reset()
    plaster = mat("plaster", (0.80, 0.78, 0.74), 0.82, bump=0.04, sss=0.06)
    card = mat("card", (0.36, 0.35, 0.33), 0.75, bump=0.05)
    table = mat("table", (0.012, 0.012, 0.013), 0.32)
    # Mesa y zócalo de cartón gris.
    bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0, -0.04)); t = bpy.context.active_object; t.data.materials.append(table)
    box("plinth", (-0.45, -0.31, -0.04), (0.45, 0.30, 0.0), card)
    box("floor", (X0 - T, Y0 - 0.015, 0.0), (X1 + T, Y1 + T, F), plaster)
    # Pared de fondo con cinco arcos pasantes (la luz de atrás entra por ahí) y las laterales con tres ventanas.
    back = box("back", (X0 - T, Y1, F), (X1 + T, Y1 + T, F + H), plaster)
    for i in range(5):
        cut(back, arch_cutter(-0.24 + 0.12 * i, Y1 + T / 2, 0.056, 0.13 if i % 2 else 0.15, 0.03, "y"))
    for sx in (-1, 1):
        x0, x1 = (X0 - T, X0) if sx < 0 else (X1, X1 + T)
        side = box("side", (x0, Y0, F), (x1, Y1, F + H), plaster)
        for j in range(3):
            c = box("c", (x0 - 0.01, -0.13 + 0.10 * j - 0.022, F + 0.06), (x1 + 0.01, -0.13 + 0.10 * j + 0.022, F + 0.19), None)
            cut(side, c)
    # Dos filas de columnas con basa, capitel y arquitrabe.
    for cx, cy in COLS:
        bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=RC, depth=H - 0.034, location=(cx, cy, F + 0.008 + (H - 0.034) / 2))
        bpy.context.active_object.data.materials.append(plaster)
        box("base", (cx - 0.017, cy - 0.017, F), (cx + 0.017, cy + 0.017, F + 0.008), plaster)
        box("cap", (cx - 0.018, cy - 0.018, F + H - 0.026), (cx + 0.018, cy + 0.018, F + H - 0.018), plaster)
    for sx in (-1, 1):
        box("beam", (sx * 0.22 - 0.016, -0.135, F + H - 0.018), (sx * 0.22 + 0.016, Y1, F + H), plaster)
    # Escalinata al fondo y la fuente sobre su pedestal.
    for k in range(3):
        box("step", (-0.13 + 0.025 * k, Y1 - 0.075 + 0.025 * k, F), (0.13 - 0.025 * k, Y1, F + 0.012 * (k + 1)), plaster)
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=0.009, depth=SRC.z - F - 0.008, location=(SRC.x, SRC.y, F + (SRC.z - F - 0.008) / 2))
    bpy.context.active_object.data.materials.append(plaster)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=0.007, location=SRC)
    core = bpy.context.active_object
    cm = bpy.data.materials.new("core"); cm.use_nodes = True
    cb = cm.node_tree.nodes["Principled BSDF"]
    cb.inputs["Base Color"].default_value = (0.9, 0.88, 0.85, 1); cb.inputs["Emission Color"].default_value = (1.0, 0.9, 0.8, 1)
    cb.inputs["Emission Strength"].default_value = 6.0; core.data.materials.append(cm)
    # Niebla: un volumen que llena el interior; su densidad sigue a Space.
    vol = box("fog", (X0, Y0 - 0.01, F), (X1, Y1, F + H + 0.02), None)
    vm = bpy.data.materials.new("fog"); vm.use_nodes = True; nt = vm.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    pv = nt.nodes.new("ShaderNodeVolumePrincipled"); pv.inputs["Anisotropy"].default_value = 0.35
    nt.links.new(pv.outputs[0], nt.nodes["Material Output"].inputs["Volume"]); vol.data.materials.append(vm)
    # Las partículas: una malla que se rearma en cada cuadro.
    pm = bpy.data.materials.new("light"); pm.use_nodes = True; nt = pm.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    at = nt.nodes.new("ShaderNodeAttribute"); at.attribute_name = "glow"
    em = nt.nodes.new("ShaderNodeEmission"); nt.links.new(at.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs[0], nt.nodes["Material Output"].inputs["Surface"])
    me = bpy.data.meshes.new("lights"); lo = bpy.data.objects.new("lights", me); sc.collection.objects.link(lo)
    me.materials.append(pm)
    # Luz: casi nada. Un rasante frío desde arriba a la izquierda para leer el yeso, y detrás de la pared del fondo
    # una caja tibia que entra por los arcos y dibuja los haces en la niebla.
    def area(name, loc, look, size, energy, color, shape="SQUARE", sy=None):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = energy; l.color = color; l.shape = shape
        if sy: l.size_y = sy
        o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat("-Z", "X").to_euler(); o.visible_camera = False; return o
    area("key", (-0.7, -0.5, 0.9), (0, 0, 0.05), 0.8, 3.2, (0.78, 0.85, 1.0))
    area("behind", (0.0, 0.40, 0.20), (0.0, 0.0, 0.08), 0.9, 5.0, (1.0, 0.84, 0.66), "RECTANGLE", 0.12)
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
    w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.004, 0.004, 0.005, 1)
    compositor(sc)
    return sc, me, vm, pv

def compositor(sc):
    """El halo de las luces (la 'halación' de la película): Glare tipo Bloom, apenas tibio."""
    ng = bpy.data.node_groups.new("comp", "CompositorNodeTree"); sc.compositing_node_group = ng
    ng.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
    rl = ng.nodes.new("CompositorNodeRLayers"); gl = ng.nodes.new("CompositorNodeGlare"); out = ng.nodes.new("NodeGroupOutput")
    gl.inputs["Type"].default_value = "Bloom"; gl.inputs["Quality"].default_value = "High"
    gl.inputs["Threshold"].default_value = 0.9; gl.inputs["Strength"].default_value = 0.55
    gl.inputs["Size"].default_value = 0.7; gl.inputs["Tint"].default_value = (1.0, 0.82, 0.72, 1.0)
    ng.links.new(rl.outputs["Image"], gl.inputs["Image"]); ng.links.new(gl.outputs["Image"], out.inputs[0])

ICO = None
def icosphere():
    global ICO
    if ICO is None:
        bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
        ICO = ([v.co.copy() for v in bm.verts], [[v.index for v in f.verts] for f in bm.faces]); bm.free()
    return ICO

def update_lights(me, sim, P, gain):
    vs, fs = icosphere()
    verts, faces, cols = [], [], []
    frz = P.get("freeze", 0.0)
    for q in sim.parts:
        r = radius(q)
        c = COLD.lerp(WARM, q.w)
        if frz > 0: c = c.lerp(FROZEN, 0.65 * frz)
        k = gain * q.e * (0.45 if q.kind == 2 else 1.0)
        sp = q.v.length
        d = q.v / sp if sp > 1e-6 else Vector((0, 0, 0))
        tail = min(1.3 * r, sp / FPS * 0.9)           # la cola: lo que recorrió en el último cuadro, corta
        base = len(verts)
        for v in vs:
            back = min(0.0, v.dot(d))
            verts.append(q.p + v * r + d * (back * tail))
        faces += [[base + i for i in f] for f in fs]
        cols += [c.x * k, c.y * k, c.z * k, 1.0] * len(vs)
    me.clear_geometry()
    me.from_pydata(verts, [], faces)
    a = me.color_attributes.new("glow", "FLOAT_COLOR", "POINT")
    if cols: a.data.foreach_set("color", cols)
    me.update()

def set_fog(vm, pv, P):
    frz = P.get("freeze", 0.0)
    pv.inputs["Density"].default_value = 0.10 + 2.6 * P["space"] ** 1.6
    col = Vector((0.92, 0.92, 0.94)).lerp(Vector((0.75, 0.68, 1.0)), frz)
    pv.inputs["Color"].default_value = (*col, 1)

# ---------------------------------------------------------------------------------------------------------------
# Tomas

def lerp(a, b, t): return a + (b - a) * t
def smooth(t): t = min(1.0, max(0.0, t)); return t * t * (3 - 2 * t)

WARMS = [0.1, 0.75, 0.3, 0.95, 0.2, 0.6, 0.05, 0.85]
def preset(space, **kw):
    P = dict(space=space, period=0.5, rays=10 + int(14 * space), speed=0.30, level=1.0,
             t60=0.9 + 5.0 * space ** 1.3, warm=WARMS, absorb=0.84 - 0.1 * space)
    P.update(kw); return P

STATES = {
    # Las fotos: (estado del plugin, segundos simulados)
    "hero":   (lambda t: preset(0.55), 6.0),
    "close":  (lambda t: preset(0.35, period=0.375), 5.0),
    "top":    (lambda t: preset(0.45, ping=True), 5.0),
    "freeze": (lambda t: preset(0.8, freeze=smooth((t - 3.5) / 1.0), period=0.5 if t < 3.5 else 0), 7.5),
    "echo":   (lambda t: preset(0.12, ping=True, period=0.375), 4.0),
}
SHOTS = {
    # nombre: (posición, punto mirado, lente mm, f, resolución, estado)
    "hero":   ((0.05, -0.98, 0.44), (0.0, 0.02, 0.07), 36, 8.0, (2400, 1100), "hero"),
    "close":  ((0.17, -0.40, 0.13), (-0.02, 0.06, 0.09), 50, 4.0, (1600, 1100), "close"),
    "top":    ((0.0, -0.05, 1.05), (0.0, 0.0, 0.0), 50, 11.0, (1400, 1400), "top"),
    "freeze": ((-0.42, -0.78, 0.40), (0.0, 0.03, 0.06), 40, 8.0, (1600, 1100), "freeze"),
    "echo":   ((0.0, -0.80, 0.22), (0.0, 0.05, 0.09), 40, 5.6, (1600, 1100), "echo"),
}

def camera(sc, loc, look, lens, fstop):
    cd = bpy.data.cameras.new("cam"); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); sc.camera = cam
    aim(cam, loc, look); return cam

def aim(cam, loc, look):
    cam.location = Vector(loc)
    d = Vector(look) - Vector(loc); cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    cam.data.dof.focus_distance = d.length

GAIN = float(arg("--gain", "130"))
SHOT_GAIN = {"close": 0.45, "film-freeze": 0.45}                    # de cerca, la fuente y los pulsos recién salidos saturan

def stills():
    for name in arg("--shots", "hero,close,top,freeze").split(","):
        sc, me, vm, pv = build()
        loc, look, lens, fstop, res, state = SHOTS[name]
        camera(sc, loc, look, lens, fstop)
        sc.render.resolution_x, sc.render.resolution_y = res
        params, secs = STATES[state]
        sim = Sim(params); sim.run_to(secs, 0.0)
        P = params(secs); update_lights(me, sim, P, GAIN * SHOT_GAIN.get(name, 1.0)); set_fog(vm, pv, P)
        print(f"[{name}] {len(sim.parts)} partículas", flush=True)
        sc.render.filepath = os.path.join(OUT, name + ".jpg")
        bpy.ops.render.render(write_still=True)

FILM = {
    # Cada toma dura 6 s (144 cuadros). Se simula desde antes para que la sala no arranque vacía.
    # echo: delay puro con ping-pong, la cámara entra despacio.
    "echo":   (lambda t: preset(0.10, ping=True, period=0.375), -1.0,
               lambda u: ((0.0, -1.02 + 0.22 * u, 0.30 - 0.06 * u), (0.0, 0.03, 0.09), 36, 8.0)),
    # space: la perilla va de Delay a Space; los rebotes se parten y la sala se llena de niebla. Órbita lenta.
    "space":  (lambda t: preset(0.08 + 0.82 * smooth(t / 5.0)), -2.0,
               lambda u: ((0.95 * math.cos(math.radians(-118 + 36 * u)), 0.95 * math.sin(math.radians(-118 + 36 * u)), 0.32),
                          (0.0, 0.02, 0.08), 36, 8.0)),
    # freeze: en el segundo 2 todo queda suspendido; después sube como un shimmer.
    "freeze": (lambda t: preset(0.75, freeze=smooth((t - 2.0) / 0.8) * (1.0 - 0.6 * smooth((t - 4.2) / 1.2)),
                                period=0.5 if t < 2.0 else 0, rise=0.05 * smooth((t - 4.2) / 1.2)), -3.0,
               lambda u: ((0.24 - 0.10 * u, -0.56 + 0.06 * u, 0.12 + 0.03 * u), (0.0, 0.05, 0.10), 45, 4.0)),
}

def film():
    shot = arg("--shot", "echo"); frames = int(arg("--frames", "144"))
    first = int(arg("--first", "1")); step = int(arg("--step", "1"))
    params, t0, path = FILM[shot]
    sc, me, vm, pv = build()
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    cam = camera(sc, (0, -1, 0.3), (0, 0, 0), 36, 8.0)
    sim = Sim(params)
    for f in range(1, frames + 1):
        t = (f - 1) / FPS
        sim.run_to(t, t0)
        if f < first or (f - first) % step: continue
        u = (f - 1) / (frames - 1)
        loc, look, lens, fstop = path(u)
        cam.data.lens = lens; cam.data.dof.aperture_fstop = fstop; aim(cam, loc, look)
        P = params(t); update_lights(me, sim, P, GAIN * SHOT_GAIN.get("film-" + shot, 1.0)); set_fog(vm, pv, P)
        sc.render.filepath = os.path.join(OUT, f"{shot}_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)

# ---------------------------------------------------------------------------------------------------------------
# Film de la web (filmkit): --mode film2 [--shot a,b] [--quality draft|final] --audio <wav> --out <dir> [--test 1,48,96]

def film2():
    """Film de la web de Halation, 24 s a 120 BPM (seis tomas de 8 tiempos, cortes en el compás). La partitura sale de
    film_audio.py, la misma que armó el sonido: cada nota es una cuenta de luz que sale de la fuente y rebota de pared
    en pared, y cada rebote cae justo en un eco del ping-pong (corchea con puntillo, izquierda / derecha, cada vez más
    tenue). Desde los 10 s el control Delay / Space sube: los rebotes se deshacen en polvo de luz y la niebla de la
    sala se enciende con la envolvente de la cola. A los 16 s un acorde abre la sala y la luz florece despacio.

      open   0–4 s   Plano general bajo desde la derecha, empuje lento y un poco de grúa hacia abajo. Las cuentas
                     cruzan la sala en el tiempo de los ecos; el tercio inferior izquierdo (la mesa oscura) queda
                     quieto para el título.
      macro  4–8 s   Macro sobre la pared derecha, 50 mm f/3.2, cámara en mano sutil y un travelling corto. Foco de
                     la columna del frente a la pared, donde pegan los ecos: cada rebote la ilumina en el tiempo.
      aisle  8–12 s  La cámara entra por la nave izquierda, entre la columnata y la pared, 24 mm; las columnas pasan
                     en primer plano y los rebotes llegan a la pared de al lado. Space empieza a subir.
      space  12–16 s Vista de héroe alta en órbita lenta; los ecos se funden en una nube y la niebla brilla. Una luz
                     rasante barre la pared del fondo y saca la textura del yeso.
      arch   16–20 s Desde atrás de la pared del fondo la cámara pasa por el arco central mientras el acorde florece:
                     la nube de luz se abre desde la fuente y llena la sala.
      end    20–24 s Se aleja despacio al plano general; la sala queda brillando con la cola (film.py la oscurece
                     y pone el cierre).

    Lo que se mueve: las cuentas (posición exacta por tiempo, con su luz puntual que ilumina el yeso y destella en
    cada rebote), las chispas de cada impacto y el polvo de la cola (simulación propia, determinista), el núcleo de
    la fuente (destello en cada nota), la niebla (densidad con Space, emisión con la envolvente del audio) y la luz
    del acorde. Todo con claves por cuadro: la cámara tiene desenfoque de movimiento real; las cuentas dibujan su
    propia estela (la malla cambia de topología en cada cuadro y no puede desenfocarse sola).
    """
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk, film_audio as FA
    only = arg("--shot"); quality = arg("--quality", "final")
    test = [int(x) for x in arg("--test").split(",")] if arg("--test") else None
    env = fk.envelope(arg("--audio"), release=0.35, start=FA.PRE)
    glow = slow(env, 0.45)                                     # la cola, lenta: para la niebla y la luz del acorde
    T = FILM_SHOTS[-1][1] + FILM_SHOTS[-1][2]
    beads = bead_plan(FA)
    snaps = film_sim_cached(FA, beads, int(round(T * FFPS)) + 2)
    for (name, t0, dur) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        sc, me, vm, pv = build()
        fk.setup(sc, quality)
        if quality == "final":                                  # con 192 muestras la sala tarda ~20 s por cuadro en la
            sc.cycles.samples = 112; sc.cycles.adaptive_threshold = 0.03   # 3090; con el denoiser 112 alcanzan
        sc.cycles.max_bounces = 8; sc.cycles.diffuse_bounces = 3; sc.cycles.volume_bounces = 1
        sc.cycles.glossy_bounces = 2; sc.cycles.transmission_bounces = 2; sc.cycles.transparent_max_bounces = 4
        sc.view_settings.exposure = EXPO
        lo = sc.objects["lights"]
        film_look(sc, lo)
        rig = film_lights(sc, name)
        cam = fk.camera(sc)
        path = FILM_PATHS[name]
        frames = int(round(dur * FFPS)); g0 = int(round(t0 * FFPS))
        cnode = bpy.data.materials["core"].node_tree.nodes["Principled BSDF"]
        pv.inputs["Emission Color"].default_value = (1.0, 0.84, 0.66, 1)
        cnode.inputs["Emission Color"].default_value = (1.0, 0.86, 0.70, 1)

        def pose(f, t, u, name=name, path=path, t0=t0):
            tt = t0 + t
            focus = None
            if name == "macro":                                 # de la columna del frente a la pared donde pegan los ecos
                focus = Vector((0.22, -0.112, 0.09)).lerp(Vector((0.335, -0.12, 0.10)), fk.rack(u, 0.30, 0.70, 0.0, 1.0))
            elif name in ("open", "space", "end"): focus = Vector((0.0, -0.04, 0.08))
            elif name == "arch": focus = SRC + Vector((0, 0, 0.02))
            shake = {"macro": 0.0010, "aisle": 0.0006, "arch": 0.0005}.get(name, 0.0)
            path.apply(cam, u, focus=focus, shake=shake, t=tt)
            moved = []
            for k, L in enumerate(rig["beads"]):                # luces de las cuentas: un lugar fijo por nota (i % 8)
                b = bead_light(beads, k, tt, FA)
                if b: L.location = b[0].lerp(Vector((0.0, b[0].y, b[0].z)), min(1.0, 0.018 / max(0.02, abs(b[0].x))))   # un poco hacia adentro: ilumina una mancha de pared, no un punto
                moved.append(L)
            sw = rig["sweep"]                                   # el barrido rasante sobre la pared del fondo
            us = fk.ease((tt - 12.3) / 3.6, "smooth")
            sw.location = Vector((-0.55 + 1.1 * us, -0.10, 0.34))
            sw.rotation_euler = (Vector((-0.30 + 0.60 * us, 0.22, 0.10)) - sw.location).to_track_quat("-Z", "Y").to_euler()
            moved.append(sw)
            return moved

        def keys(f, t0=t0):
            tt = t0 + (f - 1) / FFPS
            gi = min(len(env) - 1, int(round(tt * FFPS)))
            for k, L in enumerate(rig["beads"]):
                b = bead_light(beads, k, tt, FA)
                if b:
                    _, lvl, w, fl = b
                    c = COLD.lerp(WARM, w)
                    L.data.energy = BEAD_W * lvl * (1.0 + 4.0 * fl); L.data.color = (c.x, c.y, c.z)
                else: L.data.energy = 0.0
                L.data.keyframe_insert("energy"); L.data.keyframe_insert("color")
            sp = FA.space_at(tt) / 100.0
            gl = glow[gi] if gi < len(glow) else 0.0
            bl = bloom_env(tt, FA)
            rig["bloom"].data.energy = BLOOM_W * bl; rig["bloom"].data.keyframe_insert("energy")
            rig["sweep"].data.energy = 1.6 * math.sin(math.pi * fk.ease((tt - 12.3) / 3.6, "linear")) ** 2 if 12.3 < tt < 15.9 else 0.0
            rig["sweep"].data.keyframe_insert("energy")
            pv.inputs["Density"].default_value = 0.05 + 0.32 * sp ** 1.6
            pv.inputs["Emission Strength"].default_value = FOG_EM * gl * (0.1 + 0.9 * sp) + 0.3 * FOG_EM * bl
            pv.inputs["Color"].default_value = (0.93, 0.92, 0.94, 1)
            for s_ in ("Density", "Emission Strength"): pv.inputs[s_].keyframe_insert("default_value")
            fl = note_flash(tt, FA)
            cnode.inputs["Emission Strength"].default_value = 0.6 + CORE_FLASH * fl + 3.0 * bl
            cnode.inputs["Emission Strength"].keyframe_insert("default_value")

        def frame(f, t0=t0):
            g = min(len(snaps) - 1, int(round(t0 * FFPS)) + f - 1)
            draw_film(me, snaps[g], (t0 * FFPS + f - 1) / FFPS, beads, FA)

        render_film(sc, name, frames, pose, keys, frame, OUT, test)


EXPO = float(arg("--expo", "-0.35"))
BEAD_W, BLOOM_W, FOG_EM, CORE_FLASH = 0.30, 0.6, 0.16, 4.0
BR = 0.0055                                                # radio de una cuenta de luz
LANES_Y = (-0.17, -0.075)                                  # carriles libres de columnas y de ventanas
FILM_SHOTS = [("open", 0.0, 4.0), ("macro", 4.0, 4.0), ("aisle", 8.0, 4.0), ("space", 12.0, 4.0),
              ("arch", 16.0, 4.0), ("end", 20.0, 4.0)]
FFPS = 24
NBEAD = 8                                                  # luces de cuentas a la vez (una cuenta vive < 4 s)


def _paths():
    import filmkit as fk
    return {
        "open":  fk.Path([(0, (0.48, -1.18, 0.27), (-0.03, 0.02, 0.040), 40, 6.3),
                          (1, (0.37, -0.97, 0.22), (-0.03, 0.02, 0.050), 40, 6.3)]),
        "macro": fk.Path([(0, (0.075, -0.330, 0.060), (0.34, -0.115, 0.100), 50, 3.2),
                          (1, (0.105, -0.315, 0.066), (0.34, -0.100, 0.100), 50, 3.2)]),
        "aisle": fk.Path([(0, (-0.290, -0.36, 0.080), (-0.02, -0.05, 0.085), 24, 5.6),
                          (0.5, (-0.287, -0.17, 0.090), (0.08, 0.02, 0.090), 24, 5.6),
                          (1, (-0.282, -0.055, 0.105), (0.25, 0.04, 0.085), 24, 5.6)]),
        "space": fk.Path([(0, (-0.62, -0.78, 0.50), (0.0, 0.0, 0.06), 36, 8.0),
                          (1, (-0.30, -0.95, 0.44), (0.02, 0.0, 0.07), 36, 8.0)]),
        "arch":  fk.Path([(0, (0.0, 0.46, 0.112), (0.0, -0.09, 0.090), 30, 5.6),
                          (0.55, (0.0, 0.215, 0.095), (0.0, -0.09, 0.088), 28, 5.6),
                          (1, (0.0, 0.11, 0.090), (0.0, -0.09, 0.085), 26, 5.6)]),
        "end":   fk.Path([(0, (0.0, -0.62, 0.30), (0.0, 0.0, 0.07), 38, 8.0),
                          (1, (0.0, -1.08, 0.44), (0.0, 0.0, 0.06), 38, 8.0)], easing="out"),
    }


class _LazyPaths(dict):
    def __missing__(self, k):
        self.update(_paths()); return dict.__getitem__(self, k)
FILM_PATHS = _LazyPaths()


def slow(env, tau):
    """Seguidor lento (ataque y caída de tau s) de una envolvente por cuadro."""
    a = math.exp(-1.0 / (tau * FFPS)); y, out = 0.0, []
    for x in env: y = a * y + (1 - a) * x; out.append(y)
    m = max(out) if out else 1.0
    return [v / m for v in out]


def bloom_env(t, FA):
    """La luz del acorde: florece en ~1,2 s y se apaga con la cola larga."""
    d = t - FA.CHORD_T
    if d < 0: return 0.0
    return (1 - math.exp(-d / 0.55)) ** 2 * math.exp(-d / 5.5)


def note_flash(t, FA):
    """Destello del núcleo en cada nota (la seca, en la fuente)."""
    v = 0.0
    for (tn, c, k, chord) in FA.notes():
        d = t - tn
        if 0 <= d < 0.6: v = max(v, math.exp(-d / (0.25 if chord else 0.07)) * (1.6 if chord else 1.0))
    return v


def bead_plan(FA):
    """Por nota (salvo el acorde): (t, lado, carril y, altura z, calidez). Graves tibios, agudos fríos."""
    r = random.Random(11); plan = []
    for i, (tn, c, k, chord) in enumerate(FA.notes()):
        if chord: continue
        y = LANES_Y[r.random() < 0.5] + r.uniform(-0.005, 0.005)
        z = r.uniform(0.040, 0.165)
        w = min(1.0, max(0.0, 0.88 - 0.08 * k + 0.05 * (c - 1.5)))
        plan.append((tn, FA.side(i), y, z, w))
    return plan


NLEG = 6                                                   # una cuenta rebota seis veces (0,55^6 ≈ 3 %)

def bead_at(b, t, FA):
    """Estado de una cuenta en t: (posición, velocidad, nivel, calidez, destello) o None."""
    tn, s, y, z, w = b
    if t < tn: return None
    x = (t - tn) / FA.ECHO; leg = int(x); u = x - leg
    if leg >= NLEG: return None
    def wall(j): return Vector((s * (1 if j % 2 else -1) * (X1 - BR), y, z))
    a = SRC.copy() if leg == 0 else wall(leg); b_ = wall(leg + 1)
    p = a.lerp(b_, u)
    if leg: p.z += 0.008 * math.sin(math.pi * u)            # un arco apenas, como un rebote
    v = (b_ - a) / FA.ECHO
    sp = FA.space_at(t) / 100.0
    lvl = FA.FB ** leg * max(0.0, 1.0 - sp) ** 1.4           # con Space alto la cuenta se deshace en polvo
    if leg == NLEG - 1: lvl *= 1.0 - u
    fl = math.exp(-u * FA.ECHO / 0.09)                      # destello al salir y en cada rebote
    return p, v, lvl, w, fl


def bead_light(beads, k, t, FA):
    """La cuenta que ocupa la luz k en t: la nota i con i % NBEAD == k y viva."""
    best = None
    for i in range(k, len(beads), NBEAD):
        st = bead_at(beads[i], t, FA)
        if st: best = (st[0], st[2], st[3], st[4])
    return best


class Mote:
    __slots__ = ("p", "v", "e", "w", "kind", "tau", "drag", "rise", "r")
    def __init__(s, p, v, e, w, kind, tau, drag, rise, r):
        s.p, s.v, s.e, s.w, s.kind, s.tau, s.drag, s.rise, s.r = p, v, e, w, kind, tau, drag, rise, r


def _hemi(rng, n):
    """Dirección al azar en la semiesfera de la normal n."""
    while True:
        d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1)))
        if d.length > 1e-6: break
    d.normalize()
    if d.dot(n) < 0: d -= 2 * d.dot(n) * n
    return (d + n * 0.4).normalized()


def film_sim(FA, beads, nframes):
    """Chispas de cada impacto y polvo de la cola, con paso de 1/48 s desde −PRE. Devuelve, por cuadro del film, la
    lista de motas (p, dirección·velocidad, nivel, calidez, clase, radio)."""
    rng = random.Random(23); dt = 1.0 / 48.0
    ms, snaps = [], []
    events = []                                             # (t, tipo, datos)
    for b in beads:
        events.append((b[0], "note", b))
        for j in range(1, NLEG + 1):
            te = b[0] + j * FA.ECHO; s = b[1] * (1 if j % 2 else -1)
            events.append((te, "hit", (b, j, s)))
    for k in range(20): events.append((FA.CHORD_T + 0.05 * k, "bloom", k / 19.0))
    events.sort(key=lambda e: e[0]); ei = 0
    t = -FA.PRE; g = 0
    while g < nframes:
        while ei < len(events) and events[ei][0] <= t:
            te, kind, data = events[ei]; ei += 1
            sp = FA.space_at(te) / 100.0
            if kind == "note":
                b = data
                for _ in range(5):
                    d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), abs(rng.gauss(0, 1)))).normalized()
                    ms.append(Mote(SRC + d * 0.009, d * rng.uniform(0.05, 0.12), 0.7, b[4], 1, 0.10, 3.0, 0.0, 0.0016))
            elif kind == "hit":
                b, j, s = data
                lvl = FA.FB ** (j - 1)                          # el nivel con que llega a la pared
                melt = max(0.0, 1.0 - sp) ** 1.4
                pos = Vector((s * (X1 - 0.002), b[2], b[3]))
                n = Vector((-s, 0, 0))
                for _ in range(int(4 + 6 * sp)):                # chispas cortas
                    d = _hemi(rng, n)
                    ms.append(Mote(pos.copy(), d * rng.uniform(0.06, 0.16), lvl * (0.25 + 0.6 * melt) * rng.uniform(0.6, 1.0),
                                   b[4], 1, 0.14 + 0.5 * sp, 2.5, 0.0, 0.0014))
                nd = int(round((50 * sp + 4 * (1 - sp)) * min(1.0, lvl * 1.6)))   # polvo que queda: la cola
                for _ in range(nd):
                    d = _hemi(rng, n)
                    ms.append(Mote(pos + d * rng.uniform(0.0, 0.02), d * rng.uniform(0.03, 0.11), (0.12 + 0.32 * sp) * min(1.0, lvl * 1.5) * rng.uniform(0.5, 1.0),
                                   b[4], 2, 0.6 + 3.5 * sp, 0.55, 0.0035, rng.uniform(0.0008, 0.0016)))
            elif kind == "bloom":
                # El acorde: una flor de luz que sale de la fuente en oleadas durante ~1 s, cada vez más lenta.
                for q in range(36):
                    d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 0.8) + 0.25)).normalized()
                    spd = rng.uniform(0.03, 0.20) * (1.0 - 0.6 * data)
                    ms.append(Mote(SRC + d * rng.uniform(0.005, 0.02), d * spd, rng.uniform(0.35, 1.0), rng.uniform(0.25, 0.95),
                                   3, rng.uniform(3.0, 7.0), rng.uniform(0.5, 1.1), 0.006, rng.uniform(0.0010, 0.0022)))
        for q in ms:
            q.v *= math.exp(-q.drag * dt)
            if q.kind >= 2:
                q.v.z += q.rise * dt
                q.v += Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))) * 0.0025 * dt * 48 * 0.15
            q.p += q.v * dt
            for ax, lo_, hi in ((0, X0, X1), (1, Y0 - 0.02, Y1), (2, F, F + H + 0.03)):
                if q.p[ax] < lo_ + q.r: q.p[ax] = lo_ + q.r; q.v[ax] = abs(q.v[ax]) * 0.5
                elif q.p[ax] > hi - q.r: q.p[ax] = hi - q.r; q.v[ax] = -abs(q.v[ax]) * 0.5
            for cx, cy in COLS:
                dx, dy = q.p.x - cx, q.p.y - cy; d = math.hypot(dx, dy)
                if 1e-6 < d < RC + q.r:
                    q.p.x, q.p.y = cx + dx / d * (RC + q.r), cy + dy / d * (RC + q.r)
            q.e *= math.exp(-dt / q.tau)
            if q.kind >= 2: q.w += (0.90 - q.w) * dt * 0.12      # la cola se entibia
        ms = [q for q in ms if q.e > 0.01][-3500:]
        t += dt
        while g < nframes and t >= g / FFPS - 1e-9:
            a = array("f")                                    # plano y compacto: 10 números por mota
            for q in ms: a.extend((q.p.x, q.p.y, q.p.z, q.v.x, q.v.y, q.v.z, q.e, q.w, q.kind, q.r))
            snaps.append(a); g += 1
    return snaps


def film_sim_cached(FA, beads, nframes):
    """film_sim tarda unos minutos en Python: se guarda junto a los cuadros, con la firma del código que la produce."""
    import hashlib, inspect, pickle, time
    src = inspect.getsource(film_sim) + inspect.getsource(Mote) + inspect.getsource(_hemi) + repr(beads) + repr(nframes)
    src += open(FA.__file__).read()
    h = hashlib.sha1(src.encode()).hexdigest()[:12]
    path = os.path.join(OUT, f".sim-{h}.pkl")
    if os.path.exists(path):
        with open(path, "rb") as fh: return pickle.load(fh)
    t0 = time.time(); print("[film2] simulando…", flush=True)
    snaps = film_sim(FA, beads, nframes)
    print(f"[film2] simulación: {time.time() - t0:.0f} s", flush=True)
    with open(path, "wb") as fh: pickle.dump(snaps, fh)
    return snaps


_ICO = {}
def _ico(sub):
    if sub not in _ICO:
        bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=1.0)
        _ICO[sub] = ([v.co.copy() for v in bm.verts], [[v.index for v in f.verts] for f in bm.faces]); bm.free()
    return _ICO[sub]


GAIN_BEAD, GAIN_MOTE = float(arg("--gain", "7.0")), 3.4

def draw_film(me, motes, t, beads, FA):
    """Rearma la malla de luces: cuentas (cabeza brillante y estela que se apaga) y motas."""
    verts, faces, cols = [], [], []
    def put(p, v, r, c, k, sub, tail_k):
        vs, fs = _ico(sub)
        sp = v.length; d = v / sp if sp > 1e-6 else Vector((0, 0, 0))
        tail = min(tail_k * r * 9, sp / FFPS * 0.55)         # lo que recorre en medio cuadro (obturador de 180°)
        base = len(verts)
        for vv in vs:
            back = min(0.0, vv.dot(d))
            verts.append(p + vv * r + d * (back * tail))
            kk = k * (1.0 + 0.8 * back) if tail > r else k      # la estela se apaga hacia atrás
            cols.extend((c.x * kk, c.y * kk, c.z * kk, 1.0))
        faces.extend([[base + i for i in f] for f in fs])
    for b in beads:
        st = bead_at(b, t, FA)
        if not st or st[2] < 0.01: continue
        p, v, lvl, w, fl = st
        c = COLD.lerp(WARM, w)
        r = BR * (0.7 + 0.3 * lvl ** 0.5); k = min(GAIN_BEAD * (lvl ** 0.7) * (1 + 0.6 * fl), 11.0)
        put(p, v, r, c, k, 2, 1.0)
        put(p, v, r * 3.0, c, k * 0.07, 2, 0.4)                # halo tenue alrededor (la luz es aditiva)
    for i in range(0, len(motes), 10):
        px, py, pz, vx, vy, vz, e, w, kind, r = motes[i:i + 10]
        p, v, kind = Vector((px, py, pz)), Vector((vx, vy, vz)), int(round(kind))
        c = COLD.lerp(WARM, w)
        k = GAIN_MOTE * e * (1.5 if kind == 1 else 1.0)
        put(p, v, r, c, min(k, 6.0), 1, 0.5 if kind == 1 else 0.0)
    me.clear_geometry(); me.from_pydata(verts, [], faces)
    old = me.color_attributes.get("glow")
    if old: me.color_attributes.remove(old)
    a = me.color_attributes.new("glow", "FLOAT_COLOR", "POINT")
    if cols: a.data.foreach_set("color", cols)
    me.update()


def plaster_film(m):
    """El yeso de cerca: manchas suaves de color, grano fino, poros y una ondulación de espátula; los rincones apenas
    más oscuros (oclusión). Sólo para el film: las fotos usan el material de build()."""
    nt = m.node_tree; N, Lk = nt.nodes, nt.links
    b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord")
    def noise(scale, detail, rough=0.55):
        n = N.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = scale; n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough; Lk.new(tc.outputs["Object"], n.inputs["Vector"]); return n
    mott = noise(6.0, 3.0)
    mix = N.new("ShaderNodeMix"); mix.data_type = "RGBA"
    mix.inputs["A"].default_value = (0.74, 0.72, 0.685, 1); mix.inputs["B"].default_value = (0.83, 0.815, 0.78, 1)
    Lk.new(mott.outputs["Fac"], mix.inputs["Factor"])
    ao = N.new("ShaderNodeAmbientOcclusion"); ao.inputs["Distance"].default_value = 0.025; ao.samples = 4
    Lk.new(mix.outputs["Result"], ao.inputs["Color"])
    dirt = N.new("ShaderNodeMix"); dirt.data_type = "RGBA"; dirt.blend_type = "MULTIPLY"
    dirt.inputs["Factor"].default_value = 1.0
    mr = N.new("ShaderNodeMapRange"); mr.inputs["To Min"].default_value = 0.80; mr.inputs["To Max"].default_value = 1.0
    Lk.new(ao.outputs["AO"], mr.inputs["Value"])
    Lk.new(mix.outputs["Result"], dirt.inputs["A"]); Lk.new(mr.outputs["Result"], dirt.inputs["B"])
    Lk.new(dirt.outputs["Result"], b.inputs["Base Color"])
    # Relieve: grano fino + poros (Voronoi) + ondulación ancha, encadenados.
    grain = noise(380.0, 8.0, 0.6)
    vor = N.new("ShaderNodeTexVoronoi"); vor.inputs["Scale"].default_value = 240.0
    Lk.new(tc.outputs["Object"], vor.inputs["Vector"])
    pit = N.new("ShaderNodeMapRange"); pit.inputs["From Min"].default_value = 0.0; pit.inputs["From Max"].default_value = 0.12
    pit.inputs["To Min"].default_value = 0.0; pit.inputs["To Max"].default_value = 1.0
    Lk.new(vor.outputs["Distance"], pit.inputs["Value"])
    wave_ = noise(22.0, 2.0, 0.5)
    prev = None
    for src, out, strength, dist in ((wave_, "Fac", 0.06, 0.0012), (grain, "Fac", 0.10, 0.00025), (pit, "Result", 0.20, 0.00035)):
        bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = strength; bp.inputs["Distance"].default_value = dist
        Lk.new(src.outputs[out], bp.inputs["Height"])
        if prev: Lk.new(prev.outputs["Normal"], bp.inputs["Normal"])
        prev = bp
    for l in list(b.inputs["Normal"].links): Lk.remove(l)
    Lk.new(prev.outputs["Normal"], b.inputs["Normal"])
    rr = N.new("ShaderNodeMapRange"); rr.inputs["To Min"].default_value = 0.80; rr.inputs["To Max"].default_value = 0.92
    Lk.new(grain.outputs["Fac"], rr.inputs["Value"]); Lk.new(rr.outputs["Result"], b.inputs["Roughness"])
    b.inputs["Subsurface Weight"].default_value = 0.10; b.inputs["Subsurface Scale"].default_value = 0.003
    b.inputs["Subsurface Radius"].default_value = (1.0, 0.8, 0.6)


def film_look(sc, lo):
    """Ajustes de la escena para el film: biseles en el yeso (los cantos toman luz), yeso con textura y las luces
    de partículas sólo visibles para la cámara y los reflejos (lo que ilumina el yeso son las luces puntuales)."""
    m = bpy.data.materials["plaster"]; plaster_film(m)
    for o in sc.objects:
        if o.type == "MESH" and o.data.materials and o.data.materials[0] == m:
            bv = o.modifiers.new("bevel", "BEVEL"); bv.width = 0.0009; bv.segments = 2; bv.limit_method = "ANGLE"
            bv.angle_limit = math.radians(40); bv.harden_normals = False
    # Las luces suman luz y no tapan: emisión + transparencia (una mota tenue es un brillo tenue, no una mancha).
    nt = bpy.data.materials["light"].node_tree
    em = [n for n in nt.nodes if n.bl_idname == "ShaderNodeEmission"][0]
    tr = nt.nodes.new("ShaderNodeBsdfTransparent"); ad = nt.nodes.new("ShaderNodeAddShader")
    # Borde suave: la emisión cae hacia el contorno de cada esfera (sin el polígono del halo).
    lw = nt.nodes.new("ShaderNodeLayerWeight"); lw.inputs["Blend"].default_value = 0.5
    inv = nt.nodes.new("ShaderNodeMath"); inv.operation = "SUBTRACT"; inv.inputs[0].default_value = 1.0
    nt.links.new(lw.outputs["Facing"], inv.inputs[1])
    pw = nt.nodes.new("ShaderNodeMath"); pw.operation = "POWER"; pw.inputs[1].default_value = 2.0
    nt.links.new(inv.outputs[0], pw.inputs[0]); nt.links.new(pw.outputs[0], em.inputs["Strength"])
    nt.links.new(em.outputs[0], ad.inputs[0]); nt.links.new(tr.outputs[0], ad.inputs[1])
    nt.links.new(ad.outputs[0], nt.nodes["Material Output"].inputs["Surface"])
    lo.visible_diffuse = False; lo.visible_shadow = False; lo.visible_volume_scatter = False; lo.visible_transmission = False
    try: lo.cycles.use_motion_blur = False
    except AttributeError: pass
    # El halo: sólo lo que de verdad brilla (las luces), no el yeso iluminado.
    for n in sc.compositing_node_group.nodes:
        if n.bl_idname == "CompositorNodeGlare":
            n.inputs["Threshold"].default_value = GLARE[0]; n.inputs["Strength"].default_value = GLARE[1]
            n.inputs["Size"].default_value = GLARE[2]
    # La mesa: negra, con un brillo satinado que refleja la maqueta.
    t = bpy.data.materials["table"].node_tree.nodes["Principled BSDF"]
    t.inputs["Base Color"].default_value = (0.010, 0.010, 0.011, 1); t.inputs["Roughness"].default_value = 0.36


def film_lights(sc, shot=""):
    """Estudio oscuro: rasante frío desde la izquierda (lee la textura), relleno grande y tenue, contraluz tibio por los
    arcos, el barrido y una luz por cuenta. Reemplaza las dos luces de build()."""
    for n in ("key", "behind"):
        if n in sc.objects: bpy.data.objects.remove(sc.objects[n], do_unlink=True)
    def area(name, loc, look, size, energy, color, sy=None):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = energy; l.color = color
        if sy: l.shape = "RECTANGLE"; l.size_y = sy
        o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); o.visible_camera = False
        return o
    area("key", (-0.95, -0.45, 0.62), (0.0, 0.02, 0.06), 0.45, KEY_W, (0.86, 0.91, 1.0))
    area("fill", (0.9, -1.3, 0.55), (0.0, 0.0, 0.08), 1.6, FILL_W, (1.0, 0.95, 0.9))
    # En "arch" la cámara mira la cara de afuera de la pared del fondo: el contraluz baja para no quemarla.
    area("behind", (0.0, 0.42, 0.22), (0.0, 0.0, 0.07), 0.9, BEHIND_W * (0.04 if shot == "arch" else 1.0), (1.0, 0.80, 0.58), sy=0.12)
    area("top", (0.0, -0.05, 0.9), (0.0, 0.0, 0.0), 0.7, TOP_W, (0.92, 0.94, 1.0))
    sweep = area("sweep", (-0.55, -0.10, 0.34), (-0.3, 0.22, 0.10), 0.04, 0.0, (1.0, 0.86, 0.70), sy=0.30)
    beads = []
    for k in range(NBEAD):
        l = bpy.data.lights.new(f"bead{k}", "POINT"); l.energy = 0.0; l.shadow_soft_size = 0.010
        o = bpy.data.objects.new(f"bead{k}", l); o.location = SRC; sc.collection.objects.link(o); o.visible_camera = False
        beads.append(o)
    l = bpy.data.lights.new("bloom", "POINT"); l.energy = 0.0; l.shadow_soft_size = 0.05; l.color = (1.0, 0.82, 0.62)
    bloom = bpy.data.objects.new("bloom", l); bloom.location = SRC + Vector((0, 0, 0.035)); sc.collection.objects.link(bloom)
    bloom.visible_camera = False
    return dict(beads=beads, sweep=sweep, bloom=bloom)


KEY_W, FILL_W, BEHIND_W, TOP_W = 2.4, 0.9, 4.0, 0.5
GLARE = (1.4, 0.5, 0.75)                                   # umbral, fuerza, tamaño


def render_film(sc, name, frames, pose, keys, frame, out, test=None):
    """Como fk.render_shot, pero cuadro por cuadro: graba las claves de toda la toma (cámara, luces, materiales: el
    desenfoque de movimiento sale de ahí) y después rearma la malla de partículas antes de cada cuadro, porque su
    geometría cambia de un cuadro al otro y no se puede grabar con claves."""
    import filmkit as fk
    os.makedirs(out, exist_ok=True)
    for o in sc.objects: o.animation_data_clear()
    for f in range(1, frames + 1):
        sc.frame_set(f)
        t, u = (f - 1) / FFPS, (f - 1) / max(1, frames - 1)
        moved = pose(f, t, u) or []
        for o in set([sc.camera] + list(moved)): fk._key(o)
        keys(f)
    for o in sc.objects:
        ad = o.animation_data
        if ad and ad.action:
            for fc in fk._fcurves(ad.action):
                for kp in fc.keyframe_points: kp.interpolation = "LINEAR"
    sc.frame_start, sc.frame_end = 1, frames
    sc.render.use_persistent_data = True
    for f in (test or range(1, frames + 1)):
        sc.frame_set(f); frame(f)
        sc.render.filepath = os.path.join(out, f"{name}_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)
        print(f"[{name}] {f}/{frames}", flush=True)


mode = arg("--mode", "stills")
if mode == "film": film()
elif mode == "film2": film2()
else: stills()
