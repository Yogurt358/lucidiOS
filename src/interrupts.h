#pragma once
#include "common.h"

struct idt_entry {
   uint16_t offset1;
   uint16_t selector;
   uint8_t  ist;      
   uint8_t  type_attributes;
   uint16_t offset2;        
   uint32_t offset3;        
   uint32_t reserved0;
}__attribute__((packed));

struct stack_frame {
uint64_t r15, r14, r13, r12, r11, r10, r9, r8,
         rbp, rdi, rsi, rdx, rcx, rbx, rax,
         vector, err_code,
         rip, cs, rflags, rsp, ss; // CPU pushes automatically when interrupt
}__attribute__((packed));

struct idtr{
    uint16_t size;
    uint64_t offset;
}__attribute__((packed));

typedef struct idt_entry idt_entry_t;
typedef struct idtr idtr_t;
typedef struct stack_frame stack_frame_t;

static inline void lidt(idtr_t* ptr) {
    asm volatile("lidt %0"::"m"(*ptr):"memory");
}

static inline uint16_t get_cs(void) {
    uint16_t cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

extern void isr_handler_C(stack_frame_t *frame);
void set_gate(size_t n, uint8_t flags, uint64_t isr_address, uint8_t _ist, uint16_t css);
void init_interrupts();