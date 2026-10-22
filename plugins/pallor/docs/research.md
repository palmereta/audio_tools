# Drum machine for the dark genres (Pallor): research

Compiled 2026-10-21 for a new Audio Tools product: a free, open-source (AGPL-3.0) drum machine named
**Pallor** (`plugins/pallor/`; its working name in phase 1 was Vesper, dropped before release because a commercial
multi-effect with that name came out in October 2026). It is an **original instrument, not a clone** of any machine: sixteen voices with
an analog-circuit engine and a "Digital 80s" sample-playback engine each, two effect sends and a master chain
that carry the studio sound of the period, and a step sequencer with the features users of current drum plugins
ask for. Its target genres are dark wave, coldwave, post-punk, minimal wave, EBM, synthpop, goth rock,
industrial, new beat, italo disco / hi-NRG, witch house, 90s darkwave / trip-hop and dark ambient.

The document covers the sound of those genres (§1), the machines (§2), the circuits (§3), swing (§4), the
studio effects (§5), what users of current drum plugins ask for (§6), the design of Pallor mapped to all of
that (§7) and the test plan (§8). The DSP engine is being written in parallel; the fact sheet at the end of
§0 collects the numbers it needs.

Conventions (the same as `plugins/at303/docs/research.md`, `plugins/tarnish/docs/research.md` and
`plugins/lacquer/docs/research.md`):
- Every fact carries its source inline: a URL (all accessed 2026-10-21) or a document with page, section or
  equation. Short tags are defined in the table below. Where only a forum post, a press quote or a
  secondary source was available, the text says so. **unverified** marks a value that circulates widely but
  that no primary source fetched today confirms.
- **(inferred)** marks my own conclusions, calculations or design proposals. Values marked "calculated"
  follow from a formula or component values given in the text; the scripts are trivial and the numbers can
  be re-derived. §7 and §8 are design proposals throughout.
- Product and brand names (Roland, TR-808, LinnDrum, Oberheim DMX, E-mu, Akai, Simmons, AMS, Lexicon, SSL
  and the others) are trademarks of their owners and appear here **only for research**. **Pallor's UI,
  presets, kit names, manual and marketing never show them**: engines and programs are named generically
  ("bridged-T kick", "8-bit companded", "12-bit sampler", "nonlinear", "hexagonal tom"), never by machine.
- Pallor ships **no factory samples of any machine**. Every built-in sound is synthesised by Pallor itself
  at initialisation (§7.2). Nothing here copies proprietary text, code or ROM data. MAME drivers (BSD-3-Clause)
  are cited as documentation of the hardware, read but not copied.
- **The web search budget of the session (shared by five parallel research passes) ran out** before every item
  could be checked. Gaps are marked **unverified** and listed in §9. Reddit could not be reached at all (§6).
  Pages that refused the fetch (Gearspace, ModWiggler, Reverb.com, some MusicRadar bodies) are cited only
  through search-engine summaries, tagged **[snippet]** or "(search summary)", which count as unverified.

Source tags used throughout (URLs accessed 2026-10-21; the PDFs were downloaded only to be read, and nothing
from them is committed):

| Tag | Document | Where |
|---|---|---|
| **[SN808]** | Roland TR-808 Service Notes, 15 June 1981: p.5-6 circuit description, p.7 block diagram, p.9 voice schematic, p.14 "Checking Voices" table (frequency, decay, amplitude per voice), p.15 production changes | https://manuals.fdiskc.com/flat/Roland%20TR-808%20Service%20Manual.pdf |
| **[SN909]** | Roland TR-909 Service Notes, 15 June 1984: p.4 block diagram, p.6 analog voices, p.9 scope waveforms | https://manuals.fdiskc.com/flat/Roland%20TR-909%20Service%20Manual.pdf |
| **[SNCR78]** | Roland CR-78 Service Notes, 20 June 1979: pp.12-13 voicing schematic (VG-11A), p.15 voice adjustment table, p.18 parts | https://manuals.fdiskc.com/flat/Roland%20CR-78%20Service%20Notes.pdf |
| **[606]** | Roland TR-606 schematic, sheet 6 | https://manuals.fdiskc.com/flat/Roland%20TR-606%20Schematic.zip |
| **[W-BD]** | K. J. Werner, J. S. Abel, J. O. Smith, "A Physically-Informed, Circuit-Bendable, Digital Model of the Roland TR-808 Bass Drum Circuit", DAFx-14, Erlangen, 2014 | https://dafx14.fau.de/papers/dafx14_kurt_james_werner_a_physically_informed,_ci.pdf |
| **[W-CY]** | K. J. Werner, J. S. Abel, J. O. Smith, "The TR-808 Cymbal: a Physically-Informed, Circuit-Bendable, Digital Model", ICMC/SMC 2014, pp. 1453-1460 | https://speech.di.uoa.gr/ICMC-SMC-2014/images/VOL_2/1453.pdf |
| **[W-PhD]** | K. J. Werner, *Virtual Analog Modeling of Audio Circuitry Using Wave Digital Filters*, PhD thesis, Stanford (CCRMA), 2016 (Table 2.2 bridged-T values; App. C tom tuning) | https://stacks.stanford.edu/file/druid:jy057cz8322/KurtJamesWernerDissertation-augmented.pdf |
| **[W-CB]** | K. J. Werner, J. S. Abel, J. O. Smith, "More Cowbell: a Physically-Informed, Circuit-Bendable, Digital Model of the TR-808 Cowbell", AES 137th Convention, 2014, paper 9207 | AES e-library (paywalled, **not read**) |
| **[SS]** | Gordon Reid, *Synth Secrets*, Sound On Sound: "Synthesizing Percussion" (Nov 2001), "Synthesizing Drums: The Bass Drum" (Jan 2002), "Practical Bass Drum Synthesis" (Feb 2002), "Synthesizing Drums: The Snare Drum" (Mar 2002), "Practical Snare Drum Synthesis" (Apr 2002), "Analysing Metallic Percussion" (May 2002), "Synthesizing Realistic Cymbals" (Jun 2002), "Practical Cymbal Synthesis" (Jul 2002), "Synthesizing Cowbells & Claves" (Sep 2002) | https://www.soundonsound.com/techniques/ + the article slug |
| **[PLAITS]** | Mutable Instruments Plaits source, MIT licence, © Emilie Gillet: `plaits/dsp/drums/analog_bass_drum.h`, `synthetic_bass_drum.h`, `analog_snare_drum.h`, `hi_hat.h` | https://github.com/pichenettes/eurorack/tree/master/plaits/dsp/drums |
| **[ED-110]**, **[ED-55]** | Electric Druid, "Analogue drums: Boss DR-110" and "Analogue drums: Korg KR-55" | https://electricdruid.net/analogue-drums-boss-dr-110-drum-machine/, https://electricdruid.net/analogue-drums-korg-kr-55/ |
| **[PMI77]** | Precision Monolithics 1977 databook, DAC-76 "COMDAC" data sheet (§11-1) | https://ftp.mirrorservice.org/sites/www.bitsavers.org/components/pmi/_dataBooks/1977_PMI_Linear_and_Conversion_IC_Products.pdf |
| **[MAME-x]** | MAME drivers and devices (BSD-3-Clause), written from service manuals and schematics: `src/devices/sound/dac76.h/.cpp` (DAC-76/AM6070 law), `src/mame/linn/linndrum.cpp`, `src/mame/oberheim/dmx.cpp`, `src/mame/roland/roland_tr707.cpp` and `mb63h114.cpp`, `src/mame/akai/s900.cpp`, `mpc60.cpp`, `mpc3000.cpp`, `src/devices/sound/l4003.cpp` (MPC60 voice chip), `l7a1045_l6028_dsp_a.cpp`, `src/mame/yamaha/ymrx15.cpp`, `src/devices/sound/ym2154.cpp`, `src/mame/alesis/alesis_a.cpp` | https://github.com/mamedev/mame (master, fetched 2026-10-21) |
| **[VLINN]** | Aly James Lab, VLINN (Linn LM-1 recreation) user manual v2: LM-1 circuit description (XR2206 clocks, AM6070, CEM3320) | https://www.alyjameslab.com/exclusive/VLINN_Linn_LM-1Drums_USER_MANUAL_V2.pdf |
| **[RMX]** | AMS RMX16 Digital Reverberation System manual, Issue 1, as reproduced in Universal Audio's "AMS RMX16 Expanded" manual | https://media.uaudio.com/support/manuals/dd/AMS%20RMX16%20Expanded%20Manual.pdf |
| **[ATK-LINN]** | Roger Linn interviewed by Attack Magazine, "Roger Linn on Swing, Groove & the Magic of the MPC's Timing", 2 July 2013 (pages 1-2) | https://www.attackmagazine.com/features/interview/roger-linn-swing-groove-magic-mpc-timing/ |
| **[MPC60]** | Akai MPC60 Operator's Manual, software v3.1 (Linn and Battino, rev. 22 Jan 2002), printed pp. 26, 43-45 | https://www.polynominal.com/akai-mpc60/akai-mpc60-manual.pdf |
| **[HP-BBD]** | M. Holters, J. Parker, "A Combined Model for a Bucket Brigade Device and its Input and Output Filters", DAFx-18 | https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf |
| **[WP]** | Wikipedia articles, used for dates, prices and record credits only, each cited by URL | — |

---

## 0. Summary of recommendations

**The sound** (§1). The target genres share one palette and one era (1977-1995). Tempos: new beat 90-115 BPM
(born from 45 rpm EBM played at 33 with +8 %, a factor of ≈ 0.80, calculated), hi-NRG 120-140, italo 110-125,
EBM ≈ 115-135, trip-hop 67-95, witch house 60-75 (often written double-time), post-punk and goth anywhere from
120 to 160 with half-time feels; there is no authoritative range for coldwave, post-punk, goth or 90s darkwave,
and per-track tempos from automated sites are indications only. Archetypes: four-on-the-floor, motorik,
tom-led tribal, preset-box bossa/disco, the 16th-note "Blue Monday" kick, swung trip-hop breaks, witch-house
half-time with fast rolls. Documented machines include the CR-78, LM-1, DMX, Drumulator, TR-808, TR-707,
DR-55, KR-55, Synare and Simmons, then S900/S1000, SP-1200 and MPC60 (§1.3; Doktor Avalanche went DR-55 → 606 →
808 → 909 → DMX → RX5 + S900 → S1000). The sound on record is the machine **plus** processing: gated reverb
(born at the Townhouse in 1979 from a compressed talkback mic and a gate), the AMS RMX16 NonLin 2 (1982),
flangers on cymbals, phaser on hats, plate and spring on snares, bus compression and tape.

**The machines** (§2). The 80s sample machines store 8-bit **μ-255 companded** data decoded by the PMI DAC-76 /
AMD AM6070 (exact law in §2.1: 255 levels, 72 dB, SNR ≈ 35-40 dB that follows the signal), clock each voice
from its own analog oscillator (XR2206 in the LM-1, 12.0-30.3 kHz; 74LS627 in the LinnDrum, ≈ 30 kHz; a 555 per
DMX card, ≈ 24-27 kHz), play **without interpolation** so the images move with the pitch, and use the DAC's
reference input as the VCA. Their analog filters matter: the LinnDrum's bass low-pass sits at ≈ 1.4 kHz and
jumps to ≈ 100 kHz on each hit, the DMX has 6-pole reconstruction filters at ≈ 10 or 16 kHz. The samplers
add 12-bit linear words, fixed-rate **drop-sample** pitch (SP-1200 at 26.04 kHz; MPC60 at 40 kHz) or a
per-voice clock with a tracking filter (S900). Hexagonal pads (Simmons SDS-V) add bend, click and noise-tone
controls; the Syndrum/Synare add the downward "zap".

**The circuits** (§3). From the TR-808 service notes and Werner's papers: kick ≈ 49.5 Hz settling from ≈ 57 Hz
with a 4-6 ms half cycle at ≈ 2× on the attack, decays 50/300/800 ms to -20 dB; snare resonators 238 and 476 Hz
with snappy noise through a 2.75 kHz high-pass; clap = noise through ≈ 1 kHz with three bursts in a 30 ms window
then a 100 ms tail; six square oscillators at 205.3, 304.4, 369.6, 522.7, 540 and 800 Hz through band-passes at
≈ 3.44 and 7.1 kHz for cymbal and hats; cowbell = the 540 and 800 Hz squares with a two-slope envelope (its
band-pass centre is unverified); claves 2.5 kHz, rim 455 + 1,667 Hz; toms 80-220 Hz and congas 165-455 Hz with
amplitude-dependent pitch. CR-78: noise-based cymbal and hat, a "metal beat" from three squares at 4.08-6.17 kHz.
The 909 kick and snare structures are known, their numbers are not (unverified); the SDS-V circuit is not
documented here.

**Swing** (§4). Linn's definition: the share of each 16th (or 8th) pair given to the first note; 50 % straight,
66.7 % triplet, 75 % maximum; the second of each pair moves by (s − 0.5)·2·step. 48 PPQ on the LM-1 gives
50/54/58/62/66/71/75 %; the MPC60 runs 96 PPQ and swings 8ths or 16ths.

**Effects** (§5). Gated reverb = reverb keyed off by the dry hit, hold ≈ 100-600 ms (the original was longer);
nonlinear = flat or rising dense reflections then an abrupt cut (RMX16, with its HF half-decay table); EMT 140
up to ≈ 5 s at 500 Hz; spring = dispersive chirps below ≈ 5 kHz; early digital reverbs band-limited to 10-18 kHz
with 12-bit gain-ranged converters; BBD chorus T = N/(2·f_clk), Juno-60 0.513/0.863 Hz over 1.66-5.35 ms; SSL
bus compressor attack 0.1-30 ms, release 0.1-1.2 s or auto, ratio 2/4/10; RE-201 heads ≈ 69-177, 131-337 and
189-489 ms; exciter = high-pass → soft clipper → 20-70 % mix.

**Users** (§6). Most missed: per-track length (polymeter), MIDI drag-out and import, fill conditions, multi-out
that keeps the internal effects; then probability, ratchets, swing quality, embedded samples. Most criticised:
"sterile" sound, static samples, high CPU, small GUIs, licensing and abandoned products. The dark-genre users want
the 80s *sampled* machines and the processing built in.

**Pallor** (§7). Sixteen voices, each with an Analog engine (circuit-style models of §3) and a Digital 80s engine
(the playback chain of §2 rendered band-limited, with Pallor's own synthesised tables and optional user samples
embedded in the state); per-voice tune, decay, tone, two character controls, level, pan, accent, velocity, drive,
crush, filter, choke, two sends and a bus; a Space send (gated, nonlinear, reverse, plate, spring, hall, room), an
Echo send (12-bit delay, tape echo, BBD delay, chorus, ensemble, jet flanger, phaser) and a master (Tarnish's
hysteresis tape, exciter, glue compressor, sampler crunch); a 64-step polymetric sequencer with probability,
ratchets, flams, nudge, Linn swing, conditions, A/B/Fill variations, auto fill, song chain, MIDI in/out and `.mid`
export; multi-out.

**Tests** (§8). 25 JUCE-free tests: sample-accurate timing across block sizes and host ppq gaps, determinism
fingerprint, swing, choke, ratchets and flams, seeded probability, conditions, polymeter, MIDI balance and `.mid`
round trip, multi-out, sample embedding, voice measurements against §3 (cowbell 540/800 Hz, hat oscillator set,
kick 49.5 Hz and decays, clap bursts, snare 238/476 Hz), μ-law and 12-bit quantisation, clock-pitch imaging
without host aliasing, gated reverb timing, no NaN/denormals/DC, CPU.

### 0.1 Fact sheet for the DSP (verified numbers with sources; see the sections for context)

| Item | Value | Source |
|---|---|---|
| 808 metal oscillators | 205.3, 304.4, 369.6, 522.7 Hz fixed; 540 Hz (TM1, range 254.3-627.2) and 800 Hz (TM2, range 359.4-1149.9); square, duty 47.98 %, 5 V | [W-CY] §4 |
| 808 metal band-passes | ≈ 3,440 Hz and ≈ 7,100 Hz, both 3rd order; HPF 3 resonant near 10.5 kHz; level stage +6 dB/oct | [W-CY] §4, §9-11 |
| 808 metal envelopes (to -20 dB) | CH 50 ms; OH 90 / 450 / 600 ms; CY 350 / 800 / 1,200 ms (short/mid/long); attack smoother τ = 0.102 ms; CH chokes OH via Q23 | [SN808] p.14, p.6; [W-CY] §6 |
| 808 cowbell | squares 540 Hz (1.85 ms) and 800 Hz (1.25 ms) → swing VCAs → band-pass IC2 (centre/Q **unverified**); two-slope envelope "abrupt level decay at the initial trailing edge"; 50 ms to -20 dB | [SN808] p.6, p.14 |
| 808 kick | resonance ≈ 49.5 Hz (component-derived); measured period 18 ms (56 Hz); designers' "inherent" 16 ms (62.5 Hz); glide from ≈ 57 Hz settling to ≈ 49-50 Hz by 0.2-0.3 s; attack: R165 shorted for 4 ms → half cycle at 2× (≈ 129 Hz derived), then a retrigger pulse; decay 50 / 300 / 800 ms to -20 dB (τ ≈ 22 / 130 / 350 ms); trigger 1 ms at 4-14 V | [W-BD] §3, §8; [W-PhD] Table 2.2; [SN808] p.5, p.14 |
| 808 snare | resonators 238 Hz and 476 Hz (measured; 250/499 Hz derived); snappy noise through a Sallen-Key high-pass ≈ 2.75 kHz, Q ≈ 0.73 (derived); decay 60 ms | [SN808] p.6, p.9, p.14 |
| 808 clap | noise → band-pass "1000 Hz" (≈ 1.07 kHz, Q ≈ 1.6 derived); 30 ms window with the sawtooth restarted until "the middle of the third time" → 3 bursts ≈ 10 ms apart (inferred from the figure), then the slow tail; parallel "reverberation" VCA; overall 100 ms | [SN808] p.6, p.7, p.14 |
| 808 rim / claves | rim 455 + 1,667 Hz, 10 ms; claves 2,500 Hz, 25 ms | [SN808] p.14 |
| 808 toms / congas (tune low/mid/high) | LT 80/90/100, MT 120/135/160, HT 165/185/220 Hz (HT row possibly misprinted); LC 165/185/220, MC 250/280/310, HC 370/400/455 Hz; decays LT 200, MT 130, HT 100, LC 180, MC 100, HC 80 ms; pitch higher while loud (D80/D81) | [SN808] p.6, p.14 |
| 808 maracas | noise → high-pass ≈ 10.6 kHz (derived), 25/35 ms | [SN808] p.6, p.9, p.14 |
| 909 kick | VCO (saw/triangle) → waveshaper to near-sine; instant-attack slow-decay pitch envelope; click = short pulse + low-passed noise. **Start/end frequency and time unverified**; a fit to sample-pack recordings gives ≈ 49.5 Hz settled, ≈ 166 Hz at 20 ms, ≈ 111 Hz at 40 ms (weak) | [SN909] p.4; [SS] Feb 2002; github.com/tacowars/windsor/issues/329 |
| 909 snare | two triangle VCOs (same circuit, VCO-1 lower), phase-reset per trigger, VCO-1 bends for ≈ 20 ms; **frequencies unverified**; 32-stage shift-register noise | [SN909] p.6 |
| CR-78 metal / hats | cymbal, hat, maracas are noise through LC tanks (cymbal tank ≈ 9.1 kHz derived), decays CY 350, HH 60, M 20 ms; "metal beat" = three CMOS square oscillators at 6,170 / 5,620 / 4,080 Hz → ≈ 5.6 kHz tank, 50 ms | [SNCR78] pp.12-13, p.15 |
| CR-78 other | kick 62.5 Hz/100 ms; snare 340 Hz/60 ms; claves 2,630 Hz; cowbell 800 + 555 Hz; bongos 600/400 Hz; conga 208 Hz | [SNCR78] p.15 |
| Simmons SDS-V | controls: noise level, tone level, bend, decay, noise tone (filter), click; **no circuit numbers verified** | https://en.wikipedia.org/wiki/Simmons_SDS-V |
| μ-255 law (DAC-76/AM6070) | sign + 3-bit chord + 4-bit step; chord base 0, 33, 99, 231, 495, 1023, 2079, 4191; step 2^(c+1); full scale 8031; 72 dB; 255 levels; no bit inversion in drum ROMs | [PMI77]; [MAME-dac76]; [MAME-linndrum] |
| LM-1 | 8-bit μ-255 (AM6070), "28 kHz"; per-voice XR2206 clock 12.0-30.3 kHz (C = 0.01 µF, R = 3.3 k + 5 k pot); no interpolation; CEM3320 on bass/toms/congas (later units) | rogerlinndesign.com museum; [VLINN] |
| LinnDrum | 8-bit μ-255 (AM6070); mux voices ≈ 30 kHz (trimmer only); snare/toms/congas on tunable 74LS627 clocks; bass CEM3320 ≈ 1.4 kHz → ≈ 100 kHz on trigger; toms ≈ 650 Hz → 46 kHz; snare RC ≈ 23.7 kHz; outs ≈ 15.9 kHz | [MAME-linndrum] |
| DMX | 8-bit μ-law (AM6070); 555 per card, "approximately 24,000" Hz (≈ 21-39 kHz trim range derived); 4 KB per card; 6-pole Sallen-Key ≈ 10/16 kHz; DAC reference = VCA | electrongate; [MAME-dmx] |
| Drumulator | 8-bit, 64 KB EPROM; rate 26.04 kHz **implied, unverified**; companding unverified; no tuning | WP E-mu SP-12; polynominal |
| TR-707 | linear 8-bit DAC (multiplexed, EG as reference) + 6-bit cymbals; 1.6 MHz / 8 / 8 voices = 25 kHz per slot (derived, unverified per voice); no tuning; ≈ 15.9 kHz output RC | [MAME-tr707], [MAME-mb63h114] |
| SP-12 / SP-1200 | 12-bit linear, 26.04 kHz, drop-sample at fixed rate; SP-12 no reconstruction filter; SP-1200 ch 1-2 SSM2044 dynamic, 3-4 fixed low, 5-6 fixed high, 7-8 none | WP; Rossum education page |
| S900 / S950 | 12-bit linear, 7.5-40 kHz (S950 48 kHz), per-voice clock, clock-tracking MF6-50 filters | WP; [MAME-s900] |
| MPC60 | "12-bit non-linear, 40 kHz" (Linn); L4003 at 35.84 MHz/896 = 40.0 kHz fixed, 4.12 fixed-point pitch, no interpolation; 16 voices; 8 outs + stereo; non-linear law unverified | rogerlinndesign.com; [MAME-l4003] |
| Linn swing | 50 % straight, 66 % (66.7) triplet, 75 % max; second of each pair delayed; LM-1 48 PPQ → 50/54/58/62/66/71/75; MPC60 96 PPQ, 8th and 16th, 50-75 %; suggested 54 % and 62 % | [ATK-LINN]; [MPC60] pp.43-45 |
| Gated reverb | DS201 hold 2 ms-2 s, decay 2 ms-4 s, attack 10 µs-1 s, range 0-90 dB; recipes hold ≈ 150 ms-1 s, release ≈ 150 ms or "near-instant"; the 1979 original ≈ 1.5 s (Padgham) | DS201 manual p.5; §1.4, §5.1 |
| Nonlinear reverb | RMX16 NonLin 2: decays "only very slightly" for the set time then dies "very rapidly", decay not calibrated; Reverse builds then stops; 18 kHz bandwidth; HF half-decay 18 kHz → 1 kHz over 0 → −9; LF ×¼ to ×4 at 350 Hz | [RMX] |
| SSL bus compressor | attack 0.1/0.3/1/3/10/30 ms; release 0.1/0.3/0.6/1.2 s/Auto; ratio 2/4/10:1 | Waves SSL G-Master manual pp.3-4 |
| RE-201 | 3 playback heads ≈ 69-177, 131-337, 189-489 ms (UAD manual via KVR, snippet-grade) | §5.9 |
| BBD chorus | T = N/(2·f_clk); Juno-60: 2 × 256 stages, triangle 0.513 Hz (I), 0.863 Hz (II), 9.75 Hz (I+II), 1.66-5.35 ms, right inverted, min clock ≈ 26 kHz | [HP-BBD]; pendragon-andyh Juno60 measurements |
| Genre BPM | new beat 90-115; hi-NRG 120-140; italo 110-125; EBM ≈ 115-135; trip-hop 67-95; witch house 60-75; others: no authoritative range | §1.1 |

---

## 1. The sound of the genres

Limits of this section. No authoritative BPM range exists for coldwave, post-punk, goth rock or 90s darkwave;
the ranges below combine genre descriptions, producer recipes from the press and **per-track tempo estimates
from automated sites** (songbpm.com, musiciwant.com), which can be off by a factor of two (half- or
double-time). Track BPMs are therefore "indications", and they are labelled "(automated)". Reverb.com
articles returned HTTP 403 and MusicRadar article bodies came back empty; where a fact rests only on a
search-engine summary of such a page, the text says "(search summary)", which counts as unverified.

### 1.1 Overview by genre

| Genre | Tempo (BPM) | Pattern archetype | Signature machines and sources | Sources |
|---|---|---|---|---|
| Post-punk (1978-84) | No genre range found. "She's Lost Control" ≈ 128, "Spellbound" ≈ 149, "A Forest" ≈ 163 (likely double-time of ≈ 81) (automated) | Motorik (a steady, repeated 4/4 at moderate tempo) via Joy Division, PiL and Siouxsie; Budgie's "tom-tom-intensive", tribal patterns; drums recorded one piece at a time (Hannett) | Synare 3 and Simmons SDS-V (Stephen Morris); CR-78 bossa preset on "Decades"; AMS digital delays and a Marshall Time Modulator (Hannett); contact mics plus flangers on cymbals (The Cure) | https://en.wikipedia.org/wiki/Motorik; https://en.wikipedia.org/wiki/Budgie_(musician); https://www.electricityclub.co.uk/stephen-morris-interview/; https://www.soundonsound.com/techniques/classic-tracks-the-cure-a-forest |
| Goth rock | No genre range. "Bela Lugosi's Dead" ≈ 145 (automated) | Simon Reynolds: beats either "hypnotically dirgelike" or tom-heavy and tribal; several acts used drum machines that played down the backbeat. Kevin Haskins' bossa-nova beat on "Bela Lugosi's Dead" | Sisters of Mercy's "Doktor Avalanche" (§1.3) | https://en.wikipedia.org/wiki/Gothic_rock; https://thevinyldistrict.com/storefront/kevin-haskins-the-tvd-interview (search summary) |
| Dark wave (1980s) | No number; compared with new wave, "relatively slower tempos, lower pitches, and more minor keys" (van Elferen, cited by Wikipedia) | Drum machine among the typical instruments; no pattern source | — | https://en.wikipedia.org/wiki/Dark_wave |
| Dark wave (1990s) | No number | — | German revival (Girls Under Glass, Deine Lakaien, Diary of Dreams), Neue Deutsche Todeskunst (Das Ich), US darkwave on Projekt after 1993 | https://en.wikipedia.org/wiki/Dark_wave |
| Coldwave | No number | "Militant rhythm sections"; French coldwave influenced by Martin Hannett's "studio-produced drum sounds" | Affordable portable synths and machines; term from *Sounds*, November 1977 | https://en.wikipedia.org/wiki/Cold_wave_(music) |
| Minimal wave | No number | "Mechanical beats" and "short repetitive patterns" from 1970s-80s analog synths and drum machines; pre-MIDI period 1978-1985 | DR-55 and KR-55 (1980-81), then TR-606, TR-808, KPR-77 (≈ 1982), DR-110 (UK, 1983) [forum, §6.3] | https://en.wikipedia.org/wiki/Minimal_wave; https://minimalwave.com/forums/viewthread/811/ |
| EBM | Tracks: "Headhunter" ≈ 123, "Join in the Chant" ≈ 121 (automated); a bassline tutorial at 126. Genre guides give 110-130 or 120-140 (weak sources). **Working range 115-135 (inferred)** | Programmed 4/4 "disco beat" or rock backbeat with little syncopation and a sequenced 16th-note bass; tutorial recipe: four-on-the-floor kick, clap + snare on 2 and 4, closed hats on every 16th, off-beat open hat, drums pitched down | DAF: Korg MS-20 / ARP Odyssey from a Korg SQ-10 16-step sequencer, drums played by Robert Görl. Front 242: TR-808 early, E-mu Drumulator "on just about every track" of the *No Comment* era, Emulator II, then Akai S900/S1000 ("drum machines have been replaced by samplers", 1989) | https://en.wikipedia.org/wiki/Electronic_body_music; https://studiobrootle.com/ebm-drum-pattern/; https://www.gearnews.com/front-242-most-used-synths-drum-machines-and-sounds; https://www.muzines.co.uk/articles/front-line/93 |
| Synthpop | Tracks: "Tainted Love" ≈ 143, "Fade to Grey" ≈ 113, "Never Let Me Down Again" ≈ 106 (automated) | Simple programmed patterns, later dance beats; LM-1 locked to sequencers on *Dare* | TR-77 (Ultravox, 1977), CR-78 ("Vienna"), LM-1 (*Dare*), KR-55 + ARP 2600 kick (*Speak & Spell*), Drumulator + TR-808 + Synclavier (*Construction Time Again*) | https://en.wikipedia.org/wiki/Synth-pop; https://en.wikipedia.org/wiki/Dare_(album) |
| Industrial | No number for the original genre; a modern "industrial techno" recipe at 126-130 with 50-60 % swing | Stark percussion, tape edits, distorted loops; "metal music" (Test Dept, Laibach, Die Krupps, SPK): metal on metal. Modern recipe: four-on-the-floor, off-beat hats, snare on 2 and 4, bitcrushed woodblock, 12-bit resample | Einstürzende Neubauten's metal percussion; Depeche Mode's sampled metal (hammers on steel plates and pipes) | https://en.wikipedia.org/wiki/Industrial_music; https://www.attackmagazine.com/technique/beat-dissected/industrial-techno/; https://www.soundonsound.com/techniques/classic-tracks-depeche-mode-people-are-people |
| New beat (Belgium, 1987-89) | **90-115** (Roland Bellucci of Antler/Subway, quoted via NME: "in the beginning ... 90 bpm, then it became a little faster"); Rate Your Music ≈ 90-120; Jahsonic "rarely exceeded 100" | "A simplified form of EBM" (Jo Casters): EBM's low bass drum and heavy synths, plus acid sounds | Origin: the 45 rpm EBM 12" "Flesh" by A Split-Second (Antler, 1986) played at 33 rpm with the pitch at +8 | https://www.jahsonic.com/NewBeat.html; https://rateyourmusic.com/genre/new-beat/; https://en.wikipedia.org/wiki/New_beat; https://thevinylfactory.com/features/pitch-shift-part-ii-another-10-records-that-sound-incredible-at-the-wrong-speed |
| Hi-NRG | **120-140**, with "high energy" above ≈ 130 (Wikipedia). "I Feel Love" ≈ 128, "You Spin Me Round" ≈ 128, "Smalltown Boy" ≈ 134 (automated) | Four-on-the-floor, staccato hats, heavy machine clap; octave-jumping sequenced 16th-note bass ("the bass often takes the place of the hi-hat"). Off-beat open hats as a hi-NRG trait: unverified | Bobby Orlando's "ringing cowbell percussion lines and robotic sequencers"; machine models unverified | https://en.wikipedia.org/wiki/Hi-NRG; https://en.wikipedia.org/wiki/Bobby_Orlando |
| Italo disco | Attack Magazine recipes: LM-1 beat 110-120 BPM with 50-53 % swing; "Early 80s Groove" 110-120 with 60-65 % swing | Four-on-the-floor kick with beats 2 and 4 softer, an extra kick on the last off-beat; snare/clap on 2 and 4 with the clap slightly early; quarter-note closed hat | Electronic drums and machines; LinnDrum on Moroder's *Scarface* (search summary) | https://en.wikipedia.org/wiki/Italo_disco; https://www.attackmagazine.com/?p=27953; https://www.attackmagazine.com/?p=25836 |
| Trip-hop (1991-98) | *Blue Lines* at "'spliff' tempos" from ≈ 90 down to 67 BPM (Reynolds, via Wikipedia); "Teardrop" ≈ 77, "Sour Times" ≈ 94 (automated); Attack recipe 80-90 with 50-60 % swing | Slowed sampled breakbeats, dub weight; recipe: layered kick with ghost notes on 16th steps 9 and 16, snare on 2 and 4, straight 8th hats plus sporadic soft 16ths, vinyl crunch layer | Massive Attack: sampled breaks (Cobham "Stratus", J.J. Johnson "Parade Strut"); Portishead: own drums recorded, cut to vinyl, scuffed, resampled; Tricky: Akai S1000 + Atari/Logic + ADAT | https://en.wikipedia.org/wiki/Blue_Lines; https://www.attackmagazine.com/technique/beat-dissected/chillout-beat-raw-strip-hop/; https://en.wikipedia.org/wiki/Dummy_(album); https://en.wikipedia.org/wiki/Maxinquaye |
| Witch house (2009-12) | Chopped-and-screwed roots at 60-70 BPM (Wikipedia, *Oxford American*); "King Night" ≈ 132 (automated, likely double-time of ≈ 66) | "Drastically slowed tempos with skipping, stop-timed beats"; Salem slowed "their 808s to a funereal pace"; on "King Night", "clipped, staccato 808 snare rolls" at normal speed over slowed synths; trap-style hats and rolls | 808 sounds (descriptions); Crystal Castles' machines unverified | https://en.wikipedia.org/wiki/Chopped_and_screwed; https://www.interviewmagazine.com/music/salem; https://blog.thephoenix.com/Boston/music/110183-salem-king-night/ |
| Dark ambient | Often beatless | Occasional ritual/tribal percussion; gongs, bullroarers, low rumbles, machine noise | Samplers and effects (Lustmord, Zoviet France, SPK *Zamia Lehmanni*) | https://en.wikipedia.org/wiki/Dark_ambient |

**New beat arithmetic (calculated).** 33⅓/45 = 0.741; with +8 % pitch, 0.741 × 1.08 = 0.80. A 125 BPM EBM
record plays at ≈ 100 BPM, a full 20 % lower in pitch too, which is why new beat drums are low, heavy and
slow. Pallor's "tape varispeed" master option (§7.5) and a global pitch/tempo link reproduce this.

**Working tempo ranges for Pallor's factory patterns (inferred from the table and §6.3)**: minimal wave and
coldwave 100-130; post-punk 120-160 (often felt as 60-80 half-time); goth rock 120-150; dark wave 100-125;
EBM 115-135; synthpop 105-130; hi-NRG 120-140; italo 110-125; new beat 95-115; industrial 90-130; trip-hop
70-95; witch house 60-75 (or 120-150 double-time); dark ambient free.

### 1.2 Pattern archetypes (for factory patterns and the "auto fill")

- **Four-on-the-floor** (italo, hi-NRG, EBM, synthpop): kick every quarter, snare/clap on 2 and 4, closed hat
  on 8ths or 16ths, open hat on the off-beat. The clap a few ms ahead of the snare and the tambourine a few
  ms ahead of the snare are documented recipes (Attack, https://www.attackmagazine.com/?p=25836 and
  https://www.attackmagazine.com/technique/beat-dissected/new-wave-drums/). This needs per-track
  micro-timing (§7.6).
- **Motorik** (post-punk, coldwave): steady 8th-note pulse, kick on 1 and 3 (with an extra 8th), snare on
  2 and 4, hats on 8ths; little variation over long stretches (https://en.wikipedia.org/wiki/Motorik).
- **Tom-led tribal** (post-punk, goth): Budgie's tom patterns with the backbeat played down; the "Intruder"
  pattern documented by Wikipedia is a one-bar figure of six hits, the 3rd and 6th on snare and the rest on
  toms, played without cymbals (https://en.wikipedia.org/wiki/Intruder_(song)). Four toms in Pallor are
  justified by this archetype.
- **Bossa / preset feel** (goth, post-punk, early synthpop): Bauhaus's bossa beat; the CR-78 bossa preset on
  "Decades" (https://www.electricityclub.co.uk/stephen-morris-interview/); the CR-78 "Disco 2" preset under
  "In the Air Tonight" (https://en.wikipedia.org/wiki/In_the_Air_Tonight). Preset-box rhythms (rumba, bossa,
  beguine) belong in the factory library.
- **EBM drive**: 16th-note sequenced bass carries the groove; drums are sparse and heavy, snare on 2 and 4,
  closed hats on 16ths (https://studiobrootle.com/ebm-drum-pattern/). The bass is outside Pallor; the MIDI
  out (§7.6) can drive it.
- **16th-note kick** ("Blue Monday"): opens with a 16th-note kick run, inspired by Donna Summer's "Our Love";
  the "stutter" was an accident of the DMX's three velocity levels set to the loudest
  (https://en.wikipedia.org/wiki/Blue_Monday_(New_Order_song); https://www.electricityclub.co.uk/stephen-morris-interview/).
- **Trip-hop break**: swung 16ths at 80-95 BPM, ghost kicks, soft hats, vinyl layer (Attack, above).
- **Witch house**: half-time kick/snare, fast 808 snare and hat rolls (ratchets) at normal speed over slowed
  material (Phoenix review, above). Ratchets of 2-8 (§7.6) and a long, low glide kick are the tools.

### 1.3 Documented machines on records (sourced cases only)

| Act / record | Drums | Source |
|---|---|---|
| Joy Division, *Unknown Pleasures* (Strawberry, April 1979) | Each drum recorded separately (Hannett treated drums as "the fundamental element"); three AMS digital delays; the AMS DMX 15-80 was a microprocessor-controlled 15-bit delay, Hannett among the first owners | https://musictech.com/guides/essential-guide/landmark-productions-joy-division-unknown-pleasures/; https://blog.scienceandindustrymuseum.org.uk/factory-records-cutting-edge-technology |
| Joy Division, "She's Lost Control" | Synare 3 (Morris's memoir, p. 248: siren on "Disorder", noise washes on "Shadowplay", the "chi-chi" on "She's Lost Control"); tape-head-cleaner aerosol sprayed in time on the 12" version (March 1980). **Conflict**: some sources say "Syndrum"; Morris's own accounts name the Synare 3. The "roof" story comes from the film *24 Hour Party People* | https://blog.scienceandindustrymuseum.org.uk/factory-records-cutting-edge-technology; https://www.electricityclub.co.uk/stephen-morris-interview/; https://en.wikipedia.org/wiki/She%27s_Lost_Control |
| Joy Division, "Decades" | CR-78 bossa-nova preset (they could not program it) | https://www.electricityclub.co.uk/stephen-morris-interview/ |
| New Order, *Movement* era | Boss DR-55 (e.g. "Truth"); DR-55 triggering an ARP Quadra and Moog Source | same |
| New Order, "Blue Monday" (1983) | Oberheim DMX (chosen over the LinnDrum because it was cheaper and its kick could be tuned); kick re-amped through a monitor for room | https://en.wikipedia.org/wiki/Blue_Monday_(New_Order_song); Morris interview |
| Sisters of Mercy, "Doktor Avalanche" | Boss DR-55 (4 sounds, mono out, with added reverb) → TR-606 → TR-808 (before WEA, per Hussey 1985) → TR-909 (briefly) → Oberheim DMX (*First and Last and Always*, 1985) → *Floodland* (1987): drums sampled into an Akai S900, toms from the DMX, the rest from a Yamaha RX5 (Eldritch wanted a "tighter snare"), Yamaha SPX90 effects → Akai S1000/S3200 → computers | https://en.wikipedia.org/wiki/Doktor_Avalanche; https://en.wikipedia.org/wiki/Floodland_(album); https://mixdownmag.com.au/features/gear-rundown-the-sisters-of-mercy/; https://www.muzines.co.uk/articles/sister-wayne/12559 |
| The Cure, "A Forest" (1980) | **Live drums** (Lol Tolhurst) with C-ducer contact mics ("no ambience whatsoever"), space added with reverbs and delays; cymbals through three Urei 1176s in series; **up to seven flangers** following the envelope so the crashes "dive away" | https://www.soundonsound.com/techniques/classic-tracks-the-cure-a-forest |
| Soft Cell, "Tainted Love" (1981) | A borrowed Roland machine (CR-78 per one source, 808 per another: **conflict**), only kick and snare printed; the "bink bink" is a Synare through a Delta Labs DL4 delay | https://www.mixonline.com/?p=104853 |
| Human League, *Dare* (1981) | Linn LM-1 patterns locked to a Roland MC-8 | https://en.wikipedia.org/wiki/Dare_(album) |
| Ultravox, "Hiroshima Mon Amour" (1977), "Vienna" (1980) | TR-77 with a modified bossa preset; CR-78 | https://en.wikipedia.org/wiki/Warren_Cann; https://en.wikipedia.org/wiki/Vienna_(Ultravox_song) |
| Gary Numan, *Dance* (1981) | CR-78 programmed by Numan (search summary) | numandiscography.co.uk (search summary) |
| Kraftwerk, *Autobahn* (1974) | Modified Farfisa Rhythm Unit 10 and Vox Percussion King on custom pads (search summary) | https://mixdownmag.com.au/features/gear-rundown-kraftwerk-autobahn/ |
| Front 242 | TR-808 (*Geography*, 1982); Drumulator (*No Comment*, 1984); S900/S1000 later | https://www.gearnews.com/front-242-most-used-synths-drum-machines-and-sounds; https://www.muzines.co.uk/articles/front-line/93 |
| Depeche Mode, *Speak & Spell* (1981) | Korg KR-55 snare and hats, ARP 2600 kick | https://www.synthtopia.com/content/2023/11/03/the-drum-sounds-of-classic-depeche-mode/ |
| Depeche Mode, *Construction Time Again* / "People Are People" (1983-84) | Drumulator and TR-808; Synclavier-sampled snares; sampled metal; Hansa Studio 2 used as an echo chamber, ambiences shaped with triggered SSL and Drawmer gates | https://www.soundonsound.com/techniques/classic-tracks-depeche-mode-people-are-people |
| Cocteau Twins (Robin Guthrie's own list) | *Garlands* TR-808 (after a DR-55); *Lullabies* LM-1; *Peppermint Pig* LinnDrum; *Head over Heels*, *Treasure* Drumulator; *Aikea-Guinea* TR-707; *Victorialand* CR-78; *Blue Bell Knoll* SP-12 and RX5; *Heaven or Las Vegas* MPC60 and SP-1200; later MPC60. Guthrie built noise generators for the *Garlands* drums and preferred "a really crunchy horrible 8-bit" sample to "a fat analogue 808 kick"; Roland Dimension D chorus in the studio (1985) | https://www.cocteautwins.com/cocteau-twins-equipment-and-sounds.html; https://www.cocteautwins.com/press-and-news/1985/02/01/digging-for-treasures-electronic-soundmaker-and-computer-music/ |
| Marvin Gaye, "Sexual Healing" (1982) | TR-808 for all percussion, 94 BPM | https://en.wikipedia.org/wiki/Sexual_Healing |
| Phil Collins, "In the Air Tonight" (1981) | CR-78 "Disco 2" preset with extra programming, 96 BPM; gated drums enter late | https://en.wikipedia.org/wiki/In_the_Air_Tonight |
| Massive Attack, *Blue Lines* (1991) | Sampled breaks; hardware unverified | https://en.wikipedia.org/wiki/Blue_Lines |
| Portishead, *Dummy* (1994) | Clive Deamer's live drums cut to vinyl, distressed, resampled | https://en.wikipedia.org/wiki/Dummy_(album) |
| Tricky, *Maxinquaye* (1995) | Akai S1000 + Atari/Logic + ADAT | https://en.wikipedia.org/wiki/Maxinquaye |
| Yello | Fairlight since 1981; drums built from non-drum samples | https://www.soundonsound.com/people/boris-blank-yello-recording-zebra (search summary) |

Not verified despite searching: the "Marconi" delay attributed to Hannett, the machines on Visage's "Fade to
Grey", Clan of Xymox's debut, Cabaret Voltaire's "Nag Nag Nag", Nitzer Ebb's *That Total Age*, Crystal
Castles, and The Cure's *Faith*.

### 1.4 Production tricks

**Gated reverb: the origin.** Hugh Padgham's account (MusicTech, 2019,
https://musictech.com/features/interviews/hugh-padgham-on-gated-reverb-effect-and-evolving-technology):
the Townhouse (Virgin, London) had an SSL console with a compressor and gate on every channel; its live room
was the "Stone Room". He pressed **reverse talkback** while Collins played; the talkback ("listen") mic was
heavily compressed and sounded huge; a noise gate was then added and Collins played a cymbal-less pattern
that suited it, the gate closing "about 1.5 s" after each hit and shutting fast. First used on Peter
Gabriel's "Intruder" (late 1979), then on *Face Value*. Wikipedia adds that Gabriel had Collins remove the
cymbals so the mics could move closer, that the talkback was rerouted to a channel with a gate, and that
Padgham and Lillywhite had the console modified so the listen mic could be recorded, which SSL later made
standard (https://en.wikipedia.org/wiki/Intruder_(song); https://en.wikipedia.org/wiki/Solid_State_Logic_SL_4000).
**Conflicts**: Wikipedia says B-series console, while per-channel dynamics match the E series (1979);
Wikipedia's Padgham article speaks of cutting after "tens of milliseconds", against Padgham's "about 1.5 s";
a 2005 SOS answer mentions gated AMS reverb returns, but the RMX16 dates from March 1982, after "Intruder"
(https://www.soundonsound.com/sound-advice/q-how-do-set-gated-reverb). Precedents: Lillywhite on Siouxsie's
*The Scream* (1978), YMO's gated digital reverb on the snare (1978-79), XTC's *Drums and Wires* (1979)
(https://en.wikipedia.org/wiki/Gated_reverb).

**What it means for the DSP (inferred).** Two different things are called "gated reverb": (a) a compressed
*room* (the listen mic) gated after a hold of hundreds of ms to ≈ 1.5 s; (b) the 1980s studio version, a
reverb return gated by a key from the dry drum with a hold of ≈ 150-500 ms and a fast release (recipes:
threshold ≈ -32 dB, hold ≈ 150 ms, release ≈ 148 ms in a MusicRadar tutorial, search summary,
https://www.musicradar.com/how-to/how-to-create-a-phil-collins-style-80s-gated-reverb-drum-sound; 0.5-1 s
hold with fast release at https://emastered.com/blog/gated-reverb; hold ≈ 0.5 s adjusted to tempo at
https://ledgernote.com/columns/mixing-mastering/gated-reverb; SOS: instant attack, near-instant release, key
from the drum, https://www.soundonsound.com/sound-advice/q-how-do-set-gated-reverb). Pallor's "gated"
program does (b) with a compressed dense room as the source, so (a) is reachable by setting hold long and
compression high (§5.1, §7.5).

**Nonlinear reverb.** AMS RMX16 (March 1982; 18 kHz bandwidth, gain-ranged "pseudo-16-bit" converters,
28-bit internal words, ≈ 40 kHz sampling "if memory serves", 90 dB dynamic range; ROM programs Ambience,
Room A1, Hall C1, Plate A1, Hall B3, Chorus 1, Echo, **NonLin 2**, Reverse 1): in NonLin 2 the early
reflections **do not decay** for a period set by Decay, then drop rapidly
(https://www.soundonsound.com/reviews/ua-rmx16). Details and numbers in §5.2. Padgham reportedly helped
develop NonLin 2 (secondary, partly unverified,
https://producelikeapro.com/blog/how-one-recording-mistake-created-a-musical-phenomenon-in-the-80s/).

**Other documented practice.**
- *Big claps and snares*: a DMX clap tuned down a step, gated, placed slightly before the snare; kick bus ≈ 4 dB
  and snare bus ≈ 3-4 dB of gain reduction (Attack, https://www.attackmagazine.com/?p=25836). A gated
  "80s" kick reverb with decay shortened to 0.22 s at ≈ 25 % wet; clap reverb 4.21 s gated, ≈ 25 % wet
  (https://www.attackmagazine.com/technique/tutorials/how-to-get-big-80s-drums/).
- *Flanged and phased metal*: up to seven flangers on The Cure's cymbals (SOS above); a phaser at 35 % wet
  on 16th hats (Attack "New Wave Drums"). Flanged or chorused *hi-hats* as a Cocteau Twins trait: unverified.
- *Compression and sustain*: three 1176s in series on overdubbed cymbals, a crash ringing ≈ 20 s (SOS,
  The Cure); LA-2A at ≈ 1 dB on the kick, hats de-essed at 9-11 kHz, distortion on the master at 20 % wet
  (Attack LM-1 beat, https://www.attackmagazine.com/?p=27953).
- *Rooms and re-amping*: the "Blue Monday" kick re-amped through a monitor; Hansa Studio 2 as an echo
  chamber with gates shutting the ambience (sources above).
- *Lo-fi*: 12-bit / 26.04 kHz sampler resampling; a woodblock crushed to 6-bit / 22 kHz in a modern
  industrial recipe; Portishead's vinyl distressing (sources above).
- *Spring and plate on snares*: the EMT 140 plate (1957, steel plate ≈ 1 × 2 m, reverb time 1-5 s by a movable
  damper, https://en.wikipedia.org/wiki/EMT_140) and spring tanks (Hammond patent 1939/1941,
  https://en.wikipedia.org/wiki/Reverb_effect) were the standard reverbs before digital units; numbers in §5.3.

---

## 2. The machines

The strongest technical sources for the sample machines are the **MAME drivers** (BSD-3-Clause), which are
written from service manuals and schematics and cite component designators: LinnDrum, Oberheim DMX, TR-707/727,
Akai S900, MPC60, MPC3000, Yamaha RX15 and the DAC-76/AM6070 [MAME-*]. They are read as documentation of the
hardware; no code is copied. The LM-1 comes from Aly James's VLINN manual [VLINN], which describes the original
circuit, and the DAC from the 1977 PMI databook [PMI77].

### 2.1 The companding DAC: PMI DAC-76 "COMDAC" and AMD AM6070

- PMI sold the DAC-76 as a "COMDAC" companding D/A converter: the dynamic range of a sign + 12-bit DAC with
  sign + 7-bit coding, 72 dB in each polarity, conforming to the **Bell System μ-255 law**, with multiplying
  reference inputs and multiplexable outputs; 3 bits choose one of **8 binary-related chords**, 4 bits one of
  **16 linear steps** in the chord, plus a sign bit; an encode/decode pin; listed applications include
  "digital recording and speech synthesis" [PMI77, DAC-76 sheet, §11-1].
- The AM6070 is described in its own datasheet as an "improved pin-for-pin replacement for DAC-76" (quoted in
  [MAME-dac76.h]); VLINN quotes AM6070 datasheet text with the same 15-segment μ-255 wording [VLINN]. The
  AM6070 datasheet itself was not retrieved (**unverified** beyond these two quotes).
- **Exact decode law** as implemented by MAME [MAME-dac76.h, dac76.cpp]: chord base levels
  `{0, 33, 99, 231, 495, 1023, 2079, 4191}`, step size `2 << chord` (2, 4, ... 256),
  `|v| = level[chord] + step · stepSize`, full scale 8031 (normalised as `v/8031`), sign bit set = positive.
  Current output full scale = 3.8 · Iref "from datasheet"; positive and negative magnitudes leave on two pins
  and an op-amp sums them. The LinnDrum driver maps bit 7 = sign, bits 6-4 = chord, bits 3-0 = step, with
  **no bit inversion** (G.711 telephone μ-law stores the bits inverted; the drum ROMs do not) [MAME-linndrum].
- Calculated from that law: `level[c] = 33·(2^c - 1)`, so `|v| = (2·step + 33)·2^c - 33`, the G.711 μ-law
  magnitude at a quarter of its 14-bit scale. Max 8031, smallest step 2: 20·log10(8031/2) = **72.1 dB**,
  matching the datasheet. **255 distinct output levels** (two codes for zero). For a 997 Hz sine the
  quantisation SNR is **39.5 dB at 0 dBFS, 37.1 dB at -20, 34.7 dB at -40, 29.5 dB at -50, 20.4 dB at -60**
  (calculated, nearest-code quantisation), against 49.9 / 29.5 / 11.1 dB for 8-bit linear at 0 / -20 / -40.
  This is the companded sound: noise that **follows the signal** (about 35-40 dB below it over 40 dB of
  level), loud on a decaying tail, never the fixed hiss floor of linear 8-bit. Electric Druid describes the
  same law as ≈ 16 codes per 6 dB and an effective 12-13 bits of dynamic range
  (https://electricdruid.net/experiments-with-variable-rate-drum-sample-playback/).
- **The multiplying reference is the VCA.** In the DMX the decay envelope drives the DAC's reference current,
  so the envelope multiplies the companded signal in the analog domain [MAME-dmx] (Electrongate says the DAC
  "also acts as the VCA", http://www.electrongate.com/dmxfiles/dmxcards.html); the TR-707 switches each
  voice's envelope in as the reference of its multiplexed DAC [MAME-tr707]. Consequence (inferred): the
  quantisation steps scale with the envelope, so the noise floor decays with the sound instead of standing
  still, which a "sample × envelope, then quantise" model gets wrong. Pallor quantises the table first and
  applies the decay VCA after the DAC (§7.3).

### 2.2 80s digital sample drum machines

| Machine | Year, price | Word / DAC | Rate and pitch | Post-DAC chain | Outputs | Sources |
|---|---|---|---|---|---|---|
| Linn LM-1 | Announced 1979, shipped 1980-83; $4,995-5,500; ≈ 500-525 made | "8-bit non-linear, 28 kHz" (Linn); μ-255 data decoded by an AM6070 | Each voice has its own **XR2206** as the sample-address clock: f = 1/RC with C = 0.01 µF and R = 3.3 kΩ + 5 kΩ pot, so **12.0-30.3 kHz** per voice; one byte per clock straight to the DAC, **no interpolation** | First units unfiltered; later revisions put **CEM3320** 4-pole low-passes on bass, toms and congas with the CV shaped to let the transient through; hat from a looping PROM through a VCA whose closed decay follows the decay pot | Individual output per voice; per-voice level, pan, tune | https://rogerlinndesign.com/about/about-museum; [VLINN]; https://en.wikipedia.org/wiki/Linn_LM-1 |
| LinnDrum (LM-2) | 1982-85; $2,995; ≈ 5,000 | 8-bit μ-255, **AM6070** (one shared by 8 "mux" voices, one for snare/sidestick, one for tom/conga) | Mux voices: one shared 74LS627 VCO set by a **PCB trimmer only**, ≈ **30 kHz per voice** (MAME: "8 × ~30K"); snare/sidestick and tom/conga each on a 74LS627 VCO with front-panel tuning. Sample lengths: bass, cabasa, tambourine, clap, cowbell, snare, sidestick 4K; toms/congas 8K; hat 16K; ride, crash 32K | Bass: **CEM3320** 4-pole low-pass at ≈ **1.4 kHz** that jumps to ≈ **100 kHz** on each trigger, then closes (transient through, low-level noise filtered); toms/congas the same at ≈ **650 Hz → 46 kHz**; snare RC low-pass ≈ **23.7 kHz** (2.49 kΩ, 2,700 pF); other individual outs RC ≈ **15.9 kHz**; hat **CEM3360** VCA, slow (open) or pot-set (closed) decay; 2 loudness levels by attenuation | Individual outs + stereo mix of 15 channels | [MAME-linndrum]; https://en.wikipedia.org/wiki/LinnDrum |
| Oberheim DMX | 1980/81; $2,895 (DX 1983, $1,395) | 8-bit μ-law (AM6070 on each voice card, U8) | One **555 timer per card**; "approximately 24,000 samples per second on most cards" (Electrongate). Calculated from MAME's 555 model with C2 = 3.3 nF, R4 = 10 kΩ + T1 (0-10 kΩ), CV = 2/3 Vcc: **20.8-39.3 kHz**, ≈ 27 kHz mid-trim (2.2 nF snare card ≈ 31-59 kHz). Per-trigger pitch variations by moving the 555 control voltage. One 2732 ROM (4,096 bytes, "about 0.171 s") per card, splittable in two | **3 cascaded Sallen-Key low-passes (6-pole)**, a ≈ 10 kHz build (bass, toms) and a ≈ 16 kHz build (snare, hat, cymbal); release envelope on the DAC reference: C3 = 3.3 µF (bass), 6.8 µF (toms), 2.2 µF (hat) through R8 + R9 = 8.3 kΩ, i.e. τ ≈ 27, 56 and 18 ms (calculated) when decay is triggered | 8 individual + stereo/mono mix | [MAME-dmx]; http://www.electrongate.com/dmxfiles/dmxcards.html; https://en.wikipedia.org/wiki/Oberheim_DMX |
| E-mu Drumulator | 1983; $995; > 10,000 in two years | 8-bit; 12 sounds in 64 KB of EPROM; "12-bit-companded outputs" per a sample vendor (**unverified**); DAC part not found | 26.04 kHz implied by Wikipedia's SP-12 article (rate "chosen early on in Drumulator's development") (**unverified** for the Drumulator itself); **no tuning** ("the toms cannot be retuned", E&MM 1983) | **SSM2044** 4-pole VCFs on the tom channels only; per-drum level 0-15, accent 0-5 | 8 individual + mix | https://en.wikipedia.org/wiki/E-mu_Drumulator; https://www.polynominal.com/emu-drumulator; https://www.muzines.co.uk/articles/drumulator/4713; https://en.wikipedia.org/wiki/E-mu_SP-12 |
| Sequential Drumtraks | 1984; $1,295; ≈ 8,000 | 8-bit | Rate **unverified** (a vendor says 27-35 kHz); pitch and volume per sound; accepts LinnDrum ROMs | — | 6 channels + metronome; MIDI with velocity | https://en.wikipedia.org/wiki/Drumtraks |
| Roland TR-707 / 727 | 1985 | 8 voices time-share **one linear 8-bit DAC** (μPC624C, DAC08 type) through mux, demux and sample-and-holds; crash and ride on their own **6-bit** R-2R DACs | MB63H114 divides 1.6 MHz into 100/50/25/12.5 kHz taps; calculated from MAME's wiring: ≈ 25 kHz per voice slot, toms at 12.5 kHz on the 707, cymbals at 25 kHz (32K ≈ 1.31 s) (**derived, unverified**); **no tuning** | Each voice's EG is the DAC reference (multiplying VCA); hat HPF ≈ 723 Hz then a one-transistor VCA (slow/fast decay); cymbal HPF ≈ 339 Hz; per-voice band-pass (bass, snare and toms with diodes that change the response with level), then ≈ 15.9 kHz RC low-pass; stereo outputs ≈ 0.35 Hz-12.4 kHz | Individual outs (patching removes from the mix) | [MAME-tr707]; [MAME-mb63h114]; https://en.wikipedia.org/wiki/Roland_TR-707 |
| Yamaha RX11 / RX15 (1984), RX5 (1986) | RX11 $895 | RX11/15 **8-bit** (E&MM's RX5 review contrasts RX5 "12-bit samples" with "the 8-bit sounds of the RX11 and RX15"); MAME guesses a μ-law-like code with the DAC-76 chord/step shape in the YM2154 (**unverified**) | YM2154 at 2.7 MHz → 25 kHz (derived from MAME); RX11/15 not tunable; RX5 12-bit, tunable, reversible, with pitch-bend envelopes | — | RX11 individual outs; RX15 stereo only | https://www.muzines.co.uk/articles/yamaha-rx11-and-rx15/1465; https://www.muzines.co.uk/articles/yamaha-rx5/1993; [MAME-ymrx15, ym2154] |
| Korg DDD-1 | 1986 | 12-bit | 128 tuning steps per sound; internal rate not found; DSB-1 sampling board 13.6/23.1 kHz (**unverified**) | No filters | 6 assignable + L/R | https://en.wikipedia.org/wiki/Korg_DDD-1 |
| Alesis HR-16 | 1987; £449 | 16-bit DAC (PCM54HP); MAME: 16-bit values built from **8-bit ROMs** by a shift scheme (**unverified**) | "Claimed 47 kHz", 20 kHz bandwidth (SOS, Jan 1988); per-pad tuning | — | Two stereo pairs | https://www.muzines.co.uk/articles/alesis-hr-16-drum-machine/3449; [MAME-alesis] |

**Hit-to-hit variation.** In the LM-1 each voice has a free-running XR2206 clock, so the first sample of a hit
lands at a random phase of the clock and every hit differs slightly; Elektronauts users single this out as
something static samples miss (https://www.elektronauts.com/t/what-can-a-linn-drum-lm-1-lm-2-do-that-i-cant-do-on-a-digitakt/186151).
The hat PROM loops and its closed decay follows the pot, so closed hats also vary [VLINN].

### 2.3 Tuning by clock: what it does to the spectrum

- **The mechanism** [VLINN], [MAME-linndrum], [MAME-dmx]: the address counter advances one sample per clock
  tick and the DAC holds each value (zero-order hold, ZOH) until the next tick. Changing the clock changes
  pitch **and length** together, and there is no interpolation. VLINN notes that the hardware's weak or absent
  reconstruction filtering "results in a bright, gritty sound", and that interpolated modern pitch-shifting
  removes the images, so pitched-down samples "sound dull"; VLINN therefore decodes the bytes at the emulated
  clock in real time [VLINN].
- **Spectrum (calculated, standard sampling theory).** A table component at f (recorded at f_s0) played at
  clock f_c appears at f' = f · f_c/f_s0 and at the images k·f_c ± f' for k = 1, 2, ..., weighted by the ZOH
  response |sinc(f/f_c)| = |sin(π f/f_c)/(π f/f_c)| (-3.9 dB at f_c/2, a null at f_c), then by the analog
  filter. **The images move with the pitch**: tuning a 30 kHz voice down an octave puts the first image of a
  5 kHz component at 15 - 5 = 10 kHz, well inside the audio band, which is the metallic, "gritty" top of
  pitched-down 8-bit drums. Because the clock is an analog oscillator (XR2206, 74LS627, 555), there is no
  aliasing **inside the machine**: the images are exact, not folded.
- **The digital trap (inferred).** Rendering the held steps on a fixed host grid (44.1/48 kHz) by naive
  sample-and-hold folds every image above the host Nyquist back into the band: inharmonic, pitch-dependent
  aliasing the original never had. Pallor places each hold step as a band-limited step (a BLEP residual) at its
  exact sub-sample time, so images below the host Nyquist appear as in the hardware and nothing folds (§7.3,
  test T20). The repo's `shared/dsp/Blep.h` is a 2-sample polyBLEP, adequate for oscillators running at
  4x; for a ZOH at 26-30 kHz on a 44.1 kHz grid a longer windowed-sinc minBLEP table (16-32 taps) or a 2x
  render is needed (inferred).
- **Drop-sample at a fixed rate is different** (SP-12/SP-1200, MPC60): the output clock is fixed (26.04 or
  40 kHz) and pitch comes from skipping or repeating samples with a fractional phase accumulator. The images
  stay at multiples of the fixed rate, but the skipping itself **aliases** (it is resampling without a
  low-pass), which is the SP-1200's "authentic original, with aliasing & imaging artifacts" (Rossum,
  https://www.rossum-electro.com/pages/education-sp-1200). Pallor reproduces this at the virtual 26.04/40 kHz
  rate, where the aliasing is part of the sound, and then renders that rate's ZOH band-limited to the host.

### 2.4 Samplers of the late 80s and 90s

| Machine | Year | Word | Rate | Pitch | Filters | Outs | Sources |
|---|---|---|---|---|---|---|---|
| E-mu SP-12 | 1985 (shown as "Drumulator II" Feb 1985) | 12-bit linear | **26.04 kHz** | Tunable; multipitch over 8 pads | Reconstruction filter "deliberately omitted" (brighter from imaging) | 8 channels | https://en.wikipedia.org/wiki/E-mu_SP-12; https://www.muzines.co.uk/articles/e-mu-sp12/11851 |
| E-mu SP-1200 | Aug 1987; $2,995 | 12-bit linear | **26.04 kHz**, 10 s total, 2.5 s per sound | **Drop-sample** at the fixed rate | Ch 1-2: **SSM2044** dynamic 4-pole VCFs that start open and close after ≈ 200 ms (forum measurement); ch 3-4 fixed lower cutoff; ch 5-6 fixed higher cutoff; ch 7-8 **none** (raw DAC); mix sums all 8 unfiltered; cutoff values not published (**unverified**) | 8 + mix | https://en.wikipedia.org/wiki/E-mu_SP-1200; https://www.rossum-electro.com/pages/education-sp-1200; https://gearspace.com/threads/sp1200-internal-knobs-adjusting-dor.134782/ |
| Akai S900 | 1986 | 12-bit linear, 8 R-2R DACs (one per voice) | Variable **7.5-40 kHz** | **Per-voice clock** (8254 timers fed 8 MHz), no interpolation (derived from MAME's hardware map) | 9 × **MF6-50** switched-capacitor filters, each on its own timer, so the anti-image filter **tracks the voice clock** (the MF6-50 is a 6th-order Butterworth with f_c = f_clk/50; datasheet not re-checked, **unverified**) | 8 | https://en.wikipedia.org/wiki/Akai_S900; [MAME-s900] |
| Akai S950 | 1988 | 12-bit | Up to **48 kHz** | as S900 | as S900 | 8 | https://en.wikipedia.org/wiki/Akai_S900 |
| Akai MPC60 | Dec 1988; $5,000 | "**12-bit non-linear**" (Linn); L4003 DSP, 12-bit wave bus | Fixed **40.0 kHz** (35.84 MHz / 896); pitch as a 4.12 fixed-point increment, **no interpolation** (drop-sample) | Per-voice volume envelope; MAME converts the 12-bit words with a decompiled MPC3000 routine (a recursive filter-like stage) because it sounds closer than linear; the exact non-linear law is **unverified** | 8 individual + stereo; 16 voices | https://rogerlinndesign.com/about/about-museum; [MAME-l4003] |
| Akai MPC3000 | 1994 | 16-bit linear | 44.1 kHz | **8-point windowed-sinc interpolation** | Per-voice resonant digital low-pass | 8 + stereo; 32 voices | [MAME-mpc3000, l6028]; https://rogerlinndesign.com/about/about-museum |
| Roland S-50 / S-550 / S-330 | 1986-87 | 12-bit | 15 or 30 kHz | — | S-550/S-330 time-variant filter | — | https://en.wikipedia.org/wiki/Roland_S-50; https://articles.roland.com/a-history-of-roland-samplers/ |
| Ensoniq EPS / ASR-10 | 1988 / 1992 | EPS 13-bit; ASR-10 16-bit | ASR-10 29.76 or 44.1 kHz | Interpolating (OTIS/OTTO chips) | Digital filters | 8 with expander | https://en.wikipedia.org/wiki/Ensoniq_EPS; https://en.wikipedia.org/wiki/Ensoniq_ASR-10 |

Pallor's two sampler modes (§7.3) take the two clearest cases: **12-bit linear at 26.04 kHz with drop-sample
pitch** and a per-voice choice of the four SP-style output filters (dynamic, low, high, none); and **12-bit at
40 kHz** with drop-sample pitch (MPC-style; the "non-linear" coding is left out until a source documents it).

### 2.5 Cheap analog rhythm boxes

| Machine | Facts | Sources |
|---|---|---|
| Ace Tone R-1 / FR-1 Rhythm Ace | R-1 1964 (hand-played); FR-1 1967, 16 preset rhythms plus 4 manual buttons, distributed by Hammond | https://en.wikipedia.org/wiki/Ace_Tone |
| Korg Mini Pops 7 (1966), Minipops 120 (1976) | 15 sounds and 20 patterns; Jarre's *Oxygène* layers two presets | https://en.wikipedia.org/wiki/Korg_Mini_Pops |
| Roland CR-78 (1978) | First drum machine with a microprocessor and user-writable patterns (4 memories); 14 analog voices: bass, snare, rim, hi-hat, cymbal, maracas, claves, cowbell, 2 bongos, 2 congas, tambourine, guiro, **metallic beat** | https://en.wikipedia.org/wiki/Roland_CR-78; https://www.polynominal.com/m/roland-cr-78.htm |
| CR-78 cymbal and hat | "The bulk of the cymbal tones are created with resonant bandpass-filtered white noise", unlike the DR-110/606/808 square-wave clusters (an emulation vendor's documentation, secondary) | https://docs.cherryaudio.com/cherry-audio/instruments/cr-78/voice-edit |
| CR-78 "Metal Beat" | "Three atonally tuned square waves, distorted and filtered" add a metallic clink to the hat/cymbal when its slider is up (same secondary source; another source says ring modulation: mechanism **unverified**, see §3.9) | same |
| Boss DR-55 Dr. Rhythm (1979/80) | Four sounds (kick, snare, rim, hi-hat), six 16-step and two 12-step pattern memories, mono out | https://mixdownmag.com.au/features/gear-rundown-the-sisters-of-mercy/ |
| Korg KR-55, Roland CR-68/CR-8000 | Not researched in this session (follow-up) | — |

**Noise and metal voices of the cheap boxes (inferred from the rows above and §3).** Two families: filtered
white noise (CR-78 cymbal/hat, DR-55 hat, maracas) and clusters of detuned square waves through band-passes
(TR-606/808/DR-110 hats and cymbals, CR-78 metal beat). Pallor offers both in the hat and cymbal voices.

### 2.6 Hexagonal-pad electronic drums (Simmons)

| Model | Facts | Sources |
|---|---|---|
| SDS-V ("SDS5") | Prototype 1979, released 1981; developed with Richard James Burgess (Landscape), who shaped the hexagonal pads and the **descending ("pew") tom**, modelled on his own tuning technique; modules: bass, snare, 3 toms, optional cymbal and hi-hat. Per-module controls: **noise level, tone level, bend, decay, noise tone (a simple filter), click** (an attack that follows pad impact); 4 presets per module; balanced XLR outs; trigger input per module that accepts drum-machine audio | https://en.wikipedia.org/wiki/Simmons_SDS-V; https://en.wikipedia.org/wiki/Simmons_(electronic_drum_company) |
| SDS-7 (1983/84) | Modular sample player with analog filters: each module holds an **EPROM with an 8-bit sample** mixed with analog generation; up to 12 modules; 99 kits | https://www.simmonsmuseum.com/?area=devices&view=details&id=15 |
| SDS-8 | 5 channels, analog, same filters as the SDS-V | https://www.simmonsmuseum.com/?area=devices&view=details&id=5 |
| SDS-9 (1985) | 6 channels: snare and rim from EPROM samples, kick and toms analog; MIDI | https://www.simmonsmuseum.com/?area=devices&view=details&id=6 |
| Users | John Keeble (Spandau Ballet), Roger Taylor (Duran Duran), Warren Cann (Ultravox), Alan Wilder (Depeche Mode), Bill Bruford, Phil Collins and others | Simmons Wikipedia article |

"Second skin" as an SDS-7 control was **not found** in any source (**unverified**); circuit numbers in §3.8.

### 2.7 Syndrum and Synare

| Instrument | Facts | Sources |
|---|---|---|
| Syndrum (Pollard Industries, 1976) | Designed by Mark Barton for Joe Pollard; models 177 (single) and 477 (four); the signature sound on records is **a sine wave bending down** | https://en.wikipedia.org/wiki/Syndrum |
| Synare 3 (Star Instruments, 1977-82) | "Flying saucer" pad; **2 oscillators, white noise, a sweep for descending booms, filter with cutoff, resonance and decay**, battery powered; "used extensively in disco records". Synare 1: 1 oscillator (pulse/saw), noise, LFO, LPF with resonance, envelope | https://en.wikipedia.org/wiki/Synare |
| "Ring My Bell" (Anita Ward, 1979) | The song's article credits the **Synare** (citing Billboard 2010); the Syndrum article lists it too: Synare is the better-sourced answer | https://en.wikipedia.org/wiki/Ring_My_Bell |
| Joy Division | Synare 3 (Morris's accounts, §1.3) | §1.3 |

The "zap" in Pallor's FX voice and the hexagonal tom's bend come from this family: an oscillator with a
downward exponential sweep, noise, a resonant filter and a decay (inferred).

---

## 3. The circuits

Primary data for this section: the **TR-808 Service Notes** (Roland, 15 June 1981) [SN808], whose p.14 "Checking
Voices" table gives the frequency and decay of every voice, pp.5-6 the circuit description and p.9 the voice
schematic; the **TR-909 Service Notes** (15 June 1984) [SN909]; the **CR-78 Service Notes** (20 June 1979)
[SNCR78]; the **TR-606 schematic** [606]; Werner, Abel and Smith's **DAFx-14 bass drum** [W-BD] and **ICMC/SMC-14
cymbal** [W-CY] papers and **Werner's 2016 PhD thesis** [W-PhD], all read in full; Gordon Reid's *Synth Secrets*
drum series [SS]; Electric Druid's analyses of the DR-110 and KR-55 [ED-110], [ED-55]; and Mutable Instruments
Plaits (MIT licence) [PLAITS], whose drum models name the circuit parts they imitate. The AES "More Cowbell" paper
(Werner, Abel, Smith, AES 137th Convention, 2014, paper 9207) is paywalled and was not read; the 808 voicing board
page with the cowbell and hat filters is missing from the scan used. The SDS-V service manual sits behind a
captcha on the Simmons Museum site and was not read.

**Conventions.** "Derived" values are computed from component values with the formulas of §3.0; they are not
measurements. The 808 and CR-78 service notes define **decay time as the time for the waveform to fall to 1/10
of its peak-to-peak amplitude, i.e. -20 dB** ([SN808] p.14, [SNCR78] p.15), so for an exponential τ ≈ T/2.303
(calculated).

### 3.0 Formulas used

- **Bridged-T resonator (op-amp feedback type).** Denominator α₂s² + α₁s + 1 with α₂ = R₁R₂C₁C₂ and
  α₁ = R₁(C₁ + C₂), R₁ the bridge-to-ground resistor and R₂ the feedback resistor ([W-BD] §5, Eq. 5; [W-PhD]
  Table 2.2). Centre frequency f_c = 1/(2π√(R₁R₂C₁C₂)) ([W-BD] p.4, fn 16, citing the service notes). Derived:
  Q = √(R₁R₂C₁C₂)/(R₁(C₁+C₂)) = ½√(R₂/R₁) for C₁ = C₂, and envelope τ = 2Q/ω₀ = R₂·C for C₁ = C₂: **decay
  depends on R₂ and C, pitch also on R₁**.
- **Unity-gain Sallen-Key high-pass** (equal C, R_fb to the output, R_g to ground): f = 1/(2πC√(R_fb·R_g)),
  Q = ½√(R_g/R_fb) (standard).
- **CMOS Schmitt-inverter RC astable** ([W-CY] §3, Eqs. 1-3): t_charge = RC·ln((V_OH − V_T−)/(V_OH − V_T+)),
  t_discharge = RC·ln((V_OL − V_T+)/(V_OL − V_T−)); for the HD14584 at 5 V Werner gets a **duty cycle of
  47.98 %** and 5 V amplitude ([W-CY] §4). With assumed thresholds V_T+ = 2.9 V and V_T− = 1.9 V, T ≈ 0.812·RC
  (assumption, used for the TR-606 below).

### 3.1 TR-808 bass drum

**Measured** ([SN808] p.14, decay and tone at 12 o'clock, level at maximum, "values are typical and variable"):
period **18 ms → 56 Hz**; decay **50 / 300 / 800 ms** (to -20 dB) at short / mid / long, i.e. τ ≈ 22 / 130 /
350 ms (calculated); amplitude 3.5 Vpp normal, 10 Vpp accent.

**Components** ([W-PhD] Tables 2.2, 2.3, 4.1, 4.2; [SN808] p.9): resonator R165 47 k + R166 6.8 k (R₁ = 53.8 k),
R167 1 M (R₂), C41 = C42 = 15 nF; R161 1 M and R170 470 k in parallel with R₁ give an effective 49.0 k. Decay pot
VR6 500 k (B), C43 33 µF; tone VR5 10 k with C45 0.1 µF; level VR4 100 k. Supply 15 V. (Thesis Table 4.2 lists
R164 as 4.7 k; Table 2.3 and the schematic say 47 k.)

| Quantity | Value | Source |
|---|---|---|
| Resonance, effective R₁ = 49.0 k | **49.4 Hz** derived ("≈ 49.5 Hz" in the paper), Q ≈ 2.3 | [W-PhD] Table 2.2; [W-BD] |
| "Inherent oscillation period" in the circuit description | **16 ms (62.5 Hz)** | [SN808] p.5 |
| Measured period | 18 ms (56 Hz) | [SN808] p.14 |
| Instantaneous frequency, simulated | starts ≈ **57 Hz**, ripples once per cycle, settles at ≈ **49-50 Hz by ≈ 0.2-0.3 s** (read from Fig. 11) | [W-BD] §8.2 |
| Trigger | 1 ms pulse at the accent voltage, **4-14 V** (VR3); a pulse width other than 1 ms "will be a cause of deteriorative voices" | [W-BD] §3; [SN808] p.14 |
| Attack "pitch jump" | "immediately after a trigger pulse ... the filter's time constant, when ACCENT is present, is halved"; Q43 shorts R165 for **4 ms** (R156·C38; "1/2 × 1/2 of 16 ms"), so the filter's time constant halves and it resonates at **twice its frequency for a half cycle**; with R₁ ≈ 6.8 k the derived resonance is ≈ **129 Hz**; when Q42 turns on after 4 ms, C39 discharging through R161 produces a **retriggering pulse** | [SN808] p.5; derived |
| Same, Werner's account | the centre frequency rises by "more than an octave" for ≈ **6 ms**, less than one period at the higher frequency: heard as a punchier, crisper attack, not a pitch change; the envelope returns to ground ≈ 5 ms after the trigger falls | [W-BD] §8.1 |
| Pulse shaper | C40, R162, R163 and D53: a nonlinear low shelf; falling edge to ≈ V_trig·R162/(R162+R163) + 0.71 V; diode clamp V = 0.71(e^V − 1) for V < 0 | [W-BD] §4, Eq. 4 |
| Pitch "sigh" (glide) | leakage through R161 when the common node drops more than about a diode below ground turns Q43 partly on; fit i_C = (m/α)·log(1 + e^{α(V_comm − V₀)}) with **α = 14.3150, V₀ = 0.5560, m = 1.4765×10⁻⁵** | [W-BD] §8.2, Eq. 8 |
| Decay | VR6 sets a shelf in the feedback buffer: H_fb = ((R169 + VR6·k)C43·s + R169) / (R164(R169 + VR6·k)C43·s + R164) | [W-BD] §6 |
| Tone, level, output | low-pass R_eq·C45 with R_eq = R171 + R172‖VR5·l; level divider VR4; high-pass R177/R176 with C49 | [W-BD] Table 1 |
| Tolerances | ±20 % capacitors, ±5 % resistors: why units differ | [W-BD] §11 |
| Implementation | TDF-II filters with time-varying coefficients, bilinear transform with c = 2/T, a unit delay to break the feedback loop; **keep filter state across retriggers** to avoid the "machine-gun effect" | [W-BD] §10-11 |

**Reading the three frequencies (inferred).** 62.5 Hz (16 ms) is the designers' nominal; 56 Hz (18 ms) is a
period measured early in the sound; ≈ 49.5 Hz is where the glide settles. The "808 kick fundamental" is
therefore best modelled as a resonator at ≈ 49.5 Hz (tunable) with a glide from ≈ +15 % (57 Hz) decaying over
≈ 0.2-0.3 s, plus the 4-6 ms octave jump at the attack.

**A reusable parameterisation** ([PLAITS] `plaits/dsp/drums/analog_bass_drum.h`, MIT, © Emilie Gillet 2016; the
comments name the 808 parts): trigger pulse 1.0 ms of height 3 + 7·accent, decaying with τ = 0.2 ms after the
trigger; pulse filter one-pole τ = 0.1 ms with a diode (x ≥ 0 → x, else 0.7·2x/(1+|2x|)) (C40/R163/R162/D53);
attack-FM pulse **6 ms** multiplying the frequency by 1 + 1.7·amount (Q41-Q43/R165); a retrigger pulse of −0.8
at its end, decaying with τ = 50 ms (C39/R161); self-FM ("sigh") f·(1 + 0.08·amount·(0.7 + diode(10·lp − 1)))
(Q43/R170 leakage); the resonator as an SVF band-pass with Q = 1 + 1500·2^(decay·80/12)·f; tone as a one-pole
low-pass at min(4·f0·2^(tone·108/12), Nyquist). Plaits is MIT-licensed, so its structure may be studied and,
with attribution, reused; Pallor's own model follows [W-BD] (inferred).

### 3.2 TR-808 snare

| Element | Value | Source |
|---|---|---|
| Low resonator (measured) | period 4.2 ms → **238 Hz** | [SN808] p.14 |
| High resonator (measured) | period 2.1 ms → **476 Hz** (ratio 2.00) | [SN808] p.14 |
| Low resonator (components) | R196 680 Ω, R197 820 k, C58 = C59 27 nF → **250 Hz** derived, Q ≈ 17, τ ≈ 22 ms | [W-PhD] Table 2.2; derived |
| High resonator (components) | R195 2.2 k, R198 1 M, C60 = C61 6.8 nF → **499 Hz** derived, Q ≈ 11, τ ≈ 6.8 ms | same |
| Tone | VR8 (100 k B) sets the ratio of the two resonators | [SN808] p.6 |
| Snappy | noise through the Q48 "swing" VCA (envelope R184/R185 15 k, C51 0.47 µF, C52 0.1 µF, level VR9) into a **Sallen-Key high-pass** (C66 = C67 1.8 nF, R201 22 k, R202 47 k): **≈ 2.75 kHz, Q ≈ 0.73** derived | [SN808] p.6, p.9; derived |
| Decay | **60 ms** (to -20 dB) | [SN808] p.14 |
| Schematic change note "portion changed - SD" | C58 → 0.056 µF, C61 → 0.015 µF, R191 → 10 k, R200 → 0: derived 173/336 Hz, which no longer match the table; direction and production run **unverified**; Werner uses the original values | [SN808] p.9 |
| Reid | two bridged-T "oscillators" in fixed proportion plus white noise through a VCA, high-passed; no Hz values for the 808 | [SS] "Practical Snare Drum Synthesis", Apr 2002 |
| Plaits interpretation | 1 ms trigger; noise half-wave rectified (Q48); noise band-pass at 16 × f0 with Q = 1 + 1.5·f; mode ratio 1 : 2.00, extra modes 3.18, 4.16, 5.62 above tone 0.667 | [PLAITS] `analog_snare_drum.h` |

The commonly quoted 238/476 Hz come from the service notes (as quoted at https://scrapbox.io/0b5vr/TR-808), not
from Reid.

### 3.3 TR-808 cymbal, open and closed hats (the six-oscillator source)

| Element | Value | Source |
|---|---|---|
| Oscillators | six HD14584 Schmitt-inverter RC oscillators shared by cowbell, cymbal and both hats; in Werner's order **205.3, 369.6, 304.4, 522.7 Hz**, #5 **359.4-1149.9 Hz** (trimmer TM2, factory **800 Hz**), #6 **254.3-627.2 Hz** (TM1, factory **540 Hz**); i.e. the set **205.3, 304.4, 369.6, 522.7, 540, 800 Hz** | [W-CY] §4 |
| Waveform | square, duty 47.98 %, 5 V | [W-CY] §4 |
| Early units | different R/C on #5-6 before serial 000300; from 000300 C6 0.01 → 0.022 µF, R44 390 k → 150 k, R45 330 k → 100 k "to extend TM1 and TM2 control range" | [W-CY] fn 9; [SN808] p.15 |
| Mix | summed through a resistor network into R53 (making R53 variable is a strong "bend") | [W-CY] Eq. 4, §12 |
| Band-pass #1 | **≈ 3,440 Hz**, 3rd order | [W-CY] §4, Eq. 5 |
| Band-pass #2 | **≈ 7,100 Hz**, 3rd order (halving R56/R58 raises both centres) | [W-CY] §4, §13 |
| Trigger | 1 ms at **7-14 V** (accent) | [W-CY] §5 |
| Attack smoother | one-pole fit, V_BE = 0.7258 V, **τ = 0.10244 ms** | [W-CY] §6 |
| Envelope | diode V_on = 0.5899 V; release τ = (VR2·k ‖ R93)·C41 (VR2 = cymbal decay); emitter follower V_BE = 0.6195 V; EG 1.2 a high shelf H = R104/(sR104R105C45 + R104 + R105) | [W-CY] Eqs. 9-11 |
| "Swing" VCAs | common-emitter-like stages powered by the envelope; lower edge fitted as a sum of two exponentials of V_env plus a constant (Table 1; smoothing exponent ≈ 3.5) | [W-CY] §8 |
| High-passes | HPF 1: 2nd-order Sallen-Key with buffer; HPF 2: 2nd order, non-unity gain; HPF 3: 3rd order with a **resonance at the corner near 10.5 kHz** | [W-CY] §9-11 |
| Tone and level | passive 5th-order tone network (mainly attenuates band 3); the level stage differentiates, **+6 dB/oct** across the band | [W-CY] §10-11 |
| Band routing | low band (BP 1) through a VCA with user decay; high band (BP 2) split: Q16 = "highest frequency component ... decay time is short", Q17 = slightly lower range; VR4 (tone) sets the balance of the three | [SN808] p.6; [SS] "Practical Cymbal Synthesis", Jul 2002 |
| Hats | OH: high band gated by Q27, then a high-pass (Q26) and buffer; CH: same source gated by Q30, high-pass Q31; **a CH trigger turns Q23 on through R173, cutting the OH decay (the choke)** | [SN808] p.6 |
| Decays (to -20 dB) | **CY 350 / 800 / 1,200 ms**; **OH 90 / 450 / 600 ms**; **CH 50 ms** (short/mid/long where adjustable) | [SN808] p.14 |
| Modelling advice | 4x oversampling; naive (aliasing) squares are acceptable because the band-passes remove most aliased energy | [W-CY] §12 |

Cross-check: the Boss DR-110 uses **four squares at 305, 444, 794 and 1,136 Hz** plus noise, through a "low metal"
band-pass at **3.45 kHz** and a "high metal" one at **7.12 kHz**, both Q 6.05, almost exactly the 808's centres;
decays OH 700 ms, CH 80 ms, cymbal "ting" 60 ms and body 900 ms, tail 1.4 s; its noise is an **18-stage LFSR at
80 kHz with taps 13 ⊕ 18** [ED-110] (https://electricdruid.net/analogue-drums-boss-dr-110-drum-machine/). Plaits'
808-style hat uses ratios 1 : 1.304 : 1.466 : 1.787 : 1.932 : 2.536 on 414 Hz (414/540/607/740/800/1,050 Hz),
**not** the 808's set [PLAITS] `hi_hat.h`.

### 3.4 TR-808 cowbell

- Oscillators #5 and #6 at **800 Hz and 540 Hz** (periods 1.25 and 1.85 ms, set by TM-2 and TM-1), each through a
  swing VCA (Q14, Q15), summed into the band-pass filter IC2 ([SN808] p.6, p.14).
- **Two-slope envelope**: "a series of R82 and C34 connected in parallel with C9 forms an envelope having abrupt
  level decay at the initial trailing edge to emphasize attack effect" ([SN808] p.6, quoted by Reid in [SS]
  "Synthesizing Cowbells & Claves", Sep 2002). Overall decay **50 ms** to -20 dB ([SN808] p.14).
- **Not available**: the IC2 band-pass centre and Q, and the R82/C34/C9 values (voicing board page missing from
  the scan; the AES paper paywalled). **Unverified.**
- Reid's own patch (modelled on the CR-8000, not the 808): tones at **587 and 845 Hz** (ratio 1 : 1.44), triangle
  waves, band-pass **2.64 kHz** at 12 dB/oct with a little resonance, a three-stage attack/decay1/decay2 envelope
  ([SS] Sep 2002). CR-78 cowbell: 800 and 555 Hz through a tank derived at ≈ 3.46 kHz (§3.9).
- **Pallor starting values (inferred)**: squares at 540 and 800 Hz, band-pass ≈ 2.6 kHz (Q ≈ 3) plus a gentle
  high-pass at ≈ 500 Hz, envelope = fast segment τ ≈ 2-4 ms falling by ≈ 10-12 dB, then τ ≈ 22 ms (50 ms to
  -20 dB); tune these against a recording, test T14.

### 3.5 TR-808 hand clap and maracas

| Element | Value | Source |
|---|---|---|
| Band-pass | block diagram label "**BAND PASS FILTER 1000 Hz**"; components C128 = C129 4.7 nF, R334 100 k, R333 10 k (with R332 27 k ‖ C127 1.8 nF, R342 15 k) → as a bridged-T **≈ 1.07 kHz, Q ≈ 1.6** derived | [SN808] p.7, p.9; derived |
| Two VCAs in parallel | Q70, "a relatively long decay time ... constitutes reverberation of CP sound" (C139 0.018 µF, R348/R349 1 M, C137 0.033 µF); IC22 (BA662) with Q71/Q72 applies the "sawtooth" envelope | [SN808] p.6, p.9 |
| Sawtooth generator | the trigger is integrated by R350 1 M and C140 2.7 nF into a **30 ms** window; within it C144 0.47 µF is repeatedly charged to −15 V and discharged through R365 82 k and D71; the window ends "in the middle of the third time" and the slow decay begins | [SN808] p.6 |
| Burst count and spacing | figure shows **4 bursts**: 3 short sawtooth bursts in the 30 ms window, then the long tail; spacing **≈ 10 ms** (30 ms / 3, inferred from the figure; not printed). Derived discharge τ of each burst R365·C144 = 82 k × 0.47 µF = **38.5 ms** (calculated; the comparator restarts it before it finishes) | [SN808] p.6 Fig. 13-7; calculated |
| Overall decay | **100 ms** (to -20 dB) | [SN808] p.14 |
| Level change | "CP sound overmatches the rest in level": R346 1 k → 680 Ω, R352 22 k → 27 k from serial 010600 | [SN808] p.15 |
| Maracas | noise gated by Q65, envelope Q66/Q67, through a Sallen-Key **high-pass** Q68 (C132 = C133 1 nF, R339 3.3 k, R340 68 k): **≈ 10.6 kHz, Q ≈ 2.3** derived; decay **25 / 35 ms** | [SN808] p.6, p.9, p.14 |
| Reid | wrote no clap or tom article: toms, rimshots, claps and congas "use the principles we have already discussed" | [SS] Sep 2002 |

Cross-check: the DR-110 clap is a bridged-T band-pass at **≈ 910 Hz**, last-clap decay 140 ms and a "reverb" decay
of 700 ms [ED-110].

### 3.6 TR-808 rim shot, claves, toms and congas

| Voice | Measured frequency | Decay (to -20 dB) | Components / notes | Source |
|---|---|---|---|---|
| Claves | 0.4 ms → **2,500 Hz** | 25 ms | bridged-T R312 1 k, R308 820 k, C117 = C119 2.2 nF → 2,526 Hz derived (Q ≈ 14); JFET Q74 conducts except during the trigger to suppress leakage, since the resonator is "wired for high Q" | [SN808] p.6, p.9, p.14; [W-PhD] Table 2.2 |
| Rim shot | **455 Hz + 1,667 Hz** (periods 2.2 and 0.6 ms) | 10 ms | low resonator R315 5.6 k, R316 1 M, C 4.7 nF → 452 Hz derived; the claves resonator reconfigured by SW11 gives the high one; both into swing VCA Q62 (envelope R107 1 k, C24 0.47 µF); a Sallen-Key section Q63 derived ≈ 590 Hz, Q ≈ 2.9 (role uncertain) | same |
| Low tom / conga (tune low/mid/high) | LT **80 / 90 / 100 Hz**; LC **165 / 185 / 220 Hz** | LT 200 ms; LC 180 ms | multi-feedback bridged-T; SW8 swaps tom ↔ conga | [SN808] p.14 |
| Mid tom / conga | MT **120 / 135 / 160 Hz**; MC **250 / 280 / 310 Hz** | MT 130; MC 100 ms | — | same |
| High tom / conga | HT **165 / 185 / 220 Hz** (the same printed values as LC: possibly a misprint, unverified); HC **370 / 400 / 455 Hz** | HT 100; HC 80 ms | — | same |
| Tom pitch glide | "While the oscillation is large in amplitude immediately after triggering, it is on a higher frequency due to conductions of D80 and D81, which reduce time constant of the filter. As the resonance is damped, its frequency is lowered" | — | amplitude-dependent pitch: model as f = f0·(1 + k·env) | [SN808] p.6 |
| Low tom "reverb" | "pink noise with a slightly longer decay time is mixed" (Q52 VCA, C72 0.039 µF, R218 2.2 M → τ ≈ 86 ms derived; low-pass R222 22 k / C75 0.018 µF → ≈ 400 Hz derived) | — | | [SN808] p.6, p.9 |

The simple bridged-T formula reproduces the kick, snare, rim and claves within ≈ 5 %, but the toms and congas
measure ≈ 0.58× the formula because of a second inverting stage feeding back through R230 820 k (not in the
simplified table); **use the measured Hz** for toms and congas ([SN808] p.9; derived). Werner linearises the
tom tuning branch as R₁(x) = 500·(x² − x + 2)/(x + 2) Ω, 500 → 333 Ω over the knob ([W-PhD] App. C, Eq. C.12).

### 3.7 TR-909 (structure; numbers mostly unverified)

- **Bass drum** ([SN909] p.4 block diagram; [SS] "Practical Bass Drum Synthesis", Feb 2002): CV generator → VCO
  (IC13) → waveshaper → VCA (ENV1); pitch envelope ENV3 with instant attack and slow decay; a click from a pulse
  generator (Q8/Q9, "attack level"); a noise path, low-passed, through its own VCA (ENV2); accent scales the
  envelopes. Reid: the oscillator makes a sawtooth whose pitch follows EG3 and the waveshaper turns it into
  "something very close to a sine wave"; the noise path is low-passed and "mixed with a short pulse (essentially
  a click)".
- **909 kick numbers**: start/end frequency and sweep time could not be read from the scope page ([SN909] p.9 is a
  low-resolution scan, BD at 5 ms/div). A GitHub issue fitting a model to 909 **sample-pack** recordings reports a
  settled pitch of **49.2-49.9 Hz** and a "hard" kick at ≈ **166 Hz at 20 ms** and ≈ **111 Hz at 40 ms**
  (https://github.com/tacowars/windsor/issues/329): model fits to samples, **unverified**.
- **Plaits "synthetic" kick** ("inadvertently 909-ish", [PLAITS] `synthetic_bass_drum.h`): triangle → x/(1+|x|)
  waveshaper; FM envelope τ = 8 ms × (1 + 4·amount²), peak frequency ratio up to ×4.5; phase held at 0.25 for
  1.3 ms after the trigger; body envelope τ = 20 ms × 2^(decay²·60/12) with a 1 ms hold; click = pulse through
  an SVF band-pass at **5 kHz, Q 2**, plus noise, transient τ = 5 ms; transistor VCA 3s/(2 + |s|).
- **Snare** ([SN909] p.6): two VCOs of identical circuit (hysteresis comparator, inverter "voltage-dependent
  resistor", integrator) differing only in timing capacitors; **VCO-1 lower**; triangle outputs, phase-reset on
  each trigger; C69's charging rate "continuously changes for about **20 ms**", a pitch bend on VCO-1; snappy
  noise → low-pass, then split into a high-passed VCA path (accent) and a plain VCA path. The VCO frequencies are
  **unverified** (Reid places a real snare's (0,1) modes near 180 and 330 Hz, [SS] Mar/Apr 2002).
- **Noise**: "a quasirandom noise generator having two shift registers ... in cascade making up **32 stages**"
  ([SN909] p.6); clock and taps not given.
- **Hats and cymbals**: ROM → latch → DAC → VCA → low-pass, with the envelope derived from the address counter
  ([SN909] p.4); Reid: a **6-bit** table clocked by a ≈ **30 kHz** oscillator that sets the tuning, so the decay
  tracks the tuning ([SS] Jul 2002).
- **Clap**: band-pass → VCA with a sawtooth envelope plus a second decay VCA, sharing the snare's noise; **rim
  shot**: three band-passes → clipper → VCA → high-pass ([SN909] p.4).

### 3.8 Hexagonal electronic drums (Simmons)

- **SDS-V panel** (§2.6): noise level, tone level, bend, decay, noise tone (filter), click; 4 presets per module
  (https://en.wikipedia.org/wiki/Simmons_SDS-V). **No SDS-V circuit numbers were verified** (manual behind a
  captcha; search budget exhausted).
- **Related, verified: the SDS 800/400/200 snare-and-tom voice** (Simmons SDS series schematics, sheet 800/2, from
  the manuals.fdiskc.com archive): one **CEM3394** voice chip (VCO, VCA, VCF, noise/tone balance, pulse width, wave
  select, resonance); noise from a 2N5172 junction into an LM301; amplitude envelope C3 1 µF discharging through
  R7 470 k (10 k on the snare) + VR15 1 M: τ ≈ **0.47-1.47 s** on toms and **0.01-1.01 s** on the snare (derived);
  the pad drives a peak detector (C2 20 nF) so level follows the hit; the bend comes from a separate envelope
  into the VCO/VCF (panel mapping unverified).
- **Pallor's hexagonal voices (inferred)**: oscillator (sine/triangle/pulse) with **bend** = exponential pitch
  drop of 0-24 semitones with τ 20-400 ms (the "pew" lives at 12-24 semitones over 100-300 ms), **noise** through
  a resonant low-pass "noise tone" filter, **click** = a 1-3 ms pulse through a band-pass at 2-5 kHz scaled by
  velocity, decay 50 ms-1.5 s, and level following velocity through a peak detector. Tune by ear against
  recordings; record the chosen numbers in the code.

### 3.9 Roland CR-78 (and the cheap-box metal)

**Adjustment table** ([SNCR78] p.15; decay to 1/10 V):

| Voice | Frequency | Decay |
|---|---|---|
| Bass drum | 16 ms → **62.5 Hz** | 100 ms |
| Snare | 3.0 ms → **340 Hz** ("drum" part) | 60 ms |
| Rim shot | **≈ 1,480-1,500 Hz** (printed 6.67, read as 0.67 ms) | 5 ms |
| Hi-hat / cymbal / maracas | — | **60 / 350 / 20 ms** (trimmed together by VR60) |
| Claves | 0.43 ms → **2,630 Hz** | 18 ms |
| High / low bongo | **600 / 400 Hz** | 40 ms |
| Low conga | **208 Hz** | 150 ms |
| Cowbell high / low | **800 / 555 Hz** | 60 ms |
| Tambourine | — | 220 ms |
| Guiro high / low | **125 / 77 Hz** (scrape rate) | — |
| **Metal beat** high / mid / low | **6,170 / 5,620 / 4,080 Hz** | 50 ms |

**Construction** ([SNCR78] pp.12-13, voicing board VG-11A):
- Bass, conga and bongos are single-transistor ringing oscillators; claves and rim are LC tanks.
- Noise: a reverse-biased junction (2SC828 "for noise"), buffered.
- **Cymbal, hat, maracas: noise-based** voices with their own envelope transistors; the cymbal passes an LC tank
  (45 mH, 6.8 nF) derived ≈ **9.1 kHz**. No square oscillators on these voices (my reading of the schematic,
  **unverified**). Cherry Audio's documentation of its CR-78 recreation agrees: "the bulk of the cymbal tones are
  created with resonant bandpass-filtered white noise" (https://docs.cherryaudio.com/cherry-audio/instruments/cr-78/voice-edit).
- **Metal beat: three CMOS inverter oscillators** (MC14069) at **6,170, 5,620 and 4,080 Hz**, mixed through 470 k
  resistors into an LC tank (45 mH, 18 nF) derived ≈ **5.6 kHz**, with its own envelope (50 ms). Cherry Audio
  describes it as "three atonally tuned square waves, distorted and filtered" that add a metallic clink to hat
  and cymbal hits when its slider is raised; another source says ring modulation, so the mixing mechanism is
  unverified, but the service notes list three oscillators and no ring modulator.
- Cowbell: a phase-shift oscillator (800 Hz) and a twin-T-style oscillator (555 Hz) through a 45 mH + 47 nF tank,
  ≈ 3.46 kHz derived. Guiro: an astable multivibrator chopping noise at 125/77 Hz through a ≈ 6.1 kHz tank.
  Tambourine: noise with envelope into an LC tank.
- Reid: the CR-68/CR-78 kick does **not** fall in pitch over its decay, unlike the 808 ([SS] Feb 2002).

**TR-606 metal** ([606] sheet 6): six HD14584 oscillators mixed through 150 k resistors; with T ≈ 0.812·RC
(assumed thresholds) they derive to **≈ 147, 183, 218, 249 (121 from lot 84, R226 → 680 k), 262 and 373 Hz**,
lower than the 808's set (topology assumed, capacitor placement ambiguous in the scan: **unverified**). The rest
of the 606 uses bridged-T resonators and swing VCAs like the 808.

**Korg KR-55** [ED-55] (https://electricdruid.net/analogue-drums-korg-kr-55/): two-stage VCA envelopes (fast branch
220 k, slow 6.6 M); snare fast envelope ≈ 5 ms with noise 50-60 ms; closed hat ≈ 130 ms; claves ≈ 25 ms;
conga/tom trimmer range ≈ 9.5 semitones at Q 35-61; metallic sources from comparator squares combined in XOR
"ring" pairs (six oscillators, three XOR pairs).

### 3.10 Summary for the Analog engine (inferred)

| Voice model | Core | Key numbers to start from |
|---|---|---|
| Bridged-T kick | resonator ≈ 49.5 Hz (tune ±1 octave), Q set by decay; trigger pulse 1 ms through the shaper; 4-6 ms attack at ≈ 2× frequency; glide from ≈ +15 % settling in 0.2-0.3 s; decay 50-800 ms to -20 dB; tone low-pass | §3.1 |
| 909-style kick | VCO (triangle) → soft waveshaper; pitch envelope from ≈ 3-4.5× down to ≈ 50 Hz with τ ≈ 8-40 ms; click = 1-2 ms pulse + noise through a 5 kHz band-pass | §3.7 (unverified numbers) |
| Cheap-box kick | single ringing resonator ≈ 62 Hz, no glide, decay ≈ 100 ms | §3.9 |
| Two-resonator snare | 238 + 476 Hz (ratio 2.0), tone balance, snappy noise through a 2.75 kHz Sallen-Key high-pass, decay 60 ms | §3.2 |
| 909-style snare | two triangle VCOs phase-reset, VCO-1 bending for ≈ 20 ms; noise low-passed, split high-passed/plain | §3.7 |
| Clap | noise → ≈ 1 kHz band-pass (Q ≈ 1.6); 3 bursts ≈ 10 ms apart in a 30 ms window, then a ≈ 100 ms tail through a parallel "reverb" VCA | §3.5 |
| Six-square metal | 205.3, 304.4, 369.6, 522.7, 540, 800 Hz squares (48 % duty) → band-passes 3.44 and 7.1 kHz → VCAs → high-passes (≈ 10.5 kHz resonant on the top); CH 50 ms, OH 90-600 ms, CY 350-1,200 ms; CH chokes OH | §3.3 |
| Cheap-box noise hat | white noise → resonant band-pass/LC tank ≈ 9 kHz, decay 60 ms (CR-78) | §3.9 |
| CR-78 metal | three squares at 4,080, 5,620, 6,170 Hz → ≈ 5.6 kHz resonant filter, 50 ms | §3.9 |
| Cowbell | 540 + 800 Hz squares → band-pass ≈ 2.6 kHz (unverified) → two-slope envelope, 50 ms overall | §3.4 |
| Claves / rim | resonators 2,500 Hz (25 ms); 455 + 1,667 Hz (10 ms) | §3.6 |
| Toms / congas (808) | measured Hz of §3.6 with amplitude-dependent pitch (higher while loud) | §3.6 |
| Maracas | noise → ≈ 10.6 kHz high-pass, 25-35 ms | §3.5 |
| Hexagonal toms/snare/kick | oscillator + bend (0-24 st, 20-400 ms) + filtered noise + click | §3.8 (inferred) |

---

## 4. Swing

### 4.1 What Roger Linn says

Roger Linn, interviewed by Attack Magazine (2 July 2013,
https://www.attackmagazine.com/features/interview/roger-linn-swing-groove-magic-mpc-timing/ and page 2
https://www.attackmagazine.com/features/interview/roger-linn-swing-groove-magic-mpc-timing/2/), paraphrased:

- **Implementation**: swing delays the **second 16th note of each 8th-note pair**, that is the even 16ths
  (2, 4, 6, 8 ... counting from 1). The odd 16ths never move.
- **Definition**: the swing amount is the share of the pair's duration given to the first 16th. **50 %** is
  straight; **66 %** is perfect triplet swing (the first 16th gets 2/3 of the pair, the second lands on the
  8th-note triplet).
- **Musical guidance**: at 90 BPM, 62 % feels looser than the "perfect" 66 %; on straight-16th beats 54 %
  loosens the feel without sounding swung; the useful range is about 50-70 %.
- **History**: swing ("shuffle") arrived on the LM-1 in 1979, made by delaying every other 16th; quantise
  began as a RAM-saving measure that snapped recorded 16ths to the grid and was called "timing correct".
- **Resolution**: the LM-1 sequencer ran at **48 PPQ**, which Linn says allows **50, 54, 58, 60, 62, 66, 70 and
  75 %**; most good grooves need 48 PPQ or less.
- **Randomness**: random timing and a late snare on 2 and 4 "never" did much good in his experience; grooves
  work best with notes on exact slots once dynamics and swing are right. He also says even velocity on hats
  makes "a bad drum machine beat" (quoted in §6).
- Reader comments under the article (anecdotal, not Linn): some LM-1/LinnDrum snare samples start up to ≈ 10 ms
  late because of trimming; MPC3000 internal sync jitter ≈ 3 samples (0.06 ms).

**MPC60 Operator's Manual v3.1** (Linn and Battino, rev. 22 January 2002,
https://www.polynominal.com/akai-mpc60/akai-mpc60-manual.pdf): the sequencer runs at **96 PPQ** (printed p. 26);
the Timing Correct page (printed pp. 43-45) offers note values 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T and off
(1/384); the **Swing % field appears only for 1/16 and 1/8**, so the MPC60 swings both 16ths and 8ths;
amount = "percentage of time given to the first note in each pair", **range 50-75 %**: 50 % even, 66 %
"technically perfect swing" (first note twice the second), 75 % maximum (three times the second, "very
exaggerated"); suggested settings 1/16 at 54 % (removes stiffness from 16th hats at 100 BPM) and 1/16 at 62 %
("more relaxed than a perfect triplet swing"). Swing is applied while recording, and a soft key moves existing
data. A separate **shift** moves a track earlier or later by up to 11 ticks at 1/16 (just under half a step).

### 4.2 Arithmetic (calculated)

With a step of length `d` (a 16th: `d = 60/(4·BPM)` s; an 8th: twice that) and swing `s` in [0.50, 0.75]:

```
i          = 0-based step index at the swing resolution
pairStart  = floor(i / 2) * 2 * d
onset(i)   = pairStart                  if i is even   // first of the pair, never moved
           = pairStart + s * 2 * d      if i is odd    // = straight position + (s - 0.5) * 2 * d
```

At 120 BPM (`d` = 125 ms for 16ths) the second 16th moves by (s - 0.5)·250 ms: 54 % → 10 ms, 58 % → 20 ms,
62 % → 30 ms, 66.7 % → 41.7 ms, 71 % → 52.5 ms, 75 % → 62.5 ms.

**Tick quantisation.** At 48 PPQ a 16th pair is 24 ticks and one tick = 4.17 % of the pair, so the reachable
settings are 50, 54.2, 58.3, 62.5, 66.7, 70.8 and 75 %, i.e. the commonly quoted **50/54/58/62/66/71/75 %**
(Linn's "60" and "70" are not multiples of 1/24 and are probably approximate labels, **unverified**). At 96 PPQ
a 16th pair is 48 ticks (2.08 % per tick): 54 % → 26 ticks = 54.17 %, 62 % → 30 = 62.5 %, 66 % → 32 = 66.67 %,
75 % → 36. Whether the MPC60 rounds or truncates is **unverified**; a forum timing analysis of the MPC2000XL
found adjacent settings (53/54 %) landing on the same tick, consistent with tick quantisation ([snippet],
https://www.mpc-forums.com/viewtopic.php?p=1392519). A forum quote describes the LM-1 front panel as six LEDs
from 50 % to 70 % in 4 % steps ([snippet], https://www.mpc-forums.com/viewtopic.php?p=230742); Wikipedia says
the Rev 1 LM-1 had no shuffle LEDs (https://en.wikipedia.org/wiki/Linn_LM-1).

**Conflicts.** A 2017 blog says the LM-1 and SP-1200 used 96 PPQ and that "around 65 %" is a triplet
(https://melodiefabriek.com/blog/mpc-swing-reason); Linn's own figures (48 PPQ, 66.7 %) win. The same blog notes
that Reason's ReGroove is MPC-style (50-75 %, default 62 %) and that swing on an 8th-only pattern does nothing,
since only even 16ths move. Elektron machines use the same definition with a 50-80 % range ([snippet, forum],
https://www.elektronauts.com/t/swing/4512). SP-1200 swing steps: **unverified**.

### 4.3 Pallor's swing (inferred)

- Global swing 50-75 % at **16th** or **8th** resolution, Linn's definition (share of the pair given to the
  first step). Steps of 1 %, with detents at 50, 54, 58, 62, 66.7 and 71 %.
- A "machine ticks" option rounds the delayed onset to 1/48 or 1/96 of a quarter (LM-1 / MPC feel at low
  tempos, where a tick is several ms); default off (exact).
- Per-track swing override (users praise it in Triaz and Playbeat, §6) and per-track "shift" (± up to half a
  step) for the documented "clap a few ms early" and "tambourine ahead of the snare" recipes (§1.2).
- 8th swing moves 16th positions 3, 7, 11, 15 (1-based); 16th swing moves 2, 4, 6 ... For tracks whose step
  length differs from the swing resolution, swing is applied in musical time (the step's ppq position is
  remapped through the swing function), so a 1/32 track swings with the 16ths (inferred).
- Swing is computed on the musical position, then converted to samples once per event (test T4).

---

## 5. Studio effects of the 80s and 90s on drums

The web search budget of the session ran out while the digital delays, tape echo and BBE were being
researched; values there that could not be confirmed are marked **unverified**, and §9 lists the follow-ups.
Search-engine excerpts of pages that could not be opened are tagged **[snippet]**.

### 5.1 Gated reverb

- **Routing** (https://en.wikipedia.org/wiki/Gated_reverb; SOS Q&A, Steve Howell, March 2005,
  https://www.soundonsound.com/sound-advice/q-how-do-set-gated-reverb): the dry drum feeds a reverb (optionally
  through a compressor), the reverb return goes through a gate, and the **gate's key is the dry drum**, so the
  drum's attack opens it rather than the reverb's onset; attack "instant", release "pretty much instant",
  hold to taste; the threshold set to avoid chatter in the tail. The live-room version gates ambience mics
  keyed by close mics with a hold of "half a second or so" and a fast release (Wikipedia).
- **The original** (§1.4): a heavily compressed room ("listen") mic gated by a channel gate; Padgham recalls
  the gate closing ≈ 1.5 s after the hit (MusicTech 2019), longer than most later descriptions.
- **The classic gate, Drawmer DS201** (operator's manual p. 5, https://r2.gear4music.com/media/18/182269/download_182269.pdf):
  key filters high-pass 25 Hz-4 kHz and low-pass 250 Hz-35 kHz; **attack 10 µs-1 s; hold 2 ms-2 s; decay
  2 ms-4 s; range 0-90 dB**; external key; gate/duck.
- **Recipes with numbers**: hold ≈ 150 ms and release ≈ 148 ms at threshold ≈ -32 dB (MusicRadar, search
  summary); hold 0.5-1 s with a fast release (eMastered); ≈ 0.5 s adjusted to tempo (Ledgernote); a gated
  kick reverb of 0.22 s, a gated clap reverb of 4.21 s cut by the gate (Attack); URLs in §1.4.
- **DSP recipe (inferred)**: a dense room or hall at 100 % wet with RT60 1-2 s, optionally compressed hard
  (ratio ≥ 10:1, attack ≤ 1 ms, release 50-100 ms) to flatten its envelope (the listen-mic sound); a gate on
  the return keyed by the dry sum of the voices that send to it, with a key high-pass, attack 0.01-1 ms,
  **hold 100-600 ms** (default 250 ms, tempo-syncable), release 5-50 ms (default 15 ms) and range 60-90 dB; a
  "re-trigger" mode where each new hit restarts the hold. Gate time = hold + release.

### 5.2 Nonlinear and reverse reverb

**AMS RMX16** (manual Issue 1, reproduced in Universal Audio's "AMS RMX16 Expanded" manual,
https://media.uaudio.com/support/manuals/dd/AMS%20RMX16%20Expanded%20Manual.pdf, PDF pp. 8-19; Hugh Robjohns,
SOS, January 2015, https://www.soundonsound.com/reviews/ua-rmx16):

- Hardware: 16-bit data path, **18 kHz bandwidth** (20 Hz-18 kHz, -3/+0 dB), 90 dB dynamic range, 0.03 %
  distortion in delay mode, S/N "typically 86 dB"; gain-ranging 12-bit converters ("pseudo-16-bit"), 28-bit
  internal words; ≈ 40 kHz sampling ("if memory serves", Robjohns: soft). Released March 1982.
- Controls: pre-delay (1 ms resolution, up to 200-300 ms by program), decay in 0.1 s steps, LF and HF decay
  filters as a signed digit -9...+9.
- **NonLin 2** (program 8): "the sound decays only very slightly for a period of time (set by the decay
  control), and then dies away very rapidly"; meant for drums; at small settings a good room; **decay values
  are not calibrated**; stereo.
- **NonLin 1** (program 18): the sound "does not decay" for the set time, then dies away very rapidly; two
  mono outputs, one diffuse and one of discrete reflections; short settings multiply percussive sounds such
  as hand claps.
- **Reverse 1 / 2** (programs 9, 10): the reverb "builds up for a period of time then suddenly stops"; Reverse 1
  has a diffuse left and a discrete-reflection right output; pre-delay before the build-up.
- Other programs for reference: Ambience (decay 9.9 = almost infinite), Room A1 (realistic at 1-2 s), Plate A1/B1
  (useful to 5 s), Room B1 (max decay "of the order of 700 ms"), Delay (up to 810 ms, decay = feedback),
  Freeze, Image P1 (a few ms to ≈ 0.6 s).
- **HF decay table** (Appendix C), setting → frequency at which the decay time halves: 0 → 18 kHz (flat),
  -1 → 15 kHz, -2 → 10, -3 → 8, -4 → 6.5, -5 → 5, -6 → 3.5, -7 → 2, -8 → 1.5, -9 → 1 kHz. **LF control** acts
  at **350 Hz**, from ×1/4 of the set decay at -9 to ×4 at +9.

**Others.**
- Yamaha REV7 (SOS review, November 1985, https://www.soundonsound.com/reviews/yamaha-rev-7-digital-reverberator):
  reverb bandwidth **20 Hz-12 kHz**, dynamic range 78 dB (reverb); RT 0.3-9.9 s, HF RT 0.03-9.9 s, LF RT
  0.12-23.7 s; initial delay 0-99.9 ms; 30 presets including gated ones, and inverting one gives a
  "backwards" effect. Converter specs and the exact names and ranges of its gate/reverse presets: **unverified**.
- Roland SRV-2000 "Non-Linear" (Roland's own V-Stage reference,
  https://static.roland.com/manuals/v-stage_reference_v111/en-US/221943691.html): parameters pre-delay,
  reverb time and **gate time** ("time taken to cut off the reverb once it starts sounding"), plus 3-band EQ.
  "16 bit linear A/D/A ... 28 bit parallel arithmetic ... 90 dB" ([snippet], a dealer quoting the manual,
  https://vintageking.com/roland-srv-2000-midi-digital-reverb-588484-vintage); bandwidth 30 Hz-10 kHz and an
  estimated 22 kHz sample rate ([snippet], vintagedigital): **unverified**.
- Lexicon: "Duration" (ms before the output cuts off) in the Inverse and Gated programs of the MPX500 ([snippet]);
  "Inverse Room" on the PCM70 from software 3.0 ([snippet, forum]); 224XL "inverse room" ([user review]). Which
  224/480L version first had them and their ranges: **unverified**.

**DSP recipe (inferred).** A dense diffuse tail (a short FDN or many-tap diffuser with no audible discrete
echoes, density ≥ 1,000 reflections/s by 20 ms) under a programmable envelope: **gated** = flat for T then a
10-40 ms fall; **nonlinear** = shape control from slightly decaying to flat to rising over T (T = 50-800 ms;
default 300 ms), then the fall; **reverse** = rising (exponential or linear ramp) to T, then cut. A second
output of discrete early reflections (NonLin 1's right channel) gives the "multiplied clap". Apply the RMX-style
HF half-decay low-pass and the 350 Hz LF multiplier, and an optional 18 kHz band limit.

### 5.3 Plate and spring

- **EMT 140 plate**: a steel plate ≈ 1 m × 2 m × 0.5 mm suspended by springs ([snippet], Valhalla DSP,
  https://valhalladsp.com/?p=74060); decay set by moving a damper pad toward the plate; maximum ≈ **5 s RT60 at
  500 Hz**, calibrated at 500 Hz; highs decay faster than everything else at every setting; the low end varies
  most with damper position (Soundtoys Little Plate manual pp. 3-4,
  https://www.soundtoys.com/wp-content/uploads/Little-Plate-Manual.pdf). Wikipedia gives 1-5 s
  (https://en.wikipedia.org/wiki/EMT_140); a MusicTech review of a modern 140S gives < 1 s to ≈ 6 s ([snippet]).
  Physical model: Bilbao, DAFx-10, https://ccrma.stanford.edu/~bilbao/dafx10/electro.pdf (not read in full).
- **Spring tanks (Accutronics part numbers)**: type 4 = 17" two-spring, type 8 = 9" three-spring, type 9 = 17"
  three-spring ([snippet, forum decoder]); decay codes "long" 2.75-4.0 s and "medium" 1.75-3.0 s ([snippet],
  retailer tables). Per-spring delay times (often quoted 23-41 ms): **unverified**.
- **Spring physics for DSP** (Parker, "Efficient Dispersion Generation Structures for Spring Reverb
  Emulation", EURASIP J. Adv. Signal Process. 2011,
  https://acris.aalto.fi/ws/portalfiles/portal/13004391/art_10.1155_2011_646134.pdf): the impulse response is two
  series of dispersive echoes ("chirps"); the main series lives below a transition frequency f_C set by the
  spring, where group delay peaks, and "an f_C above 5 kHz is rarely seen", so that structure can run at
  f_s/4; dispersion comes from long cascades of stretched first-order allpasses
  `A(z) = (a + z^-k)/(1 + a z^-k)`; a crossover splits the bands. Välimäki, Parker and Abel, "Parametric Spring
  Reverberation Effect", JAES 58(7/8), 2010 (https://aes2.org/publications/elibrary-page/?id=15511): a
  spectral-delay (allpass chain) filter inside a feedback loop with a long delay line; parameters delay time,
  chirp shape, decay rate (full text not read).
- **Pallor (inferred)**: plate = a modulated FDN with a frequency-dependent decay whose HF loss is faster than
  the LF (as above), decay 0.5-5 s at 500 Hz; spring = two or three parallel lines of Välimäki/Parker type with
  f_C ≈ 3-5 kHz, a 30-40 ms round trip (unverified, tune by ear), a "drip" control that raises the input
  level into a soft clipper before the tank (the splash of a kicked tank).

### 5.4 Early digital reverbs: bandwidth and word length

| Unit | Year | Converters / word | Sample rate | Bandwidth | Sources |
|---|---|---|---|---|---|
| EMT 250 | 1976 (first commercial digital reverb) | 12-bit "quasi 15-bit" [snippet] | 24 kHz [snippet] | ≈ 11 kHz (I/O low-passed) | https://www.mixonline.com/technology/1976-emt-model-250-digital-reverb-377973; https://www.preservationsound.com/the-emt-250-and-244-digital-reverbs/ |
| Lexicon 224 | AES 1978; $7,500-7,900 | 12-bit with gain shifting | **20 kHz** | ≈ 10 kHz (Nyquist; a seller says 8 kHz, unverified) | https://tapeop.com/reviews/gear/85/lexicon-224-digital-reverb-plug-in; https://www.mixonline.com/technology/1978-lexicon-224-digital-reverb-383667 |
| Lexicon 224X / 224XL | ≈ 1984 (XL) | 12-bit gain-ranged | 32 or 34.5 kHz (224X, sources differ), 34.125 kHz (XL) [snippet] | — | Tape Op above; vintagedigital [snippet] |
| AMS RMX16 | March 1982 | gain-ranging 12-bit, 16-bit data, 28-bit internal | ≈ 40 kHz (soft) | **18 kHz** | §5.2 |
| Yamaha REV7 | reviewed Nov 1985 | — | — | **12 kHz** | §5.2 |
| Roland SRV-2000 | 1984/85 | 16-bit linear, 28-bit arithmetic [snippet] | 22 kHz est. [snippet] | 10 kHz [snippet] | §5.2 |
| Lexicon PCM70 | ≈ 1986 | 16-bit [snippet] | — | 15 kHz (wet) [snippet] | dealer listing |

Pallor's "early digital" option for hall, room and plate (inferred): run the tank at a reduced internal rate
(20, 24, 32 or 40 kHz class) with the band limit of the period (10, 12 or 18 kHz), gain-ranged 12-bit
quantisation on the tank input (a 12-bit quantiser preceded by a 0/6/12/18/24 dB gain stage chosen by the
signal level, then the inverse gain: the noise floor steps with level, as in gain-ranging converters), and
the RMX-style HF/LF decay controls.

### 5.5 BBD chorus, ensemble and delay

- **BBD timing** (Holters and Parker, "A Combined Model for a Bucket Brigade Device and its Input and Output
  Filters", DAFx-18, https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf, p. 2): with N stages
  (even) at clock f_clk the delay is **T = N / (2·f_clk)**, with a one-period sample-and-hold; anything above
  f_clk/2 aliases, and the input and output filters do the anti-aliasing and reconstruction. In the Juno-60
  chorus both are 6th order (a 1st-order high-pass plus a **5th-order low-pass**, poles and residues in their
  Table 1, p. 5); the clock is driven by a triangle LFO; the **minimum clock ≈ 26 kHz**, so the circuit itself
  may alias (p. 6). Method: a fixed-length delay running at the variable clock, with the real filters as the
  resampling filters. Calculated: 256 / (2 × 26 kHz) = 4.9 ms maximum.
- **Juno-60 chorus** (measurements by Andy Harman, https://github.com/pendragon-andyh/Juno60/blob/master/Chorus/README.md):
  one triangle LFO drives two 256-stage BBDs, the right channel's modulation inverted. **Chorus I 0.513 Hz,
  Chorus II 0.863 Hz, both 1.66-5.35 ms, stereo; I+II 9.75 Hz, 3.3-3.7 ms, mono "Leslie-like"**. Service notes
  print 0.5 / 0.83 / "1" Hz (Juno-60) and 0.4 / 0.67 / 8.06 Hz (Juno-6). A Faust model by jpcima
  (https://github.com/jpcima/rc-effect-playground/blob/master/sources/chorus.dsp) uses left 1.54-5.15 ms and
  right 1.51-5.40 ms, and a dry gain of 0.83.
- **Roland Dimension D (SDD-320)** (owner's manual, https://archive.org/stream/SDD-320_owners_manual/SDD-320_owners_manual_djvu.txt):
  four modes, 1 softest to 4 strongest, click-free switching; **compander around the delay** (COMP → DELAY →
  EXP); S/N > 95 dB(A). Two synchronised delay lines driven by a trapezoidal LFO ([snippet], Gearspace gear
  page). Per-mode rates and delays: **unverified**.
- **Boss CE-2**: MN3007 (1,024 stages) with an MN3101 clock driver ([snippet], ElectroSmash); MN3007 datasheet
  5.12-51.2 ms over a 10-100 kHz clock ([snippet], consistent with T = N/(2f)); LFO ≈ 0.3-3.5 Hz and an
  anti-alias low-pass near 5 kHz with pre-emphasis from 1 kHz (https://anasounds.com/working-analog-chorus-effect-boss-ce2/).
  CE-1 rates and CE-2 delay range: **unverified**.
- **String ensemble**: three BBD lines on the same input with clocks modulated 120° apart by a slow (< 1 Hz)
  and a faster (several Hz) LFO, delays ≈ 1-5 ms ([snippet, forum],
  https://www.kvraudio.com/forum/viewtopic.php?p=2623291). Exact rates: **unverified**.
- **Pallor (inferred)**: chorus = two BBD lines, triangle LFO 0.5 / 0.86 Hz, 1.66-5.35 ms, right inverted,
  5th-order low-pass around the line, optional companding noise; ensemble = three lines at 120° with a slow
  (0.6 Hz) plus fast (6 Hz) LFO, 1-5 ms (rates unverified, tune by ear); BBD delay = 1,024- or 4,096-stage line
  whose bandwidth falls with delay (f_clk/2 and the filters move with the clock) and whose feedback darkens.

### 5.6 Flanger

- **MXR M117R**: maximum delay 12.8 ms, rate 0.1-10 Hz; controls manual, width, speed, regen (Dunlop spec sheet,
  [snippet]); 12.8 ms = 1,024 / (2 × 40 kHz), consistent with an MN3007-class BBD (calculated). A magazine
  quoted in a user review gives a manual range of ≈ 0.5-16 ms ([snippet]).
- **EHX Electric Mistress** (David Cockerell, from 1976, Reticon SAD1024 [snippet]): its "filter matrix" mode
  stops the LFO and leaves a static comb set by Range; Color = feedback ([snippet], EHX instructions).
- Tape flanging numbers: none gathered (**unverified**).
- **Comb arithmetic (calculated)**: with the wet added to the dry, notches at f = (2k+1)/(2T) and peaks at
  k/T; at T = 0.5 ms the first notch is at 1 kHz, at 10 ms at 50 Hz. Negative feedback moves the peaks to
  the odd multiples. A "jet" is a slow sweep (0.05-0.3 Hz) of T over ≈ 0.5-10 ms with feedback of 60-90 %.
- **Pallor (inferred)**: jet flanger with T from 0.3 to 12 ms, LFO 0.02-10 Hz or tempo-synced (1-16 bars),
  feedback ±95 %, a "through-zero" option (the tape-flange effect, where the delayed copy passes in front of
  the dry), and a "manual" mode (static comb, the filter-matrix sound) that can be swept by an envelope
  follower on the hats.

### 5.7 Phaser

- Typical analog LFO 0.05-5 Hz; the voltage-to-break-frequency mapping is nonlinear and asymmetric (JFETs in
  the Phase 90, lamp and LDRs in the Uni-Vibe); the Small Stone is four first-order allpasses plus a feedback
  loop switched by "Color" (Carson et al., arXiv 2306.01332 §2.3, https://arxiv.org/pdf/2306.01332).
- **MXR Phase 90**: four JFET allpass stages give up to 720°; summed with the dry, **two notches**, where the
  total shift is 180° and 540° ([snippet], PedalPCB forum). Rate range: **unverified**.
- **Uni-Vibe** (DAFx-2019 paper 31, https://ccrma.stanford.edu/~champ/files/DAFx2019_paper_31.pdf): four stages,
  each with its own LDR lit by one lamp; speed ≈ 0-7.6 Hz; stage capacitors **0.015 µF, 0.22 µF, 470 pF,
  0.0047 µF**, so the notches are unevenly spaced; LDR ≈ 6-13 kΩ minimum to 2.6-4.2 MΩ maximum.
- **Mu-Tron Bi-Phase**: two independent **6-stage** phasers, rate 0.1-18 Hz, optical control ([snippet],
  owner's manual).
- **Notches (calculated)**: N first-order allpass stages plus dry give N/2 notches (4 → 2, 6 → 3, 8 → 4).
- **Pallor (inferred)**: 4/6/8 stages, rate 0.02-10 Hz or synced, depth, feedback ("color") 0-90 %, a "vibe"
  option with the Uni-Vibe's unequal stage ratios, stereo with the LFO phase offset 0-180°.

### 5.8 Early digital delays (12-bit era)

- Dates (https://en.wikipedia.org/wiki/Delay_(audio_effect); https://en.wikipedia.org/wiki/Lexicon_(company)):
  Eventide DDL 1745 1971; Lexicon Delta-T 101 1971; AMS DMX 15-80 1978 (Hannett had one "for about two weeks"
  when recording "Digital", https://en.wikipedia.org/wiki/Martin_Hannett); Lexicon Prime Time 1979; Lexicon
  PCM42, Roland SDE-3000 and TC 2290 offered more than 3 s; Boss DD-2 1984, the first digital delay pedal.
- The AMS DMX 15-80 was "the world's first microprocessor-controlled 15-bit digital delay"
  (https://blog.scienceandindustrymuseum.org.uk/factory-records-cutting-edge-technology); the RMX16's delay
  program runs to 810 ms at 18 kHz bandwidth (manual p. 18).
- **Unverified (search budget exhausted)**: PCM42 word length, bandwidth and extended delay; SDE-1000/2000/3000
  bandwidth, word length and maximum delay; Prime Time specifications.
- **Pallor (inferred)**: "12-bit digital delay" = a delay line quantised to 12 bits (optionally gain-ranged)
  at a virtual 32 kHz rate with a 12-16 kHz low-pass, where the bandwidth halves when the delay time doubles
  beyond the unit's "full-bandwidth" limit (the trade-off of the period, unverified per unit); tempo-synced
  times; feedback with a 12-bit quantiser in the loop so the repeats grain up.

### 5.9 Tape echo

- **Roland RE-201 Space Echo** (https://en.wikipedia.org/wiki/Roland_RE-201): 1974-1990; one record head and
  **three playback heads** selected in combinations by a rotary switch; spring reverb; free-running tape loop.
  Head delays quoted from the UAD Galaxy Tape Echo manual in a KVR post ([snippet-grade],
  https://www.kvraudio.com/forum/viewtopic.php?p=6792955): **head 1 69-177 ms, head 2 131-337 ms, head 3
  189-489 ms** over the repeat-rate range; ratio ≈ 1 : 1.9 : 2.75 at both ends (calculated), consistent with
  fixed head spacing and variable motor speed. Wow and flutter figures: **unverified**.
- **Echoplex EP-3** (https://en.wikipedia.org/wiki/Echoplex): solid-state from 1970; delay set by moving the
  record head (EP-2). Range in ms: **unverified**.
- **Pallor (inferred)**: three heads at the ratios above, a motor-speed control covering the quoted ranges,
  wow ≈ 0.5-1 Hz and flutter ≈ 6-10 Hz with depths set by an "age" control, a saturating record stage and a
  head-gap low-pass in the loop, and the spring of §5.3 in parallel. The Jiles-Atherton tape model of Tarnish
  can run in the record path at a reduced oversampling factor if CPU allows.

### 5.10 Bus compressor ("glue")

- SSL G-series centre-section compressor as documented by the Waves SSL G-Master Buss Compressor manual
  (pp. 3-4, https://www.waves.com/1lib/pdf/plugins/ssl-g-master-buss-compressor.pdf): **attack 0.1, 0.3, 1, 3,
  10, 30 ms; release 0.1, 0.3, 0.6, 1.2 s or Auto** ("the release time is dependent upon the duration of the
  program peak"); **ratio 2:1, 4:1, 10:1**; threshold -15 to +15 dB; make-up -5 to +15 dB. The E series (1979)
  introduced the master bus compressor; the G series came in 1987 (https://en.wikipedia.org/wiki/Solid_State_Logic).
- **Unverified**: the VCA topology and the auto-release circuit (commonly described as a dual time constant:
  fast for short peaks, slow for sustained compression); typical "glue" settings (2:1 or 4:1, attack 10-30 ms,
  auto release, 2-4 dB of gain reduction) are an engineering convention, not sourced here; the Attack recipes
  of §1.4 (≈ 4 dB on the kick bus, 3-4 dB on the snare bus) are the nearest sourced numbers.
- **Pallor (inferred)**: feed-forward VCA-style compressor, stepped attack and release as above, auto release
  as two parallel release time constants (≈ 100 ms and ≈ 1.5 s, the slow one charged only by sustained gain
  reduction), ratio 2/4/10, side-chain high-pass 20-200 Hz, mix knob for parallel compression.

### 5.11 Exciter

- **Aphex** patent US 4,150,253 (Knoppel, 1979, https://patents.google.com/patent/US4150253A/en): the signal is
  split; a side path runs **high-pass → harmonic generator → attenuator** and is added back. The solid-state
  embodiment's high-pass is a **4 kHz 2-pole Butterworth (12 dB/oct)**, claims also mention 1 kHz; the harmonic
  generator is a diode soft clipper with a pot-set threshold (one-sided clipping gives odd and even low-order
  harmonics); the side path is mixed at **≈ 20-70 % of the direct amplitude** (claim 3). The commercial units'
  700 Hz-7 kHz tune range: **unverified**.
- **BBE**: "frequency-dependent phase shifting" (https://en.wikipedia.org/wiki/Exciter_(effect)); a likely origin
  patent is US 4,482,866 (Crooks, Barcus-Berry, 1984, https://patents.google.com/patent/US4482866A/en): a
  reference load tuned near 400 Hz, HF correction rising ≈ 4-6 dB/oct from 1 to 10 kHz with leading phase and
  LF correction below 400 Hz with lagging phase. The link to the BBE product is inferred, not confirmed.
- **Pallor (inferred)**: exciter = high-pass (1-8 kHz, 12 dB/oct) → asymmetric soft clipper (2x oversampled) →
  amount 0-70 % → sum; optional "contour" low shelf at ≈ 100 Hz for the BBE-style low end.

### 5.12 Tape and lo-fi on the master

Tape saturation reuses the Jiles-Atherton hysteresis of Tarnish (`plugins/tarnish/Source/dsp/Hysteresis.h`:
normalised J-A, integrated along the path of h so the loop is rate-independent, fixed-step RK4, parameters from
Chowdhury's DAFx-19 tape values; see `plugins/tarnish/docs/research.md` §2.2). "Sampler crunch / lo-fi" on the
master reuses the 12-bit drop-sample chain of §2.3 at 26.04 kHz or a free rate, plus Tarnish's crush with
"clean" and "raw" modes (`plugins/tarnish/Source/dsp/Lofi.h`) (inferred).

---

## 6. What users of current drum machine plugins ask for

Method and limits. Forum threads and reviews on KVR (forums and product user reviews), Elektronauts,
Gearspace, ModWiggler, Vintage Synth Explorer, the Minimal Wave forum and the press (Sound On Sound,
MusicTech, MusicRadar, Attack Magazine, Bonedo), all accessed 2026-10-21. **Reddit could not be reached from
this session** (the fetch tool refuses reddit.com and the search tool cannot index it), so r/edmproduction,
r/synthesizers, r/darkwave and the others are not represented; this is the main gap of §6 and a follow-up
for a session with Reddit access. Gearspace, ModWiggler and Reverb returned HTTP 403 to the fetch; claims
from them come only from the search engine's summary of the page and are tagged **[snippet]**, which counts
as unverified. Tags: **[1u]** a single user, **[theme]** several independent users or sources, **[press]**
a magazine review. "Evidence" counts distinct sources found; it is an indication, not a poll.

### 6.1 Per product

| Product | What users love | What they miss or criticise | Sources |
|---|---|---|---|
| Sonic Charge Microtonic (8-voice drum synth, $99) | Sound that "cuts through" a mix; very low CPU and fast loading [theme]; simple x0x sequencer with accent, fill, chaining, roll and swing [theme]; random button, open preset format, scriptability, MIDI learn [1u]; the Patternarium pattern library [1u]; free updates for life [1u]; the most recommended software stand-in for an Analog Rytm on Elektronauts [theme] | "No polyrhythm" [1u]; 16 steps, one channel's pattern visible at a time, coarse velocity [theme, older reviews]; little per-step editing of level, pan, pitch [1u]; wants filter envelopes, more noise types, velocity to more parameters, adjustable decay curves [1u]; cramped UI, one drum at a time [press]; loading a program also loads its patterns [1u] | https://www.kvraudio.com/product/microtonic-tonic-by-sonic-charge/reviews; https://www.musicradar.com/reviews/tech/sonic-charge-microtonic-3-398862; https://www.elektronauts.com/t/2023-whats-the-closest-software-plugin-equivalent-to-the-analog-rytm/189942 |
| XLN Audio XO | The 2-D sample map for browsing: "worth the money" for the indexing alone [theme] | Sequencer "rudimentary", chaining limited to A/AB/AAAB [theme, four users in one 2025 thread]; only 8 lanes [2u]; no per-lane length or time base [theme]; breaks in odd meters [1u]; per-lane MIDI export "broken" [1u]; multi-out loses the master FX [2u]; small, non-resizable GUI [1u]; slow updates, "dead product" [theme] | https://www.kvraudio.com/forum/viewtopic.php?t=617857; https://www.kvraudio.com/product/xo-by-xln-audio/reviews; https://www.kvraudio.com/forum/viewtopic.php?t=524027&start=615 |
| Algonaut Atlas | AI map of a sample library; random kit generator [1u] | Dead space in the maps, fixed categories, poor on small libraries [1u]; "very basic as a drum VST", no step sequencer at v1.1 [1u] | https://www.kvraudio.com/product/atlas-by-algonaut/reviews |
| Wave Alchemy Triaz | 32 steps with length and rate per track (polymeter); Repeat (ratchets), Chance, start offset, pan/delay/reverb lanes; swing plus "Slop" timing randomiser; 12 patterns per kit [press, SOS 2022]; MIDI drag-out [press, MusicTech]; public roadmap [1u] | Chance capped at 50 % [press, SOS]; no pitch lane [press]; fiddly multi-out in the Kontakt version [press] | https://www.soundonsound.com/reviews/wave-alchemy-triaz; https://musictech.com/reviews/wave-alchemy-triaz-review-drum-machine/ |
| Wave Alchemy Revolution (sampled classic machines, Kontakt) | "Fun and intuitive" sequencer; a Character control (analog, digital, tape, 12-bit sampler); round-robin samples capture hit-to-hit variation; mixing modules across machines [press, Attack 2017] | No undo on the sequencer page; price £149.95 [press]; samples bend less than synthesis [press, snippet] | https://www.attackmagazine.com/reviews/gear-software/wave-alchemy-revolution/ |
| D16 Nepheton 2 (808 model) | Punchy sound with more editing than the original [press, MusicTech 8/10]; all 17 lanes in one grid, 64 steps, accent, flam, swing, randomise [press]; 16 stereo outs, channels can bypass the FX buses [press] | Kick not freely tunable to the song's key [press]; high CPU at full quality with FX [press + 1u]; v2 stopped publishing note names to the DAW, "VERY inconvenient" [2u]; hats lack the hardware's "magic" [theme] | https://musictech.com/reviews/software-instruments/d16-group-nepheton-2-review/; https://www.kvraudio.com/forum/viewtopic.php?t=602869&start=120; https://www.kvraudio.com/forum/viewtopic.php?p=8982680 |
| D16 Drumazon 2 (909 model) | FX buses and filters "phenomenal" [1u]; accurate [theme]; resizable HiDPI UI [press] | **No per-instrument track length** ("the only thing keeping me" away) [1u]; no MIDI import of patterns [1u]; FX routing "questionable" [1u] | https://www.kvraudio.com/forum/viewtopic.php?p=8752478; https://www.kvraudio.com/product/drumazon-2-by-d16-group/reviews; https://synthanatomy.com/2023/09/d16-group-drumazon-2-major-update-for-the-popular-tr-909-emulation.html |
| AudioRealism ADM (808/909/606 models plus sample slots) | The 606 model [theme]; Roland-style sequencer with accent and flam; per-step "pattern controlled FX" [theme] | Clunky sample handling; one user lost samples and routing on project reload [2u]; "needlessly complex" [1u]; no MIDI export [snippet] | https://www.kvraudio.com/forum/viewtopic.php?p=5121241; https://www.kvraudio.com/product/audiorealism-drum-machine-adm-by-audiorealism/reviews |
| Roland Cloud TR-808/909/707 | Close to the hardware [1u]; per-track last step (polymeter); MIDI or audio drag-out; 11 outs [press, Bonedo 2018]; used for writing by Molchat Doma [interview] | "Sterile", lacks grit [press, Bonedo 3.5/5]; 16 steps; no FX [press]; per-plugin price and a periodic online licence check [theme] | https://www.bonedo.de/artikel/roland-cloud-52-tr-808-und-tr-909-test/; https://www.kvraudio.com/forum/viewtopic.php?t=501494&start=15; https://articles.roland.com/a-few-minutes-with-molchat-doma/ |
| Arturia Spark 2 (discontinued) | Intuitive; hybrid modelled and sampled kits; MIDI and audio drag and drop [1u]; a "favourite drum plugin of all time" [snippet] | Abandoned ("no further development"), no Apple Silicon build [theme]; drum selections lost after OS or DAW updates [2u] | https://www.kvraudio.com/forum/viewtopic.php?t=591484; https://www.kvraudio.com/forum/viewtopic.php?t=496700 |
| Native Instruments Battery 4 | Easy, quick routing to separate channels, good saturation [theme] | **No sequencer** [theme]; routing hidden in a menu, in-kit preview lost [theme]; FX not automatable [1u] | https://www.kvraudio.com/forum/viewtopic.php?t=503453&start=45 |
| Ableton Drum Rack / Drum Sampler | Built-in choke groups, "essential" for hats [press, SOS 2025] | Per-pad sends lost in nested groups [press]; no native per-pad loop length, users add Max for Live sequencers [snippet] | https://www.soundonsound.com/techniques/live-12-drum-racks-revisited |
| UVI Drum Designer | Programmable, low CPU [press/aggregator, snippet] | iLok activation friction [theme, snippet]; limited voice set [snippet] | https://musictech.com/reviews/uvi-drum-designer-review/ [snippet] |
| Sitala (free) | One knob per parameter; any pad can choke any other; cycling through a folder from the pad; low CPU [theme, 11 KVR reviews] | One sample per pad, no velocity layers [1u]; custom kits did not recall their samples (2018) [1u] | https://www.kvraudio.com/product/sitala-by-decomposer/reviews |
| Audiomodern Playbeat (generative) | Auto-fill remix "without chaotic changes"; per-channel MIDI/audio drag-out; per-channel swing, timing and step count [press, Attack 2022] | 8 tracks; wants more swing types and onboard FX [press, snippet] | https://www.attackmagazine.com/reviews/gear-software/add-variety-to-your-drums-with-audiomodern-playbeat-3/ |
| Sugar Bytes DrumComputer | Randomiser and sequencer; auto fills; per-lane length and tempo [theme] | Weak, "mushy" kicks [theme]; confusing note mapping [2u] | https://www.kvraudio.com/product/drumcomputer-by-sugar-bytes/reviews |
| FXpansion Geist 2 (discontinued) | "The clear best groovebox" [theme] | Abandoned with no replacement [theme] | https://www.kvraudio.com/forum/viewtopic.php?t=556861 |
| Goldbaby sample packs | Tape-processed vintage machines, about 55 machines including LM-1/LinnDrum and the 505/626/707/727 [press, snippet] | Taste-dependent [snippet] | https://www.goldbaby.co.nz/reviews.html |
| Elektron Analog Rytm / Digitakt (hardware benchmark) | Parameter locks; **trig conditions**: users describe 2:2 tom fills, FILL-gated layers, low-velocity % ghost snares, PRE/NEI stutters, 4-bar patterns that "feel like 16 bars" [theme, 16 users in one thread] | Condition counters do not reset per song position ("can't program a fill into song mode") [1u + 2]; no software equivalent [1u] | https://www.elektronauts.com/t/how-do-you-use-conditional-triggers/42272; https://www.elektronauts.com/t/trig-conditions-and-song-mode/241960; https://www.elektronauts.com/t/is-there-a-vst-drum-sequencer-comparable-to-elektrons/38398 |

### 6.2 Per theme

| Theme | Evidence | Summary | Representative sources |
|---|---|---|---|
| Per-track length / polymeter | ~10 | **The most often missed sequencer feature** (XO, Drumazon 2, Microtonic); praised where it exists (Triaz, Roland Cloud, Playbeat, DrumComputer) | KVR t=617857; p=8752478; SOS Triaz; Bonedo |
| MIDI drag and drop | ~9 | "A near must have"; praised in Playbeat, Triaz, Roland Cloud; broken or missing in XO, ADM; the reverse (MIDI import) also asked for | https://www.kvraudio.com/forum/viewtopic.php?t=496700; Attack Playbeat; KVR p=8752478 |
| Fills and conditions | ~8 | Loved on Elektron hardware, nearly absent in plugins; plugins offer auto fills instead, which users like | Elektronauts 42272; Attack Playbeat; DrumComputer reviews |
| Vintage character vs. too clean | ~8 | "Sterile" is the main criticism of accurate emulations; hardware "magic" in 808 hats; LM-1 voices with free-running clocks vary hit to hit, which static samples miss | Bonedo; https://www.elektronauts.com/t/what-can-a-linn-drum-lm-1-lm-2-do-that-i-cant-do-on-a-digitakt/186151 |
| Multi-out | ~8 | Expected; the complaint is losing the internal FX when going multi-out (XO); a tutorial wants separate outs to put gated reverb on each voice | MusicTech Nepheton 2; KVR t=617857; MusicTech gated-reverb tutorial |
| Randomisation | ~8 | Loved in moderation; criticised when hidden, erratic or "intrusive" | DrumComputer reviews; KVR t=518386 |
| Probability | ~7 | Wanted for variation; Triaz's 50 % cap criticised; Elektron users use it mainly for ghost notes and warn that too much "sounds random" | Elektronauts 38398, 42272; SOS Triaz |
| CPU | ~7 | Low CPU is praised (Microtonic, Sitala); high CPU penalised (Nepheton 2 at full quality, Roland Cloud) | KVR Microtonic reviews; KVR p=8982680 |
| UI size / scaling | ~7 | "GUI too small" recurs for older plugins; resizable UIs welcomed | Synth Anatomy Drumazon 2; KVR XO thread |
| Swing quality | ~6 | Linn's description (below, §4) is the reference; per-track swing praised in Playbeat and Triaz; users add MPC-style swing to DAWs | Attack Linn interview; Attack Playbeat |
| Song mode / chaining | ~6 | Mixed: many prefer arranging in the DAW; song mode and conditions interact badly on Elektron | KVR Microtonic reviews; Elektronauts 241960 |
| Sample import | ~6 | Users of sample-based tools want drag-to-pad; clunky sample handling and lost samples on reload are criticised | Sitala, ADM reviews |
| Choke groups | ~5 | Treated as basic ("essential for realistic hi-hats") | SOS Live 12 Drum Racks; Sitala reviews |
| Velocity / accent | ~5 | More velocity levels, velocity to more parameters; Linn: even velocity on hats makes "a bad drum machine beat" | Microtonic reviews; Attack Linn interview |
| Ratchets | ~4 | Triaz Repeat and flams praised; note repeat requested elsewhere | SOS Triaz; KVR t=518386 |
| Embedded samples / recall | ~3 | Thin evidence (Sitala 2018, ADM, Spark) but each case is a lost project | Sitala, ADM reviews; KVR t=591484 |
| Price, licensing, abandonment | ~10 | Online licence checks and iLok are disliked; **abandonment fear** (Spark 2, Geist 2, XO, Battery) is strong | KVR t=501494, t=556861, t=591484 |

### 6.3 What dark-genre producers use and say

- **Sample-based 80s digital machines are the consensus core** [theme]. In one Elektronauts thread (April
  2024) users name the TR-707 (one user's "desert island" machine), a Yamaha RX11 (credited by a user to Lebanon
  Hanover), Boss DR-660, Korg DDD-1, LinnDrum, Oberheim DX/DMX, Drumtraks, Yamaha RX7, Roland R-8; one user knows of no
  darkwave tracks built on drum *synths*; another says static samples miss a 707's "nuances of the VCA"; a
  third that filters, reverbs and delays define the genre more than the drum source.
  https://www.elektronauts.com/t/best-modern-drum-synth-today-for-darkwave-and-synthwave/211459
- **Minimal wave**: 1980-81 is the era of the Boss DR-55 and Korg KR-55; the TR-606, TR-808 and Korg KPR-77
  arrived around 1982 and "changed the sound"; the Boss DR-110 was common in the UK in 1983 [1u, detailed]
  https://minimalwave.com/forums/viewthread/811/. The DR-55 is "the go-to drum sound of minimal synth" [2u]
  https://forum.vintagesynth.com/viewtopic.php?t=76506.
- **Contemporary post-punk**: Molchat Doma name the Alesis HR-16, E-mu Drumulator and TR-909 plus an Emax II
  sampler (https://www.synthhistory.com/post/interview-with-molchat-doma, July 2024) and a CR-68 for jamming
  (https://articles.roland.com/a-few-minutes-with-molchat-doma/, October 2024).
- **LinnDrum vs. DMX** [snippet]: Linn described as clean and studio-like, DMX as grittier and needing more
  processing. https://gearspace.com/threads/linndrum-or-dmx-what-do-you-prefer.995224/
- **EBM / industrial** [snippet]: Kawai R50/R100, TR-707/606, Oberheim DX, LinnDrum, Korg DDD-1, "the sound is
  in the processing" (https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/941606-gear-ebm-techno-industrial-electro.html);
  the Pearl Syncussion SY-1 credited to Nitzer Ebb and Front 242 [1u] https://www.kvraudio.com/forum/viewtopic.php?t=197724.
- **Processing recipes in the press**: MusicTech (September 2024) builds a vintage gated-drum sound from a
  TR-707 kit: compressor with fast attack → gated reverb on a 100 % wet send → EQ, with separate outputs per
  voice and a shorter reverb and hold on the hats
  (https://musictech.com/tutorials/tips/how-to-create-authentic-vintage-drum-gated-reverb-for-synthwave-chillwave-music-styles/).
  Attack's "New Wave Drums" (July 2015) layers an LM-1 snare with a large tail, puts a **phaser on the hats at
  35 % wet**, crunchy compression, at 110-125 BPM, and moves the tambourine a few ms ahead of the snare
  (https://www.attackmagazine.com/technique/beat-dissected/new-wave-drums/). Attack's "Synthwave Drums" (2015)
  uses a Drumtraks snare and hats with a gated "non-linear" reverb and saturation at 95-115 BPM with no swing
  (https://www.attackmagazine.com/technique/beat-dissected/synthwave-drums/).

### 6.4 What this means for Pallor (inferred)

1. **Polymeter, MIDI drag-out (and import), conditions/fills and multi-out without losing the effects** are
   the four sequencer/routing gaps users name most; Pallor has all four (§7.6), with the effects available
   per bus so a multi-out user keeps them.
2. **The dark genres want 80s *sampled* machines more than another 808/909 synth**, and want the processing
   (gated/nonlinear reverb, phaser/chorus on hats, drive) built in. This is the case for the Digital 80s
   engine and the two sends.
3. **"Too clean" and "static samples" are the main sound complaints.** Pallor answers with the playback chain
   (companding noise that follows the envelope, clock-pitch images, per-voice analog filter and VCA) and
   small per-hit variation (a free-running virtual clock phase per voice, as the LM-1's free-running clocks
   did, plus optional round-robin micro-variation of the synthesised tables).
4. **Abandonment and licensing fears favour an open-source, offline instrument**; the AGPL licence and no
   online activation are a feature, and should be said in the manual.
5. **Keep CPU low and the UI resizable**; publish note names to hosts that read them; never lose a user
   sample (embed it in the state, test T13).

---

## 7. Design summary for Pallor (inferred throughout)

The design below is already decided. This section describes it and ties each choice to the research above;
numbers are starting values to tune with the tests of §8.

### 7.1 Why this shape

- **One instrument for a dozen genres** (§1). The genres share a small palette: a kick that is either a
  pitched resonator with a glide (808 lineage: trip-hop, witch house, early EBM) or a short body with a click
  (909 and sample-machine lineage: EBM, hi-NRG, new beat); a snare that is either a tuned two-mode body plus
  noise or a bright 8-bit sample drowned in gated or nonlinear reverb; metallic hats from square-wave clusters,
  filtered noise or companded samples; toms that are either hexagonal-pad pitch drops or 80s sample toms; and
  the cheap-box percussion (cowbell, claves, maracas, guiro-like scrapes) of the preset rhythm boxes. Two
  engines per voice (Analog, Digital 80s) cover both lineages without loading sixteen machines.
- **The dark genres want the 80s *sampled* machines** (§6.3): LinnDrum, DMX, Drumulator, 707, RX, HR-16,
  DR-55/110. What gives them their sound is mostly the **playback chain**: the companded 8-bit DAC whose noise
  follows the signal (§2.1), the clock-tuning images (§2.3), the per-voice analog filters that open on the
  transient (LinnDrum bass and toms, §2.2), the multiplying-DAC VCA, and the free-running clocks that make every
  hit slightly different (LM-1, §2.2). These are generic techniques, so Pallor reproduces the chain and plays
  its own synthesised tables through it.
- **Original, not a clone**: no factory ROM samples of any machine, no trade dress, no machine names in the
  product (conventions at the top). Built-in kits are generated by Pallor's own synthesis at initialisation.
- **The effects are part of the sound** (§1.4, §5). On record these drums are the machine plus gated or
  nonlinear reverb, plate or spring on the snare, chorus/flanger/phaser on hats and cymbals, a 12-bit delay or
  tape echo, bus compression and tape. Users of current plugins move each voice to its own channel to add
  these, and complain when multi-out loses the internal effects (§6.2). Pallor keeps both: built-in sends with
  period programs, and multi-out buses that carry their own send returns.
- **The sequencer gaps users name most** (§6.2) are polymeter, MIDI drag-out and import, conditions/fills and
  multi-out; Pallor has all four, plus probability, ratchets, flams and Linn-style swing (§4).
- **Open source and offline** answers the abandonment and licensing complaints (§6.2): AGPL, no activation.

### 7.2 Voices (16) and engines

| # | Voice | Analog engine models (section) | Digital 80s default table (synthesised at init) | GM note (map A) | Pad note (map B) |
|---|---|---|---|---|---|
| 1 | Kick | bridged-T with glide and trigger overshoot (§3.1); 909-style VCO + waveshaper + pitch envelope + click (§3.7); cheap-box ringing kick, no glide (§3.9); modern dark-wave kick (sine, long exponential glide, drive, sub tail; inferred from the witch-house and trip-hop practice of §1.1); hexagonal kick (oscillator + bend + click + noise, §3.8) | modal membrane (Bessel-mode ratios) with beater click | 36 (35) | 36 |
| 2 | Snare | two bridged-T resonators 238/476 Hz + snappy noise through a 2.75 kHz high-pass (§3.2); 909-style two-VCO snare (§3.7); hexagonal snare (§3.8); noise snare (filtered burst, §3.9) | membrane + snare-wire noise | 38 (40) | 37 |
| 3 | Clap | noise → ≈ 1 kHz band-pass, 3 bursts ≈ 10 ms apart in a 30 ms window, tail VCA (§3.5) | layered synthetic hand impulses, jittered | 39 | 38 |
| 4 | Rim | resonators 455 + 1,667 Hz with a 10 ms VCA (§3.6) | stick-on-rim modal hit | 37 | 39 |
| 5 | Closed hat | six squares (205.3-800 Hz) → band-passes 3.44/7.1 kHz → VCA → high-pass (§3.3); noise hat (§3.9); CR-78 metal (three squares 4.08-6.17 kHz → 5.6 kHz tank, §3.9) | synthetic metal plate, short | 42 (44) | 40 |
| 6 | Open hat | as 5, long; **choke group 1 with 5** (the 808 chokes OH from CH, §3.3; users treat choke as basic, §6.2) | as 5, long | 46 | 41 |
| 7 | Crash | six-square cluster with the cymbal envelope (CY 350-1,200 ms) and band routing (§3.3); noise crash | modal plate (crash) | 49 (57) | 42 |
| 8 | Ride | six-square cluster with a "bell" pair emphasised (inferred) | modal plate with bell | 51 (59, 53) | 43 |
| 9-12 | Toms 1-4 | hexagonal toms: steep bend + noise + click (§3.8); 808-style bridged-T toms/congas with amplitude-dependent pitch (§3.6) | membranes tuned as a set | 41, 45, 48, 50 (43, 47) | 44-47 |
| 13 | Cowbell | 540 + 800 Hz squares → band-pass ≈ 2.6 kHz → two-slope envelope (§3.4) | bar-plus-shell modal | 56 | 48 |
| 14 | Clave | bridged-T 2.5 kHz, 25 ms (§3.6); woodblock | wooden bar modal | 75 (76, 77) | 49 |
| 15 | Tambourine / shaker | maracas (noise → 10.6 kHz high-pass, 25-35 ms, §3.5); tambourine (jingle cluster); guiro-like chopped noise (§3.9) | jingles + shaker | 54 (69, 70) | 50 |
| 16 | Noise / FX | zaps (fast exponential pitch sweep, Syndrum/Synare style, §2.7), noise sweeps, metal hits, reverse swells | FM metal, noise riser | user-assigned (none by default) | 51 |

Map A is the General MIDI Level 1 percussion map, notes in brackets also accepted
(https://en.wikipedia.org/wiki/General_MIDI, percussion table: 35/36 bass drum, 37 side stick, 38/40 snare,
39 clap, 41/43/45/47/48/50 toms, 42 closed hat, 44 pedal hat, 46 open hat, 49/57 crash, 51/59 ride, 53 ride bell,
54 tambourine, 56 cowbell, 69 cabasa, 70 maracas, 75 claves, 76/77 woodblocks). Map B is chromatic from 36 (C1
in the C3 = 60 convention) to 51 for pad controllers and piano rolls. Pallor publishes note names to hosts that
read them (the Nepheton 2 complaint of §6.1).

**Built-in tables (Digital 80s engine).** Synthesised at initialisation, deterministically, from fixed seeds:
modal membranes (circular-membrane mode ratios), bars (free-free bar ratios) and plates (dense inharmonic modes
with frequency-dependent damping), FM metal, shaped noise, a beater or stick contact model for the transient.
They are rendered at 44.1 or 48 kHz into float, then each mode resamples and quantises them into its virtual
machine format. Several variants per voice (round-robin of micro-variations) answer the "static samples"
complaint (§6.2).

**User samples.** WAV/AIFF/FLAC per voice, any rate and depth, stereo summed or kept; stored **inside the plugin
state** (compressed, lossless) so a project never loses them (§6.2; test T13); the original file path is kept
for reference only. A user sample goes through the same Digital 80s chain as the tables.

### 7.3 The Digital 80s chain

1. **Table** at its source rate.
2. **Word length**: 8-bit **μ-255 companded** with the DAC-76/AM6070 law of §2.1 (chord base levels 33·(2^c − 1),
   step 2^(c+1), full scale 8031; 255 levels); 12-bit linear (sampler modes); float (clean). Encoding into the
   8-bit code uses nearest-level search, with optional TPDF dither off by default (the hardware had none).
3. **Playback clock**: pitch by changing the virtual clock, one sample per tick, **no interpolation**; length
   changes with pitch, as in the hardware (§2.3). Each voice's clock has a free-running phase, randomised per
   hit within one clock period (the LM-1 effect, §2.2), with a seed for determinism (test T3).
4. **Zero-order hold rendered band-limited**: each held step is placed on the host grid as a band-limited step at
   its exact fractional time (windowed-sinc minBLEP residual, 16-32 taps; the repo's 2-sample polyBLEP in
   `shared/dsp/Blep.h` is not enough at a 26-30 kHz virtual clock on a 44.1 kHz host). Images below the host
   Nyquist are kept, because the hardware had them; nothing folds back from above it (test T20).
5. **Per-voice analog filter**: modes as in §2.2/§2.4: fixed 6-pole low-pass at ≈ 10 or ≈ 16 kHz (DMX-like); a
   4-pole low-pass that sits low (≈ 650 Hz-1.4 kHz) and **opens to full bandwidth on each trigger** then closes
   over tens of ms (LinnDrum bass and toms); a one-pole ≈ 15.9 kHz output filter; a clock-tracking 6th-order
   low-pass at f_clk/2 (S900-like); none (raw DAC).
6. **VCA**: the decay envelope multiplies **after** the DAC, as the multiplying-DAC VCAs did (§2.1), so the
   quantisation noise scales with the envelope.
7. **Sampler modes**: 12-bit linear at **26.04 kHz** with drop-sample pitch at the fixed rate and a choice of the
   four SP-style output filters (dynamic 4-pole that closes after ≈ 200 ms, fixed low, fixed high, none, §2.4);
   12-bit at **40 kHz** drop-sample (MPC60-like, §2.4).
8. **Clean mode**: float, interpolated, no colouring.

### 7.4 Per-voice controls

Tune (±24 semitones; in the Digital 80s engine it moves the clock), decay, tone, **two character controls**
(engine-specific: glide amount and attack overshoot on the bridged-T kick; resonator balance and snappy on the
snare; burst count and spread on the clap; metal/noise balance and band balance on the hats; bend depth and
click on the hexagonal voices; machine filter mode and clock jitter on the Digital 80s engine), level, pan,
accent amount, velocity sensitivity, drive, bit/rate crush, filter (low-pass/high-pass/band-pass with
resonance), choke group (off, 1-4; the choke is a 2-5 ms raised-cosine fade, never a hard cut), send A, send B,
output bus.

### 7.5 Effects

- **Send A "Space"**: gated (dense room, optional hard pre-compression, gate keyed by the dry sum of the voices
  that send, hold 100-600 ms, release 5-50 ms, §5.1); nonlinear (flat/rising/falling envelope over 50-800 ms, then
  a 10-40 ms fall, plus a discrete-reflection output for the "multiplied clap", RMX-style HF/LF decay controls,
  §5.2); reverse (rising then cut, §5.2); plate (0.5-5 s at 500 Hz, HF decaying faster, §5.3); spring
  (dispersive chirps, f_C 3-5 kHz, "drip", §5.3); hall and room with an "early digital" option (reduced rate,
  10/12/18 kHz bandwidth, gain-ranged 12-bit, §5.4).
- **Send B "Echo"**: 12-bit digital delay (quantised loop, bandwidth that falls with time, §5.8); tape echo
  (three heads at ≈ 1 : 1.9 : 2.75, motor speed, wow/flutter, saturating record, §5.9); BBD delay (bandwidth and
  noise that follow the clock, T = N/(2·f_clk), §5.5); chorus (two BBD lines, 0.513/0.863 Hz triangle,
  1.66-5.35 ms, right inverted, §5.5); ensemble (three lines at 120°, slow + fast LFO, §5.5); jet flanger
  (0.3-12 ms, feedback ±95 %, through-zero option, static "manual" comb, §5.6); phaser (4/6/8 stages, "vibe"
  option with unequal stages, §5.7).
- **Master**: tape saturation with the Jiles-Atherton hysteresis of Tarnish (`plugins/tarnish/Source/dsp/Hysteresis.h`,
  moved to a shared header) plus a varispeed option for the new-beat "45 at 33 + 8 %" trick (§1.1); exciter
  (high-pass 1-8 kHz → asymmetric soft clipper → 0-70 %, §5.11); bus compressor "glue" (attack 0.1-30 ms and
  release 0.1-1.2 s/auto in SSL-style steps, ratio 2/4/10, side-chain high-pass, mix, §5.10); sampler crunch /
  lo-fi (the 12-bit drop-sample chain on the whole mix, §2.3, §5.12); output (level, soft clip, meter).
- **Multi-out buses**: main + kick, snare/clap, hats/cymbals, toms, perc, fx. Each bus has its own send A/B
  amounts and can take the master chain or bypass it (the XO complaint, §6.1).

### 7.6 Sequencer

| Feature | Design | Research |
|---|---|---|
| Steps | up to 64 per pattern; per-track **length** (polymeter) and per-track step resolution (1/8T-1/32) | §6.2 (most missed feature) |
| Velocity and accent | velocity per step (0-127) plus a global accent lane, Roland-style | §6.1 (accent praised), Linn on hat dynamics (§4.1) |
| Probability | per step 0-100 % (not capped at 50 % as Triaz), counter-based seeded RNG per track and step | §6.1, §6.2 |
| Ratchets | 2, 3, 4, 6, 8 per step with a velocity ramp | §6.2; witch-house rolls (§1.2) |
| Flam | grace note 5-40 ms before the main, velocity ratio | ADM, Nepheton accent/flam (§6.1) |
| Micro-timing | nudge per step ± half a step, per-track shift | "clap a few ms early", "tambourine ahead" (§1.2), MPC shift (§4.1) |
| Swing | 50-75 %, 8th or 16th, Linn's definition, optional tick rounding (48/96 PPQ), per-track override | §4 |
| Conditions | FILL, !FILL, a:b (1:2, 2:2, 1:4 ... 8:8), FIRST, !FIRST, PRE, !PRE; counters reset at pattern start and on song position (the Elektron song-mode complaint, §6.1) | §6.1, §6.2 |
| Patterns | groups of variations A, B and Fill; **auto fill every N bars** (2, 4, 8, 16) switches the last bar to Fill | auto fills liked (§6.2) |
| Song | chain of pattern references with repeats; host arrangement remains the main tool for those who prefer it | §6.2 (split opinion) |
| Sync | host transport sample-accurate (tests T1, T2); internal clock when the host is stopped (optional) | — |
| MIDI in | GM map and chromatic 36-51 map; velocity; MIDI-triggered and sequenced notes choke each other | §7.2 |
| MIDI out | the sequence as notes (map selectable) to drive other instruments or record into the DAW | §6.2 |
| `.mid` export | drag-and-drop of the pattern or song (type 0 and type 1, 960 PPQ), and `.mid` import into a pattern | §6.2 (drag-out "near must have"; import missing in Drumazon 2) |

---

## 8. Test plan (automated, JUCE-free)

All tests run on the engine without JUCE, like the other instruments of the series, through a command-line
renderer (`pallorrender`, proposal) and a unit-test binary. Tolerances are starting values (inferred); a test
that fails on a published number first checks the number, then the code. "Reference" means the number and
source in §3 or §2, quoted in the row.

| # | Test | Method and pass criterion |
|---|---|---|
| T1 | Sample-accurate step timing across buffer sizes | Internal clock at 97, 120 and 174 BPM, 44.1/48/96 kHz, a pattern with all 16 tracks on every 16th. Render with blocks of 1, 7, 64, 511, 512, 4096 and seeded random sizes (1-4096). Every trigger's onset sample (detected from a click-only test voice, i.e. a single-sample impulse) equals the analytic position `round(t·fs)` of the step, ±0 samples, for every block size. Steps falling exactly on a block boundary fire once (never zero, never twice) |
| T2 | Host-sync rounding gaps | Feed a fake host whose ppq at block start is rounded to 1e-6 beats and occasionally jumps by ±1 sample's worth (the rounding gap between blocks some hosts show); loop wraps at bar 4→1; tempo change mid-bar; transport start at a non-integer ppq. No step is lost or doubled across 10,000 blocks; a step whose ppq lies in the gap between the end of block n and the start of block n+1 is fired at sample 0 of block n+1; loop wrap fires step 1 exactly once |
| T3 | Determinism fingerprint | A fixed 16-bar song (all features: probability, conditions, ratchets, flams, nudge, swing, auto fill, all effects) rendered twice, after `reset()`, and with the block sizes of T1: bit-identical output (SHA-256 of the float buffers); the hash is stored as a golden value per platform and the macOS/Windows/Linux hashes of the *event list* (not the audio) are identical |
| T4 | Swing math | For swing s ∈ {50, 54, 58, 62, 66, 71, 75} % at 16th and 8th resolution: the delayed note of each pair starts at `s·2·d` after the pair start, where `d` is the straight step length (16th: d = 60/(4·BPM) s), ±1 sample; 50 % = straight; 66 % within 1 sample of the triplet position (66.67 % exact for the "triplet" value); the first note of every pair is never moved; 8th swing moves only the 2nd 8th (16th positions 3, 7, 11, 15 counting from 1); per-track swing overrides behave the same |
| T5 | Choke groups | Open hat ringing (decay set to 2 s), closed hat triggered 100 ms later: open-hat energy falls by ≥ 60 dB within the choke fade (proposal 3 ms ± 0.5 ms, no click: the fade is a raised cosine, max sample-to-sample jump below that of the dry signal); a voice in another group is unaffected; choking applies across ratchets and flams; MIDI-triggered and sequencer-triggered voices choke each other identically |
| T6 | Ratchets and flams | Ratchet n ∈ {2, 3, 4, 6, 8} on a step: n onsets evenly spaced over the step (or the set ratchet length), ±1 sample, velocity ramp as set; ratchets never cross into the next step; flam: grace note at the set offset (5-40 ms, proposal) before the main note with the set velocity ratio, and the main note stays on the grid |
| T7 | Probability determinism | Seeded per pattern: the same seed gives the same hit/miss sequence on every run, block size and platform; over 10,000 bars a 25/50/75 % step fires within the binomial 99.9 % interval; changing one step's probability does not change the random draws of other steps (one stream per track and step, counter-based RNG such as a hash of seed/track/step/loop count) |
| T8 | Conditional steps and fills | `1:2` fires on loops 1, 3, 5...; `2:4` on 2, 6, 10...; `FILL` only while fill is held or during auto fill; `!FILL` the opposite; `FIRST` only on the first loop after start; `PRE`/`!PRE` follow the previous condition result of the same track; auto fill every N bars switches the last bar to variation Fill and back exactly at bar boundaries |
| T9 | Polymeter | Tracks of length 16, 13, 7 and 64 in one pattern: after LCM(16, 13, 7) = 1456 steps every track returns to its step 1 on the same sample; per-track step length (1/16, 1/8T, 1/32) lands on the right samples |
| T10 | MIDI in/out balance | Every note-on sent to MIDI out has a matching note-off (no stuck notes) across: stop mid-step, loop wrap, pattern change, tempo change, bypass, `reset()`, ratchets, flams, choke, 10,000 random transport events; note-on and note-off sample offsets lie inside the block; MIDI input on the GM map and on the chromatic 36-51 map triggers the right voice with the right velocity, sample-accurately |
| T11 | `.mid` export | Export of every factory pattern → re-import with a reference parser (a small SMF reader in the test, not JUCE): same notes, same ticks at 960 ppq (swing, nudge and flam positions rounded to the nearest tick and documented), velocities equal, note lengths as documented; GM map and chromatic map variants; a type-0 and a type-1 export both parse |
| T12 | Multi-out routing | With each voice assigned to bus k, a single trigger produces signal only on main (if "also to main" is on) and bus k; nulls on every other bus at -144 dBFS; sends A/B follow the bus or the main as documented; the sum of all buses with "main only" equals main within 1e-6 |
| T13 | User sample load and embed round trip | Load WAV/AIFF/FLAC at 16/24/32-float, mono and stereo, 22.05-192 kHz; save state, clear, restore: the embedded sample data is bit-identical; a state saved on one platform loads on another; a 10 MB sample is refused or downsampled as documented; a missing file path falls back to the embedded copy; corrupted chunks fail safely (voice falls back to the built-in sound, no crash) |
| T14 | 808-style cowbell partials | Analog cowbell, default tune: the two strongest partials in the first 50 ms at 540 Hz and 800 Hz ± 1 % (§3.4), ratio 1.48; the envelope shows the two-segment decay (an abrupt drop then a tail) with the overall time to -20 dB at 50 ms ± 15 % |
| T15 | Metallic hat/cymbal oscillator set | Analog "six-square" hats with the bandpasses bypassed (test hook): FFT peaks at 205.3, 304.4, 369.6, 522.7, 540 and 800 Hz ± 0.5 % (§3.3); with the bandpasses on, the spectral centroid lies between the two bandpass centres of §3.3; closed/open/cymbal decay times (time to -20 dB, the service-notes definition) within ± 15 % of §3.3 (CH 50 ms, OH 90/450/600 ms, CY 350/800/1,200 ms) |
| T16 | Kick fundamental, glide and decay | Bridged-T kick, default tune: settled frequency (zero crossings from 300 to 600 ms at long decay) equal to 49.5 Hz ± 1 % (§3.1); initial frequency ≈ 57 Hz ± 3 % in the first 50 ms and the glide settled by 0.3 s; decay (time to -20 dB) covering 50 to 800 ms over the knob's travel, 300 ms ± 15 % at mid; the attack overshoot present: the first 4 ms contain a half cycle at ≈ 2× the resonance (≈ 100-130 Hz) (§3.1). 909-style kick: start/end frequency and sweep time as chosen in §3.7/§3.10 ± 10 % (the hardware numbers are unverified) |
| T17 | Clap burst spacing | 808-style clap: 4 envelope peaks (3 bursts ≈ 10 ms apart within 30 ms, then the tail) ± 1 ms; band-pass centre (spectral peak) 1 kHz ± 10 %; overall time to -20 dB 100 ms ± 15 % (§3.5) |
| T18 | Snare resonators | Analog bridged-T snare with the noise path muted: two partials at 238 and 476 Hz ± 1 %; overall time to -20 dB 60 ms ± 15 %; snappy high-pass corner 2.75 kHz ± 10 % (§3.2) |
| T19 | μ-law and 12-bit quantisation | Digital-80s chain, clean pitch: a full-scale −0.1 dBFS 1 kHz sine table through the 8-bit μ-255 companded DAC: the set of distinct output levels equals the 255 (sign × 8 chords × 16 steps, with the duplicate zero) of the published law (§2.1), the step size doubles from chord to chord, and SNR vs. level follows the companded curve (39.5 dB at 0 dBFS, 37.1 at -20, 34.7 at -40, 20.4 at -60 for a 997 Hz sine, ± 0.5 dB; §2.1, calculated); 12-bit sampler modes: exactly 4096 levels; quantisation error spectrum white within ± 3 dB |
| T20 | Clock-pitch imaging and host-rate aliasing | A 2 kHz sine table played at its native clock (e.g. 26.04 kHz) and pitched ±12 semitones: the images at k·f_clk ± f appear at the frequencies predicted by the zero-order hold with their sinc-weighted levels (±1 dB) **before** the reconstruction filter; with the band-limited step synthesis no component appears at frequencies that are not k·f_clk ± f (aliasing by the host grid) above −100 dBFS at 44.1 and 48 kHz; with the reconstruction filter on, the images follow its response (±1 dB) |
| T21 | Gated reverb gate time | Gated program, gate time g ∈ {150, 250, 400} ms: after a snare hit the wet output stays within 3 dB of its plateau until g, then falls by ≥ 40 dB within the set release (proposal 10-30 ms); the gate opens within 1 ms of the dry onset (key = dry); nonlinear program: envelope flat or rising (set shape) until the set time, then cut |
| T22 | No denormals, NaN or DC | Silence ≥ 60 s after a full kit hit; every preset; 200 random extreme settings re-randomised every block (seeded); sample rates 22.05-192 kHz: no NaN/Inf (bit-pattern check, since `-ffast-math` can remove `std::isfinite`), peak ≤ +24 dBFS, DC on every output < −90 dBFS after 1 s of silence, CPU in silence within 10 % of CPU with signal |
| T23 | Sample-rate independence | 44.1/48/96/192 kHz: analog voice frequencies within 0.5 %, decay times within 2 %, step onsets at the same musical position (±1 sample); the digital chain's clock-based pitch identical (it runs on its own virtual clock) |
| T24 | State and presets | Save/restore round trip bit-identical (patterns, kits, songs, embedded samples, seeds); every factory kit and pattern renders without NaN and within ± 1.5 dB of its leveled target (the `level_presets.py` step of the other instruments) |
| T25 | CPU budget | `pallorrender --bench`, 48 kHz, 512-sample blocks, 16 voices on 16ths at 140 BPM, both sends and the master active: ≤ 3 % of one core on the Apple M4 and ≤ 6 % on the Ryzen 9 5900XT (proposal, inferred); idle (transport stopped, tails done) ≤ 0.2 %; numbers recorded with the date in the README |

---

## 9. Open questions and follow-up research

The search budget ran out before these could be settled. Each one names the source that would settle it.

| # | Question | Why it matters | Where to look |
|---|---|---|---|
| Q1 | 808 cowbell band-pass centre and Q; R82/C34/C9 values and the two envelope time constants | T14 tolerances; the cowbell is a signature of italo and EBM | [W-CB] (AES e-library); a complete 808 voicing-board schematic scan |
| Q2 | 808 hat/cymbal high-pass corners other than HPF 3 | metal voice brightness | [W-CY] §9-11 equations with the voicing-board values |
| Q3 | 909 kick: VCO start/end frequency, pitch-envelope time constant, click pulse width and noise filter | 909-style kick model (§3.7) | TR-909 service notes at higher resolution (scope page p.9, schematic); measurements of a hardware unit |
| Q4 | 909 snare VCO frequencies and the 20 ms bend depth | 909-style snare | same |
| Q5 | Simmons SDS-V voice circuit: VCO range, bend depth and time constant, click network, noise filter | hexagonal voices are currently tuned by ear | SDS-V service manual (Simmons Museum, behind a captcha), DIY clone documentation |
| Q6 | AM6070 datasheet itself (only quoted secondhand) | the companding law is implemented from the DAC-76 and MAME | AMD 1980s data books on bitsavers |
| Q7 | Drumulator DAC and rate (26.04 kHz implied; companded or linear?) | "8-bit 26 kHz" mode naming | Drumulator service manual; E-mu ROM documentation |
| Q8 | MPC60 "12-bit non-linear" coding law | 40 kHz sampler mode is linear for now | MPC60 service manual; L4003 documentation |
| Q9 | SP-1200 fixed filter cutoffs (channels 3-6) and the dynamic filter's envelope | sampler-mode filters | SP-1200 service manual ("Dynamic Filter Trim"), Rossum's reissue documentation |
| Q10 | TR-707 per-voice sample rates (25 / 12.5 kHz derived) | not needed unless a "707-like" mode is added | TR-707 service notes |
| Q11 | Linn's "60 %" and "70 %" at 48 PPQ; LinnDrum shuffle steps; MPC60 rounding | the "machine ticks" swing option | LinnDrum operator's manual; MPC60 service manual |
| Q12 | Lexicon PCM42, Roland SDE-1000/3000, Prime Time: word length, bandwidth, maximum delay | the 12-bit delay's bandwidth-vs-time law | the manuals (muzines.co.uk reviews; Lexicon and Roland archives) |
| Q13 | RE-201 wow/flutter, tape speed; Echoplex EP-3 delay range | tape echo defaults | RE-201 service notes |
| Q14 | Dimension D per-mode rates and delays; CE-1/CE-2 delay ranges; ensemble LFO rates | chorus/ensemble presets | SDD-320 service notes; Boss service notes |
| Q15 | Lexicon 224/480L nonlinear/inverse programs: first version and parameter ranges | nonlinear defaults | Lexicon manuals |
| Q16 | Spring tank delay times per spring; "short" decay code | spring model | Accutronics data sheets |
| Q17 | An authoritative BPM range for coldwave, post-punk, goth, 90s darkwave | factory pattern tempos | academic genre studies (van Elferen; Reynolds, *Rip It Up and Start Again*) |
| Q18 | Reddit sweep (r/darkwave, r/postpunk, r/synthesizers, r/edmproduction) | §6 lacks Reddit voices | a session with Reddit access |
| Q19 | Machines on Visage "Fade to Grey", Clan of Xymox (1985), Cabaret Voltaire "Nag Nag Nag", Nitzer Ebb *That Total Age*, Crystal Castles; Soft Cell "Tainted Love" (CR-78 vs 808) | §1.3 completeness (marketing copy must not name them anyway) | liner notes, period interviews (muzines.co.uk) |
