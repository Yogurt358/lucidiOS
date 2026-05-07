#include "filesystem.h"

void init_FS(void) {
    bpb_t* boot = (bpb_t*)(FAT32_BASE_ADDRESS);
    
    uint8_t ratio = boot->sectors_per_cluster;
}