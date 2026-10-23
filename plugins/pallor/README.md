# Pallor

A drum machine for dark wave, coldwave, post-punk, minimal wave, EBM, synthpop, goth rock, industrial, new beat,
italo/hi-NRG, witch house, 90s darkwave/trip-hop and dark ambient: an original design, not a recreation. Sixteen
voices with an analog-circuit engine and an 80s digital playback engine each, the studio effects of the period
and a step sequencer. VST3 instrument, Audio Unit (macOS), standalone app, and `pallorrender`, a command-line
renderer, on macOS, Windows and Linux (VST3, LV2). Version 1.0.2. Manual in English and Spanish (`docs/manual/`,
inside the plugin and on the website).

The product name lives only in `PALLOR_NAME` in `CMakeLists.txt` (product, plugin name, bundle id); parameter ids,
presets and saved states do not depend on it. The working name in phase 1 was Vesper.

The research behind it (the genres and their machines, the 808/909/CR-78 circuits from the service notes and
Werner's papers, the 8-bit companding DAC, clock tuning, the samplers, swing, the 80s studio effects, what users of
drum plugins ask for, the test plan) is in [`docs/research.md`](docs/research.md).

## How it works

Everything is in `Source/dsp/`, without JUCE, per sample:

1. **Analog engine** (`Analog.h`). Twelve families of circuit models, 44 models in all, each with two character
   controls whose meaning depends on the model:
   - Bridged-T resonators (kick, toms, congas, rim, clave) as modal resonators (a decaying complex phasor) struck by
     the 1 ms trigger pulse, with the pulse leaking to the output (the click). The kick settles at 49.5 Hz, starts
     about 15 % higher and glides down over 0.2-0.3 s, and resonates near twice its frequency for the first 4 ms
     when accented; the 808-style toms are higher while loud.
   - VCO kick, snare and toms with two-segment pitch envelopes (the kick: ≈ 270 Hz falling to 49.5 Hz, τ 3 and 31 ms).
   - The six-square metal source (205.3, 304.4, 369.6, 522.7, 540 and 800 Hz, PolyBLEP) through band-passes at
     3.44 and 7.1 kHz and a resonant 10.5 kHz high-pass; the two-square cowbell (540 + 800 Hz) with its two-slope
     envelope; the cheap-box noise hats; "Twin Tank" (noise in a 9.1 kHz tank plus three squares at 4,080, 5,620 and
     6,170 Hz in a 5.6 kHz tank).
   - The two-resonator snare (238 + 476 Hz, snappy noise through a 2.75 kHz high-pass), the clap (noise in a
     1.07 kHz band-pass, three sawtooth bursts 11 ms apart and the reverb tail), the 455 + 1,667 Hz rim.
   - Hexagonal electronic drums: oscillator with a steep bend (the "pew"), noise and click; and two voices of our
     time (a dark long-sweep kick, a crack snare), FM metals, noise sweeps, static, sub drops.
   Each model is calibrated (`modelGain`, measured by `pallortest --calibrate`) so a kit is balanced with every
   level at 0 dB.
2. **Digital engine** (`Digital.h`). A sample played through the vintage chain: the 8-bit companding DAC with the
   exact DAC-76/AM6070 law (sign, chord, step; 255 levels, 72 dB), pitch by changing the playback clock with no
   interpolation (pitching down brings the DAC images into the band), four-pole output filters at 10 kHz (kick,
   toms) or 16 kHz and the dynamic low-pass of kick and toms that opens on each hit; 12-bit at a fixed 26.04 kHz
   with drop-sample pitch and no filter; 12-bit at 40 kHz; clean. The zero-order hold is drawn with band-limited
   steps at each tick's exact time, at twice the host rate, then decimated with the shared half-band: the images
   are there, the host grid adds no aliasing (test T16). The decay VCA comes after the DAC, so the companding noise
   falls with the sound. The free-running clock lands each hit at another phase.
3. **Kits** (`KitGen.h`). Eight built-in kits synthesised at load (≈ 0.4 s, 11 MB, shared by every instance): no
   sample of any machine. Membranes with Bessel modes and tension glide, snare wires that follow the head, metal
   as many inharmonic modes plus banded noise, bars, struck sheets and barrels, two-operator FM, Pallor's own analog
   voices "sampled", and a room with a gate. Each sound is generated at twice its kit rate, decimated (the sampler's
   anti-alias filter), and matched to the early loudness of the analog voice of its slot.
4. **Voice** (`Voice.h`). Two lanes per slot (a retrigger fades the old hit in 2 ms; a choke in 4 ms), then crush
   (bits and sample-and-hold rate), drive, SVF filter, level. Analog lanes are delayed 24 samples so they line up
   with the digital lanes (the decimator's 23.5 + the step's 0.5): the plugin reports 24 samples of latency
   (78 with the master tape on).
5. **Sequencer** (`Seq.h`). 16 pattern groups × A, B, Fill; 64 steps; per-track length (polymeter); per step
   velocity, accent, probability (a hash of the position: deterministic, varies bar to bar), ratchet 1-8, flam (the
   grace note before the beat), nudge ±50 %, conditions (fill only, not fill, 1:2, 2:2, 1:4, 4:4, first, not first).
   Swing 50-75 % on 8ths or 16ths as a warp of each pair (the second note at swing × pair). Play modes A, B, A>B
   and Song (a chain of entries with repeats and a fill at the end). Which segment plays at a position is a pure
   function of the position and the settings, so after a host jump it plays what it would have played; while
   playing, the next segment is decided half a beat early (pattern changes land on the next bar; flams and negative
   nudges on the first step can sound before the bar). Consecutive host blocks are joined at the last window's end,
   so no hit is lost or doubled in the rounding gap (test T1, 50 tempo/block/gap combinations).
6. **Effects** (`Effects.h`). Send A, Space: gated (Halation's network as a chamber, squashed, gated by the send's
   onsets), nonlinear and reverse (velvet-noise taps over the gate time with a flat, falling or rising envelope),
   plate (the shared Dattorro plate), spring (AT700's), hall and room (Halation's network with 12 kHz bandwidth and
   12-bit wet). Send B, Echo: 12-bit digital delay, tape echo (three heads, wow, flutter, drift, Tarnish's
   Jiles-Atherton hysteresis in the loop), BBD delay (the filter follows the delay time), BBD chorus, three-line
   ensemble, jet flanger, six-stage phaser; the modulation types return wet only, so with the dry signal they make
   the comb or the notches. Master: lo-fi (Tarnish's crusher at 12-bit 26.04/40 kHz, or 8-bit companded at
   22/11 kHz), tape (Tarnish's hysteresis at 4x with Lacquer's oversampler, drive-compensated, DC-blocked), glue
   (Lacquer's compressor with console-bus attack/release steps and a dual-release Auto), exciter, volume, soft
   ceiling.
7. **Engine** (`Engine.h`). Per block: sequencer hits and MIDI notes become per-slot events (plus chokes for the
   group), each slot renders between its events, its mono output is panned into its bus (main or one of six aux:
   Kick, Snare/Clap, Hats/Cymbals, Toms, Perc, FX; a bus the host did not enable folds into main) and the sends.
   MIDI in with the GM drum map (36 kick, 38 snare, 39 clap, 37 rim, 42/46 hats, 49 crash, 51 ride, 41/45/48/50
   toms, 56 cowbell, 75 clave, 54 tambourine, 52 FX, with aliases) or chromatic 36-51; MIDI out of what the
   sequencer plays with the same map, note-off after 30 ms or before the next hit of the same note, all flushed on
   stop, jump and bypass. The MIDI leaves delayed by the latency the host knows (`shared/tools/MidiOutDelay.h`): the
   host compensates the audio, not the MIDI out, and in Live the notes were 78 samples (1.6 ms) early (DAW QA
   2026-10-22, P1). athosttest `midisync`: first note at 78, audio from 78 (1.0.0: note at 0). In REAPER bypass
   (REAPER does not call a bypassed plugin) nothing hangs: voices are one-shots and MIDI-out note-offs are timed
   in processed samples, so they still go out when processing resumes.

`PluginProcessor` keeps the pattern bank and the song chain (the editor edits them on the message thread; the audio
thread copies only the pattern that changed, without blocking), user samples (loaded on the message thread,
published by an atomic pointer, freed two blocks later; embedded in the state as 16-bit FLAC in base64 up to 8 MB in
total, otherwise only the path), the factory presets and the pattern library, `.mid` export and the multi-out buses.
`PatternIO.h` is the text format of patterns (presets, library and state).

## The editor

The panel is an original industrial design rendered in Blender (`tools/blender/pallor.py`, mode `ui`): a black
anodised slab, a dark glass screen in a brushed bezel, a bone-ceramic pad trough with sixteen pale silicone pads lit
from inside in rose, knurled knobs with ceramic tops, rubber keys with LED lenses. The render gives `bg.jpg` (all
off, no silkscreen), `lit.jpg` (pads and lenses lit), `down.jpg` (pads and keys pressed), the knob caps and
`layout.json` (positions and the silkscreen, which the editor draws as vector text). The editor (`Source/ui/`):

- `Hardware.h`: the static layer (render + silkscreen, cached with `setBufferedToImage`) and the overlay: knob caps
  with their index and value arc (the eight big knobs follow the selected voice; the character labels follow its
  model), pad hit glow (crops of `lit.jpg` clipped to each pad, opacity from the hit), pressed keys and pads, key
  LEDs, the header display. Pads play the voice (velocity by height), take dropped files and samples dragged from
  SAMPLES.
- `PatternPage.h`: the bank (16 × A/B/Fill), length, step, swing per pattern, library by genre, copy/paste, the
  16/32/64-step grid (paint by dragging, modifiers, a menu per step and per track: track length, shift, copy) and
  the lanes (velocity, probability, ratchet, condition, flam, nudge, accent), and DRAG MIDI (the pattern as a `.mid`
  in the temp folder for an external drag, or saved on click).
- `Pages.h`: VOICE (engine, model or kit and chain, every control, the hit drawn from the current settings — the
  analog voice is rendered offline at 24 kHz, the digital one is its sample — and the drop zone), MIXER (strips
  with mute, solo, pan, sends, fader, meter and bus), FX, SONG (the chain as tiles: add, drag to move, repeats,
  fill), SAMPLES (file browser; a click loads into the selected voice and plays it), PRESETS (by genre; sound
  and/or patterns; user presets with patterns, song and sample paths, `userPresetDir()`), SETUP.
- `ThemePl.h` (colours, Barlow Condensed, screen controls bound with `juce::ParameterAttachment`) and `Ctx.h`
  (selected voice, edit target, levels read once per frame).

CPU of the editor on the M4 (window at 78 %): about 1 % of a core idle, 10-13 % with the sequencer running; most of
that is Core Animation compositing the dirty rectangles, which spread from the screen to the pads.

Snapshot variables for the manual and the web: `PALLOR_SNAPSHOT`, `PALLOR_PRESET`, `PALLOR_PAGE`, `PALLOR_VOICE`,
`PALLOR_LANE`, `PALLOR_RUN`, `PALLOR_WAIT`, `PALLOR_SCALE`, `PALLOR_SCREEN` (only the screen),
`PALLOR_SAMPLES_DIR`, `PALLOR_MANUAL`.

## Tests

`pallortest` (no JUCE), 21 groups, all pass on macOS, Windows and Linux:

- T1 timing: an impulse sample on a pattern at 5 tempos × 5 block sizes (1, 64, 441, 512, random) × with and without
  a host rounding gap: every hit at its exact sample + 24, none lost or doubled.
- T2 determinism: a 6 s render with every effect, bit-identical at blocks of 1, 64, 512, 4096 and random.
- T3 swing (exact positions at 50-75 %, 8000/4000 samples at 66.7 % and 120 BPM); T4 choke (> 40 dB in 4 ms; both
  sound when struck together); T5 ratchet ×4 and a 20 ms flam to the sample; T6 probability (block-independent,
  50 % → 50.2 %); T7 conditions, fills and auto fill; T8 polymeter across bars; T9 song chain and a host jump into
  it; T10 MIDI out balanced through jumps and stop; T11 note maps and a MIDI note to the sample; T12 multi-out
  routing, folding and alignment with the tape latency.
- T13 user samples (clean pitch 441.00 Hz, reverse, octave, 8-bit companded 43.6 dB signal-to-rest).
- T14 voices against the published analyses: bridged-T kick 49.5 Hz settled, 54.5 Hz early, −20 dB at 285 ms
  (service notes: 300 ms); VCO kick 152 / 95 / 49.5 Hz at 20 / 40 ms / settled (published fit 166 / 111 / 49.5);
  snare 238 + 476 Hz; rim 455 + 1,666 Hz; clave 2,500 Hz; cowbell harmonics of 540 and 800 Hz; 12 of the 12
  strongest open-hat lines are odd harmonics of the six oscillators; clap bursts at 0 / 11 / 22 / 33 ms; the
  CR-78-style metal at 5,620 and 6,170 Hz; the hexagonal tom bending from 329 Hz to 115 Hz.
- T15 the DAC-76 law: 255 levels, 72 dB; SNR 39.5 / 37.1 / 34.7 dB at 0 / −20 / −40 dBFS (as calculated in the
  research). T16 clock pitch: the 12-bit 26 kHz image at 17.04 kHz is there (−12.9 dB), the host-grid fold at
  12.96 kHz is not (−98.9 dB); pitching an 8-bit sample down brings its image into the band.
- T17 gated room (−18.4 dB during the gate, −98.5 dB after) and a flat 300 ms nonlinear program with a sharp cut;
  T18 30 random extreme configurations (no NaN, peak ≤ 1, DC < 0.002) and 21 s of silence (no denormal slowdown);
  T19 the kits; T20 CPU.
- T21 swing per pattern (a 66 % pattern under a 50 % global: 7920/4080 samples at 120 BPM), MIDI out gate (120 ms:
  every note-off 5760 samples after its note-on), solo (a soloed snare silences the kick, its own level unchanged).

`pallorrender --state-test` saves and restores three presets (one with an embedded user sample) into a new
instance and compares the sound (3/3 on the three systems).

Host tests of the release builds (2026-10-21): macOS universal — athosttest 138 PASS, 0 WARN, 0 FAIL; pluginval
strictness 10 VST3 and AU SUCCESS; auval SUCCEEDED. Windows (MSVC, pc50) — athosttest 138/0/0, pluginval VST3
SUCCESS. Linux (Ubuntu 22.04 container) — athosttest 138/0/0, pluginval VST3 and LV2 SUCCESS, standalone opens and
closes cleanly under xvfb.

## CPU

One core at 48 kHz, blocks of 512, sound engine only:

| | Apple M4 | Ryzen 9 5900XT |
|---|---|---|
| Night Shift Mercury (`pallorrender --bench`) | 3.84 % | 5.67 % |
| Average of the 65 presets | 2.25 % | 3.78 % |
| Ash Choir Pulse, the heaviest preset | 5.37 % | 7.18 % |
| `pallortest` T20: kick, snare and hat, no effects | 0.83 % | 1.97 % |
| T20: 16 analog voices, gated room, tape echo, full master | 5.00 % | 7.42 % |
| T20: 16 digital voices, same effects | 7.06 % | 9.93 % |
| T20: 16 voices, 800 ms nonlinear room, ensemble, lo-fi, tape | 6.52 % | 10.29 % |

The master tape alone is about 2.5 % (M4). The kits take ≈ 0.4 s to generate once per process. The editor adds about
1 % idle and 10-13 % while playing (M4).

## Presets and patterns

`tools/presets.py` writes 65 presets in 13 genres (kit, effects, two pattern sets and often a song chain) and the
library: 52 original pattern sets (A, B, Fill), 156 patterns, in a one-character-per-step notation with ratchets,
flams, probability and fill conditions. `tools/level_presets.py` renders every preset and trims its master volume
to −18 LUFS (all land within −20.7 to −17.9). `tools/demos.py` renders the listening clips (one per genre, three
tours, three sketches with ATMD, ATFS1, Halation, Tarnish and Lacquer).

## Pitfalls

- A parameter-pointer cache kept in a `thread_local` map keyed by the processor's address read freed memory when a
  new instance reused the address (athosttest state and MIDI failures). It is now a member.
- Hysteresis remanence leaves DC after loud hits: the master tape has a 5 Hz high-pass; the tape echo compensates
  the drive gain or its loop goes over 1 and howls.
- Peak-normalised acoustic samples are 8 dB quieter than the analog voices: kit samples are matched by early
  loudness. Hats sat 25-30 dB under the kick before the slot peaks were raised; measure stems
  (`pallorrender --stems`), not the mix.
- `setPlayConfigDetails` collapses the aux buses into one 14-channel main bus: the renderer only sets the rate.
- The slot tail must be counted per sample, not per block, or the output differs with the block size.
- athosttest left its transport playing after a "sequence preset" render (a stop at exactly the end was never
  applied): fixed in `tools/hosttest`.
- A pattern bank is ~400 KB: on the stack it overflowed Windows' 1 MB stack in the tools (pallortest ended with
  0xC00000FD) and risked macOS's 512 KB secondary-thread stacks in the plugin. The plugin copies banks on the heap;
  the tools link with a 32 MB stack on MSVC.
- AgX desaturates bright emission: the pads' lit render uses a dark base and a moderate emission so the glow stays
  rose instead of turning pastel.

## Renders, film and web

- UI: `blender -b --factory-startup -P tools/blender/pallor.py -- --out <dir> --samples 384` (pc50-wsl, under the GPU
  lock); copy `bg.jpg lit.jpg down.jpg cap_big.png cap_small.png layout.json` to `assets/ui/`.
- Photos: `--mode photos --screen <screen.png> --display <display.png>` with the screen and the display cut from an
  editor snapshot at 2x (`PALLOR_SNAPSHOT`, `PALLOR_SCALE=2`; crops from `layout.json`), then
  `tools/web_images.py <photos>` writes `web/assets/img/pallor/`.
- Film: `--mode film2 --quality final --out ~/film/out/pallor` (six shots, 600 frames at 24 fps, 120 BPM; the pads
  follow Night Shift Mercury, the snare's Decay knob follows the automation in `tools/film.json`, written by
  `python3 tools/blender/pallor.py --mode filmjson`), then `python3 scripts/film.py pallor <frames>`.
- Listening clips: `scripts/samples.py --only pallor` (it runs `tools/demos.py`, −16 LUFS).

