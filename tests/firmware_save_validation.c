/* Reuse the actual-module storage model; all persistent images are generated. */
#define main resize_test_main
#include "firmware_save_resize.c"
#undef main

static BUPDIR target, neighbor;
static u8 *disk(int dev) { return dev ? mems_disk : slot_disk; }
static u8 *global_bitmap(int dev) { return disk(dev) + (dev ? 16 : 64); }
static void build_pair(int dev)
{
    initialize();
    target = directory("TEST0000001", 2048);
    neighbor = directory("KEEP0000001", 2048);
    u8 *a = payload(2048, 3), *b = payload(2048, 7);
    assert(sro_bup_write(dev, &target, a, 0) == 0);
    assert(sro_bup_write(dev, &neighbor, b, 0) == 0);
    free(a); free(b);
    reload_model();
}
static int header_id(int dev, BUPDIR *dir)
{
    bup_dev = dev;
    int id = find_save(dir->file_name, 0, NULL);
    assert(id > 0);
    return id;
}
static int data_id(int dev, const u8 *bp)
{
    if (dev) return *(const u16 *)(bp + 64);
    for (int i = 0; i < 512; i++) if (bp[64 + i / 8] & (1u << (i % 8))) return i;
    abort();
}
static int damage(int dev, const char *reason)
{
    int a = header_id(dev, &target), b = header_id(dev, &neighbor);
    u8 *base = disk(dev), *bp = base + a * (dev ? 1024 : 128);
    u8 *other = base + b * (dev ? 1024 : 128), *bitmap = global_bitmap(dev);
    int data = data_id(dev, bp), other_data = data_id(dev, other);
    int shared = -1;
    if (!strcmp(reason, "magic0")) *(u32 *)base ^= 1;
    else if (!strcmp(reason, "magic1")) *(u32 *)(base + 4) ^= 1;
    else if (!strcmp(reason, "total-size")) *(u32 *)(base + 8) = 0;
    else if (!strcmp(reason, "unreserved")) bitmap[0] &= ~1;
    else if (!strcmp(reason, "unallocated-header")) set_bitmap(bitmap, a, 0);
    else if (!strcmp(reason, "unallocated-data")) set_bitmap(bitmap, data, 0);
    else if (!strcmp(reason, "huge-size")) *(u32 *)(bp + 12) = 0xffffffff;
    else if (!strcmp(reason, "oversized")) *(u32 *)(bp + 12) = dev ? 479u * 1024 + 1 : 510u * 128 + 1;
    else if (!strcmp(reason, "empty-name")) bp[0] = 0;
    else if (!strcmp(reason, "self-data")) shared = a;
    else if (!strcmp(reason, "neighbor-header")) shared = b;
    else if (!strcmp(reason, "shared-data")) shared = other_data;
    else if (!strcmp(reason, "missing-data")) {
        if (dev) *(u16 *)(bp + 64) = 0;
        else set_bitmap(bp + 64, data, 0);
    } else if (!strcmp(reason, "header-oob")) {
        if (dev) *(u16 *)(base + 1024 + 14) = 8064;
        else *(u16 *)(base + 62) = 512;
    } else if (!strcmp(reason, "header-max")) {
        if (dev) *(u16 *)(base + 1024 + 14) = 65535;
        else *(u16 *)(base + 62) = 65535;
    } else if (!strcmp(reason, "header-cycle")) {
        assert(!dev); *(u16 *)(other + 62) = a;
    } else if (!strcmp(reason, "header-self-cycle")) {
        assert(!dev); *(u16 *)(bp + 62) = a;
    } else if (!strcmp(reason, "next-oob")) {
        assert(!dev); *(u16 *)(bp + 62) = 65535;
    } else if (!strcmp(reason, "block-size-zero")) {
        assert(!dev); *(u16 *)(base + 12) = 0;
    } else if (!strcmp(reason, "block-size-wrong")) {
        assert(!dev); *(u16 *)(base + 12) = 64;
    } else if (!strcmp(reason, "zero-size-with-data")) {
        assert(!dev); *(u32 *)(bp + 12) = 0;
    } else if (!strcmp(reason, "header-reserved")) {
        assert(dev); *(u16 *)(base + 1024 + 14) = 7;
    } else if (!strcmp(reason, "header-zero")) {
        assert(dev); *(u16 *)(base + 1024 + 14) = 0;
    } else if (!strcmp(reason, "header-duplicate")) {
        assert(dev); *(u16 *)(base + 1024 + 16 + 14) = a;
    } else if (!strcmp(reason, "header-name-mismatch")) {
        assert(dev); bp[0] = 'Q';
    } else if (!strcmp(reason, "header-name-after-nul")) {
        assert(dev);
        memcpy(base + 1024, "ABC\0AAAAAAA", 11);
        memcpy(bp, "ABC\0BBBBBBB", 11);
    } else if (!strcmp(reason, "duplicate-data")) {
        assert(dev); *(u16 *)(bp + 66) = data;
    } else if (!strcmp(reason, "data-oob")) {
        assert(dev); *(u16 *)(bp + 64) = 8064;
    } else if (!strcmp(reason, "data-max")) {
        assert(dev); *(u16 *)(bp + 64) = 65535;
    } else if (!strcmp(reason, "data-reserved")) {
        assert(dev); *(u16 *)(bp + 64) = 7;
    } else if (!strcmp(reason, "bad-terminator")) {
        assert(dev); *(u16 *)(bp + 68) = other_data;
    } else if (!strcmp(reason, "too-few-blocks")) {
        assert(dev); *(u32 *)(bp + 12) = 4096;
    } else if (!strcmp(reason, "too-many-blocks")) {
        assert(dev); *(u32 *)(bp + 12) = 1024;
    } else abort();
    if (shared >= 0) {
        if (dev) *(u16 *)(bp + 64) = shared;
        else { set_bitmap(bp + 64, data, 0); set_bitmap(bp + 64, shared, 1); }
    }
    reload_model();
    return !strncmp(reason, "magic", 5) ? BUP_UNFORMAT : BUP_BROKEN;
}
static int operate(int dev, const char *operation, u8 *out, BUPDIR *dirs, BUPSTAT *stat)
{
    if (!strcmp(operation, "read")) return sro_bup_read(dev, target.file_name, out);
    if (!strcmp(operation, "verify")) return sro_bup_verify(dev, target.file_name, out);
    if (!strcmp(operation, "delete")) return sro_bup_delete(dev, target.file_name);
    if (!strcmp(operation, "dir")) return sro_bup_dir(dev, "", 2, dirs);
    if (!strcmp(operation, "stat")) return sro_bup_stat(dev, 2048, stat);
    if (!strcmp(operation, "new")) {
        BUPDIR fresh = directory("NEW00000001", 2048);
        return sro_bup_write(dev, &fresh, out, 0);
    }
    assert(!strcmp(operation, "overwrite"));
    return sro_bup_write(dev, &target, out, 0);
}
static void rejected_metadata(int dev, const char *reason, const char *operation)
{
    build_pair(dev);
    int expected = damage(dev, reason);
    u8 *snapshot = malloc(sizeof(mems_disk)), *slot = malloc(sizeof(slot_disk));
    assert(snapshot && slot);
    memcpy(snapshot, mems_disk, sizeof(mems_disk)); memcpy(slot, slot_disk, sizeof(slot_disk));
    u8 out[4096], initial[4096]; BUPDIR dirs[2], old_dirs[2]; BUPSTAT stat, old_stat;
    memset(out, 0xcc, sizeof(out)); memcpy(initial, out, sizeof(out));
    memset(dirs, 0xcc, sizeof(dirs)); memcpy(old_dirs, dirs, sizeof(dirs));
    memset(&stat, 0xcc, sizeof(stat)); old_stat = stat;
    int writes = flushes;
    int result = operate(dev, operation, out, dirs, &stat);
    assert(result == (!strcmp(operation, "dir") ? 0 : expected));
    assert(flushes == writes);
    assert(memcmp(snapshot, mems_disk, sizeof(mems_disk)) == 0);
    assert(memcmp(slot, slot_disk, sizeof(slot_disk)) == 0);
    assert(memcmp((void *)MEMS_HEADER, mems_disk, 8192) == 0);
    assert(memcmp(test_shared, slot_disk, 65536) == 0);
    assert(memcmp(out, initial, sizeof(out)) == 0);
    assert(memcmp(dirs, old_dirs, sizeof(dirs)) == 0);
    assert(memcmp(&stat, &old_stat, sizeof(stat)) == 0);
    free(snapshot); free(slot);
}
static int bitmap_free(int dev)
{
    int free = 0;
    for (int i = dev ? 8 : 1; i < (dev ? 8064 : 512); i++)
        if (!(global_bitmap(dev)[i / 8] & (1u << (i % 8)))) free++;
    return free;
}
static void stale_count(int dev, const char *reason)
{
    build_pair(dev);
    int old = !strcmp(reason, "low") ? 0 : 65535;
    int orphan = dev ? 8000 : 500;
    set_bitmap(global_bitmap(dev), orphan, 1);
    *(u16 *)(disk(dev) + (dev ? 12 : 14)) = old;
    reload_model();
    int available = bitmap_free(dev); BUPSTAT stat;
    int writes = flushes;
    assert(sro_bup_stat(dev, 2048, &stat) == 0);
    assert(stat.free_block == (u32)available);
    assert(stat.data_num == (u32)(dev ? 446 : available / 17));
    assert(flushes == writes);
    BUPDIR fresh = directory("NEW00000001", 960); u8 *data = payload(960, 9);
    assert(sro_bup_write(dev, &fresh, data, 0) == 0);
    reload_model(); check_save(dev, &fresh, data);
    u8 *a = payload(2048, 3), *b = payload(2048, 7);
    check_save(dev, &target, a); check_save(dev, &neighbor, b);
    assert((dev ? MEMS->free_block : BUPMEM->free_block) == bitmap_free(dev));
    assert(sro_bup_delete(dev, target.file_name) == 0);
    reload_model(); check_save(dev, &neighbor, b); check_save(dev, &fresh, data);
    assert((dev ? MEMS->free_block : BUPMEM->free_block) == bitmap_free(dev));
    assert(global_bitmap(dev)[orphan / 8] & (1u << (orphan % 8)));
    free(a); free(b); free(data);
}
static void full_bitmap(int dev)
{
    build_pair(dev);
    memset(global_bitmap(dev), 255, dev ? 1008 : 64);
    *(u16 *)(disk(dev) + (dev ? 12 : 14)) = 65535;
    reload_model();
    BUPSTAT stat;
    assert(sro_bup_stat(dev, 1, &stat) == 0);
    assert(stat.free_block == 0 && stat.free_size == 0 && stat.data_num == 0);
    u8 data[32] = {0}; BUPDIR fresh = directory("NEW00000001", 32);
    int writes = flushes;
    assert(sro_bup_write(dev, &fresh, data, 0) == BUP_NOT_ENOUGH_MEMORY);
    assert(flushes == writes);
    assert(*(u16 *)(disk(dev) + (dev ? 12 : 14)) == 65535);
}
static void statistics(int dev)
{
    build_pair(dev);
    static const struct { u32 size, slots, mems; } cases[] = {
        {0, 477, 446}, {1, 238, 446}, {128, 238, 446}, {129, 159, 446},
        {960, 53, 446}, {961, 53, 446}, {2048, 28, 446}, {4096, 14, 446},
        {32768, 1, 243}, {65536, 0, 123}, {479u * 1024, 0, 16},
        {479u * 1024 + 1, 0, 0}, {0xffffffff, 0, 0}
    };
    BUPSTAT stat;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(sro_bup_stat(dev, cases[i].size, &stat) == 0);
        assert(stat.data_num == (dev ? cases[i].mems : cases[i].slots));
        assert(stat.free_block == (dev ? 8050u : 477u));
        assert(stat.free_size == (dev ? 8049u * 1024 : 476u * 128));
    }
}
static void names(int dev, const char *reason)
{
    initialize();
    char longer[12] = "ABCDEF", shorter[12] = "ABC";
    target = directory(longer, 32); neighbor = directory(shorter, 32);
    u8 a[32], b[32]; memset(a, 'A', 32); memset(b, 'B', 32);
    assert(sro_bup_write(dev, &target, a, 0) == 0);
    assert(sro_bup_write(dev, &neighbor, b, 0) == 0);
    reload_model();
    assert(sro_bup_read(dev, longer, a) == 0);
    assert(sro_bup_verify(dev, longer, a) == 0);
    assert(sro_bup_read(dev, shorter, b) == 0);
    for (int i = 0; i < 32; i++) assert(a[i] == 'A' && b[i] == 'B');
    assert(sro_bup_verify(dev, shorter, b) == 0);
    if (!strcmp(reason, "exact")) {
        assert(sro_bup_delete(dev, shorter) == 0);
        reload_model(); check_save(dev, &target, a);
        assert(sro_bup_read(dev, shorter, b) == BUP_NOT_FOUND);
    } else if (!strcmp(reason, "short")) {
        char *name = malloc(4); assert(name); memcpy(name, "ABC", 4);
        assert(sro_bup_read(dev, name, b) == 0);
        assert(sro_bup_verify(dev, name, b) == 0);
        BUPDIR dirs[2]; assert(sro_bup_dir(dev, name, 2, dirs) == 2);
        assert(sro_bup_dir(dev, name, 1, dirs) == -2);
        assert(sro_bup_dir(dev, name, 0, NULL) == -2);
        assert(sro_bup_delete(dev, name) == 0);
        free(name);
    } else abort();
}
static void fixed_width(int dev)
{
    build_pair(dev);
    int id = header_id(dev, &target);
    u8 *bp = disk(dev) + id * (dev ? 1024 : 128);
    bp[11] = 0xaa; bp[26] = 0xbb;
    reload_model();
    char *name = malloc(11); assert(name); memcpy(name, target.file_name, 11);
    BUPDIR dir;
    assert(sro_bup_dir(dev, name, 1, &dir) == 1);
    assert(dir.file_name[11] == 0 && dir.comment[10] == 0);
    assert(memcmp(dir.file_name, target.file_name, 11) == 0);
    assert(memcmp(dir.comment, target.comment, 10) == 0);
    u8 *data = payload(2048, 3);
    assert(sro_bup_read(dev, name, data) == 0);
    assert(sro_bup_verify(dev, name, data) == 0);
    assert(sro_bup_delete(dev, name) == 0);
    free(name); free(data);
}
static void equal_mems_name_fields(void)
{
    build_pair(1);
    int id = header_id(1, &target);
    memcpy(mems_disk + 1024, "ABC\0AAAAAAA", 11);
    memcpy(mems_disk + id * 1024, "ABC\0AAAAAAA", 11);
    reload_model();
    u8 data[2048];
    assert(sro_bup_read(1, "ABC", data) == 0);
    u8 *expected = payload(2048, 3);
    assert(memcmp(data, expected, sizeof data) == 0);
    assert(sro_bup_verify(1, "ABC", data) == 0);
    BUPDIR dir;
    assert(sro_bup_dir(1, "ABC", 1, &dir) == 1);
    assert(memcmp(dir.file_name, "ABC\0AAAAAAA", 11) == 0);
    assert(dir.file_name[11] == 0);
    free(expected);
}

static void invalid_arguments(int dev)
{
    initialize();
    BUPDIR dir = directory("TEST0000001", 0); BUPSTAT stat; u8 out[32];
    assert(sro_bup_write(dev, &dir, NULL, 0) == 0);
    assert(sro_bup_read(dev, dir.file_name, NULL) == 0);
    assert(sro_bup_verify(dev, dir.file_name, NULL) == 0);
    char empty[12] = {0};
    assert(sro_bup_read(dev, empty, out) == BUP_NOT_FOUND);
    assert(sro_bup_delete(dev, empty) == BUP_NOT_FOUND);
    assert(sro_bup_verify(dev, empty, out) == BUP_NOT_FOUND);
    assert(sro_bup_dir(dev, empty, -1, &dir) == 0);
    assert(sro_bup_stat(dev, -1, &stat) == 0 && stat.data_num == 0);
    dir.data_size = 0xffffffff;
    assert(sro_bup_write(dev, &dir, out, 0) == BUP_NOT_ENOUGH_MEMORY);
    for (int bad = -1; bad <= 2; bad += 3) {
        assert(sro_bup_stat(bad, 1, &stat) == BUP_NON);
        assert(sro_bup_write(bad, &dir, out, 0) == BUP_NON);
        assert(sro_bup_read(bad, dir.file_name, out) == BUP_NON);
        assert(sro_bup_delete(bad, dir.file_name) == BUP_NON);
        assert(sro_bup_verify(bad, dir.file_name, out) == BUP_NON);
        assert(sro_bup_dir(bad, empty, 1, &dir) == 0);
        assert(sro_bup_format(bad) == BUP_NON);
        assert(sro_bup_sel_part(bad, 0) == BUP_NON);
    }
}
static void initialization(const char *reason)
{
    build_pair(1);
    int blank = !strcmp(reason, "blank-zero") ? 0 : 255;
    int damaged = !strcmp(reason, "damaged");
    if (damaged) mems_disk[0] ^= 1;
    else memset(mems_disk, blank, sizeof(mems_disk));
    reload_model();
    u8 *before = malloc(sizeof(mems_disk)); assert(before); memcpy(before, mems_disk, sizeof(mems_disk));
    u32 work[16] = {0}; u8 lib[16] = {0}; BUPCFG cfg[3] = {0};
    int writes = flushes;
    use_sys_bup = 1; /* Avoid the fixed Saturn BIOS work address in this host model. */
    bup_init(lib, (u8 *)work, cfg);
    BUPSTAT stat;
    if (damaged) {
        assert(flushes == writes);
        assert(memcmp(before, mems_disk, sizeof(mems_disk)) == 0);
        assert(sro_bup_stat(1, 1, &stat) == BUP_UNFORMAT);
    } else {
        assert(flushes == writes + 1);
        assert(sro_bup_stat(1, 1, &stat) == 0 && stat.free_block == 8056);
        assert(memcmp(before + 8192, mems_disk + 8192, sizeof(mems_disk) - 8192) == 0);
    }
    free(before);
}
static void maximum_directory(void)
{
    initialize();
    for (int i = 0; i < 448; i++) {
        char name[12]; snprintf(name, sizeof(name), "ALL%08d", i);
        u8 *entry = mems_disk + 1024 + i * 16;
        memcpy(entry, name, 11); *(u16 *)(entry + 14) = 8 + i;
        memcpy(mems_disk + (8 + i) * 1024, name, 11);
        set_bitmap(mems_disk + 16, 8 + i, 1);
    }
    *(u16 *)(mems_disk + 12) = 8056 - 448;
    reload_model(); loads_header = loads_window = 0;
    BUPSTAT stat; assert(sro_bup_stat(1, 1, &stat) == 0);
    assert(stat.data_num == 0 && stat.free_block == 8056 - 448);
    assert(loads_header <= 448 && loads_window == 0);
    int stat_loads = loads_header;
    reload_model(); loads_header = loads_window = 0;
    char name[12] = "ALL00000000"; u8 out;
    assert(sro_bup_read(1, name, &out) == 0);
    assert(loads_header <= 449 && loads_window == 0);
    printf("validation MCU header loads: stat=%d read=%d; data-window loads=%d\n",
           stat_loads, loads_header, loads_window);
    BUPDIR dir = directory("NEW00000001", 1);
    int writes = flushes;
    assert(sro_bup_write(1, &dir, &out, 0) == BUP_NOT_ENOUGH_MEMORY);
    assert(flushes == writes);
}
int main(int argc, char **argv)
{
    assert(argc >= 3);
    if (!strcmp(argv[1], "corrupt")) {
        assert(argc == 5); rejected_metadata(atoi(argv[2]), argv[3], argv[4]);
    } else if (!strcmp(argv[1], "stale")) stale_count(atoi(argv[2]), argv[3]);
    else if (!strcmp(argv[1], "full")) full_bitmap(atoi(argv[2]));
    else if (!strcmp(argv[1], "statistics")) statistics(atoi(argv[2]));
    else if (!strcmp(argv[1], "names")) names(atoi(argv[2]), argv[3]);
    else if (!strcmp(argv[1], "fixed")) fixed_width(atoi(argv[2]));
    else if (!strcmp(argv[1], "equal-mems-fields")) equal_mems_name_fields();
    else if (!strcmp(argv[1], "arguments")) invalid_arguments(atoi(argv[2]));
    else if (!strcmp(argv[1], "init")) initialization(argv[2]);
    else if (!strcmp(argv[1], "maximum")) maximum_directory();
    else abort();
    return 0;
}
