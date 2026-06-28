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

    init_canvas(&app->canvas, 120, 60);  // initial document size
    app->viewport.canvas = &app->canvas;
    app->running = 1;
}

void cleanup_app(App *app) {
    cleanup_canvas(&app->canvas);
    endwin();
}