#include <vector>
#include <iostream>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#define GAP 6
#define PADDING 0.2
#define PEN_START (18+GAP)

#define HIGHLIGHT_COLOR 0xfffe8019
#define SEARCHBAR_COLOR 0xff3c3836
#define FOREGROUND_COLOR 0xffebdbb2
#define BACKGROUND_COLOR 0xff282828

void panic(const char *message)
{
    std::cerr << "error: " << message << std::endl;
    exit(1);
}

XftColor hex_to_color(Display *display, int hex)
{
    XRenderColor render_color;
    render_color.red = ((hex >> (2*8)) & 0xFF) << 8;
    render_color.green = ((hex >> (1*8)) & 0xFF) << 8;
    render_color.blue = ((hex >> (0*8)) & 0xFF) << 8;
    render_color.alpha = ((hex >> (3*8)) & 0xFF) << 8;

    XftColor color;
    XftColorAllocValue(display, DefaultVisual(display, 0), DefaultColormap(display, 0), &render_color, &color);
    return color;
}

struct Line {
    size_t pos;
    std::string str;

    void insert(char ch)
    {
        str.insert(pos++, 1, ch);
    }

    void prev_char()
    {
        if (pos) {
            pos--;
        }
    }

    void next_char()
    {
        if (pos < str.size()) {
            pos++;
        }
    }

    void prev_word()
    {
        while (pos && !isalnum(str[pos - 1])) {
            pos--;
        }

        while (pos && isalnum(str[pos - 1])) {
            pos--;
        }
    }

    void next_word()
    {
        while (pos < str.size() && !isalnum(str[pos])) {
            pos++;
        }

        while (pos < str.size() && isalnum(str[pos])) {
            pos++;
        }
    }

    void line_head()
    {
        pos = 0;
    }

    void line_tail()
    {
        pos = str.size();
    }

    bool delete_prev_char()
    {
        if (pos) {
            str.erase(--pos, 1);
            return true;
        }
        return false;
    }

    bool delete_next_char()
    {
        if (pos < str.size()) {
            str.erase(pos, 1);
            return true;
        }
        return false;
    }

    bool delete_prev_word()
    {
        const auto pos_init = pos;
        prev_word();

        if (pos_init != pos) {
            str.erase(pos, pos_init - pos);
            return true;
        }

        return false;
    }

    bool delete_next_word()
    {
        const auto pos_init = pos;
        next_word();

        if (pos_init != pos) {
            str.erase(pos_init, pos - pos_init);
            pos = pos_init;
            return true;
        }

        return false;
    }
};

struct Item {
    size_t count;
    std::string str;
};

struct Menu {
    size_t width;
    size_t height;

    int screen;
    Window menu;
    Window root;
    Display *display;

    GC gc;
    int pen;
    XftFont *font;
    XftDraw *draw;
    size_t items_shown;
    XftColor default_color;
    XftColor current_color;

    Line input;
    size_t current;
    std::vector<Item> items;
    std::vector<Item> matches;

    Menu()
    {
        display = XOpenDisplay(NULL);
        if (!display) {
            panic("cannot open display");
        }

        font = XftFontOpenName(display, 0, "JetBrains Mono:size=13");
        if (!font) {
            panic("cannot open font");
        }

        screen = DefaultScreen(display);
        root = RootWindow(display, screen);

        XWindowAttributes root_attr;
        if (!XGetWindowAttributes(display, root, &root_attr)) {
            panic("cannot get root window attributes");
        }

        width = root_attr.width * (1.0 - (PADDING * 2));
        height = font->ascent + font->descent + GAP;

        items_shown = root_attr.height * (1.0 - (PADDING * 2)) / height;

        XSetWindowAttributes menu_attr;
        menu_attr.override_redirect = True;
        menu_attr.background_pixel = BACKGROUND_COLOR;
        menu_attr.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

        menu = XCreateWindow(
            display, root,
            root_attr.width * PADDING, root_attr.height * PADDING,
            width, height * (items_shown + 1) + GAP,
            0, CopyFromParent, CopyFromParent, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWEventMask,
            &menu_attr);

        XGrabKeyboard(display, DefaultRootWindow(display), True, GrabModeAsync, GrabModeAsync, CurrentTime);
        XMapRaised(display, menu);

        gc = DefaultGC(display, screen);
        draw = XftDrawCreate(display, menu, DefaultVisual(display, 0), DefaultColormap(display, 0));

        current_color = hex_to_color(display, HIGHLIGHT_COLOR);
        default_color = hex_to_color(display, FOREGROUND_COLOR);

        current = 0;
        for (std::string line; std::getline(std::cin, line); ) {
            items.push_back(Item {std::min(line.size(), width / font->max_advance_width), line});
        }

        update();
    }

    ~Menu()
    {
        XftColorFree(display, DefaultVisual(display, 0), DefaultColormap(display, 0), &current_color);
        XftColorFree(display, DefaultVisual(display, 0), DefaultColormap(display, 0), &default_color);
        XDestroyWindow(display, menu);
        XCloseDisplay(display);
    }

    void line(const char *data, size_t count, XftColor *color)
    {
        XftDrawString8(draw, color, font, 0, pen, (XftChar8 *) data, count);
        pen += height;
    }

    void render()
    {
        XClearWindow(display, menu);
        pen = PEN_START;

        XSetForeground(display, gc, SEARCHBAR_COLOR);
        XFillRectangle(display, menu, gc, 0, 0, width, height + GAP);
        line(input.str.data(), std::min(input.str.size(), width / font->max_advance_width), &default_color);
        pen += GAP/2;

        for (size_t i = current - current % items_shown, count = 0; i < matches.size(); ++i) {
            line(matches[i].str.data(), matches[i].count, i == current ? &current_color : &default_color);
            if (++count == items_shown) {
                break;
            }
        }

        XSetForeground(display, gc, FOREGROUND_COLOR);
        XFillRectangle(display, menu, gc, input.pos * font->max_advance_width, GAP, 1, height - GAP);
    }

    void update()
    {
        current = 0;
        matches.clear();
        for (auto& item : items) {
            if (item.str.find(input.str) != std::string::npos) {
                matches.push_back(item);
            }
        }
    }

    void start()
    {
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
                    case 'c':
                        return;

                    case 'b':
                        input.prev_char();
                        break;

                    case 'f':
                        input.next_char();
                        break;

                    case 'a':
                        input.line_head();
                        break;

                    case 'e':
                        input.line_tail();
                        break;

                    case 'd':
                        if (input.delete_next_char()) {
                            update();
                        }
                        break;

                    case 'p':
                        if (matches.size()) {
                            current = current ? current - 1 : matches.size() - 1;
                        }
                        break;

                    case 'n':
                        current = ++current < matches.size() ? current : 0;
                        break;
                    }
                } else if (event.xkey.state & Mod1Mask) {
                    switch (key) {
                    case 'b':
                        input.prev_word();
                        break;

                    case 'f':
                        input.next_word();
                        break;

                    case XK_BackSpace:
                        if (input.delete_prev_word()) {
                            update();
                        }
                        break;

                    case 'd':
                        if (input.delete_next_word()) {
                            update();
                        }
                        break;
                    }
                } else {
                    switch (key) {
                    case XK_BackSpace:
                        if (input.delete_prev_char()) {
                            update();
                        }
                        break;

                    case XK_Return:
                        if (matches.size()) {
                            std::cout << matches[current].str << std::endl;
                        } else {
                            std::cout << input.str << std::endl;
                        }
                        return;

                    default:
                        if (32 <= key && key <= 126) {
                            input.insert(key);
                            update();
                        }
                        break;
                    }
                }

                render();
                break;
            }
        }
    }
};

int main()
{
    Menu().start();
}
