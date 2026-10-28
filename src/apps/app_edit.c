#include "apps.h"
#include "desktop.h"
#include "gfx.h"
#include "ui.h"
#include "mem.h"
#include "string.h"
#include "keyboard.h"
#include "menu.h"
#include <stddef.h>
#include <stdint.h>

window_t *g_editor = NULL;

#define ED_ROWS 34
#define ED_COLS 72
#define LINE_H  10
#define TEXT_X  34
#define TEXT_Y  46

static char ed_lines[ED_ROWS][ED_COLS + 1];
static int  ed_row = 0, ed_col = 0, ed_scroll = 0;
static fs_node *ed_file = NULL;
static char ed_name[64] = "untitled.txt";
static char ed_status[64] = "";
static int  ed_dirty = 0;
static int  ed_truncated = 0;

static uint32_t e_bg, e_text, e_dim, e_header, e_accent, e_line;

static void colors(void)
{
    e_bg     = gfx_rgb(253, 253, 249);
    e_text   = gfx_rgb(24, 27, 33);
    e_dim    = gfx_rgb(122, 128, 140);
    e_header = gfx_rgb(238, 241, 246);
    e_accent = gfx_rgb(0, 96, 170);
    e_line   = gfx_rgb(218, 223, 232);
}

static int line_len(int r)
{
    if (r < 0 || r >= ED_ROWS) return 0;
    return (int)strlen(ed_lines[r]);
}

static void set_status(const char *s)
{
    strncpy(ed_status, s ? s : "", sizeof(ed_status) - 1);
    ed_status[sizeof(ed_status) - 1] = 0;
}

static void ed_clear(void)
{
    for (int r = 0; r < ED_ROWS; r++) ed_lines[r][0] = 0;
    ed_row = ed_col = 0;
    ed_scroll = 0;
    ed_truncated = 0;
}

static void ed_load(fs_node *n)
{
    colors();
    ed_clear();
    ed_dirty = 0;
    ed_file = n;

    if (!n || n->is_dir) {
        strncpy(ed_name, "untitled.txt", sizeof(ed_name) - 1);
        ed_name[sizeof(ed_name) - 1] = 0;
        set_status("yeni dosya");
        return;
    }

    strncpy(ed_name, n->name, sizeof(ed_name) - 1);
    ed_name[sizeof(ed_name) - 1] = 0;

    const char *d = n->data;
    int r = 0, c = 0;
    int truncated = 0;

    while (d && *d) {
        char ch = *d++;
        if (ch == '\r') continue;
        if (ch == '\n') {
            if (r < ED_ROWS - 1) { r++; c = 0; }
            else {
                /* A trailing newline after the last row is harmless; only
                 * real extra content means the file was truncated. */
                const char *q = d;
                while (*q == '\n' || *q == '\r') q++;
                if (*q) truncated = 1;
            }
            continue;
        }
        if (r < ED_ROWS && c < ED_COLS) {
            ed_lines[r][c++] = ch;
            ed_lines[r][c] = 0;
        } else {
            truncated = 1;
        }
    }

    ed_truncated = truncated;
    if (truncated) set_status("dosya kismen yuklendi (cok buyuk)");
    else set_status("yuklendi");
}

static void shift_down(int start)
{
    for (int r = ED_ROWS - 1; r > start; r--) {
        memcpy(ed_lines[r], ed_lines[r - 1], ED_COLS + 1);
    }
    ed_lines[start][0] = 0;
}

static void delete_line(int row)
{
    for (int r = row; r < ED_ROWS - 1; r++) {
        memcpy(ed_lines[r], ed_lines[r + 1], ED_COLS + 1);
    }
    ed_lines[ED_ROWS - 1][0] = 0;
}

static void ed_save(void)
{
    if (ed_truncated) {
        set_status("dosya cok buyuk: kaydedilmez");
        desktop_toast("Dosya cok buyuk, kaydetmek veri kaybeder");
        return;
    }
    if (!ed_file) {
        char dir[160];
        strncpy(dir, "/home/user/", sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = 0;

        char path[160];
        strncpy(path, dir, sizeof(path) - 1);
        path[sizeof(path) - 1] = 0;
        strncat(path, ed_name, sizeof(path) - strlen(path) - 1);

        /* Never overwrite an unrelated existing file just because the new
         * buffer happens to be called untitled.txt. */
        if (fs_lookup(fs_root(), path)) {
            char stem[48], ext[12];
            stem[0] = ext[0] = 0;
            const char *dot = strrchr(ed_name, '.');
            if (dot) {
                size_t sn = (size_t)(dot - ed_name);
                if (sn > 47) sn = 47;
                memcpy(stem, ed_name, sn);
                stem[sn] = 0;
                strncpy(ext, dot, sizeof(ext) - 1);
                ext[sizeof(ext) - 1] = 0;
            } else {
                strncpy(stem, ed_name, sizeof(stem) - 1);
                stem[sizeof(stem) - 1] = 0;
            }

            int found = 0;
            for (int i = 2; i < 1000 && !found; i++) {
                char num[16];
                ui_itoa(i, num);
                strncpy(path, dir, sizeof(path) - 1);
                path[sizeof(path) - 1] = 0;
                strncat(path, stem, sizeof(path) - strlen(path) - 1);
                strncat(path, num, sizeof(path) - strlen(path) - 1);
                strncat(path, ext, sizeof(path) - strlen(path) - 1);
                if (!fs_lookup(fs_root(), path)) found = 1;
            }
            if (!found) { set_status("kaydedilemedi"); return; }

            const char *slash = strrchr(path, '/');
            if (slash) {
                strncpy(ed_name, slash + 1, sizeof(ed_name) - 1);
                ed_name[sizeof(ed_name) - 1] = 0;
            }
        }

        if (fs_touch(path) != 0) { set_status("kaydedilemedi"); return; }
        ed_file = fs_lookup(fs_root(), path);
    }
    if (!ed_file || ed_file->is_dir) { set_status("kaydedilemedi"); return; }

    int last = ED_ROWS - 1;
    while (last >= 0 && ed_lines[last][0] == 0) last--;

    size_t cap = (size_t)ED_ROWS * (ED_COLS + 1) + 2;
    char *buf = (char *)kmalloc(cap);
    if (!buf) { set_status("bellek yetersiz"); return; }

    size_t k = 0;
    for (int r = 0; r <= last; r++) {
        int n = line_len(r);
        memcpy(buf + k, ed_lines[r], (size_t)n);
        k += (size_t)n;
        buf[k++] = '\n';
    }
    buf[k] = 0;

    if (fs_write_file(ed_file, buf, k) == 0) {
        set_status("kaydedildi");
        ed_dirty = 0;
        desktop_toast("Dosya kaydedildi");
    } else {
        set_status("kaydedilemedi");
    }
    kfree(buf);
}

static int visible_rows(window_t *w)
{
    int n = (w->h - TEXT_Y - 26) / LINE_H;
    if (n > ED_ROWS) n = ED_ROWS;
    if (n < 1) n = 1;
    return n;
}

static void fix_scroll(window_t *w)
{
    int vis = visible_rows(w);
    if (ed_row < ed_scroll) ed_scroll = ed_row;
    if (ed_row >= ed_scroll + vis) ed_scroll = ed_row - vis + 1;
    if (ed_scroll < 0) ed_scroll = 0;
    if (ed_scroll > ED_ROWS - vis) ed_scroll = ED_ROWS - vis;
    if (ed_scroll < 0) ed_scroll = 0;
}

static void ed_draw(window_t *w)
{
    colors();
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, e_bg);

    /* header */
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, 22, e_header);
    char head[100];
    strncpy(head, ed_name, sizeof(head) - 1);
    head[sizeof(head) - 1] = 0;
    strncat(head, ed_dirty ? " *" : "", sizeof(head) - strlen(head) - 1);
    gfx_draw_text(w->x + 8, w->y + 29, head, e_accent, e_header);

    ui_button(w->x + w->w - 78, w->y + 26, 68, 16, "Kaydet",
              ed_dirty ? gfx_rgb(190, 120, 20) : gfx_rgb(70, 100, 130));

    gfx_fill_rect(w->x + 2, w->y + 45, w->w - 4, 1, e_line);

    /* text area */
    int tx = w->x + TEXT_X;
    int ty = w->y + TEXT_Y;
    int vis = visible_rows(w);

    gfx_fill_rect(w->x + 2, w->y + 46, w->w - 4, w->h - 68, e_bg);

    int empty = 1;
    for (int r = 0; r < ED_ROWS; r++) if (ed_lines[r][0]) { empty = 0; break; }
    if (empty)
        gfx_draw_text(tx, ty, "Bos dosya. Yazmaya baslayin.", e_dim, e_bg);

    for (int i = 0; i < vis; i++) {
        int r = ed_scroll + i;
        if (r >= ED_ROWS) break;
        int y = ty + i * LINE_H;

        char num[8];
        if (r < 9) { num[0] = '0'; ui_itoa(r + 1, num + 1); }
        else ui_itoa(r + 1, num);
        ui_text_right(w->x + 4, y, 26, num, e_dim, e_bg);

        if (ed_lines[r][0])
            gfx_draw_text(tx, y, ed_lines[r], e_text, e_bg);
    }

    int cy = ty + (ed_row - ed_scroll) * LINE_H;
    int cx = tx + ed_col * 8;
    if (cy >= ty && cy < ty + vis * LINE_H)
        gfx_fill_rect(cx, cy, 1, 9, gfx_rgb(0, 0, 0));

    /* footer */
    gfx_fill_rect(w->x + 2, w->y + w->h - 20, w->w - 4, 18, e_header);
    gfx_draw_text(w->x + 8, w->y + w->h - 15,
                  "Ctrl+S kaydet | oklar | Enter | F10 disk",
                  e_dim, e_header);
    if (ed_status[0])
        ui_text_right(w->x + 8, w->y + w->h - 15, w->w - 16, ed_status,
                      ed_dirty ? gfx_rgb(180, 110, 20) : gfx_rgb(0, 120, 70), e_header);
}

static void ed_act_save(void) { ed_save(); }

static void ed_act_new(void)
{
    if (ed_dirty) {
        ed_save();
        if (ed_dirty) {
            desktop_toast("Onceki dosya kaydedilmedi");
            return;
        }
    }
    ed_clear();
    ed_file = NULL;
    ed_dirty = 0;
    strncpy(ed_name, "untitled.txt", sizeof(ed_name) - 1);
    ed_name[sizeof(ed_name) - 1] = 0;
    set_status("yeni dosya");
}

static void ed_act_about(void) { app_about(); }

static void ed_click(window_t *w, int lx, int ly, int button)
{
    if (button == 2) {
        const char *labels[] = { "Kaydet", "Yeni Dosya", "-", "Hakkinda" };
        menu_action_t acts[] = { ed_act_save, ed_act_new, NULL, ed_act_about };
        menu_open(w->x + lx, w->y + ly, "Editor", labels, acts, 4);
        return;
    }
    if (button != 1) return;

    if (lx >= w->w - 78 && lx < w->w - 10 && ly >= 26 && ly < 42) {
        ed_save();
        return;
    }
    if (ly < TEXT_Y) return;

    int row = ed_scroll + (ly - TEXT_Y) / LINE_H;
    if (row < 0) row = 0;
    if (row >= ED_ROWS) row = ED_ROWS - 1;
    int col = (lx - TEXT_X) / 8;
    if (col < 0) col = 0;
    int len = line_len(row);
    if (col > len) col = len;
    if (col > ED_COLS) col = ED_COLS;

    ed_row = row;
    ed_col = col;
    fix_scroll(w);
}

static int ed_key(window_t *w, int k)
{
    if (k == 19) { ed_save(); return 1; }            /* Ctrl+S */
    if (k == 12) { ed_row = ed_col = 0; ed_scroll = 0; return 1; } /* Ctrl+L */

    if (k == '\n') {
        if (ed_row < ED_ROWS - 1) {
            shift_down(ed_row + 1);
            int rest = line_len(ed_row) - ed_col;
            if (rest < 0) rest = 0;
            memcpy(ed_lines[ed_row + 1], ed_lines[ed_row] + ed_col, (size_t)rest);
            ed_lines[ed_row + 1][rest] = 0;
            ed_lines[ed_row][ed_col] = 0;
            ed_row++;
            ed_col = 0;
            ed_dirty = 1;
            set_status("degisti");
        }
        fix_scroll(w);
        return 1;
    }

    if (k == '\b') {
        if (ed_col > 0) {
            int len = line_len(ed_row);
            for (int i = ed_col - 1; i < len; i++) ed_lines[ed_row][i] = ed_lines[ed_row][i + 1];
            ed_col--;
        } else if (ed_row > 0) {
            int prev = ed_row - 1;
            int plen = line_len(prev);
            int curlen = line_len(ed_row);
            if (plen + curlen <= ED_COLS) {
                memcpy(ed_lines[prev] + plen, ed_lines[ed_row], (size_t)curlen + 1);
                ed_row = prev;
                ed_col = plen;
                delete_line(prev + 1);
            } else {
                ed_row = prev;
                ed_col = plen;
            }
        }
        ed_dirty = 1;
        set_status("degisti");
        fix_scroll(w);
        return 1;
    }

    if (k == KEY_DEL) {
        int len = line_len(ed_row);
        if (ed_col < len) {
            for (int i = ed_col; i < len; i++) ed_lines[ed_row][i] = ed_lines[ed_row][i + 1];
        } else if (ed_row < ED_ROWS - 1) {
            int next = line_len(ed_row + 1);
            if (len + next <= ED_COLS) {
                memcpy(ed_lines[ed_row] + len, ed_lines[ed_row + 1], (size_t)next + 1);
                delete_line(ed_row + 1);
            }
        }
        ed_dirty = 1;
        set_status("degisti");
        return 1;
    }

    if (k == KEY_LEFT) {
        if (ed_col > 0) ed_col--;
        else if (ed_row > 0) { ed_row--; ed_col = line_len(ed_row); }
        fix_scroll(w);
        return 1;
    }
    if (k == KEY_RIGHT) {
        if (ed_col < line_len(ed_row)) ed_col++;
        else if (ed_row < ED_ROWS - 1) { ed_row++; ed_col = 0; }
        fix_scroll(w);
        return 1;
    }
    if (k == KEY_UP) {
        if (ed_row > 0) ed_row--;
        if (ed_col > line_len(ed_row)) ed_col = line_len(ed_row);
        fix_scroll(w);
        return 1;
    }
    if (k == KEY_DOWN) {
        if (ed_row < ED_ROWS - 1) ed_row++;
        if (ed_col > line_len(ed_row)) ed_col = line_len(ed_row);
        fix_scroll(w);
        return 1;
    }
    if (k == KEY_HOME) { ed_col = 0; return 1; }
    if (k == KEY_END)  { ed_col = line_len(ed_row); return 1; }
    if (k == KEY_PGUP) {
        ed_row -= visible_rows(w);
        if (ed_row < 0) ed_row = 0;
        if (ed_col > line_len(ed_row)) ed_col = line_len(ed_row);
        fix_scroll(w);
        return 1;
    }
    if (k == KEY_PGDN) {
        ed_row += visible_rows(w);
        if (ed_row >= ED_ROWS) ed_row = ED_ROWS - 1;
        if (ed_col > line_len(ed_row)) ed_col = line_len(ed_row);
        fix_scroll(w);
        return 1;
    }

    if (k == '\t') {
        for (int n = 0; n < 4; n++) {
            if (line_len(ed_row) < ED_COLS) {
                int len = line_len(ed_row);
                for (int i = len; i >= ed_col; i--) ed_lines[ed_row][i + 1] = ed_lines[ed_row][i];
                ed_lines[ed_row][ed_col++] = ' ';
            }
        }
        ed_dirty = 1;
        set_status("degisti");
        return 1;
    }

    if (k >= 32 && k < 127) {
        int len = line_len(ed_row);
        if (len < ED_COLS) {
            for (int i = len; i >= ed_col; i--) ed_lines[ed_row][i + 1] = ed_lines[ed_row][i];
            ed_lines[ed_row][ed_col++] = (char)k;
            ed_dirty = 1;
            set_status("degisti");
        }
        return 1;
    }

    return 0;
}

static void editor_ensure(void)
{
    if (g_editor) return;
    colors();
    g_editor = wm_new(70, 34, 650, 430, "Editor");
    if (!g_editor) return;
    g_editor->app_id = APP_EDITOR;
    g_editor->draw = ed_draw;
    g_editor->click = ed_click;
    g_editor->key = ed_key;

    ed_clear();
    ed_file = NULL;
    strncpy(ed_name, "untitled.txt", sizeof(ed_name) - 1);
    ed_name[sizeof(ed_name) - 1] = 0;
    set_status("yeni dosya");
}

void editor_open(fs_node *f)
{
    editor_ensure();
    if (!g_editor) return;

    /* Do not silently throw away unsaved text when another file is opened. */
    if (ed_dirty && ed_file == f) {
        wm_show(g_editor);       /* same file: keep the live buffer */
        return;
    }
    if (ed_dirty && ed_file != f) {
        ed_save();
        if (ed_dirty) {
            desktop_toast("Onceki dosya kaydedilmedi");
            wm_show(g_editor);
            return;
        }
    }

    ed_load(f);
    wm_show(g_editor);
}

window_t *app_editor(void)
{
    editor_ensure();
    if (g_editor) wm_show(g_editor);
    return g_editor;
}
