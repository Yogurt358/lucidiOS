#pragma once
#include "common.h"

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 
#define IA32_APIC_BASE_MSR_ENABLE 0x800

//APIC registers
//APIC registers
#define TPR(high)       (*(volatile uint32_t*)((high) + 0xFEE00080))
#define PPR(high)       (*(volatile uint32_t*)((high) + 0xFEE000A0))
#define EOI(high)       (*(volatile uint32_t*)((high) + 0xFEE000B0))
#define LDR(high)       (*(volatile uint32_t*)((high) + 0xFEE000D0))
#define DFR(high)       (*(volatile uint32_t*)((high) + 0xFEE000E0))
#define SVR(high)       (*(volatile uint32_t*)((high) + 0xFEE000F0))
#define ISR_start(high) (*(volatile uint32_t*)((high) + 0xFEE00100))
#define TMR_start(high) (*(volatile uint32_t*)((high) + 0xFEE00180)) 
#define IRR_start(high) (*(volatile uint32_t*)((high) + 0xFEE00200)) 
#define ICR_low(high)   (*(volatile uint32_t*)((high) + 0xFEE00300))
#define ICR_high(high)  (*(volatile uint32_t*)((high) + 0xFEE00310))
#define LINT0(high)     (*(volatile uint32_t*)((high) + 0xFEE00350))
#define LINT1(high)     (*(volatile uint32_t*)((high) + 0xFEE00360))
#define LVT_timer(high) (*(volatile uint32_t*)((high) + 0xFEE00320))
//I will add the other LVT registers later



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

extern void isr_handler_C(stack_frame_t *frame);
void set_gate(size_t n, uint8_t flags, uint64_t isr_address, uint8_t _ist, uint16_t css);
void init_interrupts();
void init_APIC(uint64_t hhdm_offset);