
#include "apic.h"

extern uint64_t g_hhdm_offset;
extern uint64_t *rsdp_pointer;
extern uint64_t ioapic_base;

rsdp_t *apic_rsdp;
acpi_header_t *apic_rsdt_header;
rsdt_t *apic_rsdt;
madt_t *the_madt;

void init_APIC_timer(void) {

    uint32_t timer = 0;
    uint32_t div_config = 0;

    //setting up LVT_timer
    timer |= 0x21;
    timer |= (0b0<<16); // mask bit
    timer |= (0b01<<17);
    LVT_timer(g_hhdm_offset) = timer;

    //setting up initial count register
    div_config |= 0b11;
    div_config |= (0b0<<3);
    Divide_Configuration_R(g_hhdm_offset) = div_config;

    //setting up current count register
    Initial_Count_R(g_hhdm_offset) = 0xFFFFFFFF;

    outb(0x43, 0x34); // Channel 0, lobyte/hibyte, rate generator
    outb(0x40, 0x9B); // Low byte
    outb(0x40, 0x2E); // High byte

    uint16_t last_count = 0xFFFF;
    while(1) {
        outb(0x43, 0x00); // Latch command
        uint8_t lo = inb(0x40);
        uint8_t hi = inb(0x40);
        uint16_t count = (hi << 8) | lo;
        if (count > last_count) break; // 10ms passed.
        last_count = count;
    }

    uint32_t current_lapic = Current_Count_R(g_hhdm_offset);
    Initial_Count_R(g_hhdm_offset) = 0; // Stop the timer
    
    uint32_t ticks_per_10ms = 0xFFFFFFFF - current_lapic;
    Initial_Count_R(g_hhdm_offset) = ticks_per_10ms;
    
}

void init_APIC_error(void) {
    LVT_error(g_hhdm_offset) = 0;
    LVT_error(g_hhdm_offset) |= 0x20;
}

void disable_pic(void) {
    // Port 0x21 is Master PIC mask register
    // Port 0xA1 is Slave PIC mask register
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void init_LINTS(void) {
    LINT0(g_hhdm_offset) |= (0b1<<16); // mask
    disable_pic();
    
    LINT1(g_hhdm_offset) |= (0b100<<8); // NMI
}

void init_LAPIC(void) {

    SVR(g_hhdm_offset) |= (0b1<<8); 

    write_better("\nsetting up LAPIC Timer\n");
    init_APIC_timer();
    write_better("\nLAPIC Timer set up\n");

    write_better("\nsetting up LAPIC Error\n");
    init_APIC_error();
    write_better("\nLAPIC Error set up\n");

    write_better("\nsetting up LINT0 and LINT1\n");
    init_LINTS();
    write_better("\nLINT0 and LINT1 are set\n");


    //TPR(g_hhdm_offset) = 0;
    uint64_t tpr_value = 0;
    asm volatile("mov %0, %%cr8"::"r"(tpr_value):"memory");

}

void check_RSDP(void) {
    if(apic_rsdp->revision == 2) {
        write_better("\nXSDT\n");
    }
    else if(apic_rsdp->revision == 0) {
        write_better("\nRSDT\n");
    }
    else {
        write_better("\nno good\n");
    }
}

void* findAPIC(void) {
    size_t entries = (apic_rsdt->h.length - sizeof(acpi_header_t)) / 4;

    for (size_t i = 0; i < entries; i++) {
        uint32_t phys_addr = apic_rsdt->pointerToOtherSDT[i];
        acpi_header_t *h = (acpi_header_t *)((uint64_t)phys_addr + g_hhdm_offset);

        if (memcmp(h->signature, "APIC", 4) == 0) { // MADT entry
            return (void *)h;
        }
    }
    return NULL;
}

void set_IRQ(size_t n, uint8_t vector) {
    uint32_t temp = 0;
    // setting bits 0 - 31
    temp = vector;
    // delivery mode already set (fixed)
    // destination mode physical for now (system ain't an SMP yet)
    // high polarity
    IOREGSEL(ioapic_base) = IOAPICREDTBL(n);
    IOWIN(ioapic_base) = temp;

    //setting bits 32-63
    IOREGSEL(ioapic_base) = IOAPICREDTBL(n)+1;
    IOWIN(ioapic_base) |= (0b0000<<24); // first LAPIC id, yeah useless I know
}

void madt_parsing(void) {
    //check_RSDP(); // got RSDP

    apic_rsdp = (rsdp_t*)(rsdp_pointer);
    apic_rsdt = (rsdt_t*)(apic_rsdp->rsdt_Address + g_hhdm_offset);
    
    the_madt = (madt_t*)(findAPIC()); 

    if (the_madt == NULL) {
        write_better("\nno MADT\n");
        return;
    }

    uint8_t *ptr = (uint8_t*)the_madt + 44; //skips acpi_header and 2 uint32_t flags
    uint8_t *end = (uint8_t*)the_madt + the_madt->h.length;

    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t length = ptr[1];

        if (type == 1) { 
            IOAPIC_t *ioapic = (IOAPIC_t *)ptr;
            ioapic_base = (uint64_t)ioapic->ioapic_address + g_hhdm_offset;
            write_better("Found I/O APIC\n");
            break; 
        }
        ptr += length; // next entry
    }
    asm volatile ("movq %0, %%rax"::"r"(ioapic_base):"rax"); // debug address
}


void init_IOAPIC(void) {

    write_better("\nMADT found, finding IOAPIC\n");
    set_IRQ(1, 0x32); // set the keyboard up

}

