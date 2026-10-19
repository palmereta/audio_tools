# KORG MS-20 (1978) hardware reference spec for software emulation

Scope: original full-size MS-20 (KLM-127/128 boards), with notes on the rev1/rev2 filter split and on the MS-20 mini (2013).
Convention: every non-obvious fact carries a source tag. **UNCONFIRMED** = not verified in a primary source; the value given is the best estimate and the reasoning is stated.

## Source keys

| Key | Source |
|---|---|
| [OM] | KORG MS-20 Owner's Manual (1978, Korg reprint), https://www.korg.com/us/support/download/manual/0/134/2441/ (PDF: https://cdn.korg.com/us/support/download/files/9af7ceee94ace953cb8abbea2d113bcd.pdf) |
| [SE] | KORG "MS-20 Original Setting Examples" (patch book, 1978), https://www.korg.com/us/support/download/manual/0/134/2442/ |
| [PB] | KORG "MS-20 Patch Book" (2015, iMS-20 derived), https://www.korg.com/us/support/download/manual/0/134/2443/ |
| [MOM] | MS-20 mini Owner's Manual, https://www.korg.com/us/support/download/manual/0/134/1955/ |
| [SM] | KORG MS-20 Service Manual (9 pp.), https://web.archive.org/web/20130625095306/http://www.cyborgstudio.com/synthmp3s/korg/ms20/manual/ms20servicemanual.pdf |
| [TS] | T. E. Stinchcombe, "A Study of the Korg MS10 & MS20 Filters" (2006/2009), https://www.timstinchcombe.co.uk/synth/MS20_study.pdf (index page: https://www.timstinchcombe.co.uk/index.php?pge=korg) |
| [SOS] | Sound On Sound, Korg MS-20 mini review (side-by-side with an original), https://www.soundonsound.com/reviews/korg-ms20-mini |
| [WP] | Wikipedia, Korg MS-20, https://en.wikipedia.org/wiki/Korg_MS-20 |
| [YEW] | Yew Tree Magic, MS-20 Retro Expansion (modder notes), https://yewtreemagic.co.uk/soundworlds3.php |
| [SLS] | Secret Life of Synthesizers, "The Korg 35 filter", https://secretlifeofsynthesizers.com/the-korg-35-filter/ |
| [CS] | Csound k35lpf/k35hpf docs + source (Pirkle AN-5/AN-7A port), https://csound.com/manual/opcodes/k35_lpf/ , https://github.com/csound/csound/blob/master/Opcodes/wpfilters.c |
| [FAUST] | Faust ve.korg35LPF / ve.korg35HPF, https://faustlibraries.grame.fr/libs/vaeffects/ , https://github.com/grame-cncm/faustlibraries/blob/master/vaeffects.lib |
| [VAFD] | V. Zavalishin, "The Art of VA Filter Design" rev 2.1, https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf |
| [CHERRY] | Cherry Audio EG-20 (MS-20 envelope clone) manual, https://docs.cherryaudio.com/cherry-audio/eg-20 |
| [MX] | MATRIXSYNTH note on filter identification, https://www.matrixsynth.com/2008/09/korg-ms20.html |
| [PRIMAL] | Primal Audio PMS-20 (MS-20 filter for Reason) announcement, https://sonicstate.com/news/2015/09/02/korg-ms-20-style-filters-for-reason |
| [PHOTO] | Measurements taken by me from the reference photos in `docs/ref/` (see section 5). Accuracy about +/-10 %. |

---

## 1. Front panel layout (control section)

### 1.1 Overall arrangement

The panel is one black slab sloped back, between two black end cheeks; the keyboard (37 keys) sits in front of it, and the CONTROL WHEEL + MOMENTARY SWITCH sit on a narrow strip left of the keyboard [OM PDF p.5 annotated photo, PHOTO].

Left 70 % of the panel = control section, organised in vertical columns separated by thin white rules. Right 30 % = patch panel (upper block) + External Signal Processor (lower block) + VOLUME/SIGNAL OUT/PHONES (top right) [PHOTO].

Column order, left to right (exact silkscreen headers) [PHOTO, OM p.4 "Normal Setting" chart]:

| # | Column header (silkscreen) | Upper part | Lower part (below a horizontal rule) |
|---|---|---|---|
| 1 | VOLTAGE CONTROLLED OSCILLATOR 1 | WAVE FORM, PW, SCALE | PORTAMENTO: TIME |
| 2 | VOLTAGE CONTROLLED OSCILLATOR 2 | WAVE FORM, PITCH, SCALE | MASTER TUNE |
| 3 | VCO MIXER | VCO 1 LEVEL, VCO 2 LEVEL | FREQUENCY MODULATION: MG/T.EXT, EG1/EXT |
| 4 | VOLTAGE CONTROLLED HIGHPASS FILTER | CUTOFF FREQUENCY (large), PEAK | CUTOFF FREQUENCY MODULATION: MG/T.EXT, EG2/EXT |
| 5 | VOLTAGE CONTROLLED LOWPASS FILTER | CUTOFF FREQUENCY (large), PEAK | CUTOFF FREQUENCY MODULATION: MG/T.EXT, EG2/EXT |
| 6 | VOLTAGE CONTROLLED AMPLIFIER | no controls: amplifier triangle graphic with an arrow into it labelled "EG 2 & EXT" (spans 2 sub-columns, top ~45 % of height) | split into MODULATION GENERATOR (LED, WAVE FORM, FREQUENCY) and ENVELOPE GENERATOR 1 (DELAY TIME, ATTACK TIME, RELEASE TIME) |
| 7 | ENVELOPE GENERATOR 2 | HOLD TIME, ATTACK TIME, DECAY TIME, SUSTAIN LEVEL, RELEASE TIME (single column, 5 knobs, full height) | — |
| 8 | Patch panel + EXTERNAL SIGNAL PROCESSOR | see section 2 | — |

A red trigger LED sits on the rule between EG1 and EG2 (it lights with the keyboard trigger) [OM block diagram p.2, SM spec "LED (KBD trigger, MG rate)"]. A second red LED in the MG section blinks at the MG rate [OM p.6].

Small round black plugs next to several knobs (after PW, after PITCH, after each SCALE, under each CUTOFF FREQUENCY, near PORTAMENTO/MASTER TUNE) are removable covers over factory trimmers ("removable covers at eight places ... never touch these controls") [OM p.8 "Caution"; PHOTO]. The ones under the filter CUTOFF knobs are the filter Fc trimmers (service manual items 1 and 2) [SM 7-4]. Draw them as decoration only.

Knob count: 32 pots + 4 rotary selectors on the whole instrument (incl. ESP and VOLUME) — matches the manual list.

### 1.2 Control-by-control

Scale convention unless noted: numerals 0 1 2 3 4 5 6 7 8 9 10 printed around the knob on a ~300 deg arc, 0 at ~7 o'clock, 5 at 12 o'clock, 10 at ~5 o'clock, with a short tick per numeral [PHOTO].

**VOLTAGE CONTROLLED OSCILLATOR 1**

| Control | Type | Markings | Function / range |
|---|---|---|---|
| WAVE FORM | 4-position rotary switch, large knob | symbols left to right: triangle `/\`, sawtooth (vertical edge + slope), pulse with double arrow `[_]<->[_` (variable pulse), noise (jagged) | triangle, sawtooth, rectangle with variable PW, white noise [OM p.5; SM spec] |
| PW | pot, small | no numerals; symbols: square `_|‾|_` at ~7 o'clock, narrow pulse `|‾|_` at 12, flat line `—` at ~5 o'clock | "At 0 the waveform is symmetrical (square). ... Near full clockwise the pulse width becomes so narrow as to virtually disappear, and no sound will be heard" [OM p.5]. Duty 1:1 to 1:infinity [SM]. No PW CV input on the original (PWM IN only on the 2015 MS-20M) [WP, YEW] |
| SCALE | 4-position rotary switch, large | 32' 16' 8' 4' | octave selector, each halving of footage = +1 octave [OM p.5] |

**VOLTAGE CONTROLLED OSCILLATOR 2**

| Control | Type | Markings | Function / range |
|---|---|---|---|
| WAVE FORM | 4-position rotary switch, large | sawtooth, square `_|‾|_`, narrow pulse `|‾|__`, `RING` | sawtooth, square, narrow (fixed-width) pulse, ring modulator [OM p.5; SM] |
| PITCH | pot, small | −5 −4 −3 −2 −1 0 +1 +2 +3 +4 +5 (0 at 12 o'clock) | VCO2 detune "over a range of +/- one octave" [OM p.5; SM "Pitch (+/-1 OCTAVES)"]. SOS measured "just over an octave" on the mini [SOS] |
| SCALE | 4-position rotary switch, large | 16' 8' 4' 2' | [OM p.5] |

**PORTAMENTO** — single knob labelled TIME, 0–10. Glide time; SM prints "max. 00 sec" (misprint). Max glide **UNCONFIRMED**: best estimate several seconds full-range (implement as RC lag on the keyboard CV with tau up to ~2 s, i.e. ~10 s to settle).
**MASTER TUNE** — knob, −5…0…+5. Range is inconsistent across Korg documents: "+/- 2 semitones" [OM p.5 text], "+/- 1/2 octave" [OM spec p.8], "+/- 100 cent" [SM spec]. **UNCONFIRMED**; recommend +/-100 cents default (SM is the technical document) with a hidden option.

**VCO MIXER**

| Control | Range | Notes |
|---|---|---|
| VCO 1 LEVEL | 0–10 | independent level of VCO1 into the filter chain [OM p.5] |
| VCO 2 LEVEL | 0–10 | ditto for VCO2 (ring mod output comes out here when VCO2 WAVE FORM = RING) |

**FREQUENCY MODULATION** (acts on both VCOs) [OM block diagram]

| Control | Source (normalled) | Range |
|---|---|---|
| MG/T.EXT | MG triangle, or whatever is patched into TOTAL | 0–10, unipolar attenuator (no polarity inversion) |
| EG1/EXT | EG1 (positive out), or whatever is patched into FREQ | 0–10 |

SM gives both as "intensity ... (+/-5V)" / "(+5V)". Depth mapping: TOTAL/FREQ are 1 V/oct inputs [OM spec "OCT/V"; SOS "Total and Freq inputs: designed to V/Oct"]. Assuming the pot at 10 passes unity: MG triangle (+/-2.5 V) gives +/-2.5 oct, EG1 (0..+5 V) gives up to +5 oct. **UNCONFIRMED** (pot law and unity point not measured).

**VOLTAGE CONTROLLED HIGHPASS FILTER / VOLTAGE CONTROLLED LOWPASS FILTER**

| Control | Range | Notes |
|---|---|---|
| CUTOFF FREQUENCY (large knob) | 0–10 → 50 Hz–15 kHz | HPF "0" = fully open, LPF "10" = fully open [OM p.5; SM spec "50Hz~15,000Hz"] |
| PEAK | 0–10 | "flat ~ self OSC" [SM]; filter self-oscillates near max [OM p.5] |
| CUTOFF FREQUENCY MODULATION MG/T.EXT | 0–10 | MG triangle / TOTAL jack [OM p.5] |
| CUTOFF FREQUENCY MODULATION EG2/EXT | 0–10 | EG2, or the respective CUTOFF FREQ jack when patched [OM p.5] |

Factory calibration anchor: CUTOFF FREQUENCY = 5, PEAK = 10, no modulation → self-oscillation at **500 Hz**, done separately for HPF and LPF [SM 7-4, Fig. 2]. A log-domain fit through 50 Hz @0, 500 Hz @5, 15 kHz @10:
`fc(k) = 500 * 2^(0.823*(k-5) + 0.0318*(k-5)^2)` Hz — derived by me from [SM], intermediate points **UNCONFIRMED**.
There is no internal keyboard tracking of either filter; tracking only by patching KBD CV OUT (Hz/V) into TOTAL or a CUTOFF FREQ jack (the factory "Hammond organ" chart does exactly this) [OM chart].

**VOLTAGE CONTROLLED AMPLIFIER** — no knob. Gain = EG2 + INITIAL GAIN jack voltage; "When the sum of both controllers reaches 5 volts, no further changes in volume will occur" [OM p.7].

**MODULATION GENERATOR**

| Control | Markings | Function |
|---|---|---|
| LED | red | blinks at MG rate [OM p.6] |
| WAVE FORM | top: square+triangle symbol; ~7 o'clock: narrow pulse + falling ramp; ~5 o'clock: rising ramp + wide pulse | continuously variable symmetry of both outputs: triangle out = falling ramp ↔ triangle ↔ rising ramp; rectangle out = narrow ↔ square ↔ wide [OM p.6 diagram; OM/SM spec symbols `|\ ~ /\ ~ /|`, `|‾|__ ~ |‾|_| ~ __|‾|`]. SM table lists "1:1 ~ 1:80" which, given the misaligned table, is most likely the symmetry range (**UNCONFIRMED** reading) |
| FREQUENCY | 0–10 | 0.1 Hz–20 Hz [SM; the value is printed one row off in the table, under "Control wheel", **UNCONFIRMED** assignment but no other row fits] |

**ENVELOPE GENERATOR 1** (DELAY TIME, ATTACK TIME, RELEASE TIME, 0–10 each). Times up to 10 s each [SM]. Cherry Audio's clone lists delay up to ~10.5 s and attack up to 30 s [CHERRY] — use SM figures for the original.
**ENVELOPE GENERATOR 2** (HOLD TIME, ATTACK TIME, DECAY TIME, SUSTAIN LEVEL, RELEASE TIME, 0–10 each). Hold up to 20 s, attack/decay/release up to 10 s, sustain 0–5 V [SM].

**Keyboard-side controls** (strip left of the keys) [OM PDF p.5 annotated photo items 26/27, SM, PHOTO]
- CONTROL WHEEL ("Programmable Control Wheel"): vertical thumbwheel rolling front/back, with a **center click** detent; output −5 V…0…+5 V, available only at the CONTROL WHEEL OUT jack (not internally connected to anything) [SM; OM p.6].
- MOMENTARY SWITCH: round push button below the wheel; output is a switch-to-ground (S-trig style) at the MOMENTARY SWITCH OUT jack [SM "Momentary switch output (⌐ GND)"; OM p.6]. Not internally connected.
- Keyboard: 37 keys, C to C, 3 octaves [OM/SM spec].

### 1.3 Normal Setting (factory "init patch") [OM p.4]
VCO1 WAVE FORM saw, PW 0, SCALE 8'; VCO2 WAVE FORM saw, PITCH 0, SCALE 8'; PORT 0; MASTER TUNE 0; VCO MIXER 10/10; HPF fc 0, PEAK 0; LPF fc 10, PEAK 0; all MG/T.EXT, EG1/EXT, EG2/EXT = 0; MG WAVE FORM center (triangle), FREQ 5; EG1 DELAY/ATTACK/RELEASE 0; EG2 HOLD 0, ATTACK 0, DECAY 0, SUSTAIN 10, RELEASE 0; no cables. ESP knobs undefined.

---

## 2. Patch panel

### 2.1 Electrical conventions printed on the panel [PHOTO patch panel; OM spec p.8; SM spec]
- Jacks: 6.35 mm (1/4") mono on the original; 3.5 mm on the mini [MOM spec; SOS].
- Trigger convention: S-trig ("⌐ GND" symbol). Trigger outputs idle at +5 V and pull to GND while active; trigger inputs fire when shorted/pulled to ground [SM "Keyboard trigger output (+5V ⌐ GND)"; KVR forum: "the MS-20's trigger triggers when the voltage goes to 0", https://www.kvraudio.com/forum/viewtopic.php?p=8885881].
- Pitch CV: Hz/V (linear response) on VCO CV IN jacks, 0…+8 V; KBD CV OUT "exponential" 0…+8 V [SM]; factory KBD CV calibration makes each C exactly half the voltage of the C above (C1:C2:C3:C4 = 1:2:4:8) [SM 7-3]. With 0…+8 V full range, top C ≈ 8 V and bottom C ≈ 1 V (**UNCONFIRMED** absolute values).
- Exponential inputs: FREQ and TOTAL = 1 V/oct; filter CUTOFF FREQ jacks = "2OCT/V" [SM; OM spec]. Stinchcombe's simulation of the OTA (rev2) filter confirms 2 oct/V (0.5 V/oct) [TS sect. 6].

### 2.2 Physical grid

Coordinates are normalised to the patch-panel photo `docs/ref/ms20_original_patchpanel_CC-BY-SA.jpg` (x to the right, y down, 0–1), measured by me [PHOTO]. Row y values are centres. Thin white lines on the panel are a printed signal-flow diagram connecting the jacks to labelled boxes.

**Top strip (y≈0.06, printed boxes, left→right):** `VOLTAGE CONTROLLED OSCILLATOR 1` → `VOLTAGE CONTROLLED OSCILLATOR 2` → `VCO MIXER` → `VOLTAGE CONTROLLED HP FILTER` → `VOLTAGE CONTROLLED LP FILTER` → `VOLTAGE CONTROLLED AMPLIFIER` → arrow to SIGNAL OUT.

| Row | y | Jacks left→right (x) |
|---|---|---|
| A | 0.20 | TOTAL (0.06), FREQ (0.19), EXT SIGNAL IN (0.30), CUTOFF FREQ [HPF] (0.41), CUTOFF FREQ [LPF] (0.51), INITIAL GAIN (0.63); VOLUME knob (0.79, 0.17); SIGNAL OUT (0.93, 0.08); PHONES (0.93, 0.20) |
| B | 0.36 | MG triangle OUT (0.06), [MODULATION GENERATOR box], MG rectangle OUT (0.24), EG1 OUT (0.33), [ENVELOPE GENERATOR 1 box], [ENVELOPE GENERATOR 2 box], EG2 REV OUT (0.68), VCO 1+2 CV IN (0.84), KBD CV OUT (0.92) |
| B' | 0.42 | VCO 2 CV IN (0.75) |
| C | 0.47 | S/H CLOCK (0.15), EG1 REV OUT (0.33), MVCA CONTROL IN PUT (0.42), EG 1 TRIG IN (0.56), TRIG IN (0.84), [KBD box], KBD TRIG OUT (0.92) |
| D | 0.59 | S/H IN (0.06), [SAMPLE & HOLD box], S/H OUT (0.24), MVCA IN (0.33), [VCA triangle], MVCA OUT (0.47), PINK (0.56), [NOISE GENERATOR box], WHITE (0.74), CONTROL WHEEL OUT (0.83, wheel symbol), MOMENTARY SWITCH OUT (0.92, push-switch symbol) |
| — | 0.65 | horizontal white rule; below it header "EXTERNAL SIGNAL PROCESSOR" |
| E | 0.74 | SIGNAL IN (0.06), [AMP triangle], OUT (pre-amp, 0.24), [BAND PASS FILTER box], OUT (BPF, 0.42), [F-V CONVERTER box], F∝V CV OUT (0.60), [ENVELOPE FOLLOWER box], ENV OUT (0.83), TRIG OUT (0.92). Red LEDs: PEAK (0.72, 0.67), TRIG (0.92, 0.67) |
| F | 0.92 | knobs: SIGNAL LEVEL (0.08), LOW CUT FREQ (0.21), HIGH CUT FREQ (0.35), CV ADJUST (0.48), THRESHOLD LEVEL (0.62); "KORG" logo + "SYNTHESIZER" at (0.82, 0.90) |

Small voltage legends printed under/next to jacks: TOTAL, FREQ, both CUTOFF FREQ: `-5V~+5V`; EXT SIGNAL IN: `3VPP MAX`; INITIAL GAIN: `0V~+5V`; MG tri: waveform `|\ /\ /|` with `+2.5V/-2.5V`; MG rect: `+5V/0V`; EG1 OUT: `+5V 0V` with trapezoid; EG1 REV OUT: `+5V 0V` inverted trapezoid; EG2 REV OUT: `0V` with inverted ADSR; MVCA CONTROL: `0V~+5V`; CLOCK, EG1 TRIG IN, TRIG IN, KBD TRIG OUT, momentary switch, ESP TRIG OUT: `⌐GND`; PINK/WHITE: noise squiggles; `-5V~+5V` near CONTROL WHEEL OUT [PHOTO].

### 2.3 Jack table (function, direction, levels, normalling)

| Section (panel) | Jack (exact label) | Dir | Level | Normalled / internal connection when empty |
|---|---|---|---|---|
| VCO+VCF mod | TOTAL | in | −5…+5 V | MG triangle → the three MG/T.EXT knobs (VCO FM, HPF, LPF). Plug breaks MG, inserted signal goes to all three [OM p.7 "TOTAL EXT ... internally patched to the MG Triangle"] |
| VCO | FREQ | in | −5…+5 V (OM spec −3…+3 V), 1 V/oct | EG1 (positive) → EG1/EXT knob → both VCOs. Plug breaks EG1 [OM p.5] |
| VCF | EXT SIGNAL IN | in | 3 Vpp max | none; summed with VCO MIXER output at the HPF input [OM p.6, block diagram] |
| VCF | CUTOFF FREQ (under HP box) | in | −5…+5 V, 2 oct/V | EG2 → HPF EG2/EXT knob. Plug breaks EG2 for HPF only [OM p.5] |
| VCF | CUTOFF FREQ (under LP box) | in | −5…+5 V, 2 oct/V | EG2 → LPF EG2/EXT knob. Plug breaks EG2 for LPF only [OM p.5] |
| VCA | INITIAL GAIN | in | 0…+5 V | none; summed with EG2 (EG2 always drives the VCA) [OM p.7] |
| MG | OUT (triangle) | out | 5 Vpp (+/-2.5 V) | — |
| MG | OUT (rectangle) | out | 0 / +5 V | — |
| EG1 | OUT | out | 0…+5 V (SM/OM spec print "−5V→0V", contradicted by panel "+5V 0V"; use panel) | also internally → FREQ normal and → MVCA CONTROL normal |
| EG1 | REV OUT | out | +5 V → 0 V (inverted) | — |
| EG2 | REV OUT | out | inverted EG2. Panel legend: "0V" on the baseline with an ADSR drawn downward, i.e. rests at 0 V and swings negative (0 → about −5 V); EG1 REV OUT by contrast is printed "+5V/0V" (rests at +5, dips to 0). SOS measured 4.82 V / 0 V for a 5 V contour on an original (magnitudes; polarity not stated) [SOS]. Polarity **UNCONFIRMED**: implement −EG2 | **EG2 has no positive output jack**: positive EG2 goes only to the VCA and the two EG2/EXT knobs [PHOTO, SM] |
| VCO | VCO 1+2 CV IN | in | 0…+8 V Hz/V | keyboard CV (post-portamento) → both VCOs; plug disconnects keyboard from both VCOs [OM p.7 MS-10 example: keyboard "disconnected by the patch cords in the VCO CV IN and TRIG IN jacks"; block diagram] |
| VCO | VCO 2 CV IN | in | 0…+8 V Hz/V | keyboard CV → VCO2; plug disconnects keyboard from VCO2 only [OM p.6, block diagram "VCO 2 ONLY EXT CV IN"] |
| KBD | KBD CV OUT | out | 0…+8 V Hz/V | taps the keyboard CV **after** PORTAMENTO [OM block diagram] |
| EG | TRIG IN | in | S-trig | keyboard trigger → EG1 + EG2 (+ trigger LED); plug replaces it [OM p.6] |
| EG | EG 1 TRIG IN | in | S-trig | the EG1+2 trigger bus → EG1; plug retriggers EG1 only [OM p.6] |
| KBD | KBD TRIG OUT (label "TRIG OUT" beside "KBD") | out | S-trig | keyboard trigger, taken before the TRIG IN switch [OM block diagram] |
| S/H | CLOCK | in | S-trig | **not normalled** (S/H only works when clocked by a patch) [OM block diagram, OM p.7 example uses MG rectangle] |
| S/H | IN | in | 5 Vpp max | not normalled |
| S/H | OUT | out | 5 Vpp max | — |
| MVCA | CONTROL IN PUT | in | 0…+5 V | EG1 (positive) [OM p.7: "The MVCA control input is internally patched to the EG1 output"] |
| MVCA | IN | in | −5…+5 V | not normalled |
| MVCA | OUT | out | −5…+5 V | — |
| NOISE GENERATOR | PINK | out | 5 Vpp | — |
| NOISE GENERATOR | WHITE | out | 5 Vpp | — (VCO1 also has an internal white-noise waveform) |
| MANUAL CONTROL | wheel symbol (CONTROL WHEEL OUT) | out | −5…0…+5 V, center click | — |
| MANUAL CONTROL | push-switch symbol (MOMENTARY SWITCH OUT) | out | switch to GND | — |
| OUTPUT | SIGNAL OUT | out | 2 Vpp, Zout 3.5 kΩ | post VOLUME |
| OUTPUT | PHONES | out | 8 Ω, 120 mW | — |
| ESP | SIGNAL IN | in | "auto pad system", input level 0 dB max, 1.0–14.0 V [SM] | — |
| ESP | OUT (after AMP) | out | 0…+8 V per OM spec (SM: "Amplifier Out") | — |
| ESP | OUT (after BAND PASS FILTER) | out | — | — |
| ESP | F∝V CV OUT | out | 0…+8.4 V, frequency-proportional (Hz/V) [SM] | — |
| ESP | ENV OUT | out | 0…+5 V [SM] | — |
| ESP | TRIG OUT | out | +5 V → GND (S-trig) [SM] | — |

Patch-panel/ESP knobs (0–10 scales): VOLUME (with click-off "POWER OFF" at full CCW; numerals 1–10) [PHOTO], SIGNAL LEVEL (input gain, 0 dB max), LOW CUT FREQ (50–2,500 Hz), HIGH CUT FREQ (100–5,000 Hz), CV ADJUST (F-V scaling), THRESHOLD LEVEL (trigger detector) [SM]. LEDs: PEAK (input overload), TRIG (trigger detector) [SM, OM p.7].

### 2.4 Normalling mechanics ("the trick")
- All normalled inputs use switching jacks: the internal source is connected through the jack's switch contact; inserting a plug lifts the contact and the plug's signal replaces the internal one. Full insertion = replace [OM p.5–7; SOS].
- Inserting a **dummy plug** (cable with the far end free) into FREQ, TOTAL, or a CUTOFF FREQ jack therefore *removes* that modulation; the knob then attenuates 0 V. In the emulation: `inputSignal = jack.connected ? jack.cableSignal : normalSource`, where an unconnected-far-end cable contributes 0 V.
- Knobs downstream of a normalled jack become attenuators for the patched signal (e.g. EG1/EXT becomes "wheel pitch-bend depth" when the wheel is patched into FREQ) [OM p.5].
- **Half-insertion hack**: on the original, half-inserting a 1/4" plug can leave the normal contact closed while the tip touches the plug, so the pre-patched CV and the patched CV are *mixed* [SOS: "Fully inserting a cable on the original replaces the pre-patched CV. Half-insertion mixes the two CVs"]. Optional feature: a per-jack "half-inserted" state that sums instead of replacing.
- Patching KBD CV OUT or KBD TRIG OUT does not affect normal operation (outputs). Patching VCO 1+2 CV IN or TRIG IN disconnects the keyboard [OM p.7].

---

## 3. Electrical / behavioural specification

### 3.1 Keyboard and controllers
- 37 keys C–C, monophonic, **high-note priority**, single-trigger (legato: no envelope retrigger while another key is held) — high-note priority stated for the mini [MOM "the highest tone played will be produced"; SOS "one note, high priority, single triggering"]; original reported as "high-note legato priority (envelope does not retrigger)" [https://forum.renoise.com/t/note-off-tool-issues-sequencing-korg-ms-20/68124]. Original high-note priority: consistent across sources, not measured by me.
- KBD CV is Hz/V (frequency ∝ voltage), portamento applied before KBD CV OUT [SM, OM block diagram]. Emulation: compute the glide in the **voltage** (linear frequency) domain, not in semitones, since the analog lag acts on an Hz/V voltage. This makes glides curve upward in pitch (fast at the start in log-pitch when going up).
- Absolute pitch: footage convention (8' bottom C = C2 ≈ 65.4 Hz) **UNCONFIRMED**; one KVR post says the manual places the 0 V point of the exponential scale at C3 ≈ 130.81 Hz (https://www.kvraudio.com/forum/viewtopic.php?p=7539357). Tuning is set with MASTER TUNE in practice.
- VCO range: "6 octaves + 6 octaves (FM)" per VCO [OM/SM spec]; VCO1 32'–4', VCO2 16'–2'.
- Pitch stability: factory spec within +/-10 cents across scales, KBD CV within +/-2 cents [SM 7-2/7-3]. Model a small static detune per VCO and slow drift (<5 cents) for realism.

### 3.2 VCOs
- VCO1 waveforms: triangle, sawtooth, rectangle with PW 50 %→0 % (1:1 to 1:infinity; silent at the end of travel), white noise [OM p.5; SM]. Only one waveform at a time leaves VCO1 (selector).
- VCO2 waveforms: sawtooth, square (50 %), narrow pulse (fixed duty; **UNCONFIRMED** duty, best estimate 10–15 % from the panel symbol), RING [OM; SM].
- Saw polarity: panel symbol is a vertical rise followed by a falling slope (falling ramp) [PHOTO]. Audibly irrelevant except when mixing VCO1 and VCO2 saws at unison; implement falling ramp.
- **RING is not a multiplier**: it is an XOR (exclusive-OR) of VCO1's rectangle (with the PW setting) and VCO2's square. "Only sounds like ring modulation in the special case that VCO1 and VCO2 are both square or pulse waves" [YEW; WP citing the same, ARP Odyssey-style]. The OM confirms PW affects it ("The two Scale controls as well as the Pitch and PW controls all affect the resulting sound") [OM p.5]. Implement: `ring = (pulse1(t, PW) XOR square2(t)) ? +A : -A`, band-limited (see 4.4). VCO1's own WAVE FORM selector does not change the ring output (UNCONFIRMED but consistent with the XOR description).
- No hard sync, no linear FM, no PWM input on the original (the MS-20M adds sync/FM toggles and PWM IN) [WP].
- Level into the mixer: **UNCONFIRMED**; assume ~5 Vpp per VCO (consistent with the 5 Vpp noise/MG levels) — this matters for filter drive (3.5).
- Pitch control routes: keyboard (Hz/V, linear), VCO CV IN jacks (Hz/V, linear), FREQ/TOTAL via FM knobs (1 V/oct, exponential), MASTER TUNE, VCO2 PITCH [OM block diagram; SM].

### 3.3 Modulation generator, S/H, noise, MVCA
- MG frequency 0.1–20 Hz [SM, see caveat in 1.2]. Waveform knob morphs both outputs together: triangle ramp-down ↔ triangle ↔ ramp-up; rectangle narrow ↔ square ↔ wide, up to about 1:80 asymmetry (UNCONFIRMED reading of SM) [OM p.6].
- Rectangle output 0 / +5 V; triangle output +/-2.5 V [panel, SM]. Phase relation **UNCONFIRMED**; the panel symbols (narrow pulse paired with fast-rise ramp) imply rectangle high during the rising segment of the triangle. Implement: `rect = triRising ? +5 : 0`.
- MG triangle is what feeds TOTAL (only the triangle is normalled) [OM p.6–7].
- S/H: clocked by an S-trig at CLOCK (sample when the clock input is pulled to ground, i.e. on the falling edge of a 0/+5 V square such as MG rectangle); holds input until next clock; input/output 5 Vpp max [SM; OM p.7 example]. Manual tip: with MG rectangle as clock, set MG WAVE FORM fully clockwise for best results [OM p.6].
- Noise: white and pink, 5 Vpp, "pink noise is a darker sounding white noise" [OM p.7; SM].
- MVCA: linear VCA, control 0…+5 V (normalled to EG1), signal −5…+5 V; intended for delayed vibrato (MG tri → MVCA IN, MVCA OUT → TOTAL) or wheel-controlled modulation [OM p.7]. SOS found the original's mod VCAs "hotter": setting 10 on the mini ≈ 9 on the original [SOS].

### 3.4 Envelopes
- EG1: trapezoid. On trigger: wait DELAY, rise over ATTACK to +5 V, stay at +5 V while the trigger is held, fall over RELEASE to 0 when released [OM p.6; SM]. Ranges 0–10 s each [SM]. Normalled to FREQ (pitch, via EG1/EXT) and MVCA control. Outputs: OUT 0→+5 V, REV OUT +5→0 V.
- EG2: H-A-D-S-R. "When triggered the EG voltage rises to a peak at a rate set by Attack, then falls at the Decay rate to the Sustain level, and eventually falls to zero following termination of the trigger signal and the end of the Hold Time at the Release rate" [OM p.6]. HOLD "extends the trigger signal by a variable amount of time" [OM p.6]. Exact hold semantics **UNCONFIRMED**: OM wording ("duration of the trigger signal plus time set by Hold") suggests gate-off + hold; Cherry's clone documents it as holding the gate high after a short strike [CHERRY]. Recommended model: monostable started at trigger, `gateEff = gate OR (t - tTrig < tHold)`; switch to "gate-off delay" if A/B listening disagrees. Ranges: hold 0–20 s, A/D/R 0–10 s, sustain 0–5 V [SM].
- Peak level 5 V for both. Curves: **UNCONFIRMED**; use analog RC shapes (exponential). Attack: charge toward an overshoot target (~1.2–1.5x of 5 V) and switch to decay when 5 V is reached; decay/release: exponential toward target with time constant set so the knob value means ~-60 dB time (or 5 tau).
- Knob law: audio/log taper; assume time = tmin * (tmax/tmin)^(k/10) with tmin ≈ 1 ms, tmax = 10 s (hold 20 s). **UNCONFIRMED**.
- Retrigger: single-trigger keyboard; legato key changes do not retrigger [see 3.1].

### 3.5 Filters
Topology: HPF then LPF in series; mixer + EXT SIGNAL IN feed the HPF; LPF feeds the VCA [OM block diagram].

**Revisions**
- **Rev1 (early units, MS-10, MS-20 mini, Monotron): Korg35** — Korg-proprietary resin-sealed thick-film module (5 transistors, 6 resistors), a Sallen-Key topology whose two "resistors" are transistors biased in reverse saturation (current-controlled resistors) [TS sect. 1–3; SLS]. Two Korg35 per MS-20 (LP and HP) [SLS].
- **Rev2 (later units): LM13600 OTA daughterboard KLM-307** — two cascaded, buffered first-order OTA sections with feedback, not Sallen-Key [TS sect. 1–2]. Identified by the daughterboard and usually a front-panel screw near the VCA graphic (not 100 % reliable) [MX; SLS reader comment]. Changeover "half-way through the production lifecycle"; no serial-number cutoff published [SOS search summary; WP]. MS-20 FS and MS-20 Kit offer both, labelled Rev1/Rev2 [WP].
- The **MS-20 mini uses the Korg35 (rev1) design** [SOS: "Korg decided to use the '35' in the Mini"].
- Sound: Korg35 version described as more aggressive, with a "slight amount of hiss which overdrives into the sound at high resonant settings"; rev2 cleaner, less aggressive (MusicRadar/forum quotes collected via https://www.musicradar.com/reviews/tech/korg-ms-20-mini-574164 and https://gearspace.com/threads/ms-20-filter-difference.473473/ — secondary, **UNCONFIRMED**).

**Transfer functions** [TS eqs. 5–8, sect. 7] (ωc = cutoff, k1·k2 = loop gain set by PEAK)
- Rev2 (OTA) LPF: `H(s) = -k1 / (s²/ωc² + (2 - k1k2)·s/ωc + 1)`; oscillates at k1k2 = 2. k1 = 1, k2 = 0…2.2 (pot 0…1/1.82 × op-amp gain ≈ 4) [TS 2.1].
- Rev1 (Korg35) LPF with C1 = 3·C2 (and R1 ≈ R2/3): `H(s) = k1 / (s²/ωc² + (2⅓ - k1k2)·s/ωc + 1)`; oscillation at k1k2 = 2⅓. k1 = JFET buffer (≈1–1.1) × op-amp 1+270k/4.7k = 58.4; k2 = 0…1/26.5 (divider R60/R61 via the PEAK pot); k1k2,max ≈ 2.2–2.4, i.e. just at the oscillation threshold [TS 2.2, eq. 8].
- **Both HPFs are 6 dB/oct, not 12**: Korg grounded the LP input and fed the signal into the bottom of C2; the result is a 12 dB HP in parallel with a 6+6 dB BP, net 6 dB/oct below cutoff [TS sect. 7: `Vo/Vin = (2k1 s/ωc + k1 s²/ωc²) / (s²/ωc² + (3 - k1k2) s/ωc + 1)`; SOS: "HPF: 6 dB/oct high-pass with resonance"]. LPF: 12 dB/oct [TS Fig.14; SOS].
- Cutoff CV law: Korg35 core simulated at ~1 V/oct (22 Hz–4.9 kHz over 8 V steps), OTA at 2 oct/V; panel jacks specified 2 oct/V [TS sect. 3, 6; SM].
- Simulated Korg35 range: ~22 Hz (−5 V) to ~12 kHz (+5 V) [TS Fig. 14]; panel spec 50 Hz–15 kHz [SM].

**Nonlinearity — the "scream"** [TS sect. 4, 5]
- Both versions have back-to-back diodes in the gain element; the position differs:
  - **Rev1 Korg35: diodes in the forward path** (single 1N4148-type pair across the ×58 op-amp). Small-signal gain 58; once the output exceeds one diode drop (~0.5 V) the stage becomes a unity follower offset by the diode drop. Distortion therefore hits **all frequencies** (passband too) and grows with input level: louder mixer settings = dirtier [TS Fig. 16–21].
  - **Rev2 OTA: diodes in the feedback path** (string of 3 diodes each way, ~2 V total drop) across a ×4 stage. Only frequencies near cutoff (the resonant peak) are limited; the passband stays clean [TS Fig. 20, 22].
  - Test to tell them apart: fc high, low-frequency sine in, raise level — gets richer = Korg35, stays clean = OTA [TS sect. 5].
- Resonance "starts at the zero crossing" because the diode-limited loop has its highest gain near 0 V; this is a diode effect, not a Sallen-Key property [TS sect. 8, Myth 2]. This amplitude-dependent gain is what keeps self-oscillation bounded and gives the squealing, compressed resonance.
- **Rev1 asymmetric resonance**: the signal modulates the Korg35 transistor currents, so the negative half-cycle rings stronger and at a higher frequency (simulated ringing ~1.7 kHz positive half vs ~2.0 kHz negative half) [TS sect. 4, Fig. 15]. Rev2 is symmetric.
- Rev1 also has CV bleed-through (Stinchcombe's guess for why Korg dropped it) [TS sect. 9] — optional: add a small amount of cutoff CV to the audio path (thumps on fast EG sweeps).
- Self-oscillation: mini's is "noticeably brighter" than an original's [SOS]; self-oscillation waveform is near-sine at low PEAK excess, flattening/asymmetric when driven hard (from the diode limiter), level set by the diode drop.

**Driving the filter** — VCO MIXER levels set the HPF input level; with rev1 the forward diodes saturate at ~8 mV at the op-amp input × 58 ≈ 0.5 V out, so normal VCO levels already run the stage into its nonlinear region at high mixer settings [TS sect. 5 numbers]. Practical emulation: a pre-filter gain `drive = (VCO1 LEVEL·v1 + VCO2 LEVEL·v2 + extIn)/10` with the nonlinearity inside each filter; do not normalise it away. Calibrate so mixer 10/10 with saws gives audible but not fuzz-box saturation through an open LPF.

### 3.6 VCA, output
- VCA control = EG2 (0…5 V) + INITIAL GAIN (0…5 V), gain saturates at 5 V total [OM p.7]; response law **UNCONFIRMED** (assume linear in control voltage).
- Original VCA has audible hiss; the mini's VCA was redesigned for better S/N, everything else is a faithful reproduction [MOM; SOS]. Optional: VCA noise floor.
- SIGNAL OUT 2 Vpp, 3.5 kΩ; PHONES 8 Ω 120 mW; VOLUME doubles as power switch [SM; PHOTO].

### 3.7 External Signal Processor (ESP) [OM p.7–8; SM]
Chain: SIGNAL IN → SIGNAL LEVEL → high-gain preamp (AMP, "auto pad system") → OUT → BAND PASS FILTER (LOW CUT 50–2,500 Hz HP edge, HIGH CUT 100–5,000 Hz LP edge) → OUT → F-V CONVERTER (pitch-to-voltage, frequency-proportional, CV ADJUST scales it) → F∝V CV OUT (0…+8.4 V, Hz/V so it plays the VCO CV IN directly).
Parallel: preamp → ENVELOPE FOLLOWER → ENV OUT (0…+5 V) → trigger detector (Schmitt comparator with THRESHOLD LEVEL) → TRIG OUT (S-trig) + TRIG LED. PEAK LED on input level.
Manual setup: SIGNAL LEVEL so PEAK only flickers; THRESHOLD until TRIG lights at medium/high playing; F∝V CV OUT → VCO 1+2 CV IN, ESP TRIG OUT → TRIG IN, ENV OUT → INITIAL GAIN; BPF: LOW CUT 10 / HIGH CUT 0, raise HIGH CUT on the highest note, lower LOW CUT on the lowest; CV ADJUST until pitch matches [OM p.7–8]. Note the BPF output feeds the F-V converter internally (normalled) even with nothing patched [OM block diagram].
DSP suggestion: preamp = gain + soft clip; BPF = 2nd-order HP + 2nd-order LP; F-V = zero-crossing period tracker with hysteresis (or YIN on a decimated signal) producing a linear-in-Hz CV with a lag; envelope follower = full-wave rectifier + RC (attack ~1 ms, release ~50 ms, UNCONFIRMED); Schmitt trigger with ~10 % hysteresis.

### 3.8 Ranges summary table

| Parameter | Value | Source |
|---|---|---|
| VCO1 scale | 32'/16'/8'/4' | OM, SM |
| VCO2 scale | 16'/8'/4'/2' | OM, SM |
| VCO2 PITCH | +/-1 octave (−5…+5 dial) | OM, SM |
| MASTER TUNE | +/-100 cent (SM) / +/-2 st (OM text) / +/-½ oct (OM spec) | conflicting, UNCONFIRMED |
| PW | 1:1 → 1:∞ | SM |
| Portamento | max printed "00 sec" | SM misprint, UNCONFIRMED |
| HPF/LPF fc | 50 Hz–15 kHz; knob 5 = 500 Hz (calibration) | SM |
| HPF slope | 6 dB/oct | TS, SOS |
| LPF slope | 12 dB/oct | TS, SOS |
| Filter ext CV | 2 oct/V, −5…+5 V | SM, TS |
| FREQ / TOTAL | 1 V/oct, +/-5 V (OM: +/-3 V) | SM, OM |
| MG | 0.1–20 Hz; tri 5 Vpp; rect 0/+5 V | SM |
| EG1 | delay/attack/release 0–10 s; 0…+5 V | SM |
| EG2 | hold 0–20 s; A/D/R 0–10 s; S 0–5 V | SM |
| Noise | white, pink, 5 Vpp | SM |
| S/H | 5 Vpp in/out, S-trig clock | SM |
| Wheel | −5…0…+5 V, center click | SM |
| ESP BPF | low cut 50–2,500 Hz; high cut 100–5,000 Hz | SM |
| ESP F-V | 0…+8.4 V | SM |
| Signal out | 2 Vpp, 3.5 kΩ | SM |
| Size / weight | 569 × 309 × 249 mm, 7.7 kg (mini: 493 × 257 × 208 mm, 4.8 kg, 86 % scale) | SM, MOM, WP |
| Power | 10 W | SM |

---

## 4. Recommended DSP models

### 4.1 Linear core: Pirkle Korg35 TPT (ZDF) — verified against Csound source [CS] and Faust [FAUST]
Per sample, with `g = tan(π·fc/fs)`, `G = g/(1+g)`, one-pole TPT `v=(u−z)·G; lp=v+z; z=lp+v`:

LPF (Pirkle AN-5, as in Csound `k35_lpf_perf`):
```
K in [0.01 .. 2.0]                 // resonance; self-osc near 2
alpha  = 1 / (1 - K*G + K*G*G)
beta2  = (K - K*G) / (1 + g)       // LPF2 state weight
beta3  = -1 / (1 + g)              // HPF1 state weight
lp1    = onepoleLP(z1, x)
u      = alpha * (lp1 + S35)
u      = tanh(sat * u)             // optional NLP (Pirkle places it here)
lp2    = onepoleLP(z2, u);  y = K * lp2
lp3    = onepoleLP(z3, y)          // HPF1 in feedback = y - lp3
S35    = beta2*z2 + beta3*z3
out    = y / K
```
HPF (AN-7A, Csound `k35_hpf_perf`): HPF1 on input, `u = alpha*(hp1 + S35)`, `y = K*u`, optional `tanh`, then HPF2 and LPF1 in the feedback with `beta_hpf2 = -G/(1+g)`, `beta_lpf1 = 1/(1+g)`, `out = y/K`.
Faust uses the same coefficients with `K = 2·(Q − 1/√2)/(10 − 1/√2)` [FAUST]. Note: this model's resonance K is not the MS-20's physical k1k2, and its HPF is 12 dB/oct, **not** the hardware's 6 dB/oct; the tanh is applied outside the implicit solve (semi-implicit), which is cheap but slightly mistunes heavy self-oscillation.

### 4.2 Recommended MS-20-specific structure (my derivation from [TS] + TPT, use this)
Write the filter the way Stinchcombe shows it: stage 1 is a one-pole whose capacitor "ground" is driven by the feedback `b`, stage 2 a plain one-pole. With capacitor voltage `vC`: `V2 = vC + b`, `dvC/dt = ωc((a − b) − vC)`, so `V2 = LP1(a − b) + b`, `y = LP2(V2)`.

TPT form (β = 1 − G; s1, s2 = one-pole states):
```
V2 = G*a + (1-G)*b + β*s1
y  = G*V2 + β*s2      =>  y = G²a + G(1-G)·b + Gβ·s1 + β·s2
```
- **Rev2 (OTA)**: a = −x (inverting input, flip output sign), `b = kp · Dfb(y)` with `kp = PEAK/10 · 0.55` (pot divider 0…1/1.82) and the feedback amp `Dfb(v) = v + Vd·tanh(3v/Vd)`, `Vd ≈ 2.1 V` (×4 small-signal, unity + 2 V offset once the 3-diode strings conduct) [TS 2.1, 5]. Oscillation threshold at loop gain 2 ⇒ kp·4 ≥ 2·(1+margin). Linear solve gives Pirkle's `alpha = 1/(1 − K·G·(1−G))` with K = 4kp.
- **Rev1 (Korg35)**: Sallen-Key with C1 = 3·C2 ⇒ use G1 for stage 1 at ωc and G2 for stage 2 (both tuned to ωc in the ideal case) plus the loading term; simplest faithful shortcut: same two-stage structure, threshold 2⅓, and put the nonlinearity **in the forward path**: `y = Dfwd(LP2(...))` with `Dfwd(u) = u + Vd·tanh((k1−1)·u/Vd)`, `k1 ≈ 58`, `Vd ≈ 0.5 V`, signal pre-scaled by 1/58 (the stage sees mV-level signals) [TS 5]. Add signal-dependent cutoff: `ωc,eff = ωc·(1 − γ·y)` with γ chosen to give ~15 % higher ringing frequency on the negative half (1.7 vs 2.0 kHz) [TS 4] — γ value UNCONFIRMED, tune by ear.
- **Implicit solve**: `y − K·G(1−G)·D(y) − C = 0` (rev2) — 2–4 Newton iterations per sample, warm-started from the previous y, `y ← y − F(y)/F'(y)`; or the cheaper "tanh(x)/x" linearisation (evaluate the slope of D at the previous sample, solve linearly) as discussed in [VAFD] and KVR threads.
- **HPF**: same core, input injected at the bottom of C2 with the LP input grounded [TS sect. 7] → keeps the hardware's 6 dB/oct slope and its resonance shape. Do not substitute a 12 dB HP.
- Cutoff mapping: knob law of 1.2 + exponential CV sum (2 oct/V at the CUTOFF jacks after the EG2/EXT attenuator, plus MG/T.EXT) → clamp ~10 Hz…0.45·fs_os.

### 4.3 Oversampling
- Filters: minimum 4x (Primal Audio's commercial MS-20 filter uses "4-X oversampling" with a ZDF loop [PRIMAL]); 8x recommended when PEAK > 8 or with audio-rate cutoff FM (e.g. VCO → CUTOFF FREQ patches, common on the MS-20). Polyphase half-band IIR/FIR down-sampling.
- Run VCOs (PolyBLEP/BLEP), mixer, both filters and VCA inside the oversampled domain so audio-rate patch-panel modulation (S/H OUT, ring, noise into cutoff) stays alias-free. Control-rate sources (EG, MG, wheel) may run at base rate and be linearly interpolated.

### 4.4 Other blocks
- VCOs: PolyBLEP saw/pulse, PolyBLAMP or integrated-BLEP triangle; XOR ring = sum of BLEP edges of both input pulses (edges occur at either oscillator's transitions). Noise mode of VCO1 = white noise at oscillator level.
- Pink noise: Paul Kellet's 7-pole filter on white (−3 dB/oct).
- S/H: sample-on-falling-edge of the S-trig input with sub-sample edge timing (BLEP the step if S/H OUT feeds audio).
- Keyboard: Hz/V voltage → portamento lag (one-pole on volts) → frequency = k·V (linear). FM paths: exponential `f = f0·2^(sum of 1V/oct mods)`.
- Patch engine: evaluate the module graph per oversampled block with a 1-sample delay only on feedback loops created by the user (e.g. LPF OUT is not on the panel, but VCO→cutoff→... loops via MVCA can form); keep normalled paths delay-free.

### 4.5 Known measurements / character notes
- [TS Fig. 14] Korg35 LP response, cutoff stepped −5…+5 V: clean 12 dB/oct slopes, upper steps bunch up above ~5 kHz.
- [TS Fig. 18, 21] Korg35 diode limiter: progressive rounding of a 500 Hz sine with level even with fc ≈ 7 kHz; OTA version stays clean in the passband.
- [SOS] side-by-side original vs mini: raw waves slightly brighter on the original; EG2 REV OUT 4.82 V/0 V (original) vs 6.48 V/1.23 V (mini); mod VCA gain differences; otherwise "all-but identical".
- Practical: the MS-20 bass/lead character = HPF 6 dB + resonant LPF with forward-path saturation; EG2 → both filters; high PEAK on both filters at once gives the classic band-limited "scream".

---

## 5. Visual design

### 5.1 Panel and case
- Panel: matte black anodised/painted metal, near-black (`#1b1b1b`–`#222222`; photo-sampled, varies with lighting) [PHOTO].
- Silkscreen: off-white / very light grey (`#e6e6e0`), all uppercase. Section rules: thin (≈0.6 mm) white lines; patch panel uses the same white for the flow diagram, boxes and arrows [PHOTO].
- Typeface: neo-grotesque sans, Helvetica-like (Helvetica / Univers style), regular weight for labels, slightly condensed for section headers; numerals Helvetica-style. Closest digital substitutes: Helvetica Neue Medium, "Nimbus Sans", or Inter Tight. **UNCONFIRMED** (no Korg font spec found). Logo: bold squared "KORG" 1970s logotype over "SYNTHESIZER" in a condensed grotesque, bottom-right of the ESP block [PHOTO].
- End cheeks: black (not wood) on the original and on the mini [PHOTO; MOM photos]. No wooden side panels on any stock MS-20.
- Keys: white and black plastic, full-size on the original, mini keys on the mini [MOM spec; SOS].
- Front-panel proportions (measured from the CC0 photo, ±10 %): visible panel ≈ 540 mm × 176 mm (≈3.1 : 1). Column widths ≈ VCO1 47 mm, VCO2 41, MIXER 37, HPF 38, LPF 38, VCA block 74 (MG 37 + EG1 37), EG2 38, patch/ESP 228 mm. Patch block ≈ 228 × 175 mm (≈1.3 : 1); ESP occupies its lower ≈ 35 %. Whole instrument 569 × 309 × 249 mm [SM].
- Knob row centres from the panel top (≈): 30 mm (WAVE FORM/LEVEL/CUTOFF/HOLD), 72 mm (PW/PITCH/PEAK), 105 mm (SCALE), 125 mm (MG/T.EXT row), 155 mm (EG/EXT row, PORT/TUNE). EG2's five knobs evenly spread top to bottom.

### 5.2 Knobs
- Two-tier black plastic knobs: a smooth flat-topped cap on a wider fluted (vertical ridges) skirt; a white indicator line runs radially across the cap from centre to edge [PHOTO VCO-section].
- Large (WAVE FORM ×2, SCALE ×2, CUTOFF FREQUENCY ×2, VOLUME): skirt ≈ 28 mm; small (all others): skirt ≈ 20 mm [PHOTO, ±10 %].
- Rotary selectors use the same large knob with detented positions on the printed symbols.
- Mini: same style at 86 % scale; pots "exhibit a fair degree of play" [SOS].

### 5.3 Jacks, LEDs, cables
- Original: 1/4" jacks with round bright-nickel bushing nuts (light silver-grey ring ~13 mm on black), black hole [PHOTO]. Mini: 3.5 mm jacks [MOM].
- LEDs: small red 3 mm, flush (MG, trigger, ESP PEAK, ESP TRIG) [PHOTO].
- Patch cables: original shipped with 2 × 35 cm patch cords + a 3 m connecting cord [SM]; photos of originals show thin black (or very dark) cables with black moulded 1/4" plugs [PHOTO front]. Colour of factory cords **UNCONFIRMED** (best estimate black). Mini ships with 10 patch cords [MOM] (colour **UNCONFIRMED**, likely black); Korg's MS-CABLE-18 accessory set is red/orange/yellow/green/blue/purple (https://www.adorama.com/komscable18.html). Recommendation: black default, optional coloured set; draw cables as sagging catenaries with a straight plug body (~35 mm on the 1/4" scale).
- Control wheel: ridged dark wheel protruding through a slot, centre detent; momentary switch: small round chrome/black push button [PHOTO].

### 5.4 Reference photos

Saved to `/Volumes/Docs/Projects/own/ms20/docs/ref/` (reference only, not for redistribution; licences noted):

| File | Content | Source / licence |
|---|---|---|
| `ms20_original_full_CC0.jpg` (7875×5296) | full instrument, straight-on, b/w | https://commons.wikimedia.org/wiki/File:Korg_Synthesizer_MS-20.jpg — Wilfredor, CC0 |
| `ms20_original_front_CC-BY-SA.jpg` (2838×1688) | full instrument, colour, two patch cords | https://commons.wikimedia.org/wiki/File:Korg_MS-20.jpg — Zinnmann, CC BY-SA 3.0 |
| `ms20_original_patchpanel_CC-BY-SA.jpg` (2472×1879) | patch panel + ESP, all labels legible | https://commons.wikimedia.org/wiki/File:Korg_MS-20_Patch-Feld.jpg — Zinnmann, CC BY-SA 3.0 |
| `ms20_original_vco_section_CC-BY-SA.jpg` (2759×1457) | VCO → EG2 sections, all knob legends legible | https://commons.wikimedia.org/wiki/File:Korg_MS-20_VCO_Section.jpg — Zinnmann, CC BY-SA 3.0 |

Further high-resolution references (not downloaded):
- https://commons.wikimedia.org/wiki/File:Korg_MS-20_knobs_details_(2017-03-08_@pxhere_1046124).jpg (6000×4000, CC0, knob close-up)
- https://commons.wikimedia.org/wiki/File:Korg_MS-20_Synthesizer_at_SMEM_Playroom.jpg (4032×3024, CC BY 4.0)
- https://commons.wikimedia.org/wiki/File:Korg_MS-20_Synthesizer_at_SMEM_Playroom_(detail).jpg (4032×3024, CC BY 4.0)
- https://commons.wikimedia.org/wiki/File:Korg_MS-20_patch_1.jpg (patched unit, CC BY-SA 2.0)
- https://commons.wikimedia.org/wiki/File:Korg_MS20_-_zoomed_%26_angled_(2008-10-27_21.39.08_by_Noel_Feans).jpg (CC BY 2.0)
- https://www.korg.com/us/products/synthesizers/ms_20mini/ (MS-20 mini product photos)
- https://www.korg.com/us/products/synthesizers/ms_20fs/ (MS-20 FS photos, incl. coloured editions)
- [OM] PDF p.5: annotated front-panel photo with all 42 numbered controls (in the [OM] PDF)

---

## 6. Classic patches (factory setting charts)

Source: [OM] "Setting Charts" pp. 25–36 and [SE] p.1, read off the chart images. Values are dial positions (0–10, PITCH/TUNE −5…+5). Unlisted knobs = Normal Setting (section 1.3) except: everything not listed in a chart was drawn blank, so use 0 for mod amounts and EG1 times. Cable notation `SRC → DST` uses the jack names of section 2.3. "EG2 HADSR" = HOLD/ATTACK/DECAY/SUSTAIN/RELEASE.

1. **Synthe Bass** [OM p.25] — "use the control wheel for fine pitch bends". VCO1 triangle 32'; MIX 10/0; HPF fc 3 pk 5, EG2/EXT 5; LPF fc 4 pk 5, EG2/EXT 6; FM MG/T.EXT 5; EG2 0/0/2/3/1. Cable: CONTROL WHEEL OUT → TOTAL (wheel replaces MG; FM MG/T.EXT = bend depth).
2. **Hammond Organ** [OM p.26] — VCO1 PW wave, PW ≈ 1–2, 16'; VCO2 square, PITCH +3 (text says +5), 4'; MIX 8/10; HPF 0/0; LPF fc 3.5 pk 5, MG/T.EXT 7, EG2/EXT 5; EG2 0/0/0/3/1. Cable: KBD CV OUT → TOTAL (keyboard tracking of LPF via MG/T.EXT; also the reason VCO FM MG/T.EXT stays 0).
3. **Parallel thirds and fifths** [OM p.27] — VCO1 saw 32'; VCO2 saw 16', PITCH "+3 or 4" (tune to a 3rd/5th above); MIX 10/10; HPF fc 3 pk 5, EG2/EXT 8; LPF fc 4 pk 5, EG2/EXT 8; EG2 0/1.5/4/5/2. No cables.
4. **Voice** [OM p.28] — "move the control wheel to create vowel sounds". VCO1 saw 32'; MIX 10/0; HPF fc 5 pk 5, MG/T.EXT 5, EG2/EXT 8; LPF fc 5 pk 5, MG/T.EXT 7, EG2/EXT 3; FM 0/0; EG2 0/1/2/5/0. Cable: CONTROL WHEEL OUT → TOTAL (wheel sweeps both filters = formant).
5. **Pitch Bend** [OM p.29] — VCO2 square 16'; MIX 0/10; HPF fc 4 pk 2, EG2/EXT 5; LPF fc 5 pk 5, EG2/EXT 5; FM EG1/EXT 7; EG2 0/0/3/5/0. Cable: CONTROL WHEEL OUT → FREQ.
6. **Steam Locomotive** [OM p.30] — VCO1 noise; MIX 10/0; HPF fc 4 pk 2, EG2/EXT 10; LPF fc 4.5 pk 5, EG2/EXT 3; MG triangle, FREQ 4.5; EG2 0/1/1/0/1. Cable: CONTROL WHEEL OUT → CUTOFF FREQ (HPF). Text says "use MG Frequency to adjust the speed", which the printed cable does not explain; for auto-chuffing add MG rectangle OUT → TRIG IN (my addition, UNCONFIRMED).
7. **UFO** [OM p.31] — "starting with MG FREQ at 0, turn it up: the UFO takes off". VCO1 triangle 8'; MIX 5/0; HPF fc 6 pk 10; LPF fc 6 pk 10; FM MG/T.EXT 5; HPF MG/T.EXT 5; LPF MG/T.EXT 5; MG WAVE FORM ≈ 3 o'clock (toward ramp-up), FREQ 0→5 (performed); PORT 10; EG2 10/5/5/5/5. No cables.
8. **Horn/Brass** [OM p.32] — VCO1 saw 16'; MIX 10/0 (raise VCO2 for horn+trumpet ensemble); HPF 0/0; LPF fc 3–4 pk 2, EG2/EXT 10; FM MG/T.EXT 1.5; MG triangle FREQ 4.5; EG2 0/1/4/5/1. No cables.
9. **Violin** [OM p.33] — VCO1 saw 8'; MIX 10/0 (VCO2 up = duet); HPF 0/0; LPF fc 7 pk 5; FM MG/T.EXT 1.5; MG triangle FREQ 4.5; EG2 0/1.5/5/5/1. No cables.
10. **S/H sound** (random pitch) [OM p.34] — VCO1 saw 8'; VCO2 saw 4', PITCH +3; MIX 10/10; HPF fc 5 pk 5; LPF fc 6 pk 5, EG2/EXT 10; FM MG/T.EXT 10; MG WAVE FORM fully CW, FREQ 5; EG2 5/5/10/10/10. Cables: PINK → S/H IN; MG rectangle OUT → S/H CLOCK; S/H OUT → TOTAL.
11. **Guitar Synthesizer I** [OM p.35] — VCO1 saw 16'; VCO2 saw 4', PITCH +3; MIX 10/10; HPF 0 pk 5; LPF fc 5 pk 5, EG2/EXT 8; EG2 0/1/2/5/2; ESP SIG LEVEL 9, LOW CUT 0, HIGH CUT 5.5, CV ADJ 10, THRES 3. Cables: instrument → ESP SIGNAL IN; ESP F∝V CV OUT → S/H IN; EG1 OUT → S/H CLOCK; S/H OUT → VCO 1+2 CV IN; ESP TRIG OUT → TRIG IN (S/H freezes the tracked pitch at each note).
12. **Guitar Synthesizer II** [OM p.36] — VCO1 PW wave, PW ≈ 7, 32'; VCO2 square, PITCH +3, 16'; MIX 10/10; HPF fc 3 pk 5, EG2/EXT 7; LPF fc 4 pk 5, EG2/EXT 8; FM MG/T.EXT 2; MG WAVE FORM ≈ 4–5 o'clock, FREQ 6; EG2 0/0/2/5/0; ESP 9/0/5.5/10/3. Cables: instrument → ESP SIGNAL IN; F∝V CV OUT → VCO 1+2 CV IN; ESP TRIG OUT → TRIG IN; MG rectangle OUT → S/H CLOCK; PINK → S/H IN; S/H OUT → CUTOFF FREQ (HPF) (random HPF steps, depth = HPF EG2/EXT).
13. **Trumpet** [SE p.1] — VCO1 saw 8'; MIX 10/0; HPF 0/0; LPF fc 5 pk 3 (7–9 "for a more penetrating sound, the point right before self-oscillation"), EG2/EXT 10; FM EG1/EXT 1; MG triangle FREQ 3.5; EG1 DELAY 2, ATTACK 2, RELEASE 0; EG2 0/1/2/7/1. Cables: PINK → MVCA IN; MVCA OUT → FREQ (EG1-shaped noise burst on pitch = breath/lip attack).
14. **Clarinet** [SE p.1] — VCO1 rectangle, PW ≈ 2, 16'; MIX 10/0; HPF 0/0; LPF fc 6 pk 4, EG2/EXT 7; FM MG/T.EXT 1; MG triangle FREQ 4; EG1 DELAY 2, ATTACK 2; EG2 0/1/2/5/1. Cables: MG triangle OUT → MVCA IN; MVCA OUT → TOTAL (classic delayed vibrato via EG1-controlled MVCA, the patch the OM describes on p.6).

More patch material: [SE] contains ~30 further charts (saxophone, cello, accordion, electric piano, Hammond, timpani, cow-bell, police siren, laser gun, dive bombing, chime, wind, etc.) plus "Patching Examples" pages; [PB] has 30 modern patches (Punchy Saw Bass, Acid Saw Bass, Hoover Switch Bass, Kick, MS Snare, MS Hi-Hat, Daft Lead, Reso Lead, 5th S&H, Explosion, MS Zap, etc.), drawn from iMS-20 (note: PB diagrams show two extra software-only switches).

---

## 7. Open items to verify on hardware (if access becomes available)
1. MASTER TUNE range (three conflicting Korg values).
2. Portamento maximum time and curve.
3. MG maximum rate (SM says 20 Hz) and rect/tri phase.
4. VCO2 narrow-pulse duty; VCO output levels into the mixer.
5. EG2 HOLD semantics (from trigger vs after gate-off) and EG curve shapes/knob tapers.
6. FM knob depth at 10 (assumed unity, 1 V/oct).
7. Filter knob law between the 50 Hz / 500 Hz / 15 kHz anchors; rev1 vs rev2 drive calibration.
