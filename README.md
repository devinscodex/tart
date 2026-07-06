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
| Tools | `p` pencil, `e` eraser, `r` rect, `s` select, `m` move |
| Brush | `t` char palette, `-`/`=` size, `Tab` shape, `f` rect-fill, `c` move cut/stamp |
| Color | `[`/`]` quick palette, click the Color row for the HSL picker |
| Frames | `n`/`N` next/prev, `a` add blank, `d` duplicate, `D` delete, `Enter` play/stop, `o` onion skin, `:fps N` |
| Layers | `{`/`}` prev/next, `:layer-add` / `:layer-del` (or the Layer menu) |
| Cursor | `h j k l`, `w`/`b` (±5), `0`/`$`, `g`/`G` |
| View | arrows pan, wheel scrolls, Alt+wheel / Shift+wheel / tilt-wheel scrolls horizontally, `v` pan mode, `Home` reset, `End` jump to bottom-right |
| Draw | hold left mouse, or hold `space` while moving |
| Command | `:` or `Ctrl+E`, fuzzy search, Enter runs (`quit`, `save`, `load`, `resize`, ...) |
| Quit | `q` |

## Document model

A document is `frame_count` animation frames × `layer_count` layers (uniform
grid — adding a layer adds it to every frame), each an independent
`width` × `height` canvas. A space cell is **transparent**: rendering
composites a frame's layers top-down and the topmost inked glyph wins, so a
character layer stacks over a background layer. Editing (all tools, select,
move) applies to the active layer of the active frame only; duplicating a
frame (`d`) copies all its layers, which is how a static background carries
across an animation.

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
