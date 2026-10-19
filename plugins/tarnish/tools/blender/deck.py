"""El grabador de carretes de Tarnish, para la web y el film. Genérico: ninguna forma ni marca de un fabricante.

Una placa de transporte color óxido sobre una mesa oscura, dos carretes de 10,5" con bridas de aluminio de tres
ventanas, las bobinas de cinta marrón (la que entrega llena, la que recibe a la mitad), brazos de tensión, rodillos
guía, el bloque de cabezas (borrado, grabación, reproducción) con la cinta pasando por su cara, el cabrestante y el
rodillo de presión, y abajo dos VU de cara crema y las teclas del transporte. La cinta tiene desgaste: rayas
longitudinales y manchas de óxido que corren con ella. Una lámpara tibia de costado y un relleno frío.

Corre en pc50-wsl (en ~/tarnish-render, siempre bajo el lock de GPU compartido):
  B=~/ai/blender/blender-5.2.2-linux-x64/blender; L=~/ai/prerendered/.gpu.lock
  flock $L $B -b -P deck.py -- --mode stills --out out --samples 256
  for s in orbit heads top; do flock $L $B -b -P deck.py -- --mode film --shot $s --out film --samples 96; done
Opciones: --pct 50 (pruebas), --shots hero,close, --first N --step N, --cpu.
Unidades: 1 = 1 m. La placa mide 0,62 x 0,46 m; el frente mira hacia -y.
"""
import bpy, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "160"))
PCT = int(arg("--pct", "100"))
FPS = 24.0
IPS15 = 0.381                                  # m/s

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
    sc.cycles.denoising_use_gpu = "--cpu" not in argv
    sc.render.resolution_percentage = PCT
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 92
    return sc

def mat(name, base, rough, metal=0.0, coat=0.0, aniso=0.0, emit=None, strength=0.0, radial=False):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.05
    if aniso:
        b.inputs["Anisotropic"].default_value = aniso
        if radial:
            t = m.node_tree.nodes.new("ShaderNodeTangent"); t.direction_type = "RADIAL"; t.axis = "Z"
            m.node_tree.links.new(t.outputs["Tangent"], b.inputs["Tangent"])
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    return m

def tape_material():
    """Cinta: marrón oscuro satinado, con rayas longitudinales y manchas de óxido que corren con la cinta (el
    desplazamiento del mapeo se anima)."""
    m = bpy.data.materials.new("tape"); m.use_nodes = True; nt = m.node_tree; N = nt.nodes; L = nt.links
    b = N["Principled BSDF"]
    b.inputs["Roughness"].default_value = 0.42
    tc = N.new("ShaderNodeTexCoord"); mp = N.new("ShaderNodeMapping"); mp.name = "scroll"
    L.new(tc.outputs["Object"], mp.inputs["Vector"])        # x: a lo largo de la cinta, z: a lo ancho
    # Rayas: ruido estirado a lo largo de la cinta (u), fino a lo ancho (v).
    sc = N.new("ShaderNodeMapping"); sc.inputs["Scale"].default_value = (3.0, 3.0, 600.0); L.new(mp.outputs["Vector"], sc.inputs["Vector"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 3.0; nz.inputs["Detail"].default_value = 2.0; L.new(sc.outputs[0], nz.inputs["Vector"])
    ramp = N.new("ShaderNodeValToRGB"); ramp.color_ramp.elements[0].position = 0.62; ramp.color_ramp.elements[1].position = 0.7
    L.new(nz.outputs["Fac"], ramp.inputs["Fac"])
    spots = N.new("ShaderNodeTexVoronoi"); spots.inputs["Scale"].default_value = 160.0; L.new(mp.outputs["Vector"], spots.inputs["Vector"])
    sr = N.new("ShaderNodeValToRGB"); sr.color_ramp.elements[0].position = 0.0; sr.color_ramp.elements[1].position = 0.06
    L.new(spots.outputs["Distance"], sr.inputs["Fac"])
    mix1 = N.new("ShaderNodeMix"); mix1.data_type = "RGBA"
    mix1.inputs[6].default_value = (0.045, 0.028, 0.018, 1); mix1.inputs[7].default_value = (0.16, 0.11, 0.075, 1)
    L.new(ramp.outputs["Color"], mix1.inputs["Factor"])
    mix2 = N.new("ShaderNodeMix"); mix2.data_type = "RGBA"; mix2.inputs[7].default_value = (0.26, 0.17, 0.10, 1)
    inv = N.new("ShaderNodeInvert"); L.new(sr.outputs["Color"], inv.inputs["Color"])
    L.new(inv.outputs["Color"], mix2.inputs["Factor"]); L.new(mix1.outputs[2], mix2.inputs[6])
    L.new(mix2.outputs[2], b.inputs["Base Color"])
    return m

def add(o, m):
    o.data.materials.append(m); return o

def cyl(r, h, loc, m, verts=96, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=h, location=loc, rotation=rot)
    o = bpy.context.active_object; bpy.ops.object.shade_smooth(); return add(o, m)

def box(size, loc, m, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc); o = bpy.context.active_object
    o.scale = size; bpy.ops.object.transform_apply(scale=True)
    if bevel:
        md = o.modifiers.new("b", "BEVEL"); md.width = bevel; md.segments = 3
    return add(o, m)

def reel(name, c, pack_r, flange_m, pack_m, hub_m):
    """Un carrete de 10,5": dos bridas con tres ventanas, la bobina entre ellas, el cubo."""
    R, hub = 0.1335, 0.050
    parent = bpy.data.objects.new(name, None); bpy.context.scene.collection.objects.link(parent); parent.location = c
    objs = []
    p = cyl(pack_r, 0.0064, (0, 0, 0.0115), pack_m); objs.append(p)
    for z in (0.0072, 0.0158):
        f = cyl(R, 0.0012, (0, 0, z), flange_m, verts=160)
        for k in range(3):
            a0 = 2 * math.pi * k / 3 + 0.3
            bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.044, depth=0.01, location=(math.cos(a0 + 0.75) * 0.084, math.sin(a0 + 0.75) * 0.084, z))
            cut = bpy.context.active_object; cut.scale = (1.0, 0.62, 1.0)
            cut.rotation_euler = (0, 0, a0 + 0.75 + math.pi / 2)
            md = f.modifiers.new("w", "BOOLEAN"); md.operation = "DIFFERENCE"; md.object = cut; md.solver = "EXACT"
            bpy.context.view_layer.objects.active = f; bpy.ops.object.modifier_apply(modifier="w")
            bpy.data.objects.remove(cut, do_unlink=True)
        objs.append(f)
    h = cyl(hub, 0.012, (0, 0, 0.0115), hub_m); objs.append(h)
    for k in range(3):
        a = 2 * math.pi * k / 3
        objs.append(box((0.02, 0.004, 0.003), (math.cos(a) * 0.03, math.sin(a) * 0.03, 0.019), hub_m))
        objs[-1].rotation_euler = (0, 0, a)
    for o in objs:
        o.location = o.location.copy()
        o.parent = parent
    return parent

def build(scene_state):
    sc = reset()
    oxide = mat("oxide", (0.055, 0.031, 0.021), 0.72)
    metal = mat("alu", (0.72, 0.70, 0.66), 0.3, metal=1.0, aniso=0.6, radial=True)
    steel = mat("steel", (0.7, 0.7, 0.72), 0.12, metal=1.0)
    rubber = mat("rubber", (0.015, 0.014, 0.013), 0.6)
    cream = mat("cream", (0.86, 0.80, 0.68), 0.5)
    table = mat("table", (0.035, 0.028, 0.024), 0.45)
    hubm = mat("hub", (0.82, 0.78, 0.70), 0.35)
    tape = tape_material()
    pack = mat("pack", (0.075, 0.045, 0.028), 0.3, aniso=0.8, radial=True)
    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, -0.03)); add(bpy.context.active_object, table)
    # Placa y chasis.
    box((0.66, 0.50, 0.03), (0, 0.02, -0.015), oxide, bevel=0.006)
    box((0.66, 0.12, 0.03), (0, -0.30, -0.025), mat("panel", (0.09, 0.05, 0.035), 0.5), bevel=0.006)
    # Carretes.
    cS, cT = Vector((-0.165, 0.09, 0.0)), Vector((0.165, 0.09, 0.0))
    rS = reel("supply", cS, scene_state["packS"], metal, pack, hubm)
    rT = reel("takeup", cT, scene_state["packT"], metal, pack, hubm)
    # Bloque de cabezas con su tapa, cabrestante, rodillo, guías y brazos de tensión.
    yH = -0.115
    box((0.15, 0.04, 0.004), (0.0, yH - 0.014, 0.002), mat("headbase", (0.05, 0.048, 0.046), 0.35, metal=0.8), bevel=0.002)
    for i, x in enumerate((-0.045, 0.0, 0.045)):
        cyl (0.011, 0.02, (x, yH - 0.002, 0.014), steel, verts=64)               # cara redondeada hacia la cinta
        box((0.0007, 0.0016, 0.014), (x, yH - 0.0128, 0.014), mat(f"gap{i}", (0.02, 0.02, 0.02), 0.3))
    cyl(0.004, 0.03, (0.085, yH - 0.0105, 0.015), steel, verts=32)                # cabrestante
    cyl(0.014, 0.012, (0.085, yH - 0.0315, 0.012), rubber)                        # rodillo de presión, del lado de afuera
    cyl(0.008, 0.016, (-0.105, yH - 0.004, 0.012), steel, verts=48)
    cyl(0.008, 0.016, (0.125, yH - 0.004, 0.012), steel, verts=48)
    for x, ang in ((-0.10, 0.6), (0.12, -0.6)):
        arm = box((0.05, 0.006, 0.004), (x, yH + 0.055, 0.01), steel); arm.rotation_euler = (0, 0, ang)
    # Cinta: de la bobina que entrega, por la guía, a lo largo de las cabezas, por el cabrestante y a la otra.
    def tangent(c, r, p, cw):
        d = p - c; dist = d.length; base = math.atan2(d.y, d.x); off = math.acos(max(-1, min(1, r / dist)))
        a = base + off if cw else base - off
        return Vector((c.x + r * math.cos(a), c.y + r * math.sin(a), 0.0115))
    yT = yH - 0.0138                                  # la cinta corre por la cara de las cabezas, del lado de afuera
    pts = [tangent(cS, scene_state["packS"], Vector((-0.105, yT + 0.004, 0)), True), Vector((-0.11, yT + 0.002, 0.0115)),
           Vector((-0.06, yT, 0.0115)), Vector((0.06, yT, 0.0115)), Vector((0.085, yT - 0.0008, 0.0115)),
           Vector((0.13, yT + 0.002, 0.0115)), tangent(cT, scene_state["packT"], Vector((0.125, yT + 0.004, 0)), False)]
    cu = bpy.data.curves.new("tape", "CURVE"); cu.dimensions = "3D"; cu.extrude = 0.0032
    sp = cu.splines.new("POLY"); sp.points.add(len(pts) - 1)
    for p, c in zip(sp.points, pts): p.co = (c.x, c.y, c.z, 1)
    for p in sp.points: p.tilt = 0.0
    to = bpy.data.objects.new("tape", cu); sc.collection.objects.link(to); cu.materials.append(tape)
    # VU de cara crema y teclas, sin marcas.
    for k, x in enumerate((-0.12, 0.12)):
        box((0.1, 0.004, 0.06), (x, -0.302, 0.012), cream).rotation_euler = (math.radians(70), 0, 0)
        nd = box((0.0012, 0.0012, 0.045), (x + 0.012 * (1 if k else -1), -0.296, 0.012), mat(f"needle{k}", (0.6, 0.08, 0.05), 0.4))
        nd.rotation_euler = (math.radians(70), math.radians(18 if k else 12), 0)
    for i in range(5):
        box((0.035, 0.03, 0.012), (-0.08 + 0.04 * i, -0.33, 0.004), mat(f"key{i}", (0.75, 0.72, 0.66) if i != 4 else (0.55, 0.12, 0.08), 0.4), bevel=0.003)
    # Luz: una lámpara tibia de costado (la escena de noche) y un relleno frío.
    def area(name, loc, look, size, energy, color):
        l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = energy; l.color = color
        o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
        o.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat("-Z", "X").to_euler(); return o
    area("lamp", (-0.55, -0.35, 0.6), (0, 0, 0), 0.35, 14.0, (1.0, 0.78, 0.52))
    area("fill", (0.6, 0.4, 0.5), (0, 0, 0), 0.8, 3.0, (0.6, 0.75, 1.0))
    area("rim", (0.0, 0.8, 0.25), (0, 0, 0), 0.6, 8.0, (1.0, 0.9, 0.8))
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
    w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.004, 0.0035, 0.003, 1)
    return sc, rS, rT, tape

STATE = {"packS": 0.118, "packT": 0.082}

SHOTS = {
    "hero":  ((0.36, -0.78, 0.44), (0.0, -0.07, 0.0), 42, 8.0, (2400, 1100)),
    "close": ((0.10, -0.30, 0.09), (0.0, -0.112, 0.013), 70, 4.0, (1600, 1100)),
    "top":   ((0.0, -0.05, 0.85), (0.0, -0.02, 0.0), 50, 11.0, (1400, 1400)),
}

def camera(sc, loc, look, lens, fstop):
    cd = bpy.data.cameras.new("cam"); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    cam = bpy.data.objects.new("cam", cd); sc.collection.objects.link(cam); sc.camera = cam
    aim(cam, loc, look); return cam

def aim(cam, loc, look):
    cam.location = Vector(loc); d = Vector(look) - Vector(loc)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); cam.data.dof.focus_distance = d.length

def set_time(t, rS, rT, tape):
    """Posición de la cinta a t segundos: carretes girando con ω = v/r y la textura corriendo."""
    rS.rotation_euler = (0, 0, -t * IPS15 / STATE["packS"])
    rT.rotation_euler = (0, 0, -t * IPS15 / STATE["packT"])
    tape.node_tree.nodes["scroll"].inputs["Location"].default_value = (-t * IPS15 * 1.6, 0, 0)

def stills():
    for name in arg("--shots", "hero,close,top").split(","):
        sc, rS, rT, tape = build(STATE)
        loc, look, lens, fstop, res = SHOTS[name]
        camera(sc, loc, look, lens, fstop); sc.render.resolution_x, sc.render.resolution_y = res
        set_time(3.0, rS, rT, tape)
        sc.render.filepath = os.path.join(OUT, name + ".jpg")
        bpy.ops.render.render(write_still=True)

def film():
    shot = arg("--shot", "orbit"); frames = int(arg("--frames", "144"))
    first = int(arg("--first", "1")); step = int(arg("--step", "1"))
    sc, rS, rT, tape = build(STATE)
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    cam = camera(sc, (0, -1, 0.4), (0, 0, 0), 45, 7.0)
    for f in range(first, frames + 1, step):
        t = (f - 1) / FPS; u = (f - 1) / (frames - 1)
        if shot == "orbit":
            a = math.radians(-120 + 40 * u)
            loc = (0.78 * math.cos(a), 0.78 * math.sin(a), 0.32 - 0.05 * u); look = (0.0, -0.02, 0.0); cam.data.lens = 45; cam.data.dof.aperture_fstop = 7.0
        elif shot == "heads":
            loc = (0.16 - 0.10 * u, -0.27, 0.06); look = (0.0 - 0.02 * u, -0.112, 0.012); cam.data.lens = 85; cam.data.dof.aperture_fstop = 3.2
        else:
            loc = (0.0, -0.05, 0.95 - 0.25 * u); look = (0.0, -0.02, 0.0); cam.data.lens = 50; cam.data.dof.aperture_fstop = 11.0
        aim(cam, loc, look)
        set_time(t, rS, rT, tape)
        sc.render.filepath = os.path.join(OUT, f"{shot}_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)

if arg("--mode", "stills") == "film": film()
else: stills()
