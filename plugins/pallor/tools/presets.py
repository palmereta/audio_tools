#!/usr/bin/env python3
"""Presets de fábrica y biblioteca de patrones de Pallor.

Escribe assets/presets/pNN_nombre.json (kit + efectos + patrones + cadena de canción) y
assets/patterns/library.json (juegos A / B / Fill por género). Todos los patrones son originales: arquetipos de cada
género (docs/research.md §1.2), ninguno transcribe un tema.

Notación de un carácter por paso (los espacios y las barras se ignoran):
    .  apagado          x  normal (100)      X  acento (127)      o  suave (72)      g  fantasma (40)
    r  ratchet ×2       R  ratchet ×3        t  ratchet ×4        f  flam (20 ms)    p  50 % de probabilidad
    q  25 %             F  sólo en el fill   N  nunca en el fill  2  la 2.ª de cada 2 vueltas  4  la última de 4
    <  corrido −20 %    >  corrido +20 %
Voces: bd sd cp rs ch oh cy rd t1 t2 t3 t4 cb cl tb fx.

    python3 plugins/pallor/tools/presets.py
"""
import copy, json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "presets")
LIB = os.path.join(HERE, "..", "assets", "patterns", "library.json")
VOICES = ["bd", "sd", "cp", "rs", "ch", "oh", "cy", "rd", "t1", "t2", "t3", "t4", "cb", "cl", "tb", "fx"]

CODES = {
    "x": "100", "X": "127a", "o": "72", "g": "40", "r": "100r2", "R": "100r3", "t": "90r4", "f": "100f20",
    "p": "100p50", "q": "90p25", "F": "100c1", "N": "100c2", "2": "100c4", "4": "100c6", "<": "100n-20", ">": "100n20",
}


def track(s):
    s = re.sub(r"[\s|]", "", s)
    out = []
    for ch in s:
        if ch == ".":
            out.append(".")
        elif ch in CODES:
            out.append(CODES[ch])
        else:
            raise ValueError("paso desconocido %r en %r" % (ch, s))
    return out


class P:
    """Un patrón: por voz, una lista de pasos (texto de PatternIO)."""

    def __init__(self, length=None, scale=1, lens=None, **tracks):
        self.scale = scale
        self.t = {}
        self.lens = dict(lens or {})
        longest = 0
        for k, v in tracks.items():
            toks = track(v)
            longest = max(longest, len(toks))
            self.t[k] = toks
        self.length = length or max(16, longest)
        assert 1 <= self.length <= 64

    def copy(self):
        return copy.deepcopy(self)

    def put(self, voice, s, at=0):
        assert voice in VOICES, voice
        toks = track(s)
        row = self.t.setdefault(voice, [])
        need = at + len(toks)
        if len(row) < need:
            row.extend(["."] * (need - len(row)))
        for i, tk in enumerate(toks):
            row[at + i] = tk
        return self

    def clear(self, voice, a=0, b=None):
        if voice in self.t:
            row = self.t[voice]
            for i in range(a, min(len(row), b if b is not None else len(row))):
                row[i] = "."
        return self

    def with_(self, **tracks):
        p = self.copy()
        for k, v in tracks.items():
            if v is None:
                p.t.pop(k, None)
            else:
                p.t[k] = track(v)
        return p

    def json(self):
        tr = {}
        for k in VOICES:
            if k not in self.t:
                continue
            toks = list(self.t[k])
            while toks and toks[-1] == ".":
                toks.pop()
            if toks:
                tr[k] = " ".join(toks)
        d = {"length": self.length, "scale": self.scale, "tracks": tr}
        if self.lens:
            d["lens"] = dict(self.lens)
        return d


# ── Fills: una figura que reemplaza el final del compás (las voces de `keep` siguen) ──
def _ramp(n, lo="g", hi="X"):
    seq = "goxX" if lo == "g" else "oxX"
    return "".join(seq[min(len(seq) - 1, int(i * len(seq) / n))] for i in range(n))


def fill(base, kind, at=None, keep=("bd",)):
    f = base.copy()
    L = f.length
    if at is None:
        at = L - 4 if kind in ("snare16", "ratchet", "clap16", "stop", "kick16") else L - 8
    n = L - at
    for v in list(f.t):
        if v not in keep:
            f.clear(v, at)
    if kind == "toms":                      # bajada por los cuatro toms
        for i in range(n):
            tom = ["t4", "t3", "t2", "t1"][min(3, i * 4 // n)]
            f.put(tom, "x" if i % 2 == 0 else "o", at + i)
    elif kind == "toms8":                   # en corcheas, con flam en el primero
        for i in range(0, n, 2):
            tom = ["t4", "t3", "t2", "t1"][min(3, i * 4 // n)]
            f.put(tom, "f" if i == 0 else "x", at + i)
    elif kind == "hexa":                    # los tambores hexagonales: golpes sueltos que bajan
        seq = ["t4", "t3", "t2", "t1", "t2", "t1"]
        for j, i in enumerate(range(0, n, 3)):
            f.put(seq[j % len(seq)], "X" if j == 0 else "x", at + i)
        f.put("sd", "x" if n > 1 else ".", L - 1)
    elif kind == "snare16":
        f.put("sd", _ramp(n), at)
    elif kind == "ratchet":
        f.put("sd", ("xxRt" * 4)[:n], at)
    elif kind == "clap16":
        f.put("cp", _ramp(n, "o"), at)
    elif kind == "kick16":
        f.clear("bd", at)
        f.put("bd", "x" * n, at)
    elif kind == "stop":                    # todo calla el último tiempo (el "break")
        for v in list(f.t):
            f.clear(v, at)
    elif kind == "flams":
        f.put("sd", ("f.f.ff.f" * 4)[:n], at)
    elif kind == "rolltoms":
        f.put("sd", ("x.x.xxxx" * 4)[:n], at)
        f.put("t1", ("....x..x" * 4)[:n], at)
    elif kind == "open":
        f.put("oh", ("x.x.x.x." * 4)[:n], at)
        f.put("sd", ("...x..xx" * 4)[:n], at)
    else:
        raise ValueError(kind)
    return f


SETS = []
SET_BY_NAME = {}


def pset(genre, name, tempo, A, B, Fill, swing=50, swing_res=1):
    assert name not in SET_BY_NAME, name
    s = {"name": name, "genre": genre, "tempo": tempo, "swing": swing, "swing_res": swing_res,
         "A": A.json(), "B": B.json(), "Fill": Fill.json()}
    SETS.append(s)
    SET_BY_NAME[name] = s
    return s


PRESETS = []


def preset(cat, name, desc, sets, params, chain=None, mode=None, tempo=None, swing=None, autofill=None):
    pats = []
    for g, sn in enumerate(sets):
        s = SET_BY_NAME[sn]
        for var in ("A", "B", "Fill"):
            e = dict(s[var]); e["group"] = g; e["var"] = var
            pats.append(e)
    first = SET_BY_NAME[sets[0]]
    p = {"tempo": tempo or first["tempo"], "swing": swing if swing is not None else first["swing"], "swing_res": first["swing_res"]}
    p.update(params)
    if autofill is not None:
        p["autofill"] = autofill
    if chain:
        p["mode"] = 3
    elif mode is not None:
        p["mode"] = mode
    assert all(len(n) <= 40 for n in [name])
    entry = {"name": name, "category": cat, "description": desc, "order": len(PRESETS), "params": p, "patterns": pats}
    if chain:
        entry["chain"] = chain
    PRESETS.append(entry)


# ───────────────────────── Kits ─────────────────────────
ANALOG, DIGITAL = 0, 1
# Índices de las listas del plugin (Source/dsp/Analog.h, KitGen.h, Effects.h).
STUDIO82, POP84, ELECTRO81, HEXA84, CRUNCH88, STUDIO91, FOUNDRY, FM85 = range(8)
GATED, NONLIN, REVERSE, PLATE, SPRING, HALL, ROOM = range(7)
DIGI12, TAPE, BBD, CHORUS, ENSEMBLE, FLANGER, PHASER = range(7)
D_1_16, D_1_8, D_1_8D, D_1_4, D_1_4D = 2, 5, 7, 8, 9


def v(voice, **kw):
    return {f"{voice}_{k}": val for k, val in kw.items()}


def kit(*parts, **globals_):
    d = {}
    for p in parts:
        d.update(p)
    d.update(globals_)
    return d


def dig(voice, k, **kw):
    """Una voz en el motor digital con el kit k, sin VCA (la muestra entera)."""
    return v(voice, engine=DIGITAL, kit=k, decay=kw.pop("decay", 100), **kw)


# La mezcla de base: paneo de un baterista visto desde el público, niveles de partida.
BASE = kit(
    v("bd", level=-1), v("sd", level=0), v("cp", level=-2), v("rs", level=-5, pan=-10),
    v("ch", level=-3, pan=20), v("oh", level=-4, pan=25), v("cy", level=-7, pan=-30), v("rd", level=-6, pan=-20),
    v("t1", level=-4, pan=-40), v("t2", level=-4, pan=-15), v("t3", level=-4, pan=15), v("t4", level=-4, pan=40),
    v("cb", level=-6, pan=-15), v("cl", level=-5, pan=30), v("tb", level=-6, pan=35), v("fx", level=-7),
)


def K(*parts, **g):
    return kit(BASE, *parts, **g)


# ───────────────────────── Biblioteca de patrones ─────────────────────────
# Dark wave
A = P(bd="x.....x.x.....x.", sd="....X.......X...", cp="....x.......x...", ch="x.x.x.x.x.x.x.x.", tb="..o...o...o...o.")
pset("Dark wave", "Night Drive", 120, A, A.with_(bd="x.....x.x...x.x.", ch="x.x.x.x.x.x.x...", oh="..............x."), fill(A, "toms", keep=("bd", "ch")))
A = P(bd="x...x...x...x...", sd="....X.......X...", ch="xgxgxgxgxgxgxgxg", oh="..x...x...x...x.")
pset("Dark wave", "Sodium Lights", 124, A, A.with_(rs="...x.....x....x.", oh="..x...x...x.x.x."), fill(A, "snare16", keep=("bd", "ch")))
A = P(bd="x.......x.x.....", sd="........X.......", ch="x.x.x.x.x.x.x.x.", t1="..........o...o.", cl="...o..........o.")
pset("Dark wave", "Undertow", 112, A, A.with_(bd="x.......x.x...x.", t2="......o.........", sd="........X.....g."), fill(A, "hexa", keep=("bd",)))
A = P(bd="x...x...x...x...", sd="....x.......x...", cp="....x.......x...", ch="xoxoxoxoxoxoxoxo", rd="x...............")
pset("Dark wave", "Cathode Hymn", 128, A, A.with_(ch="xo.oxo.oxo.oxo.o", oh="..x...x...x...x."), fill(A, "clap16", keep=("bd", "ch")))

# Coldwave
A = P(bd="x.......x.x.....", sd="....x.......x...", ch="x.x.x.x.x.x.x.x.", cl="...x......x.....")
pset("Coldwave", "Frost Lines", 132, A, A.with_(ch="xxxxxxxxxxxxxxxx", cl="...x......x...x."), fill(A, "ratchet", keep=("bd", "ch")))
A = P(bd="x..x..x...x.....", sd="....x.......x..o", ch="x.xxx.xxx.xxx.xx")
pset("Coldwave", "Paper Static", 126, A, A.with_(oh="......x.......x.", ch="x.xxx.x.x.xxx.x."), fill(A, "toms8", keep=("bd",)))
A = P(bd="x.x...x.x.x...x.", sd="....x.......x...", ch="x.x.x.x.x.x.x.x.", tb="xoxoxoxoxoxoxoxo")
pset("Coldwave", "Thin Ice", 138, A, A.with_(sd="....x.......x.x.", cy="x..............."), fill(A, "snare16", keep=("bd", "tb")))
A = P(bd="x...x...x...x...", sd="....x.......x...", ch="xpxpxpxpxpxpxpxp", cb="..o.......o.....")
pset("Coldwave", "Winter Signal", 120, A, A.with_(cb="..o...o...o..o..", rs="....g.......g..."), fill(A, "flams", keep=("bd", "ch")))

# Post-punk
A = P(bd="x.......x.......", t1="x..x..x...x..x..", t2="......x.......x.", sd="....x.......x...", ch="x.x.x.x.x.x.x.x.")
pset("Post-punk", "Floor Tom Sermon", 140, A, A.with_(t1="x.xx.x.xx.x.x.xx", sd="....x.......x..."), fill(A, "toms"))
A = P(bd="x...x.x.x...x.x.", sd="....x.......x...", ch="oooooooooooooooo")
pset("Post-punk", "Grey Factory", 150, A, A.with_(ch="xoooxoooxoooxooo", cy="x..............."), fill(A, "snare16", keep=("bd", "ch")))
A = P(bd="x...x...x...x...", sd="....x.......x...", oh="..x...x...x...x.", cb="x..x..x.x..x..x.")
pset("Post-punk", "Chalk Outline", 132, A, A.with_(cb=None, ch="x.x.x.x.x.x.x.x.", oh="..x...x...x...x."), fill(A, "rolltoms", keep=("bd", "oh")))
A = P(bd="x..x..x.x..x....", sd="....x..x....x...", rd="x.x.x.x.x.x.x.x.")
pset("Post-punk", "Stairwell Shuffle", 144, A, A.with_(sd="....x..x....x.x.", oh=".............x.."), fill(A, "toms8", keep=("rd",)), swing=58, swing_res=1)

# Minimal wave
A = P(bd="x...x.x.x...x.x.", sd="....x.......x...", ch="x.x.x.x.x.x.x.x.")
pset("Minimal wave", "Rhythm Box Rock", 110, A, A.with_(rs="..x.......x....."), fill(A, "stop"))
A = P(bd="x..x....x..x....", rs="x..x..x...x..x..", ch="x.x.x.x.x.x.x.x.", cl="...x..x...x..x..")
pset("Minimal wave", "Bedroom Bossa", 118, A, A.with_(cl="..x..x....x..x..", tb="oxoxoxoxoxoxoxox"), fill(A, "toms8", keep=("bd", "ch")))
A = P(bd="x.......x.......", sd="........x.......", ch="..x...x...x...x.", tb="gogogogogogogogo")
pset("Minimal wave", "Polaroid Pulse", 104, A, A.with_(bd="x.......x..x....", cb="......o.......o."), fill(A, "snare16", keep=("bd", "tb")))
A = P(length=12, scale=3, bd="x.....x.....", sd="...x.....x..", ch="x.xx.xx.xx.x")
pset("Minimal wave", "Cassette Ritual", 122, A, A.with_(rs="..x.....x...", ch="xxxxxxxxxxxx"), fill(A, "snare16", at=8, keep=("bd",)))

# EBM
A = P(bd="x...x...x...x...", sd="....X.......X...", cp="....X.......X...", ch="..x...x...x...x.", rs="...........x....")
pset("EBM", "Steel Boots", 122, A, A.with_(ch="..x.x.x...x.x.x."), fill(A, "ratchet", keep=("bd", "ch")))
A = P(bd="x...x...x...x..x", sd="....X.......X...", ch="x.xxx.xxx.xxx.xx", fx="...........x....")
pset("EBM", "Body Discipline", 126, A, A.with_(fx="...x.......x....", cb="..x.......x....."), fill(A, "clap16", keep=("bd", "ch")))
A = P(bd="x..x..x.x..x..x.", sd="....X..g....X.gg", ch="x.x.x.x.x.x.x.x.")
pset("EBM", "Iron Parade", 118, A, A.with_(sd="....X..g.g..X.gg", t1="..............xx"), fill(A, "flams", keep=("bd", "ch")))
A = P(bd="x...x...x...x...", cp="....X.......X...", ch="gxgxgxgxgxgxgxgx", cb="..x...x...x...x.")
pset("EBM", "Sweat Engine", 130, A, A.with_(ch="gxgxgxgxgxgxgxgx", oh="..x.......x.....", cb=None), fill(A, "kick16", keep=("ch", "cp")))

# Synthpop
A = P(bd="x.....x.x.......", sd="....x.......x...", cp="....x.......x...", ch="xoxoxoxoxoxoxoxo", tb="....x.......x...")
pset("Synthpop", "Neon Promenade", 118, A, A.with_(bd="x.....x.x.x.....", oh="......x........."), fill(A, "toms", keep=("bd", "ch")))
A = P(bd="x..x..x.x.......", sd="....x.......x...", tb="oxoxoxoxoxoxoxox", cb="x.......x.x.....")
pset("Synthpop", "Tambourine Summer", 112, A, A.with_(cb="x...x...x.x...x.", ch="x.x.x.x.x.x.x.x."), fill(A, "snare16", keep=("bd", "tb")), swing=54)
A = P(bd="x...x...x...x.x.", cp="....x.......x...", ch="..x...x...x...x.", cl="x..x..x...x.x...")
pset("Synthpop", "Satin Arcade", 122, A, A.with_(cl="..x.x...x..x..x.", rd="x.x.x.x.x.x.x.x."), fill(A, "toms8", keep=("bd", "ch")))
A = P(bd="x......xx.......", sd="....x..x.x..x...", ch="xxxxxxxxxxxxxxxx")
pset("Synthpop", "Mirror Ballroom", 116, A, A.with_(sd="....x..x.x..x..x", oh="......x.......x."), fill(A, "rolltoms", keep=("bd", "ch")), swing=58)

# Goth rock
A = P(bd="x.x...x.x.x...x.", sd="....X.......X...", oh="x.x.x.x.x.x.x.x.", cy="x...............")
pset("Goth rock", "Bat Cave Stomp", 138, A, A.with_(oh=None, ch="x.x.x.x.x.x.x.x.", t1="......x.......x."), fill(A, "toms8"))
A = P(bd="x.....x...x.....", sd="....x.......x...", rd="x.x.x.x.x.x.x.x.", t1="..............xx")
pset("Goth rock", "Crimson Nave", 128, A, A.with_(rd="x.xxx.xxx.xxx.xx", cy="x..............."), fill(A, "toms", keep=("bd",)))
A = P(length=12, bd="x...........", sd="....x...x...", ch="x.x.x.x.x.x.", cy="x...........")
pset("Goth rock", "Graveyard Waltz", 150, A, A.with_(bd="x.....x.....", oh="..........x."), fill(A, "toms8", at=6, keep=()))
A = P(bd="x.x...x.x.x...x.", sd="....x.......x...", ch="xxxxxxxxxxxxxxxx")
pset("Goth rock", "Black Lace Gallop", 146, A, A.with_(bd="x.xx..x.x.xx..x.", oh="..............x."), fill(A, "snare16", keep=("bd",)))

# Industrial
A = P(bd="x..x...x..x.....", sd="....X.......X...", fx="x.......x.......", tb="..x..x..x..x..x.", cb="......x.......x.")
pset("Industrial", "Rust Choir", 100, A, A.with_(fx="x.....x.x.......", rs="...x.......x...."), fill(A, "ratchet", keep=("bd", "tb")))
A = P(bd="x...x...x...x...", sd="....x.......x...", ch="x..x..x.x...", cb="x..x.x.", lens={"ch": 12, "cb": 7})
pset("Industrial", "Pressure Plant", 128, A, A.with_(oh="..x.......x....."), fill(A, "kick16", keep=("ch", "cb")))
A = P(bd="x.......x..x....", t1="x..x....x..x..x.", t2="...x.x.....x.x..", t4="......x.......x.", fx="....x...........")
pset("Industrial", "Scrap Ritual", 92, A, A.with_(t3="..x.......x.....", sd="........X......."), fill(A, "hexa", keep=("bd",)))
A = P(bd="x.xxx.x.x.xxx.x.", sd="....X.......X..r", ch="x.x.x.x.x.x.x.x.")
pset("Industrial", "Broken Conveyor", 136, A, A.with_(sd="....X..r....X..R", fx="x..............."), fill(A, "ratchet", keep=("bd",)))

# New beat
A = P(bd="x...x...x...x...", sd="....X.......X...", ch="..x...x...x...x.", oh="..............x.", fx="x...............")
pset("New beat", "Slow Leather", 108, A, A.with_(rs="...x.....x......", fx=None), fill(A, "clap16", keep=("bd", "ch")))
A = P(bd="x...x...x..xx...", cp="....x.......x...", ch="x.x.x.x.x.x.x.x.", rs="...x.....x......")
pset("New beat", "Low Countries Fog", 104, A, A.with_(ch="xgx.xgx.xgx.xgx.", oh="..x.......x....."), fill(A, "snare16", keep=("bd", "ch")))
A = P(bd="x...x...x...x...", sd="....x.......x...", ch="gogogogogogogogo", cb="...o.......o....")
pset("New beat", "Acid Promenade", 112, A, A.with_(cb="...o..o....o..o.", oh="..x...x...x...x."), fill(A, "ratchet", keep=("bd", "ch")))
A = P(bd="x.....x...x.....", sd="........X.......", ch="x.x.x.x.x.x.x.x.", tb="....o.......o...")
pset("New beat", "Dense Smoke", 100, A, A.with_(bd="x.....x...x...x.", fx="........x......."), fill(A, "toms8", keep=("bd", "ch")))

# Italo / hi-NRG
A = P(bd="x...x...x...x...", cp="....x.......x...", oh="..x...x...x...x.", ch="x...x...x...x...", t2="..............x.")
pset("Italo / hi-NRG", "Chrome Heartbeat", 128, A, A.with_(sd="....x..x....x.xx", t2="......x.......x."), fill(A, "snare16", keep=("bd", "oh")))
A = P(bd="x...x...x...x...", sd="....x.......x...", oh="..x...x...x...x.", tb="oxoxoxoxoxoxoxox", cb="..x.......x.....")
pset("Italo / hi-NRG", "Disco Phantom", 132, A, A.with_(cb="..x...x...x...x.", cp="....x.......x..."), fill(A, "clap16", keep=("bd", "oh", "tb")))
A = P(bd="x...x...x...x...", cp="....x.......x...", ch="xxxxxxxxxxxxxxxx", t1="x.....x.....x...", t3="...x.....x......")
pset("Italo / hi-NRG", "Laser Gondola", 122, A, A.with_(t1="x.....x...x.x...", t4=".......x.......x"), fill(A, "hexa", keep=("bd", "ch")))
A = P(bd="x...x...x...x...", sd="....x.......x...", oh="..x...x...x...x.", rs="x..x..x..x..x...")
pset("Italo / hi-NRG", "Starlight Cruiser", 126, A, A.with_(rs=None, ch="x.x.x.x.x.x.x.x.", tb="....x.......x..."), fill(A, "toms", keep=("bd", "oh")))

# Witch house (tempo lento, redobles de charles con ratchet)
A = P(bd="x.......x..x....", sd="........X.......", ch="x.x.x.xrx.x.xRx.")
pset("Witch house", "Hex Drag", 70, A, A.with_(ch="x.xrx.x.xtx.xRx.", fx="..............x."), fill(A, "ratchet", keep=("bd", "ch")))
A = P(bd="x......x..x.....", cp="........X.......", ch="x.x.xrx.x.x.x.xt", fx="x...............")
pset("Witch house", "Candle Smoke", 72, A, A.with_(bd="x......x..x..x..", oh="......x........."), fill(A, "stop"))
A = P(bd="x...............", sd="........x.......", oh="....x.......x...", ch="..x...x...x...xr")
pset("Witch house", "Occult Slowmo", 66, A, A.with_(bd="x.........x.....", rd="x.......x......."), fill(A, "rolltoms", keep=("bd",)))
A = P(bd="x.....x...x.....", sd="....x.......x...", ch="xrx.xrx.xrx.xRxt", cl="......x.......x.")
pset("Witch house", "Pale Coven", 74, A, A.with_(cl="..x...x...x...x.", fx="x..............."), fill(A, "snare16", keep=("bd", "ch")))

# 90s darkwave / trip-hop
A = P(bd="x.....x...x..x..", sd="....x..g.g..x..g", ch="x.x.x.x.x.x.x.x.")
pset("90s darkwave / trip-hop", "Rainy Breakbeat", 88, A, A.with_(oh="..............x.", sd="....x..g.g..x.gg"), fill(A, "snare16", keep=("bd", "ch")), swing=58)
A = P(bd="x......x.x......", sd="....x.......x...", rd="x.xxx.xxx.xxx.xx")
pset("90s darkwave / trip-hop", "Harbour Fog", 80, A, A.with_(rd="x.x.x.x.x.x.x.x.", rs="..g.....g....g.."), fill(A, "toms8", keep=("bd",)), swing=62)
A = P(bd="x..x....x.x.....", sd="....x..g....x.g.", ch="xgxgxgxgxgxgxgxg")
pset("90s darkwave / trip-hop", "Dusty Ritual Break", 92, A, A.with_(bd="x..x....x.x..x..", cb="..........o....."), fill(A, "flams", keep=("bd", "ch")), swing=62)
A = P(bd="x.......x.x.....", sd="....x.......x...", tb="g.o.g.o.g.o.g.o.", ch="....x.......x...")
pset("90s darkwave / trip-hop", "Sleepwalk Shuffle", 76, A, A.with_(bd="x.......x.x...x.", rd="x.......x......."), fill(A, "toms", keep=("bd", "tb")), swing=60)

# Ambient oscuro
A = P(bd="x...............", t1="......x.......x.", fx="....x...........", cl="..........o.....")
pset("Dark ambient", "Cistern", 70, A, A.with_(t2="..x.......x.....", fx="....x.......q..."), fill(A, "toms8", keep=()))
A = P(bd="x.......x.......", t1="x...x...x...x...", t2="..o...o...o...o.")
pset("Dark ambient", "Procession", 84, A, A.with_(t3="....o.......o...", cy="x..............."), fill(A, "toms", keep=("bd",)))
A = P(rd="x...x...x...x...", bd="x.........x.....", fx="........x.......")
pset("Dark ambient", "Catacomb Drift", 60, A, A.with_(rd="x...x.p.x...x.p.", cl="......q.......q."), fill(A, "stop"))
A = P(tb="p.q.p.q.p.q.p.q.", cl="...x.......x....", bd="x.......q.......", t1="..........p.....")
pset("Dark ambient", "Moth Light", 90, A, A.with_(cb="q.......q.......", t2="......p.......p."), fill(A, "hexa", keep=("tb",)))


# ───────────────────────── Presets: sonido + juegos ─────────────────────────
DW = "Dark wave"
preset(DW, "Night Shift Mercury", "A bridged-T kick and a gated 8-bit snare over driving hats: the classic dark wave engine room.",
       ["Night Drive", "Sodium Lights"],
       K(v("bd", model=0, decay=55, char1=40, char2=35), dig("sd", POP84, senda=45), v("cp", model=1, senda=40),
         v("ch", model=0, decay=40), v("oh", model=0), v("t1", model=1, senda=35, char2=55), v("t2", model=1, senda=35, char2=55),
         v("t3", model=1, senda=35, char2=55), v("t4", model=1, senda=35, char2=55), v("tb", model=1),
         a_type=GATED, a_gate=260, a_size=65, b_type=CHORUS, b_rate=25, b_depth=55, b_return=-8, m_comp=35, m_tape=1),
       chain=[[0, 0, 3, 0], [0, 1, 1, 1], [1, 0, 3, 0], [1, 1, 1, 1]])
preset(DW, "Velvet Undertow", "Half-time sway, hexagonal toms in the gaps and a long plate.",
       ["Undertow"],
       K(v("bd", model=4, decay=45, char1=30, char2=40, level=-1), v("sd", model=0, char1=60, senda=40), v("ch", model=2, char1=30),
         v("t1", model=1, senda=45), v("t2", model=1, senda=45), v("t3", model=1, senda=45), v("t4", model=1, senda=45), v("cl", model=2, sendb=40),
         a_type=PLATE, a_size=70, a_tone=50, b_type=DIGI12, b_time=D_1_8D, b_fb=40, b_return=-10, m_tape=1, m_comp=25),
       autofill=2)
preset(DW, "Cathode Hymn", "Four-on-the-floor with clap and snare stacked, hats through a slow phaser.",
       ["Cathode Hymn"],
       K(v("bd", model=0, decay=40, char1=55), dig("sd", STUDIO82, senda=30), v("cp", model=0, senda=30), v("ch", model=0, sendb=45),
         v("rd", model=1), a_type=GATED, a_gate=220, b_type=PHASER, b_rate=15, b_depth=70, b_fb=50, b_return=-4, m_comp=40, m_exciter=20),
       autofill=3)
preset(DW, "Sodium Streetlight", "Ghosted sixteenth hats, open hats on the offbeat, rims answering.",
       ["Sodium Lights", "Night Drive"],
       K(v("bd", model=1, decay=35, char2=40), v("sd", model=4, char1=60, senda=35), v("ch", model=0, decay=30, char1=30), v("oh", model=0, decay=40),
         v("rs", model=0), v("tb", model=2), a_type=ROOM, a_size=45, b_type=DIGI12, b_time=D_1_8D, b_fb=30, b_return=-12, m_comp=30),
       chain=[[0, 0, 4, 0], [0, 1, 3, 1], [1, 0, 4, 0], [1, 1, 3, 1]])
preset(DW, "Glass Cathedral", "The digital kit from 1991 with a hall and the tape on the bus.",
       ["Cathode Hymn", "Undertow"],
       K(dig("bd", STUDIO91), dig("sd", STUDIO91, senda=35), dig("cp", POP84), dig("ch", STUDIO91), dig("oh", STUDIO91), dig("rd", STUDIO91),
         dig("t1", STUDIO91, senda=40), dig("t2", STUDIO91, senda=40), dig("t3", STUDIO91, senda=40), dig("t4", STUDIO91, senda=40), dig("cl", STUDIO91),
         a_type=HALL, a_size=70, a_return=-4, m_tape=1, m_tape_drive=3, m_comp=30))

CW = "Coldwave"
preset(CW, "Frost on Concrete", "Thin cheap-box drums, a blip clave and a spring on the snare.",
       ["Frost Lines", "Paper Static"],
       K(v("bd", model=2, decay=50), v("sd", model=3, char1=60, senda=40), v("ch", model=1, char1=40), v("cl", model=2), v("t1", model=0),
         v("t2", model=0), v("t3", model=0), v("t4", model=0), a_type=SPRING, a_size=60, a_return=-3, b_type=FLANGER, b_rate=10, b_depth=60, b_fb=60,
         m_lofi=2, m_lofi_mix=60),
       chain=[[0, 0, 4, 0], [0, 1, 4, 1], [1, 0, 4, 0], [1, 1, 4, 1]])
for vo in ("ch",):
    PRESETS[-1]["params"][f"{vo}_sendb"] = 55
preset(CW, "Paper Moon Static", "Eight-bit companded kit, chorus on the hats, the room cut short.",
       ["Paper Static"],
       K(dig("bd", STUDIO82), dig("sd", STUDIO82, senda=40), dig("ch", STUDIO82, sendb=50), dig("oh", STUDIO82, sendb=50),
         dig("t1", STUDIO82), dig("t2", STUDIO82), dig("t3", STUDIO82), dig("t4", STUDIO82),
         a_type=NONLIN, a_gate=180, a_shape=0, b_type=CHORUS, b_rate=35, b_depth=60, b_return=-3, m_comp=25), autofill=2)
preset(CW, "Bleach Light", "Fast and bright: rattling sixteenth hats, tambourine, a dry crack snare.",
       ["Thin Ice"],
       K(v("bd", model=1, decay=30), v("sd", model=4, char1=70, decay=35), v("ch", model=0, decay=25), v("tb", model=2, char1=20), v("cy", model=0),
         a_type=ROOM, a_size=30, b_type=DIGI12, b_time=D_1_16, b_fb=20, b_return=-14, m_exciter=30, m_comp=35), autofill=2)
preset(CW, "Thin Ice Lines", "Hexagonal kit at eight bits, flanged hats, a cold nonlinear room.",
       ["Frost Lines", "Thin Ice"],
       K(dig("bd", HEXA84), dig("sd", HEXA84, senda=40), dig("ch", HEXA84, sendb=60), dig("oh", HEXA84, sendb=60), dig("cl", HEXA84),
         dig("t1", HEXA84), dig("t2", HEXA84), dig("t3", HEXA84), dig("t4", HEXA84), dig("tb", HEXA84),
         a_type=NONLIN, a_gate=300, a_shape=-40, b_type=FLANGER, b_rate=8, b_depth=70, b_fb=70, b_return=-2),
       chain=[[0, 0, 4, 0], [0, 1, 4, 1], [1, 0, 4, 0], [1, 1, 4, 1]])
preset(CW, "Winter Transmission", "Probabilistic hats that never repeat the same way, cowbell pings into a 12-bit echo.",
       ["Winter Signal"],
       K(v("bd", model=0, decay=40), v("sd", model=0, char1=50, senda=35), v("ch", model=2, char1=40), v("cb", model=0, sendb=60), v("rs", model=1),
         a_type=PLATE, a_size=55, b_type=DIGI12, b_time=D_1_8D, b_fb=50, b_tone=40, b_return=-6, m_comp=30), autofill=3)

PP = "Post-punk"
preset(PP, "Floor Tom Sermon", "Tribal floor toms around a backbeat, the room loud.",
       ["Floor Tom Sermon"],
       K(dig("bd", STUDIO82), dig("sd", STUDIO82, senda=30), dig("ch", STUDIO82), dig("t1", STUDIO91, senda=40, level=-2), dig("t2", STUDIO91, senda=40),
         dig("t3", STUDIO91), dig("t4", STUDIO91), a_type=ROOM, a_size=55, a_return=-2, m_tape=1, m_tape_drive=4, m_comp=45), autofill=2)
preset(PP, "Grey Factory Floor", "Motorik: kick and snare locked, sixteenths running underneath.",
       ["Grey Factory"],
       K(v("bd", model=1, decay=30, char1=60), v("sd", model=1, char1=55, senda=25), v("ch", model=1, char1=35, decay=25), v("cy", model=0),
         a_type=ROOM, a_size=35, m_tape=1, m_tape_drive=6, m_comp=50, m_ratio=2), autofill=3)
preset(PP, "Chalk Outline", "Disco-punk: four on the floor, open hats on the and, cowbell nagging.",
       ["Chalk Outline", "Grey Factory"],
       K(dig("bd", CRUNCH88), dig("sd", POP84, senda=30), dig("oh", STUDIO82), dig("ch", STUDIO82), dig("cb", STUDIO82), dig("t1", POP84),
         a_type=GATED, a_gate=200, b_type=DIGI12, b_time=D_1_8D, b_fb=25, b_return=-14, m_comp=40),
       chain=[[0, 0, 3, 0], [0, 1, 1, 1], [0, 0, 3, 0], [1, 0, 2, 1]])
preset(PP, "Wire and Rain", "Swung ride, a stairwell room and a tape echo on the snare.",
       ["Stairwell Shuffle"],
       K(dig("bd", STUDIO91), dig("sd", STUDIO91, senda=30, sendb=25), dig("rd", STUDIO91), dig("oh", STUDIO91), dig("t1", STUDIO91), dig("t2", STUDIO91),
         dig("t3", STUDIO91), dig("t4", STUDIO91), a_type=ROOM, a_size=60, b_type=TAPE, b_time=D_1_8D, b_fb=35, b_rate=40, b_return=-8, m_tape=1), autofill=2)
preset(PP, "Stairwell Echo", "Analog kit with the hats chorused and everything a little drunk on tape.",
       ["Floor Tom Sermon", "Stairwell Shuffle"],
       K(v("bd", model=0, decay=40), v("sd", model=0, char1=55, senda=30), v("ch", model=0, sendb=40), v("rd", model=0), v("t1", model=0, char1=40),
         v("t2", model=0, char1=40), v("t3", model=0), v("t4", model=0), a_type=SPRING, a_size=55, b_type=ENSEMBLE, b_depth=50, b_return=-6,
         m_tape=1, m_tape_drive=8),
       chain=[[0, 0, 4, 1], [1, 0, 4, 1]])

MW = "Minimal wave"
preset(MW, "Bedroom Rhythm Box", "The cheap analog box on its rock preset: small kick, noise snare and noise hats.",
       ["Rhythm Box Rock"],
       K(v("bd", model=2), v("sd", model=3), v("ch", model=1, decay=40), v("rs", model=0), m_lofi=2, m_lofi_mix=40, m_tape=1), autofill=3)
preset(MW, "Cassette Ritual", "Triplet sixteenths on the metal-beat hats, as if recorded to a worn cassette.",
       ["Cassette Ritual"],
       K(v("bd", model=2), v("sd", model=3, char1=40), v("ch", model=2, char1=70), v("rs", model=0), m_tape=1, m_tape_drive=8, m_lofi=1, m_lofi_mix=50), autofill=2)
preset(MW, "Small Machine Love", "A bossa from the organ-top box: rim, clave and shaker.",
       ["Bedroom Bossa"],
       K(v("bd", model=2), v("rs", model=0), v("ch", model=1), v("cl", model=0), v("tb", model=0), a_type=SPRING, a_size=40, a_return=-8,
         b_type=CHORUS, b_rate=30, b_depth=40, b_return=-10, m_tape=1), autofill=3)
preset(MW, "Basement Polaroid", "Eight-bit samples, a shaker carpet and very little else.",
       ["Polaroid Pulse"],
       K(dig("bd", ELECTRO81), dig("sd", ELECTRO81, senda=30), dig("ch", ELECTRO81), dig("tb", STUDIO82), dig("cb", ELECTRO81),
         a_type=PLATE, a_size=45, m_lofi=3, m_lofi_mix=40), autofill=2)
preset(MW, "Preset Rhumba Ghost", "Bossa and rock box patterns alternating, through the 12-bit sampler.",
       ["Bedroom Bossa", "Rhythm Box Rock"],
       K(v("bd", model=2), v("sd", model=3), v("rs", model=0), v("ch", model=2, char1=40), v("cl", model=0), v("tb", model=0),
         m_lofi=1, m_lofi_mix=70, m_tape=1),
       chain=[[0, 0, 4, 1], [1, 0, 2, 0], [1, 1, 2, 1]])

EB = "EBM"
preset(EB, "Steel Boots", "Kick on every beat, snare and clap slammed together, offbeat hats.",
       ["Steel Boots"],
       K(v("bd", model=4, decay=35, char1=60, char2=30, drive=30), v("sd", model=4, char1=70, senda=35), v("cp", model=0, senda=30), v("ch", model=0, decay=30),
         v("rs", model=0), a_type=GATED, a_gate=180, a_size=50, m_comp=55, m_ratio=1, m_attack=3, m_exciter=25), autofill=3)
preset(EB, "Body Discipline", "Distorted dark kick, metallic zaps, everything compressed hard.",
       ["Body Discipline"],
       K(v("bd", model=4, decay=40, drive=45), v("sd", model=1, char1=70, drive=30, senda=25), v("cp", model=1), v("ch", model=3, char1=40),
         v("fx", model=2, sendb=40), v("cb", model=1), a_type=GATED, a_gate=150, b_type=DIGI12, b_time=D_1_16, b_fb=35, b_return=-10,
         m_comp=65, m_ratio=2, m_tape=1, m_tape_drive=6), autofill=2)
preset(EB, "Iron Parade", "A marching snare with ghost notes over a swinging kick.",
       ["Iron Parade"],
       K(v("bd", model=1, decay=35), v("sd", model=0, char1=65, senda=30), v("ch", model=0), v("t1", model=0),
         a_type=ROOM, a_size=40, m_comp=45), autofill=2)
preset(EB, "Sweat Engine", "Clap backbeat, sixteenth hats and a clanking cowbell on the offbeat.",
       ["Sweat Engine", "Steel Boots"],
       K(v("bd", model=4, decay=30, drive=25), v("cp", model=1, senda=30), v("ch", model=0, decay=25), v("oh", model=0), v("cb", model=1, sendb=30),
         dig("sd", FOUNDRY, senda=30), a_type=GATED, a_gate=200, b_type=DIGI12, b_time=D_1_8D, b_fb=30, b_return=-12, m_comp=55),
       chain=[[0, 0, 4, 0], [0, 1, 3, 1], [1, 0, 4, 0], [1, 1, 3, 1]])
preset(EB, "Red Signal Drill", "Twelve-bit grit on the whole kit, the clap like a door slamming.",
       ["Steel Boots", "Body Discipline"],
       K(dig("bd", CRUNCH88, drive=30), dig("sd", CRUNCH88, senda=30), dig("cp", CRUNCH88, senda=30), dig("ch", CRUNCH88), dig("oh", CRUNCH88),
         dig("rs", CRUNCH88), dig("fx", CRUNCH88), dig("cb", FOUNDRY), a_type=GATED, a_gate=170, m_lofi=1, m_lofi_mix=60, m_comp=55),
       chain=[[0, 0, 4, 1], [1, 0, 4, 1]])

SP = "Synthpop"
preset(SP, "Neon Promenade", "The 1982 acoustic sample kit: claps on the snare, tambourine on the backbeat.",
       ["Neon Promenade", "Tambourine Summer"],
       K(dig("bd", STUDIO82), dig("sd", STUDIO82, senda=25), dig("cp", STUDIO82), dig("ch", STUDIO82), dig("oh", STUDIO82), dig("tb", STUDIO82),
         dig("cb", STUDIO82), dig("t1", STUDIO82, senda=25), dig("t2", STUDIO82, senda=25), dig("t3", STUDIO82, senda=25), dig("t4", STUDIO82, senda=25),
         a_type=GATED, a_gate=240, a_return=-4, b_type=CHORUS, b_rate=20, b_depth=40, b_return=-12, m_comp=35),
       chain=[[0, 0, 3, 0], [0, 1, 1, 1], [1, 0, 3, 0], [1, 1, 1, 1]])
preset(SP, "Hairspray Hearts", "Gated snare the size of a gymnasium, toms with the same room.",
       ["Neon Promenade"],
       K(dig("bd", POP84), dig("sd", POP84, senda=60), dig("cp", POP84, senda=40), dig("ch", POP84), dig("oh", POP84),
         dig("t1", POP84, senda=55), dig("t2", POP84, senda=55), dig("t3", POP84, senda=55), dig("t4", POP84, senda=55), dig("tb", POP84),
         a_type=GATED, a_gate=320, a_size=80, a_return=0, m_comp=40, m_exciter=30), autofill=3)
preset(SP, "Tambourine Summer", "A slight swing, cowbell and tambourine doing the dancing.",
       ["Tambourine Summer"],
       K(v("bd", model=1, decay=40), v("sd", model=1, char1=50, senda=30), v("tb", model=1), v("cb", model=0), v("ch", model=0),
         a_type=PLATE, a_size=50, m_comp=30), autofill=3)
preset(SP, "Satin Arcade", "Clave and clap on the electro kit, a BBD echo on the clave.",
       ["Satin Arcade"],
       K(dig("bd", ELECTRO81), dig("cp", ELECTRO81, senda=30), dig("ch", ELECTRO81), dig("cl", ELECTRO81, sendb=50), dig("rd", ELECTRO81),
         dig("t1", ELECTRO81), dig("t2", ELECTRO81), dig("t3", ELECTRO81), dig("t4", ELECTRO81),
         a_type=PLATE, a_size=50, b_type=BBD, b_time=D_1_8D, b_fb=45, b_return=-6, m_comp=30), autofill=3)
preset(SP, "Mirror Ballroom", "Shuffled sixteenths on the FM kit, glassy and quick.",
       ["Mirror Ballroom"],
       K(dig("bd", FM85), dig("sd", FM85, senda=30), dig("ch", FM85), dig("oh", FM85), dig("t1", FM85), dig("t2", FM85), dig("t3", FM85), dig("t4", FM85),
         a_type=HALL, a_size=45, a_return=-6, b_type=ENSEMBLE, b_depth=40, b_return=-10, m_comp=30), autofill=2)

GR = "Goth rock"
preset(GR, "Bat Cave Stomp", "A rock machine pattern with crash on the one and a big plate.",
       ["Bat Cave Stomp"],
       K(dig("bd", POP84), dig("sd", POP84, senda=45), dig("oh", STUDIO82), dig("ch", STUDIO82), dig("cy", STUDIO91), dig("t1", POP84, senda=40),
         dig("t2", POP84, senda=40), dig("t3", POP84, senda=40), dig("t4", POP84, senda=40),
         a_type=PLATE, a_size=75, a_return=-2, m_comp=45, m_tape=1), autofill=3)
preset(GR, "Crimson Nave", "Ride eighths, a slow pulse and a church-sized hall.",
       ["Crimson Nave"],
       K(dig("bd", STUDIO91), dig("sd", STUDIO91, senda=45), dig("rd", STUDIO91), dig("cy", STUDIO91), dig("t1", STUDIO91, senda=40), dig("t2", STUDIO91, senda=40),
         dig("t3", STUDIO91, senda=40), dig("t4", STUDIO91, senda=40), a_type=HALL, a_size=85, a_predelay=30, a_return=-2, m_comp=35), autofill=2)
preset(GR, "Vigil Machine", "The drum machine of a band without a drummer: tight, loud, gated toms.",
       ["Black Lace Gallop", "Bat Cave Stomp"],
       K(v("bd", model=1, decay=30, char1=65), v("sd", model=1, char1=60, senda=45), v("ch", model=0, decay=30), v("oh", model=0), v("cy", model=0),
         v("t1", model=2, senda=50), v("t2", model=2, senda=50), v("t3", model=2, senda=50), v("t4", model=2, senda=50),
         a_type=GATED, a_gate=280, a_size=70, m_comp=55, m_exciter=20),
       chain=[[0, 0, 4, 0], [0, 1, 4, 1], [1, 0, 4, 0], [1, 1, 4, 1]])
preset(GR, "Graveyard Waltz", "Three-four, the kick alone on the one, the ride hissing.",
       ["Graveyard Waltz"],
       K(dig("bd", STUDIO91), dig("sd", STUDIO91, senda=40), dig("ch", STUDIO91), dig("oh", STUDIO91), dig("cy", STUDIO91),
         dig("t1", STUDIO91), dig("t2", STUDIO91), dig("t3", STUDIO91), dig("t4", STUDIO91), a_type=HALL, a_size=70, m_tape=1), autofill=3)
preset(GR, "Black Lace Anthem", "Gallop kick, sixteenth hats, a reverse room swelling into the snare.",
       ["Black Lace Gallop"],
       K(dig("bd", POP84), dig("sd", POP84, senda=50), dig("ch", POP84), dig("oh", POP84), dig("cy", STUDIO91),
         a_type=REVERSE, a_gate=220, a_size=60, a_return=-3, m_comp=50), autofill=3)

IN = "Industrial"
preset(IN, "Rust Choir", "Struck sheet metal, chains and an anvil: the foundry kit.",
       ["Rust Choir"],
       K(dig("bd", FOUNDRY), dig("sd", FOUNDRY, senda=30), dig("fx", FOUNDRY), dig("tb", FOUNDRY), dig("cb", FOUNDRY), dig("rs", FOUNDRY),
         a_type=ROOM, a_size=70, a_return=-3, m_comp=50, m_tape=1, m_tape_drive=8), autofill=3)
preset(IN, "Pressure Plant", "A 12-step hat and a 7-step cowbell drifting against a straight kick.",
       ["Pressure Plant"],
       K(v("bd", model=4, decay=30, drive=50), v("sd", model=4, char1=70, drive=40), v("ch", model=3, char1=70), v("oh", model=3), v("cb", model=1, char1=70),
         a_type=NONLIN, a_gate=160, b_type=DIGI12, b_time=D_1_16, b_fb=40, b_return=-12, m_comp=60, m_lofi=1, m_lofi_mix=40))
preset(IN, "Scrap Ritual", "Toms of oil drums, a hydraulic hiss and the whole kit crushed to eight bits.",
       ["Scrap Ritual"],
       K(dig("bd", FOUNDRY), dig("sd", FOUNDRY), dig("t1", FOUNDRY, senda=30), dig("t2", FOUNDRY, senda=30), dig("t3", FOUNDRY), dig("t4", FOUNDRY), dig("fx", FOUNDRY),
         a_type=HALL, a_size=60, a_return=-6, m_lofi=4, m_lofi_mix=35, m_tape=1, m_tape_drive=10), autofill=2)
preset(IN, "Pressure Gauge", "Machine-gun kicks and ratcheted snares, distorted and bitcrushed per voice.",
       ["Broken Conveyor"],
       K(v("bd", model=1, decay=20, drive=60, bits=10), v("sd", model=4, char1=80, drive=50, bits=8, rate=16000), v("ch", model=1, bits=6), v("fx", model=4),
         a_type=GATED, a_gate=120, m_comp=60, m_ratio=2, m_attack=1), autofill=2)
preset(IN, "Broken Conveyor", "The broken conveyor on the foundry kit with a flanger turning slowly.",
       ["Broken Conveyor", "Rust Choir"],
       K(dig("bd", FOUNDRY, drive=40), dig("sd", FOUNDRY), dig("ch", FOUNDRY, sendb=50), dig("fx", FOUNDRY), dig("tb", FOUNDRY), dig("cb", FOUNDRY), dig("rs", FOUNDRY),
         b_type=FLANGER, b_rate=5, b_depth=80, b_fb=75, b_return=-3, a_type=ROOM, a_size=60, m_comp=55),
       chain=[[0, 0, 4, 1], [1, 0, 4, 1]])

NB = "New beat"
preset(NB, "Slow Leather", "Heavy and slow: gated clap-snare, a noise sweep opening each phrase.",
       ["Slow Leather"],
       K(v("bd", model=4, decay=45, char2=40), dig("sd", CRUNCH88, senda=40), v("ch", model=0), v("oh", model=0), v("fx", model=1, char2=85, decay=55), v("rs", model=0),
         a_type=GATED, a_gate=260, m_lofi=1, m_lofi_mix=50, m_comp=45), autofill=3)
preset(NB, "Low Countries Fog", "The 12-bit sampler kit, rimshots and a clap that lands like a door.",
       ["Low Countries Fog"],
       K(dig("bd", CRUNCH88), dig("cp", CRUNCH88, senda=35), dig("ch", CRUNCH88), dig("oh", CRUNCH88), dig("rs", CRUNCH88), dig("sd", CRUNCH88),
         a_type=GATED, a_gate=220, b_type=DIGI12, b_time=D_1_8D, b_fb=30, b_return=-12, m_comp=45), autofill=3)
preset(NB, "Acid Promenade", "Soft running hats, cowbell syncopation and a phaser on the whole hat bus.",
       ["Acid Promenade"],
       K(v("bd", model=1, decay=40), v("sd", model=0, senda=30), v("ch", model=0, sendb=60), v("oh", model=0, sendb=60), v("cb", model=0),
         a_type=GATED, a_gate=200, b_type=PHASER, b_rate=10, b_depth=80, b_fb=60, b_return=-3, m_comp=40), autofill=3)
preset(NB, "Heavy Velvet Rope", "Half-time snare in a long nonlinear room, the kick pushed through tape.",
       ["Dense Smoke"],
       K(v("bd", model=0, decay=60, char1=50), dig("sd", POP84, senda=55), v("ch", model=2), v("tb", model=1), v("fx", model=1, char2=20),
         a_type=NONLIN, a_gate=400, a_shape=-20, a_return=-2, m_tape=1, m_tape_drive=6, m_comp=40), autofill=2)
preset(NB, "Dense Smoke 108", "New beat chain: the leather pattern into the fog pattern and back.",
       ["Slow Leather", "Low Countries Fog"],
       K(dig("bd", CRUNCH88), dig("sd", CRUNCH88, senda=35), dig("cp", CRUNCH88, senda=30), dig("ch", CRUNCH88), dig("oh", CRUNCH88), dig("rs", CRUNCH88),
         v("fx", model=1, char2=85), a_type=GATED, a_gate=240, m_lofi=1, m_lofi_mix=60, m_comp=45),
       chain=[[0, 0, 4, 0], [0, 1, 4, 1], [1, 0, 4, 0], [1, 1, 4, 1]], tempo=108)

IT = "Italo / hi-NRG"
preset(IT, "Chrome Heartbeat", "Four on the floor, offbeat open hats and a hexagonal tom that says pew.",
       ["Chrome Heartbeat"],
       K(v("bd", model=1, decay=35), v("cp", model=1, senda=25), v("sd", model=1, senda=25), v("oh", model=0), v("ch", model=0),
         v("t2", model=1, char2=90, decay=55, tune=5, sendb=30), b_type=DIGI12, b_time=D_1_8D, b_fb=40, b_return=-8, a_type=PLATE, a_size=50, m_comp=40), autofill=3)
preset(IT, "Disco Phantom", "Tambourine sixteenths, cowbell, and the snare in a gated room.",
       ["Disco Phantom"],
       K(dig("bd", ELECTRO81), dig("sd", POP84, senda=40), dig("cp", ELECTRO81), dig("oh", ELECTRO81), dig("tb", STUDIO82), dig("cb", ELECTRO81),
         a_type=GATED, a_gate=240, m_comp=40, m_exciter=25), autofill=3)
preset(IT, "Pew Pew Romance", "Hexagonal toms in every gap, chorused, over a four-four kick.",
       ["Laser Gondola"],
       K(v("bd", model=1, decay=35), v("cp", model=0, senda=25), v("ch", model=0), v("t1", model=1, char2=85, sendb=40), v("t2", model=1, char2=85, sendb=40),
         v("t3", model=1, char2=85, sendb=40), v("t4", model=1, char2=85, sendb=40), b_type=CHORUS, b_rate=30, b_depth=60, b_return=-4,
         a_type=PLATE, a_size=50, m_comp=40), autofill=3)
preset(IT, "Starlight Cruiser", "Rim answer, open hats, a hall on the snare and the FM toms.",
       ["Starlight Cruiser"],
       K(dig("bd", FM85), dig("sd", FM85, senda=40), dig("oh", FM85), dig("ch", FM85), dig("rs", FM85), dig("tb", FM85),
         dig("t1", FM85), dig("t2", FM85), dig("t3", FM85), dig("t4", FM85), a_type=HALL, a_size=50, a_return=-4, m_comp=35), autofill=3)
preset(IT, "Hi-Energy Ghost", "Chain of the disco patterns, with a fill at the end of every phrase.",
       ["Disco Phantom", "Chrome Heartbeat"],
       K(v("bd", model=1, decay=35), v("sd", model=1, senda=35), v("cp", model=1), v("oh", model=0), v("ch", model=0), v("tb", model=1), v("cb", model=0),
         v("t2", model=1, char2=90, decay=55), a_type=GATED, a_gate=220, m_comp=40),
       chain=[[0, 0, 3, 0], [0, 1, 1, 1], [1, 0, 3, 0], [1, 1, 1, 1]])

WH = "Witch house"
preset(WH, "Hex Drag", "Slow and heavy, hat rolls stuttering, the whole thing at 11 kHz.",
       ["Hex Drag"],
       K(v("bd", model=4, decay=70, char2=70, drive=30), v("sd", model=4, senda=40), v("ch", model=0, decay=30), v("fx", model=3, decay=60),
         a_type=REVERSE, a_gate=400, a_return=-3, m_lofi=4, m_lofi_mix=45, m_tape=1, m_tape_drive=6), autofill=2)
preset(WH, "Candle Smoke Trap", "A clap on three, sub drops and a reverse room.",
       ["Candle Smoke"],
       K(v("bd", model=4, decay=80, char2=80), v("cp", model=1, senda=50), v("ch", model=0, decay=25), v("oh", model=0), v("fx", model=3, decay=70),
         a_type=REVERSE, a_gate=500, a_return=-2, m_lofi=3, m_lofi_mix=40), autofill=2)
preset(WH, "Occult Slowmo", "Half the tempo, double the room: everything in a long hall.",
       ["Occult Slowmo"],
       K(v("bd", model=4, decay=85), v("sd", model=2, senda=60), v("oh", model=1, sendb=40), v("ch", model=1), v("rd", model=2),
         a_type=HALL, a_size=90, a_return=-1, b_type=TAPE, b_time=D_1_4, b_fb=50, b_return=-8, m_tape=1), autofill=2)
preset(WH, "Pale Coven", "Hexagonal snare, crushed hats and a blip clave pinging into the tape echo.",
       ["Pale Coven"],
       K(dig("bd", HEXA84), dig("sd", HEXA84, senda=40), v("ch", model=0, bits=8, rate=12000), v("cl", model=2, sendb=60), v("fx", model=3),
         b_type=TAPE, b_time=D_1_8D, b_fb=55, b_rate=60, b_return=-4, a_type=PLATE, a_size=70, m_lofi=3, m_lofi_mix=30), autofill=2)
preset(WH, "Triangle Static", "Static bursts, sub drops and a coven of open hats.",
       ["Candle Smoke", "Hex Drag"],
       K(v("bd", model=4, decay=80), v("cp", model=1, senda=40), v("sd", model=4, senda=40), v("ch", model=0), v("oh", model=0), v("fx", model=4, char1=40, decay=50),
         a_type=REVERSE, a_gate=450, m_lofi=4, m_lofi_mix=40),
       chain=[[0, 0, 2, 0], [0, 1, 2, 1], [1, 0, 2, 0], [1, 1, 2, 1]])

TH = "90s darkwave / trip-hop"
preset(TH, "Rainy Breakbeat", "Swung break on the 12-bit sampler kit, ghost snares and dust.",
       ["Rainy Breakbeat"],
       K(dig("bd", CRUNCH88), dig("sd", CRUNCH88, senda=25), dig("ch", CRUNCH88), dig("oh", CRUNCH88), a_type=ROOM, a_size=45, m_lofi=1, m_lofi_mix=60,
         m_tape=1, m_tape_drive=4, m_comp=45), autofill=2)
preset(TH, "Velvet Dub Room", "Slow ride, rimshots and a tape echo with a lot of feedback.",
       ["Harbour Fog"],
       K(dig("bd", STUDIO91), dig("sd", STUDIO91, sendb=35), dig("rd", STUDIO91), dig("rs", STUDIO91, sendb=50), dig("t1", STUDIO91), dig("t2", STUDIO91),
         dig("t3", STUDIO91), dig("t4", STUDIO91), b_type=TAPE, b_time=D_1_8D, b_fb=60, b_rate=40, b_return=-3, a_type=SPRING, a_size=60, m_tape=1), autofill=3)
preset(TH, "Harbour Fog", "Twelve-bit 40 kHz kit, a ride swing and a plate.",
       ["Harbour Fog"],
       K(dig("bd", STUDIO91, chain=3), dig("sd", STUDIO91, chain=3, senda=30), dig("rd", STUDIO91, chain=3), dig("rs", STUDIO91, chain=3),
         a_type=PLATE, a_size=60, m_comp=35), autofill=2)
preset(TH, "Dusty Ritual Break", "A busy swung break with flams, through the 26 kHz sampler.",
       ["Dusty Ritual Break"],
       K(dig("bd", CRUNCH88), dig("sd", CRUNCH88, senda=20), dig("ch", CRUNCH88), dig("cb", CRUNCH88), m_lofi=1, m_lofi_mix=80, m_comp=50, m_tape=1), autofill=2)
preset(TH, "Sleepwalk Shuffle", "Shaker and kick, a lazy snare and a spring.",
       ["Sleepwalk Shuffle"],
       K(v("bd", model=0, decay=50), v("sd", model=0, char1=40, senda=35), v("tb", model=2), v("ch", model=0), v("rd", model=1),
         a_type=SPRING, a_size=60, m_tape=1, m_tape_drive=4), autofill=2)

DA = "Dark ambient"
preset(DA, "Cistern", "Single kicks, distant toms and static, all far away in a hall.",
       ["Cistern"],
       K(v("bd", model=0, decay=80, senda=20), v("t1", model=1, senda=70), v("t2", model=1, senda=70), v("t3", model=1, senda=70), v("t4", model=1, senda=70),
         v("fx", model=4, senda=60), v("cl", model=2, sendb=60), a_type=HALL, a_size=100, a_predelay=40, a_return=0,
         b_type=TAPE, b_time=D_1_4D, b_fb=55, b_return=-6), autofill=2)
preset(DA, "Distant Procession", "Marching toms under a crash that never quite arrives.",
       ["Procession"],
       K(dig("bd", STUDIO91, senda=30), dig("t1", STUDIO91, senda=60), dig("t2", STUDIO91, senda=60), dig("t3", STUDIO91, senda=60), dig("cy", STUDIO91, reverse=1),
         a_type=HALL, a_size=95, a_return=-1), autofill=2)
preset(DA, "Ash Choir Pulse", "A ride and a slow kick through the reverse room, metallic and patient.",
       ["Catacomb Drift"],
       K(v("rd", model=1, senda=50), v("bd", model=0, decay=85), v("fx", model=2, senda=50), v("cl", model=1),
         a_type=REVERSE, a_gate=700, a_return=-2, m_tape=1), autofill=2)
preset(DA, "Catacomb Drift", "Probability decides: nothing repeats, everything echoes.",
       ["Catacomb Drift", "Cistern"],
       K(dig("rd", FOUNDRY, senda=40), dig("bd", FOUNDRY), dig("fx", FOUNDRY, senda=50), dig("cl", FOUNDRY, sendb=50), dig("t1", FOUNDRY, senda=60), dig("t2", FOUNDRY, senda=60),
         a_type=HALL, a_size=90, b_type=TAPE, b_time=D_1_4, b_fb=60, b_return=-6),
       chain=[[0, 0, 4, 0], [1, 0, 4, 0]])
preset(DA, "Moth Light", "Shakers and claves on chance, a bell in the distance.",
       ["Moth Light"],
       K(v("tb", model=2, senda=40), v("cl", model=1, sendb=50), v("bd", model=0, decay=60), v("t1", model=1, senda=50), v("t2", model=1, senda=50),
         v("cb", model=2, senda=50), a_type=HALL, a_size=80, b_type=ENSEMBLE, b_depth=60, b_return=-8), autofill=2)

# La primera es la que carga el plugin al abrirse.
assert len(PRESETS) >= 64, len(PRESETS)
assert len(SETS) * 3 >= 150, len(SETS)


def main():
    # El volumen maestro de cada preset lo deja en el mismo nivel que los demás (tools/level_presets.py).
    lv = os.path.join(HERE, "preset_levels.json")
    if os.path.exists(lv):
        levels = json.load(open(lv))
        for p in PRESETS:
            if p["name"] in levels:
                p["params"]["m_volume"] = levels[p["name"]]
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(os.path.dirname(LIB), exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"):
            os.remove(os.path.join(OUT, f))
    names = set()
    for i, p in enumerate(PRESETS):
        assert p["name"] not in names, p["name"]
        names.add(p["name"])
        slug = re.sub(r"[^a-z0-9]+", "_", p["name"].lower()).strip("_")
        with open(os.path.join(OUT, f"p{i:02d}_{slug}.json"), "w") as fh:
            json.dump(p, fh, indent=1, ensure_ascii=False)
    with open(LIB, "w") as fh:
        json.dump(SETS, fh, indent=1, ensure_ascii=False)
    print(f"{len(PRESETS)} presets en {os.path.relpath(OUT)}; {len(SETS)} juegos ({3 * len(SETS)} patrones) en {os.path.relpath(LIB)}")


if __name__ == "__main__":
    main()
