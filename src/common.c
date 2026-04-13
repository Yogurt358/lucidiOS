#include "common.h"

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

int is_transmit_empty() {
   return inb(COM1 + 5) & 0x20;
}

void write_serial(char a) {
   while (is_transmit_empty() == 0);

   outb(COM1,a);
}

void write_better(char* a) {
    for (size_t i = 0; a[i] != '\0'; i++) {
        write_serial(a[i]);
    }
}

void init_serial() {
// init Serial input
    outb(COM1+1, 0x00); // disable interrupts
    outb(COM1+3, 0x80); //setup Baud rate (115200 / 12 = 9600)
    outb(COM1, 0x0C);
    outb(COM1+1, 0x00); 
    outb(COM1+3, 0x03); // set up line Protocol
    outb(COM1+2, 0xC7); // setup FIFO (unimportant for now)
    outb(COM1+4, 0x0B); // setup modem (unimportant for now)
    
    char *msg = "\n\nDaisy, Daisy, give me your answer do.\n\n";
    for (size_t i = 0; msg[i] != '\0'; i++) {
        write_serial(msg[i]);
    }

}

void print_hex_serial(uint64_t n) {
    char* hex_chars = "0123456789ABCDEF";
    
    // Optional: print the '0x' prefix
    write_serial('0');
    write_serial('x');

    // We loop 16 times (64 bits / 4 bits per hex digit)
    // We go from most significant to least significant
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (n >> i) & 0xF;
        write_serial(hex_chars[nibble]);
    }
}

void print_int_serial(int64_t n) {
    if (n == 0) {
        write_serial('0');
        return;
    }

    if (n < 0) {
        write_serial('-');
        n = -n;
    }

    char buffer[32]; // Long enough for any 64-bit integer
    int i = 0;

    // Extract digits by dividing by 10
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }

    // Print the buffer backward
    while (i > 0) {
        write_serial(buffer[--i]);
    }
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt); // 'fmt' is the last "known" argument

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++; // Move to the specifier (d, s, x, etc.)
            
            switch (fmt[i]) {
                case 's': {
                    char* s = va_arg(args, char*);
                    if (!s) s = "(null)";
                    write_better(s);
                    break;
                }
                case 'd': {
                    int d = va_arg(args, int);
                    print_int_serial(d); 
                    break;
                }
                case 'c': {
                    char c = va_arg(args, char);
                    write_serial(c);
                    break;
                }
                case 'x': {
                    uint64_t x = va_arg(args, uint64_t);
                    print_hex_serial(x);
                    break;
                }
                case '%': {
                    write_serial('%'); // Handle "%%" literal
                    break;
                }
                default:
                    // If it's an unknown specifier, just print it
                    write_serial('%');
                    write_serial(fmt[i]);
                    break;
            }
        } else {
            write_serial(fmt[i]);
        }
    }

    va_end(args);
}


