#include "mouse.h"
#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile ("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile ("inb %1,%0" : "=a"(r) : "Nd"(p)); return r; }

static mouse_state_t ms;
static int screen_w = 800, screen_h = 600;

static int sensitivity = 100;     /* percent */
static int acc_x = 0, acc_y = 0;  /* sub-pixel remainder */
static int moved_once = 0;

static uint8_t pkt[3];
static int pidx = 0;
static int idle_polls = 0;

static void wait_write(void) { int t = 200000; while (t-- && (inb(0x64) & 0x02)) {} }
static void wait_read(void)  { int t = 200000; while (t-- && !(inb(0x64) & 0x01)) {} }
static uint8_t rd(void) { wait_read(); return inb(0x60); }
static void small_delay(void) { for (volatile int i = 0; i < 120000; i++) {} }

static void wr(uint8_t v)
{
    wait_write();
    outb(0x64, 0xD4);
    wait_write();
    outb(0x60, v);
}

static void flush(void)
{
    int guard = 256;
    while ((inb(0x64) & 0x01) && guard--) (void)inb(0x60);
}

static void flush_wait(void)
{
    /* Let late ACKs/self-test bytes arrive, then drain them. */
    for (int round = 0; round < 8; round++) {
        small_delay();
        flush();
    }
}

static void cmd(uint8_t c, uint8_t arg, int has_arg)
{
    flush();
    wr(c);
    (void)rd();                     /* ACK */
    if (has_arg) {
        wr(arg);
        (void)rd();                 /* ACK */
    }
    flush();
}

void mouse_init(void)
{
    /* disable aux port while configuring */
    wait_write(); outb(0x64, 0xA7);
    flush();

    /* enable aux port */
    wait_write(); outb(0x64, 0xA8);

    /* controller config: IRQs off (we poll), mouse clock enabled */
    wait_write(); outb(0x64, 0x20);
    wait_read();
    uint8_t cfg = inb(0x60);
    cfg &= (uint8_t)~0x03;          /* no IRQ1 / IRQ12 */
    cfg &= (uint8_t)~0x20;          /* bit 5 = disable mouse clock: clear it */
    wait_write(); outb(0x64, 0x60);
    wait_write(); outb(0x60, cfg);

    /* reset device; the answer can be ACK, self-test and an ID */
    wr(0xFF);
    flush_wait();

    cmd(0xF6, 0, 0);                /* defaults            */
    cmd(0xF3, 0x64, 1);             /* 100 samples/second  */
    cmd(0xE8, 0x03, 1);             /* 8 counts/mm         */
    cmd(0xF4, 0, 0);                /* enable reporting    */
    flush_wait();

    pidx = 0;
    idle_polls = 0;
    acc_x = acc_y = 0;
    moved_once = 0;
    ms.buttons = 0;
    ms.pressed = 0;
    ms.released = 0;
    ms.dx = ms.dy = 0;
    ms.updated = 0;

    if (!moved_once) {
        ms.x = screen_w / 2;
        ms.y = screen_h / 2;
    }
}

void mouse_poll(void)
{
    ms.updated = 0;
    ms.pressed = 0;
    ms.released = 0;

    int got_byte = 0;
    for (;;) {
        uint8_t st = inb(0x64);
        if (!(st & 0x01)) break;
        if (!(st & 0x20)) break;    /* keyboard byte: leave it for keyboard.c */

        uint8_t b = inb(0x60);
        got_byte = 1;

        if (pidx == 0) {
            /* first byte: bit 3 must be set, overflow bits must be clear.
             * This also rejects stray ACK bytes (0xFA) after init. */
            if ((b & 0xC8) != 0x08) continue;
        }
        pkt[pidx++] = b;

        if (pidx < 3) continue;
        pidx = 0;

        uint8_t b1 = pkt[0], b2 = pkt[1], b3 = pkt[2];
        if (b1 & 0xC0) continue;         /* overflow in packet */

        unsigned char buttons = b1 & 0x07;
        int dx = (int8_t)b2;
        int dy = (int8_t)b3;

        /* mild acceleration for fast movements, precise for slow ones */
        if (dx >= 4 || dx <= -4) dx += dx / 4;
        if (dy >= 4 || dy <= -4) dy += dy / 4;

        /* fractional scaling: slow movements are not rounded to zero */
        int total_x = acc_x + dx * sensitivity;
        int total_y = acc_y + dy * sensitivity;
        int mx = total_x / 100;
        int my = total_y / 100;
        acc_x = total_x - mx * 100;
        acc_y = total_y - my * 100;

        if (mx >  40) mx =  40;
        if (mx < -40) mx = -40;
        if (my >  40) my =  40;
        if (my < -40) my = -40;

        if (mx || my) {
            ms.x += mx;
            ms.y -= my;                  /* PS/2: positive dy = up */
            if (ms.x < 0) ms.x = 0;
            if (ms.y < 0) ms.y = 0;
            if (ms.x >= screen_w) ms.x = screen_w - 1;
            if (ms.y >= screen_h) ms.y = screen_h - 1;
            ms.dx = mx;
            ms.dy = my;
            ms.updated = 1;
            moved_once = 1;
        }
        unsigned char diff = (unsigned char)(buttons ^ ms.buttons);
        if (diff) {
            ms.pressed  |= (unsigned char)(diff & buttons);
            ms.released |= (unsigned char)(diff & (~buttons & 0x07));
            ms.updated = 1;
        }
        ms.buttons = buttons;
    }

    /* A dropped byte can leave a half packet behind.  Packets arrive
     * back-to-back, so a long idle period means the rest will never come. */
    if (got_byte) {
        idle_polls = 0;
    } else if (pidx && ++idle_polls > 20000) {
        pidx = 0;
        idle_polls = 0;
    }
}

mouse_state_t mouse_state(void) { return ms; }

void mouse_set_screen(int w, int h)
{
    if (w < 32) w = 32;
    if (h < 32) h = 32;
    screen_w = w; screen_h = h;
    if (!moved_once) { ms.x = w / 2; ms.y = h / 2; }
    if (ms.x >= w) ms.x = w - 1;
    if (ms.y >= h) ms.y = h - 1;
}

void mouse_set_sensitivity(int pct)
{
    if (pct < 25) pct = 25;
    if (pct > 200) pct = 200;
    sensitivity = pct;
}

int mouse_get_sensitivity(void) { return sensitivity; }
