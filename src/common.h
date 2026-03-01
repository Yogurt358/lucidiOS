#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#define COM1 0x3F8 // I/O port of Serial

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1":: "a"(value), "Nd"(port):);
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0": "=a"(ret): "Nd"(port):);
    return ret;
}

static inline void ud2() {
    asm volatile("ud2":::);
}

static inline uint32_t get_cs(void) {
    uint32_t cs;
    asm volatile("mov %%cs, %0" : "=r"(cs)::);
    return cs;
}
static inline uint32_t get_ds(void) {
    uint32_t ds;
    asm volatile("mov %%ds, %0" : "=r"(ds)::);
    return ds;
}

static inline uint32_t str(void) {
    uint32_t tr;
    asm volatile("str %0" : "=r"(tr)::);
    return tr;
}



void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

int is_transmit_empty();
void write_serial(char a);
void init_serial();
void write_better(char* a);