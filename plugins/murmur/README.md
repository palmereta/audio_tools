# Murmur

A real-time granular effect. Everything that comes in is recorded into a memory of up to 16 seconds,
and up to 128 grains read it at once, each with its own length, pitch, direction, envelope and place in
the stereo field. VST3, Audio Unit (macOS) and standalone. Version 1.0.2.

The window draws the memory as a horizon and each grain as a bird: its place is the moment it reads, its
height is its pitch, its size follows its envelope, and reverse grains fly the other way.

## What it does

- **Grains.** 5 ms to 2 s long, 0.5 to 200 per second (or on the host grid from 1 bar to 1/32, with
  triplets, dotted notes and a chance to skip), up to 128 at once, cubic interpolation. The grain gain
  is compensated for overlap, so the cloud keeps its level at any density.
- **Time.** Buffer of 0.5 to 16 s, Position and Spray (what Spray asks for beyond the newest audio is
  reflected back instead of clipped, so grains don't pile up at the record head), Scan from −2 to +2 ×
  and Freeze from the button or the sustain pedal (CC 64).
- **Pitch.** ±24 semitones, fine tune, a random spread free or snapped to semitones, octaves, fifths,
  a major or minor chord, pentatonic or whole tone, plus shimmer (octave up) and sub (octave down)
  probabilities and reverse grains.
- **Envelope and stereo.** A taper from a soft bell to a hard gate, skewed toward an early or late peak,
  random pan and width.
- **After the grains.** Low and high cut, feedback into the memory (DC blocker and soft clipper in the
  loop), ducking under the input, a tempo-synced stereo delay and a plate reverb, dry/wet mix.
- **Movement.** Two LFOs (sine, triangle, ramp, drift, step), free or synced, on position, spray, size,
  density, pitch, pan, tone, mix or feedback.
- **MIDI.** Pitch mode: the held notes set the grains' pitch (relative to C4). Gate mode: the cloud only
  plays while keys are held. The sustain pedal holds Freeze in every mode.
- **Presets.** 30 factory presets in eight categories (Clouds, Shimmer, Stretch, Rhythm, Pitch,
  Texture, Glitch, MIDI) and user presets in `Audio Tools/Murmur/Presets`.
- **Manual.** The Manual button opens the full manual (English and Spanish, with screenshots) inside
  the plugin; the same text is on the website.

## Layout

```
Source/dsp/MurmurEngine.h   the engine: buffer, grain scheduler, voices, filters, feedback, LFOs, FX, view data
Source/ParamsMm.h           host parameters, their help text and their mapping to the engine
Source/PluginProcessor.*    buses, MIDI (notes, CC 64), bypass, presets, state, demo input for the standalone
Source/ui/                  flock view, Simple and Advanced pages, widgets, palette
docs/manual/                the manual (en.md, es.md), its screenshots and shots.json
tools/dsptest.cpp           engine tests without JUCE
tools/render.cpp            murmurrender: a WAV through a preset, offline, with CPU figure
tools/presetsmm.py          factory presets
tools/blender/murmur.py     the 3D murmuration for the website: photos (--mode hero) and film (--mode film2), pc50-wsl
tools/blender/murmur_v1.py  the previous scene, unchanged (murmur.py --mode film / hero-v1 run it), kept to compare
tools/web_images.py         the photos to web/assets/img/murmur/ (hero, og-image, hl-flock, menu thumbnail)
tools/film.json             the film's shots and audio for scripts/film.py
```

## Tools

```bash
# Engine tests (level, pitch, freeze, freeze before the buffer is full, feedback, sync, MIDI gate,
# sample rates and blocks, 60 random extreme settings, CPU with 128 grains)
clang++ -std=c++20 -O2 -I plugins/murmur/Source/dsp -I shared/dsp plugins/murmur/tools/dsptest.cpp -o /tmp/mm_dsptest && /tmp/mm_dsptest

# Offline render
build/plugins/murmur/murmurrender_artefacts/Release/murmurrender plugins/murmur/assets/presets/p02_slow_fog.json in.wav out.wav \
    [--secs 30] [--tail 6] [--bpm 120] [--notes "57+60+64:5,53+57+60:5"] [--freeze-at 4]

# Factory presets
python3 plugins/murmur/tools/presetsmm.py
```

The editor saves a snapshot without a screen grab:
`MURMUR_SNAPSHOT=/tmp/x.png MURMUR_PAGE=0|1 MURMUR_PRESET=n MURMUR_WAIT=s [MURMUR_MANUAL=1] [MURMUR_FREEZE_AT=s]`,
with `MURMUR_DEMO_INPUT=file.wav` as a looped input for the standalone. `scripts/manual-shots.py murmur`
takes the manual's screenshots that way.

## Measurements

| Check | Result |
|---|---|
| Pitch +12, no randomness, on 220 Hz | 431 Hz by zero crossings |
| Freeze with a silent input | keeps sounding; without Freeze the cloud dies out when the buffer empties |
| Freeze after 2 s of a 16 s buffer, Scan 0.5, 600 ms grains | no 100 ms window below 0.02 RMS in 12 s (before the fix, Scan wrapped over the empty part of the buffer) |
| Feedback at maximum with shimmer | bounded, and silent after the input stops |
| Sync 1/8 at 120 BPM | 16 onsets in 4 s |
| 60 random extreme settings | always finite, peak below 8 |
| pluginval strictness 8, auval (aumf Mrmr Atls) | pass (VST3 and AU on macOS, VST3 on Windows) |
| athosttest (programs, MIDI, rates, state, fuzz, parameters) | 0 FAIL on macOS and Windows; WARN only for user gain above 0 dB and for restored state not sample-identical (grains are random) |

CPU, percentage of one core at 48 kHz:

| Setting | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| First Murmur, the default preset | 0.33 % | 0.88 % |
| Granular Wash, the heaviest factory preset | 0.56 % | 1.50 % |
| 128 grains of 1 s at once, with delay and reverb (dsptest 9) | 2.55 % | 6.22 % |

## Traps

- **Freeze before the buffer is full.** The frozen region is what was recorded, not the whole buffer:
  wrapping Scan over `bufferSec` read silence for most of the cycle. Grains moving forward also must not
  cross the frozen record head (behind it is the oldest audio), so their minimum distance is their whole
  span when frozen, not span minus their length as when recording.
- **Spray beyond the newest audio.** Clipping it piled half the grains at the record head; it is
  reflected instead.
