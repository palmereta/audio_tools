# Reference material for AT700

## Free-licensed photographs (used only as layout references; not shipped)

| Subject | Author | License | Source |
|---|---|---|---|
| Full 7-block System-700 with the 701A keyboard, front view | Racingline81 (own work, 2017-05-10) | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Roland_System_700.jpg |
| Main Console (Block 1) and 717A sequencer (Block 3), front view | Chris Sobczak (Flickr, 2008-03-06) | CC BY 2.0 | https://commons.wikimedia.org/wiki/File:Roland_System_700_in_Utrecht.jpg |
| Block layout drawing | Felix2036 (2007) | Public domain | https://commons.wikimedia.org/wiki/File:Roland_System_700_blocks.GIF |

Three more photos (Flickr, CC BY-NC-SA) were consulted for cabinet and cable details; their license
does not allow use in this project's artwork, so none of them was traced or redistributed.

## Manufacturer documents (copyrighted, local reference only, NOT redistributable)

Not in this repository. The reading copies live outside it (`audiotools-private/at700-ref/` on the
maintainer's machine). They must not be committed, bundled or redistributed (`.gitignore` here
blocks `*.pdf` as a safety net).

| Document | Source | Owner |
|---|---|---|
| System 700 Synthesizer Instruction Manual (1976, printed Feb '77), 224 pp. | https://archive.org/details/synthmanual_System_700_owners_manual | Roland Corporation, all rights reserved |
| System 700 brochure | archive.org item `synthmanual_System_700_brochure.jpg` | Roland Corporation |
| Redrawn service-note schematics (702, 703B/C, 703E/F/G, 704, 705A, 706A, 707, 708A, 709A/B, 712A, 716A, 718A) | https://yusynth.net/gear/R700_MC_en.html | Roland circuits; redraws by Yves Usson |
| Service-note schematic scans (701 to 723) | https://www.florian-anwander.de/roland_system700/ | Roland Corporation |

The module positions and dimensions in `tools/panel700.py` were measured on the scaled panel
drawings of the instruction manual (each module page shows the panel at 1.565 px/mm in a 90 dpi
render). The AT700 artwork is drawn from scratch from those coordinates: no Roland logo, no
"SYSTEM-700" badge, no copied graphics. Module numbers (702A, 703E...) are kept as functional
labels, as in the manuals of the clones listed in `../hardware-spec.md` §6.
