/* Headless proof for tart_load_doc/tart_composite -- no ncurses init, just
 * loads a single .tart document and dumps every frame as plain text to
 * stdout, one frame per block, blank line between. Unlike maptest.c (which
 * only proves the .map/block-composition path), this is the single-sprite
 * equivalent: verifies any .tart file -- sprite, scene, or animation --
 * loads and composites correctly without a live terminal.
 *
 * Color: default ON, ANSI 256-color escapes (xterm-256 index straight from
 * the .tart COLOR block, same numbering the format already uses -- see
 * README's File format section). --plain forces glyph-only output for
 * piping into a non-color-aware tool or a diff. Exit 0 = loaded and
 * rendered; exit 1 = tart_load_doc failed. */
#define TART_IMPLEMENTATION
#include "tart.h"

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    const char *path = NULL;
    bool color = true;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--plain") == 0) color = false;
        else if (!path) path = argv[i];
    }
    if (!path) path = "examples/grass.tart";

    static App app; /* frames[MAX_FRAMES] of layers[MAX_LAYERS] -- static, not a plain local */
    if (!tart_load_doc(&app, path)) {
        fwprintf(stderr, L"tart_load_doc failed: %s\n", path);
        return 1;
    }

    wprintf(L"loaded %s: %dx%d, %d frame(s), %d layer(s), fps=%d\n",
            path, app.width, app.height, app.frame_count, app.layer_count, app.fps);

    for (int f = 0; f < app.frame_count; f++) {
        wprintf(L"--- frame %d/%d ---\n", f + 1, app.frame_count);
        for (int y = 0; y < app.height; y++) {
            for (int x = 0; x < app.width; x++) {
                unsigned char col;
                wchar_t ch = tart_composite(&app, f, x, y, &col, NULL);
                if (color && col != 0) {
                    wprintf(L"\033[38;5;%dm%lc\033[0m", (int)col, ch);
                } else {
                    putwchar(ch);
                }
            }
            putwchar(L'\n');
        }
    }

    tart_free_doc(&app);
    return 0;
}
