#pragma once
#include "common.h"

/* =====================================================================
 * FAT32 read/write driver — header
 * ---------------------------------------------------------------------
 * Public API:
 *   File ops:   fat32_open / read / write / seek / close / truncate
 *   Dir ops:    fat32_mkdir / rmdir / unlink / listdir
 *   LFN supported. Full path hierarchy supported.
 *
 * All paths are absolute, slash-separated, e.g. "/docs/notes.txt".
 * Negative return values are FAT_E* error codes (see below).
 * ===================================================================== */

/* ---------- Cluster value semantics ---------- */
#define CLUSTER_FREE      0x00000000u
#define CLUSTER_BAD       0x0FFFFFF7u
#define CLUSTER_EOF_MIN   0x0FFFFFF8u
#define CLUSTER_EOF       0x0FFFFFFFu
#define EOF               0x0FFFFFF8u   /* legacy alias kept for old callers */

/* ---------- Directory entry attribute bits ---------- */
#define ATTR_READ_ONLY    0x01
#define ATTR_HIDDEN       0x02
#define ATTR_SYSTEM       0x04
#define ATTR_VOLUME_ID    0x08
#define ATTR_DIRECTORY    0x10
#define ATTR_ARCHIVE      0x20
#define ATTR_LFN          0x0F   /* LFN slot marker = RO|HID|SYS|VOL */

/* ---------- Dirent name[0] sentinel bytes ---------- */
#define DIRENT_FREE       0xE5   /* deleted entry */
#define DIRENT_END        0x00   /* end of directory contents */
#define DIRENT_KANJI_E5   0x05   /* real first-byte 0xE5 escaped to 0x05 */

/* ---------- Limits ---------- */
#define MAX_OPEN_FILES    32
#define MAX_LFN_SLOTS     20     /* ceil(255 / 13) */
#define MAX_NAME_BYTES    256    /* room for 255 chars + NUL */
#define MAX_PATH_DEPTH    16

/* ---------- Open() flag bits ---------- */
#define O_RDONLY          0x01
#define O_WRONLY          0x02
#define O_RDWR            0x03   /* (O_RDONLY | O_WRONLY) */
#define O_CREATE          0x04
#define O_APPEND          0x08
#define O_TRUNC           0x10

/* ---------- seek() whence ---------- */
#define SEEK_SET          0
#define SEEK_CUR          1
#define SEEK_END          2

/* ---------- Error codes (negative on returns) ---------- */
#define FAT_OK             0
#define FAT_ENOENT        -1   /* no such file or directory */
#define FAT_EEXIST        -2   /* already exists */
#define FAT_ENOSPC        -3   /* no free clusters */
#define FAT_ENOTDIR       -4   /* path component is not a directory */
#define FAT_EISDIR        -5   /* operation invalid on directory */
#define FAT_EBADF         -6   /* invalid file descriptor */
#define FAT_EINVAL        -7   /* invalid argument */
#define FAT_ENAMETOOLONG  -8
#define FAT_ENOTEMPTY     -9   /* directory not empty */
#define FAT_EMFILE        -10  /* too many open files */
#define FAT_EROFS         -11  /* read-only attempt on RO file */

/* ---------- BPB (BIOS Parameter Block) ---------- */
struct fat32_bpb {
    uint8_t  jmp_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t extended_flags;
    uint16_t filesystem_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     filesystem_type[8];
} __attribute__((packed));

/* ---------- Standard SFN (8.3) directory entry ---------- */
struct fat32_dirent {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed));

/* ---------- LFN slot (also 32 bytes, occupies a dirent slot) ---------- */
struct fat32_lfn {
    uint8_t  order;        /* 1..N, with bit 0x40 set on the physically-first slot */
    uint8_t  name1[10];    /* chars 1..5  (UCS-2 LE) -- byte-array to dodge alignment */
    uint8_t  attr;         /* always 0x0F */
    uint8_t  type;         /* always 0    */
    uint8_t  checksum;     /* of associated SFN */
    uint8_t  name2[12];    /* chars 6..11 */
    uint16_t cluster_low;  /* always 0 */
    uint8_t  name3[4];     /* chars 12..13 */
} __attribute__((packed));

/* ---------- FS context (one per mounted volume; we have one) ---------- */
struct fat32_context {
    void*       base_address;     /* start of the FAT image in RAM */
    size_t      image_size;
    struct fat32_bpb* bpb;
    uint32_t*   fat_table;        /* points to FAT #0; #1..#N-1 follow */
    void*       data_region;      /* where cluster #2 begins */
    uint32_t    cluster_size;     /* bytes */
    uint32_t    total_clusters;   /* count of data clusters */
    uint32_t    fat_size_bytes;   /* size of ONE FAT */
    bool        initialized;
};

/* ---------- Open file handle (internal; exposed only as fd index) ---------- */
struct fat32_file {
    bool      in_use;
    bool      is_dir;
    bool      dirty;              /* dirent needs flushing on close */
    uint8_t   flags;              /* O_* */
    uint32_t  first_cluster;      /* 0 if file is empty */
    uint32_t  size;               /* file size in bytes */
    uint32_t  offset;             /* current r/w position */
    uint32_t  parent_cluster;     /* directory containing this file */
    uint32_t  dirent_dir_off;     /* byte offset within parent dir of the SFN slot */
};

typedef struct fat32_bpb     bpb_t;
typedef struct fat32_dirent  dirent_t;
typedef struct fat32_lfn     lfn_t;
typedef struct fat32_context fat32_context_t;
typedef struct fat32_file    fat32_file_t;

/* ---------- Listing callback ---------- */
typedef void (*fat32_ls_cb)(const char* name, uint8_t attr, uint32_t size, void* user);

/* ---------- Public API ---------- */
void              init_FS(void);
fat32_context_t*  get_fat32_context(void);

int  fat32_open      (const char* path, uint8_t flags);
int  fat32_close     (int fd);
int  fat32_read      (int fd, void* buf, uint32_t count);
int  fat32_write     (int fd, const void* buf, uint32_t count);
int  fat32_seek      (int fd, int32_t offset, int whence);
int  fat32_truncate  (int fd, uint32_t length);
int  fat32_tell      (int fd);

int  fat32_mkdir     (const char* path);
int  fat32_rmdir     (const char* path);
int  fat32_unlink    (const char* path);
int  fat32_listdir   (const char* path, fat32_ls_cb cb, void* user);

/* Diagnostic — kept compatible with old caller in main.c */
void fat32_read_cluster_chain(uint32_t start_cluster);

/* ---------- Inline cluster helpers ---------- */
static inline uint32_t fat32_get_next_cluster(fat32_context_t* ctx, uint32_t cluster) {
    if (!ctx || !ctx->initialized) return CLUSTER_EOF;
    return ctx->fat_table[cluster] & 0x0FFFFFFFu;
}

static inline void* fat32_get_cluster_address(fat32_context_t* ctx, uint32_t cluster) {
    if (!ctx || !ctx->initialized || cluster < 2) return NULL;
    if ((cluster - 2) >= ctx->total_clusters) return NULL;
    uint32_t offset = (cluster - 2) * ctx->cluster_size;
    return (void*)((uint8_t*)ctx->data_region + offset);
}

static inline bool fat32_is_eof(uint32_t cluster) {
    return (cluster & 0x0FFFFFFFu) >= CLUSTER_EOF_MIN;
}
static inline bool fat32_is_free(uint32_t cluster) {
    return (cluster & 0x0FFFFFFFu) == CLUSTER_FREE;
}
static inline bool fat32_is_bad(uint32_t cluster) {
    return (cluster & 0x0FFFFFFFu) == CLUSTER_BAD;
}