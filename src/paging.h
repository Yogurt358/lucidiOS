#pragma once
#include "common.h"

#define PTE_MASK 0x000FFFFFFFFFF000

void init_pmm(struct limine_memmap_response *map);
void map_page(uint64_t virt, uint64_t phys, uint64_t flags, uint64_t hhdm);
