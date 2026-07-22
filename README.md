# TART — Terminal ASCII (Art) Rendering Tool

A small, suckless-flavored TUI editor for ASCII/Unicode art and animation.
C99 + ncursesw, no other dependencies. Built as the asset tool for a terminal
ASCII adventure game: multi-frame animations (Stone Story / Sanctuary RPG
style) with stacked layers, so a character can be drawn over a background.

## Build & run

    make        # clang, -Wall -Werror, AddressSanitizer on by default
    make run

Requires a terminal with SGR mouse support (xterm, Windows Terminal, kitty,
alacritty, ...) and a font with box-drawing glyphs — i.e. anything modern.

## Layout

    ┌ menu bar (File/Edit/View/Image/Anim/Layer/Help — Alt+letter or click)
    ├ x-ruler (numbers every 10 columns; ticks are the ┬ on the border below)
    ├ tool panel │ y-ruler │ canvas viewport (solid ┌─┐ frame)
    ├ anim bar (frame/layer controls, play/stop, onion, fps — all clickable)
    └ status/command bar (mode badge, tool, offset, cursor, frm/lyr)

The canvas is a world-coordinate document (up to 512x256); the viewport pans
over it. Outside the document edge is a dotted "void" you can pan/steer into
by a small margin.

## Keys (F1 or `:help` in-app)

| | |
|---|---|
| Tools | `d` draw, `e` eraser, `r` rect, `s` select, `v` move |
| Shapes | `L` line, `C` curve (drag ends, then click to set the bend), `E` ellipse, `T` triangle — `Esc` cancels a half-built shape |
| Fill | `f` bucket fill (click repaints the connected region), `F` toggles random texture mode |
| Brush | `t` char palette, `-`/`=` size, `Tab` shape, `F` shape fill (rect/ellipse/triangle), `c` move cut/stamp |
| Triangle | apex follows the drag: release below the press row → points down, above → points up |
| Edit | `u` undo, `U` redo, `H` history viewer (`Up`/`Down` walk the timeline live) — one step per stroke/stamp; structural ops reset history |
| Color | `[`/`]` cycle the quick palette (white, gray, ROYGBIV, black), click a Color dot to jump |
| Frames | `<`/`>` prev/next, `a` add blank, `A` duplicate, `x` delete, `Enter` play/stop, `o` onion skin, `:fps N` |
| Layers | `{`/`}` prev/next, `:layer-add` / `:layer-del` (or the Layer menu); `:solo` (or the anim-bar `solo` button) dims ink from non-active layers so the layer you're editing pops |
| Cursor | `h j k l`, `w`/`b` (±5), `0`/`$`, `g`/`G` |
| View | arrows pan, wheel scrolls, Alt+wheel / Shift+wheel / tilt-wheel scrolls horizontally, `m` pan mode, `Home` reset, `End` jump to bottom-right |
| Grid | `:grid` cycles off → rect → iso; `:grid WxH` sets rect cell size (default 8×4 ≈ square on screen); iso draws 2:1 diagonal diamonds `grid_w` wide. Move tool snaps to the grid while one is shown; `:snap` toggles that |
| Draw | hold left mouse, or hold `space` while moving; middle-drag pans from any tool |
| File | bare `:save` re-saves to the current file (vim `:w` style); status bar shows the name with `*` when unsaved; `quit`/`new`/`load` on a dirty document ask — Enter saves first (prompting for a name if needed), `d` discards, `Esc` cancels |
| Command | `:`, `/` or `Ctrl+E`, fuzzy search, Enter runs (`quit`, `save`, `load`, `resize`, ...) |
| Quit | `q` |

## Document model

A document is `frame_count` animation frames × `layer_count` layers (uniform
grid — adding a layer adds it to every frame), each an independent
`width` × `height` canvas. A space cell is **transparent**: rendering
composites a frame's layers top-down and the topmost inked glyph wins, so a
character layer stacks over a background layer. Editing (all tools, select,
move) applies to the active layer of the active frame only; duplicating a
frame (`A`) copies all its layers, which is how a static background carries
across an animation.

## Animation guide

Two worked examples live in `examples/`:

- **`walk.tart`** — a stick figure's 4-key-frame walk cycle over a ground
  layer, no color. Open it in a text editor first: you can see each frame's
  pose directly in the file. The four keys are the classic pair of poses,
  twice: **contact** (full stride), **passing**, contact with the arm swing
  reversed, passing on the other leg. Each leg is a single character whose
  glyph is the leg's *state* (Stone Story style): `/` `\` striding, `|`
  planted, `)` heel trailing, `(` swinging through. Any walk reads as a
  walk with just these four frames and five glyphs.
- **`campfire.tart`** — 4 flame flicker frames over a static scene, with a
  `COLOR` block on the flame layer only (yellow tips → orange → red base).

The keyframe workflow, using the walk as the model:

1. Draw the background on layer 1 (ground, scenery). `:layer-add`, then draw
   the character's first pose on layer 2.
2. `A` duplicates the current frame — all layers copied — so the background
   carries forward and the pose is your starting point for the next key.
3. Nudge the copy into the next pose. Select+move only touches the active
   layer, so you can drag a limb without carving a hole in the background.
4. `o` turns on onion skin: the previous frame ghosts through in dim
   wherever the current frame is blank. Step with `n`/`N` and check the
   spacing between poses — big jumps read as fast motion, small ones slow.
5. `Enter` to play, `:fps N` to tune the speed (a 4-frame walk sits well at
   6–10 fps), `Enter` again to stop.

Shape-tool notes: ellipse and triangle fit the dragged bounding box, and
terminal cells are roughly 2:1 (tall), so a round-looking circle needs a box
about twice as wide as it is high. All shapes stamp with the draw tool's
brush character and current color; the drag preview traces the exact cells
the commit will paint (both walk `tart_shape_points`).

Fill notes: the bucket repaints the 4-connected region matching the clicked
cell's glyph+color, so shape/rect outlines act as natural boundaries. Random
mode is a texture fill — each cell rolls a random glyph from the palette
category containing the fill char (pick `.` for scattered punctuation, `o`
for shape glyphs, `│` for light lines), which is how you texture grass,
water, or stone for game scenes; clicking the same region again re-rolls it.

Tips for game assets:

- Animate **in place** (like `walk.tart`) — the game engine moves the
  sprite; the cycle just has to loop.
- Keep the character and scene on separate layers even for single frames:
  the game can composite them independently later, and you can re-dress the
  same walk over any background.
- Frames are cheap to reorder by hand in the file — each is just a block of
  rows after a `FRAME` line.

## File format (`.tart`)

One plain-text file per document, frames delimited by marker lines:

    <width> <height> [fps]
    <height rows of glyphs>          frame 1, layer 1 (bottom)
    COLOR                            optional per layer:
    <height rows of 2-hex-digit xterm-256 indices; 00 = default>
    LAYER                            next layer of the same frame
    <rows...>
    FRAME                            next frame (restarts at layer 1)
    <rows...>

Color index 0 means "no color"; cube-black (16) stands in for real black.
A v1 file (no fps, no markers) loads as one frame of one layer, and a
one-frame/one-layer save is still v1-shaped. The format stays line-oriented
and diffable; ragged layer counts are padded with blank layers on load.

## Map format (`.map`) — blocks + click-to-place editor

A map composites many small `.tart` documents ("blocks" — grass, water, a
campfire) onto a grid. Same line-oriented, diffable spirit as `.tart`:

    <map_w> <map_h> <block_w> <block_h>
    BLOCKS <block_count>
    <id> <walkable 0|1> <path>          repeated block_count times
    GRID
    <map_h rows of map_w whitespace-separated ids, or "." for empty>

Loader: `tart_map_load`/`tart_map_free`/`tart_map_composite` in `tart.h`.
Each `BLOCKS` entry loads once via `tart_load_doc` — every grid cell using
that id shares the *same* loaded doc, so repeated placements (a pond made
of several `water` cells) share one animation clock, same as how a crowd of
mobs already shares one shamble clock in Wordsworn. `tart_map_composite`
only reads whichever frame is currently selected; the caller advances each
block's `cur_frame` on its own tick, same division of responsibility the
game already uses for sprites. Walkability is a whole-block flag for now —
no per-cell collision inside a block yet.

`examples/grass.tart` + `examples/water.tart` + `examples/meadow.map` are a
worked example (a 4×3-block meadow, 8×4 cells/block); `make maptest` builds
a headless harness that loads it, prints two composited frames to stdout,
and exits 1 on any load failure — no ncurses/terminal needed, good for a
quick regression check after touching the loader.

`make mapedit` builds `mapedit`, a click-to-place authoring mode for
`.map` files. Deliberately a separate small binary rather than a mode
bolted onto `tart` itself — a map is a grid of block *references*, not
pixels, so it doesn't share Canvas/Frame/Layer's data model; forcing it
into the same tool-panel/undo machinery built for pixel drawing would
have meant real surgery on `input.c` for a mismatched fit. It reuses the
same loader/compositor `tart`/`maptest`/Wordsworn all share, and mirrors
`init_app`'s ncurses setup, but the editing loop itself is new and much
smaller — there is exactly one operation (place/clear a block at a grid
cell), not a dozen drawing tools:

```
make mapedit
./mapedit examples/meadow.map
```

Arrow keys/`hjkl` move the cursor one block at a time, `[`/`]`/Tab cycle
which block is "loaded", Enter/Space place it, `x` clears, `u` undoes,
`s` saves, `q` quits (prompts on unsaved changes). Left-click a legend
row to pick a block, left-click the grid to move the cursor there *and*
place immediately. No animation clock in this mode on purpose — block
preview is static (frame 0); `getch()` runs in plain blocking mode, no
`timeout()`/polling of any kind.

**Not built yet:** per-cell walkability (still a whole-block flag),
resizing the map or adding/removing `BLOCKS` entries from inside the
editor (hand-edit the `.map` file for that today).

## UI themes

10 named palettes, ported from Slate (sibling project) so all three read
from the same visual language: Light, Dark (default), Solarized
Dark/Light, Gruvbox Dark/Light, Flexoki Dark/Light, Mosscairn Dark/Light.
`:theme` (or View menu) cycles to the next one; `:theme <name>` jumps
straight to one instead, matching case-insensitively against either the
persisted key (`gruvbox_dark`) or the display label (`Gruvbox Dark`) —
`:theme gru` is enough. The current theme's name is always visible in
the status bar's bottom-right corner (`theme:Gruvbox Dark`, right next
to the `: commands` hint) so cycling blind still shows where you landed.
The choice is written to `~/.tart-theme` (one line, plain text) and
shared with Wordsworn — switch it in either app, the other picks it up
on its next launch. Wordsworn itself cycles with the `t` key in-game;
it has no command-palette/text-input of its own to jump-by-name from,
so that half stays TART-only.

Covers the whole body — menu bar, tool panel, rulers, status bar, overlay
text (pairs 1-6) and every unpainted cell (`bkgd()`, so empty canvas
space matches too), not just the menu chrome the first pass shipped with.
Still does NOT touch the 256-color canvas ramp sprites/art are drawn in —
a UI preference must never shift what a saved `.tart`/`.map` actually
looks like. Each theme is stored as plain RGB and matched to its nearest
xterm-256 pair at apply time (`tart_apply_theme` in `tart.h`) — one shared
conversion function, not ten hand-picked magic pair tables.

## Code map

| File | Contents |
|---|---|
| `main.c` | 15-line entry point: init, render/input loop, cleanup. |
| `tart.h` | Single-header library: types, constants, and (under `TART_IMPLEMENTATION`, defined only by `main.c`) all rendering, layout, save/load, and color code. |
| `input.c` | All input: SGR mouse decoding, keyboard dispatch, tool behaviors, command execution. Includes `tart.h` declarations only. |

Conventions worth knowing before editing:

- **Coordinates:** `sx`/`sy` are screen cells, `wx`/`wy` are world (document)
  cells; `offset_x`/`offset_y` is the world position under the viewport's
  top-left corner. Declared once at the top of `tart.h`.
- **Renderer/hit-test pairing:** anything clickable is described by one
  shared table so drawing and click handling can't drift: the tool panel
  (`tart_build_tool_panel` + `TP_ROW_*`), the menu bar (`tart_build_menu_bar`),
  the anim bar (`tart_build_anim_bar`), popup geometry
  (`tart_compute_*_geom`), and command matching (`tart_command_matches`).
- **One command dispatcher:** menus, the `:` palette, and Alt+letter all
  funnel into `tart_execute_command` in `input.c`.
- **Mouse:** ncurses can't parse SGR (1006) reports, so `handle_input`
  decodes `\E[<...M/m` by hand; the ncurses `getmouse` path is only a
  fallback. All decoded events funnel into `tart_apply_mouse`.
- **Color pairs:** 1–6 are UI chrome; 16–271 map the xterm-256 palette for
  canvas cells. `Canvas.colors` stores palette indices, 0 = uncolored.
