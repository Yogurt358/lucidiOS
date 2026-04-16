#pragma once
#include "common.h"

#define RGB32_WHITE     0xFFFFFF
#define RGB32_BLACK     0x000000
#define RGB32_PURPLE    0xFF4500
#define RGB32_RED       0xFF0000
#define RGB32_GREEN     0x006600

void draw_pixel (struct limine_framebuffer* _fb, size_t x, size_t y, uint32_t color);
void draw_character (struct limine_framebuffer* _fb, size_t x, size_t y, char c);
void draw_sentence(struct limine_framebuffer* _fb, char* s);
void newLine(struct limine_framebuffer* _fb);
void scrollUp(struct limine_framebuffer* _fb);
void reset(struct limine_framebuffer* _fb);
void screen_saver(struct limine_framebuffer* _fb);
void fill_half(struct limine_framebuffer* _fb, size_t x, size_t y_bottom, size_t y_top, uint32_t color);