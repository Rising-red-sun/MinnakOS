#ifndef DESKTOP_H
#define DESKTOP_H

void desktop_init(void);
void desktop_run(void);

/* Shared helpers used by applications. */
void desktop_toast(const char *msg);
void desktop_save(void);
int  desktop_theme(void);
void desktop_set_theme(int index);
int  desktop_theme_count(void);

#endif
