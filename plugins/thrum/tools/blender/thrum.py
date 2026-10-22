# Thrum: la guitarra, la pedalera, los amplificadores, los gabinetes y los micrófonos en 3D (Blender 5.2, Cycles).
# Estilo "Stage": un escenario oscuro con una sola luz de tungsteno; todo propio, sin marcas ni formas de un fabricante:
# el cuerpo es un escudo asimétrico de lados rectos con una muesca del lado agudo, la pala una paleta 3 + 3.
#
#   blender -b -P thrum.py -- --mode ui [--only guitar,board,pedal,amp,mic] [--samples 96]
#       → plugins/thrum/assets/ui/ (guitar_<k>.jpg, board_<k>.jpg, pedal_<id>.png, amp_<m>_<c>.jpg, mic_<k>.png,
#         layout.json con las proyecciones que usa la interfaz: pastillas, puente y cejuela para Inspect, la geometría
#         de cada diapasón, el LED de cada pedal, el parlante de cada gabinete y la punta de cada micrófono)
#   blender -b -P thrum.py -- --mode photos --out <dir>        → las fotos de la web
#   blender -b --factory-startup -P thrum.py -- --mode film2 --out <dir> [--shot a,b] [--quality draft]
#
# Unidades: metros en Blender, milímetros en las medidas (MM). La guitarra: x del puente (0) hacia la cejuela, y hacia
# las cuerdas graves, z hacia arriba desde la parte de atrás del cuerpo.
import bpy, bmesh, json, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
MODE = arg("--mode", "ui"); SAMPLES = int(arg("--samples", "96"))
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
UI = os.path.abspath(os.path.join(HERE, "..", "..", "assets", "ui"))
MM = 0.001

# ───────── Escena y render ─────────
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
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    try: sc.cycles.denoising_use_gpu = True
    except Exception: pass
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Base Contrast"
    sc.render.resolution_percentage = 100
    return sc

NORENDER = "--layout-only" in argv            # sólo las proyecciones de layout.json, sin volver a renderizar

def render(sc, path, res, transparent=False, fmt="JPEG"):
    if NORENDER: return
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.film_transparent = transparent
    s = sc.render.image_settings
    s.file_format = fmt
    if fmt == "JPEG": s.quality = 90; s.color_mode = "RGB"
    else: s.color_mode = "RGBA"; s.compression = 90
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)

def mat(name, base, rough=0.5, metal=0.0, coat=0.0, emit=None, strength=0.0, grain=None, sheen=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.06
    if sheen: b.inputs["Sheen Weight"].default_value = sheen
    if emit: b.inputs["Emission Color"].default_value = (*emit, 1); b.inputs["Emission Strength"].default_value = strength
    if grain:
        # Veta o textura: ruido estirado sobre una rampa entre dos colores (madera, tolex, tela).
        scale, stretch, c2 = grain
        tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (scale, scale * stretch, scale)
        nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 6.0; nz.inputs["Detail"].default_value = 12.0; nz.inputs["Roughness"].default_value = 0.62
        wv = nt.nodes.new("ShaderNodeTexWave"); wv.inputs["Scale"].default_value = 3.0; wv.inputs["Distortion"].default_value = 6.0; wv.inputs["Detail"].default_value = 6.0
        mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "FLOAT"; mx.inputs[0].default_value = 0.5
        rp = nt.nodes.new("ShaderNodeValToRGB"); rp.color_ramp.elements[0].color = (*base, 1); rp.color_ramp.elements[1].color = (*c2, 1)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"]); nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"]); nt.links.new(mp.outputs["Vector"], wv.inputs["Vector"])
        nt.links.new(nz.outputs["Fac"], mx.inputs[2]); nt.links.new(wv.outputs["Fac"], mx.inputs[3]); nt.links.new(mx.outputs[0], rp.inputs["Fac"])
        nt.links.new(rp.outputs["Color"], b.inputs["Base Color"])
        # Un poco de relieve con el mismo ruido.
        bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.08
        nt.links.new(nz.outputs["Fac"], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m

M = {}
def common_mats():
    M["black"] = mat("black", (0.015, 0.015, 0.016), 0.4)
    M["rubber"] = mat("rubber", (0.02, 0.02, 0.021), 0.85)
    M["chrome"] = mat("chrome", (0.82, 0.82, 0.83), 0.12, 1.0)
    M["nickel"] = mat("nickel", (0.72, 0.7, 0.66), 0.2, 1.0)
    M["gold"] = mat("gold", (0.78, 0.58, 0.3), 0.22, 1.0)
    M["fret"] = mat("fret", (0.82, 0.8, 0.75), 0.18, 1.0)
    M["string"] = mat("string", (0.72, 0.7, 0.64), 0.25, 1.0)
    M["bone"] = mat("bone", (0.86, 0.82, 0.72), 0.35)
    M["pearl"] = mat("pearl", (0.88, 0.86, 0.8), 0.25, coat=0.5)
    M["floor"] = mat("floor", (0.012, 0.011, 0.011), 0.8, grain=(3, 1, (0.02, 0.018, 0.016)))
    M["led_off"] = mat("led_off", (0.25, 0.03, 0.02), 0.25, coat=1.0)
    M["led_on"] = mat("led_on", (1.0, 0.2, 0.08), 0.25, emit=(1.0, 0.22, 0.08), strength=40.0)
    M["tolex"] = mat("tolex", (0.018, 0.017, 0.016), 0.55, grain=(400, 1, (0.012, 0.011, 0.011)))
    M["cone"] = mat("cone", (0.035, 0.03, 0.028), 0.8)
    M["baffle"] = mat("baffle", (0.012, 0.012, 0.012), 0.75)
    M["wood_rose"] = mat("rose", (0.06, 0.03, 0.018), 0.55, grain=(90, 0.08, (0.025, 0.012, 0.007)))
    M["wood_ebony"] = mat("ebony", (0.016, 0.013, 0.012), 0.45, grain=(110, 0.08, (0.006, 0.005, 0.005)))
    M["neck_maple"] = mat("maple", (0.5, 0.29, 0.12), 0.38, grain=(50, 0.1, (0.38, 0.2, 0.07)))

# ───────── Geometría ─────────
def link(o, m=None):
    if m is not None: o.data.materials.clear(); o.data.materials.append(m)
    return o

def smooth(o, bevel=0.0, segs=4):
    if bevel:
        bv = o.modifiers.new("bev", "BEVEL"); bv.width = bevel; bv.segments = segs; bv.limit_method = "ANGLE"
    for p in o.data.polygons: p.use_smooth = True
    return o

def catmull(points, n=16):
    out = []; P = points; k = len(P)
    for i in range(k):
        p0, p1, p2, p3 = P[(i - 1) % k], P[i], P[(i + 1) % k], P[(i + 2) % k]
        for j in range(n):
            t = j / n; t2 = t * t; t3 = t2 * t
            out.append(tuple(0.5 * ((2 * p1[a]) + (-p0[a] + p2[a]) * t + (2 * p0[a] - 5 * p1[a] + 4 * p2[a] - p3[a]) * t2 + (-p0[a] + 3 * p1[a] - 3 * p2[a] + p3[a]) * t3) for a in (0, 1)))
    return out

def extrude(name, pts_mm, z0, depth, m, bevel=0.0, segs=5):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vs = [bm.verts.new((x * MM, y * MM, z0)) for x, y in pts_mm]
    f = bm.faces.new(vs)
    r = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in [g for g in r["geom"] if isinstance(g, bmesh.types.BMVert)]: v.co.z += depth
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o)
    link(o, m); smooth(o, bevel, segs)
    return o

def box(name, c, s, m, bevel=0.0, segs=4):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c); o = bpy.context.object; o.name = name; o.scale = s
    bpy.ops.object.transform_apply(scale=True)
    link(o, m)
    if bevel: smooth(o, bevel, segs)
    return o

def cyl(name, c, r, h, m, rot=(0, 0, 0), verts=48, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=h, location=c, rotation=rot); o = bpy.context.object; o.name = name
    link(o, m); smooth(o, bevel, 3)
    return o

def rod(name, a, b, r, m, verts=12):
    d = b - a
    o = cyl(name, (a + b) / 2, r, d.length, m, verts=verts)
    o.rotation_mode = "QUATERNION"; o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    return o

def group(parts, name):
    root = bpy.data.objects.new(name, None); bpy.context.collection.objects.link(root)
    for p in parts: p.parent = root
    return root

# ───────── La guitarra ─────────
BODY = [(-158, 0), (-152, 112), (-112, 160), (-20, 170), (88, 140), (176, 136), (236, 112), (244, 66), (214, 34),
        (196, -6), (176, -46), (182, -96), (150, -138), (60, -162), (-30, -172), (-118, -160), (-154, -110)]
GUARD = [(-112, -46), (-122, -112), (-60, -146), (40, -142), (140, -104), (150, -50), (180, 30), (120, 46), (20, 48), (-60, 44)]

# Los seis tipos (mismos nombres y orden que Guitar.h). El color y el detalle son de cada uno; la silueta, la misma.
TYPES = [
    dict(id="solid3s", scale=648, frets=22, n=6, body=((0.09, 0.012, 0.012), 0.22, 1.0), guard="black", depth=44, bscale=1.0,
         pickups=[(41, "s"), (99, "s"), (160, "s")], knobs=3, hw="nickel", board="wood_rose", inlay="dot", holes=False, nut=43),
    dict(id="solid2h", scale=628, frets=22, n=6, body=((0.012, 0.012, 0.013), 0.18, 1.0), guard=None, depth=48, bscale=1.0,
         pickups=[(38, "h"), (157, "h")], knobs=4, hw="gold", board="wood_ebony", inlay="dot", holes=False, nut=43),
    dict(id="semi", scale=628, frets=22, n=6, body=((0.28, 0.09, 0.02), 0.2, 1.0), guard="black", depth=50, bscale=1.06,
         pickups=[(40, "h"), (155, "h")], knobs=4, hw="nickel", board="wood_rose", inlay="dot", holes=True, nut=43),
    dict(id="jazz", scale=628, frets=20, n=6, body=((0.55, 0.36, 0.16), 0.24, 1.0), guard="tort", depth=78, bscale=1.14,
         pickups=[(45, "h"), (150, "h")], knobs=2, hw="gold", board="wood_ebony", inlay="block", holes=True, nut=43),
    dict(id="baritone", scale=686, frets=24, n=6, body=((0.012, 0.07, 0.05), 0.2, 1.0), guard="black", depth=44, bscale=1.0,
         pickups=[(40, "h"), (145, "s")], knobs=2, hw="nickel", board="wood_rose", inlay="dot", holes=False, nut=44),
    dict(id="seven", scale=648, frets=24, n=7, body=((0.02, 0.02, 0.022), 0.45, 0.0), guard=None, depth=46, bscale=1.0,
         pickups=[(38, "h"), (140, "h")], knobs=2, hw="chrome", board="wood_ebony", inlay="dot", holes=False, nut=48),
]

def fret_d(scale, n): return scale * (1 - 2 ** (-n / 12.0))      # distancia desde la cejuela
def board_end(t): return t["scale"] - fret_d(t["scale"], t["frets"]) - 8.0    # x del final del diapasón (desde el puente)
def nut_w(t): return t["nut"]
def heel_w(t): return t["nut"] + 13 if t["n"] == 6 else t["nut"] + 16
def board_w(t, x):
    u = (t["scale"] - x) / (t["scale"] - board_end(t))
    return nut_w(t) + (heel_w(t) - nut_w(t)) * max(0.0, min(1.0, u))
def string_y(t, i, x):
    """y (mm) de la cuerda i (0 = la más aguda) en x: del espaciado del puente al de la cejuela."""
    n = t["n"]; sb = 10.5 if n == 6 else 10.0; sn = (nut_w(t) - 7.0) / (n - 1)
    u = x / t["scale"]; sp = sb + (sn - sb) * u
    return -((n - 1) / 2.0 - i) * sp          # aguda del lado −y (agudas)

def build_guitar(t, strings=True):
    parts = []
    T = t["depth"]; s = t["bscale"]
    bm_ = mat("body", t["body"][0], t["body"][1], coat=t["body"][2])
    body_pts = [(x * s + (1 - s) * 30, y * s) for x, y in catmull(BODY, 16)]
    parts.append(extrude("body", body_pts, 0, T * MM, bm_, bevel=0.006 if T < 60 else 0.009))
    if t["holes"]:
        # Aberturas acústicas: ranuras alargadas con los extremos redondos (genéricas).
        for side in (-1, 1):
            cx, cy = -10 * s, side * 108 * s
            pts = []
            for k in range(40):
                a = 2 * math.pi * k / 40
                pts.append((cx + 62 * s * math.cos(a), cy + 6 * s * math.sin(a) + 10 * s * math.sin(a) * math.cos(a) * side))
            o = extrude("fhole", pts, T * MM - 0.0002, 0.6 * MM, M["black"]); o.rotation_euler.z = 0.0; parts.append(o)
            for ex in (-1, 1): parts.append(cyl("fend", ((cx + ex * 62 * s) * MM, (cy) * MM, T * MM + 0.0001), 6.5 * s * MM, 0.6 * MM, M["black"], verts=24))
    gm = None
    if t["guard"] == "black": gm = mat("guard", (0.012, 0.012, 0.014), 0.35)
    elif t["guard"] == "tort": gm = mat("guard", (0.08, 0.035, 0.015), 0.3, coat=0.6, grain=(30, 1.0, (0.02, 0.008, 0.004)))
    if gm: parts.append(extrude("guard", [(x * s, y * s) for x, y in catmull(GUARD, 12)], (T + (6 if t["guard"] == "tort" else 0)) * MM, 2.2 * MM, gm, bevel=0.001))
    hw = M[t["hw"]]
    sc_ = t["scale"]; be = board_end(t)
    # Mástil y diapasón.
    pts = [(be - 26, -board_w(t, be) / 2), (sc_ + 2, -nut_w(t) / 2), (sc_ + 2, nut_w(t) / 2), (be - 26, board_w(t, be) / 2)]
    parts.append(extrude("neck", pts, (T - 20) * MM, 20.2 * MM, M["neck_maple"], bevel=0.003))
    bpts = [(be, -board_w(t, be) / 2), (sc_ + 2, -nut_w(t) / 2), (sc_ + 2, nut_w(t) / 2), (be, board_w(t, be) / 2)]
    parts.append(extrude("board", bpts, (T + 0.2) * MM, 5.5 * MM, M[t["board"]], bevel=0.0012))
    top = (T + 5.7) * MM
    for k in range(1, t["frets"] + 1):
        x = sc_ - fret_d(sc_, k)
        parts.append(cyl("fret", (x * MM, 0, top), 1.1 * MM, board_w(t, x) * MM, M["fret"], rot=(math.pi / 2, 0, 0), verts=16))
    marks = [3, 5, 7, 9, 12, 15, 17, 19, 21] + ([24] if t["frets"] >= 24 else [])
    for k in marks:
        if k > t["frets"]: continue
        x = sc_ - 0.5 * (fret_d(sc_, k) + fret_d(sc_, k - 1))
        if t["inlay"] == "block":
            wdt = board_w(t, x) * 0.7; ln = (fret_d(sc_, k) - fret_d(sc_, k - 1)) * 0.55
            parts.append(box("block", (x * MM, 0, top - 0.25 * MM), (ln * MM, wdt * MM, 0.7 * MM), M["pearl"]))
        else:
            ys = (-12, 12) if k in (12, 24) else (0,)
            for y in ys: parts.append(cyl("dot", (x * MM, y * MM, top - 0.3 * MM), 3.2 * MM, 0.8 * MM, M["pearl"], verts=24))
    parts.append(box("nut", (sc_ * MM, 0, top + 1 * MM), (4 * MM, (nut_w(t) + 1) * MM, 4 * MM), M["bone"], bevel=0.0008))
    # Pala: paleta redondeada; 3 + 3 o 4 + 3.
    hl = 190 if t["n"] == 6 else 215
    H = [(sc_ + 2, -nut_w(t) / 2 - 1), (sc_ + 30, -40), (sc_ + hl - 40, -47), (sc_ + hl - 8, -30), (sc_ + hl, 0), (sc_ + hl - 8, 30), (sc_ + hl - 40, 47), (sc_ + 30, 40), (sc_ + 2, nut_w(t) / 2 + 1)]
    parts.append(extrude("head", catmull(H, 10), (T - 8) * MM, 14 * MM, bm_, bevel=0.003))
    posts = []
    per = [(3, 3)] if t["n"] == 6 else [(4, 3)]
    nb, nt_ = per[0]
    for side, count in ((1, nb), (-1, nt_)):
        for i in range(count):
            x = sc_ + 50 + i * (42 if count == 3 else 34); y = side * 30
            posts.append((x, y, side))
            parts.append(cyl("post", (x * MM, y * MM, (T + 9) * MM), 3.2 * MM, 8 * MM, hw, verts=20))
            parts.append(cyl("bush", (x * MM, y * MM, (T + 6.5) * MM), 5.5 * MM, 1.5 * MM, hw, verts=28))
            parts.append(cyl("key", (x * MM, (y + side * 30) * MM, (T - 1) * MM), 6 * MM, 12 * MM, hw, rot=(math.pi / 2, 0, 0), verts=24, bevel=0.002))
    # Puente: placa y selletas.
    n = t["n"]
    parts.append(box("bridge", (-8 * MM, 0, (T + 3) * MM), (34 * MM, (72 if n == 6 else 80) * MM, 6 * MM), hw, bevel=0.0015))
    for i in range(n):
        y = string_y(t, i, 0)
        parts.append(box("saddle", (-2 * MM, y * MM, (T + 7) * MM), (10 * MM, 8 * MM, 4 * MM), hw, bevel=0.0012))
    # Pastillas.
    pm = mat("pickup", (0.02, 0.02, 0.022), 0.4) if t["id"] != "jazz" else hw
    wpu = 70 if n == 6 else 80
    for d, kind in t["pickups"]:
        if kind == "s":
            parts.append(box("pu", (d * MM, 0, (T + 4) * MM), (18 * MM, wpu * MM, 8 * MM), mat("pucov", (0.86, 0.83, 0.74), 0.35) if t["id"] == "solid3s" else pm, bevel=0.0045, segs=6))
            for i in range(n): parts.append(cyl("pole", (d * MM, string_y(t, i, d) * MM, (T + 8.1) * MM), 2.4 * MM, 0.6 * MM, hw, verts=16))
        else:
            parts.append(box("ring", (d * MM, 0, (T + 1.5) * MM), (46 * MM, (wpu + 12) * MM, 3 * MM), M["black"], bevel=0.002))
            parts.append(box("hb", (d * MM, 0, (T + 5) * MM), (38 * MM, wpu * MM, 7 * MM), pm, bevel=0.002))
            for xx in (-9, 9):
                for i in range(n): parts.append(cyl("pole", ((d + xx) * MM, string_y(t, i, d) * MM, (T + 8.6) * MM), 2.2 * MM, 0.6 * MM, hw, verts=16))
    # Perillas y selector.
    kpos = {2: [(-60, -112), (-104, -110)], 3: [(-40, -100), (-78, -118), (-112, -96)], 4: [(-46, -96), (-86, -116), (-56, -138), (-100, -150)]}[t["knobs"]]
    km = M["gold"] if t["hw"] == "gold" else (M["black"] if t["id"] in ("seven", "baritone") else mat("knobcap", (0.86, 0.83, 0.74), 0.35))
    for (x, y) in kpos: parts.append(cyl("knob", (x * s * MM, y * s * MM, (T + 9) * MM), 9 * MM, 13 * MM, km, verts=40, bevel=0.0015))
    parts.append(box("switch", (60 * s * MM, -116 * s * MM, (T + 6) * MM), (24 * MM, 6 * MM, 3 * MM), hw, bevel=0.001))
    parts.append(cyl("tip", (64 * s * MM, -116 * s * MM, (T + 14) * MM), 3 * MM, 10 * MM, M["black"], verts=16, bevel=0.001))
    # Cuerdas.
    if strings:
        for i in range(n):
            r = [0.23, 0.33, 0.43, 0.6, 0.8, 1.0, 1.25][i] * 0.6
            z0 = (T + 9.2) * MM; z1 = top + 2.6 * MM
            a = Vector((-2 * MM, string_y(t, i, 0) * MM, z0)); b = Vector((sc_ * MM, string_y(t, i, sc_) * MM, z1))
            parts.append(rod("string", a, b, r * MM, M["string"]))
            # A la clavija: las graves a un lado (+y), las agudas al otro.
            gr = [p for p in posts if p[2] == 1]; ag = [p for p in posts if p[2] == -1]
            if i >= n - len(gr): px, py, _ = gr[n - 1 - i]
            else: px, py, _ = ag[min(i, len(ag) - 1)]
            parts.append(rod("tail", b, Vector((px * MM, py * MM, (T + 11) * MM)), r * MM, M["string"]))
    return group(parts, "guitar_" + t["id"])

# ───────── Pedales ─────────
# Mismo orden que Pedals.h (kGate … kReverb). Colores profundos, rótulos dibujados por la interfaz (no por el render).
PEDALS = [
    dict(id="gate", col=(0.06, 0.065, 0.07), knobs=2), dict(id="comp", col=(0.03, 0.10, 0.32), knobs=4),
    dict(id="wah", col=(0.03, 0.03, 0.032), knobs=0), dict(id="od", col=(0.04, 0.26, 0.08), knobs=3),
    dict(id="dist", col=(0.55, 0.2, 0.015), knobs=3), dict(id="fuzz", col=(0.30, 0.03, 0.06), knobs=4),
    dict(id="chorus", col=(0.06, 0.30, 0.34), knobs=4), dict(id="phaser", col=(0.20, 0.06, 0.30), knobs=4),
    dict(id="flanger", col=(0.5, 0.51, 0.53), knobs=5, metal=0.6), dict(id="trem", col=(0.66, 0.57, 0.40), knobs=2),
    dict(id="vib", col=(0.36, 0.22, 0.34), knobs=2), dict(id="delay", col=(0.72, 0.72, 0.69), knobs=6),
    dict(id="reverb", col=(0.035, 0.05, 0.13), knobs=5),
]

def build_pedal(p):
    parts = []
    W, D, H = (74, 122, 48) if p["knobs"] <= 4 else (92, 122, 50)
    em = mat("p_" + p["id"], p["col"], 0.3, p.get("metal", 0.25), coat=0.3)
    if p["id"] == "wah":
        # Pedal de expresión: base y una placa que bascula, con goma.
        W, D = 100, 250
        parts.append(box("base", (0, 0, 22 * MM), (W * MM, D * MM, 44 * MM), em, bevel=0.006))
        tread = box("tread", (0, 0, 0), (W * 0.9 * MM, D * 0.92 * MM, 8 * MM), em, bevel=0.003)
        tread.location = (0, 0, 54 * MM); tread.rotation_euler.x = math.radians(-6); parts.append(tread)
        rub = box("rubber", (0, 0, 0), (W * 0.78 * MM, D * 0.8 * MM, 2 * MM), M["rubber"], bevel=0.001)
        rub.location = (0, 0, 59 * MM); rub.rotation_euler.x = math.radians(-6); parts.append(rub)
        led = cyl("led", (W * 0.38 * MM, -D * 0.44 * MM, 45 * MM), 2.5 * MM, 3 * MM, M["led_off"], verts=16)
        parts.append(led)
        return group(parts, "pedal_wah"), (W, D, 64), led
    # Caja con la tapa inclinada: el perfil lateral extruido a lo ancho (alta atrás, donde van las perillas).
    prof = [(-D / 2, 0), (D / 2, 0), (D / 2, H), (-D / 2 + 8, H * 0.64), (-D / 2, H * 0.64)]
    me = bpy.data.meshes.new(p["id"]); bm = bmesh.new()
    vs = [bm.verts.new((-W / 2 * MM, y * MM, z * MM)) for y, z in prof]
    f = bm.faces.new(vs); r = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in [g for g in r["geom"] if isinstance(g, bmesh.types.BMVert)]: v.co.x += W * MM
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(p["id"], me); bpy.context.collection.objects.link(o); link(o, em); smooth(o, 0.004, 5); parts.append(o)
    k = p["knobs"]
    rows = [[k]] if k <= 3 else [[2, 2]] if k == 4 else [[3, 2]] if k == 5 else [[3, 3]]
    counts = rows[0]
    for ri, cnt in enumerate(counts):
        yy = 40 - ri * 22 if len(counts) > 1 else 36
        for i in range(cnt):
            x = (i - (cnt - 1) / 2) * (22 if W < 80 else 26)
            zt = H * (0.64 + 0.36 * ((yy + D / 2 - 8) / (D - 8))) if yy > -D / 2 + 8 else H
            parts.append(cyl("k", (x * MM, yy * MM, (zt + 5) * MM), 7.2 * MM, 11 * MM, M["black"], verts=32, bevel=0.0015))
            parts.append(box("ptr", (x * MM, (yy - 4.5) * MM, (zt + 10.6) * MM), (1.2 * MM, 5 * MM, 0.6 * MM), M["bone"]))
    parts.append(cyl("sw", (0, -32 * MM, (H * 0.64 + 3) * MM), 7 * MM, 12 * MM, M["chrome"], verts=32, bevel=0.002))
    parts.append(cyl("swb", (0, -32 * MM, (H * 0.64 + 0.5) * MM), 10 * MM, 1.5 * MM, M["chrome"], verts=32))
    led = cyl("led", (0, 6 * MM, (H * 0.78 + 1.2) * MM), 2.6 * MM, 3 * MM, M["led_off"], verts=16)
    led.rotation_euler.x = math.atan2(H * 0.36, D - 8)
    parts.append(led)
    # Jacks a los costados.
    for side in (-1, 1): parts.append(cyl("jack", (side * (W / 2 + 2) * MM, 20 * MM, 26 * MM), 6 * MM, 6 * MM, M["nickel"], rot=(0, math.pi / 2, 0), verts=24))
    return group(parts, "pedal_" + p["id"]), (W, D, H), led

# ───────── Amplificadores y gabinetes ─────────
AMPS = [
    dict(id="clean", panel=(0.62, 0.62, 0.63), panel_metal=1.0, grille=(0.18, 0.2, 0.23), tolex=(0.018, 0.017, 0.016), knobcol=(0.02, 0.02, 0.02)),
    dict(id="crunch", panel=(0.62, 0.48, 0.24), panel_metal=1.0, grille=(0.20, 0.17, 0.12), tolex=(0.018, 0.017, 0.016), knobcol=(0.02, 0.02, 0.02)),
    dict(id="high", panel=(0.03, 0.03, 0.035), panel_metal=0.6, grille=(0.06, 0.06, 0.065), tolex=(0.012, 0.012, 0.013), knobcol=(0.6, 0.6, 0.62)),
    dict(id="combo", panel=(0.58, 0.52, 0.42), panel_metal=0.0, grille=(0.22, 0.12, 0.06), tolex=(0.62, 0.55, 0.42), knobcol=(0.85, 0.8, 0.7)),
]
CABS = [
    dict(id="1x12", W=540, H=470, D=280, spk=[(0, 0)], r=152, open=True),
    dict(id="2x12", W=720, H=470, D=280, spk=[(-170, 0), (170, 0)], r=152, open=True),
    dict(id="4x12", W=760, H=760, D=360, spk=[(-175, 175), (175, 175), (-175, -175), (175, -175)], r=152, open=False),
    dict(id="1x10", W=440, H=400, D=240, spk=[(0, 0)], r=128, open=True),
]

def speaker(c, r, parts):
    """Un parlante a la vista (sin tela): canasta, suspensión, cono, domo. c en el plano del bafle."""
    bpy.ops.mesh.primitive_torus_add(major_radius=r, minor_radius=0.007, location=c + Vector((0, -0.05, 0)), rotation=(math.pi / 2, 0, 0)); t = bpy.context.object; link(t, M["nickel"]); smooth(t); parts.append(t)
    bpy.ops.mesh.primitive_torus_add(major_radius=r * 0.93, minor_radius=0.009, location=c + Vector((0, -0.05, 0)), rotation=(math.pi / 2, 0, 0)); t = bpy.context.object; link(t, M["black"]); smooth(t); parts.append(t)
    me = bpy.data.meshes.new("cone"); bm = bmesh.new()
    prof = [(r * 0.3, 0.0), (r * 0.46, -0.012), (r * 0.66, -0.026), (r * 0.87, -0.044)]
    N = 64; ring = []
    for (rr, yy) in prof: ring.append([bm.verts.new((rr * math.cos(2 * math.pi * k / N), yy, rr * math.sin(2 * math.pi * k / N))) for k in range(N)])
    for a, b in zip(ring[:-1], ring[1:]):
        for k in range(N): bm.faces.new((a[k], a[(k + 1) % N], b[(k + 1) % N], b[k]))
    bm.to_mesh(me); bm.free()
    co = bpy.data.objects.new("cone", me); bpy.context.collection.objects.link(co); co.location = c; link(co, M["cone"]); smooth(co); parts.append(co)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r * 0.33, location=c + Vector((0, -0.004, 0)), segments=48, ring_count=24); d = bpy.context.object
    d.scale = (1, 0.45, 1); link(d, M["black"]); smooth(d); parts.append(d)

def build_stack(a, cb):
    parts = []
    W, H, D = cb["W"], cb["H"], cb["D"]
    tol = mat("tolex_" + a["id"], a["tolex"], 0.55, grain=(400, 1, tuple(x * 0.75 for x in a["tolex"])))
    parts.append(box("cab", (0, 0, H / 2 * MM), (W * MM, D * MM, H * MM), tol, bevel=0.012))
    parts.append(box("baffle", (0, -(D / 2 + 1) * MM, H / 2 * MM), ((W - 44) * MM, 4 * MM, (H - 44) * MM), M["baffle"]))
    for cx, cz in cb["spk"]: speaker(Vector((cx * MM, -(D / 2 + 3) * MM, (H / 2 + cz) * MM)), cb["r"] * MM, parts)
    for side in (-1, 1): parts.append(box("corner", (side * (W / 2 - 14) * MM, -(D / 2 - 10) * MM, 14 * MM), (34 * MM, 26 * MM, 30 * MM), M["nickel"] if a["id"] != "combo" else M["black"], bevel=0.004))
    # El cabezal: tolex, frente con la tela, panel de perillas arriba.
    hw = min(W - 10, 620); hd = 240; hh = 230 if W > 450 else 200
    z0 = H
    parts.append(box("head", (0, 0, (z0 + hh / 2) * MM), (hw * MM, hd * MM, hh * MM), tol, bevel=0.012))
    gr = mat("grille_" + a["id"], a["grille"], 0.9, sheen=0.4, grain=(900, 1, tuple(x * 0.6 for x in a["grille"])))
    parts.append(box("gr", (0, -(hd / 2 + 1) * MM, (z0 + hh * 0.36) * MM), ((hw - 50) * MM, 4 * MM, hh * 0.5 * MM), gr))
    pm = mat("panel_" + a["id"], a["panel"], 0.3, a["panel_metal"])
    parts.append(box("panel", (0, -(hd / 2 + 1) * MM, (z0 + hh * 0.78) * MM), ((hw - 50) * MM, 4 * MM, hh * 0.3 * MM), pm, bevel=0.002))
    km = mat("knob_" + a["id"], a["knobcol"], 0.35)
    nk = 8; span = hw - 120
    for i in range(nk):
        x = -span / 2 + i * span / (nk - 1)
        parts.append(cyl("ak", (x * MM, -(hd / 2 + 10) * MM, (z0 + hh * 0.78) * MM), 12 * MM, 16 * MM, km, rot=(math.pi / 2, 0, 0), verts=32, bevel=0.002))
    parts.append(cyl("jewel", ((hw / 2 - 40) * MM, -(hd / 2 + 4) * MM, (z0 + hh * 0.78) * MM), 8 * MM, 8 * MM, mat("jewel", (1.0, 0.15, 0.05), 0.2, emit=(1.0, 0.12, 0.04), strength=12.0), rot=(math.pi / 2, 0, 0), verts=24))
    parts.append(box("handle", (0, 0, (z0 + hh + 8) * MM), (160 * MM, 30 * MM, 14 * MM), M["rubber"], bevel=0.005))
    return group(parts, "stack"), (W, H, D, hh)

def build_mic(kind):
    """El micrófono apuntando hacia +y (al parlante), con la punta en el origen."""
    parts = []
    if kind == 0:      # dinámico: cuerpo largo y una rejilla chata
        parts.append(cyl("body", (0, -0.085, 0), 0.016, 0.15, M["black"], rot=(math.pi / 2, 0, 0), bevel=0.003))
        parts.append(cyl("grille", (0, -0.008, 0), 0.0185, 0.03, M["nickel"], rot=(math.pi / 2, 0, 0), bevel=0.004))
        parts.append(cyl("xlr", (0, -0.175, 0), 0.012, 0.03, M["nickel"], rot=(math.pi / 2, 0, 0)))
    elif kind == 1:    # condensador de lado: cuerpo grueso y una cápsula redonda arriba
        parts.append(cyl("body", (0, -0.07, -0.06), 0.026, 0.12, M["nickel"], rot=(0, 0, 0), bevel=0.004))
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.031, location=(0, -0.07, 0.012), segments=40, ring_count=20); s = bpy.context.object
        s.scale = (1, 1, 1.25); link(s, mat("mesh", (0.6, 0.6, 0.6), 0.45, 0.8)); smooth(s); parts.append(s)
        parts.append(cyl("band", (0, -0.07, -0.012), 0.0275, 0.006, M["chrome"]))
    else:              # cinta: una caja redondeada con rejilla al frente
        parts.append(box("ribbon", (0, -0.05, 0), (0.05, 0.05, 0.11), mat("ribbonbody", (0.06, 0.055, 0.05), 0.35, 0.7), bevel=0.012, segs=6))
        parts.append(box("rgrille", (0, -0.024, 0), (0.042, 0.004, 0.09), mat("rmesh", (0.55, 0.52, 0.46), 0.5, 0.8), bevel=0.003))
        parts.append(box("yoke", (0, -0.05, -0.075), (0.012, 0.02, 0.04), M["black"]))
    return group(parts, "mic%d" % kind)

# ───────── Luz ─────────
def world(strength=0.05, col=(0.004, 0.004, 0.005)):
    w = bpy.data.worlds.new("w"); bpy.context.scene.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs[0].default_value = (*col, 1); bg.inputs[1].default_value = strength

def light(kind, loc, energy, color=(1, 1, 1), size=1.0, target=None, spot=None, name=None):
    d = bpy.data.lights.new(name or kind, kind); d.energy = energy; d.color = color
    if kind == "AREA": d.size = size
    if kind == "SPOT": d.spot_size = spot or 0.6; d.spot_blend = 0.6; d.shadow_soft_size = size
    o = bpy.data.objects.new(name or kind, d); bpy.context.collection.objects.link(o); o.location = loc
    if target is not None:
        o.rotation_mode = "QUATERNION"; o.rotation_quaternion = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y")
    return o

def stage_lights(t, scale=1.0, key=160):
    t = Vector(t)
    world(0.05)
    light("SPOT", t + Vector((0.4, -0.5, 1.6)) * scale, key * scale ** 2, (1.0, 0.74, 0.45), size=0.15, target=t, spot=0.75, name="key")
    light("AREA", t + Vector((-1.2, 0.8, 0.5)) * scale, 40 * scale ** 2, (0.45, 0.55, 1.0), size=0.5, target=t, name="blue")
    light("AREA", t + Vector((0.2, 1.3, 0.25)) * scale, 30 * scale ** 2, (1.0, 0.45, 0.2), size=0.8, target=t, name="rim")

def camera(loc, target, lens=50, ortho=None, up=None):
    c = bpy.data.cameras.new("cam"); c.lens = lens
    if ortho: c.type = "ORTHO"; c.ortho_scale = ortho
    o = bpy.data.objects.new("cam", c); bpy.context.collection.objects.link(o); o.location = loc
    o.rotation_mode = "QUATERNION"; o.rotation_quaternion = (Vector(target) - Vector(loc)).to_track_quat("-Z", up or "Y")
    bpy.context.scene.camera = o
    return o

def floor(z=0.0, size=14, catcher=False):
    bpy.ops.mesh.primitive_plane_add(size=size, location=(0, 0, z)); f = bpy.context.object; link(f, M["floor"])
    if catcher: f.is_shadow_catcher = True
    return f

def proj(sc, p):
    from bpy_extras.object_utils import world_to_camera_view
    bpy.context.view_layer.update()
    v = world_to_camera_view(sc, sc.camera, p)
    return [round(v.x, 4), round(1 - v.y, 4)]

# ───────── Modo ui ─────────
def ui():
    os.makedirs(UI, exist_ok=True)
    only = set((arg("--only", "") or "").split(",")) - {""}
    lay_path = os.path.join(UI, "layout.json")
    lay = json.load(open(lay_path)) if os.path.exists(lay_path) else {}
    want = lambda k: not only or k in only

    if want("guitar"):
        # Una foto por tipo, misma luz y mismo encuadre (la página Guitar: 780 × 594 a 2x).
        lay["guitar"] = []
        for k, t in enumerate(TYPES):
            sc = reset(); common_mats(); floor()
            g = build_guitar(t)
            g.rotation_euler = (0, 0, math.radians(24)); g.location = (-0.32, -0.12, 0.002)
            stage_lights((-0.15, -0.05, 0.05), 1.0)
            camera((-0.02, -0.78, 1.05), (0.0, 0.02, 0.0), lens=41)
            bpy.context.view_layer.update()                # (si no, matrix_world todavía es la identidad)
            T = t["depth"] * MM + 0.008
            def P(x, y): return proj(sc, g.matrix_world @ Vector((x * MM, y * MM, T)))
            lay["guitar"].append({"id": t["id"], "pickups": [P(d, 0) for d, _ in t["pickups"]], "bridge": P(-2, 0), "nut": P(t["scale"], 0),
                                  "fret12": P(t["scale"] / 2, 0), "pickup_mm": [d for d, _ in t["pickups"]], "kinds": [kd for _, kd in t["pickups"]]})
            render(sc, os.path.join(UI, "guitar_%d.jpg" % k), (1560, 1188))

    if want("board"):
        # El diapasón visto desde arriba, recto, la cejuela a la izquierda y la cuerda aguda arriba, sin cuerdas (las dibuja
        # la interfaz). La tira cubre desde 14 mm antes de la cejuela hasta 16 mm después del último traste.
        lay["board"] = []
        for k, t in enumerate(TYPES):
            sc = reset(); common_mats()
            g = build_guitar(t, strings=False)
            world(0.02)
            sc_ = t["scale"]; dl = fret_d(sc_, t["frets"])
            x0 = sc_ + 14; x1 = sc_ - dl - 16          # en x de la guitarra
            half = heel_w(t) / 2 + 3
            Wpx = 2400; Hpx = int(round(Wpx * (2 * half) / (x0 - x1)))
            Hpx += Hpx % 2
            light("AREA", (((x0 + x1) / 2) * MM, -0.25, 0.7), 9, (1, 0.9, 0.78), size=1.2, target=(((x0 + x1) / 2) * MM, 0, 0))
            light("AREA", (((x0 + x1) / 2) * MM, 0.3, 0.6), 3.6, (1, 1, 1), size=1.2, target=(((x0 + x1) / 2) * MM, 0, 0))
            cx = (x0 + x1) / 2 * MM
            # Arriba de la imagen = −y (agudas), derecha = −x (hacia el puente): la cámara girada 180°.
            cam = camera((cx, 0, 0.8), (cx, 0, 0), ortho=(x0 - x1) * MM, up="Y")
            cam.rotation_euler = (0, 0, math.pi); cam.rotation_mode = "XYZ"; cam.rotation_euler = (0, 0, math.pi)
            lay["board"].append({"id": t["id"], "scale": sc_, "frets": t["frets"], "strings": t["n"], "padNut": 14, "padEnd": 16,
                                 "halfMm": half, "nutW": nut_w(t), "heelW": heel_w(t), "spacingBridge": 10.5 if t["n"] == 6 else 10.0,
                                 "w": Wpx, "h": Hpx})
            render(sc, os.path.join(UI, "board_%d.jpg" % k), (Wpx, Hpx))

    if want("pedal"):
        # Cada pedal solo, con su sombra, visto desde arriba y adelante con cámara ortográfica (así encajan en cualquier
        # lugar de la pedalera). El LED: posición proyectada (la interfaz lo enciende).
        lay["pedal"] = []
        for p in PEDALS:
            sc = reset(); common_mats()
            floor(catcher=True)
            g, (W, D, H), led = build_pedal(p)
            stage_lights((0, 0, 0.03), 0.6, key=160)
            ortho = 0.27 if p["id"] == "wah" else 0.145
            camera((0, -0.42, 0.62), (0, 0.0, 0.02), ortho=ortho)
            res = (480, 640) if p["id"] != "wah" else (480, 640)
            lay["pedal"].append({"id": p["id"], "led": proj(sc, led.matrix_world.translation), "ortho": ortho, "w": res[0], "h": res[1],
                                 "top": proj(sc, Vector((0, (D / 2) * MM, H * MM))), "bottom": proj(sc, Vector((0, -(D / 2) * MM, 0)))})
            render(sc, os.path.join(UI, "pedal_%s.png" % p["id"]), res, transparent=True, fmt="PNG")
        # La tabla de la pedalera (fondo de la página).
        sc = reset(); common_mats(); floor()
        box("board", (0, 0, 0.012), (1.4, 0.62, 0.024), mat("carpet", (0.02, 0.019, 0.018), 0.95, grain=(300, 1, (0.01, 0.01, 0.01))), bevel=0.005)
        stage_lights((0, 0, 0.03), 1.0, key=200)
        camera((0, -0.42 * 2.2, 0.62 * 2.2), (0, 0.0, 0.02), ortho=1.32)
        render(sc, os.path.join(UI, "pedalboard.jpg"), (2656, 760))

    if want("amp"):
        # Cada amplificador sobre cada gabinete (4 × 4), sin micrófonos: la interfaz los pone donde diga la posición.
        lay["amp"] = []
        for ci, cb in enumerate(CABS):
            for ai, a in enumerate(AMPS):
                sc = reset(); common_mats(); floor()
                g, (W, H, D, hh) = build_stack(a, cb)
                tot = H + hh
                tgt = Vector((0, 0, tot * 0.5 * MM))
                stage_lights(tgt, 1.6 * max(1.0, tot / 700.0))
                light("AREA", (0.0, -1.2, tot * MM), 10, (1, 0.9, 0.8), size=1.0, target=tgt, name="fill")
                dist = 1.5 * max(1.0, tot / 700.0, W / 620.0)
                camera((0.42 * dist, -dist, tot * 0.62 * MM + 0.35), tgt, lens=40)
                sx, sz = cb["spk"][0] if len(cb["spk"]) == 1 else min(cb["spk"], key=lambda s: (-s[1], s[0]))
                c = Vector((sx * MM, -(D / 2 + 3) * MM, (H / 2 + sz) * MM))
                rr = cb["r"] * MM
                if ai == 0:
                    lay["amp"].append({"cab": cb["id"], "center": proj(sc, c), "edgeX": proj(sc, c + Vector((rr, 0, 0))), "edgeZ": proj(sc, c + Vector((0, 0, rr))),
                                       "front": proj(sc, c + Vector((0, -0.25, 0))), "radiusMm": cb["r"]})
                render(sc, os.path.join(UI, "amp_%d_%d.jpg" % (ai, ci)), (1400, 1188))

    if want("mic"):
        lay["mic"] = []
        for k in range(3):
            sc = reset(); common_mats()
            g = build_mic(k)
            stage_lights((0, 0, 0), 0.5, key=160)
            # La misma dirección de cámara que la toma del gabinete (de frente a la derecha y un poco arriba).
            camera((0.42 * 0.8, -0.8, 0.22), (0, -0.06, 0), ortho=0.24)
            lay["mic"].append({"tip": proj(sc, Vector((0, 0, 0))), "w": 400, "h": 400})
            render(sc, os.path.join(UI, "mic_%d.png" % k), (400, 400), transparent=True, fmt="PNG")

    json.dump(lay, open(lay_path, "w"), indent=1)
    print("listo:", UI)

# ───────── El escenario (fotos y film) ─────────
FPS = 24
BPM = 100.0
BEAT = 60.0 / BPM
FILM_SHOTS = [("open", 8), ("neck", 6), ("pedals", 6), ("amp", 6), ("lineup", 6), ("hero", 10), ("end", 8)]

def shot_table():
    """(nombre, primer cuadro del film, cuadros) con el mismo redondeo que scripts/film.py."""
    res, f0 = [], 0
    for name, beats in FILM_SHOTS:
        n = int(round(beats * BEAT * FPS)); res.append((name, f0, n)); f0 += n
    return res

def place_upright(g, at, lean=12.0, turn=0.0):
    """La guitarra parada (el mástil hacia arriba, el frente hacia −y), apoyada hacia atrás lean grados."""
    from mathutils import Matrix
    R = Matrix.Rotation(math.radians(turn), 4, "Z") @ Matrix.Rotation(math.radians(lean), 4, "X") @ Matrix.Rotation(math.radians(90), 4, "Z") @ Matrix.Rotation(math.radians(-90), 4, "Y")
    g.matrix_world = Matrix.Translation(Vector(at)) @ R

def stage_floor():
    bpy.ops.mesh.primitive_plane_add(size=16, location=(0, 0, 0)); f = bpy.context.object
    link(f, mat("planks", (0.035, 0.024, 0.018), 0.55, grain=(6, 0.06, (0.016, 0.011, 0.008))))
    return f

def stage_scene():
    """El escenario: el cabezal crunch sobre el 4x12, la guitarra parada delante, la pedalera en el piso."""
    sc = reset(); common_mats()
    stage_floor()
    stack, (W, H, D, hh) = build_stack(AMPS[1], CABS[2])
    stack.location = (0.12, 0.55, 0)
    g = build_guitar(TYPES[0])
    place_upright(g, (-0.30, 0.02, 0.17), lean=-14, turn=-12)       # (lean negativo: la parte de arriba hacia atrás, +y)
    # El soporte: una base en el piso, un caño que sube por detrás y la horquilla donde apoya el mástil.
    bpy.context.view_layer.update()
    T = g.matrix_world
    rest = T @ Vector((470 * MM, 0, -0.012))                       # detrás del mástil, por el traste 12
    foot = Vector((rest.x, rest.y + 0.05, 0.0))
    stand = [rod("standpole", foot, rest + Vector((0, 0.012, -0.02)), 0.006, M["black"])]
    for a in (-35, 35, 180):
        stand.append(rod("leg", foot + Vector((0, 0, 0.02)), foot + Vector((0.22 * math.sin(math.radians(a)), -0.22 * math.cos(math.radians(a)) * (1 if a != 180 else -0.6), 0.004)), 0.005, M["black"]))
    low = T @ Vector((-150 * MM, 0, -0.012))
    stand.append(rod("cradle", Vector((low.x - 0.06, low.y + 0.02, 0.03)), Vector((low.x + 0.06, low.y + 0.02, 0.03)), 0.007, M["rubber"]))
    board = box("board", (0, 0, 0.012), (0.72, 0.34, 0.024), mat("carpet", (0.02, 0.019, 0.018), 0.95, grain=(300, 1, (0.01, 0.01, 0.01))), bevel=0.005)
    bg = group([board], "pedalboard"); bg.location = (0.18, -0.38, 0); bg.rotation_euler.z = math.radians(-6)
    pedals = []
    for i, k in enumerate((0, 3, 5, 6, 11, 12)):
        pg, dims, led = build_pedal(PEDALS[k])
        link(led, mat("led%d" % i, (1.0, 0.2, 0.08), 0.25, emit=(1.0, 0.22, 0.08), strength=0.0))      # cada LED con su material (se enciende en el film)
        pg.parent = bg; pg.location = (-0.28 + i * 0.112, 0.0 if i % 2 == 0 else 0.03, 0.024)
        pedals.append((pg, led))
    world(0.02, (0.005, 0.005, 0.007))
    t = Vector((0.0, 0.1, 0.4))
    light("SPOT", (-0.9, -1.6, 2.6), 900, (1.0, 0.74, 0.45), size=0.25, target=(-0.15, 0.0, 0.35), spot=0.75, name="key")
    light("SPOT", (1.4, -1.2, 2.2), 380, (1.0, 0.62, 0.32), size=0.3, target=(0.2, -0.3, 0.0), spot=0.6, name="fill")
    light("AREA", (-1.3, 1.8, 1.0), 220, (0.4, 0.5, 1.0), size=1.0, target=(0, 0.3, 0.4), name="rim")
    light("AREA", (1.2, 1.6, 1.6), 160, (1.0, 0.45, 0.2), size=1.2, target=(0.1, 0.5, 0.7), name="back")
    return sc, g, stack, bg, pedals

def mic_on_stand(spk):
    """El micrófono dinámico frente al borde del domo, en un pie con brazo que viene de la izquierda."""
    mic = build_mic(0); mp = spk + Vector((0.06, -0.10, 0.0)); mic.location = mp
    foot = Vector((mp.x - 0.45, mp.y - 0.25, 0.0)); top = Vector((foot.x, foot.y, mp.z + 0.06))
    parts = [cyl("micbase", foot + Vector((0, 0, 0.01)), 0.12, 0.02, M["black"], verts=48, bevel=0.004), rod("micpole", foot, top, 0.009, M["nickel"]),
             rod("micboom", top, mp + Vector((0, -0.15, 0.0)), 0.007, M["nickel"]), rod("cable", mp + Vector((0, -0.19, 0)), mp + Vector((0.05, -0.32, -mp.z + 0.01)), 0.004, M["rubber"])]
    return mic, parts

def lineup_scene():
    """Las seis guitarras acostadas una al lado de la otra, vistas desde arriba."""
    sc = reset(); common_mats()
    stage_floor()
    gs = []
    for k, t in enumerate(TYPES):
        g = build_guitar(t)
        g.rotation_euler = (0, 0, math.radians(90 - 8)); g.location = (-0.95 + k * 0.38, -0.2, 0.002)
        gs.append(g)
    world(0.02)
    light("SPOT", (0.0, -1.2, 3.0), 1600, (1.0, 0.78, 0.5), size=0.3, target=(0, 0.1, 0), spot=1.2, name="key")
    light("AREA", (-2.0, 1.5, 1.0), 300, (0.45, 0.55, 1.0), size=1.5, target=(0, 0.1, 0), name="rim")
    return sc, gs

def photos():
    out = os.path.abspath(arg("--out", "photos")); os.makedirs(out, exist_ok=True)
    only = set((arg("--only", "") or "").split(",")) - {""}
    res = (2400, 1500)
    def shoot(name, sc, cam_at, look, lens, fstop=None, focus=None):
        if only and name not in only: return
        c = camera(cam_at, look, lens=lens)
        if fstop:
            c.data.dof.use_dof = True; c.data.dof.aperture_fstop = fstop
            c.data.dof.focus_distance = (Vector(focus or look) - Vector(cam_at)).length
        render(sc, os.path.join(out, name + ".jpg"), res)
        bpy.data.objects.remove(c)
    sc, g, stack, bg, pedals = stage_scene()
    bpy.context.view_layer.update()
    for pg, led in pedals[1:4]: led.data.materials[0].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 40.0
    shoot("hero", sc, (1.15, -2.55, 1.05), (-0.02, 0.05, 0.42), 42, 4.0, (-0.25, 0.08, 0.5))
    T = g.matrix_world
    pu = T @ Vector((70 * MM, 0, 0.05)); body = T @ Vector((0, 0, 0.05))
    shoot("guitar", sc, tuple(pu + Vector((0.10, -0.46, 0.10))), tuple(pu), 60, 2.4, tuple(pu))
    bc = bg.matrix_world.translation
    shoot("pedals", sc, tuple(bc + Vector((-0.42, -0.62, 0.30))), tuple(bc + Vector((0.05, 0.0, 0.03))), 50, 2.8, tuple(bc + Vector((-0.06, 0.0, 0.04))))
    spk = stack.matrix_world @ Vector((-175 * MM, -(360 / 2 + 3) * MM, (760 / 2 + 175) * MM))
    mic, _ = mic_on_stand(spk)
    shoot("amp", sc, tuple(spk + Vector((0.55, -0.85, 0.12))), tuple(spk + Vector((0.02, -0.04, 0))), 75, 3.2, tuple(mic.location))
    if not only or "lineup" in only:
        sc, gs = lineup_scene()
        shoot("lineup", sc, (0.0, -0.95, 2.35), (0.0, 0.05, 0.0), 40)
    print("fotos:", out)

def film2():
    """Siete tomas con filmkit (guion: FILM_SHOTS, 100 BPM).
      blender -b --factory-startup -P thrum.py -- --mode film2 --audio <wav> --out <dir> [--shot a,b] [--quality draft|final] [--every N]"""
    sys.path.insert(0, os.path.join(REPO, "shared", "film"))
    import filmkit as fk
    out = os.path.abspath(arg("--out", "film"))
    only = arg("--shot"); quality = arg("--quality", "final"); every = int(arg("--every", "1"))
    env = fk.envelope(arg("--audio")) if arg("--audio") else []
    def E(T): i = int(T * FPS); return env[i] if 0 <= i < len(env) else 0.0
    def V(*a): return Vector(a[0] if len(a) == 1 else a)
    for (name, f0, frames) in shot_table():
        if only and name not in only.split(","): continue
        if name == "lineup": sc, gs = lineup_scene(); stack = bg = g = None; pedals = []
        else: sc, g, stack, bg, pedals = stage_scene()
        fk.setup(sc, quality)
        cam = fk.camera(sc)
        bpy.context.view_layer.update()
        if g is not None:
            T = g.matrix_world
            pu = T @ V((70 * MM, 0, 0.05)); bridge = T @ V((0, 0, 0.05)); nut = T @ V((648 * MM, 0, 0.055)); head = T @ V((760 * MM, 0, 0.05))
            bc = bg.matrix_world.translation
            spk = stack.matrix_world @ V((-175 * MM, -(360 / 2 + 3) * MM, (760 / 2 + 175) * MM))
        if name == "open":
            path = fk.Path([(0, pu + V(0.10, -0.30, -0.10), pu, 85, 2.0), (1, pu + V(0.02, -0.26, 0.06), pu + V(0, 0, 0.05), 85, 2.0)])
        elif name == "neck":
            mid = (nut + pu) * 0.5
            path = fk.Path([(0, mid + V(0.20, -0.30, -0.18), mid + V(0, 0, -0.06), 50, 2.2), (1, nut + V(0.16, -0.30, -0.06), nut + V(0, 0, 0.02), 55, 2.2)])
        elif name == "pedals":
            path = fk.Path([(0, bc + V(-0.55, -0.40, 0.12), bc + V(-0.18, 0, 0.03), 45, 2.8), (1, bc + V(0.10, -0.55, 0.16), bc + V(0.12, 0, 0.03), 45, 2.8)])
            pedal_leds = [led for _, led in pedals]
        elif name == "amp":
            mic, _ = mic_on_stand(spk)
            path = fk.Path([(0, spk + V(0.70, -1.10, 0.20), spk, 60, 3.2), (1, spk + V(0.32, -0.55, 0.06), spk + V(0.03, -0.05, 0), 70, 3.2)])
            cones = [o for o in sc.objects if o.name.startswith("cone")]
            rest = [(o, o.location.copy()) for o in cones]
        elif name == "lineup":
            path = fk.Path([(0, V(-0.6, -1.0, 1.6), V(-0.5, 0.0, 0.0), 40), (1, V(0.6, -1.0, 1.7), V(0.5, 0.0, 0.0), 40)])
        elif name == "hero":
            path = fk.Path([(0, V(-0.9, -2.3, 0.55), V(-0.05, 0.05, 0.45), 40, 5.6), (0.5, V(0.2, -2.6, 0.9), V(-0.05, 0.05, 0.42), 42, 5.6),
                            (1, V(1.25, -2.35, 1.2), V(-0.02, 0.05, 0.4), 44, 5.6)])
        else:  # end
            path = fk.Path([(0, V(0.6, -1.9, 0.9), V(-0.05, 0.05, 0.42), 45, 5.6), (1, V(0.9, -3.0, 1.6), V(0.0, 0.1, 0.45), 45, 5.6)], easing="out")
        focus_pt = {"open": pu, "neck": None, "pedals": None, "amp": spk + V(0.06, -0.10, 0.0) if g is not None else None, "lineup": None}.get(name)

        def pose(f, t, u, name=name, path=path, f0=f0):
            T_ = (f0 + f - 1) / FPS
            focus = focus_pt
            if name == "neck": focus = (pu.lerp(nut, fk.rack(u, 0.2, 0.8, 0.3, 1.0)))
            path.apply(cam, u, focus=focus, shake=0.0006 if name in ("open", "neck", "pedals", "amp") else 0.0, t=T_)
            moved = []
            if name == "amp":
                # Los conos empujan aire con el sonido.
                for o, l0 in rest: o.location = l0 + V(0, -0.004 * E(T_), 0); moved.append(o)
            return moved

        def keys_(f, name=name, f0=f0, frames=frames):
            T_ = (f0 + f - 1) / FPS; u = (f - 1) / max(1, frames - 1)
            if name == "open":
                for o in sc.objects:
                    if o.type == "LIGHT" and o.name in ("key", "fill", "rim", "back"):
                        if "e0" not in o: o["e0"] = o.data.energy
                        o.data.energy = o["e0"] * fk.rack(u, 0.0, 0.7, 0.05, 1.0); o.data.keyframe_insert("energy")
            if name == "pedals":
                # Los LED se encienden de a uno, a tiempo.
                for i, led in enumerate(pedal_leds):
                    on = T_ - f0 / FPS > (i + 1) * BEAT * 0.8
                    inp = led.data.materials[0].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
                    inp.default_value = 40.0 if on else 0.0; inp.keyframe_insert("default_value")
        if every > 1: sc.frame_step = every
        fk.render_shot(sc, name, frames, pose, out, step=every, extra_keys=keys_)

if MODE == "ui":
    ui()
elif MODE == "photos":
    photos()
elif MODE == "film2":
    film2()
