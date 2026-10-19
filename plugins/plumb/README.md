# Plumb

Real-time pitch correction for voice and monophonic instruments, built for latency first: an original design, not a
recreation of any product. VST3, Audio Unit (macOS, a music effect with MIDI input), standalone app, and
`plumbrender`, a command-line renderer. Version 1.0.0.

The research behind it (what the note editors and the real-time correctors on the market do and what users ask of
them, pitch detection at low latency, pitch shifting with near-zero added delay, the correction logic, the visual
direction, the test plan) is in [`docs/research.md`](docs/research.md). The user manual is
[`docs/manual/en.md`](docs/manual/en.md) (and `es.md`), embedded in the plugin.

## How it works

Everything is in `Source/dsp/`, without JUCE, per sample:

1. **Detector** (`Detector.h`). A YIN-family difference function on a decimated copy of the input (12 kHz, or
   24 kHz for the High and Instrument ranges), every 8 samples. Acquisition searches every lag of the voice range;
   once locked, tracking searches ±25 % of the last period with an energy-normalised difference over one period, and
   re-acquires every 16 hops to catch octave errors. The period is refined at the full rate over two periods with
   parabolic interpolation. Octave and continuity guards, a three-estimate debounce for jumps of more than five
   semitones, and a voicing gate on the analysed window's energy (absolute, and 30 dB below the window's peak) and
   steadiness. Each estimate carries the input sample at the centre of its window.
2. **Correction** (`Correct.h`), at the detector's rate (~1.5 kHz). The pitch is split into a note centre, a slow
   drift and the vibrato with two critically damped smoothers that start fast at each note onset and slow down
   (so a scoop into the note does not drag the centre); a legato note change is detected when the pitch stays more
   than 0.75 semitones from the centre for 30 ms, or at once beyond 2.5 semitones. The target is the allowed note
   nearest to the centre, with hysteresis; in hard tuning it is chosen on the vibrato-free pitch, or on the raw
   pitch when the hysteresis is 0 (the warble effect). Flex window, humanize, transition, kept drift and vibrato,
   synthetic vibrato, MIDI scale (latched until the next chord), note lock with pitch bend, auto. A key finder
   (Krumhansl-Kessler profiles on a forgetting pitch-class histogram of held notes) feeds Listen.
3. **Source and filter** (`Lpc.h`). A 48th-order linear-prediction model every 128 samples on a pre-emphasised
   signal (pre-emphasis over the whole chain and an exact de-emphasis at the end), with a 60 Hz lag window, a
   1.0001 noise floor, 10 Hz bandwidth expansion and a 60 dB cap on the prediction gain. Analysis and synthesis are
   normalised lattices with the reflection coefficients interpolated sample by sample, so with no shift the output
   is the delayed input, exactly. Throat and Formant amount scale the angles of the model's poles (roots by Aberth,
   warm-started from the previous snapshot) and rebuild the filter; a level guard keeps the output under four times
   the aligned dry envelope.
4. **Shifter** (`Engine.h`). The excitation (the residual) is read by a head that moves at 2^(Δ/12) samples per
   sample. When the content delay nears its minimum the head jumps back one period (a cycle repeated); when it
   passes the minimum plus a period it jumps forward (a cycle dropped). Each jump is tuned ±4 samples by
   correlation, scheduled at a quiet point between glottal pulses and crossfaded over 24 samples at 48 kHz. With
   Preserve off the waveform itself is spliced. The three modes are the same chain with a different look-ahead:
   32, 128 or 1024 samples at 48 kHz (doubled at 96 kHz), switched without allocating.

The editor draws the slate (a Blender render, `tools/blender/plumb.py`), the plumb bob (a damped pendulum pushed by
the tuning error, half a degree per cent) and the live note view from the engine's history.

## Tests

`plumbtest` (`tools/plumbtest.cpp`): reported latency equal to the measured one in nine cases (three modes at 44.1,
48 and 96 kHz) with an identity error below −120 dB at Amount 0; detection on synthetic vowels from 82 to 880 Hz
with and without jitter (median error, p95, octave errors, lock time); hard tuning to within 2 cents; natural
correction that keeps the vibrato; note steps; content delay within bounds; formants with and without preservation
and with Throat against an ideal voice; key detection; random parameters with noise, silence, impulses and
clipping (no NaN, bounded); bit-identical output across block sizes; splices without clicks; no correction spikes
on large note jumps; hard tuning that does not flip on vibrato near a midpoint (and does with hysteresis 0); the MIDI
modes; natural correction of notes that start with a scoop; CPU. Plus pluginval at strictness 10, auval
(`aumf Plmb Atls`) and `athosttest`.

## Web and film

- `tools/blender/plumb.py`: the interface art (`--mode ui`), the photos and the film (Cycles; Metal on the Mac or
  CUDA on pc50-wsl under the GPU lock).
- `tools/web_images.py`: web images from the renders and the manual screenshots.
- `tools/mkfilm.sh`: the film, with the Natural Lead demo clip as its sound.
- `tools/demo_voice.py`: the synthetic singing voice used by the demos and the screenshots (no recorded voice).
- `tools/presets.py`: the 25 factory presets.
- Demo clips: `python3 scripts/samples.py --only plumb`.
