#!/usr/bin/env python3
"""Presets de fábrica de Valence (assets/presets/pNN_*.json): seis o siete por grupo, con los nueve de las demos
(los preset.json exactos de las demos, ver DEMOS).

Cada preset nombra sólo lo que cambia (el resto queda en el valor por defecto de Source/ParamsVal.h). Las filas de
pasos van en "rows": {"l1": {"on": [...], "vel": [...], "accent", "tie", "ratchet", "prob", "oct", "fixed", "cc",
"finger"}}; una fila más corta que 32 se repite (un acento cada 4 pasos es [1, 0, 0, 0]).
Uso: python3 plugins/valence/tools/presets.py
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "presets")

GROUPS = ["Minimal Techno", "Minimal House", "Drum and Bass", "Acid", "Dub", "Ambient", "Classics"]
FIG = {f: i for i, f in enumerate(["1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D", "1/16", "1/16T", "1/16D", "1/32", "1/32T", "1/32D"])}
PAT = {p: i for i, p in enumerate(["up", "down", "updown", "updownrep", "converge", "diverge", "played", "random", "walk", "chord", "pedal", "finger"])}
VOICE = {"pluck": 0, "bass": 1, "stab": 2, "sub": 3}
OCTMODE = {"stack": 0, "alternate": 1, "random": 2}
VELMODE = {"fixed": 0, "played": 1, "steps": 2}
COMPLETE = {"off": 0, "triad": 1, "seventh": 2, "ninth": 3, "octaves": 4}
ORDER = {"played": 0, "low": 1, "high": 2, "random": 3}
SC = {"major": 0, "minor": 1, "dorian": 2, "phrygian": 3, "lydian": 4, "mixolydian": 5, "locrian": 6, "harmonic minor": 7,
      "phrygian dominant": 8, "double harmonic": 9, "major penta": 10, "minor penta": 11, "hirajoshi": 12, "in-sen": 13,
      "whole tone": 14, "lydian aug": 15}
KEY = {"C": 0, "C#": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}


def layer(n, rate="1/16", length=16, pattern="up", voice="pluck", rows=None, **kw):
    """Una capa: sus parámetros (prefijo lN_) y sus filas."""
    p = {f"l{n}_on": 1, f"l{n}_rate": FIG[rate], f"l{n}_length": length, f"l{n}_pattern": PAT[pattern], f"l{n}_voice": VOICE[voice]}
    for k, v in kw.items():
        if k == "oct_mode": v = OCTMODE[v]
        if k == "vel_mode": v = VELMODE[v]
        if k == "restart": v = 1 if v else 0
        p[f"l{n}_{k}"] = v
    return n, p, rows or {}


PRESETS = []


def preset(name, group, key, scale, layers, **g):
    params = {f"l{n}_on": 0 for n in range(1, 5)}
    rows = {}
    for n, p, r in layers:
        params.update(p)
        if r: rows[f"l{n}"] = r
    params.update({"key": KEY[key], "scale": SC[scale]})
    for k, v in g.items():
        if k == "complete": v = COMPLETE[v]
        if k == "order": v = ORDER[v]
        if k == "source": v = 1 if v == "auto" else 0
        params[k] = v
    PRESETS.append({"name": name, "category": group, "params": params, "rows": rows})


# Los nueve presets de las demos: exactamente los preset.json con que se rindieron las demos
# (audiotools-private/valence/demos/NN_*/preset.json), canales por capa, sin reinicio y con sus filas exactas.
# valencerender con estos archivos y el input.mid de cada demo da los mismos layerN.mid.
DEMOS = json.loads(r"""
{
 "Acid Shell": {
  "params": {
   "accent": 24,
   "complete": 0,
   "key": 9,
   "l1_cc": 74,
   "l1_channel": 1,
   "l1_gate": 0.45,
   "l1_length": 16,
   "l1_mutate": 0.0,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 10,
   "l1_rate": 6,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 54,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.25,
   "l2_length": 7,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": 2,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 9,
   "l2_rate": 6,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 54,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 11,
   "tempo": 130
  },
  "rows": {
   "l1": {
    "accent": [
     1,
     0,
     0,
     1,
     0,
     0,
     1,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0
    ],
    "cc": [
     30,
     36,
     44,
     52,
     60,
     66,
     70,
     74,
     76,
     74,
     68,
     60,
     52,
     44,
     38,
     34
    ],
    "oct": [
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0,
     0
    ],
    "ratchet": [
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     3
    ],
    "tie": [
     0,
     0,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     1,
     0,
     0
    ],
    "vel": [
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96
    ]
   },
   "l2": {
    "on": [
     1,
     0,
     0,
     0,
     0,
     0,
     0
    ],
    "prob": [
     85,
     85,
     85,
     85,
     85,
     85,
     85
    ],
    "vel": [
     100,
     100,
     100,
     100,
     100,
     100,
     100
    ]
   }
  }
 },
 "Bubble": {
  "params": {
   "accent": 24,
   "complete": 0,
   "key": 7,
   "l1_cc": -1,
   "l1_channel": 1,
   "l1_gate": 0.4,
   "l1_length": 16,
   "l1_mutate": 0.0,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 10,
   "l1_rate": 6,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 58,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.35,
   "l2_length": 12,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": 0,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 0,
   "l2_rate": 4,
   "l2_restart": 0,
   "l2_shift": 1,
   "l2_swing": 50,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 3,
   "tempo": 122
  },
  "rows": {
   "l1": {
    "accent": [
     1,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0,
     0,
     0
    ],
    "oct": [
     -2,
     -1,
     -2,
     -1,
     -2,
     -1,
     -2,
     -1,
     -2,
     -1,
     -2,
     -1,
     -2,
     -1,
     -2,
     -1
    ],
    "on": [
     1,
     1,
     1,
     0,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     0,
     1,
     1,
     1,
     1
    ],
    "vel": [
     91,
     85,
     100,
     90,
     86,
     85,
     95,
     88,
     88,
     86,
     100,
     92,
     108,
     85,
     98,
     90
    ]
   },
   "l2": {
    "on": [
     1,
     1,
     1,
     1,
     1,
     0,
     1,
     1,
     1,
     1,
     0,
     1
    ],
    "vel": [
     96,
     80,
     80,
     96,
     80,
     80,
     96,
     80,
     80,
     96,
     80,
     80
    ]
   }
  }
 },
 "Dub Chord": {
  "params": {
   "accent": 24,
   "complete": 0,
   "key": 0,
   "l1_cc": -1,
   "l1_channel": 1,
   "l1_gate": 0.2,
   "l1_length": 16,
   "l1_mutate": 0.35,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 9,
   "l1_rate": 3,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 50,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.9,
   "l2_length": 4,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": -2,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 11,
   "l2_rate": 0,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 50,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 5,
   "tempo": 124
  },
  "rows": {
   "l1": {
    "on": [
     0,
     1,
     0,
     0,
     1,
     0,
     1,
     0,
     0,
     1,
     0,
     1,
     0,
     0,
     1,
     0
    ],
    "prob": [
     60,
     100,
     60,
     60,
     60,
     60,
     60,
     60,
     60,
     100,
     60,
     60,
     60,
     60,
     60,
     60
    ],
    "vel": [
     84,
     100,
     84,
     84,
     84,
     84,
     84,
     84,
     84,
     100,
     84,
     84,
     84,
     84,
     84,
     84
    ]
   },
   "l2": {
    "finger": [
     0,
     0,
     0,
     0
    ],
    "vel": [
     100,
     100,
     100,
     100
    ]
   }
  }
 },
 "Halftime": {
  "params": {
   "accent": 24,
   "complete": 0,
   "key": 2,
   "l1_cc": -1,
   "l1_channel": 1,
   "l1_gate": 0.55,
   "l1_length": 23,
   "l1_mutate": 0.5,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 8,
   "l1_rate": 3,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 50,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.55,
   "l2_length": 29,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": 0,
   "l2_octaves": 2,
   "l2_on": 1,
   "l2_pattern": 0,
   "l2_rate": 3,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 50,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 23,
   "tempo": 172
  },
  "rows": {
   "l1": {
    "on": [
     1,
     1,
     0,
     1,
     1,
     0,
     1,
     1,
     0,
     1,
     1,
     0,
     1,
     1,
     0,
     1,
     1,
     0,
     1,
     1,
     0,
     1,
     1
    ],
    "vel": [
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92,
     92
    ]
   },
   "l2": {
    "on": [
     1,
     1,
     0,
     0,
     1,
     1,
     0,
     0,
     1,
     1,
     0,
     0,
     1,
     1,
     0,
     0,
     1,
     1,
     0,
     0,
     1,
     1,
     0,
     0,
     1,
     1,
     0,
     0,
     1
    ],
    "vel": [
     60,
     67,
     74,
     81,
     88,
     64,
     71,
     78,
     85,
     61,
     68,
     75,
     82,
     89,
     65,
     72,
     79,
     86,
     62,
     69,
     76,
     83,
     90,
     66,
     73,
     80,
     87,
     63,
     70
    ]
   }
  }
 },
 "Jack Groove": {
  "params": {
   "accent": 24,
   "complete": 0,
   "key": 4,
   "l1_cc": -1,
   "l1_channel": 1,
   "l1_gate": 0.5,
   "l1_length": 16,
   "l1_mutate": 0.0,
   "l1_oct_mode": 0,
   "l1_octave": -1,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 10,
   "l1_rate": 6,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 56,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.45,
   "l2_length": 8,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": 0,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 2,
   "l2_rate": 3,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 56,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 21,
   "tempo": 124
  },
  "rows": {
   "l1": {
    "accent": [
     1,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0,
     0,
     0
    ],
    "on": [
     1,
     1,
     1,
     1,
     0,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     0,
     1,
     1,
     1
    ],
    "prob": [
     100,
     100,
     40,
     100,
     100,
     40,
     100,
     40,
     100,
     100,
     40,
     100,
     100,
     40,
     100,
     100
    ],
    "vel": [
     94,
     96,
     38,
     96,
     96,
     38,
     96,
     38,
     94,
     96,
     38,
     96,
     96,
     38,
     96,
     96
    ]
   },
   "l2": {
    "on": [
     0,
     1,
     1,
     1,
     0,
     1,
     1,
     1
    ],
    "vel": [
     84,
     100,
     84,
     100,
     84,
     100,
     84,
     100
    ]
   }
  }
 },
 "Late Shuffle": {
  "params": {
   "accent": 24,
   "complete": 2,
   "key": 9,
   "l1_cc": -1,
   "l1_channel": 1,
   "l1_gate": 0.4,
   "l1_length": 16,
   "l1_mutate": 0.0,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 0,
   "l1_rate": 6,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 60,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.6,
   "l2_length": 12,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": -2,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 10,
   "l2_rate": 3,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 60,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 9,
   "tempo": 120
  },
  "rows": {
   "l1": {
    "on": [
     1,
     1,
     1,
     1,
     1,
     1,
     0,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     0,
     1
    ],
    "vel": [
     104,
     82,
     82,
     82,
     104,
     82,
     82,
     82,
     104,
     82,
     82,
     82,
     104,
     82,
     82,
     82
    ]
   },
   "l2": {
    "on": [
     1,
     1,
     1,
     0,
     1,
     1,
     1,
     1,
     1,
     0,
     1,
     1
    ],
    "vel": [
     100,
     100,
     100,
     100,
     100,
     100,
     100,
     100,
     100,
     100,
     100,
     100
    ]
   }
  }
 },
 "Liquid Rolls": {
  "params": {
   "accent": 24,
   "complete": 0,
   "key": 0,
   "l1_cc": -1,
   "l1_channel": 1,
   "l1_gate": 0.3,
   "l1_length": 7,
   "l1_mutate": 0.0,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 2,
   "l1_on": 1,
   "l1_pattern": 2,
   "l1_rate": 6,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 52,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.3,
   "l2_length": 11,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": 1,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 7,
   "l2_rate": 4,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 50,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 0,
   "seed": 13,
   "tempo": 174
  },
  "rows": {
   "l1": {
    "vel": [
     96,
     78,
     78,
     78,
     78,
     78,
     78
    ]
   },
   "l2": {
    "prob": [
     75,
     75,
     75,
     75,
     75,
     75,
     75,
     75,
     75,
     75,
     75
    ],
    "ratchet": [
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     2
    ],
    "vel": [
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86
    ]
   }
  }
 },
 "Phase Drift": {
  "params": {
   "accent": 24,
   "complete": 0,
   "key": 9,
   "l1_cc": 74,
   "l1_channel": 1,
   "l1_gate": 0.35,
   "l1_length": 16,
   "l1_mutate": 0.0,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 2,
   "l1_rate": 6,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 52,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.3,
   "l2_length": 15,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": 1,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 6,
   "l2_rate": 6,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 52,
   "l2_vel_mode": 2,
   "l3_cc": -1,
   "l3_channel": 3,
   "l3_gate": 0.3,
   "l3_length": 12,
   "l3_mutate": 0.0,
   "l3_oct_mode": 0,
   "l3_octave": 0,
   "l3_octaves": 1,
   "l3_on": 1,
   "l3_pattern": 7,
   "l3_rate": 6,
   "l3_restart": 0,
   "l3_shift": 0,
   "l3_swing": 52,
   "l3_vel_mode": 2,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 7,
   "tempo": 128
  },
  "rows": {
   "l1": {
    "accent": [
     1,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0
    ],
    "cc": [
     40,
     52,
     64,
     76,
     88,
     90,
     80,
     68,
     56,
     46,
     40,
     48,
     60,
     72,
     84,
     70
    ],
    "vel": [
     88,
     86,
     86,
     86,
     104,
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86,
     86
    ]
   },
   "l2": {
    "on": [
     1,
     0,
     1,
     0,
     0,
     1,
     0,
     1,
     0,
     1,
     0,
     0,
     1,
     0,
     0
    ],
    "prob": [
     100,
     70,
     70,
     70,
     70,
     100,
     70,
     70,
     70,
     100,
     70,
     70,
     70,
     70,
     70
    ],
    "vel": [
     92,
     74,
     74,
     92,
     74,
     74,
     92,
     74,
     74,
     92,
     74,
     74,
     92,
     74,
     74
    ]
   },
   "l3": {
    "oct": [
     1,
     0,
     1,
     0,
     1,
     0,
     1,
     0,
     1,
     0,
     1,
     0
    ],
    "on": [
     1,
     0,
     0,
     1,
     0,
     1,
     0,
     0,
     1,
     0,
     1,
     0
    ],
    "vel": [
     84,
     84,
     84,
     84,
     84,
     84,
     84,
     84,
     84,
     84,
     84,
     84
    ]
   }
  }
 },
 "Reese Engine": {
  "params": {
   "accent": 24,
   "complete": 1,
   "key": 5,
   "l1_cc": 74,
   "l1_channel": 1,
   "l1_gate": 1.0,
   "l1_length": 16,
   "l1_mutate": 0.0,
   "l1_oct_mode": 0,
   "l1_octave": 0,
   "l1_octaves": 1,
   "l1_on": 1,
   "l1_pattern": 10,
   "l1_rate": 6,
   "l1_restart": 0,
   "l1_shift": 0,
   "l1_swing": 50,
   "l1_vel_mode": 2,
   "l2_cc": -1,
   "l2_channel": 2,
   "l2_gate": 0.15,
   "l2_length": 5,
   "l2_mutate": 0.0,
   "l2_oct_mode": 0,
   "l2_octave": 2,
   "l2_octaves": 1,
   "l2_on": 1,
   "l2_pattern": 9,
   "l2_rate": 6,
   "l2_restart": 0,
   "l2_shift": 0,
   "l2_swing": 50,
   "l2_vel_mode": 2,
   "l3_on": 0,
   "l4_on": 0,
   "order": 1,
   "out_mode": 1,
   "scale": 1,
   "seed": 17,
   "tempo": 172
  },
  "rows": {
   "l1": {
    "accent": [
     1,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     1,
     0,
     0,
     0,
     0,
     0
    ],
    "cc": [
     20,
     34,
     50,
     70,
     90,
     100,
     84,
     60,
     40,
     56,
     76,
     96,
     110,
     92,
     64,
     40
    ],
    "on": [
     1,
     1,
     1,
     0,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     0,
     1,
     1
    ],
    "ratchet": [
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     4,
     1,
     1,
     1,
     1,
     1,
     1,
     1,
     4
    ],
    "vel": [
     86,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     96,
     86,
     96,
     96,
     96,
     96,
     96
    ]
   },
   "l2": {
    "on": [
     1,
     0,
     0,
     0,
     0
    ],
    "vel": [
     104,
     104,
     104,
     104,
     104
    ]
   }
  }
 }
}
""")


# El timbre del sonido de prueba de cada capa (sólo cambia el audio de prueba, no el MIDI).
DEMO_VOICES = {"Phase Drift": ["pluck", "pluck", "pluck"], "Acid Shell": ["bass", "stab"], "Dub Chord": ["stab", "sub"],
               "Bubble": ["bass", "pluck"], "Jack Groove": ["bass", "stab"], "Late Shuffle": ["pluck", "bass"],
               "Liquid Rolls": ["pluck", "pluck"], "Reese Engine": ["bass", "stab"], "Halftime": ["pluck", "pluck"]}


def demo(name, group):
    d = DEMOS[name]
    params = dict(d["params"])
    for i, v in enumerate(DEMO_VOICES[name], 1): params[f"l{i}_voice"] = VOICE[v]
    PRESETS.append({"name": name, "category": group, "params": params, "rows": dict(d["rows"])})


def euclid_row(pulses, steps, rot=0):
    return [1 if (((k + rot) % steps) * pulses) % steps < pulses else 0 for k in range(steps)]


# ───────── Minimal Techno ─────────
demo("Phase Drift", "Minimal Techno")
preset("Prime Lattice", "Minimal Techno", "D", "phrygian", [
    layer(1, "1/16", 13, "up", "pluck", gate=0.3, octaves=2, eu_pulses=8),
    layer(2, "1/16", 11, "down", "pluck", gate=0.25, octave=1, eu_pulses=5),
    layer(3, "1/16", 7, "pedal", "bass", gate=0.45, octave=-1, eu_pulses=4)],
    tempo=127, complete="triad")
preset("Concrete Pulse", "Minimal Techno", "F", "minor", [
    layer(1, "1/16", 16, "random", "stab", gate=0.2, eu_pulses=5, eu_rot=2, cc=74,
          rows={"cc": [40, 90, 55, 70]}),
    layer(2, "1/16", 16, "up", "pluck", gate=0.3, eu_pulses=7, octave=1),
    layer(4, "1/8", 8, "pedal", "sub", gate=0.6, octave=-2)],
    tempo=129, complete="seventh", order="random")
preset("Hollow Loop", "Minimal Techno", "C", "dorian", [
    layer(1, "1/16", 16, "pedal", "bass", gate=0.4, octave=-1,
          rows={"accent": [1, 0, 0, 0, 0, 0, 1, 0], "prob": [100, 80, 100, 60]}),
    layer(2, "1/16", 14, "updown", "pluck", gate=0.3, octaves=2, oct_mode="alternate", mutate=0.25)],
    tempo=126, complete="ninth")
preset("Tilt Shift", "Minimal Techno", "G", "minor", [
    layer(1, "1/16", 16, "up", "pluck", gate=0.3),
    layer(2, "1/16", 16, "up", "pluck", gate=0.3, shift=2, octave=1, swing=54),
    layer(3, "1/8", 9, "walk", "bass", gate=0.5, octave=-1)],
    tempo=128, complete="seventh")
preset("Static Bloom", "Minimal Techno", "Eb", "minor", [
    layer(1, "1/16", 16, "updownrep", "pluck", gate=0.25, cc=74, vel_mode="steps",
          rows={"cc": [20, 28, 36, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124, 110, 70],
                "vel": [110, 60, 80, 60, 100, 60, 80, 70], "prob": [100, 50, 100, 75]}),
    layer(2, "1/16", 15, "random", "stab", gate=0.2, eu_pulses=4, mutate=0.4)],
    tempo=130, complete="seventh")

# ───────── Minimal House ─────────
demo("Bubble", "Minimal House")
demo("Jack Groove", "Minimal House")
demo("Late Shuffle", "Minimal House")
preset("Velvet Keys", "Minimal House", "Bb", "major", [
    layer(1, "1/8", 8, "chord", "stab", gate=0.3, swing=55, rows={"on": [0, 1], "accent": [0, 0, 0, 1]}),
    layer(2, "1/16", 13, "diverge", "pluck", gate=0.3, swing=55, octave=1, mutate=0.2)],
    tempo=123, complete="ninth")
preset("Warm Floor", "Minimal House", "E", "minor", [
    layer(1, "1/16", 16, "pedal", "bass", gate=0.35, swing=56, octave=-1),
    layer(2, "1/16", 16, "updown", "pluck", gate=0.3, swing=56, octaves=2, rows={"prob": [100, 70]}),
    layer(3, "1/8T", 6, "random", "stab", gate=0.25, eu_pulses=2)],
    tempo=122, complete="seventh")
preset("Sunday Loop", "Minimal House", "G", "mixolydian", [
    layer(1, "1/16", 12, "up", "pluck", gate=0.35, swing=57, octaves=2, oct_mode="alternate"),
    layer(2, "1/16", 16, "pedal", "bass", gate=0.3, swing=57, octave=-1, rows={"on": [1, 0, 1, 1, 0, 1, 1, 0]})],
    tempo=121, complete="triad")

# ───────── Drum and Bass ─────────
demo("Liquid Rolls", "Drum and Bass")
demo("Reese Engine", "Drum and Bass")
demo("Halftime", "Drum and Bass")
preset("Neuro Steps", "Drum and Bass", "D", "phrygian dominant", [
    layer(1, "1/32", 16, "random", "bass", gate=0.5, octave=-1, cc=74,
          rows={"ratchet": [1, 1, 2, 1, 1, 4, 1, 1], "cc": [10, 127, 30, 90, 50, 110], "on": [1, 0, 1, 1, 0, 1, 0, 1]}),
    layer(2, "1/16", 9, "chord", "stab", gate=0.2, eu_pulses=3)],
    tempo=174, complete="triad")
preset("Roller", "Drum and Bass", "A", "minor", [
    layer(1, "1/16", 16, "pedal", "bass", gate=0.45, octave=-1, rows={"accent": [1, 0, 0, 1, 0, 0, 1, 0], "on": [1, 1, 0, 1, 1, 0, 1, 1]}),
    layer(2, "1/16", 6, "up", "pluck", gate=0.3, octave=1)],
    tempo=175, complete="seventh")
preset("Atmos Drift", "Drum and Bass", "Bb", "dorian", [
    layer(1, "1/8T", 13, "updown", "pluck", gate=0.6, octaves=3, oct_mode="alternate", humanize=6),
    layer(2, "1/16", 11, "random", "pluck", gate=0.3, octave=1, rows={"prob": [70, 40, 90, 50]}),
    layer(3, "1/4", 4, "chord", "stab", gate=0.9, octave=-1)],
    tempo=170, complete="ninth")

# ───────── Acid ─────────
preset("Rubber Line", "Acid", "A", "minor", [
    layer(1, "1/16", 16, "walk", "bass", gate=0.5, octave=-1, cc=74,
          rows={"accent": [1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0], "tie": [0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0],
                "oct": [0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0], "cc": [60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 100, 96, 92, 88, 84, 80]})],
    tempo=130, complete="octaves", accent=30)
demo("Acid Shell", "Acid")
preset("Squelch Walk", "Acid", "E", "phrygian", [
    layer(1, "1/16", 16, "random", "bass", gate=0.45, octave=-1, cc=74, mutate=0.3,
          rows={"accent": [1, 0, 0, 0, 0, 1, 0, 0], "tie": [0, 0, 0, 1], "cc": [30, 110, 50, 90, 70, 127, 40, 80]}),
    layer(2, "1/16", 7, "chord", "stab", gate=0.15, rows={"on": [1, 0, 0, 0, 0, 0, 0]})],
    tempo=132, complete="triad", accent=28)
preset("Acid Ratchet", "Acid", "G", "minor", [
    layer(1, "1/16", 16, "updown", "bass", gate=0.4, octave=-1, cc=74,
          rows={"ratchet": [1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 4], "accent": [1, 0, 0, 0, 0, 0, 1, 0],
                "tie": [0, 0, 0, 0, 1, 0, 0, 0], "cc": [40, 50, 60, 70, 80, 90, 100, 110]})],
    tempo=130, complete="octaves", accent=26)
preset("Tension Wire", "Acid", "C", "phrygian dominant", [
    layer(1, "1/16", 15, "converge", "bass", gate=0.35, octave=-1, cc=74, rows={"accent": [1, 0, 0], "cc": [90, 30, 60]}),
    layer(2, "1/16", 16, "pedal", "pluck", gate=0.2, octave=1, eu_pulses=6)],
    tempo=134, complete="triad", accent=24)
preset("Resonant Steps", "Acid", "D", "dorian", [
    layer(1, "1/16", 16, "finger", "bass", gate=0.5, octave=-1, cc=74, octaves=2,
          rows={"finger": [0, 0, 1, 0, 2, 0, 1, 3], "accent": [1, 0, 0, 0, 1, 0, 0, 0], "tie": [0, 1, 0, 0, 0, 0, 1, 0],
                "cc": [100, 80, 60, 40, 60, 80, 100, 120]})],
    tempo=128, complete="seventh", accent=24)
preset("Night Glide", "Acid", "F", "minor", [
    layer(1, "1/16", 12, "up", "bass", gate=0.7, octave=-1, cc=74, rows={"tie": [0, 1, 1, 0, 0, 1], "cc": [50, 70, 90, 110, 90, 70]}),
    layer(4, "1/4", 4, "pedal", "sub", gate=0.8, octave=-2)],
    tempo=128, complete="octaves")

# ───────── Dub ─────────
demo("Dub Chord", "Dub")
preset("Echo Chamber", "Dub", "G", "dorian", [
    layer(1, "1/8D", 11, "chord", "stab", gate=0.15, mutate=0.25),
    layer(2, "1/16", 16, "up", "pluck", gate=0.2, eu_pulses=3, octave=1)],
    tempo=120, complete="ninth")
preset("Deep Stab", "Dub", "C", "minor", [
    layer(1, "1/8", 8, "chord", "stab", gate=0.25, rows={"on": [0, 1, 0, 0, 0, 0, 1, 0], "accent": [0, 1]}),
    layer(2, "1/8", 8, "pedal", "bass", gate=0.6, octave=-1, rows={"on": [1, 0, 0, 1, 0, 0, 1, 0]})],
    tempo=122, complete="seventh")
preset("Fog Lights", "Dub", "F", "minor", [
    layer(1, "1/8T", 12, "chord", "stab", gate=0.15, mutate=0.4, rows={"prob": [100, 0, 60, 0, 30, 0]}),
    layer(4, "1/4", 8, "pedal", "sub", gate=0.85, octave=-2)],
    tempo=118, complete="ninth")
preset("Sub Pressure", "Dub", "A", "phrygian", [
    layer(1, "1/4", 4, "pedal", "sub", gate=0.9, octave=-2),
    layer(2, "1/8", 6, "chord", "stab", gate=0.2, rows={"on": [0, 0, 1, 0, 0, 0]})],
    tempo=126, complete="triad")
preset("Steam Room", "Dub", "Eb", "dorian", [
    layer(1, "1/8", 10, "chord", "stab", gate=0.2, mutate=0.35, eu_pulses=3),
    layer(2, "1/16", 7, "random", "pluck", gate=0.15, octave=1, rows={"prob": [60, 30, 45]})],
    tempo=121, complete="seventh")

# ───────── Ambient (la raíz Orrery: dos tocan solas con la armonía automática; Play) ─────────
preset("Auto Orbit", "Ambient", "D", "dorian", [
    layer(1, "1/8", 13, "updown", "pluck", gate=0.8, octaves=2, humanize=10),
    layer(2, "1/8T", 11, "random", "pluck", gate=0.7, octave=1, rows={"prob": [80, 50, 70]}),
    layer(3, "1/4", 7, "chord", "stab", gate=1.0, octave=-1, rows={"prob": [70]}),
    layer(4, "1/4", 5, "pedal", "sub", gate=1.0, octave=-2)],
    tempo=84, complete="ninth", source="auto", chord_bars=4)
preset("Glass Cycles", "Ambient", "E", "lydian", [
    layer(1, "1/8", 17, "converge", "pluck", gate=0.9, octaves=3, oct_mode="alternate", humanize=12),
    layer(2, "1/8", 19, "diverge", "pluck", gate=0.8, octave=1, humanize=12)],
    tempo=76, complete="ninth")
preset("Slow Bells", "Ambient", "C", "major penta", [
    layer(1, "1/4", 7, "random", "pluck", gate=1.0, octaves=2, octave=1, humanize=15, rows={"prob": [90, 60]}),
    layer(2, "1/4T", 11, "walk", "pluck", gate=1.0, octave=1, humanize=15)],
    tempo=70, complete="triad")
preset("Cloud Chamber", "Ambient", "A", "minor penta", [
    layer(1, "1/16", 29, "random", "pluck", gate=0.5, octaves=3, oct_mode="random", rows={"prob": [35, 20, 50]}, humanize=20),
    layer(2, "1/8", 23, "walk", "pluck", gate=0.8, rows={"prob": [70, 40]}, humanize=20)],
    tempo=90, complete="seventh")
preset("Long Tide", "Ambient", "F", "major", [
    layer(1, "1/8", 31, "updownrep", "pluck", gate=1.0, octaves=2, humanize=8, mutate=0.2),
    layer(3, "1/4", 3, "chord", "stab", gate=1.0, octave=-1)],
    tempo=66, complete="ninth")
preset("Polar Night", "Ambient", "B", "phrygian", [
    layer(1, "1/8T", 19, "updown", "pluck", gate=0.9, octaves=2, humanize=12, mutate=0.3),
    layer(2, "1/4", 13, "random", "pluck", gate=1.0, octave=1, rows={"prob": [60]}),
    layer(4, "1/4", 8, "pedal", "sub", gate=1.0, octave=-2)],
    tempo=72, complete="seventh", source="auto", chord_bars=8, progression=2)

# ───────── Classics (una capa, para quien quiere algo simple) ─────────
preset("Simple Up", "Classics", "C", "major", [layer(1, "1/16", 16, "up", "pluck", gate=0.5)], tempo=120)
preset("Simple Down", "Classics", "C", "major", [layer(1, "1/16", 16, "down", "pluck", gate=0.5)], tempo=120)
preset("Up and Down", "Classics", "C", "major", [layer(1, "1/16", 16, "updown", "pluck", gate=0.5)], tempo=120)
preset("Two Octaves Up", "Classics", "C", "major", [layer(1, "1/16", 16, "up", "pluck", gate=0.5, octaves=2)], tempo=120)
preset("Random Notes", "Classics", "C", "major", [layer(1, "1/16", 16, "random", "pluck", gate=0.5)], tempo=120)
preset("Chord Stabs", "Classics", "C", "major", [layer(1, "1/8", 8, "chord", "stab", gate=0.3)], tempo=120)


def slug(s):
    return "".join(c if c.isalnum() else "_" for c in s.lower()).strip("_")


def main():
    assert len(PRESETS) >= 40, len(PRESETS)
    for p in PRESETS: assert p["category"] in GROUPS, p["name"]
    names = [p["name"] for p in PRESETS]
    assert len(set(names)) == len(names), "nombres repetidos"
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json") and f.startswith("p"): os.remove(os.path.join(OUT, f))
    order = sorted(PRESETS, key=lambda p: GROUPS.index(p["category"]))
    for i, p in enumerate(order, 1):
        with open(os.path.join(OUT, f"p{i:02d}_{slug(p['name'])}.json"), "w") as fh:
            json.dump(p, fh, indent=1, sort_keys=True)
            fh.write("\n")
    print(f"{len(order)} presets en {OUT}")
    for g in GROUPS: print(f"  {g}: " + ", ".join(p["name"] for p in order if p["category"] == g))


if __name__ == "__main__":
    main()
