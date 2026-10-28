#include "apps.h"
#include "desktop.h"
#include "gfx.h"
#include "ui.h"
#include "storage.h"
#include "ata.h"
#include "string.h"
#include "keyboard.h"
#include <stdint.h>

extern void reboot(void);

window_t *g_inst = NULL;

#define INST_LOOPS_PER_PCT 6000

static int inst_step = 0;       /* 0 welcome, 1 target, 2 confirm, 3 install, 4 done */
static int inst_progress = 0;
static int inst_busy = 0;
static int inst_ticks = 0;
static int inst_result = 0;     /* 0 = ok, -1 = error */
static int inst_bytes = 0;

static uint32_t i_bg, i_text, i_dim, i_line, i_green, i_red, i_blue;

static void colors(void)
{
    i_bg    = gfx_rgb(246, 248, 252);
    i_text  = gfx_rgb(28, 32, 40);
    i_dim   = gfx_rgb(108, 116, 130);
    i_line  = gfx_rgb(214, 220, 230);
    i_green = gfx_rgb(0, 142, 78);
    i_red   = gfx_rgb(184, 52, 58);
    i_blue  = gfx_rgb(0, 112, 180);
}

static void write_file_text(const char *path, const char *text)
{
    if (fs_touch(path) != 0) return;
    fs_node *n = fs_lookup(fs_root(), path);
    if (!n || n->is_dir) return;
    fs_write_file(n, text, strlen(text));
}

static void write_file_if_missing(const char *path, const char *text)
{
    if (fs_lookup(fs_root(), path)) return;
    write_file_text(path, text);
}

static void finish_install(void)
{
    fs_ensure_dir("/etc");
    fs_ensure_dir("/home/user");
    fs_ensure_dir("/home/user/Desktop");
    fs_ensure_dir("/home/user/Documents");
    fs_ensure_dir("/usr/bin");
    fs_ensure_dir("/var/log");

    write_file_text("/etc/version",
                    "DenemeOS 0.2 x86_64\nLimine bootloader\nKurulum: sanal disk\n");
    write_file_text("/etc/install.log",
                    "install: ok\nsnapshot: DNSAVE02\n");
    write_file_if_missing("/home/user/notes.txt",
                    "Notlar\n-----\nF10: diske kaydet\nCop kutusu: silinen dosyalar\n");

    int r = storage_save();
    inst_result = (r == 0) ? 0 : -1;
    inst_bytes = storage_bytes();
    inst_step = 4;
    inst_busy = 0;

    if (r == 0) desktop_toast("Kurulum tamamlandi");
    else desktop_toast("Kurulum hatasi: disk yazilamadi");
}

static int inst_tick(window_t *w)
{
    (void)w;
    if (inst_step != 3 || !inst_busy) return 0;

    inst_ticks++;
    if (inst_ticks < INST_LOOPS_PER_PCT) return 0;

    inst_ticks = 0;
    inst_progress++;
    if (inst_progress >= 100) {
        inst_progress = 100;
        finish_install();
    }
    return 1;
}

static void step_title(window_t *w, const char *title)
{
    gfx_draw_text_scaled(w->x + 16, w->y + 34, title, i_blue, i_bg, 2);
}

static void draw_buttons(window_t *w)
{
    int by = w->y + w->h - 38;
    int right_x = w->x + w->w - 112;

    if (inst_step == 3) {
        ui_button_enabled(right_x, by, 100, 26, "Calisiyor",
                          gfx_rgb(80, 96, 120), 0);
        return;
    }

    if (inst_step == 0) {
        ui_button(right_x, by, 100, 26, "Ileri", i_blue);
    } else if (inst_step == 1) {
        ui_button(w->x + w->w - 204, by, 82, 26, "Geri", gfx_rgb(96, 104, 120));
        ui_button_enabled(right_x, by, 100, 26, "Ileri", i_blue, storage_present());
    } else if (inst_step == 2) {
        ui_button(w->x + w->w - 204, by, 82, 26, "Geri", gfx_rgb(96, 104, 120));
        ui_button(right_x, by, 100, 26, "Kur", i_green);
    } else {
        if (inst_result == 0) {
            ui_button(w->x + w->w - 204, by, 82, 26, "Tekrar",
                      gfx_rgb(96, 104, 120));
            ui_button(right_x, by, 100, 26, "Baslat", i_green);
        } else {
            ui_button(w->x + w->w - 204, by, 82, 26, "Geri",
                      gfx_rgb(96, 104, 120));
            ui_button(right_x, by, 100, 26, "Yeniden Dene", i_red);
        }
    }

    ui_button(w->x + 14, by, 70, 26, "Kapat", gfx_rgb(120, 128, 142));
}

static void installer_draw(window_t *w)
{
    colors();
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, i_bg);

    int x = w->x + 16;
    int y = w->y + 34;

    if (inst_step == 0) {
        step_title(w, "DenemeOS Kurulum");
        gfx_draw_text(x, y + 42, "Bu sihirbaz sistemi sanal sabit diske kurar.", i_text, i_bg);
        gfx_draw_text(x, y + 60, "Kurulumdan sonra dosyalariniz yeniden", i_dim, i_bg);
        gfx_draw_text(x, y + 74, "baslatinca da korunur.", i_dim, i_bg);

        gfx_fill_rect(x, y + 100, w->w - 32, 1, i_line);
        gfx_draw_text(x, y + 112, "Olusturulacak yapi:", i_text, i_bg);
        gfx_draw_text(x + 12, y + 130, "/etc      /home/user", i_green, i_bg);
        gfx_draw_text(x + 12, y + 144, "/usr/bin  /var/log", i_green, i_bg);
        gfx_draw_text(x + 12, y + 158, "/Trash    /tmp", i_green, i_bg);

        gfx_draw_text(x, y + 188, "Ileri ile devam edin.", i_dim, i_bg);
    } else if (inst_step == 1) {
        step_title(w, "Hedef Disk");
        gfx_draw_text(x, y + 42, "Sistem asagidaki diske kurulacak:", i_text, i_bg);

        gfx_fill_rect(x, y + 64, w->w - 32, 54, gfx_rgb(255, 255, 255));
        gfx_draw_rect(x, y + 64, w->w - 32, 54, i_line);

        if (storage_present()) {
            gfx_fill_rect(x + 10, y + 76, 10, 10, i_green);
            gfx_draw_text(x + 30, y + 76, "IDE sanal disk bulundu", i_text, gfx_rgb(255, 255, 255));
            char b[80];
            strncpy(b, "Kapasite: ", sizeof(b) - 1);
            b[sizeof(b) - 1] = 0;
            char num[24];
            ui_itoa((int)(ata_sectors() / 2048), num);
            strncat(b, num, sizeof(b) - strlen(b) - 1);
            strncat(b, " MB  |  snapshot alani 2 MB", sizeof(b) - strlen(b) - 1);
            gfx_draw_text(x + 30, y + 92, b, i_dim, gfx_rgb(255, 255, 255));
        } else {
            gfx_fill_rect(x + 10, y + 76, 10, 10, i_red);
            gfx_draw_text(x + 30, y + 76, "Sanal disk bulunamadi!", i_red, gfx_rgb(255, 255, 255));
            gfx_draw_text(x + 30, y + 92, "./run-vbox-disk.sh ile baslatin.", i_dim, gfx_rgb(255, 255, 255));
        }

        gfx_draw_text(x, y + 134, "ISO modunda disk yoktur; kurulum yapilamaz.", i_dim, i_bg);
    } else if (inst_step == 2) {
        step_title(w, "Kurulumu Onayla");
        gfx_draw_text(x, y + 42, "Asagidaki islemler yapilacak:", i_text, i_bg);
        gfx_draw_text(x + 12, y + 64, "- Sistem dizinleri olusturulacak", i_dim, i_bg);
        gfx_draw_text(x + 12, y + 80, "- Varsayilan dosyalar yazilacak", i_dim, i_bg);
        gfx_draw_text(x + 12, y + 96, "- Dosya sistemi snapshot'i diske yazilacak", i_dim, i_bg);
        gfx_draw_text(x + 12, y + 112, "- Ayarlar ve notlar korunacak", i_dim, i_bg);

        gfx_fill_rect(x, y + 140, w->w - 32, 1, i_line);
        gfx_draw_text(x, y + 152, "Kurulum yaklasik 1-2 saniye surer.", i_dim, i_bg);
        gfx_draw_text(x, y + 172, "Devam etmek icin Kur dugmesine basin.", i_text, i_bg);
    } else if (inst_step == 3) {
        step_title(w, "Kuruluyor");
        gfx_draw_text(x, y + 42, "Dosyalar ve ayarlar diske yaziliyor...", i_text, i_bg);
        gfx_draw_text(x, y + 60, "Lutfen bekleyin, pencereyi kapatmayin.", i_dim, i_bg);

        int bx = x;
        int by = y + 92;
        int bw = w->w - 32;
        gfx_draw_rect(bx, by, bw, 24, gfx_rgb(80, 90, 106));
        int fill = (bw - 6) * inst_progress / 100;
        if (fill > 0) gfx_fill_rect(bx + 3, by + 3, fill, 18, i_green);

        char pct[16];
        ui_itoa(inst_progress, pct);
        strncat(pct, "%", sizeof(pct) - strlen(pct) - 1);
        gfx_draw_text_scaled(bx, by + 36, pct, i_blue, i_bg, 2);

        gfx_draw_text(x, by + 72, "Snapshot alani: LBA 131072 (2 MB)", i_dim, i_bg);
    } else {
        if (inst_result == 0) {
            gfx_draw_text_scaled(x, y, "Kurulum Tamamlandi", i_green, i_bg, 2);
            gfx_draw_text(x, y + 42, "DenemeOS sanal diske yazildi.", i_text, i_bg);

            char b[96];
            strncpy(b, "Snapshot boyutu: ", sizeof(b) - 1);
            b[sizeof(b) - 1] = 0;
            char num[24];
            ui_itoa(inst_bytes, num);
            strncat(b, num, sizeof(b) - strlen(b) - 1);
            strncat(b, " bayt", sizeof(b) - strlen(b) - 1);
            gfx_draw_text(x, y + 62, b, i_dim, i_bg);

            gfx_draw_text(x, y + 88, "Yeniden baslatinca dosyalariniz korunur.", i_text, i_bg);
            gfx_draw_text(x, y + 106, "F10 ile istediginiz zaman kaydedebilirsiniz.", i_dim, i_bg);
        } else {
            gfx_draw_text_scaled(x, y, "Kurulum Basarisiz", i_red, i_bg, 2);
            gfx_draw_text(x, y + 42, "Disk yazilamadi.", i_text, i_bg);
            gfx_draw_text(x, y + 62, "Sanal diskin VM'e bagli oldugundan emin olun.", i_dim, i_bg);
            gfx_draw_text(x, y + 80, "run-vbox-disk.sh kullanin.", i_dim, i_bg);
        }
    }

    char step[32];
    strncpy(step, "Adim ", sizeof(step) - 1);
    char num[8];
    ui_itoa(inst_step > 3 ? 4 : inst_step + 1, num);
    strncat(step, num, sizeof(step) - strlen(step) - 1);
    strncat(step, " / 4", sizeof(step) - strlen(step) - 1);
    ui_text_right(w->x + 8, w->y + 28, w->w - 16, step, i_dim, i_bg);

    draw_buttons(w);
}

static void do_install(void)
{
    inst_step = 3;
    inst_progress = 0;
    inst_ticks = 0;
    inst_busy = 1;
    inst_result = 0;
    inst_bytes = 0;
}

static void installer_click(window_t *w, int lx, int ly, int button)
{
    if (button != 1) return;

    int by = w->h - 38;
    int right_x = w->w - 112;
    int left_x = w->w - 204;
    int cl = 14;

    if (ly >= by && ly < by + 26) {
        if (lx >= cl && lx < cl + 70) {
            if (inst_step != 3) wm_close(g_inst);
            return;
        }

        if (inst_step == 3) return;

        if (inst_step == 0) {
            if (lx >= right_x && lx < right_x + 100) inst_step = 1;
            return;
        }

        if (inst_step == 1) {
            if (lx >= left_x && lx < left_x + 82) { inst_step = 0; return; }
            if (lx >= right_x && lx < right_x + 100 && storage_present()) inst_step = 2;
            return;
        }

        if (inst_step == 2) {
            if (lx >= left_x && lx < left_x + 82) { inst_step = 1; return; }
            if (lx >= right_x && lx < right_x + 100) do_install();
            return;
        }

        if (inst_step == 4) {
            if (inst_result == 0) {
                if (lx >= left_x && lx < left_x + 82) { inst_step = 0; return; }
                if (lx >= right_x && lx < right_x + 100) {
                    storage_save();
                    reboot();
                }
            } else {
                if (lx >= left_x && lx < left_x + 82) { inst_step = 1; return; }
                if (lx >= right_x && lx < right_x + 100) do_install();
            }
        }
    }
}

static int installer_key(window_t *w, int k)
{
    (void)w;
    if (k == 27) {
        if (inst_step > 0 && inst_step < 3) inst_step--;
        else if (inst_step != 3) wm_close(g_inst);
        return 1;
    }
    if (k != '\n') return 0;

    if (inst_step == 0) inst_step = 1;
    else if (inst_step == 1) { if (storage_present()) inst_step = 2; }
    else if (inst_step == 2) do_install();
    else if (inst_step == 4) {
        if (inst_result != 0) inst_step = 1;
        else wm_close(g_inst);
    }

    return 1;
}

window_t *app_installer(void)
{
    if (g_inst) { wm_show(g_inst); return g_inst; }

    colors();
    g_inst = wm_new(160, 70, 480, 370, "Kurulum");
    if (!g_inst) return NULL;
    g_inst->app_id = APP_INSTALLER;
    g_inst->draw = installer_draw;
    g_inst->click = installer_click;
    g_inst->key = installer_key;
    g_inst->tick = inst_tick;

    if (storage_loaded()) {
        inst_step = 4;
        inst_result = 0;
        inst_bytes = storage_bytes();
    }
    return g_inst;
}
