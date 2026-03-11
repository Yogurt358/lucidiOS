
#include "apic.h"


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
    outb(0x40, 0x9B); 

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