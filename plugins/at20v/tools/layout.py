#!/usr/bin/env python3
"""Layout del panel AT20V: fuente única de verdad de posiciones de perillas, jacks y serigrafía.

Las coordenadas se midieron sobre la foto CC0 de alta resolución del MS-20 original
(docs/ref/ms20_original_full_CC0.jpg, 7875x5296) con grilla superpuesta. Se escriben en
píxeles de esa foto y se convierten a "unidades de panel": el panel mide 1600 x PANEL_H.

Salidas:
  assets/layout.json             (para Blender y para la serigrafía)
  Source/ui/Layout.generated.h   (para el editor JUCE)
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Bordes del panel en la foto.
PX0, PY0, PX1, PY1 = 1140, 1010, 6496, 2760
S = 1600.0 / (PX1 - PX0)
PANEL_W = 1600.0
PANEL_H = round((PY1 - PY0) * S, 1)

def P(x, y):
    return [round((x - PX0) * S, 1), round((y - PY0) * S, 1)]

def L(v):
    return round(v * S, 1)

# Escalas de perilla: lista de (ángulo_normalizado 0..1, texto).
N010 = [(i / 10, str(i)) for i in range(11)]
PM5 = [(i / 10, "0" if i == 5 else (f"+{i - 5}" if i > 5 else f"−{5 - i}")) for i in range(11)]
TICKS10 = [(i / 10, "") for i in range(11)]

# Tipos: big = selectores y cortes (Ø grande), small = el resto.
KNOBS = [
    # id, foto x, foto y, tipo, etiqueta, escala, pasos (0 = continuo)
    ("vco1_wave",   1365, 1330, "big",   "WAVE FORM", "vco1wave", 4),
    ("vco1_pw",     1390, 1715, "small", "PW",        "pw", 0),
    ("vco1_scale",  1370, 2100, "big",   "SCALE",     "scale1", 4),
    ("vco2_wave",   1790, 1330, "big",   "WAVE FORM", "vco2wave", 4),
    ("vco2_pitch",  1800, 1715, "small", "PITCH",     "pm5", 0),
    ("vco2_scale",  1790, 2100, "big",   "SCALE",     "scale2", 4),
    ("portamento",  1390, 2560, "small", "TIME",      "n010", 0),
    ("master_tune", 1800, 2560, "small", "",          "pm5", 0),
    ("vco1_level",  2195, 1340, "small", "VCO 1 LEVEL", "n010", 0),
    ("vco2_level",  2200, 1765, "small", "VCO 2 LEVEL", "n010", 0),
    ("fm_mg",       2200, 2275, "small", "MG/T.EXT",  "n010", 0),
    ("fm_eg1",      2205, 2565, "small", "EG1/EXT",   "n010", 0),
    ("hpf_cutoff",  2565, 1335, "big",   "CUTOFF FREQUENCY", "n010", 0),
    ("hpf_peak",    2570, 1760, "small", "PEAK",      "n010", 0),
    ("hpf_mod_mg",  2570, 2270, "small", "MG/T.EXT",  "n010", 0),
    ("hpf_mod_eg2", 2570, 2565, "small", "EG2/EXT",   "n010", 0),
    ("lpf_cutoff",  2940, 1335, "big",   "CUTOFF FREQUENCY", "n010", 0),
    ("lpf_peak",    2943, 1760, "small", "PEAK",      "n010", 0),
    ("lpf_mod_mg",  2945, 2270, "small", "MG/T.EXT",  "n010", 0),
    ("lpf_mod_eg2", 2947, 2565, "small", "EG2/EXT",   "n010", 0),
    ("mg_wave",     3310, 2270, "small", "WAVE FORM", "mgwave", 0),
    ("mg_freq",     3317, 2565, "small", "FREQUENCY", "n010", 0),
    ("eg1_delay",   3690, 1978, "small", "DELAY TIME",   "n010", 0),
    ("eg1_attack",  3690, 2272, "small", "ATTACK TIME",  "n010", 0),
    ("eg1_release", 3690, 2567, "small", "RELEASE TIME", "n010", 0),
    ("eg2_hold",    4060, 1338, "small", "HOLD TIME",    "n010", 0),
    ("eg2_attack",  4060, 1683, "small", "ATTACK TIME",  "n010", 0),
    ("eg2_decay",   4060, 1978, "small", "DECAY TIME",   "n010", 0),
    ("eg2_sustain", 4060, 2272, "small", "SUSTAIN LEVEL", "n010", 0),
    ("eg2_release", 4060, 2567, "small", "RELEASE TIME", "n010", 0),
    ("volume",      5972, 1293, "big",   "",          "volume", 0),
    ("esp_level",   4437, 2565, "small", "SIGNAL LEVEL",    "n010", 0),
    ("esp_lowcut",  4729, 2565, "small", "LOW CUT FREQ",    "n010", 0),
    ("esp_highcut", 5026, 2565, "small", "HIGH CUT FREQ",   "n010", 0),
    ("esp_cvadj",   5309, 2565, "small", "CV ADJUST",       "n010", 0),
    ("esp_thresh",  5610, 2569, "small", "THRESHOLD LEVEL", "n010", 0),
]

# Jacks: id, foto x, foto y, dirección (in/out), etiqueta superior, etiqueta inferior.
JACKS = [
    ("total_in",      4393, 1378, "in",  "TOTAL",          "−5V~+5V"),
    ("freq_in",       4673, 1378, "in",  "FREQ",           "−5V~+5V"),
    ("ext_sig_in",    4902, 1378, "in",  "EXT SIGNAL IN",  "3VPP MAX"),
    ("hpf_cut_in",    5123, 1378, "in",  "CUTOFF FREQ",    "−5V~+5V"),
    ("lpf_cut_in",    5369, 1378, "in",  "CUTOFF FREQ",    "−5V~+5V"),
    ("gain_in",       5632, 1378, "in",  "INITIAL GAIN",   "0V~+5V"),
    ("signal_out",    6287, 1182, "out", "SIGNAL OUT",     ""),
    ("phones_out",    6287, 1378, "out", "PHONES",         ""),
    ("mg_tri_out",    4393, 1633, "out", "OUT",            ""),
    ("mg_sq_out",     4783, 1633, "out", "OUT",            ""),
    ("eg1_out",       4975, 1633, "out", "OUT",            ""),
    ("eg2_rev_out",   5738, 1633, "out", "REV OUT",        ""),
    ("vco2_cv_in",    5900, 1733, "in",  "VCO 2 CV IN",    ""),
    ("vco12_cv_in",   6090, 1633, "in",  "VCO 1+2 CV IN",  ""),
    ("kbd_cv_out",    6285, 1633, "out", "KBD CV OUT",     ""),
    ("sh_clock_in",   4588, 1831, "in",  "CLOCK",          ""),
    ("eg1_rev_out",   4975, 1831, "out", "REV OUT",        ""),
    ("vca_ctrl_in",   5170, 1831, "in",  "CONTROL IN PUT", "0V~+5V"),
    ("eg1_trig_in",   5480, 1831, "in",  "EG 1 TRIG IN",   ""),
    ("kbd_trig_in",   6090, 1831, "in",  "TRIG IN",        ""),
    ("kbd_trig_out",  6283, 1831, "out", "TRIG OUT",       ""),
    ("sh_in",         4393, 2028, "in",  "IN",             ""),
    ("sh_out",        4780, 2028, "out", "OUT",            ""),
    ("vca_in",        4973, 2028, "in",  "IN",             ""),
    ("vca_out",       5278, 2028, "out", "OUT",            ""),
    ("pink_out",      5480, 2029, "out", "PINK",           ""),
    ("white_out",     5869, 2029, "out", "WHITE",          ""),
    ("wheel_out",     6083, 2029, "out", "",               ""),
    ("msw_out",       6280, 2029, "out", "",               ""),
    ("esp_in",        4389, 2300, "in",  "SIGNAL IN",      ""),
    ("esp_amp_out",   4780, 2300, "out", "OUT",            ""),
    ("esp_bpf_out",   5172, 2300, "out", "OUT",            ""),
    ("esp_fv_out",    5567, 2300, "out", "F∝V CV OUT",     ""),
    ("esp_env_out",   6083, 2300, "out", "ENV OUT",        ""),
    ("esp_trig_out",  6278, 2300, "out", "TRIG OUT",       ""),
]

# Agujeros de trimmers (decorativos) y LEDs.
HOLES = [(1540, 1870), (1955, 1870), (1540, 2270), (1955, 2270), (1540, 2665), (1955, 2665),
         (2440, 1590), (2815, 1595)]
LEDS = [("mg_led", 3317, 1978), ("eg_led", 3870, 1780), ("esp_peak_led", 5829, 2178),
        ("esp_trig_led", 6277, 2175)]
SCREWS = [(1197, 1213), (1210, 2582), (3500, 1135), (6332, 1213), (6362, 2590)]

# Serigrafía: títulos de sección (texto, x, y centro, tamaño en px de foto, alineación).
TITLES = [
    ("VOLTAGE CONTROLLED\nOSCILLATOR   1", 1200, 1088, 38, "left"),
    ("VOLTAGE CONTROLLED\nOSCILLATOR   2", 1612, 1088, 38, "left"),
    ("VCO  MIXER", 2190, 1072, 38, "center"),
    ("VOLTAGE CONTROLLED\nHIGHPASS FILTER", 2392, 1090, 38, "left"),
    ("VOLTAGE CONTROLLED\nLOWPASS FILTER", 2762, 1090, 38, "left"),
    ("VOLTAGE  CONTROLLED  AMPLIFIER", 3500, 1072, 38, "center"),
    ("ENVELOPE\nGENERATOR 2", 3946, 1095, 38, "left"),
    ("PORTAMENTO", 1395, 2370, 30, "center"),
    ("MASTER  TUNE", 1810, 2370, 30, "center"),
    ("FREQUENCY\nMODULATION", 2200, 2042, 27, "center"),
    ("CUTOFF  FREQUENCY\nMODULATION", 2420, 2042, 27, "left"),
    ("CUTOFF  FREQUENCY\nMODULATION", 2812, 2042, 27, "left"),
    ("MODULATION\nGENERATOR", 3210, 1700, 38, "left"),
    ("ENVELOPE\nGENERATOR 1", 3577, 1700, 38, "left"),
    ("EG 2 & EXT", 3500, 1513, 30, "center"),
    ("EXTERNAL  SIGNAL  PROCESSOR", 4790, 2170, 40, "center"),
    ("VOLUME", 5985, 1090, 30, "center"),
    ("POWER  OFF", 6035, 1484, 34, "center"),
]

# Líneas divisorias de sección (polilíneas en px de foto), ancho ~8 px.
DIVIDERS = [
    [(1597, 1050), (1597, 2300)],
    [(2012, 1050), (2012, 2730)],
    [(2385, 1060), (2385, 1950)],
    [(2755, 1060), (2755, 1950)],
    [(3128, 1060), (3128, 1625), (3875, 1625), (3875, 1060)],
    [(2010, 1970), (3128, 1970), (3128, 1625)],
    [(2395, 2000), (2395, 2730)],
    [(2758, 2000), (2758, 2730)],
    [(3128, 2000), (3128, 2730)],
    [(3503, 1650), (3503, 2730)],
    [(3875, 1650), (3875, 2730)],
    [(1185, 2320), (1995, 2320)],
    [(1600, 2345), (1600, 2730)],
    [(4242, 1060), (4242, 2730)],
    [(4265, 2120), (6430, 2120)],
]

def scale_for(name):
    if name == "n010": return N010
    if name == "pm5": return PM5
    if name == "volume": return [(i / 10, str(i)) for i in range(1, 11)]
    if name == "scale1": return [(0.0, "32′"), (1/3, "16′"), (2/3, "8′"), (1.0, "4′")]
    if name == "scale2": return [(0.0, "16′"), (1/3, "8′"), (2/3, "4′"), (1.0, "2′")]
    if name == "vco1wave": return [(0.0, "sym:tri"), (1/3, "sym:saw"), (2/3, "sym:pwm"), (1.0, "sym:noise")]
    if name == "vco2wave": return [(0.0, "sym:saw"), (1/3, "sym:square"), (2/3, "sym:pulse"), (1.0, "RING")]
    if name == "pw": return [(0.0, "sym:square"), (1.0, "sym:line")] + [(i / 10, "") for i in range(11)]
    if name == "mgwave": return [(0.0, "sym:mgleft"), (0.5, "sym:mgtop"), (1.0, "sym:mgright")] + [(i / 10, "") for i in range(11)]
    raise KeyError(name)

KNOB_RADIUS = {"big": L(118), "small": L(86)}   # radio del faldón
JACK_RADIUS = L(72)                              # radio de la tuerca hexagonal
# Recorrido angular de las perillas: 300°, con 0 abajo a la izquierda (como el MS-20).
KNOB_START_DEG, KNOB_END_DEG = -150.0, 150.0
# Selectores: los cuatro pasos ocupan sólo el arco superior (−50°..+50°).
SELECTOR_START_DEG, SELECTOR_END_DEG = -50.0, 50.0

def build():
    knobs = []
    for kid, x, y, kind, label, scale, steps in KNOBS:
        sel = steps > 0
        knobs.append(dict(id=kid, pos=P(x, y), kind=kind, label=label, steps=steps,
                          radius=KNOB_RADIUS[kind],
                          startDeg=SELECTOR_START_DEG if sel else KNOB_START_DEG,
                          endDeg=SELECTOR_END_DEG if sel else KNOB_END_DEG,
                          scale=[dict(t=t, text=s) for t, s in scale_for(scale)]))
    jacks = [dict(id=j, pos=P(x, y), dir=d, top=t, bottom=b) for j, x, y, d, t, b in JACKS]
    return dict(
        panel=dict(w=PANEL_W, h=PANEL_H, photoScale=S),
        knobs=knobs, jacks=jacks, jackRadius=JACK_RADIUS,
        holes=[P(x, y) for x, y in HOLES],
        leds=[dict(id=i, pos=P(x, y)) for i, x, y in LEDS],
        screws=[P(x, y) for x, y in SCREWS],
        titles=[dict(text=t, pos=P(x, y), size=L(sz), align=a) for t, x, y, sz, a in TITLES],
        dividers=[[P(x, y) for x, y in d] for d in DIVIDERS],
    )

def write_header(data, path):
    lines = ["#pragma once", "// Generado por tools/layout.py — no editar a mano.", "",
             "namespace dh::layout {", "",
             f"constexpr float panelW = {data['panel']['w']}f, panelH = {data['panel']['h']}f;",
             f"constexpr float jackRadius = {data['jackRadius']}f;", "",
             "struct KnobDef { const char* id; float x, y, radius, startDeg, endDeg; int steps; bool big; };",
             "struct JackDef { const char* id; float x, y; bool isOutput; };",
             "struct LedDef { const char* id; float x, y; };", "",
             "enum class KnobId : int {"]
    lines += [f"    {k['id']}," for k in data["knobs"]] + ["    Count", "};", "",
              "enum class JackId : int {"]
    lines += [f"    {j['id']}," for j in data["jacks"]] + ["    Count", "};", "",
              "inline constexpr KnobDef knobs[] = {"]
    for k in data["knobs"]:
        lines.append(f'    {{ "{k["id"]}", {k["pos"][0]}f, {k["pos"][1]}f, {k["radius"]}f, '
                     f'{k["startDeg"]}f, {k["endDeg"]}f, {k["steps"]}, {"true" if k["kind"] == "big" else "false"} }},')
    lines += ["};", "", "inline constexpr JackDef jacks[] = {"]
    for j in data["jacks"]:
        lines.append(f'    {{ "{j["id"]}", {j["pos"][0]}f, {j["pos"][1]}f, {"true" if j["dir"] == "out" else "false"} }},')
    lines += ["};", "", "inline constexpr LedDef leds[] = {"]
    for l in data["leds"]:
        lines.append(f'    {{ "{l["id"]}", {l["pos"][0]}f, {l["pos"][1]}f }},')
    lines += ["};", "", "} // namespace dh::layout", ""]
    open(path, "w").write("\n".join(lines))


# ───────────────────────── Página EXTENSIONS (diseño propio, unidades de panel) ─────────────────────────
# Cinco secciones en columnas, tres filas de perillas. Selectores con su rótulo de opciones.
EXT_ROWS = [118.0, 280.0, 442.0]
def ext_col(x0, i): return x0 + 60.0 + i * 118.0
# (param, sección x0, col, fila, rótulo, pasos, escala)
EXT_KNOBS = [
    ("voc_on",      28, 0, 0, "VOCODER",      2,  "offon"),
    ("voc_bands",   28, 1, 0, "BANDS",        4,  "bands"),
    ("voc_gain",    28, 2, 0, "INPUT LEVEL",  0,  "n010"),
    ("voc_shift",   28, 0, 1, "FORMANT",      0,  "pm5"),
    ("voc_q",       28, 1, 1, "RESONANCE",    0,  "n010"),
    ("voc_hiss",    28, 2, 1, "SIBILANCE",    0,  "n010"),
    ("voc_attack",  28, 0, 2, "ATTACK",       0,  "n010"),
    ("voc_release", 28, 1, 2, "RELEASE",      0,  "n010"),
    ("voc_mix",     28, 2, 2, "MIX",          0,  "n010"),
    ("voices",      420, 0, 0, "VOICES",      6,  "voices"),
    ("unison",      420, 1, 0, "UNISON",      4,  "unison"),
    ("detune",      420, 0, 1, "DETUNE",      0,  "n010"),
    ("spread",      420, 1, 1, "SPREAD",      0,  "n010"),
    ("priority",    420, 0, 2, "PRIORITY",    3,  "prio"),
    ("retrigger",   420, 1, 2, "RETRIGGER",   2,  "offon"),
    ("dly_on",      695, 0, 0, "DELAY",       2,  "offon"),
    ("dly_time",    695, 1, 0, "TIME",        12, "dtime"),
    ("dly_fb",      695, 0, 1, "FEEDBACK",    0,  "n010"),
    ("dly_tone",    695, 1, 1, "TONE",        0,  "n010"),
    ("dly_mix",     695, 0, 2, "MIX",         0,  "n010"),
    ("rev_on",      970, 0, 0, "REVERB",      2,  "offon"),
    ("rev_size",    970, 1, 0, "SIZE",        0,  "n010"),
    ("rev_damp",    970, 0, 1, "DAMPING",     0,  "n010"),
    ("rev_mix",     970, 1, 1, "MIX",         0,  "n010"),
    ("filter_rev",  1245, 0, 0, "FILTER",     2,  "rev"),
    ("analog",      1245, 1, 0, "ANALOG",     0,  "n010"),
    ("mg_sync",     1245, 2, 0, "MG CLOCK",   2,  "sync"),
    ("bend_range",  1245, 0, 1, "BEND RANGE", 13, "bend"),
    ("quality",     1245, 1, 1, "OVERSAMPLING", 2, "os"),
    ("midi_out",    1245, 2, 1, "MIDI OUT",   4,  "midiout"),
    ("output",      1245, 1, 2, "OUTPUT",     0,  "outdb"),
]
EXT_SECTIONS = [  # (título, x0, x1)
    ("VOCODER", 28, 404), ("POLYPHONY", 420, 680), ("DELAY", 695, 955), ("REVERB", 970, 1230), ("SYSTEM", 1245, 1585),
]

def ext_scale(name, steps):
    if name == "n010": return N010
    if name == "pm5": return PM5
    if name == "outdb": return [(0.0, "−24"), (0.8, "0"), (1.0, "+6")] + TICKS10
    opts = {
        "offon": ["OFF", "ON"], "bands": ["8", "12", "16", "20"], "voices": ["MONO", "2", "4", "6", "8", "12"],
        "unison": ["1", "2", "3", "4"], "prio": ["LAST", "HIGH", "LOW"], "rev": ["REV 1", "REV 2"],
        "sync": ["FREE", "TEMPO"], "os": ["4x", "8x"], "midiout": ["OFF", "THRU", "PATCH", "BOTH"],
    }
    if name in opts:
        o = opts[name]; n = len(o)
        return [(i / (n - 1), t) for i, t in enumerate(o)]
    if name == "dtime":
        names = ["1/32", "", "1/16", "", "", "1/8", "", "", "1/4", "", "1/2", "1 BAR"]
        return [(i / 11, t) for i, t in enumerate(names)]
    if name == "bend":
        return [(i / 12, str(i) if i % 2 == 0 else "") for i in range(13)]
    raise KeyError(name)

def ext_angles(steps):
    if steps == 0 or steps > 6: return KNOB_START_DEG, KNOB_END_DEG
    span = {2: 80.0, 3: 100.0, 4: 120.0, 5: 130.0, 6: 140.0}[steps]
    return -span / 2, span / 2

def build_ext():
    knobs = []
    for pid, x0, col, row, label, steps, scale in EXT_KNOBS:
        a0, a1 = ext_angles(steps)
        knobs.append(dict(id=pid, pos=[round(ext_col(x0, col), 1), EXT_ROWS[row]], kind="small", label=label,
                          steps=steps, radius=KNOB_RADIUS["small"], startDeg=a0, endDeg=a1,
                          scale=[dict(t=t, text=s) for t, s in ext_scale(scale, steps)]))
    titles = [dict(text=t, pos=[(x0 + x1) / 2, 34.0], size=L(38), align="center") for t, x0, x1 in EXT_SECTIONS]
    dividers = []
    for t, x0, x1 in EXT_SECTIONS[1:]:
        dividers.append([[x0 - 8.0, 18.0], [x0 - 8.0, PANEL_H - 18.0]])
    dividers.append([[20.0, 58.0], [PANEL_W - 20.0, 58.0]])
    return dict(panel=dict(w=PANEL_W, h=PANEL_H, photoScale=S), knobs=knobs, jacks=[], jackRadius=JACK_RADIUS,
                holes=[], leds=[dict(id="voc_led", pos=[384.0, 34.0])],
                screws=[[16.0, 16.0], [PANEL_W - 16.0, 16.0], [16.0, PANEL_H - 16.0], [PANEL_W - 16.0, PANEL_H - 16.0]],
                titles=titles, dividers=dividers)

def write_ext_header(ext, path):
    lines = ["#pragma once", "// Generado por tools/layout.py — no editar a mano.", "", "namespace dh::layout {", "",
             "struct ExtKnobDef { const char* param; float x, y, radius, startDeg, endDeg; int steps; };", "",
             "inline constexpr ExtKnobDef extKnobs[] = {"]
    for k in ext["knobs"]:
        lines.append(f'    {{ "{k["id"]}", {k["pos"][0]}f, {k["pos"][1]}f, {k["radius"]}f, {k["startDeg"]}f, {k["endDeg"]}f, {k["steps"]} }},')
    lines += ["};", "", f'inline constexpr float extLedX = {ext["leds"][0]["pos"][0]}f, extLedY = {ext["leds"][0]["pos"][1]}f;',
              "", "} // namespace dh::layout", ""]
    open(path, "w").write("\n".join(lines))

if __name__ == "__main__":
    data = build()
    os.makedirs(os.path.join(ROOT, "assets"), exist_ok=True)
    json.dump(data, open(os.path.join(ROOT, "assets", "layout.json"), "w"), indent=1, ensure_ascii=False)
    write_header(data, os.path.join(ROOT, "Source", "ui", "Layout.generated.h"))
    ext = build_ext()
    json.dump(ext, open(os.path.join(ROOT, "assets", "layout_ext.json"), "w"), indent=1, ensure_ascii=False)
    write_ext_header(ext, os.path.join(ROOT, "Source", "ui", "LayoutExt.generated.h"))
    print(f"panel {PANEL_W} x {PANEL_H}, {len(data['knobs'])} perillas, {len(data['jacks'])} jacks")
