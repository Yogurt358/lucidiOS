#pragma once
#include <stdint.h>
#include <stddef.h>
#include "limine.h"

#define RGB32_WHITE 0xFFFFFF
#define RGB32_BLACK 0x000000


void draw_pixel (struct limine_framebuffer* _fb, size_t x, size_t y, uint32_t color);

void draw_character (struct limine_framebuffer* _fb, size_t x, size_t y, char c);

void draw_sentence(struct limine_framebuffer* _fb, size_t x, size_t y, char* s);