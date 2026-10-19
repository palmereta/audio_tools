# Plumb: real-time pitch correction with very low latency (research)

Compiled 2026-10-19 for a new Audio Tools product: a free, open-source (AGPL-3.0) real-time pitch corrector
for monophonic voice and instruments, built with JUCE 9 (the repository pins 9.0.4) as VST3 / AU /
standalone for macOS and Windows. The request was "a real-time autotune with extremely low latency, based on
Melodyne 5 but better, easy to set up but with advanced controls too; the focus is low latency". The working
name below is "the plugin"; §9 proposes names. It covers the market and what users say (§1), pitch detection
(§2), pitch shifting (§3), the correction logic (§4), the live note view and a Melodyne-style capture mode (§5),
latency reporting and host behaviour (§6), tests (§7), the visual direction (§8), names (§9), reuse of the
repository's code (§10) and the decisions left to make (§11).

Conventions (the same as `plugins/tarnish/docs/research.md` and `plugins/at303/docs/research.md`):
- Every fact carries its source inline: a URL (all accessed 2026-10-19) or a paper with venue and year. Where
  only a review, a vendor blog, a forum post or a search-engine summary was available, the text says so.
- **(inferred)** marks my own conclusions, calculations or design proposals. Values marked "calculated"
  follow from a formula in the text. §2.6, §3.6, §4 and §5-§8 are design proposals throughout; their numbers
  are starting values to tune with the tests of §7.
- Academic references marked **(from memory, not re-verified today)** were not fetched in this session; the
  bibliographic data should be checked before they are quoted in the manual.
- Forum sweeps were thin: search engines returned mostly vendor roundups for Reddit/Gearspace queries. User
  opinion below comes from press reviews, vendor documentation and the few forum threads that surfaced; §11
  lists the follow-up sweeps.
- Product names (Melodyne, Auto-Tune/AutoTune, Waves Tune, Nectar, Graillon, Little AlterBoy, ReaTune and
  the others) are trademarks of their owners and appear here only for research. **The product shows no brand
  names, logos or other products' trade dress**, and its feature names are generic ("retune speed", "flex",
  "throat"), never a competitor's feature name. "Auto-Tune" is a trademark: the product never calls itself
  "autotune" (the folder name `plugins/autotune` is internal and should be renamed with the product name).
- Nothing here copies proprietary text or code. The GPL references (zita-at1, x42 fat1, Autotalent,
  TalentedHack) are studied through their published documentation, not their source, the same rule
  Tarnish used for Chow Tape Model.

---

## 0. Summary of recommendations

**Architecture** (§3.6, inferred). One engine, three latency modes. The audio path is a **source-filter
splice resampler**: a short-time LPC inverse filter whitens the voice into its excitation (the glottal pulse
train), the excitation is pitch-shifted by a read pointer that runs at the correction ratio and jumps by
exactly one local period when it drifts too far (a repeated or dropped cycle, refined by a small waveform
similarity search and placed between glottal pulses), and the all-pole synthesis filter puts the original
formants back. Formants are therefore preserved at every shift without any frequency-domain block, and a
formant shift ("throat") is a change of the synthesis envelope. A side-chain detector (YIN-style cumulative
normalised difference, decimated to ~12 kHz, incremental, with a voice-range preset, octave guards and a
voicing decision) drives a correction stage that separates each note into **centre, slow drift and
vibrato**, retunes only the centre by default, and offers the classic hard-tune behaviour at retune speed 0.
The same engine, with more lookahead, gives the higher-quality mode; a **capture mode** records a phrase,
analyses it offline into editable note blobs and plays it back corrected in place of the live input.

**Latency budget** (calculated; the engine is the same in every mode, only the lookahead changes; the
multiplier is 1x at 44.1/48 kHz, 2x at 88.2/96 kHz, 4x at 176.4/192 kHz):

| Mode | Use | 44.1 kHz | 48 kHz | 96 kHz | What the lookahead buys |
|---|---|---|---|---|---|
| **Live** | in-ear monitoring, stage, hard-tune effect | 32 samples, 0.73 ms | 32 samples, 0.67 ms | 64 samples, 0.67 ms | nothing beyond the shifter margin: detection from the past only |
| **Tight** (default) | tracking with correction on | 128 samples, 2.90 ms | 128 samples, 2.67 ms | 256 samples, 2.67 ms | splices planned on future glottal pulses, sibilant onsets seen before they are shifted, onsets of high voices corrected from their first stable period |
| **Studio** | mixing, natural correction, capture playback | 1024 samples, 23.2 ms | 1024 samples, 21.3 ms | 2048 samples, 21.3 ms | a centred detection window for every voice range, a short Viterbi pass over pitch candidates (no octave slips), full-period onset correction |

On top of the reported latency, while it is shifting the engine's content delay varies inside one pitch
period (it is what repeating and dropping cycles means; §3.4): up to ~4.5 ms at 220 Hz and ~12.5 ms at
80 Hz above the reported minimum. The expired Auto-Tune patent describes the same cycle-splicing approach
(§1.3), so its Classic mode very likely shares this property (inferred), and Waves Tune Real-Time's reviewers describe its
latency as "dynamic, 0 to 4 ms, longer for low notes" (§1.4). The **correction lag** (how long after a note
starts the correction locks) is separate from the audio latency: in Live it is about 1.5 longest periods of
the voice preset at onsets (Low ~23 ms, Mid ~19 ms, High ~9 ms) and about one local period for pitch changes
within a phrase; Tight subtracts 2.7 ms; Studio removes it for every preset (§2.6).

For comparison: AutoTune 2026 measured 0.8 ms (Classic) and 2.3 ms (Modern) in its low-latency setting and
58 ms in its highest-quality setting (Sound On Sound, January 2026); zita-at1 runs at ~21 ms, or ~10 ms in its
low-latency switch; Autotalent's own page gives 43 ms at 48 kHz as its theoretical minimum (§1).

**v1.0 feature list** (§4, §5, §8).
- *Easy (front page)*: key and scale with an **auto-key "Listen"** button; voice preset (Low / Mid / High /
  Instrument); **Amount** (0-100 %); **Speed** (retune, 0-400 ms); **Style** macro (Natural to Hard: sets
  vibrato keep, humanize, flex and transition together); latency mode (Live / Tight / Studio, with the
  number shown in ms); the **live note view** (scrolling piano roll with detected pitch, target notes as
  blobs and the corrected line); cents meter; bypass with level-matched compare.
- *Advanced*: transition (glide between targets), humanize (slower retune on sustained notes), flex
  (correct only within a window around the target), hysteresis, vibrato keep and drift keep (separately),
  note-centre time constant; scale editor with per-note snap/skip/remove and per-degree amount and speed;
  chord targets; MIDI target modes (scale from held notes, note lock with pitch bend, channel); transpose
  (±24 semitones and cents); formant preservation on/off, formant amount, **throat** (vocal-tract length);
  synthetic vibrato (rate, depth, delay, onset); detector range, threshold, octave lock, sibilant and breath
  handling; reference A (415-466 Hz); output gain and dry/wet; the **capture** page (record or retro-capture a
  phrase, edit note blobs: pitch, centre/drift/vibrato amounts, transition, split/merge; plays back in place
  of the input when the host transport crosses the region).
- *Not in v1.0* (v1.1 or later): detected pitch to MIDI out, harmony/doubler voices, timing edits in capture,
  ARA 2 integration, polyphonic anything (§1.2, §5.4).

**Tests** (§7). 20 automated tests: reported against measured latency (impulse and correlation), a null test
of the whole engine at ratio 1 (identity through the LPC pair to −100 dB), content-delay bounds while shifting,
NaN/denormal fuzz, bit-identical output across block sizes, detection accuracy and octave errors on synthetic
glottal-pulse vowels with jitter, shimmer, vibrato, scoops, noise and breaths (ground truth known exactly),
correction accuracy in cents, splice artefact measures, CPU on the Apple M4 and the Ryzen 9 5900XT, pluginval
at strictness 8 and auval. Test voices come from our own formant synth (ATFS1's vowel tables and LPC code
already exist) and espeak-ng; no copyrighted vocals.

**Visual** (§8). Recommended: an original rendered object, a brass plumb bob hanging over an engraved slate,
whose swing is the pitch error and which hangs still when the voice is corrected, above a full-width live note
view; the advanced page turns the slate into the piano roll with a per-note inspector.

**Name** (§9). Recommended **Plumb**; alternatives Aplomb, Keel, Gimbal, Truing, Sextant, Gnomon.

---

## 1. Market and users

### 1.1 Two families: note editors and real-time correctors

Pitch correction products split into offline note editors (Melodyne, Logic Flex Pitch, Cubase VariAudio,
Synchro Arts RePitch, the graph mode of Auto-Tune Pro) and real-time correctors (the "auto" mode of Auto-Tune,
Waves Tune Real-Time, Graillon, MAutoPitch, GSnap, ReaTune, Little AlterBoy, and the open-source zita-at1 /
x42 fat1 / Autotalent / TalentedHack). The first family analyses whole notes, including their future; the
second sees only the past plus whatever lookahead it reports as latency. The request asks for the editing
model of the first with the latency of the second; §1.11 sets out what of that is possible.

### 1.2 Melodyne 5 (Celemony): what it does and why it is not real-time

- **Analysis before editing.** Celemony's help centre says Melodyne must analyse the whole audio file before
  it can show notes, that this analysis cannot be done in real time, and that a plug-in normally sees only
  the audio being played at that instant. Hence the "transfer" workflow, in which the plug-in records its
  own copy of the track during playback, and ARA, through which an ARA-capable DAW hands Melodyne the
  complete files so editing can start immediately (Celemony help centre, "Transferring audio",
  https://helpcenter.celemony.com/M5/doc/melodyneEssential5/en/M5tour_TransferARA?env=live). ARA's SDK is
  open source under Apache 2.0 since May 2021 (Celemony, https://www.celemony.com/en/10-years-of-ara;
  https://rekkerd.org/celemony-releases-ara-sdk-under-open-source-license/).
- **Note blobs, pitch centre, modulation, drift.** Each note is an object with a pitch centre; the
  *pitch modulation* tool scales fast, usually intended deviations (vibrato, trills) and the *pitch drift*
  tool scales slow, usually unintended ones; both are percentages where 100 % is the original, 0 % a straight
  line and negative values invert the curve, and removing both leaves a flat monotone (Celemony help centre,
  "Pitch modulation and drift",
  https://helpcenter.celemony.com/M5/doc/melodyneStudio5/en/M5tour_ToolModulationDrift_2). The "Correct
  Pitch" macro pulls note centres towards the grid and reduces drift with two amounts (summarised by
  third-party tutorials, e.g. https://www.nrgrecording.com/post/tuning-vocals-with-melodyne; the help-centre
  page for the macro was not fetched).
- **New in version 5** (2020): a "Melodic" algorithm that separates pitched from unpitched components so
  tuning leaves sibilants alone, with a sibilant balance tool that works as a de-esser; correcting the
  perceived pitch of a vibrato note while keeping the vibrato; chord track and chord grid with chord
  recognition; a fade tool for individual notes inside chords; a levelling macro; a percussive-pitched
  algorithm (Sound On Sound news, https://www.soundonsound.com/news/melodyne-5-unveiled; MusicTech review,
  https://musictech.com/reviews/the-big-review-celemony-melodyne-5/; MusicRadar,
  https://musicradar.com/reviews/celemony-melodyne-51; Mix, https://www.mixonline.com/?p=107306). MusicTech's
  testers found the sibilant detection accurate on S, Z, Ch, K and T but weaker on Th (same review). Price
  range €99 to €699 across editions (same review).
- **Polyphony (DNA).** Direct Note Access edits notes inside chords; the underlying patent is US 8,022,286,
  "Sound-object oriented analysis and note-object oriented processing of polyphonic sound recordings"
  (Neubäcker, assigned to Celemony; filed 2009-03-05, granted 2011-09-20; listed as active by Google Patents,
  https://patents.google.com/patent/US8022286; Justia, https://patents.justia.com/inventor/peter-neub-cker;
  context in Sound On Sound's interview, https://www.soundonsound.com/people/peter-neubacker-melodyne-celemony-next-decade).
  **The plugin is monophonic and stays away from polyphonic note separation** (inferred; not legal advice).
- **Why it is not real-time** (inferred, from the above): a note's centre, its drift and the split between
  notes are properties of the whole note, including its end; timing edits need the future by definition; the
  pitched/unpitched separation and the DNA analysis are whole-file spectral analyses. A live tool can
  estimate the same quantities causally, with a lag (§4.3), and can offer the full editing model only on
  audio it has already captured (§5.4).

### 1.3 Auto-Tune (Antares): Pro 11, AutoTune 2026, the retired Artist and Access

- **Line-up in 2026.** Sound On Sound's review of AutoTune 2026 (John Walden, January 2026,
  https://www.soundonsound.com/reviews/antares-autotune-2026) says AutoTune 2026 replaces Artist, that Artist
  and Access were retired, and that Pro 11 remains the flagship with graphical editing and harmonies.
- **Latency.** In that review, Cubase measured **0.8 ms in Classic mode and 2.3 ms in Modern mode** with the
  low-latency option, and the highest-quality setting adds **58 ms** (same review). A retailer listing for the retired Artist claims under 1 ms in
  Classic mode
  (https://fullcompass.com/prod/576149-antares-auto-tune-artist-low-latency-pitch-correction-effects-virtual).
  Antares' own blog states a "core latency" of 2.5 ms in Low Latency Mode at 44.1 kHz and, in the same
  article, "175 samples at 44.1 kHz", which is 3.97 ms, so one of the two figures is wrong
  (https://www.antarestech.com/blog/real-time-pitch-correction-for-live-performance-with-autotune-2026;
  calculated). The same blog recommends 32- or 64-sample buffers and a dedicated live host for stage use. A
  roundup site attributes 112 samples (2.3 ms) Modern and 37 samples (0.77 ms) Classic at 48 kHz to "AutoTune
  Advanced" (https://mixinggpt.com/blog/autotune-advanced-vs-autotune-pro-11-2026; secondary, unverified
  product name). Pro X had a bug where Low Latency mode did not engage after being switched on, and the mode
  is off by default when instantiated (Antares support,
  https://help.antarestech.com/hc/en-us/articles/47201781644948-Low-Latency-Mode-Not-Working).
- **Detection and shifting: the original patent.** US 5,973,252 (Hildebrand, Auburn Audio Technologies;
  priority 1997-10-27, filed 1998-10-15, **expired 2018-10-14** per Google Patents,
  https://patents.google.com/patent/US5973252A/en) describes, in summary: period detection by an
  autocorrelation-type test (energy over two candidate periods against the lagged cross term; a lag is a
  period when their difference is small relative to the energy), computed recursively sample by sample so the
  full autocorrelation is never evaluated; a decimate-by-8 search at ~5.5 kHz for acquisition and a narrow
  window of lags around the current period for tracking; a check of L against 2L against octave errors;
  parabolic refinement of the period; correction by **resampling with a read pointer that runs at the
  period ratio and repeats or drops whole cycles** when it would overrun or fall behind; a smoothing
  parameter on the ratio that becomes the "retune speed"; targets from the nearest scale note or a MIDI note;
  optional vibrato. The patent claims locking "within a few cycles" and gives no latency in ms. Because it has
  expired, the method is free to use; the "Auto-Tune" name is not (inferred; not legal advice).
- **Controls users know.** Retune speed (how fast the pitch moves to the target); Humanize (slower retune on
  the sustained part of long notes); Flex-Tune (correct only notes that come close to the target, leaving
  bends and swoops); Natural Vibrato (scale the singer's own vibrato); synthetic vibrato; Throat (formant /
  vocal-tract length) and Transpose in Pro and the former Artist (SOS reviews above; Sound On Sound,
  https://www.soundonsound.com/reviews/antares-auto-tune-pro; Attack Magazine,
  https://www.attackmagazine.com/technique/tutorials/twisting-up-a-synth-drone-with-antares-auto-tune-pro/;
  Produce Like A Pro, https://producelikeapro.com/blog/how-to-use-auto-tune-vocal-tuning/). AutoTune 2026
  reduces the front page to Speed, Humanize and Flex Tune (Flex in Modern mode only) plus voice type, key and
  scale, and ships a separate AutoKey that listens and suggests key and scale (SOS 2026 review). Classic mode
  uses the older Auto-Tune 5 algorithm (SOS 2026); Pro 11's Classic mode reportedly turns off formant,
  transpose, flex and time editing (MixingGPT roundup, secondary).
- **What reviewers praise and complain about.** Praise: "a breeze to use", three controls, presets for
  newcomers, Classic mode for the effect, lower CPU than Artist (SOS 2026). Complaints: Transpose and
  Throat/Formant dropped from the new entry product, some exotic scales removed, perpetual price (£282 /
  $300) (SOS 2026). Live users report hearing themselves "doubled" in in-ears when correction sat on a direct
  insert with USB round-trip latency (Allen & Heath forum,
  https://forums.allen-heath.com/t/use-autotune-as-direct-insert-but-skip-autotune-for-in-ears/12693).

### 1.4 Waves Tune Real-Time

Reviews describe its latency as **dynamic, between 0 and 4 ms, longer for lower notes** because they take
longer to detect (Barry Rudolph, https://and.barryrudolph.com/newtoys/toys16/waves_tune_real-time.html, ©2016;
Music Connection, 2016-12-14, https://www.musicconnection.com/music-gear-waves-tune-real-time/). Features in
those reviews: note tune speed, note transition time, vibrato preservation, formant control, a correction
ratio, voice-range settings from bass to soprano (narrowing the range improved accuracy on fretless bass),
non-Western and custom scales with out-of-key notes flagged, MIDI note control, and SoundGrid for live
consoles. Praise: intuitive, high fidelity, MIDI workflow. A Waves forum user felt a slight delay when
tracking in Logic, and the reply blamed buffer size and other plugins
(https://forum.waves.com/t/problem-with-delay-effect-when-using-waves-tune-real-time/3900). The "0-4 ms,
depending on pitch" description matches a cycle-splicing shifter whose content delay moves within one period
(§3.4) (inferred).

### 1.5 iZotope Nectar

Nectar has a Tracking mode that runs reduced, low-latency versions of pitch correction, doubler and limiter
and disables high-latency modules, against a Mixing mode that may exceed the host's delay compensation
(review summaries: https://barryrudolph.com/mix/izotopenectar.html, https://ra.co/reviews/8667). No sample
figure for the pitch module was found. Lesson (inferred): a vocal suite's live mode is the same idea as our
Live/Studio split, and users accept lower quality for live use when the trade is explicit.

### 1.6 Graillon 3 (Auburn Sounds)

From the user's guide (https://www.auburnsounds.com/downloads/Graillon%203%20User's%20Guide.pdf, read in full
for the relevant pages): three engines, G2 (the previous version, sometimes preferable for male voices), G3
(better detection of high notes, better "personality" preservation) and I2 (the Inner Pitch shifter, more
natural on bass content, "very slow" in CPU); formant preservation with an **amount** (the guide notes full
formant correction "sounds harsh" and should be scaled back) and a formant shift of ±12 semitones; correction
**Amount**, **Smooth** (transition speed; minimum for hard tune), **Inertia** (makes chosen notes "stick";
"quite a critical setting"), **Snap Min** (deviation below which vibrato is left alone) and **Snap Max**
(how far a note may be pulled, default 8 semitones); its effects (compressor, gate, chorus, preamp,
bitcrusher) add no latency; release notes mention fixes to latency reporting in several hosts and formats
(AU in Reaper, LV2, VST3 only correct at 44.1 kHz), and that pitch shifting made the latency non-zero from
version 2. MusicTech calls G3 its fastest correction and I2 seven times slower
(https://musictech.com/news/gear/auburn-sounds-graillon-3/); the free edition includes correction and shifting
(https://www.musehub.com/learn/best-autotune-plugins). A review of an earlier version lists latency as a con
("can make recording wonky"; summarised from https://bedroomproducersblog.com/2016/09/14/graillon-voice-octaver/,
secondary), and a search summary attributes 23 ms to an older version (unverified; not used).

Lessons (inferred): "Snap Min/Max" is a clearer name for Flex than Flex; full formant preservation is not
always wanted, so formant *amount* belongs on the advanced page; latency-reporting bugs per format and sample
rate are common enough that §7 tests the reported value in every format and rate.

### 1.7 MAutoPitch (MeldaProduction), GSnap (GVST), Little AlterBoy (Soundtoys), ReaTune (Cockos)

- **MAutoPitch**: free automatic tuning for vocals and monophonic instruments, with formant shift, stereo
  widening and a limiter (https://www.meldaproduction.com/MAutopitch; review
  https://audiopluginlist.com/meldaproduction-mautopitch-review/). No latency figure published; reviewers
  call the interface technical and note there is no graphical note editing.
- **GSnap**: free; monophonic input only; detection set by Min/Max frequency, Gate and **Speed**, where a
  lower Speed detects faster but with more false detections; fixed-scale or MIDI-driven correction with
  pitch bend and vibrato; a calibration control for A (GVST manual,
  https://www.gvst.co.uk/gsnap_manual.htm). The speed/accuracy knob exposes exactly the trade-off of §2.1.
- **Little AlterBoy**: monophonic pitch/formant shifter with a hard-tune quantise mode and robot mode; no
  published latency (Dubspot's comparison leaves it blank,
  https://blog.dubspot.com/plugins/compare/blepfx-spectra-vs-little-alterboy); a Thomann user reports no
  noticeable latency unless many instances run (https://www.thomann.it/soundtoys_little_alterboy_reviews.htm).
  Its appeal is the formant knob as a character control, which our "throat" mirrors.
- **ReaTune**: Reaper's bundled tuner/corrector. Tutorials recommend detection windows of 25-50 ms depending
  on register (larger for low voices; smaller windows sound more robotic) and attack times of 20-50 ms instead
  of the 250 ms default (ProRec, https://ProRec.com/?p=6487); the tuner struggles below D2 unless the window is
  enlarged (https://ProRec.com/?p=6396); the Cockos wiki entry is a stub
  (https://www.cockos.com/wiki/index.php/ReaTune). It shows the cost of a fixed analysis window: 25-50 ms is
  far outside a live budget.

### 1.8 The open-source correctors

- **zita-at1** (Fons Adriaensen): an autotuner for small corrections of voice; **~21 ms latency normally and
  ~10 ms with a low-latency button, in which case the pitch data is 10 ms older than the audio it
  corrects**; controls Bias (favour the current note), Filter (smooth the correction while the note stays the
  same; bypassed on note changes), Correction (fraction of the error corrected; full correction can remove
  vibrato), Offset (±2 semitones); up to 12 enabled notes; MIDI only through JACK; **no formant correction,
  so only for small corrections** (quick guide,
  https://kokkinizita.linuxaudio.org/linuxaudio/zita-at1-doc/quickguide.html).
- **x42-autotune / fat1.lv2** (Robin Gareus): zita-at1 as LV2 with latency reporting, state, and MIDI as a
  side chain with Manual / MIDI / Auto modes; correction by resampling and looping as in zita-at1; GPL; no
  formant correction (https://x42-plugins.com/x42/x42-autotune).
- **Autotalent** (Tom Baran, 2009-2010, GPL-2 with BSD-licensed FFT routines): autocorrelation pitch
  detector and a time-domain overlap-add shifter synchronous with the pitch period; formant correction and
  warping, LFO vibrato, scale-aware harmony, pitch and confidence outputs; the author gives **43 ms at 48 kHz
  as its minimum theoretical latency** (http://tombaran.info/autotalent.html, fetched with an expired
  certificate).
- **TalentedHack** (Jeremy Salwen, 2010): an LV2 fork of Autotalent with MIDI in/out, separate
  pull-to-semitone and snap-to-scale controls and FFTW; its author noted artefacts in its formant corrector
  that the original did not have (LAU announcement,
  https://lists.linuxaudio.org/archives/linux-audio-user/2010-July/071617.html); listed as discontinued by a
  third-party directory (https://alternativeto.net/software/talentedhack/about/).

### 1.9 New in 2025-2026

- **UAD Topline Vocal Tune** (announced 2026-02-06, $99, intro $79): one correction dial from transparent to
  hard, "Natural Voice" settings, pitch and formant shift, MIDI re-pitching, key/scale finder; "ultra-low
  latency" without a figure (Production Expert,
  https://www.production-expert.com/production-expert-1/uad-topline-vocal-tune-released-ultra-low-latency-native-pitch-correction).
- **Vitric Audio Burnish Vocal Chain** (v1.2.0): a vocal chain whose tuning stage has a hard "Classic" path
  reporting **4 samples** of latency for tracking and a "Natural" path for mixing (12.5 ms in its Mix mode,
  per a search summary of the listing) (KVR,
  https://www.kvraudio.com/product/burnish-vocal-chain-by-vitric-audio). Its 1.2 notes say "high notes no
  longer track an octave low", the classic octave-error bug (§2.5).
- **Antelope InTune Performer**: Strength and Delay controls, chromatic/major/minor, a side-chain "Proline"
  mode that follows a guide melody; no latency figure (https://en.antelopeaudio.com/product/intune-performer/).

### 1.10 What users value and what they complain about (synthesis)

From §1.2-§1.9 (the sources are reviews and vendor documents more than forums; inferred where stated):
- **Valued**: few controls on the front page (AutoTune 2026's three; Topline's one dial); presets per voice
  type; automatic key; a Classic/hard mode that is genuinely instantaneous; vibrato kept while the centre is
  corrected (Melodyne 5's headline, Waves' vibrato preservation, Graillon's Snap Min); MIDI control of the
  target; formant/throat as a creative control.
- **Complaints**: latency that is not stated or is wrong in some hosts and formats (Graillon's release notes,
  Antares' Low Latency bug, the inconsistent Antares blog figure); quality modes that are unusable live
  (58 ms, 21 ms); octave errors on high notes and low voices; artefacts on sibilants and breaths (the reason
  for Melodyne 5's pitched/unpitched split); full formant correction sounding harsh; removed features in
  cheaper editions; price and subscriptions; no free tool combines formant preservation with sub-millisecond
  latency (zita-at1 and fat1 have no formant correction; Autotalent and Graillon are tens of ms or unstated).

### 1.11 "Better than Melodyne, but live": what is realistic (inferred)

Feasible in real time:
1. **A live note view** that draws notes as blobs while they are sung, with the detected line, the target and
   the corrected line (§5.1). Melodyne shows blobs only after analysis.
2. **Correction of the note centre with vibrato and drift kept separately** (§4.3): the causal estimate of the
   centre lags by a fraction of a vibrato cycle, which is the price of doing it live.
3. **Per-degree rules**: amount, speed and flex per scale degree or per pitch class, shown on the blobs.
4. **Formant-preserving shift at sub-millisecond latency** (§3.6), with formant amount and throat.
5. **Targets from scale, chord or MIDI**, and **automatic key detection** without a side chain (§4.6).
6. **Humanize** and natural transitions that keep scoops and falls (§4.4).

Feasible only on captured audio (capture mode, §5.4): editing a note's pitch by dragging, per-note
drift/vibrato amounts, split/merge, and (later) timing. It works like Melodyne's non-ARA transfer: the
plugin records the phrase as it passes, analyses it on a worker thread, and when the host transport plays
over that region again it outputs the edited phrase instead of the input.

Not feasible, or out of scope: polyphonic note editing (whole-file spectral analysis, and the active DNA
patent), timing correction on live input (needs the future), Melodyne-quality pitched/unpitched separation
in real time (we approximate it by not shifting unvoiced segments, §2.5).

---

## 2. Pitch detection at low latency

### 2.1 The fundamental trade-off

A time-domain period detector compares the signal with a copy of itself shifted by a candidate lag τ. To
test every lag up to the longest period T_max = 1/f_min with an integration window W, it needs W + T_max
samples of history. With W = T_max (the textbook "two periods"), at f_min = 80 Hz that is 2 × 12.5 = **25 ms**;
at 65.4 Hz (C2) 30.6 ms (calculated). Frequency-domain detectors are worse: resolving harmonics 80 Hz apart
needs a window of several periods. Real-time correctors get under this in five ways (inferred unless cited):

1. **Shorter windows.** W = T_max/2 gives 1.5 periods (18.8 ms at 80 Hz) at some cost in robustness; YIN's
   normalisation tolerates short windows better than raw autocorrelation (de Cheveigné and Kawahara 2002,
   below).
2. **Tracking instead of searching.** Once locked, search only lags near the last period (the Hildebrand
   patent's narrow lag window, §1.3), with W equal to about the current period, not the longest. A pitch
   change then shows within about one *local* period: 4.5 ms at 220 Hz (calculated).
3. **Voice-range presets.** The longest period is the preset's, not 50 Hz: a High preset (f_min 175 Hz)
   needs 8.6 ms at 1.5 periods instead of 18.8 ms (calculated). Waves' and Auto-Tune's voice types do this
   (§1.3, §1.4).
4. **Holding the previous estimate.** The audio is never delayed for the detector; the shifter keeps using
   the last ratio while the detector catches up, which is why the first milliseconds of a note sound
   uncorrected (a natural "scoop" at gentle settings, a short glitch at hard settings).
5. **Lookahead as reported latency.** Every millisecond of reported latency is a millisecond of future the
   detector can see. zita-at1's low-latency switch makes the trade explicit: half the latency, pitch data
   10 ms older (§1.8).

Pitch perception numbers that set the accuracy target (from memory, not re-verified today): the pitch
difference limen for sustained tones is around 5-10 cents in the voice range; singers' sustained notes are
judged "in tune" within roughly ±20-25 cents; the perceived pitch of a vibrato tone is close to the mean of
its excursion (Shonle and Horan 1980; d'Alessandro and Castellengo 1994). Typical singing vibrato is about
5.5-7 Hz with an extent of roughly ±0.3-1 semitone (Prame 1994, JASA 96(4); Sundberg, *The Science of the
Singing Voice*, 1987). A detector accurate to ±2-3 cents on sustained voiced frames is therefore enough.

### 2.2 Candidate detectors

| Method | Principle | Minimum data | Strengths | Weaknesses for this use |
|---|---|---|---|---|
| Autocorrelation (ACF) + parabolic interpolation | peak of r(τ) | ≥ 2 periods | simple, FFT or incremental | biased to short lags with a finite window; octave-down errors on strong 2nd harmonic; needs a peak-picking heuristic |
| Squared difference / Hildebrand recursion | d(τ) = Σ(x_j − x_{j+τ})² = E − 2H | 2 periods | incremental per sample (patent, §1.3) | raw d(τ) favours long lags at the low end; needs a threshold |
| **YIN** | cumulative-mean-normalised d(τ), absolute threshold, first dip, parabolic interpolation | W + τ_max | robust first-dip rule against octave errors; aperiodicity measure for voicing for free | threshold tuning; short W reduces accuracy (de Cheveigné and Kawahara, "YIN, a fundamental frequency estimator for speech and music", JASA 111(4):1917-1930, 2002; from memory) |
| **McLeod (MPM)** | normalised square difference function (NSDF), "key maxima", first key maximum above k × the highest | ~2 periods | clean 0-1 clarity measure; good on instruments; real-time capable (McLeod and Wyvill, "A smarter way to find pitch", ICMC 2005; TarsosDSP docs, https://0110.be/releases/TarsosDSP/TarsosDSP-2.3/TarsosDSP-2.3-Documentation/be/tarsos/dsp/pitch/McLeodPitchMethod.html) | same data need as ACF |
| Cepstrum | peak of the real cepstrum | 3-4 periods | separates source and filter | long window; poor on high voices (Noll, "Cepstrum pitch determination", JASA 41(2), 1967; from memory) |
| SWIPE / SWIPE' | match against sawtooth-like spectral templates over many window sizes | several periods | very accurate offline (Camacho and Harris, JASA 124(3), 2008; from memory) | latency and cost |
| pYIN | YIN with a distribution of thresholds → multiple candidates, HMM/Viterbi smoothing | offline or long lookahead | best octave stability among classic methods; voicing probability (Mauch and Dixon, ICASSP 2014; from memory) | needs future frames; ideal for capture analysis (§5.4) and the Studio lookahead |
| CREPE (neural) | CNN on 1024 samples at 16 kHz, 360 bins of 20 cents, 31-2006 Hz | 64 ms window | state-of-the-art accuracy | the input window alone is 64 ms (calculated); heavy (Kim et al., ICASSP 2018, https://arxiv.org/abs/1802.06182; MathWorks block docs, https://www.mathworks.com/help/audio/ref/crepeblock.html) |
| PESTO (neural, self-supervised) | VQT frames, transposition-equivariant, ~130k parameters | streaming VQT, < 10 ms reported | small, fast (≈12x real time on a CPU per Sony CSL) | a network in the audio path of an AGPL plugin, training data provenance, CPU on Windows; still more latency than Live (Riou et al., TISMIR 2025, https://arxiv.org/abs/2508.01488; https://cslmusicteam.sony.fr/news/new-paper-pesto-a-novel-method-for-pitch-estimation-with-a-self-supervised-transposition-equivariant-objective/) |
| Zero-crossing / peak-interval trackers | time between crossings or between glottal peaks | ~1 period | fastest possible | fooled by strong harmonics and formants (AT20V's own comment in `plugins/at20v/Source/dsp/Modules.h` says the same); useful only as a refinement inside a known period |

Neural detectors are attractive for the offline capture analysis but not for the Live path: their windows or
their cost exceed the budget, and they add a model file whose training data licence must be clean for an
AGPL product (inferred). Revisit for v2 (§11).

### 2.3 Sub-period and predictive approaches

- **Epoch (glottal closure) intervals.** In voiced speech the excitation is a train of glottal closures; the
  interval between the last two closures is an instantaneous period estimate available about one period
  after the change, faster than any window method. Glottal closure instant (GCI) detectors such as DYPSA
  (Naylor et al., IEEE TASLP 15(1), 2007) and SEDREAMS (Drugman and Dutoit; review in IEEE TASLP 20(3), 2012)
  are offline-oriented (from memory, not re-verified), but a causal version is easy when the period is
  already known approximately: the strongest peak of the LPC residual in the last period (inferred). We
  need the residual anyway (§3.6), so this refinement is nearly free.
- **Prediction.** Within a note the pitch trajectory is smooth; a second-order predictor (constant velocity
  in cents) extrapolates the detector's output by its own lag, reducing the effective lag on glides and
  vibrato by 30-60 % in tests on synthetic vibrato (inferred, to be measured, §7 T8). It must be switched off
  at onsets and voicing changes.
- **Using the correction's own knowledge.** In hard-tune the output pitch is known exactly; the detector
  must always run on the input, never on the output (inferred; obvious, but a feedback bug here creates
  wandering).

### 2.4 Octave errors

Causes (inferred, well known): a weak fundamental with a strong 2nd harmonic (octave-up error on low male
voices with a telephone-like tilt, or octave-down on period-doubled "vocal fry"); a strong formant near a
harmonic; noisy onsets. Guards:
1. YIN's first-dip rule: take the shortest lag whose normalised difference falls below the threshold, not
   the global minimum (this removes most sub-octave errors).
2. The patent's L/2L check (§1.3): compare the candidate with twice and half its lag on full-rate data.
3. Continuity: an octave jump is accepted only if confirmed by two consecutive analysis hops, or if an onset
   (energy rise or voicing restart) precedes it.
4. Voice-range preset limits.
5. Optional "octave lock" on the advanced page: the singer's octave is assumed stable within a phrase.
6. In Studio mode, a Viterbi pass over the candidates of the last 21 ms (pYIN-style, §2.2).

### 2.5 Voicing, onsets, sibilants and breaths

- **Voiced/unvoiced**: YIN's minimum normalised difference (aperiodicity) below 0.15-0.2 and the energy above
  a gate relative to the running noise floor (starting values; AT20V's YIN uses 0.15 with an envelope gate;
  inferred).
- **Sibilants**: high zero-crossing rate and a high ratio of energy above 4 kHz to energy below 1 kHz; they
  must not be shifted (they have no pitch; shifting them produces "lisping" or flanging). When unvoiced, the
  correction ratio glides to 1 within ~3 ms and splices stop.
- **Breaths and noise**: aperiodic and low level: pass through unshifted.
- **Onsets**: the first 1-3 glottal cycles of a note are irregular; the detector should require stable
  periodicity (two hops) before applying a *new* target. In Tight and Studio, the lookahead lets the voicing
  decision for a sibilant come before the sibilant reaches the output.
- **Returning the delay**: during unvoiced segments the shifter's content delay is brought back to its
  minimum (§3.4) by a splice inside the noise, where it is inaudible (inferred).

### 2.6 Recommended detector (inferred)

1. **Side chain**: DC blocker; high-pass 50 Hz; low-pass and decimate to ~12 kHz (factor 4 at 48 kHz, 8 at
   96 kHz) with a low-delay IIR (an 8th-order elliptic has well under 1 ms of group delay in the passband) or
   the shared half-band chain if its delay is acceptable (§10); the audio path is untouched.
2. **Acquisition**: YIN cumulative-mean-normalised difference over all lags of the preset
   (`τ ∈ [1/f_max, 1/f_min]`), window W = T_max/2, every hop of 8 decimated samples (0.67 ms); threshold 0.15;
   first dip; parabolic interpolation.
3. **Tracking**: once two hops agree, lags restricted to ±20 % around the last period; W = max(1 local
   period, 1.5 ms); the difference terms maintained incrementally per decimated sample (the patent's E/H
   recursion), so each hop costs O(number of lags) instead of O(lags × window).
4. **Refinement**: full-rate parabolic refinement plus the LPC-residual peak interval (§2.3) blended by
   confidence; output in cents (float MIDI note) with a confidence and a voiced flag, time-stamped with the
   *input* sample at the centre of the analysis window, so the shifter can look up the pitch of the exact
   sample it is reading (the read position lags the write position by the content delay; inferred, §3.4).
5. **Guards**: §2.4; voicing §2.5.
6. **Studio**: the same, plus a centred window (21 ms of future) and a Viterbi pass over the three best
   candidates per hop.

**Data needed and lags** (calculated; acquisition 1.5 × T_max, tracking ≈ 1-1.5 local periods; hop 0.67 ms
added; the presets are proposals):

| Preset | Range | T_max | Onset lock (Live) | Onset lock (Tight) | Onset lock (Studio) | Pitch change mid-note at a typical note |
|---|---|---|---|---|---|---|
| Low (bass/baritone) | 65-520 Hz | 15.3 ms | ~23 ms | ~20 ms | ~2 ms | ~7-11 ms at 130 Hz |
| Mid (tenor/alto) | 80-880 Hz | 12.5 ms | ~19 ms | ~16 ms | 0 | ~4.5-7 ms at 220 Hz |
| High (soprano) | 175-1400 Hz | 5.7 ms | ~9 ms | ~6 ms | 0 | ~2.3-3.4 ms at 440 Hz |
| Instrument | 40-2000 Hz | 25 ms | ~38 ms | ~35 ms | ~16 ms | depends |

These are correction lags, not audio latency. They are comparable with or better than the windows reported
for zita-at1 (10-21 ms) and ReaTune (25-50 ms) (§1.7, §1.8). Cost (calculated, rough): ~185 lags × 2
multiply-adds × 12 000 decimated samples/s ≈ 4.4 M operations/s for the recursion, well under 0.5 % of a
core.

---

## 3. Pitch shifting with near-zero added latency

### 3.1 Requirements

Monophonic, harmonic input; shifts of a few cents to ±1 semitone for correction, up to ±12-24 for transpose
and MIDI targets; formants preserved (or deliberately shifted); no added latency beyond a few samples in Live;
no clicks at the instants where cycles are repeated or dropped; continuous phase when the ratio changes (the
hard-tune step must be instantaneous in pitch but not a discontinuity in the waveform).

### 3.2 The options

- **Phase vocoder** (Laroche and Dolson, IEEE TSAP 7(3), 1999; from memory): analysis frames of 1024-4096
  samples at 48 kHz, i.e. 21-85 ms of window, plus resampling or envelope work. Its latency is at least one
  frame and its artefacts (phasiness, transient smearing) are wrong for voice. Rejected for every mode
  (inferred). AutoTune 2026's 58 ms high-quality mode and Burnish's 12.5 ms "Natural" path show that the
  quality modes of commercial tools pay this kind of latency (§1.3, §1.9).
- **Delay-line (Doppler) shifter**: two or more read heads sweeping a delay line with crossfaded windows; any
  input, no detection, latency of about one grain (Halation's shimmer shifter in this repository uses 90 ms
  grains). Not pitch-synchronous, so the crossfades beat against the voice's period (warble), and formants
  move with the pitch. Rejected (inferred).
- **WSOLA** (Verhelst and Roelands, ICASSP 1993; from memory): overlap-add with each segment's position
  adjusted within a tolerance to maximise similarity; a time-scale method, combined with resampling to shift
  pitch. We borrow its similarity search to refine splice offsets (§3.4) (inferred).
- **TD-PSOLA** (Moulines and Charpentier, Speech Communication 9(5-6), 1990; from memory): two-period grains,
  Hann-windowed and centred on epochs, re-spaced at the target period and overlap-added. Formants are
  preserved because each grain carries the vocal tract's response to one glottal pulse. Its causal latency is
  at least **one longest period** (a grain centred on an epoch extends one period after it; calculated: 12.5
  ms at 80 Hz, 15.3 ms at 65 Hz) plus the epoch detector's lookahead. Accurate epochs matter: inconsistent
  marks cause roughness and a slight pitch error. TD-PSOLA patents from the late 1980s have long expired
  (from memory, not re-verified). Good candidate for Studio, too slow for Live.
- **Lent's pitch-synchronous method** (Lent, "An efficient method for pitch shifting digitally sampled
  sounds", Computer Music Journal 13(4), 1989; from memory): repeats or drops windowed single periods
  synchronously with the detected pitch, preserving formants; a relative of PSOLA with smaller grains.
- **Resampling with cycle splicing** (Hildebrand patent, §1.3): read pointer at the ratio, jump by one period
  when it overruns or lags. Latency: only the interpolator plus a small margin, because both read positions
  are always in the past. Formants move with the ratio: harmless for corrections (±50 cents ≈ ±3 %), audible
  ("chipmunk"/"giant") beyond about ±2-3 semitones (inferred).
- **Spectral-envelope methods for formants**: cepstral/true-envelope estimation (Röbel and Rodet, DAFx-05,
  "Efficient spectral envelope estimation and its application to pitch shifting and envelope preservation";
  from memory) or LPC. LPC is the cheap time-domain choice, and it can run sample by sample without a frame
  delay if the filters are applied causally with coefficients from past data.

### 3.3 What makes the hard-tune ("robot") effect sound right (inferred)

1. **Instant retune**: output pitch equals the target as soon as the target is known; the singer's own
   glides become steps. Retune speed 0 must mean zero smoothing, not "fast".
2. **Instant but phase-continuous transitions**: the ratio changes at the sample, the waveform does not jump.
   A read pointer at a variable rate does this by construction.
3. **Target switching without hysteresis** in hard mode: the step happens at the midpoint between notes, so
   melismas become crisp staircases; with hysteresis the steps lag and smear.
4. **Detection lag shows as a short warble at the step**: the input has already moved while the old target
   is still applied; at ~5 ms it is part of the character (the Classic sound is identified with the older
   algorithm, §1.3). Prediction (§2.3) should be off in hard mode to keep that character.
5. **Vibrato removed** (vibrato keep 0) and a correct key; with a wrong key the effect turns into wrong notes.
6. Formant preservation can be on or off: off gives the slightly brighter, synthetic tone of resampling
   shifters on upward steps.

### 3.4 The cycle-splicing scheduler in detail (inferred)

Let the write pointer advance one sample per sample and the read pointer advance r(t) = 2^(Δ(t)/12) samples
per sample, where Δ is the correction in semitones. The content delay is d(t) = write − read.
- r > 1 (shifting up): d shrinks. When d would fall below the minimum D₀, the reader jumps back by one local
  period T: d += T (a cycle is repeated).
- r < 1 (shifting down): d grows. When d exceeds D₀ + T + margin, the reader jumps forward by T (a cycle is
  dropped).
- So d stays in [D₀, D₀ + T + margin]: **the reported latency is D₀ and the content delay moves within one
  period above it** (calculated, from the construction). At r = 1 no splice happens and d stays where it was;
  during unvoiced segments it is returned to D₀ (§2.5).
- **Splice offset**: T comes from the detector; the actual jump is T + δ with δ ∈ [−4, +4] samples chosen to
  maximise the normalised cross-correlation of 2 × crossfade samples around the two read positions (WSOLA's
  idea), so pitch-estimate errors do not cause phase jumps.
- **Splice timing**: a repeat may be done any time d < D₀ + T/2 (there is slack), so it is scheduled when the
  read position is between glottal pulses, i.e. in the low-energy part of the excitation; in Tight and Studio
  the lookahead lets the scheduler see the next pulse before choosing.
- **Crossfade**: equal-power over N_x samples (24 at 48 kHz, 0.5 ms, as a starting value), both streams read
  with the same interpolator.
- **Margin**: during a crossfade the old stream keeps advancing at r, so for upward shifts D₀ ≥ N_x(r − 1) +
  h, where h is the interpolator's look-forward. With N_x = 24, h = 4 and r ≤ 2 (one octave up), D₀ ≥ 28;
  **32 samples at 48 kHz** covers it (calculated). Larger upward ratios (transpose beyond +12) need a larger
  crossfade-adjusted margin or a shorter crossfade; cap Live transpose at +12 and document it.
- **Interpolator**: 4-point, 3rd-order Hermite (2 samples ahead) or a 4-tap Lagrange; a windowed-sinc of 8-16
  taps is better at large downward ratios (aliasing) and costs 4-8 samples of margin. Starting value: Hermite
  in Live, 16-tap windowed sinc in Studio (inferred; the test T13 decides).
- **Time-aligned ratio**: Δ for the sample being read is computed from the pitch of *that* input sample
  (the detector's time-stamped history), not the newest estimate. Otherwise, at a note change the new
  correction would be applied to audio from up to one period earlier.

### 3.5 Formants: the source-filter version (inferred)

Doing the splicing on the **LPC residual** instead of the waveform gives formant preservation at the same
latency:
1. Every 2.7 ms (128 samples at 48 kHz), estimate an all-pole envelope from the last ~20-30 ms (Hann window,
   pre-emphasis 0.9 for the analysis only, autocorrelation, Levinson-Durbin). Order 20-24 with **frequency
   warping** (warped LPC, λ ≈ 0.6-0.7 at 44.1-48 kHz; Härmä et al., "Frequency-warped signal processing for
   audio applications", JAES 48(11), 2000; from memory) to concentrate resolution where formants are, or
   order ~40 unwarped.
2. Inverse-filter the input with A(z) in lattice form, with reflection coefficients interpolated per sample
   between updates (stable as long as |k| < 1): the residual is close to a pulse train plus noise.
3. Run the splice resampler of §3.4 on the residual. The GCIs are its strongest peaks, which helps both the
   splice timing and the detector refinement (§2.3).
4. Re-synthesise with 1/A(z) using the **same** interpolated coefficients: the shifted excitation goes back
   through the original envelope, so formants stay where they were.
5. Formant shift / throat: synthesise with a modified envelope. Options: (a) a different warping factor in the
   synthesis lattice than in the analysis (cheap, continuous, but the mapping to "semitones of formant
   shift" is not a pure scaling and must be calibrated with a table measured in the tests); (b) LPC → roots →
   scale the pole angles → rebuild (exact scaling; root finding of order 24 every 2.7 ms is affordable;
   ATFS1's Fseq already finds LPC roots). Recommend (b) for accuracy, with pole radii clamped below 0.995.
6. **Formant amount** (Graillon's lesson, §1.6): blend between "formants move with the pitch" (plain
   waveform splicing) and "formants preserved", by scaling the pole angles by r^(1−amount).

Identity check: at r = 1 with no splices, step 4 inverts step 2 exactly up to floating-point rounding, which
gives the strongest test in §7 (T3). Known limit: for high voices (f₀ > ~400 Hz) the harmonics are too sparse
for LPC to see the formants and it starts to model individual harmonics; there, reduce the order and fade
formant preservation towards the waveform path (high voices need small corrections anyway) (inferred; §7 T12
checks it).

### 3.6 Recommended design and its latency (inferred)

One engine, used in every mode:

```
input ─┬─────────────────────────────────────────────── analysis delay (lookahead L) ──┐
       │                                                                              │
       ├─ detector side chain (§2.6) ─ pitch history (time-stamped) ─ correction (§4) ─ Δ(t)
       │                                                                              │
       └─ LPC analysis (every 128 samples) ─ coefficients ───────────┐                │
                                                                     ▼                ▼
             delayed input ─ A(z) lattice ─ residual ─ splice resampler (§3.4) ─ 1/A'(z) lattice ─ output
```

- **Live**: L = 0; D₀ = 32 samples (48 kHz). Reported latency 32 (0.67 ms); 32 at 44.1 kHz (0.73 ms); 64 at
  96 kHz.
- **Tight**: L = 96 samples, so reported = 128 (2.67 ms at 48 kHz). The lookahead is used by the splice
  scheduler, the sibilant/voicing decision and the detector.
- **Studio**: L = 992, reported = 1024 (21.3 ms at 48 kHz): centred detection window for all presets
  (T_max of Low = 15.3 ms fits), Viterbi over candidates, 16-tap sinc interpolator. TD-PSOLA is an
  alternative Studio engine to A/B against this one (§11); the default plan keeps one engine.

Budget check against the user-perceived round trip (inferred): an interface at 48 kHz with a 64-sample buffer
typically has 3-5 ms of round trip; Live adds 0.67 ms, Tight 2.67 ms. Both stay under the 10 ms that the
vendor blog calls undetectable for most people, and Live stays near the in-ear figures in §6.3.

CPU (calculated, rough, at 48 kHz): detector ≈ 4.4 M operations/s; LPC analysis (24 lags over 1024 samples
every 128) ≈ 9 M multiply-adds/s; two lattices of order 24 ≈ 4.6 M; interpolation and crossfade < 1 M;
root finding for throat ≈ 1-2 M. About 20 M operations per second: **target ≤ 0.5 % of one core on the
Apple M4 and ≤ 1 % on the Ryzen 9 5900XT in Live and Tight, ≤ 1 % / 2 % in Studio** (to be measured, T19).

---

## 4. Correction logic

### 4.1 Pitch representation

Everything in semitones as a float MIDI number (69 = A at the reference, 415-466 Hz, default 440). The ratio
is 2^(Δ/12) computed with the repository's `fastExp2` (`shared/dsp/Common.h`, error 0.0004 cent).

### 4.2 Targets

- **Scale/key**: 12 pitch classes, each *snap* (a target), *skip* (pulled to the neighbour) or *bypass* (no
  correction near this note, e.g. a blue note). Built-in: major, natural/harmonic/melodic minor, the modes,
  pentatonics, blues, chromatic, plus custom. Non-12-TET scales via cents offsets per degree (Waves' non-Western
  scales, §1.4) and Scala `.scl` import in a later version.
- **Chord**: allowed pitch classes from a chord symbol or from MIDI chords held on the input.
- **MIDI**: (a) scale from held notes (any notes held define the allowed set, released notes stay until a new
  chord); (b) **note lock**: the target is the held note regardless of distance, with pitch bend (±2 by
  default) and last-note priority; Auto mode = MIDI when notes are held, otherwise the scale (fat1's Manual /
  MIDI / Auto, §1.8). MIDI arrives in the same block; it is applied at its sample offset.
- **Nearest note with hysteresis**: the target is the allowed note nearest to the **note centre** (§4.3),
  not to the instantaneous pitch, and a new target is taken only when the centre passes the midpoint by more
  than the hysteresis (zita-at1's "Bias", Graillon's "Inertia"; default 15 cents natural, 0 hard).
- **Flex window** (Flex-Tune / Snap Min-Max, §1.3, §1.6): correction amount as a function of the distance
  between the centre and the target: full inside ±F_in, fading to zero at ±F_out (defaults F_in = 35 cents,
  F_out = 70 cents in Natural; F_in = F_out = 600 in Hard, i.e. always). Deliberate bends and swoops outside
  the window pass untouched.

### 4.3 Separating centre, drift and vibrato (inferred)

The detected pitch p(t) of a voiced note is modelled as centre c (constant per note), slow drift d (below
~2 Hz), vibrato v (~4-8 Hz) and jitter (faster, small). Causally:
- **Vibrato detector**: band-pass p(t) at 3.5-8.5 Hz (two 2nd-order sections at the decimated pitch rate,
  1.5 kHz); vibrato is present when that band holds more than ~70 % of the variance of p − LP₀.₅Hz(p) over the
  last 400 ms and its zero-crossing rate is regular. Its rate and extent are measured from the last two
  extrema.
- **Centre**: when vibrato is present, the mean of the last two extrema (peak and trough), updated every
  half cycle (~80 ms at 6 Hz), smoothed; when absent, a critically damped low-pass of p at 3 Hz. At note
  onsets the centre is initialised from the first stable 20-30 ms (or from the lookahead in Studio). This
  causal estimate lags by about half a vibrato cycle: the price of doing Melodyne's "pitch centre" live.
- **Drift**: LP₂Hz(p) − c. **Vibrato**: p − c − d after removing the vibrato band's high-frequency jitter.
- **Output pitch**: target + keep_d × d + keep_v × v + jitter (when keep_v = keep_d = 1 the correction is a
  pure centre shift, the Melodyne 5 behaviour of correcting the perceived pitch while keeping vibrato, §1.2).
  Δ = output − p, weighted by Amount and the Flex window, then smoothed by the retune stage (§4.4).
- **Hard mode**: keep_v = keep_d = 0 and the centre is skipped: Δ = target − p directly, so the output is the
  target exactly.

### 4.4 Retune speed, transitions and humanize (inferred)

- **Retune speed** (0-400 ms): time for the correction to reach 95 % of a step, as a critically damped
  second-order smoother on Δ (no overshoot; a one-pole has a sharp start that sounds mechanical at medium
  speeds). 0 bypasses the smoother. Antares' blog quotes 0 ms for hard and 15-50 ms for background correction
  (§1.3), a sensible default range for the Style macro.
- **Transition** (0-200 ms): when the *target* changes, the step between targets is shaped separately from
  retune: 0 = instant (staircase), larger = an S-curve glide; the singer's own scoop is kept when the glide is
  slower than the singer's.
- **Humanize** (0-100 %): retune time grows with the age of the note, from the set speed at the onset to
  speed × (1 + 4 × humanize) after ~300 ms, so long notes keep their life while onsets are corrected
  promptly (the behaviour Antares describes for Humanize, §1.3).
- **Synthetic vibrato**: rate 3-9 Hz, depth 0-100 cents, onset delay and fade-in, sine or "natural" (rate and
  depth with slow random variation from the seeded `dh::Rng`), applied to the target.
- **Transpose** ±24 semitones and ±100 cents, applied to the target (formant preserved unless throat moves).
- **Throat** (±1 octave of formant scaling, i.e. vocal-tract length) and **formant amount** (0-100 %).

### 4.5 Snapping against gentle (inferred)

The Style macro sets: Natural (speed 40 ms, transition 60 ms, humanize 40 %, flex 35/70 cents, vibrato keep
100 %, drift keep 30 %, hysteresis 15 cents) to Hard (speed 0, transition 0, humanize 0, flex off, vibrato
keep 0, drift keep 0, hysteresis 0). The front page's Speed and Amount remain independent, so a user can have
Hard style with a 10 ms speed. Values are starting points for listening tests.

### 4.6 Automatic key and scale (inferred)

- **Histogram**: for each stable voiced note (after the centre estimate has settled), add its duration ×
  confidence to its pitch class; exponential forgetting with a 30-60 s time constant so it follows
  modulations slowly. Ignore notes shorter than 120 ms and glides.
- **Matching**: correlate the 12-bin histogram with the 24 rotated major and minor key profiles of Krumhansl
  and Kessler ("Tracing the dynamic changes in perceived tonal organization in a spatial representation of
  musical keys", Psychological Review 89(4):334-368, 1982; from memory). Melodies are sparser than the
  polyphonic music those profiles came from; Temperley's revised profiles (Music Perception 17(1), 1999) or
  corpus-derived ones (Albrecht and Shanahan, Music Perception 31(1), 2013) may fit vocal lines better
  (from memory). Choose by the test T15.
- **Confidence**: margin between the best and second key; relative major/minor are often near ties, so the
  scale suggestion shows both and the user picks.
- **UX**: "Listen" collects for 15-30 s and *proposes*; it never switches the key mid-song by itself unless
  the user enables "follow". This mirrors AutoKey's suggest-and-lock and Topline's key finder (§1.3, §1.9).

---

## 5. The live note view

### 5.1 What it shows (inferred)

A piano roll scrolling right to left (newest at the right edge, 4-15 s visible, zoomable), with:
- the **detected pitch** as a thin line (grey, brighter with confidence; gaps when unvoiced);
- **note blobs**: rounded bars from the centre estimate, opened at onset and closed at offset or target
  change; height proportional to loudness (Melodyne's visual language, which is generic: notes as amplitude
  envelopes); colour by state: corrected, bypassed by flex, out of scale;
- the **target** as a faint lane under each blob;
- the **corrected output** as a bold line;
- vibrato as a shaded band around the centre; a cents readout of the centre's error per blob;
- the current note as a large label and a needle (the cents meter).

### 5.2 Per-note controls that work live (inferred)

Live input cannot be edited before it is sung, so per-note controls are **rules**: clicking a blob opens the
inspector for its pitch class or scale degree (amount, speed, flex, vibrato keep, snap/skip/bypass), and the
rule applies to the next occurrences; the blob shows the rule that was applied. A "pin" on a blob turns it
into a capture region (§5.4). Ctrl-click on the keyboard strip toggles a note in the scale.

### 5.3 Feasibility in a plugin GUI (inferred)

- **Data**: the audio thread writes one record per detector hop (0.67 ms: input sample time, f₀, confidence,
  voiced, centre, target, output pitch, level) into a lock-free single-producer/single-consumer FIFO (about
  1500 records/s, 32 bytes each, 48 KB/s). No allocation, no locks on the audio thread.
- **Drawing**: the editor drains the FIFO at 60 Hz into a ring of 15 s, decimates lines to one point per pixel
  column, and draws with `juce::Path` (a few hundred segments per frame) or the OpenGL context if profiling on
  Windows demands it. Blobs are segmented on the GUI thread from the records, so the audio thread does not do
  GUI work.
- **Timing**: draw against the input time stamps, offset by the reported latency, so the line matches what is
  heard; when the host is playing, align to the host's sample position so a blob stays under its bar.
- **Cost**: well under the editors already in the series (AT20V's rope physics, Halation's views).

### 5.4 Capture mode: the Melodyne-like part (inferred)

1. **Record**: a Capture button records the input (and the host time) into a buffer of up to 60 s, or
   "retro-capture" keeps the last 30 s always and freezes it on request; mono float 48 kHz is 11.5 MB per
   minute.
2. **Analyse** on a worker thread: pYIN-style candidates and Viterbi (§2.2), note segmentation with an HMM of
   attack/stable/release states in the style of the Tony note tracker (Mauch et al., "Computer-aided melody
   note transcription using the Tony software: accuracy and efficiency", TENOR 2015; from memory), then each
   note's centre (median of the stable part), drift and vibrato with the full future available. 30 s analyse
   in well under a second (inferred; T20 measures).
3. **Edit**: drag a blob's pitch; per-note correction amount, drift keep, vibrato keep, transition into the
   note, formant/throat; split and merge; snap all to scale with one "Correct" action with two amounts
   (centre and drift, the familiar macro model); undo.
4. **Play**: when the host transport plays over the captured region, the plugin outputs the edited phrase
   (rendered by the same engine from the buffer, with the Studio interpolator) instead of the live input,
   with a short crossfade at region edges. Outside the region the live path runs. Melodyne's non-ARA transfer
   works on the same principle (§1.2).
5. **Persistence**: the captured audio and the edits are saved in the plugin state (compressed, e.g. FLAC
   through JUCE's codec, ~50 % of the raw size), with a size cap and a warning; a "render to new audio"
   export (WAV) lets the user commit and free the state.
6. **Limits**: no timing edits in v1 (they need time-scale modification; the splice engine can do it by
   repeating and dropping cycles, so v1.1); one region per instance in v1; the host must play the region at
   the same position (time-stamped by the host sample position; if the user moves the audio clip, the region
   must be re-captured). **ARA 2** would remove these limits; the SDK is Apache 2.0 and JUCE supports ARA
   plug-ins, but it is a large extra surface; defer to v2 (§11).

---

## 6. Latency reporting and host behaviour

### 6.1 Reporting

- JUCE: `AudioProcessor::setLatencySamples(int)` from `prepareToPlay` and whenever the mode changes; the
  VST3 and AU wrappers notify the host. Tarnish and Lacquer already do this (`plugins/tarnish/Source/
  PluginProcessor.cpp`, `plugins/lacquer/Source/PluginProcessor.cpp`), and Lacquer's `tools/livetest.cpp`
  reads it back through the real processor (§10).
- The reported value must be **exactly** the minimum content delay D₀ + L of the mode at that sample rate,
  constant while running, the same in VST3, AU and standalone, and the same at every buffer size. Graillon's
  release history (wrong VST3 latency at rates other than 44.1 kHz, AU latency in Reaper, LV2) shows how this
  breaks (§1.6); T1 checks every format and rate.
- **Mode changes** change the latency. Make the latency mode a non-automatable parameter, apply it on the
  audio thread at the next block boundary with a 10 ms crossfade, call `setLatencySamples`, and show a hint
  that some hosts re-compensate only after stopping the transport (inferred; host behaviour varies and is not
  verified per host today).
- **Dry/wet**: the dry path is delayed by the reported latency. Because the wet content delay moves within one
  period while shifting (§3.4), partial mixes comb and flange on corrected notes; the front page has no mix
  knob, the advanced page has one with a warning (inferred).

### 6.2 Monitoring in the main hosts

- **Logic Pro**: plug-in delay compensation delays other tracks, which hurts performing; **Low Latency
  Monitoring mode** routes audio around plug-ins that push a track above a user-set limit in ms (Apple,
  "Manage input monitoring latency in Logic Pro for Mac", https://support.apple.com/HT207527; Logic help
  pages, https://help.apple.com/logicpro/mac/10.4/en.lproj/lgcpe11997ba.html). So the plugin must sit
  below the limit in Live and Tight or Logic will silently bypass it while recording (inferred): document
  the numbers for Logic users.
- **Ableton Live**: delay compensation aligns all tracks to the slowest; "Reduced Latency When Monitoring"
  removes the compensation added to the monitored track but keeps the latency of its own chain; return tracks
  then drift late (Ableton help, "Delay Compensation FAQ", https://help.ableton.com/hc/en-us/articles/209072409;
  "Reduced Latency When Monitoring FAQ", https://help.ableton.com/hc/en-us/articles/209072249). Live 12
  adds a "Keep Latency" toggle on the Monitor setting (Ableton help, https://help.ableton.com/hc/en-us/articles/19450890686876;
  CDM, https://cdm.link/ableton-live-latency/, which places it in 12.1). A forum user reports that Live does
  not recompute latency when a plug-in is deactivated, to avoid glitches on stage
  (https://forum.ableton.com/viewtopic.php?p=1747307; forum, not verified): bypassing a Studio-mode instance
  does not remove its latency, which is one more reason to switch modes instead of stacking instances.
- **Live hosts**: Antares recommends MainStage, Live Professor or Gig Performer with 32-64-sample buffers
  for stage use (§1.3); the standalone build serves the same purpose with the JUCE device selector at the
  lowest buffer the interface allows (inferred).
- **Mixing consoles**: a singer heard her voice "doubled" in in-ears when correction ran as an insert over
  USB; the fix was to feed the in-ears before the insert (Allen & Heath forum, §1.3). Lesson: state clearly
  that the plugin's 0.67 ms is added to the host's round trip, which dominates.

### 6.3 What latency users can work with

- Lester and Boley, "The Effects of Latency on Live Sound Monitoring", AES 123rd Convention, paper 7198,
  2007: the audibility of latency depends on the instrument and the monitoring; acceptable values range from
  42 ms down to below 1.4 ms in some conditions; in-ear monitoring is less tolerant than wedges (AES abstract,
  https://aes2.org/publications/elibrary-page/?id=14256). A secondary summary ranks vocalists as the least
  tolerant, under about 3 ms (audioXpress,
  https://audioxpress.com/article/wireless-audio-ultra-low-latency-audio-transmission-over-bluetooth-le-audio;
  not checked against the paper); a patent citing the same study gives 6.5 ms as "fair" for a vocalist with
  in-ears (https://patents.google.com/patent/FI126465B/en; secondary).
- Shure recommends 5 ms or less for in-ear systems and notes that below that comb filtering with the direct
  (bone-conducted) sound becomes the issue (https://shure.com/en-US/insights/shure-whiteboard-digital-wireless-latency-explained).
- Antares' blog: under 10 ms most people cannot detect it, 20-30 ms feels slightly off, above 40 ms disrupts
  timing (vendor blog, https://www.antarestech.com/blog/low-latency-monitoring-in-daws-what-it-is-and-why-it-matters).

Conclusion (inferred): for a vocalist on in-ears the *whole* chain should stay under ~5 ms; the interface and
buffer take most of that, so the plugin must add ≤ 1 ms (Live). Tight (2.7 ms) is acceptable on wedges and
for most studio tracking; Studio (21 ms) is for mixing. Comb filtering is at its worst when the correction is
near zero (the corrected voice equals the bone-conducted one), which argues for Live whenever the singer
hears themself.

---

## 7. Test plan (automated, JUCE-free where possible)

In `plugins/<name>/tools/dsptest.cpp` (built like the other plugins' `dsptest.cpp`: `clang++ -std=c++20 -O2
-I plugins/<name>/Source/dsp -I shared/dsp`), plus `<name>render` (a WAV through a preset to WAV with a CPU
figure, like AT303's `render.cpp`) and a `livetest.cpp` that drives the real `AudioProcessor` (Lacquer's
pattern) for latency and state. All test signals are generated in the test or by the scripts below; ground
truth is known by construction. 48 kHz unless stated.

### 7.1 Test signals without copyrighted material

- **Glottal-pulse vowel synth** (`tools/voicesynth.cpp`): an LF-model glottal flow derivative (Fant,
  Liljencrants and Lin, "A four-parameter model of glottal flow", STL-QPSR 26(4), 1985; from memory) or a
  Rosenberg pulse; jitter 0.5-1 % and shimmer 2-4 % (typical healthy-voice ranges; from memory); aspiration
  noise; a cascade of formant resonators (Klatt, "Software for a cascade/parallel formant synthesizer", JASA
  67(3), 1980; from memory) with the Peterson and Barney vowel formants that ATFS1's `Fseq.h::vowelSequence()`
  already contains; F₀ contours from a script: steady notes, scoops into notes (−80 cents over 60 ms), vibrato
  5-7 Hz ±20-80 cents with rate and depth drift, glides, octave leaps, legato melismas, vocal-fry segments
  (period doubling), sibilants (noise band-passed 4-9 kHz), breaths (low-passed noise), silence. Male (85-180
  Hz), female (165-350 Hz), child/soprano (250-1000 Hz) and bass (65-130 Hz) variants. The script writes the
  exact F₀ track and the exact epoch times alongside the WAV.
- **espeak-ng** (on our Linux box): sung-like phrases by setting pitch and range per word; useful for
  consonant/vowel transitions that our synth lacks. Use only its built-in formant voices: the MBROLA voice
  databases have their own, more restrictive licences (from memory; check before use). espeak-ng is GPL-3; its
  *output* audio is data we generate, not a derivative of the program (inferred; not legal advice). Ground
  truth for espeak-ng phrases comes from a reference detector run offline on the clean phrase (pYIN with a
  long window), not from the synthesiser.
- **Pure tones and harmonic stacks**: sines, sawtooths, missing-fundamental stacks, a strong-2nd-harmonic
  stack (octave trap).
- **Noise**: white/pink noise at −30 to −10 dB SNR added to the vowels; room reverb from Halation's engine.
- **Own recordings**: team members singing scales and phrases, released under CC0 in a separate test-asset
  download (not in the repository); public datasets only if their licence allows redistribution in an AGPL
  project's test suite (MIR-1K, PTDB-TUG, MDB-stem-synth have research or NC licences to check; from memory).

### 7.2 Tests

| # | Test | Pass criterion |
|---|---|---|
| T1 | Reported against measured latency | For each mode × {44.1, 48, 88.2, 96, 176.4, 192} kHz × {VST3, AU, standalone wrapper, engine}: an impulse train and a white-noise burst through the plugin with correction off and with an in-tune input: the cross-correlation peak is at exactly the reported latency (±0 samples); the reported value equals §0's table |
| T2 | Content delay bounds while shifting | Synthetic vowel at 110, 220, 440 Hz with corrections of ±30, ±100, ±700, ±1200 cents: the instantaneous delay (from epoch alignment against the known source epochs) stays within [reported, reported + one period + crossfade] at every sample; returns to the reported value within 50 ms of the next unvoiced segment |
| T3 | Engine identity | Ratio forced to 1 and throat 0 on 60 s of synthetic voice, noise and music-like signals: output equals input delayed by the reported latency with a residual below −100 dBFS (the LPC lattice pair inverts exactly); with formant preservation off, bit-exact |
| T4 | No NaN/Inf, no denormal slowdown | Silence ≥ 60 s, DC, full-scale square, white noise, 200 seeded random extreme settings re-randomised every block, every sample rate: no NaN/Inf (checked by bit pattern, `dh::isFiniteF`, since `-ffast-math` may drop `std::isfinite`), peak ≤ +12 dBFS, CPU in silence within 10 % of CPU with signal; reflection coefficients always |k| < 1 |
| T5 | Block-size determinism | Renders with blocks of 1, 7, 64, 512, 4096 and seeded random sizes are bit-identical (the detector hop and LPC updates are scheduled on an absolute sample counter, never per block); two runs and a run after `reset()` are bit-identical; same output on macOS arm64 and Windows x64 within 1 LSB at 24 bits (`-ffp-contract=off` on the engine, as Lacquer does) |
| T6 | Detection accuracy | On the synthetic set (every preset's range): median absolute error ≤ 2 cents and 95th percentile ≤ 8 cents on voiced frames after lock; voicing decision accuracy ≥ 97 % against the synthesiser's labels; on pure tones 60-1500 Hz ≤ 0.5 cent |
| T7 | Octave errors | Gross errors (> 50 cents from truth or an octave) ≤ 0.5 % of voiced frames on the full set, ≤ 1 % on the strong-2nd-harmonic and vocal-fry sets; zero sustained (> 20 ms) octave errors on the 1000-note melody set |
| T8 | Detection lag | Step changes of 1, 2, 5, 12 semitones inside a sustained vowel and note onsets after silence: lock time per preset and mode within the §2.6 table + 20 %; with prediction on, the lag on 6 Hz vibrato is reduced by ≥ 30 % without overshoot above 10 cents |
| T9 | Correction accuracy (hard) | Speed 0, Hard style, scale chromatic: measured output pitch (offline long-window detector on the output) within ±3 cents (median) and ±8 cents (95th percentile) of the target on sustained voiced frames, at shifts up to ±100 cents; target steps complete within one detector hop after the detector reports the change |
| T10 | Correction accuracy (natural) | Natural style on vibrato notes mistuned by 10-45 cents: the mean of the output over each vibrato cycle within ±5 cents of the target; the output's vibrato extent within ±10 % of the input's and its rate unchanged; drift reduced by (1 − drift keep) ± 10 % |
| T11 | Retune, transition, humanize, flex | Step response of the correction: 95 % at the set retune time ± 10 %, no overshoot above 2 % at any speed; transition times ± 10 %; humanize slows sustained notes as specified; notes outside the flex window are left within ±2 cents of the input |
| T12 | Formants | Synthetic vowels shifted ±1, ±3, ±7, ±12 semitones with preservation on: the first three formant frequencies (from LPC on the output) within ±5 % of the input's for f₀ ≤ 350 Hz, ±10 % up to 600 Hz; throat ±2, ±5, ±12 semitones moves F1-F3 by the requested ratio ± 5 %; with preservation off they move with the pitch |
| T13 | Splice artefacts | At every splice instant (logged by the engine): the energy above 8 kHz in a 1 ms window ≤ 3 dB above the mean of the neighbouring periods; output periodicity (YIN aperiodicity of the output) no worse than the input's + 0.03; spectral flatness of the output over voiced frames within 10 % of the input's; no sample-to-sample difference above 4 × the input's maximum (click detector); compare Hermite and sinc interpolators at −12 semitones for aliasing (components not harmonic of the target below −70 dB) |
| T14 | Unvoiced and breaths | Sibilant and breath segments pass with ratio 1 (output equals delayed input within −60 dB) after the voicing decision; in Tight/Studio, sibilant onsets are never shifted; no tonal artefacts (no spectral peak > 10 dB above the noise floor) in shifted fricatives |
| T15 | Auto key | On 200 generated melodies (seeded, diatonic with 0-10 % chromatic notes, in all 24 keys): correct key or its relative after 20 s ≥ 90 %; exact key ≥ 75 %; the K-K / Temperley / corpus profile choice made from this test |
| T16 | MIDI targets | Note lock follows MIDI at the event's sample offset (±1 hop); pitch bend range exact; scale-from-held-notes and Auto mode switch as specified; `.mid` files read with `shared/tools/MidiEvents.h` |
| T17 | Mode switching and state | Switching modes during playback: no clicks above −40 dBFS, latency reported updated; save/restore round trip bit-identical, including capture regions and edits; presets load without NaN |
| T18 | Capture | Capture → no edit → play: output equals the live output of Studio mode on the same audio within −80 dB; edits move the measured pitch of the edited note by the requested amount ± 3 cents; region edges crossfade without clicks; state size within the cap |
| T19 | CPU | `<name>render --bench`, 48 kHz, 64- and 512-sample blocks, mono voice, every mode, on the Apple M4 and the Ryzen 9 5900XT: within §3.6's targets; numbers recorded with the date in the README |
| T20 | Capture analysis time | 30 s phrase analysed in ≤ 1 s on the M4 and ≤ 1.5 s on the 5900XT on the worker thread, with the audio thread unaffected (no xruns at 64 samples in the standalone) |

**Not automated**: pluginval at strictness 8 and `auval -v aufx` for every format, as for every product in
the series; host matrix by hand (Logic with Low Latency Monitoring, Ableton Live 12 with and without Keep
Latency, Reaper, Cubase, Bitwig, FL Studio) checking that the delay compensation matches T1; blind listening
with 3-5 listeners on our own recordings: natural style against the input (is it audibly corrected? does
the vibrato survive?), hard style against the reference behaviour users expect, and singers on in-ears in
Live and Tight ("can you hear a delay?").

---

## 8. UI direction

### 8.1 Constraints (inferred)

- No competitor imitation: no yellow-on-black "auto" knob, no Melodyne-style blob colours or toolbars, no
  copied layouts; notes-as-blobs and piano rolls are generic.
- The series' product images are 3D renders (Blender, as for ATMD and the AT instruments); the original
  products (Orrery, Murmur, Lacquer, Halation, Tarnish) are named after objects or materials, so the
  identity should be an object the render can show.
- Front page usable in 10 seconds: key, voice, amount, speed, style, mode, and the live view.
- Resizable; the live view must stay readable at 900 × 560.

### 8.2 Three directions

**A. Plumb bob over slate** (recommended). The render: a brass plumb bob on a fine cord hanging over a dark
slate engraved with staff lines and note names. The bob swings with the pitch error of the current note (a
damped pendulum driven by the cents error) and hangs dead still when the corrected output is on target; the
slate is the live note view, with the blobs engraved in light and the corrected line in warm brass. Front
page: key and scale chips across the top, four large controls (Amount, Speed, Style, Voice), a three-way
mode switch labelled with the latency in ms, and the slate below at full width. Advanced page: the slate
fills the window as the piano roll; a right-hand inspector for the selected blob or degree; capture tools in
a strip above. Fits the name Plumb (§9) and the "true vertical" metaphor of tuning.

**B. Strobe disc.** A rendered stroboscopic tuning disc (a generic principle: bands that appear to stand
still when the frequency matches) that rotates with the pitch error and freezes when corrected; good for a
single-note readout, weaker as a container for the piano roll, and closer to existing tuner products'
look.

**C. Glass score.** Mostly flat: a dark glass panel with the piano roll as the hero and rendered glass caps
on the controls only; the most functional, the least distinctive inside a series of rendered objects.

**Recommendation**: A, with C's discipline for the advanced page (the piano roll is a flat, high-contrast
surface; the render frames it).

### 8.3 Interaction details (inferred)

- Mode switch shows "0.7 ms / 2.7 ms / 21 ms" computed from the current sample rate.
- Hovering a blob shows its centre error, vibrato rate/extent and the rule applied.
- The keyboard strip at the left of the roll toggles notes in the scale; MIDI notes held light up the strip.
- "Listen" for auto key shows a progress ring and the two best candidates.
- Compare: level-matched A/B of corrected and input at the same latency.
- Colour-blind-safe palette for corrected / bypassed / out-of-scale (luminance differences, not hue only).

---

## 9. Name ideas

Short original English words for an object or a craft term, like the rest of the series' own products.
Collision search on 2026-10-19 (KVR and Plugin Boutique through a site-restricted web search, plus a general
web search; **KVR's own search returned 403 to scripts**, and no trademark registry was searched):

| Candidate | Why | Collision found | Verdict |
|---|---|---|---|
| **Plumb** | A plumb line marks true vertical; "plumb" also means exactly, squarely: a voice brought true. Short, a tool, pairs with Lacquer and Tarnish as a craft word | None as a product (the only hit is "Plum", a plug-in API spec discussed on KVR) | Free |
| **Aplomb** | Composure, from "à plomb" (perpendicular by the plumb line): confidence on stage | None as a product (only the word in review copy) | Free |
| **Keel** | What keeps a boat steady and upright; also "on an even keel" | None found | Free |
| **Gimbal** | The mount that keeps a compass or camera level whatever moves around it | None found (nearest: "GAINBAL" by BARI Audio) | Free |
| **Truing** | Making a wheel or a blade true: small, precise corrections | None found | Free; less familiar word |
| **Sextant** | The navigator's instrument for an exact angle to a fixed star | A preset pack "Ivory Sextant" for another synth | Free (minor) |
| **Gnomon** | The pin of a sundial that casts the true line | None found (nearest: MAAT "GŌN") | Free; hard to pronounce for some |
| Vernier | The fine-reading scale on a caliper | No audio product, but Vernier Software & Technology sells science software and sensors under that name (from memory) | Risky in software |

Rejected: **Burnish** (Vitric Audio's "Burnish Vocal Chain", which includes pitch correction,
https://www.kvraudio.com/product/burnish-vocal-chain-by-vitric-audio, and a mastering app of the same name);
**Lodestar** (Lodestar Audio, a developer of pitch and doubler plug-ins,
https://www.kvraudio.com/product/basalt-by-lodestar-audio); **Strobe** (an existing synth line, from
memory); **Temper** (an existing distortion plug-in, from memory); **Cantor** (too close to Antares' Kantos,
from memory); anything with "tune", "auto", "pitch" or "talent" (generic or too close to existing products).

**Recommendation** (inferred): **Plumb** first (short, unclaimed, a visible object for the render, the
right meaning); **Aplomb** second (the same root with a performance meaning, but harder to spell); **Keel**
third. Before deciding: USPTO and EUIPO in class 9, KVR's product search by hand, and app stores.

---

## 10. Reusing the repository's code

| Existing code | Use here |
|---|---|
| `shared/dsp/Common.h`: `fastExp2` (cents to ratio, 0.0004 cent), `isFiniteF`, `Smoother`, `DcBlocker`, `OnePoleLP`, `Rng` (seeded), `clampf` | Directly: ratio computation, side-chain filters, parameter smoothing, NaN guards, seeded synthetic vibrato |
| `shared/dsp/Fx.h`: `DelayLine` (linear-interpolated `tap`, integer `tapInt`, the wrap fix for tiny negative positions) | Base of the residual buffer; add a Hermite and a windowed-sinc tap; keep its wrap fix |
| `shared/dsp/Fx.h`: `SvfBand` (TPT band-pass) | Vibrato-band filter on the pitch track; sibilant band energy |
| `shared/dsp/Oversampling.h`: `HalfbandFir`, `Decimator` | Candidate for the detector's decimation; measure its group delay (it adds to the correction lag, not to the audio latency); an elliptic IIR may be better |
| `plugins/at20v/Source/dsp/Modules.h`: YIN on a decimated (~12 kHz) ring, threshold 0.15, first dip, parabolic interpolation, envelope gate | Starting point of the detector; generalise (incremental difference, tracking window, presets, confidence) and move to `shared/dsp/Pitch.h` so AT20V can use the improved version |
| `plugins/atfs1/Source/dsp/Fseq.h`: Levinson-Durbin `lpc()`, FFT, LPC root finding for formants, `vowelSequence()` with Peterson-Barney formants | LPC analysis (move `lpc()` to shared), throat by pole-angle scaling, and the synthetic test voices |
| `plugins/halation/Source/dsp/Modules.h`: two-head granular `PitchShifter` (90 ms grains) | Not suitable for the audio path (latency, not pitch-synchronous); a reference for the doubler voice in v1.1 |
| `plugins/lacquer/tools/livetest.cpp` | Pattern for T1/T17: drive the real processor, read `getLatencySamples()`, compare |
| `plugins/tarnish` / `plugins/lacquer` `setLatencySamples` usage and Lacquer's determinism checklist (`-ffp-contract=off`, fixed sub-blocks) | Latency reporting and T5 |
| `shared/tools/MidiEvents.h` | MIDI-target tests from `.mid` files |
| `shared/tools/SafeString.h`, `shared/manual/` | Preset name across threads; the bilingual manual like the other products |
| `shared/dsp/Blep.h` | Not needed |

---

## 11. Decisions to make

1. **Name** (§9): Plumb, Aplomb or Keel, after the registry checks; rename the folder `plugins/autotune`.
2. **Default mode**: Tight (recommended: the best balance for studio tracking) or Live (the request's focus).
3. **Studio engine**: the same splice engine with lookahead (recommended for v1) or TD-PSOLA, chosen by T12-T13
   and listening.
4. **Formant path for high voices**: fade to waveform splicing above ~400 Hz (recommended) or reduced-order LPC
   only.
5. **Throat method**: pole-angle scaling (recommended) or synthesis-warping.
6. **Interpolators**: Hermite in Live/Tight, 16-tap sinc in Studio (recommended), or sinc everywhere with a
   larger D₀.
7. **Key profiles** for auto key: decided by T15.
8. **Capture in v1.0** (recommended, pitch-only) or v1.1.
9. **ARA 2**: v2, or never.
10. **Neural detector** (PESTO-like) for capture analysis in v2: only with a training-data licence compatible
    with AGPL.
11. **Detected pitch to MIDI out** and **doubler/harmony voices**: v1.1.
12. **Test assets**: who records the CC0 vocal set, and where it lives (not in the repository).
13. **Patents**: a freedom-to-operate read of active Antares, Celemony and Waves patents on real-time
    correction features (flex/humanize-style behaviour, key detection), beyond the expired US 5,973,252 and
    the polyphonic US 8,022,286 we avoid (not legal advice).
14. **Follow-up research**: forum sweeps (Gearspace, Reddit r/audioengineering and r/WeAreTheMusicMakers, KVR)
    on live correction complaints; measured latency of Graillon 3, MAutoPitch, Little AlterBoy and Pro 11 in
    our own host (a 10-minute loopback test each would settle what no source states).

---

## Sources

Accessed 2026-10-19 unless stated.

Products, manuals and reviews:
- Celemony help centre, "Transferring audio": https://helpcenter.celemony.com/M5/doc/melodyneEssential5/en/M5tour_TransferARA?env=live
- Celemony help centre, "Pitch modulation and drift": https://helpcenter.celemony.com/M5/doc/melodyneStudio5/en/M5tour_ToolModulationDrift_2
- Celemony, "10 years of ARA": https://www.celemony.com/en/10-years-of-ara; ARA SDK open source: https://rekkerd.org/celemony-releases-ara-sdk-under-open-source-license/
- Melodyne 5: Sound On Sound news https://www.soundonsound.com/news/melodyne-5-unveiled; MusicTech https://musictech.com/reviews/the-big-review-celemony-melodyne-5/; MusicRadar https://musicradar.com/reviews/celemony-melodyne-51; Mix https://www.mixonline.com/?p=107306; tutorial https://www.nrgrecording.com/post/tuning-vocals-with-melodyne
- US 8,022,286 (Celemony): https://patents.google.com/patent/US8022286; https://patents.justia.com/inventor/peter-neub-cker; Sound On Sound interview https://www.soundonsound.com/people/peter-neubacker-melodyne-celemony-next-decade
- US 5,973,252 (Hildebrand, Auburn Audio Technologies): https://patents.google.com/patent/US5973252A/en
- Sound On Sound, "Antares AutoTune 2026" (J. Walden, January 2026): https://www.soundonsound.com/reviews/antares-autotune-2026; Auto-Tune Pro review https://www.soundonsound.com/reviews/antares-auto-tune-pro
- Antares blog, live use of AutoTune 2026: https://www.antarestech.com/blog/real-time-pitch-correction-for-live-performance-with-autotune-2026; low-latency monitoring: https://www.antarestech.com/blog/low-latency-monitoring-in-daws-what-it-is-and-why-it-matters; support, Low Latency mode: https://help.antarestech.com/hc/en-us/articles/47201781644948-Low-Latency-Mode-Not-Working
- MixingGPT roundup (secondary): https://mixinggpt.com/blog/autotune-advanced-vs-autotune-pro-11-2026; Full Compass listing (Artist): https://fullcompass.com/prod/576149-antares-auto-tune-artist-low-latency-pitch-correction-effects-virtual
- Attack Magazine: https://www.attackmagazine.com/technique/tutorials/twisting-up-a-synth-drone-with-antares-auto-tune-pro/; Produce Like A Pro: https://producelikeapro.com/blog/how-to-use-auto-tune-vocal-tuning/
- Waves Tune Real-Time reviews: https://and.barryrudolph.com/newtoys/toys16/waves_tune_real-time.html; https://www.musicconnection.com/music-gear-waves-tune-real-time/; Waves forum https://forum.waves.com/t/problem-with-delay-effect-when-using-waves-tune-real-time/3900
- Nectar reviews: https://barryrudolph.com/mix/izotopenectar.html; https://ra.co/reviews/8667
- Graillon 3 User's Guide: https://www.auburnsounds.com/downloads/Graillon%203%20User's%20Guide.pdf; MusicTech https://musictech.com/news/gear/auburn-sounds-graillon-3/; Bedroom Producers Blog https://bedroomproducersblog.com/2016/09/14/graillon-voice-octaver/; MuseHub https://www.musehub.com/learn/best-autotune-plugins
- MAutoPitch: https://www.meldaproduction.com/MAutopitch; https://audiopluginlist.com/meldaproduction-mautopitch-review/
- GSnap manual: https://www.gvst.co.uk/gsnap_manual.htm
- Little AlterBoy: https://blog.dubspot.com/plugins/compare/blepfx-spectra-vs-little-alterboy; https://www.thomann.it/soundtoys_little_alterboy_reviews.htm
- ReaTune: https://ProRec.com/?p=6487; https://ProRec.com/?p=6396; https://www.cockos.com/wiki/index.php/ReaTune
- zita-at1 quick guide: https://kokkinizita.linuxaudio.org/linuxaudio/zita-at1-doc/quickguide.html
- x42-autotune (fat1): https://x42-plugins.com/x42/x42-autotune
- Autotalent: http://tombaran.info/autotalent.html
- TalentedHack: https://lists.linuxaudio.org/archives/linux-audio-user/2010-July/071617.html; https://alternativeto.net/software/talentedhack/about/
- UAD Topline Vocal Tune: https://www.production-expert.com/production-expert-1/uad-topline-vocal-tune-released-ultra-low-latency-native-pitch-correction
- Vitric Audio Burnish Vocal Chain: https://www.kvraudio.com/product/burnish-vocal-chain-by-vitric-audio
- Antelope InTune Performer: https://en.antelopeaudio.com/product/intune-performer/
- Allen & Heath forum (in-ear doubling): https://forums.allen-heath.com/t/use-autotune-as-direct-insert-but-skip-autotune-for-in-ears/12693

Hosts and monitoring:
- Apple, "Manage input monitoring latency in Logic Pro for Mac": https://support.apple.com/HT207527; Logic help https://help.apple.com/logicpro/mac/10.4/en.lproj/lgcpe11997ba.html
- Ableton help: https://help.ableton.com/hc/en-us/articles/209072409, https://help.ableton.com/hc/en-us/articles/209072249, https://help.ableton.com/hc/en-us/articles/19450890686876; CDM https://cdm.link/ableton-live-latency/; Ableton forum https://forum.ableton.com/viewtopic.php?p=1747307
- M. Lester, J. Boley, "The Effects of Latency on Live Sound Monitoring", AES 123rd Convention, paper 7198, 2007: https://aes2.org/publications/elibrary-page/?id=14256; summaries https://audioxpress.com/article/wireless-audio-ultra-low-latency-audio-transmission-over-bluetooth-le-audio, https://patents.google.com/patent/FI126465B/en
- Shure, digital wireless latency: https://shure.com/en-US/insights/shure-whiteboard-digital-wireless-latency-explained

Pitch detection, shifting and voice (bibliographic; "from memory" items were not fetched today):
- A. de Cheveigné, H. Kawahara, "YIN, a fundamental frequency estimator for speech and music", JASA 111(4):1917-1930, 2002
- P. McLeod, G. Wyvill, "A smarter way to find pitch", ICMC 2005; TarsosDSP documentation https://0110.be/releases/TarsosDSP/TarsosDSP-2.3/TarsosDSP-2.3-Documentation/be/tarsos/dsp/pitch/McLeodPitchMethod.html
- M. Mauch, S. Dixon, "pYIN: a fundamental frequency estimator using probabilistic threshold distributions", ICASSP 2014
- M. Mauch et al., "Computer-aided melody note transcription using the Tony software: accuracy and efficiency", TENOR 2015
- A. Camacho, J. G. Harris, "A sawtooth waveform inspired pitch estimator for speech and music", JASA 124(3), 2008
- A. M. Noll, "Cepstrum pitch determination", JASA 41(2), 1967
- J. W. Kim, J. Salamon, P. Li, J. P. Bello, "CREPE: a convolutional representation for pitch estimation", ICASSP 2018: https://arxiv.org/abs/1802.06182; MathWorks CREPE block https://www.mathworks.com/help/audio/ref/crepeblock.html
- A. Riou et al., "PESTO: real-time pitch estimation with self-supervised transposition-equivariant objective", TISMIR 2025: https://arxiv.org/abs/2508.01488; Sony CSL https://cslmusicteam.sony.fr/news/new-paper-pesto-a-novel-method-for-pitch-estimation-with-a-self-supervised-transposition-equivariant-objective/
- P. A. Naylor et al., "Estimation of glottal closure instants in voiced speech using the DYPSA algorithm", IEEE TASLP 15(1), 2007; T. Drugman et al., "Detection of glottal closure instants from speech signals: a quantitative review", IEEE TASLP 20(3), 2012
- E. Moulines, F. Charpentier, "Pitch-synchronous waveform processing techniques for text-to-speech synthesis using diphones", Speech Communication 9(5-6), 1990
- K. Lent, "An efficient method for pitch shifting digitally sampled sounds", Computer Music Journal 13(4), 1989
- W. Verhelst, M. Roelands, "An overlap-add technique based on waveform similarity (WSOLA) for high quality time-scale modification of speech", ICASSP 1993
- J. Laroche, M. Dolson, "Improved phase vocoder time-scale modification of audio", IEEE TSAP 7(3), 1999
- A. Röbel, X. Rodet, "Efficient spectral envelope estimation and its application to pitch shifting and envelope preservation", DAFx-05
- A. Härmä et al., "Frequency-warped signal processing for audio applications", JAES 48(11), 2000
- G. Fant, J. Liljencrants, Q. Lin, "A four-parameter model of glottal flow", STL-QPSR 26(4), 1985; D. H. Klatt, "Software for a cascade/parallel formant synthesizer", JASA 67(3), 1980; G. E. Peterson, H. L. Barney, "Control methods used in a study of the vowels", JASA 24(2), 1952
- C. L. Krumhansl, E. J. Kessler, Psychological Review 89(4):334-368, 1982; D. Temperley, Music Perception 17(1), 1999; J. Albrecht, D. Shanahan, Music Perception 31(1), 2013
- E. Prame, "Measurements of the vibrato rate of ten singers", JASA 96(4), 1994; J. Sundberg, *The Science of the Singing Voice*, 1987

Name checks: https://www.kvraudio.com/product/burnish-vocal-chain-by-vitric-audio; https://www.kvraudio.com/product/basalt-by-lodestar-audio; https://www.kvraudio.com/product/cherry-audio-esq-1-presets-ivory-sextant-by-vicious-antelope; https://www.kvraudio.com/product/gainbal-by-bari-audio; https://www.kvraudio.com/news/maat-releases-gon---free-audio-visualization-plug-in-49692; https://www.kvraudio.com/forum/viewtopic.php?t=572480
