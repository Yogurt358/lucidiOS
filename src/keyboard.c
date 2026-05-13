#include "keyboard.h"

static volatile char     buf[KBD_BUF_SIZE];
static volatile uint32_t head;   // ISR writes here
static volatile uint32_t tail;   // shell reads here 

static bool shift_down;
static bool capslock;
static bool extended; // set after 0xE0, cleared after next byte 

// layout was assisted by AI JUST TO MAKE IT LOOK APPEALING!!! NOT TO MAKE THE CODE ITSELF!
static const char sc_normal[128] = {
    [0x01] = 27,                                                            // Esc        
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b',                              // Backspace  
    [0x0F] = '\t',                                                          // Tab        
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',                                                          // Enter      
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x27] = ';', [0x28] = '\'',
    [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*',                                                           // KP *       
    [0x39] = ' ',                                                           // Space      
};

static const char sc_shift[128] = {
    [0x01] = 27,
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x0E] = '\b',
    [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L',
    [0x27] = ':', [0x28] = '"',
    [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M',
    [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*',
    [0x39] = ' ',
};

void keyboard_init(void) {
    head = tail = 0;
    shift_down = capslock = extended = false;
}

static void buf_push(char c) {
    uint32_t next = (head + 1) & (KBD_BUF_SIZE - 1);
    if (next == tail) return;   // full drop the new byte 
    buf[head] = c;
    head = next;
}

int keyboard_pop(void) {
    if (head == tail) return -1;
    char c = buf[tail];
    tail = (tail + 1) & (KBD_BUF_SIZE - 1);
    return (unsigned char)c;
}

void keyboard_handle_scancode(uint8_t sc) {
    // extended (characters that are 2 bytes instead of 1)
    if (sc == 0xE0) { extended = true; return; }

    bool    released = (sc & 0x80) != 0;
    uint8_t code     = sc & 0x7F;

    if (extended) {
        extended = false;
        return;
    }

    // shift
    if (code == 0x2A || code == 0x36) {
        shift_down = !released;
        return;
    }

    // CAPS LOCK
    if (code == 0x3A) {
        if (!released) capslock = !capslock;
        return;
    }

    if (released) return;

    char base = sc_normal[code];
    if (base == 0) return;

    // caps lock and shift handler for alphabetic letters
    bool is_letter = (base >= 'a' && base <= 'z');
    bool use_shift = is_letter ? (shift_down ^ capslock) : shift_down;
    char ch        = use_shift ? sc_shift[code] : base;
    if (ch == 0) return;

    buf_push(ch);
}