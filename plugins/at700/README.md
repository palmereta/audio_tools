# AT700

Modular synthesizer modeled on the Roland System-700 (1976): the Main Console (Block 1, 17 modules),
the 701A keyboard controller and the 717A analog sequencer, laid out on one canvas as the cabinets
stand in a studio, plus the extensions listed in the root README. VST3, AU and standalone, macOS and
Windows. The panel graphics are original: no Roland logos or artwork.

| | |
|---|---|
| Version | `AT700_VERSION` in `CMakeLists.txt` (1.0.0) |
| Plugin codes | manufacturer `Atls`, plugin `At70` (instrument, MIDI in, stereo out) |
| Hardware reference | [`docs/hardware-spec.md`](docs/hardware-spec.md), sources in [`docs/ref/CREDITS.md`](docs/ref/CREDITS.md) |
| Offline render | `build/plugins/at700/at700render_artefacts/Release/at700render preset.json out.wav [--notes "60+64:1,62:0.5"] [--midi f.mid] [--run]` |

## Layout

```
tools/panel700.py             canvas in mm (900 x 1090: 717A, Main Console, 701A) → assets/layout700.json,
                              Source/dsp/Jacks700.generated.h (137 jacks: in / out / mult, voice or global),
                              Source/dsp/Params700.generated.h (219 panel parameters), silkscreen700.png, vu_face.png
Source/dsp/Modules700.h       702 VCO (saw core, strong and weak sync), 703A/B SVF and 703E OTA ladder, 704 VCA lin/exp,
                              705 ADSR, 706 LFO, 709 S&H, 708 noise, 707 envelope follower, 711A spring reverb and phaser
Source/dsp/Keyboard701.h      duophonic 701A (CV1 lowest key, CV2 highest), mono and poly extensions, portamento, droop
Source/dsp/Seq717.h           717A: 3 x 12 steps, divide/fine per step, hold, series A/A-B/A-B-C/manual, internal,
                              external (1/32 per pulse) or host clock, pulse shaper
Source/dsp/Engine700.h        console buses as wired at the factory, patching, per-voice duplication for poly
Source/ui/PanelView700.h      canvas (knobs, faders, slide switches, levers, toggles, VU, keys, 1/4" cables) and its port
tools/blender/scene700.py     Blender: background, sprites (6 knob strips, faders, nubs, lever, bats, buttons, plugs),
                              hero stills and film shots with the cabinets standing behind the keyboard
tools/presets700.py           32 factory presets (cables included); level_presets.py sets MONITOR to ≈ −17 dBFS RMS
tools/dsptest.cpp             tuning, filter self-oscillation, passband loss, sequencer, duo CVs, poly CPU, fuzz
```

## Pipelines

```bash
python3 tools/panel700.py
# pc50-wsl (Blender 5.2, CUDA), files in ~/ai/at700: scene700.py, layout700.json, silkscreen700.png, vu_face.png
blender -b -P scene700.py -- --mode bg --samples 160 --out out        # 25 s on the 3090
blender -b -P scene700.py -- --mode sprites --samples 128 --out out   # 6 knob strips x 128 frames
blender -b -P scene700.py -- --mode hero --preset full.json --out out_hero
blender -b -P scene700.py -- --mode film --shot wide|seq|vcos|keys|filters --frames 120 --preset full.json --out film
rsync pc50-wsl:ai/at700/out/ assets/render/out/ && python3 tools/pack_assets700.py
python3 tools/presets700.py && python3 tools/level_presets.py <path to at700render>
clang++ -std=c++20 -O2 -ISource/dsp -I../../shared/dsp tools/dsptest.cpp -o /tmp/dsptest700 && /tmp/dsptest700
```

Blender needs the full panel (defaults + preset) to pose the controls: merge the defaults from
`Params700.generated.h` into the preset's `panel` before `--preset`.

Editor self-test: `AT700_SNAPSHOT=/tmp/x.png AT700_VIEW=0..3 AT700_TAB=0|1 AT700_PRESET=n AT700_KEYS=1 AT700_RUN=1`;
`AT700_SELFTEST=1` also drags three cables (one through a multiple), a knob, a fader and two switches
with synthetic mouse events and prints the result.

## Measured

| What | Value |
|---|---|
| Tuning | A3 = 439.9 Hz at 8' (440); ranges 32'..2' exact octaves; keys C1–C6 = 65.4 … 2092.5 Hz |
| VCF self-oscillation | 703E from RES ≈ 7.5, 703A from ≈ 7.6; tracks the keyboard (346 → 693 → 1395 Hz per octave) |
| 703E passband loss | −9.7 dB at RES 4, −13 dB at RES 7 (no compensation, as the original) |
| CPU (render tool, M4, 2x) | 1.8 % one voice; 6.6 % six-voice strings; 8.8 % eight voices with every module per voice |
| Validation | pluginval strictness 8 (VST3 on macOS), auval; no NaN in a 300-block fuzz with random cables |
| DAW QA fixes (1.0.5) | `at700proctest` P1-P11 (tools/proctest.cpp): an instance prepared again after playing renders bit-identical to a fresh one (was +4.6 dB apart), state byte-stable, fresh = program 0, Init plays, notes released after a host gap (running, stopped, Poly 8, sustain pedal); athosttest VST3 + AU 0 FAIL, pluginval strictness 10 on the VST3 |

## Trampas

- **701–707 are module codes, not cabinets**: the cabinets are Blocks 1–7. The research brief had it
  wrong; `docs/hardware-spec.md` §0 lists the corrections.
- **The 717A clock counts whole notes**: CLOCK RATE is steps per minute with DIVIDE at "1", so a
  sequence at 128 BPM in sixteenths is a rate of 32 with DIVIDE 1/16. Feeding BPM straight into the
  rate made the first sequences run four times too fast.
- **A phaser on a panning channel with its level at 0 is silent**: P.S. OUT is not normalled anywhere
  (as in the original). Presets that use it patch it into a PAN input and raise that channel.
- **The VU face was hidden** inside a solid bezel box (then under an opaque "glass"): the meter is a
  frame of four boxes around a recessed face.
- **Blender objects read before a depsgraph update** have an identity `matrix_world`: parenting them
  to the pivot that stands the cabinets up sent everything to the origin. `stand_up()` updates the
  view layer first and parents with `matrix_parent_inverse = Translation(-pivot)`.
- **Multiples sum their inputs and feed back**: a multiple patched into itself, or two multiples in a
  loop, grow without bound. The editor refuses those cables.
- **The manuals are copyrighted**: they stay outside the repo (`audiotools-private/at700-ref/`).
