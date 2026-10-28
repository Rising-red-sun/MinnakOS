#ifndef MOUSE_H
#define MOUSE_H

typedef struct {
    int x, y;
    unsigned char buttons;
    unsigned char pressed;    /* buttons that went down during this poll  */
    unsigned char released;   /* buttons that went up during this poll    */
    int dx, dy;
    int updated;
} mouse_state_t;

void mouse_init(void);
void mouse_set_screen(int w, int h);
void mouse_poll(void);
mouse_state_t mouse_state(void);

/* 25..200 (100 = 1:1). Lower = calmer pointer. */
void mouse_set_sensitivity(int pct);
int mouse_get_sensitivity(void);

#endif
