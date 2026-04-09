#pragma once
#include "common.h"

#define PAGE_SIZE 4096
#define BLOCK_SIZE 32
#define PAGE_TO_BIT 1
#define PAGE_TO_QWORD 64
#define MAXIMUM_PHYSICAL_BITS 40
#define MAXIMUM_VIRTUAL_BITS 48

#define PTE_MASK 0x000FFFFFFFFFF000
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_PCD      (1ULL << 4)

#define read_cr3() ({ \
    uint64_t cr3_val; \
    asm volatile("mov %%cr3, %0" : "=r"(cr3_val) : : "memory"); \
    cr3_val; \
})

struct pte {
uint8_t flags;
uint8_t AVL0:4;
uint32_t next_entry_phys:28; // bits 12 to 39
uint16_t rsvd:12;
uint16_t AVL1_XD:12; // XD is at the final bit (bit 12)

}__attribute__((packed));


typedef struct pte pte_t;

//------------------------------------BITMAP------------------------------------------
bool is_entry_type(size_t type, size_t i, struct limine_memmap_response *map);
uint64_t get_bitmap_length(struct limine_memmap_response *map, int data_type);
void init_bitmap_pmm(struct limine_memmap_response *map);
void set_useable_ram(struct limine_memmap_response *map);
void all_bitmap_1();
void bitmap_pointer(struct limine_memmap_response *map);
void protect_bitmap_space();
//------------------------------------BITMAP------------------------------------------

//------------------------------------PMM------------------------------------------
uint64_t pmm_alloc2();
void pmm_free_page(uint64_t page_idx, uint64_t* bitmap_arg);
void pmm_lock_page(uint64_t page_idx, uint64_t* bitmap_arg);
//------------------------------------PMM------------------------------------------

//------------------------------------VMM------------------------------------------
void vmm_alloc(uint64_t virt, uint64_t phys, uint8_t flags);
//------------------------------------VMM------------------------------------------

//------------------------------------HEAP------------------------------------------
void set_bits(uint64_t page_idx, uint64_t *bitmap_arg, size_t length, size_t sign);
uint64_t create_mask(size_t amount);
size_t list_bits(uint64_t *bitmap_arg, size_t index, size_t amount);
void* kmalloc(uint64_t *bitmap_arg, size_t amount);
void kfree(void *p, size_t amount);
//------------------------------------HEAP------------------------------------------

