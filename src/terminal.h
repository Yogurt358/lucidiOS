#pragma once
#include "common.h"
#include "mm.h"

#define RGB32_WHITE     0x00FFFFFF
#define RGB32_BLACK     0x00000000
#define RGB32_PURPLE    0x00FF4500
#define RGB32_RED       0x00FF0000
#define RGB32_GREEN     0x00006600

void draw_pixel (struct limine_framebuffer* _fb, size_t x, size_t y, uint32_t color);
void draw_character (struct limine_framebuffer* _fb, size_t x, size_t y, char c);
void draw_sentence(struct limine_framebuffer* _fb, char* s);
void newLine(struct limine_framebuffer* _fb);
void scrollUp(struct limine_framebuffer* _fb);
void reset(struct limine_framebuffer* _fb);
uint32_t plasma_pixel(size_t x, size_t y, uint32_t t, uint64_t W, uint64_t H);
void screen_saver(struct limine_framebuffer* _fb);
void copy_screen(struct limine_framebuffer* _fb);
void paste_screen(struct limine_framebuffer* _fb);