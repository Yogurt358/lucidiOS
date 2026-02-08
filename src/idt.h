#pragma once
#include "common.h"

struct idt_entry {
   uint16_t offset1;        
   uint16_t selector;        
   uint8_t  ist:2;             
   uint8_t  type_attributes; 
   uint16_t offset_2;        
   uint32_t offset_3;        
   uint32_t reserved0;
}__attribute__((packed));

struct idtr{
    uint16_t size;
    uint64_t offset;
}__attribute__((packed));

typedef struct idt_entry idt_entry_t;
typedef struct idtr idtr_t;

static inline void lidt(idtr_t* ptr) {
    asm volatile("lidt"::"m"(*ptr):"memory");
}

static inline void sidt(idtr_t* ptr) {
    asm volatile("sidt %0":"=m"(*ptr)::"memory");
}

void init_idt();
void load_idt();