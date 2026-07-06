#include "tart.h"

void handle_input(App *app) {
    int ch = getch();
    if (ch == ERR) {
        return;
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
        MEVENT event;
        if (getmouse(&event) == OK) {
            int sx = event.x;
            int sy = event.y;

            if (!(event.bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED))) {
                int wx = (sx >= CANVAS_START_X) ? (sx - CANVAS_START_X) + app->viewport.offset_x : -9999;
                int wy = (sy >= 3) ? (sy - 3) + app->viewport.offset_y : -9999;
                app->viewport.mouse_world_x = wx;
                app->viewport.mouse_world_y = wy;
                app->viewport.current_mouse_world_x = wx;
                app->viewport.current_mouse_world_y = wy;
                app->viewport.vim_cursor_x = wx;
                app->viewport.vim_cursor_y = wy;
                app->viewport.cursor_doc_x = wx;
                app->viewport.cursor_doc_y = wy;

                if (app->viewport.last_mouse_screen_x == 0 && app->viewport.last_mouse_screen_y == 0) {
                    app->viewport.last_mouse_screen_x = sx;
                    app->viewport.last_mouse_screen_y = sy;
                }
            }

            if (event.bstate & BUTTON1_PRESSED) {
                app->viewport.left_click_held = true;
                if (app->viewport.is_dragging) {
                    app->viewport.last_mouse_screen_x = sx;
                    app->viewport.last_mouse_screen_y = sy;
                }
            }
            if (event.bstate & BUTTON1_RELEASED) {
                app->viewport.left_click_held = false;
            }

            if (event.bstate & REPORT_MOUSE_POSITION) {
                if (app->viewport.left_click_held) {
                    int world_x = (sx >= CANVAS_START_X) ? (sx - CANVAS_START_X) + app->viewport.offset_x : -9999;
                    int world_y = (sy >= 3) ? (sy - 3) + app->viewport.offset_y : -9999;
                    if (!app->viewport.is_dragging) {
                        if (world_x >= 0 && world_y >= 0) {
                            tart_draw_at(app, world_x, world_y, app->viewport.current_brush);
                        }
                    } else {
                        if (app->viewport.last_mouse_screen_x != 0 || app->viewport.last_mouse_screen_y != 0) {
                            int dx = sx - app->viewport.last_mouse_screen_x;
                            int dy = sy - app->viewport.last_mouse_screen_y;
                            app->viewport.offset_x -= dx;
                            app->viewport.offset_y -= dy;
                        }
                        app->viewport.last_mouse_screen_x = sx;
                        app->viewport.last_mouse_screen_y = sy;
                    }
                } else if (app->viewport.space_held) {
                    int world_x = (sx >= CANVAS_START_X) ? (sx - CANVAS_START_X) + app->viewport.offset_x : -9999;
                    int world_y = (sy >= 3) ? (sy - 3) + app->viewport.offset_y : -9999;
                    if (!app->viewport.is_dragging) {
                        if (world_x >= 0 && world_y >= 0) {
                            tart_draw_at(app, world_x, world_y, app->viewport.current_brush);
                        }
                    } else {
                        if (app->viewport.last_mouse_screen_x != 0 || app->viewport.last_mouse_screen_y != 0) {
                            int dx = sx - app->viewport.last_mouse_screen_x;
                            int dy = sy - app->viewport.last_mouse_screen_y;
                            app->viewport.offset_x -= dx;
                            app->viewport.offset_y -= dy;
                        }
                        app->viewport.last_mouse_screen_x = sx;
                        app->viewport.last_mouse_screen_y = sy;
                    }
                } else if (app->viewport.is_dragging) {
                    if (app->viewport.last_mouse_screen_x != 0 || app->viewport.last_mouse_screen_y != 0) {
                        int dx = sx - app->viewport.last_mouse_screen_x;
                        int dy = sy - app->viewport.last_mouse_screen_y;
                        app->viewport.offset_x -= dx;
                        app->viewport.offset_y -= dy;
                    }
                    app->viewport.last_mouse_screen_x = sx;
                    app->viewport.last_mouse_screen_y = sy;
                }
            }

            if (event.bstate & BUTTON4_PRESSED || event.bstate & BUTTON5_PRESSED) {
                int delta = (event.bstate & BUTTON4_PRESSED) ? -4 : 4;
                if (event.bstate & BUTTON_SHIFT) {
                    app->viewport.offset_x += delta;
                } else {
                    app->viewport.offset_y += delta;
                }
            }

            if (sx < CANVAS_START_X && sy >= 3 && sy < LINES - 1) {
                if (sy == 6) app->viewport.current_brush = '#';
                else if (sy == 7) app->viewport.current_brush = ' ';
            }

            if (sx >= CANVAS_START_X && sy >= 3 && sy < LINES - 1 && !app->viewport.space_held && !app->viewport.left_click_held) {
                int world_x = (sx - CANVAS_START_X) + app->viewport.offset_x;
                int world_y = (sy - 3) + app->viewport.offset_y;
                if (event.bstate & (BUTTON1_PRESSED | REPORT_MOUSE_POSITION)) {
                    if (!app->viewport.is_dragging) {
                        tart_draw_at(app, world_x, world_y, app->viewport.current_brush);
                    }
                }
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