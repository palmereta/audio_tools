# ATFS1

Formant and FM synthesizer based on the Yamaha FS1R (1998): 8 voiced and 8 unvoiced (noise-formant)
operators per voice, the 88 algorithms, Formant Sequences, a per-voice filter and insertion,
variation and reverb effects, with a full-screen editor under a working 1U front panel.
VST3, Audio Unit (macOS) and standalone. Version 1.0.0.

What the engine implements, and where each law comes from: [`docs/hardware-spec.md`](docs/hardware-spec.md).
Sources and licences: [`docs/ref/CREDITS.md`](docs/ref/CREDITS.md).

## Layout

```
Source/dsp/FsData.h          voice data with the FS1R ranges (common + 8 voiced + 8 unvoiced operators)
Source/dsp/FsAlgorithms.h    the 88 algorithm topologies (modulators per operator, carriers, feedback)
Source/dsp/FsDsp.h           tables, EG laws, voiced operator (8 spectral forms), unvoiced operator, filter
Source/dsp/Fseq.h            Formant Sequence player, vowel generator, WAV analysis (pitch + LPC formants)
Source/dsp/FsEngine.h        voices, algorithm routing, pitch EG, LFOs, filter EG, part offsets, Fseq
Source/dsp/FsEffects.h       insertion, variation, reverb, EQ, limiter
Source/dsp/Syx.h             FS1R sysex: voice (608 bytes) and Fseq bulks, read and write
Source/SynthFs.h             engine + effects + MIDI controllers (shared by the plugin and atfs1render)
Source/ParamsFs.h            host parameters (part, Fseq playback, effects, system)
Source/PresetFs.h            JSON for voices, Fseqs (base64 of the FS1R bulk) and presets
Source/ui/                   rack front (RackView), pages, graphs, widgets, keyboard
tools/                       panel, Blender scene, asset packer, presets, Fseq library, levels, tests, renderer
assets/ui                    rendered front panel and sprites (generated, embedded)
assets/presets, assets/fseq  factory presets and Formant Sequences (generated, embedded)
```

## Tools

```bash
# DSP checks without JUCE: spectral forms, EG law, FM index, noise band, Fseq analysis, sysex, effects
clang++ -std=c++17 -O2 -I shared/dsp -I plugins/atfs1/Source/dsp plugins/atfs1/tools/dsptest.cpp -o /tmp/atfs1_dsptest && /tmp/atfs1_dsptest

# Offline render (CPU figure printed), Fseq generation and analysis
build/plugins/atfs1/atfs1render_artefacts/Release/atfs1render preset.json out.wav --notes "48+55+60:4"
atfs1render --vowels out.json "Name" "aeiou" --pitch 130 [--female] [--secs-per 0.4]
atfs1render --analyze speech.wav out.json "Name"

# Factory content (order matters: presets read the Fseq names, levels read the presets)
python3 plugins/atfs1/tools/fseqlib.py --tts <folder with the espeak-ng WAVs>
python3 plugins/atfs1/tools/presetsfs1.py
python3 plugins/atfs1/tools/level_presets.py build/plugins/atfs1/atfs1render_artefacts/Release/atfs1render

# Front panel: layout + silkscreen, Blender (pc50-wsl, CUDA), packing
python3 plugins/atfs1/tools/panelfs1.py
blender -b -P tools/blender/scenefs1.py -- --layout layoutfs1.json --silk silkscreenfs1.png --out out --mode bg,sprites --samples 160
python3 plugins/atfs1/tools/packfs1.py
```

The editor can save a snapshot without a screen grab:
`ATFS1_SNAPSHOT=/tmp/x.png ATFS1_PAGE=0..7 ATFS1_PRESET=n ATFS1_OP=1..8 ATFS1_UNVOICED=1 ATFS1_KEYS=1 ATFS1.app/Contents/MacOS/ATFS1`.

## Measurements (dsptest and atfs1render)

| Check | Result |
|---|---|
| all1 at skirt 0 | one partial, the next one below −100 dB |
| odd forms at skirt 0 | partials 1 and 3 only |
| res1, resonance 4 | lines on harmonics 4, 5, 6; sides at −6.0 dB; harmonic 2 below −130 dB |
| group width at skirt 7 (one side) | all1 1.23, all2 3.35 partials (FSVR, both sides: 1.34 and 6.5) |
| EG traverse | 6.1 ms (T 0) to 350 s (T 99); decay linear in dB |
| WAV → Fseq | synthetic /a/ at 120 Hz: pitch 120 Hz, F1/F2/F3 731/1102/2454 Hz (targets 730/1090/2440) |
| speech resynthesis | F1 and F2 tracks correlate 0.82 and 0.74 with the analyzed phrase |
| sysex | voice 619 bytes with checksum, every field round-trips; Fseq frequencies within 1.7 cents |
| CPU, Apple M4 / Ryzen 9 5900XT | speech 0.4 / 0.7 %, 16-voice choir 6.8 / 13.9 %, 32 voices 8.7 / 21.9 % of a core |

## Pitfalls

- **Delay lines.** `DelayLine::tap` read one sample past the buffer when `w − d` was a tiny negative
  number: adding the length rounds to exactly N in float. It showed up as a single full-scale click
  in a chorus after minutes of audio, and only with some builds (the FMA contraction changed the
  rounding), so adding debug prints made it disappear. Fixed in `shared/dsp/Fx.h` for every plugin.
- **Carrier phase of the grain forms.** With a sine carrier starting at 0 the window peak meets a
  zero crossing, so at high skirt the group moves away from the fundamental. The all and res forms
  start the carrier at a quarter cycle (crest at the window centre); the odd forms alternate 0 and ½.
- **Formant analysis.** Peak-picking the LPC envelope merged F1 and F2 and pre-emphasis inflated the
  high formants; the roots of the LPC polynomial (Durand-Kerner) with the de-emphasis correction fixed
  both.
- **Windows speed.** Per-sample coefficient work (noise one-poles, filter divisions, `isfinite`) moved
  to the control rate, silent operators are skipped, and the window tables were halved to stay in a
  32 KB L1 cache. On the M4 that took the 32-voice case from 26 % to 8.7 % of a core; on the Ryzen the
  last two changes took it from 27.6 % to 21.9 %.
- **Editor CPU.** Big LCD panels repaint as a cached background layer plus a small live layer
  (meters, playhead, curve); repainting the gradients at 20-30 Hz took the standalone to about a third
  of a core.
