# Orrery

A MIDI generator for ambient, dark ambient, drone and calm music. Eight lanes orbit a slow shared
harmony; each lane plays one role (drone, pad, melody, arpeggio, cloud, bass or pulse) on its own MIDI
channel, so one Orrery plays a whole rack of software or hardware synthesizers. VST3, Audio Unit
(macOS) and standalone. Version 1.0.0.

Orrery makes no sound by itself: it plays other instruments. The built-in preview sound (Outputs page)
exists only to try it without routing anything.

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
  (IAC Driver, loopMIDI, a hardware interface), with MIDI clock out. In Ableton Live, which merges
  channels when routing between tracks, either use a virtual port or put one Orrery per track with Link
  set to Lead on one and Follow on the others: the followers play over the leader's chords.
- **Capture.** Saves the last minutes played as a MIDI file, one channel per lane.
- **Moods.** 21 factory presets, three per mood: Ambient, Dark Ambient, Drone, Sleep, Meditation, Focus
  and Celestial. Simple mode has the moods and five macros (density, motion, space, brightness,
  tension); Advanced mode has every parameter of every lane.

## Layout

```
Source/gen/Theory.h       scales, chords, voice leading, Markov walk, deterministic math (dm::), RNG
Source/gen/Generator.h    the engine: harmony, lanes, roles, loops, echoes, sessions, controls, Link
Source/gen/Preview.h      the built-in preview sound (one voice per role, plate reverb, delay)
Source/ParamsOr.h         host parameters (globals + l1..l8) and their mapping to the engine
Source/PluginProcessor.*  host MIDI, direct ports (own thread), Link hub, clock out, capture, presets
Source/ui/                orbit view, pages (Simple, Lanes, Harmony & session, Outputs), widgets, help
tools/gentest.cpp         engine tests without JUCE (scale, hanging notes, traffic, loops, sessions,
                          breathing, Link, determinism)
tools/render.cpp          orreryrender: offline MIDI + preview audio of a preset, CPU figures
tools/miditest.cpp        direct MIDI output through a virtual port (macOS)
tools/presetsor.py        factory presets
tools/blender/orrery.py   the 3D orrery for the website (pc50-wsl, CUDA)
```

## Tools

```bash
# Engine tests (also on Windows: cl /std:c++17 /O2 /fp:precise /EHsc ...)
clang++ -std=c++17 -O2 -ffp-contract=off -I plugins/orrery/Source/gen plugins/orrery/tools/gentest.cpp -o /tmp/orr_gentest && /tmp/orr_gentest

# Offline render: outdir/all.mid (one channel per lane), laneN.mid, preview.wav, CPU figure
build/plugins/orrery/orreryrender_artefacts/Release/orreryrender plugins/orrery/assets/presets/p01_first_light.json out --secs 120
orreryrender --bench      # generator alone, 8 busy lanes, 10 minutes

# Direct MIDI ports (macOS): creates a virtual port, plays through it, counts notes and clock
build/plugins/orrery/orrerymiditest_artefacts/Release/orrerymiditest

# Factory presets
python3 plugins/orrery/tools/presetsor.py
```

The editor can save a snapshot without a screen grab:
`ORRERY_SNAPSHOT=/tmp/x.png ORRERY_PAGE=0..3 ORRERY_PRESET=n ORRERY_RUN=1 ORRERY_WAIT=seconds Orrery.app/Contents/MacOS/Orrery`.

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

A variation number is only useful if it means the same music everywhere. Four things had to hold:

| Source of drift | Fix |
|---|---|
| Time summed block by block (rounding depends on the host buffer) | beats counted from an anchor in samples, moved only on tempo changes |
| Chord changes and fades evaluated once per block | lanes and chord changes run in time order; fades and breathing are evaluated at each note's own time |
| Controllers drawing from the notes' random stream once per block | controllers on a fixed 30 Hz grid of session time, with their own random stream |
| `std::log`, `std::sin`, `std::exp2` differ by platform; `/fp:fast` reorders; C++ leaves argument evaluation order unspecified (clang and MSVC disagree) | own math in `dm::` (only + − × ÷, `frexp`, `ldexp`), `/fp:precise` on MSVC and `-ffp-contract=off` on clang, one random draw per statement |

Check: `gentest` test 8 prints a fingerprint of three minutes of seven lanes (notes, controllers and
their sample positions). macOS arm64, macOS x86_64 and Windows x64 (MSVC `/fp:precise`) print
`60e8a62919b73c14`; MSVC with `/fp:fast` prints another one. The same test plays the same 10,194 messages
with buffers of 64, 333, 441, 512 and 1024 samples at 44.1, 48 and 96 kHz.

## Measurements

| Check | Result |
|---|---|
| Ten minutes of seven lanes at tension 0 | no note outside the scale, no hanging note at stop |
| MIDI traffic, all 21 presets, 5 minutes each | at most 37 messages per second on one channel (Breath Six); a 5-pin MIDI cable carries about 1000 |
| Loop of 17 beats | 84 of 84 notes repeat 17 s later |
| One-minute session | last note at 59.5 s, CC7 at 0 at the end |
| Gentle mode | velocity at most 92, highest note 86 |
| Link follower | 56 notes, none outside the leader's chord |
| Direct MIDI port (orrerymiditest) | 36 of 36 notes and 744 of ~744 clock ticks received |
| pluginval strictness 8, auval | pass (VST3 and AU on macOS, VST3 on Windows) |

CPU, percentage of one core at 48 kHz:

| Setting | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| Generator alone, 8 busy lanes (`orreryrender --bench`) | 0.003 % | 0.009 % |
| With the preview sound, Orbiting (the busiest preset) | 1.6 % | 3.8 % |
