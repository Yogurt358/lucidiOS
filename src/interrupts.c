#include "interrupts.h"
extern void isr0();

idtr_t idt_reg;
idt_entry_t i_entry[256] __attribute__((aligned));

extern void isr_handler_C(stack_frame_t *frame) {
    size_t temp = frame->vector;

    switch(temp) {
        case(0):
            for(;;);
            break;
        default:
            break;
    }
}

void set_gate(size_t n, uint8_t flags, uint64_t isr_address, uint8_t _ist) {

    memset(&i_entry[n], 0, sizeof(i_entry[n])); // ensure reserved fields in idt entry are 0
    i_entry[n].type_attributes = flags;
    i_entry[n].selector = CS_SELECTOR;
    i_entry[n].offset1 = (uint16_t)(isr_address&0xFFFF);
    i_entry[n].offset2 = (uint16_t)((isr_address>>16)&0xFFFF);
    i_entry[n].offset3 = (uint32_t)((isr_address>>32)&0xFFFFFFFF);
    i_entry[n].ist = _ist;

}

void init_interrupts() {
    memset(&i_entry, 0, sizeof(i_entry));

    set_gate(0, 0x8E, (uint64_t)isr0, 0);

    idt_reg.size = (sizeof(idt_entry_t) * 256) - 1;
    idt_reg.offset = (uint64_t)&i_entry;

    lidt(&idt_reg);
}
