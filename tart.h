#ifndef TART_H
#define TART_H

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CANVAS_WIDTH  512
#define MAX_CANVAS_HEIGHT 256

// Configuration macros - define these before including tart.h to customize
#ifndef TART_DEFAULT_CANVAS_WIDTH
#define TART_DEFAULT_CANVAS_WIDTH 120
#endif
#ifndef TART_DEFAULT_CANVAS_HEIGHT
#define TART_DEFAULT_CANVAS_HEIGHT 60
#endif

// Declarative helper macros (Clay-inspired for clean UI definition)
#define TART_RECT(x_, y_, w_, h_) ((TartRect){ .x = (x_), .y = (y_), .w = (w_), .h = (h_) })

typedef struct {
    int width;
    int height;
    char *cells; // flat array of characters (row-major)
    // Later: attr_t *attrs; for colors / styles
} Canvas;

typedef struct {
    Canvas *canvas;
    int offset_x;
    int offset_y;
    int cursor_doc_x;
    int cursor_doc_y;
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

/* Function declarations */
void init_app(App *app);
void cleanup_app(App *app);

void init_canvas(Canvas *c, int w, int h);
void cleanup_canvas(Canvas *c);

void render(App *app);
void handle_input(App *app);

/* Layout calculation - computes positions for all UI regions */
void tart_compute_layout(App *app, TartRect *menu, TartRect *ruler_x, TartRect *left, TartRect *canvas, TartRect *status);

#endif /* TART_H */

#ifdef TART_IMPLEMENTATION

void init_canvas(Canvas *c, int w, int h) {
    c->width = w;
    c->height = h;
    c->cells = malloc((size_t)w * h);
    if (c->cells) {
        memset(c->cells, ' ', (size_t)w * h);
    }
}

void cleanup_canvas(Canvas *c) {
    if (c->cells) {
        free(c->cells);
        c->cells = NULL;
    }
}

void init_app(App *app) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);

    init_canvas(&app->canvas, TART_DEFAULT_CANVAS_WIDTH, TART_DEFAULT_CANVAS_HEIGHT);
    app->viewport.canvas = &app->canvas;
    app->viewport.offset_x = 0;
    app->viewport.offset_y = 0;
    app->viewport.cursor_doc_x = 0;
    app->viewport.cursor_doc_y = 0;
    app->running = 1;
}

void cleanup_app(App *app) {
    cleanup_canvas(&app->canvas);
    endwin();
}

void tart_compute_layout(App *app, TartRect *menu, TartRect *ruler_x, TartRect *left, TartRect *canvas, TartRect *status) {
    int lines = LINES;
    int cols = COLS;

    // Menu bar - fixed top, full width, height 1
    *menu = TART_RECT(0, 0, cols, 1);

    // Top ruler (X) - below menu, full width, height 1
    *ruler_x = TART_RECT(0, 1, cols, 1);

    // Left tool/ruler area - fixed width, below rulers, above status
    int left_width = 6;
    *left = TART_RECT(0, 2, left_width, lines - 3);

    // Main canvas/workspace area - grows to fill remaining space
    *canvas = TART_RECT(left_width, 2, cols - left_width, lines - 3);

    // Status bar - fixed bottom, full width, height 1
    *status = TART_RECT(0, lines - 1, cols, 1);
}

void render(App *app) {
    erase();

    TartRect menu_r, ruler_r, left_r, canvas_r, status_r;
    tart_compute_layout(app, &menu_r, &ruler_r, &left_r, &canvas_r, &status_r);

    /* === Top Menu Bar === */
    attron(A_REVERSE);
    mvprintw(menu_r.y, menu_r.x, " File  Edit  View  Layer  Window  Help ");
    attroff(A_REVERSE);

    /* === Top Ruler (X) === */
    mvprintw(ruler_r.y, 4, "0");
    for (int x = 10; x <= 110 && x < ruler_r.w; x += 10) {
        mvprintw(ruler_r.y, x + 1, "%3d", x);
    }

    /* === Left Tool / Ruler Area === */
    attron(A_BOLD);
    mvprintw(left_r.y + 1, left_r.x + 1, "curr");
    mvprintw(left_r.y + 2, left_r.x + 1, "tool");
    attroff(A_BOLD);

    for (int y = 0; y < left_r.h; y++) {
        int doc_y = y + app->viewport.offset_y;
        mvprintw(left_r.y + y, left_r.x, "%3d|", doc_y);
    }

    /* === Main Canvas / Workspace === */
    // Workspace border
    attron(COLOR_PAIR(1) | A_DIM);
    for (int y = 0; y < canvas_r.h; y++) {
        mvaddch(canvas_r.y + y, canvas_r.x - 1, '|');
    }
    attroff(COLOR_PAIR(1) | A_DIM);

    // Grid content - now sized from computed canvas rect, respects offset
    for (int dy = 0; dy < canvas_r.h && (canvas_r.y + dy) < LINES - 1; dy++) {
        for (int dx = 0; dx < canvas_r.w && (canvas_r.x + dx) < COLS; dx++) {
            int world_x = dx + app->viewport.offset_x;
            int world_y = dy + app->viewport.offset_y;
            char ch = (world_x % 10 == 0 && world_y % 5 == 0) ? '+' : '.';
            mvaddch(canvas_r.y + dy, canvas_r.x + dx, ch);
        }
    }

    /* === Status Bar === */
    mvprintw(status_r.y, status_r.x, 
        "Cursor: (%d, %d)  |  Offset: (%d, %d)  |  q=quit  Arrows to pan",
        app->viewport.cursor_doc_x, app->viewport.cursor_doc_y,
        app->viewport.offset_x, app->viewport.offset_y);

    refresh();
}

void handle_input(App *app) {
    int ch = getch();
    if (ch == 'q' || ch == 'Q') {
        app->running = 0;
    } else if (ch == KEY_LEFT) {
        app->viewport.offset_x -= 5;
        if (app->viewport.offset_x < 0) app->viewport.offset_x = 0;
    } else if (ch == KEY_RIGHT) {
        app->viewport.offset_x += 5;
    } else if (ch == KEY_UP) {
        app->viewport.offset_y -= 3;
        if (app->viewport.offset_y < 0) app->viewport.offset_y = 0;
    } else if (ch == KEY_DOWN) {
        app->viewport.offset_y += 3;
    } else if (ch == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK) {
            // TODO: use computed canvas rect for accurate mapping
            app->viewport.cursor_doc_x = event.x + app->viewport.offset_x;
            app->viewport.cursor_doc_y = event.y + app->viewport.offset_y;
        }
    }
}

#endif /* TART_IMPLEMENTATION */
