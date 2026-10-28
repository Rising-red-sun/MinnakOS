#include "ata.h"
#include <stdint.h>

/* Primary IDE channel (PIIX4). The VM attaches the disk here. */
#define P_DATA   0x1F0
#define P_ERROR  0x1F1
#define P_SECCNT 0x1F2
#define P_LBA0   0x1F3
#define P_LBA1   0x1F4
#define P_LBA2   0x1F5
#define P_DRIVE  0x1F6
#define P_STATUS 0x1F7
#define P_CMD    0x1F7
#define P_CTRL   0x3F6

#define ST_ERR 0x01
#define ST_DRQ 0x08
#define ST_DF  0x20
#define ST_BSY 0x80

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile ("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile ("inb %1,%0" : "=a"(r) : "Nd"(p)); return r; }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile ("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint16_t inw(uint16_t p) { uint16_t r; __asm__ volatile ("inw %1,%0" : "=a"(r) : "Nd"(p)); return r; }

static int present = 0;
static uint32_t total_sectors = 0;

static void delay400(void)
{
    for (int i = 0; i < 4; i++) (void)inb(P_CTRL);
}

static int wait_not_busy(void)
{
    for (int i = 0; i < 2000000; i++) {
        uint8_t st = inb(P_STATUS);
        if (st == 0xFF) return 0;
        if (!(st & ST_BSY)) return 1;
    }
    return 0;
}

static int wait_drq(void)
{
    for (int i = 0; i < 2000000; i++) {
        uint8_t st = inb(P_STATUS);
        if (st == 0xFF) return 0;
        if (st & ST_ERR) return 0;
        if (st & ST_DF) return 0;
        if (!(st & ST_BSY) && (st & ST_DRQ)) return 1;
    }
    return 0;
}

int ata_init(void)
{
    present = 0;
    total_sectors = 0;

    outb(P_CTRL, 0);                 /* nIEN off; we poll anyway */
    outb(P_DRIVE, 0xA0);
    delay400();

    outb(P_SECCNT, 0);
    outb(P_LBA0, 0);
    outb(P_LBA1, 0);
    outb(P_LBA2, 0);
    outb(P_CMD, 0xEC);               /* IDENTIFY */
    delay400();

    uint8_t st = inb(P_STATUS);
    if (st == 0 || st == 0xFF) return 0;
    if (!wait_not_busy()) return 0;
    if (inb(P_LBA1) || inb(P_LBA2)) return 0;    /* ATAPI, not plain ATA */
    if (!wait_drq()) return 0;

    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(P_DATA);

    total_sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    present = 1;
    return 1;
}

int ata_present(void) { return present; }
uint32_t ata_sectors(void) { return total_sectors; }

static int setup(uint32_t lba, int count, uint8_t cmd)
{
    if (!present) return 0;
    if (lba > 0x0FFFFFFF || count <= 0 || count > 256) return 0;
    if (total_sectors && lba + (uint32_t)count > total_sectors) return 0;

    if (!wait_not_busy()) return 0;

    outb(P_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    delay400();
    outb(P_SECCNT, (uint8_t)(count == 256 ? 0 : count));
    outb(P_LBA0, (uint8_t)(lba & 0xFF));
    outb(P_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(P_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(P_CMD, cmd);
    delay400();

    return wait_drq();
}

int ata_read(uint32_t lba, int count, void *buf)
{
    uint16_t *p = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (!setup(lba + (uint32_t)s, 1, 0x20)) return -1;
        for (int i = 0; i < 256; i++) p[i] = inw(P_DATA);
        p += 256;
    }
    return 0;
}

int ata_write(uint32_t lba, int count, const void *buf)
{
    const uint16_t *p = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (!setup(lba + (uint32_t)s, 1, 0x30)) return -1;
        for (int i = 0; i < 256; i++) outw(P_DATA, p[i]);
        p += 256;
        if (!wait_not_busy()) return -1;
    }
    return 0;
}

int ata_flush(void)
{
    if (!present) return -1;
    if (!wait_not_busy()) return -1;
    outb(P_CMD, 0xE7);               /* FLUSH CACHE */
    delay400();
    return wait_not_busy() ? 0 : -1;
}
