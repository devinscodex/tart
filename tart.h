#ifndef TART_H
#define TART_H

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <ncursesw/curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <math.h>
#include <ctype.h>

/* Coordinate convention used throughout: sx/sy name a screen (terminal cell)
 * position; wx/wy name a world (canvas/document) position. offset_x/offset_y
 * is the world coordinate currently sitting under the canvas viewport's
 * top-left screen cell, i.e. the pan origin. */

#define MAX_CANVAS_WIDTH  512
#define MAX_CANVAS_HEIGHT 256

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
 * column the right border/scrollbar renders on; kept as one macro since
 * several places outside tart_compute_layout need to agree with it exactly
 * (view clamping, End-key centering, ...). */
#define CANVAS_VIEW_W (COLS - CANVAS_START_X - 1)
#define CANVAS_VIEW_H (LINES - 4)

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
    { "deselect", NULL, "Clear current selection" },
    { "reset-view", NULL, "Reset pan offset to origin" },
    { "toggle-pan", NULL, "Toggle pan/normal mode" },
    { "resize", NULL, "Resize canvas (WxH)" },
    { "save", NULL, "Save canvas to file" },
    { "load", NULL, "Load canvas from file" },
};
#define TART_COMMAND_COUNT (sizeof(TART_COMMANDS) / sizeof(TART_COMMANDS[0]))

/* One top-level menu (File/Edit/View/Image/Help): a label plus the command
 * names (from TART_COMMANDS) shown in its dropdown, in order. */
typedef struct {
    const char *label;
    const char *items[3];
    int item_count;
} TartMenuDef;

static const TartMenuDef TART_MENUS[] = {
    { "File",  { "save", "load", "quit" },     3 },
    { "Edit",  { "deselect" },                 1 },
    { "View",  { "reset-view", "toggle-pan" }, 2 },
    { "Image", { "resize" },                   1 },
    { "Help",  { "help" },                     1 },
};
#define TART_MENU_COUNT (sizeof(TART_MENUS) / sizeof(TART_MENUS[0]))

/* Computed screen position of one menu-bar label, so the renderer and the
 * click hit-test in input.c agree on where each menu name sits. */
typedef struct {
    const char *label;
    int x0, x1;
} TartMenuBarItem;

typedef enum { TOOL_PENCIL, TOOL_ERASER, TOOL_RECT, TOOL_SELECT, TOOL_MOVE } TartTool;
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
} TartPromptMode;

/* What clicking a given tool-panel row does; see tart_apply_panel_action.
 * Shared between the panel's own click hit-test and identical keyboard
 * shortcuts ('[', ']', Tab, ...) so both paths funnel through one place. */
typedef enum {
    PANEL_NONE,
    PANEL_SELECT_TOOL,
    PANEL_CHAR_OPEN,
    PANEL_SIZE_DEC,
    PANEL_SIZE_INC,
    PANEL_SHAPE_TOGGLE,
    PANEL_RECT_FILL_TOGGLE,
    PANEL_FILL_CHAR_OPEN,
    PANEL_COLOR_OPEN,
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

/* Quick-access basic colors for the '-'/'=' cycle keys, so common colors
 * don't require opening the full HSL picker. Values are xterm-256 indices;
 * plain black (0) is skipped because Canvas.colors uses 0 as its own
 * "no color" sentinel, so cube-black (16) stands in for it instead. */
static const unsigned char TART_BASIC_COLORS[] = { 15, 9, 10, 11, 12, 13, 14, 16 };
#define TART_BASIC_COLOR_COUNT (sizeof(TART_BASIC_COLORS) / sizeof(TART_BASIC_COLORS[0]))

typedef struct {
    int row;            /* y offset from tool_r.y */
    int x0, x1;         /* x offset range from tool_r.x, inclusive; -1,-1 = whole row */
    TartPanelAction action;
    int value;          /* payload, e.g. TartTool for PANEL_SELECT_TOOL */
} TartPanelRow;

#define TOOL_PANEL_MAX_ROWS 16

/* Row layout shared by the tool-panel renderer and its click hit-test, so
 * they can never drift out of sync with each other. */
#define TP_ROW_TOOLS_HEADER   1
#define TP_ROW_PENCIL         3
#define TP_ROW_ERASER         4
#define TP_ROW_RECT           5
#define TP_ROW_SELECT         6
#define TP_ROW_MOVE           7
#define TP_ROW_OPTIONS_HEADER 9
#define TP_ROW_OPT1           10
#define TP_ROW_OPT2           11
#define TP_ROW_OPT3           12
#define TP_ROW_COLOR          14
#define TP_SIZE_DEC_X         9
#define TP_SIZE_INC_X         12

#define PAN_VOID_MARGIN 10

typedef struct {
    int width;
    int height;
    wchar_t *cells;
    unsigned char *colors; /* 0 = no color/default; 1-255 = xterm-256 palette index */
} Canvas;

typedef struct {
    Canvas *canvas;
    int offset_x;                   /* viewport pan origin, in world coords */
    int offset_y;
    int cursor_doc_x;
    int cursor_doc_y;
    int last_mouse_screen_x;
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

    /* Shared drag state for rect/select/move (see tart_tool_rect etc. in
     * input.c) - what's being dragged depends on `tool` above. */
    bool tool_dragging;
    int drag_start_wx, drag_start_wy;
    int drag_cur_wx, drag_cur_wy;
    bool has_selection;
    int sel_w, sel_h;
    wchar_t *sel_buffer;
    unsigned char *sel_colors;

    /* Color: current_color paints; the HSL picker and the '-'/'=' quick
     * palette are two different ways of setting it. */
    unsigned char current_color;
    int quick_color_idx;        /* position in TART_BASIC_COLORS for -/= cycling */
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
    int mouse_world_x;
    int mouse_world_y;
    int vim_cursor_x;
    int vim_cursor_y;

    int command_mode;
    char command_buffer[256];
    int command_len;
    char mode;
    bool space_held;
    bool left_click_held;   /* drives "is a draw/pan gesture active" */
    bool mod_shift;         /* shift+wheel = horizontal scroll */
    bool scrollbar_drag_v;
    bool scrollbar_drag_h;
    bool help_visible;
} Viewport;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} TartRect;

typedef struct {
    Canvas canvas;
    Viewport viewport;
    int running;
} App;

void init_app(App *app);
void cleanup_app(App *app);
void init_canvas(Canvas *c, int w, int h);
void cleanup_canvas(Canvas *c);
void render(App *app);
void handle_input(App *app);
void tart_compute_layout(App *app, TartRect *menu, TartRect *ruler, TartRect *tool, TartRect *y_ruler, TartRect *canvas, TartRect *status);
void tart_draw_at(App *app, int world_x, int world_y, wchar_t ch);
void tart_stamp(App *app, int cx, int cy, int size, BrushShape shape, wchar_t ch, unsigned char color);
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
void tart_draw_prompt(App *app, TartRect status_r);
void tart_compute_palette_geom(int *out_x, int *out_y, int *out_w, int *out_h, int *out_max_chars);
void tart_draw_palette(App *app);
bool tart_resize_canvas(App *app, int new_w, int new_h);
bool tart_save_canvas(App *app, const char *path);
bool tart_load_canvas(App *app, const char *path);
void tart_draw_box(int x, int y, int w, int h);
int tart_command_matches(const char *query, int match_idx[TART_COMMAND_COUNT]);
void tart_command_verb(const char *buf, char *out, size_t out_sz);
void tart_scrollbar_thumb(int content, int visible, int pos, int track_len, int *out_start, int *out_len);
void tart_scrollbar_seek(int content, int visible, int track_pos, int track_len, int *out_pos);
void tart_draw_scrollbars(App *app, TartRect canvas_r);

#endif

#ifdef TART_IMPLEMENTATION

void init_canvas(Canvas *c, int w, int h) {
    c->width = w;
    c->height = h;
    c->cells = malloc(sizeof(wchar_t) * (size_t)w * h);
    if (c->cells) wmemset(c->cells, L' ', (size_t)w * h);
    c->colors = malloc((size_t)w * h);
    if (c->colors) memset(c->colors, 0, (size_t)w * h);
}

void cleanup_canvas(Canvas *c) {
    if (c->cells) { free(c->cells); c->cells = NULL; }
    if (c->colors) { free(c->colors); c->colors = NULL; }
}

void init_app(App *app) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    timeout(50);
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

    init_canvas(&app->canvas, TART_DEFAULT_CANVAS_WIDTH, TART_DEFAULT_CANVAS_HEIGHT);
    app->viewport.canvas = &app->canvas;
    app->viewport.offset_x = -2;
    app->viewport.offset_y = -2;
    app->viewport.cursor_doc_x = 0;
    app->viewport.cursor_doc_y = 0;
    app->viewport.last_mouse_screen_x = 0;
    app->viewport.last_mouse_screen_y = 0;
    app->viewport.is_dragging = 0;
    app->viewport.tool = TOOL_PENCIL;
    app->viewport.brush_char = L'#';
    app->viewport.brush_size = 1;
    app->viewport.brush_shape = BRUSH_SQUARE;
    app->viewport.eraser_size = 1;
    app->viewport.eraser_shape = BRUSH_SQUARE;
    app->viewport.rect_filled = false;
    app->viewport.fill_char = L'#';
    app->viewport.palette_visible = false;
    app->viewport.palette_target = PALETTE_TARGET_PENCIL;
    app->viewport.palette_hover_row = 0;
    app->viewport.palette_hover_char = 0;
    app->viewport.tool_dragging = false;
    app->viewport.drag_start_wx = 0;
    app->viewport.drag_start_wy = 0;
    app->viewport.drag_cur_wx = 0;
    app->viewport.drag_cur_wy = 0;
    app->viewport.has_selection = false;
    app->viewport.sel_w = 0;
    app->viewport.sel_h = 0;
    app->viewport.sel_buffer = NULL;
    app->viewport.sel_colors = NULL;
    app->viewport.current_color = 0;
    app->viewport.quick_color_idx = 0;
    app->viewport.color_picker_visible = false;
    app->viewport.hsl_h = 0;
    app->viewport.hsl_s = 100;
    app->viewport.hsl_l = 50;
    app->viewport.color_slider_focus = 0;
    app->viewport.open_menu = -1;
    app->viewport.menu_hover_item = 0;
    app->viewport.cmd_selected = 0;
    app->viewport.cmd_scroll_off = 0;
    app->viewport.prompt_mode = PROMPT_NONE;
    app->viewport.prompt_len = 0;
    memset(app->viewport.prompt_buffer, 0, sizeof(app->viewport.prompt_buffer));
    app->viewport.current_mouse_world_x = 0;
    app->viewport.current_mouse_world_y = 0;
    app->viewport.mouse_world_x = 0;
    app->viewport.mouse_world_y = 0;
    app->viewport.vim_cursor_x = 0;
    app->viewport.vim_cursor_y = 0;
    app->viewport.command_mode = 0;
    app->viewport.command_len = 0;
    memset(app->viewport.command_buffer, 0, sizeof(app->viewport.command_buffer));
    app->viewport.mode = 'n';
    app->viewport.space_held = false;
    app->viewport.left_click_held = false;
    app->viewport.mod_shift = false;
    app->viewport.scrollbar_drag_v = false;
    app->viewport.scrollbar_drag_h = false;
    app->viewport.help_visible = false;
    app->running = 1;
}

void cleanup_app(App *app) {
    printf("\033[?1006l\033[?1003l");   // disable SGR encoding + any-motion tracking
    fflush(stdout);
    free(app->viewport.sel_buffer);
    app->viewport.sel_buffer = NULL;
    free(app->viewport.sel_colors);
    app->viewport.sel_colors = NULL;
    cleanup_canvas(&app->canvas);
    endwin();
}

void tart_compute_layout(App *app, TartRect *menu, TartRect *ruler, TartRect *tool, TartRect *y_ruler, TartRect *canvas, TartRect *status) {
    int lines = LINES;
    int cols = COLS;
    *menu    = TART_RECT(0, 0, cols, 1);
    *ruler   = TART_RECT(0, 1, cols, 2);
    *tool    = TART_RECT(0, 3, TOOL_PANEL_WIDTH, lines - 4);
    *y_ruler = TART_RECT(TOOL_PANEL_WIDTH, 3, Y_RULER_WIDTH, lines - 4);
    *canvas  = TART_RECT(CANVAS_START_X, 3, CANVAS_VIEW_W, CANVAS_VIEW_H);
    *status  = TART_RECT(0, lines - 2, cols, 1);
}

void tart_draw_at(App *app, int world_x, int world_y, wchar_t ch) {
    if (world_x >= 0 && world_x < app->canvas.width && world_y >= 0 && world_y < app->canvas.height) {
        app->canvas.cells[world_y * app->canvas.width + world_x] = ch;
    }
}

wchar_t tart_read_cell(App *app, int x, int y) {
    if (x >= 0 && x < app->canvas.width && y >= 0 && y < app->canvas.height) {
        return app->canvas.cells[y * app->canvas.width + x];
    }
    return L' ';
}

void tart_set_color(App *app, int x, int y, unsigned char color) {
    if (x >= 0 && x < app->canvas.width && y >= 0 && y < app->canvas.height) {
        app->canvas.colors[y * app->canvas.width + x] = color;
    }
}

unsigned char tart_read_color(App *app, int x, int y) {
    if (x >= 0 && x < app->canvas.width && y >= 0 && y < app->canvas.height) {
        return app->canvas.colors[y * app->canvas.width + x];
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

/* Draws a plain +-|  bordered box outline; caller fills the interior and
 * sets whatever color/attrs it wants around the call. Shared by every popup
 * (help overlay, char palette, color picker) so they can't drift apart. */
void tart_draw_box(int x, int y, int w, int h) {
    mvaddch(y, x, '+');
    mvaddch(y, x + w - 1, '+');
    mvaddch(y + h - 1, x, '+');
    mvaddch(y + h - 1, x + w - 1, '+');
    for (int i = 1; i < w - 1; i++) {
        mvaddch(y, x + i, '-');
        mvaddch(y + h - 1, x + i, '-');
    }
    for (int row = 1; row < h - 1; row++) {
        mvaddch(y + row, x, '|');
        mvaddch(y + row, x + w - 1, '|');
    }
}

/* One axis of a scrollbar: given how much content there is, how much of it
 * is visible, and where the view currently sits, return the thumb's start
 * offset and length along the track (both in track cells). Shared by the
 * vertical/horizontal renderer below and by the click/drag handler in
 * input.c so the two can't disagree about where the thumb is. */
void tart_scrollbar_thumb(int content, int visible, int pos, int track_len, int *out_start, int *out_len) {
    if (content <= visible) {
        *out_start = 0;
        *out_len = track_len;
        return;
    }
    int max_pos = content - visible;
    int clamped = pos < 0 ? 0 : (pos > max_pos ? max_pos : pos);
    int len = track_len * visible / content;
    if (len < 1) len = 1;
    int start = (track_len - len) * clamped / max_pos;
    *out_start = start;
    *out_len = len;
}

/* Inverse of tart_scrollbar_thumb: given a click/drag position on the track,
 * return the content position that should be scrolled to. Centers the thumb
 * under track_pos so dragging tracks the cursor naturally. */
void tart_scrollbar_seek(int content, int visible, int track_pos, int track_len, int *out_pos) {
    int max_pos = content - visible;
    if (max_pos <= 0) { *out_pos = 0; return; }
    int len = track_len * visible / content;
    if (len < 1) len = 1;
    int usable = track_len - len;
    if (usable < 1) usable = 1;
    int centered = track_pos - len / 2;
    if (centered < 0) centered = 0;
    if (centered > usable) centered = usable;
    *out_pos = centered * max_pos / usable;
}

void tart_draw_scrollbars(App *app, TartRect canvas_r) {
    int vstart, vlen, hstart, hlen;
    tart_scrollbar_thumb(app->canvas.height, canvas_r.h, app->viewport.offset_y, canvas_r.h, &vstart, &vlen);
    tart_scrollbar_thumb(app->canvas.width, canvas_r.w, app->viewport.offset_x, canvas_r.w, &hstart, &hlen);

    attron(COLOR_PAIR(1) | A_REVERSE);
    for (int i = 0; i < vlen; i++) {
        mvaddch(canvas_r.y + vstart + i, canvas_r.x + canvas_r.w, '|');
    }
    for (int i = 0; i < hlen; i++) {
        mvaddch(canvas_r.y + canvas_r.h, canvas_r.x + hstart + i, '-');
    }
    attroff(COLOR_PAIR(1) | A_REVERSE);
}

void render(App *app) {
    erase();

    TartRect menu_r, ruler_r, tool_r, y_ruler_r, canvas_r, status_r;
    tart_compute_layout(app, &menu_r, &ruler_r, &tool_r, &y_ruler_r, &canvas_r, &status_r);

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

    attron(COLOR_PAIR(4) | A_DIM);
    int ruler_num_y = ruler_r.y;
    int ruler_tick_y = ruler_r.y + 1;

    for (int wx = ((app->viewport.offset_x / 10) * 10) - 30; wx < app->viewport.offset_x + canvas_r.w + 30; wx += 10) {
        int screen_x = (wx - app->viewport.offset_x) + CANVAS_START_X;
        if (screen_x < CANVAS_START_X) continue;
        if (screen_x < ruler_r.w) {
            mvprintw(ruler_num_y, screen_x, "%d", wx);
        }
    }

    for (int i = CANVAS_START_X; i < ruler_r.w; i++) {
        int world_x = i + app->viewport.offset_x;
        int mod = ((world_x % 10) + 10) % 10;
        char c = (mod == 0) ? '|' : (mod == 5 ? ',' : '.');
        mvaddch(ruler_tick_y, i, c);
    }
    attroff(COLOR_PAIR(4) | A_DIM);

    tart_draw_tool_panel(app, tool_r);

    attron(COLOR_PAIR(4) | A_DIM);
    for (int y = 0; y < y_ruler_r.h; y++) {
        int doc_y = y + app->viewport.offset_y;
        mvprintw(y_ruler_r.y + y, y_ruler_r.x, "%3d ", doc_y);
    }
    attroff(COLOR_PAIR(4) | A_DIM);

    attron(COLOR_PAIR(4));
    for (int x = 0; x < canvas_r.w; x++) {
        mvaddch(canvas_r.y - 1, canvas_r.x + x, '-');
        mvaddch(canvas_r.y + canvas_r.h, canvas_r.x + x, '-');
    }
    for (int y = 0; y < canvas_r.h; y++) {
        mvaddch(canvas_r.y + y, canvas_r.x - 1, '|');
        mvaddch(canvas_r.y + y, canvas_r.x + canvas_r.w, '|');
    }
    mvaddch(canvas_r.y - 1, canvas_r.x - 1, '+');
    mvaddch(canvas_r.y - 1, canvas_r.x + canvas_r.w, '+');
    mvaddch(canvas_r.y + canvas_r.h, canvas_r.x - 1, '+');
    mvaddch(canvas_r.y + canvas_r.h, canvas_r.x + canvas_r.w, '+');
    attroff(COLOR_PAIR(4));

    tart_draw_scrollbars(app, canvas_r);

    for (int dy = 0; dy < canvas_r.h; dy++) {
        for (int dx = 0; dx < canvas_r.w; dx++) {
            int world_x = dx + app->viewport.offset_x;
            int world_y = dy + app->viewport.offset_y;

            wchar_t ch_to_draw;
            attr_t attrs = A_NORMAL;
            unsigned char cell_color = 0;
            bool is_cursor = (world_x == app->viewport.current_mouse_world_x && world_y == app->viewport.current_mouse_world_y);

            if (world_x >= 0 && world_x < app->canvas.width && world_y >= 0 && world_y < app->canvas.height) {
                ch_to_draw = app->canvas.cells[world_y * app->canvas.width + world_x];
                cell_color = app->canvas.colors[world_y * app->canvas.width + world_x];
                if (is_cursor) attrs |= A_REVERSE;
            } else {
                attrs = A_DIM;
                ch_to_draw = (world_x % 10 == 0 && world_y % 5 == 0) ? L'+' : L'.';
                if (is_cursor) attrs |= A_REVERSE;
            }

            if (cell_color != 0) attrs |= COLOR_PAIR(16 + cell_color);
            attron(attrs);
            mvaddnwstr(canvas_r.y + dy, canvas_r.x + dx, ((wchar_t[]){ch_to_draw, 0}), 1);
            attroff(attrs);
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
                        mvaddnwstr(sy, sx, ((wchar_t[]){preview_ch, 0}), 1);
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
                        mvaddnwstr(sy, sx, ((wchar_t[]){tart_read_cell(app, x, y), 0}), 1);
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
                        mvaddnwstr(sy, sx, ((wchar_t[]){app->viewport.sel_buffer[y * app->viewport.sel_w + x], 0}), 1);
                        attroff(A_REVERSE | A_DIM);
                    }
                }
            }
        }
    }

    if (app->viewport.command_mode) {
        tart_draw_command_palette(app, status_r);
    } else if (app->viewport.prompt_mode != PROMPT_NONE) {
        tart_draw_prompt(app, status_r);
    } else {
        const char *mode_badge = (app->viewport.mode == 'n') ? " NORMAL " : " PAN ";
        int cx = status_r.x;

        mvprintw(status_r.y, status_r.x, "%*s", status_r.w, "");

        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(status_r.y, cx, "%s", mode_badge);
        attroff(COLOR_PAIR(3) | A_BOLD);
        cx += (int)strlen(mode_badge) + 1;

        {
            const char *tool_name = (app->viewport.tool == TOOL_PENCIL) ? "pencil" :
                                     (app->viewport.tool == TOOL_ERASER) ? "eraser" :
                                     (app->viewport.tool == TOOL_RECT)   ? "rect"   :
                                     (app->viewport.tool == TOOL_SELECT) ? "select" : "move";
            attron(COLOR_PAIR(2));
            if (app->viewport.tool == TOOL_PENCIL) {
                mvprintw(status_r.y, cx, "tool:%s '%lc'", tool_name, (wint_t)app->viewport.brush_char);
                cx += (int)strlen("tool:") + (int)strlen(tool_name) + 5;
            } else {
                mvprintw(status_r.y, cx, "tool:%s", tool_name);
                cx += (int)strlen("tool:") + (int)strlen(tool_name) + 1;
            }
            attroff(COLOR_PAIR(2));
        }

        attron(COLOR_PAIR(4) | A_DIM);
        mvprintw(status_r.y, cx, "offset[%4d,%4d]", app->viewport.offset_x, app->viewport.offset_y);
        attroff(COLOR_PAIR(4) | A_DIM);
        cx += 18;

        attron(COLOR_PAIR(4) | A_DIM);
        mvprintw(status_r.y, cx, "cursor[%4d,%4d]", app->viewport.vim_cursor_x, app->viewport.vim_cursor_y);
        attroff(COLOR_PAIR(4) | A_DIM);
        cx += 18;

        const char *hint = ": commands   ?/F1 help   q quit";
        int hint_x = status_r.x + status_r.w - (int)strlen(hint) - 1;
        if (hint_x > cx) {
            attron(COLOR_PAIR(4) | A_DIM);
            mvprintw(status_r.y, hint_x, "%s", hint);
            attroff(COLOR_PAIR(4) | A_DIM);
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

    refresh();
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

    rows[n++] = (TartPanelRow){ TP_ROW_PENCIL, -1, -1, PANEL_SELECT_TOOL, TOOL_PENCIL };
    rows[n++] = (TartPanelRow){ TP_ROW_ERASER, -1, -1, PANEL_SELECT_TOOL, TOOL_ERASER };
    rows[n++] = (TartPanelRow){ TP_ROW_RECT,   -1, -1, PANEL_SELECT_TOOL, TOOL_RECT };
    rows[n++] = (TartPanelRow){ TP_ROW_SELECT, -1, -1, PANEL_SELECT_TOOL, TOOL_SELECT };
    rows[n++] = (TartPanelRow){ TP_ROW_MOVE,   -1, -1, PANEL_SELECT_TOOL, TOOL_MOVE };

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
        { TP_ROW_PENCIL, "Pencil", TOOL_PENCIL },
        { TP_ROW_ERASER, "Eraser", TOOL_ERASER },
        { TP_ROW_RECT,   "Rect",   TOOL_RECT },
        { TP_ROW_SELECT, "Select", TOOL_SELECT },
        { TP_ROW_MOVE,   "Move",   TOOL_MOVE },
    };
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        bool sel = (vp->tool == tools[i].tool);
        attr_t a = sel ? (A_REVERSE | A_BOLD) : A_NORMAL;
        attron(a);
        mvprintw(tool_r.y + tools[i].row, tool_r.x + 1, "%-*s", tool_r.w - 2, tools[i].label);
        attroff(a);
        /* Every tool binds to its label's first letter (p/e/r/s/m) - underline
         * it so that binding is visible without opening the help overlay. */
        attron(a | A_UNDERLINE);
        mvprintw(tool_r.y + tools[i].row, tool_r.x + 1, "%c", tools[i].label[0]);
        attroff(a | A_UNDERLINE);
    }

    const char *header = (vp->tool == TOOL_PENCIL) ? "Pencil" :
                          (vp->tool == TOOL_ERASER) ? "Eraser" :
                          (vp->tool == TOOL_RECT)   ? "Rectangle" :
                          (vp->tool == TOOL_SELECT) ? "Select" : "Move";
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(tool_r.y + TP_ROW_OPTIONS_HEADER, tool_r.x + 1, "%s", header);
    attroff(A_BOLD | COLOR_PAIR(1));

    if (vp->tool == TOOL_PENCIL) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Char: '%lc'", (wint_t)vp->brush_char);
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Size:%d  -  +", vp->brush_size);
        mvprintw(tool_r.y + TP_ROW_OPT3, tool_r.x + 1, "Shape: %s", vp->brush_shape == BRUSH_ROUND ? "round" : "square");
    } else if (vp->tool == TOOL_ERASER) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Size:%d  -  +", vp->eraser_size);
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Shape: %s", vp->eraser_shape == BRUSH_ROUND ? "round" : "square");
    } else if (vp->tool == TOOL_RECT) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Fill: %s (f)", vp->rect_filled ? "on" : "off");
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Fill char: '%lc'", (wint_t)vp->fill_char);
    } else if (vp->tool == TOOL_SELECT) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Drag to select");
    } else if (vp->tool == TOOL_MOVE) {
        if (vp->has_selection) mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Buf: %dx%d", vp->sel_w, vp->sel_h);
        else mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "(no selection)");
    }

    mvprintw(tool_r.y + TP_ROW_COLOR, tool_r.x + 1, "Color: ");
    if (vp->current_color != 0) attron(COLOR_PAIR(16 + vp->current_color));
    mvaddch(tool_r.y + TP_ROW_COLOR, tool_r.x + 9, '#');
    mvaddch(tool_r.y + TP_ROW_COLOR, tool_r.x + 10, '#');
    if (vp->current_color != 0) attroff(COLOR_PAIR(16 + vp->current_color));
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
            mvaddnwstr(row_y, cx, ((wchar_t[]){chars[c], 0}), 1);
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

bool tart_resize_canvas(App *app, int new_w, int new_h) {
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;
    if (new_w > MAX_CANVAS_WIDTH) new_w = MAX_CANVAS_WIDTH;
    if (new_h > MAX_CANVAS_HEIGHT) new_h = MAX_CANVAS_HEIGHT;

    wchar_t *new_cells = malloc(sizeof(wchar_t) * (size_t)new_w * new_h);
    if (!new_cells) return false;
    wmemset(new_cells, L' ', (size_t)new_w * new_h);

    unsigned char *new_colors = malloc((size_t)new_w * new_h);
    if (!new_colors) { free(new_cells); return false; }
    memset(new_colors, 0, (size_t)new_w * new_h);

    int copy_w = (new_w < app->canvas.width) ? new_w : app->canvas.width;
    int copy_h = (new_h < app->canvas.height) ? new_h : app->canvas.height;
    for (int y = 0; y < copy_h; y++) {
        wmemcpy(&new_cells[y * new_w], &app->canvas.cells[y * app->canvas.width], (size_t)copy_w);
        memcpy(&new_colors[y * new_w], &app->canvas.colors[y * app->canvas.width], (size_t)copy_w);
    }

    free(app->canvas.cells);
    free(app->canvas.colors);
    app->canvas.cells = new_cells;
    app->canvas.colors = new_colors;
    app->canvas.width = new_w;
    app->canvas.height = new_h;
    return true;
}

bool tart_save_canvas(App *app, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fwide(f, 1);
    fwprintf(f, L"%d %d\n", app->canvas.width, app->canvas.height);
    for (int y = 0; y < app->canvas.height; y++) {
        for (int x = 0; x < app->canvas.width; x++) {
            fputwc(app->canvas.cells[y * app->canvas.width + x], f);
        }
        fputwc(L'\n', f);
    }

    bool any_color = false;
    for (int i = 0; i < app->canvas.width * app->canvas.height && !any_color; i++) {
        if (app->canvas.colors[i] != 0) any_color = true;
    }
    if (any_color) {
        fwprintf(f, L"COLOR\n");
        for (int y = 0; y < app->canvas.height; y++) {
            for (int x = 0; x < app->canvas.width; x++) {
                fwprintf(f, L"%02x", app->canvas.colors[y * app->canvas.width + x]);
            }
            fputwc(L'\n', f);
        }
    }

    fclose(f);
    return true;
}

bool tart_load_canvas(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fwide(f, 1);

    int w, h;
    if (fwscanf(f, L"%d %d", &w, &h) != 2 || w < 1 || h < 1 ||
        w > MAX_CANVAS_WIDTH || h > MAX_CANVAS_HEIGHT) {
        fclose(f);
        return false;
    }
    fgetwc(f); /* consume the newline after the header */

    wchar_t *new_cells = malloc(sizeof(wchar_t) * (size_t)w * h);
    if (!new_cells) { fclose(f); return false; }
    wmemset(new_cells, L' ', (size_t)w * h);

    unsigned char *new_colors = malloc((size_t)w * h);
    if (!new_colors) { free(new_cells); fclose(f); return false; }
    memset(new_colors, 0, (size_t)w * h);

    for (int y = 0; y < h; y++) {
        int x = 0;
        wint_t wc;
        while (x < w && (wc = fgetwc(f)) != WEOF && wc != L'\n') {
            new_cells[y * w + x] = (wchar_t)wc;
            x++;
        }
        if (x == w) {
            wint_t rest;
            while ((rest = fgetwc(f)) != WEOF && rest != L'\n') {}
        }
    }

    /* Optional COLOR block: a marker line followed by w*h 2-hex-digit
     * palette indices, row by row. Older files without it just render at
     * default color (new_colors is already zeroed above). */
    wchar_t marker[8] = {0};
    if (fwscanf(f, L"%7ls", marker) == 1 && wcscmp(marker, L"COLOR") == 0) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                unsigned int val = 0;
                if (fwscanf(f, L"%2x", &val) == 1) new_colors[y * w + x] = (unsigned char)val;
            }
        }
    }
    fclose(f);

    free(app->canvas.cells);
    free(app->canvas.colors);
    app->canvas.cells = new_cells;
    app->canvas.colors = new_colors;
    app->canvas.width = w;
    app->canvas.height = h;
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
        "View    v toggle view/pan   wheel scroll (shift = horizontal)   Home reset",
        "Scroll  drag the bar on the canvas' right/bottom edge to jump the view",
        "Tools   p pencil   e eraser   r rect   s select   m move (underlined letter)",
        "Brush   t open tip palette (1-9 jump category)   [ ] size (1-21)   Tab shape",
        "        f rect fill toggle",
        "Draw    left-click hold to use tool   space+move = alt draw/pan",
        "Color   -/= cycle 8 quick colors   click Color row for full HSL picker",
        "Image   Image menu > Resize Canvas (WxH)",
        "File    File menu > Save/Load, or :save <name> / :load <name>",
        "Menu    Alt+F/E/V/I/H or click to open, arrows/Left-Right, Enter, Esc closes",
        "Cmd     : or Ctrl+E, fuzzy-searches as you type, Up/Down or wheel to pick,",
        "          Enter or click a row to run (quit, help, deselect, reset-view,",
        "          toggle-pan, resize, save, load)",
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

#endif