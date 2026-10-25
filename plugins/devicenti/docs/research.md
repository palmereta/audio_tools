# DeVicenti: research, design decisions and test plan

DeVicenti is an original FM groovebox: four tracks, each a four-operator FM voice (TONE), a drum machine of eight FM
lanes (KIT) or a MIDI sender (MIDI), a 64-step sequencer with step locks and conditions, per-track inserts, two send
effects and a master bus. It is aimed at downtempo, trip hop, acid jazz, ambient and dark ambient, with heavy, hard
synths (the user's direction of 2026-10-24). This file is the research behind it; the user interface, parameter
names, presets and manual never name the products cited here.

## 1. The reference: FM grooveboxes (checked 2026-10-24)

### 1.1 The reference instrument (2018), its manual of 2026
- 8 voices, 4 synth tracks and 4 MIDI tracks.
- FM engine: four operators grouped as C (carrier), A, and B1 + B2 driven together with one revolving ratio control;
  eight algorithms with two carrier outputs X and Y and a MIX crossfade; ratios 0.25-16 with offsets on a second page.
- HARM (bipolar additive series on C or on A/B1), DTUN on A and B2, one feedback per algorithm.
- Only two modulation envelopes (A, and B shared by B1 and B2), attack-decay-end or gated, with delay and reset.
- Signal path: FM → overdrive → base-width filter (6 dB high-pass and low-pass) → multimode filter → amp ADSR.
- Two LFOs per voice; sends to chorus, delay, reverb; overdrive on the master.
- Sequencer: parameter locks, sound locks, trig conditions (FILL, PRE, NEI, 1ST, X %, A:B and negations), fill
  mode, micro timing, retrigs, per-track length and scale, keyboard scales, a three-note chord key.

### 1.2 Its successor (2024)
- 16 voices, 16 tracks (synth or MIDI, 16 notes and 16 CCs per step on MIDI tracks).
- Machines: the FM tone of the first model, an FM drum (three-operator body into a wavefolder plus a noise and
  sampled-attack transient), a wavetable voice and a swarm voice.
- Six filter machines, a base-width filter before or after; three LFOs; four modulation sources with four
  destinations each.
- 128 steps, probability separate from conditions, Euclidean generator, per-track length and time scale, page loop,
  note edit, arpeggiator, chord mode with chord memory, keyboard mode with 36 scales, song mode, perform kits.
- A saturating delay, a long reverb and a chorus as sends; overdrive and compressor on the master.
- USD 999-1149; hardware only (the maker's remote software is not a plugin).

### 1.3 What users say
- Love: the sound (FM made approachable by a subtractive filter); the sequencer (locks, conditions, fills); deep
  memory. MusicTech 9/10.
- Complaints: menu diving; live control (no MIDI feedback or relative CC, controllers jump); the grouped B
  operator with a shared envelope ("the only thing I dislike"); voice-stealing clicks on the 8-voice model; no direct
  pattern jump; no polyphonic aftertouch; no 6-operator mode; USB-B and a long wait for the remote software.

Sources: the manufacturer's manual and product page, two magazine reviews, a retailer's user reviews and the maker's
user forum (listed in the project's private notes; this file does not name the products).

Plugin landscape: Arturia DX7 V, Native Instruments FM8, Dexed (free DX7 model, no sequencer), KORG opsix native,
Ableton Operator (no sequencer). None is a free, open (AGPL), Linux-capable FM synth with a built-in four-track step
sequencer, locks and conditions.

### 1.4 What DeVicenti does not copy
The reference's algorithm diagrams, its C/A/B1/B2 grouping, the revolving B ratio, the X/Y MIX, the bipolar HARM mapping,
parameter names, screen layouts and the PCM attack samples. The UI and manual say **step lock**, **step condition**
and **patch swap**; no product names.

## 2. The genres (audiotools-private/vicenti/genre-recipes.md)

The user asked for downtempo, trip hop, acid jazz, ambient and dark ambient, with heavy, hard synths. The recipes
file (written for this plugin from genre references, practice and Chowning 1973) gives drum grids, harmony,
FM recipes, effect chains and demo blueprints per genre, and ranks the engine requirements (its §9). How each was
answered in phase 1:

| Requirement (recipes §9) | DeVicenti |
|---|---|
| Clean mono phase-coherent sub, out of drive and inserts, mono below 120 Hz | SUB layer at the host rate, sine or triangle, −1/−2 oct, follows the amp envelope, delayed by the voice latency so it is phase-locked to the FM layer; added after the insert and the tape; low-end mono (side high-passed at 120 Hz, 2 poles) |
| Asymmetric saturation + fold + crush, no aliasing at the far end | DRIVE 0-36 dB asymmetric cubic soft clip and FOLD (triangle folder), both with first-order ADAA inside a local 2x→4x→2x stage (33-tap half-bands); CRUSH (bits and sample-and-hold); DC blocker after |
| Saturating LP24 that keeps the bass at high resonance | TPT ladder solved in closed form, tanh on the input, soft per-stage saturation, ±2.5 % stage spread per voice, bass compensation 0-1 (input gain 1 + c·k) |
| Tape/AGE depth, vinyl crackle, 12-bit sampler colour with aliasing switchable | AGE per voice (pitch drift, operator detune drift, cutoff and level spread) and per track (wow, flutter, drift, HF loss, saturation, hiss); BITCRUSH/LO-FI insert (16→4 bits, 44.1→2.7 kHz sample-and-hold tied to the position, anti-alias filters on/off); master TAPE with hiss, surface noise and crackle |
| Long dark modulated reverb with freeze and shimmer; a real spring | VOID (Halation's FDN, 4-30 s, dark, modulated), HALL, PLATE (Dattorro), SPRING (three dispersive loops of 24 first-order all-passes tuned by frequency, decay control); FREEZE (the network freezes, or a second network catches the send for SPRING/PLATE); SHIMMER (octave-up two-head shifter in the loop) |
| Dub delay: saturating band-passed feedback, self-oscillation, click-free throws | DUB mode: band-pass in the loop, tanh, feedback to 1.12, freeze; feedback smoothed (30 ms), send levels smoothed (8 ms); feedback, time and freeze lockable per step (global locks) |
| Comb/resonator tuned to a note, with a safe limiter | COMB/RESONATOR insert tuned to the pattern root ±24 st, feedback to 0.95, damping; an RMS limiter in the loop pulls the gain down above −6 dBFS |
| Unison with detune and spread; polymeter and probability, also on KIT lanes | Unison 1-4 (detune, spread, start-phase offsets); per-track and per-lane lengths, per-track speed; probability and conditions per step on every lane |
| Slow LFOs (0.01 Hz-16 bars), envelopes to 10 s, AD loop | LFO 0.01-40 Hz or synced 1/64-16 bars, free/trig/hold/one-shot, fade in/out; envelopes to 20 s (30 s decay/release), curve, delay, AD loop |
| KIT with weight: sub kick with sweep, impacts, metal, scrape, lane fold/crush/sends/decay | Every lane is a full voice: fixed-frequency operators, pitch sweep (lockable), noise with its own filter and decay, fold, crush, sends, decays to 30 s |
| Master TAPE, GLUE with DUCK, soft LIMIT | TAPE; GLUE (Lacquer's compressor, auto release); DUCK from the KIT kick lane with a 90 Hz sidechain high-pass, applied to the TONE tracks; LIMIT (soft, no look-ahead) and a soft ceiling always on |
| Per-genre loudness for the demos | −16 LUFS; −20 LUFS for ambient and dark ambient |
| Noise source and pitch envelope on TONE | Noise (LP/BP/HP, own decay or following the amp), pitch envelope on every voice |
| Chord types: sus, add9, quartal, cluster, shell, b9/#11, drone, humanise | Chord step types diatonic (2-7 notes), sus2, sus4, add9, quartal, cluster, open fifths, shell, power; flags root bass, b9, #11, b5; voicings close, drop-2, drop-3, spread, no root; inversion; strum up/down/alternate; humanise (velocity, timing); drone hold |

## 3. Design decisions

- **One sound table.** A sound is a flat array of ~200 floats (`Source/dsp/SoundParams.h`); step locks, LFO and
  matrix destinations, presets and patch swap all address it by index or key.
- **Four equal operators**, each with its own ADSR, feedback, harmonics morph, velocity and key scaling: the answer
  to the most specific complaint about the reference.
- **2x oversampling** of the voice with a 97-tap half-band decimator per voice (exactly 24 samples of delay), plus
  index limits per note: Carson (f_d + (I+1)·f_m < fs2/2) and a Bessel-tail limit (the first sideband that would fold
  back after the 2x stage must be of order > 1.5·I + 6). Feedback is reduced above 1.2 kHz. Measured (devicentitest
  T2): feedback saw and FM 1:3.5 at the metric floor from C2 to C7; drive at the far end with fold, −38 dB in the bass
  register (an honest number: extreme drive on high notes still aliases, −25 dB at C7).
- **Control rate** of 16 host samples aligned to the absolute sample count, per-sample interpolation inside: any
  block size gives the same output (T4: bit-exact for blocks of 1-2048 and random sizes).
- **Voice pool** of 16 sounding voices in 24 slots: a stolen voice fades for 5 ms (raised cosine) and then flushes
  its decimator while the new note starts in another slot.
- **Host parameters (64)**: per track cutoff, resonance, filter envelope, drive, AGE, delay and reverb sends, level,
  the four operator levels, insert amount; global swing, master, delay time and feedback, reverb size and decay, TAPE,
  GLUE, DUCK, LIMIT, tempo and pattern. On a KIT track the per-track values act on the kit bus (low-pass, decay
  scale, drive, lane-group levels). The plan's "LFO depth of the focused track" pair was replaced by tempo and
  pattern: automating a value that follows the UI focus would be ambiguous.
- **Effects reuse:** Halation's network and shimmer, the shared Dattorro plate, Lacquer's compressor. The spring
  is new (dispersion chain + loops with decay); the tape stages are cheap closed-form versions of Tarnish's transport
  idea (wow/flutter as functions of the position).
- **Prototype parity.** The Python prototype's patches render through the C++ engine within 1 dB in level and
  2 % in envelope time (`tools/proto_compare.py`); the filters use the prototype's resonance curves.
- **MIDI**: channels 1-4 or OMNI (default: the focused track, track 2 on a new instance because track 1 of the
  factory projects is the KIT), KIT GM-like note map (36 kick, 38 snare, 39 clap, 42/46 hats, 45/50 toms, 37 rim,
  70 shaker, 49 impact), bend ±1-24, wheel, aftertouch, poly pressure, breath, expression, sustain. Fixed CC map for
  every panel knob and its FUNC layer (`Source/PanelMap.h`), learn, relative CC (two's complement or offset 64),
  CC feedback out, program change → pattern or preset, MIDI clock in and out with start/stop in the standalone, live
  recording into steps (and knob moves into locks). MIDI out: MIDI-machine tracks (notes, chords, locks as CC,
  conditions resolved), doubling of TONE/KIT tracks, thru; all delayed by the latency.
- **Harmony follow**: a MIDI channel (setting) whose held notes set the chord root and major/minor scale for chord
  mode. Following an Orrery constellation in-process would need Orrery to publish its harmony (its `MainBus` carries
  controls and hashes, not the chord); deferred, see §5.

## 4. Test plan and results (phase 1)

| Tool | What | Result |
|---|---|---|
| `devicentitest` T1 | tuning per ratio 0.5-16, fixed mode, tune/fine | ≤ 0.01 cent |
| T2 | aliasing at 2x: sine floor, feedback saw, FM 1:3.5, drive 0.6, drive 1 + fold 0.5 | at the floor (−88…−101 dB); drive+fold −38 dB (C2-C4) |
| T3 | steal, KIT choke, legato glide, note start through drive+fold, kill | all below the sine \|d²\| bound; no onset spike |
| T4 | determinism with blocks 32/64/113/997/2048 and random sizes | bit-exact |
| T5 | reset() and re-prepare = fresh instance | ≤ 3e-7 |
| T6 | 24 random sounds: finite, DC, denormals | pass |
| T7 | step timing at 120 BPM, random blocks, host rounding gap (T2b), micro timing, retrig ramp, swing | every step on its sample |
| T8 | A:B, PRE/!PRE/NEI chains, FILL, 1ST, BAR, LAST, probability determinism | pass |
| T9 | chord types, voicings, inversion, flags, chord memory quantised to the scale, strum | pass |
| T10 | MIDI channels, OMNI, KIT map, bend, sustain | pass |
| T11 | MIDI machine: notes, chord, lock → CC, balanced on/off; doubling | pass |
| T12 | CPU: dense 4-track project; 16 voices STACK + feedback, LP24, AGE 1 | see the report; ≤ 10 % target met |
| T13 | latency and sub alignment | 200 samples at 48 kHz; aligned |
| `devicentirender --midi-test` | CC map (absolute, relative), CC to sound parameters, per-channel mode, learn, program change, CC feedback, MIDI-out latency, MIDI machine + thru, standalone clock out and in, bypass, direct jump, controller notes | 13/13 |
| `devicentirender --state-test` | state byte-stable, new instance = program 0, restored = same sound | pass |
| `athosttest` (VST3) | the standard host battery | 0 FAIL |
| `tools/proto_compare.py` | prototype parity | within 1 dB / 2 % |

## 5. Deferred and open

- Orrery-style per-track virtual MIDI ports and in-process harmony follow from an Orrery Main (needs a small
  publication of the current chord in Orrery's bus).
- The full UI (phase 2): the unit in 3/4 perspective, the display pages and the "back" pages; grid and step
  recording, copy/paste, Euclidean fill and song editing live in it (the engine functions exist).
- MPE per-note slide (pressure is a modulation source).
- auval needs the component installed in the real `~/Library` (not done in phase 1 by rule); the AU was tested
  in-process with athosttest.
