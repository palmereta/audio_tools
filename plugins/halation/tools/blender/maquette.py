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
Cuadros en JPEG (C: de pc50 anda justo de espacio). Opciones: --pct 50 (pruebas), --shots hero,close,
--first N --step N, --cpu (render por CPU, sin lock: para cuando el estudio de generación usa la GPU).
Unidades: 1 = 1 m. La maqueta mide 0,9 x 0,6 m; el frente abierto mira hacia -y.
"""
import bpy, bmesh, math, os, sys, random
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

if arg("--mode", "stills") == "film": film()
else: stills()
