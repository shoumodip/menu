#include <ctype.h>

#include "app.h"
#include "config.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

#define render_color(c)                                                                            \
    ((XRenderColor){                                                                               \
        .red = (((c) >> (2 * 8)) & 0xFF) << 8,                                                     \
        .green = (((c) >> (1 * 8)) & 0xFF) << 8,                                                   \
        .blue = (((c) >> (0 * 8)) & 0xFF) << 8,                                                    \
        .alpha = (((c) >> (3 * 8)) & 0xFF) << 8,                                                   \
    })

int app_init(App *a) {
    // Read Items
    {
        while (!feof(stdin)) {
            da_append_many(&a->buffer, NULL, 1024);
            a->buffer.count += fread(a->buffer.data + a->buffer.count, sizeof(char), 1024, stdin);
        }

        Str contents = str_new(a->buffer.data, a->buffer.count);
        while (contents.size) {
            Str line = str_split(&contents, '\n');
            if (line.size) {
                da_append(&a->items, line);
            }
        }

        if (a->items.count == 0) {
            return 0;
        }
    }

    // Initialize X11
    {
        a->display = XOpenDisplay(NULL);
        if (!a->display) {
            fprintf(stderr, "Error: could not open display\n");
            return 0;
        }
    }

    {
        a->font = XftFontOpenName(a->display, 0, FONT);
        if (!a->font) {
            fprintf(stderr, "Error: could not open font\n");
            return 0;
        }

        for (char ch = 32; ch < 127; ch++) {
            XGlyphInfo extents = {0};
            XftTextExtentsUtf8(a->display, a->font, (const FcChar8 *)&ch, 1, &extents);
            a->font_widths[ch - 32] = extents.xOff;
        }

        a->font_height = a->font->ascent + a->font->descent;
        a->item_height = a->font_height * 1.4;
    }

    // Create Window
    {
        const Window root = DefaultRootWindow(a->display);

        XWindowAttributes ra = {0};
        if (!XGetWindowAttributes(a->display, root, &ra)) {
            fprintf(stderr, "Error: could not get root window attributes\n");
            return 0;
        }
        a->window_width = ra.width * 0.6;
        a->window_height = a->item_height * (ITEMS + 1) + PADDING * 2;

        XSetWindowAttributes wa = {0};
        wa.override_redirect = True;
        wa.background_pixel = BACKGROUND_COLOR;
        wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                        KeyPressMask | VisibilityChangeMask | FocusChangeMask;

        int x = (ra.width - a->window_width) / 2;
        int y = (ra.height - a->window_height) / 2;

        a->window = XCreateWindow(
            a->display,
            root,
            x,
            y,
            a->window_width,
            a->window_height,
            2,
            CopyFromParent,
            CopyFromParent,
            CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWEventMask,
            &wa);

        XSetWindowBorder(a->display, a->window, BORDER_COLOR);

        XGrabKeyboard(a->display, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        XMapRaised(a->display, a->window);

        XGetInputFocus(a->display, &a->revert_window, &a->revert_return);
        XSetInputFocus(a->display, a->window, RevertToParent, CurrentTime);

        XSelectInput(a->display, root, SubstructureNotifyMask);
    }

    // Create Renderer
    {
        a->visual = DefaultVisual(a->display, 0);
        a->colormap = DefaultColormap(a->display, 0);

        XRenderColor background_color = render_color(BACKGROUND_COLOR);
        XftColorAllocValue(a->display, a->visual, a->colormap, &background_color, &a->colors[0]);

        XRenderColor foreground_color = render_color(FOREGROUND_COLOR);
        XftColorAllocValue(a->display, a->visual, a->colormap, &foreground_color, &a->colors[1]);

        a->gc = DefaultGC(a->display, 0);
        XSetLineAttributes(a->display, a->gc, PADDING, LineSolid, CapRound, JoinRound);

        a->draw = XftDrawCreate(a->display, a->window, a->visual, a->colormap);
        if (!a->draw) {
            fprintf(stderr, "Error: could not create Xft draw object\n");
            return 0;
        }
    }

    // Initialize Fzy
    {
        fzy_init();
        fzy_filter(
            &a->fzy, str_new(a->prompt.data, a->prompt.count), a->items.data, a->items.count);
    }

    return 1;
}

void app_free(App *a) {
    da_free(&a->items);
    da_free(&a->buffer);
    da_free(&a->prompt);
    fzy_free(&a->fzy);

    if (a->draw) {
        XftDrawDestroy(a->draw);
    }

    if (a->font) {
        XftFontClose(a->display, a->font);
    }

    if (a->display) {
        XSetInputFocus(a->display, a->revert_window, a->revert_return, CurrentTime);
        XftColorFree(a->display, a->visual, a->colormap, &a->colors[0]);
        XftColorFree(a->display, a->visual, a->colormap, &a->colors[1]);
        XDestroyWindow(a->display, a->window);
        XCloseDisplay(a->display);
    }
}

void app_line(App *a, int x, int y, Str str, XftColor *color) {
    y += a->font->ascent + (a->item_height - a->font_height) / 2;
    XftDrawString8(a->draw, color, a->font, x, y, (const FcChar8 *)str.data, str.size);
}

void app_draw(App *a) {
    XClearWindow(a->display, a->window);
    XSetForeground(a->display, a->gc, HIGHLIGHT_COLOR);

    int y = PADDING;
    int prompt_width = 0;
    for (size_t i = 0; i < a->prompt.count; i++) {
        prompt_width += a->font_widths[a->prompt.data[i] - 32];
    }

    int matches_found = a->items.count && !a->fzy.matches.count;
    if (matches_found) {
        XSetForeground(a->display, a->gc, NOMATCH_COLOR);
        XFillRectangle(
            a->display,
            a->window,
            a->gc,
            0,
            0,
            prompt_width + PADDING * 2,
            a->item_height + PADDING);
    }
    app_line(
        a, PADDING * 2, y, str_new(a->prompt.data, a->prompt.count), &a->colors[!matches_found]);

    int item_offset = (a->item_height - a->font_height) / 2;
    XSetForeground(a->display, a->gc, FOREGROUND_COLOR);
    XFillRectangle(
        a->display,
        a->window,
        a->gc,
        prompt_width + PADDING * 2 + 1,
        item_offset + PADDING,
        1,
        a->font_height);

    if (a->fzy.matches.count) {
        XSetForeground(a->display, a->gc, HIGHLIGHT_COLOR);
        XFillRectangle(
            a->display,
            a->window,
            a->gc,
            0,
            (a->current + 1 - a->anchor) * a->item_height + PADDING,
            a->window_width,
            a->item_height);

        y += a->item_height;
        for (size_t i = 0; i < min(a->fzy.matches.count - a->anchor, ITEMS); ++i) {
            Match match = a->fzy.matches.data[a->anchor + i];
            app_line(a, PADDING * 2, y, match.str, &a->colors[1]);

            for (size_t j = 0, p = 0, x = PADDING * 2; j < a->prompt.count; j++) {
                size_t k = match.positions[j];
                while (p < k) {
                    x += a->font_widths[match.str.data[p++] - 32];
                }

                int w = a->font_widths[match.str.data[k] - 32];
                XSetForeground(a->display, a->gc, MATCH_COLOR);
                XFillRectangle(a->display, a->window, a->gc, x, y, w, a->item_height);

                app_line(a, x, y, str_new(match.str.data + k, 1), &a->colors[0]);
            }

            y += a->item_height;
        }
    }
}

void app_sync(App *a) {
    a->anchor = 0;
    a->current = 0;
    fzy_filter(&a->fzy, str_new(a->prompt.data, a->prompt.count), a->items.data, a->items.count);
    app_draw(a);
}

void app_next(App *a) {
    if (a->fzy.matches.count) {
        a->current += 1;
        if (a->current == a->fzy.matches.count) {
            a->current = 0;
        }

        if (a->current >= a->anchor + ITEMS) {
            a->anchor = a->current - ITEMS + 1;
        }

        if (a->current < a->anchor) {
            a->anchor = a->current;
        }
        app_draw(a);
    }
}

void app_prev(App *a) {
    if (a->fzy.matches.count) {
        if (a->current == 0) {
            a->current = a->fzy.matches.count - 1;
        } else {
            a->current -= 1;
        }

        if (a->current >= a->anchor + ITEMS) {
            a->anchor = a->current - ITEMS + 1;
        }

        if (a->current < a->anchor) {
            a->anchor = a->current;
        }
        app_draw(a);
    }
}

void app_loop(App *a) {
    XEvent event = {0};
    while (!XNextEvent(a->display, &event)) {
        switch (event.type) {
        case Expose:
            app_draw(a);
            break;

        case FocusOut:
            XSetInputFocus(a->display, a->window, RevertToParent, CurrentTime);
            break;

        case VisibilityNotify:
            if (((XVisibilityEvent *)&event)->state != VisibilityUnobscured) {
                XRaiseWindow(a->display, a->window);
            }
            break;

        case ConfigureNotify:
            if (((XConfigureEvent *)&event)->window != a->window) {
                XRaiseWindow(a->display, a->window);
            }
            break;

        case ButtonPress:
            switch (event.xbutton.button) {
            case Button4:
                app_prev(a);
                break;

            case Button5:
                app_next(a);
                break;
            }
            break;

        case ButtonRelease:
            if (event.xbutton.button == Button1) {
                const size_t index = event.xbutton.y / a->item_height;
                if (index && a->anchor + index < a->fzy.matches.count + 1) {
                    Str current = a->fzy.matches.data[a->anchor + index - 1].str;
                    printf("%.*s\n", (int)current.size, current.data);
                    return;
                }
            }
            break;

        case MotionNotify: {
            const size_t index = event.xmotion.y / a->item_height;
            if (index && a->anchor + index < a->fzy.matches.count + 1) {
                a->current = a->anchor + index - 1;
                app_draw(a);
            }
        } break;

        case KeyPress: {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (event.xkey.state & ControlMask) {
                switch (key) {
                case 'c':
                    return;

                case 'n':
                    app_next(a);
                    break;

                case 'p':
                    app_prev(a);
                    break;

                case 'j':
                    if (a->prompt.count) {
                        printf("%.*s\n", (int)a->prompt.count, a->prompt.data);
                    }
                    return;
                }
            } else {
                switch (key) {
                case XK_Tab:
                    if (event.xkey.state & ShiftMask) {
                        app_prev(a);
                    } else {
                        app_next(a);
                    }
                    break;

                case XK_Escape:
                    return;

                case XK_Return:
                    if (a->fzy.matches.count) {
                        Str current = a->fzy.matches.data[a->current].str;
                        printf("%.*s\n", (int)current.size, current.data);
                    }
                    return;

                case XK_BackSpace:
                    if (a->prompt.count) {
                        if (event.xkey.state & Mod1Mask) {
                            while (a->prompt.count &&
                                   !isalnum(a->prompt.data[a->prompt.count - 1])) {
                                a->prompt.count--;
                            }

                            while (a->prompt.count &&
                                   isalnum(a->prompt.data[a->prompt.count - 1])) {
                                a->prompt.count--;
                            }
                        } else {
                            a->prompt.count--;
                        }
                        app_sync(a);
                    }
                    break;

                default:
                    key = XLookupKeysym(&event.xkey, event.xkey.state & ShiftMask);
                    if (32 <= key && key < 127) {
                        da_append(&a->prompt, key);
                        app_sync(a);
                    }
                    break;
                }
            }
        } break;
        }
    }
}
