#include "ui.h"
#include "gfx.h"
#include "string.h"

int ui_hit(int x, int y, int w, int h, int px, int py)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

void ui_fill(int x, int y, int w, int h, uint32_t color) { gfx_fill_rect(x, y, w, h, color); }

void ui_button_enabled(int x, int y, int w, int h, const char *label, uint32_t bg, int enabled)
{
    if (!enabled) bg = gfx_rgb(165, 165, 165);

    uint32_t light = gfx_rgb(
        (uint8_t)(((bg >> 16) & 0xFF) + 40 > 255 ? 255 : ((bg >> 16) & 0xFF) + 40),
        (uint8_t)(((bg >> 8) & 0xFF) + 40 > 255 ? 255 : ((bg >> 8) & 0xFF) + 40),
        (uint8_t)((bg & 0xFF) + 40 > 255 ? 255 : (bg & 0xFF) + 40));
    uint32_t dark = gfx_rgb(
        (uint8_t)(((bg >> 16) & 0xFF) > 40 ? ((bg >> 16) & 0xFF) - 40 : 0),
        (uint8_t)(((bg >> 8) & 0xFF) > 40 ? ((bg >> 8) & 0xFF) - 40 : 0),
        (uint8_t)((bg & 0xFF) > 40 ? (bg & 0xFF) - 40 : 0));

    gfx_fill_rect(x, y, w, h, bg);
    gfx_fill_rect(x, y, w, 1, light);
    gfx_fill_rect(x, y, 1, h, light);
    gfx_fill_rect(x, y + h - 1, w, 1, dark);
    gfx_fill_rect(x + w - 1, y, 1, h, dark);

    int tw = (int)strlen(label) * 8;
    gfx_draw_text(x + (w - tw) / 2, y + (h - 8) / 2, label, 0xFFFFFF, bg);
}

void ui_button(int x, int y, int w, int h, const char *label, uint32_t bg)
{
    ui_button_enabled(x, y, w, h, label, bg, 1);
}

void ui_text(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    gfx_draw_text(x, y, s, fg, bg);
}

void ui_text_center(int x, int y, int w, const char *s, uint32_t fg, uint32_t bg)
{
    int tw = (int)strlen(s) * 8;
    gfx_draw_text(x + (w - tw) / 2, y, s, fg, bg);
}

void ui_text_right(int x, int y, int w, const char *s, uint32_t fg, uint32_t bg)
{
    int tw = (int)strlen(s) * 8;
    gfx_draw_text(x + w - tw, y, s, fg, bg);
}

void ui_itoa(int v, char *out)
{
    char tmp[16];
    int n = 0, neg = 0;
    unsigned u;
    if (v < 0) { neg = 1; u = (unsigned)(-v); } else u = (unsigned)v;
    if (!u) tmp[n++] = '0';
    while (u && n < 15) { tmp[n++] = (char)('0' + (u % 10)); u /= 10; }
    int k = 0;
    if (neg) out[k++] = '-';
    while (n > 0) out[k++] = tmp[--n];
    out[k] = 0;
}

int ui_streq(const char *a, const char *b) { return strcmp(a, b) == 0; }
