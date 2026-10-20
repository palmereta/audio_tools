# Tarnish user manual

## What Tarnish is

Tarnish puts your sound through an old machine: a studio tape recorder, a cassette deck, a video recorder, a record player, an AM radio or a telephone line. It does what those machines did to music, for good and for bad: the warm saturation of magnetic tape, the slow and fast wobble of a transport, hiss, dropouts where the tape lifts off the head, crackle and scratches on a record, the fading and static of a distant station, the narrow, gritty voice of a phone. It also has the effects people build out of them: tape stop, half-speed, bit and sample-rate reduction.

The machines are modelled, not sampled. The tape saturation is a magnetic hysteresis model (Jiles-Atherton) with the record and playback equalisation of real tape standards; dropouts are the head lifting off the tape, so the highs go first; the wobble is made of the lines a real transport produces (reels, pinch roller, capstan, motor). Everything random follows the song position, so a bounce sounds exactly like what you heard while playing.

The window shows the machine from above. The reels turn at the speed of the tape and their packs grow and shrink as the tape winds; the tape runs over the heads with its wear; and each dropout is a mark on the tape that reaches the play head at the moment you hear it.

![Tarnish with Dusty Cassette: the cassette and its tape running over the heads, the service window on the right, the machines, the eight main knobs and the Damage tab.](img/overview.jpg)

## Installing

Download the package for your system from the website and unzip it.

On macOS:

1. Copy `Tarnish.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `Tarnish.component` (Audio Unit) to `~/Library/Audio/Plug-Ins/Components/`.
2. Rescan plugins in your DAW.
3. `Tarnish.app` is the standalone version and `tarnishrender` the command-line renderer.

The files are not notarized. If macOS refuses to open them, run `xattr -dr com.apple.quarantine` followed by the path of each file.

On Windows:

1. Copy `Tarnish.vst3` to `C:\Program Files\Common Files\VST3\`.
2. Rescan plugins in your DAW.
3. `Tarnish.exe` is the standalone version and `tarnishrender.exe` the command-line renderer.

## First sounds in one minute

1. Put Tarnish on a track, a bus or the master.
2. Open the preset menu and pick **Warm Studio Tape**. That is tape as glue: a little saturation, a soft low end, almost no wobble.
3. Pick **Dusty Cassette**. Watch the tape: the amber marks are dropouts on their way to the play head.
4. Pick **Worn Vinyl**, then **Old Radio**, then **Telephone**.
5. Press **Stop** in the top bar. The machine slows to a halt; press it again and it starts. Any MIDI note you send to Tarnish does the same.

Every control explains itself in the help line at the bottom of the window when you point at it.

## The interface at a glance

- **Top bar.** Presets, **Stop** and **Manual**.
- **The machine.** What the sound goes through, drawn from above. On the right, the service window: the record level, the magnetic loop and the readings.
- **Machines.** Studio, Cassette, Video, Vinyl, Radio and Phone, with the list next to them for the speed, the tape type, the record speed, the band or the line.
- **Eight knobs.** Drive, Saturation, Bias, Wow, Flutter, Wear, Noise and Mix.
- **Tabs.** Damage, Tone, Crush, Speed, Machine and More.

## The machines

![Studio tape seen from above: the supply reel full, the take-up reel filling, the tape running over the erase, record and play heads. The amber marks are dropouts on their way to the play head.](img/deck-studio.jpg)

- **Studio.** A reel-to-reel machine at 30, 15, 7.5 or 3.75 inches per second. The faster it runs, the cleaner and brighter it is and the higher the head bump (the low-end lift every tape machine has). **EQ** chooses the IEC or NAB curve; NAB lifts the lows on playback and cuts them on record, so hum and rumble change with it.
- **Cassette.** **Ferric** tape is warm and hissy, **Chrome** and **Metal** brighter and cleaner. **Half speed** is a dictation or answering machine. Cassette treble saturates first: record it hot and the top end closes, which is the cassette sound.
- **Video.** The linear audio track of a home video recorder: **SP** has about 9 kHz of top, **EP** (long play) about 4.5 kHz. Narrow, hissy, with the wobble of a servo hunting for speed.
- **Vinyl.** A turntable at **33**, **45** or **78** rpm. The music is cut with the RIAA curve and played back through it, so clicks and noise sound like they do on a record. Toward the label the grooves lose highs and distort more; the wobble of an off-centre record grows toward the centre too. A 78 is mono, narrow and has the roar of shellac.

![Vinyl: the scratches sit at fixed places on the disc and turn with it; the arm moves inward as the song plays.](img/deck-vinyl.jpg)

- **Radio.** An AM receiver: the station's heavy compressor, the amplitude modulation (with overmodulation), fading that takes the lows and highs separately, a detector that can't follow fast drops, static, a whistle from a neighbouring station, the receiver's narrow filter and a small speaker. **Narrow** is an old set, **Wide** a hi-fi tuner.

![Radio: the dial with the station slightly off centre, the signal meter following the fading, the speaker flashing with static.](img/deck-radio.jpg)

- **Phone.** **Landline**: 300 to 3400 Hz, the handset's bump, sampled at 8 kHz with the 8-bit mu-law of the phone network. **Mobile**: fewer bits and frames that drop out in bursts.

## Drive, Saturation and Bias

- **Drive** is how hard the signal hits the tape, from -24 to +24 dB. At 0 dB a peak at -18 dBFS sits on the tape's reference level. With **Auto gain** on (the default, in More), Tarnish lowers its output by the same amount, so you hear the saturation and not the volume.
- **Saturation** moves the tape's ceiling: low is a tape with a lot of headroom, high saturates sooner and more gently.
- **Bias** is the machine's alignment. At the centre the tape is linear at low levels. **Under-bias** (left) opens the magnetic loop: quiet notes get a grainy, crossover-like distortion and the top gets rougher. **Over-bias** (right) is cleaner, slightly duller and more compressed.

The magnetic loop in the service window draws what the tape is doing. A thin line is clean tape; as you push Drive, it bends into the corners; with under-bias, it opens into a loop.

## Wow, Flutter and Wear

- **Wow** is slow speed variation, **Flutter** fast. Both are in weighted percent, the way machines are measured (an approximation of IEC 386): 0.03 % is a studio machine, 0.1 % a good cassette deck, 0.3 % a portable player, 1 % something broken. They are made of the lines a real transport produces: on tape the two reels (whose speed changes as the pack winds from one to the other), the pinch roller, the capstan and the motor, plus a slow random drift; on vinyl the turn of an off-centre record and its warp.
- **Wear** is a used tape: the tape lifts off the head (the highs fade), oxide sheds, damaged stretches appear where dropouts cluster. On vinyl it is a dirtier record.

The reading in the service window is measured from the speed of the transport, not copied from the knob.

## Noise and Mix

- **Noise** is the machine's own noise: tape hiss, record surface, line noise. At 50 % it sits where that machine usually does (about -68 dB below the reference on a studio machine at 15 ips, -52 dB on a ferric cassette); 0 is off, 100 % is 20 dB more. The hiss goes through the playback chain, so its colour changes with the machine and the speed.
- **Mix** blends the machine with the dry sound. The dry sound is delayed to line up exactly, but wow and flutter keep moving the tape copy a few milliseconds around it, so a blend in the middle phases like a flanger, as two real machines would. For clean parallel saturation turn Wow and Flutter down; for the flanging, leave them up.

## Damage

- **Dropouts.** Dust or a lump of oxide lifts the tape off the head for a moment. The loss follows the spacing law of magnetic recording: the highs go first and most, the lows survive, so a dropout sounds dull and hollow, not silent. Tapes have damaged stretches where they come in clusters.
- **Crackle.** On vinyl: crackle, clicks and pops, three kinds at their own rates. On the radio: static.
- **Scratches.** Up to three scratches on the record. Each one sits at a fixed place on the disc and pops once per turn, exactly.
- **Scrape.** Modulation noise: the fuzz tape adds around a sustained note.
- **Hum.** Mains hum: 60 Hz, 50 Hz on radio and phone, the field rate on video.
- **Azimuth.** The play head is tilted. The highs drop and the right channel lags a few microseconds, so the stereo image smears and the mono sum turns dull and phasey.
- **Print-through.** On a reel, each layer of tape magnetises the next one. You hear a faint echo of the layer one turn away; its timing follows the size of the reel.

## Tone

- **Low cut** and **High cut** shape the sound after the machine.
- **Noise reduction.** A generic compander, like the ones in cassette decks: **Mild** about 10 dB at the top, **Strong** about 20. It lifts quiet highs before recording and lowers them on playback.
- **Mistrack.** The decoder hears a different level than the encoder, as when a tape is played on another deck. The highs breathe and pump.

## Crush

- **Bits** from 24 (off) down to 1, continuous.
- **Sample rate** from the host's rate (off) down to 500 Hz.
- **Mode.** **Clean** filters before and after and smooths the steps, like a sampler with good filters. **Raw** keeps all the aliasing.
- **Jitter** makes the sample clock waver. **Dither** turns quantisation distortion into hiss.

## Speed

![The Speed tab with Tape Stop Drop: varispeed, and the stop synced to one bar.](img/speed.jpg)

- **Varispeed** runs the machine slower or faster, up to an octave each way.
- **Stop time**, **Stop sync** and **Stop curve**: how long the tape takes to stop, in seconds or as a note value of the host tempo, and how: a straight stop (friction) or a long glide (inertia). The pitch and the level fall with the speed, as with a real playback head.
- **Start time**: how long it takes to get back to speed. When it gets there, Tarnish fades back to the live signal, so the timing lines up again.
- **MIDI stops**: while any MIDI note is held, the tape is stopped. **Host stops**: the tape stops when the host stops playing (in hosts that keep processing with the transport stopped).

The **Stop** button in the top bar does the same, and can be automated.

## Machine

The settings of the selected machine:

- **Studio, Cassette and Video**: dropouts, scrape, azimuth and print-through (the same as in Damage).
- **Vinyl**: **Groove position**, from the outer edge to the label, and **Follow song**, which moves the stylus inward as the song plays, like a side of a record.
- **Radio**: **Station** (how hard the station compresses), **Fading**, **Tuning** (off centre adds a whistle and a lopsided sound), **Whistle** and **Detector** (a slow detector can't follow fast drops).
- **Phone**: **Frame loss** and **Codec**.

## More

- **Output**, after the machine.
- **Auto gain** undoes the level change of Drive.
- **Stereo wobble**: each channel wobbles on its own. Real machines move both channels together; this is not physical, but it is wide.
- **Lock to timeline**: dropouts, crackle, static and the slow drift follow the song position. With it on, a bounce is identical to playback, and bar 17 sounds the same whether you start at bar 1 or bar 16. With it off, every pass is different, like a real worn tape.
- **Quality**: **Eco** halves the CPU of the magnetic model, with a less exact loop at high drive.

## Reading the service window

![The service window with Under-Biased Grit: the record level, the open magnetic loop and the readings.](img/service.jpg)

- **Record level**: the level going onto the tape, against its reference. Above 0 the tape saturates; the play head's gap turns from green to red.
- **The loop**: magnetisation (M) against the head field (H), live.
- **Wow and flutter**: measured from the transport, weighted.
- **Head-to-tape spacing**: how far the tape is from the head right now, in micrometres (wear plus dropouts). One micrometre costs about 1.4 dB at 10 kHz on studio tape at 15 ips and 11 dB on a cassette, which is why worn cassettes lose their top end and studio tape does not.
- **Groove position** on vinyl, **Station** on the radio, and the **Speed** of the machine (it falls during a stop).

## Presets

Thirty-three presets in six groups:

- **Studio tape.** Warm Studio Tape, 30 ips Master, 7.5 ips Demo Reel, Hot Drum Bus, Bounced Four Times, Under-Biased Grit.
- **Cassette.** Dusty Cassette, Chrome Mixtape, Pocket Player Jog, Fourth-Generation Dub, Answering Machine, Chewed Tape, Hot Ferric Saturation, Forgotten in the Car, Lo-Fi Beat Tape.
- **Video tape.** Broken VHS, Rental Copy EP Mode, Tracking Error.
- **Vinyl.** Worn Vinyl, Clean Pressing, Thrift Store 45, Shellac 78, Warped LP, Inner Groove Blues.
- **Radio and phone.** Old Radio, Distant Station, Telephone, Mobile Call Breakup, Intercom.
- **Lo-fi and effects.** Tape Stop Drop, Half-Speed Dream, 12-bit Sampler, Toy Sampler.

## The command-line renderer

`tarnishrender` runs a file through the whole effect:

```
tarnishrender in.wav out.wav --preset "Dusty Cassette"
tarnishrender in.wav out.wav --preset "Tape Stop Drop" --bpm 96 --at 8 stop=1 --at 10 stop=0
tarnishrender in.wav out.wav --set machine=3 --set crackle=50 --set scratches=2
```

- `--preset` loads a preset by name. Without it, the defaults.
- `--tail` adds seconds of silence at the end (6 by default).
- `--bpm` is the tempo for a synced stop (120 by default).
- `--set id=value` sets a parameter before starting; `--at seconds id=value` changes it at that moment.

## Getting the best from it

- **On a bus or the master**, keep it subtle: Warm Studio Tape or 30 ips Master, Drive a few dB, Wow and Flutter low.
- **Push Drive with Auto gain on** and listen to the character change without the volume changing.
- **Cassette sound comes from the level**: record a ferric cassette hot and the highs close down.
- **Dropouts and Wear together** make a tape that sounds used; Wear alone just makes it duller.
- **Lock to timeline** keeps the damage repeatable while you mix; turn it off for a performance that is different every time.

## Limits

- The wow and flutter reading uses an approximation of the IEC 386 weighting, not the curve of the standard.
- Varispeed is a pitch shifter with two crossfading heads: on long sustained notes you may hear a slight flutter at its window rate.
- The radio is modelled on the envelope of the AM signal, not on a carrier.
- Host stops only works in hosts that keep processing audio while the transport is stopped.

## Technical notes

- **Magnetic tape.** Jiles-Atherton hysteresis in normalised units, solved with fourth-order Runge-Kutta over the path of the head field, four times oversampled (second-order in Eco). The record pre-emphasis comes before it and the playback de-emphasis after, as exact inverses; the playback losses, the head bump and the hiss come in between.
- **Losses.** The playback corner of each machine plus the spacing loss of magnetic recording (54.6·d/λ dB) for wear and dropouts, and the azimuth loss with its interchannel delay.
- **Transport.** Wow and flutter as delay modulation from the lines of the transport; reel frequencies follow the pack radius as the tape winds; a slow random drift. The weighted level is set by the knob, and measured.
- **Determinism.** Everything is computed per sample, and random events come from a hash of the position: the output is identical for any block size, and identical to playback when locked to the timeline.
- **Latency.** Constant: 25 ms of transport plus 54 samples of oversampling and one sample, reported to the host.
- **CPU.** For the default studio machine (magnetic model at 4x), 5.3 % of one core on an Apple M4 and 5.5 % on an AMD Ryzen 9 5900XT; Dusty Cassette 5.6 / 6.1 %; vinyl, radio and phone about 0.5 / 1.1 %. Eco halves the magnetic model.
- **Aliasing.** Below -78 dBFS up to 9 kHz with the drive at +12 dB. A full-scale 15 kHz tone at +12 dB on 30 ips tape (the extreme case) aliases at about -46 dBFS.
- **Tested** with an automated suite of 13 tests (no NaN, exact latency, determinism across block sizes and start positions, measured wow and flutter, hysteresis loop, aliasing, frequency response, noise levels, event rates, tape stop timing, sample-rate independence, CPU), pluginval and auval.
