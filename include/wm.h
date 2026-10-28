#ifndef WM_H
#define WM_H

#include <stdint.h>

#define WM_TASKBAR_H 30
#define WM_TITLE_H   22
#define WM_MAXW      16

typedef struct window {
    int x, y, w, h;

    /* saved geometry while maximized */
    int ox, oy, ow, oh;
    int maximized;

    char title[48];

    int visible;
    int focused;
    int dragging;
    int drag_off_x, drag_off_y;
    int app_id;

    void (*draw)(struct window *w);
    void (*click)(struct window *w, int lx, int ly, int button);
    int  (*key)(struct window *w, int key);  /* 1 = key consumed */
    int  (*tick)(struct window *w);   /* 1 = needs redraw */
} window_t;

void wm_init(void);
window_t *wm_new(int x, int y, int w, int h, const char *title);
void wm_show(window_t *w);
void wm_close(window_t *w);
void wm_focus(window_t *w);
void wm_draw_windows(void);

window_t *wm_at(int x, int y);
int  wm_press(int x, int y, int button);   /* 1 if a window consumed it */
int  wm_drag_to(int x, int y);
void wm_release(void);

window_t *wm_focused(void);
int  wm_count(void);
window_t *wm_get(int i);

void wm_toggle_max(window_t *w);

#endif
