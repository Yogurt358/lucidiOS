#include "idt.h"

idtr_t idt_reg;
idt_entry_t i_entry;
idt_entry_t* pentry = &i_entry;

void init_idt() {
    sidt(&idt_reg);
    idt_reg.size += 16;
}

void load_idt() {
    ;
}