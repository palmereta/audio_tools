# Audio Tools

> This repository is published from the development repository with `scripts/publish-github.py`: each commit
> here names the development commit it comes from. The website folder and the reference photographs of the
> original instruments (`plugins/*/docs/ref/`, credited in their `CREDITS.md`) are not included.

Free, open-source recreations of legendary synthesizers, as VST3 / Audio Unit / LV2 / standalone
plugins for macOS, Windows and Linux, plus six tools of our own: Orrery, a MIDI generator that plays them,
Murmur, a granular effect, Halation, a delay and reverb in one space, Tarnish, a tape and lo-fi machine,
Plumb, real-time pitch correction with 0.67 ms of latency, and Lacquer, an automatic mastering processor.

Each instrument is rebuilt from its original documentation and measurements: service manuals,
circuit analyses and the factory settings charts. The aim is a model that anyone can study, verify
against the hardware and improve.

| Plugin | Based on | Version | Status |
|---|---|---|---|
| [AT20V](plugins/at20v/) | Korg MS-20 (1978), semi-modular monophonic synthesizer | 1.0.7 | Released |
| [AT101V](plugins/at101v/) | Roland SH-101 (1982), monophonic synthesizer with arpeggiator and sequencer | 1.0.7 | Released |
| [AT42](plugins/at42/) | Elta Music SOLAR 42 (2023), ambient drone machine | 1.0.8 | Released |
| [AT700](plugins/at700/) | Roland System-700 (1976), modular synthesizer with analog sequencer | 1.0.5 | Released |
| [ATFS1](plugins/atfs1/) | Yamaha FS1R (1998), formant and FM synthesizer | 1.0.6 | Released |
| [ATMD](plugins/atmd/) | Moog Minimoog Model D (1970), monophonic synthesizer with the transistor ladder filter | 1.0.3 | Released |
| [AT303](plugins/at303/) | A classic 1981 bass line synthesizer with sequencer: diode ladder filter, accent and slide | 1.0.3 | Released |
| [Orrery](plugins/orrery/) | Original: generative MIDI for ambient, dark ambient and drone, eight lanes on their own channels | 1.0.7 | Released |
| [Murmur](plugins/murmur/) | Original: real-time granular effect, up to 128 grains over a 16-second memory | 1.0.4 | Released |
| [Lacquer](plugins/lacquer/) | Original: automatic mastering, live (Learn) or from a file, with a command-line renderer | 1.0.3 | Released |
| [Halation](plugins/halation/) | Original: delay and reverb in one space, with freeze, shimmer, reverse, grains and more | 1.0.3 | Released |
| [Tarnish](plugins/tarnish/) | Original: tape, cassette, video, vinyl, AM radio and telephone, with magnetic hysteresis | 1.0.3 | Released |
| [Plumb](plugins/plumb/) | Original: real-time pitch correction for voice, natural to hard, from 0.67 ms of latency | 1.0.2 | Released |
| [Thrum](plugins/thrum/) | Original: electric guitar by physical modelling: six guitars with waveguide strings and real pickups, a guitarist that fingers, strums and plays the articulations, thirteen pedals, four amps and a cabinet with movable microphones | 1.0.1 | Released |
| [Pallor](plugins/pallor/) | Original: drum machine for dark wave, coldwave, post-punk and EBM, analog and 80s digital engines, 80s/90s effects, step sequencer | 1.0.2 | Released |
| [AT330](plugins/at330/) | Roland VP-330 (1979), strings, choir and 10-band vocoder with a BBD ensemble | 1.0.0 | Released |

## AT20V

- Korg35 filters solved implicitly (Newton per sample), with the hardware's real slopes: 12 dB/oct
  low-pass and 6 dB/oct high-pass. Factory calibration: cutoff 5 with peak at 10 self-oscillates
  at 500 Hz.
- The full 35-jack patch bay, including the normalled connections, and cables with rope physics.
- External Signal Processor with pitch tracking.
- Extensions the MS-20 never had:
  - a vocoder with 8 to 20 bands;
  - up to 12 voices with unison;
  - a tempo-synced modulation generator;
  - MIDI out, in Thru and Patch modes;
  - delay and plate reverb.
- **AT20V FX**: an effect version, so you can vocode an audio track.
- 51 presets in five categories: factory charts, classics, self-playing, ambient and vocoder.
- Photorealistic interface rendered in Blender from measurements of a public-domain photograph.

Technical notes, sources and pitfalls: [`plugins/at20v/README.md`](plugins/at20v/README.md) and
[`plugins/at20v/docs/hardware-spec.md`](plugins/at20v/docs/hardware-spec.md).

## AT101V

- IR3109 four-pole filter with the feedback solved in closed form (no one-sample delay), diode-limited
  resonance that self-oscillates as a sine tracking 1 V/oct, and output-side bass compensation.
- CEM3340-style saw and pulse from one core, PWM by LFO, envelope or hand, and the sub-oscillator
  as phase-locked flip-flops (−1 oct, −2 oct, 25 % pulse).
- RC envelope with the overshooting attack, the 101 keyboard logic (priority set by the trigger
  switch, Auto portamento only when legato), the modulator as clock, arpeggiator and the 100-step
  sequencer loaded from the keyboard (REST and LEGATO), as on the original.
- Extensions the SH-101 never had:
  - a visual step editor with accent, slide, tie and probability per step, random and mutate,
    rotate, transpose and `.mid` export;
  - up to 8 voices with unison, host tempo sync with swing and gate length, arpeggio octaves;
  - velocity, drive, BBD-style chorus, delay and plate reverb;
  - MIDI learn, MIDI out of every note it plays, user presets with the sequence and a random patch;
  - three body colours (grey, blue, red).
- 44 presets: basses, leads, self-playing sequences, arpeggios, ambient, poly and FX.

Technical notes: [`plugins/at101v/README.md`](plugins/at101v/README.md) and
[`plugins/at101v/docs/hardware-spec.md`](plugins/at101v/docs/hardware-spec.md).

## AT42

- Four drone voices of five negistor-style sawtooth oscillators (the original tune ranges and pot
  curve), VOLT with supply coupling (cross-modulation past half travel) and light-dependent pitch
  from photo orbs (ambient light, the hand: hover, press or drag the mouse or a finger, or automate the Drone N Hand
  parameters, and an internal LED driven by CV).
- Two Papa Srapa noise voices (square modulator, tone oscillator, FM, AM, noise, sample and hold),
  two AS3340-style VCOs with six waveforms, linear FM and sync, ADSR envelopes with hold and loop.
- 10-channel pan mixer, dual 12 dB Polivoks filter (LP/BP) with stereo distortion, and a dual
  cartridge effector: 13 cartridges x 3 programs.
- LFO A/B, joystick, 5-step sequencer with pulser, preamp and envelope follower, touch keyboard with
  19 scales, arpeggiator and 16-step sequencer, six drone keys, and a 62-jack patch bay with the
  hardware's normals.
- Extensions: MIDI (notes, drone keys, bend, sustain pedal, mod wheel, learn), host sync, chromatic drone tuning,
  30 s releases, stereo width, presets with cables, random patch, four panel colours.
- 28 presets plus Init.

## AT700

- The Main Console of the System-700 with its 17 modules and factory buses: three 702 VCOs (four
  simultaneous waveforms, strong and weak sync), 708A noise and ring modulator, VCF-1 703E (24 dB
  OTA low-pass with voltage-controlled resonance and the original passband loss) or 703A, VCF-2 703B
  (12 dB HP/BP/LP), two 704 VCAs (lin/exp), the 711A output module (spring reverb, three-channel
  panner, 8-stage phase shifter, VU meters, standard oscillator), 712A voltage processor, two LFOs,
  sample and hold, 707A amplifier/envelope follower/integrator, dual ADSR, mixer and multiples.
- The 701A keyboard, duophonic as the original (KEY CV1 the lowest key, KEY CV2 the highest), and the
  717A analog sequencer: 3 x 12 steps or 36 in series, divide and fine per step, hold, analog inputs,
  master controller and pulse shaper.
- 137 jacks at 1/4", multiples that take a cable in any jack, stacked plugs, views of the sequencer,
  console, keyboard or all three cabinets.
- Extensions: up to 8 voices (the voice modules are duplicated per key), MIDI with velocity, mod wheel
  and aftertouch jacks, MIDI learn, host tempo and transport for the 717A, VCO drift, key CV droop.
- 31 presets plus Init, with their cables.

## ATFS1

- 8 voiced operators per voice with the FS1R's 8 spectral forms (sine, all1/2, odd1/2, res1/2, frmt),
  modelled as a carrier in a pitch-synchronous window (Yamaha's patent), with the window lengths, skirt
  exponents and group widths measured on real units; 8 unvoiced noise-formant operators.
- The 88 algorithms with feedback, FM depth of 3.37 cycles, the FS1R EG law (6.1 ms to 350 s per
  traverse, linear decays, exponential attacks), pitch EG, two LFOs, per-voice filter with its EG,
  Formant and FM knob matrices, insertion/variation/reverb effects.
- Formant Sequences at 126 frames per second, 21 factory ones (vowels, choirs, scat, spoken words), and
  new ones from any WAV or AIFF (autocorrelation pitch, LPC-root formants) or from typed vowels.
- Extensions: a full-screen editor under a working 1U front panel (algorithm graph with live levels,
  spectrum per operator, draggable envelopes, inline help), 32 voices with the filter on every voice,
  an Fseq per preset, interpolated playback, 10-1000 % speed, FS1R sysex import and export.
- 32 presets of our own. No Yamaha voices or sequences are included.

Technical notes: [`plugins/atfs1/README.md`](plugins/atfs1/README.md) and
[`plugins/atfs1/docs/hardware-spec.md`](plugins/atfs1/docs/hardware-spec.md).

## ATMD

- Three oscillators with the six waveforms of the Model D (osc 3 with the reverse sawtooth), a passive mixer
  that saturates the filter input, white/pink noise and an external input with the OVERLOAD lamp.
- The four-pole transistor ladder solved without delay, with a saturating stage in every pole: one octave per
  division on the cutoff dial (−1.1 = 440 Hz), self-oscillation from EMPHASIS 8, resonance that fades with a
  low cutoff, the passband loss.
- Two contours with the DECAY switch, glide in time per octave, low-note priority and single trigger, the
  pitch and mod wheels, osc 3 as a modulator.
- A panel built to the original's measurements and rendered in Blender, where every control works; up to 8
  voices, unison, velocity and pressure, delay and reverb as extensions; 32 presets.

Technical notes: [`plugins/atmd/README.md`](plugins/atmd/README.md) and
[`plugins/atmd/docs/hardware-spec.md`](plugins/atmd/docs/hardware-spec.md).

## Orrery

- A MIDI generator, not a synthesizer: eight lanes (drone, pad, melody, arpeggio, cloud, bass, pulse)
  play other instruments, each on its own MIDI channel, over a slow shared harmony with 16 scales,
  voice leading, chord colour and tension.
- Loops of prime lengths that drift in and out of phase for hours, Euclidean rhythms, MIDI echoes,
  controller curves and pitch-bend drift for analog synths.
- Sessions with slow fades, a breathing pace and a gentle mode for sleep; 21 presets in seven moods;
  Simple mode with five macros and Advanced mode with every lane parameter.
- Direct MIDI ports per lane with MIDI clock for hardware synths, Link (Lead/Follow) between instances
  for Ableton Live, MIDI capture to a file, and a built-in preview sound.
- The same variation number plays the same notes on macOS and Windows, at any buffer size and sample
  rate.

Technical notes: [`plugins/orrery/README.md`](plugins/orrery/README.md).

## Murmur

- A granular effect: everything that comes in is recorded into a memory of up to 16 seconds, and up to
  128 grains (5 ms to 2 s) read it at once, each with its own pitch, direction, envelope and pan.
- Freeze (button or sustain pedal), Scan from −2 to +2 × to stretch a frozen sound without changing its
  pitch, feedback into the memory with shimmer and sub octaves for rising clouds.
- Random pitch snapped to octaves, fifths, chords or scales; grains on the host grid with chance; two
  LFOs on nine targets; low and high cut, ducking, delay and plate reverb.
- MIDI Pitch mode plays the grains' pitch from a keyboard, Gate mode plays the cloud only while keys
  are held.
- The window draws the memory as a horizon and each grain as a bird; Simple and Advanced pages; 30
  presets in eight categories.

Technical notes: [`plugins/murmur/README.md`](plugins/murmur/README.md).

## Lacquer

- Automatic mastering: it measures the mix (BS.1770-5 loudness and true peak, loudness range, spectrum of
  the loud parts, stereo, harshness, sibilance, clipping) and sets a whole chain only as much as the mix
  needs: tone EQ toward our own target curves, two dynamic EQ bands, three-band and glue compression,
  mono lows and width, oversampled saturation and clipper, and a look-ahead limiter with 8x true-peak
  detection.
- Two ways in: Learn on the master bus (it listens while the song plays), or master a file with A/B at the
  same loudness and export. `lacquerrender` is the same engine from the command line, with a JSON report.
- Six styles, an intensity knob that never changes the loudness, seven loudness targets (Streaming -14,
  Apple -16, Loud -11, Club -8, CD -9, Broadcast -23, Match) and a ceiling.
- Deterministic and block-size independent; tested against the EBU Tech 3341/3342 signals and with a
  15-part test suite (`lacquertest`, `lacquerlivetest`).

Technical notes: [`plugins/lacquer/README.md`](plugins/lacquer/README.md) and the research behind it,
[`plugins/lacquer/docs/research.md`](plugins/lacquer/docs/research.md).

## Halation

- A delay and a reverb in one space: eight delay lines whose mixing matrix, diffusion and lengths move from
  separate echoes on the beat to a dense room with one control, Delay to Space. Length is a T60 (0.2 to
  60 s, or endless), so the tail keeps its length at any position.
- Echo patterns and ping-pong; digital, tape, BBD and spring characters; room, hall, plate and chamber.
- Ten modules inside the loop: freeze (all or by bands), shimmer, reverse, grains, spectral smear, a
  resonator tuned to a chord or to MIDI notes, stutter, breathe and collapse, duck and gate; an envelope
  follower and two scenes with Morph.
- A limiter in the loop and a -1 dBFS ceiling: no runaway feedback, even at infinite. Zero latency,
  deterministic, tested with `halationtest`; the window draws the effect as light in a plaster room.

Technical notes: [`plugins/halation/README.md`](plugins/halation/README.md) and the research behind it,
[`plugins/halation/docs/research.md`](plugins/halation/docs/research.md).

## Tarnish

- Six machines: studio tape (30, 15, 7.5 and 3.75 ips, IEC or NAB), cassette (ferric, chrome, metal, half
  speed), video linear audio, vinyl (33, 45, 78), AM radio and telephone.
- Jiles-Atherton magnetic hysteresis at 4x, integrated over the head-field path (sample-rate independent),
  with drive, saturation and bias; record and playback equalisation as exact inverses; head bump, spacing
  and azimuth losses, self-erasure, hiss through the playback chain.
- Wow and flutter from the transport's spectral lines (reels that follow the pack radius, roller, capstan,
  motor) plus drift, weighted and measured; dropouts as head-to-tape spacing; crackle and scratches locked
  to the rotation; noise reduction with mistracking; tape stop, varispeed and a band-limited crusher.
- Everything random is a hash of the timeline position: bounces match playback. The window draws each
  machine from above, with the dropouts reaching the play head as you hear them and the magnetic loop live.

Technical notes: [`plugins/tarnish/README.md`](plugins/tarnish/README.md) and the research behind it,
[`plugins/tarnish/docs/research.md`](plugins/tarnish/docs/research.md).

## AT303

- A four-pole diode ladder with the original's capacitor ratio, zero-delay feedback solved every sample; it loses
  bass as the resonance rises and rings near 500 Hz at half cutoff. The filter and volume envelopes, the accent
  VCA path and the accent sweep circuit (C13) are their RC networks, so runs of accents climb; RC slide.
- The original's two-list sequencer (pitches in order, time per step) with a visible editor, 64 patterns,
  triplets, host sync locked to the bar or its own clock, MIDI mode; an acid line generator with Mutate.
- Sixteen classic circuit modifications on one page with a Stock button; drive and a synced ping-pong delay.
- The window is an original graphite instrument rendered in 3D, with Inspect: the circuit drawn live.

Technical notes: [`plugins/at303/README.md`](plugins/at303/README.md) and the research behind it,
[`plugins/at303/docs/research.md`](plugins/at303/docs/research.md).

## AT330

- A top-octave divider with every note phase-locked and within 2 cents, and the waveforms measured on the original's
  adjustment sheet: an exponential-charge sawtooth at 8' and the truncated one at 4'.
- Strings at 4'; a choir of male 8', male 4' and female 4' voices through one fixed bank of seven formant filters;
  the original's single envelope per section (it re-attacks the whole chord on every key) as Classic, and per-key
  envelopes as Poly.
- A ten-band vocoder, 200 Hz to 6 kHz, through the audio input (sidechain): Hold, a consonant path, direct mic,
  and the carrier from the keys, an external signal on the right channel, or both; Freeze and formant shift.
- The four-BBD ensemble with the LFO periods measured on the original (3.9 s / 150 ms, 4.6 s / 175 ms), a BBD
  vibrato with delay, a pitch shift that bends down; velocity, pitch bend both ways, mod-wheel vibrato, a plate reverb.
- The window is a rendered instrument in oiled walnut with knobs, tablets, the band display and the keyboard;
  45 presets.

Technical notes: [`plugins/at330/README.md`](plugins/at330/README.md) and the research behind it,
[`plugins/at330/docs/research.md`](plugins/at330/docs/research.md).

## Plumb

- Three latency modes from one chain: Live 32 samples (0.67 ms at 48 kHz), Tight 128 (2.7 ms), Studio 1024 (21 ms),
  the same milliseconds at every sample rate, measured equal to the reported latency.
- A YIN-family detector on a decimated copy, refined at full rate over two periods, with octave and continuity
  guards and a voicing gate; median error under 1.3 cents on synthetic vowels from 82 to 880 Hz.
- The pitch is split into note centre, drift and vibrato; Style goes from moving only the centre (Natural) to
  every note exactly on pitch (Hard), with the character controls by hand, 14 scales and a custom one, key
  detection, MIDI scales and note lock.
- Shifting by repeating and dropping single cycles of the LPC excitation at quiet points, with the formants held by
  a 48th-order model in normalised lattices (Throat moves them); with Amount at 0 the output is the input, exactly.
- The window is a slate with a brass plumb bob that swings with the tuning error, and a live note view.

Technical notes: [`plugins/plumb/README.md`](plugins/plumb/README.md) and the research behind it,
[`plugins/plumb/docs/research.md`](plugins/plumb/docs/research.md).

## Manuals

Every plugin has a full user manual with screenshots, in English and Spanish, inside the plugin (the
Manual button in its top bar) and on the website (`/<name>/manual/`, `/es/<name>/manual/`). Both come
from the same files, `plugins/<name>/docs/manual/en.md` and `es.md`, written in the Markdown subset of
[`shared/manual/FORMAT.md`](shared/manual/FORMAT.md). `shared/manual/ManualView.h` renders it in the
plugins (embedded with `dh_add_manual` in CMake), `scripts/manual_web.py` on the website, and
`python3 scripts/manual-shots.py [name]` regenerates the screenshots from each `shots.json` with the
standalone apps.

## System requirements

| | Minimum | Recommended |
|---|---|---|
| macOS | macOS 11 Big Sur or later; Apple silicon (M1 or later) or Intel Core i5 quad-core from 2017 | Any Mac with Apple silicon |
| Windows | Windows 10 or 11, 64-bit; Intel Core i5-6500 (2015) or AMD Ryzen 5 1600 | Intel Core i5-12400 or AMD Ryzen 5 5600 |
| Linux | x86_64 with glibc 2.35 or later (Ubuntu 22.04, Debian 12, Fedora 36 or newer), X11 or XWayland; the same processors as Windows | the same as Windows |
| All | 4 GB RAM, 1280 x 800 screen, a 64-bit VST3, AU or LV2 host (or the standalone app) | 8 GB RAM, 1920 x 1080 screen |

Measured CPU load, percentage of one core at 48 kHz, sound engine only (the editor adds about 5-15 %
on an M4). Hosts run each instrument on one core, so single-core speed is what counts.

| Setting | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| AT20V mono, 4x | 1.6 % | 3.6 % |
| AT20V 8 voices with delay and reverb, 4x | 11.2 % | 32.4 % |
| AT20V 12 voices, 8x (needs the recommended CPU) | 28.9 % | 78.1 % |
| AT101V mono, 4x | 1.0 % | 1.8 % |
| AT101V 6-voice pad, 4x / 8x | 7.8 % / 14.5 % | 14.5 % / 27.6 % |
| AT42 every voice and the effector, 2x / 4x | 2.6 % / 4.8 % | 7.5 % / 14.5 % |
| AT700 duo, one voice of the console, 2x | 1.7 % | 3.5 % |
| AT700 6-voice strings with phaser and reverb, 2x | 6.1 % | 13.6 % |
| AT700 8 voices, every voice module, 4x | 16.1 % | 34.9 % |
| ATFS1 one voice speaking a Formant Sequence | 0.4 % | 0.7 % |
| ATFS1 16-voice vowel choir with chorus and reverb | 6.8 % | 13.9 % |
| ATFS1 32 voices, 16 operators each (needs the recommended CPU) | 8.7 % | 21.9 % |
| ATMD one voice, three oscillators, 4x | 0.85 % | 1.54 % |
| ATMD eight voices, 4x | 4.79 % | 8.32 % |
| Orrery generator, 8 busy lanes | 0.003 % | 0.009 % |
| Orrery with its preview sound, busiest preset | 1.6 % | 3.8 % |
| Murmur, heaviest factory preset (Granular Wash) | 0.56 % | 1.50 % |
| Murmur, 128 grains of 1 s with delay and reverb | 2.55 % | 6.22 % |
| Lacquer, the chain learned from the demo mix | 2.84 % | 4.84 % |
| Lacquer, every module on | 2.91 % | 4.89 % |
| Halation, Cathedral of Glass | 0.47 % | 1.33 % |
| Halation, Spectral Ice (heaviest factory preset) | 1.56 % | 2.25 % |
| Halation, every module on, Length at infinite | 2.65 % | 4.96 % |
| Tarnish, studio tape (default, magnetic model at 4x) | 5.29 % | 5.46 % |
| Tarnish, Dusty Cassette | 5.64 % | 6.05 % |
| Tarnish, Worn Vinyl | 0.47 % | 1.11 % |
| AT303, the default sound at 4x, sequencer running | 0.86 % | 1.79 % |
| AT303, square, distortion, delay and filter FM | 1.17 % | 3.38 % |
| AT303, the default sound at 8x | 1.65 % | 2.99 % |
| Plumb, Natural Lead (Tight mode) | 3.16 % | 3.53 % |
| Plumb, Hard Tune (Tight mode) | 3.16 % | 3.54 % |
| Plumb, Throat Down (formants moved) | 7.95 % | 9.61 % |

Measured with the offline renderers (`at20vrender`, `at101vrender`, `at42render`, `at700render`, `atfs1render`, `orreryrender`, `murmurrender`, `atmdrender`, `lacquerrender --bench`, `halationrender`, `tarnishrender`, `plumbrender`, and `at303test` for AT303); the minimum
processors are extrapolated from these two machines by single-core benchmark ratios.

## Download

Prebuilt packages for every plugin are attached to the [releases](https://github.com/palmereta/audio_tools/releases) of this repository,
one release per plugin version (tag `<plugin>-v<version>`), and on https://audiotools.com.ar. Each plugin comes as
`<Name>-<version>-macOS.zip` (macOS 11+, universal arm64 + x86_64), `<Name>-<version>-Windows.zip`
(Windows 10/11 x64) and `<Name>-<version>-Linux.zip` (x86_64, glibc 2.35+, VST3 + LV2); each zip includes an `INSTALL.txt`. In hosts the plugins are named **Audio Tools <name>**
(for example Audio Tools Tarnish), under the maker Audio Tools.

## Build from source

```bash
git clone --recurse-submodules https://github.com/palmereta/audio_tools.git
cd audio_tools
sh scripts/build-macos.sh                                          # macOS
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1 # Windows
sh scripts/build-linux.sh                                          # Linux, in Docker on the docker host (ssh docker)
```

| Platform | Requirements |
|---|---|
| macOS | Xcode Command Line Tools, CMake ≥ 3.22, Ninja |
| Windows | Visual Studio 2022 Build Tools (C++), CMake ≥ 3.22, Ninja |
| Linux | Docker on the build host (`scripts/linux/Dockerfile`, Ubuntu 22.04 for glibc 2.35); or locally g++ ≥ 9, CMake ≥ 3.22, Ninja and the packages in `external/JUCE/docs/Linux Dependencies.md` |

JUCE 9.0.4 is a git submodule (`external/JUCE`). If it is missing, CMake downloads it. The scripts
build every plugin and leave the packages in `dist/` and `web/downloads/`.

## Repository layout

```
CMakeLists.txt        root project: JUCE + one add_subdirectory per instrument
external/JUCE         JUCE 9.0.4 (submodule)
plugins/at20v/        AT20V: DSP, interface, presets, Blender art pipeline, docs
plugins/at101v/       AT101V: same structure
plugins/at42/         AT42: same structure
plugins/at700/        AT700: same structure
plugins/atfs1/        ATFS1: same structure, plus the Formant Sequence library
plugins/atmd/         ATMD: same structure, the 3D panel pipeline and the hardware reference
plugins/at303/        AT303: voice and sequencer (JUCE-free), rendered instrument with Inspect, presets, renderer, tests, Blender scene
plugins/orrery/       Orrery: generator, preview sound, interface, presets, tests, Blender scene
plugins/murmur/       Murmur: granular engine, flock interface, presets, tests, Blender scene
plugins/lacquer/      Lacquer: mastering engine (JUCE-free), plugin with file mode, renderer, tests, Blender scene
plugins/halation/     Halation: delay/reverb engine (JUCE-free), room view, presets, renderer, tests, Blender scene
plugins/tarnish/      Tarnish: tape and lo-fi engine (JUCE-free), machine view, presets, renderer, tests, Blender scene
plugins/plumb/        Plumb: pitch correction engine (JUCE-free), slate and plumb bob view, presets, renderer, tests, Blender scene
plugins/pallor/       Pallor: drum engine and sequencer (JUCE-free), effects, rendered editor, presets and pattern library, renderer, tests, manual
plugins/thrum/        Thrum: guitar engine and guitarist (JUCE-free), pedals, amps, cabinet, rendered editor, presets, renderer, tests, Blender scene, manual
shared/manual/        the in-plugin manual viewer, its Markdown subset and the CMake helper
shared/dsp/           DSP shared by the instruments (oversampling, BLEP, delay, reverb, vocoder)
shared/ui/            UI shared by the instruments (patch cable rope physics)
shared/tools/         code shared by the offline render tools (MIDI file reader)
scripts/              build and packaging for macOS, Windows and Linux, website generator
```

## Testing

`tools/hosttest` builds `athosttest`, a small plugin host that loads the compiled VST3 (or AU) the way a DAW
does and checks what comes out. It runs on macOS, Windows and Linux (`scripts/linux/qa.sh`, headless with xvfb).

```bash
cmake --build build --target athosttest
build/tools/hosttest/athosttest_artefacts/Release/athosttest build/plugins/at101v/AT101V_artefacts/Release/VST3/AT101V.vst3 --out hosttest-out
athosttest --chain Orrery.vst3 AT700.vst3 --secs 90     # Orrery playing a synth, every Orrery preset
```

| Test | What it checks |
|---|---|
| `programs` | every factory program with a chord: no NaN/Inf, peak, DC, silence after the release (sequence presets are also tried with the host transport running; generators by their MIDI) |
| `midi` | 27 MIDI scenarios: velocity-0 note-offs, orphan note-offs, the same note twice, 128 notes at once, CC123/CC120, sustain, pitch bend and mod wheel extremes, all 128 CCs, 16 channels, notes 0 and 127, events on the last sample, 5 ms retriggers, program changes, garbage sysex, random MIDI storms, random parameter changes with keys held (voices, unison, quality, modes), bypass with the note-off inside the bypass |
| `rates` | 44.1–192 kHz × 32/512/4096-sample blocks, 1-sample blocks, random block sizes with 0-sample blocks: level and tuning against 48 kHz |
| `state` | save/restore into a new instance (parameters, level, correlation) and seven kinds of broken state |
| `fuzz`, `params` | every parameter at its minimum and maximum, and random automation while playing |
| `cpu` | a long silence after a note (denormals) |
| `reprepare` | sample-rate change and `reset()` with notes sounding |
| `midiout` | MIDI out balanced (every note-on gets its note-off) through transport jumps, tempo changes, stop and random parameters |

Each plugin also has engine tests without JUCE in `plugins/<name>/tools/dsptest.cpp` (`gentest.cpp` for
Orrery). The results of the 1.0.2 / 1.0.1 round, the bugs it found and how each was fixed are in the commit
message of that release.

## Website

The website, https://audiotools.com.ar, is generated by `scripts/build-web.py` with the demo clips of
`scripts/samples.py`. Its folder (`web/`, with the downloads, films and clips) lives in the development
repository and is not part of this one.

## Why open source

Synthesizers are cultural instruments. When a commercial plugin is discontinued, the music and
sounds built with it can disappear too. Publishing the models keeps the knowledge of how these
circuits behave available to musicians, students and anyone who wants to improve them.

## License

[GNU Affero General Public License v3](LICENSE): the same license as the JUCE framework these
plugins are built on. You may use, study, modify and share the code. If you distribute a modified
version, you must publish your changes under the same license.

Korg and MS-20 are trademarks of Korg Inc. Roland, SH-101 and System-700 are trademarks of Roland Corporation.
Yamaha and FS1R are trademarks of Yamaha Corporation. Elta Music and SOLAR 42 are trademarks of their owners.
Audio Tools is an independent project, not affiliated with or endorsed by Korg, Roland, Yamaha, Elta Music
or Arturia. Product names are used only to identify the historical
instruments that inspired these recreations. Reference photographs are credited in
[`plugins/at20v/docs/ref/CREDITS.md`](plugins/at20v/docs/ref/CREDITS.md) and
[`plugins/at101v/docs/ref/CREDITS.md`](plugins/at101v/docs/ref/CREDITS.md),
[`plugins/at42/docs/ref/CREDITS.md`](plugins/at42/docs/ref/CREDITS.md),
[`plugins/at700/docs/ref/CREDITS.md`](plugins/at700/docs/ref/CREDITS.md) and
[`plugins/atfs1/docs/ref/CREDITS.md`](plugins/atfs1/docs/ref/CREDITS.md).

---

**Español.** Audio Tools recrea sintetizadores legendarios como plugins libres y de código abierto
para macOS, Windows y Linux: el AT20V (Korg MS-20), el AT101V (Roland SH-101), el AT42 (Elta Music SOLAR 42), el AT700 (Roland System-700), el ATFS1 (Yamaha FS1R) el ATMD (Moog Minimoog Model D) y el AT303 (un sintetizador de bajo clásico de 1981 con su secuenciador), y cinco herramientas propias: Orrery, un generador de MIDI para
ambient, dark ambient y drone que los toca (y toca sintes físicos), Murmur, un efecto granular en tiempo real, Halation, un delay y una reverb en un solo espacio, Tarnish, una máquina de cinta y lo-fi, y Lacquer, un masterizador automático (en vivo con Learn o desde un archivo). Cada plugin trae su manual completo con capturas, adentro (botón Manual) y en la web. La web está en `web/`, con
el inglés en `/` y el español en `/es/`. Los builds están en `web/downloads/`, y el código se
publica bajo la licencia GNU AGPL v3.
