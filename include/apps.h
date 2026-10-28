#ifndef APPS_H
#define APPS_H

#include "wm.h"
#include "fs.h"

enum {
    APP_NONE = 0,
    APP_TERM,
    APP_FILES,
    APP_EDITOR,
    APP_CALC,
    APP_SETTINGS,
    APP_TRASH,
    APP_INSTALLER,
    APP_ABOUT
};

extern window_t *g_term, *g_files, *g_editor, *g_calc;
extern window_t *g_set, *g_trash, *g_inst, *g_about;

window_t *app_terminal(void);
window_t *app_files(void);
window_t *app_editor(void);
window_t *app_calc(void);
window_t *app_settings(void);
window_t *app_trash(void);
window_t *app_installer(void);
window_t *app_about(void);

void files_open_dir(fs_node *d);
void editor_open(fs_node *f);
void trash_refresh(void);

#endif
