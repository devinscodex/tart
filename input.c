#include "tart.h"

static void tart_open_palette(App *app, PaletteTarget target) {
    Viewport *vp = &app->viewport;
    vp->palette_visible = true;
    vp->palette_target = target;
    vp->palette_hover_row = 0;
    vp->palette_hover_char = 0;
}

static int tart_cycle_size(int size, int dir) {
    int idx = 0;
    for (int i = 0; i < (int)TART_BRUSH_SIZE_COUNT; i++) if (TART_BRUSH_SIZES[i] == size) idx = i;
    idx += dir;
    if (idx < 0) idx = 0;
    if (idx > (int)TART_BRUSH_SIZE_COUNT - 1) idx = (int)TART_BRUSH_SIZE_COUNT - 1;
    return TART_BRUSH_SIZES[idx];
}

/* Executes whatever a tool-panel row (or its keyboard equivalent) does. The
 * size/shape actions apply to whichever of brush_size/eraser_size is active
 * for the current tool, so pencil and eraser keep independent settings. */
static void tart_apply_panel_action(App *app, TartPanelAction action, int value) {
    Viewport *vp = &app->viewport;
    switch (action) {
        case PANEL_SELECT_TOOL:
            vp->tool = (TartTool)value;
            break;
        case PANEL_CHAR_OPEN:
            tart_open_palette(app, PALETTE_TARGET_PENCIL);
            break;
        case PANEL_FILL_CHAR_OPEN:
            tart_open_palette(app, PALETTE_TARGET_FILL);
            break;
        case PANEL_SIZE_DEC:
            if (vp->tool == TOOL_ERASER) vp->eraser_size = tart_cycle_size(vp->eraser_size, -1);
            else vp->brush_size = tart_cycle_size(vp->brush_size, -1);
            break;
        case PANEL_SIZE_INC:
            if (vp->tool == TOOL_ERASER) vp->eraser_size = tart_cycle_size(vp->eraser_size, 1);
            else vp->brush_size = tart_cycle_size(vp->brush_size, 1);
            break;
        case PANEL_SIZE_SET:
            if (value >= 0 && value < (int)TART_BRUSH_SIZE_COUNT) {
                if (vp->tool == TOOL_ERASER) vp->eraser_size = TART_BRUSH_SIZES[value];
                else vp->brush_size = TART_BRUSH_SIZES[value];
            }
            break;
        case PANEL_SHAPE_TOGGLE:
            if (vp->tool == TOOL_ERASER) vp->eraser_shape = (vp->eraser_shape == BRUSH_SQUARE) ? BRUSH_ROUND : BRUSH_SQUARE;
            else vp->brush_shape = (vp->brush_shape == BRUSH_SQUARE) ? BRUSH_ROUND : BRUSH_SQUARE;
            break;
        case PANEL_RECT_FILL_TOGGLE:
            vp->rect_filled = !vp->rect_filled;
            break;
        case PANEL_MOVE_CUT_TOGGLE:
            vp->move_cut = !vp->move_cut;
            break;
        case PANEL_COLOR_OPEN:
            vp->color_picker_visible = true;
            vp->color_slider_focus = 0;
            break;
        case PANEL_COLOR_SET:
            if (value >= 0 && value < (int)TART_BASIC_COLOR_COUNT) {
                vp->quick_color_idx = value;
                vp->current_color = TART_BASIC_COLORS[value];
            }
            break;
        default:
            break;
    }
}

/* Case-insensitive match against each top menu's mnemonic (its first
 * letter), for Alt+<letter> access. Returns -1 if c isn't any menu's letter. */
static int tart_menu_mnemonic_index(int c) {
    for (size_t i = 0; i < TART_MENU_COUNT; i++) {
        if (tolower(c) == tolower((unsigned char)TART_MENUS[i].label[0])) return (int)i;
    }
    return -1;
}

/* Every vim-motion key moves vim_cursor_x/y then re-syncs the mouse-cursor
 * shadow used for rendering the highlighted cell, and flags that the
 * viewport may need to scroll to keep it in view. Clamped to the canvas
 * plus the same PAN_VOID_MARGIN the view itself can pan into - vim motions
 * can wander into the dotted "void", just not endlessly. */
static void tart_move_cursor(App *app, bool *moved) {
    Viewport *vp = &app->viewport;
    int x_lo = -PAN_VOID_MARGIN;
    int x_hi = app->width - 1 + PAN_VOID_MARGIN;
    int y_lo = -PAN_VOID_MARGIN;
    int y_hi = app->height - 1 + PAN_VOID_MARGIN;
    if (vp->vim_cursor_x < x_lo) vp->vim_cursor_x = x_lo;
    if (vp->vim_cursor_x > x_hi) vp->vim_cursor_x = x_hi;
    if (vp->vim_cursor_y < y_lo) vp->vim_cursor_y = y_lo;
    if (vp->vim_cursor_y > y_hi) vp->vim_cursor_y = y_hi;
    vp->current_mouse_world_x = vp->vim_cursor_x;
    vp->current_mouse_world_y = vp->vim_cursor_y;
    *moved = true;
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
    free(vp->sel_colors);
    vp->sel_colors = NULL;
    vp->sel_w = 0;
    vp->sel_h = 0;
    vp->has_selection = false;
}

/* Bound panning to "canvas plus a small margin of void" - shared by the
 * end-of-frame clamp in handle_input and anything that changes canvas
 * dimensions (resize, load), since either can leave offset_x/y stale. */
static void tart_clamp_view(App *app) {
    Viewport *vp = &app->viewport;
    int view_w = CANVAS_VIEW_W;
    int view_h = CANVAS_VIEW_H;
    int h_lo = -PAN_VOID_MARGIN;
    int h_hi = (app->width > view_w) ? (app->width - view_w + PAN_VOID_MARGIN) : PAN_VOID_MARGIN;
    int v_lo = -PAN_VOID_MARGIN;
    int v_hi = (app->height > view_h) ? (app->height - view_h + PAN_VOID_MARGIN) : PAN_VOID_MARGIN;
    if (vp->offset_x < h_lo) vp->offset_x = h_lo;
    if (vp->offset_x > h_hi) vp->offset_x = h_hi;
    if (vp->offset_y < v_lo) vp->offset_y = v_lo;
    if (vp->offset_y > v_hi) vp->offset_y = v_hi;
}

static void tart_open_prompt(App *app, TartPromptMode mode, const char *initial) {
    Viewport *vp = &app->viewport;
    vp->prompt_mode = mode;
    strncpy(vp->prompt_buffer, initial, sizeof(vp->prompt_buffer) - 1);
    vp->prompt_buffer[sizeof(vp->prompt_buffer) - 1] = '\0';
    vp->prompt_len = (int)strlen(vp->prompt_buffer);
}

static bool tart_parse_wh(const char *s, int *w, int *h) {
    int a = 0, b = 0;
    if (sscanf(s, "%dx%d", &a, &b) != 2 && sscanf(s, "%d %d", &a, &b) != 2) return false;
    if (a <= 0 || b <= 0) return false;
    *w = a;
    *h = b;
    return true;
}

/* Single dispatch point for every named command, reached from the menu bar,
 * the ':' palette, and Alt+letter menu selection alike - arg is the text
 * after the command's first space (e.g. a filename or "WxHeight"), or NULL
 * when none was given, in which case save/load/resize fall back to opening
 * their own prompt instead. */
static void tart_execute_command(App *app, const char *name, const char *arg) {
    if (strcmp(name, "quit") == 0) {
        app->running = 0;
    } else if (strcmp(name, "help") == 0) {
        app->viewport.help_visible = true;
    } else if (strcmp(name, "about") == 0) {
        app->viewport.about_visible = true;
    } else if (strcmp(name, "deselect") == 0) {
        tart_deselect(app);
    } else if (strcmp(name, "reset-view") == 0) {
        tart_reset_view(app);
    } else if (strcmp(name, "toggle-pan") == 0) {
        tart_toggle_pan(app);
    } else if (strcmp(name, "resize") == 0) {
        int w, h;
        if (arg && tart_parse_wh(arg, &w, &h)) {
            tart_resize_doc(app, w, h);
            tart_clamp_view(app);
        } else {
            tart_open_prompt(app, PROMPT_RESIZE, "");
        }
    } else if (strcmp(name, "save") == 0) {
        if (arg) tart_save_doc(app, arg);
        else tart_open_prompt(app, PROMPT_SAVE, "untitled.tart");
    } else if (strcmp(name, "load") == 0) {
        if (arg) {
            if (tart_load_doc(app, arg)) tart_clamp_view(app);
        } else {
            tart_open_prompt(app, PROMPT_LOAD, "untitled.tart");
        }
    }
}

/* Rect tool: drag from press to release defines the rectangle, stamped onto
 * the canvas only on release (drag_start/drag_cur meanwhile drive the live
 * dashed preview drawn in render()). */
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
                if (border) {
                    tart_draw_at(app, x, y, vp->brush_char);
                    tart_set_color(app, x, y, vp->current_color);
                } else if (vp->rect_filled) {
                    tart_draw_at(app, x, y, vp->fill_char);
                    tart_set_color(app, x, y, vp->current_color);
                }
            }
        }
        vp->tool_dragging = false;
    }
}

/* Select tool: drag a rectangle; on release, copy its cells+colors into
 * sel_buffer/sel_colors so the move tool has something to stamp elsewhere. */
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
        free(vp->sel_colors);
        vp->sel_buffer = malloc(sizeof(wchar_t) * (size_t)w * h);
        vp->sel_colors = malloc((size_t)w * h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                vp->sel_buffer[y * w + x] = tart_read_cell(app, x0 + x, y0 + y);
                vp->sel_colors[y * w + x] = tart_read_color(app, x0 + x, y0 + y);
            }
        }
        vp->sel_w = w;
        vp->sel_h = h;
        vp->sel_origin_wx = x0;
        vp->sel_origin_wy = y0;
        vp->has_selection = true;
        vp->tool_dragging = false;
    }
}

/* Move tool: needs an existing selection (from tart_tool_select); dragging
 * previews the buffer following the cursor. In stamp mode (default,
 * move_cut == false) release pastes a copy and the selection stays put -
 * repeatable, source untouched. In cut mode release also clears the
 * source region and drops the selection - a one-shot true move. */
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
                tart_set_color(app, wx + x, wy + y, vp->sel_colors[y * vp->sel_w + x]);
            }
        }
        if (vp->move_cut) {
            for (int y = 0; y < vp->sel_h; y++) {
                for (int x = 0; x < vp->sel_w; x++) {
                    tart_draw_at(app, vp->sel_origin_wx + x, vp->sel_origin_wy + y, L' ');
                    tart_set_color(app, vp->sel_origin_wx + x, vp->sel_origin_wy + y, 0);
                }
            }
            tart_deselect(app);
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
            tart_execute_command(app, menu->items[item_idx], NULL);
        }
        vp->open_menu = -1;
        return true;
    }

    return false;
}

/* mid/right press-release and ctrl/alt are decoded here but not consumed by
 * any tool yet; kept in the signature so a future tool binding doesn't have
 * to touch the SGR/getmouse decode paths again to get at them. */
static void tart_apply_mouse(App *app, int sx, int sy,
                              bool left_press, bool left_release,
                              bool mid_press, bool mid_release,
                              bool right_press, bool right_release,
                              bool wheel_up, bool wheel_down,
                              bool wheel_left, bool wheel_right,
                              bool shift, bool ctrl, bool alt) {
    (void)mid_press; (void)mid_release; (void)right_press; (void)right_release;
    (void)ctrl;
    Viewport *vp = &app->viewport;

    if (left_press)    vp->left_click_held = true;
    if (left_release)  vp->left_click_held = false;

    /* A prompt owns all interaction while open (same as an open dropdown
     * menu already does via the click-away/dismiss path below) - button
     * state above is updated so press/release stay balanced, but nothing
     * past this point should happen while the user is mid-typing. */
    if (vp->prompt_mode != PROMPT_NONE) return;

    if (vp->command_mode) {
        TartRect menu_r, ruler_r, tool_r, y_ruler_r, canvas_r, status_r;
        tart_compute_layout(app, &menu_r, &ruler_r, &tool_r, &y_ruler_r, &canvas_r, &status_r);

        char verb[64];
        tart_command_verb(vp->command_buffer, verb, sizeof(verb));
        int match_idx[TART_COMMAND_COUNT];
        int match_count = tart_command_matches(verb, match_idx);
        int visible = (match_count < CMD_PALETTE_MAX_VISIBLE) ? match_count : CMD_PALETTE_MAX_VISIBLE;
        int popup_h = (visible > 0) ? visible : 1;
        int popup_y = status_r.y - popup_h;
        if (popup_y < 0) popup_y = 0;

        if (wheel_up || wheel_down) {
            if (match_count > 0) {
                int dir = wheel_up ? -1 : 1;
                vp->cmd_selected = (vp->cmd_selected + dir + match_count) % match_count;
            }
            return;
        }

        if (left_press && sy >= popup_y && sy < popup_y + visible) {
            int mi = vp->cmd_scroll_off + (sy - popup_y);
            if (mi >= 0 && mi < match_count) {
                const char *buf = vp->command_buffer;
                const char *space = strchr(buf, ' ');
                const char *arg = NULL;
                if (space) {
                    arg = space + 1;
                    while (*arg == ' ') arg++;
                    if (*arg == '\0') arg = NULL;
                }
                tart_execute_command(app, TART_COMMANDS[match_idx[mi]].name, arg);
                vp->command_mode = 0;
                vp->mode = 'n';
                vp->command_len = 0;
                vp->cmd_selected = 0;
                vp->cmd_scroll_off = 0;
                memset(vp->command_buffer, 0, sizeof(vp->command_buffer));
            }
        }
        return;
    }

    if (vp->palette_visible) {
        int px, py, pw, ph, max_chars;
        tart_compute_palette_geom(&px, &py, &pw, &ph, &max_chars);
        int content_y = py + 2;
        int n = (int)TART_PALETTE_CAT_COUNT;
        int rel_row = sy - content_y;
        bool inside_popup = (sx >= px && sx < px + pw && sy >= py && sy < py + ph);

        if (rel_row >= 0 && rel_row < n) {
            int clen = (int)wcslen(TART_PALETTE[rel_row].chars);
            int rel_char = (sx - (px + 2 + PALETTE_LABEL_WIDTH)) / PALETTE_CHAR_CELL_W;
            if (rel_char >= 0 && rel_char < clen) {
                vp->palette_hover_row = rel_row;
                vp->palette_hover_char = rel_char;
                if (left_press) {
                    wchar_t chosen = TART_PALETTE[rel_row].chars[rel_char];
                    if (vp->palette_target == PALETTE_TARGET_FILL) vp->fill_char = chosen;
                    else vp->brush_char = chosen;
                    vp->palette_visible = false;
                }
                return;
            }
        } else if (rel_row == n) {
            vp->palette_hover_row = n;
            if (left_press) {
                vp->palette_visible = false;
                tart_open_prompt(app, PROMPT_CUSTOM_CHAR, "");
            }
            return;
        }

        if (left_press && !inside_popup) {
            vp->palette_visible = false;
        }
        return;
    }

    if (vp->color_picker_visible) {
        int px, py, pw, ph, bar_x, bar_w;
        tart_compute_color_picker_geom(&px, &py, &pw, &ph, &bar_x, &bar_w);
        bool inside = (sx >= px && sx < px + pw && sy >= py && sy < py + ph);

        int slider = sy - (py + 3);
        if (slider >= 0 && slider < 3 && sx >= bar_x && sx < bar_x + bar_w) {
            if (vp->left_click_held) {
                int max_v = (slider == 0) ? 360 : 100;
                int value = (sx - bar_x) * max_v / (bar_w - 1);
                if (value < 0) value = 0;
                if (value > max_v) value = max_v;
                if (slider == 0) vp->hsl_h = value;
                else if (slider == 1) vp->hsl_s = value;
                else vp->hsl_l = value;
                vp->color_slider_focus = slider;
            }
            return;
        }

        int swatch_row = py + 7;
        if (sy == swatch_row && sx >= px + 11 && sx < px + 19) {
            if (left_press) {
                int r, g, b;
                tart_hsl_to_rgb(vp->hsl_h, vp->hsl_s, vp->hsl_l, &r, &g, &b);
                vp->current_color = (unsigned char)tart_rgb_to_256(r, g, b);
                vp->color_picker_visible = false;
            }
            return;
        }

        if (left_press && !inside) {
            vp->color_picker_visible = false;
        }
        return;
    }

    /* Wheel pans the view. Horizontal arrives several ways, all handled:
     * native horizontal-wheel events (SGR buttons 66/67, from a tilt wheel);
     * Shift+wheel with the shift bit, from terminals that pass it through;
     * and Alt+wheel - the reliable modifier, because xterm-family terminals
     * (Windows Terminal included) reserve Shift+mouse for making a normal
     * text selection past the app's mouse capture, and may swallow the
     * event entirely instead of reporting it. */
    if (wheel_up || wheel_down || wheel_left || wheel_right) {
        if (wheel_left)       vp->offset_x -= 4;
        else if (wheel_right) vp->offset_x += 4;
        else if (shift || alt) vp->offset_x += wheel_up ? -4 : 4;
        else                  vp->offset_y += wheel_up ? -4 : 4;
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
    vp->current_mouse_world_x = wx;
    vp->current_mouse_world_y = wy;
    vp->vim_cursor_x = wx;
    vp->vim_cursor_y = wy;

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
                if (acting) tart_stamp(app, wx, wy, vp->brush_size, vp->brush_shape, vp->brush_char, vp->current_color);
                break;
            case TOOL_ERASER:
                if (acting) tart_stamp(app, wx, wy, vp->eraser_size, vp->eraser_shape, L' ', 0);
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
    bool sgr_wheel_left = false, sgr_wheel_right = false;
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
                        /* Wheel events: low two bits pick the direction
                         * (0=up 1=down 2=left 3=right). */
                        int wheel_dir = cb & 3;
                        sgr_wheel_up    = (wheel_dir == 0);
                        sgr_wheel_down  = (wheel_dir == 1);
                        sgr_wheel_left  = (wheel_dir == 2);
                        sgr_wheel_right = (wheel_dir == 3);
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
        } else if (c1 != ERR) {
            /* Most terminals send Alt+<letter> as ESC followed immediately by
             * the letter, which is exactly what this zero-delay peek is
             * built to detect (see the SGR-mouse comment above). */
            int mi = tart_menu_mnemonic_index(c1);
            if (mi >= 0) {
                timeout(50);
                app->viewport.open_menu = mi;
                app->viewport.menu_hover_item = 0;
                return;
            }
            ungetch(c1);
        }
        timeout(50);
    }

    if (app->viewport.help_visible) {
        if (ch != KEY_MOUSE) {
            app->viewport.help_visible = false;
        }
        return;
    }

    if (app->viewport.about_visible) {
        if (ch != KEY_MOUSE) {
            app->viewport.about_visible = false;
        }
        return;
    }

    if (app->viewport.command_mode && ch != KEY_MOUSE) {
        Viewport *vp = &app->viewport;
        if (ch == '\n' || ch == KEY_ENTER) {
            const char *buf = vp->command_buffer;
            const char *space = strchr(buf, ' ');
            const char *arg = NULL;
            if (space) {
                arg = space + 1;
                while (*arg == ' ') arg++;
                if (*arg == '\0') arg = NULL;
            }
            char verb[64];
            tart_command_verb(buf, verb, sizeof(verb));

            int match_idx[TART_COMMAND_COUNT];
            int match_count = tart_command_matches(verb, match_idx);
            if (match_count > 0) {
                int sel = vp->cmd_selected;
                if (sel < 0 || sel >= match_count) sel = 0;
                tart_execute_command(app, TART_COMMANDS[match_idx[sel]].name, arg);
            }

            vp->command_mode = 0;
            vp->mode = 'n';
            vp->command_len = 0;
            vp->cmd_selected = 0;
            vp->cmd_scroll_off = 0;
            memset(vp->command_buffer, 0, sizeof(vp->command_buffer));
        } else if (ch == 27) {
            vp->command_mode = 0;
            vp->mode = 'n';
            vp->command_len = 0;
            vp->cmd_selected = 0;
            vp->cmd_scroll_off = 0;
            memset(vp->command_buffer, 0, sizeof(vp->command_buffer));
        } else if (ch == KEY_UP || ch == KEY_DOWN) {
            char verb[64];
            tart_command_verb(vp->command_buffer, verb, sizeof(verb));
            int match_idx[TART_COMMAND_COUNT];
            int match_count = tart_command_matches(verb, match_idx);
            if (match_count > 0) {
                int dir = (ch == KEY_UP) ? -1 : 1;
                vp->cmd_selected = (vp->cmd_selected + dir + match_count) % match_count;
            }
        } else if ((ch == KEY_BACKSPACE || ch == 127) && vp->command_len > 0) {
            vp->command_buffer[--vp->command_len] = '\0';
            vp->cmd_selected = 0;
            vp->cmd_scroll_off = 0;
        } else if (ch >= 32 && ch < 127 && vp->command_len < 255) {
            vp->command_buffer[vp->command_len++] = ch;
            vp->command_buffer[vp->command_len] = '\0';
            vp->cmd_selected = 0;
            vp->cmd_scroll_off = 0;
        }
        return;
    }

    if (app->viewport.prompt_mode != PROMPT_NONE && ch != KEY_MOUSE) {
        Viewport *vp = &app->viewport;
        if (ch == '\n' || ch == KEY_ENTER) {
            switch (vp->prompt_mode) {
                case PROMPT_RESIZE: {
                    int w, h;
                    if (tart_parse_wh(vp->prompt_buffer, &w, &h)) {
                        tart_resize_doc(app, w, h);
                        tart_clamp_view(app);
                    }
                    break;
                }
                case PROMPT_SAVE:
                    if (vp->prompt_buffer[0]) tart_save_doc(app, vp->prompt_buffer);
                    break;
                case PROMPT_LOAD:
                    if (vp->prompt_buffer[0] && tart_load_doc(app, vp->prompt_buffer)) tart_clamp_view(app);
                    break;
                case PROMPT_CUSTOM_CHAR: {
                    wchar_t code = (wchar_t)strtol(vp->prompt_buffer, NULL, 16);
                    if (code > 0) vp->brush_char = code;
                    break;
                }
                default:
                    break;
            }
            vp->prompt_mode = PROMPT_NONE;
            vp->prompt_len = 0;
            memset(vp->prompt_buffer, 0, sizeof(vp->prompt_buffer));
        } else if (ch == 27) {
            vp->prompt_mode = PROMPT_NONE;
            vp->prompt_len = 0;
            memset(vp->prompt_buffer, 0, sizeof(vp->prompt_buffer));
        } else if ((ch == KEY_BACKSPACE || ch == 127) && vp->prompt_len > 0) {
            vp->prompt_buffer[--vp->prompt_len] = '\0';
        } else if (ch >= 32 && ch < 127 && vp->prompt_len < 255) {
            vp->prompt_buffer[vp->prompt_len++] = ch;
            vp->prompt_buffer[vp->prompt_len] = '\0';
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
        } else if (ch == KEY_LEFT || ch == KEY_RIGHT) {
            int dir = (ch == KEY_LEFT) ? -1 : 1;
            app->viewport.open_menu = (app->viewport.open_menu + dir + (int)TART_MENU_COUNT) % (int)TART_MENU_COUNT;
            app->viewport.menu_hover_item = 0;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            tart_execute_command(app, TART_MENUS[app->viewport.open_menu].items[app->viewport.menu_hover_item], NULL);
            app->viewport.open_menu = -1;
        }
        return;
    }

    if (app->viewport.palette_visible && ch != KEY_MOUSE) {
        Viewport *vp = &app->viewport;
        int n = (int)TART_PALETTE_CAT_COUNT;
        if (ch == 27) {
            vp->palette_visible = false;
        } else if (ch == KEY_UP || ch == KEY_DOWN) {
            vp->palette_hover_row = (vp->palette_hover_row + (ch == KEY_UP ? -1 : 1) + (n + 1)) % (n + 1);
            if (vp->palette_hover_row < n) {
                int clen = (int)wcslen(TART_PALETTE[vp->palette_hover_row].chars);
                if (vp->palette_hover_char >= clen) vp->palette_hover_char = clen - 1;
            } else {
                vp->palette_hover_char = 0;
            }
        } else if (ch == KEY_LEFT || ch == KEY_RIGHT) {
            if (vp->palette_hover_row < n) {
                int clen = (int)wcslen(TART_PALETTE[vp->palette_hover_row].chars);
                vp->palette_hover_char = (vp->palette_hover_char + (ch == KEY_LEFT ? -1 : 1) + clen) % clen;
            }
        } else if (ch >= '1' && ch <= '9' && (ch - '1') < n) {
            /* Quick-jump straight to a category row by number; arrows/Enter
             * still pick the exact character from there. */
            vp->palette_hover_row = ch - '1';
            vp->palette_hover_char = 0;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (vp->palette_hover_row < n) {
                wchar_t chosen = TART_PALETTE[vp->palette_hover_row].chars[vp->palette_hover_char];
                if (vp->palette_target == PALETTE_TARGET_FILL) vp->fill_char = chosen;
                else vp->brush_char = chosen;
                vp->palette_visible = false;
            } else {
                vp->palette_visible = false;
                tart_open_prompt(app, PROMPT_CUSTOM_CHAR, "");
            }
        }
        return;
    }

    if (app->viewport.color_picker_visible && ch != KEY_MOUSE) {
        Viewport *vp = &app->viewport;
        if (ch == 27) {
            vp->color_picker_visible = false;
        } else if (ch == 9) {
            vp->color_slider_focus = (vp->color_slider_focus + 1) % 3;
        } else if (ch == KEY_LEFT || ch == KEY_RIGHT) {
            int dir = (ch == KEY_LEFT) ? -1 : 1;
            if (vp->color_slider_focus == 0) {
                vp->hsl_h = ((vp->hsl_h + dir * 5) % 360 + 360) % 360;
            } else if (vp->color_slider_focus == 1) {
                vp->hsl_s += dir * 5;
                if (vp->hsl_s < 0) vp->hsl_s = 0;
                if (vp->hsl_s > 100) vp->hsl_s = 100;
            } else {
                vp->hsl_l += dir * 5;
                if (vp->hsl_l < 0) vp->hsl_l = 0;
                if (vp->hsl_l > 100) vp->hsl_l = 100;
            }
        } else if (ch == '\n' || ch == KEY_ENTER) {
            int r, g, b;
            tart_hsl_to_rgb(vp->hsl_h, vp->hsl_s, vp->hsl_l, &r, &g, &b);
            vp->current_color = (unsigned char)tart_rgb_to_256(r, g, b);
            vp->color_picker_visible = false;
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
        int last_x = app->width - 1;
        int last_y = app->height - 1;
        app->viewport.vim_cursor_x = last_x;
        app->viewport.vim_cursor_y = last_y;
        app->viewport.current_mouse_world_x = last_x;
        app->viewport.current_mouse_world_y = last_y;

        int vis_w = CANVAS_VIEW_W;
        int vis_h = CANVAS_VIEW_H;
        if (last_x < app->viewport.offset_x || last_x > app->viewport.offset_x + vis_w - 1) {
            app->viewport.offset_x = last_x - vis_w / 2;
        }
        if (last_y < app->viewport.offset_y || last_y > app->viewport.offset_y + vis_h - 1) {
            app->viewport.offset_y = last_y - vis_h / 2;
        }
    }
    if (ch == KEY_F(1) || ch == '?') {
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
        tart_apply_panel_action(app, PANEL_CHAR_OPEN, 0);
    }
    if (ch == '-') {
        tart_apply_panel_action(app, PANEL_SIZE_DEC, 0);
    }
    if (ch == '=') {
        tart_apply_panel_action(app, PANEL_SIZE_INC, 0);
    }
    if (ch == '[' || ch == ']') {
        Viewport *vp = &app->viewport;
        int dir = (ch == '[') ? -1 : 1;
        vp->quick_color_idx = (vp->quick_color_idx + dir + (int)TART_BASIC_COLOR_COUNT) % (int)TART_BASIC_COLOR_COUNT;
        vp->current_color = TART_BASIC_COLORS[vp->quick_color_idx];
    }
    if (ch == 9) {
        tart_apply_panel_action(app, PANEL_SHAPE_TOGGLE, 0);
    }
    if (ch == 'f' && app->viewport.tool == TOOL_RECT) {
        tart_apply_panel_action(app, PANEL_RECT_FILL_TOGGLE, 0);
    }
    if (ch == 'c' && app->viewport.tool == TOOL_MOVE) {
        tart_apply_panel_action(app, PANEL_MOVE_CUT_TOGGLE, 0);
    }
    if (ch == 'v' || ch == 'V') {
        tart_toggle_pan(app);
    }
    if (ch == 'h') {
        app->viewport.vim_cursor_x--;
        tart_move_cursor(app, &vim_cursor_moved);
        app->viewport.mode = 'n';
    }
    if (ch == 'l') {
        app->viewport.vim_cursor_x++;
        tart_move_cursor(app, &vim_cursor_moved);
        app->viewport.mode = 'n';
    }
    if (ch == 'j') {
        app->viewport.vim_cursor_y++;
        tart_move_cursor(app, &vim_cursor_moved);
        app->viewport.mode = 'n';
    }
    if (ch == 'k') {
        app->viewport.vim_cursor_y--;
        tart_move_cursor(app, &vim_cursor_moved);
        app->viewport.mode = 'n';
    }
    if (ch == '0') {
        app->viewport.vim_cursor_x = 0;
        tart_move_cursor(app, &vim_cursor_moved);
    }
    if (ch == '$') {
        app->viewport.vim_cursor_x = app->width - 1;
        tart_move_cursor(app, &vim_cursor_moved);
    }
    if (ch == 'w') {
        app->viewport.vim_cursor_x += 5;
        tart_move_cursor(app, &vim_cursor_moved);
    }
    if (ch == 'b') {
        app->viewport.vim_cursor_x -= 5;
        tart_move_cursor(app, &vim_cursor_moved);
    }
    if (ch == 'G') {
        app->viewport.vim_cursor_y = app->height - 1;
        tart_move_cursor(app, &vim_cursor_moved);
    }
    if (ch == 'g') {
        app->viewport.vim_cursor_y = 0;
        tart_move_cursor(app, &vim_cursor_moved);
    }
    if (ch == 4) {
        app->viewport.offset_y += CANVAS_VIEW_H / 2;
        app->viewport.mode = 'n';
    }
    if (ch == 21) {
        app->viewport.offset_y -= CANVAS_VIEW_H / 2;
        app->viewport.mode = 'n';
    }
    if (ch == KEY_PPAGE) {
        app->viewport.offset_y -= CANVAS_VIEW_H;
        app->viewport.mode = 'n';
    }
    if (ch == KEY_NPAGE) {
        app->viewport.offset_y += CANVAS_VIEW_H;
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
                sgr_wheel_left, sgr_wheel_right,
                sgr_shift, sgr_ctrl, sgr_alt);
        } else {
            /* ncurses' mouse protocol (v1) has no horizontal-wheel buttons,
             * so wheel left/right only ever arrive via the SGR path above. */
            MEVENT event;
            if (getmouse(&event) == OK) {
                tart_apply_mouse(app, event.x, event.y,
                    (event.bstate & BUTTON1_PRESSED) != 0, (event.bstate & BUTTON1_RELEASED) != 0,
                    (event.bstate & BUTTON2_PRESSED) != 0, (event.bstate & BUTTON2_RELEASED) != 0,
                    (event.bstate & BUTTON3_PRESSED) != 0, (event.bstate & BUTTON3_RELEASED) != 0,
                    (event.bstate & BUTTON4_PRESSED) != 0, (event.bstate & BUTTON5_PRESSED) != 0,
                    false, false,
                    (event.bstate & BUTTON_SHIFT) != 0, (event.bstate & BUTTON_CTRL) != 0, (event.bstate & BUTTON_ALT) != 0);
            }
        }
    }

    if (vim_cursor_moved) {
        int visible_w = CANVAS_VIEW_W;
        int visible_h = CANVAS_VIEW_H;
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

    tart_clamp_view(app);
}
