#pragma once
#include "common.h"
#include "limine.h"

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;
} __attribute__((packed));

struct tss_descriptor { 
    uint16_t limit1;
    uint16_t base1;
    uint8_t base2;
    uint8_t access_byte;
    uint8_t limit2:4;
    uint8_t flags:4;
    uint8_t base3;
    uint32_t base4;
    uint32_t reserved0;
} __attribute__((packed));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));


typedef struct gdtr gdtr_t;
typedef struct tss_entry tss_entry_t;
typedef struct tss_descriptor tss_descriptor_t;

static inline void sgdt(gdtr_t* ptr) {
    asm volatile ("sgdt %0" :"=m"(*ptr)::"memory");
}

static inline void ltr() {
    asm volatile ("ltr %w0"::"r"((uint16_t)0x28):);
}

static inline void lgdt(gdtr_t* ptr) {
    asm volatile ("lgdt %0"::"m"(*ptr): "memory");
}

void init_tss();
void load_tss();