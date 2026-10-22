# Thrum

A complete electric guitar by physical modelling: six guitar types, strings that are waveguides, pickups with
their position and their passive circuit, a guitarist that assigns every note to a real string and fret,
strums, picks with the fingers and plays the articulations, and a reorderable pedal board, four amps and a
cabinet modelled from the speaker and the microphones. No samples. VST3, Audio Unit (macOS), LV2 (Linux),
standalone app, `thrumrender` (command-line renderer) and `thrumtest` (numbered engine tests), for macOS,
Windows and Linux. Version 1.0.0, with a user manual in English and Spanish (`docs/manual/`, also inside the
plugin and on the website).

The research behind it (string models, pickups, amps, pedals, what makes a virtual guitar sound real, the
market, MIDI conventions, the design and the test plan, with sources) is in [`docs/research.md`](docs/research.md);
§8.11 lists what was built differently from the proposal.

**Name.** The product name is one variable,
`THRUM_NAME` in `CMakeLists.txt`: the plugin, the bundle id and the tools (`<name>render`, `<name>test`) follow
it; parameter ids, presets and the saved state do not depend on it. Manufacturer code `Atls`, plugin code `Thrm`.

## How it works

Everything that makes sound is in `Source/dsp/`, without JUCE, so the plugin, the renderer and the tests run
the same code.

1. **Strings** (`Waveguide.h`). One single-delay-loop waveguide per polarisation (the pickup hears mostly the
   vertical one; the horizontal one decays 1.6 times slower and is detuned a fraction of a cent: two-stage
   decay and slow beating). Per trip: a 6-point Lagrange read (bends, slides, vibrato and the whammy move it
   without allpass transients), a one-pole loss filter fitted to the T60 of the fundamental and of 3 kHz, six
   dispersion allpasses fitted per fret to the stiff-string partials (B = π³·E·d⁴/(64·T·L²) from the gauge, the
   tension of the tuning and the vibrating length), the pick contact, the fret buzz and the excitation. The loop
   delay is corrected by the phase of the filters at the fundamental, so every fret is in tune (T1). Tension
   modulation raises the pitch of loud notes a few cents and lets it settle.
2. **Pick and fingers.** A short pulse (25-300 µs for a pick, 0.2-0.8 ms for a finger) low-passed by hardness
   and velocity, the scrape of the pick on the winding, the pick-position comb (the position is measured from
   the bridge, so the same pick sounds rounder high on the neck), the split between the polarisations by the
   pick angle and its sign by the direction. A new pick on a sounding string stops it first (one trip of the
   loop at low gain): a new note on the same string cuts the previous one, as on a guitar. Harmonics average k
   shifted copies of the excitation (only the multiples of k survive); pinch harmonics do it at the node under
   the pick.
3. **The guitar** (`Instrument.h`, `Guitar.h`). Six types (solid with three single coils, solid with two
   humbuckers, semi-hollow, hollow jazz, baritone, seven-string) with their scale, frets, string set, body
   sustain and brightness, three bridge/body modes (sympathetic strings, microphonic leakage on the hollow
   bodies) and pickups at their real distances from the bridge. Each coil is a comb on the loop (two reads: the
   aperture of a single coil, the two coils of a humbucker); the sum is integrated (the coil senses velocity) and
   goes through the passive network: coil L and R, self and cable capacitance, the volume pot and the amp's
   input, the tone pot and capacitor, solved as a third-order analog transfer and discretised by the bilinear
   transform when a control moves. A neck mode per type takes energy from the partials near it (the dead spot). Selector of three or five positions, volume, tone, cable length. Tunings:
   standard, drop, half-step down, open G, open D, open E, DADGAD, a tone lower, custom per string; four gauges;
   capo. Hand noises: finger squeak on the winding (a pulse train at the rate the finger crosses the turns),
   release noise, pickup hum and hiss.
4. **The guitarist** (`Performer.h`, `Fingering.h`). Modes: melody (polyphonic or monophonic with smart
   legato: overlapping notes become hammer-ons, pull-offs or slides on the same string), chords (the held notes
   are recognised by templates, 22 types with slash basses, and voiced by a search over the real tuning: open,
   barre, power, triads, jazz, in a chosen region), strum patterns and fingerpicking (32 and 11 patterns in the
   library, `tools/patterns.py`, plus an editor), synced to the host or to a clock started by the first key.
   **Strum timing**: centred on the beat (default; the step is resolved up to 45 ms early and the middle strings
   land on the beat, as players anchor them) or starting on the beat. A chord change during a step cancels the
   strings not yet picked and replays the step with the new chord within 8 ms.
   Fingering: every note goes to a string and fret by a cost (hand window, string changes, low positions, open
   strings), with a fixed hand position and a preferred string. Human variation (tuning, timing, force, pick
   position, hardness and angle) comes from a hash of the event index and the seed: the same MIDI renders the
   same audio at any block size (T5).
5. **Pedals, amp, cabinet** (`Pedals.h`, `Amp.h`, `Cab.h`, `Engine.h`). Fifteen reorderable units: noise gate,
   compressor (Lacquer's), wah (manual, auto, CC 11), green overdrive, distortion, fuzz (silicon or germanium),
   chorus, phaser, flanger, tremolo (sine, optical, harmonic), vibrato, delay (digital, analog, tape with
   Tarnish's hysteresis in the loop), reverb (spring, room and hall from Halation's network, with freeze), the
   amp and the cabinet. Drives use first-order ADAA (no latency). Amps: American clean, British crunch, modern
   high gain and a small combo; triode stages with cathode shelf, grid-current blocking, coupling and Miller
   filters, the FMV tonestack (formula checked against the nodal solution of the circuit, T10), a push-pull
   power stage with presence, depth and sag, at 2x, 4x or 8x. The cabinet is our own speaker and microphone
   model (five speakers, four boxes, dynamic/condenser/ribbon microphones with position and distance, two
   mics with their real time-of-flight difference, a floor reflection) rendered to a minimum-phase IR; eight
   factory IRs from the same model (`thrumrender --export-irs`) and user IRs (any WAV/AIFF/FLAC up to 3 s,
   resampled, and again when the host changes rate). The convolution is uniformly partitioned with no added
   latency (`Conv.h`: a 256-sample direct head and FFT partitions of 256 for the tail, our own real FFT).
   **Auto level** (on by default) compensates the loudness change of Gain; off, Gain also changes the volume
   as on a real amp (the two modes are equally loud at Gain 5, T14).
6. **Outputs.** The main stereo output and a second bus, "DI", with the guitar before the board, delayed by
   the amp latency so a re-amp lines up. Latency: 47, 55 or 57 samples (amp at 2x, 4x, 8x), the same with the
   amp off, measured equal to the declared one (T8).

## The editor

`PluginEditor.cpp` and `Source/ui/` (style "Stage", chosen among three directions): a 1360 × 850 canvas scaled
with the window. The top bar has the presets, the pages, **Inspect** and **Manual**. The live fretboard runs
across the window on every page: the rendered board of the chosen guitar (`board_<k>.jpg`, without strings)
with the strings drawn by the UI so the sounding ones vibrate (their level from the engine), a dot per finger
and a ring for open strings; the geometry (scale, frets, margins, string spacing) comes from `layout.json`, the
same numbers the render used. The pages:

- **Guitar**: the chosen type rendered in 3D (`guitar_<k>.jpg`), strings and tuning, the pickup selector with
  the labels of each guitar (five positions on the three single coils, Bridge / Both / Neck on the others),
  volume, tone, cable and the hand noises.
- **Play**: modes, articulations, legato, chords, the strum step editor and the library, the picking pattern,
  vibrato and bends, fingering, the picking hand, human variation and the MIDI export buttons.
- **Pedals**: the thirteen rendered pedals (`pedal_<id>.png`, orthographic so they fit any slot) and two plates
  for the amp and the cabinet, in chain order on two rows; footswitch to toggle, drag to reorder, the chosen
  pedal's controls below. The LED position comes from the render.
- **Amp / Cab**: 16 renders, every amp on every cabinet (`amp_<m>_<c>.jpg`), with the microphone sprites
  (`mic_<k>.png`) placed from the speaker's projection and the pad (position across the cone, distance).
- **Settings**: MIDI in and out, outputs, and the keyswitch and controller map with the real note names.

**Inspect** draws the technical layer in cyan: note names on the fretboard, pickup positions and the scale on
the guitar, the signal path and the delay's echoes on the pedals, the cabinet's frequency response (the same IR
the engine uses). The editor repaints only what changes (the fretboard only while something rings).

All the renders come from `tools/blender/thrum.py --mode ui` (Blender 5.2, Cycles; about 5 minutes on an M4).
Headless captures: `THRUM_SNAPSHOT=out.png THRUM_PAGE=play THRUM_PRESET=43 THRUM_DEMO="43 48 52 55"` on the
standalone (`THRUM_DEMO` plays those notes through the processor's keyboard; `THRUM_DEMO_PERIOD` repeats them,
`THRUM_INSPECT=1`, `THRUM_PEDAL=n`, `THRUM_SET="id=value"`); `scripts/manual-shots.py thrum` uses them.

## MIDI

**Keyswitches** (from the "Keyswitch base note" parameter, MIDI 12 by default, which is C0 when C4 = 60 or C-1
when C3 = 60; they only work below the lowest open string):

| Key | Articulation (latching) | | Key | While held |
|---|---|---|---|---|
| base + 0 | Sustain | | base + 12 | Up-strokes |
| base + 1 | Palm mute | | base + 13 | Feedback |
| base + 2 | Staccato | | base + 14 | Vibrato |
| base + 3 | Dead notes | | base + 15 | Mute all strings |
| base + 4 | Harmonics (natural, else artificial) | | base + 16 | Fingers |
| base + 5 | Pinch harmonics | | | |
| base + 6 | Tremolo picking | | | |
| base + 7 | Bend in | | | |
| base + 8 | Slide in | | | |
| base + 9 | Slide out | | | |

**Velocity**: below "Dead notes below velocity" a note is dead; above "Pinch above velocity" it is a pinch.
**Controllers**: CC 1 vibrato depth, CC 2 palm mute amount (0 = off), CC 4 whammy (down; the bridge flutters
when released), CC 11 wah (wah in "MIDI CC 11" mode), CC 64 let ring, CC 68 legato on/off, CC 74 pick
hardness, CC 120/123 all sound/notes off. Pitch bend bends the last string played (or all of them, an option);
channel pressure and polyphonic aftertouch add vibrato.
**MPE** (MIDI input "MPE"): each note keeps its channel; the channel's pitch bend (±48 semitones by default),
pressure and CC 74 act on the string that plays it. **Guitar** mode (one channel per string, as guitar MIDI
controllers send it): channel 1 is the first (highest) string; its bend range is a parameter.
**MIDI out**: what the guitar played, every string expanded (strums, fingering, legato), optionally one channel
per string. The Play page exports the current pattern over the chord being held (or C) and everything played
since the last clear to `.mid`.

## Tests

`thrumtest` (no JUCE; `thrumtest 3 9` runs tests 3 and 9, `--tuning` prints the full tuning table):

- T1 tuning of every string at frets 0, 5, 12 and 17 on the six types against k·f0·√(1 + B·k²);
- T2 fingering: every note playable and correct, reproducible, hand window, fixed position, preferred string,
  six-note chords on six strings;
- T3 chord recognition and 5184 voicing requests (styles, regions, three tunings): all playable;
- T4 strum timing: the same events at the same sample with blocks of 1, 7, 64, 333, 4096 and random, with the
  rounding gap a host leaves between blocks, and with the host stopped (the clock starts on the key);
- T5 determinism of the whole chain (processed and DI) across block sizes;
- T6 MIDI out balanced, guitar mode (channel per string with its own bend) and MPE per-note bend;
- T7 160 random settings, half of them with MIDI chaos (repeated keys, 0.6 ms notes, chords in one sample, sustain
  pedal, keyswitches): no NaN, no DC, no subnormals, silence at the end, no hanging MIDI out note;
- T8 latency declared = measured, DI aligned;
- T9 articulations: palm mute, natural harmonic, pitch bend, hammer-on on the same string, a new note cuts the
  old one, bridge brighter than neck;
- T10 the FMV tonestack formula against the nodal solution of the circuit;
- T11 cabinet: mic position and proximity, user IR resampling;
- T12 CPU;
- T13 partitioned convolution: equal to the direct convolution (2.5 s IR, error 2e-6), no latency, IRs of 3 s;
- T14 Auto level: Gain 3→9 within 6 dB with it, Gain 1→9 more than 8 dB louder without it, equal at Gain 5.

T4 also checks the strum timing: centred (the middle of a six-string strum on the beat, the same with blocks of
1 and 512), from the beat, and a quantised chord change on the beat.

Results on the Apple M4 (2026-10-21, universal build): 0 failures, also on the Intel slice under Rosetta;
fundamental within ±0.7 cents and partials within ±2.5 cents of the stiff string over 148 notes; voicing search
2 µs. `athosttest` 0 FAIL 0 WARN on the VST3 and on the AU, pluginval at strictness 10 (VST3 and AU), and
`auval -v aumu Thrm Atls` on both slices pass. `thrumrender --state-test` (bit-identical after a save and
restore) and `--check-presets` pass.

Windows (pc50, MSVC 19.44, 2026-10-21): `thrumtest` 0 failures, `athosttest` 142 PASS 0 WARN 0 FAIL, pluginval
strictness 10 SUCCESS. Linux (Docker, Ubuntu 22.04, `scripts/build-linux.sh` with `QA=1`): `thrumtest` 0
failures, `athosttest` 142 PASS, pluginval strictness 10 SUCCESS on the LV2 and on the VST3 in 26 of 28 runs, the
standalone opens and closes cleanly. The two VST3 failures (2026-10-22) are a segfault in pluginval's own Linux
VST3 run loop (`juce::RunLoop::Impl` dispatching a file-descriptor callback of an event handler already
unregistered) after the last test, while the validator thread stops; the first one looked like a crash in
"Fuzz parameters" only because the log is buffered. Every test had passed in both.

## CPU

One core, 48 kHz (`thrumrender --bench`, each preset's demo phrase, blocks of 512), 2026-10-21:

| | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| Average over the 69 presets | 3.25 % | 6.23 % |
| Heaviest preset | 4.91 % (Wah Lead) | 7.89 % (Pinch Squeal) |
| The guitar alone (DI presets) | 1.44 % | 2.66 % |
| High gain at 8x with delay and hall (`thrumtest` T12) | 8.5 % | 11.7 % |
| A 3 s user IR, one channel (T13) | 0.84 % | 1.20 % |

## Tools

- `thrumrender out.wav --preset "Name" [--midi f.mid | --chords "C G Am F"] [--bpm n] [--di di.wav]` renders
  with the full processor (latency removed); `--list`, `--bench`, `--check-presets`, `--state-test`,
  `--export-irs dir`, `--export-pattern f.mid`, `--snapshot ui.png`.
- `tools/presets.py` writes `assets/presets/` (69 presets in nine categories), `tools/level_presets.py` levels
  them to −20 LUFS with a −3 dBFS peak cap, `tools/patterns.py` writes the pattern library.
- `tools/demos.py OUTDIR` renders the listening clips (−16 LUFS, WAV and MP3, with an index), including a dark
  wave clip with Pallor and the AT101V; `scripts/samples.py --only thrum` turns them into the website's clips.
- `tools/blender/thrum.py`: `--mode ui` (the editor's renders and `layout.json`), `--mode photos --out dir`
  (the website's photos), `--mode film2` (the film's shots; `tools/film.json` and `scripts/film.py` assemble it).
- `tools/web_images.py <photos>` writes `web/assets/img/thrum/` from the photos and the manual captures.

## Code and licences

Written for this project from the papers and measurements cited in `docs/research.md`; no code was copied from
STK, Faust, Guitarix or Neural Amp Modeler (studied only). Reused from this repository: Halation's network
(`plugins/halation/Source/dsp/Network.h`), Tarnish's hysteresis (`plugins/tarnish/Source/dsp/Hysteresis.h`),
Lacquer's compressor (`plugins/lacquer/Source/dsp/Dynamics.h`) and `shared/dsp`. No samples or third-party
impulse responses: the factory IRs are generated by our cabinet model.
