#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#define IA32_APIC_BASE_MSR 0x1B
#define COM1 0x3F8 // I/O port of Serial

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1":: "a"(value), "Nd"(port):);
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0": "=a"(ret): "Nd"(port):);
    return ret;
}

static inline void ud2(void) {
    asm volatile("ud2":::);
}

static inline void ud0() {
    asm volatile("ud0":::);
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

/*
static inline bool is_LAPIC(void) {
    uint32_t edx;
    asm volatile(
        "cpuid\n\t"
        :"=d"(edx)
        :"a"(1)
        :"ebx", "ecx"
    );
    return ((edx>>9)&0b1);
}

static inline bool check_ARAT(void) {
    uint32_t eax; 
    asm volatile(
        "cpuid\n\t"
        :"=a"(eax)
        :"a"(6)
        :"ebx", "ecx", "edx"
    );
    return ((eax>>2)&0b1);
}

static inline uint32_t check_CPUID(void) {
    uint32_t eax; 
    asm volatile(
        "cpuid\n\t"
        :"=a"(eax)
        :"a"(0)
        :"ebx", "ecx", "edx"
    );
    return eax;
}

static inline uint8_t CPUID_shish(void) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile(
        "cpuid\n\t"
        :"=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :"a"(1)
    );
    return (uint8_t)(ebx>>24);
}

static inline uint32_t get_max_extended_leaf(void) {
    uint32_t eax;
    asm volatile("cpuid" : "=a"(eax) : "a"(0x80000000) : "ebx", "ecx", "edx");
    return eax;
}

static inline uint8_t what(void) {
    uint32_t eax;
    asm volatile(
        "cpuid\n\t"
        :"=a"(eax)
        :"a"(0x80000008)
        :"ecx", "ebx", "edx"
    );
    return (uint8_t)(eax);
}

*/

static inline uint64_t get_lapic_base(void) {
    uint32_t eax, edx;
    asm volatile(
        "rdmsr"
        : "=a"(eax), "=d"(edx)
        : "c"(IA32_APIC_BASE_MSR)
    );

    uint64_t msr_full = ((uint64_t)edx << 32) | eax;

    uint64_t mask = 0x000000FFFFFFF000; 

    return msr_full & mask;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

int is_transmit_empty();
void write_serial(char a);
void init_serial();
void write_better(char* a);