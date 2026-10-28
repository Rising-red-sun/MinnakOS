#include "fs.h"
#include "console.h"
#include "mem.h"
#include "string.h"
#include <stddef.h>

static fs_node *root = NULL;
static fs_node *cwd = NULL;
static char pwd_buf[256];

/* ------------------------------------------------------------- internals */

static fs_node *node_new(const char *name, int is_dir)
{
    fs_node *n = kmalloc(sizeof(fs_node));
    if (!n) return NULL;
    memset(n, 0, sizeof(fs_node));
    strncpy(n->name, name, 63);
    n->name[63] = 0;
    n->is_dir = is_dir;

    if (is_dir) {
        n->child_cap = 8;
        n->children = kmalloc(n->child_cap * sizeof(fs_node *));
        if (n->children) memset(n->children, 0, n->child_cap * sizeof(fs_node *));
        else n->child_cap = 0;
    } else {
        n->cap = 4096;
        n->data = kmalloc(n->cap);
        if (n->data) n->data[0] = 0;
        else n->cap = 0;
    }
    return n;
}

fs_node *fs_child(fs_node *dir, const char *name)
{
    if (!dir || !dir->is_dir || !name) return NULL;
    for (size_t i = 0; i < dir->child_count; i++) {
        if (strcmp(dir->children[i]->name, name) == 0) return dir->children[i];
    }
    return NULL;
}

static int add_child(fs_node *dir, fs_node *child)
{
    if (!dir || !dir->is_dir || !child) return -1;
    if (dir->child_count == dir->child_cap) {
        size_t nc = dir->child_cap ? dir->child_cap * 2 : 8;
        fs_node **na = kmalloc(nc * sizeof(fs_node *));
        if (!na) return -1;
        if (dir->children)
            memcpy(na, dir->children, dir->child_count * sizeof(fs_node *));
        memset(na + dir->child_count, 0, (nc - dir->child_count) * sizeof(fs_node *));
        dir->children = na;
        dir->child_cap = nc;
    }
    child->parent = dir;
    dir->children[dir->child_count++] = child;
    return 0;
}

fs_node *fs_add(fs_node *parent, const char *name, int is_dir)
{
    if (!parent || !parent->is_dir || !name || !*name) return NULL;
    if (fs_child(parent, name)) return NULL;          /* no duplicate names */
    fs_node *n = node_new(name, is_dir);
    if (!n) return NULL;
    if (add_child(parent, n) != 0) return NULL;
    return n;
}

static fs_node *resolve(fs_node *base, const char *path)
{
    if (!path || !*path) return base;
    fs_node *cur = (*path == '/') ? root : base;
    if (!cur) cur = root;
    if (*path == '/') path++;

    char tmp[256];
    strncpy(tmp, path, 255);
    tmp[255] = 0;

    char *tok = strtok(tmp, "/");
    while (tok) {
        if (strcmp(tok, ".") == 0) { /* same dir */ }
        else if (strcmp(tok, "..") == 0) {
            if (cur->parent) cur = cur->parent;
        } else {
            fs_node *ch = fs_child(cur, tok);
            if (!ch) return NULL;
            cur = ch;
        }
        tok = strtok(NULL, "/");
    }
    return cur;
}

/* split "a/b/c" -> parent dir + leaf name (path is copied) */
static fs_node *split_parent(char *tmp, char **name)
{
    char *slash = strrchr(tmp, '/');
    if (!slash) { *name = tmp; return cwd; }

    *slash = 0;
    *name = slash + 1;
    if (!*name) return NULL;
    if (!*tmp) return root;
    return resolve(cwd, tmp);
}

static void unique_name(fs_node *dir, const char *base, char *out, size_t n)
{
    strncpy(out, base, n - 1);
    out[n - 1] = 0;
    if (!fs_child(dir, out)) return;

    char stem[52];
    strncpy(stem, base, 48);
    stem[48] = 0;
    for (int i = 1; i < 1000; i++) {
        char num[16];
        int k = 0, v = i;
        char t[16];
        int tn = 0;
        while (v) { t[tn++] = (char)('0' + v % 10); v /= 10; }
        while (tn) num[k++] = t[--tn];
        num[k] = 0;

        char cand[64];
        strncpy(cand, stem, 48);
        cand[48] = 0;
        strcat(cand, ".");
        strcat(cand, num);
        if (strlen(cand) < n && !fs_child(dir, cand)) {
            strcpy(out, cand);
            return;
        }
    }
}

/* ------------------------------------------------------------------- API */

void fs_reset(void)
{
    root = node_new("/", 1);
    cwd = root;
}

void fs_init(void)
{
    if (root) return;
    fs_reset();
    fs_defaults();
}

void fs_defaults(void)
{
    if (!root) fs_reset();
    if (!root) return;

    struct { const char *path; const char *text; } files[] = {
        { "/etc/version",  "DenemeOS 0.2 x86_64\nLimine bootloader\n" },
        { "/etc/motd",     "DenemeOS'a hos geldiniz!\nSag tik: menu, F10: diske kaydet.\n" },
        { "/etc/issue",    "DenemeOS 0.2\n" },
        { "/etc/hostname", "denemeos\n" },
        { "/home/user/welcome.txt",
          "Merhaba! Bu dosya sizin kullanici alaninizda.\n"
          "Dosya yoneticisinden acip duzenleyebilirsiniz.\n" },
        { "/home/user/notes.txt",
          "Notlar\n-----\n"
          "- Ctrl+S ile kaydet (Editor)\n"
          "- F10 tum sistemi diske yazar\n"
          "- Cop kutusu: silinen dosyalar burada tutulur\n" },
        { "/home/user/readme.txt",
          "DenemeOS hakkinda\n"
          "=================\n"
          "Masaustu kisayollari: Home, Terminal, Dosyalar, Editor,\n"
          "Hesap, Ayarlar, Cop, Kurulum, Hakkinda.\n"
          "F1-F8 uygulamalar, F9 baslat menusu, F10 diske kaydet.\n" },
        { "/home/user/Documents/notlar.txt",
          "Kisisel notlar\n"
          "--------------\n"
          "Bu dosya /home/user/Documents icinde.\n" },
        { "/home/user/Documents/merhaba.txt",
          "Merhaba dunya!\n" },
        { "/home/user/Desktop/kisayollar.txt",
          "Masaustu kisayollari uygulama pencerelerini acar.\n" },
        { "/home/user/Downloads/indirilenler.txt",
          "Indirilen dosyalar buraya konabilir.\n" },
        { "/usr/bin/readme.txt",
          "Bu dizin uygulamalari barindirir.\n" },
        { "/var/log/boot.log",
          "boot: ok\n" },
        { NULL, NULL }
    };

    fs_ensure_dir("/etc");
    fs_ensure_dir("/home/user");
    fs_ensure_dir("/home/user/Desktop");
    fs_ensure_dir("/home/user/Documents");
    fs_ensure_dir("/home/user/Downloads");
    fs_ensure_dir("/home/user/Pictures");
    fs_ensure_dir("/usr/bin");
    fs_ensure_dir("/usr/share");
    fs_ensure_dir("/var/log");
    fs_ensure_dir("/tmp");
    fs_ensure_dir("/Trash");

    for (int i = 0; files[i].path; i++) {
        char tmp[128];
        strncpy(tmp, files[i].path, 127);
        tmp[127] = 0;
        char *name;
        fs_node *parent = split_parent(tmp, &name);
        if (!parent) continue;
        fs_node *f = fs_child(parent, name);
        if (!f) f = fs_add(parent, name, 0);
        if (f) fs_write_file(f, files[i].text, strlen(files[i].text));
    }
}

fs_node *fs_root(void) { return root; }
fs_node *fs_cwd(void) { return cwd; }

void fs_set_cwd(fs_node *n)
{
    if (n && n->is_dir) cwd = n;
}

fs_node *fs_lookup(fs_node *base, const char *path)
{
    if (!base) base = cwd;
    if (!path || !*path) return base;
    return resolve(base, path);
}

fs_node *fs_ensure_dir(const char *abs_path)
{
    if (!abs_path) return NULL;
    fs_node *cur = (abs_path[0] == '/') ? root : cwd;
    const char *p = abs_path;
    while (*p == '/') p++;

    char part[64];
    while (*p) {
        int n = 0;
        while (*p && *p != '/' && n < 63) part[n++] = *p++;
        part[n] = 0;
        while (*p == '/') p++;
        if (!part[0]) break;

        fs_node *ch = fs_child(cur, part);
        if (!ch) ch = fs_add(cur, part, 1);
        if (!ch || !ch->is_dir) return NULL;
        cur = ch;
    }
    return cur;
}

int fs_mkdir(const char *path)
{
    if (!path || !*path) return -1;
    char tmp[256];
    strncpy(tmp, path, 255);
    tmp[255] = 0;
    char *name;
    fs_node *parent = split_parent(tmp, &name);
    if (!parent || !parent->is_dir || !*name) return -1;
    if (fs_child(parent, name)) return -1;
    return fs_add(parent, name, 1) ? 0 : -1;
}

int fs_mkdir_p(const char *path)
{
    return fs_ensure_dir(path) ? 0 : -1;
}

int fs_touch(const char *path)
{
    if (!path || !*path) return -1;
    char tmp[256];
    strncpy(tmp, path, 255);
    tmp[255] = 0;
    char *name;
    fs_node *parent = split_parent(tmp, &name);
    if (!parent || !parent->is_dir || !*name) return -1;
    fs_node *existing = fs_child(parent, name);
    if (existing) return existing->is_dir ? -1 : 0;
    return fs_add(parent, name, 0) ? 0 : -1;
}

int fs_unlink(fs_node *n)
{
    if (!n || !n->parent) return -1;
    fs_node *p = n->parent;
    for (size_t i = 0; i < p->child_count; i++) {
        if (p->children[i] == n) {
            for (size_t j = i; j + 1 < p->child_count; j++)
                p->children[j] = p->children[j + 1];
            p->child_count--;
            p->children[p->child_count] = NULL;
            n->parent = NULL;
            return 0;
        }
    }
    return -1;
}

static int is_ancestor(fs_node *maybe_ancestor, fs_node *n)
{
    for (fs_node *p = n; p; p = p->parent)
        if (p == maybe_ancestor) return 1;
    return 0;
}

/* Never remove (or move away) the directory tree the shell is sitting in:
 * cwd would become detached from the root and later saves would lose it. */
static int holds_cwd(fs_node *n)
{
    return n == cwd || is_ancestor(n, cwd);
}

int fs_delete(fs_node *n)
{
    if (!n || n == root || holds_cwd(n)) return -1;
    if (n->is_dir) {
        while (n->child_count > 0) {
            fs_node *c = n->children[0];
            if (fs_delete(c) != 0) return -1;
        }
    }
    return fs_unlink(n);
}

int fs_move(fs_node *n, fs_node *newdir, const char *newname)
{
    if (!n || !newdir || !newdir->is_dir || n == root) return -1;
    if (is_ancestor(n, newdir)) return -1;      /* cannot move dir into itself */

    const char *finalname = (newname && *newname) ? newname : n->name;
    if (fs_child(newdir, finalname)) return -1; /* collision: keep the source */

    fs_node *oldparent = n->parent;
    char oldname[64];
    strncpy(oldname, n->name, sizeof(oldname) - 1);
    oldname[sizeof(oldname) - 1] = 0;

    if (fs_unlink(n) != 0) return -1;

    if (newname && *newname) {
        strncpy(n->name, newname, 63);
        n->name[63] = 0;
    }
    if (add_child(newdir, n) != 0) {
        /* Should not happen after the collision check, but never lose a node. */
        strncpy(n->name, oldname, 63);
        n->name[63] = 0;
        add_child(oldparent, n);
        return -1;
    }
    return 0;
}

int fs_trash(fs_node *n)
{
    if (!n || n == root || holds_cwd(n)) return -1;
    fs_node *trash = fs_ensure_dir("/Trash");
    if (!trash) return -1;
    if (n->parent == trash) return 0;

    if (fs_child(trash, n->name)) {
        char uniq[64];
        unique_name(trash, n->name, uniq, sizeof(uniq));
        return fs_move(n, trash, uniq);
    }
    return fs_move(n, trash, NULL);
}

int fs_restore(fs_node *n)
{
    if (!n || n == root) return -1;
    fs_node *home = fs_ensure_dir("/home/user");
    if (!home) return -1;

    if (fs_child(home, n->name)) {
        char uniq[64];
        unique_name(home, n->name, uniq, sizeof(uniq));
        return fs_move(n, home, uniq);
    }
    return fs_move(n, home, NULL);
}

int fs_rm(const char *path)
{
    if (!path || !*path) return -1;
    fs_node *node = resolve(cwd, path);
    if (!node || node == root || holds_cwd(node)) return -1;
    if (node->is_dir && node->child_count > 0) return -1;
    return fs_unlink(node);
}

int fs_write_file(fs_node *n, const char *data, size_t len)
{
    if (!n || n->is_dir) return -1;
    if (len + 1 > n->cap) {
        size_t nc = n->cap ? n->cap : 4096;
        while (nc < len + 1) nc *= 2;
        char *nd = kmalloc(nc);
        if (!nd) return -1;
        n->data = nd;
        n->cap = nc;
    }
    if (!n->data) return -1;
    memcpy(n->data, data, len);
    n->data[len] = 0;
    n->size = len;
    return 0;
}

char *fs_read_file(fs_node *n)
{
    if (!n || n->is_dir) return NULL;
    return n->data;
}

void fs_list(fs_node *dir)
{
    if (!dir || !dir->is_dir) { console_print("Not a directory\n"); return; }
    for (size_t i = 0; i < dir->child_count; i++) {
        fs_node *c = dir->children[i];
        console_print(c->name);
        console_print(c->is_dir ? "/  " : "  ");
    }
    console_print("\n");
}

const char *fs_pwd(void)
{
    fs_node *chain[64];
    int depth = 0;
    fs_node *cur = cwd;

    if (!cur || cur == root) { strcpy(pwd_buf, "/"); return pwd_buf; }

    while (cur && cur != root && depth < 63) {
        chain[depth++] = cur;
        cur = cur->parent;
    }

    size_t used = 0;
    pwd_buf[0] = 0;
    for (int i = depth - 1; i >= 0; i--) {
        const char *nm = chain[i]->name;
        if (used + 1 < sizeof(pwd_buf)) pwd_buf[used++] = '/';
        while (*nm && used + 1 < sizeof(pwd_buf)) pwd_buf[used++] = *nm++;
    }
    pwd_buf[used] = 0;
    if (depth == 0) strcpy(pwd_buf, "/");
    return pwd_buf;
}

static size_t count_rec(fs_node *n)
{
    size_t c = 1;
    if (n && n->is_dir)
        for (size_t i = 0; i < n->child_count; i++)
            c += count_rec(n->children[i]);
    return c;
}

size_t fs_count(void) { return count_rec(root); }
