/* mapedit -- click-to-place authoring mode for .map files.
 *
 * Deliberately a separate small binary, not a mode bolted onto tart itself:
 * a map is a grid of block REFERENCES, not pixels, so it doesn't share
 * Canvas/Frame/Layer's data model. It reuses tart.h's map loader/compositor
 * (same code the game and maptest use, so what you see here is exactly what
 * Wordsworn renders) and mirrors TART's own ncurses init verbatim, but the
 * editing loop, undo, and panel below are new and MUCH smaller than TART's
 * full tool machinery -- there's exactly one operation (place/clear a block
 * at a grid cell), not a dozen drawing tools.
 *
 * No animation clock here on purpose: TART's own play loop is the one
 * moving part most likely to spin, and this mode has no need for it (block
 * preview is static, frame 0). getch() runs in plain blocking mode -- no
 * timeout(), no nodelay -- so there is no polling loop of any kind; the
 * process is asleep in the kernel between keystrokes/clicks like any
 * ordinary terminal program.
 */
#define TART_IMPLEMENTATION
#include "tart.h"

#define MAPEDIT_PANEL_W 18
#define MAPEDIT_UNDO_MAX 256

typedef struct { int col, row, prev_id; } MapeditUndo;

static TartMap g_map;               /* ~1MB: static, not a stack local */
static char g_path[256];
static int g_cur_col, g_cur_row;    /* cursor, in BLOCK units */
static int g_sel_block;             /* index into g_map.blocks; -1 = eraser */
static bool g_dirty;
static char g_status[80] = "";
static MapeditUndo g_undo[MAPEDIT_UNDO_MAX];
static int g_undo_count;

static void set_status(const char *msg) {
    snprintf(g_status, sizeof(g_status), "%s", msg);
}

static void mapedit_place(int col, int row, int block_id) {
    if (col < 0 || col >= g_map.map_w || row < 0 || row >= g_map.map_h) return;
    int prev = g_map.grid[row][col];
    if (prev == block_id) return;   /* no-op: nothing to undo, nothing dirtied */
    if (g_undo_count < MAPEDIT_UNDO_MAX) {
        g_undo[g_undo_count++] = (MapeditUndo){ col, row, prev };
    } /* stack full: oldest entries just become un-undoable, placement itself never blocked */
    g_map.grid[row][col] = block_id;
    g_dirty = true;
}

static void mapedit_undo(void) {
    if (g_undo_count == 0) { set_status("nothing to undo"); return; }
    MapeditUndo u = g_undo[--g_undo_count];
    g_map.grid[u.row][u.col] = u.prev_id;
    g_dirty = true;
    set_status("undone");
}

static void mapedit_save(void) {
    if (tart_map_save(&g_map, g_path)) {
        g_dirty = false;
        set_status("saved");
    } else {
        set_status("SAVE FAILED (path not writable?)");
    }
}

/* Bounds-checked single-cell draw -- a world/panel coordinate landing off
 * whatever terminal size is actually running must never reach ncurses'
 * own addch (tart_addwch itself does no clamping; TART's real app gets
 * away with that because canvas_r is pre-clamped to LINES/COLS by
 * tart_compute_layout before this is ever called -- mapedit has no such
 * layout pass, so the check has to live here instead). Same draw call
 * TART's own render() uses for every composited cell (tart.h:~1229),
 * not a separate untested path. */
static void put_at(int y, int x, wchar_t ch, int pair, attr_t extra) {
    if (y < 0 || y >= LINES || x < 0 || x >= COLS) return;
    attr_t attrs = extra;
    if (pair != 0) attrs |= COLOR_PAIR(pair);
    attron(attrs);
    tart_addwch(y, x, ch);
    attroff(attrs);
}

static void mapedit_render(void) {
    erase();

    mvprintw(0, 0, "MAPEDIT  %s  arrows/hjkl:move  enter/space:place  x:clear  "
                    "[ ]:pick block  u:undo  s:save  q:quit  click:pick/place",
              g_path);

    /* Left panel: block legend, doubles as click-to-pick target. */
    put_at(2, 1, L'.', g_sel_block == -1 ? 3 : 2, g_sel_block == -1 ? A_BOLD : A_NORMAL);
    mvprintw(2, 3, "empty (clear)");
    for (int i = 0; i < g_map.block_count; i++) {
        int row = 3 + i;
        bool sel = (i == g_sel_block);
        if (sel) attron(A_REVERSE);
        mvprintw(row, 1, "%-2s %.*s", g_map.blocks[i].id, MAPEDIT_PANEL_W - 4, g_map.blocks[i].path);
        if (sel) attroff(A_REVERSE);
    }

    /* Grid: composited world, cursor's whole block highlighted. */
    int gx0 = MAPEDIT_PANEL_W + 1, gy0 = 2;
    int world_w = g_map.map_w * g_map.block_w, world_h = g_map.map_h * g_map.block_h;
    for (int y = 0; y < world_h; y++) {
        int by = y / g_map.block_h;
        for (int x = 0; x < world_w; x++) {
            int bx = x / g_map.block_w;
            unsigned char color;
            wchar_t ch = tart_map_composite(&g_map, x, y, &color);
            int pair = color ? 16 + color : 0;
            attr_t extra = (bx == g_cur_col && by == g_cur_row) ? A_REVERSE : A_NORMAL;
            put_at(gy0 + y, gx0 + x, ch, pair, extra);
        }
    }

    char sel_name[24] = "eraser";
    if (g_sel_block >= 0) snprintf(sel_name, sizeof(sel_name), "%s", g_map.blocks[g_sel_block].id);
    mvprintw(LINES - 1, 0, "cursor %d,%d  placing: %s  %s%s",
              g_cur_col, g_cur_row, sel_name, g_dirty ? "[modified] " : "", g_status);

    move(LINES - 1, 0);  /* park real cursor somewhere harmless, no visible caret drift */
    refresh();
}

/* 'q' with unsaved changes: same y/n/cancel shape as TART's own guard, kept
 * tiny here since there's exactly one caller. */
static bool mapedit_confirm_quit(void) {
    if (!g_dirty) return true;
    mvprintw(LINES - 1, 0, "unsaved changes -- s=save & quit, q=quit without saving, any other key=cancel   ");
    clrtoeol();
    refresh();
    int ch = getch();
    if (ch == 's' || ch == 'S') { mapedit_save(); return true; }
    if (ch == 'q' || ch == 'Q') return true;
    return false;
}

static void mapedit_handle_mouse(void) {
    MEVENT ev;
    if (getmouse(&ev) != OK) return;
    if (!(ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) return;

    /* Panel click: pick a block (or the eraser row) as current. */
    if (ev.x < MAPEDIT_PANEL_W) {
        if (ev.y == 2) { g_sel_block = -1; return; }
        int i = ev.y - 3;
        if (i >= 0 && i < g_map.block_count) g_sel_block = i;
        return;
    }

    /* Grid click: move cursor there and place immediately -- that's the
     * "click-to-place" the ask named, vs. panel clicks which only pick. */
    int gx0 = MAPEDIT_PANEL_W + 1, gy0 = 2;
    int wx = ev.x - gx0, wy = ev.y - gy0;
    if (wx < 0 || wy < 0) return;
    int col = wx / g_map.block_w, row = wy / g_map.block_h;
    if (col < 0 || col >= g_map.map_w || row < 0 || row >= g_map.map_h) return;
    g_cur_col = col;
    g_cur_row = row;
    mapedit_place(col, row, g_sel_block);
}

static void mapedit_init_term(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED, NULL);
    start_color();
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);
    for (int i = 0; i < 256 && 16 + i < COLOR_PAIRS; i++)
        init_pair((short)(16 + i), (short)i, COLOR_BLACK);
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: mapedit [file.map]\n");
        return 1;
    }
    snprintf(g_path, sizeof(g_path), "%s", argc > 1 ? argv[1] : "examples/meadow.map");

    if (!tart_map_load(&g_map, g_path)) {
        fprintf(stderr, "mapedit: failed to load %s\n", g_path);
        return 1;
    }
    g_sel_block = g_map.block_count > 0 ? 0 : -1;
    set_status("");

    mapedit_init_term();

    bool running = true;
    while (running) {
        mapedit_render();
        int ch = getch();
        switch (ch) {
            case KEY_UP: case 'k':    g_cur_row = g_cur_row > 0 ? g_cur_row - 1 : 0; break;
            case KEY_DOWN: case 'j':  g_cur_row = g_cur_row < g_map.map_h - 1 ? g_cur_row + 1 : g_cur_row; break;
            case KEY_LEFT: case 'h':  g_cur_col = g_cur_col > 0 ? g_cur_col - 1 : 0; break;
            case KEY_RIGHT: case 'l': g_cur_col = g_cur_col < g_map.map_w - 1 ? g_cur_col + 1 : g_cur_col; break;
            case '\t': case ']':
                if (g_map.block_count > 0) g_sel_block = (g_sel_block + 1) % g_map.block_count;
                break;
            case KEY_BTAB: case '[':
                if (g_map.block_count > 0) g_sel_block = (g_sel_block - 1 + g_map.block_count) % g_map.block_count;
                break;
            case '\n': case '\r': case KEY_ENTER: case ' ':
                mapedit_place(g_cur_col, g_cur_row, g_sel_block);
                break;
            case 'x': case KEY_DC:
                mapedit_place(g_cur_col, g_cur_row, -1);
                break;
            case 'u':
                mapedit_undo();
                break;
            case 's':
                mapedit_save();
                break;
            case KEY_MOUSE:
                mapedit_handle_mouse();
                break;
            case 'q':
                if (mapedit_confirm_quit()) running = false;
                break;
            default:
                break;
        }
    }

    endwin();
    tart_map_free(&g_map);
    return 0;
}
