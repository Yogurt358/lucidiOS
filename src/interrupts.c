#include "interrupts.h"

idtr_t idt_reg;
idt_entry_t i_entry;
idt_entry_t* pentry = &i_entry;

extern void isr_handler_0() {
    ;
}


void entry_DE() {
    sidt(&idt_reg);
    idt_reg.size += 16;
    memset(pentry, 0, sizeof(i_entry)); // ensure reserved fields in idt entry are 0
    i_entry.type_attributes = 0b10001111; // p=1;DPL=00;0=0;gate=0xF
    i_entry.selector = CS_SELECTOR;

}

void load_idt() {
    lidt(&idt_reg);
}