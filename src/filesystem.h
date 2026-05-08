#pragma once
#include "common.h"

#define EOF                            0x0FFFFFF8
//#define FAT32_BASE_ADDRESS(fat)        (uint64_t*)(fat->address)

struct fat32_bpb {
    uint8_t  jmp_boot[3];                   // Offset 0-2 
    char     oem_name[8];                   //  3-10
    uint16_t bytes_per_sector;              //  11-12 (512)  
    uint8_t  sectors_per_cluster;           //  13
    uint16_t reserved_sector_count;         //  14-15  
    uint8_t  num_fats;                      //  16 (2)
    uint16_t root_entry_count;              //  17-18 (0 for FAT32)
    uint16_t total_sectors_16;              //  19-20 (actually in offset 32)
    uint8_t  media_type;                    //  21
    uint16_t sectors_per_fat_16;            //  22-23 (actually in offset 36)
    uint16_t sectors_per_track;             //  24-25
    uint16_t num_heads;                     //  26-27
    uint32_t hidden_sectors;                //  28-31 (0)
    uint32_t total_sectors_32;               //  32-35
    // ===== FAT32-SPECIFIC FIELDS =====
    uint32_t sectors_per_fat_32;            //  36-39x
    uint16_t extended_flags;                //  40-41
    uint16_t filesystem_version;            //  42-43
    uint32_t root_cluster;                  //  44-47 (2)
    uint16_t fs_info;                       //  48-49 (1)
    uint16_t backup_boot_sector;            //  50-51 (6)
    uint8_t  reserved[12];                  //  52-63
    uint8_t  drive_number;                  //  64
    uint8_t  reserved1;                     //  65
    uint8_t  boot_signature;                //  66 (0x29)
    uint32_t volume_id;                     //  67-70: Volume serial number
    char     volume_label[11];              //  71-81: Volume label (11 bytes, padded with spaces)
    char     filesystem_type[8];            //  82-89: Filesystem type string ("FAT32   ")
    // boot code 90-512
    
} __attribute__((packed));

struct fat32_context{
    void*     base_address;
    size_t    image_size;
    struct fat32_bpb*    bpb;
    uint32_t* fat_table;
    void*     data_region;
    uint32_t  cluster_size;
    bool      initialized;
} __attribute__((packed));

typedef struct fat32_bpb bpb_t;
typedef struct fat32_context fat32_context_t;

void init_FS(void);

fat32_context_t* get_fat32_context(void);

static inline uint32_t fat32_get_next_cluster(fat32_context_t* ctx, uint32_t cluster) {
    if (!ctx || !ctx->initialized) return EOF;
    // only lower 28 bits are used
    return ctx->fat_table[cluster] & 0x0FFFFFFF;
}

static inline void* fat32_get_cluster_address(fat32_context_t* ctx, uint32_t cluster) {
    if (!ctx || !ctx->initialized || cluster < 2) return NULL;
    // Cluster 2 is at offset 0 in the data region
    uint32_t offset = (cluster - 2) * ctx->cluster_size;
    return (void*)((uint8_t*)ctx->data_region + offset);
}

static inline bool fat32_is_eof(uint32_t cluster) {
    return cluster >= 0x0FFFFFF8;
}

static inline bool fat32_is_free(uint32_t cluster) {
    return cluster == 0x00000000;
}

static inline bool fat32_is_bad(uint32_t cluster) {
    return cluster == 0x0FFFFFF7;
}

// Example function: Demonstrate reading a cluster chain
void fat32_read_cluster_chain(uint32_t start_cluster);