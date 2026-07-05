#include "tart.h"

void show_help() {
    clear();
    printw("TART - Terminal ASCII Rendering Tool (+ VIM learning)\n\n");
    printw("MODE: n=normal | c=command | p=pan\n\n");
    printw("VIM motions (cursor highlight + auto follow, can roam workspace):\n");
    printw("  h j k l     Move cursor highlight one cell\n");
    printw("  w / b       Move cursor highlight +5 / -5 horizontally\n");
    printw("  0 / $       Cursor to start / end of line\n");
    printw("  G / g       Cursor to bottom / top of canvas\n");
    printw("  Ctrl+d / Ctrl+u   Pan view down / up half page\n");
    printw("  PageUp / PageDn   Pan view full page\n");
    printw("\nView & Pan:\n");
    printw("  Arrows        Pan viewport (single step)\n");
    printw("  v             Toggle pan mode (then Left drag to pan)\n");
    printw("  Wheel         Vertical pan (Shift+Wheel = horizontal)\n");
    printw("  Home          Reset view to (0,0) + padding\n");
    printw("  End           Bottom-right corner of canvas\n");
    printw("\nDraw & Tools:\n");
    printw("  Left click/drag   Draw continuous line (pencil/eraser)\n");
    printw("  p / e             Pencil / Eraser\n");
    printw("\nOther:\n");
    printw("  Ctrl+E        Enter command mode (type q / help / binds)\n");
    printw("  F1            This help screen\n");
    printw("  q             Quit\n\n");
    printw("Active mouse tracking: Mouse[] shows live mouse world position (canvas 0,0) as you move the mouse.\n");
    printw("Dead space: simple grid fills all area outside canvas.\n");
    printw("Press any key (keyboard only) to close...");
    refresh();
    int c;
    do {
        c = getch();
    } while (c == KEY_MOUSE);
}

void handle_input(App *app) {
    int ch = getch();
    if (ch == ERR) {
        return;  // timeout or no input - loop will re-render soon for live feel
    }

    bool vim_cursor_moved = false;

    if (ch == 'q' || ch == 'Q') {
        app->running = 0;
    } else if (ch == KEY_HOME) {
        app->viewport.offset_x = -2;
        app->viewport.offset_y = -2;
        app->viewport.current_mouse_world_x = 0;
        app->viewport.current_mouse_world_y = 0;
        app->viewport.vim_cursor_x = 0;
        app->viewport.vim_cursor_y = 0;
    } else if (ch == KEY_END) {
        int vis_w = COLS - CANVAS_START_X;
        int vis_h = LINES - 4;
        app->viewport.offset_x = app->canvas.width - vis_w;
        app->viewport.offset_y = app->canvas.height - vis_h;
        app->viewport.vim_cursor_x = app->canvas.width - 1;
        app->viewport.vim_cursor_y = app->canvas.height - 1;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
    } else if (ch == KEY_F(1)) {
        show_help();
    } else if (ch == 5) {
        app->viewport.command_mode = 1;
        app->viewport.mode = 'c';
        app->viewport.command_len = 0;
        memset(app->viewport.command_buffer, 0, sizeof(app->viewport.command_buffer));
    } else if (app->viewport.command_mode) {
        if (ch == '\n' || ch == KEY_ENTER) {
            if (strcmp(app->viewport.command_buffer, "q") == 0 || strcmp(app->viewport.command_buffer, "quit") == 0) {
                app->running = 0;
            } else if (strcmp(app->viewport.command_buffer, "help") == 0) {
                show_help();
            } else if (strcmp(app->viewport.command_buffer, "binds") == 0 || strcmp(app->viewport.command_buffer, "keybinds") == 0) {
                show_help();
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
    } else if (ch == 'e' || ch == 'E') {
        app->viewport.current_brush = ' ';
    } else if (ch == 'p' || ch == 'P') {
        app->viewport.current_brush = '#';
    } else if (ch == 'v' || ch == 'V') {
        app->viewport.is_dragging = !app->viewport.is_dragging;
        app->viewport.mode = app->viewport.is_dragging ? 'p' : 'n';
    } else if (ch == 'h') {
        app->viewport.vim_cursor_x--;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    } else if (ch == 'l') {
        app->viewport.vim_cursor_x++;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    } else if (ch == 'j') {
        app->viewport.vim_cursor_y++;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    } else if (ch == 'k') {
        app->viewport.vim_cursor_y--;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
        app->viewport.mode = 'n';
    } else if (ch == '0') {
        app->viewport.vim_cursor_x = 0;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    } else if (ch == '$') {
        app->viewport.vim_cursor_x = app->canvas.width - 1;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    } else if (ch == 'w') {
        app->viewport.vim_cursor_x += 5;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    } else if (ch == 'b') {
        app->viewport.vim_cursor_x -= 5;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    } else if (ch == 'G') {
        app->viewport.vim_cursor_y = app->canvas.height - 1;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    } else if (ch == 'g') {
        app->viewport.vim_cursor_y = 0;
        app->viewport.current_mouse_world_x = app->viewport.vim_cursor_x;
        app->viewport.current_mouse_world_y = app->viewport.vim_cursor_y;
        vim_cursor_moved = true;
    } else if (ch == 4) {
        app->viewport.offset_y += (LINES - 4) / 2;
        app->viewport.mode = 'n';
    } else if (ch == 21) {
        app->viewport.offset_y -= (LINES - 4) / 2;
        app->viewport.mode = 'n';
    } else if (ch == KEY_PPAGE) {
        app->viewport.offset_y -= LINES - 4;
        app->viewport.mode = 'n';
    } else if (ch == KEY_NPAGE) {
        app->viewport.offset_y += LINES - 4;
        app->viewport.mode = 'n';
    } else if (ch == KEY_LEFT) {
        app->viewport.offset_x -= 5;
    } else if (ch == KEY_RIGHT) {
        app->viewport.offset_x += 5;
    } else if (ch == KEY_UP) {
        app->viewport.offset_y -= 3;
    } else if (ch == KEY_DOWN) {
        app->viewport.offset_y += 3;
    } else if (ch == KEY_MOUSE) {
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
            }

            if (event.bstate & BUTTON1_PRESSED) {
                if (app->viewport.is_dragging) {
                    app->viewport.last_mouse_screen_x = sx;
                    app->viewport.last_mouse_screen_y = sy;
                }
            }

            if ((event.bstate & REPORT_MOUSE_POSITION) && app->viewport.is_dragging) {
                int dx = sx - app->viewport.last_mouse_screen_x;
                int dy = sy - app->viewport.last_mouse_screen_y;
                app->viewport.offset_x -= dx;
                app->viewport.offset_y -= dy;
                app->viewport.last_mouse_screen_x = sx;
                app->viewport.last_mouse_screen_y = sy;
            }

            if (event.bstate & BUTTON4_PRESSED) {
                if (event.bstate & BUTTON_SHIFT) {
                    app->viewport.offset_x -= 4;
                } else {
                    app->viewport.offset_y -= 4;
                }
            }
            if (event.bstate & BUTTON5_PRESSED) {
                if (event.bstate & BUTTON_SHIFT) {
                    app->viewport.offset_x += 4;
                } else {
                    app->viewport.offset_y += 4;
                }
            }

            if (sx < CANVAS_START_X && sy >= 3 && sy < LINES - 1) {
                if (sy == 6) app->viewport.current_brush = '#';
                else if (sy == 7) app->viewport.current_brush = ' ';
            }

            if (sx >= CANVAS_START_X && sy >= 3 && sy < LINES - 1) {
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