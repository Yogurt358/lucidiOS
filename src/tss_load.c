#include "gdt_handle.h"
#include "limine.h"

volatile uint8_t kernel_stack[8192];

void init_tss() { // gives all the tss entry components their needed values, need to use memset, how?

}

//void load_tss(){}
//void switch_save(){}

