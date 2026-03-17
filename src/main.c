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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
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

    
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    uint64_t half_high = hhdm_request.response->offset; 
    struct limine_memmap_entry **map = memmap_request.response->entries;
    

    init_pmm(memmap_request.response);
    map_page(half_high + 0xFEE00000, 0xFEE00000, 0x13, half_high);

    init_APIC(half_high);
    asm volatile("sti");

    draw_sentence(framebuffer, "Check 1");

    /*
    volatile int a = 3;
    volatile int b = 0;
    volatile int c = a/b; //checking that #DE works
    

    //ud2(); //checking that #UD works
    
    if(base_MSR()){
        write_better("good\n");
    }
    else {
        write_better("no good\n");
    }
    
    uint32_t max_leaf = get_max_extended_leaf();
    if (max_leaf >= 0x80000008) {
        write_better("can support");
    }
    
    //what(); // after calculation, APIC is from 12 to 39

    uint64_t apic_phys = get_lapic_base();
    if (apic_phys == 0xFEE00000) {
        write_better("LAPIC is at the standard address!\n");
    }
    */

    reset(framebuffer);
    draw_sentence(framebuffer, "Check 2");

    hcf();
}
