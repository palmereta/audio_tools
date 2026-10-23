# AT330

Strings, choir and a ten-band vocoder with a four-BBD ensemble, recreated from the service documentation of a 1979
string, choir and vocoder keyboard. VST3 instrument, Audio Unit (macOS), standalone app and `at330render`, a
command-line renderer, on macOS, Windows and Linux (VST3, LV2). Version 1.0.0. Manual in English and Spanish
(`docs/manual/`, inside the plugin and on the website).

The research behind it (the original's service notes and owner's manual, the measured waveforms, filter frequencies
and LFO periods, the products that exist and what their users ask for, the design decisions and the test results) is
in [`docs/research.md`](docs/research.md). Values the documents do not give are marked there as inferred.

## How it works

Everything is in `Source/dsp/Engine330.h`, without JUCE, per sample:

1. **Divider.** Twelve top-octave tones from a 2.00024 MHz clock divided by integers (478 ... 253), every lower
   octave by halving: all the notes of a pitch class are phase-locked and each note is within 2 cents of equal
   temperament (A4 = 440 Hz; the original was tuned to 442, Tune covers ±50 cents). The waveforms are the ones on
   the service notes' adjustment sheet: 8' an exponential-charge sawtooth over the whole period, 4' flat for half the
   period and the same charge in the other half, both band-limited with PolyBLEP and PolyBLAMP, with the measured
   level per octave.
2. **Keys.** One gate per key with a common release (time to -40 dB). *Classic* reproduces the original's single
   envelope per section: every new key dumps it for 50 ms and attacks again. *Poly* (the default) gives each key its
   own attack per section. Velocity, sustain pedal, a movable split (C4).
3. **Strings.** The 4' wave through a fixed tone filter, the section envelope and the TONE low-pass.
4. **Choir.** Male 8', male 4' and female 4' voices through one shared bank of seven band-passes (200, 230, 600, 900,
   1300, 2800 and 3300 Hz). Which filters each voice uses is inferred (`kChoirMix`).
5. **Vocoder.** Ten analysis and ten synthesis bands (200, 280, 400, 600, 900, 1300, 2000, 2800, 4000, 6000 Hz, each
   two cascaded band-passes), pre- and de-emphasis, peak followers (Vocoder Response; Hold = 2.5 s; Freeze), a
   consonant path (three high-passes at 4.5 kHz) open only while there is a carrier, direct mic, formant shift. The
   carrier is the 8' of the keys, an external signal on the right input channel, or both.
6. **Vibrato and ensemble.** The choir and the vocoder go through a modulated BBD line (4.5-11 Hz, with a delay
   before it starts). The ensemble is four BBD lines around 5.12 ms, two per output, each pair modulated in opposite
   directions by a slow triangle plus a fast sine with the measured periods (3.9 s / 150 ms and 4.6 s / 175 ms), with
   the BBD filters, a soft saturation and a very low noise floor.
7. **Rest state.** With no keys and nothing into the BBDs for about 21 ms, the divider and the LFOs return to fixed
   phases and stay there: a render after a silence gives the same bits whatever played before.

Latency 0. Plate reverb (Space) after the ensemble. 44 automatable parameters (`Source/Params330.h`).

## Editor

`Source/PluginEditor.cpp`: 1600 x 960, resizable with a fixed aspect. The instrument is rendered in Blender
(`tools/blender/at330.py --mode ui`, own design, no marks): `assets/ui/bg.jpg` (tablets off), `on.jpg` (all on),
`keysdown.jpg`, the knob and lever sprites and `layout.json` with where everything is. The interface draws on top
what changes and the silkscreen as vector text; the static layer is cached at 2x and only the dirty part is
redrawn. Pages: Panel, Extended (the extras) and Manual. The walnut uses a CC0 texture from Poly Haven
(`tools/blender/fetch_textures.sh`, not in the repository); without it the scene falls back to a procedural walnut.

Headless screenshots: `AT330_SNAPSHOT=/path.png AT330_PRESET=n AT330_PAGE=panel|extended|manual AT330_KEYS=45,52,57
AT330_WAIT=2` on the standalone app (used by `scripts/manual-shots.py`).

## Tools

| Tool | What it does |
|---|---|
| `at330test` | Numbered tests T1-T16: tuning, waveforms and aliasing, envelopes, formants, vocoder bands, Hold and Freeze, consonant gate, ensemble LFO periods, vibrato, pitch shift, determinism with any block size, robustness, CPU, host gap, program 0 and state bytes, host reset, smoothing, preset levels |
| `at330render out.wav --preset "Name" [--midi f.mid \| --chords "Am F C G"] [--voice v.wav] [--carrier c.wav]` | The full processor to WAV; `--bench` measures every preset, `--list` lists them |
| `tools/presets.py` | Writes the 45 factory presets (`assets/presets/`) |
| `tools/headroom.py <at330render> [--apply]` | Sets each preset's volume (RMS toward -16 dBFS, peak at most -1.5 dBFS) in `tools/preset_levels.json` |
| `tools/demo_voice.py` | A synthetic speaking voice with unvoiced consonants, for the vocoder demos (no recorded voice) |
| `tools/demos.py OUTDIR` | The demo clips (-16 LUFS) and two sketches with Pallor, Thrum and Orrery |
| `tools/blender/at330.py` | `--mode ui` (editor layers), `--mode photos` (web photos), `--mode film2` (web film) |
| `tools/web_images.py`, `tools/web_samples.py` | Website images and listening clips |

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target AT330_VST3 AT330_AU AT330_Standalone at330test at330render
build/plugins/at330/at330test_artefacts/Release/at330test
```

## Pitfalls

- **The 50 ms dump of the original is real and sounds like a gap.** The service notes' adjustment sheet shows the
  shared envelope held at zero for 50 ms after every key-on. Reproduced in Classic; the factory presets use Poly
  except two that show the original behaviour.
- **Free-running divider and LFOs made renders depend on history.** Two bounces of the same project, or a saved
  session reopened, started the chorus and the waveforms at other phases (athosttest's state check gave a
  correlation of 0.08). The rest state fixes it; it has to include the Classic envelopes too.
- **The vocoder presets are silent without a voice.** Their names end in "(Audio In)" (the AT20V convention);
  athosttest reports them as WARN "no sound with a chord", which is expected.
- **The AU "fresh instance == program 0" state check** fails for the AU only, and also for the released AT303 AU:
  it comes from the AU wrapper's preset metadata, not from the plugin's state (the VST3 passes byte for byte).
