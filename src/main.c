#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h" 
#include "fb.h"
#include "gdt_handle.h"
#include "common.h"

#define COM1 0x3F8 // I/O port of Serial

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

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1":: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0": "=a"(ret): "Nd"(port));
    return ret;
}

int is_transmit_empty() {
   return inb(COM1 + 5) & 0x20;
}

void write_serial(char a) {
   while (is_transmit_empty() == 0);

   outb(COM1,a);
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {



    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
       hcf();
    }

    

    
    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    //volatile uint32_t *fb_ptr = framebuffer->address;
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    /*
    // i = height 
    // j = width
    // drawing a rectangle
    for (size_t i = 0; i < height; i++) {
        for  (size_t j = 0; j < width; j++) {
            fb_ptr[i* (framebuffer->pitch / 4) + j] = 0x800080;
        }
    }
    */

    char* test_passage = 
"\tVideo Graphics Array (VGA) is a video display controller and accompanying graphics standard, "
"first introduced by IBM with the PS/2 line of computers in 1987. \nDue to its widespread adoption, "
"the term has also come to mean either an analog computer display standard, the 15-pin D-subminiature "
"VGA connector, or the 640x480 resolution itself.\n\n"
"Technical Specifications:\n"
"\t* 256 KB Video RAM (On-board)\n"
"\t* 16-color and 256-color modes\n"
"\t* 262,144-color palette (6 bits each for red, green, and blue)\n"
"\t* Selectable 25.175 MHz or 28.322 MHz master clock\n\n"
"Standard Graphics Modes:\n"
"\t640x480 in 16 colors\n"
"\t320x200 in 256 colors (Mode 13h)\n"
"\tText mode: 80x25 or 40x25 characters\n\n"
"Testing Carriage Return (\r): \n"
"Loading System... [          ]\r"
"Loading System... [##########]\n\n"
"The VGA standard was officially succeeded by the IBM XGA standard, but it remained "
"the most common interface for PC graphics for over a decade. \tEven today, VGA is used "
"as a fallback 'safe mode' for modern operating systems when high-resolution drivers "
"are unavailable. \n\t\t--- End of Test ---";
    draw_sentence(framebuffer, test_passage);

    // init Serial input
    outb(COM1+1, 0x00); // disable interrupts
    outb(COM1+3, 0x80); //setup Baud rate (115200 / 12 = 9600)
    outb(COM1, 0x0C);
    outb(COM1+1, 0x00); 
    outb(COM1+3, 0x03); // set up line Protocol
    outb(COM1+2, 0xC7); // setup FIFO (unimportant for now)
    outb(COM1+4, 0x0B); // setup modem (unimportant for now)
    char *msg = "\n\nDaisy, Daisy, give me your answer do.\n\n";
    for (size_t i = 0; msg[i] != '\0'; i++) {write_serial(msg[i]);}
    
    gdtr_t testing_ptr;
    sgdt(&testing_ptr);
    init_tss();

    hcf();
}