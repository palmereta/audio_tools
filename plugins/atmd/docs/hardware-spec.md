# Minimoog Model D (1970-1981): hardware specification for the ATMD recreation

Compiled 2026-10-18 from the original owner's manual, the Norlin-era service manual, the 1970-71
R.A. Moog schematics, the reissue user's manual, a 2016 side-by-side review of a vintage unit
against the reissue, and published ladder-filter analyses. Every fact carries a source key in
square brackets; the key list is in section 9. Where sources disagree, all versions are listed.
Anything not found is marked **NOT FOUND**. Inferences I made myself (from schematics, photos or
arithmetic) are marked **(inferred)**.

Page convention: "SM p.N", "OM p.N" and "SCH p.N" are PDF page numbers of the files listed in
`ref/CREDITS.md`; "RM p.N" is the printed page number, which equals the PDF page. None of these
documents is in the repository (copyrighted; reading copies in `audiotools-private/minimoog-ref/`).

The control IDs used throughout (`osc1_range`, `filter_cutoff`, ...) are the ones in
`../assets/layoutmd.json`; the full table is in section 10.

---

## 0. Corrections to the brief (read first)

- **"Oscillator board 1" is a board number, not the early revision.** Moog numbered the Minimoog's
  cards by function: Board 1 = oscillators, Board 2 = keyboard circuit and contour generators,
  Board 3 = power supply, noise, headphone amp and modulation-mix amp, Board 4 = filter, VCA,
  A-440 and external preamp, Board 5 = rectifier [SM p.7-16] [SCH]. The oscillator revision
  boundary is **serial number 10175**: units from 10175 up carry the temperature-stabilised "D"
  oscillator card; units below it carry the "old Minimoog oscillator board" [SM p.8, p.14].
- **There are three oscillator-board designs, not two.** Moog's Trent Thompson calls the pre-10175
  board "the original Rev 2 oscillator board, which was the most common of the three major board
  designs" [SOS16]. The oscillator-board bulletin covers "Serial Numbers between 1300 and 10175"
  [SM p.52], which suggests the first design ran up to about serial 1300 (1970-71)
  **(inferred)**.
- **Serial 10175 is about 1975-76, not "pre-1975".** No factory date list was found. Gordon
  Reid's serial 11235 was "40 years old" in 2016 [SOS16]; owners date serials in the 7000s to
  about 1975 [MF] [REV]. Treat any serial-to-year mapping as approximate.
- **Model choice: a mid-1970s Rev 2 unit, not a late-70s unit.** Moog built the reissue on the
  Rev 2 board and compared its prototypes with vintage serials 3212 and 8785 [SOS16]. The Rev 2
  board is also the most common one [SOS16]. ATMD therefore models a Rev 2 unit, with the late "D"
  board as a variant (section 1).
- **The GLIDE on/off and DECAY switches are not on the front panel.** They sit on the small
  left-hand controller panel beside the wheels, with jacks for foot switches. Only the GLIDE
  *time* knob is in the CONTROLLERS section [OM p.10, p.16, p.20] [PH-kpr2].
- **The routing rockers sit on the section borders.** OSCILLATOR MODULATION and OSC. 3 CONTROL
  straddle the CONTROLLERS / OSCILLATOR BANK line. FILTER MODULATION and KEYBOARD CONTROL 1/2
  straddle the MIXER / MODIFIERS line [RM p.55] [PH-flat].
- **OUTPUT has two volume knobs.** There is a MAIN OUTPUT volume with an ON switch, a headphone
  VOLUME with the PHONES jack, the A-440 switch, and a separate POWER strip (switch and lamp)
  [OM p.6] [PH-flat].
- **The feedback loop is not built into the original.** On a vintage unit, players patched an
  output back into EXT. SIGNAL INPUT: "a user practice, not a factory feature" [SOS16]. The
  reissue normals an attenuated main output into the external input when that jack is empty
  [RM p.27, p.36].
- **The readily available "reissue manual" is Ver. 2 (2022).** It says the pitch wheel "is now
  spring-loaded" [RM p.10, p.24]. The 2016 run, like the original, used a centre detent.
- **With DECAY off, release is fast but not instantaneous.** The contour capacitor is dumped
  through 1.5 kohm, a time constant of about 15 ms (section 3.5) [SM p.13] [SCH p.1, p.7]. The
  manuals' "cut off immediately" / "diminish instantly" [OM p.16] [SM p.36] describe how it
  sounds.
- **Osc 3's reverse sawtooth replaces the triangle-sawtooth mix, not the sawtooth.** Osc 3 keeps
  the normal sawtooth [OM p.6] [SCH p.3]. The service-manual spec page misprints both waveforms
  as "Oscillator 2 only" / "1 and 2 only" [SM p.6].
- **Keyboard numbering: the brief's "F2..C6" is right.** The reissue's MIDI out puts "the C above
  low F" on MIDI note 48, so the keys are MIDI 41-84 [RM p.51]. At 8' the original sounds at that
  concert pitch (A4 = 440 Hz on the third A from the bottom); one service step disagrees by an
  octave (section 2.7).

---

## 1. Which unit ATMD models, and how the versions differ

| Version | Serials / years | Oscillator board | Notes |
|---|---|---|---|
| R.A. Moog Inc., Trumansburg NY | 1001 to about 1300, 1970-71 **(inferred)** | first design | The schematics in [SCH] are from this period (drawings dated Nov 1970 to Jul 1971; filter sheet marked "SN 1130") |
| Moog Music Inc. (Williamsville / Buffalo) | about 1300 to 10174, about 1971-1976 | **Rev 2**: discrete exponential converters with a TC-resistor temperature compensation, **individual summing** of the control inputs per oscillator [SM p.7, p.15] | Most common [SOS16]. Known Osc 2 -> Osc 1 and Osc 3 -> Osc 2 bleed [SOS16]. Bulletins: oscillator board (serials 1300-10175) "to improve tracking and pitch stability"; power supply (below 2000) "to reduce oscillator bleed-through and cross modulation" [SM p.52] |
| Moog Music / Norlin | 10175 to the end (July 1981) [WP] | **"D" stabilised board**: common summing amplifier, uA726 heated matched pair held at about 78.4 C [SM p.8-9] | Tighter tracking. The service manual warns that it "may seem at times that the instrument does not produce the 'fat', rich, multiple oscillator sound" until the oscillators are detuned slightly [SM p.29]. From 10175 the mod wheel also has a gold-plated connector and a pot with a silvered "landing zone", which reduces wheel bleed-through [LW] |
| 2016 reissue (Moog Music, Asheville) | 2016 to about Aug 2017 [WP] | recreation of Rev 2 [SOS16] | Adds a dedicated LFO (0.05-200 Hz, triangle/square), the OSC. 3/FILTER EG and NOISE/LFO source switches, MIDI, velocity and aftertouch CVs, a CV patch panel, an internal feedback normal, a Fatar TP-9 keyboard with selectable priority and an external PSU [RM p.10, p.21, p.80] [SOS16] |
| 2022 reissue | 2022 to present [WP] | same engine | Spring-loaded pitch wheel, updated MIDI [WP] [RM p.10] |

**Decision: ATMD models a mid-1970s Rev 2 unit**, with a "late D board" option. The reasons:
- It is the most common board [SOS16].
- Moog chose it as the reference for the reissue [SOS16].
- Its quirks are what most people know as "the Minimoog sound": individual summing, slightly
  looser tracking, small oscillator bleed.

The circuit details below come from the service manual, which describes both boards; the "D"
board description is more detailed. Where a fact applies to one board only, it is marked.
**Reissue-only controls are not part of ATMD's panel**: no LFO knob, no OSC. 3/FILTER EG or
NOISE/LFO switches.

---

## 2. Panel controls, ranges and behaviour

Unless noted, knobs are 300-degree pots with an eleven-tick 0-10 dial (labels 0, 2, 4, 6, 8, 10)
[PH-flat]. Rockers: switches are off with the "left-hand or bottom half depressed" [OM p.4]; the
ON legend sits by the right half (horizontal) or top half (vertical) [PH-flat].

Rocker colours on 1970s units [PH-flat] [PH-monto] [RM p.15]:
- orange: modulation and routing;
- blue: audio on/off (grey on some early units);
- cream: left-hand performance switches;
- black: power.

### 2.1 CONTROLLERS

| ID | Legend | Range / behaviour | Source |
|---|---|---|---|
| `tune` | TUNE | Master tune for all three oscillators; dial -2..0..+2 (eleven ticks). Full sweep is **4-6 semitones** (about +/-2.5 st) | Range [SM p.37]; "few tones" [OM p.10]; all oscillators [SM p.14] |
| `glide` | GLIDE | Portamento time, 0-10. Acts only while GLIDE is on (left-hand rocker or foot switch). Spec 1 ms to 1 s per octave (original) [SM p.6]; 1 ms to 10 s (reissue spec) [RM p.80]. "Neither synth approaches the 10s maximum ... the maximum is a tad longer on my Minimoog" [SOS16] | |
| `mod_mix` | MODULATION MIX | Crossfade of the modulation sources: 0 = OSC. 3, 10 = NOISE [OM p.8] [PH-flat]. Noise for modulation is pink (WHITE selected) or red (PINK selected) [SM p.10] | |
| `osc_mod_on` | OSCILLATOR MODULATION | Sends the mod wheel output to the oscillators' pitch [OM p.8] | |
| `osc3_ctrl` | OSC. 3 CONTROL (vertical) | On (up): Osc 3 follows the keyboard. Off: the keyboard, pitch bend, modulation and external pitch inputs are removed from Osc 3, and the range of its FREQUENCY knob widens | Rev 2 board [SM p.16]; "sweep of 6 octaves rather than one octave" [OM p.7]; +/-3 octaves on the D board [SM p.8] |

### 2.2 OSCILLATOR BANK

| ID | Legend | Range / behaviour | Source |
|---|---|---|---|
| `osc1_range`, `osc2_range`, `osc3_range` | RANGE | Six-position rotary: **LO, 32', 16', 8', 4', 2'**. Octave steps except LO; LO gives "sub-audio clicks". At 8' the keys play concert pitch (section 2.7) | [OM p.6] [SM p.29-31] |
| `osc2_freq`, `osc3_freq` | FREQUENCY (Osc 2, Osc 3 only) | Detune relative to Osc 1. Dial -7..0..+7 with a tick per unit, labels at odd numbers [PH-flat]. Full sweep **14-17 semitones** | [SM p.37]; "+/-7 semitones" [SOS16]; "as much as a major sixth" [OM p.7]; "tuning to a musical fifth" (D board) [SM p.8] |
| `osc3_freq` (OSC. 3 CONTROL off) | | **6 octaves** total (+/-3) | [OM p.7] [SM p.8] |
| `osc1_wave`, `osc2_wave` | WAVEFORM | Six positions, left to right: **triangle, triangle-sawtooth ("shark's tooth"), sawtooth, square, wide rectangle, narrow rectangle** | [OM p.6] [SCH p.3] [SOS16] |
| `osc3_wave` | WAVEFORM | Same, but position 2 is the **reverse sawtooth** | [OM p.6] [SM p.10] [RM p.18] |

- **Osc 1 has no FREQUENCY knob.** The panel prints "OSCILLATOR-1 FREQUENCY" over the top row
  only as a caption; Osc 1 follows TUNE [PH-flat] [RM p.17].
- **Frequency range:** 0.1 Hz to 20 kHz in six overlapping ranges [SM p.6] [RM p.80].
- **Short-term stability:** better than 0.25 % [SM p.6].
- **LO range: about 3 octaves below 32' (inferred).** Test 5.36: Osc 3 alone, OSC. 3 CONTROL
  off, LO, FREQUENCY at minimum, saw wave, gives "audible clicks ... between two to five seconds
  apart", i.e. 0.2-0.5 Hz [SM p.38]. With the keyboard reference at 0 V (low F) and FREQUENCY
  at -3 octaves, that puts LO about 3 octaves below 32'. At LO the keyboard then spans about
  2.7-33 Hz.
  - The service manual also requires that "the high end of the LO range overlaps the low end of
    the 32' range" [SM p.38].
  - The exact LO offset is **NOT FOUND**.
- **Waveform switch detents:** the RANGE and WAVEFORM legends sit about 35 degrees apart, from
  about -87 to +87 degrees around 12 o'clock **(inferred, from [PH-flat])**.

### 2.3 MIXER

| ID | Legend | Behaviour | Source |
|---|---|---|---|
| `mix_osc1`, `mix_osc2`, `mix_osc3` | VOLUME (one column, rows Osc 1/2/3) | Oscillator level into the mixer, 0-10 | [OM p.11] |
| `ext_level` | EXTERNAL INPUT VOLUME | Level of the external-input preamp (high-impedance mic/line input on the rear, 10 mV to 2 V) | [OM p.11] [SM p.6] |
| `mix_noise` | NOISE VOLUME | Noise level | [OM p.12] |
| `mix_osc1_on`, `mix_ext_on`, `mix_osc2_on`, `mix_noise_on`, `mix_osc3_on` | (ON) | One rocker per source, in this top-to-bottom order between the volume column and the EXT/NOISE knobs | [OM p.11] [PH-flat] [RM p.55] |
| `noise_color` | WHITE / PINK (vertical) | Audio noise colour; also switches the modulation noise to pink or red | [OM p.12] [SM p.10] |
| (lamp) `overload` | OVERLOAD | Lights when the **external preamp** output exceeds about 1.2 V AC, and holds briefly. Adjust EXTERNAL INPUT VOLUME "so that the OVERLOAD light blinks on and off occasionally" | [SM p.7, p.14] [OM p.11] |

- **Osc 3's ON switch is audio only:** it "does not affect the control signal produced by
  Oscillator 3 via the Modulation Mix" [OM p.11].
- **Reissue overload lamp:** it lights regardless of the EXT switch [RM p.36], consistent with
  the detector sitting before the switch [SM p.14].

### 2.4 MODIFIERS

**Filter** (top half; FILTER caption over EMPHASIS):

| ID | Legend | Range / behaviour | Source |
|---|---|---|---|
| `filter_cutoff` | CUTOFF FREQUENCY | Dial **-5..+5**, labels -4, -2, 0, 2, 4. **1 octave per division**: "a rotation of approximately 2 divisions" takes 440 Hz to 1760 Hz. With keyboard control off and EMPHASIS at 10, the filter oscillates at 440 Hz with the dial at **-1.1**. Full CW reaches at least 16 kHz | [PH-flat]; 1 oct/div [SM p.35]; -1.1 = 440 Hz [SM p.35]; also "CUTOFF at -1, EMPHASIS 10, zero beat with A-440" [SM p.33] |
| | | Spec range 40 Hz-20 kHz (9 octaves). The reissue quotes "10Hz-32kHz frequency response" [RM p.29] and 10 Hz-20 kHz [RM p.80] | [SM p.6] |
| `filter_emphasis` | EMPHASIS | Resonance. Regeneration (self-oscillation) starts between **7.5 and 8.0** at cutoff -1.1, or "between 7 and 8" in the abbreviated table. Reid's vintage unit started "at a setting of 9 or thereabouts", the reissue "around 8". At 10 the filter "breaks into oscillation, and produces a pure sine wave tone" | [SM p.35] [SM p.33] [SOS16] [OM p.15] |
| `filter_contour` | AMOUNT OF CONTOUR | Depth of the filter contour, added to the cutoff ("additive effect") | [OM p.13]; depth in section 3.4 |
| `filter_mod_on` | FILTER MODULATION | Sends the mod wheel output to the cutoff | [OM p.14-15] |
| `kbd_ctrl1`, `kbd_ctrl2` | KEYBOARD CONTROL 1 / 2 | Key tracking: **1 = 1/3, 2 = 2/3, both = 1 octave per octave** | [RM p.30]; test: switch 1 only, low A to high A (3 octaves) moves 440 to 880 +/- 50 Hz [SM p.35]; "approximately 33, 67 or 100 percent" [SOS16] |

**Filter contour** (`f_attack`, `f_decay`, `f_sustain`) and **loudness contour** (`l_attack`,
`l_decay`, `l_sustain`; LOUDNESS CONTOUR caption under the horizontal divider):

| Control | Range | Source |
|---|---|---|
| ATTACK TIME | 10 ms to 10 s (spec); "as short as 10 milliseconds or as long as 10 seconds"; reissue 1 ms to 10 s | [SM p.6] [OM p.13] [RM p.80] |
| DECAY TIME | 10 ms to 10 s (spec); reissue "4 milliseconds to > 35 seconds" | [SM p.6] [RM p.80] |
| SUSTAIN LEVEL | 0-100 % of the contour peak | [SM p.6] |
| Time dial legend | 10 (ms), 200, 600, 1, 5, 10 (s), with "M-SEC." and "SEC." at the ends. Label angles are about -120, -90, -35, +35, +70 and +115 degrees **(inferred, read from [PH-flat], +/-10 degrees)** | [PH-flat] |

- **Contour shape:** attack, decay to sustain, sustain. With DECAY on, the release uses the DECAY
  TIME; with DECAY off, it is about 15 ms (section 3.5) [OM p.16] [SM p.13] [RM p.30-31].
- **Shorting plug:** the supplied S-TRIG shorting plug holds both contours at sustain (drone)
  [OM p.16].

### 2.5 OUTPUT and POWER

| ID | Legend | Behaviour | Source |
|---|---|---|---|
| `volume` | VOLUME (MAIN OUTPUT) | Level to the HIGH and LOW main outputs | [OM p.6] [RM p.33] |
| `main_on` | MAIN OUTPUT ON | Mutes the main outputs; the phones stay live | [OM p.6] |
| `a440` | A-440 | Sine reference from a Wien-bridge oscillator. It is fed into the second VCA, after the filter and loudness contour, so it sounds continuously; level about -8 +/- 2 dB at the HIGH output | [SM p.7, p.13, p.34] [SCH p.8, p.10] |
| `phones_volume` | VOLUME (PHONES) | Headphone level, independent of the main volume | [OM p.6] |
| (jack) `phones` | PHONES | 1/4" stereo, the same mono signal on both sides, 0.3 V max into 8 ohm | [SM p.6] [RM p.33] |
| (rocker) `power`, (lamp) `power_led` | POWER, ON | Vertical black rocker with a red jewel lamp above it, in its own strip at the right end | [PH-flat] |

### 2.6 Left-hand controller (beside the keys)

| ID | Legend | Behaviour | Source |
|---|---|---|---|
| `pitch_wheel` | PITCH | Centre detent (original and 2016); spring return on the 2022 run. Bends Osc 1/2, and Osc 3 when OSC. 3 CONTROL is on | Detent [OM p.10]; spring [RM p.24] |
| | | Bend range: "as much as half an octave up or down"; spec "5 semitones minimum"; service check **13-17 semitones** total; reissue +/-7 st ("a fifth") | [OM p.10] [SM p.6] [SM p.37] [RM p.24, p.80] |
| | | Pot: 25 kohm linear, 90 degrees of rotation used, 15 kohm to ground at the detent (set to 15.3 kohm) | [SCH p.7] [SM p.34] |
| `mod_wheel` | MOD. | Level of the modulation-mix signal to the OSC and FILTER modulation switches. Zero is fully down | [OM p.8] [RM p.24] |
| | | Pot: 50 kohm **audio taper wired as a shunt to ground**, 90 degrees used, about 12 kohm "when fully fwd". Modulation comes in gently at first and heavily near the top [LW] [SCH p.7]. "Not voltage controllable" [SOS16] | |
| | | Depth with Osc 3 square at full wheel: oscillators move **13-23 semitones** [SM p.38]; filter at least 440 Hz -> 2.4 kHz (2.45 oct) [SM p.35]; spec "modulation injection range 0 to 1-1/4 octaves" [SM p.6] | |
| `glide_on` | GLIDE (cream rocker) + jack | Enables portamento. The jack (Switchcraft S-260, 0.206") takes a normally closed foot switch, or a 10 Mohm audio pedal for a faster glide | [OM p.20-21] |
| `decay_on` | DECAY (cream rocker) + jack | Final decay on/off. The jack takes a foot switch, or a 2.5 Mohm pedal to set faster release times | [OM p.16, p.20-21] [SM p.36] |

### 2.7 Keyboard

- **Keys:** 44, F to C (3-1/2 octaves) [OM p.3] [SM p.6]. In MIDI numbering the keys are
  **F2-C6 (41-84)**: the reissue sends MIDI 48 for "the C above low F" [RM p.51].
- **Priority and gate:**
  - Low-note priority: "If more than one key is held down, only the lowest one has effect"
    [OM p.10].
  - Single trigger: the contour generators are triggered "whenever a single key is depressed"
    [SM p.6], and the trigger bus stays closed while any key is held [SCH p.4].
  - Legato playing does not retrigger; the 2016 reissue added multi-trigger as an option [SOS16].
- **Key CV:**
  - 1 V/octave from a 43-resistor string fed by an 8.5 mA temperature-compensated current source.
  - The low end is grounded, so the **lowest F = 0 V** [SM p.16] [SOS16].
  - Keyboard tracking therefore does nothing on the bottom F [SOS16]. The reissue's bottom key
    outputs 0.5 V and shifts the cutoff by up to five semitones [SOS16].
- **Track and hold:**
  - The keyboard CV goes through a track-and-hold. "The pitch voltage always occurs before the
    trigger and ... the trigger goes off before the pitch voltage is released", so the last
    pitch is held through the release [SM p.16].
  - The same held CV feeds the oscillators and the filter's keyboard control, so glide also glides
    the filter tracking [SCH p.4].
- **Pitch reference at 8' (sources disagree by an octave):**
  - Majority: at 8' the highest A is 880 Hz, so 8' = concert pitch for MIDI 41-84 (A4 = MIDI 69 =
    440 Hz). Evidence:
    - high-end compensation at 2': low A = 440, high A = 3520 Hz [SM p.29];
    - octave adjust at 32': high A = 220 Hz [SM p.30];
    - old-board table at 2': high A = 3520 Hz, low A = 440 Hz [SM p.31];
    - "Press A3 (440Hz)" for the shift trim, the third A from the bottom [SM p.30].
  - Outlier: step 5.3.1 says "Octave RANGE at 8' ... low A (55Hz) ... high A (440Hz)" [SM p.29],
    one octave lower than the others.
  - The reissue tunes with OSC 1 at 4' and "the highest A key" against the A-440 [RM p.43-44],
    which does not settle the question.
  - **ATMD uses 8' = concert pitch.**
- **Keybed:** "Standard 44 key ... organ keyboard" with gold-plated buss bars and spring contacts
  [SM p.6] [SM p.38-39]. Double triggering from dirty contacts is a service item [SM p.36].

### 2.8 Rear connector strip (background only)

The jacks sit on the rear edge of the panel box, the "rear connector strip" [OM p.4] [SOS16]:
- Outputs: HIGH (0.5 V typical, 3 kohm) and LOW (15 mV typical, -30 dB, 1 kohm) main outputs
  [SM p.6].
- Inputs: EXT. SIGNAL INPUT; external control inputs LOUDNESS, FILTER and OSCILLATOR (1 V/oct,
  +/-2 % for pitch, +/-5 % for the filter; amplifier input linear, 0-4 V) [SM p.6].
- Trigger: S-TRIG input (switch closure to ground; the supplied shorting plug drones the
  contours) [OM p.16-17].
- Also: a noise output (Reid lists "red/pink noise output") [SOS16] and two regulated +/-10 V
  50 mA DC outlets [SM p.6].
- Also: the 115/230 V selector, fuses and tuning-trim access holes (on serials 1237 and up)
  [OM p.22] [SM p.29].
- None of this appears on ATMD's panel.

---

## 3. Circuit facts that matter for the sound

### 3.1 Signal flow

Signal path [SCH p.2] [SM p.7, p.12]:
- The three VCOs, the noise and the external preamp feed a **passive resistive mixer**.
- The mixer drives the **input pair of the transistor ladder** directly.
- The ladder output goes through a differential gain-recovery buffer to **VCA 1** (loudness
  contour), then **VCA 2** (external loudness jack, normalled on).
- VCA 2 feeds the main and headphone volume controls. The A-440 enters at VCA 2.

Control path [OM p.10] [SM p.8, p.12] [SCH p.11]:
- **Oscillators:** key CV + TUNE + pitch wheel + modulation (via OSC MOD) + external input, plus
  the RANGE offset and the FREQUENCY knob for Osc 2/3.
- **Filter:** CUTOFF knob + key CV x (1/3, 2/3) + modulation (via FILTER MOD) + filter contour x
  AMOUNT + external input.

Power: +/-10.000 V regulated, referenced to a 6.2 V zener; the oscillators sense the rails
remotely [SM p.11].

### 3.2 Oscillators

**Old (Rev 2) board, the modelled one** [SM p.15-16]:
- **Core:** a precision sawtooth relaxation oscillator (FET-input amplifier, Schmitt trigger,
  discharge transistor). It produces "a +4 volt to -4 volt sawtooth" before the waveform switch's
  divider.
- **Triangle:** derived from the sawtooth by one transistor (Q2). It follows the ramp until the
  ramp crosses zero, then switches to inverter mode. The result is a folded ramp whose corner
  sharpness and slope matching depend on one transistor **(inferred: expect slightly unequal
  slopes and a soft corner, not an ideal triangle)**.
- **Pulse:** a Schmitt trigger compares the ramp with a bias set by the WAVEFORM switch; duty
  "varies from a 50 percent to a 15 percent duty cycle".
- **Exponential converter:** a scale-factor temperature compensation through a TC resistor; R42
  adds positive feedback because "the exponential relationship tends to flatten at higher
  frequencies".
- **Individual summing:** each oscillator sums its own control inputs. The D board's common
  summer was introduced so that when the oscillators "track the keyboard input, they also track
  these other common drive signals" [SM p.8]. **Implication:** on a Rev 2 unit, pitch bend,
  modulation and glide move the three oscillators by slightly different amounts, so the beat
  rate changes while bending **(inferred)**.

**D board (variant)** [SM p.8-10]:
- **Sawtooth:** integrating capacitor reset by a FET; output +1.75 to -1.75 V. A "high end comp"
  trim corrects the finite reset time.
- **Triangle:** a precision inverter/follower on the ramp, +/-1.75 V, with an RC de-glitch at the
  reset.
- **Pulse:** 0 to -3.5 V. A control voltage of 0 V gives a square; -2.5 V gives 15 % duty.
- **Reverse sawtooth (Osc 3):** a transistor inverter, +/-1.75 V unloaded.
- **Exponential converter:** 21.0 mV/octave at the base of a uA726 heated to about 78.4 C.

**Waveform switch (both boards)** [SCH p.3]:
- **Triangle-sawtooth:** a resistive mix. The saw goes through 47 kohm and the triangle through
  10 kohm, and the switch taps the junction. Unloaded, that is about **82 % triangle + 18 %
  sawtooth (inferred)**: an asymmetric triangle with a small step at each reset.
- **Pulse widths:** the bias divider is 7.5 k / 1 k / 1.5 k from -10 V. It gives 0 V (square),
  -1.5 V (wide) and -2.5 V (narrow). With 15 % at -2.5 V and a linear ramp, **wide = about 29 %**
  and **narrow = 15 %** **(inferred)**.
- **Narrow pulse on the reissue:** its duty differs audibly from the vintage unit [SOS16].

**Bleed:** small leakage Osc 2 -> Osc 1 and Osc 3 -> Osc 2 on Rev 2, mostly audible on the
square at high pitches. Osc 3 is unaffected [SOS16].

**Tuning and drift:**
- Warm-up: "about ten minutes" before tuning [OM p.6]; 30 minutes "for optimum accuracy" before
  calibration [SM p.29]; 10-20 minutes on the reissue [RM p.12].
- Stability: short-term better than 0.25 % (about 4 cents) [SM p.6].
- Pre-10175 boards "drifted more with temperature" [SCU]. The Rev 2 oscillator-board bulletin
  (serials 1300-10175) is "to improve tracking and pitch stability" [SM p.52].
- Held-pitch droop: a keyboard-board bulletin (serials in the 7000s) fixes "pitch drift when key
  is released (DECAY switch on)", caused by board leakage at the track-and-hold [SM p.52-53].
  It is an optional quirk: a slow pitch sag during long releases.
- The reissue manual offers an optional 10-cent random per-key error "to replicate the tolerance
  values of the original resistors" [RM p.49].

### 3.3 Mixer and overload into the filter

Mixer topology [SCH p.11]:
- Each source goes through its 25 kohm volume pot and then a series resistor into a common node.
  The resistor is 33 kohm for the oscillators and the external preamp, and **12 kohm for the
  noise**.
- The node drives the base of the ladder's input pair through 10 uF. A 470 ohm resistor ties it
  to the ladder bias chain, so the node impedance is low.

Drive level at the ladder input **(inferred, from these 1971 values)**:
- One oscillator at volume 10 (3.5 V p-p from the D board, or the Rev 2 divider output) gives
  roughly +/-20-25 mV at the input pair.
- The pair's transfer is tanh(V / 2VT) with 2VT = 52 mV [ST eq.5]. One oscillator at full volume
  is therefore only lightly compressed (about 5 %).
- Two or three oscillators at full volume, or the noise (about 3x hotter path), push the pair well
  into compression. This is the "mixer overdrive" players dial in by turning the mixer above
  about 5.
- The external preamp has a gain of 200 [SM p.7]. Routing any output back into EXT. SIGNAL INPUT
  therefore drives the pair hard, which is why the external input / feedback loop is turned up.

Other observations:
- **The usual patch** on an original is the LOW main output into EXT. SIGNAL INPUT, with HIGH
  going to the amplifier. "The preamp overdrives on the Low output" (Kevin Lightner), and
  "resonance levels may change" [MF-OD]. Players set EXT level "so that overdrive light flickers
  on attack transients" [MF-OD].
- Pushed further, the sound can "fracture into Sub-Harmonics" depending on cutoff and emphasis, or
  become a "blasted FM sound" that flips between two tones (reissue owner) [MF-OD].
- The reissue advises keeping MAIN OUTPUT VOLUME "at or below 6 for the most musical range of
  overdrive" when using its internal feedback normal [RM p.33, p.36].
- At maximum external level and maximum volume, "only one sound is heard and different pitches do
  not sound" [RM p.27].
- An early bulletin (filter boards below serial 2000) changed three resistors "to reduce
  intermodulation distortion which occurs when mixing two or more signals" [SM p.52].

### 3.4 Filter: the 4-pole transistor ladder

**Topology** [SM p.12] [SCH p.11] [PAT]:
- A "patented design": four stages of matched TIS97 NPN pairs with capacitors C601-C604 between
  the emitters, plus a matched input pair. The cutoff is set by the standing current from a
  matched NPN-PNP exponential converter.
- The output is taken differentially across the top capacitor ("the control current is
  rejected"), then converted to single-ended by the gain-recovery amplifier and AC-coupled
  (0.22 uF) towards the VCA.
- EMPHASIS is a 50 kohm reverse-audio pot. It feeds the buffered output back to the other base of
  the input pair through a 10 uF capacitor, a 1 kohm REGEN CAL trim and 150 ohm. The positive
  feedback raises the Q "until the filter breaks into a sine wave oscillation at the cutoff
  frequency" [SM p.12].

**Control law** [SCH p.11] [SM p.35] **(inferred)**: the control mixer sums currents into the
exponential converter.

| Input | Resistor | Scale |
|---|---|---|
| External input | 100 kohm | the 1 V/oct reference |
| Keyboard 1/3 | 300 kohm | 1/3 V/oct |
| Keyboard 2/3 | 150 kohm | 2/3 V/oct |
| CUTOFF knob (+/-10 V wiper) | 200 kohm | +/-5 octaves, i.e. 1 octave per dial division, matching the dial |
| Modulation | 33 kohm | 3 oct/V of wheel output |
| AMOUNT OF CONTOUR (5 kohm pot) | 47 kohm | about 2.1 oct/V of contour |

A workable law for ATMD:

    fc = 440 Hz * 2^( (cutoff + 1.1) + kbd * (note - 41)/12 + contour_oct + mod_oct + ext_V )

where kbd = 0, 1/3, 2/3 or 1, and note 41 is the low F at 0 V.

**Contour depth (sources disagree):**
- Spec: "Continuously variable from 0 to 4 octaves" [SM p.6] [RM p.80].
- Calibration check: AMOUNT full CW sweeps the self-oscillation "from 440Hz to 35kHz +/- 10kHz",
  about 5.8-6.7 octaves [SM p.35].
- The 1971 resistor (47 kohm) with a contour peak near 5 V would allow about 10 octaves before
  the ladder runs out of range **(inferred)**.
- **Recommendation:** about 6 octaves at AMOUNT = 10, as a trim.

**Limits:**
- Full CW cutoff: at least 16 kHz with no more than 2 dB amplitude change.
- Full CCW: oscillation must survive to below 300 Hz [SM p.35].
- Reid found that emphasis "decrease[s] with decreasing cutoff frequency": tracking downwards in
  self-oscillation, his unit stopped "fewer than 20 semitones (to around 150Hz)" below where it
  started, while the reissue kept going at 50 Hz [SOS16]. The mechanism is **NOT FOUND**. Model
  it as a feedback gain that falls at low cutoff.

**Ladder theory to model** (published analyses, apply to any Moog ladder):
- **Core:** each stage is a one-pole lowpass. The core is G(s) = 1/(s+1)^4, and with feedback k,
  H(s) = 1/((s+1)^4 + k) [ST eq.21-22] [SS96].
- **Self-oscillation:** at k = 4 the poles reach the imaginary axis and the filter oscillates at
  the cutoff [ST p.20-21].
- **Passband loss:** the DC gain is 1/(1+k), about -14 dB at k = 4. "Open the filter right up ...
  turn the resonance up, and if the level of the signal drops, chances are the filter is of this
  type" [ST p.22]. The Minimoog has no passband compensation, so ATMD must keep this loss.
- **Slope:** 24 dB/octave [SM p.6]; the stopband asymptote crosses 0 dB at the nominal cutoff for
  every k [ST p.24].
- **Cutoff from current:** fc = If / (8 pi C VT), so the cutoff is linear in the ladder current
  and exponential in the control voltage [ST eq.13].
- **Nonlinearity:**
  - Each differential pair contributes tanh(dV / 2VT) [ST eq.5].
  - Huovilainen's model puts a tanh on the input and in every stage, uses the thermal voltage as
    the scale and oversamples 2x. It corrects the resonance tuning error caused by the unit delay
    in the feedback [HU].
  - D'Angelo and Välimäki refine that nonlinear model from the circuit equations [DV13].
  - Zavalishin treats the zero-delay-feedback solution of the same ladder, including the
    nonlinear stages [ZA].
- **Bass/resonance trade-off:** expect the bass to thin out as emphasis rises (passband loss) and
  the drive to change the resonance character (tanh in the loop).

### 3.5 Contour generators

Circuit [SM p.12-13] [SCH p.1]: two identical generators.

- **Trigger:** the keyboard (or an external S-trigger) sets an R-S flip-flop.
- **Attack:** a +9.3 V source charges a **10 uF** timing capacitor through the ATTACK pot
  (**1 Mohm audio**) plus 100 ohm, "resulting in an exponentially rising attack". When the
  follower output "reaches approximately 5 volts", the flip-flop resets.
  - The attack is therefore the first ~54 % of an RC charge towards 9.3 V: concave, ending at
    about **0.77 RC (inferred)**.
  - RC runs from 1 ms (100 ohm) to 10 s (1 Mohm), so the attack runs from about 1 ms to 8 s.
    Reid measured about 1 ms minimum and 9 s (filter) / 14 s (loudness) maximum on his vintage
    unit. He attributes the long times to Allen-Bradley pots reading high (1 Mohm reading about
    1.2 Mohm) [SOS16].
- **Decay:** the capacitor discharges through the DECAY pot (1 Mohm audio) towards the SUSTAIN
  voltage (5 kohm pot): exponential, tau = R_decay x 10 uF, up to 10 s. Reaching silence takes
  about 3-5 tau. Reid measured about 30 s maximum decay on both contours [SOS16]; the reissue
  spec says "> 35 seconds" [RM p.80].
- **Release with DECAY on:** the sustain driver is grounded and the capacitor decays through the
  same DECAY pot. "The final decay rate is equal to the initial decay rate" [SM p.13].
- **Release with DECAY off:** a diode is coupled "through a 1.5K resistor" to the trigger driver,
  which "discharges C5 abruptly resulting in no final decay" [SM p.13]. The 1.5 kohm is R1401 on
  the left-hand controller [SCH p.7]: **tau = 1.5 kohm x 10 uF = 15 ms (inferred)**. It is
  shorter when the decay pot is lower, since both paths discharge the capacitor.
- **Retrigger:** a new key after all keys were released restarts the attack from the current
  level. "If you play a second note early in the release phase, the next attack peaks a little
  higher than the first" (vintage and reissue alike) [SOS16].
- **Pots and taper:**
  - The pots are audio taper [SCH p.1], which is why the panel legend crowds 200 ms to 1 s into
    the middle of the dial (section 2.4).
  - Contour peak: about 5 V [SM p.13].
  - The foot-pedal option shows that the DECAY path is a series resistance (2.5 Mohm pedal)
    [OM p.21].

### 3.6 VCAs

- **VCA 1:** a TIS97 differential transconductance multiplier whose current is set by the
  loudness contour [SM p.12] [SCH p.10]. The gain law is **linear**: "Linear control
  voltage/gain relationship. Gain range spanned by 0-4 volts" [SM p.6]. Exponential RC contours
  therefore decay linearly in dB.
- **Dynamic range:** 80 dB [SM p.6].
- **Balance trims** null control-voltage thump [SM p.12, p.33]. A contour-board bulletin (serials
  below 2000) targets "thumping which may occur when a key is depressed" [SM p.52].
- **VCA 2:** same circuit, held fully on by a shorting jack unless an external loudness voltage is
  applied. +2.00 V there lowers the output by 10 +/- 3 dB [SM p.12, p.36].

### 3.7 Modulation path

- **Modulation mix amplifier:** inverting, gain 2 [SM p.10] [SCH p.13]. Osc 3's selected waveform
  and the selected noise feed the two ends of the 25 kohm MODULATION MIX pot through 24 kohm each,
  with the wiper grounded.
  - At one end only one source passes. In the middle each passes at about 70 % of its end level
    **(inferred)**.
  - The Osc 3 ON switch does not affect this path [OM p.11].
- **Mod wheel:** the mix output goes through the wheel's shunt pot to the OSC and FILTER MOD
  switches [LW] [SCH p.7]. A worn wheel pot leaves residual vibrato at zero wheel
  ("bleedthru") [LW]; optional quirk.
- **Self-modulation:** with OSC MOD and OSC. 3 CONTROL both on, Osc 3 modulates its own pitch
  **(inferred from the routing)**.
- **Sawtooth polarity:** Osc 3's "sawtooth" position heard through the modulation gives
  "repeated upward glissandi" [OM p.9]. The reverse sawtooth gives downward ramps.

### 3.8 Noise

[SM p.10] [SCH p.6]:
- **Source:** an avalanche-biased transistor junction (about -60 dB), selected, burned in, and
  amplified to white noise at about -5 dB.
- **Pink:** a passive -3 dB/octave network.
- **Red:** the pink noise through a further lowpass (the sheet marks a 100 Hz lowpass).
- **Use:** white or pink for audio; pink or red for modulation, following the WHITE/PINK switch.
  Panel test: "White noise should cause less rumbling" as a modulator [SM p.38].

### 3.9 Glide

The glide resistance sits inside the track-and-hold loop: a 5 Mohm audio GLIDE pot plus 330 ohm,
in series with a 1 uF hold capacitor through a JFET switch [SCH p.4] [SM p.16]. "With no
resistance (with the GLIDE switch off ...) ... an instant pitch change". The owner's manual's
pedal option uses 10 Mohm [OM p.21].

Because the error amplifier saturates on large steps, the capacitor charges from a near-constant
voltage through the pot. Large jumps therefore glide at a roughly constant rate (time per
octave), and only the last part approaches exponentially **(inferred)**. This is consistent with
the spec quoting glide as "1 millisecond to 1 second/octave" [SM p.6].

---

## 4. Published measurements (vintage unless noted)

| Quantity | Value | Source |
|---|---|---|
| Filter regeneration onset | EMPHASIS 7.5-8.0 at cutoff -1.1 (calibration); about 9 on Reid's 1976 unit; about 8 on the reissue | [SM p.35] [SOS16] |
| Filter self-oscillation, low limit | stops about 150 Hz when tracking downwards (vintage); still oscillating at 50 Hz (reissue) | [SOS16] |
| Cutoff dial scale | 2 divisions = 2 octaves (440 -> 1760 Hz) | [SM p.35] |
| Cutoff at dial -1.1, tracking off | 440 Hz (self-oscillation) | [SM p.35] |
| Cutoff full CW | >= 16 kHz, amplitude within 2 dB | [SM p.35] |
| External filter input | +2.00 V -> 440 to 1760 +/- 100 Hz | [SM p.35] |
| Keyboard control 1 alone | 3 keyboard octaves -> 1 octave (440 -> 880 +/- 50 Hz) | [SM p.35] |
| AMOUNT OF CONTOUR full | 440 Hz -> 35 +/- 10 kHz | [SM p.35] |
| Filter mod, Osc 3 square, wheel full | 440 Hz -> >= 2.4 kHz | [SM p.35] |
| Oscillator mod, Osc 3 square, wheel full | 13-23 semitones | [SM p.38] |
| TUNE full sweep | 4-6 semitones | [SM p.37] |
| Osc 2/3 FREQUENCY full sweep | 14-17 semitones | [SM p.37] |
| Pitch wheel full sweep | 13-17 semitones | [SM p.37] |
| Osc 3 LO, FREQUENCY min, OSC. 3 CONTROL off | clicks 2-5 s apart | [SM p.38] |
| External oscillator input | +2.00 V -> 440 to 1760 +/- 150 Hz | [SM p.38] |
| Filter contour | attack about 1 ms to 9 s; decay about 30 s max | [SOS16] |
| Loudness contour | attack about 1 ms to 14 s; decay/release about 30 s max | [SOS16] |
| Reissue contours | filter attack/decay max about 7 s / 30 s; loudness about 10 s / 24 s | [SOS16] |
| Filter tracking error | identical to within 2-3 Hz across the keyboard (vintage vs reissue) | [SOS16] |
| Output levels | HIGH 0.5 V typ; LOW -30 +/- 3 dB re HIGH; phones -1 +/- 3 dB (test level) | [SM p.6, p.35] |
| A-440 | 440 +/- 1 Hz, -8 +/- 2 dB | [SM p.34] |
| Noise | -5 +/- 3 dB (white and pink, audio) | [SM p.36] |

---

## 5. ATMD modelling notes (numbers to use)

- **VCOs:**
  - Saw core with a finite reset; triangle folded from the ramp by a one-transistor stage
    (slightly unequal slopes, soft corner).
  - Triangle-sawtooth = 0.82 tri + 0.18 saw. Reverse saw on Osc 3.
  - Pulse duties 50 / 29 / 15 %.
  - Ranges LO (32' - 3 oct, inferred) and 32'-2'. TUNE +/-2.5 st. Osc 2/3 FREQUENCY +/-7.5 st, or
    +/-3 oct on Osc 3 with OSC. 3 CONTROL off.
  - Per-oscillator control summing with small scale mismatches (Rev 2).
  - Slow drift (a few cents) with a warm-up offset; small Osc 2 -> 1 and Osc 3 -> 2 bleed.
  - "D board" option: common summing, tight tracking, less drift, less bleed.
- **Mixer:**
  - Linear sum with weights 33 k (oscillators, external) and 12 k (noise) into a tanh input stage
    scaled so that one oscillator at volume 10 sits near tanh(0.4).
  - External input = gain 200, plus an optional internal feedback normal from the main output
    (reissue behaviour; off by default for the original).
- **Filter:**
  - 4-pole ladder with per-stage tanh, k = 0..~4.4 mapped so that self-oscillation starts at
    EMPHASIS of about 8 (vintage about 9).
  - Uncompensated passband loss.
  - Resonance that fades at low cutoff (below about 150-300 Hz).
  - Cutoff law as in 3.4. Contour depth about 6 oct at AMOUNT 10. Key tracking 0 / 1/3 / 2/3 / 1
    referenced to the low F.
- **Contours:**
  - RC segments. Attack towards 9.3/5 of the peak, stopped at the peak.
  - tau = (100 ohm + R_audio(dial) x 1 Mohm) x 10 uF for attack, decay and release.
  - Release with DECAY off: tau = 15 ms (or the decay tau if shorter).
  - Single trigger, legato does not retrigger; attacks start from the current level.
- **VCA:** linear in the loudness contour.
- **Modulation:**
  - Osc 3 / noise crossfade (about 0.7 each at the centre). Pink/red noise for modulation.
  - Mod wheel with an audio-taper shunt law: roughly cubic depth vs travel (inferred).
  - Full wheel = about 1.5 oct p-p on pitch (13-23 st) and about 2.5 oct on cutoff.
- **Keyboard:** MIDI 41-84 shown on screen; 8' = concert pitch; low-note priority; track-and-hold
  pitch through the release; glide at roughly constant rate on large steps.
- **Bend:** +/-7 st default.

---

## 6. Physical design

### 6.1 Panel

- **Size:**
  - The face is 672 x 179 mm in `layoutmd.json` (section 7 says how it was measured).
  - About 8 mm of its top runs under the wooden top rail.
  - Aluminium side plates show about 5.5 mm at each end [PH-flat] [PH-hyv].
- **Finish:**
  - Black face on most units; early units have a dark blue-grey face [PH-monto] [PH-liu].
  - Legends are white, in a geometric sans-serif in capitals; section titles are larger and sit
    along the bottom edge [PH-flat].
- **Grid:**
  - Knob columns are on a 44.45 mm (1-3/4") pitch.
  - Rows are on 22.2 mm (7/8") steps: knob centres at about 44.3, 88.7 and 133.2 mm from the face
    top, mixer rockers on every 7/8" row. The filter-contour row is at 85.5 mm and the loudness
    row at 135.9 mm [RM p.55, measured].
- **Section dividers:**
  - Thin vertical lines separate CONTROLLERS | OSCILLATOR BANK | MIXER | MODIFIERS | OUTPUT |
    POWER.
  - A horizontal line splits the FILTER half from the LOUDNESS CONTOUR half of MODIFIERS.
  - The routing rockers sit on the dividers [RM p.55] [PH-flat].
- **Knobs:**
  - Black skirted knobs with brushed aluminium inserts: 25 mm skirt for the standard knob and 30 mm
    for the large Osc 2/3 FREQUENCY knobs [RM p.55].
  - RANGE and WAVEFORM use black pointer knobs with a small insert.
  - The parts list names "Knob, Large", "Knob, Pointer" and "Knob, Small" [SM p.38-39].
- **Rockers:** about 24 x 9 mm caps (cream ones on the left-hand panel about 28 x 10 mm)
  [RM p.55].
- **Wooden top rail:** 27-1/8" long, 1-1/4" high and 5/8" thick, with a slot 1/8" wide and 1/2"
  deep for the panel edge, and 4 wood screws through rail and panel [FM-rail]. On the Hyvönen
  unit the screws sit at 4" and 12" either side of the centre, about 8" apart **(inferred,
  [PH-hyv])**.
- **Hinge and tilt:**
  - The panel is hinged along its lower edge and lies flat for transport.
  - It is raised on a kickstand that rests against one of four screw heads, giving four angles
    (reissue) [RM p.11]. The original: "secure the Front Panel in the 'up' position by means of the
    metal bracket underneath" [OM p.4].
  - The angles are **NOT FOUND**. `layoutmd.json` uses 60 degrees as a photo-based estimate.

### 6.2 Case

- **Size:** 28-5/8 x 17-1/8 x 5-3/4" (W x D x H) with the panel down [SM p.6]. The spec page
  prints "41cm" for the depth, a misprint of 43.5 cm. The reissue gives 727 x 435 x 146 mm, 14.5 kg
  [RM p.80]; the original weighs 12.7 kg [SM p.6].
- **Wood:** the photographed units range from light stained wood to dark walnut [PH-monto]
  [PH-hyv] [PH-liu]. A dated list of woods is **NOT FOUND**.
- **Layout (measured from photos):**
  - wooden end cheeks;
  - a front lip below the key fronts;
  - a sloping wooden name board between keys and panel, carrying the manufacturer's nameplate at
    its right end (ATMD leaves it blank);
  - the left-hand controller plate (83 x 139 mm, the same depth as the white keys) [RM p.55];
  - 26 white keys on a 164.5 mm octave **(standard; photo ratio within 2 %)**.
- **Estimated dimensions:** cheek thickness, lip height and name-board height are estimates
  (+/-5-10 mm), recorded as such in `layoutmd.json`.

---

## 7. How `layoutmd.json` was made

**Coordinates:**
- The panel-drawing patch sheets of [RM] (p.55) are vector art. PyMuPDF gives every knob skirt,
  rocker, lamp, jack and divider as exact geometry in PDF points.
- The drawing matches the original panel: the five section dividers line up with the straightened
  photo [PH-flat] at a constant 5.97 px/pt (spread under 1 %). Every original control sits where
  the drawing puts it.
- The only reissue-only items are two extra rockers (OSC. 3/FILTER EG, NOISE/LFO) under the
  Controllers knobs and the LFO knob on the left-hand panel. They were left out.

**Scale:**
- The knob column pitch (33.67 pt) is taken as 1-3/4" = 44.45 mm, i.e. 1.3202 mm/pt.
- Cross-check: that makes the panel 683 mm across its side plates. The top rail is 689 mm
  [FM-rail] and stands about 3.5 mm proud at each end in the Hyvönen photo (rail/panel = 1.0137),
  which predicts 680 mm. The two agree within 0.5 %.
- Overall scale uncertainty: about +/-1.5 %.

**Labels:** they are outlined paths in the drawing, so their positions come from [PH-flat], mapped
into the drawing's frame by the divider fit. An overlay check put every knob, rocker, lamp, jack,
label and divider on the photo within about 1 mm.

**Deck:** the left-hand plate uses the same drawing (p.55, lower panel), mapped to the deck frame
at the same scale. Black keys use the twelve-equal-slot layout of a 164.5 mm octave. The case,
name board, hinge and cheek profile are photo estimates.

**Dial legends:** the angles were read from [PH-flat]. The time dials and the RANGE/WAVEFORM
detents are marked approximate.

**No logos:** the file contains no logo, wordmark or nameplate artwork.

---

## 8. Not found, uncertain

- A factory serial-number-to-date table (the 10175 change is dated only approximately).
- The exact LO-range offset (estimated as 3 octaves below 32').
- The panel tilt angles (four kickstand positions on the reissue; estimated 60 degrees).
- The cheek thickness and profile, and the key and lip heights (photo estimates).
- The ladder capacitor value (not legible on the 1971 sheet) and any later filter-board resistor
  changes beyond the early bulletin.
- The mechanism behind the vintage loss of resonance at low cutoff.
- The contour-depth discrepancy (spec 4 oct vs calibration about 6 oct vs schematic about 10 oct).
- The mixer switch "off" wiring on the 1971 sheet (it shows each rocker choosing between a direct
  path and the 33 kohm path; the leakage of an "off" source, if any, is unknown).
- Measured oscillator waveform plots of a Rev 2 unit (no scope captures under a usable license
  were found); the shapes in 3.2 are from the circuit descriptions.

---

## 9. Sources

Manufacturer documents (copyrighted; see `ref/CREDITS.md` for URLs):
- **[OM]** *A Guide to the Operation of the Mini Moog Model D*, text by Thom Rhea (1971),
  re-typeset edition with the July 1971 accessory sheets and the 4/12/72 back-panel sheet.
  archive.org `synthmanual-moog-minimoog-owners-manual`.
- **[SM]** *Service Manual for Moog Minimoog Model 204D*, Moog Music / Norlin, manual no.
  993443232402. archive.org `synthmanual-moog-minimoog-service-notes`.
- **[SCH]** R.A. Moog Co., Mini "D" schematics, drawings 1429-1456 (Nov 1970 to Jul 1971).
  archive.org `moog_MINIMOOG-D_SCHEMATICS`. Sheets:
  - p.1 contour generators;
  - p.2 block diagram;
  - p.3 waveform switching;
  - p.4 keyboard circuit;
  - p.6 noise ("random signal generator");
  - p.7 left-hand controller;
  - p.8 A-440;
  - p.9 external preamp and overload lamp;
  - p.10 VCA;
  - p.11 VCF and mixers;
  - p.13 modulation mix amplifier;
  - p.14 oscillator board.
- **[RM]** *Minimoog Model D User's Manual*, Ver. 2, Moog Music Inc. archive.org
  `minimoog-model-d-manual`.
- **[SB]** *Minimoog Service Bulletins*, archive.org `JL11354`.

Analyses and papers:
- **[ST]** T. E. Stinchcombe, "Analysis of the Moog Transistor Ladder and Derivative Filters",
  25 Oct 2008, https://www.timstinchcombe.co.uk/synth/Moog_ladder_tf.pdf
- **[HU]** A. Huovilainen, "Non-linear digital implementation of the Moog ladder filter", Proc.
  DAFx-04, Naples, 2004, pp. 61-64. https://dafx.de/paper-archive/2004/P_061.PDF
- **[DV13]** S. D'Angelo and V. Välimäki, "An improved virtual analog model of the Moog ladder
  filter", Proc. IEEE ICASSP 2013, Vancouver, pp. 729-733. Not re-read for this document; cited as
  the reference refinement of [HU].
- **[ZA]** V. Zavalishin, *The Art of VA Filter Design*, rev. 2.1.2, Native Instruments, 2020,
  chapter "Ladder filter" (cited from prior reading, not re-read for this document).
- **[SS96]** T. Stilson and J. O. Smith, "Analyzing the Moog VCF with considerations for digital
  implementation", Proc. ICMC 1996. https://ccrma.stanford.edu/~stilti/papers/moogvcf.pdf
- **[DA12]** P. Daly, "A comparison of virtual analogue Moog VCF models", MSc dissertation, Univ.
  of Edinburgh, 2012 (literature survey; background only).
- **[PAT]** R. A. Moog, US Patent 3,475,623, "Electronic high-pass and low-pass filters employing
  the base to emitter diode resistance of bipolar transistors", 1969 (expired).
- **[LW]** K. Lightner, "Minimoog Modwheel Bleedthru Explained", archive.org `JL11353`.

Reviews, references, forums:
- **[SOS16]** G. Reid, "Moog Minimoog Model D", *Sound On Sound*, Aug 2016 (vintage serial 11235
  vs the 2016 reissue), https://www.soundonsound.com/reviews/moog-minimoog-model-d
- **[WP]** Wikipedia, "Minimoog", https://en.wikipedia.org/wiki/Minimoog (production 1970-81,
  July 1981 end, over 12,000 units; reissue dates).
- **[MF]** Moog forum, "197x Minimoog Serial 5128", https://forum.moogmusic.com/viewtopic.php?p=129969
  (owner estimates; serial 6920/6921 factory move).
- **[REV]** Reverb listing "c1976 Moog Minimoog Model D" (seller's dating of a 7000s serial),
  https://reverb.com/item/6915059-c1976-moog-minimoog-model-d
- **[SCU]** scopeusers forum, "max accs", https://www.scopeusers.com/ScopeRise/issue_19/max_accs.htm
  (pre-10175 boards drift more).
- **[MF-OD]** Moog forum, "Minimoog Model D Overdrive Trick", posts by MiniMoogMe, Kevin_Lightner,
  sunny_pedaal, MC1 (Apr 2011), horowizard and Hooch (Aug 2016),
  https://forum.moogmusic.com/t/minimoog-model-d-overdrive-trick/9412
- **[FM-rail]** Moog forum, "MiniMoog Owners: could you measure a part for me?", post by
  thealien666, 27 Jun 2015, https://forum.moogmusic.com/t/minimoog-owners-could-you-measure-a-part-for-me/16311

Photos (licenses in `ref/CREDITS.md`):
- **[PH-flat]** File:Minimoog_panel.jpg (Alex Harden / Clusternote, CC BY 2.0);
- **[PH-kpr2]** kpr2 left-angle close-up (CC0);
- **[PH-hyv]** Ville Hyvönen (CC BY-SA 2.0);
- **[PH-monto]** Tiia Monto (CC BY 4.0);
- **[PH-liu]** Vincent Liu (CC BY 2.0).

---

## 10. Parameter and control IDs

Dial values are the panel numbers. "Unit" is what the dial approximately means; the laws are in
sections 3 and 5.

Defaults are an init patch:
- Osc 1 sawtooth at 8' through the open-ish filter, with medium contours.
- DECAY on, everything else routed off.

| ID | Panel legend | Kind | Range / steps | Default | Unit | What it does |
|---|---|---|---|---|---|---|
| `tune` | TUNE | knob (small) | -2.5 .. +2.5 (dial -2..2) | 0 | semitones (approx) | Master pitch of all oscillators |
| `glide` | GLIDE | knob | 0-10 | 0 | glide time, about 1 ms to about 1 s/oct | Portamento time (when `glide_on`) |
| `mod_mix` | MODULATION MIX | knob | 0 (OSC. 3) - 10 (NOISE) | 0 | crossfade | Mix of the Osc 3 waveform and the pink/red noise sent to the mod wheel |
| `osc_mod_on` | OSCILLATOR MODULATION | rocker | off/on | off | | Mod wheel -> oscillator pitch |
| `osc3_ctrl` | OSC. 3 CONTROL | rocker (vertical) | off/on | on | | Off: Osc 3 ignores keyboard, bend, modulation and external pitch; FREQUENCY becomes +/-3 oct |
| `osc1_range` | RANGE | rotary switch | 6: LO, 32', 16', 8', 4', 2' | 8' (index 3) | octave | Osc 1 octave |
| `osc2_range` | RANGE | rotary switch | 6 (as above) | 8' | octave | Osc 2 octave |
| `osc3_range` | RANGE | rotary switch | 6 (as above) | 8' | octave | Osc 3 octave |
| `osc2_freq` | OSCILLATOR-2 (FREQUENCY) | knob (big) | -7.5 .. +7.5 (dial -7..7) | 0 | semitones (approx) | Osc 2 detune |
| `osc3_freq` | OSCILLATOR-3 (FREQUENCY) | knob (big) | -7.5 .. +7.5; +/-3 oct with `osc3_ctrl` off | 0 | semitones (approx) | Osc 3 detune / free-running frequency |
| `osc1_wave` | WAVEFORM | rotary switch | 6: tri, tri-saw, saw, square, wide, narrow | saw (index 2) | | Osc 1 waveform |
| `osc2_wave` | WAVEFORM | rotary switch | 6: as Osc 1 | saw | | Osc 2 waveform |
| `osc3_wave` | WAVEFORM | rotary switch | 6: tri, **reverse saw**, saw, square, wide, narrow | saw | | Osc 3 waveform (audio and modulation) |
| `mix_osc1` | VOLUME (row 1) | knob | 0-10 | 7 | level | Osc 1 into the mixer |
| `mix_osc2` | VOLUME (row 2) | knob | 0-10 | 0 | level | Osc 2 into the mixer |
| `mix_osc3` | VOLUME (row 3) | knob | 0-10 | 0 | level | Osc 3 into the mixer |
| `ext_level` | EXTERNAL INPUT VOLUME | knob | 0-10 | 0 | level | External input (or feedback) preamp level |
| `mix_noise` | NOISE VOLUME | knob | 0-10 | 0 | level | Noise into the mixer |
| `mix_osc1_on` | ON (row 1) | rocker | off/on | on | | Osc 1 audio on/off |
| `mix_ext_on` | ON (row 2) | rocker | off/on | off | | External input on/off |
| `mix_osc2_on` | ON (row 3) | rocker | off/on | off | | Osc 2 audio on/off |
| `mix_noise_on` | ON (row 4) | rocker | off/on | off | | Noise audio on/off |
| `mix_osc3_on` | ON (row 5) | rocker | off/on | off | | Osc 3 audio on/off (not its modulation) |
| `noise_color` | WHITE / PINK | rocker (vertical) | pink / white | white | | Audio noise colour; modulation noise pink (white) or red (pink) |
| `filter_cutoff` | CUTOFF FREQUENCY | knob | -5 .. +5 | 0 | octaves (1 per division; -1.1 = 440 Hz) | Filter cutoff |
| `filter_emphasis` | EMPHASIS | knob | 0-10 | 0 | resonance | Ladder feedback; self-oscillation from about 8 |
| `filter_contour` | AMOUNT OF CONTOUR | knob | 0-10 | 5 | 0 to about 6 oct | Filter contour depth |
| `filter_mod_on` | FILTER MODULATION | rocker | off/on | off | | Mod wheel -> cutoff |
| `kbd_ctrl1` | KEYBOARD CONTROL 1 | rocker | off/on | off | | +1/3 key tracking |
| `kbd_ctrl2` | KEYBOARD CONTROL 2 | rocker | off/on | off | | +2/3 key tracking |
| `f_attack` | ATTACK TIME (filter) | knob | 0-10 (legend 10 ms .. 10 s) | 0 | s | Filter contour attack |
| `f_decay` | DECAY TIME (filter) | knob | 0-10 (legend 10 ms .. 10 s) | 4 (about 0.6 s) | s | Filter decay; release when `decay_on` |
| `f_sustain` | SUSTAIN LEVEL (filter) | knob | 0-10 | 5 | % of peak | Filter sustain |
| `l_attack` | ATTACK TIME (loudness) | knob | 0-10 (legend 10 ms .. 10 s) | 0 | s | Loudness attack |
| `l_decay` | DECAY TIME (loudness) | knob | 0-10 (legend 10 ms .. 10 s) | 4 | s | Loudness decay; release when `decay_on` |
| `l_sustain` | SUSTAIN LEVEL (loudness) | knob | 0-10 | 8 | % of peak | Loudness sustain |
| `volume` | VOLUME (MAIN OUTPUT) | knob | 0-10 | 6 | level | Main output level (and feedback amount if the normal is on) |
| `main_on` | MAIN OUTPUT ON | rocker | off/on | on | | Main output mute |
| `a440` | A-440 | rocker | off/on | off | | 440 Hz reference sine, post-filter, continuous |
| `phones_volume` | VOLUME (PHONES) | knob | 0-10 | 6 | level | Headphone level (UI/monitor only) |
| `glide_on` | GLIDE (left-hand) | rocker (cream) | off/on | off | | Portamento on/off |
| `decay_on` | DECAY (left-hand) | rocker (cream) | off/on | on | | Release = DECAY TIME (on) or about 15 ms (off) |
| `pitch_wheel` | PITCH | wheel | -1 .. +1, centre detent | 0 | +/-7 st default | Pitch bend of Osc 1/2 (and 3 with `osc3_ctrl`) |
| `mod_wheel` | MOD. | wheel | 0 .. 1 | 0 | depth | Modulation amount (audio-taper shunt law) |
| `power` | POWER | rocker (vertical, black) | off/on | on | | UI only |
| `overload` | OVERLOAD | lamp | | | | Lit by the external preamp level detector |
| `power_led` | (POWER lamp) | lamp | | | | UI only |
| `phones` | PHONES | jack | | | | UI only |
| `glide_jack`, `decay_jack` | (GLIDE / DECAY jacks) | jacks | | | | UI only (foot-switch jacks of the original) |
