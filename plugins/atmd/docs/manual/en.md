# ATMD user manual

## What it is

ATMD is a free, open-source monophonic synthesizer modeled on the Moog Minimoog Model D, built from 1970 to 1981. It recreates a mid-1970s unit with the Rev 2 oscillator board, the most common version of the instrument and the one Moog itself took as the reference for its later reissues.

It has the same panel and the same signal path: three oscillators, a mixer that can push the filter into saturation, the four-pole transistor ladder filter, two contour generators, the GLIDE and DECAY switches and the two wheels beside the keys, the external input with its OVERLOAD lamp and the A-440 tuning tone. It plays like the original too: low-note priority, single trigger, and a keyboard that holds the last pitch, so GLIDE always slides from the previous note.

A SETTINGS page adds what the Model D never had: up to 8 voices of polyphony, unison up to 4 voices, adjustable analog drift, velocity and aftertouch, a choice of note priority and trigger mode, a wider or narrower pitch wheel, a tempo-synced delay, a plate reverb and 32 factory presets.

ATMD runs as a VST3 plugin, as an Audio Unit on macOS, and as a standalone application on macOS and Windows.

![ATMD with the Fat Bass preset: the front panel, the left-hand controller and the 44-key keyboard.](img/overview.jpg)

## Installing

Download the zip file for your system from the Audio Tools website. Both packages include an `INSTALL.txt` file with the same steps as below.

### macOS

ATMD needs macOS 11 or later. One universal binary runs on Apple silicon and Intel Macs.

1. Unzip the file.
2. Copy `ATMD.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
3. Copy `ATMD.component` to `~/Library/Audio/Plug-Ins/Components/`.
4. Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan.
5. `ATMD.app` is the standalone version. Copy it to Applications if you want to use it without a DAW.

The binaries are signed ad hoc and not notarized, so macOS may refuse to open them the first time. If that happens, open Terminal and run this command, then rescan again:

`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/ATMD*.vst3 ~/Library/Audio/Plug-Ins/Components/ATMD*.component`

### Windows

ATMD needs Windows 10 or 11, 64-bit.

1. Unzip the file.
2. Copy the `ATMD.vst3` folder to `C:\Program Files\Common Files\VST3\`. Windows asks for administrator rights.
3. Rescan plugins in your DAW.
4. `ATMD.exe` is the standalone version and runs without a DAW.

The binaries are not code-signed, so Windows SmartScreen may ask you to confirm the first time you run them.

## Loading it in a DAW

ATMD is an instrument: put it on a MIDI or instrument track, not on an audio track. It has a stereo output (it also works on a mono bus), receives MIDI on any channel, and has an optional stereo audio input called External Input that feeds the EXTERNAL INPUT of the mixer (see Routing audio into the external input). The plugin reports a tail of 12 seconds, so long decays, the delay and the reverb are not cut off when a clip ends.

### Ableton Live

1. After rescanning, open the browser and choose Plug-Ins.
2. Open Audio Units (macOS) or VST3, then the Audio Tools folder.
3. Drag ATMD onto an empty MIDI track, or onto the empty area of the session to create a new one.
4. Arm the track and play your MIDI keyboard. Click the wrench icon in the device title bar if the editor window is closed.

> On Windows, if ATMD does not appear, open Settings, Plug-Ins, turn on Use VST3 Plug-in System Folders and rescan.

### Other DAWs

In any other host, insert ATMD as an instrument and send it MIDI. Every knob and switch on the panel and on the SETTINGS page is a host parameter, so you can automate it. The two wheels are not parameters, as on the original they are performance controls: automate them with pitch bend and CC 1 in your MIDI clips. The factory and user presets also appear in the host's program list.

### Standalone

Open `ATMD.app` or `ATMD.exe`. Use the Options button at the top of the window to choose the audio output and the MIDI input. Without a MIDI keyboard you can play the keyboard on screen with the mouse, or the computer keyboard (see Playing without a MIDI keyboard). In the standalone there is no host tempo, so the delay follows 120 BPM. The external input is the audio input you choose in Options (see Routing audio into the external input).

## First sound

1. Load ATMD. It opens with the Init preset: oscillator 1 playing a sawtooth through a half-open filter, with a short filter contour on each note.
2. Play a few notes on your MIDI keyboard, or click the keys on screen.
3. Turn **CUTOFF FREQUENCY** in the MODIFIERS section down and **EMPHASIS** up: the sound gets darker and starts to whistle.
4. Turn on the second oscillator: in the MIXER, click the third blue ON rocker from the top, then turn up the second VOLUME knob. Turn the oscillator 2 FREQUENCY knob a little away from 0 and listen to the two oscillators beat against each other.
5. Click the preset name in the top bar and pick a sound from the Bass or Lead menu.
6. Drag the editor's lower right corner to make the window bigger or smaller. It keeps its proportions, from half to one and a half times its full size.

## A tour of the interface

The editor has a top bar, the instrument itself and a help bar at the bottom. **PANEL** shows the instrument as it was; **SETTINGS** lays a black plate with the extensions over the front panel, while the wheels and the keyboard stay in view and keep working.

![The front panel with the Init preset: CONTROLLERS, OSCILLATOR BANK, MIXER, MODIFIERS and OUTPUT.](img/panel-init.jpg)

The front panel reads from left to right in the order of the signal: the CONTROLLERS set the master tuning, the glide and the modulation; the OSCILLATOR BANK makes the sound; the MIXER chooses what goes into the filter and how loud; the MODIFIERS shape it with the filter and the two contours; OUTPUT sets the level. The left-hand controller beside the keys holds the GLIDE and DECAY switches and the PITCH and MOD wheels.

### Top bar

![The top bar: preset selector, SAVE, the PANEL and SETTINGS tabs and MANUAL.](img/top-bar.jpg)

- **<** and **>** step through the presets. The list wraps around and includes your own presets at the end.
- The wide button in the middle shows the name of the current preset. Click it to open the preset menu.
- **SAVE** stores the current sound as a user preset.
- **PANEL** and **SETTINGS** switch between the two pages.
- **MANUAL**, at the right end, opens this manual inside the plugin, in English or Spanish. Press Esc or the cross to close it.

![This manual open inside the plugin.](img/manual.jpg)

### Help bar

![The help bar: what the control under the mouse does, and on the right the voices sounding, the CPU load and the version.](img/help-bar.jpg)

Hover over any knob, rocker or wheel and the help bar shows its name, its value and a short note on what it does. With nothing under the mouse it reminds you of the computer keys. On the right it shows how many voices are sounding, the CPU load and the version number.

### Working with the controls

- **Knobs**: drag up or down. Hold Shift while dragging for fine adjustment. The mouse wheel turns the knob under the pointer (Shift for smaller steps). Double-click a knob to return it to its default value.
- **RANGE and WAVEFORM selectors**: click to move one position clockwise; after the last position it goes back to the first. Shift-click or right-click moves one position back. You can also drag up or down across the positions.
- **Rockers**: click to switch them. A rocker is on when its ON side is pressed in.
- **PITCH wheel**: drag it up or down. It springs back to the centre when you let go.
- **MOD wheel**: drag it up or down. It stays where you leave it; fully down is no modulation.
- **Keys**: click a key to play it. Clicking lower on a key, nearer to you, plays louder. Drag across the keys for a glissando.

The jacks on the left-hand controller and the PHONES jack are part of the picture and do nothing. **POWER** is always on in the plugin.

### Playing without a MIDI keyboard

The computer keyboard plays ATMD while its window has the keyboard focus (click on the editor first; some DAWs keep the computer keys for themselves).

- A, S, D, F, G, H, J and K play the white keys of one octave from C, and W, E, T, Y and U the black keys. O, L and P continue with C sharp, D and D sharp above.
- Z moves the octave down and X moves it up. At the start, A plays C3, one octave below middle C, and the octave goes from C0 to C6.

## Controllers

![CONTROLLERS and the OSCILLATOR BANK with Lucky Lead: oscillator 3 in LO, free of the keyboard, ready to modulate.](img/controllers-oscillators.jpg)

### TUNE

**TUNE** tunes the three oscillators together by up to 2.5 semitones up or down. At 0 the instrument plays in concert pitch, with A at 440 Hz. Use it with the A-440 tone to tune by ear, or to match a recording that is not at 440 Hz.

### GLIDE

The **GLIDE** knob sets how long the pitch takes to slide from one note to the next, from about 1 ms (0) to about 1 second per octave (10); the middle of the knob gives about a tenth of a second per octave. The knob only works while the **GLIDE** rocker on the left-hand controller is on.

On large jumps the pitch moves at a nearly constant speed, so a two-octave slide takes twice as long as a one-octave slide, and only the last semitone settles in gently. The glide moves the filter's keyboard tracking along with the pitch, so the tone slides too.

### MODULATION MIX and the modulation switches

The Model D has no separate LFO. Its modulation comes from two sources, oscillator 3 and the noise generator, and the MOD wheel decides how much of it reaches the sound.

- **MODULATION MIX** blends the two sources: at 0 only oscillator 3, at 10 only noise, and in the middle both at about 70 % each.
- **OSCILLATOR MODULATION** (the orange rocker on the border with the OSCILLATOR BANK) sends the modulation to the pitch of oscillators 1 and 2, and of oscillator 3 too while OSC. 3 CONTROL is on (with oscillator 3 as the source, it then modulates itself, as on the original). At full wheel, a square wave from oscillator 3 moves them by about 9 semitones up and down.
- **FILTER MODULATION** (the orange rocker on the border between the MIXER and the MODIFIERS) sends the same modulation to the filter cutoff, up to about 1.25 octaves either way.

The two switches can be on together. With both off, the MOD wheel does nothing.

The noise that goes to the modulation is darker than the noise you hear: pink when the WHITE/PINK switch is on WHITE, and red (even darker, a slow rumble) when it is on PINK. It works whether or not the noise is turned up in the mixer.

## Oscillator bank

Three oscillators make the sound. Each has a **RANGE** selector and a **WAVEFORM** selector; oscillators 2 and 3 also have a **FREQUENCY** knob. Oscillator 1 has no FREQUENCY knob: it follows TUNE, and the other two are tuned against it.

### RANGE

**RANGE** sets the octave in organ footages: **LO**, **32'**, **16'**, **8'**, **4'** and **2'**. 8' is concert pitch (the A above middle C plays 440 Hz); each step is one octave. LO is three octaves below 32', five octaves below 8': too low to hear as a pitch, it gives slow clicks and is meant for oscillator 3 as a modulator.

### FREQUENCY

**FREQUENCY** on oscillator 2 tunes it against oscillator 1, by up to 7.5 semitones up or down. Set it a hair away from 0 for the classic slow beating of two detuned oscillators, or tune it to an interval: at +7 it plays a fifth above, at +5 a fourth, at -5 a fourth below. On oscillator 3 it does the same, unless OSC. 3 CONTROL is off (see below).

### WAVEFORM

**WAVEFORM** chooses one of six waves, from left to right:

- **Triangle**: soft and flute-like, few harmonics.
- **Triangle-sawtooth**: a triangle with some sawtooth mixed in, the "shark's tooth" of the original. Rounder than the sawtooth, brighter than the triangle. On oscillator 3 this position is a **reverse sawtooth**, falling instead of rising, which makes downward ramps when it modulates.
- **Sawtooth**: bright and full, all harmonics. The default and the most used.
- **Square**: hollow, odd harmonics only.
- **Wide pulse**: about 29 % duty, a thinner, reedy square.
- **Narrow pulse**: about 15 % duty, thin and nasal.

As on the original, the square and the pulses come out a little louder than the triangle for the same mixer setting.

### OSC. 3 CONTROL

**OSC. 3 CONTROL** is the vertical orange rocker at the bottom left of the OSCILLATOR BANK. On (the default), oscillator 3 is a third sound source that follows the keyboard, glide and the PITCH wheel like the other two.

Off, oscillator 3 ignores the keyboard, glide and the PITCH wheel and stays on a fixed pitch, as a drone or as a modulator. Its FREQUENCY knob then covers 3 octaves up and down instead of 7.5 semitones, and RANGE still sets the octave. In LO, with OSC. 3 CONTROL off, it runs from about 0.3 Hz (FREQUENCY at -7.5) through about 2.7 Hz (0) to about 22 Hz (+7.5): a full LFO range. This is how the Model D makes vibrato, trills and slow sweeps.

> Oscillator 3's VOLUME knob and ON rocker in the mixer only affect what you hear. Turn oscillator 3 off in the mixer when you use it as a modulator, and its modulation keeps working.

## Mixer

![The MIXER with Screamer: two oscillators at 10 and a little noise drive the filter hard.](img/mixer.jpg)

The mixer has five inputs, each with a VOLUME knob from 0 to 10 and its own blue ON rocker. The rockers sit in a column between the oscillator volumes and the two knobs on the right, in this order from top to bottom: oscillator 1, external input, oscillator 2, noise, oscillator 3.

- The three **VOLUME** knobs on the left set the level of oscillators 1, 2 and 3.
- **EXTERNAL INPUT VOLUME** sets the level of the external input, the audio arriving at the plugin's External Input (see Routing audio into the external input).
- **NOISE VOLUME** sets the level of the noise generator.
- The **WHITE/PINK** rocker chooses the colour of the noise you hear: white (bright hiss) when it is up, pink (softer, with more low end) when it is down. It also chooses the modulation noise (see Controllers).
- The **OVERLOAD** lamp lights when the external input drives its preamp into clipping.

### Driving the filter

The mixer feeds the filter directly, and the filter's input stage saturates as the level rises. One oscillator at 10 is only lightly compressed. Two or three oscillators high, or the noise (its path is hotter than the oscillators'), push the filter into a thick, compressed saturation that is a large part of the Model D's character. Keep the volumes around 5 for a clean sound; turn them up for growl. Use the main VOLUME, not the mixer, to set how loud ATMD plays.

With every source off, the filter still has a tiny noise floor to work with, as on the original, so it can sing on its own with high EMPHASIS.

## Modifiers

![The MODIFIERS with Singing Filter: the filter with its contour on top, the loudness contour below, and the routing rockers on the left edge.](img/modifiers.jpg)

The MODIFIERS hold the filter with its contour generator (upper half) and the loudness contour that drives the amplifier (lower half). The three orange rockers on the left edge are FILTER MODULATION (see Controllers) and the two KEYBOARD CONTROL switches.

### FILTER

The filter is a four-pole low-pass, 24 dB per octave, modeled on the Moog transistor ladder. As on the original, raising the resonance thins out the bass, and at the top of its range the filter sings a nearly pure sine wave.

- **CUTOFF FREQUENCY** sets where the filter starts to cut, one octave per division of the dial (from -5 to +5). At -1.1 the cutoff is 440 Hz, at 0 about 940 Hz, at -5 about 30 Hz, and at +5 the filter is wide open. The keyboard tracking and the contour are added on top.
- **EMPHASIS** is the resonance: a peak at the cutoff that makes the filter whistle and sweep. From about 8 the filter oscillates on its own. Below about 300 Hz the resonance weakens, and under about 150 Hz the filter stops oscillating, as on the original.
- **AMOUNT OF CONTOUR** sets how far the filter contour opens the filter above CUTOFF FREQUENCY, up to about 6 octaves at 10. At 0 the contour does nothing to the filter.
- **KEYBOARD CONTROL** 1 and 2 make the cutoff follow the keys. Switch 1 alone gives one third, switch 2 alone two thirds, both together one octave per octave, so a self-oscillating filter plays in tune. The tracking starts at the lowest F of the keyboard, where it adds nothing.

### Filter contour

The contour generator gives each note a shape in three stages: it rises to its peak, falls to a held level, and stays there while you hold the key. There is no separate release knob: what happens after you let go depends on the DECAY rocker.

- **ATTACK TIME** sets how fast the contour rises, from about 1 ms (0) to 10 s (10). The knob is logarithmic, like the original: about 0.15 s at 2, 0.5 s at 4, 1.5 s at 6 and 4 s at 8.
- **DECAY TIME** sets how fast it falls from the peak to the sustain level, over the same range. With the DECAY rocker on, it also sets the release after you let go.
- **SUSTAIN LEVEL** sets the level held while the key is down, from 0 to 10 (the peak).

The attack rises almost in a straight line and stops at the peak; the decay and the release are curved, fast at first and slower at the end. A new attack starts from wherever the contour is, not from zero.

### Loudness contour

The loudness contour has the same three knobs, **ATTACK TIME**, **DECAY TIME** and **SUSTAIN LEVEL**, and works the same way, but it shapes the volume of the note. With SUSTAIN LEVEL at 0 every note dies away even while you hold the key, like a plucked string or a piano; at 10 it plays at full level as long as you hold it, like an organ.

## Output

![OUTPUT: main VOLUME and its ON switch, A-440, the PHONES volume, and POWER.](img/output.jpg)

- **VOLUME** (top) sets the level of the main output. The knob is logarithmic. At 6, the default, the factory presets are leveled to play at about the same loudness.
- The **MAIN OUTPUT** rocker mutes the main output when it is off.
- **A-440** adds a steady 440 Hz sine tone to the output, the reference the original used for tuning. It sounds whether or not you play and even with MAIN OUTPUT off.
- **VOLUME** (bottom, above PHONES) was the headphone level of the original. In ATMD it sets the level of the A-440 tone.
- **POWER** and its lamp are always on in the plugin.

To tune with A-440, play an A, turn on A-440 and turn TUNE (or the FREQUENCY of an oscillator) until the beating between the two tones slows down and stops. The A-440 tone also goes through the delay and the reverb, so switch them off while tuning.

## Left-hand controller

![The left-hand controller: the GLIDE and DECAY rockers and the two wheels. The PITCH wheel is pushed up, so its centre mark sits higher.](img/left-hand.jpg)

### GLIDE and DECAY

- **GLIDE** turns the glide on and off; the GLIDE knob in the CONTROLLERS sets its time. With GLIDE on, every new note slides from the previous one, even when you play detached, because the keyboard holds the last pitch, as on the original.
- **DECAY** decides what happens when you let go of the key. On (the default): both contours fall back with their DECAY TIME, so the note dies away naturally. Off: they fall back in about 15 ms (or faster, if DECAY TIME is shorter), so the note stops almost at once while DECAY TIME still shapes it while you hold the key.

### PITCH and MOD wheels

- The **PITCH** wheel bends oscillators 1 and 2, and oscillator 3 too when OSC. 3 CONTROL is on. It bends up to 7 semitones up or down by default (a fifth); change the range on the SETTINGS page. Push it up to bend up. It springs back to the centre when you let go. The pitch bend of your MIDI controller moves it.
- The **MOD** wheel sets how much modulation from oscillator 3 and noise reaches the oscillators and the filter, through the OSCILLATOR MODULATION and FILTER MODULATION switches. Fully down is no modulation. The depth grows slowly at first and fast near the top, as on the original: at half travel it gives only about an eighth of the full depth, which leaves room for a delicate vibrato. The mod wheel of your MIDI controller (CC 1) moves it.

The wheels are not host parameters and are not stored in your presets. They start at the centre and at 0, except in the factory presets whose sound is the modulation itself (Sub Drop, Laser Zap, Bird Call, Wind and Drone Machine), which load with the MOD wheel already raised.

## Keyboard and voices

![A C major chord held on the Poly Strings preset. The keyboard runs from F2 to C6.](img/keyboard-chord.jpg)

The keyboard on screen has 44 keys, from F2 to C6, like the original. MIDI notes outside that range play too.

### As on the original

With VOICES at 1 ATMD is monophonic, and it behaves like the Model D:

- **Low-note priority**: when you hold several keys, the lowest one sounds. When you release it, the note goes back to the lowest key still held.
- **Single trigger**: the contours start only when you press a key with no other key held. Notes played legato (a new key before you release the previous one) change the pitch without restarting the contours, so a phrase played legato flows as one gesture, and detached notes each get their own attack.

The NOTE PRIORITY and TRIGGER settings change both (see the SETTINGS page).

### Polyphony

With VOICES above 1, each note takes its own voice, and every voice has its own oscillators, filter and contours. Each new note starts its contours, whatever TRIGGER says. When all voices are busy, the oldest note is taken over. With GLIDE on, a voice that is still sounding slides from its last pitch to the new note.

The modulation (oscillator 3 as a modulator, the noise and the wheels) acts on every voice in the same way.

## SETTINGS page

![The SETTINGS page with the Poly Strings preset. The keyboard stays in view and keeps playing.](img/settings-page.jpg)

The SETTINGS page holds the extensions in six groups. On this page, drag the knobs as on the panel (Shift for fine, double-click for the default), click a box to choose a value, and click a rocker to switch it. The current value of each knob is printed above it.

![The upper row of the SETTINGS page: VOICES, EXPRESSION and KEYBOARD.](img/settings-voices.jpg)

### VOICES

- **VOICES** chooses how many notes can sound at once, from 1 (the monophonic original) to 8.
- **UNISON** stacks 1 to 4 voices on every note, for a thicker sound. It works with VOICES at 1 too: one note, up to four voices.
- **UNISON DETUNE** spreads the stacked voices in pitch, from 0 to 30 cents between the outermost two (8 by default).
- **STEREO SPREAD** pans the stacked voices across the stereo field. With several voices and no unison, it places the notes a little to the left, centre and right in turn.
- **ANALOG DRIFT** makes the pitch of every oscillator wander slowly by a few cents, up to about 6 cents at 100 %, as in a warm analog circuit. Each voice drifts on its own, so the voices of a chord or a unison stack differ slightly. At 0 the oscillators hold perfectly steady.

The engine has 16 voices in all. VOICES times UNISON never goes above that: when it would, the unison is reduced (with 8 voices, 2 voices per note at most).

### EXPRESSION

The original had no velocity or aftertouch, so these all start at 0 %.

- **VELOCITY TO FILTER**: harder playing opens the filter and softer playing closes it, up to about two octaves between the softest and the hardest note at 100 %. A medium velocity, around 90, leaves the filter as set.
- **VELOCITY TO LOUDNESS**: softer playing is quieter. At 100 % the volume follows velocity fully.
- **PRESSURE TO FILTER**: pressing the keys harder after the note has started (aftertouch) opens the filter, up to 2 octaves.
- **PRESSURE TO MOD.**: aftertouch adds to the MOD wheel, so you can bring in vibrato by pressing into a key. The OSCILLATOR MODULATION or FILTER MODULATION switch must be on, as for the wheel.
- **PITCH WHEEL RANGE (SEMITONES)** sets how far the PITCH wheel bends: 1, 2, 3, 5, 7 (the default, as on the original), 12 or 24 semitones up and down.

With VOICES at 1 and single trigger, legato notes keep the velocity of the first note, because the contours do not restart.

### KEYBOARD

These two settings apply with VOICES at 1.

- **NOTE PRIORITY** chooses which held key sounds: **LOW** (the original), **LAST** (the most recent key) or **HIGH** (the highest key).
- **TRIGGER** chooses when the contours restart. **SINGLE** (the original): only when you press a key with no other key held. **MULTI**: also on every key you press that changes the sounding note, so legato notes get their own attack. Going back to a held key when you release one never restarts the contours.

![The lower row of the SETTINGS page with Slow Sweep Pad: DELAY, REVERB and SYSTEM.](img/settings-effects.jpg)

### DELAY

- **ON** switches the delay on or off. It is a ping-pong delay: the first repeat comes from the left and each repeat crosses to the other side.
- **TIME** sets the delay time in note values of the host tempo: 1/16, 1/8 T, 1/8, 1/8 D (default), 1/4 T, 1/4, 1/4 D, 1/2 or 1 bar. In the standalone it follows 120 BPM.
- **FEEDBACK** sets how many repeats you hear.
- **TONE** darkens the repeats at low values and keeps them bright at high values.
- **MIX** sets the level of the repeats.

### REVERB

- **ON** switches the plate reverb on or off.
- **SIZE** sets the length of the tail.
- **DAMPING** darkens the tail.
- **MIX** sets the level of the reverb.

The delay comes first, then the reverb.

### SYSTEM

- **OVERSAMPLING** runs the voices at 1x, 2x, 4x (default) or 8x the sample rate. Higher values keep bright, resonant sounds and fast modulation from oscillator 3 cleaner, and cost more CPU. Changing it restarts the engine, so notes that are sounding stop.
- **OUTPUT** sets the final level after the effects, from -24 to +6 dB.

OVERSAMPLING and OUTPUT are your own preferences: loading a preset never changes them.

## Routing audio into the external input

The Model D could filter any sound through its mixer and filter: a guitar, a drum loop, a voice. ATMD has the same EXTERNAL INPUT, fed by the plugin's optional stereo input, named External Input (the two channels are mixed to mono). In most DAWs it appears as a side-chain input of the instrument.

1. Choose the source in your DAW. In Ableton Live, open the Sidechain section in the title bar of the ATMD device and choose the track you want to process. In other hosts, look for the side-chain or input routing of the instrument.
2. In the MIXER, turn on the external input's ON rocker (the second one from the top).
3. Turn up **EXTERNAL INPUT VOLUME** until the **OVERLOAD** lamp lights on the loudest moments. The input goes through a high-gain preamp, so a little goes a long way; further up, the preamp clips and distorts.
4. Hold a key. The external signal goes through the filter and the loudness contour, so you only hear it while a note plays. For a steady filter, set the loudness SUSTAIN LEVEL to 10 and turn the oscillators off in the mixer, or keep them on to mix the synth with the input.

The OVERLOAD lamp watches the preamp, before the mixer's ON rocker, so you can set the level with the input switched off; it stays lit for about a third of a second after each clip. In the standalone app the external input is the audio input chosen in Options.

### The feedback loop

Owners of the original often patched the main output back into the external input. The signal then runs around the mixer and the filter a second time, which fattens and roughens the sound and changes how the resonance behaves. You can do the same in a DAW if it lets you route a track back into its own instrument:

1. Add a send from the ATMD track to a return track (or an aux bus).
2. Choose that return track as the source of ATMD's External Input.
3. Turn up the send, switch on the external input in the MIXER and raise **EXTERNAL INPUT VOLUME** slowly from 0, with the main VOLUME at 6 or lower.

Because the signal makes a round trip through the host, it comes back at least one audio buffer late, a few milliseconds. The loop therefore also colours the sound like a very short echo, and at high settings it can howl on its own; smaller buffer sizes bring it closer to the original. Some hosts refuse this routing, because it feeds a track back into itself.

> Start with the send and EXTERNAL INPUT VOLUME low and your monitors quiet. A feedback loop can get loud very quickly.

## Presets

ATMD comes with 32 factory presets: Init plus 31 sounds in five categories. A preset stores every knob and rocker on the panel and every setting on the SETTINGS page, except OVERSAMPLING and OUTPUT. It does not store the wheels.

### Choosing a preset

- Click **<** or **>** in the top bar to step through all presets.
- Click the preset name to open the menu. Each category is a submenu; your own presets are under User.
- **Show user presets folder** opens the folder where your presets are stored.

### Saving your own

1. Click **SAVE**.
2. Type a name (the dialog suggests the current one) and click Save, or press Return.

The preset is stored as a `.json` file in the user folder and appears under User in the menu. Saving with the name of an existing user preset replaces it without asking.

- macOS: `~/Library/Application Support/Audio Tools/ATMD/Presets`
- Windows: `%APPDATA%\Audio Tools\ATMD\Presets`

To delete or rename a user preset, do it in that folder. Preset files you copy into the folder from elsewhere appear the next time ATMD opens. Your DAW project stores the full state of ATMD, whether or not you saved a preset.

### Factory presets

| Category | Presets |
|---|---|
| Init | Init |
| Bass | Fat Bass, Taurus Low, Plucked Bass, Squelch Bass, Rubber Bass, Moog Thump, Funk Clav Bass, Sub Drop |
| Lead | Lucky Lead, Prog Solo, Square Whistle, Singing Filter, Fifth Lead, Thin Pulse Lead, Screamer, Detuned Saws |
| Brass and keys | Brass Section, Soft Horn, Organ Keys, Electric Piano, Harpsichord |
| Effects and percussion | Kick, Snare, Hi-Hat, Wind, Laser Zap, Bird Call |
| Drones and pads | Poly Strings, Unison Stack, Slow Sweep Pad, Drone Machine |

- **Bass**: the sound the Model D is best known for. Fat Bass stacks the three oscillators, with oscillator 3 an octave below; Taurus Low goes down to 32' with a slow glide; Squelch Bass pushes EMPHASIS to the edge; Plucked Bass and Funk Clav Bass have DECAY off for short, tight notes.
- **Lead**: Lucky Lead and Prog Solo are the classic glide leads of the 1970s; Singing Filter lets the resonance sing over the oscillator; Fifth Lead tunes oscillator 2 a fifth above; Screamer drives the mixer hard.
- **Brass and keys**: the slower filter attacks of Brass Section and Soft Horn, and the shorter, velocity-sensitive Electric Piano.
- **Effects and percussion**: Kick is the self-oscillating filter swept down by its contour, with no oscillator at all; Snare and Hi-Hat are mostly noise.
- **Drones and pads**: these use the extensions. Poly Strings and Slow Sweep Pad play 6 voices with reverb, Unison Stack stacks 4 voices on every note, and Drone Machine sweeps the filter slowly with oscillator 3.

> In Lucky Lead and Square Whistle, oscillator 3 is set up for vibrato that you add with the MOD wheel, as on the original. Sub Drop, Laser Zap, Bird Call, Wind and Drone Machine load with the wheel already raised, because the modulation is their sound. Move the wheel slowly: the effect grows fast near the top.

## MIDI

| Message | Effect |
|---|---|
| Notes | Play, on any channel, with velocity |
| Pitch bend | Moves the PITCH wheel |
| CC 1, mod wheel | Moves the MOD wheel |
| CC 64, sustain pedal | Holds the notes while the pedal is down |
| Channel pressure and polyphonic pressure | Aftertouch, for PRESSURE TO FILTER and PRESSURE TO MOD. |
| CC 120, All Sound Off | Stops every note at once |
| CC 121, Reset All Controllers | Returns the wheels and aftertouch to rest and releases the pedal |
| CC 123, All Notes Off | Releases every note |

- Velocity matters only as much as VELOCITY TO FILTER and VELOCITY TO LOUDNESS allow; at their default of 0 % every note sounds the same, as on the original.
- Polyphonic pressure is not applied per note: the last value received acts on all voices.
- With VOICES at 1, keys held by the sustain pedal still count as held for the note priority.
- Bypassing the plugin releases every note, so nothing is left hanging when you turn it back on.

## Recipes

### Fat bass

1. Choose Init.
2. Set **RANGE** of oscillators 1 and 2 to 16' and of oscillator 3 to 32'. Set oscillator 3's **WAVEFORM** to square.
3. Turn oscillator 2's **FREQUENCY** just a touch above 0, about +0.1, for a slow beating.
4. In the MIXER, switch on oscillators 2 and 3 and set the volumes to 8, 7 and 5.
5. Set **CUTOFF FREQUENCY** to -1.5, **EMPHASIS** to 2.5 and **AMOUNT OF CONTOUR** to 6.5. Switch on **KEYBOARD CONTROL** 1.
6. Set the filter contour to ATTACK 0, DECAY 3.6, SUSTAIN 2.5, and the loudness contour to ATTACK 0, DECAY 4.5, SUSTAIN 8.
7. Play low notes. Raise AMOUNT OF CONTOUR for more snap, or lower the oscillator volumes for a cleaner sound.

This is the Fat Bass preset. For the classic rubbery slide, turn on the **GLIDE** rocker, set the GLIDE knob to about 2 and play legato: the notes slide without a new attack.

### Lead with vibrato on the wheel

1. Choose Init. Switch on oscillator 2 in the MIXER at 7, with **FREQUENCY** at about +0.1.
2. Set oscillator 3's **RANGE** to LO and **WAVEFORM** to triangle, and switch **OSC. 3 CONTROL** off. Set its **FREQUENCY** to about +1, for a vibrato of about 3.5 times per second. Leave oscillator 3 off in the mixer.
3. Switch on **OSCILLATOR MODULATION** and leave **MODULATION MIX** at 0.
4. Set **CUTOFF FREQUENCY** to 0, **EMPHASIS** to 3 and **AMOUNT OF CONTOUR** to 4.5, with both **KEYBOARD CONTROL** switches on.
5. Turn on the **GLIDE** rocker and set the GLIDE knob to about 3.5.
6. Play a melody, mostly legato. On long notes, push the MOD wheel up slowly past the middle to bring in the vibrato.

Compare it with Lucky Lead. For a filter wah instead of vibrato, switch FILTER MODULATION on and OSCILLATOR MODULATION off. For a trill, set oscillator 3's WAVEFORM to square.

### The filter as an instrument

1. Choose Init and switch oscillator 1 off in the MIXER, so nothing goes into the filter.
2. Set **EMPHASIS** to 10 and **AMOUNT OF CONTOUR** to 0. The filter now sings a sine wave on its own.
3. Switch on both **KEYBOARD CONTROL** switches, so it follows the keys one octave per octave.
4. Set **CUTOFF FREQUENCY** to about -3.4 and play the A above middle C.
5. Switch on **A-440** and turn CUTOFF FREQUENCY slowly until the beating between the two tones stops. Switch A-440 off.
6. Play melodies from about A3 upward. Below about 150 Hz the filter stops singing, as on the original.

Turn on **GLIDE** for a theremin-like slide. Mix a little oscillator 1 back in and the filter rings with it.

### Oscillator 3 as an FM source

1. Choose Init. Set oscillator 3's **RANGE** to 8' and leave **OSC. 3 CONTROL** on, so oscillator 3 follows the keys at audio rate.
2. Switch on **OSCILLATOR MODULATION** with **MODULATION MIX** at 0.
3. Set oscillator 1's **WAVEFORM** to triangle and **CUTOFF FREQUENCY** to +2.
4. Hold a key and raise the MOD wheel. Oscillator 1 turns metallic and bell-like as oscillator 3 modulates it faster than you can hear as vibrato.
5. Turn oscillator 3's **FREQUENCY** to change the colour: at 0, +7 and +5 it stays harmonic, in between it gets clangorous.

Switch FILTER MODULATION on as well for a growling, buzzing filter. Raise **OVERSAMPLING** to 8x if the high notes sound harsh.

### Wind

1. Choose Init. Switch oscillator 1 off and the noise on in the MIXER, with **NOISE VOLUME** at 8, and set the noise rocker to **PINK**.
2. Set **CUTOFF FREQUENCY** to -1, **EMPHASIS** to 7 and **AMOUNT OF CONTOUR** to 0.
3. Set the loudness contour to ATTACK 4.5, DECAY 5 and SUSTAIN 10.
4. Set oscillator 3 to LO, triangle, **OSC. 3 CONTROL** off and **FREQUENCY** at -6, for a slow wave. Switch on **FILTER MODULATION** with **MODULATION MIX** at 0.
5. Hold a key and raise the MOD wheel. The resonant peak of the filter sweeps up and down with oscillator 3, like gusts of wind.

This is the Wind preset. Turn MODULATION MIX toward 10 and the sweep becomes random, driven by the red noise.

### Polyphonic strings

1. Choose Init. Switch on oscillator 2 in the MIXER at 7, with **FREQUENCY** at about +0.1.
2. Set **CUTOFF FREQUENCY** to -0.6, **EMPHASIS** to 1 and **AMOUNT OF CONTOUR** to 2.5. Set the filter contour to ATTACK 4, DECAY 5.5, SUSTAIN 6.
3. Set the loudness contour to ATTACK 4.5, DECAY 6, SUSTAIN 9.
4. On the SETTINGS page, set **VOICES** to 6, **ANALOG DRIFT** to about 50 % and **STEREO SPREAD** to 70 %.
5. Switch the reverb **ON** with **MIX** at 25 %.
6. Play slow chords.

Compare it with Poly Strings. For a wider sound, set **UNISON** to 2 and UNISON DETUNE to about 12 cents.

## Troubleshooting

### No sound

- Check the main **VOLUME**, the **MAIN OUTPUT** rocker and **OUTPUT** on the SETTINGS page.
- Make sure at least one source is on in the MIXER, both its ON rocker and its VOLUME knob, or that EMPHASIS is high enough for the filter to sing.
- With **CUTOFF FREQUENCY** near -5 and AMOUNT OF CONTOUR at 0 the filter is closed. Turn it up.
- With the loudness **SUSTAIN LEVEL** at 0, held notes die away after the decay. That is normal.
- In your DAW, make sure the track receives MIDI and is armed or monitored.

### The modulation does nothing

Raise the MOD wheel: at 0 there is no modulation, and the depth grows mostly in the upper half of its travel. Check that OSCILLATOR MODULATION or FILTER MODULATION is on, and that MODULATION MIX points at the source you want. If oscillator 3 is the source, check its RANGE: in LO with OSC. 3 CONTROL off it gives vibrato; at 8' it gives fast, buzzing modulation.

### Oscillator 3 does not follow the keys

**OSC. 3 CONTROL** is off. Switch it on, or use the FREQUENCY knob to set its pitch by hand.

### Notes do not restart, or the pitch jumps back to another note

That is the original's low-note priority and single trigger. Lift your fingers between notes for a new attack, or set **TRIGGER** to MULTI and **NOTE PRIORITY** to LAST on the SETTINGS page.

### Notes slide when I do not want them to

The **GLIDE** rocker on the left-hand controller is on. Switch it off, or turn the GLIDE knob down.

### Notes stop abruptly when I release the key

The **DECAY** rocker is off. Switch it on to let notes fade with the DECAY TIME.

### A steady tone does not stop

A-440 is on. Switch it off in the OUTPUT section.

### The sound is distorted

Several mixer sources are high and saturate the filter, which is part of the character. Lower the mixer volumes, and set the level with the main VOLUME instead. If the OVERLOAD lamp lights, lower EXTERNAL INPUT VOLUME.

### The external input is silent

Check that the source is routed to ATMD's External Input in your DAW, that the external input's ON rocker in the MIXER is on and its VOLUME above 0, and that you hold a key. In the standalone app, choose the audio input in Options.

### Stuck notes

Send CC 123 (All Notes Off) from your controller, or bypass ATMD and turn it back on.

### ATMD does not appear in the DAW

Check that the files are in the folders given in Installing, and rescan. On macOS run the `xattr` command from Installing. On Windows, copying to the VST3 folder needs administrator rights.

### High CPU

Set **OVERSAMPLING** to 2x, use fewer voices or less unison, and switch off the delay and the reverb if you do not use them.

## Specifications

| Item | Value |
|---|---|
| Inspired by | Moog Minimoog Model D (1970-1981), a mid-1970s unit with the Rev 2 oscillator board |
| Formats | VST3, Audio Unit (macOS), standalone |
| Systems | macOS 11 or later (universal), Windows 10 or 11 64-bit |
| Type | Instrument with MIDI in, optional stereo external input |
| Output | Stereo (mono bus supported) |
| Voices | 1 (original) to 8; unison 1 to 4; 16 in all |
| Keyboard | 44 keys on screen, F2 to C6; low, last or high priority; single or multi trigger |
| Oscillators | 3; triangle, triangle-sawtooth (reverse sawtooth on oscillator 3), sawtooth, square, 29 % and 15 % pulse |
| Ranges | LO, 32', 16', 8', 4', 2'; 8' is concert pitch; LO is 3 octaves below 32' |
| Tuning | TUNE +/-2.5 semitones; oscillators 2 and 3 +/-7.5 semitones; oscillator 3 +/-3 octaves without keyboard control |
| Mixer | 3 oscillators, external input with OVERLOAD lamp, white or pink noise; saturates the filter when driven |
| Filter | Four-pole transistor ladder, 24 dB per octave; 1 octave per division, -1.1 = 440 Hz |
| Self-oscillation | From EMPHASIS of about 8; in tune with both KEYBOARD CONTROL switches on, down to about 150 Hz |
| Filter contour | Up to about 6 octaves |
| Contours | Attack, decay and sustain; time constants about 1 ms to 10 s; release with DECAY, or about 15 ms without |
| Glide | About 1 ms to 1 s per octave |
| Modulation | Oscillator 3 and pink or red noise, through the MOD wheel to the oscillators (3 only with OSC. 3 CONTROL on) and to the filter |
| Pitch wheel | 7 semitones up and down by default; 1 to 24 |
| Effects | Tempo-synced ping-pong delay, plate reverb |
| Oversampling | 1x, 2x, 4x or 8x |
| Presets | 32 factory presets, plus user presets |
| CPU | About 0.9 % of one core in mono, 2.8 % with 4-voice unison, 3.4 % for five-note chords on 6 voices (Apple M4, 4x) |
| Tail | 12 s |
| License | GNU AGPL v3 |

Moog and Minimoog are trademarks of Moog Music Inc. Audio Tools is an independent project, not affiliated with or endorsed by Moog Music. Product names are used only to identify the historical instrument that inspired this recreation. The panel graphics are our own.
