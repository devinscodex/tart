#include "tart.h"

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
        if (sy == tool_r.y + 3) vp->current_brush = '#';
        else if (sy == tool_r.y + 4) vp->current_brush = ' ';
    }

    bool first_event = (vp->last_mouse_screen_x == 0 && vp->last_mouse_screen_y == 0);
    bool acting = vp->left_click_held || vp->space_held;

    if (acting && !vp->is_dragging) {
        if (wx >= 0 && wy >= 0) tart_draw_at(app, wx, wy, vp->current_brush);
    } else if ((acting || vp->is_dragging) && !first_event) {
        vp->offset_x -= (sx - vp->last_mouse_screen_x);
        vp->offset_y -= (sy - vp->last_mouse_screen_y);
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
                        int btn = cb & 3;
                        sgr_left_press    = (!released && btn == 0);
                        sgr_left_release  = (released && btn == 0);
                        sgr_mid_press     = (!released && btn == 1);
                        sgr_mid_release   = (released && btn == 1);
                        sgr_right_press   = (!released && btn == 2);
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

            if (resolved) {
                if (strcmp(resolved, "quit") == 0) {
                    app->running = 0;
                } else if (strcmp(resolved, "help") == 0) {
                    app->viewport.help_visible = true;
                }
            }

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
        app->viewport.offset_x = 0;
        app->viewport.offset_y = 0;
        app->viewport.current_mouse_world_x = 0;
        app->viewport.current_mouse_world_y = 0;
        app->viewport.vim_cursor_x = 0;
        app->viewport.vim_cursor_y = 0;
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
    if (ch == 'e' || ch == 'E') {
        app->viewport.current_brush = ' ';
    }
    if (ch == 'p' || ch == 'P') {
        app->viewport.current_brush = '#';
    }
    if (ch == 'v' || ch == 'V') {
        app->viewport.is_dragging = !app->viewport.is_dragging;
        app->viewport.mode = app->viewport.is_dragging ? 'p' : 'n';
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
