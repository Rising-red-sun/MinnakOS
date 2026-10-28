#include "mem.h"
#include <stddef.h>

/* Static heap: lives in .bss, so it is zero-filled by the loader and does
 * not depend on any allocator/metadata from the bootloader. */
#define HEAP_SIZE (8u * 1024u * 1024u)

static unsigned char heap[HEAP_SIZE] __attribute__((aligned(16)));
static size_t heap_used = 0;

void mem_init(uint64_t hhdm_offset, int have_hhdm, struct limine_memmap_response *memmap)
{
    (void)hhdm_offset; (void)have_hhdm; (void)memmap;
    heap_used = 0;
}

void *kmalloc(size_t size)
{
    if (size == 0) size = 1;
    size = (size + 15) & ~(size_t)15;
    if (heap_used + size > HEAP_SIZE) return NULL;
    void *p = &heap[heap_used];
    heap_used += size;
    return p;
}

void kfree(void *p) { (void)p; }

size_t mem_used(void) { return heap_used; }
size_t mem_total(void) { return HEAP_SIZE; }
