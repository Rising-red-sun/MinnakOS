#include "console.h"
#include "serial.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

#include "font8x8_basic.h"

static struct limine_framebuffer *fb = NULL;
static uint64_t hhdm = 0;
static int have_hhdm_flag = 0;

static int use_fb = 0;
static uint32_t *fbmem = NULL;
static uint32_t width = 0, height = 0, pitch_pixels = 0;
static uint32_t fg_color = 0x00FF00;
static uint32_t bg_color = 0x000000;

static int cur_col = 0, cur_row = 0;
static int cols = 80, rows = 25;

// VGA text mode
static volatile uint16_t *vga = NULL;

static void vga_putc(char c) {
    if (!vga) return;
    if (c == '\n') { cur_col = 0; cur_row++; }
    else if (c == '\r') { cur_col = 0; }
    else if (c == '\b') {
        if (cur_col > 0) cur_col--;
        vga[cur_row*80 + cur_col] = (uint16_t)((bg_color?0:0) | (cur_col?0:0));
        // simpler: space
        vga[cur_row*80 + cur_col] = ' ' | (0x0F << 8);
    } else if (c == '\t') {
        cur_col = (cur_col + 8) & ~7;
    } else {
        vga[cur_row*80 + cur_col] = (uint16_t)c | (0x0F << 8);
        cur_col++;
    }
    if (cur_col >= 80) { cur_col = 0; cur_row++; }
    if (cur_row >= 25) {
        // scroll
        for (int i = 0; i < 24*80; i++) vga[i] = vga[i+80];
        for (int i = 24*80; i < 25*80; i++) vga[i] = ' ' | (0x0F<<8);
        cur_row = 24;
    }
}

static void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= width || y >= height) return;
    fbmem[y * (pitch_pixels) + x] = color;
}

static void fb_draw_char(char c, int col, int row) {
    if ((unsigned char)c > 127) c = '?';
    for (int yy = 0; yy < 8; yy++) {
        unsigned char bits = font8x8_basic[(int)c][yy];
        for (int xx = 0; xx < 8; xx++) {
            uint32_t color = (bits & (1 << xx)) ? fg_color : bg_color;
            fb_put_pixel(col*8 + xx, row*8 + yy, color);
        }
    }
}

static void fb_scroll(void) {
    // move up one text row (8 pixel rows)
    uint32_t row_pixels = 8;
    // We'll memmove by pixel rows
    for (uint32_t y = row_pixels; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            fbmem[(y - row_pixels) * pitch_pixels + x] = fbmem[y * pitch_pixels + x];
        }
    }
    // clear last 8 rows
    for (uint32_t y = height - row_pixels; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            fbmem[y * pitch_pixels + x] = bg_color;
        }
    }
}

static void fb_putc(char c) {
    if (c == '\n') { cur_col = 0; cur_row++; }
    else if (c == '\r') { cur_col = 0; }
    else if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
            fb_draw_char(' ', cur_col, cur_row);
        }
    } else if (c == '\t') {
        cur_col = (cur_col + 8) & ~7;
    } else {
        fb_draw_char(c, cur_col, cur_row);
        cur_col++;
    }
    if (cur_col >= (int)cols) { cur_col = 0; cur_row++; }
    if (cur_row >= (int)rows) {
        fb_scroll();
        cur_row = rows - 1;
    }
}

void console_init(struct limine_framebuffer *f, uint64_t hhdm_offset, int have_hhdm) {
    fb = f;
    hhdm = hhdm_offset;
    have_hhdm_flag = have_hhdm;
    if (fb && fb->address) {
        use_fb = 1;
        fbmem = (uint32_t *)(uintptr_t)fb->address;
        width = fb->width;
        height = fb->height;
        pitch_pixels = fb->pitch / 4;
        cols = width / 8;
        rows = height / 8;
        if (cols < 1) cols = 1;
        if (rows < 1) rows = 1;
        // clear
        for (uint32_t y = 0; y < height; y++)
            for (uint32_t x = 0; x < width; x++)
                fbmem[y*pitch_pixels + x] = bg_color;
    } else {
        use_fb = 0;
        if (have_hhdm) vga = (volatile uint16_t *)(hhdm + 0xB8000);
        else vga = (volatile uint16_t *)0xB8000;
        // clear
        for (int i = 0; i < 80*25; i++) vga[i] = ' ' | (0x0F << 8);
    }
    cur_col = 0; cur_row = 0;
}

void console_putc(char c) {
    serial_putc(c);
    if (use_fb) fb_putc(c);
    else vga_putc(c);
}

void console_print(const char *s) {
    while (*s) console_putc(*s++);
}

void console_clear(void) {
    if (use_fb) {
        for (uint32_t y = 0; y < height; y++)
            for (uint32_t x = 0; x < width; x++)
                fbmem[y*pitch_pixels + x] = bg_color;
    } else {
        for (int i = 0; i < 80*25; i++) vga[i] = ' ' | (0x0F << 8);
    }
    cur_col = 0; cur_row = 0;
}

void console_set_color(uint32_t fg, uint32_t bg) {
    fg_color = fg;
    bg_color = bg;
}
