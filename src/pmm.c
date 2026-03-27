#include "pmm.h"

static uint64_t* bitmap;
extern uint64_t g_hhdm_offset;

bool is_entry_type(size_t type, size_t i, struct limine_memmap_response *map) {
    if (map->entries[i]->type == type) {
            return 1;
        }
    return 0;
}

uint64_t get_bitmap_length(struct limine_memmap_response *map) { // returns length of bitmap
    uint64_t max_addr = 0;
    for(uint64_t i = 0; i<map->entry_count; i++) {
        uint64_t entry_end = map->entries[i]->base + map->entries[i]->length;
        if(entry_end >= max_addr) {
            max_addr = entry_end;
        }
    } // by the end of this loop, max will have the absolute last byte in the RAM. 
    return (max_addr+4095)/PAGE_SIZE;
}

void set_bitmap_pmm(struct limine_memmap_response *map) {
    size_t bitmap_length = get_bitmap_length(map);
    bitmap_length = (bitmap_length+63)/64; // the length in uint64_t

    for (uint64_t i = 0; i < map->entry_count; i++) {
        // Look for a USABLE block that is big enough
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE && map->entries[i]->length >= (bitmap_length*8)) {
            bitmap = (uint64_t*)(map->entries[i]->base + g_hhdm_offset);
            for (size_t j = 0; j<bitmap_length; j++) {bitmap[j] = 0xFFFFFFFFFFFFFFFF;}
            break;
        }
    }

    for (uint64_t i = 0; i < map->entry_count; i++) {
        if(is_entry_type(LIMINE_MEMMAP_USABLE, i, map)) map->entries[i]->
    }
}