#include "storage.h"
#include "ata.h"
#include "fs.h"
#include "string.h"
#include <stddef.h>

#define SB_MAGIC "DNSAVE02"
#define MAX_BLOB (STORAGE_MAX * 512)
#define MAX_NODES 2048

static uint8_t blob[MAX_BLOB] __attribute__((aligned(16)));
static uint8_t sb[512] __attribute__((aligned(16)));

static int have_disk = 0;
static int was_loaded = 0;
static int last_size = 0;

/* ------------------------------------------------------------------ util */

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t crc32(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (~(c & 1u) + 1u));
    }
    return ~c;
}

/* ------------------------------------------------------------ serialize */

static size_t put_bytes(size_t off, const void *src, size_t n)
{
    if (n > MAX_BLOB || off > MAX_BLOB - n) return 0;
    memcpy(blob + off, src, n);
    return off + n;
}

/* Recursive writer with explicit parent index (DFS order = index order). */
static uint32_t dfs_index = 0;

static size_t write_node(fs_node *n, uint32_t parent, size_t off)
{
    uint32_t idx;
    size_t start = off;

    /* reserve header, fill parent after we know our index */
    if (off + 16 > MAX_BLOB) return 0;
    if (dfs_index >= MAX_NODES) return 0;
    idx = dfs_index++;
    off += 16;

    size_t namelen = strlen(n->name);
    off = put_bytes(off, n->name, namelen);
    if (!off) return 0;

    size_t size = n->is_dir ? 0 : n->size;
    if (!n->is_dir && size) {
        off = put_bytes(off, n->data, size);
        if (!off) return 0;
    }

    uint8_t *hdr = blob + start;
    put32(hdr + 0, (uint32_t)(n->is_dir ? 1 : 0));
    put32(hdr + 4, parent);
    put32(hdr + 8, (uint32_t)size);
    put32(hdr + 12, (uint32_t)namelen);

    if (n->is_dir) {
        for (size_t i = 0; i < n->child_count; i++) {
            off = write_node(n->children[i], idx, off);
            if (!off) return 0;
        }
    }
    return off;
}

/* ---------------------------------------------------------- deserialize */

static fs_node *parsed[MAX_NODES];
static int parsed_count = 0;

static int parse_blob(size_t size)
{
    fs_node *root = NULL;
    size_t off = 0;
    parsed_count = 0;

    while (off + 16 <= size) {
        const uint8_t *hdr = blob + off;
        uint32_t type = get32(hdr + 0);
        uint32_t parent = get32(hdr + 4);
        uint32_t dsize = get32(hdr + 8);
        uint32_t namelen = get32(hdr + 12);
        off += 16;

        if (namelen == 0 || namelen > 63) return -1;
        if (off + namelen > size) return -1;

        char name[64];
        memcpy(name, blob + off, namelen);
        name[namelen] = 0;
        off += namelen;

        if (off + dsize > size) return -1;
        if (dsize && type) return -1;                 /* dirs have no data */

        if (parent == 0xFFFFFFFFu) {
            if (parsed_count != 0) return -1;         /* only one root */
            fs_reset();
            root = fs_root();
            if (!root) return -1;
            strncpy(root->name, name, 63);
            root->name[63] = 0;
            parsed[parsed_count++] = root;
        } else {
            if (!root) return -1;
            if (parsed_count >= MAX_NODES || parent >= (uint32_t)parsed_count) return -1;
            fs_node *p = parsed[parent];
            fs_node *n = fs_add(p, name, type ? 1 : 0);
            if (!n) return -1;
            if (!type && dsize) {
                if (fs_write_file(n, (const char *)(blob + off), dsize) != 0) return -1;
            }
            parsed[parsed_count++] = n;
        }
        off += dsize;
    }

    if (!root || !root->is_dir) return -1;
    fs_set_cwd(root);
    return 0;
}

/* ------------------------------------------------------------- superblock */

static int read_sb(void)
{
    if (ata_read(STORAGE_LBA, 1, sb) != 0) return -1;
    if (memcmp(sb, SB_MAGIC, 8) != 0) return 0;
    if (get32(sb + 8) != 2) return 0;
    return 1;
}

static void build_sb(uint32_t size, uint32_t sectors)
{
    memset(sb, 0, sizeof(sb));
    memcpy(sb, SB_MAGIC, 8);
    put32(sb + 8, 2);
    put32(sb + 12, size);
    put32(sb + 16, sectors);
    put32(sb + 20, crc32(blob, size));
}

/* ------------------------------------------------------------------- API */

int storage_init(void)
{
    have_disk = ata_init();
    was_loaded = 0;
    last_size = 0;
    return have_disk;
}

int storage_present(void) { return have_disk; }
int storage_loaded(void) { return was_loaded; }
int storage_bytes(void) { return last_size; }

int storage_load(void)
{
    was_loaded = 0;
    last_size = 0;
    if (!have_disk) return 0;

    int r = read_sb();
    if (r <= 0) return r;

    uint32_t size = get32(sb + 12);
    uint32_t sectors = get32(sb + 16);
    uint32_t crc = get32(sb + 20);

    if (size < 16 || size > MAX_BLOB) return -1;
    if (sectors != (size + 511) / 512 || sectors > STORAGE_MAX) return -1;

    for (uint32_t s = 0; s < sectors; s += 64) {
        uint32_t n = sectors - s;
        if (n > 64) n = 64;
        if (ata_read(STORAGE_LBA + 1 + s, (int)n, blob + (size_t)s * 512) != 0) return -1;
    }

    if (crc32(blob, size) != crc) return -1;
    if (parse_blob(size) != 0) return -1;

    was_loaded = 1;
    last_size = (int)size;
    return 1;
}

int storage_save(void)
{
    if (!have_disk) return -2;

    dfs_index = 0;
    size_t n = write_node(fs_root(), 0xFFFFFFFFu, 0);
    if (!n) return -1;
    if (n > MAX_BLOB) return -1;

    uint32_t sectors = (uint32_t)((n + 511) / 512);
    if (sectors > STORAGE_MAX) return -1;

    memset(blob + n, 0, (size_t)sectors * 512 - n);

    for (uint32_t s = 0; s < sectors; s += 64) {
        uint32_t cnt = sectors - s;
        if (cnt > 64) cnt = 64;
        if (ata_write(STORAGE_LBA + 1 + s, (int)cnt, blob + (size_t)s * 512) != 0) return -1;
    }

    build_sb((uint32_t)n, sectors);
    if (ata_write(STORAGE_LBA, 1, sb) != 0) return -1;
    if (ata_flush() != 0) return -1;

    last_size = (int)n;
    was_loaded = 1;          /* a snapshot now exists on disk */
    return 0;
}

int storage_erase(void)
{
    if (!have_disk) return -2;
    memset(sb, 0, sizeof(sb));
    if (ata_write(STORAGE_LBA, 1, sb) != 0) return -1;
    if (ata_flush() != 0) return -1;
    last_size = 0;
    was_loaded = 0;
    return 0;
}
