
#include "apic.h"

extern uint64_t g_hhdm_offset;

void init_APIC_timer(void) {

    uint32_t timer = 0;
    uint32_t div_config = 0;

    //setting up LVT_timer
    memset(&timer, 0, 4);
    timer |= 0x21;
    timer |= (0b0<<16); // yeah it's redundant but I keep this for consistency sake
    timer |= (0b01<<17);
    LVT_timer(g_hhdm_offset) = timer;

    //setting up initial count register
    memset(&div_config, 0, 4);
    div_config |= 0b11;
    div_config |= (0b0<<3);
    Divide_Configuration_R(g_hhdm_offset) = div_config;

    //setting up current count register
    Initial_Count_R(g_hhdm_offset) = 0xFFFFFFFF;

    outb(0x43, 0x34); // Channel 0, lobyte/hibyte, rate generator
    outb(0x40, 0x9B); // Low byte
    outb(0x40, 0x2E); // High byte

    uint16_t last_count = 0xFFFF;
    while(1) {
        outb(0x43, 0x00); // Latch command
        uint8_t lo = inb(0x40);
        uint8_t hi = inb(0x40);
        uint16_t count = (hi << 8) | lo;
        if (count > last_count) break; // 10ms passed.
        last_count = count;
    }

    // 4. Stop LAPIC and calculate the rate
    uint32_t current_lapic = Current_Count_R(g_hhdm_offset);
    Initial_Count_R(g_hhdm_offset) = 0; // Stop the timer
    
    uint32_t ticks_per_10ms = 0xFFFFFFFF - current_lapic;
    Initial_Count_R(g_hhdm_offset) = ticks_per_10ms;
    
}

void init_APIC_error(void) {
    memset(&LVT_error(g_hhdm_offset), 0, 4);
    LVT_error(g_hhdm_offset) |= 0x20;
}

void disable_pic(void) {
    // Port 0x21 is Master PIC mask register
    // Port 0xA1 is Slave PIC mask register
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void init_APIC(void) {

    SVR(g_hhdm_offset) |= (0b1<<8); 

    write_better("\nsetting up LAPIC Timer\n");
    init_APIC_timer();
    write_better("\nLAPIC Timer set up\n");

    write_better("\nsetting up LAPIC Error\n");
    init_APIC_error();
    write_better("\nLAPIC Error set up\n");

    write_better("\nsetting up LINT0 and LINT1\n");
    memset(&LINT0(g_hhdm_offset), 0, 4);
    LINT0(g_hhdm_offset) |= (0b1<<16); // mask
    disable_pic();
    
    memset(&LINT1(g_hhdm_offset), 0, 4);
    LINT1(g_hhdm_offset) |= (0b100<<8); // NMI
    write_better("\nthey set\n");

    TPR(g_hhdm_offset) = 0;

}

