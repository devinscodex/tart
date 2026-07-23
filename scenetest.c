/* Practice harness (Donatello, 2026-07-23): composites ONE foreground
 * sprite (loaded as its own App via tart_load_doc) on top of a background
 * .map (tart_map_composite), at a caller-given world offset. Not a shipped
 * TART tool -- proves whether background+figure compositing works at all
 * before any chapter-scale scene attempts it. The .map format itself has
 * no entity/placement grammar (confirmed by reading tart_map_load: header,
 * BLOCKS, GRID -- nothing else), so this is deliberately caller-side, the
 * same division of responsibility tart_map_composite already uses one
 * level down for blocks-inside-a-map.
 *
 * Compositing rule: background first, sprite drawn over it cell-by-cell,
 * sprite space (L' ') cells treated as transparent so the tile underneath
 * shows through -- same transparency convention .tart already uses for
 * layers within one document, just applied one level up.
 *
 * Usage: scenetest <map> <sprite.tart> <sprite_ox> <sprite_oy> [--plain] */
#define TART_IMPLEMENTATION
#include "tart.h"

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    const char *map_path = NULL, *sprite_path = NULL;
    int ox = 0, oy = 0;
    bool color = true;
    int pos_arg = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--plain") == 0) { color = false; continue; }
        switch (pos_arg) {
            case 0: map_path = argv[i]; break;
            case 1: sprite_path = argv[i]; break;
            case 2: ox = atoi(argv[i]); break;
            case 3: oy = atoi(argv[i]); break;
        }
        pos_arg++;
    }
    if (!map_path || !sprite_path) {
        fwprintf(stderr, L"usage: scenetest <map> <sprite.tart> <ox> <oy> [--plain]\n");
        return 1;
    }

    static TartMap map;
    if (!tart_map_load(&map, map_path)) {
        fwprintf(stderr, L"tart_map_load failed: %s\n", map_path);
        return 1;
    }
    static App sprite;
    if (!tart_load_doc(&sprite, sprite_path)) {
        fwprintf(stderr, L"tart_load_doc failed: %s\n", sprite_path);
        tart_map_free(&map);
        return 1;
    }

    int world_w = map.map_w * map.block_w;
    int world_h = map.map_h * map.block_h;
    wprintf(L"map %s: %dx%d world | sprite %s: %dx%d at (%d,%d), anchor cell (%d,%d)\n",
            map_path, world_w, world_h, sprite_path, sprite.width, sprite.height,
            ox, oy, ox + sprite.width / 2, oy + sprite.height - 1);

    for (int f = 0; f < sprite.frame_count; f++) {
        wprintf(L"--- sprite frame %d/%d ---\n", f + 1, sprite.frame_count);
        for (int y = 0; y < world_h; y++) {
            for (int x = 0; x < world_w; x++) {
                unsigned char bg_col;
                wchar_t bg_ch = tart_map_composite(&map, x, y, &bg_col);

                wchar_t ch = bg_ch;
                unsigned char col = bg_col;

                int sx = x - ox, sy = y - oy;
                if (sx >= 0 && sx < sprite.width && sy >= 0 && sy < sprite.height) {
                    unsigned char fg_col;
                    wchar_t fg_ch = tart_composite(&sprite, f, sx, sy, &fg_col, NULL);
                    if (fg_ch != L' ') { ch = fg_ch; col = fg_col; }
                }

                if (color && col != 0) {
                    wprintf(L"\033[38;5;%dm%lc\033[0m", (int)col, ch);
                } else {
                    putwchar(ch);
                }
            }
            putwchar(L'\n');
        }
    }

    tart_free_doc(&sprite);
    tart_map_free(&map);
    return 0;
}
