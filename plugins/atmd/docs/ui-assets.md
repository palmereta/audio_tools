# ATMD editor art: the `assets/ui` contract and how to re-render it

The editor shows a photoreal render of the whole instrument: the tilted panel on top, the wooden
name board, then the keyboard deck with the left-hand controller (LHC). Everything that moves or
lights up is a sprite drawn over one static background:

- knobs, rotary switches and rockers;
- the two lamps;
- the two wheels;
- pressed keys, which come from a second render of the keyboard with every key down.

All art is drawn from `assets/layoutmd.json`. It carries no manufacturer logo: the name board
carries our own "ATMD / Audio Tools" plate.

| Step | Script | Runs on | Output |
|---|---|---|---|
| 1. Silkscreen | `tools/panelmd.py` | Mac (Pillow, Futura from macOS) | `assets/silkscreenmd.png` (+ `_preview.jpg`), `assets/lhcmd.png`, `assets/nameplatemd.png` |
| 2. Renders | `tools/blender/scenemd.py` | pc50-wsl (Blender 5.2.2, CUDA) | `out/`: `bg.png`, `keysdown.png`, `bg.json`, `sprites.json`, sprite frames; `hero_*.png`; `film_<shot>/f_####.png` |
| 3. Packing | `tools/pack_assetsmd.py` | Mac | `assets/ui/` (about 6.9 MB) |

Raw renders go in `assets/render/out/`, which is git-ignored.

---

## 1. Coordinate systems

There are three coordinate systems. All values are in mm unless noted.

**Panel** (`layoutmd.json` → `panel`, `knobs`, `switches`, `lamps`, `jacks`, `texts`):
- The origin is the top-left corner of the printed face.
- x runs right; y runs *down the face*.
- h is the height above the face, along its normal.
- The panel is hinged at `panel.hinge.y` = 182.8 and tilted `panel.tilt_deg.default` = 60°.
- In case coordinates it sits centred between the cheeks: case x = panel x + `panelXOffsetMm` (27.4).

**Deck / case** (`layoutmd.json` → `keyboard`, `deck.lhc`, `case`):
- The origin is the rear-left-bottom corner of the case (the outer face of the left cheek).
- x runs right; y runs toward the player; z runs up.
- Reference heights (also in `ui.json → projection.heights`):

| Surface | z (mm) |
|---|---|
| White key top | 70 |
| Black key top | 82 |
| LHC plate | 66 |
| Front lip top | 58 |

**Image px**:
- The origin is the top-left corner of `bg.jpg`, which is 3004 x 1788 px at 4.0 px/mm.
- The images are rendered at 2x. The editor works at 1x, so divide every px value by 2 (`editorScale` = 0.5).

## 2. The camera and the projection

- The camera is orthographic.
- It looks along the **bisector of the panel normal and the vertical**: 30° from vertical toward the player.
- Both planes therefore get the same foreshortening, cos 30° = 0.866, along their "y".
- The image spans the case width plus 12 mm on each side. Vertically it runs from 7 mm above the
  top rail to 6 mm below the bottom of the case front.

A layout point maps to a background pixel through an affine map, one per plane. The map is exact:
the render script compares it with Blender's own camera projection, and the error is 0.0005 px
(`projection.checkPx`).

```
panel:  px = 4.0 * x + 157.6          py = 3.4641 * y + 126.346 + 2.0 * h
deck:   px = 4.0 * x +  48.0          py = 3.4641 * y + 257.443 - 2.0 * z
```

The numbers above come from `ui.json → projection.panel` and `projection.deck`:

```
"panel": {"affine": [[a00, 0, a02], [0, a11, a12]], "perMmHeight": [0, hp]}
"deck":  {"affine": [[a00, 0, a02], [0, a11, a12]], "perMmHeight": [0, hd]}
px = a00*x + a02
py = a11*y + a12 + perMmHeight[1] * (h for the panel | z for the deck)
```

What the map means for points above a surface:
- A point *above* the panel (a knob top, h > 0) moves **down** on screen, by 0.5 px per mm of
  height at 1x.
- A point above a deck surface moves **up** on screen.
- Sprites are rendered in their own plane, so this is already baked into them. The editor only
  projects the base point (the anchor).

Example: `tune` at panel (44.3, 66.5) → px (334.8, 356.7) → editor (167.4, 178.35).

## 3. `ui.json`

| Key | Content |
|---|---|
| `version`, `generatedBy`, `editorScale` | 1; the script name; 0.5 (multiply px by this for 1x) |
| `background` | `file` = "bg.jpg", `width` 3004, `height` 1788, `pxPerMm` 4.0 |
| `projection` | `camera` (tiltDeg 60, viewFromVerticalDeg 30), `panel`, `deck` (section 2), `heights`, `x0Mm`/`s0Mm` (image origin in screen mm), `checkPx` |
| `keys` | `file` = "keysdown.jpg", `crop` = [x0, y0, x1, y1] (where keysdown.jpg sits in bg px: 432, 1110, 2900, 1622), `firstNote` 41, `lastNote` 84, `keys[]` = {`note`, `black`, `hull`: [[px, py], ...]} |
| `sprites` | name → {`file`, `w`, `h`, `frames`, `cols`, `rows`, `anchor`: [ax, ay], `plane`, extras} |
| `controls[]` | one entry per drawn control (below) |
| `wheels[]` | {`id`, `px`, `hit`}: a shortcut copy of the two wheel controls |

### Sprites

- A sheet has `frames` cells of `w` x `h`, laid out row by row in `cols` columns.
- Frame f sits at (`(f % cols)*w`, `(f / cols)*h`).
- To draw a sprite, put its `anchor` pixel on the control's `px`:
  `drawImage(cell, px - anchor)`, at 1x with everything halved.
- Every sprite carries its own soft shadow (premultiplied alpha), so it composites straight onto `bg.jpg`.

| Sprite | Size (px) | Frames | Used by |
|---|---|---|---|
| `knob_std` | 192x192 | 128 (16x8) | `tune`, `glide`, `mod_mix`, mixer volumes, filter and contour knobs, `volume`, `phones_volume` (25 mm skirt) |
| `knob_big` | 216x216 | 128 | `osc2_freq`, `osc3_freq` (30 mm) |
| `knob_ptr` | 200x200 | 128 | `osc1..3_range`, `osc1..3_wave` (pointer knob) |
| `rocker_<colour>_<h\|v>_<0\|1>` | 160x104 or 104x160 | 1 | panel rockers: orange h/v, blue h/v, black v |
| `rocker_cream_h_<0\|1>` | 168x112 | 1 | `glide_on`, `decay_on` (deck plane) |
| `lamp_overload_<0\|1>`, `lamp_power_led_<0\|1>` | 124, 104 | 1 | lamps: bezel and jewel, with a halo baked into the lit state |
| `pitch_wheel`, `mod_wheel` | 136x344 | 33 (11x3) | the wheels, including the dark slot under them |

The sprites cover their own wells:
- The background has the dark rocker openings and the wheel slots, but no caps, bezels or wheels.
- Lamps are not in the background at all; draw state 0 when a lamp is off.

**Knob frames:**
- Frame 0 has the pointer at 12 o'clock on the panel; frames run clockwise, `degPerFrame` = 2.8125.
- Frame index: `frame = round(angleDeg / 360 * 128) mod 128`, where the angle is the dial angle of
  `layoutmd.json` (negative = counter-clockwise).
- Standard and large knobs sweep `a0..a1` = -150..+150.
- The pointer knobs stop at `detentsDeg` (-87.5 ... +87.5). The nearest frame is within 1.4°.

**Rocker state index:**
- 0 = left half (horizontal) or bottom half (vertical) pressed in = **off**.
- 1 = right or top half pressed in = **on** (or the second legend, e.g. `noise_color` 1 = WHITE).

**Wheel frames:** `value = values[0] + (values[1] - values[0]) * f / (frames - 1)`.
- `pitch_wheel` runs -1..+1; frame 16 is the centre detent, where the notch shows.
- `mod_wheel` runs 0..1; 0 = rolled fully toward the player.
- Pushing a wheel away from the player raises its value.

### `controls[]`

Every control has `id`, `type`, `plane` (`panel` | `deck`), its layout `x`/`y` (and `z` for the
deck) and `px`, the anchor in bg px. Then, by type:

| type | Extra fields |
|---|---|
| `knob` | `sprite`, `a0`, `a1`, `legends` (from the layout), `hitRadiusPx` [rx, ry]: an ellipse, since y is foreshortened |
| `rotary` | `sprite` = knob_ptr, `steps` 6, `detentsDeg`, `hitRadiusPx` |
| `rocker` | `sprites` [off, on], `orient`, `hit`: the projected cap rectangle (4 px points) |
| `lamp` | `sprites` [off, on] |
| `wheel` | `sprite`, `values`, `centerDetent`, `hit` (slot rectangle plus margin), `dragPxPerFullTravel` (projected slot length in 2x px) |

### Keys

- `hull` is a polygon in bg px for each MIDI note, F2 (41) to C6 (84). Test black keys first.
- **White hull:**
  - the real top outline: the narrow tail between the black keys and the full-width head;
  - clipped at the rear by the name-board edge;
  - extended at the front down the key's front face to the wooden lip, so it covers the key both
    up and pressed.
- **Black hull:** the base rectangle, from the name-board edge to the foot of the sloped front.
- **Pressed key:** draw `keysdown.jpg` at (`crop[0]`, `crop[1]`), clipped to that key's hull.
  `keysdown.jpg` is the same camera with every key pivoted 9 mm down at the front (pivot at y = 150,
  under the panel).
- Known limit: a pressed white key next to a black key shows a sliver of that black key's
  *pressed* front. It is about 1 mm at 1x and looks like the black key's exposed side.

## 4. Re-rendering

On pc50 the working folder is `~/ai/atmd/`. `run.sh` wraps every Blender run in the shared GPU
lock:

```sh
# run.sh (already on pc50):
#   flock ~/ai/prerendered/.gpu.lock ~/ai/blender/blender-5.2.2-linux-x64/blender -b -P scenemd.py "$@"

# 1. Mac: silkscreen and plates
python3 plugins/atmd/tools/panelmd.py

# 2. Copy inputs to pc50
cd plugins/atmd
scp tools/blender/scenemd.py assets/layoutmd.json assets/silkscreenmd.png assets/lhcmd.png \
    assets/nameplatemd.png pc50-wsl:ai/atmd/

# 3. Render the editor art (background, keys down, all sprites)
ssh pc50-wsl 'cd ~/ai/atmd && rm -rf out && mkdir out && \
  ./run.sh bg.log -- --mode bg --samples 512 && ./run.sh sprites.log -- --mode sprites --samples 384'
#   Re-render a subset of sprites: --mode sprites --only knob_ptr,rocker_blue
#   (sprites.json is merged, not replaced)

# 4. Bring the renders back and pack (also writes composite_test*.jpg)
rm -rf assets/render/out && mkdir -p assets/render/out
rsync -a pc50-wsl:ai/atmd/out/ assets/render/out/
rm -f assets/ui/* && python3 tools/pack_assetsmd.py --test

# 5. Web stills and film
ssh pc50-wsl 'cd ~/ai/atmd && ./run.sh hero.log -- --mode hero --out outh --samples 512'
#   --shots front,panel,keys   --w 2400 --h 1100   --preset <json with "values">
ssh pc50-wsl 'cd ~/ai/atmd && for s in wide panel keys; do \
  ./run.sh film_$s.log -- --mode film --out outf --shot $s --frames 120 --samples 256 --w 1920 --h 1080; done'
#   --probe N renders only frame N

# Quick check (bg camera, half size, controls populated in 3D: the ground truth for the sprites)
ssh pc50-wsl 'cd ~/ai/atmd && ./run.sh prev.log -- --mode preview --samples 48 --pct 50'
#   --pct 100 gives an image the same size as bg.png; --nopop leaves the controls out
```

**Disk:**
- C: (which backs the WSL disk) had about 12 GB free, and WSL does not give space back.
- The editor renders take 23 MB; a 1920x1080 film frame takes about 3.5 MB (about 420 MB per
  120-frame shot).
- Delete `outf/` frames once they are encoded.

**Verifying sprite alignment:**
- `pack_assetsmd.py --test` writes `composite_test.jpg` (bg plus every sprite at its default) and
  `composite_test2.jpg` (pressed keys, moved wheels, other states).
- Compare them with `preview --pct 100`, which renders the same camera with real 3D controls.
- On 2026-10-18 the difference image showed no position offsets.

## 5. Measured render times (RTX 3090, CUDA, OIDN on the GPU, 2026-10-18)

| Job | Settings | Time |
|---|---|---|
| `bg` (3004x1788) | 512 spp | 28-29 s |
| `keysdown` (border crop 2468x512) | 512 spp | 16 s |
| `sprites` (3 x 128 knob frames, 12 rockers, 4 lamps, 2 x 33 wheel frames) | 384 spp | 131 s (each knob strip about 29 s) |
| `hero` (3 x 2400x1100) | 512 spp | about 40 s (12-14 s per shot) |
| `film` (1920x1080) | 256 spp | 12 frames in 86 s, i.e. about 7 s per frame: a 120-frame shot is about 14 min (single probe frame 7-9 s including scene build) |
| `preview` (bg camera) | 48 spp | about 10 s |

## 6. Decisions and deviations from the layout

- **Name-board front height: 88 mm, not 78.** The layout gives 78 mm (an estimate, ±10 mm). At 78
  the 82 mm black-key tops would pass through the board. The front face drops to 75 mm and hides
  the key tails. The black keys start at y = 293.5, just behind that face.
- **Panel side plates and box depth (42 mm) are estimates** taken from the photos and the case
  height. The tilted panel box is a wedge whose bottom edge runs to y = 100.
- **Rocker caps** have a V-shaped top (both ends high). They tilt ±10° about a pivot 4 mm under the face.
- **The wheels are modelled as full discs** (r = 38 mm, 120 ribs; the pitch wheel has a 6.4°
  centre notch). Only the arc above the plate is ever visible.
- **Lamp glow** is added by the packer (a Gaussian halo of the bright pixels). Cycles has no bloom.
- **Lighting:**
  - The editor scene uses only direction-dependent lights: three suns and a procedural studio dome.
    A sprite rendered alone at the origin is therefore lit exactly like the same control in place.
  - Sprite shadow catchers are 2 m planes, so that light from behind the panel plane is blocked as
    it is in the full scene.
  - The hero and film modes add area lights and a floor.
