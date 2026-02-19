#include "segments.h"

gdtr_t gdt_reg;
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

void init_gdt() {
    gdt_reg.base = (uint64_t)&s_entry;
    gdt_reg.limit = 56;

    

    lgdt(&gdt_reg);
}

