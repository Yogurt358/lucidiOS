#pragma once
#include <stdint.h>
#include <stddef.h>
#include "limine.h"

struct tss_entry {
    uint32_t _reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t _reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t _reserved2;
    uint16_t _reserved3;
    uint16_t iopb;
} __attribute__((packed));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

typedef struct gdtr gdtr_t;
typedef struct tss_entry tss_entry_t;

static inline void sgdt(gdtr_t* ptr) {
    asm volatile ("sgdt %0" :"=m"(*ptr)::"memory");
}
void init_tss();
void load_tss();
void switch_save();

