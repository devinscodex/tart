#include "tart.h"
#include <stdlib.h>
#include <string.h>

void init_canvas(Canvas *c, int w, int h) {
    c->width = w;
    c->height = h;
    c->cells = malloc((size_t)w * h);
    memset(c->cells, ' ', (size_t)w * h);
}

void cleanup_canvas(Canvas *c) {
    free(c->cells);
}