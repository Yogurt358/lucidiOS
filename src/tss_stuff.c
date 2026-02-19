#include "gdt_handle.h"

volatile uint8_t kernel_stack[8192] __attribute__((aligned(16)));
tss_entry_t entry;
tss_entry_t* entry_ptr = &entry;
tss_descriptor_t descriptor;
gdtr_t tss_reg;



void init_tss() {

    //limit is the limit of the TSS_entry
    //base is the address of the entry

    sgdt(&tss_reg);
    tss_reg.limit += 16;

    uint32_t entry_size = sizeof(tss_entry_t);

    memset(&entry,0,sizeof(tss_entry_t));
    entry.rsp0 = (uint64_t)&kernel_stack + sizeof(kernel_stack);
    entry.iopb = sizeof(tss_entry_t);
       
    memset(&descriptor,0,sizeof(tss_descriptor_t));
    descriptor.limit1 = (entry_size-1) & 0x0FFFF; // 0-15 bits of tss entry size
    descriptor.limit2 = ((entry_size-1)>>16) & 0xF; // // 16-19 bits of tss entry size
    descriptor.base1 =  (uint64_t)(entry_ptr) & 0xFFFF; // 0-15 bits of tss entry address
    descriptor.base2 =  (uint64_t)(entry_ptr)>>16 & 0xFF;// 16-23 bits of tss entry address
    descriptor.base3 = (uint64_t)(entry_ptr)>>24 & 0xFF;// 24-31 bits -...-
    descriptor.base4 = (uint64_t)(entry_ptr)>>32 & 0xFFFFFFFF; // 32-63 bits -...-
    descriptor.access_byte = 0b10001001;
    descriptor.flags = 0b0000;

    void* gdt_dest = (void*)(tss_reg.base + 0x28);
    memcpy(gdt_dest, &descriptor, sizeof(tss_descriptor_t));

}

void load_tss() {
    lgdt(&tss_reg);
    ltr();
}
