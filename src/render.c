#include "tart.h"

void render(App *app) {
    clear();

    /* === Top Menu Bar === */
    attron(A_REVERSE);
    mvprintw(0, 0, " File  Edit  View  Layer  Window  Help ");
    attroff(A_REVERSE);

    /* === Top Ruler (X) === */
    mvprintw(1, 4, "0");
    for (int x = 10; x <= 110; x += 10) {
        mvprintw(1, x + 1, "%3d", x);
    }

    /* === Left Tool / Ruler Area === */
    attron(A_BOLD);
    mvprintw(3, 1, "curr");
    mvprintw(4, 1, "tool");
    attroff(A_BOLD);

    for (int y = 2; y < LINES - 3; y++) {
        int doc_y = y - 2 + app->viewport.offset_y;
        mvprintw(y, 0, "%3d|", doc_y);
    }

    /* === Main Canvas / Workspace === */
    int canvas_start_x = 6;
    int canvas_start_y = 2;

    // Workspace border
    attron(COLOR_PAIR(1) | A_DIM);
    for (int y = canvas_start_y; y < LINES - 2; y++) {
        mvaddch(y, canvas_start_x - 1, '|');
    }
    attroff(COLOR_PAIR(1) | A_DIM);

    // Placeholder content (grid)
    for (int dy = 0; dy < 30 && (canvas_start_y + dy) < LINES - 2; dy++) {
        for (int dx = 0; dx < 100 && (canvas_start_x + dx) < COLS; dx++) {
            int world_x = dx + app->viewport.offset_x;
            int world_y = dy + app->viewport.offset_y;
            char ch = (world_x % 10 == 0 && world_y % 5 == 0) ? '+' : '.';
            mvaddch(canvas_start_y + dy, canvas_start_x + dx, ch);
        }
    }

    /* === Status Bar === */
    mvprintw(LINES - 1, 0, 
        "Cursor: (%d, %d)  |  Offset: (%d, %d)  |  Workspace Size  |  q=quit  Middle-drag to pan",
        app->viewport.cursor_doc_x, app->viewport.cursor_doc_y,
        app->viewport.offset_x, app->viewport.offset_y);

    refresh();
}