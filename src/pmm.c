#include "pmm.h"

static uint64_t* bitmap;
extern uint64_t g_hhdm_offset;

//------------------------------------PMM------------------------------------------
void pmm_free_page(uint64_t page_idx) {
    uint64_t array_index = (page_idx) / 64; // The "Row" (which uint64_t)
    uint64_t bit_index = page_idx % 64;   // The "Column" (which bit)

    bitmap[array_index] &= ~(1ULL << bit_index);
}

void pmm_lock_page(uint64_t page_idx) {
    uint64_t array_index = (page_idx) / 64; // The "Row" (which uint64_t)
    uint64_t bit_index = page_idx % 64;   // The "Column" (which bit)

    bitmap[array_index] |= (1ULL << bit_index);
}


uint64_t pmm_alloc2(size_t length) { // assuming length is page to bit
    for(size_t i = 0; i < length/64; i++) {
        if (bitmap[i] == ~0ULL) continue;
        for(size_t j = 0; j < 64; j++) {
            if(!(bitmap[i] & (1ULL << j))) {
                pmm_lock_page(i*64+j);
                return (i*64+j)*PAGE_SIZE;
            }
            
        }
    }
    return 0;
}
//------------------------------------PMM------------------------------------------

//------------------------------------BITMAP------------------------------------------
bool is_entry_type(size_t type, size_t i, struct limine_memmap_response *map) {
    if (map->entries[i]->type == type) {
            return 1;
        }
    return 0;
}

uint64_t get_bitmap_length(struct limine_memmap_response *map, int data_type) { // returns length of bitmap
    uint64_t max_addr = 0;
    uint64_t result;
    for(uint64_t i = 0; i<map->entry_count; i++) {
        uint64_t entry_end = map->entries[i]->base + map->entries[i]->length;
        if(entry_end >= max_addr) {
            max_addr = entry_end;
        }
    } // by the end of this loop, max will have the absolute last byte in the RAM. 
    result = (max_addr+PAGE_SIZE-1)/PAGE_SIZE;
    return (result+data_type-1)/data_type;
}

void set_useable_ram(struct limine_memmap_response *map) {
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if(is_entry_type(LIMINE_MEMMAP_USABLE, i, map)) {
            // Calculate absolute start and count
            uint64_t start_page = map->entries[i]->base / PAGE_SIZE;
            uint64_t page_count = map->entries[i]->length / PAGE_SIZE;

            for(uint64_t j = 0; j < page_count; j++) {
                // Just pass the absolute page number
                pmm_free_page(start_page + j);
            }
        }
    }
}

void protect_bitmap_space(size_t length) { // assuming length is page to qword
    uintptr_t phys_pos = (uintptr_t)bitmap - g_hhdm_offset;
    uint64_t pos = (uint64_t)(phys_pos)/PAGE_SIZE;
    size_t pages_to_lock = (length*8 + PAGE_SIZE - 1) / PAGE_SIZE;
    // length is already here
    for(size_t i = 0; i < pages_to_lock; i++) {
        pmm_lock_page(pos + i);
    }
}

void all_bitmap_1(size_t length) {
    for(size_t i = 0; i < length; i++) bitmap[i] = ~0ULL; // everything is unavailable
}

void bitmap_pointer(struct limine_memmap_response *map, size_t length) {
    size_t required_bytes = length * 8; // assuming length is page to qword
    for(size_t i = 0; i < map->entry_count; i++) {
        if(is_entry_type(LIMINE_MEMMAP_USABLE, i, map) && map->entries[i]->length >= required_bytes) {
            bitmap = (uint64_t*)(map->entries[i]->base + g_hhdm_offset);
            break;
        }
    }
}

void init_bitmap_pmm(struct limine_memmap_response *map) {
    size_t bitmap_length = get_bitmap_length(map, PAGE_TO_QWORD); // bit map
    bitmap_pointer(map, bitmap_length);
    all_bitmap_1(bitmap_length);
    set_useable_ram(map);
    protect_bitmap_space(bitmap_length);
    pmm_lock_page(0); // just for safety and debugging

}
//------------------------------------BITMAP------------------------------------------



