#include "apps.h"
#include "desktop.h"
#include "gfx.h"
#include "menu.h"
#include "ui.h"
#include "string.h"
#include "keyboard.h"
#include <stdint.h>

window_t *g_files = NULL;

static fs_node *fm_dir = NULL;
static int fm_sel = -1;
static int fm_scroll = 0;
static char fm_status[96];
static fs_node *fm_ctx_node = NULL;

#define ROW_H 14

static uint32_t c_bg, c_text, c_dim, c_accent, c_sel, c_line;

static void colors(void)
{
    c_bg     = gfx_rgb(250, 251, 253);
    c_text   = gfx_rgb(26, 30, 38);
    c_dim    = gfx_rgb(110, 118, 132);
    c_accent = gfx_rgb(0, 118, 190);
    c_sel    = gfx_rgb(205, 228, 250);
    c_line   = gfx_rgb(214, 220, 230);
}

static void build_path(fs_node *n, char *out, int max)
{
    if (!out || max <= 0) return;
    if (!n || n == fs_root()) { strncpy(out, "/", (size_t)max); out[max - 1] = 0; return; }

    fs_node *chain[40];
    int d = 0;
    while (n && n != fs_root() && d < 39) { chain[d++] = n; n = n->parent; }

    out[0] = 0;
    for (int i = d - 1; i >= 0; i--) {
        strncat(out, "/", (size_t)max - strlen(out) - 1);
        strncat(out, chain[i]->name, (size_t)max - strlen(out) - 1);
    }
    if (!out[0]) strncpy(out, "/", (size_t)max);
    out[max - 1] = 0;
}

static int has_parent(void)
{
    return fm_dir && fm_dir != fs_root() && fm_dir->parent;
}

static int max_rows(window_t *w)
{
    int y0 = w->y + 60;
    int bottom = w->y + w->h - 22;
    int n = (bottom - y0) / ROW_H;
    return n < 1 ? 1 : n;
}

static int row_count(void)
{
    if (!fm_dir || !fm_dir->is_dir) return 0;
    return (int)fm_dir->child_count + (has_parent() ? 1 : 0);
}

/* row -> child index (-2 parent, -1 none) */
static int row_child(int row)
{
    if (!fm_dir || !fm_dir->is_dir || row < 0) return -1;
    int r = row;
    if (has_parent()) {
        if (r == 0) return -2;
        r--;
    }
    for (size_t i = 0; i < fm_dir->child_count; i++)
        if (fm_dir->children[i]->is_dir) {
            if (r == 0) return (int)i;
            r--;
        }
    for (size_t i = 0; i < fm_dir->child_count; i++)
        if (!fm_dir->children[i]->is_dir) {
            if (r == 0) return (int)i;
            r--;
        }
    return -1;
}

static int child_row(int child)
{
    if (!fm_dir || child < 0 || (size_t)child >= fm_dir->child_count) return -1;
    int row = has_parent() ? 1 : 0;
    for (size_t i = 0; i < fm_dir->child_count; i++) {
        if ((int)i == child) return row;
        if (fm_dir->children[i]->is_dir) row++;
    }
    for (size_t i = 0; i < fm_dir->child_count; i++) {
        if ((int)i == child) return row;
        if (!fm_dir->children[i]->is_dir) row++;
    }
    return -1;
}

static void set_status(const char *s)
{
    strncpy(fm_status, s ? s : "", sizeof(fm_status) - 1);
    fm_status[sizeof(fm_status) - 1] = 0;
}

static void refresh_status(void)
{
    if (fm_sel >= 0 && fm_dir && (size_t)fm_sel < fm_dir->child_count) {
        fs_node *n = fm_dir->children[fm_sel];
        char b[96];
        strncpy(b, n->name, sizeof(b) - 1);
        b[sizeof(b) - 1] = 0;
        strncat(b, n->is_dir ? "/" : "", sizeof(b) - strlen(b) - 1);
        if (!n->is_dir) {
            char num[24];
            ui_itoa((int)n->size, num);
            strncat(b, "  ", sizeof(b) - strlen(b) - 1);
            strncat(b, num, sizeof(b) - strlen(b) - 1);
            strncat(b, " bayt", sizeof(b) - strlen(b) - 1);
        }
        set_status(b);
    } else {
        char b[64];
        strncpy(b, "0 oge", sizeof(b));
        if (fm_dir) {
            int n = (int)fm_dir->child_count;
            char num[24];
            ui_itoa(n, num);
            strncpy(b, num, sizeof(b) - 1);
            b[sizeof(b) - 1] = 0;
            strncat(b, " oge  |  sag tik: menu", sizeof(b) - strlen(b) - 1);
        }
        set_status(b);
    }
}

static void files_ensure(void);

static void open_node(fs_node *n)
{
    if (!n) return;
    if (n->is_dir) files_open_dir(n);
    else editor_open(n);
}

void files_open_dir(fs_node *d)
{
    files_ensure();
    if (!d || !d->is_dir) d = fs_root();
    fm_dir = d;
    fm_sel = -1;
    fm_scroll = 0;
    refresh_status();
    if (g_files) wm_show(g_files);
}

/* --------------------------------------------------------------- drawing */

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

static void draw_dir_icon(int x, int y, uint32_t bg)
{
    uint32_t amber = gfx_rgb(230, 164, 40);
    uint32_t light = gfx_rgb(250, 196, 80);
    gfx_fill_rect(x + 1, y + 2, 5, 2, light);
    gfx_fill_rect(x, y + 4, 11, 8, amber);
    gfx_fill_rect(x + 1, y + 5, 9, 1, light);
    (void)bg;
}

static void draw_file_icon(int x, int y, uint32_t bg)
{
    uint32_t white = gfx_rgb(255, 255, 255);
    uint32_t edge = gfx_rgb(120, 132, 150);
    gfx_fill_rect(x, y + 1, 10, 11, white);
    gfx_draw_rect(x, y + 1, 10, 11, edge);
    gfx_fill_rect(x + 2, y + 4, 6, 1, gfx_rgb(150, 160, 175));
    gfx_fill_rect(x + 2, y + 6, 6, 1, gfx_rgb(150, 160, 175));
    gfx_fill_rect(x + 2, y + 8, 4, 1, gfx_rgb(150, 160, 175));
    (void)bg;
}

static void fm_draw(window_t *w)
{
    colors();
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, c_bg);

    char path[256];
    build_path(fm_dir, path, sizeof(path));

    int head_w = w->w - 92;
    draw_clipped(w->x + 8, w->y + 29, path, head_w, c_accent, c_bg);

    ui_button(w->x + w->w - 70, w->y + 26, 62, 16, "Yukari", gfx_rgb(70, 82, 100));

    gfx_fill_rect(w->x + 6, w->y + 44, w->w - 12, 1, c_line);
    gfx_draw_text(w->x + 8, w->y + 48, "Ad", c_dim, c_bg);
    ui_text_right(w->x + 8, w->y + 48, w->w - 16, "Tur / Boyut", c_dim, c_bg);

    int y0 = w->y + 60;
    int rows = max_rows(w);
    int total = row_count();

    if (total == 0) {
        gfx_draw_text(w->x + w->w / 2 - 48, y0 + 30, "(bos klasor)", c_dim, c_bg);
    }

    for (int r = 0; r < rows; r++) {
        int row = fm_scroll + r;
        if (row >= total) break;
        int child = row_child(row);
        if (child == -1) break;

        int ry = y0 + r * ROW_H;
        int selected = (child >= 0 && child == fm_sel);

        if (selected)
            gfx_fill_rect(w->x + 6, ry - 1, w->w - 18, ROW_H, c_sel);
        else if (r & 1)
            gfx_fill_rect(w->x + 6, ry - 1, w->w - 18, ROW_H, gfx_rgb(244, 247, 251));

        uint32_t bg = selected ? c_sel : ((r & 1) ? gfx_rgb(244, 247, 251) : c_bg);

        if (child == -2) {
            draw_dir_icon(w->x + 12, ry, bg);
            gfx_draw_text(w->x + 30, ry + 2, "..", c_dim, bg);
            continue;
        }

        fs_node *n = fm_dir->children[child];
        if (n->is_dir) {
            draw_dir_icon(w->x + 12, ry, bg);
            draw_clipped(w->x + 30, ry + 2, n->name, w->w - 150, c_accent, bg);
            ui_text_right(w->x + 8, ry + 2, w->w - 30, "Dizin", c_dim, bg);
        } else {
            draw_file_icon(w->x + 12, ry, bg);
            draw_clipped(w->x + 30, ry + 2, n->name, w->w - 170, c_text, bg);
            char num[24];
            ui_itoa((int)n->size, num);
            strncat(num, " B", sizeof(num) - strlen(num) - 1);
            ui_text_right(w->x + 8, ry + 2, w->w - 30, num, c_dim, bg);
        }
    }

    /* scrollbar */
    if (total > rows) {
        int bar_x = w->x + w->w - 7;
        int bar_y = y0;
        int bar_h = rows * ROW_H;
        gfx_fill_rect(bar_x, bar_y, 4, bar_h, gfx_rgb(226, 230, 238));
        int th = bar_h * rows / total;
        if (th < 12) th = 12;
        int ty = bar_y;
        if (total > rows) ty = bar_y + (bar_h - th) * fm_scroll / (total - rows);
        gfx_fill_rect(bar_x, ty, 4, th, c_dim);
    }

    gfx_fill_rect(w->x + 2, w->y + w->h - 20, w->w - 4, 18, gfx_rgb(235, 239, 245));
    draw_clipped(w->x + 8, w->y + w->h - 15, fm_status, w->w - 16, c_dim, gfx_rgb(235, 239, 245));
}

/* --------------------------------------------------------------- actions */

static void act_open(void) { if (fm_ctx_node) open_node(fm_ctx_node); }

static void act_trash(void)
{
    if (!fm_ctx_node) return;
    fs_node *n = fm_ctx_node;
    if (fs_trash(n) == 0) {
        fm_sel = -1;
        refresh_status();
        set_status("Cop kutusuna tasindi");
        desktop_toast("Cop kutusuna tasindi");
        trash_refresh();
    } else {
        set_status("Silinemedi");
    }
}

static fs_node *unique_new(const char *base, int is_dir)
{
    if (!fm_dir || !fm_dir->is_dir) return NULL;

    char cand[64];
    strncpy(cand, base, sizeof(cand) - 1);
    cand[sizeof(cand) - 1] = 0;
    if (!fs_child(fm_dir, cand)) return fs_add(fm_dir, cand, is_dir);

    char stem[48], ext[12];
    stem[0] = ext[0] = 0;
    const char *dot = strrchr(base, '.');
    if (dot && !is_dir) {
        size_t sn = (size_t)(dot - base);
        if (sn > 47) sn = 47;
        memcpy(stem, base, sn);
        stem[sn] = 0;
        strncpy(ext, dot, sizeof(ext) - 1);
        ext[sizeof(ext) - 1] = 0;
    } else {
        strncpy(stem, base, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = 0;
    }

    for (int i = 2; i < 1000; i++) {
        char num[16];
        ui_itoa(i, num);
        strncpy(cand, stem, sizeof(cand) - 1);
        cand[sizeof(cand) - 1] = 0;
        strncat(cand, num, sizeof(cand) - strlen(cand) - 1);
        strncat(cand, ext, sizeof(cand) - strlen(cand) - 1);
        if (!fs_child(fm_dir, cand)) return fs_add(fm_dir, cand, is_dir);
    }
    return NULL;
}

static void act_new_folder(void)
{
    fs_node *n = unique_new("yeni_klasor", 1);
    if (n) {
        set_status("Yeni klasor olusturuldu");
        desktop_toast("Yeni klasor olusturuldu");
        refresh_status();
    } else set_status("Klasor olusturulamadi");
}

static void act_new_file(void)
{
    fs_node *n = unique_new("yeni.txt", 0);
    if (n) {
        set_status("Yeni dosya olusturuldu");
        desktop_toast("Yeni dosya olusturuldu");
        editor_open(n);
    } else set_status("Dosya olusturulamadi");
}

static void act_parent(void)
{
    if (fm_dir && fm_dir->parent) files_open_dir(fm_dir->parent);
}

static void act_refresh(void)
{
    if (fm_sel >= 0 && fm_dir && (size_t)fm_sel >= fm_dir->child_count) fm_sel = -1;
    refresh_status();
}

static void item_menu(int x, int y, fs_node *n)
{
    fm_ctx_node = n;
    const char *labels[] = { "Ac", "Cop Kutusuna Tasi", "-", "Yenile" };
    menu_action_t acts[] = { act_open, act_trash, NULL, act_refresh };
    menu_open(x, y, n ? n->name : "Oge", labels, acts, 4);
}

static void empty_menu(int x, int y)
{
    const char *labels[] = { "Yeni Klasor", "Yeni Dosya", "-", "Yukari", "Yenile" };
    menu_action_t acts[] = { act_new_folder, act_new_file, NULL, act_parent, act_refresh };
    menu_open(x, y, fm_dir ? fm_dir->name : "Dosyalar", labels, acts, 5);
}

/* ------------------------------------------------------------------ input */

static void clamp_scroll(window_t *w)
{
    int total = row_count();
    int rows = max_rows(w);
    if (fm_scroll > total - rows) fm_scroll = total - rows;
    if (fm_scroll < 0) fm_scroll = 0;
}

static void ensure_visible(window_t *w)
{
    int row = child_row(fm_sel);
    if (row < 0) return;
    int rows = max_rows(w);
    if (row < fm_scroll) fm_scroll = row;
    if (row >= fm_scroll + rows) fm_scroll = row - rows + 1;
    clamp_scroll(w);
}

static void move_sel(window_t *w, int dir)
{
    if (!fm_dir || fm_dir->child_count == 0) return;

    int order[256];
    int n = 0;
    for (size_t i = 0; i < fm_dir->child_count && n < 256; i++)
        if (fm_dir->children[i]->is_dir) order[n++] = (int)i;
    for (size_t i = 0; i < fm_dir->child_count && n < 256; i++)
        if (!fm_dir->children[i]->is_dir) order[n++] = (int)i;
    if (n == 0) return;

    int pos = -1;
    for (int i = 0; i < n; i++) if (order[i] == fm_sel) { pos = i; break; }
    if (pos < 0) pos = dir > 0 ? 0 : n - 1;
    else pos += dir;
    if (pos < 0) pos = 0;
    if (pos >= n) pos = n - 1;

    fm_sel = order[pos];
    ensure_visible(w);
    refresh_status();
}

static int fm_key(window_t *w, int k)
{
    if (k == KEY_UP)    { move_sel(w, -1); return 1; }
    if (k == KEY_DOWN)  { move_sel(w, 1); return 1; }
    if (k == KEY_HOME)  { fm_sel = -1; move_sel(w, 1); return 1; }
    if (k == KEY_END)   { fm_sel = -1; move_sel(w, -1); return 1; }
    if (k == KEY_PGUP)  { for (int i = 0; i < max_rows(w); i++) move_sel(w, -1); return 1; }
    if (k == KEY_PGDN)  { for (int i = 0; i < max_rows(w); i++) move_sel(w, 1); return 1; }
    if (k == '\n') {
        if (fm_dir && fm_sel >= 0 && (size_t)fm_sel < fm_dir->child_count)
            open_node(fm_dir->children[fm_sel]);
        return 1;
    }
    if (k == '\b') { act_parent(); return 1; }
    if (k == KEY_DEL) {
        if (fm_dir && fm_sel >= 0 && (size_t)fm_sel < fm_dir->child_count) {
            fm_ctx_node = fm_dir->children[fm_sel];
            act_trash();
        }
        return 1;
    }

    return 0;
}

static void fm_click(window_t *w, int lx, int ly, int button)
{
    int ax = w->x + lx;
    int ay = w->y + ly;

    if (button == 1) {
        /* Yukari button */
        if (lx >= w->w - 70 && lx < w->w - 8 && ly >= 26 && ly < 42) {
            act_parent();
            return;
        }
    }

    int y0 = 60;
    int rows = max_rows(w);
    if (ly >= y0) {
        int r = (ly - y0) / ROW_H;
        if (r >= 0 && r < rows) {
            int row = fm_scroll + r;
            int child = row_child(row);
            if (child == -2) {
                if (button == 2) { empty_menu(ax, ay); return; }
                act_parent();
                return;
            }
            if (child >= 0) {
                fm_sel = child;
                refresh_status();
                if (button == 2) { item_menu(ax, ay, fm_dir->children[child]); return; }
                if (button == 1) { open_node(fm_dir->children[child]); return; }
                return;
            }
        }
    }

    if (button == 2) empty_menu(ax, ay);
}

/* ------------------------------------------------------------------- init */

static void files_ensure(void)
{
    if (g_files) return;
    colors();
    g_files = wm_new(60, 46, 500, 390, "Dosyalar");
    if (!g_files) return;
    g_files->app_id = APP_FILES;
    g_files->draw = fm_draw;
    g_files->click = fm_click;
    g_files->key = fm_key;

    fm_dir = fs_lookup(fs_root(), "/home/user");
    if (!fm_dir || !fm_dir->is_dir) fm_dir = fs_root();
    fm_sel = -1;
    fm_scroll = 0;
    refresh_status();
}

window_t *app_files(void)
{
    files_ensure();
    if (g_files) wm_show(g_files);
    return g_files;
}
