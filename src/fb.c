#include "fb.h"
#include "limine.h"


void draw_pixel (struct limine_framebuffer* _fb, int x, int y, uint32_t color) {
    //uint64_t width = _fb->width;
    //uint64_t height = _fb->height;
    volatile uint32_t *fb_ptr = _fb->address;

    fb_ptr[y*(_fb->pitch/4)+x] = color;

}