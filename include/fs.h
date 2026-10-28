#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdint.h>

typedef struct fs_node {
    char name[64];
    int is_dir;
    char *data;
    size_t size;
    size_t cap;
    struct fs_node **children;
    size_t child_count;
    size_t child_cap;
    struct fs_node *parent;
} fs_node;

void fs_init(void);                          /* default tree, idempotent        */
void fs_defaults(void);                      /* create default tree in root     */
void fs_reset(void);                         /* fresh empty root                */
fs_node *fs_root(void);
fs_node *fs_cwd(void);
void fs_set_cwd(fs_node *n);

fs_node *fs_add(fs_node *parent, const char *name, int is_dir);
fs_node *fs_child(fs_node *dir, const char *name);
fs_node *fs_lookup(fs_node *base, const char *path);
fs_node *fs_ensure_dir(const char *abs_path); /* mkdir -p, returns the dir      */

int  fs_mkdir(const char *path);
int  fs_mkdir_p(const char *path);
int  fs_touch(const char *path);
int  fs_write_file(fs_node *n, const char *data, size_t len);
char *fs_read_file(fs_node *n);
int  fs_rm(const char *path);                /* hard delete, empty dirs only   */
int  fs_unlink(fs_node *n);                  /* detach from parent             */
int  fs_delete(fs_node *n);                  /* recursive permanent delete     */
int  fs_move(fs_node *n, fs_node *newdir, const char *newname);
int  fs_trash(fs_node *n);                   /* move into /Trash               */
int  fs_restore(fs_node *n);                 /* move into /home/user           */
void fs_list(fs_node *dir);                  /* console helper                 */

const char *fs_pwd(void);
size_t fs_count(void);

#endif
