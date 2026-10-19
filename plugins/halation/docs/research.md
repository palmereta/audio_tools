# Space: research for a delay-to-reverb "room of light" effect

> Working title "Space" (the folder name). The product name is still open; §6 evaluates candidates and
> recommends **Motes**, **Halation** or **Lightwell**.

Compiled 2026-10-19 for a new Audio Tools product: a free, open-source (AGPL-3.0) VST3 / AU / standalone
effect that unifies delay and reverb in one feedback delay network (FDN), with a single **Delay ↔ Space**
control that morphs from rhythmic multitap and ping-pong echoes into dense reverb. Characters cover tape,
BBD, clean digital, ping-pong and patterned delays, and plate, room, hall, spring, shimmer, infinite and
gated reverbs. Experimental features: full and per-band freeze, interval and chord shimmer, reverse and
reverse shimmer, granular and spectral processing inside the loop, smart ducking, elastic time changes,
a resonator that tunes the tail to MIDI, A/B scene morph, envelope-follower modulation, tempo-synced
glitch, "impossible rooms", and a safety limiter in the feedback path. The window shows a 3D room in which
every echo is a particle of light. The audience includes ambient producers, music therapists and
meditative or psychedelic listening; the product makes no medical or therapeutic claims.

Conventions (as in `plugins/lacquer/docs/research.md`):
- Every fact carries its source inline or in §2.14 (URL, paper, date read). Ableton facts come from the
  Live 12 manual and release notes; product facts from vendor pages, reviews and forum threads. Where only
  an abstract, a search snippet or a third-party quote was available, the text says so.
- **(inferred)** marks my own conclusions, calculations or design proposals. Sections 3, 4, 5 and 7 are
  design proposals throughout; their numbers are starting values to tune with the tests in §5.
- Product names are trademarks of their owners. Nothing here copies proprietary text, code or presets;
  descriptions of other products are paraphrased.
- Nothing here is legal advice. Before release, run a trademark search on the chosen name (§6).

---

## 0. Summary of recommendations

**What the market does** (§1).
- Ableton splits the territory into eight devices with separate vocabularies: Echo (two lines, tape-like
  character, reverb that can sit inside the loop, ducking, gate), Delay (Repitch / Fade / Jump time
  changes, Freeze), Hybrid Reverb (convolution + five algorithms, Freeze In, per-band decay multipliers),
  Reverb (Freeze with Flat/Cut), Grain Delay, Spectral Time (onset-retriggered spectral freeze, per-bin
  delay), Beat Repeat and Spectral Resonator (MIDI-tuned partials).
- Third-party leaders: Valhalla Supermassive (free; 22 FDN modes; **Warp** moves from echoes to reverb),
  Eventide Blackhole (inverse decay, **Ribbon** morph between two settings), EchoBoy (styles, groove),
  Strymon BigSky/TimeLine/Volante (Freeze vs Infinite Sustain, Pattern, Ice, golden-ratio head spacing,
  Wear/Mechanics).
- Users love playable infinite tails, one knob that morphs a lot, low CPU and modelled imperfection. They
  complain about menu diving, washes that drown a mix, metallic or grainy shimmer, CPU spikes, glitches
  when parameters move, and interfaces that show nothing of what the tail is doing.

**The core idea** (§2.5, §3.2).
- One FDN of **8 lines** (4 in Eco, 16 in Deep) whose feedback matrix is a **Kronecker product of 2×2
  rotations**, R(θ)⊗R(θ)⊗R(θ). It is the identity at θ = 0 (independent echoes), the normalised Hadamard
  at θ = π/4 (dense reverb), and orthogonal, hence stable, at every angle in between. Ping-pong is a
  permutation composed with it.
- The **Delay ↔ Space** macro drives five things together: matrix angle, input diffusion (0 → 0.75),
  line lengths (tempo grid → clusters → mutually prime room set), modulation depth and output taps.
- **Decay is always T60** (per-line gains g = 10^(−3m/(T60·fs)), Jot), shown as "repeats" at the delay
  end and seconds at the space end, so moving the macro changes how the sound is spread, not how long it
  lasts.
- Frequency-dependent decay with 3-band absorption filters per line; **per-band freeze** falls out of the
  same filters (one band's gain = 1).
- Time changes: **Tape** (glide) or **Clean** (two-head crossfade), shared by Time, tempo and the macro.

**Characters and experiments** (§2.7-§2.11, §3.3). 12 characters (Tape, Dark BBD, Digital, Ping-Pong,
Pattern, Plate, Room, Hall, Spring, Shimmer, Infinite, Gated) as loop inserts and absorption presets
over the same network. Shimmer gives each shifted line its own interval, so chords build in the tail;
granular shifting by default (Murmur's engine), spectral as an option; filtered and limited to 2-4 lines to
avoid metallic artefacts. Spectral freeze uses an STFT with random phase (the Paulstretch idea). Reverse
reverb that arrives *before* the note needs reported latency (lookahead mode). The resonator tunes line
lengths to multiples of the note period so the whole tail rings at that pitch.

**Safety** (§2.12). Orthogonal matrix + line gains < 1, insert gains counted in a computed loop gain, an
energy limiter across all lines (keeps colour), per-line soft clip as last resort, DC blocker in the loop,
output ceiling at −1 dBFS, and a **warning before self-oscillation** based on the computed loop gain plus a
measured guard.

**View** (§4). The 8 lines sit at the corners of a cube; the matrix's three rotation factors are the cube's
three axes, so turning Space visibly sends light along the edges until the room glows evenly. Particle size
= level, colour = a cheap per-line brightness proxy for the spectral centroid, lifetime = T60. Indicators:
tempo-grid strip with predicted echoes, decay graph with set vs measured T60, headroom bar. No flashing
above 3 per second (WCAG 2.3.1).

**Tests** (§5). 25 automated checks, including: no NaN under 200 random extreme settings; exact latency;
bit-identical output across block sizes 1-4096; T30 within ±10 % of the set T60 (goal 5 %, the ISO 3382-1
JND); freeze holds within ±1 dB for 60 s with no growth; loop never above the limiter ceiling; no
subnormals; matrix orthogonality to 1e-6; shimmer and resonator tuning to ±3/±5 cents; CPU < 1.5 % (M4)
for the default preset.

**CPU** (§2.13, inferred). The FDN core is ~12.5 MFLOP/s at 48 kHz (about 0.3-0.5 % of an M4 core);
expected 0.5-1 % for the default preset and 4-6 % for the heaviest, scaled from Murmur's measurements.

**Name** (§6). "Echo Chamber" and "Hall of Mirrors" are already used by a reverb plugin and a reverb pedal.
Recommended, with no collision found in audio: **Motes** (dust in a beam of light), **Halation** (the glow
around a light on film), **Lightwell** (a shaft that lets light into a building).

**Presets** (§7). 32 in four categories (Classic, Ambient, Rhythmic, Experimental), including all 16 names
from the brief.

**Reuse** (§8). From `shared/dsp/Fx.h`: the `DelayLine` wrap fix, `StereoDelay` as the 2-line ping-pong
reference, `PlateReverb` (Dattorro) as the Plate character's reference and regression oracle and its
input diffusers as the pre-loop diffuser; `SvfBand` for loop filters. From Murmur: grain voices and
scheduler (in-loop granular and the shimmer shifter), interval tables, sync divisions, LFOs, ducking
follower, the lock-free view publishing and `FlockView`'s render loop, and the dsptest/render tool pattern.

---

## 1. What existing devices do

### 1.0 Sources for this section

- [M] Ableton Live 12 Reference Manual, chapter "Live Audio Effect Reference",
  https://www.ableton.com/en/manual/live-audio-effect-reference/ (read 2026-10-19). Section numbers below
  (§29.x) are the manual's.
- [RN12] Live 12 release notes, https://www.ableton.com/en/release-notes/live-12/ (through 12.4.3,
  2026-07-14). [RN11] Live 11 release notes, https://www.ableton.com/en/release-notes/live-11/.
- The manual describes behaviour but gives numeric ranges for very few controls. Where a range is missing
  below it is because the manual does not give it; none were filled in from memory.
- Third-party products: vendor pages, reviews and forums, each cited inline. Valhalla's own site refused
  automated fetches (HTTP 403) on 2026-10-19, so Valhalla facts come from KVR, press coverage and
  Valhalla's Shimmer design notes PDF; that is said where it matters.

### 1.1 Ableton Echo (§29.14)

A two-line stereo delay with a built-in reverb, a modulation section and "character" (imperfection)
controls. [M]
- **Top section**: Channel Mode Stereo / Ping Pong / Mid-Side (in Mid/Side the two time knobs become Mid
  and Side); left/right time in note divisions or ms; Sync Mode choosers Notes / Triplet / Dotted / 16th;
  Stereo Link; per-channel **Delay Offset** sliders (a fraction of the time, for swing, independent even
  when linked); **Input** gain on the dry signal with a **D** (distortion) button; **Feedback** with a
  **Ø** button that inverts each channel inside the loop.
- **Echo tab**: the "Echo Tunnel" display draws each repeat as a ring, draggable to set time, with white
  dots on a fixed 1/8 grid; HP and LP filters each with resonance, drawn as a curve with handles.
- **Modulation tab**: Sine, Triangle, Saw Up, Saw Down, Square, Noise; Rate synced or in Hz; Phase
  (L/R offset, 180° = opposite); Mod Delay depth with an ×4 button (deep flanging on short times); Mod
  Filter depth; **Env Mix** blends LFO (0 %) and envelope follower (100 %) as the modulation source.
- **Character tab**: **Gate** (threshold, release); **Ducking** (threshold, release; turns the wet down
  while input is present); **Noise** (amount, morph between noise types); **Wobble** (amount, morph:
  irregular tape-like time modulation); **Repitch** (on: time changes bend pitch like hardware; off:
  crossfade); **Auto-off** (switches off about 8 s after the input stops unless Noise and Gate are both on).
- **Global**: Reverb amount with **Reverb Location** Pre Delay / Post Delay / inside the Feedback loop;
  reverb Decay; Stereo (0 % mono, > 100 % wider); Output; Dry/Wet with an Equal-Loudness option.
- Release notes: 12.0 "improved delay time calculations"; 12.1 (2024-10-08) made the Repitch smoothing
  time adjustable [RN12].

### 1.2 Ableton Delay (§29.11)

The plain stereo delay that replaced Simple Delay and Ping Pong Delay in Live 11. [M]
- Per side: **Sync** (16th-note buttons, 4 = one beat) or **Time** (1.00 ms-5.00 s); **Offset** in Sync mode
  (earlier/later); Stereo Link (Offset stays independent).
- **Feedback** (each side feeds only itself; no cross-feed except in Ping Pong) and **Freeze** (loops
  the buffer from the moment it is pressed and ignores new input).
- **Filter**: band-pass before the delay, Freq and Width on an X-Y display.
- **LFO**: Delay (time depth), Filter (frequency depth); since 12.4 (2026-05-05) rate modes Hz / ms /
  synced / triplet / dotted / sixteenth, 7 waveforms (Sine, Triangle, Ramp Up, Ramp Down, Square, S&H,
  Wander) and a **Morph** control (greyed out for S&H and Wander since 12.4.3) [M, RN12].
- **Time change behaviour**: **Repitch** (default, tape-style pitch bend), **Fade** (crossfade to the new
  time), **Jump** (instant, may click).
- Ping Pong toggle, Dry/Wet; context menu: Hi-Quality (better interpolation in Repitch/Fade, added in 11.2,
  costs CPU) and Equal-Loudness Dry/Wet [RN11].

### 1.3 Ableton Hybrid Reverb (§29.23)

Convolution and algorithmic engines in one device. [M]
- **Input**: Send (gain into the reverb only), Predelay in ms or synced (manual calls 1-25 ms typical for
  natural sounds), Predelay Feedback (separate for timed and synced pre-delay).
- **Routing**: Serial (convolution feeds the algorithm; Blend balances), Parallel (default in the Core
  Library since 11.3.20 [RN11]), Algorithm only, Convolution only.
- **Convolution**: IR categories Early Reflections, Real Places, Chambers and Large Rooms, Made for Drums,
  Halls, Plates, Springs, Bigger Spaces, Textures, User; user IRs by drag and drop (lost if the device is
  deleted and re-added); Attack and Decay envelope on the IR; Size.
- **Algorithms** (shared: Decay to −60 dB, Size, Delay, **Freeze**, **Freeze In** which lets new input
  into the frozen tail):
  - *Dark Hall*: Damping, Mod, Shape (small and resonant → large and diffuse), Bass X (low crossover),
    Bass Mult (low-band decay multiplier);
  - *Quartz*: Damping, Lo Damp, Mod, Diffusion (sparse → dense), Distance;
  - *Shimmer*: Damping, Mod, Pitch (semitones in the feedback), Diffusion (below 10 % gives dub-style
    echoes: Ableton's own example of a reverb sliding into a delay), Shimmer amount; on percussive input
    Size sets the echo spacing;
  - *Tides*: Damping, Wave (0 % noise, 50 % sine, 100 % square), Tide (depth), Phase, Rate (synced);
  - *Prism*: Low Mult, High Mult, X Over; small Decay and Size give the gated '80s snare.
- **EQ**: 4 bands, low/high switchable between pass (6-96 dB/oct) and shelf, two peaks; Pre Algo option.
- **Output**: Stereo, **Vintage** (Subtle, Old, Older, Extreme: lower sample rate and bit depth), **Bass
  Mono** (below 180 Hz), Dry/Wet.
- Release notes: 12.2 IR envelope decay fixed; 12.3 (2025-11-25) stale tail after toggling the device fixed
  [RN12]. Ableton's Hybrid Reverb FAQ (https://help.ableton.com/hc/en-us/articles/360019484980-Hybrid-Reverb-FAQ,
  seen only as a search snippet) says CPU can spike with small buffers or long IRs.

### 1.4 Ableton Reverb (§29.32)

The classic algorithmic reverb. [M]
- **Input filter**: low cut, high cut, X-Y pad (frequency, width).
- **Early reflections**: **Spin** (amount, rate; too high a rate gives audible Doppler) and **Shape**
  (small: gradual decay overlapping the diffuse part; large: faster decay and later diffuse onset).
- **Global**: Predelay (1-25 ms typical), Smooth (None / Slow / Fast, how size changes are smoothed), Size,
  Decay (to −60 dB), **Freeze** with **Flat** (bypasses the shelves while frozen) and **Cut** (stops new
  input reaching the frozen tail), Stereo (0-120°), Density (Sparse / Low / Mid / High; renamed from
  "Quality" in 11.2, Sparse is cheapest) [RN11].
- **Diffusion network**: Hi shelf (or 1-pole LP since 11.2) and Lo shelf, Diffusion and Scale (density and
  coarseness), Chorus amount and rate.
- **Output**: Reflect level, Diffuse level, Dry/Wet. No Live 12 changes [RN12].

### 1.5 Ableton Grain Delay (§29.22)

A delay whose output is re-granulated. [M] X-Y pad with selectable axes; Sync (16ths with a swing
percentage) or ms; **Spray** (random time jitter: smear at low values, broken rhythm at high);
**Frequency** (grain rate/size; Pitch and Spray depend on it); **Pitch** (crude grain pitch shift);
**Random Pitch**; **Feedback** (can self-oscillate); Dry/Wet. No numeric ranges in the manual. A 2005
forum thread reports pops at very low Frequency/Pitch (https://forum.ableton.com/viewtopic.php?p=85362;
may no longer apply).

### 1.6 Ableton Spectral Time (§29.37)

STFT freezer plus spectral delay. [M]
- **Freezer**: Manual (Fade In / Fade Out in ms) or Retrigger by **Onsets** (Sensitivity 0-100 %) or
  **Sync** (interval in ms or beats); Fade Shape Crossfade (X-Fade as % of interval) or Envelope (up to 8
  stacked freezes).
- **Delay**: Time / Notes / 16th / 16th Triplet / 16th Dotted; Feedback; **Shift** (frequency shift per
  repeat); **Tilt** (highs delayed more, or lows); **Spray** (random time per frequency bin); **Mask**
  (limit Tilt/Spray to highs or lows); Stereo; Delay Dry/Wet.
- **Resolution**: lower = less latency, less accuracy. Order Frz > Dly or Dly > Frz; Input Send; Dry/Wet;
  "Zero Dry Signal Latency" option.
- User report: ~29 ms of latency that disturbed externally monitored gear even with the device off
  (https://forum.ableton.com/viewtopic.php?p=1832433, 2025-08-03).

### 1.7 Ableton Beat Repeat (§29.5)

Tempo-synced capture and repeat. [M] Interval (1/32 to 4 bars), Offset (16ths), Chance (0-100 %), Gate
(length of the repeats, 16ths), Repeat (capture now), Grid (slice size) with No Triplets, Variation (with
Trigger / 1/4 / 1/8 / 1/16 / Auto), Pitch (by resampling) and Pitch Decay, Filter (centre, width), Mix
modes Mix / Insert / Gate, Volume, Decay.

### 1.8 Ableton Spectral Resonator (§29.36)

Spectral partials excited by the input. [M]
- Pitch mode Internal or MIDI (external source, mono/poly with 2-16 voices, glide in Mono, pitch-bend
  range 0-24 st); Freq (Hz or note) or scale degrees with Use Current Scale (12.2); Transpose ±48 st;
  **Stretch** (partial spacing; 100 % leaves only odd harmonics); **Shift** ±48 st on the input; Decay, HF
  Damp, LF Damp; Modulation None / Chorus / Wander / Granular with rate and pitch depth; Harmonics;
  Quantize to the scale or tuning; Input Send with limiter; Unison and Unison Amount; Dry/Wet.
- 12.2 (2025-06-11): scale awareness and tuning systems; Granular mode fixed; partial phases randomised
  [RN12].

### 1.9 Valhalla DSP

- **Supermassive** (free, v5.0.0, late 2025): 22 feedback-delay-network modes named after stars and
  galaxies, each with its own attack/decay/density character (e.g. Aquarius, "a dedicated echo that
  morphs into reverb"; Scorpio with cross-channel feedback). Controls Delay (longest line, ms or synced),
  **Warp** (scales the line lengths relative to Delay, from simple echoes through resonant/smeared
  repeats to reverb), Feedback, **Density**, Width (negative swaps L/R), Mod Rate/Depth, Low/High Cut, Mix.
  Max delay about 2 s per line (MusicTech, Dubspot). Sources:
  https://www.kvraudio.com/product/valhallasupermassive-by-valhalla-dsp/details,
  https://thebeatcommunity.com/2025/11/28/valhalla-dsp-release-supermassive-v5-0-a-free-download/ (2025-11-28),
  https://blog.dubspot.com/plugins/valhalla-supermassive (verified 2026-07-09).
  *Loved*: very low CPU, free, huge spaces, regular new modes (KVR forum,
  https://www.kvraudio.com/forum/viewtopic.php?p=7784164, 2020). *Complaints*: built for size, not tight
  rooms; can wash out a mix (Dubspot).
- **VintageVerb**: 22 modes (v4), three Color eras (1970s, 1980s, Now; 1980s audibly grainier). *Loved*:
  Lexicon-style modulation, sits in a mix, price (https://musicradar.com/reviews/tech/valhalladsp-vintageverb-574692).
  *Complaints*: not subtle, wants more EQ (https://www.musicianwave.com/valhalla-dsp-vintageverb-review/).
- **Shimmer** (2010): pitch shifter in the feedback loop; the classic sound is +12 with feedback ≥ 0.5;
  modes Single, SingleReverse, DualReverse (reverse modes smoother). Valhalla's design notes say the pitch
  shifting was deliberately left "un-deglitched" to avoid a metallic tone; the grain that remains reads as
  a string-section texture (https://www.valhalladsp.com/shimmer/ValhallaShimmerNotes.pdf). *Complaint*:
  noise/grain in Single mode (https://www.kvraudio.com/forum/viewtopic.php?p=4243257). Valhalla has not
  announced a successor (unverified either way).
- **Freq Echo** (free): delay with a frequency shifter in the loop; self-oscillates.

### 1.10 Eventide Blackhole and ShimmerVerb

- **Blackhole** ($99, iLok, from the DSP4000 era): **Gravity** inverts the decay envelope (swells instead of
  decays); **Ribbon** morphs between two parameter sets and **HotSwitch** jumps between them; Mix Lock;
  **Kill Switch** mutes the input so only the tail plays (users use it as a freeze)
  (https://www.eventideaudio.com/plug-ins/blackhole/). *Loved*: otherworldly tails, playable morphing.
  *Complaints*: not a natural room, dated UI, CPU on dense settings, iLok trouble
  (https://blog.dubspot.com/fr/plugins/blackhole).
- **ShimmerVerb** ($99): parallel pitch shifters in fourths, fifths and octaves with micro-pitch, ±2
  octaves; pitched delay up to 1 s or synced; three-band feedback; Freeze; Ribbon and HotSwitch
  (https://www.eventideaudio.com/plug-ins/shimmerverb/).

### 1.11 Soundtoys EchoBoy

30 echo styles modelled on classic units; modes Single, Dual, Ping-Pong and Rhythm Echo (up to 16 taps);
Saturation amount and type; **Groove / Feel / Accent** push and drag timing and add swing; Wobble (tape
flutter); Diffusion in the style editor; $199 (https://www.soundtoys.com/product/echoboy/). *Loved*: sound,
low CPU, stability (Sound On Sound called it the best of its kind,
https://www.soundonsound.com/node/4909575). *Complaints*: the interface ("horrible", "not well thought
out", even from fans: https://gearspace.com/board/music-computers/1051766-i-dont-like-echoboy.html) and
price.

### 1.12 Strymon BigSky, TimeLine, Volante

- **BigSky** (2013, $479): 12 machines Room, Hall, Plate, Spring, Swell, Bloom, Cloud, Chorale, Shimmer,
  Magneto, Nonlinear, Reflections; **Freeze** (holds, no new notes) vs **Infinite Sustain** (holds and adds
  new notes); spillover between presets (https://www.strymon.net/product/bigsky/). *Complaints*: tiny
  labels, 6-character names, editor setup (https://www.soundonsound.com/reviews/strymon-big-sky).
- **BigSky MX** (2024, $679): two engines in series/parallel/split, 10 s stereo IRs with editing,
  dedicated Infinite/Freeze switch (https://www.strymon.net/product/bigsky-mx/). Praised for moving
  functions back onto knobs (https://www.premierguitar.com/gear/reviews/strymon-bigsky-mx, 2024-09-09).
- **TimeLine** (2011, $449): dTape, dBucket, Digital, Dual, **Pattern**, Reverse, **Ice** (repeats pitched
  from −1 to +2 octaves), **Duck**, **Swell**, Trem, Filter, Lo-Fi; 30 s looper
  (https://www.strymon.net/product/timeline/). *Complaints*: menu diving, "programming is not fun".
  **TimeLine MX** adds Drum, Oil Can, MultiTap and Spectral engines, dual engines
  (https://www.strymon.net/product/timeline-mx/).
- **Volante** ($429): Drum, Tape and Studio (reel) echo; 4 playback heads with level, feedback and pan;
  **Spacing** Even / Triplet / Golden ratio / Silver ratio, morphing continuously; **Wear** (HF loss of worn
  media), **Mechanics** (speed wobble, creases, splices), Rec Level (saturation), spin-down on Pause; max
  4 s at half speed (https://www.strymon.net/product/volante/). *Complaints*: cryptic key combinations,
  heads only full or half level.

### 1.13 Chase Bliss, Red Panda, Fors, Arturia

- **Chase Bliss MOOD MKII** ($399): wet channel Reverb / Delay / Slip plus a micro-looper (Env, Tape,
  Stretch) that feeds it; freeze, sync, a "Classic" switch that brings back the original's glitches
  (https://www.chasebliss.com/mood-mkii). Loved for being "quirky and soundscapey"; complaints are
  learning curve and unpredictability. **Dark World** (two reverbs, 33 routings), **Lossy** (codec-style
  degradation, evolving freeze), **Habit** (echo collector with a long memory, retired 2026).
- **Red Panda Particle 2**: granular delay/pitch, 8 modes, ±1 octave, 2.5 s, freeze; "endlessly fun" but
  chaotic to tame (https://www.premierguitar.com/gear/red-panda-particle-2-review). **Tensor**: tape speed
  from reverse through stop to forward, time-stretch up to 4:1, always-listening buffer.
- **Fors**: the names in the brief ("Glue", "Sauna") do not exist; Opal and Dyad are Max for Live
  instruments. Relevant here is **Sala**, a free shimmer reverb with five controls (Size, Decay, Tone,
  Dry/Wet, **Zoom** = pre-diffusion focus), 2025 (https://fors.fm/devices,
  https://synthanatomy.com/2025/03/fors-sala-free-shimmer-reverb-plugin-for-macos-linux-and-windows.html).
  It is the clearest example of "very few controls, one of them unusual".
- **Arturia Efx**: Delay ETERNITY (5 routings, three time-change modes repitch / cut / fade, 2 LFOs with
  26 destinations, envelope follower), Rev INTENSITY (freeze with a release time, envelope follower to 4
  targets), FRAGMENTS (granular with freeze and a spatializer), MOTIONS (beat repeater and step
  modulators), and models of a plate, spring, RE-201 tape echo and Memory Man. Complaints are about the
  installer and plugin bloat more than the sound (vendor pages at arturia.com, MusicRadar/MusicTech
  reviews).

### 1.14 Open-source references (licence matters for an AGPL project)

| Project | Licence | What to learn | Source |
|---|---|---|---|
| Dragonfly Reverb | GPL-3 | Hall/Room/Plate/ER built on Freeverb3 algorithms (Dattorro, Moorer ER model) | https://github.com/michaelwillis/dragonfly-reverb |
| CloudSeedCore | MIT (original Cloud Seed, 2014) | multitap + diffuser + modulated delay network, used in Daisy Seed pedals; v2.0 is now commercial (Ghost Note Audio, 2024) | https://github.com/ValdemarOrn/CloudSeed |
| Surge XT effects | GPL-3 (licence on GitHub; not stated in the manual page read) | Reverb 2 decay 0-64 s with buildup and diffusion; Nimbus (Clouds port); spring with "Knock" | https://surge-synthesizer.github.io/manual-xt/ |
| Airwindows Galactic, kCathedral | MIT | Galactic "Replace" (how much new audio overwrites the space) is a freeze-amount control; kCathedral uses a 5×5 Householder matrix | https://www.airwindows.com/galactic/, https://www.airwindows.com/kcathedral/ |
| ChowMatrix | BSD-3 | a growable tree of delay lines, each with feedback, pan and distortion | https://github.com/Chowdhury-DSP/ChowMatrix |
| CHOW Tape Model | GPL-3 | physical tape model (hysteresis, loss filters); early fixed 16× oversampling maxed out CPUs | https://github.com/jatinchowdhury18/AnalogTapeModel |
| Vital | GPL-3 with trademark limits | reverb with chorus in the tank | https://github.com/mtytel/vital |

AGPL-3.0 is compatible with incorporating GPL-3, MIT and BSD code (GPL-3 §13 explicitly allows combining
with AGPL-3 works), but the project's practice (Lacquer §4.1, README credits) is to implement from
published methods and not copy code; keep that.

### 1.15 What users love and complain about (synthesis)

Recurring complaints (from §1.9-§1.13 and the forum threads cited there):
1. **Menu diving and hidden functions** (Strymon TimeLine/BigSky, Volante key combinations, Chase Bliss
   dip switches). The MX models were praised for putting controls back on knobs.
2. **Too big for a mix** (Supermassive, Blackhole, Cloud Seed): users fix it with filtered feedback, lower
   density, sends. Implication: good low/high cut in the loop and ducking are not optional.
3. **Shimmer artefacts**: grain and metallic tone from cheap pitch shifting (Valhalla's notes say so
   directly); the fixes users reach for are reverse-grain modes, darker colour, less feedback.
4. **CPU spikes** (Chow Tape's fixed oversampling, dense Blackhole settings, Hybrid Reverb with long IRs).
5. **Dated or awkward interfaces** (EchoBoy, Blackhole) and **no visual feedback** (Volante has no preset
   indicator; most plugins show nothing of what the tail is doing).
6. **Glitches on parameter changes** (Airwindows Galactic's "Bigness" crashes when moved; Surge room-shape
   changes interrupt the audio; Ableton Delay's Jump clicks by design).
7. **Licensing friction** (iLok, installers). Not our problem, but a reason people like free tools.

Recurring loves:
1. **Playable infinite and frozen tails** (BigSky Freeze vs Infinite Sustain, Hybrid Reverb Freeze In,
   Blackhole Kill Switch, MOOD).
2. **One knob that morphs a lot** (Supermassive Warp, Blackhole Ribbon, Volante Spacing, Galactic
   Replace, Hybrid Reverb's Shimmer Diffusion that falls into dub echoes below 10 %).
3. **Low CPU, so instances can be stacked** (Supermassive, EchoBoy).
4. **Free and open, still excellent** (Supermassive, Airwindows, Surge, Sala).
5. **Modelled imperfection as a feature** (VintageVerb eras, Volante Wear/Mechanics, EchoBoy Wobble,
   Echo's Noise/Wobble, Chase Bliss Classic).
6. **Groove in delays** (EchoBoy Groove/Feel, Echo's Delay Offset, Volante's golden-ratio spacing,
   TimeLine Pattern).
7. **Spillover**: tails that finish naturally when a preset changes or the effect is bypassed.

### 1.16 What to bring from Ableton, and how to go beyond it

Bring (the vocabulary Live users already know, so the manual can say "like Echo's ..."):
- Echo's **Character** ideas (Noise, Wobble, Ducking, Gate, Repitch) and its **reverb location** choice
  (pre, post, in the loop): in an FDN the "in the loop" position is native, not an add-on.
- Delay's three **time-change behaviours** (Repitch, Fade, Jump). We keep two (Tape glide, Clean
  crossfade) and drop the clicking Jump.
- Echo's **Delay Offset** for swing and **Env Mix** (LFO vs envelope follower as one blend).
- Hybrid Reverb's **Freeze In**, per-band decay multipliers (**Bass Mult**, Prism's Low/High Mult), Bass
  Mono and the Prism gated-snare idea; Reverb's **Freeze Flat / Cut** distinction.
- Spectral Time's **onset-retriggered freeze** and **Tilt** (frequency-dependent delay).
- Beat Repeat's **Interval / Chance / Gate / Grid** vocabulary for the glitch section.
- Spectral Resonator's **MIDI-tuned resonance** (Mono/Poly), applied to the tail.
- Echo's Tunnel display: a picture of the repeats.

Go beyond (what none of the eight devices does, inferred from the reading above):
1. **One continuous Delay ↔ Space control** over a single network, instead of chaining Echo into Hybrid
   Reverb. Hybrid Reverb's Shimmer algorithm hints at it (Diffusion < 10 % = dub echoes) but only inside
   one algorithm; Supermassive's Warp is the closest third-party precedent.
2. **Decay in seconds that means the same thing for a delay and a reverb** (T60 everywhere, shown as
   repeats at the delay end).
3. **Per-band freeze** (hold the lows, let the highs decay, or the opposite), which Live's freezes do not
   offer.
4. **Musical-interval and chord shimmer** assigned per delay line, plus **reverse shimmer**.
5. **Granular and spectral processing inside the feedback loop** (Grain Delay and Spectral Time are
   separate devices with only their own feedback).
6. **Tail tuned to MIDI notes** inside the loop, not as a separate resonator.
7. **Scene A/B morph** (Blackhole's Ribbon idea) and **impossible rooms** that change size or decay over
   time.
8. **A safety limiter in the loop and a warning before self-oscillation**, so infinite settings are safe
   to explore.
9. **A view that explains the sound** (the room of light, §4), plus measured T60 shown next to the set
   value.


---

## 2. Algorithms

References are listed in §2.14 with URLs; inline tags like [Jot91] point there. Items read only as an
abstract or snippet are marked there.

### 2.1 Why a feedback delay network

A feedback delay network (FDN) is N delay lines whose outputs are mixed by an N×N matrix A and fed back
to their inputs, with a gain or filter on each line [Stautner82, Jot91, JOS-PASP]. It is the right core
for this product because the same structure is both things the brief asks for:
- with A = identity (or a permutation), it is N independent delays (multitap, ping-pong, patterns);
- with A dense and orthogonal (Hadamard, Householder) and mutually prime lengths, it is a reverberator
  whose echo density grows each pass;
- every point in between is a valid, stable structure as long as A stays orthogonal and the line gains
  stay below 1 (§2.5, §2.12).

Schroeder's comb/allpass reverberator [Schroeder62] and Moorer's low-passed combs plus a tapped
early-reflection delay [Moorer79] are special cases or ancestors (a parallel comb bank is an FDN with a
diagonal matrix). Dattorro's plate [Dattorro97] is a different topology (allpass diffusers into a
figure-of-eight tank), already in the repo (§8.1); in this product it serves as the Plate character's
reference sound and as a regression oracle.

### 2.2 The FDN core: lengths, matrix, density

**Equations** (per sample, N lines): s_i = line i output; y = Σ c_i s_i (output taps);
input to line i = b_i x + Σ_j A_ij · g_j · h_j(s_j), where g_j is the line's broadband gain and h_j its
absorption filter [Jot91, JOS-PASP].

**Matrix choices** [JOS-PASP, Schlecht17-lossless, Signalsmith21]:

| Matrix | Cost per sample (N = 8) | Notes |
|---|---|---|
| Identity / permutation | 0 | independent echoes / ping-pong; no diffusion |
| Hadamard (normalised, N = 2^k) | N·log₂N = 24 add/sub + 1 scale (fast Walsh-Hadamard) | maximally dense mixing, every entry ±1/√N |
| Householder A = I − (2/N)·11ᵀ | ~2N = 16 ops | cheap, any N; for N = 4 every entry is ±½ (same density as Hadamard), for larger N the diagonal dominates (1 − 2/N = 0.75 at N = 8), so mixing per pass is weaker |
| Dense random orthogonal | N² = 64 MACs | no structure; useful for listening comparisons only |
| Kronecker rotation R(θ)⊗R(θ)⊗R(θ) (proposed, §2.5) | same as Hadamard butterfly with sin/cos instead of ±1: 3 stages × 4 rotations × 4 mult = 48 mult + 24 add | identity at θ = 0, normalised Hadamard at θ = π/4, orthogonal for every θ |

**Delay lengths** (documented rules, then our numbers):
- Lengths should be **mutually prime** (or at least share no small common factors) so echoes from
  different lines do not pile up on the same samples and the modes do not cluster [Jot91, JOS-PASP].
- **Modal density**: the total length M = Σ m_i (samples) equals the number of modes between 0 and fs; a
  common criterion asks for M ≥ 0.15·T60·fs so that modes are dense enough at the desired decay time
  [JOS-PASP, after Schroeder]. Computed for our three sets at 48 kHz (inferred, our numbers):

| Set | Lengths (ms, rounded to the next prime number of samples at 48 kHz) | Σ (ms) | T60 meeting the rule |
|---|---|---|---|
| Small | 7.3, 9.1, 11.4, 13.7, 16.9, 19.1, 23.4, 27.1 | 128 | ≤ 0.9 s |
| Room | 29.7, 37.1, 41.1, 43.7, 53.7, 60.0, 67.3, 79.1 | 412 | ≤ 2.7 s |
| Hall | 61.2, 73.1, 89.0, 103.1, 127.1, 149.1, 167.0, 193.3 | 963 | ≤ 6.4 s |

  Longer decays than the rule allows still work well in practice because modulation (§2.6) smears the
  modes in time; Valhalla and Signalsmith both rely on modulation against ringing and metallic colour [ValhallaBlog,
  Signalsmith21]. For T60 > 10 s, either N = 16 or deeper modulation (inferred).
- **Spread**: a ratio of about 1:2.5-3 between shortest and longest line (as in the sets above) avoids
  both a "boingy" short loop and a slow-building long one; geometric spacing with a small random jitter
  is the usual recipe [Signalsmith21; inferred for the exact ratio].
- **Echo density** grows roughly as Nᵏ after k passes through a dense matrix; with N = 8 and a 30 ms mean
  length, after 3 passes (~ 90 ms) there are ~ 512 reflections, which is why a pre-loop diffuser is still
  useful for transients (the first 50 ms) [Schlecht-density; inferred numbers].

**Per-line gains for a target T60** [Jot91]: g_i = 10^(−3·m_i / (T60·fs)). Each line loses 60 dB in T60
seconds regardless of its length, so all modes decay together. Computed for the Room set: g ranges
0.579-0.814 at T60 = 1 s, 0.834-0.934 at 3 s, 0.947-0.980 at 10 s.

**Frequency-dependent T60 (absorption filters)** [Jot91, JOS-PASP, Jot-Chaigne]: replace g_i by a filter
h_i(z) whose magnitude at each frequency equals 10^(−3·m_i / (T60(f)·fs)). The simple version is a
first-order low-pass or a low and a high shelf whose gains in dB are proportional to m_i; a "tone
correction" filter after the network restores the overall spectrum so that a short-decay band is not also
quieter in level. Proposed (inferred): 3-band (low shelf, broadband, high shelf) per line, crossovers
200 Hz-1 kHz and 2-8 kHz, multipliers 0.25×-4× of the mid T60 (Ableton's Bass Mult and Prism Low/High Mult
are the same idea, §1.3). Per-band freeze (§2.11.1) is this filter with one band's gain set to exactly 1.
Recent work fits these filters more accurately with a graphic-EQ per line ("accurate reverberation time
control in FDNs", [Prawda-Schlecht]); for a creative effect a two-shelf design is enough (inferred).

**Line count recommendation** (inferred):
- **N = 8** default: dense enough for halls with modulation, cheap, and maps onto the cube view (§4.1).
- **N = 4** "Eco": small rooms, delays and patterns; half the cost; a square in the view.
- **N = 16** "Deep": long halls and the Infinite character; Hadamard 16 is still a butterfly
  (64 add/sub).

### 2.3 Diffusion before the loop (Schroeder, Dattorro, Griesinger)

- Allpass diffusers (Schroeder allpass, coefficient around 0.7 [Schroeder62]; Dattorro uses 0.75 and 0.625
  on four input allpasses of 142/107/379/277 samples at 29761 Hz [Dattorro97]) turn an impulse into a
  dense burst before it enters the tank, so the tail starts smooth.
- The multichannel alternative [Signalsmith21]: split into N channels, apply a short random delay per
  channel, a Hadamard mix and a polarity flip, repeat 3-4 times with doubling delay ranges; this gives
  diffusion without the allpass "ringing" colour.
- Allpass diffusion raises echo density quickly but leaves a metallic colour on vocals and drums; Valhalla's
  answer is a user Diffusion control over the allpass coefficients plus modulation [ValhallaBlog]. Lexicon-era
  designs (Griesinger) relied on random modulation of delay taps for smooth tails [Griesinger89; memory].
- **For the morph**: the diffuser coefficient g_d is one of the five quantities that `space` drives
  (0 at the Delay end, 0.70-0.75 at the Space end). At g_d = 0 the allpass is a pure delay of its length,
  so the diffuser lengths should be short (1-12 ms) to avoid shifting the echo grid; better, subtract the
  diffuser group delay from the pre-delay (inferred).

### 2.4 Signalsmith's "let's write a reverb" structure (practical reference)

[Signalsmith21] builds a good-sounding reverb in a few hundred lines: 8 internal channels; a diffuser of
4 steps whose delay ranges double (20, 40, 80, 160 ms in the example), each step a random delay per
channel (one per equal segment of the range), a channel shuffle with polarity flips and a Hadamard mix;
then a feedback loop of 8 delays of 100-200 ms mixed by a Householder matrix ("some mixing without too
much"), with decay set by one feedback gain (85 % in the example). Optional extras: gentle modulation of
some loop delays with fractional reads, shelving filters for frequency-dependent decay (a −1.5 dB high
shelf next to −1.5 dB of loop gain makes the highs die away about twice as fast) and early reflections
tapped from the diffuser. The author notes elsewhere that a dense matrix needs a few loops to build
density, so pre-diffusion helps, and that 3-4 diffusion steps are enough. This is the closest public,
clearly documented reference to what we need. Its structure fits the proposed morph: keep the diffuser
(its depth driven by `space`) and replace the fixed Householder with the Kronecker rotation of §2.5, or
keep Householder as an alternative "Room" matrix (inferred).

### 2.5 Morphing an FDN between a multitap delay and a reverb

The documented tools:
- **Orthogonal matrix families**: any product or Kronecker product of orthogonal matrices is orthogonal;
  lossless FDNs need a unitary (orthogonal) feedback matrix, or more generally one whose eigenvalues lie on
  the unit circle [Schlecht17-lossless]. Time-varying orthogonal matrices keep the network stable while
  they change [Schlecht15-timevarying].
- **Mixing time and echo density** depend on how dense the matrix is [Schlecht-density].
- **Precedents**: Supermassive's *Warp* scales line lengths relative to the longest one (from echoes to
  smeared repeats to reverb) and its *Density* thins the tail (§1.9); Hybrid Reverb's Shimmer algorithm
  falls into dub-style echoes with Diffusion < 10 % (§1.3); Umbrella Company's HUME pedal sweeps from
  delay repeats to reverb wash with one slider (§6.2). None publishes its method (inferred: they very
  likely do something similar to what follows).

Proposed method (inferred; the five quantities of §3.2):
1. **Matrix angle**: A(θ) = R(θ) ⊗ R(θ) ⊗ R(θ) for N = 8, with R(θ) = [[cos θ, sin θ], [−sin θ, cos θ]].
   At θ = 0 it is the identity; at θ = π/4 every entry is ±1/√8 = ±0.3536, i.e. the normalised
   Hadamard matrix (checked numerically: orthogonality error 2·10⁻¹⁶ at θ = 0.3; entries 0.3536 at π/4).
   The energy a line keeps per pass is cos⁶θ (0.970 at θ = 0.10, 0.760 at 0.30, 0.125 = 1/N at π/4).
   Because the network is stable for every θ, `space` can be automated freely. For ping-pong, compose
   with a permutation P that swaps the left-panned and right-panned lines: A = P·A(θ) (still orthogonal).
   N = 4 uses two factors, N = 16 four.
2. **Input diffusion** g_d: 0 → 0.75 (§2.3).
3. **Line lengths**: at the Delay end the lengths are the musical times (one time for all lines, or the
   pattern taps); they move toward the mutually prime room set as `space` rises, through an intermediate
   "cluster" stage where each line sits a few percent off the grid (each echo becomes a small cloud). The
   length change is slewed (Tape feel) or crossfaded (Clean feel), §3.2.
4. **Modulation depth**: character-only at the Delay end, reverb wander at the Space end (§2.6).
5. **Output taps**: at the Delay end each line has its own level and pan (taps of a pattern); at the
   Space end the output vectors are orthogonal combinations for a decorrelated stereo tail.

Equal line lengths with a dense matrix make a periodic, metallic flutter echo (every path takes the same
time), so the length spread must be well under way before θ gets large; the table in §3.2 orders them that
way (inferred, to be checked by ear and by test S14).

### 2.6 Modulation

- **Why**: static FDNs ring at their modal frequencies; slowly modulating delay lengths moves the modes so
  no single one rings, and it widens the stereo image [Griesinger89, ValhallaBlog, Dattorro97].
  Dattorro's plate modulates two tank allpasses by ±16 samples at 29.8 kHz (~ ±0.54 ms) at about 1 Hz.
- **How much** (proposed ranges, inferred from the above and from typical settings):
  - reverb wander: 0.1-1.5 Hz, depth 0.2-2 ms on each line, different rates per line (e.g. golden-ratio
    spaced) or filtered random ("drift");
  - chorus-like lush halls: up to 3-5 ms at 0.2-0.5 Hz (pitch deviation ≈ 2π·f·depth: 2π·0.3·0.004 =
    0.75 % ≈ 13 cents, audible as gentle detune);
  - tape wow/flutter: §2.7; BBD: §2.8.
- **Interpolation**: modulated lines need fractional reads. Linear interpolation is a time-varying
  low-pass (worst at half-sample positions: −∞ at Nyquist), which inside a feedback loop turns modulation
  into a fluttering HF loss; cubic Lagrange/Hermite costs ~4 MACs more and is flat enough below 10 kHz at
  48 kHz; first-order allpass interpolation is flat in magnitude but has transients when the delay changes
  quickly [Dattorro97-part2, JOS-PASP]. Proposal: cubic Hermite for all modulated reads, integer reads
  for unmodulated fixed taps (inferred).
- **Time-varying matrix** as an alternative to delay modulation: slowly rotating θ around its set value
  (± 0.02 rad) modulates the mixing without pitch change [Schlecht15-timevarying]; useful for the Clean
  characters where pitch wobble is unwanted (inferred).

### 2.7 Tape delay

- **Wow and flutter** [Arnardottir08, Chowdhury19; spec values from standards practice]: wow is slow speed
  variation (below ~4-6 Hz: capstan eccentricity, reel), flutter faster (~6-100 Hz). A worn consumer echo
  sits around 0.1-0.3 % weighted rms; a deliberately "broken" setting 1-3 %. Model as a sum of 2-3
  sinusoids at incommensurate rates (e.g. 0.6, 1.1 Hz wow; 7, 11 Hz flutter) plus low-passed noise
  (inferred, a common recipe). Depth in samples = speed deviation × delay time: 0.2 % of 375 ms = 0.75 ms.
- **Saturation**: record-head saturation is a soft, slightly asymmetric curve with hysteresis
  [Chowdhury19]; a tanh with a small even-order term, 2× oversampled, captures most of it for an effect
  (inferred). In the loop it also limits runaway feedback (the sound of dub sirens).
- **Head bump**: playback heads produce a low-frequency resonance (a few dB) whose frequency scales with
  tape speed, roughly 40-120 Hz [Chowdhury19]. A peaking filter +1 to +3 dB.
- **Losses and wear**: gap and spacing losses (HF roll-off rising with wear), noise, dropouts (short random
  gain dips) [Chowdhury19; Volante's Wear/Mechanics as the product precedent, §1.12].
- **Time changes**: the Tape time feel is literally a tape speed change: the read pointer slews, pitch
  glides, exactly like Ableton Delay's Repitch (§1.2).

### 2.8 BBD (bucket brigade) delay

- **Delay time** = N_stages / (2·f_clock) [Raffel10, Holters18]: e.g. a 4096-stage chip at 10-100 kHz clock
  gives 20-205 ms; a 1024-stage chip 5-51 ms. Long BBD delays therefore have a low clock and need strong
  anti-alias filtering, which is why they are dark.
- **Model** [Holters18]: input anti-aliasing filter, sample-and-hold at the clock rate (the BBD is a
  discrete-time system at f_clock), output reconstruction filter; Holters & Parker combine the BBD with its
  filters into one efficient model. Clock leakage gives a faint whine at f_clock (usually filtered).
- **Companding** (NE570/571-style 2:1 compressor before, 1:2 expander after) reduces noise but makes
  "breathing" on decays [Raffel10]; model as a slow envelope-driven gain pair with mismatched time
  constants (inferred).
- **For this plugin**: run the BBD section at a variable internal rate tied to delay time (that is what
  makes long times dark and grainy) or, cheaper, approximate with a delay-time-dependent low-pass
  (2.5-8 kHz), soft clipping, noise and compander (inferred).

### 2.9 Spring reverb

- **Physics**: a helical spring is strongly dispersive: high frequencies travel at a different speed from
  low ones, so an impulse becomes a chirp that repeats at the round-trip time, the familiar "drip"
  [Välimäki10, Parker11, Abel06, Bilbao-Parker].
- **Efficient model** [Välimäki10, Parker11]: a cascade of many identical first-order (or "stretched")
  allpass filters produces the chirp; a feedback loop with a delay gives the repeats; a low-pass sets the
  bandwidth (springs cut off a few kHz); two or three springs in parallel with slightly different
  parameters, as in real tanks. Typical figures: tens to a few hundred allpass sections (stretched allpass
  reduces that), round-trip times of tens of ms.
- **For this plugin**: replace 2-3 of the 8 FDN lines' plain delays with "dispersive lines" (delay +
  stretched-allpass chain) in the Spring character; keep the rest as diffuse tail (inferred). Cost: the
  allpass chain dominates (e.g. 3 springs × 50 sections × 4 ops ≈ 600 ops per sample, still < 1 % of a
  core).

### 2.10 Gated and nonlinear reverb

- Historically, "gated" reverb was a reverb (often a room with compression) cut by a noise gate keyed from
  the dry drum, first found by accident at Townhouse Studios around 1979-80; the AMS RMX16's "NonLin2"
  program then made it repeatable [History-gated]. Such "nonlinear" programs have a decay envelope that
  stays roughly flat and then ends abruptly (or rises) instead of decaying exponentially (envelope shape
  from general knowledge, not from a primary source).
- **Two implementations** (inferred): (a) **gate**: envelope follower on the input, hold 50-400 ms, release
  5-50 ms on the wet output; (b) **nonlinear shape**: a burst of dense diffusion whose output is shaped by
  a deterministic envelope (flat, rising, or "reverse") triggered by onsets, with very short T60 in the
  loop. Ableton's Prism algorithm gets the gated snare from small decay and size (§1.3). Blackhole's
  Gravity (inverse decay) is the same envelope idea pushed further (§1.10).

### 2.11 Experimental features

#### 2.11.1 Freeze, full and per band

- **Full freeze**: input to the loop off (or crossfaded to 0 over 20-50 ms), line gains exactly 1,
  absorption filters bypassed (or made allpass), matrix orthogonal ⇒ the loop is lossless and holds its
  energy indefinitely. Modulation with cubic interpolation is very slightly lossy at high frequencies, so
  a frozen tail darkens slowly; compensate with a tiny HF shelf or accept it ("Freeze Flat" in Ableton
  Reverb bypasses the shelves for the same reason, §1.4) (inferred).
- **Freeze In** (BigSky's Infinite Sustain, Hybrid Reverb's Freeze In): keep the input on while frozen; the
  energy then grows without bound unless the loop limiter or an energy normaliser holds it. Proposal:
  measure loop energy E = Σ s_i² (smoothed, 50 ms) and scale the input gain so E stays at the level at the
  moment of freezing (inferred).
- **Per-band freeze** (new, inferred): set the absorption filter's gain to 1 in the frozen band only, and
  remove that band from the input with the complementary filter so new material does not pile up there.
  "Hold the lows as a drone, let the highs decay" or "freeze the shimmer, keep the body moving".
  Crossovers: Linkwitz-Riley 2nd order on the input (cheap) and the shelf filters in the loop.

#### 2.11.2 Shimmer in the loop

- **Where**: a pitch shifter on some lines after the absorption filter (§3.1); each pass shifts again, so
  +12 climbs octave by octave until the absorption and a low-pass after the shifter remove it [ValhallaShimmer;
  Eventide ShimmerVerb, §1.9-1.10].
- **Granular (time-domain) shifter**: 2-4 overlapping read heads moving through a short buffer at the
  pitch ratio, each windowed (Hann), 30-100 ms grains. Cheap (a few MACs per head per sample), zero
  extra latency beyond the grain, good on pads, grainy on transients and periodic "flutter" at the grain
  rate. Randomising grain start and length by 10-30 % and using reverse grains reduces the periodicity
  (Valhalla's SingleReverse/DualReverse modes, §1.9). Murmur's grain engine can serve as this shifter
  (§8.3).
- **Phase vocoder / spectral shifter**: STFT 2048-4096 at 48 kHz, hop N/4; scales bin frequencies with
  phase propagation [Bernsee99]. Smooth on sustained material, smears transients ("phasiness"), adds
  latency equal to the window, which inside the loop just adds to the loop delay. Open libraries:
  Signalsmith Stretch (MIT) [SignalsmithStretch]; Rubber Band is GPL/commercial (GPL is compatible with
  AGPL, but our practice is to implement methods, §1.14).
- **Avoiding metallic artefacts** (from the sources above plus inference): shift only 2-4 of 8 lines so
  unshifted energy keeps the tail natural; low-pass after the shifter (5-8 kHz) and high-pass in the loop
  (150-300 Hz); keep shimmer amount (the fraction of the shifted signal) at 10-35 %; randomise grain
  timing; avoid shifting already-shifted material more than ~3 times before it is filtered out; prefer
  the spectral method for chords (several ratios from one analysis).
- **Musical intervals and chords** (new, inferred): give each shifted line its own ratio: lines 1-2 +12,
  lines 3-4 +7 (ratio 2^(7/12) = 1.4983; a just fifth 1.5 sounds purer in a feedback loop because
  repeated shifts stay on harmonics: offer "just" tuning as an option), others 0. The matrix then mixes
  them, so the tail builds the stacked intervals (+7, +12, +19, +24 ...). Chords: major +4/+7/+12, minor
  +3/+7/+12, sus +5/+7/+12, fifths +7/+14/+21.

#### 2.11.3 Reverse and reverse shimmer

- **Reverse delay**: a buffer of window W read backward in overlapping segments (two heads, Hann
  crossfade, 50 % overlap), W 50 ms-2 s; TimeLine's Reverse machine is the hardware precedent (§1.12).
- **Reverse reverb, real time**: the reversed tail can only follow the note (you cannot play backward what
  has not happened); capture the wet tail into windows and play each window backward ("swell after the
  note") (inferred).
- **Reverse reverb with lookahead** ("Ghost Before You"): delay the dry signal by W and report W as plugin
  latency; the host's delay compensation then aligns the dry, and the reversed tail of each note arrives
  before it in playback. Costs W of latency (unusable for live monitoring; fine on playback and when
  rendering). Latency must not change at run time, so W is chosen per preset from a small set (inferred).
- **Reverse shimmer**: reverse window + pitch shift in the loop (Valhalla's reverse pitch modes are the
  precedent).

#### 2.11.4 Granular inside the loop

Grains read from a short history of one or more lines and are written back into the loop (Murmur's
feedback-into-memory, §8.3, but inside the FDN). Parameters: density 2-60 grains/s, size 20-300 ms, spray
in position, pitch spray ±0-12 st quantised to intervals, reverse probability. Use: "the tail sparkles",
"clouds of fragments that become a room". Cost scales with grains (Murmur: 128 one-second grains = 2.55 %
of an M4 core with its delay and reverb), so cap at 32 grains in the loop (inferred).

#### 2.11.5 Spectral freeze and smear

- **Freeze**: STFT (window 2048-8192, Hann, hop N/4); capture the magnitude spectrum; resynthesise each
  frame with that magnitude and random phase (or phase advanced by the bin frequency plus a random term);
  overlap-add. Random phase removes the "buzzy frame repeat" of a looped frame. This is the core idea of
  Paulstretch [Paulstretch]: STFT overlap-add with a Hann window, very large windows and randomised phases
  instead of phase propagation. Paulstretch's code is GPL (not public domain); we implement the method.
- **Smear**: recursive averaging of magnitudes over frames (|X|_t = a·|X|_(t−1) + (1−a)·|X_t|, a = 0.8-0.995)
  makes a time-blurred spectrum: transients melt into a pad. Spectral Time's Spray and Tilt (per-bin delay)
  are related (§1.6).
- **Placement**: on the wet path after the network (cheap and predictable), or as a loop insert on one
  line for "spectral ice" that keeps being diffused (inferred). Cost at 48 kHz with N = 4096, hop 1024:
  ~ 47 frames/s × 2 FFTs × 2 channels ≈ 46 MFLOP/s ≈ 1 % of a core (inferred estimate).

#### 2.11.6 Resonator tuned to MIDI

- **Post resonator**: a bank of tuned comb or two-pole resonators per note (like Ableton's Resonators /
  Spectral Resonator in MIDI mode, §1.8) on the wet path.
- **In-loop tuning** (new, inferred): set every line length to an integer multiple of the note period
  P = fs/f0 (with fractional precision via allpass or cubic interpolation). Then every echo path through
  the network has a length that is a multiple of P, so the impulse response lives on a lattice of period
  P and its spectrum peaks at the harmonics of f0: the whole tail rings at that note while keeping its
  diffusion. For chords, assign line pairs to chord notes and keep θ moderate (0.2-0.4 rad) so the pairs
  mix less (mixing between lattices blurs the tuning). Test S16 checks ±5 cents.

#### 2.11.7 Elastic time changes

Tempo changes, Time knob moves and `space` moves all change line lengths. Tape feel: slew the length
(1-pole, 150-600 ms, or a rate limit in octaves per second), pitch glides. Clean feel: two heads per line
and an equal-power crossfade of 30-80 ms, pitch never changes (Ableton Delay's Fade, Arturia ETERNITY's
"digital fade", §1.2, §1.13). A third behaviour, "half/double speed" (Volante), is a tape-speed jump that
glides by an octave (inferred, optional).

#### 2.11.8 Smart ducking

Classic ducking (Echo, TimeLine's Duck machine, Murmur) lowers the wet signal while the input is loud.
"Smart" (inferred): threshold relative to the current tail level, so the duck depth adapts; separate
attack 2-20 ms and release 100-800 ms; mode "duck the tail" (output) or "duck the feed" (input to the
loop: old tails keep ringing, new notes stay dry); optional band-limited (duck only the mid band where the
voice sits).

#### 2.11.9 Envelope follower modulation

One follower on the input (attack 1-50 ms, release 50 ms-2 s), routed to up to two targets with signed
amounts: Space (loud = clean echoes, quiet = room), Decay, Mix, Shimmer amount, grain density, "auto
freeze" when the level drops below a threshold. Echo's Env Mix and Arturia's envelope followers are
precedents (§1.1, §1.13).

#### 2.11.10 Tempo-synced glitch and stutter

Beat Repeat's model (Interval, Offset, Chance, Gate, Grid, §1.7): capture a slice of 1/32-1/2 note at a
grid position with a probability, repeat it 2-16 times with optional pitch/decay/reverse, and feed the
repeats into the network (so the stutters become part of the space). Crossfade 2-5 ms at slice edges.

#### 2.11.11 Impossible rooms

Time-varying room parameters (inferred, using the stability of §2.5):
- **Breathing**: line lengths scaled by 1 ± a·sin(2π f t), a 2-15 %, f 0.03-0.2 Hz (Tape feel: the pitch
  sways; Clean feel would need constant crossfading, so breathing is a tape-feel feature).
- **Collapsing**: size ramps down after each onset (100 % → 15 % over 2-8 s) while T60 is held, so pitch
  rises and density increases; or T60 ramps from long to short ("the walls fall in").
- **Inverse decay**: envelope rising instead of decaying (Blackhole's Gravity, §1.10), done with an output
  envelope on a long tail.
- **Decay that breathes**: T60 modulated by an LFO between two values, keeping per-line gains < 1.

#### 2.11.12 A/B scene morph

Two complete parameter sets; one Scene control interpolates continuous parameters (times and frequencies
in log, gains in dB, θ linearly, all smoothed) and switches discrete ones at 50 %. Blackhole's Ribbon and
HotSwitch are the precedent (§1.10). Because every intermediate state is a valid FDN (§2.5), the morph
cannot destabilise the loop; the safety limiter still guards inserts (shimmer, resonator) whose gains may
peak mid-morph.

### 2.12 Feedback safety

Layers (inferred design; references for the principles):
1. **Structural**: the matrix is orthogonal for every setting (losslessness [Schlecht17-lossless]), so the
   loop gain equals the largest line gain × filter gain. Keep every computed g_i·max|h_i| ≤ 0.9999 except
   in freeze (exactly 1, inserts bypassed or energy-normalised) and in "allow self-oscillation".
2. **Gain budget of inserts**: head bump, resonator Q, shimmer level compensation and saturation make-up
   are counted in the loop-gain estimate (§4.5); the engine lowers the line gains to keep the product
   below 1 when the user sets an insert that would push it above.
3. **Energy limiter in the loop**: compute E = Σ_i s_i² over the lines (one value per sample, smoothed with
   1 ms attack, 100-300 ms release); if √(E/N) exceeds the loop ceiling (default −6 dBFS per line), scale
   all line inputs by the same factor. Scaling all lines equally keeps the matrix orthogonal and the
   tail's colour intact (unlike per-line clipping, which distorts) (inferred).
4. **Per-line soft clip** as the last resort (tanh-like, knee at the ceiling + 3 dB), as `StereoDelay`
   already does with `fastTanh` (§8.1). In the Tape and BBD characters this is the saturation itself.
5. **DC blocker in the loop** (5-10 Hz): saturation and asymmetric curves generate DC, which a lossless loop
   would accumulate.
6. **Output ceiling**: a fast peak limiter or soft clip on the wet output at −1 dBFS (default on), so a
   preset can never blast the listener; tests S1 and S10 check it.
7. **NaN guard**: if any line state is not finite (`isFiniteF` in `Common.h`), clear the network and fade in
   (should never trigger; counted in tests).

### 2.13 Recommended ranges and CPU estimate

| Parameter | Range | Default | Source |
|---|---|---|---|
| Lines N | 4 / 8 / 16 | 8 | §2.2 (inferred) |
| Line length (Space end) | 7-200 ms (sets of §2.2) scaled by Size 0.25-2× | Room set | §2.2 |
| Line length (Delay end) | 1 ms-4 s per line (buffer 4.1 s, like `StereoDelay`) | 1/8 dotted | §1.2 (Ableton Delay: 5 s max) |
| Pre-delay | 0-500 ms or synced | 20 ms | §1.3, inferred |
| Decay T60 | 0.1-60 s, ∞ = freeze | 2.5 s | Surge Reverb 2 goes to 64 s (§1.14) |
| LF / HF T60 multiplier | 0.25-4× / 0.1-1.5× | 1.2× / 0.5× | §2.2 |
| Matrix | Kronecker rotation θ 0-π/4; Householder as "Room" alternative | by `space` | §2.5 |
| Input diffusion | 0-0.75, 0-4 stages | by `space` | §2.3 |
| Modulation (reverb) | 0.05-2 Hz, 0-5 ms | 0.4 Hz, 0.6 ms | §2.6 |
| Wow / flutter | 0.3-2 Hz / 5-15 Hz, 0-3 % speed | 0.8 Hz / 9 Hz, 0.15 % | §2.7 |
| BBD bandwidth | 2-10 kHz, by delay time | auto | §2.8 |
| Shimmer | interval −12..+24 st or chord, amount 0-50 %, 1-4 lines | +12, 20 %, 2 lines | §2.11.2 |
| Loop ceiling | −12 to 0 dBFS | −6 dBFS | §2.12 |

**CPU estimate** (inferred; to be replaced by measurements from test S23). Per line per sample: cubic read
~8 ops, 3-band absorption ~10, gain/limiter/DC ~6, modulation ~3 ⇒ ~27 ops; matrix 48 mult + 24 add for
N = 8; ≈ 12.5 MFLOP/s at 48 kHz for the core. A modern core does a few GFLOP/s scalar, so the FDN core
alone is about 0.3-0.5 % of an Apple M4 core and roughly 2.5× that on the Ryzen (the M4/Ryzen ratio
measured for Murmur, 0.33 % vs 0.88 %). Add-ons: shimmer granular 2-4 heads × 2-4 lines (~0.2 %), spectral
freeze N = 4096 (~1 %), granular 32 grains in the loop (~0.6 %, scaled from Murmur's 128 grains = 2.55 %),
2× oversampled saturation on 8 lines (~0.5-1 %), spring allpass chains (~0.5 %). Expected: default preset
~0.5-1 % on the M4, heaviest preset ~4-6 %, within the S23 budget. Memory: 8 lines × 4.1 s × 192 kHz ×
4 bytes ≈ 25 MB at the highest rate (allocate per sample rate in `prepare`, never in the audio thread).

### 2.14 References

Read status: **[read]** = the page or PDF was opened on 2026-10-19 and the cited facts checked there;
**[abstract]** = only the abstract, a library record or a search snippet was available;
**[memory]** = citation from background knowledge, not re-checked on 2026-10-19 (verify before quoting).

| Tag | Reference | Status |
|---|---|---|
| [Stautner82] | J. Stautner, M. Puckette, "Designing multi-channel reverberators", Computer Music Journal 6(1), 1982 | [memory] |
| [Jot91] | J.-M. Jot, A. Chaigne, "Digital delay networks for designing artificial reverberators", AES 90th Convention, Paris, 1991, preprint 3030. Per-line gain g_i = 10^(−3m_i/(T60·fs)), absorption and tone-correction filters | [memory]; the gain rule is also given in JOS-PASP |
| [JOS-PASP] | J. O. Smith III, *Physical Audio Signal Processing*, CCRMA online book: "FDN Reverberation", "Choice of Delay Lengths", "Mode Density Requirement", "Choice of Lossless Feedback Matrix", "Hadamard Matrix". https://ccrma.stanford.edu/~jos/pasp/Choice_Delay_Lengths.html (mutually prime lengths, sufficient mode density); https://ccrma.stanford.edu/~jos/pasp/Mode_Density_Requirement.html (M ≥ 0.15·T60·fs after Schroeder and Logan; example: fs = 50 kHz, T60 = 1 s ⇒ M ≥ 7500); https://www.dsprelated.com/freebooks/pasp/Choice_Lossless_Feedback_Matrix.html (Hadamard has maximal determinant among matrices with entries bounded by 1; Householder for N a power of 2 needs no multiplies) | [read] |
| [Schroeder62] | M. R. Schroeder, "Natural sounding artificial reverberation", JAES 10(3), 1962. Comb and allpass sections, allpass gain about 0.7 | [memory] |
| [Moorer79] | J. A. Moorer, "About this reverberation business", Computer Music Journal 3(2), 1979. Low-passed combs, tapped-delay early reflections | [memory] |
| [Dattorro97] | J. Dattorro, "Effect Design Part 1: Reverberator and Other Filters", JAES 45(9), 660-684, 1997. https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf. Plate at 29761 Hz, input diffusion 0.75/0.625, decay diffusion 0.70/0.50, bandwidth 0.9995, excursion 16 samples, delay and tap tables | [read] first page; the parameter values are those implemented and verified in `shared/dsp/Fx.h` |
| [Dattorro97-part2] | J. Dattorro, "Effect Design Part 2: Delay-Line Modulation and Chorus", JAES 45(10), 1997 (interpolation for modulated delays) | [abstract] |
| [Griesinger89] | D. Griesinger, "Practical processors and programs for digital reverberation", AES 7th International Conference, 1989 | [memory] |
| [Schlecht17-lossless] | S. J. Schlecht, E. A. P. Habets, "On Lossless Feedback Delay Networks", IEEE Trans. Signal Processing 65(6), 1554-1564, 2017, doi 10.1109/TSP.2016.2637323; https://arxiv.org/pdf/1606.07729. An FDN is lossless for any delays if all irreducible components of the matrix are diagonally similar to a unitary matrix | [abstract] |
| [Schlecht15-timevarying] | S. J. Schlecht, E. A. P. Habets, "Time-varying feedback matrices in feedback delay networks and their application in artificial reverberation", JASA 138(3), 1389-1398, 2015, doi 10.1121/1.4928394 | [abstract] |
| [Schlecht-density] | S. J. Schlecht, E. A. P. Habets, "Feedback Delay Networks: Echo Density and Mixing Time", IEEE/ACM Trans. Audio, Speech and Language Processing, 2017 | [memory] |
| [Prawda-Schlecht] | K. Prawda, V. Välimäki, S. J. Schlecht, "Improved reverberation time control for feedback delay networks", DAFx 2019 (graphic-EQ absorption filters) | [memory] |
| [Valimaki12] | V. Välimäki, J. D. Parker, L. Savioja, J. O. Smith, J. S. Abel, "Fifty years of artificial reverberation", IEEE Trans. Audio, Speech and Language Processing 20(5), 2012 | [memory] |
| [Signalsmith21] | Signalsmith Audio (G. Pearce), "Let's write a reverb", 2021, https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/ (ADC 2021 talk). 8 channels; 4 diffusion steps whose delay ranges double (20, 40, 80, 160 ms in the example), each step: random delay per channel, shuffle and polarity flip, Hadamard mix; feedback loop of 100-200 ms delays with a Householder matrix ("some mixing without too much"); decay by a feedback gain (85 % in the example); optional modulation of some feedback delays, shelving filters for frequency-dependent decay (a −1.5 dB high shelf with −1.5 dB loop gain makes highs decay about twice as fast), early reflections from a tap of the diffuser. Author's comments (KVR, https://www.kvraudio.com/forum/viewtopic.php?p=8122342): a dense mix takes a few loops to build density, so pre-diffusion helps | [read] |
| [ValhallaBlog] | Valhalla DSP blog: "Reverbs: Diffusion, allpass delays, and metallic artifacts" (2011-01-21), https://valhalladsp.com/2011/01/21/reverbs-diffusion-allpass-delays-and-metallic-artifacts/ (allpass diffusion raises echo density but gives metallic artefacts on vocals and drums; Diffusion sets the allpass coefficients to suit the material); "ValhallaUberMod: The DIFF parameters" (2012-01-27), https://valhalladsp.com/2012/01/27/valhallaubermod-the-diff-parameters/ (modulation reduces metallic colouration in the diffusion section); "ValhallaPlate: On diffusion" (2015-11-18), https://valhalladsp.com/2015/11/18/valhallaplate-diffusion-and-presets/ (Size and Diffusion balance grain against metallic colouration); "ValhallaDelay: The Diffusion Section" (2019-06-13), https://valhalladsp.com/2019/06/13/valhalladelay-the-diffusion-section/ | [abstract] (the site refused direct fetches; content seen in search snippets) |
| [ValhallaShimmer] | Valhalla Shimmer design notes, https://www.valhalladsp.com/shimmer/ValhallaShimmerNotes.pdf (pitch shifter in the loop; cascaded modulated allpasses; pitch shifting left "un-deglitched" to avoid a metallic tone; reverse modes) | [abstract] |
| [Abel06] | J. S. Abel, D. P. Berners, S. Costello, J. O. Smith, "Spring reverb emulation using dispersive allpass filters in a waveguide structure", AES 121st Convention, 2006 | [memory] |
| [Välimäki10] | V. Välimäki, J. Parker, J. S. Abel, "Parametric spring reverberation effect", JAES 58(7/8), 547-562, 2010, https://aes2.org/publications/elibrary-page/?id=15511. A spectral delay filter (allpass chain) in a feedback loop with a long delay; delay time, chirp shape and decay adjustable | [abstract] |
| [Parker11] | J. Parker, "Efficient dispersion generation structures for spring reverb emulation", EURASIP J. Advances in Signal Processing, 2011, article 646134, https://link.springer.com/article/10.1155/2011/646134 (open access). Long allpass chains are costly; multirate/multiband versions cut the cost to about a third; "stretched" allpass | [abstract] |
| [Bilbao-Parker] | S. Bilbao, J. Parker, "A virtual model of spring reverberation", IEEE Trans. Audio, Speech and Language Processing, 2010 (finite-difference model) | [memory] |
| [Raffel10] | C. Raffel, J. O. Smith, "Practical modeling of bucket-brigade device circuits", DAFx 2010, https://dafx.de/paper-archive/details/JhVfAOFXD1lAtctkMTUODg | [abstract] |
| [Holters18] | M. Holters, J. D. Parker, "A combined model for a bucket brigade device and its input and output filters", DAFx 2018, Aveiro, https://dafx.de/paper-archive/details/KbFTgvcTMmHQ2bHZvOUekw. Variable-sample-rate model that uses the device's own input/output filters to avoid interpolation | [abstract] |
| [Arnardottir08] | S. Arnardottir, J. S. Abel, J. O. Smith, "A digital model of the Echoplex tape delay", AES 125th Convention, paper 7649, 2008, https://aes2.org/publications/elibrary-page/?id=14800. Read, write and erase pointers on a circular buffer; quasi-periodic capstan and pinch-wheel components plus drift, from measured delay trajectories; anti-alias filter with variable cutoff for the moving record head | [abstract] |
| [Chowdhury19] | J. Chowdhury, "Real-time physical modelling for analog tape machines", DAFx 2019, https://dafx.de/paper-archive/details/Z5Ow_E5YHVxvndTAyeEuzA; code (GPL-3) https://github.com/jatinchowdhury18/AnalogTapeModel. Hysteresis, playhead loss, wow/flutter, degradation (Sony TC-260) | [abstract]; head-bump frequency range is [memory] |
| [Bernsee99] | S. M. Bernsee, "Pitch shifting using the Fourier transform" (smbPitchShift), 1999, dspdimension.com | [memory] |
| [SignalsmithStretch] | Signalsmith Stretch, MIT-licensed pitch/time library, https://github.com/Signalsmith-Audio/signalsmith-stretch | [memory] |
| [Paulstretch] | N. O. Paul, Paulstretch (2006): STFT overlap-add with a Hann window, very large windows (seconds) and randomised phases instead of phase propagation. Code is GPL-2 (original C++) / GPL-3 (PaulXStretch), not public domain. https://csound.com/manual/opcodes/paulstretch/, https://github.com/essej/paulxstretch | [read] (Csound manual description) |
| [History-gated] | Gated reverb origin at Townhouse Studios (SSL listen-mic compressor plus channel gates, about 1979-80) and the AMS RMX16 "NonLin2" program that made it repeatable: https://reverb.com/news/get-the-in-the-air-tonight-reverb-and-193-more-from-the-townhouse-studios-rmx16, https://bobbyowsinskiblog.com/gated-reverb/ (dates and credits differ between accounts) | [abstract] |
| [Blevins-JND] | M. G. Blevins et al., on the JND of reverberation time: ISO 3382-1 lists 5 % (after Seraphim 1958), measured JNDs 6-39 %; https://digitalcommons.unl.edu/archengfacpub/63 | [abstract] |


---

## 3. Proposed engine and controls

Everything in this section is a design proposal (inferred from §1-§2), not a measured fact. Numbers are
starting values to tune with the tests in §5.

### 3.1 Signal flow

```
in L/R ─► [input trim, DC block] ─► [ducking + envelope follower (sidechain = dry input)]
       ─► [pre-delay / glitch capture]                                    (tempo grid)
       ─► [input diffuser: 0-4 allpass stages, coefficient set by Space]
       ─► ┌──────────────── FDN, N = 8 lines (4 in Eco mode) ─────────────────┐
          │  write → line i (length m_i, modulated read, fractional)          │
          │  read  → per-line absorption filter (3-band shelf, sets T60(f))   │
          │        → character insert (tape / BBD / digital)                  │
          │        → loop inserts (shimmer, reverse, granular, spectral, gate)│
          │        → feedback matrix A(θ) · P  (orthogonal for every θ)        │
          │        → loop safety limiter (energy-based, then per-line soft clip)
          └──────────────────────────────────────────────────────────────────┘
       ─► [output taps: per-line gains and pans → L/R; early "tap" outputs for multitap]
       ─► [post: tone tilt, resonator bank (optional), width]
       ─► [wet duck gain] ─► [dry/wet mix] ─► [output safety ceiling] ─► out
```

Why this order (inferred):
- **Ducking before the network acts on the input to the tail** in one mode ("duck the feed") and on the
  wet output in the other ("duck the tail"). Both are useful: the first keeps old tails ringing while new
  notes are dry, the second is the classic EchoBoy/Echo-style duck.
- **Diffuser before the loop**: Dattorro (§2.3) and Signalsmith (§2.4) both put allpass diffusion ahead of
  the recirculating part so the tail starts dense. Setting the diffuser coefficient to zero leaves a clean
  delay, which is exactly what the Delay end of the morph needs.
- **Loop inserts after the absorption filter and before the matrix**: whatever a loop insert does (pitch,
  reverse, grains) is then mixed into every line on the next pass, which is what makes shimmer "climb" and
  granular smear spread. The safety limiter is the last thing before the write, so no insert can push the
  loop past the ceiling.
- **Resonator after the network** by default (cheap, predictable). An "in loop" option tunes the line
  lengths themselves (§2.11.6) for the Tuned Abyss style.

### 3.2 The Delay ↔ Space control

One macro, `space` ∈ [0, 1], drives five internal quantities along curves chosen so that every point is
musically usable. This is the core idea of the product and the main way it goes beyond Ableton, where
Echo, Delay and the reverbs are separate devices with separate vocabularies.

| `space` | Matrix angle θ (§2.5) | Input diffusion g_d | Line lengths | Time control means | Modulation |
|---|---|---|---|---|---|
| 0.00 | 0 (identity or ping-pong permutation) | 0 | exactly on the tempo grid / pattern | echo time | character only (tape wow/flutter, BBD clock jitter) |
| 0.25 | 0.10 rad (3 % of energy leaks per pass) | 0.25 | grid ± 2 % spread | echo time | + 0.5 ms chorus on the lines |
| 0.50 | 0.30 rad (24 % leaks) | 0.50 | grid ± 8 % spread (each echo a small cluster) | echo time | + 1-2 ms |
| 0.75 | 0.55 rad (62 % leaks) | 0.62 | blend toward the room set scaled by Size | pre-delay + early pattern | 2-4 ms, slow |
| 1.00 | π/4 (full Hadamard, 1/N stays) | 0.70-0.75 | room set (mutually prime, §2.2) | pre-delay | reverb wander (§2.6) |

(The leak figures are computed, not guessed: for N = 8 each diagonal entry of the Kronecker-rotation matrix
is cos³θ, so the energy a line keeps per pass is cos⁶θ: 0.970 at θ = 0.10, 0.760 at 0.30, 0.384 at 0.55 and
0.125 = 1/N at π/4. The rest of the table is a proposal.)

A single **Decay** control works across the whole range because feedback gain and T60 are the same thing
seen from two sides: for a loop of length m samples with gain g, T60 = −3·m / (fs·log₁₀ g). At the Delay
end the UI shows Decay as "≈ 12 repeats"; at the Space end as "3.4 s". Internally the engine always works
in T60 and derives per-line gains, so moving `space` does not change how long the sound lasts, only how
it is spread. That is what makes the morph feel like one effect and not a crossfade between two.

Moving `space` changes line lengths, so it creates pitch glides while sound is in the loop. Two
behaviours, chosen by the **Time feel** switch (shared with tempo changes, §2.11.7):
- **Tape** (default): lengths slew with a 1-pole of 150-600 ms, the read pointer moves, pitch glides
  (the pitch sweep of tape echoes and of Ableton Delay's Repitch, §1.2, §1.12).
- **Clean**: each line has two read heads; a new length is jumped to by a 30-80 ms equal-power crossfade
  (what Ableton's Delay calls Fade, §1.2). No pitch change, a brief smear.

### 3.3 Characters

The character picker sets the loop inserts, the absorption shape, the default modulation and the output
tap layout. It never changes the `space` mapping, so a "Tape" character pushed to Space = 1 becomes a
tape-saturated reverb, and a "Hall" character pulled to Space = 0 becomes a clean, slightly diffused
delay. Proposed set (12, the same count as BigSky's machines; each one a starting point, not a separate algorithm):

| Character | Default `space` | Loop insert | Absorption / tone | Modulation |
|---|---|---|---|---|
| Tape | 0.05 | wow + flutter, soft saturation (2× OS), head bump +2 dB at 80-120 Hz, HF loss rising with Wear | HF roll-off per pass 5-8 kHz | wow 0.5-1.5 Hz, flutter 6-12 Hz (§2.7) |
| Dark BBD | 0.05 | anti-alias LPF that follows the clock, compander breath, clock whine (optional), dark and grainy | LPF 2.5-5 kHz depending on time | clock jitter, chorus 0.3-0.8 Hz |
| Digital | 0.00 | none (pure) | flat unless Tone moves | off |
| Ping-Pong | 0.00 | permutation P swaps L/R lines | flat | off |
| Pattern | 0.00 | per-line lengths = pattern taps, identity matrix | flat | off |
| Plate | 0.95 | Dattorro-style dense diffusion, bright | high-frequency damping low | 1 Hz, ±16 samples at 29.8 kHz scaled |
| Room | 0.90 | short set (small/room lengths), early reflections strong | moderate HF loss | slow, shallow |
| Hall | 1.00 | long set, slow build | LF T60 × 1.3, HF T60 × 0.5 | slow wander |
| Spring | 0.85 | dispersive allpass chirp in 2-3 lines, "drip" on transients | band-limited 100 Hz-5 kHz | none (spring "boing" is the modulation) |
| Shimmer | 0.95 | pitch shifter on 2-4 lines (+12 default) | LPF after shift, HPF 200 Hz in loop | slow wander |
| Infinite | 1.00 | none; Decay maximum maps to freeze | flat | slow wander, lossless |
| Gated | 0.95 | output envelope gate (hold 50-400 ms, fast release) or nonlinear decay shape | bright | none |

### 3.4 Controls

**Simple page** (what most users see; 8 controls and one button):

| Control | Range | Notes |
|---|---|---|
| Character | 12 choices | §3.3 |
| Delay ↔ Space | 0-100 % | the morph, §3.2 |
| Time | 1/64 to 4 bars synced, or 1 ms-4 s free; at Space end shown as pre-delay 0-500 ms | tap-tempo button next to it |
| Size | 0-100 % | room scale at the Space end, echo spread at the Delay end |
| Decay | 0.1 s to 60 s, then ∞ | displayed as repeats or seconds, §3.2 |
| Tone | dark ↔ bright, centre = flat | moves the absorption filter (frequency-dependent T60) and a post tilt |
| Motion | 0-100 % | one knob for modulation depth; its colour shows what it is modulating |
| Mix | 0-100 % (with a "100 % wet" lock for send use) | equal-power |
| **Freeze** | button (and sustain pedal CC 64) | holds the current tail, §2.11.1 |

**Advanced page**, grouped in panels so names explain themselves:
- *Echoes*: pattern editor (up to 8 taps on the tempo grid, level/pan per tap), ping-pong amount, feedback
  per tap, swing.
- *Room*: diffusion override, density (N = 4 / 8 / 16 lines), early reflections level, LF / mid / HF
  decay multipliers and crossovers, width, pre-delay.
- *Colour*: tape (wow, flutter, wear, saturation, age), BBD (clock noise, compander, bandwidth),
  low cut / high cut in the loop.
- *Shimmer*: interval (−12, −5, +5, +7, +12, +19, +24, or chord: major, minor, sus, fifths stack),
  amount, number of lines shifted, pitch method (grains / spectral), reverse shimmer.
- *Freeze & Spectral*: freeze mode (full / low / mid / high band), spectral freeze, smear amount,
  infinite-room hold.
- *Movement*: two LFOs (reuse Murmur's: sine, triangle, ramp, drift, step; free or synced), one envelope
  follower (attack, release, amount), each with up to 2 targets from a short list (Space, Size, Decay,
  Tone, Mix, Shimmer, Grain density, Freeze threshold).
- *Ducking*: amount, attack, release, mode (tail / feed).
- *Glitch*: stutter length (1/32-1/2), chance, repeats, reverse chance, on the host grid.
- *Resonator*: off / post / in loop, note or chord from MIDI or a note picker, decay, brightness.
- *Scenes*: A, B, morph slider (and MIDI CC learn), copy A→B.
- *Safety*: limiter ceiling (−12 to 0 dBFS, default −3), "allow self-oscillation" toggle (off by default;
  when on, the limiter becomes the sound, as on a real tape echo pushed past unity).

### 3.5 How the UI teaches the concepts (inferred)

Requirements from the brief: very easy, but exposing advanced concepts, for ambient, music-therapy and
meditative listening audiences.
- **One sentence per control** in the help line at the bottom, as in the other Audio Tools plugins
  (`ParamsMm.h` keeps help text with each parameter); plain language: "Space: from separate echoes to one
  continuous room".
- **The 3D room (§4) is the explanation**: turning Space visibly turns separate bouncing lights into a
  cloud; Decay shows as how long lights live; Freeze makes them stop and shimmer.
- **Musical numbers, not engineering numbers**, on the Simple page (repeats, seconds, note values,
  intervals by name). Engineering units appear on hover and on the Advanced page.
- **No surprise loudness**: the safety limiter is on by default and shown as a small headroom bar; the
  self-oscillation warning appears before it happens (§4.5).
- **Calm defaults**: smooth parameter changes, no flashing in the visualization above 3 per second (WCAG
  2.3.1, §4.6), a "still" view option, and a low-motion mode.
- **No medical claims** anywhere in UI, manual or presets. Preset descriptions talk about sound ("slow,
  dark, wide, very long tail"), never about effects on the listener.


---

## 4. Visualization: the room of light

Ableton's Echo already draws each repeat as a ring in its "Echo Tunnel" (manual §29.14.1, see §1.1), and
Murmur draws grains as birds. Here the picture should explain the one idea the product sells: echoes
that turn into space. Everything in this section is a proposal (inferred) unless a source is given.

### 4.1 The mapping that makes the view honest

The 8-line FDN with the Kronecker-rotation matrix of §2.5 has a geometric reading that is exact, not a
metaphor: the matrix is R(θ) ⊗ R(θ) ⊗ R(θ), one 2×2 rotation per axis of a cube. Index the eight lines by
three bits (x, y, z) and put each line at a corner of a cube-shaped room. Then:
- at θ = 0 (Delay end) energy never leaves its corner: each echo is a light that bounces in place, and
  ping-pong (permutation P) is a light jumping between the left and right faces;
- as θ grows, each pass sends a fraction sin²θ of each line's energy along each axis to the neighbouring
  corner, so the lights travel along the cube's edges and the room fills;
- at θ = π/4 a pass spreads energy equally over all eight corners: the room glows evenly.

With N = 4 (Eco mode) the room is a square (two axes); with N = 16 a tesseract is not drawable, so the
view keeps the cube and maps two lines per corner.

### 4.2 Particles

Each particle is one echo (Delay end) or one parcel of tail energy (Space end).

| Property | Source in the DSP | Visual |
|---|---|---|
| Birth | onset in the input (§4.3), or a periodic emitter fed by per-line energy | a flash at the "source" position (centre of the room, slightly forward) |
| Position | the line it is in (cube corner), interpolated along an edge while it "travels" between lines in one pass | 3D point, perspective projection |
| Size | level in dB: radius ∝ 10^(L/40), clamped; L = per-line RMS over the last view frame | big = loud |
| Colour | spectral centroid of the line's signal (cheap estimate, §4.3), mapped low → warm amber, mid → white, high → cool blue | perceptual, colour-blind-safe ramp (no red/green opposition) |
| Brightness / alpha | the same level, with a floor so faint echoes stay visible down to −60 dB | fade = decay |
| Lifetime | until its level crosses −60 dB relative to its birth (that is T60, visibly) | Decay is "how long the lights live" |
| Trail | shimmer up = particles rise slightly per pass; reverse = trail points backward; granular = particle splits into sparks | each loop insert has one gesture |
| Frozen | freeze on: particles stop fading and twinkle slowly (≤ 1 Hz), positions drift by ≤ 2 % of the room | "held" is visible |

At the Delay end the echo times are known analytically, so the view does not need to measure them: with
identity matrix, echo k of line i arrives at k·m_i with level g_iᵏ times the tap gain. The view can draw
the next echoes before they sound (ghosted), which makes the tempo grid indicator (§4.4) exact. At the
Space end the echoes are too dense to draw one by one (echo density grows like Nᵏ per pass, §2.2), so
particles become a cloud whose emission rate per corner is proportional to that line's energy.

### 4.3 Deriving the data cheaply from the DSP

All of this runs in the audio thread and costs a few operations per line per sample (inferred estimates):
- **Per-line energy**: accumulate y_i² over the block, publish RMS once per view frame (view runs at
  30-40 Hz like Murmur's `FlockView`, 40 Hz). 8 multiply-adds per sample.
- **Per-line spectral centroid estimate**: run a one-pole high-pass (~1.5 kHz) on each line output and
  accumulate its energy too; brightness b_i = E_hp / E_all ∈ [0, 1] maps monotonically to centroid for
  typical material. This is a "brightness ratio" proxy, not a true centroid; a true centroid would need an
  FFT per line (avoid). For the whole wet output (one signal) a 256-point FFT per frame for a true centroid
  is affordable if wanted.
- **Onset tagging**: on the dry input, a fast (1 ms attack, 10 ms release) and a slow (50 ms / 300 ms)
  envelope; onset when fast/slow > 1.5 (+3.5 dB) and fast > −50 dBFS, with 50 ms refractory time. Each
  onset gets an id, timestamp (sample position), level and brightness. This is the same envelope pair the
  ducker and the envelope follower need, so it is free.
- **Echo bookkeeping (Delay end)**: for each onset, the view computes future echoes from the current line
  lengths, gains and pattern. No per-echo state in the audio thread.
- **Tail level and decay**: wet output level in dB (RMS, 50 ms window) appended to a 10 s history.
- **Loop headroom**: the loop-gain estimate and the limiter's gain reduction (§2.12).
- **Transport to the UI**: a lock-free single-producer single-consumer FIFO of small structs (onset
  events) plus an atomic snapshot of per-line energy/brightness with a serial number, exactly the pattern
  Murmur's `publishView()` already uses (`std::atomic<int> viewSerial`).

### 4.4 Indicators around the room

- **Tempo grid strip** (under the room): one bar of the host grid with beat and sub-division lines; each
  delay tap is a dot at its time with height = level; the next echoes of the last onset are drawn as
  ghosts. At the Space end the strip shows pre-delay and the early pattern, and a soft gradient where the
  tail becomes continuous.
- **Decay graph** (side panel): wet level in dB against time, the last 10 s scrolling; the set T60 drawn
  as a straight line of slope −60 dB / T60 from the last onset; the measured slope (least-squares fit
  over −5 to −35 dB of the last decay, as in a T30 measurement) shown as a number when a clean decay is
  available ("set 4.0 s · measured 3.8 s"). This doubles as a live check of the absorption calibration.
- **Freeze indicator**: the graph line becomes flat and the label says "held"; for per-band freeze, the
  graph shows three thin lines (low / mid / high).
- **Headroom bar** (top right, small): loop gain from 0 to 1. Green below 0.9, amber 0.9-0.98 ("long tail"),
  red at ≥ 0.98 ("will build up"), plus a limiter gain-reduction sliver. The warning comes from the
  computed loop gain, so it appears *before* self-oscillation, not after.

### 4.5 Predicting self-oscillation before it happens

The loop is lossless when the matrix is orthogonal and every line gain is 1. So the worst-case loop gain
per pass is max over lines and frequencies of |g_i · H_abs,i(f) · H_insert,i(f)|, times the feedback
setting (inferred). Each factor is known from the parameters: the absorption shelves' peak gain, the
character's head bump (+2 dB in Tape is a real gain above 1 at 80-120 Hz), the shimmer's level compensation,
the resonator Q. The engine computes this number when parameters change (not per sample) and publishes it.
A second, measured guard watches the actual wet level while the input is silent: if it rises more than 1 dB
over 500 ms, the bar turns red even if the computed number said otherwise (catches unmodelled cases such as
interactions between modulation and the resonator). Both feed the same indicator.

### 4.6 Rendering and comfort

- JUCE `Graphics` 2D with a software perspective projection is enough for ~300-600 particles at 40 Hz
  (Murmur draws 128 birds this way; no plugin in the repo uses OpenGL yet). An OpenGL path is optional
  later; avoid making it a requirement (hosts and Windows drivers vary).
- Slow auto-rotation of the cube (one turn per 60-120 s) or user drag; "still" option.
- **Photosensitivity**: no full-field flashing; at most 3 flashes per second (WCAG 2.1 success criterion
  2.3.1, "Three Flashes or Below Threshold", https://www.w3.org/WAI/WCAG21/Understanding/three-flashes-or-below-threshold,
  read 2026-10-19). Onset flashes are small and local; glitch/stutter presets must not strobe the view.
  This matters more than usual for the meditative/psychedelic audience.
- Reduced-motion mode follows the OS setting where JUCE exposes it, else a toggle.
- The website video can reuse the Blender pipeline already used for Murmur and Lacquer
  (`tools/blender/*.py`, pc50-wsl), driven by a JSON dump of the same particle data from the offline
  renderer, so the film shows what the plugin really did.


---

## 5. Tests that prove it works

Automated in `tools/dsptest.cpp` (JUCE-free, same pattern as Murmur and Lacquer), plus host checks with
pluginval, auval and `tools/hosttest`. Criteria are proposals (inferred); tighten them once the engine
exists. "All characters" means every character of §3.3 at Space 0, 0.5 and 1, and every factory preset.

| # | Test | Method | Pass criterion |
|---|---|---|---|
| S1 | Finite output | white noise, full-scale square, DC, silence, single-sample impulses at 0 dBFS, and 200 random extreme parameter settings (seeded), 30 s each, with parameter automation every 64 samples | no NaN/Inf ever; peak ≤ limiter ceiling + 0.1 dB with "allow self-oscillation" off |
| S2 | Exact latency | impulse through each mode with Mix 0 % and 100 %; for the dry path cross-correlate in/out | reported latency equals measured to the sample, in every mode and sample rate; 0 samples except the lookahead reverse mode, which reports its window exactly; latency never changes without the host being told |
| S3 | Determinism across block sizes | same input and automation, block sizes 1, 32, 64, 480, 512, 4096 and random 1-4096; same seed | bit-identical output (all randomness from seeded `Rng`, parameter smoothing per sample or per fixed internal sub-block, STFT stages on an internal fixed hop) |
| S4 | Determinism across runs and reset | render twice; render, `reset()`, render | bit-identical; reset leaves no tail |
| S5 | Decay time: measured vs set | impulse; Schroeder backward integration of the energy decay curve; T30 fit (−5 to −35 dB), per octave band 125 Hz-8 kHz | broadband T30 within ±10 % of set T60 for 0.3-20 s at Space = 1 (ISO 3382-1 lists 5 % as the just-noticeable difference of reverberation time, after Seraphim 1958, while listening studies found 6-39 % [Blevins-JND]; 10 % is the first gate, 5 % the goal); LF/HF multipliers produce band T60s within ±15 % of target |
| S6 | Decay at the Delay end | impulse, Digital character, Space = 0, Decay set to N repeats | the k-th echo's level = g^k within ±0.1 dB for k ≤ 10; echo times exact to the sample on the tempo grid (120 BPM 1/8 = 12000 samples at 48 kHz) |
| S7 | Freeze holds without growth | 2 s of pink noise, freeze, input continues (must be ignored) for 60 s | level of each 1 s window within ±1 dB of the first frozen second over 60 s (full freeze); never rises > 0.5 dB above it; no new input energy (correlate with a marker tone added after freeze: below −80 dB) |
| S8 | Per-band freeze | freeze the low band only, then stop input | low band (< crossover) level within ±1 dB over 30 s; mid and high bands decay with their set T60 (±15 %) |
| S9 | Spectral freeze | sine + noise; capture; 60 s | magnitude spectrum of the held sound within ±1.5 dB per 1/3 octave of the captured frame, no growth |
| S10 | Feedback never exceeds the ceiling | Decay = ∞, shimmer +12 on 4 lines, Tape character with head bump, resonator Q max, input pink noise at 0 dBFS for 60 s | per-line loop amplitude ≤ loop ceiling at every sample; output ≤ output ceiling + 0.1 dB; with "allow self-oscillation" on: bounded and stable (no runaway, peak constant within 1 dB after 10 s) |
| S11 | Silence after the input stops | each character, Decay 5 s, input stops | output below −120 dBFS within (Decay × 2.5 + 1 s), then exactly 0 or denormal-free tiny values; Echo-style auto-sleep must not cut a tail above −90 dBFS |
| S12 | No denormals | feed 1e-30-level noise for 10 s, then silence for 60 s; count subnormal floats in every state variable; time each block | zero subnormals (flush-to-zero set + DC-offset trick or explicit flush in filters); no CPU spike > 1.5× steady state during the silent tail |
| S13 | Matrix losslessness | for every θ on a 100-step grid and every N: ‖AᵀA − I‖∞ | < 1e-6 (float) |
| S14 | Morph continuity | sweep Space 0 → 1 → 0 over 10 s on a sustained input, both Time feel modes | no sample-to-sample step > 6 dB in the wet RMS (10 ms windows); Clean mode: no pitch deviation > 5 cents (pitch tracker on a sine input); Tape mode: glide monotonic |
| S15 | Shimmer interval accuracy | 220 Hz sine, shimmer +12 / +7 / +19 / chord | strongest new partials at 440 / 329.6 / 659.3 Hz within ±3 cents (zero crossings or FFT peak interpolation); no partials > −40 dB below the target at non-musical ratios (metallic check) |
| S16 | Resonator tuning | white noise into Tuned mode at A2 (110 Hz), and a C major chord | tail spectral peaks at the chord partials within ±5 cents |
| S17 | Tempo sync | 120 BPM and 97.3 BPM, 1/8 dotted, host tempo changes mid-render | echo onsets within ±1 sample of the grid; after a tempo change, Clean mode lands on the new grid within 80 ms |
| S18 | Ducking | 1 kHz bursts over a long tail | wet attenuation reaches set depth within attack ± 20 %, recovers within release ± 20 % |
| S19 | Reverse | impulse into reverse mode with window W | the reversed swell peaks at W (± 1 ms) after the impulse; crossfades leave no click (no step > −40 dB relative) |
| S20 | Sample-rate independence | 44.1 / 48 / 88.2 / 96 / 192 kHz | T30 within ±3 % across rates; echo times within 1 sample; tone (1/3-octave LTAS of the tail, loudness-normalised) within 0.5 dB below 16 kHz |
| S21 | State and automation | save/restore every preset; automate every parameter with ramps and jumps | restored state reproduces the render bit for bit from reset; no click above −50 dB on jumps (parameter smoothing) |
| S22 | Scene morph | A = Tape delay, B = Hall; morph 0 → 1 | every intermediate point passes S1, S10, S13 |
| S23 | CPU budget | `render --bench`, 48 kHz, 512 block, Apple M4 and Ryzen 9 5900XT (the machines in the other READMEs) | default preset < 1.5 % / 3 % of one core; heaviest factory preset (granular + shimmer + spectral) < 8 % / 15 %; N = 16 Hall alone < 2 % / 4 % |
| S24 | Mono compatibility | Space = 1, mono fold-down | (L+R)/2 loudness within 3 dB of stereo; no audible comb (correlation of L and R tail not < −0.3 over 1 s) |
| S25 | Host validation | pluginval strictness 8, auval, athosttest | pass on macOS (VST3, AU) and Windows (VST3), as for Murmur |

Subjective checks (not automated): metallic ringing on a snare and a hand clap at Space = 1 for every
reverb character (listen for flutter echo and periodic ringing at 50-200 ms, the classic sign of too few
or too commensurate delay lines); a 20-minute "long listen" of the Endless Pad and Infinite presets for
slow build-up or periodic modulation patterns; loudness-matched A/B against the stock Ableton devices on
the same material (§1).


---

## 6. Name

Criteria (inferred from the existing line-up Murmur, Orrery, Lacquer): one evocative English word, easy to
say in English and Spanish, about the image rather than the technique, not a trademark of a large
company, and not used by another audio plugin, pedal, module or app. The searches below were made on
2026-10-19 on the KVR product database, ModularGrid, GitHub, Reverb.com listings and the general web.
"Clear" means nothing was found, not proof that nothing exists; a USPTO/EUIPO trademark search on the
finalist is still needed before release (not legal advice).

### 6.1 The two candidates from the brief

| Name | Collision | Verdict |
|---|---|---|
| Echo Chamber | Syntheway "Echo Chamber Reverb and Delay" plugin (https://www.kvraudio.com/product/echo-chamber-reverb-and-delay-by-syntheway-virtual-musical-instruments); also the generic name of a physical reverb room and of a political metaphor | taken, and generic: avoid |
| Hall of Mirrors | Poison Noises "Hall of Mirrors" reverb pedal (https://www.pedal-of-the-day.com/2022/08/05/poison-noises-hall-of-mirrors-reverb-v2/) | taken in the same category: avoid |

### 6.2 Alternatives

| Name | Meaning / fit | Collisions found | Verdict |
|---|---|---|---|
| **Motes** | dust motes floating in a beam of light: each echo a particle in a lit room | none in audio; "Mote" (singular) is a voice-notes app (https://apps.apple.com/app/id1568351949), so keep the plural | clear |
| **Halation** | the glow that bleeds around a bright light on film: echoes smearing into space | none in audio; FilmConvert sells a video plug-in called Halation (https://www.newsshooter.com/2024/11/24/filmconvert-hazy-diffusion-plugin/), and it is a common film-look term | clear in audio |
| **Lightwell** | a shaft that brings daylight into the middle of a building: light entering a room | none in audio; Red Hat/IBM use "Lightwell" for an unrelated product in 2026 (https://www.redhat.com/en/lightwell), different class | clear in audio |
| Clerestory | the high windows that light a nave: light entering a large space | none found | clear, but hard to spell and pronounce, especially in Spanish |
| Luciole | French for firefly: points of light, sidesteps Adobe Firefly and Firefly Synth | none found | clear, but French |
| Dustlight | coined: light made visible by dust | none found | clear, easiest to own, slightly less evocative |
| Cloister | an enclosed, quiet architectural space | only a preset name inside another synth | clear; about the room more than the light |
| Gloaming | twilight | a small GitHub web-audio library "gloaming-instruments" (https://github.com/diatominstruments/gloaming-instruments); Pigtronix "Gloamer" pedal is close | minor collision |
| Iridis | rainbow-like (iridescence) | none; Strymon "Iridium" is close in sound | clear but near a pedal-maker product |
| Apse | the vaulted end of a church | none found | clear but cold |

Rejected after search (already used for a reverb, delay or synth): Lumina (Mountainroad DSP Lumina Delay),
Afterglow (TC Electronic pedal), Aurora (iZotope reverb, Qu-Bit module), Halo (Antelope, Keeley), Nimbus
(Exponential Audio/iZotope), Bloom, Glow, Prism, Atrium, Parallax, Reverie, Vesper (a delay+reverb
released in October 2026), Ember, Nave, Phosphor, Spectra, Corona, Lanternfish; trademarks of large
companies: Oculus, Lightroom, Firefly. Examples of the collisions (all consulted 2026-10-19):
https://www.kvraudio.com/product/lumina-delay-by-mountainroad-dsp,
https://www.kvraudio.com/product/aurora-by-izotope, https://www.kvraudio.com/product/atrium-by-convex-audio,
https://www.kvraudio.com/product/glow-by-wave-alchemy, https://www.kvraudio.com/product/bloom-by-azalea-studios,
https://www.kvraudio.com/product/reverie-by-quiet-music, https://www.kvraudio.com/product/halo-by-antelope-audio,
https://thebeatcommunity.com/2026/10/08/mike-gazzaruso-releases-vesper-multieffect-for-desktop-ios/ (Vesper),
https://reverb.com/p/tc-electronic-afterglow-chorus-pedal (Afterglow).

Closest existing product to the concept (not a name clash): Umbrella Company's HUME pedal, whose slider
sweeps from defined delay repeats to a reverb wash, with Time setting delay time and room size together,
a reverse switch and a morph between current and saved settings
(https://delicious-audio.com/umbrella-company-hume/, https://www.perfectcircuit.com/umbrella-company-hume.html).
Worth a look before release, so the manual can say honestly what is different (an FDN with a visible
room, per-band freeze, interval shimmer, in-loop granular and spectral stages, open source).

### 6.3 Recommendation

1. **Motes**: the most literal fit for the particle view and the delay that becomes a lit room; short,
   pronounceable in both languages ("Motes" reads naturally in Spanish too), plural noun like the idea
   (many echoes). Pairs well with Murmur (both are "many small things moving").
2. **Halation**: the best fit for the *sound* (the smear of a delay into a glow), and unclaimed in audio.
3. **Lightwell**: the best fit for "light coming into a room"; slightly more architectural.

"Echo Chamber" and "Hall of Mirrors" could still work as preset names (they are descriptive phrases), but
not as the product name.


---

## 7. Factory presets

32 presets in four categories (8 each). One line each on the settings idea. All are proposals (inferred);
tempo values assume the host tempo. Names in **bold** are the ones required by the brief. "S" = the
Delay ↔ Space value, "D" = Decay.

### 7.1 Classic

| # | Name | Character | Settings idea |
|---|---|---|---|
| 1 | **Tape Echo 1974** | Tape | S 0.05, 1/8 dotted, D ≈ 5 repeats, wow 0.8 Hz light, flutter 9 Hz light, Wear 40 %, saturation on, head bump on: the warm single-head echo of the era. |
| 2 | **Plate Vocal** | Plate | S 0.95, pre-delay 1/32 (≈ 60 ms at 120), D 1.8 s, HF decay × 0.8, low cut 200 Hz in the loop, ducking 30 % on the tail so words stay clear. |
| 3 | **Spring Surf** | Spring | S 0.85, 3 springs with slightly different chirp lengths, drip on, D 2.5 s, band 120 Hz-4.5 kHz, a touch of drive before the tank. |
| 4 | Dark Bucket | Dark BBD | S 0.05, 300 ms free, D ≈ 8 repeats, bandwidth 3 kHz, compander breath on, clock noise low, slow chorus. |
| 5 | Clean Quarter | Digital | S 0.0, 1/4, D ≈ 4 repeats, flat; the reference delay. |
| 6 | Small Room | Room | S 0.90, Size 25 %, D 0.6 s, strong early reflections; for drums and acoustic guitar. |
| 7 | Concert Hall | Hall | S 1.0, Size 70 %, pre-delay 25 ms, D 2.8 s, LF × 1.3, HF × 0.55, gentle wander. |
| 8 | Slapback 57 | Tape | S 0.0, 110 ms free, one repeat (D minimal), tape saturation, mono, the rockabilly slap. |

### 7.2 Ambient

| # | Name | Character | Settings idea |
|---|---|---|---|
| 9 | **Cathedral of Glass** | Shimmer | S 1.0, Size 95 %, D 12 s, shimmer +12 on 2 of 8 lines at 25 %, HF decay × 0.9, very slow wander, Motion 30 %. |
| 10 | **Endless Pad** | Infinite | S 1.0, D ∞ (lossless), Freeze In on: new notes add to the held pad without growth (limiter + energy normalisation), Tone slightly dark. |
| 11 | **Breathing Room** | Hall | S 0.95, Size breathes ± 12 % with a 0.08 Hz sine (Tape time feel, so the room's pitch sways a little), D 6 s, envelope follower opens Mix on loud passages. |
| 12 | Slow Fog | Hall | S 1.0, input diffusion max, D 9 s, LP in the loop 3 kHz, Motion 50 %: a soft, out-of-focus wash. |
| 13 | Morning Haze | BBD → Space | S 0.70, BBD character as a reverb: dark, grainy, compander breath, 1/4 note pre-delay, D 7 s. |
| 14 | Distant Shore | Tape | S 0.60, 1/2 note, D 10 s, Wear 70 %, wow deep and slow (0.3 Hz): echoes melting into a tape-soaked space. |
| 15 | Fifths in the Air | Shimmer | S 1.0, shimmer +7 on 2 lines and +12 on 2 lines at 20 %, LP after shift 6 kHz, D 15 s: a hovering open-fifth halo. |
| 16 | Night Garden | Room | S 0.85, small Size, D 4 s, granular in the loop (20 grains/s, 80 ms, ±3 % pitch spray): a tail that sparkles quietly. |

### 7.3 Rhythmic

| # | Name | Character | Settings idea |
|---|---|---|---|
| 17 | **Dotted Rain** | Pattern | S 0.15, taps on 1/8 dotted, 1/4, 3/8 and 1/2 with falling levels, slight space smear on each tap, HP 300 Hz, D 3 s. |
| 18 | **Ping-Pong Machine** | Ping-Pong | S 0.0, 1/8 left and 1/4 right, full width, D ≈ 10 repeats, a 1/16 swing offset, BBD colour off. |
| 19 | **Stutter Grid** | Digital | Glitch: 1/16 captures with 40 % chance per beat, 2-8 repeats, reverse chance 20 %, repeats fed into a short room (S 0.5). |
| 20 | Triplet Steps | Pattern | S 0.0, taps on 1/8T, 1/4T, 1/2T panned L-C-R, D ≈ 6 repeats, tape colour light. |
| 21 | Dub Siren Send | Tape | S 0.10, 3/16, D near unity with the limiter as the sound ("allow self-oscillation" off but D 95 %), Tone moves a resonant LP in the loop; made to be ridden by hand. |
| 22 | Pulse to Cloud | Pattern → Space | S automated by the envelope follower: loud hits are clean taps, quiet passages spread into a room. |
| 23 | Clockwork Room | Room | S 0.55, lengths quantised to 1/32 multiples so the early reflections fall on the grid, D 1.5 s: a reverb that grooves. |
| 24 | Ducked Quarter | Digital | S 0.0, 1/4, D ≈ 6 repeats, ducking 80 % (tail mode) attack 10 ms release 300 ms: echoes only in the gaps. |

### 7.4 Experimental

| # | Name | Character | Settings idea |
|---|---|---|---|
| 25 | **Ghost Before You** | Reverse | Lookahead reverse reverb: Hall tail captured and played backward, window 1 bar, latency reported so that in playback the swell arrives before each note (live input: it follows). |
| 26 | **Tuned Abyss** | Hall + Resonator in loop | Line lengths tuned to the held MIDI note (default A1) and its fifth, D 20 s, dark: every input sinks into a drone at that pitch. |
| 27 | **Broken Satellite** | BBD | Glitch + BBD clock noise up, aliasing on (clock low), random time jumps (Clean feel off, Tape feel with fast slew), shimmer −12 at 15 %: a failing transmission. |
| 28 | **Spectral Ice** | Infinite | Spectral freeze on onsets (retrigger), smear 70 %, phase randomisation on, crossfade 2 s, Tone bright, small shimmer +24: frozen glassy spectra. |
| 29 | **Rising Choir** | Shimmer | Chord shimmer (major: +4, +7, +12 across 6 lines) at 30 %, LP after shift, formant-friendly grain size 60-90 ms, D 18 s: the tail climbs into a chord. |
| 30 | **Collapsing Hall** | Hall | Impossible room: on each onset Size shrinks from 100 % to 15 % over 4 s while D is held (Tape time feel, so the pitch rises as walls close in), then resets. |
| 31 | **Self-Oscillation** | Tape | "Allow self-oscillation" on, D past unity (loop gain 1.03), limiter ceiling −6 dBFS, Tone dark; the loop sings on its own and Time becomes a pitch control. Clearly labelled; starts quiet. |
| 32 | Reverse Shimmer | Reverse + Shimmer | Reverse in the loop (window 1/2 note) and shimmer +12: each pass comes back backward and an octave higher, a rising, inhaled tail. |

Notes (inferred): every preset is rendered through S1, S10, S11 and S23 in CI; Self-Oscillation is the
only preset allowed to sound without input, and it must never exceed its ceiling. Preset descriptions
describe sound only (no claims about wellbeing or effects on the listener).


---

## 8. What this repo already has, and the layout

Read for this section: `shared/dsp/Fx.h` (254 lines), `shared/dsp/Common.h`, `shared/dsp/Oversampling.h`,
`plugins/murmur/README.md` and the outline of `plugins/murmur/Source/dsp/MurmurEngine.h` (454 lines),
`plugins/murmur/Source/ui/FlockView.h`.

### 8.1 `shared/dsp/Fx.h`

| Piece | What it is | Reuse |
|---|---|---|
| `DelayLine` | circular buffer, `tap(float d)` linear interpolation, `tapInt(int)`; includes the fix for the float wrap at `r + N` that caused isolated clicks | **Reuse the idea and the fix, not as is.** For the FDN: power-of-two buffer with mask (no `while` / `%` per read), and a cubic (Lagrange 3rd order or Hermite) or allpass read for modulated lines: linear interpolation is a low-pass whose depth changes with the fractional position, so modulation turns into a fluttering HF loss inside a feedback loop (§2.6). Keep `tapInt` for fixed taps. |
| `StereoDelay` | ping-pong (input to the left, each repeat crosses), 1-pole LP 1.5-15.5 kHz and 90 Hz HP in the loop, `fastTanh` soft clip in the loop, time smoothed with an 80 ms `Smoother` (so time changes glide in pitch, tape-like), 4 s max | Good reference for the Ping-Pong character and the "Tape" time feel; it is the 2-line special case of the FDN (permutation matrix). Not reused directly: mono-summed input, fixed filters, no T60 calibration. |
| `PlateReverb` | Dattorro 1997 figure-of-eight tank, lengths scaled from 29761 Hz, input diffusers 0.75/0.75/0.625/0.625, decay diffusion −0.70 / 0.5, bandwidth 0.9995, ~1 Hz modulation of ±16 samples (scaled), the 14 output taps; includes the fix that the 1800-sample allpass needs a longer buffer because an output tap reads 1913 | **Reuse as the Plate character's reference** and as a regression oracle (the new Plate at the same settings should be close in T30 and spectrum). Its `decay` (0.45-0.95) is not calibrated to T60 and the pre-delay is fixed at 12 ms; the FDN version replaces both. The input diffuser chain is directly reusable as the pre-loop diffuser of §3.1. |
| `SvfBand`, `Vocoder` | TPT state-variable band-pass, channel vocoder | `SvfBand` (TPT SVF) is the right filter type for the per-line absorption shelves and the loop LP/HP (stable under modulation). |

### 8.2 `shared/dsp/Common.h` and `Oversampling.h`

- `Rng` (xorshift32, seeded): all randomness (modulation drift, grain spray, phase randomisation in the
  spectral freeze, glitch chance) must come from seeded instances so test S3 can be bit-exact.
- `Smoother`, `DcBlocker` (DC blocker in the loop is mandatory with saturation and with shimmer), `OnePoleLP`
  (TPT), `prewarp`, `fastTanh` (used by `StereoDelay`).
- `Oversampling.h`: 2×/4×/8× Kaiser half-band cascade; use 2× around the tape and BBD saturation inside
  the loop. Its group delay adds to the loop delay, not to the plugin latency, as long as the oversampled
  block sits inside the loop (inferred); subtract it from the line length so the echo time stays exact.

### 8.3 Murmur (granular engine)

What Murmur does (from its README): up to 128 grains reading a 0.5-16 s memory, grain 5 ms-2 s, density
0.5-200/s or synced from 1 bar to 1/32 with triplets, dotted and a skip chance, cubic interpolation, gain
compensated for overlap, Spray reflected rather than clipped, Scan ±2×, Freeze (button or CC 64),
pitch ±24 st with quantised random spread (octaves, fifths, major/minor chord, pentatonic, whole tone),
shimmer (octave up) and sub (octave down) probabilities, reverse grains, envelope from soft bell to hard gate
with skew, feedback into the memory with DC blocker and soft clipper, ducking (5 ms attack, 300 ms
release), two LFOs (sine, triangle, ramp, drift, step), MIDI pitch and gate modes. CPU on an Apple M4:
0.33 % default, 2.55 % for 128 one-second grains with delay and reverb.

Reusable for this product:
- **Grain voice, scheduler and envelope** → the in-loop granular processor (§2.11.4) and the granular
  pitch shifter for shimmer (a pitch shifter is a granular engine with fixed density and two to four
  overlapping grains). The "freeze before the buffer is full" and "spray beyond the newest audio" fixes
  in Murmur's Traps apply unchanged to a granular stage that reads an FDN line's history.
- **Interval quantisation tables** (octaves, fifths, chords, pentatonic) → shimmer intervals and the
  resonator's chord choices.
- **Division table and host sync** (`divisionBeats`, "Off|1 bar|1/2|...|1/8 D") → delay times, glitch,
  LFO sync.
- **LFO module** (shapes, free/synced) → the Movement panel.
- **Ducking follower** → ducking, envelope follower and onset detection share one envelope pair (§4.3).
- **View publishing** (`publishView()`, `std::array<GrainView, 128>`, `std::atomic<int> viewSerial`) and
  `FlockView` (JUCE 2D at 40 Hz) → the room view's data path and render loop.
- **Tools pattern**: `tools/dsptest.cpp` (engine tests without JUCE), `tools/render.cpp` (offline render
  with CPU figure, `--freeze-at`, `--notes`), `tools/presets*.py`, snapshot environment variables for
  manual screenshots.

What not to reuse directly: Murmur's 16 s record buffer is sized for a granular memory; inside the FDN the
granular stage should read from a short per-line history (≤ 2 s) to keep memory and cache use low with 8
lines.

### 8.4 Proposed layout (following Murmur and Lacquer)

```
plugins/<name>/Source/dsp/Fdn.h          lines, Kronecker-rotation matrix, absorption filters, T60 → gains
plugins/<name>/Source/dsp/Diffuser.h      input allpass chain (from PlateReverb), multichannel Hadamard diffuser
plugins/<name>/Source/dsp/Characters.h    tape (wow/flutter/wear/head bump), BBD, spring dispersion, gate
plugins/<name>/Source/dsp/Shimmer.h       granular and spectral pitch shifting for the loop
plugins/<name>/Source/dsp/Spectral.h      STFT freeze, smear, phase randomisation
plugins/<name>/Source/dsp/Reverse.h       reverse windows, lookahead mode
plugins/<name>/Source/dsp/Resonator.h     tuned line lengths, post resonator bank
plugins/<name>/Source/dsp/Safety.h        loop-gain estimate, energy limiter, soft clip, output ceiling
plugins/<name>/Source/dsp/SpaceEngine.h   the chain, Space macro mapping, scenes, view data
plugins/<name>/Source/Params*.h           host parameters, help text, mapping
plugins/<name>/Source/ui/RoomView.h       the cube of light, grid strip, decay graph, headroom bar
plugins/<name>/tools/dsptest.cpp          S1-S25 of §5
plugins/<name>/tools/render.cpp           offline render, --bench, --dump-particles for Blender
plugins/<name>/tools/presets.py           the 32 factory presets of §7
plugins/<name>/docs/ref/CREDITS.md        Jot, Dattorro, Schlecht, Välimäki/Parker/Abel, Holters/Parker, ...
```

Determinism checklist (as in Lacquer §8): no threads in the engine; seeded `Rng`; `-ffp-contract=off` on
the engine translation units; parameter smoothing per sample; STFT and granular schedulers on an internal
fixed hop independent of the host block; flush-to-zero on entry; reset clears every line and filter state.


---

## 9. Decisions to make

1. **Name**: Motes, Halation or Lightwell (§6.3), then a trademark search on the winner.
2. **Line count and matrix**: N = 8 with the Kronecker-rotation matrix as the only matrix, or also offer
   Householder as a "Room" alternative? Ship N = 16 "Deep" in 1.0 or later?
3. **Morph curve**: confirm the order of the five quantities in §3.2 by ear (length spread must lead the
   matrix angle) before freezing presets; decide whether `space` is automatable with Clean time feel or
   always glides.
4. **Shimmer method in 1.0**: granular only (cheap, Murmur code, grainy character) or granular + spectral
   (smoother chords, more code and CPU, window latency inside the loop).
5. **Reverse lookahead mode**: accept a reported latency of up to 1 bar for "Ghost Before You", or ship
   only the after-the-note reverse? (Hosts handle it on playback; live monitoring is unusable.)
6. **Spectral freeze**: on the wet path only, or also as a loop insert? Window size fixed (4096) or
   selectable?
7. **Character count**: 12 as proposed, or fewer for 1.0 (e.g. drop Gated and Spring and add them in 1.1)?
8. **Self-oscillation**: keep "allow self-oscillation" (off by default, Self-Oscillation preset) or never
   allow loop gain above 1?
9. **View technology**: JUCE 2D only (as Murmur) or an OpenGL path from the start for more particles?
10. **MIDI**: resonator note/chord from MIDI input (needs a MIDI-enabled effect bus, as Murmur does) and
    CC 64 for freeze; any other MIDI (scene morph on the mod wheel)?
11. **Scope of 1.0**: which experimental features ship first. Suggested core: FDN + macro + 12 characters
    + full/per-band freeze + interval shimmer + reverse + ducking + envelope follower + scenes + safety +
    view; then granular-in-loop, spectral, resonator in loop, glitch and impossible rooms in 1.1.
12. **Copy and claims**: wording for the audience of music therapy and meditative listening that
    describes sound only, reviewed once for any implied health claim.
