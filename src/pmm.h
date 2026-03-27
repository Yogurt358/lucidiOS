#pragma once
#include "common.h"

#define PAGE_SIZE 4096

bool is_entry_type(size_t type, size_t i, struct limine_memmap_response *map);
uint64_t get_bitmap_length(struct limine_memmap_response *map);
void set_bitmap_pmm(struct limine_memmap_response *map);