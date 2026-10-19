# Roland System-700 (1976): hardware specification for the AT700 recreation

Compiled 2026-10-17 from the original Roland instruction manual, service-note schematics, a
first-hand Japanese owner/demonstrator account, and secondary web sources. Every fact carries a
source key in square brackets; the key list is in section 9. Where sources disagree, all versions
are listed. Anything not found is marked **NOT FOUND**. Inferences I made myself (from schematics,
scaled drawings or arithmetic) are marked **(inferred)**.

Primary source: **[M]** = *System 700 Synthesizer Instruction Manual*, (c) 1976 Roland, printed
Feb '77 (C-2), 224 pp. Not in this repository (copyrighted; see `ref/CREDITS.md`). "M p.N" = PDF page N.

---

## 0. Corrections to the brief (read first)

- **"701-707" are not cabinet numbers.** Three-digit 7xx codes are *module* codes (701A keyboard,
  702 VCO, 703 VCF, 704 VCA, ...). Cabinets are called **Blocks 1-7**, with an extra **Block 8
  "Laboratory" console** [M p.5, p.12-15, p.21] [WP] [FA].
- **The panels are black, not grey/blue/white.** Every photo shows black (anodised) aluminium panels
  with white legends, in black-tolex cabinets [PH1] [PH2] [RLM]. The grey/blue/white look belongs to
  other Roland products, not the 700.
- **The VCFs are OTA designs, not ladders.** The 12 dB filters are state-variable filters built
  from CA3080 OTAs; the 24 dB filters are four cascaded CA3080 OTA stages. No transistor ladder and
  no IR3109 are used [FA-F] [YS-703BC] [YS-703EFG].
- **The sequencer is a 717A, not a "104".** "104" is the *System-100* sequencer manual number.
  The 700's sequencer is the **717A**, with 3 rows x 12 steps [M p.64].
- **The "Lab Series" is part of the 700 range, not the 100M.** Roland sold a *Laboratory System*
  (Block 8 console + 701A keyboard) as the smallest System-700 [ANZ-idx] [WP]. The System-100M (1978)
  is a different, later product [WP]. One blog calls the Main Console the "Lab Series" [AS], which is
  wrong.
- **There is no "Expander" cabinet.** The optional cabinets are Blocks 3-7. "Expander Unit"
  (Model 102) is a *System-100* product; the 700 manual only recommends its manuals for theory
  [M p.3-4].

---

## 1. Configuration

### 1.1 The complete system

"The complete system consists of the MAIN CONSOLE, the KEYBOARD CONTROLLER, and five optional blocks
for a total of 47 modules" [M p.5]. The Japanese Product News (Vol. 20) also says 47 modules
[ANZ-etc]. Anzai elsewhere says "46 modules, 20 types" [ANZ-know]. The block lists below add up to
47 when the 701A and 717A are counted as modules **(inferred)**.

| Block | Name | Dimensions with cover (W x H x D) | Weight | Source |
|---|---|---|---|---|
| 1 | MAIN CONSOLE | 830 x 570 x 305 mm | 36 kg | [M p.12] |
| 2 | KEYBOARD CONTROLLER (701A) | 1075 x 120 x 240 mm | 10 kg | [M p.13] |
| 3 | ANALOG SEQUENCER (717A) | 838 x 360 x 305 mm | 19 kg | [M p.13] |
| 4 | VCO BANK | 445 x 570 x 305 mm | 19 kg | [M p.14] |
| 5 | VCF/VCA BANK | 445 x 570 x 305 mm | 18 kg | [M p.14] |
| 6 | INTERFACE/MIXER | 445 x 360 x 305 mm | 11 kg | [M p.15] |
| 7 | PHASE SHIFTER/AUDIO DELAY | 445 x 360 x 305 mm | 11 kg | [M p.15] |
| 8 | LABORATORY console (sold separately) | "same size as the VCO bank" cabinet [ANZ-know]; "size of one lower wing cabinet" [VSE] [RLM-LAB] | NOT FOUND | |

System total: 124 kg [M p.12]. The 2008 eBay system was described as "7 enclosures ... 220 pounds"
[MX-2008].

The Science Museum measured its own unit (object 1992-441, c.1981) slightly differently: main
console 560 x 840 x 300 mm, Block 2 115 x 1068 x 227, Block 3 300 x 340 x 840, Blocks 4/5 540 x 445 x
300, Block 6 340 x 445 x 300, Block 7 330 x 445 x 300 [SMG]. Those are measured H x W x D figures,
not the catalogue values.

**Physical arrangement** (front view; rear view in [M p.16]; photo [PH1]):

```
 +-----------+---------------------------+-----------+
 |  Block 6  |   Block 3  717A SEQUENCER |  Block 7  |   upper row, 360 mm tall
 | INTERFACE |                           | PHASE/DLY |
 +-----------+---------------------------+-----------+
 |  Block 4  |   Block 1  MAIN CONSOLE   |  Block 5  |   lower row, 570 mm tall
 | VCO BANK  |                           | VCF/VCA   |
 +-----------+---------------------------+-----------+
             [   Block 2  701A KEYBOARD   ]
```

Assembled size is about 1728 mm wide x 930 mm tall **(inferred)** (445 + 838 + 445 mm; 570 + 360
mm). The left/right placement of Blocks 4/5 and 6/7 is taken from [PH1]. The manual's rear-view
figure shows the same 3 x 2 grid, with bus cords from every cabinet to the main console and the
keyboard's DIN cable going to the main console [M p.16]. Except for the main console, modules are
interchangeable between blocks [M p.5] [M p.17].

### 1.2 Module codes [M p.21]

701 Keyboard Controller · 702 VCO · 703 VCF · 704 VCA · 705 Dual Envelope Generator · 706 LFO ·
707 Amplifier/Envelope Follower/Integrator · 708 Noise Generator/Ring Modulator · 709 Sample & Hold ·
710 Multiple Jack · 711 Output Module · 712 Monitor/Voltage Processor · 713 Gate Delay · 714 Interface
(F/V converter) · 715 Multimode Filters/Audio Mixer · 716 Mixer (Signal/CV) · 717 Analog Sequencer ·
718 Power Switch · 719 (unused) · 720 2CH Phase Shifter · 721 2CH Audio Delay · 722 (unused) · 723
Analog Switch · 724-728 (unused). The numbers 719, 722 and 724-728 are blank in the manual. Anwander
suggests that photos of some racks with pairs of modules in the bottom corners may explain a gap
[FA].

The letter suffix encodes *which internal (bus) connections* a module has. The circuit is the same:
"exactly the same ... except for bus connections" [M p.26-27, p.31, p.35-36, p.46].

### 1.3 Block contents, in panel order

Panel heights: tall modules are 280 mm, short modules 140 mm (from per-module "Panel size") [M].
Widths come in steps of 65 / 98 / 131 / 197 mm (roughly 1, 1.5, 2 and 3 units) [M].

**Block 1: MAIN CONSOLE** [M p.12]. Its layout is fixed at the factory because of the internal
wiring; other layouts were special order only [M p.5] [M p.17].
- **Upper row, left to right (280 mm tall):** 702A VCO-1 · 702B VCO-2 · 702C VCO-3 · 708A NOISE
  GENERATOR / RING MODULATOR · 703A VCF-1 · 703B VCF-2 · 704A VCA-1 · 704B VCA-2 · 711A OUTPUT MODULE
  (Reverberator, Panning, Standard Osc, Phase Shifter).
- **Lower row, left to right (140 mm tall):** 712A MONITOR / VOLTAGE PROCESSOR · 706A LFO-1 · 706B
  LFO-2 · 709A SAMPLE & HOLD · 707A AMPLIFIER / ENVELOPE FOLLOWER / INTEGRATOR · 705A DUAL ENVELOPE
  GENERATOR (ADSR-1, ADSR-2) · 716A MIXER · 710A MULTIPLE JACK · 718A POWER switch (with the "Roland
  SYNTHESIZER SYSTEM-700" logo).
- **Below the module rows:** an empty recessed tray/handle area [M p.12] [PH2].
- **VCF-1 variant (sources disagree):** the 1976/77 manual shows **703A** (12 dB multimode HP/BP/LP)
  as VCF-1 [M p.12, p.29]. Surviving consoles are documented with **703E** (24 dB low-pass, no mode
  switch, voltage-controlled resonance) as VCF-1 and 703B as VCF-2: yusynth's console ("VCFs 703E/B:
  E 24dB/oct; B 12dB/oct multimode") [YS] and RL Music's console s/n 590069 ("1 x Low Pass VCF, 1 x
  LP/BP/HP VCF") [RLM]. Anzai lists 703A/B/D/E/G as the internally wired (console/Lab) VCF variants
  [ANZ-p9]. **Recommendation for AT700:** offer both VCF-1 variants (703A or 703E).

**Block 2: 701A KEYBOARD CONTROLLER**, see section 2.1.

**Block 3: 717A ANALOG SEQUENCER**, one module filling the cabinet [M p.13, p.64].

**Block 4: VCO BANK** [M p.14]
- **Upper row:** 702D · 702D · 702D · 702E · 702E · 702E. The 702D is the same as 702A and the 702E
  the same as 702B/C, except that keyboard CV is the only internal connection [M p.7] [M p.27].
- **Lower row:** 716A MIXER · 710A MULTIPLE JACK · 706B LFO-2 · 709B SAMPLE & HOLD · 705A DUAL ENVELOPE
  GENERATOR.

**Block 5: VCF/VCA BANK** [M p.14]
- **Upper row:** 703C VCF · 703C VCF · 704C VCA · 704C VCA · 704C VCA.
- **Lower row:** 713A GATE DELAY · 705A DUAL ENVELOPE GENERATOR · 705A DUAL ENVELOPE GENERATOR · 710A
  MULTIPLE JACK.
- The 703C is the same as 703A/B, but keyboard CV is its only internal connection. The 704C has no
  internal connections. The 705As get keyboard gate/trigger internally [M p.7] [M p.31] [M p.36].

**Block 6: INTERFACE/MIXER** [M p.15], left to right: 714A INTERFACE · 704 VCA · 715A MULTIMODE FILTERS
/ AUDIO MIXER.
- The VCA is labelled "704A" in the block drawing [M p.15] but "704C" in the brochure text [M p.8].
  Gearogs, Anzai and Wikipedia list the interface as **714B**, the manual as 714A [GO] [ANZ-p1] [WP]
  [M p.58].

**Block 7: PHASE SHIFTER/AUDIO DELAY** [M p.15], left to right: 710B MULTIPLE JACK · 723A ANALOG SWITCH
· 720B 2CH PHASE SHIFTER · 721A 2CH AUDIO DELAY.
- The manual says 721A. The schematic scan [FA-721], Gearogs [GO] and Anzai [ANZ-know] say **721B**.

**Block 8: LABORATORY SYSTEM** (not in the 1977 manual)
- **Module list per Wikipedia:** 702A, 702B, 702C, 708A, 703G, 704D, 716A, 706A, 709A, 705A, 718A
  [WP].
- **Functions:** 3 VCO, 1 VCF, 1 VCA, 1 LFO, 2 ADSR, white and pink noise, ring mod, S/H, mixer
  [VSE] [RLM-LAB].
- **703G:** 4 audio inputs (others have 5); plugging into its jack cancels the normalled noise
  [ANZ-p9]. 24 dB with voltage-controlled resonance [ANZ-p9] [FA] [YS-703EFG].
- **704D:** VCF-1 and ring modulator are normalled to its inputs [ANZ-pa].
- **Panel order:** **NOT FOUND**.
- **Narrower modules:** a Lab-specific "narrower modules" claim appears only in a search summary of
  a forum post, unverified.
- **702F/702G:** the yusynth VCO redraw is titled "MODULE 702A-B-C-D-E-F-G" [YS-702], so variants
  702F and 702G exist. Their roles are **NOT FOUND**; they may be Lab variants **(inferred)**.
- **1975 prototype:** first shown at the autumn 1975 musical-instrument fair. Its module
  arrangement differed slightly, and it had monitor speakers at the lower left and right of the main
  console, "like the ARP 2600" [ANZ-know] [ANZ-etc].

### 1.4 Standard module counts (full 7-block system)

Per Anzai's panel pages [ANZ-p4..pe]: 9 x 702, 4 x 703, 6 x 704, 4 x 705A (8 ADSRs), 3 x 706, 2 x 709,
3 x 710, 2 x 716.

Roland Australia's chronicle says "nine oscillators, four filters, five amplifiers, four envelope
generators, three LFOs" [RAU]. Its "five amplifiers" conflicts with the six VCAs in the block lists:
2 in Block 1, 3 in Block 5 and 1 in Block 6 [M p.12-15].

---

## 2. Modules: controls, jacks, ranges

### Conventions common to all modules [M p.17-18, p.21-]

- **Panel layout:** the upper row of sliders is the **audio input mixer**, the lower row is the
  **CV input mixer**, and module function controls plus the OUTPUT sit between them [M p.17].
- **Audio levels:** audio outputs run 10 V p-p for optimum performance. An input sum of about 13 V
  p-p (more than one input slider at "10") lights the **red overload LED**. A **green LED** shows
  normal signal and goes out when red lights [M p.17] [M p.29].
- **CV levels:** control voltages run 0 to +/-10 V. Gates and other "digital" signals are +15 V
  [M p.17].
- **Pitch standard:** **1 V/octave** throughout (VCO, VCF, LFO rate, F/V converter, sequencer clock)
  [M] [ANZ-know].
- **Impedances:** output impedance 600 ohm and input impedance >50 kohm on most modules. Outputs
  should drive at least 10 kohm, and short-circuits of a few seconds do no damage [M p.17].
- **Gate/trigger thresholds:** gate/trigger inputs (S&H clock, gate delay, analog switch, interface
  EXT CONT, VCO sync) switch at about **+5 V** [M p.47, p.57, p.73, p.59, p.83].
- **Power:** each block regulates its own **+/-15.00 V +/-0.1 %**, plus **+14.4 V** for LEDs/lamps,
  from **+/-23 V** unregulated delivered over the bus cords. Modules plug into 15-pin edge
  connectors [M p.18, p.166-167].
- **Total current:** +23 V 1489 mA and -23 V 1080 mA for the standard system plus a second keyboard
  [M p.67] [M p.18].

### 2.1 701A KEYBOARD CONTROLLER (Block 2) [M p.23-24, p.75-79]

- **Keys:** 61 keys, C to C (5 octaves) [M p.5] [M p.23].
- **Two voices:** **CV1 = lowest key held, CV2 = highest key held** [M p.75-77]. Anzai calls it
  "low- and high-note priority" [ANZ-know].
- **Unison behaviour:** releasing all but one key gives unison (CV1 = CV2) [M p.76].
- **Key CV:** 1 V/octave, **0 V at the lowest C (C1) to +5.00 V at C6** (calibration) [M p.23]
  [M p.167-168].
- **CV hold and droop:** the CV is held after key release by an internal S/H. "Due to leakage, the
  voltage will slowly drift downwards" [M Section 5 preliminaries, ~p.165]. Optional emulation
  quirk.
- **Gate:** +15 V while any key is down. Legato gives one gate for the whole passage [M p.78].
- **Trigger:** +15 V, 2 ms, each time a key is pressed. When several keys are held, a trigger fires
  each time the *lowest* key in the group is pressed or released [M p.78].

**Front panel** (135 x 155 mm) [M p.23]:
| Control | Label | Range |
|---|---|---|
| Knob | PITCH | +/-6 semitones (tuning) |
| Knob | PORTAMENTO TIME | 0-10 sec |
| Toggle | PORTAMENTO | ON / OFF |
| Slide switch | PITCH BEND RANGE | OFF / 0.1V / 1V / 5V |
| Horizontal lever | BENDER | "-" / 0 / "+", centre click stop [M p.6] |
| Jack (panel, per drawing) | KEY CV OUT | |

- **Rear panel** (120 x 36 mm): **TO CONSOLE** DIN socket carrying +/-15 V, gate, CV1 and trigger
  (a single DIN patch cord makes all mono connections) [M p.6] [M p.24]. Jacks: **KEY CV1, KEY CV2,
  GATE OUT, KEY TRIG., BEND CV**.
- **BEND CV OUT:** 0 to +/-5 V straight from the lever, independent of the range switch [M p.79].
- **Portamento:** RC slew after the S/H (block diagram) [M p.24]. Curve shape is **NOT FOUND**;
  exponential RC is likely **(inferred)**.
- **Power:** +15 V 40 mA / -15 V 36 mA [M p.23].

### 2.2 702 VCO (A/B/C on console; D/E in Block 4) [M p.25-28, p.83-90, p.178] [YS-702] [ANZ-p4]

**Specs** [M p.28]:
- Frequency range 0.1 Hz-100 kHz. Output 10 V p-p. 1 V/oct.
- Stability after a 3-minute warm-up: 0.05 % for +/-10 % line voltage, 0.1 % over 0-40 C.
- Square-wave PWM sensitivity 10 %/V (702A/D).
- Panel 65 x 280 mm.
- Power +15 V 44 mA / -15 V 35 mA.

**Outputs:** four simultaneous waveform jacks (symbols on panel; names on 702D/E panels) [M p.25,
p.27]. Levels per the yusynth redraw [YS-702]:
| Waveform | Level |
|---|---|
| SINE | -5 / +5 V |
| TRIANGLE | -5 / +5 V |
| SAWTOOTH | 0 / +10 V |
| RECTANGULAR (pulse) | 0 / +10 V |

**Controls (702A, top to bottom)** [M p.25] [PH2]:

| Control | Label | Details |
|---|---|---|
| 4-position lever switch | WAVE FORM SELECTOR | Picks which waveform goes onto the VCO bus feeding the console VCFs and ring mod. **702A/B/C only** [M p.25] [ANZ-p4] |
| Knob | PULSE WIDTH | 10 % (left) - 50 % - 90 % |
| Rotary switch | RANGE | **LO · 32' · 16' · 8' · 4' · 2'**. At 8', A3 key = 440 Hz; 2' A3 = 1760 Hz; 32' A3 = 110 Hz [M p.178]. At 8', C1 = 65.406 Hz [M p.177] |
| Concentric knob | PITCH (SEMITONE) | Outer: -12...0...+12 semitones (+/-1 octave, except in LO). Inner FINE: +/-1 semitone [M p.25] [ANZ-p4] |
| Slider + jack | PWM | PWM CV input and amount (**702A/D only**) |
| Toggle | KEY CV | ON / OFF; keyboard CV bus to the VCO |
| Sliders | MODULATION | 702A: LFO-1 (bus) and EXT (jack). 702B: ADSR-1, LFO-1, EXT. 702C: S&H, LFO-1, EXT |
| Toggle per mod slider | 1/10 attenuator | x1 / x1/10 |

- **Keyboard CV scaling:** with keyboard CV patched to a mod input at x1 and the slider at 10, the
  VCO tracks the 12-tone scale exactly [ANZ-know].
- **702B/C/E additions:** **SYNC** jack plus 2-position switch **S (strong) / W (weak)**. Sync
  needs a positive-going edge above +5 V. The input is AC coupled [M p.83-88].
- **Strong sync** = hard reset: the slave locks only to harmonics of the master, otherwise you get
  the classic distorted sync timbre.
- **Weak sync** = soft lock: it also locks to some sub-harmonic and non-octave intervals, and beats
  between them [M p.85-89]. Anzai: use strong for timbre, weak to kill beating at unison, fifths and
  octaves [ANZ-p4].
- **702D:** 2 external mod inputs plus PWM. **702E:** 3 external mod inputs plus sync. Anzai: "702A/D
  have 2 inputs, 702B/C/E have 3" [ANZ-p4]. The manual's input table is ambiguous on this [M p.28].

**Circuit** [YS-702] [FA-702]:
- **Expo converter:** **uA726** heated, temperature-stabilised matched transistor pair (thermostat on
  the die).
- **Integrator:** LF356, with an **NF510** JFET reset.
- **Comparator:** LM311.
- **Waveshapers:** discrete 2SC945/2SA733 transistors and LM301A op-amps. The sine is diode-shaped
  from the triangle.
- **Sync:** strong sync resets the core through a 2SK30A JFET; weak sync injects through a 250 pF
  capacitor.
- **RANGE ladder:** 1 kohm 0.1 % per octave step, with 2 kohm for LO. This suggests **LO = 2 octaves
  below 32'** **(inferred, unverified)**. LO also widens the PITCH knob range through switch S1b
  ("ON=LO") [YS-702]. The manual only says the PITCH range differs in LO [M p.25].
- **Stability claims:** "very stable (unlike Moog modulars)" [SS]. The uA726 is meant to stop drift
  with ambient temperature [MW-726]. Anzai calls the 700 durable and rarely faulty [ANZ-know].

### 2.3 703 VCF [M p.29-33] [ANZ-p9] [FA-F] [YS-703BC] [YS-703EFG]

**Two circuit families:**

| Family | Variants | Topology | Slope / modes | Extra |
|---|---|---|---|---|
| 12 dB multimode | **703A, B, C** (and **D** per [GO] [ANZ-p9]) | 2-pole **state-variable**: 2 x CA3080 OTA integrators + CA3140AT buffers, LM741C input summer [FA-F] [YS-703BC] | **FILTER MODE switch HP / BP / LP** [M p.29] | Resonance through a dual-gang pot, with an anti-parallel 1S1555 diode pair limiting the resonance loop [YS-703BC] |
| 24 dB low-pass | **703E, F, G** | **4 cascaded CA3080 OTA** one-pole stages + CA3140AT buffers, global feedback; expo converter tempco'd with an SDT1000 thermistor ("CTN 10K 4300ppm/K") [YS-703EFG] [FA-F] | LP only, no mode switch | **RESONANCE IN** jack (voltage-controlled resonance) [ANZ-p9] [YS-703EFG] |

Gearogs: "703B,C,D: 12dB VCFs; 703E,F: 24dB VCFs" [GO]. VintageSynth repeats this [VSE]. Anwander's
page header says "703e, d, f" but its schematic section says "703e, f, g" [FA-F]. 703G as a 24 dB
filter is supported by [ANZ-p9] and [YS-703EFG].

**703A panel, top to bottom** [M p.29] [PH2]:
- **Audio input sliders:** VCO-1, VCO-2, VCO-3 (buses), WHITE NOISE (703A) or RING MOD (703B) (bus),
  plus EXT slider and jack.
- **CUTOFF FREQ:** vertical slider, scale in Hz (100, 1K, 10K marked).
- **RES.:** vertical slider, 0-10.
- **FILTER MODE:** 3-position slide switch HP / BP / LP.
- **VCF OUT** jack, red and green LEDs.
- **MODULATION sliders:** KEY CV (bus), ADSR (bus, with an **ADSR1 / ADSR2** select switch), LFO-1
  (bus), EXT (jack).
- **703C:** all five audio inputs and three CV inputs are jacks. Key CV is the only internal
  connection [M p.31] [ANZ-p9].
- **703E/F/G:** same layout without FILTER MODE, plus the RESONANCE IN (V.C. RESONANCE) jack
  [ANZ-p9] [YS-703EFG].

**Specs** (703, printed for the 1977 family) [M p.33]:
- Frequency response 20 Hz-20 kHz.
- S/N (LP, no modulation): >70 dB at high cutoff, >85 dB at low cutoff.
- **Cutoff range 5 Hz-30 kHz.** Anzai gives 20 Hz-20 kHz [ANZ-p9].
- **Resonance 0 to self-oscillation.**
- **"Fall off rate above cutoff: RESONANCE at '0': 12 dB/octave; RESONANCE at '7': 24 dB/octave".**
  As printed. This describes the apparent steepness near cutoff on a 2-pole SVF; the circuit is
  2-pole.
- Control sensitivity 1 oct/V. Input >50 kohm, output 600 ohm.
- Panel 98 x 280 mm. Power +15 V 21 mA, -15 V 21 mA, +14.4 V 11 mA.
- The response plot shows LP, BP and HP curves with a resonant peak of about +8 dB at about 1.5 kHz,
  passband about -13 dB [M p.33].

**Behaviour** [ANZ-p9] [ANZ-know]:
- **Self-oscillation:** both families self-oscillate with resonance at about **7-8 and above**, and
  the oscillation tracks 1 V/oct (usable for whistles and melodies).
- **Key tracking:** keyboard CV slider at 10 = full key tracking.
- **703E/F/G passband loss:** raising resonance lowers perceived passband volume, making it "difficult
  to use". This is a classic OTA-cascade passband loss to model.

### 2.4 704 VCA (A/B on console; C in Blocks 5/6; D in Lab) [M p.34-37] [ANZ-pa] [YS-704]

- **Controls:**
  - Audio input sliders: VCF-1, VCF-2 (buses) and EXT slider + jack.
  - **VCA GAIN** knob (initial gain, 0-10).
  - **LIN / EXP** toggle.
  - MODULATION sliders: ADSR (with ADSR1/ADSR2 select switch) and EXT (jack).
  - **VCA OUT** jack, red and green LEDs [M p.34].
- **704C:** all inputs are jacks (3 audio, 2 CV) [M p.36]. **704D:** VCF-1 and ring mod normalled
  [ANZ-pa].
- **Specs** [M p.37]:
  - DC to >20 kHz (usable on CVs). S/N >85 dB.
  - **LIN 10 %/V, EXP 10 dB/V.**
  - Input >50 kohm, output 600 ohm. Panel 65 x 280 mm.
  - Power +15 V 15 mA, -15 V 8 mA, +14.4 V 11 mA.
- **Circuit:** **CA3080** OTA core with 2SC945/2SA733 and 2SC1000GR/2SA492GR; DC BAL and EXP WIDTH
  trims [YS-704].

### 2.5 705A DUAL ENVELOPE GENERATOR [M p.38-39] [ANZ-p8] [YS-705]

Two identical ADSRs (ADSR-1, ADSR-2) [M p.38].

**Per-ADSR controls:**
| Control | Label | Details |
|---|---|---|
| 3-position switch | trigger select | **GATE + TRIG / GATE / EXT GATE** |
| Jack | EXT GATE | |
| Slider | ATTACK TIME | |
| Slider | DECAY TIME | |
| Slider | SUSTAIN LEVEL | 0-10 |
| Slider | RELEASE TIME | |
| Toggle | TIME | **x1 / x1/10** |
| Jack | OUT | |

- **MANUAL GATE:** one push button (red in photos) gates **both** ADSRs [M p.38] [PH2].
- **Trigger select behaviour:** GATE+TRIG retriggers on every new note. GATE triggers only on the
  first note of a legato phrase [ANZ-p8].
- **Times (sources disagree):** the spec page says A/D/R 2 ms-10 s and sustain 0-100 % (0-10 V)
  [M p.39]. The panel callouts say "0.2 ms - 10 sec" [M p.38]. Anzai explains: **x1 = 2 ms-10 s,
  x1/10 = 0.2 ms-1 s** [ANZ-p8] [ANZ-know].
- **Slider scale:** 2 ms, 100, 200, 500 ms, 1, 2, 5, 10 s [M p.38].
- **Circuit:**
  - Classic RC envelope (exponential segments): 3.3 uF timing cap for x1, 0.33 uF for x1/10.
  - Pots: 2 Mohm attack, 1 Mohm decay and release.
  - An N13T1 PUT and discrete transistors detect the attack peak. RC1458 buffer [YS-705].
  - Output peak is about +10 V [M p.39].
- **Panel:** 131 x 140 mm. Power +15 V 23 mA / -15 V 12 mA [M p.39].
- **Reputation:** "extremely fast envelopes" [SMB].

### 2.6 706 LFO (706A LFO-1, 706B LFO-2) [M p.40-41] [ANZ-pe] [YS-706]

**706A panel:**
- **DELAY TIME** knob, 2 ms-10 sec (scale 2ms, 100, 500, 1, 2, 5, 10sec).
- 4-position waveform slide switch: **square, saw (falling ramp), triangle, sine**.
- **LFO OUT** jack and red LED (flashes at the rate).
- **FREQ C.V IN** jack.
- **FREQUENCY** knob, scale 0.03-0.05-0.1-0.2-0.5-1-2-5-10-20-30 Hz.
- **KEY TRIG.** ON / OFF toggle [M p.40] [M hi-res crop].

**706B:** **PULSE WIDTH** knob (10-90 %) replaces DELAY TIME, and the square becomes a variable pulse
[M p.40] [ANZ-pe].

**Specs** [M p.41]:
- **Frequency 0.01-30 Hz** (spec page). The knob scale and Anzai say 0.03-30 Hz [ANZ-pe].
- 10 V p-p. 1 oct/V CV. Panel 65 x 140 mm.

**Waveform levels and phase** [M p.41]:
| Waveform | Level |
|---|---|
| Sine | +/-5 V |
| Triangle | +/-5 V |
| Saw (descending ramp) | 0 to +10 V |
| Square | 0 to +10 V |

- **Delay:** the 706A delay is an envelope-controlled VCA (CA3080) on the output that fades the LFO in
  after key gate, with curves from 2 ms to 10 s [M p.41] [YS-706]. It only acts on the first note of
  a legato phrase [ANZ-pe].
- **KEY TRIG start phase:** sine and triangle restart at 90 degrees, saw and square at 0 degrees
  [ANZ-pe].

### 2.7 707A AMPLIFIER / ENVELOPE FOLLOWER / INTEGRATOR [M p.42-43] [YS-707]

| Section | Controls and jacks | Specs |
|---|---|---|
| AMPLIFIER | IN, gain select switch **x10 / x100 / x1000**, GAIN slider 0-1, OUT (normalled to the EF input) | 20/40/60 dB; 20 Hz-20 kHz; max output 20 V p-p; S/N >75/65/50 dB |
| ENVELOPE FOLLOWER | IN (switching jack, breaks the AMP normal), **E.F.** green LED, OUT (normalled to the integrator input) | 1 V DC per 1 V p-p |
| INTEGRATER (sic, as printed on the panel) | IN (switching jack), **LAG TIME** slider (scale 0, 0.1, 0.2, 0.5, 1, 2, 3 sec), OUT | Lag 50 ms-3 s (spec); "0-3 sec" (brochure) [M p.6] |

- **Panel:** 131 x 140 mm [M p.43].
- **Quirk:** the integrator's poor linearity detunes pitch CVs used as portamento [ANZ-know].
- **Circuit:** LM301 op-amps [YS-707].

### 2.8 708A NOISE GENERATOR / RING MODULATOR [M p.44-45] [ANZ-pc] [YS-708]

- **Noise:** **WHITE NOISE OUT** and **PINK NOISE OUT** jacks, both 15 V p-p. Pink slope -3 dB/oct.
  Output 600 ohm [M p.45]. Source: 2SC828 transistor junction noise [YS-708].
- **Ring modulator controls:**
  - **"X" input selector** (lever): VCO1 / VCO2 / VCO3 / EXT.
  - **"Y" input selector**: VCO1 / VCO2 / NOISE / EXT.
  - External X and Y jacks.
  - **R.M. OUT** jack (also normalled to VCF-2), green LED [M p.44] [M p.6].
- **Ring modulator specs** [M p.45]: 20 Hz-20 kHz, 60 dB input rejection, max X + Y = 20 V p-p.
  Output contains sum and difference only.
- **Circuit:** **LM1496** balanced modulator with a carrier-balance trim [YS-708].
- **Panel:** 65 x 280 mm.

### 2.9 709 SAMPLE & HOLD (709A console, 709B no internal connections) [M p.46-47] [ANZ-p7]

- **709A controls:**
  - **SAMPLE MODE** switch: LFO-1 / NOISE / EXT.
  - **EXT SIG** jack, **S&H OUT** (normalled to VCO-3), **CLOCK OUT**.
  - Clock select switch (INT / EXT), **CLOCK RATE** knob (0.3-30 Hz), **EXT CLOCK** jack.
- **709B:** SIGNAL IN jack in place of the mode switch.
- **Specs:** max input +/-10 V. Ext clock +5 V square/pulse. Output +/-10 V. Clock out +15 V square.
  Clock 0.3-30 Hz. **Sample time 0.3 ms**. Panel 65 x 140 mm [M p.47].
- **Circuit:** CA3140 hold buffer, 2SK30A switch, TA7504 [YS-709].

### 2.10 710 MULTIPLE JACK [M p.48-49]

- **710A:** two columns of 5 jacks, each column bused (10 jacks) [M p.48].
  - The spec says panel 65 x 280 mm [M p.49], but the 710A sits in the 140 mm lower rows of Blocks
    1/4/5 [M p.12, p.14]. Treat it as a **short (140 mm)** module; the printed size looks like an
    erratum **(inferred)**.
- **710B:** three groups of 4 jacks, with two **ON/OFF** switches that join upper+centre and
  centre+lower groups (12 jacks). Panel 65 x 140 mm [M p.48-49]. It sits full-height in Block 7
  [M p.15].

### 2.11 711A OUTPUT MODULE (197 x 280 mm) [M p.50-53] [FA-711]

**REVERBERATOR:**
- Sliders VCA1, VCA2 (buses) and EXT (jack); "REV." block; **REV OUT** jack (also normalled to PAN 3).
- **Max reverb time 2 s.**
- **Spring tank** ("REVERB UNIT" in the schematic), driven by a TA7200P and recovered by a TA7136P
  [FA-711].
- Anzai: sound quality "quite poor, especially the reverb" [ANZ-know].

**PANNING:**
- Three input-level sliders, VCA1, VCA2 and REV, each with a **switching** EXT jack that breaks the
  bus.
- Three horizontal **PAN 1 / PAN 2 / PAN 3** sliders (L-R).
- Panning is **manual** [M p.51]. Gearogs says "voltage-controlled panning" [GO]; the manual shows no
  pan CV input, so that claim looks wrong.
- Two illuminated **VU meters** (L ch, R ch) and **OUT** jacks L and R: the final synth output,
  600 ohm.
- Specs: 20 Hz-20 kHz, S/N >75 dB with reverb off, separation >54 dB, output 0.775 V / 1 kohm
  [M p.53].
- Circuit: TA7136P output amps, discrete meter drivers [FA-711].

**STANDARD OSC (test oscillator):**
- **LEVEL** knob (OFF-10, with on/off switch), channel selector **L / LR / R**, frequency selector
  **110 · 220 · 440 · 880 · 1760 Hz** (square, from one master oscillator and an LM3216 divider so
  octaves are beat-free) and **1 kHz** (sine, separate oscillator, for 0 dB reference) [M p.51,
  p.53, p.105] [FA-711].

**PHASE SHIFTER:**
- Sliders VCA1 and VCA2 with switching EXT jacks, **RESONANCE** slider, "PHASE" block.
- **AUTO SPEED** knob (0.03-10 Hz), **INTENSITY** knob (0-10), auto/EXT selector switch, **EXT IN** jack
  (sweep CV), **P.S. OUT** jack (**not** internally connected) [M p.52].
- Specs: phase shift **1440 degrees**, centre-notch range 100 Hz-10 kHz, sweep 0.03-10 Hz, shift
  frequency fixed 1 kHz, max audio 10 V p-p [M p.53].
- Circuit: **8 first-order all-pass stages** (2 x uPC324C) with selected **2SK30A JFETs** as variable
  resistors; 8 x 180 degrees = 1440 degrees [FA-711].

**Power:** includes +14.4 V at 175 mA for the VU lamps [M p.53]. Board revisions from s/n 610066
[FA-711].

### 2.12 712A MONITOR / VOLTAGE PROCESSOR [M p.54-55] [ANZ-pd]

This module also holds the main-console DC regulators (2 x LM723) [M p.54] [YS-712].

- **MONITOR:**
  - **MONITOR LEVEL** knob and **STEREO HEADPHONES** jack (8 ohm or more), fed from 711A L/R
    [M p.54] [ANZ-pd].
  - **EXT CONTROLLER** DIN jack for a second keyboard, with a **CONSOLE / OFF** switch. CONSOLE = the
    second keyboard drives the Main Console and the main keyboard drives the other blocks.
- **VOLTAGE PROCESSOR:**
  - Slider 0-10 (+10 V fixed voltage), **+OUT** (0..+10 V), **-OUT** (0..-10 V) [M p.54-55].
  - **KEY CV OUT** and **GATE OUT** jacks carry the keyboard from the rear DIN. With no keyboard
    connected they can be used as external CV/gate *inputs* into the bus [M p.111-112] [ANZ-pd].
- **Panel size:** not printed. **131 mm wide (inferred)** from the lower-row width arithmetic.

### 2.13 713A GATE DELAY (Block 5) [M p.56-57] [ANZ-pb]

- **Controls:** GATE IN, DELAY OUT, **RISE** and **FALL** sliders (0-3 s each), red and green LEDs.
- **Specs:** threshold +5 V, output +15 V.
- **Behaviour:** the output goes high RISE seconds after the gate rises and low FALL seconds after it
  falls. If FALL + gate length < RISE, the output stays at 0.
- **Panel:** 65 x 140 mm.

### 2.14 714A (714B) INTERFACE (Block 6, 131 x 280 mm) [M p.58-59, p.113] [ANZ-p1]

- **Controls:**
  - **IN** jack, **INPUT SENS** switch **H (x1000, mic) / M / L**, **THRESHOLD LEVEL** knob,
    **VU meter**.
  - **FREQUENCY TO VOLTAGE** block, **HOLD / FOLLOW** switch, **EXT CONT** jack.
  - **F.V. OUT** x2, **ENVELOPE FOLLOWER** with **E.F. OUT** x2, **GATE SIG. GENERATOR** with
    **GATE OUT** x2, red gate LED.
- **Specs** [M p.59]:
  - Input 80 Hz-4 kHz, -60 to 0 dB. F/V 1 V/oct, output 0 to +6 V.
  - EF 0 to +10 V.
  - Gate +15 V when the VU reads above -10 dB. EXT CONT threshold +5 V.
- **Behaviour:** below -10 dB the F/V output auto-holds (internal S/H) [M p.113].
- **Tracking:** Anzai says F/V tracking is worse than Roland's later P/V synthesizer [ANZ-know].

### 2.15 715A MULTIMODE FILTERS / AUDIO MIXER (Block 6, 197 x 280 mm) [M p.60-61] [ANZ-p2] [ANZ-p3]

**Filters:**
- Four fixed (manually tuned, **not** voltage-controlled) filters in parallel, each with **LEVEL**
  slider, **RESONANCE** knob, **FREQUENCY** slider (scale 100, 200, 500, 1K, 2K, 5K, 10K Hz) and
  **LP / BP / HP** switch.
- **FILTERS IN** x2 (summed), **OUT** x2, red and green LEDs.
- Specs [M p.61]: cutoff 20 Hz-30 kHz (Anzai: 20 Hz-20 kHz), S/N >99 dB, "RESONANCE at 0: 12 dB/oct,
  at 7: 24 dB/oct". Anzai describes them as 12 dB/oct.

**AUDIO MIXER:**
- 9 sliders with 9 **AUDIO IN** jacks, **OUT** x2, LEDs.
- Unity gain, max 10 V p-p, S/N >89 dB [M p.61].
- 9 channels because a full system has 9 VCOs [ANZ-p3].

The brochure calls the filters a "fixed filter bank" [M p.8] [VSE].

### 2.16 716A MIXER (Signal/CV) [M p.62-63] [ANZ-p5]

- **Controls:** 3 input jacks with 3 gain sliders, **INVERT OUT** and **NON-INVERT OUT**.
- **Specs:** DC-20 kHz, +/-10 V max, **unity gain**, S/N >70 dB. Panel 65 x 140 mm.
- **Circuit:** 2 x LM301AH [YS-716].

### 2.17 717A ANALOG SEQUENCER (Block 3) [M p.64-65, Section 4 ~p.130-160] [ANZ-know]

**Panel, left to right** [M p.64] [PH2] [PH3]:
- **PULSE SHAPER:** OUT jacks x2, LED, **SWITCH-ON TIME** knob (0-3 sec), **THRESHOLD LEVEL** knob,
  **PULSE IN** jack.
- **MASTER CONTROLLER:**
  - Push buttons **START** and **CONTINUE** (white), **STOP** and **END** (red/orange in photos),
    with LEDs.
  - **CONTROL PULSE IN** x4 (one per function), **START PULSE OUT**, **END PULSE OUT**.
- **CLOCK OSCILLATOR:** **PULSE WIDTH** knob (10-90 %), LED, **CLOCK RATE** knob (2-20 STEP/MINUTE),
  **RANGE** switch x1 / x10, **CONTROL VOLTAGE IN**, **EXT CLOCK**, **CLOCK OUT** x2.
- **12 step columns.** Each column has:
  - three **voltage-register knobs** CH-A / CH-B / CH-C (0-10);
  - **STEP TIME DIVIDE** knob (1, 1/2, 1/4, 1/8, 1/16, 1/32);
  - **STEP TIME FINE** knob (1 to 2, "1.5" mid);
  - **NORMAL / HOLD** switch, step LED, red **MANUAL** button;
  - **ANALOG IN** and **GATE OUT** jacks.
- **PARALLEL OUT:** per channel A/B/C, a **RANGE** switch (10V / 4V / 2V) and 2 output jacks.
- **SERIES OUT:** CH A/B/C LEDs and **MANUAL** buttons, **MODE** switches **A / A-B / A-B-C / MANUAL**
  and **STOP / REPEAT** ("1"/"2" positions), **STEP SELECTOR** knob (1-12), 2 SERIES OUT jacks.

**Specs** [M p.65]:
- Steps 0-12 parallel, 0-36 series.
- Output 0-2.5 / 0-5 / 0-10 V. The panel and brochure label the range switch 2V / 4V / 10V
  [M p.7, p.64]; this is a printed discrepancy.
- Analog input +/-10 V / 17 kohm. Gate out +15 V. Clock out +15 V square.
- Ext clock >+10 V. Clock CV: 1 V doubles the rate.
- **Clock 2-20 steps/min (x1) and 20-200 steps/min (x10)**, calibrated with DIVIDE and FINE at "1".
  A step at DIVIDE 1 is a "whole note"; DIVIDE 1/32 runs 32x faster, so the effective rate is
  CLOCK x (1/DIVIDE) / FINE [M Section 4].
- Start pulse +15 V 0.5 ms. End pulse out +15 V continuous while stopped. Control pulse in +7 V.
- Pulse shaper: input 0-+10 V, on-time 0-3 s, input 100 kohm.
- Power: +23 V 150 mA, -23 V 70 mA, with its own regulator.

**Functions** [M p.7, Section 4]:
- **NORMAL/HOLD:** HOLD sustains the previous register and suppresses that step's CLOCK OUT gate (a
  rest). Per-step GATE OUT jacks still fire.
- **ANALOG IN:** each step's register knob becomes a level control for the signal patched there, so
  the sequencer works as a 12-way switch for CV or audio.
- **END/START pulses:** for tape-synced sequence chaining.
- **Step limit:** the 12-step length comes from the step-control IC being a TV channel-selector chip
  [ANZ-know].
- **Quirk:** "does not work properly when the clock frequency is too high" [ANZ-know]. Anzai skipped
  the 717A for that reason.

### 2.18 718A POWER SWITCH [M p.66-67]

- **Controls:** main **POWER** switch (in the low-voltage circuit), red pilot LED, "Roland
  SYNTHESIZER SYSTEM-700" badge [M p.66].
- **Power path:** the regulators are on the 712A, not here [M p.66] [ANZ-pf]. Transformer primary and
  a 2 A fuse sit behind the 718A/710A [M p.165]. Fuses: 2 A [M p.16].
- **Circuit:** rectifier and capacitor PSU [YS-718].

### 2.19 720B 2CH PHASE SHIFTER (Block 7, 131 x 280 mm) [M p.68-69] [FA-720]

- **Per channel:** IN, OUT, **BYPASS** push switch (ON = phase shift), **RESONANCE** knob, **SHIFT
  FREQUENCY** knob (300 Hz-6 kHz; scale 300, 500, 1K, 3K, 6K Hz), **MOD INTENSITY** knob, SHIFT FREQ CV
  jack.
- **Common:** a CV jack for both channels, **SWEEP SPEED** knob (0.03-10 Hz), beat LED.
- **CH-1 only:** **INVERTER** ON/OFF (inverts the sweep for stereo motion).
- **Specs** [M p.69]: 1440 degrees, notch range 100 Hz-10 kHz, S/N >50 dB.
- **Circuit:** 8 all-pass stages per channel (2 x uPC324C each, 2SK30A JFETs), TA7136P output
  [FA-720].
- **Quirk:** Anzai calls the S/N "not very good" [ANZ-know].

### 2.20 721A (721B) 2CH AUDIO DELAY (Block 7, 131 x 280 mm) [M p.70-71] [FA-721]

- **Controls:** same layout as 720B, with **DELAY TIME** knob (5, 4, 2, 1, 0.5 ms) in place of shift
  frequency.
- **Specs:** **delay 0.5-5 ms** (flanger/chorus range), sweep 0.03-10 Hz, S/N >45 dB [M p.71].
- **Circuit:** **MN3004 BBD** (512-stage) per channel, TC4013/TC4011 clock, 3-section RC
  anti-alias/reconstruction filters, C1458 op-amps, TA7504M LFO [FA-721]. Revision from s/n 660065
  [FA-721].
- **Quirk:** Anzai rates its S/N poor [ANZ-know].

### 2.21 723A ANALOG SWITCH (Block 7, 65 x 280 mm) [M p.72-73]

- **Controls:** 4 x **IN** (CV or audio), each with a **MANUAL** push switch (in = ON) and green LED.
  One common **OUT**. **SWITCH CONTROL SW1-SW4** jacks (+5 V or more = ON).
- **Specs:** DC-20 kHz, S/N >75 dB [M p.73].

### 2.22 Main Console internal (normalled) connections [M p.6, p.12, module pages] [ANZ]

| Source | Normalled to |
|---|---|
| Keyboard CV1 (DIN) | VCO-1/2/3 (KEY CV switch); VCF-1/2 (KEY slider). In Blocks 4/5: 702D/E and 703C only |
| Keyboard gate/trigger | All 705A trigger selectors; LFO KEY TRIG and 706A delay |
| VCO-1/2/3 WAVE FORM SELECTOR bus | VCF-1 and VCF-2 audio sliders; ring mod X/Y selectors |
| White noise | VCF-1 (703A) audio slider; ring mod Y selector; 709A SAMPLE MODE |
| Ring mod out | VCF-2 (703B) audio slider |
| ADSR-1 | VCO-2 mod; VCF/VCA ADSR sliders (select 1/2) |
| ADSR-2 | VCF/VCA ADSR sliders (select 1/2) |
| LFO-1 | VCO-1/2/3 mod sliders; VCF-1/2 mod sliders; 709A SAMPLE MODE |
| S&H out | VCO-3 mod slider |
| VCF-1, VCF-2 | VCA-1 and VCA-2 audio sliders |
| VCA-1, VCA-2 | 711A reverb sliders, PAN 1 / PAN 2, phase-shifter sliders |
| REV OUT | PAN 3 |
| 711A L/R out | 712A headphones |
| 707A AMP OUT, EF OUT | EF IN, INT IN (switching jacks) |

The phase-shifter output (P.S. OUT) is **not** normalled anywhere [M p.52]. Only keyboard CV reaches
the lower cabinets; the upper Blocks 6/7 get no keyboard CV, so the manual advises against putting
VCOs or VCFs there [M p.17].

---

## 3. Physical design

### 3.1 Panels and modules

- **Panel heights:** 280 mm (tall) and 140 mm (short) [M].
- **Panel widths:** 65, 98, 131 and 197 mm [M]. Keyboard control panel 135 x 155 mm [M p.23].
- **Mounting:** aluminium front panels screwed to aluminium module chassis, which slide into the
  block frame onto 15-pin edge connectors [M p.166].
- **Finish:** **black anodised aluminium** with white legends [PH1] [PH2] [RLM] (RL Music mentions
  oxidised anodising on aged units).
- **Equipboard's "5U ... 8.75-inch (22.225 cm) panels" claim** [EB] conflicts with the manual's
  280/140 mm and is **wrong**.
- **Panel graphics:**
  - White condensed sans-serif capitals for titles (e.g. "VOLTAGE CONTROLLED OSCILLATOR -1").
  - Signal-flow lines drawn on the panel, connecting sliders to labelled boxes ("VCA", "REV.",
    "PHASE", "S.&H.", "RING MOD.", "NOISE GEN.", "E.F.").
  - Bus inputs named in small boxed tags ([VCO1] [VCO2] [VCO3] [WHITE NOISE] [KEY CV] [ADSR1]
    [LFO1] ...).
  - Slider scales 0-5-10 with tick ladders.
  - "Roland" logo bottom-left, module number (e.g. "702A") bottom-right [M p.25-72] [PH2].
- **Knobs:** small black knobs with silver metal caps and white pointer line; scales printed on the
  panel. The PITCH knob is large and concentric (outer coarse, inner fine) [M p.25] [PH2].
  Manufacturer and part number **NOT FOUND**.
- **Sliders:** vertical (and some horizontal: PAN, GAIN, LAG TIME) with black rectangular caps
  [PH2]. A 3D-printable replacement "Fader Knob/Slider Cap for Roland SH-1, -2, -7, -09, Jupiter 4,
  System 700, Promars" exists (Thingiverse 4961266, via archive.org search), so the caps are shared
  with SH-series synths **(inferred from the title)**.
- **Switches:**
  - Small toggles with cream/white lever caps (1/10 attenuators, KEY CV, LIN/EXP, KEY TRIG).
  - Slide switches for multi-position selectors (filter mode, LFO wave, range).
  - A 4-position lever "WAVE FORM SELECTOR" on the VCOs [PH2].
- **Buttons:** sequencer START/CONTINUE white, STOP/END red-orange; sequencer step MANUAL buttons
  and ADSR MANUAL GATE red [PH2] [PH3].
- **LEDs:** red and green (overload/signal pairs on VCF/VCA/mixers; rate LEDs) [M].
- **Meters:** VU meters on the 711A (two) and 714A, lamp-lit [M p.53, p.59].

### 3.2 Jacks

- **Size: probably 1/4" (6.35 mm) mono phone jacks. Not stated in the manual.** Evidence:
  - RL Music's System 700 Lab listing: "1/4 inch jack In/Out" [RLM-LAB].
  - RL Music's full-system listing: "not supplying any 1/4" jack audio cables" [RLM].
  - Pharmasonic's clone is sold as "710 Multiple Jack (1/4")" [PS-710].
  - An Equipboard user describes "thick patch cables that resemble telephone-switchboard cords"
    [EB].
  - In the manual's scaled drawings, the 702 panel (65 mm wide) shows jack bezels about 9-10 mm
    across **(inferred)**.
  - **Recommendation:** treat as 1/4" and verify against a real unit or high-res photo.
- **Switching jacks:** 711A external inputs and 707A EF/INT inputs [M p.42, p.51].
- **Other connectors:** DIN sockets for keyboard (rear of main console; 712A front for a second
  keyboard) [M p.24, p.54]. Inter-cabinet power/bus uses 8-pin "bus plugs": -23 V, +23 V, key gate,
  key trigger, ground, key CV1, ground [M p.18].

### 3.3 Patch-point count (inferred, from the manual drawings)

Front-panel patch jacks per block, counted by me from module drawings and descriptions:

| Block | Jacks |
|---|---|
| Block 1 | about 83, plus the headphone jack and the second-keyboard DIN |
| Block 3 | about 45 |
| Block 4 | about 70 |
| Block 5 | about 56 |
| Block 6 | about 29 |
| Block 7 | about 35 |
| **Total** | **about 318** |

Add the keyboard's 5 rear jacks and DIN. No published total was found.

### 3.4 Cabinets

- **Construction:** "Up to six wooden cabinets" [SMB], covered in **black tolex** with chrome/nickel
  corner and edge hardware [RLM] [RLM-LAB] [PH1].
- **Lids and handles:** each cabinet has a removable **lid** ("dimensions with cover"; "all lids
  present", badge on the lid) [M p.12] [RLM]. A carrying handle sits on top (photo of the 717A cabinet)
  [PH4].

---

## 4. Sound character, quirks, users

### 4.1 Character (sources disagree)

- **"Fat" camp:**
  - "A very fat sounding synth" [VSE].
  - "Sheer muscle and beef" [SG].
  - "Sounds more powerful than any synth he's heard ... easily blows away the ARP 2600 and 100m" [SS].
- **"Sharp" camp:**
  - Anzai, who owned one and demonstrated it for Roland/broadcasters at about age 17: like the 100M,
    it excels at "sharp, harsh" sounds, suiting aggressive percussive sounds and edgy basses rather
    than Moog-style fatness [ANZ-know].
  - Equipboard reviewers: "punchy, metallic, hollow", "tinnier than Moog" [EB].
- **Positive overall:** "outstanding oscillators, extremely fast envelopes, very musical filters"
  [SMB]. "The filters are excellent" and the sequencer is "the best analogue sequencer I have ever
  used" [SS].

### 4.2 Quirks to model

| Quirk | Source |
|---|---|
| VCO stability: heated uA726 expo pair; 3-min warm-up spec | [M p.28] [YS-702] [SS] |
| Calibration needs a 10-min warm-up | [M p.166] |
| VCF self-oscillation from resonance about 7-8 and up; tracks 1 V/oct | [ANZ-p9] |
| 703E/F/G passband volume drops as resonance rises | [ANZ-p9] |
| 12 dB SVF resonance is diode-limited (soft clipping in the loop) | [YS-703BC] |
| Red overload LEDs at about 13 V p-p summed input, so summing mixers can be driven into clipping | [M p.17] |
| Keyboard held CV slowly droops after key release (S/H leakage) | [M ~p.165] |
| Two-voice keyboard: low/high note split, retriggers only on the lowest key | [M p.76-78] |
| 707 integrator is non-linear (detunes glides) | [ANZ-know] |
| 711A spring reverb low quality | [ANZ-know] |
| 720B/721B S/N mediocre (>50 / >45 dB spec) | [ANZ-know] [M p.69, p.71] |
| 717A unreliable at high clock rates | [ANZ-know] |
| Original patch cables tarnish | [SG] |
| Hard-wired main console means fewer patch points (seen as a drawback by one owner) | [SS] |
| Lower cross-talk and better build than ARP | [SS] |
| Build quality and durability high, few failures | [ANZ-know] [SG] |

### 4.3 Users and recordings (verification level noted)

- **Isao Tomita:** *Kosmos* (1978) and *Daphnis et Chloé / The Ravel Album* [EB] [SMB]. Wikipedia
  lists Tomita as a user [WP]. Album-level credit rests on secondary sources.
- **The Human League:** "used it extensively on *Dare*, including 'Don't You Want Me'" [SMB]. Single
  source. Martyn Ware is noted as a user [EB].
- **Visage:** "Fade to Grey" [SMB]. Single source.
- **Vince Clarke:** owns one ("Cabin Studio" gear list, Keyboard Magazine May 2012) [EB]. No specific
  album found.
- **Hans Zimmer:** owns one, with parts integrated into his custom Moog modular wall [SMB].
- **Aphex Twin (Richard D. James):** "all you really need is a system700"; owns two 717 sequencers
  [EB].
- **Flood and Alan Moulder:** System 700 in the collection of their studio Tapeworm (Polymath Perspective article, via [EB]). Flood and Ed O'Brien
  "reportedly owned two" [SG].
- **Ed O'Brien:** "Brasil" uses it as a guitar effect [SG].
- **Others listed:** Richard Barbieri / Japan (visible in the c.1978 "Life In Tokyo" promo at about
  2:45) [EB]; Giorgio Moroder, Depeche Mode, Eric Carmen, Rhett Lawrence, Matthias Becker, Paul Davis,
  Eloy Fritsch, Kim Wilde [WP]; Vangelis, Nitzer Ebb [VSE]; Daniel Miller, Chemical Brothers, VNV
  Nation, Harold Faltermeyer "Axel F" (unverified), Air Liquide [EB].
- **First large installations:** **NHK** and **BBC** [SMB] [ANZ-idx]. Roland's Product News also names
  **DIKE music studio (Canada)** and **Tokyo University of the Arts** [ANZ-etc]. Anzai adds **TBS**,
  whose first full System-700 production was a title sequence for "Pink Lady vs Candies"
  [ANZ-know].

---

## 5. Production, price, value, owners

- **Introduced:** prototype at the autumn **1975** instrument fair, sold from **1976** [ANZ-know]
  [ANZ-idx]. Wikipedia and most sources give 1976 [WP].
- **Production span (sources disagree):** 1976-1982 [VSE] [GO] [SS]; 1976-1983 (search summary of a
  listing); "about 8 years" [RLM].
- **Units made: NOT FOUND (no official figure).**
  - Dealer lore says "only around 40 full systems" [SG-auction] [RLM]. Reverb (search summary) and
    Soundgas's own blog dispute it: "I can name about half a dozen in the UK alone" [SG].
  - "Only a small number of units were produced" [AS].

**Launch prices in Japan** (Anzai, from Roland price lists; 1981 revised price in brackets)
[ANZ-idx]:

| Item | Price |
|---|---|
| Main Console | ¥780,000 (¥860,000) |
| Keyboard Controller | ¥80,000 (¥90,000) |
| Analog Sequencer | ¥280,000 (¥310,000) |
| VCO Bank | ¥450,000 (¥500,000) |
| VCF/VCA Bank | ¥350,000 (¥390,000) |
| Interface/Mixer | ¥230,000 (¥250,000) |
| 2CH Phase Shifter/2CH Audio Delay | ¥230,000 (¥250,000) |
| **7-block total** | **¥2,400,000 (¥2,650,000)** |
| Main Console System (console + keyboard) | ¥860,000 (¥950,000) |
| Laboratory System | console ¥400,000 + keyboard ¥80,000 = **¥480,000** |

- **Roland Australia:** "The full system was priced at ¥2,650,000 in Japan (roughly $9,000)" [RAU].
  That matches Anzai's *1981 revised* total, not the 1976 launch total.
- **US prices** (unverified dealer/fan figures): "$14,500 in 1979" (Reverb listing, search summary);
  "up to about $18,000" for full systems; Lab "$3,500-$4,000" [VSE].

**Market value:**
| Date | Sale | Source |
|---|---|---|
| July 2008 | eBay, full 7-block system (ex-Tom Beckman, founder of Roland US): $50,100.99 | [MX-2008] |
| April 2017 | VEMIA: bid at £76,050 | Gearspace (search summary) |
| November 2017 | VEMIA: "£110K" | [RLM] |
| February 2019 | RL Music, full system: £70,000, sold | [RLM] |
| No date | Soundgas auction, full system: listed £113,500 (another page $163,688), "SOLD OUT", no hammer price | [SG-auction] |
| No date | RL Music, Lab (Block 8 + 2): £18,600 | [RLM-LAB] |
| 2025 | Lab listing on Reverb, no price | [MX-2025] |

Anzai (1990s): worth buying at ¥1.0-1.5 M for a full set; above that, buy a Moog or Polyfusion
instead [ANZ-know].

**Known owners and holdings:**
- Science Museum London, object 1992-441 (c.1981, ex-University of Hull) [SMG].
- Hans Zimmer [SMB]; Vince Clarke [EB]; Aphex Twin (sequencers) [EB]; Flood/Moulder [EB].
- NHK, BBC, TBS, Tokyo University of the Arts [ANZ-idx] [ANZ-etc].
- One restored installation reportedly in Australia with about 129 modules across 17 blocks, nearly
  5 m wide [EB].

---

## 6. Emulations, clones, reissues: what exists

**Software (as of 2026-10-17):**
- **No commercial or official System-700 emulation was found.** Searches cover Roland Cloud,
  Cherry Audio and VCV Rack. A Cherry Audio forum thread only *requests* 702E/703C/704C modules
  [CA].
- The only System-700-specific software found is **"Rolend Block-8"**, a free, Windows-only plugin
  by **Kevin Brown (KBPlugs)**, part of the "K Brown Synth Bundle" [KB].
  - Based on the Laboratory (Block 8) configuration, with drop-down patch points and pre-wired
    connections [KB].
  - Described as "inspired by the real unit circuits but does not aim to sound accurate" [HHM].
  - Users report problems at high resonance [KB].
  - One site mislabels it a System-500 emulation [KB].
- **Roland SYSTEM-100 PLUG-OUT (2015)** emulates the *System-100* (Model 101/102), not the 700
  (Synthtopia/Mixmag, search results).
- **Roland JD-XA** has no System-700 connection.
- **Stated accurately:** "no faithful software emulation of the System-700 is known; the only
  System-700-specific plugin is the free, non-circuit-accurate Rolend Block-8."

**Roland hardware:**
- **SYSTEM-500** (Eurorack, 2015, built with **Malekko**): 512 VCO, 521 Dual VCF, 530 Dual VCA,
  540 2ENV-LFO, 572 Phase Shifter/Delay/LFO, later 505/510/531/555 [SOS500].
  - Roland's announcement: "based on two of the most revered electronic instruments of all time, the
    System-700 and System-100m" [SOS500]. Roland's product pages call it a recreation of the
    **SYSTEM-100M** [ROL572].
  - The model numbers mirror 100M modules (112, 121, 130, 140, 172) **(inferred)**. The 505 follows
    the SH-5 filter (review, search summary). The 572 is a 5-stage phaser [ROL572], whereas the 700's
    phasers have 8 stages [FA-720].
  - **Not** a 700 reissue.
- **No Roland System-700 reissue exists.**

**Third-party hardware:**
- **Behringer:** no System-700 clone found. Behringer's **System 100** Eurorack line (11 modules,
  2020) clones the **System-100M** [BEH].
- **Pharmasonic SYS-700** (Eurorack clones): VCO 702, VCF 703F (24 dB LP), VCF 703C (12 dB multimode),
  VCA 704, LFO 706, S&H 709, Mixer 716, Multiple 710 (1/4"), Phase Shifter 711 [PS] [PS-710].
- **Omnisyn (UK):** System-700-based Eurorack modules, including a "mini Roland System 700 LAB"
  (2019) [MX-omni] [MW].
- **Frequency Central "State 700":** a 700-style filter (Equipboard comment) [EB].
- **SynthCube:** also mentioned for 700 clones (forum summary) [MW].

---

## 7. AT700 modelling notes derived from the above

- **VCO:** saw-core (integrator + JFET reset) with derived triangle, sine (diode-shaped) and pulse
  (comparator, 10-90 %). Saw 0/+10, tri and sine +/-5, pulse 0/+10. Separate strong (hard) and weak
  (capacitive soft) sync. Range LO/32'-2' plus +/-12 st coarse and +/-1 st fine. A3 = 440 Hz at 8'
  and pitch 0.
- **VCF:** two models.
  - 12 dB OTA SVF, HP/BP/LP, diode-limited resonance, self-oscillating.
  - 24 dB 4-pole OTA cascade, LP, voltage-controlled resonance, with passband loss as resonance rises.
  - Cutoff 5 Hz-30 kHz, 1 V/oct.
- **VCA:** OTA, LIN (10 %/V) and EXP (10 dB/V) response, initial-gain knob.
- **ADSR:** RC exponential segments, x1 = 2 ms-10 s, x1/10 = 0.2 ms-1 s. GATE+TRIG / GATE / EXT
  modes.
- **Effects:** 8-stage JFET all-pass phasers (711A, 720B); MN3004-style 0.5-5 ms BBD (721); spring
  reverb (711A).
- **Levels:** internal standard 10 V p-p audio; overload LED at about 13 V p-p.

---

## 8. Things not found (explicit)

- An official production count.
- Knob and jack part numbers and manufacturer.
- The jack size stated in any Roland document.
- 712A and 718A panel sizes (712A inferred as 131 mm).
- Lab (Block 8) panel order and the 702F/702G roles.
- A dated changeover from 703A to 703E in the main console.
- Exact US launch price from a Roland document.
- Exact LO-range frequency.
- A Sound On Sound "Retrozone"/feature article on the System 700 (none located). A muzines review
  (none located).
- A single-file complete Service Notes PDF (Wikipedia lists "System 700 Synthesizer Service Notes"
  and "Service Supplement" as existing documents).

---

## 9. Sources

Reading copies of the copyrighted files live outside the repository (`audiotools-private/at700-ref/` on the maintainer's machine); see `ref/CREDITS.md`.

**Primary and technical:**
- **[M]** Roland, *System 700 Synthesizer Instruction Manual* (c)1976, printed Feb '77 C-2, 224 pp.
  archive.org `synthmanual_System_700_owners_manual`. Local: `system700_owners_manual.pdf`
  (+ `_text.pdf`).
- **[YS]** yusynth.net, "Roland System 700 main console", https://yusynth.net/gear/R700_MC_en.html.
  **[YS-702]** etc. = redrawn schematics `schematics/yusynth_*.pdf` (702ABCDEFG-VCO, 703B-C-VCF,
  703E-F-G-VCF, 704-ABCD-VCA, 705A-ADSR, 706A-LFO, 707-INSTR-INTERF, 708A-NOISE+RM, 709A-B-S&H,
  712A-VP-PSU, 716A-SIGNAL-MIXER, 718A-PSU).
- **[FA]** Florian Anwander, "Roland System 700 schematics", https://www.florian-anwander.de/roland_system700/.
  **[FA-711] [FA-720] [FA-721] [FA-702]** = service-note scans `schematics/anwander_*.jpg`.
- **[FA-F]** Florian Anwander, "Roland filter versions", https://www.florian-anwander.de/roland_filters/.

**Fumitaka Anzai** (owner and former Roland demonstrator), Japanese:
- **[ANZ-idx]** http://anz.yokohama/Museum/Roland/System-700/index.html
- **[ANZ-know]** http://anz.yokohama/Museum/Roland/System-700/system-700know.html
- **[ANZ-etc]** http://anz.yokohama/Museum/Roland/System-700/system-700etc.html
- **[ANZ-p1..pf]** http://anz.yokohama/Museum/Roland/System-700/system-700panel{1..f}.html

**Secondary:**
- **[WP]** Wikipedia, "Roland System 700", https://en.wikipedia.org/wiki/Roland_System_700 (raw
  wikitext, 2026-10-17).
- **[VSE]** Vintage Synth Explorer, https://www.vintagesynth.com/roland/system-700.
- **[GO]** Gearogs "Roland System-700" (Wayback 2019-07-24),
  https://web.archive.org/web/20190724051654/https://www.gearogs.com/maker/4520-roland-system-700.
- **[RAU]** Roland Australia, "Roland Synth Chronicle 1973-2014",
  https://rolandcorp.com.au/blog/roland-synth-chronicle-1973-2014.
- **[SMB]** Synthesizer Museum Berlin, https://www.synthesizermuseum.info/roland-system-700.
- **[SMG]** Science Museum Group, object 1992-441, https://collection.sciencemuseumgroup.org.uk/objects/co34122.
- **[SS]** SonicState synth site + user review, https://sonicstate.com/synth/roland_sys700m/.
- **[EB]** Equipboard, https://equipboard.com/items/roland-system-700.
- **[SG]** Soundgas blog, "Fantastic Beast" (2021-01-19),
  https://soundgas.com/blog/fantastic-beast-roland-system-700-for-sale/.
  **[SG-auction]** https://soundgas.com/products/roland-system-700-auction-2.
- **[RLM]** RL Music, full system listing, https://www.rlmusic.co.uk/rlm3/roland-system-700/.
  **[RLM-LAB]** https://www.rlmusic.co.uk/rlm3/roland-700-lab/.
- **[MX-2008]** MATRIXSYNTH, https://www.matrixsynth.com/2008/07/roland-system-700-sells-for-5010099.html.
  **[MX-2025]** https://www.matrixsynth.com/2025/08/rare-roland-system-700-modular.html.
  **[MX-omni]** https://www.matrixsynth.com/2019/12/new-mini-roland-system-700-lab-eurorack.html
  (title only, from search).
- **[AS]** Analogue Solutions blog (2025-01-24), https://www.analoguesolutions.com/blog/2025/1/24/roland-system-700
  (contains errors; used only to flag them).
- **[MW]** / **[MW-726]** Mod Wiggler threads (System 700 clone; uA726), via search summaries:
  https://www.modwiggler.com/forum/viewtopic.php?t=270952, https://modwiggler.com/forum/viewtopic.php?t=226963,
  https://www.modwiggler.com/forum/viewtopic.php?t=153796.
- **[PS]** ModularGrid Pharmasonic vendor/forum, https://modulargrid.net/e/vendors/view/710,
  https://modulargrid.net/e/forum/posts/index/8063. **[PS-710]** https://www.matrixsynth.com/2020/12/pharmasonic-710-multiple-jacks-in.html.
- **[KB]** KVR "K Brown Synth Bundle" threads (https://www.kvraudio.com/forum/viewtopic.php?p=6595146)
  and https://freevstplugins.net/rolend-block-8/.
- **[HHM]** https://hiphopmakers.com/best-free-roland-vst-emulators.
- **[CA]** Cherry Audio forum, https://forums.cherryaudio.com/viewtopic.php?t=1454.
- **[SOS500]** Sound On Sound news, https://www.soundonsound.com/news/roland-officially-announce-system-500-power-rack.
- **[ROL572]** Roland SYSTEM-500 572 page, https://www.roland.com/global/products/system-500_572/
  (via search summary).
- **[BEH]** ask.audio, https://ask.audio/articles/behringer-reveals-full-details-on-roland-system-100m-eurorack-module-clones;
  Engadget, https://www.engadget.com/2020-01-16-behringer-system-100-modular-synth-clone-namm.html.

**Photos:**
- **[PH1]** Commons "Roland System 700.jpg" (Racingline81, CC BY-SA 4.0).
- **[PH2]** Commons "Roland System 700 in Utrecht.jpg" (Chris Sobczak, CC BY 2.0).
- **[PH3]** Flickr allert 6955987065.
- **[PH4]** Flickr Michael Dewberry 5647107448 / 5646559487.
- See `CREDITS.md`.
