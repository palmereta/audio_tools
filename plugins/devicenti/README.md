# DeviceNTI V

An original FM groovebox for downtempo, trip hop, acid jazz, ambient and dark ambient: four tracks, each a
four-operator FM voice with a sub layer, a drum machine of eight FM lanes, or a MIDI track for an external synth; a
64-step sequencer with step locks, conditions, chord mode and chord memory; one insert per track, a delay and a reverb
on sends, and a master bus with tape, glue, duck and a limiter. VST3 instrument, Audio Unit (macOS), standalone app
and `devicentirender`, a command-line renderer, on macOS, Windows and Linux (VST3, LV2). Version 1.0.1. Manual in
English and Spanish (`docs/manual/`, inside the plugin and on the website). Version 1.0.0 was called DeVicenti: the slug
(`devicenti`), the CMake target, the class names, the bundle id, the plugin codes and the LV2 URI keep that name, so
projects saved with 1.0.0 open in 1.0.1 unchanged.

The design notes, the decisions and the research behind it are in [`docs/research.md`](docs/research.md).

## How it works

The engine is `Source/dsp/`, without JUCE (the plugin, `devicentirender` and `devicentitest` use the same code):

1. **Sequencer** (`Seq.h`). 16 patterns of up to 64 steps; per track a length, a speed (1/8x-2x) and a swing amount
   (how much of the host Swing parameter it takes, counted from the pattern's start); eight lanes per KIT track. A step holds notes or a chord (scale degree or a chord memory slot), velocity, length with tie
   and slide, micro timing, retrig, a condition (probability, A:B, FILL, PRE, NEI, 1ST, LAST, BAR n/m) and up to 255
   step locks. Playback is a chain of pattern passes that is a pure function of the host position, so a jump or a
   loop recomputes from there; the steps of a block are evaluated in time order, so the result does not depend on
   the block size.
2. **Voices** (`Voice.h`). Four operators in eight algorithms, run at twice the sample rate with a 97-tap half-band
   decimator; the modulation index is limited per note (Carson plus the Bessel tail) so high notes do not alias.
   Drive and fold run at 4x with first-order ADAA; above F#4 the highest drive and fold gains are capped. A
   saturating ladder (LP24) or state-variable filter (LP12, BP, HP, notch, peak), SPAN, a sub oscillator that
   bypasses the drive and the insert, noise, AGE (tape-like drift and vintage voicing). Voices are rendered eight
   at a time in SIMD (NEON, SSE2 or plain arrays), structure of arrays; a group whose upper half is empty runs one
   register wide, and voices are compacted at control boundaries. A logical slot keeps the allocation order, so the
   sound does not depend on which group a voice lands in. 16 voices sound at once from 24 slots (a stolen voice
   fades in 5 ms while the new one starts).
3. **Buses and effects** (`Bus.h`, `Sends.h`, `Master.h`). Per track an insert (overdrive/fold, bitcrush/lo-fi,
   chorus/ensemble, phaser, ring modulator, comb tuned to the pattern root) and the AGE tape stage; a delay (tape,
   dub, ping-pong, freeze) and a reverb (spring, plate, hall, void, shimmer, freeze); the master bus. A bus or a send
   with no input and an output under -120 dBFS for longer than its longest delay stops being processed.
4. **MIDI.** Notes, bend, aftertouch, mod wheel and sustain into the focused track (OMNI) or one channel per track;
   a fixed CC map for every panel control plus MIDI learn, relative modes and CC feedback; program change selects
   the pattern; MIDI tracks and doubled tracks send notes, chords and locks out, delayed by the plugin latency; in
   the standalone, MIDI clock in and out.

Latency 200 samples at 48 kHz (decimator 24, the voice's 4x stage 8, track tape 96, master tape 72). 64 automatable
parameters (`Source/Params.h`): per track cutoff, resonance, filter envelope, drive, AGE, the two sends, level, the
four operator levels and the insert amount; global swing, master, delay time and feedback, reverb size and decay,
tape, glue, duck, limit, tempo and pattern. Everything else is project state (`Source/ProjectIO.h`, JSON with values
rounded to six significant digits, byte-stable).

## Editor

`Source/PluginEditor.cpp` and `Source/ui/`: 1120 x 830, resizable from 60 % to 160 % with a fixed aspect. The unit is
rendered in Blender from the panel table (`Source/PanelMap.h`, exported with `devicentirender --panel-json`):
`tools/blender/devicenti.py --mode ui` on the pc50 GPU, packed by `tools/pack_ui.py` into `assets/ui/bg.jpg`, one
sprite atlas (keys pressed and lit, LEDs green and red, keys rendered in two alternating passes so a crop never shows
a changed neighbour) and `ui.json` with each control's plane homography. Knob pointers and the five screens are drawn
through those homographies and every control is hit-tested through their inverses. The paint timer runs only while
the editor is showing; the screens redraw only when their content changes.

The panel follows the plan's control map: FUNC latches on a click (Shift holds it), a STEP key held for half a second
or Alt-clicked stays held for locks and trig fields, hold CHORD and play notes to store a chord memory. Back pages:
VOICE, OPS, FILTER/AMP, LFO, MOD, TRIG, CHORD/SCALE, MIX, FX, MIDI, SONG, SEQ (the step grid) and SETTINGS; MANUAL
when `docs/manual/` exists.

Headless screenshots and the editor's self-test on the standalone app: `DV_SNAPSHOT=/path.png DV_SNAP_SCALE=2
DV_SNAP_QUIT=1 DV_PRESET=n DV_TRACK=1..4 DV_PAGE=VOICE|...|MANUAL DV_REC=1 DV_FUNC=1 DV_HOLD=n DV_PLAYHEAD=n DV_W=w`;
`DV_UITEST=/path.txt` sends real mouse events to the projected position of every control and checks the result.

## Tools

| Tool | What it does |
|---|---|
| `devicentitest` | Engine tests T1-T13: tuning, aliasing at 2x (sine, feedback saw, FM, drive and fold on C2-C7), clicks (steals, chokes, legato, drive onsets, kill), envelopes, filters and DC, effects, sequencer timing, conditions, chord mode and memory, locks, determinism with any block size, CPU (16 voices), MIDI in and out |
| `devicentirender out.wav [--preset name \| --project f.json] [--bars n \| --seconds s] [--song] [--midi f.mid] [--stems dir]` | The full processor to WAV; `--bench` measures every preset, `--midi-test` and `--state-test` test the processor, `--panel-json` writes the panel table, `--no-cull` turns off the culling of inaudible voices (null tests) |
| `tools/presets.py`, `tools/genre_presets.py` | The 82 factory presets (`assets/presets/`; the last seven are the demo clips, from `tools/clips.py`) |
| `tools/headroom.py` | Preset levels (`tools/preset_levels.json`) |
| `tools/demo_songs.py`, `tools/demos.py OUTDIR` | The demo projects and their renders (-16 / -20 LUFS) |
| `tools/blender/devicenti.py` | `--mode ui` (the editor's layers), `--mode hero` (a still) |
| `tools/pack_ui.py <render dir>` | Renders to `assets/ui/` |

## Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target DeVicenti_VST3 DeVicenti_AU DeVicenti_Standalone devicentitest devicentirender
build/plugins/devicenti/devicentitest
```

## Measured (Apple M4, one core, 48 kHz, 512-sample blocks)

| What | Value |
|---|---|
| The 82 presets, each playing its pattern (10 voices on average) | 4.3 % average, 6.2 % worst |
| 16 voices, four-operator stack with feedback, LP24, AGE | 3.7 % |
| Nothing playing | 0.25 % |
| Drive 1 + fold 0.5 + feedback, alias energy below 20 kHz | C2-C4 under -38 dB, C5-C7 under -34 dB |

## Release QA (version 1.0.1, 2026-10-25)

| Where | What | Result |
|---|---|---|
| macOS 15, Apple M4 (universal build) | athosttest VST3 (all tests) | 157 PASS, 1 WARN, 0 FAIL; the WARN is midisync: the tested presets send no MIDI out |
| | pluginval strictness 10 VST3, with the editor tests | SUCCESS |
| | devicentitest T1-T13 on arm64 and on x86_64 (Rosetta, the SSE2 path), with the new host Swing (T7) and arp (T10) tests; `--midi-test`, `--state-test` | 0 failures; PASS |
| | editor self-test (DV_UITEST) on arm64 and x86_64 | 18/18 |
| | CPU (`devicentirender --bench`, one core) | 4.3 % average over the 82 presets, 6.2 % worst; 16 voices 3.7 % |
| | disk | VST3 21 MB, AU 21 MB, standalone 23 MB (universal) |
| | 1.0.0 projects: states saved by the published 1.0.0 zip, loaded by 1.0.1 (athosttest `--save-state`/`--load-state`) | VST3 and AU, three programs: every parameter equal, the played phrase bit-identical |
| | launcher (atlaunchertest): a renamed product updates in place; the real 1.0.1 zip | 52 checks and 60 with the real zip, 0 failed |
| Windows 11, Ryzen 9 5900XT (MSVC) | athosttest VST3; pluginval 10; devicentitest; `--midi-test`, `--state-test` | 157 PASS, 1 WARN (midisync), 0 FAIL; SUCCESS; 0 failures; PASS |
| | editor: self-test, open, idle | 18/18; opens and saves a snapshot in 0.4 s plus the 2.6 s wait; under 1 % of a core idle with the editor showing |
| | CPU | 9.5 % average over the presets, 14.0 % worst; 16 voices 6.9 %, the dense project 11.5 % (about 2.2x the M4, as the other plugins under MSVC) |
| | disk | VST3 9.6 MB |
| Linux (Ubuntu 22.04 container on a Xeon E3-1220 v3) | athosttest VST3; pluginval 10 VST3 and LV2; standalone | 157 PASS, 1 WARN (midisync), 0 FAIL; SUCCESS, SUCCESS; opens and closes cleanly |
| | devicentitest | every functional test passes; the timing checks fail on this 2013 shared host (T6 denormal check 25.0 % of real time against 25 %, T12 27.4 % against 15 % and 15.4 % against 10 %) |

REAPER and Ableton Live: the DAW QA of 1.0.0 (`audiotools-private/qa/daw-devicenti-2026-10-25/REPORT.md`); its fixes
(tempo change inside a block, doubled MIDI out, Song mode saved, host Swing, arp first step) are in 1.0.1, covered by
devicentitest and `--midi-test`. Not run on 1.0.1: auval and the DAWs (they need the plugin installed into `~/Library`).

## Pitfalls

- **An unprimed ADAA state made a spike at every note start through the folder** (the antiderivative of the fold
  is not 0 at 0). The first sample after a reset takes f(x) and primes the state.
- **System headers included inside a namespace** compiled on Apple Silicon and broke MSVC and GCC (the SIMD
  header): keep `<arm_neon.h>` and `<emmintrin.h>` at the top of `Base.h`, outside `dh::dv`.
- **Culling a mono voice changed the music.** A legato voice still in its tail is what the next note reuses (its
  phase and its glide); poly voices are culled under -90 dB, mono and legato voices keep their tail.
- **Rendering voices in groups changed which voice got which analog spread.** The filter spread follows a logical
  slot (the old one-by-one allocation order), not the physical lane.
- **Anything decided per block breaks block-size determinism.** Control-rate work, coefficient refreshes, tape wow
  and the silence bypass all run on the absolute 16-sample timeline.
