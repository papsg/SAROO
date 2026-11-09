/* Adapted from paps-g d05062a1: actual public service and FatFs, test-only faults. */
#define main startup_test_main
#include "mcu_save_startup.c"
#undef main

typedef struct { FIL *fp; int sector_size; u32 file_offset; u8 mode; } TRACK_INFO;
static struct { TRACK_INFO tracks[1]; } cdb;
#define f_read checked_read
#include "game-id.inc"
#undef f_read

/* Actual tail of load_disc, including its save call, cleanup and return. */
static int disc_finish(char *gameid)
{
    int retv = 0;
    int start_save = 1;
    char *fname = malloc(256); assert(fname);
    if (1) {
#include "disc-finish.inc"

static const char key_a[17] = "ABCDEFGH00000001";
static const char key_b[17] = "ABCDEFGH00000002";
static const char key_c[17] = "ABCDEFGH00000003";
static const char key_d[17] = "ABCDEFGH00000004";
static u8 *slot_before, *mems_before;
static FSIZE_t slot_size, mems_size;
static int fixture_trailer;

static void raw_write(FIL *fp, FSIZE_t offset, const void *data, UINT size)
{
    UINT count;
    assert(f_lseek(fp, offset) == FR_OK && f_tell(fp) == offset);
    assert(f_write(fp, data, size, &count) == FR_OK && count == size);
    assert(f_sync(fp) == FR_OK);
}
static u8 *read_archive(FIL *fp)
{
    UINT count; FSIZE_t size = f_size(fp);
    u8 *data = malloc(size); assert(data);
    assert(f_sync(fp) == FR_OK && f_lseek(fp, 0) == FR_OK);
    assert(f_read(fp, data, size, &count) == FR_OK && count == size);
    return data;
}
static void assert_archives(const u8 *slot, FSIZE_t size, const u8 *mems)
{
    assert(f_size(&ss_fp) == size && f_size(&sm_fp) == mems_size);
    u8 *data = read_archive(&ss_fp); assert(!memcmp(data, slot, size)); free(data);
    data = read_archive(&sm_fp); assert(!memcmp(data, mems, mems_size)); free(data);
}
static void clear_io(void)
{
    api_reads = api_writes = api_seeks = api_syncs = 0;
    read_bytes = write_bytes = 0;
    hits = fault_seen = 0;
}
static void initialize(void)
{
    assert(f_mkdir("/SAROO") == FR_OK);
    make_archive(0, 4 * 65536, 0); make_archive(1, 0x800000 + (fixture_trailer ? 37 : 0), 0);
    reset_service(); assert(open_savefile() == 0);
    u8 *data = malloc(65536); assert(data);
    for (int i = 1; i <= 3; i++) {
        memset(data, 0x11 * i, 65536); raw_write(&ss_fp, i * 65536, data, 65536);
    }
    raw_write(&ss_fp, 16, key_a, 16); raw_write(&ss_fp, 48, key_b, 16);
    for (int i = 0; i < 65536; i++) data[i] = (i / 1024 * 7 + i % 1024 * 3) % 251;
    /* Distinct full blocks, including the last legal 64 KiB window's tail. */
    for (int block = 8; block < 8192; block += 64) {
        UINT count = block + 64 <= 8192 ? 65536 : (8192 - block) * 1024;
        raw_write(&sm_fp, block * 1024, data, count);
        for (int i = 0; i < 65536; i++) data[i] = (data[i] + 17) % 251;
    }
    free(data);
    if (fixture_trailer) {
        u8 trailer[37]; for (int i=0;i<37;i++) trailer[i]=(i*13+7)%251;
        raw_write(&sm_fp,0x800000,trailer,sizeof(trailer));
    }
    mems_size = f_size(&sm_fp);
    slot_size = f_size(&ss_fp); slot_before = read_archive(&ss_fp); mems_before = read_archive(&sm_fp);
    clear_io();
}
static int expected_fault(void)
{
    return strstr(fault,"short") ? FR_INT_ERR : FR_DISK_ERR;
}
static void check_no_flush(void)
{
    unsigned n = api_writes;
    assert(flush_savefile() != 0 && api_writes == n && ss_index == 0);
}
static void slot_case(const char *mode)
{
    if (!strcmp(mode,"slot-not-loaded")) {
        assert(ss_index == 0); check_no_flush();
    } else if (!strcmp(mode,"slot-first") || !strcmp(mode,"slot-after-hole") || !strcmp(mode,"slot-unindexed-tail")) {
        int index = !strcmp(mode,"slot-after-hole") ? 3 : 1;
        if (!strcmp(mode,"slot-unindexed-tail")) {
            u8 tail=0xa5; raw_write(&ss_fp,slot_size,&tail,1);
            slot_before=realloc(slot_before,++slot_size); assert(slot_before); slot_before[slot_size-1]=tail;
            clear_io();
        }
        assert(load_savefile((char*)(index == 1 ? key_a : key_b)) == 0 && ss_index == index);
        assert(!memcmp((void*)SSAVE_ADDR,slot_before + index*65536,65536));
        assert(api_reads == 2 && read_bytes == 131072 && !api_writes);
    } else if (!strcmp(mode,"slot-create") || !strcmp(mode,"slot-append") || !strcmp(mode,"slot-append-tail") || !strcmp(mode,"slot-short-id")) {
        int index = 2;
        const char *key = key_c;
        if (!strcmp(mode,"slot-short-id")) key = "SHORT";
        if (!strcmp(mode,"slot-append") || !strcmp(mode,"slot-append-tail")) {
            if (!strcmp(mode,"slot-append-tail")) {
                u8 tail[37]; memset(tail,0xa5,sizeof(tail));
                raw_write(&ss_fp,slot_size,tail,sizeof(tail));
            }
            raw_write(&ss_fp,32,key_c,16); memcpy(slot_before+32,key_c,16);
            index = 4; key = key_d;
            slot_before = realloc(slot_before, 5*65536); assert(slot_before);
            slot_size = 5*65536;
        }
        assert(load_savefile((char*)key) == 0 && ss_index == index);
        memset(slot_before + index*65536,0,65536);
        memset(slot_before + index*16,0,16);
        memcpy(slot_before + index*16,key,strlen(key));
        assert(api_writes == 2 && write_bytes == 65552 && api_syncs == 2);
        assert(write_offsets[0] == (FSIZE_t)index*65536 && write_sizes[0] == 65536);
        assert(write_offsets[1] == (FSIZE_t)index*16 && write_sizes[1] == 16);
        clear_io(); assert(load_savefile((char*)key) == 0 && ss_index == index && !api_writes);
        assert(!memcmp((void*)SSAVE_ADDR,slot_before + index*65536,65536));
    } else {
        const char *key = key_b;
        if (!strcmp(mode,"slot-duplicate")) {
            raw_write(&ss_fp,32,key_b,16); memcpy(slot_before+32,key_b,16);
        } else if (!strcmp(mode,"slot-unbacked")) {
            raw_write(&ss_fp,64,key_c,16); memcpy(slot_before+64,key_c,16);
        } else if (!strcmp(mode,"slot-empty-id")) key = "";
        else if (!strcmp(mode,"slot-null-id")) key = NULL;
        else if (!strcmp(mode,"slot-signature-terminator")) {
            u8 value=0xa5; raw_write(&ss_fp,15,&value,1);slot_before[15]=value;
        } else if (!strcmp(mode,"slot-bad-signature")) {
            u8 value = 'X'; raw_write(&ss_fp,0,&value,1); slot_before[0] = value;
        } else assert(0);
        assert(load_savefile((char*)key) != 0 && ss_index == 0 && !api_writes);
        check_no_flush();
    }
    assert_archives(slot_before,slot_size,mems_before);
}
static void slot_read_fault(void)
{
    assert(load_savefile((char*)key_a) == 0 && ss_index == 1);
    clear_io(); armed = 1;
    assert(load_savefile((char*)key_b) == expected_fault() && ss_index == 0 && hits == 1);
    assert(!api_writes); check_no_flush(); armed = 0;
    assert_archives(slot_before,slot_size,mems_before);
    assert(load_savefile((char*)key_b) == 0 && ss_index == 3);
    assert(!memcmp((void*)SSAVE_ADDR,slot_before + 3*65536,65536));
}
static void slot_create_fault(void)
{
    armed = 1;
    assert(load_savefile((char*)key_c) == expected_fault() && ss_index == 0 && hits == 1);
    check_no_flush(); armed = 0;
    u8 *actual = read_archive(&ss_fp);
    int entry_attempted = api_writes == 2;
    if (entry_attempted) {
        /* Even a partially published ID can only refer to initialized data. */
        for (int i = 2*65536; i < 3*65536; i++) assert(actual[i] == 0);
    } else assert(!memcmp(actual,slot_before,65536));
    /* Only the selected free slot and its own directory entry may change. */
    memcpy(actual+32,slot_before+32,16);
    memcpy(actual+2*65536,slot_before+2*65536,65536);
    assert(!memcmp(actual,slot_before,slot_size)); free(actual);
    actual = read_archive(&sm_fp); assert(!memcmp(actual,mems_before,mems_size)); free(actual);
}
static void slot_flush(void)
{
    assert(load_savefile((char*)key_a) == 0); clear_io();
    memset((void*)SSAVE_ADDR,0xab,65536); armed = 1;
    int result = flush_savefile();
    assert((result == 0) == !strcmp(fault,"none"));
    if (result) assert(result == expected_fault());
    assert(ss_index == 1 && hits == (strcmp(fault,"none") != 0));
    armed = 0;
    u8 *actual = read_archive(&ss_fp);
    memcpy(actual+65536,slot_before+65536,65536);
    assert(!memcmp(actual,slot_before,slot_size)); free(actual);
    /* Retry still owns the same fully prepared buffer and destination. */
    assert(flush_savefile() == 0);
    memset(slot_before+65536,0xab,65536);
    assert_archives(slot_before,slot_size,mems_before);
}
static void mems_load_case(const char *mode, int id)
{
    int header = id & 0x8000;
    int flag = header ? 2 : 4;
    int size = header ? 1024 : 65536;
    u8 *buffer = header ? (u8*)SMEMS_HDR + 8192 : (u8*)SMEMS_BUF;
    int *index = header ? &sm_index0 : &sm_index1;
    if (!strcmp(mode,"mems-read-fault") || !strcmp(mode,"mems-invalid")) {
        assert(load_smems((header ? 0x8000 : 0) | 64) == 0 && *index == 64);
        clear_io(); armed = !strcmp(mode,"mems-read-fault");
        assert(load_smems(id) == (armed ? expected_fault() : FR_INVALID_PARAMETER) && *index == 0);
        assert(!api_writes && hits == armed);
        assert(flush_smems(flag) != 0 && !api_writes);
        armed = 0;
        if (!strcmp(mode,"mems-read-fault")) {
            assert(load_smems(id) == 0 && *index == (id & 0x7fff));
            assert(!memcmp(buffer,mems_before+(id&0x7fff)*1024,size));
        }
    } else {
        assert(load_smems(id) == 0 && *index == (id & 0x7fff));
        assert(api_reads == 1 && read_bytes == (unsigned)size && !api_writes);
        assert(!memcmp(buffer,mems_before+(id&0x7fff)*1024,size));
    }
    assert_archives(slot_before,slot_size,mems_before);
}
static void mems_prepare(int overlap)
{
    assert(load_smems(64) == 0 && load_smems(0x8000 | (overlap ? 70 : 200)) == 0);
    memset((void*)SMEMS_BUF,0xb4,65536);
    memset((u8*)SMEMS_HDR+8192,0xa2,1024);
    ((u8*)SMEMS_HDR)[12] ^= 0x11;
    clear_io();
}
static void mems_expected(int flags, int overlap)
{
    if (flags & 4) {
        memset(mems_before+64*1024,0xb4,65536);
        if (overlap) memset(mems_before+70*1024,0xa2,1024);
    }
    if (flags & 2) memset(mems_before+(overlap ? 70 : 200)*1024,0xa2,1024);
    if (flags & 1) mems_before[12] ^= 0x11;
}
static void mems_flush_case(const char *mode, int value)
{
    if (!strcmp(mode,"mems-flush-invalid")) {
        assert(flush_smems(value) != 0 && !api_writes && !api_seeks && !api_syncs);
    } else if (!strcmp(mode,"mems-coherence")) {
        mems_prepare(1);
        assert(flush_smems(2) == 0);
        assert(load_smems(0x8000 | 200) == 0);
        assert(flush_smems(4) == 0);
        mems_expected(6,1);
    } else {
        int overlap = value & 16, flags = value & 7;
        mems_prepare(overlap);
        armed = !strcmp(mode,"mems-flush-fault");
        int result = flush_smems(flags);
        assert((result != 0) == armed && hits == armed);
        if (result) assert(result == expected_fault());
        assert(sm_index0 == (overlap ? 70 : 200) && sm_index1 == 64);
        if (armed) {
            unsigned n = api_writes;
            /* No later write or sync after the first failing operation. */
            if (!strcmp(fault,"sync-error")) assert(n == 3 && api_syncs == 1);
            else {
                assert(api_syncs == 0);
                assert(n == (unsigned)(fault_nth - (!strncmp(fault,"seek-",5) ? 1 : 0)));
            }
            u8 *actual = read_archive(&sm_fp);
            if (n < 3) assert(!memcmp(actual,mems_before,8192));
            /* Check every unaffected byte, including the other archive. */
            memcpy(actual,mems_before,8192);
            memcpy(actual+64*1024,mems_before+64*1024,65536);
            memcpy(actual+200*1024,mems_before+200*1024,1024);
            assert(!memcmp(actual,mems_before,mems_size)); free(actual);
            armed = 0; assert(flush_smems(flags) == 0);
        } else assert(api_syncs == 1);
        mems_expected(flags,overlap);
    }
    assert_archives(slot_before,slot_size,mems_before);
}
static void disc_id_case(int sector_size)
{
    u8 header[256]; memset(header,0,256);
    int offset = sector_size == 2048 ? 32 : 48;
    memcpy(header+offset,key_a,16);
    raw_write(&ss_fp,65536,header,256);
    cdb.tracks[0].fp = &ss_fp; cdb.tracks[0].sector_size = sector_size;
    cdb.tracks[0].file_offset = 65536; cdb.tracks[0].mode = 1;
    assert(f_lseek(&ss_fp,65536) == 0);
    char id[17]; memset(id,0xcc,17);
    require_lock = 0; armed = 1;
    int result = get_gameid(id);
    assert((result == 0) == !strcmp(fault,"none"));
    if (!result) assert(!memcmp(id,key_a,17));
    else for (int i=0;i<17;i++) assert((u8)id[i] == 0xcc);
}
static void disk_error_case(int kind)
{
    FIL *fp = kind ? &sm_fp : &ss_fp;
    int id = kind == 1 ? 0x8064 : 100;
    disk_read_fault = 1;
    int result = kind ? load_smems(id) : load_savefile((char*)key_b);
    assert(result == FR_DISK_ERR && !disk_read_fault && fp->err == FR_DISK_ERR);
    result = kind ? load_smems(id) : load_savefile((char*)key_b);
    assert(result == FR_DISK_ERR && !api_writes);
    /* Recovery is explicit in this harness; production does not reopen yet. */
    assert(f_close(fp) == FR_OK);
    assert(f_open(fp,kind ? SMEMS_FILE : SSAVE_FILE,FA_READ|FA_WRITE) == FR_OK);
    result = kind ? load_smems(id) : load_savefile((char*)key_b);
    assert(result == FR_OK);
    assert_archives(slot_before,slot_size,mems_before);
}
static void large_slot_case(const char *mode)
{
    assert(f_mkdir("/SAROO") == FR_OK);
    int existing = strcmp(mode,"large-last-new") != 0;
    make_archive(0,existing ? 0x10000000 : 0x0fff0000,0);
    make_archive(1,0x800000,0);
    reset_service(); assert(open_savefile() == 0);
    u8 directory[65536]; fill_header(directory,0);
    for (int i=1;i<(existing ? 4096 : 4095);i++)
        snprintf((char*)directory+i*16,16,"ID%013d",i);
    if (existing) memcpy(directory+4095*16,key_c,16);
    raw_write(&ss_fp,0,directory,65536);
    unsigned writes = disk_writes; clear_io();
    int result = load_savefile((char*)(!strcmp(mode,"large-full") ? key_d : key_c));
    if (!strcmp(mode,"large-full")) {
        assert(result == FR_DENIED && ss_index == 0 && !api_writes && disk_writes == writes);
        check_no_flush();
    } else {
        assert(result == 0 && ss_index == 4095);
        assert(f_size(&ss_fp) == 0x10000000);
        if (!existing) {
            memcpy(directory+4095*16,key_c,16);
            for (int i=0;i<65536;i++) assert(((u8*)SSAVE_ADDR)[i] == 0);
            assert(api_writes == 2 && api_syncs == 2);
        } else assert(!api_writes && disk_writes == writes);
    }
    UINT count; assert(f_lseek(&ss_fp,0) == 0);
    assert(f_read(&ss_fp,test_scratch,65536,&count) == 0 && count == 65536);
    assert(!memcmp(test_scratch,directory,65536));
    /* An out-of-range selected slot must not expand the archive on flush. */
    ss_index = 4096; clear_io(); assert(flush_savefile() != 0 && !api_writes && !api_seeks);
}
#ifndef MCU_RUNTIME_TEST_ENTRY
#define MCU_RUNTIME_TEST_ENTRY main
#endif
int MCU_RUNTIME_TEST_ENTRY(int argc, char **argv)
{
    assert(argc == 6);
    const char *mode = argv[2]; int value = atoi(argv[3]);
    disk = fopen(argv[1],"r+b"); assert(disk);
    if (!strncmp(mode,"large-",6)) disk_sectors = 1048576;
    FATFS fs; assert(f_mount(&fs,"0:",1) == FR_OK);
    fault = argv[4]; fault_nth = atoi(argv[5]);
    fault_target = !strncmp(mode,"mems-",5);
    if (!strncmp(mode,"large-",6)) large_slot_case(mode);
    else {
    fixture_trailer = !strncmp(mode,"mems-trailer-",13);
    initialize();
    if (!strcmp(mode,"locking")) {
        require_lock = 1;
        assert(load_savefile((char*)key_a) == 0 && flush_savefile() == 0);
        assert(load_smems(64) == 0 && load_smems(0x8000 | 200) == 0 && flush_smems(7) == 0);
    }
    else if (!strcmp(mode,"slot-read-fault")) slot_read_fault();
    else if (!strcmp(mode,"slot-create-fault")) slot_create_fault();
    else if (!strcmp(mode,"slot-flush")) slot_flush();
    else if (!strncmp(mode,"slot-",5)) slot_case(mode);
    else if (!strcmp(mode,"mems-trailer-load")) mems_load_case("mems-load",value);
    else if (!strcmp(mode,"mems-trailer-flush")) mems_flush_case("mems-flush",value);
    else if (!strncmp(mode,"mems-flush",10) || !strcmp(mode,"mems-coherence")) mems_flush_case(mode,value);
    else if (!strncmp(mode,"mems-",5)) mems_load_case(mode,value);
    else if (!strcmp(mode,"disc-id")) disc_id_case(value);
    else if (!strcmp(mode,"disc-finish")) {
        if (value == 2) { fault = "read-error"; armed = 1; }
        int result = disc_finish(value == 1 ? NULL : (char*)key_b);
        assert(result == (value == 0 ? FR_OK : value == 1 ? FR_INVALID_PARAMETER : FR_DISK_ERR));
        armed = 0; assert_archives(slot_before,slot_size,mems_before);
    }
    else if (!strcmp(mode,"disk-read-fault")) disk_error_case(value);
    else assert(0);
    }
    assert(!lock_depth);
    printf("runtime %s value=%d fault=%s nth=%d reads=%u bytes=%u writes=%u bytes=%u syncs=%u\n",
           mode,value,fault,fault_nth,api_reads,read_bytes,api_writes,write_bytes,api_syncs);
    assert(f_close(&ss_fp) == 0 && f_close(&sm_fp) == 0);
    free(slot_before); free(mems_before); assert(!fclose(disk)); return 0;
}
