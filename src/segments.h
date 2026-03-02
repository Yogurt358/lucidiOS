#pragma once
#include "common.h"

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

struct tss_descriptor { // 16 bytes
    uint16_t limit1;
    uint16_t base1;
    uint8_t base2;
    uint8_t access_byte;
    uint8_t limit2:4; // is only 4 bit
    uint8_t flags:4; // is only 4 bits
    uint8_t base3;
    uint32_t base4;
    uint32_t reserved0;
} __attribute__((packed));

struct gdt_descriptor { // 8 bytes
    uint16_t limit1;
    uint16_t base1;
    uint8_t base2;
    uint8_t access_byte;
    uint8_t limit2:4;
    uint8_t flags:4;
    uint8_t base3;
} __attribute__((packed));



struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));


typedef struct gdtr gdtr_t;
typedef struct tss_entry tss_entry_t;
typedef struct gdt_descriptor gdt_descriptor_t;
typedef struct tss_descriptor tss_descriptor_t;

struct the_gdt {
    gdt_descriptor_t null;      // Entry 0
    gdt_descriptor_t kernel_cs; // Entry 1
    gdt_descriptor_t user_cs;   // Entry 2
    gdt_descriptor_t kernel_ds; // Entry 3
    gdt_descriptor_t user_ds;   // Entry 4
    tss_descriptor_t tss;       // Entry 5 & 6 (16 bytes)
} __attribute__((packed));
typedef struct the_gdt the_gdt_t;


static inline void ltr() {
    asm volatile ("ltr %w0"::"r"((uint16_t)0x28):);
}

static inline void lgdt(gdtr_t* ptr) {
    asm volatile ("lgdt %0"::"m"(*ptr): "memory");
}

static inline void reload_segments() {
    asm volatile (
        // 1. Reload CS using a far return
        "pushq $0x08\n\t"          // Push the new Kernel CS selector
        "leaq 1f(%%rip), %%rax\n\t"// Load the address of the label '1' below
        "pushq %%rax\n\t"          // Push the return address
        "lretq\n\t"                // Far return: pops address into RIP, and selector into CS

        // 2. Reload Data Segments
        "1:\n\t"                   // lretq
        "mov $0x18, %%ax\n\t"      // load new Kernel DS
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        ::: "rax", "memory"
    );
}

void set_segment(size_t n, uint8_t access, uint8_t flag);
void set_TSS(uint8_t access, uint8_t flag);
void init_gdt();

