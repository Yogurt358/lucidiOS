#pragma once
#include "common.h"

#define PAGE_SIZE 4096
#define PAGE_TO_BIT 1
#define PAGE_TO_QWORD 64
//------------------------------------BITMAP------------------------------------------
bool is_entry_type(size_t type, size_t i, struct limine_memmap_response *map);
uint64_t get_bitmap_length(struct limine_memmap_response *map, int data_type);
void init_bitmap_pmm(struct limine_memmap_response *map);
void set_useable_ram(struct limine_memmap_response *map);
void all_bitmap_1(size_t length);
void bitmap_pointer(struct limine_memmap_response *map, size_t length);
void protect_bitmap_space(size_t length);
//------------------------------------BITMAP------------------------------------------

//------------------------------------PMM------------------------------------------
uint64_t pmm_alloc2(size_t length);
void pmm_free_page(uint64_t page_idx);
void pmm_lock_page(uint64_t page_idx);
//------------------------------------PMM------------------------------------------
