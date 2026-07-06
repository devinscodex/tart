#ifndef TART_H
#define TART_H

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L    /* clock_gettime for animation playback */
#endif
#include <ncursesw/curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#define TART_VERSION "0.4.0"

/* Coordinate convention used throughout: sx/sy name a screen (terminal cell)
 * position; wx/wy name a world (canvas/document) position. offset_x/offset_y
 * is the world coordinate currently sitting under the canvas viewport's
 * top-left screen cell, i.e. the pan origin. */

#define MAX_CANVAS_WIDTH  512
#define MAX_CANVAS_HEIGHT 256
#define MAX_FRAMES 128
#define MAX_LAYERS 8

#ifndef TART_DEFAULT_CANVAS_WIDTH
#define TART_DEFAULT_CANVAS_WIDTH 120
#endif
#ifndef TART_DEFAULT_CANVAS_HEIGHT
#define TART_DEFAULT_CANVAS_HEIGHT 30
#endif

#define TOOL_PANEL_WIDTH 18
#define Y_RULER_WIDTH 5
#define CANVAS_START_X (TOOL_PANEL_WIDTH + Y_RULER_WIDTH)
/* Visible canvas viewport size in screen cells. -1 on the width reserves the
 * column the right border renders on (without it, canvas.x + canvas.w lands
 * exactly on COLS, one past the last valid column, so that border would
 * silently never appear); -6 on the height reserves 3 header rows plus one
 * dedicated row each for the bottom border, the animation bar, and the
 * status/command bar. Kept as macros since several places outside
 * tart_compute_layout need to agree with them exactly (view clamping,
 * End-key centering, page-scroll amounts, ...). */
#define CANVAS_VIEW_W (COLS - CANVAS_START_X - 1)
#define CANVAS_VIEW_H (LINES - 6)

#define TART_RECT(x_, y_, w_, h_) ((TartRect){ .x = (x_), .y = (y_), .w = (w_), .h = (h_) })

#define CMD_PALETTE_MAX_VISIBLE 6

/* One entry in the ':' command palette / menu system. alias is an optional
 * short form (e.g. "q" for "quit"); desc is what menus display instead of
 * the raw name. */
typedef struct {
    const char *name;
    const char *alias;
    const char *desc;
} TartCommand;

static const TartCommand TART_COMMANDS[] = {
    { "quit", "q",     "Quit TART" },
    { "help", "binds", "Show keybindings" },
    { "about", NULL,   "About TART" },
    { "undo", "u",     "Undo last edit" },
    { "redo", NULL,    "Redo undone edit" },
    { "history", NULL, "View undo/redo history" },
    { "deselect", NULL, "Clear current selection" },
    { "reset-view", NULL, "Reset pan offset to origin" },
    { "toggle-pan", NULL, "Toggle pan/normal mode" },
    { "resize", NULL, "Resize canvas (WxH)" },
    { "save", NULL, "Save to file" },
    { "load", NULL, "Load from file" },
    { "play", NULL, "Play/stop animation" },
    { "onion", NULL, "Toggle onion skin" },
    { "fps", NULL, "Set playback FPS" },
    { "frame-add", "fa", "Add blank frame" },
    { "frame-dup", "fd", "Duplicate frame" },
    { "frame-del", NULL, "Delete frame" },
    { "layer-add", "la", "Add layer" },
    { "layer-del", NULL, "Delete layer" },
};
#define TART_COMMAND_COUNT (sizeof(TART_COMMANDS) / sizeof(TART_COMMANDS[0]))

/* One top-level menu: a label plus the command names (from TART_COMMANDS)
 * shown in its dropdown, in order. Every label's first letter doubles as
 * its Alt+<letter> mnemonic, so they must stay unique. */
typedef struct {
    const char *label;
    const char *items[5];
    int item_count;
} TartMenuDef;

static const TartMenuDef TART_MENUS[] = {
    { "File",  { "save", "load", "quit" },              3 },
    { "Edit",  { "undo", "redo", "history", "deselect" }, 4 },
    { "View",  { "reset-view", "toggle-pan", "onion" }, 3 },
    { "Image", { "resize" },                            1 },
    { "Anim",  { "play", "frame-add", "frame-dup", "frame-del", "fps" }, 5 },
    { "Layer", { "layer-add", "layer-del" },            2 },
    { "Help",  { "help", "about" },                     2 },
};
#define TART_MENU_COUNT (sizeof(TART_MENUS) / sizeof(TART_MENUS[0]))

/* Computed screen position of one menu-bar label, so the renderer and the
 * click hit-test in input.c agree on where each menu name sits. */
typedef struct {
    const char *label;
    int x0, x1;
} TartMenuBarItem;

typedef enum {
    TOOL_PENCIL, TOOL_ERASER, TOOL_RECT, TOOL_SELECT, TOOL_MOVE,
    TOOL_LINE, TOOL_CURVE, TOOL_ELLIPSE, TOOL_TRI, TOOL_FILL,
} TartTool;
typedef enum { BRUSH_SQUARE, BRUSH_ROUND } BrushShape;

/* PROMPT_NONE means no modal text-entry prompt is open; any other value
 * both opens the prompt (see tart_draw_prompt) and says what Enter should
 * do with its buffer once submitted. */
typedef enum {
    PROMPT_NONE,
    PROMPT_RESIZE,
    PROMPT_SAVE,
    PROMPT_LOAD,
    PROMPT_CUSTOM_CHAR,
    PROMPT_FPS,
} TartPromptMode;

/* One region of the animation bar (the frame/layer/playback controls row
 * between the canvas and the status bar); ANIM_NONE marks plain text. Same
 * renderer/hit-test pairing idea as TartPanelRow. */
typedef enum {
    ANIM_NONE,
    ANIM_FRAME_PREV, ANIM_FRAME_NEXT, ANIM_FRAME_ADD, ANIM_FRAME_DUP, ANIM_FRAME_DEL,
    ANIM_LAYER_PREV, ANIM_LAYER_NEXT, ANIM_LAYER_ADD, ANIM_LAYER_DEL,
    ANIM_PLAY, ANIM_ONION, ANIM_FPS,
} TartAnimAction;

typedef struct {
    const char *label;
    int x0, x1;
    TartAnimAction action;
} TartAnimItem;

#define TART_ANIM_MAX_ITEMS 24

/* What clicking a given tool-panel row does; see tart_apply_panel_action.
 * Shared between the panel's own click hit-test and identical keyboard
 * shortcuts ('[', ']', Tab, ...) so both paths funnel through one place. */
typedef enum {
    PANEL_NONE,
    PANEL_SELECT_TOOL,
    PANEL_CHAR_OPEN,
    PANEL_SIZE_DEC,
    PANEL_SIZE_INC,
    PANEL_SIZE_SET,      /* value = index into TART_BRUSH_SIZES */
    PANEL_SHAPE_TOGGLE,
    PANEL_RECT_FILL_TOGGLE,
    PANEL_FILL_CHAR_OPEN,
    PANEL_MOVE_CUT_TOGGLE,
    PANEL_FILL_RANDOM_TOGGLE,
    PANEL_COLOR_OPEN,
    PANEL_COLOR_SET,     /* value = index into TART_BASIC_COLORS */
} TartPanelAction;

/* Which brush the char palette popup is picking a glyph for: the pencil's
 * main tip, or the rect tool's fill character. */
typedef enum { PALETTE_TARGET_PENCIL, PALETTE_TARGET_FILL } PaletteTarget;

typedef struct {
    const char *label;
    const wchar_t *chars;
} TartPaletteCategory;

static const TartPaletteCategory TART_PALETTE[] = {
    { "Standard",           L"`~!^()-_+=;:'\",./\\|<>[]{}" },
    { "Extended",           L"´‾¡·" },
    { "Shapes",             L"oOvVTL7UcCxX" },
    { "Junctions (single)", L"├┤┬┴┼" },
    { "Junctions (double)", L"╠╣╦╩╬" },
    { "Junctions (mixed)",  L"╟╢╥╨" },
    { "Lines (light)",      L"│─" },
    { "Lines (double)",     L"║═" },
    { "Lines (heavy)",      L"┃━" },
    { "Special",            L" " },
};
#define TART_PALETTE_CAT_COUNT (sizeof(TART_PALETTE) / sizeof(TART_PALETTE[0]))
#define PALETTE_LABEL_WIDTH 20
#define PALETTE_CHAR_CELL_W 2

/* Quick-access basic colors for the '['/']' cycle keys, so common colors
 * don't require opening the full HSL picker. Values are xterm-256 indices;
 * plain black (0) is skipped because Canvas.colors uses 0 as its own
 * "no color" sentinel, so cube-black (16) stands in for it instead. */
static const unsigned char TART_BASIC_COLORS[] = { 15, 9, 10, 11, 12, 13, 14, 16 };
#define TART_BASIC_COLOR_COUNT (sizeof(TART_BASIC_COLORS) / sizeof(TART_BASIC_COLORS[0]))

/* Discrete brush/eraser sizes for the '-'/'=' cycle keys and the size
 * slider; shared with tart_cycle_size in input.c so panel and keyboard
 * agree on the same stops. */
static const int TART_BRUSH_SIZES[] = { 1, 3, 5, 7, 9, 13, 17, 21 };
#define TART_BRUSH_SIZE_COUNT (sizeof(TART_BRUSH_SIZES) / sizeof(TART_BRUSH_SIZES[0]))

typedef struct {
    int row;            /* y offset from tool_r.y */
    int x0, x1;         /* x offset range from tool_r.x, inclusive; -1,-1 = whole row */
    TartPanelAction action;
    int value;          /* payload, e.g. TartTool for PANEL_SELECT_TOOL */
} TartPanelRow;

#define TOOL_PANEL_MAX_ROWS 32

/* Row layout shared by the tool-panel renderer and its click hit-test, so
 * they can never drift out of sync with each other. */
#define TP_ROW_TOOLS_HEADER   1
#define TP_ROW_PENCIL         3
#define TP_ROW_ERASER         4
#define TP_ROW_RECT           5
#define TP_ROW_SELECT         6
#define TP_ROW_MOVE           7
#define TP_ROW_LINE           8
#define TP_ROW_CURVE          9
#define TP_ROW_ELLIPSE        10
#define TP_ROW_TRI            11
#define TP_ROW_FILL           12
#define TP_ROW_OPTIONS_HEADER 14
#define TP_ROW_OPT1           15
#define TP_ROW_OPT2           16
#define TP_ROW_OPT3           17
#define TP_ROW_SIZE_SLIDER    18    /* pencil/eraser only: "-[dots]= NN" */
#define TP_ROW_COLOR          19    /* "Color [dots]" - dots double as quick-jump targets */
#define TP_SIZE_DEC_X         9
#define TP_SIZE_INC_X         12
#define TP_SIZE_SLIDER_DOT_X0 3     /* first size-slider dot's column offset from tool_r.x */
#define TP_COLOR_SLIDER_DOT_X0 8    /* first color-slider dot's column offset from tool_r.x */

#define PAN_VOID_MARGIN 10

typedef struct {
    int width;
    int height;
    wchar_t *cells;
    unsigned char *colors; /* 0 = no color/default; 1-255 = xterm-256 palette index */
} Canvas;

/* One animation frame: a bottom-to-top stack of same-sized canvases. A cell
 * whose glyph is a space is transparent - lower layers show through - which
 * is what lets a character layer sit on top of a background layer. */
typedef struct {
    Canvas layers[MAX_LAYERS];
} Frame;

/* One undo/redo entry: a full snapshot of a single layer canvas, taken just
 * before an edit gesture (stroke, rect stamp, move paste) touches it.
 * Structural changes (frame/layer add/del, resize, load) clear both stacks
 * rather than trying to keep frame/layer indices valid across them. */
typedef struct {
    const char *desc;   /* action name for the history viewer; always a static string */
    int frame, layer;
    int width, height;
    wchar_t *cells;
    unsigned char *colors;
} TartUndoState;

#define UNDO_STACK_MAX 64

typedef struct {
    int offset_x;                   /* viewport pan origin, in world coords */
    int offset_y;
    int last_mouse_screen_x;        /* previous mouse position, for pan-drag deltas */
    int last_mouse_screen_y;
    int is_dragging;                /* pan mode active (toggled by 'v') */

    /* Active tool + its per-tool settings. */
    TartTool tool;
    wchar_t brush_char;
    int brush_size;
    BrushShape brush_shape;
    int eraser_size;
    BrushShape eraser_shape;
    bool rect_filled;
    wchar_t fill_char;

    /* Char-palette popup (pencil tip / rect fill character picker). */
    bool palette_visible;
    PaletteTarget palette_target;
    int palette_hover_row;
    int palette_hover_char;

    /* Shared drag state for rect/select/move/shape tools (see tart_tool_rect
     * etc. in input.c) - what's being dragged depends on `tool` above. */
    bool tool_dragging;
    int drag_start_wx, drag_start_wy;
    int drag_cur_wx, drag_cur_wy;

    /* Curve tool's second stage: endpoints are frozen in drag_start/drag_cur
     * and the control point rides the cursor until a click commits. */
    bool curve_pending;
    int curve_cx, curve_cy;
    bool has_selection;
    int sel_w, sel_h;
    int sel_origin_wx, sel_origin_wy;  /* top-left of the selected region, for its persistent highlight */
    wchar_t *sel_buffer;
    unsigned char *sel_colors;
    bool move_cut;  /* false = stamp (keep source, repeatable); true = cut (clears source, one-shot) */
    bool fill_random;   /* bucket fill draws random glyphs from fill_char's palette row */

    /* Color: current_color paints; the HSL picker and the '['/']' quick
     * palette are two different ways of setting it. */
    unsigned char current_color;
    int quick_color_idx;        /* position in TART_BASIC_COLORS for [/] cycling */
    bool color_picker_visible;
    int hsl_h, hsl_s, hsl_l;
    int color_slider_focus;

    /* Top menu bar (File/Edit/View/Image/Help). */
    int open_menu;               /* -1 = closed, else index into TART_MENUS */
    int menu_hover_item;

    /* ':' command palette. */
    int cmd_selected;           /* index into the current fuzzy-match list */
    int cmd_scroll_off;         /* first visible row when matches overflow the popup */

    /* Modal single-line text prompt (resize/save/load/custom-char). */
    TartPromptMode prompt_mode;
    char prompt_buffer[256];
    int prompt_len;

    /* Cell the "cursor" highlight is drawn on - kept in sync with whichever
     * of mouse or vim-style keyboard nav moved last. */
    int current_mouse_world_x;
    int current_mouse_world_y;
    int vim_cursor_x;
    int vim_cursor_y;

    int command_mode;
    char command_buffer[256];
    int command_len;
    char mode;              /* status-bar badge: 'n' normal, 'p' pan, 'c' command */
    bool space_held;
    bool left_click_held;   /* drives "is a draw/pan gesture active" */
    bool stroke_open;       /* a draw/erase gesture is mid-flight; its undo snapshot is already taken */
    bool help_visible;
    bool about_visible;
    bool history_visible;   /* undo/redo history viewer (Up/Down walk it live) */
} Viewport;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} TartRect;

typedef struct {
    /* Document: frame_count frames of layer_count layers each (uniform -
     * adding/deleting a layer touches every frame), every canvas exactly
     * width x height. All editing applies to cur_frame/cur_layer; rendering
     * composites cur_frame's whole layer stack. */
    int width, height;
    Frame frames[MAX_FRAMES];
    int frame_count;
    int layer_count;
    int cur_frame;
    int cur_layer;

    /* Playback + animation editing aids. */
    int fps;            /* playback speed, persisted in the save file */
    bool playing;
    bool onion;         /* ghost the previous frame under the current one */
    long play_last_ms;  /* CLOCK_MONOTONIC ms of the last playback advance */

    TartUndoState undo_stack[UNDO_STACK_MAX];
    TartUndoState redo_stack[UNDO_STACK_MAX];
    int undo_count;
    int redo_count;

    Viewport viewport;
    int running;
} App;

void init_app(App *app);
void cleanup_app(App *app);
bool init_canvas(Canvas *c, int w, int h);
void cleanup_canvas(Canvas *c);
Canvas *tart_canvas(App *app);
wchar_t tart_composite(App *app, int frame, int x, int y, unsigned char *out_color);
bool tart_frame_add(App *app, bool dup);
void tart_frame_del(App *app);
bool tart_layer_add(App *app);
void tart_layer_del(App *app);
void tart_undo_push(App *app, const char *desc);
void tart_undo(App *app);
void tart_redo(App *app);
void tart_undo_clear(App *app);
typedef void (*TartPlotFn)(int wx, int wy, void *ctx);
void tart_shape_points(TartTool tool, int x0, int y0, int x1, int y1, int cx, int cy, TartPlotFn plot, void *ctx);
void tart_draw_history_overlay(App *app);
void tart_flood_fill(App *app, int x, int y);
void render(App *app);
void handle_input(App *app);
void tart_compute_layout(App *app, TartRect *menu, TartRect *ruler, TartRect *tool, TartRect *y_ruler, TartRect *canvas, TartRect *anim, TartRect *status);
int tart_build_anim_bar(App *app, TartAnimItem items[TART_ANIM_MAX_ITEMS]);
void tart_draw_anim_bar(App *app, TartRect anim_r);
void tart_draw_at(App *app, int world_x, int world_y, wchar_t ch);
void tart_stamp(App *app, int cx, int cy, int size, BrushShape shape, wchar_t ch, unsigned char color);
bool tart_brush_outline(int dx, int dy, int size, BrushShape shape);
bool tart_rect_outline(int x, int y, int rx, int ry, int rect_w, int rect_h);
wchar_t tart_read_cell(App *app, int x, int y);
void tart_set_color(App *app, int x, int y, unsigned char color);
unsigned char tart_read_color(App *app, int x, int y);
void tart_hsl_to_rgb(int h, int s, int l, int *r, int *g, int *b);
int tart_rgb_to_256(int r, int g, int b);
void tart_draw_color_picker(App *app);
void tart_compute_color_picker_geom(int *out_x, int *out_y, int *out_w, int *out_h, int *out_bar_x, int *out_bar_w);
int tart_build_tool_panel(App *app, TartPanelRow rows[TOOL_PANEL_MAX_ROWS]);
int tart_build_menu_bar(App *app, TartMenuBarItem items[TART_MENU_COUNT]);
void tart_draw_tool_panel(App *app, TartRect tool_r);
void tart_draw_menu_dropdown(App *app, TartRect menu_r);
void tart_draw_command_palette(App *app, TartRect status);
void tart_draw_help_overlay(App *app);
void tart_draw_about_overlay(App *app);
void tart_draw_prompt(App *app, TartRect status_r);
void tart_compute_palette_geom(int *out_x, int *out_y, int *out_w, int *out_h, int *out_max_chars);
void tart_draw_palette(App *app);
bool tart_resize_doc(App *app, int new_w, int new_h);
bool tart_save_doc(App *app, const char *path);
bool tart_load_doc(App *app, const char *path);
void tart_draw_box(int x, int y, int w, int h);
int tart_command_matches(const char *query, int match_idx[TART_COMMAND_COUNT]);
void tart_command_verb(const char *buf, char *out, size_t out_sz);

#endif

#ifdef TART_IMPLEMENTATION

bool init_canvas(Canvas *c, int w, int h) {
    c->width = w;
    c->height = h;
    c->cells = malloc(sizeof(wchar_t) * (size_t)w * h);
    c->colors = malloc((size_t)w * h);
    if (!c->cells || !c->colors) { cleanup_canvas(c); return false; }
    wmemset(c->cells, L' ', (size_t)w * h);
    memset(c->colors, 0, (size_t)w * h);
    return true;
}

void cleanup_canvas(Canvas *c) {
    if (c->cells) { free(c->cells); c->cells = NULL; }
    if (c->colors) { free(c->colors); c->colors = NULL; }
}

/* The canvas all editing tools act on: the active layer of the active frame. */
Canvas *tart_canvas(App *app) {
    return &app->frames[app->cur_frame].layers[app->cur_layer];
}

/* Composited view of one frame's layer stack at (x,y): topmost non-space
 * glyph wins, space is transparent. Caller guarantees bounds. */
wchar_t tart_composite(App *app, int frame, int x, int y, unsigned char *out_color) {
    for (int l = app->layer_count - 1; l >= 0; l--) {
        Canvas *c = &app->frames[frame].layers[l];
        wchar_t ch = c->cells[y * c->width + x];
        if (ch != L' ') {
            *out_color = c->colors[y * c->width + x];
            return ch;
        }
    }
    *out_color = 0;
    return L' ';
}

/* Insert a frame right after the current one and switch to it; dup copies
 * the current frame's layers (the usual animation step), otherwise the new
 * frame is blank. */
bool tart_frame_add(App *app, bool dup) {
    if (app->frame_count >= MAX_FRAMES) return false;
    Frame nf;
    memset(&nf, 0, sizeof(nf));
    for (int l = 0; l < app->layer_count; l++) {
        if (!init_canvas(&nf.layers[l], app->width, app->height)) {
            for (int i = 0; i < l; i++) cleanup_canvas(&nf.layers[i]);
            return false;
        }
        if (dup) {
            Canvas *src = &app->frames[app->cur_frame].layers[l];
            wmemcpy(nf.layers[l].cells, src->cells, (size_t)app->width * app->height);
            memcpy(nf.layers[l].colors, src->colors, (size_t)app->width * app->height);
        }
    }
    int at = app->cur_frame + 1;
    memmove(&app->frames[at + 1], &app->frames[at], (size_t)(app->frame_count - at) * sizeof(Frame));
    app->frames[at] = nf;
    app->frame_count++;
    app->cur_frame = at;
    tart_undo_clear(app);
    return true;
}

void tart_frame_del(App *app) {
    if (app->frame_count <= 1) return;
    for (int l = 0; l < app->layer_count; l++) cleanup_canvas(&app->frames[app->cur_frame].layers[l]);
    memmove(&app->frames[app->cur_frame], &app->frames[app->cur_frame + 1],
            (size_t)(app->frame_count - app->cur_frame - 1) * sizeof(Frame));
    app->frame_count--;
    if (app->cur_frame >= app->frame_count) app->cur_frame = app->frame_count - 1;
    tart_undo_clear(app);
}

/* Layer count is uniform across frames, so add/del touch every frame. The
 * new layer goes directly above the current one and becomes active. Blanks
 * are staged before any frame is touched so a failed alloc can't leave the
 * document ragged. */
bool tart_layer_add(App *app) {
    if (app->layer_count >= MAX_LAYERS) return false;
    static Canvas stage[MAX_FRAMES];
    for (int f = 0; f < app->frame_count; f++) {
        if (!init_canvas(&stage[f], app->width, app->height)) {
            for (int i = 0; i < f; i++) cleanup_canvas(&stage[i]);
            return false;
        }
    }
    int at = app->cur_layer + 1;
    for (int f = 0; f < app->frame_count; f++) {
        Frame *fr = &app->frames[f];
        memmove(&fr->layers[at + 1], &fr->layers[at], (size_t)(app->layer_count - at) * sizeof(Canvas));
        fr->layers[at] = stage[f];
    }
    app->layer_count++;
    app->cur_layer = at;
    tart_undo_clear(app);
    return true;
}

void tart_layer_del(App *app) {
    if (app->layer_count <= 1) return;
    for (int f = 0; f < app->frame_count; f++) {
        Frame *fr = &app->frames[f];
        cleanup_canvas(&fr->layers[app->cur_layer]);
        memmove(&fr->layers[app->cur_layer], &fr->layers[app->cur_layer + 1],
                (size_t)(app->layer_count - app->cur_layer - 1) * sizeof(Canvas));
    }
    app->layer_count--;
    if (app->cur_layer >= app->layer_count) app->cur_layer = app->layer_count - 1;
    tart_undo_clear(app);
}

static void tart_undo_free_state(TartUndoState *u) {
    free(u->cells);
    free(u->colors);
    u->cells = NULL;
    u->colors = NULL;
}

static bool tart_undo_snapshot(App *app, int frame, int layer, TartUndoState *out) {
    Canvas *c = &app->frames[frame].layers[layer];
    size_t n = (size_t)c->width * c->height;
    out->frame = frame;
    out->layer = layer;
    out->width = c->width;
    out->height = c->height;
    out->cells = malloc(sizeof(wchar_t) * n);
    out->colors = malloc(n);
    if (!out->cells || !out->colors) { tart_undo_free_state(out); return false; }
    wmemcpy(out->cells, c->cells, n);
    memcpy(out->colors, c->colors, n);
    return true;
}

/* Push onto a bounded stack; when full the oldest entry falls off. */
static void tart_undo_stack_push(TartUndoState *stack, int *count, TartUndoState s) {
    if (*count == UNDO_STACK_MAX) {
        tart_undo_free_state(&stack[0]);
        memmove(&stack[0], &stack[1], (UNDO_STACK_MAX - 1) * sizeof(TartUndoState));
        (*count)--;
    }
    stack[(*count)++] = s;
}

/* Call just before an edit gesture modifies the active canvas: one call =
 * one undoable step, labeled with desc (a static string) for the history
 * viewer. A fresh edit invalidates the redo stack. */
void tart_undo_push(App *app, const char *desc) {
    TartUndoState s;
    if (!tart_undo_snapshot(app, app->cur_frame, app->cur_layer, &s)) return;
    s.desc = desc;
    tart_undo_stack_push(app->undo_stack, &app->undo_count, s);
    while (app->redo_count > 0) tart_undo_free_state(&app->redo_stack[--app->redo_count]);
}

/* Undo and redo are the same swap in opposite directions: pop a snapshot,
 * save the canvas's present state onto the other stack, restore, and jump
 * the editor to that frame/layer so the change is visible. */
static void tart_undo_apply(App *app, TartUndoState *from, int *from_count,
                            TartUndoState *to, int *to_count) {
    if (*from_count == 0) return;
    TartUndoState s = from[*from_count - 1];
    /* Structure ops clear the stacks, so entries always reference a live
     * same-sized canvas; guard anyway. */
    if (s.frame >= app->frame_count || s.layer >= app->layer_count ||
        s.width != app->width || s.height != app->height) {
        tart_undo_free_state(&s);
        (*from_count)--;
        return;
    }
    TartUndoState cur;
    if (!tart_undo_snapshot(app, s.frame, s.layer, &cur)) return;
    cur.desc = s.desc;   /* the label stays with its slot in the timeline */
    tart_undo_stack_push(to, to_count, cur);

    Canvas *c = &app->frames[s.frame].layers[s.layer];
    wmemcpy(c->cells, s.cells, (size_t)s.width * s.height);
    memcpy(c->colors, s.colors, (size_t)s.width * s.height);
    app->cur_frame = s.frame;
    app->cur_layer = s.layer;
    tart_undo_free_state(&s);
    (*from_count)--;
}

void tart_undo(App *app) {
    tart_undo_apply(app, app->undo_stack, &app->undo_count, app->redo_stack, &app->redo_count);
}

void tart_redo(App *app) {
    tart_undo_apply(app, app->redo_stack, &app->redo_count, app->undo_stack, &app->undo_count);
}

void tart_undo_clear(App *app) {
    while (app->undo_count > 0) tart_undo_free_state(&app->undo_stack[--app->undo_count]);
    while (app->redo_count > 0) tart_undo_free_state(&app->redo_stack[--app->redo_count]);
}

void init_app(App *app) {
    setlocale(LC_ALL, "");
    srand((unsigned)time(NULL));    /* random-texture fill */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(50);        /* getch blocks at most 50ms, so the UI stays live */
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    printf("\033[?1003h\033[?1006h");   // any-motion tracking (1003) + SGR extended encoding (1006)
    fflush(stdout);
    mouseinterval(0);
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    /* Dark-slate menu chrome (pair 5) and its hover/open highlight (pair 6),
     * styled after a dark code editor's menu bar instead of a reversed-white
     * classic-desktop look. */
    init_pair(5, 253, 236);
    init_pair(6, 255, 24);
    /* Canvas cell colors use a dedicated 256-color palette, offset by 16 to
     * stay clear of the chrome pairs above (COLORS/pairs here are 256/65536
     * per terminfo, so there's ample room). Index 0 in Canvas.colors means
     * "no color / default" and is never looked up as a pair. */
    for (int i = 0; i < 256; i++) {
        init_pair(16 + i, i, COLOR_BLACK);
    }

    app->width = TART_DEFAULT_CANVAS_WIDTH;
    app->height = TART_DEFAULT_CANVAS_HEIGHT;
    app->frame_count = 1;
    app->layer_count = 1;
    app->cur_frame = 0;
    app->cur_layer = 0;
    app->fps = 8;
    app->playing = false;
    app->onion = false;
    app->undo_count = 0;
    app->redo_count = 0;
    init_canvas(&app->frames[0].layers[0], app->width, app->height);

    /* Zero is the correct default for nearly every Viewport field (TOOL_PENCIL,
     * BRUSH_SQUARE, PROMPT_NONE, ... are all 0), so clear the struct and list
     * only the exceptions. */
    Viewport *vp = &app->viewport;
    memset(vp, 0, sizeof(*vp));
    vp->offset_x = -2;      /* start panned slightly out so the canvas edge is visible */
    vp->offset_y = -2;
    vp->brush_char = L'#';
    vp->brush_size = 1;
    vp->eraser_size = 1;
    vp->fill_char = L'#';
    vp->hsl_s = 100;
    vp->hsl_l = 50;
    vp->open_menu = -1;
    vp->mode = 'n';
    app->running = 1;
}

void cleanup_app(App *app) {
    printf("\033[?1006l\033[?1003l");   // disable SGR encoding + any-motion tracking
    fflush(stdout);
    free(app->viewport.sel_buffer);
    app->viewport.sel_buffer = NULL;
    free(app->viewport.sel_colors);
    app->viewport.sel_colors = NULL;
    tart_undo_clear(app);
    for (int f = 0; f < app->frame_count; f++)
        for (int l = 0; l < app->layer_count; l++)
            cleanup_canvas(&app->frames[f].layers[l]);
    endwin();
}

void tart_compute_layout(App *app, TartRect *menu, TartRect *ruler, TartRect *tool, TartRect *y_ruler, TartRect *canvas, TartRect *anim, TartRect *status) {
    int lines = LINES;
    int cols = COLS;
    *menu    = TART_RECT(0, 0, cols, 1);
    *ruler   = TART_RECT(0, 1, cols, 2);
    *tool    = TART_RECT(0, 3, TOOL_PANEL_WIDTH, CANVAS_VIEW_H);
    *y_ruler = TART_RECT(TOOL_PANEL_WIDTH, 3, Y_RULER_WIDTH, CANVAS_VIEW_H);
    *canvas  = TART_RECT(CANVAS_START_X, 3, CANVAS_VIEW_W, CANVAS_VIEW_H);
    *anim    = TART_RECT(0, lines - 2, cols, 1);
    *status  = TART_RECT(0, lines - 1, cols, 1);
}

void tart_draw_at(App *app, int world_x, int world_y, wchar_t ch) {
    Canvas *c = tart_canvas(app);
    if (world_x >= 0 && world_x < c->width && world_y >= 0 && world_y < c->height) {
        c->cells[world_y * c->width + world_x] = ch;
    }
}

wchar_t tart_read_cell(App *app, int x, int y) {
    Canvas *c = tart_canvas(app);
    if (x >= 0 && x < c->width && y >= 0 && y < c->height) {
        return c->cells[y * c->width + x];
    }
    return L' ';
}

void tart_set_color(App *app, int x, int y, unsigned char color) {
    Canvas *c = tart_canvas(app);
    if (x >= 0 && x < c->width && y >= 0 && y < c->height) {
        c->colors[y * c->width + x] = color;
    }
}

unsigned char tart_read_color(App *app, int x, int y) {
    Canvas *c = tart_canvas(app);
    if (x >= 0 && x < c->width && y >= 0 && y < c->height) {
        return c->colors[y * c->width + x];
    }
    return 0;
}

void tart_stamp(App *app, int cx, int cy, int size, BrushShape shape, wchar_t ch, unsigned char color) {
    int r = (size - 1) / 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (shape == BRUSH_ROUND && (dx * dx + dy * dy) > r * r) continue;
            tart_draw_at(app, cx + dx, cy + dy, ch);
            tart_set_color(app, cx + dx, cy + dy, color);
        }
    }
}

/* True if (dx,dy) - an offset from the brush center - sits on the outline
 * (not interior) of the size/shape footprint tart_stamp would paint, so the
 * cursor preview can trace just the perimeter rather than filling it. */
bool tart_brush_outline(int dx, int dy, int size, BrushShape shape) {
    int r = (size - 1) / 2;
    if (dx < -r || dx > r || dy < -r || dy > r) return false;
    if (shape == BRUSH_SQUARE) {
        return (dx == -r || dx == r || dy == -r || dy == r);
    }
    int r2 = r * r;
    if (dx * dx + dy * dy > r2) return false;
    return (dx + 1) * (dx + 1) + dy * dy > r2 || (dx - 1) * (dx - 1) + dy * dy > r2 ||
           dx * dx + (dy + 1) * (dy + 1) > r2 || dx * dx + (dy - 1) * (dy - 1) > r2;
}

/* True if world (x,y) sits on the perimeter of the rect_w x rect_h box whose
 * top-left is (rx,ry) - used to keep the current selection outlined on the
 * canvas at all times, not just while it's being dragged out. */
bool tart_rect_outline(int x, int y, int rx, int ry, int rect_w, int rect_h) {
    if (x < rx || x >= rx + rect_w || y < ry || y >= ry + rect_h) return false;
    return (x == rx || x == rx + rect_w - 1 || y == ry || y == ry + rect_h - 1);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void tart_plot_seg(int x0, int y0, int x1, int y1, TartPlotFn plot, void *ctx) {
    /* Bresenham, the integer-error form that handles all octants. */
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        plot(x0, y0, ctx);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Emits every cell of a shape through the plot callback, so the live drag
 * preview in render() and the commit path in input.c trace the exact same
 * cells (same renderer/hit-test pairing idea as the panel tables).
 * Line uses the raw endpoints; ellipse/triangle treat them as a bounding
 * box; curve is a quadratic Bezier bent toward control point (cx,cy),
 * drawn as short segments between dense samples so it can't leave gaps. */
void tart_shape_points(TartTool tool, int x0, int y0, int x1, int y1, int cx, int cy, TartPlotFn plot, void *ctx) {
    switch (tool) {
        case TOOL_LINE:
            tart_plot_seg(x0, y0, x1, y1, plot, ctx);
            break;
        case TOOL_CURVE: {
            int steps = abs(x1 - x0) + abs(y1 - y0) + abs(2 * cx - x0 - x1) + abs(2 * cy - y0 - y1);
            if (steps < 8) steps = 8;
            int px = x0, py = y0;
            for (int i = 1; i <= steps; i++) {
                double t = (double)i / steps, u = 1.0 - t;
                int qx = (int)lround(u * u * x0 + 2.0 * u * t * cx + t * t * x1);
                int qy = (int)lround(u * u * y0 + 2.0 * u * t * cy + t * t * y1);
                tart_plot_seg(px, py, qx, qy, plot, ctx);
                px = qx; py = qy;
            }
            break;
        }
        case TOOL_ELLIPSE: {
            if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
            if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
            double ecx = (x0 + x1) / 2.0, ecy = (y0 + y1) / 2.0;
            double rx = (x1 - x0) / 2.0, ry = (y1 - y0) / 2.0;
            /* Dense parametric sampling; cheap at canvas scale and immune to
             * the axis special-casing a midpoint algorithm would need. */
            int steps = (int)(8 * (rx + ry)) + 8;
            for (int i = 0; i < steps; i++) {
                double a = 2.0 * M_PI * i / steps;
                plot((int)lround(ecx + rx * cos(a)), (int)lround(ecy + ry * sin(a)), ctx);
            }
            break;
        }
        case TOOL_TRI: {
            if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
            if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
            int mx = (x0 + x1) / 2;   /* apex at top-center, base along the bottom */
            tart_plot_seg(mx, y0, x0, y1, plot, ctx);
            tart_plot_seg(mx, y0, x1, y1, plot, ctx);
            tart_plot_seg(x0, y1, x1, y1, plot, ctx);
            break;
        }
        default:
            break;
    }
}

/* Bucket fill: repaints the 4-connected region of cells matching the start
 * cell's glyph+color with fill_char in current_color. Random mode instead
 * draws each cell from the palette category containing fill_char - a
 * texture fill (pick '.' for scattered punctuation, 'o' for shape glyphs);
 * clicking a random-filled region again re-rolls it. Pushes its own undo
 * entry, and only once it knows it will actually change something. */
void tart_flood_fill(App *app, int x, int y) {
    Canvas *c = tart_canvas(app);
    Viewport *vp = &app->viewport;
    int w = c->width, h = c->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;

    wchar_t from_ch = c->cells[y * w + x];
    unsigned char from_col = c->colors[y * w + x];
    if (!vp->fill_random && from_ch == vp->fill_char && from_col == vp->current_color) return;

    /* Random mode rolls each cell from the palette category containing
     * fill_char; chars in no category (like the default '#') would leave
     * the set empty and quietly degrade to a fixed fill, so fall back to
     * the Standard row - random must always visibly do something. */
    const wchar_t *set = NULL;
    int set_len = 0;
    if (vp->fill_random) {
        for (size_t i = 0; i < TART_PALETTE_CAT_COUNT; i++) {
            if (wcschr(TART_PALETTE[i].chars, vp->fill_char)) {
                set = TART_PALETTE[i].chars;
                break;
            }
        }
        if (!set) set = TART_PALETTE[0].chars;
        set_len = (int)wcslen(set);
    }

    /* Iterative fill with an explicit stack; visited[] (marked at push time)
     * keeps every cell pushed at most once, which both bounds the stack at
     * w*h and makes random re-rolls terminate even when a rolled glyph
     * happens to equal the glyph being replaced. */
    int *stack = malloc(sizeof(int) * (size_t)w * h);
    unsigned char *visited = calloc((size_t)w * h, 1);
    if (!stack || !visited) { free(stack); free(visited); return; }

    tart_undo_push(app, "fill");
    int top = 0;
    visited[y * w + x] = 1;
    stack[top++] = y * w + x;
    while (top > 0) {
        int idx = stack[--top];
        c->cells[idx] = (set_len > 0) ? set[rand() % set_len] : vp->fill_char;
        c->colors[idx] = vp->current_color;
        int cx = idx % w, cy = idx / w;
        int nbr[4] = { idx - 1, idx + 1, idx - w, idx + w };
        bool ok[4] = { cx > 0, cx < w - 1, cy > 0, cy < h - 1 };
        for (int i = 0; i < 4; i++) {
            if (ok[i] && !visited[nbr[i]] &&
                c->cells[nbr[i]] == from_ch && c->colors[nbr[i]] == from_col) {
                visited[nbr[i]] = 1;
                stack[top++] = nbr[i];
            }
        }
    }
    free(stack);
    free(visited);
}

/* Standard HSL->RGB (h in [0,360), s/l in [0,100]), each output channel 0-255. */
void tart_hsl_to_rgb(int h, int s, int l, int *r, int *g, int *b) {
    double H = ((h % 360) + 360) % 360 / 360.0;
    double S = s / 100.0, L = l / 100.0;
    double C = (1.0 - fabs(2.0 * L - 1.0)) * S;
    double Hp = H * 6.0;
    double X = C * (1.0 - fabs(fmod(Hp, 2.0) - 1.0));
    double r1, g1, b1;
    if (Hp < 1)      { r1 = C; g1 = X; b1 = 0; }
    else if (Hp < 2) { r1 = X; g1 = C; b1 = 0; }
    else if (Hp < 3) { r1 = 0; g1 = C; b1 = X; }
    else if (Hp < 4) { r1 = 0; g1 = X; b1 = C; }
    else if (Hp < 5) { r1 = X; g1 = 0; b1 = C; }
    else             { r1 = C; g1 = 0; b1 = X; }
    double m = L - C / 2.0;
    *r = (int)((r1 + m) * 255.0 + 0.5);
    *g = (int)((g1 + m) * 255.0 + 0.5);
    *b = (int)((b1 + m) * 255.0 + 0.5);
}

/* Quantizes to the nearest color in xterm's 216-color cube (indices 16-231),
 * using its actual (non-linear) per-channel level table. Skips the 16 base
 * ANSI colors (theme-dependent, not reliably RGB-mappable) and the 24-step
 * grayscale ramp - the 216-cube alone covers plenty of range for a first
 * pass and keeps quantization simple and predictable. */
int tart_rgb_to_256(int r, int g, int b) {
    static const int levels[6] = { 0, 95, 135, 175, 215, 255 };
    int best_ri = 0, best_gi = 0, best_bi = 0, best_d;

    best_d = 256;
    for (int i = 0; i < 6; i++) { int d = abs(r - levels[i]); if (d < best_d) { best_d = d; best_ri = i; } }
    best_d = 256;
    for (int i = 0; i < 6; i++) { int d = abs(g - levels[i]); if (d < best_d) { best_d = d; best_gi = i; } }
    best_d = 256;
    for (int i = 0; i < 6; i++) { int d = abs(b - levels[i]); if (d < best_d) { best_d = d; best_bi = i; } }

    return 16 + 36 * best_ri + 6 * best_gi + best_bi;
}

/* mvaddch only takes narrow chtypes; this is the one-cell wide-glyph
 * equivalent, used everywhere a box-drawing or canvas character lands on
 * screen. */
static void tart_addwch(int y, int x, wchar_t wc) {
    mvaddnwstr(y, x, ((wchar_t[]){ wc, L'\0' }), 1);
}

/* Draws a solid single-line box outline; caller fills the interior and sets
 * whatever color/attrs it wants around the call. Shared by every popup
 * (help overlay, char palette, color picker) so they can't drift apart. */
void tart_draw_box(int x, int y, int w, int h) {
    tart_addwch(y, x, L'┌');
    tart_addwch(y, x + w - 1, L'┐');
    tart_addwch(y + h - 1, x, L'└');
    tart_addwch(y + h - 1, x + w - 1, L'┘');
    for (int i = 1; i < w - 1; i++) {
        tart_addwch(y, x + i, L'─');
        tart_addwch(y + h - 1, x + i, L'─');
    }
    for (int row = 1; row < h - 1; row++) {
        tart_addwch(y + row, x, L'│');
        tart_addwch(y + row, x + w - 1, L'│');
    }
}

/* Plot callback for shape drag previews: draws the brush char directly on
 * screen (caller sets the dim/cyan attrs), clipped to the viewport. */
struct TartShapePreview { App *app; TartRect canvas_r; };
static void tart_preview_plot(int wx, int wy, void *vctx) {
    struct TartShapePreview *p = vctx;
    int sx = wx - p->app->viewport.offset_x + p->canvas_r.x;
    int sy = wy - p->app->viewport.offset_y + p->canvas_r.y;
    if (sx >= p->canvas_r.x && sx < p->canvas_r.x + p->canvas_r.w &&
        sy >= p->canvas_r.y && sy < p->canvas_r.y + p->canvas_r.h) {
        tart_addwch(sy, sx, p->app->viewport.brush_char);
    }
}

void render(App *app) {
    erase();

    TartRect menu_r, ruler_r, tool_r, y_ruler_r, canvas_r, anim_r, status_r;
    tart_compute_layout(app, &menu_r, &ruler_r, &tool_r, &y_ruler_r, &canvas_r, &anim_r, &status_r);

    attron(COLOR_PAIR(5));
    mvprintw(menu_r.y, menu_r.x, "%-*s", menu_r.w, "");
    attroff(COLOR_PAIR(5));
    {
        TartMenuBarItem mitems[TART_MENU_COUNT];
        tart_build_menu_bar(app, mitems);
        for (size_t i = 0; i < TART_MENU_COUNT; i++) {
            bool open = (app->viewport.open_menu == (int)i);
            attr_t a = open ? (COLOR_PAIR(6) | A_BOLD) : COLOR_PAIR(5);
            attron(a);
            mvprintw(menu_r.y, mitems[i].x0, "%s", mitems[i].label);
            attroff(a);
            /* Underline the Alt+<letter> mnemonic (always the first letter). */
            attron(a | A_UNDERLINE);
            mvprintw(menu_r.y, mitems[i].x0, "%c", mitems[i].label[0]);
            attroff(a | A_UNDERLINE);
        }
    }

    /* X-ruler numbers every 10 world columns; their tick marks live on the
     * canvas's top border below (the ┬ junctions), which occupies what would
     * otherwise be the ruler's tick row. */
    attron(COLOR_PAIR(4) | A_DIM);
    for (int wx = ((app->viewport.offset_x / 10) * 10) - 30; wx < app->viewport.offset_x + canvas_r.w + 30; wx += 10) {
        int screen_x = (wx - app->viewport.offset_x) + CANVAS_START_X;
        if (screen_x < CANVAS_START_X) continue;
        if (screen_x < ruler_r.w) {
            mvprintw(ruler_r.y, screen_x, "%d", wx);
        }
    }
    attroff(COLOR_PAIR(4) | A_DIM);

    tart_draw_tool_panel(app, tool_r);

    attron(COLOR_PAIR(4) | A_DIM);
    for (int y = 0; y < y_ruler_r.h; y++) {
        int doc_y = y + app->viewport.offset_y;
        mvprintw(y_ruler_r.y + y, y_ruler_r.x, "%3d ", doc_y);
    }
    attroff(COLOR_PAIR(4) | A_DIM);

    /* Solid viewport frame. Top/bottom edges double as the ruler's tick row:
     * a ┬/┴ junction every 10 world columns, aligned under the numbers above. */
    attron(COLOR_PAIR(4));
    for (int x = 0; x < canvas_r.w; x++) {
        int world_x = x + app->viewport.offset_x;
        bool tick = (((world_x % 10) + 10) % 10) == 0;
        tart_addwch(canvas_r.y - 1, canvas_r.x + x, tick ? L'┬' : L'─');
        tart_addwch(canvas_r.y + canvas_r.h, canvas_r.x + x, tick ? L'┴' : L'─');
    }
    for (int y = 0; y < canvas_r.h; y++) {
        tart_addwch(canvas_r.y + y, canvas_r.x - 1, L'│');
        tart_addwch(canvas_r.y + y, canvas_r.x + canvas_r.w, L'│');
    }
    tart_addwch(canvas_r.y - 1, canvas_r.x - 1, L'┌');
    tart_addwch(canvas_r.y - 1, canvas_r.x + canvas_r.w, L'┐');
    tart_addwch(canvas_r.y + canvas_r.h, canvas_r.x - 1, L'└');
    tart_addwch(canvas_r.y + canvas_r.h, canvas_r.x + canvas_r.w, L'┘');
    attroff(COLOR_PAIR(4));

    /* Pencil/eraser get a live outline of their actual size+shape footprint
     * around the cursor; other tools have their own drag preview below. */
    bool show_brush_outline = (app->viewport.tool == TOOL_PENCIL || app->viewport.tool == TOOL_ERASER);
    int brush_sz = (app->viewport.tool == TOOL_ERASER) ? app->viewport.eraser_size : app->viewport.brush_size;
    BrushShape brush_shp = (app->viewport.tool == TOOL_ERASER) ? app->viewport.eraser_shape : app->viewport.brush_shape;
    /* Tools that actually paint with current_color; eraser always clears to
     * color 0 regardless, so previewing current_color there would mislead. */
    bool tool_uses_color = (app->viewport.tool != TOOL_ERASER &&
                            app->viewport.tool != TOOL_SELECT &&
                            app->viewport.tool != TOOL_MOVE);

    for (int dy = 0; dy < canvas_r.h; dy++) {
        for (int dx = 0; dx < canvas_r.w; dx++) {
            int world_x = dx + app->viewport.offset_x;
            int world_y = dy + app->viewport.offset_y;

            wchar_t ch_to_draw;
            attr_t attrs = A_NORMAL;
            int color_pair = 0;
            bool is_cursor = (world_x == app->viewport.current_mouse_world_x && world_y == app->viewport.current_mouse_world_y);

            if (world_x >= 0 && world_x < app->width && world_y >= 0 && world_y < app->height) {
                unsigned char cell_color;
                ch_to_draw = tart_composite(app, app->cur_frame, world_x, world_y, &cell_color);
                if (cell_color != 0) color_pair = 16 + cell_color;
                /* Onion skin: where this frame is blank, ghost the previous
                 * frame's composite in dim monochrome. */
                if (ch_to_draw == L' ' && app->onion && !app->playing && app->frame_count > 1) {
                    int prev = (app->cur_frame + app->frame_count - 1) % app->frame_count;
                    unsigned char ghost_color;
                    wchar_t ghost = tart_composite(app, prev, world_x, world_y, &ghost_color);
                    (void)ghost_color;
                    if (ghost != L' ') {
                        ch_to_draw = ghost;
                        attrs |= A_DIM;
                    }
                }
            } else {
                attrs = A_DIM;
                ch_to_draw = (world_x % 10 == 0 && world_y % 5 == 0) ? L'+' : L'.';
            }

            if (show_brush_outline &&
                tart_brush_outline(world_x - app->viewport.current_mouse_world_x,
                                    world_y - app->viewport.current_mouse_world_y,
                                    brush_sz, brush_shp)) {
                attrs = (attrs & ~A_DIM) | A_BOLD;
                color_pair = 1;
                /* A blank cell has no ink to tint, so the outline would be
                 * invisible over untouched canvas - give it a visible dot
                 * there instead of silently doing nothing. */
                if (ch_to_draw == L' ') ch_to_draw = L'.';
            }

            /* Selection stays outlined on the canvas as long as it's held,
             * regardless of which tool is active now - otherwise it becomes
             * invisible the instant you release the Select drag. */
            if (app->viewport.has_selection &&
                tart_rect_outline(world_x, world_y, app->viewport.sel_origin_wx, app->viewport.sel_origin_wy,
                                   app->viewport.sel_w, app->viewport.sel_h)) {
                attrs = (attrs & ~A_DIM) | A_BOLD;
                color_pair = 4;
                if (ch_to_draw == L' ') ch_to_draw = L'.';
            }

            /* The exact cell under the cursor previews what a click would
             * actually do: reversed as before, but also tinted to the color
             * that would be painted, taking priority over the outline's
             * generic cyan hint on the degenerate size-1 case where they
             * coincide. */
            if (is_cursor) {
                attrs |= A_REVERSE;
                if (tool_uses_color && app->viewport.current_color != 0) {
                    color_pair = 16 + app->viewport.current_color;
                }
            }

            if (color_pair != 0) attrs |= COLOR_PAIR(color_pair);
            attron(attrs);
            tart_addwch(canvas_r.y + dy, canvas_r.x + dx, ch_to_draw);
            attroff(attrs);
        }
    }

    /* Shape tools preview through the same tart_shape_points that will
     * commit them, so the preview can never lie about the final cells.
     * The curve previews in both stages: a straight baseline while its
     * endpoints are dragged out, then the live bend while the control
     * point follows the cursor. */
    {
        Viewport *vp = &app->viewport;
        bool shape_drag = vp->tool_dragging &&
            (vp->tool == TOOL_LINE || vp->tool == TOOL_ELLIPSE || vp->tool == TOOL_TRI || vp->tool == TOOL_CURVE);
        bool curve_bend = (vp->tool == TOOL_CURVE && vp->curve_pending);
        if (shape_drag || curve_bend) {
            struct TartShapePreview pctx = { app, canvas_r };
            attron(A_DIM | COLOR_PAIR(1));
            if (curve_bend) {
                tart_shape_points(TOOL_CURVE, vp->drag_start_wx, vp->drag_start_wy,
                                  vp->drag_cur_wx, vp->drag_cur_wy,
                                  vp->curve_cx, vp->curve_cy, tart_preview_plot, &pctx);
            } else {
                TartTool t = (vp->tool == TOOL_CURVE) ? TOOL_LINE : vp->tool;
                tart_shape_points(t, vp->drag_start_wx, vp->drag_start_wy,
                                  vp->drag_cur_wx, vp->drag_cur_wy, 0, 0, tart_preview_plot, &pctx);
            }
            attroff(A_DIM | COLOR_PAIR(1));
        }
    }

    if (app->viewport.tool_dragging) {
        int x0 = app->viewport.drag_start_wx, x1 = app->viewport.drag_cur_wx;
        int y0 = app->viewport.drag_start_wy, y1 = app->viewport.drag_cur_wy;
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }

        if (app->viewport.tool == TOOL_RECT) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    bool border = (x == x0 || x == x1 || y == y0 || y == y1);
                    if (!border && !app->viewport.rect_filled) continue;
                    int sx = x - app->viewport.offset_x + canvas_r.x;
                    int sy = y - app->viewport.offset_y + canvas_r.y;
                    if (sx >= canvas_r.x && sx < canvas_r.x + canvas_r.w && sy >= canvas_r.y && sy < canvas_r.y + canvas_r.h) {
                        wchar_t preview_ch = border ? app->viewport.brush_char : app->viewport.fill_char;
                        attron(A_DIM | COLOR_PAIR(1));
                        tart_addwch(sy, sx, preview_ch);
                        attroff(A_DIM | COLOR_PAIR(1));
                    }
                }
            }
        } else if (app->viewport.tool == TOOL_SELECT) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    bool border = (x == x0 || x == x1 || y == y0 || y == y1);
                    if (!border) continue;
                    int sx = x - app->viewport.offset_x + canvas_r.x;
                    int sy = y - app->viewport.offset_y + canvas_r.y;
                    if (sx >= canvas_r.x && sx < canvas_r.x + canvas_r.w && sy >= canvas_r.y && sy < canvas_r.y + canvas_r.h) {
                        attron(A_REVERSE);
                        tart_addwch(sy, sx, tart_read_cell(app, x, y));
                        attroff(A_REVERSE);
                    }
                }
            }
        } else if (app->viewport.tool == TOOL_MOVE && app->viewport.has_selection) {
            for (int y = 0; y < app->viewport.sel_h; y++) {
                for (int x = 0; x < app->viewport.sel_w; x++) {
                    int wx = app->viewport.drag_cur_wx + x;
                    int wy = app->viewport.drag_cur_wy + y;
                    int sx = wx - app->viewport.offset_x + canvas_r.x;
                    int sy = wy - app->viewport.offset_y + canvas_r.y;
                    if (sx >= canvas_r.x && sx < canvas_r.x + canvas_r.w && sy >= canvas_r.y && sy < canvas_r.y + canvas_r.h) {
                        attron(A_REVERSE | A_DIM);
                        tart_addwch(sy, sx, app->viewport.sel_buffer[y * app->viewport.sel_w + x]);
                        attroff(A_REVERSE | A_DIM);
                    }
                }
            }
        }
    }

    tart_draw_anim_bar(app, anim_r);

    if (app->viewport.command_mode) {
        tart_draw_command_palette(app, status_r);
    } else if (app->viewport.prompt_mode != PROMPT_NONE) {
        tart_draw_prompt(app, status_r);
    } else {
        /* Same dark chrome as the command-input line (tart_draw_command_palette),
         * so the bottom row reads as one persistent command/status area
         * whether or not you're actively typing a command. */
        const char *mode_badge = (app->viewport.mode == 'n') ? " NORMAL " : " PAN ";
        int cx = status_r.x;

        attron(COLOR_PAIR(5));
        mvprintw(status_r.y, status_r.x, "%*s", status_r.w, "");
        attroff(COLOR_PAIR(5));

        attron(COLOR_PAIR(6) | A_BOLD);
        mvprintw(status_r.y, cx, "%s", mode_badge);
        attroff(COLOR_PAIR(6) | A_BOLD);
        cx += (int)strlen(mode_badge) + 1;

        {
            const char *tool_name = (app->viewport.tool == TOOL_PENCIL) ? "draw" :
                                     (app->viewport.tool == TOOL_ERASER) ? "eraser" :
                                     (app->viewport.tool == TOOL_RECT)   ? "rect"   :
                                     (app->viewport.tool == TOOL_SELECT) ? "select" :
                                     (app->viewport.tool == TOOL_MOVE)   ? "move"   :
                                     (app->viewport.tool == TOOL_LINE)   ? "line"   :
                                     (app->viewport.tool == TOOL_CURVE)  ? "curve"  :
                                     (app->viewport.tool == TOOL_ELLIPSE) ? "ellipse" :
                                     (app->viewport.tool == TOOL_TRI)    ? "triangle" : "fill";
            attron(COLOR_PAIR(5) | A_BOLD);
            if (app->viewport.tool == TOOL_PENCIL) {
                mvprintw(status_r.y, cx, "tool:%s '%lc'", tool_name, (wint_t)app->viewport.brush_char);
                cx += (int)strlen("tool:") + (int)strlen(tool_name) + 5;
            } else {
                mvprintw(status_r.y, cx, "tool:%s", tool_name);
                cx += (int)strlen("tool:") + (int)strlen(tool_name) + 1;
            }
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        attron(COLOR_PAIR(5));
        mvprintw(status_r.y, cx, "offset[%4d,%4d]", app->viewport.offset_x, app->viewport.offset_y);
        attroff(COLOR_PAIR(5));
        cx += 18;

        attron(COLOR_PAIR(5));
        mvprintw(status_r.y, cx, "cursor[%4d,%4d]", app->viewport.vim_cursor_x, app->viewport.vim_cursor_y);
        attroff(COLOR_PAIR(5));
        cx += 18;

        {
            char fl[32];
            int len = snprintf(fl, sizeof(fl), "frm %d/%d lyr %d/%d",
                               app->cur_frame + 1, app->frame_count,
                               app->cur_layer + 1, app->layer_count);
            attron(COLOR_PAIR(5));
            mvprintw(status_r.y, cx, "%s", fl);
            attroff(COLOR_PAIR(5));
            cx += len + 1;
        }

        const char *hint = ": commands   ?/F1 help   q quit";
        int hint_x = status_r.x + status_r.w - (int)strlen(hint) - 1;
        if (hint_x > cx) {
            attron(COLOR_PAIR(5));
            mvprintw(status_r.y, hint_x, "%s", hint);
            attroff(COLOR_PAIR(5));
        }
    }

    if (app->viewport.open_menu >= 0) {
        tart_draw_menu_dropdown(app, menu_r);
    }

    if (app->viewport.palette_visible) {
        tart_draw_palette(app);
    }

    if (app->viewport.color_picker_visible) {
        tart_draw_color_picker(app);
    }

    if (app->viewport.help_visible) {
        tart_draw_help_overlay(app);
    }

    if (app->viewport.about_visible) {
        tart_draw_about_overlay(app);
    }

    if (app->viewport.history_visible) {
        tart_draw_history_overlay(app);
    }

    refresh();
}

/* Undo/redo history viewer: the timeline drawn oldest-first, a "now" marker
 * between the undo (past) and redo (future) stacks. It's live - Up/Down in
 * input.c actually run undo/redo while it's open, moving the marker. */
void tart_draw_history_overlay(App *app) {
    enum { SHOW_MAX = 10 };   /* per side; older/further entries summarized */
    int u_show = app->undo_count > SHOW_MAX ? SHOW_MAX : app->undo_count;
    int r_show = app->redo_count > SHOW_MAX ? SHOW_MAX : app->redo_count;
    int u_hidden = app->undo_count - u_show;
    int r_hidden = app->redo_count - r_show;

    int rows = u_show + r_show + 1 + (u_hidden > 0) + (r_hidden > 0);
    if (app->undo_count + app->redo_count == 0) rows = 2;   /* "(no edits)" + marker */
    int w = 34;
    int h = rows + 4;   /* borders, title, hint */
    int x = (COLS - w) / 2;
    int y = (LINES - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    attron(COLOR_PAIR(2));
    for (int row = 0; row < h; row++) mvprintw(y + row, x, "%*s", w, "");
    tart_draw_box(x, y, w, h);
    attron(A_BOLD);
    mvprintw(y + 1, x + 2, "History  (Up undo / Down redo)");
    attroff(A_BOLD);

    int line = y + 2;
    if (app->undo_count + app->redo_count == 0) {
        attron(A_DIM);
        mvprintw(line++, x + 2, "(no edits yet)");
        attroff(A_DIM);
    }
    if (u_hidden > 0) {
        attron(A_DIM);
        mvprintw(line++, x + 2, "... %d older", u_hidden);
        attroff(A_DIM);
    }
    for (int i = app->undo_count - u_show; i < app->undo_count; i++) {
        TartUndoState *s = &app->undo_stack[i];
        mvprintw(line++, x + 2, "%-10s frame %d layer %d", s->desc, s->frame + 1, s->layer + 1);
    }
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(line++, x + 2, "--- now ---");
    attroff(A_BOLD | COLOR_PAIR(1));
    /* Redo entries are the future: nearest (next redo) first, dimmed. */
    attron(A_DIM);
    for (int i = app->redo_count - 1; i >= app->redo_count - r_show; i--) {
        TartUndoState *s = &app->redo_stack[i];
        mvprintw(line++, x + 2, "%-10s frame %d layer %d", s->desc, s->frame + 1, s->layer + 1);
    }
    if (r_hidden > 0) mvprintw(line++, x + 2, "... %d further", r_hidden);
    attroff(A_DIM);

    mvprintw(y + h - 2, x + 2, "any other key closes");
    attroff(COLOR_PAIR(2));
}

static const TartCommand *tart_find_command(const char *name) {
    for (size_t i = 0; i < TART_COMMAND_COUNT; i++) {
        if (strcmp(TART_COMMANDS[i].name, name) == 0) return &TART_COMMANDS[i];
    }
    return NULL;
}

/* True if every character of query appears in text in order (case-
 * insensitive), e.g. "rsz" matches "resize". Lower score = tighter match:
 * each character skipped between two matched letters costs one point, so
 * contiguous/prefix matches naturally sort above scattered ones. */
static bool tart_fuzzy_score(const char *text, const char *query, int *out_score) {
    if (!*query) { *out_score = 0; return true; }
    int score = 0, ti = 0, last_match = -1;
    for (int qi = 0; query[qi]; qi++) {
        int qc = tolower((unsigned char)query[qi]);
        bool found = false;
        for (; text[ti]; ti++) {
            if (tolower((unsigned char)text[ti]) == qc) {
                if (last_match >= 0) score += ti - last_match - 1;
                last_match = ti;
                ti++;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    *out_score = score;
    return true;
}

/* Fuzzy-filters TART_COMMANDS against query (matching name or alias),
 * writing matches into match_idx best-first. Shared by the command-palette
 * renderer and its Up/Down/Enter handling in input.c so they always agree
 * on what "the Nth match" means. */
int tart_command_matches(const char *query, int match_idx[TART_COMMAND_COUNT]) {
    int scores[TART_COMMAND_COUNT];
    int count = 0;
    for (size_t i = 0; i < TART_COMMAND_COUNT; i++) {
        const TartCommand *c = &TART_COMMANDS[i];
        int s_name = 0, s_alias = 0;
        bool m_name = tart_fuzzy_score(c->name, query, &s_name);
        bool m_alias = c->alias && tart_fuzzy_score(c->alias, query, &s_alias);
        if (!m_name && !m_alias) continue;
        match_idx[count] = (int)i;
        scores[count] = (m_name && (!m_alias || s_name <= s_alias)) ? s_name : s_alias;
        count++;
    }
    for (int i = 1; i < count; i++) {
        int idx = match_idx[i], score = scores[i], j = i - 1;
        while (j >= 0 && scores[j] > score) {
            scores[j + 1] = scores[j];
            match_idx[j + 1] = match_idx[j];
            j--;
        }
        scores[j + 1] = score;
        match_idx[j + 1] = idx;
    }
    return count;
}

int tart_build_tool_panel(App *app, TartPanelRow rows[TOOL_PANEL_MAX_ROWS]) {
    Viewport *vp = &app->viewport;
    int n = 0;

    rows[n++] = (TartPanelRow){ TP_ROW_PENCIL,  -1, -1, PANEL_SELECT_TOOL, TOOL_PENCIL };
    rows[n++] = (TartPanelRow){ TP_ROW_ERASER,  -1, -1, PANEL_SELECT_TOOL, TOOL_ERASER };
    rows[n++] = (TartPanelRow){ TP_ROW_RECT,    -1, -1, PANEL_SELECT_TOOL, TOOL_RECT };
    rows[n++] = (TartPanelRow){ TP_ROW_SELECT,  -1, -1, PANEL_SELECT_TOOL, TOOL_SELECT };
    rows[n++] = (TartPanelRow){ TP_ROW_MOVE,    -1, -1, PANEL_SELECT_TOOL, TOOL_MOVE };
    rows[n++] = (TartPanelRow){ TP_ROW_LINE,    -1, -1, PANEL_SELECT_TOOL, TOOL_LINE };
    rows[n++] = (TartPanelRow){ TP_ROW_CURVE,   -1, -1, PANEL_SELECT_TOOL, TOOL_CURVE };
    rows[n++] = (TartPanelRow){ TP_ROW_ELLIPSE, -1, -1, PANEL_SELECT_TOOL, TOOL_ELLIPSE };
    rows[n++] = (TartPanelRow){ TP_ROW_TRI,     -1, -1, PANEL_SELECT_TOOL, TOOL_TRI };
    rows[n++] = (TartPanelRow){ TP_ROW_FILL,    -1, -1, PANEL_SELECT_TOOL, TOOL_FILL };

    if (vp->tool == TOOL_PENCIL) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, -1, -1, PANEL_CHAR_OPEN, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, TP_SIZE_DEC_X, TP_SIZE_DEC_X, PANEL_SIZE_DEC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, TP_SIZE_INC_X, TP_SIZE_INC_X, PANEL_SIZE_INC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT3, -1, -1, PANEL_SHAPE_TOGGLE, 0 };
    } else if (vp->tool == TOOL_ERASER) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, TP_SIZE_DEC_X, TP_SIZE_DEC_X, PANEL_SIZE_DEC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, TP_SIZE_INC_X, TP_SIZE_INC_X, PANEL_SIZE_INC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, -1, -1, PANEL_SHAPE_TOGGLE, 0 };
    } else if (vp->tool == TOOL_RECT) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, -1, -1, PANEL_RECT_FILL_TOGGLE, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, -1, -1, PANEL_FILL_CHAR_OPEN, 0 };
    } else if (vp->tool == TOOL_MOVE) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, -1, -1, PANEL_MOVE_CUT_TOGGLE, 0 };
    } else if (vp->tool == TOOL_LINE || vp->tool == TOOL_CURVE ||
               vp->tool == TOOL_ELLIPSE || vp->tool == TOOL_TRI) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, -1, -1, PANEL_CHAR_OPEN, 0 };
    } else if (vp->tool == TOOL_FILL) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, -1, -1, PANEL_FILL_CHAR_OPEN, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT3, -1, -1, PANEL_FILL_RANDOM_TOGGLE, 0 };
    }

    if (vp->tool == TOOL_PENCIL || vp->tool == TOOL_ERASER) {
        for (int i = 0; i < (int)TART_BRUSH_SIZE_COUNT; i++) {
            int x = TP_SIZE_SLIDER_DOT_X0 + i;
            rows[n++] = (TartPanelRow){ TP_ROW_SIZE_SLIDER, x, x, PANEL_SIZE_SET, i };
        }
    }

    for (int i = 0; i < (int)TART_BASIC_COLOR_COUNT; i++) {
        int x = TP_COLOR_SLIDER_DOT_X0 + i;
        rows[n++] = (TartPanelRow){ TP_ROW_COLOR, x, x, PANEL_COLOR_SET, i };
    }
    rows[n++] = (TartPanelRow){ TP_ROW_COLOR, -1, -1, PANEL_COLOR_OPEN, 0 };
    return n;
}

void tart_draw_tool_panel(App *app, TartRect tool_r) {
    Viewport *vp = &app->viewport;

    attron(A_BOLD);
    mvprintw(tool_r.y + TP_ROW_TOOLS_HEADER, tool_r.x + 1, "Tools");
    attroff(A_BOLD);

    static const struct { int row; const char *label; TartTool tool; } tools[] = {
        { TP_ROW_PENCIL,  "Draw",     TOOL_PENCIL },
        { TP_ROW_ERASER,  "Eraser",   TOOL_ERASER },
        { TP_ROW_RECT,    "Rect",     TOOL_RECT },
        { TP_ROW_SELECT,  "Select",   TOOL_SELECT },
        { TP_ROW_MOVE,    "Move",     TOOL_MOVE },
        { TP_ROW_LINE,    "Line",     TOOL_LINE },
        { TP_ROW_CURVE,   "Curve",    TOOL_CURVE },
        { TP_ROW_ELLIPSE, "Ellipse",  TOOL_ELLIPSE },
        { TP_ROW_TRI,     "Triangle", TOOL_TRI },
        { TP_ROW_FILL,    "Fill",     TOOL_FILL },
    };
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        bool sel = (vp->tool == tools[i].tool);
        attr_t a = sel ? (A_REVERSE | A_BOLD) : A_NORMAL;
        attron(a);
        mvprintw(tool_r.y + tools[i].row, tool_r.x + 1, "%-*s", tool_r.w - 2, tools[i].label);
        attroff(a);
        /* Every tool binds to its label's first letter - lowercase for the
         * classic five (d/e/r/s/m), CAPITAL for the shapes (L/C/E/T) since
         * their lowercase letters are all taken by other bindings. Underline
         * it so the binding is visible without opening the help overlay. */
        attron(a | A_UNDERLINE);
        mvprintw(tool_r.y + tools[i].row, tool_r.x + 1, "%c", tools[i].label[0]);
        attroff(a | A_UNDERLINE);
    }

    const char *header = (vp->tool == TOOL_PENCIL) ? "Draw" :
                          (vp->tool == TOOL_ERASER) ? "Eraser" :
                          (vp->tool == TOOL_RECT)   ? "Rectangle" :
                          (vp->tool == TOOL_SELECT) ? "Select" :
                          (vp->tool == TOOL_MOVE)   ? "Move" :
                          (vp->tool == TOOL_LINE)   ? "Line" :
                          (vp->tool == TOOL_CURVE)  ? "Curve" :
                          (vp->tool == TOOL_ELLIPSE) ? "Ellipse" :
                          (vp->tool == TOOL_TRI)    ? "Triangle" : "Fill";
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(tool_r.y + TP_ROW_OPTIONS_HEADER, tool_r.x + 1, "%s", header);
    attroff(A_BOLD | COLOR_PAIR(1));

    if (vp->tool == TOOL_PENCIL) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Char: '%lc'", (wint_t)vp->brush_char);
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Size:%d  -  =", vp->brush_size);
        mvprintw(tool_r.y + TP_ROW_OPT3, tool_r.x + 1, "Shape: %s", vp->brush_shape == BRUSH_ROUND ? "round" : "square");
    } else if (vp->tool == TOOL_ERASER) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Size:%d  -  =", vp->eraser_size);
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Shape: %s", vp->eraser_shape == BRUSH_ROUND ? "round" : "square");
    } else if (vp->tool == TOOL_RECT) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Fill: %s (f)", vp->rect_filled ? "on" : "off");
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Fill char: '%lc'", (wint_t)vp->fill_char);
    } else if (vp->tool == TOOL_SELECT) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Drag to select");
    } else if (vp->tool == TOOL_MOVE) {
        if (vp->has_selection) mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Buf: %dx%d", vp->sel_w, vp->sel_h);
        else mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "(no selection)");
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Mode: %s (c)", vp->move_cut ? "cut" : "stamp");
    } else if (vp->tool == TOOL_LINE) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Drag start-end");
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Char: '%lc'", (wint_t)vp->brush_char);
    } else if (vp->tool == TOOL_CURVE) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, vp->curve_pending ? "Click sets bend" : "Drag ends first");
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Char: '%lc'", (wint_t)vp->brush_char);
    } else if (vp->tool == TOOL_ELLIPSE || vp->tool == TOOL_TRI) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Drag bounding box");
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Char: '%lc'", (wint_t)vp->brush_char);
    } else if (vp->tool == TOOL_FILL) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Click fills region");
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Char: '%lc'", (wint_t)vp->fill_char);
        mvprintw(tool_r.y + TP_ROW_OPT3, tool_r.x + 1, "Random: %s (f)", vp->fill_random ? "on" : "off");
    }

    /* Size slider (pencil/eraser only): "-[dots]= NN", each dot a stop in
     * TART_BRUSH_SIZES, current one solid; the '-'/'=' bracketing chars are
     * literally the keys that step through it one stop at a time, while
     * clicking a dot jumps straight to that size. */
    if (vp->tool == TOOL_PENCIL || vp->tool == TOOL_ERASER) {
        int size = (vp->tool == TOOL_ERASER) ? vp->eraser_size : vp->brush_size;
        int idx = 0;
        for (int i = 0; i < (int)TART_BRUSH_SIZE_COUNT; i++) if (TART_BRUSH_SIZES[i] == size) idx = i;
        mvaddch(tool_r.y + TP_ROW_SIZE_SLIDER, tool_r.x + 1, '-');
        mvaddch(tool_r.y + TP_ROW_SIZE_SLIDER, tool_r.x + 2, '[');
        for (int i = 0; i < (int)TART_BRUSH_SIZE_COUNT; i++) {
            bool cur = (i == idx);
            attr_t a = COLOR_PAIR(4) | (cur ? (A_BOLD | A_REVERSE) : A_DIM);
            attron(a);
            mvaddch(tool_r.y + TP_ROW_SIZE_SLIDER, tool_r.x + TP_SIZE_SLIDER_DOT_X0 + i, cur ? '#' : '.');
            attroff(a);
        }
        mvaddch(tool_r.y + TP_ROW_SIZE_SLIDER, tool_r.x + TP_SIZE_SLIDER_DOT_X0 + (int)TART_BRUSH_SIZE_COUNT, ']');
        mvprintw(tool_r.y + TP_ROW_SIZE_SLIDER, tool_r.x + TP_SIZE_SLIDER_DOT_X0 + (int)TART_BRUSH_SIZE_COUNT + 1, "=%2d", size);
    }

    /* Color slider: same idea as the size slider, but each dot is drawn in
     * its own basic color so the row doubles as a mini legend. '['/']'
     * step it one stop at a time; clicking a dot jumps straight to it;
     * clicking anywhere else on the row opens the full HSL picker. */
    mvprintw(tool_r.y + TP_ROW_COLOR, tool_r.x + 1, "Color [");
    for (int i = 0; i < (int)TART_BASIC_COLOR_COUNT; i++) {
        bool cur = (i == vp->quick_color_idx);
        attr_t a = COLOR_PAIR(16 + TART_BASIC_COLORS[i]) | (cur ? (A_BOLD | A_REVERSE) : A_DIM);
        attron(a);
        mvaddch(tool_r.y + TP_ROW_COLOR, tool_r.x + TP_COLOR_SLIDER_DOT_X0 + i, cur ? '#' : '.');
        attroff(a);
    }
    mvaddch(tool_r.y + TP_ROW_COLOR, tool_r.x + TP_COLOR_SLIDER_DOT_X0 + (int)TART_BASIC_COLOR_COUNT, ']');
}

/* Appends one anim-bar item at *x with a two-space gap. disp_w is on-screen
 * width, which differs from strlen for the UTF-8 │ separators. */
static int tart_anim_append(TartAnimItem *items, int n, int *x, const char *label, int disp_w, TartAnimAction action) {
    items[n] = (TartAnimItem){ label, *x, *x + disp_w - 1, action };
    *x += disp_w + 2;
    return n + 1;
}

int tart_build_anim_bar(App *app, TartAnimItem items[TART_ANIM_MAX_ITEMS]) {
    static char frame_num[24], layer_num[24], fps_num[16];
    snprintf(frame_num, sizeof(frame_num), "%d/%d", app->cur_frame + 1, app->frame_count);
    snprintf(layer_num, sizeof(layer_num), "%d/%d", app->cur_layer + 1, app->layer_count);
    snprintf(fps_num, sizeof(fps_num), "fps %d", app->fps);

    int n = 0, x = 1;
    n = tart_anim_append(items, n, &x, "frame", 5, ANIM_NONE);
    n = tart_anim_append(items, n, &x, frame_num, (int)strlen(frame_num), ANIM_NONE);
    n = tart_anim_append(items, n, &x, "<", 1, ANIM_FRAME_PREV);
    n = tart_anim_append(items, n, &x, ">", 1, ANIM_FRAME_NEXT);
    n = tart_anim_append(items, n, &x, "+", 1, ANIM_FRAME_ADD);
    n = tart_anim_append(items, n, &x, "dup", 3, ANIM_FRAME_DUP);
    n = tart_anim_append(items, n, &x, "del", 3, ANIM_FRAME_DEL);
    n = tart_anim_append(items, n, &x, "│", 1, ANIM_NONE);
    n = tart_anim_append(items, n, &x, "layer", 5, ANIM_NONE);
    n = tart_anim_append(items, n, &x, layer_num, (int)strlen(layer_num), ANIM_NONE);
    n = tart_anim_append(items, n, &x, "{", 1, ANIM_LAYER_PREV);
    n = tart_anim_append(items, n, &x, "}", 1, ANIM_LAYER_NEXT);
    n = tart_anim_append(items, n, &x, "+", 1, ANIM_LAYER_ADD);
    n = tart_anim_append(items, n, &x, "del", 3, ANIM_LAYER_DEL);
    n = tart_anim_append(items, n, &x, "│", 1, ANIM_NONE);
    n = tart_anim_append(items, n, &x, app->playing ? "stop" : "play", 4, ANIM_PLAY);
    n = tart_anim_append(items, n, &x, app->onion ? "onion:on" : "onion:off", app->onion ? 8 : 9, ANIM_ONION);
    n = tart_anim_append(items, n, &x, fps_num, (int)strlen(fps_num), ANIM_FPS);
    return n;
}

void tart_draw_anim_bar(App *app, TartRect anim_r) {
    TartAnimItem items[TART_ANIM_MAX_ITEMS];
    int n = tart_build_anim_bar(app, items);

    attron(COLOR_PAIR(5));
    mvprintw(anim_r.y, anim_r.x, "%-*s", anim_r.w, "");
    attroff(COLOR_PAIR(5));

    for (int i = 0; i < n; i++) {
        bool active = (items[i].action == ANIM_PLAY && app->playing) ||
                      (items[i].action == ANIM_ONION && app->onion);
        attr_t a = active ? (COLOR_PAIR(6) | A_BOLD)
                 : (items[i].action == ANIM_NONE) ? COLOR_PAIR(5)
                 : (COLOR_PAIR(5) | A_BOLD);
        attron(a);
        mvprintw(anim_r.y, items[i].x0, "%s", items[i].label);
        attroff(a);
    }
}

int tart_build_menu_bar(App *app, TartMenuBarItem items[TART_MENU_COUNT]) {
    (void)app;
    int x = 1;
    for (size_t i = 0; i < TART_MENU_COUNT; i++) {
        items[i].label = TART_MENUS[i].label;
        items[i].x0 = x;
        items[i].x1 = x + (int)strlen(TART_MENUS[i].label) - 1;
        x = items[i].x1 + 3;
    }
    return TART_MENU_COUNT;
}

void tart_draw_menu_dropdown(App *app, TartRect menu_r) {
    Viewport *vp = &app->viewport;
    if (vp->open_menu < 0) return;

    TartMenuBarItem mitems[TART_MENU_COUNT];
    tart_build_menu_bar(app, mitems);
    const TartMenuDef *menu = &TART_MENUS[vp->open_menu];

    int w = 6;
    for (int i = 0; i < menu->item_count; i++) {
        const TartCommand *c = tart_find_command(menu->items[i]);
        int l = (c ? (int)strlen(c->desc) : (int)strlen(menu->items[i])) + 3;
        if (l > w) w = l;
    }
    int x = mitems[vp->open_menu].x0;
    int y = menu_r.y + 1;

    attron(COLOR_PAIR(5));
    for (int i = 0; i < menu->item_count; i++) {
        const TartCommand *c = tart_find_command(menu->items[i]);
        const char *label = c ? c->desc : menu->items[i];
        bool hovered = (i == vp->menu_hover_item);
        mvprintw(y + i, x, "%*s", w, "");
        if (hovered) attron(COLOR_PAIR(6) | A_BOLD);
        mvprintw(y + i, x, " %-*s", w - 1, label);
        if (hovered) attroff(COLOR_PAIR(6) | A_BOLD);
    }
    attroff(COLOR_PAIR(5));
}

/* The verb being matched is only the part of the buffer before the first
 * space - once a space is typed the rest is a command argument (a filename
 * for save/load, WxH for resize), not more of the fuzzy query. */
void tart_command_verb(const char *buf, char *out, size_t out_sz) {
    const char *sp = strchr(buf, ' ');
    size_t vlen = sp ? (size_t)(sp - buf) : strlen(buf);
    if (vlen >= out_sz) vlen = out_sz - 1;
    memcpy(out, buf, vlen);
    out[vlen] = '\0';
}

void tart_draw_command_palette(App *app, TartRect status_r) {
    Viewport *vp = &app->viewport;
    const char *buf = vp->command_buffer;

    char verb[64];
    tart_command_verb(buf, verb, sizeof(verb));
    int match_idx[TART_COMMAND_COUNT];
    int match_count = tart_command_matches(verb, match_idx);

    if (vp->cmd_selected < 0) vp->cmd_selected = 0;
    if (vp->cmd_selected >= match_count) vp->cmd_selected = match_count > 0 ? match_count - 1 : 0;
    int visible = (match_count < CMD_PALETTE_MAX_VISIBLE) ? match_count : CMD_PALETTE_MAX_VISIBLE;
    int max_scroll = (match_count > visible) ? match_count - visible : 0;
    if (vp->cmd_scroll_off > vp->cmd_selected) vp->cmd_scroll_off = vp->cmd_selected;
    if (visible > 0 && vp->cmd_selected >= vp->cmd_scroll_off + visible) vp->cmd_scroll_off = vp->cmd_selected - visible + 1;
    if (vp->cmd_scroll_off > max_scroll) vp->cmd_scroll_off = max_scroll;
    if (vp->cmd_scroll_off < 0) vp->cmd_scroll_off = 0;

    int popup_h = (visible > 0) ? visible : 1;
    int popup_y = status_r.y - popup_h;
    if (popup_y < 0) popup_y = 0;

    attron(COLOR_PAIR(5));
    for (int row = 0; row < popup_h; row++) mvprintw(popup_y + row, 0, "%*s", COLS, "");
    if (match_count == 0) {
        attron(A_DIM);
        mvprintw(popup_y, 2, "no matching command");
        attroff(A_DIM);
    }
    for (int i = 0; i < visible; i++) {
        int mi = vp->cmd_scroll_off + i;
        const TartCommand *c = &TART_COMMANDS[match_idx[mi]];
        bool sel = (mi == vp->cmd_selected);
        if (sel) attron(COLOR_PAIR(6) | A_BOLD);
        mvprintw(popup_y + i, 2, "%-10s %s", c->name, c->desc);
        if (sel) attroff(COLOR_PAIR(6) | A_BOLD);
    }
    attroff(COLOR_PAIR(5));

    attron(COLOR_PAIR(5));
    mvprintw(status_r.y, status_r.x, "%*s", status_r.w, "");
    attroff(COLOR_PAIR(5));
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(status_r.y, status_r.x, ":");
    attroff(COLOR_PAIR(5) | A_BOLD);
    attron(COLOR_PAIR(5));
    mvprintw(status_r.y, status_r.x + 1, "%s", buf);
    attroff(COLOR_PAIR(5));
}

static const char *tart_prompt_label(TartPromptMode mode) {
    switch (mode) {
        case PROMPT_RESIZE:      return "Resize to WxH:";
        case PROMPT_SAVE:        return "Save as:";
        case PROMPT_LOAD:        return "Load file:";
        case PROMPT_CUSTOM_CHAR: return "Custom char (hex codepoint):";
        case PROMPT_FPS:         return "Playback FPS (1-60):";
        default:                 return "";
    }
}

void tart_draw_prompt(App *app, TartRect status_r) {
    Viewport *vp = &app->viewport;
    attron(COLOR_PAIR(5));
    mvprintw(status_r.y, status_r.x, "%*s", status_r.w, "");
    attroff(COLOR_PAIR(5));
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(status_r.y, status_r.x, "%s ", tart_prompt_label(vp->prompt_mode));
    attroff(COLOR_PAIR(5) | A_BOLD);
    attron(COLOR_PAIR(5));
    mvprintw(status_r.y, status_r.x + (int)strlen(tart_prompt_label(vp->prompt_mode)) + 1, "%s_", vp->prompt_buffer);
    attroff(COLOR_PAIR(5));
}

void tart_compute_palette_geom(int *out_x, int *out_y, int *out_w, int *out_h, int *out_max_chars) {
    int n = (int)TART_PALETTE_CAT_COUNT;
    int max_chars = 0;
    for (int i = 0; i < n; i++) {
        int l = (int)wcslen(TART_PALETTE[i].chars);
        if (l > max_chars) max_chars = l;
    }
    int w = PALETTE_LABEL_WIDTH + max_chars * PALETTE_CHAR_CELL_W + 4;
    int h = n + 5; /* top border, title, category rows, custom row, hint row, bottom border */
    int x = (COLS - w) / 2;
    int y = (LINES - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    *out_x = x;
    *out_y = y;
    *out_w = w;
    *out_h = h;
    *out_max_chars = max_chars;
}

void tart_draw_palette(App *app) {
    Viewport *vp = &app->viewport;
    int x, y, w, h, max_chars;
    tart_compute_palette_geom(&x, &y, &w, &h, &max_chars);
    (void)max_chars;
    int n = (int)TART_PALETTE_CAT_COUNT;

    attron(COLOR_PAIR(2));
    for (int row = 0; row < h; row++) {
        mvprintw(y + row, x, "%*s", w, "");
    }
    tart_draw_box(x, y, w, h);

    attron(A_BOLD);
    mvprintw(y + 1, x + 2, "Select a character (click, arrows+Enter, Esc)");
    attroff(A_BOLD);

    int content_y = y + 2;
    for (int i = 0; i < n; i++) {
        int row_y = content_y + i;
        mvprintw(row_y, x + 2, "%-*s", PALETTE_LABEL_WIDTH, TART_PALETTE[i].label);
        const wchar_t *chars = TART_PALETTE[i].chars;
        int clen = (int)wcslen(chars);
        for (int c = 0; c < clen; c++) {
            int cx = x + 2 + PALETTE_LABEL_WIDTH + c * PALETTE_CHAR_CELL_W;
            bool hovered = (vp->palette_hover_row == i && vp->palette_hover_char == c);
            if (hovered) attron(A_REVERSE);
            tart_addwch(row_y, cx, chars[c]);
            if (hovered) attroff(A_REVERSE);
        }
    }

    int custom_row_y = content_y + n;
    bool custom_hovered = (vp->palette_hover_row == n);
    if (custom_hovered) attron(A_REVERSE);
    mvprintw(custom_row_y, x + 2, "Custom character (hex codepoint)...");
    if (custom_hovered) attroff(A_REVERSE);

    mvprintw(y + h - 2, x + 2, "Esc to cancel");
    attroff(COLOR_PAIR(2));
}

void tart_compute_color_picker_geom(int *out_x, int *out_y, int *out_w, int *out_h, int *out_bar_x, int *out_bar_w) {
    int w = 66;
    int h = 12;
    int x = (COLS - w) / 2;
    int y = (LINES - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    *out_x = x;
    *out_y = y;
    *out_w = w;
    *out_h = h;
    *out_bar_x = x + 9;
    *out_bar_w = 50;
}

void tart_draw_color_picker(App *app) {
    Viewport *vp = &app->viewport;
    int x, y, w, h, bar_x, bar_w;
    tart_compute_color_picker_geom(&x, &y, &w, &h, &bar_x, &bar_w);

    attron(COLOR_PAIR(2));
    for (int row = 0; row < h; row++) mvprintw(y + row, x, "%*s", w, "");
    tart_draw_box(x, y, w, h);
    attron(A_BOLD);
    mvprintw(y + 1, x + 2, "Color Picker");
    attroff(A_BOLD | COLOR_PAIR(2));

    static const char *labels[3] = { "Hue  ", "Sat  ", "Light" };
    for (int slider = 0; slider < 3; slider++) {
        int row_y = y + 3 + slider;
        bool focused = (vp->color_slider_focus == slider);
        attron(focused ? A_BOLD : A_NORMAL);
        mvprintw(row_y, x + 2, "%s", labels[slider]);
        attroff(focused ? A_BOLD : A_NORMAL);

        int value = (slider == 0) ? vp->hsl_h : (slider == 1) ? vp->hsl_s : vp->hsl_l;
        int max_v = (slider == 0) ? 360 : 100;
        for (int i = 0; i < bar_w; i++) {
            int h2 = (slider == 0) ? (i * 360 / bar_w) : vp->hsl_h;
            int s2 = (slider == 1) ? (i * 100 / bar_w) : vp->hsl_s;
            int l2 = (slider == 2) ? (i * 100 / bar_w) : vp->hsl_l;
            int r, g, b;
            tart_hsl_to_rgb(h2, s2, l2, &r, &g, &b);
            int idx = tart_rgb_to_256(r, g, b);
            attron(COLOR_PAIR(16 + idx));
            mvaddch(row_y, bar_x + i, '#');
            attroff(COLOR_PAIR(16 + idx));
        }
        int marker_x = bar_x + (value * (bar_w - 1) / max_v);
        attron(A_REVERSE);
        mvaddch(row_y, marker_x, ' ');
        attroff(A_REVERSE);
        mvprintw(row_y, bar_x + bar_w + 2, "%3d", value);
    }

    attron(COLOR_PAIR(2));
    mvprintw(y + 7, x + 2, "Preview:");
    int r, g, b;
    tart_hsl_to_rgb(vp->hsl_h, vp->hsl_s, vp->hsl_l, &r, &g, &b);
    int idx = tart_rgb_to_256(r, g, b);
    bool swatch_focused = (vp->color_slider_focus == 3);
    attron(COLOR_PAIR(16 + idx) | (swatch_focused ? A_REVERSE : A_NORMAL));
    mvprintw(y + 7, x + 11, "        ");
    attroff(COLOR_PAIR(16 + idx) | (swatch_focused ? A_REVERSE : A_NORMAL));

    mvprintw(y + 9, x + 2, "Tab/click bar focus, drag or arrows to adjust, Enter/click swatch = confirm, Esc cancel");
    attroff(COLOR_PAIR(2));
}

/* Resizes one canvas in place, preserving overlapping content. On alloc
 * failure the canvas is untouched. */
static bool tart_canvas_resize(Canvas *c, int new_w, int new_h) {
    wchar_t *new_cells = malloc(sizeof(wchar_t) * (size_t)new_w * new_h);
    unsigned char *new_colors = malloc((size_t)new_w * new_h);
    if (!new_cells || !new_colors) { free(new_cells); free(new_colors); return false; }
    wmemset(new_cells, L' ', (size_t)new_w * new_h);
    memset(new_colors, 0, (size_t)new_w * new_h);

    int copy_w = (new_w < c->width) ? new_w : c->width;
    int copy_h = (new_h < c->height) ? new_h : c->height;
    for (int y = 0; y < copy_h; y++) {
        wmemcpy(&new_cells[y * new_w], &c->cells[y * c->width], (size_t)copy_w);
        memcpy(&new_colors[y * new_w], &c->colors[y * c->width], (size_t)copy_w);
    }

    free(c->cells);
    free(c->colors);
    c->cells = new_cells;
    c->colors = new_colors;
    c->width = new_w;
    c->height = new_h;
    return true;
}

/* Every frame/layer canvas keeps identical dimensions (the compositor
 * depends on it), so resize walks the whole document; on a mid-walk alloc
 * failure the already-resized canvases are put back to the old size before
 * returning, keeping the invariant. */
bool tart_resize_doc(App *app, int new_w, int new_h) {
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;
    if (new_w > MAX_CANVAS_WIDTH) new_w = MAX_CANVAS_WIDTH;
    if (new_h > MAX_CANVAS_HEIGHT) new_h = MAX_CANVAS_HEIGHT;

    int done = 0;
    for (int f = 0; f < app->frame_count; f++) {
        for (int l = 0; l < app->layer_count; l++) {
            if (!tart_canvas_resize(&app->frames[f].layers[l], new_w, new_h)) {
                for (int i = 0; i < done; i++)
                    tart_canvas_resize(&app->frames[i / app->layer_count].layers[i % app->layer_count],
                                       app->width, app->height);
                return false;
            }
            done++;
        }
    }
    app->width = new_w;
    app->height = new_h;
    tart_undo_clear(app);
    return true;
}

/* .tart format, plain text throughout:
 *
 *   <width> <height> [fps]
 *   <height rows of glyphs>              frame 1, layer 1
 *   COLOR                                optional per layer: <height> rows
 *   <hex rows>                           of 2-hex-digit palette indices
 *   LAYER                                next layer of the same frame
 *   <rows...>
 *   FRAME                                next frame (restarts at layer 1)
 *   <rows...>
 *
 * A v1 file (no fps, no markers) is exactly one frame of one layer, and a
 * one-frame one-layer save is a valid v1 file plus the fps field. */

static void tart_skip_line(FILE *f) {
    wint_t wc;
    while ((wc = fgetwc(f)) != WEOF && wc != L'\n') {}
}

/* Reads c->height glyph rows starting at the current position (which must be
 * the start of a row). Short rows stay blank-padded; overlong rows are
 * clipped to c->width. */
static void tart_read_rows(FILE *f, Canvas *c) {
    for (int y = 0; y < c->height; y++) {
        int x = 0;
        wint_t wc;
        while (x < c->width && (wc = fgetwc(f)) != WEOF && wc != L'\n') {
            c->cells[y * c->width + x] = (wchar_t)wc;
            x++;
        }
        if (x == c->width) tart_skip_line(f);
    }
}

static bool tart_layer_has_color(Canvas *c) {
    for (int i = 0; i < c->width * c->height; i++) {
        if (c->colors[i] != 0) return true;
    }
    return false;
}

bool tart_save_doc(App *app, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fwide(f, 1);
    fwprintf(f, L"%d %d %d\n", app->width, app->height, app->fps);

    for (int fi = 0; fi < app->frame_count; fi++) {
        for (int li = 0; li < app->layer_count; li++) {
            if (fi > 0 && li == 0) fwprintf(f, L"FRAME\n");
            if (li > 0) fwprintf(f, L"LAYER\n");
            Canvas *c = &app->frames[fi].layers[li];
            for (int y = 0; y < c->height; y++) {
                for (int x = 0; x < c->width; x++) fputwc(c->cells[y * c->width + x], f);
                fputwc(L'\n', f);
            }
            if (tart_layer_has_color(c)) {
                fwprintf(f, L"COLOR\n");
                for (int y = 0; y < c->height; y++) {
                    for (int x = 0; x < c->width; x++) fwprintf(f, L"%02x", c->colors[y * c->width + x]);
                    fputwc(L'\n', f);
                }
            }
        }
    }

    fclose(f);
    return true;
}

bool tart_load_doc(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fwide(f, 1);

    int w, h;
    if (fwscanf(f, L"%d %d", &w, &h) != 2 || w < 1 || h < 1 ||
        w > MAX_CANVAS_WIDTH || h > MAX_CANVAS_HEIGHT) {
        fclose(f);
        return false;
    }

    /* Optional fps rides the header line; parse only up to the newline
     * because %d would happily skip into the first glyph row. */
    int fps = 8;
    {
        wchar_t rest[32];
        int ri = 0;
        wint_t wc;
        while ((wc = fgetwc(f)) != WEOF && wc != L'\n') {
            if (ri < 31) rest[ri++] = (wchar_t)wc;
        }
        rest[ri] = L'\0';
        int v;
        if (swscanf(rest, L"%d", &v) == 1 && v >= 1 && v <= 60) fps = v;
    }

    /* Stage the whole document before touching app, so a malformed or
     * partially-read file can't destroy the open one. */
    static Frame frames[MAX_FRAMES];
    memset(frames, 0, sizeof(frames));
    int cur_f = 0, cur_l = 0, max_layers = 1;
    bool ok = init_canvas(&frames[0].layers[0], w, h);
    if (ok) tart_read_rows(f, &frames[0].layers[0]);

    wchar_t marker[8];
    while (ok && fwscanf(f, L"%7ls", marker) == 1) {
        if (wcscmp(marker, L"COLOR") == 0) {
            Canvas *c = &frames[cur_f].layers[cur_l];
            for (int i = 0; i < w * h; i++) {
                unsigned int val = 0;
                if (fwscanf(f, L"%2x", &val) == 1) c->colors[i] = (unsigned char)val;
            }
        } else if (wcscmp(marker, L"LAYER") == 0 && cur_l + 1 < MAX_LAYERS) {
            cur_l++;
            if (cur_l + 1 > max_layers) max_layers = cur_l + 1;
            ok = init_canvas(&frames[cur_f].layers[cur_l], w, h);
            if (ok) { tart_skip_line(f); tart_read_rows(f, &frames[cur_f].layers[cur_l]); }
        } else if (wcscmp(marker, L"FRAME") == 0 && cur_f + 1 < MAX_FRAMES) {
            cur_f++;
            cur_l = 0;
            ok = init_canvas(&frames[cur_f].layers[0], w, h);
            if (ok) { tart_skip_line(f); tart_read_rows(f, &frames[cur_f].layers[0]); }
        } else {
            break;  /* unknown marker or capacity hit: keep what we have */
        }
    }
    fclose(f);

    /* The format allows ragged layer counts; pad every frame to the max so
     * the uniform-layers invariant holds. */
    int fcount = cur_f + 1;
    for (int fi = 0; ok && fi < fcount; fi++)
        for (int li = 0; ok && li < max_layers; li++)
            if (!frames[fi].layers[li].cells)
                ok = init_canvas(&frames[fi].layers[li], w, h);

    if (!ok) {
        for (int fi = 0; fi < MAX_FRAMES; fi++)
            for (int li = 0; li < MAX_LAYERS; li++)
                cleanup_canvas(&frames[fi].layers[li]);
        return false;
    }

    for (int fi = 0; fi < app->frame_count; fi++)
        for (int li = 0; li < app->layer_count; li++)
            cleanup_canvas(&app->frames[fi].layers[li]);
    memcpy(app->frames, frames, sizeof(frames));
    app->frame_count = fcount;
    app->layer_count = max_layers;
    app->cur_frame = 0;
    app->cur_layer = 0;
    app->width = w;
    app->height = h;
    app->fps = fps;
    app->playing = false;
    tart_undo_clear(app);
    return true;
}

void tart_draw_help_overlay(App *app) {
    (void)app;
    const char *lines[] = {
        "TART - Terminal ASCII Rendering Tool",
        "",
        "MODE:  n normal   c command   p pan",
        "",
        "Move    h j k l   w/b +-5 horiz   0/$ line start/end   G/g bottom/top",
        "Pan     arrows    Ctrl+d/u half page   PgUp/PgDn full page",
        "View    v toggle view/pan   wheel scroll (alt/shift/tilt = horizontal)   Home reset",
        "Tools   d draw   e eraser   r rect   s select   m move (underlined letter)",
        "Shapes  L line   C curve   E ellipse   T triangle - drag to place;",
        "        curve: drag the endpoints, then move to bend, click to set;",
        "        ellipse/triangle fit the dragged box (cells are ~2:1, so a",
        "        round-looking circle needs a box about twice as wide as tall)",
        "Fill    F bucket fill - click repaints the connected region with the",
        "        fill char; f toggles random texture mode (each cell rolls a",
        "        glyph from the fill char's palette row; click again re-rolls)",
        "Brush   t open tip palette (1-9 jump category)   -/= size (1-21)   Tab shape",
        "        f rect fill toggle",
        "Select  drag to select (stays outlined); Move: drag to stamp a copy,",
        "        repeatable by default - c toggles cut mode (clears source, one-shot)",
        "Draw    left-click hold to use tool   space+move = alt draw/pan   Esc cancels",
        "Edit    u undo   U redo   H history viewer (Up/Down walk the timeline)",
        "        one step per stroke/stamp; frame, layer, resize, load reset it",
        "Color   [ ] cycle 8 quick colors   click Color row for full HSL picker",
        "Anim    n/N next/prev frame   a add blank   A duplicate   x delete",
        "        Enter play/stop   o onion skin (ghost of previous frame)   :fps N",
        "Layer   { } prev/next   Layer menu or :layer-add / :layer-del",
        "        space is transparent - lower layers show through; edits hit the",
        "        active layer only (see the anim bar / status readout)",
        "Image   Image menu > Resize Canvas (WxH) - resizes every frame and layer",
        "File    File menu > Save/Load, or :save <name> / :load <name>",
        "Menu    Alt+F/E/V/I/A/L/H or click to open, arrows to move, Enter, Esc closes",
        "Cmd     : or Ctrl+E, fuzzy-searches as you type, Up/Down or wheel to pick,",
        "          Enter or click a row to run (save, load, resize, play, fps,",
        "          frame-add/dup/del, layer-add/del, onion, deselect, quit, ...)",
        "",
        "Press any key to close",
    };
    int n = (int)(sizeof(lines) / sizeof(lines[0]));

    int w = 0;
    for (int i = 0; i < n; i++) {
        int l = (int)strlen(lines[i]);
        if (l > w) w = l;
    }
    w += 4;
    int h = n + 2;
    int x = (COLS - w) / 2;
    int y = (LINES - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    attron(COLOR_PAIR(2));
    for (int row = 0; row < h; row++) {
        mvprintw(y + row, x, "%*s", w, "");
    }
    tart_draw_box(x, y, w, h);
    for (int i = 0; i < n; i++) {
        mvprintw(y + 1 + i, x + 2, "%s", lines[i]);
    }
    attroff(COLOR_PAIR(2));
}

void tart_draw_about_overlay(App *app) {
    (void)app;
    const char *banner[] = {
        " _____  _    ____ _____",
        "|_   _|/ \\  |  _ \\_   _|",
        "  | | / _ \\ | |_) || |",
        "  | |/ ___ \\|  _ < | |",
        "  |_/_/   \\_\\_| \\_\\|_|",
    };
    int nb = (int)(sizeof(banner) / sizeof(banner[0]));

    const char *lines[] = {
        "Terminal ASCII (Art) Rendering Tool  v" TART_VERSION,
        "",
        "A small, suckless-flavored TUI ASCII-art and animation editor:",
        "draw/eraser/rect/line/curve/ellipse/triangle/fill/select/move,",
        "a 256-color canvas, stacked layers, animation frames with",
        "playback and onion skin, undo/redo with a history viewer,",
        "resizable images, plain-text .tart save/load.",
        "",
        "Built with C99 + ncursesw. No dependencies beyond that.",
        "",
        "Press any key to close",
    };
    int n = (int)(sizeof(lines) / sizeof(lines[0]));

    int w = 0;
    for (int i = 0; i < nb; i++) { int l = (int)strlen(banner[i]); if (l > w) w = l; }
    for (int i = 0; i < n; i++) { int l = (int)strlen(lines[i]); if (l > w) w = l; }
    w += 4;
    int h = nb + n + 2;
    int x = (COLS - w) / 2;
    int y = (LINES - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    attron(COLOR_PAIR(2));
    for (int row = 0; row < h; row++) {
        mvprintw(y + row, x, "%*s", w, "");
    }
    tart_draw_box(x, y, w, h);
    attroff(COLOR_PAIR(2));

    attron(COLOR_PAIR(1) | A_BOLD);
    for (int i = 0; i < nb; i++) {
        mvprintw(y + 1 + i, x + 2, "%s", banner[i]);
    }
    attroff(COLOR_PAIR(1) | A_BOLD);

    attron(COLOR_PAIR(2));
    for (int i = 0; i < n; i++) {
        mvprintw(y + 1 + nb + i, x + 2, "%s", lines[i]);
    }
    attroff(COLOR_PAIR(2));
}

#endif