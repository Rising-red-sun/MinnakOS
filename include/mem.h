#ifndef MEM_H
#define MEM_H
#include <stddef.h>
#include <stdint.h>
#include "limine.h"

void mem_init(uint64_t hhdm_offset, int have_hhdm, struct limine_memmap_response *memmap);
void *kmalloc(size_t size);
void kfree(void *p);              /* bump allocator: no-op            */
size_t mem_used(void);            /* bytes handed out so far          */
size_t mem_total(void);           /* heap size                        */
#endif
