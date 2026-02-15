#include "interrupts.h"

idtr_t idt_reg;
idt_entry_t i_entry;
idt_entry_t* pentry = &i_entry;

extern void isr_handler_0() {
    ;
}


void init_idt(uint8_t idt_flags) {
    sidt(&idt_reg);
    idt_reg.size += 16;
    memset(pentry, 0, sizeof(i_entry)); // ensure reserved fields in idt entry are 0
    i_entry.type_attributes = idt_flags;
    i_entry.selector = CS_SELECTOR;

}

void load_idt() {
    lidt(&idt_reg);
}