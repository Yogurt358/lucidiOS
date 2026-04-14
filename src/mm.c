#include "mm.h"

static uint64_t* bitmap;
static uint64_t* heap_bitmap; // length is 8192

extern uint64_t g_hhdm_offset;
extern size_t bitmap_length;

//------------------------------------PMM------------------------------------------
void pmm_free_page(uint64_t page_idx, uint64_t* bitmap_arg) {
    uint64_t array_index = (page_idx) / 64; // The "Row" (which uint64_t)
    uint64_t bit_index = page_idx % 64;   // The "Column" (which bit)

    bitmap_arg[array_index] &= ~(1ULL << bit_index);
}

void pmm_lock_page(uint64_t page_idx, uint64_t* bitmap_arg) {
    uint64_t array_index = (page_idx) / 64; // The "Row" (which uint64_t)
    uint64_t bit_index = page_idx % 64;   // The "Column" (which bit)

    bitmap_arg[array_index] |= (1ULL << bit_index);
}


uint64_t pmm_alloc2() { // assuming length is page to bit
    for(size_t i = 0; i < bitmap_length/64; i++) {
        if (bitmap[i] == ~0ULL) continue;
        for(size_t j = 0; j < 64; j++) {
            if(!(bitmap[i] & (1ULL << j))) {
                pmm_lock_page(i*64+j, bitmap);
                return (i*64+j)*PAGE_SIZE;
            }
            
        }
    }
    return 0;
}
//------------------------------------PMM------------------------------------------

//------------------------------------VMM------------------------------------------
void vmm_alloc(uint64_t virt, uint64_t phys, uint8_t flags) {
    uint64_t pml4_phys = read_cr3() & 0x000FFFFFFFFFF000ULL; 
    uint64_t *pml4_virt = (uint64_t*)(pml4_phys + g_hhdm_offset); 

    // is this useless? a lot of local variables on the stack
    size_t pml4_index = (virt>>39)&0x1FF; // bits 47-39 are the pml4 entry
    size_t pdpt_index = (virt>>30)&0x1FF; // bits 38-30 are the pdpt entry
    size_t pd_index = (virt>>21)&0x1FF;   // bits 29-21 pd entry
    size_t pt_index = (virt>>12)&0x1FF;   // 20-12 pt entry

    if(!(pml4_virt[pml4_index] & PTE_PRESENT)) { 
        uint64_t new_table = pmm_alloc2(bitmap_length);
        memset((void*)(new_table + g_hhdm_offset), 0, 4096);
        pml4_virt[pml4_index] = new_table | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t *pdpt_virt = (uint64_t*)((pml4_virt[pml4_index] & PTE_MASK) + g_hhdm_offset);

    if(!(pdpt_virt[pdpt_index] & PTE_PRESENT)) { 
        uint64_t new_table = pmm_alloc2(bitmap_length);
        memset((void*)(new_table + g_hhdm_offset), 0, 4096);
        pdpt_virt[pdpt_index] = new_table | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t *pd_virt = (uint64_t*)((pdpt_virt[pdpt_index] & PTE_MASK) + g_hhdm_offset);

    if(!(pd_virt[pd_index] & PTE_PRESENT)) { 
        uint64_t new_table = pmm_alloc2(bitmap_length);
        memset((void*)(new_table + g_hhdm_offset), 0, 4096);
        pd_virt[pd_index] = new_table | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t *pt_virt = (uint64_t*)((pd_virt[pd_index] & PTE_MASK) + g_hhdm_offset);

    pt_virt[pt_index] = (phys & PTE_MASK) | flags | 1;
    
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    write_better("\nallocated a page\n");

}
//------------------------------------VMM------------------------------------------


//------------------------------------BITMAP------------------------------------------
bool is_entry_type(size_t type, size_t i, struct limine_memmap_response *map) {
    if (map->entries[i]->type == type) {
            return 1;
        }
    return 0;
}

uint64_t get_bitmap_length(struct limine_memmap_response *map, int data_type) {
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
                pmm_free_page(start_page + j, bitmap);
            }
        }
    }
}

void protect_bitmap_space() { // assuming length is page to qword
    uintptr_t phys_pos = (uintptr_t)bitmap - g_hhdm_offset;
    uint64_t pos = (uint64_t)(phys_pos)/PAGE_SIZE;
    size_t pages_to_lock = (bitmap_length*8 + PAGE_SIZE - 1) / PAGE_SIZE;
    // length is already here
    for(size_t i = 0; i < pages_to_lock; i++) {
        pmm_lock_page(pos + i, bitmap);
    }
}

void all_bitmap_1() {
    for(size_t i = 0; i < bitmap_length; i++) bitmap[i] = ~0ULL; // everything is unavailable
}

void bitmap_pointer(struct limine_memmap_response *map) {
    size_t bytes = bitmap_length * 8; // assuming length is page to qword
    for(size_t i = 0; i < map->entry_count; i++) {
        if(is_entry_type(LIMINE_MEMMAP_USABLE, i, map) && map->entries[i]->length >= bytes) {
            bitmap = (uint64_t*)(map->entries[i]->base + g_hhdm_offset);
            break;
        }
    }
}


//------------------------------------BITMAP------------------------------------------


//------------------------------------HEAP------------------------------------------

void heap_bitmap_pointer() {
    uint64_t phys_for_bitmap = pmm_alloc2();
    heap_bitmap = (uint64_t*)(phys_for_bitmap + g_hhdm_offset);
    memset(heap_bitmap, 0, 8192 * 8);
}

void* kmalloc(size_t amount) {
    size_t blocks = (amount + BLOCK_SIZE - 1) / BLOCK_SIZE + 1;
    size_t contiguous_found = 0;
    size_t start_bit = 0;
    for (size_t i = 0; i < HEAP_MAX_BLOCKS; i++) {
        uint64_t array_idx = i / 64;
        uint64_t bit_idx = i % 64;

        if (!(heap_bitmap[array_idx] & (1ULL << bit_idx))) {
            if (contiguous_found == 0) start_bit = i;
            contiguous_found++;
            if (contiguous_found == blocks) {
                // list is big enough
                for (size_t k = 0; k < blocks; k++) {
                    pmm_lock_page(start_bit + k, heap_bitmap);
                }
                
                uint64_t virt_addr = HEAP_VIRT_START + (start_bit * BLOCK_SIZE);
                *(uint64_t*)virt_addr = blocks; // Write header
                return (void*)(virt_addr + BLOCK_SIZE);
            }
        } else {
            contiguous_found = 0; // Reset search
        }
    }
    return NULL; // Out of memory
}

void kfree(void *p) {
    if (!p) return;
    uint64_t virt_addr = (uint64_t)p - BLOCK_SIZE;
    uint64_t* header = (uint64_t*)virt_addr;
    uint64_t blocks_free = *header;
    uint64_t bit_idx = (virt_addr - HEAP_VIRT_START) / BLOCK_SIZE;

    for (uint64_t i = 0; i < blocks_free; i++) {
        pmm_free_page(bit_idx + i, heap_bitmap);
    }
}
//------------------------------------HEAP------------------------------------------

void init_bitmap_pmm(struct limine_memmap_response *map) {
    bitmap_length = get_bitmap_length(map, PAGE_TO_QWORD); // bit map
    bitmap_pointer(map);
    all_bitmap_1();
    set_useable_ram(map);
    protect_bitmap_space();
    pmm_lock_page(0, bitmap); // just for safety and debugging
    heap_bitmap_pointer();
}