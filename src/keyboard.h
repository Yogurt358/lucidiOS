#pragma once
#include "common.h"

#define KBD_BUF_SIZE 256u

void keyboard_init(void);                          
void keyboard_handle_scancode(uint8_t scancode);   
int  keyboard_pop(void);                          