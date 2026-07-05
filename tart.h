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
void show_help();

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
    app->running = 1;
}

void cleanup_app(App *app) {
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

    const char *mode_name = (app->viewport.mode == 'n') ? "normal" :
                            (app->viewport.mode == 'c') ? "command" : "pan";
    mvprintw(status_r.y, status_r.x,
        "Mouse[%d,%d] Cursor[%d,%d] Offset[%d,%d] MODE[%s] | Brush:'%c' | hjkl/0$wbGg | F1 help",
        app->viewport.mouse_world_x, app->viewport.mouse_world_y,
        app->viewport.vim_cursor_x, app->viewport.vim_cursor_y,
        app->viewport.offset_x, app->viewport.offset_y,
        mode_name,
        app->viewport.current_brush);

    if (app->viewport.command_mode) {
        mvprintw(LINES-1, 0, "> %s", app->viewport.command_buffer);
    }

    refresh();
}

#endif
