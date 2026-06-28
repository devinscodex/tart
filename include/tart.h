#ifndef TART_H
#define TART_H

#include <ncurses.h>

#define MAX_CANVAS_WIDTH  512
#define MAX_CANVAS_HEIGHT 256

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

#endif