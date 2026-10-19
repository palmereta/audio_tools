# ATFS1: what the engine models, and from where

This is the public summary of the model behind ATFS1. Every law below cites where it comes from:

- **[OM]** Yamaha FS1R Owner's Manual (1998) and **[DL]** Data List, free downloads from Yamaha's
  manual library. Not redistributed here (see [`ref/CREDITS.md`](ref/CREDITS.md)).
- **[PAT]** US Patent 5,610,354, "Tone signal generating apparatus" (Yamaha, priority 1995, expired).
- **[FSVR]** the FSVR project (https://github.com/musicastudio/FSVR, GPL-3), which measured real
  units and decompiled the firmware. ATFS1 uses its *published measurements and documents* as
  reference numbers. No FSVR code is used, and no Yamaha ROM data (voices, Fseqs, tables) ships with
  ATFS1.

Where a law could not be pinned down, the value we chose is marked **(ours)**.

## 1. Architecture

| Item | FS1R | ATFS1 |
|---|---|---|
| Polyphony | 32; with the filter on, only 16 voices are filtered [FSVR] | 32, every voice filtered; the *Authentic polyphony* switch restores 16 |
| Parts | 4 parts per performance [OM] | one part per instance (load four instances for four parts) |
| Voice | 8 voiced + 8 unvoiced operators, 88 algorithms, feedback 0-7, LFO1/LFO2, pitch EG, filter + EG, Formant and FM knob matrices [OM, DL] | same |
| Sample rate | 48 kHz fixed [FSVR] | host rate; every law is expressed in Hz or seconds |
| Control rate | pitch EG and LFO at 192 Hz [FSVR] | everything at 1/16 of the sample rate (3 kHz at 48 kHz), amplitudes interpolated per sample |

## 2. Voiced operator

**Frequency** [OM p.63; FSVR]. Ratio mode: coarse 0 = 0.5, else coarse × (1 + fine/100). Fixed mode:
440 Hz × 2^(coarse + fine/128 − 16), so coarse is in octaves and fine in 1/128 octave; *note scale*
tracks the keyboard around C3. Detune is a few cents, smaller at the top **(ours: 0.6 cent per step,
halving every three octaves up)**. The frequency EG only reaches ratio operators, 24 semitones per
traverse [FSVR].

**Spectral forms** [PAT; FSVR measurements; OM p.62]. Each form is a carrier inside a window restarted
at a grain rate (two overlapping grain slots, so a window can last two periods):

| Form | Grain rate | Window | Carrier | Measured result we reproduce |
|---|---|---|---|---|
| sine | none | none | operator frequency | one partial |
| all1, all2 | operator frequency | 2 periods, sin^p (all2) or sin^p rise / sin² fall (all1) | operator frequency, crest at the window centre **(ours)** | one partial at skirt 0; group width at skirt 7 ≈ 1.3 (all1) and 6.5 (all2) partials |
| odd1, odd2 | 2 × operator frequency | 1 grain | continuous, alternate grains inverted | partials 1 and 3 at skirt 0, odd partials only |
| res1, res2 | operator frequency | 1 period | (resonance + 1) × operator frequency | 3 lines at harmonics r, r+1, r+2, sides at −6 dB at skirt 0 |
| frmt | note pitch × 2^(transpose/12) | min(2 periods, 1/(3.1 Hz × 2^(BW/8))) | formant centre (operator frequency) | bandwidth in Hz doubles every 8 steps, independent of the note |

Skirt sets the window exponent: p = 2·√2^skirt for frmt, p = 2·2^skirt for the other forms [FSVR].
The window length and carrier frequency are latched at the start of each grain, as the chip does.

**FM** [FSVR]. A modulator at level 99 shifts the carrier phase by 3.369 cycles. The algorithm's
feedback loop adds 0.5 × 2^(fb − 7) of the mean of the last two output samples (DX7 style).

**Level** [OM]. 0.75 dB per step. Level scaling is DX7 style around a break point (note = value + 21)
**(curve shapes ours)**; velocity, LFO amplitude sensitivity ({0, 1, 2, 4, 5, 6, 7, 8}/8 [FSVR]) and
EG bias are dB offsets. Carrier level correction lowers an operator by 1.5 dB per step when it is a
carrier.

## 3. Amplitude EG [OM p.66-67; FSVR]

- Hold, then L1 at T1, L2 at T2, L3 at T3 (sustain), L4 at T4 after key-off; it starts from L4.
- TIME → chip rate q = ((99 − T) × 164) >> 8; a full 96 dB traverse takes 2^(26 − q/4) / (4 + q mod 4)
  / 48000 s, from 6.1 ms (T 0) to 350 s (T 99).
- Decays are linear in dB. Attacks approach the target exponentially with a time constant of 1/16
  traverse, from a −53.2 dB floor, aiming 3.8 dB past the target.
- Time scaling adds trunc(scale × x / 8) to the rate, x = (note − 55)/4 saturating at ±13.
- The part's Attack, Decay and Release offsets move T1, T2/T3 and T4.

## 4. Unvoiced operator [PAT; FSVR measurements]

White noise through two one-pole low-passes, ring-modulated to the centre frequency: a band of ±corner
around the formant. The corner grows 17 Hz per bandwidth register step (saturating at BW 60) and
×1.35 per skirt step; resonance 4 narrows the band, 5 to 7 add a sine at the centre (0.21, 0.46,
0.76, 1.02 of the band level). Level against bandwidth: −20 dB at 0, flat from 5 to 20, 15 dB lower
at the top. The output is the mean of two samples. Modes: own frequency (fixed, as a voiced fixed
operator), linkFO (follows the pitch), linkFF (follows the paired voiced formant). Unvoiced gain
relative to voiced **(ours: about −9 dB RMS at BW 30 for level 99)**.

## 5. Formant Sequences [DL p.40; FSVR]

- Frame: fundamental pitch, frequency and level of 8 voiced and 8 unvoiced formants. Frequencies are
  words of 1/1024 octave, Hz = 440 × 2^((w − 26861)/1024); levels are 7-bit.
- Playback: about 126 frames per second at 100 % (7.9 ms per frame), no interpolation between
  frames on the FS1R; levels glide over about 1.6 ms on the chip. Loops one-way or round between
  the loop points while the key is held.
- An operator with its Fseq switch on takes the frame's frequency and level (track n drives the
  voiced and unvoiced pair n); with *pitch* mode the frame pitch moves the note, relative to the
  sequence's note assign.
- **Extensions:** optional linear interpolation, 10 to 1000 % speed, an Fseq per preset.

**Factory sequences (ours).** Vowels use the average formants of Peterson and Barney (1952); spoken
phrases are spoken by espeak-ng (free speech synthesis) and analysed by ATFS1. **Analysis:** pitch by
normalized autocorrelation (60 to 500 Hz); formants from the roots of a 16-to-28-order LPC polynomial
(Durand-Kerner) with bandwidth under 500 Hz, levels from the LPC envelope without the pre-emphasis
tilt; noise from the energy of 8 bands weighted by aperiodicity.

## 6. Common [OM p.51-59; DL p.39]

- **Pitch EG:** L0 → L1 → L2 → L3 (sustain) → L4, ±50 = 8, 2, 1 or ½ octave by range; velocity
  scales the depth.
- **LFO1** (triangle, saw down, saw up, square, sine, sample and hold) to pitch (per-operator
  sensitivity), amplitude, fixed frequencies and the filter; **LFO2** to the filter. Speed law
  **(ours: 0.0645 Hz × e^(0.0675 × speed))**.
- **Filter** [FSVR measurements]: cutoff 17.4 Hz × 2^(value/12); LPF 24, 18 and 12 dB/oct, HPF
  12 dB/oct, BPF and BEF; resonance −16 (damper than flat) to +100 (edge of self-oscillation); input
  gain −12 to +12 dB with soft drive; filter EG with ±50 levels and its own depth.
- **Knob matrices:** each slot moves the level, frequency or width of one operator; +63 reaches
  100 % at half travel [OM p.31].

## 7. Effects (ours)

The FS1R effects are Yamaha DSP programs; ATFS1 keeps the topology (insertion on the part, sends to
variation and reverb, variation into reverb, EQ) with its own algorithms: 8 insertion types, 10
variation types and 7 reverbs.

## 8. Sysex [DL p.35-42]

Voice bulk: 608 bytes (common 112 + 8 × (35 voiced + 27 unvoiced)), checksum over count, address and
data. Fseq bulk: 32-byte header + 50 bytes per frame (up to 512 frames; the byte count overflows at
512, so the header's frame format is used instead). ATFS1 reads voice and Fseq bulks (addresses
40-43, 51, 60, 61, 70) and writes both.
