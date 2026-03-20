#include "fb.h"
#include "interrupts.h"
#include "apic.h"
#include "segments.h"
#include "paging.h"



// Set the base revision to 4, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))                                  // framebuffer
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};
__attribute__((used, section(".limine_requests")))                                 // HHDM offset
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};
__attribute__((used, section(".limine_requests")))                                 // memory map
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};
__attribute__((used, section(".limine_requests")))                                 // rsdp
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.



// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

uint64_t g_hhdm_offset = 0;
uint64_t *rsdp_pointer = 0;
uint64_t ioapic_base = 0;

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {

    init_serial();
    init_gdt();
    init_interrupts();


    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
    
        write_better("framebuffer ain't valid");
        hcf();
    }

    if (hhdm_request.response == NULL) {
        write_better("bruh HHDM request ain't working\n");
        hcf();
    }

    if (memmap_request.response == NULL
    || memmap_request.response->entry_count == 0
    || memmap_request.response->entries == NULL) {
        write_better("memmap isn't valid");
        hcf();
    }

    if (rsdp_request.response == NULL
    || rsdp_request.response->address == NULL) {
        write_better("\nrsdp ain't working\n");
        hcf();
    }

    
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    g_hhdm_offset = hhdm_request.response->offset; 
    rsdp_pointer = rsdp_request.response->address; 

    madt_parsing();

    init_pmm(memmap_request.response);
    map_page(g_hhdm_offset + 0xFEE00000, 0xFEE00000, 0x13, g_hhdm_offset);
    map_page(ioapic_base, ioapic_base - g_hhdm_offset, 0x13, g_hhdm_offset);

    init_LAPIC();
    init_IOAPIC();
    while (inb(0x64) & 1) {
        inb(0x60);
    }
    asm volatile("sti");

    draw_sentence(framebuffer, "Check 1");

    reset(framebuffer);
    draw_sentence(framebuffer, "Check 2");
    asm volatile("int $0x32");
    hcf();
}
