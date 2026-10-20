"""Escena de Plumb para Blender 5.2 (Cycles): una plomada de bronce que cuelga de un riel sobre una pizarra grabada.

    blender -b --factory-startup -P plumb.py -- --mode ui --out <dir> [--samples 128]
        bg.jpg      la pizarra entera, vista de frente (2400 × 1520: la interfaz a 2x), con su marco y el riel
        bob.png     la plomada sola, vertical, con el cordel; el punto de giro es el centro del borde superior
        knob.png    tapa de bronce de perilla vista desde arriba, sin índice (la interfaz lo dibuja)
    blender -b --factory-startup -P plumb.py -- --mode photos --out <dir> [--shots hero,close,top]
    blender -b --factory-startup -P plumb.py -- --mode film --shot swing|slate|orbit --out <dir>

Todo es original: ninguna marca, ningún producto copiado.
"""
import bpy, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "128"))
FPS = 24

# Medidas (m): la pizarra de 0,60 × 0,38 (la ventana de 1200 × 760 px a 2 mm por px).
SW, SH = 0.60, 0.38

def reset():
    global BRASS
    BRASS = None                                   # read_factory_settings borra los materiales
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
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True; sc.cycles.denoising_use_gpu = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Base Contrast"
    return sc

def mat(name, base, rough, metal=0.0, aniso=0.0, coat=0.0, emit=None, strength=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if aniso: b.inputs["Anisotropic"].default_value = aniso
    if coat: b.inputs["Coat Weight"].default_value = coat
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    return m

def slate_material():
    """Piedra pizarra: gris azulado muy oscuro con vetas suaves y rugosidad que cambia (ruido procedural)."""
    m = bpy.data.materials.new("slate"); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (1.0, 2.5, 1.0)
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 18.0; nz.inputs["Detail"].default_value = 12.0; nz.inputs["Roughness"].default_value = 0.62
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"]); nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.022, 0.025, 0.029, 1); ramp.color_ramp.elements[1].color = (0.034, 0.038, 0.043, 1)
    nt.links.new(nz.outputs["Fac"], ramp.inputs["Fac"]); nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    rr = nt.nodes.new("ShaderNodeMapRange"); rr.inputs["To Min"].default_value = 0.48; rr.inputs["To Max"].default_value = 0.75
    nt.links.new(nz.outputs["Fac"], rr.inputs["Value"]); nt.links.new(rr.outputs["Result"], b.inputs["Roughness"])
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08
    nt.links.new(nz.outputs["Fac"], bump.inputs["Height"]); nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

BRASS = None

def brass():
    global BRASS
    if BRASS is None: BRASS = mat("brass", (0.78, 0.56, 0.26), 0.28, metal=1.0, aniso=0.3)
    return BRASS

def plumb_bob(name="bob", scale=1.0):
    """Plomada clásica: cuerpo cónico torneado con una cintura, tapa con argolla, punta afilada. Origen en la argolla."""
    h = 0.10 * scale; r = 0.026 * scale
    # Perfil de revolución (r, z) desde la punta (z = −h) hasta la tapa (z = 0).
    prof = [(0.0, -h), (r * 0.12, -h * 0.94), (r * 0.55, -h * 0.70), (r * 0.92, -h * 0.40), (r, -h * 0.30), (r * 0.98, -h * 0.22),
            (r * 0.80, -h * 0.18), (r * 0.82, -h * 0.13), (r * 0.62, -h * 0.08), (r * 0.30, -h * 0.05), (r * 0.30, -h * 0.02), (0.0, -h * 0.02)]
    verts = []; faces = []; seg = 96
    for i in range(seg):
        a = 2 * math.pi * i / seg
        for (rr, z) in prof: verts.append((rr * math.cos(a), rr * math.sin(a), z))
    n = len(prof)
    for i in range(seg):
        j = (i + 1) % seg
        for k in range(n - 1):
            faces.append((i * n + k, j * n + k, j * n + k + 1, i * n + k + 1))
    me = bpy.data.meshes.new(name); me.from_pydata(verts, [], faces); me.update()
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(brass())
    for p in o.data.polygons: p.use_smooth = True
    # Argolla.
    bpy.ops.mesh.primitive_torus_add(major_radius=0.006 * scale, minor_radius=0.0014 * scale, location=(0, 0, 0.004 * scale), rotation=(math.pi / 2, 0, 0))
    ring = bpy.context.active_object; ring.data.materials.append(brass()); ring.parent = o
    for p in ring.data.polygons: p.use_smooth = True
    return o

def cord(top, bottom, name="cord"):
    """Cordel de algodón entre dos puntos."""
    d = Vector(bottom) - Vector(top); L = d.length
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.0007, depth=L, location=(Vector(top) + Vector(bottom)) / 2)
    c = bpy.context.active_object; c.name = name
    c.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    c.data.materials.append(mat("cotton", (0.82, 0.78, 0.68), 0.9))
    return c

def area(sc, name, loc, look, size, energy, color=(1, 1, 1)):
    l = bpy.data.lights.new(name, "AREA"); l.size = size; l.energy = energy; l.color = color
    o = bpy.data.objects.new(name, l); o.location = loc; sc.collection.objects.link(o)
    o.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    return o

def slate_board(engrave=True):
    """Pizarra en el plano XY (frente hacia +Z), centrada en el origen, con marco de bronce y el riel arriba a la izquierda."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.006))
    s = bpy.context.active_object; s.scale = (SW, SH, 0.012); bpy.ops.object.transform_apply(scale=True)
    md = s.modifiers.new("b", "BEVEL"); md.width = 0.002; md.segments = 3
    s.data.materials.append(slate_material())
    # Marco: cuatro listones de bronce.
    t = 0.006
    for (x, y, w, h) in ((0, SH / 2 + t / 2, SW + 2 * t, t), (0, -SH / 2 - t / 2, SW + 2 * t, t), (SW / 2 + t / 2, 0, t, SH), (-SW / 2 - t / 2, 0, t, SH)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.001)); f = bpy.context.active_object
        f.scale = (w, h, 0.008); bpy.ops.object.transform_apply(scale=True)
        m2 = f.modifiers.new("b", "BEVEL"); m2.width = 0.0015; m2.segments = 3
        f.data.materials.append(brass())
    # Riel y gancho del que cuelga la plomada (arriba a la izquierda, sobre la columna de la plomada).
    px, py = -SW / 2 + 0.075, SH / 2 - 0.040
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.0035, depth=0.11, location=(px, py + 0.003, 0.012), rotation=(0, math.pi / 2, 0))
    rail = bpy.context.active_object; rail.data.materials.append(brass())
    for p in rail.data.polygons: p.use_smooth = True
    for dx in (-0.05, 0.05):
        bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.004, depth=0.012, location=(px + dx, py + 0.003, 0.006), rotation=(0, 0, 0))
        bpy.context.active_object.data.materials.append(brass())
    # Grabados: la escala de cents bajo la plomada (líneas finas, apenas más claras que la piedra).
    if engrave:
        ink = mat("engrave", (0.12, 0.13, 0.14), 0.85)
        cx, cy, rad = px, py - 0.150, 0.065
        for i in range(-10, 11):
            a = math.radians(i * 2.5)
            L = 0.009 if i % 5 == 0 else 0.005
            x0, y0 = cx + math.sin(a) * rad, cy - math.cos(a) * rad
            bpy.ops.mesh.primitive_cube_add(size=1, location=(x0, y0, 0.00005))
            g = bpy.context.active_object; g.scale = (0.0007, L, 0.0001); g.rotation_euler = (0, 0, a); g.data.materials.append(ink)
    return (px, py + 0.003, 0.012)

def ui():
    # Fondo: la pizarra de frente, cámara ortográfica que llena exactamente la ventana.
    sc = reset()
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.01, 0.01, 0.011, 1)
    slate_board(engrave=True)
    area(sc, "key", (-0.25, 0.35, 0.55), (0, 0, 0), 0.6, 3.2, (1.0, 0.96, 0.9))
    area(sc, "fill", (0.4, -0.3, 0.35), (0, 0, 0), 0.5, 0.7, (0.8, 0.88, 1.0))
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"; cd.ortho_scale = SW + 0.012
    cam = bpy.data.objects.new("cam", cd); cam.location = (0, 0, 1.0); sc.collection.objects.link(cam); sc.camera = cam
    sc.render.resolution_x = 2400; sc.render.resolution_y = int(2400 * (SH + 0.012) / (SW + 0.012))
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 92
    sc.render.filepath = os.path.join(OUT, "bg.jpg"); bpy.ops.render.render(write_still=True)
    # Plomada sola, vertical, con el cordel desde el punto de giro; fondo transparente.
    sc = reset()
    # Cordel de 0,12 m: la punta queda sobre la escala grabada (0,22 m bajo el gancho).
    o = plumb_bob(); o.location = (0, 0, -0.12)
    cord((0, 0, 0), (0, 0, -0.12 + 0.004))
    area(sc, "key", (-0.3, 0.4, 0.2), (0, 0, -0.2), 0.5, 18.0, (1.0, 0.96, 0.9))
    area(sc, "rim", (0.4, -0.2, -0.1), (0, 0, -0.2), 0.4, 8.0, (0.8, 0.88, 1.0))
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"; cd.ortho_scale = 0.24
    cam = bpy.data.objects.new("cam", cd); cam.location = (0.0, -1.0, -0.12); sc.collection.objects.link(cam); sc.camera = cam
    cam.rotation_euler = (math.pi / 2, 0, 0)
    sc.render.resolution_x = 240; sc.render.resolution_y = 720           # 0,08 × 0,24 m; el giro es el centro del borde superior
    sc.render.film_transparent = True; sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    sc.render.filepath = os.path.join(OUT, "bob.png"); bpy.ops.render.render(write_still=True)
    # Tapa de perilla de bronce vista desde arriba.
    sc = reset()
    r = 0.012
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=r, depth=r * 0.4, location=(0, 0, r * 0.2))
    sk = bpy.context.active_object; sk.data.materials.append(mat("dark", (0.03, 0.03, 0.032), 0.5, metal=0.3))
    for k in range(40):
        a = 2 * math.pi * k / 40
        bpy.ops.mesh.primitive_cube_add(size=1, location=(math.cos(a) * r * 0.87, math.sin(a) * r * 0.87, r * 0.42))
        g = bpy.context.active_object; g.scale = (r * 0.24, r * 0.05, r * 0.08); g.rotation_euler = (0, 0, a); g.data.materials.append(brass())
    bpy.ops.mesh.primitive_cylinder_add(vertices=128, radius=r * 0.74, depth=r * 0.9, location=(0, 0, r * 0.85))
    top = bpy.context.active_object; top.data.materials.append(brass())
    md = top.modifiers.new("b", "BEVEL"); md.width = r * 0.12; md.segments = 6; md.limit_method = "ANGLE"
    bpy.ops.object.shade_smooth()
    area(sc, "key", (-0.05, 0.12, 0.4), (0, 0, 0), 0.3, 1.2, (1.0, 0.96, 0.9))
    cd = bpy.data.cameras.new("cam"); cd.type = "ORTHO"; cd.ortho_scale = r * 2 + 0.008
    cam = bpy.data.objects.new("cam", cd); cam.location = (0, 0, 0.6); sc.collection.objects.link(cam); sc.camera = cam
    sc.render.resolution_x = sc.render.resolution_y = 192
    sc.render.film_transparent = True; sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    sc.render.filepath = os.path.join(OUT, "knob.png"); bpy.ops.render.render(write_still=True)

# ── Fotos y film: la plomada colgando sobre la pizarra, con la línea de tiza de una frase cantada ──
def chalk_line(points, z=0.0004, name="chalk", color=(0.92, 0.9, 0.84), strength=0.6, width=0.0011):
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = width; cu.bevel_resolution = 2
    sp = cu.splines.new("POLY"); sp.points.add(len(points) - 1)
    for i, (x, y) in enumerate(points): sp.points[i].co = (x, y, z, 1)
    o = bpy.data.objects.new(name, cu); bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat(name, color, 0.9, emit=color, strength=strength))
    return o

def phrase(t0=0.0, n=220):
    """Una frase cantada (altura en semitonos alrededor de un centro) y su versión corregida: escalones con vibrato."""
    notes = [(0.0, 0.0), (0.55, 2.0), (1.0, 4.0), (1.5, 2.0), (1.9, 0.0), (2.6, -1.0), (3.2, 0.0)]
    raw, fixed = [], []
    for i in range(n):
        t = t0 + 3.6 * i / (n - 1)
        k = max(j for j, (ts, _) in enumerate(notes) if ts <= t)
        target = notes[k][1]
        age = t - notes[k][0]
        scoop = -0.6 * math.exp(-age / 0.06)
        off = 0.28 * math.sin(1.7 * k + 0.3)
        vib = 0.22 * math.sin(2 * math.pi * 5.5 * t) * min(1.0, max(0.0, (age - 0.15) / 0.2))
        raw.append((t, target + off + scoop + vib)); fixed.append((t, target + vib * 0.9 + scoop * 0.3))
    return raw, fixed

def scene_photo(sc_t=1.2):
    sc = reset()
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.006, 0.006, 0.007, 1)
    pivot = slate_board(engrave=True)
    # Líneas de tiza: lo cantado (blanco) y lo corregido (bronce), en el plano de la pizarra.
    raw, fixed = phrase()
    to_xy = lambda t, s: (-SW / 2 + 0.17 + (SW - 0.21) * (t / 3.6), -0.02 + s * 0.012)
    chalk_line([to_xy(t, s) for t, s in raw], name="raw", color=(0.75, 0.75, 0.72), strength=0.04)
    chalk_line([to_xy(t, s) for t, s in fixed], name="fixed", color=(0.95, 0.45, 0.08), strength=0.35)
    # Brazo del gancho: saca el punto de giro 3 cm hacia afuera para que la plomada cuelgue libre.
    px, py, pz = pivot
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.0025, depth=0.03, location=(px, py, pz + 0.015))
    bpy.context.active_object.data.materials.append(brass())
    # Todo lo de la pizarra cuelga de un vacío que la pone de pie sobre la mesa, de frente a −Y.
    root = bpy.data.objects.new("root", None); sc.collection.objects.link(root)
    for o in list(sc.objects):
        if o is not root and o.parent is None: o.parent = root
    root.rotation_euler.x = math.radians(90); bpy.context.view_layer.update()
    pv = root.matrix_world @ Vector((px, py, pz + 0.03))
    bpy.ops.mesh.primitive_plane_add(size=3, location=(0, 0, -SH / 2 - 0.006))
    bpy.context.active_object.data.materials.append(mat("table", (0.035, 0.028, 0.022), 0.55))
    # La plomada cuelga del gancho, desviada apenas por el error de la nota.
    bob = plumb_bob(scale=0.7)
    L = 0.14; ang = math.radians(7.0)
    bob.location = pv + Vector((math.sin(ang) * L, 0.0, -math.cos(ang) * L))
    bob.rotation_euler = (0, -ang, 0)
    cord(pv, bob.location + Vector((0, 0, 0.004)))
    area(sc, "key", (-0.5, -0.6, 0.6), (0, 0, 0), 0.7, 22.0, (1.0, 0.95, 0.88))
    area(sc, "fill", (0.6, -0.4, 0.2), (0, 0, 0), 0.6, 5.0, (0.75, 0.85, 1.0))
    area(sc, "rim", (0.0, 0.6, 0.5), (0, 0, 0), 0.6, 8.0, (1.0, 0.9, 0.8))
    return sc, bob, pv, L

PSHOTS = {
    "hero":  ((0.50, -1.05, 0.10), (-0.02, 0.0, -0.01), 40, 6.0, (2400, 1100)),
    "close": ((-0.05, -0.30, 0.04), (-0.20, -0.04, -0.02), 75, 2.8, (1600, 1100)),
    "top":   ((-0.05, -0.30, 0.30), (-0.12, 0.0, 0.02), 50, 4.0, (1400, 1100)),
}

def pcamera(sc, loc, look, lens, fstop):
    cd = bpy.data.cameras.new("pcam"); cd.lens = lens; cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    cam = bpy.data.objects.new("pcam", cd); sc.collection.objects.link(cam); sc.camera = cam
    cam.location = Vector(loc); d = Vector(look) - Vector(loc)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); cd.dof.focus_distance = d.length
    return cam

def photos():
    for name in arg("--shots", "hero,close,top").split(","):
        sc, bob, pv, L = scene_photo()
        loc, look, lens, fstop, res = PSHOTS[name]
        pcamera(sc, loc, look, lens, fstop); sc.render.resolution_x, sc.render.resolution_y = res
        sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 92
        sc.render.filepath = os.path.join(OUT, name + ".jpg"); bpy.ops.render.render(write_still=True)

def film():
    shot = arg("--shot", "swing"); frames = int(arg("--frames", "144"))
    first = int(arg("--first", "1")); step = int(arg("--step", "1"))
    sc, bob, pv, L = scene_photo()
    cordo = [o for o in sc.objects if o.name.startswith("cord")][0]
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 92
    cam = pcamera(sc, (0.4, -0.8, 0.1), (0, 0, 0), 40, 6.0)
    for f in range(first, frames + 1, step):
        t = (f - 1) / FPS; u = (f - 1) / (frames - 1)
        # La plomada: oscila con el error de una nota desafinada y se calma cuando la corrección la alcanza.
        ang = math.radians(9.0) * math.exp(-t / 1.6) * math.cos(2 * math.pi * 0.9 * t)
        bob.location = pv + Vector((math.sin(ang) * L, 0.0, -math.cos(ang) * L)); bob.rotation_euler = (0, -ang, 0)
        d = bob.location + Vector((0, 0, 0.004)) - pv
        cordo.location = (pv + bob.location + Vector((0, 0, 0.004))) / 2; cordo.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
        cordo.scale = (1, 1, d.length / L)
        if shot == "swing":
            loc = (-0.05 + 0.10 * u, -0.38, 0.12); look = (-0.20, 0.0, -0.03); cam.data.lens = 70; cam.data.dof.aperture_fstop = 3.2
        elif shot == "slate":
            loc = (0.10 + 0.12 * u, -0.42, 0.04); look = (0.05 + 0.10 * u, 0.0, -0.02); cam.data.lens = 60; cam.data.dof.aperture_fstop = 3.5
        else:
            a = math.radians(-115 + 30 * u)
            loc = (1.05 * math.cos(a), 1.05 * math.sin(a), 0.12); look = (0, 0, 0); cam.data.lens = 42; cam.data.dof.aperture_fstop = 7.0
        cam.location = Vector(loc); dd = Vector(look) - Vector(loc)
        cam.rotation_euler = dd.to_track_quat("-Z", "Y").to_euler(); cam.data.dof.focus_distance = dd.length
        sc.render.filepath = os.path.join(OUT, f"{shot}_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)

# ── Film de la web (filmkit): cinco tomas sobre la frase de demostración, primero cantada y después corregida ──
# La plomada es un péndulo amortiguado que cada nota empuja: en la mitad cantada oscila y tarda en calmarse; en la
# corregida (desde CORR s) se calma enseguida. Las líneas de tiza se dibujan al ritmo de la frase.
FILM_SHOTS = [("open", 0.0, 3.5), ("trace", 3.5, 4.0), ("settle", 7.5, 3.0), ("wide", 10.5, 3.5), ("end", 14.0, 4.0)]
CORR = 9.2

def bob_angles(env, fps=FPS):
    """Ángulo de la plomada (rad) por cuadro de todo el film: péndulo de 0,9 Hz empujado en cada nota."""
    import filmkit as fk
    hits = set(fk.onsets(env, thresh=0.18, gap=6))
    w = 2 * math.pi * 0.9; th, v, out = math.radians(2.0), 0.0, []
    for i in range(len(env) + 4 * fps):
        t = i / fps
        if i in hits: v += (1 if (len(out) // 7) % 2 else -1) * math.radians(14.0 if t < CORR else 6.0)
        z = 0.06 if t < CORR else 0.55
        for _ in range(8):
            h = 1.0 / (fps * 8); v += (-w * w * th - 2 * z * w * v) * h; th += v * h
        th = max(-math.radians(20), min(math.radians(20), th)); out.append(th)
    return out

def film2():
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final")
    env = fk.envelope(arg("--audio"))
    angles = bob_angles(env)
    for (name, t0, dur) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        sc, bob, pv, L = scene_photo()
        fk.setup(sc, quality)
        cordo = [o for o in sc.objects if o.name.startswith("cord")][0]
        raw, fixed = bpy.data.objects["raw"], bpy.data.objects["fixed"]
        # Luz de recorte cálida que barre el bronce durante el film.
        sweep = area(sc, "sweep", (-0.6, 0.3, 0.35), (0, 0, 0), 0.25, 0.0, (1.0, 0.82, 0.6))
        cam = fk.camera(sc)
        bobp = lambda: bob.location.copy()
        head = lambda u: Vector((-SW / 2 + 0.17 + (SW - 0.21) * u, -0.01, -0.02))
        C = pv + Vector((0, 0, -0.162))                  # centro de la plomada en reposo
        paths = {
            "open":   fk.Path([(0, C + Vector((0.11, -0.32, 0.035)), C + Vector((0, 0, -0.012)), 70, 2.8),
                               (1, C + Vector((0.05, -0.25, 0.015)), C + Vector((0, 0, -0.010)), 70, 2.8)]),
            "trace":  fk.Path([(0, head(0.0) + Vector((0.02, -0.30, 0.05)), head(0.0), 50, 3.2),
                               (1, head(1.0) + Vector((-0.02, -0.30, 0.04)), head(1.0), 50, 3.2)], easing="linear"),
            "settle": fk.Path([(0, C + Vector((-0.15, -0.12, 0.012)), Vector((-0.11, -0.01, -0.015)), 55, 2.0),
                               (1, C + Vector((-0.13, -0.14, 0.008)), Vector((-0.09, -0.01, -0.015)), 55, 2.0)]),
            "wide":   fk.Path([(0, (0.95 * math.cos(math.radians(-118)), 0.95 * math.sin(math.radians(-118)), 0.10), (0.0, 0.0, -0.01), 40, 5.6),
                               (1, (0.95 * math.cos(math.radians(-78)), 0.95 * math.sin(math.radians(-78)), 0.20), (0.02, 0.0, -0.01), 40, 5.6)]),
            "end":    fk.Path([(0, (0.0, -0.55, 0.03), (0.0, 0.0, -0.005), 45, 4.0),
                               (1, (0.0, -0.86, 0.07), (0.0, 0.0, -0.005), 45, 4.0)], easing="out"),
        }
        path = paths[name]
        frames = int(round(dur * FPS))
        f0 = int(round(t0 * FPS))

        def pose(f, t, u, name=name, path=path, f0=f0):
            g = min(len(angles) - 1, f0 + f - 1); ang = angles[g]
            bob.location = pv + Vector((math.sin(ang) * L, 0.0, -math.cos(ang) * L)); bob.rotation_euler = (0, -ang, 0)
            d = bob.location + Vector((0, 0, 0.004)) - pv
            cordo.location = (pv + bob.location + Vector((0, 0, 0.004))) / 2
            cordo.rotation_euler = d.to_track_quat("Z", "Y").to_euler(); cordo.scale = (1, 1, d.length / L)
            focus = None
            if name == "open": focus = bob.location + Vector((0, 0, -0.02))
            if name == "trace": focus = head(u)
            if name == "settle":                     # de la línea corregida (fondo) a la plomada cuando se calma
                k = fk.rack(u, 0.25, 0.7, 0.0, 1.0)
                focus = Vector((0.03, -0.005, -0.015)).lerp(bob.location + Vector((0, 0, -0.02)), k)
            path.apply(cam, u, focus=focus, shake=0.0012 if name in ("open", "settle") else 0.0006, t=t0 + t)
            return [bob, cordo]

        def keys(f, name=name, f0=f0, frames=frames):
            u = (f - 1) / max(1, frames - 1)
            # Tiza: en "trace" se dibuja la frase; antes no hay nada; después está completa.
            k = {"open": 0.0, "trace": fk.ease(u, "linear"), "settle": 1.0, "wide": 1.0, "end": 1.0}[name]
            for o, lag in ((raw, 0.0), (fixed, 0.06)):
                o.data.bevel_factor_end = max(0.0, min(1.0, k * (1 + lag) - lag)) if k < 1 else 1.0
                o.data.keyframe_insert("bevel_factor_end")
            sweep.data.energy = 6.0 * (0.5 - 0.5 * math.cos(2 * math.pi * ((f0 + f) / FPS) / 7.0))
            sweep.data.keyframe_insert("energy")

        fk.render_shot(sc, name, frames, pose, OUT, extra_keys=keys)

mode = arg("--mode", "ui")
if mode == "ui": ui()
elif mode == "photos": photos()
elif mode == "film2": film2()
else: film()
