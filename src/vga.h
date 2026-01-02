#pragma once
#include <stdint.h>

#define COLOR_BLACK 0x0
#define COLOR_WHITE 0xF

#define width 80
#define height 25

void print(const char* s);
void scrollUp();
void newLine();
void Reset();