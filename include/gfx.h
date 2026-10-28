#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "limine.h"

void gfx_init(struct limine_framebuffer *fb, uint64_t hhdm, int have_hhdm);
int gfx_width(void);
int gfx_height(void);
void gfx_putpixel(int x, int y, uint32_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_fill_circle(int cx, int cy, int r, uint32_t color);
void gfx_draw_circle(int cx, int cy, int r, uint32_t color);
void gfx_draw_text(int x, int y, const char *s, uint32_t color, uint32_t bg);
void gfx_draw_text_scaled(int x, int y, const char *s, uint32_t color, uint32_t bg, int scale);
void gfx_clear(uint32_t color);
uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Cursor is composited on the back buffer and restored from a saved
 * background, so mouse movement only touches a tiny dirty region. */
void gfx_cursor_draw(int x, int y);
void gfx_cursor_reset(void);

/* Flush only the dirty rectangle from the back buffer to the framebuffer. */
void gfx_present(void);
#endif
