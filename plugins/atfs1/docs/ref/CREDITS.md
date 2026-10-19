# Reference material for ATFS1

## Free-licensed photographs (layout reference only; not shipped)

| Subject | Author | License | Source |
|---|---|---|---|
| FS1R front panel in a rack | Raymangold22 (own work, 2022-06-15) | CC0 1.0 | https://commons.wikimedia.org/wiki/File:Yamaha_FS1R.jpg |
| Close-up of the knob section | Raymangold22 (own work, 2022-06-15) | CC0 1.0 | https://commons.wikimedia.org/wiki/File:Yamaha_FS1R_Controls.jpg |

One more photo (Flickr, CC BY-NC-SA) was looked at for proportions; its license does not allow use in
this project's artwork, so nothing was traced from it or redistributed.

The positions in `tools/panelfs1.py` were measured on the CC0 photo scaled to the 482.6 mm rack width.
The ATFS1 artwork is drawn from scratch: no Yamaha logo, no FS1R logotype, no copied graphics. The
label texts (ATTACK, PART/OP, PERFORM...) are kept as functional names.

## Manufacturer documents (copyrighted, local reference only, NOT redistributable)

Not in this repository; the reading copies live outside it (`audiotools-private/fs1r-ref/`).

| Document | Source | Owner |
|---|---|---|
| FS1R Owner's Manual (88 pp.) | https://usa.yamaha.com/files/download/other_assets/5/333335/FS1RE1.PDF | Yamaha Corporation |
| FS1R Data List (44 pp.) | https://usa.yamaha.com/files/download/other_assets/4/317954/FS1RE2.PDF | Yamaha Corporation |
| FS1R Algorithm Sheet (2 pp.) | https://usa.yamaha.com/files/download/other_assets/2/334832/fs1r_en_as_a1.pdf | Yamaha Corporation |

The 88 algorithm topologies in `Source/dsp/FsAlgorithms.h` are the connections drawn on the
Algorithm Sheet (which operator modulates which, the carriers and the feedback loop), written as our
own bit masks. Sysex byte positions come from the Data List. Neither is copied text.

## Other sources

- US Patent 5,610,354 (Yamaha, expired): the windowed-carrier model of the operators.
- FSVR, https://github.com/musicastudio/FSVR (GPL-3): published measurements of real units (window
  lengths, skirt exponents, EG traverse law, noise operator, filter cutoff law, Fseq timing). Used as
  reference numbers only; no FSVR code or data is in ATFS1.
- Peterson, G. E. and Barney, H. L. (1952), "Control methods used in a study of the vowels", JASA 24:
  average vowel formants for the generated Fseqs.
- espeak-ng (GPL-3, https://github.com/espeak-ng/espeak-ng): spoke the phrases that were analysed into
  the factory Fseqs. Only the analysis (formant tracks) ships; the recordings do not.
