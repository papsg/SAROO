/* Adapted from paps-g 96f9329a; preserved public baseline, actual current types. */
/* Unchanged pinned type definitions; hardware/callee effects are recorded. */
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"
CDBLOCK cdb;
int log_mask;
static unsigned cases;
static struct capture {
    unsigned used, text_used;
    struct { int kind; uint32_t values[20]; } event[8];
    char text[512];
} captured, expected;
static void record(int kind, unsigned value)
{
    assert(captured.used < 8);
    assert(cdb.cddev_filter < 24);
    unsigned n = captured.used++;
    captured.event[n].kind = kind;
    uint32_t values[] = {value, cdb.trans_type, cdb.trans_part_index,
        cdb.trans_block_start, cdb.trans_block_end, cdb.rdir_state,
        cdb.cddev_filter, cdb.pause_request, cdb.play_type, cdb.play_fad_start,
        cdb.play_fad_end, cdb.track, cdb.fad, cdb.old_fad, cdb.cdir_pfid, cdb.cdir_size,
        cdb.filter[cdb.cddev_filter].fad, cdb.filter[cdb.cddev_filter].range,
        cdb.filter[cdb.cddev_filter].mode, cdb.filter[cdb.cddev_filter].c_true};
    memcpy(captured.event[n].values, values, sizeof values);
}
static void printk(const char *format, ...)
{
    record(1, captured.text_used);
    va_list args; va_start(args, format);
    int n = vsnprintf(captured.text + captured.text_used,
                      sizeof captured.text - captured.text_used, format, args);
    va_end(args);
    assert(n >= 0 && (unsigned)n < sizeof captured.text - captured.text_used);
    captured.text_used += n;
}
static void set_status(u8 status) { record(2, status); }
static void set_report(u8 status) { record(3, status); }
static void trans_start(void) { record(4, 0); }
static int fad_to_track(u32 fad) { record(5, fad); return (fad / 75) % 32 + 1; }
static void disk_task_wakeup(void) { record(6, 0); }
#include "filter.inc"
#include "original.inc"
#include "candidate.inc"
static CDBLOCK input, output;
static void compare(int (*old)(void), int (*new)(void))
{
    memcpy(&input, &cdb, sizeof input);
    memset(&captured, 0, sizeof captured);
    int old_result = old();
    memcpy(&output, &cdb, sizeof output); expected = captured;
    memcpy(&cdb, &input, sizeof cdb);
    memset(&captured, 0, sizeof captured);
    int new_result = new();
    assert(old_result == new_result);
    assert(!memcmp(&output, &cdb, sizeof cdb));
    assert(!memcmp(&expected, &captured, sizeof captured));
    ++cases;
}
static void reset(void)
{
    memset(&cdb, 0, sizeof cdb);
    memset(cdb.filter, 0x35, sizeof cdb.filter);
    cdb.status = STAT_PLAY; cdb.fad = 4321; cdb.old_fad = 9876;
    cdb.pause_request = 1; cdb.play_type = PLAYTYPE_FILE;
    cdb.trans_part_index = 23; cdb.trans_block_start = 15; cdb.trans_block_end = 99;
}
static void transfers(void)
{
    const unsigned counts[] = {0, 1, 2, 17, 199, 200};
    for (unsigned pt = 0; pt < 24; ++pt)
    for (unsigned b = 0; b < 6; ++b) {
        unsigned count = counts[b];
        unsigned boundaries[] = {0, 1, count ? count-1 : 0, count, count+1, 0xffff};
        for (unsigned s = 0; s < 6; ++s)
        for (unsigned n = 0; n < 6; ++n)
        for (unsigned busy = 0; busy < 2; ++busy)
        for (unsigned logs = 0; logs < 2; ++logs)
        for (unsigned remove = 0; remove < 2; ++remove) {
            reset();
            cdb.part[pt].numblocks = count;
            cdb.cr3 = pt << 8; cdb.cr2 = boundaries[s]; cdb.cr4 = boundaries[n];
            cdb.trans_type = busy ? TRANS_PUT : 0;
            log_mask = logs ? _BUFIO : LOG_MASK_DEFAULT;
            compare(remove ? original_get_del_sector_data : original_get_sector_data,
                    remove ? get_del_sector_data : get_sector_data);
        }
    }
    /* Busy transfers return before looking at even an invalid partition. */
    reset(); cdb.trans_type = TRANS_PUT; cdb.cr3 = 0xff00;
    compare(original_get_sector_data, get_sector_data);
    compare(original_get_del_sector_data, get_del_sector_data);
}
static void directories(void)
{
    const unsigned lengths[] = {0, 1, 2, 254};
    const unsigned sizes[] = {0, 1, 255, 256};
    const unsigned ids[] = {0, 1, 4, 5, 6, 7, 258, 259, 0xffffff};
    for (unsigned sel = 0; sel < 24; ++sel)
    for (unsigned page = 0; page < 4; ++page)
    for (unsigned size = 0; size < 4; ++size)
    for (unsigned id = 0; id < 9; ++id)
    for (unsigned root = 0; root < 2; ++root)
    for (unsigned current = 0; current < 2; ++current)
    for (unsigned logs = 0; logs < 2; ++logs)
    for (unsigned read = 0; read < 2; ++read) {
        reset();
        for (unsigned i = 0; i < 256; ++i) {
            cdb.fi[i].lba = 1000 + i * 300;
            cdb.fi[i].size = sizes[size] * 2048;
        }
        cdb.cr3 = sel << 8 | ids[id] >> 16; cdb.cr4 = ids[id];
        cdb.cdir_pfid = 5; cdb.cdir_pfnum = lengths[page];
        cdb.root_lba = root ? 512 : 0; cdb.root_size = sizes[size];
        cdb.cdir_lba = current ? 768 : 0; cdb.cdir_size = sizes[size];
        log_mask = logs ? _FILEIO : 0;
        compare(read ? original_read_dir : original_change_dir,
                read ? read_dir : change_dir);
    }
}
static void contracts(void)
{
    for (unsigned remove = 0; remove < 2; ++remove) {
        reset(); memset(&captured, 0, sizeof captured); log_mask = 0;
        cdb.part[3].numblocks = 2;
        cdb.cr3 = 3u << 8; cdb.cr2 = cdb.cr4 = 0xffff;
        int result = remove ? get_del_sector_data() : get_sector_data();
        assert(result == (HIRQ_DRDY | HIRQ_EHST));
        assert(cdb.trans_type == (remove ? TRANS_DATA_DEL : TRANS_DATA));
        assert(cdb.trans_part_index == 3 && cdb.trans_block_start == 1 && cdb.trans_block_end == 2);
        assert(captured.used == 2 && captured.event[0].kind == 4 && captured.event[1].kind == 2);
        assert(captured.event[0].values[1] == (remove ? TRANS_DATA_DEL : TRANS_DATA));
        assert(captured.event[0].values[2] == 3 && captured.event[0].values[3] == 1 &&
               captured.event[0].values[4] == 2);
        assert(captured.event[1].values[0] == (STAT_TRNS | STAT_PLAY));
        ++cases;

        reset(); memset(&captured, 0, sizeof captured);
        cdb.trans_type = TRANS_PUT; cdb.cr3 = 0xff00;
        result = remove ? get_del_sector_data() : get_sector_data();
        assert(!result && cdb.trans_type == TRANS_PUT && captured.used == 1);
        assert(captured.event[0].kind == 2 && captured.event[0].values[0] == (STAT_WAIT | STAT_PLAY));
        ++cases;

        reset(); memset(&captured, 0, sizeof captured);
        cdb.part[3].numblocks = 2; cdb.cr3 = 3u << 8;
        result = remove ? get_del_sector_data() : get_sector_data();
        assert(!result && !cdb.trans_type && captured.used == 1);
        assert(captured.event[0].kind == 2 && captured.event[0].values[0] == (STAT_WAIT | STAT_PLAY));
        ++cases;
    }
    reset(); memset(&captured, 0, sizeof captured); log_mask = 0;
    cdb.cdir_lba = 512; cdb.cdir_size = 2; cdb.cr3 = 4u << 8; cdb.cr4 = 7;
    assert(read_dir() == 0 && cdb.cdir_pfid == 7 && cdb.rdir_state == 2);
    assert(cdb.cddev_filter == 4 && !cdb.pause_request && cdb.play_type == PLAYTYPE_DIR);
    assert(cdb.play_fad_start == 662 && cdb.play_fad_end == 664);
    assert(cdb.old_fad == 4321 && cdb.fad == 662);
    assert(cdb.filter[4].fad == 662 && cdb.filter[4].range == 2);
    assert(captured.used == 3 && captured.event[0].kind == 5 && captured.event[1].kind == 6);
    assert(captured.event[0].values[0] == 662 && captured.event[2].kind == 3);
    assert(captured.event[1].values[16] == 662 && captured.event[1].values[17] == 2 &&
           captured.event[1].values[18] == 0x40);
    ++cases;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "transfer")) transfers();
    else if (!strcmp(argv[1], "directory")) directories();
    else if (!strcmp(argv[1], "contracts")) contracts();
    else return 2;
    printf("pass: %u comparisons\n", cases);
    return 0;
}
