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

#define TOOL_PANEL_WIDTH 10
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
};
#define TART_COMMAND_COUNT (sizeof(TART_COMMANDS) / sizeof(TART_COMMANDS[0]))

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
    char current_brush;
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
    app->viewport.current_brush = '#';
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

void render(App *app) {
    erase();

    TartRect menu_r, ruler_r, tool_r, y_ruler_r, canvas_r, status_r;
    tart_compute_layout(app, &menu_r, &ruler_r, &tool_r, &y_ruler_r, &canvas_r, &status_r);

    attron(COLOR_PAIR(3) | A_REVERSE);
    mvprintw(menu_r.y, menu_r.x, "%-*s", menu_r.w, " File  Edit  View  Layer  Window  Help");
    attroff(COLOR_PAIR(3) | A_REVERSE);

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

    attron(A_BOLD);
    mvprintw(tool_r.y + 1, tool_r.x + 1, "Tools");
    mvprintw(tool_r.y + 3, tool_r.x + 1, "Pencil");
    mvprintw(tool_r.y + 4, tool_r.x + 1, "Eraser");
    mvprintw(tool_r.y + 6, tool_r.x + 1, "Brush:");
    mvprintw(tool_r.y + 7, tool_r.x + 1, "  '%c'", app->viewport.current_brush);
    attroff(A_BOLD);

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

        attron(COLOR_PAIR(2));
        mvprintw(status_r.y, cx, "brush '%c'", app->viewport.current_brush);
        attroff(COLOR_PAIR(2));
        cx += (int)strlen("brush 'X'") + 1;

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

    if (app->viewport.help_visible) {
        tart_draw_help_overlay(app);
    }

    refresh();
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
        "View    v toggle view/pan   wheel scroll   Home reset   End jump to content",
        "Draw    left-click hold = pencil   space+move = alt draw/pan   p/e pencil/eraser",
        "Cmd     : open command palette (quit, help)",
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