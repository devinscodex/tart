#include "common.h"

// what are suckless, best practices when it comes to block comments at the head of a .c or .h files?

WINDOW *game_win = NULL;
static int last_lines = 0;
static int last_cols = 0;

void resize_game_window(void) {
    // only resize if terminal size actually changed
    if (LINES == last_lines && COLS == last_cols && game_win != NULL)
        return;

    last_lines = LINES;
    last_cols = COLS;

    // delete old window if it exists
    if (game_win != NULL) {
        delwin(game_win);
        game_win = NULL;
    }

    // calculate new dynamic size with margins
    int height = LINES - 2;
    int width  = COLS  - 2;

    if (height < 10) height = 10;
    if (width  < 20) width  = 20;

    int starty = (LINES - height) / 2;
    int startx = (COLS  - width)  / 2;

    game_win = newwin(height, width, starty, startx);
    box(game_win, 0, 0);
}

void render_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    resize_game_window();
    wrefresh(game_win);
}

void render_frame(void) {
    // Check for resize every frame
    resize_game_window();

    werase(game_win);
    box(game_win, 0, 0);

    int maxy = getmaxy(game_win);
    int maxx = getmaxx(game_win);

    // Main content
    mvwprintw(game_win, 2, 2, "TermScape - Press Q to quit");

    if (show_debug) {
        // Debug header
        mvwprintw(game_win, 2, maxx - 20, "DEBUG MODE");

        // Corner coordinates
        mvwprintw(game_win, 1, 1,                  "(%d,%d)", 0, 0);
        mvwprintw(game_win, 1, maxx - 8,           "(%d,%d)", 0, maxx - 1);
        mvwprintw(game_win, maxy - 2, 1,           "(%d,%d)", maxy - 1, 0);
        mvwprintw(game_win, maxy - 2, maxx - 8,    "(%d,%d)", maxy - 1, maxx - 1);

        // Debug info
        mvwprintw(game_win, 4, 2, "show_debug = %s", show_debug ? "true" : "false");
        mvwprintw(game_win, 5, 2, "Press F1 to toggle");
        mvwprintw(game_win, 7, 2, "Terminal: %d x %d", COLS, LINES);

        // Simple coordinate grid
        for (int x = 5; x < maxx; x += 5) {
            mvwprintw(game_win, 0, x, "%d", x);
        }
        for (int y = 2; y < maxy; y += 2) {
            mvwprintw(game_win, y, 0, "%d", y);
        }
    }

    wrefresh(game_win);
}