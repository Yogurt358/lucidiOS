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

// data types taken from OSdev (altered by me)
struct rsdp{ 
    char signature[8];
    uint8_t checksum;
    char oem_ID[6];
    uint8_t revision;
    uint32_t rsdt_Address;
}__attribute__((packed));

struct acpi_header {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_ID[6];
  char oem_table_ID[8];
  uint32_t oem_revision;
  uint32_t creator_ID;
  uint32_t creator_revision;
}__attribute__((packed));

struct rsdt {
    struct acpi_header h;
    uint32_t pointerToOtherSDT[];
}__attribute__((packed));

struct proc_LAPIC {
    uint8_t type;
    uint8_t length;
    uint8_t acip_proc_ID;
    uint8_t apic_ID;
    uint32_t more_flags;
}__attribute__((packed));

struct IOAPIC {
    uint8_t type;
    uint8_t length;
    uint8_t ioapic_ID;
    uint8_t rsvd;
    uint32_t ioapic_address;
    uint32_t global_system_interrupt_base;
}__attribute__((packed));

struct madt{ 
    struct acpi_header h;
    uint32_t LAPIC_address;
    uint32_t flags;
    // if needed I will add the other entries but for now unnecesarry;

}__attribute__((packed));

typedef struct rsdp rsdp_t;
typedef struct acpi_header acpi_header_t;
typedef struct rsdt rsdt_t;
typedef struct proc_LAPIC proc_LAPIC_t;
typedef struct IOAPIC IOAPIC_t;
typedef struct madt madt_t;

void init_LAPIC(void);
void init_APIC_timer(void);
void init_APIC_error(void);
void disable_pic(void);

void init_IOAPIC(void);
void check_RSDP(void);
void* findAPIC(void);
uint64_t madt_parsing(void);
