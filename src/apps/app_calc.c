#include "apps.h"
#include "gfx.h"
#include "ui.h"
#include "string.h"
#include "keyboard.h"
#include <stdint.h>

window_t *g_calc = NULL;

static char disp[24] = "0";
static long long accum = 0;
static char pending = 0;
static int fresh = 1;
static int error_state = 0;

static const char *key_labels[20] = {
    "C",  "<",  "%",  "/",
    "7",  "8",  "9",  "*",
    "4",  "5",  "6",  "-",
    "1",  "2",  "3",  "+",
    "0",  "00", "+/-", "="
};

static void ll_str(long long v, char *out)
{
    char tmp[32];
    int n = 0;
    unsigned long long u;
    int neg = 0;
    if (v < 0) { neg = 1; u = (unsigned long long)(-v); }
    else u = (unsigned long long)v;
    if (!u) tmp[n++] = '0';
    while (u) { tmp[n++] = (char)('0' + u % 10); u /= 10; }
    int k = 0;
    if (neg) out[k++] = '-';
    while (n) out[k++] = tmp[--n];
    out[k] = 0;
}

static long long parse_ll(const char *s)
{
    long long v = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return neg ? -v : v;
}

static void reset_all(void)
{
    strncpy(disp, "0", sizeof(disp));
    accum = 0;
    pending = 0;
    fresh = 1;
    error_state = 0;
}

static void set_disp(long long v)
{
    ll_str(v, disp);
    fresh = 1;
}

static void input_digit(char c)
{
    if (error_state) reset_all();
    if (fresh) { disp[0] = 0; fresh = 0; }
    size_t n = strlen(disp);
    if (n == 0) {
        disp[0] = c; disp[1] = 0;
        return;
    }
    if (n == 1 && disp[0] == '0') {
        disp[0] = c;
        return;
    }
    if (n < sizeof(disp) - 1) {
        disp[n] = c;
        disp[n + 1] = 0;
    }
}

static void input_double_zero(void)
{
    input_digit('0');
    input_digit('0');
}

static void backspace(void)
{
    if (error_state) { reset_all(); return; }
    size_t n = strlen(disp);
    if (n <= 1) { strncpy(disp, "0", sizeof(disp)); fresh = 0; return; }
    disp[n - 1] = 0;
    fresh = 0;
}

static void negate(void)
{
    if (error_state) return;
    if (disp[0] == '0' && disp[1] == 0) return;
    if (disp[0] == '-') {
        for (int i = 0; disp[i]; i++) disp[i] = disp[i + 1];
    } else {
        size_t n = strlen(disp);
        if (n < sizeof(disp) - 1) {
            for (size_t i = n; i > 0; i--) disp[i] = disp[i - 1];
            disp[0] = '-';
        }
    }
    fresh = 0;
}

static void percent(void)
{
    if (error_state) return;
    long long v = parse_ll(disp);
    set_disp(v / 100);
}

static void apply_op(char op)
{
    if (error_state) reset_all();
    long long cur = parse_ll(disp);

    if (pending) {
        switch (pending) {
        case '+': accum += cur; break;
        case '-': accum -= cur; break;
        case '*': accum *= cur; break;
        case '/':
            if (cur == 0) {
                strncpy(disp, "Hata", sizeof(disp));
                error_state = 1;
                pending = 0;
                fresh = 1;
                return;
            }
            accum /= cur;
            break;
        default: accum = cur; break;
        }
    } else {
        accum = cur;
    }

    pending = op;
    set_disp(accum);
}

static void equals(void)
{
    if (error_state) return;
    if (!pending) { accum = parse_ll(disp); return; }
    long long cur = parse_ll(disp);
    long long r = accum;

    switch (pending) {
    case '+': r = accum + cur; break;
    case '-': r = accum - cur; break;
    case '*': r = accum * cur; break;
    case '/':
        if (cur == 0) {
            strncpy(disp, "Hata", sizeof(disp));
            error_state = 1;
            pending = 0;
            fresh = 1;
            return;
        }
        r = accum / cur;
        break;
    }
    accum = r;
    pending = 0;
    set_disp(r);
}

static void press_key_index(int idx)
{
    if (idx < 0 || idx >= 20) return;
    const char *k = key_labels[idx];

    if (k[0] >= '0' && k[0] <= '9' && k[1] == 0) input_digit(k[0]);
    else if (!strcmp(k, "00")) input_double_zero();
    else if (!strcmp(k, "C")) reset_all();
    else if (!strcmp(k, "<")) backspace();
    else if (!strcmp(k, "%")) percent();
    else if (!strcmp(k, "+/-")) negate();
    else if (!strcmp(k, "=")) equals();
    else apply_op(k[0]);
}

static void calc_draw(window_t *w)
{
    uint32_t bg = gfx_rgb(242, 245, 250);
    uint32_t panel = gfx_rgb(22, 28, 38);
    gfx_fill_rect(w->x + 2, w->y + 23, w->w - 4, w->h - 25, bg);

    gfx_fill_rect(w->x + 10, w->y + 28, w->w - 20, 48, panel);
    gfx_draw_rect(w->x + 10, w->y + 28, w->w - 20, 48, gfx_rgb(70, 80, 96));

    if (pending) {
        char e[48];
        ll_str(accum, e);
        strncat(e, " ", sizeof(e) - strlen(e) - 1);
        char op[2] = { pending, 0 };
        strncat(e, op, sizeof(e) - strlen(e) - 1);
        gfx_draw_text(w->x + 16, w->y + 33, e, gfx_rgb(130, 190, 230), panel);
    } else if (error_state) {
        gfx_draw_text(w->x + 16, w->y + 33, "islem hatasi", gfx_rgb(240, 120, 120), panel);
    }

    int scale = 2;
    int tw = (int)strlen(disp) * 8 * scale;
    if (tw > w->w - 36) { scale = 1; tw = (int)strlen(disp) * 8; }
    int dx = w->x + w->w - 16 - tw;
    gfx_draw_text_scaled(dx, w->y + 48, disp, gfx_rgb(245, 248, 255), panel, scale);

    int x0 = w->x + 10;
    int y0 = w->y + 84;
    int gap = 4;
    int bw = (w->w - 20 - gap * 3) / 4;
    int bh = 34;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int idx = r * 4 + c;
            const char *lab = key_labels[idx];
            uint32_t col = gfx_rgb(70, 86, 110);
            if (lab[0] >= '0' && lab[0] <= '9') col = gfx_rgb(50, 64, 86);
            if (!strcmp(lab, "C")) col = gfx_rgb(176, 56, 60);
            if (!strcmp(lab, "=")) col = gfx_rgb(0, 140, 80);
            if (!strcmp(lab, "/") || !strcmp(lab, "*") ||
                !strcmp(lab, "-") || !strcmp(lab, "+")) col = gfx_rgb(0, 116, 160);
            ui_button(x0 + c * (bw + gap), y0 + r * (bh + gap), bw, bh, lab, col);
        }
    }

    gfx_draw_text(w->x + 10, w->y + w->h - 16,
                  "klavye: 0-9  + - * /  Enter  C",
                  gfx_rgb(100, 110, 126), bg);
}

static void calc_click(window_t *w, int lx, int ly, int button)
{
    if (button != 1) return;
    int x0 = 10;
    int y0 = 84;
    int gap = 4;
    int bw = (w->w - 20 - gap * 3) / 4;
    int bh = 34;

    if (lx < x0 || ly < y0) return;
    int c = (lx - x0) / (bw + gap);
    int r = (ly - y0) / (bh + gap);
    if (c < 0 || c > 3 || r < 0 || r > 4) return;

    int px = x0 + c * (bw + gap);
    int py = y0 + r * (bh + gap);
    if (lx >= px + bw || ly >= py + bh) return;

    press_key_index(r * 4 + c);
}

static int calc_key(window_t *w, int k)
{
    (void)w;
    if (k >= '0' && k <= '9') { input_digit((char)k); return 1; }
    if (k == '+' || k == '-' || k == '*' || k == '/') { apply_op((char)k); return 1; }
    if (k == '\n' || k == '=') { equals(); return 1; }
    if (k == '\b' || k == KEY_DEL) { backspace(); return 1; }
    if (k == 'c' || k == 'C') { reset_all(); return 1; }
    if (k == '%') { percent(); return 1; }
    if (k == 'n' || k == 'N') { negate(); return 1; }

    return 0;
}

window_t *app_calc(void)
{
    if (g_calc) { wm_show(g_calc); return g_calc; }

    g_calc = wm_new(430, 80, 300, 340, "Hesap Makinesi");
    if (!g_calc) return NULL;
    g_calc->app_id = APP_CALC;
    g_calc->draw = calc_draw;
    g_calc->click = calc_click;
    g_calc->key = calc_key;
    reset_all();
    return g_calc;
}
