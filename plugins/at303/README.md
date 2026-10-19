# AT303

A bass line synthesizer with its own step sequencer, modelled on the published circuit of a classic 1981 bass
line synthesizer. VST3 instrument, Audio Unit (macOS), standalone app, and `at303render`, a command-line renderer.
Version 1.0.0.

The research behind it (the circuit, block by block, from the service notes and the open reproductions; the
sequencer's memory and write modes; what users ask of the software versions; the test plan) is in
[`docs/research.md`](docs/research.md). The user manual is [`docs/manual/en.md`](docs/manual/en.md) (and `es.md`),
embedded in the plugin. Copyrighted references stay outside the repo.

## How it works

Everything is in `Source/dsp/`, without JUCE, per sample:

1. **Voice** (`Voice303.h`). A band-limited ramp (PolyBLEP); the square comes from the ramp through a model of the
   transistor waveshaper (low-pass, bias follower, tanh). The filter is a four-pole diode ladder with the
   capacitor ratio of the original (C18 = 0.018 µF against 0.033 µF), as TPT integrators with the tanh of each
   stage linearised around its previous state (Padé tanh(x)/x) and the whole loop, feedback high-pass included,
   solved every sample by forward elimination of the tridiagonal system: zero-delay feedback without iteration.
   The cutoff follows f = 300·(top/300)^(u²) so it rings near 500 Hz at half; Env Mod lowers the base as it
   raises the sweep, as on the hardware. The filter envelope (MEG), the volume envelope (VEG, with the 4 ms note
   delay), the accent VCA path and the accent sweep circuit (D24, 47 kΩ, the Resonance pot, C13 1 µF) are their
   RC networks. Slide is the RC glide of the control voltage. 2x, 4x or 8x oversampling with the shared decimator.
2. **Sequencer** (`Seq303.h`). A pattern is the original's two lists: up to 16 pitches in order (note, up, down,
   accent, slide) and a time per step (note, tie, rest). `compile()` turns them into notes; the sequencer runs on
   24 clocks per quarter, 6 per step (8 in triplets), with the gate falling at 3.5/6 of the last step unless a
   slide ties it to the next note. It advances from absolute clock positions, so it stays locked to the host
   after a jump or a loop; consecutive blocks are joined by the last clock it processed, so a step that falls in
   the rounding gap between two host blocks is never lost (test T2b). The generator writes acid lines from a
   seed, a scale and densities; Mutate keeps the rhythm.
3. **Engine** (`Engine303.h`). Sequencer or MIDI (accent by velocity, slide by overlap; the keys always play while
   the sequencer is stopped), the output high-pass (47 Hz, 4.7 Hz with Full bass), drive (soft clip, hard clip,
   fuzz, with a DC blocker after it), a ping-pong delay synced to the tempo, makeup gain and a soft ceiling.

The editor (`PluginEditor.cpp`) draws the rendered instrument (`tools/blender/scene303.py`: an original graphite
design, no marks of any manufacturer) and everything that moves on it: knob indexes over static caps, the step
pads, the seven-segment display, the accent charge bar (the voltage of C13), and pressed keys and buttons cut
from a second render. Inspect draws the circuit live from the engine's state. Only the moving regions repaint.

## Tests

`at303test` (no JUCE):

- T1 compile: time list against pitch list, ties, rests, and editing the rhythm moves the pitches along.
- T2 gate at 3.5/6 and slide without retrigger; T2b the host clock block by block at five tempos and three block
  sizes: every step and every gate-off arrives.
- T3 calibration ring (cutoff at half, resonance full) near 460-500 Hz; T4 accent climb on C13; T5 slide time.
- T6 determinism; T7 no NaN and a bounded output under random parameters; T8 CPU.

`athosttest` (0 FAIL on macOS and Windows; the three state warnings come from the free-running oscillator phase),
pluginval at strictness 10 (macOS and Windows) and auval.

## CPU

At 48 kHz, one core, sequencer running: the default sound at 4x 0.86 % on an Apple M4 and 1.79 % on an AMD Ryzen 9
5900XT; square, distortion, delay and filter FM 1.17 / 3.38 %; 8x 1.65 / 2.99 %. The editor adds about 5 % on the M4.

## Assets

- `assets/ui/`: `bg.jpg`, `keysdown.jpg`, `knob_big.png`, `knob_small.png` and `layout.json`, rendered by
  `blender -b -P tools/blender/scene303.py -- --out <dir>` (copy them here as JPEG/PNG).
- `assets/presets/`: written by `python3 tools/presets.py` (34 presets, each with an original pattern).
- Web: `tools/blender/scene303.py --mode photos` and `--mode film --shot orbit|knobs|pads`, then
  `tools/web_images.py <stills>` and `sh tools/mkfilm.sh <frames>`; demo clips with `scripts/samples.py --only at303`.
