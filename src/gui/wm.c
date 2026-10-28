#include "wm.h"
#include "gfx.h"
#include "string.h"

static window_t wins[WM_MAXW];
static int wcount = 0;

static window_t *order[WM_MAXW];
static int ocount = 0;

static window_t *focus = NULL;
static window_t *drag = NULL;

static uint32_t c_title_active, c_title_idle, c_body, c_text, c_shadow, c_border;
static uint32_t c_btn_close, c_btn_min, c_btn_max;

static void colors(void)
{
    c_title_active = gfx_rgb(0, 96, 170);
    c_title_idle   = gfx_rgb(88, 94, 106);
    c_body         = gfx_rgb(236, 239, 243);
    c_text         = gfx_rgb(24, 26, 30);
    c_shadow       = gfx_rgb(16, 18, 22);
    c_border       = gfx_rgb(38, 42, 50);
    c_btn_close    = gfx_rgb(196, 54, 58);
    c_btn_min      = gfx_rgb(74, 78, 88);
    c_btn_max      = gfx_rgb(74, 78, 88);
}

void wm_init(void)
{
    memset(wins, 0, sizeof(wins));
    memset(order, 0, sizeof(order));
    wcount = ocount = 0;
    focus = drag = NULL;
    colors();
}

static void order_remove(window_t *w)
{
    int k = 0;
    for (int i = 0; i < ocount; i++)
        if (order[i] != w) order[k++] = order[i];
    ocount = k;
}

static void order_push(window_t *w)
{
    order_remove(w);
    if (ocount < WM_MAXW) order[ocount++] = w;
}

static void clamp_window(window_t *win)
{
    if (win->w < 120) win->w = 120;
    if (win->h < 80) win->h = 80;

    if (win->w > gfx_width() - 8) win->w = gfx_width() - 8;
    if (win->h > gfx_height() - WM_TASKBAR_H - 8)
        win->h = gfx_height() - WM_TASKBAR_H - 8;

    if (win->x + win->w > gfx_width() - 4) win->x = gfx_width() - 4 - win->w;
    if (win->y + win->h > gfx_height() - WM_TASKBAR_H - 2)
        win->y = gfx_height() - WM_TASKBAR_H - 2 - win->h;
    if (win->x < 4) win->x = 4;
    if (win->y < 4) win->y = 4;
}

window_t *wm_new(int x, int y, int w, int h, const char *title)
{
    if (wcount >= WM_MAXW) return NULL;
    window_t *win = &wins[wcount++];
    memset(win, 0, sizeof(window_t));
    win->x = x; win->y = y; win->w = w; win->h = h;
    strncpy(win->title, title ? title : "Pencere", sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = 0;
    win->visible = 1;
    clamp_window(win);

    order_push(win);
    wm_focus(win);          /* a freshly opened window is always active */
    return win;
}

static window_t *top_visible(void)
{
    for (int i = ocount - 1; i >= 0; i--)
        if (order[i] && order[i]->visible) return order[i];
    return NULL;
}

void wm_show(window_t *w)
{
    if (!w) return;
    w->visible = 1;
    wm_focus(w);
}

void wm_close(window_t *w)
{
    if (!w) return;
    w->visible = 0;
    w->dragging = 0;
    order_remove(w);
    if (drag == w) drag = NULL;
    if (focus == w) {
        focus = top_visible();
        if (focus) focus->focused = 1;
    }
}

void wm_focus(window_t *w)
{
    if (!w) return;
    for (int i = 0; i < wcount; i++) wins[i].focused = 0;
    w->focused = 1;
    w->visible = 1;
    focus = w;
    order_push(w);
}

void wm_toggle_max(window_t *w)
{
    if (!w) return;
    if (!w->maximized) {
        w->ox = w->x; w->oy = w->y; w->ow = w->w; w->oh = w->h;
        w->x = 2;
        w->y = 2;
        w->w = gfx_width() - 4;
        w->h = gfx_height() - WM_TASKBAR_H - 4;
        w->maximized = 1;
    } else {
        w->x = w->ox; w->y = w->oy;
        w->w = w->ow; w->h = w->oh;
        w->maximized = 0;
        clamp_window(w);
    }
    w->dragging = 0;
    wm_focus(w);
}

static void title_copy(char *out, size_t n, const char *src, int max_chars)
{
    if (n == 0) return;
    size_t k = 0;
    for (int i = 0; src && src[i] && k + 1 < n && i < max_chars; i++)
        out[k++] = src[i];
    out[k] = 0;
}

void wm_draw_windows(void)
{
    colors();

    for (int i = 0; i < ocount; i++) {
        window_t *w = order[i];
        if (!w || !w->visible) continue;

        gfx_fill_rect(w->x + 5, w->y + 5, w->w, w->h, c_shadow);
        gfx_fill_rect(w->x, w->y, w->w, w->h, c_body);

        uint32_t tc = (w == focus) ? c_title_active : c_title_idle;
        gfx_fill_rect(w->x, w->y, w->w, WM_TITLE_H, tc);

        int max_chars = (w->w - 78) / 8;
        if (max_chars < 0) max_chars = 0;
        char title[48];
        title_copy(title, sizeof(title), w->title, max_chars);
        gfx_draw_text(w->x + 8, w->y + 7, title, gfx_rgb(255, 255, 255), tc);

        /* close */
        gfx_fill_rect(w->x + w->w - 20, w->y + 4, 15, 14, c_btn_close);
        gfx_draw_text(w->x + w->w - 16, w->y + 7, "x", gfx_rgb(255, 255, 255), c_btn_close);

        /* minimize */
        gfx_fill_rect(w->x + w->w - 39, w->y + 4, 15, 14, c_btn_min);
        gfx_draw_text(w->x + w->w - 35, w->y + 7, "-", gfx_rgb(255, 255, 255), c_btn_min);

        /* maximize / restore */
        gfx_fill_rect(w->x + w->w - 58, w->y + 4, 15, 14, c_btn_max);
        int mx = w->x + w->w - 58;
        int my = w->y + 4;
        if (w->maximized) {
            gfx_draw_rect(mx + 3, my + 5, 7, 6, gfx_rgb(255, 255, 255));
            gfx_draw_rect(mx + 5, my + 3, 7, 6, gfx_rgb(255, 255, 255));
        } else {
            gfx_draw_rect(mx + 3, my + 3, 9, 8, gfx_rgb(255, 255, 255));
        }

        if (w->draw) w->draw(w);

        gfx_draw_rect(w->x, w->y, w->w, w->h, c_border);
    }
}

window_t *wm_at(int x, int y)
{
    for (int i = ocount - 1; i >= 0; i--) {
        window_t *w = order[i];
        if (!w || !w->visible) continue;
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            return w;
    }
    return NULL;
}

int wm_press(int x, int y, int button)
{
    window_t *w = wm_at(x, y);
    if (!w) return 0;

    if (button == 1) {
        /* close */
        if (x >= w->x + w->w - 20 && x < w->x + w->w - 5 &&
            y >= w->y + 4 && y < w->y + 18) {
            wm_close(w);
            return 1;
        }
        /* minimize */
        if (x >= w->x + w->w - 39 && x < w->x + w->w - 24 &&
            y >= w->y + 4 && y < w->y + 18) {
            w->visible = 0;
            order_remove(w);
            if (focus == w) {
                focus = top_visible();
                if (focus) focus->focused = 1;
            }
            return 1;
        }
        /* maximize */
        if (x >= w->x + w->w - 58 && x < w->x + w->w - 43 &&
            y >= w->y + 4 && y < w->y + 18) {
            wm_toggle_max(w);
            return 1;
        }
        /* title bar: drag */
        if (y >= w->y && y < w->y + WM_TITLE_H) {
            wm_focus(w);
            if (!w->maximized) {
                drag = w;
                w->dragging = 1;
                w->drag_off_x = x - w->x;
                w->drag_off_y = y - w->y;
            }
            return 1;
        }
    }

    wm_focus(w);
    if (w->click) w->click(w, x - w->x, y - w->y, button);
    return 1;
}

int wm_drag_to(int x, int y)
{
    if (!drag || !drag->dragging || drag->maximized) return 0;

    int nx = x - drag->drag_off_x;
    int ny = y - drag->drag_off_y;
    int maxx = gfx_width() - 40;
    int maxy = gfx_height() - WM_TASKBAR_H - WM_TITLE_H - 2;
    if (nx > maxx) nx = maxx;
    if (ny > maxy) ny = maxy;
    if (nx < 40 - drag->w) nx = 40 - drag->w;
    if (ny < 2) ny = 2;
    if (nx == drag->x && ny == drag->y) return 0;
    drag->x = nx;
    drag->y = ny;
    return 1;
}

void wm_release(void)
{
    if (drag) drag->dragging = 0;
    drag = NULL;
}

window_t *wm_focused(void) { return focus; }
int wm_count(void) { return wcount; }
window_t *wm_get(int i) { return (i >= 0 && i < wcount) ? &wins[i] : NULL; }
