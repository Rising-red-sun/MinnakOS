#include "rtc.h"
#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile ("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile ("inb %1,%0" : "=a"(r) : "Nd"(p)); return r; }

static uint8_t cmos(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static int bcd2bin(uint8_t v) { return (v & 0x0F) + ((v >> 4) * 10); }

static void read_time(int *h, int *m)
{
    for (int i = 0; i < 100000; i++)
        if (!(cmos(0x0A) & 0x80)) break;      /* wait for update to finish */

    uint8_t status_b = cmos(0x0B);
    uint8_t rh = cmos(0x04);
    uint8_t rm = cmos(0x02);

    int hh, mm;
    if (status_b & 0x04) {                    /* binary mode */
        hh = rh; mm = rm;
    } else {                                  /* BCD */
        hh = bcd2bin((uint8_t)(rh & 0x7F));
        mm = bcd2bin((uint8_t)(rm & 0x7F));
    }
    if (!(status_b & 0x02) && (rh & 0x80))    /* 12h + PM */
        hh = (hh % 12) + 12;

    if (hh > 23 || mm > 59) { hh = 0; mm = 0; }
    *h = hh; *m = mm;
}

int rtc_hour(void)   { int h, m; read_time(&h, &m); return h; }
int rtc_minute(void) { int h, m; read_time(&h, &m); return m; }

void rtc_time(char *out)
{
    int h, m;
    read_time(&h, &m);
    out[0] = (char)('0' + (h / 10) % 10);
    out[1] = (char)('0' + h % 10);
    out[2] = ':';
    out[3] = (char)('0' + (m / 10) % 10);
    out[4] = (char)('0' + m % 10);
    out[5] = 0;
}
