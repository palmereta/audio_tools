"""DeviceNTI V (slug devicenti): the unit in Blender, built from the front-panel table (tools/blender/panel.json, written by
`devicentirender --panel-json`). Original design: an orange case, a cream plate, coloured zones, black knobs, white
keys, the DEVICENTI V wordmark and the Audio Tools script; no other marks.

Modes (blender -b -P devicenti.py -- --mode ui --out out [--samples 160] [--scale 2]):
  --mode ui      the plugin view in 3/4 perspective: bg.png (keys up, LEDs off, screens dark, knobs without pointer),
                 keys_down.png, keys_lit.png, leds_green.png, leds_red.png and ui.json (every control projected
                 through the camera: plane homographies for knob tops, keys and screens, crop rectangles for sprites).
                 tools/pack_ui.py turns these into assets/ui.
  --mode hero    a still in a steeper 3/4 view with knob pointers, lit keys and LEDs (README, web).
  --mode photos  the web photos (hero.jpg 16:9, top.jpg, close.jpg, keys.jpg, filter.jpg): the unit in metres, knob
                 pointers, lit keys and LEDs, shallow depth of field. --shot a,b renders only some; --quality draft|preview|final.
  --mode film    the shots of the web film (scripts/film.py and tools/film.json): open, ops, steps, filter, hero, end at 84 BPM,
                 the playhead and the lit steps follow the trip hop demo's pattern (tools/demo_projects/01-trip-hop.json),
                 knobs sweep, the lights come up. Run only on pc50-wsl, under the GPU lock:
                   flock ~/ai/prerendered/.gpu.lock blender -b --factory-startup -P devicenti.py -- --mode film --out <dir> [--shot a,b] [--quality draft]
  --quick        low resolution and few samples (layout checks on the Mac).

Units: panel millimetres, origin at the top-left corner of the case, x right, y towards the front. Blender units are
centimetres (x / 10, −y / 10, z / 10). The top of the plate is at z = CASE_Z + PLATE.
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
HERE = os.path.dirname(os.path.abspath(__file__))
MODE = arg("--mode", "ui")
OUT = os.path.abspath(arg("--out", "out"))
QUICK = "--quick" in argv
SAMPLES = int(arg("--samples", "24" if QUICK else "160"))
SCALE = float(arg("--scale", "0.5" if QUICK else "2"))
FONTS = arg("--fonts", os.path.join(HERE, "..", "..", "assets", "fonts"))
PANEL = json.load(open(arg("--panel", os.path.join(HERE, "panel.json"))))
CASE_W, CASE_H, CASE_Z = PANEL["case"]
CTRL = {c["id"]: c for c in PANEL["controls"]}
PLATE = 0.8                    # the plate stands this far above the case top
TOP = CASE_Z + PLATE           # plate surface
# Window of the plugin view (logical pixels at scale 1): the editor below the top bar.
VIEW_W, VIEW_H = 1120, 800

def P(x, y, z): return Vector((x / 10.0, -y / 10.0, z / 10.0))
def font(name): return os.path.join(FONTS, name)
F_LABEL, F_MED, F_WORD, F_SCRIPT = font("ArchivoCondensed-Bold.ttf"), font("ArchivoCondensed-Medium.ttf"), font("ArchivoExpanded-ExtraBold.ttf"), font("Yellowtail-Regular.ttf")

# ───────────── colours (sRGB) ─────────────
def srgb(h):
    h = h.lstrip("#"); c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(((v + 0.055) / 1.055) ** 2.4 if v > 0.04045 else v / 12.92 for v in c) + (1.0,)
COL = dict(case="#e2701f", plate="#efe2c4", blue="#3aa8d8", yellow="#efaa2c", orange="#e9822b", olive="#a8a161",
           ink="#2b2016", inkSoft="#6b4a2c", funcInk="#8c2f08", funcOnColour="#4a1e06", line="#3a2a1c",
           key="#ecebe4", bezel="#1d1b19", knob="#121212", led="#1c3a1c", ledOn="#2fe043", ledRed="#ff2a1a",
           glass="#0b0d0e", floor="#140d09", word="#3a1f0c")

# ───────────── scene ─────────────
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for dev in ("CUDA", "METAL", "NONE"):
            try:
                prefs.compute_device_type = dev; prefs.get_devices()
                if dev == "NONE" or any(d.type == dev for d in prefs.devices): break
            except Exception: continue
        for d in prefs.devices: d.use = d.type != "CPU" or prefs.compute_device_type == "NONE"
        sc.cycles.device = "GPU" if prefs.compute_device_type != "NONE" else "CPU"
    except Exception as e: print("device:", e)
    sc.cycles.samples = SAMPLES
    sc.cycles.use_denoising = True
    try: sc.cycles.denoising_use_gpu = True
    except Exception: pass
    sc.cycles.seed = 7
    sc.render.resolution_percentage = 100
    for vt in (arg("--view", "Standard"), "AgX"):
        try: sc.view_settings.view_transform = vt; break
        except Exception: continue
    sc.view_settings.look = "None"; sc.view_settings.exposure = float(arg("--exposure", "-0.35"))
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs[0].default_value = srgb("#1a120d"); bg.inputs[1].default_value = 0.35
    return sc

MAT = {}
def mat(name, col, rough=0.5, coat=0.0, metal=0.0, emit=None, strength=0.0, spec=0.5):
    if name in MAT: return MAT[name]
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = srgb(col) if isinstance(col, str) else col
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    for k, v in (("Coat Weight", coat), ("Specular IOR Level", spec)):
        if k in b.inputs: b.inputs[k].default_value = v
    if emit is not None:
        b.inputs["Emission Color"].default_value = srgb(emit); b.inputs["Emission Strength"].default_value = strength
    MAT[name] = m; return m

def materials():
    mat("case", COL["case"], 0.42, coat=0.25); mat("plate", COL["plate"], 0.62)
    for z in ("blue", "yellow", "orange", "olive"): mat(z, COL[z], 0.58)
    mat("ink", COL["ink"], 0.7); mat("func", COL["funcInk"], 0.7); mat("funcC", COL["funcOnColour"], 0.7); mat("line", COL["line"], 0.7)
    mat("word", COL["word"], 0.5); mat("key", COL["key"], 0.35, coat=0.3); mat("bezel", COL["bezel"], 0.5)
    mat("knob", COL["knob"], 0.32, coat=0.5); mat("knobSkirt", COL["knob"], 0.45)
    mat("led", COL["led"], 0.2, coat=0.6); mat("glass", COL["glass"], 0.08, coat=1.0); mat("floor", COL["floor"], 0.55)
    mat("vent", "#120b07", 0.9); mat("rubber", "#0d0d0d", 0.9)
    mat("keyLit", "#ffc480", 0.35, coat=0.3, emit="#ff8a2a", strength=16.0)
    mat("ledOn", COL["ledOn"], 0.2, coat=0.6, emit=COL["ledOn"], strength=2.4)
    mat("ledRed", COL["ledRed"], 0.2, coat=0.6, emit=COL["ledRed"], strength=2.6)
    mat("pointer", "#f4f1e8", 0.4)

def link(o): bpy.context.scene.collection.objects.link(o); return o
def obj_from_bm(name, bm, m, smooth=False):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    if smooth:
        for p in me.polygons: p.use_smooth = True
    o = bpy.data.objects.new(name, me); link(o)
    o.data.materials.append(m)
    return o

def rrect_pts(x0, y0, x1, y1, r, seg=8):
    r = max(0.0, min(r, (x1 - x0) / 2 - 1e-3, (y1 - y0) / 2 - 1e-3))
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for i in range(seg + 1):
            a = math.radians(a0 + 90.0 * i / seg)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts

def prism(name, pts, z0, z1, m, bevel=0.0, seg=3, smooth=False):
    """Extrude a 2D outline (panel mm) from z0 to z1 (mm); optional bevel on the top edges."""
    bm = bmesh.new()
    vb = [bm.verts.new(P(x, y, z0)) for x, y in pts]
    vt = [bm.verts.new(P(x, y, z1)) for x, y in pts]
    bm.faces.new(list(reversed(vb))); top = bm.faces.new(vt)
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n; bm.faces.new([vb[i], vb[j], vt[j], vt[i]])
    bm.normal_update()
    if bevel > 0:
        edges = [e for e in top.edges]
        bmesh.ops.bevel(bm, geom=edges, offset=bevel / 10.0, segments=seg, profile=0.5, affect="EDGES")
    return obj_from_bm(name, bm, m, smooth)

def rbox(name, x0, y0, x1, y1, z0, z1, r, m, bevel=0.0, seg=3):
    return prism(name, rrect_pts(x0, y0, x1, y1, r), z0, z1, m, bevel, seg)

def lathe(name, prof, cx, cy, z0, m, segs=64, smooth=True):
    """Surface of revolution: prof = [(radius, height)], from the bottom centre up to the top centre."""
    bm = bmesh.new(); rings = []
    for r, h in prof:
        if r < 1e-6: rings.append([bm.verts.new(P(cx, cy, z0 + h))]); continue
        rings.append([bm.verts.new(P(cx + r * math.cos(2 * math.pi * k / segs), cy + r * math.sin(2 * math.pi * k / segs), z0 + h)) for k in range(segs)])
    for a, b in zip(rings[:-1], rings[1:]):
        if len(a) == 1 and len(b) == 1: continue
        if len(a) == 1:
            for k in range(segs): bm.faces.new([a[0], b[k], b[(k + 1) % segs]])
        elif len(b) == 1:
            for k in range(segs): bm.faces.new([a[k], b[0], a[(k + 1) % segs]])
        else:
            for k in range(segs): bm.faces.new([a[k], a[(k + 1) % segs], b[(k + 1) % segs], b[k]])
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, m, smooth)

def text(s, x, y, size, m, align="CENTER", z=TOP + 0.09, track=0.0, fnt=F_LABEL, valign="CENTER"):
    """Flat silkscreen text; size = cap height in mm (approximately)."""
    cu = bpy.data.curves.new("t", "FONT"); cu.body = s
    cu.size = size * 0.1 * 1.38; cu.align_x = align; cu.align_y = valign
    cu.space_character = 1.0 + track
    if os.path.exists(fnt): cu.font = bpy.data.fonts.load(fnt, check_existing=True)
    o = bpy.data.objects.new("T_" + s[:12], cu); link(o)
    o.location = P(x, y, z); o.data.materials.append(m)
    return o

# ───────────── the unit ─────────────
ZONES = [  # x0, y0, x1, y1, material (panel mm)
    (93.4, 16.8, 216.5, 80.6, "blue"),
    (14.2, 123.6, 150.1, 210.6, "yellow"), (150.1, 123.6, 286.2, 210.6, "yellow"),
    (14.2, 210.6, 150.1, 298.4, "orange"), (150.1, 210.6, 286.2, 298.4, "orange"),
    (13.9, 306.0, 150.0, 352.0, "plate"), (150.0, 306.0, 217.8, 352.0, "olive"), (217.8, 306.0, 286.1, 352.0, "plate"),
]
PLATE_RECT = (9.5, 9.8, 290.5, 399.0)

def build_case():
    rbox("Case", 0, 0, CASE_W, CASE_H, 2.0, CASE_Z, 10.0, MAT["case"], bevel=2.6, seg=4)
    rbox("Feet", 6, 6, CASE_W - 6, CASE_H - 6, 0.0, 2.2, 6.0, MAT["rubber"])
    x0, y0, x1, y1 = PLATE_RECT
    rbox("PlateGap", x0 - 0.7, y0 - 0.7, x1 + 0.7, y1 + 0.7, CASE_Z - 0.6, CASE_Z + 0.25, 3.2, MAT["line"])
    rbox("Plate", x0, y0, x1, y1, CASE_Z - 0.5, TOP, 2.6, MAT["plate"], bevel=0.35, seg=2)
    # Vents on the left side: slots set into the side face.
    for i in range(24):
        y = 70.0 + i * 11.5
        b = prism("Vent%d" % i, [(-0.02, y), (-0.02, y + 4.2), (0.6, y + 4.2), (0.6, y)], 14.0, 38.0, MAT["vent"])
    # Wordmark and script on the orange rim.
    text("DEVICENTI V", 14.5, 412.6, 11.0, MAT["word"], "LEFT", z=CASE_Z + 0.05, track=0.16, fnt=F_WORD)
    text("Audio Tools", 284.0, 411.8, 9.6, MAT["word"], "RIGHT", z=CASE_Z + 0.05, track=0.02, fnt=F_SCRIPT)

def build_zones():
    for i, (x0, y0, x1, y1, m) in enumerate(ZONES):
        rbox("ZoneLine%d" % i, x0 - 0.45, y0 - 0.45, x1 + 0.45, y1 + 0.45, TOP - 0.2, TOP + 0.02, 1.6, MAT["line"])
        rbox("Zone%d" % i, x0 + 0.45, y0 + 0.45, x1 - 0.45, y1 - 0.45, TOP - 0.2, TOP + 0.03, 1.2, MAT[m])
    # Titles and the line above each operator's screen.
    for k, n in enumerate("ABCD"):
        x0, y0, x1, y1, _ = ZONES[1 + k]
        text("OPERATOR " + n, (x0 + x1) / 2, y0 + 8.8, 5.2, MAT["ink"], track=0.06)
        prism("OpLine%d" % k, rrect_pts(x0 + 6.0, y0 + 63.0, x1 - 6.0, y0 + 63.45, 0.1, 1), TOP, TOP + 0.06, MAT["line"])
    x0, y0, x1, y1, _ = ZONES[5]
    text("FILTER", (x0 + x1) / 2, y0 + 3.7, 3.6, MAT["ink"], track=0.1)

def knob(c):
    R = c["size"] / 2.0; H = 0.46 * c["size"]
    prof = [(0, 0), (R, 0), (R, 0.9), (R * 0.97, 1.5), (R * 0.88, 2.1), (R * 0.87, H - 1.0), (R * 0.85, H - 0.45), (R * 0.81, H - 0.1), (R * 0.6, H), (0, H)]
    o = lathe("Knob_" + c["id"], prof, c["x"], c["y"], TOP, MAT["knob"], segs=72)
    # Skirt ring (matte), just above the plate.
    lathe("Skirt_" + c["id"], [(R * 0.82, 0), (R + 0.6, 0), (R + 0.6, 0.6), (R * 0.82, 0.6)], c["x"], c["y"], TOP, MAT["knobSkirt"], segs=72)
    return o, TOP + H

def key_dims(c):
    w = c["size"]; h = 14.9 if c["id"].startswith("step") else 15.8
    return w, h

KEYS = {}
def key(c, pressed=False, lit=False):
    w, h = key_dims(c); x, y = c["x"], c["y"]
    rbox("Bezel_" + c["id"], x - w / 2 - 1.1, y - h / 2 - 1.1, x + w / 2 + 1.1, y + h / 2 + 1.1, TOP - 0.1, TOP + 1.4, 2.0, MAT["bezel"], bevel=0.3, seg=2)
    dz = -1.7 if pressed else 0.0
    cap = rbox("Key_" + c["id"], x - w / 2, y - h / 2, x + w / 2, y + h / 2, TOP + 0.4 + dz, TOP + 4.0 + dz, 1.6, MAT["keyLit" if lit else "key"], bevel=0.9, seg=4)
    KEYS[c["id"]] = cap
    return cap, TOP + 4.0

def led(c, state=None):
    r = c["size"] / 2.0
    m = MAT["ledOn"] if state == "green" else MAT["ledRed"] if state == "red" else MAT["led"]
    lathe("Led_" + c["id"], [(0, 0), (r + 0.5, 0), (r + 0.5, 0.5), (r, 0.6), (r * 0.9, 1.1), (r * 0.5, 1.45), (0, 1.55)], c["x"], c["y"], TOP, m, segs=40)
    lathe("LedRing_" + c["id"], [(r, 0), (r + 0.9, 0), (r + 0.9, 0.35), (r, 0.35)], c["x"], c["y"], TOP, MAT["bezel"], segs=40)

DISP_H = {"display": 15.0}
def disp_h(c): return DISP_H.get(c["id"], 11.0)
def screen(c):
    w, h = c["size"], disp_h(c); x, y = c["x"], c["y"]
    rbox("ScreenBezel_" + c["id"], x - w / 2 - 1.4, y - h / 2 - 1.4, x + w / 2 + 1.4, y + h / 2 + 1.4, TOP - 0.1, TOP + 0.7, 1.4, MAT["bezel"], bevel=0.25, seg=2)
    rbox("Screen_" + c["id"], x - w / 2, y - h / 2, x + w / 2, y + h / 2, TOP + 0.3, TOP + 0.6, 0.6, MAT["glass"])
    return TOP + 0.6

def silkscreen(c, onColour):
    """Main label above, FUNC label below (smaller, rust)."""
    k = c["kind"]
    if k == "knob":
        R = c["size"] / 2.0
        size = 4.3 if c["size"] < 22 else 4.6
        # Names under the knobs (towards the viewer: a knob never hides its own name in the 3/4 view), the FUNC
        # name under it, smaller and in rust.
        text(c["label"], c["x"], c["y"] + R + 4.2, size, MAT["ink"], track=0.03)
        # FUNC names: bold and as large as the spacing allows (they must read at 100 % on a 1x screen).
        if c["func"]: text(c["func"], c["x"], c["y"] + R + 9.25, 3.8, MAT["funcC" if onColour else "func"], track=0.03, fnt=F_LABEL)
    elif k == "key":
        w, h = key_dims(c)
        if c["id"].startswith("step"):
            text(c["label"], c["x"], c["y"] - h / 2 - 14.6, 3.4, MAT["ink"])
            text(c["func"], c["x"], c["y"] + h / 2 + 3.7, 3.2, MAT["func"], track=0.02, fnt=F_LABEL)
        else:
            text(c["label"], c["x"], c["y"] - h / 2 - 4.4, 3.8, MAT["ink"], track=0.04)
            if c["func"]: text(c["func"], c["x"], c["y"] + h / 2 + 3.7, 3.4, MAT["func"], track=0.03, fnt=F_LABEL)

def on_colour(c):
    return (123.6 < c["y"] < 298.4) or (c["id"] in ("drive", "age"))

def type_scale(c):
    # The TYPE knob's positions, a column to its right (top = fully left).
    names = ["LP24", "LP12", "BP", "HP", "NOTCH", "PEAK", "OFF"]
    for i, n in enumerate(names):
        text(n, c["x"] + c["size"] / 2 + 4.0, c["y"] - 11.7 + 3.9 * i, 2.6, MAT["ink"], "LEFT", track=0.03, fnt=F_MED)

def build_unit(pressed=(), lit=(), leds=None):
    """pressed / lit: key ids; leds: {id: 'green'|'red'}."""
    materials(); mat("inkSoft", COL["inkSoft"], 0.7)
    build_case(); build_zones()
    tops = {}
    for c in PANEL["controls"]:
        k = c["kind"]
        if k == "knob":
            o, z = knob(c); tops[c["id"]] = z; silkscreen(c, on_colour(c))
            if c["id"] == "type": type_scale(c)
        elif k == "key":
            o, z = key(c, c["id"] in pressed, c["id"] in lit); tops[c["id"]] = z; silkscreen(c, False)
        elif k == "led":
            led(c, (leds or {}).get(c["id"])); tops[c["id"]] = TOP + 1.5
        elif k == "display":
            tops[c["id"]] = screen(c)
    return tops

def floor():
    bm = bmesh.new()
    for x, y in ((-2000, -2000), (2300, -2000), (2300, 2400), (-2000, 2400)): bm.verts.new(P(x, y, 0))
    bm.faces.new(bm.verts[:])
    obj_from_bm("Floor", bm, MAT["floor"])

def lights():
    tgt = P(CASE_W / 2, CASE_H / 2, CASE_Z)
    def area(name, loc_mm, size_mm, energy, col="#fff3e6"):
        l = bpy.data.lights.new(name, "AREA"); l.size = size_mm / 10.0; l.energy = energy; l.color = srgb(col)[:3]
        o = bpy.data.objects.new(name, l); link(o); o.location = P(*loc_mm)
        o.rotation_euler = (tgt - o.location).to_track_quat("-Z", "Y").to_euler()
    E = float(arg("--light", "1.0"))
    area("Key", (-220, 120, 760), 600, 160000 * E)
    area("Fill", (560, 720, 420), 700, 50000 * E, "#e8eeff")
    area("Rim", (160, -420, 520), 600, 50000 * E)

def camera(az_deg, el_deg, lens, fill):
    sc = bpy.context.scene
    W, H = int(VIEW_W * SCALE), int(VIEW_H * SCALE)
    sc.render.resolution_x, sc.render.resolution_y = W, H
    cam = bpy.data.cameras.new("Cam"); cam.lens = lens; cam.sensor_fit = "HORIZONTAL"; cam.sensor_width = 36.0
    co = bpy.data.objects.new("Cam", cam); link(co); sc.camera = co
    tgt = P(CASE_W / 2, CASE_H / 2 + 8, CASE_Z / 2)
    az, el = math.radians(az_deg), math.radians(el_deg)
    d = Vector((-math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
    from bpy_extras.object_utils import world_to_camera_view
    corners = [P(x, y, z) for x in (0, CASE_W) for y in (0, CASE_H) for z in (0, CASE_Z)]
    dist = 120.0
    for _ in range(40):
        co.location = tgt + d * dist
        # Looking down at `el` but turned only a little towards the unit: the front edge stays nearly level and the
        # lens shift centres the unit (an off-axis view, like a photo taken from the left with a shift lens).
        co.rotation_euler = (math.radians(90.0 - el_deg), 0.0, -math.radians(az_deg * YAW))
        cam.shift_x = cam.shift_y = 0.0
        bpy.context.view_layer.update()
        pts = [world_to_camera_view(sc, co, c) for c in corners]
        xs = [p.x for p in pts]; ys = [p.y for p in pts]
        ext = max(max(xs) - min(xs), max(ys) - min(ys))      # each normalised to its own side
        dist *= ext / fill
        if "--debug" in argv: print("cam", round(dist, 2), round(ext, 3), [round(v, 2) for v in co.location], flush=True)
    # Centre the projected box (shift is in units of the larger sensor side).
    bpy.context.view_layer.update()
    pts = [world_to_camera_view(sc, co, c) for c in corners]
    xs = [p.x for p in pts]; ys = [p.y for p in pts]
    cam.shift_x = ((min(xs) + max(xs)) / 2 - 0.5)
    cam.shift_y = ((min(ys) + max(ys)) / 2 - 0.5) * H / W
    bpy.context.view_layer.update()
    return co

def render(path):
    sc = bpy.context.scene
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGB"; sc.render.image_settings.color_depth = "8"
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("wrote", path, flush=True)

# ───────────── projection data ─────────────
def homography(src, dst):
    import numpy as np
    A = []
    for (u, v), (x, y) in zip(src, dst):
        A.append([u, v, 1, 0, 0, 0, -x * u, -x * v, -x])
        A.append([0, 0, 0, u, v, 1, -y * u, -y * v, -y])
    _, _, vt = np.linalg.svd(np.array(A, dtype=float))
    h = vt[-1].reshape(3, 3); h /= h[2, 2]
    return [round(float(t), 9) for t in h.flatten()]

def project_plane(cx, cy, z, half):
    """Homography from local plane coordinates (mm, origin at (cx, cy), u right, v towards the front) at height z
    to image pixels (origin top-left)."""
    from bpy_extras.object_utils import world_to_camera_view
    sc = bpy.context.scene; cam = sc.camera
    W, H = sc.render.resolution_x, sc.render.resolution_y
    src, dst = [], []
    for u in (-half, 0.0, half):
        for v in (-half, 0.0, half):
            p = world_to_camera_view(sc, cam, P(cx + u, cy + v, z))
            src.append((u, v)); dst.append((p.x * W, (1.0 - p.y) * H))
    return homography(src, dst)

def bbox_px(points3d, margin):
    from bpy_extras.object_utils import world_to_camera_view
    sc = bpy.context.scene; cam = sc.camera
    W, H = sc.render.resolution_x, sc.render.resolution_y
    xs, ys = [], []
    for p3 in points3d:
        p = world_to_camera_view(sc, cam, p3); xs.append(p.x * W); ys.append((1.0 - p.y) * H)
    x0 = max(0, int(math.floor(min(xs) - margin))); y0 = max(0, int(math.floor(min(ys) - margin)))
    x1 = min(W, int(math.ceil(max(xs) + margin))); y1 = min(H, int(math.ceil(max(ys) + margin)))
    return [x0, y0, x1 - x0, y1 - y0]

def ui_json(tops):
    sc = bpy.context.scene
    out = {"width": sc.render.resolution_x, "height": sc.render.resolution_y, "scale": SCALE, "view": [VIEW_W, VIEW_H], "controls": []}
    for c in PANEL["controls"]:
        e = {"id": c["id"], "kind": c["kind"], "x": c["x"], "y": c["y"], "size": c["size"]}
        k = c["kind"]
        if k == "knob":
            R = c["size"] / 2.0
            e["top"] = project_plane(c["x"], c["y"], tops[c["id"]], R)
            e["base"] = project_plane(c["x"], c["y"], TOP, R)
            e["r"] = R * 0.75                  # radius of the cap top (where the pointer is drawn)
        elif k == "key":
            w, h = key_dims(c)
            e["top"] = project_plane(c["x"], c["y"], tops[c["id"]], max(w, h) / 2)
            e["w"], e["h"] = w, h
            pts = [P(c["x"] + sx * (w / 2 + 1.6), c["y"] + sy * (h / 2 + 1.6), z) for sx in (-1, 1) for sy in (-1, 1) for z in (TOP - 0.2, TOP + 4.2)]
            e["crop"] = bbox_px(pts, 3.0 * SCALE)
        elif k == "led":
            r = c["size"] / 2.0 + 2.6
            pts = [P(c["x"] + sx * r, c["y"] + sy * r, z) for sx in (-1, 1) for sy in (-1, 1) for z in (TOP, TOP + 1.6)]
            e["crop"] = bbox_px(pts, 2.0 * SCALE)
            e["top"] = project_plane(c["x"], c["y"], TOP + 1.5, 2.0)
        elif k == "display":
            w, h = c["size"], disp_h(c)
            e["top"] = project_plane(c["x"], c["y"], tops[c["id"]] + 0.01, w / 2)
            e["w"], e["h"] = w, h
        out["controls"].append(e)
    json.dump(out, open(os.path.join(OUT, "ui.json"), "w"), indent=1)
    print("wrote ui.json", flush=True)

# ───────────── modes ─────────────
AZ, EL, LENS, FILL, YAW = 28.0, 50.0, 70.0, 0.94, 0.3
KEY_IDS = [c["id"] for c in PANEL["controls"] if c["kind"] == "key"]
LED_IDS = [c["id"] for c in PANEL["controls"] if c["kind"] == "led"]

def scene_for(pressed=(), lit=(), leds=None, az=AZ, el=EL, lens=LENS, fill=FILL):
    MAT.clear(); KEYS.clear(); reset()
    tops = build_unit(pressed, lit, leds); floor(); lights(); camera(az, el, lens, fill)
    return tops

def mode_ui():
    os.makedirs(OUT, exist_ok=True)
    only = arg("--only", "bg,keys_down,keys_lit,leds_green,leds_red").split(",")
    if "bg" in only:
        tops = scene_for(); ui_json(tops); render(os.path.join(OUT, "bg.png"))
    # Keys in two passes (every other key), so a key's crop never contains a pressed or lit neighbour.
    for half, tag in ((0, "a"), (1, "b")):
        ids = KEY_IDS[half::2]
        if "keys_down" in only: scene_for(pressed=ids); render(os.path.join(OUT, "keys_down_%s.png" % tag))
        if "keys_lit" in only: scene_for(lit=ids); render(os.path.join(OUT, "keys_lit_%s.png" % tag))
    if "leds_green" in only: scene_for(leds={i: "green" for i in LED_IDS}); render(os.path.join(OUT, "leds_green.png"))
    if "leds_red" in only: scene_for(leds={i: "red" for i in LED_IDS}); render(os.path.join(OUT, "leds_red.png"))

def mode_hero():
    os.makedirs(OUT, exist_ok=True)
    tops = scene_for(lit=["track2", "play"], leds={"led1": "green", "led5": "green", "led9": "green", "led11": "red", "led13": "green"}, az=24.0, el=38.0, lens=70.0, fill=0.8)
    # Pointers on the knobs (the UI draws its own; the still needs them).
    for c in PANEL["controls"]:
        if c["kind"] != "knob": continue
        R = c["size"] / 2.0; a = math.radians(-135 + 270 * (0.35 + 0.3 * math.sin(len(c["id"]) * 1.7)))
        x0, y0 = c["x"] + 0.25 * R * math.sin(a), c["y"] - 0.25 * R * math.cos(a)
        x1, y1 = c["x"] + 0.72 * R * math.sin(a), c["y"] - 0.72 * R * math.cos(a)
        nx, ny = math.cos(a) * 0.55, math.sin(a) * 0.55
        prism("Ptr_" + c["id"], [(x0 - nx, y0 - ny), (x1 - nx, y1 - ny), (x1 + nx, y1 + ny), (x0 + nx, y0 + ny)], tops[c["id"]] - 0.02, tops[c["id"]] + 0.06, MAT["pointer"])
    render(os.path.join(OUT, "hero.png"))

if MODE == "ui": mode_ui()
elif MODE == "hero": mode_hero()

# ───────────── web photos and film ─────────────
FILM_BPM, FPS = 84.0, 24
BEAT = 60.0 / FILM_BPM
STEP_S = BEAT / 4.0                                    # a sixteenth
FILM_SHOTS = [("open", 8), ("ops", 8), ("steps", 8), ("filter", 8), ("hero", 12), ("end", 8)]
STEP_IDS = ["step%d" % i for i in range(1, 17)]

def new_objects(fn):
    before = set(bpy.data.objects.keys()); r = fn()
    return r, [bpy.data.objects[n] for n in bpy.data.objects.keys() if n not in before]

def make_pointers(tops, u_of=None):
    """A thin pointer per knob, its origin at the knob centre so rotation_euler.z turns it. u in 0..1 is the knob position."""
    mat("pointer", "#f4f1e8", 0.4)
    out = {}
    for c in PANEL["controls"]:
        if c["kind"] != "knob": continue
        R = c["size"] / 2.0
        r0, r1, w = 0.25 * R, 0.72 * R, 0.045 * c["size"] * 0.5 + 0.18
        bm = bmesh.new()
        v = [bm.verts.new((sx * w / 10.0, r / 10.0, z / 10.0)) for z in (0.0, 0.3) for (sx, r) in ((-1, r0), (1, r0), (1, r1), (-1, r1))]
        bm.faces.new(v[0:4][::-1]); bm.faces.new(v[4:8])
        for i in range(4): bm.faces.new([v[i], v[(i + 1) % 4], v[4 + (i + 1) % 4], v[4 + i]])
        bm.normal_update()
        o = obj_from_bm("Ptr_" + c["id"], bm, MAT["pointer"])
        o.location = P(c["x"], c["y"], tops[c["id"]] + 0.02)
        u = (u_of or {}).get(c["id"], 0.35 + 0.3 * math.sin(len(c["id"]) * 1.7))
        o.rotation_euler = (0, 0, -math.radians(-135 + 270 * u))
        out[c["id"]] = o
    return out

def lit_parts():
    """Lit cap for every key (hidden), green and red lamps for every LED (hidden); the caps and lamps that build_unit made stay as the off state."""
    keys = {}
    for c in PANEL["controls"]:
        if c["kind"] != "key": continue
        w, h = key_dims(c); x, y = c["x"], c["y"]
        o = rbox("KeyLit_" + c["id"], x - w / 2, y - h / 2, x + w / 2, y + h / 2, TOP + 0.4, TOP + 4.0, 1.6, mat("keyLitSoft", "#ffc480", 0.35, coat=0.3, emit="#ff8a2a", strength=3.2), bevel=0.9, seg=4)
        o.hide_render = True; keys[c["id"]] = (KEYS[c["id"]], o)
    leds = {}
    for c in PANEL["controls"]:
        if c["kind"] != "led": continue
        r = c["size"] / 2.0
        prof = [(0, 0), (r + 0.5, 0), (r + 0.5, 0.5), (r, 0.6), (r * 0.9, 1.1), (r * 0.5, 1.45), (0, 1.55)]
        off = bpy.data.objects.get("Led_" + c["id"])
        g = lathe("LedG_" + c["id"], prof, c["x"], c["y"], TOP, MAT["ledOn"], segs=40); g.hide_render = True
        rd = lathe("LedR_" + c["id"], prof, c["x"], c["y"], TOP, MAT["ledRed"], segs=40); rd.hide_render = True
        leds[c["id"]] = (off, g, rd)
    return keys, leds

def to_metres():
    """Everything built in centimetre units becomes 1 BU = 1 m (scale 0.01 about the origin; lights lose 1e4 of power), so the
    depth of field of the camera behaves as it would for a 30 cm instrument."""
    for o in bpy.context.scene.objects:
        if o.type in ("MESH", "CURVE", "FONT", "LIGHT"):
            o.location = o.location * 0.01
            o.scale = (o.scale[0] * 0.01, o.scale[1] * 0.01, o.scale[2] * 0.01)
            if o.type == "LIGHT": o.data.energy *= 1e-4

def M(x, y, z): return Vector((x / 1000.0, -y / 1000.0, z / 1000.0))

def scene_media(quality, res):
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    MAT.clear(); KEYS.clear(); reset()
    tops = build_unit(); floor(); lights()
    ptrs = make_pointers(tops); keys, leds = lit_parts()
    to_metres()
    sc = bpy.context.scene
    fk.setup(sc, quality, res=res, blur=0.5 if MODE == "film" else 0)
    sc.view_settings.view_transform = "Standard"; sc.view_settings.look = "None"          # the colours of the UI render
    sc.view_settings.exposure = float(arg("--exposure", "-0.3"))
    return fk, sc, ptrs, keys, leds

def render_still(fk, sc, path, cam_pos, look, lens, fstop, focus=None):
    cam = fk.camera(sc, lens=lens, fstop=fstop)
    cam.location = cam_pos
    cam.rotation_euler = (look - cam_pos).to_track_quat("-Z", "Y").to_euler()
    cam.data.dof.focus_distance = (focus or look - cam_pos).length if focus is None else (focus - cam_pos).length
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 95
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True); print("wrote", path, flush=True)
    bpy.data.objects.remove(cam, do_unlink=True)

PHOTO_ON = ["step1", "step4", "step7", "step11", "step13", "track2", "play"]
def mode_photos():
    out = os.path.abspath(arg("--out", "photos")); os.makedirs(out, exist_ok=True)
    only = arg("--shot"); quality = arg("--quality", "final")
    shots = {   # name: (resolution, position, look, lens, f-stop)
        "hero":   ((2400, 1350), M(-270, 880, 330), M(150, 215, 30), 50, 9.0),
        "top":    ((2400, 1800), M(-20, 330, 190), M(120, 70, 50), 55, 10.0),
        "close":  ((2400, 1800), M(60, 340, 210), M(150, 190, 50), 50, 11.0),
        "keys":   ((2400, 1800), M(0, 520, 110), M(130, 375, 50), 40, 10.0),
        "filter": ((2400, 1800), M(40, 470, 130), M(110, 326, 50), 55, 10.0),
    }
    for name, (res, pos, look, lens, fs) in shots.items():
        if only and name not in only.split(","): continue
        if arg("--pos") and only:                         # try a viewpoint: --pos x,y,z --look x,y,z --lens mm --fstop n (panel mm)
            pos = M(*[float(v) for v in arg("--pos").split(",")]); look = M(*[float(v) for v in arg("--look").split(",")])
            lens = float(arg("--lens", lens)); fs = float(arg("--fstop", fs))
        fk, sc, ptrs, keys, leds = scene_media(quality, res)
        sc.render.resolution_percentage = 100 if quality == "final" else 50
        for i in PHOTO_ON:
            if i in keys: keys[i][0].hide_render = True; keys[i][1].hide_render = False
        for n, (off, g, rd) in leds.items():
            k = int(n[3:])
            if k in (1, 5, 9, 13): off.hide_render = True; g.hide_render = False
            if k == 11: off.hide_render = True; rd.hide_render = False
        render_still(fk, sc, os.path.join(out, name + ".jpg"), pos, look, lens, fs)

# Knob sweeps per shot: id → (centre u, amplitude, period in beats, phase)
SWEEPS = {
    "open":   {"cutoff": (0.55, 0.25, 16, 0.0), "tempo": (0.3, 0.0, 8, 0), "lfo": (0.5, 0.2, 8, 1.0)},
    "ops":    {k: (0.5, 0.28, 8 + i, 0.7 * i) for i, k in enumerate(
        ["op_a_ratio", "op_a_level", "op_a_feedback", "op_a_env", "op_b_level", "op_b_feedback", "op_b_env", "op_c_level", "op_c_detune", "op_c_env",
         "op_d_level", "op_d_feedback", "op_d_ratio", "algorithm", "data"])},
    "steps":  {"data": (0.5, 0.3, 8, 0), "algorithm": (0.5, 0.25, 12, 1.0)},
    "filter": {"cutoff": (0.5, 0.4, 8, 0.0), "reso": (0.4, 0.2, 6, 0.5), "fenv": (0.55, 0.25, 8, 1.2), "type": (0.3, 0.0, 8, 0),
               "drive": (0.45, 0.3, 12, 0.3), "age": (0.4, 0.3, 10, 2.0), "delay": (0.4, 0.3, 8, 0.8), "reverb": (0.4, 0.3, 16, 0.2)},
    "hero":   {"cutoff": (0.5, 0.3, 16, 0.0), "algorithm": (0.4, 0.2, 12, 1.0), "tempo": (0.3, 0.0, 8, 0), "op_b_level": (0.5, 0.25, 8, 0.4), "op_d_feedback": (0.5, 0.25, 12, 2.0)},
    "end":    {"cutoff": (0.6, 0.2, 16, 0.0)},
}

def film_timeline():
    """The trip hop demo's chain: for time t → (step inside the pattern, the pattern index). Bass track steps are what the keys show."""
    d = json.load(open(arg("--demo", os.path.join(HERE, "..", "demo_projects", "01-trip-hop.json"))))
    pats = {p["index"]: p for p in d["patterns"]}
    segs = []; t = 0.0
    for e in d["chain"]:
        for _ in range(int(e[1])):
            L = int(pats[e[0]]["length"]); segs.append((t, e[0], L)); t += L * STEP_S
    def at(T):
        T = T % max(t, 1e-6)
        for s0, pi, L in segs:
            if T < s0 + L * STEP_S: return int((T - s0) / STEP_S), pi
        return 0, segs[-1][1]
    def on_steps(pi):
        tr = pats[pi]["tracks"][1]
        return {int(s["i"]) for s in tr["steps"] if s.get("lane", 0) == 0}
    return at, on_steps

def film():
    out = os.path.abspath(arg("--out", "film")); only = arg("--shot"); quality = arg("--quality", "final")
    at, on_steps = film_timeline()
    f0 = 0
    for name, beats in FILM_SHOTS:
        frames = int(round(beats * BEAT * FPS)); first = f0; f0 += frames
        if only and name not in only.split(","): continue
        fk, sc, ptrs, keys, leds = scene_media(quality, (1920, 1080))
        cam = fk.camera(sc)
        Pth = lambda *a: fk.Path(list(a))
        if name == "open":
            path = fk.Path([(0, M(-260, 760, 420), M(150, 210, 40), 40, 9.0), (1, M(70, 450, 190), M(115, 60, 50), 55, 7.0)])
        elif name == "ops":
            path = fk.Path([(0, M(-90, 330, 190), M(60, 175, 50), 50, 7.0), (1, M(330, 420, 200), M(235, 235, 50), 50, 7.0)])
        elif name == "steps":
            path = fk.Path([(0, M(-40, 500, 120), M(60, 376, 50), 40, 7.0), (1, M(300, 500, 125), M(240, 376, 50), 40, 7.0)])
        elif name == "filter":
            path = fk.Path([(0, M(-50, 480, 150), M(80, 326, 50), 50, 7.0), (1, M(270, 490, 150), M(235, 326, 50), 50, 7.0)])
        elif name == "hero":
            path = fk.Path([(0, M(-300, 700, 330), M(150, 215, 40), 40, 7.0), (0.5, M(150, 840, 430), M(150, 215, 40), 40, 7.0), (1, M(560, 680, 340), M(150, 215, 40), 40, 7.0)])
        else:
            path = fk.Path([(0, M(560, 680, 340), M(150, 215, 40), 40, 7.0), (1, M(80, 1000, 700), M(150, 205, 35), 32, 8.0)], easing="out")
        sweeps = SWEEPS[name]
        lights_ = [o for o in sc.objects if o.type == "LIGHT"]
        for o in lights_: o["e0"] = o.data.energy

        def pose(f, t, u, name=name, path=path, first=first):
            T = (first + f - 1) / FPS
            path.apply(cam, u, shake=0.0004 if name in ("open", "ops", "steps", "filter") else 0.0, t=T)
            moved = []
            for kid, (c0, amp, per, ph) in sweeps.items():
                o = ptrs.get(kid)
                if o is None: continue
                uu = min(1.0, max(0.0, c0 + amp * math.sin(2 * math.pi * (T / BEAT) / per + ph)))
                o.rotation_euler = (0, 0, -math.radians(-135 + 270 * uu)); moved.append(o)
            return moved

        def keys_(f, name=name, first=first):
            T = (first + f - 1) / FPS
            step, pi = at(T)
            page = step // 16; on = on_steps(pi)
            for i in range(16):
                lit = (page * 16 + i) in on
                off, litcap = keys["step%d" % (i + 1)]
                off.hide_render = lit; litcap.hide_render = not lit
                off.keyframe_insert("hide_render"); litcap.keyframe_insert("hide_render")
                lo, g, rd = leds["led%d" % (i + 1)]
                head = (step % 16) == i
                lo.hide_render = head or lit; g.hide_render = head or not lit; rd.hide_render = not head
                for o in (lo, g, rd): o.keyframe_insert("hide_render")
            u = (f - 1) / max(1, frames - 1)
            if name == "open":
                for o in lights_:
                    o.data.energy = o["e0"] * fk.rack(u, 0.0, 0.7, 0.12, 1.0); o.data.keyframe_insert("energy")
            else:
                for o in lights_:
                    o.data.energy = o["e0"]; o.data.keyframe_insert("energy")
        fk.render_shot(sc, name, frames, pose, out, extra_keys=keys_)

if MODE == "photos": mode_photos()
elif MODE == "film": film()

