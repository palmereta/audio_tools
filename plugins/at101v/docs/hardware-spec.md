# Roland SH-101 hardware reference spec (for at101v)

Target: a faithful JUCE VST3/AU emulation of the Roland SH-101 monophonic synthesizer (Nov 1982 to 1986),
including the arpeggiator, the 100-step sequencer and a photorealistic panel.
Compiled 2026-10-16.

## Sources and conventions

Primary sources, cited by short tag throughout:

| Tag | Document | URL |
|---|---|---|
| **[OM pN]** | Roland SH-101 Owner's Manual (1982, EN/JP), page N as printed. Page 52 is a blank full-panel drawing | https://archive.org/details/synthmanual-roland-sh-101-owners-manual |
| **[SN pN]** | Roland SH-101 Service Notes, 1st ed., 1 Nov 1982 (10-page scan; p2 specs/parts, p3 block diagram, p4-5 CPU program, p6 adjustment, p9 circuit diagram) | https://web.archive.org/web/2015/http://fa.utfs.org/diy/rolandsh101/Roland-SH101-Service-Manual.pdf |
| **[SPEC]** | Roland's official spec sheet (same text as OM p56) | https://support.roland.com/hc/en-us/articles/201921519-SH-101-Technical-Specifications (mirror: https://www.sweetwater.com/sweetcare/articles/roland-sh-101-technical-specifications/) |
| **[PHOTO]** | My own measurements on `docs/ref/sh101-grey-topview_CC-BY-SA-3.0_909bd.jpg` (case-cropped top view; 0.2377 mm/px at full resolution, 0.356 mm/px at a 1600 px wide view) | https://commons.wikimedia.org/wiki/File:SH-101.jpg |

Secondary sources:

| Tag | Document | URL |
|---|---|---|
| [ED-3109] | Electric Druid, "Roland filter designs with the IR3109 or AS3109" | https://electricdruid.net/?p=97905 |
| [ED-SUB] | Electric Druid, "A study of sub-oscillators" | https://electricdruid.net/a-study-of-sub-oscillators/ |
| [OTT] | Paul Colbert, review in *One Two Testing*, Dec 1983 (Muzines scan) | https://www.muzines.co.uk/articles/roland-sh-101/4776 |
| [CB] | Circuitbenders SH-101 mods page (stock-behaviour remarks) | https://www.circuitbenders.co.uk/synthmod/SH101.html |
| [WP] | Wikipedia, Roland SH-101 | https://en.wikipedia.org/wiki/Roland_SH-101 |
| [VSE] | Vintage Synth Explorer, Roland SH-101 | https://www.vintagesynth.com/roland/sh-101 |
| [RA] | Roland Articles, SH-101 lead patch recipes (written for the Roland Cloud plug-in) | https://articles.roland.com/?p=70529 |

**UNCONFIRMED** marks a value I could not verify in a primary source. Each one gives a best estimate and the
reason behind it. Behringer MS-101, Roland SH-01A, Roland Cloud SH-101 and TAL-BassLine-101 add features the
original never had (filter envelope, 32' range, swing, MIDI), so they were not used for hardware facts.

---

## 1. Panel layout

### 1.1 Physical size and grid

- **Case:** 570 (W) x 311 (D) x 80 (H) mm, 4.1 kg without batteries [SPEC], [SN p2]. Top-view aspect ratio
  570:311 = **1.833:1**.
- The top surface is a single plane with **five horizontal bands** (y measured from the rear edge) [PHOTO]:

| Band | y (mm) | Content |
|---|---|---|
| A. Jack strip | 0-34 | A slightly raised rear step carrying all jacks. There is no rear-panel jack field: the jacks point up/back from the top strip. "MODULATION GRIP" at the far left; DC IN ... OUTPUT on the right half |
| B. Section header row | 37-48 | One rectangular framed box per section with the section name in bold white caps |
| C. Synth control row | 50-123 | Sliders, rotary knobs and slide switches, each inside its own framed area per section |
| D. Function row | 125-160 | POWER, the 7 function buttons with LEDs, the Roland wordmark and the "SH-101" dot-matrix logo |
| E. Keyboard band | 167-311 | Left performance panel (x 0-89 mm) + 32-key keyboard (x 89-534 mm) + right cheek (534-570 mm) |

- **Slider travel: about 30 mm.** UNCONFIRMED. The 0-to-10 scale spans 31 mm on [PHOTO]. The service parts list
  calls the sliders "S3018P405-100KA/-100KB/-1MA" [SN p2], which looks like a 30 mm-travel series.
- Slider pitch inside a section is about **15 mm** [PHOTO]. Section dividers add about 5-8 mm.

### 1.2 Band A: jack strip (left to right) [PHOTO], [SPEC]

| x (mm) | Silkscreen | Type and electrical spec |
|---|---|---|
| 34, 46 | MODULATION GRIP | Two small jacks for the MGS-1/MG-1 grip (bender + LFO MOD button) [SPEC], [OM p40] |
| 373 | DC IN 9V~12V (with polarity symbol) | DC barrel, PSA-120/220/240 adaptor [SPEC] |
| 396 | EXT CLK IN | 1/4"; +2.5 V or more = clock high [SPEC] |
| 417 | HOLD | DP-2 pedal switch [SPEC] |
| 440 / 457 | CV / GATE, bracketed "IN" | CV in 1 V/oct, 0-7 V; Gate in +2.5 V or more [SPEC]. Plugging into these breaks the internal CV/gate [OM p37] |
| 477 / 496 | CV / GATE, bracketed "OUT" | CV out 1 V/oct, 0.415-5 V; Gate out 0 V off / 10 V on into 100 kΩ [SPEC] (service notes say 12 V [SN p2]) |
| 519 | PHONES | 1/4", 8 Ω, stereo [SPEC] |
| 540 | OUTPUT | 1/4", 0 dBm max [SPEC] |

The four CV/GATE jacks are 3.5 mm minijacks (small holes on [PHOTO]). The manual tells you to use the PCS-4
mini-to-1/4" cable for gear with 1/4" jacks, including for patching GATE OUT into EXT CLK IN [OM p37-38]. EXT CLK
IN, HOLD, PHONES and OUTPUT are visibly larger 1/4" jacks [PHOTO].

### 1.3 Band B/C: synth sections (left to right)

Exact silkscreen labels come from the panel drawing [OM p52] and were checked on [PHOTO]. x = control centre
from the left case edge in mm [PHOTO]. All sliders are vertical, 0 at the bottom and 10 at the top. Each has a
tick ladder on both sides with 11 lines (0...10) and numerals **0, 5, 10** on both sides.

| Section (header text) | x span (mm) | Control (silkscreen) | x (mm) | Type | Positions / scale |
|---|---|---|---|---|---|
| (no header; framed box) | 12-60 | **TUNE** | 41 | Rotary knob, centre detent region, arc legend with **♭** (left) and **♯** (right) | ±50 cent [SPEC] |
| **MODULATOR** | 60-119 | **LFO/CLK RATE** (two lines) + small **red rectangular LED** to the right of the label | 72 (LED 83) | Slider | 0-10; LED flashes at the LFO/clock rate [SPEC] "Rate Indicator" |
| | | **WAVE FORM** | 100 | 4-position rotary selector | Triangle symbol / square symbol / **RANDOM** / **NOISE**, clockwise [OM p52], [SPEC] |
| **VCO** | 119-223 | **MOD** | 132 | Slider | 0-10 (LFO to VCO depth) |
| | | **RANGE** | 160 | 4-position rotary selector | **16' 8' 4' 2'**, clockwise [OM p52] |
| | | **PULSE WIDTH** + pulse symbol | 188 | Slider | 0-10 |
| | | (PWM source switch, unlabelled; legends at its right) | 207 | 3-position vertical slide switch in a slider-size well | top **LFO** (with triangle symbol), middle **MAN**, bottom **ENV** (with envelope symbol). A bracket under the PW slider and the switch shows they belong together [OM p52] |
| **SOURCE MIXER** | 223-325 | Pulse symbol | 235 | Slider | 0-10 |
| | | Saw symbol (rising ramp, "⊿") | 250 | Slider | 0-10 |
| | | **SUB OSC** | 265 | Slider (level) | 0-10 |
| | | (sub waveform switch; legends at its right) | 285 | 3-position vertical slide switch | top **1OCT DOWN** + square symbol; middle **2OCT DOWN** + square symbol; bottom **2OCT DOWN** + narrow-pulse symbol [OM p52], [SPEC]. Bracket under level + switch |
| | | **NOISE** | 311 | Slider | 0-10 |
| **VCF** | 325-419 | **FREQ** | 337 | Slider | 0-10 |
| | | **RES** | 352 | Slider | 0-10 |
| | | **ENV** | 374 | Slider | 0-10 |
| | | **MOD** | 389 | Slider | 0-10 |
| | | **KYBD** | 404 | Slider | 0-10 |
| **VCA** | 419-445 | (switch; legends at its left) | 435 | 2-position vertical slide switch | top **ENV** (envelope symbol), bottom **GATE** (gate symbol) |
| **ENV** | 445-554 | (trigger switch; legends at its left) | 462 | 3-position vertical slide switch | top **GATE+TRIG** (written "GATE / + / TRIG"), middle **GATE**, bottom **LFO** |
| | | **A** | 481 | Slider | 0-10 |
| | | **D** | 496 | Slider | 0-10 |
| | | **S** | 511 | Slider | 0-10 |
| | | **R** | 527 | Slider | 0-10 |

Row geometry [PHOTO]: slider well tops at y≈52 mm, scale line "10" at y≈72 mm, "0" at y≈103 mm, wells end at
y≈118 mm. Rotary knob centres at y≈87 mm (TUNE knob cap about 20 mm; WAVE FORM/RANGE about 18 mm, UNCONFIRMED from
[PHOTO]). The slide switches sit in slider-sized wells with the same 3-detent spacing (~10 mm between detents,
UNCONFIRMED).

### 1.4 Band D: function row [OM p33], [OM p52], [PHOTO]

| Control | x (mm) | Notes |
|---|---|---|
| **POWER** | 40 | Black push button (larger, ~16x9 mm) with a red LED above it at y≈129 mm. The LED doubles as the battery-check light. It flashes at the LFO/CLK rate when the batteries are low [OM p41], [SN p5] |
| **LOAD**, **PLAY** | 105, 121 | Grouped under the bracket label **SEQUENCER** |
| **DOWN**, **U & D**, **UP** | 144, 160, 176 | Grouped under **ARPEGGIO**. Order left to right is DOWN, U&D, UP [OM p32] |
| **HOLD** | 198 | Second legend below: **LEGATO** |
| **KEY TRANSPOSE** (2 lines) | 220 | Second legend below: **REST** |

- Buttons: light-grey/cream rectangular keycaps about 12.5 x 6.5 mm, label under each button, group labels on a
  bracket line below. Each function button has a **small red rectangular LED** about 9 mm above it [PHOTO]. The
  LEDs are GL-9PR2 parts [SN p2].
- A long bracket line runs under all seven function buttons. Under it sit the second legends LEGATO (under HOLD)
  and REST (under KEY TRANSPOSE). In LOAD mode these two buttons enter a tie and a rest [OM p33], [SN p5].
- **Logos:** the "Roland" wordmark with the R-logo, white, x≈386-447 mm, cap height about 9 mm. "**SH-101**" in a
  white **dot-matrix (grid of small squares)** typeface, x≈452-538 mm, y≈139-159 mm, about 20 mm tall [PHOTO],
  [ref/sh101-logo-env-closeup]. Both are right-aligned above the right half of the keyboard.

### 1.5 Band E, left: performance panel (x 0-89 mm, y 167-311) [OM p31], [PHOTO]

| Control (silkscreen) | Position (x, y mm) | Type | Notes |
|---|---|---|---|
| **VOLUME** | 22, 192 | Rotary knob, scale 0-5-10 with 11 ticks | |
| **PORTAMENTO** | 68, 192 | Rotary knob, scale 0-5-10 | Time 0-5 s [SPEC] |
| **VCO** (bender sens) | 14, 217-262 | Short slider, scale 0-5-10 on the left | Grouped by bracket "**BENDER**" with VCF |
| **VCF** (bender sens) | 29, 217-262 | Short slider | |
| **LFO∿ / MOD** | 43, 205-262 | Slider with scale on the right. A line is drawn from it down to the BENDER lever (push-forward function) | |
| (portamento mode) | 68, 227 | Horizontal 3-position mini slide switch in an oval slot, chrome lever | Legends above: **AUTO OFF ON**; label below: **PORTAMENTO**. A bracket connects it to the PORTAMENTO knob |
| (transpose) | 68, 251 | Horizontal 3-position mini slide switch | Legends **L M H**; label **TRANSPOSE** |
| **BENDER** lever | slot x 22-65, y 285-298 | Horizontal spring-centred lever, black, in a rectangular slot. Label "BENDER" with a double-headed arrow below | Left = pitch/filter down, right = up, push away from the player = LFO MOD (triangle vibrato/growl) [OTT], [OM p31] |

### 1.6 Keyboard

- **32 keys, F to C** ("32 key, F-scale") [SPEC], [OTT]: 19 white and 13 black keys, full-size [OTT]. White key
  pitch 23.4 mm [PHOTO] (445 mm / 19). The visible white key length from under the panel lip is about 142 mm and
  the visible black key length about 87 mm [PHOTO]. The keybed is a Roland SK-331-AR [SN p2].
- With RANGE 8' and TRANSPOSE M, the **lowest C (the 8th key) is middle C**, so the keyboard spans **F3-C6** [OM p24],
  [OM p30]. Concert tuning in the factory procedure is **A4 = 442 Hz** [SN p6]. Use 440 Hz as the default and offer
  442 Hz as an option.

### 1.7 Underside / accessories (for completeness)

Battery compartment for 6 x UM-2 (C) cells. 3 cells in the rear slots keep only the sequencer memory alive.
Battery life is about 10 h [OM p41]. Optional MGS-1 grip with strap button [SPEC].

---

## 2. Behaviour and ranges

### 2.1 Pitch path, CV and transposition

- The CPU (**NEC/Toshiba 80C49**, "TMP80C49P-6-7301") scans the keys and the switches and writes a 1 V/oct key CV
  through a D/A converter [SN p4], [SN p9]. It runs one scan loop every **1.5-3.5 ms** [SN p4]. This is the
  real timing granularity of note changes, arpeggio and sequencer steps.
- The **RANGE** selector is read by the CPU and added as a DC offset: **16' = 1 V, 8' = 2 V, 4' = 3 V, 2' = 4 V**
  [SN p4]. RANGE changes are therefore quantized to whole octaves and applied as part of the CV. Portamento then
  glides them as well, because the portamento lag sits after the DAC [SN p3].
- **TRANSPOSE L/M/H** (performance panel) shifts the keyboard by -1/0/+1 octave. The lowest F gives 0.417 V (L),
  1.417 V (M), 2.417 V (H), and the highest C at H gives 5.0 V [SN p5]. This shift **also moves KYBD
  filter tracking** [OM p31]. TRANSPOSE **does not work while the sequencer is playing**, but it can be used
  while loading, which gives a 56-key loading range [OM p31], [OM p34].
- **KEY TRANSPOSE** (button): hold it and press a key. Keys below the lowest C transpose down (lowest F = a 5th
  down). Keys above it transpose up by the interval from C (any G = a 5th up; the two upper C keys = +1 octave).
  Hold KEY TRANSPOSE and press any C except the highest to return to normal; the LED goes out [OM p36]. So the
  range is -7..+24 semitones. It **does not move the VCF key follow** [OM p36]. It applies to held (HOLD) notes,
  to held arpeggios and to the running sequence [OM p36]. The CPU adds the key-shift to the range data [SN p4].
- The **TUNE** knob gives ±50 cents [SPEC].
- CV IN drives the synth module directly. Key Transpose, Portamento and Bender still work on it [OM p37].

### 2.2 VCO (Curtis CEM3340) [SN p3], [SN p9]

- **Core:** CEM3340 (IC13). Saw and pulse come from the same core.
- **Footage / frequency:** at 8' and M the keyboard covers F3 (174.6 Hz) to C6 (1046.5 Hz). Each footage step
  is one octave (16' = F2-C5 ... 2' = F5-C8). With 16', L and KEY TRANSPOSE -7 the lowest note is A#0 (29 Hz).
  The top end in theory goes past C9 with 2', H and +24 st, but the DAC/CV path tops out at about 5 V of key CV
  [SN p4] (UNCONFIRMED how the hardware clips beyond that). Clamp the emulation's VCO at 20 kHz.
- **Saw:** a **rising ramp** (the panel symbol is "⊿", i.e. it ramps up and drops sharply) [OM p52], [OM p25]. The
  CEM3340 ramp is natively rising. A 0-10 V ramp feeds the sub-oscillator circuit [ED-SUB].
- **Pulse:** PW slider in MAN = **50 % down to "min."** [SPEC] ("50 % ~ 0 %" [SN p2]). The slider at 0 gives a
  square, at 10 the narrowest pulse, which can thin out to silence. In **LFO** or **ENV** mode the same slider
  sets the modulation **depth** around the 50 % point [OM p24].
  - PWM-by-LFO **always uses the LFO triangle**, whatever the WAVE FORM selector says [CB].
  - Modulation narrows the pulse from 50 %. Deep settings drive the width to 0 %, so the pulse drops out
    periodically. This is a known quirk that users report as a "square tremolo" [CB]. UNCONFIRMED exact mapping.
    Best estimate: `pw = 0.5 - 0.5 * depth * m`, where m ∈ [0,1] is the unipolar LFO triangle or the ENV output,
    clamped to [0, 0.5]. For MAN mode, `pw = 0.5 - 0.47 * slider` (minimum ≈ 3 %, UNCONFIRMED).
- **MOD** slider: depth of the MODULATOR signal (the selected waveform) applied to pitch. UNCONFIRMED maximum depth;
  estimate **±1 octave at 10** for triangle/square, with a roughly exponential slider law (log taper).
- **Drift:** described as having "very stable tuning in operation". Unserviced units drift in octave scaling, VCF
  tracking and self-oscillation pitch [CB]. Recommend a ≤ ±2 cent slow random drift and a "tracking error" option
  of ±5 cents per 4 octaves (UNCONFIRMED values).

### 2.3 Sub-oscillator (CD4013) [SN p9 IC17], [ED-SUB]

- Two chained D flip-flops (CD4013B, +14 V) clocked from the VCO, so the sub is **phase-locked to the saw reset**
  [ED-SUB].
- Positions [SPEC], [OM p52], [ED-SUB]:
  1. **1OCT DOWN**: square, f/2 (first flip-flop).
  2. **2OCT DOWN**: square, f/4 (second flip-flop).
  3. **2OCT DOWN pulse**: diode-OR of both flip-flops. It is high 75 % / low 25 % of the period, the "25 % pulse"
     (equivalent up to inversion) [ED-SUB]. It has a strong 2nd harmonic, so it still sounds partly an octave up.
- The sub is **not** affected by PWM.

### 2.4 Noise

White noise from a selected **2SC945** transistor junction (TR23, "Selected for Noise") [SN p9]. The NOISE slider
mixes it in. The MODULATOR NOISE position uses the same generator as a modulation source [OM p30].

### 2.5 Source mixer

Four independent level sliders (pulse, saw, sub, noise), 0-10, summed before the VCF [OM p26]. Taper:
UNCONFIRMED. The service parts list mixes 100KA (audio/log) and 100KB (linear) sliders [SN p2]. Assume **audio taper
(A) for mixer levels**. All four at 10 overdrive the IR3109 input noticeably. Model this with the filter's input
tanh (UNCONFIRMED; it is the usual SH-101 "fat" behaviour).

### 2.6 MODULATOR (LFO / clock)

- **Rate:** LFO/CLK RATE **0.1 Hz to 30 Hz** [SPEC]. Use an exponential slider law (log taper):
  `f = 0.1 * 300^(x)`, x ∈ [0,1] (UNCONFIRMED law, endpoints confirmed).
- **Circuit:** a classic integrator (IC3a, C9 = 0.1 µF) + comparator (IC3b) triangle/square core. TR4 discharges
  the integrator on the CPU's **CLK RST** line [SN p9 "LFO"]. The square output is the clock for the arpeggiator,
  the sequencer and the ENV-LFO trigger mode [SN p5].
- **Waveforms** (WAVE FORM selector) [OM p30], [SPEC]:
  - **Triangle**: vibrato / growl.
  - **Square**: trill. Polarity UNCONFIRMED. Model it bipolar (±), the same amplitude as the triangle.
  - **RANDOM**: **sample & hold generated by the CPU**. On every rising clock edge the CPU makes a new random value
    and writes it through the DAC [SN p5 steps 6-7]. So the random steps are synchronised to the LFO **or** to the
    EXT CLK pulses. This is the "connect GATE OUT to EXT CLK IN + RANDOM" trick for per-note random filter
    [OM p38].
  - **NOISE**: the noise generator used as a fast, chaotic modulation source.
- **Reset:** the LFO restarts "from its head" on a new key-on when the ENV switch is at **LFO**, or when the
  **arpeggio** is active (CLK RST) [OM p29], [SN p5].
- **EXT CLK IN** plugged: the internal clock is disconnected from the arpeggio/sequencer. The RATE knob then
  controls only the LFO [OM p38].
- The **rate LED** blinks with the LFO square. It does not follow EXT CLK (UNCONFIRMED, best guess from [SN p5]
  "LED Illumination ... does not illuminate the LEDs for the LFO and power").
- The bender **LFO MOD** (push) always uses the **triangle** [OM p30], [OM p31].

### 2.7 VCF (IR3109) [SN p9], [ED-3109]

- **Topology:** Roland IR3109 = four OTA + buffer cells (essentially four BA662 cells), wired as a **4-pole
  24 dB/oct low-pass**. Each stage uses a **240 pF** capacitor (C47, C48, C50, C51) with **68 kΩ / 560 Ω** input
  dividers (R114-R124) [SN p9]. These are the same values as the Juno-6 filter [ED-3109].
- **Cutoff range:** **10 Hz to 20 kHz** [SPEC]. The exponential converter is inside the IR3109 (pin 9) with an
  SDT1000 thermistor (TH1) for temperature compensation [SN p9]. Cutoff CV sum = FREQ + ENV·env + MOD·lfo + KYBD·key
  + VCF-bend.
- **Resonance:** 0 to self-oscillation [SPEC]. The resonance at full gives a sine-like self-oscillation at the
  cutoff [OM p26]. The feedback goes through a transistor phase splitter (TR26/TR27: inverted feedback +
  non-inverted output) with **anti-parallel clipping diodes (D30/D31) in the feedback path**. These limit the
  oscillation amplitude and add mild, filtered distortion [ED-3109], [SN p9].
- **Resonance compensation:** the SH-101 compensates the passband volume loss at high resonance by
  **raising the output (VCA drive) level**, not the filter input. Electric Druid calls this unusual [ED-3109]. So at
  high RES the bass loss is partly made up, but the input drive into the stages is **not** boosted. The filter
  thins out less than a Moog ladder, and the resonant peak gets louder at the output.
- **KYBD:** 0-100 % key follow [SPEC]. At 10, the filter tracks 1 V/oct. The manual recommends maximum
  for normal playing [OM p27]. Tracking was factory-trimmed with the VCF WIDTH trimmer (VR8), so that F4-F5 gives
  exactly 2:1 in self-oscillation [SN p6].
- **ENV:** depth of the ADSR to cutoff, unipolar positive (there is no inverted envelope on the panel).
- **MOD:** depth of the MODULATOR waveform to cutoff.
- **Self-oscillation level:** UNCONFIRMED. Estimate ≈ 0 dBFS-equivalent, about the same level as a full-scale
  saw at the output. The diodes clamp the feedback at about ±0.6 V relative to a ~1-2 V signal swing.

### 2.8 VCA (BA662A) [SN p9]

- VCA switch **ENV**: VCA gain follows the ADSR. **GATE**: gain is a rectangular on/off from the gate (organ-type).
  The ADSR then only drives the VCF [OM p27].
- In GATE mode there is a short RC smoothing on the gate, so expect a slight click at fast notes. UNCONFIRMED
  time constant; use a 1-2 ms one-pole slew to stay safe.
- The output stage drives the VOLUME knob, then OUTPUT and PHONES.

### 2.9 ENV (ADSR, discrete) [SN p9 "ENV GEN"], [SPEC]

| Stage | Range [SPEC] | Circuit hints [SN p9] |
|---|---|---|
| A | **1.5 ms - 4 s** | C59 = 4.7 µF timing capacitor, buffered by IC16b (M5218); attack series resistor R141 = 270 Ω + A slider |
| D | **2 ms - 10 s** | R148 = 220 Ω + D slider |
| S | **0-100 %** | Level |
| R | **2 ms - 10 s** | R164 = 150 Ω + R slider |

- τ = R·C. The minimum τ values: attack 270 Ω x 4.7 µF ≈ 1.3 ms, decay ≈ 1.0 ms, release ≈ 0.7 ms. This is
  consistent with the spec minima and with the envelope's reputation as "snappy" and "crisp" [OTT].
- Slider pots: 1 MΩ audio (S3018P405-1MA in the parts list [SN p2]). 1 MΩ x 4.7 µF = 4.7 s, which matches the 4 s
  attack. 10 s D/R correspond to about 2.2τ (to -19 dB). Likely mapping: `R = Rmin + 1 MΩ * taperA(x)`.
- **Curves:** RC exponential in all stages. The attack charges toward a target above the peak, and a comparator
  (IC16a) switches to decay at the peak. So the attack is a **truncated, concave-down exponential** (fast start,
  slower approach). Estimate: target = 1.5 x peak (UNCONFIRMED; it is the usual Roland discrete-ADSR ratio).
  Decay and release are true exponentials toward S and toward 0.
- **All ADSR at 0** gives just a short click, which the manual uses as an organ "key click" [OM p29], [OM p50].
- **Trigger switch** [OM p29]:
  - **GATE+TRIG**: every new key restarts the envelope, even when played legato. Note priority = **last note**.
  - **GATE**: legato notes do **not** retrigger the envelope. Note priority = **lowest note**.
  - **LFO**: while a key is held, the envelope retriggers on **each LFO cycle**, and the LFO is reset to its start
    at key-on. Stops on key release. Note priority = **lowest note**.
- **Retrigger behaviour:** a retrigger restarts the attack from the current level (no reset to 0), the usual
  analog behaviour (UNCONFIRMED but standard for this topology).

### 2.10 Note priority, legato, portamento

- Note priority depends on the ENV trigger switch: **last-note in GATE+TRIG, low-note in GATE and LFO** [OM p29].
- **PORTAMENTO** time **0-5 s** [SPEC]. It is an analog lag after the DAC [SN p3 "PORTAMENTO" block]. So the glide is
  an **exponential (constant time-constant) approach**: wide intervals glide faster in Hz/s but take about the
  same time. Model: one-pole on the CV in volts/octave, τ = 0..~1.7 s (5 s ≈ 3τ, UNCONFIRMED).
- **Mode switch AUTO / OFF / ON** [OM p31]:
  - **OFF**: never.
  - **ON**: always.
  - **AUTO**: glide only when playing **legato**, i.e. the new key is pressed while the previous key is still held.
    The English manual wording ("releasing the key before pressing") is a translation error. [OTT] confirms
    "active only when two keys are held together".
- With **arpeggio**, portamento works only in **ON**. AUTO acts as OFF [OM p32].
- **Bender and portamento work during sequencer playback** [OM p35].

### 2.11 Bender and modulation [OM p31], [OM p40], [OTT]

- **Lever:** spring-centred. **Left = down, right = up** [OTT].
  - **VCO** slider: pitch bend depth, **max ±1200 cents (±1 octave)** at 10 [OM p31]. Assume a linear slider law.
  - **VCF** slider: cutoff bend depth. Max UNCONFIRMED; estimate ±2 octaves at 10.
  - **Push forward** (away from the player): turns on **LFO MOD**. The **triangle** LFO modulates **both VCO
    (vibrato) and VCF (growl)**, with depth set by the **LFO∿ MOD** slider [OM p31]. The VCO-vs-VCF split is fixed.
    UNCONFIRMED ratio; estimate VCO ±0.5 semitone and VCF ±1 octave at 10.
- **Modulation grip MG-1/MGS-1** (optional, plugs into the MODULATION GRIP jacks): a bender that bends **up only**
  (pitch up, filter brighter), plus an **LFO MOD button** that does the same as pushing the lever [OM p40], [OTT].
  There is no MOD button on the main panel. Implement "MOD" as a momentary/toggle button in the plugin to stand in
  for the grip's button.

### 2.12 HOLD [OM p36], [OM p39]

- The HOLD button (or the DP-2 pedal on the HOLD jack, momentary) holds the gate after the key is released. The
  sustained level is the S level, so with S = 0 HOLD does nothing audible.
- A new key while in HOLD replaces the held note (mono).

---

## 3. Arpeggiator and sequencer

### 3.1 Clock

- The clock is the **LFO square**: one arpeggio note or sequencer step per LFO cycle, on the rising edge [OM p32],
  [OM p33], [SN p5]. Tempo range is therefore **0.1-30 steps/s**. As 16th notes that is **1.5-450 BPM**.
- **EXT CLK IN:** one note per pulse, +2.5 V or more threshold. Built for the TR-606/808 TRIGGER OUT, CR-8000/5000,
  DR-55, TB-303, CSQ-600, MC-4 [OM p38].
- On key-on with the arpeggio active, the CPU resets the LFO (CLK RST), so the **first arpeggio note fires
  immediately** and the grid restarts at the key press [SN p5].
- Timing jitter: the CPU polls the clock in its 1.5-3.5 ms loop [SN p4]. A faithful mode can add 0-3.5 ms of
  latency/jitter to step onsets (optional).

### 3.2 Arpeggio [OM p32], [SN p5]

- Press **UP**, **U&D** or **DOWN** (LED lights; the three are mutually exclusive). Press the lit one again to
  cancel.
- Notes are sorted **by pitch** (CPU "32-key arpeggio key buffer", a bitmap of the 32 keys [SN p5]). Arpeggios
  work only over the held keys, with **no octave-range option**.
- **U&D** with C-E-G held plays **C E G E C E G E...** The top and bottom notes are **not** repeated at the turns
  [OM p32 timing diagram "Do Mi Sol Mi"].
- The arpeggio runs only while a chord is held, unless HOLD is on. **A single key played non-legato plays
  normally**, which lets you switch between arpeggio and normal playing by touch [OM p32]. Chord keys that are not
  pressed together can make the first cycle "imperfect" [OM p32].
- **Gate behaviour:** during an arpeggio the **GATE stays high continuously**. Note articulation comes from the
  clock trigger [OM p32 diagram]:
  - ENV **GATE+TRIG**: the envelope retriggers on every note (normal arp).
  - ENV **GATE**: a single envelope for the whole chord. With S = 0 the sound dies after the first decay.
  - ENV **LFO**: the envelope retriggers on each clock (same rate). The manual suggests longer A and D here.
- **Arpeggio + HOLD:** pressing HOLD while the arpeggio plays keeps it running after you let go. A new chord
  replaces the pattern [OM p32]. **Arpeggio + HOLD + KEY TRANSPOSE:** the held arpeggio can be transposed [OM p36].
- **Arpeggio + portamento:** only in ON [OM p32].
- **CV/GATE OUT** carries the arpeggio [OM p37].

### 3.3 Sequencer [OM p33-36], [SN p5]

- **Capacity: 100 steps**, one step = the shortest note value. Longer notes use several steps (ties). Rests use
  steps [OM p33-34]. Only pitch is stored, with no velocity or accent. Loading is only from the SH-101's own
  keyboard [OM p33].
- **Memory** is kept by the batteries. With the adaptor connected, keep cells installed for backup [OM p41].
- **Loading:** press **LOAD** (LED on), then play notes. Timing is ignored, each key = one step.
  - **REST** = the KEY TRANSPOSE button: each press stores one rest step.
  - **LEGATO** = the HOLD button: **hold it while pressing the next key** to store a slur/tie to that note. A tie on
    the same pitch extends the note [OM p34].
  - Press LOAD again to finish. When **100 steps** are filled, loading stops automatically and the unit returns to
    normal playing [OM p34].
  - Starting a new load **from the beginning erases the old sequence** at once [OM p35].
  - **Partial re-record:** run PLAY, and press LOAD while the **last good note** is sounding. Loading continues from
    there. If the new data is shorter than the old, the old tail stays and is still played [OM p35].
  - The TRANSPOSE switch (L/M/H) works while loading, so you can enter a 56-key range [OM p34].
- **Playback:** press **PLAY** (LED). The sequence loops until PLAY is pressed again [OM p34]. It is free-running;
  you do not need to hold a key (forum reports agree, but this was not tested on hardware).
  - Steps play **non-legato** (gate drops between steps) except tied/slurred steps. Rests drop the gate for that
    step [OM p34-35 diagram]. Gate length of a normal step = the **high part of the clock** (≈50 % of the step, from
    the LFO square duty). UNCONFIRMED exact duty; the diagram shows short gaps. Estimate 50 %.
  - For **slurs to sound legato** (no envelope restart) set the ENV switch to **GATE** [OM p34].
  - ENV **LFO** with a sequence of only shortest notes and no slurs gives a slightly different articulation
    (envelope retriggers on the clock) [OM p35].
  - **Transpose during play:** hold KEY TRANSPOSE and press a key [OM p36]. The TRANSPOSE switch is ignored during
    play [OM p31]. Bender and portamento work [OM p35].
  - With the DP-2 pedal, pressing and releasing the pedal stops the sequence [OM p34].
- **LOAD and PLAY** are exclusive modes. Arpeggio and sequencer PLAY should be treated as exclusive. UNCONFIRMED
  what the hardware does if both are pressed; best guess is that the last pressed wins.
- **Test mode** (power on with LOAD + KEY TRANSPOSE held) outputs fixed CVs for calibration [SN p4]. Not needed for
  the emulation.

---

## 4. Recommended DSP models

### 4.1 Oscillator

- Use a **phase accumulator saw** (rising, -1..+1) with **PolyBLEP** or BLAMP-free minBLEP for the reset. The
  CEM3340 reset is fast (sub-µs), so treat it as ideal.
- **Pulse** = comparator of the same ramp against a threshold from PW, with BLEP on both edges. Apply PWM
  per sample (smooth the LFO/ENV control at about 1 kHz to avoid zipper noise).
- **Sub** = toggle flip-flops on each saw reset:
  `q1 ^= 1 on reset; q2 ^= 1 on q1 rising; sub = mode==0 ? q1 : mode==1 ? q2 : (q1 | q2)`. Each step is a
  discontinuity at the saw reset time, so it can reuse the same BLEP offset. Output levels are ±1 like the others.
- **Mixer:** sum the levels × waveform (pulse and saw roughly equal peak level; sub about the same; noise about
  -6 dB relative, UNCONFIRMED), then send it into the filter input stage with drive.
- Add 2-4 x oversampling (or rely on BLEP) to the oscillator + filter path. The filter's tanh stages generate
  harmonics.

### 4.2 IR3109 VCF

Recommended structure (per sample, 2x oversampled):

```
x  = drive * mix_in                     // drive ~1..3 from mixer sum; tanh at input
u  = x - k * clipD(y4)                  // inverted global feedback, diode clip in the loop
s1 = TPT1pole(tanh(u)  - tanh(y1)) ...  // 4 cascaded OTA integrators (Zavalishin TPT / ZDF)
y4 = stage4
out = y4 * (1 + c_res * k)              // SH-101 compensates at the OUTPUT (VCA drive)
```

- **OTA nonlinearity:** each stage is `y' = g·(tanh(in) - tanh(y))`. Use the cheap tanh, or a full ZDF solve with
  Newton iterations, or an "instantaneous linearisation" (per Zavalishin / Huovilainen-style OTA cascades).
- **Feedback:** k ∈ [0, ~4.0-4.4]. Self-oscillation starts at k = 4 for the linear model. Map the RES slider so
  that RES ≈ 9 reaches oscillation (UNCONFIRMED point; the manual says it starts "at its highest level"
  [OM p26]). Use `clipD(v) = tanh(v / Vd) * Vd` (anti-parallel diode pair, D30/D31), Vd tuned so that the
  self-oscillation peak is about equal to a full-scale saw.
- **Passband compensation** at the output: c_res ≈ 0.5 restores roughly half the level loss at full resonance
  (UNCONFIRMED; tune by ear against recordings). Do **not** add input compensation like the Juno-6 does
  [ED-3109].
- **Cutoff mapping:** FREQ slider (exponential) 10 Hz-20 kHz [SPEC] ≈ 11 octaves. Then add ENV (≈ +10 octaves max,
  UNCONFIRMED), MOD (≈ ±3 oct max, UNCONFIRMED), KYBD (0-1 oct/oct), VCF-bend. Clamp below Nyquist (prewarp).
- **Cutoff jitter/drift:** a tiny amount of noise on the cutoff CV (≈0.5 cent RMS) adds life (optional).
- Do **not** model it as a diode ladder (TB-303) or a Moog ladder. The IR3109 is a cascade of identical buffered
  OTA poles, cleaner and with less passband loss.

### 4.3 VCA

BA662 OTA: a gentle tanh at high input level, otherwise linear. In GATE mode use a 1-2 ms slew on the gate
(UNCONFIRMED). The output gain stage includes the resonance-compensation term above.

### 4.4 Envelope

Analog RC model per stage with a sample-accurate state:

```
attack:  v += (Vt - v) * (1 - exp(-1/(tauA*fs)));  if v >= 1 -> decay   // Vt ≈ 1.5 (UNCONFIRMED)
decay:   v += (S - v)  * (1 - exp(-1/(tauD*fs)))
release: v += (0 - v)  * (1 - exp(-1/(tauR*fs)))
tauA = (270 + 1e6*taperA(a)) * 4.7e-6      // 1.3 ms .. 4.7 s
tauD = (220 + 1e6*taperA(d)) * 4.7e-6      // decay to S;   spec 2 ms .. 10 s (~2.2 tau)
tauR = (150 + 1e6*taperA(r)) * 4.7e-6
```

- taperA(x) is an audio (log) pot law, e.g. `(pow(10, 2x) - 1) / 99` (UNCONFIRMED shape). Check the 4 s / 10 s
  maxima against the spec.
- Retrigger: keep v and go back to attack (no reset).
- ENV-LFO mode: retrigger on each LFO rising edge while the gate is high, and reset the LFO phase at key-on.

### 4.5 LFO, random and noise

- Triangle/square from one phase accumulator. Reset the phase to the "start" on CLK RST: key-on in ENV-LFO mode
  or arpeggio mode.
- RANDOM: a new uniform value on each clock rising edge (internal LFO square **or** EXT CLK), held (stepped, no
  slew), output through the "DAC" (quantize to 8 bits for authenticity, optional).
- Noise: white Gaussian noise with a gentle 1-pole HPF at about 20 Hz and an LPF at about 15 kHz to mimic the
  transistor + coupling (UNCONFIRMED colour; the hardware noise is close to white).

### 4.6 CPU/key timing

Model the key scanner as a 2 ms (configurable 1.5-3.5 ms) control-rate tick for: key changes, arpeggio steps,
sequencer steps, KEY TRANSPOSE recognition and the random S&H update. Offer a "tight" mode with sample-accurate
timing.

### 4.7 Known measurements summary

| Item | Value | Source |
|---|---|---|
| Cutoff range | 10 Hz - 20 kHz | [SPEC] |
| Resonance | 0 to self-oscillation (sine-like at cutoff) | [SPEC], [OM p26] |
| Self-oscillation amplitude | Limited by D30/D31 diode clipping; UNCONFIRMED level | [SN p9], [ED-3109] |
| ENV A / D / R | 1.5 ms-4 s / 2 ms-10 s / 2 ms-10 s | [SPEC] |
| LFO | 0.1-30 Hz | [SPEC] |
| Portamento | 0-5 s | [SPEC] |
| Pitch bend | max ±1200 cents | [OM p31] |
| Tune | ±50 cents | [SPEC] |
| Calibration | A4 = 442 Hz at 8'/M; VCF tracking trimmed to 2:1 between F4 and F5 | [SN p6] |
| CPU loop | 1.5-3.5 ms | [SN p4] |
| Drift | Stable when serviced; old units drift in scaling | [CB] |

---

## 5. Visual design

### 5.1 Colours

| Element | Description | Approx. sRGB (from photos, daylight; adjust to taste) |
|---|---|---|
| **Body + top panel, grey version** (standard) | One single-colour moulded plastic case. The panel is **the same grey as the case**; there is no separate dark faceplate [PHOTO], [ref/grey-angled]. [OTT] calls it "lightweight grey plastic" | Mid warm grey **#7E7A70 - #888680** (sampled; studio photos read lighter, ~#8A8A86) |
| **Blue** limited edition | Medium-saturated royal/cobalt blue case and panel, same white lettering | about **#1F5FA8** top, **#0A4259** shaded side (sampled, stage light) |
| **Red** limited edition | Bright signal red case and panel, white lettering | No free-license photo found. Estimate **#C8282A** (UNCONFIRMED) |
| White | Reported but considered a myth or a fake [VSE] | do not implement, or mark it non-original |
| **Lettering** | White / off-white silkscreen for all labels, scales, section names and logos | **#F2F0EA** |
| **Section frames** | Thin light lines/embossed edges framing each section header and control area; the header row is a separate box per section | lighter than the body, ~#A8A49A, 1 px highlight + 1 px shadow |
| **Slider wells** | Recessed dark slots, with a raised rectangular frame around each slider/switch | slot ~#211D22 |
| **Slider caps** | Black, rectangular, with a moulded step/ridge and a **coloured indicator line across the front face** | cap #141414. **Green line**: LFO/CLK RATE and all four SOURCE MIXER levels. **Orange line**: VCO MOD, PULSE WIDTH, all VCF, all ENV ADSR, the three BENDER-section sliders. **Yellow line**: the four slide switches (PWM source, SUB OSC waveform, VCA, ENV trigger) [PHOTO], [ref/logo-closeup] |
| Cap colour part numbers | Slide knobs 016H057 **yellow**, 016H059 **green**, 016H060 **orange**; rotary knobs 016H071 | [SN p2] |
| **Rotary knobs** (TUNE, WAVE FORM, RANGE, VOLUME, PORTAMENTO) | Black, round, knurled skirt-less cap with an **orange pointer line** on the top | pointer ~#E07A1F |
| **Function buttons** | Light grey/cream rectangular keycaps | ~#C0B8AD |
| **POWER** button | Black | |
| **LEDs** | Small **red** rectangular LEDs (GL-9PR2) above POWER, the 7 function buttons and next to LFO/CLK RATE | off ~#3A2226, on ~#FF2A1A with bloom [SN p2] |
| **Portamento / transpose switches** | Small horizontal slide switches with a **chrome/silver bat** in a black oval slot | |
| **Bender** | Black lever with a short handle in a black rectangular slot | |
| **Keys** | White naturals, black sharps (glossy black) | white ~#F4F4F0, black ~#151515 |

### 5.2 Typography and logos

- Section headers (MODULATOR, VCO, SOURCE MIXER, VCF, VCA, ENV): bold condensed grotesque, white caps, about 4 mm
  cap height [PHOTO]. Control labels: about 2 mm caps of the same family. **Helvetica Bold / Helvetica Condensed
  Bold** is a close match (UNCONFIRMED exact face).
- Waveform legends are drawn as symbols: pulse (⊓⊔), saw (⊿), triangle, square and envelope shapes.
- "Roland" wordmark with the R-logo; "SH-101" in the **dot-matrix logotype** (each glyph built from small square
  dots), right side of the function row.

### 5.3 Dimensions for the plugin canvas

- Physical: 570 x 311 mm → aspect **1.833**. Suggested base UI: **1466 x 800 px** (2.57 px/mm) or 1833 x 1000.
  Use the mm coordinates in sections 1.2-1.6 directly × the px/mm factor.
- Slider travel ≈ 30 mm, slot pitch ≈ 15 mm, cap ≈ 10 x 13 mm (UNCONFIRMED, from [PHOTO]).

### 5.4 Reference photos (free licenses; see `docs/ref/CREDITS.md`)

1. `ref/sh101-grey-topview_CC-BY-SA-3.0_909bd.jpg`: straight-on top view, the layout master.
2. `ref/sh101-grey-angled_CC-BY-2.0_eternal-jamzzzz.jpg`: true grey tone and cap colours.
3. `ref/sh101-blue-angled_CC-BY-2.0_byCharly.jpg`: blue variant.
4. `ref/sh101-logo-env-closeup_CC-BY-SA-2.0_Mickey-Destro.jpg`: logo typography and slider-cap detail.

Others listed in CREDITS.md: blue on JD-800 (CC BY 2.0), Commons sequencer crop (CC BY-SA 3.0), red on stage (PD),
Flickr front view (CC BY-SA 2.0).

---

## 6. Classic patches

Notation: sliders 0-10. RANGE in feet. Switches: PWM {LFO, MAN, ENV}; SUB {1OCT, 2OCT, 2OCT-PULSE}; VCA {ENV,
GATE}; TRIG {G+T, GATE, LFO}; PORTA {OFF, AUTO, ON}; TRANS {L, M, H}. Unlisted controls = 0 / irrelevant.

### 6.1 Factory "Sample Sounds" (read from the owner's manual diagrams, about ±1 division)

The manual's 40 sample sounds include Flute, Clarinet, Oboe, Trumpet, Horn, Tuba, Violin, Electric Bass Guitar,
Harpsichord, E.Piano I/II, Accordion, Xylophone, Vibraphone, Glockenspiel, Claves, Synthe Drum, Fuzz Guitar,
Funky, Jazz Organ I/II, Effect Sound I/II, Funny Cat, Pulsar, Gong, Funky Bass, Synthe Voice, Whistle, Barking,
Chirping, Wind, Surf, Steam Locomotive, Machine Gun, Space Sound I/II, Boat, Thunder, Random Notes [OM p42-51].
These four were read from the diagrams:

| # | Name | Settings |
|---|---|---|
| F1 | **Electric Bass Guitar** [OM p43] | RANGE 16', TRANS **L**. PW 5, PWM MAN. Pulse 10, Saw 8, Sub 2. FREQ 3.5, RES 2, ENV 7, KYBD 6.5. VCA ENV, TRIG G+T. A 0, D 4.5, S 4, R 5. "Vary FREQ and ENV" |
| F2 | **Funky Bass** [OM p48] | RANGE 16'. Pulse 8, Sub 8 (1OCT). FREQ 4, RES ~1-2, ENV 5, KYBD 4. VCA ENV, TRIG G+T. A 0, D 3, S 2, R 2. "FREQ, RES and ENV change it drastically" |
| F3 | **Synthe Drum** [OM p46] | All sources 0 (self-oscillating filter). FREQ 1.5, **RES 10**, ENV 6, KYBD 8. VCA ENV, TRIG GATE. A 1, D 4, S 1, R 3. Variant: WAVE FORM square/triangle, VCF MOD ≈ 5, high RATE |
| F4 | **Violin** [OM p43] | RANGE 8'. Saw high. **PORTA AUTO, time ≈ 3**. Vibrato with the LFO∿ MOD (bender push). Add the pulse for a different tone. Moderate A, high S |

The manual also gives these hints: Tuba and E.Bass with TRANSPOSE L. Space Sound I with A = 10. Wind, Surf, Steam
Locomotive and Space Sound II use **HOLD on** + noise. Jazz Organ II uses VCF self-oscillation tuned an octave,
3rd or 5th from the VCO. Jazz Organ I adds key click with ADSR all at 0 + VCF ENV [OM p42-51].

### 6.2 Classic/community patches (UNCONFIRMED settings; typical recipes, tune by ear)

| # | Name / style | Settings |
|---|---|---|
| P1 | **Sub bass** (house/techno) | RANGE 16', TRANS L. Pulse 0, Saw 0, **Sub 10 (1OCT)**, plus Pulse 6 (PW 0, MAN). FREQ 3, RES 0, ENV 3, KYBD 5. VCA ENV, G+T. A 0, D 3, S 7, R 1 |
| P2 | **Acid squelch** (303-ish on a 101) | RANGE 16'. Saw 10, Sub 4 (1OCT). FREQ 2.5, **RES 7-8**, **ENV 7**, MOD 0, KYBD 3. VCA ENV, TRIG GATE (slides via LEGATO steps). A 0, **D 3**, S 0, R 2. **PORTA AUTO, time 2** for slides; sequencer with ties [RA "Acid Lead"], [OM p34] |
| P3 | **Bubbly sequence bass** | RANGE 8', TRANS L. Pulse 10 (PW 3, MAN), Sub 6 (2OCT). FREQ 3.5, RES 5, ENV 5, KYBD 5. G+T. A 0, D 2.5, S 0, R 2. Sequencer at about 7-8 RATE |
| P4 | **PWM string-bass / pad** | RANGE 16'. Pulse 10, **PW 6, PWM LFO**, RATE 3.5, triangle. Sub 4 (1OCT). FREQ 5, RES 1, ENV 2, KYBD 7. A 2, D 5, S 7, R 4 [RA "PWM Lead"] |
| P5 | **Snappy pluck lead** | RANGE 4'. Saw 10. FREQ 4, RES 4, ENV 6, KYBD 10. A 0, D 2.5, S 2, R 3 |
| P6 | **Orbital-style rave lead** (UNCONFIRMED) | RANGE 8'. Pulse 10 (PW 4, PWM LFO slow), Saw 6. FREQ 6, RES 3, ENV 3, KYBD 8. LFO 4, MOD (VCO) 0. PORTA AUTO 2. A 0, D 5, S 8, R 4 [VSE mentions an Orbital-style lead patch] |
| P7 | **Chiptune lead** | RANGE 4'. Pulse 10 (PW 0 = square). Sub 7, **2OCT**. FREQ 10, RES 0, KYBD 10. VCA GATE. LFO triangle RATE 6, VCO MOD 1 (subtle vibrato) [RA "Chiptune Lead"] |
| P8 | **Self-oscillating whistle / sine lead** | All sources 0. **RES 10**, FREQ ~6, **KYBD 10**. VCA ENV. A 2, D 5, S 8, R 4. PORTA AUTO 2. LFO MOD via the bender for vibrato [OM p48 "Whistle"], [RA "Opera Lead"] |
| P9 | **Kick drum** | Sources 0. RES 10, FREQ 1, **ENV 8**, KYBD 6. VCA ENV, G+T. A 0, D 3, S 0, R 2 (pitch-dropping filter ping) |
| P10 | **Snare / hat (noise)** | Noise 10, Pulse 0. FREQ 7, RES 2, ENV 4, KYBD 0. A 0, D 2 (snare 3), S 0, R 2 |
| P11 | **Zap / laser** | Saw 10. FREQ 2, RES 8, ENV 10. A 0, D 2, S 0, R 1. RANGE 4' |
| P12 | **Random S&H bleeps** (Space, "computer") | Pulse 10, RANGE 4'. **WAVE FORM RANDOM**, RATE 7, **VCO MOD 6**, VCF MOD 4. FREQ 5, RES 6, KYBD 6. TRIG **LFO**, A 0, D 2, S 0. Hold a key or HOLD on [OM p51 "Random Notes"] |
| P13 | **Random filter arp** | Saw 10, Sub 5. FREQ 3, RES 7, ENV 4, **VCF MOD 5**, **WAVE FORM RANDOM**. Patch GATE OUT → EXT CLK IN so each note gets a new random cutoff [OM p38] |
| P14 | **Arp pluck** | RANGE 8'. Pulse 10 (PW 2). FREQ 4, RES 5, ENV 5, KYBD 6. G+T. A 0, D 2, S 0, R 2. ARPEGGIO U&D, RATE ≈ 6, HOLD on |
| P15 | **Trill lead** | Pulse 10. WAVE FORM **square**, RATE 6.5, VCO MOD set for a whole-tone trill (≈ 1-2). FREQ 7, KYBD 8. VCA GATE |
| P16 | **Noise wind / surf** | Noise 10. FREQ 4, RES 7, **VCF MOD 6**, WAVE FORM triangle RATE 1.5. VCA ENV, A 6, S 10, R 7. **HOLD on** [OM p49-50 "Wind"/"Surf"] |
| P17 | **Thunder / rumble** | Noise 10, RANGE n/a. WAVE FORM NOISE, VCF MOD 6, FREQ 2, RES 4. A 3, D 8, S 0, R 8 [OM p51 "Thunder"] |
| P18 | **Brass stab** | Saw 10, Pulse 4. FREQ 3.5, RES 1, ENV 6, KYBD 8. A 1, D 4, S 6, R 2. Fine-tune with VCF ENV [OM p43 "Horn"] |
| P19 | **Electro-funk bass** (Prodigy/808 State-era; UNCONFIRMED) | RANGE 16'. Pulse 8 (PW 2), Sub 10 (1OCT). FREQ 2.5, RES 4, ENV 6, KYBD 4. G+T. A 0, D 3.5, S 1, R 2. PORTA AUTO 1.5 |
| P20 | **Boards-of-Canada-style detuned pad lead** (UNCONFIRMED) | Saw 10. FREQ 4, RES 2, ENV 2, KYBD 6. A 4, D 6, S 8, R 6. LFO triangle RATE 1, **VCO MOD 0.5** (slow wobble), PWM LFO. Add a slow pitch drift in the plugin "age" control |

Artists documented using the SH-101: The Prodigy, Orbital, Aphex Twin, The Chemical Brothers, Boards of Canada,
Autechre, 808 State, The Future Sound of London, Luke Vibert, Squarepusher, Josh Wink, Nitzer Ebb, Devo, and others
[WP], [VSE]. I found no primary documentation of specific artist patches, so P6, P19 and P20 are stylistic
approximations.

### 6.3 Example sequences and arpeggios (original material for presets)

Notation: one token = one step (100 max). `-` = tie (LEGATO, extends the previous note), `r` = rest (REST),
`~X` = slur into X (LEGATO held while pressing X). Keys relative to the lowest C at TRANS M.

| # | Name | Steps | Suggested patch |
|---|---|---|---|
| S1 | **Octave house bass** (16 steps) | `C C' r C  C' r C C'  r C C' r  C C' Eb' r` (C = low C, C' = octave up) | P1 or P3, RATE ≈ 7 (≈ 120 BPM 16ths) |
| S2 | **Acid line** (16 steps, A minor) | `A ~A' r A  G r ~C' A  r A ~E' D'  A r G ~A` | P2, ENV switch **GATE** so slurs glide (PORTA AUTO) |
| S3 | **Electro riff** (16 steps) | `C r C Eb  r C F r  C - Bb r  G r Eb r` | P19 |
| S4 | **Rolling 3-note arp** | ARPEGGIO **UP** on C-Eb-G-Bb, HOLD on; KEY TRANSPOSE to F then G every 2 bars | P14 |
| S5 | **U&D trance arp** | ARPEGGIO **U&D** on A-C-E-A', RATE synced via EXT CLK (16th pulses) | P14 / P6 |
| S6 | **Random bleep sequence** | Any 8-step sequence at RATE 8 + WAVE FORM RANDOM to VCF MOD 5, GATE OUT → EXT CLK IN | P13 |

Implementation note: use the sequencer's KEY TRANSPOSE (−7..+24 st) for live transposition, and keep the
TRANSPOSE switch inactive during PLAY, as on the hardware [OM p31], [OM p36].

---

## 7. Open items (UNCONFIRMED summary)

1. Slider taper laws (A vs B) per slider. The parts list has both 100KA and 100KB parts but does not map them to
   controls [SN p2].
2. Max depths: VCO MOD, VCF MOD, VCF ENV, VCF bend, LFO MOD split between VCO and VCF.
3. LFO square polarity (unipolar trill vs bipolar).
4. Exact PWM mapping and minimum pulse width.
5. Attack target ratio and the comparator threshold of the discrete ADSR.
6. Sequencer gate duty (assumed 50 % of the clock period) and what happens if ARP and PLAY are both pressed.
7. Self-oscillation level, and the mixer level ratios between pulse, saw, sub and noise.
8. Red/blue exact paint colours (sample a real unit or a colour-calibrated photo).

These can be settled by measuring the service-notes schematic in detail ([SN p9] has every component value), or
by measuring a real unit: record an envelope/LFO/filter sweep at the OUTPUT jack and fit the curves.
