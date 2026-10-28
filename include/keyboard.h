#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Special keys are >= 0x80 so they can share the same ring buffer as
 * normal characters (0 = no key available). */
#define KEY_F1    0x81
#define KEY_F2    0x82
#define KEY_F3    0x83
#define KEY_F4    0x84
#define KEY_F5    0x85
#define KEY_F6    0x86
#define KEY_F7    0x87
#define KEY_F8    0x88
#define KEY_F9    0x89
#define KEY_F10   0x8A
#define KEY_F11   0x8B
#define KEY_F12   0x8C
#define KEY_UP    0x90
#define KEY_DOWN  0x91
#define KEY_LEFT  0x92
#define KEY_RIGHT 0x93
#define KEY_HOME  0x94
#define KEY_END   0x95
#define KEY_DEL   0x96
#define KEY_PGUP  0x97
#define KEY_PGDN  0x98

void keyboard_init(void);
void keyboard_poll(void);
int  keyboard_has_key(void);
int  keyboard_getkey(void);

/* compatibility helpers (0 when the buffer is empty) */
int  keyboard_has_char(void);
char keyboard_getchar(void);

#endif
