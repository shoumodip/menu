#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <assert.h>

// Configuration
#define FONT "JetBrains Mono:pixelsize=17"
#define ITEMS 10

#define MATCH_COLOR 0xFFA9B665
#define NOMATCH_COLOR 0xFFEA6962
#define HIGHLIGHT_COLOR 0xFF45403D
#define FOREGROUND_COLOR 0xFFD4BE98
#define BACKGROUND_COLOR 0xFF282828

// Misc
#define min(a, b) ((a) < (b) ? (a) : (b))

#define render_color(c)                                                                            \
  ((XRenderColor){                                                                                 \
    .red = (((c) >> (2 * 8)) & 0xFF) << 8,                                                         \
    .green = (((c) >> (1 * 8)) & 0xFF) << 8,                                                       \
    .blue = (((c) >> (0 * 8)) & 0xFF) << 8,                                                        \
    .alpha = (((c) >> (3 * 8)) & 0xFF) << 8,                                                       \
  })

// List
#define LIST_INIT_CAP 128

#define List(T)                                                                                    \
  struct {                                                                                         \
    T *data;                                                                                       \
    size_t count;                                                                                  \
    size_t capacity;                                                                               \
  }

#define list_free(l)                                                                               \
  do {                                                                                             \
    free((l)->data);                                                                               \
    memset((l), 0, sizeof(*(l)));                                                                  \
  } while (0)

#define list_append(l, v)                                                                          \
  do {                                                                                             \
    if ((l)->count >= (l)->capacity) {                                                             \
      (l)->capacity = (l)->capacity == 0 ? LIST_INIT_CAP : (l)->capacity * 2;                      \
      (l)->data = realloc((l)->data, (l)->capacity * sizeof(*(l)->data));                          \
      assert((l)->data);                                                                           \
    }                                                                                              \
                                                                                                   \
    (l)->data[(l)->count++] = (v);                                                                 \
  } while (0)

#define list_append_many(l, v, c)                                                                  \
  do {                                                                                             \
    if ((l)->count + (c) > (l)->capacity) {                                                        \
      if ((l)->capacity == 0) {                                                                    \
        (l)->capacity = LIST_INIT_CAP;                                                             \
      }                                                                                            \
                                                                                                   \
      while ((l)->count + (c) > (l)->capacity) {                                                   \
        (l)->capacity *= 2;                                                                        \
      }                                                                                            \
                                                                                                   \
      (l)->data = realloc((l)->data, (l)->capacity * sizeof(*(l)->data));                          \
      assert((l)->data);                                                                           \
    }                                                                                              \
                                                                                                   \
    if ((v) != NULL) {                                                                             \
      memcpy((l)->data + (l)->count, (v), (c) * sizeof(*(l)->data));                               \
      (l)->count += (c);                                                                           \
    }                                                                                              \
  } while (0)

// String
typedef struct {
  const char *data;
  size_t size;
} Str;

#define str_new(d, s) ((Str){.data = (d), .size = (s)})

int str_find(Str a, Str b) {
  if (!b.size) {
    return 0;
  }

  for (size_t i = 0; i < a.size; i++) {
    int found = i;
    for (size_t j = 0; j < b.size; j++) {
      if (tolower(a.data[i + j]) != tolower(b.data[j])) {
        found = -1;
        break;
      }
    }

    if (found != -1) {
      return found;
    }
  }

  return -1;
}

Str str_split(Str *str, char ch) {
  Str result = *str;
  const char *end = memchr(str->data, ch, str->size);

  if (end == NULL) {
    str->data += str->size;
    str->size = 0;
  } else {
    result.size = end - str->data;
    str->data += result.size + 1;
    str->size -= result.size + 1;
  }

  return result;
}

// Match
typedef struct {
  Str str;
  size_t start;
} Match;

// App
typedef struct {
  size_t current;
  List(Str) items;
  List(char) buffer;
  List(char) prompt;
  List(Match) matches;

  GC gc;
  Window window;
  Visual *visual;
  Display *display;
  Colormap colormap;

  XftDraw *draw;
  XftFont *font;
  XftColor colors[2];

  int font_height;
  int item_height;
  int window_width;

  int revert_return;
  Window revert_window;
} App;

int app_text_size(App *a, const char *data, size_t count) {
  XGlyphInfo extents = {0};
  XftTextExtentsUtf8(a->display, a->font, (const FcChar8 *)data, count, &extents);
  return extents.xOff;
}

void app_free(App *a) {
  list_free(&a->items);
  list_free(&a->buffer);
  list_free(&a->prompt);
  list_free(&a->matches);

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

int app_init(App *a) {
  // Initialize GUI
  {
    a->display = XOpenDisplay(NULL);
    if (!a->display) {
      fprintf(stderr, "Error: could not open display\n");
      return 0;
    }

    a->font = XftFontOpenName(a->display, DefaultScreen(a->display), FONT);
    if (!a->font) {
      fprintf(stderr, "Error: could not open font\n");
      return 0;
    }
    a->font_height = a->font->ascent + a->font->descent;
    a->item_height = a->font_height * 1.4;
  }

  // Create Window
  {
    Window root = DefaultRootWindow(a->display);

    XWindowAttributes ra = {0};
    if (!XGetWindowAttributes(a->display, root, &ra)) {
      fprintf(stderr, "Error: could not get root window attributes\n");
      return 0;
    }
    a->window_width = ra.width * 0.6;

    XSetWindowAttributes wa = {0};
    wa.override_redirect = True;
    wa.background_pixel = BACKGROUND_COLOR;
    wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

    int h = a->item_height * (ITEMS + 1);
    int x = (ra.width - a->window_width) / 2;
    int y = (ra.height - h) / 2;

    a->window =
      XCreateWindow(a->display, root, x, y, a->window_width, h, 2, CopyFromParent, CopyFromParent,
                    CopyFromParent, CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);

    XSetWindowBorder(a->display, a->window, HIGHLIGHT_COLOR);
    XGrabKeyboard(a->display, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XMapRaised(a->display, a->window);

    XGetInputFocus(a->display, &a->revert_window, &a->revert_return);
    XSetInputFocus(a->display, a->window, RevertToParent, CurrentTime);
  }

  // Create Renderer
  {
    a->visual = DefaultVisual(a->display, 0);
    a->colormap = DefaultColormap(a->display, 0);

    XRenderColor background_color = render_color(BACKGROUND_COLOR);
    XftColorAllocValue(a->display, a->visual, a->colormap, &background_color, &a->colors[0]);

    XRenderColor foreground_color = render_color(FOREGROUND_COLOR);
    XftColorAllocValue(a->display, a->visual, a->colormap, &foreground_color, &a->colors[1]);

    a->gc = DefaultGC(a->display, DefaultScreen(a->display));
    a->draw = XftDrawCreate(a->display, a->window, a->visual, a->colormap);
    if (!a->draw) {
      fprintf(stderr, "Error: could not create Xft draw object\n");
      return 0;
    }
  }

  // Read Items
  {
    while (!feof(stdin)) {
      list_append_many(&a->buffer, NULL, 1024);
      a->buffer.count += fread(a->buffer.data + a->buffer.count, sizeof(char), 1024, stdin);
    }

    Str contents = str_new(a->buffer.data, a->buffer.count);
    while (contents.size) {
      Str line = str_split(&contents, '\n');
      if (line.size) {
        list_append(&a->items, line);
        list_append(&a->matches, ((Match){line, 0}));
      }
    }
  }

  return 1;
}

void app_line(App *a, int x, int y, Str str, XftColor *color) {
  y += a->font->ascent + (a->item_height - a->font_height) / 2;
  XftDrawString8(a->draw, color, a->font, x, y, (const FcChar8 *)str.data, str.size);
}

void app_draw(App *a) {
  XClearWindow(a->display, a->window);

  int y = 0;
  int prompt_width = app_text_size(a, a->prompt.data, a->prompt.count);

  int matches_found = a->items.count && !a->matches.count;
  if (matches_found) {
    XSetForeground(a->display, a->gc, NOMATCH_COLOR);
    XFillRectangle(a->display, a->window, a->gc, 0, 0, prompt_width, a->item_height);
  }
  app_line(a, 0, y, str_new(a->prompt.data, a->prompt.count), &a->colors[!matches_found]);

  int item_offset = (a->item_height - a->font_height) / 2;
  XSetForeground(a->display, a->gc, FOREGROUND_COLOR);
  XFillRectangle(a->display, a->window, a->gc, prompt_width + 1, item_offset, 1, a->font_height);

  if (a->matches.count) {
    size_t begin = a->current - a->current % ITEMS;

    XSetForeground(a->display, a->gc, HIGHLIGHT_COLOR);
    XFillRectangle(a->display, a->window, a->gc, 0, (a->current + 1 - begin) * a->item_height,
                   a->window_width, a->item_height);

    y += a->item_height;
    for (size_t i = 0; i < min(a->matches.count - begin, ITEMS); ++i) {
      Match match = a->matches.data[begin + i];
      app_line(a, 0, y, match.str, &a->colors[1]);

      int start_width = app_text_size(a, match.str.data, match.start);
      int match_width = app_text_size(a, match.str.data + match.start, a->prompt.count);
      XSetForeground(a->display, a->gc, MATCH_COLOR);
      XFillRectangle(a->display, a->window, a->gc, start_width, y, match_width, a->item_height);

      Str highlight = str_new(match.str.data + match.start, a->prompt.count);
      app_line(a, start_width, y, highlight, &a->colors[0]);
      y += a->item_height;
    }
  }
}

void app_sync(App *a) {
  a->current = 0;
  a->matches.count = 0;
  for (size_t i = 0; i < a->items.count; i++) {
    Str item = a->items.data[i];
    int start = str_find(item, str_new(a->prompt.data, a->prompt.count));
    if (start != -1) {
      list_append(&a->matches, ((Match){item, start}));
    }
  }
  app_draw(a);
}

void app_next(App *a) {
  if (a->matches.count) {
    a->current += 1;
    if (a->current == a->matches.count) {
      a->current = 0;
    }
    app_draw(a);
  }
}

void app_prev(App *a) {
  if (a->matches.count) {
    if (a->current == 0) {
      a->current = a->matches.count - 1;
    } else {
      a->current -= 1;
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
          if (a->matches.count) {
            Str current = a->matches.data[a->current].str;
            printf("%.*s\n", (int)current.size, current.data);
          }
          return;

        case XK_BackSpace:
          if (a->prompt.count) {
            a->prompt.count--;
            app_sync(a);
          }
          break;

        default:
          key = XLookupKeysym(&event.xkey, event.xkey.state & ShiftMask);
          if (32 <= key && key <= 126) {
            list_append(&a->prompt, key);
            app_sync(a);
          }
          break;
        }
      }
    } break;
    }
  }
}

// Main
int main(void) {
  App app = {0};
  if (!app_init(&app)) {
    app_free(&app);
    return 1;
  }

  app_loop(&app);
  app_free(&app);
  return 0;
}
