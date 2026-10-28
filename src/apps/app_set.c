#include "apps.h"
#include "desktop.h"
#include "gfx.h"
#include "ui.h"
#include "mouse.h"
#include "mem.h"
#include "storage.h"
#include "ata.h"
#include "string.h"
#include <stdint.h>

window_t *g_set = NULL;

static uint32_t s_bg, s_panel, s_text, s_dim, s_line, s_accent;

static void colors(void)
{
    s_bg     = gfx_rgb(245, 247, 250);
    s_panel  = gfx_rgb(255, 255, 255);
    s_text   = gfx_rgb(28, 32, 40);
    s_dim    = gfx_rgb(112, 120, 134);
    s_line   = gfx_rgb(216, 222, 232);
    s_accent = gfx_rgb(0, 118, 160);
}

static void write_int_file(const char *path, int v)
{
    if (fs_touch(path) != 0) return;
    fs_node *n = fs_lookup(fs_root(), path);
    if (!n || n->is_dir) return;
    char b[24];
    ui_itoa(v, b);
    strncat(b, "\n", sizeof(b) - strlen(b) - 1);
    fs_write_file(n, b, strlen(b));
}

static void set_sensitivity(int v)
{
    mouse_set_sensitivity(v);
    write_int_file("/etc/mouse", mouse_get_sensitivity());
    int ok = 1;
    if (storage_present() && storage_save() != 0) ok = 0;
    desktop_toast(ok ? "Fare hassasiyeti kaydedildi"
                     : "Fare ayarlandi, diske yazilamadi");
}

static void set_theme(int idx)
{
    desktop_set_theme(idx);
    int ok = 1;
    if (storage_present() && storage_save() != 0) ok = 0;
    desktop_toast(ok ? "Tema kaydedildi" : "Tema degisti, diske yazilamadi");
}

static void settings_draw(window_t *w)
{
    colors();
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, s_bg);

    int x = w->x + 12;
    int y = w->y + 32;

    /* mouse */
    gfx_draw_text(x, y, "Fare", s_accent, s_bg);
    gfx_fill_rect(x, y + 12, w->w - 24, 1, s_line);

    char b[100];
    strncpy(b, "Hassasiyet: ", sizeof(b) - 1);
    b[sizeof(b) - 1] = 0;
    char num[16];
    ui_itoa(mouse_get_sensitivity(), num);
    strncat(b, num, sizeof(b) - strlen(b) - 1);
    strncat(b, "%", sizeof(b) - strlen(b) - 1);
    gfx_draw_text(x, y + 22, b, s_text, s_bg);

    ui_button(x, y + 36, 34, 22, "-", gfx_rgb(70, 86, 110));
    ui_button(x + 40, y + 36, 34, 22, "+", gfx_rgb(70, 86, 110));

    int bar_x = x + 86;
    int bar_w = w->w - 100 - 12;
    if (bar_w < 20) bar_w = 20;
    gfx_fill_rect(bar_x, y + 43, bar_w, 8, gfx_rgb(218, 224, 234));
    int pct = (mouse_get_sensitivity() - 25) * 100 / 175;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    gfx_fill_rect(bar_x, y + 43, bar_w * pct / 100, 8, s_accent);

    /* theme */
    y += 72;
    gfx_draw_text(x, y, "Masaustu temasi", s_accent, s_bg);
    gfx_fill_rect(x, y + 12, w->w - 24, 1, s_line);

    const char *themes[3] = { "Yesil", "Mavi", "Mor" };
    uint32_t cols[3] = { gfx_rgb(20, 130, 116), gfx_rgb(52, 88, 180), gfx_rgb(150, 60, 120) };
    for (int i = 0; i < 3; i++) {
        int bx = x + i * 86;
        ui_button(bx, y + 22, 80, 24, themes[i], cols[i]);
        if (desktop_theme() == i)
            gfx_draw_rect(bx, y + 22, 80, 24, gfx_rgb(20, 24, 30));
    }

    /* system */
    y += 60;
    gfx_draw_text(x, y, "Sistem", s_accent, s_bg);
    gfx_fill_rect(x, y + 12, w->w - 24, 1, s_line);

    char line[120];
    strncpy(line, "Cozunurluk: ", sizeof(line) - 1);
    line[sizeof(line) - 1] = 0;
    ui_itoa(gfx_width(), num);
    strncat(line, num, sizeof(line) - strlen(line) - 1);
    strncat(line, "x", sizeof(line) - strlen(line) - 1);
    ui_itoa(gfx_height(), num);
    strncat(line, num, sizeof(line) - strlen(line) - 1);
    gfx_draw_text(x, y + 20, line, s_text, s_bg);

    strncpy(line, "Bellek: ", sizeof(line) - 1);
    line[sizeof(line) - 1] = 0;
    ui_itoa((int)mem_used(), num);
    strncat(line, num, sizeof(line) - strlen(line) - 1);
    strncat(line, " / ", sizeof(line) - strlen(line) - 1);
    ui_itoa((int)mem_total(), num);
    strncat(line, num, sizeof(line) - strlen(line) - 1);
    strncat(line, " bayt", sizeof(line) - strlen(line) - 1);
    gfx_draw_text(x, y + 34, line, s_text, s_bg);

    if (storage_present()) {
        strncpy(line, "Disk: IDE, snapshot: ", sizeof(line) - 1);
        line[sizeof(line) - 1] = 0;
        ui_itoa(storage_bytes(), num);
        strncat(line, num, sizeof(line) - strlen(line) - 1);
        strncat(line, " bayt", sizeof(line) - strlen(line) - 1);
    } else {
        strncpy(line, "Disk: yok (ISO/ram modu)", sizeof(line) - 1);
    }
    line[sizeof(line) - 1] = 0;
    gfx_draw_text(x, y + 48, line, s_text, s_bg);

    /* bottom buttons */
    int by = w->y + w->h - 38;
    ui_button(w->x + 12, by, 104, 26, "Diske Kaydet", gfx_rgb(0, 130, 80));
    ui_button(w->x + w->w - 82, by, 70, 26, "Kapat", gfx_rgb(90, 100, 116));
}

static int settings_key(window_t *w, int k)
{
    (void)w;
    if (k == '+' || k == '=') { set_sensitivity(mouse_get_sensitivity() + 10); return 1; }
    if (k == '-' || k == '_') { set_sensitivity(mouse_get_sensitivity() - 10); return 1; }
    if (k >= '1' && k <= '3') { set_theme(k - '1'); return 1; }
    if (k == 's' || k == 'S') { desktop_save(); return 1; }

    return 0;
}

static void settings_click(window_t *w, int lx, int ly, int button)
{
    if (button != 1) return;

    int y = 32;
    if (ly >= y + 36 && ly < y + 58) {
        if (lx >= 12 && lx < 46) { set_sensitivity(mouse_get_sensitivity() - 10); return; }
        if (lx >= 52 && lx < 86) { set_sensitivity(mouse_get_sensitivity() + 10); return; }
    }

    int ty = 32 + 72 + 22;
    if (ly >= ty && ly < ty + 24) {
        if (lx >= 12 && lx < 92)   { set_theme(0); return; }
        if (lx >= 98 && lx < 178)  { set_theme(1); return; }
        if (lx >= 184 && lx < 264) { set_theme(2); return; }
    }

    int by = w->h - 38;
    if (ly >= by && ly < by + 26) {
        if (lx >= 12 && lx < 116) { desktop_save(); return; }
        if (lx >= w->w - 82 && lx < w->w - 12) { wm_close(g_set); return; }
    }
}

window_t *app_settings(void)
{
    if (g_set) { wm_show(g_set); return g_set; }
    colors();
    g_set = wm_new(330, 66, 390, 380, "Ayarlar");
    if (!g_set) return NULL;
    g_set->app_id = APP_SETTINGS;
    g_set->draw = settings_draw;
    g_set->click = settings_click;
    g_set->key = settings_key;
    return g_set;
}
