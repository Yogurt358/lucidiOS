#include "fb.h"
#include "limine.h"


uint8_t font[16] = {
    0b00000000, //
    0b00000000, //
    0b00000000, //
    0b00010000, //    █    
    0b00111000, //   ███   
    0b01101100, //  ██ ██  
    0b11000110, // ██   ██ 
    0b11000110, // ██   ██ 
    0b11000110, // ██   ██ 
    0b11111110, // ███████ 
    0b11000110, // ██   ██ 
    0b11000110, // ██   ██ 
    0b11000110, // ██   ██ 
    0b11000110, // ██   ██ 
    0b11000110, // ██   ██ 
    0b00000000  //
};


void draw_pixel (struct limine_framebuffer* _fb, int x, int y, uint32_t color) {
    //uint64_t width = _fb->width;
    //uint64_t height = _fb->height;
    volatile uint32_t *fb_ptr = _fb->address;

    fb_ptr[y*(_fb->pitch/4)+x] = color;

}

void draw_character(struct limine_framebuffer* _fb, int x, int y) {
    volatile uint32_t *fb_ptr = _fb->address;


    for (size_t j = 0; j < 16; j++) {
        uint8_t glyph = font[j]; // glyph starts at 0b00000000
        
        for (size_t i = 0; i < 8; i++) {
            if (glyph & (0x80 >> i)) {fb_ptr[(y+j)*(_fb->pitch/4)+(x+i)] = RGB32_WHITE;}
            else {fb_ptr[(y+j)*(_fb->pitch/4)+(x+i)] = RGB32_BLACK;}
        }
    }
}