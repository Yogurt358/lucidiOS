#include "segments.h"

volatile uint8_t kernel_stack[8192] __attribute__((aligned(16)));
gdtr_t gdt_reg;
gdt_descriptor_t g_desc;
tss_descriptor_t t_desc;
tss_entry_t t_entry;
/*
entry 0 - NULL mustn't be filled    8 bytes 
entry 1 - Kernel CS                 8 bytes
entry 2 - User CS                   8 bytes 
entry 3 - Kernel DS                 8 bytes 
entry 4 - User DS                   8 bytes 
entry 5 - TSS                       16 bytes 
entry 6 - TSS                       (second half)
*/

gdt_descriptor_t s_entry[7] __attribute__((aligned)); 

void set_segment(uint8_t access, uint8_t flag) {
    g_desc.access_byte = access;
    g_desc.flags = flag;

}
void set_TSS(uint8_t access, uint8_t flag) {
    uint64_t temp = (uint64_t)&t_entry;
    // base is the address of t_entry
    // limit is ignored
    t_entry.rsp0 = (uint64_t)&kernel_stack + sizeof(kernel_stack);
    t_entry.iopb = sizeof(tss_entry_t);
    // for now there is no need for IST

    t_desc.base1 = (uint16_t)(temp)&0xFFFF;
    t_desc.base2 = (uint8_t)(temp>>8)&0xFF;
    t_desc.base3 = (uint8_t)(temp>>24)&0xFF;
    t_desc.base4 = (uint32_t)(temp>>32)&0xFFFFFFFF;

    t_desc.flags = flag;
    t_desc.access_byte = access;

}

void init_gdt() {
    gdt_reg.base = (uint64_t)&s_entry;
    gdt_reg.limit = 56;
    memset(&gdt_reg,0,sizeof(gdt_reg)); // reserved regions must be 0 or else general protection fault

    write_better("setting up GDT\n");

    set_segment(0b1001101,0b0010); // Kernel CS
    set_segment(0b1111101,0b0010); // User CS
    set_segment(0b1001001,0b0100); // Kernel DS
    set_segment(0b1111001,0b0100); // User DS
    set_TSS(0b10001001,0b0000); // TSS

    write_better("GDT set up\n");

    lgdt(&gdt_reg);
    ltr();
}



