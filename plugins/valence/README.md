# Valence

A multi-layer MIDI arpeggiator. Four layers orbit the chord you hold; each has its own length in steps, step
figure, pattern, octaves, gate, swing and MIDI channel, so layers of 16, 15 and 13 steps drift in and out of phase
and only realign after minutes. VST3, Audio Unit and AU MIDI FX (macOS), LV2 (Linux) and standalone. Version 1.0.0.

Valence makes no sound by itself: it plays other instruments. The built-in preview sound (pluck, filtered bass,
chord stab, sub; one per layer) exists only to try it without routing anything.

## What it does

- **Input.** The notes you hold, with latch, the sustain pedal as latch, transpose and the chord order (as played,
  low to high, high to low, reproducible random). **Complete**: with one or two notes held it builds the scale chord
  on the lowest one (triad, seventh, ninth) or adds octaves; the added notes are *neutrons*, the held ones *protons*.
  **Auto harmony**: Orrery's chord walk plays the chord by itself (press Play), for demos and fully deterministic
  pieces. **Link**: a Lead instance shares its chord; Follow instances play over it.
- **Layers (4).** On, channel, figure (1/4 to 1/32, straight, triplet, dotted) times a multiplier, length 1-32,
  pattern (up, down, up-down with and without repeated ends, converge, diverge, as played, random, random walk,
  chord, pedal, finger order), octave range 1-4 and mode (stack, alternate, random), register, index shift, gate,
  swing, velocity (fixed, as played, per step), humanize, Euclidean generator (pulses, rotation), new-chord
  behaviour (restart at step 0 or keep phase), mutation and a CC number for the CC row.
- **Step rows** (in the state, not parameters): on, velocity, accent, tie, ratchet 1-4, probability, octave, fixed
  note (a scale degree) and a CC value.
- **Mutation.** At the start of every cycle a layer changes a little (probability, octave, a step that appears or
  goes, velocity, ratchet), with its own random stream: turning it on does not change the notes of the others.
- **Global.** Host sync or own tempo, scale and key, variation (seed), phrase (every N bars all layers back to step
  0; off = free polymeter), accent amount, output on channel 1 (default, works with any synth) or per layer.
- **Outputs.** The host always; per-layer direct MIDI ports (own thread); on macOS and Linux each layer as a virtual
  MIDI port ("Valence Layer 1".. "Valence Layer 4", "Valence 2 Layer 1" for a second instance); MIDI clock out;
  capture of the last minutes as a MIDI file (one track per layer).
- **Fusion.** The engine predicts when all layers are back at step 0 together (shown on the Layers page and in
  `events.json`). The atom view of Phase 2 is built on it.

## Layout

```
../../shared/gen/Theory.h   scales, chords, the chord walk, deterministic math (dm::), RNG (shared with Orrery)
Source/gen/Engine.h         the engine: input, Complete, Auto harmony, layers, steps, ties, ratchets, collisions, fusion, clock
Source/gen/Preview.h        the preview sound (pluck, bass, stab, sub; CC 74 of a layer opens its filter)
Source/ParamsVal.h          host parameters (64 automatable first) and the step rows as text (state) and JSON (presets)
Source/PluginProcessor.*    host MIDI, direct and virtual ports, Link, clock out, capture, presets, state
Source/ui/                  Look and Widgets (Orrery's), the Layers and Outputs pages (the atom comes in Phase 2)
tools/valencetest.cpp       engine tests without JUCE (12 numbered tests, fingerprint in test 2)
tools/render.cpp            valencerender: offline MIDI per layer, preview audio, events.json, --bench
tools/presets.py            factory presets (43, seven groups; the nine demo presets use docs/research.md 2.10)
```

## Tools

```bash
# Engine tests (Linux: g++; Windows: cl /std:c++20 /O2 /fp:precise /EHsc ...). `valencetest 2 --long`: 20 minutes, 15 hosts.
clang++ -std=c++20 -O2 -ffp-contract=off -I plugins/valence/Source/gen plugins/valence/tools/valencetest.cpp -o /tmp/valencetest && /tmp/valencetest

# Offline render (the transport runs from beat 0 at --bpm, or the preset's tempo)
valencerender preset.json input.mid outdir [--secs 60] [--bpm 124] [--sr 48000] [--seed n] [--block 256] [--from-beat 0]
valencerender preset.json auto outdir --secs 90       # no input: Auto harmony plays
valencerender --bench                                 # engine alone, 4 busy layers, 10 minutes

# Factory presets
python3 plugins/valence/tools/presets.py

# Drawing cost of the atom view, no window: demo dir, mode (none|direct|cachedfull|dirty|dirtylist), Hz, seconds, scale
valenceatombench 01_phase_drift dirty 24 20 1.7        # BENCH_FAST=1: no waiting, cost per frame on a warm core

# The plugin as a DAW would use it
athosttest build/plugins/valence/Valence_artefacts/Release/VST3/Valence.vst3
athosttest Valence.vst3 --tests hostgap,statebytes,repro,progorder,ports
```

`valencerender` writes `all.mid` (one track per layer, on each layer's output channel), `layerN.mid` (layer N alone
on channel 1, to play it through another plugin's render CLI), `preview.wav` (24 bits, 2.5 s of tail) and
`events.json`:

```
{ "preset", "category", "input", "bpm", "sampleRate", "seconds", "fromBeat", "ticksPerBeat",
  "layers": [ { "layer", "on", "channel", "length", "rate", "multiplier", "stepBeats", "pattern", "cycleBeats" } ],
  "notes":  [ { "layer", "t", "sample", "beat", "dur", "ch", "note", "vel",
                "nucleon": "proton"|"neutron", "src", "step", "accent", "tie", "ratchet", "fixed" } ],
  "cc":     [ { "layer", "t", "sample", "cc", "value", "step" } ],
  "chords": [ { "t", "sample", "beat", "protons": [notes], "neutrons": [notes] } ],
  "fusions":[ { "beat", "t", "sample" } ] }
```

`t` is seconds from the start of the render, `sample` the sample at `sampleRate`, `beat` the song position.
`src` is the chord note a played note comes from (the proton, or the proton a neutron was built on); a fixed note has
`src` -1. `chords` and `fusions` carry the exact sample of each change.

The editor can save a snapshot: `VALENCE_SNAPSHOT=/tmp/x.png VALENCE_PAGE=0|1 VALENCE_PRESET=n VALENCE_RUN=1
VALENCE_WAIT=s Valence.app/Contents/MacOS/Valence` (RUN switches to Auto harmony and presses Play).

## Time and determinism

Steps live on integer ticks: 100,800 per beat, divisible by every figure (1/4 to 1/32, triplet, dotted), ratchets of
2, 3 and 4 and swing in whole percent. A tick becomes a sample from an anchor (sample, tick, tempo) that moves only on
a tempo change, a transport jump or a new session, so the music does not depend on the block size. The five rules of
Orrery's "Determinism" table hold: time from an anchor, events in time order inside the block (input, chord change,
layers 1-4 at the same sample), no per-block random draws, the transport start as a new session anchored on the
song's 1/16 grid (a pre-roll waits for beat 0), and own math with `-ffp-contract=off` / `/fp:precise` and one random
draw per statement.

- **Sessions.** With the host's transport running the grid is the song's: a layer enters at the step its position
  gives (`grid mod length`, or from the last phrase start), so the same position plays the same step. A jump (a host
  loop) is a new session: every pass of a loop plays the same. With the transport stopped (or the Free clock) a
  session starts on the sample of the first note of a new hand and ends when everything is released.
- **Input.** Notes, pedal and CC 123/120 apply at their sample. A step reads the chord at its own sample; input at
  the same sample comes first. A chord that arrives after a step's sample plays from the next step (DAW clips land on
  the same sample as the grid, since both come from the same tempo map).
- **Humanize** is a reproducible delay of 0 to +N ms per step, never early: an early note would need its chord
  before the step's sample, so a chord played right on the beat would miss it.
- **Same note, same channel** (two layers, a ratchet, a tie, all layers on channel 1): if it is already sounding
  and started at the same sample, the notes **merge** (one note-on; it lasts as long as the longer one); if it
  started earlier, it is **cut** (note-off and note-on at that sample). Never two note-ons without a note-off in
  between, and no zero-length notes. The note-off always carries the layer of the note-on that sounded, so per-layer
  ports and the preview release it. Known limit: with all layers on channel 1 and each layer on its own port, a
  merged note reaches only the port of the layer that struck first; for one synth per layer use Per layer channels. A tie to the same note sustains without a new attack; a tie to another note is
  legato (the new note-on before the old note-off, at the same sample).
- **Fusion** is solved as congruences (Chinese remainder theorem) over each layer's cycle start and period in ticks;
  layers restarted out of phase may never realign ("never" in the view).

Checks (`valencetest`, macOS arm64 Apple clang and Linux x86_64 GCC 11, GCC 15.2 and clang print the same fingerprint
`fb6a4ec244705c77`, 3 minutes of four layers with ratchets, ties, swing, humanize, mutation, CC rows and Complete):

| Test | Result |
|---|---|
| 1 Patterns | 21 exact sequences (chords of 1, 2, 3, 4 and 7 notes, octaves, shift, register, fixed note); chord, random and walk |
| 2 Determinism | 25,179 messages identical to the sample with blocks of 64/333/441/512/1024 and random 1-1024; at 44.1 and 96 kHz the same messages in order within 0.07 ms; long sweep (pc50) 169,529 of 169,529 over 20 minutes in 15 combinations |
| 3 Grid | 16,518 notes on the exact sample of their step (3 rates, 120/87/174 BPM, own clock and host, swing, 1/8T, 1/16D, 1/32T) |
| 4 Hanging notes | release, latch off, transport stop, bypass (HostGap), preset change, layer off, channel change, panic, input CC 123, pedal, Stop in Auto: 0 hanging, 0 double note-ons, 0 zero-length |
| 5 Polymeter | 16, 15 and 13 steps realign at step 3120; the counter predicts it from the start (780 beats); with mixed figures and partial restarts the prediction matches the first real fusion to the sample |
| 6 Latch, pedal, new chord | latch holds, a new hand replaces, adding adds, off silences; the pedal holds and releases; restart goes to step 0, keep phase does not |
| 7 Ratchets and ties | ratchet attacks and lengths exact; tie to the same note one note of a step and a half; tie to another legato; three layers on one channel merge and cut, no double note-on |
| 8 Complete | 79,192 notes in 5 scales over 50 minutes (all Complete modes, fixed notes, Auto harmony): none outside the scale |
| 9 MIDI traffic | one layer at 1/32, ratchet 4, 180 BPM, CC every step: 216 messages per second on its channel; four such layers on channel 1: 624 (a 5-pin cable carries about 1000) |
| 10 Transport | host clock: same messages (±0 samples) with blocks 64/333/768/1024, pre-rolls of 999 and 12,345 samples, 1.3 and 9 s stopped before; host loop of 4 bars: every pass identical; tempo change mid-bar: the grid stays in place |
| 11 Clock and Link | 384 clock pulses in 7.5 s at 128 BPM on their exact sample, start and stop, same with any block; a follower plays only the leader's chord |
| 12 CPU | engine alone 0.0037 % of one Apple M4 core (0.0083 % on a Ryzen 9 5900XT) |

## Plugin

64 automatable parameters first (20 global, 11 per layer: on, channel, rate, length, pattern, octaves, register,
gate, swing, Euclid pulses, mutate), then 42 that are not automatable (the rest of each layer and the session
settings); 106 in all. The step rows travel in the state as text (same state, same bytes).

- `JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS` stays on, as in AT303, AT330 and DeviceNTI: a VST3 only receives CC
  through those parameters, and Valence reads the sustain pedal, CC 123/120 and forwards other CCs. With it off,
  athosttest `midi` left the arpeggio playing after CC 123. The price is the ~2,080 "MIDI CC n|m" in REAPER's list,
  after Valence's own.
- The AU MIDI FX (`aumi`, `Vlnm`, "Audio Tools Valence MIDI FX") is the same code without audio buses; the wrapper
  still renders a buffer, which stays silent (no preview sound). Time comes from the play head; with no tempo from
  the host it uses the last one it saw, else its own tempo. Whether Logic gives an `aumi` a play head is not yet
  checked (see docs/research.md, section 3).
- Live's panel shows the first 64 parameters: athosttest `info` checks that the VST3's first 64 are Valence's own and
  automatable (64 of 64; the VST3 bypass and the "MIDI CC" ones come after). The AU host lists them in another order
  (40 of the first 64 automatable); Live uses the VST3.
- `dh::HostGap` at the top of `processBlock` (REAPER does not call a bypassed plugin): output notes are released and
  the held input keys forgotten (their note-offs were lost); a latched chord stays. Preparing and the host's
  `reset()` leave the plugin as new. A new instance is program 0.

| Check (macOS arm64) | Result |
|---|---|
| athosttest, all default tests | VST3 121 PASS, 0 WARN, 0 FAIL; AU 121 PASS, 0 FAIL, 0 WARN; MIDI FX AU 0 FAIL, 44 WARN (programs: no audio, by design) |
| athosttest hostgap, repro, progorder, ports, zipper | PASS (ports: 4 virtual ports per instance, two instances, gone on close, also from the MIDI FX; zipper on Preview Volume at 64 and 512: largest gain step 1.4e-4 per sample, 4.3e-3 would be a block step) |
| athosttest statebytes | VST3 PASS; WARN on integer parameters reopened from random continuous values (cosmetic, as in the series); AU and MIDI FX: same bytes, but a new AU reports program -1 until the host picks one (JUCE's AU wrapper; AT330 and AT42 AU fail the same check, also pluginval's warning) |
| pluginval strictness 10 | VST3 (with GUI tests), AU and MIDI FX AU: SUCCESS (again after the note-off fix) |
| Linux x86_64 (Docker image of scripts/linux, Ubuntu 22.04, GCC 11, on pc50) | scripts/linux/qa.sh valence: athosttest 120 PASS, 1 WARN, 0 FAIL; pluginval 10 VST3 and LV2 SUCCESS; standalone opens and closes; hostgap, statebytes (all PASS but the integer WARN), repro, progorder PASS; valencetest same fingerprint. The one WARN is ports: the container has no ALSA sequencer |
| auval -strict | aumu Vlnc Atls and aumi Vlnm Atls: AU VALIDATION SUCCEEDED (JUCE 9.0.4: no "Default Format does not match" error on the MIDI FX) |

## Editor

Four pages; the editor opens at 85 % and comes back to the last page shown (not saved in the state).
- **Simple:** the atom and six macros, the on/off of each layer and the main harmony controls.
- **Layers:** the four layers as step strips; the selected one with all its settings and every step row (on,
  velocity, accent, tie, ratchet, probability, octave, fixed note, CC, finger).
- **Harmony:** input and chords (auto harmony, progression, complete, order, latch, key, scale, transpose), time
  (clock, tempo, phrase, variation, accent) and the nucleus on a keyboard (protons held, neutrons added).
- **Outputs:** as in Orrery.

The macros are not parameters: each one writes existing automatable parameters of the four layers (so state,
presets and Live's list do not change, and the host records what the macro moves). At the start of a gesture each
layer's value is the base, and the macro's offset is added to it, clipped to the range: going back returns the
exact base. They read the first layer that is on (Figure) or the average of the layers that are on, and show a
range when the layers differ (`Source/ui/Macros.h`).

| Macro | Writes | Step |
|---|---|---|
| Figure | `lN_rate` | one division (1/4, 1/8, 1/16, 1/32); each layer keeps its family (straight, triplet, dotted) |
| Density | `lN_eu_pulses` | share of the steps that play, as evenly spread pulses |
| Swing | `lN_swing` | 50 to 75 % |
| Octaves | `lN_octaves` | 1 to 4 |
| Mutation | `lN_mutate` | 0 to 100 % |
| Gate | `lN_gate` | 5 to 100 % of the step |

CPU of the message thread with a real window, Linux (Standalone under Xvfb on pc50, Ryzen 9 5900XT, editor at
85 %, 3 windows of 5 s, two runs). The engine plays at real time: the ALSA null device is paced by a small
`LD_PRELOAD` that waits each block's duration (`~/valence/pace/pace.c`, script `~/valence/cpu-win3.sh`). Without it
the null device runs the engine many times faster than real time and the atom is flooded with photons.

| Page | Before (commit 6607db9) | Now |
|---|---|---|
| Simple (atom) | 23.1 % (Atom page) | 3.5 % |
| Layers | 13.8 % | 1.8 % |
| Harmony | (none) | 1.6 % |
| Outputs | | 1.0 % |

## Atom view

Direction A ("Bohr luminoso", chosen at checkpoint 2). Live, the view draws into its own frame (`fb`, physical pixels)
over two caches of what does not move (back half of the orbits with their marks, front half), and per frame repaints
only the zones that change, in groups: each group is one rectangle and one pass with a simple clip. The front half of
the orbits goes over the nucleus and back electrons through a hand blend of its non-empty runs. The orbits precess
at 0.005 of the video speed (a cache rebuild and a whole repaint every ~6 s); the fusion is a fixed glow and the ring
at double speed. 24 Hz playing, 10 Hz stopped, 2 Hz while the host hides the window. Recording (`VALENCE_RECORD`)
draws everything every frame, as before.

`valenceatombench`, macOS arm64 (M4), editor at 85 % on a retina screen (scale 1.7), 2026-10-27:

| Demo | Everything every frame (`direct`, ms per frame, warm core) | Live (`dirty`, ms per frame, warm core) | Live, real time at 24 Hz (CPU of one core) |
|---|---|---|---|
| 01 Phase Drift | 3.56 | 0.77 (0.20 of it is the bench's own copy to the screen) | 8.2 % (16.6 % at 30 Hz before this work) |
| 08 Reese Engine (a fusion every ~7 s) | 8.98 | 1.05 | 10.4 % (28 % before) |

With a real window on Linux, see the table in "Editor".

The final frame of `dirty` is identical to `cachedfull` (the whole frame from the caches), pixel for pixel, in both
demos. Still open: the window's own cost on macOS (Core Animation and the copy to the screen) is not in these numbers
and needs the Standalone opened once.

## Film

`tools/film.json` cuts the main demo (`10_main`) to 86 s: techno from 1:07.2, the house up to its second fusion
(1:53.28), then the bridge and drum and bass from 2:20.16 to 3:00, both cuts on fusions. `tools/blender/valence.py
--mode film2` renders its nine shots from the events.json of the part playing in each (pc50-wsl, ~1.8 s per frame at
96 samples, 2,150 frames), `tools/blender/film_audio.py` makes the same cut of `10_main.wav`, and
`python3 scripts/film.py valence <frames>` puts them together at -16 LUFS.

```bash
flock ~/ai/prerendered/.gpu.lock blender -b --factory-startup -P plugins/valence/tools/blender/valence.py -- --mode film2 \
    --demo <demos>/10_main --samples 96 --shift 0 --res 1920x1080 --extra 12 --out ~/film/out/valence
```

## Web page

`scripts/build-web.py` (texts `VA`, accent `#7fd1c7`) publishes the page once `Valence-<version>-{macOS,Windows}.zip`
are in `web/downloads`. Its media:

- **Images:** `tools/web_images.py` from the Blender stills (hero, og-image, menu thumbnail, `hl-*`); `hl-layers` and
  the `ui-*` shots are 2x snapshots of the Standalone taken under Xvfb on pc50 (`snap2.sh`, the manual's presets and
  pages, `VALENCE_CLEAN=1 VALENCE_SNAPSHOT_SCALE=2`), sized to 1200 and 2400.
- **Listen:** `python3 scripts/samples.py --only valence` takes the nine finished demos (`01` to `09`, already at
  -16 LUFS) from `../audiotools-private/valence/demos` (`VALENCE_DEMOS` overrides it), encodes them to AAC and copies
  each `all.mid` as the MIDI download.
- **See it play:** three cuts of the atom video of the main demo (`VALENCE_RECORD`, synced to `10_main.wav`):
  techno 44.16 s, house 99.84 s, drum and bass 156.16 s, 26.88 s each (14 bars at 125 BPM), each with a fusion inside;
  `web/assets/video/valence-demo-<id>.mp4` (x264 CRF 27) and a poster at 6 s in `img/valence/demo/`.
- **Film:** see above; `scripts/film.py` takes the end card colour from the accent.

## Traps

| Symptom | Cause | Why it misled |
|---|---|---|
| athosttest `midi`: the arpeggio keeps playing after CC 123 (VST3 only) | with `JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0` a VST3 receives no CC at all (no pedal, no CC 123/120) | Orrery builds with 0 and passes, because it ignores its input |
| athosttest `programs`: Bubble "keeps sounding" | a note merged from two layers sent its note-off with the other layer; the preview (and a per-layer port) never released it | the MIDI to the host was balanced, so the MIDI checks passed |
| athosttest `statebytes` on AU: "new instance == program 0" FAIL | JUCE's AU wrapper starts on the host preset "Untitled" (present preset -1) until the host picks a program; Valence now refreshes the name on prepare, so the bytes match, but the program number stays -1 | it looks like Valence's program 0 trap; AT330 AU (build-at330) and AT42 AU fail the same check, and pluginval warns "Current program is -1" |
| atom view: 3.7 ms per frame just to copy the frame to the window (Linux) | with the editor at 85 % the view starts between two pixels, and JUCE only blits an image when the offset is whole or the resampling quality is low; otherwise it resamples every pixel | the frame was already in physical pixels, so the copy looked free; low quality makes JUCE round the offset (half a pixel, invisible) |
| atom view: 3.5 ms per frame in real time, 0.7 ms with `BENCH_FAST=1` | between frames the core slows down and the caches go cold; the % of a core is what Activity Monitor shows, the warm figure is what compares code changes | the same code looked five times dearer in real time; compare changes with `BENCH_FAST`, report CPU in real time |
| atom view: clipping to the exact shape of what moves went from 10 % to 44 % | JUCE's software renderer intersects every edge table with every rectangle of a clip list | fewer pixels, but each draw paid for the whole list; groups of single rectangles are cheaper |
| a chord pressed right on the beat missed that step | early humanize meant choosing the notes before the step's sample | only with Humanize on; fixed by making it delay-only |
