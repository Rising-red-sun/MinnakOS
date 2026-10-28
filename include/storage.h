#ifndef STORAGE_H
#define STORAGE_H

/* Persistent filesystem snapshot kept in the unused tail of the virtual
 * disk.  The FAT32 partition ends at LBA 131071. */
#define STORAGE_LBA   131072u
#define STORAGE_MAX   4096          /* sectors (2 MiB) */

int storage_init(void);
int storage_present(void);
int storage_loaded(void);           /* boot: 1 if a snapshot was restored */
int storage_load(void);             /* 1 restored, 0 empty, -1 corrupt     */
int storage_save(void);             /* 0 ok, <0 error                      */
int storage_erase(void);
int storage_bytes(void);            /* size of the last snapshot           */

#endif
