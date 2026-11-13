/* Exercise the production loader and overlay mount with real FatFs.
 * The disc controller addresses and reader pause are modeled by the fixture. */
#define MCU_OVERLAY_TEST
#define MCU_CUE_TEST_ENTRY cue_fixture_main
#include "mcu_save_cue.c"

static u32 cache_first, cache_end;
static unsigned invalidations, preflight_reads, pack_opens;
static int clip_overlay_seek;
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

static uint8_t original[3][2352], edited[2352], pack[SECTOR_OVERLAY_MAX_BYTES + 1];
static const char pack_name[] = "/SAROO/PATCHES/968872B4.SOV";

static size_t read_input(const char *path, uint8_t *data, size_t capacity)
{
    FILE *fp = fopen(path, "rb");
    assert(fp);
    size_t n = fread(data, 1, capacity, fp);
    assert(!ferror(fp) && fgetc(fp) == EOF && !fclose(fp));
    return n;
}

static void verify_overlay(int enabled)
{
    uint8_t sector[2352];
    memcpy(sector, original[1], sizeof(sector));
    assert(sector_overlay_apply(151, sector) == enabled);
    assert(!memcmp(sector, enabled ? edited : original[1], sizeof(sector)));
}

static void verify_image(void)
{
    FIL fp;
    uint8_t contents[sizeof(original)];
    UINT got;
    assert(!f_open(&fp, "/D/a.bin", FA_READ));
    assert(!f_read(&fp, contents, sizeof(contents), &got) && got == sizeof(contents));
    assert(!f_close(&fp) && !memcmp(contents, original, sizeof(contents)));
}

int main(int argc, char **argv)
{
    assert(argc == 6);
    disk = fopen(argv[1], "r+b");
    assert(disk);
    FATFS fs;
    assert(!f_mount(&fs, "0:", 1) && !f_mkdir("/D") && !f_mkdir("/SAROO") &&
           !f_mkdir("/SAROO/PATCHES"));
    assert(read_input(argv[3], original[0], sizeof(original)) == sizeof(original));
    assert(read_input(argv[4], edited, sizeof(edited)) == sizeof(edited));
    size_t pack_bytes = read_input(argv[5], pack, sizeof(pack));
    create("/D/a.bin", original, sizeof(original));
    create(cue_name, single, strlen(single));
    int which = atoi(argv[2]);
    if (which != 1) {
        if (which == 2) pack[0] ^= 1;
        if (which == 4) pack_bytes = sizeof(pack);
        create(pack_name, pack, pack_bytes);
    }
    unsigned writes = disk_writes;
    cache_first = 150;
    cache_end = 153;
    media_case = 1;
    int requested = which == 3 ? 0 : SD_OVERLAY_REQUEST;
    if (which == 5) inject_open = 5;
    if (which == 6) short_read = 3;
    if (which == 7) clip_overlay_seek = 1;
    if (which == 8) inject_read = 4;
    if (which == 9) inject_close = 2;
    if (which == 12) fail_overlay_save = 1;
    int result = load_disc(requested, 1);
    assert((result == 0) == (which < 5 || which == 10 || which == 11));
    assert(overlay_sysinfo[0x38 / 4] == (which == 11 ? SD_OVERLAY_REPLACEMENT : 0));
    assert(!lock_depth && !cache_first && !cache_end && invalidations >= 1);
    assert(disk_writes == writes && pack_opens == (which != 3));
    if (which == 0 || which == 10 || which == 11) {
        assert(mounted_disc == 0 && preflight_reads == 1);
        verify_overlay(1);
        uint8_t corrupt[2352];
        memcpy(corrupt, original[1], sizeof(corrupt));
        corrupt[50] ^= 1;
        assert(sector_overlay_apply(151, corrupt) == -1);
        assert(sector_overlay_apply(151, corrupt) == -1);
        verify_overlay(1);
        verify_image();
        assert(overlay_sysinfo[0x38 / 4] == (which == 11 ? SD_OVERLAY_REPLACEMENT : 0));
        if (which == 10) {
            assert(!f_unlink(pack_name));
            writes = disk_writes;
            assert(!load_disc(0, 0) && mounted_disc == 0);
            assert(pack_opens == 2 && preflight_reads == 1);
            verify_overlay(0);
        }
    } else {
        verify_overlay(0);
        assert(mounted_disc == (result ? -1 : 0));
    }
    assert(!unload_disc() && !handles_count && !lock_depth);
    assert(mounted_disc == -1 && disk_writes == writes && !overlay_sysinfo[0x38 / 4]);
    verify_overlay(0);
    assert(!fclose(disk));
    return 0;
}
