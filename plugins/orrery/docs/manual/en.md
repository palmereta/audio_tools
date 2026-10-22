# Orrery user manual

## What Orrery is

Orrery is a generative MIDI instrument for ambient, dark ambient, drone and calm music. It does not make sound by itself: it writes notes and controller moves and sends them to other instruments, either software synths in your DAW or hardware synths on a MIDI cable. One Orrery can play a single synth or a whole rack of them.

Inside, eight **lanes** orbit a slow shared harmony. Each lane has a role (drone, pad, melody, arpeggio, cloud, bass or pulse) and its own MIDI channel, so lane 1 can play a low drone on one synth while lane 3 plays a melody on another. All the lanes listen to the same chord, which changes slowly, so the parts always agree with each other.

A lane can also record what it plays and repeat it as a loop of its own length, mutating a little on every pass. With loops of different lengths the parts drift in and out of phase for hours, the technique Brian Eno described for Music for Airports. On top of that there are sessions with a length and slow fades, a breathing pace that times the chords and phrases, and a gentle mode that keeps everything soft and low.

Every random choice comes from a **variation** number, so the same number with the same settings plays the same notes every time. A small built-in **preview sound** lets you try it all without routing anything.

> Orrery is a music tool for relaxation and focus. It is not a medical device and makes no therapeutic claims.

## Installing

Orrery comes as a VST3 plugin, an Audio Unit (macOS only) and a standalone application. It runs on macOS 11 or later (one universal binary for Apple silicon and Intel) and on 64-bit Windows 10 or 11.

On macOS:

1. Unzip the download.
2. Copy `Orrery.vst3` to `~/Library/Audio/Plug-Ins/VST3`.
3. Copy `Orrery.component` to `~/Library/Audio/Plug-Ins/Components`.
4. Rescan plugins in your DAW. In Ableton Live, use the VST3 (see below).

On Windows:

1. Unzip the download.
2. Copy `Orrery.vst3` to `C:\Program Files\Common Files\VST3`.
3. Rescan plugins in your DAW.
4. `Orrery.exe` is the standalone version: it runs on its own and plays your MIDI ports.

> The builds are not code-signed yet. macOS may refuse to open them until you run `xattr -dr com.apple.quarantine` on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Each package includes an INSTALL.txt with the exact steps.

## Loading it in a DAW

Orrery loads as an instrument on its own MIDI track. Its MIDI comes out of that track; your synths sit on other tracks and take their MIDI input from it. Every lane always goes to the DAW on its own MIDI channel: lane 1 on channel 1, lane 2 on channel 2, and so on, unless you change the channels.

If your DAW can route a plugin's MIDI output to other tracks, set each synth track's MIDI input to the Orrery track, and to the lane's channel if your DAW lets you filter by channel. If it cannot, use the direct MIDI outputs with a virtual port (see Routing, ports and sync).

Orrery ignores incoming MIDI and does not pass it through. When the plugin is bypassed it sends note-offs for everything that is sounding and then stays silent.

In Ableton Live:

1. Drag the Orrery VST3 onto an empty MIDI track. Live does not pass on the MIDI that Audio Unit plugins produce, so the AU version cannot play other tracks there.
2. Put your synth on another MIDI track.
3. In the synth track's I/O section, set **MIDI From** to the Orrery track, and in the chooser below it pick `Audio Tools Orrery`.
4. Set the synth track's **Monitor** to `In`, or arm the track.
5. Click **Preview sound** in Orrery's top bar, next to Play, so it reads `Preview sound: Off`. You then hear only your synth.
6. Press Play in Orrery.

In REAPER, use the VST3 too: REAPER does not pass on the MIDI that Audio Unit plugins produce either. Send the Orrery track to each synth track (a send with MIDI), or record-arm the synth track with the Orrery track's MIDI as its input.

Live merges all MIDI channels when it routes MIDI from one track to another, so that synth receives every lane that is on. That is perfect for one synth playing everything. To send different lanes to different synths in Live, add more Orrerys: the first one is the Main and every Orrery you add after it becomes a satellite that plays one lane of the Main. The steps are in One Orrery, many synths, at the start of Routing, ports and sync. A virtual port, or one Orrery per track with Link, also work.

## First sound

1. Open Orrery (the standalone, or the plugin on a MIDI track).
2. On the Simple page, click a mood on the left, for example **Ambient**.
3. Press **Play** in the top bar.

The preview sound plays right away and the orrery in the middle comes alive: one orbit per lane, a planet travelling round each orbit and a spark for every note. If you hear nothing, see Troubleshooting.

What you hear is Orrery's **preview sound**, a small built-in synth that is there so you can try Orrery without routing anything. It is not your synth. While it plays, the button next to Play reads `Preview sound: On` and the line above the orrery says "You're hearing Orrery's preview sound". When your own synths play, click the button so it reads `Preview sound: Off`: the MIDI Orrery sends is the same either way.

> Many presets fade in over 20 seconds or more, and sleep presets over a minute. The first seconds are quiet on purpose.

## The interface at a glance

The window has a top bar, a page area and a footer. It can be resized from the corner, from half to one and a half times its normal size, and keeps its proportions.

![The top bar: Play, Preview sound, the preset selector, Save, Simple and Advanced, Capture MIDI and All notes off.](img/topbar.jpg)

The top bar, from left to right:

- **orrery**: the logo. Clicking it returns to the Simple page. Under it, the status line shows `playing` and the session time (and the session length when there is one), `stopped`, or `session finished` when a session has ended.
- **Play** / **Stop**: starts and stops Orrery.
- **Preview sound: On** / **Off**: switches the built-in preview sound. It sits next to Play so you can always see whether the sound you hear is Orrery's own or your synth's. It is the same setting as Preview sound on the Outputs page.
- **<** and **>**: the previous and next preset. The preset name between them opens the preset menu, grouped by mood, with your own presets under User.
- **Save**: saves the current settings as a user preset.
- **Simple** and **Advanced**: switch between the Simple page and the three Advanced pages. On the Advanced pages, tabs below the top bar choose **Lanes**, **Harmony & session** or **Outputs**.
- **Capture MIDI**: saves the last minutes played as a MIDI file.
- **All notes off**: stops every sounding note on all 16 channels, in the DAW and on the direct ports.
- **Manual**, at the right of the footer, opens this manual inside the plugin, in English or Spanish. Press Esc or the cross to close it.

On a satellite (see Main and satellites) the top bar keeps only the logo, Preview sound and All notes off: the rest belongs to the Main.

The footer shows a short help line for whatever is under the mouse, messages such as the name of a captured file, and the captured file itself, ready to drag into your DAW.

The controls work the same everywhere:

- Round knobs: drag up or down. Hold Shift for fine steps, double-click to return to the default, or use the mouse wheel.
- Boxes with a small triangle: click to open a menu of choices.
- Boxes with a number: drag up or down, or use the mouse wheel. Double-click returns to the default.
- Boxes with two states (Off and On, Stopped and Playing): click to switch.

Every setting is also a host parameter, so you can automate it in your DAW. Lane settings appear in the host as `Lane 1 Density`, `Lane 2 Role`, and so on.

## The Simple page

The Simple page is all most people need: the moods, the orrery, a switch for each lane, five knobs that shape everything at once, the key and the session.

![The Simple page playing Tape Loops: moods on the left, the orrery in the middle, the Shape knobs on the right.](img/simple.jpg)

### Moods

![The Moods panel with Ambient selected and its three presets.](img/moods.jpg)

The **Moods** panel lists the seven moods: Ambient, Dark Ambient, Drone, Sleep, Meditation, Focus and Celestial. Clicking a mood loads its first preset and shows a one-line description under the buttons, such as "Low, minor, unresolved" for Dark Ambient. Below that, the list shows the three presets of the mood; click one to load it. If Orrery is playing, it keeps playing with the new preset.

### The orrery

![The orrery: one orbit per lane, the chord in the sun and a spark for every note.](img/orbit.jpg)

The picture in the middle shows what is playing. The sun in the centre shows the current chord (for example `Dm9`) and, under it, the key and scale. An arc round the sun fills up as the chord advances and starts again at each chord change. When Orrery is stopped the sun says `rest`.

Each lane that is on has its own orbit, in the lane's colour. Its planet goes once round the orbit every **Loop length** beats of that lane, so lanes with different loop lengths almost never line up, just as their loops do not. The planet grows while the lane is playing notes, and every note leaves a spark on the orbit that fades slowly: bigger sparks for louder notes. When breathing is on, a soft halo round the sun opens and closes with each breath.

Hover over an orbit to see its lane, role and channel. Click an orbit to open the Lanes page with that lane highlighted.

### Role, satellites and ports

![Above the orrery: Role, Skip satellite lanes and Publish lanes as MIDI ports, with the satellites of this Main and the preview sound notice.](img/rolestrip.jpg)

The row above the orrery has:

- **Role**: `Auto`, `Independent`, `Main` or `Satellite`. The button shows the role in use, for example `Role: Main (Auto)`. See Main and satellites.
- **Skip satellite lanes** (on a Main only): when On, the lanes your satellites play are not also sent from the Main's own track, so no synth receives them twice. On by default.
- **Publish lanes as MIDI ports** (macOS and Linux): one virtual MIDI port per lane, named after the lane, that each synth track in your DAW can pick as its input. Off by default. See Routing, ports and sync.

The line under them says who plays what. On a Main it lists the satellites and the lane each one plays, and the lane switches of those lanes get a gold ring. While the preview sound is playing, the right end of the line reads "You're hearing Orrery's preview sound".

### Lane switches

![The lane switches: role, MIDI channel and an On/Off switch for each lane.](img/chips.jpg)

Under the orrery there is a switch for each of the eight lanes, in the lane's colour, with its role and MIDI channel above it. Click a switch to turn the lane on or off. A lane turned off releases its notes at once.

### Shape

![The five Shape knobs: Density, Motion, Space, Brightness and Tension.](img/knobs.jpg)

The **Shape** panel has five knobs that act on every lane at once. They are the same as the first five knobs on the Harmony & session page.

| Knob | Range | What it does |
|---|---|---|
| Density | 0 to 100 % | How many notes: multiplies every lane's density by 0.5 to 2 |
| Motion | 0 to 100 % | How fast chords and loops change |
| Space | 0 to 100 % | Longer notes, wider gaps, longer echoes |
| Brightness | 0 to 100 % | Register and velocity |
| Tension | 0 to 100 % | From consonant to suspended chords, clusters and tritones |

At 50 % **Density** leaves each lane as set. **Motion** also makes looped notes mutate more often, gives melodies more leaps and makes Drift controller curves wander faster; at 100 % chords last half the Chord length, at 0 % twice as long. **Space** stretches note lengths and echo times by 0.5 to 2 times and lengthens the rests between phrases. **Brightness** moves every lane up or down by up to seven semitones and makes notes softer or louder.

**Tension** adds colour: above 25 % chords may become sus2 or sus4, above 55 % a minor-second cluster may join them, above 75 % a tritone. Below 50 % the slow walk avoids diminished chords. Above 55 % the chord name in the sun gets an asterisk.

### Key and session

![Key and session, with the variation number, on the Breath Six preset.](img/keysession.jpg)

- **Key**: the tonic, C to B.
- **Scale**: one of 16 scales (listed under the Harmony & session page).
- **Tempo**: 20 to 140 BPM. It applies when the clock is Free, or when the host is stopped.
- **Session**: how long Orrery plays, with fades, before stopping. `Endless` never stops.
- **Breathing**: a breathing pace in breaths per minute, or Off.
- **Gentle**: caps loud notes, high notes and leaps.

Under them, **Variation** is the number that seeds every random choice, and **New variation** picks a new random number from 1 to 9999. See Loops and variations.

## How Orrery makes music

Everything starts from the shared harmony. Orrery builds chords by stacking thirds on the degrees of the scale, so every chord belongs to the key. A chord lasts **Chord length** beats (adjusted by Motion), then the next one is chosen, usually a fourth, a fifth or a third away. Every note of every lane comes from the current chord or the scale, unless Tension adds clusters and tritones on purpose.

Each lane plays that harmony in the way its **role** sets. Every role has its own home register: bass lowest, then drone, pad and pulse in the middle, melody, arpeggio, and cloud highest. Octave, Range and Brightness move it from there.

### Drone

Holds the root of the chord low down, sustained until the next chord. With the lane's Density above 35 % it adds the fifth, and above 70 % the octave above. It strikes again at each chord change, and on long chords every Loop length beats (at least every 8). Give drones a synth with a slow attack and a long release.

### Pad

Plays the chord with the number of notes set by **Voices** (more with higher density, fewer with lower), and moves each voice to the nearest note of the next chord, so pads glide between chords instead of jumping. The notes start slightly apart, like a slow strum; Length widens that spread. Pads hold until the chord changes.

### Melody

Plays phrases of three to nine notes, walking the scale mostly by step with occasional leaps, then rests. Higher density means longer phrases, quicker notes and shorter rests; Space lengthens the rests; Motion makes leaps more likely. Long notes land on chord tones when Follow is set to Chord. With breathing on, each phrase starts as you breathe out.

### Arp

An arpeggio stepping at the lane's **Rate** through the notes allowed by Follow within the lane's range, in the order set by **Arp pattern**. Which steps play comes from the Euclidean rhythm, or from chance when Euclid pulses is 0. The first step of each pattern is slightly accented.

### Cloud

Single notes scattered at random moments and pitches, like drops of glass. At low density a note arrives every 16 beats or so on average, at full density about every beat. Clouds are where echoes shine: two or three echoes with a long Echo time turn a few notes into a shimmering field.

### Bass

Plays the root of the chord in the low register at each chord change and holds it. With the lane's density above 50 %, long chords are split into two notes, and the second one is sometimes the fifth.

### Pulse

A single repeated note, the root or fifth nearest the middle of its range, played in the same rhythm engine as the arpeggio (Rate, Euclid steps, pulses and rotate). Low and slow, it is a heartbeat; higher and faster, a gentle tick for focus music.

## The Lanes page

The Lanes page has one column per lane and one row per setting: 29 settings for each of the 8 lanes. The column header shows the lane number in its colour. Clicking an orbit on the Simple page opens this page with that lane highlighted.

![The Lanes page with the First Light preset: one column per lane.](img/lanes.jpg)

![The rows of the Lanes page, with lanes 1 to 4.](img/lanestable.jpg)

Some settings matter only for some roles: Rate and the Euclid rows for Arp and Pulse, Arp pattern for Arp, Voices for Pad. **Follow** chooses the notes for Melody, Arp and Cloud, and for every lane while it plays back a loop; Drone, Bass and Pulse always use the root (and fifth), and Pad always uses the chord.

### Role, channel and register

| Setting | Range | Default |
|---|---|---|
| On | Off, On | lanes 1, 2, 3, 5 and 6 on |
| Role | Drone, Pad, Melody, Arp, Cloud, Bass, Pulse | per lane |
| MIDI channel | 1 to 16 | the lane number |
| Octave | -3 to +3 | 0 |
| Range | 4 to 48 semitones | 19 |

**MIDI channel** decides which synth hears the lane. Two lanes on one channel share its notes and controllers: the same note played by both is cut short, and one lane's drift or controller curve moves the other too.

**Octave** moves the lane's home register by octaves. **Range** is how far the notes may spread around it, in semitones: narrow for a focused voice, wide for arpeggios and clouds that cover several octaves.

### Notes

| Setting | Range | Default |
|---|---|---|
| Density | 0 to 100 % | per lane |
| Length | 0 to 100 % | 50 % |
| Velocity | 0 to 100 % | 60 % |
| Velocity random | 0 to 100 % | 25 % |
| Probability | 0 to 100 % | 100 % |

**Density** is how much the lane plays, in the way its role sets; the global Density knob multiplies it. **Length** is the note length, also scaled by Space. **Velocity** sets how hard the lane plays and **Velocity random** how much each note varies around it. **Probability** is the chance that each note plays: at 70 % about three notes in ten are left out, so a pattern breathes without changing its rhythm.

### Loop

| Setting | Range | Default |
|---|---|---|
| Loop | Off, On | on for lanes 3, 4 and 8 |
| Loop length | 2 to 128 beats | a different prime per lane, 13 to 41 |
| Mutate | 0 to 100 % | 15 % |

With **Loop** on, the lane records its first **Loop length** beats and repeats them. **Mutate** is the chance that a looped note changes on each pass: it moves to a nearby note, drops to half its velocity or shifts slightly in time, and the change stays. See Loops and variations.

### Rhythm

| Setting | Range | Default |
|---|---|---|
| Rate | 2, 1, 1/2, 1/4, 1/8 beats | 1/2 |
| Euclid steps | 1 to 32 | 8 |
| Euclid pulses | 0 to 32 | 0 |
| Euclid rotate | 0 to 31 | 0 |
| Arp pattern | Up, Down, Up-down, Random | Up-down |

These rows act on Arp and Pulse lanes. **Rate** is the length of each step in beats: 2 beats, 1 beat, half a beat, a quarter or an eighth of a beat.

**Euclid steps** is the length of the rhythmic cycle and **Euclid pulses** how many of those steps play, spread as evenly as possible: 3 pulses over 8 steps gives the familiar three-against-eight pattern, 5 over 8 a lilting one. **Euclid rotate** moves the start of the pattern. With pulses at 0 there is no fixed pattern: each step plays by chance, more often with higher density.

**Arp pattern** is the order of the arpeggio: going up, going down, up and down, or random.

### Echoes

| Setting | Range | Default |
|---|---|---|
| Echoes | 0 to 8 | 1 on lane 3, 3 on lane 5, 2 on lane 8 |
| Echo time | 0.125 to 4 beats | 0.75 beats |
| Echo decay | 0 to 95 % | 55 % |

Echoes are MIDI repeats: each note is played again **Echoes** times, every **Echo time** beats (stretched by Space), each repeat softer by **Echo decay**. Because they are real notes, your synth plays them with its own sound. Repeats that would be very soft are left out.

### Harmony per lane

| Setting | Range | Default |
|---|---|---|
| Follow | Chord, Scale, Root | Chord |
| Voices | 1 to 8 | 4 |

**Follow** chooses the notes a lane may use: only the tones of the current chord, any note of the scale, or just the root and fifth. Chord is the calmest, Scale lets passing notes through, Root is useful for a tanpura-like arpeggio. **Voices** is the number of notes in each pad chord.

### Controller curve and drift

| Setting | Range | Default |
|---|---|---|
| CC number | Off, CC 0 to CC 119 | Off |
| CC shape | Sine, Drift, Breath, Swell | Sine |
| CC period | 1 to 128 beats | 16 beats |
| CC depth | 0 to 100 % | 40 % |
| CC centre | 0 to 100 % | 50 % |
| Pitch drift | 0 to 50 cents | 0 |

Each lane can draw a slow curve on one MIDI controller of its channel. Pick the controller with **CC number**: 74 is often the filter cutoff, 1 the mod wheel, 11 expression, 2 breath. Controllers 120 to 127 are channel mode messages and are not offered.

**CC shape** chooses the curve:

- **Sine**: a smooth wave that repeats every **CC period** beats.
- **Drift**: a slow random wander, faster with more Motion.
- **Breath**: follows the breathing pace (it rises as you breathe in and falls as you breathe out). With Breathing off it stays at the centre.
- **Swell**: rises while the lane is playing notes and sinks in the silences.

**CC centre** is the middle value and **CC depth** how far the curve moves either side of it. The curve is updated up to 30 times a second, and only when its value changes.

**Pitch drift** makes the lane's pitch wander slowly by pitch bend, up to the cents set here, like the oscillator of an old analog synth. Set **Synth bend range** on the Outputs page to your synth's pitch bend range, or the drift will be larger or smaller than intended.

## Loops and variations

### Loops

With Loop length 17, a lane generates 17 beats of music, then plays those notes again every 17 beats. Each repeated note is fitted to the current chord, so when the harmony moves the loop moves with it, keeping its shape.

Loops of different lengths are the heart of the Eno technique. Two loops of 17 and 23 beats only start together again after 391 beats; add a third of 29 and the full pattern takes more than eleven thousand beats to repeat. Prime numbers (11, 13, 17, 19, 23, 29, 31, 37, 41) keep lanes out of step the longest.

Mutate keeps a loop from becoming mechanical. At 5 % a loop changes barely at all over a long piece; at 30 % it renews itself every few passes. Turning Loop on while Orrery is playing starts recording at that moment. If a lane plays nothing during its first cycle, it simply records again.

### Variations

The **Variation** number (1 to 9999) seeds every random choice: chords, notes, rhythms, mutations, controller drift and humanized timing. Each time Orrery starts, it starts from that number, so the same variation with the same settings plays the same notes every time, on macOS and Windows, at any buffer size and sample rate. With Clock on Host, starting the DAW transport is such a start: from the same song position you get the same notes in playback and in an offline render, whatever Orrery played before with the transport stopped. Changing settings while it plays changes the music from that point.

When a variation sounds right, write its number down or save a preset; for something new with the same character, press **New variation**.

## The Harmony & session page

This page has every global setting: the five Shape knobs plus chord colour, the harmony and timing, and the session.

![The Harmony & session page with the Breath Six preset.](img/harmony.jpg)

### Shape

The first five knobs are Density, Motion, Space, Brightness and Tension, the same as on the Simple page. The sixth is **Chord colour** (0 to 100 %, default 50 %): plain triads at 0 %, sevenths above 30 %, ninths above 55 % and elevenths above 85 %. In five-note scales such as the pentatonics, chords are built from the scale itself in fourths and fifths, and Chord colour adds more notes of the scale above 40 % and 75 %.

![Harmony and time on the left, Session on the right.](img/harmonysettings.jpg)

### Harmony and time

| Setting | Range | Default |
|---|---|---|
| Key | C to B | D |
| Scale | 16 scales | Dorian |
| Chord length | 2 to 64 beats | 16 |
| Progression | Slow walk, Drone, Two chords | Slow walk |
| Tempo | 20 to 140 BPM | 60 |
| Clock | Free, Host | Host |
| Play with host stopped | Off, On | On |
| Variation | 1 to 9999 | 1 |
| Humanize | 0 to 30 ms | 8 ms |

The 16 scales are Major (Ionian), Natural minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic minor, Phrygian dominant, Double harmonic, Major pentatonic, Minor pentatonic, Hirajoshi, In-sen, Whole tone and Lydian augmented.

**Chord length** is how many beats each chord lasts when Motion is at 50 %. With breathing on, it is rounded to a whole number of breaths.

**Progression** chooses how the harmony moves:

- **Slow walk**: the chords move by fourths, fifths and thirds, now and then by step, returning to the tonic from time to time.
- **Drone**: the centre barely moves. Most chord changes keep the same chord; now and then it goes to the fourth or fifth degree and comes back.
- **Two chords**: alternates between the tonic and the chord on the sixth degree (the fourth degree in five-note scales).

**Clock**: with `Host`, Orrery follows the DAW's tempo and song position while the transport runs, and its own Tempo is ignored. With `Free` it always uses its own Tempo. With Clock on `Host`, starting the DAW transport is the same as pressing Play, and stopping it is the same as Stop; the Play button shows it. Each start begins a new session at that song position (on the next sixteenth note if the position falls between them); a pre-roll or count-in before bar 1 waits for bar 1. **Play with host stopped** decides what Play does while the DAW transport is stopped: On plays at Orrery's own tempo until the transport starts (then Orrery starts again from the song position), Off waits for the transport. When the DAW transport jumps (you move the play position or a loop wraps), Orrery releases its notes and picks up from the new position.

**Humanize** moves each note slightly early or late, at random, by up to about that many milliseconds.

### Session

| Setting | Range | Default |
|---|---|---|
| Session | Endless, 1 to 120 min | Endless |
| Fade in | 0 to 120 s | 20 s |
| Fade out | 0 to 300 s | 60 s |
| Breathing | Off, 1 to 12 per minute | Off |
| Gentle | Off, On | Off |
| Fade with CC7 | Off, On | On |
| Level | 0 to 100 % | 85 % |

These are explained in the next section. **Level** sets the overall velocity of every lane; it is a global setting that presets do not change.

## Sessions, fades, breathing and gentle mode

### Sessions and fades

**Session** sets how long Orrery plays. When the time is up, Orrery stops, releases every note and the status line says `Session finished`. With `Endless` it plays until you stop it.

**Fade in** brings the music up over its first seconds; **Fade out** takes it down during the last seconds before the session ends (so Fade out only works when Session has a length). Fades act on velocity: notes become softer, down to about a third of their normal velocity at the very start or end. With **Fade with CC7** on, Orrery also moves each lane's channel volume (MIDI CC7) between 0 and 100, so held notes fade too and the session ends in silence. When Orrery stops, it sets CC7 back to 100 and centres the pitch bend on the channels it moved.

The session clock counts only while Orrery plays: Stop pauses it and Play continues it. With Clock on Host, starting the DAW transport starts a new session from zero; it fades in only when the transport starts at the beginning of the song, so stopping and going on from the middle (playing live) comes back at full level. Loading a preset starts a new session from zero, with its fade in. When a session has finished, press Stop and then Play to start another one.

> If your synth uses CC7 for something else, or ignores it, turn Fade with CC7 off. The fades then act on velocity only.

### Breathing

**Breathing** sets a slow pace in breaths per minute (1 to 12; 6 is a common slow-breathing pace). Each breath has a rising part (the first 40 %, breathing in) and a falling part (the rest, breathing out). With breathing on:

- Chord changes fall on a whole number of breaths.
- Melodic phrases start as you breathe out.
- Pads and drones swell slightly in velocity with each breath.
- Clouds play more notes at the top of the breath.
- A controller curve with the Breath shape follows it.
- The halo round the sun in the orrery opens and closes with it.

The breathing pace is a musical timing: a slow, regular rise and fall you can follow if you like.

### Gentle mode

**Gentle** keeps everything soft and low for quiet listening and sleep. It caps velocity at 92 (out of 127), keeps the notes low (no higher than about E6, MIDI note 88), limits each lane's density to 70 % and Tension to 30 %, and makes melodic leaps rare. The Sleep, Meditation and most Focus presets use it.

## The Outputs page

The Outputs page decides where the lanes go beyond the DAW, and holds the options for clock, link, preview sound and tuning.

![The Outputs page: a port and a channel for each lane, and the options.](img/outputs.jpg)

### Direct MIDI outputs

![Direct MIDI outputs: one row per lane with its port and channel.](img/directouts.jpg)

Every lane always goes to the DAW on its own channel. In addition, each lane can go straight to a MIDI port:

- Hardware synths: their USB port or the port of your MIDI interface.
- Other tracks in your DAW: a virtual port (IAC Driver on macOS, loopMIDI on Windows), with each track's input set to that port and the lane's channel.

Each row shows the lane, its role, a port menu and the lane's **Channel**. `DAW only` means no direct port. The Channel box here is the same setting as MIDI channel on the Lanes page.

- **Send every lane to the first port**: copies lane 1's port to all the other lanes. Pick a port for lane 1 first.
- **Rescan ports**: reads the list of MIDI ports again. Orrery also notices by itself, within a few seconds, when a port appears or disappears.

When a lane leaves a port, or Orrery closes, the port receives All Notes Off on the lane's channel. The ports are saved with your DAW project, not with presets.

### Options

![The Options panel: clock, link, preview sound and tuning.](img/options.jpg)

| Setting | Range | Default |
|---|---|---|
| MIDI clock out | Off, On | Off |
| Link | Off, Lead, Follow | Off |
| Preview sound | Off, On | On (Off on a new satellite) |
| Preview volume | 0 to 100 % | 60 % |
| Preview reverb | 0 to 100 % | 60 % |
| Reference pitch | 415 to 466 Hz | 440 Hz |
| Tune by pitch bend | Off, On | Off |
| Synth bend range | 2, 3, 12, 24, 48 semitones | 2 |
| Skip satellite lanes | Off, On | On |
| Lanes as MIDI ports | Off, On (macOS and Linux) | Off |

**MIDI clock out** sends MIDI clock (24 pulses per beat) plus Start and Stop to every direct port in use, so drum machines, sequencers, arpeggiators and tempo-synced delays on your hardware follow Orrery. The clock runs at the host tempo while the host plays and the clock is Host, and at Orrery's own Tempo otherwise. It is not sent to the DAW, and it is sent only when at least one lane has a direct port.

**Link** connects several Orrery instances loaded in the same DAW. It works on Independent Orrerys; a Main can also be the Lead, and satellites ignore it. The one set to `Lead` shares its key, scale and chords; the ones set to `Follow` play their own lanes over the leader's chords, ignoring their own Key, Scale and chord settings. Use one Lead. A Follow instance with no Lead in the session plays its own harmony. This link is Orrery's own: it works between instances inside the same host application, and it does not exchange tempo with other applications.

**Preview sound** is the built-in synth, also switched by the button next to Play: one sound per role (a dark drone, a warm pad, a triangle voice, an FM bell for arpeggios, a glass drop for clouds, a round bass, a soft beat for pulses), each lane panned to its own place, with a delay and a long reverb. **Preview volume** and **Preview reverb** set its level and its reverb. Turn it off when your synths play: the MIDI is the same either way.

**Reference pitch** retunes the lanes away from A4 = 440 Hz, by pitch bend, for synths that cannot be retuned themselves. It works only with **Tune by pitch bend** on. Set **Synth bend range** to the pitch bend range of your synth (the number of semitones a full bend moves): Pitch drift uses it too. If it does not match, the tuning will be wrong.

**Skip satellite lanes** and **Lanes as MIDI ports** are the same settings as on the Simple page (see Main and satellites, and Routing, ports and sync). On Windows the Options panel explains that a loopMIDI port is needed instead of Lanes as MIDI ports.

> Pitch bend applies to a whole MIDI channel, so tuning and drift need each lane on its own channel.

## Capture

**Capture MIDI** saves what Orrery has just played as a MIDI file. Click it and choose `Last minute`, `Last 2 minutes`, `Last 5 minutes` or `Last 10 minutes`. The file is saved in an `Orrery` folder inside your Music folder, named with the date and time, and the footer shows where. A box in the footer then reads "Drag ... into your DAW": drag it onto a track to use the passage as ordinary MIDI.

The file has one track per MIDI channel (so one per lane when each lane has its own channel) and the tempo Orrery was playing at. It holds the notes with their velocities and timing, including the fades. It does not hold the echoes, the controller curves, the pitch bend or the CC7 fades. Orrery keeps the last 8192 notes, so a very busy preset may give less than ten minutes. If nothing has been played yet, the footer says "Nothing to capture yet: press Play first".

## Presets and moods

Orrery comes with 21 factory presets, three per mood. Presets change the musical settings: harmony, session and every lane. They do not change Play, Clock, Play with host stopped, Level, the preview sound settings, the tuning settings, MIDI clock out, Link, the role, the satellite lanes, Skip satellite lanes, Lanes as MIDI ports or the direct ports, so switching presets never breaks your routing. Lanes a preset does not use are turned off.

A new Orrery starts with the first preset, First Light (what hosts show as its first program): drone, pad, looped melody, cloud with echoes and bass, in D Dorian.

Ableton Live does not show plugins' factory presets (it ignores VST3 program lists); use Orrery's own preset menu, or the browser: the factory presets appear in Live's browser under Plug-Ins → Audio Tools Orrery (expand the plugin), installed by the launcher or by copying the Presets folder from the zip.

| Preset | Mood | Key and scale | What plays |
|---|---|---|---|
| First Light | Ambient | D Dorian | Drone, pad, looped melody, cloud with echoes, bass |
| Tape Loops | Ambient | F Lydian | Five loops of 17, 21, 23, 29 and 31 beats over a pad |
| Slow Tide | Ambient | A minor pentatonic | Pad, looped Euclidean arpeggio, cloud, bass |
| Undertow | Dark Ambient | C Phrygian | Drifting drone, pad with a filter wander, cloud, bass |
| Catacomb | Dark Ambient | D harmonic minor | Two chords, drone, pad, low pulse, looped melody |
| Black Lake | Dark Ambient | E Phrygian dominant | Drifting drone, pad on the mod wheel, cloud with long echoes |
| Om Field | Drone | C major pentatonic | Two drones and a sparse cloud |
| Tanpura | Drone | D major | Drone, a root-and-fifth arpeggio loop, quiet pad |
| Monolith | Drone | B Locrian | Two deep drones with pitch drift and a filter wander |
| Night Garden | Sleep | G major pentatonic | Pad, cloud, bass; 45 min session |
| Deep Sleep | Sleep | F major | Drone and pad; 60 min session |
| Lullaby Stars | Sleep | C Lydian | Looped melody, sparse arpeggio, pad; 30 min session |
| Breath Six | Meditation | D major pentatonic | Drone, pad on a breath curve, cloud; 6 breaths per minute |
| Slow Breath | Meditation | A Dorian | Pad on a breath curve, melody, bass; 4 breaths per minute |
| Singing Bowls | Meditation | D major pentatonic | Two clouds with long echoes over a drone |
| Deep Work | Focus | E natural minor | Pad, steady pulse, bass |
| Study Rain | Focus | Bb major | Two clouds of short notes over a pad |
| Flow State | Focus | G Mixolydian | Looped Euclidean arpeggio, bass, pad |
| Orbiting | Celestial | E Lydian | Arpeggio with echoes, cloud, high pad, bass |
| Aurora | Celestial | Ab Lydian augmented | Five-voice pad with a filter wave, cloud, looped melody |
| Starfield | Celestial | C whole tone | Two high clouds, looped random arpeggio, drone |

All Sleep and Meditation presets use gentle mode, sessions and long fades. Deep Work and Study Rain use gentle mode too.

To save your own, press **Save**, type a name and confirm. User presets appear under User in the preset menu, and the arrows step through them after the factory ones. They are files in `~/Library/Application Support/Audio Tools/Orrery/Presets` on macOS and in `Audio Tools\Orrery\Presets` inside your AppData\Roaming folder on Windows. Saving with an existing name replaces that preset. To remove one, delete its file; it disappears the next time Orrery opens.

## Main and satellites

In Ableton Live a synth track that takes its MIDI from another track receives every MIDI channel of it, so one Orrery track would send every lane to every synth. Main and satellites solve that inside Orrery: one Orrery, the **Main**, holds the music; each other Orrery in the set is a **satellite** that plays exactly one lane of the Main (or a few), on its own track. Each synth then takes its MIDI from one satellite.

| Role | What it does |
|---|---|
| Auto | The default for a new Orrery. The first Orrery in the host application becomes the Main; every Orrery added after it becomes a satellite that takes the next lane of the Main that is on and that no other satellite plays. |
| Independent | Orrery as it was up to 1.0.4: it plays all its own lanes, and Link (Off, Lead, Follow) works as before. |
| Main | Plays like Independent, publishes everything it does for its satellites and can be a Link leader. |
| Satellite | Plays only the lanes of the Main it was given. Its own settings are not used. |

What Auto chose is saved with the project, so when you open the set again the Main is still the Main and each satellite plays the same lane, whatever order the host loads them in. Projects saved with Orrery 1.0.4 or earlier have no role: they open as Independent, with the Link setting they had, and sound as before. Presets never change the role, the satellite's lanes, Skip satellite lanes or the MIDI ports.

A copy of an Orrery does not inherit its role: when you duplicate a track in Live (or copy the track or the effect in REAPER), the copy becomes a satellite on the next free lane, like a new Orrery, even if the original was the Main. A copy of an Independent Orrery stays Independent.

### The satellite page

![A satellite: which lane of the Main it plays, its activity, and the lane buttons.](img/satellite.jpg)

A satellite shows one page instead of the full editor:

- **Satellite of** and the name of the Main's track (when the host tells plugins their track name), and which lane it plays, with the lane's role and MIDI channel. Ableton Live tells plugins a track's name only when you rename the track: until then the satellite says `Satellite of the Main Orrery` and the Main lists its satellites by lane (`1 Drone, 2 Pad`). Rename the tracks to see their names.
- The state: in sync with the Main, syncing, or waiting for a Main.
- **Lane**: the eight lanes of the Main with their roles. Click one to play it instead; Shift-click (or Cmd-click) adds or removes a lane, so one satellite can play several lanes. A lane that is off in the Main shows `off` and stays silent.
- **Activity**: how much each of its lanes is playing, and the Main's current chord. The orrery on the right shows only its lanes.
- **Role**: the same menu as on the Simple page.
- **Make independent**: turns this satellite into an Independent Orrery with the Main's current settings and only its own lanes on, so you can edit everything from there.
- **Make this the Main** appears instead when no Main is playing in the set.

A satellite follows the Main in everything: Play and Stop, the transport, moods and presets, every setting, the variation number and the session clock. Its Preview sound starts Off, because its synth should be what you hear; it can be turned on with the button in the top bar. All notes off on a satellite only resets its own lanes' channels.

### Skip satellite lanes

A Main still sends its MIDI to its own track. With **Skip satellite lanes** On (the default), the lanes that satellites play are left out of it, so a synth fed from the Main's track does not play them a second time. The Main's preview sound leaves them out too. A satellite that stops being processed for more than a second and a half (deleted, deactivated) gives its lane back to the Main.

### How they stay together

A satellite does not receive notes from the Main. It runs its own copy of Orrery's generator with all of the Main's musical state, and because the generator is deterministic it produces the same notes, velocities, controller moves and pitch bend, at the same sample, as the Main does for that lane. To do that:

- Each Orrery reads the host transport (playing, position, tempo) itself: in one processing cycle every track sees the same one.
- The Main applies setting changes, Play, Stop, new presets and All notes off one audio buffer after reading them, and publishes them for that buffer. When a satellite processes a buffer, the Main has always finished the previous one, whichever track the host processes first and on whichever processor core, so the satellite always knows what applies.
- When a satellite joins, it copies the Main's state at a buffer boundary and works out, once, which of its buffers is which of the Main's. It uses the host's transport position when the transport runs; otherwise the time the host gives each processing cycle (VST3 system time, Audio Unit host time), or the computer's clock. From then on both count buffers and samples together.
- For every buffer the Main publishes a fingerprint of all it generated, and each satellite checks its own against it. If they ever differ, or the Main stops being processed, the satellite releases its notes, copies the Main's state again and attacks the notes of its lanes that are sounding at that moment.

This was measured with a Main and several satellites at buffers from 64 to 1024 samples, in random processing orders and with each Orrery on its own thread: every satellite played exactly the Main's messages for its lane.

Things to know:

- Use one Main per set. With two, satellites follow the one that became Main first, and the other one says so.
- Satellites and their Main must be in the same host application. They do not connect between applications or computers.
- A satellite plays only while its Main is processed. Bypassing or deactivating the Main silences its satellites, which release their notes and continue when the Main does.
- Setting changes on a Main take effect one buffer later than on an Independent Orrery (about 10 ms at 512 samples).
- In REAPER with anticipative FX processing (the default), tracks are not processed in step: one can run several buffers ahead of another, most of all in an offline render. With the transport running, a satellite places itself by the song position, not by the order in which tracks are processed: it copies the Main's state and catches up to its own position, and when it runs ahead of the Main it assumes the Main's settings stay as they were and checks that when the Main gets there. A setting changed at that very moment makes it resynchronize. It never sends notes of zero length.
- With the host transport stopped, in a host that gives plugins no time for each processing cycle, and when the host always processes a satellite's track more than half a buffer after the Main's, a satellite can end up one buffer away from the Main. Starting the transport, or using Clock on Host with the transport running, makes it exact.

## Routing, ports and sync

### One Orrery, many synths in Ableton Live

Live merges MIDI channels between tracks and does not pass on the MIDI of Audio Unit plugins, so use the VST3. The quickest way to give each synth its own lane is Main and satellites:

1. Put Orrery on an empty MIDI track and choose a mood. This first Orrery is the **Main**.
2. For each synth, add another MIDI track and put Orrery on it. Each new Orrery becomes a **satellite** and takes the next lane of the Main: lane 1 (Drone), then lane 2 (Pad), and so on. Its page says `Satellite of ...` and which lane it plays; click another lane there to change it.
3. Put each synth on its own MIDI track. In the synth track's I/O section set **MIDI From** to the satellite's track, pick `Audio Tools Orrery` in the chooser below it, and set **Monitor** to `In`.
4. On the Main, click **Preview sound** in the top bar so it reads `Preview sound: Off` once your synths play. Satellites start with it off.
5. Press Play on the Main, or start Live's transport (with Clock on Host). The satellites follow.

The Main's own track still carries the lanes no satellite plays, so a synth fed from the Main's track (as in step 3) plays the rest. Skip satellite lanes keeps it from doubling the satellites' lanes.

### Lanes as MIDI ports (macOS and Linux)

Instead of satellites, one Orrery can publish each lane as a MIDI port of its own:

1. On the Simple page, turn **Publish lanes as MIDI ports** on. Orrery creates one virtual MIDI port per lane, named after its role: `Orrery Drone`, `Orrery Pad`, `Orrery Melody`... When two lanes share a role the lane number is added (`Orrery Cloud 5`, `Orrery Cloud 8`); a second Orrery that also publishes uses `Orrery 2 Drone` and so on.
2. Once only: in Live's Settings, on the Link, Tempo & MIDI tab, turn on **Track** for the input of each `Orrery` port you want to use.
3. On each synth track, set **MIDI From** to a port, for example `Orrery Pad`, with `All Channels`, and set **Monitor** to `In`.

The names are given when you turn the option on: after changing lane roles, turn it off and on to rename the ports. The ports disappear when you remove that Orrery or close the set, and each port receives All Notes Off before it closes. Windows has no virtual MIDI ports of its own, so the option is not shown there: use loopMIDI with the direct outputs as described below.

### Ableton Live, other ways

These setups from earlier versions still work.

One Orrery, one synth: follow the steps in Loading it in a DAW. Every lane that is on reaches that synth, so turn off the lanes you do not want.

One Orrery, several synths, through a virtual port:

1. Create a virtual port. On macOS, open Audio MIDI Setup, show the MIDI Studio, open IAC Driver and tick "Device is online". On Windows, install loopMIDI and create a port.
2. In Live's settings, on the Link, Tempo & MIDI tab, turn on **Track** for that port as an input.
3. In Orrery's Outputs page, choose the port for each lane (or choose it for lane 1 and click **Send every lane to the first port**), and check each lane's channel.
4. On each synth track, set MIDI From to the virtual port and the channel of the lane it should play, and set Monitor to `In`.
5. Turn Preview sound off.

Live's own channel filter on an input port keeps the lanes apart, so each synth hears only its lane.

One Orrery per synth, with Link:

1. Put one Orrery on its own MIDI track for each synth, and route each synth track's MIDI From to its Orrery track.
2. On the first Orrery's Outputs page set **Link** to `Lead`. On the others set it to `Follow`.
3. In each Orrery, turn on only the lanes that synth should play.
4. Press Play on each Orrery (or start the transport, with Clock set to Host).

The followers play over the leader's chords, so the whole set stays in one harmony, and each synth hears only its own Orrery.

### Hardware synths

1. Connect the synths by USB or through a MIDI interface.
2. On the Outputs page, pick each synth's port for the lane that should play it, and set each synth to receive on that lane's channel (or set the lane's Channel to the synth's channel).
3. Turn on **MIDI clock out** if a drum machine, arpeggiator or delay on your hardware should follow Orrery's tempo.
4. If you use Pitch drift or tuning by pitch bend, set **Synth bend range** to the synth's bend range.
5. Turn Preview sound off.

Several lanes can go to the same port on different channels, which suits a multitimbral synth or a chain of synths on one MIDI cable. Traffic is light: across all factory presets Orrery sends at most 37 messages per second on one channel, where a 5-pin MIDI cable carries about a thousand.

### The standalone application

The standalone plays the direct ports, with MIDI clock out if you want it, and the preview sound through your audio device (the standalone window's Options button opens the audio settings). With no host there is no transport, so Orrery always uses its own Tempo; keep **Play with host stopped** on, or it will wait for a transport that never comes.

## Recipes

### A pad for one synth

1. Load Orrery on a MIDI track and a pad synth on another; route the synth's MIDI input from Orrery.
2. On the Simple page, turn off every lane switch except lane 2 (Pad).
3. On the Lanes page, set lane 2's Voices to 4 or 5 and Length to 70 %.
4. On the Harmony & session page, set Chord length to 24 and Chord colour to 60 % for ninths.
5. Turn Preview sound off and press Play.

For movement, set lane 2's CC number to 74, CC shape to Sine and CC period to 32 beats: the filter opens and closes slowly.

### A whole rack of hardware

1. Load First Light (Ambient). It uses lanes 1, 2, 3, 5 and 6 on channels 1, 2, 3, 5 and 6.
2. On the Outputs page, give each lane its synth's port: the drone and the bass to one synth each, the pad to a polysynth, the melody and cloud to a third synth.
3. Set each synth to receive on its lane's channel.
4. Turn on MIDI clock out so tempo-synced delays follow along.
5. Set Synth bend range to match your synths and give the drone 10 to 15 cents of Pitch drift.
6. Turn Preview sound off and press Play.

### Eno-style tape loops

1. Load Tape Loops (Ambient), or start from First Light.
2. Set four or five lanes to Melody or Cloud with low density (10 to 20 %) and Loop on.
3. Give them prime Loop lengths far apart: 17, 23, 29, 31.
4. Keep Mutate low (around 5 %) so each loop keeps its identity.
5. Set Progression to Drone, Chord colour around 30 % and Space around 70 %.
6. Give each lane its own sound and watch the planets: they drift apart and almost never meet.

### A sleep session

1. Load Deep Sleep or Night Garden (Sleep).
2. Set Session to the length you want, for example 45 minutes.
3. Set Fade in to 60 seconds or more and Fade out to 300 seconds.
4. Check that Gentle is on, and leave Fade with CC7 on so the end is true silence.
5. Lower Level if the synths are still too present.
6. Press Play. When the time is up Orrery fades out and stops by itself.

### Two Orrerys linked

1. In Live, put Orrery A on a track feeding a pad synth, and Orrery B on another track feeding a bell or piano sound.
2. On A's Outputs page set Link to Lead; on B's set it to Follow.
3. On A, keep the drone, pad and bass lanes; on B, keep only a melody and a cloud.
4. Set Clock to Host on both and start the transport.

B's melody and cloud stay inside A's chords, even though each synth has its own track and channel.

### A filter that breathes

1. Load Breath Six (Meditation), or set Breathing to 6 on any preset.
2. On a pad lane, set CC number to 74 (or whatever your synth uses for the filter), CC shape to Breath, CC depth to 80 % and CC centre to 50 %.
3. Press Play. The filter opens as each breath rises and closes as it falls, the chords change on whole breaths and the melodies begin as the breath falls.

### Dark ambient bed

1. Load Undertow (Dark Ambient).
2. Lower Brightness to 15 % and raise Tension to 70 % for clusters and tritones.
3. Set the drone's Octave to -2 and its Pitch drift to 20 cents.
4. Set the cloud's Echoes to 5, Echo time to 2.5 beats and Echo decay to 70 %.
5. Raise Space to 85 % for long notes and long silences.

### Keeping a passage

1. When Orrery plays something you like, click Capture MIDI and choose how far back to go.
2. Drag the file from the footer onto a MIDI track in your DAW.
3. Each MIDI channel arrives on its own track, ready to edit, re-voice or arrange.

Write down the Variation number too: with the same settings, Orrery will play the same thing again from the start.

## Troubleshooting

**No sound at all.** Orrery makes MIDI, not audio. Either route it to an instrument (see Loading it in a DAW) or click **Preview sound** in the top bar so it reads `Preview sound: On`, with Preview volume up on the Outputs page.

**I hear a sound, but it is not my synth.** That is Orrery's preview sound: the button next to Play reads `Preview sound: On` and the line above the orrery says "You're hearing Orrery's preview sound". Click the button to turn it off.

**Nothing happens when I press Play in the DAW.** Orrery has its own Play button: press it in the top bar. The button reads Stop when Orrery is playing.

**Orrery stays silent while the DAW is stopped.** With Clock on Host, Orrery waits for the transport when Play with host stopped is Off. Start the transport, or turn Play with host stopped on.

**It plays very quietly at first.** That is the fade in (20 seconds by default, longer in sleep presets). Set Fade in to 0 if you want full level at once.

**The status line says Session finished.** The session time is up. Press Stop and then Play to start a new one, or set Session to Endless.

**The Tempo setting does nothing.** With Clock on Host and the DAW playing, Orrery follows the DAW's tempo. Set Clock to Free to use Orrery's own Tempo.

**In Ableton Live every synth plays every lane.** Live merges channels between tracks. Feed each synth from a satellite (see One Orrery, many synths in Ableton Live), use Lanes as MIDI ports or a virtual port, or one Orrery per track with Link.

**A new Orrery shows "Satellite" when I wanted a separate one.** With the role on Auto, every Orrery after the first becomes a satellite. Choose `Independent` from its Role button, or click Make independent to keep the Main's settings.

**A satellite says "Waiting for a Main Orrery".** There is no Main in this host application, or it is not being processed (deleted, deactivated or bypassed). Add one, set an Orrery's Role to Main, or click Make this the Main.

**A satellite plays nothing.** Its lane is off in the Main, or the Main is stopped. Turn the lane on in the Main, pick another lane on the satellite page, or press Play on the Main.

**A lane plays twice.** A synth gets it both from a satellite and from the Main's track. Turn Skip satellite lanes on in the Main.

**The Orrery ports do not appear in Live.** Turn on Publish lanes as MIDI ports first, then open Live's Settings, Link, Tempo & MIDI, and turn on Track for the `Orrery` inputs. On Windows there are no virtual ports: use loopMIDI.

**In Ableton Live (or REAPER) no MIDI reaches the other tracks.** You are using the Audio Unit. Load the VST3 instead.

**Orrery starts when I press Play in the DAW.** With Clock on Host, starting the transport is Play and stopping it is Stop. Set Clock to Free if Orrery should ignore the transport.

**A duplicated track plays the same lane twice.** That was Orrery 1.0.5. From 1.0.6 a copy becomes a satellite on the next free lane; if a set saved with 1.0.5 has two satellites on one lane, click another lane on one of them.

**A port is not in the list.** Click Rescan ports. On macOS, check that the IAC Driver has "Device is online" ticked; on Windows, that loopMIDI is running.

**A synth is out of tune.** Pitch drift or Tune by pitch bend is on and Synth bend range does not match the synth. Set it to the synth's bend range, or turn drift and tuning off.

**My synth's volume stays low.** Fade with CC7 moves channel volume. Orrery sets it back to 100 when it stops; if your synth uses CC7 differently, turn Fade with CC7 off.

**Notes hang.** Click All notes off. It sends note-offs and resets all 16 channels, in the DAW and on the direct ports.

**Changing a lane setting makes no difference.** Some settings only matter for some roles: Rate and the Euclid rows for Arp and Pulse, Arp pattern for Arp, Voices for Pad, Follow for Melody, Arp and Cloud. Check the lane's role.

**A Follow instance does not follow.** There must be an Orrery set to Lead in the same host application, and it must be playing. Follow works on Independent Orrerys only.

## Specifications

| Item | Value |
|---|---|
| Formats | VST3, Audio Unit (macOS), standalone |
| Systems | macOS 11 or later (universal), Windows 10 or 11 (64-bit) |
| Lanes | 8, each with its own MIDI channel and role |
| Roles | Drone, pad, melody, arpeggio, cloud, bass, pulse |
| Harmony | 16 scales and modes, triads to elevenths, tension, three progressions |
| Tempo | 20 to 140 BPM, or the host's |
| Loops | 2 to 128 beats per lane, with mutation |
| Rhythm | Euclidean patterns, probability, rates from 2 beats to 1/8, humanize up to 30 ms |
| Expression | Up to 8 MIDI echoes, one CC curve per lane, pitch drift up to 50 cents |
| Session | Up to 120 min, fade in up to 120 s, fade out up to 300 s, breathing 1 to 12 per minute, gentle mode |
| Outputs | DAW MIDI per channel, a direct MIDI port per lane, MIDI clock, Lead and Follow link, Main and satellites, a virtual MIDI port per lane (macOS, Linux), preview sound |
| Tuning | Reference pitch 415 to 466 Hz by pitch bend |
| Capture | Last 1, 2, 5 or 10 minutes to a MIDI file |
| Presets | 21 in seven moods, plus your own |
| Audio output | Stereo or mono (preview sound only) |

Measured on the engine:

| Check | Result |
|---|---|
| Ten minutes of seven lanes at Tension 0 | No note outside the scale, no hanging note at stop |
| MIDI traffic, all 21 presets, 5 minutes each | At most 37 messages per second on one channel |
| Loop of 17 beats | 84 of 84 notes repeat 17 s later |
| Gentle mode | Velocity at most 92, highest note 86 |
| Link follower | 56 notes, none outside the leader's chord |
| Same variation | Same notes on macOS and Windows, at buffers from 64 to 1024 samples and at 44.1, 48 and 96 kHz; with Clock on Host, the same notes from the transport start after any wait with the transport stopped, with a pre-roll, offline or in real time |
| Satellites | Exactly the Main's messages for their lane, at buffers from 64 to 1024 samples, in any processing order, on separate threads and with tracks up to 16 buffers ahead or behind (REAPER's anticipative FX); no notes of zero length |
| Lanes as MIDI ports | Every note of every lane arrives on its port; two Orrerys get distinct port names (macOS, Linux) |

CPU, as a percentage of one core at 48 kHz:

| Setting | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| Generator alone, 8 busy lanes | 0.003 % | 0.009 % |
| With the preview sound, Orbiting | 1.6 % | 3.8 % |
