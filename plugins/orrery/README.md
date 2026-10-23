# Orrery

A MIDI generator for ambient, dark ambient, drone and calm music. Eight lanes orbit a slow shared
harmony; each lane plays one role (drone, pad, melody, arpeggio, cloud, bass or pulse) on its own MIDI
channel, so one Orrery plays a whole rack of software or hardware synthesizers. VST3, Audio Unit
(macOS), LV2 (Linux) and standalone. Version 1.0.7.

Orrery makes no sound by itself: it plays other instruments. The built-in preview sound exists only to try
it without routing anything; its On/Off button sits next to Play, and the Simple page says "You're hearing
Orrery's preview sound" while it plays.

## What it does

- **Harmony.** A slow chord walk shared by every lane: 16 scales, chord colour from triads to elevenths,
  tension (sus chords, clusters, tritones), voice leading between chords, and three progression modes
  (walk, drone, two chords). With tension below 0.5 the walk avoids diminished chords.
- **Lanes.** Role, MIDI channel, register, density, note length, velocity and its randomness,
  probability, Euclidean rhythm, arpeggio pattern, MIDI echoes, a controller curve (sine, drift, breath,
  swell) and pitch-bend drift for analog synths.
- **Loops of different lengths.** A lane can record its first cycle and repeat it, mutating slowly. With
  prime lengths (17, 19, 23, 29 beats) the parts drift in and out of phase for hours, as Eno's tape loops
  did on Music for Airports. The orbit view shows each lane turning at its own loop period.
- **Sessions.** A length with slow fade in and out (velocity and, optionally, CC7), a breathing pace in
  breaths per minute (chord changes on whole breaths, melodic phrases on the exhale) and a gentle mode
  that caps velocity, register and density for sleep.
- **Variations.** The variation number seeds every random choice. The same number plays the same notes
  on macOS and Windows, at any buffer size and sample rate (see Determinism below).
- **Outputs.** Every lane goes to the host on its own channel and, optionally, to a direct MIDI port
  (IAC Driver, loopMIDI, a hardware interface), with MIDI clock out. On macOS and Linux each lane can also
  be published as a virtual MIDI port of its own ("Orrery Drone", "Orrery Pad"...).
- **Main and satellites (1.0.5).** Ableton Live merges channels when routing between tracks. With the
  default role, Auto, the first Orrery in the host becomes the Main and every Orrery added later becomes a
  satellite that plays one lane of the Main (the next one free), on its own track, so each synth takes its
  MIDI from one satellite. A satellite runs its own replica of the Main's generator and outputs exactly
  the Main's messages for its lane. Projects from 1.0.4 open as Independent, as before; Link (Lead and
  Follow) still works between Independent Orrerys.
- **Capture.** Saves the last minutes played as a MIDI file, one channel per lane.
- **Moods.** 21 factory presets, three per mood: Ambient, Dark Ambient, Drone, Sleep, Meditation, Focus
  and Celestial. Simple mode has the moods and five macros (density, motion, space, brightness,
  tension); Advanced mode has every parameter of every lane.

## Layout

```
Source/gen/Theory.h       scales, chords, voice leading, Markov walk, deterministic math (dm::), RNG
Source/gen/Generator.h    the engine: harmony, lanes, roles, loops, echoes, sessions, controls, Link
Source/gen/Constellation.h  Main and satellites: the registry, what a Main publishes, the satellite replica
Source/gen/Preview.h      the built-in preview sound (one voice per role, plate reverb, delay)
Source/ParamsOr.h         host parameters (globals + l1..l8) and their mapping to the engine
Source/PluginProcessor.*  host MIDI, roles, direct and virtual ports (own thread), Link hub, clock out, capture, presets
Source/ui/                orbit view, pages (Simple, Lanes, Harmony & session, Outputs, Satellite), widgets, help
tools/gentest.cpp         engine tests without JUCE (scale, hanging notes, traffic, loops, sessions,
                          breathing, Link, determinism; 9-15: Main and satellites)
tools/render.cpp          orreryrender: offline MIDI + preview audio of a preset, CPU figures
tools/miditest.cpp        direct MIDI output through a virtual port, and lanes as virtual ports (macOS)
tools/presetsor.py        factory presets
tools/blender/orrery.py   the 3D orrery for the website (pc50-wsl, CUDA)
```

## Tools

```bash
# Engine tests (also on Windows: cl /std:c++20 /O2 /fp:precise /EHsc ...; on Linux add -pthread)
clang++ -std=c++20 -O2 -ffp-contract=off -I plugins/orrery/Source/gen plugins/orrery/tools/gentest.cpp -o /tmp/orr_gentest && /tmp/orr_gentest

# Main and satellites through the real VST3 (several instances in one process), and the virtual ports
athosttest build/plugins/orrery/Orrery_artefacts/Release/VST3/Orrery.vst3 --tests roles,ports

# Offline render: outdir/all.mid (one channel per lane), laneN.mid, preview.wav, CPU figure
build/plugins/orrery/orreryrender_artefacts/Release/orreryrender plugins/orrery/assets/presets/p01_first_light.json out --secs 120
orreryrender --bench      # generator alone, 8 busy lanes, 10 minutes

# Direct MIDI ports (macOS): creates a virtual port, plays through it, counts notes and clock; then publishes
# the lanes as virtual ports and checks that every lane's notes arrive on its own port
build/plugins/orrery/orrerymiditest_artefacts/Release/orrerymiditest

# Factory presets
python3 plugins/orrery/tools/presetsor.py
```

The editor can save a snapshot without a screen grab:
`ORRERY_SNAPSHOT=/tmp/x.png ORRERY_PAGE=0..3 ORRERY_PRESET=n ORRERY_RUN=1 ORRERY_WAIT=seconds Orrery.app/Contents/MacOS/Orrery`.
`ORRERY_ROLE=0..3` sets the role; `ORRERY_SAT_DEMO=1` makes the standalone a satellite of a Main it creates in
the same process (`ORRERY_SAT_LANES=mask`), and `=2` a Main with two satellites. After the snapshot the role
goes back to Auto, so the standalone does not keep it.

The website demos are recorded the same way. `ORRERY_RECORD=dir` saves one JPEG per 1/30 s of session
time (not wall-clock time, so a late frame never shifts the picture against the sound), and
`ORRERY_DEMO=1` applies the clip settings of `scripts/samples.py` (fade-in 3 s, no session, no CC7) with
the preview sound off. Because the generator is deterministic, the video lines up with the clip's audio:

```bash
ORRERY_SNAPSHOT=/tmp/x.png ORRERY_WAIT=600 ORRERY_PAGE=0 ORRERY_PRESET=1 ORRERY_RUN=1 ORRERY_DEMO=1 \
ORRERY_RECORD=/tmp/demo ORRERY_RECORD_SECS=25.2 ORRERY_RECORD_SCALE=1.4545 Orrery.app/Contents/MacOS/Orrery
# then: fill missing frame numbers with the previous one, and mux 1920x1192 at 30 fps with
# web/assets/samples/orrery/<clip>.m4a into web/assets/video/orrery-demo-<clip>.mp4
```

## Determinism

A variation number is only useful if it means the same music everywhere. Five things had to hold:

| Source of drift | Fix |
|---|---|
| Time summed block by block (rounding depends on the host buffer) | beats counted from an anchor in samples, moved only on tempo changes |
| Chord changes and fades evaluated once per block | lanes and chord changes run in time order; fades and breathing are evaluated at each note's own time |
| Controllers drawing from the notes' random stream once per block | controllers on a fixed 30 Hz grid of session time, with their own random stream |
| With the host clock, what played before the transport started (free-running with the host stopped: wall-clock time) and where the first block fell (pre-roll, buffer size) | starting the transport is Play and starts a new session: generator reseeded, music anchored at the song position on a 1/16-beat grid (a pre-roll waits for beat 0), session clock and controller grid counted from that point; the fade-in only when the start is the song's beginning (DAW QA 2026-10-22, REAPER H2) |
| `std::log`, `std::sin`, `std::exp2` differ by platform; `/fp:fast` reorders; C++ leaves argument evaluation order unspecified (clang and MSVC disagree) | own math in `dm::` (only + − × ÷, `frexp`, `ldexp`), `/fp:precise` on MSVC and `-ffp-contract=off` on clang, one random draw per statement |

Check: `gentest` test 8 prints a fingerprint of three minutes of seven lanes (notes, controllers and
their sample positions). macOS arm64, macOS x86_64, Windows x64 (MSVC `/fp:precise`) and Linux x86_64
(GCC 11) print `60e8a62919b73c14`; MSVC with `/fp:fast` prints another one. The same test plays the same 10,194 messages
with buffers of 64, 333, 441, 512 and 1024 samples at 44.1, 48 and 96 kHz. Test 17 does the REAPER case with the host
clock: 0 to 41 s of free-running with the host stopped, pre-rolls of 3 blocks and 12,345 samples, buffers 64/333/512/768/1024:
8,830 of 8,830 messages identical (±1 sample) over 2 minutes. athosttest `roles` 6 does it with the real plugin (547/547).

## Main and satellites: the time base

A satellite does not receive the Main's notes: it runs its own `Generator` with the Main's full musical state
and outputs only its lanes. The generator is deterministic, so the replica generates what the Main generates,
at the same sample, as long as both get the same inputs in the same blocks (`Source/gen/Constellation.h`):

- **Transport**: each instance reads its own playhead; in one host cycle every track sees the same one.
- **Controls** (settings, Play, a new preset, panic): the Main applies them one block after reading them and
  publishes them for that block. When a satellite processes cycle k, the Main has finished cycle k-1
  whatever the processing order or thread, so the controls for cycle k are always there.
- **Common clock**: every instance counts its blocks and samples. To join, a satellite copies the Main's
  generator at a block boundary (`copyStateFrom`) and decides once whether the Main has already processed
  the current cycle, comparing per-cycle stamps: the host's sample position while the transport runs, else
  the host's per-cycle system time (VST3 `systemTime`, AU host time), else the wall clock. From then on both
  advance in lockstep.
- **Proof at run time**: the Main publishes an FNV hash of each block's output; a satellite compares its
  replica's. A replica one block off generates the same events one block late, which the hash cannot see, so
  the stamps are compared too (sample positions must match; system times must agree on average). On any
  mismatch, or if the Main stops being processed, the satellite releases its notes, copies the Main again
  and re-attacks the notes of its lanes that are sounding.

**Tracks out of step** (REAPER's anticipative FX, above all in offline renders: each track on its own thread,
several blocks ahead of or behind the others; DAW QA H1, where a satellite resynced every block and flooded
zero-length notes). With the transport running a satellite places itself by the host's sample position, not by the
processing order: it maps its block to the Main's sample clock from the two positions, and either keeps a copy taken
at a boundary it has not reached yet until it gets there, or, when it is ahead, catches up from an older copy with
the inputs it recorded for its own past blocks. While ahead of the Main it predicts that the controls stay the same
and confirms them when the Main publishes them (a mismatch resyncs). In real time a Main that has not processed for
3 blocks and 60 ms is stalled (bypass), not late. After a loss the satellite waits longer each time (up to 0.5 s),
and a note attacked and released at the same sample is dropped. gentest 16: lead/lag up to 16 blocks at 64-1024,
three satellites, one joining late: identical, no zero-length or hanging notes; with settings changes, a preset,
a jump, Stop/Play and panic, 30 of 30 windows identical from 3 s after each change.

**Copies** (Duplicate Track in Live copies the state into a new instance while the original is open; QA O2): each
instance saves a random `instanceId`. A state whose id belongs to another open Orrery is a copy: it gets a new id
and resolves Auto again (satellite on the next free lane). A project that is reopened keeps its roles; states
without an id (1.0.5 and earlier) keep theirs too. athosttest `roles` 7.

Known limit: with the host transport stopped, no per-cycle host time and a host that always processes a
satellite's track more than half a block after the Main's, the wall clock cannot tell "late in this cycle"
from "early in the next", and the satellite settles one block off (gentest 14 checks it stays stable).
Starting the transport makes it exact.

| Check (gentest 9-18, athosttest roles) | Result |
|---|---|
| Generator copied mid-run | 1,924 of 1,924 messages identical over 83 s |
| Auto | first instance Main, next ones lanes 1, 2, 3; a freed lane is taken again; lanes that are off are skipped |
| Satellites vs Main, one thread, random order per cycle, blocks 64/333/512/1024, host transport or own clock | every message identical (notes, velocities, sample positions, CC, bend), no resync |
| Same with each instance on its own thread | identical |
| Wall-clock stamps with heavy jitter (up to one block) | corrects itself while the Main is stopped; identical |
| Satellite joining mid-play, satellite changing lane | identical from the next block; no hanging notes |
| Main not processed for 3 s | satellites release, wait and resync; identical after |
| Real VST3, 4 instances, blocks 64/512/1024, host clock, own clock, threads | 230/230, 238/238, 488/488 messages (host clock) |
| Lanes as virtual ports | orrerymiditest (macOS): 33 of 33 notes on the right ports; athosttest ports (macOS, Linux with ALSA sequencer): two instances get distinct names, ports gone on close |
| Same tests on Windows x64 and Linux x86_64 | gentest 15/15 and athosttest roles identical to macOS |

## Measurements

| Check | Result |
|---|---|
| Ten minutes of seven lanes at tension 0 | no note outside the scale, no hanging note at stop |
| MIDI traffic, all 21 presets, 5 minutes each | at most 37 messages per second on one channel (Breath Six); a 5-pin MIDI cable carries about 1000 |
| Loop of 17 beats | 84 of 84 notes repeat 17 s later |
| One-minute session | last note at 59.5 s, CC7 at 0 at the end |
| Gentle mode | velocity at most 92, highest note 86 |
| Link follower | 56 notes, none outside the leader's chord |
| Satellites | identical to the Main's lane (see above) |
| Direct MIDI port (orrerymiditest) | 36 of 36 notes and 744 of ~744 clock ticks received |
| pluginval strictness 8, auval | pass (VST3 and AU on macOS, VST3 on Windows) |

CPU, percentage of one core at 48 kHz:

| Setting | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| Generator alone, 8 busy lanes (`orreryrender --bench`) | 0.003 % | 0.009 % |
| With the preview sound, Orbiting (the busiest preset) | 1.6 % | 3.8 % |
