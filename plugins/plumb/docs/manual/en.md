# Plumb user manual

## What Plumb is

Plumb corrects the pitch of a voice, or of any instrument that plays one note at a time, while it is being sung. It finds the note the singer is aiming at and moves the voice onto it, from a gentle nudge that keeps every scoop and vibrato to the hard, stepped effect where every note lands exactly on pitch.

It is built for latency first. In its fastest mode Plumb adds 32 samples, 0.67 ms at 48 kHz: less than the delay of the converters in most audio interfaces, so a singer can hear the corrected voice in their headphones or in-ear monitors without feeling it behind them. The default mode adds 2.7 ms, and the mixing mode 21 ms.

The everyday controls are few: a key, a scale, a voice range and three knobs. Everything else is on one Advanced page, and every control explains itself in a tooltip when you point at it.

The window is a slate in a brass frame. On the left hangs a plumb bob: it swings with the tuning error of what is sung and hangs still when the note is right. On the slate, the live note view draws the sung pitch in chalk and the corrected pitch in brass, over the lines of the notes of the scale.

![Plumb with Natural Lead in A minor: the plumb bob on the left, the live note view on the slate, the key and scale below, and the three main knobs.](img/overview.jpg)

## Installing

Download the package for your system from the website and unzip it. The Audio Tools launcher can also download and install it for you.

On macOS:

1. Copy `Plumb.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `Plumb.component` (Audio Unit) to `~/Library/Audio/Plug-Ins/Components/`.
2. Rescan plugins in your DAW. Plumb appears as **Audio Tools Plumb**.
3. `Plumb.app` is the standalone version and `plumbrender` the command-line renderer.

The files are not notarized. If macOS refuses to open them, run `xattr -dr com.apple.quarantine` followed by the path of each file.

On Windows:

1. Copy `Plumb.vst3` to `C:\Program Files\Common Files\VST3\`.
2. Rescan plugins in your DAW.
3. `Plumb.exe` is the standalone version and `plumbrender.exe` the command-line renderer.

## First sounds in one minute

1. Put Plumb on a vocal track. It is a mono effect: on a stereo track it corrects the sum and returns it on both sides.
2. Click the key of the song in the row of note buttons and pick the scale (Major or Minor for most songs). If you don't know the key, press **Listen**, sing or play the song for 15 to 30 seconds, and press **Apply** when Plumb proposes one.
3. Pick **Voice**: Low for bass and baritone, Mid for tenor and alto, High for soprano, Instrument for anything else.
4. The default preset, **Natural Lead**, already corrects. Sing, and watch the brass line settle on the note lines.
5. Turn **Style** to the right for a firmer correction; all the way for the hard-tune effect. Or pick **Hard Tune** from the presets.
6. Turn on **Compare** to hear the voice without correction, at the same level and the same latency; turn it off to hear Plumb again.

## The interface at a glance

- **Top bar.** The presets (arrows and menu), the three latency modes (Live, Tight, Studio) with the latency they add, **Manual** (this manual) and **Advanced**.
- **The plumb bob.** On the left. Below it, the sung note and the output note, each with its distance in cents from the nearest note.
- **The note view.** The last seconds of singing, scrolling to the left.
- **Key and scale.** The twelve note buttons, the scale list, the voice range list, **Listen** and **Apply**.
- **Three knobs.** Amount, Speed and Style.
- **Compare**, bottom left.

## Latency modes

Plumb hears the voice through a short look-ahead window, and that window is the only delay it adds. The three modes trade that delay for how much Plumb knows before it moves the voice.

| Mode | Added latency at 48 kHz | Use it for |
|---|---|---|
| **Live** | 32 samples, 0.67 ms | Monitoring: stage, headphones, in-ears |
| **Tight** | 128 samples, 2.7 ms | Recording and most mixing. The default |
| **Studio** | 1024 samples, 21 ms | Mixing, when the delay does not matter |

The latency is the same number of milliseconds at every sample rate (64, 256 and 2048 samples at 96 kHz). Plumb reports it to the host, so a DAW with delay compensation keeps the track in time.

What changes between modes is how the voice is cut and rejoined. Plumb moves the pitch by repeating or leaving out single cycles of the voice, and it does so at the quietest point of each cycle. With more look-ahead it can pick a better point, so Tight and Studio are smoother than Live on low voices and fast runs. All three are corrected by the same detector and the same rules.

Two delays are not audio latency and are worth knowing:

- **Note starts.** Plumb needs one or two periods of a new note to know its pitch: about 10 to 23 ms depending on the note (less for high notes). In Live and Tight the first moment of each note passes as it was sung; in practice that is the scoop into the note, which singers expect to hear. In Studio the window already covers it, so note starts are corrected too.
- **Splices.** Each time a cycle is repeated or left out, the voice moves later or earlier by up to one period (a few milliseconds). Plumb keeps that drift within a period and returns to the reported latency when the voice stops.

## Key, scale and voice

The twelve note buttons choose the **key**: the lit one is the tonic, and a dot marks every note of the scale.

The **scale** list has Chromatic (every semitone is a target), Major, Minor, Harmonic minor, Melodic minor, the modes (Dorian, Phrygian, Lydian, Mixolydian, Locrian), Major and Minor pentatonic, Blues, and **Custom**.

With **Custom**, each note button has three states; click to cycle them:

- **Snap**: the note is a target.
- **Skip**: the note is not a target; a voice near it is pulled to a neighbour.
- **Bypass**: no correction near this note at all. Useful for a blue note or a passing note you want left as sung.

**Listen** gathers 15 to 30 seconds of what is sung and proposes a key and a scale (major or minor). It never changes anything by itself: the proposal appears next to the button and **Apply** sets it. It only uses notes held for more than about a tenth of a second, so speech and fast runs don't confuse it.

**Voice** sets the range the detector listens to. The narrower the range, the faster and surer the detector is, so choose the one that fits:

| Voice | Range |
|---|---|
| Low | 65 to 520 Hz |
| Mid | 80 to 880 Hz |
| High | 175 to 1400 Hz |
| Instrument | 40 to 2000 Hz |

## Amount, Speed and Style

- **Amount** is how much of the distance to the target is corrected. 100 % lands on the note; 50 % goes half way.
- **Speed** is how fast the correction reaches the target, in milliseconds. 0 is instant, the hard-tune effect; 20 ms is quick but natural; 100 ms and more lets the singer's own movement through.
- **Style** goes from **Natural** to **Hard**. It sets the whole character of the correction at once: how notes glide into each other, how much vibrato and drift are kept, how wide the window of corrected notes is, and how far a note must go before the target changes. Natural keeps vibrato, scoops and bends and only moves the centre of each note; Hard puts every note exactly on its line.

At Style 100 % Plumb switches to hard tuning: the output is the target note itself, with no vibrato and instant steps when Speed is 0.

## The Advanced page

![The Advanced page: the character of the correction, pitch and synthetic vibrato, formants, MIDI and input, output.](img/advanced.jpg)

### Character

These are the values Style sets. Turn on **Custom** to set them by hand; Style then stops changing them.

- **Transition** (0 to 200 ms): how the correction glides from one target note to the next. Short is a step; long is a slide.
- **Humanize** (0 to 100 %): long notes are corrected more slowly than note starts, so a held note keeps its life while the start is on pitch.
- **Flex** (cents): only notes within this distance of the target are corrected. A bend or a swoop wider than that passes as sung. At Hard it opens to everything.
- **Hysteresis** (0 to 50 cents): how far past the midpoint between two notes the voice must go before the target changes. It keeps a note sung between two targets from flipping. At 0, with Style at 100 %, the target follows every movement of the voice, vibrato included: the deliberate warble effect of the **Warble** preset.
- **Vibrato** (0 to 100 %): how much of the singer's vibrato is kept.
- **Drift** (0 to 100 %): how much of the slow wander inside a note is kept.

### Pitch

- **Transpose** (±24 semitones) and **Fine** (±100 cents) move the target. With the formants preserved, an octave down is a second, deeper voice, not a slowed-down one.
- **Vibrato add**, **Rate** and **Delay** add a steady synthetic vibrato to every held note, starting after the delay and fading in. Useful on top of hard tuning.

### Formants

The formants are the resonances of the mouth and throat that make a voice sound like itself and make vowels recognisable. When a pitch shifter moves them with the pitch, a voice sounds like a chipmunk going up and like a giant going down.

- **Preserve** (on by default) keeps the formants where they are while the pitch moves.
- **Amount**: 100 % keeps them in place; lower values let them follow the pitch part of the way.
- **Throat** moves them on purpose: down for a longer vocal tract (darker, bigger), up for a shorter one (brighter, younger), without changing the pitch. It costs two to three times the CPU of plain correction.

With Preserve off, Plumb shifts the waveform itself: the cheapest and the most transparent for small corrections, and the chipmunk effect for large ones.

### MIDI and input

- **MIDI** sets what MIDI notes sent to Plumb do:
  - **Off**: MIDI is ignored.
  - **Scale from held notes**: the notes you hold become the only targets, and they stay when you let go, until you play the next chord. Play the chords of the song and the voice follows them.
  - **Note lock**: the last note you hold is the target, whatever is sung. Pitch bend moves it by up to **Bend** semitones.
  - **Auto**: held notes are the targets while held; the scale is used otherwise.
- **A =** sets the reference tuning, from 415 to 466 Hz.
- **Gate** (dB): below this level the detector ignores the input, so breaths and room noise are not corrected.

### Output

- **Mix** blends the corrected voice with the dry one. The dry voice is delayed to stay aligned, so a mix below 100 % thickens without phasing.
- **Output** sets the level.

## Reading the plumb bob and the note view

![The plumb bob swings with the tuning error of what is sung and hangs still when the note is right. Below: the sung note and the output note, each with its distance in cents from the nearest note.](img/bob.jpg)

The bob swings toward the side the voice is off: to the right when sharp, to the left when flat, further the larger the error. Each small mark of the scale engraved under it is 5 cents, and the scale ends at 50 cents either way. The numbers below are the useful part when tuning a performance: **Sung** is what arrives and **Out** what leaves, each as the nearest note and its distance from it in cents. With Natural settings Out moves with the vibrato and stays near zero on average; with Hard it reads zero.

![Natural Lead: the corrected pitch (brass) follows the sung pitch (chalk) with its vibrato and its scoops, moved onto the target notes (the blocks).](img/notes-natural.jpg)

In the note view, each horizontal line is a note: bright for notes of the scale, faint for the others. The chalk line is the sung pitch and the brass line the corrected one; the soft blocks are the target notes Plumb chose. Where the chalk and the brass part company, Plumb is working.

![Hard Tune: the same phrase, now every note exactly on its line and the steps instant.](img/notes-hard.jpg)

![A minor pentatonic: the key chip lit, a dot under every note of the scale; Listen finds the key from what is sung.](img/keys.jpg)

## Presets

The presets set the character of the correction. They never change the key, the scale, the voice reference or the gate: those belong to the song and stay as you set them.

| Group | Presets |
|---|---|
| Natural | Natural Lead, Gentle Polish, Pop Lead, Ballad, Backing Vocals, Choir Section |
| Hard | Hard Tune, Hard Tune Live, Soft Robot, Stepped Glide, Warble |
| Live | Stage Natural, Stage Firm, Tracking |
| Voice | Low Voice, High Voice, Rap Pitch Lock, Instrument |
| Character | Throat Down, Throat Up, Octave Down, Octave Up, Chipmunk, Added Vibrato, MIDI Note Lock |

## The command-line renderer

`plumbrender` runs a WAV file through Plumb with a preset and writes the result:

```
plumbrender in.wav out.wav --preset "Natural Lead" --set key=4 --set scale=1
plumbrender in.wav out.wav --preset "Hard Tune" --set key=9 --set scale=2 --at 6 style=50
```

- `--set id=value` sets a parameter (the ids are the ones in the Advanced page: `amount`, `speed`, `style`, `key` 0 to 11 from C, `scale` 0 to 13 in the order of the list, `voice`, `mode`, `transpose`, `throat` and so on).
- `--at seconds id=value` changes a parameter at that moment.
- `--tail seconds` renders past the end of the file (6 s by default).

It prints the CPU used, as a percentage of one core.

## Getting the best from it

- **Set the key.** Chromatic works for anything, but a voice sung between two notes then has two candidates a semitone apart; with the right scale it has one, and the correction can be gentler.
- **Feed it one voice.** Plumb follows one note at a time. Put it before reverb and delay, and before anything that adds a second pitch (doubling, chorus, harmonies).
- **Pick the narrowest voice range that fits.** It makes the detector faster at note starts and immune to octave mistakes.
- **Start natural and go firmer.** Most vocals sound best with the centre of the note corrected and the movement kept: Natural Lead or Pop Lead, then Style up until the out-of-tune moments disappear.
- **For monitoring, Live.** For recording, Tight. Switch to Studio for the mix if you want note starts corrected too.
- **Set the gate** a little above the noise of the room, so breaths are left alone.

## Limits

- Plumb is monophonic: chords, choirs on one track and instruments playing two notes at once are not corrected properly.
- Whispered and unvoiced sounds have no pitch and pass unchanged; very breathy or growled singing may be corrected less steadily.
- In Live and Tight the first 10 to 23 ms of each new note pass as sung.
- Large shifts (several semitones) with Preserve on are clean on held vowels and less so on fast consonant clusters.
- Plumb corrects pitch, not timing.

## Technical notes

- **Detection.** A difference-function detector (YIN family) runs on a decimated copy of the voice, every 8 samples at 12 kHz (24 kHz for the High and Instrument ranges), and is refined at the full rate over two periods. Guards catch octave errors at note starts, and a gate on the energy and steadiness of the window keeps breaths and noise out. Measured on synthetic vowels from 82 to 880 Hz: median error under 1.3 cents, no octave errors, lock within 11 to 23 ms.
- **Correction.** The pitch is split into a note centre (about 0.6 Hz), a slow drift and the vibrato; the target is chosen on the centre, with hysteresis. Style and the Character controls weigh what is kept of each.
- **Shifting.** The voice is shifted by repeating or dropping single cycles of the excitation (the residual of a linear-prediction model), joined at the quietest point of each cycle with a short crossfade. The formants come from a 48th-order linear-prediction model on a pre-emphasised signal, applied through normalised lattice filters so that changing them every block stays stable; Throat moves the model's resonances by scaling their angles.
- **Measured latency.** 32, 128 and 1024 samples at 44.1 and 48 kHz (64, 256 and 2048 at 96 kHz), equal to the reported latency in all nine cases. With Amount at 0 the output equals the delayed input to better than −120 dB.
- **CPU.** About 3 % of one core at 48 kHz on Apple M4 in every mode, and about 8 % with Throat. Measured figures for both platforms are on the website.
- Output is the same with any block size, bit for bit, and identical on every run.
