#ifndef UI_H
#define UI_H

#include <stdint.h>

int  ui_hit(int x, int y, int w, int h, int px, int py);
void ui_fill(int x, int y, int w, int h, uint32_t color);
void ui_button(int x, int y, int w, int h, const char *label, uint32_t bg);
void ui_button_enabled(int x, int y, int w, int h, const char *label, uint32_t bg, int enabled);
void ui_text(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void ui_text_center(int x, int y, int w, const char *s, uint32_t fg, uint32_t bg);
void ui_text_right(int x, int y, int w, const char *s, uint32_t fg, uint32_t bg);
void ui_itoa(int v, char *out);
int  ui_streq(const char *a, const char *b);

#endif
