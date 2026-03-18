#pragma once
#include "common.h"

#define IA32_APIC_BASE_MSR_BSP 0x100 
#define IA32_x2APIC_MSR 0x802

//APIC_timer registers
#define LINT0(high)                     (*(volatile uint32_t*)((high) + 0xFEE00350))
#define LINT1(high)                     (*(volatile uint32_t*)((high) + 0xFEE00360))

#define LVT_timer(high)                 (*(volatile uint32_t*)((high) + 0xFEE00320))
#define Initial_Count_R(high)           (*(volatile uint32_t*)((high) + 0xFEE00380))
#define Current_Count_R(high)           (*(volatile uint32_t*)((high) + 0xFEE00390))
#define Divide_Configuration_R(high)    (*(volatile uint32_t*)((high) + 0xFEE003E0))

#define LVT_error(high)                 (*(volatile uint32_t*)((high) + 0xFEE00370))
#define ESR(high)                       (*(volatile uint32_t*)((high) + 0xFEE00280))

#define TPR(high)                       (*(volatile uint32_t*)((high) + 0xFEE00080))

#define EOI(high)                       (*(volatile uint32_t*)((high) + 0xFEE000B0))

#define SVR(high)                       (*(volatile uint32_t*)((high) + 0xFEE000F0))



void init_APIC(void);
void init_APIC_timer(void);
void init_APIC_error(void);
void disable_pic(void);