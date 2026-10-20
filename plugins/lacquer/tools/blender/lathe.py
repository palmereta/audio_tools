"""El plato de un torno de corte con una laca a medio cortar, para la web de Lacquer.

La laca es negra y brillante; el surco es una espiral cuya separación y profundidad siguen el envolvente real del
master de demostración (demo_envelope.json, generado por tools/demo_mix.py + lacquerrender), así que las partes
fuertes de la canción se ven como bandas más anchas y oscuras. El cabezal de corte entra desde la derecha con su
luz roja encendida y la viruta que sale del estilete.

Corre en pc50-wsl (en ~/lacquer-render, siempre bajo el lock de GPU compartido):
  B=~/ai/blender/blender-5.2.2-linux-x64/blender; L=~/ai/prerendered/.gpu.lock
  flock $L $B -b -P lathe.py -- --mode stills --out out --samples 256
  for s in orbit macro top; do flock $L $B -b -P lathe.py -- --mode film --shot $s --frames 216 --out film --samples 64; done
Film de la web (filmkit, torno más detallado, ver film2()):
  flock $L $B -b --factory-startup -P lathe.py -- --mode film2 --quality draft|final [--shot open,scope] [--probe 36]
      --audio <balanced.wav> --out <dir>      y después: python3 scripts/film.py lacquer <dir>
Cuadros en JPEG (C: de pc50 anda justo de espacio). Opciones: --pct 50 (pruebas), --first N --step N, --cpu (render
por CPU, sin lock: para cuando el estudio de generación está usando la GPU).
Unidades: 1 = 1 m. El disco está centrado en el origen, el cabezal hacia +x.
"""
import bpy, math, os, sys, json
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "160"))
PCT = int(arg("--pct", "100"))
FPS = 24.0
HERE = os.path.dirname(os.path.abspath(__file__))

R_OUT, R_IN, R_CUT = 0.146, 0.062, 0.098       # borde del surco, final de la canción, posición actual del estilete
RPM = 100.0 / 3.0                              # 33 1/3

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    try:
        prefs.compute_device_type = "CUDA"; prefs.get_devices()
        for d in prefs.devices: d.use = d.type == "CUDA"
    except Exception: pass
    sc.cycles.device = "CPU" if "--cpu" in argv else "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    sc.cycles.denoising_use_gpu = "--cpu" not in argv
    sc.render.resolution_percentage = PCT
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 92
    return sc

def mat(name, base, rough, metal=0.0, coat=0.0, aniso=0.0, emit=None, strength=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.03
    if aniso:
        b.inputs["Anisotropic"].default_value = aniso
        t = m.node_tree.nodes.new("ShaderNodeTangent"); t.direction_type = "RADIAL"; t.axis = "Z"
        m.node_tree.links.new(t.outputs["Tangent"], b.inputs["Tangent"])
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    return m

def envelope_image():
    """Envolvente (0..1) como imagen de 1 fila: x = tramo del surco (0 = borde, 1 = final de la canción)."""
    env = json.load(open(os.path.join(HERE, "demo_envelope.json")))["db"]
    mx = max(env)
    lv = [min(1.0, max(0.0, (v - (mx - 36.0)) / 36.0)) for v in env]
    img = bpy.data.images.new("envelope", len(lv), 1, float_buffer=True)
    px = []
    for v in lv: px += [v, v, v, 1.0]
    img.pixels = px
    return img

def lacquer_material():
    """Laca: negro con barniz brillante; en la zona cortada, un relieve de espiral modulado por la canción y un
    brillo anisotrópico circular (la 'estrella' de luz de un disco)."""
    m = bpy.data.materials.new("lacquer"); m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; L = nt.links
    b = N["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.008, 0.008, 0.010, 1)
    b.inputs["Roughness"].default_value = 0.18
    b.inputs["Coat Weight"].default_value = 1.0; b.inputs["Coat Roughness"].default_value = 0.02
    tc = N.new("ShaderNodeTexCoord")
    sep = N.new("ShaderNodeSeparateXYZ"); L.new(tc.outputs["Object"], sep.inputs[0])
    def op(kind, a, bb=None, val=None):
        n = N.new("ShaderNodeMath"); n.operation = kind
        if isinstance(a, float): n.inputs[0].default_value = a
        else: L.new(a, n.inputs[0])
        if bb is not None:
            if isinstance(bb, float): n.inputs[1].default_value = bb
            else: L.new(bb, n.inputs[1])
        return n.outputs[0]
    r = op("SQRT", op("ADD", op("MULTIPLY", sep.outputs[0], sep.outputs[0]), op("MULTIPLY", sep.outputs[1], sep.outputs[1])))
    th = op("ARCTAN2", sep.outputs[1], sep.outputs[0])
    # Tramo del surco (0 en el borde, 1 al final de la canción) → nivel desde la imagen.
    u = op("DIVIDE", op("SUBTRACT", R_OUT, r), R_OUT - R_IN)
    comb = N.new("ShaderNodeCombineXYZ"); L.new(u, comb.inputs[0]); comb.inputs[1].default_value = 0.5
    tex = N.new("ShaderNodeTexImage"); tex.image = envelope_image(); tex.extension = "EXTEND"; tex.interpolation = "Linear"
    L.new(comb.outputs[0], tex.inputs[0])
    level = tex.outputs["Color"]
    # Espiral: fase = (R_OUT − r)/paso + θ/2π. El paso visible (0.55 mm) es una exageración del real.
    pitch = 0.00055
    phase = op("ADD", op("DIVIDE", op("SUBTRACT", R_OUT, r), pitch), op("DIVIDE", th, 2 * math.pi))
    fr = op("FRACT", phase)
    # Perfil en V cuyo ancho crece con el nivel (las partes fuertes dejan menos "tierra" entre surcos).
    lvl = N.new("ShaderNodeRGBToBW"); L.new(level, lvl.inputs[0])
    width = op("ADD", 0.35, op("MULTIPLY", lvl.outputs[0], 0.55))
    v = op("ABSOLUTE", op("SUBTRACT", fr, 0.5))
    groove = op("MAXIMUM", op("SUBTRACT", op("DIVIDE", width, 2.0), v), 0.0)
    # Sólo en la zona cortada (entre el estilete y el borde) y fuera de la etiqueta.
    cut = op("MULTIPLY", op("GREATER_THAN", r, R_CUT), op("LESS_THAN", r, R_OUT))
    h = op("MULTIPLY", groove, cut)
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 1.0; bump.inputs["Distance"].default_value = 0.0009
    L.new(h, bump.inputs["Height"])
    L.new(bump.outputs["Normal"], b.inputs["Normal"])
    L.new(bump.outputs["Normal"], b.inputs["Coat Normal"])          # el barniz es la superficie cortada
    # Brillo anisotrópico circular en la zona cortada.
    b.inputs["Anisotropic"].default_value = 0.0
    t = N.new("ShaderNodeTangent"); t.direction_type = "RADIAL"; t.axis = "Z"
    L.new(t.outputs["Tangent"], b.inputs["Tangent"])
    an = op("MULTIPLY", cut, 0.85); L.new(an, b.inputs["Anisotropic"])
    rough = op("ADD", 0.12, op("MULTIPLY", cut, 0.16)); L.new(rough, b.inputs["Roughness"])
    return m

def cylinder(name, r, depth, z, m, verts=256):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=(0, 0, z))
    o = bpy.context.active_object; o.name = name; o.data.materials.append(m)
    bpy.ops.object.shade_smooth()
    return o

def build():
    sc = reset()
    world = bpy.data.worlds.new("w"); sc.world = world; world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.012, 0.013, 0.012, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
    enamel = mat("enamel", (0.14, 0.15, 0.125), 0.62)
    alu = mat("alu", (0.80, 0.80, 0.78), 0.22, metal=1.0, aniso=0.6)
    dark = mat("dark", (0.025, 0.025, 0.028), 0.35, metal=0.6)
    chrome = mat("chrome", (0.9, 0.9, 0.9), 0.06, metal=1.0)
    red = mat("lamp", (0.6, 0.05, 0.03), 0.3, emit=(1.0, 0.12, 0.05), strength=30.0)
    # Cubierta del torno (esmalte gris verdoso), con una ranura alrededor del plato.
    bpy.ops.mesh.primitive_plane_add(size=4.0, location=(0.15, 0, -0.012)); deck = bpy.context.active_object; deck.data.materials.append(enamel)
    cylinder("well", 0.172, 0.004, -0.0115, dark)
    cylinder("platter", 0.160, 0.016, -0.004, alu)
    lac = cylinder("lacquer", 0.1524, 0.0016, 0.0048, lacquer_material(), verts=512)
    cylinder("core", 0.036, 0.0018, 0.0049, mat("core", (0.05, 0.05, 0.055), 0.6))
    cylinder("spindle", 0.0036, 0.022, 0.012, chrome, verts=48)
    # Gira el disco a 33 1/3 rpm (para el film).
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    for o in (lac, bpy.data.objects["core"], bpy.data.objects["platter"]):
        o.rotation_euler = (0, 0, 0)
        o.keyframe_insert("rotation_euler", frame=1)
        o.rotation_euler = (0, 0, -2 * math.pi * RPM / 60.0 * (1000 / FPS))
        o.keyframe_insert("rotation_euler", frame=1001)
    # Carro y cabezal de corte que entran desde la derecha.
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.30, 0.0, 0.035)); carr = bpy.context.active_object
    carr.scale = (0.24, 0.07, 0.03); carr.data.materials.append(dark); bpy.ops.object.transform_apply(scale=True)
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=0.012, depth=0.30, location=(0.30, -0.06, 0.04), rotation=(0, math.pi / 2, 0))
    rail = bpy.context.active_object; rail.data.materials.append(chrome)
    hx = R_CUT
    bpy.ops.mesh.primitive_cube_add(size=1, location=(hx + 0.035, 0.0, 0.038)); head = bpy.context.active_object
    head.scale = (0.07, 0.045, 0.05); head.data.materials.append(mat("head", (0.12, 0.12, 0.13), 0.25, metal=0.9)); bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type="BEVEL"); head.modifiers["Bevel"].width = 0.004; head.modifiers["Bevel"].segments = 3
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.006, depth=0.03, location=(hx + 0.004, 0.0, 0.018)); arm = bpy.context.active_object; arm.data.materials.append(chrome)
    bpy.ops.mesh.primitive_cone_add(vertices=24, radius1=0.0016, radius2=0.0, depth=0.008, location=(hx, 0.0, 0.0095), rotation=(math.pi, 0, 0))
    stylus = bpy.context.active_object; stylus.data.materials.append(mat("sapphire", (0.9, 0.85, 0.8), 0.02, metal=0.0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.0045, location=(hx + 0.06, -0.026, 0.065)); lamp = bpy.context.active_object; lamp.data.materials.append(red)
    pl = bpy.data.lights.new("lampglow", "POINT"); pl.energy = 0.6; pl.color = (1.0, 0.15, 0.06); pl.shadow_soft_size = 0.004
    o = bpy.data.objects.new("lampglow", pl); o.location = (hx + 0.06, -0.034, 0.065); sc.collection.objects.link(o)
    # Viruta: el hilo de laca que levanta el estilete y se lleva la aspiradora.
    cu = bpy.data.curves.new("swarf", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = 0.00045; cu.bevel_resolution = 3
    sp = cu.splines.new("POLY"); pts = []
    for i in range(160):
        t = i / 159.0
        a = t * 9.0 * math.pi
        pts.append((hx - 0.002 + 0.008 * t + 0.004 * math.sin(a) * t, -0.002 - 0.018 * t + 0.004 * math.cos(a) * t, 0.006 + 0.05 * t * t))
    sp.points.add(len(pts) - 1)
    for p, c in zip(sp.points, pts): p.co = (*c, 1)
    sw = bpy.data.objects.new("swarf", cu); cu.materials.append(mat("swarfm", (0.02, 0.02, 0.025), 0.15, coat=1.0)); sc.collection.objects.link(sw)
    # Luces: caja suave arriba a la izquierda (el reflejo largo sobre la laca), contraluz ámbar y relleno frío.
    def area(name, loc, rot, size, energy, color):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = energy; l.color = color
        o = bpy.data.objects.new(name, l); o.location = loc; o.rotation_euler = rot; sc.collection.objects.link(o); return o
    area("key", (-0.35, -0.25, 0.55), (math.radians(35), math.radians(-30), 0), 0.45, 9.0, (1.0, 0.97, 0.92))
    # Tira de luz detrás del disco: su reflejo largo cruza la zona cortada y hace ver el surco desde la cámara principal.
    strip = area("strip", (0.28, 0.60, 0.20), (0, 0, 0), 0.6, 6.0, (1.0, 0.86, 0.66))
    strip.data.shape = "RECTANGLE"; strip.data.size = 0.7; strip.data.size_y = 0.07
    d = Vector((0.08, 0.02, 0.0)) - strip.location; strip.rotation_euler = d.to_track_quat("-Z", "X").to_euler()
    area("fill", (0.1, -0.6, 0.25), (math.radians(68), 0, 0), 0.6, 1.5, (0.75, 0.85, 1.0))
    sc.render.film_transparent = False
    return sc

SHOTS = {
    # nombre: (posición, punto mirado, lente mm, resolución)
    "hero":  ((-0.12, -0.52, 0.17), (0.05, 0.03, 0.0), 50, (2400, 1100)),
    "close": ((0.0, -0.20, 0.10), (0.105, 0.0, 0.004), 70, (1600, 1100)),
    "top":   ((0.0, -0.02, 0.55), (0.0, 0.0, 0.0), 50, (1400, 1400)),
}

def camera(sc, loc, look, lens, res):
    cd = bpy.data.cameras.new("cam"); cd.lens = lens; cd.dof.use_dof = True
    cd.dof.focus_distance = (Vector(look) - Vector(loc)).length; cd.dof.aperture_fstop = 7.0
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); sc.camera = cam
    cam.location = loc
    d = Vector(look) - Vector(loc); cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    sc.render.resolution_x, sc.render.resolution_y = res
    return cam

def stills():
    for name in (arg("--shots", "hero,close,top")).split(","):
        sc = build()
        loc, look, lens, res = SHOTS[name]
        camera(sc, loc, look, lens, res)
        sc.frame_set(1)
        sc.render.filepath = os.path.join(OUT, name + ".jpg")
        bpy.ops.render.render(write_still=True)

def film():
    shot = arg("--shot", "orbit"); frames = int(arg("--frames", "216"))
    first = int(arg("--first", "1")); step = int(arg("--step", "1"))
    sc = build()
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    sc.render.use_motion_blur = True; sc.render.motion_blur_shutter = 0.5
    cd = bpy.data.cameras.new("cam"); cd.lens = 50; cd.dof.use_dof = True; cd.dof.aperture_fstop = 7.0
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); sc.camera = cam
    for f in range(first, frames + 1, step):
        t = (f - 1) / (frames - 1)
        if shot == "orbit":      # vuelta lenta y baja alrededor del disco
            a = math.radians(-115 + 40 * t)
            loc = Vector((0.45 * math.cos(a), 0.45 * math.sin(a), 0.13 - 0.03 * t)); look = Vector((0.03, 0.0, 0.0)); cd.lens = 45
        elif shot == "macro":    # cerca del estilete, deslizándose sobre el surco recién cortado
            loc = Vector((0.02 - 0.03 * t, -0.21 + 0.02 * t, 0.10 - 0.02 * t)); look = Vector((R_CUT + 0.006, 0.0, 0.004)); cd.lens = 70
        else:                    # desde arriba, bajando
            loc = Vector((0.0, -0.03, 0.62 - 0.18 * t)); look = Vector((0.0, 0.0, 0.0)); cd.lens = 50
        cam.location = loc
        cam.rotation_euler = (look - loc).to_track_quat("-Z", "Y").to_euler()
        cd.dof.focus_distance = (look - loc).length
        sc.frame_set(f)
        sc.render.filepath = os.path.join(OUT, f"{shot}_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)



# ══ Film de la web (filmkit, --mode film2) ══
# Un torno más detallado que el de las fotos (las fotos siguen con build()): cubierta de esmalte martillado, viga con
# husillo y carro, cabezal con aletas, lámpara roja al frente, estilete con hilos calefactores, boquilla de succión
# con la viruta, microscopio en el carro y un puente de dos vúmetros. El surco se calcula de verdad: una espiral de
# paso variable (con "look-ahead": el paso de cada vuelta se abre antes de un golpe fuerte, como en un torno real)
# cuyo ancho y profundidad siguen el envolvente del master, y que se corta mientras el disco gira: el borde del
# surco nuevo está siempre debajo del estilete.
BPM = 104; BEAT = 60.0 / BPM
A0 = 8 * BEAT                          # el film arranca en el compás 3 (entra el piano); la batería entra 8 tiempos después
FILM_SHOTS = [("open", 8), ("stylus", 6), ("scope", 6), ("hero", 8), ("carriage", 6), ("end", 8)]
Z_TOP = 0.0056                         # cara de la laca
REV = RPM / 60.0                       # vueltas por segundo
P0 = 0.0009                            # paso medio del surco (exagerado ~10 veces para que se vea)
LAMP = 3.5                             # emisión de la lámpara de corte (más, y AgX la vuelve blanca)
SCOPE_SLOW = 6.0                       # cámara lenta de la toma del microscopio
LEAD = 0.0005                          # avance del husillo por vuelta: gira a ojos vista mientras el carro entra


def groove_plan(env, fps=24):
    """Surco por cuadro de tiempo de corte τ: radio, paso, ancho y profundidad. Tramos: entrada (paso abierto),
    tema 1 entero (ya cortado cuando empieza el film), banda entre temas, tema 2 (el que suena) y una cola."""
    import numpy as np
    e = np.maximum(np.array(env, dtype=float), 1e-4)
    lv = np.clip((20 * np.log10(e) + 20.0) / 20.0, 0.0, 1.0)
    lv = np.convolve(lv, np.ones(3) / 3, mode="same")
    nrev = int(round(fps / REV))                                    # cuadros por vuelta
    segs = [(np.zeros(int(1.5 * fps)), 2.6 * P0), (lv, None), (np.zeros(int(3.5 * fps)), 3.2 * P0), (lv, None),
            (np.zeros(int(14 * fps)), 0.6 * P0)]
    L, P = [], []
    for lvl, pf in segs:
        if pf is None:                                             # look-ahead de una vuelta
            la = np.array([lvl[i:i + nrev].max() for i in range(len(lvl))])
            P.append(P0 * (0.55 + 0.9 * la))
        else: P.append(np.full(len(lvl), pf))
        L.append(lvl)
    lvl, p = np.concatenate(L), np.concatenate(P)
    n = len(lvl); tau = np.arange(n) / fps
    r = R_OUT - np.concatenate([[0.0], np.cumsum(p * REV / fps)[:-1]])
    w = P0 * (0.30 + 0.62 * lvl)                                    # ancho: más fuerte, más ancho
    i = np.arange(n)
    gin = r - r[np.minimum(i + nrev, n - 1)]
    gout = np.where(i >= nrev, r[np.maximum(i - nrev, 0)] - r, 1.0)
    w = np.minimum(w, 0.8 * np.minimum(gin, gout))                  # sin pisar la vuelta vecina
    tau2 = (len(segs[0][0]) + len(segs[1][0]) + len(segs[2][0])) / fps
    return dict(tau=tau, r=r, p=p, w=w, d=0.42 * w, lv=lvl, tau2=tau2, fps=fps, plead=segs[0][1])


def plan_images(G):
    """Dos texturas de una fila: A = vueltas cortadas en función del radio; B = paso, ancho y profundidad en función
    del tiempo de corte."""
    import numpy as np
    tau, r = G["tau"], G["r"]
    ra_hi, ra_lo = R_OUT + 0.003, r[-1]
    WA = 4096
    ri = ra_hi - (np.arange(WA) + 0.5) / WA * (ra_hi - ra_lo)
    T = np.interp(ri, r[::-1], (tau * REV)[::-1])
    out = ri > R_OUT; T[out] = -(ri[out] - R_OUT) / G["plead"]    # afuera del borde: vueltas negativas (sin surco)
    A = bpy.data.images.new("turns", WA, 1, float_buffer=True); A.colorspace_settings.name = "Non-Color"
    pa = np.zeros((WA, 4), np.float32); pa[:, 0] = T; pa[:, 3] = 1; A.pixels.foreach_set(pa.ravel())
    WB = len(tau)
    B = bpy.data.images.new("groove", WB, 1, float_buffer=True); B.colorspace_settings.name = "Non-Color"
    pb = np.zeros((WB, 4), np.float32); pb[:, 0] = G["p"]; pb[:, 1] = G["w"]; pb[:, 2] = G["d"]; pb[:, 3] = 1
    B.pixels.foreach_set(pb.ravel())
    for im in (A, B): im.pack()
    return A, B, ra_hi, ra_lo, WB


def lacquer_film_material(G):
    """Laca con el surco calculado: para cada punto (r, θ) del disco busca la vuelta de espiral más cercana, lee su
    paso/ancho/profundidad en el tiempo en que se cortó y arma un perfil en V (relieve real en metros). Sólo existe
    lo ya cortado: vueltas entre 0 y "now" (las vueltas que dio el disco hasta este cuadro, animado)."""
    A, B, ra_hi, ra_lo, WB = plan_images(G)
    m = bpy.data.materials.new("lacquer_film"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; Lk = nt.links
    b = N["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.006, 0.006, 0.008, 1); b.inputs["Roughness"].default_value = 0.10
    b.inputs["Coat Weight"].default_value = 1.0; b.inputs["Coat Roughness"].default_value = 0.012
    def op(kind, a, bb=None):
        n = N.new("ShaderNodeMath"); n.operation = kind
        for k, v in enumerate((a, bb)):
            if v is None: continue
            if isinstance(v, (int, float)): n.inputs[k].default_value = float(v)
            else: Lk.new(v, n.inputs[k])
        return n.outputs[0]
    def tex(img, x):
        c = N.new("ShaderNodeCombineXYZ"); Lk.new(x, c.inputs[0]); c.inputs[1].default_value = 0.5
        t = N.new("ShaderNodeTexImage"); t.image = img; t.extension = "EXTEND"; t.interpolation = "Linear"
        Lk.new(c.outputs[0], t.inputs[0])
        s = N.new("ShaderNodeSeparateColor"); Lk.new(t.outputs["Color"], s.inputs[0]); return s.outputs
    tc = N.new("ShaderNodeTexCoord"); sep = N.new("ShaderNodeSeparateXYZ"); Lk.new(tc.outputs["Object"], sep.inputs[0])
    x, y = sep.outputs[0], sep.outputs[1]
    r = op("SQRT", op("ADD", op("MULTIPLY", x, x), op("MULTIPLY", y, y)))
    a = op("FRACT", op("DIVIDE", op("ARCTAN2", y, x), 2 * math.pi))
    T = tex(A, op("DIVIDE", op("SUBTRACT", ra_hi, r), ra_hi - ra_lo))[0]
    s = op("SUBTRACT", T, a); k = op("ROUND", s); nc = op("ADD", k, a)
    dt = op("ABSOLUTE", op("SUBTRACT", s, k))
    gb = tex(B, op("ADD", op("MULTIPLY", nc, G["fps"] / REV / WB), 0.5 / WB))
    dist = op("MULTIPLY", dt, gb[0])
    prof = op("MAXIMUM", op("SUBTRACT", 1.0, op("DIVIDE", dist, op("MULTIPLY", gb[1], 0.5))), 0.0)
    prof = op("POWER", prof, 0.85)                                   # paredes apenas cóncavas, fondo en punta
    now = N.new("ShaderNodeValue"); now.name = "now"; now.outputs[0].default_value = 0.0
    mask = op("MULTIPLY", op("GREATER_THAN", nc, 0.0), op("LESS_THAN", nc, now.outputs[0]))
    h = op("MULTIPLY", op("MULTIPLY", gb[2], prof), op("MULTIPLY", mask, -1.0))
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 1.0; bump.inputs["Distance"].default_value = 1.0
    Lk.new(h, bump.inputs["Height"])
    Lk.new(bump.outputs["Normal"], b.inputs["Normal"]); Lk.new(bump.outputs["Normal"], b.inputs["Coat Normal"])
    # Zona cortada: brillo anisotrópico circular (la "estrella" de luz de un disco) y un poco menos de espejo.
    zone = op("MULTIPLY", op("GREATER_THAN", T, 0.0), op("LESS_THAN", T, now.outputs[0]))
    t = N.new("ShaderNodeTangent"); t.direction_type = "RADIAL"; t.axis = "Z"; Lk.new(t.outputs["Tangent"], b.inputs["Tangent"])
    Lk.new(op("MULTIPLY", zone, 0.75), b.inputs["Anisotropic"])
    Lk.new(op("ADD", 0.10, op("MULTIPLY", zone, 0.12)), b.inputs["Roughness"])
    return m, now


def pmat(name, base, rough, metal=0.0, coat=0.0, aniso=0.0, emit=None, strength=0.0, noise=0.0, bumpn=0.0, nscale=400.0):
    """Material con variación fina opcional (rugosidad con ruido, golpe de martillo): que el metal no se vea de plástico."""
    m = mat(name, base, rough, metal, coat, aniso, emit, strength)
    if noise or bumpn:
        nt = m.node_tree; b = nt.nodes["Principled BSDF"]
        nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = nscale; nz.inputs["Detail"].default_value = 6.0
        if noise:
            mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs["To Min"].default_value = max(0.0, rough - noise); mr.inputs["To Max"].default_value = rough + noise
            nt.links.new(nz.outputs["Fac"], mr.inputs["Value"]); nt.links.new(mr.outputs["Result"], b.inputs["Roughness"])
        if bumpn:
            bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = bumpn
            nt.links.new(nz.outputs["Fac"], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m


def adopt(o, parent, keep):
    """Emparenta; keep=True conserva la posición de mundo (para padres que no están en el origen)."""
    o.parent = parent
    if keep: o.matrix_parent_inverse = parent.matrix_world.inverted()


def box(name, c, size, m, bev=0.0012, seg=3, parent=None, keep=False):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c)
    o = bpy.context.active_object; o.name = name; o.scale = size; bpy.ops.object.transform_apply(scale=True)
    if bev:
        md = o.modifiers.new("bev", "BEVEL"); md.width = bev; md.segments = seg; md.harden_normals = True
        bpy.ops.object.shade_smooth()
    o.data.materials.append(m)
    if parent: adopt(o, parent, keep)
    return o


def cyl(name, c, r, depth, m, rot=(0, 0, 0), verts=64, bev=0.0, parent=None, keep=False):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=c, rotation=rot)
    o = bpy.context.active_object; o.name = name; o.data.materials.append(m)
    if bev:
        md = o.modifiers.new("bev", "BEVEL"); md.width = bev; md.segments = 3; md.harden_normals = True; md.limit_method = "ANGLE"
    bpy.ops.object.shade_smooth()
    if parent: adopt(o, parent, keep)
    return o


def tube(name, pts, radius, m, parent=None, res=4):
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = radius; cu.bevel_resolution = res
    sp = cu.splines.new("NURBS"); sp.points.add(len(pts) - 1); sp.order_u = min(4, len(pts)); sp.use_endpoint_u = True
    for q, c in zip(sp.points, pts): q.co = (*c, 1)
    o = bpy.data.objects.new(name, cu); bpy.context.scene.collection.objects.link(o); cu.materials.append(m)
    if parent: o.parent = parent
    return o


def along(name, p0, p1, r, m, verts=48, parent=None, bev=0.0):
    """Cilindro entre dos puntos."""
    p0, p1 = Vector(p0), Vector(p1); d = p1 - p0
    o = cyl(name, (p0 + p1) / 2, r, d.length, m, verts=verts, bev=bev, parent=parent)
    o.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    return o


def build_film(G):
    """El torno del film. Devuelve un dict con lo que se anima."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    world = bpy.data.worlds.new("w"); sc.world = world; world.use_nodes = True
    bg = world.node_tree.nodes["Background"]; bg.inputs[0].default_value = (0.006, 0.0065, 0.007, 1); bg.inputs[1].default_value = 1.0
    hammer = pmat("hammertone", (0.016, 0.018, 0.016), 0.72, noise=0.10, bumpn=0.06, nscale=180.0)
    hammer.node_tree.nodes["Principled BSDF"].inputs["IOR"].default_value = 1.08     # mate: sin el reflejo rasante de Fresnel
    hammer.node_tree.nodes["Principled BSDF"].inputs["Specular IOR Level"].default_value = 0.3
    alu = mat("alu", (0.80, 0.80, 0.78), 0.20, metal=1.0, aniso=0.65)
    brushed = pmat("brushed", (0.72, 0.72, 0.70), 0.26, metal=1.0, aniso=0.5, noise=0.05)
    gun = pmat("gunmetal", (0.045, 0.046, 0.05), 0.34, metal=0.85, noise=0.08, nscale=900.0)
    blackan = pmat("anodized", (0.018, 0.018, 0.02), 0.38, metal=0.7, noise=0.06, nscale=900.0)
    chrome = mat("chrome", (0.92, 0.92, 0.92), 0.05, metal=1.0)
    copper = mat("copper", (0.85, 0.42, 0.22), 0.22, metal=1.0)
    rubber = mat("rubber", (0.015, 0.015, 0.016), 0.65)
    well = mat("well", (0.012, 0.012, 0.013), 0.5, metal=0.3)
    sapph = mat("sapphire", (0.95, 0.9, 0.85), 0.01)
    sapph.node_tree.nodes["Principled BSDF"].inputs["Transmission Weight"].default_value = 0.85
    enamel = pmat("enamel", (0.022, 0.023, 0.025), 0.34, coat=0.35, noise=0.05)
    satin = pmat("satin", (0.30, 0.30, 0.30), 0.30, metal=1.0, aniso=0.4, noise=0.06)
    O = {}
    # Cubierta y pozo del plato.
    box("deck", (0.16, 0.02, -0.046), (0.92, 0.64, 0.068), hammer, bev=0.008, seg=4)
    cyl("well", (0, 0, -0.0115), 0.172, 0.0012, well, verts=256)
    box("floor", (0.1, 0.0, -0.5), (6, 6, 0.01), mat("floor", (0.01, 0.01, 0.011), 0.6), bev=0)
    # Plato, laca, centro.
    pl = cyl("platter", (0, 0, -0.004), 0.160, 0.016, alu, verts=256, bev=0.0012)
    lm, now = lacquer_film_material(G)
    lac = cyl("lacquer", (0, 0, 0.0048), 0.1524, 0.0016, lm, verts=512)
    puck = cyl("puck", (0, 0, Z_TOP + 0.004), 0.020, 0.008, brushed, verts=128, bev=0.0012)
    for k in range(60):                                              # moleteado del pisador (gira con el disco)
        a = 2 * math.pi * k / 60
        g = box(f"knurl{k}", (math.cos(a) * 0.0201, math.sin(a) * 0.0201, Z_TOP + 0.004), (0.0012, 0.0014, 0.006), brushed, bev=0, parent=puck, keep=True)
        g.rotation_euler.z = a
    cyl("spindle", (0, 0, Z_TOP + 0.0095), 0.0030, 0.004, chrome, verts=48, bev=0.0008, parent=puck, keep=True)
    O.update(lacquer=lac, platter=pl, puck=puck, now=now)
    # Viga, columna, husillo (fijos).
    box("column", (0.51, 0.04, 0.049), (0.08, 0.10, 0.122), hammer, bev=0.004)
    box("beam", (0.285, 0.040, 0.102), (0.49, 0.056, 0.016), blackan, bev=0.0015)
    box("motor", (0.505, 0.0, 0.085), (0.05, 0.03, 0.03), gun, bev=0.003)
    screw_m = pmat("screw", (0.85, 0.85, 0.85), 0.12, metal=1.0)
    nt = screw_m.node_tree; bsdf = nt.nodes["Principled BSDF"]          # rosca helicoidal: se ve girar
    tc = nt.nodes.new("ShaderNodeTexCoord"); sp = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(tc.outputs["Object"], sp.inputs[0])
    def nop(kind, a, b=None):
        n = nt.nodes.new("ShaderNodeMath"); n.operation = kind
        for k, v in enumerate((a, b)):
            if v is None: continue
            if isinstance(v, float): n.inputs[k].default_value = v
            else: nt.links.new(v, n.inputs[k])
        return n.outputs[0]
    ph = nop("ADD", nop("DIVIDE", sp.outputs[0], 0.0012), nop("DIVIDE", nop("ARCTAN2", sp.outputs[2], sp.outputs[1]), 2 * math.pi))
    th = nop("ABSOLUTE", nop("SUBTRACT", nop("FRACT", ph), 0.5))
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Distance"].default_value = 0.0004; nt.links.new(th, bp.inputs["Height"])
    nt.links.new(bp.outputs["Normal"], bsdf.inputs["Normal"])
    screw = cyl("screw", (0.265, 0.0, 0.085), 0.004, 0.42, screw_m, rot=(0, math.pi / 2, 0), verts=48)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)     # eje en X: gira con rotation.x
    cpl = cyl("coupling", (0.474, 0.0, 0.085), 0.0075, 0.012, brushed, rot=(0, math.pi / 2, 0), verts=48, bev=0.0008, parent=screw, keep=True)
    box("slot", (0.474, 0.0, 0.0926), (0.0124, 0.0016, 0.0012), blackan, bev=0, parent=screw, keep=True)   # ranura: muestra el giro
    cyl("bearing", (0.052, 0.0, 0.085), 0.008, 0.012, gun, rot=(0, math.pi / 2, 0), verts=48, bev=0.001)
    box("bearingblock", (0.052, 0.020, 0.088), (0.014, 0.034, 0.022), blackan, bev=0.001)
    O["screw"] = screw
    # Carro y cabezal (origen en el eje del estilete, sobre el radio +x).
    carr = bpy.data.objects.new("carriage", None); sc.collection.objects.link(carr)
    box("carblock", (0.007, 0.040, 0.082), (0.058, 0.066, 0.026), blackan, bev=0.002, parent=carr)
    box("nut", (0.007, 0.0, 0.085), (0.022, 0.016, 0.018), gun, bev=0.0015, parent=carr)
    box("neck", (0.006, 0.026, 0.0645), (0.026, 0.024, 0.009), gun, bev=0.001, parent=carr)
    box("head", (0.006, 0.025, 0.038), (0.042, 0.046, 0.044), gun, bev=0.003, seg=4, parent=carr)
    box("faceplate", (0.006, 0.0016, 0.040), (0.038, 0.0012, 0.036), satin, bev=0.0004, parent=carr)
    for (sx, sz) in ((-0.010, 0.054), (0.022, 0.054), (-0.010, 0.026), (0.022, 0.026)):
        cyl("bolt", (sx, 0.0008, sz), 0.0012, 0.0012, chrome, rot=(math.pi / 2, 0, 0), verts=24, parent=carr)
    for i in range(8):                                               # aletas de disipación al costado
        box(f"fin{i}", (0.0295, 0.007 + i * 0.0052, 0.038), (0.005, 0.0016, 0.040), blackan, bev=0.0003, parent=carr)
    # Lámpara roja de corte al frente, con bisel cromado.
    cyl("bezel", (0.020, 0.0004, 0.051), 0.0042, 0.0024, chrome, rot=(math.pi / 2, 0, 0), verts=48, bev=0.0005, parent=carr)
    lampm = mat("lamp", (0.7, 0.05, 0.03), 0.15, emit=(1.0, 0.04, 0.012), strength=LAMP)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.0033, location=(0.020, -0.0010, 0.051), segments=32, ring_count=16)
    lamp = bpy.context.active_object; lamp.scale = (1, 0.55, 1); lamp.data.materials.append(lampm); bpy.ops.object.shade_smooth(); lamp.parent = carr
    pl_ = bpy.data.lights.new("lampglow", "POINT"); pl_.energy = 0.25; pl_.color = (1.0, 0.12, 0.05); pl_.shadow_soft_size = 0.003
    lg = bpy.data.objects.new("lampglow", pl_); lg.location = (0.020, -0.007, 0.051); sc.collection.objects.link(lg); lg.parent = carr
    O.update(lampm=lampm, lamplight=pl_)
    # Estilete: porta, vástago, punta de zafiro, hilos calefactores.
    box("holder", (0.0, 0.0024, 0.0135), (0.0045, 0.0048, 0.005), chrome, bev=0.0004, parent=carr)
    cyl("shank", (0.0, 0.0, 0.0094), 0.0007, 0.0052, chrome, verts=24, parent=carr)
    bpy.ops.mesh.primitive_cone_add(vertices=24, radius1=0.0007, radius2=0.0, depth=0.0017, location=(0.0, 0.0, Z_TOP + 0.00082), rotation=(math.pi, 0, 0))
    tip = bpy.context.active_object; tip.data.materials.append(sapph); tip.parent = carr
    for sxx in (-1, 1):
        tube("heater", [(sxx * 0.0016, 0.0012, 0.016), (sxx * 0.0032, -0.0008, 0.012), (sxx * 0.0016, -0.0006, 0.0085), (sxx * 0.0006, -0.0002, 0.0080)],
             0.00012, copper, parent=carr)
    # Boquilla de succión al costado interior y su manguera.
    mouth = Vector((-0.0058, -0.0006, Z_TOP + 0.0032)); nend = Vector((-0.0125, 0.004, 0.0185))
    along("nozzle", mouth, nend, 0.0013, chrome, verts=32, parent=carr)
    cyl("nozzlein", mouth + (mouth - nend).normalized() * -0.00002, 0.00095, 0.0002, rubber, verts=24, parent=carr).rotation_euler = (nend - mouth).to_track_quat("Z", "Y").to_euler()
    tube("hose", [tuple(nend), (-0.016, 0.006, 0.030), (-0.020, 0.012, 0.050), (-0.016, 0.030, 0.064), (-0.010, 0.040, 0.069)], 0.0021, rubber, parent=carr)
    # Viruta: hélice que nace en la punta y entra en la boquilla; al girar sobre su eje parece correr hacia adentro.
    ax = bpy.data.objects.new("swarfaxis", None); sc.collection.objects.link(ax); ax.parent = carr
    p0 = Vector((-0.0004, -0.0002, Z_TOP + 0.0002)); ax.location = p0
    dvec = mouth + (nend - mouth).normalized() * 0.0012 - p0; ax.rotation_euler = dvec.to_track_quat("Z", "Y").to_euler()
    Ls = dvec.length
    cu = bpy.data.curves.new("swarf", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = 0.00008; cu.bevel_resolution = 2
    spl = cu.splines.new("POLY"); pts = []
    for i in range(400):                                             # rizo irregular, no un resorte
        t = i / 399.0; a = 2 * math.pi * (7.0 * t + 0.9 * math.sin(2.3 * math.pi * t))
        rad = 0.0013 * math.sin(math.pi * t) ** 0.6 * (1.0 + 0.35 * math.sin(11.0 * t + 1.0)) * (1.0 - 0.3 * t)
        pts.append((rad * math.cos(a) + 0.0007 * math.sin(math.pi * t) ** 2, rad * math.sin(a) + 0.0010 * math.sin(math.pi * t),
                    Ls * (t + 0.05 * math.sin(2 * math.pi * 3 * t) * math.sin(math.pi * t))))
    spl.points.add(len(pts) - 1)
    for q, c in zip(spl.points, pts): q.co = (*c, 1)
    sw = bpy.data.objects.new("swarf", cu); sc.collection.objects.link(sw); sw.parent = ax
    cu.materials.append(mat("swarfm", (0.05, 0.045, 0.045), 0.10, coat=1.0))
    O["swarf"] = sw
    # Microscopio en el carro: objetivo sobre el surco recién cortado, ocular hacia el operador.
    micro = []
    axis = Vector((0.22, -0.52, 0.82)).normalized(); ob = Vector((0.011, -0.021, 0.030))
    micro.append(along("objective", ob, ob + axis * 0.014, 0.0052, chrome, parent=carr, bev=0.0006))
    micro.append(along("mtube", ob + axis * 0.014, ob + axis * 0.090, 0.0095, enamel, parent=carr, bev=0.0012))
    micro.append(along("mring", ob + axis * 0.040, ob + axis * 0.046, 0.0105, blackan, parent=carr, bev=0.0005))
    top = ob + axis * 0.090
    e2 = Vector((0.08, -0.9, 0.42)).normalized()
    micro.append(along("prism", top - axis * 0.006, top + axis * 0.012, 0.0115, enamel, parent=carr, bev=0.0015))
    micro.append(along("eyetube", top + axis * 0.004, top + axis * 0.004 + e2 * 0.040, 0.0075, blackan, parent=carr, bev=0.0008))
    micro.append(along("eyecup", top + axis * 0.004 + e2 * 0.036, top + axis * 0.004 + e2 * 0.050, 0.0098, rubber, parent=carr, bev=0.0015))
    side = axis.cross(Vector((0, 0, 1))).normalized()
    mid = ob + axis * 0.060
    for sgn in (-1, 1):
        micro.append(along("focus", mid + side * sgn * 0.0095, mid + side * sgn * 0.0155, 0.0085, brushed, parent=carr, bev=0.0008))
    micro.append(along("marm", mid + side * -0.004 + Vector((0, 0.004, 0)), Vector((0.016, 0.012, 0.080)), 0.0038, blackan, parent=carr))
    O.update(carriage=carr, micro=micro)
    # Puente de vúmetros atrás a la derecha, inclinado hacia el operador.
    hous = bpy.data.objects.new("meters", None); sc.collection.objects.link(hous)
    hous.location = (0.33, 0.235, 0.028); hous.rotation_euler = (math.radians(-18), 0, math.radians(-8))
    box("mhouse", (0, 0, 0), (0.17, 0.070, 0.078), gun, bev=0.004, parent=hous)
    needles = []
    face = pmat("face", (0.80, 0.72, 0.55), 0.5, emit=(1.0, 0.78, 0.50), strength=0.7)
    tick = mat("tick", (0.03, 0.03, 0.03), 0.6)
    redz = mat("redzone", (0.6, 0.05, 0.03), 0.6)
    glass = mat("glass", (1, 1, 1), 0.02); glass.node_tree.nodes["Principled BSDF"].inputs["Transmission Weight"].default_value = 1.0
    for j, cx in enumerate((-0.040, 0.040)):
        box("mbezel", (cx, -0.0352, 0.004), (0.068, 0.002, 0.050), chrome, bev=0.0008, parent=hous)
        box("mface", (cx, -0.0364, 0.004), (0.062, 0.0006, 0.044), face, bev=0, parent=hous)
        pv = Vector((cx, -0.0372, -0.016))
        for k in range(11):                                         # escala en arco, la zona roja al final
            a = math.radians(-48 + 96 * k / 10)
            q = pv + Vector((math.sin(a) * 0.029, 0, math.cos(a) * 0.029))
            g = box("tick", tuple(q), (0.0007, 0.0004, 0.0055 if k % 5 == 0 else 0.0035), redz if k >= 8 else tick, bev=0, parent=hous)
            g.rotation_euler.y = a
        nd = bpy.data.objects.new(f"needle{j}", None); sc.collection.objects.link(nd); nd.parent = hous; nd.location = pv
        box("nbar", (0, -0.0003, 0.015), (0.0006, 0.0004, 0.030), tick, bev=0, parent=nd)
        cyl("nhub", (0, -0.0004, 0), 0.0022, 0.001, blackan, rot=(math.pi / 2, 0, 0), verts=24, parent=nd)
        box("mglass", (cx, -0.0375 - 0.0012, 0.004), (0.064, 0.0008, 0.046), glass, bev=0, parent=hous)
        needles.append(nd)
    O["needles"] = needles
    # Luces: caja grande arriba a la izquierda, contraluz cálido en tira, relleno frío, barrido y aro del microscopio.
    def area(name, loc, look, size, energy, color, sy=None, spread=None):
        l = bpy.data.lights.new(name, "AREA"); l.energy = energy; l.color = color
        if spread: l.spread = math.radians(spread)
        if sy is None and name == "key": l.shape = "DISK"                  # reflejo redondo de caja de luz
        if sy: l.shape = "RECTANGLE"; l.size = size; l.size_y = sy
        else: l.size = size
        o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat("-Z", "Y").to_euler(); return o
    O["key"] = area("key", (-0.18, -0.24, 0.55), (0.04, 0.0, 0.0), 0.85, 6.5, (1.0, 0.96, 0.90), spread=55)
    O["rim"] = area("rim", (0.25, 0.55, 0.30), (0.18, 0.02, 0.06), 0.7, 7.0, (1.0, 0.86, 0.66), sy=0.07, spread=40)
    O["fill"] = area("fill", (0.35, -0.65, 0.22), (0.1, 0.0, 0.03), 0.6, 0.8, (0.75, 0.85, 1.0), spread=70)
    O["sweep"] = area("sweep", (0.0, -0.5, 0.45), (0.0, 0.0, 0.0), 0.9, 0.0, (1.0, 0.95, 0.88), sy=0.045)
    O["ring"] = area("ring", (0, 0, 0.05), (0, 0, 0), 0.012, 0.0, (1.0, 0.97, 0.93))
    O["sc"] = sc
    return O


def film2():
    """Film de la web de Lacquer (24,2 s, 104 bpm, 42 tiempos). Audio: el master demo "Balanced"
    (web/assets/samples/lacquer/balanced.m4a) desde el compás 3 (A0 = 4,62 s): intro de cuerdas y piano y la
    batería entra justo en el primer corte. El envolvente del mismo audio mueve todo y además ES el surco: el disco
    trae cortado el tema 1, una banda, y el tema 2 se corta durante el film (paso y ancho siguen al envolvente).

      open      8 t  4,6 s  Toma baja rasante sobre la laca, de frente-izquierda: el plato gira a 33 1/3, el
                            cabezal con su lámpara roja a la derecha, la luz principal sube de 25 a 100 %. Empuje
                            lento con subida; foco en el estilete. El tercio inferior izquierdo es laca negra quieta
                            (el título).
      stylus    6 t  3,5 s  Entra la batería. Macro de 100 mm del estilete cortando: la viruta en hélice sube a la
                            boquilla, el surco nuevo sale de la punta; foco que pasa del surco recién cortado a la
                            punta. Cámara en mano muy sutil.
      scope     6 t  3,5 s  Por el microscopio: campo circular con retícula; el surco nace bajo el estilete arriba y
                            baja hacia el ojo, las vueltas más anchas donde la música golpea; el operador termina de
                            enfocar al principio.
      hero      8 t  4,6 s  General baja en arco por el frente: todo el torno, el plato girando, la banda cortada
                            contra la laca virgen, los vúmetros bailando, y una tira de luz que barre la laca.
      carriage  6 t  3,5 s  Travelling bajo la viga, a lo largo del husillo, desde la columna hacia el cabezal:
                            arranca desenfocado (bokeh de la lámpara y el husillo) y el foco aterriza en el
                            cabezal, sus aletas y la lámpara roja.
      end       8 t  4,6 s  Fundido; vista alta de tres cuartos que se aleja despacio (film.py oscurece y pone el
                            cierre).
    """
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    import numpy as np
    only = arg("--shot"); quality = arg("--quality", "final"); probe = int(arg("--probe", "0"))
    env = fk.envelope(arg("--audio"))
    G = groove_plan(env)
    hits = fk.onsets(env, thresh=0.22, gap=5)
    flash = np.zeros(len(env) + 400)
    for h in hits:
        for i in range(24): flash[h + i] = max(flash[h + i], math.exp(-i / 4.0))
    vu = np.zeros(len(env) + 400); v = 0.0                            # balística de vúmetro (≈300 ms)
    for i in range(len(vu)):
        x = env[i] if i < len(env) else 0.0
        v += (x - v) * (0.28 if x > v else 0.12); vu[i] = v
    t0 = 0.0
    for (name, beats) in FILM_SHOTS:
        frames = int(round(beats * BEAT * FPS)); f0 = int(round(t0 * FPS)); t0 += frames / FPS
        if only and name not in only.split(","): continue
        O = build_film(G); sc = O["sc"]
        fk.setup(sc, quality)
        cam = fk.camera(sc)
        slow = SCOPE_SLOW if name == "scope" else 1.0                # el microscopio en cámara lenta (como con estroboscopio)
        tau_at = lambda f, f0=f0, slow=slow: G["tau2"] + A0 + (f0 + (f - 1) / slow) / FPS
        r_at = lambda tau: float(np.interp(tau, G["tau"], G["r"]))
        xs0 = r_at(tau_at(1))
        STY = Vector((xs0, 0.0, Z_TOP))
        P = lambda *v: STY + Vector(v)
        paths = {
            "open":     fk.Path([(0, (-0.13, -0.47, 0.046), (0.050, 0.030, 0.040), 55, 3.2),
                                 (1, (-0.07, -0.40, 0.056), (0.065, 0.025, 0.040), 60, 3.2)]),
            "stylus":   fk.Path([(0, P(0.040, -0.050, 0.028), P(-0.001, -0.002, 0.003), 100, 8.0),
                                 (1, P(0.030, -0.050, 0.022), P(-0.001, -0.002, 0.003), 100, 8.0)]),
            "scope":    fk.Path([(0, P(0.0016, -0.0090, 0.0270), P(0.0009, -0.0018, 0.0), 58, 32.0),
                                 (1, P(0.0014, -0.0086, 0.0262), P(0.0009, -0.0018, 0.0), 60, 32.0)], easing="linear"),
            "hero":     fk.Path([(0, (-0.36, -0.40, 0.09), (0.09, 0.03, 0.02), 35, 5.6),
                                 (0.5, (-0.22, -0.48, 0.11), (0.09, 0.03, 0.02), 35, 5.6),
                                 (1, (-0.06, -0.50, 0.13), (0.10, 0.03, 0.02), 35, 5.6)]),
            "carriage": fk.Path([(0, (0.50, -0.075, 0.080), (0.30, 0.0, 0.084), 50, 4.0),
                                 (1, (0.28, -0.090, 0.080), P(0.006, 0.0, 0.040), 50, 4.0)]),
            "end":      fk.Path([(0, (0.10, -0.40, 0.26), (0.10, 0.03, 0.0), 40, 8.0),
                                 (1, (0.10, -0.60, 0.40), (0.10, 0.03, 0.0), 40, 8.0)], easing="out"),
        }
        path = paths[name]
        if name == "scope":
            for o in O["micro"]: o.hide_render = True                 # la cámara está dentro del microscopio
        if name == "stylus":                                         # la laca refleja la caja: se apaga y entra una luz chica de macro
            sc.render.motion_blur_shutter = 0.35
            O["key"].data.energy = 1.6
            mk = bpy.data.lights.new("macro", "AREA"); mk.size = 0.03; mk.energy = 0.5; mk.color = (1.0, 0.95, 0.88)
            mo = bpy.data.objects.new("macro", mk); sc.collection.objects.link(mo); mo.location = STY + Vector((-0.03, 0.05, 0.06))
            mo.rotation_euler = (STY - mo.location).to_track_quat("-Z", "Y").to_euler()
        rot_objs = [O["lacquer"], O["platter"], O["puck"]]

        def pose(f, t, u, name=name, path=path, f0=f0, slow=slow):
            tau = tau_at(f); N = tau * REV
            for o in rot_objs: o.rotation_euler.z = -2 * math.pi * N
            xs = r_at(tau); O["carriage"].location.x = xs
            O["screw"].rotation_euler.x = 2 * math.pi * (R_OUT - xs) / LEAD
            O["swarf"].rotation_euler.z = -2 * math.pi * 2.2 * t / slow
            g = A0 * FPS + f0 + f - 1; gi = min(len(vu) - 1, int(round(g)))
            for j, nd in enumerate(O["needles"]):
                lvl = vu[max(0, gi - j)] * (1.0 if j == 0 else 0.93)
                nd.rotation_euler.y = math.radians(-48 + 96 * min(1.0, 0.15 + 0.85 * lvl))
            sty = Vector((xs, 0.0, Z_TOP))
            focus = None
            if name == "open": focus = sty + Vector((0, 0, 0.01))
            if name == "stylus":
                focus = (sty + Vector((-0.004, 0.0, 0.003))).lerp(sty + Vector((0, 0, 0.0015)), fk.rack(u, 0.25, 0.60, 0.0, 1.0))
            if name == "scope":
                cam_p, look, *_ = path.at(u)
                focus = look + (look - cam_p).normalized() * fk.rack(u, 0.0, 0.32, 0.006, 0.0)
            if name == "carriage":
                focus = Vector((0.44, 0.0, 0.085)).lerp(sty + Vector((0.020, -0.001, 0.051 - Z_TOP)), fk.rack(u, 0.20, 0.75, 0.0, 1.0))
            if name == "hero": focus = sty + Vector((-0.04, -0.02, 0.0))
            if name == "end": focus = Vector((0.08, 0.0, 0.01))
            # el recorrido de la cámara acompaña al carro en las tomas cercanas
            path.apply(cam, u, focus=focus, shake={"stylus": 0.0006, "scope": 0.00025, "open": 0.0004}.get(name, 0.0), t=t0 + t)
            if name in ("stylus", "scope"): cam.location.x += xs - xs0
            if name == "scope":
                ring = O["ring"]; ring.location = cam.location; ring.rotation_euler = cam.rotation_euler
            return rot_objs + [O["carriage"], O["screw"], O["swarf"], O["ring"]] + O["needles"]

        def keys(f, name=name, f0=f0, frames=frames):
            u = (f - 1) / max(1, frames - 1)
            tau = tau_at(f)
            O["now"].outputs[0].default_value = tau * REV; O["now"].outputs[0].keyframe_insert("default_value")
            gi = min(len(flash) - 1, int(round(A0 * FPS + f0 + f - 1)))
            lampk = 0.85 + 0.6 * flash[gi]
            O["lampm"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = LAMP * lampk
            O["lampm"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].keyframe_insert("default_value")
            O["lamplight"].energy = 0.25 * lampk; O["lamplight"].keyframe_insert("energy")
            if name == "open":
                O["key"].data.energy = 6.5 * fk.rack(u, 0.05, 0.6, 0.25, 1.0); O["key"].data.keyframe_insert("energy")
            if name == "hero":                                       # barrido: la tira cruza de adelante hacia atrás
                sw = O["sweep"]; k = fk.ease(u, "inout")
                sw.location = Vector((0.05, -0.42 + 0.80 * k, 0.40)); sw.data.energy = 26.0 * math.sin(math.pi * min(1, max(0, u * 1.15 - 0.05)))
                sw.rotation_euler = (Vector((0.05, 0.0 + 0.35 * (k - 0.5), 0.0)) - sw.location).to_track_quat("-Z", "Y").to_euler()
                sw.keyframe_insert("location"); sw.keyframe_insert("rotation_euler"); sw.data.keyframe_insert("energy")
            if name == "scope":
                O["ring"].data.energy = 0.035; O["ring"].data.keyframe_insert("energy")

        fk.render_shot(sc, name, frames, pose, OUT, step=probe or 1, extra_keys=keys)
        if name == "scope": scope_post(name, frames, probe or 1)


def scope_post(name, frames, step):
    """Lo que ve el ojo en el ocular: campo circular con borde suave, caída hacia el borde y una retícula fina con
    escala. Se aplica a los JPEG ya renderizados (filmkit no lo hace)."""
    import numpy as np
    for f in range(1, frames + 1, step):
        path = os.path.join(OUT, f"{name}_{f:04d}.jpg")
        if not os.path.exists(path): continue
        img = bpy.data.images.load(path); w, h = img.size
        px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px); px = px.reshape(h, w, 4)
        yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
        cx, cy, R = w / 2, h / 2, 0.47 * h
        d = np.hypot(xx - cx, yy - cy)
        edge = np.clip((R - d) / (0.035 * h), 0, 1); edge = edge * edge * (3 - 2 * edge)
        vig = 1.0 - 0.30 * (np.clip(d / R, 0, 1) ** 3)
        rgb = px[..., :3] * (edge * vig)[..., None]
        # Retícula: línea horizontal con marcas cada R/10 (largas cada 5) y una cruz vertical corta.
        lw = max(1.0, h / 900)
        ret = np.zeros((h, w), np.float32)
        ret[np.abs(yy - cy) < lw * 0.6] = 1
        for k in range(-9, 10):
            L = (0.06 if k % 5 == 0 else 0.03) * R
            ret[(np.abs(xx - (cx + k * R / 10)) < lw * 0.6) & (yy - cy > -L) & (yy - cy < L * 0.25)] = 1
        ret[(np.abs(xx - cx) < lw * 0.6) & (np.abs(yy - cy) < 0.10 * R)] = 1
        ret *= (d < R * 0.92)
        col = np.array([0.80, 0.82, 0.78], np.float32)
        rgb = rgb * (1 - 0.55 * ret[..., None]) + col * 0.55 * ret[..., None]
        px[..., :3] = rgb
        img.pixels.foreach_set(px.ravel())
        img.save(filepath=path, quality=94)
        bpy.data.images.remove(img)


mode = arg("--mode", "stills")
if mode == "film": film()
elif mode == "film2": film2()
else: stills()
