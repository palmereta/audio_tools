# Valence: the 3D atom (Blender 5.2, Cycles). Direction "Bohr luminoso": thin light orbits on deep black, electrons
# as comets with trails, warm protons and cold neutrons inside a glass nucleus, photons fired on the exact frame of
# every note in a demo's events.json.
#
#   blender -b --factory-startup -P valence.py -- --mode still --demo <demo dir> --t 28.0 --cam hero --res 1600x900 --samples 128 --out still.jpg
#   blender -b --factory-startup -P valence.py -- --mode anim  --demo <demo dir> --t0 22 --t1 36 --cam hero --out <frames dir>
#   blender -b --factory-startup -P valence.py -- --mode loop  --demo <demo dir> --out <frames dir> [--loop-s 10]
#
# Social videos (valence-social-fusion-1080x1920/1080x1350.mp4): the drum and bass part of the main demo
# (10_main/C_dnb, which starts at 147.84 s of 10_main.wav), t 12.6-30.6 around the drop on the fusion at 19.310:
#   ... --mode anim --demo <C_dnb> --t0 12.6 --t1 30.6 --samples 64 --shift 0 --cam persp1 --fill 0.85 --res 1080x1920
#   ... same with --fill 0.95 --res 1080x1350
# audio = 10_main.wav samples 7701120-8565120, loudnorm two-pass to -16 LUFS, H.264 crf 17 + AAC 192k.
#
# --mode still   one frame at --t seconds.
# --mode anim    frames [--t0, --t1) at 30 fps, driven by the demo (frame_00000.png ...; existing frames are skipped).
# --mode film2   the website film: the shots of tools/film.json over its edit of the main demo, at its fps, with a camera
#                move per shot (FILM_CAMS), as <out>/<shot>_0001.jpg ... for scripts/film.py (existing frames are skipped):
#   ... --mode film2 --demo <10_main> --samples 96 --shift 0 --out ~/film/out/valence [--shots open,drop] [--extra 12] [--step N]
# --mode loop    a seamless --loop-s second loop. The demo's real cycles are remapped so every layer does a whole
#                number of revolutions per loop, its notes are the demo's first cycles, and everything that moves
#                (precession, camera, nucleus spin, photons) is periodic, so the last frame leads into the first.
# --cam hero|persp1|persp2|top|low|orbit   (orbit = hero with a slow periodic sway, for the loop)
# --res WxH  --samples N  --fps 30  --stars N  --fstop F
# --fill F   overrides the preset's framing (smaller = closer; the social videos use ~0.8)   --shift X  lens shift_x (0.04)
#
# Units: 1 Blender unit = the largest orbit radius. Atom frame: orbits are tilted ellipses around the origin; the
# camera presets look at the nucleus. Every perspective preset must be checked by eye (parallax and floating bits only
# show there).
import bpy, bmesh, json, math, os, sys, bisect, random
from mathutils import Vector, Matrix, Quaternion

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
MODE = arg("--mode", "still"); DEMO = arg("--demo"); OUT = arg("--out", "out")
FPS = int(arg("--fps", "30")); SAMPLES = int(arg("--samples", "96"))
RES = tuple(int(v) for v in arg("--res", "1600x900").split("x")); CAM = arg("--cam", "hero")
FSTOP = float(arg("--fstop", "5.6")); NSTARS = int(arg("--stars", "140")); LOOP_S = float(arg("--loop-s", "10"))
FILL = arg("--fill"); SHIFT = float(arg("--shift", "0.04"))
TAU = math.tau

COL_SUN = (0.949, 0.784, 0.471); COL_COLD = (0.498, 0.718, 0.910)       # protons, neutrons (Look.h)
COL_LAYER = [(0.498, 0.820, 0.780), (0.949, 0.627, 0.482), (0.663, 0.612, 0.941), (0.549, 0.753, 0.961)]
def lin(c): return tuple(((v + 0.055) / 1.055) ** 2.4 if v > 0.04045 else v / 12.92 for v in c)
# (ecc, incl, node, omega, nodeRate, omegaRate, regOff): same family as AtomView.h, no perfect symmetry
OD = [(0.78, 0.30, 0.20, 0.20, 0.045, -0.020, 0.00), (0.60, -1.10, 2.05, 1.10, -0.030, 0.035, 0.025),
      (0.90, 1.45, -0.90, 2.60, 0.025, 0.050, 0.055), (0.72, 0.80, 2.60, -0.70, -0.050, -0.030, 0.09)]
RADII = [0.62, 0.82, 1.00, 0.40]      # always four clearly different orbits (smallest inner, largest outer)
NUC_R = 0.058            # nucleon radius
LIFE_RAY, LIFE_RING = 0.24, 0.55

# ───────── Data: demo events or the remapped loop ─────────
class Data:
    pass

def load_demo(path):
    e = json.load(open(os.path.join(path, "events.json")))
    d = Data(); d.bpm = e["bpm"]; d.loop = None
    d.notes = sorted(({"t": n["t"], "l": n["layer"] - 1, "step": n["step"], "vel": n["vel"], "nuc": n["nucleon"], "src": n["src"],
                       "note": n["note"], "accent": n["accent"], "ratchet": n["ratchet"], "beat": n["beat"], "tie": n["tie"]} for n in e["notes"]), key=lambda n: n["t"])
    d.layers = []
    for l in e["layers"]:
        pitches = sorted(n["note"] for n in e["notes"] if n["layer"] == l["layer"])
        d.layers.append({"length": l["length"], "cycle": l["cycleBeats"], "pitches": pitches,
                         "on": l["on"] and len(pitches) > 0})
    rows = {}
    try: rows = json.load(open(os.path.join(path, "preset.json"))).get("rows", {})
    except Exception: pass
    for i, lay in enumerate(d.layers):
        r = rows.get("l%d" % (i + 1), {}); pr = r.get("prob")
        lay["prob"] = [min(1.0, max(0.0, v / 100.0)) for v in pr] if pr else None       # step probability -> opacity of the mark
    d.chords = [(c["t"], c["protons"], c["neutrons"]) for c in e["chords"]]
    d.fusions = [f["t"] for f in e["fusions"]]
    d.raw = e
    return d

def make_loop(d, T):
    """Synthesise a T-second loop from the demo: whole revolutions per layer, the demo's first cycles as the notes."""
    L = Data(); L.bpm = d.bpm; L.loop = T; L.layers = []; L.notes = []
    t_end = d.chords[1][0] if len(d.chords) > 1 else 1e9
    for i, lay in enumerate(d.layers):
        sec = lay["cycle"] * 60.0 / d.bpm; k = max(1, round(T / sec))
        L.layers.append({**lay, "k": k, "P": T / k})
        have = [n for n in d.notes if n["l"] == i and n["t"] < t_end and n["beat"] < k * lay["cycle"]]
        for n in have:
            c = int(n["beat"] // lay["cycle"]); ph = (n["beat"] / lay["cycle"]) % 1.0
            L.notes.append({**n, "t": ((c % k) + ph) * T / k, "beat": 0})
        if not have: L.layers[-1]["on"] = False
    P = list(d.chords[0][1]); Nn = list(d.chords[0][2])
    L.notes.sort(key=lambda n: n["t"])
    L.chords = [(0.0, P, Nn)]; L.fusions = [0.0]
    return L

def layer_phase(D, l, t):
    lay = D.layers[l]
    if D.loop: return (t / lay["P"]) % 1.0
    return (t * D.bpm / 60.0 / lay["cycle"]) % 1.0

def layer_radius(D, l): return RADII[l]

# ───────── Scene ─────────
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene; sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("CUDA", "METAL"):
        try:
            prefs.compute_device_type = kind; prefs.get_devices()
            if any(d.type == kind for d in prefs.devices):
                for dv in prefs.devices: dv.use = dv.type == kind
                break
        except Exception: pass
    sc.cycles.device = "GPU"; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
    try: sc.cycles.denoising_use_gpu = True
    except Exception: pass
    sc.cycles.max_bounces = 6; sc.cycles.transmission_bounces = 6; sc.cycles.transparent_max_bounces = 16
    sc.cycles.sample_clamp_direct = 0; sc.cycles.sample_clamp_indirect = 8; sc.cycles.use_adaptive_sampling = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Base Contrast"
    sc.render.resolution_x, sc.render.resolution_y = RES; sc.render.resolution_percentage = 100
    sc.render.fps = FPS
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.0016, 0.0020, 0.0060, 1); bg.inputs["Strength"].default_value = 1.0
    return sc

FLASH_NODES = []        # Value nodes shared by every emissive material: the fusion flash multiplier
def emission_mat(name, color, strength, depth=None, objcolor=True, flash=1.0):
    """Emission * Object Info colour (r = per-object intensity) * depth fade * (1 + fusion flash)."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear(); N = nt.nodes; L = nt.links.new
    out = N.new("ShaderNodeOutputMaterial"); em = N.new("ShaderNodeEmission"); em.inputs["Color"].default_value = (*lin(color), 1)
    k = N.new("ShaderNodeMath"); k.operation = "MULTIPLY"; k.inputs[1].default_value = strength
    fl = N.new("ShaderNodeValue"); FLASH_NODES.append(fl)
    f1 = N.new("ShaderNodeMath"); f1.operation = "MULTIPLY_ADD"; f1.inputs[1].default_value = flash; f1.inputs[2].default_value = 1.0
    L(fl.outputs[0], f1.inputs[0])
    oi = N.new("ShaderNodeObjectInfo"); L(oi.outputs["Color"], N.new("ShaderNodeSeparateColor").inputs[0]) if False else None
    sep = N.new("ShaderNodeSeparateColor"); L(oi.outputs["Color"], sep.inputs[0])
    mul1 = N.new("ShaderNodeMath"); mul1.operation = "MULTIPLY"
    if objcolor: L(sep.outputs[0], mul1.inputs[0])
    else: mul1.inputs[0].default_value = 1.0
    L(f1.outputs[0], mul1.inputs[1])
    last = mul1.outputs[0]
    if depth is not None:
        cd = N.new("ShaderNodeCameraData"); dm = N.new("ShaderNodeMapRange"); dm.clamp = True
        dm.inputs["From Min"].default_value = depth - 1.1; dm.inputs["From Max"].default_value = depth + 1.1
        dm.inputs["To Min"].default_value = 1.3; dm.inputs["To Max"].default_value = 0.55
        L(cd.outputs["View Z Depth"], dm.inputs["Value"])
        m3 = N.new("ShaderNodeMath"); m3.operation = "MULTIPLY"; L(last, m3.inputs[0]); L(dm.outputs[0], m3.inputs[1]); last = m3.outputs[0]
    L(last, k.inputs[0]); L(k.outputs[0], em.inputs["Strength"]); L(em.outputs[0], out.inputs["Surface"])
    return m

def nucleon_mat(name, color):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*lin(color), 1); b.inputs["Roughness"].default_value = 0.28
    b.inputs["Emission Color"].default_value = (*lin(color), 1); b.inputs["Emission Strength"].default_value = 0.0
    b.inputs["Coat Weight"].default_value = 0.4
    N = nt.nodes; L = nt.links.new
    oi = N.new("ShaderNodeObjectInfo"); sep = N.new("ShaderNodeSeparateColor"); L(oi.outputs["Color"], sep.inputs[0])
    fl = N.new("ShaderNodeValue"); FLASH_NODES.append(fl)
    a = N.new("ShaderNodeMath"); a.operation = "MULTIPLY_ADD"; a.inputs[1].default_value = 2.4; a.inputs[2].default_value = 1.0; L(fl.outputs[0], a.inputs[0])
    mm = N.new("ShaderNodeMath"); mm.operation = "MULTIPLY"; L(sep.outputs[0], mm.inputs[0]); L(a.outputs[0], mm.inputs[1])
    s = N.new("ShaderNodeMath"); s.operation = "MULTIPLY"; s.inputs[1].default_value = 0.8; L(mm.outputs[0], s.inputs[0]); L(s.outputs[0], b.inputs["Emission Strength"])
    return m

def glass_mat():
    """Nucleus shell: clear, with a cool glossy rim (a Fresnel mix; a real Glass BSDF filled it with caustic speckle)."""
    m = bpy.data.materials.new("Glass"); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear(); N = nt.nodes; L = nt.links.new
    out = N.new("ShaderNodeOutputMaterial"); tr = N.new("ShaderNodeBsdfTransparent"); gl = N.new("ShaderNodeBsdfGlossy")
    gl.inputs["Roughness"].default_value = 0.06; gl.inputs["Color"].default_value = (0.8, 0.9, 1.0, 1)
    em = N.new("ShaderNodeEmission"); em.inputs["Color"].default_value = (0.35, 0.55, 0.9, 1); em.inputs["Strength"].default_value = 1.4
    ad = N.new("ShaderNodeAddShader"); L(gl.outputs[0], ad.inputs[0]); L(em.outputs[0], ad.inputs[1])
    lw = N.new("ShaderNodeLayerWeight"); lw.inputs["Blend"].default_value = 0.5
    pw = N.new("ShaderNodeMath"); pw.operation = "POWER"; pw.inputs[1].default_value = 4.0; L(lw.outputs["Facing"], pw.inputs[0])
    mx = N.new("ShaderNodeMixShader"); L(pw.outputs[0], mx.inputs[0]); L(tr.outputs[0], mx.inputs[1]); L(ad.outputs[0], mx.inputs[2])
    L(mx.outputs[0], out.inputs["Surface"])
    return m

def set_slot(o, m):
    """Per-object material, so pooled objects can share one mesh."""
    if o.material_slots[0].material != m: o.material_slots[0].material = m

def add_obj(name, mesh, mat=None, coll=None):
    o = bpy.data.objects.new(name, mesh); (coll or bpy.context.scene.collection).objects.link(o)
    if mat:
        if not mesh.materials: mesh.materials.append(None)
        o.material_slots[0].link = "OBJECT"; o.material_slots[0].material = mat
    return o

def uv_sphere(r, seg=24, rings=12):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=r)
    for f in bm.faces: f.smooth = True
    me = bpy.data.meshes.new("s"); bm.to_mesh(me); bm.free(); return me

def ring_mesh(inner, segs=64, name="ring"):
    bm = bmesh.new(); vs = []
    for j in range(segs):
        c, sn = math.cos(TAU * j / segs), math.sin(TAU * j / segs); vs.append((bm.verts.new((c, sn, 0)), bm.verts.new((c * inner, sn * inner, 0))))
    for j in range(segs):
        a0, i0 = vs[j]; a1, i1 = vs[(j + 1) % segs]; bm.faces.new((a0, a1, i1, i0))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); return me

def tube_topology(n, sides):
    faces = []
    for i in range(n - 1):
        for s in range(sides):
            a = i * sides + s; b = i * sides + (s + 1) % sides
            faces.append((a, b, b + sides, a + sides))
    return faces

def tube_verts(pts, radii, sides=6, up=None):
    """Ring of `sides` vertices around each point, in the plane normal to the polyline."""
    out = []; n = len(pts)
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)])
        if t.length < 1e-9: t = Vector((1, 0, 0))
        t.normalize(); u = (up or Vector((0, 0, 1)))
        if abs(t.dot(u)) > 0.95: u = Vector((1, 0, 0))
        a = t.cross(u).normalized(); b = t.cross(a).normalized()
        for s in range(sides):
            ang = TAU * s / sides; out.append(p + (a * math.cos(ang) + b * math.sin(ang)) * radii[i])
    return out

def mesh_from(verts, faces, name="m"):
    me = bpy.data.meshes.new(name); me.from_pydata([tuple(v) for v in verts], [], faces); me.update()
    for p in me.polygons: p.use_smooth = True
    return me

# ───────── Build ─────────
class Atom:
    pass

def build(D, cam_name):
    sc = reset(); A = Atom(); A.D = D; A.sc = sc; A.path = None
    cd = bpy.data.cameras.new("Cam"); A.cam = add_obj("Cam", cd); sc.camera = A.cam
    cd.lens = 50; cd.sensor_width = 36; cd.dof.use_dof = True; cd.dof.aperture_fstop = FSTOP; cd.clip_start = 0.05; cd.clip_end = 60
    A.depth = 5.0                                                              # set by place_camera
    # materials (depth fade needs the final camera distance, so build after the preset is known)
    preset = CAMS[cam_name]; A.preset = preset
    amax = max([layer_radius(D, l) for l in range(4) if D.layers[l]["on"]] or [1.0]); amax = max(0.55, amax / 0.95)
    A.depth = preset_dist(preset) * amax
    A.mat_orbit = [emission_mat("Orbit%d" % l, COL_LAYER[l], 1.9, depth=A.depth, objcolor=False, flash=4.5) for l in range(4)]
    A.mat_orbit_off = [emission_mat("OrbitOff%d" % l, COL_LAYER[l], 0.55, depth=A.depth, objcolor=False, flash=1.0) for l in range(4)]
    A.mat_step = [emission_mat("Step%d" % l, COL_LAYER[l], 5.0, depth=A.depth, flash=3.0) for l in range(4)]
    A.mat_trail = [emission_mat("Trail%d" % l, COL_LAYER[l], 16.0, depth=A.depth, objcolor=False, flash=2.5) for l in range(4)]
    white = tuple(0.55 * c + 0.45 for c in (1, 1, 1))
    A.mat_electron = [emission_mat("Electron%d" % l, tuple(0.45 * c + 0.55 for c in COL_LAYER[l]), 60.0, flash=2.0) for l in range(4)]
    A.mat_ray = [emission_mat("Ray%d" % l, tuple(0.7 * c + 0.3 for c in COL_LAYER[l]), 40.0, flash=0.5) for l in range(4)]
    A.mat_ring = [emission_mat("Ring%d" % l, COL_LAYER[l], 14.0, flash=0.5) for l in range(4)]
    A.mat_p = nucleon_mat("Proton", COL_SUN); A.mat_n = nucleon_mat("Neutron", COL_COLD); A.glass = glass_mat()
    A.mat_star = emission_mat("Star", (0.62, 0.70, 0.95), 3.0, objcolor=True, flash=1.0)
    # orbits (local frame meshes; the matrix moves them), step marks
    A.hollow = ring_mesh(0.80, 24, "hollow"); A.orb = []; A.steps = []; A.trail = []; A.elec = []; A.lit = []
    for l in range(4):
        lay = D.layers[l]; a = layer_radius(D, l); b = a * OD[l][0]; lay["a"] = a; lay["b"] = b
        pts = [Vector((a * math.cos(TAU * j / 192), b * math.sin(TAU * j / 192), 0)) for j in range(193)]
        verts = tube_verts(pts, [0.0021] * len(pts), 5, up=Vector((0, 0, 1)))
        me = mesh_from(verts, tube_topology(len(pts), 5), "orb%d" % l)
        o = add_obj("Orbit%d" % l, me, A.mat_orbit[l] if lay["on"] else A.mat_orbit_off[l]); A.orb.append(o)
        # step marks: lit if the step ever sounds, hollow (tiny, dim) if it is a rest
        sounding = {n["step"] % lay["length"] for n in D.notes if n["l"] == l}
        objs = []
        for s in range(lay["length"]):
            th = TAU * s / lay["length"]
            snd = s in sounding
            so = add_obj("Step%d_%d" % (l, s), uv_sphere(0.0085, 12, 6) if snd else A.hollow, A.mat_step[l])
            so.hide_render = not lay["on"]
            pr = lay["prob"][s] if lay.get("prob") and s < len(lay["prob"]) else 1.0
            objs.append((so, Vector((a * math.cos(th), b * math.sin(th), 0)), snd, pr))
        A.steps.append(objs)
        # trail: tapered tube with fixed topology, vertices rewritten each frame
        N = 40; sides = 6
        me = mesh_from([(0, 0, 0)] * (N * sides), tube_topology(N, sides), "trail%d" % l)
        to = add_obj("Trail%d" % l, me, A.mat_trail[l]); to.hide_render = not lay["on"]; A.trail.append(to)
        eo = add_obj("Electron%d" % l, uv_sphere(0.019, 20, 10), A.mat_electron[l]); eo.hide_render = not lay["on"]; A.elec.append(eo)
    # nucleus
    A.maxn = max(len(c[1]) + len(c[2]) for c in D.chords); A.maxn = max(A.maxn, 3)
    A.nuc = []
    for i in range(A.maxn + 2):
        o = add_obj("Nucleon%d" % i, uv_sphere(NUC_R, 32, 16), A.mat_p); A.nuc.append(o)
    for nm, loc, en, col in (("Key", (-2.6, -3.2, 3.0), 260, (1.0, 0.85, 0.65)), ("Fill", (3.4, -2.0, -1.6), 140, (0.55, 0.72, 1.0))):
        ld = bpy.data.lights.new(nm, "POINT"); ld.energy = en; ld.color = col; ld.shadow_soft_size = 0.8; ld.use_shadow = False
        add_obj(nm, ld).location = loc
    bl = bpy.data.lights.new("Burst", "POINT"); bl.energy = 0; bl.use_shadow = False; bl.color = (1.0, 0.9, 0.75); A.burst = add_obj("Burst", bl)
    A.shell = add_obj("Shell", uv_sphere(1.0, 48, 24), A.glass)
    bpy.context.view_layer.update()
    # photon pools
    cyl = bmesh.new(); bmesh.ops.create_cone(cyl, cap_ends=False, segments=8, radius1=1.0, radius2=1.0, depth=1.0)
    bmesh.ops.translate(cyl, verts=cyl.verts, vec=(0, 0, 0.5))
    for f in cyl.faces: f.smooth = True
    cme = bpy.data.meshes.new("ray"); cyl.to_mesh(cme); cyl.free()
    rm = bmesh.new(); inner = 0.965; segs = 64; vs = []
    for j in range(segs):
        c, s = math.cos(TAU * j / segs), math.sin(TAU * j / segs); vs.append((rm.verts.new((c, s, 0)), rm.verts.new((c * inner, s * inner, 0))))
    for j in range(segs):
        a0, i0 = vs[j]; a1, i1 = vs[(j + 1) % segs]; rm.faces.new((a0, a1, i1, i0))
    rme = bpy.data.meshes.new("ring"); rm.to_mesh(rme); rm.free()
    fm = bmesh.new(); vs = []
    for j in range(segs):
        c, sn = math.cos(TAU * j / segs), math.sin(TAU * j / segs); vs.append((fm.verts.new((c, sn, 0)), fm.verts.new((c * 0.994, sn * 0.994, 0))))
    for j in range(segs):
        a0, i0 = vs[j]; a1, i1 = vs[(j + 1) % segs]; fm.faces.new((a0, a1, i1, i0))
    fme = bpy.data.meshes.new("fring"); fm.to_mesh(fme); fm.free()
    A.frings = [add_obj("FRing%d" % i, fme, A.mat_ring[0]) for i in range(24)]
    for o in A.frings: o.hide_render = True
    A.rays = [add_obj("Ray%d" % i, cme, A.mat_ray[0]) for i in range(110)]
    A.rings = [add_obj("Ring%d" % i, rme, A.mat_ring[0]) for i in range(110)]
    for o in A.rays + A.rings: o.hide_render = True
    # stars: far, tiny, out of focus
    rnd = random.Random(7); A.stars = []
    for i in range(NSTARS):
        v = Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))).normalized() * rnd.uniform(4.5, 14)
        so = add_obj("Star%d" % i, uv_sphere(rnd.uniform(0.006, 0.02), 6, 4), A.mat_star); so.location = v
        k = rnd.uniform(0.25, 1.0); so.color = (k, k, k, 1)
    # event index per layer (for step lighting)
    A.times = [t["t"] for t in D.notes]
    A.g1, A.g2 = setup_compositor(sc)
    A.fmax = 1.25 * amax
    return A

# ───────── Camera presets: (azimuth deg, elevation deg, focal mm, fill) ─────────
CAMS = {"hero": (24, 40, 50, 1.20), "herotight": (24, 40, 50, 1.06), "persp1": (38, 26, 35, 1.15), "persp2": (-52, 14, 35, 1.20), "top": (14, 80, 50, 1.18),
        "low": (20, 6, 35, 1.20), "nuc": (15, 25, 50, 0.35), "orbit": (24, 38, 50, 1.20)}
def preset_dist(p):
    # sensor fit AUTO: the 36 mm go to the larger side, so a portrait frame (social videos) is narrow, not tall
    az, el, f, fill = p; fill = float(FILL) if FILL else fill; W, H = RES; sw, sh = (36, 36 * H / W) if W >= H else (36 * W / H, 36)
    half = min(sh / 2 / f, sw / 2 / f)
    return fill / half
def place_camera(A, t):
    az, el, f, fill = A.preset; dist = A.depth
    if A.path:                                 # film2: the shot's camera move, the distance follows the framing
        az, el, f, fill = A.path(t); dist = A.depth * preset_dist((az, el, f, fill)) / preset_dist(A.preset)
    if CAM == "orbit":
        ph = TAU * t / LOOP_S; az += 11 * math.sin(ph); el += 4 * math.sin(ph + 1.0)
    a, e = math.radians(az), math.radians(el)
    loc = Vector((dist * math.sin(a) * math.cos(e), -dist * math.cos(a) * math.cos(e), dist * math.sin(e)))
    A.cam.location = loc; A.cam.rotation_euler = (-loc).to_track_quat("-Z", "Y").to_euler()
    A.cam.data.lens = f; A.cam.data.dof.focus_distance = dist; A.cam.data.shift_x = SHIFT; A.cam.data.shift_y = 0.0

def setup_compositor(sc):
    g = bpy.data.node_groups.new("Comp", "CompositorNodeTree"); sc.compositing_node_group = g
    g.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
    N = g.nodes; L = g.links.new
    rl = N.new("CompositorNodeRLayers"); go = N.new("NodeGroupOutput")
    g1 = N.new("CompositorNodeGlare"); g2 = N.new("CompositorNodeGlare")
    for gl, sz, th, st in ((g1, 6, 1.2, 0.55), (g2, 9, 0.8, 0.30)):
        gl.inputs["Type"].default_value = "Fog Glow"; gl.inputs["Quality"].default_value = "High"
        gl.inputs["Threshold"].default_value = th; gl.inputs["Strength"].default_value = st; gl.inputs["Size"].default_value = sz / 10.0
        gl.inputs["Smoothness"].default_value = 1.0
    L(rl.outputs["Image"], g1.inputs["Image"]); L(g1.outputs[0], g2.inputs["Image"]); L(g2.outputs[0], go.inputs[0])
    return g1, g2

# ───────── Per-frame state ─────────
def rz(a): return Matrix.Rotation(a, 4, "Z")
def rx(a): return Matrix.Rotation(a, 4, "X")
def ry(a): return Matrix.Rotation(a, 4, "Y")
def hash01(x):
    x = (x ^ (x >> 16)) * 0x7feb352d & 0xffffffff; x = (x ^ (x >> 15)) * 0x846ca68b & 0xffffffff; x ^= x >> 16
    return (x & 0xffffff) / 16777216.0

def global_matrix(D, t):
    tilt = 0.50
    if D.loop: wob = 0.20 * math.sin(TAU * t / D.loop)
    else: wob = 0.20 * math.sin(t * 0.07) + 0.15 * math.sin(t * 0.031 + 1.0)
    return rx(tilt) @ ry(wob)

def orbit_matrix(D, l, t):
    ecc, incl, node, om, nr, orr, _ = OD[l]
    if D.loop:
        s = math.sin(TAU * t / D.loop + l * 1.3); node += 0.16 * s; om += 0.12 * math.sin(TAU * t / D.loop + l * 0.7 + 2.0)
    else: node += nr * t; om += orr * t
    return global_matrix(D, t) @ rz(node) @ rx(incl) @ rz(om)

def chord_at(D, t):
    k = 0
    for i, c in enumerate(D.chords):
        if c[0] <= t + 1e-9: k = i
    return k

def nucleon_target(i, n):
    if n <= 1: return Vector((0, 0, 0))
    rc = NUC_R * 1.25 * (0.45 + 0.85 * n ** (1 / 3.0)) * 0.9
    y = 1.0 - 2.0 * (i + 0.5) / n; r = math.sqrt(max(0.0, 1 - y * y)); phi = i * 2.3999632
    j = 0.80 + 0.35 * hash01(i * 7919 + 13)
    return Vector((r * math.cos(phi) * rc * j, r * math.sin(phi) * rc * j, y * rc * j))

def nucleus_state(A, t):
    D = A.D; k = chord_at(D, t); tc, P, N = D.chords[k]; n = len(P) + len(N)
    spin = TAU * t / D.loop if D.loop else t * 0.33
    spinM = rz(spin) @ rx(0.35 + 0.12 * math.sin(t * 0.21 if not D.loop else TAU * t / D.loop))
    if k > 0:
        pn = len(D.chords[k - 1][1]) + len(D.chords[k - 1][2])
    else: pn = n
    dt = t - tc; W0 = math.sqrt(62.0); zw = 3.75; wd = math.sqrt(62.0 - zw * zw)
    pos = []
    for i in range(n):
        T = nucleon_target(i, n)
        if k > 0 and dt < 2.5:
            x0 = (nucleon_target(i, pn) - T) if i < pn else Vector((0, 0, 0))
            v0 = Vector(((hash01(k * 131 + i * 17) - .5) * 1.5, (hash01(k * 71 + i * 29 + 5) - .5) * 1.5, (hash01(k * 53 + i * 43 + 9) - .5) * 1.5)) * 0.1
            e = math.exp(-zw * dt)
            T = T + e * (x0 * math.cos(wd * dt) + (v0 + zw * x0) * (math.sin(wd * dt) / wd))
        pos.append(T)
    G = global_matrix(D, t)
    return n, [(G @ spinM @ p) for p in pos], (len(P), P, N)

def electron_point(A, l, t, ph=None):
    D = A.D; lay = D.layers[l]
    if ph is None: ph = layer_phase(D, l, t)
    th = TAU * ph
    return orbit_matrix(D, l, t) @ Vector((lay["a"] * math.cos(th), lay["b"] * math.sin(th), 0))

def set_matrix(o, loc, rot3=None, scale=(1, 1, 1)):
    M = Matrix.Translation(loc)
    if rot3 is not None: M = M @ rot3.to_4x4()
    M = M @ Matrix.Diagonal((*scale, 1.0)); o.matrix_world = M

def fusion_flash(D, t):
    f = 0.0
    for ft in D.fusions:
        d = t - ft
        if D.loop:
            d = ((t - ft + D.loop / 2) % D.loop) - D.loop / 2
        if -0.14 < d < 0: f = max(f, 0.35 * (1 + d / 0.14) ** 2)
        elif d >= 0: f = max(f, math.exp(-d / 0.50) * (1.0 if d > 0.02 else d / 0.02 * 0.65 + 0.35))
    return f

def set_frame(A, t):
    D = A.D; sc = A.sc
    place_camera(A, t)
    flash = fusion_flash(D, t)
    for fn in FLASH_NODES: fn.outputs[0].default_value = flash
    cam_rot = A.cam.matrix_world.to_3x3()
    # nucleus
    n, npos, (npro, P, Nn) = nucleus_state(A, t)
    for i, o in enumerate(A.nuc):
        if i < n:
            o.hide_render = False; set_slot(o, A.mat_p if i < npro else A.mat_n)
            o.location = npos[i]
            o.color = (0.55, 0.55, 0.55, 1)
        else: o.hide_render = True
    rc = max((p.length for p in npos), default=0.05) + NUC_R * 1.7
    set_matrix(A.shell, Vector((0, 0, 0)), None, (rc, rc, rc))
    # electrons, trails, steps, orbits
    for l in range(4):
        lay = D.layers[l]; M = orbit_matrix(D, l, t); A.orb[l].matrix_world = M
        A.orb[l].hide_render = False
        if not lay["on"]: continue
        ph = layer_phase(D, l, t); th0 = TAU * ph
        pos = M @ Vector((lay["a"] * math.cos(th0), lay["b"] * math.sin(th0), 0))
        set_matrix(A.elec[l], pos); A.elec[l].hide_render = False
        # trail along the orbit behind the electron (arc fraction 0.17), tapered and fading
        # a tied note keeps the trail lit over the arc, from its step to the electron, until the next step
        span = 0.17; tied = False; stepsec = lay["cycle"] / lay["length"] * 60.0 / D.bpm
        for ni in notes_in(A, t - stepsec - 0.03, t):
            nn = D.notes[ni]
            if nn["l"] != l or not nn.get("tie"): continue
            age = (t - nn["t"]) % D.loop if D.loop else t - nn["t"]
            if 0 <= age < stepsec + 0.02:
                tied = True; span = max(span, (age / (stepsec * lay["length"])) + 0.10 / 1.0 * 0.4 + 0.02)
        NN = 40; pts = []; radii = []
        for j in range(NN):
            s = j / (NN - 1); th = th0 - TAU * span * s
            pts.append(M @ Vector((lay["a"] * math.cos(th), lay["b"] * math.sin(th), 0)))
            radii.append((0.016 * (1 - s) ** 1.1 + 0.0007) if not tied else 0.012 * (1 - 0.55 * s) + 0.0007)
        verts = tube_verts(pts, radii, 6)
        co = [c for v in verts for c in v]; A.trail[l].data.vertices.foreach_set("co", co); A.trail[l].data.update()
        A.trail[l].hide_render = False
        # steps: sounding steps lit, rests hollow and dim; a step flares when it fires
        lt = lay["length"]; ts = A.times
        for (so, lp, sounding, pr) in A.steps[l]:
            if sounding: so.matrix_world = M @ Matrix.Translation(lp)
            else: set_matrix(so, M @ lp, cam_rot, (0.0085, 0.0085, 0.0085))      # a rest: a hollow ring facing the camera
        for s_i, (so, lp, sounding, pr) in enumerate(A.steps[l]):
            k = 0.34 * (0.22 + 0.78 * pr) if sounding else 0.12                  # opacity of the mark = step probability
            so.color = (min(1.0, k), 0, 0, 1)
        # steps flare: find notes of this layer in the last 0.5 s
        for ni in notes_in(A, t - 0.5, t):
            nn = D.notes[ni]
            if nn["l"] != l: continue
            age = (t - nn["t"]) % D.loop if D.loop else t - nn["t"]
            so, lp, snd, pr = A.steps[l][nn["step"] % lt]
            if not snd: continue
            kk = min(1.0, 0.34 + 0.66 * math.exp(-age / 0.16)); so.color = (max(so.color[0], kk), 0, 0, 1)
            sc_ = 1.0 + 0.9 * math.exp(-age / 0.16); so.scale = (sc_, sc_, sc_)
    # photons
    ri = 0; gi = 0
    big = 0.0
    for ni in notes_in(A, t - LIFE_RING, t):
        nn = D.notes[ni]; l = nn["l"]
        if not D.layers[l]["on"]: continue
        age = (t - nn["t"]) % D.loop if D.loop else t - nn["t"]
        if age < -1e-9 or age >= LIFE_RING: continue
        vel = nn["vel"] / 127.0; acc = nn["accent"]
        # nucleon slot
        slot = None
        if nn["nuc"] == "proton":
            for j, nt_ in enumerate(P):
                if nt_ == nn["src"]: slot = j; break
        else:
            for j, nt_ in enumerate(Nn):
                if nt_ == nn["src"]: slot = npro + j; break
        if slot is None: slot = nn["note"] % max(1, n)
        slot = min(slot, n - 1)
        te = nn["t"] if not D.loop else nn["t"]
        ph_e = layer_phase(D, l, te) if not D.loop else layer_phase(D, l, te)
        p_fire = electron_point(A, l, t - age, ph_e) if False else electron_point(A, l, t, ph_e)
        # ray: from the electron (as it was fired; the orbit moves slowly) to the nucleon
        life = LIFE_RAY * (0.7 + 0.7 * vel) * (1.25 if acc else 1.0)
        if age < life and gi < len(A.rays):
            u = 1 - age / life; inten = (u ** 1.6) * (0.55 + 0.7 * vel) * (1.9 if acc else 1.0) * (0.8 if nn["ratchet"] else 1.0)
            a_ = electron_point(A, l, t) if False else p_fire; b_ = npos[slot]
            d = b_ - a_; L = d.length
            if L > 1e-6:
                o = A.rays[gi]; gi += 1; o.hide_render = False
                rad = (0.0013 + 0.0014 * vel) * (1.5 if acc else 1.0)
                q = d.to_track_quat("Z", "Y"); set_matrix(o, a_, q.to_matrix(), (rad, rad, L))
                o.color = (min(1.0, inten * 0.5), min(1.0, inten * 0.5), min(1.0, inten * 0.5), 1)
                set_slot(o, A.mat_ray[l])
        # ring: at the step mark where it fired, facing the camera, expanding
        if ri < len(A.rings):
            u = age / LIFE_RING; size = 0.028 + 0.050 * vel + (0.02 if acc else 0.0)
            rad = size * (0.25 + 0.95 * (1 - (1 - u) ** 2.2)); inten = (1 - u) ** 1.5 * (0.6 + 0.5 * vel) * (1.7 if acc else 1.0)
            o = A.rings[ri]; ri += 1; o.hide_render = False
            set_matrix(o, p_fire, cam_rot, (rad, rad, rad))
            o.color = (min(1.0, inten * 0.5),) * 3 + (1,)
            set_slot(o, A.mat_ring[l])
        # nucleon hit
        if slot < len(A.nuc):
            o = A.nuc[slot]; hit = math.exp(-age / 0.10) * (0.4 + 0.8 * vel) * (1.6 if acc else 1.0)
            o.color = (max(o.color[0], min(1.0, 0.55 + 0.45 * min(1.0, hit))), 0.5, 0.5, 1)
    # fusion rings off the nucleus
    fi_n = 0
    for fi, ft in enumerate(D.fusions):
        d = t - ft
        if D.loop: d = ((t - ft + D.loop / 2) % D.loop) - D.loop / 2
        for r_i in range(4):
            dd = d - 0.07 * r_i
            if 0 <= dd < 0.9 and fi_n < len(A.frings):
                u = dd / 0.9; rad = 0.10 + A.fmax * (1 - (1 - u) ** 2.2); o = A.frings[fi_n]; fi_n += 1; o.hide_render = False
                set_matrix(o, Vector((0, 0, 0)), cam_rot, (rad, rad, rad)); k = min(1.0, (1 - u) ** 2.0 * 0.9)
                o.color = (k, k, k, 1); set_slot(o, A.mat_ring[r_i % 4])
    # fusion: every layer fires from its step 0 into the nucleus, and a ring spreads in the plane of each orbit
    for fi, ft in enumerate(D.fusions):
        d = t - ft
        if D.loop: d = ((t - ft + D.loop / 2) % D.loop) - D.loop / 2
        if not (0 <= d < 0.9): continue
        for l in range(4):
            lay = D.layers[l]
            if not lay["on"]: continue
            if d < 0.5 and gi < len(A.rays):
                a_ = electron_point(A, l, t, 0.0); dv = -a_; Ln = dv.length
                inten = min(1.0, (1 - d / 0.5) ** 1.5 * 0.9); o = A.rays[gi]; gi += 1; o.hide_render = False
                q = dv.to_track_quat("Z", "Y"); rad = 0.0034 * (1 - d / 0.5) + 0.0012
                set_matrix(o, a_, q.to_matrix(), (rad, rad, Ln)); o.color = (inten, inten, inten, 1); set_slot(o, A.mat_ray[l])
            if fi_n < len(A.frings):
                u = d / 0.9; sc_ = 0.08 + 1.1 * (1 - (1 - u) ** 2.0); o = A.frings[fi_n]; fi_n += 1; o.hide_render = False
                o.matrix_world = orbit_matrix(D, l, t) @ Matrix.Diagonal((lay["a"] * sc_, lay["b"] * sc_, sc_, 1.0)); k = min(1.0, (1 - u) ** 1.2 * 0.9)
                o.color = (k, k, k, 1); set_slot(o, A.mat_ring[l])
    for o in A.rays[gi:]: o.hide_render = True
    for o in A.frings[fi_n:]: o.hide_render = True
    for o in A.rings[ri:]: o.hide_render = True
    A.g1.inputs["Strength"].default_value = 0.55 + 0.4 * flash; A.g2.inputs["Strength"].default_value = 0.30 + 0.25 * flash
    A.burst.data.energy = 420 * flash * flash

def notes_in(A, t0, t1):
    """Indices of notes with t in [t0, t1], wrapping around the loop."""
    D = A.D; ts = A.times
    if D.loop:
        T = D.loop; t0m = t0 % T; t1m = t1 % T
        if t0m <= t1m: return list(range(bisect.bisect_left(ts, t0m), bisect.bisect_right(ts, t1m)))
        return list(range(bisect.bisect_left(ts, t0m), len(ts))) + list(range(0, bisect.bisect_right(ts, t1m)))
    return list(range(bisect.bisect_left(ts, t0), bisect.bisect_right(ts, t1)))

# ───────── Modes ─────────
def render_to(sc, path, jpg=True):
    s = sc.render.image_settings
    if jpg: s.file_format = "JPEG"; s.quality = 95; s.color_mode = "RGB"
    else: s.file_format = "PNG"; s.color_mode = "RGB"; s.compression = 15
    sc.render.filepath = path; bpy.ops.render.render(write_still=True)

# ───────── Film (--mode film2) ─────────
# One move per shot of tools/film.json: keys (u, azimuth, elevation, focal mm, fill) eased over the whole shot.
FILM_CAMS = {
    "open":    [(0, 2, 26, 50, 1.95), (1, 22, 36, 50, 1.22)],      # push in from the stars, minimal techno
    "side":    [(0, 62, 10, 35, 1.12), (1, 88, 20, 35, 1.04)],     # low and sideways, into the fusion that starts the house
    "house":   [(0, 12, 76, 50, 1.28), (1, -28, 62, 50, 1.14)],    # overhead, turning: the 16, 12 and 7 step orbits
    "nucleus": [(0, 12, 22, 50, 0.50), (1, 42, 32, 50, 0.40)],     # close on the chord: protons and neutrons
    "bridge":  [(0, -42, 6, 35, 1.00), (1, -62, 18, 35, 1.45)],    # the bridge pulls out, low
    "dnb":     [(0, -52, 14, 35, 1.22), (1, -18, 24, 35, 1.06)],   # drum and bass, the hats come in on the first fusion
    "rise":    [(0, 34, 28, 50, 1.40), (1, 24, 38, 50, 1.02)],     # slow push in up to the drop
    "drop":    [(0, 24, 40, 50, 1.06), (1, -12, 30, 35, 1.10)],    # the break and the reese drop on the fusion
    "end":     [(0, 18, 40, 50, 1.20), (1, 28, 46, 50, 1.45)],     # pull back under the end card
}

def film_path(keys, t0, t1):
    def at(t):
        u = min(1.0, max(0.0, (t - t0) / max(1e-6, t1 - t0))); u = u * u * (3 - 2 * u)
        i = max(j for j in range(len(keys)) if keys[j][0] <= u or j == 0); i = min(i, len(keys) - 2)
        a, b = keys[i], keys[i + 1]; w = (u - a[0]) / (b[0] - a[0])
        return tuple(a[k] + (b[k] - a[k]) * w for k in range(1, 5))
    return at

def film2():
    """tools/film.json: "edit" maps the film's time to the demo's (cuts on the main demo's timeline, the part playing
    at each moment with its offset in 10_main.wav); every shot is rendered from its part's events.json."""
    spec = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "film.json")))
    fps = spec["fps"]; globals()["FPS"] = fps; ed = spec["edit"]
    def film_to_main(T):
        for g0, g1 in ed["cuts"]:
            if T < g1 - g0 or (g0, g1) == tuple(ed["cuts"][-1]): return g0 + T
            T -= g1 - g0
    def part_of(g): return [p for p in ed["parts"] if p[1] <= g + 1e-9][-1]
    want = arg("--shots"); want = want.split(",") if want else [sh["name"] for sh in spec["shots"]]; extra = int(arg("--extra", "0"))
    if len(want) > 1:                       # a second build() in the same Blender crashes it: one process per shot
        import subprocess
        for name in want:
            subprocess.run([bpy.app.binary_path, "-b", "--factory-startup", "-P", os.path.abspath(__file__), "--"]
                           + [a for i, a in enumerate(argv) if a != "--shots" and (i == 0 or argv[i - 1] != "--shots")] + ["--shots", name], check=True)
        return
    n0 = 0
    for k, sh in enumerate(spec["shots"]):
        n = int(round(sh["seconds"] * fps)); first, last = n0, n0 + n + (extra if k < len(spec["shots"]) - 1 else 0); n0 += n
        if sh["name"] not in want: continue
        name, offset = part_of(film_to_main((first + n / 2) / fps))         # the part playing in the middle of the shot
        todo = [f for f in range(first, last) if not os.path.exists(os.path.join(OUT, "%s_%04d.jpg" % (sh["name"], f - first + 1)))]
        todo = todo[::int(arg("--step", "1"))]                              # --step N: every Nth frame, to check a shot
        if not todo: continue
        keys = FILM_CAMS[sh["name"]]; CAMS[sh["name"]] = keys[0][1:]
        A = build(load_demo(os.path.join(DEMO, name)), sh["name"]); globals()["CAM"] = sh["name"]
        A.path = film_path(keys, film_to_main(first / fps) - offset, film_to_main((first + n - 1) / fps) - offset)
        A.sc.render.fps = fps
        for f in todo:
            set_frame(A, film_to_main(f / fps) - offset); bpy.context.view_layer.update()
            render_to(A.sc, os.path.join(OUT, "%s_%04d.jpg" % (sh["name"], f - first + 1)))
            print("frame", sh["name"], f - first + 1, "/", last - first, flush=True)

def main():
    if MODE == "film2": os.makedirs(OUT, exist_ok=True); film2(); return
    d = load_demo(DEMO)
    if MODE == "loop": d = make_loop(d, LOOP_S)
    A = build(d, "orbit" if MODE == "loop" and CAM == "hero" else CAM)
    if MODE == "loop" and CAM == "hero": globals()["CAM"] = "orbit"
    sc = A.sc
    if MODE == "still":
        t = float(arg("--t", "0")); set_frame(A, t); bpy.context.view_layer.update()
        os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True); render_to(sc, OUT, OUT.lower().endswith((".jpg", ".jpeg")))
    else:
        os.makedirs(OUT, exist_ok=True)
        if MODE == "loop": f0, f1 = 0, int(round(LOOP_S * FPS)); base = 0.0
        else: base = float(arg("--t0", "0")); f0 = 0; f1 = int(round((float(arg("--t1", "10")) - base) * FPS))
        step = int(arg("--step", "1")); first = int(arg("--first", "0")); last = int(arg("--last", str(f1)))
        for f in range(max(f0, first), min(f1, last), step):
            path = os.path.join(OUT, "frame_%05d.png" % f)
            if os.path.exists(path): continue
            set_frame(A, base + f / FPS); bpy.context.view_layer.update(); render_to(sc, path, False)
            print("frame", f, "/", f1, flush=True)
main()
