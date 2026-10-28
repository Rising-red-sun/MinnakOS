#ifndef SERIAL_H
#define SERIAL_H
void serial_init(void);
void serial_putc(char c);
void serial_print(const char *s);
char serial_getc(void); // blocking
int serial_has_char(void);
#endif
