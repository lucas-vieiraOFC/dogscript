#include "dog_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(DOG_GUI_FALLBACK)
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif

typedef struct {
    int id;
    int win_id;
    int x;
    int y;
    int w;
    int h;
    DogString *text;
    int kind;
    void *callback;
} GuiWidget;

typedef struct {
    int id;
#if !defined(DOG_GUI_FALLBACK)
    SDL_Window *window;
    SDL_Renderer *renderer;
#endif
    DogString *title;
    int w;
    int h;
} GuiWindow;

static DogList *g_windows;
static DogList *g_widgets;
static int g_next_win = 1;
static int g_next_widget = 1;

static GuiWindow *find_window(int id) {
    size_t i;
    if (!g_windows) return NULL;
    for (i = 0; i < g_windows->len; i++) {
        GuiWindow *w = (GuiWindow *)g_windows->items[i];
        if (w && w->id == id) return w;
    }
    return NULL;
}

int dog_gui_init(void) {
    if (!g_windows) g_windows = dog_list_new();
    if (!g_widgets) g_widgets = dog_list_new();
#if defined(DOG_GUI_FALLBACK)
    fprintf(stderr, "[dog_gui] SDL2 not found: fallback mode active\n");
    return 1;
#else
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 0;
    TTF_Init();
    return 1;
#endif
}

int dog_gui_window_create(const char *title, int w, int h) {
    GuiWindow *gw = (GuiWindow *)calloc(1, sizeof(GuiWindow));
    gw->id = g_next_win++;
    gw->title = dog_string_new(title ? title : "DogScript");
    gw->w = w;
    gw->h = h;
#if !defined(DOG_GUI_FALLBACK)
    gw->window = SDL_CreateWindow(gw->title->data, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_HIDDEN);
    if (!gw->window) { free(gw); return 0; }
    gw->renderer = SDL_CreateRenderer(gw->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gw->renderer) { SDL_DestroyWindow(gw->window); free(gw); return 0; }
#endif
    dog_list_push(g_windows, gw);
    return gw->id;
}

void dog_gui_window_show(int handle) {
    GuiWindow *w = find_window(handle);
    if (!w) return;
#if defined(DOG_GUI_FALLBACK)
    printf("[dog_gui] show window #%d '%s' (%dx%d)\n", w->id, w->title->data, w->w, w->h);
#else
    SDL_ShowWindow(w->window);
#endif
}

void dog_gui_window_close(int handle) {
    GuiWindow *w = find_window(handle);
    if (!w) return;
#if !defined(DOG_GUI_FALLBACK)
    if (w->renderer) SDL_DestroyRenderer(w->renderer);
    if (w->window) SDL_DestroyWindow(w->window);
#endif
}

static int widget_create(int win, const char *text, int x, int y, int w, int h, int kind) {
    GuiWidget *gw = (GuiWidget *)calloc(1, sizeof(GuiWidget));
    gw->id = g_next_widget++;
    gw->win_id = win;
    gw->x = x;
    gw->y = y;
    gw->w = w;
    gw->h = h;
    gw->kind = kind;
    gw->text = dog_string_new(text ? text : "");
    dog_list_push(g_widgets, gw);
    return gw->id;
}

int dog_gui_button_create(int handle, const char *label, int x, int y, int w, int h) {
    return widget_create(handle, label, x, y, w, h, 1);
}

int dog_gui_label_create(int handle, const char *text, int x, int y) {
    return widget_create(handle, text, x, y, 100, 24, 2);
}

int dog_gui_input_create(int handle, const char *placeholder, int x, int y, int w, int h) {
    return widget_create(handle, placeholder, x, y, w, h, 3);
}

DogString *dog_gui_input_value(int id) {
    size_t i;
    for (i = 0; g_widgets && i < g_widgets->len; i++) {
        GuiWidget *w = (GuiWidget *)g_widgets->items[i];
        if (w && w->id == id) return dog_string_new(w->text->data);
    }
    return dog_string_new("");
}

void dog_gui_on_click(int id, void *callback) {
    size_t i;
    for (i = 0; g_widgets && i < g_widgets->len; i++) {
        GuiWidget *w = (GuiWidget *)g_widgets->items[i];
        if (w && w->id == id) {
            w->callback = callback;
            return;
        }
    }
}

void dog_gui_color(int r, int g, int b) {
    (void)r;
    (void)g;
    (void)b;
}

void dog_gui_alert(const char *msg) {
#if defined(DOG_GUI_FALLBACK)
    fprintf(stderr, "[dog_gui][alert] %s\n", msg ? msg : "");
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "DogScript", msg ? msg : "", NULL);
#endif
}

void dog_gui_loop(void) {
#if defined(DOG_GUI_FALLBACK)
    printf("[dog_gui] fallback loop (no SDL event loop)\n");
#else
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }
        SDL_Delay(16);
    }
#endif
}
