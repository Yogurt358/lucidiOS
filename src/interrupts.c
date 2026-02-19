#include "interrupts.h"
extern void isr0();
extern void isr1();
extern void isr8();
extern void isr14();


idtr_t idt_reg;
volatile idt_entry_t i_entry[256] __attribute__((aligned));

extern void isr_handler_C(stack_frame_t *frame) {
    size_t temp = frame->vector;

    switch(temp) {
        case(0):
            write_better("\n#DE Dumbass Piece of Shit you divided by zero you stupid what's 9+10 21\n");
            for(;;);
            break;
        case(1):
            write_better("\n why'd you stop?");
            for(;;);
            break;
        case(8):
            write_better("\n#DF not only did you fuck it up once but TWICE! define your goddam IDT Gates!\n");
            for(;;);
            break;
        case(14):
            write_better("\n#PG did you actually just try to go there? THAT MEMORY ISN'T DEFINED!!");
            uint64_t faulting_address;
            asm volatile("mov %%cr2, %0" : "=r"(faulting_address));
            for(;;);
            break;
        default:
            break;
    }
}

void set_gate(size_t n, uint8_t flags, uint64_t isr_address, uint8_t _ist, uint16_t css) {

    i_entry[n].type_attributes = flags;
    i_entry[n].selector = css;
    i_entry[n].offset1 = (uint16_t)(isr_address&0xFFFF);
    i_entry[n].offset2 = (uint16_t)((isr_address>>16)&0xFFFF);
    i_entry[n].offset3 = (uint32_t)((isr_address>>32)&0xFFFFFFFF);
    i_entry[n].ist = _ist;

}

void init_interrupts() {
    memset(&i_entry, 0, sizeof(i_entry));

    uint16_t current_cs = get_cs();

    set_gate(0, 0x8E, (uint64_t)isr0, 0, current_cs); // #DE gate
    set_gate(1, 0x8E, (uint64_t)isr1, 0, current_cs); // #DB gate
    set_gate(8, 0x8E, (uint64_t)isr8, 0, current_cs); // #DF gate
    set_gate(14, 0x8E, (uint64_t)isr14, 0, current_cs); // #PF gate

    idt_reg.size = (sizeof(idt_entry_t) * 256) - 1;
    idt_reg.offset = (uint64_t)&i_entry;

    lidt(&idt_reg);
}
