# AT303 user manual

## What AT303 is

AT303 is a bass line synthesizer with its own step sequencer: one oscillator (saw or square), a four-pole diode ladder filter, one envelope for the filter and one for the volume, accent and slide. It is the circuit behind acid: a resonant filter that squelches, accents that get louder and brighter the more of them you play in a row, and notes that glide into each other.

The voice is modelled from the circuit, not sampled. The filter is a diode ladder with its real capacitor values, so it loses bass as the resonance rises and rings near 500 Hz with the cutoff at half. The accent is the real sweep circuit: every accent charges a capacitor that only drains slowly, so a run of accents climbs. The slide is the original's RC glide.

The sequencer works as the original did, with two separate lists: the pitches, in order, and the rhythm, step by step. Each note step plays the next pitch in the list. Change the rhythm and the melody moves along with it; that is where many acid lines come from. On top of that, AT303 adds what the original lacks: a pattern editor you can see, an acid line generator, a MIDI mode, sync to the host, the classic circuit modifications on one page, distortion and a delay.

![AT303 with Squelch Line: the instrument on top, with the step lights, the display and the accent charge bar live; below, the pattern banks, the two lists of the pattern and the Generator page.](img/overview.jpg)

## Installing

Download the package for your system from the website and unzip it.

On macOS:

1. Copy `AT303.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `AT303.component` (Audio Unit) to `~/Library/Audio/Plug-Ins/Components/`.
2. Rescan plugins in your DAW.
3. `AT303.app` is the standalone version and `at303render` the command-line renderer.

The files are not notarized. If macOS refuses to open them, run `xattr -dr com.apple.quarantine` followed by the path of each file.

On Windows:

1. Copy `AT303.vst3` to `C:\Program Files\Common Files\VST3\`.
2. Rescan plugins in your DAW.
3. `AT303.exe` is the standalone version and `at303render.exe` the command-line renderer.

**Ableton Live.** Factory presets appear in Live's browser under Plug-Ins → Audio Tools AT303 (expand the plugin), installed by the launcher or from the zip's Presets folder. Live 12 doesn't show a plugin's own program list, so this is the way to browse them from Live; the preset menu inside AT303 always has them all.

## First sounds in one minute

1. Put AT303 on an instrument track and press play in your DAW. The sequencer follows the host: it starts and stops with the transport, at the host's tempo.
2. Open the preset menu and pick **Squelch Line**.
3. Turn **Cutoff** slowly up and down while it plays. Then **Resonance**. Then **Env Mod**. These three are most of the sound.
4. Pick **Accent Climb** and watch the **Accent charge** bar: four accents in a row, and the fourth one screams.
5. Go to the **Generator** page and press **Generate** a few times. Each press is a new line in the chosen key and scale.

In the standalone app, or with **Follow host** off, press **Run** on the instrument to start the internal clock; **Tempo** sets its speed.

Every control explains itself in a tooltip when you rest the pointer on it.

## The interface at a glance

- **The instrument.** The front panel. The knobs and the waveform switch work as on hardware; the 16 pads show the pattern and the step that is playing; the keys play notes and write pitches; the buttons write patterns the original way.
- **Preset bar.** Presets, **Manual** and **Inspect**.
- **Patterns.** Four groups (I to IV), two banks (A and B) and eight patterns per bank: 64 patterns, saved with your project. **MIDI mode**, **Follow host**, **Triplets** and the **Length** of the pattern.
- **The pattern editor.** The two lists of the pattern, and which pitch each step plays.
- **Pages.** **Generator**, **Extended** (the circuit modifications) and **Output** (distortion, delay, transpose and quality).

## The voice

- **Cutoff.** Where the filter sits. With the resonance up, at half it rings near 500 Hz, as the factory calibrated it.
- **Resonance.** The squelch. The ladder loses low end as it rises, and it makes the accent sweep stronger: on the original the accent capacitor drains through this knob.
- **Env Mod.** How far the envelope sweeps the filter. As on the original, turning it up also lowers where the sweep starts, so the sound gets both deeper and more vocal. It never quite reaches zero; the **Env to 0** modification lets it.
- **Decay.** How long the filter sweep lasts on normal notes, from about 200 ms to 2.5 s. Accented notes always use the shortest decay; **Accent decay** on the Extended page changes that.
- **Accent.** How much louder and brighter accented notes are, and how hard they charge the sweep capacitor.
- **Waveform.** Saw or square. The square is made from the saw by a transistor shaper, as on the original, so its corners are soft.
- **Tuning.** ±7 semitones. **Tempo** is the internal clock. **Volume** is the output level.

### Accent and slide

**Accent** makes a note louder, gives it a short, sharp filter sweep and charges the sweep capacitor. The capacitor drains slowly, so accents close together climb: the second is brighter than the first, the fourth brighter still. The **Accent charge** bar shows its voltage live.

**Slide** ties a note to the next one: the gate stays open, the envelopes do not restart and the pitch glides into the next note in about 60 ms. A slide into a rest does nothing.

## The sequencer

A pattern holds two lists, as on the original:

- **Time**: for each step, a **note** (N), a **tie** (T, holds the note before) or a **rest** (-).
- **Pitch**: up to 16 pitches in order, each with its note (C to the C above), octave **up** or **down**, **accent** and **slide**.

Each N step takes the next pitch from the list. The lines in the editor show which pitch each step plays. Turn a rest into a note and every note after it moves one pitch along: the same melody, a new rhythm, a different line.

![The pattern editor with Squelch Line: the Time row on top, the lines to the pitches each step plays, and the pitch list with its octave, accent and slide flags.](img/editor.jpg)

### Editing on screen

- **Time row**: click a step to cycle note, tie, rest.
- **Pitch**: drag up or down to change the note.
- **Up / Down, Accent, Slide**: click to switch.
- **Length**: the - and + buttons, from 1 to 16 steps (15 in triplets).
- **Triplets**: three steps per beat instead of four.

On the instrument, click a pad to cycle its step through note, tie and rest.

### Writing the original way

- **Pitch** mode: press **Pitch**, then play the keys. Each key writes the next pitch in the list; press **Up** or **Down** before a key for the octave. **Accent** and **Slide** switch the flags of the last pitch you wrote. The display shows the place in the list.
- **Time** mode: press **Time**, then **Down** for a note, **Up** for a tie and **Accent** for a rest, one step at a time. The display shows the step.
- Press the same button again to leave the mode. **Clear** empties the current pattern.

### Clock and MIDI

- **Follow host** on: the sequencer runs with the host's transport and tempo, locked to its bars, so it is always in time after a jump or a loop.
- **Follow host** off: **Run** and **Tempo** on the instrument.
- **MIDI mode**: the sequencer stops and AT303 plays from MIDI. A velocity at or above **Accent vel.** (100 by default) is an accent; notes that overlap slide, as on the original's keyboard. The keys of the instrument always play notes when not writing.

## The Generator

The Generator writes an acid line into the current pattern: **Root**, **Scale** (minor, Phrygian, Dorian, minor pentatonic, harmonic minor, Mixolydian or chromatic), **Octaves**, and how much **Density**, **Accents**, **Slides** and **Ties** the line has.

- **Generate** writes a whole new pattern. Each press is a new one.
- **Mutate** keeps the rhythm and redraws the pitches, accents and slides.

If you like a line, it is already in the pattern: move to another pattern before generating again.

## Extended: the circuit modifications

The page has the classic modifications people make to the hardware. **Stock 303** puts all of them back to the original circuit.

- **Overdrive.** More level into the filter: the ladder distorts before anything else.
- **Normal decay.** A fixed decay in milliseconds instead of the Decay knob (Knob = use the knob).
- **Accent decay.** The decay of accented notes, 200 ms on the original.
- **Soft attack.** The attack of normal notes (3 ms on the original; accents are always sharp).
- **Slide time.** How long the glide takes.
- **Tracking.** The filter follows the keys, so high notes stay bright. The original has none.
- **Filter FM.** The output modulates the filter: growl and buzz.
- **Env range.** Up to three times the envelope sweep.
- **Cutoff top.** The top of the Cutoff knob, from 2.4 kHz up to 5 kHz.
- **VCA click.** The thump at the start of each note.
- **Sweep.** The speed of the accent sweep capacitor: fast, normal or slow.
- **Muffler.** A soft clip after the VCA that rounds the attacks.
- **High reso.** Twice the resonance: the filter oscillates on its own.
- **Env to 0.** Env Mod reaches zero.
- **Full bass.** The output coupling capacitor that cuts the bass at 47 Hz moves down to 4.7 Hz.
- **4 ms delay.** The original waits 4 ms before the attack of normal notes; turn it off for tighter timing.

![The Extended page: the circuit modifications, with Stock 303 to go back to the original.](img/extended.jpg)

## Output

- **Drive** (off, soft clip, hard clip or fuzz), **Amount** and **Tone**: the usual pedals after an acid box.
- **Delay**, **Time** (1/16 to 1/2, with dotted values), **Feedback** and **Delay tone**: a ping-pong delay synced to the tempo.
- **Transpose**: ±12 semitones for the whole pattern.
- **Accent vel.**: the MIDI velocity from which a note is an accent.
- **Quality**: the oversampling of the voice, 2x (Eco), 4x (Normal) or 8x (High).

## Inspect

**Inspect** fades the instrument into its circuit, live: the oscillator's control voltage, the four capacitors of the diode ladder with their charge, the VCA, the two envelopes as they move, and C13, the accent capacitor, filling with every accent. It is the same model you hear, drawn while it plays.

![Inspect with Accent Climb: the ladder's capacitors, the VCA, the two envelopes and the accent capacitor C13 charging.](img/inspect.jpg)

## Presets

Thirty-four presets in six groups, each with its own pattern:

- **Classic.** First Squelch, Rubber Root, Sliding Fifths, Square Pulse, Accent Climb, Low Decay Chug, Long Sweep, Minor Walk.
- **Acid.** Squelch Line, Night Bus, Wet Concrete, Glass Eel, Twelve O'Clock, Pressure Drop, Bubble Square, Off The Grid.
- **Bass.** Deep Root, Dub Pulse, Sub Ladder, Funk Thumb, Tied Drone, Pocket.
- **Driven.** Hot Box, Broken Speaker, Fuzz Worm, Overdriven Ladder.
- **Triplets.** Triplet Roll, Shuffle Gate, Six Against Four.
- **Extended.** Long Accent, Screamer, Soft Attack, Tracking Lead, Muffled Box.

Loading a preset writes its pattern into the pattern that is selected. All the lines are original.

## The command-line renderer

`at303render` plays a preset's pattern through the whole instrument into a file:

```
at303render out.wav --preset "Squelch Line"
at303render out.wav --preset "Wet Concrete" --seconds 30 --bpm 128 --at 8 cutoff=80 --at 16 cutoff=30
at303render out.wav --set waveform=1 --set resonance=90
```

- `--preset` loads a preset by name. Without it, the defaults.
- `--seconds` is the length (16 by default) and `--sr` the sample rate (48000).
- `--bpm` is the tempo (the preset's by default).
- `--set id=value` sets a parameter before starting; `--at seconds id=value` changes it at that moment.

## Getting the best from it

- **Ride the knobs.** Acid lines are performances: record Cutoff, Resonance and Env Mod as automation while the pattern loops.
- **Accents in runs** climb; spread out, they all sound the same. Move them around before changing anything else.
- **Change the rhythm, not the notes.** Turn a tie into a note or a rest into a note and listen to the melody shift.
- **Decay short and Env Mod high** for plucks; **Decay long and Cutoff low** for slow, vocal sweeps.
- **For a deep bass under a track**, turn on Full bass and keep the resonance moderate.

## Limits

- The original's tap-write and pattern chaining are not implemented; use your host to change patterns (the Pattern parameter can be automated).
- In MIDI mode the internal sequencer stops; it does not transpose from incoming notes.
- Values on the original vary from unit to unit; AT303 follows the service-notes calibration.

## Technical notes

- **Filter.** A diode ladder with its capacitor ratio, solved with zero-delay feedback (each nonlinear stage linearised around its state and the whole loop solved every sample), with the feedback high-pass of the original, oversampled 4x by default.
- **Envelopes and accent.** The filter envelope, the volume envelope and the accent sweep circuit are modelled as their RC networks, including the diode and resistors that charge and drain C13.
- **Slide.** The RC glide of the control voltage, about 60 ms to arrive.
- **Oscillator.** A band-limited ramp; the square comes from the ramp through a transistor waveshaper model.
- **Sequencer.** 24 clocks per quarter note, 6 per step (8 in triplets), locked to the host position.
- **CPU.** About 0.9 % of one core on an Apple M4 with the default sound at 4x; 1.7 % at 8x.
- **Tested** with an automated suite (filter ring frequency, accent climb, slide time, determinism, no NaN, bounded output, CPU), pluginval and auval.
