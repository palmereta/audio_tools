"""Una bandada de estorninos (murmuration) para la web de Murmur: cielo de atardecer sobre agua quieta,
el sol coral bajo apoyado en una línea de árboles lejana cuya silueta, con su reflejo, dibuja una forma
de onda; miles de pájaros chicos forman una nube que se pliega y fluye.

Corre en pc50-wsl (en ~/murmur-render, siempre bajo el lock de GPU compartido):
  B=~/ai/blender/blender-5.2.2-linux-x64/blender; L=~/ai/prerendered/.gpu.lock
  flock $L $B -b -P murmur.py -- --mode hero --shots hero,flock --out out --samples 256
  for s in wide close low rise; do flock $L $B -b -P murmur.py -- --mode film --shot $s --frames 216 --out film --samples 64; done
Las cuatro tomas (9 s a 24 fps) se encadenan con fundidos de 0,8 s: 33,6 s. Póster = cuadro 60 de "wide".
Opciones: --birds 60000 (cantidad), --t 2.0 (instante de la bandada para las fotos), --size 0.72 (escala de
la bandada), --birdscale 1.25, --pct 50 (escala de resolución para pruebas), --step N y --first N (cuadros).
Tiempos medidos en la 3090: foto 2400x1100 a 256 muestras ~4 s; cuadro 1080p a 64 muestras ~1,3 s.
Unidades: 1 = 1 m. La cámara mira hacia +y; el sol se pone hacia +y, un poco a la izquierda.
"""
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out")); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(arg("--samples", "160"))
NB = int(arg("--birds", "60000"))
PCT = int(arg("--pct", "100"))
SIZE = float(arg("--size", "0.72"))       # escala de la bandada (1 = 220 m de largo)
FPS = 24.0

SUN_AZ, SUN_EL = math.radians(-9.0), math.radians(0.62)
SUN = Vector((math.sin(SUN_AZ) * math.cos(SUN_EL), math.cos(SUN_AZ) * math.cos(SUN_EL), math.sin(SUN_EL)))
CORAL = (0.90, 0.36, 0.20)            # #f3a27e en lineal, un poco más saturado para la luz

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
    sc.cycles.max_bounces = 4; sc.cycles.diffuse_bounces = 2; sc.cycles.glossy_bounces = 2
    sc.cycles.transmission_bounces = 2; sc.cycles.volume_bounces = 0
    sc.render.use_persistent_data = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGB"
    sc.render.resolution_percentage = PCT
    return sc

# ───────── cielo ─────────
class Nodes:
    """Atajos para armar árboles de nodos sin repetir cinco líneas por operación."""
    def __init__(self, nt): self.nt = nt; self.N = nt.nodes; self.L = nt.links
    def _in(self, sock, v):
        if hasattr(v, "is_output"): self.L.new(v, sock)
        else: sock.default_value = v
    def math(self, op, a, b=0.0, c=0.0):
        n = self.N.new("ShaderNodeMath"); n.operation = op
        self._in(n.inputs[0], a); self._in(n.inputs[1], b); self._in(n.inputs[2], c); return n.outputs[0]
    def vec(self, op, a, b=(0, 0, 0), scale=1.0):
        n = self.N.new("ShaderNodeVectorMath"); n.operation = op
        self._in(n.inputs[0], a); self._in(n.inputs[1], b)
        if op == "SCALE": self._in(n.inputs["Scale"], scale)
        return n.outputs["Value"] if op in ("DOT_PRODUCT", "LENGTH", "DISTANCE") else n.outputs["Vector"]
    def maprange(self, v, a, b, c=0.0, d=1.0):
        n = self.N.new("ShaderNodeMapRange"); n.clamp = True
        self._in(n.inputs["Value"], v); n.inputs["From Min"].default_value = a; n.inputs["From Max"].default_value = b
        n.inputs["To Min"].default_value = c; n.inputs["To Max"].default_value = d; return n.outputs["Result"]
    def smooth(self, v, a, b):
        n = self.N.new("ShaderNodeMapRange"); n.interpolation_type = "SMOOTHSTEP"; n.clamp = True
        self._in(n.inputs["Value"], v); n.inputs["From Min"].default_value = a; n.inputs["From Max"].default_value = b
        return n.outputs["Result"]
    def xyz(self, x, y, z):
        n = self.N.new("ShaderNodeCombineXYZ")
        for i, v in enumerate((x, y, z)): self._in(n.inputs[i], v)
        return n.outputs[0]

def sky():
    """Degradé de atardecer por elevación, halo coral alrededor del sol, disco del sol y estratos finos."""
    sc = bpy.context.scene
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    nt = w.node_tree; X = Nodes(nt)
    bg = nt.nodes["Background"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    d = X.vec("NORMALIZE", tc.outputs["Generated"])            # en el mundo, Generated = dirección del rayo
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(d, sep.inputs[0])
    z = sep.outputs["Z"]
    ramp = nt.nodes.new("ShaderNodeValToRGB"); cr = ramp.color_ramp; cr.interpolation = "B_SPLINE"
    stops = [(0.00, (0.42, 0.12, 0.040)), (0.05, (0.20, 0.065, 0.045)), (0.15, (0.055, 0.040, 0.050)),
             (0.34, (0.013, 0.022, 0.030)), (1.00, (0.002, 0.003, 0.005))]
    cr.elements[0].position, cr.elements[0].color = stops[0][0], (*stops[0][1], 1)
    cr.elements[1].position, cr.elements[1].color = stops[-1][0], (*stops[-1][1], 1)
    for p, c in stops[1:-1]:
        e = cr.elements.new(p); e.color = (*c, 1)
    nt.links.new(X.maprange(z, -0.01, 0.6), ramp.inputs["Fac"])
    dot = X.vec("DOT_PRODUCT", d, tuple(SUN))
    cl = X.math("MAXIMUM", dot, 0.0)
    p3 = X.math("POWER", cl, 3.0); p40 = X.math("POWER", cl, 40.0); p900 = X.math("POWER", cl, 900.0)
    # Del lado del sol el cielo es más cálido y claro; del lado opuesto se apaga.
    broad = X.math("MULTIPLY_ADD", p3, 1.2, 0.22)
    base = X.vec("SCALE", ramp.outputs["Color"], scale=broad)
    # Del lado contrario al sol, la hora azul: un velo verde azulado (el teal de la interfaz) a media altura.
    tband = X.math("MULTIPLY", X.smooth(z, 0.02, 0.12), X.math("SUBTRACT", 1.0, X.smooth(z, 0.25, 0.55)))
    base = X.vec("ADD", base, X.vec("SCALE", (0.008, 0.026, 0.030), scale=X.math("MULTIPLY", tband, X.math("SUBTRACT", 1.0, p3))))
    # En los reflejos (rayos glossy) el núcleo del sol se apaga: si no, el agua lo devuelve como una barra blanca.
    lp = nt.nodes.new("ShaderNodeLightPath")
    damp = X.math("MULTIPLY_ADD", lp.outputs["Is Glossy Ray"], -0.45, 1.0)
    near = X.math("MULTIPLY_ADD", X.math("MULTIPLY", p900, damp), 1.1, X.math("MULTIPLY", p40, 0.45))
    # Sólo para los reflejos: un halo coral más ancho que dibuja en el agua un camino suave hacia el sol.
    p200 = X.math("POWER", cl, 200.0)
    near = X.math("MULTIPLY_ADD", X.math("MULTIPLY", p200, lp.outputs["Is Glossy Ray"]), 1.4, near)
    lowband = X.math("SUBTRACT", 1.0, X.smooth(z, 0.0, 0.25))
    glow = X.vec("SCALE", CORAL, scale=X.math("MULTIPLY", near, lowband))
    skyc = X.vec("ADD", base, glow)
    # Estratos: ruido proyectado sobre un plano de nubes; la perspectiva los aplasta en bandas hacia el horizonte.
    zz = X.math("ADD", z, 0.035)
    q = X.xyz(X.math("DIVIDE", sep.outputs["X"], zz), X.math("MULTIPLY", X.math("DIVIDE", sep.outputs["Y"], zz), 2.5), 0.0)
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 0.22
    nz.inputs["Detail"].default_value = 7.0; nz.inputs["Roughness"].default_value = 0.58
    nt.links.new(q, nz.inputs["Vector"])
    dens = X.maprange(nz.outputs["Fac"], 0.50, 0.70)
    band = X.math("MULTIPLY", X.smooth(z, 0.004, 0.03), X.math("SUBTRACT", 1.0, X.smooth(z, 0.07, 0.22)))
    dens = X.math("MULTIPLY", X.math("MULTIPLY", dens, band), 0.85)
    cloudlit = X.math("MULTIPLY_ADD", p40, 1.6, X.math("MULTIPLY", p3, 0.35))
    cloud = X.vec("ADD", X.vec("SCALE", CORAL, scale=cloudlit), (0.008, 0.006, 0.009))
    mixc = X.vec("ADD", X.vec("SCALE", skyc, scale=X.math("SUBTRACT", 1.0, dens)), X.vec("SCALE", cloud, scale=dens))
    # Disco del sol (radio aparente agrandado a 0.6°), tapado por las nubes donde las hay.
    disk = X.smooth(dot, math.cos(math.radians(0.64)), math.cos(math.radians(0.56)))
    diskc = X.vec("SCALE", (1.0, 0.24, 0.07), scale=X.math("MULTIPLY", X.math("MULTIPLY", disk, damp), X.math("MULTIPLY_ADD", dens, -1.0, 1.4)))
    nt.links.new(X.vec("ADD", mixc, diskc), bg.inputs["Color"]); bg.inputs["Strength"].default_value = 1.0
    if hasattr(w, "cycles"): w.cycles.sampling_method = "MANUAL"; w.cycles.sample_map_resolution = 2048
    # La luz del sol bajo: viene de atrás de la bandada y le recorta un filo cálido.
    l = bpy.data.lights.new("Sun", "SUN"); l.energy = 1.2; l.color = (1.0, 0.50, 0.30); l.angle = math.radians(1.2)
    o = link(bpy.data.objects.new("Sun", l)); o.rotation_euler = (-SUN).to_track_quat("-Z", "Y").to_euler()
    o.visible_glossy = False          # a pleno, su reflejo en el agua era una barra blanca

# ───────── materiales y objetos ─────────
def mat(name, col, rough, metal=0.0, spec=0.5, ior=1.45):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal; b.inputs["IOR"].default_value = ior
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    return m

def water_material():
    """Agua quieta: muy oscura, casi espejo, con ondas finas estiradas a lo ancho."""
    m = mat("Water", (0.004, 0.006, 0.008), 0.08, ior=1.33)
    nt = m.node_tree; N = nt.nodes; b = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord"); mp = N.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (0.12, 0.5, 1.0)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 3.0; nz.inputs["Detail"].default_value = 6.0
    nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.22; bump.inputs["Distance"].default_value = 0.02
    nt.links.new(nz.outputs["Fac"], bump.inputs["Height"]); nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

def link(o): bpy.context.collection.objects.link(o); return o

def water():
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=6000)
    me = bpy.data.meshes.new("Water"); bm.to_mesh(me); bm.free(); me.materials.append(water_material())
    o = link(bpy.data.objects.new("Water", me)); o.location = (0, 2000, 0)

def treeline():
    """Árboles lejanos: columnas de 6 m cuyo alto sigue una forma de onda grabada (envolventes y picos).
    Con su reflejo en el agua la silueta queda simétrica, como la onda del plugin."""
    rng = np.random.default_rng(3)
    xs = np.arange(-5200.0, 5200.0, 3.5)
    env = 6 + 30 * np.exp(-((xs + 900) / 700) ** 2) + 44 * np.exp(-((xs - 600) / 520) ** 2) \
        + 24 * np.exp(-((xs - 2100) / 900) ** 2) + 18 * np.exp(-((xs + 2600) / 1100) ** 2)
    osc = 0.55 + 0.45 * np.abs(np.sin(xs * 0.031) * np.sin(xs * 0.0083 + 1.0))
    h = env * osc * (0.62 + 0.38 * rng.uniform(0, 1, xs.size) ** 0.5) + 2.5
    ys = 2200 + 180 * np.sin(xs / 1300.0)
    bm = bmesh.new(); top = []; bot = []
    for x, y, hh in zip(xs, ys, h):
        bot.append(bm.verts.new((x, y, -3.0))); top.append(bm.verts.new((x, y, hh)))
    for i in range(len(xs) - 1): bm.faces.new((bot[i], bot[i + 1], top[i + 1], top[i]))
    me = bpy.data.meshes.new("Trees"); bm.to_mesh(me); bm.free()
    me.materials.append(mat("Trees", (0.003, 0.003, 0.004), 0.9, spec=0.1))
    link(bpy.data.objects.new("Trees", me))

# ───────── la bandada ─────────
class Flock:
    """Cada pájaro tiene coordenadas fijas (u, v) sobre una lámina y un espesor w. La lámina se pliega,
    se tuerce y gira con el tiempo, y un campo de ondas la deforma: vista de canto, la bandada oscurece en
    bandas, que es lo que se ve en una murmuración real. El rumbo de cada pájaro sale de su velocidad."""
    def __init__(self, n, seed=7):
        r = np.random.default_rng(seed); self.n = n
        self.th = r.uniform(0, 2 * np.pi, n)
        self.rho = np.sqrt(r.uniform(0, 1, n)) ** 1.15
        self.w = np.clip(r.normal(0, 1, n), -2.5, 2.5)
        self.jph = r.uniform(0, 2 * np.pi, (3, n)); self.jf = r.uniform(0.3, 0.9, (3, n))
        self.span = 0.42 * r.uniform(0.85, 1.15, n)
        self.flap_f = r.uniform(4.0, 7.0, n); self.flap_ph = r.uniform(0, 2 * np.pi, n)
        self.glide = r.uniform(0, 1, n) < 0.3
        # Campo de ondas: seis modos con dirección, largo (60–200 m), amplitud y velocidad propias.
        k = r.normal(0, 1, (6, 3)); k /= np.linalg.norm(k, axis=1)[:, None]
        self.kv = k * (2 * np.pi / r.uniform(60, 200, 6))[:, None]
        a = r.normal(0, 1, (6, 3)); a -= (a * k).sum(1)[:, None] * k; a /= np.linalg.norm(a, axis=1)[:, None]
        self.ka = a * r.uniform(3, 9, 6)[:, None]
        self.kw = r.uniform(0.2, 0.5, 6); self.kph = r.uniform(0, 2 * np.pi, 6)

    @staticmethod
    def center(t):
        return np.array([45 + 25 * math.sin(0.21 * t), 260 + 15 * math.sin(0.13 * t + 1), 33 + 5 * math.sin(0.17 * t)])

    def pos(self, t):
        th, rho = self.th, self.rho; S = SIZE
        R = 1 + 0.16 * np.sin(2 * th + 0.31 * t + 0.4) + 0.11 * np.sin(3 * th - 0.23 * t + 1.3) + 0.07 * np.sin(5 * th + 0.41 * t + 2.1)
        Lx = 110 * S * (1 + 0.15 * math.sin(0.17 * t)); Lz = 42 * S * (1 - 0.2 * math.sin(0.17 * t + 0.5))
        a = Lx * rho * R * np.cos(th); b = Lz * rho * R * np.sin(th)
        a0 = a / S; b0 = b / S
        thick = S * (1.8 + 2.4 * (1 - rho ** 2))
        y = S * (30 * np.sin(0.045 * a0 + 0.37 * t) + 12 * np.sin(0.05 * b0 + 0.012 * a0 - 0.29 * t + 1.0)
                 + 8 * np.sin(0.035 * a0 - 0.04 * b0 + 0.53 * t + 2.0)) + self.w * thick
        psi = 0.5 * np.sin(0.008 * a0 + 0.25 * t)                 # torsión a lo largo de la lámina
        y, b = y * np.cos(psi) - b * np.sin(psi), y * np.sin(psi) + b * np.cos(psi)
        phi = 0.6 + 0.5 * math.sin(0.09 * t)                      # la bandada entera gira despacio
        x = a * math.cos(phi) - y * math.sin(phi); y = a * math.sin(phi) + y * math.cos(phi)
        p = np.stack([x, y, b], 1)
        ph = (p / S) @ self.kv.T + self.kw * t + self.kph         # (n, 6)
        p += S * (np.sin(ph) @ self.ka)
        p += 0.8 * np.sin(self.jf.T * t + self.jph.T)             # cada pájaro se mueve un poco por su cuenta
        return p + self.center(t)

    def geometry(self, t):
        """Ocho vértices por pájaro: cuerpo en rombo fino (pico, cola, flancos) y dos alas triangulares
        barridas hacia atrás que aletean; de lejos es una mota, de cerca la silueta de un estornino."""
        dt = 0.04; p = self.pos(t); v = (self.pos(t + dt) - self.pos(t - dt)) / (2 * dt)
        f = v / np.maximum(np.linalg.norm(v, axis=1), 1e-6)[:, None]
        s = np.cross(f, np.array([0.0, 0.0, 1.0])); s /= np.maximum(np.linalg.norm(s, axis=1), 1e-6)[:, None]
        u = np.cross(s, f)
        amp = np.where(self.glide, 0.08, 0.55)
        ang = (0.12 + amp * np.sin(2 * np.pi * self.flap_f * t + self.flap_ph))[:, None]
        hs = (self.span / 2)[:, None]
        k = self.span[:, None] / 0.42                              # proporciones del cuerpo según el tamaño
        nose = p + f * 0.12 * k; tail = p - f * 0.12 * k
        fl_ = p + s * 0.03 * k; fr_ = p - s * 0.03 * k
        rf = p + f * 0.045 * k; rb = p - f * 0.05 * k
        wl = p + s * hs * np.cos(ang) + u * hs * np.sin(ang) - f * 0.11 * k
        wr = p - s * hs * np.cos(ang) + u * hs * np.sin(ang) - f * 0.11 * k
        return np.stack([nose, tail, fl_, fr_, rf, rb, wl, wr], 1).reshape(-1, 3)

def flock_object(fl, scale):
    fl.span *= scale
    n = fl.n; me = bpy.data.meshes.new("Flock")
    i = np.arange(n) * 8
    faces = np.concatenate([np.stack([i, i + 2, i + 1], 1), np.stack([i, i + 1, i + 3], 1),
                            np.stack([i + 4, i + 6, i + 5], 1), np.stack([i + 4, i + 5, i + 7], 1)]).tolist()
    me.from_pydata(fl.geometry(0.0).tolist(), [], faces); me.update()
    m = mat("Bird", (0.018, 0.020, 0.022), 0.8, spec=0.04)
    me.materials.append(m)
    return link(bpy.data.objects.new("Flock", me))

def set_flock(o, fl, t):
    o.data.vertices.foreach_set("co", fl.geometry(t).ravel().astype(np.float32)); o.data.update()

# ───────── cámara y render ─────────
def camera(loc, target, lens=35, res=(2400, 1100), fstop=None):
    sc = bpy.context.scene
    cam = sc.camera.data if sc.camera else bpy.data.cameras.new("Cam")
    if not sc.camera:
        o = link(bpy.data.objects.new("Cam", cam)); sc.camera = o
    o = sc.camera; cam.lens = lens; cam.clip_start = 0.1; cam.clip_end = 20000
    o.location = Vector(loc); t = Vector(target)
    o.rotation_euler = (t - o.location).to_track_quat("-Z", "Y").to_euler()
    cam.dof.use_dof = fstop is not None
    if fstop: cam.dof.aperture_fstop = fstop; cam.dof.focus_distance = (t - o.location).length
    sc.render.resolution_x, sc.render.resolution_y = res
    return o

def render(path): bpy.context.scene.render.filepath = path; bpy.ops.render.render(write_still=True)

def build(scale):
    reset(); sky(); water(); treeline()
    fl = Flock(NB); o = flock_object(fl, scale)
    return fl, o

def mode_hero():
    fl, o = build(float(arg("--birdscale", "1.25")))
    t = float(arg("--t", "2.0"))
    set_flock(o, fl, t); c = fl.center(t)
    shots = {"hero": ((0, 0, 1.6), (0, 100, 1.6 + 100 * math.tan(math.radians(4.8))), 35, (2400, 1100)),
             # Detalle de la bandada: tele desde más cerca, alineado con el sol para que los pájaros se
             # recorten contra el resplandor; 4:3 para las tarjetas de la web.
             "flock": ((c[0] + 0.156 * 140, c[1] - 0.988 * 140, 5), (c[0] - 3, c[1], 21), 85, (1600, 1200))}
    for nm in (arg("--shots") or "hero,flock").split(","):
        loc, tgt, lens, res = shots[nm]
        camera(loc, tgt, lens, res)
        render(os.path.join(OUT, f"murmur_{nm}.png"))

# Tomas del film: cada una sigue la historia de la bandada donde la dejó la anterior.
T0 = {"wide": 0.0, "close": 9.0, "low": 18.0, "rise": 27.0}

def shot_cam(shot, e, t, fl):
    c = fl.center(t)
    if shot == "wide":          # la bahía entera, acercándose apenas
        return (0, 30 * e, 1.6), (4 * e, 130, 1.6 + 130 * math.tan(math.radians(5.5))), 35
    if shot == "close":         # adentro de la bandada: tele que la acompaña
        return (c[0] - 50 + 20 * e, c[1] - 150, 14 + 4 * e), (c[0] - 5, c[1], c[2] + 4), 60
    if shot == "low":           # al ras del agua: la bandada y su reflejo
        return (-20 + 20 * e, 110, 0.45), (20 * e, 400, 0.45 + 290 * math.tan(math.radians(6.5))), 28
    # rise: la cámara sube y se aleja, la bandada sigue sola
    return (-10 * e, -40 * e, 2.0 + 10 * e), (10, 300, 2.0 + 10 * e + 300 * math.tan(math.radians(6.5 + 1.5 * e))), 26

def mode_film():
    fl, o = build(float(arg("--birdscale", "1.25")))
    shot = arg("--shot", "wide"); frames = int(arg("--frames", "216"))
    t0 = float(arg("--t0", str(T0[shot]))); first = int(arg("--first", "1"))
    d = os.path.join(OUT, f"film_{shot}"); os.makedirs(d, exist_ok=True)
    step = int(arg("--step", "1"))                                # >1 para revisar una toma salteando cuadros
    for f in range(first - 1, frames, step):
        t = t0 + f / FPS
        set_flock(o, fl, t)
        e = f / max(1, frames - 1); e = e * e * (3 - 2 * e)
        loc, tgt, lens = shot_cam(shot, e, t, fl)
        camera(loc, tgt, lens, (1920, 1080))
        render(os.path.join(d, f"f_{f + 1:04d}.png"))

{"hero": mode_hero, "film": mode_film}[arg("--mode", "hero")]()
