# AT101V user manual

## What it is

AT101V is a free, open-source monophonic synthesizer modeled on the Roland SH-101 of 1982. It has the same front panel: one oscillator with a sub-oscillator and noise, a four-pole low-pass filter that self-oscillates, a simple envelope, a modulator that doubles as the clock, an arpeggiator and a sequencer that records up to 100 steps from the keyboard.

On top of the original it adds a step editor with accent, slide and probability on every step, up to 8 voices and 4-voice unison, sync to your DAW with swing, velocity, MIDI learn, MIDI out, drive, chorus, delay, reverb, three body colours and 44 factory presets.

AT101V runs as a VST3 plugin, as an Audio Unit on macOS, and as a standalone application on macOS and Windows.

![The SYNTH page: the front panel and the 32-key keyboard with the 101 Bass preset.](img/synth-panel.jpg)

## Installing

Download the zip file for your system from the Audio Tools website. Both packages include an `INSTALL.txt` file with the same steps as below.

### macOS

AT101V needs macOS 11 or later. One universal binary runs on Apple silicon and Intel Macs.

1. Unzip the file.
2. Copy `AT101V.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
3. Copy `AT101V.component` to `~/Library/Audio/Plug-Ins/Components/`.
4. Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan.
5. `AT101V.app` is the standalone version. Copy it to Applications if you want to use it without a DAW.

The binaries are signed ad hoc and not notarized, so macOS may refuse to open them the first time. If that happens, open Terminal and run this command, then rescan again:

`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/AT101V*.vst3 ~/Library/Audio/Plug-Ins/Components/AT101V*.component`

### Windows

AT101V needs Windows 10 or 11, 64-bit.

1. Unzip the file.
2. Copy the `AT101V.vst3` folder to `C:\Program Files\Common Files\VST3\`. Windows asks for administrator rights.
3. Rescan plugins in your DAW.
4. `AT101V.exe` is the standalone version and runs without a DAW.

The binaries are not code-signed, so Windows SmartScreen may ask you to confirm the first time you run them.

## Loading it in a DAW

AT101V is an instrument: put it on a MIDI or instrument track, not on an audio track. It has a stereo output (it also works on a mono bus), receives MIDI on any channel and can send MIDI out.

### Ableton Live

1. After rescanning, open the browser and choose Plug-Ins.
2. Open Audio Units (macOS) or VST3, then the Audio Tools folder.
3. Drag AT101V onto an empty MIDI track, or onto the empty area of the session to create a new one.
4. Arm the track and play your MIDI keyboard. Click the wrench icon in the device title bar if the editor window is closed.

> On Windows, if AT101V does not appear, open Settings, Plug-Ins, turn on Use VST3 Plug-in System Folders and rescan.

To record the notes that the arpeggiator or the sequencer plays as MIDI clips, see MIDI out in the MIDI section. Live takes MIDI out only from the VST3, not from the Audio Unit (REAPER too).

Factory presets appear in Live's browser under Plug-Ins → Audio Tools AT101V (expand the plugin), installed by the launcher or from the zip's Presets folder. Live doesn't show a plugin's own program list; the preset menu in the editor has them all.

Live fills the device panel by itself only for plugins with up to 64 automatable parameters, so Oversampling and MIDI Out, two session settings, are not offered for automation. They stay on the Settings page and in presets and projects. Configure adds any parameter to the device panel.

### Other DAWs

In any other host, insert AT101V as an instrument and send it MIDI. Every control on the panel and on the Settings page is a host parameter, so you can automate it (except Oversampling and MIDI Out). The factory and user presets also appear in the program list of hosts that show one (REAPER, Logic, Bitwig; not Ableton Live, see above).

### Standalone

Open `AT101V.app` or `AT101V.exe`. Use the Options button at the top of the window to choose the audio output and the MIDI input. Without a MIDI keyboard you can play the keyboard on screen with the mouse. In the standalone there is no host tempo, so everything that follows the tempo runs at 120 BPM.

## First sound

1. Load AT101V. It opens with the Init preset: a sawtooth through a half-open filter.
2. Play a few notes on your MIDI keyboard, or click the keys on screen.
3. Pull the **FREQ** fader of the VCF down and push **RES** up: the sound gets darker and starts to whistle.
4. Click the preset name in the top bar and pick a sound from the Bass or Seq menu. The Seq presets play a sequence on their own as soon as they load.
5. Drag the editor's lower right corner to make the window bigger or smaller. It keeps its proportions, from half to one and a half times its full size.

## A tour of the interface

The editor has a top bar, three pages and a lower bar. **SYNTH** is the instrument as it was in 1982, **SEQUENCER** is the step editor, and **SETTINGS** holds everything the original never had.

### Top bar

![The top bar: preset selector, SAVE, RANDOM and the three page tabs.](img/top-bar.jpg)

- **<** and **>** step through the presets. The list wraps around and includes your own presets at the end.
- The wide button in the middle shows the category and the name of the current preset. Click it to open the preset menu.
- **SAVE** stores the current sound as a user preset.
- **RANDOM** rolls a random patch within musical ranges. It keeps the sequence, the settings, the arpeggio and HOLD, and leaves TUNE, VOLUME, the bender and the transpose switches alone.
- The three tabs on the right switch between the pages.
- **MANUAL**, at the right end, opens this manual inside the plugin, in English or Spanish. Press Esc or the cross to close it.

### Lower bar

![The lower bar: voices, MIDI activity, clock source, key transpose, keyboard octave and the sequencer step.](img/lower-bar.jpg)

From left to right:

- **VOICES** shows Mono, Poly 2, Poly 4, Poly 6 or Poly 8, and the unison count when unison is on.
- **MIDI IN** and **MIDI OUT** light up when MIDI comes in or goes out.
- **CLOCK** shows the clock source: Modulator or Host tempo.
- **KEY TR** shows the key transposition in semitones, or "press a key" while KEY TRANSPOSE waits for a key.
- **OCTAVE** with its **-** and **+** buttons shifts the keyboard on screen by up to three octaves either way. It only affects the mouse keyboard, not MIDI.
- **STEP** shows the current step while the sequencer plays, and **LOADING** shows how many of the 100 steps you have entered while recording.
- **CPU** shows the processor load, followed by the version number.

### Working with the controls

- Drag a fader or a knob up or down. Hold Shift while dragging for fine adjustment.
- Click a switch position, or drag along the switch.
- The mouse wheel moves any fader, knob or switch under the pointer.
- Double-click a control to return it to its default value.
- While you touch a control, a bubble shows its name and value, in real units where it helps (Hz, ms, s, semitones, cents).
- Right-click a control (or Ctrl-click on macOS) for MIDI Learn, to clear its MIDI CC, or to reset it.
- On the keyboard on screen, clicking nearer the front of a key plays louder. Dragging across the keys plays them one after another.

## Modulator and tune

![TUNE, the MODULATOR and the VCO.](img/panel-modulator-vco.jpg)

### TUNE

The **TUNE** knob tunes the whole instrument by up to 50 cents up or down. The centre position is concert pitch.

### MODULATOR

The modulator is the SH-101's LFO, and it is also the internal clock of the arpeggiator and the sequencer. The red LED next to the rate fader blinks at its speed.

- **LFO/CLK RATE** sets the speed from 0.1 Hz (0) to 30 Hz (10). When the modulator is the clock, this is also the tempo: one arpeggio note or sequencer step per cycle.
- **WAVE FORM** chooses what the modulator sends to the VCO and the VCF: triangle, square, RANDOM (a new random value on every clock step, the classic sample and hold) or NOISE.

The pulse-width modulation and the bender's LFO push always use the triangle, whatever WAVE FORM says.

## VCO

The single oscillator produces a rising sawtooth and a pulse wave from the same core, plus the sub-oscillator and noise that you mix in the next section.

- **MOD** sets how much the modulator moves the pitch, up to one octave either way at 10. The curve is gentle at the bottom, so small values give a subtle vibrato and the top of the fader gives sirens and trills.
- **RANGE** sets the octave: 16', 8', 4' or 2'. 8' is the normal pitch.
- **PULSE WIDTH** sets the shape of the pulse wave. What it does depends on the switch next to it.
- The pulse-width source switch has three positions. **LFO**: the triangle of the modulator sweeps the width, and the fader sets the depth. **MAN**: the fader sets a fixed width, from a square wave (50 %) at 0 to a thin 3 % pulse at 10. **ENV**: the envelope sweeps the width, and the fader sets the depth.

> With LFO or ENV as the source and PULSE WIDTH near 10, the pulse gets so thin that it disappears for a moment. That is how the original behaves.

## Source mixer

![The SOURCE MIXER and the VCF.](img/panel-mixer-vcf.jpg)

Four faders set the level of each source going into the filter. They can be mixed freely.

- **Pulse** (the square symbol) is the pulse wave from the VCO.
- **Saw** (the ramp symbol) is the sawtooth.
- **SUB OSC** is the sub-oscillator, locked in phase to the VCO.
- The sub-oscillator switch chooses **1 OCT DOWN** (a square one octave below), **2 OCT DOWN** (a square two octaves below) or the third position, a narrow 25 % pulse two octaves below with a hollow, nasal tone.
- **NOISE** is white noise. Mixed in a little it adds breath; alone, with the filter, it makes wind, snares and effects.

With all four faders at 0 the filter still has a tiny noise floor to work with, as on the original, so it can self-oscillate with nothing mixed in.

## VCF

The filter is a four-pole low-pass, 24 dB per octave, modeled on the IR3109 chip of the original. Its resonance thins the bass as it rises and turns into a pure sine near the top.

- **FREQ** sets the cutoff frequency, from 10 Hz (0) to 20 kHz (10).
- **RES** sets the resonance. Around 9 the filter starts to self-oscillate.
- **ENV** sets how far the envelope opens the filter, up to 10 octaves above FREQ.
- **MOD** sets how much the modulator moves the cutoff, up to 3 octaves either way.
- **KYBD** sets how much the cutoff follows the keyboard. At 10 it follows note for note (one octave per octave, around middle C), so a self-oscillating filter plays in tune. At 0 every note gets the same cutoff.

The TRANSPOSE switch moves the keyboard tracking too; RANGE and KEY TRANSPOSE do not.

## VCA and envelope

![The VCA and the ENV.](img/panel-vca-env.jpg)

### VCA

The **VCA** switch chooses what shapes the volume. **ENV**: the envelope. **GATE**: the sound is fully on while a key is held and stops when you let go, like an organ. In GATE the envelope still moves the filter and the pulse width.

### ENV

There is one envelope, shared by the filter, the amplifier and the pulse width.

- The trigger switch sets when the envelope starts, and also the note priority in mono. **GATE+TRIG**: every new key restarts the envelope, even played legato; the last key pressed wins. **GATE**: legato notes do not restart the envelope; the lowest held key wins. **LFO**: while a key is held, the envelope restarts on every clock step (every cycle of the modulator, or every step of the host tempo when the clock follows the host); the lowest held key wins.
- **A** (attack) from about 1 ms to 4 s.
- **D** (decay) from about 2 ms to 10 s.
- **S** (sustain) sets the level held while the key is down, 0 to 10.
- **R** (release) from about 2 ms to 10 s, after the key is released.

## Performance controls

![The performance panel: VOLUME, PORTAMENTO, the BENDER faders, the PORTAMENTO and TRANSPOSE switches and the bender lever.](img/panel-performance.jpg)

The block to the left of the keyboard holds the controls you play with.

- **VOLUME** sets the output level of the synth.
- **PORTAMENTO** sets the glide time between notes, from 0 to about 5 s.
- The **PORTAMENTO** switch sets when the glide happens. **AUTO**: only when you play legato, that is, when you press a new key while the previous one is still held. **OFF**: never. **ON**: always.
- The **TRANSPOSE** switch shifts the whole instrument by one octave: **L** down, **M** normal, **H** up. It also moves the filter's keyboard tracking.
- The **BENDER** faders set how far the lever bends. **VCO** bends the pitch up to one octave either way at 10 (at the default of 2, about 2.4 semitones). **VCF** bends the cutoff up to two octaves either way.
- The **LFO** fader (with the triangle symbol) sets the depth of vibrato and growl when you push the lever forward: up to about three quarters of a semitone on the pitch and one octave on the cutoff.
- The **BENDER** lever: drag it left or right to bend, drag it up to push it forward. It springs back to the centre when you let go. The pitch bend of your MIDI controller moves it left and right, and the mod wheel pushes it forward.

> With the arpeggiator and the sequencer, portamento glides only in ON; AUTO acts as OFF. Steps with SLIDE always glide.

### POWER and the function buttons

![The function row: POWER, the SEQUENCER buttons LOAD and PLAY, the ARPEGGIO buttons, HOLD and KEY TRANSPOSE.](img/panel-buttons.jpg)

- **POWER** switches the sound off and on. When its LED is out, AT101V is silent and all notes are released. It is always on when the plugin opens.
- **LOAD** and **PLAY** (SEQUENCER) record and play the sequence; **DOWN**, **U & D** and **UP** (ARPEGGIO) start the arpeggiator. Both have their own sections below.
- **HOLD** keeps notes sounding after you release the keys. While loading a sequence it works as **LEGATO**.
- **KEY TRANSPOSE** shifts everything you play by the interval of the next key you press. While loading a sequence it works as **REST**.

Each button has a red LED above it. The jacks along the top edge of the panel are part of the picture and do nothing.

### HOLD

In mono, HOLD keeps the last note sounding after you release the key; a new key replaces it. With several voices it works like a sustain pedal for every note. With the arpeggiator it latches the chord, so the arpeggio keeps running with your hands off the keys. The sustain pedal of your MIDI keyboard does the same as HOLD while it is down.

> HOLD holds the sustain level. If S is at 0 and the VCA is on ENV, a held note is silent after its decay.

### KEY TRANSPOSE

KEY TRANSPOSE shifts everything AT101V plays: the keys, held notes, the arpeggio and the running sequence, as on the original.

1. Click **KEY TRANSPOSE**. Its LED blinks and the lower bar says "press a key".
2. Press a key. The interval from middle C (C4) to that key becomes the transposition, from one octave down to two octaves up. With OCTAVE at 0, middle C is the lowest C of the keyboard on screen, the eighth key from the left.
3. The LED stays lit while a transposition is active, and KEY TR in the lower bar shows it in semitones.

To return to normal pitch, transpose again with middle C, or right-click (or Alt-click) the button. Key transposition does not move the filter's keyboard tracking. It is stored with your DAW project, not with presets.

## Voices, unison and note priority

![The VOICES and EXPRESSION sections of the Settings page.](img/settings-voices-expression.jpg)

AT101V is monophonic by default, like the original. The VOICES section of the Settings page makes it polyphonic or thicker.

- **VOICES** chooses Mono, 2, 4, 6 or 8 voices.
- **UNISON** stacks 1, 2, 3 or 4 voices on every note.
- **DETUNE** spreads the stacked voices in pitch, 0 to 50 cents between the outermost two.
- **SPREAD** pans the stacked voices across the stereo field, 0 to 10.
- **DRIFT** adds a slow random wandering of pitch and cutoff, as in an analog circuit, 0 to 10.

Unison works in mono too: one note, up to four detuned voices. The engine has 16 voices in all, so with high unison values the number of notes is limited (for example, 8 voices with unison 4 play 4 notes at once).

### Note priority

In mono, the ENV trigger switch decides which key sounds when you hold several: the last one in GATE+TRIG, the lowest one in GATE and LFO. When you release the sounding key, the note returns to the one that is still held. Portamento in AUTO glides between these legato notes.

With several voices, each new note takes a free voice; when all are busy, the oldest note is stolen. Portamento in AUTO glides when another key is held, from the last pitch of that voice.

### Expression

- **VEL VCF** sets how much key velocity scales the envelope's effect on the filter. At 0, velocity is ignored.
- **VEL VCA** sets how much velocity scales the volume.
- **ACCENT** sets how strongly accented sequencer steps open the filter and push the volume.
- **SLIDE** sets the glide time of sequencer steps with SLIDE, from 20 to 300 ms. If PORTAMENTO is set longer, the longer time wins.

## Arpeggiator

The arpeggiator plays the keys you hold one after another, sorted by pitch, at the speed of the clock.

1. Press **UP**, **U & D** or **DOWN**. Its LED lights. Press the lit button again to switch the arpeggiator off.
2. Hold a chord. The arpeggio starts on the first key, at once.
3. Press **HOLD** if you want it to keep running after you release the keys. A new chord, played after letting go of the old one, replaces it.

- **UP** plays from the lowest note to the highest and starts again.
- **DOWN** plays from the highest to the lowest.
- **U & D** goes up and down without repeating the top and bottom notes: C E G E C E G.

A single key without HOLD (and with ARP OCT at 1) plays normally, as on the original, so you can switch between arpeggio and normal playing by touch. Starting the arpeggiator stops the sequencer, and pressing PLAY switches the arpeggiator off.

### Speed, octaves and articulation

The speed comes from the clock: the modulator's RATE, or the host tempo and DIVISION (see Sync). On the Settings page, **ARP OCT** repeats the arpeggio over 1, 2, 3 or 4 octaves upward.

In mono the gate stays open through the whole arpeggio, so the ENV trigger switch shapes it: GATE+TRIG restarts the envelope on every note, GATE gives one envelope for the whole chord (with S at 0 it fades after the first decay), and LFO restarts the envelope on every clock. With several voices each note of the arpeggio is played on its own voice, so release tails overlap.

The velocity of the arpeggio is that of the last key you pressed. Key transposition shifts a held arpeggio, which is the classic way to play it from the keys.

## Sequencer: recording from the keyboard

The sequencer stores up to 100 steps: notes, rests and ties. As on the original, you can record it from the keyboard without seeing it, or draw it on the SEQUENCER page.

1. Press **LOAD**. Its LED lights, the previous sequence is erased and the lower bar shows LOADING 0 / 100.
2. Play the notes in order, one key per step, on the keyboard on screen or on your MIDI keyboard. Timing does not matter.
3. Press **KEY TRANSPOSE** (REST) to enter a rest step.
4. Press **HOLD** (LEGATO) before a key to tie the previous note to it. A tie to the same pitch makes a longer note; a tie to a different pitch plays legato.
5. Press **LOAD** again to finish. Recording also stops by itself at 100 steps.

You hear each note as you enter it. LOAD always starts a new sequence from step 1; to change a few steps of an existing one, use the SEQUENCER page.

## Sequencer: playback and key transpose

Press **PLAY**. The LED lights and the sequence loops until you press PLAY again. You do not need to hold a key. With the modulator as the clock it starts at once on step 1; with the host tempo it follows the DAW (see Sync). Starting the sequencer switches the arpeggiator off. If the sequence is empty, PLAY does nothing.

- Each step plays for the GATE length set on the Settings page (50 % of the step by default), then the note stops, unless the step is tied or the next step has SLIDE.
- Rests play nothing.
- Tied steps keep the gate open. In GATE+TRIG the envelope still restarts on the tied note; set the trigger switch to GATE for smooth ties. With several voices a tie always restarts the note.
- Steps with SLIDE glide into their note from the previous one without restarting the envelope.
- Accented steps open the filter further and play louder, by the ACCENT amount.
- Steps with a probability below 100 % play only some of the time.

### Transposing with the keys

While the sequence plays, the keys transpose it instead of playing notes. Middle C (C4, the lowest C on screen at OCTAVE 0) plays the sequence as recorded; any other key moves it by the interval from C4. The transposition starts again from zero each time you press PLAY. Turn off **KEY TRANSP** on the Settings page if you prefer to play notes over the running sequence.

KEY TRANSPOSE on the panel works on top of this and stays in effect until you reset it. The bender, portamento and every fader work while the sequence plays.

## The Sequencer page

![The SEQUENCER page playing Acid Line: accents in orange, slides in green, the red LED marks the current step.](img/seq-page.jpg)

The SEQUENCER page shows the sequence as a note grid, with lanes for accent, slide, tie and probability underneath. Changes are heard at once, even while it plays.

### Toolbar

![The toolbar and page buttons of the Sequencer page.](img/seq-toolbar.jpg)

- **LENGTH** shows the number of steps. Drag the display up or down to change it, from 1 to 100.
- **LEN -** and **LEN +** remove or add one step at the end. New steps are rests.
- **SHIFT <** and **SHIFT >** rotate the whole sequence one step left or right.
- **TRANSP -** and **TRANSP +** move every note one semitone; **OCT -** and **OCT +** one octave.
- **RANDOM** writes a new bass line in a minor pentatonic or Dorian scale, with accents, slides, rests and the odd tie (16 steps, or the current length if it is 8 or more).
- **MUTATE** changes a few random steps: an octave jump, an accent, a slide or a rest switched on or off.
- **CLEAR** leaves 16 empty steps (all rests).
- **EXPORT .MID** saves the sequence as a MIDI file, by default `AT101V Sequence.mid` in your Documents folder. Steps are 16th notes, accented notes have velocity 127 and the others 90, and tied notes of the same pitch become one long note.
- **LOAD** and **PLAY** work as on the panel.

### Pages and view

- The page buttons **1-32**, **33-64**, **65-96** and **97-100** show each group of 32 steps.
- **FOLLOW** makes the view jump to the page that is playing. Choosing a page by hand switches FOLLOW off.
- The grid shows two octaves. **-** and **+** next to **LOWEST ROW** move the view by an octave, and the mouse wheel over the grid moves it by a semitone. The display shows the lowest visible note.
- A note outside the view appears as a small arrow at the top or bottom edge of its column.

### Drawing notes

- Click a cell to place that note on that step. You hear it, unless the sequencer is playing.
- Click the same note again to turn the step into a rest.
- Drag across the grid to paint notes, or to erase them if you started on a note.
- Click past the end of the sequence to extend it up to that step; the steps in between become rests.
- Shift-click a step to switch its accent on or off.

### Lanes

![The lanes under the grid: ACCENT, SLIDE, TIE and PROB.](img/seq-lanes.jpg)

- **ACCENT** (orange): the step plays louder and brighter. An accented note also shows an orange line in the grid.
- **SLIDE** (green): the step glides into its note from the previous step without restarting the envelope. A green line joins the two notes in the grid.
- **TIE** (yellow): the gate stays open into the next step.
- **PROB**: click at the height you want to set the chance that the step plays: 0, 25, 50, 75 or 100 %. Notes below 100 % are drawn paler, with their percentage.

Click a lane cell to switch it, or drag along the lane to set several steps at once.

The sequence is saved with presets and with your DAW project.

## Sync: modulator clock or host tempo

![The CLOCK section of the Settings page.](img/settings-clock.jpg)

The arpeggiator and the sequencer step on a clock. The CLOCK section of the Settings page chooses which one.

- **SOURCE Modulator**: the clock is the modulator, as on the original. One step per cycle, at the speed of LFO/CLK RATE (0.1 to 30 steps per second). The first key of an arpeggio and the PLAY button restart the clock, so the first note sounds at once. This clock is free: it does not follow the DAW.
- **SOURCE Host Tempo**: the clock follows the tempo of the DAW. **DIVISION** sets the length of one step: 1/4, 1/8, 1/8 T, 1/16 (default), 1/16 T or 1/32. While the DAW plays, the sequence is locked to the song position (the same step always lands on the same beat) and the first note of an arpeggio waits for the next step of the grid. While the DAW is stopped, the arpeggiator and the sequencer keep running at the host tempo.
- **SWING** (Host Tempo only) delays every second step, from 0 to 75 %. At 75 % the off-beat step arrives 37.5 % of a step late.
- **GATE** sets how long each sequencer step sounds, from 5 to 100 % of the step, with either clock source. The arpeggio is not affected: its gate stays open.
- **ARP OCT** and **KEY TRANSP** are described with the arpeggiator and the sequencer.

> With Host Tempo, the clock steps also drive RANDOM on the modulator and the LFO position of the trigger switch. The tempo-synced delay always follows the host tempo, whichever clock source you choose. In the standalone, the host tempo is 120 BPM.

## Settings page: character, effects and system

![The Settings page with the Trance Gate Arp preset.](img/settings-page.jpg)

The upper row of the Settings page holds VOICES, EXPRESSION and CLOCK, described above. The lower row holds CHARACTER, CHORUS, DELAY, REVERB and SYSTEM.

![The lower row of the Settings page: CHARACTER, CHORUS, DELAY, REVERB and SYSTEM.](img/settings-effects.jpg)

On this page, drag the faders as on the panel (Shift for fine, double-click for the default), click a line of a list to choose it, click a toggle button to switch it, and use **-** and **+** under a display to step through its values.

### CHARACTER

- **DRIVE** pushes the mixer harder into the filter, from clean (0) to thick and saturated (10).
- **OVERSAMPLING** runs the whole voice at 2x, 4x (default) or 8x the sample rate. Higher values keep the bright, resonant sounds cleaner and cost more CPU. This is a global setting: presets do not change it.

### CHORUS

- **MODE** chooses Off, I, II or I+II, after the BBD chorus of classic synths. I is slow and gentle, II is faster and deeper, I+II has a more complex movement.
- **MIX** sets the amount of chorus.

### DELAY

- **ON** switches the delay on or off. It is a ping-pong delay: the first repeat comes from the left and each repeat crosses to the other side.
- **TIME** sets the delay time in note values, synced to the host tempo: 1/32, 1/16 T, 1/16, 1/16 D, 1/8 T, 1/8, 1/8 D (default), 1/4 T, 1/4, 1/4 D, 1/2 or 1 bar.
- **FEEDBACK** sets how many repeats you hear, 0 to 9.5.
- **TONE** darkens the repeats at low values and keeps them bright at high values.
- **MIX** sets the level of the repeats.

### REVERB

- **ON** switches the plate reverb on or off.
- **SIZE** sets the size of the space and the length of the tail.
- **DAMP** softens the high frequencies of the tail.
- **MIX** sets the level of the reverb.

The order of the chain is chorus, delay, reverb. A soft limiter after them catches the peaks.

### SYSTEM

- **MIDI OUT** chooses what AT101V sends to its MIDI output: Off, Thru or Synth Notes (default). See the MIDI section.
- **OUTPUT** sets the final level, from -24 to +6 dB.
- **BODY** chooses the colour of the case: Grey, Blue or Red.

MIDI OUT, OUTPUT, BODY and OVERSAMPLING are your own preferences: loading a preset never changes them.

![AT101V in the blue body, with a C major chord held on the Poly Brass preset.](img/body-blue-chord.jpg)

## Presets

AT101V comes with 44 factory presets: Init plus 43 sounds in seven categories. A preset stores the whole panel, the sequence and the settings, except the four system preferences.

### Choosing a preset

- Click **<** or **>** in the top bar to step through all presets.
- Click the preset name to open the menu. Each category is a submenu; your own presets are under User.
- **Init** in the menu loads the basic patch, with an empty sequence and default settings.
- **Show user presets folder** opens the folder where your presets are stored.

### Saving your own

1. Click **SAVE**.
2. Type a name and click Save (or press Return).

The preset is stored as a `.json` file in the user folder and appears under User in the menu. Saving with the name of an existing user preset replaces it without asking.

- macOS: `~/Library/Application Support/Audio Tools/AT101V/Presets`
- Windows: `%APPDATA%\Audio Tools\AT101V\Presets`

To delete or rename a user preset, do it in that folder. Preset files you copy into the folder from elsewhere appear the next time AT101V opens.

Your DAW project stores the full state of AT101V, including the sequence, the key transposition and the MIDI learn assignments, whether or not you saved a preset.

### Factory presets

| Category | Presets |
|---|---|
| Init | Init |
| Bass | 101 Bass, Pulse Bass, Sub Rumble, Rubber Bass, Square Sub, Gate Bass, Funk Pluck, Drive Bass |
| Lead | Classic Lead, Whistle, Bright Saw, PWM Solo, Singing Res, Unison Lead, Envelope PWM |
| Seq | Acid Line, Acid Drive, Berlin Night, Electro Funk, Minimal Pulse, Italo Octaves, Motorik, Techno Stab, Probability Bass, Hold Me |
| Arp | Arp Up, Arp U&D Host, Arp Down Bells, Trance Gate Arp |
| Ambient | Drift Sequence, Glass Bells, Dark Drone, Ocean Wind, Soft Pad, Self-Playing Bleeps |
| Poly | Poly Brass, Poly Keys, Poly Pluck |
| FX | Sample & Hold, Zap Tom, Noise Snare, Laser, Siren |

The Seq presets and some Ambient presets start their sequence as soon as they load; most follow the host tempo. For the Arp presets, hold a chord, and press HOLD to leave it running.

### In REAPER

REAPER lists every factory preset twice in the preset menu: once from the plugin's own list and once as a preset file (the .vstpreset files that the launcher or the Presets folder of the zip install for Ableton Live, in ~/Library/Audio/Presets/Audio Tools/ on macOS, Documents\VST3 Presets\Audio Tools\ on Windows and ~/.vst3/presets/Audio Tools/ on Linux). Both load the same settings. If you don't use Live, you can delete the "Audio Tools AT101V" folder there and each preset appears once.

## MIDI

### What AT101V responds to

| Message | Effect |
|---|---|
| Notes | Play, on any channel, with velocity |
| Pitch bend | Moves the bender lever left and right |
| CC 1, mod wheel | Pushes the lever forward (LFO MOD) |
| CC 64, sustain pedal | HOLD while the pedal is down |
| CC 120, CC 123 | Release all notes |

Velocity matters only as much as VEL VCF and VEL VCA allow; at their default of 0 every note sounds the same. While the sequencer plays with KEY TRANSP on, incoming notes transpose it, and while LOAD is lit they record steps.

### MIDI learn

You can control any fader, knob or switch on the panel, and any control on the Settings page, from a knob or fader of your MIDI controller.

1. Right-click the control (or Ctrl-click on macOS) and choose **MIDI Learn**.
2. Move the knob or fader on your controller. The control is now assigned to that CC.

On the Settings page the assigned CC appears in orange under the control. To remove it, right-click the control and choose **Clear MIDI CC**. Each control takes one CC, and each CC drives one control: learning a CC that was already in use moves it to the new control. A learned CC replaces the built-in meaning of CC 1 and CC 64. The assignments are stored with your DAW project, not with presets. The buttons of the panel (POWER, LOAD, PLAY, the arpeggio buttons, HOLD and KEY TRANSPOSE) and the bender lever cannot be learned.

### MIDI out

AT101V has a MIDI output. The **MIDI OUT** list on the Settings page chooses what it sends.

- **Off**: nothing.
- **Thru**: the incoming MIDI, unchanged.
- **Synth Notes** (default): the notes AT101V actually plays, on channel 1, with velocity: your keys, the arpeggio and the sequence, with the key transposition applied.

Synth Notes lets you record an arpeggio or a sequence as MIDI, or drive another instrument with it. In Ableton Live: create a second MIDI track, set its MIDI From to the track that holds AT101V and, in the chooser below, to AT101V; then arm the second track and record. If you hear notes twice or they get stuck, check that the output of AT101V is not routed back into its own input. Use the VST3: Live and REAPER ignore MIDI out from an Audio Unit.

MIDI out leaves as late as the plugin's latency (27 samples, the oversampling filters), the same delay as its audio: the host compensates the audio, so another instrument played through MIDI From lands together with AT101V instead of 0.6 ms early.

## Recipes

### Classic 101 bass

1. Choose Init in the preset menu.
2. Set **RANGE** to 16'.
3. In the mixer, set Saw to 8 and **SUB OSC** to 7, with the sub switch on 1 OCT DOWN.
4. In the VCF, set **FREQ** to 3.4, **RES** to 3, **ENV** to 5 and **KYBD** to 6.
5. In the ENV, set **A** to 0, **D** to 3.5, **S** to 2 and **R** to 1.5.
6. Play low notes, short and detached. Raise ENV for more snap, or lower FREQ for a rounder tone.

This is the 101 Bass preset. For a rubbery variation, set the trigger switch to GATE and PORTAMENTO to 2 on AUTO, and play legato.

### Acid line

1. Choose Init, then set Saw to 10, **FREQ** to 2.4, **RES** to 8.2, **ENV** to 6 and **KYBD** to 4.
2. Set **D** to 3.4, **S** to 0 and **R** to 1.5, with the trigger switch on GATE+TRIG.
3. On the Settings page, set **SOURCE** to Host Tempo, **DIVISION** to 1/16 and **ACCENT** to 7.
4. Open the SEQUENCER page and click **CLEAR**. You get 16 empty steps.
5. Draw a line around C2: mostly C2, a few C3 and D#2, G2 and A#1, and leave two or three rests.
6. In the ACCENT lane, mark about four steps. In the SLIDE lane, mark the steps that should glide in, such as the jumps to C3.
7. Press **PLAY** and start the DAW. While it plays, ride FREQ, ENV and RES on the SYNTH page.

Compare it with the Acid Line preset. For more bite, set **DRIVE** to about 6 and add the delay at 1/8 D.

### Arpeggio synced to the DAW

1. Choose Init. Set Saw to 10, **FREQ** to 4.6, **RES** to 5, **ENV** to 4, **D** to 2.4, **S** to 0 and **R** to 2.
2. Press **U & D**.
3. On the Settings page, set **SOURCE** to Host Tempo, **DIVISION** to 1/16 and **ARP OCT** to 2.
4. Switch the delay **ON** with **TIME** at 1/8 D and **MIX** around 2.
5. Start the DAW, hold a chord, and press **HOLD**. Let go: the arpeggio keeps running.
6. Click **KEY TRANSPOSE** and press another key to move the held arpeggio. Play a new chord to replace it.

For the original feel, leave SOURCE on Modulator and set the speed with **LFO/CLK RATE** instead.

### Sequence recorded from the keyboard, transposed live

1. Choose the 101 Bass preset.
2. Press **LOAD**.
3. Play C4, C4, then press **KEY TRANSPOSE** for a rest, then play D#4.
4. Press **HOLD** and play D#4 again: it is tied to the first D#4, making one longer note.
5. Play G4, A#3, C4 and F4, then press **LOAD** to finish.
6. Press **PLAY**. The sequence loops at the modulator's speed; set **LFO/CLK RATE** for the tempo.
7. Press C4 (the lowest C on screen) and the sequence plays as recorded. Press F4 and it moves up a fourth, press G3 and it moves down a fourth.
8. Set the trigger switch to GATE to hear the tie as one smooth note.

Open the SEQUENCER page to see what you recorded, and add accents or slides with the lanes.

### Polyphonic pad

1. Choose Init. On the Settings page set **VOICES** to 6 and **UNISON** to 1.
2. In the mixer set Pulse to 8 and Saw to 6. Set the pulse-width switch to **LFO**, **PULSE WIDTH** to 6 and **LFO/CLK RATE** to about 2.
3. Set **FREQ** to 4.8, **RES** to 1.5 and **ENV** to 2.
4. Set **A** to 6.5, **D** to 7, **S** to 8 and **R** to 7.5.
5. On the Settings page set chorus **MODE** to II with **MIX** at 6, and switch the reverb **ON** with **SIZE** at 8.5 and **MIX** at 3.5.
6. Play slow chords.

This is the Soft Pad preset. For a wider sound, raise **UNISON** to 2 and **DETUNE** to about 15 cents; with 6 voices and unison 2 you can still play 6 notes.

### Sample and hold bleeps

1. Choose Init. Set the **WAVE FORM** knob to RANDOM and **LFO/CLK RATE** to about 7.5.
2. Set the trigger switch to **LFO**.
3. Set Saw to 10, **FREQ** to 3.8, **RES** to 7.5, **ENV** to 2 and the VCF **MOD** to 5.
4. Set **D** to 2.5, **S** to 0 and **R** to 2.
5. Hold a key. On every cycle of the modulator the filter jumps to a new random value and the envelope restarts.
6. Raise the VCO **MOD** for random pitches as well, and add the delay at 1/16 T.

## Troubleshooting

### No sound

- Check the **POWER** LED. If it is out, click POWER.
- Check **VOLUME** on the panel and **OUTPUT** on the Settings page.
- Make sure at least one mixer fader is up, or RES is high enough to self-oscillate.
- With **FREQ** near 0 and ENV at 0 the filter is closed. Raise FREQ.
- With **S** at 0 and the VCA on ENV, held notes fade after the decay. That is normal.
- In your DAW, make sure the track receives MIDI and is armed or monitored.

### The keys transpose instead of playing

The sequencer is playing and **KEY TRANSP** is on. Press PLAY to stop the sequence, or turn KEY TRANSP off on the Settings page to play over it.

### Everything sounds in the wrong key or octave

- If the **KEY TRANSPOSE** LED is lit, right-click the button (or Alt-click) to reset it. KEY TR in the lower bar shows the current value.
- Check the **TRANSPOSE** switch (M is normal) and **RANGE** (8' is normal).
- Check TUNE.

### PLAY does nothing

The sequence is empty. Record one with LOAD, draw one on the SEQUENCER page, or load a Seq preset.

### The arpeggio plays one note without moving

A single key without HOLD plays normally, by design. Hold two keys or more, press HOLD, or raise **ARP OCT**.

### The arpeggio or the sequence is out of time with the DAW

Set **SOURCE** to Host Tempo on the Settings page. With the modulator as the clock, the tempo comes from LFO/CLK RATE and does not follow the DAW.

### Ties restart the envelope, or portamento does not glide

Set the trigger switch to GATE for smooth ties. With the arpeggio and the sequence, portamento glides only in ON.

### Stuck notes

Click POWER twice, or send CC 123 (All Notes Off) from your controller. Also check the MIDI routing (see MIDI out).

### AT101V does not appear in the DAW

Check that the files are in the folders given in Installing, and rescan. On macOS run the `xattr` command from Installing. On Windows, copying to the VST3 folder needs administrator rights.

### High CPU

Set **OVERSAMPLING** to 2x, use fewer voices or less unison, and switch off the effects you do not use.

## Specifications

| Item | Value |
|---|---|
| Inspired by | Roland SH-101 (1982) |
| Formats | VST3, Audio Unit (macOS), standalone |
| Systems | macOS 11 or later (universal), Windows 10 or 11 64-bit |
| Type | Instrument with MIDI in and MIDI out |
| Output | Stereo (mono bus supported) |
| Voices | Mono, or 2, 4, 6 or 8; unison 1 to 4; 16 in all |
| Oscillator | Saw and pulse, sub one or two octaves down or 25 % pulse, noise |
| Filter | Four-pole low-pass, 24 dB per octave, 10 Hz to 20 kHz |
| Filter response | -12 dB at cutoff, -28 dB at twice, -49 dB at four times the cutoff |
| Self-oscillation | From about RES 9; at RES 10, KYBD 10: 218, 436 and 874 Hz for C3, C4 and C5 |
| Envelope | Attack about 1 ms to 4 s, decay and release about 2 ms to 10 s |
| Modulator | 0.1 to 30 Hz; triangle, square, random, noise |
| Portamento | 0 to about 5 s; Auto, Off, On |
| Arpeggiator | Up, Up and Down, Down; 1 to 4 octaves |
| Sequencer | 100 steps: note, rest, tie, accent, slide, probability |
| Clock | Modulator, or host tempo from 1/4 to 1/32 with triplets; swing 0 to 75 % |
| Effects | Drive, chorus (I, II, I+II), synced delay, plate reverb |
| Oversampling | 2x, 4x or 8x |
| Presets | 44 factory presets; presets leveled to about -17 dBFS RMS |
| CPU | About 1 % of one core in mono, 3 % with 4-voice unison, 6 % for a 6-voice pad (Apple M-series, 4x) |
| Validation | pluginval at strictness 8, auval on macOS |
| License | GNU AGPL v3 |

Roland and SH-101 are trademarks of Roland Corporation. Audio Tools is an independent project, not affiliated with or endorsed by Roland. Product names are used only to identify the historical instrument that inspired this recreation.
