# ATMD

A recreation of the Moog Minimoog Model D, modelled on a mid-1970s unit with the Rev 2 oscillator board
(the one Moog itself measured for the 2016 reissue). VST3, Audio Unit (macOS) and standalone.
Version 1.0.3.

The panel, the left-hand controller and the 44-key keyboard are a 3D model built to the panel's published
measurements (`assets/layoutmd.json`, `docs/hardware-spec.md` §6-7), rendered in Blender; every control of
the original works.

## What it does

- **Oscillators.** Three VCOs with RANGE LO (32' − 3 oct), 32', 16', 8', 4', 2' and six waveforms: triangle,
  triangle-sawtooth (0.82 tri + 0.18 saw), sawtooth, square, 29 % and 15 % pulse; oscillator 3 has a reverse
  sawtooth in place of the triangle-sawtooth. Oscillators 2 and 3 tune ±7.5 semitones; with OSC 3 CONTROL off,
  oscillator 3 ignores the keyboard, the wheels and the modulation and its FREQUENCY covers ±3 octaves.
  PolyBLEP/PolyBLAMP band limiting, slow analog drift per oscillator.
- **Mixer.** Passive: 33 kΩ per oscillator and external input, 12 kΩ for the noise, straight into the
  ladder's input pair, scaled so that one oscillator at 10 sits near tanh(0.4); several sources saturate the
  filter. White or pink noise to the audio, pink or red to the modulation. External input through a high-gain
  preamp with the OVERLOAD lamp (the plugin's optional side input).
- **Filter.** Four-pole transistor ladder, 24 dB/oct, solved without delay in the loop (TPT) with a
  linearized tanh per stage (`Source/dsp/LadderMd.h`). Cutoff one octave per division, −1.1 = 440 Hz with
  keyboard control off; keyboard control 1/3, 2/3 or full from the low F; contour depth up to about 6 octaves;
  EMPHASIS self-oscillates from about 8 (k = 4) up to k ≈ 4.4; the resonance fades with the cutoff below
  ~300 Hz (the original stopped oscillating near 150 Hz); uncompensated passband loss.
- **Contours.** ATTACK, DECAY, SUSTAIN for the filter and the loudness: RC segments, τ = (100 Ω + log pot of
  1 MΩ) × 10 µF = 1 ms .. 10 s; the attack charges toward 9.3/5 of the peak; the DECAY switch makes the release
  the decay time, otherwise τ = 15 ms. Single trigger: legato notes don't retrigger.
- **Controllers.** TUNE ±2.5 st, GLIDE 1 ms .. 1 s per octave at a near-constant rate on large steps,
  MODULATION MIX (equal-power osc 3 ↔ noise), OSCILLATOR MODULATION and FILTER MODULATION switches; MOD wheel
  with a roughly cubic law (full: about ±9 st on pitch, ±1.25 oct on the cutoff); PITCH wheel ±7 st by default.
- **Keyboard.** Low-note priority, track and hold through the release, 8' = concert pitch.
- **Extensions** (SETTINGS page): up to 8 voices and 4-voice unison with detune and spread, velocity and
  pressure to filter/loudness/modulation, note priority low/last/high, single or multi trigger, pitch wheel
  range, tempo-synced delay, plate reverb, oversampling 1x-8x (4x by default), output level.
- **Presets.** 32 factory presets: Init, Bass, Lead, Brass and keys, Effects and percussion, Drones and pads.
- **Manual.** The MANUAL button opens the full manual (English and Spanish, with screenshots); the same text is
  on the website.

## Layout

```
Source/dsp/VcoMd.h          oscillators (six shapes, band-limited) and white/pink/red noise
Source/dsp/LadderMd.h       the transistor ladder (TPT, tanh per stage)
Source/dsp/ContourMd.h      contour generator, audio-taper pot law
Source/dsp/EngineMd.h       voices, mixer, keyboard logic, glide, modulation, oversampling, delay, reverb
Source/ParamsMd.h           host parameters (panel ids of hardware-spec §10 + extensions) and their mapping
Source/PluginProcessor.*    buses (optional external input), MIDI, wheels, presets, state
Source/ui/                  the rendered panel (AssetsMd.h, PanelViewMd.h) and the SETTINGS page
assets/layoutmd.json        panel, deck and case in millimetres (from the reissue manual's vector drawing)
assets/ui/                  rendered background, sprite sheets, ui.json (contract: docs/ui-assets.md)
docs/hardware-spec.md       the original: controls, circuit, measurements, sources
docs/manual/                the user manual (en.md, es.md), its screenshots and shots.json
tools/panelmd.py            silkscreen texture
tools/blender/scenemd.py    the 3D instrument: editor background, sprites, heroes, film (pc50-wsl, CUDA)
tools/pack_assetsmd.py      renders → assets/ui
tools/dsptest.cpp           engine tests without JUCE
tools/render.cpp            atmdrender: a preset played from a MIDI file, to WAV, with CPU figure
tools/presetsmd.py          factory presets; tools/level_presets.py levels them afterwards
```

## Tools

```bash
# Engine tests (tuning, osc 2 interval, filter self-oscillation and tracking, passband loss, DECAY switch,
# single trigger and low-note priority, glide, osc 3 as LFO, poly and unison, sample rates and blocks,
# 200 random extreme settings, panel laws of hardware-spec §3-5, CPU)
clang++ -std=c++20 -O2 -I plugins/atmd/Source/dsp -I shared/dsp plugins/atmd/tools/dsptest.cpp -o /tmp/atmd_dsptest && /tmp/atmd_dsptest

# A preset played from a MIDI file (or a test phrase without --midi)
build/plugins/atmd/atmdrender_artefacts/Release/atmdrender plugins/atmd/assets/presets/p09_lucky_lead.json out.wav [--midi f.mid] [--os 4]

# Presets
python3 plugins/atmd/tools/presetsmd.py && python3 plugins/atmd/tools/level_presets.py build/plugins/atmd/atmdrender_artefacts/Release/atmdrender
```

The editor saves a snapshot without a screen grab:
`ATMD_SNAPSHOT=/tmp/x.png ATMD_PAGE=0|1 ATMD_PRESET=n ATMD_WAIT=s [ATMD_MANUAL=1] [ATMD_HOLD="48,55,60"] [ATMD_MOD=0..1] [ATMD_BEND=-1..1]`;
`scripts/manual-shots.py atmd` takes the manual's screenshots that way. Re-rendering the panel: `docs/ui-assets.md`.

## Measurements

| Check | Result |
|---|---|
| Sawtooth at 8', A4 | 440.0 Hz (32' and 2' within 0.4 cents) |
| CUTOFF −1.1, EMPHASIS 10, keyboard control off | oscillates at 442 Hz |
| Self-oscillation | none at EMPHASIS 7, from about 8 |
| Keyboard control 1 + 2, one octave | filter moves 2.012× (+10 cents) |
| Ladder tuning, 300 Hz to 15 kHz at 96 kHz | within 12 cents of the requested cutoff |
| Cutoff about 118 Hz, EMPHASIS 10 | no oscillation (the original stopped near 150 Hz) |
| DECAY off, 100-150 ms after release | about −80 dBFS; with DECAY on the note keeps sounding |
| 200 random extreme settings | always finite, peak ≤ 4 (output limiter) |
| CUTOFF automated in 512-sample host blocks, −3 to +3 in 2 s (dsptest 15) | residue above 2 kHz against an 8-sample reference −59 dB (1.0.2: −28 dB, a step per block) |
| Two engines prepared alike, one used before (dsptest 16) | identical sample for sample (1.0.2: up to 0.82 apart) |
| athosttest hostgap, statebytes (1.0.3) | the note is released after a host gap (REAPER bypass); state byte-stable, fresh instance = program 0 |
| pluginval strictness 10 (1.0.3), auval (aumu Atmd Atls) | pass (VST3 on macOS; AU and Windows by the release build) |
| pluginval strictness 8, auval (aumu Atmd Atls) | pass (VST3 and AU on macOS, VST3 on Windows) |
| athosttest | 0 FAIL on macOS and Windows; WARN only for user gain with random parameters and for noise presets that are not sample-identical after a state restore |

CPU, percentage of one core at 48 kHz:

| Setting | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| One voice, three oscillators, 4x (Lucky Lead) | 0.85 % | 1.54 % |
| Eight voices, three oscillators each, 4x (dsptest 12) | 4.79 % | 8.32 % |

## Traps

- **Windows was 3.3× slower than the M4** (16.9 % for eight voices at 4x): `std::tan` of the MSVC runtime on
  every oversampled sample. With `fastTan` and the pitch and cutoff recomputed per oversampled sample only
  when audio-rate modulation actually reaches them, it went to 8.3 % (M4: 4.8 %).

- **Huovilainen's explicit ladder lost the resonance above ~3 kHz and its tuning wandered by up to +290
  cents** at 96 kHz. The TPT ladder with linearized tanh holds the tuning by prewarp and oscillates up to
  15 kHz.
- **Self-oscillation needs a seed.** With the mixer closed the model sits at exactly zero; a thermal noise
  floor of −94 dB at the filter input starts it, as in the circuit.
- **The pressed key barely shows from the editor's angle** (the camera looks 30° from vertical): a light shade
  over the pressed key's outline tells what is sounding.
- **Black keys in perspective renders** use their real tapered shape and long lenses (see the AT101V scene),
  or they look crooked.
