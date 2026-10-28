#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>
#include "limine.h"

void console_init(struct limine_framebuffer *fb, uint64_t hhdm_offset, int have_hhdm);
void console_putc(char c);
void console_print(const char *s);
void console_clear(void);
void console_set_color(uint32_t fg, uint32_t bg);
#endif
