"""Contenido del Sample Pack: qué se toca, con qué preset y cómo se mojan los loops.

Todo es original (MIDI propio o los patrones de fábrica de los presets) y suena con nuestros motores.
Un loop de teclado es una lista de compases de 16 pasos (ver samplepack_lib.song_from).
"""
from samplepack_lib import *

R16 = " ".join(["."] * 16)
def P(s):
    assert len(s.split()) == 16, f"{len(s.split())} pasos: {s}"
    return s
def tr(bars, n): return [transpose(b, n) for b in bars]

SYNTHS = []
def synth(**kw):
    kw.setdefault("oneshots", []); kw.setdefault("loops", []); kw.setdefault("fx", [])
    SYNTHS.append(kw); return kw

# One-shot: (etiqueta, preset, notas, velocidades, sostenido s, cola s, args extra)
def OS(label, preset, notes, vels=(100,), hold=1.0, tail=2.5, extra=(), plain=False):
    return dict(label=label, preset=preset, notes=list(notes), vels=list(vels), hold=hold, tail=tail, extra=list(extra), plain=plain)

# Loop de teclado. parts = [(preset, compases, ganancia dB)], tail = cola que se pliega.
def LM(id, slug, parts, bpm, key, bars, swing=0.0, gate=0.9, vel=100, extra=(), tail=3.5, en="", es="", mode="midi", **kw):
    return dict(id=id, slug=slug, parts=parts, bpm=bpm, key=key, nbars=bars, swing=swing, gate=gate, vel=vel, extra=list(extra),
                tail=tail, en=en, es=es, mode=mode, **kw)

# Loop de secuenciador propio del plugin (AT303, Pallor, DeviceNTI, Thrum): args del render CLI.
def LC(id, slug, args, bpm, key, bars, tail=3.0, en="", es="", **kw):
    return dict(id=id, slug=slug, args=list(args), bpm=bpm, key=key, nbars=bars, tail=tail, en=en, es=es, mode="cli", **kw)

# Efectos: (origen, motor, preset, etiqueta, descripción EN, ES). origen = ("loop", id) | ("os", archivo sin extensión) | ("tex", id)
def FX(src, kind, preset, tag, en, es): return dict(src=src, kind=kind, preset=preset, tag=tag, en=en, es=es)

HAL = "halation"; TAR = "tarnish"; MUR = "murmur"

# ═════════════════════════ AT20V (Korg MS-20) ═════════════════════════
_acid = [P("A1! . A2 A1 . A1~ C2 A1 D2! . . E2 A1 G2 A1 A2")]
_acid_b = [P("A1! . A2 A1 . A1~ C2 A1 D2! . . E2 A1 G2! A1 A2")]
_kick = P("C2! . . . C2 . . . C2! . . . C2 . . .")
_kick_f = P("C2! . . . C2 . . . C2! . . . C2 . C2~ C2")
_snare = P(". . . . C3! . . . . . . . C3! . . .")
_snare_f = P(". . . . C3! . . . . . C3~ . C3! C3~ C3 C3!")
_hat = P(". . C4 . . . C4 . . . C4 . . . C4 C4~")
synth(
    slug="at20v", name="AT20V", folder="AT20V", kind="synth",
    tag=dict(en="Semi-modular monosynth: screaming filters, acid, hoover and drones", es="Monosintetizador semimodular: filtros que gritan, acid, hoover y drones"),
    genres="acid, EBM, techno, ambient",
    oneshots=[
        OS("AcidBass", "15_acid", ["A1", "C2", "E2", "A2"], [100], 0.8, 1.2),
        OS("AcidBass", "15_acid", ["A1"], [55], 0.8, 1.2),
        OS("FatSawBass", "16_fat_saw", ["E1", "A1", "E2"], [100], 1.0, 1.5),
        OS("ScreamLead", "18_scream", ["A3", "E4", "A4"], [105], 1.4, 2.0),
        OS("ResoPluck", "19_reso", ["A3", "C4", "E4", "A4"], [100], 0.4, 1.8),
        OS("RingBell", "21_ring", ["C5", "E5", "G5"], [100], 0.5, 3.5),
        OS("GlassPad", "39_glass", ["A3"], [90], 3.0, 3.5),
        OS("Kick", "22_ms_kick", ["C2"], [70, 115], 0.3, 0.8),
        OS("Snare", "23_ms_snare", ["C3"], [70, 115], 0.3, 0.8),
        OS("HiHat", "24_ms_hi", ["C4"], [70, 115], 0.2, 0.5),
        OS("LaserZap", "27_laser", ["C4"], [100], 0.8, 1.2),
    ],
    loops=[
        LM("acid", "at20v", [("15_acid", [transpose(b, t) for b, t in zip(_acid + _acid_b + _acid + _acid_b, (0, 0, -4, -2))], 0)], 124, "Am", 4, gate=0.85,
           en="Acid line in A minor, chord roots A A F G.", es="Línea acid en La menor, raíces La La Fa Sol."),
        LM("fatbass", "at20v", [("16_fat_saw", [P("F1! . . F2 . . F1 . F2! . . F1 . . F2 ."), P("F1! . . F2 . . F1 . F2! . . F1 . Ab1 Ab2 ."), P("F1! . . F2 . . F1 . F2! . . F1 . . F2 ."), P("Eb1! . . Eb2 . . Eb1 . Eb2! . . Eb1 . Bb1 Bb2 .")], 0)], 122, "Fm", 4, gate=0.8,
           en="House bass with octave jumps, F minor.", es="Bajo house con saltos de octava, Fa menor."),
        LM("hoover", "at20v", [("17_hoover", [P("D2! . D3~ . D2 . F3 . D2 D3~ . C3 . . . ."), P("D2! . D3~ . D2 . F3 . D2 D3~ . C3 . . . ."), P("F2! . F3~ . F2 . A3 . F2 F3~ . Eb3 . . . ."), P("C2! . C3~ . C2 . E3 . C2 C3~ . Bb2 . . . .")], 0)], 140, "Dm", 4, gate=0.8,
           en="Hoover riff in octaves and fifths, D minor.", es="Riff hoover en octavas y quintas, Re menor."),
        LM("ringbells", "at20v", [("21_ring", [P("C4 Eb4 G4 Bb4 C5 Bb4 G4 Eb4 C4 Eb4 G4 Bb4 C5 Bb4 G4 Eb4"), P("Ab3 C4 Eb4 G4 Ab4 G4 Eb4 C4 Ab3 C4 Eb4 G4 Ab4 G4 Eb4 C4"), P("Eb4 G4 Bb4 D5 Eb5 D5 Bb4 G4 Eb4 G4 Bb4 D5 Eb5 D5 Bb4 G4"), P("Bb3 D4 F4 A4 Bb4 A4 F4 D4 Bb3 D4 F4 A4 Bb4 A4 F4 D4")], 0)], 100, "Cm", 4, vel=88, gate=0.7, tail=4.5,
           en="Bell arpeggio over Cm, Ab, Eb, Bb.", es="Arpegio de campanas sobre Cm, Ab, Eb, Bb."),
        LM("glasspad", "at20v", [("39_glass", [P("A2+E3+A3+C4+E4:16 - - - - - - - - - - - - - - -"), P("F2+C3+F3+A3+C4:16 - - - - - - - - - - - - - - -"), P("C3+G3+C4+E4+G4:16 - - - - - - - - - - - - - - -"), P("G2+D3+G3+B3+E4:16 - - - - - - - - - - - - - - -")], 0)], 70, "Am", 4, gate=0.98, vel=84, tail=5.0,
           en="Slow pad: Am9, Fmaj7, Cmaj7, G6sus.", es="Pad lento: Am9, Fmaj7, Cmaj7, G6sus."),
        LM("fogdrone", "at20v", [("40_analog_fog", [P("A1+E2+A2:64 - - - - - - - - - - - - - - -")] + [R16] * 3, 0)], 60, "A", 4, gate=0.99, vel=90, tail=6.0,
           en="Drone in A, four bars of fog.", es="Drone en La, cuatro compases de niebla."),
        LM("drums", "at20v", [("22_ms_kick", [_kick, _kick, _kick, _kick_f], 0), ("23_ms_snare", [_snare, _snare, _snare, _snare_f], -2), ("24_ms_hi", [_hat] * 4, -8)], 130, "", 4, gate=0.5, tail=2.0,
           en="Kick, snare and hats from three AT20V patches, EBM feel.", es="Bombo, caja y charles de tres parches del AT20V, sentir EBM."),
    ],
    fx=[
        FX(("loop", "acid"), HAL, "p03", "HalPingPong", "Acid line through a ping-pong delay.", "Línea acid con un eco ping-pong."),
        FX(("loop", "acid"), TAR, "p06", "TarCassette", "Acid line on a dusty cassette.", "Línea acid en un casete gastado."),
        FX(("loop", "hoover"), HAL, "p05", "HalSmallRoom", "Hoover riff in a small room.", "Riff hoover en una sala chica."),
        FX(("loop", "ringbells"), HAL, "p08", "HalCathedral", "Bells in a glass cathedral.", "Campanas en una catedral de vidrio."),
        FX(("loop", "glasspad"), TAR, "p00", "TarStudioTape", "Pad through warm studio tape.", "Pad por cinta de estudio cálida."),
        FX(("loop", "drums"), TAR, "p03", "TarHotDrums", "Drums hitting hot tape.", "Batería contra cinta saturada."),
        FX(("os", "AT20V_ScreamLead_A3_v105"), HAL, "p06", "HalHall", "Scream lead in a concert hall.", "Lead que grita en una sala de conciertos."),
        FX(("os", "AT20V_RingBell_E5_v100"), HAL, "p26", "HalReverseShimmer", "Ring bell with a reverse shimmer.", "Campana con un shimmer invertido."),
        FX(("tex", "glasspad"), MUR, "p05", "MurChoirRising", "Pad turned into a rising grain cloud.", "Pad convertido en una nube de granos que sube."),
        FX(("tex", "ringbells"), MUR, "p03", "MurStarlings", "Bells scattered as a starling cloud.", "Campanas dispersas como una bandada."),
    ],
)

# ═════════════════════════ AT101V (Roland SH-101) ═════════════════════════
_k = P("C2! . . . C2 . . . C2! . . . C2 . . .")
synth(
    slug="at101v", name="AT101V", folder="AT101V", kind="synth",
    tag=dict(en="Monosynth: basses, leads and sequences", es="Monosintetizador: bajos, leads y secuencias"),
    genres="house, minimal techno, acid, italo",
    oneshots=[
        OS("101Bass", "01_101_bass", ["F1", "C2", "F2"], [100], 0.7, 1.0),
        OS("PulseBass", "02_pulse_bass", ["F1", "Ab1", "C2"], [100], 0.7, 1.0),
        OS("SubRumble", "03_sub_rumble", ["C1", "F1"], [100], 1.5, 1.5),
        OS("ClassicLead", "09_classic_lead", ["C4", "Eb4", "G4", "C5"], [100], 1.0, 1.5),
        OS("Whistle", "10_whistle", ["G4", "D5"], [96], 1.5, 1.5),
        OS("PolyPluck", "38_poly_pluck", ["C3", "G3", "C4"], [100], 0.3, 1.5),
        OS("GlassBell", "31_glass_bells", ["C5", "G5"], [100], 0.5, 3.0),
        OS("SoftPad", "34_soft_pad", ["C3"], [90], 3.0, 3.0),
        OS("ZapTom", "40_zap_tom", ["C3"], [70, 115], 0.3, 0.8),
        OS("NoiseSnare", "41_noise_snare", ["C3"], [70, 115], 0.3, 0.8),
        OS("Laser", "42_laser", ["C4"], [100], 0.8, 1.0),
    ],
    loops=[
        LM("housebass", "at101v", [("01_101_bass", [P("F1! . . F2 . F1 . F2 . . F1 . F2 . C2 ."), P("F1! . . F2 . F1 . F2 . . F1 . F2 . C2 ."), P("Ab1! . . Ab2 . Ab1 . Ab2 . . Ab1 . Ab2 . Eb2 ."), P("Eb1! . . Eb2 . Eb1 . Eb2 . . Eb1 . Eb2 . Bb1 .")], 0)], 122, "Fm", 4, gate=0.55,
           en="Offbeat house bass, F minor.", es="Bajo house a contratiempo, Fa menor."),
        LM("acidline", "at101v", [("16_acid_line", [P("E1! . E2 E1 . E1~ G1 E1 A1! . . B1 E1 D2 E1 E2")] * 2 + [P("E1! . E2 E1 . E1~ G1 E1 A1! . . B1 E1 D2! E1 G2")] + [P("C1! . C2 C1 . C1~ Eb1 C1 D1! . . E1 C1 B1 C1 C2")], 0)], 135, "Em", 4, gate=0.8,
           en="Acid line, E minor with a drop to C at the end.", es="Línea acid en Mi menor con caída a Do al final."),
        LM("minimalpulse", "at101v", [("20_minimal_pulse", [P("D2~ . D2 . D2~ . D2 . D2~ . D2 . D2~ . F2 ."), P("D2~ . D2 . D2~ . D2 . D2~ . D2 . D2~ . A2 ."), P("D2~ . D2 . D2~ . D2 . D2~ . D2 . D2~ . F2 ."), P("C2~ . C2 . C2~ . C2 . C2~ . C2 . C2~ . G2 .")], 0)], 128, "Dm", 4, gate=0.45,
           en="Minimal pulse, D minor.", es="Pulso minimal, Re menor."),
        LM("italooctaves", "at101v", [("21_italo_octaves", [P("A1 A2 A1 A2 A1 A2 A1 A2 A1 A2 A1 A2 A1 A2 A1 A2"), P("A1 A2 A1 A2 A1 A2 A1 A2 A1 A2 A1 A2 A1 A2 A1 A2"), P("F1 F2 F1 F2 F1 F2 F1 F2 F1 F2 F1 F2 F1 F2 F1 F2"), P("G1 G2 G1 G2 G1 G2 G1 G2 G1 G2 G1 G2 G1 G2 G1 G2")], 0)], 118, "Am", 4, gate=0.7,
           en="Rolling octave bass, A minor.", es="Bajo de octavas, La menor."),
        LM("technostab", "at101v", [("23_techno_stab", [P(". . . . . . C3+Eb3+G3 . . . . . . . C3+Eb3+G3 ."), P(". . . . . . C3+Eb3+G3 . . . . . . . C3+Eb3+G3 ."), P(". . . . . . Ab2+C3+Eb3 . . . . . . . Ab2+C3+Eb3 ."), P(". . . . . . Bb2+D3+F3 . . . . . . . Bb2+D3+F3 .")], 0)], 132, "Cm", 4, gate=0.5, tail=2.5,
           en="Offbeat chord stabs, C minor.", es="Acordes cortos a contratiempo, Do menor."),
        LM("glassarp", "at101v", [("31_glass_bells", [P("C4 G4 C5 G4 Eb4 Bb4 Eb5 Bb4 C4 G4 C5 G4 Eb4 Bb4 Eb5 Bb4"), P("Ab3 Eb4 Ab4 Eb4 C4 G4 C5 G4 Ab3 Eb4 Ab4 Eb4 C4 G4 C5 G4"), P("Eb4 Bb4 Eb5 Bb4 G4 D5 G5 D5 Eb4 Bb4 Eb5 Bb4 G4 D5 G5 D5"), P("Bb3 F4 Bb4 F4 D4 A4 D5 A4 Bb3 F4 Bb4 F4 D4 A4 D5 A4")], 0)], 120, "Cm", 4, gate=0.6, tail=4.5,
           en="Bell arpeggio over Cm, Ab, Eb, Bb.", es="Arpegio de campanas sobre Cm, Ab, Eb, Bb."),
        LM("darkdrone", "at101v", [("32_dark_drone", [P("C1+G1+C2:64 - - - - - - - - - - - - - - -")] + [R16] * 3, 0)], 60, "C", 4, gate=0.99, vel=90, tail=6.0,
           en="Dark drone on C.", es="Drone oscuro en Do."),
    ],
    fx=[
        FX(("loop", "housebass"), HAL, "p00", "HalTapeEcho", "House bass through a tape echo.", "Bajo house con un eco de cinta."),
        FX(("loop", "acidline"), HAL, "p16", "HalDottedRain", "Acid line with dotted-eighth echoes.", "Línea acid con ecos de corchea con puntillo."),
        FX(("loop", "acidline"), TAR, "p14", "TarLoFiBeat", "Acid line on lo-fi beat tape.", "Línea acid en cinta lo-fi."),
        FX(("loop", "minimalpulse"), HAL, "p19", "HalDuckedDub", "Minimal pulse in a ducked dub space.", "Pulso minimal en un espacio dub."),
        FX(("loop", "technostab"), HAL, "p20", "HalGated1984", "Stabs in a gated room.", "Acordes en una sala con compuerta."),
        FX(("loop", "glassarp"), TAR, "p07", "TarChromeMixtape", "Bells on a chrome mixtape.", "Campanas en un casete cromo."),
        FX(("os", "AT101V_ClassicLead_G4_v100"), HAL, "p04", "HalPlate", "Classic lead through a plate.", "Lead clásico con una placa."),
        FX(("os", "AT101V_PolyPluck_C4_v100"), HAL, "p08", "HalCathedral", "Pluck in a glass cathedral.", "Pluck en una catedral de vidrio."),
        FX(("tex", "darkdrone"), MUR, "p02", "MurSlowFog", "Drone as a slow grain fog.", "Drone como niebla lenta de granos."),
    ],
)

# ═════════════════════════ AT303 (bajo con secuenciador) ═════════════════════════
def _a303(id, preset, bpm, key, bars, gate=0.6, en="", es="", **kw):
    return LM(id, "at303", [(preset, bars, 0)], bpm, key, 4, gate=gate, en=en, es=es, **kw)
synth(
    slug="at303", name="AT303", folder="AT303", kind="synth",
    tag=dict(en="Acid bass machine: squelch, accents and slides", es="Máquina de bajos acid: chillidos, acentos y slides"),
    genres="acid, techno, EBM",
    oneshots=[
        OS("Squelch", "First Squelch", ["A1", "C2", "E2", "A2"], [100, 127], 0.6, 1.0),
        OS("RubberRoot", "Rubber Root", ["A1", "E2"], [100], 0.6, 1.0),
        OS("DeepRoot", "Deep Root", ["F1", "C2"], [100], 0.8, 1.2),
        OS("Screamer", "Screamer", ["A2", "E3"], [110, 127], 0.6, 1.2),
        OS("HotBox", "Hot Box", ["D2", "A2"], [100], 0.6, 1.0),
        OS("Pocket", "Pocket", ["G1", "D2"], [100], 0.5, 0.8),
    ],
    loops=[
        _a303("acid", "First Squelch", 135, "Am", [P("A1! A1 . A2 A1~ . C2 A1 . A1! D2 . A1 E2! . G1"), P("A1! A1 . A2 A1~ . C2 A1 . A1! D2 . A1 E2! . G1"), P("F1! F1 . F2 F1~ . Ab1 F1 . F1! Bb1 . F1 C2! . Eb1"), P("G1! G1 . G2 G1~ . Bb1 G1 . G1! C2 . G1 D2! . F1")], en="Classic acid line, A minor.", es="Línea acid clásica, La menor."),
        _a303("squelch", "Squelch Line", 128, "Em", [P("E1! . E2 . E1~ E1 G1 . E1! . B1 . E1~ D2! . E1"), P("E1! . E2 . E1~ E1 G1 . E1! . B1 . E1~ D2! . E1"), P("E1! . E2 . E1~ E1 G1 . E1! . B1 . E1~ D2! . E1"), P("C1! . C2 . C1~ C1 Eb1 . C1! . G1 . C1~ Bb1! . D1")], en="Open squelch with accents, E minor.", es="Chillido abierto con acentos, Mi menor."),
        _a303("rolling", "Night Bus", 140, "Gm", [P("G1! G1~ G1 G1~ G1! G1~ G1 G1~ G1! G1~ G1 G1~ G1! G1~ Bb1 C2")] * 3 + [P("G1! G1~ G1 G1~ G1! G1~ G1 G1~ Eb1! Eb1~ Eb1 Eb1~ F1! F1~ F1 D2!")], gate=0.5, en="Rolling sixteenths, G minor.", es="Semicorcheas rodantes, Sol menor."),
        _a303("deepsub", "Deep Root", 122, "Dm", [P("D1! . . . . . D2 . D1 . . . . D1~ . F1"), P("D1! . . . . . D2 . D1 . . . . D1~ . A1"), P("D1! . . . . . D2 . D1 . . . . D1~ . F1"), P("C1! . . . . . C2 . C1 . . . . C1~ . G1")], gate=0.8, en="Sparse sub, D minor.", es="Sub espaciado, Re menor."),
        _a303("slides", "Sliding Fifths", 124, "Fm", [P("F1:3 - - C2:2 - Ab1:3 - - F1:2 - C2:3 - - Eb2:3 - -"), P("F1:3 - - C2:2 - Ab1:3 - - F1:2 - C2:3 - - Eb2:3 - -"), P("Db1:3 - - Ab1:2 - F1:3 - - Db1:2 - Ab1:3 - - C2:3 - -"), P("Eb1:3 - - Bb1:2 - G1:3 - - Eb1:2 - Bb1:3 - - D2:3 - -")], gate=1.06, en="Legato fifths with slides, F minor.", es="Quintas ligadas con slides, Fa menor."),
        _a303("offbeat", "Hot Box", 126, "Cm", [P(". . C2 . . . C2 . . . C2 . . . Eb2 ."), P(". . C2 . . . C2 . . . C2 . . . G2 ."), P(". . Ab1 . . . Ab1 . . . Ab1 . . . C2 ."), P(". . Bb1 . . . Bb1 . . . Bb1 . . . D2 .")], gate=0.5, en="Offbeat minimal bass, C minor.", es="Bajo minimal a contratiempo, Do menor."),
    ],
    fx=[
        FX(("loop", "acid"), HAL, "p03", "HalPingPong", "Acid line with ping-pong echoes.", "Línea acid con ecos ping-pong."),
        FX(("loop", "acid"), TAR, "p06", "TarCassette", "Acid line on a dusty cassette.", "Línea acid en un casete gastado."),
        FX(("loop", "squelch"), HAL, "p05", "HalSmallRoom", "Squelch in a small room.", "Chillido en una sala chica."),
        FX(("loop", "rolling"), HAL, "p19", "HalDuckedDub", "Rolling line in a ducked dub space.", "Línea rodante en un espacio dub."),
        FX(("loop", "slides"), TAR, "p14", "TarLoFiBeat", "Slides on lo-fi tape.", "Slides en cinta lo-fi."),
        FX(("loop", "offbeat"), HAL, "p00", "HalTapeEcho", "Offbeat bass through a tape echo.", "Bajo a contratiempo con eco de cinta."),
        FX(("os", "AT303_Screamer_A2_v127"), HAL, "p06", "HalHall", "Screamer in a hall.", "Screamer en una sala."),
        FX(("os", "AT303_Squelch_A1_v127"), TAR, "p12", "TarHotFerric", "Accented squelch on hot tape.", "Chillido acentuado en cinta saturada."),
        FX(("tex", "slides"), MUR, "p23", "MurGranularWash", "Slides smeared into a granular wash.", "Slides disueltos en una marea granular."),
    ],
)

# ═════════════════════════ ATMD (Minimoog Model D) ═════════════════════════
synth(
    slug="atmd", name="ATMD", folder="ATMD", kind="synth",
    tag=dict(en="Classic monosynth: fat basses, singing leads, keys and strings", es="Monosintetizador clásico: bajos gordos, leads que cantan, teclas y cuerdas"),
    genres="funk, synth-pop, prog, ambient",
    oneshots=[
        OS("FatBass", "p01_fat", ["E1", "A1", "E2"], [100], 0.9, 1.0),
        OS("SquelchBass", "p04_squelch", ["E1", "B1", "E2"], [100], 0.6, 1.0),
        OS("LowThump", "p06_moog", ["C1", "G1"], [110], 0.6, 1.0),
        OS("LuckyLead", "p09_lucky", ["E3", "G3", "B3", "E4"], [100], 1.0, 1.5),
        OS("Screamer", "p15_screamer", ["A3", "E4"], [105], 1.2, 1.5),
        OS("ElectricPiano", "p20_electric", ["C3", "E3", "G3", "C4"], [70, 110], 1.0, 2.0),
        OS("Harpsichord", "p21_harpsichord", ["C4", "G4"], [100], 0.4, 1.5),
        OS("PolyStrings", "p28_poly_strings", ["A2"], [90], 2.5, 2.5),
        OS("Kick", "p22_kick", ["C2"], [70, 115], 0.3, 0.8),
        OS("Snare", "p23_snare", ["C3"], [70, 115], 0.3, 0.8),
        OS("HiHat", "p24_hi_hat", ["C4"], [70, 115], 0.2, 0.5),
        OS("LaserZap", "p26_laser", ["C4"], [100], 0.8, 1.0),
    ],
    loops=[
        LM("funkbass", "atmd", [("p07_funk", [P("E1! . . E2 . E1~ . G1 . A1! . . B1 . D2 E2"), P("E1! . . E2 . E1~ . G1 . A1! . . B1 . D2 E2"), P("A1! . . A2 . A1~ . C2 . D2! . . E2 . D2 C2"), P("B1! . . B2 . B1~ . D2 . E2! . . F#2 . E2 D2")], 0)], 100, "Em", 4, gate=0.6, en="Funk bass line, E minor.", es="Línea de bajo funk, Mi menor."),
        LM("squelch", "atmd", [("p04_squelch", [P("A1! . A1 . A2~ . A1 . C2! . A1 . G1 . A1 E2"), P("A1! . A1 . A2~ . A1 . C2! . A1 . G1 . A1 E2"), P("F1! . F1 . F2~ . F1 . Ab1! . F1 . Eb1 . F1 C2"), P("G1! . G1 . G2~ . G1 . Bb1! . G1 . F1 . G1 D2")], 0)], 112, "Am", 4, gate=0.6, en="Squelchy bass in A minor.", es="Bajo chillón en La menor."),
        LM("epchords", "atmd", [("p20_electric", [P("F3+A3+C4+E4:6 - - - - - . . F3+A3+C4+E4:4 - - - . . . ."), P("Bb2+D3+F3+A3:6 - - - - - . . Bb2+D3+F3+A3:4 - - - . . . ."), P("G3+Bb3+D4+F4:6 - - - - - . . G3+Bb3+D4+F4:4 - - - . . . ."), P("C3+E3+G3+Bb3:6 - - - - - . . C3+E3+G3+Bb3:4 - - - . . . .")], 0)], 85, "F", 4, gate=0.95, vel=84, tail=4.0, en="Electric piano comping, F major.", es="Acompañamiento de piano eléctrico, Fa mayor."),
        LM("harpsiarp", "atmd", [("p21_harpsichord", [P("D4 F4 A4 D5 A4 F4 D4 F4 A4 D5 A4 F4 D4 F4 A4 D5"), P("Bb3 D4 F4 Bb4 F4 D4 Bb3 D4 F4 Bb4 F4 D4 Bb3 D4 F4 Bb4"), P("G3 Bb3 D4 G4 D4 Bb3 G3 Bb3 D4 G4 D4 Bb3 G3 Bb3 D4 G4"), P("A3 C#4 E4 A4 E4 C#4 A3 C#4 E4 A4 E4 C#4 A3 C#4 E4 A4")], 0)], 105, "Dm", 4, gate=0.7, tail=3.0, en="Harpsichord arpeggio, D minor.", es="Arpegio de clave, Re menor."),
        LM("lucky", "atmd", [("p09_lucky", [P("E4:4 - - - G4:2 - A4:2 - B4:4 - - - D5:2 - B4:2 -"), P("A4:4 - - - G4:2 - E4:2 - D4:4 - - - E4:4 - - -"), P("E4:4 - - - G4:2 - A4:2 - B4:4 - - - D5:2 - E5:2 -"), P("D5:4 - - - B4:2 - A4:2 - E4:8 - - - - - - -")], 0)], 96, "Em", 4, gate=0.96, vel=104, tail=3.0, en="Singing lead phrase, E minor.", es="Frase de lead cantante, Mi menor."),
        LM("strings", "atmd", [("p28_poly_strings", [P("A2+E3+A3+C4:16 - - - - - - - - - - - - - - -"), P("F2+C3+F3+A3:16 - - - - - - - - - - - - - - -"), P("D3+A3+D4+F4:16 - - - - - - - - - - - - - - -"), P("E2+B2+E3+G#3:16 - - - - - - - - - - - - - - -")], 0)], 66, "Am", 4, gate=0.98, vel=84, tail=5.0, en="Poly string pad, A minor.", es="Pad de cuerdas poli, La menor."),
        LM("drums", "atmd", [("p22_kick", [_kick, _kick, _kick, _kick_f], 0), ("p23_snare", [_snare, _snare, _snare, _snare_f], -2), ("p24_hi_hat", [_hat] * 4, -8)], 108, "", 4, gate=0.5, tail=2.0, en="Kick, snare and hat patches, steady backbeat.", es="Bombo, caja y charles, ritmo firme."),
    ],
    fx=[
        FX(("loop", "funkbass"), HAL, "p07", "HalSpringSurf", "Funk bass through a spring tank.", "Bajo funk con un resorte."),
        FX(("loop", "squelch"), TAR, "p02", "TarDemoReel", "Squelch on a 7.5 ips demo reel.", "Chillido en un carrete demo a 7,5 ips."),
        FX(("loop", "epchords"), HAL, "p04", "HalPlate", "Electric piano through a plate.", "Piano eléctrico con placa."),
        FX(("loop", "epchords"), TAR, "p07", "TarChromeMixtape", "Electric piano on a chrome mixtape.", "Piano eléctrico en un casete cromo."),
        FX(("loop", "harpsiarp"), HAL, "p16", "HalDottedRain", "Harpsichord in dotted echoes.", "Clave con ecos con puntillo."),
        FX(("loop", "lucky"), HAL, "p29", "HalGrainStorm", "Lead in a grain storm.", "Lead en una tormenta de granos."),
        FX(("loop", "strings"), TAR, "p18", "TarWornVinyl", "Strings on worn vinyl.", "Cuerdas en vinilo gastado."),
        FX(("os", "ATMD_LuckyLead_E4_v100"), HAL, "p06", "HalHall", "Lead in a concert hall.", "Lead en una sala de conciertos."),
        FX(("tex", "strings"), MUR, "p06", "MurGlassHalo", "Strings as a glass halo.", "Cuerdas como un halo de vidrio."),
    ],
)

# ═════════════════════════ AT700 (Roland System-700) ═════════════════════════
def _pad(chords, step=16):   # un acorde por compás, sostenido
    return [P(f"{c}:{step} " + " ".join(["-"] * (step - 1))) for c in chords]
synth(
    slug="at700", name="AT700", folder="AT700", kind="synth",
    tag=dict(en="Modular synthesizer: brass, strings, basses and effects", es="Sintetizador modular: metales, cuerdas, bajos y efectos modulares"),
    genres="synth-pop, new wave, ambient, cinematic",
    oneshots=[
        OS("FatSawLead", "01_fat_saw", ["A3", "E4", "A4"], [100], 1.2, 1.5),
        OS("BrassStab", "02_tomita", ["Bb3", "F4"], [100], 1.4, 1.8),
        OS("Bass700", "05_700_bass", ["E1", "A1", "E2"], [100], 0.8, 1.0),
        OS("SubPulse", "07_sub_pulse", ["C1", "F1"], [100], 1.0, 1.2),
        OS("SyncSweep", "08_sync_sweep", ["A3", "E4"], [100], 1.2, 1.5),
        OS("Flute", "10_flute", ["D4", "A4"], [96], 1.2, 1.5),
        OS("RingBell", "14_ring_bells", ["C5", "G5"], [100], 0.5, 3.5),
        OS("TineKeys", "15_tine_keys", ["C3", "E3", "G3"], [70, 110], 0.8, 2.0),
        OS("EnsembleStrings", "03_ensemble", ["A2"], [90], 2.5, 2.5),
        OS("Kick", "29_kick", ["C2"], [70, 115], 0.3, 0.8),
        OS("Snare", "30_snare", ["C3"], [70, 115], 0.3, 0.8),
        OS("HiHat", "31_hi_hat", ["C4"], [70, 115], 0.2, 0.5),
        OS("LaserZap", "25_laser", ["C4"], [100], 0.8, 1.0),
        OS("Thunder", "26_thunder", ["C2"], [100], 3.0, 3.5),
    ],
    loops=[
        LM("synthpopbass", "at700", [("05_700_bass", [P("E2 E2 E3 E2 G2 A2 E2 B2 E2 E2 E3 E2 G2 A2 E2 B2")] * 2 + [transpose(P("E2 E2 E3 E2 G2 A2 E2 B2 E2 E2 E3 E2 G2 A2 E2 B2"), -4), transpose(P("E2 E2 E3 E2 G2 A2 E2 B2 E2 E2 E3 E2 G2 A2 E2 B2"), -2)], 0)], 118, "Em", 4, gate=0.45, vel=100, en="Eighth-note synth-pop bass, E minor.", es="Bajo de corcheas synth-pop, Mi menor."),
        LM("brassfanfare", "at700", [("02_tomita", [P("Bb3+D4+F4+Bb4:8 - - - - - - - A3+C4+F4+A4:8 - - - - - - -"), P("G3+Bb3+D4+G4:8 - - - - - - - D4+F4+Bb4+D5:8 - - - - - - -"), P("Eb3+G3+Bb3+Eb4:8 - - - - - - - F3+A3+C4+F4:8 - - - - - - -"), P("Bb3+D4+F4+Bb4:16 - - - - - - - - - - - - - - -")], 0)], 72, "Bb", 4, gate=0.96, vel=92, tail=4.0, en="Slow brass chords, Bb major.", es="Acordes lentos de metales, Si bemol mayor."),
        LM("stringpad", "at700", [("03_ensemble", _pad(["D3+A3+D4+F4+A4", "Bb2+F3+Bb3+D4+F4", "F2+C3+F3+A3+C4", "C3+G3+C4+E4+G4"]), 0)], 66, "Dm", 4, gate=0.98, vel=84, tail=5.0, en="Ensemble strings: Dm, Bb, F, C.", es="Cuerdas ensemble: Dm, Sib, Fa, Do."),
        LM("syncphrase", "at700", [("08_sync_sweep", [P("A4:4 - - - C5:4 - - - D5:2 - E5:6 - - - - -"), P("G5:4 - - - E5:4 - - - D5:4 - - - C5:4 - - -"), P("A4:4 - - - C5:4 - - - D5:2 - E5:6 - - - - -"), P("G5:4 - - - E5:2 - D5:2 - A4:8 - - - - - - -")], 0)], 108, "Am", 4, gate=0.9, vel=100, tail=3.0, en="Sync lead phrase, A minor.", es="Frase de lead con sync, La menor."),
        LM("pwmarp", "at700", [("11_pwm_lead", [P("A3 C4 E4 A4 E4 C4 A3 C4 E4 A4 E4 C4 A3 C4 E4 A4"), P("F3 A3 C4 F4 C4 A3 F3 A3 C4 F4 C4 A3 F3 A3 C4 F4"), P("C4 E4 G4 C5 G4 E4 C4 E4 G4 C5 G4 E4 C4 E4 G4 C5"), P("G3 B3 D4 G4 D4 B3 G3 B3 D4 G4 D4 B3 G3 B3 D4 G4")], 0)], 112, "Am", 4, gate=0.75, vel=96, tail=3.0, en="PWM arpeggio: Am, F, C, G.", es="Arpegio PWM: Am, Fa, Do, Sol."),
        LM("bellmotif", "at700", [("14_ring_bells", [P("C5:3 - - G5:3 - - E5:2 - D5:4 - - - A4:2 - C5:2 -"), P("G5:3 - - E6:3 - - D6:2 - C6:4 - - - G5:2 - E5:2 -"), P("C5:3 - - G5:3 - - E5:2 - D5:4 - - - A4:2 - C5:2 -"), P("C5+G5+E6:16 - - - - - - - - - - - - - - -")], 0)], 84, "C", 4, gate=0.9, vel=100, tail=5.0, en="Bell motif in C.", es="Motivo de campanas en Do."),
        LM("drums", "at700", [("29_kick", [_kick, _kick, _kick, _kick_f], 0), ("30_snare", [_snare, _snare, _snare, _snare_f], -2), ("31_hi_hat", [_hat] * 4, -8)], 118, "", 4, gate=0.5, tail=2.0, en="Kick, snare and hat patches, new wave pulse.", es="Bombo, caja y charles, pulso new wave."),
    ],
    fx=[
        FX(("loop", "synthpopbass"), HAL, "p02", "HalCleanDigital", "Synth-pop bass with a clean digital echo.", "Bajo synth-pop con un eco digital limpio."),
        FX(("loop", "brassfanfare"), HAL, "p06", "HalConcertHall", "Brass in a concert hall.", "Metales en una sala de conciertos."),
        FX(("loop", "stringpad"), TAR, "p01", "Tar30ipsMaster", "Strings on a 30 ips master.", "Cuerdas en una cinta master de 30 ips."),
        FX(("loop", "syncphrase"), HAL, "p00", "HalTapeEcho", "Sync lead through a tape echo.", "Lead con sync y eco de cinta."),
        FX(("loop", "pwmarp"), HAL, "p03", "HalPingPong", "PWM arpeggio ping-pong.", "Arpegio PWM en ping-pong."),
        FX(("loop", "bellmotif"), HAL, "p08", "HalCathedral", "Bells in a glass cathedral.", "Campanas en una catedral de vidrio."),
        FX(("loop", "drums"), HAL, "p20", "HalGated1984", "Drums in a gated room.", "Batería en una sala con compuerta."),
        FX(("os", "AT700_RingBell_C5_v100"), HAL, "p26", "HalReverseShimmer", "Bell with a reverse shimmer.", "Campana con shimmer invertido."),
        FX(("tex", "stringpad"), MUR, "p05", "MurChoirRising", "Strings into a rising choir of grains.", "Cuerdas convertidas en un coro de granos."),
    ],
)

# ═════════════════════════ ATFS1 (Yamaha FS1R) ═════════════════════════
synth(
    slug="atfs1", name="ATFS1", folder="ATFS1", kind="synth",
    tag=dict(en="Formant synthesis: choirs, talking basses, glass and bells", es="Síntesis de formantes: coros, bajos que hablan, vidrio y campanas"),
    genres="ambient, dark wave, cinematic, electro",
    oneshots=[
        OS("VowelChoir", "p05_vowel_choir", ["C3", "G3", "C4"], [90], 2.0, 2.5),
        OS("HighChoir", "p06_high_choir", ["E4", "B4"], [90], 2.0, 2.5),
        OS("LaLaLead", "p07_la_la", ["A3", "E4"], [100], 1.2, 1.5),
        OS("FormantBass", "p27_formant_bass", ["E1", "A1", "E2"], [100], 0.8, 1.0),
        OS("OddSubBass", "p28_odd_sub", ["C1", "G1"], [100], 1.0, 1.2),
        OS("TinePiano", "p21_tine_piano", ["C3", "E3", "G3", "C4"], [70, 110], 0.8, 2.0),
        OS("GlassMarimba", "p23_glass_marimba", ["C4", "G4", "C5"], [100], 0.4, 2.0),
        OS("FormantBells", "p22_formant_bells", ["C5", "G5"], [100], 0.5, 3.5),
        OS("WhisperPad", "p08_whisper_pad", ["A2"], [90], 2.5, 3.0),
        OS("BreathFlute", "p31_breath_flute", ["D4", "A4"], [96], 1.2, 1.5),
    ],
    loops=[
        LM("choirchords", "atfs1", [("p05_vowel_choir", _pad(["D3+A3+C4+F4+A4", "Bb2+F3+A3+D4+A4", "F2+C3+A3+C4+E4", "C3+G3+C4+E4+G4"]), 0)], 72, "Dm", 4, gate=0.96, vel=84, tail=5.0, en="Vowel choir: Dm9, Bbmaj7, Fmaj7, C.", es="Coro de vocales: Dm9, Sibmaj7, Famaj7, Do."),
        LM("formantbass", "atfs1", [("p27_formant_bass", [P("A1! . . A2 . A1~ . C2 . A1! . . G1 . A1 E2"), P("A1! . . A2 . A1~ . C2 . A1! . . G1 . A1 E2"), P("F1! . . F2 . F1~ . Ab1 . F1! . . Eb1 . F1 C2"), P("G1! . . G2 . G1~ . Bb1 . G1! . . F1 . G1 D2")], 0)], 100, "Am", 4, gate=0.6, en="Talking formant bass, A minor.", es="Bajo de formantes que habla, La menor."),
        LM("marimba", "atfs1", [("p23_glass_marimba", [P("C4 G4 C5 E5 C5 G4 C4 G4 E4 B4 E5 G5 E5 B4 E4 B4"), P("A3 E4 A4 C5 A4 E4 A3 E4 F4 C5 F5 A5 F5 C5 F4 C5"), P("C4 G4 C5 E5 C5 G4 C4 G4 E4 B4 E5 G5 E5 B4 E4 B4"), P("D4 A4 D5 F5 D5 A4 D4 A4 G3 D4 G4 B4 G4 D4 G3 D4")], 0)], 110, "C", 4, gate=0.55, tail=4.0, en="Glass marimba pattern in C.", es="Patron de marimba de vidrio en Do."),
        LM("tinecomp", "atfs1", [("p21_tine_piano", [P("D3+F3+A3+C4:6 - - - - - . . D3+F3+A3+C4:4 - - - . . . ."), P("G3+B3+D4+F4:6 - - - - - . . G3+B3+D4+F4:4 - - - . . . ."), P("C3+E3+G3+B3:6 - - - - - . . C3+E3+G3+B3:4 - - - . . . ."), P("A2+C3+E3+G3:6 - - - - - . . A2+C3+E3+G3:4 - - - . . . .")], 0)], 92, "C", 4, gate=0.95, vel=88, tail=3.5, en="Tine piano comping, Dm7 G7 Cmaj7 Am7.", es="Acompañamiento de piano de púas, Dm7 G7 Cmaj7 Am7."),
        LM("lalalead", "atfs1", [("p07_la_la", [P("A4:4 - - - C5:4 - - - E5:6 - - - - - D5:2 -"), P("C5:4 - - - B4:2 - A4:2 - G4:8 - - - - - - -"), P("A4:4 - - - C5:4 - - - E5:6 - - - - - G5:2 -"), P("E5:4 - - - D5:4 - - - A4:8 - - - - - - -")], 0)], 84, "Am", 4, gate=0.96, vel=100, tail=3.0, en="Sung lead phrase, A minor.", es="Frase de lead cantada, La menor."),
        LM("whisperpad", "atfs1", [("p08_whisper_pad", _pad(["A2+E3+A3+C4", "F2+C3+F3+A3", "G2+D3+G3+B3", "E2+B2+E3+G3"]), 0)], 72, "Am", 4, gate=0.98, vel=84, tail=5.5, en="Whisper pad: Am, F, G, Em.", es="Pad susurrado: Am, Fa, Sol, Em."),
    ],
    fx=[
        FX(("loop", "choirchords"), HAL, "p08", "HalCathedral", "Choir in a glass cathedral.", "Coro en una catedral de vidrio."),
        FX(("loop", "choirchords"), TAR, "p09", "TarFourthGenDub", "Choir on a fourth-generation dub.", "Coro en una copia de cuarta generacion."),
        FX(("loop", "formantbass"), HAL, "p19", "HalDuckedDub", "Formant bass in a ducked dub space.", "Bajo de formantes en un espacio dub."),
        FX(("loop", "marimba"), HAL, "p16", "HalDottedRain", "Marimba with dotted echoes.", "Marimba con ecos con puntillo."),
        FX(("loop", "tinecomp"), TAR, "p07", "TarChromeMixtape", "Tine piano on a chrome mixtape.", "Piano de púas en un casete cromo."),
        FX(("loop", "lalalead"), HAL, "p04", "HalPlateVocal", "Lead through a vocal plate.", "Lead con placa vocal."),
        FX(("os", "ATFS1_VowelChoir_G3_v90"), HAL, "p06", "HalHall", "Choir note in a concert hall.", "Nota de coro en una sala de conciertos."),
        FX(("tex", "whisperpad"), MUR, "p04", "MurDustMotes", "Whisper pad as drifting dust.", "Pad susurrado como polvo a la deriva."),
        FX(("tex", "choirchords"), MUR, "p07", "MurCathedralBloom", "Choir blooming into grains.", "Coro que florece en granos."),
    ],
)

# ═════════════════════════ AT42 (Elta SOLAR 42) ═════════════════════════
def _keys(*ks): return ks
synth(
    slug="at42", name="AT42", folder="AT42", kind="synth",
    tag=dict(en="Negistor drones, orbs and flute: slow, huge, alive", es="Drones negistor, orbes y flauta: lentos, enormes, vivos"),
    genres="drone, ambient, dark ambient, experimental",
    oneshots=[
        OS("CathedralOfA", "01_cathedral", ["C1", "D1", "E1"], [100], 3.0, 3.0),
        OS("LowChoir", "02_low_choir", ["C1", "E1"], [100], 3.0, 3.0),
        OS("ShimmerGarden", "07_shimmer", ["C1", "F1"], [100], 3.0, 3.0),
        OS("FluteLead", "20_flute", ["A3", "C4", "E4", "A4"], [100], 1.0, 1.5),
        OS("SequencedPlates", "24_sequenced", ["C3", "E3", "G3"], [100], 1.5, 2.0),
        OS("PressurePad", "23_pressure", ["C2", "G2"], [100], 3.0, 3.0),
        OS("RingBell", "27_ring_bells", ["C4", "G4"], [100], 0.5, 3.5),
        OS("SirenTower", "12_siren", ["C1"], [100], 3.0, 2.5),
    ],
    loops=[
        LM("cathedral", "at42", [("01_cathedral", [P("C1+E1:64 - - - - - - - - - - - - - - -")] + [R16] * 3, 0)], 72, "A", 4, gate=0.99, tail=6.0, en="Cathedral drone, four long bars.", es="Drone de catedral, cuatro compases largos."),
        LM("lowchoir", "at42", [("02_low_choir", [P("C1+D1:64 - - - - - - - - - - - - - - -")] + [R16] * 3, 0)], 72, "D", 4, gate=0.99, tail=6.0, en="Low choir drone.", es="Drone de coro grave."),
        LM("shimmer", "at42", [("07_shimmer", [P("C1:8 - - - - - - - E1:8 - - - - - - -"), P("D1:8 - - - - - - - F1:8 - - - - - - -"), P("C1:8 - - - - - - - E1:8 - - - - - - -"), P("D1:16 - - - - - - - - - - - - - - -")], 0)], 66, "", 4, gate=0.98, tail=6.0, en="Shimmer garden, slow key changes.", es="Jardín de brillos, cambios lentos de tecla."),
        LM("flutepent", "at42", [("20_flute", [P("A3:4 - - - C4:2 - D4:2 - E4:4 - - - G4:4 - - -"), P("E4:4 - - - D4:2 - C4:2 - D4:4 - - - A3:4 - - -"), P("A3:4 - - - C4:2 - D4:2 - E4:4 - - - G4:4 - - -"), P("A4:8 - - - - - - - . . . . . . . .")], 0)], 84, "Am", 4, gate=0.95, tail=3.0, en="Flute phrase, A minor pentatonic.", es="Frase de flauta, pentatonica de La menor."),
        LM("plates", "at42", [("24_sequenced", [P("C3:2 - E3:2 - G3:2 - E3:2 - C3:2 - E3:2 - A3:2 - G3:2 -"), P("C3:2 - E3:2 - G3:2 - E3:2 - C3:2 - E3:2 - A3:2 - G3:2 -"), P("A2:2 - C3:2 - E3:2 - C3:2 - A2:2 - C3:2 - F3:2 - E3:2 -"), P("G2:2 - B2:2 - D3:2 - B2:2 - G2:2 - B2:2 - E3:2 - D3:2 -")], 0)], 96, "C", 4, gate=0.8, tail=4.0, en="Sequenced plates, steady eighths.", es="Placas secuenciadas, corcheas constantes."),
        LM("berlinfive", "at42", [("17_berlin_five", [P("C1:64 - - - - - - - - - - - - - - -")] + [R16] * 3, 0)], 118, "", 4, gate=0.99, tail=3.0, en="Berlin five: one key held, the machine plays itself.", es="Berlin five: una tecla sostenida, la maquina toca sola."),
        LM("bells", "at42", [("27_ring_bells", [P("C4:4 - - - G4:4 - - - Eb4:4 - - - F4:4 - - -"), P("D4:4 - - - A4:4 - - - F4:4 - - - G4:4 - - -"), P("C4:4 - - - G4:4 - - - Eb4:4 - - - F4:4 - - -"), P("C4+G4+Eb5:16 - - - - - - - - - - - - - - -")], 0)], 72, "Cm", 4, gate=0.9, tail=5.0, en="Ring bells, C minor.", es="Campanas con anillo, Do menor."),
    ],
    fx=[
        FX(("loop", "cathedral"), HAL, "p09", "HalEndlessPad", "Cathedral drone into an endless pad space.", "Drone de catedral en un espacio de pad infinito."),
        FX(("loop", "lowchoir"), TAR, "p30", "TarHalfSpeedDream", "Low choir at half-speed tape.", "Coro grave en cinta a media velocidad."),
        FX(("loop", "shimmer"), HAL, "p26", "HalReverseShimmer", "Shimmer with a reverse shimmer.", "Brillos con shimmer invertido."),
        FX(("loop", "flutepent"), HAL, "p05", "HalSmallRoom", "Flute in a small room.", "Flauta en una sala chica."),
        FX(("loop", "plates"), HAL, "p16", "HalDottedRain", "Plates with dotted echoes.", "Placas con ecos con puntillo."),
        FX(("loop", "bells"), HAL, "p08", "HalCathedral", "Bells in a glass cathedral.", "Campanas en una catedral de vidrio."),
        FX(("tex", "cathedral"), MUR, "p02", "MurSlowFog", "Drone as a slow grain fog.", "Drone como niebla lenta de granos."),
        FX(("loop", "berlinfive"), TAR, "p03", "TarHotDrumBus", "Berlin five on a hot tape bus.", "Berlin five en un bus de cinta saturada."),
    ],
)

# ═════════════════════════ AT330 (Korg VP-330 style) ═════════════════════════
synth(
    slug="at330", name="AT330", folder="AT330", kind="synth",
    tag=dict(en="Vocal ensemble: strings, choirs and slow pads", es="Ensamble vocal: cuerdas, coros y pads lentos"),
    genres="dark wave, coldwave, ambient, cinematic",
    oneshots=[
        OS("EnsembleStrings", "Ensemble Strings", ["A2", "A3", "E4"], [100], 2.0, 2.2),
        OS("DarkStrings", "Dark Strings", ["D2", "D3"], [100], 2.5, 2.5),
        OS("ShortStrings", "Short Strings", ["C3+G3+C4", "A2+E3+A3"], [100], 0.3, 1.2),
        OS("FemaleChoir", "Female Choir", ["C4", "E4", "G4"], [90], 2.0, 2.5),
        OS("MaleChoir", "Male Choir 8'", ["C3", "G3"], [90], 2.0, 2.5),
        OS("FullChoir", "Full Choir", ["A2+E3+A3+C4"], [90], 3.0, 3.5),
        OS("StringsAndChoir", "Strings and Choir", ["F2+C3+F3+A3"], [90], 3.0, 3.5),
        OS("GlassHall", "Glass Hall", ["C3+G3+E4"], [90], 3.0, 3.5),
        OS("RisingChoir", "Rising Choir", ["A3"], [90], 2.5, 3.0),
    ],
    loops=[
        LM("stringschoir", "at330", [("Strings and Choir", _pad(["A2+E3+A3+C4+E4", "F2+C3+F3+A3+C4", "C3+G3+C4+E4+G4", "G2+D3+G3+B3+D4"]), 0)], 72, "Am", 4, gate=0.97, vel=88, tail=5.0, en="Strings and choir: Am, F, C, G.", es="Cuerdas y coro: Am, Fa, Do, Sol."),
        LM("rainneon", "at330", [("Rain on Neon", _pad(["D3+A3+D4+F4", "Bb2+F3+Bb3+D4", "G2+D3+G3+Bb3", "A2+E3+A3+C#4"]), 0)], 66, "Dm", 4, gate=0.97, vel=84, tail=5.0, en="Slow neon pad: Dm, Bb, Gm, A.", es="Pad lento de neón: Dm, Sib, Gm, La."),
        LM("nightpad", "at330", [("Night Shift Pad", _pad(["E2+B2+E3+G3+B3", "C3+G3+C4+E4", "A2+E3+A3+C4", "B2+F#3+B3+D#4"]), 0)], 72, "Em", 4, gate=0.97, vel=84, tail=5.0, en="Dark wave bed: Em, C, Am, B.", es="Base dark wave: Em, Do, Am, Si."),
        LM("glasshall", "at330", [("Glass Hall", _pad(["C3+G3+C4+E4+G4", "A2+E3+A3+C4+E4", "F2+C3+F3+A3+C4", "G2+D3+G3+B3+D4"]), 0)], 76, "C", 4, gate=0.97, vel=84, tail=5.0, en="Bright swells in C.", es="Swells brillantes en Do."),
        LM("stabs", "at330", [("Short Strings", [P("C4+E4+G4 . . C4+E4+G4~ . . C4+E4+G4 . . . C4+E4+G4~ . . . . ."), P("C4+E4+G4 . . C4+E4+G4~ . . C4+E4+G4 . . . C4+E4+A4~ . . . . ."), P("C4+F4+A4 . . C4+F4+A4~ . . C4+F4+A4 . . . C4+F4+A4~ . . . . ."), P("B3+D4+G4 . . B3+D4+G4~ . . B3+D4+G4 . . . B3+D4+A4~ . . . . .")], 0)], 110, "C", 4, gate=0.4, vel=96, tail=2.5, en="Rhythmic string stabs.", es="Cortes rítmicos de cuerdas."),
        LM("monks", "at330", [("Low Monks", [P("D2+A2+D3:16 - - - - - - - - - - - - - - -"), P("D2+A2+D3:16 - - - - - - - - - - - - - - -"), P("C2+G2+C3:16 - - - - - - - - - - - - - - -"), P("D2+A2+D3:16 - - - - - - - - - - - - - - -")], 0)], 60, "D", 4, gate=0.98, vel=88, tail=6.0, en="Low monks drone, D.", es="Drone de monjes graves, Re."),
    ],
    fx=[
        FX(("loop", "stringschoir"), HAL, "p08", "HalCathedral", "Strings and choir in a glass cathedral.", "Cuerdas y coro en una catedral de vidrio."),
        FX(("loop", "stringschoir"), TAR, "p09", "TarFourthGenDub", "Strings and choir on a worn dub.", "Cuerdas y coro en una copia gastada."),
        FX(("loop", "rainneon"), HAL, "p12", "HalWarmBlanket", "Neon pad in a warm blanket of reverb.", "Pad de neón en una manta calida de reverb."),
        FX(("loop", "nightpad"), TAR, "p18", "TarWornVinyl", "Dark wave bed on worn vinyl.", "Base dark wave en vinilo gastado."),
        FX(("loop", "glasshall"), HAL, "p26", "HalReverseShimmer", "Swells with a reverse shimmer.", "Swells con shimmer invertido."),
        FX(("loop", "stabs"), HAL, "p20", "HalGated1984", "Stabs in a gated room.", "Cortes en una sala con compuerta."),
        FX(("loop", "stabs"), TAR, "p07", "TarChromeMixtape", "Stabs on a chrome mixtape.", "Cortes en un casete cromo."),
        FX(("tex", "monks"), MUR, "p19", "MurFifthsDrone", "Monks as a granular drone in fifths.", "Monjes como drone granular en quintas."),
        FX(("os", "AT330_FemaleChoir_E4_v90"), HAL, "p06", "HalHall", "Female choir note in a hall.", "Nota de coro femenino en una sala."),
    ],
)

# ═════════════════════════ DeviceNTI V (FM, trip hop y oscuro) ═════════════════════════
def _dv(id, preset, bpm, key, en, es, extra=(), tail=3.5):
    return LC(id, "devicenti", ["--preset", preset, "--set", "tape=0"] + list(extra), bpm, key, 4, tail=tail, en=en, es=es)
synth(
    slug="devicenti", name="DeviceNTI V", folder="DeviceNTI", kind="synth",
    tag=dict(en="FM groovebox: heavy basses, dub chords, dark ambient and trip hop", es="Groovebox FM: bajos pesados, acordes dub, dark ambient y trip hop"),
    genres="trip hop, dub, dark ambient, heavy",
    oneshots=[
        OS("CrownBass", "Iron Growl", ["C1", "G1", "C2"], [100], 0.9, 1.0, ["--focus", "2", "--set", "tape=0"]),
        OS("SineSub", "Concrete Sub", ["C1", "F1", "C2"], [100], 1.0, 1.0, ["--focus", "2", "--set", "tape=0"]),
        OS("HammerStab", "Hammer Stab", ["C3+Eb3+G3", "F3+Ab3+C4"], [100], 0.4, 1.5, ["--focus", "3", "--set", "tape=0"]),
        OS("VibeMallet", "Vibe Mallet", ["C4", "Eb4", "G4"], [100], 0.5, 2.5, ["--focus", "3", "--set", "tape=0"]),
        OS("FunkTine", "Funk Tine", ["C3+Eb3+G3+Bb3"], [80, 110], 0.8, 2.0, ["--focus", "3", "--set", "tape=0"]),
        OS("SundayOrgan", "Sunday Organ", ["C3+G3+C4", "F2+C3+F3"], [100], 1.5, 1.2, ["--focus", "3", "--set", "tape=0"]),
        OS("HornSection", "Horn Section", ["G3+Bb3+D4"], [100], 1.0, 1.2, ["--focus", "3", "--set", "tape=0"]),
        OS("RiteBell", "Rite Bell", ["C4"], [100], 0.5, 4.0, ["--focus", "2", "--set", "tape=0"]),
        OS("HollowGong", "Hollow Gong", ["C2"], [100], 0.5, 4.0, ["--focus", "2", "--set", "tape=0"]),
        OS("HeldBreath", "Held Breath", ["C2"], [90], 3.0, 3.0, ["--focus", "2", "--set", "tape=0"]),
        OS("Kick", "Anvil Kit", ["C2"], [75, 115], 0.3, 0.8, ["--focus", "1", "--set", "tape=0"]),
        OS("Snare", "Anvil Kit", ["D2"], [75, 115], 0.3, 0.8, ["--focus", "1", "--set", "tape=0"]),
        OS("ClosedHat", "Club Kit", ["F#2"], [75, 115], 0.2, 0.5, ["--focus", "1", "--set", "tape=0"]),
        OS("OpenHat", "Club Kit", ["A#2"], [100], 0.3, 1.0, ["--focus", "1", "--set", "tape=0"]),
    ],
    loops=[
        _dv("lowswell", "Low Swell", 76, "Cm", "Heavy driven bass pulsing under a C minor bed.", "Bajo pesado y saturado que pulsa sobre una base en Do menor."),
        _dv("halflight", "Half-Light", 80, "Fm", "Half-time kit and a soft deep kick, F minor.", "Batería en half-time con bombo profundo, Fa menor."),
        _dv("dubthrows", "Dub Throws", 82, "Am", "Dark lo-fi stab thrown into the dub delay, A minor.", "Acorde oscuro lanzado al delay dub, La menor."),
        _dv("slowweight", "Slow Weight", 60, "D", "Phrygian cluster pad, no pulse.", "Pad en cluster frigio, sin pulso."),
        _dv("frozentail", "Frozen Tail", 70, "Dm", "Chords into a long hall, D minor.", "Acordes en una sala larga, Re menor."),
        _dv("foldbass", "Drive and Fold", 75, "E", "One bass line, clean at first, then driven and folded.", "Una línea de bajo, limpia al principio y luego saturada y plegada."),
        _dv("tapechords", "Tape Chords", 88, "D", "Chord-memory shapes in D Dorian.", "Acordes memorizados en Re dórico."),
    ],
    fx=[
        FX(("loop", "dubthrows"), HAL, "p19", "HalDuckedDub", "Dub stabs in a ducked dub space.", "Acordes dub en un espacio dub."),
        FX(("loop", "halflight"), TAR, "p14", "TarLoFiBeat", "Half-time groove on lo-fi tape.", "Groove half-time en cinta lo-fi."),
        FX(("loop", "slowweight"), HAL, "p09", "HalEndlessPad", "Cluster pad in an endless space.", "Pad en cluster en un espacio infinito."),
        FX(("loop", "frozentail"), HAL, "p27", "HalFrozenLows", "Chords with frozen lows.", "Acordes con graves congelados."),
        FX(("loop", "lowswell"), TAR, "p04", "TarBouncedFourTimes", "Low swell bounced four times.", "Low swell copiado cuatro veces."),
        FX(("loop", "tapechords"), TAR, "p06", "TarCassette", "Chords on a dusty cassette.", "Acordes en un casete gastado."),
        FX(("os", "DeviceNTI_RiteBell_C4_v100"), HAL, "p08", "HalCathedral", "Rite bell in a glass cathedral.", "Campana ritual en una catedral de vidrio."),
        FX(("tex", "slowweight"), MUR, "p02", "MurSlowFog", "Pad as a slow grain fog.", "Pad como niebla lenta de granos."),
    ],
)

# ═════════════════════════ Pallor (caja de ritmos) ═════════════════════════
_KITS = [("DarkWave", "Night Shift Mercury"), ("EBM", "Steel Boots"), ("MinimalWave", "Bedroom Rhythm Box"),
         ("Synthpop", "Hairspray Hearts"), ("Industrial", "Rust Choir"), ("Breakbeat", "Rainy Breakbeat")]
_VOICES = [("Kick", "C2", [75, 115], 0.3, 0.9), ("Snare", "D2", [75, 115], 0.3, 1.2), ("Clap", "D#2", [110], 0.3, 1.2), ("Rim", "C#2", [110], 0.2, 0.5),
           ("ClosedHat", "F#2", [110], 0.15, 0.4), ("OpenHat", "A#2", [110], 0.4, 1.0), ("TomLow", "F2", [110], 0.3, 1.2), ("TomMid", "A2", [110], 0.3, 1.2),
           ("TomHigh", "D3", [110], 0.3, 1.2), ("Crash", "C#3", [110], 0.4, 3.0), ("Ride", "D#3", [110], 0.4, 2.0), ("Cowbell", "G#3", [110], 0.2, 0.9),
           ("Tambourine", "F#3", [110], 0.2, 0.8), ("Shaker", "A#4", [110], 0.2, 0.6), ("Clave", "D#5", [110], 0.1, 0.4)]
_pal_os = []
for kit, pn in _KITS:
    for v, note, vels, hold, tail in _VOICES:
        _pal_os.append(OS(f"{v}_{kit}", pn, [note], vels, hold, tail, plain=True))
synth(
    slug="pallor", name="Pallor", folder="Pallor", kind="synth",
    tag=dict(en="Drum machine: dark wave, EBM, post-punk and minimal wave kits", es="Caja de ritmos: kits de dark wave, EBM, post-punk y minimal wave"),
    genres="dark wave, EBM, post-punk, breakbeat",
    oneshots=_pal_os,
    loops=[
        LC("darkwave", "pallor", ["--preset", "Night Shift Mercury"], 120, "", 4, tail=3.0, en="Bridged-T kick and gated snare.", es="Bombo bridged-T y caja con compuerta."),
        LC("ebm", "pallor", ["--preset", "Steel Boots"], 122, "", 4, tail=3.0, en="Kick on every beat, snare and clap slammed together.", es="Bombo en cada tiempo, caja y palmas juntas."),
        LC("motorik", "pallor", ["--preset", "Grey Factory Floor"], 150, "", 4, tail=3.0, en="Motorik post-punk groove.", es="Groove motorik post-punk."),
        LC("minimalwave", "pallor", ["--preset", "Bedroom Rhythm Box"], 110, "", 4, tail=3.0, en="Cheap analog box, small kick and noise snare.", es="Caja analógica barata, bombo chico y caja de ruido."),
        LC("synthpop", "pallor", ["--preset", "Neon Promenade"], 118, "", 4, tail=3.0, en="Eighties sample kit with claps and tambourine.", es="Kit de samples de los ochenta con palmas y pandereta."),
        LC("coldwave", "pallor", ["--preset", "Frost on Concrete"], 132, "", 4, tail=3.0, en="Thin cheap-box drums with a blip clave.", es="Batería fina de caja barata con una clave."),
        LC("newbeat", "pallor", ["--preset", "Slow Leather"], 108, "", 4, tail=3.0, en="Heavy and slow new beat with a gated clap-snare.", es="New beat pesado y lento con caja-palma con compuerta."),
        LC("dnbbreak", "pallor", ["--preset", "Dusty Ritual Break"], 172, "", 4, tail=2.5, en="Busy swung break at drum and bass tempo.", es="Break con swing al tempo del drum and bass."),
    ],
    fx=[
        FX(("loop", "ebm"), TAR, "p03", "TarHotDrumBus", "EBM drums on a hot tape bus.", "Batería EBM en un bus de cinta saturada."),
        FX(("loop", "motorik"), HAL, "p05", "HalSmallRoom", "Motorik groove in a small room.", "Groove motorik en una sala chica."),
        FX(("loop", "minimalwave"), TAR, "p06", "TarCassette", "Minimal wave box on a dusty cassette.", "Caja minimal wave en un casete gastado."),
        FX(("loop", "synthpop"), HAL, "p20", "HalGated1984", "Synthpop kit in a gated room.", "Kit synthpop en una sala con compuerta."),
        FX(("loop", "coldwave"), TAR, "p15", "TarBrokenVHS", "Coldwave drums on a broken VHS.", "Batería coldwave en un VHS roto."),
        FX(("loop", "newbeat"), HAL, "p19", "HalDuckedDub", "New beat in a ducked dub space.", "New beat en un espacio dub."),
        FX(("loop", "dnbbreak"), TAR, "p18", "TarWornVinyl", "Break on worn vinyl.", "Break en vinilo gastado."),
        FX(("loop", "dnbbreak"), HAL, "p16", "HalDottedRain", "Break with dotted echoes.", "Break con ecos con puntillo."),
        FX(("os", "Pallor_Snare_EBM_v115"), HAL, "p20", "HalGated1984", "EBM snare in a gated room.", "Caja EBM en una sala con compuerta."),
        FX(("os", "Pallor_Kick_DarkWave_v115"), TAR, "p12", "TarHotFerric", "Dark wave kick on hot tape.", "Bombo dark wave en cinta saturada."),
        FX(("os", "Pallor_Clap_Synthpop_v110"), HAL, "p08", "HalCathedral", "Synthpop clap in a cathedral.", "Palmas synthpop en una catedral."),
        FX(("os", "Pallor_Crash_Industrial_v110"), HAL, "p06", "HalHall", "Industrial crash in a hall.", "Crash industrial en una sala."),
    ],
)

# ═════════════════════════ Thrum (guitarra) ═════════════════════════
def _th(id, preset, chords, bpm, key, en, es, tail=3.0):
    return LC(id, "thrum", ["--preset", preset, "--chords", chords], bpm, key, 4, tail=tail, en=en, es=es)
synth(
    slug="thrum", name="Thrum", folder="Thrum", kind="synth",
    tag=dict(en="Electric guitar: clean arpeggios, post-punk strums, fuzz and DI", es="Guitarra electrica: arpegios limpios, rasgueos post-punk, fuzz y DI"),
    genres="post-punk, dark wave, shoegaze, rock",
    oneshots=[
        OS("CleanNote", "Warm Jazz Box", ["E3", "A3", "D4", "G4"], [100], 1.2, 1.5),
        OS("CrunchNote", "Small Combo Breakup", ["E2", "A2", "E3"], [100], 1.0, 1.5),
        OS("CleanChordEm", "Sparkle Pop Strum", ["E2+B2+E3+G3+B3+E4"], [100], 1.4, 1.8),
        OS("PowerChordE5", "Classic Stack Eighths", ["E2+B2+E3"], [100], 1.0, 1.5),
        OS("FuzzPowerA5", "Stoner Fuzz", ["A1+E2+A2"], [100], 1.2, 1.5),
        OS("HarmonicBell", "Harmonics Bells", ["E4", "B4"], [100], 0.6, 3.0),
        OS("TwangNote", "Twang and Spring", ["E3", "B3"], [100], 0.8, 2.0),
        OS("BaritoneNote", "Baritone Shadows", ["B1", "E2"], [100], 1.0, 1.5),
        OS("DINote", "DI Single Coils", ["E3", "A3", "E4"], [100], 1.0, 1.2),
        OS("DIChordEm", "DI Single Coils", ["E2+B2+E3+G3+B3+E4"], [100], 1.4, 1.8),
    ],
    loops=[
        _th("coldarp", "Cold Chorus Arpeggio", "Em C G D", 100, "Em", "Chorused arpeggios, Em C G D.", "Arpegios con chorus, Em Do Sol Re."),
        _th("nightdrive", "Night Drive Strum", "Am F C G", 112, "Am", "Steady eighths, Am F C G.", "Corcheas firmes, Am Fa Do Sol."),
        _th("downstrokes", "Post-Punk Downstrokes", "Dm Bb F C", 140, "Dm", "Fast downstrokes, Dm Bb F C.", "Rasgueo rapido hacia abajo, Dm Sib Fa Do."),
        _th("wirestabs", "Wire Stabs", "Em Em G A", 128, "Em", "Cut-off stabs on the off-beats.", "Cortes a contratiempo."),
        _th("icecathedral", "Ice Cathedral", "Cm Ab Eb Bb", 96, "Cm", "Cascading notes into a big hall.", "Notas en cascada hacia una sala enorme.", tail=4.5),
        _th("coldtremolo", "Coldwave Tremolo", "Em Am C B", 118, "Em", "Optical tremolo pulsing in a big room.", "Tremolo optico pulsando en una sala grande."),
        _th("shoegaze", "Shoegaze Wall", "A F#m D E", 108, "A", "Fuzz and a long hall.", "Fuzz y una sala larga.", tail=4.5),
    ],
    fx=[
        FX(("loop", "coldarp"), HAL, "p00", "HalTapeEcho", "Arpeggios through a tape echo.", "Arpegios con un eco de cinta."),
        FX(("loop", "nightdrive"), TAR, "p07", "TarChromeMixtape", "Strum on a chrome mixtape.", "Rasgueo en un casete cromo."),
        FX(("loop", "downstrokes"), TAR, "p03", "TarHotDrumBus", "Downstrokes on a hot tape bus.", "Rasgueo en un bus de cinta saturada."),
        FX(("loop", "wirestabs"), HAL, "p07", "HalSpringSurf", "Stabs through a spring tank.", "Cortes con un resorte."),
        FX(("loop", "icecathedral"), HAL, "p08", "HalCathedral", "Picked notes in a glass cathedral.", "Notas punteadas en una catedral de vidrio."),
        FX(("loop", "coldtremolo"), TAR, "p18", "TarWornVinyl", "Tremolo guitar on worn vinyl.", "Guitarra con tremolo en vinilo gastado."),
        FX(("loop", "shoegaze"), HAL, "p26", "HalReverseShimmer", "Fuzz with a reverse shimmer.", "Fuzz con shimmer invertido."),
        FX(("os", "Thrum_DIChordEm_E2B2E3G3B3E4_v100"), HAL, "p06", "HalHall", "DI chord in a concert hall.", "Acorde DI en una sala de conciertos."),
        FX(("os", "Thrum_HarmonicBell_E4_v100"), HAL, "p00", "HalTapeEcho", "Harmonic bell with tape echo.", "Campana armónica con eco de cinta."),
        FX(("tex", "icecathedral"), MUR, "p03", "MurStarlings", "Guitar scattered as starlings.", "Guitarra dispersa como una bandada."),
    ],
)

# Titulos cortos (EN, ES) para los loops que se escuchan en la web.
TITLES = {
 "at20v/acid": ("Acid line", "Línea acid"), "at20v/fatbass": ("Fat house bass", "Bajo house gordo"), "at20v/hoover": ("Hoover riff", "Riff hoover"),
 "at20v/ringbells": ("Bell arpeggio", "Arpegio de campanas"), "at20v/glasspad": ("Glass pad", "Pad de vidrio"), "at20v/fogdrone": ("Fog drone", "Drone de niebla"), "at20v/drums": ("Drum loop", "Loop de batería"),
 "at101v/housebass": ("Offbeat house bass", "Bajo house a contratiempo"), "at101v/acidline": ("Acid line", "Línea acid"), "at101v/minimalpulse": ("Minimal pulse", "Pulso minimal"),
 "at101v/italooctaves": ("Octave bass", "Bajo de octavas"), "at101v/technostab": ("Techno stabs", "Acordes techno"), "at101v/glassarp": ("Glass arpeggio", "Arpegio de vidrio"), "at101v/darkdrone": ("Dark drone", "Drone oscuro"),
 "at303/acid": ("Classic acid", "Acid clásico"), "at303/squelch": ("Open squelch", "Chillido abierto"), "at303/rolling": ("Rolling sixteenths", "Semicorcheas rodantes"),
 "at303/deepsub": ("Deep sub", "Sub profundo"), "at303/slides": ("Sliding fifths", "Quintas con slides"), "at303/offbeat": ("Offbeat bass", "Bajo a contratiempo"),
 "atmd/funkbass": ("Funk bass", "Bajo funk"), "atmd/squelch": ("Squelch bass", "Bajo chillón"), "atmd/epchords": ("Electric piano", "Piano eléctrico"),
 "atmd/harpsiarp": ("Harpsichord arpeggio", "Arpegio de clave"), "atmd/lucky": ("Singing lead", "Lead cantante"), "atmd/strings": ("String pad", "Pad de cuerdas"), "atmd/drums": ("Drum loop", "Loop de batería"),
 "at700/synthpopbass": ("Synth-pop bass", "Bajo synth-pop"), "at700/brassfanfare": ("Brass chords", "Acordes de metales"), "at700/stringpad": ("Ensemble strings", "Cuerdas ensemble"),
 "at700/syncphrase": ("Sync lead", "Lead con sync"), "at700/pwmarp": ("PWM arpeggio", "Arpegio PWM"), "at700/bellmotif": ("Bell motif", "Motivo de campanas"), "at700/drums": ("Drum loop", "Loop de batería"),
 "atfs1/choirchords": ("Vowel choir", "Coro de vocales"), "atfs1/formantbass": ("Talking bass", "Bajo que habla"), "atfs1/marimba": ("Glass marimba", "Marimba de vidrio"),
 "atfs1/tinecomp": ("Tine piano", "Piano de púas"), "atfs1/lalalead": ("Sung lead", "Lead cantado"), "atfs1/whisperpad": ("Whisper pad", "Pad susurrado"),
 "at42/cathedral": ("Cathedral drone", "Drone de catedral"), "at42/lowchoir": ("Low choir", "Coro grave"), "at42/shimmer": ("Shimmer garden", "Jardín de brillos"),
 "at42/flutepent": ("Flute phrase", "Frase de flauta"), "at42/plates": ("Sequenced plates", "Placas secuenciadas"), "at42/berlinfive": ("Self-playing machine", "Máquina que toca sola"), "at42/bells": ("Ring bells", "Campanas"),
 "at330/stringschoir": ("Strings and choir", "Cuerdas y coro"), "at330/rainneon": ("Neon rain pad", "Pad de lluvia de neón"), "at330/nightpad": ("Dark wave bed", "Base dark wave"),
 "at330/glasshall": ("Bright swells", "Swells brillantes"), "at330/stabs": ("String stabs", "Cortes de cuerdas"), "at330/monks": ("Low monks", "Monjes graves"),
 "devicenti/lowswell": ("Low swell", "Low swell"), "devicenti/halflight": ("Half-light groove", "Groove half-light"), "devicenti/dubthrows": ("Dub throws", "Dub throws"),
 "devicenti/slowweight": ("Slow weight pad", "Pad slow weight"), "devicenti/frozentail": ("Frozen tail", "Frozen tail"), "devicenti/foldbass": ("Drive and fold bass", "Bajo drive and fold"), "devicenti/tapechords": ("Chord memory", "Acordes memorizados"),
 "pallor/darkwave": ("Dark wave kit", "Kit dark wave"), "pallor/ebm": ("EBM kit", "Kit EBM"), "pallor/motorik": ("Motorik groove", "Groove motorik"), "pallor/minimalwave": ("Minimal wave box", "Caja minimal wave"),
 "pallor/synthpop": ("Synth-pop kit", "Kit synth-pop"), "pallor/coldwave": ("Coldwave kit", "Kit coldwave"), "pallor/newbeat": ("New beat", "New beat"), "pallor/dnbbreak": ("Drum and bass break", "Break de drum and bass"),
 "thrum/coldarp": ("Chorus arpeggios", "Arpegios con chorus"), "thrum/nightdrive": ("Night drive strum", "Rasgueo night drive"), "thrum/downstrokes": ("Post-punk downstrokes", "Rasgueo post-punk"),
 "thrum/wirestabs": ("Wire stabs", "Cortes de alambre"), "thrum/icecathedral": ("Ice cathedral", "Catedral de hielo"), "thrum/coldtremolo": ("Coldwave tremolo", "Tremolo coldwave"), "thrum/shoegaze": ("Shoegaze wall", "Pared shoegaze"),
}

# One-shot que se escucha en la tarjeta cuando el sinte no tiene textura granular (por defecto, el del medio de la carpeta).
PREVIEW_OS = {"pallor": "Pallor_Snare_EBM_v115"}
