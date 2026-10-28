#ifndef MENU_H
#define MENU_H

#define MENU_MAX_ITEMS 14

typedef void (*menu_action_t)(void);

void menu_open(int x, int y, const char *title,
               const char *const *labels, const menu_action_t *actions, int count);
void menu_close(void);
int  menu_visible(void);
int  menu_height(const char *title, const char *const *labels, int count);
int  menu_hover(int x, int y);      /* item index, -1 outside/separator */
int  menu_click(int x, int y);      /* 1 = click consumed (action ran)  */
int  menu_key(int key);             /* 1 = key consumed                  */
void menu_draw(int mouse_x, int mouse_y);

#endif
