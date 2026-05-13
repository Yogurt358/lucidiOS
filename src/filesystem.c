#include "filesystem.h"
#include "terminal.h"

/* `fat32` is the limine module pointer set up in main.c.
 * Note: this driver does NOT call kmalloc/kfree — all scratch buffers
 * are stack-allocated.  That makes integration painless regardless of
 * what your heap allocator is named. */
extern struct limine_file *fat32;

/* =====================================================================
 * MODULE-PRIVATE STATE
 * ===================================================================== */
static fat32_context_t g_ctx;
static fat32_file_t    g_files[MAX_OPEN_FILES];

/* Placeholder timestamps until you wire up the RTC.
 * FAT date  = ((year-1980) << 9) | (month << 5) | day
 * FAT time  = (hour << 11) | (minute << 5) | (second/2)
 * Below = 2026-01-01 00:00:00. */
#define FAT_DATE_DEFAULT  (((2026 - 1980) << 9) | (1 << 5) | 1)
#define FAT_TIME_DEFAULT  0

/* =====================================================================
 * INIT
 * ===================================================================== */
void init_FS(void) {
    bpb_t *bpb = (bpb_t*)(fat32->address);

    uint32_t reserved_bytes = (uint32_t)bpb->reserved_sector_count * bpb->bytes_per_sector;
    uint32_t fat_bytes      = bpb->sectors_per_fat_32 * bpb->bytes_per_sector;
    uint32_t data_offset    = reserved_bytes + (uint32_t)bpb->num_fats * fat_bytes;
    uint32_t cluster_bytes  = (uint32_t)bpb->sectors_per_cluster * bpb->bytes_per_sector;

    uint32_t total_sectors  = bpb->total_sectors_32 ? bpb->total_sectors_32
                                                    : bpb->total_sectors_16;
    uint32_t fat_sectors    = bpb->num_fats * bpb->sectors_per_fat_32;
    uint32_t data_sectors   = total_sectors - (bpb->reserved_sector_count + fat_sectors);
    uint32_t total_clusters = data_sectors / bpb->sectors_per_cluster;

    g_ctx.base_address    = fat32->address;
    g_ctx.image_size      = fat32->size;
    g_ctx.bpb             = bpb;
    g_ctx.fat_table       = (uint32_t*)((uint8_t*)fat32->address + reserved_bytes);
    g_ctx.data_region     = (void*)((uint8_t*)fat32->address + data_offset);
    g_ctx.cluster_size    = cluster_bytes;
    g_ctx.total_clusters  = total_clusters;
    g_ctx.fat_size_bytes  = fat_bytes;
    g_ctx.initialized     = true;

    for (int i = 0; i < MAX_OPEN_FILES; i++) g_files[i].in_use = false;

    /* Copy 8-byte fields into NUL-terminated buffers — your kprintf doesn't
     * support %.Ns precision, so we can't print them directly. */
    char oem[9] = {0}, fs_type[9] = {0};
    memcpy(oem,     bpb->oem_name,        8);
    memcpy(fs_type, bpb->filesystem_type, 8);

    kprintf("FAT32 init OK\n");
    kprintf("  OEM: %s | FS: %s\n", oem, fs_type);
    kprintf("  cluster=%d B, total_clusters=%d, root_cluster=%d\n",
            cluster_bytes, total_clusters, bpb->root_cluster);
    kprintf("  num_fats=%d, fat_size=%d B\n", bpb->num_fats, fat_bytes);
}

fat32_context_t* get_fat32_context(void) { return &g_ctx; }

/* =====================================================================
 * FAT-TABLE OPERATIONS
 * ---------------------------------------------------------------------
 * On a real disk these would issue sector reads/writes. We're a ramdisk,
 * so it's pointer arithmetic.
 *
 * fat_set: writes the value to ALL FAT mirrors (num_fats copies).
 *          Per spec we MUST preserve the top 4 bits of each entry.
 * ===================================================================== */
static void fat_set(uint32_t cluster, uint32_t value) {
    if (cluster >= g_ctx.total_clusters + 2) return;
    uint32_t masked = value & 0x0FFFFFFFu;
    for (uint8_t i = 0; i < g_ctx.bpb->num_fats; i++) {
        uint32_t* fat = (uint32_t*)((uint8_t*)g_ctx.fat_table
                                    + (size_t)i * g_ctx.fat_size_bytes);
        fat[cluster] = (fat[cluster] & 0xF0000000u) | masked;
    }
}

/* Scan FAT linearly for a free entry.  Mark it EOF, zero its data area,
 * return its number.  Returns 0 if disk full (cluster 0 is never valid). */
static uint32_t fat_alloc_cluster(void) {
    for (uint32_t c = 2; c < g_ctx.total_clusters + 2; c++) {
        if ((g_ctx.fat_table[c] & 0x0FFFFFFFu) == 0) {
            fat_set(c, CLUSTER_EOF);
            void* p = fat32_get_cluster_address(&g_ctx, c);
            if (p) memset(p, 0, g_ctx.cluster_size);
            return c;
        }
    }
    return 0;
}

/* Free entire chain starting at `start`. */
static void fat_free_chain(uint32_t start) {
    uint32_t cur = start;
    while (cur >= 2 && !fat32_is_eof(cur)) {
        uint32_t next = fat32_get_next_cluster(&g_ctx, cur);
        fat_set(cur, 0);
        if (fat32_is_free(next)) break;   /* defensive: corrupt chain */
        cur = next;
    }
}

/* Walk to the cluster at logical index `idx` from `start`.
 * If grow==true, allocate-and-link new clusters when chain is too short.
 * Returns 0 on failure (start invalid, disk full, or short chain w/o grow). */
static uint32_t fat_walk_to(uint32_t start, uint32_t idx, bool grow) {
    if (start < 2) return 0;
    uint32_t cur = start;
    for (uint32_t i = 0; i < idx; i++) {
        uint32_t next = fat32_get_next_cluster(&g_ctx, cur);
        if (fat32_is_eof(next)) {
            if (!grow) return 0;
            uint32_t fresh = fat_alloc_cluster();
            if (!fresh) return 0;
            fat_set(cur, fresh);
            cur = fresh;
        } else {
            cur = next;
        }
    }
    return cur;
}

/* Return last cluster in chain (the one whose FAT entry is EOF). */
static uint32_t fat_chain_last(uint32_t start) {
    uint32_t cur = start;
    while (1) {
        uint32_t n = fat32_get_next_cluster(&g_ctx, cur);
        if (fat32_is_eof(n)) return cur;
        cur = n;
    }
}

/* Truncate chain so it has exactly `keep` clusters. Frees the rest.
 * If keep==0, frees the whole chain. Returns the new EOF cluster (or 0). */
static uint32_t fat_truncate_chain(uint32_t start, uint32_t keep) {
    if (start < 2) return 0;
    if (keep == 0) {
        fat_free_chain(start);
        return 0;
    }
    uint32_t cur = start;
    for (uint32_t i = 1; i < keep; i++) {
        uint32_t n = fat32_get_next_cluster(&g_ctx, cur);
        if (fat32_is_eof(n)) return cur;   /* chain already short enough */
        cur = n;
    }
    /* `cur` is now the last cluster we want to keep. Free the tail. */
    uint32_t tail = fat32_get_next_cluster(&g_ctx, cur);
    fat_set(cur, CLUSTER_EOF);
    if (!fat32_is_eof(tail)) fat_free_chain(tail);
    return cur;
}

/* =====================================================================
 * STRING / NAME UTILITIES
 * ===================================================================== */
static size_t k_strlen(const char *s) {
    size_t n = 0; while (s[n]) n++; return n;
}
static int k_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
static char k_toupper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}
static int k_strcasecmp(const char *a, const char *b) {
    while (*a && k_toupper(*a) == k_toupper(*b)) { a++; b++; }
    return (uint8_t)k_toupper(*a) - (uint8_t)k_toupper(*b);
}

/* =====================================================================
 * SFN (8.3) GENERATION + CHECKSUM
 * ---------------------------------------------------------------------
 * Per Microsoft FAT spec:
 *  - SFN is 11 bytes: 8 base + 3 extension, space-padded.
 *  - Stored uppercase. Disallowed bytes get replaced with '_'.
 *  - If the original name doesn't fit the basis, we append "~N" before
 *    the extension and bump N until no collision exists in the parent dir.
 *  - First byte 0xE5 is escaped to 0x05 because 0xE5 marks deletion.
 *  - Checksum used in LFN slots is computed from the 11 raw SFN bytes.
 * ===================================================================== */
static uint8_t lfn_checksum(const uint8_t *sfn) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + sfn[i]);
    return sum;
}

/* Is character valid in an SFN?  Anything else gets replaced with '_'. */
static bool sfn_valid_char(char c) {
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    static const char ok[] = "$%'-_@~`!(){}^#&";
    for (int i = 0; ok[i]; i++) if (c == ok[i]) return true;
    return false;
}

/* Generate the *basis* SFN for a long name, pre-tilde. */
static void sfn_basis(const char *long_name, uint8_t out[11]) {
    memset(out, ' ', 11);

    int len = (int)k_strlen(long_name);
    /* Find LAST '.' -- everything after it is the extension */
    int dot = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (long_name[i] == '.') { dot = i; break; }
    }
    int base_end  = (dot < 0) ? len : dot;
    int ext_start = (dot < 0) ? len : dot + 1;

    /* Skip leading dots and spaces in the basis */
    int start = 0;
    while (start < base_end &&
           (long_name[start] == '.' || long_name[start] == ' ')) start++;

    int o = 0;
    for (int i = start; i < base_end && o < 8; i++) {
        char c = long_name[i];
        if (c == '.' || c == ' ') continue;          /* strip embedded */
        c = k_toupper(c);
        out[o++] = sfn_valid_char(c) ? (uint8_t)c : (uint8_t)'_';
    }

    o = 8;
    for (int i = ext_start; i < len && o < 11; i++) {
        char c = long_name[i];
        if (c == '.' || c == ' ') continue;
        c = k_toupper(c);
        out[o++] = sfn_valid_char(c) ? (uint8_t)c : (uint8_t)'_';
    }

    if (out[0] == 0xE5) out[0] = DIRENT_KANJI_E5;
    if (out[0] == 0x00) out[0] = (uint8_t)'_';        /* defensive */
}

/* Apply a "~N" suffix in the basis half of an SFN.
 * E.g. "MYDOCUMENT.TXT" + N=1 -> "MYDOCU~1.TXT" (stored as "MYDOCU~1TXT" 11B). */
static void sfn_apply_tilde(uint8_t sfn[11], int n) {
    /* Render N as decimal digits (we don't have itoa). */
    char digits[6]; int dlen = 0;
    if (n <= 0) { digits[dlen++] = '0'; }
    else {
        char tmp[6]; int tn = 0; int x = n;
        while (x > 0 && tn < 6) { tmp[tn++] = (char)('0' + (x % 10)); x /= 10; }
        while (tn > 0) digits[dlen++] = tmp[--tn];
    }
    int suffix = 1 + dlen;        /* "~" + digits */

    /* Find current basis length (rtrim spaces from first 8 bytes) */
    int blen = 8;
    while (blen > 0 && sfn[blen-1] == ' ') blen--;

    int new_blen = blen;
    if (new_blen + suffix > 8) new_blen = 8 - suffix;

    sfn[new_blen] = '~';
    for (int i = 0; i < dlen; i++) sfn[new_blen + 1 + i] = (uint8_t)digits[i];
    for (int i = new_blen + suffix; i < 8; i++) sfn[i] = ' ';
}

/* True if two SFNs (raw 11-byte form) are equal. */
static bool sfn_eq(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 11; i++) if (a[i] != b[i]) return false;
    return true;
}

/* Render an 11-byte SFN as a printable "NAME.EXT" (or "NAME") string. */
static void sfn_to_printable(const uint8_t sfn[11], char *out) {
    int o = 0;
    for (int i = 0; i < 8 && sfn[i] != ' '; i++) out[o++] = (char)sfn[i];
    if (sfn[8] != ' ') {
        out[o++] = '.';
        for (int i = 8; i < 11 && sfn[i] != ' '; i++) out[o++] = (char)sfn[i];
    }
    out[o] = '\0';
    if ((uint8_t)out[0] == DIRENT_KANJI_E5) out[0] = (char)0xE5;
}

/* =====================================================================
 * LFN SLOT (DE)CODING
 * ---------------------------------------------------------------------
 * Each LFN slot stores 13 UCS-2 chars: 5 in name1, 6 in name2, 2 in name3.
 * Slots come PHYSICALLY BEFORE their SFN, ordered last-to-first by name
 * position. The physically-first slot has the 0x40 bit set in `order`.
 * Slot order N covers chars [(N-1)*13 .. N*13 - 1].
 * Padding rule: first unused char = 0x0000, the rest = 0xFFFF.
 *
 * We're lossy at the API boundary: UCS-2 -> ASCII, non-ASCII -> '_'.
 * That's fine for a teaching OS; if you ever want UTF-8 here it slots in
 * cleanly at lfn_chars_to_ascii() / ascii_to_lfn_chars().
 * ===================================================================== */
static uint16_t lfn_get_char(const lfn_t *e, int idx) {
    /* idx is 0..12 within the slot. Use byte access — name fields are
     * unaligned because the struct is packed. */
    const uint8_t *p;
    int off;
    if (idx < 5)        { p = e->name1; off = idx * 2; }
    else if (idx < 11)  { p = e->name2; off = (idx - 5) * 2; }
    else                { p = e->name3; off = (idx - 11) * 2; }
    return (uint16_t)p[off] | ((uint16_t)p[off+1] << 8);
}
static void lfn_set_char(lfn_t *e, int idx, uint16_t ch) {
    uint8_t *p; int off;
    if (idx < 5)        { p = e->name1; off = idx * 2; }
    else if (idx < 11)  { p = e->name2; off = (idx - 5) * 2; }
    else                { p = e->name3; off = (idx - 11) * 2; }
    p[off]     = (uint8_t)(ch & 0xFF);
    p[off + 1] = (uint8_t)(ch >> 8);
}

/* Convert assembled UCS-2 buffer (length in chars) to ASCII C-string. */
static void lfn_chars_to_ascii(const uint16_t *in, size_t in_len,
                               char *out, size_t out_max) {
    size_t k = 0;
    for (size_t i = 0; i < in_len && k + 1 < out_max; i++) {
        uint16_t c = in[i];
        out[k++] = (c < 128) ? (char)c : '_';
    }
    out[k] = '\0';
}

/* =====================================================================
 * DIRECTORY-LEVEL RANDOM I/O
 * ---------------------------------------------------------------------
 * Directories are cluster chains.  The k-th 32-byte slot lives in cluster
 * (root + k / entries_per_cluster), at byte offset (k % entries_per_cluster)*32.
 * Helpers below let us read/write a slot by linear index, growing the
 * directory's chain on demand for writes.
 * ===================================================================== */
static uint32_t dir_entries_per_cluster(void) {
    return g_ctx.cluster_size / 32;
}

/* Returns pointer to slot or NULL if past end (and grow==false). */
static void* dir_slot_ptr(uint32_t dir_first_cluster, uint32_t slot_idx,
                          bool grow) {
    uint32_t per_cluster = dir_entries_per_cluster();
    uint32_t cluster_idx = slot_idx / per_cluster;
    uint32_t in_cluster  = slot_idx % per_cluster;
    uint32_t cluster     = fat_walk_to(dir_first_cluster, cluster_idx, grow);
    if (!cluster) return NULL;
    uint8_t *base = (uint8_t*)fat32_get_cluster_address(&g_ctx, cluster);
    if (!base) return NULL;
    return base + (size_t)in_cluster * 32;
}

/* =====================================================================
 * DIRECTORY ITERATION
 * ---------------------------------------------------------------------
 * Yields one logical entry at a time. A "logical entry" is the SFN slot
 * plus any contiguous LFN slots that precede it. Skips deleted slots,
 * volume-label entries, and orphaned LFNs.  Stops at the 0x00 sentinel.
 * ===================================================================== */
typedef struct {
    char     name[MAX_NAME_BYTES];   /* assembled long name (or 8.3 if no LFN) */
    uint8_t  attr;
    uint32_t first_cluster;
    uint32_t size;
    uint32_t sfn_slot_idx;           /* linear slot index of the SFN slot */
    uint32_t total_slots;            /* number of consecutive slots used (LFNs + 1 SFN) */
    bool     end_of_dir;             /* true when we hit 0x00 sentinel */
} dir_entry_info_t;

/* Read one logical entry starting at *cursor. On success, advances *cursor
 * past the entry and fills out info. If end-of-dir reached, sets info->end_of_dir.
 * Returns true while iteration may continue, false on hard end. */
static bool dir_iter_next(uint32_t dir_first_cluster,
                          uint32_t *cursor,
                          dir_entry_info_t *info) {
    info->name[0]      = '\0';
    info->total_slots  = 0;
    info->end_of_dir   = false;

    /* Buffer of LFN slot copies — we collect them physically (top-down)
     * then place each slot's chars at the correct position via order field. */
    lfn_t   lfn_buf[MAX_LFN_SLOTS];
    int     lfn_count = 0;
    uint32_t first_slot_idx = *cursor;
    uint8_t expected_checksum = 0;

    while (1) {
        void* sp = dir_slot_ptr(dir_first_cluster, *cursor, false);
        if (!sp) {
            /* Off the end of the allocated chain without seeing 0x00.
             * Treat as end-of-dir. */
            info->end_of_dir = true;
            return false;
        }
        uint8_t *raw  = (uint8_t*)sp;
        uint8_t  b0   = raw[0];
        uint8_t  attr = raw[11];

        if (b0 == DIRENT_END) {
            info->end_of_dir = true;
            return false;
        }
        if (b0 == DIRENT_FREE) {
            /* Deleted slot — discard any LFN buffer we were collecting. */
            lfn_count = 0;
            (*cursor)++;
            first_slot_idx = *cursor;
            continue;
        }

        if (attr == ATTR_LFN) {
            lfn_t *e = (lfn_t*)sp;
            if (lfn_count == 0) {
                /* First LFN slot we see — must have 0x40 bit set (it's
                 * physically first, name-wise last). */
                if ((e->order & 0x40) == 0) {
                    /* Orphan — skip it. */
                    (*cursor)++;
                    first_slot_idx = *cursor;
                    continue;
                }
                expected_checksum = e->checksum;
                first_slot_idx = *cursor;
            } else {
                /* Subsequent slots must share the same checksum and
                 * have decreasing `order & 0x3F`. */
                if (e->checksum != expected_checksum) {
                    /* Inconsistent chain — discard and resync. */
                    lfn_count = 0;
                    first_slot_idx = *cursor;
                    continue;
                }
            }
            if (lfn_count < MAX_LFN_SLOTS) {
                memcpy(&lfn_buf[lfn_count], sp, sizeof(lfn_t));
                lfn_count++;
            } else {
                /* Too many LFN slots — give up on this name */
                lfn_count = 0;
            }
            (*cursor)++;
            continue;
        }

        /* Regular SFN slot. */
        if (attr & ATTR_VOLUME_ID) {
            /* Skip volume-label entries entirely. */
            lfn_count = 0;
            (*cursor)++;
            first_slot_idx = *cursor;
            continue;
        }

        dirent_t *de = (dirent_t*)sp;

        /* If we collected LFN slots, validate checksum against this SFN. */
        char assembled[MAX_NAME_BYTES] = {0};
        bool used_lfn = false;
        if (lfn_count > 0 && lfn_checksum(de->name) == expected_checksum) {
            /* Place chars from each slot.  Slot with order=K covers
             * chars [(K-1)*13 .. K*13-1]. */
            uint16_t chars[MAX_LFN_SLOTS * 13];
            size_t   max_pos = 0;
            for (int i = 0; i < lfn_count; i++) {
                int order = lfn_buf[i].order & 0x3F;
                if (order < 1 || order > MAX_LFN_SLOTS) continue;
                size_t base = (size_t)(order - 1) * 13;
                for (int k = 0; k < 13; k++) {
                    chars[base + k] = lfn_get_char(&lfn_buf[i], k);
                    if (base + k + 1 > max_pos) max_pos = base + k + 1;
                }
            }
            /* Trim at first 0x0000 terminator */
            size_t real_len = 0;
            while (real_len < max_pos && chars[real_len] != 0x0000
                   && chars[real_len] != 0xFFFF) real_len++;
            lfn_chars_to_ascii(chars, real_len, assembled, sizeof(assembled));
            used_lfn = true;
        }

        if (!used_lfn) {
            /* Fall back to SFN as the name. */
            sfn_to_printable(de->name, assembled);
        }

        /* Fill out the result struct. */
        size_t cpy = k_strlen(assembled);
        if (cpy > MAX_NAME_BYTES - 1) cpy = MAX_NAME_BYTES - 1;
        memcpy(info->name, assembled, cpy);
        info->name[cpy] = '\0';

        info->attr          = de->attr;
        info->first_cluster = ((uint32_t)de->cluster_high << 16) | de->cluster_low;
        info->size          = de->file_size;
        info->sfn_slot_idx  = *cursor;
        info->total_slots   = (*cursor - first_slot_idx) + 1;

        (*cursor)++;
        return true;
    }
}

/* =====================================================================
 * DIRECTORY MUTATION: SLOT ALLOCATION + ENTRY CREATION
 * ---------------------------------------------------------------------
 * To add a new file/dir we need N consecutive free slots in the parent
 * directory: 1 SFN + ceil(name_len / 13) LFN slots. We linearly scan the
 * directory, treating DIRENT_END (0x00) and DIRENT_FREE (0xE5) as free.
 * On EOC of the chain we extend the directory by another cluster.
 *
 * After writing N entries we MUST place a fresh DIRENT_END sentinel at
 * slot+N if we overwrote the previous 0x00.
 * ===================================================================== */

/* Find a run of `n` consecutive free slots in directory `dir_first_cluster`.
 * Allocates new clusters if needed.  Returns the linear slot index of the
 * START of the run, or 0xFFFFFFFF on failure. */
static uint32_t dir_find_free_run(uint32_t dir_first_cluster, uint32_t n) {
    uint32_t run_start = 0;
    uint32_t run_len   = 0;
    uint32_t idx       = 0;

    /* Phase 1: scan existing dir entries. Stop at first DIRENT_END — we
     * can extend past it, treating all remaining slots as free. */
    while (1) {
        void *sp = dir_slot_ptr(dir_first_cluster, idx, false);
        if (!sp) break;                       /* end of allocated chain */
        uint8_t b0 = ((uint8_t*)sp)[0];
        if (b0 == DIRENT_END) break;
        if (b0 == DIRENT_FREE) {
            if (run_len == 0) run_start = idx;
            run_len++;
            if (run_len >= n) return run_start;
        } else {
            run_len = 0;
        }
        idx++;
    }

    /* Phase 2: from idx onwards, all slots are free (incl. the sentinel
     * we'll re-place after writing). Possibly we already accumulated some
     * trailing free run before DIRENT_END; merge it. */
    if (run_len == 0) run_start = idx;
    /* Make sure we have enough physical slots; grow chain as needed. */
    uint32_t needed_until = run_start + n;
    void *probe = dir_slot_ptr(dir_first_cluster, needed_until, true);
    if (!probe) return 0xFFFFFFFFu;            /* disk full while extending */
    /* Zero the slots we're about to use (so DIRENT_END appears beyond them). */
    for (uint32_t k = run_start; k <= needed_until; k++) {
        void *p = dir_slot_ptr(dir_first_cluster, k, true);
        if (p) memset(p, 0, 32);
    }
    return run_start;
}

/* Decide how many LFN slots a name needs.  Pure ASCII path; UCS-2 only
 * matters on disk. We treat 1 ASCII char == 1 UCS-2 char. */
static uint32_t lfn_slots_needed(const char *name) {
    size_t len = k_strlen(name);
    if (len == 0) return 0;
    return (uint32_t)((len + 12) / 13);
}

/* Look up a name in `dir_first_cluster`. Case-insensitive match against
 * either the assembled long name or the printable SFN.
 * On success fills out info and returns FAT_OK; otherwise FAT_ENOENT. */
static int dir_lookup(uint32_t dir_first_cluster, const char *name,
                      dir_entry_info_t *out) {
    uint32_t cursor = 0;
    while (1) {
        dir_entry_info_t info;
        if (!dir_iter_next(dir_first_cluster, &cursor, &info)) {
            return FAT_ENOENT;
        }
        if (k_strcasecmp(info.name, name) == 0) {
            *out = info;
            return FAT_OK;
        }
    }
}

/* Check whether a raw SFN is currently used in `dir_first_cluster`. */
static bool dir_sfn_collides(uint32_t dir_first_cluster, const uint8_t sfn[11]) {
    uint32_t cursor = 0;
    while (1) {
        void *sp = dir_slot_ptr(dir_first_cluster, cursor, false);
        if (!sp) return false;
        uint8_t b0 = ((uint8_t*)sp)[0];
        if (b0 == DIRENT_END) return false;
        if (b0 == DIRENT_FREE) { cursor++; continue; }
        uint8_t attr = ((uint8_t*)sp)[11];
        if (attr == ATTR_LFN || (attr & ATTR_VOLUME_ID)) {
            cursor++; continue;
        }
        if (sfn_eq(((dirent_t*)sp)->name, sfn)) return true;
        cursor++;
    }
}

/* Generate an SFN that doesn't collide in this directory.
 * Output sfn[11] is the final on-disk form. */
static void dir_make_unique_sfn(uint32_t dir_first_cluster,
                                const char *long_name,
                                uint8_t sfn[11]) {
    sfn_basis(long_name, sfn);
    /* Always apply ~1 if the long name doesn't already cleanly fit;
     * for simplicity we always tilde-suffix when name length > 11
     * or contains lowercase / disallowed chars.  Easier to just always
     * try: if no collision with basis form alone, accept it. */
    if (!dir_sfn_collides(dir_first_cluster, sfn)) return;

    for (int n = 1; n < 1000000; n++) {
        uint8_t cand[11];
        sfn_basis(long_name, cand);
        sfn_apply_tilde(cand, n);
        if (!dir_sfn_collides(dir_first_cluster, cand)) {
            memcpy(sfn, cand, 11);
            return;
        }
    }
    /* Catastrophic — caller will get a colliding SFN; vanishingly unlikely. */
}

/* Write a complete entry (LFN slots + SFN) into `dir_first_cluster`.
 * Fills the dirent metadata: first_cluster, size, attr.
 * Returns the slot index of the SFN on success, or 0xFFFFFFFF on failure. */
static uint32_t dir_write_entry(uint32_t dir_first_cluster,
                                const char *long_name,
                                uint8_t   attr,
                                uint32_t  first_cluster,
                                uint32_t  size) {
    uint32_t lfn_n   = lfn_slots_needed(long_name);
    uint32_t total_n = lfn_n + 1;

    uint32_t start = dir_find_free_run(dir_first_cluster, total_n);
    if (start == 0xFFFFFFFFu) return 0xFFFFFFFFu;

    /* Pick a non-colliding SFN. */
    uint8_t sfn[11];
    dir_make_unique_sfn(dir_first_cluster, long_name, sfn);
    uint8_t cksum = lfn_checksum(sfn);

    /* Build name as UCS-2, NUL-terminated. */
    size_t   name_len = k_strlen(long_name);
    uint16_t ucs[MAX_LFN_SLOTS * 13 + 1];
    for (size_t i = 0; i < name_len; i++) ucs[i] = (uint16_t)(uint8_t)long_name[i];
    ucs[name_len] = 0x0000;

    /* Write LFN slots: physically first slot (lowest index) carries the
     * highest order number (= lfn_n) and the 0x40 bit.  Slot K covers
     * chars [(K-1)*13 .. K*13 - 1]. */
    for (uint32_t i = 0; i < lfn_n; i++) {
        uint32_t order = lfn_n - i;     /* physical-first gets highest order */
        lfn_t *slot = (lfn_t*)dir_slot_ptr(dir_first_cluster, start + i, false);
        if (!slot) return 0xFFFFFFFFu;
        memset(slot, 0, sizeof(*slot));
        slot->order    = (uint8_t)(order | (i == 0 ? 0x40 : 0x00));
        slot->attr     = ATTR_LFN;
        slot->type     = 0;
        slot->checksum = cksum;
        slot->cluster_low = 0;

        /* This slot covers chars [base .. base+12]. */
        size_t base = (size_t)(order - 1) * 13;
        bool   seen_terminator = false;
        for (int k = 0; k < 13; k++) {
            uint16_t ch;
            if (seen_terminator) {
                ch = 0xFFFF;
            } else if (base + k < name_len) {
                ch = ucs[base + k];
            } else if (base + k == name_len) {
                ch = 0x0000;             /* NUL terminator */
                seen_terminator = true;
            } else {
                ch = 0xFFFF;
                seen_terminator = true;
            }
            lfn_set_char(slot, k, ch);
        }
    }

    /* Write SFN. */
    dirent_t *sde = (dirent_t*)dir_slot_ptr(dir_first_cluster, start + lfn_n, false);
    if (!sde) return 0xFFFFFFFFu;
    memset(sde, 0, sizeof(*sde));
    memcpy(sde->name, sfn, 11);
    sde->attr          = attr;
    sde->cluster_high  = (uint16_t)(first_cluster >> 16);
    sde->cluster_low   = (uint16_t)(first_cluster & 0xFFFF);
    sde->file_size     = size;
    sde->create_date = sde->modify_date = sde->access_date = FAT_DATE_DEFAULT;
    sde->create_time = sde->modify_time = FAT_TIME_DEFAULT;

    /* Make sure DIRENT_END sentinel exists past our last slot.
     * dir_find_free_run already zeroed slot[start+total_n], so we're good. */

    return start + lfn_n;
}

/* Mark an entry (SFN + preceding LFN slots) as deleted (0xE5). */
static void dir_remove_entry(uint32_t dir_first_cluster,
                             uint32_t sfn_slot_idx,
                             uint32_t total_slots) {
    uint32_t first = sfn_slot_idx + 1 - total_slots;
    for (uint32_t i = 0; i < total_slots; i++) {
        uint8_t *raw = (uint8_t*)dir_slot_ptr(dir_first_cluster, first + i, false);
        if (raw) raw[0] = DIRENT_FREE;
    }
}

/* Update an existing SFN entry's metadata. */
static void dir_update_dirent(uint32_t dir_first_cluster,
                              uint32_t sfn_slot_idx,
                              uint32_t first_cluster,
                              uint32_t size) {
    dirent_t *de = (dirent_t*)dir_slot_ptr(dir_first_cluster, sfn_slot_idx, false);
    if (!de) return;
    de->cluster_high = (uint16_t)(first_cluster >> 16);
    de->cluster_low  = (uint16_t)(first_cluster & 0xFFFF);
    de->file_size    = size;
    de->modify_date  = FAT_DATE_DEFAULT;
    de->modify_time  = FAT_TIME_DEFAULT;
}

/* =====================================================================
 * PATH RESOLUTION
 * ---------------------------------------------------------------------
 * "/a/b/c.txt" -> resolve_parent fills parent_cluster=cluster of /a/b
 * and basename_out points into the original path at "c.txt".
 * Path must be absolute (begin with '/').
 * ===================================================================== */
static int path_split_segment(const char **p, char *seg_out, size_t max) {
    while (**p == '/') (*p)++;
    if (**p == '\0') return 0;
    size_t i = 0;
    while (**p && **p != '/' && i + 1 < max) {
        seg_out[i++] = **p;
        (*p)++;
    }
    if (**p && **p != '/') return -1;          /* segment too long */
    seg_out[i] = '\0';
    return 1;
}

/* Walks intermediate path components.  Returns parent dir cluster and
 * a pointer (into `path`) to the basename. */
static int path_resolve_parent(const char *path,
                               uint32_t *parent_cluster_out,
                               char *basename_out, size_t basename_max) {
    if (!path || path[0] != '/') return FAT_EINVAL;

    /* Strip trailing slashes, e.g. "/foo/" -> "/foo" */
    size_t plen = k_strlen(path);
    while (plen > 1 && path[plen-1] == '/') plen--;

    /* Find last '/' before plen */
    int last_slash = -1;
    for (size_t i = 0; i < plen; i++) if (path[i] == '/') last_slash = (int)i;
    if (last_slash < 0) return FAT_EINVAL;

    /* basename = path[last_slash+1 .. plen-1] */
    size_t bn_len = plen - (size_t)(last_slash + 1);
    if (bn_len == 0)               return FAT_EINVAL;     /* path was "/" */
    if (bn_len + 1 > basename_max) return FAT_ENAMETOOLONG;
    memcpy(basename_out, path + last_slash + 1, bn_len);
    basename_out[bn_len] = '\0';

    /* Walk from root through path[0 .. last_slash]. */
    uint32_t cur = g_ctx.bpb->root_cluster;
    const char *p = path;

    while (1) {
        while (*p == '/') p++;
        if ((size_t)(p - path) >= (size_t)last_slash + 1) break;

        char seg[MAX_NAME_BYTES];
        size_t i = 0;
        while (*p && *p != '/' && (size_t)(p - path) < (size_t)last_slash + 1
               && i + 1 < sizeof(seg)) {
            seg[i++] = *p++;
        }
        seg[i] = '\0';
        if (i == 0) break;

        dir_entry_info_t info;
        int rc = dir_lookup(cur, seg, &info);
        if (rc != FAT_OK) return rc;
        if (!(info.attr & ATTR_DIRECTORY)) return FAT_ENOTDIR;
        cur = info.first_cluster ? info.first_cluster : g_ctx.bpb->root_cluster;
    }

    *parent_cluster_out = cur;
    return FAT_OK;
}

/* =====================================================================
 * FILE-DESCRIPTOR TABLE
 * ===================================================================== */
static int alloc_fd(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!g_files[i].in_use) return i;
    }
    return FAT_EMFILE;
}

/* =====================================================================
 * PUBLIC API: file ops
 * ===================================================================== */
int fat32_open(const char *path, uint8_t flags) {
    if (!g_ctx.initialized) return FAT_EINVAL;
    if (!path)              return FAT_EINVAL;

    uint32_t parent;
    char basename[MAX_NAME_BYTES];
    int rc = path_resolve_parent(path, &parent, basename, sizeof(basename));
    if (rc != FAT_OK) return rc;

    dir_entry_info_t info;
    rc = dir_lookup(parent, basename, &info);

    if (rc == FAT_ENOENT) {
        if (!(flags & O_CREATE)) return FAT_ENOENT;
        /* Create empty file: dirent with first_cluster=0, size=0 */
        uint32_t slot = dir_write_entry(parent, basename, ATTR_ARCHIVE, 0, 0);
        if (slot == 0xFFFFFFFFu) return FAT_ENOSPC;
        info.first_cluster = 0;
        info.size          = 0;
        info.attr          = ATTR_ARCHIVE;
        info.sfn_slot_idx  = slot;
    } else if (rc != FAT_OK) {
        return rc;
    } else {
        /* Existed already */
        if (info.attr & ATTR_DIRECTORY)  return FAT_EISDIR;
        if ((info.attr & ATTR_READ_ONLY) && (flags & O_WRONLY)) return FAT_EROFS;
        if (flags & O_TRUNC) {
            if (info.first_cluster) fat_free_chain(info.first_cluster);
            info.first_cluster = 0;
            info.size = 0;
            dir_update_dirent(parent, info.sfn_slot_idx, 0, 0);
        }
    }

    int fd = alloc_fd();
    if (fd < 0) return fd;
    fat32_file_t *f = &g_files[fd];
    f->in_use         = true;
    f->is_dir         = false;
    f->dirty          = false;
    f->flags          = flags;
    f->first_cluster  = info.first_cluster;
    f->size           = info.size;
    f->offset         = (flags & O_APPEND) ? info.size : 0;
    f->parent_cluster = parent;
    f->dirent_dir_off = info.sfn_slot_idx;
    return fd;
}

int fat32_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return FAT_EBADF;
    fat32_file_t *f = &g_files[fd];
    if (!f->in_use) return FAT_EBADF;
    if (f->dirty) {
        dir_update_dirent(f->parent_cluster, f->dirent_dir_off,
                          f->first_cluster, f->size);
    }
    f->in_use = false;
    return FAT_OK;
}

int fat32_read(int fd, void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return FAT_EBADF;
    fat32_file_t *f = &g_files[fd];
    if (!f->in_use)               return FAT_EBADF;
    if (!(f->flags & O_RDONLY))   return FAT_EBADF;
    if (f->offset >= f->size)     return 0;

    uint32_t want = count;
    if (f->offset + want > f->size) want = f->size - f->offset;

    uint8_t *dst = (uint8_t*)buf;
    uint32_t left = want;
    while (left > 0) {
        uint32_t cluster_idx = f->offset / g_ctx.cluster_size;
        uint32_t in_cluster  = f->offset % g_ctx.cluster_size;
        uint32_t cluster     = fat_walk_to(f->first_cluster, cluster_idx, false);
        if (!cluster) break;
        uint8_t *p = (uint8_t*)fat32_get_cluster_address(&g_ctx, cluster);
        if (!p) break;
        uint32_t chunk = g_ctx.cluster_size - in_cluster;
        if (chunk > left) chunk = left;
        memcpy(dst, p + in_cluster, chunk);
        dst       += chunk;
        f->offset += chunk;
        left      -= chunk;
    }
    return (int)(want - left);
}

int fat32_write(int fd, const void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return FAT_EBADF;
    fat32_file_t *f = &g_files[fd];
    if (!f->in_use)             return FAT_EBADF;
    if (!(f->flags & O_WRONLY)) return FAT_EBADF;

    if (f->flags & O_APPEND) f->offset = f->size;

    /* If the file has no clusters yet, allocate the first one. */
    if (f->first_cluster == 0 && count > 0) {
        uint32_t c = fat_alloc_cluster();
        if (!c) return FAT_ENOSPC;
        f->first_cluster = c;
        f->dirty = true;
    }

    const uint8_t *src = (const uint8_t*)buf;
    uint32_t left = count;
    while (left > 0) {
        uint32_t cluster_idx = f->offset / g_ctx.cluster_size;
        uint32_t in_cluster  = f->offset % g_ctx.cluster_size;
        /* grow=true so missing clusters get linked on the fly */
        uint32_t cluster     = fat_walk_to(f->first_cluster, cluster_idx, true);
        if (!cluster) {
            if (count - left == 0) return FAT_ENOSPC;
            break;     /* partial write — return what we managed */
        }
        uint8_t *p = (uint8_t*)fat32_get_cluster_address(&g_ctx, cluster);
        if (!p) break;
        uint32_t chunk = g_ctx.cluster_size - in_cluster;
        if (chunk > left) chunk = left;
        memcpy(p + in_cluster, src, chunk);
        src       += chunk;
        f->offset += chunk;
        left      -= chunk;
    }

    uint32_t wrote = count - left;
    if (f->offset > f->size) f->size = f->offset;
    f->dirty = true;
    return (int)wrote;
}

int fat32_seek(int fd, int32_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return FAT_EBADF;
    fat32_file_t *f = &g_files[fd];
    if (!f->in_use) return FAT_EBADF;

    int64_t base;
    switch (whence) {
        case SEEK_SET: base = 0;            break;
        case SEEK_CUR: base = (int64_t)f->offset; break;
        case SEEK_END: base = (int64_t)f->size;   break;
        default:       return FAT_EINVAL;
    }
    int64_t newpos = base + offset;
    if (newpos < 0) return FAT_EINVAL;
    f->offset = (uint32_t)newpos;
    return (int)f->offset;
}

int fat32_tell(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return FAT_EBADF;
    if (!g_files[fd].in_use) return FAT_EBADF;
    return (int)g_files[fd].offset;
}

int fat32_truncate(int fd, uint32_t length) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return FAT_EBADF;
    fat32_file_t *f = &g_files[fd];
    if (!f->in_use) return FAT_EBADF;
    if (!(f->flags & O_WRONLY)) return FAT_EBADF;

    if (length == 0) {
        if (f->first_cluster) fat_free_chain(f->first_cluster);
        f->first_cluster = 0;
    } else {
        uint32_t need = (length + g_ctx.cluster_size - 1) / g_ctx.cluster_size;
        if (f->first_cluster == 0) {
            f->first_cluster = fat_alloc_cluster();
            if (!f->first_cluster) return FAT_ENOSPC;
        }
        /* Grow if shorter than required */
        uint32_t last = fat_walk_to(f->first_cluster, need - 1, true);
        if (!last) return FAT_ENOSPC;
        /* Shrink if longer */
        fat_truncate_chain(f->first_cluster, need);
    }
    f->size  = length;
    f->dirty = true;
    if (f->offset > length) f->offset = length;
    return FAT_OK;
}

/* =====================================================================
 * PUBLIC API: directory ops
 * ===================================================================== */
int fat32_unlink(const char *path) {
    if (!g_ctx.initialized) return FAT_EINVAL;
    uint32_t parent;
    char basename[MAX_NAME_BYTES];
    int rc = path_resolve_parent(path, &parent, basename, sizeof(basename));
    if (rc != FAT_OK) return rc;

    dir_entry_info_t info;
    rc = dir_lookup(parent, basename, &info);
    if (rc != FAT_OK) return rc;
    if (info.attr & ATTR_DIRECTORY) return FAT_EISDIR;

    /* Make sure no fd has this file open (would cause UAF on the chain). */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (g_files[i].in_use &&
            g_files[i].parent_cluster == parent &&
            g_files[i].dirent_dir_off == info.sfn_slot_idx)
            return FAT_EINVAL;       /* "busy" — close it first */
    }

    if (info.first_cluster) fat_free_chain(info.first_cluster);
    dir_remove_entry(parent, info.sfn_slot_idx, info.total_slots);
    return FAT_OK;
}

int fat32_mkdir(const char *path) {
    if (!g_ctx.initialized) return FAT_EINVAL;
    uint32_t parent;
    char basename[MAX_NAME_BYTES];
    int rc = path_resolve_parent(path, &parent, basename, sizeof(basename));
    if (rc != FAT_OK) return rc;

    dir_entry_info_t tmp;
    if (dir_lookup(parent, basename, &tmp) == FAT_OK) return FAT_EEXIST;

    /* Allocate cluster for new directory */
    uint32_t new_cluster = fat_alloc_cluster();
    if (!new_cluster) return FAT_ENOSPC;

    /* Write '.' (points to self) and '..' (points to parent — if parent
     * is the root cluster, the FAT spec says '..' must store 0, not the
     * actual root cluster number). */
    dirent_t *slots = (dirent_t*)fat32_get_cluster_address(&g_ctx, new_cluster);
    if (!slots) { fat_free_chain(new_cluster); return FAT_EINVAL; }
    memset(slots, 0, g_ctx.cluster_size);

    memcpy(slots[0].name, ".          ", 11);
    slots[0].attr         = ATTR_DIRECTORY;
    slots[0].cluster_high = (uint16_t)(new_cluster >> 16);
    slots[0].cluster_low  = (uint16_t)(new_cluster & 0xFFFF);
    slots[0].create_date = slots[0].modify_date = slots[0].access_date = FAT_DATE_DEFAULT;

    uint32_t parent_for_dotdot = (parent == g_ctx.bpb->root_cluster) ? 0 : parent;
    memcpy(slots[1].name, "..         ", 11);
    slots[1].attr         = ATTR_DIRECTORY;
    slots[1].cluster_high = (uint16_t)(parent_for_dotdot >> 16);
    slots[1].cluster_low  = (uint16_t)(parent_for_dotdot & 0xFFFF);
    slots[1].create_date = slots[1].modify_date = slots[1].access_date = FAT_DATE_DEFAULT;

    /* Add the entry to parent dir */
    uint32_t slot = dir_write_entry(parent, basename, ATTR_DIRECTORY, new_cluster, 0);
    if (slot == 0xFFFFFFFFu) {
        fat_free_chain(new_cluster);
        return FAT_ENOSPC;
    }
    return FAT_OK;
}

int fat32_rmdir(const char *path) {
    if (!g_ctx.initialized) return FAT_EINVAL;
    uint32_t parent;
    char basename[MAX_NAME_BYTES];
    int rc = path_resolve_parent(path, &parent, basename, sizeof(basename));
    if (rc != FAT_OK) return rc;

    dir_entry_info_t info;
    rc = dir_lookup(parent, basename, &info);
    if (rc != FAT_OK) return rc;
    if (!(info.attr & ATTR_DIRECTORY)) return FAT_ENOTDIR;
    if (info.first_cluster == 0)       return FAT_EINVAL;

    /* Verify directory contains only "." and ".." */
    uint32_t cursor = 0;
    while (1) {
        dir_entry_info_t e;
        if (!dir_iter_next(info.first_cluster, &cursor, &e)) break;
        if (k_strcmp(e.name, ".") == 0 || k_strcmp(e.name, "..") == 0) continue;
        return FAT_ENOTEMPTY;
    }

    fat_free_chain(info.first_cluster);
    dir_remove_entry(parent, info.sfn_slot_idx, info.total_slots);
    return FAT_OK;
}

int fat32_listdir(const char *path, fat32_ls_cb cb, void *user) {
    if (!g_ctx.initialized) return FAT_EINVAL;

    uint32_t dir_cluster;
    if (k_strcmp(path, "/") == 0) {
        dir_cluster = g_ctx.bpb->root_cluster;
    } else {
        uint32_t parent;
        char basename[MAX_NAME_BYTES];
        int rc = path_resolve_parent(path, &parent, basename, sizeof(basename));
        if (rc != FAT_OK) return rc;
        dir_entry_info_t info;
        rc = dir_lookup(parent, basename, &info);
        if (rc != FAT_OK) return rc;
        if (!(info.attr & ATTR_DIRECTORY)) return FAT_ENOTDIR;
        dir_cluster = info.first_cluster;
        if (dir_cluster == 0) dir_cluster = g_ctx.bpb->root_cluster;
    }

    int count = 0;
    uint32_t cursor = 0;
    while (1) {
        dir_entry_info_t e;
        if (!dir_iter_next(dir_cluster, &cursor, &e)) break;
        if (cb) cb(e.name, e.attr, e.size, user);
        count++;
    }
    return count;
}

/* =====================================================================
 * Diagnostic — kept compatible with the existing call in main.c
 * ===================================================================== */
void fat32_read_cluster_chain(uint32_t start_cluster) {
    if (!g_ctx.initialized) {
        kprintf("FAT32 not initialized!\n");
        return;
    }
    kprintf("\nFollowing chain from cluster %d:\n", start_cluster);
    uint32_t cur = start_cluster;
    int n = 0;
    while (!fat32_is_eof(cur) && n < 100) {
        kprintf("  cluster %d -> data %x\n", cur,
                (uint64_t)fat32_get_cluster_address(&g_ctx, cur));
        uint32_t next = fat32_get_next_cluster(&g_ctx, cur);
        if (fat32_is_bad(next))  { kprintf("  bad cluster!\n"); break; }
        if (fat32_is_free(next)) { kprintf("  unexpected free!\n"); break; }
        if (fat32_is_eof(next))  { kprintf("  EOF\n"); break; }
        cur = next;
        n++;
    }
    kprintf("Length: %d\n", n + 1);
}