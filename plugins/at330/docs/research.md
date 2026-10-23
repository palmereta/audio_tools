# Vocoder and string/choir synthesizer (AT330): research

Compiled 2026-10-23 for a new Audio Tools product: a free, open-source (AGPL-3.0) recreation of the Roland VP-330
Vocoder Plus (Mk I 1979 / Mk II 1980), working name **AT330**. The series names recreations "AT" plus the original's
number (AT303 = TB-303, AT20V = MS-20, AT101V = SH-101, ATMD = Minimoog). The document covers the hardware (§1), the
products and what users say (§2), the DSP as implemented (§3), the extras beyond the original (§4), the tests and
their measured results (§5), presets and level method (§6), demos (§7) and the questions that need the schematics
or a real unit (§8).

Conventions (the same as `plugins/at303/docs/research.md`):
- Every fact carries its source inline: a tag from the table below or a URL (all accessed 2026-10-23). Where only a
  forum post, a retailer page or a third-party quote was available, the text says so.
- **(inferred)** marks our own conclusions, calculations or design choices. §3 describes what the code does (the
  implementation is the ground truth); values marked (inferred) there are starting values, not published numbers.
  "Not found" means looked for and not found in the sources.
- Product names (Roland, VP-330, XILS, Behringer, Arturia and the others) are trademarks of their owners and appear
  **only in this research document**. The product shows no brand names, logos or the original's trade dress; feature
  names are generic ("ensemble", "pitch shift", "hold").
- Nothing here copies proprietary text or code. Copyrighted references (service notes, owner's manual, scans) are
  referenced, never quoted at length, and live only in `/Volumes/Docs/Projects/own/audiotools-private/at330-ref/`.
  This document paraphrases short facts (numbers, block names, measured levels) from them.

Source tags used throughout:

| Tag | Document | Where |
|---|---|---|
| **[SM1 pN]** | Roland VP-330 Service Notes, 1st ed., dated 1979-09-21 (Mk I, boards AGH17, HVH56, ETH09, FLH16, VBH58, OPH60, JAH59). p.4 general description, p.7 block diagram, p.9 ETH09, p.10 FLH16 adjust table, p.11 VBH58, p.12 HVH56, p.13 AGH17 adjustment, p.14 pitch-shift waveforms, p.16 parts list | https://archive.org/details/Roland_VP-330_service_notes ; private copy `at330-ref/Roland_VP-330_service_notes.pdf` and OCR text (imperfect; key numbers checked against the page images) |
| **[SM2 pN]** | Roland VP-330 Service Notes, 2nd ed., dated 1980-10-25 (S/N 961450+, with the version comparison table on p.2; Mk II boards AGH19, HVH105, ETH16, FLH16 C, VBH106, OPH107) | https://archive.org/details/roland_VP-330_SERVICE_NOTES_2nd ; private copy `at330-ref/VP-330_SERVICE_NOTES_2nd.pdf` |
| **[OM pN]** | Roland VP-330 Owner's Manual, July 1980 (A-3 print) | https://archive.org/details/synthmanual-roland-vp-330-owners-manual ; private copy `at330-ref/rolandvp-330ownersmanual.pdf` |
| **[SM1 p.13]**, **[SM1 p.7 block diagram]** | Read directly from the page images by the engineer (not OCR): the AGH17 adjustment-sheet waveforms and levels, and the general block diagram | same files as [SM1] |
| **[BRIEF]** | Our sourced technical brief of 2026-10-23, from which the facts of §1-§2 are taken (it cites the primary sources above) | `at330-ref/research-notes.md` |
| **[WP]** | Wikipedia, "Roland VP-330" | https://en.wikipedia.org/wiki/Roland_VP-330 |
| **[SOS]** | Sound On Sound, "Roland VP330 Vocoder Plus" (1995) | https://www.soundonsound.com/reviews/roland-vp330-vocoder-plus |
| **[KVR]** | KVR forum thread on emulating the VP-330 (poster "omissis", technical but unverified) | https://www.kvraudio.com/forum/viewtopic.php?t=126684 |
| **[EM]** | electro-music forum thread on the ensemble (BBD count, LFO structure; schematic images dead) | https://electro-music.org/forum/post-403806.html |
| **[MT92]** | Music Technology, Feb 1992, "Vocal codes from the underground" | https://www.muzines.co.uk/articles/vocal-codes-from-the-underground/948 |
| **[GS]** | GreatSynthesizers, VP-330 review (Mk I vs Mk II) | https://greatsynthesizers.com/en/review/roland-vp-330-figurehead-of-the-vocoder-hype/ |
| **[SMEM]** | smemmusic.ch VP-330 page | https://www.smemmusic.ch/de/node/1049 |
| **[AM]** | Attack Magazine, XILS-lab V+ review | https://www.attackmagazine.com/reviews/gear-software/xils-lab-v-plus/ |
| **[SOS-X]** | Sound On Sound, XILS-lab V+ review | https://www.soundonsound.com/reviews/xils-lab-v |
| **[SYN]** | Synthtopia, XILS V launch (2013-05-28) | https://www.synthtopia.com/content/2013/05/28/xils-v-now-available-recreates-roland-vp-330-vocoder-plus/ |
| **[MTECH]** | MusicTech, Behringer VC340 review | https://musictech.com/reviews/hardware-instruments/behringer-vc340-vocoder/ |
| **[KEN]** | Kenton UK, MIDI retrofit page | https://kentonuk.com/?p=647 |
| **[SRC]** | The implementation, the ground truth for §3-§6: `plugins/at330/Source/dsp/Engine330.h`, `Source/Params330.h`, `Source/PluginProcessor.cpp`, `tools/at330test.cpp`, `tools/presets.py`, `tools/headroom.py`, `tools/demos.py`, `tools/demo_voice.py` | this repository |

---

## 0. Summary of recommendations and fact sheet

**The hardware** (§1). One master VCO, a top-octave generator of the AY-3-0214 class, six divider ICs and 49
transistor-chopper tone gates; three sections (strings, human voice, vocoder carrier) summed per section, so the
machine is paraphonic: one VCA per section, one shared release generator [SM1 p.4], [SM2 p.3]. Every key press grounds
the shared envelope capacitors for about 50 ms, so a new chord note re-attacks the whole chord, the machine's best known
fault [SM1 p.4 (via [BRIEF] §2)], [SOS]. Waveforms: the 8' output is an exponential-charge sawtooth over the whole
period with a sharp reset; the 4' output is flat for half the period, then the same charge curve, then the reset
[SM1 p.13]. Choir: a bank of 7 band-pass filters (200, 230, 600, 900, 1300, 2800, 3300 Hz), four per voice; the routing
is only in the schematic [SM1 p.12]. Vocoder: 10 analysis and 10 synthesis band-passes (200 ... 6000 Hz), PWM-chopper
VCAs at 12.5 kHz, a compander, a high-frequency consonant bypass and a Hold circuit (2-3 s) [SM1 p.10], [SM2 p.13].
Ensemble: BA662 compander plus four BBDs with LFOs of 3.9 s / 150 ms and 4.6 s / 175 ms; vibrato for choir and vocoder
is a separate pair of BBDs [SM1 p.9]. Pitch shift bends the master VCO down only [OM p.3].

**Market** (§2). XILS-lab V+ (plugin, 49 EUR on sale at the fetch time) is the only close plugin model; Behringer
VC340 (hardware) is the clone; Roland's VP-03 is discontinued; the only free VP-330-inspired plugin is the Windows-only
closed-source VSP-330. No free AGPL/open-source VP-330 plugin was found. Users ask for polyphonic attack, an ensemble
switch for strings and external input, more octaves, MIDI, and fewer fiddly installs/licences.

**The DSP** (§3, as implemented). Integer top-octave divisors 478..253 from a 2.00024 MHz clock, A4 = 440 Hz, all
notes of one pitch class phase-locked; exponential-charge 8' and truncated 4' waveforms with PolyBLEP/PolyBLAMP;
footages as in the original (strings 4' only, choir male 8'/male 4'/female 4', vocoder 8'); a Classic envelope mode that
reproduces the 50 ms dump and a Poly mode (extra, the factory default); 10-band vocoder with two cascaded 2-pole
band-passes per band, pre-/de-emphasis, Hold, consonant path; BBD vibrato and a four-BBD ensemble with the measured LFO
periods; Auto/Manual pitch shift. CPU 4.3 % of one Apple M4 core with all 49 keys held (T11).

**Extras** (§4). Poly envelopes, velocity, Freeze, formant shift, vocoder response, external carrier, bend up, mod-wheel
vibrato, plate reverb, split point, ensemble depth, strings ensemble switch; 44 automatable parameters.

**Tests** (§5). 16 engine/processor tests (T1-T16), all passing, plus athosttest (VST3: 0 FAIL), pluginval strictness 10
(VST3 and AU), auval; one known AU-only item pending for phase 3.

**Presets and demos** (§6-§7). 45 presets in five categories, levelled to about -16 dBFS RMS with peak <= -1.5 dBFS;
12 character clips and two sketches, with a synthetic voice written for the project.

**Biggest open item** (§8): the per-voice assignment of the 7 choir filters, their Q and the waveform network values;
everything else is documented or measured.

---

## 1. The hardware

### 1.1 Identity, specifications and history

| Item | Value | Source |
|---|---|---|
| Name, years | Vocoder Plus, 1979 (Mk I) and 1980 (Mk II) | [SM1], [SM2], [WP] |
| Units built / price | about 2,000 built; USD 2,695 in 1979 (UK GBP 1,100-1,143) | [WP]; [SMEM] |
| Keyboard | 49 keys, C to C (4 octaves), no velocity, no aftertouch | [OM p.10], [SM1 p.2] |
| Keyboard split | UPPER/LOWER between middle C and the B below | [OM p.2] |
| Tuning procedure | "A3 key pressed -> 442 Hz at the output jack" (VR1A on AGH17) | [SM1 p.13] |
| Tune knob | +/-50 cents | [OM spec], [SM2 p.1] |
| Master VCO stability | about +/-15 cents over 0-40 C; varicap-tuned, shiftable one octave by varying the voltage | [SM1 p.4], [SM2 p.3] |
| Mic head amp | +20 dB (board JAH59) | [SM1 p.5], [SM2 p.4] |
| External carrier | EXT SYNTH IN, INPUT LEVEL knob and red/green LED; the manual advises a sawtooth or about 10 % pulse, filter open, ADSR as plain gate | [OM p.7-8, p.10] |
| Outputs | MONO and STEREO jacks, level switch H/M/L; stereo headphone out | [OM p.10], [SM1 p.2] |
| Pedal inputs | two: vocoder Hold and pitch-shift external trigger | [OM p.10] |
| MIDI | none (a retrofit kit existed: notes 36-84, bends down only) | [KEN] |
| Absolute note range | not stated in Hz; C2-C6 at 8' is our reading of the key labels | (inferred) from [SM1 p.13] |

### 1.2 Mk I and Mk II

The 2nd edition compares the two versions [SM2 p.2]: keyboard SK-191B -> SK-361A; rocker switches -> push switches with
LED; generator AGH17 -> AGH19; ensemble ETH09 (SAD512D / SAD1024A BBDs) -> ETH16 (MN3004 512-stage / MN3009 256-stage);
vocoder filter FLH16 A -> FLH16 C ("require different adjustment"); human voice HVH56 -> HVH105; vibrato VBH58 ->
VBH106; pitch OPH60 -> OPH107 (identical circuit); late addition OPH118 (an anti-clipping circuit when mic voice and
keyboard formants coincide) and R51 on VBH106 39 k -> 22 k for 3 dB more mic-amp headroom [SM2 p.2, p.13]. Kenton says
the functions are identical and the internal wiring differs [KEN]; a repairer found an S/N 870926 unit whose boards
matched neither edition, so revisions overlap (https://forum.moogmusic.com/t/roland-vp-330/9672). Subjective: Mk I is
"more direct and rougher", Mk II "airier" and cleaner [GS], and Sonicstate users say Mk I warmer
(https://www.sonicstate.com/synth/roland_vp330/). **The AT330 models the Mk I numbers**, because the Mk I notes are the
ones read in detail (LFO periods, filter table, levels); the Mk II HVH105 filter table and ETH16 LFO periods were not
transcribed (§8) (inferred).

### 1.3 Panel and controls

Sources: [OM p.2-8, p.10], [SM1 p.2], summarised in [BRIEF] §1.2.
- **Strings**: tablets UPPER 4' and LOWER 4' (only 4' exists), ATTACK and TONE sliders.
- **Human voice** (the choir): tablets FEMALE 4', MALE 8', MALE 4', MALE 8'. Per half: UPPER has male 8' and female 4';
  LOWER has male 4' and male 8' ([MT92] gives the same split). Own ATTACK slider.
- **Vocoder**: tablets UPPER 8', LOWER 8', ENSEMBLE; TONE slider; MIC LEVEL with an LED (red lights "only rarely").
- **One shared RELEASE** slider for all three sections [OM spec p.10], [SM1 p.2]. Attack pots: 1 MOhm (strings) and
  100 kOhm (human voice) per the OCR of [SM1 p.12]; treat the assignment as approximate.
- **Ensemble**: a tablet in the human-voice block and one in the vocoder block; the strings always pass through it
  ("non-switchable on the string sound" [SOS]; "Always on in the Strings section" [KVR]).
- **Vibrato** (choir and vocoder only, not strings): RATE, DEPTH, DELAY TIME [OM p.5, p.10].
- **Balance**: STRINGS, HUMAN VOICE, DIRECT MIC level sliders [OM p.7, p.10].
- **Pitch shift**: slider, PITCH SET knob ("more than one octave"), TIME knob, mode switch AUTO / OFF (external) /
  MANUAL; works on all three sections [OM p.3], [SM1 p.4-5], [KEN].
- Master volume, phones volume, tune, power.

### 1.4 Signal flow

From the block diagram [SM1 p.7 block diagram] and the text of [SM1 p.4]:
- **AGH17**: the master VCO feeds a divide-by-2 and the top-octave generator, then six divider ICs for the lower
  octaves, then 49 tone gates (a chopper transistor per key). The key gates feed four buses: U4', U8', L4', L8'
  (upper/lower half, 4' and 8'). Key contacts are sensed on a bus bar by a key-trigger detector.
- **HVH56**: switches U Female 4', U Male 8', L Male 4', L Male 8' feed a shared bank of 7 "Human Voice Tone Filters"
  (each filter receives several voice lines), then the Human Voice VCA with its own attack envelope. The strings go
  Strings Tone Filter -> Strings VCA (strings envelope with attack) -> Tone control. A shared Release Envelope
  Generator serves all of them.
- **VBH58**: mic pre-amp; high-consonant circuits (three high-pass filters in series) through an analog switch gated by
  the carrier ("musical sound") detection; the vibrato LFO with its delay controller; the vocoder carrier is the
  Upper/Lower 8' mixer plus the external synth input.
- **FLH16**: compressor -> 10 analysing filters -> amplitude detectors (with Hold) -> VCAs of the 10 synthesizer
  filters -> expander.
- **ETH09**: per-section compressors (vocoder, human voice, strings); two vibrato BBD delay lines clocked by a
  high-frequency VCO modulated by the vibrato LFO; low-pass filters; "Inv" blocks; then four BBD delay lines each with its
  own high-frequency VCO (the clock), driven by two LFO blocks (each a slow triangle plus a fast wave); expanders; two
  mixes CH1 and CH2.

### 1.5 Tone generation

- **Divider chain.** Master VCO -> IC7 (divide by 2) -> IC8 (12 top-octave notes) -> IC1-IC6 (lower octaves); parts
  list: AY-3-0214 type, TC4013BP, TC4520BP, CD4066-type gates [SM1 p.16]. 12 notes x 4 octaves + 1 = 49 keys. The master
  frequency in Hz is **not found**; a top-octave chip needs a clock in the low MHz, about 2 MHz (inferred, [BRIEF] §2).
- **Paraphony.** All 49 notes sound at once; there is no per-key envelope. When a key is pressed, a ground is applied to
  Q155 for about 50 ms, fully discharging the shared envelope capacitors before a new envelope voltage develops
  [SM1 p.4]. The adjustment sheet shows the effect: TP7A holds the shared envelope at "off" for 50 ms after each key-on,
  then attacks; release follows key-off [SM1 p.13]. Consequence: a new note re-attacks the whole chord, and a held chord is
  cut by each added note [SOS], [AM].
- **Release.** One Release Envelope Generator (Q11-Q13 Mk I, Q1-Q3 Mk II) holds a control voltage while any key is down and
  decays after release, discharging the tone-gate capacitor through D1 [SM1 p.12]. Time range in seconds: not found
  (release pot 10 kA; attack pots 100 kA and 1 MA per the HVH105 silkscreen in [SM2]).
- **Waveforms.** [SM1 p.13] (AGH17 adjustment sheet, read from the page image): the 8' output is an exponential-charge
  sawtooth over the whole period with a sharp reset; the 4' output is flat for half the period, then the same charge curve
  in the other half, then the reset ("truncated"). Measured levels per octave, C to C: 8' 2.9, 2.4, 2.0, 1.8 V; 4' 1.5,
  1.6, 1.6, 1.6 V. A KVR poster described the voice section as an "8' inverse truncated exponential sawtooth" and a "4'
  inverted exponential sawtooth" [KVR] (unverified; it matches the page image only in part). The network values that make
  the curve are not transcribed (§8).
- **Footages.** Strings 4' only; human voice male 8', male 4', female 4'; vocoder carrier 8' [OM p.2-8]. 4' and 8' are
  different divider taps one octave apart, selected per half by the tablets (U8'/L8' go to the vocoder carrier input,
  "u/m 4' out" and "8'" outputs appear on the AGH page) [SM1 p.7 block diagram].
- **Tuning.** A3 key gives 442 Hz at the output [SM1 p.13]: the original was tuned slightly sharp of 440.
- **Strings** are the RS-505 string section with only 4' (attack and release adjustable, TONE brightens) [SMEM], [GS].

### 1.6 Choir (human voice)

Seven band-pass filters on HVH56 with centre frequencies F1 200, F2 230, F3 600, F4 900, F5 1.3 kHz, F6 2.8 kHz, F7
3.3 kHz [SM1 p.12, page image]. Each of the four voice settings uses four of them [SMEM], [WP]; which four is in the HVH56
schematic and is not in any text (§8). XILS describe 14 filters for the choir and three for the strings [SYN]; [KVR] says
"two serial band-pass filters". Filter Q: not found. Without the ensemble the choir is "weedy" [SOS] and "Mellotron-like"
with it [MT92].

### 1.7 Strings filter

A tone filter then a tone control. The strings are a divider sawtooth/square-like signal through a TONE low-pass, always
through the ensemble [SMEM], [SOS]. XILS use three filters for the strings [SYN]. Cut-off range of TONE: not found.

### 1.8 Vocoder

[SM1 p.3-5, p.10-11], [SM2 p.3-4, p.13-15], summarised in [BRIEF] §5.
- **Bands.** 10 analysis and 10 synthesis band-pass filters ("with a high Q"; state-variable op-amp type). Centre
  frequencies from the FLH16 adjust table [SM1 p.10]: 200, 280, 400, 600, 900, 1300, 2000, 2800, 4000, 6000 Hz; the notes
  give the analysis range as 170 Hz - 7 kHz. A commonly quoted list (150, 220, 350, 500, 760, 1.1 k, 1.6 k, 2.2 k, 3.6 k,
  5.2 kHz [SMEM]) disagrees; its origin was not found, so the Roland table is used. Q and slope: "high Q" only.
- **Flow.** Mic -> head amp (+20 dB) -> compressor -> pre-emphasis -> analysis bank -> peak/amplitude detectors -> the VCAs of
  the synthesis bank; carrier -> synthesis bank -> VCA -> smoothing band-pass -> expander -> mix. A "musical sound
  detector" mutes the synthesis output when no carrier is present and enables the bypass filter. The synthesis bank is
  de-emphasised to complement the pre-emphasis [SM2 p.13].
- **Envelope followers.** Rectified and peak-detected per band; attack/release without Hold: not found.
- **Hold.** A pedal opens the Hold circuit: 50 ms later a transistor shunts the detector input and a FET turns off so the
  discharge is lengthened; the voltage is held "for 2-3 s" [SM1 p.4].
- **Synthesis VCA = PWM chopper.** Each VCA is a FET switch whose duty is set by comparing the detector DC with a triangle
  tuned to 80 us per cycle (12.5 kHz) [SM2 p.13] (FLH16 adjust step 5, TP-18). Gain is linear in duty.
- **Consonants.** No noise generator is documented. A "high consonant circuit" (VBH58/VBH106), three high-pass filters in
  series on the mic amp output, passes only high frequencies straight to the output, gated so it passes only when the
  vocoder is "ready" (carrier present) [SM1 p.7 block diagram], [SM1 p.11]. [MT92] mentions a "blue noise sibilant
  generator" that the notes do not confirm. Cut-off: not found.
- **Carrier.** The internal 8' (UPPER/LOWER tablets) or EXT SYNTH IN, summed on VBH [OM p.8]. The strings and choir do not
  feed the vocoder.
- **Direct mic** can be mixed in. Vibrato, pitch shift, release and ensemble apply to the vocoder [OM p.5-6].
- **Dynamic range.** XILS found an early build "leaked too much unmodulated carrier" and added a per-band minimum level
  [SOS-X], which suggests the real VCA has a very high on/off ratio.

### 1.9 Vibrato

VBH58/VBH106: the LFO TP-4C is a sine with T = 220 ms at the Rate pot at 0 and T = 90 ms at 10, about 4.5 to 11 Hz
[SM1 p.11], [SM2 p.9]. DELAY TIME fades the vibrato in after key-on: about 0.7 s at 0 to 2.2 s at 10 [SM1 p.11 waveform
notes]. DEPTH is not given in cents: it is a BBD clock modulation (the clock pulse width goes from 8/12 us at depth 10 to
symmetric at 0) [SM1 p.9]. Vibrato applies to human voice and vocoder only [OM p.5].

### 1.10 Pitch shift

OPH60/107 (the same envelope generator as the RS-505) produces a control voltage that bends the master VCO **down**; the
range is "more than one octave" and set by PITCH SET [OM p.3], [SM1 p.4-5]. AUTO: a key press starts the glide from the
PITCH SET pitch back up, with TIME as the decay; MANUAL: the slider bends down, PITCH SET scales the maximum; OFF/EXT: a
pedal triggers it. Waveform points of about 11, 22, 118, 130, 160 ms are on [SM1 p.14] (OCR unclear), so there is no
clean TIME range.

### 1.11 Ensemble

Sources: [SM1 p.9], [SM1 p.16], [SM2 p.2, p.14-15], [BRIEF] §4.
- **Structure.** Each of the three sections has its own compressor (BA662A-based, trimmed to 18 Vpp), then BBDs, then an
  expander. Two channels (CH1, CH2) with separate expanders. Four ensemble BBDs ("2 parallel BBDs per channel" [WP]);
  vibrato uses a separate pair.
- **BBDs.** Mk I SAD512D and SAD1024A [SM1 p.16]; Mk II MN3004 (512 stages, 2.56-25.6 ms, S/N 85 dB, clock 10-100 kHz) and
  MN3009 (256 stages, 0.64-12.8 ms, S/N 88 dB) [SM2 p.14].
- **Clock.** The BBD clock pulses are about 5 us + 15 us, a period of about 20 us, about 50 kHz at depth 0 [SM1 p.9].
  Nominal delay = N / (2 x clock): 2.56 ms for 256 stages, 5.12 ms for 512 stages at 50 kHz (calculated, inferred); the
  actual mean delay is not stated.
- **LFOs** (ETH09, page image of [SM1 p.9]): two slow triangles T = 3.9 s (TP13E) and T = 4.6 s (TP15E), +/-10 V; two
  faster sines T = 150 ms (TP14E, 7.2 Vpp) and T = 175 ms (TP16E, 8.0 Vpp). The periods are unequal, so they are not
  locked and not the classic 0.6 Hz / 6 Hz pair at 120 degrees. Phases: not found. One slow plus one fast per stereo side
  matches the descriptions in [EM] (inferred).
- **Dimension-D similarity.** Forum posters say the ensemble is Dimension-D-like with four BBDs, no feedback, straight and
  inverted outputs mixed [EM], [KVR]. The "Inv" blocks of the diagram agree (inferred).
- **Compander** around the BBDs: BA662A compress/expand pair; time constants not found.
- **Stereo.** The ensemble gives a stereo spread; the two halves cannot go to separate outputs [OM p.1], [MT92].

---

## 2. Products and what users say

### 2.1 Competition (verified 2026-10-23 as far as the tools allow)

| Product | Type | Price | Platforms | Status | Source |
|---|---|---|---|---|---|
| XILS-lab XILS V+ | Plugin, closest VP-330 model: 10-band vocoder, 49-voice strings and human voice, 4+2 BBD ensemble model | 49 EUR sale (179 EUR list) at fetch; launch 99-149 EUR | macOS 10.13+ (Intel/ARM) VST2/3, AU, AAX, NKS; Windows VST2/3, AAX, NKS; no standalone [SOS-X]; eLicenser or iLok | Current, v1.6.1 | https://www.xils-lab.com/store/xils-vp/, [SYN], [AM] |
| Behringer VC340 | Hardware clone, analog, 37 velocity keys, USB/MIDI, BBD chorus | USD 599 at launch (April 2019); USD 366 at Thomann US at fetch | n/a | Available, long lead times | https://www.thomannmusic.com/behringer_vocoder_vc340.htm |
| Roland VP-03 | Boutique hardware, digital model, no keys, gooseneck mic | not listed | n/a | Discontinued | https://www.roland.com/global/products/vp-03/ |
| Roland Cloud VP-330 | Searches found no evidence that it exists | - | - | not found | search |
| Arturia Vocoder V | Multi-model vocoder; not a VP-330 clone | 149 EUR | Win/macOS, VST/AU/AAX/standalone | Current | https://www.arturia.com/products/software-instruments/vocoder-v/overview |
| VSP-330 (G-Storm) | Freeware synthesis plugin, quad-delay ensemble, VP-330 "inspired" | Free | Windows only, VST + standalone | Not updated since 2011 | https://www.kvraudio.com/product/vsp-330-by-g-storm-plugins |
| Full Bucket Vocoder | Free, modelled on a different vocoder, 20 bands | Free | Win, macOS | Not a VP-330 clone | https://sonicstate.com/news/2019/12/04/free-vocoder-for-mac-and-windows/ |
| Sample packs | Kontakt/Logic libraries | GBP 24-26 | - | - | https://samplesfrommars.com/pages/free-vp330-from-mars |

Gap: **no free AGPL or open-source VP-330 plugin, and no free macOS/Linux option, was found** (the only free one is the
Windows-only closed-source VSP-330). Retailer prices change daily; the figures are those of the pages fetched.

### 2.2 What users complain about and ask for

Original hardware [SOS], [AM], [GS]:
- One shared envelope: only single lines or simple chords work; a new note re-attacks the whole chord; "Attack only in full
  length if all keys released".
- Long release; strings "pleasant but tinny"; "weedy" without the ensemble; limited pitch-shift range; poor value.
- Fragile Mk I rocker switches; "myriad of small background noises" [GS].

XILS V+ [SOS-X], [AM], and https://www.musicradar.com/reviews/tech/xils-lab-xils-v-582822 (as summarised by search):
price against limited scope; complex routing; eLicenser/iLok; effects boosting the level (phaser about 7 dB, reverb up to 10
dB); vocoder sensitive to noise; no standalone; polyphonic attack was the thing users wanted, and was delivered.

Behringer VC340 [MTECH], https://www.thomannmusic.com/behringer_vocoder_vc340_reviews.htm: only 3 octaves (a fourth
wanted); the paraphonic envelope cuts the previous chord's release; "crude" pitch bend; quality-control complaints; an ensemble
delay "ran too fast", and users wanted an ensemble on/off for the strings and the external input.

VSP-330 [KVR product page]: GUI failing to load in some hosts; ensemble noise reduced by lowering the chorus amount (from a
search summary; the forum thread itself returned HTTP 403).

Generic wishes drawn from the reviews, not from forum scrapes: polyphonic attack, a per-voice envelope, ensemble on/off for the
strings, separate outputs per section, MIDI, a wider pitch-shift range. Forum sweeps (Gearspace, ModWiggler) were blocked,
so this list is thinner than for AT303 (§8).

**Consequence for AT330 (inferred).** The product answers each recurring request with a switch that is off or on without
changing the original's sound (§4), keeps the original's behaviour available (Classic envelopes), and carries no licence
mechanism.

---

## 3. DSP design as implemented

All of §3 describes the code in `plugins/at330/Source/dsp/Engine330.h` [SRC]. The engine has no JUCE dependency, runs sample
by sample, takes events at their exact sample and does not depend on the block size (T9).

### 3.1 Divider and tuning

- Twelve integer divisors for the top octave, C8..B8: 478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253, from a
  clock of 2.00024 MHz (the AY-3-0214 / MK50240 family). The exact table of the AY-3-0214 is **inferred** to match; the
  integers are the ones that give the 12 equal-tempered ratios to better than 2 cents.
- The table is calibrated so that the top A (divisor 284) lands on exactly 7040 Hz, which makes A4 = 440 Hz. The original was
  tuned to 442 Hz [SM1 p.13]; the Tune control (+/-50 cents) covers it. A deliberate choice: a plugin is played against
  other 440 Hz instruments (inferred).
- Every note of a pitch class is derived from one counter of that class, so all octaves are phase-locked, as in a real
  divider chain. Measured worst deviation: -1.96 cents (T1).
- The rest state: after about 21 ms of silence into the BBDs with no keys (1024 samples at 48 kHz, longer than the longest
  delay line), the 12 counters and all LFO phases return to fixed values (a fixed offset per pitch class, so a chord does not
  start with every edge together) and stay still. A render after a silence is therefore reproducible (needed for
  bit-identical bounces and for the host test's state check, T9, T13, T14); it is inaudible because the lines are empty
  (inferred).

### 3.2 Waveforms

- 8': `y = (1 - exp(-k x)) / (1 - exp(-k))` over the whole period, then a reset. 4': 0 for the first half period, the same
  charge curve over the second half, then the reset. Both unipolar.
- Curvature k = 1.8, **inferred from the drawing** in [SM1 p.13] (the component values are not known).
- Anti-aliasing: PolyBLEP at the reset and PolyBLAMP at the slope breaks (the 4' mid-period break and the derivative
  change at the reset), so no table is needed. Aliasing is more than 60 dB down (61 dB on C6 at 4', T2).
- Level per octave: the measured table of [SM1 p.13] (8' 2.9, 2.4, 2.0, 1.8 V; 4' 1.5, 1.6, 1.6, 1.6 V), interpolated in
  octaves and normalised to 2.9 V. The code carries a fifth entry (8' 1.7 V, 4' 1.6 V) for the top key above C6, which is
  **extrapolated** (inferred). C2 = MIDI 36.
- Each bus is AC-coupled (a one-pole DC blocker at 18 Hz) because the waveforms are unipolar.

### 3.3 Footages and split

Strings: 4' only. Choir: male 8' (both halves), male 4' (lower half), female 4' (upper half). Vocoder: 8' (both halves).
Each tablet is a switch per half, as in the original [OM p.2-8]. The split defaults to C4 (MIDI 60), as the manual's "middle
C" boundary [OM p.2]; the split note is adjustable (extra). Key range played: any MIDI note 0-127 (a note above or below the
original's 49 keys uses the next divider octave).

### 3.4 Envelopes

- **Classic** mode reproduces the measured behaviour: one envelope per section, shared by all keys. Each key-on forces the
  section envelopes to zero for 50 ms (time constant 3 ms) and then attacks again, so the whole chord re-attacks. The first
  note also waits 50 ms (the TP7A trace of [SM1 p.13]). T3: a second key drops the level by about 83 dB versus 20 dB before.
- **Poly** (extra) gives each key its own gate per section (strings, choir, vocoder carrier), so notes added to a held chord do
  not disturb it. Poly is the **factory default**, because the dump cuts held chords, which is what every review complains about
  (§2.2). The original is one choice away (and preset "Vintage Strings").
- **Release** is a single parameter, 0.02-8 s, defined as the time to reach -40 dB (the exponential constant is chosen as
  4.6 / T). T3: 2.0 s setting gives -40 dB at 1.99 s. Shared as in the original.
- **Attack ranges**: strings 5 ms-4 s, choir 5 ms-1.5 s, **inferred** from the pot values (1 MOhm vs 100 kOhm) since the
  original range in seconds is not found. The vocoder carrier gate uses a 2 ms attack.
- Velocity (extra) scales the key's gain, in Poly mode only.

### 3.5 Choir

Seven fixed band-pass filters (2-pole state-variable, unity gain at the centre) at 200, 230, 600, 900, 1300, 2800 and 3300 Hz
[SM1 p.12]. Each voice feeds a subset with a weight (`kChoirMix`):

| Voice | 200 | 230 | 600 | 900 | 1300 | 2800 | 3300 |
|---|---|---|---|---|---|---|---|
| male 8' | 1.0 | - | 1.0 | 0.8 | - | 0.35 | - |
| male 4' | - | 0.8 | 1.0 | - | 0.7 | 0.35 | - |
| female 4' | - | 0.5 | - | 1.0 | 0.8 | - | 0.4 |

Q per filter (`kChoirQ`): 2.5, 2.5, 5, 5, 5, 6, 6. **The assignment, the weights and the Qs are inferred** (the seven
centre frequencies are documented; which four serve which voice is only in the schematic; the Q is not found). The design
intention is a male formant set (low first formant, F2 near 1 kHz) and a female set shifted up, with the 4' footage supplying
the octave. **This is the first thing to verify against the HVH56 schematic** (§8). T4 measures the consequence: the
energy ratio of 750-1600 Hz to below 750 Hz is -4.1 dB for the female 4' voice and -11.7 dB for the male 8' voice.

### 3.6 Strings

Fixed tone filter (inferred): a 2-pole high-pass at 140 Hz plus a broad peak at 1.5 kHz (Q 1, added at half level), then the
VCA with the strings envelope, then TONE: a 2-pole low-pass sweeping 1.2-12 kHz (1200 x 2^(3.3 x tone) Hz; the range is
inferred, the original's is not found). Strings always go through the ensemble in the original; here a switch (extra, §4).

### 3.7 Vocoder

- **Analysis bank**: 10 bands at 200, 280, 400, 600, 900, 1300, 2000, 2800, 4000, 6000 Hz (the FLH16 table, not the dealer
  list, §1.8). Each band is two cascaded 2-pole band-passes with Q = 3.2 (the text says only "high Q"; 3.2 and the cascade are
  **inferred**).
- **Pre-emphasis** `1 - 0.6 z^-1` on the mic and the matching de-emphasis `y + 0.6 y[n-1]` after the synthesis sum (coefficient
  inferred; the notes only say there is a pair).
- **Detector**: full-wave rectifier and a peak follower, attack 1.5 ms, release = Vocoder Response (3-300 ms, default 40 ms; the
  parameter is an extra, the original's release without Hold is not found).
- **Synthesis bank**: the same 10 centres (moved by the Formant Shift extra), the same two-cascade topology, driven by the
  carrier and multiplied by the band's envelope. The PWM-chopper VCA is modelled as **linear gain**; the chopper ripple at
  12.5 kHz is not modelled (inferred; it is inaudible after the smoothing filter).
- **Hold**: the detector release becomes 2.5 s (documented 2-3 s [SM1 p.4]). **Freeze** (extra): the release is infinite, so
  each band keeps the loudest level it has heard until Freeze is switched off.
- **Consonants**: three 2-pole high-pass filters in series at 4.5 kHz (the cut-off is inferred), gated by a carrier detector
  (a 20 ms envelope of the carrier above 2e-4, smoothed) so the path is silent without a key, as in the original's "musical
  sound" gate; a level control sets the amount. **Direct Mic** mixes the dry mic.
- **Tone**: a 2-pole low-pass on the vocoder output, 1.5-15 kHz (inferred range).
- **Carrier**: the keys (vocoder 8' tablets), the external input (the right input channel; extra), or both. The left channel is the
  mic.
- **Not modelled** (inferred, possible future work): the compander around the vocoder and around the BBDs. It is net linear for
  steady levels; the original's time constants, which would give its characteristic pumping, are not found.

### 3.8 Vibrato

A BBD delay modulation, not oscillator FM: a delay line of 2.56 ms (256 stages at about 50 kHz), modulated by a sine of 4.5-11 Hz
(measured T = 220-90 ms [SM1 p.11]), maximum excursion 0.6 ms (inferred), depth 0-100 % (150 % with the mod wheel). A delay of 0-3 s fades the vibrato in after the first key-on (the
original's range is 0.7-2.2 s; the wider range is an extension); legato playing keeps the vibrato running. A mod wheel (CC1) adds
depth (extra). Two separate lines carry the choir and the vocoder, as the original has two vibrato BBDs [SM1 p.9]; the strings
bypass it.

### 3.9 Ensemble

- Four BBD lines of base delay 5.12 ms (512 stages at about 50 kHz), each with an anti-alias filter at 9 kHz and a
  reconstruction filter at 8.5 kHz, a soft saturation (`tanh`), and a noise floor of about -96 dB.
- Two LFOs per side, each the sum of a slow triangle and a fast sine with the measured periods: left 3.9 s and 150 ms, right 4.6 s and
  175 ms [SM1 p.9]. T6 measures them back: 3.89 s / 150 ms and 4.60 s / 175 ms.
- Excursions: slow triangle 1.2 ms, fast sine 0.15 ms (**inferred**, by ear against recordings, since the notes do not give the depth).
- Each output channel sums two lines whose delays are modulated in **opposite directions** (inferred from the "Inv" blocks of the
  diagram and the Dimension-D-like descriptions, §1.11). Left = lines 0 and 1 (+mod0, -mod1); right = lines 2 and 3 (-mod0,
  +mod1). L/R correlation 0.35 (T6).
- Strings, choir and vocoder go through it as the original; the choir and vocoder have a switch, the strings too (extra). Whatever
  bypasses the ensemble is added dry to both channels. Ensemble Depth scales the excursions, 0-150 % (extra).
- The compander around the BBDs is not modelled (§3.7).
- The output has a DC blocker at 8 Hz, since the BBD saturation of asymmetric signals leaves a little DC.

### 3.10 Pitch shift

Auto: each key-on sets the pitch offset to Pitch Set semitones **below** (0-15) and it glides back to zero with a time constant of
Pitch Time (10 ms-2 s). Manual: a slider bends down by `amount x Pitch Set`. Off: nothing. The shift applies to the master
counters, so it affects all sections as in the original. T8: Auto -12.00 st at key-on, -0.60 st after 300 ms. MIDI pitch bend moves
the master up and down by Bend Range (0-12 st, default 2; the up direction is an extra, the original bends down only).

### 3.11 MIDI

Notes (velocity used only in Poly with Velocity > 0), CC64 sustain, CC1 mod wheel, pitch bend, CC120 all sound off, CC123 all
notes off and reset controllers. No MIDI output. [SRC `PluginProcessor.cpp`]

### 3.12 Parameters

44 automatable parameters: 28 floats, 11 switches (strings upper/lower, four choir tablets, two vocoder tablets, three ensemble
switches), and choices for Hold (Off/Hold/Freeze), Carrier, Pitch Shift Mode, Envelopes (Classic/Poly) and Split (C2..C6, default C4). The total
is below 64, the number Live shows in its automatable panel without a configure step. Ids are stable because presets and saved
state depend on them. [SRC `Params330.h`]

---

## 4. Extras beyond the original

Everything the original did not have, each marked "extra" in the code and the UI documentation:

| Extra | What it does | Why (inferred) |
|---|---|---|
| Poly envelopes (default) | Per-key attack per section; the original's single envelope stays as Classic | The most common complaint [SOS], [AM], [MTECH] |
| Velocity | 0-100 % scaling in Poly | A MIDI keyboard has it |
| Freeze | The vocoder bands never decay | Hold is limited to 2-3 s |
| Formant Shift | -12..+12 st on the synthesis bank | Creative range, as XILS added |
| Vocoder Response | 3-300 ms detector release | Sets the machine-voice to smooth range |
| External carrier | Right input channel, level control, keys + external | Same as the hardware's external input, made explicit |
| Bend up | Pitch bend up and down with Bend Range | The original bends down only; the retrofit's bend was called "crude" [MTECH] |
| Mod-wheel vibrato | CC1 adds depth | Plays like a controller |
| Space | Plate reverb on the stereo output | No reverb in the original; the demos need a hall |
| Split point | Any note C2-C6 | The original is fixed at middle C |
| Ensemble Depth | 0-150 % | VC340 owners found the ensemble too fast [MTECH] |
| Strings Ensemble switch | Takes the strings off the ensemble | The most repeated wish [MTECH], [KVR] |
| Vibrato Delay up to 3 s | Beyond the original's 0.7-2.2 s | Margin |
| Volume, Tune, Mic Level, External Level | Gain staging | Needed in a plugin |

Note on defaults (inferred): the factory sound is the original's machine with its ensemble; the extras are off or neutral except
Poly, which is on.

---

## 5. Test plan and results

The tests are in `plugins/at330/tools/at330test.cpp` (T1-T11 engine only, T12-T16 on the complete processor without a host). They
print what they measure and exit 1 on a failure. Latest run: **all pass**.

| Test | What it checks | Measured |
|---|---|---|
| T1 Divider | Pitch of every note against equal temperament; octaves phase-locked | Worst deviation -1.96 cents; A3 on the strings = 440.00 Hz |
| T2 Waveforms | 4' flat for half the period; 8' charge curve; aliasing | 4' flat 50 %; 8' mid value 0.711; aliasing 61 dB down on C6 at 4' |
| T3 Envelopes | Classic dump and re-attack; Poly unchanged; release time | Classic second key about -83 dB versus -20 dB before (dump); Poly unchanged; release 2.0 s -> -40 dB at 1.99 s |
| T4 Choir | The formant bank and the three voices | Female 4' energy 750-1600 Hz versus below 750 Hz: -4.1 dB; male 8': -11.7 dB |
| T5 Vocoder | 10 bands at their centres; Hold, Freeze; consonants | 10/10 bands; 2 s after cutting the voice: no hold -194 dB, Hold -7.1 dB, Freeze 0.0 dB; consonant noise -32.9 dB with a key versus -103 dB without |
| T6 Ensemble | The four LFO periods; stereo decorrelation | 3.89 s / 150 ms and 4.60 s / 175 ms; L/R correlation 0.35 |
| T7 Vibrato | Rate range; delay fade-in | 4.50 and 11.00 Hz; the delay fade-in works |
| T8 Pitch shift | Auto starts low and rises; Manual only bends down | Auto -12.00 st at key-on, -0.60 st after 300 ms |
| T9 Determinism | Identical output for block sizes 1, 512 and random; after `restart()` | Bit-identical in all cases |
| T10 Robustness | 30 random and extreme configurations: no NaN, bounded, no DC | No NaN; peak 5.68; DC 0.0010 |
| T11 CPU | Apple M4, 48 kHz stereo | Strings, 8 notes 1.27 %; everything on, 16 notes with vocoder and reverb about 2.0 %; all 49 keys 4.3 % of one core |
| T12 Host gap | A note-off lost while the host did not call the processor does not leave the note hanging, with the transport running or stopped | Pass in both (note above -40 dB, final level below -90 dB) |
| T13 Program 0 | A freshly inserted plugin is preset 0, byte for byte; state round trip | Pass |
| T14 Reset | `reset()` from the host sounds as a new instance | Pass |
| T15 Smoothing | Moving a knob at once makes no click | Pass |
| T16 Presets | Levels, DC and peaks of the 45 presets (6-note chord, 3 s; vocoder presets with a voice) | Median -16.9 dBFS RMS; loudest +2.4 dB, quietest -3.4 dB; worst peak -2.97 dBFS; DC 0.00001 |

**Host-level checks** (run on the built plugin):
- athosttest (VST3): 0 FAIL, 10 WARN. The 10 WARNs are the vocoder presets that are silent without a voice at the input, which is
  expected. statebytes, repro and progorder all PASS.
- pluginval strictness 10: SUCCESS on VST3 and on AU.
- auval: PASS.
- Zipper captures (a parameter swept while a note holds): no block-rate lines in the spectrogram.
- **Known AU-only item** (pending for phase 3): athosttest's statebytes check "fresh instance == program 0" fails on the AU build of
  AT330 and also on the already released AT303 AU, so it comes from the AU wrapper/host metadata, not from the engine (T13 passes
  with the processor). The AU "state" check (save, close, reopen, play) gives correlation 1.000 on all three programs it
  tries, once the rest state also clears the Classic envelopes (the released AT303 AU gives 0.08-0.60 on the same check).

---

### 5.1 Release QA (version 1.0.0, 2026-10-23)

| Where | What | Result |
|---|---|---|
| macOS 15, Apple M4 (universal build) | athosttest VST3 and AU | 0 FAIL; 10 WARN = the "(Audio In)" presets, silent without a voice |
| | statebytes, repro, progorder, editor | PASS on VST3; on the AU the "fresh instance == program 0" check fails as on every AU of the series (AT303 too) |
| | pluginval strictness 10 VST3 and AU, auval | SUCCESS, PASS |
| Windows 11, Ryzen 9 5900XT (MSVC) | athosttest VST3; statebytes, repro, progorder; pluginval 10; at330test | 104 PASS, 10 WARN, 0 FAIL; PASS; SUCCESS; 0 failures |
| | CPU (at330test T11) | strings 8 notes 3.4 %, everything on 16 notes 6.0 %, all 49 keys 15.3 % of a core (2.6x the M4, as in the other plugins under MSVC) |
| Linux (Ubuntu 22.04 container) | athosttest VST3; pluginval 10 VST3 and LV2; at330test; standalone | 104 PASS, 10 WARN, 0 FAIL; SUCCESS; 0 failures; opens and closes cleanly |
| REAPER 7 (QA harness, VST3 and AU) | 45 presets rendered from MIDI, preset consistency, state, automation, real time, editor | all render; flags only the expected ones: the 12 "(Audio In)" presets silent, three long-release presets still sounding after the render end, "Upper Strings Only" silent while the material plays below the split |
| Ableton Live 12.3.5 | the vocoder through the plugin sidechain (AT303 as the voice), device panel, save and reopen | vocoder meter 0.90 with the sidechain, 0 without it; Live filled the device panel by itself with every parameter (46 = 44 + the host's two); sidechain routing kept after save; no errors in Live's log |

## 6. Presets and level method

**45 presets** in five categories, no brand, film, artist or song names (a rule checked in `presets.py`):

| Category | Count | Presets |
|---|---|---|
| Strings | 9 | Ensemble Strings, Slow Swell Strings, Dark Strings, Bright Strings, Short Strings, Vintage Strings (Classic envelopes), Dry Strings, Wide Strings, Upper Strings Only |
| Choir | 8 | Male Choir 8', Female Choir, Full Choir, Low Monks, Cathedral Voices, Choir Vibrato, Distant Voices, Dry Choir |
| Layers | 8 | Strings and Choir, Rain on Neon, Night Shift Pad, Glass Hall, Cold Horizon, Split Choir over Strings, Frozen Cathedral, Velvet Poly |
| Vocoder | 12 | (every name ends in "(Audio In)", the AT20V convention for presets that are silent without a voice at the input) Vocoder Ensemble, Vocoder Dry, Machine Voice, Vocoder with Choir, Vocoder with Strings, Whisper Bands, Hold Pad, Formant Freeze, Deep Formants, Small Formants, Slow Response, External Carrier |
| Pitch and Motion | 8 | Rising Choir, Siren Strings, Scoop Voices, Sinking Pad, Wobbling Tape, Classic Retrigger, Poly Swells, Bend Choir |

Each preset stores only what differs from the defaults (`Params330.h`), plus a demo progression and a flag for presets that need a
voice at the input. Descriptions are one sentence in words.

**Level method** (`tools/headroom.py`): each preset is rendered with its demo progression (a chord per bar at 96 BPM) and with the
render tool's test chord (A minor over three octaves, 6 s), vocoder presets with the synthetic voice. Only the preset's `volume` is changed:
the RMS of the body is brought toward **-16 dBFS** without the peak passing **-1.5 dBFS**; the ensemble and the reverb are not linear
in the peak, so it iterates up to six times to within 0.3 dB. The values go to `tools/preset_levels.json`, then `presets.py` is rerun and the
plugin rebuilt. T16 is the independent check (median -16.9 dBFS, spread within +2.4/-3.4 dB, worst peak -2.97 dBFS).

---

## 7. Demos

`tools/demos.py` renders, with the offline render tool, 12 character clips and 2 sketches to
`/Volumes/Docs/Projects/own/audiotools-private/at330/demos/` (WAV 24-bit, MP3 LAME V2 and an `index.md`). Every clip is taken to
-16 LUFS (BS.1770) with one gain and, if the peak passes -1 dBFS, a look-ahead peak limiter whose reduction is logged and never exceeds 3 dB.

| Clip | Preset | What it shows |
|---|---|---|
| 01_strings | Ensemble Strings | 4' strings through the four-BBD ensemble on a slow minor progression, 72 BPM |
| 02_ensemble_on_off | Dry Strings | The same chords twice: dry, then the ensemble switched on at bar 9 |
| 03_male_choir | Male Choir 8' | Male 8' through the seven-filter bank, vibrato after its delay |
| 04_split_choir | Female Choir | Female 4' above the split, male 4' below, a slow line on top |
| 05_cathedral | Cathedral Voices | All four choir tablets and the extra reverb |
| 06_rain_on_neon | Rain on Neon | Strings with male and female voices, long release, melody above |
| 07_vocoder | Vocoder Ensemble | 10 bands plus ensemble, synthetic speaking voice over Am F C G |
| 08_machine_voice | Machine Voice | Fast band response, no ensemble, the voice counting on a held chord |
| 09_whisper | Whisper Bands | A whispered voice: the consonant path carries s, t, k only while a key is down |
| 10_hold | Hold Pad | Hold: each band keeps its level for seconds after the voice stops |
| 11_rising_choir | Rising Choir | Pitch Shift Auto: each chord starts a fifth below and glides up |
| 12_classic_vs_poly | Classic Retrigger | One arpeggiated chord twice: Classic (each new key re-attacks the whole chord) then Poly |
| 20_sketch_darkwave | Night Shift Pad + Vocoder Ensemble | Dark wave in A minor, 120 BPM: Pallor drums (bar 1), Thrum guitar (bar 5), the AT330 pad (bar 9) and vocoder with the synthetic voice (bar 17); mastered with Lacquer |
| 21_sketch_ambient | Frozen Cathedral, Low Monks, Glass Hall | Ambient: an Orrery preset (48 BPM) generates the parts and the AT330 plays pad, low male drone and high cloud; mastered with Lacquer |

**The synthetic voice** (`tools/demo_voice.py`): the project's own, with no recorded voice (no rights to clear). It speaks: vowels, nasals and
liquids with a glottal pulse and formants from published average male-vowel tables (Peterson and Barney, cited in the script's comments, not
re-checked here), and unvoiced consonants (s, sh, f, h, t, k, p) made of filtered noise, because without them the demos could not show the
consonant path. Phonemes are written as text; scripts "night", "count" and "whisper"; each phrase is aligned to the bars of the track.
The sketches reuse the other products of the series (Pallor, Thrum, Orrery, Lacquer) as render tools. Clip 12 and the ensemble toggle
clip use the render tool's `--at` event (a parameter change at a time).

---

## 8. Open questions (need the schematics or a real unit)

Everything below is a guess in the current build; the schematic fold-outs are image-only (SM1 PDF pages 5-8 for AGH17, HVH56, ETH09,
FLH16 and VBH58; SM2 pages 5-11) and can be rendered with `pdftoppm -r 150 -png` [BRIEF] §10.

1. **Choir filter routing and Q.** Which four of the seven HVH56 filters serve each voice, with what gain and Q (§3.5). **First to check.**
   Also the HVH105 (Mk II) table.
2. **Waveform network values**, the divider-to-sawtooth and square networks and the TONE network on AGH17/AGH19, which would replace the
   inferred curvature k = 1.8 (§3.2) and the strings filter (§3.6).
3. **Attack and release ranges in seconds** (only pot values and the 50 ms discharge are in the notes) (§3.4).
4. **Mk II ETH16 LFO periods and phases**, the BBD clock mean frequency and the actual delay depths of vibrato and ensemble (§3.8, §3.9).
5. **Consonant high-pass cut-off** (assumed 4.5 kHz) (§3.7).
6. **Band Q and slope** of the vocoder banks (assumed Q 3.2, two cascaded 2-pole sections), and the follower attack/release without Hold.
7. **Compander time constants** (BA662 around the vocoder and around the BBDs; not modelled).
8. **Master clock frequency** and the exact AY-3-0214 divisor table (assumed 2.00024 MHz and divisors 478..253).
9. **Forum opinion.** Gearspace and ModWiggler returned HTTP 403 or were not searched; the user wishes in §2.2 come from reviews and retailer pages.
10. **Academic literature** on channel vocoders and string-ensemble circuits was not searched in depth.
11. **Pitch-shift TIME range**: only five waveform points are readable on [SM1 p.14].
12. **A real unit.** A measurement of the choir spectra of each voice would settle item 1 without the schematics.
