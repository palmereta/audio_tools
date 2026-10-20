# Halation

A delay and a reverb in one space: an original design, not a recreation. VST3, Audio Unit (macOS, a music
effect with MIDI input), standalone app, and `halationrender`, a command-line renderer. Version 1.0.2.

The research behind it (classic delays and reverbs, feedback delay networks, shimmer, freeze, spectral and
granular processing, what the common delay and reverb devices in DAWs do and where Halation goes further, the
visual direction, the test plan) is in [`docs/research.md`](docs/research.md). The user manual is
[`docs/manual/en.md`](docs/manual/en.md) (and `es.md`), embedded in the plugin.

## How it works

1. **The space** (`Source/dsp/Network.h`). Eight delay lines (Hermite interpolation, 32-bit float) mixed by
   an orthogonal matrix built from Givens rotations (`ButterflyMatrix` in `Core.h`): angle 0 is the identity
   (each line is its own echo), π/4 a Hadamard-like full mix (a room). Delay to Space moves that angle, the
   input diffusers and the line lengths, which are interpolated in log steps from the echo pattern times
   (`patterns()`, Time × pattern) to the room lengths (`rooms()`, Size × room). Ping-pong is a permutation
   between the left and right lines.
2. **Decay.** Each line's gain comes from its own length and the Length time (Jot: `g = 10^(−3·len/T60)`),
   so the tail keeps its T60 at any Delay to Space position. Three complementary one-pole bands per line
   (250 Hz and 2.5 kHz) get their own T60 from Tone (high ratio) and Bass decay (low ratio). Length at the top
   is infinite: gain 1, no damping.
3. **Character.** Tape: wow (0.45 Hz plus a slow drift) and flutter (6.8 Hz) with separate phases per line,
   a soft saturation, a head bump and wear (deeper modulation and short dark dropouts). BBD: a low-pass that
   darkens with the delay time, as a bucket-brigade clock would. Spring: a chain of 24 stretched all-passes.
4. **Freeze.** All: the network stops feeding the input and keeps its energy (gain 1, no damping, DC block
   bypassed). By bands: a 2048-point STFT (hop 512, Hann) that holds the magnitude of the frozen bins captured
   at the moment of freezing and lets the live bins through with their phase.
5. **Modules** (`Source/dsp/Modules.h`). Shimmer: a two-head granular pitch shifter in the feedback path,
   intervals and chords. Grains: a grain cloud reading the tail forward and backward. Reverse: slices of a
   note value played backwards. Spectral: the same STFT with smeared magnitudes and scattered phases (the
   Paulstretch idea). Resonator: comb filters in delay mode, and in a room the line lengths quantised to the
   note's period. Stutter: a slice of the output repeated on the beat. Breathe and Collapse: the room lengths
   moved every 32 samples (deterministic). Duck: Lacquer's compressor as a ducker keyed by the input. Gate:
   an envelope gate with hold.
6. **Safety** (`Source/dsp/Engine.h`). A soft clip on every line, an energy limiter on the loop and a −1 dBFS
   ceiling at the output.

`PluginProcessor` maps the parameters (Length → T60 = 0.2·300^L s, Tone and Bass decay → band ratios, Freeze
mode → network or spectral), applies scene B (Morph) and the envelope follower, and feeds the room view
through atomics and a lock-free onset FIFO. `Source/ui/RoomSim.h` is the particle simulation the editor draws
(`RoomView.h`): pulses leave on onsets, return at the line times, split at walls in a room and leave mist.

## Tests

`halationtest` (`tools/spacetest.cpp`): echo time, decay per round trip, T60 within ±10 % for three rooms,
freeze holding ±1.5 dB for 10 s, robustness, dry passthrough at Mix 0, bit-identical output across block
sizes, the output ceiling, shimmer intervals, spectral freeze, matrix orthogonality, band freeze, the tuned
tail, 200 random extreme settings without NaN, and CPU. On macOS and Windows; plus pluginval at strictness
10, auval (`aumf Halo Atls`) and `athosttest`.

## Web and film

- `tools/blender/maquette.py`: the plaster model and its light particles (pc50-wsl, under the GPU lock).
- `tools/web_images.py`: web images from the renders and the manual screenshots.
- `tools/mkfilm.sh`: the film and its sound (Delay to Cathedral with Morph and Freeze automated).
- Demo clips: `python3 scripts/samples.py --only halation`.
