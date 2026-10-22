# Electric guitar by physical modelling (Thrum): research

Compiled 2026-10-21 for a new Audio Tools product: a free, open-source (AGPL-3.0) electric guitar instrument
built by physical modelling, working name **Thrum** (slug `thrum`, product name "Audio Tools Thrum"). It is
one plug-in (VST3 / AU / standalone) that contains the strings, the pickups and the passive controls, a pedal
board, an amplifier, a cabinet with microphones and a room, plus the performance layer that a keyboard player
needs to sound like a guitarist: string and fret assignment, a strummer, a fingerpicking arpeggiator,
articulations and per-note variation. It covers the physics and the published models of the plucked string
(§1), pickups and the passive circuit (§2), amplifiers and cabinets (§3), pedals (§4), what makes a virtual
guitar sound real (§5), the market and what users criticise (§6), MIDI conventions for guitars (§7), a DSP and
performance design (§8), the licences of the code studied (§9), the test plan (§10) and the open questions
(§11).

Conventions (the same as `plugins/at303/docs/research.md` and `plugins/halation/docs/research.md`):
- Every fact carries its source inline: a URL (all accessed 2026-10-21) or a document with page, table or
  section. Short tags are defined in the table below. The column "Read" says how much of each source was
  actually read: **full** (the whole text or source file), **part** (pages, tables or files), **abstract**
  (only the publisher's abstract or a catalogue entry), **snippet** (only a search-engine summary or a
  third-party quote). Where a claim rests on a forum post, a retailer listing or a vendor's own text, the
  text says so.
- **(inferred)** marks my own conclusions, calculations or design proposals. Values marked "calculated"
  follow from a formula given in the text; the script is trivial (Python, a few lines) and the numbers can be
  re-derived. §8, §10 and §11 are design proposals throughout; their numbers are starting values to tune with
  the tests of §10.
- Web search and fetch were available in this session, but several primary sources are paywalled (AES,
  IEEE, CMJ) and electrosmash.com did not resolve from this machine. For those, only abstracts, catalogue
  entries or search summaries were available, and the tag table says so. Nothing marked "abstract" or
  "snippet" should be quoted as a result of the paper without reading it; §11 lists the papers to obtain.
- Product names (Fender, Stratocaster, Telecaster, Gibson, Les Paul, Marshall, Vox, Mesa/Boogie, Ibanez Tube
  Screamer, Dunlop Cry Baby, Pro Co RAT, Fuzz Face, D'Addario, Celestion, Shure, Seymour Duncan, Ample Sound,
  Native Instruments, Impact Soundworks, MusicLab, Orange Tree Samples, Prominy, UJAM, Applied Acoustics
  Systems, IK Multimedia, Neural DSP, Line 6, Kemper, Fishman, Roland, Jamstik and the others) are trademarks
  of their owners and appear here only for research. **The product shows no brand names, logos or
  recognisable body or headstock shapes**: guitar types are named by construction ("solid, three single
  coils, 25.5 in"), amplifier voices by region and era ("American clean", "British crunch"), pedals by
  function ("green overdrive" becomes "mid overdrive").
- **Nothing here copies proprietary text or code.** The implementation is written from the papers and from
  first principles. Open-source code was read to learn structure and pitfalls, never copied; §9 records each
  licence and what was read. Component values (resistor and capacitor values of published circuits) and
  measured physical constants are facts and are cited, not copied as code.
- Nothing here is legal advice. Patent status is reported as Google Patents shows it.

Source tags used throughout:

| Tag | Document | Where | Read |
|---|---|---|---|
| **[PASP §]** | Julius O. Smith III, *Physical Audio Signal Processing*, W3K Publishing, 2010, ISBN 978-0-9745607-2-4, online edition; sections cited by page name | https://ccrma.stanford.edu/~jos/pasp/ (e.g. `Extended_Karplus_Strong_Algorithm.html`, `Commuted_Synthesis.html`, `Tension_Modulation.html`, `Electric_Guitars.html`, `Dispersion_Filter_Design.html`) | part |
| **[KS83]** | K. Karplus, A. Strong, "Digital synthesis of plucked-string and drum timbres" (title as commonly cited), *Computer Music Journal* 7(2), 1983, pp. 43-55 | via [PASP Karplus_Strong_Algorithms] and the catalogue https://codemist.co.uk/AmsterdamCatalog/15/index.html | abstract |
| **[JS83]** | D. A. Jaffe, J. O. Smith, "Extensions of the Karplus-Strong plucked-string algorithm", *CMJ* 7(2), 1983; reprinted in C. Roads (ed.), *The Music Machine*, MIT Press 1989, pp. 481-494 | same catalogue; the components are given in [PASP Extended_Karplus_Strong_Algorithm] | via PASP |
| **[KVT98]** | M. Karjalainen, V. Välimäki, T. Tolonen, "Plucked-string models: from the Karplus-Strong algorithm to digital waveguides and beyond", *CMJ* 22(3), 1998, pp. 17-32 | Aalto record https://aaltodoc.aalto.fi/items/2a2302cd-d0d7-4390-b531-fc743eff29e2 | abstract |
| **[VHKJ96]** | V. Välimäki, J. Huopaniemi, M. Karjalainen, Z. Jánosy, "Physical modeling of plucked string instruments with application to real-time sound synthesis", *JAES* 44(5), 1996, pp. 331-353 | https://aes2.org/publications/elibrary-page/?id=7902 | abstract |
| **[KVJ93]** | M. Karjalainen, V. Välimäki, Z. Jánosy, "Towards high-quality sound synthesis of the guitar and string instruments", ICMC 1993 | dblp https://dblp.org/pid/175/5434 | abstract |
| **[RV06]** | J. Rauhala, V. Välimäki, "Dispersion modeling in waveguide piano synthesis using tunable allpass filters", DAFx-06; and the companion letter "Tunable dispersion filter design for piano synthesis", *IEEE SPL*, 2006 (volume and pages not verified) | https://dafx.de/paper-archive/details/n4Qv1ScD_VbmqShsq0ZVQQ; [PASP Dispersion_Filter_Design] cites the letter as "RauhalaAndVesa2006" | abstract |
| **[AVS10]** | J. S. Abel, V. Välimäki, J. O. Smith III, "Robust, efficient design of allpass filters for dispersive string sound synthesis", *IEEE SPL* 17(4), 2010, pp. 406-409, doi 10.1109/LSP.2010.2040924 | CCRMA wiki https://ccrma.stanford.edu/wiki/Dispersion_Filter_Design (abstract and `adf.m`) | abstract |
| **[VDS94]** | S. Van Duyne, J. O. Smith, "A simplified approach to modeling dispersion caused by stiffness in strings and plates", ICMC 1994 | Michigan ICMC archive https://quod.lib.umich.edu/i/icmc/bbp2372.1994.105/ (record shows title and year; authors as commonly cited, not shown in the record) | snippet |
| **[FL64]** | H. Fletcher, "Normal vibration frequencies of a stiff piano string", *JASA* 36(1), 1964, pp. 203-209 | via https://www.acs.psu.edu/drussell/Demos/Stiffness-Inharmonicity/Stiffness-B.html | snippet |
| **[PLF14]** | A. Paté, J.-L. Le Carrou, B. Fabre, "Predicting the decay time of solid body electric guitar tones", *JASA* 135(5), May 2014, pp. 3045-3055 (11 pp.), doi 10.1121/1.4871360 | https://www.lam.jussieu.fr/Membres/LeCarrou/Articles/A8_Pate_PredictingDecayTime.pdf | full |
| **[TVK00]** | T. Tolonen, V. Välimäki, M. Karjalainen, "Modeling of tension modulation nonlinearity in plucked strings", *IEEE TSAP* 8(3), 2000, pp. 300-310 | dblp; Aalto record (gives pp. 300-311) | abstract |
| **[EE10]** | G. Evangelista, F. Eckerholm, "Player-instrument interaction models for digital waveguide synthesis of guitar: touch and collisions", *IEEE TASLP* 18(4), 2010, pp. 822-832, doi 10.1109/TASL.2009.2038822 | precursor: "The PluckSynth touch string", DAFx-08, https://dafx.de/paper-archive/details/XJTIoSIXj3qqer3WzLlEHQ | abstract |
| **[BT15]** | S. Bilbao, A. Torin, "Numerical modeling and sound synthesis for articulated string/fretboard interactions", *JAES* 63(5), 2015; companion "Numerical simulation of string/barrier collisions: the fretboard", DAFx-14 (best paper) | https://aes2.org/publications/elibrary-page/?id=17639, https://dafx.de/paper-archive/details/VW3ckMHQUdwpOen-KsadwA | abstract |
| **[WLG]** | "Influence of damping and nonlinearity in plucked strings: why do light-gauge strings sound brighter?" (Cambridge, J. Woodhouse's group; full author list not verified) | https://www.repository.cam.ac.uk/handle/1810/270956 | abstract |
| **[WH04]** | J. Woodhouse, "On the synthesis of guitar plucks", *Acta Acustica united with Acustica* 90 (2004) 928-944; "Plucked guitar transients", same issue, 945-965 | reference lists, e.g. https://acta-acustica.edpsciences.org/articles/aacus/ref/2021/01/aacus200055/aacus200055.html | snippet |
| **[LVKL96]** | T. I. Laakso, V. Välimäki, M. Karjalainen, U. K. Laine, "Splitting the unit delay", *IEEE Signal Processing Magazine* 13(1), 1996, pp. 30-60, doi 10.1109/79.482137 | research.fi record | abstract |
| **[PPB07]** | J. Pakarinen, H. Penttinen, B. Bank, "Analysis of handling noises on wound strings", *JASA* 122(6), 2007, pp. EL197-EL202 | https://research.aalto.fi/en/publications/analysis-of-handling-noises-on-wound-strings/ | abstract |
| **[PPV08]** | J. Pakarinen, T. Puputti, V. Välimäki, "Virtual slide guitar", *CMJ* 32(3), 2008, pp. 42-54; NIME-08 "Slide guitar synthesizer with gestural control" | https://nime.org/proc/nime2008_pakarinen/ | abstract |
| **[DAD]** | D'Addario, "A complete technical reference for fretted instrument string tensions" (tension chart, 14 pp., item 13934; PDF creation date 1999) | archived copy of http://www.daddario.com/upload/tension_chart_13934.pdf via web.archive.org; the current URL https://www.daddario.com/globalassets/pdfs/accessories/tension_chart_13934.pdf now redirects to an online calculator | full |
| **[STK]** | Synthesis ToolKit in C++ (P. R. Cook, G. P. Scavone), `LICENSE`, `Plucked.cpp`, `StifKarp.{h,cpp}`, `Twang.{h,cpp}`, `Guitar.{h,cpp}`, branch master | https://github.com/thestk/stk | part (those files) |
| **[FPM]** | Faust "Physical Models Library" `physmodels.lib` v1.2.4 (R. Michon, P.-A. Grumiaux, Y. Orlarey; GRAME) | https://github.com/grame-cncm/faustlibraries/blob/master/physmodels.lib | part (header, strings, guitars) |
| **[FTS]** | Faust `tonestacks.lib` (author "Guitarix project", licence LGPL-2.1-or-later): FMV tonestack coefficients and 25 component sets | https://github.com/grame-cncm/faustlibraries/blob/master/tonestacks.lib | full |
| **[GX]** | Guitarix, `trunk/COPYING` and source tree, latest release V0.47.0 (2025-08-25) | https://github.com/brummer10/guitarix | part (licence, file list) |
| **[NAM]** | Neural Amp Modeler: `NeuralAmpModelerCore` (v0.6.0, 2026-10-01), `NeuralAmpModelerPlugin`, `neural-amp-modeler` (trainer); licences from the GitHub API | https://github.com/sdatkinson/NeuralAmpModelerCore | part (README, file list, licence) |
| **[PPV12]** | R. C. D. Paiva, J. Pakarinen, V. Välimäki, "Acoustics and modeling of pickups", *JAES* 60(10), 2012, pp. 768-782 | https://aes2.org/publications/elibrary-page/?id=16551; abstract at https://www.aminer.org/pub/56d835a7dabfae2eee3de3c9/acoustics-and-modeling-of-pickups | abstract |
| **[HM09]** | N. G. Horton, T. R. Moore, "Modeling the magnetic pickup of an electric guitar", *Am. J. Phys.* 77(2), 2009, doi 10.1119/1.2990663 | https://scholarship.rollins.edu/stud_fac/14 | abstract |
| **[LEM]** | H. Lemme, *Elektrogitarren: Technik und Sound* / "The secrets of electric guitar pickups" (book and articles; first published 1977 per a forum post) | only via secondary quotes: https://ozvalveamps.org/pickups.html and a Music Electronics Forum post | snippet |
| **[ZOL]** | M. Zollner, *Physik der Elektrogitarre*, 2 vols., self-published, Regensburg 2014 (≈1,300 pp.) | TUM catalogue https://search.ub.tum.de/vufind/Record/DE-604.BV042222407; review https://www.spektrum.de/magazin/rezension-zu-physik-der-elektrogitarre-von-manfred-zollner/1342448 | snippet |
| **[VV]** | P. Biltoft (Vintage Vibe Guitars), "Pickup inductance", Tech Tips, June 2017, reposted by Planet Z | https://www.planetz.com/pickup-inductance/ | full |
| **[SD-DCR]** | Seymour Duncan, "What is the DC resistance of popular pickups?" | https://www.seymourduncan.com/?p=5293 | full |
| **[ESP214]** | R. Elliott, Elliott Sound Products, Project 214 (guitar cable capacitance) | https://www.sound-au.com/project214.htm | snippet |
| **[GEW]** | Guitar Electronics, "Guitar wiring FAQs" (pot values, tone capacitors) | https://guitarelectronics.com/diagrams-resources/guitar-wiring-faqs/ | snippet |
| **[PY09]** | J. Pakarinen, D. T. Yeh, "A review of digital techniques for modeling vacuum-tube guitar amplifiers", *CMJ* 33(2), 2009, pp. 85-100 | https://research.aalto.fi/en/publications/a-review-on-digital-techniques-for-modeling-vacuum-tube-guitar-am/ | abstract |
| **[YS06]** | D. T. Yeh, J. O. Smith, "Discretization of the '59 Fender Bassman tone stack", DAFx-06 | https://dafx.de/paper-archive/details/bbvGpyU59H2nHvV3Iamezg, https://ccrma.stanford.edu/~dtyeh/tonestack/ | abstract (coefficients via [FTS]) |
| **[YAS07]** | D. T. Yeh, J. S. Abel, J. O. Smith, "Simplified, physically-informed models of distortion and overdrive guitar effects pedals", DAFx-07 | https://dafx.de/paper-archive/details/mIq7vlQKu6pUlF467vvhgw | abstract |
| **[YEH09]** | D. T. Yeh, *Digital implementation of musical distortion circuits by analysis and simulation*, PhD thesis, Stanford, June 2009 | https://ccrma.stanford.edu/~dtyeh/papers/DavidYehThesisdoublesided.pdf | snippet |
| **[KOR]** | N. Koren, "Improved vacuum tube models for SPICE simulations", *Glass Audio* 8(5), 1996, p. 18 (an erratum gives no. 6), web version | https://normankoren.com/Audio/Tubemodspice_article.html | snippet |
| **[CH10]** | I. Cohen, T. Hélie, "Real-time simulation of a guitar power amplifier", DAFx-10 | https://dafx.de/paper-archive/details/yZNxN3FPA2HJUN5vKmCG0A | abstract |
| **[PZL16]** | J. D. Parker, V. Zavalishin, E. Le Bivic, "Reducing the aliasing of nonlinear waveshaping using continuous-time convolution", DAFx-16 | https://dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA | abstract |
| **[BEPV17]** | S. Bilbao, F. Esqueda, J. D. Parker, V. Välimäki, "Antiderivative antialiasing for memoryless nonlinearities", *IEEE SPL* 24(7), 2017, pp. 1049-1053 | https://aaltodoc.aalto.fi/items/470aab15-1702-4ccf-a148-24e6173079fb (open access) | abstract |
| **[VPA10]** | V. Välimäki, J. Parker, J. S. Abel, "Parametric spring reverberation effect", *JAES* 58(7/8), 2010, pp. 547-562 | https://aes2.org/publications/elibrary-page/?id=15511 | abstract |
| **[PB09]**, **[P11]** | J. Parker, S. Bilbao, "Spring reverberation: a physical perspective", DAFx-09; J. Parker, "Efficient dispersion generation structures for spring reverb emulation", *EURASIP J. Adv. Signal Process.* 2011, art. 646134 | https://dafx.de/paper-archive/details/MBjqiMDVjnAseoN9C-fzBw, https://link.springer.com/article/10.1155/2011/646134 | abstract |
| **[RS10]** | C. Raffel, J. O. Smith, "Practical modeling of bucket-brigade device circuits", DAFx-10 | https://dafx.de/paper-archive/details/JhVfAOFXD1lAtctkMTUODg | abstract |
| **[EFHZ14]** | F. Eichas, M. Fink, M. Holters, U. Zölzer, "Physical modeling of the MXR Phase 90 guitar effect pedal", DAFx-14 | https://dafx14.fau.de/papers/dafx14_felix_eichas_physical_modeling_of_the_.pdf | abstract |
| **[HZ11]** | M. Holters, U. Zölzer, nodal DK method for circuits with variable parts, exemplified with the Cry Baby wah, DAFx-11 | DAFx archive (via search) | snippet |
| **[TS-UO]** | University of Oregon physics wiki, "Electronics" page (Tube Screamer clipping stage) | https://june.uoregon.edu/mediawiki/index.php/Electronics | snippet |
| **[ES]** | ElectroSmash circuit analyses (Fuzz Face, RAT, Cry Baby, Tube Screamer); the site did not resolve from this machine | https://electrosmash.com/fuzz-face, https://electrosmash.com/proco-rat | snippet |
| **[SAY89]** | S. I. Sayegh, "Fingering for string instruments with the optimum path paradigm", *CMJ* 13, 1989, pp. 76-84 | via citing papers, e.g. https://archives.ismir.net/ismir2007/paper/000355.pdf | snippet |
| **[TP05]** | D. R. Tuohy, W. D. Potter, "A genetic algorithm for the automatic generation of playable guitar tablature", ICMC 2005, pp. 499-502 | Tuohy's MS thesis https://www.ai.uga.edu/sites/default/files/inline-files/tuohy_daniel.pdf | snippet |
| **[RAL04]**, **[RL05]** | D. P. Radicioni, L. Anselma, V. Lombardo, "A segmentation-based prototype to compute string instruments fingering", CIM 2004; D. P. Radicioni, V. Lombardo, CSP model of guitarist hand gestures, 2005 | https://iris.unito.it/bitstream/2318/28825/1/radicioni05csp.pdf | snippet |
| **[BTSB12]** | I. Barbancho, L. J. Tardón, S. Sammartino, A. M. Barbancho, "Inharmonicity-based method for the automatic generation of guitar tablature", *IEEE TASLP* 20(6), 2012, pp. 1857-1868, doi 10.1109/TASL.2012.2191281 | https://riuma.uma.es/entities/publication/a6596b84-8c45-4440-827c-d79247375685 | abstract |
| **[FAVS18]** | S. Freire, A. Armondes, J. Viana, R. Silva, "Strumming on an acoustic nylon guitar: microtiming, beat control and rhythmic expression in three different accompaniment patterns", SMC 2018, pp. 543-548 | https://zenodo.org/record/1422685 (`smc_2018_077.pdf`) | full |
| **[MPE]** | MIDI Association, MPE (MIDI Polyphonic Expression), ratified 28 Jan 2018; quotes of the spec on pitch-bend sensitivity in a midi.org forum thread; zones and dimensions on Roger Linn Design's page | https://www.midi.org/midi-articles/midi-polyphonic-expression-mpe, https://midi.org/community/midi-specifications/how-midi-mpe-pitch-bend-works, https://rogerlinndesign.com/support/support-linnstrument-what-is-mpe | part |
| **[MODE4]** | MIDI Mode 4 (Omni Off / Mono) and guitar "Mono mode" | https://www.sweetwater.com/insync/more-midi-modes-clearer-definition/, https://midi.org/community/midi-specifications/mono-mode-on-parameter | snippet |
| **[GTR-MIDI]** | Fishman TriplePlay (SOS review, user guide), Roland GR-20 and GR-50 documents, Jamstik support | https://www.soundonsound.com/reviews/fishman-tripleplay, https://cdn.roland.com/assets/media/pdf/GR20WS03.pdf, https://support.roland.com/hc/en-us/articles/201921239, https://support.jamstik.com/hc/en-us/articles/360061090831 | snippet |
| **[PAT]** | Google Patents: US 4,649,783 (Strong, Karplus; Stanford) and US 4,984,276 (Smith; Stanford) | https://patents.google.com/patent/US4649783A/en, https://patents.google.com/patent/US4984276A/en | part |
| **[MKT]** | Market sources, each cited inline in §6 (vendor pages, reviews, forum threads) | §6 | mixed |

---

## 0. Summary of recommendations

**The string** (§1). A plucked string is a pair of travelling waves on a delay loop closed by lossy, slightly
dispersive terminations. The Karplus-Strong loop [KS83] and its extensions [JS83] (pick-direction lowpass,
pick-position comb, damping filter, stiffness allpass, tuning allpass, dynamic-level lowpass; formulas in
[PASP Extended_Karplus_Strong_Algorithm]) are a special case of the digital waveguide, and the single-delay-loop
(SDL) form [KVT98] is the efficient way to run one string: one delay line, one loss filter, one fractional
delay, a dispersion chain, with the pluck and pickup positions moved outside the loop as combs. The electric
guitar adds facts that the acoustic literature underplays: the pickup is far more sensitive to the
out-of-plane polarisation than to the in-plane one, the pickup adds no damping, and decay is dominated by the
string's own losses plus coupling to the neck, which produces dead spots more than 2 s shorter than their
neighbours [PLF14]. Stiffness makes partial n sharp by a factor sqrt(1 + B n²) [FL64]; with real gauges and
tensions from [DAD], B runs from about 1·10⁻⁵ (plain high E) to 2·10⁻⁴ (wound low E) on open strings and
quadruples at the 12th fret (calculated, §1.11). Tension modulation makes the attack sharp and glide down
[TVK00], [PASP Tension_Modulation]; fret and finger collisions make buzz [EE10], [BT15], and [WLG] points to
fret buzz as the reason light strings sound brighter.

**What we learn from open code** (§1.12, §1.13). STK's `Guitar` class runs N `Twang` strings, sums their outputs
through a one-pole filter with a gain of 0.01/N and feeds that back into every string: the simplest bridge
coupling there is [STK]. Faust's `elecGuitar` is a two-segment waveguide with one-pole "dispersion"
smoothers (not allpasses), two-zero FIR terminations whose loss is set from a T60 and a noise-burst pluck
[FPM]. Both are permissively licensed (STK: MIT-style; Faust libraries: LGPL-2.1+ with an exception for
compiled output) but **we copy neither** (§9).

**Pickups and passive electronics** (§2). A magnetic pickup senses string velocity over an aperture a few
millimetres wide; its position makes a per-note comb (notches at the harmonics that have a node over the
pickup), and a humbucker is the sum of two such combs at slightly different positions. The coil (single coil
1.8-2.5 H and 5.5-6.5 kΩ; humbucker 2.8-4.6 H and 6.5-12+ kΩ [VV], [SD-DCR]) resonates with its own and the
cable's capacitance (52-200 pF/m [ESP214]) into the pot load (250 kΩ for single coils, 500 kΩ for humbuckers
[GEW]): a 2.4 H coil into 600 pF peaks near 4.2 kHz, a 4.4 H humbucker into 800 pF near 2.7 kHz (calculated).
The tone pot and its capacitor (0.022 or 0.047 µF [GEW]) pull that resonance down to a few hundred hertz at
zero. We solve this network as an analogue transfer function and discretise it whenever a control moves.

**Amplifier, pedals, cabinet** (§3, §4). The tube-amp literature converges on a chain of triode stages with
coupling capacitors, cathode bypass and grid conduction, a passive tonestack, a push-pull power stage inside a
negative-feedback loop with presence and resonance, an output transformer, a sagging supply and a speaker
[PY09]. The FMV tonestack has a closed-form third-order transfer function [YS06], and 25 published component
sets exist [FTS]. Aliasing is handled by antiderivative antialiasing (ADAA) [PZL16], [BEPV17] where a
nonlinearity is memoryless and by oversampling where it is not. Pedals: compressor, gate, mid overdrive
(op-amp with diodes in the feedback after a ~720 Hz high-pass [TS-UO]), hard-clipping distortion, two-transistor
fuzz [ES], inductor wah [HZ11], BBD modulation [RS10], OTA/JFET phaser [EFHZ14], digital/BBD/tape delay,
spring reverb [VPA10], [PB09] and room reverb.

**Realism** (§5). What makes users say "fake": every note the same, a mod-wheel vibrato that is identical on
every note, no hand noises, chord changes with no travel time, strums with no per-string spread, and bends that
need hand editing to sound human [MKT: Audio Masterclass, Gearspace]. The measurements we have: a six-string
strum spreads over 26-62 ms at moderate tempo and 37-46 ms slowly, block chords over 5-18 ms, arpeggios over
64-202 ms; on up-strokes two of three players hit fewer strings, and the spread varies by a factor of two
between players [FAVS18]. Finger noise on wound strings is a pitched
zipper whose harmonics and level scale linearly with slide speed [PPB07].

**Market** (§6). Sample libraries (Ample Guitar, Session Guitarist, Shreddage, RealStrat, Evolution, Prominy,
Virtual Guitarist) win on recorded realism and lose on disk size, keyswitch complexity, repetition and rigid
patterns; modelled instruments (AAS Strum GS-2, MODO Bass for bass) win on size and flexibility and are judged
weaker on leads and amp sound [MKT]. No free, open instrument combines a modelled string, a real fingering
engine, a strummer with measured timing, MPE and channel-per-string input, MIDI out of what was played and a
full amp chain.

**DSP** (§8). One SDL waveguide per string (six to seven, plus voice stealing per string, never across
strings) with two polarisations, a one-pole-plus-gain loss filter fitted to T60(f), four first-order dispersion
allpasses fitted per note, a 4-point Lagrange fractional delay for bends, slides and vibrato, a global
tension-modulation glide, an optional one-sided fret-buzz limiter in the loop and the pluck-position comb on
the excitation. A lossy bridge resonator sums all strings and feeds them back (sympathetic resonance); body
modes depend on construction. Pickups: per-string comb and aperture, velocity sensing, a passive network
solved per control change by the bilinear transform. Pedals with ADAA at the base rate (zero latency); the amp
at 2x/4x/8x with the repo's linear-phase half-band chain (`shared/dsp/Oversampling.h`), its fixed latency
reported to the host and the DI output delayed to match; FMV tonestack; power stage with sag and presence; a
cabinet built from our own speaker-and-microphone model rendered to a minimum-phase IR, our own IR set and
user IR loading; reverbs and delays reused from Halation, Tarnish, AT700 and Lacquer.

**Performance layer** (§8.6-§8.9). A deterministic fingering engine (dynamic programming over string/fret
candidates with span, shift, open-string and string-preference costs), template chord detection and voicing
search over the real tuning, a tempo-synced strummer with per-string spread and a pattern library, a
fingerpicking arpeggiator, articulations from keyswitches, velocity, CCs, MPE or per-string channels, human
variation seeded by a hash of the event index (bit-identical on every render), hand noises, and MIDI out (and
`.mid` export) of what the virtual guitarist actually played.

**Tests** (§10): 24 automated checks (tuning against the stiff-string formula per string and fret, fingering
golden files, strum timing across buffer sizes, determinism, MIDI/MPE round trips, latency declared = measured,
no NaN/DC/denormals, CPU on the Apple M4 and the Ryzen 9 5900XT), host tests (athosttest, pluginval, auval)
and listening criteria. **Open questions** (§11): 18.

---

## 1. Plucked-string physics and models

### 1.1 The ideal string and the digital waveguide

- The ideal string's transverse displacement is the sum of two waves travelling in opposite directions at
  speed c = sqrt(T/μ) (T tension, μ mass per unit length). A digital waveguide samples those two travelling
  waves and stores them in two delay lines; terminations reflect them with sign inversion and filtering
  [PASP Digital_Waveguides], [PASP Ideal_Vibrating_String].
- For a string of length L fixed at both ends, the round trip is 2L/c = 1/f0, so f0 = c/(2L) = (1/(2L))
  sqrt(T/μ). This is the relation D'Addario's tension formula inverts: T = UW·(2·L·F)²/386.4, with UW in lb/in,
  L in inches, F in Hz and T in lb [DAD p3]; 386.4 is g in in/s².
- Losses and dispersion are distributed along a real string, but for a linear, time-invariant string they
  commute with the delays and can be lumped into one filter per round trip [PASP Digital_Waveguide_Models].
  That lumping is what makes the method cheap: one delay of about fs/f0 samples per string (583 samples for a
  low E at 48 kHz; 36 for a high E at the 24th fret, calculated).
- The Karplus-Strong loop (a delay line refilled through a two-point average, H(z) = 0.5 + 0.5 z⁻¹, excited
  with noise) is the special case with a one-zero loss filter and a random initial shape [PASP
  Karplus_Strong_Algorithms]. Smith recognised that the filtered delay loop has the transfer function of an
  idealised vibrating string, which is how KS became a special case of the waveguide [PASP], [KVT98].

### 1.2 The extended Karplus-Strong algorithm (Jaffe and Smith 1983)

Jaffe and Smith added musical control over tuning, brightness and dynamic level, which David Jaffe needed for
*Silicon Valley Breakdown* (1982) [PASP Karplus_Strong_Algorithms]. The components, as Smith lists them with N
the period in samples [PASP Extended_Karplus_Strong_Algorithm]:

| Block | Transfer function | Meaning |
|---|---|---|
| Pick-direction lowpass H_p | (1 − p)/(1 − p z⁻¹) | p = 0 for one pick direction, 0 < p < 1 for the other |
| Pick-position comb H_β | 1 − z^(−⌊βN + 1/2⌋) | β ∈ (0, 1) the normalised pick position |
| String damping H_d | one or two poles/zeros, \|H_d\| ≤ 1 | sets decay vs frequency |
| String stiffness H_s | allpass with several poles and zeros | inharmonicity |
| Tuning allpass H_η | −(η − z⁻¹)/(1 − η z⁻¹) | first-order allpass for the fractional delay; η ∈ [−1/11, 2/3] covers delays of 0.2-1.2 samples, and the delay can be offset to [ε, 1 + ε) to avoid near-zero delays |
| Dynamic-level lowpass H_L | (1 − R_L)/(1 − R_L z⁻¹), R_L = e^(−πLT) | L the bandwidth in Hz, T the sample period: softer notes are darker |

What we keep (inferred): the comb (outside the loop, on the excitation; §8.1), the pick-direction filter
(re-cast as a physical pick model, §8.2), a real loss filter fitted to T60 instead of a fixed average, a
dispersion chain, and a Lagrange interpolator instead of the first-order allpass, because the allpass's state
transients click when the delay is modulated quickly (bends, vibrato, slides) and Lagrange FIR interpolation
has no recursive state [LVKL96] (the trade-off is a mild, delay-dependent lowpass, which we absorb in the loss
filter design).

### 1.3 The single-delay-loop form and the loss filter

- [KVT98] (abstract) derives KS from the waveguide and shows the equivalent single-delay-loop (SDL) form: the
  bidirectional string with the excitation point and the output point inside it reduces to one delay loop with
  a loop filter, preceded by an excitation-position comb and followed by an output-position comb, as long as
  everything is linear and time-invariant. [VHKJ96] (abstract) describes the real-time plucked-string models of
  the same group, with fractional-delay tuning and loop filters estimated from recordings; the design details
  are in the paywalled paper.
- The SDL is what makes six strings with two polarisations, pickups and per-note combs cheap enough to leave
  CPU for the amp (inferred). Its limitation is the same as commuted synthesis: anything nonlinear inside the
  string (collisions, tension modulation) must be expressed as a modulation of the loop parameters or as a
  nonlinearity applied at one point of the loop (§1.8, §1.9).
- **Loss filter from T60 (inferred derivation).** For a loop of N samples (one period), a partial at f must
  lose 60 dB in T60(f), so the loop gain per period is G(f) = 10^(−3/(f0·T60(f))). A one-pole filter with DC
  gain g, H(z) = g(1 + a)/(1 + a z⁻¹), −1 < a < 0, gives G(0) = g and a monotone high-frequency loss; solving
  at two frequencies (f0, where T60 is longest, and a "brightness" frequency such as 8 f0 or 3 kHz) gives g and
  a in closed form. Example (calculated): a low E with T60 = 8 s at f0 needs g = 0.98958 per period; a high E
  with T60 = 3 s needs 0.99304. The one-pole form is the one used throughout the Helsinki plucked-string
  papers; I did not read [VHKJ96] to confirm its exact parameterisation, so the code derives it from scratch.
- The fractional delay, the dispersion allpasses and the loss filter all add phase delay at f0; the delay line
  length is N_d = fs/f0 − τ_loss(f0) − τ_disp(f0) − τ_frac, which is how STK's `Twang` subtracts
  `loopFilter_.phaseDelay(frequency)` from fs/f0 [STK Twang.cpp]. Tuning must be checked per partial, not just
  at f0, because dispersion moves every partial differently (§10 T2).

### 1.4 Commuted synthesis

- If string and body are linear and time-invariant they commute, so the body's impulse response can be
  convolved with the excitation once and stored as the "aggregate excitation": the body filter becomes an
  inexpensive lookup table. Long tables can be shortened by converting to minimum phase and applying an
  exponential window, which uniformly increases resonator damping [PASP Commuted_Synthesis]. The method is
  credited to Smith (1993) and to Karjalainen, Välimäki and Jánosy (1993) [KVJ93], [PASP].
- For a solid-body electric guitar the body's acoustic radiation is irrelevant (the pickup reads the string),
  so commuted synthesis is useful for two things only (inferred): (a) the semi-hollow and hollow types, where a
  short body-and-air excitation adds the acoustic thump that reaches the pickup through the top and the
  "unplugged" sound used as a blend; (b) pick and finger transients, recorded once (by us, from our own
  instruments) or synthesised, stored as short excitation tables per pick and per articulation.
- What commuted synthesis cannot do (inferred): string-to-string coupling, collisions, tension modulation and
  any interaction between a ringing string and a new pluck on it. Those need the explicit model of §8.

### 1.5 Stiffness, inharmonicity and dispersion filters

- A stiff string's partials follow f_n = n f0 sqrt(1 + B n²) (pinned ends) [FL64]. With the second moment of
  area I = π d⁴/64, Paté et al. write the coefficient as b = E I π²/(T L²) [PLF14 eq. 5], i.e.
  **B = π³ E d⁴ / (64 T L²)** (d the diameter of the part that bends, E Young's modulus). [PLF14 Table I]
  measured a plain G3 string of 0.432 mm on a 0.629 m scale at 69.43 N and used E = 2·10¹¹ Pa for steel; that
  string's open B is 1.2·10⁻⁴ (calculated).
- Wound strings: the winding adds mass but little bending stiffness, so their B is set mostly by the core
  diameter [search summary of string-maker patents, e.g. US 5,817,960]; hex cores are reported to be more
  inharmonic than round cores (inside-guitar.com, snippet). The core diameters of commercial strings are not
  published in [DAD]; §1.11 uses assumed cores and marks them.
- Audibility (calculated): with B = 1.6·10⁻⁴ (wound low E, open) partial 10 is 13.7 cents sharp and partial 20
  is 54 cents sharp; at the 12th fret (B ×4) partial 10 is 54 cents sharp. With B = 1.3·10⁻⁵ (plain high E)
  partial 10 is only 1.1 cents sharp. Inharmonicity therefore matters for the wound strings, for high frets
  and for distorted sounds, where intermodulation between stretched partials is what makes a stiff string
  sound "metallic" rather than synthetic (inferred).
- **Filter design.** The dispersion filter is an allpass in the loop whose group delay decreases with
  frequency. Van Duyne and Smith proposed a simplified approach with cascaded first-order allpasses for strings
  and plates [VDS94]. [PASP Dispersion_Filter_Design] surveys least-squares allpass design (Lang and Laakso),
  Bark-warped fits that tune "the first several tens of partials" of recorded piano strings with a total
  allpass order of 20 or less, and closed-form designs parameterised by inharmonicity. Rauhala and Välimäki
  give a dynamically tunable design based on Thiran's closed-form formulas and apply it to a cascade of
  first-order allpasses [RV06]. Abel, Välimäki and Smith design a cascaded-biquad allpass directly from the
  target group delay, placing pole frequencies where the group-delay area reaches odd multiples of π with radii
  from a smoothness parameter; it is numerically robust at very high orders [AVS10]. The CCRMA wiki hosts a
  MATLAB `adf.m` with parameters f0, B, design bandwidth and smoothing β = 0.85 [AVS10] (licence not stated;
  not used).
- What we choose (inferred): four identical first-order allpasses per string (the [RV06] structure), with the
  coefficient computed per note from B and f0 by a small table fitted offline (B × f0 grid, 2-D interpolation),
  then the delay line shortened by their phase delay at f0. Four sections are enough to place the first
  ~15-25 partials of a guitar string within a few cents (to verify in §10 T2); [AVS10] is the reference for an
  offline "Deep" quality with 8-16 biquads if listening shows the need.

### 1.6 Two polarisations, beating and what the pickup hears

- A string vibrates in two transverse planes: parallel to the top (in-plane, "y") and perpendicular to it
  (out-of-plane, "z"). They see different bridge and nut admittances, so they decay at different rates and can
  have slightly different frequencies; their sum beats, and the decay curve shows a "double decay" (fast
  initial, slower tail). [PLF14] measures separate Q factors and dead spots for the two polarisations of one
  string (below); the two-loop treatment in the waveguide literature ([KVT98] and the Helsinki papers) was not
  checked in full text.
- For the electric guitar, [PLF14] found the pickup "far more sensitive to the out-of-plane polarization of
  the string" (abstract): with each polarisation excited separately (wire-breaking, the other polarisation at
  least 10 times smaller), the pickup's output waveform resembles the z polarisation, not the y one [PLF14
  §IV, Fig. 10]; in normal playing, a high-amplitude (z) and a low-amplitude (y) component are identified in
  the pickup signal, each with its own dead spots (9th fret for y, 12th for z on the G3 string) [PLF14
  Fig. 11]. The pickup is a velocity sensor (they cite [HM09]) and adds no damping (abstract).
- Consequence for the design (inferred): two loops per string with a common excitation split by the pick
  angle (a pick moving parallel to the top excites mostly y, a pick or finger pulling up or pushing down
  excites z); different loss filters (z couples to the neck and bridge more strongly for a solid body: §1.7)
  and a detune of 0-0.5 cents; the pickup reads z at full weight and y at about −20 dB. That keeps the beating
  and double decay without the "chorus" sound that equal mixing would give.

### 1.7 Bridge coupling, sympathetic strings, body modes and dead spots

- [PLF14] identifies two damping mechanisms in a solid-body electric guitar: the string's intrinsic damping,
  and damping by mechanical coupling to the **neck** (not the bridge, which they found much stiffer than the
  fret end). Decay time is predicted from the isolated-string damping and the out-of-plane conductance at the
  neck; where a string partial meets a neck mode the decay collapses: at fret 12 of the G3 string the measured
  T30 is more than 2 s shorter than at its neighbours (a "dead spot"), and their synthesis predicts it within
  0.6 s [PLF14 §V, Fig. 12]. Dead spots of electric guitars are also the subject of Fleischer and Zwicker's
  work (Acta Acustica 85(1), 1999; title and journal from https://www.ingentaconnect.com/contentone/dav/aaua/
  1999/00000085/00000001/art00020, snippet).
- Sympathetic resonance: strings coupled through a common, slightly mobile termination exchange energy, so
  open strings ring when a note shares partials with them. STK's `Guitar` does this with one global path:
  every sample the summed string outputs pass through a one-pole filter (pole 0.9) with gain 0.01/N and are
  added to the input of every string [STK Guitar.h `tick`, Guitar.cpp]. It is crude (one coupling point, no
  frequency dependence beyond one pole) but shows the topology.
- Our design (inferred): a "bridge/neck resonator" made of a few damped modes (4-8 two-pole resonators per
  guitar type: neck modes for dead spots, body modes for semi-hollow and hollow types) driven by the sum of the
  z-polarisation forces of all strings and added back, scaled and filtered, into every loop. The loop gain of
  the whole network must stay below one; with coupling gains of order 10⁻³-10⁻² and lossy resonators, a
  per-block energy check suffices (§10 T6). Muted strings (the left hand on them) receive the coupling but with
  a loss filter of a few tens of milliseconds, so they "thud" without ringing.

### 1.8 Tension modulation and pitch glide

- A large vibration stretches the string, raising the mean tension and therefore the pitch; as the vibration
  decays the pitch falls back. The effect is most audible at the attack of low-tension strings [PASP
  Tension_Modulation]. Waveguide implementations are either **local** (each section's delay modulated with a
  first-order allpass according to local slope or force) or **global** (the total loop delay modulated as a
  function of the total energy of the string); the global methods are attributed to [TVK00] and Tolonen's
  1998 work [PASP Tension_Modulation]. Tension modulation also generates "missing" harmonics and longitudinal
  components (the 2f0 pulsing of the tension), described in the same family of papers (abstract level).
- Our design (inferred): the global form. Each loop keeps a running estimate of its energy (sum of squares of
  the delay-line contents, updated incrementally per sample); the delay is shortened by Δ = κ·E, with κ per
  string set so that a hard pluck starts 5-15 cents sharp on a light plain string and 2-5 cents on a wound one
  (starting values; to be matched to recordings). The 4-point Lagrange interpolator (§1.10) carries the
  modulation without clicks.

### 1.9 Collisions: fret buzz, fingers, slides

- Evangelista and Eckerholm model the player's touch and collisions in a digital waveguide guitar, revising
  the Cuzzucoli-Lombardo pluck model whose inconsistency they remove "by introducing the finger/pick
  perturbation in a balanced form on the digital waveguide" [EE10] (DAFx-08 abstract). Germain and Evangelista
  modelled the plectrum (WASPAA 2009, snippet).
- Bilbao and Torin simulate the fretted string with a finite-difference scheme in which a penalty potential
  models distributed collisions with the fretboard and fingers, within an energy-balanced framework; the
  finger position and force are gestural parameters [BT15]. It is the reference for accuracy and far too
  expensive for six real-time strings with an amp (inferred).
- [WLG] (abstract): neither bending stiffness, frequency-dependent damping nor transverse-axial coupling explain
  why low-tension strings sound brighter; measurements "strongly suggest" nonlinear interaction with other
  parts of the structure such as the frets, because lower-tension strings are played at larger amplitude and
  buzz more. This is the best published argument for having a buzz model at all.
- The slide guitar model of [PPV08] (abstract) is a waveguide with extensions that generate the contact sounds
  of the slide tube on the strings, plus a parametric model of the tube-string contact noise.
- Our design (inferred): (a) **fret buzz** as a one-sided soft limiter on the z-polarisation inside the loop
  at one point near the fretting position: the string displacement estimate (integrated velocity) is
  compared with a "fret clearance" that depends on action, fret and string; excursions beyond it are clipped
  softly and the removed energy is injected as a short broadband click into the loop and the pickup path.
  Default off for clean presets, low for "low action", and automatically raised for hard plucks on low frets;
  (b) **finger damping** (palm mute, left-hand mute, harmonics) as a localised loss: a second, short lossy
  section at the touch point, modelled in SDL form as a frequency-dependent extra loss plus a comb for
  harmonics (§5.5); (c) **slides** as a delay-length trajectory plus a contact-noise generator whose
  spectrum is set by the winding pitch and the slide speed [PPB07] (§5.2).

### 1.10 Fractional delay for tuning, bends, slides and vibrato

- [LVKL96] reviews FIR (Lagrange, least squares) and allpass (Thiran) fractional-delay filters, emphasising
  methods suited to fast coefficient updates and continuous control of the delay; musical synthesis is one of
  its applications (abstract). MathWorks' Lagrange fractional-delay filter documentation cites it as its
  reference (https://au.mathworks.com/help/dsp/ref/lagrange.html, snippet).
- Our choice (inferred): third-order (4-point) Lagrange with the fractional part kept between 1 and 2 samples
  (where its magnitude response is flattest), recomputed every sample during bends and slides. Its
  high-frequency droop at fractional delays near 1.5 (a few dB near Nyquist at 48 kHz) is compensated by
  running each string at the host rate and treating the droop as part of the loss filter, which is re-fitted per
  note; bends of a few semitones change it by an amount we measure in §10 T3 and correct if audible. Faust's
  `waveguide` uses a fourth-order fractional delay (`waveguideFd4`) for the same reason [FPM].

### 1.11 Measured values: gauges, unit weights, tensions, B, decay, scales and frets

**Unit weights and tensions.** D'Addario's chart gives the formula of §1.1 and unit weights per item, with all
guitar tensions computed at 25.5 in [DAD p3-4]. Selected unit weights (lb/in) [DAD]: plain steel PL009
0.00001794, PL010 0.00002215, PL011 0.00002680, PL013 0.00003744, PL014 0.00004342, PL016 0.00005671,
PL017 0.00006402; nickel wound NW024 0.00010857, NW026 0.00012671, NW032 0.00019347, NW036 0.00023964,
NW042 0.00032279, NW046 0.00038216, NW049 0.00043014, NW052 0.00048109, NW054 0.00053838, NW059 0.00064191,
NW062 0.00070697, NW064 0.00074984. Tensions computed from them (calculated; pounds):

| Set (scale) | E4 | B3 | G3 | D3 | A2 | E2 | Total |
|---|---|---|---|---|---|---|---|
| 9-42 (25.5 in) | 13.1 | 11.0 | 14.7 | 15.8 | 15.8 | 14.8 | 85.1 |
| 10-46 (25.5 in) | 16.2 | 15.4 | 16.6 | 18.4 | 19.5 | 17.5 | 103.5 |
| 11-49 (25.5 in) | 19.6 | 17.8 | 16.6 | 18.4 | 19.5 | 19.7 | 111.6 |
| 10-46 (24.75 in) | 15.3 | 14.5 | 15.6 | 17.3 | 18.4 | 16.5 | 97.5 |

The 10-46 figures at 25.5 in match the per-string tensions that retailers reprint for D'Addario's EXL110 set
(16.2/15.4/16.6/18.4/19.5/17.5 lb on https://player.rs/gitare/zice/zice-za-elektricnu-gitaru/d-addario-exl110-
nickel-wound--10-46; another retailer lists 19.04 and 16.91 for the two lowest strings), which confirms the
calculation. Extended ranges (calculated): a 7-string low B1 with NW059 at 25.5 in is 16.5 lb; a baritone in B
standard (B1 E2 A2 D3 F#3 B3) with 13-17-26-36-46-62 is 17.2-21.9 lb at 27 in and 18.6-24.6 lb at 28.625 in;
a jazz set 12-52 with a wound G (NW024) at 24.75 in puts 26.4 lb on the G, 22.0 on the high E and 20.7 on the
low E (flatwound weights differ; [DAD] lists them separately and we have not tabulated them).

**Inharmonicity per string (calculated, open strings, E = 200 GPa; wound strings with an assumed core of about
0.4 times the outer diameter, marked \*)**:

| Set, scale | E4 | B3 | G3 | D3\* | A2\* | E2\* |
|---|---|---|---|---|---|---|
| 10-46, 25.5 in | 1.3·10⁻⁵ | 4.0·10⁻⁵ | 1.1·10⁻⁴ | 2.4·10⁻⁵ | 7.3·10⁻⁵ | 1.6·10⁻⁴ |
| 10-46, 24.75 in | 1.5·10⁻⁵ | 4.5·10⁻⁵ | 1.2·10⁻⁴ | 2.7·10⁻⁵ | 8.2·10⁻⁵ | 1.8·10⁻⁴ |
| 9-42, 25.5 in | 1.1·10⁻⁵ | 2.9·10⁻⁵ | 9.7·10⁻⁵ | 2.4·10⁻⁵ | 5.3·10⁻⁵ | 1.5·10⁻⁴ |

Two consequences (inferred): the plain G3 is the most inharmonic of the treble strings (thick plain wire at
low tension), which is the physical reason many players dislike a plain G in heavy sets and jazz players use a
wound G; and B scales as 1/L², so it quadruples at the 12th fret and is ×16 at the 24th if tension stays
constant. A measured B per string from our own recordings is an open item (§11).

**Decay.** Measured decay data for electric guitars are scarce. [PLF14] reports T30 per fret of the G3
string with dead spots; the isolated-string Q factors are in their §III. A Leicester undergraduate study of an
Ibanez RG570 model gives sustain time constants (2τ) of about 7.8 s for the low E and about 1.1 s for the high
E, with body coupling roughly halving the high E (https://journals.le.ac.uk/index.php/pst/article/download/
3678/3191/10415, snippet). Fleischer quantifies sustain as a 30 dB drop rather than 60 dB (snippet). Starting
values for our presets (inferred, to fit to recordings): open-string fundamental T60 of 10-20 s on the wound
strings and 4-8 s on the plain ones for a solid body, shorter for hollow bodies and for fretted notes, with a
loss filter that halves T60 every octave or so above 1 kHz.

**Scales and frets.** Common scales: 25.5 in (647.7 mm; Fender lists it for the American Performer
Stratocaster, https://shop.fender.com/en-AU/electric-guitars/stratocaster/american-performer-stratocaster/
0114910342.html), 24.75 in (628.65 mm) and baritone scales of 27-30 in (inferred from product ranges; 28.625
in is a common baritone figure, unverified). Fret n sits at d_n = L(1 − 2^(−n/12)) from the nut (equal
temperament): on 25.5 in, fret 1 at 36.4 mm, fret 5 at 162.5, fret 7 at 215.4, fret 12 at 323.8 and fret 24
at 485.8 mm (calculated). Real guitars compensate intonation at the saddle (slightly longer than L for the
thicker strings); we model the saddle offset per string as a small extra delay (inferred).

### 1.12 STK: Plucked, StifKarp, Twang and Guitar

Licence: MIT-style permission notice, "Copyright (c) 1995-2023 Perry R. Cook and Gary P. Scavone", with a
non-binding request to send modifications back [STK LICENSE] (§9). Every class header warns that it is "a
digital waveguide model, making its use possibly subject to patents held by Stanford University, Yamaha, and
others" [STK]. The two Stanford patents named most often have expired: US 4,649,783 (Strong and Karplus,
"Wavetable-modification instrument and method for generating musical sound", filed 1984, granted 17 Mar 1987,
"Expired - Lifetime", anticipated expiry 17 Mar 2004) and US 4,984,276 (Smith, "Digital signal processing
using waveguide networks", filed 1989, granted 8 Jan 1991, anticipated expiry 8 Jan 2008) [PAT].

What each class does [STK sources, read]:
- `Plucked`: the basic KS string; for a more advanced string the header points to `Twang`.
- `StifKarp`: KS with "Jaffe-Smith, Smith, and others" enhancements. The delay is fs/f − 0.5; the loop gain is
  `baseLoopGain + f·0.000005` capped at 0.99999 (higher notes lose less per period); stiffness is **four
  second-order allpass sections** (b0 = a2 = r², b1 = a1 = −2r cos ω, b2 = 1) with r = 0.5 + 0.5·stretch and
  centre frequencies starting at 2f0 and spaced by a quarter of the distance to Nyquist; the pickup-position
  comb delay is 0.5·position·period; the pluck fills the delay line with noise mixed with its current content
  (0.6 old + 0.4 new). CC 1 maps to "stretch", CC 4 to pickup position, CC 11 to sustain.
- `Twang`: the "enhanced plucked string" with a pluck-position comb; the loop filter starts as a two-tap
  average and its phase delay at f0 is subtracted from the delay line; `tick()` adds an input sample to the
  delay line input, so it supports commuted synthesis (an excitation from a body response) and feedback.
- `Guitar` (Scavone, 2012): N `Twang` strings; excitation either from a body impulse-response file (commuted
  synthesis, "as discussed by Smith and Karjalainen") or from noise with raised-cosine fades over the first
  and last 20 % of the table, filtered by a one-pole "pick filter" (pole 0.95, CC 128); the bridge coupling of
  §1.7; a string switches off after 0.1 s below 0.001; plucks with gain below 0.2 let the string ring
  without exciting it.

What we learn (inferred): the topology (shared coupling path, per-string loop, excitation table) and the
practical guards (string switch-off, delay compensation), not the numbers, which are ad hoc and tuned by ear.
The stretch-allpass design is not fitted to B; we fit ours to the stiff-string formula.

### 1.13 Faust physmodels.lib: elecGuitar, guitar and nylonGuitar

Licence of the file: "Except where noted otherwise, Copyright (C) 2016-2026 by GRAME", **LGPL 2.1 or later**,
with a GRAME exception allowing the compiled output of a Faust program that imports the library to be
distributed under any licence [FPM header]. No section of the file is marked STK-4.3 or MIT in the part read;
STK is cited only as the reference for `bridgeFilter` (§9).

What the guitar models do [FPM, read]:
- **Waveguide primitives.** `chain` connects bidirectional blocks; `stringSegment(maxLength, length)` is a
  `waveguide` built on a fourth-order fractional delay; terminations close the ends.
- **`openStringPick(length, stiffness, pluckPosition, excitation)`**: two string segments (upper
  length·pluckPosition, lower length·(1 − pluckPosition)) with the excitation injected and the output taken at
  the same point, and "dispersion filters" that are `si.smooth(stiffness)` (one-pole smoothers on the two
  travelling waves): a lowpass, not an allpass, so the model has frequency-dependent loss but no real
  inharmonicity (inferred from the code).
- **`elecGuitarModel(length, pluckPosition, mute, excitation)`**: nut and bridge terminations with
  reflectance −`bridgeFilter(0.8, 0.6)`, a hand-tuned length correction of 0.11 m ("tuned by hand"), stiffness
  0.05, and a mute that scales both travelling waves. `bridgeFilter(brightness, absorption)` is a symmetric
  three-tap FIR ρ·(h0 x[n−1] + h1 (x[n] + x[n−2])) with h0 = (1 + brightness)/2, h1 = (1 − brightness)/4 and
  ρ = 0.001^(1/(320·t60)), t60 = (1 − absorption)·20: decay is set as a T60 referred to a 320 Hz reference.
- **`elecGuitar(stringLength, pluckPosition, mute, gain, trigger)`** pipes `pluckString` into
  `elecGuitarModel` (partial application: the excitation is the input signal). `pluckString` is white noise
  through a second-order lowpass at 5·f0·cutoff with an attack-release envelope whose time shrinks as f0 rises:
  att = 0.002·sharpness·(1 − f0/(2000·maxFreq))².
- **`guitarModel`** (steel-string acoustic) and **`nylonGuitarModel`** use the same structure with
  `bridgeFilter(0.4, 0.5)` (darker) and stiffness 0.05 and 0.4; the docs warn that `guitarBody` is not
  implemented (pass-through placeholders).

What we learn (inferred): the T60-referred termination is a tidy way to expose decay as a musical parameter
(we do the same with a two-frequency fit); the pluck-position split is the waveguide-native version of the
EKS comb; the noise-burst pluck with pitch-dependent length is a cheap baseline we will improve on with a
pick model (§8.2). The missing allpass dispersion and body are the gaps.

### 1.14 What the models leave out and where we go further (inferred)

| Phenomenon | KS/EKS | STK | Faust | Thrum (§8) |
|---|---|---|---|---|
| Inharmonicity fitted to B | allpass (unspecified) | 4 biquad "stretch", not fitted | none (one-pole smoothers) | 4 first-order allpasses fitted per note |
| Two polarisations | no | no | no | yes, pickup-weighted |
| Pick position | comb | comb | segment split | comb on excitation, per note |
| Pickup position and aperture | no | "pickup" comb only | output at pluck point | per-string comb + aperture lowpass + coil network |
| Bridge coupling / sympathetic | no | one global path | no | modal resonator, all strings |
| Tension modulation | no | no | no | global, energy-driven |
| Collisions / buzz | no | no | no | one-sided limiter (optional) |
| Bends, slides, vibrato | allpass tuning | linear interpolation | 4th-order FD | 4-point Lagrange, per sample |
| Same-string retrigger | — | external voice management | — | new note on a string cuts the old one, with a damped transition |

---

## 2. Pickups and passive electronics

### 2.1 How a magnetic pickup senses the string

- The pickup is a coil around magnetised pole pieces; the moving steel string modulates the flux through the
  coil, and Faraday's law makes the voltage proportional to the rate of change of flux, i.e. (for small
  motion) to the string's **velocity** over the pole piece. Horton and Moore model the magnet and the
  ferromagnetic wire as surfaces of magnetic charge and predict the field change caused by the wire "with high
  accuracy"; they note that textbooks present the pickup as a Faraday-law example without discussing its
  distortion [HM09] (abstract). [PLF14] confirms the velocity sensitivity and the preference for the
  out-of-plane polarisation (§1.6).
- The flux is a nonlinear function of the string-to-magnet distance, so large vibrations are distorted
  ("distortion from distance-dependent magnetic flux") [PPV12] (abstract); an experimental study of pickup
  nonlinearity exists at DAFx (https://dafx.de/paper-archive/details/JCqRtbANQpElhfD0MhcwmA, title only).
- The pickup does not read one point: it averages the string motion over a **sensing aperture** set by the
  pole piece and field shape. [PPV12] covers "the pickup's position and sensitivity width" and wave dispersion
  in a steel string (abstract). Forum paraphrases of [ZOL] say a stronger magnetic field shortens the effective
  aperture and slightly raises the treble, and that the aperture length is what lets the comb response be
  computed (https://music-electronics-forum.com/forum/instrumentation/pickup-makers/pickup-theory/990050-
  magnets-in-pickups, forum; not checked against the book).

### 2.2 Position comb, aperture and humbuckers

- **Position comb (inferred derivation, standard modal argument).** For a string of vibrating length L and a
  pickup at distance x from the bridge, mode n contributes in proportion to sin(nπx/L). Notches fall at the
  harmonics n = kL/x. A pickup 40.5 mm from the saddle on a 647.7 mm scale (the "48th-fret" position, scale ×
  15/16 from the nut, that one forum poster reports for a Fender-style bridge pickup:
  https://unofficialwarmoth.com/threads/bridge-humbucker-placement.32956, forum) notches the 16th harmonic of
  an open string; a neck pickup near a quarter of the scale notches the 4th, 8th, 12th... (calculated). When
  the string is fretted, L shrinks and the notches move: this is why the comb must be **per string and per
  note**, not a fixed filter on the mix (inferred). The "Mottola" and "Lemme" treatments the brief mentions
  could not be checked; the derivation above is elementary.
- **Aperture.** Averaging over a width w adds a sinc-shaped lowpass in the spatial frequency of each mode:
  partial n is weighted by sinc(nπw/(2L)) roughly (inferred; [PPV12] has the measured version).
- **Humbucker.** Two coils with opposite magnetic polarity and opposite winding: the string signals add while
  external hum (which induces the same voltage in both coils) cancels. Acoustically, the output is the sum of
  two combs at two positions a coil-spacing apart, which smooths the comb and removes some treble (inferred).
  The spacing between the two pole rows of a full-size humbucker is not in any source we read; the design uses
  ±9 mm around the centre as a starting value to be measured (§11).
- **Pickup mixing.** Two pickups selected together sum (parallel) and their combs interfere; out of phase,
  their difference cancels the low partials common to both and gives the thin, hollow sound (inferred;
  [PPV12] covers "mixing several pickups", abstract).

### 2.3 Coil inductance, resistance and capacitance

| Pickup type | Inductance (H, at 1 kHz) | DC resistance (kΩ) | Source |
|---|---|---|---|
| Strat-style single coil | 1.8-2.5 | 5.5-6.5 (Fender Stratocaster: 5.8-6.3) | [VV], [SD-DCR] |
| Telecaster lead / rhythm | — | 6.2-6.6 / 7.6 | [SD-DCR] |
| Jaguar-style single coil | 2.5-3.2 | — | [VV] |
| P-90-style single coil | 4.9-6.8 | 8.6 (Gibson P-90) | [VV], [SD-DCR] |
| PAF-style humbucker | 2.8-4.5 (summary: 3.2-4.6) | 8.4 (Gibson P.A.F.) | [VV], [SD-DCR] |
| Mini humbucker | — | 6.8 | [SD-DCR] |
| Hot humbucker (JB model) | — | 16.4 | [SD-DCR] |
| Blade single coil / humbucker | 6.0 to over 9.0 | — | [VV] |

- [VV] also shows why DCR is a poor guide: two coils of 8,500 turns measured 6.2 kΩ (42 AWG) and 8.1 kΩ (43
  AWG) but almost the same inductance (2.41 and 2.28 H) and output (102 and 103 mV). Inductance is also
  frequency-dependent (an LCR meter at 120 Hz, 1 kHz and 10 kHz gives different values; snippet of a
  University of Illinois lab).
- Winding (self-)capacitance: about 80-200 pF according to a forum quotation of [LEM]; inductances "for most
  available pickups" between 1 and 10 H (same quotation, snippet).
- Eddy currents in the pole pieces and cover add losses that lower the resonance's Q (inferred from the
  literature on pickup measurement; not quantified in what we read). The model gets an extra parallel
  resistance per pickup type to set Q.

### 2.4 The circuit: resonance, pots, tone, cable

- The pickup is a source with series L and R and a parallel C (winding), loaded by the volume and tone pots,
  the tone capacitor and the cable's capacitance, then by the amp's input (typically 1 MΩ, inferred). Its
  resonance is f_r = 1/(2π sqrt(L C)) with C the total parallel capacitance; more cable capacitance lowers
  the peak (a cable patent states the formula, https://image-ppubs.uspto.gov/dirsearch-public/print/
  downloadPdf/7700872; a Lemme table reproduced at https://ozvalveamps.org/pickups.html shows cable capacitance
  dragging a peak from about 10 kHz to about 3 kHz, snippet).
- **Cable capacitance**: 52-200 pF/m with about 125 pF/m as a middle value; a 3 m lead with plugs can reach
  600 pF; old coiled cables up to 400 pF/m [ESP214] (snippet). Other sources give "around 100 pF per metre"
  (https://www.premierguitar.com/diy/mod-garage/are-you-using-the-right-cable, snippet). Cable loss matters
  most with the volume turned down, because the source impedance is then much higher [ESP214].
- **Pots**: 250 kΩ for single coils and 500 kΩ for humbuckers is the convention; the higher value loads the
  pickup less and keeps more treble; P-90s often get 500 kΩ [GEW] and a Seymour Duncan forum thread (snippet).
  **Tone capacitors**: 0.01-0.1 µF, with 0.022 and 0.047 µF the most common; the pot and capacitor form a
  variable lowpass [GEW] (snippet). Sources disagree on whether a larger capacitor removes "more treble" or
  "more mids"; the circuit answer (inferred, below) is that it moves the loaded resonance lower.
- **Calculated examples** (inferred, lossless approximation, values from the rows above):
  - 2.4 H single coil + 100 pF winding + 3 m of cable (≈ 500 pF) → about 600 pF → f_r ≈ 4.2 kHz.
  - 4.4 H humbucker + about 800 pF → f_r ≈ 2.7 kHz.
  - Tone at zero: the tone capacitor appears in parallel through the pot's residual resistance; 2.4 H with
    0.022 µF → ≈ 680 Hz; with 0.047 µF → ≈ 470 Hz; 4.4 H with 0.047 µF → ≈ 350 Hz. That "wah-like" peak at
    zero tone is a recognisable sound and the model must keep it (inferred).
  - Volume pot at the half-resistance point: the source sees up to R/4 in series (62.5 kΩ for 250 kΩ, 125 kΩ
    for 500 kΩ); into 600 pF of cable that is a first-order lowpass near 4.2 and 2.1 kHz respectively: the
    familiar "volume down loses treble". A **treble bleed** (a small capacitor, often with a resistor, across
    the volume pot's input and wiper) compensates; values vary by maker and are not in any source we read.
- **Selectors**: three-way (neck, both, bridge) for two-pickup guitars; five-way for three single coils
  (neck, neck+middle, middle, middle+bridge, bridge), where the in-between positions put two pickups in
  parallel; "50s" versus "modern" wiring changes how volume and tone interact [GEW] (snippet). Exact switching
  matrices are a design table (§8.4), not a research claim.

### 2.5 Hum and noise

- Single coils pick up mains hum (50 or 60 Hz and harmonics, strongly dependent on orientation towards
  transformers and monitors); humbuckers cancel most of it (§2.2). Coil resistance adds thermal noise: a
  6 kΩ coil produces about 10 nV/√Hz (calculated, 4kTR at 300 K), negligible against hum and amp noise but
  audible at high gain (inferred). The model adds hum per pickup type, scaled by gain, with an on/off switch
  and a 50/60 Hz choice.

---

## 3. Amplifiers, cabinets and microphones

### 3.1 The canonical tube amplifier and the modelling literature

- Pakarinen and Yeh review the digital techniques: static waveshaping, circuit simulation (nodal and
  state-space), wave digital filters, and black-box methods; the premise is that tube overdrive is preferred to
  solid-state distortion, while tube amps are heavy, fragile, power-hungry and costly [PY09] (abstract and the
  first page available at https://muse.jhu.edu/article/266412).
- **Triode model.** Koren's plate-current equation (a softplus-style logarithmic term, with a signum to block
  current when the intermediate value is negative; a pentode variant with an arctangent term) fits the 12AX7
  datasheet curves well [KOR] (snippet; validated by an Electronic Design column cited in the same summary).
  Cohen and Hélie start from Koren's models to simulate a class-A single-ended power amplifier with the output
  transformer modelled from datasheet values, as a nonlinear DAE solved in real time [CH10] (abstract);
  Dempwolf, Holters and Zölzer discretise parametric analogue circuits for real-time simulation (DAFx-10,
  https://dafx.de/paper-archive/details/74c2I_PbWhkM6MUukii5IQ, title).
- **The stage-by-stage behaviours the literature agrees on** (from [PY09] and [PASP Vacuum_Tube_Modeling]
  chapter titles; detail inferred from circuit theory):
  1. A common-cathode triode stage is an asymmetric soft clipper: grid conduction clips the positive input
     swing softly (the grid draws current through the source impedance), cutoff clips the negative swing
     hard; the cathode resistor sets the bias.
  2. A **cathode bypass capacitor** raises gain above a corner frequency (more treble gain); unbypassed, the
     stage has local feedback, less gain and a softer clip.
  3. **Coupling capacitors and grid resistors** form a high-pass between stages; grid conduction charges the
     coupling capacitor, shifting the next stage's bias for tens to hundreds of milliseconds after a loud
     attack: **blocking distortion** ("farting out"), the reason high-gain amps sound different on chords
     than on single notes. It cannot be captured by a memoryless waveshaper.
  4. The **tonestack** (§3.2) sits after one or two gain stages in American and British designs, and its
  insertion loss is made up by the next stage.
  5. The **phase inverter** and **push-pull power stage**: two tubes conduct alternately; class AB biasing
     leaves a small crossover region; the output transformer sums them and its leakage inductance and core
     limit the bandwidth; **global negative feedback** from the speaker output to the phase inverter lowers
     distortion and output impedance, and **presence** and **resonance** controls reduce that feedback at
     high and low frequencies respectively, raising the amp's gain there (§3.3).
  6. The **power supply** sags under load (§3.4); the speaker's impedance (rising with frequency, peaking at
     its resonance) interacts with the output stage's output impedance (inferred; standard).

### 3.2 The FMV tonestack

- Yeh and Smith analyse the Fender/Marshall/Vox family's passive tonestack symbolically and discretise it,
  comparing responses for the '59 Bassman's component values [YS06] (abstract); Yeh's page notes that other
  amps share the circuit with different values (https://ccrma.stanford.edu/~dtyeh/tonestack/).
- The continuous-time transfer function is third order: H(s) = (b1 s + b2 s² + b3 s³)/(a0 + a1 s + a2 s² +
  a3 s³), a0 = 1, with coefficients polynomial in the knob positions t (treble), m (middle) and l (bass) and in
  R1-R4, C1-C3. Faust's `tonestacks.lib` (authored by the Guitarix project, LGPL-2.1-or-later) writes them out
  and applies a bass taper l = exp(3.4·(L − 1)) for a linear control L; its comments explain why a direct-form
  IIR fails "in single precision at high sampling rates" (poles cluster near z = 1) and use trapezoidal
  integrators in observable canonical form instead [FTS]. The coefficient expressions are the published
  mathematics of [YS06]; we derive ours from the circuit and check against them (§10 T12).
- Component values [FTS] (R1 treble pot, R2 bass pot, R3 middle pot, R4 slope resistor; C1 treble cap, C2
  bass cap, C3 middle cap):

| Stack (as labelled in [FTS]) | R1 | R2 | R3 | R4 | C1 | C2 | C3 |
|---|---|---|---|---|---|---|---|
| '59 Bassman 5F6-A ("American tweed") | 250 k | 1 M | 25 k | 56 k | 250 pF | 20 nF | 20 nF |
| '69 Twin Reverb AA270 ("American blackface") | 250 k | 250 k | 10 k | 100 k | 120 pF | 100 nF | 47 nF |
| '64 Princeton AA1164 | 250 k | 250 k | 4.8 k | 100 k | 250 pF | 100 nF | 47 nF |
| JCM-800 2203 ("British lead") | 220 k | 1 M | 22 k | 33 k | 470 pF | 22 nF | 22 nF |
| JTM 45 | 250 k | 1 M | 25 k | 33 k | 270 pF | 22 nF | 22 nF |
| Mesa Boogie Mark ("American high gain") | 250 k | 250 k | 25 k | 100 k | 250 pF | 100 nF | 47 nF |
| Soldano SLO 100 ("modern high gain") | 250 k | 1 M | 25 k | 47 k | 470 pF | 20 nF | 20 nF |
| Bogner Triple Giant preamp | 250 k | 1 M | 33 k | 51 k | 220 pF | 15 nF | 47 nF |
| AC-30 (R3 fixed; "circuit differs anyway") | 1 M | 1 M | 10 k | 100 k | 50 pF | 22 nF | 22 nF |

  The Vox top-boost stack is a different topology (treble and bass only, no middle pot, a cut control in the
  power amp); [FTS] approximates it with the FMV equations and says so in a comment ("circuit differs
  anyway"). We will derive the top-boost network separately (inferred; §11).
- Behaviour (inferred, from the equations): the FMV stack is never flat; with all knobs at noon it has a mid
  scoop whose depth and frequency depend mainly on R4 and C2/C3 (the "British" values move the scoop up and
  make it shallower than the "American" ones); the controls interact (treble affects the middle level); and
  the insertion loss is 10-20 dB.

### 3.3 Power amplifier, negative feedback, presence and resonance

- A traditional presence control sits in the global negative-feedback loop: it shunts part of the fed-back
  signal at high frequencies, so the amplifier's gain rises there; in tube amps it is "usually a shelving type
  of equalizer, typically implemented as a low-pass filter inside the global negative feedback loop"; the
  shelf frequency and amount vary greatly between amps; some presence controls (1975 Super Twin) are active
  EQ at 3.9 kHz instead (https://en.wikipedia.org/wiki/Presence_(amplification); patent US 7,412,064,
  https://patents.google.com/patent/US7412064). The resonance (or "depth") control is described on forums as
  the low-frequency counterpart, reducing feedback at bass frequencies (https://www.trinityamps.com/phpbb/
  viewtopic.php?p=34154, forum).
- Model (inferred): the power stage as two complementary soft-clipping halves with a bias-dependent crossover
  region, preceded by a phase inverter with its own soft clip, inside a feedback loop with one sample of delay
  (at the oversampled rate) through presence (high shelf) and resonance (low shelf) filters; the output
  transformer as a band-pass (high-pass from primary inductance, low-pass from leakage) plus a mild saturation
  at low frequency at high level. The speaker's impedance curve (resonance peak, rising inductive top) shapes
  the response when feedback is low (inferred; standard tube-amp theory).

### 3.4 Sag

- A tube rectifier and the filter capacitors form a supply whose voltage drops under sustained load; the drop
  depends on the rectifier's effective series resistance and the filter capacitance; one forum post gives about
  200 Ω for a 5U4 and 270 Ω for a 5R4, and others say a GZ34 holds the supply noticeably higher at full load,
  and that solid-state diodes sag least (https://music-electronics-forum.com/forum/amplification/guitar-amps/
  theory-design/40641-the-definitive-rectifier-tube-sag-thread, forum; anecdotal).
- Model (inferred): a one-pole "supply" state driven by the power stage's average current, with fast drop
  and slower recovery (time constants from the RC of resistance and capacitance, tens to hundreds of ms), that
  lowers the clipping thresholds and gain of the power stage and, more weakly, the preamp. A "rectifier"
  control maps diode → GZ34 → 5U4-like resistances.

### 3.5 Speakers, cabinets and microphones

- **Speaker.** A typical 12-inch guitar speaker (Celestion Vintage 30, as an example of published data) has a
  resonance of 75 Hz and a frequency range of 70 Hz-5 kHz, 100 dB sensitivity
  (https://www.parts-express.com/Celestion-Vintage-30-12-16-Ohm-Guitar-Speaker-60W-294-2276, retailer listing of
  the maker's data). Above about 1-2 kHz the cone stops moving as a piston and breaks up into modes, which
  gives the dense, peaky upper mids and the steep roll-off above 5 kHz that make a guitar speaker sound like
  one (inferred; standard loudspeaker theory, not measured by us).
- **Cabinet.** Open-back cabinets leak the rear wave, which cancels the low end and adds room; closed-back
  cabinets add a low-frequency resonance from the enclosed air and tighter bass (inferred; standard).
- **Microphones.** The dynamic cardioid most used on cabinets (Shure SM57) is specified at 40 Hz-15 kHz;
  Shure's guide says a source within about 6 mm gets roughly 6-10 dB of bass boost at 100 Hz (proximity
  effect), aiming at the cone's centre gives the sharpest attack, and moving towards the edge gives a darker
  sound (Shure SM57 user guide, e.g. https://r2.gear4music.com/media/23/231382/download_231382.pdf, snippet).
  Guides describe 45° off-axis as darker and warmer (https://www.recordingmag.com/resources/recording-info/
  mics-miking/capturing-the-cab/, snippet). Ribbon microphones (figure-8, strong proximity effect, soft top)
  and condensers (extended top, more room) are the other two classes (inferred; standard).
- **Two microphones**: a path difference Δd between them makes a comb with the first notch at c/(2Δd):
  Δd = 5 cm → 3.4 kHz, 10 cm → 1.7 kHz, 20 cm → 860 Hz (calculated, c = 343 m/s). This is the phase problem
  engineers fight and also a tone control (inferred).
- **Impulse responses.** Cabinets are linear enough at moderate level that a measured IR (speaker + cabinet +
  microphone + room) is the industry's standard representation; IR loaders are in every amp suite. Guitarix
  ships a convolver (zita-convolver) and cabinet IRs [GX file list].

### 3.6 Open implementations: Guitarix and Neural Amp Modeler

- **Guitarix** (GPL-2+ for its own code, conveyed as a whole under GPL-3+ because of GPL-3 and LGPL-3
  dependencies [GX trunk/COPYING]). A Linux-first amp simulator whose DSP is mostly written in Faust: the
  source tree has `faust/` with `gxamp.dsp` … `gxamp18.dsp`, `champ.dsp`, several wah models (`colwah`,
  `dallaswah`, `foxwah`, `jenwah`, `maestrowah`), `fuzzface.dsp`, `cabinet_impulse_former.dsp`, plus `NAM/`
  and `RTNeural/` directories and the zita convolver and resampler [GX file list]. It loads `.nam` files
  through NAM modules and RTNeural JSON/`.aidax` models, runs standalone, as LV2 and VST3, and on a Raspberry Pi
  (repository README). The Faust `tonestacks.lib` values above come from the Guitarix project [FTS]. **We read
  Guitarix only for orientation; nothing is copied** (§9).
- **Neural Amp Modeler (NAM)**: the core library, the plug-in and the trainer are all **MIT** (GitHub licence
  API) [NAM]. The core (C++ with Eigen) implements `linear`, `convnet`, `lstm` and `wavenet` models and reads
  `.nam` files (a JSON model description) [NAM file list]. Community "types" of WaveNet capture are Standard,
  Lite, Feather and Nano, and a newer "A2" generation in Full and Lite sizes (https://www.tone3000.com/blog/
  understanding-nam-types, vendor blog). CPU: on an embedded MOD Dwarf a Nano model used about 36 % and a
  Feather about 66 % at 128 frames after a build tweak, and Standard models were reported as too heavy; on a
  desktop i7 a user saw Standard ≈ 8 % and Feather ≈ 5 % in Task Manager (https://forum.mod.audio/t/neural-amp-
  modeler/10112, forum; anecdotal). One user measured ESR 0.070 for a Feather and 0.026 for a Standard capture
  of the same amp and heard most of the difference above 12 kHz (same thread).
- What we could use later (inferred): NAM is MIT and therefore compatible with AGPL-3.0; a later version could
  load user `.nam` captures in place of our amp model (user-supplied files only; captures carry their own terms
  on the sites that host them). It is not part of version 1: our amp is a model with controls, and a capture
  is a snapshot of one setting.

### 3.7 Aliasing, oversampling and ADAA

- Antiderivative antialiasing (ADAA) replaces y = f(x) by the average of f over the line between consecutive
  input samples, computed from the antiderivative F: y[n] = (F(x[n]) − F(x[n−1]))/(x[n] − x[n−1]) (first order),
  which markedly reduces aliasing, especially combined with mild oversampling, and can be extended to feedback
  systems [PZL16] (abstract); Bilbao, Esqueda, Parker and Välimäki generalise it to higher orders [BEPV17]
  (abstract). First-order ADAA adds half a sample of delay and needs an ill-conditioning guard when
  x[n] ≈ x[n−1] (inferred; standard).
- ADAA applies to memoryless nonlinearities. Stages with memory in the nonlinear loop (the power amp inside
  global feedback, grid conduction charging a coupling capacitor, diode clippers in an op-amp feedback path)
  need oversampling or an implicit solver (inferred).
- The repo already has a cascaded Kaiser-windowed half-band FIR chain for 2x/4x/8x whose final 2x→1x stage
  has a 20-28 kHz transition at 96 kHz and about 100 dB rejection (`shared/dsp/Oversampling.h`, classes
  `HalfbandFir`, `Interpolator`, `Decimator`, namespace `dh`). Being linear phase, it adds a fixed latency
  that must be reported to the host (§8.10).

---

## 4. Pedals

### 4.1 Compressor and noise gate

- A guitar compressor is a fast feed-forward or feedback compressor with a long release that adds sustain and
  evens picking. The repo's `Compressor` in Lacquer (`plugins/lacquer/Source/dsp/Dynamics.h`; feed-forward,
  log-domain detector after the gain computer, "smooth decoupled" peak detector after Giannoulis, Massberg and
  Reiss, *JAES* 60(6), 2012, eq. 4 and 17, as its header cites) is the basis; a pedal voicing adds a fixed
  ratio, a soft knee and a sustain control (inferred).
- A noise gate with hysteresis (open and close thresholds), hold and a release of tens of milliseconds;
  placed before the drives and optionally keyed from the DI (inferred).

### 4.2 Mid overdrive ("green" type)

- In the Tube Screamer-style clipping stage, a 4.7 kΩ and 0.047 µF high-pass at about 720 Hz thins the bass
  going into the clipper, and 51 pF across the diodes softens the clipped edges [TS-UO] (snippet;
  1/(2π·4.7 kΩ·47 nF) = 721 Hz, calculated). Another analysis attributes 720 Hz to a different pole and says
  the high-pass corner depends on the drive setting (https://www.ecircuitcenter.com/Circuits/distortion_box/
  op_disto1.htm, snippet); the two readings agree that the clipped signal is mid-heavy. The diodes are in the
  op-amp's feedback path, so the stage adds the clipped signal to the clean one: the output never fully
  squares and the sound stays "amp-like" (inferred, circuit theory).
- Yeh, Abel and Smith model the "distortion" and "overdrive" pedals as a cascade of a conditioning filter, a
  memoryless nonlinearity and an equalisation filter [YAS07] (abstract); Yeh's thesis analyses the distortion
  circuits in depth [YEH09].
- Model (inferred): input buffer; high-pass 720 Hz (drive-dependent) into a feedback-diode clipper solved as
  y = x + g·clip(…) with ADAA on the diode characteristic; 51 pF lowpass; tone stage (lowpass with a variable
  shelf); level. Zero latency.

### 4.3 Distortion (hard clipping to ground)

- The RAT-style pedal is a single LM308 op-amp gain stage followed by two 1N914 diodes clipping to ground, a
  tone filter and an output buffer [ES proco-rat] (snippet). MusicRadar's teardown says the original LM308's
  slow response causes "a significant amount of clipping in the op-amp, as well as high-frequency
  attenuation" (https://musicradar.com/news/the-fx-files-pro-co-rat, snippet), while forum posters argue the
  diodes clip before slew limiting matters (https://www.diystompboxes.com/smfforum/index.php?msg=91489,
  forum). The slew rate figure was not verified.
- Model (inferred): gain stage with a slew-rate limiter (a rate clamp at the oversampled rate, the only pedal
  that needs oversampling) → diode pair to ground (ADAA) → variable lowpass "filter" → level.

### 4.4 Fuzz (two-transistor feedback)

- The Fuzz Face-style circuit is an input stage, an output stage and a feedback network that shapes gain,
  input impedance, output impedance and frequency response; that is why it does not respond well after other
  pedals and should come first [ES fuzz-face] (snippet). Germanium transistors have high leakage and
  inconsistent gain; ElectroSmash recommends β ≈ 70-80 for the first stage and 110-130 for the second (or a
  compressed alternative 90-120 / 150-190) [ES fuzz-face] (snippet). A builder's rule of thumb is about 5 V on
  the second transistor's collector (https://forum.pedalpcb.com/goto/post?id=217127, forum). Bias sets the
  clipping asymmetry, gating ("sputter") when under-biased, and germanium is temperature-sensitive
  (https://www.soundhouse.co.jp/en/products/detail/item/374428/, vendor).
- The low input impedance loads the pickup, which is why turning the guitar's volume down "cleans up" a fuzz
  rather than just lowering it (widely reported; e.g. https://www.diystompboxes.com/smfforum/index.php?
  msg=594488, forum).
- Model (inferred): a small nonlinear state-space model (two transistors, Ebers-Moll, the feedback resistor and
  capacitors), solved with Newton at 4x, **including the pickup network as its source impedance**: the fuzz is
  the one pedal that must see the guitar's passive circuit (§8.4), so it reads the pickup model's output
  impedance at the current volume setting. Controls: fuzz, volume, bias, silicon/germanium, temperature.

### 4.5 Wah

- The Cry Baby-style wah is an inductor band-pass (resonant LC in a transistor feedback stage) swept by a
  pot; Holters and Zölzer use it to demonstrate the nodal DK method for circuits with variable parts, because
  "the continuous change of the potentiometer position is an extremely important aspect" [HZ11] (snippet);
  Bogason and Werner model time-varying reactances with wave digital filters (DAFx-18 best paper, snippet);
  Gnegy and Werner digitised the Ibanez Weeping Demon (DAFx-15, https://dafx.de/paper-archive/details/
  1jzmFOCPECrcGgYQZxF6gw, snippet). Wahs with inductors from about 225 to 650 mH are reported to sound good,
  and the capacitor C2 sets the centre of the sweep (https://forum.pedalpcb.com/threads/wah-inductors-no-hype-
  just-measurements.24076/post-396680, forum). The sweep range in hertz (the brief's 400 Hz-2.2 kHz) was not
  verified (§11).
- Model (inferred): a second-order band-pass with frequency and Q both tracking the pedal position (Q rising
  towards the heel), discretised with the TPT state-variable form so the sweep is click-free; pedal position
  from a CC (11 by default), an envelope (auto-wah) or an LFO.

### 4.6 Modulation

- **BBD chorus, flanger and vibrato**: Raffel and Smith model each part of a bucket-brigade system (the
  anti-aliasing and reconstruction filters, the companding, the BBD's nonlinearities and frequency-dependent
  insertion gain) by circuit analysis and measurement; requirements differ for echo versus chorus, vibrato and
  flanger circuits [RS10] (abstract). Holters and Parker combine the BBD with its filters (DAFx-18,
  https://dafx.de/paper-archive/details/KbFTgvcTMmHQ2bHZvOUekw, title).
- **Phaser**: Eichas et al. model a four-stage JFET phaser with the nodal DK method; the JFETs are variable
  resistors that move the allpass cascade's phase, and they give an efficient description of the JFET
  nonlinearity [EFHZ14] (abstract). OTA phasers are the other family (inferred).
- **Tremolo** (amplitude) and **vibrato** (pitch) with a choice of LFO shapes, including the "photocell"
  asymmetry of optical tremolos (inferred).
- Models (inferred): reuse the repo's BBD character (Halation's `Character::bbd` loop insert) for chorus,
  flanger and vibrato; a four- or six-stage allpass phaser with feedback; tremolo with a soft optical
  envelope.

### 4.7 Delays

- Digital (clean, tempo-synced, ping-pong), BBD (dark, noisy, companded, with clock-noise and limited time),
  and tape (wow and flutter, saturation, head bumps). The repo already has: Halation's FDN-based delay with
  tape, BBD and digital characters and "Tape" (glide) or "Clean" (crossfade) time changes; Tarnish's
  Jiles-Atherton hysteresis for tape saturation (`plugins/tarnish/Source/dsp/Hysteresis.h`, normalised units,
  fixed-step RK4 integrated along the path of h, which makes it independent of the sample rate as its header
  explains). The guitar delay is a two-head subset of these (inferred).

### 4.8 Spring and room reverb

- **Spring**: Välimäki, Parker and Abel model the spring tank parametrically: a spectral delay filter (an
  allpass chain giving the chirp) inside a feedback loop with a long delay and adjustable loop gain, with
  delay time, chirp shape and decay as parameters [VPA10] (abstract). Parker and Bilbao give the physical view
  and measurements of two tanks [PB09]; Parker reduces the cost by about two-thirds with multirate and
  multiband structures [P11] (abstract summary). The repo's AT700 has `Spring711` (two springs of 38.3 and
  46.1 ms, a dispersive allpass chain in a delay loop, dark and metallic, reverb time up to about 2 s:
  `plugins/at700/Source/dsp/Modules700.h`), which follows the same parametric idea (inferred).
- **Room / plate / hall**: Halation's 8-line FDN (`plugins/halation/Source/dsp/Network.h`; orthogonal
  rotation matrix, T60-based gains per band after Jot, per-band freeze), used at its "space" end as the amp's
  room and the post-mic reverb (inferred).

### 4.9 Order and routing

- Default order (inferred, conventional): tuner → compressor → wah → fuzz → overdrive/distortion → gate →
  amp (preamp, tonestack) → [effects loop: modulation, delay] → power amp → cabinet/mics → room → post
  reverb/delay. The board is reorderable except for the fuzz, which warns when not first (its sound depends on
  the source impedance, §4.4).

---

## 5. What makes a virtual guitar sound real

### 5.1 What listeners and reviewers say gives a virtual guitar away

- Audio Masterclass compared Ample Sound's Strat instrument with a real Stratocaster through the same chain
  (compressor, amp simulator, EQ, delay, reverb) and found the virtual one easy to pick out: "It sounds like a
  robot guitarist". Simple lines and block chords work; the mod-wheel vibrato is "regular", the same on every
  note, varying only in depth; velocity switching gives some pick variety but "falls far short of a real
  player"; human bends and slides are "nearly impossible without extra work"; the real guitar sounded more
  sustained, and the author attributes part of the difference to finger vibrato spreading the reverb and
  delay across frequencies (https://www.audiomasterclass.com/blog/can-an-electric-guitar-virtual-instrument-
  ever-sound-like-a-real-electric-guitar, full).
- In a Gearspace "fake or real" thread a listener names what was missing: "the time it takes to get from one
  chord to another, the inversions, the variation in pick tone" (https://gearspace.com/threads/fake-or-
  real.1343921/, snippet; the page returned 403 to direct fetch). Sound On Sound's review of Steinberg's
  Virtual Guitarist observed that even the best guitar multisample "has a habit of ending up sounding like an
  undistinguished electric piano" (https://www.soundonsound.com/reviews/steinberg-virtual-guitarist,
  snippet).
- These match the brief's list (inferred synthesis): repetition, uniform vibrato, no transition time or noise
  between chords, the wrong voicings (keyboard voicings rather than guitar voicings), simultaneous strums and
  unconvincing bends.

### 5.2 Variation per note instead of round robin

- Sample libraries cycle through a few recordings per note (round robin) to avoid the "machine-gun" effect;
  MusicLab advertises a "Sound Humanize" feature that "completely removes the 'machine gun' effect on note
  repetition (tremolo, strumming)" (vendor claim, https://www.adorama.com/mlrealstrat4.html).
- A model gets variation for free if its inputs vary like a player's (inferred): pick position along the
  string (a few millimetres per note, larger for strums), pick angle (the split between polarisations),
  attack force (velocity mapped through a curve plus a small random term), pick hardness and edge (the
  excitation's bandwidth), timing (a few milliseconds, more on weak beats), the string's state (a new pluck
  on a still-ringing string interacts with its motion), and the left hand (fretting force, which changes
  pitch by a few cents and buzz). Values are deterministic: drawn from a counter-based hash of (seed, event
  index, parameter id), so a song renders identically every time and at any buffer size (§8.8, §10 T10).

### 5.3 Hand noises

| Noise | Physics / source | Model (inferred) |
|---|---|---|
| Pick attack | the pick's contact and release, a broadband click whose spectrum depends on pick hardness and speed | short excitation table per pick type, scaled by velocity; part of the excitation, so it goes through the string and pickup |
| Finger squeak on wound strings | sliding along the winding makes noise with a harmonic structure from the periodic winding; harmonic frequencies and RMS level scale **linearly with sliding speed**; the sliding also excites longitudinal modes that add static harmonics, which depend on the sliding position [PPB07] (abstract) | noise generator with a comb at f = v/p (v slide speed from the left-hand trajectory, p winding pitch per string: about the outer wrap wire's diameter) plus 2-3 fixed longitudinal resonances per string; only on wound strings, only during position changes and slides |
| String release / mute | lifting a fretting finger or touching a ringing string: a short damped "thud" and a brief buzz against the frets | loss ramp in the loop (§8.1) plus a small click; Prominy lists mute, picking, fret, tap, pick-stop, bridge-mute and release noises as separate sample sets in SR5 (https://www.soundonsound.com/reviews/prominy-sr5-rock-bass, snippet) |
| Fret noise / buzz | the string hitting frets, louder at low action and hard attack [WLG] | in-loop limiter (§1.9) |
| Sympathetic open strings | coupling through the bridge and neck (§1.7) | the bridge resonator; open strings left undamped unless the voicing mutes them |
| Pickup hum and hiss | mains field, coil resistance (§2.5) | per pickup type; humbuckers cancel |
| Body knocks, pick scrapes | percussive and gestural | keyswitch-triggered excitations; a pick scrape is a long, rough excitation on a wound string with a falling comb |

### 5.4 Assigning each note to a string and fret

- **The problem**: a guitar plays the same pitch at up to five or six places; the choice changes timbre
  (thicker string, longer or shorter vibrating length, open or fretted), what can ring together, and which
  transitions are physically possible.
- **Literature.** Sayegh formulates fingering as a shortest path through a layered graph (a Viterbi network)
  whose layers are the notes and whose nodes are the admissible fingerings of each note [SAY89] (via citing
  papers). Radicioni, Anselma and Lombardo use finger span and minimum/maximum spans between pairs of fingers
  to score the difficulty of transitions, segmenting the piece into phrases first [RAL04], and later a
  constraint-satisfaction model of the hand checked against three experts' fingerings [RL05] (snippets).
  Tuohy and Potter search tablatures with a genetic algorithm whose fitness scores playability [TP05], later
  replaced by a neural network in Tuohy's thesis (snippet). Barbancho et al. go the other way: they estimate
  string and fret from audio through each note's inharmonicity (thicker strings are stiffer), for single notes
  and chords, on classical, acoustic and electric guitars [BTSB12] (abstract). A 2016 ISMIR paper summarises
  earlier approaches as "mainly based on path optimization by minimizing the difficulty of a whole phrase"
  (https://archives.ismir.net/ismir2016/paper/000285.pdf, snippet); recent machine-learning MIDI-to-tab work
  exists (e.g. https://arxiv.org/pdf/2510.10619, title).
- **Our algorithm (inferred).** Dynamic programming (Viterbi) over a causal window, because a live instrument
  cannot look ahead: candidates per note are (string, fret) pairs within the tuning and the fret range; the
  state is the hand position (lowest fret of a four-fret box, with a stretch allowance) and the per-string
  occupancy; the cost of a transition is
  - hand shift: proportional to the distance moved in frets, larger for shifts during a sustained note;
  - span: zero within four frets, rising steeply beyond (scaled by fret width, so high positions allow more);
  - open strings: a small bonus (or penalty, user-selectable per style) and always allowed to ring;
  - string preference: a style weight (prefer lower strings for "fat", higher for "bright") and a penalty for
    skipping strings in fast passages;
  - same-string legato: when two notes overlap or are marked legato on one string, a bonus for hammer-on or
    pull-off if the interval is ≤ 4-5 frets, and a slide if larger;
  - collision: two simultaneous notes cannot share a string; a new note on a sounding string cuts it (§8.1).
  With a look-ahead of zero for live play and of one bar when rendering from a host timeline (the host's
  "process ahead" is not available to plug-ins in general, so look-ahead applies only to the offline MIDI
  export and the arranger; inferred). Deterministic tie-breaking (lowest string, then lowest fret) makes the
  result reproducible (§10 T7).
- Overrides: "position lock" (a fret window from a keyswitch or CC), a per-note string choice by channel
  (channel-per-string input, §7.2) and by keyswitch.

### 5.5 Strumming

- **Measured.** Freire et al. recorded three guitarists on a nylon guitar with hexaphonic pickups and
  defined the global spread interval (first to last attack of a chord) and the intertone spread (between
  adjacent strings) [FAVS18]. Their summary table (Table 6, combining a previous study):

| Chord type | Tempo | Intertone spread (ms) | Global spread (ms) |
|---|---|---|---|
| 4-note block | very fast | 0-6 | 5-18 |
| 3-note block | very slow | 0-5.5 | 5-11 |
| 6-note strummed | adagio (≈ 65 BPM) | 8-11 | 37-46 |
| 6-note strummed | moderate | 5-12.5 | 26-62 |
| 6-note arpeggio | moderate | 16-40 | 64-202 |

  Other findings [FAVS18]: in a fast 16th-note pattern the number of strings per chord departed from the score
  in both directions (score: 23 % six-note and 34 % three-note chords; the three renditions: 31/21/51 %
  six-note and 22/13/4 % three-note, Table 4); on the up-strokes two players hit fewer strings while the third
  used all six, which gave "less salient accents and a darker timbre"; spreads differ strongly
  between players (26, 39 and 62 ms for six-note chords in the same excerpt) and correlate with playing behind
  the click; beat control is anchored on one or two middle strings (2-3 or 3-4) rather than on the first
  attack, and can shift string within a performance; down-beat arpeggios were spread more than third-beat
  ones, and players aimed the beat at the first string of each arpeggio; temporal resolution in hearing is 1-18
  ms, so the spreads are near perception thresholds and are heard as "colour" rather than as rhythm. Up- and
  down-stroke spreads were not tabulated separately.
- **Our strummer (inferred)**: per chord, the strum direction, the strings hit (down-strokes from the lowest
  voiced string; up-strokes default to the top 3-4 strings, configurable), a global spread from a tempo- and
  velocity-dependent distribution (centred on 30 ms at 100 BPM, narrower when faster and louder, bounded by
  the table), a per-string jitter of ±1-2 ms, an accent that shortens the spread and raises velocity, and an
  "anchor" string placed on the grid (default string 3), so the strum starts before the beat. Per-string
  velocity decays slightly across the stroke (the pick loses energy) and depends on direction.

### 5.6 Articulations

| Articulation | Physics / evidence | Model (inferred) |
|---|---|---|
| Hammer-on / pull-off | new note on the same string without a pick; the string keeps its motion; a hammer adds a fretting impulse; a pull-off plucks with the finger | delay length jumps (with a 1-2 ms Lagrange glide to avoid clicks), the loop keeps its content; hammer adds a small excitation at the fret; pull-off adds a finger-pluck excitation |
| Slides | continuous change of length; contact noise; on wound strings the zipper of [PPB07] | delay trajectory with a realistic speed profile (accelerate, cruise, decelerate), fret "bumps" as tiny pitch steps when sliding with a finger (none with a slide tube [PPV08]) |
| Bends | raising the string sideways raises tension; real bends overshoot or undershoot and are corrected | trajectory: fast rise (80-200 ms for a whole tone), slight overshoot of 5-20 cents, settle; release faster than rise; per-note random within bounds |
| Finger vibrato | for bowed strings and voice the rate is 5.5-8 Hz (a 40-player string study gives a mean of about 5.5 Hz, https://journals.sagepub.com/doi/abs/10.2307/3345438, snippet; singers 5.5-6.7 Hz); no guitar measurement verified | one-sided (bend vibrato only raises pitch) or two-sided (classical), rate 5-7 Hz with ±10 % drift, depth ramping in after 150-300 ms; rate and depth vary per note |
| Palm mute | the hand's edge on the strings near the bridge adds strong, frequency-dependent loss and lowers brightness | extra loss in the loop (T60 of 0.1-0.4 s, darker loss filter) plus a shorter excitation; depth from velocity or CC |
| Dead notes | fretting hand touches without pressing: almost no pitch, a percussive thud | very high loss, noise-like excitation, pitch at the touch position |
| Natural harmonics | touching the string at 1/n of its length damps every mode without a node there | in SDL form: a comb that keeps multiples of n and a short loss burst while the finger touches, then free ringing |
| Pinch harmonics | the thumb touches right after the pick at a node position | the same comb applied for a few milliseconds at a pick-relative node chosen from the pick position; strongly favoured by high gain |
| Tremolo picking | fast alternate picking, up/down alternating | repeats on one string with alternating pick direction and per-stroke variation |
| Whammy (vibrato bar) | lowering the bridge changes all string tensions; the change is uneven across strings (the bridge pivots), and releasing the bar makes it flutter | per-string detune factors (larger on the wound strings, inferred to be measured), a second-order "arm" resonance of a few hertz with low damping for flutter |
| Controlled feedback | at high gain and volume the string is driven by the speaker; the note sustains and may jump to a harmonic | a feedback path from the post-cabinet signal back into the strings (gain, delay, a band-pass around the selected harmonic); sustain mode keeps the fundamental, "bloom" mode crossfades into the 2nd or 3rd harmonic after a delay. STK's `Twang` takes an input sample for this purpose [STK] |

---

## 6. The market

### 6.1 Sample-based virtual guitars

| Product | What it offers | What users and reviewers criticise |
|---|---|---|
| **Ample Sound, Ample Guitar series** (e.g. Ample Guitar T, M, SC) | Finger, pick and strum sample sets; Riffer and Strummer modules; "Strummer with 28 play styles per chord"; Tab Reader for Guitar Pro GP3-GP8; v4 "Strum Note" object and piano-roll/tab views (https://www.pluginboutique.com/products/15077-Ample-Guitar-T, vendor text) | Audio Masterclass's "robot guitarist" verdict on the Strat model: uniform vibrato, bends and slides need manual work (§5.1) |
| **Native Instruments, Session Guitarist** (Strummed Acoustic 1-2, Electric Sunburst, others) | Pattern-based: 154 patterns in Electric Sunburst, a playback engine that follows chord changes, and individual note samples for arpeggiated patterns (vendor and reviews); Sound On Sound found creating mix-ready parts "ridiculously easy" (https://www.soundonsound.com/reviews/native-instruments-electric-sunburst, snippet) | Future Music: distorted tones less convincing than clean ones because the guitar was recorded clean and gain relies on amp modelling; patterns are "far more realistic" but less flexible than single-note sampling (https://www.pressreader.com/australia/future-music-9629/20180628/283214443924081, snippet) |
| **Impact Soundworks, Shreddage 3** (Serpent, Abyss, Rogue…) | Fretting and voicing algorithms selectable by menu or keyswitch; keyswitches default in 3.5; sustains, power chords, palm mutes, harmonics, hammer-ons and pull-offs; Console multi-FX with amps, cabs, pedals; 3-5 GB per instrument, 16-25 GB for the "Rock Band" bundle; Kontakt Player (https://impactsoundworks.com/shreddage-3-5-has-arrived/, https://www.bestservice.com/en/shreddage_3_rogue.html, vendor) | Disk and RAM footprint; keyswitch-driven workflow (inferred from the feature list; no forum complaint verified) |
| **MusicLab, RealGuitar / RealStrat / RealLPC / RealEight** | Chord and Bass&Chord modes, keyboard split into Main and Repeat zones, Pattern Manager with a large rhythm library and Song mode, "Sound Humanize" against machine-gun repetition (https://rekkerd.org/?p=1990, vendor text) | MusicRadar was enthusiastic ("scarily similar to the real thing", https://musicradar.com/reviews/tech/musiclab-realstrat-122295, snippet); the complexity of modes and zones is the usual learning-curve remark (inferred) |
| **Orange Tree Samples, Evolution** series | Strawberry (electric): "intelligent legato" for hammer-ons, pull-offs and sampled half-step transitions, automatic string selection; Songwriter (acoustic): velocity-sensitive strumming and chord recognition (https://www.orangetreesamples.com/news/evolution-electric-guitar-strawberry-reviewed-by-prorec-receives-9-10-stars, vendor news; Equipboard summary) | Steep learning curve for newcomers (Equipboard summary, snippet) |
| **Prominy**, SC / LPC electric guitars, SR5 bass | Real-time legato slides, hammer-ons, pull-offs, trills, tremolo picking; separate noise sets (mute, picking, fret, tap, pick-stop, bridge-mute, release); SR5 Rock Bass 2 added chord voicings, strumming, arpeggio picking, stompboxes and amp sims; about 21,700 samples (https://www.soundonsound.com/reviews/prominy-sr5-rock-bass-2, snippet). A VI-Control poster calls LPC and SC "almost close to real in lead guitar case" thanks to many bend and vibrato samples (https://vi-control.net/community/threads/ultra-real-sounding-guitar-libraries-prominy-once-a-year-winter-sale-2024-2025.159739/, forum) | Complexity of playing techniques from a keyboard; the original SR5 is discontinued and unsupported (https://prominy.com/?p=378) |
| **UJAM, Virtual Guitarist** (Iron 2, Amber, Sparkle, Carbon…) | Phrase-based: Iron 2 has 30 styles and 350+ phrases (palm mutes, gallops, hammer-ons, slides, power chords), an Instrument mode for own riffs, drop-D and doubling (https://www.kvraudio.com/marketplace/virtual-guitarist-iron-2-by-ujam-instruments, vendor text) | Sounds real "when used subtly" (one KVR user review, 2021); phrases limit what can be played (inferred) |
| **Vir2 Electri6ity**, **Warpiv** and others | Keyswitched legato modes (Poly, Solo, Legato); Warpiv's pitch bend limited to ±2 semitones by default (https://vi-control.net/community/threads/new-release-warpiv-electric-guitar.35002/, forum) | keyswitch-based legato logic |

### 6.2 Modelled virtual instruments

- **Applied Acoustics Systems, Strum GS-2**: models "strings, bridge, soundboard, pickups, amplifier, and
  speaker cabinet"; recognises played chords and voices them as a guitarist would; auto-strum and dedicated
  strumming keys; MIDI loops with rhythm figures; acoustic first (March 2015), electric added (June 2015);
  new multi-effect, EQ and compressor (https://www.applied-acoustics.com/aas-strum-gs-2-electric-update-press-
  release/, vendor). A MusicRadar review, as summarised by Equipboard, praises the "excellent chord
  translation" and calls the amp simulation "mediocre" and leads unconvincing; users say it sits well in a mix
  but is less realistic than samples, and one KVR reviewer heard "a weird sound … that emulates a hummed
  string, very hard to mix out" (https://equipboard.com/items/applied-acoustics-systems-strum-gs, snippet).
  Small download and quick loading are the model's advantage (same summary).
- **IK Multimedia, MODO Bass** (bass, but the closest modelled competitor in spirit): modal synthesis with
  each string a nonlinear resonator and the player's action modelled; timbre changes with where and how hard
  you pluck; 20-24 interchangeable pickups whose position can be moved (counts differ by version); developed
  over about eight years with a university (https://www.soundonsound.com/reviews/ik-multimedia-modo-bass,
  https://www.songwritingmagazine.co.uk/gear/ik-multimedia-releases-modo-bass, snippets).
- **Pianoteq** (Modartt) is the analogous success story for pianos (modelled, small, playable, tweakable);
  mentioned for positioning only (inferred).

### 6.3 Amp and pedal modellers

- Commercial suites (Neural DSP's Archetype plug-ins, IK AmpliTube, NI Guitar Rig, Line 6 Helix Native,
  Kemper's profiling hardware) and open ones (NAM, Guitarix) are the standard against which our amp will be
  judged (inferred). Forum themes: high-gain "fizz" (a Fractal forum user describes Line 6 high-gain tones as
  "grainy and fizzy, not integrated" when presence is pushed, https://forum.fractalaudio.com/threads/if-you-
  play-metal-don%E2%80%99t-buy-the-helix.170035/page-5, forum), "feel" (one user: "sounds good when you listen
  to it, feels awful when you actually play through it"), and latency (a user measured 7.6 ms round trip in
  Reaper at 48 kHz/64 samples without Helix Native and 12.5 ms with it, https://thefretboard.co.uk/
  discussion/110124/helix-native/p17, forum). For a MIDI-driven instrument, the "feel" of the amp under the
  player's hands is less critical than for a guitarist playing through it, but latency still matters for
  live keyboard play (inferred).

### 6.4 What the market lacks (inferred)

- A free, open instrument with a modelled string, an explicit string/fret engine, measured strum timing, MPE and
  channel-per-string input, MIDI out of the realised performance, and a full amp chain.
- Small footprint (tens of MB, not tens of GB) and instant load.
- Bends, vibrato and slides that sound human by default (trajectories with per-note variation) without hand
  editing of pitch bend.
- Transparency: show the fretboard, the chosen fingering and the strum in real time, so a non-guitarist learns
  what the virtual player does and can correct it.

---

## 7. MIDI conventions for guitars

### 7.1 Keyswitches and controllers in sample libraries

- Keyswitches (notes outside the instrument's range that select articulations) are the default in Shreddage
  3.5 and consistent across its guitars (vendor, §6.1); Electri6ity's main keyswitches are Poly, Solo and Legato
  (snippet). Libraries differ in key placement, latching versus momentary behaviour and which octave they use;
  there is no standard (inferred).
- Our convention (inferred): keyswitches in the lowest octave below the instrument's range, latching for modes
  (poly, mono-legato, strum, arpeggio), momentary for one-shots (dead note, pick scrape, harmonic) and
  mirrored as CCs and as an articulation menu, so they can be automated without notes; velocity layers for
  palm-mute depth; CC 1 for vibrato depth, CC 11 for the wah or volume pedal, CC 64 for "let ring",
  pitch bend ±2 semitones by default on a single channel (Warpiv's default, §6.1) with a user setting.

### 7.2 Channel per string (guitar mono mode)

- MIDI 1.0 defines four channel modes; Mode 4 ("Omni Off, Mono") makes a receiver monophonic per channel. A
  guitar "Mono mode" developed later uses six such channels at once, one per string (https://www.sweetwater.com/
  insync/more-midi-modes-clearer-definition/, snippet); a midi.org forum thread says the main example of Omni
  Off Mono with multiple channels is a guitar controller sending each string on its own channel and that it
  sends a Mono Mode On (CC 126) message whose parameter is the number of channels (https://midi.org/community/
  midi-specifications/mono-mode-on-parameter, forum; the exact semantics are in the MIDI 1.0 Detailed
  Specification, not read).
- Roland: in mono mode "each of your guitar's strings sends its data on its own MIDI channel", the basic
  channel carrying the high E and the following channels the lower strings (GR-20 document); on the GR-50,
  poly mode sends all strings on channel 1 and mono mode uses channels 1-6, and Roland recommends a bend range
  of 12 semitones "for guitar technique" with the receiver set to match [GTR-MIDI].
- Fishman TriplePlay: normally a single channel for all strings; one channel per string requires "Boot in
  Hardware Mode" or a multi-timbral preset; the receiving synth must be multi-channel and mono-voice
  [GTR-MIDI].
- Jamstik: Single-Channel, Multi-Channel (channels 1-6 by default, first channel configurable) and MPE
  modes; firmware 3.10 sets the default pitch-bend range to ±48 semitones; ranges must match on both sides
  [GTR-MIDI].
- Our convention (inferred): an input mode "Channel per string" with a first channel (default 1 = high E, as
  Roland, or 1 = low E, user choice), a bend range per mode (default 12, with 2, 24 and 48 offered), notes on a
  channel always mapped to that string (fingering engine bypassed for string choice, still used for fret
  validity), and per-channel pitch bend applied to that string only.

### 7.3 MPE

- The MIDI Association ratified MPE on 28 January 2018: each sounding note gets its own channel between Note On
  and Note Off so that Pitch Bend and Control Change can address it; each zone has a dedicated master channel
  for Program Change, pedals and overall pitch bend; a Registered Parameter Number configures the zones [MPE
  midi.org]. The document number RP-053 is commonly cited but was not verified.
- Zones: a Lower Zone with master channel 1 and member channels 2 up to 16, and an Upper Zone with master
  channel 16 and members 15 down to 2; per-note dimensions are Pitch Bend (x), CC 74 (y) and Channel Pressure
  (z) on the member channel [MPE Linn].
- Pitch-bend sensitivity: on receiving an MPE Configuration Message a receiver sets the manager channel to 2
  semitones and every member channel to 48 semitones; both can be changed later with RPN 0 sent to the manager
  or to every member channel; whole semitones are recommended and devices may limit themselves to 0-96 [MPE
  forum quote of the spec, p. 14].
- Our use (inferred): in MPE mode each member channel is a "finger": its note is assigned to a string by the
  fingering engine at Note On and keeps that string until Note Off; per-note pitch bend bends that string
  (beyond a whole step it becomes a slide, configurable); pressure maps to vibrato depth or sustain (user
  choice); CC 74 maps to pick position (bridge ↔ neck) or palm-mute depth. A guitar controller in MPE mode
  (Jamstik) therefore drives the strings directly.

### 7.4 MIDI out and export

- Output (inferred): the performance the engine realised (strummed and arpeggiated notes with their timing,
  articulations as keyswitches or CCs, bends as pitch bend) as MIDI out, optionally one channel per string with
  the bend range announced by RPN 0, and a `.mid` export of a region. The point is to let the user print the
  "guitarist's" part into the DAW, edit it, and play it back bypassing the strummer and fingering engine
  (string choice carried by the channel).

---

## 8. DSP and performance design (inferred throughout)

Everything in this section is a design proposal written to match the engine being built. Numbers are starting
values for the tests of §10. The engine is JUCE-free C++ in `plugins/thrum/Source/dsp/`, as in the other
products, and runs at the host rate except where stated.

### 8.1 String

- **Topology**: one SDL loop per polarisation per string (z and y), 6 strings (7 for the 7-string type),
  plus one "ghost" voice per string for the tail of a cut note (below). Loop: delay line (power-of-two ring,
  length ≥ fs/f_min + 8) → 4-point Lagrange read (fraction kept in [1, 2)) → loss filter (one pole plus gain,
  fitted to T60 at f0 and at a brightness frequency, §1.3) → 4 first-order dispersion allpasses (coefficient
  from a precomputed (B, f0) table, §1.5) → optional fret-buzz limiter (z only, §1.9) → bridge coupling input
  (§8.3) → back into the delay. The delay length is fs/f0 minus the phase delays of the loss filter, the
  allpasses and the Lagrange at f0, recomputed when f0, B or the loss change.
- **Pitch**: f0 from the string's open pitch (tuning), the fret (equal temperament from the fret formula,
  plus a per-string saddle-compensation offset in cents and a "fretting sharpness" of +1 to +4 cents that
  grows with fretting force), bends, vibrato, the whammy and tension modulation (energy-driven delay shortening
  with a per-string κ). B scales with 1/L² as the note moves up the neck.
- **Excitation**: a short buffer written into the loop at the pluck instant, shaped by the pick model (§8.2)
  and filtered by the pick-position comb 1 − z^(−βN) computed per note (β from the pick position in mm divided
  by the vibrating length; fractional by linear interpolation; the pick position varies per note, §8.8). The
  excitation is split between z and y by the pick angle.
- **Same string retrigger**: a new note on a sounding string hands the old loop to the ghost voice with a
  12-25 ms damping ramp (the pick or finger stops the string), while the new pluck starts in the main loop
  with a fresh excitation added to the residual motion (a pick on a moving string does not erase it); legato
  transitions (hammer, pull, slide) keep the loop and move the delay instead.
- **Release and damping**: Note Off without "let ring" applies a damping ramp whose speed depends on the
  articulation (finger lift on a fretted note: 30-80 ms with a release thud; open string: the palm or a
  finger, 50-150 ms); "let ring" (CC 64, or the arpeggiator's ring mode) keeps the string free until it is
  needed again.
- **Denormals and safety**: flush-to-zero on; no noise dither in the loops (it would break determinism);
  instead loops whose energy drops below −140 dBFS are zeroed and parked; a soft limiter at ±4 on the loop
  state guards against runaway coupling (never active in normal play; §10 T6).

### 8.2 Pick and fingers

- **Pick**: a pulse whose duration falls with hardness and velocity (soft pick 1.5-3 ms, hard 0.3-0.8 ms at
  mf), whose shape is a raised cosine of displacement (the pick pushes the string, then releases it), followed
  by the release click. Direction (down/up) changes the sign of the z/y split and adds the EKS pick-direction
  lowpass difference (up-strokes slightly darker, §1.2). Angle 0-45° moves energy from y to z. A small noise
  component scaled by "pick wear" adds scrape.
- **Fingers**: wider, softer pulse (3-8 ms), more y, and a thumb mode (darker, lower on the string). Chosen per
  preset or per articulation; the fingerpicking arpeggiator uses fingers by default.
- **Velocity**: maps to pick force through a curve and to the EKS dynamic-level lowpass (softer = darker,
  §1.2), plus tension-modulation depth.

### 8.3 Bridge, neck and body

- A shared resonator bank: 4-8 two-pole resonators per guitar type (neck modes for every type; plus top and
  air modes for semi-hollow and hollow), fed by the sum of the strings' z-loop outputs weighted by a per-string
  coupling, and fed back into every loop with a coupling gain of order 10⁻³ (STK uses 0.01/N, §1.12). A
  per-string, per-mode coupling matrix lets a neck mode create a dead spot near a chosen fret (the [PLF14]
  phenomenon) at "realism high", and no dead spots at "realism low".
- The acoustic body sound of the hollow types (what a microphone near the guitar would hear) is a separate
  commuted-synthesis output (§1.4), blendable with the amp.

### 8.4 Pickups and the passive circuit

- **Per string**: the velocity of each string's z (and −20 dB y) is read at the pickup position by a comb
  (difference of two taps on the loop delay, i.e. the output-position comb of the SDL form) and an aperture
  lowpass (two taps averaged ±w/2); a humbucker reads two positions at ±9 mm and sums them (switchable to one
  coil: "split"). Magnetic nonlinearity: a mild static curve on the displacement-to-flux relation with ADAA
  (strength by pickup height).
- **Network**: per pickup a series L-R source with parallel winding C and eddy-loss resistance; the selector
  combines pickups (3-way or 5-way tables, plus "out of phase" and series options); the volume pot (audio
  taper, 250 k or 500 k), the tone pot and capacitor (0.022 or 0.047 µF), an optional treble bleed, the cable
  capacitance (length × pF/m from a "cable" control) and the amp's input impedance. Solved symbolically as a
  rational H(s) of order ≤ 4 per selector position and discretised by the bilinear transform with prewarping at
  the resonance whenever a control moves (control-rate, smoothed by crossfading two filter states over 10 ms).
- **Output impedance** at the current settings is exported to the first pedal (the fuzz, §4.4).
- **Hum and hiss**: §2.5.

### 8.5 Guitar types, strings and tunings

| Type | Scale | Pickups | Default strings | Body |
|---|---|---|---|---|
| Solid, three single coils | 25.5 in | 3 × single coil, 5-way | 10-46 | solid; neck modes only |
| Solid, two humbuckers | 24.75 in | 2 × humbucker, 3-way, coil split | 10-46 | solid |
| Semi-hollow | 24.75 in | 2 × humbucker | 10-46 | centre block; top and air modes, feedback-prone |
| Hollow jazz | 24.75 in | 1-2 humbuckers (neck default) | 12-52, wound G | hollow; strong top and air modes, short sustain |
| Baritone | 27 in (28.625 option) | 2 × humbucker | 13-62 | solid |
| 7-string | 25.5 in | 2 × humbucker | 10-59 | solid |

Tunings: standard (E A D G B E), drop D, half-step and whole-step down, D standard, open G, open D, open E,
DADGAD, B standard (baritone), B E A D G B E (7-string), and custom per string (±12 semitones from the set's
nominal pitch). Gauges: light / regular / medium / heavy per type, with tensions computed from [DAD] unit
weights (§1.11) and shown in the UI; tension feeds tension modulation, B and the buzz threshold, so a slack
drop tuning buzzes and glides more, as it should.

### 8.6 Performance: melody and legato

- Modes: poly (each note a string from the fingering engine), mono (last note), mono-legato (overlapping notes
  become hammer-on, pull-off or slide on the same string when the interval allows; otherwise a new pluck on
  another string), and "smart legato" (the fingering engine chooses legato or re-pick from interval, tempo and
  velocity: a velocity below a threshold on an overlapping note = legato).
- Bends from pitch bend follow the trajectory model of §5.6 when "humanise bends" is on: the wheel sets the
  target, the model adds overshoot and settle; off, the wheel is followed literally.

### 8.7 Chords, strumming and fingerpicking

- **Chord detection**: template matching on the held pitch-class set (major, minor, 5, 6, 7, maj7, m7, dim,
  aug, sus2, sus4, add9, 9, m9, 11, 13 and slash chords with the lowest held note as bass), scored by the
  number of matching and missing tones, ties broken by simplicity; the user can also play "one-finger" chords
  (root + modifier keys) in an easy mode.
- **Voicing search**: enumerate frettings over the real tuning inside a window of 4-5 frets (plus open
  strings), one note per string, muted strings allowed, constraints on the bass (root or the requested bass),
  and cost terms: span, number of fingers (barre counts as one), open-string preference per style, distance from
  the previous voicing (voice leading and hand travel), and style families (open, barre, power chord, triads on
  string sets, drop-2 and shell voicings for jazz). Cached per (chord, tuning, window, style).
- **Strummer**: tempo-synced patterns (1/8, 1/16, triplets, swing) where each step has direction, accent,
  string range (all, low, high, n strings), type (normal, palm mute, dead/scratch, ghost), and length; the per-
  string spread follows §5.5. Chord changes insert transition noise and a short gap (the left hand lifting)
  unless the voicings share a shape. Pattern library (written for this product, no transcriptions): rock 8ths,
  pop 16ths, funk 16ths with scratches, reggae off-beat skank, ballad, post-punk downstrokes, country boom-chick
  with bass alternation, folk, shuffle; plus an editor and import from MIDI.
- **Fingerpicking arpeggiator**: patterns as finger sequences (p-i-m-a) over voiced strings: Travis picking
  (alternating thumb bass), forward and backward rolls, classical p-i-m-a arpeggios, pinches; with
  let-ring per pattern, timing feel and per-finger velocity.

### 8.8 Human variation and determinism

- Every random quantity is drawn from a counter-based hash: h = mix64(seed ⊕ (eventIndex × K1) ⊕ (paramId ×
  K2)) (a SplitMix64-style finaliser), where eventIndex counts note and strum events from the host's
  transport position (or from the plug-in start when the host is stopped). No state carries over between
  events, so results do not depend on buffer size or render order, and a bounce matches playback.
- Parameters and default ranges: timing ±4 ms (strums: per-string ±1.5 ms), velocity ±6 % (log), pick position
  ±6 mm (strums ±10 mm), pick angle ±8°, fretting sharpness 0-4 cents, bend overshoot 5-20 cents, vibrato
  rate ±10 %, tension-modulation depth ±20 %. A single "human" control scales them all; "seed" changes the
  performance.

### 8.9 Hand noises and MIDI out

- Noises per §5.3, each with a level and an on/off switch, triggered by the performance engine (position
  changes, releases, chord changes) and by keyswitches.
- MIDI out per §7.4: the realised notes, optionally one channel per string; `.mid` export of the last N bars.

### 8.10 Pedals, amp, cabinet and latency

- **Pedals** at the base rate with first-order ADAA on every memoryless nonlinearity: zero latency. Exception:
  the fuzz (§4.4) and the slew-limited distortion (§4.3) run inside the amp's oversampled section when placed
  before it, or with their own 2x when the board is reordered (the latency of that path is then reported).
- **Amp** at 2x, 4x (default) or 8x using the repo's half-band chain (`shared/dsp/Oversampling.h`); latency is
  the chain's group delay (a fixed number of base-rate samples per factor, reported to the host via
  `setLatencySamples`); the DI output is delayed by the same amount so DI and amp stay aligned. Preamp: 2-4
  triode stages (asymmetric soft clip with grid-conduction knee, cathode-bypass shelf, coupling high-pass with
  bias shift for blocking); FMV tonestack (bilinear, coefficient update with state-preserving form, §3.2) or the
  top-boost and "modern" stacks; phase inverter and push-pull power stage in a feedback loop with presence and
  resonance; output transformer; sag (§3.4); master volume before or after the phase inverter (both exist).
  Six voices: American clean, American tweed, British crunch, British lead, modern high gain, "chime"
  (top-boost-like); each a set of stage gains, bias points, tonestack values (from §3.2) and power-amp settings.
- **Cabinet**: our own speaker model (cone resonance, cone break-up modes, inductive rise) and microphone
  models (dynamic, ribbon, condenser; position from centre to edge, distance 1-30 cm with proximity effect,
  angle 0-45°, two mics with relative distance and polarity, floor reflection as a delayed, filtered copy),
  rendered to a minimum-phase IR whenever the user changes them (background thread, crossfade when ready);
  a generated IR set of our own cabinets; user IR loading (WAV, resampled to the session rate, truncated to
  the maximum length with a fade). Convolution: direct FIR (zero latency) up to 2048 taps at 48 kHz (~43 ms),
  which covers cabinet IRs; longer user IRs fall back to a uniformly partitioned FFT with a direct-form head
  (still zero latency).
- **Room and post effects**: Halation's FDN for the room and post reverb (with freeze), Tarnish's
  Jiles-Atherton hysteresis for the tape delay's saturation, AT700's `Spring711`-style tank for the amp's
  spring reverb, Lacquer's `Compressor` for the compressor pedal (§4). All shared code moves to `shared/dsp/`
  if two products use it (the repo's convention; to confirm with the owner of the code).
- **Budget** (inferred, to verify in §10 T20): six strings with two polarisations, pickups and the
  performance engine ≤ 1.5 % of a core on the Apple M4 at 48 kHz; amp at 4x ≤ 4 %; cabinet 2048-tap FIR ≤ 1 %;
  whole default preset ≤ 8 % on the M4 and ≤ 12 % on the Ryzen 9 5900XT.

### 8.11 As built in phase 1 (differences from the proposal above)

Measured or decided while building (2026-10-21); the code is the reference (`Source/dsp/`, comments in Spanish).

- **Interpolation**: a 6-point (fifth-order) Lagrange read instead of 4 points: the 4-point read loses up to
  0.074 dB per trip at 6 kHz with the fraction at 0.5, which shortens the treble of high notes depending on where
  their delay falls (a T60 of 1.2 s at 6 kHz on the high E at fret 0 from interpolation alone); 6 points lose
  0.009 dB (calculated from the Lagrange coefficients: the magnitude of the 4- and 6-tap filters at 6 kHz, 48 kHz).
- **Dispersion**: six identical first-order allpasses per loop (not four), the coefficient fitted per fret at
  the first use (golden-section search, ~0.1 ms) to minimise the maximum error in cents of partials 2..K, with K
  the last partial under 1.5 kHz and under 50 cents of stiffness deviation. Measured (thrumtest T1, 148 notes on
  the six types): fundamental within ±0.7 cents (the two polarisations are detuned ±0.15-0.65 cents and the
  analysis reads their blend), partials 2..8 within ±2.5 cents of k·f0·√(1 + B·k²) relative to the fundamental.
  Above K the partials are about half as sharp as the stiff string (one-pole-type allpasses cannot follow the
  curve over the whole band). Wound strings use a core of 0.38 × the outer diameter.
- **Retrigger**: no ghost voice. A new pick on a sounding string multiplies the loop by a cosine dip to 0.05
  during exactly one trip of the new loop (the read delay minus two samples); the new excitation enters after
  the contact point and arrives back there when the window has closed. The previous note drops more than 50 dB
  in 50 ms (T9).
- **Excitation domain**: the loop carries acceleration; the pluck is a Hann pulse of 25-300 µs (pick) or
  0.2-0.8 ms (finger) whose height scales with the sample rate, low-passed by hardness and velocity, plus a
  differentiated noise burst for the scrape (stronger on wound strings), then the pick-position comb and, for
  harmonics, the average of k copies shifted by N/k. The pickup integrates once (velocity).
- **Body**: three bridge/body modes per type in one bank shared by all strings (sympathetic resonance, about
  35 dB under the note that excites it at the default amount, and microphonic pickup leakage on the hollow types),
  and one neck mode per type (220-290 Hz) inside every loop as a narrow loss peak: a partial right at the mode
  decays with an extra T60 of 4 s (a dead spot); its phase enters the tuning. No separate acoustic output yet.
- **Pickups**: two reads per coil (aperture ±5 mm for single coils, coils at ±9 mm for humbuckers), the
  passive network as a third-order transfer (source R+sL, self+cable C, volume and amp load, tone pot + cap),
  parallel pickups merged into one equivalent coil; volume below 10 adds the pot's output resistance against
  the cable capacitance as a one-pole low-pass (an approximation of the divider). No coil split, series or
  out-of-phase options yet; no magnetic nonlinearity.
- **Amps**: four voices (American clean, British crunch, modern high gain, small combo) instead of six. Stage
  gains of a 12AX7 (30-40) in units of the grid bias, dividers between stages, an audio-taper gain pot and a
  table that compensates the loudness change of the gain knob (measured with the engine's own strum: without it
  the clean amp spans 50 dB between gain 1 and 10). Amp latency at 2x/4x/8x: 47, 55 and 57 samples (the
  half-band lengths are chosen so the round trip is an integer number of base-rate samples); the latency stays
  constant with the amp switched off (a matching delay) and the DI is delayed by the same amount.
- **Pedals**: all at the base rate with ADAA on the drives (cubic soft clip and hard clip with polynomial
  antiderivatives: no log or exp per sample); the fuzz is not moved into the amp's oversampled section.
- **Cabinet**: direct FIR up to 4096 taps (45 ms at 48 kHz for the model and the factory IRs); user IRs are
  resampled with a windowed sinc and truncated to 4096 taps with a fade (no partitioned FFT yet). The model IR is
  designed on a background thread in the plug-in and synchronously when rendering offline. **Phase 2:**
  uniformly partitioned convolution without latency (a 256-tap direct head plus 256-sample FFT partitions,
  overlap-save, a frequency-domain delay line; Wefers 2015 ch. 5, Gardner 1995 for the head), user IRs up to 3 s;
  the partitions are transformed on the IR thread and swapped in without allocation. Matches the direct
  convolution within 2e-6; a 3 s IR costs 0.84 % of an M4 core and 1.2 % of a Ryzen 9 5900XT core.
- **Performance**: strum steps are scheduled as events and resolved (chord, voicing, strings) when they fire, so
  the result depends only on the timeline; phase 2 adds the anchor of §5.5: by default a step is resolved up
  to 45 ms before its beat and the middle strings land on it (an option keeps the first string on the beat). A
  chord that changes during a step (quantised MIDI changes exactly on the beat, after the strum has started)
  cancels the strings not yet picked and replays the step with the new chord 8 ms later; keys arriving up to 60 ms
  after a step replay it with the new chord; a 60 ms grace before muting on key release. In the polyphonic melody
  mode, notes that arrive in the same sample are placed jointly (distinct strings, one possible hand) and picked
  low to high over 30 % of the strum time. Chord detection covers 22 chord types with slash basses; the voicing
  search is per call (2 µs average, no cache needed).
- **CPU on the Apple M4** (thrumrender --bench, 48 kHz, blocks of 512, each preset's demo phrase): 4.9 % of one
  core on average over the 69 presets, 5.8 % at most; the guitar alone 1.4 %.

---

## 9. Licences and code provenance

| Code studied | Licence (verified text) | Compatible with AGPL-3.0? | What we did |
|---|---|---|---|
| STK (Cook, Scavone) | MIT-style permission notice, "Copyright (c) 1995-2023 Perry R. Cook and Gary P. Scavone", plus a non-binding request to send modifications back [STK LICENSE] | yes, with the notice kept if code were copied | read `Plucked`, `StifKarp`, `Twang`, `Guitar`; **nothing copied** |
| Faust `physmodels.lib` v1.2.4 (GRAME) | LGPL 2.1 or later, with a GRAME exception for compiled output of Faust programs [FPM header]; no MIT or STK-4.3 section found in the part read (the brief's guess was not confirmed) | yes (LGPL-2.1+ code can be conveyed under GPL-3/AGPL-3) | read the guitar models; **nothing copied** |
| Faust `tonestacks.lib` | LGPL-2.1-or-later, author "Guitarix project" [FTS] | yes | read; component values cited as facts; coefficients re-derived from the circuit |
| Guitarix | own code GPL-2+, whole program conveyed under GPL-3+ (GPL-3/LGPL-3 dependencies) [GX COPYING] | GPL-3 and AGPL-3 may be combined (each licence's §13), but we do not | licence and file list only; **read only, nothing copied** |
| NAM core, plug-in, trainer | MIT (GitHub licence API for all three repositories) [NAM] | yes | README and file list; not used in v1 |
| `adf.m` (Abel, Välimäki, Smith dispersion design) on the CCRMA wiki | not stated | unknown | not used; the design is re-derived from [AVS10] |
| Papers ([KS83] … [FAVS18]) | copyright of the publishers | — | read for methods; no text copied; equations re-derived |
| Patents | KS (US 4,649,783) and Smith's waveguide (US 4,984,276) shown as expired on Google Patents [PAT]; STK headers mention "Stanford University, Yamaha, and others" | — | nothing to license on the evidence read; not legal advice |

**Our implementation copies no code from any of the above.** It is written from the papers, the circuit
equations and first principles, and every reused module comes from this repository (Halation, Tarnish, AT700,
Lacquer, `shared/dsp`).

---

## 10. Test plan

### 10.1 Automated (JUCE-free)

In `plugins/thrum/tools/dsptest.cpp` (built like the other products' `dsptest.cpp`: `clang++ -std=c++20 -O2
-I plugins/thrum/Source/dsp -I shared/dsp`), plus `thrumrender` (a preset and a MIDI file to WAV with a CPU
figure) and the shared host tests. 48 kHz unless stated.

| # | Test | Pass criterion |
|---|---|---|
| T1 | Open-string tuning | Every string of every type, set and tuning, plucked at mf: f0 within ±1 cent of the target after the first 300 ms (tension glide over), measured by a high-resolution partial tracker |
| T2 | Partials vs stiff-string formula | Partials 1-20 of each open string and at frets 5, 12, 19 within ±3 cents of n f0 sqrt(1 + B n²) with the B of §1.11 (±1 cent for n ≤ 8); B estimated from the render within ±15 % of the design B |
| T3 | Fret and bend accuracy | Every fret 0-24 on every string within ±1 cent (plus the configured fretting sharpness); a 2-semitone bend reaches the target ±3 cents after settling; vibrato and slides produce no clicks (spectral flux below threshold) |
| T4 | Decay | T60 of the fundamental and of partial 8 within ±10 % of the preset's design values, open strings, solid types |
| T5 | Pickup comb | For a pickup at x on a string of length L, the measured notch harmonics fall at n = kL/x ± 0.3 (single coil); humbucker response equals the average of two single-coil responses at ±9 mm within 0.5 dB |
| T6 | Network stability | Coupling and feedback at maximum, all strings ringing, 60 s: no growth (energy non-increasing after the last pluck), no NaN; the guard limiter never activates in factory presets |
| T7 | Fingering golden files | 40 reference melodies and chord sequences: string/fret output matches the committed golden file exactly; every assignment is playable (one note per string, span ≤ the limit, frets in range); the same input in a different tuning gives a valid result |
| T8 | Chord detection and voicing | All templates × 12 roots × inversions detected correctly; every voicing contains the chord tones required by its family, is playable, and the bass matches the request |
| T9 | Strum timing | With a 120 BPM host, the anchor string of each strum lands on the grid ±1 sample; global spreads within the configured distribution; identical timing for block sizes 1, 32, 64, 441, 512, 4096 and random sizes |
| T10 | Determinism | Two renders, renders after `reset()`, and renders with different block sizes are bit-identical; changing the seed changes the output; starting at bar 9 gives the same audio from bar 17 as starting at bar 1 |
| T11 | MIDI in/out and MPE | Channel-per-string: notes on channel k always sound on string k; per-channel bend affects only that string; MPE: zones configured by the MPE Configuration Message, member bend range 48 default, pressure and CC 74 mapped; MIDI out of a strum reproduces the realised notes and timing; `.mid` export → import → play with the strummer off is identical to the original render (±1 sample) |
| T12 | Tonestack | FMV response vs the analytic H(s) of [YS06] (as written out in [FTS]) for the nine stacks of §3.2 at 27 knob settings: within ±0.1 dB from 20 Hz to 20 kHz at 48 and 96 kHz |
| T13 | Passive network | Pickup + pots + cable: resonance frequency within ±2 % of 1/(2π sqrt(LC)) for the lossless case; tone at zero and volume at half match the analytic model within ±0.5 dB |
| T14 | Aliasing | 1 kHz and 3 kHz sines at 0 dBFS into each pedal and amp voice: aliased (non-harmonic) components below −80 dBFS for the amp at 4x, below −60 dBFS for ADAA pedals at the base rate |
| T15 | Latency | Declared latency equals the measured delay of an impulse through amp and DI paths at 2x/4x/8x and 44.1-192 kHz (exact, in samples); DI and amp outputs aligned |
| T16 | No NaN, DC, denormals | Silence 60 s, DC input, full-scale input, 200 random extreme presets with parameters re-randomised every block (seeded), 22.05-192 kHz: no NaN/Inf (bit-pattern check), DC below −80 dBFS at the output, CPU in silence within 10 % of CPU with signal |
| T17 | Sample-rate independence | 44.1/48/96/192 kHz: tuning ±1 cent, T60 ±3 %, strum timing ±1 sample, tonestack ±0.1 dB |
| T18 | Retrigger and legato | A new note on a sounding string never produces a step larger than the configured damping ramp (no click); hammer-on/pull-off keep phase continuity; legato chains of 16 notes produce no excitation for legato notes |
| T19 | State and presets | Save/restore bit-identical; every factory preset renders without NaN and within ±1.5 dB of the levelled target |
| T20 | CPU | `thrumrender --bench`, 48 kHz, 512-sample blocks, default preset and a "worst" preset (7 strings, 8x, two mics, long user IR) on the Apple M4 and the Ryzen 9 5900XT: within the budget of §8.10; numbers recorded with the date in the README |
| T21 | User IR loading | 44.1/48/96 kHz WAV IRs of 0.1-2 s load, resample and truncate without clicks; mono and stereo; corrupted files rejected cleanly |
| T22 | Whammy and feedback | Whammy dive and release: per-string detunes match the configured factors; flutter decays; controlled feedback sustains a note for 10 s without growth and the harmonic jump happens within the configured delay |
| T23 | Noises | Finger-squeak fundamental tracks slide speed linearly (r² > 0.95 over a speed sweep) and is absent on plain strings |
| T24 | Host | athosttest, pluginval at strictness 8 and auval pass on macOS; pluginval on Windows |

### 10.2 Listening criteria for the user

- Blind A/B against our own DI recordings of a real guitar through the same amp model (loudness matched,
  3-5 listeners): single notes at three dynamics, bends with vibrato, a chord progression strummed, a
  fingerpicked pattern, a palm-muted riff; "which is the real one?" at or near chance is the target.
- Checklist per preset: no two consecutive repeated notes sound identical; open strings ring sympathetically
  when musically plausible and stop when muted; chord changes have a gap and noise; up-strokes sound thinner
  than down-strokes; the high E at fret 15 does not sound like a synthesizer; low notes have a sharp attack
  and a slight pitch drop; the amp cleans up when the guitar's volume is lowered; the fuzz reacts to the
  guitar volume; a non-guitarist can play a convincing rhythm part from one-finger chords within ten minutes.

---

## 11. Open questions and follow-ups

1. Obtain and read the paywalled sources marked "abstract": [KVT98], [VHKJ96], [RV06] (and the 2006 IEEE SPL
   letter's volume and pages), [AVS10], [TVK00], [EE10], [BT15], [PPV12], [PY09], [YS06], [VPA10], [PPB07].
2. Read [ZOL] (pickup aperture, humbucker coil spacing, measured coil parameters) and [LEM] directly;
   until then the humbucker's ±9 mm is an assumption.
3. Measure our own guitars: B per string (open and fret 12), T60(f) per string, pickup positions and
   inductances, pot values, so presets are fitted to data rather than to literature ranges.
4. Wound-string core diameters: ask string makers or measure; they set B for the wound strings (§1.11).
5. Flatwound unit weights for the jazz type from [DAD] (not yet tabulated).
6. Electric-guitar vibrato rate and bend trajectories: no measurement verified; record players (5-10) and fit
   the §5.6 trajectories.
7. Strum spreads on steel-string electric guitars with a pick, and up- versus down-stroke spreads: [FAVS18] is
   nylon, fingers/pick mixed and three players; consider our own hexaphonic recordings.
8. The wah's sweep range in hertz and the Q curve (the brief's 400 Hz-2.2 kHz is unverified); measure or read
   [HZ11] and the ElectroSmash analysis when reachable.
9. The Vox top-boost and "modern" tonestacks need their own derivations; [FTS] approximates the AC-30 with the
   FMV equations.
10. Power-supply sag data (rectifier resistance, capacitances) from schematics rather than forums.
11. Speaker break-up modelling: measure a cabinet (or use our own generated IRs only) and decide how many
    modes the speaker model needs.
12. Whether to offer NAM `.nam` loading (MIT core) in a later version, and how to present user captures.
13. Look-ahead fingering for the offline arranger: how far, and whether hosts give enough information.
14. MPE: map pressure to vibrato or to sustain by default? Needs a test with an MPE controller and a Jamstik.
15. Channel-per-string default: 1 = high E (Roland) or 1 = low E? Make it a setting, choose the default after
    checking TriplePlay and Jamstik defaults.
16. The product name "Thrum" needs a trademark search before release.
17. UI: show fretboard, voicing and strum in real time (§6.4); define the body art so that no guitar shape is
    recognisable.
18. Which reusable modules move to `shared/dsp/` (Halation FDN, Tarnish hysteresis, AT700 spring, Lacquer
    compressor): decision for the owner of those products.
