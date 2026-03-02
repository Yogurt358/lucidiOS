#include "segments.h"

volatile uint8_t kernel_stack[8192] __attribute__((aligned(16)));

the_gdt_t a_gdt __attribute__((aligned));

gdtr_t gdt_reg;
tss_descriptor_t tss_desc;
tss_entry_t tss_entry;


/*
entry 0 - NULL mustn't be filled    8 bytes | vector 0
entry 1 - Kernel CS                 8 bytes | vector 8
entry 2 - User CS                   8 bytes | vector 16
entry 3 - Kernel DS                 8 bytes | vector 24
entry 4 - User DS                   8 bytes | vector 32

entry 5 - TSS                       16 bytes | vector 40
entry 6 - TSS                       (second half) | vector 48
*/


void set_segment(size_t n, uint8_t access, uint8_t flag) { // is this function efficient?
    switch(n) {
        case(1): // Kernel CS
        a_gdt.kernel_cs.limit1 = 0xFFFF;      // Standard full limit
        a_gdt.kernel_cs.base1 = 0;
        a_gdt.kernel_cs.base2 = 0;
        a_gdt.kernel_cs.access_byte = access;
        a_gdt.kernel_cs.limit2 = 0xF;         // Part of the 0xFFFFF limit
        a_gdt.kernel_cs.flags = flag;         // 4-bit flags
        a_gdt.kernel_cs.base3 = 0;
        break;
        
        case(2): // User CS
        a_gdt.user_cs.limit1 = 0xFFFF;      // Standard full limit
        a_gdt.user_cs.base1 = 0;
        a_gdt.user_cs.base2 = 0;
        a_gdt.user_cs.access_byte = access;
        a_gdt.user_cs.limit2 = 0xF;         // Part of the 0xFFFFF limit
        a_gdt.user_cs.flags = flag;         // 4-bit flags
        a_gdt.user_cs.base3 = 0;
        break;

        case(3): // Kernel DS
        a_gdt.kernel_ds.limit1 = 0xFFFF;      // Standard full limit
        a_gdt.kernel_ds.base1 = 0;
        a_gdt.kernel_ds.base2 = 0;
        a_gdt.kernel_ds.access_byte = access;
        a_gdt.kernel_ds.limit2 = 0xF;         // Part of the 0xFFFFF limit
        a_gdt.kernel_ds.flags = flag;         // 4-bit flags
        a_gdt.kernel_ds.base3 = 0;
        break;

        case(4): // User DS
        a_gdt.user_ds.limit1 = 0xFFFF;      // Standard full limit
        a_gdt.user_ds.base1 = 0;
        a_gdt.user_ds.base2 = 0;
        a_gdt.user_ds.access_byte = access;
        a_gdt.user_ds.limit2 = 0xF;         // Part of the 0xFFFFF limit
        a_gdt.user_ds.flags = flag;         // 4-bit flags
        a_gdt.user_ds.base3 = 0;
        break;

        default: // undefined segment accessed, causing a fault
        ud2();
        break;

    }
}

void set_TSS(uint8_t access, uint8_t flag) {
    uint64_t temp = (uint64_t)&tss_entry;

    a_gdt.tss.flags = flag & 0x0F;
    a_gdt.tss.access_byte = access;

    // base is the address of tss_entry
    // limit is ignored
    tss_entry.rsp0 = (uint64_t)&kernel_stack + sizeof(kernel_stack);
    tss_entry.iopb = sizeof(tss_entry_t);
    // for now there is no need for IST
    a_gdt.tss.limit1 = sizeof(tss_entry)-1;
    a_gdt.tss.limit2 = 0;
    a_gdt.tss.base1 = (uint16_t)(temp)&0xFFFF;
    a_gdt.tss.base2 = (uint8_t)(temp>>16)&0xFF;
    a_gdt.tss.base3 = (uint8_t)(temp>>24)&0xFF;
    a_gdt.tss.base4 = (uint32_t)(temp>>32)&0xFFFFFFFF;

}

void init_gdt() {

    write_better("setting up GDT\n");
    gdt_reg.base = (uint64_t)&a_gdt;
    gdt_reg.limit = (sizeof(a_gdt))-1;
    memset(&a_gdt,0,sizeof(a_gdt)); // reserved regions must be 0 or else general protection fault

    set_segment(1, 0x9B, 0x0A); // Kernel CS
    set_segment(2, 0xFB, 0x0A); // User CS
    set_segment(3, 0x93, 0x0C); // Kernel DS
    set_segment(4, 0xF3, 0x0C); // User DS
    set_TSS(0b10001001,0b0000); // TSS

    lgdt(&gdt_reg);
    ltr();
    reload_segments();


    write_better("GDT set up\n\n");

}



