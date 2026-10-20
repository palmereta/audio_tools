# Lacquer

Automatic mastering: an original design, not a recreation. VST3, Audio Unit (macOS), standalone app with a
file mode, and `lacquerrender`, a command-line renderer. Version 1.0.2.

The research behind every decision (what Mixea, LANDR, Ozone and the others do, loudness standards, target
curves, the algorithms and their sources, the test plan) is in [`docs/research.md`](docs/research.md). The
user manual is [`docs/manual/en.md`](docs/manual/en.md) (and `es.md`), embedded in the plugin.

## How it works

1. **Analysis** (`Source/dsp/Analysis.h`). BS.1770-5 loudness, LRA, true peak, PLR and the minimum PSR; a
   mask of the loud parts (short-term loudness ≥ integrated − 3 LU); over that mask only: Welch spectrum of
   mid and side in 31 third-octave bands and in sixths of an octave, correlation overall and below 150 Hz,
   crest factor, onset rate, the most prominent resonance in 2-5 kHz, sibilance events, high-frequency
   cut-off and clipped runs.
2. **Plan** (`Source/dsp/EngineMaster.h`, `Learner::learn`). Staged: each module is decided on the signal as
   the previous modules left it, applied to a copy, and measured again for the next one.
   - Tone EQ (`ToneEq.h`): target curve of the style (`Targets.h`, parametric: tilt, high-pass, low bump,
     high roll-off) minus the measured spectrum, aligned by the median 100 Hz-8 kHz; smoothing, a dead zone
     inside the tolerance band (30 % correction), confidence weights, clamps per band and per slope, a boost
     energy cap; fitted with Gauss-Newton least squares to a fixed topology (low shelf 90 Hz, bells at 160,
     315, 630, 1250, 2500, 5000 Hz, high shelf 9 kHz). Loudness-neutral by measurement.
   - Dynamic EQ (`DynEq.h`): bands relative to the broadband level; thresholds from the percentile of that
     relative level over the loud mask. Harshness only if the resonance is ≥ 3 dB and the EQ is not boosting
     there.
   - Multiband (parallel form: `x + Σ (g − 1)·band`, untouched when idle) and glue (sidechain high-pass
     90 Hz): thresholds from level percentiles with a gain-reduction cap; makeup = measured average gain
     reduction. Their strength follows how much the target asks for: `need = PLR_in − (ceiling − target)`.
   - Stereo (`Stereo.h`): LR4 high-pass on the side only (mono lows), side gain toward a style target with a
     dead zone, never widening when the correlation is low.
   - Saturation (`Saturation.h`): tape or valve, 4x oversampled, input normalised to −14 LUFS so the curve
     does not depend on the mix level.
   - Loudness: gain → clipper (4x) → true-peak limiter (`Dynamics.h`), searched by secant / regula falsi on
     the real output until it reads the target within 0.05 LU. When the limiter alone would need more than
     8 dB, the clipper takes more (up to 6 dB on loud targets); past that, Lacquer stops and says so.
   - Already-mastered input (minimum PSR < 8 dB and loudness near the target): no compression, no clipper,
     half the EQ.
3. **Chain** (`Chain`): DC block/HPF → tone EQ → dynamic EQ → multiband → glue → stereo → saturation →
   gain → clipper → limiter. Per-sample, double precision, no threads: the output does not depend on the
   block size, and the renderer reproduces the plan bit for bit.

The plugin (`Source/PluginProcessor.*`) records what it hears during Learn (up to 6 min, skipping silence,
stopping when the music stops), plans on a worker thread and hands the new chain to the audio thread
lock-free with a 50 ms crossfade. Compare plays the delayed dry signal at the master's loudness; "Hear it
as" applies a platform's normalisation to both. File mode renders with the same code as `lacquerrender`.
Latency is constant (both oversampled regions always run): 275 samples at 48 kHz.

## Build, test, render

```bash
cmake --build build --target Lacquer_VST3 Lacquer_AU Lacquer_Standalone lacquerrender lacquertest lacquerlivetest
build/plugins/lacquer/lacquertest            # engine tests (T1-T15 of docs/research.md §6.10); --quick: meter, limiter, oversampling
build/plugins/lacquer/lacquerlivetest_artefacts/Release/lacquerlivetest mix.wav   # Learn, chain hand-over, A/B, state
build/plugins/lacquer/lacquerrender mix.wav master.wav --style warm --intensity 60 --target streaming --report master.json
build/plugins/lacquer/lacquerrender mix.wav --bench                                 # CPU of the chain
python3 plugins/lacquer/tools/demo_mix.py        # demo mix from our own instruments (build/lacquer-demo/)
python3 plugins/lacquer/tools/web_clips.py       # web clips, all at -16 LUFS with a fixed gain
python3 scripts/manual-shots.py lacquer          # manual screenshots (uses the demo mix)
```

Measured (Apple M4 / AMD Ryzen 9 5900XT, 48 kHz, 512-sample blocks, `lacquerrender --bench`): 2.9 % / 4.9 % of
one core with every module on (3.9 % / 5.8 % before the 8x peak detector got a contiguous, vectorisable
history). Tests: all EBU Tech 3341/3342 cases at 44.1, 48 and 96 kHz; limiter within 0.02 dB of the ceiling
on an independent 8x interpolator; loudness within 0.04 LU on every reachable target; bit-identical output
for block sizes 1, 64, 512, 4096 and random; aliasing of the saturation below −90 dBFS.

## Pitfalls

- **Phase rotation raises the peaks of an already limited master.** A 3-band LR4 crossover summed back
  (an all-pass) raised a −14 LUFS limited track from −5.2 to −0.3 dBTP; a 2nd-order 20 Hz high-pass, by
  1.6 dB. That is why the multiband is in parallel form, the mid is never filtered by the mono-maker and
  the high-pass only appears for real rumble. The symptom looked like a limiter bug (the output always
  touched the ceiling with almost no gain), but the limiter was fine.
- **A slow limiter release caps the loudness.** With "slow release when gain reduction > 2 dB", louder
  targets got stuck around −11 LUFS with 20 dB of reduction: the gain never recovered between kicks, so
  the limiter became a slow compressor and more input gain changed nothing. The release is now fast down
  to the average *needed* reduction (0.4 s) and slow below it. Averaging the applied reduction instead
  brings the problem back (it feeds on itself).
- **Learn on part of the song.** The level is set from what was heard; a song with quiet parts then
  measures 1-1.5 LU below the target. Learn therefore keeps listening while the music plays.
- **Synthetic test songs are not sample-rate invariant** (naive sawtooths alias differently): the
  sample-rate test synthesises at 96 kHz and resamples with a windowed sinc.
- **The clipper must not switch on and off during the loudness search.** Deciding it from the gain of each
  iteration made the output jump 0.35 LU and the search oscillate between two values (it ended 0.19 LU off).
  It is decided once per clipper level; its threshold stays continuous in the gain.
- **pc50: the generation studio (ComfyUI) does not take the GPU lock.** With one of its jobs running, Blender
  ran out of VRAM and stalled; `lathe.py --cpu` renders the film on the CPU instead (10 s per frame).
- `juce::String (double, 0)` prints every decimal (0 means "default"), not zero decimals.
- Blender: the bump of the groove must also go to **Coat Normal**, or the perfect clear coat hides it.

## Credits

Giannoulis, Massberg and Reiss (compressor design, JAES 2012); Pestana et al. (spectra of commercial
music); Ma, Reiss and Black (automatic EQ, AES 2013); Signalsmith Audio, "Designing a limiter" (moving
minimum + boxcars); the BS.1770 filter coefficients for any sample rate as derived by libebur128. No code
from any of them; only the published methods. Barlow Condensed by Jeremy Tribby (SIL OFL,
`assets/fonts/OFL-BarlowCondensed.txt`).
