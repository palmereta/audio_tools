# AT20V user manual

## What AT20V is

AT20V is a semi-modular synthesizer modeled circuit by circuit on the Korg MS-20 of 1978. It has the two filters that give the original its scream, the full 35-jack patch bay with its hidden default connections, and the External Signal Processor that turns a guitar or a voice into control voltages. A second page adds what the original never had: a vocoder, polyphony, a tempo-synced delay, a plate reverb and system settings.

It comes in two versions with the same sound and the same interface:

- **AT20V** is an instrument, played from a MIDI track. Its audio input, used by the vocoder and the External Signal Processor, arrives through the sidechain.
- **AT20V FX** is an audio effect that also receives MIDI. It opens with **Filter FX**: the track runs through the MS-20 filters with no MIDI needed (the External Signal Processor feeds the filters and its trigger opens the VCA). The Effect presets (Filter FX, Auto-Wah FX, Screaming Filter FX, Envelope Gate FX) work the same way. With the Vocoder presets, the track becomes the vocoder modulator and you play the synth from a MIDI track routed to it.

Both run as VST3 on macOS and Windows, as Audio Unit on macOS, and as a standalone app. AT20V is free, open-source software under the GNU AGPL v3.

## Installation

Each zip file includes an `INSTALL.txt` with these steps.

### macOS

AT20V needs macOS 11 or later and runs natively on Apple silicon and Intel.

1. Unzip the file.
2. Copy `AT20V.vst3` and `AT20V FX.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
3. Copy the two `.component` bundles to `~/Library/Audio/Plug-Ins/Components/`.
4. Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan.

`AT20V.app` is the standalone version.

> The binaries are signed ad hoc and not notarized. If macOS refuses to open them, run this in Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/AT20V*.vst3 ~/Library/Audio/Plug-Ins/Components/AT20V*.component`

### Windows

AT20V needs Windows 10 or 11, 64-bit.

1. Unzip the file.
2. Copy the `AT20V.vst3` and `AT20V FX.vst3` folders to `C:\Program Files\Common Files\VST3\`. Windows asks for administrator rights.
3. Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan.

`AT20V.exe` is the standalone version and runs without a DAW.

> The binaries are not code-signed. Windows SmartScreen may ask you to confirm the first time.

## Loading AT20V in a DAW

Both versions appear under the maker name **Audio Tools**: AT20V among the instruments, AT20V FX among the effects.

### Ableton Live

The plugins are in Browser, Plug-Ins, Audio Tools. If they are missing, check in Settings, Plug-Ins that the VST3 system folders (and, on a Mac, Audio Units) are turned on, then press Rescan.

- **AT20V**: drag it onto a MIDI track, arm the track and play.
- **AT20V FX**: drag it onto an audio track, for example a vocal, a drum loop or a guitar. It filters the track right away (Filter FX); turn the filter knobs. To play the vocoder, load a Vocoder preset, create a MIDI track, set its MIDI To chooser to that audio track and the chooser below it to AT20V FX. The MIDI track now plays the synth, and the vocal is its input.
- **AT20V with a sidechain**: load AT20V on a MIDI track, open the Sidechain section in the title bar of the device and choose the track you want as the input. It feeds the vocoder and the External Signal Processor exactly like AT20V FX.

> AT20V FX replaces the audio of its track with the sound of the synth. To keep hearing the dry vocal, duplicate the track or send it to a return track.

### Other hosts and the standalone app

In AT20V the audio input is a sidechain bus named Audio In (Vocoder / ESP); route it in your host to use the vocoder or the External Signal Processor.

The standalone app opens the editor in its own window; choose your audio and MIDI devices in its options. On macOS it asks for permission to use the microphone, which feeds the vocoder and the External Signal Processor. In the standalone AT20V the audio input stays off, so to sing into the vocoder without a DAW use the standalone AT20V FX, where the input is always active.

### First sound

AT20V opens with the first factory preset, Synthe Bass. To start from a blank panel, click the preset name in the top bar and choose **Init (Normal Setting)**: both oscillators play a sawtooth at 8', both filters are open, the sound holds while you hold a key, and there are no cables. This is the "Normal Setting" of the original owner's manual, with VOLUME at 7. Every recipe in this manual starts from it.

## A tour of the interface

![The SYNTH page with the Sample & Hold preset: the front panel, the patch bay and the keyboard.](img/overview.jpg)

From top to bottom, the editor has the top bar, the page (SYNTH or EXTENSIONS), the keyboard and the lower bar.

### Top bar

![The top bar: preset browser, SAVE, and the SYNTH and EXTENSIONS tabs.](img/top-bar.jpg)

- **<** and **>** step through every preset, factory and user, and wrap around at the ends.
- The preset button shows the category and name of the current preset. Click it to open the preset menu.
- **SAVE** stores the current sound as a user preset.
- **SYNTH** shows the front panel and the patch bay. **EXTENSIONS** shows the vocoder, polyphony, delay, reverb and system settings.
- **MANUAL** opens this manual inside the plugin, in English or Spanish. Press Esc or the cross to close it.

### The SYNTH page

The front panel follows the original, in columns separated by thin white lines: the two oscillators (with PORTAMENTO and MASTER TUNE below), VCO MIXER (with FREQUENCY MODULATION below), the high-pass and low-pass filters (each with its CUTOFF FREQUENCY MODULATION), the VOLTAGE CONTROLLED AMPLIFIER with the MODULATION GENERATOR and ENVELOPE GENERATOR 1 below it, and ENVELOPE GENERATOR 2. The right side holds the patch bay, VOLUME and the EXTERNAL SIGNAL PROCESSOR.

Four red LEDs show activity: the MG LED blinks at the MG rate, the LED between the envelope generators glows with EG1, and the PEAK and TRIG LEDs of the External Signal Processor show overload and trigger detection.

### Keyboard

![The keyboard with a chord held, the control wheel and the momentary switch on the left.](img/keyboard.jpg)

The keyboard has 37 keys, C to C; at OCTAVE 0 the lowest key is MIDI note 36. Click a key to play it and drag across the keys for a glissando. To the left:

- The **control wheel**: drag it up or down. When you let go it springs back to the center, like a pitch bend wheel; hold Alt as you let go and it stays where it is. A double-click centers it.
- The **momentary switch**: the round button under the wheel. Click and hold it to press it.

As on the original, neither does anything by itself: they act only through their jacks in the patch bay, CONTROL WHEEL and MOMENTARY SWITCH.

### Lower bar

![The lower bar: voices, MIDI activity, audio input level, octave and CPU load.](img/lower-bar.jpg)

- **VOICES** shows the voice mode (Mono or Poly 2 to Poly 12) and the unison.
- **MIDI IN** and **MIDI OUT** light up with MIDI activity.
- **AUDIO IN** meters the audio input that feeds the vocoder and the External Signal Processor. It turns red near clipping.
- **-** and **+** shift the on-screen keyboard by octaves, from -2 to +2.
- **CPU** shows the load of the sound engine.

### Working with the controls

- **Knobs**: drag up or down; hold Shift for fine adjustment. The mouse wheel also works. Double-click returns a knob to its default (on the SYNTH page, the Normal Setting value).
- **Selectors** such as WAVE FORM or SCALE move one position per short drag or per wheel step.
- **Two-position switches** on the EXTENSIONS page (OFF and ON, REV 1 and REV 2, FREE and TEMPO, 4x and 8x) change with one click.
- Hover over a knob to see its value on the 0 to 10 scale of the panel and, when it helps, in Hz, ms, semitones or cents. Hover over a jack to see its name and default connection.
- Drag the bottom right corner of the window to resize it, from half to 1.4 times the full size.

## How the synthesizer is wired

Without cables, AT20V is already a complete synthesizer. The audio runs left to right: the two oscillators go into the VCO MIXER, then through the high-pass filter, the low-pass filter and the amplifier to VOLUME and the output.

The modulation is pre-wired. The keyboard plays both oscillators and its trigger starts both envelopes. The modulation generator (MG) reaches the three MG/T.EXT knobs. Envelope generator 1 (EG1) reaches the EG1/EXT knob and the modulation VCA. Envelope generator 2 (EG2) always drives the amplifier and reaches the two EG2/EXT filter knobs.

Each of these connections passes through a jack. Plug a cable into that jack and it replaces the internal connection; the knob after it now scales the patched signal. This is called normalling, and it is how the original works.

## Oscillators

![VOLTAGE CONTROLLED OSCILLATOR 1 and 2, with PORTAMENTO and MASTER TUNE below.](img/oscillators.jpg)

Both oscillators are band-limited and oversampled, with the slight curvature of the original circuit and a little analog drift (ANALOG on the EXTENSIONS page). At 8' with MASTER TUNE centered they play at concert pitch.

### VOLTAGE CONTROLLED OSCILLATOR 1

- **WAVE FORM**: triangle (soft, flute-like), sawtooth (bright and full), pulse (hollow) or white noise (wind, snares, effects).
- **PW**: the width of the pulse. At 0 it is square; toward 10 it narrows until it disappears, as on the original. PW also shapes the RING wave of VCO 2.
- **SCALE**: 32', 16', 8' or 4'. Each step is one octave.

### VOLTAGE CONTROLLED OSCILLATOR 2

- **WAVE FORM**: sawtooth, square, narrow pulse (fixed, nasal) or RING. RING combines the pulse of VCO 1 and the square of VCO 2 into a metallic, bell-like tone. It follows both SCALE knobs, PITCH and PW, but not the WAVE FORM of VCO 1. To hear it alone, turn VCO 1 LEVEL down.
- **PITCH**: -5 to +5, which detunes VCO 2 from one octave down to one octave up (the balloon shows semitones). A little gives a fat chorus; around +3.5 or +5, fifths and octaves.
- **SCALE**: 16', 8', 4' or 2'.

### PORTAMENTO and MASTER TUNE

- **PORTAMENTO TIME**: the glide between notes, from off at 0 to about 2 s at 10. It works on the Hz/V keyboard voltage, as on the original, so the glide curves.
- **MASTER TUNE**: tunes both oscillators up to one semitone up or down (shown in cents).

## VCO mixer and frequency modulation

![VCO MIXER and FREQUENCY MODULATION, and the two filters with their cutoff modulation.](img/mixer-filters.jpg)

- **VCO 1 LEVEL** and **VCO 2 LEVEL**: how much of each oscillator goes to the filters, 0 to 10. With filter revision REV 1, more level also means more filter distortion.

FREQUENCY MODULATION acts on the pitch of both oscillators:

- **MG/T.EXT**: the depth of the MG, or of whatever is patched into TOTAL. With the MG it gives vibrato and sirens; at 10 the pitch swings about 2.5 octaves each way. The knob has a square law, so vibrato lives around 1 to 2.
- **EG1/EXT**: the depth of EG1, or of whatever is patched into FREQ. It gives pitch sweeps for lasers, drums and brass attacks, up to about 5 octaves at 10.

## Filters

A high-pass filter followed by a low-pass filter, both with resonance (PEAK), modeled from Tim Stinchcombe's analysis of the circuit and the service manual.

### VOLTAGE CONTROLLED HIGHPASS FILTER

- **CUTOFF FREQUENCY** (large knob): 50 Hz to 15 kHz. At 0 it is fully open; turning it up removes the low end. The slope is 6 dB per octave, the real slope of the original.
- **PEAK**: resonance. Near the top of its travel (around 9) the filter oscillates on its own.

### VOLTAGE CONTROLLED LOWPASS FILTER

- **CUTOFF FREQUENCY** (large knob): 50 Hz to 15 kHz. At 10 it is fully open; turning it down darkens the sound. The slope is 12 dB per octave.
- **PEAK**: resonance, with self-oscillation near the top. Diodes limit the resonance, so at full PEAK the filter screams instead of whistling.

With CUTOFF FREQUENCY at 5 and PEAK at 10, each filter oscillates at 500 Hz, the factory calibration of the original. With the high-pass cutting below and the low-pass above, you get the band-pass "talking" sound typical of the MS-20.

### CUTOFF FREQUENCY MODULATION

Each filter has two modulation knobs:

- **MG/T.EXT**: the depth of the MG, or of TOTAL, on that cutoff. At 10 the MG sweeps about 5 octaves each way.
- **EG2/EXT**: the depth of EG2, or of that filter's CUTOFF FREQ jack. This is the filter envelope; 3 to 6 is usually enough.

As on the original, the filters do not follow the keyboard. The Hammond Organ preset shows a way to add it: KBD CV OUT patched into TOTAL.

### Filter revisions

The original used two filter circuits over its life. Choose one with **FILTER** on the EXTENSIONS page:

- **REV 1** (Korg35, the default): the distortion grows with the mixer level, about 5 % with one oscillator at full level and about 15 % with both, and the resonance is slightly asymmetric. The gritty version.
- **REV 2** (OTA): the passband stays clean and only the resonance is limited.

## Amplifier and envelopes

![The VOLTAGE CONTROLLED AMPLIFIER, the MODULATION GENERATOR and both envelope generators.](img/modulation.jpg)

### VOLTAGE CONTROLLED AMPLIFIER

The amplifier has no knob. Its gain is EG2 plus the voltage at INITIAL GAIN, and it stops rising when the sum reaches full level. With nothing patched, EG2 alone shapes every note.

> For a drone that sounds without holding a key, patch CONTROL WHEEL into VCA INITIAL GAIN, push the wheel up and let go with Alt held so it stays up.

### ENVELOPE GENERATOR 1

When triggered, EG1 waits for the delay time, rises during the attack, stays at full level while the key is held, and falls during the release.

- **DELAY TIME**: 0 to 10 s before the attack. For delayed vibrato or late sweeps.
- **ATTACK TIME**: about 0.8 ms to 10 s.
- **RELEASE TIME**: about 2 ms to 10 s.

EG1 feeds the EG1/EXT knob and the MVCA. Its outputs are EG1 OUT and EG1 REV OUT (inverted).

### ENVELOPE GENERATOR 2

EG2 always drives the amplifier and, through the EG2/EXT knobs, the filters.

- **HOLD TIME**: 0 to 20 s. The envelope keeps going until the hold time has passed since the trigger, even after a short tap. It is what keeps the UFO and Sample & Hold patches sounding.
- **ATTACK TIME**: about 0.8 ms to 10 s.
- **DECAY TIME**: about 2 ms to 10 s, down to the sustain level.
- **SUSTAIN LEVEL**: 0 to 10, held while the key is down.
- **RELEASE TIME**: about 2 ms to 10 s after the key is released.

The time knobs follow an audio-taper curve: the lower half covers short, snappy times and the top quarter the very long ones. As on the original, the only EG2 output in the patch bay is EG2 REV OUT, which is inverted.

## Modulation generator

The MODULATION GENERATOR (MG) is the low-frequency oscillator, with a triangle output (also normalled to the three MG/T.EXT knobs) and a rectangle output.

- **WAVE FORM**: continuously variable. In the center, a triangle and a square. To the left, a falling ramp and a narrow pulse; to the right, a rising ramp and a wide pulse.
- **FREQUENCY**: 0.1 Hz to about 25 Hz. Around 4 to 5 gives the 4 to 6 Hz of a natural vibrato. The red LED blinks with the rectangle.

When **MG CLOCK** on the EXTENSIONS page is set to TEMPO, the MG follows the tempo of your DAW and FREQUENCY chooses a division:

| FREQUENCY | Division |
|---|---|
| 0 | 4 bars |
| 1 | 2 bars |
| 2 | 1 bar |
| 3 | 1/2 |
| 4 | 1/4 |
| 5 | 1/4 triplet |
| 6 | 1/8 |
| 7 | 1/8 triplet |
| 8 | 1/16 |
| 9 | 1/16 triplet |
| 10 | 1/32 |

> In TEMPO mode the balloon of FREQUENCY still shows the free-running rate in Hz. Read the division from this table.

While the transport runs, the MG locks to the song position. When it is stopped, the MG keeps running at the tempo.

## Patch bay

![The patch bay with the cables of the Sample & Hold preset, VOLUME, SIGNAL OUT and PHONES.](img/patch-bay.jpg)

All 35 jacks work, including the default connections. The white lines printed between them are the signal-flow diagram of the original.

### Working with cables

- **Connect**: drag from a free jack to a compatible one, always output to input. While you drag, the jacks you can reach are highlighted.
- **Stack**: an output accepts several plugs, so one source can feed many inputs. Shift-drag from an output that already has a cable to add another one. An input takes one plug; a new cable replaces the old one.
- **Move**: click a jack with a cable to lift the end of the last cable plugged into it, and drop it on another jack.
- **Unplug**: lift an end and drop it away from any jack. Alt-click (Option-click on a Mac) a jack to remove all its cables.
- **Menu**: right-click a jack for Cable color (red, blue, yellow, green, white or black) and Unplug.

Cables hang and swing with real physics. Feedback loops are allowed anywhere: like the power rails of the original, the patch bay saturates a loop instead of letting it grow without limit.

### Electrical conventions

- **Pitch** voltages (KBD CV OUT, the VCO CV inputs, ESP F∝V CV OUT) are Hz/V, as on the original, so they play the oscillators in tune. Patched into a 1 V/oct input such as TOTAL, they give a curve instead of a straight scale.
- **TOTAL** and **FREQ** respond at 1 V/oct; the two **CUTOFF FREQ** inputs at 2 oct/V.
- **Triggers** are S-trig: outputs rest at +5 V and drop to 0 V while active, and inputs fire when their voltage drops near 0 V. A 0 to +5 V signal such as MG RECTANGLE triggers while it is low.

### Jack reference

Names as the balloon shows them.

| Jack | Type | Level | With nothing plugged in |
|---|---|---|---|
| TOTAL | input | -5 to +5 V, 1 V/oct | MG triangle to the three MG/T.EXT knobs |
| FREQ | input | -5 to +5 V, 1 V/oct | EG1 to the EG1/EXT knob |
| EXT SIGNAL IN | input | audio | nothing; added to the mixer at the HPF input |
| HPF CUTOFF FREQ | input | -5 to +5 V, 2 oct/V | EG2 to the HPF EG2/EXT knob |
| LPF CUTOFF FREQ | input | -5 to +5 V, 2 oct/V | EG2 to the LPF EG2/EXT knob |
| VCA INITIAL GAIN | input | 0 to +5 V | nothing; added to EG2 at the VCA |
| SIGNAL OUT | output | audio after VOLUME | - |
| PHONES | output | audio after VOLUME | - |
| MG TRIANGLE | output | -2.5 to +2.5 V | - |
| MG RECTANGLE | output | 0 or +5 V | - |
| EG1 OUT | output | 0 to +5 V | - |
| EG1 REV OUT | output | +5 to 0 V | - |
| EG2 REV OUT | output | 0 to -5 V | - |
| VCO 2 CV IN | input | Hz/V | whatever reaches VCO 1+2 CV IN |
| VCO 1+2 CV IN | input | Hz/V | keyboard CV |
| KBD CV OUT | output | Hz/V, after portamento | - |
| S/H CLOCK | input | S-trig | nothing |
| MVCA CONTROL | input | 0 to +5 V | EG1 |
| EG1 TRIG IN | input | S-trig | the trigger bus (keyboard or TRIG IN) |
| TRIG IN | input | S-trig | keyboard trigger |
| KBD TRIG OUT | output | S-trig, low while a key is held | - |
| S/H IN | input | signal to sample | nothing |
| S/H OUT | output | held voltage | - |
| MVCA IN | input | signal | nothing |
| MVCA OUT | output | signal | - |
| PINK NOISE | output | 5 Vpp | - |
| WHITE NOISE | output | 5 Vpp | - |
| CONTROL WHEEL | output | -5 to +5 V | - |
| MOMENTARY SWITCH | output | S-trig | - |
| ESP SIGNAL IN | input | audio | the audio input of the plugin |
| ESP AMP OUT | output | audio after the preamp | - |
| ESP BPF OUT | output | audio after the band pass | - |
| ESP F∝V CV OUT | output | pitch as Hz/V | - |
| ESP ENV OUT | output | 0 to +5 V | - |
| ESP TRIG OUT | output | S-trig | - |

Good to know when patching:

- **TOTAL** feeds all three MG/T.EXT knobs (pitch, HPF and LPF); a cable there disconnects the MG from all three.
- **TRIG IN** replaces the keyboard trigger for both envelopes, but the keyboard still sets the pitch: hold a key to transpose a self-triggering patch. **EG1 TRIG IN** retriggers EG1 alone.
- **VCO 1+2 CV IN** disconnects the keyboard from both oscillators; **VCO 2 CV IN** only from VCO 2.
- **SIGNAL OUT** and **PHONES** carry the plugin output. Patching them elsewhere, for example into ESP SIGNAL IN for feedback, does not mute it.
- **EXT SIGNAL IN** is not connected to the audio input. To filter your track, patch ESP AMP OUT into it (see the recipes).

### The patch-only modules

- **SAMPLE & HOLD**: each time S/H CLOCK drops to ground, S/H OUT takes the voltage at S/H IN and holds it. With noise at the input you get random steps. It needs a clock patched, such as MG RECTANGLE.
- **MVCA**: multiplies MVCA IN by MVCA CONTROL, which is EG1 unless you patch something else. The classic use is delayed vibrato.
- **NOISE GENERATOR**: PINK NOISE (darker) and WHITE NOISE.
- **CONTROL WHEEL** and **MOMENTARY SWITCH**: the wheel and the button next to the keyboard. MIDI pitch bend and the mod wheel move the wheel; the sustain pedal presses the switch.

## External Signal Processor

![The EXTERNAL SIGNAL PROCESSOR with the cables of the Guitar Synth I preset.](img/esp.jpg)

The External Signal Processor (ESP) turns the pitch and level of an audio signal, such as a guitar or a voice, into control voltages. Its input, ESP SIGNAL IN, is connected to the audio input of the plugin (the sidechain of AT20V, or the track in AT20V FX); you can also patch any synth output into it. The signal goes through a preamp, a band-pass filter and a frequency-to-voltage converter, while an envelope follower and a trigger detector read the preamp.

- **SIGNAL LEVEL**: preamp gain, about -20 to +26 dB, with soft clipping. Set it so the red **PEAK** LED only flickers on the loudest notes.
- **LOW CUT FREQ**: the low edge of the band pass, 50 Hz to 2.5 kHz.
- **HIGH CUT FREQ**: the high edge, 100 Hz to 5 kHz. Narrow the band around the fundamentals of your instrument so the pitch detector does not jump to harmonics.
- **CV ADJUST**: the scale of the pitch voltage. At 10 the ESP plays the oscillators at the pitch it hears; lower values transpose down, up to two octaves at 0.
- **THRESHOLD LEVEL**: how loud the signal must be to fire the trigger. The red **TRIG** LED lights while it is active.

The outputs are ESP AMP OUT (preamp), ESP BPF OUT (band pass), ESP F∝V CV OUT (pitch, for VCO 1+2 CV IN), ESP ENV OUT (envelope, about 1 ms attack and 50 ms decay) and ESP TRIG OUT (for TRIG IN). The pitch detector follows fundamentals from about 40 Hz to 1.2 kHz, and only runs while ESP F∝V CV OUT has a cable.

> Init (Normal Setting) leaves LOW CUT FREQ at 10 and HIGH CUT FREQ at 0, which closes the band pass. Before tracking pitch, turn LOW CUT FREQ down and HIGH CUT FREQ up (the Guitar Synth presets use 0 and 5.5).

## The EXTENSIONS page

![The EXTENSIONS page with the Vocoder Choir preset.](img/extensions.jpg)

Five sections drawn in the style of the panel: VOCODER, POLYPHONY, DELAY, REVERB and SYSTEM.

### Vocoder

![The VOCODER section and its SIGNAL LED.](img/ext-vocoder.jpg)

The vocoder imposes the spectrum of a voice on the synth. The voice (the modulator) is the audio input; the synth (the carrier) is whatever AT20V is playing. The voice is analyzed in bands from 120 Hz to 7.5 kHz, and each band opens the same band of the synth. Sing or talk into the input and play chords.

- **VOCODER**: OFF or ON.
- **BANDS**: 8, 12, 16 (default) or 20. More bands make words clearer; fewer sound more robotic.
- **INPUT LEVEL**: -12 to +24 dB (default +6 dB). The **SIGNAL** LED next to the title glows with the input.
- **FORMANT**: shifts the synth bands up to one octave up or down. Up gives a smaller, brighter voice; down a larger, darker one.
- **RESONANCE**: narrower, more resonant bands as you turn it up, for a sharper robot voice.
- **SIBILANCE**: adds filtered noise on consonants (voice energy above 5 kHz), so S and T sounds stay clear.
- **ATTACK**: how fast the bands open, 1 to 100 ms.
- **RELEASE**: how fast they close, 10 ms to 1 s.
- **MIX**: from the plain synth (0) to the vocoder alone (10, the default).

The carrier is the full synth after the amplifier, so hold notes while you sing. Bright carriers work best: both oscillators, an open low-pass filter and a high EG2 SUSTAIN LEVEL. The vocoder comes before the delay and the reverb.

### Polyphony

![The POLYPHONY and DELAY sections.](img/ext-poly-delay.jpg)

AT20V starts monophonic, like the original. In polyphonic modes each voice is a complete MS-20 with its own envelopes, MG and patch bay, and the cables apply to all voices.

- **VOICES**: MONO, 2, 4, 6, 8 or 12 notes at once.
- **UNISON**: 1 to 4 voices stacked on each note. There are 12 voices in total, so with VOICES at 12 and UNISON at 4 you can play 3 notes.
- **DETUNE**: 0 to 50 cents (default 12) between the outermost unison voices.
- **SPREAD**: stereo width. Unison voices spread left to right; without unison, successive notes alternate sides.
- **PRIORITY**: in MONO, which held note sounds: LAST, HIGH (default, as on the original) or LOW.
- **RETRIGGER**: in MONO, OFF (default) triggers the envelopes only once while you play legato, as on the original, so overlapping notes glide without a new attack. ON restarts them on every note.

Changing VOICES or UNISON while holding notes replays them with the new layout.

### Delay

A stereo ping-pong delay synced to the DAW tempo: the input goes left and each repeat crosses sides.

- **DELAY**: OFF or ON.
- **TIME**: from 1/32 to 1 bar, with triplet and dotted values (default dotted 1/8). Up to 4 s.
- **FEEDBACK**: 0 to 9.5. High values saturate softly.
- **TONE**: lower values make each repeat darker.
- **MIX**: the level of the repeats, added to the dry sound.

### Reverb

![The REVERB and SYSTEM sections.](img/ext-reverb-system.jpg)

A plate reverb, used by most ambient presets.

- **REVERB**: OFF or ON.
- **SIZE**: the length of the decay.
- **DAMPING**: how fast the highs fade in the tail.
- **MIX**: the reverb level, added to the dry sound.

### System

- **FILTER**: REV 1 (Korg35) or REV 2 (OTA). See Filter revisions.
- **ANALOG**: 0 to 10 (default 3.5): slow pitch drift, small differences between oscillators and voices, and a faint hiss. At 0 everything is perfectly stable.
- **MG CLOCK**: FREE or TEMPO (the MG follows the DAW and FREQUENCY chooses the division).
- **BEND RANGE**: 0 to 12 semitones (default 0). Pitch bend always moves the control wheel; above 0 it also bends the oscillators directly.
- **OVERSAMPLING**: 4x (default) or 8x for the whole voice. 8x is slightly smoother on high, resonant sounds and costs about twice the CPU.
- **MIDI OUT**: OFF, THRU (default), PATCH or BOTH. See MIDI out.
- **OUTPUT**: -24 to +6 dB for the whole instrument.

BEND RANGE, OVERSAMPLING, MIDI OUT and OUTPUT are global: presets do not store or change them. They are saved with your project.

## Presets

### Browsing and saving

Click the preset name to open the menu. Each category is a submenu, with the current preset checked. At the end, **Init (Normal Setting)** loads the blank panel and **Show user presets folder** opens the folder of your presets.

To save, click **SAVE**, type a name and press Save. The preset appears under the User category; saving again with the same name replaces it without asking. User presets are JSON files:

- macOS: `~/Library/Application Support/Audio Tools/AT20V/Presets`
- Windows: `%APPDATA%\Audio Tools\AT20V\Presets`

To delete a preset, remove its file from that folder. Files you add appear the next time AT20V loads.

A preset stores every knob of the SYNTH page, every cable with its color, and the EXTENSIONS settings except the four global ones. Settings a preset does not include return to their defaults, so the reverb or unison of one preset never sticks to the next.

### Factory presets

| Category | Presets |
|---|---|
| Factory | Synthe Bass, Hammond Organ, Parallel Fifths, Voice, Pitch Bend, Steam Locomotive, UFO, Horn, Violin, Sample & Hold, Guitar Synth I, Guitar Synth II, Trumpet, Clarinet |
| Bass | Acid Bass, Fat Saw Bass, Hoover Square |
| Lead | Scream Lead, Reso Pluck, Delayed Vibrato Lead |
| Keys | Ring Bells |
| Drums | MS Kick, MS Snare, MS Hi-Hat |
| FX | Howling Wind, Random Bloops, Laser Zap |
| Self-Playing | Random Bleeps, Robot Talk, Drip Cave, Morse Station, Chaos Arp, Pulse Train Bass, Wind Chimes, Clockwork Toy, Bubbling Swamp, Endless Drift, Space Telemetry |
| Ambient | Glass Cathedral, Analog Fog, Frozen Horizon, Deep Ocean Drone, Aurora, Distant Choir, Tape Memories, Breathing Strings, Lunar Bells |
| Vocoder | Classic Robot Voice, Vocoder Choir, Whisper Machine, Talk Box Lead |
| Effect | Filter FX, Auto-Wah FX, Screaming Filter FX, Envelope Gate FX |

- **Factory**: 14 patches transcribed from the setting charts of the original owner's manual. Synthe Bass, Voice, Pitch Bend and Steam Locomotive use the control wheel; the Guitar Synth patches need a guitar at the audio input.
- **Bass, Lead, Keys, Drums, FX**: 13 classic sounds, from acid basses to a drum kit made of filters.
- **Self-Playing**: 11 patterns where the MG in TEMPO mode fires the envelopes and the sample and hold, in time with your DAW. Hold a key to transpose.
- **Ambient**: 9 slow polyphonic pads with delay and reverb.
- **Vocoder**: 4 presets that stay silent until a voice reaches the audio input.
- **Effect**: 4 patches for AT20V FX that process the audio input with no MIDI: the External Signal Processor feeds the filters (EXT SIGNAL IN) and its trigger opens the VCA (KBD TRIG IN). Auto-Wah FX also sends the input envelope to the low-pass cutoff.

## MIDI and sync

### Notes and controllers

AT20V plays notes from every MIDI channel. Velocity has no effect, as on the original. The original's wheel and switch only act through their jacks, and MIDI is mapped onto them:

- **Pitch bend** moves the control wheel. If BEND RANGE is above 0, it also bends the oscillators.
- **Mod wheel** (CC1) moves the same wheel, upward only, added to the pitch bend.
- **Sustain pedal** (CC64) presses the momentary switch. It does not hold notes.
- **CC121** centers the wheel and releases the switch; **CC120** and **CC123** release every voice.

### MIDI out

Set with MIDI OUT on the EXTENSIONS page:

- **THRU** (default) passes on the incoming MIDI plus the notes of the on-screen keyboard.
- **PATCH** turns every trigger of the keyboard bus into a note, including triggers made by the patch bay (for example MG RECTANGLE into TRIG IN), with the pitch VCO 1 is playing, on channel 1. A self-playing preset can then play another instrument.
- **BOTH** does both; **OFF** sends nothing.

In Ableton Live, create a MIDI track with another instrument, set its MIDI From to the AT20V track (and the chooser below to AT20V), and set Monitor to In. When bypassed, AT20V is silent and passes MIDI only in THRU or BOTH.

### Host tempo

The MG in TEMPO mode and the delay follow the DAW tempo, or 120 BPM if the host reports none.

### Automation and MIDI control

Every knob and setting can be automated; the cables cannot. AT20V has no MIDI learn of its own: map a hardware controller to its parameters with the MIDI mapping of your DAW. In the host, names combine section and knob (HPF Cutoff, EG2 Hold), and a few settings read differently: MG CLOCK is MG Tempo Sync, FILTER is Filter Revision, INPUT LEVEL is Vocoder Input, RESONANCE is Vocoder Q.

## Recipes

Each recipe starts from Init (Normal Setting).

### Acid bass

1. Set VCO 1 WAVE FORM to sawtooth and SCALE to 16'. Turn VCO 2 LEVEL to 0.
2. On the low-pass filter, set CUTOFF FREQUENCY to 3, PEAK to 8 and EG2/EXT to 6.
3. On EG2, set DECAY TIME to 3, SUSTAIN LEVEL to 1.5 and RELEASE TIME to 1.5.
4. Set PORTAMENTO TIME to 2.5 and play legato: overlapping notes glide without a new attack.
5. For more bite, raise HPF CUTOFF FREQUENCY to about 1.5, or push the low-pass PEAK higher with FILTER at REV 1.

### Delayed vibrato

1. Turn VCO 2 LEVEL to 0.
2. Patch MG TRIANGLE into MVCA IN and MVCA OUT into TOTAL.
3. Set FREQUENCY MODULATION MG/T.EXT to 3 and MG FREQUENCY to 5.5.
4. Set EG1 DELAY TIME to 4.5 and ATTACK TIME to 5. EG1 controls the MVCA, so the vibrato waits, then fades in.
5. Keep the HPF and LPF MG/T.EXT knobs at 0, or raise them for a delayed filter wobble: TOTAL feeds them too.

### The wheel as pitch bend

1. Patch CONTROL WHEEL into FREQ. EG1 leaves the pitch, and EG1/EXT now sets the bend range.
2. Set FREQUENCY MODULATION EG1/EXT to about 1.8 for a whole tone, or about 4.5 for an octave.
3. Move the on-screen wheel or the pitch bend of your keyboard.

Patched into TOTAL instead, the wheel can bend the pitch and move both filters at once through the three MG/T.EXT knobs. For ordinary pitch bend, just set BEND RANGE.

### Sample and hold

1. Patch PINK NOISE into S/H IN, MG RECTANGLE into S/H CLOCK, and S/H OUT into TOTAL.
2. Set FREQUENCY MODULATION MG/T.EXT to 6 for random pitch, or leave it at 0 and raise LPF MG/T.EXT with a high PEAK for random filter steps.
3. MG FREQUENCY sets the speed. Set MG CLOCK to TEMPO to lock it to your song.

### Kick drum from the filter

1. Set VCO 1 WAVE FORM to triangle, SCALE to 32' and VCO 1 LEVEL to 6. Turn VCO 2 LEVEL to 0.
2. On the low-pass filter, set CUTOFF FREQUENCY to 1.5, PEAK to 9.4 and EG2/EXT to 6.
3. On EG2, set DECAY TIME and RELEASE TIME to 3.6 and SUSTAIN LEVEL to 0.
4. Play low notes. The ringing filter is the body, EG2 the pitch drop; DECAY TIME sets the length.

### A pattern that plays another instrument

![The SYNTH page with the Random Bleeps preset: MG RECTANGLE drives TRIG IN and the sample and hold.](img/patch-example.jpg)

1. Set VCO 1 WAVE FORM to pulse and turn VCO 2 LEVEL to 0.
2. Patch MG RECTANGLE into TRIG IN, then drag a second cable from MG RECTANGLE into S/H CLOCK.
3. Patch WHITE NOISE into S/H IN and S/H OUT into TOTAL. Set FREQUENCY MODULATION MG/T.EXT to 6.
4. On EG2, set DECAY TIME to 2.5, SUSTAIN LEVEL to 0 and RELEASE TIME to 2. On the low-pass filter, set CUTOFF FREQUENCY to 6, PEAK to 6 and EG2/EXT to 3.
5. On the EXTENSIONS page, set MG CLOCK to TEMPO. Back on the SYNTH page, set MG FREQUENCY to 8 (1/16 notes). The patch plays by itself in time; hold a key to transpose it.
6. Set MIDI OUT to PATCH and route the MIDI to another instrument as described in MIDI out. It now plays the same random notes.

The Random Bleeps preset is a close relative, with a delay added.

### Vocoder in Ableton Live

1. Put AT20V FX on the audio track with the voice: a recorded vocal, or a microphone with Monitor set to In.
2. Create a MIDI track, set its MIDI To to the vocal track and the chooser below to AT20V FX, and arm it.
3. Load Vocoder Choir, or Classic Robot Voice for a harder sound.
4. Play the vocal and hold chords.
5. Set INPUT LEVEL so the SIGNAL LED and the AUDIO IN meter move clearly without turning red.
6. Adjust BANDS for clarity, FORMANT for the size of the voice, SIBILANCE for consonants, and MIX to blend in the plain synth.

With AT20V instead, load it on the MIDI track and choose the vocal track in its Sidechain section. To build a vocoder sound from Init, set VOICES to 6, keep both sawtooth oscillators and the open low-pass filter, and turn VOCODER on.

### A guitar playing the synth

1. Feed the guitar into the audio input: AT20V FX on the guitar track, or AT20V with the guitar as its sidechain.
2. Patch ESP F∝V CV OUT into VCO 1+2 CV IN, and ESP TRIG OUT into TRIG IN.
3. Set ESP SIGNAL LEVEL so PEAK only flickers on the hardest notes.
4. Set LOW CUT FREQ to 0, HIGH CUT FREQ to 5.5, CV ADJUST to 10 and THRESHOLD LEVEL to 3. The TRIG LED should light with each note.
5. Use SCALE, PITCH and MASTER TUNE to tune the synth to the guitar or an interval away.

Guitar Synth I and II are complete versions of this patch. A voice works the same way.

### Your track through the filters

1. Put AT20V FX on an audio track, such as drums or a synth loop. Turn VCO 1 LEVEL and VCO 2 LEVEL to 0.
2. Patch ESP AMP OUT into EXT SIGNAL IN. The track now runs through both filters and the amplifier.
3. Patch ESP ENV OUT into VCA INITIAL GAIN. The level of the track opens the amplifier, so no notes are needed.
4. Play with the CUTOFF FREQUENCY and PEAK knobs, and with ESP SIGNAL LEVEL for drive. For a rhythmic sweep, set MG CLOCK to TEMPO and raise LPF MG/T.EXT.

## Tips and troubleshooting

### No sound

- Check VOLUME (7 is unity gain) and OUTPUT.
- A cable in TRIG IN replaces the keyboard trigger: if its source never drops to ground, the envelopes never start. A cable in VCO 1+2 CV IN disconnects the keyboard from the oscillators.
- The vocoder presets are silent without audio at the input. Check the AUDIO IN meter: in AT20V FX the track must play audio; in AT20V the sidechain must be routed. Turn the vocoder MIX down to hear the plain synth.

### Sounds that play by themselves

Self-Playing presets sound as soon as they load, even with the transport stopped. Load another preset or unplug TRIG IN to stop them.

### The wheel, the pedal or velocity do nothing

Pitch bend and the mod wheel move the on-screen wheel, but the sound only changes if CONTROL WHEEL is patched or BEND RANGE is above 0. The sustain pedal presses the momentary switch and never holds notes. Velocity is ignored.

### Distortion and level

- Both filters distort at high PEAK, and with REV 1 the distortion also grows with the mixer level. Lower the VCO levels, or switch to REV 2.
- A soft limiter keeps the output below 0 dB. If a sound gets squashed, lower VOLUME or OUTPUT.
- If a feedback patch goes out of control, AT20V mutes for a moment and resets the voices instead of locking up.

### CPU

- Keep OVERSAMPLING at 4x; 8x costs about twice as much.
- Use fewer voices and less unison: each voice is a complete synth.
- Turn off the delay, reverb or vocoder when unused.
- Close the editor, which adds about 5 to 15 % of a core.

## Specifications

| | |
|---|---|
| Version | 1.0.2 |
| Plugins | AT20V (instrument), AT20V FX (audio effect with MIDI input) |
| Formats | macOS: VST3, Audio Unit, standalone. Windows: VST3, standalone |
| Audio | stereo output (mono supported); stereo or mono input, summed to mono |
| MIDI in | notes on all channels, pitch bend, CC1, CC64, CC120, CC121, CC123 |
| MIDI out | Thru, Patch or both |
| Voices | mono or 2 to 12; unison 1 to 4; 12 voices in total |
| Oversampling | 4x or 8x for the whole voice |
| Patch bay | 35 jacks with the normalling of the original |
| Filters | HPF 6 dB/oct, LPF 12 dB/oct, Rev 1 or Rev 2 |
| Vocoder | 8 to 20 bands, 120 Hz to 7.5 kHz |
| Effects | tempo-synced ping-pong delay, plate reverb |
| Presets | 51 factory presets in 9 categories, plus user presets |

### System requirements

| | Minimum | Recommended |
|---|---|---|
| macOS | macOS 11 Big Sur or later; Apple silicon (M1 or later) or Intel Core i5 quad-core from 2017 | Any Mac with Apple silicon |
| Windows | Windows 10 or 11, 64-bit; Intel Core i5-6500 (2015) or AMD Ryzen 5 1600 | Intel Core i5-12400 or AMD Ryzen 5 5600 |
| Both | 4 GB RAM, 1280 x 800 screen, a 64-bit VST3 or AU host (or the standalone app) | 8 GB RAM, 1920 x 1080 screen |

### CPU load

Percentage of one core at 48 kHz, sound engine only; the editor adds about 5 to 15 % on an Apple M4. Hosts run each instrument on one core, so single-core speed is what counts.

| Setting | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| Mono, 4x | 1.6 % | 3.6 % |
| 8 voices with delay and reverb, 4x | 11.2 % | 32.4 % |
| 12 voices, 8x (needs the recommended CPU) | 28.9 % | 78.1 % |
