"""Un orrery de bronce para la web de Orrery: sol encendido, ocho planetas (los colores de los carriles)
en brazos de distinto largo y altura, anillos finos que marcan las órbitas, sobre un pedestal oscuro.

Corre en pc50-wsl:
  blender -b -P orrery.py -- --mode hero --out out --samples 160
  blender -b -P orrery.py -- --mode film --shot wide --frames 120 --out out --samples 40
Unidades: 1 = 10 cm.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "160"))
LANES = [(0.498, 0.820, 0.780), (0.663, 0.612, 0.941), (0.941, 0.612, 0.718), (0.549, 0.753, 0.961),
         (0.722, 0.890, 0.627), (0.949, 0.627, 0.482), (0.847, 0.706, 0.973), (0.961, 0.886, 0.478)]
PERIODS = [13, 17, 19, 23, 29, 31, 37, 41]      # vueltas lentas y distintas, como los lazos de los carriles

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    try:
        prefs.compute_device_type = "CUDA"; prefs.get_devices()
        for d in prefs.devices: d.use = d.type == "CUDA"
    except Exception: pass
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    if hasattr(sc.cycles, "denoising_use_gpu"): sc.cycles.denoising_use_gpu = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.006, 0.007, 0.016, 1); bg.inputs["Strength"].default_value = 1.0
    return sc

def mat(name, col, rough, metal=0.0, emit=None, coat=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if emit:
        b.inputs["Emission Color"].default_value = (*emit[0], 1); b.inputs["Emission Strength"].default_value = emit[1]
    return m

def link(o): bpy.context.collection.objects.link(o); return o
def mesh_obj(name, bm, m, smooth=True):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    if smooth:
        for p in me.polygons: p.use_smooth = True
    me.materials.append(m); return link(bpy.data.objects.new(name, me))
def sphere(name, r, m, loc, seg=64):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=seg // 2, radius=r)
    o = mesh_obj(name, bm, m); o.location = loc; return o
def cyl(name, r, h, m, loc, seg=48, rot=None):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=h)
    o = mesh_obj(name, bm, m); o.location = loc
    if rot: o.rotation_euler = rot
    return o
def torus(name, R, r, m, z):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=192, minor_segments=12, location=(0, 0, z))
    o = bpy.context.active_object; o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    return o

M = {}
def build():
    sc = reset()
    M["brass"] = mat("Brass", (0.78, 0.56, 0.26), 0.28, metal=1.0)
    M["brassDark"] = mat("BrassDark", (0.45, 0.32, 0.15), 0.4, metal=1.0)
    M["base"] = mat("Base", (0.012, 0.012, 0.016), 0.35, coat=0.6)
    M["floor"] = mat("Floor", (0.008, 0.009, 0.013), 0.75)
    M["sun"] = mat("Sun", (1.0, 0.5, 0.12), 0.35, emit=((1.0, 0.46, 0.1), 1.6))
    # Piso y pedestal.
    cyl("Floor", 60.0, 0.1, M["floor"], (0, 0, -0.05), seg=96)
    cyl("Plinth", 3.4, 0.6, M["base"], (0, 0, 0.3), seg=128)
    torus("PlinthRing", 3.38, 0.05, M["brass"], 0.6)
    cyl("Column", 0.16, 3.2, M["brass"], (0, 0, 2.2), seg=48)
    sphere("Sun", 0.75, M["sun"], (0, 0, 3.9))
    l = bpy.data.lights.new("SunLight", "POINT"); l.energy = 450; l.color = (1.0, 0.75, 0.45); l.shadow_soft_size = 0.7
    link(bpy.data.objects.new("SunLight", l)).location = (0, 0, 3.9)
    pivots = []
    for k in range(8):
        r = 1.55 + 0.42 * k; z = 1.2 + 0.28 * k
        piv = link(bpy.data.objects.new(f"Pivot{k}", None)); piv.location = (0, 0, 0)
        collar = cyl(f"Collar{k}", 0.24, 0.12, M["brassDark"], (0, 0, z)); collar.parent = piv
        arm = cyl(f"Arm{k}", 0.025, r, M["brass"], (r / 2, 0, z), seg=16, rot=(0, math.pi / 2, 0)); arm.parent = piv
        post = cyl(f"Post{k}", 0.02, 0.5, M["brass"], (r, 0, z + 0.25), seg=12); post.parent = piv
        sat = tuple(max(0.0, min(1.0, (c - 0.55) * 2.2 + 0.3)) for c in LANES[k])
        pm = mat(f"Planet{k}", sat, 0.3, emit=(sat, 0.12), coat=0.5)
        size = [0.17, 0.21, 0.15, 0.19, 0.14, 0.24, 0.16, 0.13][k]
        pl = sphere(f"Planet{k}", size, pm, (r, 0, z + 0.5 + size)); pl.parent = piv
        piv.rotation_euler.z = k * 2.399      # ángulo dorado: arrancan repartidos
        pivots.append(piv)
        torus(f"Orbit{k}", r, 0.004, M["brassDark"], z + 0.5 + size)
    # Luces de estudio.
    for nm, loc, e, size, col in (("Key", (-9, -7, 9), 2600, 6, (1.0, 0.95, 0.88)), ("Rim", (8, 9, 5), 1600, 5, (0.55, 0.65, 1.0)),
                                  ("Fill", (6, -9, 2), 500, 8, (0.7, 0.75, 1.0))):
        li = bpy.data.lights.new(nm, "AREA"); li.energy = e; li.size = size; li.color = col
        o = link(bpy.data.objects.new(nm, li)); o.location = loc
        o.rotation_euler = (Vector((0, 0, 2.5)) - o.location).to_track_quat("-Z", "Y").to_euler()
    return pivots

def camera(loc, target, lens=45, res=(2400, 1100), fstop=4.0):
    sc = bpy.context.scene
    for o in [o for o in bpy.data.objects if o.type == "CAMERA"]: bpy.data.objects.remove(o)
    cam = bpy.data.cameras.new("Cam"); cam.lens = lens; cam.dof.use_dof = True; cam.dof.aperture_fstop = fstop
    o = link(bpy.data.objects.new("Cam", cam)); sc.camera = o
    o.location = loc; t = Vector(target)
    o.rotation_euler = (t - o.location).to_track_quat("-Z", "Y").to_euler()
    cam.dof.focus_distance = (t - o.location).length
    sc.render.resolution_x, sc.render.resolution_y = res
    return o

def render(path): bpy.context.scene.render.filepath = path; bpy.ops.render.render(write_still=True)

def mode_hero():
    pivots = build()
    shots = {"hero": ((13.5, -10.5, 6.4), (0.4, 0, 2.2), 40, (2400, 1100)),
             "close": ((7.6, -6.2, 5.0), (0.6, 0, 3.0), 50, (1600, 1200)),     # 4:3, las tarjetas de la web
             "top": ((0.01, -1.5, 16.0), (0, 0, 2.0), 40, (1600, 1200))}
    for nm in (arg("--shots") or "hero,close,top").split(","):
        loc, tgt, lens, res = shots[nm]
        camera(Vector(loc), tgt, lens, res)
        render(os.path.join(OUT, f"orrery_{nm}.png"))

def mode_film():
    pivots = build()
    shot = arg("--shot", "wide"); frames = int(arg("--frames", "120"))
    os.makedirs(os.path.join(OUT, f"film_{shot}"), exist_ok=True)
    for f in range(frames):
        t = f / 24.0
        for k, piv in enumerate(pivots): piv.rotation_euler.z = k * 2.399 + t * 2 * math.pi / PERIODS[k] * 1.6
        e = f / max(1, frames - 1); e = e * e * (3 - 2 * e)
        if shot == "wide":
            a = -0.6 + 0.5 * e; d = 13.5 - 1.5 * e
            loc = (d * math.cos(a), d * math.sin(a), 6.0 - 0.8 * e); tgt = (0, 0, 2.6); lens = 40
        elif shot == "close":
            a = -0.9 + 0.35 * e
            loc = (7.4 * math.cos(a), 7.4 * math.sin(a), 5.4 + 0.3 * e); tgt = (0.3, 0, 3.2); lens = 38
        else:
            loc = (0.6 * math.sin(e), -1.0 - 1.5 * e, 15.5 - 2.0 * e); tgt = (0, 0, 2.0); lens = 38
        camera(Vector(loc), tgt, lens, (1920, 1080), 5.6)
        render(os.path.join(OUT, f"film_{shot}", f"f_{f + 1:04d}.png"))

{"hero": mode_hero, "film": mode_film}[arg("--mode", "hero")]()
