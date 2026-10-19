# Bass line synthesizer with sequencer (AT303): research

Compiled 2026-10-19 for a new Audio Tools product: a free, open-source (AGPL-3.0) recreation of the Roland
TB-303 Bass Line (1981-1984), working name **AT303**. The series names recreations "AT" plus the original's
number (AT20V = MS-20, AT101V = SH-101, ATMD = Minimoog). It covers the circuit (§1), the sequencer (§2), the
products and what users think of them (§3), a DSP proposal (§4), features beyond the original (§5), the visual
direction (§6), tests (§7), factory patterns (§8) and the decisions left to make (§9).

Conventions (the same as `plugins/lacquer/docs/research.md` and `plugins/tarnish/docs/research.md`):
- Every fact carries its source inline: a URL (all accessed 2026-10-19) or a document with page or section.
  Short tags are defined in the table below. Where only a forum post, a retailer rating or a third-party quote
  was available, the text says so.
- **(inferred)** marks my own conclusions, calculations or design proposals. Values marked "calculated"
  follow from a formula in the text; the scripts are trivial and the numbers can be re-derived. §4-§9 are
  design proposals throughout; their numbers are starting values to tune with the tests of §7.
- **The web search budget of this session was exhausted before the research began** (a shared limit). All
  facts below come from fetching known primary URLs, cloning open-source repositories and reading the manuals
  and scans. Forum sweeps (Reddit, Gearspace, Elektronauts, ModWiggler, YouTube) could not be done; user opinion
  comes from KVR reviews, retailer ratings and press reviews. §9 lists the follow-up searches.
- Product names (Roland, TB-303, TB-03, TD-3, Devil Fish, ReBirth, x0xb0x and the others) are trademarks of
  their owners and appear here only for research. **The product shows no brand names, logos or the
  original's trade dress** (§6.1). Feature names are generic ("dual decay", "accent sweep speed"), never
  "Devil Fish".
- Nothing here copies proprietary text or code. Copyrighted references (service notes, owner's manual, the
  Devil Fish manual, Stinchcombe's figures, product manuals, schematic scans) are kept only in
  `/Volumes/Docs/Projects/own/audiotools-private/at303-ref/` and are referenced, never committed.

Source tags used throughout:

| Tag | Document | Where |
|---|---|---|
| **[SN pN]** | Roland TB-303 Service Notes, 1st ed., dated Feb. 19, 1982 ("Nov. 1984 E-2" on p1). p1 specs and parts, p3 block diagram, p4 digital schematic, p5 analogue schematic, p6-7 adjustment, p8 "VCF envelope modulation" and parts list | https://www.synthmanuals.com/manuals/roland/tb-303/service_notes/roland_tb-303_service_notes_rvgm.pdf; page scans by Robin Whittle at http://machines.hyperreal.org/manufacturers/Roland/TB-303/schematics/. Private copies: `at303-ref/Roland-TB-303-Service-Notes-synthmanuals.pdf`, `at303-ref/hyperreal/` |
| **[OM pN]** | Roland TB-303 Owner's Manual (92 pp.; page numbers of the archive.org scan, which match the printed numbers on pp. 89-90) | https://static.roland.com/assets/media/pdf/TB-303_OM.pdf, OCR at https://archive.org/details/synthmanual-roland-tb-303-owners-manual |
| **[DF pN]** | Robin Whittle, Devil Fish user manual, 2019-02-09, 45 pp. | https://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf |
| **[DF-U]** | Robin Whittle, "What makes the TB-303 unique" (1 June 1999) | https://www.firstpr.com.au/rwi/dfish/303-unique.html |
| **[DF-S]** | Robin Whittle, "TB-303 slide and gate timing" (31 May 1999, with a June 2012 caveat at the top) | https://www.firstpr.com.au/rwi/dfish/303-slide.html |
| **[DF-M]** | Robin Whittle, mods page for stock TB-303s | https://www.firstpr.com.au/rwi/dfish/303-mods/ |
| **[DF-MEM]** | Robin Whittle, "Devil Fish memory backup" | https://www.firstpr.com.au/rwi/dfish/DF-Memory-Backup.pdf |
| **[TS08 §n]** | Tim Stinchcombe, "Analysis of the Moog Transistor Ladder and Derivative Filters", 25 Oct 2008, chapter 3 "Diode Ladder Filters" (pp. 25-50) | https://www.timstinchcombe.co.uk/synth/Moog_ladder_tf.pdf |
| **[TS-D1]**, **[TS-D2]** | Tim Stinchcombe, TB-303 diode ladder pages (with updates of 19 Feb and 19 Jul 2009) | https://www.timstinchcombe.co.uk/index.php?pge=diode, https://www.timstinchcombe.co.uk/index.php?pge=diode2 |
| **[VAFD §n]** | Vadim Zavalishin, *The Art of VA Filter Design*, rev. 2.1.2, 14 Feb 2020. §5.10 "Diode ladder" (pp. 164-172), §6.6, §6.9 (pp. 200-201), §6.13 | mirror https://www.discodsp.net/VAFilterDesign_2.1.2.pdf (the Native Instruments URLs return 404) |
| **[O303]** | Robin Schmidt, Open303, MIT licence (`License.txt`), commit 313bf0d (2024-03-29) | https://github.com/RobinSchmidt/Open303 |
| **[X0X]** | x0xb0x (Adafruit / Limor Fried), MIT licence; mirror of the ladyada.net build pages and firmware | https://github.com/x0xb0x/x0xb0x.github.io, firmware via https://www.openmusiclabs.com/wiki/x0xb0x |
| **[HR]** | Analogue Heaven posts archived at hyperreal (1995-1997): `roland.TB-303.pattern.lengths.txt` (Whittle 1997), `roland.TB-303.programming.txt` (Perkowitz 1995), `roland.TB-303.secrets.txt`, `TB-303.internal-pots.txt`, `roland.303.mods.txt`, `303.memory.txt`, `roland.TB-303.txt` (Haines 1995) | http://machines.hyperreal.org/manufacturers/Roland/TB-303/info/ |
| **[WP]** | Wikipedia, "Roland TB-303" (and the German article, [WP-de]) | https://en.wikipedia.org/wiki/Roland_TB-303, https://de.wikipedia.org/wiki/Roland_TB-303 |

---

## 0. Summary of recommendations

**The hardware** (§1). One exponential VCO (a falling ramp on a 0.01 µF core, 1 V/oct, A = 110 Hz at 2.75 V,
pattern range 32.7-523 Hz plus ±7 semitones of tuning [SN p1, p5, p7]); the square is not generated by a
comparator but by a single PNP transistor (Q8) fed from the ramp through a 0.01 µF capacitor, which is why it
droops, is not 50 % and changes with pitch [SN p5], [X0X]. The filter is a four-pole **diode ladder** whose
bottom capacitor is about half the others (0.018 µF against 0.033 µF [SN p5]); it is 24 dB/oct asymptotically
but its corner is so soft that the octave two to four times above the resonant frequency falls at about
19 dB/oct, the source of the "18 dB" myth [TS08 §3.5]. Its loop gain for self-oscillation is k = 17 (against 4
for the Moog), so resonance costs 25 dB of bass [VAFD §5.10] (calculated, §1.5.3), and coupling capacitors
inside the loop add a low-frequency resonance near 10 Hz [TS-D1], [TS-D2]. Stock units do not self-oscillate
across the range. Two envelopes: the VEG (VCA only, fixed decay of a few seconds) and the MEG (filter; Decay
200 ms-2.5 s to 10 %, forced to 200 ms on accents) [SN p5], [DF p3-6]. Accents add the MEG to the VCA and
feed it to the cutoff through the **accent sweep circuit**: a diode, 47 kΩ and a 1 µF capacitor on the second
gang of the Resonance pot, whose charge accumulates over consecutive accents. That accumulation is "the wow"
[DF-U], [DF p4]. Slide is a 100 kΩ into 0.22 µF RC on the pitch CV (τ = 22 ms, "60 ms" audible, independent
of the interval) [DF p16], [SN p5].

**The sequencer** (§2). Pitch and time are separate lists: Pitch mode stores notes with octave up/down,
accent and slide; Time mode stores note/tie/rest per step; each "note" step consumes the next pitch. This is
why editing one scrambles the other, and why accents "move" [OM p12-51], [HR Perkowitz], [HR Whittle 1997].
24 ppq clock, 6 clocks per 16th (8 in triplet mode); the gate is high for 3.5 of 6 clocks; a slid note keeps
its gate high into the next note and the slide switch is on for the whole of the following note; no envelope
retrigger without a gate edge [DF-S].

**Market** (§3). AudioRealism ABL3 ($95) is the accuracy reference in user reviews; D16 Phoscyon 2 (€119) won
the press with its sequencer; Arturia Acid V ($149) and Roland's own plug-in ($149, ACB) add generators and
effects; Behringer's TD-3 (≈ €100) made the sound cheap but users fight its sequencer; JC303 (GPL-3, built on
MIT Open303) is the free competitor. Users judge accents and resonance first, then the sequencer. They
complain about hardware-style step entry, built-in distortion that is worse than outboard, lossy MIDI export,
fixed effect chains and online authentication. Nobody publishes measurements; no free product models the
accent capacitor, the coupling-capacitor poles and the measured envelope times together.

**DSP** (§4). At 4x oversampling (the shared half-band chain): ramp core with PolyBLEP/BLAMP and a soft reset;
the square from a small circuit model of the Q8 stage, pre-computed in SPICE into a pitch-indexed,
band-limited table set; a nonlinear diode ladder in TPT form with the actual capacitor ratio, Zavalishin's
tanh-difference stages solved with the "cheap ZDF" linearisation the repo already uses (AT101V, ATMD) and an
optional 2-iteration Newton mode; the coupling high-passes from component values; the accent sweep as a
two-node RC with a diode; the slide as the 22 ms RC; MEG and VEG as RC segments with the measured times.
Budget: ≤ 1.0 % of a core on the Apple M4 and ≤ 2.0 % on the Ryzen 9 5900XT for the default sound at 4x.

**Beyond the original** (§5). P0: the classic pitch/time entry and a modern lane grid on the same data;
host sync with sample-accurate gates; MIDI play mode (configurable accent threshold or continuous accent,
slide on overlap or CC 65); 64 patterns in 4 groups, chaining and tracks; a constrained acid generator with
seed, lane locks and mutate; an "extended" page with the classic mod set (overdrive, dual decay, soft attack,
slide time, accent sweep speed, filter tracking, filter FM, post-VCA soft clip, high-resonance); lossless
`.mid` round trip. P1: drive models, delay and reverb, swing, a "battery loss" scramble, unit condition trims.

**Visual** (§6). Recommended: direction A, an original photoreal instrument (not a silver box), rendered in
Blender like ATMD, with a live "inspect" layer that shows the accent capacitor charging and the filter
response (direction C's idea), and a lane-grid sequencer page.

**Tests** (§7). 18 JUCE-free engine tests: linear filter response against the analytical diode-ladder
transfer function (±0.3 dB), k = 17 oscillation threshold, calibration ring of 2.0 ms ± 0.1 ms at the trimmer
setting, decay and slide times, accent accumulation against a SPICE reference, gate timing in clocks,
pitch/time scramble golden files, block-size determinism, sample-rate independence, CPU, fuzz.

**Presets** (§8): 32 patterns with sounds in six categories, described in words. **Decisions** (§9): 20.

---

## 1. The hardware

### 1.1 Identity, specifications and history

| Item | Value | Source |
|---|---|---|
| Released / discontinued | 1981 / 1984 (Attack Magazine says 1982; Sound On Sound lists it among Roland's 1981 launches with production "less than two years") | [WP]; https://www.attackmagazine.com/technique/hardware-focus/roland-tb-303/; https://www.soundonsound.com/people/history-roland-part-2 |
| Designer | Tadao Kikumoto (also the TR-909) | [WP] |
| Launch price | US$395; UK £234 or £238 (the article gives both); £215 retail in the April 1982 *E&MM* review | [WP]; http://www.muzines.co.uk/articles/roland-tb-303-bass-line/4250 |
| Units built | "10,000" (Roland US blog, which also says "TB" stands for "Transistorized Bass"); "rumoured ... nearly 20,000" (Sound On Sound) | http://www.rolandus.com/blog/2013/03/28/tb-303-acid-flashback/; SOS above |
| Patterns | 64 = 8 × A/B × 4 groups (I-IV); store pitch, note length, accent, slide | [SN p1], [OM p89] |
| Steps | 1-16 in sixteenths, 1-15 in triplets | [OM p89] |
| Tracks | 7 tracks × 64 measures; combined "256 measures maximum"; store pattern order, transpose ("shift") and D.C. | [OM p64-66, p89] |
| Range | 3 octaves in a pattern (with transpose), 4 in a track (with shift) | [OM p79, p89] |
| Tempo | quarter note = 40-300 | [SN p1], [OM p89] |
| Outputs | Output 10 kΩ; headphones 8-30 Ω; Gate Out 0/+12 V; CV Out +1 to +5 V, 1 V/oct; Mix In 100 kΩ, unity, mixed after the VCA | [OM p89-90], [SN p3] |
| Sync | DIN Sync **in** only (for TR-606, CR-8000, MC-4, CSQ-600); inserting a plug disables the internal clock | [OM p90]; https://en.wikipedia.org/wiki/DIN_sync |
| Power | 6 V from 4 × C cells or a 9 V adaptor; 80-120 mA (Whittle measured ≈ 85 mA) | [OM p89], [DF p34] |
| Size, weight | 300 × 146 × 55 mm, 1.0 kg | [OM p89] |
| Memory | battery-backed; data kept if the batteries are changed within one minute | [OM p2] |

Roland's US blog states the filter is "often misquoted as 18 dB, 3-pole" and that it "can't self-oscillate"
(rolandus.com above). The German Wikipedia names the digital emulation of the analogue distortion chain as the
main remaining difficulty for emulations [WP-de].

### 1.2 Block diagram and signal flow [SN p3], [SN p5]

- **Digital.** CPU IC8, NEC µPD650C-133 (a µCOM-43 4-bit MCU; MAME clocks it at 454.545 kHz:
  https://raw.githubusercontent.com/mamedev/mame/master/src/mame/roland/roland_tb303.cpp, BSD-3-Clause).
  Three µPD444C 1K × 4 CMOS RAMs (IC3-IC5) on battery. The tempo pot VR1 drives an oscillator in IC6, divided
  by D flip-flops (IC1) through a mono-multi and a Schmitt trigger.
- **Pitch.** Latch IC9 (HD14174) feeds a resistor-ladder DAC (R74-R90, seventeen 200 kΩ resistors
  "matched pair ±1 % selected" on the scan; 1 % on the x0xb0x [X0X fab/vco]); op-amp IC11 buffers it to CV
  OUT. A 4066 switch (IC12) and C35 form the **slide** circuit, so CV OUT carries the slide [OM p79].
- **VCO.** Exponential converter Q26 (2SC1583 matched pair, with a 560 Ω PTC "posistor" R100 for temperature
  compensation) with the TUNING pot VR2 and trimmers TM4 TUNE / TM5 WIDTH; reset transistors Q24, Q25, Q27;
  JFET buffer Q28 (2SK30); saw-to-square converter Q8 and the WAVEFORM switch S1.
- **VCF.** "TR. C LADDER" (diode-connected transistors and capacitors), anti-log converter Q10/Q11, pots
  CUTOFF FREQ VR3, RESONANCE VR4 (dual), ENV MOD VR5; trimmer TM3 FREQ.
- **VCA.** IC15 BA662A (Roland: "vari-conductance amp", an OTA) with its own envelope generator (Q31/Q32,
  the VEG), inputs GATE, TRIGGER and ACCENT.
- **Filter envelope.** A mono-multi drives the "ENV. GEN" (C62, Q40, Q41), the MEG. DECAY VR6 in series with
  R138; an "ACC. ON/OFF" analogue switch bypasses VR6 on accents; the envelope also feeds the ACCENT pot VR7.
- **Output.** VCA → VOLUME VR8 → mixer (Q33, Q34) → headphone amp IC14 (LA4140); OUTPUT from the mixer; MIX
  IN joins the mixer (after the VCA, not into the filter).
- **Rails.** +12 V and 5.333 V (IC16, the DAC reference, trimmed by TM6) and 6 V (Q42).

Signal path to model **(inferred, from the above)**: CPU → DAC → [slide RC] → expo → ramp core → (saw | Q8
square) → input coupling → diode ladder (cutoff from CUTOFF + ENV MOD × MEG + accent sweep) → output
differential amp with coupling capacitors → OTA VCA (VEG + accent × MEG) → volume → mixer → output.

### 1.3 Pitch path: DAC, CV, tuning, transpose

- **Calibration.** "TUNE: 110 Hz (set TUNING centre, CV OUT 2.75 V, A key)", "WIDTH: 1 oct/volt" [SN p5]. The
  adjustment: TUNE centred, alternate the low and high C and set TM5 for 2:1, set TM4 for 110 Hz on A, check
  4:1 ± 0.5 % with TRANSPOSE UP [SN p7]. CV: low C to high C must read CVL + 1.000 V ± 3 mV, and the same with
  TRANSPOSE UP [SN p6].
- **Range.** The OM's "Sound Range Diagram": 1 V = 32.7 Hz, 2 V = 65.4 Hz, 3 V = 130.8 Hz, 4 V = 261.6 Hz,
  5 V = 523.2 Hz. The normal keyboard covers 2-3 V, transpose down 1-2 V, transpose up 3-4 V; a track's shift
  adds up to one more octave. The OM writes its example scores one octave above the sounding pitch [OM p88].
  The x0xb0x documents the same 2.0 V = 65.4 Hz [X0X fab/vco].
- **Tuning knob.** The sources disagree: "approx. ±700 cents (perfect fifth)" [SN p1]; "±500 cents" [OM p89].
  The OM's tuning procedure tunes the A key to 220 Hz [OM p33] while the service notes calibrate it to 110 Hz,
  an octave inconsistency I could not resolve (probably the OM's octave-up score convention) **(inferred)**.
  Tuning moves the VCO only, not CV OUT or the filter [DF p15].
- **Pitch law (calculated).** f = 65.41 Hz × 2^(CV − 2 V) with the knob centred; 32.7-523 Hz over 1-5 V;
  ±7 semitones of tuning extend that to 21.8-784 Hz.
- **No filter tracking.** The filter cutoff does not follow the note [DF p5].

### 1.4 VCO

**Core [SN p5]** (my reading of the 150 dpi scan, checked visually; component designators as printed):
- IC11a (AN6562) with C34 = 0.001 µF sums the CV and drives the reference side of Q26; the timing capacitor
  is **C33 = 0.01 µF**, discharged by Q26's collector current and reset by Q24/Q25/Q27 (2SC536/2SA733, D25,
  R101 10 kΩ); Q28 (2SK30 JFET source follower, R105 10 kΩ) buffers it. Other values: R103 220 kΩ, R104 2.2 kΩ, R106 27 kΩ (24.0 kΩ 1 % on the x0xb0x), R118 100 kΩ, TM5 4.7 kΩ.
- The waveform sketch on the schematic is a **falling ramp** between about 12 V and 5.5 V with a vertical
  reset (≈ 6.5 V peak to peak); the square sketch sits between about 8 V and 5 V, period "9 ms KEY A" (the 110 Hz
  A) [SN p5].
- Charge current (calculated): I = C·ΔV·f = 0.01 µF × 6.5 V × 65.4 Hz ≈ 4.3 µA at the bottom C, 34 µA at the top
  of the pattern range.
- The x0xb0x notes say the saw is about 5.5 Vpp on an 8.5 V bias, and that "the tip of the saw isn't sharp"
  [X0X fab/vco]: the reset takes a finite time **(inferred: Q24/Q25 recharge C33 through their saturation
  resistance; model it as a short exponential reset, §4.2)**.
- Open303 inverts the oscillator sign before the filter [O303 `rosic_Open303.h`]; polarity matters for the
  asymmetry of the ladder's nonlinearity **(inferred)**.

**The square [SN p5], [X0X fab/vco]:**
- The square is made by a single **PNP transistor Q8 (2SA733)**, not a comparator. As drawn: the ramp from
  Q28's source reaches Q8's base through **C10 = 0.01 µF** and **R34 = 10 kΩ**; Q8's emitter sits on
  **R45 = 22 kΩ** to the supply, bypassed by **C11 = 1 µF/50 V**, and is tied to the ramp node through
  **R35 = 100 kΩ**; the output is taken across **R36 = 10 kΩ** in the collector.
- The x0xb0x documentation: the square is about 4 Vpp on a 7 V bias, "is not really that square", "droops
  slightly and isn't 50 % duty cycle" [X0X fab/vco].
- Open303 approximates it as a static waveshaper of the saw: −tanh(gain·saw + 4.37) with the gain at 36.9 dB,
  then circularly shifted by 180° (189° in the VST default) [O303 `rosic_MipMappedWaveTable.cpp`]: a hard,
  offset tanh, i.e. a fixed duty cycle and no pitch dependence.
- **(inferred)** Because the base is driven through a capacitor, the transistor responds to the *slope* of
  the ramp (base current ≈ C10·dV/dt, about 4 µA at the bottom C, so the collector current is β times that
  until Q8 saturates), while the emitter's DC point follows the ramp's average through R35/C11 (τ ≈ (22 k ∥
  100 k) × 1 µF ≈ 18 ms). So the duty cycle, the droop of the "flat" parts and possibly the amplitude at the
  lowest notes should depend on pitch, which fits the x0xb0x description and which Open303's static shaper
  does not capture. This is a hypothesis from the topology; I found no published analysis of duty cycle
  against pitch. It must be checked by simulating the stage (§4.2) and against recordings.

### 1.5 VCF: the diode ladder

#### 1.5.1 Components [SN p5], [X0X fab/vcf]
- Ladder capacitors C26, C24, C19 = **0.033 µF**; the bottom capacitor C18 = **0.018 µF** (ratio 0.545, so
  "half" is approximate).
- Ladder "diodes": diode-connected transistors Q13-Q17 and Q23 (2SC536F on the x0xb0x); bottom differential
  pair Q12 (2SC1583), top pair Q22 (2SC2291). The single transistor pair at the top behaves like one diode
  per arm [TS-D1].
- Output: differential amplifier Q21 (2SC1583), AC-coupled through C25/C27 = 0.1 µF with R113/R114 =
  100 kΩ; emitter follower Q19.
- Electrolytics: 1 µF at C13, C14, C15, C17, C22, C23, C29; 10 µF at C16, C30; 47 µF at C28. R62 220 kΩ.
- Pots: CUTOFF VR3 50 kΩ (A) with R47 10 kΩ; RESONANCE VR4 dual 50 kΩ (B); ENV MOD VR5 50 kΩ (A) with R61 10 kΩ;
  R63 220 kΩ; R72, R73 100 kΩ. Trimmer TM3 FREQ 470 kΩ (500 kΩ on the x0xb0x).
- Florian Anwander: a 24 dB four-stage diode ladder from transistors with collector tied to base, with no filter
  changes over the production life (http://www.florian-anwander.de/roland_filters/).

#### 1.5.2 The linear transfer function
- **Stinchcombe** derives the diode ladder for one or three top diodes and equal or halved bottom capacitor
  [TS08 §3.1-3.3]. For the TB-303 (one diode, C1 = C/2) the open-loop core is, normalised,
  G(s) = −1 / (s⁴ + 6.727 s³ + 14.142 s² + 9.514 s + 1), exactly −1 / (s⁴ + 2^(11/4) s³ + 10√2 s² + 2^(13/4) s + 1),
  with ω_c = I_f / (2^(7/4)·C·V_T). With equal capacitors: −1/(s⁴ + 7s³ + 15s² + 10s + 1) [TS08 eq. 30-31].
  With feedback k: H(s) = 1 / (D(s) + k) [TS08 eq. 34]. All four poles are real at k = 0: −3.24, −2.33, −1.04,
  −0.13 [TS-D1].
- **Zavalishin** writes the diode ladder as four coupled one-poles [VAFD eq. 5.18]: ẏ1 = ω_c((x + y2) − y1);
  ẏ2 = ω_c((y1 + y3)/2 − y2); ẏ3 = ω_c((y2 + y4)/2 − y3); ẏ4 = ω_c(y3/2 − y4). Its transfer function is
  1/T4(s + 1), with T4(x) = 8x⁴ − 8x² + 1 the 4th Chebyshev polynomial ("nothing to do with Chebyshev
  filters"), poles at −1 ± 1/2 ± 1/(2√2), unity DC gain; with feedback H(s) = 1/(8(1+s)⁴ − 8(1+s)² + 1 + k);
  the phase crosses 180° at ω = 1/√2 and **self-oscillation starts at k = 17** [VAFD eq. 5.26-5.29, Fig. 5.51].
  The book never names the TB-303.
- **Cross-check (calculated).** Expanding Zavalishin's denominator gives 8s⁴ + 32s³ + 40s² + 16s + 1, which is
  Stinchcombe's unnormalised TB-303 case with aCs → s, so the two agree. Writing the first stage as
  ẏ1 = (ω_c/(2r))(x + y2 − y1) with r = C1/C (the bottom pair injects a current, the diode above it conducts
  between y1 and y2) reproduces both sources: r = 0.5 gives k_osc = 17.000 at ω = 0.7071, and r = 1 gives
  k_osc = 18.39 (Stinchcombe's "≈ 18.4" for the equal-capacitor case [TS08 §3.5]). With the real ratio
  r = 0.545 the threshold is 17.02 and the resonant frequency 2.1 % lower: the difference is a calibration
  offset, not a change of character.
- **Numbers that matter for the model (calculated, r = 0.5, frequencies relative to the resonant frequency
  ω₀ = 1/√2):**

| | k = 0 | k = 8.5 (half) | k = 16 (94 %) |
|---|---|---|---|
| DC gain | 0 dB | −19.6 dB | −24.6 dB |
| Peak position | none (low-pass) | 0.75 ω₀ | 0.98 ω₀ |
| Peak above DC | — | +7.8 dB | +29.4 dB |
| Slope, octave by octave from ω₀ | −13.7, −18.6, −22.1, −23.5, −23.9 dB | −19.3, −19.2, −22.1 ... | −37.3, −19.6, −22.0 ... |

  - The −3 dB point at k = 0 is at 0.0745 in Zavalishin's units, **9.5 × (3.2 octaves) below** the frequency
    where resonance appears. A "cutoff" knob must therefore be calibrated to the resonant frequency (as Roland
    did, §1.5.5), not to the −3 dB point.
  - The resonant peak *moves up* with resonance (0.75 ω₀ at half, 0.98 ω₀ near the top).
  - At k = 17 the passband has lost 25.1 dB (the Moog ladder loses 14.0 dB at k = 4): resonance thins the bass
    far more than in a Moog.
- **18 or 24 dB/oct.** Stinchcombe: the filter "is" fourth-order, 24 dB/oct; "3-pole, 18 dB/oct" is "at best
  misleading" and describes the early, gentle corner caused by the dominant real pole near the axis; SPICE
  slopes per stage are −6.1, −11.9, −17.9 and −24.1 dB/oct, and the far stopband is −23.9 dB/oct
  [TS08 §3.5], [TS-D1]. AudioRealism says the same about ABL3: "actually 24dB/oct with an 18dB response in the
  audible range" (https://www.audiorealism.se/abl3.html). The calculated table above shows where the 18 comes
  from: from 2ω₀ to 4ω₀ the slope is about −19 dB/oct at every resonance.
- **Why it is not a Moog ladder (summary, inferred from the above).** (1) The stages are not buffered: each
  capacitor sees the diode above and below, so the poles are spread (−3.24 to −0.13) instead of a four-fold pole;
  (2) hence a soft knee and a resonance that needs k = 17 instead of 4; (3) resonance takes 25 dB of bass
  instead of 14; (4) the peak moves with resonance; (5) the nonlinearity is a difference of tanh terms between
  neighbouring stages [VAFD §6.9], not a tanh per buffered stage, so it distorts differently.

#### 1.5.3 Coupling capacitors and the low-frequency resonance
- Stinchcombe's update of 19 Feb 2009: the coupling capacitors around the loop add five poles clustered near
  10 Hz with matching zeros; because they are inside the feedback loop, they make the main peak
  frequency-dependent and create **a second resonant peak at low frequency** [TS-D1].
- His update of 19 Jul 2009 gives a full model of 10 poles and 6 zeros (SPICE-validated): a triple zero at
  s = 0, zeros at −109.9, −34.0, −7.41, the ladder denominator plus five low-frequency factors, and a feedback
  term of about 18.7 k s⁴(s + 46.5)(s + 4.40) [TS-D1]. The units of those factors are not stated on the page
  (rad/s is likely) **(inferred)**: check before using them.
- His second page groups the high-pass sections: (1) C17 with R62 and the R68/C16 network; (2) C25/C27 with
  R113/R114 at Q21; (3) C14 from Q19 to the resonance pot; (4) C15 with R66/R99 at Q18; (5) C23 with R97/R98
  at the ladder input. A Laplace-block model with resonance 0-1 and a fixed 820 Hz cutoff agrees with a
  component-level simulation. The low-frequency peak "is very real": his TBX-303 clone oscillated at about
  8 Hz when his test setup loaded it capacitively; he suspects this bass-boosting peak is a large part of the
  303 sound [TS-D2].
- Reading his plot (approximate, from the figure): at zero resonance the mid band (20-500 Hz) sits near
  −5 dB; towards full resonance the mid band falls to about −25 dB while peaks grow near 10 Hz and at the
  820 Hz cutoff [TS-D2 figure `tb303_lap_v_comp_580.gif`, private copy]. This agrees with the 25 dB bass loss
  calculated above.
- Simple corners from the schematic values (calculated): 0.1 µF into 100 kΩ ≈ 16 Hz; 1 µF into 100 kΩ ≈ 1.6 Hz.
- **Open303** places tuned high-passes instead: 44.486 Hz before the filter, a 150 Hz high-pass in the feedback
  path ("reduces the resonance on low cutoff frequencies"), then after the filter an all-pass at 14.008 Hz, a
  high-pass at 24.167 Hz and a notch at 7.5164 Hz (bandwidth 4.7) [O303 `rosic_Open303.cpp`,
  `rosic_TeeBeeFilter.h`]. The code does not say how they were derived. **(inferred)** The 150 Hz feedback
  high-pass is an order of magnitude above the corners the components give; it is a perceptual fit for the
  weaker resonance at low cutoff, not the circuit. §4.3 proposes the circuit's own sections.

#### 1.5.4 Resonance and self-oscillation
- Stock units are described as non-self-oscillating ([WP] infobox; Roland US blog). The Devil Fish manual,
  for a filter it says is unchanged: it "will only self oscillate at mid and high frequencies"; attempts to
  raise its low-frequency gain made it sound more like a normal filter [DF p23]. The Devil Fish adds a switch
  that doubles the resonance feedback so the filter self-oscillates at mid and high frequencies [DF p5].
- **(inferred)** The stock maximum loop gain sits just below the k = 17 threshold, and the loop's coupling
  capacitors reduce the effective gain further at low cutoff: long rings, no sustained oscillation. The
  service-note calibration (§1.5.5) uses exactly that ring.
- Behringer's TD-3 is reviewed as "nearly self-oscillating" (https://elektronische-musik-produzieren.de/roland-tb-303-clone-die-besten-tb-303-vsts-und-hardware-clones).

#### 1.5.5 Cutoff range, calibration and the Env Mod "gimmick"
- **Factory calibration.** At TP6 with CUTOFF centred, SAWTOOTH, RES fully clockwise and ENV MOD, DECAY and
  ACCENT fully anticlockwise, set TM3 for a **ringing period of 2 ms ± 0.5 ms** [SN p7]; the scope sketch on
  p5 shows the ring decaying between 5.8 V and 5.2 V with "15 ms KEY C". The x0xb0x states the same as 500 Hz
  ± 100 Hz with low C [X0X fab/vcf]. Calculated: 500 Hz nominal, 400-667 Hz tolerance (±25 % of cutoff).
- **Range.** Devil Fish: its maximum resonant frequency without envelope or tracking is about 5 kHz, "an octave
  above the typical maximum for the TB-303", and component variations used to make it vary between units
  [DF p6, p32]; so the stock top is around 2.5 kHz **(inferred)**. Open303's "measured" mapping uses 313.8 Hz
  and 2394.4 Hz as the lowest and highest nominal cutoff, source unstated [O303 `calculateEnvModScalerAndOffset`].
  A 1995 Analogue Heaven post found the first 25 % of the knob useful only for bass sounds and notes TM3 is in
  series with the Cutoff pot [HR `roland.303.mods.txt`]. **(inferred)** A stock range of about 300 Hz to
  2.4-2.5 kHz at full resonance before the envelope, with 500 Hz at mid-knob (the A taper puts the middle low).
- **Env Mod lowers the base cutoff.** Service notes p8: in the TB-303, Q9 biases Q10/Q11 so the starting cutoff
  is mid-range; turning ENV MOD up feeds more envelope *and* lowers the bias, "equivalent to turning CUTOFF
  anticlockwise", so the sweep stays in a useful range [SN p8]. Env Mod cannot reach zero on a stock unit; the
  Devil Fish's upper limit is about 3 × stock [DF p7], [DF-U]. Stock mods: short R61 for zero Env Mod; 100 kΩ
  across R63 to triple its range; short R47 to lower the bottom of Cutoff [DF-M §4.1].
- **Settling.** A large step of the filter frequency completes 99.8 % in under 1 ms, then creeps to the final
  value over about 500 ms, because several capacitors are resistively coupled to the summing node; audible only
  when the filter rings [DF p13].

### 1.6 Envelopes

- **Names** (Whittle's): **VEG**, the Volume Envelope Generator: sharp attack, exponential decay, fixed and
  "rather long", drives only the VCA. **MEG**, the Main Envelope Generator: sharp attack, exponential decay,
  mainly the filter; on normal notes the Decay pot sets its time, on accents the pot is shorted and the MEG
  runs at the pot's minimum time [DF-U].
- **Times** [DF p3-6]: VEG decay "~3 to 4 seconds"; MEG decay 200 ms-2 s (normal), fixed 200 ms (accent);
  non-accented notes: a typical 4 ms delay then a 3 ms attack, audible 1-5 ms after the gate; end of an
  unaccented note about 16 ms after the gate ends (8 ms at full level, then 8 ms of linear decay); accented
  notes keep sounding after gate-off because the MEG still drives the VCA [DF-S].
- **Service notes** [SN p5]: a graph from 100 % to 10 % labelled "DECAY VR MAX T = 2.5 sec, MIN T = 200 ms".
  MEG parts: C62 1 µF, follower Q40/Q41, Q39 2SK30, R138 68 kΩ, a charge pulse from Q38 (C54 0.047 µF,
  R148 10 kΩ, R151 22 kΩ, D37, R152 100 Ω), 4066 switches IC12 on the decay/accent lines. VEG parts: Q31/Q32,
  C42 1 µF, R123 1.5 MΩ, R131 220 kΩ, R133 2.2 kΩ (gate through D35); trigger through D36, R134 22 kΩ, C41
  0.1 µF.
- **Time constants (calculated).** "T" is time to 10 %, so τ = T / ln 10: 1.09 s at maximum and 87 ms at
  minimum. That matches 1 µF × (1 MΩ pot + 68 kΩ) = 1.07 s and 68 kΩ × 1 µF = 68 ms plus switch and source
  resistance; accent shorting VR6 leaves R138, i.e. "accent = minimum decay". VEG: 1.5 MΩ × 1 µF = 1.5 s,
  consistent with an audible 3-4 s (to near silence). The MEG charge pulse (C54 with ≈ 22 kΩ) lasts about 1 ms:
  the "sharp attack" (unverified).
- **Open303 for comparison** [O303]: MEG ("mainEnv") a pure exponential with τ in ms, normal 1000, accent 200,
  knob 200-2000 ms; VEG ("ampEnv") decay 1230 ms, release 1 ms normal / 50 ms accent, a 200 Hz 12 dB
  "declicker"; normalAttack stored as 3 ms but the attack integrator has τ = 0. **(inferred)** Open303 treats
  200-2000 ms as an e-folding τ; if the service-note "T" is time to 10 %, Open303's decays are about 2.3 × too
  slow.

### 1.7 Accent: the VCA path and the sweep circuit

- **What an accent does** [DF-U]: the MEG decay switches to the minimum; the MEG voltage passes a switch (on
  only during accents) and the ACCENT pot, then (1) adds to the VCA control current through a 47 kΩ /
  0.033 µF network that slightly softens the attack (Whittle: the main reason accented notes are louder), and
  (2) raises the cutoff through the **accent sweep circuit**. Accent is not on CV OUT [OM p79].
- **Accent sweep circuit** [DF-U]: a diode in series with 47 kΩ drives one end of a pot; a 1 µF capacitor to
  ground hangs off the other end; the wiper feeds a filter-frequency summing node through a 100 kΩ mixing
  resistor. **That pot is the second gang of the RESONANCE pot.** Fully anticlockwise the filter gets about
  (100/147) × the MEG voltage minus the capacitor voltage (a near copy of the MEG); fully clockwise the pulse is
  smoothed by the capacitor charging through the diode and 147 kΩ. With repeated accents the capacitor has not
  discharged, so each sweep peaks higher; the effect grows with resonance.
- **Schematic** [SN p5]: C13 = 1 µF/50 V, R46 = 47 kΩ, D24, VR4 dual 50 kΩ (B). Whittle's text says "100k pot";
  the schematic and his own mods page say 50 kΩ [DF-M]. **(calculated)** Charge τ fully clockwise ≈ (47 k +
  50 k) × 1 µF ≈ 97 ms (147 ms with 100 kΩ); discharge through the pot and the 100 kΩ mixing resistor
  ≈ 100-150 ms; VCA accent smoothing τ = 47 k × 33 nF ≈ 1.6 ms.
- **Why it matters.** Whittle: the charge in C13 is "one of the keys to the emotional nature of the TB-303 -
  you poke it and it squeals a little ... you poke it again and it squeals even more" [DF p4]. In the stock
  ("normal") sweep, some charge remains on C13 when the next accent arrives, so the second and later accents
  go higher [DF p5].
- **(calculated)** At 120 BPM a 16th lasts 125 ms; with a discharge τ of 100-150 ms, 35-43 % of the previous
  charge remains at the next step, so a run of three or four accented 16ths climbs visibly before it
  saturates: the "wow" is a property of tempo, resonance and accent density together.
- Open303 models a "sloped accent" as a 15 ms leaky integrator fed with the MEG on accents, added in octaves to
  the cutoff; its peak normalisers are hard-set to 1.0 ("test") [O303]. **(inferred)** That is a smoothing,
  not the diode-RC accumulation through the resonance gang. [WP-de] claims Roland's TB-3 and TB-03 apply a
  static accent modulation instead of the accumulating one; single source, unverified.

### 1.8 Slide

- **Circuit.** The DAC's 100 kΩ source impedance into a 0.22 µF capacitor (C35), switched by IC12 (4066) and
  buffered by IC11b; without slide IC11b follows the DAC directly [DF p16, p32], [SN p5]. Stock slide time
  "60 ms", defined by Whittle as the approximate audible time to reach the final pitch [DF p4].
- **(calculated)** τ = 100 kΩ × 0.22 µF = 22 ms; 95 % in 3τ ≈ 66 ms. The RC is linear in volts and the CV is
  1 V/oct, so the glide is an exponential approach in log-frequency with the same τ for every interval:
  constant time, not constant rate ("a large interval takes as long as a small one" [HR secrets]).
- Open303 uses a 60 ms τ slew limiter [O303]; **(inferred)** about 3 × slower than the RC above.
- Whittle rejects the 1995 rumour that the sequencer anticipates the next note and starts gliding early [DF-S],
  [HR Haines].

### 1.9 VCA, output and noise

- VCA: BA662A OTA (the x0xb0x accepts BA662 or BA6110 [X0X fab/vca]), driven by the gated VEG and, on accents,
  by the MEG through the ACCENT pot and the 47 kΩ / 0.033 µF network [SN p3], [DF-U].
- **Bass.** In a stock unit 32 Hz is 5 dB down [DF p6]; Whittle blames C20 and C21 (0.01 µF), calling the
  stock bass "weedy", and fits 0.1 µF [DF-M §4.2]. **(calculated)** A single first-order high-pass that is
  −5 dB at 32 Hz has its corner at 47 Hz, close to Open303's 44.5 Hz pre-filter high-pass: two independent
  routes to the same number.
- Headphone amp: 5-6 Vpp, two 10 Ω resistors and a 1000 µF capacitor, switched on by inserting a 6.5 mm plug
  [DF p21-22]. CV OUT straight from an op-amp after the slide; GATE OUT through a diode and 1 kΩ to +12 V
  [DF p14-15].
- AudioRealism exposes "VCA Clicks" (attack click) and "VCA Noise" (gate on/off noise) as trims (ABL3 manual
  p12-13, https://www.audiorealism.se/uploads/4/5/5/4/45545237/abl3vstauoperationmanual.pdf); an emulation
  that omits the VCA's gate thump sounds "too clean" to users of the hardware (press quote on the TB-03's
  "no characterful VCA attack noise", elektronische-musik-produzieren.de above).

### 1.10 Controls: ranges and tapers

| Control | Part [SN p5, p8] | Taper | Range / behaviour |
|---|---|---|---|
| TUNING (VR2) | 50 kΩ B | linear | ±700 cents [SN p1] or ±500 [OM p89]; VCO only |
| CUTOFF FREQ (VR3) | 50 kΩ A, R47 10 kΩ, TM3 in series | audio (log) | ≈ 300 Hz-2.5 kHz resonant frequency, 500 Hz ± 25 % at centre (calibrated) |
| RESONANCE (VR4) | dual 50 kΩ B | linear (Whittle: 2006-09 replacements were "log at both ends") | 0 to just below oscillation; 2nd gang = accent sweep smoothing |
| ENV MOD (VR5) | 50 kΩ A, R61 10 kΩ, R63 220 kΩ | audio | never zero; raising it also lowers the base cutoff [SN p8] |
| DECAY (VR6) | 1 MΩ A | audio (Whittle lists it as linear, then corrected to log) | MEG 200 ms-2.5 s to 10 % [SN p5] (200 ms-2 s [DF p3]); bypassed on accents |
| ACCENT (VR7) | 50 kΩ B | linear | scales the MEG into the VCA and the sweep, accented notes only |
| VOLUME (VR8) | 50 kΩ A with power switch | audio | |
| TEMPO (VR1) | 1 MΩ (K161100FAE-1MC) | | 40-300 BPM; TM1 sets 8 ms ± 1 ms at TP1 with the knob fully clockwise (= 300 BPM × 24 ppq, calculated) [SN p6] |
| WAVEFORM (S1) | switch | | saw / square; the middle position of the switch gives silence [DF] |

Internal trimmers: TM1 tempo, TM2 internal clock (1.8 ms ± 0.2 ms at TP2), TM3 filter FREQ, TM4 TUNE, TM5 WIDTH
(V/oct), TM6 CV (the 5.333 V DAC reference) [SN p6-7], [HR `TB-303.internal-pots.txt`]. Part numbers and
replacement notes: [DF-M].

### 1.11 Unit-to-unit variation

- Per-unit trims: TM5 for 2:1, TM4 for 110 Hz, TM6 for 1.000 V ± 3 mV per octave, 4:1 ± 0.5 %; the DAC uses
  0.1 %-selected 200 kΩ resistors [SN p5-7]. Pitch error budget (calculated): ±3 mV/oct is ±3.6 cents per
  octave.
- Filter: calibrated to a 2 ms ± 0.5 ms ring, i.e. ±25 % of cutoff [SN p7]; the maximum filter frequency varied
  between units [DF p32]. No filter revisions over production (Anwander, above).
- Each CPU has its own minimum working voltage, which may rise with age [DF p19].
- **(inferred)** 5 % resistors and ±20 % electrolytics move the 10 Hz cluster of poles and zeros (§1.5.3) and
  the accent sweep time constants by similar amounts; 2SA733 β spread changes the square (§1.4). I found no
  documented early/late analogue revisions and no measured study of square-wave differences between units.
- Commercial emulations expose this as user trims: Roland's plug-in "Vintage Condition" ("age and condition
  of the virtual hardware") and VCF Trim; ABL3 Reso Trim, Gate Trim, Detune, Low Cut; Phoscyon 2 "Calibration
  Parameters" mirroring the service trimmers (§3).

### 1.12 Open questions that need a hardware or SPICE check

1. The Q8 square: duty cycle, droop and amplitude against pitch (§1.4). SPICE first, recordings second.
2. The reset shape of the ramp ("the tip isn't sharp").
3. The stock maximum loop gain (how close to k = 17), and the loop's coupling poles in Hz (§1.5.3).
4. The accent sweep's discharge path exactly (wiper, mixing resistor, summing-node impedance).
5. The Env Mod law (depth against the bias shift) in numbers: Open303's constants are the only published fit.
6. Gate timing in triplet mode, on a slide into a rest, and the latch point of step 1 relative to DIN start
   (§2.7).
7. The service manual's 110 Hz against the owner's manual's 220 Hz for the A key (§1.3).

---

## 2. The sequencer

### 2.1 Memory organisation
- 64 patterns: 4 groups (I-IV) × 2 sections (A, B) × 8 patterns, chosen with the group knob, A/B and a button
  1-8 [OM p11, p89]. Each pattern stores pitch, note length (timing), accent and slide [OM p89].
- 7 tracks of 64 measures; tracks combine up to 256 measures. A track stores the pattern order, the shift
  (transpose) per measure and D.C. (the end mark) [OM p64, p66, p89]. Track and group share one knob: tracks 1-2
  map to group I, 3-4 to II, 5-6 to III, 7 to IV (MAME driver input map, above; the TB-03 manual confirms tracks
  1-2 → group I). A track can only use the 16 patterns of its own group [OM p65].
- Three 1024 × 4 SRAMs ("1.5 k bytes") hold patterns, pre-scale values, pattern lengths and tracks
  [DF-MEM p4]: IC3 groups I-II, IC4 groups III-IV, IC5 the edit buffer and the tracks [DF-MEM].

### 2.2 Pattern Write: Pitch mode and Time mode
- **Pitch mode.** Press the one-octave keys (13 keys, C to C) in playing order; holding DOWN or UP writes the
  note an octave lower or higher; BACK steps back [OM p12-13, p20]. Each TAP plays the next stored pitch; tapping
  past the written pitches gives "a random selection of notes" [OM p13]. **Accent and slide are written in Pitch
  mode** (hold TAP on a note and press ACCENT or SLIDE; pressing again toggles) [OM p40-43], so they belong to
  the pitch list, not to the time steps. Rewriting pitches clears accents and slides; rewriting timing does not
  [OM p44, p46].
- **Time mode.** Each step is "sound begins" (note, •), "sound continues" (tie, ○) or rest (—) [OM p49-51]. A
  tie "increases the value of the previous note by one sixteenth note value" [OM p51]. No sound while writing;
  after the last step the unit returns to normal mode [OM p15]. A tie at the start of a pattern is rewritten as a
  note [OM p63]. On the TB-03 the three functions sit on DOWN, UP and ACCENT (TB-03 manual, "Entering the
  timing", https://static.roland.com/assets/media/pdf/TB-03_eng03_W.pdf).
- **Decoupling.** Time mode says only when a note starts, continues or rests; every note step plays the *next*
  pitch from the pitch list; a tie holds the previous note; a rest plays nothing [HR Perkowitz 1995]. Whittle
  (1997): "the pitch, accent and slide memory of each pattern seems to be 48 steps long — but only the first 16
  steps can have notes assigned"; setting the step count clears timing but not pitches, accents or slides;
  clearing a pattern "does not get rid of the notes (or accents or slides)" [HR Whittle 1997]. CLEAR/RESET
  after tap-writing clears the timing and keeps the pitches [OM p37].
- **The scramble (inferred from the above).** Keep `pitch[]` slots (note, up/down, accent, slide) apart from
  `time[]` steps (N/T/R). A pointer advances through `pitch[]` once per N step and returns to slot 0 at each
  pattern start. Then: more N steps than written pitches plays stale or uninitialised slots (the manual's
  "random" notes); adding or removing an N step shifts every later pitch *with its accent and slide* to another
  step; accents and slides "move" because they ride on pitch slots. Whether the pointer can pass slot 16 (the
  48-slot observation suggests it can) is unverified. The TB-03 manual on its original mode: it "requires
  practice ... it can result in some remarkable phrases being created by accident" (TB-03 manual, "NoDE").

### 2.3 Length, triplets, clear and tap write
- Length 1-16, set by holding FUNCTION and pressing the STEP key (key "9") N times; Pattern Clear resets to 16
  steps, normal mode [OM p11, p53-56], [HR Whittle 1997].
- **Triplet mode** (FUNCTION + key "0"): one beat = 3 steps (eighth-note triplets); a length over 12 becomes 12,
  the maximum is 15; setting 16 steps leaves triplet mode [OM p52-57, p89], [HR Whittle 1997]. A step is 6 clocks
  at 24 ppq, 8 in triplet mode; a 15-step triplet pattern lasts 5 quarter notes [HR Whittle 1997].
- Odd meters come from step counts (12 steps for 3/4; 5/4 split over two chained patterns); one pattern has one
  step mode, so mixing straight and triplet needs separate patterns [OM p50, p57, p61-62].
- **Tap write.** Run the pattern, press CLEAR/RESET, tap the rhythm against a metronome (eighths in normal
  mode, quarters in triplet mode, lower click on step 1). Holding any of keys 1-8 while tapping writes ties
  (sustain); otherwise the note sounds only while TAP is held; small errors are quantised; tap writing with
  sustain never writes rests [OM p36-37, p55, p63].

### 2.4 Pattern play, transpose and chaining
- A new pattern starts at the next measure; a transpose (hold PITCH MODE, press a key) also applies from the
  next measure; changing pattern removes the shift; the group cannot change while playing [OM p47-48].
- **Chaining**: press two keys in the same block (1-4 or 5-8); the range plays in ascending order. Valid: 12,
  123, 1234, 23, 234, 34, 56, 567, 5678, 67, 678, 78; no high-to-low, no skipping, no crossing blocks; a pattern
  change during a chain waits for the chain to end [OM p59-60], [HR Whittle 1997].

### 2.5 Track write and play
- Select the track, BAR RESET, RUN; per measure select a pattern, optionally shift (PITCH MODE + key), press
  WRITE/NEXT; on the last measure press D.C. before WRITE/NEXT [OM p23-28]. Shift is the interval of the
  pressed key above C, 0 to +12, upward only [OM p27]. Rewriting replaces measure by measure; measures after D.C.
  are unused [OM p64].
- D.S./segno (TAP during play jumps to the segno measure), measure insert and delete, play from a measure,
  turning the track knob during play finishes to D.C. and moves on, combined tracks continue into the next
  [OM p66-73].

### 2.6 Clock, tempo and DIN Sync
- Internal tempo 40-300 BPM [OM p89]. DIN Sync **in** only; inserting the plug opens a switch that disables
  the internal clock; a half-inserted plug unofficially works as sync out (DIN sync article above; Taylor
  Deupree 1996, http://machines.hyperreal.org/manufacturers/Roland/TB-303/info/roland.TB-303.sync.out.txt).
- DIN Sync: pin 1 start/stop (0 V stop, +5 V run), pin 2 ground, pin 3 clock (rising edge = tick), pins 4-5
  optional; Roland uses 24 ppq; a sender resets on the start edge and starts clocking about 9 ms later
  (https://en.wikipedia.org/wiki/DIN_sync). The OM says to press CLEAR/RESET before a synced performance and that
  Stop/Continue of the CSQ-600 or MC-4 "cannot synchronize correctly" [OM p75-78].
- The latch point of step 1 relative to the start edge and the first clock is undocumented (open question).

### 2.7 Gate, slide, accent and tie in time (the core)

**Whittle's measurement** (31 May 1999) [DF-S]:
- Clocks 0-5 make one 16th. The gate rises on the rising edge of clock 0 and falls "half-way through clock
  pulse 3 — on its negative edge": **3.5 clocks on, 2.5 off** (58 % of a step).
- On a **slid** note the gate does not fall; it stays high into the next note (no new gate edge), while the DAC
  CV changes to the next pitch at the step boundary.
- Slide rule 1: the slide switch goes on one 16th after the start of the slid note (the start of the next note
  when the slid note is one step long). Rule 2: it stays on until the end of the note *following* the slid
  note, however many 16ths that note lasts.
- In June 2012 Whittle added "Perhaps some of what I wrote below is wrong" after a Synth-DIY discussion and
  planned a review; it remains the best published measurement.

**The manual** [OM p79, p44, p63]: the gate time "is set a little shorter than the length of the note on the
score" (also in triplet mode); with slide the gate is continuous. Slide is written on the first of the two
notes; with a rest between them it does not work; a slide between two notes of the same pitch gives a "TIED"
note; slide works across patterns (a repeating pattern ending in a slide slides into its own first note; if the
next pattern starts with a rest, no slide).

**Envelopes**: the slid-into note does not retrigger the envelopes, because there is no gate edge
[HR Perkowitz]; when the gate falls the level drops quickly except on accented notes, where the MEG also feeds
the VCA [DF-S]. The Devil Fish latches the accent state: if the sequencer stops on an accented note, accent
stays on [DF p14].

**Other measured or documented gate lengths**: x0xb0x firmware: gate high for 3 of 6 clocks (50 %), the
half-step handler sets the slide line instead of dropping the gate, and "slide is from prev note!" [X0X
firmware `main.c`, `synth.c`]; Open303's AcidSequencer: gate 0.5 of a 16th, no note-off scheduled when the next
step is gated and the current one slides [O303]; JC303 "the authentic 303 half-step (50 %)"
(https://raw.githubusercontent.com/midilab/jc303/main/README.md); "antto" on KVR: 0.5 step, 1.5 for one slide,
2.5 for two (https://www.kvraudio.com/forum/viewtopic.php?t=262829, page 1). The Cyclone TT-303 always leaves a
short gap between notes ("1/64th quarter note, I think", https://www.firstpr.com.au/rwi/dfish/TT-303/).

Timing diagram (24 ppq, one step = clocks c0-c5; ↑0 = rising edge of c0; ↓3.5 = falling edge in c3):

```
                 step n                    step n+1                  step n+2
clock            0  1  2  3  4  5 | 0  1  2  3  4  5 | 0  1  2  3  4  5
N  N  N          ↑0 gate ↓3.5     | ↑0 gate ↓3.5      | ↑0 gate ↓3.5            [DF-S]
CV               pitch A          | pitch B (DAC at c0) | pitch C              [DF-S]
N/sl N  N        ↑0 gate ───────────────── high ─────── ↓3.5 (of n+1) | ↑0     [DF-S]
  slide switch   off              | on for all of note n+1             | off   [DF-S rules 1, 2]
  envelopes      trigger          | no retrigger (no edge)             | trigger
N  T  N          ↑0 gate high through the boundary ... ↓3.5 of n+1 = 9.5 clk  (inferred)
N  R  N          ↑0 ↓3.5          | low, no CV change   | ↑0                   (inferred)
triplet          8 clocks per step; gate fraction undocumented                (open)
```

Inferences from the sources above **(inferred)**:
- **Tie.** A note plus one tie should gate for 6 + 3.5 = 9.5 clocks (extrapolated, not measured); rule 2 shows a
  tied note is one note event.
- **Slid note followed by ties.** The slide line rises only on the tie step; Whittle says it makes no audible
  difference, the glide happens when the next N arrives.
- **Slide into a rest.** "Does not work" [OM]; the exact gate-off time is unmeasured. The x0xb0x holds the gate
  until the middle of the rest (≈ 1.5 steps); Open303 drops it at the normal length. Pick one, document it,
  make it measurable (§9).
- **Accent on a slid-into note.** No gate edge, so the MEG does not restart; an accent flag on the destination
  can only change the VCA level and accent path part-way through. Open303's `slideToNote()` does exactly this
  [O303]; unverified on hardware.
- **No swing** on the original (none in the OM or specs); Whittle states Roland's sequencers play "in perfect
  time with the clock pulses" [DF-S]. Shuffle appears on the TB-03 (−50 to +50) and in Roland's plug-in.
- **Where the "groove" comes from**: the 58 % gate, legato and slide merging steps, stacked accent sweeps,
  the fixed-time RC glide (which completes in the first half of the next 16th at 120 BPM and fails to complete
  above ≈ 200 BPM, calculated: 66 ms against a 75 ms step), and the pitch/time misalignment that puts accents in
  "wrong" places.

### 2.8 Quirks people love, and the "random patterns" legend
- Accent build-up (§1.7), the 60 ms glide, the 58 % gate, ties as one envelope, and the scramble (§2.2).
- **Battery loss.** A 100 µF capacitor holds the RAM supply for "a few days, and perhaps a week"; below roughly
  2 V the six-transistor SRAM cells lose state and power up in states set by device physics, "which we think
  of as random"; the result differs by RAM brand (NEC, Mitsubishi, Toshiba) [DF-MEM p4-5]. A 1995 user report:
  many 303s keep memory "up to a week", most "at least a minute" [HR `303.memory.txt`]. **(inferred)** An
  emulation of the legend should fill the *raw* memory (pitch slots, flags, time list, step count, pre-scale)
  with pseudo-random bits and decode them through the normal format, rather than generate musical patterns.

### 2.9 Data model for AT303 (inferred)
- Per pattern: `pitchSlots[48]` (semitone 0-12, up, down, accent, slide), `time[16]` (N/T/R), `length` 1-16
  (≤ 15 in triplet), `prescale` 6 or 8 clocks. A run-time **render** turns this into a list of note events
  (`start clock, length in clocks, pitch, accent, slide-in, slide-out`) that the modern grid shows and edits
  (§5.1). Editing in the grid writes back into both lists so the classic view stays consistent; a "classic
  quirks" switch decides whether grid edits re-flow the pitch list (scramble) or not.
- Clock: everything derives from a 24 ppq clock (internal, host or MIDI clock); gate fraction 3.5/6 by default,
  adjustable (§5.3).
- The AT101V already stores steps as note/rest/tie/accent/slide/probability and has a step editor, random,
  mutate, rotate and `.mid` export (`plugins/at101v/Source/dsp/ArpSeq.h`, root README): reuse the editor code
  and the JSON step text ("C2 acc", "-", "G2 slide"), but not its 1-list model, which cannot express the
  303's pitch/time split.

---

## 3. What existing products do, and what users love and hate

Tags: **[recurring]** = the same point in three or more independent sources; **[1 post]** = a single user;
"press" = a published review. Forum sweeps were not possible (see the conventions); retailer ratings and KVR
reviews stand in for them.

### 3.1 Roland TB-3 (2014), TB-03 (2016/17) and the TB-303 plug-in
- **TB-3 Touch Bassline** (2014): touchpad interface, MIDI and USB [WP]; modelled digitally ([WP-de] says on
  an FPGA).
- **TB-03** (Boutique; en.wikipedia says 2017, de.wikipedia 2016): ACB ("Analog Circuit Behavior") modelling,
  original layout; adds a 4-digit display, overdrive, delay/reverb types, MIDI and USB MIDI/audio, fine tempo,
  shuffle, step write, trigger in, CV/gate out, 96 patterns (section C = A + B), CV −1 to +7 V
  (https://www.roland.com/global/products/tb-03/; TB-03 manual). Firmware 1.04 adds Pattern Copy, Random Pattern
  1 (a new pattern) and Random Pattern 2 (randomises octave, slide and accent, keeps the keys)
  (https://static.roland.com/assets/media/pdf/TB-03_AddFunc_e01_W.pdf). Press: weakest in a German round-up,
  for "digital-sounding resonance", no characterful VCA attack noise and a "tinny, annoying" overdrive
  (elektronische-musik-produzieren.de, above).
- **TB-303 Software Bass Line** (Roland Cloud): VST3/AU/AAX, ACB; hidden panel with VCF Trim, "Vintage
  Condition" and master tune 430-450 Hz; 16 steps × 8 variations, graphic editor C0-C6, shuffle, scale 1/8T,
  1/16T, 1/16, 1/32; play modes forward, reverse, forward-and-reverse, invert, random; RANDOMIZE GENERATE with
  degrees for accent, slide, pitch and gate plus key range, root and scale; MODIFY randomises accent, slide and
  octave on gated steps "so the rhythmic feel and key ... are preserved"; MIDI and audio drag to the DAW;
  "position locking to DAW"; drive (overdrive, distortion, metal, fuzz, drive, compressor) and delay/modulation
  sections; needs an internet connection for authentication (https://www.roland.com/global/products/rc_tb-303/;
  manual `TB-303_Software_Bass_Line_eng03_W.pdf`). KVR lists it at $149 with zero user reviews
  (https://www.kvraudio.com/product/tb-303-by-roland).

### 3.2 Behringer TD-3, TD-3-MO, TD-3-AM
- **TD-3** (2019): analogue VCO/VCF/VCA, 16-step sequencer with 250 pattern slots, distortion, poly-chain up to
  16 units, USB-MIDI, MIDI in/out, sync and CV/gate, arpeggiator; about €83 ex VAT on Thomann's Argentine store,
  about €120 in the German round-up (https://www.thomannmusic.com/es-ar/behringer_td_3_sr.htm). Buyers: 4.6/5
  from 498 ratings; it follows the original programming method and the SynthTribe software makes patterns much
  easier; the original's limitations are challenging **[recurring, few posts]**; low output level **[1 post]**.
  Press: excellent value, resonance nearly self-oscillates, "hollow pulse wave", clones the quirky sequencer;
  the distortion is less successful (German round-up). The TD-3 manual could not be retrieved (behringer.com
  404), so its random pattern generator is unverified.
- **TD-3-MO** ("modded out"): soft attack, normal decay, accent decay, filter tracking, slide time, filter FM,
  sub-oscillator, accent sweep (3 steps), sweep speed (3 steps), muffler (2 steps), manual accent button,
  cutoff by MIDI CC; about €125 ex VAT, 4.6/5 from 93 ratings; buyers: patching takes it well beyond the TD-3; the
  sequencer is cumbersome, "wish it had a more modern design" **[recurring, 2+ posts]**
  (https://www.thomannmusic.com/es-ar/behringer_td_3_mo_sr.htm). Whittle calls it "an unauthorised copy of the
  Devil Fish circuitry" with "no Filter Frequency MIDI control" (https://www.firstpr.com.au/rwi/dfish/), which
  conflicts with the retailer's "cutoff via MIDI CC" (unresolved).
- **TD-3-AM**: not verified (retailer returned HTTP 429).

### 3.3 AudioRealism ABL3 (ABL 2003, ABL2 2007)
- $95 (KVR); AU/VST2/VST3, Apple Silicon; offline activation; a Reason Rack Extension version
  (https://www.kvraudio.com/product/audiorealism-bass-line-3-abl3-by-audiorealism; https://www.audiorealism.se/).
  "A complete rewrite ... with the single goal of making the most authentic 303 emulation possible" (ABL3 manual).
- **Model**: "analog modeling techniques", no samples; three VCO models (1 clean, "more similar to certain
  TB-303 clones"; 2 default, "most authentic ... a bit of grit and dirt"; 3 less dirty, less bottom); knob
  response "scaled to match the original 303 precisely" (ABL3 manual; product page).
- **Setup tweakables**: Reso Trim, Gate Trim (VCA gate release, default 50 % matched to a 303), Detune (per
  octave), Low Cut (matched at 50 %), VCA Clicks, VCA Noise, Vibrato (Up + Down on a step) (manual p12-13).
- **Randomizer**: user scale with octaves ±1/±2 and root; note "diversity" (0 % = root only, 100 % = all scale
  notes equally); gate, slide and accent density; next/previous seed; Fix Scale (scale notes in order =
  arpeggios); live Apply; Random (pitch, down, up, slide, accent) and Alter (permutes step order). Note its
  **slide convention**: Slide "will tie the step to the *previous* step", the opposite of the 303's.
- **Other**: Wave Analyzer (transcribes 303 audio into patterns); imports ReBirth, technoBox2, T-8, TB-03, TD-3
  patterns; `.mid` export with the caveat that it "may not always create a pattern that sounds identical to the
  internal pattern sequencer" and pads to 16 steps; MIDI Note mode (accent at velocity > 100, slide by
  overlapping notes, no pitch bend) and Pattern mode (a key selects one of 128 patterns); Host Sync (follows
  the transport, including scrubbing) or MIDI Trigger (plays only while a note is held).
- **Users** (KVR, 9 reviews, 4.00; only one about ABL3): accuracy is the dominant praise **[recurring, 7 of
  9]**: "most accurate softsynth 303", most listeners failed A/B tests (2011); "indistinguishable ... in a blind
  test" (2008); a real 303 is fuller through high-end analogue gear **[1 post]**. Built-in distortion "good
  but external is better" **[recurring, 4 posts 2003-2023]**. Sequencer ReBirth-like, could be more accessible
  **[recurring, 3 posts]**. MIDI maps not saved in Logic 8 **[1 post]**. A purist wanted no built-in effects
  and simulated low-battery and failing-component modes **[1 post]**
  (https://www.kvraudio.com/product/audiorealism-bass-line-3-abl3-by-audiorealism/reviews and ABL2 reviews).
- Press: ABL3 against Phoscyon 2, "very similar but not identical, roughly like two real TB-303 units"; ABL3
  slightly duller; calibration aligns them (https://www.buenasideas.de/test/testbericht-d16-group-phoscyon-2-die-letzte-303).
  ABL "more subtle, Plastikman-style", Phoscyon 2 more aggressive (https://www.gearnews.com/d16-phoscyon-2-review).

### 3.4 D16 Phoscyon (2006) and Phoscyon 2 (2022)
- Phoscyon 1: €59 (2010 review), arpeggiator, distortion, 96-128 patterns, randomizer; "to my ears it sounds
  very similar to a real TB-303", pattern controls unintuitive at first (http://rekkerd.org/review-d16-group-phoscyon);
  "D16 have nailed the wave shapes and filter" **[1 post]** (KVR).
- Phoscyon 2: €119 (intro €89 until 29 Sept 2022, upgrade €29); VST2/VST3/AU/AAX, no standalone; v2.1.0
  (https://d16.pl/phoscyon2). "Calibration Parameters" exposing service trims (pulse width, VCA noise, VCA
  click); extended controls: slide time, sweep amount, envelope attack, accent decay, accent volume, two-knob
  vibrato; 19 distortion models; compressor; five reorderable inserts; play modes EXT (MIDI), SEQ (classic
  303-limited mode or a piano-roll of 1-64 steps with lanes for notes, slides, accents, vibrato and octave,
  shuffle, transpose, shift), ARP (template riffs); "music roulette" randomizer with densities; patterns by
  MIDI note, CC or GUI; drag to DAW. Slide uses the 303 convention ("the current step will slide to the next
  set Note") (Phoscyon 2 manual).
- **Press**: the sequencer is the headline **[recurring, every review]**: "can now be programmed easily and
  comprehensibly", "really fun" (synthmania.de), "a joy to work with" (Gearnews), "intuitive" (MusicTech);
  "really does sound indistinguishable from an original 303" (Computer Music, quoted by D16); CPU-friendly on
  an i7-4790K at highest quality (buenasideas). **Complaints**: randomizer has scales but no root; arpeggiator
  cannot randomize or export; GUI cut off on small laptop screens (Gearnews/Bonedo); distortion fixed first in
  the chain (MusicTech); no hardware pattern exchange; English-only manual (buenasideas). KVR users 5.00;
  one uses the sequencer as a MIDI pattern generator for other plugins **[1 post]**.

### 3.5 Arturia Acid V (2023)
- $149, in V Collection 11 Pro; "modeled down to component-level detail" (TAE); hidden panel for drift and
  noise; sub-oscillator, vibrato to audio rate, three drawable function generators, 14 distortions, 4 FX slots,
  MTS-ESP; 64-step sequencer with polymetric lanes (accent, slide, vibrato at different lengths),
  "Transmutation" one-click generation, 350+ sequence presets, MIDI drag/bounce
  (https://www.arturia.com/products/software-instruments/acid-v/overview). KVR 5.00 from 1 review **[1 post]**.

### 3.6 Hardware clones and DIY
- **x0xb0x** (Adafruit, mid-2000s): near-exact analogue clone; files under **MIT**, the name an Adafruit
  trademark [X0X download page, LICENSE]. 128 patterns (16 banks × 8, EEPROM), no separate time mode (longer notes
  by repeating and sliding: "Essentially, this is how the original TB-303 did it"); byte format 6-bit pitch,
  bit 6 accent, bit 7 slide, 0 = rest, 0xFF = end; rests can carry accent and slide; chains of up to 32
  patterns in any order; transpose ±1 octave; DIN and MIDI clock in and out; tap tempo 20-200 BPM; keyboard
  mode; MIDI play mode (velocity > 100 = accent); a "c0ntr0l" computer app [X0X firmware and pages].
- **Din Sync RE-303** (2015, DIY): replaces the unobtainable BA662 with discrete parts; "indistinguishable" when
  built with correct parts [WP-de]; about €1,000 built, called the most accurate replica (German round-up).
  The Sonic Potions RE-303 CPU is a pin-compatible replacement for the D650C whose timing quirks were
  "replicated from measurements on vintage Roland machines" (https://www.sonic-potions.com/re303).
- **Cyclone Analogic TT-303 Bass Bot**: 224 patterns plus 224 "Bot" random patterns, up to 64 notes, mutate,
  arpeggiator, MIDI notes with velocity accent and overlap slide, no DIN Sync receive
  (https://www.firstpr.com.au/rwi/dfish/TT-303/); MK2 adds adjustable gate and slide length, manual accent,
  shuffle, "smart random"; resonance less sharp than the TD-3 (German round-up).
- **Others**: Acidlab Bassline3 €490 and M303 modular voice €290 (https://www.acidlab.de/); ML-303 (original
  parts); MAM MB-33 "too clean"; Abstrakt Avalon (copies the board layout, last run 2022) [WP-de]; Aciduino
  (MIT, Arduino 303/808 sequencer with one-button generation and a mode-constrained harmonizer,
  https://github.com/midilab/aciduino).

### 3.7 Devil Fish (Robin Whittle, since 1993; v2.0 1996)
The best-documented extension of the 303 and the reference for an "extended" page. Core mod AUD 1,236; MIDI
In AUD 412 (https://www.firstpr.com.au/rwi/dfish/prices-int/); as of April 2026 Whittle takes no new work.
Features with numbers [DF], [DF-U]:
- **Overdrive**: oscillator level into the filter from 0 to 66.6 × normal.
- **Slide time**: 60-360 ms from the sequencer, 2-300 ms from CV (stock 60 ms).
- **Soft attack**: 0.3-30 ms on non-accented notes (stock ≈ 4 ms delay + 3 ms attack).
- **Decay** pot moved to the VEG: 16 ms-3 s over the first half, a sustain level over the second half.
- **Normal decay / accent decay**: two independent MEG times, each 30 ms-3 s.
- **Accent sweep** switch (disables accent → filter/VCA) and **sweep speed** (fast: first accent strong, later
  smaller; normal: stock accumulation; slow: longer τ, can rise twice as high and cools slower).
- **Hi-resonance**: doubles the feedback; self-oscillation at mid and high frequencies.
- **Filter tracking**: 0 to over-tracking, up to ≈ 2.7 kHz/V, zero point near C/D at the bottom of the normal
  octave; low notes can push the cutoff below the fundamental.
- **Filter FM**: the VCA output (after the muffler) modulates the cutoff, stronger on accents, "approaches
  chaos" at high settings.
- **Muffler**: soft clipping after the VCA, three positions, "softens the loudest extremes ... whilst allowing the
  bass to pass largely unaffected".
- **Other**: manual accent button; cutoff range to ≈ 5 kHz resonant maximum; Env Mod tripled and able to reach
  zero; 32 Hz −1 dB instead of −5 dB; notes start within 0.5 ms; logarithmic note-end decay; CV/gate in, CV in
  for slide, accent and filter, accent out, audio in to the filter, filter FM in, filter out.
- The manual documents the knob positions that restrict a Devil Fish to stock-303 sounds [DF p7].
- Related CPU replacements: Quicksilver (MIDI and filter-frequency control, 2012-2017), Sonic Potions (MIDI
  in/out). JC303, Phoscyon 2 and the TD-3-MO all borrow this parameter set, which is evidence of demand
  **(inferred)**.

### 3.8 ReBirth RB-338 (Propellerhead, 1996/97)
Two 303s and an 808 (a 909 from v2), each with its own pattern selector, which the hardware lacked; development
stopped in January 1999, support in September 2005, then a free download; the iOS version (2010) was removed
from the App Store on 15 June 2017 **after Roland claimed IP infringement** (https://en.wikipedia.org/wiki/ReBirth_RB-338).
Its low CPU made it lead the PC market of its time. ABL3 imports ReBirth patterns, so its format is a de facto
interchange format **(inferred)**.

### 3.9 Open303 and JC303
- **Open303** (Robin Schmidt, 2009): "The Open303 source code is released under the terms of the MIT
  license", Copyright (c) 2009 Robin Schmidt [O303 `License.txt`]; on SourceForge since 2009, now on GitHub
  being converted to a GUI-less CLAP plug-in. Architecture [O303]: constant 4x oversampling; oscillator →
  44.486 Hz high-pass → TeeBeeFilter → 12th-order elliptic quarter-band anti-alias filter → decimation; then
  all-pass 14.008 Hz, high-pass 24.167 Hz, notch 7.5164 Hz, amp envelope. Oscillator: two mip-mapped
  2048-sample wavetables (SAW303, SQUARE303) cross-faded, not BLEP. Filter TB_303 mode: an explicit, Euler-like
  update structurally like [VAFD eq. 5.18], a unit-delayed feedback through the 150 Hz high-pass, **no
  nonlinearity** (the shaper call is commented out), coefficients from rational fits with k → 16.9988 at low
  frequency; the VST default program uses the LP_18 mode, not TB_303. Envelopes and accent as in §1.6-1.7.
  Note logic: velocity ≥ 100 = accent, legato = slide. A copy of Ooura's FFT (`fft4g.c`) has its header
  stripped; replace it with JUCE's FFT if any code is reused.
- **JC303** (midilab, Romulo Silva, 2023): **GPL-3.0**, the Open303 part stays MIT; JUCE/CMake, CLAP, VST3, LV2,
  AU, standalone; Devil Fish-inspired parameters ("not meant to be a clone"), decay 30-3000 ms; v0.12 overdrive
  from GuitarML/BYOD neural models (GPL-3); v0.13.0 adds a 16-step sequencer ported from Aciduino, internal /
  host / MIDI clock, record mode (velocity ≥ 100 = accent, overlap = slide, CC64 tap = rest), generative controls,
  a diode-ladder option "based on Will Pirkle's analysis" with K = 17 and a soft knee near 17.5, and an SH-101-style
  LFO; roadmap: pattern storage and presets (https://github.com/midilab/jc303, releases, README).
  **(inferred)** The closest free competitor. AT303 has to beat it on modelling depth (accent capacitor,
  coupling poles, envelope times, the Q8 square), on pattern management and on DAW workflow.

### 3.10 Generators and randomizers

| Product | Generator | Source |
|---|---|---|
| acid-banger (Vitling, CC BY 4.0, web) | random root (MIDI 16-30) and one of 8 interval sets (repeats weight the root); 16 steps; onset probability 0.6 on steps 0/4/8/12, 0.5 on other multiples of 3, 0.3 on other even steps, 0.1 on odd; accent 30 %, glide 10 % per sounding step | https://github.com/vitling/acid-banger `src/pattern.ts` |
| Open303 `randomize()` | uniform key 0-11, octave −2..+2, 50/50 accent, slide and gate; "permissible keys" quantise to the nearest allowed key | [O303] |
| ABL3 | scale, root, octaves, diversity, gate/slide/accent density, seeds, live apply, Alter | ABL3 manual |
| Phoscyon 2 | template of note names, octaves and attributes, densities for note, slide, accent, vibrato; no root | Phoscyon 2 manual; Gearnews |
| Roland plug-in | degrees for accent, slide, pitch, gate; key range, root, scale; MODIFY keeps gates and keys | Roland manual |
| TB-03 v1.04 | Random 1 (new), Random 2 (octave, slide, accent only) | TB-03 add-on |
| Acid V | "Transmutation" generate or alter; polymetric lanes | Arturia |
| JC303 | fill, accent, slide, tie, tones 1-12, range, harmony, shift, length | JC303 README |
| TT-303 MK2 | "smart random", 224 "Bot" patterns, mutate | firstpr TT-303; German round-up |

Max for Live acid generators were not researched (no search).

### 3.11 DAW and MIDI workflow
- **Convention everywhere**: accent = velocity ≥ 100 (ABL3 "over 100"; Open303, JC303, x0xb0x > 100); slide =
  overlapping notes, the second glides without retrigger (ABL3, Open303, JC303, TT-303); TAL-BassLine-101 also
  exports slides as overlapping notes (https://tal-software.com/products/tal-bassline-101); ABL3 ignores pitch
  bend.
- Pattern-per-key triggering (ABL3 Pattern mode, Phoscyon 2), host sync against MIDI trigger (ABL3), host,
  internal or MIDI clock (JC303), drag-to-DAW MIDI (ABL3, Phoscyon 2, Roland, Acid V).
- **Pain points documented**: lossy MIDI export (ABL3's own caveat); Phoscyon 2's arpeggiator cannot export; no
  hardware pattern exchange (Phoscyon 2); MIDI-learn maps not recalled (ABL2 in Logic 2014; Phoscyon 2.1.0's
  changelog fixes "MIDI CC assignment recall", stuck arpeggiator notes and an Ableton automation bug,
  https://d16.pl/pub/changelogs/Phoscyon%202%20-%20Changelist.txt); 2003 users asked for an easier programming
  mode and sequencer automation.
- **(inferred)** Every product fixes accent at a velocity threshold; none offers a continuous accent amount or a
  configurable threshold. The overlap convention is fragile: quantising or trimming note ends in a piano roll
  silently removes slides, and moving a note's end by one tick adds one. A dedicated, explicit slide signal
  (CC 65, the MIDI "portamento on/off" controller, or a per-note expression in hosts that have it) fixes that
  without breaking the convention.

### 3.12 Synthesis: what users love and complain about
**Love**:
1. Accents and resonance, the "squelch" and the accent "wow" **[recurring]**: the first criterion in ABL
   reviews; Whittle calls the accumulating sweep the emotional core [DF p4].
2. A modern sequencer that keeps the 303 feel **[recurring, every Phoscyon 2 review]**: lanes, 64 steps, odd
   lengths, drag to the DAW.
3. Constrained randomisation: scale, root, densities, keep-the-rhythm variations; reviewers ask for more
   constraint (Phoscyon's missing root).
4. Hidden trims, "your own 303" **[recurring]**: VCA click and noise, reso trim, gate trim, vintage condition.
5. Devil Fish-style extensions (TD-3-MO, Phoscyon 2, JC303).
6. Value (TD-3 4.6/5 over 498 ratings).

**Complaints**:
1. Hardware-style step programming **[recurring: TD-3 and TD-3-MO buyers, ABL2 reviews, Phoscyon 1 review]**.
2. Built-in distortion worse than outboard **[recurring, 4+ KVR posts over 20 years; press on TB-03 and TD-3]**.
3. Fixed effect order (Phoscyon 2).
4. Lossy MIDI round trip and MIDI-map recall bugs.
5. "Digital" resonance and missing accent dynamics on ACB hardware (press, [WP-de]; forums unchecked).
6. Online authentication (Roland Cloud); offline activation is advertised as a plus by AudioRealism and D16
   reviewers.
7. GUI that does not fit small screens (Phoscyon 2).

### 3.13 Gaps nobody fills, and what AT303 brings (inferred)
1. **A free model with depth and published measurements.** No product publishes measured comparisons; every
   accuracy claim found is subjective. AT303 can document each circuit behaviour of §1 with a test (§7) and a
   number in the README, as the other Audio Tools instruments do.
2. **The accent capacitor, the coupling poles, the Q8 square and the measured envelope times together.**
   Open303/JC303 do none of these from the circuit; the commercial products do not say.
3. **A lossless DAW representation** of a 303 pattern (explicit slide and accent, configurable threshold or
   continuous accent, round-trip `.mid`), plus the pitch/time view for people who want the original's
   happy accidents.
4. **A verified "stock" switch** next to the extended page, as the Devil Fish manual documents for its own
   hardware.
5. **A generator with every constraint in one place**: root, scale, range, positional weights, accent and slide
   probabilities, seed recall, lane locks, live apply, mutate.
6. **A drive stage designed as part of the instrument** (post-VCA soft clip, pre-filter overdrive, a pedal-style
   clipper) with a free order.
7. **Offline, free, open, small-screen friendly.**

### 3.14 Licences of the open references
- Open303: MIT. Code may be reused with the notice kept; its tuned constants are facts and can be cited.
- x0xb0x: MIT (files); "x0xb0x" is a trademark. The schematic and BOM are a usable open source of component
  values.
- JC303: GPL-3.0 (with MIT Open303 inside); combinable with AGPL-3.0 under GPLv3 §13 **(inferred)**, but
  AT303 should not need its code.
- acid-banger: CC BY 4.0; we use its published idea and numbers with attribution, not its code.
- Aciduino: MIT.
- Will Pirkle's RackAFX diode ladder project: "Copyright Tritone Systems Inc. 2006-2012", no open-source grant:
  do not copy. The Faust `ve.diodeLadder` (Eric Tarr, after Pirkle AN-6) is STK-4.3 (MIT-style)
  (https://raw.githubusercontent.com/grame-cncm/faustlibraries/master/vaeffects.lib).
- mystran's "cheap non-linear zero-delay filters" (KVR t=349859): permissive with attribution; karrikuh's diode
  ladder (KVR t=346155): MIT.
- JUCE modules: AGPLv3 or the commercial licence (https://raw.githubusercontent.com/juce-framework/JUCE/master/LICENSE.md).
- Roland's manuals, the service notes, Whittle's manuals and pages, Stinchcombe's papers and figures:
  copyrighted; cited, kept private, never committed. Circuit facts (component values, topologies, time
  constants) are not protected by copyright; the 1999 *Mackie v. Behringer* ruling that circuit schematics are
  not covered by copyright is reported in https://en.wikipedia.org/wiki/Behringer **(not legal advice)**.

---

## 4. DSP proposal (inferred throughout)

Everything in this section is a design proposal built on the facts of §1-§3; numbers are starting values for
the tests of §7. The engine is a JUCE-free header set under `plugins/at303/Source/dsp/`, like the other
instruments, reusing `shared/dsp/` (`Oversampling.h`, `Blep.h`, `Fx.h`, `Common.h`).

### 4.1 Signal flow and rates

```
sequencer / MIDI ──► note events (sample-exact) ──► DAC volts (+ unit scale error)
                                                     │
                                         slide RC (τ 22 ms, switched)  ──► CV OUT (shown on the panel)
                                                     │
   ┌──────────────── 4x oversampled ─────────────────▼──────────────────────────────────────────┐
   │ ramp core (BLEP/BLAMP, soft reset) ─► saw | Q8 square table ─► input coupling ─► × overdrive │
   │      ─► diode ladder (nonlinear TPT, k from RESONANCE, coupling HPs inside the loop)          │
   │      ─► output coupling (C25/C27) ─► OTA VCA (tanh, control feedthrough)                      │
   └───────────────────────────────────────────────────────────────────────────────────────────────┘
        ▲ cutoff exponent = CUTOFF law + ENV MOD (depth and bias) × MEG + accent sweep (+ tracking, filter FM)
        ▲ VCA control     = VEG (gated) + ACCENT × MEG (1.6 ms RC, accented notes only)
   base rate: output high-pass (C20/C21) ─► [soft clip] ─► [drive] ─► [delay] ─► [reverb] ─► volume
```

- Envelopes, the accent sweep circuit and the slide run once per base-rate sample and are linearly
  interpolated inside the oversampled block (they are smooth: the fastest corner is the 1.6 ms accent RC).
  Filter FM (extended page) is the exception and is computed per oversampled sample.
- Phase in double, everything else in float, as in AT101V and ATMD.
- A −90 dBFS thermal noise floor at the ladder input, so a ringing or (in the extended mode) oscillating filter
  has something to grow from (the AT101V and ATMD traps: "self-oscillation needs a seed").

### 4.2 Oscillator

- **Core.** A falling ramp (the polarity of [SN p5]) from a double-precision phase accumulator, frequency
  f = 65.406 Hz × 2^(V − 2) × 2^(tune/1200). The reset is not instantaneous ("the tip of the saw isn't sharp",
  [X0X]): model it as a linear rise over a reset time t_r, which is exactly two slope discontinuities; correct
  them with two PolyBLAMPs (`shared/dsp/Blep.h`) instead of one PolyBLEP. t_r comes from the SPICE run of the
  reset transistors (§4.10); until then 5 µs (a placeholder, not a measurement).
- **Square: a pitch-indexed table set from a circuit simulation.** The Q8 stage (§1.4) is a few components;
  its output is a deterministic function of the ramp's phase *and* its slope (i.e. the pitch), plus a slowly
  varying emitter bias. Plan:
  1. Simulate the stage in ngspice (BSD licence) with the [SN p5] values and a 2SA733 model, driven by the
     ramp core at every semitone from 21.8 Hz to 784 Hz (the full tuning range, 54 points), until the
     emitter bias settles; keep one cycle per pitch, normalised to the phase.
  2. Store each cycle as 2048 points; build per-octave band-limited mip-maps of each (as Open303 does for its
     static tables [O303]); at run time interpolate between the two nearest pitch tables and the two nearest
     mip levels. Slide moves the table index continuously, so the shape changes during a glide as on the
     hardware (if the hypothesis of §1.4 holds).
  3. Check the tables against the x0xb0x description (≈ 4 Vpp on 7 V, droop, not 50 %) and recordings.
  Fallback if the SPICE result shows no meaningful pitch dependence: a single static shaper like Open303's
  offset tanh, with BLEP/BLAMP corrections at its threshold crossings (the crossing time is solved from the
  phase in closed form).
- Memory: 54 pitches × 9 mip levels × 2048 floats ≈ 4 MB; halve it with 1024-point tables for the upper mip
  levels, or store 27 pitches (every whole tone) and interpolate.
- **Saw**: the ramp itself after the JFET buffer, normalised to ≈ 5.5-6.5 Vpp in the model's volt units (§4.3
  scales volts into the ladder's 2V_T units).
- **Pitch condition** (§4.8): DAC scale error (±3 mV/oct = ±3.6 cents/oct), tune offset, a very slow drift
  (≤ 0.5 cent, off by default).

### 4.3 The diode ladder

**Equations.** State y1..y4 in units of 2V_T at the ladder, input u, capacitor ratio r = C18/C = 0.545:
- ẏ1 = (ω_c / (2r)) · ( tanh(u) − tanh(y1 − y2) )
- ẏ2 = (ω_c / 2) · ( tanh(y1 − y2) − tanh(y2 − y3) )
- ẏ3 = (ω_c / 2) · ( tanh(y2 − y3) − tanh(y3 − y4) )
- ẏ4 = (ω_c / 2) · ( tanh(y3 − y4) − tanh(y4) )
- u = g_in · x − k · F(y4), where F is the feedback path including its coupling high-passes (below) and k the
  resonance gain.

This is [VAFD §6.9] with the first stage generalised to the real capacitor ratio (§1.5.2 showed r = 0.5 gives
Zavalishin's and Stinchcombe's TB-303 case exactly). In the small-signal limit it is the linear model that §7
tests against.

**Solver, default ("normal"): linearised TPT.** Replace each tanh(v) by v·τ(v_prev) with τ(v) = tanh(v)/v,
evaluated at the previous sample's arguments (mystran's "cheap non-linear zero-delay filters", KVR t=349859;
the same method as `Ir3109.h` in AT101V and `LadderMd.h` in ATMD, with which the repo has good experience).
Discretised with the trapezoidal rule, each sample is then a linear system: tridiagonal in y1..y4 plus the
feedback coupling from y4 to the input. Solve it in closed form, either with Zavalishin's nested
instantaneous-gain structure (g23, g12, g01, then the outer loop; [VAFD Fig. 5.52-5.53], which also proves the
loop is never instantaneously unstable for k ≥ −1 when 0 < g < 1/2) or with a Thomas solve plus a rank-one
correction for the feedback. Cost estimate: 70-90 flops and 5 tanh per oversampled sample.

**Solver, "accurate": Newton.** Two Newton iterations on the trapezoidal equations, starting from the
linearised solution; the Jacobian is tridiagonal plus one column, so each iteration is still O(4). Offline
render: up to 4 iterations at 8x, with a convergence test. Zavalishin lists the options (linearise at zero, at
the operating point = one Newton step, by the secant through the origin) [VAFD §6.6]; Civolani and Fontana's
VCS3 diode ladder never needed more than 2 fixed-point iterations at a 10 mV threshold
(DAFx-08, https://www.dafx.de/paper-archive/2008/papers/dafx08_06.pdf §5).

**Feedback, resonance and coupling capacitors.**
- k = k_max · law(RES) with a linear pot law (50 kΩ B) and **k_max ≈ 16.5** in stock mode, just below the k = 17
  threshold (§1.5.4): long rings, no sustained oscillation. k_max is a unit trim (ABL3's "Reso Trim"); the
  extended "high resonance" doubles k (Devil Fish) and the tanh terms limit the oscillation amplitude.
- The five high-pass sections of [TS-D2] become one-pole TPT high-passes at their places in the loop (ladder
  input C23; feedback C14 and C15; output C25/C27; the C17/C16 network), with corners computed from the
  component values (a table in the code, with the calculation). A TPT one-pole is affine in its input, so the
  per-sample loop stays solvable in closed form. Target: the full linearised model reproduces Stinchcombe's
  10-pole function (once its units are confirmed) and the low-frequency peak (§7, T3). If the circuit sections
  do not reproduce the weaker resonance at low cutoff that Open303 approximates with a 150 Hz feedback high-pass,
  record the discrepancy rather than fit a non-physical corner.
- No separate "bass compensation": the 303 does not compensate the 25 dB loss; it is part of the sound.

**Cutoff mapping (calculated).** Calibrate on the resonant frequency, as Roland did: CUTOFF at 0.5, RES max,
ENV MOD min → 500 Hz ring (2.0 ms period, [SN p7]). With a range of 300 Hz-2.4 kHz (§1.5.5), an exponential law
in u² puts 500 Hz at the middle: f = 300 Hz × 8^(u²) gives 300, 500 and 2400 Hz at u = 0, 0.5, 1 (the "A" taper).
ENV MOD (depth m, A taper) adds m·d·MEG octaves and subtracts m·b octaves of bias [SN p8]; start d and b from
Open303's measured mapping (313.8-2394.4 Hz bounds, MIT, cited) and refine against recordings. Stock Env Mod has
a non-zero minimum; the extended page allows zero and × 3 [DF p7].
- ω_c is prewarped with a fast tan at the oversampled rate; recomputed per oversampled sample only when filter
  FM or audio-rate modulation is active (the ATMD Windows trap: MSVC's `std::tan` per oversampled sample made
  Windows 3.3 × slower than the M4).
- **Settling** [DF p13]: the cutoff exponent passes a two-path lag, 99.8 % with τ ≈ 0.15 ms and 0.2 % with
  τ ≈ 150 ms (values fitted to "99.8 % in under 1 ms, then ... about 500 ms"). Audible only on rings, cheap.

**Levels.** Saw and square enter the ladder slightly into the tanh region (the bottom pair saturates on the
saw's reset edge first); g_in is set so that the default sound has ≈ 1-2 % THD at the filter input with the
filter open, then tuned by ear against recordings. The overdrive control of the extended page scales g_in by
1-66.6 × [DF].

### 4.4 Envelopes and the accent circuit

- **MEG**: a ≈ 1 ms charge pulse (C54, [SN p5]) to the peak, then an RC decay with τ = (68 kΩ + R_decay)·1 µF,
  R_decay = 1 MΩ × A-taper(DECAY): 68 ms-1.07 s (200 ms-2.5 s to 10 %). On accents R_decay = 0 (the 4066 shorts
  VR6). The MEG does not restart on a slid-into note (no gate edge).
- **VEG**: attack after a 4 ms delay with 3 ms rise on non-accented notes [DF p3]; decay τ = 1.5 s (R123·C42)
  while the gate is high; at gate-off 8 ms at level then 8 ms linear fall [DF p6]. Accented notes: the MEG path
  keeps the VCA open after gate-off (tail) [DF-S].
- **Accent into the VCA**: ACCENT (linear) × MEG, through the 47 kΩ/33 nF RC (τ 1.6 ms), added to the VEG's
  control current; only while the accent latch is on.
- **Accent sweep circuit**, solved as a circuit, not a smoothing filter: source v_a = ACCENT × MEG (accents only,
  else 0) → diode D24 (exponential, or an ideal diode with 0.6 V drop in the model's volt scale) → R46 47 kΩ →
  pot end P1; pot track 50 kΩ split by the RES position ρ: R_a = 50 kΩ·ρ from P1 to the wiper, R_b = 50 kΩ·(1−ρ)
  from the wiper to C13 (1 µF to ground); wiper → 100 kΩ → summing node (virtual ground). One state (V_C13),
  trapezoidal, one diode: two Newton steps or an on/off case split per sample. The wiper current (V_w/100 kΩ)
  adds to the cutoff exponent with a scale fixed by calibration (§4.10). It needs the MEG in volts to put the
  diode drop in the right place; take the MEG peak from the schematic's envelope voltages and confirm against
  recordings. Because the capacitor discharges only through the wiper path, runs of accents accumulate (§1.7).
- **Sweep speed** (extended): fast / normal / slow as C13 × 0.33 / 1 / 2.2 (slow "can rise twice as high and
  cools slower" [DF]); tune the factors by ear and the T8 accumulation test.
- **Dual decay, soft attack** (extended): MEG normal and accent decay each 30 ms-3 s; VEG decay mode 16 ms-3 s
  plus sustain; soft attack 0.3-30 ms on non-accented notes [DF].

### 4.5 Slide and pitch

- One-pole RC on the CV in volts, τ = 22 ms, switched by the sequencer's slide line (§2.7); constant time for
  every interval because the RC is in volts and the CV is 1 V/oct (§1.8). Extended: slide time 60-360 ms
  "audible" (τ = T/2.7).
- With the switch off the CV jumps (the op-amp follows the DAC directly); a 20 µs smoothing avoids a numerical
  step.
- CV OUT on the panel shows this signal; in MIDI mode, legato notes switch the slide on (§5.3).

### 4.6 VCA and output

- OTA: out = I_ctl · tanh(v_in / v_lin), with v_lin set so accented peaks at full resonance saturate gently.
- **Control feedthrough** ε · I_ctl added before the output high-pass: the gate "thump" that users miss in
  clean emulations (ABL3 exposes "VCA Clicks" and "VCA Noise", Phoscyon 2 "VCA click"); ε from recordings, a
  trim from 0 to 2 × nominal.
- Output high-pass at 47 Hz (first order, calculated from "32 Hz is 5 dB down" [DF p6]); the extended "bass
  fix" moves it to 4.7 Hz (C20/C21 → 0.1 µF [DF-M]).

### 4.7 Distortion and the extended page

- **Post-VCA soft clip** ("muffler", three positions [DF]): Whittle's description (loudest extremes softened,
  bass largely unaffected) suggests a level-dependent soft clip that mostly catches the square's buzz; first
  proposal: a diode-pair soft clip with antiderivative anti-aliasing (ADAA, first order; DAFx-16,
  https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf) at the base rate after the
  decimator, or before decimation if ADAA is not enough. Compare by ear with the hardware description.
- **Drive** (P1): three generic circuit types users reach for with a 303: an op-amp clipper with diodes in
  the feedback (smooth mid drive), a hard clipper (distortion) and an asymmetric fuzz. Each with input gain,
  tone and level, first-order ADAA tanh/clip at 2x (Parker et al. Table 1: rectangular-kernel ADAA needs 4x
  where plain clipping needs 12x for similar aliasing), placeable before or after the delay. No pedal brand
  names.
- **Filter FM**: VCA output (after the soft clip) into the cutoff exponent, depth from 0 up to the point where it
  "approaches chaos", stronger on accents because the VCA output is louder [DF]; per oversampled sample.
- **Filter tracking**: CV into the cutoff, 0 to over-tracking; Whittle gives up to 2.7 kHz/V with the zero point
  near C/D of the normal octave [DF]; implement as octaves per volt referenced to 2.1 V.
- **High resonance** (k × 2), **accent sweep off**, **manual accent**, **cutoff range to 5 kHz**, **Env Mod to
  zero and × 3**, **bass fix**, **note start within 0.5 ms** (removes the 4 ms delay): each one a single
  parameter of the model above.
- The extended page shows a "stock" button that sets every extension to its stock value (the Devil Fish
  manual's restriction to stock sounds [DF p7], exposed as one click).

### 4.8 Unit condition

A seeded "unit" (a serial number stored in the preset) draws: cutoff calibration within the ±25 % of TM3,
k_max within 16.0-16.9, tune scale within ±3 mV/oct, Q8 β (square shape) within the 2SA733 h_FE class,
electrolytics ±20 % (coupling poles, C13, C62, C42), VEG/MEG ±10 %, VCA feedthrough. Unit 0 is the nominal
schematic. This is what Roland calls "Vintage Condition" and AudioRealism/D16 expose as trims (§1.11, §3), but
reproducible and documented.

### 4.9 Oversampling and CPU

- 4x by default with the shared half-band chain; 2x ("eco"), 8x ("high") and an offline override, as in ATMD.
  The ladder and the soft-reset ramp need the oversampling; envelopes and the sweep circuit do not.
- **Targets**, percentage of one core at 48 kHz, 512-sample blocks, measured with `at303render --bench` on
  the machines of the root README:

| Setting | Apple M4 | Ryzen 9 5900XT | Basis |
|---|---|---|---|
| Default sound, 4x, linearised TPT | ≤ 1.0 % | ≤ 2.0 % | AT101V mono 4x 1.0 / 1.8 %; ATMD one voice 4x 0.85 / 1.54 % (root README) |
| "Accurate" (Newton, 2 iterations), 4x | ≤ 2.0 % | ≤ 4.0 % | ≈ 2 × the ladder cost |
| Everything on (extended page, drive, delay, reverb), 4x | ≤ 3.0 % | ≤ 6.0 % | AT101V with FX |
| 8x, accurate | ≤ 4.0 % | ≤ 8.0 % | |

- Traps to avoid from the repo: `std::tan` on Windows (ATMD), denormals in the coupling high-passes at silence
  (flush to zero, plus the −90 dB noise floor), filter coefficients recomputed per oversampled sample when
  nothing modulates them.

### 4.10 Verifying against the hardware

Usable targets, by kind:
- **Calibration facts** (no licence issue; facts): 110 Hz on A at 2.75 V, 1 V/oct ± 3 mV, the 2 ms ± 0.5 ms ring,
  MEG 200 ms / 2.5 s to 10 %, slide τ 22 ms, VEG 1.5 s τ, accent sweep RC values [SN p5-8]; 60 ms slide, 3-4 s VEG,
  4 + 3 ms attack, 16 ms gate end, 32 Hz −5 dB [DF p3-6]; gate 3.5/6 clocks [DF-S].
- **Analyses**: Stinchcombe's transfer functions and Zavalishin's k = 17 are equations (facts); his SPICE plots
  are copyrighted figures: compare numerically, never publish them.
- **Our own SPICE references** (ngspice, from the x0xb0x MIT schematic and the service-note values): the Q8
  square per pitch, the ladder with its coupling sections (linear sweeps and rings), the accent sweep circuit
  with runs of 1-8 accents at 100/120/140 BPM. These are our work; commit them as CSV references for the tests.
- **Recordings**:
  - Wikimedia Commons, category "Roland TB-303": "The Sound of Roland TB-303 for Wikipedia" (2:08, CC BY-SA 4.0,
    user "I speak so quietly", 2014); "Two Simple 303 Patterns" (24 s, CC BY-SA 3.0, Djnebula1, 2011);
    "Overdriven TB303 Patterns varying resonance" (39 s, CC BY-SA 3.0, Djnebula1, 2011)
    (https://commons.wikimedia.org/wiki/Category:Roland_TB-303). None states whether the source is a real TB-303,
    a clone or software. Usable for listening and private analysis; anything derived and committed must carry
    the attribution and share-alike terms, so keep them out of the repo.
  - Commercial sample packs usually forbid redistribution: private analysis only.
- **Best evidence: record a unit with a protocol** (decision §9): a real TB-303 if one can be borrowed, else an
  x0xb0x or a TD-3 (clones; differences to be documented). Protocol: (1) every note C1-C4, saw and square,
  filter open, Env Mod min, for the Q8 shape and the reset; (2) the calibration ring; (3) cutoff sweeps at three
  resonances; (4) Decay min and max; (5) runs of 1, 2, 4 and 8 accented 16ths at 100/120/140 BPM at RES 0, 50 %
  and 100 %, with the resonant frequency tracked over time (the accent accumulation); (6) slides of 1, 7, 12 and 24
  semitones; (7) gate and CV OUT captured at audio rate under a known DIN clock, including triplet mode and a
  slide into a rest (the open questions of §1.12 and §2.7). Record at 96 kHz, store privately, commit only the
  measured numbers.

---

## 5. Features beyond the original (inferred, prioritised)

P0 = version 1.0; P1 = 1.x; P2 = later or only if asked. The order inside each band is the order of value to
users, from the complaints and gaps of §3.12-3.13.

### 5.1 P0: two editors on one pattern
- **Classic entry**, on the instrument panel: Pitch mode (13 keys, UP/DOWN, ACCENT, SLIDE, BACK, TAP to audition)
  and Time mode (note/tie/rest), with the decoupled lists of §2.9 and their scramble. A "classic quirks" switch
  (on by default in this view) keeps the stale-slot behaviour; tap write with quantise and sustain.
- **Modern grid** on its own page: 16 steps × lanes (pitch on a one- or two-octave roll, octave up/down, accent,
  slide, tie), 1-16 steps and triplets as on the original, plus extended lengths (up to 64 steps, P1), per-step
  probability (P1) and per-lane lengths for polymeters (P2, Acid V). The grid edits the rendered note events; the
  classic lists are rebuilt so both views always agree. Keyboard shortcuts for everything (Phoscyon 2's lesson:
  the sequencer sells the instrument).
- Copy/paste, clear, rotate, transpose, reverse, invert pitches, "shift accents", undo. Reuse AT101V's step
  editor code where it fits.

### 5.2 P0: memory, chaining and tracks
- 4 groups × A/B × 8 = 64 patterns per bank, as on the original, stored in the preset; chaining with the original's
  rules (adjacent ranges in blocks of four) plus free chains of up to 32 in any order (x0xb0x) in the modern view.
- Track mode with 7 tracks, shift, D.C., segno and measure insert/delete (P1 for the full track editor; P0 for
  playback of tracks loaded from presets).
- Pattern switching at the next measure, as on the original, or immediately (option).

### 5.3 P0: clock, host sync and MIDI
- 24 ppq internally; sources: internal tempo (40-300 BPM, fine tempo), host transport (sample-accurate from the
  host's PPQ position, following loops and jumps; "position locking", as Roland's plug-in calls it), MIDI
  clock in the standalone.
- **MIDI modes** (ABL3's split is the right one):
  - *Play*: AT303 is a mono synth. Legato (overlap) → slide; accent from velocity with a **configurable
    threshold** (default ≥ 100, the convention) or **continuous** (velocity scales ACCENT); **CC 65** (portamento
    on/off) as an explicit slide flag that survives quantising; CC 64 sustain as a tie in record; pitch bend
    ±2 semitones (an extension; ABL3 ignores bend).
  - *Pattern*: a key in a chosen octave selects a pattern, the next key from C3 up transposes it (live transpose
    as on the original's PITCH MODE + key).
  - *Trigger*: the pattern plays only while a note is held, transposed by the note (ABL3's MIDI Trigger).
  - *Record*: play into the grid with the same conventions (JC303's record mode).
- **MIDI out** of every note the sequencer plays (where the host allows), with the same conventions, so the
  sequencer can drive other instruments (a Phoscyon 2 user does this **[1 post]**).
- **`.mid` export by drag and import**: non-accent velocity 90, accent 127, slides both as overlaps *and* CC 65,
  ties as long notes, rests as gaps; a text meta-event carrying the classic lists, so AT303 → AT303 is lossless
  and other hosts still get the conventions. Fixes ABL3's documented lossy export.

### 5.4 P0: acid generator
- Constraints in one panel: root, scale (with a "chromatic" option), range (1-3 octaves), note diversity
  (ABL3), onset density with **positional weights** (downbeats, offbeats, the 3-3-2 grid; acid-banger's weights as
  one preset curve), accent probability and placement bias, slide probability (only between sounding steps;
  bias towards small intervals and octave jumps), tie probability, rest density, motif length (4 or 8 steps
  repeated with variation), seed with previous/next (ABL3), live apply.
- **Lane locks**: keep rhythm, regenerate pitch; keep pitch, regenerate accents/slides/octaves (TB-03 Random 2,
  Roland MODIFY); "mutate" a percentage of steps each loop or on demand.
- Deterministic: same seed, same pattern, on every platform.
- **Battery loss** (P1, a joke with substance): fill the raw pattern memory with seeded noise and decode it
  through the pitch/time format (§2.8).

### 5.5 P0: the extended page and the condition page
- Extended ("classic mods", generic names; §4.7): overdrive, normal decay, accent decay, VEG decay with sustain,
  soft attack, slide time, accent sweep on/off and speed, high resonance, filter tracking, filter FM, post-VCA soft
  clip, Env Mod range, cutoff range, bass fix, fast note start, manual accent; a "stock" button.
- Condition (§4.8): unit seed, calibration trims (cutoff, max resonance, tuning scale, VCA click/noise, gate
  length, low cut), oversampling, voice drift.

### 5.6 P1
- Drive models with free placement, tempo-synced delay and plate reverb from `shared/dsp/Fx.h` (the series
  standard), a compressor-style output stage only if users ask (ABL3 reviewers prefer external effects).
- Swing (−50 to +50, as the TB-03) and gate length per pattern; per-step probability; extended lengths to 64.
- Pattern import from documented open formats (x0xb0x's byte format; acid-banger-style text) and from
  user-exportable hardware formats where the format is published; ReBirth and SynthTribe imports only if their
  formats are documented without reverse-engineering proprietary files (decision §9).
- MIDI learn with recall in the preset and the host session (the ABL2/Phoscyon recall bugs).

### 5.7 P2
- Two instances' worth in one ("dual" mode, the ReBirth idea): left to the host, unless users ask.
- Audio-to-pattern transcription (ABL3 Wave Analyzer).
- A sub-oscillator, LFO or vibrato lane (Acid V, Phoscyon 2, JC303): not 303 features; only on the extended page
  and only if requested.

---

## 6. Visual direction

### 6.1 What is safe and what is not (inferred; not legal advice)
- The series renders photoreal panels (AT20V, ATMD) and carries **no manufacturer logos**: ATMD's name board
  carries "ATMD / Audio Tools" (`plugins/atmd/docs/ui-assets.md`). AT303 follows the same rule and the brief's:
  no brand marks, no trade dress beyond a generic box.
- **Roland enforces IP against emulations**: ReBirth's iOS app was removed in 2017 "after Roland claimed IP
  infringement" (ReBirth article above); Roland settled a trade-dress and trademark suit with Behringer over guitar
  pedals in 2006 after Behringer changed the designs (https://en.wikipedia.org/wiki/Behringer). Behringer argues
  cloning expired-patent hardware is legal (Fact, 2017, quoted there); the TD-3's close copy of the layout is a
  business risk Behringer takes, not a precedent for us.
- **Avoid**: the original's model name and the "Bass Line" wordmark; "Computer Controlled" lettering; the
  maker's logo and typeface; the exact panel composition (knob row order and spacing above a single row of
  13 small piano buttons, the pattern-group selector, the silver top with its printed keyboard graphics); the
  original's colour scheme as a whole; screenshots or photos of the original anywhere in the UI or on the
  website.
- **Safe**: functional control names that are generic synthesiser vocabulary (Tuning, Cutoff, Resonance, Env
  Mod, Decay, Accent, Slide, Tempo, Volume); a one-octave key row (any keyboard is one); a 16-step row; the
  circuit behaviour itself; describing the product in text as "a recreation of the 1981 bass line synthesizer"
  and, in the README, naming the original for identification as the other plugins do (ATMD: "A recreation of
  the Moog Minimoog Model D"). The product name **AT303** uses the original's number, as AT101V and AT20V do;
  whether "303" alone is a risk is a decision for the user (§9).
- Reference photos for proportions only (not shipped, not committed): Wikimedia Commons has a CC0 panel photo
  ("Roland TB-303 Panel.jpg", Steve Sims, 2008, 1656 × 814,
  https://commons.wikimedia.org/wiki/File:Roland_TB-303_Panel.jpg) and others in the same category; since the
  design must *not* copy the panel, these are for understanding the original's ergonomics, not for modelling.

### 6.2 What should move (common to every direction)
- Knobs, the waveform switch, step LEDs (note / tie / rest / accent / slide states), the playing step, pattern
  and group indicators, the CV and gate "outputs" as small meters.
- **The accent capacitor** as a visible element: a small glowing bar or a lit "charge" lamp that rises with each
  accent and drains between them, driven by the real model state (C13 voltage). The single most informative
  piece of UI for this instrument: users see why the fourth accent screams.
- The resonant frequency as a moving marker on a tiny response plot (optional, inspect layer).

### 6.3 Three directions

**A. "Graphite bench unit" (photoreal, original object).** A compact, low extruded enclosure in dark anodised
graphite or deep olive with bead-blasted end caps; a single tilted control strip with the six sound knobs
(large, knurled aluminium caps with a coloured index), Tuning, Tempo and Volume smaller to the side; below, a
16-pad step row with RGB status under translucent caps and a one-octave key row of short mechanical keys in our
own form (square keys in two tiers, not a printed piano); a small amber seven-segment display for tempo,
pattern and step (the TB-03 added a display, so a display is not trade dress); "AT303 / Audio Tools" engraved.
Rendered in Blender on pc50-wsl with the AT101V/ATMD pipeline (layout JSON in mm → silkscreen → scene →
sprites → `pack_assets`). Two or three body colours as in AT101V. *Pros*: series consistency, premium,
clearly not the original. *Cons*: the most render work; the step pads and keys must read at 1x.

**B. "Silver box homage" (photoreal, closer).** Brushed light aluminium, a knob row and a key row, original
typography, reorganised layout. *Pros*: instantly reads as "the acid box" to users. *Cons*: this is exactly the
trade dress the brief excludes beyond "a generic silver box"; the closer it gets the higher the risk, and the less
it looks like an Audio Tools product. Recommended only as a body colour of A (a light "aluminium" finish of the
same original object), not as a layout.

**C. "Live schematic" (vector, non-photoreal).** A dark panel with the circuit as the interface: the ramp core,
the Q8 square, four stacked ladder capacitors whose fill shows their state, the feedback loop, C13 charging, the
two envelopes as live traces, an oscilloscope and the filter response; the knobs sit on the schematic where they
act. The sequencer page is a clean lane grid. *Pros*: unique, educational (fits the README's "a model that
anyone can study, verify against the hardware and improve"), cheap to build and to scale, fits small screens.
*Cons*: breaks the series' photoreal look; less "instrument".

**Recommendation: A as the instrument page, with C as an "inspect" layer** (a button that fades the render
into the live schematic over the same layout, as Tarnish proposes for its service-manual overlay), and a
modern grid page for the sequencer. A body-colour option in light aluminium covers the nostalgia of B without
copying the layout.

### 6.4 Interaction ideas
- Drag on a step LED: vertical = pitch, horizontal = length (tie); click = accent cycle; shift-click = slide.
- Hold a key on the key row while the pattern runs = live transpose from the next measure (the original's
  PITCH MODE + key).
- The C13 bar doubles as a "manual accent" button (the Devil Fish has one).
- Hover a knob: show its value in the circuit's units (Hz at the resonant frequency, ms to 10 %, k), as the
  other Audio Tools instruments show physical units.

---

## 7. Tests (automated, JUCE-free)

In `plugins/at303/tools/dsptest.cpp` (built like the other instruments' `dsptest.cpp`: `clang++ -std=c++20 -O2
-I plugins/at303/Source/dsp -I shared/dsp`), plus `at303render` (a preset or pattern to WAV with a CPU figure)
and the shared host tests. References computed in the test (transfer functions, RC curves) or loaded from small
committed CSVs produced by our own SPICE runs (§4.10). 48 kHz unless stated.

| # | Test | Pass criterion |
|---|---|---|
| T1 | No NaN/Inf, bounded output | Silence ≥ 60 s, DC, full-scale input to the ladder, every preset, 200 random extreme settings with parameters re-randomised every block (seeded), sample rates 22.05-192 kHz, k at the extended maximum: no NaN/Inf (checked by bit pattern, since `-ffast-math` may remove `std::isfinite`), peak ≤ +24 dBFS, CPU in silence within 10 % of CPU with signal (no denormals) |
| T2 | Linear ladder vs analysis | Ladder alone, coupling sections off, input −60 dBFS (tanh negligible), r = 0.5: magnitude within ±0.3 dB of 1/(8(1+s)⁴ − 8(1+s)² + 1 + k) from 0.01 ω₀ to 8 ω₀ for k = 0, 4, 8.5, 12, 16; with r = 0.545 within ±0.3 dB of the generalised model of §1.5.2; slopes from 2ω₀ to 4ω₀ between −18 and −20 dB/oct for every k (the "18 dB"); DC loss at k = 16 = 24.6 ± 0.3 dB |
| T3 | Coupling poles and the low-frequency peak | Full linearised filter (with the five sections) at the 820 Hz setting: response matches the SPICE CSV of the same circuit within ±0.5 dB from 2 Hz to 10 kHz at RES 0, 0.5, 0.99; the low-frequency peak present at high resonance at the SPICE frequency ±10 %; and, once its units are confirmed, Stinchcombe's 10-pole function within ±1 dB |
| T4 | Oscillation threshold and tracking | Extended high-resonance mode: sustained oscillation for k > 17.0 ± 0.2 (r = 0.5) and not below; oscillation frequency within ±10 cents of the prewarped target from 200 Hz to 8 kHz at 4x (ATMD's ladder holds 12 cents to 15 kHz at 96 kHz); stock mode at RES max: no sustained oscillation anywhere in the cutoff range (envelope of the ring decays to −60 dB within 1 s) |
| T5 | Factory calibration ring | CUTOFF 0.5, RES max, ENV MOD/DECAY/ACCENT min, saw, low C: ring period 2.00 ± 0.10 ms (nominal unit); cutoff law: ring at 300 ± 15 Hz and 2.4 ± 0.12 kHz at the knob ends; unit seeds stay within the ±25 % TM3 window |
| T6 | VCO pitch and range | A key at 2.75 V → 110.00 Hz ± 0.5 cent; octave ratio 2.000 ± 0.05 %; four octaves 16.00 ± 0.5 %; tuning knob ends ±700 cents ± 5; slide-free note changes reach the new pitch within one oversampled sample |
| T7 | Waveforms and aliasing | Saw and square at 21.8, 65.4, 261.6 and 784 Hz: single-cycle shape within 1 % RMS of the SPICE tables (after band-limiting); square duty cycle and droop vary with pitch as the tables do; aliased components (non-harmonic) below −90 dBFS at 4x and −80 dBFS at 2x, measured on the raw oscillator with the filter open |
| T8 | Envelopes | MEG time to 10 %: 200 ms ± 5 % at DECAY min, 2.5 s ± 5 % at max (stock); accented notes 200 ms ± 5 % at any DECAY; VEG τ 1.5 s ± 5 %; note start: 4 ms delay + 3 ms attack ± 0.5 ms (stock), < 0.5 ms with fast start; gate end: 8 ms hold + 8 ms linear ± 0.5 ms |
| T9 | Accent accumulation | Runs of 1, 2, 4, 8 accented 16ths at 100/120/140 BPM, RES 0/0.5/1: C13 voltage and the peak cutoff exponent per accent match the SPICE CSV of the sweep circuit within 5 %; peaks strictly increasing over the first four accents at RES 1 and 120 BPM; no accumulation (each peak within 1 % of the first) at RES 0; sweep speeds fast/normal/slow ordered as described [DF] |
| T10 | Slide | 95 % of the interval in 66 ± 3 ms for 1, 7, 12 and 24 semitones (constant time); exponential in log-frequency (residual from the RC curve < 1 cent); no MEG or VEG retrigger on the slid-into note; a slide between equal pitches = one envelope ("TIED") |
| T11 | Gate timing in clocks | Internal clock at 120 BPM: gate rises at c0 and falls at c3.5 of the note's last step (±1 sample); a slid note's gate stays high through the next note; slide switch on for the whole following note; tie N+T = 9.5 clocks; rest = no gate; triplet steps of 8 clocks; documented choices for slide-into-rest and triplet gate fraction hold |
| T12 | Pitch/time scramble (golden) | Classic-mode edits from a fixed script (write 7 pitches with accents and slides, then add an N step at position 3, remove one at 9, set length 12) produce the event list stored in a golden file; tapping past the written pitches plays the stale slots; the grid view of the same pattern matches; disabling "classic quirks" re-flows instead |
| T13 | Host sync and determinism | Renders with blocks of 1, 64, 512, 4096 and seeded random sizes are bit-identical; host PPQ jumps and loop wraps land the next step on the right sample; starting playback at bar 1 or bar 9 gives identical output from bar 17 on; two runs and a run after `reset()` are bit-identical |
| T14 | Sample-rate independence | 44.1/48/96/192 kHz: ring frequency within 0.5 %, decay and slide times within 1 %, gate edges at the same musical position (±1 sample), accent peaks within 2 % |
| T15 | MIDI conventions | Overlap → slide without retrigger; CC 65 → slide even with a gap of 1 tick; velocity threshold and continuous accent; `.mid` export → import round trip reproduces the pattern exactly (all 64 factory patterns) |
| T16 | Generator | Same seed → identical pattern on macOS and Windows; constraints hold over 10,000 seeds (every note in scale and range; slides only between sounding steps; densities within the binomial 99 % interval of the set values) |
| T17 | State and presets | Save/restore round trip bit-identical (patterns, tracks, unit seed); every factory preset renders without NaN and within ±1.5 dB of the leveled target (the `level_presets.py` step of the other instruments) |
| T18 | CPU | `at303render --bench`, 48 kHz, 512-sample blocks, on the Apple M4 and the Ryzen 9 5900XT: within the targets of §4.9; numbers recorded with the date in the README |

Not automated: blind A/B against recordings of a unit (§4.10, decision §9) with 3-5 listeners, loudness
matched, per preset ("which is the hardware?"), and pluginval at strictness 8 plus auval, as for every
instrument.

---

## 8. Factory patterns (proposal: 32 in six categories)

Each preset is a sound plus a bank with the named pattern on A1 (some with a chain or a short track). Patterns
are described by their construction, written for this product; none transcribes a published song. Keys and
tempos vary across the set; knob positions are on the panel's 0-10 scale; "ext" marks extended-page settings.

**Classic squelch (8)**
1. **First Squelch**: saw, cutoff 3, res 8, env mod 6, decay 4, accent 7; 16 steps in A minor pentatonic,
   root-heavy with an octave-up on every third note, accents on the "e" of beats 2 and 4, two slides into
   the octave. 124 BPM.
2. **Warehouse Walk**: saw, cutoff 4, res 7, env mod 5, decay 3, accent 6; root and fifth bounce with ties making
   dotted eighths against the straight grid; one slide down at the end of the bar. 120 BPM.
3. **Rubber Octaves**: square, cutoff 3.5, res 7.5, env mod 7, decay 5, accent 8; octave pairs (down/up) on
   every eighth with slides between the pairs, accents on the upper octave only. 126 BPM.
4. **Accent Ladder**: saw, cutoff 2.5, res 9, env mod 4, decay 2, accent 10; one repeated note, accents on four
   consecutive 16ths at the end of each bar so the accent capacitor climbs: the "wow" in isolation. 122 BPM.
5. **Minor Third Machine**: saw, cutoff 3, res 8.5, env mod 6, decay 6; root, minor third and flat seventh in a
   12-step pattern over a 4/4 bar (polymetric against drums), slides only on the third. 128 BPM.
6. **Slow Bloom**: saw, cutoff 1.5, res 9, env mod 8, decay 9, accent 4; long tied notes with slides between,
   four notes per bar: the decay knob does the work. 110 BPM.
7. **Tight Sixteens**: square, cutoff 5, res 6, env mod 4, decay 1.5, accent 7; every step a note, no ties,
   accents syncopated (3+3+2 grouping), short and percussive. 130 BPM.
8. **Triplet Roll**: saw, triplet mode 12 steps, cutoff 3.5, res 8, env mod 6, decay 3; rising arpeggio of the
   minor triad over two octaves with a slide back down. 118 BPM.

**Deep and hypnotic (6)**
9. **Night Bus**: saw, cutoff 2, res 6, env mod 3, decay 7, accent 3; sparse, half the steps rests, one slide per
   bar, low register; the filter barely opens. 118 BPM.
10. **Submerged**: square, cutoff 1.5, res 5, env mod 4, decay 8; root pedal with a chromatic neighbour every
    second bar (a two-pattern chain). ext: bass fix on. 115 BPM.
11. **Hypnotic Loop 7**: saw, 7-step pattern against the 4/4 grid (length 7), cutoff 3, res 7, env mod 5, decay 5;
    the phrase rotates against the bar. 124 BPM.
12. **Rain Tape**: saw, cutoff 2.5, res 7, env mod 6, decay 6, accent 5; with tempo-synced dotted-eighth delay at 25 %
    and short plate; ties and slides make a legato line. 112 BPM.
13. **Long Fade Track**: a 4-pattern track (shift 0, 0, +5, +7) of a two-bar riff; cutoff 2.5, res 8, env mod 5;
    shows track mode. 120 BPM.
14. **Pulse Under**: square, cutoff 2, res 4, env mod 2, decay 4; straight eighth notes on one pitch with octave
    jumps, a bed for other sounds. 125 BPM.

**Hard and driven (6)**
15. **Screamer Line**: saw, cutoff 4, res 9, env mod 7, decay 4, accent 9; drive "mid" at 60 %; busy 16ths in
    Phrygian with accents on the flat second. 135 BPM.
16. **Bent Steel**: square, cutoff 5, res 8, env mod 8, decay 3; drive "hard" 70 %; ext: soft clip 2. 138 BPM.
17. **Fuzz Pressure**: saw, cutoff 3, res 7, env mod 6, decay 5; drive "fuzz" 50 %, gate length short; octave
    stabs. 132 BPM.
18. **Overdriven Filter**: ext overdrive × 12 into the ladder (pre-filter saturation, the Devil Fish way), cutoff
    3.5, res 8, env mod 6; root-fifth-octave riff. 128 BPM.
19. **Acid Rain Storm**: saw, cutoff 4, res 9.5, env mod 9, decay 2, accent 10; every other step accented with
    slides on the off-beats; drive "mid" 40 %; delay 1/16 dotted 20 %. 140 BPM.
20. **Distorted Hypnosis**: square, cutoff 2.5, res 8, env mod 5, decay 6; drive "hard" 50 %, 12-step pattern.
    130 BPM.

**Extended (6)**
21. **Screaming Oscillator**: ext high resonance on, cutoff 4, res 7; a slow two-note pattern so the filter's
    own tone sings over it.
22. **Filter FM Chaos**: ext filter FM 60 %, cutoff 3, res 8, env mod 6; short accented notes.
23. **Long Slides**: ext slide time 250 ms, cutoff 3, res 7; wide intervals (octave and a fifth) with slides on
    every second note.
24. **Tracking Lead**: ext filter tracking 100 %, decay sustain, cutoff 4, res 6; a higher-register melody that
    works played from MIDI.
25. **Dual Decay Snap**: ext normal decay 1.5 s, accent decay 60 ms; accents snap, the rest blooms.
26. **Slow Sweep Builds**: ext sweep speed slow, accents on eight consecutive 16ths every two bars: the slow C13
    climbs twice as high.

**Play it (4)** (MIDI play mode, no internal pattern)
27. **Keys Classic Bass**: stock sound tuned for MIDI play; velocity ≥ 100 accent; legato slides.
28. **Expressive Accent**: continuous accent (velocity scales ACCENT), slide on CC 65 as well as legato.
29. **Soft Low End**: square, ext bass fix, cutoff 2, res 3, env mod 2, decay 7: a plain bass for songs.
30. **Pluck Lead**: saw, cutoff 5, res 5, env mod 5, decay 2, ext tracking 60 %.

**Init and generator (2)**
31. **Init**: the nominal unit (seed 0), stock settings, factory calibration, an empty 16-step pattern.
32. **Generator Start**: the acid generator set to minor pentatonic, two octaves, density 60 %, accent 30 %,
    slide 15 %, live apply on: press "new" for a pattern.

All patterns use only notes, accents and slides the 303 format can store (except where "ext" says otherwise),
so the classic view can display them; the presets are leveled to the series' ≈ −17 dBFS RMS with the
`level_presets.py` step.

---

## 9. Decisions to make

1. **Name**: AT303 (series convention: the original's number, like AT101V and AT20V) or a name without the
   number, after a trademark check of "303" alone (the original's model name is the maker's trademark, not checked in a
   registry today; whether the number on its own is a risk is a legal question, not answered here).
2. **Hardware reference**: borrow a real TB-303, buy a clone (TD-3 ≈ €100, x0xb0x kit) or work only from SPICE
   and published numbers? The "measured against the hardware" claim of the root README needs one of the first
   two (§4.10).
3. **Square model**: SPICE-derived pitch-indexed tables (recommended, pending the SPICE result) or a static
   shaper like Open303's.
4. **Filter solver**: linearised TPT at 4x as default with Newton as "accurate" (recommended), or Newton always.
5. **Coupling poles**: model the five sections from component values (recommended) or fit Open303-style
   perceptual corners if the circuit model and recordings disagree.
6. **Stock maximum resonance**: k_max ≈ 16.5 (rings, no oscillation) pending measurement.
7. **Envelope time convention**: the service notes' "time to 10 %" (recommended; Open303 is ≈ 2.3 × slower).
8. **Slide τ**: 22 ms from the RC (recommended) against Open303's 60 ms τ.
9. **Gate on slide-into-rest and in triplet mode**: x0xb0x-style (half of the rest) or Open303-style (normal
   gate), until measured; and the default gate fraction (3.5/6 measured by Whittle vs 50 % in x0xb0x, Open303,
   JC303).
10. **Classic quirks default**: scramble on in the classic view, off in the grid (recommended)?
11. **Pattern memory**: one bank of 64 per preset (recommended) or a global user bank shared by presets.
12. **Track mode in 1.0**: playback only (recommended) or the full editor.
13. **MIDI accent**: threshold ≥ 100 by default, continuous as an option (recommended); CC 65 slide on by default?
14. **Extended page scope for 1.0**: the full classic mod set (recommended, §5.5) or a subset.
15. **Effects in 1.0**: soft clip and overdrive only, or also the drive models, delay and reverb (series
   standard). ABL3 users prefer external distortion; TD-3/TB-03 reviews criticise weak built-in drive.
16. **Imports**: x0xb0x and text formats only, or reverse-engineered ReBirth/SynthTribe/TD-3 formats (only if
   documented publicly).
17. **Visual direction**: A with the C inspect layer (recommended), C alone (cheaper), or A only. Body colours:
   graphite, olive, light aluminium?
18. **Reuse of Open303** (MIT): reuse nothing (recommended; cite its constants as data) or reuse its wavetable
   mip-mapping and sequencer logic with the notice.
19. **Battery-loss scramble**: include (P1) or not.
20. **Follow-up research** with a search budget: forum sweeps (Reddit, Gearspace, Elektronauts, ModWiggler,
   YouTube) for TD-3/ABL3/Phoscyon/Acid V/JC303 opinions; Sound On Sound and MusicRadar reviews; DAFx/WDF/neural
   TB-303 papers; the KVR "page 40" thread Open303 credits; the 2012 Synth-DIY timing discussion; the RE-303
   timing analysis; TD-3 and TD-3-AM manuals; licensed TB-303 recordings.

---

## Sources

Accessed 2026-10-19 unless stated. Files marked "private copy" are in
`/Volumes/Docs/Projects/own/audiotools-private/at303-ref/` and are not in the repository.

Hardware documentation:
- Roland TB-303 Service Notes, 1st ed., Feb. 19, 1982: https://www.synthmanuals.com/manuals/roland/tb-303/service_notes/roland_tb-303_service_notes_rvgm.pdf; scans http://machines.hyperreal.org/manufacturers/Roland/TB-303/schematics/ (private copies)
- Roland TB-303 Owner's Manual: https://static.roland.com/assets/media/pdf/TB-303_OM.pdf; https://archive.org/details/synthmanual-roland-tb-303-owners-manual (private copies)
- Roland TB-03 Owner's Manual and v1.04 additions: https://static.roland.com/assets/media/pdf/TB-03_eng03_W.pdf, https://static.roland.com/assets/media/pdf/TB-03_AddFunc_e01_W.pdf (private copies)
- Roland TB-303 Software Bass Line manual: https://static.roland.com/assets/media/pdf/TB-303_Software_Bass_Line_eng03_W.pdf (private copy); product page https://www.roland.com/global/products/rc_tb-303/
- Roland TB-03 product and support pages: https://www.roland.com/global/products/tb-03/, https://www.roland.com/global/products/tb-03/support/
- Analogue Heaven archive (Whittle 1997 pattern lengths; Perkowitz 1995 programming; secrets; internal pots; mods; memory; sync in/out; Haines 1995): http://machines.hyperreal.org/manufacturers/Roland/TB-303/info/
- MAME TB-303 driver (BSD-3-Clause): https://raw.githubusercontent.com/mamedev/mame/master/src/mame/roland/roland_tb303.cpp
- DIN sync: https://en.wikipedia.org/wiki/DIN_sync

Robin Whittle (Real World Interfaces):
- Devil Fish manual (2019-02-09): https://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf (private copy)
- What makes the TB-303 unique: https://www.firstpr.com.au/rwi/dfish/303-unique.html
- Slide and gate timing: https://www.firstpr.com.au/rwi/dfish/303-slide.html
- Mods for stock units: https://www.firstpr.com.au/rwi/dfish/303-mods/
- Memory backup: https://www.firstpr.com.au/rwi/dfish/DF-Memory-Backup.pdf (private copy)
- Devil Fish home, prices, MIDI options, TT-303 notes: https://www.firstpr.com.au/rwi/dfish/, https://www.firstpr.com.au/rwi/dfish/prices-int/, https://www.firstpr.com.au/rwi/dfish/midi-options/, https://www.firstpr.com.au/rwi/dfish/TT-303/

Filter analysis and DSP:
- T. Stinchcombe, "Analysis of the Moog Transistor Ladder and Derivative Filters", 2008: https://www.timstinchcombe.co.uk/synth/Moog_ladder_tf.pdf (private copy); TB-303 pages https://www.timstinchcombe.co.uk/index.php?pge=diode and ?pge=diode2 (figures: private copies)
- V. Zavalishin, *The Art of VA Filter Design*, rev. 2.1.2, 2020: https://www.discodsp.net/VAFilterDesign_2.1.2.pdf (private copy)
- M. Civolani, F. Fontana, "A nonlinear digital model of the EMS VCS3 voltage-controlled filter", DAFx-08: https://www.dafx.de/paper-archive/2008/papers/dafx08_06.pdf; F. Fontana, M. Civolani, "Modeling of the EMS VCS3 Voltage-Controlled Filter as a Nonlinear Filter Network", IEEE TASLP 18(4):760-772, 2010, doi:10.1109/TASL.2010.2046287 (bibliographic only)
- A. Huovilainen, "Non-linear digital implementation of the Moog ladder filter", DAFx-04: https://www.dafx.de/paper-archive/2004/P_061.PDF
- V. Välimäki, A. Huovilainen, "Oscillator and Filter Algorithms for Virtual Analog Synthesis", CMJ 30(2), 2006, doi:10.1162/comj.2006.30.2.19; "Antialiasing Oscillators in Subtractive Synthesis", IEEE SPM 24(2), 2007, doi:10.1109/MSP.2007.323276 (bibliographic only)
- S. D'Angelo, V. Välimäki, "An improved virtual analog model of the Moog ladder filter", ICASSP 2013, doi:10.1109/ICASSP.2013.6637744; "Generalized Moog Ladder Filter" I and II, IEEE TASLP 22(12), 2014, doi:10.1109/TASLP.2014.2352495, doi:10.1109/TASLP.2014.2352556 (bibliographic only)
- E. Brandt, "Hard Sync Without Aliasing", ICMC 2001: https://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf
- F. Esqueda, V. Välimäki, S. Bilbao, "Rounding Corners with BLAMP", DAFx-16: https://www.dafx.de/paper-archive/2016/dafxpapers/18-DAFx-16_paper_33-PN.pdf
- J. Parker, V. Zavalishin, E. Le Bivic, "Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution", DAFx-16: https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf
- S. Bilbao, F. Esqueda, J. Parker, V. Välimäki, "Antiderivative Antialiasing for Memoryless Nonlinearities", IEEE SPL 24(7), 2017, doi:10.1109/LSP.2017.2675541 (bibliographic only)
- mystran, "Cheap non-linear zero-delay filters", KVR: https://www.kvraudio.com/forum/viewtopic.php?t=349859
- karrikuh, diode ladder thread, KVR: https://www.kvraudio.com/forum/viewtopic.php?t=346155
- Open303 KVR thread (page 1): https://www.kvraudio.com/forum/viewtopic.php?t=262829
- Faust `vaeffects.lib` (`ve.diodeLadder`, STK-4.3): https://raw.githubusercontent.com/grame-cncm/faustlibraries/master/vaeffects.lib
- W. Pirkle, RackAFX downloads (VADiodeLadderFilter.zip, proprietary; AN-6 PDF returns 404): https://www.willpirkle.com/Downloads/
- Cytomic technical papers: https://cytomic.com/technical-papers/
- u-he Diva user guide 1.4.8: https://dl.u-he.com/manuals/plugins/diva/Diva-user-guide.pdf
- F. Anwander, Roland filters: http://www.florian-anwander.de/roland_filters/

Open source:
- Open303 (MIT): https://github.com/RobinSchmidt/Open303, https://sourceforge.net/projects/open303/
- JC303 (GPL-3.0): https://github.com/midilab/jc303 (README, LICENSE, releases)
- x0xb0x (MIT): https://github.com/x0xb0x/x0xb0x.github.io; https://www.openmusiclabs.com/wiki/x0xb0x; https://sourceforge.net/projects/x0xb0x/
- acid-banger (CC BY 4.0): https://github.com/vitling/acid-banger, https://www.vitling.xyz/toys/acid-banger/
- Aciduino (MIT): https://github.com/midilab/aciduino
- JUCE licence: https://raw.githubusercontent.com/juce-framework/JUCE/master/LICENSE.md
- Sonic Potions RE-303 CPU: https://www.sonic-potions.com/re303

Products, reviews and ratings:
- AudioRealism: https://www.audiorealism.se/, https://www.audiorealism.se/abl3.html, ABL3 manual https://www.audiorealism.se/uploads/4/5/5/4/45545237/abl3vstauoperationmanual.pdf (private copy); KVR https://www.kvraudio.com/product/audiorealism-bass-line-3-abl3-by-audiorealism (and /reviews), https://www.kvraudio.com/product/audiorealism-bass-line-2-abl2-by-audiorealism/reviews
- D16 Phoscyon 2: https://d16.pl/phoscyon2, manual https://d16.pl/pub/manuals/Phoscyon%202-manual-gb.pdf (private copy), changelog https://d16.pl/pub/changelogs/Phoscyon%202%20-%20Changelist.txt; KVR https://www.kvraudio.com/product/phoscyon-2-by-d16-group (and /reviews), https://www.kvraudio.com/product/phoscyon-by-d16-group
- Reviews: https://musictech.com/reviews/software-instruments/d16-group-phoscyon-2-acid-line-review; https://www.gearnews.com/d16-phoscyon-2-review; https://www.bonedo.de/artikel/d16-group-phoscyon-2-tb-303-emulation-plugin-distortion-effekte-sequencer-arpeggiator-angecheckt-kurztest; https://synthmania.de/2022/12/test-d16-phosycon-2-update-des-roland-tr-303-clones; https://www.buenasideas.de/test/testbericht-d16-group-phoscyon-2-die-letzte-303; http://rekkerd.org/review-d16-group-phoscyon; https://www.musicradar.com/reviews/d16-group-phoscyon-2 (body not loaded)
- Round-up: https://elektronische-musik-produzieren.de/roland-tb-303-clone-die-besten-tb-303-vsts-und-hardware-clones
- Arturia Acid V: https://www.arturia.com/products/software-instruments/acid-v/overview, https://www.kvraudio.com/product/acid-v-by-arturia
- Roland plug-in on KVR: https://www.kvraudio.com/product/tb-303-by-roland
- Behringer TD-3 and TD-3-MO (Thomann): https://www.thomannmusic.com/es-ar/behringer_td_3_sr.htm, https://www.thomannmusic.com/es-ar/behringer_td_3_mo_sr.htm
- Acidlab: https://www.acidlab.de/
- TAL-BassLine-101: https://tal-software.com/products/tal-bassline-101
- ReBirth: https://en.wikipedia.org/wiki/ReBirth_RB-338, https://www.kvraudio.com/product/rebirth-rb-338-by-propellerhead-software

History and background:
- https://en.wikipedia.org/wiki/Roland_TB-303; https://de.wikipedia.org/wiki/Roland_TB-303
- https://en.wikipedia.org/wiki/Behringer
- https://www.attackmagazine.com/technique/hardware-focus/roland-tb-303/
- https://www.soundonsound.com/people/history-roland-part-2 (read by one research pass today; a second fetch returned HTTP 410)
- http://www.muzines.co.uk/articles/roland-tb-303-bass-line/4250 (E&MM, April 1982)
- http://www.rolandus.com/blog/2013/03/28/tb-303-acid-flashback/
- Wikimedia Commons, Category:Roland TB-303 (CC0 panel photo; CC BY-SA recordings): https://commons.wikimedia.org/wiki/Category:Roland_TB-303
