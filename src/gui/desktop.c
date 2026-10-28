#include "desktop.h"
#include "apps.h"
#include "wm.h"
#include "menu.h"
#include "gfx.h"
#include "mouse.h"
#include "keyboard.h"
#include "fs.h"
#include "mem.h"
#include "storage.h"
#include "rtc.h"
#include "ui.h"
#include "string.h"
#include <stdint.h>

extern void reboot(void);
extern void halt(void);

/* ------------------------------------------------------------------ theme */

typedef struct {
    uint32_t top, bottom, accent;
    const char *name;
} theme_t;

static const theme_t themes[3] = {
    { 0x0E2F3A, 0x1B6A63, 0x2FBFA0, "Yesil" },
    { 0x141B33, 0x2B4670, 0x6C9CFF, "Mavi"  },
    { 0x2A1426, 0x5A2A4A, 0xE06C9F, "Mor"   }
};

static int theme_idx = 0;

int desktop_theme(void) { return theme_idx; }
int desktop_theme_count(void) { return 3; }

static uint32_t lerp_color(uint32_t a, uint32_t b, int t)
{
    if (t < 0) t = 0;
    if (t > 255) t = 255;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    int r = ar + (br - ar) * t / 255;
    int g = ag + (bg - ag) * t / 255;
    int bl = ab + (bb - ab) * t / 255;
    return gfx_rgb((uint8_t)r, (uint8_t)g, (uint8_t)bl);
}

static uint32_t dim_color(uint32_t c, int pct)
{
    int r = ((c >> 16) & 0xFF) * pct / 100;
    int g = ((c >> 8) & 0xFF) * pct / 100;
    int b = (c & 0xFF) * pct / 100;
    return gfx_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static int read_int_file(const char *path, int def)
{
    fs_node *n = fs_lookup(fs_root(), path);
    if (!n || n->is_dir || !n->data || n->size == 0) return def;

    int v = 0, sign = 1;
    size_t i = 0;
    if (n->data[0] == '-') { sign = -1; i = 1; }
    for (; i < n->size && n->data[i] >= '0' && n->data[i] <= '9'; i++)
        v = v * 10 + (n->data[i] - '0');
    return v * sign;
}

static void write_setting(const char *path, int val)
{
    if (fs_touch(path) != 0) return;
    fs_node *n = fs_lookup(fs_root(), path);
    if (!n || n->is_dir) return;
    char b[24];
    ui_itoa(val, b);
    strncat(b, "\n", sizeof(b) - strlen(b) - 1);
    fs_write_file(n, b, strlen(b));
}

void desktop_set_theme(int index)
{
    if (index < 0) index = 0;
    if (index > 2) index = 2;
    theme_idx = index;
    write_setting("/etc/theme", theme_idx);
}

/* ------------------------------------------------------------------ icons */

enum {
    IC_HOME = 0,
    IC_TERM,
    IC_FILES,
    IC_EDITOR,
    IC_CALC,
    IC_SETTINGS,
    IC_TRASH,
    IC_INSTALLER,
    IC_ABOUT,
    IC_COUNT
};

static const char *icon_labels[IC_COUNT] = {
    "Home", "Terminal", "Dosyalar", "Editor", "Hesap",
    "Ayarlar", "Cop", "Kurulum", "Hakkinda"
};

static const int icon_app[IC_COUNT] = {
    -1, APP_TERM, APP_FILES, APP_EDITOR, APP_CALC,
    APP_SETTINGS, APP_TRASH, APP_INSTALLER, APP_ABOUT
};

#define ICON_CW 84
#define ICON_CH 70
#define ICON_X0 18
#define ICON_Y0 20

static int selected_icon = -1;
static int ctx_icon = 0;
static char ctx_msg[80];

static window_t *app_window(int id)
{
    switch (id) {
    case APP_TERM: return g_term;
    case APP_FILES: return g_files;
    case APP_EDITOR: return g_editor;
    case APP_CALC: return g_calc;
    case APP_SETTINGS: return g_set;
    case APP_TRASH: return g_trash;
    case APP_INSTALLER: return g_inst;
    case APP_ABOUT: return g_about;
    default: return NULL;
    }
}

static void open_app_id(int id)
{
    switch (id) {
    case APP_TERM: app_terminal(); break;
    case APP_FILES: app_files(); break;
    case APP_EDITOR: app_editor(); break;
    case APP_CALC: app_calc(); break;
    case APP_SETTINGS: app_settings(); break;
    case APP_TRASH: app_trash(); break;
    case APP_INSTALLER: app_installer(); break;
    case APP_ABOUT: app_about(); break;
    default: break;
    }
}

static void open_icon(int i)
{
    if (i < 0 || i >= IC_COUNT) return;
    if (i == IC_HOME) {
        fs_node *home = fs_lookup(fs_root(), "/home/user");
        files_open_dir(home ? home : fs_root());
        return;
    }
    open_app_id(icon_app[i]);
}

static void icon_cell(int i, int *x, int *y)
{
    *x = ICON_X0 + (i % 2) * ICON_CW;
    *y = ICON_Y0 + (i / 2) * ICON_CH;
}

static int icon_at(int x, int y)
{
    for (int i = 0; i < IC_COUNT; i++) {
        int cx, cy;
        icon_cell(i, &cx, &cy);
        if (x >= cx && x < cx + ICON_CW && y >= cy && y < cy + ICON_CH)
            return i;
    }
    return -1;
}

static void tile(int x, int y, uint32_t accent, int sel)
{
    gfx_fill_rect(x, y, 52, 52, gfx_rgb(30, 34, 42));
    gfx_fill_rect(x + 1, y + 1, 50, 1, gfx_rgb(58, 64, 76));
    gfx_draw_rect(x, y, 52, 52, sel ? accent : gfx_rgb(66, 74, 88));
}

static void draw_home(int x, int y)
{
    uint32_t wall = gfx_rgb(232, 166, 62);
    uint32_t roof = gfx_rgb(206, 126, 44);
    for (int k = 0; k < 3; k++) {
        gfx_line(x + 10, y + 25 + k, x + 26, y + 9 + k, roof);
        gfx_line(x + 26, y + 9 + k, x + 42, y + 25 + k, roof);
    }
    gfx_fill_rect(x + 14, y + 24, 24, 19, wall);
    gfx_fill_rect(x + 22, y + 32, 9, 11, gfx_rgb(78, 48, 26));
    gfx_fill_rect(x + 30, y + 37, 2, 2, gfx_rgb(240, 220, 160));
}

static void draw_term_icon(int x, int y)
{
    uint32_t scr = gfx_rgb(12, 18, 24);
    uint32_t green = gfx_rgb(110, 235, 140);
    gfx_fill_rect(x + 8, y + 11, 36, 27, scr);
    gfx_draw_rect(x + 8, y + 11, 36, 27, gfx_rgb(90, 180, 130));
    gfx_draw_text(x + 12, y + 16, ">", green, scr);
    gfx_fill_rect(x + 22, y + 22, 7, 2, green);
    gfx_fill_rect(x + 18, y + 40, 16, 2, gfx_rgb(100, 108, 124));
}

static void draw_files_icon(int x, int y)
{
    uint32_t amber = gfx_rgb(228, 164, 44);
    uint32_t light = gfx_rgb(248, 198, 86);
    gfx_fill_rect(x + 10, y + 14, 15, 6, light);
    gfx_fill_rect(x + 10, y + 18, 33, 24, amber);
    gfx_fill_rect(x + 11, y + 19, 31, 2, light);
}

static void draw_editor_icon(int x, int y)
{
    uint32_t white = gfx_rgb(250, 250, 246);
    gfx_fill_rect(x + 13, y + 9, 27, 35, white);
    gfx_draw_rect(x + 13, y + 9, 27, 35, gfx_rgb(130, 140, 156));
    gfx_fill_rect(x + 17, y + 15, 15, 1, gfx_rgb(120, 150, 190));
    gfx_fill_rect(x + 17, y + 19, 19, 1, gfx_rgb(170, 180, 196));
    gfx_fill_rect(x + 17, y + 23, 17, 1, gfx_rgb(170, 180, 196));
    gfx_fill_rect(x + 17, y + 27, 13, 1, gfx_rgb(170, 180, 196));
    gfx_line(x + 30, y + 38, x + 41, y + 27, gfx_rgb(220, 140, 30));
    gfx_line(x + 31, y + 39, x + 42, y + 28, gfx_rgb(160, 90, 20));
}

static void draw_calc_icon(int x, int y)
{
    uint32_t body = gfx_rgb(210, 216, 228);
    gfx_fill_rect(x + 12, y + 8, 28, 37, body);
    gfx_draw_rect(x + 12, y + 8, 28, 37, gfx_rgb(120, 130, 146));
    gfx_fill_rect(x + 16, y + 12, 20, 8, gfx_rgb(30, 40, 54));
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            gfx_fill_rect(x + 16 + c * 7, y + 25 + r * 6, 5, 4, gfx_rgb(70, 90, 120));
}

static void draw_settings_icon(int x, int y, uint32_t accent)
{
    int cx = x + 26, cy = y + 26;
    gfx_fill_circle(cx, cy, 14, accent);
    gfx_fill_circle(cx, cy, 8, gfx_rgb(30, 34, 42));
    gfx_fill_circle(cx, cy, 3, accent);
    gfx_fill_rect(cx - 2, cy - 19, 5, 6, accent);
    gfx_fill_rect(cx - 2, cy + 14, 5, 6, accent);
    gfx_fill_rect(cx - 19, cy - 2, 6, 5, accent);
    gfx_fill_rect(cx + 14, cy - 2, 6, 5, accent);
}

static void draw_trash_icon(int x, int y)
{
    uint32_t metal = gfx_rgb(190, 198, 210);
    uint32_t dark = gfx_rgb(120, 128, 142);
    gfx_fill_rect(x + 18, y + 8, 16, 4, dark);
    gfx_fill_rect(x + 16, y + 12, 20, 4, metal);
    gfx_fill_rect(x + 18, y + 16, 16, 24, metal);
    gfx_fill_rect(x + 18, y + 16, 16, 2, dark);
    gfx_fill_rect(x + 22, y + 20, 1, 16, dark);
    gfx_fill_rect(x + 26, y + 20, 1, 16, dark);
    gfx_fill_rect(x + 30, y + 20, 1, 16, dark);
    gfx_fill_rect(x + 20, y + 40, 12, 2, dark);
}

static void draw_install_icon(int x, int y)
{
    uint32_t box = gfx_rgb(0, 156, 100);
    uint32_t lid = gfx_rgb(0, 190, 124);
    gfx_fill_rect(x + 9, y + 20, 34, 23, box);
    gfx_fill_rect(x + 9, y + 16, 34, 4, lid);
    gfx_fill_rect(x + 9, y + 20, 34, 2, gfx_rgb(0, 100, 70));
    uint32_t white = gfx_rgb(255, 255, 255);
    gfx_fill_rect(x + 24, y + 8, 4, 10, white);
    gfx_line(x + 19, y + 14, x + 26, y + 21, white);
    gfx_line(x + 33, y + 14, x + 26, y + 21, white);
}

static void draw_about_icon(int x, int y, uint32_t accent)
{
    int cx = x + 26, cy = y + 26;
    gfx_fill_circle(cx, cy, 18, accent);
    gfx_draw_circle(cx, cy, 18, gfx_rgb(0, 64, 110));
    gfx_fill_rect(cx - 2, cy - 9, 4, 4, gfx_rgb(255, 255, 255));
    gfx_fill_rect(cx - 2, cy - 2, 4, 12, gfx_rgb(255, 255, 255));
}

static void draw_icon(int i)
{
    int cx, cy;
    icon_cell(i, &cx, &cy);
    int tx = cx + 16;
    int ty = cy + 2;
    uint32_t accent = themes[theme_idx].accent;
    int sel = (i == selected_icon);

    tile(tx, ty, accent, sel);

    switch (i) {
    case IC_HOME: draw_home(tx, ty); break;
    case IC_TERM: draw_term_icon(tx, ty); break;
    case IC_FILES: draw_files_icon(tx, ty); break;
    case IC_EDITOR: draw_editor_icon(tx, ty); break;
    case IC_CALC: draw_calc_icon(tx, ty); break;
    case IC_SETTINGS: draw_settings_icon(tx, ty, accent); break;
    case IC_TRASH: draw_trash_icon(tx, ty); break;
    case IC_INSTALLER: draw_install_icon(tx, ty); break;
    case IC_ABOUT: draw_about_icon(tx, ty, accent); break;
    default: break;
    }

    int app = icon_app[i];
    if (app >= 0) {
        window_t *w = app_window(app);
        if (w && w->visible)
            gfx_fill_rect(tx + 43, ty + 43, 6, 6, gfx_rgb(70, 220, 130));
    }

    const char *lab = icon_labels[i];
    int tw = (int)strlen(lab) * 8;
    int pw = tw + 8;
    int px = cx + (ICON_CW - pw) / 2;
    int py = cy + 57;
    gfx_fill_rect(px, py, pw, 12, sel ? accent : gfx_rgb(16, 20, 26));
    gfx_draw_text(px + 4, py + 2, lab, gfx_rgb(255, 255, 255),
                  sel ? accent : gfx_rgb(16, 20, 26));
}

static void draw_icons(void)
{
    for (int i = 0; i < IC_COUNT; i++) draw_icon(i);
}

/* --------------------------------------------------------------- wallpaper */

static void draw_wallpaper(void)
{
    int H = gfx_height();
    int bottom = H - WM_TASKBAR_H;
    uint32_t top = themes[theme_idx].top;
    uint32_t bot = themes[theme_idx].bottom;

    for (int y = 0; y < bottom; y++) {
        int t = bottom > 1 ? y * 255 / (bottom - 1) : 0;
        gfx_fill_rect(0, y, gfx_width(), 1, lerp_color(top, bot, t));
    }

    uint32_t dot = dim_color(themes[theme_idx].accent, 42);
    for (int y = 24; y < bottom - 8; y += 48)
        for (int x = 24; x < gfx_width() - 8; x += 48)
            gfx_fill_rect(x, y, 2, 2, dot);
}

/* --------------------------------------------------------------- new files */

static fs_node *unique_new(const char *base, int is_dir)
{
    fs_node *home = fs_ensure_dir("/home/user");
    if (!home) return NULL;

    char cand[64];
    strncpy(cand, base, sizeof(cand) - 1);
    cand[sizeof(cand) - 1] = 0;
    if (!fs_child(home, cand)) return fs_add(home, cand, is_dir);

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
        if (!fs_child(home, cand)) return fs_add(home, cand, is_dir);
    }
    return NULL;
}

/* ------------------------------------------------------------------ menus */

static void open_start_menu(void);

static void act_term(void)     { open_app_id(APP_TERM); }
static void act_files(void)    { open_app_id(APP_FILES); }
static void act_editor(void)   { open_app_id(APP_EDITOR); }
static void act_calc(void)     { open_app_id(APP_CALC); }
static void act_settings(void) { open_app_id(APP_SETTINGS); }
static void act_trash(void)    { open_app_id(APP_TRASH); }
static void act_about(void)    { open_app_id(APP_ABOUT); }
static void act_install(void)  { open_app_id(APP_INSTALLER); }
static void act_save(void)     { desktop_save(); }
static void act_start(void)    { open_start_menu(); }

static void act_new_folder(void)
{
    fs_node *n = unique_new("yeni_klasor", 1);
    if (n) desktop_toast("Yeni klasor olusturuldu");
    else desktop_toast("Klasor olusturulamadi");
}

static void act_new_file(void)
{
    fs_node *n = unique_new("yeni.txt", 0);
    if (n) {
        desktop_toast("Yeni dosya olusturuldu");
        editor_open(n);
    } else {
        desktop_toast("Dosya olusturulamadi");
    }
}

static void act_icon_open(void) { open_icon(ctx_icon); }

static void act_icon_info(void)
{
    if (ctx_icon < 0 || ctx_icon >= IC_COUNT) return;
    strncpy(ctx_msg, "Kisayol: ", sizeof(ctx_msg) - 1);
    ctx_msg[sizeof(ctx_msg) - 1] = 0;
    strncat(ctx_msg, icon_labels[ctx_icon], sizeof(ctx_msg) - strlen(ctx_msg) - 1);
    desktop_toast(ctx_msg);
}

static void act_icon_refresh(void)
{
    /* Desktop is redrawn after every menu action. */
}

static void act_reboot(void)
{
    desktop_save();
    reboot();
}

static void act_halt(void) { desktop_save(); halt(); }

static void desktop_empty_menu(int x, int y)
{
    const char *labels[] = {
        "Terminal", "Dosya Yoneticisi", "Editor", "-",
        "Yeni Klasor", "Yeni Dosya", "-", "Ayarlar",
        "Diske Kaydet", "Hakkinda"
    };
    menu_action_t acts[] = {
        act_term, act_files, act_editor, NULL,
        act_new_folder, act_new_file, NULL, act_settings,
        act_save, act_about
    };
    menu_open(x, y, "Masaustu", labels, acts, 10);
}

static void icon_menu(int i, int x, int y)
{
    if (i < 0 || i >= IC_COUNT) return;
    ctx_icon = i;
    const char *labels[] = { "Ac", "Ozellikler", "-", "Yenile" };
    menu_action_t acts[] = { act_icon_open, act_icon_info, NULL, act_icon_refresh };
    menu_open(x, y, icon_labels[i], labels, acts, 4);
}

static void taskbar_menu(int x, int y)
{
    const char *labels[] = { "Baslat Menusu", "Diske Kaydet", "-",
                             "Yeniden Baslat", "Hakkinda" };
    menu_action_t acts[] = { act_start, act_save, NULL, act_reboot, act_about };
    menu_open(x, y, "Gorev Cubugu", labels, acts, 5);
}

static void open_start_menu(void)
{
    const char *labels[] = {
        "Terminal", "Dosyalar", "Editor", "Hesap Makinesi",
        "Ayarlar", "Cop Kutusu", "Kurulum", "Hakkinda",
        "-", "Diske Kaydet", "Yeniden Baslat", "Kapat"
    };
    menu_action_t acts[] = {
        act_term, act_files, act_editor, act_calc,
        act_settings, act_trash, act_install, act_about,
        NULL, act_save, act_reboot, act_halt
    };

    int h = menu_height("DenemeOS", labels, 12);
    menu_open(4, gfx_height() - WM_TASKBAR_H - h - 4,
              "DenemeOS", labels, acts, 12);
}

/* --------------------------------------------------------------- taskbar */

#define TB_BTN_W 72
#define TB_BTN_H 20
#define TB_APP_COUNT 8

static const int tb_apps[TB_APP_COUNT] = {
    APP_TERM, APP_FILES, APP_EDITOR, APP_CALC,
    APP_SETTINGS, APP_TRASH, APP_INSTALLER, APP_ABOUT
};

static const char *tb_labels[TB_APP_COUNT] = {
    "Terminal", "Dosyalar", "Editor", "Hesap",
    "Ayarlar", "Cop", "Kurulum", "Hakkinda"
};

static int tb_button_x(int i) { return 74 + i * TB_BTN_W; }
static int tb_save_x(void) { return gfx_width() - 126; }

static void taskbar_click(int x, int y, int right)
{
    (void)y;
    if (right) {
        taskbar_menu(x, y);
        return;
    }

    if (x >= 4 && x < 66) {
        open_start_menu();
        return;
    }

    int sx = tb_save_x();
    if (x >= sx && x < sx + 60) {
        desktop_save();
        return;
    }

    for (int i = 0; i < TB_APP_COUNT; i++) {
        int bx = tb_button_x(i);
        if (x >= bx && x < bx + TB_BTN_W - 2) {
            int id = tb_apps[i];
            window_t *w = app_window(id);
            if (w && w->visible && wm_focused() == w) {
                wm_close(w);
            } else {
                open_app_id(id);
            }
            return;
        }
    }
}

static void draw_taskbar(char *clock)
{
    int W = gfx_width();
    int H = gfx_height();
    int y = H - WM_TASKBAR_H;
    uint32_t accent = themes[theme_idx].accent;
    uint32_t bg = gfx_rgb(22, 26, 34);
    uint32_t bg2 = gfx_rgb(30, 35, 45);

    gfx_fill_rect(0, y, W, WM_TASKBAR_H, bg);
    gfx_fill_rect(0, y, W, 1, accent);

    ui_button(4, y + 5, 62, TB_BTN_H, "Menu", accent);

    for (int i = 0; i < TB_APP_COUNT; i++) {
        int bx = tb_button_x(i);
        window_t *w = app_window(tb_apps[i]);
        uint32_t col = bg2;
        if (w && w->visible) {
            col = (w == wm_focused()) ? dim_color(accent, 72) : gfx_rgb(40, 96, 92);
        }
        ui_button(bx, y + 5, TB_BTN_W - 2, TB_BTN_H, tb_labels[i], col);
        if (w && w->visible)
            gfx_fill_rect(bx, y + 2, TB_BTN_W - 2, 2, accent);
    }

    int sx = tb_save_x();
    int disk = storage_present();
    ui_button(sx, y + 5, 60, TB_BTN_H, "Kaydet",
              disk ? gfx_rgb(0, 122, 82) : gfx_rgb(90, 98, 112));

    gfx_fill_rect(sx - 14, y + 12, 6, 6,
                  disk ? gfx_rgb(70, 220, 130) : gfx_rgb(210, 90, 90));

    ui_text_right(W - 58, y + 11, 50, clock, gfx_rgb(240, 244, 250), bg);
}

/* ------------------------------------------------------------------ toast */

#define TOAST_LOOPS 600000

static char toast_msg[96];
static int toast_ticks = 0;

void desktop_toast(const char *msg)
{
    strncpy(toast_msg, msg ? msg : "", sizeof(toast_msg) - 1);
    toast_msg[sizeof(toast_msg) - 1] = 0;
    toast_ticks = TOAST_LOOPS;
}

static void draw_toast(void)
{
    if (toast_ticks <= 0 || !toast_msg[0]) return;

    int W = gfx_width();
    int H = gfx_height();
    int w = 310, h = 24;
    int x = W - w - 8;
    int y = H - WM_TASKBAR_H - h - 8;
    if (x < 4) x = 4;

    uint32_t bg = gfx_rgb(24, 28, 36);
    uint32_t accent = themes[theme_idx].accent;
    gfx_fill_rect(x + 2, y + 2, w, h, gfx_rgb(10, 12, 16));
    gfx_fill_rect(x, y, w, h, bg);
    gfx_fill_rect(x, y, 4, h, accent);
    gfx_draw_rect(x, y, w, h, gfx_rgb(74, 82, 96));

    char b[40];
    int i = 0;
    while (toast_msg[i] && i < 38) { b[i] = toast_msg[i]; i++; }
    b[i] = 0;
    gfx_draw_text(x + 12, y + 8, b, gfx_rgb(238, 242, 248), bg);
}

/* ------------------------------------------------------------------- save */

void desktop_save(void)
{
    if (!storage_present()) {
        desktop_toast("Disk yok: ISO/ram modu, kayit yapilamaz");
        return;
    }
    int r = storage_save();
    if (r == 0) {
        char b[64];
        strncpy(b, "Diske kaydedildi: ", sizeof(b) - 1);
        b[sizeof(b) - 1] = 0;
        char num[24];
        ui_itoa(storage_bytes(), num);
        strncat(b, num, sizeof(b) - strlen(b) - 1);
        strncat(b, " bayt", sizeof(b) - strlen(b) - 1);
        desktop_toast(b);
    } else {
        desktop_toast("Kaydetme hatasi");
    }
}

/* ------------------------------------------------------------------- draw */

static void draw_all(int mx, int my, char *clock)
{
    draw_wallpaper();
    draw_icons();
    wm_draw_windows();
    draw_taskbar(clock);
    draw_toast();
    menu_draw(mx, my);
}

/* ------------------------------------------------------------------ input */

static void toggle_start_menu(void)
{
    if (menu_visible()) menu_close();
    else open_start_menu();
}

static void handle_key(int k)
{
    if (k == KEY_F9) { toggle_start_menu(); return; }
    if (k == KEY_F10) { desktop_save(); return; }
    if (k == KEY_F11) {
        window_t *f = wm_focused();
        if (f && f->visible) wm_toggle_max(f);
        return;
    }

    if (k >= KEY_F1 && k <= KEY_F8) {
        menu_close();
        open_app_id(APP_TERM + (k - KEY_F1));
        return;
    }

    if (k == 27) {
        if (menu_visible()) { menu_close(); return; }
        window_t *f = wm_focused();
        if (!f || !f->visible) return;
        if (f->key && f->key(f, k)) return;   /* app handled Esc itself */
        wm_close(f);
        return;
    }

    if (menu_visible()) {
        menu_key(k);
        return;
    }

    window_t *f = wm_focused();
    if (f && f->visible && f->key) (void)f->key(f, k);
}

/* ------------------------------------------------------------------- init */

void desktop_init(void)
{
    wm_init();
    mouse_init();

    int W = gfx_width();
    int H = gfx_height();
    if (W > 0) mouse_set_screen(W, H);

    fs_ensure_dir("/home/user");
    fs_ensure_dir("/home/user/Desktop");
    fs_ensure_dir("/home/user/Documents");
    fs_ensure_dir("/home/user/Downloads");
    fs_ensure_dir("/Trash");

    theme_idx = read_int_file("/etc/theme", 0);
    if (theme_idx < 0) theme_idx = 0;
    if (theme_idx > 2) theme_idx = 2;

    int sens = read_int_file("/etc/mouse", 100);
    mouse_set_sensitivity(sens);

    selected_icon = -1;
    toast_ticks = 0;
    toast_msg[0] = 0;

    if (storage_loaded()) {
        app_terminal();
        desktop_toast("Snapshot geri yuklendi");
    } else if (storage_present()) {
        app_installer();
        desktop_toast("Sanal disk hazir - kurulum");
    } else {
        app_terminal();
        desktop_toast("ISO modu: degisiklikler kalici degil");
    }
}

/* -------------------------------------------------------------------- run */

void desktop_run(void)
{
    int changed = 1;
    int menu_hover_last = -1;
    char clock[8];
    rtc_time(clock);
    unsigned loop = 0;

    for (;;) {
        mouse_poll();
        mouse_state_t ms = mouse_state();

        int left_pressed  = (ms.pressed & 1) != 0;
        int right_pressed = (ms.pressed & 2) != 0;
        int released      = (ms.released & 1) != 0;

        if (menu_visible()) {
            int h = menu_hover(ms.x, ms.y);
            if (h != menu_hover_last) { menu_hover_last = h; changed = 1; }
        } else if (menu_hover_last != -1) {
            menu_hover_last = -1;
        }

        if (left_pressed || right_pressed) {
            if (menu_visible()) {
                if (left_pressed && menu_click(ms.x, ms.y)) {
                    /* action ran */
                } else {
                    menu_close();
                }
                menu_hover_last = -1;
                changed = 1;
            } else if (ms.y >= gfx_height() - WM_TASKBAR_H) {
                taskbar_click(ms.x, ms.y, right_pressed);
                changed = 1;
            } else {
                window_t *w = wm_at(ms.x, ms.y);
                if (w) {
                    wm_press(ms.x, ms.y, right_pressed ? 2 : 1);
                    changed = 1;
                } else {
                    int ic = icon_at(ms.x, ms.y);
                    selected_icon = ic;
                    if (ic >= 0) {
                        if (right_pressed) icon_menu(ic, ms.x, ms.y);
                        else open_icon(ic);
                    } else if (right_pressed) {
                        desktop_empty_menu(ms.x, ms.y);
                    }
                    changed = 1;
                }
            }
        }

        if (released) wm_release();

        if (ms.buttons & 1) {
            if (wm_drag_to(ms.x, ms.y)) changed = 1;
        }

        while (keyboard_has_key()) {
            int k = keyboard_getkey();
            if (k) { handle_key(k); changed = 1; }
        }

        for (int i = 0; i < wm_count(); i++) {
            window_t *w = wm_get(i);
            if (w && w->visible && w->tick)
                if (w->tick(w)) changed = 1;
        }

        loop++;
        if ((loop & 1023) == 0) {
            char t[8];
            rtc_time(t);
            if (strcmp(t, clock) != 0) {
                strncpy(clock, t, sizeof(clock) - 1);
                clock[sizeof(clock) - 1] = 0;
                changed = 1;
            }
        }

        if (toast_ticks > 0) {
            toast_ticks--;
            if (toast_ticks == 0) changed = 1;
        }

        if (changed) {
            draw_all(ms.x, ms.y, clock);
            gfx_cursor_reset();
            changed = 0;
        }

        gfx_cursor_draw(ms.x, ms.y);
        gfx_present();

        __asm__ volatile ("pause");
    }
}
