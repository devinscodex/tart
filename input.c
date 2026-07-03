#include "tart.h"

static void apply_tool(App *app, int doc_x, int doc_y) {
    if (doc_x < 0 || doc_x >= app->canvas.width ||
        doc_y < 0 || doc_y >= app->canvas.height) return;

    char *cell = &app->canvas.cells[doc_y * app->canvas.width + doc_x];
    switch (app->active_tool) {
        case TOOL_PENCIL: *cell = '#'; break;
        case TOOL_ERASER: *cell = ' '; break;
        case TOOL_SELECT: /* TODO */ break;
        case TOOL_MOVE:   /* TODO */ break;
        case TOOL_BRUSH:  *cell = 'o'; break;
        default: break;
    }
}

void handle_input(App *app) {
    int ch = getch();

    if (ch == 'q' || ch == 'Q') {
        app->running = 0;
        return;
    }

    /* quick tool switch */
    if (ch >= '1' && ch <= '5') {
        app->active_tool = (Tool)(ch - '1');
        return;
    }

    /* arrow / vim keys for panning */
    if (ch == KEY_UP || ch == 'k')    { app->viewport.offset_y--; return; }
    if (ch == KEY_DOWN || ch == 'j')  { app->viewport.offset_y++; return; }
    if (ch == KEY_LEFT || ch == 'h')  { app->viewport.offset_x--; return; }
    if (ch == KEY_RIGHT || ch == 'l') { app->viewport.offset_x++; return; }

    if (ch == KEY_MOUSE) {
        MEVENT ev;
        if (getmouse(&ev) != OK) return;

        int sw = app->sidebar_width;
        int main_x = sw + 1;
        int y_ruler_w = 6;
        int canvas_x = main_x + y_ruler_w;
        int content_y = 3;

        int dx = ev.x - app->last_mouse_x;
        int dy = ev.y - app->last_mouse_y;
        app->last_mouse_x = ev.x;
        app->last_mouse_y = ev.y;

        /* map to doc coords */
        int doc_x = ev.x - canvas_x + app->viewport.offset_x;
        int doc_y = ev.y - content_y + app->viewport.offset_y;
        if (ev.y >= content_y && ev.x >= canvas_x) {
            app->viewport.cursor_doc_x = doc_x;
            app->viewport.cursor_doc_y = doc_y;
        }

        /* sidebar tool selection */
        if (ev.x < sw && ev.y >= 3 && (ev.bstate & BUTTON1_PRESSED)) {
            int tool_idx = ev.y - 3;
            if (tool_idx >= 0 && tool_idx < TOOL_COUNT) {
                app->active_tool = (Tool)tool_idx;
            }
            return;
        }

        /* sidebar resize on separator */
        if (ev.x == sw && (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
            app->dragging_sidebar = true;
        }
        if (app->dragging_sidebar) {
            if (ev.bstate & BUTTON1_RELEASED) {
                app->dragging_sidebar = false;
            } else {
                app->sidebar_width = ev.x;
                if (app->sidebar_width < 18) app->sidebar_width = 18;
                if (app->sidebar_width > COLS / 2) app->sidebar_width = COLS / 2;
            }
            return;
        }

        /* drawing in workspace */
        bool in_canvas = (ev.y >= content_y && ev.x >= canvas_x);
        if (in_canvas && !app->dragging_sidebar) {
            if (ev.bstate & BUTTON1_PRESSED) {
                app->mouse_down = true;
                apply_tool(app, doc_x, doc_y);
            }
            if (ev.bstate & BUTTON1_RELEASED) {
                app->mouse_down = false;
            }
            if (app->mouse_down && (ev.bstate & REPORT_MOUSE_POSITION)) {
                apply_tool(app, doc_x, doc_y);
            }
        }

        /* middle mouse pan */
        if (ev.bstate & BUTTON2_PRESSED && (dx != 0 || dy != 0)) {
            app->viewport.offset_x -= dx;
            app->viewport.offset_y -= dy;
        }
    }
}