/* Headless proof for tart_map_load/tart_map_composite -- no ncurses init,
 * just loads a .map, prints the composited world as plain text, then steps
 * every block's own animation clock one tick and prints again so a change
 * (e.g. water's ripple) is visible between the two dumps. Exit 0 = loaded
 * and rendered; exit 1 = tart_map_load failed. */
#define TART_IMPLEMENTATION
#include "tart.h"

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    const char *path = argc > 1 ? argv[1] : "examples/meadow.map";

    static TartMap map; /* ~1MB: static, not a plain local */
    if (!tart_map_load(&map, path)) {
        fwprintf(stderr, L"tart_map_load failed: %s\n", path);
        return 1;
    }

    int world_w = map.map_w * map.block_w;
    int world_h = map.map_h * map.block_h;
    wprintf(L"loaded %s: %dx%d blocks, %dx%d cells/block -> %dx%d world, %d block type(s)\n",
            path, map.map_w, map.map_h, map.block_w, map.block_h, world_w, world_h, map.block_count);

    for (int pass = 0; pass < 2; pass++) {
        wprintf(L"--- frame %d ---\n", pass);
        for (int y = 0; y < world_h; y++) {
            for (int x = 0; x < world_w; x++) {
                unsigned char col;
                putwchar(tart_map_composite(&map, x, y, &col));
            }
            putwchar(L'\n');
        }
        for (int i = 0; i < map.block_count; i++) {
            App *d = &map.blocks[i].doc;
            if (d->frame_count > 1) d->cur_frame = (d->cur_frame + 1) % d->frame_count;
        }
    }

    tart_map_free(&map);
    return 0;
}
