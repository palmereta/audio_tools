# AT42

Ambient drone machine modeled on the Elta Music SOLAR 42 family (2023-2025), using the 42N as the
superset, plus the extensions listed in the root README. VST3, AU and standalone, macOS and Windows.
The panel graphics are original: no Elta logos or artwork.

| | |
|---|---|
| Version | `AT42_VERSION` in `CMakeLists.txt` (1.0.0) |
| Plugin codes | manufacturer `Atls`, plugin `At42` (instrument with optional stereo audio input) |
| Hardware reference | [`docs/hardware-spec.md`](docs/hardware-spec.md), sources in [`docs/ref/CREDITS.md`](docs/ref/CREDITS.md) |
| Offline render | `build/plugins/at42/at42render_artefacts/Release/at42render preset.json out.wav [--keys 1,2] [--notes "0:2,7:2"]` |

## Layout

```
tools/panel42.py             panel in mm → assets/layout42.json, Source/dsp/Jacks42.generated.h (jacks and normals),
                             Source/dsp/Params42.generated.h (194 panel parameters), silkscreen42.png
Source/dsp/Drone42.h         negistor drone oscillator, classic voice (5 osc, VOLT coupling, LDR orb, AR), Papa Srapa voice
Source/dsp/Vco42.h           AS3340-style VCO (6 waves, linear FM, sync), ADSR with hold/loop, Polivoks filter, distortion
Source/dsp/Modules42.h       LFO A/B, 5-step sequencer, envelope follower, touch keyboard (19 scales, arp, 16-step seq)
Source/dsp/Effector42.h      13 cartridges x 3 programs (own algorithms), FV-1-style reconstruction filter
Source/dsp/Engine42.h        patch routing with hardware normals, 10-channel mixer, width, filters, effector, master
Source/ExtParams42.h         Settings page parameters; Source/Preset42.h presets with cables and keyboard sequence
Source/ui/                   PanelView42 (panel, cables, orbs, plates, joystick, display), ExtView42, Bars42
tools/blender/scene42.py     Blender: 4 panel colours, sprites (12 knob strips, buttons, bats, keys, plugs), hero, video
tools/pack_assets42.py       renders → assets/ui (fades sprite edges so shadows do not end in squares)
tools/presets42.py           presets; drone chords are tuned by inverting the real tune-pot curve
tools/level_presets.py       sets MASTER so presets sit at about −17 dBFS RMS
tools/dsptest.cpp            levels, every effector program, fuzz with random cables
```

## Pipelines

```bash
python3 tools/panel42.py
# pc50-wsl (Blender 5.2, CUDA), files in ~/ai/at42: scene42.py, layout42.json, silkscreen42.png
blender -b -P scene42.py -- --mode bg,sprites --samples 160 --out out        # ~15 min on the 3090
blender -b -P scene42.py -- --mode hero --samples 160 --preset p.json --out out_hero
blender -b -P scene42.py -- --mode anim --samples 36 --frames 192 --preset p.json --out out_hero
rsync pc50-wsl:ai/at42/out/ assets/render/out/ && python3 tools/pack_assets42.py
python3 tools/presets42.py && python3 tools/level_presets.py <path to at42render>
clang++ -std=c++20 -O2 -ISource/dsp -I../../shared/dsp tools/dsptest.cpp -o /tmp/dsptest42 && /tmp/dsptest42
```

Editor self-test: `AT42_SNAPSHOT=/tmp/x.png AT42_TAB=0|1 AT42_PRESET=n AT42_PANEL=0..3 AT42_KEYS=1`.

## Measured

| What | Value |
|---|---|
| Drone voice 1, 5 oscillators, default tuning | −5.2 dBFS peak, 0.205 RMS before the master |
| Effector | 39 programs produce output with drones playing; no NaN in a 300-block fuzz with random cables |
| Presets | leveled to ≈ −17 dBFS RMS (Piezo Percussion needs an audio input and is left at MASTER 7) |
| CPU (standalone, Apple Silicon, 2x) | 10-20 % of one core for the whole instrument; 1x halves it |
| Validation | pluginval strictness 8 (VST3 + AU on macOS, VST3 with GUI on Windows), auval |

## Trampas

- **Shimmer ran away**: the octave-up signal went back into a reverb with a decay near 1, so the loop
  gain passed 1 and the preset hit the limiter (−2.7 dBFS RMS at the lowest MASTER). The feedback
  now goes through a soft clip with gain 0.32.
- **Knob shadows ended in squares** on the Settings page: the sprite frame (d + 9 mm) was smaller than
  the soft shadow. `pack_assets42.py` fades alpha near the frame edges.
- **Drone voice tabs hidden under the orbs**: the tab at the bottom right sits under the 23 mm orb.
  The classic voices use a tab above the box.
- **MUTE and MOD are interleaved** in the parameter order (one column at a time in `panel42.py`):
  index them as `mute1 + 2i` and `mute1 + 2i + 1`.
- **The manuals are copyrighted**: they stay outside the repo (`audiotools-private/at42-ref/`).
