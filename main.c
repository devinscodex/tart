#define TART_IMPLEMENTATION
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