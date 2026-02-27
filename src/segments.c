#include "segments.h"

volatile uint8_t kernel_stack[8192] __attribute__((aligned(16)));
gdt_descriptor_t gdt_desc[5] __attribute__((aligned));
gdtr_t gdt_reg;
tss_descriptor_t tss_desc;
tss_entry_t tss_entry;
volatile uint16_t check_CS;

/*
entry 0 - NULL mustn't be filled    8 bytes 
entry 1 - Kernel CS                 8 bytes
entry 2 - User CS                   8 bytes 
entry 3 - Kernel DS                 8 bytes 
entry 4 - User DS                   8 bytes 

entry 5 - TSS                       16 bytes 
entry 6 - TSS                       (second half)
*/


void set_segment(size_t n, uint8_t access, uint8_t flag) {
    gdt_desc[n].access_byte = access;
    gdt_desc[n].flags = flag;

}

void set_TSS(uint8_t access, uint8_t flag) {
    uint64_t temp = (uint64_t)&tss_entry;

    tss_desc.flags = flag & 0x0F;
    tss_desc.access_byte = access;

    // base is the address of tss_entry
    // limit is ignored
    tss_entry.rsp0 = (uint64_t)&kernel_stack + sizeof(kernel_stack);
    tss_entry.iopb = sizeof(tss_entry_t);
    // for now there is no need for IST
    tss_desc.limit1 = sizeof(tss_entry)-1;
    tss_desc.base1 = (uint16_t)(temp)&0xFFFF;
    tss_desc.base2 = (uint8_t)(temp>>16)&0xFF;
    tss_desc.base3 = (uint8_t)(temp>>24)&0xFF;
    tss_desc.base4 = (uint32_t)(temp>>32)&0xFFFFFFFF;

}

void init_gdt() {
    gdt_reg.base = (uint64_t)&gdt_desc;
    gdt_reg.limit = (sizeof(gdt_desc))-1;
    memset(&gdt_desc,0,sizeof(gdt_desc)); // reserved regions must be 0 or else general protection fault

    write_better("setting up GDT\n");

    set_segment(1,0b10011011,0b0110); // Kernel CS
    check_CS = get_cs();
    set_segment(2,0b11111011,0b0110); // User CS
    set_segment(3,0b10010011,0b0100); // Kernel DS
    set_segment(4,0b11110011,0b0100); // User DS
    //set_TSS(0b10001001,0b0000); // TSS

    lgdt(&gdt_reg);
    reload_segments();


    write_better("GDT set up\n");

    //ltr();
}



