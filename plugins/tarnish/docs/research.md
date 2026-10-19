# Tape machine / lo-fi effect: research

Compiled 2026-10-19 for a new Audio Tools product: a free, open-source (AGPL-3.0) tape machine and lo-fi
effect. It models four machines (studio reel-to-reel tape, cassette, vinyl, AM radio / telephone) with
magnetic hysteresis saturation, wow and flutter, wear, dropouts, hiss and crackle, bandwidth, bit and
sample-rate crush, speed variation and tape stop. The interface shows the reels turning and the tape
passing over the heads, with visible wear. Its tape model is shared with the delay/reverb being built at
the same time (`plugins/space`). The working name below is "the plugin"; §6 proposes names.

Conventions (the same as `plugins/lacquer/docs/research.md`):
- Every fact carries its source inline (URL and access date, or paper and section). Where only a search
  snippet, a forum post or a third-party quote was available, the text says so.
- **(inferred)** marks my own conclusions, calculations or design proposals. Values marked "calculated"
  follow from a formula in the text. §3 and §5-§8 are design proposals throughout; their numbers are
  starting values to tune with the tests in §5.
- The web search budget of this session ran out before every value could be re-verified. Values that
  come from general engineering knowledge rather than a source fetched today are marked "inferred" or
  "not re-verified today"; they should be checked against the cited standard before they are quoted in
  the manual.
- Product names are trademarks of their owners and appear here only for research. The product itself
  shows no brand names or logos: machine types are generic ("studio tape", "cassette", "vinyl",
  "AM radio / telephone"), and noise reduction is "NR", never a trademark.
- Nothing here copies proprietary text or code. Chow Tape Model is GPL: we study its published paper and
  blog posts, not its source.

---

## 0. Summary of recommendations

**Market** (§1). Chow Tape Model is the open physical reference: Jiles-Atherton hysteresis (the DAFx-19
paper ran RK4 at 16x with a real 55 kHz bias tone; the plugin defaults to RK2 at 2x and models bias as loop
width), a speed-dependent loss filter, OU-process wow, three-harmonic flutter, and "Chew"/"Degrade" wear.
Its releases have stalled since 2023, and users complained about v1's CPU, bias whine, static hiss and host
bugs. The commercial products win on UX: Wavesfactory Cassette and RC-20 put two or three macros over a
deep engine; UAD and Softube offer calibration, formulations and auto gain; Goodhertz documents every
control and gives randomness a seed; Mello-Fi and Kilohearts make tape stop tempo-synced. Common
complaints: CPU, aliasing, noise and wow too strong at modest settings, undocumented controls, licensing,
and noise that does not follow speed. Gaps nobody fills: print-through, NR mistracking, a stop/varispeed
that moves the whole machine, wow/flutter as calibrated transport spectra, vinyl scratches locked to the
rotation, and a free, maintained, physically grounded model with a modern UX.

**Model** (§2, §3.4). One chain for the magnetic machines: record EQ (IEC1/NAB/AES, cassette 120/70 µs) →
Jiles-Atherton hysteresis at 4x oversampling (RK4 default, Newton-Raphson for high drive; drive, `M_s` and
bias mapped to physical parameters) → self-erasure → transport (wow and flutter as spectral lines at the
capstan, pinch-roller and reel rotation rates, with the reel rates drifting as the pack changes, plus
Ornstein-Uhlenbeck drift; calibrated to an IEC 386-style weighted %) → reproduce losses from Wallace's
formulas (gap, spacing `54.6·d/λ` dB, thickness, azimuth) with a speed-scaled head bump → hiss injected
before the reproduce EQ, plus modulation noise → generic sliding-shelf NR with a "mistrack" control.
Dropouts are transient increases of head-to-tape spacing fed into the spacing-loss filter (dull and
hollow, as real ones sound), not amplitude gates. Vinyl: RIAA around crackle (three Poisson classes,
scratches locked to the rotation), eccentricity wow that grows toward the inner groove (`e/r`), warp in
the side channel, inner-groove HF loss and tracing distortion. AM radio: a real low-carrier AM
modulate/detect stage (overmodulation, diagonal clipping, selective fading), 2.5-5 kHz IF, static and
whistle. Telephone: 300-3400 Hz and exact μ-law. Crush with "clean" (anti-aliased, BLEP-corrected hold
edges) and "raw" modes. Tape stop from rotational dynamics (viscous plus dry friction), tempo-syncable,
MIDI-triggerable.

**Sharing with Space** (§3). Space's current tape character has a wow/flutter that is deeper than intended
(0.77 % + 1.8 % peak at zero wear), a phase step at every wow cycle (a bug), a shelf instead of a head
bump, and dropouts whose rate doubles at 96 kHz and that switch abruptly. Proposal: a JUCE-free
`shared/dsp/tape/` library (Transport, Hysteresis, Losses, Emphasis, Noise, Dropouts, Compander, Machine
profiles) used in a "full" profile here and a "light" profile in each of Space's 8 feedback lines. Lift
Lacquer's per-sample double `Os4` oversampler into `shared/dsp/` as `OsN<2|4|8>`.

**Determinism**. Bit-identical output for any block size; event rates in events per second; an optional
**timeline lock** that makes every random process a function of the host sample position, so bounces
match playback and bar 17 sounds the same however playback started.

**Visual** (§4). Direction A, "oxide and cream": a top-down transport in deep oxide brown with cream
lettering and a verdigris accent; reels whose pack radius follows `sqrt(r_h² + ℓ·h/π)`; scratches and
blemishes on a scrolling tape that reach the reproduce head exactly when each dropout is heard; a live
hysteresis (B-H) scope; a measured W&F readout; the view morphs per machine (shell, platter with tonearm,
tuning dial). An "inspect" overlay turns it into a service-manual schematic with live curves.

**Tests** (§5). 17 automated tests in the style of Lacquer's §6.10, including exact latency, block-size
determinism, wow/flutter measured by Hilbert demodulation against the set value (±10 %), hysteresis loop
sanity against a 64x reference, aliasing below −80 dBFS at 4x, event-rate independence of the sample rate,
and a CPU budget of 3 % / 5 % of one core (M4 / Ryzen 9 5900XT) for the default preset.

**Name** (§6): Tarnish (recommended), Foxing, or Spool. **Presets** (§7): 32 in six categories.
**Decisions** (§8): 18 open questions.

---

## 1. What the products do, and what users love and hate

Accessed 2026-10-19 unless a date is given. Tags: **[O]** official page, manual or published paper;
**[U]** user or reviewer opinion read today (KVR reviews and forum, GitHub issues, one Sound On Sound
review); **[BG]** general knowledge not re-checked today (verify before quoting). User opinion is thinner
than I wanted, because the search budget ran out (several review URLs returned 410/403).

Three names in the brief needed correcting:
- Baby Audio sells **TAIP**, not "TAPE MAN".
- **Wow Control** is a Goodhertz product, not AudioThing (AudioThing's tape product is **Reels**).
- Klevgrand's tape product is **DAW Cassette** (Degrader and DAW LP are its other lo-fi tools).

### 1.1 Chow Tape Model (ChowDSP), GPL-3.0, free: the open physical reference

**The paper [O]**: J. Chowdhury, "Real-time Physical Modelling for Analog Tape Machines", DAFx-19,
Birmingham, September 2019, https://www.dafx.de/paper-archive/2019/DAFx2019_paper_3.pdf.
- Reference machine: a consumer reel-to-reel at 3.75 and 7.5 ips.
- Chain: bias added → record head field H → Jiles-Atherton magnetisation M → flutter delay line → play
  head with loss filter.
- JA constants for ferric oxide: `M_s` = 3.5·10⁵ A/m, `a` = 22 kA/m, `α` = 1.6·10⁻³, `k` = 27 kA/m
  (approximated by the coercivity), `c` = 1.7·10⁻¹.
- Record head: 100 turns, efficiency 0.1, gap 6 µm; about 0.1 mA peak-to-peak input current, giving a
  field of roughly 5·10⁵ A/m peak-to-peak.
- Solver: trapezoidal rule for `dH/dt`; explicit RK4 for M, with linear interpolation for the half-sample
  values. The paper reports that lower-order implicit methods "quickly became unstable" at high
  frequencies once the bias tone was added. A small-argument approximation for the Langevin function
  below 1e-4.
- Bias: a real bias tone, 55 kHz, added to the head current, then a 24 kHz recovery low-pass. Lowering the
  bias reproduces the under-bias "deadzone"; with no input, the bias through the hysteresis produces hiss.
- Oversampling: 16x, because the 55 kHz bias needs at least 3x at 44.1 kHz and the nonlinearity more.
- Loss filter: an order-100 FIR from the inverse DFT of the spacing, thickness and gap losses, dependent on
  speed.
- Wow and flutter: measured by recording 1000 pulses through the machine at both speeds, fitted with a
  periodic function that drives a modulated delay.
- The author's own verdict: close, but not close enough to fool a listener; the missing parts were the
  machine's circuitry (two shelving filters) and wear noises.

**The plugin [O]** (manual and changelog at https://github.com/jatinchowdhury18/AnalogTapeModel;
v2.11.4, 2023-11-05; last push 2025-04-09; all major formats including CLAP, LV2 and iOS):
- Chain: input filters → tone pre-emphasis → compression → hysteresis → tone post-emphasis → Chew →
  Degrade → Wow/Flutter → Loss → output, with dry/wet.
- Hysteresis: Drive, Saturation, Bias (0-1). Solver modes RK2 (default), RK4, NR4, NR8, STN and V1. The
  manual says the Runge-Kutta modes are cheaper and less accurate than Newton-Raphson, and that STN (a set
  of tiny neural networks trained on the solver, one per grid point of saturation × bias, after Parker et
  al., DAFx-19) approximates NR8 more cheaply but "distorts more harshly at extreme settings".
- Since v2, Bias changes the loop width through the reversibility parameter `c` instead of injecting a
  tone; only the V1 mode keeps the paper's scaling and a real bias tone (changelog and manual). We take
  the idea, not the mapping.
- Loss: speed 1-50 ips (presets 3.75, 7.5, 15, 30), spacing, thickness, gap and azimuth (the manual's unit
  labels are inconsistent with the paper's microns).
- Wow/flutter: flutter rate and depth; wow rate, depth, variance and drift; sync to tape speed or tempo.
  The wow combines an LFO with an Ornstein-Uhlenbeck random process; the flutter is a periodic waveform of
  three harmonics.
- Degrade (noise, low-pass, gain loss with variance) and Chew (alternating clean and "crinkled" segments,
  with an expander and a low-pass that sweeps down with depth).
- Oversampling 1x-16x, minimum- or linear-phase, with a separate setting for offline renders; default 2x
  minimum-phase.

**Users [U]**: praised as "sounding great" and "thorough and solid" (KVR forum, December 2019,
https://www.kvraudio.com/forum/viewtopic.php?p=7601469). Complaints: v1 "very expensive on the CPU"
(fixed 16x oversampling; v2 cut it sharply); an audible "whining" from bias aliasing; static-sounding hiss;
GitHub issues on pops in one host (#17), crashes and GUI freezes (#64, #252, #298), "Degrade scales up very
quickly" (#165), a still-open call for community presets (#30) and requests for more oversampling (#308).

**Take-away (inferred)**: the physics is right and documented, and the solver menu is a good idea. What
users lack is a curated, level-matched preset set, documented units, a light CPU mode that still sounds
like tape, and maintenance. Releases have stalled since 2023.

### 1.2 Wavesfactory Cassette (€59)

[O] https://www.wavesfactory.com/audio-plugins/cassette/: tape types I-IV (automatable); models frequency
response, frequency-dependent saturation, HF compression, hiss and asperity noise, wow, flutter, random HF
loss, crosstalk and stereo imbalance. Front panel of three macros (Stability, Artifacts, Mix); a settings
page with a modular chain (each stage can be switched off), "Re-Cassette" (process several times, i.e.
generation loss) and "Random Snap". Serial-number protection, no iLok.
[U] (KVR, 2019-2020, https://www.kvraudio.com/product/cassette-by-wavesfactory): "whimsical",
"BRILLIANT PLUGIN"; one reviewer asked for print-through (the faint pre-echo one revolution early).
Take-away: three macros over a detailed engine is the UX to beat; "Re-Cassette" and the print-through
request both point at features we plan (§2.8).

### 1.3 XLN Audio RC-20 Retro Color ($99)

[O] https://www.xlnaudio.com/products/addictive_fx/effect/rc-20_retro_color: six modules (Noise: vinyl
crackle, tape hiss, hum and more; Wobble; Distortion; Digital: rate and bit reduction; Space: reverb;
Magnetic: volume drops), a "Flux" engine that adds instability across modules, and a global Magnitude
slider. Internet needed at install.
[BG] Loved for instant vibe and the Magnitude macro for transitions; criticised for a recognisable,
overused sound, hot presets and CPU cost relative to simpler tools.
Take-away: a single "amount" macro that scales the whole character is the most loved control in the
category; our "Wear" plus a global "Age" macro should play that role.

### 1.4 Goodhertz Lossy ($79), Vulf Compressor ($149), Wow Control ($95)

- **Lossy** [O] (manual v3.13, https://goodhertz.com/lossy/manual/): loss modes (standard, inverse, phase
  jitter, packet repeat, packet loss and combinations), loss amount/speed/gain, a reverb that can sit
  before or after, band filter, gate, stereo/joint/mono, perceptual or flat weighting, auto gain; about 33
  presets ("128 kbps", "Bad Cell Connection"). [BG] The reference for codec artefacts; complaints about
  subscription/bundle pricing.
- **Vulf Compressor** [O] (https://goodhertz.com/vulf-comp/manual/): based on the "vinyl sim" compression
  of a 1990s hardware sampler; compression amount, wow/flutter amount (speed 33⅓/45/78 rpm, phase 0-180°
  between channels), lo-fi amount with noise, crunch and type (analog, 1990s digital, 1980s digital),
  sidechain, HQ mode; about 26 presets. [BG] Loved for its pumping on drums and keys; criticised for the
  price of a single-purpose effect.
- **Wow Control** [O] (https://goodhertz.com/wow-ctrl/manual/): speed in Hz or synced, amount with a
  multiplier up to 128x, shape between square, sine and triangle, analog section (colour, saturation,
  15 ips / 7.5 ips / cassette), stereo phase and offset, flutter amount, noise, a random mode with seed,
  HQ mode; 69 presets.

Take-away: Goodhertz's strength is precise, well-documented controls with a **seed** for randomness and
explicit stereo phase. A seed control fits our determinism plan (§3.5). Telephone/codec artefacts are a
crowded niche where Lossy is the reference; ours should be a good "phone" machine, not a codec lab.

### 1.5 Baby Audio TAIP ($69 at launch in 2022, $99 later)

[O] https://babyaud.io/taip-plugin: described as an "AI-powered" tape saturator, a machine-learning model
of a 1970s tape machine; Drive (with auto gain), Glue, Hi-Shape, Lo-Shape, Wear (tape flanging), Noise,
Presence, Hot, single or dual machine, Mix; 135 presets.
[U] Sound On Sound (John Walden, March 2022, https://www.soundonsound.com/reviews/baby-audio-taip): a
"very cool 'make it better' plug-in" with quick, intuitive controls. KVR (2026): "very elegant sounding";
but Hi/Lo-Shape are poorly named and undocumented, and negative values can "badly damage" a source.
Take-away: auto gain on Drive matters; badly named controls get noticed.

### 1.6 Softube Tape (€99)

[O] https://www.softube.com/tape: three machine types (a linear Swiss-style, one with transformer weight,
a vintage British-style), amount, speed, high shelf, wet/dry, "Stability" for wow and flutter (with
wet/dry it flanges), a remote panel for crosstalk and more; presets by named engineers; iLok account
licensing; described as CPU-efficient.
[BG] Praised for being subtle, usable and light; licensing complaints.

### 1.7 UAD Studer A800 and Ampex ATR-102 emulations

[O] https://www.uaudio.com/products/studer-a800-tape-recorder and
https://www.uaudio.com/products/ampex-atr-102: Input, Sync and Repro paths; IPS selector; tape formulas
(four on the A800; GP9, 456, 900, 250 on the ATR); head widths ¼", ½", 1" (ATR); emphasis EQs, adjustable
calibration, auto gain; native and DSP; iLok.
[BG] Considered the realism reference (the ATR-102 especially on the master bus); heavy on DSP/CPU; the
repro-head path adds delay that confuses timing; price; iLok.
Take-away: users of the "serious" tape emulations expect calibration (reference level), EQ standards and
tape formulations as real choices, and auto gain. These are cheap to offer (§2.4, §2.20).

### 1.8 iZotope Vinyl (free)

[O] https://www.izotope.com/en/products/vinyl.html: mechanical noise, electrical noise (60 Hz hum), wear,
dust, scratch, warp (amount and shape), "record player year" (back to the 1930s), stereo/mono, gains.
[U] (KVR, average 4.5/5 from 4 reviews, https://www.kvraudio.com/product/vinyl-by-izotope): "really simple
to use", "surprisingly authentic"; but noise is "too much just at 30%", "you can't pitch the noise", the
spin-down is only average, and activation problems.
Take-away: noise controls need a usable taper (log, in dB, with 0-30 % covering the subtle range), and
noise should follow speed (our varispeed and stop move the noise too, §2.17).

### 1.9 Arturia Tape MELLO-FI (€49)

[O] https://www.arturia.com/products/software-effects/tape-mello-fi/overview: preamp (drive, boost, tone,
noise), tape (flutter, wow, wear, mechanics), one tilt filter from 12 dB low-pass to 12 dB high-pass,
stereo width by offsetting wow/flutter between channels, tempo-synced **tape stop** from 8 bars to ¼ bar
with an optional instant catch-up, a clickable tape wheel for glitches; 25 presets.
Take-away: a single tilt filter for bandwidth is efficient; clicking the transport to glitch is the kind
of interaction §4.4 proposes.

### 1.10 Klevgrand DAW Cassette ($39.99 desktop, $12.99 iOS)

[O] https://klevgrand.com/products/dawcassette: tape, head and motor "quality" knobs; tape type normal /
chrome / metal; noise reduction emulation with noise type and gain; bleed (crosstalk); head tilt
(azimuth); a 4-band EQ; low motor quality with mix gives flanging.
Take-away: "head tilt" (azimuth) as a user control exists here; rare elsewhere.

### 1.11 AudioThing Reels ($59)

[O] https://www.audiothing.net/effects/reels/: a reel-to-reel emulation with tape start/stop (separate
record and stop speeds), 16x oversampling, CLAP and Linux builds.

### 1.12 Ableton Live 12 devices

[O] https://www.ableton.com/en/live-manual/12/live-audio-effect-reference/:
- **Roar**: 7 routing modes (single, serial, parallel, multiband, mid/side, feedback, delay), 12 shaper
  types (tube preamp, diode clipper, bit crusher, fractal, shards and more), 9 filter types, a modulation
  matrix (two LFOs, envelope follower, noise), feedback and a compressor.
- **Redux**: rate with jitter, pre- and post-filter (with octave), bits, shape, DC shift. The pre/post
  filter switch is the "clean vs raw" choice of §2.16.
- **Erosion**: noise or sine modulation of the signal (amount, frequency, width): a degradation by
  modulation, close to our "modulation noise".
- **Vinyl Distortion**: tracing model and pinch distortion (drive, frequency, Q; pinch stereo/mono), soft
  or hard, and crackle with density and volume. It names the two groove distortions of §2.13.
- Live has no dedicated tape device; the nearest are Echo's wobble and noise and the repitch modes of
  Echo and Delay.

### 1.13 Tape stop tools

[O]: Kilohearts **Tape Stop** (free with an account: play, stop time, start time, curve;
https://kilohearts.com/products/tape_stop); dblue **TapeStop** (legacy Windows 32-bit VST2; Glitch² has a
newer one; https://www.illformed.com/); Cableguys **TimeShaper** (€29: drawn playhead curves from 1/128 to
32 bars, MIDI or audio trigger, multiband; https://www.cableguys.com/timeshaper); Image-Line **Gross Beat**
(36 time and 36 volume slots over a 2-bar buffer;
https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Gross%20Beat.htm).
Take-away: stop time, start time and one curve parameter are the expected controls; tempo sync and MIDI
trigger are expected in 2026. None of the tools found ties the stop to a machine model (the loss filter,
head bump and hiss should all slide with speed; agent survey, inferred from the documented controls).

### 1.14 Others worth knowing

- **Aberrant DSP SketchCassette II** ($36, no copy protection; https://aberrantdsp.com/plugins/sketchcassette/)
  [O]: 12 tape type/quality profiles, age, two saturation models, hiss, dropouts with stereo width,
  wow/flutter with tempo sync and an FM mode (wow modulates the flutter rate), an NR encode compressor;
  61 presets. [U] "sounds amazing", "just works" (KVR 5/5).
- **Caelum Audio Tape Cassette 2** (free) [O]: saturation ±24 dB, low-pass 5-22 kHz, noise −80 to 0 dB,
  wow and flutter 0-100 %, a cassette impulse response, oversampling 2x-16x, auto gain. **Tape Pro**
  (£29.99): saturation types including "hysteresis" and "remanence" controls, 12 response IRs, sampled
  noise. (KVR product pages.)
- **Kiive Tape Face** ($100, Plugin Alliance) [O]: three modes (a specific vintage machine, transparent,
  lo-fi), bias switch, speeds, low boost, saturation, width, CPU-friendly or linear-phase oversampling.
- **Airwindows ToTape6/7/9 and IronOxide5** (MIT; https://www.airwindows.com/totape6/,
  https://github.com/airwindows/airwindows) [O]: head bump, flutter, bias, an encode/decode pair
  ("Dubly"), a clip stage; no GUI, no DRM. ToTape6 notes that flutter adds small varying latency and that
  dry/wet with flutter flanges. [U] Mixed CPU comments.
- Academic: Mikkonen, Wright, Moliner and Välimäki, "Neural modeling of magnetic tape recorders" (2023,
  https://arxiv.org/abs/2305.16862; only the abstract was seen): black-box models of a real recorder,
  including its delay trajectories. Relevant as a measurement method for wow/flutter.

### 1.15 Synthesis: what users love and complain about

Loves (from [U] where available, [BG] otherwise):
1. One macro that scales the whole character (RC-20 Magnitude, Wavesfactory Stability/Artifacts).
2. Subtle glue on buses (Softube, TAIP, UAD).
3. Convincing extreme settings: under-bias deadzone, chewed tape (Chow).
4. Tempo-synced wow and tape stops (Mello-Fi, SketchCassette, Wow Control).
5. Stereo decorrelated wow for width (Mello-Fi, Vulf, Wow Control).
6. Fun presets, often by named artists.
7. Low CPU in the "lite" designs (Softube, Airwindows).
8. Free or DRM-free options (Airwindows, Chow, Kilohearts, iZotope Vinyl).

Complaints:
1. CPU of physical/high-oversampling models (Chow v1, UAD).
2. Aliasing and bias whine at low oversampling (Chow).
3. Noise and wow too strong at modest settings (iZotope Vinyl).
4. Undocumented or unclear controls and units (TAIP Hi/Lo-Shape; Chow's manual units).
5. Licensing friction: iLok, online activation.
6. Host bugs: crashes, pops, render freezes (Chow issues).
7. Flutter adding latency or smear; flanging when dry/wet is used with flutter (Airwindows, Chow).
8. Noise that does not follow speed or pitch (iZotope Vinyl).
9. Price of single-purpose effects.
10. Preset quality and level mismatches (Chow had to normalise preset levels).

Gaps no product fills well (survey synthesis plus my inference):
- Print-through (pre/post-echo): requested by a user, not found in any product surveyed.
- NR mistracking (encode/decode level and azimuth mismatch): partly in SketchCassette, DAW Cassette and
  Airwindows' encode/decode pair; nowhere as a single control.
- Tape stop and varispeed that move the whole machine (bump, losses, hiss) with speed.
- Wow and flutter as per-machine spectra with drifting reel lines and scrape flutter / modulation noise,
  calibrated to a standard unit. Most products use LFOs; Chow uses a measured three-harmonic flutter plus
  an OU wow.
- A free, open, physically grounded model with a modern UX: curated level-matched presets, documented
  units, a light CPU mode, current maintenance.
- Explicit latency and mono-compatibility behaviour with modulated delays.
- Vinyl crackle with scratches locked to the rotation and eccentricity wow tied to groove position (not
  found in the products surveyed; inferred from their documented controls).

### 1.16 What to bring, and how to go beyond (inferred)

Bring:
- Three or four macros on the front (Machine, Saturation, Wear/Age, Wow) over a detailed engine one click
  away (Wavesfactory's layout, Chow's depth).
- Real EQ standards, formulations and reference-level calibration (UAD's credibility), auto gain on
  drive (TAIP, UAD).
- Tempo-synced, MIDI-triggerable tape stop with start/stop times and a curve (Kilohearts, Mello-Fi).
- A solver menu with honest labels (Chow), a seed (Goodhertz), stereo phase of the wow (Vulf, Wow Control).
- Generation loss in one control (Wavesfactory's Re-Cassette).
- No DRM, all formats the project already builds (VST3, AU, standalone; CLAP is worth considering, as
  Chow and Reels show).

Go beyond:
1. **Dropouts as spacing loss** (HF-first, physically derived) rather than gain gates.
2. **Wow/flutter as transport spectra** calibrated in IEC-weighted %, with reel lines that drift as the
   pack changes, shown on a measured readout.
3. **Print-through** (post-echo in real time, optional pre-echo with honest latency).
4. **NR mistrack** as a single control on a generic compander.
5. **Speed moves the machine**: stop, spin-up and varispeed slide bump, losses and hiss.
6. **Vinyl realism**: scratches locked to rotation, eccentricity wow growing toward the label, warp in the
   side channel, tracing distortion.
7. **AM radio as a real modulate/detect chain** (overmodulation, diagonal clipping, selective fading,
   tuning).
8. **Determinism**: block-size independence and timeline-locked randomness, so a bounce equals playback.
9. **A transport view that shows the model**: blemishes reaching the head when heard, a live hysteresis
   loop, reels obeying the pack geometry.
10. **One shared, documented tape library** used by two products, with measured test numbers in the
    README (the Audio Tools habit).

---

## 2. Physics and DSP

The plugin models four machines that share one skeleton: a **record path** (pre-emphasis, a nonlinear
medium), a **transport** (speed errors: wow, flutter, stop), a **reproduce path** (losses that depend on
wavelength, then de-emphasis) and **noise sources** (hiss, crackle, static), followed by a **degradation
stage** (dropouts, bit/sample crush). The sections below go through each piece with the numbers a model
needs, then §2.20 collects the recommended parameter ranges.

### 2.1 How a tape machine colours the sound (the chain to model)

Record amplifier → record EQ (HF pre-emphasis) → record head, where audio is summed with a 50-150 kHz
**bias** tone → tape (magnetic hysteresis, saturation, self-erasure at short wavelengths) → transport
(speed fluctuations) → reproduce head (gap loss, spacing loss, thickness loss, head bump, azimuth) →
reproduce EQ (de-emphasis, plus the LF boost of the NAB curve) → line amplifier.

The order matters in two ways (inferred from the chain; Chowdhury's DAFx 2019 model follows the same
order, §1.1):
- The record pre-emphasis comes *before* the nonlinearity. Treble therefore hits saturation earlier than
  bass, and that is why a hot cassette loses its top end ("HF compression", §2.12) rather than just
  distorting.
- The wavelength losses come *after* the nonlinearity and *before* de-emphasis. Distortion products are
  filtered by the playback losses, and hiss generated on the tape is shaped by the playback EQ (§2.10).

### 2.2 Magnetic hysteresis: the Jiles-Atherton model

**Documented.** Chowdhury's DAFx 2019 paper ("Real-time Physical Modelling for Analog Tape Machines",
§1.1) models the tape magnetisation M driven by the head field H with the Jiles-Atherton (JA) equation
(D. C. Jiles and D. L. Atherton, "Theory of ferromagnetic hysteresis", *J. Magn. Magn. Mater.* 61,
1986, pp. 48-60). In the usual notation:

- anhysteretic magnetisation `M_an = M_s · L(Q)`, with `Q = (H + α·M) / a` and the Langevin function
  `L(x) = coth(x) − 1/x`;
- `δ = sign(dH/dt)`; `δ_M = 1` when `(M_an − M)` has the sign of `dH/dt`, else 0 (this suppresses
  non-physical negative susceptibility);
- the magnetisation follows

  `dM/dt = [ (1−c)·δ_M·(M_an − M) / ((1−c)·δ·k − α·(M_an − M)) · dH/dt + c·(M_s/a)·L'(Q)·dH/dt ]
           / [ 1 − c·α·(M_s/a)·L'(Q) ]`

  with `L'(x) = 1/x² − 1/sinh²(x)`.

Five physical parameters: `M_s` saturation magnetisation, `a` anhysteretic shape (domain density),
`α` inter-domain coupling, `k` pinning (sets loop width / coercivity), `c` reversibility (0 = fully
irreversible, 1 = no loop). The paper's tape values and the plugin mapping are in §1.1.

**What each control should do** (inferred, consistent with the Chow Tape Model controls in §1.1):
- *Drive* scales H (input gain into the medium). This is the main "saturation" control.
- *Saturation* (or "tape formulation") scales `M_s`. Lower `M_s` = earlier, harder ceiling.
- *Bias / width* moves `c` (and/or `k`): a properly biased tape is almost anhysteretic, i.e. linear at
  low level with a soft ceiling (high `c`, narrow loop); an **under-biased** tape shows the full loop:
  crossover-like distortion at low level, grit, brighter top. An **over-biased** tape is cleaner and
  duller (the bias partly erases short wavelengths). Real machines are aligned to the "overbias" point
  for low distortion (typically 2-4 dB past peak output at 10 kHz on studio machines; from alignment
  practice, to verify against a machine manual before quoting).

**Numerical pitfalls** (inferred, standard for JA):
- `L(x)` and `L'(x)` lose precision near `x = 0`: use the series `L(x) ≈ x/3 − x³/45 + 2x⁵/945` and
  `L'(x) ≈ 1/3 − x²/15 + 2x⁴/189` for `|x| < 1e-3` (or larger in float). Without this the model emits NaN
  on digital silence, the commonest JA bug in public code.
- The denominator `(1−c)·δ·k − α·(M_an − M)` can approach zero; clamp it away from zero, keeping its sign.
- `δ` from the sign of `dH/dt` flips on numerical noise in silence; use a small dead band or hold the
  last sign.
- Run the state in double. M and H are of order 1e5 (A/m), and products like `α·M` are cancellations of
  large numbers; float works only with normalised units (divide H and M by `M_s`, which the plugin should
  do anyway).

**Solvers.** JA is a stiff-ish ODE driven by the audio. Options, with their trade-offs:

| Solver | Cost per sample (inferred, function evaluations) | Behaviour |
|---|---|---|
| Forward Euler | 1 | Unstable at high drive unless heavily oversampled. Not recommended. |
| RK2 (Heun / midpoint) | 2 | Good at ≥ 4x oversampling; error shows as a slightly wrong loop shape, not as instability at moderate drive. |
| RK4 | 4 | The paper's choice (§1.1, at 16x with a real bias tone); accurate at 2-4x without the tone; still explicit, so very high drive can misbehave. Chow's plugin defaults to RK2 at 2x. |
| Newton-Raphson on the implicit (trapezoidal) step, fixed 4 or 8 iterations | 4-8 with derivative | Unconditionally better stability; fixed iteration count keeps CPU constant and deterministic. |
| Learned solver (Chow's "STN", a tiny network trained on the solver) | 1 small MLP | Fast, but needs training data and a training pipeline; output not bit-reproducible across compilers if fused-multiply-add differs. |

The input derivative `dH/dt` must be estimated from samples. The paper uses a trapezoidal estimate
(`Ḣ[n] = 2·fs·(H[n] − H[n−1]) − Ḣ[n−1]`), which is consistent with the trapezoidal integration of the
rest of the system but can ring at Nyquist; a plain backward difference is cheaper and more damped
(inferred).

**Recommendation** (inferred): implement JA in normalised units with **RK4 at 4x** as the default (the
reference behaviour) and **NR with 4 fixed iterations** as the "high drive" or "accurate" option; RK2 at
2x for the "light" profile used inside the delay network (§3). Every solver runs a fixed number of
operations per sample, so CPU is constant and the output is deterministic.

**Cheaper alternatives** worth keeping as fall-backs (inferred):
- Memoryless `tanh` with pre/de-emphasis around it (what Lacquer's "tape" flavour does,
  `plugins/lacquer/Source/dsp/Saturation.h`): no loop, no level-dependent phase, but 10x cheaper.
- A "hysteretic tanh" `y[n] = tanh(g·x[n] + β·y[n−1])`: with `β < 1` it is only a frequency-dependent
  lag (the "loop" closes at low frequency, unlike magnetic hysteresis, which is rate-independent); with
  `β > 1` it becomes bistable (Schmitt-trigger-like) and far too harsh. Cheap, not physical.
- Preisach models: more accurate for minor loops, far more expensive (a density over a 2-D plane). Not
  worth it here.

### 2.3 Oversampling and the bias question

**Bias.** Real bias is a 50-150 kHz sine (consumer cassette decks roughly 85-105 kHz, studio machines
higher; from service-manual practice, inferred range). Simulating it directly needs a sample rate several
times the bias frequency (≥ 500 kHz) and a filter to remove it afterwards. The paper does exactly that:
a 55 kHz tone added to the head current, 16x oversampling, a 24 kHz recovery low-pass (§1.1). Chow's
plugin later replaced the tone by a change of loop width through `c` (only its "V1" mode keeps the tone),
and early users still heard a "whining" bias alias (KVR thread, §1.1). For our design the practical
choice is the same as the plugin's: model the *effect* of bias (the anhysteretic linearisation) through
`c`/`k`, offer under/over-bias as a character control, and never run a tone (inferred). What is lost: the
hiss that the bias tone itself generates through the hysteresis on silence; our hiss generator (§2.10)
replaces it.

**Oversampling factor.** A JA magnetiser is a strong nonlinearity with memory. Without oversampling the
harmonics of a 5 kHz tone at high drive alias straight back into the band. Recommended (inferred, to be
verified by test T6 in §5):
- 2x for the light profile (in the feedback loop of the delay, where the loop low-pass removes most of the
  aliasing on each pass);
- 4x default for the Tape plugin;
- 8x as an offline/"render" option.

**What to reuse.** Two oversamplers already exist in the repo:
- `shared/dsp/Oversampling.h`: float, block-based, 2/4/8x, cascaded Kaiser half-bands (95 taps for the
  stage at the base rate, 31 for the others). It multiplies every coefficient, including the zero taps of
  the half-band, and its `Decimator` *adds* `gain·y` into the output (designed for summing synth voices).
- `plugins/lacquer/Source/dsp/Saturation.h`: `HalfbandD` + `Os4`, double, per-sample, skips the zero taps
  (half the multiplies), and takes the nonlinearity as a lambda (`os.process(x, f)`). Latency is an
  integer by construction: 54 base-rate samples (stated in the header and verified by Lacquer's
  `mastertest`).

`Os4`'s per-sample lambda interface is exactly what a stateful hysteresis needs. The recommendation is to
lift `HalfbandD`/`Os4` into `shared/dsp/` as a templated `OsN<factor>` (2x, 4x, 8x), keep Lacquer's
bit-identical test as the guard for the move, and have both Tape and Space include it (§3.3).

### 2.4 Record and reproduce equalisation

Time constants of the standard reproduce curves (the record curve is the complement, plus machine-specific
corrections):

| Machine / speed | Standard | LF time constant | HF time constant | HF turnover |
|---|---|---|---|---|
| Studio, 30 ips (76 cm/s) | AES | none | 17.5 µs | 9.1 kHz |
| Studio, 15 ips (38 cm/s) | IEC1 / CCIR | none | 35 µs | 4.55 kHz |
| Studio, 15 ips | NAB (IEC2) | 3180 µs | 50 µs | 50 Hz / 3.18 kHz |
| Studio, 7.5 ips (19 cm/s) | IEC1 / CCIR | none | 70 µs | 2.27 kHz |
| Studio, 7.5 ips | NAB (IEC2) | 3180 µs | 50 µs | 50 Hz / 3.18 kHz |
| Cassette, 1⅞ ips (4.76 cm/s), Type I (ferric) | IEC | 3180 µs | 120 µs | 50 Hz / 1.33 kHz |
| Cassette, Type II / IV (chrome, metal) | IEC | 3180 µs | 70 µs | 50 Hz / 2.27 kHz |

Sources: reeltoreel.nl, "NAB vs. IEC" (https://www.reeltoreel.nl/knowledgebase/books/audio/page/nab-vs-iec,
accessed 2026-10-19) for the 7.5/15 ips NAB and CCIR values; IASA TC-04 §5.4.10 "Replay equalisation"
(https://www.iasa-web.org/node/490, seen in search results 2026-10-19; the page returned 404 on fetch) for
the cassette values; the 30 ips AES 17.5 µs value is the commonly published one and was not re-verified
today. Turnover frequency = 1/(2π·τ) (calculated). Older tapes used other values (90/140/200 µs at 3¾ ips
before 1968, per IASA and sound-au.com project 247).

Why the plugin should implement them as real filters (inferred):
- The emphasis around the nonlinearity is what makes 7.5 ips and cassette *sound* different from 15 ips
  under drive, not only the frequency response.
- A record curve with 120 µs boosts 10 kHz by about 18 dB relative to the mids at the record head
  (|1 + jωτ| at 10 kHz, τ = 120 µs ≈ 7.6 → 17.6 dB; calculated). That is why cassette treble saturates
  first.
- NAB's 3180 µs LF boost on playback (with the complementary record cut) lifts LF hum and rumble; this is
  a reason to keep an "EQ standard" switch on the studio machine.

Implementation: first-order shelves via bilinear transform with frequency pre-warping, run at the
oversampled rate when they sit inside the oversampled region (the record curve does; the reproduce curve
can sit outside). The record and reproduce curves must cancel to within ±0.05 dB at low level with the
nonlinearity bypassed (test T11).

### 2.5 Head bump

Below a certain frequency the recorded wavelength becomes comparable to the length of the reproduce
head's contact face, and the head picks up flux from its outer edges as well as the gap. The response
shows a peak (the "bump") and a series of dips below it, roughly at `λ ≈ L/n` (inferred from the
standard explanation; exact shape depends on the head contour).

- The bump frequency scales with tape speed: `f_bump ≈ v / L_eff`. Doubling the speed moves the bump up an
  octave (forum consensus summarised in search results: e.g. a 30 ips bump at 80-120 Hz becomes 40-60 Hz
  at 15 ips; homerecording.com "30 ips / 15 ips mastering", https://homerecording.com/bbs/threads/30ips-15-ips-mastering.282941/,
  accessed 2026-10-19; anecdotal, not a measurement).
- A head datasheet (Nortronics studio series) gives an LF cutoff of 25 Hz at 15 ips and 50 Hz at 30 ips
  (https://www.discolathe.com/.pdfs/Nortronics_Silvr_pp1-9.pdf, accessed 2026-10-19).
- Typical magnitude: +1 to +3 dB at the bump with a −1 to −3 dB ripple below it (inferred from published
  response plots; ±2 dB is described as audible in the whatsbestforum thread found today).

Recommended model (inferred): a peaking biquad at `f_bump` (Q ≈ 1.5-2.5, +1 to +3 dB) followed by a
second-order high-pass at about `0.5·f_bump` and an optional low-Q dip at about `0.6·f_bump`.
Speed-scaled defaults: 30 ips ≈ 100 Hz, 15 ips ≈ 55 Hz, 7.5 ips ≈ 30 Hz; cassette 40-60 Hz with a smaller
peak (+1 dB) because cassette heads are shorter but the tape also much slower (a guess to be checked
against published cassette deck response plots before freezing).

Compare with Space's current "bump" (`plugins/space/Source/dsp/Network.h`): a one-pole low-pass at 110 Hz
mixed in at +0.18. That is a low shelf of about +1.4 dB (20·log10 1.18), not a resonant bump; see §3.

### 2.6 Wavelength losses: gap, spacing, thickness

With tape speed `v` and frequency `f`, the recorded wavelength is `λ = v / f`:

| Speed | λ at 1 kHz | λ at 10 kHz | λ at 20 kHz |
|---|---|---|---|
| 30 ips (0.762 m/s) | 762 µm | 76 µm | 38 µm |
| 15 ips (0.381 m/s) | 381 µm | 38 µm | 19 µm |
| 7.5 ips (0.191 m/s) | 191 µm | 19 µm | 9.5 µm |
| Cassette (0.0476 m/s) | 47.6 µm | 4.8 µm | 2.4 µm |

(calculated)

Three classic loss terms (R. L. Wallace Jr., "The Reproduction of Magnetically Recorded Signals", *Bell
System Technical Journal* 30 (4), Oct. 1951, p. 1145; cited via patents found 2026-10-19, e.g.
https://patents.google.com/patent/US8111473):

- **Spacing loss** (head-to-tape distance `d`): amplitude `e^(−2π·d/λ)`, i.e. `54.6·d/λ` dB.
- **Gap loss** (reproduce gap length `g`): `|sin(π·g/λ) / (π·g/λ)|` (first null at `λ = g`; practical heads
  have an effective gap about 1.1x the physical one).
- **Thickness loss** (coating thickness `δ`): `(1 − e^(−2π·δ/λ)) / (2π·δ/λ)`.

Worked examples (calculated):
- Cassette, 10 kHz, 1 µm of spacing (a little oxide shed or dust): 54.6 × 1/4.76 = **11.5 dB** loss. The
  same 1 µm at 15 ips costs 1.4 dB at 10 kHz. This one formula explains why worn cassettes lose their
  top end and studio tape does not, and it is the physical basis for our "wear" control.
- Gap of 1.5 µm (cassette playback head, plausible value; inferred) at 10 kHz: π·g/λ = 0.99 → −1.4 dB; at
  15 kHz: −3.4 dB.
- Gap of 3 µm (studio reproduce head, plausible; inferred) at 15 ips and 20 kHz: −0.7 dB.

Recommended model (inferred): compute the combined loss magnitude on a frequency grid from (v, g, d, δ),
fit a minimum-phase low-order filter once per parameter change (e.g. two biquads, or a 32-64-tap
minimum-phase FIR at the base rate), and crossfade between old and new filters when "wear" or "speed"
moves. Chow's plugin does a similar loss filter from speed/spacing/thickness/gap (§1.1). Modulating `d`
in time gives physically consistent dropouts (§2.9).

### 2.7 Azimuth error

If the reproduce gap is tilted by angle θ relative to the record gap, the head integrates across the track
width `w` with a time skew, giving a loss `|sin(π·w·tanθ/λ) / (π·w·tanθ/λ)|` (standard result, inferred
here), plus a delay difference between the two tracks of a stereo head of `Δt = s·tanθ / v`, where `s` is
the distance between track centres.

Examples (calculated):
- Cassette stereo track ≈ 0.6 mm wide, θ = 0.2° (a few minutes of arc too many, common on cheap decks):
  `w·tanθ` ≈ 2.1 µm → at 10 kHz (λ 4.76 µm) the loss is about 3 dB; at 15 kHz, about 8 dB. The two
  channels (centres roughly 0.9 mm apart; inferred) are skewed by about 66 µs, so L+R starts to comb.
- Studio ¼" two-track (track ≈ 2 mm), θ = 0.05° at 15 ips: `w·tanθ` ≈ 1.7 µm, negligible against
  λ = 19 µm at 20 kHz.

As a control: "azimuth" (0-1) maps to θ per machine; its sonic signature is HF loss *plus* an
inter-channel delay that makes the mono sum dull and phasey. Cheap to implement (a fractional delay on one
channel plus the loss term in the loss filter) and almost never offered (inferred from §1).

### 2.8 Print-through

The field of one layer of tape on the reel magnetises adjacent layers over time. On playback a faint copy
of the signal appears one reel revolution *before* (pre-echo) and *after* (post-echo) the original. Its
level depends on storage time, temperature and tape type; −45 to −60 dB relative to the signal is the
range usually quoted for open-reel tape (inferred from archival literature; not re-verified today).

Timing (calculated, from the circumference of the layer):
- Open reel at 15 ips, pack radius from 5 cm (hub) to 13 cm (full 10.5" reel): circumference 0.31-0.82 m
  → echo spacing **0.8-2.1 s**, getting longer toward the outside of the pack.
- Cassette at 1⅞ ips, pack radius 11-25 mm: circumference 69-157 mm → **1.5-3.3 s**.

Model: two taps of a delay line at ±(one revolution), with the revolution time following the pack radius
of the current position (§2.17 "reel geometry"), low-passed (print-through favours long wavelengths),
−40 to −70 dB. The pre-echo needs the signal in advance, i.e. latency of 1-3 s, which a plugin cannot add.
Options: offer only the post-echo in real time, or a "pre-echo" mode that is honest about the 2 s latency
it reports (inferred). A plugin delay compensation of 2 s is possible in DAWs but unusual; this is a
decision for §8.

### 2.9 Wow and flutter

**Definitions and measurement.** Wow is slow speed variation (below ~4-6 Hz), flutter faster (up to
~100-200 Hz), **scrape flutter** the longitudinal vibration of the tape over heads and guides (above
~100 Hz, into the kHz; measured on a 10 kHz tone). The measurement standards IEC 386 (= DIN 45507 =
BS 4847 = CCIR 409-3 = AES6-2008) use a 3.15 kHz test tone, a frequency discriminator, a weighting filter
that peaks around 4 Hz (where flutter is most audible) and a slow quasi-peak rectifier; the reading is the
"weighted peak". The 1988 IEC amendment added a "2-sigma" statistical method. US practice often quotes
unweighted or weighted RMS on 3 kHz (NAB/JIS). Record-replay flutter is roughly twice the replay-only
figure. Sources: Wikipedia, "Wow and flutter measurement" (https://en.wikipedia.org/wiki/Wow_and_flutter_measurement,
accessed 2026-10-19); IEC 60386:1972 catalogue entry (https://online.standard.no/en/iec-60386-1972-ed1);
the vendor meter sheet giving the IEC weighted band as 0.2-200 Hz
(https://parts.ttech.vn/wow-flutter-meter-mk-669c-keisoku-p107431.aspx). The exact weighting curve is in
the paid standard; we should buy IEC 60386 (or AES6-2008) before writing the measurement test, or derive
the curve from a published plot and say so.

**Typical figures** (weighted; "documented" where a source is given, otherwise from manufacturer
specifications as commonly published and marked inferred):

| Machine | Typical weighted W&F | Character |
|---|---|---|
| Studio reel-to-reel, 15/30 ips | ≈ 0.02-0.05 % | Inaudible; Wikipedia gives ~0.02 % as the professional figure |
| Home reel-to-reel, 7.5 ips | 0.05-0.1 % (inferred) | Barely audible on piano |
| High-end cassette deck | ≈ 0.08 % (Wikipedia); 0.03-0.05 % for the best three-head decks (inferred) | Audible on sustained piano |
| Average cassette deck | 0.1-0.15 % (inferred) | Slight "seasick" on held notes |
| Portable / personal cassette player | 0.15-0.4 % (inferred) | Clearly wobbly; motion makes it worse |
| VHS linear audio | ≈ 0.2 % (Sencore tech tip, "conventional VCR"; https://xdevs.com/doc/Sencore/Sencore_Tech_Tips/TT190p%20-%20How%20VCR%20Hi-Fi%20Stereo%20Works.pdf) | Wobbly, plus servo hunting |
| Turntable, direct drive / good belt | 0.02-0.1 % (inferred) | Dominated by record eccentricity, §2.13 |
| Worn / broken anything | 0.5-3 % (inferred) | The lo-fi effect people buy plugins for |

Audibility anchors (Wikipedia, same page): ~1 % pitch change is detectable by a good musical ear in
isolation; a semitone is ~6 %.

**Spectrum** (inferred from transport mechanics): flutter is not noise with a flat spectrum. It is a set
of lines at the rotation frequencies of the transport parts, each spread by drift, plus a broadband floor:
- capstan: `f = v / (π·D_capstan)`. A 2 mm cassette capstan gives 7.6 Hz; a 1" studio capstan at 15 ips
  gives 4.8 Hz; a direct-drive capstan motor larger still (calculated; capstan diameters vary by machine);
- pinch roller: `v / (π·D_roller)`, about 1.5 Hz for a 10 mm cassette roller (calculated);
- reels: `v / (2π·r_pack)`, changing as the tape winds: cassette 0.3-0.7 Hz, studio reel at 15 ips
  0.47-1.2 Hz (calculated from the pack radii in §2.8). The supply reel speeds up as it empties: the wow
  lines drift in frequency across a long recording. No product in §1 is documented as modelling this.
- motor cogging and servo hunting (VHS, cheap cassette): 5-30 Hz lines;
- scrape flutter: band-limited noise, 1-5 kHz (inferred; commonly cited range).

**Delay-domain implementation.** Speed error is applied as a modulated read position of a delay line.
For a sinusoidal delay `τ(t) = A·sin(2π·f_m·t)`, the peak relative pitch deviation is `2π·f_m·A`
(calculated). Hence the delay amplitude needed for a given deviation:

| Component | f_m | Deviation (peak) | Delay amplitude A |
|---|---|---|---|
| Reel wow | 0.5 Hz | 0.1 % | 318 µs (15 samples at 48 kHz) |
| Capstan flutter | 8 Hz | 0.05 % | 10 µs (0.48 samples) |
| Broken cassette wow | 1 Hz | 1 % | 1.6 ms (76 samples) |
| Scrape flutter | 3 kHz | 0.01 % | 5 ns (0.00025 samples) |

(calculated) Two consequences:
- Scrape flutter cannot sensibly be done through a delay line at the base rate; it is equivalent to
  phase-modulation sidebands, so model it as low-level **modulation noise**: the signal multiplied by a
  narrow-band noise around 1-5 kHz at −50 to −70 dB (inferred). This also covers the "modulation noise"
  that tape shows in the presence of signal (§2.10).
- For wow and capstan flutter, a cubic Hermite read (as in `plugins/space/Source/dsp/Core.h`) is adequate;
  a 4th-order Lagrange or short windowed-sinc read is the HQ option, because Hermite droops about 1-3 dB
  near 20 kHz at fractional positions around 0.5 (inferred; measure in T7).

**Generator** (inferred design): speed `s(t) = 1 + Σ_i a_i·sin(φ_i(t)) + n(t)`, where each `φ_i` advances
at `f_i·(1 + small drift)` (drift from an Ornstein-Uhlenbeck process, time constant 2-10 s), the amplitudes
`a_i` are set so the IEC-weighted result equals the user's percent figure, and `n(t)` is low-passed noise
(OU, cut-off 5-20 Hz). Read position integrates `1 − s(t)` (so the delay changes by the integral of the
speed error, which is what a real transport does), with a constant offset so it never goes negative.
For stereo, studio and cassette transports move both channels together (identical speed signal); only
azimuth separates them. A "stereo wobble" option (decorrelated speed per channel) is not physical but
popular (§1).

**Comparison: Space's current tape** (working copy of `plugins/space/Source/dsp/Network.h`,
2026-10-19). The tape wow is a sine whose phase advances at `0.55 + 9·flutterMix` Hz; `flutterMix` is a
public member that nothing sets, so it stays at 0.3 and the "wow" runs at 3.25 Hz. Its amplitude is
`(0.0015 + 0.004·wear)·fs·modDepth` samples. The flutter is a second sine at 7.3 times the wow phase
(23.7 Hz) of `0.00012·fs·(1 + 3·wear)` samples, not scaled by `modDepth`. With the formula above, at the
default `modDepth = 0.25` and `wear = 0` (calculated):
- wow 0.77 % peak at 3.25 Hz;
- flutter 1.8 % peak at 23.7 Hz;
- plus the general read modulation (1 ms at 0.4 Hz, another 0.25 %).

That is "broken portable" territory even at zero wear, and the 24 Hz component is heard as a warble, not
a wow. There is also a bug: `sin(2π·7.3·wowPh)` with `wowPh` wrapping at 1 jumps by 0.3 of a cycle at every
wrap, a step in the delay time 3.25 times per second (a small click and pitch glitch on each repeat). §3
proposes the shared generator that replaces both.

### 2.10 Tape hiss and modulation noise

**Spectrum.** The noise of the tape itself (particle noise) is roughly white in flux at the reproduce
head. After the reproduce EQ (de-emphasis by the HF time constant, plus head losses), the audible hiss is
concentrated in 2-10 kHz and rolls off above the loss corner (inferred from the chain; this is why hiss
character differs between 120 µs and 70 µs cassette EQ: the 70 µs Type II playback cuts more treble, so it
is about 4 dB quieter for the same tape noise; commonly quoted figure, inferred).

**Levels** (re maximum output level, typical published specifications; inferred, verify against test
reports before quoting as documented):

| Machine | S/N, unweighted | Notes |
|---|---|---|
| Studio, 15 ips, modern high-output tape | 65-72 dB | A-weighted a few dB better |
| Studio, 7.5 ips | 60-66 dB | Hiss moves down an octave when speed halves (forum summary, 2026-10-19) |
| Cassette Type I, no NR | 50-56 dB | "Relatively high background hiss" (Cassette FAQ, stereo2go.com) |
| Cassette Type II, no NR | 54-60 dB | |
| Cassette + Dolby-B-like NR | +≈10 dB at HF | B-type "about 10 dB at 5 kHz"; C-type about 20 dB (patent US4473852, https://patents.google.com/patent/US4473852) |
| VHS linear audio | ≈ 42-45 dB | Wikipedia passage quoted on digitalfaq.com (citation flagged "needed"), Sencore table 45 dB |

**Modulation noise** (noise that rises and falls with the signal, from particle non-uniformity): heard as
a "fuzz" around sustained tones. Model as a band-limited noise multiplied by the signal envelope (or the
signal itself, for the scrape-flutter part, §2.9), 20-30 dB below the signal at most (inferred). Very
cheap and very effective at making a sine sound "on tape" (inferred; to verify by listening).

**Implementation.** White noise (deterministic xorshift, as in `plugins/space/Source/dsp/Core.h`) injected
*before* the reproduce loss filter and de-emphasis, so its spectrum follows the machine automatically. One
noise generator per channel, decorrelated, with level in dB relative to the machine's reference level.
Hiss must be gated by nothing (real hiss is constant), but the UI should offer an "only while playing"
option, because users ask for it (§1).

### 2.11 Companding noise reduction (generic, "Dolby-B-like")

What the classic consumer system does (documented in general terms by the patent literature found today):
a level-dependent HF boost on record that is undone by a complementary cut on playback. Low-level HF is
boosted up to about 10 dB (B-type, "about 10 dB at 5 kHz") or 20 dB (C-type); loud signals pass
unchanged. The B-type is a "sliding band" design (the shelf frequency moves up with level so loud HF is not
boosted into saturation). The patents disagree on the lower edge of action (~200 Hz to ~1 kHz).

Why it matters for a lo-fi plugin (inferred):
- Without the decoder (tape recorded with NR, played without), the result is bright and hissy-less but
  "spitty". With decoder but a playback level or azimuth error, the decoder **mistracks**: transients
  breathe and the top end pumps. That mistracking is part of the "cassette sound" people remember, and a
  "NR mistrack" control (level offset between encoder and decoder of 0 to ±6 dB) is cheap to build once the
  generic compander exists.
- Trademark: the plugin must not say "Dolby". Use "NR: off / B-type-like / C-type-like" in the docs and
  "NR" with "mild / strong" in the UI.

Design: a single-band sliding shelf (first-order high-shelf whose corner moves from about 1.5 kHz to
8 kHz as the HF level rises; side-chain on the HF band, 1-5 ms attack, 50-100 ms release), encoder and
decoder sharing the same code with inverse gains (inferred; this is a generic compander, no proprietary
curves).

### 2.12 Cassette saturation behaviour

- MOL (maximum output level) is defined at 3 % third-harmonic distortion at 315 Hz (or 400 Hz); SOL
  (saturation output level) is the treble ceiling, usually at 10 kHz, where tape saturates far below the
  MOL (Wikipedia, "Compact Cassette tape types and formulations",
  https://en.wikipedia.org/wiki/Compact_Cassette_tape_types_and_formulations; stereo2go Cassette FAQ;
  both accessed 2026-10-19). References are 200 nWb/m ("Dolby level") or 250 nWb/m.
- In practice: on Type I tape the 10 kHz SOL sits roughly 10-15 dB below the 315 Hz MOL (inferred, from
  published test reports; values not retrieved today). Combined with the 120 µs pre-emphasis, a cassette
  recorded at 0 dB shows a frequency response that collapses above 5-8 kHz, while at −20 dB it can be flat
  to 14-16 kHz. That **level-dependent top end** is the defining cassette behaviour and comes out of the
  model naturally if pre-emphasis, JA and self-erasure (a level-dependent HF loss, see below) are in the
  right order.
- Self-erasure: the bias field partially erases short wavelengths as they leave the record head; heavier
  with more bias and louder HF. Model as an HF loss whose depth grows with the recorded HF level (a dynamic
  low-shelf at the record side, 0-6 dB above 8 kHz; inferred).
- Type I vs II vs IV: Type II has lower hiss and higher HF headroom; Type IV (metal) much higher MOL and
  SOL (same Wikipedia page). As a control: a "formulation" switch (ferric / chrome / metal) that sets
  `M_s`, the EQ (120 µs vs 70 µs), SOL and hiss together.

### 2.13 Vinyl

- **RIAA curve** (cutting = inverse of playback): time constants 3180 µs, 318 µs and 75 µs (turnovers
  50 Hz, 500 Hz, 2122 Hz; RIAA 1954, the standard values). The 7950 µs (20 Hz) "IEC amendment" rolloff is
  optional. In the plugin, RIAA matters only where noise and distortion enter: crackle and surface noise
  enter *after* the cut and before playback EQ, so they are de-emphasised (a click is softened above 2 kHz
  and its LF tail lifted). The music path itself cancels (inferred).
- **Groove speed falls toward the centre.** 12" LP at 33⅓ rpm, outer groove radius ≈ 146 mm, inner limit
  ≈ 60 mm: groove speed 0.51 m/s outside, 0.21 m/s inside (calculated, v = 2π·r·rpm/60). At the inner
  groove a 15 kHz wavelength is 14 µm, close to the scanning radius of a conical stylus (≈ 15-18 µm) or the
  minor radius of an elliptical one (≈ 5-8 µm) (stylus radii inferred from common specifications). Result:
  **inner-groove HF loss** (several dB above 10 kHz) and **tracing distortion** (mostly second harmonic,
  rising with frequency and level and with smaller radius).
- **Model of inner-groove behaviour** (inferred): a "groove position" parameter (0 outside → 1 inside)
  that scales an HF loss (−0 to −6 dB at 15 kHz) and a level-dependent even-order distortion (an
  asymmetric soft curve applied to the HF band: tracing distortion is caused by the stylus tip being
  round, not by tape-like saturation). Option: let the position advance with the host timeline
  (a 20-minute side goes outside → inside), deterministic.
- **Wow from eccentricity**: an off-centre hole of eccentricity `e` gives a sinusoidal speed error at
  rotation frequency (0.555 Hz at 33⅓, 0.75 Hz at 45, 1.3 Hz at 78) of relative amplitude `e/r`
  (calculated). For e = 0.2 mm: 0.14 % peak at the outer groove, 0.33 % at the inner (calculated). The
  0.55 Hz figure is confirmed by the hifihub.ai article found today (secondhand, attributes Stereophile).
  So eccentricity wow grows toward the centre, and it is a pure line at the rotation rate: the model is a
  sine whose amplitude follows the groove position.
- **Warps**: vertical undulation at 1-3x rotation (0.5-5 Hz) with large amplitude; produce wow (through
  stylus/arm geometry) and subsonic vertical motion, which after the stereo matrix is mostly in L−R. Model:
  subsonic signal in the side channel at −30 to −50 dB plus correlated wow (inferred).
- **Rumble**: turntable motor and bearing, 5-30 Hz broadband plus lines; DIN 45539 weighted figures
  around −60 to −72 dB for good decks (one lab report: −71.8 dB silent groove, DIN-B weighted, HiFi News
  Holbo report, https://www.hifinews.com/content/holbo-airbearing-turntable-lab-report, accessed
  2026-10-19; the typical range is inferred). Model: low-passed noise plus a line at the motor rate,
  −50 to −70 dB.
- **Surface noise**: steady broadband noise (vinyl S/N, roughly 55-70 dB A-weighted for new pressings and
  40-50 dB for worn ones; inferred), with a slight emphasis in the side channel because vertical groove
  noise maps to L−R (inferred).
- **Clicks and crackle** (statistics):
  - duration classes: crackle < 0.3 ms, clicks 0.3-1 ms, pops ≥ 2 ms (patent summary found today,
    https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/8892226);
  - restoration literature models impulsive noise as a switched process: a Poisson/Bernoulli occurrence
    process with random amplitude and an autoregressive "shape" (Godsill and Rayner, *Digital Audio
    Restoration*, Springer 1998, https://sigproc.eng.cam.ac.uk/Main/SJGSpringer; the exact model equations
    were not retrieved today);
  - recommended generator (inferred): three independent Poisson processes: crackle 50-2000 events/s at
    −50 to −30 dBFS, clicks 0.2-20 /s at −30 to −10 dBFS, pops 0-0.5 /s at −20 to −3 dBFS; amplitudes from
    a heavy-tailed distribution (log-normal or Pareto with exponent 1.5-3); each event a short decaying
    shape (a damped oscillation filtered by the RIAA playback curve); stereo correlation 0.3-0.7 (a dust
    particle touches both walls, a scratch one more than the other); pops locked to the rotation period
    (a scratch repeats every 1.8 s at 33⅓): "scratch count" is the parameter, and each scratch has a fixed
    angular position. This last point is what makes vinyl crackle sound real and almost no generator does
    it (inferred from §1).

### 2.14 AM radio

Chain to model (inferred from the receiver literature found today and standard radio practice):

1. **Station processing**: heavy multiband compression and limiting with pre-emphasis; the audio is loud
   and dense even before the receiver.
2. **Modulation**: carrier `(1 + m·x(t))·cos(ω_c t)` with modulation index up to 100 %; overmodulation
   (`m·x < −1`) cuts the carrier and causes harsh distortion.
3. **Channel**: skywave **fading** (slow amplitude variation and selective fading, 0.1-10 s time scale;
   inferred), atmospheric **static** (impulsive, Poisson bursts, heavy-tailed), **adjacent-channel
   whistle** at the channel spacing (10 kHz in the Americas, 9 kHz in ITU Regions 1 and 3), and "monkey
   chatter" (adjacent-channel audio with spectrum inverted around 10 kHz) in wide receivers (patent text
   summarised from US5303413 and relatives, found 2026-10-19).
4. **Receiver IF filter**: receivers typically cut audio above about 3 kHz to avoid the whistle; good
   wide sets reached 6-12 kHz close to the transmitter; "narrow" settings limit to about 5 kHz (same patent
   literature). So: **2.5-5 kHz** for "old radio", up to 7-8 kHz for a hi-fi tuner.
5. **Envelope detector**: diode + RC. Too long an RC time constant causes "diagonal clipping" (the
   capacitor cannot follow a fast falling envelope at high modulation and high frequency); the diode knee
   adds low-level distortion. Model with an actual envelope follower on a synthetic AM signal at a
   low internal carrier (the plugin can modulate a 20-40 kHz carrier at the oversampled rate and detect
   it, so overmodulation, diagonal clipping and selective fading come out naturally; inferred).
6. **Audio stage and speaker**: small speaker, 150 Hz-4 kHz, cabinet resonance around 200-400 Hz
   (inferred).

Heterodyne whistle in the plugin: a sine at 9 or 10 kHz is above the IF filter of a narrow receiver, so
the *audible* whistle comes from close carriers (a few hundred Hz offset between co-channel stations,
inferred) or from tuning: model as a slowly gliding sine 200 Hz-4 kHz at −30 to −50 dB, plus the 9/10 kHz
line for the "wide" receiver. A "tuning" knob that detunes the station (adds whistle, side-band asymmetry
and distortion as you move off) is an engaging control (inferred).

### 2.15 Telephone

- **Band**: 300-3400 Hz (ITU-T G.712 channel; G.711 at 8 kHz sampling). Standard values, not re-verified
  today.
- **Codec**: G.711 μ-law (μ = 255) or A-law, 8 bits: about 38 dB SQNR over a wide range of levels, with
  the noise riding on the signal (calculated from the companding law; inferred). Cheap to implement
  exactly (the μ-law encode/decode is a closed formula).
- **Mobile/VoIP flavour**: low-bit-rate CELP-family codecs smear transients and give "watery" artefacts;
  packet loss concealment repeats or fades frames (20 ms frames). A generic approximation (inferred): a
  20 ms frame-based spectral smoothing (coarse LPC envelope + noisy excitation blend) plus random frame
  repeats/mutes (Gilbert-Elliott two-state loss model, loss 0-15 %). This is the most "invented" part of
  the plugin and can come in a later version.
- **Handset**: carbon-mic style emphasis around 1-2 kHz (+6 dB), plus soft clipping (inferred), and line
  hum at 50/60 Hz option.

### 2.16 Bit depth and sample-rate reduction

- **Bit crush**: quantise to N bits. SQNR of an ideal N-bit quantiser on a full-scale sine is
  6.02·N + 1.76 dB (standard). Options that change the character a lot: rounding vs truncation (the latter
  adds a DC offset and odd behaviour near zero), dither on/off (TPDF dither turns distortion into hiss),
  and "mu-law 8-bit" as a different curve.
- **Sample-rate reduction** (sample-and-hold at rate R): a zero-order hold multiplies the spectrum by a
  sinc with nulls at multiples of R and images every R; without an anti-alias filter, input content above
  R/2 folds back. Classic samplers had an anti-alias filter (sometimes a low-order one, so partial
  aliasing) and a reconstruction filter. The plugin should offer both: "clean" (steep low-pass at 0.45·R
  before the hold and after it) and "raw" (no filters; the aliasing is the point).
- **Fractional hold rates**: a hold rate that is not an integer divisor of the host rate makes the hold
  edges jitter by up to one host sample, which adds its own noise. Using band-limited steps at the hold
  edges removes it (the repo already has `shared/dsp/Blep.h`, PolyBLEP of 2 samples). This is what
  separates a smooth "8 kHz sampler" sweep from a gritty one, and few products document it (inferred).
- **Ranges**: bits 1-16 (continuous, with fractional bits via a variable step), rate 500 Hz to the host
  rate; "jitter" 0-100 % of the hold period.

### 2.17 Tape stop, spin-up and speed variation

**Physics** (inferred, standard rotational dynamics): when the motor drive stops, the speed obeys
`J·dω/dt = −b·ω − τ_c·sign(ω)` (inertia J, viscous loss b, dry friction τ_c). Viscous loss alone gives an
exponential decay that never reaches zero; dry friction alone a linear ramp to zero. Real stops look
like a blend: they slow gently, then stop abruptly. Normalised speed `s(t)` from 1 to 0 over time T with
a shape parameter `κ ∈ [0, 1]` (0 = linear, 1 = exponential-like) is the user control.

**Typical stop times** (inferred, from listening references; no measurements found today): turntable
platter with motor off 1-3 s (heavy platters longer), turntable with a hand brake 0.2-0.5 s, open reel
0.5-2 s depending on reel size, cassette 0.1-0.3 s.

**Implementation**: read from a ring buffer at variable rate. Read position `p(t) = ∫ s(t) dt` while the
write runs at 1. At the end of a stop of duration T, the read pointer lags by `∫ (1 − s) dt`: T/2 for a
linear ramp, more for an exponential one. A 4 s maximum stop needs a buffer of about 4 s (plus the
spin-up catch-up below).
- When `s < 1` the pitch goes down; no aliasing issue except that the interpolator must be good
  (Hermite is fine).
- **Spin-up** (restart): the read pointer is behind by L samples. Options: (a) jump back to "now" with a
  crossfade at the start of spin-up (the classic, used by most tape stop plugins; inferred); (b) spin up
  and stay delayed (adds latency that is not reported, a problem); (c) overshoot speed >1 until caught up
  (sounds like a fast-forward chirp; needs an anti-alias low-pass at 1/s because pitch goes up). Default
  (a), offer (c) as "catch up".
- **Triggers**: a button (momentary), a MIDI note (note on = stop, note off = start), host transport
  follow (when the host stops, play the stop curve: only works in hosts that keep calling `process`
  after stop; inferred), and a tempo-synced length (1/16 to 4 bars).
- **Speed variation** (static detune): "speed" ±1 octave via the same variable-rate read with a constant
  `s`, plus a matching change of the machine's speed-dependent EQ (bump frequency, losses, hiss pitch).
  This is what physically half-speed tape does and it is the most requested "lo-fi" gesture after wow
  (§1). The buffer grows with time at `s < 1`, so a static slow speed must be a "varispeed with periodic
  resync" or a pitch-shifter; the honest real-time version is: speed change = pitch change only for the
  duration of the buffer, then a crossfaded jump (inferred; decision in §8).

### 2.18 VHS linear audio

Documented (with the caveats of the sources found today):
- SP frequency response roughly 100 Hz-10 kHz (NTSC), PAL somewhat worse; EP/SLP down to about 4-5 kHz
  (Wikipedia passage quoted at https://www.digitalfaq.com/forum/video-capture/5874-vhs-frequency-response.html,
  accessed 2026-10-19; flagged "citation needed" there).
- S/N about 42-45 dB; W&F about 0.2 % (Sencore tech tip TT190, "conventional VCR" column).
- Tape speed SP 33.35 mm/s (NTSC) and 23.39 mm/s (PAL) (standard VHS figures, not re-verified today):
  slower than cassette, with a narrow 1 mm linear track at the tape edge (where edge damage and crease
  dropouts are common; inferred).

Inferred "broken VHS" ingredients: narrow band (150 Hz-6 kHz in SP, 4 kHz in EP), strong hiss, capstan
servo hunting (wow at 1-5 Hz, 0.3-1 %), edge damage (frequent short dropouts), crinkle (long deep
dropouts, a few per minute), hum and buzz at the field rate (50/59.94 Hz and harmonics) from the head
drum leaking into the audio head, and for hi-fi decks the switch to the linear track on tracking errors
(sudden bandwidth collapse for 0.2-2 s). A dedicated "VHS" machine mode can be implemented as a cassette
profile with different speed and EQ plus those extras; it does not need its own model.

### 2.19 Dropouts

**Cause** (documented in the video literature found today): dust, base damage or a lump of oxide lifts
the tape off the head for a moment. Video standards define a dropout as a reduction of 16 dB or more for
5 µs or longer (SMPTE RP 47 and RP 121, https://pub.smpte.org/pub/rp47/rp0047-1999_stable2004.pdf). No
audio standard with a duration threshold was found today.

**Model** (inferred): because the mechanism is a momentary increase of head-to-tape spacing, the right
model is **a transient spacing term `d(t)` fed into the spacing-loss filter of §2.6**: high frequencies
drop first and most (54.6·d/λ dB), lows survive. A 2 µm lift on cassette is −23 dB at 10 kHz but only
−2.3 dB at 1 kHz (calculated). That is how real dropouts sound (dull and hollow, not silent). A broadband amplitude gate,
like the one in Space's network (multiply by 0.35 for 10-30 ms, abrupt), does not do it; Chow's "Chew"
approximates it with an expander plus a low-pass that sweeps down with depth, and Wavesfactory lists
"random HF loss" (§1). The spacing formula gives the same behaviour from the physics, with one parameter
(inferred).
- Occurrence: Poisson, rate 0-5 per second scaled by "wear"; tapes also have *clusters* (a damaged
  section), so use a two-state (good/damaged) Markov model whose damaged state multiplies the rate by
  10-30 for 0.5-5 s.
- Shape: rise 1-5 ms, hold 5-200 ms (log-normal duration, median 20-40 ms), fall 5-30 ms; depth
  `d_max` 0.5-5 µm (log-normal).
- Interaction: during a dropout the level falls, so the hysteresis stage sees nothing different (the
  dropout is on playback), but the hiss level does **not** drop as much as the signal (the hiss is partly
  amplifier noise): keep 30-50 % of the hiss through the spacing filter (inferred).

### 2.20 Recommended parameter ranges (proposal)

| Control | Range | Default | Notes |
|---|---|---|---|
| Machine | studio tape, cassette, vinyl, AM radio / telephone (VHS as a cassette variant or own mode, §8) | studio tape | Sets the profile below |
| Speed (studio) | 30 / 15 / 7.5 ips (continuous 3.75-30 with scaled curves) | 15 ips | Moves bump, losses, hiss, EQ |
| EQ standard (studio) | IEC1 (CCIR) / NAB | IEC1 | |
| Formulation (cassette) | ferric / chrome / metal | ferric | M_s, SOL, EQ 120/70 µs, hiss |
| Input / drive | −24 to +24 dB | 0 dB | Into H; 0 dB = reference level at −18 dBFS RMS (inferred) |
| Saturation | 0-100 % (maps M_s over ~1 decade) | 50 % | |
| Bias | −100 % (under) … 0 (nominal) … +100 % (over) | 0 | c/k and self-erasure |
| Wow | 0-3 % weighted-equivalent (log taper) | machine default | Display in % on the IEC scale |
| Flutter | 0-1 % (log taper) | machine default | Includes capstan and roller lines |
| Scrape / modulation noise | 0-100 % (−80 to −40 dB) | 20 % | |
| Wear | 0-100 % | 0 | Spacing loss (0-3 µm), dropout rate, crease clusters, crackle density, hiss +0-6 dB |
| Dropouts | 0-100 % (0-5 /s) | 0 | Linked to wear, can be unlinked |
| Hiss / noise | off, −90 to −30 dB re ref. | machine | |
| Crackle (vinyl) / static (radio) | 0-100 % | 0 | Three Poisson classes |
| Bandwidth | 1-20 kHz low-pass, 20-500 Hz high-pass | machine | Applies after the machine |
| Azimuth | 0-100 % (θ per machine) | 0 | HF loss + inter-channel delay |
| Print-through | off, −70 to −40 dB | off | Post-echo only in real time |
| NR | off / mild / strong; mistrack 0 to ±6 dB | off | Generic compander |
| Bits | 1-24 (continuous) | 24 (off) | Dither on/off |
| Rate | 500 Hz-host | host (off) | AA on/off, jitter |
| Speed variation | −12 to +12 semitones | 0 | With machine curves following |
| Tape stop | time 0.05-4 s or 1/16-4 bars, curve 0-1, start mode | 0.6 s, 0.5 | Button, MIDI, host transport |
| Mix | 0-100 % | 100 % | Latency-compensated dry |
| Output | −24 to +12 dB, auto-gain on/off | auto | Loudness-matched A/B, §1 |
| Oversampling | 2x / 4x / 8x (render) | 4x | Constant reported latency, §8 |

---

## 3. Architecture, and sharing the tape model with Space

### 3.1 What Space's tape character does today

`plugins/space/Source/dsp/Network.h` (working copy, 2026-10-19) runs the tape character inside each of the
8 feedback lines. Per line and per sample:

| Element | Current code | Assessment (inferred) |
|---|---|---|
| Saturation | `tanh(1.2·v + 0.05)/1.2 − tanh(0.05)/1.2`, at the base rate | Memoryless; slightly asymmetric (even harmonics), unit slope at the origin. Not oversampled, but in a damped feedback loop the aliasing is partly filtered on each pass. Acceptable for a delay. |
| "Head bump" | `bump += (v − bump)·k₁₁₀Hz; v += 0.18·bump` | A low shelf of +1.4 dB below ~110 Hz, not a resonance. In a loop it compounds: +1.4 dB per repeat until the loop low-cut stops it. |
| HF loss | `dampHz = min(dampHz, 7500 − 4500·wear)` one-pole | Plausible "worn cassette" corner; not speed-dependent. |
| Wow / flutter | Two pure sines, 3.25 Hz and 23.7 Hz (§2.9) | Too deep at zero wear; periodic; phase step at every wrap (bug). |
| Dropouts | Per-sample probability `2e-5·wear`; when hit, gain ×0.35 for 10-30 ms | (1) The rate is per *sample*, so at 96 kHz there are twice as many dropouts per second as at 48 kHz (sample-rate dependence). (2) The gain switches abruptly (a click at both edges). (3) Broadband: real dropouts are HF-first (§2.19). |
| Noise | `wear·2e-4·rng.bi()` per pass | White and unshaped; −74 dBFS-ish uniform per pass. |

None of this is wrong for a creative delay, but each item has a direct replacement in the tape model of
this plugin, and sharing the code gives Space a better tape for free.

### 3.2 A shared, JUCE-free tape library

Proposal (inferred): a header-only set under `shared/dsp/tape/`, in the house style (per-sample
`process`, `prepare(fs, seed)`, POD parameter structs, deterministic xorshift noise, no allocation after
`prepare`, no JUCE):

| Header | Contents | Used by Tape | Used by Space |
|---|---|---|---|
| `Transport.h` | Speed generator: transport lines (capstan, roller, reels with pack radius), OU drift and noise, IEC-calibrated amplitude, tape stop / spin-up curve, varispeed. Output: speed `s(t)` and, integrated, a read offset in samples. | Full | Wow/flutter per line (8 instances with different seeds, or 1 shared transport + small per-line decorrelation) |
| `Hysteresis.h` | JA in normalised units; solvers RK2 / RK4 / NR4; Langevin with series near zero; denominator clamps. | Full, inside the oversampler | Optional "HQ tape" mode, RK2, 1x or 2x |
| `Losses.h` | Gap, spacing, thickness, azimuth magnitudes → fitted minimum-phase biquads; head bump biquads by speed; spacing modulation input for dropouts. | Full | Bump + loss as a cheap 2-biquad set per line |
| `Emphasis.h` | Record/reproduce shelves for IEC1/NAB/AES/cassette 120 & 70 µs, RIAA. | Full | Not needed (no point of entry) |
| `Noise.h` | Hiss (white → shaped by the reproduce chain), modulation noise, crackle (3 Poisson classes + scratches locked to rotation), static. | Full | Hiss and modulation noise |
| `Dropouts.h` | Two-state Markov occurrence process with rates in events per **second**; spacing envelope out. | Full | Replaces the per-sample gate |
| `Compander.h` | Generic sliding-shelf NR encoder/decoder. | Full | No |
| `Machine.h` | Machine profiles (§3.4) as data: speed, EQ, bump, gap, spacing, hiss, W&F lines, M_s/k/c. | Full | "Tape character" picks the cassette or studio profile |

Two profiles of the same code:
- **Full** (this plugin): everything above, JA at 4x.
- **Light** (Space, per feedback line, 8 instances): `Transport` with 2-3 lines + OU noise, losses as two
  biquads, `Dropouts` driving a one-pole spacing filter instead of the full loss fit, saturation either the
  current tanh or JA-RK2 at 1x (decision §8). Budget: the light profile must cost less than 0.3 % of an M4
  core per line at 48 kHz (inferred target, to be measured).

Migration for Space (inferred, in order of value for the effort):
1. Replace the wow/flutter sines with `Transport` (fixes the phase-wrap bug and the excessive depth; keeps
   the `modDepth`/`wear` meaning by mapping them to the % scale).
2. Replace the dropout gate with `Dropouts` + a spacing low-pass (fixes the sample-rate dependence and the
   clicks).
3. Replace the shelf "bump" with a real bump biquad at the speed-scaled frequency.
4. Optionally offer JA as a "hysteresis" tape in Space when the CPU numbers allow.

Namespaces: Space uses `dh::sp` with its own `Core.h` (`Rand`, `OnePole`, `DelayLine`); shared code lives in
`dh` (`shared/dsp/Common.h` has `fastTanh`, `isFiniteF`, `fastExp`). The shared tape library should depend
only on `shared/dsp/Common.h` and bring its own minimal `Rand` (bit-identical to Space's xorshift32 so the
existing Space tests keep passing); Space's `Core.h` can later include it instead of duplicating.

### 3.3 Reusing the oversamplers

- Lift `HalfbandD` and `Os4` out of `plugins/lacquer/Source/dsp/Saturation.h` into
  `shared/dsp/Oversampling.h` (or a sibling `OversamplingPs.h`, "per sample") as `OsN<2|4|8>` in double,
  with the same Kaiser designs (95 taps β 9 for the stage at the base rate, 31 taps β 7 for the others),
  the zero-tap skipping and the integer-latency construction. Lacquer then includes it; its existing
  bit-identical determinism test (T12 in Lacquer's §6.10) proves the move changed nothing.
- Keep the block-based float `Interpolator`/`Decimator` for the synths.
- The tape plugin runs one `OsN` per channel around: record EQ → hysteresis → self-erasure. Everything
  else (losses, transport, noise, crush) runs at the base rate, except the AM radio carrier simulation,
  which needs the oversampled domain if implemented with a real carrier (§2.14).
- Latency: with 4x it is 54 samples (Lacquer's figure); 2x and 8x have their own integer latencies. To
  report a single constant latency regardless of the oversampling choice, pad the lower factors with a
  plain delay up to the 8x latency, or accept a latency change (re-reported to the host) when the user
  switches factor. Lacquer chose "constant"; the same choice here costs a few samples (decision §8).

### 3.4 Signal flow and machine profiles

Per channel (proposal, inferred):

```
in → input gain → [machine]
   studio / cassette / VHS:
      record EQ → ⟦4x: JA hysteresis (drive, M_s, bias) → self-erasure⟧ → NR encode
      → transport (wow, flutter, stop, varispeed; print-through taps)
      → reproduce losses (gap, spacing + dropouts, thickness, azimuth) + head bump
      → hiss + modulation noise (injected before reproduce EQ) → reproduce EQ → NR decode
   vinyl:
      cutting EQ (inverse RIAA) → groove distortion (position-dependent) → eccentricity/warp wow
      → crackle, pops (scratches locked to rotation), surface noise, rumble → RIAA playback
      → inner-groove HF loss
   AM radio / telephone:
      station compressor → band limit → (AM: modulate, fading, static, whistle, IF filter,
      envelope detector) | (phone: 300-3400 Hz, μ-law, frame loss) → small speaker
→ bandwidth (HP/LP) → bit / rate crush → output gain (auto-gain) → mix with delayed dry → out
```

Machine profile defaults (proposal; every value is a starting point to tune by listening and by T7):

| Parameter | Studio 15 ips | Studio 7.5 ips | Cassette (ferric) | VHS linear SP | Vinyl LP | AM radio | Telephone |
|---|---|---|---|---|---|---|---|
| EQ | IEC1 35 µs | IEC1 70 µs | 120 µs / 3180 µs | cassette-like 120 µs | RIAA | pre-emph. off | none |
| Bump | 55 Hz +2 dB | 30 Hz +2.5 dB | 50 Hz +1 dB | 60 Hz +1 dB | none | none | none |
| HF −3 dB (−20 dB level) | 20 kHz | 14 kHz | 13 kHz | 9 kHz | 18 kHz (outer) | 4 kHz | 3.4 kHz |
| LF −3 dB | 25 Hz | 25 Hz | 35 Hz | 100 Hz | 20 Hz | 150 Hz | 300 Hz |
| Wow / flutter (weighted) | 0.03 % | 0.06 % | 0.12 % | 0.25 % | 0.08 % + eccentricity | 0 | 0 |
| Hiss (re ref.) | −68 dB | −63 dB | −52 dB | −44 dB | surface −60 dB | static | line −50 dB |
| Saturation | JA, M_s high | JA | JA, low SOL, self-erasure | JA low | groove distortion | detector | μ-law + soft clip |

### 3.5 Determinism and timeline-locked randomness

House rule from Lacquer: the output does not depend on the block size, and a render reproduces bit for bit.
For a lo-fi plugin this needs care (inferred):
- All random processes (wow drift, dropouts, crackle, static) draw from seeded xorshift generators advanced
  **per sample**, never per block.
- Events are scheduled in samples (Poisson by exponential inter-arrival times drawn per event), so the
  rates are in events per second at any sample rate (unlike Space's per-sample probability).
- **Timeline lock** (an option, default on in a DAW, inferred): make every random process a function of
  the absolute host sample position instead of a running generator. Use a counter-based hash (SplitMix64
  or similar) of (preset seed, sample index) for events (`p = rate / fs` per sample), and for the slow
  processes (wow drift, fading, warp) smooth value noise: hashed control points every 50-100 ms on the
  timeline, interpolated with a cubic. Then bar 17 gets the same dropouts and crackle whether playback
  starts at bar 1 or bar 16, and an offline bounce sounds exactly like the real-time playback. (No
  user complaint about this was collected today; the claim is inferred: a free-running random generator
  cannot make a bounce match what was heard.) Off = free-running
  (a different performance every time, like a real worn tape). Without a host position (standalone, or a
  host that reports none) fall back to the free-running generator.
- Reel geometry follows the timeline too: the supply reel's pack radius is a function of the song
  position (start of reel at bar 1), so the reel wow frequencies and the print-through spacing drift
  through the song deterministically, and the UI shows the same reel state for the same position.

---

## 4. Visual design

### 4.1 Constraints

- Distinct from the siblings: Lacquer is an enamel mastering lathe in light grey and amber; Murmur a dusk
  flock; Space a 3D room with light particles. So: no amber-on-grey, no particles, no dusk gradient.
- No brand marks: no real machine silhouettes that identify a manufacturer (no recognisable meter faces,
  logos, reel hub patterns of a specific brand, cassette shell designs of a specific maker). Generic
  geometry only.
- The picture must **show the model**, not decorate it (this is the Audio Tools habit: Lacquer's disc,
  AT20V's patch cables). Everything that moves on screen is driven by the same signals as the audio,
  latency-compensated.
- Cheap to draw: the JUCE editor repaints at 30-60 fps; rotating cached images and drawing a few paths is
  fine, per-pixel effects are not.

### 4.2 What should move, and how (common to every direction)

- **Reels whose radius follows the tape pack.** With hub radius `r_h`, tape thickness `h` and wound length
  `ℓ`, the pack radius is `r = sqrt(r_h² + ℓ·h/π)` and the angular speed `ω = v/r` (calculated). The supply
  reel speeds up as it empties, the take-up slows down. Studio defaults: hub 50 mm, full 130 mm,
  h ≈ 50 µm; cassette: hub 11 mm, full 25 mm, h ≈ 12-18 µm (C60-C90; inferred typical values). The reel
  angle integrates `ω`, so wow and tape stop are visible as the reels slowing; at a tape stop the reels
  decelerate on exactly the audio's speed curve.
- **Tape path over the heads**: supply reel → tension arm → guide → erase / record / reproduce heads →
  capstan and pinch roller → guide → take-up. The tension arms move with the wow signal (scaled up so it
  is visible; a real arm moves by fractions of a millimetre).
- **Wear on the tape itself**: the tape is a scrolling texture moving at tape speed. Wear draws as
  longitudinal scratches and oxide-shed patches whose density follows "wear"; each scheduled dropout is a
  visible blemish that reaches the reproduce head exactly when it is heard (the UI reads the event queue
  shifted by the reported latency). This is the one image that explains dropouts without words.
- **Head glow / level**: the reproduce head gap brightens with level; above the saturation knee it tints
  (a warm red in direction A, see below), telling the user the tape is saturating.
- **Hysteresis scope** (unique to this product): a small B-H plot of M against H, traced live from the
  hysteresis state (decimated to a few hundred points per frame). Under-bias opens the loop, drive
  stretches it into the saturation corners. Engineers will love it and it teaches what the knobs do.
- **Meters**: a needle meter (VU ballistics: 300 ms integration, IEC 60268-17) for the record level
  against the tape's reference, and a small "W&F %" readout that displays the *measured* weighted value of
  the generated speed signal (§5, T4), not the knob value.
- **Machine changes morph the view**: cassette = a generic shell window over two small hubs and packs
  (and the tape path through the shell opening); vinyl = top view of a platter with a tonearm that moves
  inward with groove position, dust specks on the surface that are the actual scheduled pops (fixed
  angular positions, §2.13), and a visible warp as a slight wobble of the arm; AM radio = a tuning dial
  with a drifting needle and a signal-strength meter that follows the fading; telephone = a coiled cord
  and a line-level lamp. Keep the morph quick (300 ms) and the controls in the same places.

### 4.3 Three directions

**A. "Oxide and cream", a top-down transport on a workbench (recommended).** Deep oxide brown
(around #2B1E17) deck, cream panel lettering (#EFE6D2), tape in a dark warm brown-black with a faint sheen,
brushed-steel heads and capstan, and one accent colour: verdigris teal (#3E8C84) for indicators and
selected controls, with signal red only for overload. Reels drawn as rotated cached images (flanges with
generic three-window cut-outs) over a separately drawn pack disc with fine ring texture. Lighting from the
top-left, soft shadows under the reels. The hysteresis scope sits in a small dark "service window" in the
corner. Controls below the transport in two rows (machine/formulation/speed; then saturation, wow,
wear, noise, bandwidth, crush, stop). Clearly different from Lacquer's light grey/amber and from Space's
dark 3D room. Cost: 2D only, a dozen cached images; a Blender pre-render of the static deck and heads
would lift it to the photoreal level of AT20V without per-frame 3D.

**B. "Service manual", a technical line drawing.** The whole transport as a clean schematic in pale
lines on a dark slate blue (#1D2733), like a service-manual diagram: tape path drawn as a line with
moving tick marks, reels as concentric circles whose pack radius changes, heads as labelled blocks.
Overlaid measurement panels: frequency response at the current settings (the machine profile curve, live),
W&F spectrum, hysteresis loop, noise spectrum. Very honest and very cheap; fits the "a model anyone can
study" pitch of Audio Tools; weaker emotional pull for producers who buy "Dusty Cassette" presets.

**C. "Night session", a photoreal 3D render.** A dark room with a reel machine lit by a single warm
practical lamp, rendered in Blender (the AT20V / Lacquer pipeline: `plugins/lacquer/tools/blender/lathe.py`
shows the approach), reels and tension arms as separate layers composited at run time, tape blemishes
drawn over the tape layer. The most striking, but close in mood to Space's dark 3D room, and heavier to
maintain (every machine needs its own renders). Could be the "film" for the website rather than the UI.

Recommendation (inferred): **A for the plugin, with B's measurement panels available as an "inspect"
overlay** (one toggle that turns the deck into the schematic with live curves), and C only as the hero
film for the website, rendered from the same geometry.

### 4.4 Interaction ideas

- Drag a reel to scrub (only in the standalone/file mode; in a DAW the timeline owns the position).
- Click the pinch roller to trigger a tape stop; hold to keep it stopped; release to spin up.
- Wear has a small "age" slider drawn as a strip of tape getting dirtier from left to right.
- The level meter has a "reference level" marker that the user can drag (sets the drive relative to the
  tape's reference; the most misunderstood setting in tape plugins, §1).
- Hovering any control shows its physical meaning ("spacing 1.2 µm, −11 dB at 10 kHz"), computed from the
  model.

---

## 5. Tests that prove it works

Automated in `plugins/tape/tools/tapetest.cpp` (JUCE-free, like `mastertest.cpp` and `spacetest.cpp`),
plus `hosttest` for the plugin wrapper. Signals are synthesised in the test (sines, sweeps, impulses,
pink noise, the demo mix of `plugins/lacquer/tools/demo_mix.py` for music), so the tests run without
external files. "Default preset" below means the init state of the studio machine.

| # | Test | Pass criterion |
|---|---|---|
| T1 | No NaN/Inf, bounded output | Silence (≥ 60 s, also checks the Langevin series and the `δ` dead band), DC ±1, full-scale square, white noise at +12 dBFS, single-sample impulses, 1 s files, sample rates 22.05-192 kHz, every machine and every preset, with all continuous parameters randomised every block (seeded): no NaN/Inf (checked by bit pattern, `isFiniteF`, because `-ffast-math` may remove `std::isfinite`), peak ≤ +24 dBFS, no denormals (CPU per block in silence within 10 % of the CPU with signal) |
| T2 | Exact latency | Impulse with every character control at zero (studio machine, wow/flutter/noise/dropouts off, drive low): output peak at exactly the reported latency, for every oversampling factor and every machine; cross-correlation of a music render with all character on but wow off: lag = reported latency ±0 samples; the latency value does not change when switching machine or (if decision §8 says constant) oversampling |
| T3 | Determinism across block sizes | Renders with blocks of 1, 64, 512, 4096 and random sizes (seeded) are bit-identical, with wow, dropouts, crackle, static, tape stop (MIDI-triggered at fixed sample positions) and parameter automation all active; two runs and a run after `reset()` are bit-identical; timeline-locked mode: starting playback at bar 1 or at bar 16 gives bit-identical output from bar 17 on (after the processing tail settles: compare from 2 s after the later start) |
| T4 | Wow/flutter measured vs set | Render a 3.15 kHz sine 60 s; instantaneous frequency by Hilbert transform (analytic signal phase derivative, 4x decimated); IEC 386-style weighting (from the standard's curve, §2.9) and quasi-peak / 2σ readings. Pass: measured weighted value within ±10 % of the set value from 0.02 % to 2 %, for every machine; spectral lines of the speed signal at the configured capstan/roller/reel frequencies ±2 %; reel lines drift as the timeline advances (supply reel frequency rises); stereo: speed signals of L and R identical unless "stereo wobble" is on. Also run the measurement on Space's tape character to document its real figures before and after the migration (§3.2) |
| T5 | Hysteresis loop sanity | Drive the JA core directly with a slow sine H (1 Hz at 48 kHz × oversampling, several amplitudes): after 2 cycles the loop repeats within 1e-6 (periodic steady state); odd symmetry M(−H) on the falling branch = −M(H) on the rising branch within 1e-4; coercivity > 0 and growing with k; remanence < M_s; \|M\| ≤ M_s always; loop area grows with drive and shrinks as c → 1; at −40 dB drive THD < 0.1 % (near-linear with nominal bias); with "under-bias", low-level crossover distortion measurable (THD rising at *low* level, the signature). Cross-check: RK2 / RK4 / NR4 against a reference RK4 run at 64x in double: loop shape error (RMS over the cycle) < 1 % of M_s at the default oversampling |
| T6 | Aliasing with oversampling | Sine sweep 20 Hz-20 kHz and fixed tones 5, 9, 15 kHz at drive +12 dB, plus a 19 + 20 kHz two-tone: non-harmonic (aliased) components below −80 dBFS at 4x and below −90 dBFS at 8x for the hysteresis stage (Lacquer's tanh reached −90 dBFS at 4x, README); report the 1x and 2x figures too, so the user-facing oversampling labels are honest. The crusher is excluded in "raw" mode (aliasing intended); in "clean" mode its aliases stay below −70 dBFS |
| T7 | Machine frequency response | Swept sine or log-chirp at −20 dB below reference (below saturation), nonlinearity bypassed and engaged: response matches the profile table (§3.4) within ±0.5 dB (bump frequency ±5 %, bump gain ±0.3 dB, HF −3 dB point ±5 %); record/reproduce EQ cancel within ±0.05 dB with the nonlinearity bypassed; cassette at 0 dB vs −20 dB: the 10 kHz response is at least 6 dB lower at 0 dB (level-dependent top end, §2.12); interpolator droop at 20 kHz measured and documented per quality mode |
| T8 | Noise levels and spectra | Hiss level within ±1 dB of the set value (A-weighted and unweighted, re reference level); third-octave spectrum within ±1.5 dB of the shape predicted by the reproduce chain; the 120 µs vs 70 µs cassette difference ≈ 4 dB (§2.10); μ-law path SQNR ≈ 38 dB on a −10 dBFS sine (±1 dB) |
| T9 | Event statistics | Over 10 min: dropout, crackle, click, pop and static counts within the 99 % Poisson interval of the set rates; dropout duration median within ±15 % of the set value; rates in events per second identical at 44.1, 48 and 96 kHz (the bug class found in Space, §3.1); dropout depth frequency-dependent (10 kHz loss ≥ 5x the 1 kHz loss in dB, §2.19); scratch pops repeat at the rotation period ±1 sample |
| T10 | Tape stop and speed | Stop time within ±1 ms of the set time (or of N beats at the host BPM); instantaneous frequency of a 1 kHz sine follows the configured speed curve within 1 %; no click at the start or end of a stop or spin-up (high-pass residual after removing the expected signal < −60 dBFS); after spin-up the output is re-aligned (latency back to the reported value) within the set catch-up time; varispeed: bump frequency and hiss spectrum scale with speed |
| T11 | Transparency at zero | Machine "studio", all character controls at zero, drive low, oversampling on: output equals input delayed by the reported latency, residual < −90 dB (except the half-band ripple, documented); mix at 0 %: residual < −120 dB (latency-compensated dry path) |
| T12 | Sample-rate independence | Same input at 44.1 / 48 / 96 / 192 kHz: frequency response within 0.3 dB (20 Hz-16 kHz), measured W&F within 5 %, hiss level within 0.5 dB, event rates per second equal (T9), tape stop time equal within 1 ms |
| T13 | Automation and switching | Step changes of every continuous parameter (0 → 100 % in one block): no click above −60 dBFS relative to steady output (spectral flux check); machine and formulation switches crossfade (≤ 50 ms) without a gap; loss-filter refits crossfade (§2.6) |
| T14 | State and presets | Save/restore round trip: identical parameter values and bit-identical render; every factory preset loads and renders without NaN, with output loudness within ±1.5 LU of the input when auto-gain is on |
| T15 | Shared model | The shared `shared/dsp/tape/` headers produce bit-identical output in Tape and in Space for the same parameters (one golden file per module); Space's existing `spacetest` still passes after the migration (with updated expectations only where the bugs of §3.1 were fixed, documented) |
| T16 | CPU budget | `taperender --bench` at 48 kHz, 512-sample blocks, stereo, on the Apple M4 and the AMD Ryzen 9 5900XT (the machines of Lacquer's README): default preset (JA RK4 at 4x) ≤ 3 % / ≤ 5 % of one core (Lacquer's whole chain measured 2.9 % / 4.9 %); worst case (8x, NR4, every module on, vinyl + crush + NR) ≤ 12 % / ≤ 18 %; Space light profile ≤ 0.3 % per line on the M4; record the numbers in the README with the date, like Lacquer |
| T17 | Visual sync | Logged UI events (dropout blemish at the head, pop on the platter, reel speed at a stop) within one frame (≤ 33 ms) of the audio event when the host reports the latency; manual check in two hosts |

Subjective check (not automated): blind A/B against the dry signal and, where licences allow, against
renders from two reference plugins of §1 at loudness-matched levels; 3-5 listeners; questions per preset
("which sounds more like a worn cassette?") rather than "which is better". Also a reference recording:
dub a test file to a real cassette deck and a real turntable (lacquer-cut or a pressed test record, if
available) and compare measured W&F, response and noise against the plugin's profile; this is the
"measured against the hardware" claim the Audio Tools README makes for the synths.

---

## 6. Name

The family's original products are single evocative English words for an object or material (Orrery,
Murmur, Lacquer). The product covers tape, vinyl and radio, so a name about **ageing** fits all machines
better than one about tape alone.

Collision search on 2026-10-19 (KVR, Plugin Boutique, general web; no trademark registry was searched,
and the search budget ran out before every candidate was checked):

| Candidate | Fit | Collision found | Verdict |
|---|---|---|---|
| **Tarnish** | Ageing of a surface; pairs naturally with Lacquer | Only as a preset name inside iZotope Trash 2 | Free (minor) |
| **Spool** | The tape object; short, friendly | None found (general and KVR search) | Free, but tape-only and a plain word |
| **Foxing** | Archival term for the age spots on old paper: wear made visible | None found | Free; obscure, needs a one-line explanation |
| **Lodestone** | A natural magnet: a nod to magnetic tape | None found | Free; does not cover vinyl/radio |
| **Weathered** | Describes wear, dropouts, hiss | None found | Free; adjective, weaker as a product name |
| **Selvage** | The woven edge of a fabric (tape edge damage) | None found | Free; loose link |
| **Umber** | Brown earth pigment (oxide colour) | Nearest: Orchestral Tools Umbra | Free (minor) |
| **Coercion** | Coercivity is a real tape property | None found | Free, but the everyday meaning is negative |
| Palimpsest, Vellum, Emulsion, Idler, Degauss, Mothball | Archive / transport terms | **Not checked** | Check before use |

Rejected because a tape, vinyl or wow/flutter product already uses the name (URLs from the search,
2026-10-19):
- Oxide (Universal Audio "Oxide Tape Recorder", https://www.kvraudio.com/product/uadx-oxide-tape-recorder-by-universal-audio/details; also Airwindows "Chrome Oxide");
- Iron Oxide (Airwindows); Ferric (Variety of Sound "FerricTDS", https://rekkerd.org/bootsy-releases-ferrictds/);
- Remanence (Stillwell Audio, https://www.kvraudio.com/product/remanence-by-stillwell-audio);
- Patina (Minta Foundry vinyl emulator, https://www.kvraudio.com/product/patina-by-minta-foundry; Orchestral Tools);
- Magnetite (Black Rooster Audio, https://blackroosteraudio.com/en/products/magnetite);
- Rust (Viator DSP lo-fi plugin, https://www.kvraudio.com/product/rust-by-viator-dsp);
- Capstan (Celemony wow/flutter removal);
- Hysteresis (Glitchmachines, https://www.kvraudio.com/product/hysteresis-by-glitchmachines);
- Reel / Reels (AudioThing "Reels", https://www.pluginboutique.com/product/2-Effects/96-Tape-Emulation/4778-Reels);
- Halide (XNULLX, 2026), Heirloom (Spitfire Audio), Attic (Soniccouture), Gauss (iOS tape looper),
  Bias (Positive Grid), Splice (the company);
- from memory, not re-checked: Ferrite (an iOS audio editor), Shellac and Erasure (bands).

**Recommendation** (inferred):
1. **Tarnish**: covers every machine (ageing, not tape), reads well next to Lacquer ("Lacquer" finishes a
   master, "Tarnish" ages it), short, and the only hit is a preset name in another product.
2. **Foxing**: the most distinctive and unclaimed; the archival meaning ("spots of age") matches the wear
   and dropout visuals. Risk: few people know the word.
3. **Spool**: the friendliest and most obvious, but it says "tape" and the product also does vinyl and
   radio.

Before deciding: a USPTO and EUIPO search in class 9 (software) for the top two, a direct KVR product
search, and a check of app stores (iOS/Android) for audio apps with the same name.

---

## 7. Factory presets (proposal: 32 in 6 categories)

Settings are starting points in the units of §2.20; "W&F" is the weighted figure on the plugin's scale.
Names are descriptive and brand-free.

**Studio tape (6)**
1. **Warm Studio Tape**: studio 15 ips IEC1, drive +4 dB, saturation 55 %, bias 0, W&F 0.03 %, hiss −70 dB, bump 55 Hz +2 dB. The everyday "on tape" glue.
2. **30 ips Master**: studio 30 ips, drive +2 dB, bias +20 % (clean), W&F 0.02 %, hiss −72 dB; bump at 100 Hz, open top. For the mix bus.
3. **7.5 ips Demo Reel**: studio 7.5 ips NAB, drive +6 dB, saturation 60 %, W&F 0.08 %, hiss −63 dB, HF −3 dB at 13 kHz.
4. **Hot Drum Bus**: studio 15 ips, drive +12 dB, saturation 75 %, bias −20 % (a little grit), auto-gain on, mix 70 %.
5. **Bounced Four Times**: studio 7.5 ips, drive +5 dB, wear 25 %, hiss −58 dB, azimuth 20 %, print-through −55 dB; generation loss in one click.
6. **Under-Biased Grit**: studio 15 ips, bias −80 %, drive +6 dB: open hysteresis loop, crossover fuzz on quiet notes.

**Cassette (8)**
7. **Dusty Cassette**: cassette ferric, drive +3 dB, W&F 0.25 %, wear 45 %, dropouts 30 %, hiss −50 dB, azimuth 25 %.
8. **Chrome Mixtape**: cassette chrome 70 µs, NR mild with 0 mistrack, drive +2 dB, W&F 0.1 %, hiss −58 dB. The good copy.
9. **Pocket Player Jog**: cassette ferric, W&F 0.6 % with strong reel wow and OU drift, wear 30 %, bandwidth LP 10 kHz.
10. **Fourth-Generation Dub**: cassette ferric, hiss −46 dB, azimuth 45 %, wear 50 %, NR strong with mistrack +4 dB (breathing top end).
11. **Answering Machine**: cassette at half speed (microcassette-like: LP 4.5 kHz, HP 300 Hz), W&F 0.8 %, hiss −42 dB, drive +8 dB.
12. **Chewed Tape**: cassette ferric, W&F 1.5 %, dropouts 70 % with damage clusters, wear 80 %, crinkle (long deep dropouts) on.
13. **Hot Ferric Saturation**: cassette ferric, drive +10 dB: the level-dependent top end of §2.12 as an effect, hiss −52 dB.
14. **Forgotten in the Car**: cassette ferric, wear 70 %, W&F 0.9 % with warped-shell wow at 0.4 Hz, print-through −45 dB, bandwidth 6 kHz.

**Video tape (3)**
15. **Broken VHS**: VHS linear SP, W&F 0.6 % with servo hunting at 2 Hz, dropouts 50 % (edge damage), hum 60 Hz −50 dB, hiss −44 dB, occasional tracking collapse.
16. **Rental Copy, EP Mode**: VHS EP, LP 4.5 kHz, hiss −40 dB, W&F 0.35 %, wear 40 %.
17. **Tracking Error**: VHS SP, frequent bandwidth collapses (0.2-2 s), field-rate buzz −40 dB, dropouts 60 %.

**Vinyl (6)**
18. **Worn Vinyl**: LP, crackle 40 %, clicks 2/s, two scratches, surface −52 dB, eccentricity 0.2 mm, groove position follows the timeline.
19. **Clean Pressing**: LP, surface −66 dB, crackle 5 %, rumble −68 dB, eccentricity 0.05 mm. Just the air of a record.
20. **Thrift Store 45**: 45 rpm, crackle 60 %, clicks 6/s, one deep scratch (pop every 1.33 s), inner-groove loss 50 %.
21. **Shellac 78**: 78 rpm profile, LP 5 kHz, HP 120 Hz, surface −38 dB (shellac roar), crackle 70 %, groove distortion 40 %, mono.
22. **Warped LP**: LP, warp 70 % (wow 0.55 Hz at 0.6 %, subsonic side-channel motion), crackle 20 %.
23. **Inner Groove Blues**: LP at groove position 1.0 (fixed): HF −6 dB at 15 kHz, tracing distortion 60 %, crackle 25 %.

**Radio and telephone (5)**
24. **Old Radio**: AM, receiver bandwidth 3.5 kHz, station compressor 60 %, static 25 %, fading 20 %, small speaker on.
25. **Distant Station**: AM, bandwidth 2.8 kHz, fading 70 % (selective), static 60 %, whistle 30 % with slow glide, tuning off-centre 20 %.
26. **Telephone**: phone, 300-3400 Hz, μ-law 8-bit, handset emphasis, line noise −50 dB.
27. **Mobile Call Breakup**: phone, codec smear 60 %, frame loss 8 % in bursts, AGC pumping.
28. **Intercom**: phone band 400-3000 Hz, μ-law, soft clip drive +10 dB, hum 50 Hz −45 dB.

**Lo-fi, crush and effects (4)**
29. **Tape Stop Drop**: studio 15 ips clean, tape stop 1 bar at host tempo, curve 0.6, triggered by MIDI note C1 or the button; spin-up with crossfade.
30. **Half-Speed Dream**: cassette chrome, speed −12 semitones with machine curves following, W&F 0.2 %, hiss −56 dB, mix 100 %.
31. **12-bit Sampler**: studio clean, bits 12, rate 26 kHz with clean AA, drive +3 dB: the late-80s sampler sheen.
32. **Toy Sampler**: bits 8, rate 8 kHz raw (no AA, aliasing on), jitter 20 %, cassette hiss −48 dB, W&F 0.3 %.

Init state: studio 15 ips, drive 0 dB, every character control at zero, mix 100 %, auto-gain on.

---

## 8. Decisions to make

1. **Name**: Tarnish, Foxing or Spool (§6), after the trademark searches.
2. **Machine list for 1.0**: studio tape, cassette, vinyl, AM radio, telephone. Is VHS a machine of its
   own or a cassette variant reached through presets (§2.18)? Is telephone a separate machine or a mode
   of the radio machine?
3. **Hysteresis**: JA by default with RK4 at 4x (recommended), or start with the cheaper memoryless chain
   and add JA in 1.1? Which solvers to expose (RK2 / RK4 / NR4) and under what labels ("eco / normal /
   accurate")? Train a learned solver (STN-like) at all? (Recommended: no, keep it analytic.)
4. **Oversampling and latency**: constant latency across factors by padding (Lacquer's choice), or a
   latency that changes with the factor? 8x as a real-time option or render-only?
5. **Double or float** for the oversampled region (recommended: double for the JA state, as in Lacquer's
   `Os4`).
6. **Lift `Os4` into `shared/dsp/`** now (touches Lacquer, guarded by its bit-identical test) or copy it
   into the tape plugin and merge later?
7. **Shared tape library scope**: which modules Space adopts in its first version (recommended: transport
   and dropouts at least, because they fix the phase-wrap bug, the excess depth and the sample-rate
   dependence of §3.1), and whether Space gets a JA option.
8. **Timeline-locked randomness** on by default in DAWs (§3.5)? It makes renders match playback but makes
   loops repeat the same dropouts.
9. **Print-through pre-echo**: offer it with an honest 1-3 s reported latency, or post-echo only?
10. **Static varispeed** (slow speed beyond the buffer length): resync jumps with crossfades, or a real
    pitch shifter? (Tape stop and short speed gestures need neither.)
11. **Tape stop triggers**: button, MIDI note, host transport follow, automation; which are in 1.0? Spin-up
    mode default (crossfade jump vs catch-up chirp)?
12. **Stereo behaviour**: physically correlated wow (default) plus a non-physical "stereo wobble" option?
13. **Measurement standard**: buy IEC 60386 / AES6-2008 for the exact flutter weighting curve (for the
    displayed "W&F %" and test T4), or derive it from a published plot and label the display as
    approximate.
14. **Visual direction**: A (oxide and cream, top-down transport) with the B inspect overlay
    (recommended), or B alone (cheaper), or C (3D render)?
15. **Telephone codec artefacts** (CELP-like smear, packet loss) in 1.0 or later?
16. **Auto-gain** default on (recommended, users judge saturation by loudness otherwise) and the reference
    level convention (0 dB drive = −18 dBFS RMS to the tape's reference level?).
17. **Presets**: the 32 of §7, or a smaller first set (around 24) with the effects category later?
18. **Hardware references**: dub test signals to a real cassette deck and turntable for the
    "measured against hardware" claim (§5)? Which decks are available?

---

## Sources

Accessed 2026-10-19 unless stated. Papers and standards are cited by their bibliographic reference where
no stable URL was fetched.

Physics, standards and measurement:
- J. Chowdhury, "Real-time Physical Modelling for Analog Tape Machines", DAFx-19, Birmingham, 2019:
  https://www.dafx.de/paper-archive/2019/DAFx2019_paper_3.pdf
- J. Chowdhury, "Complex Nonlinearities, Episode 3: Hysteresis" (notebook version):
  https://ccrma.stanford.edu/~jatin/ComplexNonlinearities/Hysteresis.html (Medium original:
  https://medium.com/@jatinchowdhury18/complex-nonlinearities-episode-3-hysteresis-fdeb2cd3e3f6, returned 403)
- D. C. Jiles and D. L. Atherton, "Theory of ferromagnetic hysteresis", *J. Magn. Magn. Mater.* 61 (1986)
  48-60 (not fetched; standard reference)
- R. L. Wallace Jr., "The Reproduction of Magnetically Recorded Signals", *Bell System Technical Journal*
  30 (4), 1951, p. 1145 (cited via US8111473, https://patents.google.com/patent/US8111473)
- Wikipedia, "Wow and flutter measurement": https://en.wikipedia.org/wiki/Wow_and_flutter_measurement
- IEC 60386:1972 catalogue entry: https://online.standard.no/en/iec-60386-1972-ed1
- Wow & flutter meter specification (IEC weighted band): https://parts.ttech.vn/wow-flutter-meter-mk-669c-keisoku-p107431.aspx
- reeltoreel.nl, "NAB vs. IEC": https://www.reeltoreel.nl/knowledgebase/books/audio/page/nab-vs-iec
- IASA TC-04 §5.4.10, "Replay equalisation": https://www.iasa-web.org/node/490 (seen in search results;
  404 on fetch)
- Pspatial Audio, "Tape equalisation correction": https://pspatialaudio.com/tape%20equalisation%20correction.htm
- sound-au.com, project 247 (tape replay EQ): https://www.sound-au.com/project247.htm
- Nortronics head data (LF cutoffs by speed): https://www.discolathe.com/.pdfs/Nortronics_Silvr_pp1-9.pdf
- homerecording.com, "30 ips / 15 ips mastering" (head bump, anecdotal):
  https://homerecording.com/bbs/threads/30ips-15-ips-mastering.282941/
- Wikipedia, "Compact Cassette tape types and formulations":
  https://en.wikipedia.org/wiki/Compact_Cassette_tape_types_and_formulations
- stereo2go.com, "The Cassette FAQ": https://stereo2go.com/forums/posts/58840/
- Noise-reduction patents (B-type ≈ 10 dB, C-type ≈ 20 dB at 5 kHz): https://patents.google.com/patent/US4473852,
  https://patents.google.com/patent/US4476502, https://patents.google.com/patent/US5185806
- Sencore Tech Tip TT190, "How VCR Hi-Fi Stereo Works":
  https://xdevs.com/doc/Sencore/Sencore_Tech_Tips/TT190p%20-%20How%20VCR%20Hi-Fi%20Stereo%20Works.pdf
- digitalFAQ forum, "VHS frequency response" (quotes Wikipedia): https://www.digitalfaq.com/forum/video-capture/5874-vhs-frequency-response.html
- SMPTE RP 47 (dropout definition): https://pub.smpte.org/pub/rp47/rp0047-1999_stable2004.pdf;
  SMPTE RP 121: https://pub.smpte.org/doc/rp121/19971207-pub/rp0121-1997_stable2010.pdf
- AM receiver bandwidth, whistle and monkey chatter (patents): https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/5303413,
  https://patents.google.com/patent/US5253298; Silicon Chip, "IF amplifiers pt. 2" (2003):
  https://siliconchip.com.au/Issue/2003/January/Intermediate+Frequency+%28IF%29+Amplifiers%3B+Pt.2
- Record eccentricity wow (secondhand): https://hifihub.ai/articles/what-is-wow-and-flutter;
  Lindos, "Rumble measurement": https://lindos.co.uk/articles/rumble-measurement;
  HiFi News, Holbo turntable lab report: https://www.hifinews.com/content/holbo-airbearing-turntable-lab-report
- Click/crackle duration classes (patent): https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/8892226
- S. J. Godsill and P. J. W. Rayner, *Digital Audio Restoration*, Springer 1998: https://sigproc.eng.cam.ac.uk/Main/SJGSpringer
- O. Mikkonen, A. Wright, E. Moliner, V. Välimäki, "Neural modeling of magnetic tape recorders", 2023:
  https://arxiv.org/abs/2305.16862 (abstract only)

Products (see §1 for each page): Chow Tape Model (https://github.com/jatinchowdhury18/AnalogTapeModel,
https://chowdsp.com/products.html), Wavesfactory Cassette, XLN Audio RC-20, Goodhertz Lossy / Vulf
Compressor / Wow Control, Baby Audio TAIP, Softube Tape, UAD Studer A800 / Ampex ATR-102, iZotope Vinyl,
Arturia Tape MELLO-FI, Klevgrand DAW Cassette, AudioThing Reels, Ableton Live 12 manual, Kilohearts Tape
Stop, dblue TapeStop, Cableguys TimeShaper, Image-Line Gross Beat, Aberrant DSP SketchCassette II, Caelum
Audio Tape Cassette 2 / Tape Pro, Kiive Tape Face, Airwindows ToTape / IronOxide.

In this repository: `plugins/space/Source/dsp/Network.h` and `Core.h` (working copy, 2026-10-19),
`plugins/lacquer/Source/dsp/Saturation.h`, `shared/dsp/Oversampling.h`, `shared/dsp/Blep.h`,
`shared/dsp/Common.h`, `plugins/lacquer/README.md`, `plugins/lacquer/docs/research.md` §6.10.

