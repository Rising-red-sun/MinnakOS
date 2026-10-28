#include "gfx.h"
#include "mem.h"
#include <string.h>

extern char font8x8_basic[128][8];

static uint32_t *front = NULL;   /* real framebuffer */
static uint32_t *back  = NULL;   /* off-screen buffer  */
static int W = 0, H = 0, pitch = 0;

/* dirty rectangle (x1/y1 exclusive) */
static int dirty = 0;
static int d_x0 = 0, d_y0 = 0, d_x1 = 0, d_y1 = 0;

#define CUR_W 14
#define CUR_H 18
static uint32_t cur_bg[CUR_W * CUR_H];
static int cur_x = -1, cur_y = -1, cur_active = 0;

static void mark(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    if (!dirty) {
        dirty = 1;
        d_x0 = x0; d_y0 = y0; d_x1 = x1; d_y1 = y1;
        return;
    }
    if (x0 < d_x0) d_x0 = x0;
    if (y0 < d_y0) d_y0 = y0;
    if (x1 > d_x1) d_x1 = x1;
    if (y1 > d_y1) d_y1 = y1;
}

void gfx_init(struct limine_framebuffer *f, uint64_t hhdm, int have_hhdm)
{
    (void)hhdm; (void)have_hhdm;
    front = NULL; back = NULL; W = 0; H = 0; pitch = 0;
    dirty = 0; cur_active = 0; cur_x = -1; cur_y = -1;
    if (!f || !f->address) return;

    W = (int)f->width;
    H = (int)f->height;
    pitch = (int)(f->pitch / 4);
    if (pitch < W) pitch = W;
    front = (uint32_t *)(uintptr_t)f->address;

    size_t bytes = (size_t)pitch * (size_t)H * sizeof(uint32_t);
    back = (uint32_t *)kmalloc(bytes);
    if (!back) back = front;
    else memset(back, 0, bytes);

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            front[(size_t)y * pitch + x] = 0;
}

int gfx_width(void)  { return W; }
int gfx_height(void) { return H; }

static inline uint32_t *buf(void) { return back ? back : front; }

void gfx_putpixel(int x, int y, uint32_t color)
{
    uint32_t *m = buf();
    if (!m || x < 0 || y < 0 || x >= W || y >= H) return;
    m[(size_t)y * pitch + x] = color;
    mark(x, y, x + 1, y + 1);
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    uint32_t *m = buf();
    if (!m || w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > W) x1 = W;
    int y1 = y + h; if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = m + (size_t)yy * pitch;
        for (int xx = x0; xx < x1; xx++) row[xx] = color;
    }
    mark(x0, y0, x1, y1);
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0) return;
    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

void gfx_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = dx - dy;

    for (;;) {
        gfx_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void gfx_fill_circle(int cx, int cy, int r, uint32_t color)
{
    if (r <= 0) return;
    int rr = r * r;
    for (int yy = -r; yy <= r; yy++) {
        int rem = rr - yy * yy;
        int xx = 0;
        while ((xx + 1) * (xx + 1) <= rem) xx++;
        gfx_fill_rect(cx - xx, cy + yy, xx * 2 + 1, 1, color);
    }
}

void gfx_draw_circle(int cx, int cy, int r, uint32_t color)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_putpixel(cx + x, cy + y, color);
        gfx_putpixel(cx + y, cy + x, color);
        gfx_putpixel(cx - y, cy + x, color);
        gfx_putpixel(cx - x, cy + y, color);
        gfx_putpixel(cx - x, cy - y, color);
        gfx_putpixel(cx - y, cy - x, color);
        gfx_putpixel(cx + y, cy - x, color);
        gfx_putpixel(cx + x, cy - y, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void gfx_clear(uint32_t color)
{
    gfx_fill_rect(0, 0, W, H, color);
}

/* glyph = 8x8, bit 0 = leftmost pixel */
static void glyph(int x, int y, unsigned char c, uint32_t color, uint32_t bg, int scale)
{
    if (c >= 128) c = '?';
    for (int yy = 0; yy < 8; yy++) {
        unsigned char bits = (unsigned char)font8x8_basic[c][yy];
        for (int xx = 0; xx < 8; xx++) {
            uint32_t col = (bits & (1u << xx)) ? color : bg;
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    gfx_putpixel(x + xx * scale + sx, y + yy * scale + sy, col);
        }
    }
}

void gfx_draw_text(int x, int y, const char *s, uint32_t color, uint32_t bg)
{
    int cx = x;
    while (s && *s) {
        unsigned char c = (unsigned char)*s++;
        if (c == '\n') { cx = x; y += 8; continue; }
        glyph(cx, y, c, color, bg, 1);
        cx += 8;
    }
}

void gfx_draw_text_scaled(int x, int y, const char *s, uint32_t color, uint32_t bg, int scale)
{
    if (scale < 1) scale = 1;
    int cx = x;
    while (s && *s) {
        unsigned char c = (unsigned char)*s++;
        if (c == '\n') { cx = x; y += 8 * scale; continue; }
        glyph(cx, y, c, color, bg, scale);
        cx += 8 * scale;
    }
}

uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* ---------------------------------------------------------------- cursor */

/* Arrow shape relative to the hot spot (tip at 0,0). */
static int cur_shape(int x, int y)
{
    if (x < 0 || y < 0 || x >= 12 || y >= 16) return 0;
    if (y <= 11) return x <= y / 2;
    return x < 3;
}

void gfx_cursor_reset(void)
{
    cur_active = 0;
    cur_x = -1;
    cur_y = -1;
}

void gfx_cursor_draw(int x, int y)
{
    uint32_t *m = buf();
    if (!m || W <= 0 || H <= 0) return;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= W) x = W - 1;
    if (y >= H) y = H - 1;

    if (cur_active && x == cur_x && y == cur_y) return;

    int bx = x - 1, by = y - 1;

    /* 1. restore what the previous cursor covered */
    if (cur_active) {
        int px = cur_x - 1, py = cur_y - 1;
        for (int j = 0; j < CUR_H; j++) {
            int yy = py + j;
            if (yy < 0 || yy >= H) continue;
            for (int i = 0; i < CUR_W; i++) {
                int xx = px + i;
                if (xx < 0 || xx >= W) continue;
                m[(size_t)yy * pitch + xx] = cur_bg[j * CUR_W + i];
            }
        }
        mark(px, py, px + CUR_W, py + CUR_H);
        cur_active = 0;
    }

    /* 2. save the background (clipped at the screen edges) */
    for (int j = 0; j < CUR_H; j++) {
        int yy = by + j;
        if (yy < 0 || yy >= H) continue;
        for (int i = 0; i < CUR_W; i++) {
            int xx = bx + i;
            if (xx < 0 || xx >= W) continue;
            cur_bg[j * CUR_W + i] = m[(size_t)yy * pitch + xx];
        }
    }

    /* 3. draw a black arrow with a white outline, visible everywhere */
    for (int j = 0; j < CUR_H; j++) {
        int yy = by + j;
        if (yy < 0 || yy >= H) continue;
        for (int i = 0; i < CUR_W; i++) {
            int xx = bx + i;
            if (xx < 0 || xx >= W) continue;
            int dx = xx - x;
            int dy = yy - y;
            if (cur_shape(dx, dy)) continue;
            int near = 0;
            for (int sy = -1; sy <= 1 && !near; sy++)
                for (int sx = -1; sx <= 1 && !near; sx++)
                    if (cur_shape(dx + sx, dy + sy)) near = 1;
            if (near)
                m[(size_t)yy * pitch + xx] = 0xFFFFFF;
        }
    }
    for (int j = 0; j < CUR_H; j++) {
        int yy = by + j;
        if (yy < 0 || yy >= H) continue;
        for (int i = 0; i < CUR_W; i++) {
            int xx = bx + i;
            if (xx < 0 || xx >= W) continue;
            int dx = xx - x;
            int dy = yy - y;
            if (cur_shape(dx, dy))
                m[(size_t)yy * pitch + xx] = 0x000000;
        }
    }

    mark(bx, by, bx + CUR_W, by + CUR_H);
    cur_x = x; cur_y = y; cur_active = 1;
}

void gfx_present(void)
{
    if (!dirty) return;
    if (!front || !back || back == front) { dirty = 0; return; }

    int w = d_x1 - d_x0;
    if (w > 0) {
        for (int y = d_y0; y < d_y1; y++) {
            memcpy(front + (size_t)y * pitch + d_x0,
                   back  + (size_t)y * pitch + d_x0,
                   (size_t)w * sizeof(uint32_t));
        }
    }
    dirty = 0;
}
