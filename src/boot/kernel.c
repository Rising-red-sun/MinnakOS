#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "string.h"
#include "console.h"
#include "keyboard.h"
#include "mem.h"
#include "serial.h"
#include "gfx.h"
#include "fs.h"
#include "storage.h"
#include "desktop.h"

#define REQ __attribute__((used, section(".limine_requests")))

static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }

__attribute__((used, section(".limine_requests_start_marker")))
static volatile uint64_t requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end_marker")))
static volatile uint64_t requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

REQ static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(0);

REQ static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0, .response = NULL
};

REQ static volatile struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID, .revision = 0, .response = NULL
};

REQ static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0, .response = NULL
};

REQ static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 1, .response = NULL
};

REQ static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0, .response = NULL
};

REQ static volatile struct limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID, .revision = 0, .response = NULL,
    .stack_size = 512 * 1024
};

uint64_t hhdm_offset = 0;
bool have_hhdm = false;
struct limine_memmap_response *memmap = NULL;
struct limine_framebuffer *framebuffer = NULL;
struct limine_bootloader_info_response *bootloader_info = NULL;

void out_char(char c) { console_putc(c); }

__attribute__((noreturn)) void halt(void) { for (;;) __asm__ volatile ("hlt"); }

void reboot(void) {
    __asm__ volatile ("cli");
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile ("hlt");
}

void kernel_main(void) {
    /* We poll the PS/2 controller and have no IDT yet: interrupts must stay
     * disabled, otherwise any IRQ1/IRQ12 would triple-fault the machine. */
    __asm__ volatile ("cli");
    outb(0x21, 0xFF);   /* mask master PIC */
    outb(0xA1, 0xFF);   /* mask slave PIC  */

    if (hhdm_request.response) {
        hhdm_offset = hhdm_request.response->offset;
        have_hhdm = true;
    }
    if (framebuffer_request.response && framebuffer_request.response->framebuffer_count > 0) {
        framebuffer = framebuffer_request.response->framebuffers[0];
    }
    if (memmap_request.response) {
        memmap = memmap_request.response;
    }
    if (bootloader_info_request.response) {
        bootloader_info = bootloader_info_request.response;
    }

    serial_init();
    mem_init(hhdm_offset, have_hhdm, memmap);
    console_init(framebuffer, hhdm_offset, have_hhdm);
    gfx_init(framebuffer, hhdm_offset, have_hhdm);
    keyboard_init();

    console_clear();
    console_print("DenemeOS v0.2\n");
    console_print("Bootloader: ");
    if (bootloader_info && bootloader_info->name) console_print(bootloader_info->name);
    console_print("\n");

    serial_print("denemeos: boot\n");

    fs_init();
    serial_print("denemeos: ramfs ready\n");

    int have_disk = storage_init();
    if (!have_disk) {
        console_print("Disk: yok (ISO/ram modu)\n");
        serial_print("denemeos: no disk, ram mode\n");
    } else {
        int r = storage_load();
        if (r > 0) {
            console_print("Disk: snapshot geri yuklendi\n");
            serial_print("denemeos: snapshot restored\n");
        } else if (r == 0) {
            console_print("Disk: bos, varsayilan dosya sistemi\n");
            serial_print("denemeos: disk empty, defaults\n");
        } else {
            console_print("Disk: bozuk snapshot, varsayilanlar\n");
            serial_print("denemeos: corrupt snapshot, reset\n");
            fs_reset();
            fs_defaults();
            storage_erase();
        }
    }

    console_print("Starting desktop...\n");
    serial_print("denemeos: desktop start\n");

    desktop_init();
    desktop_run();

    halt();
}

void _start(void) {
    kernel_main();
}
