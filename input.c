#include "tart.h"

static const char TART_BRUSH_CHARS[] = "#@*.o+";
#define TART_BRUSH_CHAR_COUNT (sizeof(TART_BRUSH_CHARS) - 1)

static char tart_cycle_char(char cur) {
    for (size_t i = 0; i < TART_BRUSH_CHAR_COUNT; i++) {
        if (TART_BRUSH_CHARS[i] == cur) return TART_BRUSH_CHARS[(i + 1) % TART_BRUSH_CHAR_COUNT];
    }
    return TART_BRUSH_CHARS[0];
}

static int tart_cycle_size(int size, int dir) {
    static const int sizes[] = { 1, 3, 5 };
    int idx = 0;
    for (int i = 0; i < 3; i++) if (sizes[i] == size) idx = i;
    idx += dir;
    if (idx < 0) idx = 0;
    if (idx > 2) idx = 2;
    return sizes[idx];
}

static void tart_apply_panel_action(App *app, TartPanelAction action, int value) {
    Viewport *vp = &app->viewport;
    switch (action) {
        case PANEL_SELECT_TOOL:
            vp->tool = (TartTool)value;
            break;
        case PANEL_CHAR_CYCLE:
            vp->brush_char = tart_cycle_char(vp->brush_char);
            break;
        case PANEL_SIZE_DEC:
            if (vp->tool == TOOL_ERASER) vp->eraser_size = tart_cycle_size(vp->eraser_size, -1);
            else vp->brush_size = tart_cycle_size(vp->brush_size, -1);
            break;
        case PANEL_SIZE_INC:
            if (vp->tool == TOOL_ERASER) vp->eraser_size = tart_cycle_size(vp->eraser_size, 1);
            else vp->brush_size = tart_cycle_size(vp->brush_size, 1);
            break;
        case PANEL_SHAPE_TOGGLE:
            if (vp->tool == TOOL_ERASER) vp->eraser_shape = (vp->eraser_shape == BRUSH_SQUARE) ? BRUSH_ROUND : BRUSH_SQUARE;
            else vp->brush_shape = (vp->brush_shape == BRUSH_SQUARE) ? BRUSH_ROUND : BRUSH_SQUARE;
            break;
        case PANEL_RECT_FILL_TOGGLE:
            vp->rect_filled = !vp->rect_filled;
            break;
        default:
            break;
    }
}

static void tart_reset_view(App *app) {
    Viewport *vp = &app->viewport;
    vp->offset_x = 0;
    vp->offset_y = 0;
    vp->current_mouse_world_x = 0;
    vp->current_mouse_world_y = 0;
    vp->vim_cursor_x = 0;
    vp->vim_cursor_y = 0;
}

static void tart_toggle_pan(App *app) {
    Viewport *vp = &app->viewport;
    vp->is_dragging = !vp->is_dragging;
    vp->mode = vp->is_dragging ? 'p' : 'n';
}

static void tart_deselect(App *app) {
    Viewport *vp = &app->viewport;
    free(vp->sel_buffer);
    vp->sel_buffer = NULL;
    vp->sel_w = 0;
    vp->sel_h = 0;
    vp->has_selection = false;
}

static void tart_execute_command(App *app, const char *name) {
    if (strcmp(name, "quit") == 0) {
        app->running = 0;
    } else if (strcmp(name, "help") == 0) {
        app->viewport.help_visible = true;
    } else if (strcmp(name, "deselect") == 0) {
        tart_deselect(app);
    } else if (strcmp(name, "reset-view") == 0) {
        tart_reset_view(app);
    } else if (strcmp(name, "toggle-pan") == 0) {
        tart_toggle_pan(app);
    }
}

static void tart_tool_rect(App *app, int wx, int wy, bool left_press, bool left_release) {
    Viewport *vp = &app->viewport;
    if (left_press) {
        vp->tool_dragging = true;
        vp->drag_start_wx = wx;
        vp->drag_start_wy = wy;
    }
    if (vp->tool_dragging) {
        vp->drag_cur_wx = wx;
        vp->drag_cur_wy = wy;
    }
    if (left_release && vp->tool_dragging) {
        int x0 = vp->drag_start_wx, x1 = wx;
        int y0 = vp->drag_start_wy, y1 = wy;
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                bool border = (x == x0 || x == x1 || y == y0 || y == y1);
                if (border || vp->rect_filled) tart_draw_at(app, x, y, vp->brush_char);
            }
        }
        vp->tool_dragging = false;
    }
}

static void tart_tool_select(App *app, int wx, int wy, bool left_press, bool left_release) {
    Viewport *vp = &app->viewport;
    if (left_press) {
        vp->tool_dragging = true;
        vp->drag_start_wx = wx;
        vp->drag_start_wy = wy;
    }
    if (vp->tool_dragging) {
        vp->drag_cur_wx = wx;
        vp->drag_cur_wy = wy;
    }
    if (left_release && vp->tool_dragging) {
        int x0 = vp->drag_start_wx, x1 = wx;
        int y0 = vp->drag_start_wy, y1 = wy;
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        int w = x1 - x0 + 1, h = y1 - y0 + 1;

        free(vp->sel_buffer);
        vp->sel_buffer = malloc((size_t)w * h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                vp->sel_buffer[y * w + x] = tart_read_cell(app, x0 + x, y0 + y);
            }
        }
        vp->sel_w = w;
        vp->sel_h = h;
        vp->has_selection = true;
        vp->tool_dragging = false;
    }
}

static void tart_tool_move(App *app, int wx, int wy, bool left_press, bool left_release) {
    Viewport *vp = &app->viewport;
    if (left_press && vp->has_selection) {
        vp->tool_dragging = true;
    }
    if (vp->tool_dragging) {
        vp->drag_cur_wx = wx;
        vp->drag_cur_wy = wy;
    }
    if (left_release && vp->tool_dragging) {
        for (int y = 0; y < vp->sel_h; y++) {
            for (int x = 0; x < vp->sel_w; x++) {
                tart_draw_at(app, wx + x, wy + y, vp->sel_buffer[y * vp->sel_w + x]);
            }
        }
        vp->tool_dragging = false;
    }
}

/* Returns true if the click was consumed by the menu bar / an open dropdown,
 * meaning the rest of tart_apply_mouse should not also treat it as a canvas
 * or tool-panel interaction. */
static bool tart_handle_menu_click(App *app, int sx, int sy, TartRect menu_r) {
    Viewport *vp = &app->viewport;
    TartMenuBarItem mitems[TART_MENU_COUNT];
    tart_build_menu_bar(app, mitems);

    if (sy == menu_r.y) {
        for (size_t i = 0; i < TART_MENU_COUNT; i++) {
            if (sx >= mitems[i].x0 && sx <= mitems[i].x1) {
                vp->open_menu = (vp->open_menu == (int)i) ? -1 : (int)i;
                vp->menu_hover_item = 0;
                return true;
            }
        }
    }

    if (vp->open_menu >= 0) {
        const TartMenuDef *menu = &TART_MENUS[vp->open_menu];
        int box_y0 = menu_r.y + 1;
        int box_x0 = mitems[vp->open_menu].x0;
        if (sy >= box_y0 && sy < box_y0 + menu->item_count && sx >= box_x0) {
            int item_idx = sy - box_y0;
            tart_execute_command(app, menu->items[item_idx]);
        }
        vp->open_menu = -1;
        return true;
    }

    return false;
}

static void tart_apply_mouse(App *app, int sx, int sy,
                              bool left_press, bool left_release,
                              bool mid_press, bool mid_release,
                              bool right_press, bool right_release,
                              bool wheel_up, bool wheel_down,
                              bool shift, bool ctrl, bool alt) {
    Viewport *vp = &app->viewport;
    vp->mod_shift = shift;
    vp->mod_ctrl = ctrl;
    vp->mod_alt = alt;

    if (left_press)    vp->left_click_held = true;
    if (left_release)  vp->left_click_held = false;
    if (mid_press)      vp->mid_click_held = true;
    if (mid_release)    vp->mid_click_held = false;
    if (right_press)    vp->right_click_held = true;
    if (right_release)  vp->right_click_held = false;

    if (wheel_up || wheel_down) {
        int delta = wheel_up ? -4 : 4;
        if (shift) vp->offset_x += delta;
        else vp->offset_y += delta;
        return;
    }

    TartRect menu_r, ruler_r, tool_r, y_ruler_r, canvas_r, status_r;
    tart_compute_layout(app, &menu_r, &ruler_r, &tool_r, &y_ruler_r, &canvas_r, &status_r);

    if (left_press && tart_handle_menu_click(app, sx, sy, menu_r)) {
        vp->last_mouse_screen_x = sx;
        vp->last_mouse_screen_y = sy;
        return;
    }

    int wx = (sx >= canvas_r.x) ? (sx - canvas_r.x) + vp->offset_x : -9999;
    int wy = (sy >= canvas_r.y) ? (sy - canvas_r.y) + vp->offset_y : -9999;
    vp->mouse_world_x = wx;
    vp->mouse_world_y = wy;
    vp->current_mouse_world_x = wx;
    vp->current_mouse_world_y = wy;
    vp->vim_cursor_x = wx;
    vp->vim_cursor_y = wy;
    vp->cursor_doc_x = wx;
    vp->cursor_doc_y = wy;

    if (left_press && sx >= tool_r.x && sx < tool_r.x + tool_r.w && sy >= tool_r.y && sy < tool_r.y + tool_r.h) {
        TartPanelRow rows[TOOL_PANEL_MAX_ROWS];
        int n = tart_build_tool_panel(app, rows);
        int rel_row = sy - tool_r.y;
        int rel_x = sx - tool_r.x;
        for (int i = 0; i < n; i++) {
            if (rows[i].row != rel_row) continue;
            if (rows[i].x0 != -1 && (rel_x < rows[i].x0 || rel_x > rows[i].x1)) continue;
            tart_apply_panel_action(app, rows[i].action, rows[i].value);
            break;
        }
    }

    bool first_event = (vp->last_mouse_screen_x == 0 && vp->last_mouse_screen_y == 0);

    if (vp->is_dragging) {
        if (!first_event) {
            vp->offset_x -= (sx - vp->last_mouse_screen_x);
            vp->offset_y -= (sy - vp->last_mouse_screen_y);
        }
    } else {
        bool acting = vp->left_click_held || vp->space_held;
        switch (vp->tool) {
            case TOOL_PENCIL:
                if (acting) tart_stamp(app, wx, wy, vp->brush_size, vp->brush_shape, vp->brush_char);
                break;
            case TOOL_ERASER:
                if (acting) tart_stamp(app, wx, wy, vp->eraser_size, vp->eraser_shape, ' ');
                break;
            case TOOL_RECT:
                tart_tool_rect(app, wx, wy, left_press, left_release);
                break;
            case TOOL_SELECT:
                tart_tool_select(app, wx, wy, left_press, left_release);
                break;
            case TOOL_MOVE:
                tart_tool_move(app, wx, wy, left_press, left_release);
                break;
        }
    }

    vp->last_mouse_screen_x = sx;
    vp->last_mouse_screen_y = sy;
}

void handle_input(App *app) {
    int ch = getch();
    if (ch == ERR) {
        return;
    }

    bool have_sgr_mouse = false;
    int sgr_sx = 0, sgr_sy = 0;
    bool sgr_left_press = false, sgr_left_release = false;
    bool sgr_mid_press = false, sgr_mid_release = false;
    bool sgr_right_press = false, sgr_right_release = false;
    bool sgr_wheel_up = false, sgr_wheel_down = false;
    bool sgr_shift = false, sgr_ctrl = false, sgr_alt = false;

    if (ch == 27) {
        /* ncurses' terminfo kmous is the legacy "\E[M" prefix, so it never
         * recognizes xterm SGR mouse reports ("\E[<...M/m"). Terminals that
         * support SGR (nearly all modern ones) still emit them once we
         * request mode 1006, so we parse those bytes by hand here. A real
         * standalone Escape keypress has nothing following it, so a
         * zero-delay peek reliably tells the two apart. */
        timeout(0);
        int c1 = getch();
        if (c1 == '[') {
            int c2 = getch();
            if (c2 == '<') {
                int fields[3] = {0, 0, 0};
                int fidx = 0;
                int c;
                bool terminated = false;
                bool released = false;
                while ((c = getch()) != ERR) {
                    if (c >= '0' && c <= '9') {
                        fields[fidx] = fields[fidx] * 10 + (c - '0');
                    } else if (c == ';' && fidx < 2) {
                        fidx++;
                    } else if (c == 'M' || c == 'm') {
                        released = (c == 'm');
                        terminated = true;
                        break;
                    } else {
                        break;
                    }
                }
                if (terminated) {
                    int cb = fields[0];
                    sgr_sx = fields[1] - 1;
                    sgr_sy = fields[2] - 1;
                    sgr_shift = (cb & 4) != 0;
                    sgr_alt   = (cb & 8) != 0;
                    sgr_ctrl  = (cb & 16) != 0;
                    if (cb & 64) {
                        sgr_wheel_up = ((cb & 1) == 0);
                        sgr_wheel_down = !sgr_wheel_up;
                    } else {
                        /* A fresh press never carries the motion flag (bit
                         * 0x20); drag-continuation samples repeat the same
                         * button number WITH it set. Without this check,
                         * every motion sample during a drag would look like
                         * a brand-new press, which re-arms drag-start state
                         * (rect/select) on every intermediate sample instead
                         * of only at the real press. */
                        bool motion = (cb & 32) != 0;
                        int btn = cb & 3;
                        sgr_left_press    = (!released && !motion && btn == 0);
                        sgr_left_release  = (released && btn == 0);
                        sgr_mid_press     = (!released && !motion && btn == 1);
                        sgr_mid_release   = (released && btn == 1);
                        sgr_right_press   = (!released && !motion && btn == 2);
                        sgr_right_release = (released && btn == 2);
                    }
                    have_sgr_mouse = true;
                    ch = KEY_MOUSE;
                }
            } else {
                if (c2 != ERR) ungetch(c2);
                ungetch('[');
            }
        } else {
            if (c1 != ERR) ungetch(c1);
        }
        timeout(50);
    }

    if (app->viewport.help_visible) {
        if (ch != KEY_MOUSE) {
            app->viewport.help_visible = false;
        }
        return;
    }

    if (app->viewport.command_mode) {
        if (ch == '\n' || ch == KEY_ENTER) {
            const char *buf = app->viewport.command_buffer;
            const char *resolved = NULL;

            for (size_t i = 0; i < TART_COMMAND_COUNT; i++) {
                if (strcmp(buf, TART_COMMANDS[i].name) == 0 ||
                    (TART_COMMANDS[i].alias && strcmp(buf, TART_COMMANDS[i].alias) == 0)) {
                    resolved = TART_COMMANDS[i].name;
                    break;
                }
            }
            if (!resolved && buf[0] != '\0') {
                const char *candidate = NULL;
                int matches = 0;
                size_t len = strlen(buf);
                for (size_t i = 0; i < TART_COMMAND_COUNT; i++) {
                    if (strncmp(TART_COMMANDS[i].name, buf, len) == 0) {
                        candidate = TART_COMMANDS[i].name;
                        matches++;
                    }
                }
                if (matches == 1) resolved = candidate;
            }

            if (resolved) tart_execute_command(app, resolved);

            app->viewport.command_mode = 0;
            app->viewport.mode = 'n';
            app->viewport.command_len = 0;
            memset(app->viewport.command_buffer, 0, sizeof(app->viewport.command_buffer));
        } else if (ch == 27) {
            app->viewport.command_mode = 0;
            app->viewport.mode = 'n';
            app->viewport.command_len = 0;
            memset(app->viewport.command_buffer, 0, sizeof(app->viewport.command_buffer));
        } else if ((ch == KEY_BACKSPACE || ch == 127) && app->viewport.command_len > 0) {
            app->viewport.command_buffer[--app->viewport.command_len] = '\0';
        } else if (ch >= 32 && ch < 127 && app->viewport.command_len < 255) {
            app->viewport.command_buffer[app->viewport.command_len++] = ch;
            app->viewport.command_buffer[app->viewport.command_len] = '\0';
        }
        return;
    }

    if (app->viewport.open_menu >= 0 && ch != KEY_MOUSE) {
        int n = TART_MENUS[app->viewport.open_menu].item_count;
        if (ch == 27) {
            app->viewport.open_menu = -1;
        } else if (ch == KEY_UP) {
            app->viewport.menu_hover_item = (app->viewport.menu_hover_item - 1 + n) % n;
        } else if (ch == KEY_DOWN) {
            app->viewport.menu_hover_item = (app->viewport.menu_hover_item + 1) % n;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            tart_execute_command(app, TART_MENUS[app->viewport.open_menu].items[app->viewport.menu_hover_item]);
            app->viewport.open_menu = -1;
        }
        return;
    }

    if (ch == ':' || ch == 5) {
        app->viewport.command_mode = 1;
        app->viewport.mode = 'c';
        app->viewport.command_len = 0;
        memset(app->viewport.command_buffer, 0, sizeof(app->viewport.command_buffer));
        return;
    }

    bool vim_cursor_moved = false;

    if (ch == 'q' || ch == 'Q') {
        app->running = 0;
    }
    if (ch == ' ') {
        app->viewport.space_held = true;
    } else if (ch != KEY_MOUSE) {
        app->viewport.space_held = false;
    }
    if (ch == KEY_HOME) {
        tart_reset_view(app);
    }
    if (ch == KEY_END) {
        int last_x = app->canvas.width - 1;
        int last_y = app->canvas.height - 1;
        app->viewport.vim_cursor_x = last_x;
        app->viewport.vim_cursor_y = last_y;
        app->viewport.current_mouse_world_x = last_x;
        app->viewport.current_mouse_world_y = last_y;

        int vis_w = COLS - CANVAS_START_X;
        int vis_h = LINES - 4;
        if (last_x < app->viewport.offset_x || last_x > app->viewport.offset_x + vis_w - 1) {
            app->viewport.offset_x = last_x - vis_w / 2;
        }
        if (last_y < app->viewport.offset_y || last_y > app->viewport.offset_y + vis_h - 1) {
            app->viewport.offset_y = last_y - vis_h / 2;
        }
    }
    if (ch == KEY_F(1)) {
        app->viewport.help_visible = true;
    }
    if (ch == 'p' || ch == 'P') {
        app->viewport.tool = TOOL_PENCIL;
    }
    if (ch == 'e' || ch == 'E') {
        app->viewport.tool = TOOL_ERASER;
    }
    if (ch == 'r') {
        app->viewport.tool = TOOL_RECT;
    }
    if (ch == 's') {
        app->viewport.tool = TOOL_SELECT;
    }
    if (ch == 'm') {
        app->viewport.tool = TOOL_MOVE;
    }
    if (ch == 't') {
        tart_apply_panel_action(app, PANEL_CHAR_CYCLE, 0);
    }
    if (ch == '[') {
        tart_apply_panel_action(app, PANEL_SIZE_DEC, 0);
    }
    if (ch == ']') {
        tart_apply_panel_action(app, PANEL_SIZE_INC, 0);
    }
    if (ch == 9) {
        tart_apply_panel_action(app, PANEL_SHAPE_TOGGLE, 0);
    }
    if (ch == 'f' && app->viewport.tool == TOOL_RECT) {
        tart_apply_panel_action(app, PANEL_RECT_FILL_TOGGLE, 0);
    }
    if (ch == 'v' || ch == 'V') {
        tart_toggle_pan(app);
    }
    if (ch == 'h') {
        app->viewport.vim_cursor_x--;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    }
    if (ch == 'l') {
        app->viewport.vim_cursor_x++;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    }
    if (ch == 'j') {
        app->viewport.vim_cursor_y++;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    }
    if (ch == 'k') {
        app->viewport.vim_cursor_y--;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    }
    if (ch == '0') {
        app->viewport.vim_cursor_x = 0;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    }
    if (ch == '$') {
        app->viewport.vim_cursor_x = app->canvas.width - 1;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    }
    if (ch == 'w') {
        app->viewport.vim_cursor_x += 5;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    }
    if (ch == 'b') {
        app->viewport.vim_cursor_x -= 5;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    }
    if (ch == 'G') {
        app->viewport.vim_cursor_y = app->canvas.height - 1;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    }
    if (ch == 'g') {
        app->viewport.vim_cursor_y = 0;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    }
    if (ch == 4) {
        app->viewport.offset_y += (LINES - 4) / 2;
        app->viewport.mode = 'n';
    }
    if (ch == 21) {
        app->viewport.offset_y -= (LINES - 4) / 2;
        app->viewport.mode = 'n';
    }
    if (ch == KEY_PPAGE) {
        app->viewport.offset_y -= LINES - 4;
        app->viewport.mode = 'n';
    }
    if (ch == KEY_NPAGE) {
        app->viewport.offset_y += LINES - 4;
        app->viewport.mode = 'n';
    }
    if (ch == KEY_LEFT) {
        app->viewport.offset_x -= 5;
    }
    if (ch == KEY_RIGHT) {
        app->viewport.offset_x += 5;
    }
    if (ch == KEY_UP) {
        app->viewport.offset_y -= 3;
    }
    if (ch == KEY_DOWN) {
        app->viewport.offset_y += 3;
    }
    if (ch == KEY_MOUSE) {
        if (have_sgr_mouse) {
            tart_apply_mouse(app, sgr_sx, sgr_sy,
                sgr_left_press, sgr_left_release,
                sgr_mid_press, sgr_mid_release,
                sgr_right_press, sgr_right_release,
                sgr_wheel_up, sgr_wheel_down,
                sgr_shift, sgr_ctrl, sgr_alt);
        } else {
            MEVENT event;
            if (getmouse(&event) == OK) {
                tart_apply_mouse(app, event.x, event.y,
                    (event.bstate & BUTTON1_PRESSED) != 0, (event.bstate & BUTTON1_RELEASED) != 0,
                    (event.bstate & BUTTON2_PRESSED) != 0, (event.bstate & BUTTON2_RELEASED) != 0,
                    (event.bstate & BUTTON3_PRESSED) != 0, (event.bstate & BUTTON3_RELEASED) != 0,
                    (event.bstate & BUTTON4_PRESSED) != 0, (event.bstate & BUTTON5_PRESSED) != 0,
                    (event.bstate & BUTTON_SHIFT) != 0, (event.bstate & BUTTON_CTRL) != 0, (event.bstate & BUTTON_ALT) != 0);
            }
        }
    }

    if (vim_cursor_moved) {
        int visible_w = COLS - CANVAS_START_X;
        int visible_h = LINES - 4;
        if (app->viewport.vim_cursor_x < app->viewport.offset_x) {
            app->viewport.offset_x = app->viewport.vim_cursor_x;
        } else if (app->viewport.vim_cursor_x > app->viewport.offset_x + visible_w - 1) {
            app->viewport.offset_x = app->viewport.vim_cursor_x - visible_w / 2;
        }
        if (app->viewport.vim_cursor_y < app->viewport.offset_y) {
            app->viewport.offset_y = app->viewport.vim_cursor_y;
        } else if (app->viewport.vim_cursor_y > app->viewport.offset_y + visible_h - 1) {
            app->viewport.offset_y = app->viewport.vim_cursor_y - visible_h / 2;
        }
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
    }

    {
        int h_limit = app->canvas.width / 2 + 20;
        int v_limit = app->canvas.height / 2 + 20;
        if (app->viewport.offset_x < -h_limit) app->viewport.offset_x = -h_limit;
        if (app->viewport.offset_x >  h_limit) app->viewport.offset_x =  h_limit;
        if (app->viewport.offset_y < -v_limit) app->viewport.offset_y = -v_limit;
        if (app->viewport.offset_y >  v_limit) app->viewport.offset_y =  v_limit;
    }
}
