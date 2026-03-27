#include "interrupts.h"
extern void isr0();
extern void isr1();
extern void isr6();
extern void isr8();
extern void isr13();
extern void isr14();

extern void isr32();
extern void isr33();

extern void isr50();

extern uint64_t g_hhdm_offset;

idtr_t idt_reg;
idt_entry_t i_entry[256] __attribute__((aligned(16))); // removed volatile because of warning with memset. will figure it out
uint64_t faulting_address;

extern void isr_handler_C(stack_frame_t *frame) {
    size_t temp = frame->vector;

    switch(temp) {
        case(0):
            write_better("\n#DE\n");
            for(;;);
            break;

        case(1):
            write_better("\n#DB\n");
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
            asm volatile("movq %0, %%rax" ::"r"(frame->code):"rax");
            for(;;);
            break; 

        case(14):
            write_better("\n#PF\n");
            asm volatile("mov %%cr2, %0" : "=a"(faulting_address)::);
            for(;;);
            break;

        case(32):
            write_better("\n#APIC error\n");
            ESR(g_hhdm_offset) = 0;
            uint8_t error_flag = ESR(g_hhdm_offset);
            asm volatile("movzbl %0, %%eax" ::"r"(error_flag):"rax");
            EOI(g_hhdm_offset) = 0;
            break;
    
        case(33):
            write_better("\n#APIC timer\n");
            EOI(g_hhdm_offset) = 0;
            break;
        
        case(50):
            //uint8_t scancode = inb(0x60);
            write_better("\n#I/O APIC keyboard\n");
            EOI(g_hhdm_offset) = 0;
            break;

        default:
            write_better("\nNo valid interrupt given\n");
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

    set_gate(32, 0x8E, (uint64_t)isr32, 0, current_cs); // APIC error
    set_gate(33, 0x8E, (uint64_t)isr33, 0, current_cs); // APIC timer

    set_gate(50, 0x8E, (uint64_t)isr50, 0, current_cs); // keyboard

    write_better("IDT set up\n\n");


    idt_reg.size = (sizeof(idt_entry_t) * 256) - 1;
    idt_reg.offset = (uint64_t)&i_entry;

    lidt(&idt_reg);
}
