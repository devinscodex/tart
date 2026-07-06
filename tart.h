#ifndef TART_H
#define TART_H

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

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

#define TART_RECT(x_, y_, w_, h_) ((TartRect){ .x = (x_), .y = (y_), .w = (w_), .h = (h_) })

#define CMD_PALETTE_MAX_VISIBLE 6

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
};
#define TART_COMMAND_COUNT (sizeof(TART_COMMANDS) / sizeof(TART_COMMANDS[0]))

typedef struct {
    const char *label;
    const char *items[3];
    int item_count;
} TartMenuDef;

static const TartMenuDef TART_MENUS[] = {
    { "File", { "quit" },                 1 },
    { "Edit", { "deselect" },              1 },
    { "View", { "reset-view", "toggle-pan" }, 2 },
    { "Help", { "help" },                  1 },
};
#define TART_MENU_COUNT (sizeof(TART_MENUS) / sizeof(TART_MENUS[0]))

typedef struct {
    const char *label;
    int x0, x1;
} TartMenuBarItem;

typedef enum { TOOL_PENCIL, TOOL_ERASER, TOOL_RECT, TOOL_SELECT, TOOL_MOVE } TartTool;
typedef enum { BRUSH_SQUARE, BRUSH_ROUND } BrushShape;

typedef enum {
    PANEL_NONE,
    PANEL_SELECT_TOOL,
    PANEL_CHAR_CYCLE,
    PANEL_SIZE_DEC,
    PANEL_SIZE_INC,
    PANEL_SHAPE_TOGGLE,
    PANEL_RECT_FILL_TOGGLE,
} TartPanelAction;

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
#define TP_SIZE_DEC_X         9
#define TP_SIZE_INC_X         12

typedef struct {
    int width;
    int height;
    char *cells;
} Canvas;

typedef struct {
    Canvas *canvas;
    int offset_x;
    int offset_y;
    int cursor_doc_x;
    int cursor_doc_y;
    int last_mouse_screen_x;
    int last_mouse_screen_y;
    int is_dragging;
    TartTool tool;
    char brush_char;
    int brush_size;
    BrushShape brush_shape;
    int eraser_size;
    BrushShape eraser_shape;
    bool rect_filled;
    bool tool_dragging;
    int drag_start_wx, drag_start_wy;
    int drag_cur_wx, drag_cur_wy;
    bool has_selection;
    int sel_w, sel_h;
    char *sel_buffer;
    int open_menu;
    int menu_hover_item;
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
    bool left_click_held;
    bool mid_click_held;
    bool right_click_held;
    bool mod_shift;
    bool mod_ctrl;
    bool mod_alt;
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
void tart_draw_at(App *app, int world_x, int world_y, char ch);
void tart_stamp(App *app, int cx, int cy, int size, BrushShape shape, char ch);
char tart_read_cell(App *app, int x, int y);
int tart_build_tool_panel(App *app, TartPanelRow rows[TOOL_PANEL_MAX_ROWS]);
int tart_build_menu_bar(App *app, TartMenuBarItem items[TART_MENU_COUNT]);
void tart_draw_tool_panel(App *app, TartRect tool_r);
void tart_draw_menu_dropdown(App *app, TartRect menu_r);
void tart_draw_command_palette(App *app, TartRect status);
void tart_draw_help_overlay(App *app);

#endif

#ifdef TART_IMPLEMENTATION

void init_canvas(Canvas *c, int w, int h) {
    c->width = w;
    c->height = h;
    c->cells = malloc((size_t)w * h);
    if (c->cells) memset(c->cells, ' ', (size_t)w * h);
}

void cleanup_canvas(Canvas *c) {
    if (c->cells) { free(c->cells); c->cells = NULL; }
}

void init_app(App *app) {
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
    app->viewport.brush_char = '#';
    app->viewport.brush_size = 1;
    app->viewport.brush_shape = BRUSH_SQUARE;
    app->viewport.eraser_size = 1;
    app->viewport.eraser_shape = BRUSH_SQUARE;
    app->viewport.rect_filled = false;
    app->viewport.tool_dragging = false;
    app->viewport.drag_start_wx = 0;
    app->viewport.drag_start_wy = 0;
    app->viewport.drag_cur_wx = 0;
    app->viewport.drag_cur_wy = 0;
    app->viewport.has_selection = false;
    app->viewport.sel_w = 0;
    app->viewport.sel_h = 0;
    app->viewport.sel_buffer = NULL;
    app->viewport.open_menu = -1;
    app->viewport.menu_hover_item = 0;
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
    app->viewport.mid_click_held = false;
    app->viewport.right_click_held = false;
    app->viewport.mod_shift = false;
    app->viewport.mod_ctrl = false;
    app->viewport.mod_alt = false;
    app->viewport.help_visible = false;
    app->running = 1;
}

void cleanup_app(App *app) {
    printf("\033[?1006l\033[?1003l");   // disable SGR encoding + any-motion tracking
    fflush(stdout);
    free(app->viewport.sel_buffer);
    app->viewport.sel_buffer = NULL;
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
    *canvas  = TART_RECT(CANVAS_START_X, 3, cols - CANVAS_START_X, lines - 4);
    *status  = TART_RECT(0, lines - 2, cols, 1);
}

void tart_draw_at(App *app, int world_x, int world_y, char ch) {
    if (world_x >= 0 && world_x < app->canvas.width && world_y >= 0 && world_y < app->canvas.height) {
        app->canvas.cells[world_y * app->canvas.width + world_x] = ch;
    }
}

char tart_read_cell(App *app, int x, int y) {
    if (x >= 0 && x < app->canvas.width && y >= 0 && y < app->canvas.height) {
        return app->canvas.cells[y * app->canvas.width + x];
    }
    return ' ';
}

void tart_stamp(App *app, int cx, int cy, int size, BrushShape shape, char ch) {
    int r = (size - 1) / 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (shape == BRUSH_ROUND && (dx * dx + dy * dy) > r * r) continue;
            tart_draw_at(app, cx + dx, cy + dy, ch);
        }
    }
}

void render(App *app) {
    erase();

    TartRect menu_r, ruler_r, tool_r, y_ruler_r, canvas_r, status_r;
    tart_compute_layout(app, &menu_r, &ruler_r, &tool_r, &y_ruler_r, &canvas_r, &status_r);

    attron(COLOR_PAIR(3) | A_REVERSE);
    mvprintw(menu_r.y, menu_r.x, "%-*s", menu_r.w, "");
    attroff(COLOR_PAIR(3) | A_REVERSE);
    {
        TartMenuBarItem mitems[TART_MENU_COUNT];
        tart_build_menu_bar(app, mitems);
        for (size_t i = 0; i < TART_MENU_COUNT; i++) {
            bool open = (app->viewport.open_menu == (int)i);
            attr_t a = COLOR_PAIR(3) | A_REVERSE | (open ? A_BOLD : A_NORMAL);
            attron(a);
            mvprintw(menu_r.y, mitems[i].x0, "%s", mitems[i].label);
            attroff(a);
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

    for (int dy = 0; dy < canvas_r.h; dy++) {
        for (int dx = 0; dx < canvas_r.w; dx++) {
            int world_x = dx + app->viewport.offset_x;
            int world_y = dy + app->viewport.offset_y;

            char ch_to_draw;
            attr_t attrs = A_NORMAL;
            bool is_cursor = (world_x == app->viewport.current_mouse_world_x && world_y == app->viewport.current_mouse_world_y);

            if (world_x >= 0 && world_x < app->canvas.width && world_y >= 0 && world_y < app->canvas.height) {
                ch_to_draw = app->canvas.cells[world_y * app->canvas.width + world_x];
                if (is_cursor) attrs |= A_REVERSE;
            } else {
                attrs = A_DIM;
                ch_to_draw = (world_x % 10 == 0 && world_y % 5 == 0) ? '+' : '.';
                if (is_cursor) attrs |= A_REVERSE;
            }

            attron(attrs);
            mvaddch(canvas_r.y + dy, canvas_r.x + dx, ch_to_draw);
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
                        attron(A_DIM | COLOR_PAIR(1));
                        mvaddch(sy, sx, app->viewport.brush_char);
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
                        mvaddch(sy, sx, tart_read_cell(app, x, y));
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
                        mvaddch(sy, sx, app->viewport.sel_buffer[y * app->viewport.sel_w + x]);
                        attroff(A_REVERSE | A_DIM);
                    }
                }
            }
        }
    }

    if (app->viewport.command_mode) {
        tart_draw_command_palette(app, status_r);
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
                mvprintw(status_r.y, cx, "tool:%s '%c'", tool_name, app->viewport.brush_char);
                cx += (int)strlen("tool:") + (int)strlen(tool_name) + 5;
            } else {
                mvprintw(status_r.y, cx, "tool:%s", tool_name);
                cx += (int)strlen("tool:") + (int)strlen(tool_name) + 1;
            }
            attroff(COLOR_PAIR(2));
        }

        attron(COLOR_PAIR(4) | A_DIM);
        mvprintw(status_r.y, cx, "mouse[%4d,%4d]", app->viewport.mouse_world_x, app->viewport.mouse_world_y);
        attroff(COLOR_PAIR(4) | A_DIM);
        cx += 17;

        attron(COLOR_PAIR(4) | A_DIM);
        mvprintw(status_r.y, cx, "cursor[%4d,%4d]", app->viewport.vim_cursor_x, app->viewport.vim_cursor_y);
        attroff(COLOR_PAIR(4) | A_DIM);
        cx += 18;

        {
            bool L = app->viewport.left_click_held;
            bool M = app->viewport.mid_click_held;
            bool R = app->viewport.right_click_held;
            mvprintw(status_r.y, cx, "btn[");
            attron(L ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 4, "%c", L ? 'L' : 'l');
            attroff(L ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            attron(M ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 5, "%c", M ? 'M' : 'm');
            attroff(M ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            attron(R ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 6, "%c", R ? 'R' : 'r');
            attroff(R ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 7, "]");
            cx += 9;
        }

        {
            bool S = app->viewport.mod_shift;
            bool C = app->viewport.mod_ctrl;
            bool A = app->viewport.mod_alt;
            mvprintw(status_r.y, cx, "mod[");
            attron(S ? (COLOR_PAIR(1) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 4, "%c", S ? 'S' : 's');
            attroff(S ? (COLOR_PAIR(1) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            attron(C ? (COLOR_PAIR(1) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 5, "%c", C ? 'C' : 'c');
            attroff(C ? (COLOR_PAIR(1) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            attron(A ? (COLOR_PAIR(1) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 6, "%c", A ? 'A' : 'a');
            attroff(A ? (COLOR_PAIR(1) | A_BOLD) : (COLOR_PAIR(4) | A_DIM));
            mvprintw(status_r.y, cx + 7, "]");
            cx += 9;
        }

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

int tart_build_tool_panel(App *app, TartPanelRow rows[TOOL_PANEL_MAX_ROWS]) {
    Viewport *vp = &app->viewport;
    int n = 0;

    rows[n++] = (TartPanelRow){ TP_ROW_PENCIL, -1, -1, PANEL_SELECT_TOOL, TOOL_PENCIL };
    rows[n++] = (TartPanelRow){ TP_ROW_ERASER, -1, -1, PANEL_SELECT_TOOL, TOOL_ERASER };
    rows[n++] = (TartPanelRow){ TP_ROW_RECT,   -1, -1, PANEL_SELECT_TOOL, TOOL_RECT };
    rows[n++] = (TartPanelRow){ TP_ROW_SELECT, -1, -1, PANEL_SELECT_TOOL, TOOL_SELECT };
    rows[n++] = (TartPanelRow){ TP_ROW_MOVE,   -1, -1, PANEL_SELECT_TOOL, TOOL_MOVE };

    if (vp->tool == TOOL_PENCIL) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, -1, -1, PANEL_CHAR_CYCLE, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, TP_SIZE_DEC_X, TP_SIZE_DEC_X, PANEL_SIZE_DEC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, TP_SIZE_INC_X, TP_SIZE_INC_X, PANEL_SIZE_INC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT3, -1, -1, PANEL_SHAPE_TOGGLE, 0 };
    } else if (vp->tool == TOOL_ERASER) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, TP_SIZE_DEC_X, TP_SIZE_DEC_X, PANEL_SIZE_DEC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, TP_SIZE_INC_X, TP_SIZE_INC_X, PANEL_SIZE_INC, 0 };
        rows[n++] = (TartPanelRow){ TP_ROW_OPT2, -1, -1, PANEL_SHAPE_TOGGLE, 0 };
    } else if (vp->tool == TOOL_RECT) {
        rows[n++] = (TartPanelRow){ TP_ROW_OPT1, -1, -1, PANEL_RECT_FILL_TOGGLE, 0 };
    }
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
        if (sel) attron(A_REVERSE | A_BOLD);
        mvprintw(tool_r.y + tools[i].row, tool_r.x + 1, "%-*s", tool_r.w - 2, tools[i].label);
        if (sel) attroff(A_REVERSE | A_BOLD);
    }

    const char *header = (vp->tool == TOOL_PENCIL) ? "Pencil" :
                          (vp->tool == TOOL_ERASER) ? "Eraser" :
                          (vp->tool == TOOL_RECT)   ? "Rectangle" :
                          (vp->tool == TOOL_SELECT) ? "Select" : "Move";
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(tool_r.y + TP_ROW_OPTIONS_HEADER, tool_r.x + 1, "%s", header);
    attroff(A_BOLD | COLOR_PAIR(1));

    if (vp->tool == TOOL_PENCIL) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Char: '%c'", vp->brush_char);
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Size:%d  -  +", vp->brush_size);
        mvprintw(tool_r.y + TP_ROW_OPT3, tool_r.x + 1, "Shape: %s", vp->brush_shape == BRUSH_ROUND ? "round" : "square");
    } else if (vp->tool == TOOL_ERASER) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Size:%d  -  +", vp->eraser_size);
        mvprintw(tool_r.y + TP_ROW_OPT2, tool_r.x + 1, "Shape: %s", vp->eraser_shape == BRUSH_ROUND ? "round" : "square");
    } else if (vp->tool == TOOL_RECT) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Fill: %s (f)", vp->rect_filled ? "on" : "off");
    } else if (vp->tool == TOOL_SELECT) {
        mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Drag to select");
    } else if (vp->tool == TOOL_MOVE) {
        if (vp->has_selection) mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "Buf: %dx%d", vp->sel_w, vp->sel_h);
        else mvprintw(tool_r.y + TP_ROW_OPT1, tool_r.x + 1, "(no selection)");
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

    attron(COLOR_PAIR(2));
    for (int i = 0; i < menu->item_count; i++) {
        const TartCommand *c = tart_find_command(menu->items[i]);
        const char *label = c ? c->desc : menu->items[i];
        mvprintw(y + i, x, "%*s", w, "");
        if (i == vp->menu_hover_item) attron(A_REVERSE);
        mvprintw(y + i, x, " %-*s", w - 1, label);
        if (i == vp->menu_hover_item) attroff(A_REVERSE);
    }
    attroff(COLOR_PAIR(2));
}

void tart_draw_command_palette(App *app, TartRect status_r) {
    const char *buf = app->viewport.command_buffer;
    size_t len = strlen(buf);

    int match_idx[TART_COMMAND_COUNT];
    int match_count = 0;
    for (size_t i = 0; i < TART_COMMAND_COUNT && match_count < CMD_PALETTE_MAX_VISIBLE; i++) {
        const TartCommand *c = &TART_COMMANDS[i];
        if (len == 0 ||
            strncmp(c->name, buf, len) == 0 ||
            (c->alias && strncmp(c->alias, buf, len) == 0)) {
            match_idx[match_count++] = (int)i;
        }
    }

    int popup_h = match_count + 1;
    int popup_y = status_r.y - popup_h;
    if (popup_y < 0) popup_y = 0;

    attron(COLOR_PAIR(4) | A_DIM);
    mvhline(popup_y, 0, '-', COLS);
    attroff(COLOR_PAIR(4) | A_DIM);

    for (int i = 0; i < match_count; i++) {
        const TartCommand *c = &TART_COMMANDS[match_idx[i]];
        int row = popup_y + 1 + i;
        mvprintw(row, 0, "%*s", COLS, "");
        if (i == 0) attron(A_REVERSE);
        mvprintw(row, 2, "%-10s %s", c->name, c->desc);
        if (i == 0) attroff(A_REVERSE);
    }

    mvprintw(status_r.y, status_r.x, "%*s", status_r.w, "");
    attron(A_BOLD);
    mvprintw(status_r.y, status_r.x, ":%s", buf);
    attroff(A_BOLD);
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
        "Tools   p pencil   e eraser   r rect   s select   m move",
        "Brush   t cycle tip char   [ ] size   Tab shape   f rect fill toggle",
        "Draw    left-click hold to use tool   space+move = alt draw/pan",
        "Menu    click File/Edit/View/Help, or arrows+Enter, Esc closes",
        "Cmd     : open command palette (quit, help, deselect, reset-view, toggle-pan)",
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
    for (int i = 0; i < n; i++) {
        mvprintw(y + 1 + i, x + 2, "%s", lines[i]);
    }
    attroff(COLOR_PAIR(2));
}

#endif