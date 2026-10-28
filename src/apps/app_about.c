#include "apps.h"
#include "desktop.h"
#include "gfx.h"
#include "ui.h"
#include "mem.h"
#include "storage.h"
#include "mouse.h"
#include "rtc.h"
#include "string.h"
#include "keyboard.h"
#include <stdint.h>

window_t *g_about = NULL;

static uint32_t a_bg, a_text, a_dim, a_accent, a_line;

static void colors(void)
{
    a_bg     = gfx_rgb(247, 249, 252);
    a_text   = gfx_rgb(28, 32, 40);
    a_dim    = gfx_rgb(108, 116, 132);
    a_accent = gfx_rgb(0, 102, 168);
    a_line   = gfx_rgb(214, 220, 230);
}

static void about_draw(window_t *w)
{
    colors();
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, a_bg);

    int cx = w->x + 56;
    int cy = w->y + 74;
    gfx_fill_circle(cx, cy, 34, a_accent);
    gfx_draw_circle(cx, cy, 34, gfx_rgb(0, 70, 120));
    gfx_draw_text_scaled(cx - 8, cy - 8, "D", gfx_rgb(255, 255, 255), a_accent, 2);

    gfx_draw_text_scaled(w->x + 104, w->y + 48, "DenemeOS", a_accent, a_bg, 2);
    gfx_draw_text(w->x + 106, w->y + 72, "Surum 0.2  |  x86_64", a_dim, a_bg);
    gfx_draw_text(w->x + 106, w->y + 88, "C cekirdek + Limine", a_dim, a_bg);

    gfx_fill_rect(w->x + 16, w->y + 120, w->w - 32, 1, a_line);

    int x = w->x + 18;
    int y = w->y + 132;
    char b[120];
    char num[24];

    strncpy(b, "Cozunurluk: ", sizeof(b) - 1);
    b[sizeof(b) - 1] = 0;
    ui_itoa(gfx_width(), num);
    strncat(b, num, sizeof(b) - strlen(b) - 1);
    strncat(b, " x ", sizeof(b) - strlen(b) - 1);
    ui_itoa(gfx_height(), num);
    strncat(b, num, sizeof(b) - strlen(b) - 1);
    gfx_draw_text(x, y, b, a_text, a_bg);

    strncpy(b, "Bellek: ", sizeof(b) - 1);
    b[sizeof(b) - 1] = 0;
    ui_itoa((int)mem_used(), num);
    strncat(b, num, sizeof(b) - strlen(b) - 1);
    strncat(b, " / ", sizeof(b) - strlen(b) - 1);
    ui_itoa((int)mem_total(), num);
    strncat(b, num, sizeof(b) - strlen(b) - 1);
    strncat(b, " bayt", sizeof(b) - strlen(b) - 1);
    gfx_draw_text(x, y + 16, b, a_text, a_bg);

    if (storage_present()) {
        strncpy(b, "Disk: IDE, snapshot ", sizeof(b) - 1);
        b[sizeof(b) - 1] = 0;
        ui_itoa(storage_bytes(), num);
        strncat(b, num, sizeof(b) - strlen(b) - 1);
        strncat(b, " bayt", sizeof(b) - strlen(b) - 1);
    } else {
        strncpy(b, "Disk: yok (ISO/ram modu)", sizeof(b) - 1);
        b[sizeof(b) - 1] = 0;
    }
    gfx_draw_text(x, y + 32, b, a_text, a_bg);

    strncpy(b, "Fare: PS/2, hassasiyet ", sizeof(b) - 1);
    b[sizeof(b) - 1] = 0;
    ui_itoa(mouse_get_sensitivity(), num);
    strncat(b, num, sizeof(b) - strlen(b) - 1);
    strncat(b, "%", sizeof(b) - strlen(b) - 1);
    gfx_draw_text(x, y + 48, b, a_text, a_bg);

    char t[8];
    rtc_time(t);
    strncpy(b, "Saat: ", sizeof(b) - 1);
    b[sizeof(b) - 1] = 0;
    strncat(b, t, sizeof(b) - strlen(b) - 1);
    gfx_draw_text(x, y + 64, b, a_text, a_bg);

    gfx_fill_rect(w->x + 16, y + 84, w->w - 32, 1, a_line);
    gfx_draw_text(x, y + 94, "Masaustu, gorev cubugu, sag tik, cop kutusu,", a_dim, a_bg);
    gfx_draw_text(x, y + 108, "kalici sanal disk ve dahili uygulamalar.", a_dim, a_bg);

    int by = w->y + w->h - 38;
    ui_button(w->x + w->w - 186, by, 84, 26, "Terminal", gfx_rgb(0, 122, 132));
    ui_button(w->x + w->w - 94, by, 78, 26, "Tamam", gfx_rgb(0, 110, 180));
}

static void about_click(window_t *w, int lx, int ly, int button)
{
    if (button != 1) return;
    int by = w->h - 38;
    if (ly < by || ly >= by + 26) return;

    if (lx >= w->w - 186 && lx < w->w - 102) { app_terminal(); return; }
    if (lx >= w->w - 94 && lx < w->w - 16) { wm_close(g_about); return; }
}

static int about_key(window_t *w, int k)
{
    (void)w;
    if (k == 27 || k == '\n') { wm_close(g_about); return 1; }

    return 0;
}

window_t *app_about(void)
{
    if (g_about) { wm_show(g_about); return g_about; }
    colors();
    g_about = wm_new(190, 92, 420, 330, "Hakkinda");
    if (!g_about) return NULL;
    g_about->app_id = APP_ABOUT;
    g_about->draw = about_draw;
    g_about->click = about_click;
    g_about->key = about_key;
    return g_about;
}
