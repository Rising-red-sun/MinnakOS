#include "apps.h"
#include "desktop.h"
#include "gfx.h"
#include "fs.h"
#include "mem.h"
#include "string.h"
#include "keyboard.h"
#include "storage.h"
#include "ata.h"
#include "rtc.h"
#include "menu.h"
#include <stdint.h>

extern void reboot(void);
extern void halt(void);

#define COLS 72
#define ROWS 26
#define INMAX 72
#define HISTMAX 16

window_t *g_term = NULL;

static char tb[ROWS][COLS + 1];
static int  tr = 0, tc = 0;

static char input[INMAX + 1];
static int  ilen = 0, icur = 0;

static char hist[HISTMAX][INMAX + 1];
static int  hcount = 0, hpos = -1;
static char hsave[INMAX + 1];

static const uint32_t BG  = 0x0A0C10;
static const uint32_t FG  = 0x7CE38B;
static const uint32_t FG2 = 0xE8EEF7;
static const uint32_t DIM = 0x6E7A8C;

/* ------------------------------------------------------------- terminal IO */

static void scroll(void)
{
    for (int i = 1; i < ROWS; i++)
        memcpy(tb[i - 1], tb[i], COLS + 1);
    tb[ROWS - 1][0] = 0;
}

static void putc_(char c)
{
    if (c == '\n') {
        tc = 0;
        if (++tr >= ROWS) { scroll(); tr = ROWS - 1; }
        return;
    }
    if (c == '\r') { tc = 0; return; }
    if (c == '\t') {
        int next = (tc + 4) & ~3;
        while (tc < next && tc < COLS) putc_(' ');
        return;
    }
    if (tc >= COLS) {
        tc = 0;
        if (++tr >= ROWS) { scroll(); tr = ROWS - 1; }
    }
    tb[tr][tc++] = c;
    tb[tr][tc] = 0;
}

static void print(const char *s)
{
    while (s && *s) putc_(*s++);
}

static void print_ll(long long v)
{
    char b[32];
    int n = 0, neg = 0;
    unsigned long long u;
    if (v < 0) { neg = 1; u = (unsigned long long)(-v); } else u = (unsigned long long)v;
    char t[32];
    if (!u) t[n++] = '0';
    while (u) { t[n++] = (char)('0' + (u % 10)); u /= 10; }
    int k = 0;
    if (neg) b[k++] = '-';
    while (n) b[k++] = t[--n];
    b[k] = 0;
    print(b);
}

static void prompt(char *out, int max)
{
    strncpy(out, "denemeos:", (size_t)max - 1);
    out[max - 1] = 0;
    strncat(out, fs_pwd(), (size_t)max - strlen(out) - 1);
    strncat(out, "$ ", (size_t)max - strlen(out) - 1);
}

static void clear_screen(void)
{
    for (int i = 0; i < ROWS; i++) tb[i][0] = 0;
    tr = tc = 0;
}

/* ------------------------------------------------------------------ helpers */

static fs_node *path_node(const char *p)
{
    return fs_lookup(fs_cwd(), p);
}

static void list_dir(fs_node *d)
{
    if (!d || !d->is_dir) { print("dizin degil\n"); return; }
    if (d->child_count == 0) { print("(bos)\n"); return; }

    int dirs = 0, files = 0;
    for (size_t i = 0; i < d->child_count; i++) {
        fs_node *c = d->children[i];
        if (c->is_dir) {
            print("[D] ");
            print(c->name);
            print("/\n");
            dirs++;
        } else {
            files++;
        }
    }
    for (size_t i = 0; i < d->child_count; i++) {
        fs_node *c = d->children[i];
        if (c->is_dir) continue;
        print("[F] ");
        print(c->name);
        print("  ");
        print_ll((long long)c->size);
        print(" bayt\n");
    }
    print("toplam ");
    print_ll(dirs);
    print(" dizin, ");
    print_ll(files);
    print(" dosya\n");
}

static void tree_rec(fs_node *d, int depth)
{
    if (!d || depth > 8) return;
    for (size_t i = 0; i < d->child_count; i++) {
        fs_node *c = d->children[i];
        for (int k = 0; k < depth; k++) print("  ");
        print(c->is_dir ? "+ " : "- ");
        print(c->name);
        if (c->is_dir) putc_('/');
        putc_('\n');
        if (c->is_dir) tree_rec(c, depth + 1);
    }
}

static int copy_file(const char *src, const char *dst)
{
    fs_node *s = path_node(src);
    if (!s || s->is_dir) { print("kaynak dosya degil\n"); return -1; }
    if (fs_touch(dst) != 0) { print("hedef olusturulamadi\n"); return -1; }
    fs_node *d = path_node(dst);
    if (!d || d->is_dir) { print("hedef dosya degil\n"); return -1; }
    return fs_write_file(d, s->data, s->size);
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
}

/* --------------------------------------------------------------- commands */

static void cmd_help(void)
{
    print("komutlar:\n");
    print("  help clear ls [d] cd [d] pwd cat f mkdir d touch f\n");
    print("  rm f (cop)  del f (kalici)  write f metin  echo metin\n");
    print("  mv a b  cp a b  tree  date  mem  df  uname\n");
    print("  save  reboot  halt  history\n");
    print("uygulamalar: calc files edit <dosya> install settings trash about\n");
}

static void run(const char *cmdline)
{
    /* echo metin > dosya */
    const char *gt = strchr(cmdline, '>');
    if (gt) {
        char left[INMAX + 1];
        size_t n = (size_t)(gt - cmdline);
        if (n > INMAX) n = INMAX;
        memcpy(left, cmdline, n);
        left[n] = 0;
        trim(left);

        char file[INMAX + 1];
        strncpy(file, gt + 1, INMAX);
        file[INMAX] = 0;
        trim(file);

        char *text = left;
        while (*text == ' ') text++;
        if (strncmp(text, "echo", 4) == 0) text += 4;
        while (*text == ' ') text++;

        if (!file[0]) { print("hedef dosya yok\n"); return; }
        if (fs_touch(file) != 0) { print("dosya olusturulamadi\n"); return; }
        fs_node *fn = path_node(file);
        if (!fn || fn->is_dir) { print("hedef dosya degil\n"); return; }

        char data[INMAX + 2];
        strncpy(data, text, INMAX);
        data[INMAX] = 0;
        size_t dl = strlen(data);
        data[dl++] = '\n';
        fs_write_file(fn, data, dl);
        print("yazildi: ");
        print(file);
        putc_('\n');
        return;
    }

    char line[INMAX + 1];
    strncpy(line, cmdline, INMAX);
    line[INMAX] = 0;

    char *argv[12];
    int argc = 0;
    char *p = line;
    while (*p && argc < 12) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; }
    }
    if (argc == 0) return;

    const char *c = argv[0];

    if (!strcmp(c, "help")) {
        cmd_help();
    } else if (!strcmp(c, "clear")) {
        clear_screen();
    } else if (!strcmp(c, "ls")) {
        list_dir(argc > 1 ? path_node(argv[1]) : fs_cwd());
    } else if (!strcmp(c, "cd")) {
        if (argc < 2) { fs_set_cwd(fs_root()); return; }
        fs_node *n = path_node(argv[1]);
        if (!n || !n->is_dir) { print("cd: dizin bulunamadi\n"); return; }
        fs_set_cwd(n);
    } else if (!strcmp(c, "pwd")) {
        print(fs_pwd());
        putc_('\n');
    } else if (!strcmp(c, "cat")) {
        if (argc < 2) { print("kullanim: cat dosya\n"); return; }
        fs_node *n = path_node(argv[1]);
        if (!n || n->is_dir) { print("cat: dosya bulunamadi\n"); return; }
        if (n->data) print(n->data);
        if (n->size == 0 || n->data[n->size - 1] != '\n') putc_('\n');
    } else if (!strcmp(c, "mkdir")) {
        if (argc < 2) { print("kullanim: mkdir dizin\n"); return; }
        if (fs_mkdir(argv[1]) != 0) print("mkdir: basarisiz\n");
    } else if (!strcmp(c, "touch")) {
        if (argc < 2) { print("kullanim: touch dosya\n"); return; }
        if (fs_touch(argv[1]) != 0) print("touch: basarisiz\n");
    } else if (!strcmp(c, "rm")) {
        if (argc < 2) { print("kullanim: rm dosya\n"); return; }
        fs_node *n = path_node(argv[1]);
        if (!n || n == fs_root()) { print("rm: bulunamadi\n"); return; }
        if (fs_trash(n) != 0) print("rm: cop kutusuna tasinamadi\n");
        else { print("cop kutusuna tasindi\n"); trash_refresh(); }
    } else if (!strcmp(c, "del")) {
        if (argc < 2) { print("kullanim: del dosya\n"); return; }
        fs_node *n = path_node(argv[1]);
        if (!n || n == fs_root()) { print("del: bulunamadi\n"); return; }
        if (fs_delete(n) != 0) print("del: basarisiz\n");
        else { print("kalici olarak silindi\n"); trash_refresh(); }
    } else if (!strcmp(c, "write")) {
        if (argc < 3) { print("kullanim: write dosya metin\n"); return; }
        if (fs_touch(argv[1]) != 0) { print("write: dosya olusturulamadi\n"); return; }
        fs_node *n = path_node(argv[1]);
        if (!n || n->is_dir) { print("write: hedef dosya degil\n"); return; }
        char data[INMAX + 2];
        int k = 0;
        for (int i = 2; i < argc && k < INMAX - 1; i++) {
            if (i > 2) data[k++] = ' ';
            const char *s = argv[i];
            while (*s && k < INMAX - 1) data[k++] = *s++;
        }
        data[k++] = '\n';
        if (fs_write_file(n, data, (size_t)k) != 0) print("write: yazilamadi\n");
        else print("yazildi\n");
    } else if (!strcmp(c, "cp")) {
        if (argc < 3) { print("kullanim: cp kaynak hedef\n"); return; }
        if (copy_file(argv[1], argv[2]) == 0) print("kopyalandi\n");
    } else if (!strcmp(c, "mv")) {
        if (argc < 3) { print("kullanim: mv kaynak hedef\n"); return; }
        fs_node *s = path_node(argv[1]);
        if (!s || s == fs_root()) { print("mv: kaynak bulunamadi\n"); return; }
        fs_node *dst = path_node(argv[2]);
        int rc;
        if (dst && dst->is_dir) {
            rc = fs_move(s, dst, NULL);
        } else {
            char tmp[INMAX + 1];
            strncpy(tmp, argv[2], INMAX);
            tmp[INMAX] = 0;
            char *slash = strrchr(tmp, '/');
            char *name = tmp;
            fs_node *dir = fs_cwd();
            if (slash) {
                *slash = 0;
                name = slash + 1;
                if (*tmp) dir = path_node(tmp);
                else dir = fs_root();
            }
            if (!dir || !dir->is_dir || !*name) { print("mv: hedef gecersiz\n"); return; }
            rc = fs_move(s, dir, name);
        }
        if (rc != 0) print("mv: basarisiz\n");
    } else if (!strcmp(c, "tree")) {
        print("/\n");
        tree_rec(fs_root(), 1);
    } else if (!strcmp(c, "echo")) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) putc_(' ');
            print(argv[i]);
        }
        putc_('\n');
    } else if (!strcmp(c, "date")) {
        char t[8];
        rtc_time(t);
        print("saat ");
        print(t);
        putc_('\n');
    } else if (!strcmp(c, "mem")) {
        print("heap: ");
        print_ll((long long)mem_used());
        print(" / ");
        print_ll((long long)mem_total());
        print(" bayt, dosya dugumu: ");
        print_ll((long long)fs_count());
        putc_('\n');
    } else if (!strcmp(c, "df")) {
        if (!storage_present()) {
            print("disk: yok (ISO/ram modu)\n");
        } else {
            print("disk: IDE, ");
            print_ll((long long)ata_sectors() / 2048);
            print(" MB, snapshot: ");
            print_ll((long long)storage_bytes());
            print(" bayt\n");
        }
    } else if (!strcmp(c, "uname")) {
        print("DenemeOS 0.2 x86_64 Limine\n");
    } else if (!strcmp(c, "history")) {
        for (int i = 0; i < hcount; i++) {
            print_ll(i + 1);
            print("  ");
            print(hist[i]);
            putc_('\n');
        }
    } else if (!strcmp(c, "save") || !strcmp(c, "sync")) {
        desktop_save();
    } else if (!strcmp(c, "calc")) {
        app_calc();
    } else if (!strcmp(c, "files")) {
        app_files();
    } else if (!strcmp(c, "edit") || !strcmp(c, "open")) {
        if (argc < 2) { app_editor(); return; }
        fs_node *n = path_node(argv[1]);
        if (!n || n->is_dir) { print("edit: dosya bulunamadi\n"); return; }
        editor_open(n);
    } else if (!strcmp(c, "install")) {
        app_installer();
    } else if (!strcmp(c, "settings") || !strcmp(c, "set")) {
        app_settings();
    } else if (!strcmp(c, "trash")) {
        app_trash();
    } else if (!strcmp(c, "about")) {
        app_about();
    } else if (!strcmp(c, "reboot")) {
        reboot();
    } else if (!strcmp(c, "halt")) {
        halt();
    } else {
        print("komut yok: ");
        print(c);
        print("  (help)\n");
    }
}

/* ------------------------------------------------------------------ menus */

static void term_act_clear(void) { clear_screen(); }
static void term_act_save(void)  { desktop_save(); }
static void term_act_about(void) { app_about(); }

static void term_click(window_t *w, int lx, int ly, int button)
{
    if (button != 2) return;

    const char *labels[] = { "Temizle", "Diske Kaydet", "-", "Hakkinda" };
    menu_action_t acts[] = { term_act_clear, term_act_save, NULL, term_act_about };
    menu_open(w->x + lx, w->y + ly, "Terminal", labels, acts, 4);
}

/* -------------------------------------------------------------------- draw */

static void term_draw(window_t *w)
{
    int x = w->x + 8;
    int y = w->y + 28;
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, BG);

    int vis = (w->h - 28 - 58) / 8;
    if (vis > ROWS) vis = ROWS;
    if (vis < 1) vis = 1;
    int first = ROWS - vis;
    if (first < 0) first = 0;

    for (int r = 0; r < vis; r++)
        gfx_draw_text(x, y + r * 8, tb[first + r], FG, BG);

    int py = y + vis * 8 + 4;
    char pr[128];
    prompt(pr, sizeof(pr));
    gfx_draw_text(x, py, pr, FG2, BG);
    gfx_draw_text(x, py + 10, input, FG2, BG);

    int cx = x + icur * 8;
    gfx_fill_rect(cx, py + 10, 1, 9, FG);

    gfx_draw_text(w->x + 8, w->y + w->h - 12,
                  "help | F9 menu | F10 kaydet | sag tik menu",
                  DIM, BG);
}

static void history_add(const char *s)
{
    if (!s || !*s) return;
    if (hcount < HISTMAX) {
        strncpy(hist[hcount++], s, INMAX);
        hist[hcount - 1][INMAX] = 0;
    } else {
        for (int i = 1; i < HISTMAX; i++) {
            strncpy(hist[i - 1], hist[i], INMAX);
            hist[i - 1][INMAX] = 0;
        }
        strncpy(hist[HISTMAX - 1], s, INMAX);
        hist[HISTMAX - 1][INMAX] = 0;
    }
}

static void history_line(int up)
{
    if (hcount == 0) return;
    if (hpos == -1) {
        strncpy(hsave, input, INMAX);
        hsave[INMAX] = 0;
        hpos = hcount;
    }

    if (up) {
        if (hpos > 0) hpos--;
    } else {
        if (hpos < hcount) hpos++;
    }

    if (hpos >= hcount) {
        strncpy(input, hsave, INMAX);
        input[INMAX] = 0;
        hpos = -1;
    } else {
        strncpy(input, hist[hpos], INMAX);
        input[INMAX] = 0;
    }
    ilen = (int)strlen(input);
    icur = ilen;
}

static int term_key(window_t *w, int k)
{
    (void)w;

    if (k == '\n') {
        char cmd[INMAX + 1];
        strncpy(cmd, input, INMAX);
        cmd[INMAX] = 0;

        char pr[128];
        prompt(pr, sizeof(pr));
        print(pr);
        print(input);
        putc_('\n');

        if (ilen > 0) history_add(input);
        hpos = -1;
        ilen = icur = 0;
        input[0] = 0;

        run(cmd);
        return 1;
    }

    if (k == 3) {                       /* Ctrl+C */
        print("^C\n");
        ilen = icur = 0;
        input[0] = 0;
        hpos = -1;
        return 1;
    }
    if (k == 12) { clear_screen(); return 1; }   /* Ctrl+L */
    if (k == 19) { desktop_save(); return 1; }   /* Ctrl+S */

    if (k == '\b') {
        if (icur > 0) {
            for (int i = icur - 1; i < ilen; i++) input[i] = input[i + 1];
            ilen--; icur--;
            input[ilen] = 0;
        }
        return 1;
    }
    if (k == KEY_DEL) {
        if (icur < ilen) {
            for (int i = icur; i < ilen; i++) input[i] = input[i + 1];
            ilen--;
            input[ilen] = 0;
        }
        return 1;
    }
    if (k == KEY_LEFT)  { if (icur > 0) icur--; return 1; }
    if (k == KEY_RIGHT) { if (icur < ilen) icur++; return 1; }
    if (k == KEY_HOME)  { icur = 0; return 1; }
    if (k == KEY_END)   { icur = ilen; return 1; }
    if (k == KEY_UP)    { history_line(1); return 1; }
    if (k == KEY_DOWN)  { history_line(0); return 1; }

    if (k >= 32 && k < 127 && ilen < INMAX) {
        for (int i = ilen; i > icur; i--) input[i] = input[i - 1];
        input[icur++] = (char)k;
        ilen++;
        input[ilen] = 0;
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------- init */

window_t *app_terminal(void)
{
    if (g_term) {
        wm_show(g_term);
        return g_term;
    }

    g_term = wm_new(20, 30, 640, 380, "Terminal");
    if (!g_term) return NULL;
    g_term->app_id = APP_TERM;
    g_term->draw = term_draw;
    g_term->key = term_key;
    g_term->click = term_click;

    clear_screen();
    print("DenemeOS 0.2  -  help yazarak komutlari gorebilirsiniz.\n");
    if (storage_loaded()) {
        print("Disk snapshot hazir (");
        print_ll((long long)storage_bytes());
        print(" bayt).\n");
    } else if (storage_present()) {
        print("Sanal disk hazir, henuz kurulum yok. F7 ile kurulum.\n");
    } else {
        print("Disk bulunamadi: ISO/ram modu, degisiklikler kalici degil.\n");
    }
    print("F1 Terminal  F2 Dosyalar  F3 Editor  F4 Hesap\n");
    print("F5 Ayarlar  F6 Cop  F7 Kurulum  F8 Hakkinda  F9 Menu\n");
    print("F10 diske kaydet   F11 buyut   Sag tik: masaustu menusu\n\n");
    return g_term;
}
