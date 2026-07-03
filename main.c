#include "tart.h"

int main(void) {
    App app = {0};
    init_app(&app);

    while (app.running) {
        render(&app);
        handle_input(&app);
    }

    cleanup_app(&app);
    return 0;
}

void init_app(App *app) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    start_color();

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, COLOR_WHITE);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);

    init_canvas(&app->canvas, 80, 25);  // new empty file: 80x25 canvas
    app->viewport.canvas = &app->canvas;
    app->running = 1;
    app->active_tool = TOOL_PENCIL;
    app->sidebar_width = 22;
    app->dragging_sidebar = false;
    app->mouse_down = false;
    app->last_mouse_x = 0;
    app->last_mouse_y = 0;

    // initial padded workspace view (~25% margin each side, rounded up to nearest 10)
    int pad_x = ((80 * 25 + 99) / 100 + 9) / 10 * 10;   // ~20
    int pad_y = ((25 * 25 + 99) / 100 + 9) / 10 * 10;   // ~10
    app->viewport.offset_x = -(pad_x / 2);
    app->viewport.offset_y = -(pad_y / 2);
}

void cleanup_app(App *app) {
    cleanup_canvas(&app->canvas);
    endwin();
}