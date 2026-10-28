#include "keyboard.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port)
{
    uint8_t r;
    __asm__ volatile ("inb %1,%0" : "=a"(r) : "Nd"(port));
    return r;
}

static const char lower[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char upper[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0,'|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#define KBUF 128
static int  kbuf[KBUF];
static volatile int khead = 0, ktail = 0;

static int shift = 0, ctrl = 0, caps = 0, e0 = 0;

static void push(int c)
{
    int n = (khead + 1) % KBUF;
    if (n == ktail) return;              /* full: drop */
    kbuf[khead] = c;
    khead = n;
}

static void handle(uint8_t sc)
{
    /* extended prefix: arrows / home / end / delete / right ctrl */
    if (sc == 0xE0) { e0 = 1; return; }

    if (e0) {
        e0 = 0;
        if (sc & 0x80) {
            if (sc == 0x9D) ctrl = 0;    /* right ctrl release */
            return;
        }
        if (sc == 0x1D) { ctrl = 1; return; }
        switch (sc) {
        case 0x48: push(KEY_UP);    return;
        case 0x50: push(KEY_DOWN);  return;
        case 0x4B: push(KEY_LEFT);  return;
        case 0x4D: push(KEY_RIGHT); return;
        case 0x47: push(KEY_HOME);  return;
        case 0x4F: push(KEY_END);   return;
        case 0x53: push(KEY_DEL);   return;
        case 0x49: push(KEY_PGUP);  return;
        case 0x51: push(KEY_PGDN);  return;
        default: return;
        }
    }

    if (sc == 0x2A || sc == 0x36) { shift = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return; }
    if (sc == 0x1D) { ctrl = 1; return; }
    if (sc == 0x9D) { ctrl = 0; return; }
    if (sc == 0x3A) { caps = !caps; return; }

    if ((sc & 0x80) == 0 && sc >= 0x3B && sc <= 0x44) {   /* F1..F10 */
        push(KEY_F1 + (sc - 0x3B));
        return;
    }
    if (sc == 0x57) { push(KEY_F11); return; }
    if (sc == 0x58) { push(KEY_F12); return; }

    if (sc & 0x80) return;               /* other releases */

    char c = shift ? upper[sc] : lower[sc];
    if (!c) return;

    if (ctrl) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 1);
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 1);
    } else if (caps && c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    push((unsigned char)c);
}

void keyboard_init(void)
{
    int guard = 64;
    while ((inb(0x64) & 1) && guard--) (void)inb(0x60);
    khead = ktail = 0;
    shift = ctrl = caps = e0 = 0;
}

/* Drain controller bytes that belong to the keyboard (AUX bit clear). */
void keyboard_poll(void)
{
    for (;;) {
        uint8_t st = inb(0x64);
        if (!(st & 0x01)) break;
        if (st & 0x20) break;            /* aux byte: mouse's business */
        handle(inb(0x60));
    }
}

int keyboard_has_key(void)
{
    keyboard_poll();
    return khead != ktail;
}

int keyboard_getkey(void)
{
    keyboard_poll();
    if (khead == ktail) return 0;
    int k = kbuf[ktail];
    ktail = (ktail + 1) % KBUF;
    return k;
}

int keyboard_has_char(void) { return keyboard_has_key(); }

char keyboard_getchar(void)
{
    int k = keyboard_getkey();
    return (k > 0 && k < 0x80) ? (char)k : 0;
}
