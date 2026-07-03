#ifndef TART_H
#define TART_H

#include <ncurses.h>
#include <stdbool.h>

#define MAX_CANVAS_WIDTH  512
#define MAX_CANVAS_HEIGHT 256

typedef enum {
    TOOL_PENCIL = 0,
    TOOL_ERASER,
    TOOL_SELECT,
    TOOL_MOVE,
    TOOL_BRUSH,
    TOOL_COUNT
} Tool;

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
} Viewport;

typedef struct {
    Canvas canvas;
    Viewport viewport;
    int running;
    Tool active_tool;
    int sidebar_width;
    bool dragging_sidebar;
    bool mouse_down;
    int last_mouse_x;
    int last_mouse_y;
} App;

/* Function declarations */
void init_app(App *app);
void cleanup_app(App *app);

void init_canvas(Canvas *c, int w, int h);
void cleanup_canvas(Canvas *c);

void render(App *app);
void handle_input(App *app);
const char* tool_name(Tool t);

#endif