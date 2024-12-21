#ifndef APP_H
#define APP_H

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include "fzy.h"

typedef struct {
    size_t anchor;
    size_t current;
    DynamicArray(Str) items;
    DynamicArray(char) buffer;
    DynamicArray(char) prompt;

    Fzy fzy;

    GC gc;
    Window window;
    Visual *visual;
    Display *display;
    Colormap colormap;

    XftDraw *draw;
    XftFont *font;
    XftColor colors[2];

    int font_height;
    int font_widths[127 - 32];

    int item_height;
    int window_width;
    int window_height;

    int revert_return;
    Window revert_window;
} App;

int app_init(App *a);
void app_free(App *a);
void app_loop(App *a);

#endif // APP_H
