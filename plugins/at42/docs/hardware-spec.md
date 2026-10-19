# Elta Music SOLAR 42 family: hardware reference spec for the AT42 software recreation

AT42 is an independent, open-source recreation. It is not affiliated with Elta Music. The plugin
must not use the "SOLAR" wordmark, the Elta Music logo, the moon/sun graphics, the "AMBIENT MACHINE"
lettering or the Cyrillic "СОЛАР" badge. This document only describes the instrument so it can be
modelled.

Research date: 2026-10-16. Anything not stated by a primary source is marked **UNCONFIRMED** and
comes with a best estimate.

## Source keys

| Key | Source | Notes |
|---|---|---|
| [M42] | SOLAR 42 user manual, file `solar42_instruct_01_24` (Jan 2024), 28 pp. https://www.eltamusic.com/_files/ugd/12d408_d4c9881033524945ae276dc3559589ff.pdf | Primary. Local copy (not for redistribution): `ref/solar42_manual.pdf` |
| [M42F] | SOLAR 42F user manual, `solar42f_instruct_03_25_v9` (Mar 2025). https://www.eltamusic.com/_files/ugd/64d4dc_eaf864218c9141189e277fb80dbcd476.pdf | Primary. `ref/solar42f_manual.pdf` |
| [M42N] | SOLAR 42N user manual, `solar42f_instruct_08_04_v14` (Oct 2025). https://www.eltamusic.com/_files/ugd/12d408_5492a3f213ac47ca90e614c44c40f2e6.pdf | Primary. `ref/solar42n_manual.pdf` |
| [R42] | SOLAR 42 panel render, cream (Affinity Designer vector, 14 Sep 2023). https://www.eltamusic.com/_files/ugd/12d408_c41009e41fa84e6c92aca8f330684d60.pdf (identical file: `.../12d408_7ebf910cdc2e458e916ad1bb4899c032.pdf`); black: `.../12d408_e4f30746058c458ab6bacb97f7f642a7.pdf` | **1:1 scale**: page is 1403.28 x 907.2 pt = 495.0 x 320.0 mm, exactly the panel size. All coordinates in section 2 were extracted from its vector paths (PyMuPDF) and text bounding boxes (`pdftotext -bbox`). Not downloaded into the repo (copyrighted artwork) |
| [R42F] | SOLAR 42F panel render ("Solar42_panel_13_Silk_06_KEYBOARD", 26 Mar 2024). https://www.eltamusic.com/_files/ugd/12d408_6a256838294a404c984a807b84843b3e.pdf | Same 1:1 scale |
| [R42N] | SOLAR 42N panel render ("Solar42_panel_42n_04", 24 Jul 2025). https://www.eltamusic.com/_files/ugd/12d408_033e08b2b1a84db19e36f17aea1b8edb.pdf | Same 1:1 scale |
| [EP42] | Elta product page SOLAR 42. https://www.eltamusic.com/solar-42 | Specs, colours, box contents |
| [EP42N] | Elta product page SOLAR 42N (URL still `solar-42f`). https://www.eltamusic.com/solar-42f | Changelogs MK2 (F) and MK3 (N), manual/render links |
| [OCH] | OCHRE cartridge card (PDF, image only). https://www.eltamusic.com/_files/ugd/12d408_e85c7dd812c84f7296ccad3570dd95ab.pdf | `ref/solar42_ochre_card.pdf` |
| [PC] | Perfect Circuit, "Elta Music Solar 42N Review", read via Wayback snapshot 2026-07-12. https://www.perfectcircuit.com/signal/elta-solar-42n-review | Review |
| [ET] | Elektronauts thread "Solar 42 Ambient Machine", Oct 2023 to Oct 2026, pages 1-14. https://www.elektronauts.com/t/solar-42-ambient-machine/201283 | Owner feedback |
| [SA] | Synth Anatomy, Superbooth 23 first look (5 Sep 2023). https://synthanatomy.com/2023/09/superbooth-23-elta-music-solar-42-a-microtonal-polyphonic-ambient-machine-first-look.html | |
| [GN] | Gearnews, "Elta Music Solar 42 ... open for preorders" (Jul 2023). https://www.gearnews.com/?p=153012 | |
| [ST] | Synthtopia (7 Aug 2023). https://www.synthtopia.com/content/2023/08/07/elta-music-solar-42-microtonal-drone-ambient-machine-now-available-to-pre-order/ | |
| [SS23] / [SS24] | Sonicstate, Superbooth 2023 (https://sonicstate.com/news/2023/05/13/superbooth-2023-elta-music-solar-42/) and 42F pre-order news (https://sonicstate.com/news/2024/07/31/elta-music-solar-42f) | |
| [RA] | Resident Advisor news. https://ra.co/news/79473 | |
| [JU] | Juno Daily, "Elta Music Solar 42F review" (21 Mar 2025). https://www.juno.co.uk/junodaily/2025/03/21/elta-music-solar-42f-review/ | Site returned 403; only quoted through search-engine snippets |
| [GS] | Gearspace thread "ELTA Music Solar 42". https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1414217-elta-music-solar-42-a-3.html | 403; search-snippet only |
| [MW] | ModWiggler thread. https://www.modwiggler.com/forum/viewtopic.php?t=274086&start=25 | 403; not read |
| [SIG] | Signal Sounds 42F listing (changelog incl. "toggle switches replaced by buttons", "firmware v3"). https://www.signalsounds.com/elta-music-solar-42f-multi-voice-drone-instrument/ | Retailer |
| [VCV] | VCV Rack community, "ELTA Solar 42 Emulator v2 and challenge" (Dave Benham). https://community.vcvrack.com/t/elta-solar-42-emulator-v2-and-challenge/21906 | Prior art |
| [PC50] / [SA50] | Perfect Circuit "Elta Solar 50" (https://www.perfectcircuit.com/signal/elta-solar-50); Synth Anatomy 2019 teaser (https://synthanatomy.com/2019/11/elta-music-solar-50-patchable-touch-analog-synth-with-50-oscillators.html) | Predecessor |
| [MX] | Matrixsynth posts: https://www.matrixsynth.com/2024/07/off-to-space-bye-elta-music-solar-42f.html, https://www.matrixsynth.com/2024/05/a-box-of-ambient-solar-42.html | Demos |

---

## 1. Overview

### 1.1 What it is

"SOLAR 42 is a unique experimental electronic music instrument, analogue microtonal ambient drone
machine and semi-modular stand-alone synthesizer" [M42 p.2]. It is the successor of the SOLAR 50,
inspired by Theremin's "Harmonium" and the *Blade Runner 2049* soundtrack [M42 p.2]. It is "fully
analogue ... except for the touch-sensitive multifunctional keyboard and effector which are
semi-digital" [M42 p.26]. Designer: Arseniy Tokarev; PCB and touch keyboard firmware: Szabo Mate
"DOBOZ"; production by NA company, Marupe, Latvia; "Made in Riga, LV" [M42 p.25-27].

Eight voices as separate blocks: six "drone" voices without V/oct (four 5-oscillator "classic"
voices from the SOLAR 50 and two "Papa Srapa" noise-synth voices) and two V/oct VCO voices played
from a 12-plate capacitive touch keyboard. A 10-channel stereo pan mixer feeds a dual (L/R) Polivoks
12 dB filter and a dual cartridge digital effector [M42 p.3-6]. **No MIDI** on any version (not
mentioned by any manual or product page [M42][EP42][EP42N]; owners confirm "no MIDI is pretty much a
dealbreaker" [ET, Sep 2025]).

### 1.2 Timeline and variants

| Model | Dates | Key facts |
|---|---|---|
| SOLAR 50 "BIG Ambient Machine" | Teased Nov 2019 [SA50]; pre-order Apr 2020 [SA] (about EUR 950 in 2020, UNCONFIRMED, bonedo via search) | 10 voices x 5 saw oscillators = 50 [PC50]; photo orb and touch plate per bank; mono 12 dB Polivoks LP; cartridge effector [PC50]. About 240 units built (forum claim, UNCONFIRMED) [ET, Oct 2023] |
| SOLAR 42 (MK1, "OG") | Shown Superbooth May 2023 [SS23][PC]; pre-order Jul/Aug 2023 at EUR 1500 + shipping, cartridge pack EUR 100 [GN][ST]; first units shipped Dec 2023 - Jan 2024 [ET]; later EUR 1800 [ET, Sep 2024] | Spec of this document's primary panel. Keyboard firmware 2.0 for serials 001-206 (Mar 2024) [ET] |
| SOLAR 42F (MK2) | Announced May 2024 [ET]; pre-order 31 Jul 2024, shipping Oct-Nov 2024 [SS24] | New panel graphics, voices renumbered (drones 1-6, VCO A/B), filter LP/BP + stereo distortion, sub-osc, lin/exp CV, bypass per effector channel, joystick LEDs, push-buttons instead of many toggles, VCO dry outs moved to front [M42F p.26][EP42N]. Keyboard firmware v3 [SIG] |
| SOLAR 42N (MK3) | Announced Jun 2025 at EUR 1900 [ET quoting Elta shop]; first owner reports Nov 2025 [ET] | LFO speed switch (x1/x6/x10), independent CV for both filter channels (CV L normalled to CV R) with MOD L / MOD R amounts, simplified cartridge loading, bypass switches removed, "return to the design of the first model" [M42N p.26][EP42N] |

Recommendation for AT42: model the **42N signal path as a superset** (it contains everything the 42
has except the 8-position octave rotary and rear dry outs), with a "panel revision" option. Section 2
documents the original 42 in full and lists the 42F/42N deltas with their own coordinates.

### 1.3 Physical

- Size: 49.5 cm x 32 cm x 2.9 cm; 5.6 cm high including knobs [M42 p.4][EP42]. The panel render
  is exactly 495.0 x 320.0 mm [R42].
- Weight: 5.4 kg (11.9 lb) [M42 p.4]. Shipping box 67 x 42 x 10 cm, 7 kg [EP42].
- Power: DC 12 V, 1-2 A; supplied PSU 12 V 2 A [M42 p.4].
- Box: unit, 12-cartridge kit (+ OCHRE card), PSU, manual, 5 minijack patch cables, stickers [EP42].
- Case: metal ("built like a tank" [ET, Feb 2024]); panel removable by ten M2 hex screws [ET, Mar 2024].
  The whole case acts as a contact microphone (piezo under the touch keyboard) [M42 p.12].
- Colours (powder-coated panel): white cream (RAL 9001), black, maize yellow (RAL 1006), green brown
  (RAL 8000), pink [EP42][ET, Oct 2023]; 42N adds blue [PC]; limited "alumo white" and "white with
  red keys" editions [ET, Jun 2025]. Render panel colour: `#E9DFD2` [R42].
- Patch points: 3.5 mm minijacks (Thonkiconn-style, render shows a hex nut 8.0 x 6.9 mm and a
  6.0-7.0 mm ring) [R42]. Main stereo out and ext audio in are on the rear edge; headphone jack on
  the front [PC][R42].

### 1.4 Overall panel layout (all versions)

Coordinates: origin at the panel's top-left corner, x to the right, y downward, millimetres
[R42]. The panel is mirror-symmetric around x = 247.5 mm.

```
y   0 ┌──────────────────────────────────────────────────────────────────────────────┐
      │ rear-edge legends: EXT.AUDIO  WET OUT R L  DRY OUT V4 V5         POWER 12V   │
 23   │ [wordmark area 10-160 x 23-44]               [lettering/logo 358-466 x 26-44]│
 43   │                   ┌──────── DUAL EFFECTOR 166.8-328.6 ───────┐               │
 53   │ VOICE1  VOICE2    │ cvx cvy cvz  cart.slot BLEND MASTER  ☊    │ VOICE6 VOICE7 │
      │ 4.6-84.5 85.9-165.8 X  Y  Z     1-2-3 L/R                   │329.7-  411.0- │
 89   │                   │ FILTER L freq res  cv  link  freq res FILTER R          │
110   │                   ├──────── MIXER 10 x (PAN / VOL) ──────────┤               │
117   │ VOICE3  VOICE4 / VCO1          (y 110-147)        VOICE5 / VCO2   VOICE8     │
      │ 4.6-84.5 85.9-234.4 incl. EG VCA 1        260.1-409.6 incl. EG VCA 2 411.0-  │
182   │ LFO A | JOYSTICK | 5 STEP SEQ. VOLTAGE | PREAMP | ENVELOPE FOLLOWER | LFO B  │
212   │                                                                              │
227   │ (joystick)  ┌──── TOUCH SENSITIVE CONTROLLER 82.5-412.4 x 227.5-307.5 ─┐ DRONE │
      │  37,267     │ 6 plates  [jacks][▽][enc][△][jacks]  6 plates           │ KEYS  │
307   │             └──────────────────────────────────────────────────────────┘ 3x2  │
320   └──────────────────────────────────────────────────────────────────────────────┘
```

Section boxes are drawn as rounded black outlines (stroke about 0.35 mm) with black title tabs
(white bold uppercase text, tab height 5.0 mm) [R42].

---

## 2. Panel description (original SOLAR 42, from [R42], cross-checked with the panel drawing on [M42 p.2])

### 2.0 Conventions and component vocabulary

All centres are measured from the 1:1 vector render; precision of the render geometry is about
±0.2 mm. The production silkscreen of MK1 matches the render label-for-label ([M42 p.2] drawing,
Jan 2024), but physical tolerances are **UNCONFIRMED**. Diameters are the drawn diameters.

| Code | Component as drawn [R42] |
|---|---|
| KS | Small knob: Ø8.0 disc with white dot pointer, no skirt (drone TUNE, ATT, RLS, CV) |
| KM | Medium knob: Ø12.1 knurled skirt (`#353535`) with Ø9.5 coloured cap and white line pointer |
| KL | Large knob: Ø14.0 skirt, Ø9.5 cap (VCO, bottom-row knobs) |
| KXL | Master volume: Ø17.9 skirt, Ø12.1 cap, with an 11-tick scale |
| RS | Rotary switch: Ø14.0 (octave, 8 positions) or Ø18.8 skirt / Ø12.8 grey cap (waveform, 6 positions) |
| BT | Round push-button Ø6.5 (MUTE black, MOD grey `#8E969E`, HOLD light grey `#C6CCCE`) |
| TG | Mini toggle: Ø5.5 bushing, Ø4.3 nut, bat lever drawn pointing down (2-position unless stated) |
| J | 3.5 mm jack: grey hex nut 8.0 x 6.9, Ø7.0 ring, Ø4.3 hole |
| L | LED Ø3.5 (amber `#EDAD00`, red `#FF230A`, blue `#0589CC`, green `#35AA00`) |
| ORB | Photo-sensitive detector: Ø23.3 light-grey dome (`#D6D3CE`) |
| BAR | "OSC STATUS" 5-segment LED bar window, 14.2 x 11.2 mm, black, segments red (on) / grey (muted) |

Knob cap colours in the MK1 render: red `#F23016`, slate `#47545E`, teal-blue `#006893`, sky blue
`#0093DB`, black `#000000`, light grey `#C6CCCE`. Pointer: white. Scales: thin black arc with
start/end dots and an arrow at the clockwise end; filter knobs and master volume have 11 radial
ticks [R42].

### 2.1 Rear edge (legends printed at the top edge; connectors on the rear panel)

| Legend | x (legend centre) | Function | Source |
|---|---|---|---|
| EXT. AUDIO | 18.0 | External audio input, mono (summed into mixer channel EXT.AUDIO) | [R42]; mono confirmed by Elta via owner [ET, Dec 2024] |
| WET OUT R / L | 34.5 / 56.7 | Main stereo output after effector and master volume, max 2 V | [M42 p.4] |
| DRY OUT VOICE 4 / VOICE 5 | 77.5 / 97.8 | Direct VCO voice outs, max 1 V (MK1 only; moved to front on F/N) | [M42 p.4][M42F p.26] |
| POWER 12V DC 2A | about 468 | DC barrel | [R42] |

Exact rear connector positions and type (3.5 vs 6.35 mm) are **UNCONFIRMED** (estimate: 3.5 mm,
directly below each legend). No power switch is documented (UNCONFIRMED).

### 2.2 Drone voices 1, 2, 6, 7 ("classic", from SOLAR 50)

Box: voice 1 at x 4.6-84.5, y 53.3-115.4 (upper box 4.6-68.4 x 53.3-113.7 for the oscillator
columns plus the orb area). Voice tag (black square with white number) at bottom right
(81.7, 113.5). Vertical side tab on the left with rotated legends MUTE / TUNE / MOD (upper) and
HOLD / GATE (lower) [R42].

Voice 1 coordinates. For the other voices add **dx = +81.3** (voice 2), **+325.1** (voice 6),
**+406.4** (voice 7). Rows are identical (not mirrored).

| Element | Label | Type | Centre (x, y) | Notes |
|---|---|---|---|---|
| Column numbers | 1 2 3 4 5 | text | x = 12.9, 25.4, 38.4, 51.0, 63.7; y 55.4 | oscillator index |
| Mute buttons x5 | MUTE | BT black | (12.8 / 25.5 / 38.2 / 50.9 / 63.6, 60.9) | latching on/off per oscillator [M42 p.7] |
| Tune knobs x5 | TUNE | KS black | (12.8 / 25.5 / 38.4 / 50.9 / 63.6, 71.0) | one per oscillator; a short ladder graphic links rows |
| Mod buttons x5 | MOD | BT grey | (12.8 / 25.5 / 38.2 / 50.9 / 63.6, 83.7) | connects that oscillator to the photo-sensor [M42 p.7] |
| OSC status bar | 1 2 3 4 5 | BAR | window 69.2-83.4 x 55.3-66.5 (legend "12345" at 76.3, 68.1) | shows which oscillators are on [M42 p.7 "OSC STATUS"] |
| Volt | VOLT | KM teal-blue | (76.3, 78.6) | transposes all 5 down; past half travel the oscillators cross-modulate (FM) [M42 p.7] |
| Hold | HOLD | BT light grey | (12.8, 93.9) | latches the voice VCA open |
| Gate LED | - | L amber | (12.8, 100.2) | gate/envelope activity |
| Gate in | GATE | J | (12.8, 109.1) | external gate; normalled from drone key (UNCONFIRMED: normalling) |
| Attack | ATT | KS slate | (25.5, 93.9) | AR envelope attack |
| Release | RLS | KS slate | (38.4, 93.9) | AR envelope release |
| CV amount | CV | KS black | (51.0, 93.9) | level of the CV input that drives the orb's internal red LED (UNCONFIRMED; inferred from [M42 p.10] and [ET, Oct 2023]) |
| Env out | env out (with a Ø-ring icon at 23.5, 108.7) | J | (38.2, 109.1) | envelope out, -10..+10 V spec [M42 p.4] |
| CV in | - (under CV knob) | J | (50.9, 109.1) | "CV MOD" input [M42 p.7] |
| Photo orb | (PHOTO-SENSITIVE DETECTOR) | ORB | (70.0, 102.8), Ø23.3 | ambient light + internal red LED modulate pitch of MOD-enabled oscillators [M42 p.7, p.10] |

### 2.3 Drone voices 3 and 8 ("new", Papa Srapa noise synth)

Box: voice 3 at x 4.6-84.5, y 116.8-181.5; title tab "VOICE 3" top right (66.7-84.5 x 116.8-121.8).
Voice 8 = voice 3 translated by **dx = +406.4** (not mirrored) [R42].

| Element | Label | Type | Centre voice 3 | Voice 8 | Function [M42 p.8] |
|---|---|---|---|---|---|
| Rate multiplier | x10 / x1 | TG | (23.5, 126.9) | (429.9, 126.9) | modulator oscillator range |
| FM switch | fm | TG | (44.6, 126.9) | (451.0, 126.9) | modulator -> tone FM on/off |
| AM switch | am | TG | (65.6, 126.9) | (472.0, 126.9) | modulator -> tone AM (chopping) on/off |
| Rate | rate | KM slate | (12.8, 134.5) | (419.2, 134.5) | modulator (LF Schmitt-trigger square) frequency |
| FM amount | fm amt | KM slate | (33.9, 134.5) | (440.3, 134.5) | FM depth |
| Divider | divider | KM slate | (55.0, 134.5) | (461.4, 134.5) | second FM depth/deviation control |
| Pitch | PITCH | KM teal-blue | (76.1, 134.5) | (482.5, 134.5) | tone oscillator pitch, about C0..E7 overall |
| Range | range | TG | (65.6, 142.1) | (472.0, 142.1) | tone oscillator range |
| Modulator out | out | J | (23.5, 145.9) | (429.9, 145.9) | square, 0..+12 V [M42 p.4] |
| FM in | fm in | J | (44.6, 145.9) | (451.0, 145.9) | external FM input |
| Hold | HOLD | BT | (12.8, 157.4) | (419.2, 157.4) | VCA open |
| Gate LED | - | L amber | (12.8, 163.7) | (419.2, 163.7) | |
| Attack / Release | ATT / RLS | KS slate | (25.5, 157.4) / (38.2, 157.4) | (431.8 / 444.7, 157.4) | AR envelope |
| Noise | NOISE | KM light grey | (57.2, 157.4) | (463.7, 157.4) | white noise level into the voice |
| S&H LED | (S&H tab 69.5-84.5 x 166.9-171.5) | L red | (76.3, 157.4) | (482.7, 157.4) | S&H activity; decorative dots around it |
| Gate in | GATE | J | (12.8, 172.6) | (419.2, 172.6) | |
| Icon | - | "Y" glyph in a box 21.0-29.8 x 168.2-179.5 | | | decorative/noise symbol (UNCONFIRMED meaning) |
| Env out | env out | J | (38.2, 172.6) | (444.6, 172.6) | |
| S&H in | in | J | (50.9, 172.6) | (457.3, 172.6) | normalled from the voice's white noise [M42 p.8] |
| S&H clock | clock | J | (63.6, 172.6) | (470.0, 172.6) | needs a square clock (LFO square or PULSER) [M42 p.8][ET] |
| S&H out | out | J | (76.3, 172.6) | (482.7, 172.6) | -5..+5 V [M42 p.4] |

Red arrows on the panel show internal normals: rate -> out jack; fm switch -> fm amt; am switch ->
am; fm in -> fm amt [R42].

### 2.4 Voices 4 and 5 (V/oct VCOs, "VCO 1" / "VCO 2") with EG VCA 1 / 2

VCO 1 box x 85.9-234.4, y 116.8-181.5 (title tab "VOICE 4" and sub-label "VCO 1"). The EG VCA 1
sub-box sits inside it at x 166.1-234.4, y 147-181.5 (shares space under the mixer). VCO 2 / EG VCA 2
box x 260.1-409.6 [R42].

| Element | Label | Type | VCO 1 centre | VCO 2 centre | Function / range |
|---|---|---|---|---|---|
| FM attenuator | (arrow from fm jack) | KL black | (100.4, 134.5) | (344.4, 134.5) | linear FM depth [M42 p.9] |
| Octave | octave: LO, 64', 32', 16', 8', 4', 2', 1' | RS Ø14 black, 8 positions | (125.8, 134.5) | (369.8, 134.5) | 8-position octave switch [M42 p.9]. Legend positions (VCO 1): LO (111.7, 139.2), 64' (112.0, 130.7), 32' (115.1, 123.7), 16' (122.7, 120.3), 8' (130.1, 120.3), 4' (136.8, 123.7), 2' (139.3, 130.7), 1' (138.5, 139.2) |
| Tune | tune | KL teal-blue | (151.5, 134.5) | (395.4, 134.5) | continuous over 1 octave [M42 p.9] |
| Shape attenuator | (arrow from shape jack) | KL light grey | (100.4, 157.5) | (344.4, 157.5) | CV depth for SHAPE input |
| Waveform | 6 icons: sine, triangle, saw, square, morph saw<->inverted saw, morph sine<->triangle | RS Ø18.8, grey cap | (125.8, 157.5) | (369.8, 157.5) | 6 waveforms [M42 p.9]. Icon positions roughly at -150°, -120°, -60°... around the knob (see R42); sine at left (~108, 160), "X" morph and "∿×" morph at right |
| Pulse width | pw | KL light grey | (151.5, 157.5) | (395.4, 157.5) | pulse width / SHAPE manual offset |
| V/oct in | volt oct | J | (94.1, 172.6) | (337.9, 172.6) | normalled from touch keyboard V/OCT (both VCOs) [M42 p.9, p.13] |
| FM in | fm | J | (116.9, 172.6) | (360.8, 172.6) | VCO 2: normalled from VCO 1 output [M42 p.9] |
| Shape in | shape | J | (134.7, 172.6) | (378.6, 172.6) | PW/shape CV |
| VCO out / Sync | vco out (VCO 1) / sync (VCO 2) | J | (157.8, 172.6) | (401.7, 172.6) | VCO 1 output -5..+5 V; VCO 2 hard-sync input [M42 p.4, p.9] |
| EG hold | hold | TG | (182.5, 153.6) | (280.0, 153.6) | VCA always open |
| EG loop | loop | TG | (198.7, 153.6) | (296.3, 153.6) | envelope cycles (LFO) |
| EG LED | - | L amber | (215.0, 153.6) | (312.5, 153.6) | |
| Attack / Decay / Sustain / Release | attack decay sustain release | KM light grey | x 174.4 / 190.6 / 206.9 / 223.0, y 162.5 | x 272.1 / 288.3 / 304.5 / 320.7, y 162.5 | ADSR |
| Gate in | gate in | J | (182.5, 175.4) | (280.0, 175.4) | normalled from keyboard GATE LEFT (MAIN) [M42 p.13] |
| EG out | eg out | J | (198.7, 175.4) | (296.3, 175.4) | 0..+8 V [M42 p.4] |
| VCA CV | vca cv | J | (215.0, 175.4) | (312.5, 175.4) | external VCA control |

### 2.5 Dual effector (top centre)

Box x 166.8-328.6, y 43.1-109.7 with title tab "DUAL EFFECTOR" (y 43.1-48) [R42].

| Element | Label | Type | Centre | Function |
|---|---|---|---|---|
| CV X / Y / Z in | ▸cv x, ▸cv y, ▸cv z | J | (174.3, 57.6), (198.7, 57.6), (223.1, 57.6) | -10..+10 V CV for the three program parameters, shared by both channels [M42 p.22] |
| X / Y / Z | X Y Z | KM red with tick scale | (174.4, 73.5), (198.7, 73.5), (223.1, 73.5) | program parameters (meaning per cartridge, section 3.10) |
| Cartridge slot | cartridge slot | slot graphic | outline 231.9-263.3 x 49.7-91.8; slot window 235.7-259.2 x 55.6-66.1 | card can be inserted either side [M42 p.22] |
| Program select L | 1 / 2 / 3 | TG 3-position | (238.6, 73.6) | program 1-3 of the cartridge |
| L/R fx | L/R fx | BT (unfilled outline Ø6.5) | (247.5, 73.6) | selects which channel loads (off = L, on = R) |
| Program select R | 1 / 2 / 3 | TG 3-position | (256.4, 73.6) | |
| Reset L / Reset R | L reset R | small tact buttons Ø3.5 | (239.4, 83.7), (255.6, 83.7) | load/reset the effect into that channel |
| Blend | BLEND | KM slate | (271.8, 73.6) | dry/wet effect level |
| Master volume | MASTER VOLUME | KXL red | (296.3, 73.6) | WET OUT level. Manual: keep at max 3-4 o'clock, effector noise at full [M42 p.26] |
| Headphones | (headphone icon at 320.5, 47.7) | J | (320.7, 57.6) | stereo phones |
| Phones volume | - | KM sky blue | (320.6, 73.5) | crunchy pot noted in MK1 [M42 p.26] |

### 2.6 Dual filter (inside the effector box, lower strip y 89-110)

Rotated side tabs "FILTER L" (x about 167-171) and "FILTER R" (x about 325-329) [R42].

| Element | Label | Type | Centre | Function [M42 p.21] |
|---|---|---|---|---|
| Freq L | freq | KM slate, 11 ticks | (182.5, 99.2) | cutoff L |
| Res L | res | KM slate, 11 ticks | (206.8, 99.2) | resonance L |
| CV freq in | ▸ (input arrow) | J | (227.4, 99.2) | cutoff CV |
| CV freq | cv freq | KM light grey | (247.6, 99.2) | CV amount (a line connects jack to knob) |
| Link | link | TG | (267.6, 99.2) | CV of filter 1 also drives filter 2 |
| Freq R | freq | KM slate | (288.1, 99.2) | cutoff R |
| Res R | res | KM slate | (312.4, 99.2) | resonance R |

### 2.7 Voice mixer (centre, y 110.4-146.6)

Box x 166.8-328.6, y 110.4-146.6. Ten channel strips 16.2 mm apart, each with a PAN knob (black KM,
"PAN" + dot above, "L•" / "•R" at the sides) and a VOL knob (slate KM), separated by a black legend
band (y 125.5-131) [R42]. Below: "VOICE MIXER" tab (235.2-259.4 x 146.5-155.3) and a decorative
graphic (240.0-254.9 x 161.1-176.1), not to be reproduced.

| Ch | Legend | x | PAN y | VOL y |
|---|---|---|---|---|
| 1 | VOICE 1 | 174.4 | 119.3 | 139.3 |
| 2 | VOICE 2 | 190.6 | 119.3 | 139.3 |
| 3 | VOICE 3 | 207.0 | 119.3 | 139.3 |
| 4 | EXT.AUDIO | 223.1 | 119.3 | 139.3 |
| 5 | VOICE 4 | 239.5 | 119.3 | 139.3 |
| 6 | VOICE 5 | 255.7 | 119.3 | 139.3 |
| 7 | PREAMP | 271.8 | 119.3 | 139.3 |
| 8 | VOICE 6 | 288.0 | 119.3 | 139.3 |
| 9 | VOICE 7 | 304.2 | 119.3 | 139.3 |
| 10 | VOICE 8 | 320.6 | 119.3 | 139.3 |

### 2.8 Bottom CV row (y 182.3-212.2)

Six boxes with black title bands at the bottom (y 207.2-212.2) [R42].

| Box (x range) | Element | Label | Type | Centre | Function |
|---|---|---|---|---|---|
| LFO A (4.4-60.5) | Wave | wave (Λ ... ⊓ icons) | KL slate | (13.6, 192.4) | triangle <-> square mix [M42 p.10] |
| | Out | out | J | (31.9, 192.4) | 0..+10 V unipolar |
| | LED | - | L blue | (31.9, 201.6) | |
| | Rate | rate | KL black | (50.1, 192.4) | LFO A is the slow one [M42 p.10] |
| JOYSTICK (61.5-123.2) | Offset X | offset X | KL light grey | (72.2, 192.4) | range offset (see 3.6) |
| | X out | - | J | (92.3, 187.3) | -10..+10 V |
| | Y out | - | J | (92.3, 197.5) | |
| | Offset Y | offset Y | KL light grey | (112.4, 192.4) | |
| 5 STEP SEQ. VOLTAGE (124.3-316.3) | Pulser | pulser | KL slate | (134.2, 192.4) | internal clock rate [M42 p.11] |
| | Clock out | out | J | (152.5, 187.3) | pulser out, -10..+10 V [M42 p.4] |
| | Ext clock in | ext / clock | J | (152.5, 197.5) | |
| | Stages | stages 3 / 4 / 5 | TG 3-position | (166.5, 192.4) | sequence length |
| | Step 1-5 knobs | step 1 ... step 5 | KM red | x 181.0, 205.4, 229.7, 254.1, 278.7; y 192.4 | 0..+5 V each |
| | Step LEDs | - | L red | x 193.1, 217.5, 241.9, 266.3, 290.7; y 187.9 | current step |
| | Step gate switches | gate | TG | x 193.1, 217.5, 241.9, 266.3, 290.7; y 195.0 | gate on/off per step (CV unaffected) |
| | CV out | cv out | J | (306.7, 187.3) | 0..+5 V |
| | Gate out | gate out | J | (306.7, 197.5) | 0..+10 V |
| | Decoration | - | dashed bar | y about 210, x 173-316 | |
| PREAMP (317.2-355.5) | Ext source in | ext. source | J | (325.0, 192.4) | bypasses the internal piezo [M42 p.12] |
| | Gain | gain | KL slate | (339.7, 192.4) | up to 40 dB, 1 MΩ input [M42 p.12] |
| | Clip LED | - | L blue | (351.9, 192.4) | "Amplifier clipping indicator" |
| ENVELOPE FOLLOWER (356.4-433.5) | Attack | attack | KM light grey | (364.1, 192.4) | |
| | Release | release | KM light grey | (388.5, 192.4) | |
| | Envelope LED | - | L red | (402.2, 192.4) | follower level |
| | Envelope out | envelope | J | (409.8, 192.4) | 0..+10 V |
| | Gate LED | - | L green | (417.4, 192.4) | gate detector activity |
| | Gate out | gate | J | (425.0, 192.4) | 0..+8 V |
| LFO B (434.5-490.9) | Wave | wave | KL slate | (444.9, 192.4) | |
| | Out | out | J | (463.1, 192.4) | |
| | LED | - | L blue | (463.1, 201.6) | |
| | Rate | rate | KL black | (481.4, 192.4) | LFO B is the fast one |

### 2.9 Touch sensitive controller (bottom centre)

Black panel x 82.5-412.4, y 227.5-307.5 (329.9 x 80.0 mm), legend "TOUCH SENSITIVE CONTROLLER"
above its right end (x 351-398, y 223.0). Mounting screws Ø3.2 at y 223.8 and 311.2,
x = 76.2, 162.5, 247.5, 332.5, 418.7 [R42]. Plate positions measured from the raster of [R42]
(±0.5 mm). Plates are white frames with horizontal black stripes (about 19 x 50 mm outer).
The plates follow an arch: heights step 7.5 mm.

| Plate | Centre (x, y) | Plate | Centre (x, y) |
|---|---|---|---|
| 1 | (95.0, 270.9) | 7 | (278.0, 277.9) |
| 2 | (119.7, 263.2) | 8 | (302.2, 270.9) |
| 3 | (144.0, 255.7) | 9 | (327.0, 263.2) |
| 4 | (168.5, 263.2) | 10 | (351.5, 255.7) |
| 5 | (193.1, 270.9) | 11 | (376.1, 263.2) |
| 6 | (217.5, 277.9) | 12 | (400.6, 270.9) |

Pitch 24.5 mm; centre gap 60.5 mm. Two decorative outline triangles (about 20 mm, under plates 3
and 10, centred about (144, 289) and (351.5, 289)). Centre column:

| Element | Icon | Centre | Function [M42 p.13-14] |
|---|---|---|---|
| Clock in | clock face | J (Ø6.0 ring) (184.1, 234.7) | external clock, 0..+5 V recommended |
| V/oct out | lightning | J (196.7, 237.8) | 0..+8 V, normalled to both VCO V/oct |
| Gate left (main) out | ⊓ gate | J (209.5, 241.0) | 0..+10 V, normalled to EG 1/2 gate |
| Transpose down | ▽ | red button Ø9.5 (225.5, 245.1) | octave/offset buttons |
| Encoder | - | white ring Ø about 18 with white Ø7 cap, (247.5, 245.0) | scroll/click |
| Display | - | blue window 236.0-258.7 x 256.6-263.1 | parameter display (type UNCONFIRMED, likely small OLED/LCD) |
| Sun graphic (piezo location) | - | about Ø32 centred (247.5, 283) | contact mic "mounted under the sun" [M42 p.12]. Do not copy the artwork |
| Transpose up | △ | red button Ø9.5 (269.7, 245.1) | |
| Gate right out | ⊓ | J (285.8, 241.0) | 0..+10 V |
| Pressure out | arrow onto bar | J (298.2, 237.8) | 0..+8 V |
| Reset in | ↩ | J (311.0, 234.7) | arpeggiator reset |

### 2.10 Drone keypad and joystick

- Legend "DRONE VOICES" (448-466, 223.0). Six keycap-style push-buttons, 17.0 x 17.0 mm, each
  with a small white indicator window at the top (owners: a red LED lights when pressed [ET, Dec
  2023]). Centres: key 1 (447.0, 240.5), key 6 (467.0, 240.5), key 2 (447.0, 267.5), key 7
  (467.0, 267.5), key 3 (447.0, 294.5), key 8 (467.0, 294.5). Number legends above each key at
  y 229.1 / 256.1 / 283.0 [R42]. Momentary 5 V gates into the drone voices' AR envelopes [M42 p.7].
  Screws at (433.5, 223.8 / 267.5 / 311.2) and (480.5, 223.8 / 267.5 / 311.2).
- Joystick: housing Ø23-24 at (37.2, 267.5), stick cap Ø10.1; direction arrows ▲ (37.2, about 245),
  ▼ (37.2, about 290), ◀ (about 16, 267.5), ▶ (about 59, 267.5); screws at (24.2, 250.7),
  (50.1, 250.7), (24.2, 284.2), (50.1, 284.2). "Small position-locking ... joystick made with a style
  of game consoles" [M42 p.10]; owners hope it lasts [ET, Dec 2023].

### 2.11 SOLAR 42F deltas [R42F][M42F]

- New silkscreen: outputs labelled red, inputs black; colour-coded knobs per block; voices
  renumbered DRONE 1-6 (Papa Srapa voices become DRONE 3 and DRONE 6), VOICE 4/5 become VCO A /
  VCO B; mixer legends DRONE 1, DRONE 2, DRONE 3, EXT.AUDIO, VCO A, VCO B, PREAMP, DRONE 4, DRONE 5,
  DRONE 6 [M42F p.26][R42F].
- Filter: BP/LP button per side, MOD (cv amount) knob, DIST and GAIN knobs (stereo distortion after
  the filter) [M42F p.21].
- Effector: "channel" L/R button, reset L/R, and per-channel **bypass** buttons [M42F p.22].
- VCO: octave rotary replaced by "oct+3 / low" switch and "-1 ⊓ sub" switch; lin/exp CV switch;
  "cv amt" and "pwm" knobs; dry outs "VCO A" / "VCO B" on the front panel (EG blocks) [M42F p.9, p.26].
- Joystick block: position LEDs; envelope blocks "envelope A/B" with hold and loop buttons [M42F].
- Lower-left joystick graphic replaced by an alien figure (do not copy) [R42F].

### 2.12 SOLAR 42N deltas, with coordinates [R42N][M42N]

Same grid as MK1. Knob colours in [R42N]: orange `#EA8C3F` (effector + filter), teal `#005E7A`
(drone voices VOLT/rate/mod/divider/PITCH/NOISE), green `#005E3A` (VCOs and envelopes), light grey
`#C4C4C4` (mixer VOL), black (PAN, drone KS knobs, all buttons), red `#D8352B` (whole bottom row and
the keyboard encoder). Bottom-row knobs and sequencer steps are all KL (Ø14) in 42N.

| Block | Element (label) | Type | Centre |
|---|---|---|---|
| Drone 1/2/4/5 | as MK1; HOLD, ATT, RLS, CV now black; orb white | | MK1 coordinates |
| Drone 3/6 | LFO x1/x10 button ("LFO ... 1 10") | button Ø5.5 | (23.3, 126.9) / (429.8, 126.9) |
| | fm, am buttons | button Ø5.5 | (44.5, 126.9), (65.6, 126.9) / (451.0, 126.9), (472.1, 126.9) |
| | rate, mod, divider, PITCH | KM teal | x 12.8, 33.9, 55.0, 76.1 (D3) / 419.2, 440.3, 461.4, 482.5 (D6); y 134.5 |
| | hi/low button | button Ø5.5 | (65.6, 142.1) / (472.1, 142.2) |
| | LFO out, ▲cv in | J | (23.5, 145.9), (44.6, 145.9) / (429.9, 145.9), (451.0, 145.9) |
| | NOISE | KM teal | (57.2, 157.3) / (463.7, 157.3) |
| | jacks gate, env out, S&H in, clock, out | J | x 12.8, 38.2, 50.9, 63.6, 76.3 (D3); 419.2, 444.6, 457.3, 470.0, 482.7 (D6); y 172.6 |
| VCO A / VCO B | MORPHING WAVEFORM legend | text | (125.8, 120) / (369.5, 120) |
| | cv amt | KM green | (100.4, 134.5) / (344.2, 134.5) |
| | oct+3 / low | TG | (118.2, 134.5) / (362.0, 134.5) |
| | -1 ⊓ sub | TG | (133.7, 134.5) / (377.5, 134.5) |
| | tune | KL green | (151.5, 134.5) / (395.4, 134.5) |
| | lin / exp | TG | (91.8, 146.0) / (335.6, 145.9) |
| | pwm | KM green | (100.4, 157.4) / (344.2, 157.4) |
| | waveform | RS Ø18.8 green | (125.8, 157.4) / (369.8, 157.4) |
| | pw | KL green | (151.5, 157.4) / (395.4, 157.4) |
| | jacks 1v/oct, cv, pwm, sync (A) / osc (B) | J | x 94.1, 116.9, 134.7, 157.8 / 337.8, 360.8, 378.6, 401.7; y 172.6 |
| | calibration trimmers 1-7 (A), 8-14 (B), 15 (seq) | small grey Ø2.2 with number legends | near (111.5,122), (90.7,130.8), (125.5,136.9), (98.1,147.4), (103.2,147.4), (90.3,167.6), (105.4,175.6); B mirrored; 15 at (316.4,192). Decorative for the UI |
| Envelope A / B | hold button, loop button (ʌʌ icon), mode icon, LED | buttons Ø6.5 | (182.4, 153.5), (198.7, 153.5), LED (215.0, 153.5) / (280.0, 153.5), (296.3, 153.5), LED (312.5, 153.5) |
| | A D S R | KM green | x 174.4, 190.6, 206.9, 223.0 / 272.1, 288.3, 304.5, 320.7; y 162.4 |
| | jacks | J | A: gate (182.5), env (198.7), vca cv (215.0), VCO A out (231.3); B: VCO B out (263.7), gate (280.0), env (296.3), vca cv (312.5); y 175.4 |
| Filter | FREQ L, RES L | KM orange | (182.5, 99.1), (206.8, 99.1) |
| | BP/LP L button | button Ø6.7 | (194.6, 93.6) |
| | CV L in | J | (218.5, 99.2) |
| | DIST | KM orange | (230.3, 99.1) |
| | link | button Ø6.6 | (247.6, 99.2) |
| | GAIN | KM orange | (264.6, 99.2) |
| | CV R in | J | (276.6, 99.2) |
| | FREQ R, RES R | KM orange | (288.1, 99.1), (312.4, 99.1) |
| | BP/LP R button | button Ø6.7 | (300.3, 93.6) |
| | MOD L, MOD R (CV amounts) | black Ø7.7 (small knob without pointer in the render; type UNCONFIRMED) | (238.8, 78.7), (256.5, 78.7) |
| Effector | program 1-2-3 L / R | TG 3-pos | (238.6, 66.7) / (256.4, 66.7) |
| | centre element | black Ø6.7 | (247.5, 66.7), function UNCONFIRMED (manual: loading by flipping the 1-2-3 switch only) |
| | X, Y, Z, BLEND, MASTER, phones | orange | MK1 positions |
| LFO A / LFO B | speed switch x6 / x1 / x10 | TG 3-pos | (39.8, 195.0) / (469.3, 195.0) |
| | wave, out, LED, rate | KL red / J / L | A: (13.6,192.4), (28.3,192.4), (28.4,201.6), (52.1,192.4); B: (443.2,192.4), (458.0,192.4), (457.9,201.5), (481.4,192.4) |
| Joystick | X, Y knobs | KL red | (73.0, 192.4), (113.1, 192.4) |
| | X / Y out | J | (93.2, 187.4), (93.2, 197.5) |
| | position LEDs | L red | (84.0, 192.4), (102.3, 192.4) |
| Seq | pulser, steps (KL red) | | pulser (134.2,192.4); steps x 181.0, 205.4, 229.7, 254.1, 278.5 |
| Preamp | mic icon under clip LED | | (351.9, 204.4) |
| Keyboard | encoder | KL red Ø14 | (247.4, 245.0) |
| Drone keys | legends 1 4 / 2 5 / 3 6 | | MK1 positions |
| Rear | EXT.AUDIO, WET OUT R L, POWER 12V DC (no DRY OUT) | | [R42N] |

---

## 3. Architecture and signal flow

### 3.1 Block diagram [M42 p.6][M42F p.6]

```
 drone keys 1..6 ─gate─┐                         touch keyboard (12 plates, digital)
                       ▼                          │V/oct  │gate L │gate R │pressure
 V1  V2  V3   EXT  V4(VCO1) V5(VCO2) PREAMP V6 V7 V8   └──► VCO1+VCO2 V/oct, EG1+EG2 gate (normals)
  │   │   │    │      │        │       │    │  │  │
  └───┴───┴────┴──── 10 x (VOL, PAN) stereo mixer ────┘          V4/V5 DRY OUT (rear, MK1;
                         │L            │R                         front on F/N)
                   Polivoks VCF L   Polivoks VCF R   (12 dB; F/N: LP/BP)
                         │             │
                  [F/N: distortion L   distortion R  (DIST mix, GAIN drive)]
                         │             │
                   effector L      effector R    (digital, cartridge programs, shared X/Y/Z, BLEND)
                         │             │
                     MASTER VOLUME + phones volume ──► WET OUT L/R (rear), phones (front)

 CV sources: LFO A, LFO B, joystick X/Y, 5-step seq (CV, gate, clock), EG1/EG2, drone envs 1,2,3,6,7,8,
 Papa Srapa modulators (V3, V8), S&H x2, envelope follower (CV, gate), keyboard (V/oct, gates, pressure)
```

Audio input options: EXT AUDIO (rear, mono, own mixer channel) and PREAMP (internal piezo or EXT.
SOURCE jack, own mixer channel) [M42 p.12][ET, Dec 2024]. All voices pass the filters and effector;
there are no per-voice outputs except V4/V5 dry [PC].

### 3.2 Classic drone voices (1, 2, 6, 7 on MK1; 1, 2, 4, 5 on F/N)

- 5 "simple sawtooth generators (NO VOLT OCTAVE)" per voice, each with only a TUNE pot, no
  linear/exponential CV law [M42 p.7]. Each oscillator is a single-transistor **negistor**
  (avalanche negative-resistance relaxation) oscillator [M42F p.7].
- Tuning ranges (approximate, "may vary slightly") [M42 p.7]:

| Osc | Range | Hz (12-TET, A4 = 440) |
|---|---|---|
| 1 | C0 - G2 | 16.4 - 98.0 |
| 2 | B1 - G3 | 61.7 - 196.0 |
| 3 | D3 - B4 | 146.8 - 493.9 |
| 4 | E4 - C6 | 329.6 - 1046.5 |
| 5 | G5 - D7 | 784.0 - 2349.3 |

  Overall about B0 (sic) to D7, 8 octaves. 42F text: "first and second oscillators are low pitched,
  the third medium, the fourth and fifth high pitched" [M42F p.7].
- MUTE per oscillator builds chords, dyads or single notes [M42 p.7]. No noise from muted drones
  on the 42 (an improvement over SOLAR 50 bleed) [M42 p.26][ET].
- MOD per oscillator: pitch is modulated by the photo-sensitive detector, which responds to ambient
  light and to an internal red LED driven by the CV input. "If the MOD buttons are not switched on,
  the generators run stably" [M42 p.7]. LFOs are unipolar because red LEDs only respond to positive
  voltage [M42 p.10]. The orbs also show CV/gate activity visually [ET, Oct 2023]. Daylight bulbs can
  be heard (flicker) [M42 p.26].
- VOLT knob: transposes all 5 down together; past half travel "generators start to modulate each
  other creating FM synthesis effect" [M42 p.7].
- Envelope: Attack/Release (AR) triggered by a 5 V gate from the drone keys or the GATE jack; opens
  the voice VCA; HOLD latches it open; env out can trigger the next voice or modulate filter/effector
  [M42 p.7]. Env out spec -10..+10 V [M42 p.4].
- Drift: chords drift after a while, "normal, as these are very primitive drone oscillators"
  [M42F p.7]; owners confirm difficulty tuning finely and warm-up drift [GS via search snippet], while
  another measured "extremely minimal" drift over 30 min [ET, Mar 2024].

### 3.3 Papa Srapa voices (3, 8 on MK1; 3, 6 on F/N)

"Based on the DIY noise synthesizers created by Russian electro-shaman artist Papa Srapa" (early
2000s); sirens, bird cries, space monsters, firearms volleys, surf, wind [M42 p.8].

- Two Schmitt-trigger oscillators: a low-frequency **square** modulator (RATE, x1/x10, CV OUT
  0..+12 V) and an audio-rate tone oscillator about C0..E7 (PITCH + RANGE) [M42 p.8].
- Modes (MK1 toggles; F/N buttons with inverted "down = on" logic [M42F p.8]):
  1. continuous tone (FM off, AM off);
  2. FM: modulator frequency-modulates the tone; depth by FM AMT and DIVIDER;
  3. AM: tone interrupted at the modulator rate;
  4. AM + FM: interrupted tone with FM;
  5. pure noise: (MK1) all knobs fully left, switches down, NOISE max; (F/N) PITCH zero, MOD and
     DIVIDER fully right, NOISE max.
- White noise (one generator per Papa Srapa voice: "two white noise generators" [M42 p.3]) is mixed
  by NOISE and normalled into the voice's S&H IN; S&H needs an external clock (LFO square or pulser);
  S&H out -5..+5 V [M42 p.4, p.8]. Only square clocks work reliably [ET, Mar 2024].
- AR envelope + VCA + HOLD as the classic voices [M42 p.8].

### 3.4 VCO voices (4/5, "VCO 1/2"; F/N "VCO A/B")

- Triangle-core VCO on the AS3340 chip (CEM3340 clone); 6 waveforms: 4 classic (sine, triangle,
  saw, square/pulse) plus two morphing (saw <-> inverted saw, sine <-> triangle) [M42 p.9].
- PW control (SHAPE) with CV input and attenuator; linear FM input with attenuator; 8-position
  octave switch (MK1); tune over 1 octave; external sync; V/oct input [M42 p.9].
- MK1 normals: VCO 1 output -> VCO 2 FM input (attenuated by FM knob); VCO 1 has VCO OUT, VCO 2
  has SYNC; keyboard V/oct and gate go to both voices [M42 p.9].
- F/N: oct "+3 / low" switch, sub-oscillator -1 octave, lin/exp CV switch, CV AMT knob; VCO A has
  SYNC, VCO B has the VCO output; VCO A output normalled to VCO B CV input [M42F p.9].
- Each VCO has its own ADSR + VCA ("EG VCA 1/2"): HOLD keeps VCA open; LOOP / "self-generation"
  makes the EG an LFO (set A and R to minimum..9 o'clock) [M42F p.9]. EG out 0..+8 V [M42 p.4].
- VCO out -5..+5 V; dry out max 1 V [M42 p.4].

### 3.5 LFOs, generators and the "five LFOs" claim

- LFO A (slow) and LFO B (fast): waveform knob crossfades triangle and square (centre = mix);
  unipolar 0..+10 V [M42 p.10]. 42N adds a 3-position speed switch x1 / x6 / x10 [M42N p.10][R42N].
- [M42 p.3] lists "Five LFO generators" while the F/N pages say "Two LFOs and three square/pulse
  generators" [EP42N]. Best reading: LFO A, LFO B + the two Papa Srapa modulators + the sequencer
  PULSER = 5 periodic CV sources (UNCONFIRMED but consistent with the CV list). EG loop modes add two more.
- Rates: **UNCONFIRMED** (no source gives Hz). Estimates: LFO A about 0.02-5 Hz, LFO B about
  0.2-40 Hz; 42N speed switch multiplies those ranges (x6, x10; whether it speeds up or slows down
  is unclear, owners wished for slower LFOs [ET, Sep 2025]). Pulser about 0.3-30 Hz. Papa Srapa
  modulator about 0.1-30 Hz at x1, x10 above (reaches audio rate for FM).

### 3.6 Joystick

Two-axis, position-locking; two offset knobs and X/Y CV outs. Range -10..+10 V with 0 V at
centre; the offset shifts the window: left = -10..0 V (centre -5), middle = -5..+5 V, right =
0..+10 V [M42 p.10]. Model: `out = 5 V * j + offset`, j in [-1, 1], offset in [-5, +5] V.
42F/N: LED position indication [M42F p.10].

### 3.7 5-step sequencer

"Buchla inspired 5-stage sequential voltage source"; each step 0..+5 V; internal PULSER clock with
CLOCK OUT; EXT CLOCK input; per-step GATE switches (CV unaffected); STAGES 3/4/5 [M42 p.11]. CV out
0..+5 V, gate out 0..+10 V [M42 p.4].

### 3.8 Touch keyboard (digital, MPR121 capacitive, 16-bit DAC)

From [M42 p.13-20]:

- 12 capacitive plates, responds to skin area (not force). Outputs: V/OCT 0..+8 V (8 octaves),
  PRESSURE 0..+8 V, GATE LEFT (main, normalled to EG 1/2), GATE RIGHT 0..+10 V. Inputs: CLOCK
  (auto-switches to external), RESET (arpeggiator).
- Controls: two transpose buttons (octave range when quantised; offset 0.0025 V steps over 5 V when
  not), encoder, display. Hold a plate + turn the encoder to tune the plate.
- Behaviour: SINGLE (12 keys), TWIN (two 6-plate halves; pressure out becomes right-side V/oct),
  SPLIT (two halves with separate parameters).
- Mode: KEYBOARD / ARPEGGIATOR / SEQUENCER (16 steps, transposed by the active plate).
- Arpeggiator: HOLD, CLOCK mult/div, DIRECTION (forward, backward, ping-pong, random, ordered by plate
  number), VARIATION off/x1/x2/x3, INTERVAL 1-12 semitones, RHYTHM gate pattern with LENGTH 1-8.
- Sequencer: RUN free / keyboard-controlled, LENGTH 2-16, CLOCK, DIRECTION, EDITOR, CV OUTPUT
  continuous / gated, RHYTHM + LENGTH 1-8.
- Portamento 0-255 + LEGATO; Vibrato SPEED/DEPTH/DELAY 0-127 + pressure control.
- Pressure output: pressure / ASR / AD / LOOP (looped AD) / random; RISE and FALL 0-255.
- Quantiser: scale editor (all notes off = free microtonal), LOAD SCALE (Semitones, Ionian, Dorian,
  Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Blues major, Blues minor, Pentatonic major,
  Pentatonic minor, Folk, Japanese, Gamelan, Gypsy, Arabian, Flamenco, Whole tone), ROOT NOTE C..H.
- Internal clock 10-300 BPM; 4 presets A-D (all parameters except tempo).
- Calibration: V/oct and pressure at 0/2/5/8 V; DAC VREF (DAC8562/DAC8563 internal, AD5663
  external); pressure min/max; MPR121 charge/discharge and debounce; encoder direction [M42 p.20].
  F/N add touch threshold (default 650) and release threshold (690) [M42F p.19].
- Firmware: 2.0 (Mar 2024, serials 001-206, plate dropouts, hanging notes fixed) [ET]; v3 on 42F
  [SIG]. Owners report root-note quirks [ET, Mar 2024].

### 3.9 Mixer, filter, distortion

- Mixer: VOL + PAN for 8 voices, EXT AUDIO and PREAMP [M42 p.21] (manual typo "SOLAR 52").
  Pan law **UNCONFIRMED** (analog pan pot; estimate -3 dB centre).
- Filter: dual Polivoks 12 dB. MK1 uses Soviet UD12 chips (K140UD12 programmable op-amps), so the
  two channels differ slightly by design; "dirty and angry"; "with the resonance turned up, you won't
  lose bass frequencies" [M42 p.21]. FREQ, RES, CV with LINK [M42 p.21]. F/N: "new circuit ... more
  stable and does not require tuning, unlike the previous circuit on the classic UD12 chips", BP/LP
  mode (switching clicks), DIST (dry/distorted balance) and GAIN (amount) [M42F p.21, p.26]. 42N: CV L
  normalled to CV R; MOD L / MOD R CV amounts; LINK [M42N p.21]. Owners: linked mode "kills a lot of
  the juicy high-end" [ET, Feb 2026] (UNCONFIRMED cause).

### 3.10 Effector (digital, cartridges)

- Same circuit as the Elta CONSOLE pedal and SOLAR 50; one block per channel; X, Y, Z, BLEND and
  MASTER shared; X/Y/Z CV -10..+10 V [M42 p.22]. No VCA after the effector, so digital noise is
  audible at full MASTER [M42 p.26]. The two channels differ slightly ("live" stereo) [M42 p.26].
- Loading: insert card, choose channel with L/R fx, choose program with 1-2-3, press RESET L/R; a
  loaded program stays in memory until reloaded; removing the card then switching erases [M42 p.22].
  42N: flip 1-2-3 to load that channel [M42N p.22].
- DSP chip: **UNCONFIRMED**. Estimate: Spin Semiconductor FV-1 (EEPROM program cartridges, 3 pots,
  about 32 kHz, about 1 s delay memory, digital noise floor) based on the CONSOLE pedal lineage and
  the observed behaviour.

Cartridge programs [M42 p.23-24][M42F p.24]:

| Cartridge | Program 1 (X / Y / Z) | Program 2 | Program 3 |
|---|---|---|---|
| CATHEDRAL (reverbs) | Shimmer: octave up / octave down / decay | Oct-up delay: feedback / delay / reverb | Space reverb: feedback / delay / reverb |
| MAGIC (pitched delays) | Pitch delay: feedback / delay / pitch | Reverse pitch delay: feedback / delay / pitch | Bell pitch delay: feedback / delay / pitch |
| TIME (mod delays) | Delay reverb: feedback / delay / reverb | Delay chorus: feedback / delay / mod depth | Delay vibrato: feedback / delay-vibrato rate / mod depth |
| VIBROTREM | Tremolo: depth / rate / reverb | Vibrato: depth / rate / reverb | Chorus: depth / rate / reverb |
| FILTER | Auto wah: filter amount / envelope / reverb | HP/LP filter: HP cutoff / LP cutoff / resonance | Notch: cut 1 / cut 2 / resonance |
| VIBE | Phaser: depth / rate / reverb | Flanger: depth / rate / reverb | Resonance flanger: resonance / rate / mod depth |
| PITCH SHIFTER | SynthTaver: octave down / octave up / direct | Octaver: octave down / octave up / direct | Pitch harmonizer: pitch 1 / pitch 2 / voice mix |
| INFINITY (big ambient) | Resonance reverb: pre-delay / pre-delay mod / decay | O.D.D. oscillating dirty delay: feedback / delay / pitch | Resonance delay: feedback / delay / pitch |
| STRING RINGER | Synthetic ring: frequency / resonance / sub | Ring mod: frequency / rate / reverb | S&H ring mod: pitch speed / S&H rate / freq ring mod |
| SYNTEX-1 (bass synth) | Vibe synth: vibrato rate / resonance / sub | Pulse synth: tremolo rate / resonance / sub | Acid synth: tone / color / sub |
| DIGITAL (crusher) | Filter DAC: sample rate / cutoff / input gain | LFO DAC: sample rate / LFO speed / LFO amount | Envelope crusher: sample rate / envelope amount / input gain |
| GENERATOR (noise mini synth) | FM tone: pitch 1 / pitch 2 / FM 2-1 | Ramp: LFO rate / pitch / pitch mod +/- | Voice: cutoff / pitch / LP/HP |
| OCHRE (13th card, reverse delay) [M42F p.24][OCH] | One-shot long: delay time / feedback / trigger threshold | One-shot short: same | Free-run loop: delay time / feedback / delay mod LFO/RND |

### 3.11 Preamp and envelope follower

Built-in piezo under the keyboard turns the case into a microphone; EXT. SOURCE jack bypasses it;
1 MΩ input, up to 40 dB gain, clip LED; envelope follower with attack/release, CV out 0..+10 V and
gate detector out +8 V [M42 p.12]. At max gain the touch-keyboard scanning noise is audible
[M42 p.26].

### 3.12 Control voltage summary [M42 p.4]

| Output | Range |
|---|---|
| DRY V4/V5 | max 1 V audio |
| WET | max 2 V audio |
| VCO out | 10 V p-p (-5..+5 V) |
| EG | 0..+8 V |
| Drone env 1, 2, 3, 6, 7, 8 | -10..+10 V |
| LFOs | 0..+10 V |
| Pulser | -10..+10 V |
| Env follower CV / gate | 0..+10 V / 0..+8 V |
| Joystick | -10..+10 V (window by offset) |
| S&H | -5..+5 V |
| V3/V5 modulator (sic; V3/V8) | 0..+12 V |
| 5-step CV / gate | 0..+5 V / 0..+10 V |
| Keyboard V/oct / pressure / gates | 0..+8 V / 0..+8 V / 0..+10 V [M42 p.13] |

MIDI: none. CV/gate: as above; all patch points are semi-modular with internal normals (keyboard ->
VCOs and EGs; VCO1 -> VCO2 FM; noise -> S&H in; drone keys -> drone gates).

---

## 4. Behaviour details for DSP modelling

| Topic | What is known | Model guidance |
|---|---|---|
| Drone oscillator waveform | Sawtooth from a negistor relaxation oscillator [M42F p.7] | Capacitor charges through the tune resistance toward the supply, discharges abruptly at breakdown: rising ramp is an exponential segment (slightly concave), fall is near-instant. Pitch is roughly proportional to 1/R, so the tune pot is very non-linear at the top of its travel (matches the "coarse resolution" complaint [GS]) |
| Drone tune ranges | Table in 3.2 | Map each TUNE knob over its manual range with a hyperbolic-ish taper; add a fine-tune option in the plugin |
| VOLT knob | Transposes down; FM past half [M42 p.7] | Model as the shared oscillator supply voltage: lowering it stretches the charge time (lower pitch, waveform shape changes); add a shared supply impedance so each oscillator's discharge spike couples into the others, rising with VOLT past 50% (gives the described cross-modulation) |
| Drift | Present, temperature dependent [M42F p.7] | Per-oscillator slow random walk (about ±3-8 cents, 1/f), plus a warm-up offset that decays over the first minutes; user control "drift amount" |
| Photo orb | LDR-type response to light and an internal red LED [M42 p.7, p.10] | LDR model: R = R_dark * (1 + k*L)^(-γ), γ ≈ 0.7, asymmetric response (on about 5-20 ms, off about 100 ms to 1 s). MOD inserts it into the timing network (pitch rises with light). Inputs: ambient-light parameter, mouse "hand" XY over the orb, CV into the LED (unipolar) |
| Papa Srapa voice | Two Schmitt-trigger RC oscillators [M42 p.8] | CD40106-style: capacitor triangle-ish (exponential segments) and square output; FM by switching/summing the modulator into the tone oscillator's charging current (depth = fm amt and divider); AM = gating the tone with the modulator square. Keep it raw and aliased-but-bandlimited (polyBLEP on edges) |
| VCO | AS3340 triangle core [M42 p.9] | Exponential converter with small HF tracking error, triangle core, standard shapers; sine from a triangle shaper (soft clip); morph waveforms: saw <-> inverted saw is likely a variable-slope ramp passing through triangle (UNCONFIRMED), sine <-> triangle a shaper crossfade. Linear FM and hard sync need 2x oversampling or BLEP-based sync |
| Envelopes | AR (drones), ADSR + hold + loop (VCOs) | RC-style exponential segments. Times UNCONFIRMED: estimate attack 1 ms-5 s, release 5 ms-8 s; owners want longer drone releases [ET, Sep 2024] |
| Polivoks filter | 12 dB/oct, K140UD12 integrators, dirty, bass kept with resonance [M42 p.21] | See section 8.3 |
| Distortion (F/N) | Stereo, DIST = dry/wet, GAIN = drive, after filter [M42F p.21] | Asymmetric diode/op-amp soft clipper with pre/de-emphasis; level gets loud, user must lower master [M42F p.21] |
| Mixer summing | Owners praise "the way all the voices are summed, the overdrive" [MX][ET, Jul 2024] | Gentle bus saturation (tanh/ADAA) on the mix bus before the filters |
| Stereo width | Cannot be narrowed before effects [ET, Mar 2026] | Add a width control as an extension (off by default for faithfulness) |
| Effector noise | Digital noise at master max, no output VCA [M42 p.26] | Optional "vintage noise" amount |
| L/R mismatch | Filters and effector channels differ slightly [M42 p.26] | Per-channel component offsets (cutoff ±3%, resonance ±5%, delay times ±1%) |
| Character | "Shrill and harsh" demos, Polivoks blamed; "pristine in higher registers"; raw VCO "woody bagpipe", "oboe/bassoon" sweet spot [ET Jan 2024, Feb 2025] | Calibrate against official demos (section 5) |

---

## 5. Sound character as described by reviewers

- Elta: "make your sound dirty and angry" (filter) [M42 p.21]; designed so that you can "create
  full-blown drone or ambient tracks on it without multitrack recording" [M42 p.3].
- Perfect Circuit (42N review): "part science experiment, part musical puzzler, part grumbling sound
  wizard, and part cavernous sonic sculpture"; "fiercely microtonal"; can "pull itself together in a
  slow, lumbering fashion from fog to focus"; Papa Srapa voices "quickly descend into chaotic sirens,
  squirts, and streams of beeps"; the effector is "an integral part of the instrument" [PC].
- Juno (42F): designed for drones and soundscapes "but also capable of being tamed into more
  conventional melodic and harmonic territory" [JU, search snippet].
- Gearnews: "It's not melodic in the traditional sense"; "it sounds flipping amazing!" [GN].
- Synth Anatomy: "much more interesting as an instrument" than the SOLAR 50; focus on "epic analog
  drones" [SA].
- Owners: "a really big sawtooth swarm" [ET, Jan 2024]; flute-like VCO leads (Benn Jordan demo)
  [ET, Jan 2024]; "very vintage tone. The VCO A/B alone as a synth is really, really great" [ET, Jul
  2025]; "with a few patch cables this thing becomes a techno monster" [ET, Mar 2024]; hard-panned
  VCOs as a "binaural synth" through the filter [ET, Oct 2024]; used for Hoover/gabber and industrial
  [ET, 2026].

Demos (read-only references):
- Official: "SOLAR 42 | DRONE AMBIENT MACHINE" https://www.youtube.com/watch?v=LfCUgCI-Zl0;
  "SOUND DEMO 1" https://www.youtube.com/watch?v=qcjaBlqCHcA; "SOUND DEMO 2"
  https://www.youtube.com/watch?v=LRubpHnBCvo [ET, Oct 2023].
- Benn Jordan, "A Box of Ambient [Solar 42]" (embedded at [MX], May 2024); Bobeats review (mentioned
  in [ET], URL not found).
- "Is This The Best Ambient Synth EVER MADE??!! (Solar 42 f)" https://www.youtube.com/watch?v=AdygIkwh9i4.
- Typical uses: cinematic ambient, drones, microtonal chords, sci-fi effects (sirens, birds, surf),
  Berlin-school sequences, piezo-driven percussion and envelope-follower "303 stuff" [ET, Dec 2023].

---

## 6. Community feedback

**Praised**
- Build and finish: "Built like a tank", knob feel and paint "phenomenal" [ET, Feb/Mar 2024].
- Immersive, inspiring; "a Canvas that calls you" [ET, Dec 2024, Mar 2026].
- Light orbs: "perform like a more gentle Roland D-Beam" [ET, Mar 2024].
- Central mixer "freaking powerful for sound sculpting" [ET, Mar 2024].
- Pressure-sensitive touch keyboard with arp/seq [ET, Dec 2023, Dec 2024].
- FX cards, especially CATHEDRAL shimmer and INFINITY [ET, Mar 2024].
- Morphing VCO waveforms [ET, Jan 2025]; responsive maker support [ET].

**Wished for / criticised** (inspiration for AT42 extensions)
- No MIDI (clock and notes) [ET, Sep 2025].
- Drone tuning is hard and drifts; wish for stepped/chromatic tuning option [ET, Oct 2024][GS].
- Longer release times on drone voices; CV control of panning; keyboard settings recall on power
  cycle; utilities (attenuators, mults, add/union) [ET, Sep 2024].
- Few audio outputs (only VCO dry outs) [PC]; stereo width cannot be narrowed before effects
  [ET, Mar 2026].
- LFOs not slow enough (fixed by 42N switch) [ET, Sep 2025]; cartridge loading confusing [ET, Jun 2025].
- Harsh/shrill at times (Polivoks) [ET, Jan 2024]; one owner found the melodic oscillators "weak
  sauce" [ET, Jan 2025].
- Firmware update needs removing ten M2 screws [ET, Mar 2024]; early S&H resistor fault (serials
  002-152) [ET, Mar 2024].
- Rapid revisions (42, 42F, 42N in about two years) put some buyers off [ET, Jun 2025].

Prior art: a VCV Rack patch emulation by Dave Benham covers all 8 voices with linear FM (noted as
inaccurate for drones), mixer, sequencer, LFOs, filters; no keyboard, joystick, piezo [VCV].

---

## 7. Reference photos and licensing

- Searches of Wikimedia Commons (`Solar 42`, `Elta Music`, `Elta Solar`, `Superbooth 2023`) and
  Openverse (indexes Flickr CC) on 2026-10-16 returned **no free-licensed photo** of any SOLAR model.
  Nothing was downloaded. See `ref/CREDITS.md`.
- Official, copyrighted, read-only references (do not ship or trace for distribution):
  - Panel renders [R42] [R42F] [R42N] (URLs in the source table) and black MK1 render
    https://www.eltamusic.com/_files/ugd/12d408_e4f30746058c458ab6bacb97f7f642a7.pdf
  - Product photos: https://static.wixstatic.com/media/12d408_0ced3490650149d5a6faaae623122084~mv2.jpg,
    https://static.wixstatic.com/media/12d408_4b3962bd34c249e6b88050e5f2bb251b~mv2.jpg,
    https://static.wixstatic.com/media/12d408_6727249cbab04508803c8c8c9150e074~mv2.jpg,
    https://static.wixstatic.com/media/12d408_b4f662089247441d804d6f1f35a03da3~mv2.jpg [EP42];
    42N/42F: https://static.wixstatic.com/media/64d4dc_72128496e6f64d9a8042d161abc5fa6e~mv2.jpg and
    the gallery IDs listed on [EP42N].
  - Colour renders posted on [ET] p.1 (cream, black, maize yellow, green brown).

---

## 8. Recommended DSP models

### 8.1 Engine layout and budget

- Run at the host rate with **2x oversampling** for the VCOs (FM/sync), Polivoks filters and
  distortion; drones and Papa Srapa voices are fine at 1x with polyBLEP/polyBLAMP.
- Voice count is fixed (no polyphony allocation): 20 drone oscillators, 2 Papa Srapa voices,
  2 VCOs (+2 sub), 2 filters, 2 effect engines. Estimated CPU on Apple Silicon: under 8% of one core.
- Patch bay: implement every jack as a CV/audio node with the hardware normals; a cable list is
  state (cables as presets). Scale internal CV in volts so ranges in 3.12 apply literally.

### 8.2 Oscillators

1. **Negistor drone oscillator** (per oscillator): phase accumulator whose ramp follows
   `v(t) = Vs - (Vs - Vlow) * exp(-t / RC)` until `v = Vbr`, then resets. Frequency
   `f = 1 / (RC * ln((Vs - Vlow) / (Vs - Vbr)))`. Parameters: R from the TUNE pot (range per
   oscillator from the table), Vs from VOLT (and supply sag), Vbr about 8-9 V, Vlow about 1-2 V
   (UNCONFIRMED; tune by ear to the official demos). Precompute the normalised ramp shape in a
   table indexed by phase and the ratio `(Vs - Vbr) / (Vs - Vlow)`; apply polyBLEP at the reset.
   Supply coupling: `Vs_eff = Vs - Rsupply * sum(discharge pulses)` filtered by a one-pole; scale
   Rsupply up past VOLT = 0.5 to create the FM effect. MOD + photo orb: add `R_ldr` in parallel with
   the tune resistance.
2. **LDR (photo orb)**: light `L` = ambient + hand + LED(CV, unipolar, LED threshold about 1.6 V);
   `R_ldr = R_dark / (1 + k*L)^γ` with separate rise/fall smoothing (attack 10 ms, decay 300 ms).
3. **Schmitt-trigger pair (Papa Srapa)**: model both as RC relaxation oscillators with hysteresis
   thresholds (1/3, 2/3 Vdd); tone oscillator charging current modulated by the modulator square
   (FM, depth from fm amt * divider), output gated by the modulator (AM); noise added before the VCA;
   per-voice S&H sampling the voice noise on rising clock edges with a short droop-free hold.
4. **AS3340 VCO**: standard BLEP triangle-core design: exponential converter with optional
   high-frequency droop and a tiny thermal drift, triangle -> saw/pulse/sine shapers, hard sync with
   BLEP, linear FM through the exponential converter output (true linear). Morph 1: variable-slope
   ramp (saw -> triangle -> inverted saw) with polyBLAMP; morph 2: crossfade sine <-> triangle. Sub
   oscillator (F/N): divide-by-2 square from the core.

### 8.3 Polivoks filter

- Topology: 2-pole state-variable filter whose two integrators are K140UD12 programmable op-amps.
  The set current controls both the integrator gain (cutoff) and the **slew-rate limit**, so large
  signals become rate-limited (the "dirty" character); resonance is fed back through a limiting
  network. Elta adds compensation so the bass does not drop with resonance [M42 p.21].
  (Topology from public Polivoks literature; the slew-limiting detail is a modelling hypothesis,
  UNCONFIRMED for Elta's MK1/F/N circuits.)
- Implementation: TPT/ZDF SVF (Zavalishin) with each integrator input passed through a smooth rate
  limiter `y' = SR * tanh(g * x / SR)` where `SR ∝ g` (both scale with the set current);
  resonance feedback through `tanh` or an antiparallel-diode function; bass compensation by adding
  `k_res * LP-filtered input` to the output. Solve per sample with one Newton iteration, 2x
  oversampled. LP and BP outputs from the same core (BP = 6 dB/oct slopes) for F/N.
- Channel mismatch: cutoff ±3%, resonance ±5% between L and R (MK1 UD12 spread); smaller for F/N.
- Distortion (F/N): `y = mix(x, clip(gain * x), DIST)` with asymmetric soft clip and output level
  that rises with GAIN (faithful), plus an optional auto-gain extension.

### 8.4 Envelopes, VCAs, CV sources

- AR and ADSR: analog RC curves (attack aims at 1.2x target and stops at 1.0; decay/release
  exponential). Loop mode retriggers at end of decay. Drone VCA: exponential-ish law with slight
  bleed off (none on MK1 per [M42 p.26]).
- LFOs: triangle core with square comparator; output `mix(tri, square, wave)` mapped to 0..+10 V;
  42N speed switch as range multipliers.
- 5-step sequencer, pulser (square with fast edges), S&H, white noise (two independent generators),
  joystick formula from 3.6, envelope follower (full-wave rectifier, attack/release one-pole, gate
  comparator with hysteresis).
- Touch keyboard: implement as a software module that reproduces the firmware features in 3.8
  (behaviour, modes, arpeggiator, 16-step sequencer, quantiser scales, portamento 0-255, vibrato,
  pressure modes, 4 presets, 10-300 BPM clock). Drive it from MIDI notes (plates mapped to MIDI
  notes or to the on-screen plates) and MIDI aftertouch/MPE pressure.

### 8.5 Effector

- Emulate an FV-1-class engine: 32.768 kHz internal rate with resampling, 24-bit fixed-point style
  accumulation, delay memory limited to about 1 s, gentle anti-alias/reconstruction low-pass about
  14-15 kHz, three parameters X/Y/Z (knob + CV -10..+10 V, clamped). Write 13 cartridges x 3
  programs from the table in 3.10 with original algorithms (Dattorro/FDN reverbs, pitch shifters with
  two crossfaded delay taps, BBD-free chorus/flanger, ring mods, crushers). Two independent instances
  (L, R) with slight parameter offsets; BLEND as dry/wet; optional noise floor at high MASTER.
  The FV-1 identification is UNCONFIRMED; the algorithms are AT42's own.

### 8.6 Extension features (from section 6, all optional and off by default)

MIDI note/clock input and CC mapping for every knob; per-voice and per-section audio outputs
(multi-out plugin); chromatic/quantised tuning mode and fine-tune for drone oscillators;
extended release ranges; CV/automation for pan; stereo width before effects; tempo-synced LFOs
and slower ranges; attenuator/mult utility block; one-click cartridge loading for both channels;
drift and orb-light amount controls; selectable panel revision (42 / 42F / 42N behaviours).
