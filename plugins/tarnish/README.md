# Tarnish

Tape, cassette, video, vinyl, AM radio and telephone: an original design, not a recreation of any product. VST3,
Audio Unit (macOS, a music effect with MIDI input), standalone app, and `tarnishrender`, a command-line renderer.
Version 1.0.0.

The research behind it (what the tape and lo-fi plugins on the market do and what users ask of them, the physics of
magnetic recording, transports, vinyl, AM radio and the phone network, the visual direction, the test plan) is in
[`docs/research.md`](docs/research.md). The user manual is [`docs/manual/en.md`](docs/manual/en.md) (and `es.md`),
embedded in the plugin.

## How it works

Everything is in `Source/dsp/`, without JUCE, per sample, in double precision:

1. **Magnetic tape** (`Hysteresis.h`). Jiles-Atherton hysteresis in units normalised by the saturation
   magnetisation (tape values from Chowdhury, DAFx 2019, divided by M_s). It is integrated over the path of the
   head field, not over time, so the model is sample-rate independent like real hysteresis: between two
   sub-samples h moves in a straight line and dm = χ(h, m, δ)·dh, solved with RK4 (RK2 in Eco) at 4x
   oversampling (`dh::lq::Os4` from Lacquer). The Langevin function and its derivative come from one exponential,
   with their series near zero (silence never produces NaN). Bias moves the reversibility c and the pinning k:
   nominal is close to anhysteretic (linear at low level, soft ceiling), under-bias opens the loop. The output gain
   is calibrated at the tape's reference level with the drive at 0 dB.
2. **Machines** (`Machines.h`). Speed, equalisation time constants (IEC, NAB, cassette 120/70 µs), head bump,
   playback corner, tape formulation, self-erasure, hiss, reel geometry and transport diameters for each machine
   and variant.
3. **Transport** (`Transport.h`). Speed error as delay modulation from spectral lines: reels (their frequency follows
   the pack radius, and their phase is a closed formula: turns = (r − r_hub)/h), pinch roller, capstan, motor, a
   video servo, the turn and warp of a record, plus a drift drawn as smooth value noise. The amplitudes are set so
   the weighted reading (an approximation of IEC 386) equals the knob. Tape stop: the read pointer slows on a
   friction-to-inertia curve and the level follows the speed; at the end of the start it crossfades back to the
   live position. Print-through reads one take-up revolution further back.
4. **Events** (`Events.h`). Dropouts are a transient head-to-tape spacing d(t), fed into the playback loss, so the
   highs go first (spacing loss 54.6·d/λ dB). Each event is decided by a hash of (seed, position): rates are in
   events per second at any sample rate, independent of the block size, and the editor can compute the coming
   dropouts to draw them on the tape before they reach the head. Vinyl crackle in three classes with a
   heavy-tailed amplitude; scratches at fixed angles of the disc, triggered when the rotation crosses them.
5. **The rest** (`Lofi.h`). A generic sliding-shelf compander for noise reduction and its mistracking; the AM radio
   on the envelope (station compressor, overmodulation, two-band fading, diode detector with its RC, static,
   whistle, IF filter, speaker); the telephone (300-3400 Hz, 8 kHz, exact mu-law, frame loss); the crusher (hold
   phase from the position, band-limited steps with PolyBLEP, raw mode); varispeed (two-head shifter).
6. **Engine** (`Engine.h`). The chain of each machine; record pre-emphasis before the hysteresis and playback
   de-emphasis after it as exact inverses at the same rate; losses, head bump, azimuth (loss plus interchannel delay),
   hiss injected before the de-emphasis and calibrated from the filter magnitudes; output stage that saturates
   softly above +12 dBFS; dry path delayed by the constant latency (25 ms + 54 + 1 samples).

The editor (`Source/ui/DeckView.h`) draws each machine from above and the service window: the record level, the
hysteresis loop from the engine's state and the measured wow and flutter.

## Tests

`tarnishtest` (`tools/tarnishtest.cpp`), 13 groups: robustness (every machine, random settings, extreme signals,
30 s of digital silence), exact latency and Mix 0, bit-identical output across block sizes (1, 64, 512, 4096,
random) and across start positions on the timeline, wow and flutter measured by zero crossings and weighted
against the set value (±15 %), hysteresis (periodic steady state, odd symmetry, |M| ≤ M_s, loop area with drive
and bias, THD < 0.1 % at −40 dBFS, the under-bias signature), aliasing (below −78 dBFS up to 9 kHz at +12 dB of drive; a full-scale 15 kHz tone at +12 dB on 30 ips tape reaches −46 dBFS, reported as a figure), the response of each
machine (−3 dB at the profile corner, cassette top end closing with level), hiss within ±1 dB, dropout rates
independent of the sample rate and scratches exactly once per turn, tape stop within 1 ms and pitch following the
speed, sample-rate independence, and CPU. Plus pluginval at strictness 10, auval (`aumf Trns Atls`) and
`athosttest`.

## Web and film

- `tools/blender/deck.py`: the generic open-reel machine (pc50-wsl, under the GPU lock).
- `tools/web_images.py`: web images from the renders and the manual screenshots.
- `tools/mkfilm.sh`: the film and its sound (Dusty Cassette, with a tape stop).
- `tools/presets.py`: the 33 factory presets.
- Demo clips: `python3 scripts/samples.py --only tarnish`.
