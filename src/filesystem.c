#include "filesystem.h"
#include "terminal.h"

extern struct limine_file *fat32;

// Global FAT32 context
static fat32_context_t g_fat32_context;

void init_FS(void) {
    bpb_t *bpb = (bpb_t*)(fat32->address);

    // Calculate important offsets
    // Reserved region size in bytes
    uint32_t reserved_region_size = bpb->reserved_sector_count * bpb->bytes_per_sector;
    uint32_t fat_offset = reserved_region_size;
    uint32_t data_offset = reserved_region_size +
                          (bpb->num_fats * bpb->sectors_per_fat_32 * bpb->bytes_per_sector);
    uint32_t cluster_size_bytes = bpb->sectors_per_cluster * bpb->bytes_per_sector;

    // Initialize the global FAT32 context
    g_fat32_context.base_address = fat32->address;
    g_fat32_context.image_size = fat32->size;
    g_fat32_context.bpb = bpb;
    g_fat32_context.fat_table = (uint32_t*)((uint8_t*)fat32->address + fat_offset);
    g_fat32_context.data_region = (void*)((uint8_t*)fat32->address + data_offset);
    g_fat32_context.cluster_size = cluster_size_bytes;
    g_fat32_context.initialized = true;

    kprintf("FAT32 Filesystem Initialized:\n");
    kprintf("  OEM Name: %.8s\n", bpb->oem_name);
    kprintf("  Filesystem: %.8s\n", bpb->filesystem_type);
    kprintf("  Bytes/Sector: %d\n", bpb->bytes_per_sector);
    kprintf("  Sectors/Cluster: %d\n", bpb->sectors_per_cluster);
    kprintf("  Cluster Size: %d bytes\n", cluster_size_bytes);
    kprintf("  Reserved Sectors: %d\n", bpb->reserved_sector_count);
    kprintf("  Number of FATs: %d\n", bpb->num_fats);
    kprintf("  Sectors per FAT: %d\n", bpb->sectors_per_fat_32);
    kprintf("  Root Cluster: %d\n", bpb->root_cluster);
    kprintf("  FAT Table at offset: 0x%x\n", fat_offset);
    kprintf("  Data Region at offset: 0x%x\n", data_offset);
}

fat32_context_t* get_fat32_context(void) {
    return &g_fat32_context;
}

void fat32_read_cluster_chain(uint32_t start_cluster) {
    fat32_context_t* ctx = get_fat32_context();
    if (!ctx || !ctx->initialized) {
        kprintf("FAT32 not initialized!\n");
        return;
    }

    kprintf("\nFollowing cluster chain starting at cluster %d:\n", start_cluster);

    uint32_t current_cluster = start_cluster;
    int chain_length = 0;

    while (!fat32_is_eof(current_cluster) && chain_length < 100) {
        kprintf("  Cluster %d", current_cluster);

        void* cluster_data = fat32_get_cluster_address(ctx, current_cluster);
        if (cluster_data) {
            kprintf(" -> data at %p\n", cluster_data);
        } else {
            kprintf(" -> invalid cluster!\n");
            break;
        }

        // Get next cluster in chain
        uint32_t next = fat32_get_next_cluster(ctx, current_cluster);

        if (fat32_is_eof(next)) {
            kprintf("  End of chain (EOF)\n");
            break;
        } else if (fat32_is_bad(next)) {
            kprintf("  Bad cluster detected!\n");
            break;
        } else if (fat32_is_free(next)) {
            kprintf("  Unexpected free cluster!\n");
            break;
        }

        current_cluster = next;
        chain_length++;
    }

    kprintf("Total clusters in chain: %d\n", chain_length + 1);
}