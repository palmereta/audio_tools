# Reference material for ATMD

## Free-licensed photographs

Every photo below was used only as a layout reference: to measure the panel and read the original
legends, wood and colours. None of them ships with the plugin. The ATMD artwork is drawn from
scratch from the coordinates in `../../assets/layoutmd.json`, with no manufacturer logo, wordmark
or nameplate.

The author, license and source of each photo were checked through the Wikimedia Commons API
(`prop=imageinfo&iiprop=extmetadata`) on 2026-10-18.

### Stored in this folder

| File | What it shows | Author | License | Source |
|---|---|---|---|---|
| `minimoog-panel-flat_CC-BY-2.0_Alex-Harden_Clusternote.jpg` | **Main layout reference.** The control panel of a 1970s unit, cropped and straightened into a flat rectangle, 3108x795 px, covering all five sections. It confirmed that the vector panel drawing is to scale horizontally (see `../hardware-spec.md` section 7) and gave the label positions | Alex Harden (photo); crop and straightening by Clusternote | CC BY 2.0 (https://creativecommons.org/licenses/by/2.0/) | https://commons.wikimedia.org/wiki/File:Minimoog_panel.jpg (derived from https://commons.wikimedia.org/wiki/File:Minimoog_(Buffalo_Museum_of_Science).jpg, from https://www.flickr.com/photos/24396426@N00/4255849310/) |
| `minimoog-lhc-wheels-angled_CC0_kpr2.jpg` | Close-up from the upper left, 1990x2012 px: the left-hand controller (GLIDE and DECAY rockers and jacks, PITCH and MOD wheels), the Controllers and Oscillator sections, the wooden name board and the cheek | kpr2 | CC0 1.0 (public domain) | https://commons.wikimedia.org/wiki/File:Minimoog_-_pitch_%26_modulation_wheels,_etc._-_left_angle_view_(2016-07_by_kpr2).jpg (originally Pixabay 1505322) |

### Consulted, kept outside the repository

These files are in `audiotools-private/minimoog-ref/photos/` on the maintainer's machine. They are
large, and the CC BY-SA ones would carry share-alike terms if redistributed.

| Subject | Author | License | Source |
|---|---|---|---|
| Front view of a worn 1970s unit, full panel, keyboard and wheels (5621x3162). Used for the panel/rail width ratio and the rail screw spacing | Ville Hyvönen | CC BY-SA 2.0 | https://commons.wikimedia.org/wiki/File:Minimoog_Ville_Hyv%C3%B6nen_2017.jpg (https://flickr.com/photos/35106208@N00/34345551873) |
| Front view, Deutsches Museum unit (5024x3028), early-style grey mixer rockers and blue-grey panel | Tiia Monto | CC BY 4.0 | https://commons.wikimedia.org/wiki/File:Minimoog_synthesizer.jpg |
| Front view, Buffalo Museum of Science unit (4000x3000), the uncropped source of the flat panel photo | Alex Harden | CC BY 2.0 | https://commons.wikimedia.org/wiki/File:Minimoog_(Buffalo_Museum_of_Science).jpg |
| Front view, SMEM museum unit (4032x3024) | 1904.CC | CC BY 4.0 | https://commons.wikimedia.org/wiki/File:Minimoog_Model_D_Synthesizer_at_SMEM_Playroom.jpg |
| A 1979 unit beside a 2017 reissue (4349x1339) | Qwave | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Minimoog_1979_left_2017_right.jpg |
| 2016 reissue from the left (7952x5304): aluminium side plate, panel tilt, LFO knob on the left-hand panel | Nir Yaniv | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Moog_Model_D_Synthesizer.jpg |
| Mixer close-up, nearly straight on (1600x1200) | Ishmael Hallin | CC BY-SA 2.0 | https://commons.wikimedia.org/wiki/File:Minimoog_mixer.jpg (https://www.flickr.com/photos/14877943@N07/4297274197/) |
| Unit at the Moogseum, NAMM 2010, from the front right (3648x2736): right cheek, case depth | Vincent Liu | CC BY 2.0 | https://commons.wikimedia.org/wiki/File:Minimoog_@_Moogseum,_NAMM_2010.jpg (https://www.flickr.com/photos/30438408@N00/4275959882/) |
| Close-up of the wheels and Controllers section from the left | kpr2 | CC0 | (the copy stored above) |

Checked, not downloaded (free licenses, lower value): `Minimoog, Deutsches Museum.jpg` (CC BY 2.0,
1024x683), `Minimoog model D (1975) (by Alain, 6919141447).jpg` (CC BY 2.0, 1600x1200),
`Minimoog, Museum of Making Music.jpg` (CC BY-SA 2.0, a sign covers the panel),
`Minimoog model D (early 1970s), MIM PHX.jpg` (CC BY-SA 2.0, angled), `R.A.Moog minimoog 1.jpg`
and `2.jpg` (CC BY-SA 2.0, an early R.A. Moog unit). No NC, ND or fair-use image was used.

## Manufacturer documents (copyrighted, local reference only, NOT redistributable)

The reading copies are not in this repository. They live in `audiotools-private/minimoog-ref/` on
the maintainer's machine and can be downloaded from the URLs below. They must not be committed,
bundled or redistributed.

| Key | Document | Source | Owner |
|---|---|---|---|
| [OM] | *A Guide to the Operation of the Mini Moog Model D* (owner's manual, text by Thom Rhea, 1971), a re-typeset edition with the July 1971 accessory sheets and the 4/12/72 back-panel adjustment sheet, 24 pp. | https://archive.org/details/synthmanual-moog-minimoog-owners-manual | Moog Music |
| [SM] | *Service Manual for Moog Minimoog Model 204D* (Norlin era, manual no. 993443232402), 82 pp. Scanned images; archive.org also provides an OCR text layer (`_text.pdf`) | https://archive.org/details/synthmanual-moog-minimoog-service-notes | Moog Music / Norlin |
| [SCH] | R.A. Moog Co. schematics for the Mini "D" (drawings 1429 to 1456, dated Nov 1970 to Jul 1971; the filter sheet is marked SN 1130), 14 large sheets | https://archive.org/details/moog_MINIMOOG-D_SCHEMATICS | Moog Music |
| [RM] | *Minimoog Model D User's Manual*, Ver. 2 (covers the reissue; mentions the spring-loaded pitch wheel of the 2022 run), 84 pp. Its patch-sheet panel drawing (p.55) is vector art and is the source of the control coordinates | https://archive.org/details/minimoog-model-d-manual | Moog Music Inc. |
| [SB], [LW] | *Minimoog Service Bulletins*; Kevin Lightner, *Minimoog Modwheel Bleedthru Explained* | https://archive.org/details/JL11354, https://archive.org/details/JL11353 | Moog Music; K. Lightner |

Papers (free downloads, kept in `audiotools-private/minimoog-ref/papers/`): Stinchcombe 2008,
Huovilainen 2004, Stilson and Smith 1996, Daly 2012 (full references in `../hardware-spec.md`
section 9).
