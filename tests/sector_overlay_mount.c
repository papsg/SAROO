/* Exercise the production loader and overlay mount with real FatFs.
 * The disc controller addresses and reader pause are modeled by the fixture. */
#define MCU_OVERLAY_TEST
#define MCU_CUE_TEST_ENTRY cue_fixture_main
#include "mcu_save_cue.c"

static u32 cache_first, cache_end;
static unsigned invalidations, preflight_reads, pack_opens;
static int clip_overlay_seek, missing_overlay_dir;
static uint32_t overlay_sysinfo[16];
#define SYSINFO_ADDR ((uintptr_t)overlay_sysinfo)
#define REG(address) (*(volatile uint32_t *)(address))

static void invalidate_sector_cache(void)
{
    ++invalidations;
    cache_first = cache_end = 0;
}

static FRESULT overlay_open(FIL *file, const char *path, BYTE flags)
{
    assert(lock_depth == 1);
    ++pack_opens;
    if (missing_overlay_dir && pack_opens == 1) return FR_NO_PATH;
    return cue_open(file, path, flags);
}

static FRESULT overlay_read(FIL *file, void *buffer, UINT length, UINT *got)
{
    assert(lock_depth == 1);
    if (buffer == test_scratch) ++preflight_reads;
    return cue_read(file, buffer, length, got);
}

static FRESULT overlay_seek(FIL *file, FSIZE_t offset)
{
    assert(lock_depth == 1);
    return cue_seek(file, clip_overlay_seek ? offset - 1 : offset);
}

#define f_open overlay_open
#define f_read overlay_read
#define f_close cue_close
#define f_lseek overlay_seek
#include "overlay-media.inc"
#undef f_lseek
#undef f_close
#undef f_read
#undef f_open

static uint8_t original[14][2352], edited[2352], pack[4201];
static size_t original_bytes;
static const char pack_name[] = "/SAROO/PATCHES/968872B4.SOV";
static const char v1000_name[] = "/SAROO/PATCHES/B97758E7.SOV";
static const char v1001_name[] = "/SAROO/PATCHES/CE706871.SOV";

static size_t read_input(const char *path, uint8_t *data, size_t capacity)
{
    FILE *fp = fopen(path, "rb");
    assert(fp);
    size_t n = fread(data, 1, capacity, fp);
    assert(!ferror(fp) && fgetc(fp) == EOF && !fclose(fp));
    return n;
}

static void verify_overlay(unsigned index, int enabled)
{
    uint8_t sector[2352];
    memcpy(sector, original[index], sizeof(sector));
    assert(sector_overlay_apply(150 + index, sector) == enabled);
    assert(!memcmp(sector, enabled ? edited : original[index], sizeof(sector)));
}

static void verify_image(void)
{
    FIL fp;
    uint8_t contents[sizeof(original)];
    UINT got;
    assert(!f_open(&fp, "/D/a.bin", FA_READ));
    assert(!f_read(&fp, contents, sizeof(contents), &got) && got == original_bytes);
    assert(!f_close(&fp) && !memcmp(contents, original, original_bytes));
}

int main(int argc, char **argv)
{
    assert(argc == 6);
    disk = fopen(argv[1], "r+b");
    assert(disk);
    FATFS fs;
    assert(!f_mount(&fs, "0:", 1) && !f_mkdir("/D") && !f_mkdir("/SAROO") &&
           !f_mkdir("/SAROO/PATCHES"));
    original_bytes = read_input(argv[3], (uint8_t *)original, sizeof(original));
    assert(original_bytes >= 3 * 2352 && original_bytes % 2352 == 0);
    unsigned last = (unsigned)(original_bytes / 2352) - 2;
    assert(read_input(argv[4], edited, sizeof(edited)) == sizeof(edited));
    size_t pack_bytes = read_input(argv[5], pack, sizeof(pack));
    create("/D/a.bin", original, original_bytes);
    create(cue_name, single, strlen(single));
    int which = atoi(argv[2]);
    if (which < 13 && which != 1) {
        if (which == 2) pack[0] ^= 1;
        if (which == 4) pack_bytes = sizeof(pack);
        create(pack_name, pack, pack_bytes);
    }
    if (which == 13 || which == 14) {
        create(which == 13 ? v1000_name : v1001_name, pack, pack_bytes);
        pack[0] ^= 1;
        create(which == 13 ? v1001_name : v1000_name, pack, pack_bytes);
        create(pack_name, pack, pack_bytes);
    }
    if (which >= 15 && which <= 20) {
        create(pack_name, pack, pack_bytes);
        if (which == 15) pack[0] ^= 1;
        if (which == 16) {
            pack[28] ^= 1;  /* Wrong source with an intact pack checksum. */
            memset(pack + 20, 0, 4);
            uint32_t crc = sector_overlay_crc32(pack, pack_bytes);
            for (unsigned i = 0; i < 4; ++i) pack[20 + i] = (uint8_t)(crc >> (24 - 8 * i));
        }
        if (which == 17) pack_bytes = sizeof(pack);
        if (which == 20) --pack_bytes;
        if (which != 19) create(v1000_name, pack, pack_bytes);
    }
    if (which == 21) create(v1001_name, pack, pack_bytes);
    if (which == 22 || which == 23) create(v1000_name, pack, pack_bytes);
    unsigned writes = disk_writes;
    cache_first = 150;
    cache_end = 153;
    media_case = 1;
    int requested = which == 3 ? 0 : SD_OVERLAY_REQUEST;
    if (which == 5 || which == 18) inject_open = 5;
    if (which == 6) short_read = 3;
    if (which == 7) clip_overlay_seek = 1;
    if (which == 8) inject_read = 4;
    if (which == 9) inject_close = 2;
    if (which == 12) fail_overlay_save = 1;
    if (which == 19) missing_overlay_dir = 1;
    int result = load_disc(requested, 1);
    int failure = (which >= 5 && which <= 9) || which == 12 || which == 18;
    int admitted = which == 0 || which == 10 || which == 11 ||
                   which == 13 || which == 14 || which == 19 || which == 22;
    uint32_t handoff = (which == 11 || which == 22) ? SD_OVERLAY_REPLACEMENT : 0;
    unsigned expected_opens = 2;
    if (which == 5 || (which >= 13 && which != 19 && which != 21)) expected_opens = 1;
    if (which == 3) expected_opens = 0;
    unsigned expected_reads = which >= 22 ? 12 :
        (admitted || which == 8 || which == 12 || which == 16);
    assert((result != 0) == failure);
    assert(overlay_sysinfo[0x38 / 4] == handoff);
    assert(!lock_depth && !cache_first && !cache_end && invalidations >= 1);
    assert(disk_writes == writes && pack_opens == expected_opens);
    assert(preflight_reads == expected_reads);
    if (admitted) {
        assert(mounted_disc == 0);
        verify_overlay(last, 1);
        uint8_t corrupt[2352];
        memcpy(corrupt, original[last], sizeof(corrupt));
        corrupt[50] ^= 1;
        assert(sector_overlay_apply(150 + last, corrupt) == -1);
        assert(sector_overlay_apply(150 + last, corrupt) == -1);
        verify_overlay(last, 1);
        verify_image();
        assert(overlay_sysinfo[0x38 / 4] == handoff);
        if (which == 10) {
            assert(!f_unlink(pack_name));
            writes = disk_writes;
            assert(!load_disc(0, 0) && mounted_disc == 0);
            assert(pack_opens == 4 && preflight_reads == 1);
            verify_overlay(last, 0);
        }
    } else {
        verify_overlay(1, 0);
        if (last != 1) verify_overlay(last, 0);
        assert(mounted_disc == (result ? -1 : 0));
    }
    assert(!unload_disc() && !handles_count && !lock_depth);
    assert(mounted_disc == -1 && disk_writes == writes && !overlay_sysinfo[0x38 / 4]);
    verify_overlay(last, 0);
    assert(!fclose(disk));
    return 0;
}
