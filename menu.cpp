#include <vector>
#include <iostream>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#define FONT "Monospace:size=14"
#define ITEMS 10

#define HIGHLIGHT_COLOR 0xFF3E4452
#define FOREGROUND_COLOR 0xFFABB2BF
#define BACKGROUND_COLOR 0xFF282C34

void panic(const char *message) {
    std::cerr << "error: " << message << std::endl;
    exit(1);
}

struct Menu {
    int current;
    std::string pattern;
    std::vector<std::string> items;
    std::vector<std::string> matches;

    GC gc;
    int width;
    Window window;
    Visual *visual;
    Display *display;
    Colormap colormap;

    XftDraw *draw;
    XftFont *font;
    XftColor color;
    int font_height;

    Menu() {
        for (std::string line; std::getline(std::cin, line); ) {
            items.push_back(line);
        }

        display = XOpenDisplay(NULL);
        if (!display) {
            panic("cannot open display");
        }

        auto screen = DefaultScreen(display);
        auto root = RootWindow(display, screen);

        font = XftFontOpenName(display, screen, FONT);
        if (!font) {
            panic("cannot open font");
        }
        font_height = font->ascent + font->descent;

        XWindowAttributes root_attr;
        if (!XGetWindowAttributes(display, root, &root_attr)) {
            panic("cannot get root window attributes");
        }
        width = root_attr.width;

        XSetWindowAttributes window_attr;
        window_attr.override_redirect = True;
        window_attr.background_pixel = BACKGROUND_COLOR;
        window_attr.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

        window = XCreateWindow(
            display, root,
            0, 0,
            width, font_height * (ITEMS + 1),
            0, CopyFromParent, CopyFromParent, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWEventMask,
            &window_attr);

        XGrabKeyboard(display, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        XMapRaised(display, window);

        visual = DefaultVisual(display, 0);
        colormap = DefaultColormap(display, 0);

        gc = DefaultGC(display, screen);
        draw = XftDrawCreate(display, window, visual, colormap);

        XRenderColor render_color;
        render_color.alpha = ((FOREGROUND_COLOR >> (3*8)) & 0xFF) << 8;
        render_color.red = ((FOREGROUND_COLOR >> (2*8)) & 0xFF) << 8;
        render_color.green = ((FOREGROUND_COLOR >> (1*8)) & 0xFF) << 8;
        render_color.blue = ((FOREGROUND_COLOR >> (0*8)) & 0xFF) << 8;
        XftColorAllocValue(display, visual, colormap, &render_color, &color);

        update();
    }

    ~Menu() {
        XftColorFree(display, visual, colormap, &color);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
    }

    void draw_line(std::string& str, int& y) {
        XftDrawString8(draw, &color, font, 0, y, (FcChar8 *) str.data(), str.size());
        y += font_height;
    }

    void render() {
        XClearWindow(display, window);

        auto y = font->ascent;
        draw_line(pattern, y);

        XGlyphInfo extents;
        XftTextExtents8(display, font, (FcChar8 *) pattern.data(), pattern.size(), &extents);
        XSetForeground(display, gc, FOREGROUND_COLOR);
        XFillRectangle(display, window, gc, extents.width + 2, 0, 1, font_height);

        if (!matches.empty()) {
            auto begin = current - current % ITEMS;

            XSetForeground(display, gc, HIGHLIGHT_COLOR);
            XFillRectangle(display, window, gc, 0, (current + 1 - begin) * font_height, width, font_height);

            for (auto i = 0; i < std::min(int(matches.size() - begin), ITEMS); ++i) {
                draw_line(matches[begin + i], y);
            }
        }
    }

    void update() {
        current = 0;
        matches.clear();
        for (auto item : items) {
            if (item.find(pattern) != std::string::npos) {
                matches.push_back(item);
            }
        }
        render();
    }

    void run() {
        XEvent event;
        while (!XNextEvent(display, &event)) {
            switch (event.type) {
            case Expose:
                render();
                break;

            case KeyPress:
                auto key = XLookupKeysym(&event.xkey, event.xkey.state & ShiftMask);
                if (event.xkey.state & ControlMask) {
                    switch (key) {
                    case 'p':
                        if (!matches.empty()) {
                            if (current == 0) {
                                current = matches.size() - 1;
                            } else {
                                current -= 1;
                            }
                            render();
                        }
                        break;

                    case 'n':
                        if (!matches.empty()) {
                            current += 1;
                            if (current == int(matches.size())) {
                                current = 0;
                            }
                            render();
                        }
                        break;
                    }
                } else {
                    switch (key) {
                    case XK_Escape:
                        return;

                    case XK_Return:
                        if (!matches.empty()) {
                            std::cout << matches[current] << std::endl;
                        }
                        return;

                    case XK_BackSpace:
                        if (pattern.size() > 0) {
                            pattern.pop_back();
                            update();
                        }
                        break;

                    default:
                        if (32 <= key && key <= 126) {
                            pattern.push_back(key);
                            update();
                        }
                        break;
                    }
                }
                break;
            }
        }
    }
};

int main() {
    Menu().run();
}
