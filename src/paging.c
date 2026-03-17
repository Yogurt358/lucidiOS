#include "common.h"
#include <stdint.h>

// Flags for the Page Table Entries
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_PCD      (1ULL << 4)

static uint64_t next_free_frame = 0;

// Simple PMM: Find the first usable big block of RAM
void init_pmm(struct limine_memmap_response *map) {
    for (uint64_t i = 0; i < map->entry_count; i++) {
        if (map->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            next_free_frame = map->entries[i]->base;
            return;
        }
    }
}

// Gives us a fresh 4KB of physical RAM
uint64_t pmm_alloc() {
    uint64_t frame = next_free_frame;
    next_free_frame += 4096;
    return frame;
}

// The VMM: Builds the "Bridge" between Virtual and Physical
void map_page(uint64_t virt, uint64_t phys, uint64_t flags, uint64_t hhdm) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    // Level 4: PML4
    uint64_t* pml4 = (uint64_t*)((cr3 & ~0xFFFULL) + hhdm);
    uint16_t pml4_idx = (virt >> 39) & 0x1FF;

    // Level 3: PDPT
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t new_table = pmm_alloc();
        memset((void*)(new_table + hhdm), 0, 4096);
        pml4[pml4_idx] = new_table | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & ~0xFFFULL) + hhdm);
    uint16_t pdpt_idx = (virt >> 30) & 0x1FF;

    // Level 2: Page Directory
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t new_table = pmm_alloc();
        memset((void*)(new_table + hhdm), 0, 4096);
        pdpt[pdpt_idx] = new_table | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + hhdm);
    uint16_t pd_idx = (virt >> 21) & 0x1FF;

    // Level 1: Page Table
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t new_table = pmm_alloc();
        memset((void*)(new_table + hhdm), 0, 4096);
        pd[pd_idx] = new_table | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + hhdm);
    uint16_t pt_idx = (virt >> 12) & 0x1FF;

    // INJECT: Link the virtual index to the physical hardware address
    pt[pt_idx] = (phys & ~0xFFFULL) | flags;

    // Flush the TLB (CPU cache for paging)
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}