"""El plato de un torno de corte con una laca a medio cortar, para la web de Lacquer.

La laca es negra y brillante; el surco es una espiral cuya separación y profundidad siguen el envolvente real del
master de demostración (demo_envelope.json, generado por tools/demo_mix.py + lacquerrender), así que las partes
fuertes de la canción se ven como bandas más anchas y oscuras. El cabezal de corte entra desde la derecha con su
luz roja encendida y la viruta que sale del estilete.

Corre en pc50-wsl (en ~/lacquer-render, siempre bajo el lock de GPU compartido):
  B=~/ai/blender/blender-5.2.2-linux-x64/blender; L=~/ai/prerendered/.gpu.lock
  flock $L $B -b -P lathe.py -- --mode stills --out out --samples 256
  for s in orbit macro top; do flock $L $B -b -P lathe.py -- --mode film --shot $s --frames 216 --out film --samples 64; done
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

if arg("--mode", "stills") == "film": film()
else: stills()
