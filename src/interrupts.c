#include "interrupts.h"
extern void isr0();
extern void isr1();
extern void isr6();
extern void isr8();
extern void isr13();
extern void isr14();
extern void isr32();


idtr_t idt_reg;
idt_entry_t i_entry[256] __attribute__((aligned(16))); // removed volatile because of warning with memset. will figure it out
uint64_t faulting_address;

extern void isr_handler_C(stack_frame_t *frame, uint64_t hhdm_offset) {
    size_t temp = frame->vector;

    switch(temp) {
        case(0):
            write_better("\n#DE\n");
            for(;;);
            break;

        case(1):
            write_better("\n#DB");
            for(;;);
            break;

        case(6):
            write_better("\n#UD\n");
            for(;;);
            break;

        case(8):
            write_better("\n#DF\n");
            for(;;);
            break;

        case(13):
            write_better("\n#GP\n");
            asm volatile("mov %%cr2, %0" : "=r"(faulting_address)::);
            for(;;);
            break;

        case(14):
            write_better("\n#PF");
            asm volatile("mov %%cr2, %0" : "=r"(faulting_address)::);
            for(;;);
            break;

        case(32):
            write_better("\n#APIC timer");
            EOI(hhdm_offset) = 0;
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

    write_better("setting up IDT\n");

    set_gate(0, 0x8E, (uint64_t)isr0, 0, current_cs); // #DE gate
    set_gate(1, 0x8F, (uint64_t)isr1, 0, current_cs); // #DB gate
    set_gate(6, 0x8E, (uint64_t)isr6, 0, current_cs); // #UD gate
    set_gate(8, 0x8E, (uint64_t)isr8, 0, current_cs); // #DF gate
    set_gate(13, 0x8E, (uint64_t)isr13, 0, current_cs); // #GP gate
    set_gate(14, 0x8E, (uint64_t)isr14, 0, current_cs); // #PF gate

    write_better("IDT set up\n\n");


    idt_reg.size = (sizeof(idt_entry_t) * 256) - 1;
    idt_reg.offset = (uint64_t)&i_entry;

    lidt(&idt_reg);
}

void init_APIC_timer(uint64_t hhdm_offset) {

    uint32_t timer = 0;
    uint32_t div_config = 0;

    //setting up LVT_timer
    memset(&timer, 0, 4);
    timer |= 0x20;
    timer |= (0b0<<16); // yeah it's redundant but I keep this for consistency sake
    timer |= (0b01<<17);
    LVT_timer(hhdm_offset) = timer;

    //setting up initial count register
    memset(&div_config, 0, 4);
    div_config |= 0b11;
    div_config |= (0b0<<3);
    Divide_Configuration_R(hhdm_offset) = div_config;

    //setting up current count register
    Initial_Count_R(hhdm_offset) = 0xFFFFFFFF;

    //setting up PIT
    outb(0x43, 0x30); 
    outb(0x40, 0x2E); 
    outb(0x40, 0x29); 

    while(1) {
    outb(0x43, 0x00);
    uint8_t low = inb(0x40);
    uint8_t high = inb(0x40);
    if ((low | (high << 8)) == 0) break;
    }

    uint32_t final_apic_count = Current_Count_R(hhdm_offset);
    uint32_t ticks_per_10ms = 0xFFFFFFFF - final_apic_count;
    Initial_Count_R(hhdm_offset) = 0;
    Initial_Count_R(hhdm_offset) = ticks_per_10ms;
    
}

void init_APIC(uint64_t hhdm_offset) {
    write_better("setting up LAPIC Timer");
    init_APIC_timer(hhdm_offset);
    write_better("LAPIC Timer set up");

}