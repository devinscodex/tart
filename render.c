#include "tart.h"
#include <stdio.h>

const char* tool_name(Tool t) {
    switch (t) {
        case TOOL_PENCIL: return "Pencil";
        case TOOL_ERASER: return "Eraser";
        case TOOL_SELECT: return "Select";
        case TOOL_MOVE:   return "Move";
        case TOOL_BRUSH:  return "Brush";
        default:          return "?";
    }
}

void render(App *app) {
    clear();

    int sw = app->sidebar_width;
    int main_x = sw + 1;
    int y_ruler_w = 6;
    int canvas_x = main_x + y_ruler_w;
    int content_y = 3;

    int doc_w = app->canvas.width;   // 80
    int doc_h = app->canvas.height;  // 25

    // padded workspace for rulers (25% rounded up to nearest 10)
    int pad_x = ((doc_w * 25 + 99) / 100 + 9) / 10 * 10;
    int pad_y = ((doc_h * 25 + 99) / 100 + 9) / 10 * 10;
    int ruler_max_x = doc_w + pad_x;
    int ruler_max_y = doc_h + pad_y;

    /* === Row 0: Full-width top menu bar === */
    attron(A_REVERSE | COLOR_PAIR(2));
    for (int x = 0; x < COLS; x++) mvaddch(0, x, ' ');
    mvprintw(0, 2, "File  Edit  View  Layer  Window  Help");
    mvprintw(0, COLS - 30, "TART  |  %s  |  %dx%d", tool_name(app->active_tool), doc_w, doc_h);
    attroff(A_REVERSE | COLOR_PAIR(2));

    /* === Row 1: Split header bar === */
    attron(A_BOLD | COLOR_PAIR(3));
    mvprintw(1, 0, "%-*.*s", sw, sw, " Tools");
    mvaddch(1, sw, ACS_VLINE);
    mvprintw(1, sw + 2, "untitled.tart   [new empty file]");
    attroff(A_BOLD | COLOR_PAIR(3));

    /* === Row 2: X ruler (padded workspace) === */
    attron(COLOR_PAIR(4));
    mvprintw(2, canvas_x, "0");
    for (int x = 10; x <= ruler_max_x && (canvas_x + x) < COLS; x += 10) {
        mvprintw(2, canvas_x + x, "%d", x);
    }
    attroff(COLOR_PAIR(4));

    /* === Sidebar tool list (clickable, highlighted active) === */
    int ty = 3;
    const char* tools[TOOL_COUNT] = {"Pencil", "Eraser", "Select", "Move", "Brush"};
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (app->active_tool == (Tool)i) attron(A_REVERSE);
        mvprintw(ty + i, 2, "%s", tools[i]);
        if (app->active_tool == (Tool)i) attroff(A_REVERSE);
    }

    /* === Y ruler + blank canvas content === */
    int visible_h = LINES - content_y - 2;
    int visible_w = COLS - canvas_x - 1;

    for (int dy = 0; dy < visible_h; dy++) {
        int wy = dy + app->viewport.offset_y;
        int sy = content_y + dy;
        if (sy >= LINES - 1) break;

        if (wy >= -pad_y && wy < ruler_max_y) {
            mvprintw(sy, main_x, "%4d|", wy);
        } else {
            mvprintw(sy, main_x, "    |");
        }

        for (int dx = 0; dx < visible_w; dx++) {
            int wx = dx + app->viewport.offset_x;
            int sx = canvas_x + dx;
            if (sx >= COLS) break;

            char ch = ' ';  // blank canvas space (margin + empty doc)
            if (wy >= 0 && wy < doc_h && wx >= 0 && wx < doc_w) {
                ch = app->canvas.cells[wy * doc_w + wx];
            }
            mvaddch(sy, sx, ch);
        }
    }

    /* === Sidebar/main separator === */
    attron(COLOR_PAIR(3) | A_DIM);
    for (int y = 2; y < LINES - 1; y++) {
        mvaddch(y, sw, ACS_VLINE);
    }
    attroff(COLOR_PAIR(3) | A_DIM);

    /* === Status bar: Live (screen) + Canvas (doc) cursor === */
    attron(A_REVERSE);
    for (int x = 0; x < COLS; x++) mvaddch(LINES - 1, x, ' ');
    mvprintw(LINES - 1, 0,
        " Live:(%d,%d)  Canvas:(%d,%d)  |  Offset:(%d,%d)  |  Tool:%s  |  q=quit  L-drag:draw  M-drag:pan  Drag|:resize",
        app->last_mouse_x, app->last_mouse_y,
        app->viewport.cursor_doc_x, app->viewport.cursor_doc_y,
        app->viewport.offset_x, app->viewport.offset_y,
        tool_name(app->active_tool));
    attroff(A_REVERSE);

    refresh();
}