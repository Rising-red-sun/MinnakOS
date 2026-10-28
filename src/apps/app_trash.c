#include "apps.h"
#include "desktop.h"
#include "gfx.h"
#include "menu.h"
#include "ui.h"
#include "string.h"
#include "keyboard.h"
#include <stdint.h>

window_t *g_trash = NULL;

static int tr_sel = -1;
static int tr_scroll = 0;
static char tr_status[96];

#define TROW_H 14

static uint32_t t_bg, t_text, t_dim, t_line, t_sel, t_accent;

static void colors(void)
{
    t_bg     = gfx_rgb(250, 251, 253);
    t_text   = gfx_rgb(28, 32, 40);
    t_dim    = gfx_rgb(112, 120, 134);
    t_line   = gfx_rgb(216, 222, 232);
    t_sel    = gfx_rgb(255, 226, 226);
    t_accent = gfx_rgb(176, 56, 60);
}

static fs_node *trash_dir(void)
{
    fs_node *d = fs_lookup(fs_root(), "/Trash");
    if (!d || !d->is_dir) d = fs_ensure_dir("/Trash");
    return d;
}

static int max_rows(window_t *w)
{
    int y0 = w->y + 48;
    int bottom = w->y + w->h - 66;
    int n = (bottom - y0) / TROW_H;
    return n < 1 ? 1 : n;
}

static void set_status(const char *s)
{
    strncpy(tr_status, s ? s : "", sizeof(tr_status) - 1);
    tr_status[sizeof(tr_status) - 1] = 0;
}

static void refresh_status(void)
{
    fs_node *d = trash_dir();
    char b[96];
    if (!d || d->child_count == 0) {
        set_status("Cop kutusu bos");
        return;
    }
    if (tr_sel >= 0 && (size_t)tr_sel < d->child_count) {
        fs_node *n = d->children[tr_sel];
        strncpy(b, n->name, sizeof(b) - 1);
        b[sizeof(b) - 1] = 0;
        if (!n->is_dir) {
            char num[24];
            ui_itoa((int)n->size, num);
            strncat(b, "  ", sizeof(b) - strlen(b) - 1);
            strncat(b, num, sizeof(b) - strlen(b) - 1);
            strncat(b, " bayt", sizeof(b) - strlen(b) - 1);
        }
        set_status(b);
    } else {
        char num[24];
        ui_itoa((int)d->child_count, num);
        strncpy(b, num, sizeof(b) - 1);
        b[sizeof(b) - 1] = 0;
        strncat(b, " oge  |  sag tik: menu", sizeof(b) - strlen(b) - 1);
        set_status(b);
    }
}

void trash_refresh(void)
{
    fs_node *d = trash_dir();
    if (d && tr_sel >= 0 && (size_t)tr_sel >= d->child_count) tr_sel = -1;
    if (d && d->child_count == 0) tr_sel = -1;
    if (tr_scroll < 0) tr_scroll = 0;
    refresh_status();
}

static void draw_clipped(int x, int y, const char *s, int max_px, uint32_t fg, uint32_t bg)
{
    char b[96];
    int maxc = max_px / 8;
    if (maxc < 0) maxc = 0;
    if (maxc > 95) maxc = 95;
    int i = 0;
    for (; s && s[i] && i < maxc; i++) b[i] = s[i];
    b[i] = 0;
    gfx_draw_text(x, y, b, fg, bg);
}

static void trash_draw(window_t *w)
{
    colors();
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, t_bg);

    fs_node *d = trash_dir();
    gfx_draw_text(w->x + 10, w->y + 29, "Silinen dosyalar burada tutulur.",
                  t_accent, t_bg);
    gfx_fill_rect(w->x + 8, w->y + 43, w->w - 16, 1, t_line);

    int y0 = w->y + 48;
    int rows = max_rows(w);
    int n = d ? (int)d->child_count : 0;

    if (n == 0) {
        gfx_draw_text(w->x + w->w / 2 - 56, w->y + 120,
                      "Cop kutusu bos", t_dim, t_bg);
    }

    if (d) {
        for (int r = 0; r < rows; r++) {
            int i = tr_scroll + r;
            if (i >= n) break;
            fs_node *it = d->children[i];
            int ry = y0 + r * TROW_H;
            int selected = (i == tr_sel);

            uint32_t bg = selected ? t_sel : ((r & 1) ? gfx_rgb(244, 247, 251) : t_bg);
            if (selected || (r & 1))
                gfx_fill_rect(w->x + 6, ry - 1, w->w - 18, TROW_H, bg);

            if (it->is_dir) {
                gfx_fill_rect(w->x + 12, ry + 3, 10, 8, gfx_rgb(220, 160, 40));
                gfx_fill_rect(w->x + 13, ry + 1, 4, 2, gfx_rgb(245, 190, 70));
            } else {
                gfx_fill_rect(w->x + 12, ry + 1, 10, 11, gfx_rgb(255, 255, 255));
                gfx_draw_rect(w->x + 12, ry + 1, 10, 11, gfx_rgb(130, 140, 156));
            }

            draw_clipped(w->x + 30, ry + 2, it->name, w->w - 160, t_text, bg);
            if (!it->is_dir) {
                char num[24];
                ui_itoa((int)it->size, num);
                strncat(num, " B", sizeof(num) - strlen(num) - 1);
                ui_text_right(w->x + 8, ry + 2, w->w - 30, num, t_dim, bg);
            } else {
                ui_text_right(w->x + 8, ry + 2, w->w - 30, "Dizin", t_dim, bg);
            }
        }
    }

    if (n > rows) {
        int bar_x = w->x + w->w - 7;
        int bar_y = y0;
        int bar_h = rows * TROW_H;
        gfx_fill_rect(bar_x, bar_y, 4, bar_h, gfx_rgb(226, 230, 238));
        int th = bar_h * rows / n;
        if (th < 12) th = 12;
        int ty = bar_y + (bar_h - th) * tr_scroll / (n - rows);
        gfx_fill_rect(bar_x, ty, 4, th, t_dim);
    }

    int by = w->y + w->h - 58;
    ui_button(w->x + 12, by, 96, 24, "Geri Yukle",
              n ? gfx_rgb(0, 130, 80) : gfx_rgb(160, 166, 176));
    ui_button(w->x + 116, by, 90, 24, "Kalici Sil",
              n ? gfx_rgb(176, 56, 60) : gfx_rgb(160, 166, 176));
    ui_button(w->x + 214, by, 72, 24, "Bosalt",
              n ? gfx_rgb(110, 80, 60) : gfx_rgb(160, 166, 176));

    gfx_fill_rect(w->x + 2, w->y + w->h - 22, w->w - 4, 20, gfx_rgb(235, 239, 245));
    draw_clipped(w->x + 8, w->y + w->h - 16, tr_status, w->w - 16, t_dim,
                 gfx_rgb(235, 239, 245));
}

static void act_restore(void)
{
    fs_node *d = trash_dir();
    if (!d || tr_sel < 0 || (size_t)tr_sel >= d->child_count) return;
    fs_node *n = d->children[tr_sel];
    if (fs_restore(n) == 0) {
        desktop_toast("Dosya geri yuklendi");
        tr_sel = -1;
        trash_refresh();
    } else set_status("Geri yuklenemedi");
}

static void act_delete(void)
{
    fs_node *d = trash_dir();
    if (!d || tr_sel < 0 || (size_t)tr_sel >= d->child_count) return;
    fs_node *n = d->children[tr_sel];
    if (fs_delete(n) == 0) {
        desktop_toast("Kalici olarak silindi");
        tr_sel = -1;
        trash_refresh();
    } else set_status("Silinemedi");
}

static void act_empty(void)
{
    fs_node *d = trash_dir();
    if (!d) return;
    while (d->child_count > 0) {
        if (fs_delete(d->children[0]) != 0) break;
    }
    tr_sel = -1;
    tr_scroll = 0;
    desktop_toast("Cop kutusu bosaltildi");
    trash_refresh();
}

static void act_refresh(void) { trash_refresh(); }

static void item_menu(int x, int y, fs_node *n)
{
    const char *labels[] = { "Geri Yukle", "Kalici Sil", "-", "Yenile" };
    menu_action_t acts[] = { act_restore, act_delete, NULL, act_refresh };
    menu_open(x, y, n ? n->name : "Cop", labels, acts, 4);
}

static void empty_menu(int x, int y)
{
    const char *labels[] = { "Bosalt", "Yenile" };
    menu_action_t acts[] = { act_empty, act_refresh };
    menu_open(x, y, "Cop Kutusu", labels, acts, 2);
}

static void clamp_scroll(window_t *w)
{
    fs_node *d = trash_dir();
    int n = d ? (int)d->child_count : 0;
    int rows = max_rows(w);
    if (tr_scroll > n - rows) tr_scroll = n - rows;
    if (tr_scroll < 0) tr_scroll = 0;
}

static void trash_click(window_t *w, int lx, int ly, int button)
{
    int ax = w->x + lx;
    int ay = w->y + ly;
    fs_node *d = trash_dir();

    int by = w->h - 58;
    if (button == 1 && ly >= by && ly < by + 24 && d) {
        if (lx >= 12 && lx < 108) { act_restore(); return; }
        if (lx >= 116 && lx < 206) { act_delete(); return; }
        if (lx >= 214 && lx < 286) { if (d->child_count) act_empty(); return; }
    }

    int y0 = 48;
    if (d && ly >= y0 && ly < w->h - 66) {
        int r = (ly - y0) / TROW_H;
        int i = tr_scroll + r;
        if (r >= 0 && r < max_rows(w) && i >= 0 && (size_t)i < d->child_count) {
            tr_sel = i;
            refresh_status();
            if (button == 2) item_menu(ax, ay, d->children[i]);
            return;
        }
    }

    if (button == 2) empty_menu(ax, ay);
}

static int trash_key(window_t *w, int k)
{
    fs_node *d = trash_dir();
    int n = d ? (int)d->child_count : 0;
    if (n == 0) return 0;

    if (k == KEY_UP) {
        if (tr_sel > 0) tr_sel--;
        else tr_sel = 0;
        refresh_status();
    } else if (k == KEY_DOWN) {
        if (tr_sel < 0) tr_sel = 0;
        else if (tr_sel < n - 1) tr_sel++;
        refresh_status();
    } else if (k == KEY_HOME) {
        tr_sel = 0; refresh_status();
    } else if (k == KEY_END) {
        tr_sel = n - 1; refresh_status();
    } else if (k == '\n') {
        act_restore();
    } else if (k == KEY_DEL) {
        act_delete();
    }

    if (tr_sel >= 0) {
        int rows = max_rows(w);
        if (tr_sel < tr_scroll) tr_scroll = tr_sel;
        if (tr_sel >= tr_scroll + rows) tr_scroll = tr_sel - rows + 1;
        clamp_scroll(w);
    }

    return 0;
}

window_t *app_trash(void)
{
    if (g_trash) {
        trash_refresh();
        wm_show(g_trash);
        return g_trash;
    }
    colors();
    g_trash = wm_new(170, 60, 460, 380, "Cop Kutusu");
    if (!g_trash) return NULL;
    g_trash->app_id = APP_TRASH;
    g_trash->draw = trash_draw;
    g_trash->click = trash_click;
    g_trash->key = trash_key;
    tr_sel = -1;
    tr_scroll = 0;
    trash_refresh();
    return g_trash;
}
