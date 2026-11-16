/* paps-g d063f6db (SSR-117); public adaptation by papsg. */
/* Complete selected FatFs, synthetic media, and the existing disk/mutex seam.
 * Field positions, output encodings and capacities are independent test oracles. */
#include "disk.inc"
#include <stdbool.h>

static unsigned checks, errors;
static FATFS fs;
static BYTE root_template[512];
static LBA_t root_sector;
static TCHAR root[4];

static void ascii(TCHAR *out, const char *s) {
    do { *out++ = (BYTE)*s; } while (*s++);
}
static unsigned capacity(void) {
#if FF_LFN_UNICODE == 2
    return 34;
#elif FF_LFN_UNICODE == 0
    return FF_FS_EXFAT ? 23 : 12;
#else
    return 12;
#endif
}
static void check(bool ok, const char *name) {
    checks++;
    if (!ok) { errors++; fprintf(stderr, "failed=%s check=%u\n", name, checks); }
}
static WORD bmp_word(void) {
    return FF_LFN_UNICODE == 0 ? 0x00e9 : 0x0800;
}
static unsigned encode(TCHAR *out, const WORD *words, unsigned count) {
    unsigned n = 0;
    for (unsigned i = 0; i < count; i++) {
        unsigned cp = words[i];
        if (cp >= 0xd800 && cp <= 0xdbff) {
            assert(i + 1 < count && words[i + 1] >= 0xdc00 && words[i + 1] <= 0xdfff);
            cp = 0x10000 + ((cp - 0xd800) << 10) + words[++i] - 0xdc00;
        }
#if FF_LFN_UNICODE == 0
        if (cp == 0x00e9) out[n++] = (TCHAR)0x82;
        else { assert(cp < 128); out[n++] = (TCHAR)cp; }
#elif FF_LFN_UNICODE == 1
        if (cp > 0xffff) {
            cp -= 0x10000; out[n++] = (TCHAR)(0xd800 + (cp >> 10));
            out[n++] = (TCHAR)(0xdc00 + (cp & 1023));
        } else out[n++] = (TCHAR)cp;
#elif FF_LFN_UNICODE == 2
        if (cp < 128) out[n++] = (TCHAR)cp;
        else if (cp < 0x800) {
            out[n++] = (TCHAR)(0xc0 | (cp >> 6)); out[n++] = (TCHAR)(0x80 | (cp & 63));
        } else {
            if (cp >= 0x10000) {
                out[n++] = (TCHAR)(0xf0 | (cp >> 18));
                out[n++] = (TCHAR)(0x80 | ((cp >> 12) & 63));
            } else out[n++] = (TCHAR)(0xe0 | (cp >> 12));
            out[n++] = (TCHAR)(0x80 | ((cp >> 6) & 63));
            out[n++] = (TCHAR)(0x80 | (cp & 63));
        }
#else
        out[n++] = (TCHAR)cp;
#endif
    }
    out[n] = 0;
    return n;
}

static void label_entry(unsigned count, unsigned slot, unsigned pattern, bool absent) {
    assert(slot == 0 || slot == 15);
    assert(!f_mount(NULL, root, 0));
    BYTE sector[512]; memcpy(sector, root_template, sizeof sector);
    /* Move the label past the formatter's bitmap/upcase entries. Inactive
     * entries retain traversal; an end marker before the label would not. */
    if (slot) {
        sector[0] = 0x03;
        for (unsigned i = 3; i < slot; i++) sector[i * 32] = 0x03;
    }
    BYTE *entry = sector + slot * 32;
    memset(entry, 0, 32); entry[0] = absent ? 0x03 : 0x83; entry[1] = (BYTE)count;
    /* Valid entries leave unused label words and reserved bytes zero. Malformed
     * counts poison the reserved bytes to expose a reader that trusts them. */
    for (unsigned i = 0; i < (count <= 11 ? count : 15); i++) {
        WORD wc = pattern == 0 ? 'A' : bmp_word();
        if (pattern == 2 && FF_LFN_UNICODE != 0) {
            wc = (i & 1) ? 0xde00 : 0xd83d;
            if ((count & 1) && i + 1 == count) wc = 'Z';
        }
        entry[2 + i * 2] = (BYTE)wc; entry[3 + i * 2] = (BYTE)(wc >> 8);
    }
    assert(disk_write(0, sector, root_sector, 1) == RES_OK);
    assert(disk_ioctl(0, CTRL_SYNC, NULL) == RES_OK);
    assert(!f_mount(&fs, root, 1));
}

static void probe(unsigned count, unsigned slot, unsigned pattern, bool heap) {
    label_entry(count, slot, pattern, false);
    unsigned cap = capacity(), padding = heap ? 0 : 64;
    for (unsigned hot = 0; hot < 2; hot++) {
        TCHAR *storage = malloc((cap + padding) * sizeof(TCHAR)); assert(storage);
        for (unsigned i = 0; i < cap + padding; i++) storage[i] = (TCHAR)0x5a;
        unsigned old_writes = writes;
        FRESULT result = f_getlabel(root, storage, NULL);
#if FF_FS_REENTRANT
        int grant = pthread_mutex_trylock(&mutex);
        check(grant == 0, "reader released volume grant");
        if (grant != 0) exit(42); /* Preserve a refusal without deadlocking a repeat. */
        assert(!pthread_mutex_unlock(&mutex));
#endif
        unsigned changed = 0;
        for (unsigned i = cap; i < cap + padding; i++) changed += storage[i] != (TCHAR)0x5a;
        check(!changed, "post-capacity guard");
        check(writes == old_writes, "reader writes no disk sectors");
        if (count > 11) {
            check(result == FR_INT_ERR, "invalid count is an internal filesystem error");
            bool untouched = true;
            for (unsigned i = 0; i < cap; i++) untouched &= storage[i] == (TCHAR)0x5a;
            check(untouched, "invalid count publishes no label");
            if (changed || result != FR_INT_ERR) fprintf(stderr,
                "count=%u slot=%u encoding=%u capacity=%u hot=%u changed_guard_units=%u result=%d\n",
                count, slot, FF_LFN_UNICODE, cap, hot, changed, result);
        } else {
            WORD words[11]; TCHAR expected[34];
            for (unsigned i = 0; i < count; i++) {
                words[i] = pattern == 0 ? 'A' : bmp_word();
                if (pattern == 2 && FF_LFN_UNICODE != 0) {
                    words[i] = (i & 1) ? 0xde00 : 0xd83d;
                    if ((count & 1) && i + 1 == count) words[i] = 'Z';
                }
            }
            unsigned n = encode(expected, words, count); assert(n < cap);
            check(result == FR_OK, "valid count succeeds");
            check(!memcmp(expected, storage, (n + 1) * sizeof(TCHAR)), "exact encoded label and NUL");
        }
        free(storage);
    }
}

int main(int argc, char **argv) {
    assert(argc == 3); const char *mode = argv[2];
    bool fat32 = !strcmp(mode, "fat32");
    disk = fopen(argv[1], "r+b"); assert(disk && !fseeko(disk, 0, SEEK_END));
    sectors = ftello(disk) / 512; ascii(root, "0:");
    BYTE work[32768]; TCHAR input[24];
    MKFS_PARM options = {.fmt = (fat32 ? FM_FAT32 : FM_EXFAT) | FM_SFD,
                         .n_fat = 1, .au_size = fat32 ? 512 : 65536};
    assert(!f_mkfs(root, &options, work, sizeof work));
    assert(!f_mount(&fs, root, 1));
    assert(fs.fs_type == (fat32 ? FS_FAT32 : FS_EXFAT));
    ascii(input, "0:ORIGINAL"); assert(!f_setlabel(input));
    DWORD serial = 0; assert(!f_getlabel(root, NULL, &serial));
    if (fat32) {
        const char *labels[] = {"ABCDEFGHIJK", "SHORT", "", "RECOVERED"};
        for (unsigned i = 0; i < 4; i++) {
            ascii(input, "0:"); ascii(input + 2, labels[i]); assert(!f_setlabel(input));
            for (unsigned cold = 0; cold < 2; cold++) {
                if (cold) { assert(!f_mount(NULL, root, 0)); assert(!f_mount(&fs, root, 1)); }
                TCHAR *out = malloc(capacity() * sizeof(TCHAR)); assert(out);
                DWORD got = ~serial; TCHAR expected[12]; ascii(expected, labels[i]);
                unsigned old_writes = writes;
                check(!f_getlabel(root, out, &got) && got == serial, "FAT32 label/serial result");
                check(!memcmp(out, expected, (strlen(labels[i]) + 1) * sizeof(TCHAR)), "FAT32 label bytes");
                check(writes == old_writes, "FAT32 read performs no writes"); free(out);
            }
        }
    } else {
        root_sector = fs.database + ((LBA_t)fs.dirbase - 2) * fs.csize;
        assert(disk_read(0, root_template, root_sector, 1) == RES_OK && root_template[0] == 0x83);
        if (!strcmp(mode, "valid")) {
            for (unsigned slot = 0; slot <= 15; slot += 15)
                for (unsigned n = 0; n <= 11; n++)
                    for (unsigned pattern = 0; pattern < 3; pattern++) probe(n, slot, pattern, true);
        } else if (!strcmp(mode, "bounds")) {
            probe(12, 0, 1, false); probe(13, 0, 1, false);
        } else if (!strcmp(mode, "heap")) {
            probe(11, 0, 1, true); probe(12, 0, 1, true);
        } else if (!strcmp(mode, "counts")) {
            for (unsigned slot = 0; slot <= 15; slot += 15)
                for (unsigned n = 12; n < 256; n++) probe(n, slot, 1, true);
        } else if (!strcmp(mode, "repair")) {
            label_entry(255, 15, 1, false);
            DWORD got = ~serial; unsigned old_writes = writes;
            check(!f_getlabel(root, NULL, &got) && got == serial, "serial-only skips malformed label");
            check(!f_getlabel(root, NULL, NULL) && writes == old_writes, "null outputs skip malformed label");
            ascii(input, "0:REPAIRED"); check(!f_setlabel(input), "setlabel can repair malformed label");
            assert(!f_mount(NULL, root, 0) && !f_mount(&fs, root, 1));
            TCHAR out[34], expected[12]; ascii(expected, "REPAIRED");
            check(!f_getlabel(root, out, &got) && got == serial, "repaired label result");
            check(!memcmp(out, expected, sizeof(TCHAR) * 9), "repaired label persists exactly");
        } else if (!strcmp(mode, "absent")) {
            label_entry(0, 0, 1, true); TCHAR out[34]; out[0] = 'X'; DWORD got = ~serial;
            check(!f_getlabel(root, out, &got) && !out[0] && got == serial, "absent label empty with serial");
            check(!f_getlabel(root, NULL, NULL), "null outputs succeed");
        } else assert(0);
    }
    assert(!f_mount(NULL, root, 0) && !fclose(disk));
    printf("mode=%s encoding=%u width=%zu checks=%u errors=%u\n", mode, FF_LFN_UNICODE, sizeof(TCHAR), checks, errors);
    return errors ? 42 : 0;
}
