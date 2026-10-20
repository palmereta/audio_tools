"""Una bandada de estorninos (murmuration) para la web de Murmur: cielo de atardecer sobre agua quieta, el sol
bajo apoyado en una línea de árboles lejana cuya silueta, con su reflejo, dibuja una forma de onda; decenas de
miles de pájaros con alas que aletean forman una nube que se hincha, se divide y gira con la música.

Corre en pc50-wsl (siempre bajo el lock de GPU compartido; B = ~/ai/blender/blender-5.2.2-linux-x64/blender):
  flock ~/ai/prerendered/.gpu.lock $B -b --factory-startup -P murmur.py -- --mode hero --out out [--shots hero,flock]
      murmur_hero.png (2400 × 1100) y murmur_flock.png (1600 × 1200); tools/web_images.py las pasa a la web
  flock ~/ai/prerendered/.gpu.lock $B -b --factory-startup -P murmur.py -- --mode film2 --audio <wav> --out <dir>
        [--quality draft|preview|final] [--shot open,close] [--step N | --frames 1,49] [--denoise 0|1]
      las seis tomas del film (ver film2); scripts/film.py las arma con tools/film.json
  --mode film --shot wide|close|low|rise y --mode hero-v1: el film y las fotos anteriores (murmur_v1.py, intacto)
Opciones: --birds 50000 (cantidad), --t 6.0 (instante de la bandada en las fotos; si no, 6 s en hero y 11 s en flock), --size 0.72 (escala de la
bandada), --birdscale 1.2, --samples 384 (fotos), --denoise 0|1 (sin denoiser por omisión: empasta los pájaros).
Unidades: 1 = 1 m. La cámara base está en el origen y mira hacia +y; el sol se pone hacia +y, un poco a la izquierda.
Fuera de Blender el módulo se puede importar (sin bpy) para previsualizar la coreografía con numpy.
"""
import math, os, sys
import numpy as np
try:
    import bpy, bmesh
except ImportError:                                      # previsualización fuera de Blender
    bpy = bmesh = None
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
def arg(n, d=None): return argv[argv.index(n) + 1] if n in argv else d
OUT = os.path.abspath(arg("--out", "out"))
SAMPLES = int(arg("--samples", "384"))
NB = int(arg("--birds", "50000"))
SIZE = float(arg("--size", "0.72"))       # escala de la bandada (1 = 220 m de largo)
BIRDSCALE = float(arg("--birdscale", "1.2"))
FPS = 24.0
HERE = os.path.dirname(os.path.abspath(__file__))

SUN_AZ, SUN_EL = math.radians(-9.0), math.radians(0.62)
SUN = Vector((math.sin(SUN_AZ) * math.cos(SUN_EL), math.cos(SUN_AZ) * math.cos(SUN_EL), math.sin(SUN_EL)))
CORAL = (0.90, 0.36, 0.20)            # #f3a27e en lineal, un poco más saturado para la luz


# ───────── la música mueve la bandada ─────────
def onepole(x, sec):
    """Suavizado de un polo (constante de tiempo en segundos), cuadro a cuadro."""
    a = math.exp(-1.0 / (sec * FPS)); y = np.empty_like(x); acc = x[0]
    for i, v in enumerate(x): acc = a * acc + (1 - a) * v; y[i] = acc
    return y

def smoothstep(x): x = np.clip(x, 0.0, 1.0); return x * x * (3 - 2 * x)

class Drive:
    """Curvas por cuadro que salen de la envolvente del audio (filmkit.envelope): energía rápida y lenta, un tiempo
    interno que corre más rápido cuando la música sube, la división (cuando la frase respira, la bandada se parte en
    dos y se vuelve a juntar al entrar la frase siguiente) y los giros (cada ataque dispara un giro que recorre la
    bandada como una ola). Sin audio, una bandada tranquila de energía constante."""
    def __init__(self, env=None, seconds=60.0):
        e = np.array(env, float) if env is not None else np.full(int(seconds * FPS), 0.65)
        e = np.concatenate([e, np.full(int(4 * FPS), e[-1])])         # cola para mirar un poco más allá
        self.T = np.arange(len(e)) / FPS
        self.ef = onepole(e, 0.35); self.es = onepole(e, 1.0)
        rate = 0.55 + 0.8 * self.ef
        self.tau = np.cumsum(rate) / FPS                                # tiempo de la forma
        sp = np.clip((0.58 - self.es) / 0.25, 0, 1)
        self.sp = smoothstep(onepole(sp, 1.2))                          # 0 junta, 1 dividida
        self.circ = np.cumsum(rate * (1 - 0.75 * self.sp)) / FPS        # recorrido de cada pájaro por la lámina
        turn = np.zeros(len(e)); last = -99; k = 0
        if env is not None:
            for i in range(1, len(e)):
                if e[i] - e[i - 1] > 0.15 and i - last >= 24:
                    turn += (1 if k % 2 == 0 else -1) * 0.3 * smoothstep((self.T - self.T[i]) / 3.0); last = i; k += 1
        self.turn = turn
        # El picado: después de la división, la bandada baja hacia el agua y se acerca (coreografía fija del film).
        self.swoop = np.exp(-((self.T - 19.5) / 2.6) ** 2) if env is not None else np.zeros(len(e))

    def at(self, name, t): return np.interp(t, self.T, getattr(self, name))


# ───────── la bandada ─────────
# Un estornino en coordenadas propias (x adelante, y a la derecha, z arriba), envergadura 0,436 m.
BODY = np.array([(0.105, 0, 0.004), (0.012, -0.026, 0), (0.012, 0.026, 0), (0.0, 0, 0.024), (0.006, 0, -0.024),
                 (-0.062, 0, 0.004), (-0.122, -0.028, 0.002), (-0.122, 0.028, 0.002)])
FACES = [(0, 3, 1), (0, 2, 3), (0, 4, 2), (0, 1, 4), (5, 1, 3), (5, 3, 2), (5, 2, 4), (5, 4, 1), (5, 6, 7),
         (8, 10, 11), (8, 11, 9), (10, 12, 11), (13, 15, 16), (13, 16, 14), (15, 17, 16)]
NV = 18

class Flock:
    """Cada pájaro tiene coordenadas (ángulo, radio) sobre una lámina y un espesor w. Los pájaros recorren la lámina
    (a ~7 m/s, así cada uno vuela de verdad hacia adelante) mientras la lámina se pliega, se tuerce, se hincha con la
    energía de la música, se divide en dos cuando la frase respira y gira con cada ataque: el giro arranca en una
    punta y recorre la bandada como una ola. Vista de canto, la lámina oscurece en bandas, como en una murmuración
    real. El rumbo sale de la velocidad, la inclinación (alabeo) de la aceleración lateral."""
    def __init__(self, n, drive, seed=7):
        r = np.random.default_rng(seed); self.n = n; self.D = drive
        self.th = r.uniform(0, 2 * np.pi, n)
        self.rho = np.sqrt(r.uniform(0, 1, n)) ** 1.1
        self.w = np.clip(r.normal(0, 1, n), -2.5, 2.5)
        self.om = 7.0 / (110 * SIZE * np.maximum(self.rho, 0.3))       # rad por unidad de recorrido
        self.jph = r.uniform(0, 2 * np.pi, (3, n)); self.jf = r.uniform(0.3, 0.9, (3, n))
        self.span = 0.436 * BIRDSCALE * r.uniform(0.9, 1.1, n)
        self.flap_f = r.uniform(6.0, 8.5, n); self.flap_ph = r.uniform(0, 2 * np.pi, n)
        self.gf = r.uniform(0.25, 0.55, n); self.gph = r.uniform(0, 2 * np.pi, n)   # ciclo aleteo / planeo
        # Campo de ondas: seis modos con dirección, largo (60–200 m), amplitud y velocidad propias.
        k = r.normal(0, 1, (6, 3)); k /= np.linalg.norm(k, axis=1)[:, None]
        self.kv = k * (2 * np.pi / r.uniform(60, 200, 6))[:, None]
        a = r.normal(0, 1, (6, 3)); a -= (a * k).sum(1)[:, None] * k; a /= np.linalg.norm(a, axis=1)[:, None]
        self.ka = a * r.uniform(3, 9, 6)[:, None]
        self.kw = r.uniform(0.2, 0.5, 6); self.kph = r.uniform(0, 2 * np.pi, 6)
        self.grp = np.where(r.uniform(0, 1, n) < 0.45, 1.0, -1.0)      # a qué grupo va cuando se divide
        # Rezagados: un 2 % vuela suelto alrededor de la bandada (a 8–45 m de la lámina); dan profundidad de cerca.
        sd = r.normal(0, 1, (n, 3)); sd /= np.linalg.norm(sd, axis=1)[:, None]
        self.stray = (sd * r.uniform(8, 45, n)[:, None]) * (r.uniform(0, 1, n) < 0.02)[:, None]
        self.sph = r.uniform(0, 2 * np.pi, n)

    def center(self, t):
        D = self.D; es = float(D.at("es", t)); sw = float(D.at("swoop", t))
        return np.array([45 + 25 * math.sin(0.21 * t) - 20 * sw, 260 + 15 * math.sin(0.13 * t + 1) - 35 * sw,
                         42 + 5 * math.sin(0.17 * t) + 8 * (es - 0.6) - 20 * sw])

    def phi(self, t):
        """Rumbo de la lámina entera (el de su centro, sin la ola de giro)."""
        D = self.D
        return 0.6 + 0.5 * math.sin(0.09 * float(D.at("tau", t))) + float(D.at("turn", t - 0.45))

    def normal(self, t):
        """Normal horizontal de la lámina: mirando a lo largo de ella se la ve de frente."""
        f = self.phi(t); return np.array([-math.sin(f), math.cos(f), 0.0])

    def pos(self, t):
        D = self.D
        E, Es, tau, sp, circ = (float(D.at(k, t)) for k in ("ef", "es", "tau", "sp", "circ"))
        S = SIZE * (0.84 + 0.32 * Es)                                   # se hincha con la música
        th = self.th + self.om * circ; rho = self.rho
        R = 1 + 0.16 * np.sin(2 * th + 0.31 * tau + 0.4) + 0.11 * np.sin(3 * th - 0.23 * tau + 1.3) + 0.07 * np.sin(5 * th + 0.41 * tau + 2.1)
        Lx = 110 * S * (1 + 0.15 * math.sin(0.17 * tau)); Lz = 42 * S * (1 - 0.2 * math.sin(0.17 * tau + 0.5))
        a = Lx * rho * R * np.cos(th); b = Lz * rho * R * np.sin(th)
        a0 = a / S; b0 = b / S
        thick = S * (1.8 + 2.4 * (1 - rho ** 2))
        y = S * (30 * np.sin(0.045 * a0 + 0.37 * tau) + 12 * np.sin(0.05 * b0 + 0.012 * a0 - 0.29 * tau + 1.0)
                 + 8 * np.sin(0.035 * a0 - 0.04 * b0 + 0.53 * tau + 2.0)) + self.w * thick
        psi = 0.5 * np.sin(0.008 * a0 + 0.25 * tau)                    # torsión a lo largo de la lámina
        y, b = y * np.cos(psi) - b * np.sin(psi), y * np.sin(psi) + b * np.cos(psi)
        # Giro: cada ataque gira la bandada; el giro llega antes a una punta que a la otra (ola de giro).
        delay = np.clip(0.45 * (1 + a0 / 110), 0, 1.1)
        phi = 0.6 + 0.5 * math.sin(0.09 * tau) + np.interp(t - delay, D.T, D.turn) + 0.5 * sp * self.grp
        x = a * np.cos(phi) - y * np.sin(phi); y = a * np.sin(phi) + y * np.cos(phi)
        p = np.stack([x, y, b], 1)
        ph = (p / S) @ self.kv.T + self.kw * tau + self.kph             # (n, 6)
        p += S * (0.6 + 0.8 * E) * (np.sin(ph) @ self.ka)
        p += 0.8 * np.sin(self.jf.T * t + self.jph.T)                  # cada pájaro se mueve un poco por su cuenta
        p += self.stray * (0.75 + 0.25 * np.sin(0.3 * t + self.sph))[:, None]
        if sp > 1e-4:     # división: cada grupo es una copia más chica de la bandada que se aparta hacia su lado
            p = p * (1 - 0.38 * sp) + sp * S * self.grp[:, None] * np.array([62.0, -14.0, 9.0])
        p += self.center(t)
        # Piso blando: nadie baja de 2,5 m; cerca del agua la bandada se aplasta en vez de hundirse.
        p[:, 2] = 2.5 + 3.0 * np.logaddexp(0.0, (p[:, 2] - 2.5) / 3.0)
        return p

    def local(self, t):
        """Vértices de cada pájaro en sus coordenadas (n, 18, 3), con el aleteo del instante t: ala interna y
        punta con retraso (el ala se dobla), plegada en la subida; tramos planeando con las alas abiertas."""
        n = self.n
        gate = smoothstep((np.sin(2 * np.pi * self.gf * t + self.gph) + 0.35) / 0.7)
        A = 0.10 + 0.58 * gate
        ph = 2 * np.pi * self.flap_f * t + self.flap_ph
        up = np.clip(np.cos(ph), 0, 1) * gate                          # subida del ala
        a1 = 0.10 + A * np.sin(ph)
        a2 = a1 + 0.45 * A * np.sin(ph - 1.0) - 0.04
        Li, Lo = 0.085, 0.115 * (1 - 0.32 * up)
        sweep = 0.075 + 0.04 * up
        V = np.empty((n, NV, 3)); V[:, :8] = BODY
        for j, sg in ((8, -1.0), (13, 1.0)):
            ey = sg * (0.018 + Li * np.cos(a1)); ez = 0.010 + Li * np.sin(a1)
            V[:, j] = (0.035, sg * 0.018, 0.010); V[:, j + 1] = (-0.032, sg * 0.018, 0.010)
            V[:, j + 2, 0] = 0.030; V[:, j + 2, 1] = ey; V[:, j + 2, 2] = ez
            V[:, j + 3, 0] = -0.050; V[:, j + 3, 1] = ey; V[:, j + 3, 2] = ez
            V[:, j + 4, 0] = 0.030 - sweep; V[:, j + 4, 1] = ey + sg * Lo * np.cos(a2); V[:, j + 4, 2] = ez + Lo * np.sin(a2)
        return V * (self.span / 0.436)[:, None, None]

    @staticmethod
    def avoid(p, cam, r0):
        """Los pájaros esquivan la cámara: nadie se acerca a menos de r0 (empuje suave, sin saltos)."""
        d = p - cam; r = np.maximum(np.linalg.norm(d, axis=1), 1e-6)
        return cam + d * (np.sqrt(r * r + r0 * r0) / r)[:, None]

    def geometry(self, t, cam=None, r0=2.5, h=1 / 48):
        """Vértices (n·18, 3) en el instante t y su velocidad (m/s), para el desenfoque de movimiento."""
        P = [self.pos(t + dt) for dt in (-h, 0.0, h)]
        if cam is not None: P = [self.avoid(p, np.asarray(cam, float), r0) for p in P]
        pm, p0, pp = P
        v = (pp - pm) / (2 * h); acc = (pp - 2 * p0 + pm) / (h * h)
        f = v / np.maximum(np.linalg.norm(v, axis=1), 1e-6)[:, None]
        s = np.cross(f, np.array([0.0, 0.0, 1.0])); s /= np.maximum(np.linalg.norm(s, axis=1), 1e-6)[:, None]
        u = np.cross(s, f)
        bank = np.clip(np.arctan2((acc * s).sum(1), 9.8), -1.0, 1.0)[:, None]
        s, u = s * np.cos(bank) - u * np.sin(bank), u * np.cos(bank) + s * np.sin(bank)
        def world(p, L):
            return p[:, None] + L[..., 0:1] * f[:, None] + L[..., 1:2] * s[:, None] + L[..., 2:3] * u[:, None]
        G0 = world(p0, self.local(t))
        vel = (world(pp, self.local(t + h)) - world(pm, self.local(t - h))) / (2 * h)
        return G0.reshape(-1, 3), vel.reshape(-1, 3), p0


# ───────── nodos ─────────
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
    def lerp(self, a, b, k): return self.vec("ADD", a, self.vec("SCALE", self.vec("SUBTRACT", b, a), scale=k))
    def smooth(self, v, a, b):
        n = self.N.new("ShaderNodeMapRange"); n.interpolation_type = "SMOOTHSTEP"; n.clamp = True
        self._in(n.inputs["Value"], v); n.inputs["From Min"].default_value = a; n.inputs["From Max"].default_value = b
        return n.outputs["Result"]
    def xyz(self, x, y, z):
        n = self.N.new("ShaderNodeCombineXYZ")
        for i, v in enumerate((x, y, z)): self._in(n.inputs[i], v)
        return n.outputs[0]
    def sep(self, v):
        n = self.N.new("ShaderNodeSeparateXYZ"); self._in(n.inputs[0], v); return n.outputs
    def ramp(self, fac, stops):
        n = self.N.new("ShaderNodeValToRGB"); cr = n.color_ramp; cr.interpolation = "B_SPLINE"
        cr.elements[0].position, cr.elements[0].color = stops[0][0], (*stops[0][1], 1)
        cr.elements[1].position, cr.elements[1].color = stops[-1][0], (*stops[-1][1], 1)
        for p, c in stops[1:-1]: cr.elements.new(p).color = (*c, 1)
        self._in(n.inputs["Fac"], fac); return n.outputs["Color"]
    def noise(self, v, scale, detail=6.0, rough=0.55, distortion=0.0):
        n = self.N.new("ShaderNodeTexNoise"); self._in(n.inputs["Vector"], v)
        n.inputs["Scale"].default_value = scale; n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        if "Distortion" in n.inputs: n.inputs["Distortion"].default_value = distortion
        return n.outputs["Fac"]


# ───────── cielo ─────────
# Degradé por altura (en raíz de la elevación, para tener detalle cerca del horizonte), del lado del sol y del lado
# contrario: del lado del sol, oro sobre el horizonte, rosa y malva hacia arriba; del lado contrario, la sombra de la
# Tierra gris azulada con el cinturón de Venus rosado encima; arriba, azul de la hora azul.
SKY_SUN = [(0.00, (1.00, 0.36, 0.08)), (0.10, (0.62, 0.17, 0.05)), (0.20, (0.30, 0.08, 0.06)), (0.32, (0.11, 0.045, 0.07)),
           (0.50, (0.028, 0.030, 0.065)), (0.75, (0.008, 0.014, 0.040)), (1.00, (0.003, 0.006, 0.020))]
SKY_ANTI = [(0.00, (0.040, 0.036, 0.065)), (0.12, (0.055, 0.042, 0.075)), (0.25, (0.090, 0.048, 0.072)),
            (0.40, (0.036, 0.034, 0.068)), (0.60, (0.012, 0.018, 0.045)), (1.00, (0.003, 0.006, 0.020))]

def atmosphere(X, d, clouds=True, disk=True, glossy=None):
    """Color del cielo en la dirección d (normalizada). Para la tierra lejana se usa sin nubes ni sol, como bruma."""
    x, y, z = X.sep(d)
    zc = X.math("MAXIMUM", z, 0.0)
    fac = X.math("POWER", X.math("ADD", zc, 1e-5), 0.5)
    hs = Vector((SUN.x, SUN.y, 0)).normalized()
    hl = X.math("ADD", X.math("SQRT", X.math("ADD", X.math("MULTIPLY", x, x), X.math("MULTIPLY", y, y))), 1e-4)
    az = X.math("DIVIDE", X.math("ADD", X.math("MULTIPLY", x, hs.x), X.math("MULTIPLY", y, hs.y)), hl)
    w = X.math("POWER", X.math("MULTIPLY_ADD", az, 0.5, 0.5), 2.2)     # 1 mirando al sol, 0 de espaldas
    base = X.lerp(X.ramp(fac, SKY_ANTI), X.ramp(fac, SKY_SUN), w)
    dot = X.vec("DOT_PRODUCT", d, tuple(SUN)); cl = X.math("MAXIMUM", dot, 0.0)
    p6, p40, p60 = (X.math("POWER", cl, e) for e in (6.0, 40.0, 60.0))
    low = X.math("SUBTRACT", 1.0, X.smooth(z, 0.0, 0.3))
    halo = X.vec("ADD", X.vec("SCALE", (1.0, 0.40, 0.12), scale=X.math("MULTIPLY", p6, X.math("MULTIPLY", low, 0.25))),
                 X.vec("SCALE", (1.0, 0.50, 0.20), scale=X.math("MULTIPLY", p60, 0.6)))
    # Bruma sobre el horizonte: la franja más baja se aclara.
    haze = X.math("MULTIPLY", X.math("SUBTRACT", 1.0, X.smooth(z, 0.0, 0.02)), 0.2)
    c = X.vec("ADD", X.vec("ADD", base, halo), X.vec("SCALE", base, scale=haze))
    if not clouds: return c
    damp = X.math("MULTIPLY_ADD", glossy, -0.45, 1.0) if glossy is not None else 1.0
    p900 = X.math("POWER", cl, 900.0)
    c = X.vec("ADD", c, X.vec("SCALE", (1.0, 0.8, 0.55), scale=X.math("MULTIPLY", p900, X.math("MULTIPLY", damp, 1.2))))
    if glossy is not None:            # sólo en los reflejos: un halo más ancho que dibuja en el agua el camino al sol
        p200 = X.math("POWER", cl, 200.0)
        c = X.vec("ADD", c, X.vec("SCALE", CORAL, scale=X.math("MULTIPLY", X.math("MULTIPLY", p200, glossy), 0.45)))
    # Nubes altas (altocúmulos): ruido con distorsión proyectado sobre un plano, estirado por el viento a lo ancho.
    zz = X.math("ADD", z, 0.02)
    qx, qy = X.math("DIVIDE", x, zz), X.math("DIVIDE", y, zz)
    n1 = X.noise(X.xyz(X.math("MULTIPLY", qx, 0.5), qy, 0.0), 0.55, 9.0, 0.62, 0.35)
    n2 = X.noise(X.xyz(X.math("MULTIPLY", qx, 0.3), qy, 7.0), 0.11, 3.0, 0.5)
    raw = X.math("MULTIPLY", X.smooth(n1, 0.47, 0.70), X.smooth(n2, 0.45, 0.68))
    band = X.math("MULTIPLY", X.smooth(z, 0.03, 0.10), X.math("SUBTRACT", 1.0, X.smooth(z, 0.35, 0.70)))
    dh = X.math("MULTIPLY", X.math("MULTIPLY", raw, band), 0.9)
    # Panza de las nubes iluminada desde abajo: dorada junto al sol, rosa y malva lejos; los bordes finos brillan.
    warm = X.vec("SCALE", (1.0, 0.42, 0.15), scale=X.math("ADD", X.math("MULTIPLY_ADD", p6, 0.8, 0.05), X.math("MULTIPLY", p60, 1.2)))
    far = X.lerp((0.028, 0.022, 0.040), (0.13, 0.05, 0.07), X.smooth(fac, 0.15, 0.55))
    hc = X.lerp(far, warm, X.math("POWER", w, 1.3))
    hc = X.vec("SCALE", hc, scale=X.math("MULTIPLY_ADD", raw, -0.5, 1.25))
    hc = X.vec("ADD", hc, X.vec("SCALE", (1.0, 0.6, 0.3), scale=X.math("MULTIPLY", p60, X.math("MULTIPLY", X.math("SUBTRACT", 1.0, raw), 2.0))))
    c = X.lerp(c, hc, dh)
    # Estratos bajos: bandas oscuras sobre el horizonte con el borde encendido cerca del sol.
    n3 = X.noise(X.xyz(X.math("MULTIPLY", qx, 0.35), X.math("ADD", X.math("MULTIPLY", qy, 0.9), 3.7), 1.3), 0.9, 6.0, 0.55, 0.2)
    dl = X.math("MULTIPLY", X.smooth(n3, 0.54, 0.68),
                X.math("MULTIPLY", X.smooth(z, 0.0025, 0.010), X.math("SUBTRACT", 1.0, X.smooth(z, 0.022, 0.05))))
    lc = X.vec("ADD", (0.030, 0.016, 0.024), X.vec("SCALE", (1.0, 0.40, 0.14), scale=X.math("MULTIPLY_ADD", p40, 0.5, X.math("MULTIPLY", p6, 0.08))))
    c = X.lerp(c, lc, X.math("MULTIPLY", dl, 0.55))
    if disk:                           # disco del sol (radio aparente agrandado a 0,6°), tapado por las nubes
        dk = X.smooth(dot, math.cos(math.radians(0.64)), math.cos(math.radians(0.56)))
        occl = X.math("MULTIPLY", X.math("SUBTRACT", 1.0, dl), X.math("SUBTRACT", 1.0, X.math("MULTIPLY", dh, 0.8)))
        c = X.vec("ADD", c, X.vec("SCALE", (2.2, 0.75, 0.20), scale=X.math("MULTIPLY", X.math("MULTIPLY", dk, damp), occl)))
    return c

def sky():
    sc = bpy.context.scene
    w = bpy.data.worlds.new("W"); sc.world = w; w.use_nodes = True
    nt = w.node_tree; X = Nodes(nt); bg = nt.nodes["Background"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    d = X.vec("NORMALIZE", tc.outputs["Generated"])            # en el mundo, Generated = dirección del rayo
    lp = nt.nodes.new("ShaderNodeLightPath")
    nt.links.new(atmosphere(X, d, glossy=lp.outputs["Is Glossy Ray"]), bg.inputs["Color"]); bg.inputs["Strength"].default_value = 1.0
    if hasattr(w, "cycles"): w.cycles.sampling_method = "MANUAL"; w.cycles.sample_map_resolution = 2048
    # La luz del sol bajo: viene de atrás de la bandada y le recorta un filo cálido.
    l = bpy.data.lights.new("Sun", "SUN"); l.energy = 1.4; l.color = (1.0, 0.50, 0.28); l.angle = math.radians(1.2)
    o = link(bpy.data.objects.new("Sun", l)); o.rotation_euler = (-SUN).to_track_quat("-Z", "Y").to_euler()
    o.visible_glossy = False          # a pleno, su reflejo en el agua era una barra blanca


# ───────── materiales y objetos ─────────
def link(o): bpy.context.scene.collection.objects.link(o); return o

def mat(name, col, rough, metal=0.0, spec=0.5, ior=1.45):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal; b.inputs["IOR"].default_value = ior
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    return m

def haze_mat(name, k, col=(0.004, 0.004, 0.005), tint=(1.0, 1.0, 1.0)):
    """Tierra lejana con perspectiva aérea: además de su color oscuro emite el color del cielo en esa dirección
    (sin nubes ni sol) por k; cuanto más lejos, más k y más se funde con el horizonte."""
    m = mat(name, col, 0.95, spec=0.05); nt = m.node_tree; X = Nodes(nt); b = nt.nodes["Principled BSDF"]
    g = nt.nodes.new("ShaderNodeNewGeometry")
    d = X.vec("NORMALIZE", X.vec("SCALE", g.outputs["Incoming"], scale=-1.0))
    x, y, z = X.sep(d)
    d = X.vec("NORMALIZE", X.xyz(x, y, X.math("MAXIMUM", z, 0.006)))
    X._in(b.inputs["Emission Color"], X.vec("MULTIPLY", atmosphere(X, d, clouds=False, disk=False), tint))
    b.inputs["Emission Strength"].default_value = k
    return m

def water_material():
    """Agua quieta casi espejo: ondas finas estiradas a lo ancho, con parches de calma y de brisa que rompen el
    reflejo en franjas (como un lago al atardecer)."""
    m = mat("Water", (0.002, 0.004, 0.006), 0.03, ior=1.33)
    nt = m.node_tree; X = Nodes(nt); b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); ob = tc.outputs["Object"]
    ox, oy, _ = X.sep(ob)
    fine = X.noise(X.xyz(X.math("MULTIPLY", ox, 0.5), X.math("MULTIPLY", oy, 3.2), 0.0), 2.4, 8.0, 0.6, 0.4)
    swell = X.noise(X.xyz(X.math("MULTIPLY", ox, 0.05), X.math("MULTIPLY", oy, 0.35), 2.0), 1.0, 3.0, 0.5)
    # Parches de brisa redondeados (~100 m) y de borde suave: estirados, al ras del agua se veían como una raya.
    patch = X.noise(X.xyz(X.math("MULTIPLY", ox, 0.010), X.math("MULTIPLY", oy, 0.012), 5.0), 1.0, 2.0, 0.5)
    calm = X.math("MULTIPLY_ADD", X.smooth(patch, 0.30, 0.70), 0.65, 0.35)
    hgt = X.math("MULTIPLY", X.math("MULTIPLY_ADD", fine, 0.6, X.math("MULTIPLY", swell, 0.4)), calm)
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.30; bump.inputs["Distance"].default_value = 0.02
    X._in(bump.inputs["Height"], hgt); nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

def water():
    bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=25000)
    me = bpy.data.meshes.new("Water"); bm.to_mesh(me); bm.free(); me.materials.append(water_material())
    o = link(bpy.data.objects.new("Water", me)); o.location = (0, 8000, 0)

def strip(name, xs, ys, h, base, m):
    """Silueta de tierra: una tira vertical que sigue (xs, ys) con alto h."""
    bm = bmesh.new(); top = []; bot = []
    for x, y, hh in zip(xs, ys, h):
        bot.append(bm.verts.new((x, y, base))); top.append(bm.verts.new((x, y, hh)))
    for i in range(len(xs) - 1): bm.faces.new((bot[i], bot[i + 1], top[i + 1], top[i]))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); me.materials.append(m)
    return link(bpy.data.objects.new(name, me))

def land():
    """Tres planos de tierra para dar profundidad: lomas lejanas en la bruma, la línea de árboles (su silueta sigue
    una forma de onda grabada; con el reflejo queda simétrica, como la onda del plugin) y una punta de costa cercana
    a la derecha, casi negra."""
    rng = np.random.default_rng(3)
    # Lomas a 5–6 km: suaves, con un valle donde se pone el sol.
    xs = np.arange(-30000.0, 30000.0, 40.0)       # largas: el tele de "close" mira 55° a la izquierda
    sun_x = 5600 * math.tan(SUN_AZ)
    h = 40 + 70 * np.exp(-((xs - 2400) / 1800) ** 2) + 55 * np.exp(-((xs + 3600) / 1500) ** 2) + 30 * np.sin(xs / 900.0) ** 2
    h *= 1 - 0.75 * np.exp(-((xs - sun_x) / 1300) ** 2)
    h *= 0.85 + 0.3 * np.convolve(rng.uniform(0, 1, xs.size), np.ones(5) / 5, "same")   # bosque sobre las lomas
    strip("Hills", xs, 5600 + 500 * np.sin(xs / 3100.0), h, -20.0, haze_mat("Hills", 0.26, tint=(0.75, 0.82, 1.25)))
    # Línea de árboles a ~2,2 km.
    xs = np.arange(-12000.0, 12000.0, 3.5)
    env = 6 + 30 * np.exp(-((xs + 900) / 700) ** 2) + 44 * np.exp(-((xs - 600) / 520) ** 2) \
        + 24 * np.exp(-((xs - 2100) / 900) ** 2) + 18 * np.exp(-((xs + 2600) / 1100) ** 2)
    osc = 0.55 + 0.45 * np.abs(np.sin(xs * 0.031 + 2.2 * np.sin(xs * 0.0051)) * np.sin(xs * 0.0083 + 1.0))
    crown = np.convolve(rng.uniform(0, 1, xs.size), np.ones(4) / 4, "same")   # copas irregulares de 10–20 m
    h = env * osc * (0.45 + 0.75 * crown) * (0.75 + 0.25 * rng.uniform(0, 1, xs.size) ** 0.5) + 2.5
    strip("Trees", xs, 2200 + 180 * np.sin(xs / 1300.0), h, -3.0, haze_mat("Trees", 0.10))
    # Punta de costa a la derecha: entra en diagonal desde 700 m hasta 1,6 km, con árboles de copa irregular.
    xs = np.arange(160.0, 2600.0, 1.6)
    yy = 700 + 0.42 * (xs - 160)
    crown = np.convolve(rng.uniform(0, 1, xs.size), np.ones(9) / 9, "same") * rng.uniform(0.75, 1.0, xs.size)
    h = (3 + 16 * smoothstep((xs - 160) / 260)) * (0.35 + 1.1 * crown)
    strip("Shore", xs, yy, h, -2.0, haze_mat("Shore", 0.025))

def pilings():
    """Restos de un muelle viejo a la derecha: postes de madera que salen del agua, alguno inclinado."""
    rng = np.random.default_rng(11)
    m = mat("Wood", (0.040, 0.030, 0.022), 0.7, spec=0.3)
    for i in range(7):
        h = rng.uniform(0.9, 1.9) if i not in (3,) else 0.55
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, segments=10, radius1=0.14, radius2=0.12, depth=h + 2.0)
        me = bpy.data.meshes.new(f"Post{i}"); bm.to_mesh(me); bm.free(); me.materials.append(m)
        for p in me.polygons: p.use_smooth = True
        o = link(bpy.data.objects.new(f"Post{i}", me))
        o.location = (11.0 + 2.6 * i + rng.uniform(-0.4, 0.4), 44 + 4.2 * i + rng.uniform(-1, 1), (h + 2.0) / 2 - 2.0)
        o.rotation_euler = (rng.uniform(-0.06, 0.06), rng.uniform(-0.08, 0.08) + (0.18 if i == 5 else 0), 0)

def reeds(cx, cy, n, spread, hmax, seed):
    """Juncos: hojas finas curvas en un manchón; de cerca, siluetas negras sobre el agua encendida."""
    rng = np.random.default_rng(seed); bm = bmesh.new()
    for _ in range(n):
        x0, y0 = cx + rng.normal(0, spread), cy + rng.normal(0, spread * 0.5)
        hh = hmax * rng.uniform(0.45, 1.0); lean = rng.uniform(0, 2 * math.pi); bend = rng.uniform(0.05, 0.35)
        wd = rng.uniform(0.008, 0.016); seg = 5; prev = None
        for k in range(seg + 1):
            q = k / seg; off = bend * hh * q * q
            px, py, pz = x0 + off * math.cos(lean), y0 + off * math.sin(lean), -0.1 + hh * q
            ww = wd * (1 - q) + 0.001
            vs = (bm.verts.new((px - ww * math.sin(lean), py + ww * math.cos(lean), pz)),
                  bm.verts.new((px + ww * math.sin(lean), py - ww * math.cos(lean), pz)))
            if prev: bm.faces.new((prev[0], prev[1], vs[1], vs[0]))
            prev = vs
    me = bpy.data.meshes.new("Reeds"); bm.to_mesh(me); bm.free()
    me.materials.append(mat("Reed", (0.020, 0.020, 0.012), 0.65, spec=0.3))
    return link(bpy.data.objects.new("Reeds", me))

def flock_object(fl):
    n = fl.n; me = bpy.data.meshes.new("Flock")
    base = (np.arange(n) * NV)[:, None, None]
    tris = (np.array(FACES)[None] + base).reshape(-1, 3)
    try:
        me.vertices.add(n * NV); me.loops.add(tris.size); me.polygons.add(len(tris))
        me.loops.foreach_set("vertex_index", tris.ravel().astype(np.int32))
        me.polygons.foreach_set("loop_start", (np.arange(len(tris)) * 3).astype(np.int32))
        me.update()
    except Exception:
        me = bpy.data.meshes.new("Flock"); me.from_pydata(np.zeros((n * NV, 3)).tolist(), [], tris.tolist())
    me.attributes.new("velocity", "FLOAT_VECTOR", "POINT")
    # Estornino: casi negro, con un brillo satinado que agarra el filo del sol.
    m = mat("Bird", (0.016, 0.015, 0.017), 0.42, spec=0.45)
    me.materials.append(m)
    o = link(bpy.data.objects.new("Flock", me))
    return o

def set_flock(o, fl, t, cam=None):
    G, V, _ = fl.geometry(t, cam)
    me = o.data
    me.vertices.foreach_set("co", G.astype(np.float32).ravel())
    me.attributes["velocity"].data.foreach_set("vector", V.astype(np.float32).ravel())
    me.update()


# ───────── escena ─────────
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("CUDA", "METAL", "HIP"):
        try:
            prefs.compute_device_type = kind; prefs.get_devices()
        except TypeError:
            continue
        if any(d.type == kind for d in prefs.devices):
            for d in prefs.devices: d.use = d.type == kind
            break
    sc.cycles.device = "GPU"
    sc.cycles.max_bounces = 4; sc.cycles.diffuse_bounces = 2; sc.cycles.glossy_bounces = 2
    sc.cycles.transmission_bounces = 2; sc.cycles.volume_bounces = 0
    sc.cycles.filter_width = 1.2                       # pájaros chicos nítidos
    sc.render.use_persistent_data = True
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    return sc

def build(drive, with_reeds=False):
    sc = reset(); sky(); water(); land(); pilings()
    if with_reeds: reeds(-1.6, 30.0, 260, 0.9, 1.5, 5); reeds(21.0, 64.0, 120, 1.2, 1.7, 6)
    fl = Flock(NB, drive); o = flock_object(fl)
    return sc, fl, o

def look_cam(sc, loc, target, lens, res, fstop=None):
    cd = bpy.data.cameras.new("Cam"); o = link(bpy.data.objects.new("Cam", cd)); sc.camera = o
    cd.lens = lens; cd.sensor_width = 36; cd.clip_start = 0.05; cd.clip_end = 30000
    o.location = Vector(loc); t = Vector(target)
    o.rotation_euler = (t - o.location).to_track_quat("-Z", "Y").to_euler()
    cd.dof.use_dof = fstop is not None
    if fstop: cd.dof.aperture_fstop = fstop; cd.dof.focus_distance = (t - o.location).length
    sc.render.resolution_x, sc.render.resolution_y = res; sc.render.resolution_percentage = int(arg("--pct", "100"))
    return o

def drive_from(path):
    if not path: return Drive(None)
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    return Drive(fk.envelope(path))


# ───────── fotos de la web ─────────
def mode_hero():
    """hero: la bahía entera, la bandada a la derecha y el sol a la izquierda, los postes del muelle en el agua.
    flock: detalle de la bandada con un tele, alineado con el sol para que los pájaros se recorten en el resplandor."""
    drive = drive_from(arg("--audio"))
    for nm in (arg("--shots") or "hero,flock").split(","):
        t = float(arg("--t", {"hero": "6.0", "flock": "11.0"}[nm]))
        sc, fl, o = build(drive)
        sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = arg("--denoise", "0") == "1"
        sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGB"
        c = fl.center(t)
        if nm == "hero":
            cam = look_cam(sc, (0, -6, 1.7), (6, 130, 1.7 + 136 * math.tan(math.radians(6.2))), 30, (2400, 1100), 11.0)
        else:             # en línea con el sol: el sol queda abajo, sobre los árboles, y la bandada encima
            loc = (c[0] + 0.156 * 150, c[1] - 0.988 * 150, 3.0)
            cam = look_cam(sc, loc, (c[0], c[1], 3.0 + 150 * math.tan(math.radians(10.5))), 45, (1600, 1200), 8.0)
        set_flock(o, fl, t, cam.location)
        sc.render.filepath = os.path.join(OUT, f"murmur_{nm}.png")
        bpy.ops.render.render(write_still=True)


# ───────── film de la web (filmkit, --mode film2) ─────────
FILM_SHOTS = [("open", 0.0, 4.0), ("close", 4.0, 4.0), ("low", 8.0, 4.0), ("split", 12.0, 5.0), ("swell", 17.0, 4.0),
              ("end", 21.0, 4.0)]

def shot_paths(fk, fl):
    """Recorridos de cámara de cada toma (fk.Path) y a qué distancia enfoca. Los que acompañan a la bandada se
    arman sobre su centro al principio y al final de la toma."""
    def up(y, deg, z): return z + y * math.tan(math.radians(deg))
    C = lambda t: Vector(fl.center(t).tolist())
    def front(t, dist, dz, side=0.0):
        """Punto frente a la lámina (del lado de la cámara base) a dist m del centro, un poco corrido de costado."""
        n = fl.normal(t); n = -n if n[1] > 0 else n                 # del lado de la orilla
        sv = np.array([n[1], -n[0], 0.0])
        return Vector((fl.center(t) + dist * n + side * sv + np.array([0, 0, dz])).tolist())
    c4, c8 = C(4.0), C(8.0)
    c12, c17 = C(12.0), C(17.0)
    c18, c21 = C(17.5), C(21.0)
    return {
        "open":  fk.Path([(0, (0.0, -16, 1.75), (7, 130, up(146, 5.4, 1.75)), 35, 9.0),
                          (1, (0.6, -4, 1.65), (8, 130, up(134, 5.4, 1.65)), 35, 9.0)]),
        "close": fk.Path([(0, front(4.0, 150, -10, -10), c4 + Vector((0, 0, -10 + 150 * math.tan(math.radians(2.0)))), 85, 4.0),
                          (0.5, front(6.0, 145, -10, -4), C(6.0) + Vector((0, 0, -10 + 145 * math.tan(math.radians(2.0)))), 85, 4.0),
                          (1, front(8.0, 140, -10, 2), c8 + Vector((0, 0, -10 + 140 * math.tan(math.radians(2.0)))), 85, 4.0)],
                         easing="linear"),
        "low":   fk.Path([(0, (3.2, 23.5, 0.30), (-8, 330, up(306, 4.6, 0.30)), 24, 5.6),
                          (1, (6.0, 25.0, 0.50), (-2, 330, up(305, 4.6, 0.50)), 24, 5.6)]),
        "split": fk.Path([(0, (-36, -95, 2.2), c12 + Vector((0, 0, 4)), 45, 11.0),
                          (1, (-31, -66, 2.4), c17 + Vector((0, 0, 4)), 45, 11.0)]),
        "swell": fk.Path([(0, Vector((c18.x - 34, c18.y - 135, 1.2)), c18 + Vector((0, 0, 2)), 28, 5.6),
                          (1, Vector((c21.x - 28, c21.y - 125, 1.6)), c21 + Vector((4, 0, 4)), 28, 5.6)]),
        "end":   fk.Path([(0, (2.0, 6, 1.5), (5, 200, up(194, 6.0, 1.5)), 30, 10.0),
                          (1, (0.0, -24, 2.6), (5, 200, up(224, 6.0, 2.6)), 30, 10.0)], easing="out"),
    }

def film2():
    """Film de la web (25 s, 24 cuadros/s) sobre "shimmer-choir" (el coro con shimmer de las muestras): tres frases
    largas; la primera crece de 0 a 14 s, respira de 14 a 17 s y la segunda entra a los 17,5 s. La bandada lee esa
    envolvente (Drive): se hincha y fluye más rápido cuando el coro sube, gira en cada ataque (la ola de giro recorre
    la bandada) y en la respiración se parte en dos; al volver la frase se junta y baja en picada sobre el agua.

    Guion (tiempo del film = tiempo del audio):
      open   0,0–4,0  35 mm a 1,7 m sobre el agua, empuje lento hacia adelante. La bandada empieza en dos grupos
                      que se juntan mientras entra el coro, y gira en los ataques de 2,1 y 3,4 s; los postes del
                      muelle a la derecha. El tercio inferior izquierdo es agua y reflejo: ahí va el título.
      close  4,0–8,0  Tele de 85 mm a ~150 m, de frente a la lámina y a la altura de la bandada, que la acompaña:
                      cada pájaro se lee (aleteo, alabeo, desenfoque de movimiento) contra el resplandor; los
                      rezagados pasan más cerca y más grandes. Cámara en mano mínima (giro de 0,05°).
      low    8,0–12,0 Al ras del agua (30 cm), 24 mm: juncos negros en primer plano, los postes, la bandada y su
                      reflejo; la cámara se desliza y sube apenas; giros en los ataques de 8,2 y 10 s.
      split 12,0–17,0 45 mm desde la orilla, empuje lento: la frase se apaga y la bandada se parte en dos grupos
                      que se apartan, cada uno con su forma.
      swell 17,0–21,0 Entra la segunda frase: los dos grupos se juntan y la bandada baja en picada hacia el agua;
                      28 mm desde abajo, cámara en mano: los pájaros pasan cerca, grandes y movidos.
      end   21,0–25,0 (fundido) 30 mm que se aleja despacio: la bahía, los postes y la bandada hacia el sol.
                      film.py la oscurece y pone el cierre en el centro.
    """
    sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "..", "shared", "film"))
    import filmkit as fk
    only = arg("--shot"); quality = arg("--quality", "final"); step = int(arg("--step", "1"))
    drive = Drive(fk.envelope(arg("--audio")))
    for (name, t0, dur) in FILM_SHOTS:
        if only and name not in only.split(","): continue
        sc, fl, o = build(drive, with_reeds=name == "low")      # los juncos sólo en la toma al ras del agua
        fk.setup(sc, quality)
        sc.cycles.use_denoising = arg("--denoise", "0") == "1"      # el denoiser empasta los pájaros chicos
        sc.cycles.filter_width = 1.2
        cam = fk.camera(sc); cam.data.clip_start = 0.05; cam.data.clip_end = 30000
        path = shot_paths(fk, fl)[name]
        frames = int(round(dur * FPS))
        shake = {"close": 0.0009, "swell": 0.0012}.get(name, 0.0)      # rad

        def pose(f, t, u, path=path, t0=t0, name=name):
            T = t0 + t
            focus = None
            if name in ("close", "swell"): focus = Vector(fl.center(T).tolist())
            path.apply(cam, u, focus=focus, t=T)
            if shake:                        # cámara en mano: un giro mínimo (en un tele, lo que se nota es el ángulo)
                cam.rotation_euler.x += shake * fk.noise1(T, 7, 0.45); cam.rotation_euler.z += shake * fk.noise1(T, 8, 0.45)
            return []
        render_frames(sc, name, frames, pose, OUT, lambda f, t0=t0: set_flock(o, fl, t0 + (f - 1) / FPS, cam.matrix_world.translation.copy()),
                      fk, step=step)

def render_frames(sc, name, frames, pose, out, per_frame, fk, step=1):
    """Como fk.render_shot, pero cuadro por cuadro: la bandada es una malla que se recalcula en cada cuadro (con su
    atributo de velocidad para el desenfoque de movimiento), y la cámara va con claves para que se mueva de verdad."""
    os.makedirs(out, exist_ok=True)
    for ob in sc.objects: ob.animation_data_clear()
    for f in range(1, frames + 1):
        sc.frame_set(f)
        t, u = (f - 1) / FPS, (f - 1) / max(1, frames - 1)
        moved = pose(f, t, u) or []
        for ob in set([sc.camera] + list(moved)): fk._key(ob)
    for ob in sc.objects:
        ad = ob.animation_data
        if ad and ad.action:
            for fc in fk._fcurves(ad.action):
                for kp in fc.keyframe_points: kp.interpolation = "LINEAR"
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 94
    pick = [int(x) for x in arg("--frames").split(",")] if arg("--frames") else range(1, frames + 1, step)
    for f in pick:
        sc.frame_set(f)
        per_frame(f)
        sc.render.filepath = os.path.join(out, f"{name}_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)


def legacy():
    """Los modos de la escena anterior (murmur_v1.py, sin cambios): --mode film (las cuatro tomas de 9 s del film
    viejo) y --mode hero-v1 (sus fotos). Se conservan para poder comparar lo viejo con lo nuevo."""
    import runpy
    i = sys.argv.index("--mode") + 1
    if sys.argv[i] == "hero-v1": sys.argv[i] = "hero"
    runpy.run_path(os.path.join(HERE, "murmur_v1.py"), run_name="__main__")

if bpy is not None and __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    {"hero": mode_hero, "film2": film2, "film": legacy, "hero-v1": legacy}[arg("--mode", "hero")]()
