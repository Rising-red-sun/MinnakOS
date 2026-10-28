#include "serial.h"
#include <stdint.h>

static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t r;__asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p));return r;}

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1+1, 0x00);
    outb(COM1+3, 0x80);
    outb(COM1+0, 0x01);
    outb(COM1+1, 0x00);
    outb(COM1+3, 0x03);
    outb(COM1+2, 0xC7);
    outb(COM1+4, 0x0B);
}

void serial_putc(char c) {
    int t=100000;
    while (t-- && !(inb(COM1+5) & 0x20)) {}
    outb(COM1, c);
}

void serial_print(const char *s) {
    while (s && *s) serial_putc(*s++);
}

char serial_getc(void) {
    while (!(inb(COM1+5) & 0x01)) {}
    return inb(COM1);
}

int serial_has_char(void) {
    return (inb(COM1+5) & 0x01) != 0;
}
