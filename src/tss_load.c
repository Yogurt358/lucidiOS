#include "gdt_handle.h"
#include "limine.h"
#include "common.h"

volatile uint8_t kernel_stack[8192];
uint16_t tss_selector = 0x28;
tss_entry_t entry;
tss_descriptor_t descriptor;



void init_tss() {

    //limit is the limit of the TSS_entry
    //base is the address of the entry

    memset(&entry,0,TSS_ENTRY_SIZE);
    entry.rsp0 = (uint64_t)&kernel_stack + sizeof(kernel_stack);
    entry.iopb = sizeof(tss_entry_t);

       
    memset(&descriptor,0,sizeof(tss_descriptor_t));
    /*descriptor.limit1 = 
    descriptor.limit2 = 
    descriptor.base1 = 
    descriptor.base2 = 
    descriptor.base3 = 
    descriptor.base4 = 
    */descriptor.access_byte = 0b10010001;
    descriptor.flags = 0b0000;

}

//void load_tss(){}
//void switch_save(){}
