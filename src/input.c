#include "tart.h"

void handle_input(App *app) {
    int ch = getch();
    if (ch == 'q' || ch == 'Q') {
        app->running = 0;
    } else if (ch == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK) {
            app->viewport.cursor_doc_x = event.x;
            app->viewport.cursor_doc_y = event.y;
            // TODO: Middle click drag logic here
        }
    }
}