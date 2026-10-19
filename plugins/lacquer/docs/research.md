# Lacquer: research for an automatic "easy master" plugin and app

> Written before the product had its name: "ATMaster" below is Lacquer (the paths are already updated).
> Where the shipped engine differs from the proposal, `../README.md` says how and why
> (parallel multiband, release toward the average needed reduction, Learn that listens to the whole song).

Compiled 2026-10-18 for a new Audio Tools product: a free, open-source (AGPL-3.0), offline, deterministic
automatic mastering tool in the spirit of DistroKid Mixea, LANDR, iZotope Ozone's Master Assistant, BandLab
Mastering, eMastered and Logic's Mastering Assistant. A musician inserts it on the master bus (or drops a
finished mix into the standalone app), picks a style, an intensity and a loudness target, and gets a
release-ready master. Advanced controls are optional. No cloud.

Conventions:
- Every fact carries its source inline (URL, paper, page or section). Standards (BS.1770-5, EBU R 128,
  Tech 3341/3342/3344, R 128 s2) and source code (libebur128, pyloudnorm, FFmpeg, Matchering, x42-dpl,
  Airwindows, CTAGDRC) were read directly. Where only an abstract, a search snippet or a third-party quote
  was available, the text says so.
- **(inferred)** marks my own conclusions, calculations or design proposals. Section 6 is a design
  proposal throughout; its numbers are starting values to tune with the tests in §6.10.
- Product names are trademarks of their owners. Nothing here copies proprietary curves or code; the
  target curves are built from our own measurements (§3.3).
- Nothing here is legal advice. Where a patent or dataset licence matters, the doc says to read it.

---

## 0. Summary of recommendations

**What the market sells** (§1). Mixea is just two controls: Intensity (mostly compression) and a Brighter ↔
Warmer EQ slider. LANDR offers Warm / Balanced / Open × Lo / Med / Hi. Logic offers Transparent / Clean /
Valve / Punch characters, plus Loudness, an Auto-EQ amount and Width. Ozone's Assistant listens to the
loudest ≥ 30 s, blends genre target curves into one EQ target, turns dynamics on only when the crest factor
is outside 3-10 dB, and drives a true-peak maximizer to a loudness set by destination. Users value speed and
loudness-matched A/B. They complain about over-compression and pumping, over-brightening of mixes that are
already bright, hot peaks, missed loudness targets, inconsistency between tracks, and opacity. In the largest
public blind test (472 listeners), the open-source DSP matcher Matchering placed just behind two human
engineers and ahead of Ozone and LANDR.

**Chain** (§6.2), in order:
1. Input trim, DC blocking and an automatic sub-sonic HPF.
2. Static minimum-phase tonal EQ: a match to our style target curve plus a style tilt.
3. Dynamic EQ: harshness at 2-5 kHz and sibilance at 5-10 kHz.
4. Optional 3-band LR4 multiband "glue" (120 Hz / 2.5 kHz, with all-pass compensation).
5. Gentle bus compressor (Giannoulis smooth decoupled log-domain detector, 1.5-2 : 1).
6. Oversampled saturation for "warm".
7. M/S stage: mono below ~120 Hz, small guarded width changes.
8. Optional oversampled soft clipper.
9. Lookahead true-peak limiter: moving-minimum plus box smoothing, two-stage release, a low-frequency hold,
   and an oversampled detector.
10. Dither.

Loudness is set by the gain into stages 8-9 only, so intensity and target stay independent.

**Analysis** (§6.4).
- Measure everything on the "loud mask", the 3 s blocks within 3 LU of the integrated loudness:
  - BS.1770-5 LUFS-I/S/M, LRA, true peak, PLR/PSR;
  - 1/3-octave LTAS of mid and side;
  - per-band crest factor and onset rate;
  - per-band correlation and side/mid ratio;
  - harshness and sibilance indices.
- The plugin measures this over a Learn pass (30 s of the loudest part). The app measures the whole file and
  renders in two passes, with one or two secant iterations on the output loudness.
- Decision rules:
  - EQ: the difference between the style target and the measured LTAS, smoothed, confidence-weighted (nothing
    below 30 Hz or above 16 kHz), with a dead zone inside the target's P10-P90 band, scaled by intensity and
    clamped to ±4 dB.
  - Dynamics: the gap between the measured PLR and the PLR the target needs, shared out under per-stage GR
    budgets (limiter first, then clipper, glue, multiband). If the input is already limited, every stage
    except the safety limiter is turned off.
  - Width: a step toward the target side/mid ratio, never letting correlation drop below zero.

**Targets** (§2, §6.7).
- Loudness presets: Streaming −14 LUFS-I / −1 dBTP (default), Apple −16 / −1, Loud −11 / −2, Club −8 / −2,
  CD −9 / −0.3, R128 −23 / −1, and "match input".
- The ceiling switches to −2 dBTP automatically when the target is louder than −14 LUFS (Spotify's own
  guidance).
- Meters follow BS.1770 exactly: coefficients at any sample rate are derived and verified in §2.1. The
  limiter detector is stricter than the meter, because a 4× meter can under-read by up to 0.69 dB.
- Tonal targets are our own curves: first a parametric seed taken from Pestana et al.'s published slopes,
  then the median and P10/P90 of a CC-licensed corpus (FMA), grouped by percussiveness and brightness (after
  Elowsson & Friberg).

**ML verdict** (§5).
- The core uses no ML. It is a deterministic DSP chain driven by measured features, style targets and a rule
  table. That is what the commercial assistants turn out to be, and it is reproducible, explainable, light on
  CPU and clean to license.
- Licence check of the open models:
  - Not usable (non-commercial terms): DeepAFx-ST and Diff-MST.
  - Weights of unclear provenance: Koo et al.'s models.
  - Permissive but useless here: SonicMaster (0.86 B parameters, generative, GPU-only).
- If needed, a tiny feature-based classifier, or a ≤ 10 k-parameter MLP run in RTNeural, may *suggest* a
  style.
- Natural language ("warmer, more punch") should go through a deterministic keyword grammar. An opt-in Ollama
  bridge in the standalone app could map free text onto that grammar, but never onto raw parameters.

**Tests** (§6.10).
- Meter conformance against the EBU sets.
- |LUFS-I − target| ≤ 0.5 LU when rendering.
- TP ≤ ceiling on two independent meters.
- Decoded AAC/MP3 output ≤ 0 dBFS.
- Spectral distance to target reduced.
- An envelope-modulation pumping index.
- Mono compatibility.
- A null test at zero intensity.
- Bit-identical output across block sizes.
- Sample-rate independence, CPU budget, and robustness.

**Test material** (§7).
- First set: our own multitracks, rendered lossless from the Audio Tools plugins with synthesised drums and
  with controlled defects whose correct fix is known.
- Public material we can commit: Slakh2100 (CC BY 4.0), Freesound CC0 and FMA CC-BY clips.
- Local listening only, never committed: Cambridge-MT, MUSDB18 and MedleyDB (non-commercial or educational
  terms).
- The existing web clips are AAC, already normalised to −16 LUFS and single-instrument, so they are only
  good for robustness checks.

---


## 1. What the commercial products actually do

None of these vendors publishes its algorithm. What follows is what their own pages, manuals, reviews and
(for LANDR) a patent say. Treat reviewer measurements as single data points.

### 1.1 DistroKid Mixea

- **Origin.** Launched 2023-02-23, credited to DistroKid itself; no external vendor or acquisition is
  named anywhere ([Hypebot 2023-02](https://www.hypebot.com/hypebot/2023/02/distrokid-adds-mixea-ai-powered-music-mastering-service.html)).
  `mixea.com` now redirects to [distrokid.com/mixea](https://distrokid.com/mixea).
- **Controls.** Not Warm/Balanced/Open (those are LANDR's). The official page shows two sliders:
  **Intensity** (Low → High) and **EQ** (Brighter → Warmer) ([distrokid.com/mixea](https://distrokid.com/mixea)).
  Ari Herstand's hands-on review counts 3 intensities × 4 EQ positions (Brighter … Neutral … Warmer) = 12
  combinations, and observes that intensity "essentially means compression" because waveforms flatten as it
  rises ([Ari's Take](https://aristake.com/distrokid-mixea-mastering-review/)).
- **Claimed processing.** "Optimizes bass, compression, stereo enhancement, EQ, limiting, and loudness"
  (Hypebot, above). No LUFS target is published; the page says only "optimized for streaming services".
- **Flow.** Upload → preview the whole song (not a 30 s clip) with a Mastered/Original toggle →
  "Preview different sound settings, pick your favorite" → download. No source says the A/B is
  loudness-matched.
- **Output.** 24-bit / 48 kHz WAV (official page and Hypebot). Input WAV, FLAC, MP3, M4A, AIFF.
- **Price.** First track free, then USD 99/year unlimited.
- **Reviewer complaints.** Less dynamic than a human master ("lows didn't thump as hard… highs didn't
  sparkle as much"), no album-level consistency, no reference upload (Ari's Take).

**Takeaway for us:** the market-leading "easy" product is literally two controls: an intensity
(mostly dynamics) and a tilt (bright ↔ warm). That is the minimum UI we must match.

### 1.2 LANDR

- **Origin.** Formerly MixGenius; the technology came out of Queen Mary University of London (C4DM)
  research ([Vice interview](https://www.vice.com/en/article/6vwpbr/landr-technology-interview)); first
  automated mastering service, 2014 ([SOS review](https://www.soundonsound.com/reviews/landr-mastering-plugin)).
- **Styles** (Tonic engine, 2019): **Warm** (vintage, softer compression, fuller and smoother),
  **Balanced** (controlled, clarity and depth), **Open** (modern, extra punch and presence). The same
  release added **Volume Matching** for previews explicitly to avoid "louder is better"
  ([MusicTech 2019](https://musictech.com/news/gear/landr-adds-three-new-mastering-styles-to-ai-mastering-service/)).
- **Intensities: Lo / Med / Hi.** Lo keeps dynamics, Med aims at commercial loudness, Hi pushes loudness
  ([LANDR guide, ES](https://blog.landr.com/es/select-guia-de-masterizacion/)). A Gearspace user measured
  ≈ −10 LUFS (Med) and ≈ −9 LUFS (Hi) (search snippet of
  [gearspace thread](https://gearspace.com/threads/mastering-a-master.1428853/), unverified).
- **Online service**: reference-track mastering ("similar sound profile"), album mastering with "the same
  settings and loudness standards", unlimited revisions, MP3 320 / WAV 16 / "HD WAV" 24
  ([landr.com](https://www.landr.com/online-audio-mastering/)).
- **LANDR Mastering Plugin (Pro), late 2023**, the most transparent public description of a commercial
  assistant ([Mix, 2023-12-13](https://www.mixonline.com/?p=142956);
  [SOS, Dec 2023](https://www.soundonsound.com/reviews/landr-mastering-plugin)):
  - Local analysis of ≈ 30 s: frequency balance, dynamic range, stereo image and genre, compared with a
    reference library to build a per-track target.
  - Style Warm / Balanced / Open; **EQ** Low, Mid, High (gain only: low shelf, high shelf, broad bell
    ≈ 400 Hz) + **Presence** (≈ 2.5-5 kHz); **De-esser** (Frequency, Amount); **Stereo Field** (Focus ↔
    Wide, an M/S balance); **Compression**, **Character** (attack/release: faster/punchier ↔
    slower/smoother), **Saturation**; **Input gain**, **Loudness**; **Gain Match**; LUFS-I and TP meters.
  - Measured: rock/pop ≈ −8 to −9 LUFS short-term, true peak ≈ −0.1 to −0.2 dBFS (SOS, Mix). Computer
    Music notes there is no way to set a peak ceiling
    ([review](https://www.thomann.ae/prod_review_84980_AR_577828.html)).
- **Patent US 9,304,988 B2** (MixGenius → LANDR; inventors Terrell, Mansbridge, Reiss, De Man; priority
  2013-08-28, expected expiry 2034-08-30) ([Google Patents](https://patents.google.com/patent/US9304988)):
  semantic features (MFCCs, tempo, genre, instrumentation, loudness, spectral shape) → k-NN lookup in a
  database of reference productions → processing configuration + **target production features**; static
  rules (fixed settings) and dynamic rules (target outcomes, e.g. output RMS); a "production feature
  mapper" iterates parameters until the measured output hits the targets. Example mastering chain:
  **HPF → spatial processor → EQ → multiband compressor → limiter**.
  *Note for us:* "measure, compare with a target, iterate until the output matches" is generic
  engineering practice (and a classic in the C4DM literature, §4.4), but the claims of an unexpired patent
  should be read before shipping a k-NN-over-reference-database design; a fixed rule table over measured
  features with hand-built targets is the conservative route (inferred; not legal advice).

### 1.3 iZotope Ozone Master Assistant (Ozone 8 → 12)

- **Ozone 8 (2018).** An ML classifier characterises the audio and blends **10 genre target classes** into
  a per-track target EQ curve; "intelligent dynamics" decides whether compression is needed; the Maximizer
  targets a loudness by destination
  ([iZotope, assistive audio](https://www.izotope.com/en/learn/izotope-and-assistive-audio-technology)).
- **Ozone 9 manual** ([Master Assistant](https://s3.amazonaws.com/izotopedownloads/docs/ozone9/en/master-assistant/index.html)):
  - Play ≥ 30 s, ideally the loudest part.
  - Destination → ceiling: **Streaming −1.0 dB**, **CD −0.3 dB**. Manual intensity: **Low −14, Medium −12,
    High −11 LUFS**.
  - Reference mode takes loudness and EQ curve from a reference track.
  - EQ: band 1 low shelf + band 8 high shelf always on, up to 6 bells in between.
  - **Dynamics is enabled only if the crest factor is outside an ideal range (3-10 dB)**.
  - Modern chain: EQ, Dynamics, Maximizer, Dynamic EQ. Vintage chain: EQ, Vintage EQ, Vintage Compressor,
    Vintage Limiter, Maximizer (Vintage Compressor bypassed unless extreme dynamics).
- **Maximizer** ([Ozone 9 docs](https://s3.amazonaws.com/izotopedownloads/docs/ozone9/en/maximizer/index.html)):
  IRC LL (low latency), IRC I (fast on transients, slow on bass), IRC II (more transient preservation),
  IRC III (aggressive; styles Clipping/Crisp/Balanced/Pumping), IRC IV (spectral: dozens of
  psychoacoustically spaced bands, limits the bands that cause the peaks; styles Classic/Modern/Transient).
  Character Fast 0 … Slow 10; Transient Emphasis; Stereo Independence (transient/sustain); True Peak;
  docs suggest ceiling −0.3 dB with dither, **−0.6 to −0.8 dB before MP3/AAC**.
- **Match EQ**: snapshot reference vs current spectra, Smoothing, Amount, cut-off handles, linear phase
  option ([docs](https://downloads.izotope.com/docs/ozone9/en/match-eq/index.html)).
- **Tonal Balance Control**: started with 3 curves (Orchestral, Bass Heavy, Modern) "built from thousands
  of tracks"; TBC 2 had 12, TBC 3 has 30 genre targets and a blender
  ([TBC 3](https://www.native-instruments.com/en/products/izotope/tonal-balance-control-3)). The display is
  a target line with a tolerance band per frequency region.
- **Ozone 10 (2022)**: genre targets are averages of each genre's top-10 hits; Assistant also sets
  Stabilizer (dynamic spectral shaping), Impact (4-band micro-dynamics), Imager; macros EQ, Stabilizer,
  Width Match, Dynamics Match; gain-matched bypass. Maximizer **Soft Clip** (Light/Moderate/Heavy), 4×
  oversampled. Genre detection called "fairly hit and miss"; Maximizer "teeters close to distortion"
  ([MusicTech review](https://musictech.com/reviews/plug-ins/izotope-ozone-10-advanced-review/)).
- **Ozone 11 (2023)**: Clarity (spectral, > 300 Hz, Tilt), Upward Compress in the Maximizer, Vocal
  Balance, Stem Focus ([SOS](https://www.soundonsound.com/reviews/izotope-ozone-11-advanced)).
- **Ozone 12 (2025-09)**: Assistant **Auto-Master** or **Custom** (choose genre target, loudness target,
  allowed modules); analysis time in seconds/bars (≤ 60 s); **35 genre targets**; reference matching of
  tonal balance, width, dynamics and loudness; macros Tonal Balance, Loudness, Dynamic Match, Width
  Match, Clarity, Stabilizer, Vocal Balance
  ([iZotope](https://www.izotope.com/en/learn/how-to-use-master-assistant-in-ozone)). **IRC 5**: first
  multiband limiter (4 bands, separate gain envelopes, peak-contributing bands attenuated first,
  psychoacoustic audibility model); **Unlimiter** neural net
  ([Inside Ozone 12](https://www.izotope.com/community/blog/inside-ozone-12)).
- **Patents.** None found on the Assistant itself. Related: iZotope US 10,396,744 B2 (masking
  remediation); **Dolby US 12,375,053 B2** "Automatic generation and selection of target profiles for
  dynamic equalization" (granted 2025-07-29) clusters per-band features of reference audio into target
  profiles and drives a dynamic EQ toward the nearest one
  ([Google Patents](https://patents.google.com/patent/US12375053)), which is close to the target-curve
  approach we propose; worth reading its claims (inferred caution, not legal advice).

### 1.4 BandLab Mastering

- Free presets **Universal** (balanced, "organic dynamic processing"), **Fire** ("pushed, punchy lows
  without muted vocals"), **Clarity** ("pristine breathy highs with light dynamic expansion"), **Tape**
  ("warm saturation with analog dynamic processing")
  ([BandLab help](https://help.bandlab.com/hc/en-us/articles/360001374513-How-do-I-master-my-music-on-BandLab), via search snippets).
- Sept 2026 update: member presets Natural, Cinematic, Spatial, Punch (designed with Mandy Parnell et
  al.), **11 intensity levels per preset**, 3-band mastering EQ, reference mastering on the top tier
  ([MusicTech](https://musictech.com/news/music/bandlabs-mastering-capabilities-just-got-an-upgrade-heres-whats-new/)).
  No LUFS target published.

### 1.5 eMastered

- Founded ≈ 2016 by Smith Carlson and Collin McLoughlin
  ([help](https://help.emastered.com/hc/en-us/articles/5615179151629)); "powered by AI".
- Options: mastering strength, compressor intensity (very low … extreme), EQ intensity, 3-band EQ,
  stereo width, volume; reference upload; level-matched A/B
  ([review](https://www.whippedcreamsounds.com/emastered-review)). **Strength scales the adjustments
  already chosen**, "a large EQ boost will be boosted even further"
  ([help](https://help.emastered.com/hc/en-us/articles/5571971090061)). High intensity reported to cause
  "pumping, squashed transients" ([review](https://violetrecording.com/emastered-review/)).

### 1.6 Apple Logic Pro Mastering Assistant (Logic Pro 10.8, late 2023)

([Apple support 108294](https://support.apple.com/en-gb/108294);
[Logic guide](https://support.apple.com/ar-kw/guide/logicpro/lgcpa4b810ba/mac);
[SOS technique article](https://www.soundonsound.com/techniques/logic-pros-ai-mastering-assistant))

- **Characters**: **Transparent** (default; "classic analogue hardware mastering signal paths"),
  **Clean** ("transparent yet punchy", EDM/acoustic), **Valve** (tube chain, "deep low end and refined high
  end"), **Punch** ("aggressive… subtle emphasis on the mid-range", rock). Characters need Apple silicon;
  Intel Macs get Clean only (SOS).
- Last slot on the stereo out; analyses the whole project or a locator range (≈ 1 min); **Reanalyse**
  after mix changes; chain not disclosed.
- **Controls**: Loudness (centre ≈ −14 LUFS-I); **Auto EQ** amount slider (SOS: 0-200 %; computed curve
  rarely beyond ±4 dB); **Custom EQ** 3 points ±6 dB (low shelf 20-200 Hz, mid 200 Hz-8 kHz, high shelf
  8-20 kHz); **Excite** (upper-mid saturation, on/off); **Width** (auto; fully left = mono).
- **Meters**: TP, LUFS-M/S/I, correlation. **Loudness Compensation** (loudness-matched bypass; turn off
  before bouncing).
- Production Expert listener poll on a piano ballad: Valve won (≈ 1/3), Transparent ≈ Clean second, Punch
  far behind, unprocessed last ([Production Expert 2024-01-30](https://www.production-expert.com/production-expert-1/how-good-is-logic-pro-mastering-assistant-take-a-listen-the-results)).

### 1.7 Others, briefly

- **Waves Online Mastering**: styles Precise / Organic / Elevating, tone Presence/Depth, reference track,
  no LUFS setting; **same input + settings → same output** (determinism advertised)
  ([FAQ](https://www.waves.com/online-mastering/faq)); MusicRadar says it did not appear to true-peak limit.
- **CloudBounce**: ~15 genre presets + warmth/brightness/bass; defaults "considerably louder than we would
  have liked" ([MusicRadar](https://musicradar.com/news/online-e-mastering-services)).
- **sonible smart:limit**: Learn pass, 26 genre profiles + reference profiles, separate loudness-target
  dropdown ([MusicTech](https://musictech.com/reviews/plug-ins/sonible-smartlimit-review/)).
- **Mastering The Mix** LIMITER (play loudest section, Analyze; presets Loud/Spotify/Apple), EXPOSE
  (checks TP clipping, over-compression, phase) ([plugins](https://www.masteringthemix.com/pages/plugins)).

### 1.8 Common pattern (synthesis, inferred from 1.1-1.7)

| Element | Who does it | Our decision |
|---|---|---|
| One "style" choice of 3-4 named characters | all | yes (§6.5) |
| Intensity in 3 steps (or continuous) | Mixea, LANDR, Ozone 9, BandLab (11 steps) | continuous 0-100 % with 3 labelled detents |
| Separate loudness target | Ozone, Logic, sonible | yes, decoupled from intensity |
| Learn/analysis of the loudest ~30 s | Ozone, LANDR plugin, Logic, sonible, MTM | yes, plugin; whole file in render mode |
| Genre target curve with tolerance band | Ozone, TBC, LANDR, smart:EQ, Dolby patent | yes, our own curves (§3.3) |
| Reference-track matching | LANDR, Ozone, eMastered, Waves, BandLab | yes (Matchering-style, §3.4), later phase |
| Loudness-matched A/B | LANDR, Ozone, Logic, eMastered | yes, mandatory |
| Ceiling choice / true peak | Ozone yes; LANDR plugin no (−0.1 dBTP) | yes, default −1 dBTP |
| Disclosure of what was done | Ozone yes (modules visible), Logic/LANDR/Mixea no | yes: every parameter visible + JSON report |

### 1.9 What users value, and what they complain about

- **Valued**: speed and price; whole-song preview; loudness-matched A/B (standard nearly everywhere now);
  use as a reference/rough master during production (sources in 1.1-1.6).
- **Blind tests**: Production Expert 2017 (1000+ votes): LANDR 46 %, CloudBounce 29 %, Ozone 8 25 %
  ([PE](https://www.production-expert.com/home-page/2017/10/30/ai-mastering-shootout-results-landr-masters-win-with-46-of-votes)).
  **Benn Jordan, Oct 2024** (472 listeners, double-blind): two human engineers 1st and 2nd, **Matchering
  2.0 (open source, DSP) 3rd**, Ozone + Neutron 4th, Ozone 11 7th, LANDR disqualified
  ([MusicTech](https://musictech.com/news/gear/benn-jordan-ai-mastering-study)). A transparent DSP matcher
  beating ML-marketed products is strong evidence for the design we propose (§5).
- **Complaints** (each a design requirement for us):
  - *Over-processing / over-compression*: Ozone 10/11 Assistant "will always overload the track with …
    impact, clarity, stabilizer" regardless of the mix ([KVR](https://www.kvraudio.com/forum/viewtopic.php?p=8743340));
    LANDR Hi "eliminates dynamic range" ([Planetary Group](https://www.planetarygroup.com/landr-ai-mastering-engine-overview-test/));
    eMastered high = pumping. → our auto mode must be able to decide *not* to use a module (Ozone 9's
    crest-factor gate is the right idea) and has GR budgets (§6.4).
  - *Over-brightening*: "run a professional mix through it that's already bright and it will make it
    brighter" ([KVR, Ozone 12](https://www.kvraudio.com/forum/viewtopic.php?p=9252759)). → EQ is a
    *difference* to target, clamped, never a fixed boost; Bright style is a target offset, not a boost.
  - *Stereo/bass surprises*: Imager narrowing 20-30 %, unexpected low cuts (same thread). → width changes
    small, guarded, shown.
  - *Hot peaks / loudness misses*: LANDR plugin TP −0.1 dBFS; Waves no TP limiting; Ozone 12 "does not hit
    the value set". → T2/T4 tests in §6.10.
  - *Inconsistency*: hit-and-miss genre detection; no album consistency (Mixea). → deterministic, style
    chosen by the user, "album mode" sharing one analysis across tracks (§6.8).
  - *Opacity*: Logic and LANDR don't show the chain. → show everything.
  - *Already-limited mixes get flatter*. → detect low PLR on input and warn / reduce intensity (§6.4).

## 2. Loudness and delivery targets

Primary documents read: [ITU-R BS.1770-5 (11/2023)](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf),
[EBU R 128-2023](https://tech.ebu.ch/docs/r/r128.pdf), [EBU Tech 3341-2023](https://tech.ebu.ch/docs/tech/tech3341.pdf),
[EBU Tech 3342-2023](https://tech.ebu.ch/docs/tech/tech3342.pdf), [EBU Tech 3344-2016](https://tech.ebu.ch/docs/tech/tech3344.pdf),
[EBU R 128 s2-2023](https://tech.ebu.ch/docs/r/r128s2.pdf), and the sources of
[libebur128](https://github.com/jiixyj/libebur128) (MIT), [pyloudnorm](https://github.com/csteinmetz1/pyloudnorm) (MIT) and
FFmpeg's [`f_ebur128.c`](https://github.com/FFmpeg/FFmpeg/blob/master/libavfilter/f_ebur128.c) / `af_loudnorm.c`.
AES TD1004/TD1008/AES77 PDFs were not reachable (aes.org 403); their values below are secondary.

### 2.1 BS.1770 loudness

**K-weighting** is two biquads in cascade, direct form with a0 = 1 (BS.1770-5 Annex 1, Tables 1-2, pp. 4-5).
At 48 kHz, exactly as printed:

| | b0 | b1 | b2 | a1 | a2 |
|---|---|---|---|---|---|
| Stage 1, pre-filter (high shelf, "spherical head") | 1.53512485958697 | −2.69169618940638 | 1.19839281085285 | −1.69065929318241 | 0.73248077421585 |
| Stage 2, RLB weighting (high-pass) | 1.0 | −2.0 | 1.0 | −1.99004745483398 | 0.99007225036621 |

The standard says other rates need coefficients "chosen to provide the same frequency response that the
specified filter provides at 48 kHz" (p. 4-5); it does not give a prototype. The analog parameters that
reproduce the table, as used by libebur128 (`ebur128_init_filter()`) and FFmpeg (and, to ~1e-11, by
pyloudnorm's `filter_class="DeMan"` after B. De Man, "Evaluation of implementations of the EBU R128
loudness measurement", AES 145th Conv., 2018, [e-lib 19790](https://aes2.org/publications/elibrary-page/?id=19790)):

```c
// Stage 1: high shelf
const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
double K  = tan(M_PI * f0 / fs);
double Vh = pow(10.0, G / 20.0);
double Vb = pow(Vh, 0.4996667741545416);
double a0 = 1.0 + K / Q + K * K;
b0 = (Vh + Vb * K / Q + K * K) / a0;   b1 = 2.0 * (K * K - Vh) / a0;   b2 = (Vh - Vb * K / Q + K * K) / a0;
a1 = 2.0 * (K * K - 1.0) / a0;         a2 = (1.0 - K / Q + K * K) / a0;
// Stage 2: high-pass (numerator NOT normalised, as in the standard)
const double f1 = 38.13547087602444, Q1 = 0.5003270373238773;
K = tan(M_PI * f1 / fs);  double d = 1.0 + K / Q1 + K * K;
c0 = 1.0; c1 = -2.0; c2 = 1.0;  e1 = 2.0 * (K * K - 1.0) / d;  e2 = (1.0 - K / Q1 + K * K) / d;
```

I evaluated this at several rates (scratch script, Python, double precision). At 48 kHz it reproduces the
printed table to all 14 digits. Other rates:

| fs | stage 1 b0, b1, b2, a1, a2 | stage 2 a1, a2 |
|---|---|---|
| 44 100 | 1.53084123005035, −2.65097999515473, 1.16907907992159, −1.66365511325602, 0.71259542807323 | −1.98916967362980, 0.98919903578704 |
| 88 200 | 1.55751537557965, −2.90562707992635, 1.36133397747221, −1.83091998796233, 0.84414226108785 | −1.99457751545034, 0.99458487587805 |
| 96 000 | 1.55971422897580, −2.92674157825108, 1.37826120231582, −1.84460946989011, 0.85584332293064 | −1.99501754472472, 0.99502375904092 |
| 192 000 | 1.57222721509128, −3.04728305156155, 1.47797134097961, −1.92220223060749, 0.92511773511683 | −1.99750722284070, 0.99750877835551 |

Cascade magnitude (computed, 48 kHz; 44.1 kHz within 0.005 dB): 20 Hz −13.28 dB, 50 Hz −3.93, 100 Hz −1.13,
997 Hz **+0.691**, 2 kHz +3.07, 5 kHz +4.01, ≥ 10 kHz +4.04. The +0.691 dB at 997 Hz is what the −0.691
constant in the loudness formula cancels (BS.1770-5 p. 7, Note 1). Pitfall: re-using the 48 kHz table at
44.1 kHz shifts both corners by 48/44.1; pyloudnorm's *default* filter is an RBJ approximation (≈ −0.04 LU
bias), use `DeMan` for reference values.

Implementation notes: run the biquads in double (the stage-2 pole is at |z| ≈ 0.995; single precision is
marginal at 192 kHz, inferred); store per-100 ms partial sums of y² per channel, so momentary, short-term,
gated blocks and LRA all come from the same 100 ms ring.

**Loudness** (eq. 2): `L = −0.691 + 10·log10( Σ_i G_i · z_i )` LKFS (= LUFS), z_i = mean square of the
K-filtered channel. Weights (Table 3): L, R, C = 1.0; Ls, Rs = 1.41; LFE excluded. For stereo: G = 1, 1.
Sanity values: a 0 dBFS 997 Hz sine in one channel reads −3.01 LUFS; a 1 kHz sine at −18 dBFS peak on
both channels reads −18.0 LUFS (Tech 3341 §2.9). A mono file is one channel, so it reads 3 dB lower than
the same signal as dual-mono stereo (libebur128 `EBUR128_DUAL_MONO` weight 2.0); ATMaster should treat a
mono input as dual-mono (it will output stereo).

**Gating** (eqs. 3-7): blocks T_g = 400 ms, 75 % overlap (100 ms hop), incomplete final block discarded.
Block loudness l_j. Absolute gate Γa = −70 LUFS. Relative gate Γr = (loudness of the mean power of the
blocks above Γa) − 10 LU. Integrated = loudness of the mean power of blocks with l_j > Γr and l_j > Γa.

**Momentary / short-term** (Tech 3341 §2.2): rectangular windows of 0.4 s and 3 s, ungated, no extra
ballistics; live update ≥ 10 Hz; integrated ≥ 1 Hz.

**LRA** (Tech 3342 §3.1, p. 4-5): short-term (3 s) values at ≥ 10 Hz (overlap ≥ 2.9 s); absolute gate
−70 LUFS; relative gate **−20 LU** below the power mean of the absolute-gated short-term values;
**LRA = P95 − P10** of what remains; the reference MATLAB uses nearest rank
`sorted(round((n−1)·p/100 + 1))` and `>=` at the gates; file meters should append ≥ 1.5 s of silence.
Pitfalls in references: libebur128 computes LRA short-term blocks only every 1 s (still passes ±1 LU);
pyloudnorm uses linearly interpolated percentiles.

**Conformance**: Tech 3341 Table 1 (pp. 8-9) and Tech 3342 p. 6 define the minimum test set (EBU
downloadable WAVs, 48 kHz). Examples: stereo 1 kHz at −23 dBFS → M, S, I = −23.0 ±0.1; 20 s at −26 /
20.1 s at −20 / 20 s at −26 → I = −23.0; LRA cases 20 s −20/−30 dBFS → 10 LU, −20/−15 → 5, −40/−20 → 20,
−50/−35/−20/−35/−50 → 15 (±1 LU). These become unit test T1 (§6.10).

### 2.2 True peak (BS.1770 Annex 2)

Algorithm (BS.1770-5 pp. 18-19): (optional, fixed-point only: attenuate 12.04 dB) → **4× oversample**
(48 → 192 kHz) → low-pass interpolation → |x| → 20·log10 (+12.04 dB if attenuated). Inputs at higher rates
need proportionally less oversampling (2× at 96 kHz). The example interpolator is a **48-tap, 4-phase
FIR** (12 taps per phase), coefficients multiples of 2⁻¹⁶; phases 2 and 3 are phases 1 and 0 reversed:

| tap | Phase 0 | Phase 1 | Phase 2 | Phase 3 |
|---|---|---|---|---|
| 1 | 0.0017089843750 | −0.0291748046875 | −0.0189208984375 | −0.0083007812500 |
| 2 | 0.0109863281250 | 0.0292968750000 | 0.0330810546875 | 0.0148925781250 |
| 3 | −0.0196533203125 | −0.0517578125000 | −0.0582275390625 | −0.0266113281250 |
| 4 | 0.0332031250000 | 0.0891113281250 | 0.1015625000000 | 0.0476074218750 |
| 5 | −0.0594482421875 | −0.1665039062500 | −0.2003173828125 | −0.1022949218750 |
| 6 | 0.1373291015625 | 0.4650878906250 | 0.7797851562500 | 0.9721679687500 |
| 7 | 0.9721679687500 | 0.7797851562500 | 0.4650878906250 | 0.1373291015625 |
| 8 | −0.1022949218750 | −0.2003173828125 | −0.1665039062500 | −0.0594482421875 |
| 9 | 0.0476074218750 | 0.1015625000000 | 0.0891113281250 | 0.0332031250000 |
| 10 | −0.0266113281250 | −0.0582275390625 | −0.0517578125000 | −0.0196533203125 |
| 11 | 0.0148925781250 | 0.0330810546875 | 0.0292968750000 | 0.0109863281250 |
| 12 | −0.0083007812500 | −0.0189208984375 | −0.0291748046875 | 0.0017089843750 |

**Under-read bound** (Annex 2, Attachment 1, p. 21): worst case `20·log10(cos(π·f_norm/n))` dB for
oversampling n and f_norm = f/fs. At f_norm = 0.5: **0.688 dB (4×)**, 0.169 dB (8×), 0.042 dB (16×); at
0.45: 0.554 dB (4×), 0.136 dB (8×). A sample-peak meter can under-read 3 dB on an fs/4 tone. EBU Tech 3344
p. 26 gives this as the reason for the −1 dBTP ceiling: it "covers the maximum under-read of a True Peak
meter that is using a four times oversampling interpolation filter".

**Tech 3341 true-peak tests** (tolerance +0.2/−0.4 dB): #15-18 fs/4, fs/6, fs/8 sines at 0.5 FS with
phase offsets → −6.0 dBTP; #19 fs/4 at 1.41 FS, 45° → +3.0 dBTP; #20-23 decimation-offset bursts → 0.0 dBTP.

**Reference implementations**: libebur128 uses a 49-tap Hann-windowed sinc polyphase interpolator,
factor 4 below 96 kHz, 2 at 96-192 kHz, none at ≥ 192 kHz; FFmpeg resamples to 192 kHz with
libswresample. Note 44.1 kHz × 4 = 176.4 kHz is below the 192 kHz the standard names for "dBTP"
labelling; everyone reports it as TP anyway.

**Design consequence** (inferred): the *meter* should follow the standard (4×, 2× at 88.2/96 kHz) so our
numbers agree with every other meter; the *limiter's detector* should be stricter (8× at 44.1/48 kHz, or
4× plus a 0.2 dB safety margin) because the 4× worst-case under-read is 0.69 dB and we promise
"TP ≤ ceiling" as measured by anyone. The repo's `shared/dsp/Oversampling.h` (Kaiser half-band cascade,
95 taps for the last stage, ~100 dB rejection) can be reused for the oversampled detector and the
clipper; for metering use the BS.1770 table above so the meter is standard-exact.

### 2.3 Ceilings and codec overshoot

- Lossy transform codecs (MDCT: AAC, MP3, Vorbis, Opus) quantise spectral coefficients and band-limit;
  the decoded waveform's phase relations change, so peaks grow, especially on brick-wall-limited or
  clipped material, and more at lower bit rates (inferred, standard theory; consistent with Lund below).
- T. Lund, "Level and distortion in digital broadcasting", EBU Technical Review, Apr 2007, p. 3
  ([PDF](https://tech.ebu.ch/docs/techreview/trev_310-lund.pdf)): "lower data rates generate higher output
  peak values… MP3 encodings at 128 kbit/s should for instance stay below −5 dBFS sample peak".
- EBU Tech 3344 p. 26: end-stage limiter at **−2 dBTP** before an encoder; lower for low bit rates, higher
  possible for high bit rates.
- AES TD1008 (2021; now AES77-2023), as quoted secondarily
  ([productionadvice](https://productionadvice.co.uk/td1008/), [baken notes](https://raw.githubusercontent.com/M-Igashi/baken/main/docs/true-peak-ceiling.md)):
  "Maximum True Peak level not exceed −1 dBTP at the codec input"; 256 kbps coders "may work
  satisfactorily with as little as −0.5 dBTP"; music Distribution Loudness −16 LUFS.
- **Spotify for Artists** ([loudness normalization](https://support.spotify.com/us/artists/article/loudness-normalization/),
  fetched 2026-10-18): "−1 dB TP max… best for lossy formats"; "If you master louder than −14 dB
  integrated LUFS… keep True Peak below **−2 dB**".
- **Apple Digital Masters** ([PDF, 2021](https://www.apple.com/apple-music/apple-digital-masters/docs/apple-digital-masters.pdf)):
  leave "at least 1 dB of headroom"; check encoded files with `afclip`, encode with `afconvert` "using the
  same commands Apple uses at ingest", AURoundTripAAC AU; no LUFS target is stated.
- Ozone 9 docs: ceiling −0.3 dB with dither for CD, −0.6 to −0.8 dB before MP3/AAC (§1.3).

**ATMaster rule**: ceiling −1.0 dBTP by default; auto −2.0 dBTP when target > −14 LUFS-I or style = Loud/Club;
−0.3 dBTP only for an explicit "CD / no codec" preset. Test T6 (§6.10) verifies decoded AAC/MP3 peaks.

### 2.4 Platform normalisation (as of Oct 2026; dated sources)

| Platform | Reference | Behaviour | Source |
|---|---|---|---|
| Spotify | **−14 LUFS** (Normal); Loud −11; Quiet −19 | album normalisation when playing an album, track otherwise; positive gain capped to keep 1 dB TP headroom; Loud mode adds a limiter (−1 dB, 5 ms attack, 100 ms decay) | [Spotify for Artists](https://support.spotify.com/us/artists/article/loudness-normalization/) (2026-10-18) |
| Apple Music | **−16 LUFS** (Sound Check, LUFS since 2022) | on by default on new installs; turns loud songs down | [MeterPlugs 2022-03-23](https://www.meterplugs.com/blog/2022/03/23/apple-switch-to-lufs.html) (secondary; Apple publishes no number) |
| YouTube | **−14 LUFS** (since 2019-09) | turns down only; "Stats for nerds → content loudness" | [MeterPlugs 2019](https://www.meterplugs.com/blog/2019/09/18/youtube-changes-loudness-reference-to-14-lufs.html) (secondary) |
| TIDAL | **−14 LUFS** | album normalisation (loudest track → −14), attenuation only, on by default on mobile | [audioXpress 2017](https://audioxpress.com/news/tidal-implements-album-loudness-normalization-and-activates-it-by-default-for-mobile-players) |
| Amazon Music | **−14 LUFS** (one distributor FAQ says −9…−13) | desktop/mobile; does not raise level | [MeterPlugs 2019](https://meterplugs.com/blog/2019/10/15/loudness-penalty-amazon-deezer.html) |
| Deezer | **−15 LUFS** | track-level, does not raise level | same |
| SoundCloud | **−14 LUFS** recommended, normalisation on playback (start date unknown; older sources: none) | | [help 360053660014](https://help.soundcloud.com/hc/en-us/articles/360053660014) (snippet only) |
| Pandora, Qobuz | no published value | | |
| AES TD1004 (2015) / TD1008 (2021) / AES77-2023 | music −16 LUFS distribution loudness; window −16…−20; ≤ −1 dBTP | | secondary, see 2.3 |
| EBU R128 (broadcast) | −23 LUFS ±0.5 (±1 live); ≤ −1 dBTP | | R 128-2023 p. 3-4 |

Platform "loudness space" (MeterPlugs Dynameter guide p. 15): PLR available before turn-down = (−1 dBTP) −
reference: Spotify/YouTube/Tidal 13 dB, Apple 15 dB.

### 2.5 Loudness of commercial masters by genre

- **Freshly Baked Studios, 2026-08-22** (vendor dataset, 314 876 tracks, BS.1770 projected, claimed
  ±0.5 dB, not peer reviewed) ([blog](https://freshlybakedstudios.com/blog/average-lufs-by-genre)): overall
  median **−9.5 LUFS** (IQR −11.4…−8.2); 90.4 % louder than −14. Medians: Metal −8.4, Dance/EDM −8.7,
  House −9.1, Rock −9.2, Pop −9.5, Hip-Hop −9.7, R&B −9.6, Country/Folk −9.6, Singer-songwriter −10.6,
  Jazz −12.8, Instrumental/Score −12.9, Ambient −14.1.
- **iZotope, top-10 Dance/Electronic, 2025-04** ([blog](https://www.izotope.com/community/blog/dance-electronic-analysis)):
  average −8.3 LUFS-I (−6.2…−11.3), LRA 6.6 LU, TP −0.29…**+1.50 dBTP** (most above 0). Billboard Global
  200 top-10 (2024): −8.3 ±1 LUFS-I, LRA 5.5 ±1.5.
- **RoEx/QMUL, Mourgela et al., AES 157th Conv. 2024, paper 10186** ([arXiv 2412.03373](https://arxiv.org/pdf/2412.03373)):
  user-submitted masters cluster ≈ −14 LUFS, mixes ≈ −23 LUFS; 79 % of masters louder than −14; only
  42.5 % of masters clip-free vs 68.6 % of mixes. Metrics CSV (CC BY 4.0):
  [Zenodo 13683186](https://zenodo.org/records/13683186), useful to calibrate "typical mix" inputs.
- Deruty & Tardieu, "About dynamic processing in mainstream music", JAES 62(1/2), 2014: macro-dynamics
  (LRA) did not shrink much during the loudness war; the change is in micro-dynamics (crest/PSR)
  ([e-lib 17085](https://aes2.org/publications/elibrary-page/?id=17085), abstract). Vickers, "The loudness
  war: background, speculation and recommendations", AES 129th Conv., 2010
  ([text](https://www.sfxmachine.com/docs/loudnesswar/)).

Working table for presets (inferred from the data above; jazz/classical have weak data):

| Genre family | Typical LUFS-I | Typical PLR (TP − LUFS-I) | Typical LRA |
|---|---|---|---|
| EDM / club | −6 … −9 | 7-9 dB | 4-7 LU |
| Pop / hip-hop | −8 … −10 | 8-10 dB | 4-7 LU |
| Rock / metal | −7.5 … −10 | 7-10 dB | 3-6 LU |
| Acoustic / folk / singer-songwriter | −10 … −13 | 10-12 dB | 5-9 LU |
| Jazz | −11 … −15 | 11-15 dB | 6-12 LU |
| Classical / score | −16 … −23 | 15-22 dB | 10-20 LU |

### 2.6 Dynamics measures

- **Crest factor** = 20·log10(peak / RMS) (sine 3.01 dB). Ozone 9's assistant uses 3-10 dB as the "ideal"
  window that gates its compressor (§1.3).
- **PLR** = max TP (dBTP) − LUFS-I. Defined in EBU R 128 s2-2023 p. 5.
- **PSR** = TP − LUFS-S (3 s), real time ([MeterPlugs](https://www.meterplugs.com/blog/2017/05/18/crest-factor-psr-and-plr.html)).
  Dynameter guide ([PDF](https://www.meterplugs.com/files/dynameter-guide.pdf) pp. 9-10, 15-16): PSR < 8
  "often indicate[s] excessive limiting or clipping", minimum-PSR presets Limited 8 / Competitive 10 /
  Balanced 12 / Wide 14; "no lower than PSR 8 during the loudest parts"; healthy pop/rock example min
  PSR 9 / PLR 11; "Uptown Funk" min PSR 8 / PLR 11.
- **TT DR** (Pleasurize Music Foundation; algorithm per [drmeter](https://codeberg.org/janw/drmeter)
  `algorithm.py`): non-overlapping 3 s blocks per channel; block RMS = sqrt(2·mean(x²)); take the loudest
  20 % of blocks, R = sqrt(mean(RMS²)); P₂ = second-highest block sample peak; DR_ch = 20·log10(P₂/R);
  track DR = mean over channels, rounded; ≥ 15 s needed.
- **LRA**: §2.1.

**ATMaster uses**: PLR_in and the PSR distribution over the loud mask to decide how much dynamic
reduction is needed and whether to warn "input already heavily limited" (min PSR_in < 8 dB); min PSR_out
≥ 8 dB as a soft guard for all styles except Club/Loud (≥ 6 dB).

### 2.7 Loudness normalisation algorithms (for reference)

FFmpeg `loudnorm` ([docs](https://ffmpeg.org/ffmpeg-filters.html#loudnorm)): defaults I = −24, LRA = 7,
TP = −2; two-pass "linear" mode applies one gain if the measured values are supplied and the gain does not
push TP over target, otherwise falls back to "dynamic" (3 s buffer, 100 ms frames, gain = target −
short-term, smoothed with a 21-tap Gaussian σ = 3.5 frames, then a lookahead TP limiter); it forces
192 kHz internally. Our render mode is the "linear" idea plus our own limiter: one static gain into the
limiter, solved iteratively on the output (§6.4).

## 3. Tonal balance targets

### 3.1 What the literature measured

**Pestana, Ma, Reiss, Barbosa, Black, "Spectral characteristics of popular commercial recordings
1950-2010"**, AES 135th Convention, New York, Oct 2013, Convention Paper 8960
([author PDF, Wayback](http://web.archive.org/web/2019id_/https://www.eecs.qmul.ac.uk/~josh/documents/2013/Pestana%20Ma%20Reiss%20-%20AES135.pdf);
[AES e-lib 17010](https://aes2.org/publications/elibrary-page/?id=17010)):

- Corpus: **772** US/UK No. 1 singles (Table 1, p. 2: 50s 71, 60s 156, 70s 129, 80s 193, 90s 96, 2000s 127),
  uncompressed files, genre tags from Wikipedia / EchoNest / Last.fm.
- Method (§1): mono (L+R)/2 at 44.1 kHz; contiguous 4096-sample frames (~10.8 Hz bins); per song the mean
  of |X(k)| over frames. **Normalisation**: each song's spectrum scaled to sum 1 → cumulative distribution
  → average the CDFs across songs → differentiate → rescale by the average magnitude. (They rejected
  plain level normalisation because one strong peak would push every other bin down.)
- Findings (Conclusions, p. 5): **"a linearly decaying distribution of around 5 dB per octave between 100
  and 4000 Hz, becoming gradually steeper with higher frequencies, and a severe low-cut around 60 Hz"**.
  Over the decades: overall level up, low end extends lower, presence (1.5-4 kHz) slope shallower, spectrum
  flatter; recent decades show comb peaks at each equal-tempered semitone (key/melody content averaged in).
- **Genre table** (Table 2, p. 5; slope = log-log regression of magnitude, 100 Hz-10 kHz; dB/oct column is
  slope × 6.02, my conversion):

| Genre | Centroid (Hz) | Slope | ≈ dB/oct (narrowband magnitude) |
|---|---|---|---|
| Electronic | 845 | −0.746 | −4.5 |
| Disco | 963 | −0.804 | −4.8 |
| Hip-hop | 662 | −0.814 | −4.9 |
| Pop | 868 | −0.943 | −5.7 |
| Rock | 858 | −0.979 | −5.9 |
| Soul | 760 | −1.030 | −6.2 |
| R&B | 811 | −1.034 | −6.2 |
| Folk | 603 | −1.182 | −7.1 |
| Jazz | 785 | −1.293 | −7.8 |

  Low-end differences between genres are the only region where confidence intervals don't overlap;
  top-end differences between genres exceed 10 dB; brightest hip-hop > electronic > disco, dullest folk/jazz.

**Units pitfall** (inferred, important for implementation): a slope on FFT bins (constant Hz width) differs
from a slope on fractional-octave bands (width ∝ f) by **+3.01 dB/oct**. Pink noise is −3 dB/oct on FFT
bins and 0 dB/oct in 1/3-octave band power. So Pestana's −5.7 dB/oct (pop, narrowband) ≈ **−2.7 dB/oct in
1/3-octave band power**. The "4.5 dB/oct" tilt that analysers such as FabFilter Pro-Q and Voxengo SPAN offer
to make a typical mix look flat (from memory; verify the defaults) is a narrowband convention: it means
"typical music ≈ −4.5 dB/oct per FFT bin ≈ −1.5 dB/oct per octave band". Always store our targets as
**band power in dB on a log-frequency grid**, and convert explicitly when displaying.

**Other work**:
- Adrian & Bitzer, DAGA 2014 ([PDF](https://pub.dega-akustik.de/DAGA_2014/data/articles/000365.pdf)):
  20 chart titles/year 1955-2004; trim 5 % at both ends, normalise to −30 dBFS RMS (ITU-T P.56); averaged PSD
  falls ≈ 6 dB/oct 100 Hz-10 kHz; bandwidth grows per decade; modulation spectra lose energy (compression).
- Elowsson & Friberg, "Long-term average spectrum in popular music and its relation to the level of the
  percussion", AES 142nd Conv., 2017, paper 9762 ([e-lib 18638](https://aes2.org/publications/elibrary-page/?id=18638),
  abstract): LTAS fitted with two quadratics; **tracks with more percussion have relatively more bass and
  treble energy**; genre differences in LTAS are largely explained by percussion prominence; they suggest
  conditioning automatic-EQ targets on percussion level. *Design consequence*: a target selector by
  "percussiveness" (onset rate / percussive-harmonic energy ratio) is better grounded than one by genre
  label.
- Tamer, JAES 69(5), 2021 ([e-lib 21036](https://aes2.org/publications/elibrary-page/?id=21036)): streaming
  playlists show a steeper presence-band slope than earlier studies (abstract).
- Voss & Clarke, Nature 258, 1975: their "1/f" is the spectrum of *loudness and pitch fluctuations*, not of
  the audio signal; don't cite it for spectral slope.
- iZotope Tonal Balance Control ([docs](https://downloads.izotope.com/docs/ozone8/tonal-balance-control/index.html)):
  4 broad bands (Low 20-250 Hz, Low-mid 250 Hz-2 kHz, Mid 2-8 kHz, High 8-20 kHz) plus a fine view; a target
  shows "the typical range of spectral variation", built from thousands of tracks.

### 3.2 Smoothing

| Use | Resolution | Why |
|---|---|---|
| Target curves and the EQ match | **1/3 octave** (31 bands 20 Hz-20 kHz, ISO 266 centres) | Ma, Reiss & Black design on 1/3-oct centres (§3.4); fine enough for tonal balance, coarse enough to ignore notes |
| Display | 1/6 octave | readable without semitone comb |
| Harshness/resonance detection | 1/6 vs 1/3 difference | a resonance is a 1/6-oct excess over the 1/3-oct trend |
| Matchering's match | LOWESS frac 0.0375 on a log axis ≈ **0.375 octave** window (computed) | |

Fractional-octave smoothing on an FFT power spectrum: for each output centre fc, average **power** (not
dB) over bins in [fc·2^(−b/2), fc·2^(b/2)] with b = 1/3 (rectangular in log f; a Hann-weighted window in
log f avoids steps), then convert to dB. Below ~100 Hz an 8192-point FFT at 48 kHz has 5.9 Hz bins, so a
1/3-oct band at 25 Hz (≈ 6 Hz wide) holds ~1 bin: use 16384-32768 points for the low bands in the offline
analysis, or a constant-Q filterbank (inferred).

### 3.3 Building our own target curves (no proprietary curves)

We must not copy iZotope/LANDR/sonible curves (they're not published anyway; the TBC curves are
product data). Statistical averages of measured spectra are facts about the recordings, not copies of
them, and we never distribute the audio (inferred; not legal advice). Plan:

1. **Parametric seed (v1, needs no corpus).** Define each target as band power on 1/3-oct centres:
   `T(f) = tilt·log2(f/1 kHz) + LF(f) + HF(f)`, with
   - tilt (band power) −2.0 to −3.0 dB/oct (Pestana genre slopes converted: electronic ≈ −1.5, pop ≈ −2.7,
     jazz/folk ≈ −4 to −5 dB/oct, per-band power);
   - LF: a 2nd-order high-pass shape at 30-60 Hz (Pestana's "severe low-cut around 60 Hz") plus a low-end
     bump of 0-4 dB centred 50-90 Hz (electronic/hip-hop high, jazz/folk low);
   - HF: extra roll-off above 4-8 kHz ("gradually steeper", Pestana) of −1 to −3 dB/oct.
   Tolerance band ±2 dB (mids) widening to ±4 dB below 60 Hz and above 10 kHz (inferred from TBC's display
   behaviour and Pestana's statement that low and top end vary most).
2. **Corpus-derived refinement (v1.1).** Measure a corpus we're allowed to process:
   - FMA (`fma_full`, per-track CC licences; released, mostly mastered tracks) filtered to CC BY / CC BY-SA
     / CC0 ([mdeff/fma](https://github.com/mdeff/fma)); thousands of tracks across genres;
   - optionally the developers' own purchased music, processed locally, only curves kept.
   Per track: BS.1770 loudness-normalise; restrict to the loud mask (short-term ≥ LUFS-I − 3 LU, or
   Matchering's "pieces with RMS ≥ RMS of piece RMS"); Welch PSD, 1/3-oct band power in dB; mid and side
   separately. Per target class: **median** per band (robust to outliers), **P10/P90** per band as the
   tolerance band, and the side/mid ratio median per band as the stereo target. Classes by
   *percussiveness × brightness* clusters (Elowsson) rather than by genre label alone; give them friendly
   names afterwards. Keep each class ≥ 200 tracks (inferred).
3. **User targets.** "Learn target from reference files": the same procedure on 1-N user files, stored
   in the preset. This is what LANDR/Ozone/eMastered/BandLab call reference mastering.
4. **Remove melody**: averaging hundreds of tracks cancels key-specific peaks; for a single reference track
   (3.) the 1/3-oct smoothing plus a ≥ 1/2-oct smoothing above 200 Hz (Ma et al. used a 17-point moving
   average above 200 Hz only) prevents copying its key.

Store targets as JSON: `{ "name", "fc":[31], "mid_db":[31], "p10":[31], "p90":[31], "side_minus_mid_db":[31],
"source", "n_tracks" }`, with the provenance recorded.

### 3.4 Match EQ, done safely

Algorithms in the open literature and code:

- **Matchering 2.0** ([sergree/matchering](https://github.com/sergree/matchering), GPL-3.0-or-later; read
  from `defaults.py`, `stages.py`, `stage_helpers/match_frequencies.py`): M/S split; average magnitude
  spectra by STFT (boxcar, 4096, no overlap) over the loudest pieces; **linear magnitude ratio**
  ref/target; resampled onto a log grid of 8193 points (21.5 Hz-22.05 kHz); LOWESS smoothing (frac 0.0375);
  back to linear; `ifftshift(irfft(curve)) × Hann` → **4096-tap linear-phase FIR**, applied with
  zero-latency `fftconvolve(..., 'same')` offline. **No boost/cut clamp and no frequency-range limit**.
  Then RMS re-match in 4 iterations around a clip, then the Hyrax limiter (§4.6).
- **Ma, Reiss, Black, "Implementation of an intelligent equalization tool using Yule-Walker for music
  mixing and mastering"**, AES 134th Conv., 2013, paper 8892
  ([PDF, Wayback](http://web.archive.org/web/2016id_/http://www.eecs.qmul.ac.uk/~josh/documents/2013/Ma%20Reiss-%20Implementation%20of%20an%20Intelligent%20Equalization%20Tool%20Using%20Yule-Walker%20for%20Music%20Mixing%20and%20Mastering.pdf)):
  target = Pestana average, smoothed by a 17-point moving average above 200 Hz only; R128 hysteresis gate
  −25/−30 LUFS (inactive frames keep the previous filter); 4096 Hann FFT; desired response = target ÷
  frame spectrum sampled at 1/3-oct centres 16 Hz-20 kHz; an "amount" mix between target and current;
  smoothing between frames τ = 1.28 s; **Yule-Walker IIR of order 16** (8, 16, 32 performed alike; order 2
  poor); fit biased 20-100 Hz for lack of resolution.
- **iZotope Match EQ guidance** ([Ozone 9 docs](https://s3.amazonaws.com/izotopedownloads/docs/ozone9/en/match-eq/index.html)):
  "We suggest working with the Matched Curve amount under 50 %"; "If your Matching EQ curve has narrow
  peaks and valleys, increase the Smoothing"; aim for "the overall tonal shape… as opposed to an exact
  match"; Amount 100 % + Smoothing 0 % "can result in extreme, unnatural EQs".
- **FabFilter Pro-Q EQ Match** ([help](https://www.fabfilter.com/help/pro-q/using/eqmatch)): fits a
  chosen number of parametric bands; spectra settle in about 30 s.
- Logic's Auto EQ rarely exceeds ±4 dB (SOS, §1.6).

**ATMaster match-EQ procedure** (synthesis of the above; numbers are proposals):

1. Input LTAS over the loud mask (§6.4), mid channel (side handled by the stereo stage), 1/3-oct band power
   in dB, loudness-normalised to the target's reference loudness (K-weighted, so both curves sit at the
   same LUFS).
2. `D(f) = T(f) − X(f)`; zero-mean it with K-weighting-like weights (so the EQ doesn't change loudness).
3. Smoothing of D across bands: 3-band (1-octave) moving average below 100 Hz, 1/3-oct elsewhere; an extra
   1/2-oct smoothing if the input is a single reference (3.3 step 4).
4. **Dead zone**: if |D| is inside the target's P10..P90 band, scale the correction by 0.3 (the mix is
   already "normal" there, don't push it to the median); outside, full correction of the excess only
   (inferred; this is what prevents the "already bright gets brighter" complaint).
5. **Confidence weighting**: weight 0 below 30 Hz and above 16 kHz (low information, noise, codec cut-off,
   kick-pitch dependence), 0.5 for 30-50 Hz and 12-16 kHz.
6. **Amount** = intensity map (§6.6), default 60 %; **clamp** ±4 dB (±2 dB above 12 kHz, ±3 dB below 50 Hz);
   max 1.5 dB difference between adjacent 1/3-oct bands (avoids narrow features); total boost energy cap
   (sum of positive gains weighted by band loudness contribution ≤ +2 dB, inferred).
7. Realisation:
   - Plugin (real time): fit a fixed topology of **low shelf (80-150 Hz), 6 peaking bells at fixed log
     spacing (Q ≈ 0.9-1.4), high shelf (8-12 kHz)**, minimum phase, gains solved by linear least squares on
     the dB response (the bands' dB responses are nearly additive at these gains; 2-3 Gauss-Newton
     iterations make it exact, inferred). This matches Ozone 9's "shelf + up to 6 bells + shelf" layout and
     Ma's finding that order ~16 is enough. Zero latency, no pre-ringing.
   - Render: the same IIR by default (so plugin and render sound identical); an optional linear-phase FIR
     (4096-8192 taps at 48 kHz, Matchering-style, latency compensated) for users who prefer it. Linear
     phase pre-rings on low-frequency boosts, so cap LF boosts at +2 dB in linear-phase mode (inferred).
8. Never apply the match continuously in time (no dynamic re-analysis): one static curve per Learn /
   per file. Time-varying spectral correction is the dynamic EQ's job (§4.4), with tight limits.

**Low end specifically**: the region 30-120 Hz depends on the kick and bass pitch and the arrangement, so
matching it to a genre median can boost a note. Use 1-octave smoothing there, clamp ±3 dB, and prefer the
multiband compressor / dynamic EQ to control LF *variance* rather than static boosts (inferred; consistent
with Ma's observed bias below 100 Hz and with user complaints in §1.9).

**High end**: above 12 kHz the input may be band-limited (MP3 source, sample libraries) and boosting
there only raises noise and hiss. Detect a cut-off (energy falls > 20 dB within 1/3 oct) and freeze the
EQ above it (inferred).

## 4. Automatic analysis → settings: algorithms

### 4.1 Licences of what we can study or reuse

The repo is AGPL-3.0 (`LICENSE`). AGPLv3 §13 allows combining with GPLv3 code
([agpl-3.0.txt](https://www.gnu.org/licenses/agpl-3.0.txt)); the GPL FAQ: "You can always link GPLv3-covered
modules with AGPLv3-covered modules, and vice versa" ([gpl-faq](https://www.gnu.org/licenses/gpl-faq.html#AGPLGPL)).
GPLv2-*only* code is not compatible. Ideas and published algorithms can always be reimplemented; this
table matters only if we copy code.

| Project | Licence | Copy code into ATMaster? | What's worth studying |
|---|---|---|---|
| Matchering 2.0 (sergree) | GPL-3.0-or-later | yes (GPL-3 → AGPL combined work) | match EQ, RMS matching, Hyrax limiter |
| x42-dpl (Gareus, after Adriaensen's zita) | GPL-3.0-or-later (`src/peaklim.cc`) | yes | lookahead limiter, LF hold, TP side-chain |
| LSP Plugins | LGPL-3.0 | yes | limiter modes, oversampling, ALR |
| Calf | LGPL-2.1-or-later | yes | MB compressor (120 Hz/1 kHz/6 kHz LR4/LR8), de-esser, exciter |
| Airwindows | MIT | yes | ClipOnly2, ADClip, console/tape flavours |
| Signalsmith DSP | MIT | yes | limiter design article and code |
| CTAGDRC (p-hlp) | GPL-3.0-or-later | yes | Giannoulis detectors + crest-factor automation, JUCE |
| SimpleCompressor (Rudrich) | GPL-3.0 | yes | lookahead fade-in |
| Squeezer (mzuther) | GPL-3.0 | yes | |
| ChowTapeModel | GPL-3.0 | yes | tape hysteresis |
| libebur128 / pyloudnorm | MIT | yes | meters |
| TDR, Melda, LoudMax, Harrison | freeware / proprietary | **no** (listen/compare only) | |

Recommendation: write our own code (the engine is small and we want JUCE-free, deterministic,
block-size-independent DSP); credit Matchering, x42-dpl, Signalsmith and Giannoulis et al. in
`docs/ref/CREDITS.md` as the other plugins do.

### 4.2 Compressor core (Giannoulis, Massberg, Reiss)

"Digital dynamic range compressor design — a tutorial and analysis", JAES 60(6):399-408, 2012
([PDF, Wayback](https://web.archive.org/web/2020/https://www.eecs.qmul.ac.uk/~josh/documents/2012/GiannoulisMassbergReiss-dynamicrangecompression-JAES2012.pdf)).

Gain computer, soft knee (Eq. 4), threshold T, ratio R, knee width W (dB), xG = 20·log10|x|:

```
yG = xG                                         if 2(xG − T) < −W
yG = xG + (1/R − 1)·(xG − T + W/2)² / (2W)      if 2|xG − T| ≤ W
yG = T + (xG − T)/R                             if 2(xG − T) > W
```

One-pole coefficient α = exp(−1 / (τ·fs)) (τ = time to 63 %; a 10-90 % rise time is τ·ln 9).
Recommended topology (§5 of the paper): **feed-forward, detector in the log domain after the gain
computer, smooth decoupled peak detector** (Eq. 17):

```
xL[n] = xG[n] − yG[n]                                  // desired gain reduction (dB, ≥ 0)
y1[n] = max(xL[n], αR·y1[n−1] + (1 − αR)·xL[n])
yL[n] = αA·yL[n−1] + (1 − αA)·y1[n]
gain_dB[n] = M − yL[n]
```

Measured in the paper: smooth detectors give the lowest THD (decoupled slightly better than branching);
the log-domain detector gives release exponential in dB, independent of the amount of compression.
For a mastering bus compressor use an RMS pre-detector (Eq. 10, `yL² = α·yL² + (1−α)·x²`, τ ≈ 5-20 ms)
before the gain computer, and stereo link by max(L, R) or by mid (inferred, standard practice).

**Automatic attack/release from crest factor** (Giannoulis, Massberg, Reiss, "Parameter automation in a
dynamic range compressor", JAES 61(10):716-726, 2013,
[PDF, Wayback](http://web.archive.org/web/2019id_/https://www.eecs.qmul.ac.uk/~josh/documents/2013/Giannoulis%20Massberg%20Reiss%20-%20dynamic%20range%20compression%20automation%20-%20JAES%202013.pdf)):

```
y²peak[n] = max(x², α·y²peak[n−1] + (1−α)·x²);  y²rms[n] = α·y²rms[n−1] + (1−α)·x²;  τ = 200 ms
yC = ypeak / yrms
τA = 2·τAmax / yC²      τR = 2·τRmax / yC² − τA      (τAmax = 80 ms, τRmax = 1 s)
```

A sine (yC = √2) gets the maximum times; transient material gets short ones. Auto make-up (Eq. 18):
`c_makeup = −(c_Dev + c_Est)` with `c_Est = T(1 − 1/R)/2` and c_Dev a 2 s average of (c − c_Est).
Implemented in CTAGDRC (GPL-3, JUCE): [p-hlp/CTAGDRC](https://github.com/p-hlp/CTAGDRC).
For a *mastering* compressor I'd use the crest-factor law only to place attack/release inside a
style-defined window (e.g. glue attack 10-30 ms, release 100-600 ms), computed once from the analysis,
not sample-by-sample (deterministic, explainable; inferred).

**Mastering-oriented automation**:
- Hilsamer & Herzog, "A statistical approach to automated offline dynamic processing in the audio
  mastering process", DAFx-14, pp. 35-40 ([PDF](https://www.dafx14.fau.de/papers/dafx14_marcel_hilsamer_a_statistical_approach_to.pdf)):
  choose T/R/makeup so that the **statistical moments of the output level distribution** (of an RMS
  feature) hit genre targets; ballistics scaled by ΔRMS (positive → attack, negative → release); cites
  Katz's mastering ranges **attack 50-300 ms (typ. 100), release 50-500 ms (typ. 150-250)**.
  This is the closest published method to what we need for "glue": we do the same with simple percentiles
  of the short-term level distribution (§6.4).
- Maddams, Finn, Reiss, "An autonomous method for multi-track dynamic range compression", DAFx-12
  ([PDF](https://www.dafx.de/paper-archive/2012/papers/dafx12_submission_6.pdf)): smooth decoupled
  detector; attack/release from spectral flux; LRA from 400 ms windows at 7.5 Hz; "threshold" mode R = ∞,
  knee = |T|; "ratio" mode knee 3 dB, T = RMS − 12 dB; target LRA reduction **ΔLRA = 3 / 6 / 9 LU for
  light / medium / heavy**; parameters found iteratively (ratio step 0.1, threshold 0.5 dB). The
  light/medium/heavy LRA-reduction idea maps directly onto our intensity.
- Ma, De Man, Pestana, Black, Reiss, "Intelligent multitrack dynamic range compression", JAES 63(6), 2015
  (doi 10.17743/jaes.2015.0053): ratio/threshold regressed from engineers' settings on signal features.
  (No paper titled "intelligent multiband DRC for mastering" was found; Ma's 2016 QMUL thesis "Intelligent
  tools for multitrack frequency and dynamics processing" combines target-spectrum EQ with DRC.)
- Mimilakis et al., "Deep neural networks for dynamic range compression in mastering applications",
  AES 140th, 2016, paper 9539 (abstract): a DNN predicts per-critical-band gains; rated on average equal to
  professional masters. No code or weights.
- Ozone 9: dynamics enabled only if crest factor outside 3-10 dB (§1.3). A good *gate* rule for "do we
  need a compressor at all?".

**Rules ATMaster uses** (proposal; each to be tuned on the test set):

```
need_dB   = max(0, PLR_in − PLR_target(style, target_LUFS))       // total peak-to-loudness reduction needed
limiter   = min(need_dB, L_max(style))                            // e.g. 3 dB Gentle … 6 dB Club
clipper   = min(need_dB − limiter, C_max(style))                  // 0 … 2.5 dB
glue_GR   = min(need_dB − limiter − clipper, G_max(intensity))    // 0 … 2.5 dB average GR
mb_GR     = remainder, ≤ 2 dB/band (4 at max intensity)
```

PLR_target from the genre table in §2.5 (e.g. pop at −9 LUFS → ≈ 8-9 dB; streaming −14 → ≥ 11 dB).
Glue threshold = level at the (100 − 10·(1 + 2·intensity))-th percentile of the RMS-detector distribution
over the loud mask (i.e. compress only the loudest 10-30 % of the time), ratio 1.5-2 : 1, knee 6-10 dB;
then verify by running the compressor on the analysis buffer and adjusting T in 0.5 dB steps until
the average GR over the loud mask equals glue_GR (Maddams-style iteration, deterministic).

Sidechain HPF at 60-100 Hz for the bus compressor (keeps kick/bass from pumping everything; inferred,
standard practice).

### 4.3 Multiband compression and crossovers

- **LR4**: two cascaded 2nd-order Butterworths per output; outputs −6 dB at fc and in phase; sum flat in
  magnitude ([Rane Note 160](https://www.ranecommercial.com/legacy/note160.html);
  [Linkwitz](https://www.linkwitzlab.com/filters.htm), who calls orders above LR4 "probably not useful"
  because group delay grows). The sum is an all-pass: LP + HP = (s² − √2·s + 1)/(s² + √2·s + 1)
  (2nd-order all-pass, Q = 0.7071, −180° at fc; derivation, inferred).
- **Three bands** (f1 < f2), tree split: split at f1 into low / rest; split rest at f2 into mid / high;
  **pass the low band through a 2nd-order all-pass at f2 (Q = 0.7071)** so all three bands carry the same
  phase and sum to a pure all-pass (inferred from the derivation above; the general rule "LR order 2N needs
  an Nth-order all-pass in the bands that skip a split" is discussed on the
  [JUCE forum](https://forum.juce.com/t/perfect-crossover-filters/36125)).
- Unity-gain transparency test: with all band gains 0 dB the MB output must equal the input passed through
  the composite all-pass (null against that, < −120 dB in double; T11 uses this).
- **Phase modes** ([FabFilter Pro-MB](https://www.fabfilter.com/help/pro-mb/using/processingmode)): linear
  phase = flat phase, latency, possible pre-ringing; minimum phase = zero latency, static phase shifts at
  the crossovers. Linear-phase complementary FIR: HP = δ(n − D) − LP, latency D = (L−1)/2 (inferred).
  For a mastering tool: LR4 IIR by default (no pre-echo on kicks, zero latency), linear-phase as an option
  in the render path only.
- **Crossover defaults**: Calf MB compressor 120 Hz / 1 kHz / 6 kHz
  ([calf `metadata.cpp`](https://github.com/calf-studio-gear/calf)). For a 3-band "glue" MB in mastering
  I propose 120 Hz and 2.5 kHz (auto 80-200 Hz, 1.5-5 kHz): the low split at the kick/bass boundary, the
  high split below the presence region so harshness and cymbals are controlled separately (inferred).
- **Gentle settings**: ratio 1.2-2.5, attack low/mid/high ≈ 30/15/5 ms, release 200/150/100 ms, knee 6 dB,
  ≤ 3 dB GR per band (inferred from Katz's ranges in Hilsamer & Herzog and common practice).
- **Spectral balance mode** (inferred, inspired by Ma's target-spectrum work and the Dolby target-profile
  patent): per band, makeup gain that moves the band's loudness toward the target curve's band energy,
  clamped ±2 dB, computed once from the analysis. This is the static part; the compression handles the
  time variance.

### 4.4 Dynamic EQ: harshness and sibilance

- Detection band 2-5 kHz (harshness) and 5-10 kHz (sibilance); practitioner convention, consistent with
  LANDR's Presence knob (2.5-5 kHz, §1.2) and Calf's de-esser defaults (split 6 kHz, peak 4.5 kHz, ratio 3,
  threshold −18 dBFS).
- Analysis: per 1/6-oct band in 2-10 kHz, `excess(f) = X_1/6(f) − X_1/3(f)` (resonance) and
  `rel(f) = X(f) − T(f)` (overall harshness vs target). Pick the band with max (excess + max(0, rel)) if
  > 2 dB as the dynamic EQ centre; Q from the measured width (1-2).
- Detector: band-passed sidechain (bell at the same fc, 2× Q), RMS 5 ms, threshold relative: band level
  exceeding its own loud-mask median by > 3 dB triggers cut; ratio ~3:1 equivalent, attack 2-5 ms,
  release 60-120 ms, max cut 4 dB (6 advanced). Apply on mid only (side sibilance is rarer; inferred).
- Sibilance events/s over the loud mask (count of 20 ms windows where 5-10 kHz exceeds the 2-5 kHz band by
  > 6 dB) decides whether to enable the de-esser at all.

### 4.5 Stereo

- **Correlation**: `r = Σ L·R / sqrt(Σ L² · Σ R²)` over a window (meter: 300-400 ms exponential); +1 mono,
  0 uncorrelated, −1 polarity-inverted mono (Apple Logic Correlation Meter; Studio One manual). Mono-safe if
  positive.
- **M/S**: M = (L+R)/2, S = (L−R)/2 (Matchering) → L = M+S, R = M−S.
- **Side/mid ratio per band**: 10·log10(ΣS²/ΣM²) per 1/3-oct band over the loud mask; compare to the
  target's `side_minus_mid_db`.
- **Mono bass**: vinyl "elliptical EQ" high-passes the side signal; practitioners quote 60-300 Hz and
  "below 90 Hz should be centred" ([lathetrolls](https://www.lathetrolls.com/viewtopic.php?p=59214);
  [proaudiodesignforum](https://proaudiodesignforum.com/forum/php/viewtopic.php?p=15510)). Digital
  equivalent: LR4 high-pass on S at 80-150 Hz (default 120 Hz). Only engage if low-band correlation is
  < 0.9 or low-band S/M > −15 dB (inferred); otherwise leave the bass alone (it's already mono).
- **Width**: S gain per band (above the mono-maker), ±3 dB at most; no delay-based widening (Haas delays
  < 30 ms comb-filter in mono, inferred). Guard: mono fold-down loss per band ≤ 3 dB; correlation stays
  > 0 in low/mid bands (inferred). Logic's guidance: reduce width if correlation goes negative (§1.6).

### 4.6 Saturation / exciter ("warm")

- **Exciter principle** (Aphex Aural Exciter, [US 4,150,253](https://patents.google.com/patent/US4150253A),
  expired): high-pass a copy, generate harmonics, mix a little back. Calf Exciter defaults: scope HP
  7.5 kHz (2-12 kHz), drive 8.5, even/odd blend ±10, amount.
- **Harmonics**: odd-symmetric curves (tanh, x − x³/3) → odd harmonics; asymmetry or DC bias → even
  harmonics (standard). "Warm" = mostly 2nd/3rd at low level on low-mids; "tape" = soft symmetric
  saturation + HF roll-off + subtle LF bump (inferred, common modelling practice).
- **Aliasing**: oversample 2-8× or use antiderivative anti-aliasing (Parker, Zavalishin, Le Bivic,
  "Reducing the aliasing of nonlinear waveshaping using continuous-time convolution", DAFx-16,
  [paper](https://dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA); notes by
  [Vicanek](https://www.vicanek.de/articles/AADistortion.pdf)); best combined: ADAA + 2-4× (inferred).
- **Tape model** for a richer "Warm" later: Chowdhury, "Real-time physical modelling for analog tape
  machines", DAFx-19 ([PDF](http://dafx2019.bcu.ac.uk/papers/DAFx2019_paper_3.pdf)); ChowTapeModel GPL-3.
- Level-compensate saturation by K-weighted loudness (computed on the analysis buffer), so "Warm" is not
  just "louder".

### 4.7 The final limiter

Published designs:

- **x42-dpl** ([x42/dpl.lv2](https://github.com/x42/dpl.lv2), `src/peaklim.cc`, GPL-3.0+): blocks of 8
  samples (16 > 65 kHz); lookahead = ceil(1.2 ms·fs/8)·8 (64 samples ≈ 1.33 ms at 48 kHz); per block the
  needed gain h = 1/peak, **running minimum** over the lookahead blocks; a second path on a 500 Hz
  low-passed peak with a 16 ms hold (**automatic hold for low frequencies**, which prevents bass distortion
  from fast release); per-sample smoothing z += w·(h − z) with w = 10/delay (residual e^−10 → ≈ +0.0004 dB
  worst-case overshoot); release z3 rising with w = 1/(release·fs); optional 4× TP side-chain (48-tap
  windowed sinc).
- **Signalsmith, "Designing a limiter"** ([article](https://signalsmith-audio.co.uk/writing/2022/limiter/),
  library MIT): (1) per-sample maximum allowed gain; (2) **moving minimum** over N samples; (3) smoothing
  with a **finite moving average (box or cascaded box) no longer than the min window**, which guarantees
  the smoothed gain never exceeds the allowed gain; lookahead = min length + smoothing length; release
  applied before smoothing (exponential 1/(releaseSamples+1), or cascaded); example attack 5 ms, hold
  15 ms, release 40 ms; also discusses upsampled detection and multiband limiting.
- **Matchering's Hyrax** (offline, non-causal): GR target `1 − thr/max(|x|,thr)`; attack = centred sliding
  max of 89 samples (1 ms at 44.1 kHz) then zero-phase one-pole (coef exp(−2/44)); hold = causal max-hold +
  7 Hz Butterworth LP; release = 0.27 Hz Butterworth LP; final gain = 1 − max(GR_hard, GR_attack,
  GR_release) so no sample exceeds the threshold. **Sample peak only, no true-peak detection** (from the
  code).
- **LSP Limiter** ([manual](https://lsp-plug.in/?page=manuals&section=limiter_stereo), LGPL-3): attack ≤
  lookahead, release ≤ 2× lookahead; GR patch shapes Herm/Exp/Line; Lanczos oversampling 2-8× side-chain
  only or full; ALR automatic level regulation stage.
- **Rudrich SimpleCompressor** ([docs](https://github.com/DanielRudrich/SimpleCompressor/blob/master/docs/lookAheadLimiter.md),
  GPL-3): processes the GR buffer backwards adding a dB-linear fade-in (5 ms) before each peak.
- **Airwindows ClipOnly2** (MIT): clip at −0.2 dB with one-sample interpolated entry/exit using constants
  derived from x = cos x ≈ 0.739; one-sample latency.
- **Ozone IRC IV / IRC 5** (§1.3): multiband/spectral limiting that attenuates only the bands causing the
  peaks; reduces pumping and distortion. Not needed for v1; a possible v2 "transparent" mode built from
  our 3-band split (inferred).

**ATMaster limiter (proposal)**, combining dpl and Signalsmith:

1. Input gain g (from the loudness solve) applied.
2. Detector on the **4× upsampled** signal (8× at 44.1/48 kHz in render mode; reuse the half-band cascade in
   `shared/dsp/Oversampling.h`): `need[n] = min(1, ceiling / max_k |x_up[4n+k]|)` over both channels (linked).
3. Moving minimum over W_a = lookahead (default 3 ms; 1.5 ms for "Punchy"/"Loud"), O(1) amortised with a
   monotonic deque (deterministic).
4. Release: `g_rel[n] = min(need_min[n], g_rel[n−1] + (1 − g_rel[n−1])·k_rel)` with **two-stage release**:
   k_rel fast (τ 10-50 ms) while GR < 2 dB, slow (τ 200-800 ms) beyond, crossfaded in dB (inferred, the
   "dual release" idea of dpl's LF hold / Signalsmith's cascaded release); plus an LF-hold path like dpl
   (peak of a 500 Hz low-passed copy holds the gain 10-20 ms) to avoid bass distortion.
5. Smoothing: cascaded two boxcars of length W_a/2 each (Signalsmith: never exceeds the allowed gain);
   delay the audio by W_a.
6. Apply gain to the delayed audio (base rate). Because the gain is smooth (bandwidth ≪ fs/2), the
   inter-sample peaks of `gain × audio` follow those of the audio within ~0.05 dB (inferred; verify T4).
7. Safety: oversampled soft-knee clip at ceiling + 0.0 dB in the 4× domain, then decimate; count events.
   In tests these must be 0 for all normal presets; any count > 0 is a design bug signal.

**Clipper before the limiter** for loud styles (multi-stage, inferred from common practice, Ozone's Soft
Clip option and the Hyrax "hard clip as part of the max"): 4× oversampled `y = tanh(x/t)·t` blended with
hard clip, threshold at ceiling + 1..3 dB pre-limiter, taking 1-2.5 dB off fast transients. It trades
a little THD for much less limiter pumping.

**Distortion budget**: aliasing products < −90 dBFS (T9); THD from limiting on a 50 Hz sine at 6 dB GR
< 1 % thanks to the LF hold (inferred target; dpl's reason for the LF path).

## 5. Does an ML model help?

### 5.1 What exists in the open (as of Oct 2026)

| Work | What it does | Code / weights licence | Size, CPU | Usable in an AGPL plugin? |
|---|---|---|---|---|
| DeepAFx (Ramírez et al., ICASSP 2021) / **DeepAFx-ST** (Steinmetz, Bryan, Reiss, JAES 2022, [arXiv 2207.08759](https://arxiv.org/abs/2207.08759)) | NN controls (differentiable proxies of) EQ + compressor to transfer a reference's style | **Adobe Research License**: "noncommercial research purposes only", excludes distribution ([LICENSE](https://raw.githubusercontent.com/adobe-research/DeepAFx-ST/main/LICENSE)) | README: TCN proxies RTF 0.13-0.27 on a 16-core Xeon; DSP inference RTF 0.003 | **No** (extra restrictions; not open source) |
| **Diff-MST** (Vanka, Steinmetz, Rolland, Reiss, Fazekas, ISMIR 2024, [repo](https://github.com/sai-soum/Diff-MST)) | multitrack mixing style transfer | CC BY-NC-SA 4.0 | not stated | **No**; also needs stems |
| Differentiable mixing console (Steinmetz, Pons, Pascual, Serrà, ICASSP 2021, [arXiv 2010.10291](https://arxiv.org/abs/2010.10291)); [automix-toolkit](https://github.com/csteinmetz1/automix-toolkit) | multitrack automix | Apache-2.0 code; checkpoints drums-only (ENST) | | technically yes; useless for mastering |
| **micro-TCN** (Steinmetz & Reiss, AES 152, 2022, [arXiv 2102.06200](https://arxiv.org/abs/2102.06200)) | models one analog compressor (LA-2A) | Apache-2.0 | 26-51 k params; TCN-100-C **5× real time on CPU** (Table 1) | yes, but it emulates a box, doesn't decide settings |
| FxNorm-automix (Martínez-Ramírez et al., Sony, ISMIR 2022, [repo](https://github.com/sony/fxnorm-automix)) | multitrack automix with effect normalisation | MIT (weights in repo, no separate terms) | torch | needs stems; not mastering |
| Koo et al., "End-to-end music remastering system…" ICASSP 2022 ([repo](https://github.com/jhtonykoo/e2e_music_remastering_system)) | reference-based mastering style conversion | MIT code; weights on Google Drive, no licence stated; encoder trained on MTG-Jamendo (non-commercial research data) | not stated | **risky** (weights provenance) |
| Koo et al., "Music mixing style transfer" ICASSP 2023 ([repo](https://github.com/jhtonykoo/music_mixing_style_transfer)) | FXencoder + MixFXcloner | MIT code; weights unlicensed, trained on MUSDB18 (educational only) | | **risky** |
| **ITO-Master** (Koo et al., Sony, ISMIR 2025, [arXiv 2506.16889](https://arxiv.org/abs/2506.16889)) | mastering-processor modelling with inference-time optimisation of a reference embedding; CLAP text conditioning | only an HF Space (MIT in README) | iterative gradient descent at inference | **No** in practice (no release; CPU-heavy; hard to make bit-deterministic) |
| Mimilakis et al., AES 140, 2016 | DNN per-band DRC gains for mastering | none published | | no |
| **SonicMaster** (AMAAI Lab, [arXiv 2508.03448](https://arxiv.org/html/2508.03448v3)) | text-instructed restoration/mastering, flow-matching DiT | **Apache-2.0** code and weights | **0.86 B params, 3.45 GB**; trained on 5× L40S | licence yes; **not viable on CPU** in a plugin; generative (re-synthesises the audio) |
| iZotope Ozone 12 Unlimiter / Bass Control | proprietary NN | | | n/a |
| Matchering 2.0 | **DSP** reference matching (no ML) | GPL-3.0 | trivial | **yes**; placed 3rd of all entries (behind two human engineers, ahead of Ozone and LANDR) in Benn Jordan's 472-listener blind test ([MusicTech 2024](https://musictech.com/news/gear/benn-jordan-ai-mastering-study)) |

Small classifiers (for an optional "suggest a style" feature):

| Model | Licence | Size | Notes |
|---|---|---|---|
| musicnn ([jordipons/musicnn](https://github.com/jordipons/musicnn)) | ISC (weights in repo) | ≈ 0.8 M params (≈ 3 MB) | MTT/MSD tags incl. genres; trained on non-free audio (provenance question, not a licence barrier) |
| YAMNet (Google) | Apache-2.0 | 3.7 M params, 15 MB; 69 M multiplies per 0.96 s | 16 kHz, mel 125-7500 Hz: coarse AudioSet genres only |
| PANNs MobileNetV1 / Cnn14 ([repo](https://github.com/qiuqiangkong/audioset_tagging_cnn)) | MIT code, CC BY 4.0 weights | 24 MB / 327 MB | attribution required |
| LAION-CLAP (HF ports) | Apache-2.0 / CC0 | 150-200 M params, 600-780 MB | text↔audio; too big for a plugin |
| Essentia models (Discogs-EffNet 400 styles, etc.) | **CC BY-NC-SA / BY-NC-ND** ([LICENSE](https://essentia.upf.edu/models/LICENSE), self-contradictory) | 18 MB | **No** (NonCommercial; the Essentia *library* is AGPL and fine) |
| MERT-v1-95M | CC BY-NC 4.0 | 95 M | **No** |

Runtimes: RTNeural (BSD-3; Dense/GRU/LSTM/Conv1D; designed for real time;
[repo](https://github.com/jatinchowdhury18/RTNeural)) for tiny models; ONNX Runtime (MIT) for CNN
taggers, but ~10-20 MB of library and determinism only with 1 thread (`intra_op_num_threads = 1`,
sequential), and bit-exactness across CPUs can break on SIMD/FMA paths
([ORT threading](https://onnxruntime.ai/docs/performance/tune-performance/threading.html)).

### 5.2 LLMs (e.g. via Ollama)

- Ollama models are **text-only LLMs**: they can't listen to the audio. They could map a request such as
  "warmer, more punch, less harsh" to parameter deltas, given a numeric description of the analysis.
- **LLM2Fx** (Doh, Koo, Martínez-Ramírez, Liao, Nam, Mitsufuji, WASPAA 2025,
  [arXiv 2505.20770](https://arxiv.org/html/2505.20770v2)): zero-shot JSON prediction of a 6-band PEQ (18
  params) and a reverb from words like "warm", "bright". MMD on FX embeddings (lower = better), EQ:
  random 0.53, Text2FX optimisation 0.55, Mistral-7B 0.30, Llama-3.3-70B 0.24, GPT-4o 0.22, **Mistral-7B with
  DSP features + few-shot examples 0.22**, ground truth 0.09. No code released; temperature not stated.
  Reading: the few-shot "examples" are effectively a word→EQ table; a 7B model with that table equals
  GPT-4o, which suggests the table is doing the work (inferred).
- **Text2FX** (Chu, O'Reilly, Barnett, Pardo, ICASSP 2025, [arXiv 2409.18847](https://arxiv.org/html/2409.18847v2)):
  per-instance optimisation of EQ/reverb toward a CLAP text embedding (hundreds of iterations); repo has
  **no licence**.
- Determinism: Ollama is reproducible only with `temperature: 0` + fixed `seed`, and not across Ollama
  versions, quantisations, CPU/GPU splits or context sizes
  ([makandra](https://makandracards.com/makandra/626358-ollama-generate-reproducible-results),
  community sources). It also requires the user to install and run a server; a plugin can't assume that.
- Market: no mainstream mastering product applies free-text prompts (Ozone 12's "Custom" assistant uses
  menus); Spike Stent's "Spike AI" announced natural-language mix adjustments for 2025 (status unverified,
  [SOS](https://www.soundonsound.com/node/4932588)).

### 5.3 Recommendation (honest tradeoffs)

1. **Core: no ML.** A DSP chain driven by measured features (BS.1770 loudness, PLR/PSR, LTAS vs target,
   crest factor, correlation) and **style targets + a rule table** (§6). Reasons: deterministic and
   bit-reproducible (a stated requirement; Waves even advertises it); CPU-cheap; explainable (every
   decision traceable to a number in the JSON report, fixing the "what did it do?" complaint); licence-clean;
   no 100 MB+ weights in a free plugin. Evidence that it's enough: the commercial assistants are themselves
   "analysis + target curve + module rules" (Ozone 9 manual, LANDR patent, §1), and Matchering (pure DSP)
   out-scored them in the largest public blind test.
2. **Where ML would actually help**, in decreasing value:
   - *Choosing the target* (genre/percussiveness). A hand-crafted feature classifier (spectral centroid,
     slope, low/high ratio, onset rate, percussive/harmonic ratio by median-filter HPSS) with a
     nearest-centroid rule over our target classes does this deterministically in microseconds. A tiny
     MLP (≤ 10 k params, RTNeural, trained on FMA CC-licensed features) is a possible v2 if the rules
     mis-classify; Ozone 10's genre detection was itself called "hit and miss", so the user picks the style
     and the detector only *suggests* (inferred).
   - *Learning the mapping analysis → parameters from data* (what LANDR claims: "trained on adjustments
     human mastering engineers make"). We have no paired unmastered/mastered corpus with a usable licence
     (§7.4), so this is not feasible for us now.
   - *Neural limiting/unlimiting, stem-aware EQ* (Ozone 12): large models, out of scope.
3. **Natural language (optional, later)**: a deterministic **keyword grammar** mapping ~40 descriptors to
   parameter deltas (e.g. warm → −1 dB shelf > 6 kHz, +0.5 dB 150-300 Hz, tape drive +1 dB; punch → glue
   attack +10 ms, clipper +0.5 dB, +0.5 dB 60-100 Hz; harsh → dynamic EQ 2-5 kHz amount +; air → +1 dB shelf
   12 kHz; mud → −1 dB 200-400 Hz; wide → S +1 dB > 2 kHz), with "more/less/a bit" modifiers scaling the
   deltas. The semantic-descriptor literature (SAFE / "warm" and "bright" EQ studies from the same C4DM
   group) can seed the table (inferred; to be sourced when building it). An **opt-in** Ollama bridge in the
   standalone app could translate free text into that same grammar (temperature 0, seed fixed, grammar-
   constrained JSON output), never into raw parameters; the plugin itself never depends on it.
4. **What not to do**: ship a generative model (SonicMaster-class) as the mastering engine: it
   re-synthesises the audio (can change the music), needs a GPU, and is not deterministic.

## 6. Proposed product design

Everything in this section is a design proposal (inferred from sections 1-5), not a measured fact. Numbers
are starting values to be tuned against the tests in §6.10.

### 6.1 Two deliverables, one engine

| | ATMaster plugin (VST3 / AU / standalone) | ATMaster render (CLI + standalone "file mode") |
|---|---|---|
| Input | live master bus | a WAV/AIFF/FLAC file (any rate 44.1-192 kHz, 16/24/32f) |
| Analysis | "Learn" pass: the user plays the loudest representative section (default 30 s, min 10 s, max 120 s) | exact two-pass: whole file analysed, then rendered |
| Loudness hit | ±1 LU (the learned section may differ from the song) | ±0.3 LU after one correction iteration (measured on the output) |
| Latency | reported to host (lookahead + linear-phase FIR if used), ~10-60 ms | n/a (latency compensated in the file) |
| Output | the stream | WAV 24-bit or 16-bit (TPDF dither), same rate as the input; optional 44.1 kHz SRC; a JSON report |

The DSP engine is a JUCE-free C++ library in `plugins/lacquer/Source/dsp/` (same pattern as the other
plugins' `tools/dsptest.cpp` and `tools/render.cpp`), so the CLI renderer, the plugin and the tests run the
exact same code, block-size independent and deterministic (no threads inside the engine, no
`std::random_device`, fixed dither seed, no denormals: flush-to-zero + DC-safe filters).

### 6.2 Processing chain (in order)

```
in ─► [0 Input trim / DC / sub-sonic HPF]
   ─► [1 Corrective + tonal EQ  (static, min-phase; target-match curve + style tilt)]
   ─► [2 Dynamic EQ: harshness 2-5 kHz, sibilance 5-10 kHz  (M or M/S)]
   ─► [3 Multiband compressor, 3 bands, LR4 crossovers  (gentle "glue", optional per style)]
   ─► [4 Broadband bus compressor  (RMS/peak blend, slow, low ratio)]
   ─► [5 Saturation / warmth  (oversampled, mix-able; tape or tube flavour)]
   ─► [6 Stereo: mono-below-Fc, per-band width (S gain), correlation guard]
   ─► [7 Clipper (soft, oversampled 4x, optional)]
   ─► [8 Lookahead true-peak limiter (oversampled detection, two-stage release)]
   ─► [9 Output: loudness trim already folded into limiter input gain; dither + quantize]
```

Why this order: corrective EQ before any dynamics, so compressors react to the balanced signal (Ozone 9's
Modern chain is EQ → Dynamics → Maximizer with a Dynamic EQ, and the LANDR patent's example is HPF → spatial
→ EQ → multiband compressor → limiter, §1.2-1.3). Dynamic EQ before the
compressors so harsh peaks do not drive the gain computer. Saturation after compression (more consistent
drive level). Width after saturation (saturation of S content can re-introduce low-frequency side energy;
the mono-below-Fc stage cleans it). Clipper + limiter last; loudness is reached only by the input gain into
these two stages, never by makeup gain inside earlier stages (keeps the earlier stages loudness-neutral, so
"intensity" and "target" are orthogonal).

### 6.3 Modules, parameters and safe ranges

Ranges are the limits the automatic mode may use; the advanced panel may widen some (in brackets).

**0. Input stage.** Trim −24..+24 dB. DC blocker (1-pole HP at 5 Hz). Sub-sonic HPF Butterworth 2nd order,
auto 20-30 Hz [off, 10-40 Hz]; enable at 30 Hz only if energy below 30 Hz is > −20 dB relative to the
40-80 Hz band (rumble / DC offset detection).

**1. Tonal EQ.** Two parts summed into one minimum-phase curve:
- *Match part*: smoothed difference between target curve and measured LTAS (method §3.4), at 1/3-octave
  resolution, 31 bands 25 Hz-16 kHz, realised as a cascade of 8-10 peaking/shelf biquads fitted to the
  curve (least squares on the dB curve, frequencies fixed on a log grid, gains free) or as a
  minimum-phase FIR (2048 taps @ 48 kHz via cepstral method) in the render path.
  Limits: ±4 dB per band in auto (±6 dB at intensity max), ±2 dB above 12 kHz, ±3 dB below 40 Hz;
  max slope between adjacent 1/3-oct bands 1.5 dB; overall curve mean removed (EQ is loudness-neutral
  before the limiter: re-normalise by K-weighted loudness of the EQ'd analysis buffer).
- *Style part*: tilt and shelves defined per style (§6.5).
- Amount control 0-100 % scales the match part (default from intensity).

**2. Dynamic EQ (2 bands).** Bell, Q 1.0-2.0, centre auto-picked from the harshness analysis
(band 2-5 kHz, step 1/6 oct) and sibilance (5-10 kHz). Max cut 0..−6 dB (auto ≤ −4 dB), threshold set so
the band's short-term level is cut only when it exceeds the target band level + 3 dB; attack 2-5 ms,
release 60-120 ms, ratio 3:1 equivalent; detection on Mid (or L/R linked). Sidechain: band-passed
(same bell, 2× Q).

**3. Multiband compressor.** 3 bands, crossovers default 120 Hz and 2.5 kHz (auto range 80-200 Hz and
1.5-5 kHz), LR4 with allpass compensation of the low band (§4.3). Per band: threshold auto, ratio 1.2-2.5
(auto), knee 6 dB, attack low/mid/high 30/15/5 ms (10-80, 5-40, 1-20 ms), release 200/150/100 ms
(auto program-dependent 50-500 ms), max GR in auto 3 dB per band (6 dB at intensity max). Thresholds set
from the per-band short-term level distribution: threshold = level at the 70th..90th percentile
(intensity maps 90→70), so GR happens on the louder 10-30 % of the time. Optional "balance" mode
(Ma et al. 2013, §4.3): per-band makeup that pushes band loudness toward the target curve's band
energy, clamped to ±2 dB.

**4. Bus compressor ("glue").** Feed-forward, log-domain, smooth decoupled peak detector on an RMS-ish
pre-detector (Giannoulis et al. 2012, §4.2), stereo-linked (max of L/R or mid). Ratio 1.5:1-2:1 (auto) [1-4],
knee 6-10 dB, attack 10-30 ms, release auto (dual-time-constant: 100 ms fast / 1 s slow, the way SSL-style
"auto release" works, inferred), GR target 1-2 dB average, ≤ 4 dB peak. Sidechain HPF 90 Hz (bass doesn't
pump the bus). Off for "Gentle" at low intensity.

**5. Saturation.** 4× oversampled (reuse `shared/dsp/Oversampling.h`). Two flavours:
- *Tape*: pre-emphasis shelf (+3 dB above 3 kHz) → asymmetric soft clip `y = tanh(k(x+b)) − tanh(kb)` with
  small bias b (even harmonics) → de-emphasis → gentle HF loss (1-pole LP at 16-18 kHz at max drive).
- *Tube/valve*: soft clip on the low-mids-weighted signal, mostly 2nd harmonic.
Drive 0..+12 dB equivalent, auto 0-4 dB; THD target at −14 LUFS-equivalent input: < 1 % (Warm max ~1.5 %).
Output level-compensated (K-weighted). Mix 0-100 %.

**6. Stereo.** Mid/side matrix. Mono-maker below Fc = 100-150 Hz (default 120 Hz; LR4 on the S channel:
S low band removed or scaled 0-100 %). Width per band (low-mid/high) via S gain −6..+3 dB (auto ≤ +1.5 dB,
and only if correlation > 0.3 and side/mid energy ratio below the style's target). Correlation guard: if
short-term correlation of the output < 0 for more than 1 % of the time, auto-width reverts to 0 dB.
No Haas/delay-based widening (mono-compatibility).

**7. Clipper.** Optional, 4× oversampled soft clipper (cubic/tanh blend), threshold = limiter ceiling +
0..3 dB, used for "Loud"/"Club" to take the first 1-3 dB of peaks off transients before the limiter
(multi-stage loudness, §4.7). Output of the clipper is band-limited by the decimator before the limiter.

**8. Limiter.** Lookahead brickwall with true-peak detection:
- Detection on the 4× upsampled signal (BS.1770 Annex 2 style interpolator, §2.2); required gain per
  base-rate sample = min over its 4 sub-samples of ceiling/|x|.
- Lookahead 1.5-5 ms (default 3 ms); gain curve = sliding minimum over the lookahead window, then a
  moving-average (boxcar or Hann) of the same length so the gain reaches the needed value exactly at the
  peak (the "min filter + FIR smoothing" scheme of x42-dpl / Signalsmith, §4.7).
- Release: two-stage, fast 10-50 ms for the first ~2 dB of GR, slow 200-800 ms beyond (program dependent;
  style sets the pair). Attack is the lookahead itself.
- Ceiling −1.0 dBTP default, −2.0 dBTP auto-selected when target is louder than −14 LUFS-I or for "Club"
  (Spotify guidance, §2.3); user range −3.0..−0.1 dBTP.
- Safety: a final true-peak-aware hard clip at the ceiling on the oversampled signal catches the residue
  (counts and reports any event).

**9. Output.** Dither TPDF ±1 LSB when writing 16-bit (deterministic xorshift, seed fixed), none for
24/32f. Optional noise-shaped dither is not needed for a mastering tool aimed at streaming (inferred).

### 6.4 Automatic analysis

Measurements (render mode: whole file; plugin: Learn window), all on the input to the chain:

| Quantity | How | Used for |
|---|---|---|
| LUFS-I, LUFS-S series (3 s, 10 Hz), LUFS-M | BS.1770-5 (§2.1) | target gain, sections |
| LRA | EBU Tech 3342 | compressor intensity, warning if > 15 LU |
| True peak, sample peak, PLR = TP − LUFS-I, PSR series | §2.2, §2.6 | how much limiting is needed |
| Loud-section mask | blocks with LUFS-S ≥ LUFS-I − 3 LU (and ≥ −45 LUFS) | all spectral/stereo statistics are taken here only, so intros/breaks don't skew the EQ |
| LTAS (mid and side) | Hann FFT 8192 @ 48 kHz (scale with fs), 50 % overlap, power-averaged over the loud mask, then 1/3- and 1/6-oct smoothing | match EQ, width |
| Band levels (3 MB bands + 31 bands) S-series | filterbank RMS 50 ms | MB thresholds, dynamic EQ |
| Crest factor per band, transient density | peak/RMS per 50 ms; onset rate from spectral flux (onsets/s) | attack/release (§4.2) |
| Correlation, side/mid ratio per band | per 1/3-oct band over loud mask; global correlation (400 ms windows) | width, mono-maker |
| Low-end mono check | correlation < 0.8 below 150 Hz | mono-maker Fc |
| Harshness index | energy 2-5 kHz relative to target band + peakiness (max 1/6-oct minus 1/3-oct smoothed) | dynamic EQ |
| Sibilance index | 5-10 kHz short-term excess events/s | de-esser |
| Clipping detection | runs of ≥ 3 samples at |x| ≥ 0.999 | warning "input already clipped" |
| Genre/style hint (optional) | spectral centroid, low/high ratio, onset rate; or tiny classifier (§5) | default style suggestion only |

Setting rules (each clamped to the ranges of §6.3):
1. **EQ**: diff = target(style) − LTAS_norm (both normalised to equal K-weighted loudness); smooth at
   1/3 oct, then apply a confidence weight: zero below 30 Hz and above 16 kHz, half weight 30-50 Hz and
   12-16 kHz; scale by amount(intensity); clamp; remove mean; fit biquads.
2. **Dynamics need**: PLR_in vs PLR_target(target LUFS, style). If PLR_in − PLR_target ≤ 3 dB the limiter
   alone suffices (MB and glue at minimum). Otherwise distribute the reduction: limiter ≤ 3-4 dB average
   GR (club 5-6), clipper ≤ 2 dB, glue ≤ 2 dB, MB ≤ 2 dB per band (inferred budget, to be tuned by listening
   tests; matches the multi-stage limiting practice of §4.7).
   **Already-mastered input** (min PSR_in < 8 dB and LUFS-I_in within 1 LU of the target or louder): MB, glue
   and clipper off, EQ amount halved, only the safety limiter acts, and the UI says so (the
   "already-limited mixes just get flatter" complaint, §1.9).
3. **Compressor times**: computed once from the analysis with the crest-factor law of Giannoulis et al.
   2013 (§4.2), then clamped to the style window (glue attack 10-30 ms, release 100-600 ms); if a tempo is
   detected, release is snapped to 1/8-1/4 beat inside that window (practitioner rule, inferred).
4. **Width**: target side/mid ratio per style; adjust S gain toward it by at most +1.5/−3 dB; mono-maker Fc
   = highest frequency below 200 Hz where low-band correlation is < 0.8, clamped to 80-150 Hz.
5. **Loudness**: render mode computes the chain output once with limiter input gain g0 = target − LUFS_I(after
   EQ/dynamics) and measures; then corrects g1 = g0 + (target − measured) (one secant step; limiter GR makes
   the relation sub-linear so a second step is allowed, tolerance 0.2 LU). Plugin mode uses the Learn
   window's loudness and a slow (10-30 s) auto-gain trim option that is off by default (it would ride the mix).

### 6.5 Styles

Each style is a small preset over targets (not over raw parameters), so the analysis still adapts to the mix:

| Style | Tonal target | Dynamics | Saturation | Stereo | Limiter |
|---|---|---|---|---|---|
| **Balanced** (default) | genre-neutral corpus median (§3.3) | glue 1.5:1, MB gentle | off / 0-1 dB tape | neutral, mono < 120 Hz | release 30/300 ms |
| **Warm** | target −1.5 dB tilt above 4 kHz (shelf), +1 dB 100-250 Hz | glue slower (attack 30 ms) | tape 2-4 dB drive | slightly narrower highs (−0.5 dB S >5 kHz) | slower release |
| **Bright / Open** | +1.5 dB air shelf (10 kHz), −1 dB 200-400 Hz | MB high band lighter | off | +1 dB S above 2 kHz (guarded) | standard |
| **Punchy** | +1 dB 60-100 Hz, +0.5 dB 2-4 kHz | glue attack 30 ms (lets transients through), release short; MB low band slower attack | light tube | mono < 150 Hz | clipper on 1 dB, fast release |
| **Loud** | as Balanced, less low sub (protect headroom) | more MB, glue 2:1 | light | mono < 150 Hz | clipper 2-3 dB, ceiling −2 dBTP, release 20/200 ms |
| **Gentle** | as Balanced, half amount | glue off, MB off at intensity < 50 % | off | untouched | slow, GR cap 2 dB |

(These names deliberately overlap the common market vocabulary — Warm/Balanced/Open, §1 — because users know
them; the curves behind them are ours, built per §3.3.)

### 6.6 Intensity

One knob 0-100 % (shown as Low / Medium / High at 25 / 50 / 75 %), mapped jointly:

| | 0 % | 50 % | 100 % |
|---|---|---|---|
| EQ amount (match part) | 25 % | 60 % | 100 % |
| EQ clamp | ±2 dB | ±3 dB | ±4 dB (±6 adv.) |
| MB max GR / band | 0 | 2 dB | 4 dB |
| Glue ratio / GR target | off | 1.5:1 / 1 dB | 2:1 / 2.5 dB |
| Saturation drive | 0 | style value | style value +3 dB |
| Width deviation | 0 | 50 % of suggested | 100 % |

Intensity does **not** change the loudness target: loudness is the separate Target control (so a quiet
gentle master and a loud dense one are both reachable). This avoids the most-cited complaint about
one-knob tools (louder = "better" in a quick A/B, §1).

### 6.7 Loudness targets

| Preset | LUFS-I | Ceiling | Note |
|---|---|---|---|
| Streaming (default) | −14 | −1 dBTP | Spotify/YouTube/Tidal reference level |
| Apple / dynamic | −16 | −1 dBTP | Apple Music Sound Check, AES TD1008 |
| Loud streaming | −11 | −2 dBTP | louder than platforms (they turn it down) |
| Club | −8 | −2 dBTP (−1 for DJ-only WAV) | |
| CD / loud | −9 | −0.3 dBTP (no codec) | |
| Broadcast R128 | −23 | −1 dBTP | for completeness |
| Match input loudness | measured | −1 dBTP | for "just EQ and glue" |

The UI shows the expected platform turn-down for each (e.g. "Spotify will play this 5 dB quieter") using
the targets of §2.4 (the idea of the Loudness Penalty site, loudnesspenalty.com).

### 6.8 User flow

**Plugin.** Insert last on the master bus → press LEARN, play the chorus/loudest part (progress bar and
"seconds analysed"; it auto-stops after 30 s of non-silent audio) → it proposes style (default Balanced or
the hint) and sets everything → user picks Style, Intensity, Target → A/B. Advanced panel exposes each
module with its auto value shown as a ghost marker. LEARN again re-analyses; changes in the mix after
learning are not followed automatically (deterministic, like Ozone's Assistant, §1).

**Standalone / CLI.** Drop a file → full analysis (progress) → preview playback of the processed signal
with A/B and a scrubber → Render → writes `<name>_master.wav` + `<name>_master.json` (all measurements in
and out, every parameter value, the version, so a render can be reproduced bit-exact).
CLI: `lacquerrender in.wav out.wav --style warm --intensity 60 --target -14 --ceiling -1 --report out.json`.

**Album mode** (render): analyse all tracks first, use one shared style/target and EQ "amount" policy,
set each track's loudness so the *loudest* track hits the target and the others keep their relative
loudness from the mix stage (as TIDAL/Spotify album normalisation would play them, §2.4), optionally ±1 LU
pull toward the album median. Addresses the "no album consistency" complaint (§1.9).

**Loudness-matched A/B.** Bypass plays the input gain-matched to the output by K-weighted loudness
(use LUFS-I of the analysed section in the plugin; render mode: full-file LUFS-I), with a 30 ms crossfade.
Displayed: "A/B matched (+5.2 dB)". An optional "Hear it as Spotify" mode applies the platform's
normalisation gain to both (−14 LUFS), the honest comparison.

### 6.9 Metering

- LUFS-M, LUFS-S, LUFS-I (since reset/learn), LRA, TP max (dBTP), PLR, PSR; input and output.
- GR meters: dynamic EQ (2), MB (3), glue, clipper, limiter (with history graph).
- Correlation meter (−1..+1) and a goniometer; correlation per band (low/mid/high).
- Spectrum: output LTAS (1/6-oct smoothed, slow), target curve with tolerance band (10th-90th percentile
  of the style corpus, §3.3), input LTAS ghost.
- Codec preview (optional, later): not built in (no AAC encoder in-process; licensing), instead measure
  TP at 4× and warn when PLR is small and ceiling > −1 dBTP.

### 6.10 Tests that prove it works

Automated in `tools/mastertest.cpp` (JUCE-free, like `dsptest.cpp`), run on the test set of §7:

| # | Test | Pass criterion |
|---|---|---|
| T1 | Meter compliance | EBU Tech 3341 #1-23 and Tech 3342 cases within their tolerances (±0.1 LU for M/S/I, ±1 LU LRA, TP +0.2/−0.4 dB); 997 Hz sine at −20 dBFS peak in both channels reads −20.00 ±0.05 LUFS, in one channel −23.01; K-weighting coefficients equal the BS.1770 table at 48 kHz to 1e-12 |
| T2 | Target loudness, render | |LUFS-I(out) − target| ≤ 0.5 LU for every file and every preset (goal 0.2) |
| T3 | Target loudness, plugin | ≤ 1.0 LU for full song when Learn used on its loudest 30 s |
| T4 | True peak | TP(out, 4× BS.1770 meter, and an independent 8× reference meter) ≤ ceiling + 0.1 dB |
| T5 | No clipping | zero samples with |x| ≥ 1.0 in float output; zero events reported by the safety clip in normal presets |
| T6 | Codec headroom | encode out with ffmpeg AAC 256k and libmp3lame 320k, decode: TP ≤ 0 dBFS (for the −1 dBTP presets) |
| T7 | Spectral distance | D = RMS over 1/3-oct bands 40 Hz-12 kHz of (LTAS_out − target), loudness-normalised: D_out < D_in for ≥ 90 % of files, never increases by > 0.5 dB |
| T8 | Pumping | envelope-modulation index: band-limited (20-200 Hz and 2-8 kHz) RMS envelope of out/in ratio, its spectrum's energy at 0.5-8 Hz relative to DC; below threshold (e.g. modulation depth < 1.5 dB at the beat rate) on sustained-pad material; compare against a deliberately badly-set compressor as a positive control |
| T9 | Distortion | on sine sweeps and two-tone (19+20 kHz IMD) at the target loudness: aliasing products < −90 dBFS with oversampling on; THD within style budget |
| T10 | Mono compatibility | correlation(out) ≥ min(correlation(in), 0.2) over loud mask; mono fold-down loudness drop (L+R)/2 vs stereo ≤ 3 dB; low band (<120 Hz) correlation ≥ 0.95 when mono-maker on |
| T11 | Transparency at zero | intensity 0, target = input loudness, ceiling well above TP: output equals input within −60 dB residual (null test) apart from latency |
| T12 | Determinism | two renders, different block sizes (64, 512, 4096, random) → bit-identical output |
| T13 | Sample-rate independence | same file at 44.1/48/96 kHz → LUFS-I within 0.1 LU, LTAS within 0.3 dB (20 Hz-16 kHz) |
| T14 | Real-time | plugin CPU < 5 % of one core at 48 kHz, 512 block, oversampling 4× (measure with hosttest) |
| T15 | Robustness | silence, DC, full-scale square, white noise, mono, phase-inverted channels, 1-second files: no NaN/Inf, no crash, sane report |

Subjective check (not automated): blind A/B, loudness-matched, against the input and against a reference
commercial mastering service output where licence allows, 3-5 listeners, MUSHRA-style 0-100 (the C4DM
automatic-mixing evaluations of §4.2 used similar listening tests).

## 7. Test material

### 7.1 What is legally usable, and how

| Source | Licence (checked Oct 2026) | Commit to the public repo? | Use |
|---|---|---|---|
| **Our own renders** (Audio Tools plugins, §7.2) | ours (AGPL repo; audio is our output) | **yes** | first test set, CI |
| **Synthetic signals** generated by the test code | ours | n/a (generated) | meters, limiter, distortion, robustness |
| EBU loudness test set (Tech 3341/3342) | free download from EBU; redistribution terms not stated (inferred: download on demand) | no; fetch in CI with checksum | meter conformance T1 |
| **Slakh2100-redux** ([Zenodo 4599666](https://zenodo.org/records/4599666)) | **CC BY 4.0** | yes, short excerpts + ATTRIBUTION | 2100 automatically mixed, synthetic, unmastered multitracks (mono stems, 44.1 kHz FLAC, 104 GB full) |
| **Freesound** CC0 clips ([FAQ](https://freesound.org/help/faq/)) | CC0 / CC BY 4.0 / CC BY-NC (pick CC0) | CC0 yes | drum loops, real instruments |
| **FMA** ([mdeff/fma](https://github.com/mdeff/fma)) | audio under each artist's licence (CC BY, BY-SA, BY-NC…), metadata CC BY 4.0; "meant for research" | only hand-picked CC BY/CC0 clips with attribution | released (mastered) tracks: corpus for target curves (§3.3), "already mastered" robustness |
| ccMixter ([about](https://ccmixter.org/about)) | per-upload CC (some NC) | CC BY stems only, licence recorded per file | real stems |
| OpenMIC-2018 ([Zenodo 1432913](https://zenodo.org/records/1432913)) | CC BY 4.0, 10 s snippets | yes | genre/instrument variety for classifier tests |
| Mourgela et al. mix/master metrics ([Zenodo 13683186](https://zenodo.org/records/13683186)) | CC BY 4.0, CSV only | yes | statistics of real mixes vs masters (LUFS, TP, clipping) to calibrate expectations |
| Cambridge-MT "Mixing Secrets" library | "free of charge for educational purposes only… not… for any commercial purpose"; research use "hasn't been specifically agreed with contributors"; no redistribution; asks users not to stockpile ZIPs ([FAQ](https://cambridge-mt.com/ms/mtk-faq), via Wayback; live site 403 to bots) | **no** | local listening only, by each developer, downloaded by hand |
| DSD100 / MUSDB18(-HQ) ([sigsep](https://sigsep.github.io/datasets/musdb.html), [Zenodo 3338373](https://zenodo.org/records/3338373)) | non-commercial / educational / academic only; MUSDB mixtures = sum of 4 stems (unmastered) | **no** | local listening only |
| MedleyDB v1/v2 ([Zenodo 1649325](https://zenodo.org/records/1649325)) | CC BY-NC-SA 4.0, access by request | **no** | local only |
| MoisesDB, MTG-Jamendo dataset | CC BY-NC-SA / non-commercial research | **no** | |
| Solid State Bus-Comp dataset ([arXiv 2504.04589](https://arxiv.org/abs/2504.04589)) | no licence stated; built from Cambridge-MT | no | reference for SSL-style glue behaviour |

There is **no openly licensed dataset of real unmastered → mastered commercial pairs** (searched; the
closest are SonicMasterDataset, synthetic degradations of Jamendo CC audio, and MSRBench). This is why
§5 rules out learning the analysis→parameter mapping from data.

Note on "free plugin" and NC licences (inferred): ATMaster is free, but AGPL distribution allows
commercial use by others, and "non-commercial" dataset terms restrict *our* use; keep NC material out of
the repo and out of anything shipped (target curves included).

### 7.2 Our own clips (`web/assets/samples/`)

Measured today with `ffmpeg -af ebur128=peak=true` on all 58 clips:

- Format: **AAC-LC 160 kbps, 48 kHz stereo** (`.m4a`), with the source `.mid` beside most of them.
  `scripts/samples.py` renders each preset with the plugin's CLI renderer (`at20vrender`, `at101vrender`,
  `at42render`, `at700render`, `atfs1render`, `atmdrender`, `orreryrender`, `murmurrender` in `build/`),
  **normalises to −16 LUFS with ffmpeg `loudnorm`**, then encodes AAC.
- Loudness: LUFS-I −15.4 to −19.0 (almost all −16 ± 0.5); LRA 0.1-12.9 LU (median ≈ 2 LU; `at700/ring-bells`
  12.9, `murmur/grain-chords` 9.3); TP −0.3 to −13.9 dBTP.
- Length 6.5-68 s; tempos 50-150 BPM; one instrument per clip; **no drums**.

Consequences:
1. As they are, they're fine for robustness tests and quick listening, but **not** as mastering input:
   lossy (codec artefacts above ~16 kHz and pre-echo would be "matched"), already loudness-normalised and
   peak-controlled by `loudnorm`, and single-instrument (no mix balance to correct).
2. Better: **re-render lossless, un-normalised WAVs** with the same renderers and MIDI (24-bit/48 kHz,
   no `loudnorm`, no AAC), into a test-only folder (e.g. `plugins/lacquer/tests/material/`, generated, not
   committed if large). This needs a small option in `scripts/samples.py` or a separate script; not done
   now (the brief says touch nothing else).
3. Best first test set: **our own multitracks**. Write a few short arrangements as multi-part MIDI at one
   tempo, render each part with a different plugin, add **synthesised drums** (kick/snare/hat generated
   deterministically by the test tool; Freesound CC0 loops as a second option), and sum at typical mix
   levels (LUFS-I −18 to −23, TP −3 to −6 dBTP, Mourgela et al.'s mix distribution). Suggested 120 BPM
   kit from existing material: `at101v/acid-line` + `at101v/arp-up` + `at20v/random-bleeps` (+ a bass and
   drums), all 120 BPM in `samples.json`; and a slow one from `atmd/poly-strings` + `at101v/soft-pad` +
   `at20v/glass-cathedral` (all 70 BPM). Because we own the stems we can create *controlled defects* with
   known ground truth: bass 6 dB too loud, harsh lead peak at 3 kHz, too-wide pad with low-end side energy,
   over-limited mix (PLR 6 dB), dull mix (−6 dB above 8 kHz), very dynamic mix (LRA 15 LU). Each defect is
   a test with an expected correction direction (e.g. "harsh at 3 kHz" → dynamic EQ engages within ±1/3 oct
   of 3 kHz; "too much side below 120 Hz" → mono-maker engages).

### 7.3 Synthetic signals (generated by `mastertest`)

- Meter: EBU Tech 3341 #1-23 and Tech 3342 cases (regenerated from their definitions if the EBU files
  aren't available), 997 Hz sines, pink noise at known LUFS.
- Limiter/TP: fs/4 sine at 45° phase (sample peak −3 dB below true peak), square waves, impulse trains,
  full-scale white noise, 19+20 kHz two-tone.
- Dynamics: tone bursts (attack/release measurement), a sustained pad + kick at 2 Hz (pumping detector
  positive/negative controls), sine sweep 20 Hz-20 kHz (THD and aliasing).
- Stereo: L only, R only, L = −R (correlation −1), decorrelated noise, mono.
- Edge: silence, DC offset 0.1, 1 s file, 10 min file, NaN injection in the input (must be sanitised).

### 7.4 Local-only listening set (not committed)

Each developer can keep a few Cambridge-MT multitrack projects, mixed by themselves, and a handful of
commercial releases of different genres for subjective A/B and target-curve sanity checks. Results
(numbers) can be committed; the audio cannot.

## 8. Implementation notes for this repo, and open questions

**Layout** (following `plugins/atmd/README.md`):

```
plugins/lacquer/Source/dsp/Loudness.h      BS.1770-5 meter: K-weighting any fs, 100 ms ring, M/S/I, LRA (Tech 3342), TP (Annex 2 table)
plugins/lacquer/Source/dsp/Analysis.h      loud mask, LTAS (Welch + 1/3-, 1/6-oct), crest/onsets, correlation, harshness, report struct
plugins/lacquer/Source/dsp/Targets.h       target curves (JSON-baked), styles, intensity map, rule table (§6.4-6.6)
plugins/lacquer/Source/dsp/ToneEq.h        biquad cascade + least-squares fit of the target difference
plugins/lacquer/Source/dsp/DynEq.h         2-band dynamic EQ
plugins/lacquer/Source/dsp/Crossover.h     LR4 3-band with all-pass compensation
plugins/lacquer/Source/dsp/Compressor.h    Giannoulis gain computer + smooth decoupled log detector (MB and glue)
plugins/lacquer/Source/dsp/Saturation.h    tape/tube waveshapers, oversampled (shared/dsp/Oversampling.h)
plugins/lacquer/Source/dsp/Stereo.h        M/S, mono-maker, width, correlation guard
plugins/lacquer/Source/dsp/Limiter.h       clipper + lookahead TP limiter (§4.7)
plugins/lacquer/Source/dsp/EngineMaster.h  the chain, latency, deterministic state
plugins/lacquer/Source/PluginProcessor.*   Learn pass, A/B loudness match, parameters, state
plugins/lacquer/tools/render.cpp           lacquerrender: two-pass file render + JSON report
plugins/lacquer/tools/mastertest.cpp       §6.10 tests, JUCE-free
plugins/lacquer/tools/targets.py           builds target JSON from a corpus (§3.3), offline
plugins/lacquer/docs/ref/CREDITS.md        Giannoulis/Massberg/Reiss, Pestana et al., Ma et al., Matchering, x42-dpl, Signalsmith, libebur128
```

Reuse what's already there: `shared/dsp/Oversampling.h` (2×/4×/8× Kaiser half-band cascade) for the limiter
detector, clipper and saturation; `shared/dsp/Common.h` (`Smoother`, `DcBlocker`, `isFiniteF`, `railV`);
`tools/hosttest` for CPU, state and automation checks in real hosts.

**Determinism checklist** (inferred): no threads in the engine; double for meters and filter state;
`-ffp-contract=off` for the engine translation units (FMA contraction changes results across CPUs); fixed
dither seed; all analysis in the render path on whole blocks independent of the host block size
(internal fixed 64-sample sub-blocks for gain computers); state reset on Learn; no `std::rand`, no time
or address-dependent behaviour.

**Latency** (plugin): limiter lookahead 3 ms + oversampling filters (half-band cascade group delay, a few
hundred µs to ~1 ms at 4×) + optional linear-phase EQ (off by default). Report it with
`setLatencySamples`, constant per sample rate and settings (changing lookahead at run time is a host
pain; inferred).

**Open questions / risks**:
1. Patents: LANDR US 9,304,988 (expires 2034) on target-feature-driven production, Dolby US 12,375,053 on
   target profiles for dynamic EQ. Our approach (fixed rule table, our own targets, static EQ) differs
   from the k-NN/reference database of the LANDR patent and from Dolby's dynamic EQ toward clustered
   profiles, but someone should read the claims before release (not legal advice).
2. Target curve corpus: FMA's CC subset skews to independent/electronic/experimental music (inferred from
   the archive's history); genre coverage of pop/rock/hip-hop might be thin. Check per-class counts.
3. The plugin's Learn window is not the whole song: loudness accuracy in the plugin is ±1 LU at best;
   recommend the app/CLI for final delivery and say so in the UI.
4. The pumping metric (T8) needs calibration against listening; start with a positive control (a
   deliberately fast, high-ratio compressor) and a negative control (bypass).
5. Listening tests are the real acceptance test; plan a small blind, loudness-matched comparison
   against Matchering 2.0 (which we can run locally; GPL-3) and against the unprocessed mix.

