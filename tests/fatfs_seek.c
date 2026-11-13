/* Adapted from paps-g 60d5df01. Actual public FatFs on synthetic host files only. */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
static FILE *disk;
static LBA_t sectors;
static LBA_t boot_alias;
static unsigned reads,writes,syncs,failures;
enum Media { MEDIA_READY, MEDIA_ABSENT, MEDIA_DIFFERENT, MEDIA_RETURNED };
static enum Media media;
static unsigned reidentifications;
static LBA_t fault_sector;
static UINT fault_prefix;
static unsigned fail_read;
static int fail_write,fail_sync;
static struct {
    const BYTE *payload,*cache;
    UINT bytes;
    unsigned calls,fail_at;
    LBA_t first_sector;
    int persistent;
} abort_fault;
static pthread_mutex_t mutex;
int ff_mutex_create(int vol) { assert(!vol);return pthread_mutex_init(&mutex,NULL)==0; }
void ff_mutex_delete(int vol) { assert(!vol);assert(!pthread_mutex_destroy(&mutex)); }
int ff_mutex_take(int vol) { assert(!vol);return pthread_mutex_lock(&mutex)==0; }
void ff_mutex_give(int vol) { assert(!vol);assert(!pthread_mutex_unlock(&mutex)); }
DSTATUS disk_status(BYTE drive) { return drive || media != MEDIA_READY ? STA_NOINIT : 0; }
DSTATUS disk_initialize(BYTE drive)
{
    if (!drive && media == MEDIA_RETURNED) {
        reidentifications++;
        media = MEDIA_READY;
    }
    return disk_status(drive);
}
DRESULT disk_read(BYTE drive,BYTE *out,LBA_t sector,UINT count)
{
    /* One synthetic partition boot sector near the LBA limit. */
    if (boot_alias && sector == boot_alias) { assert(count == 1); sector = 1; }
    assert(!drive && media == MEDIA_READY && count && sector<sectors && count<=sectors-sector);reads++;
    if (reads == fail_read) {
        failures++;
        return RES_ERROR;
    }
    assert(!fseeko(disk,(off_t)sector*512,SEEK_SET));return fread(out,512,count,disk)==count?RES_OK:RES_ERROR;
}
DRESULT disk_write(BYTE drive,const BYTE *data,LBA_t sector,UINT count)
{
    assert(!drive && media == MEDIA_READY && count && sector+count<=sectors);writes++;
    if (abort_fault.payload) {
        uintptr_t ptr=(uintptr_t)data, start=(uintptr_t)abort_fault.payload;
        int matches=abort_fault.cache ? data==abort_fault.cache :
                    ptr>=start && ptr-start<abort_fault.bytes;
        if (matches) {
            if (!abort_fault.calls++) abort_fault.first_sector=sector;
            if (abort_fault.calls==abort_fault.fail_at ||
                (abort_fault.persistent && abort_fault.calls>abort_fault.fail_at)) {
                failures++;
                return RES_ERROR;
            }
        }
    }
    if (fail_write && fault_sector >= sector && fault_sector - sector < count) {
        if (fault_prefix) {
            const BYTE *source = data + (fault_sector - sector) * 512;
            assert(fault_prefix < 512 && !fseeko(disk, (off_t)fault_sector * 512, SEEK_SET));
            assert(fwrite(source, 1, fault_prefix, disk) == fault_prefix && !fflush(disk));
        }
        failures++;
        return RES_ERROR;
    }
    assert(!fseeko(disk,(off_t)sector*512,SEEK_SET));return fwrite(data,512,count,disk)==count?RES_OK:RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive,BYTE cmd,void *out)
{
    assert(!drive && media == MEDIA_READY);
    if (cmd == CTRL_SYNC) {
        syncs++;
        if (fail_sync) {
            failures++;
            return RES_ERROR;
        }
        return fflush(disk) ? RES_ERROR : RES_OK;
    }
    if(cmd==GET_SECTOR_COUNT){*(LBA_t*)out=sectors;return RES_OK;}
    if(cmd==GET_BLOCK_SIZE){*(DWORD*)out=1;return RES_OK;}
    assert(0);return RES_PARERR;
}
static int selected(FSIZE_t ofs,FSIZE_t ifptr,DWORD bcs)
{
#include "seek.inc"
}
static uint64_t random_word(uint64_t *state)
{ *state^=*state<<13;*state^=*state>>7;*state^=*state<<17;return *state; }
static void comparison(void)
{
    const uint64_t points[]={0,1,2,511,512,513,65535,65536,65537,0xffffffffULL,
        0x100000000ULL,0x100000001ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,UINT64_MAX};
    uint64_t state=0x724baf908197ULL;unsigned cases=0;
    for(unsigned bit=0;bit<32;bit++) {
        uint32_t bcs=1u<<bit;
        for(unsigned a=0;a<sizeof(points)/sizeof(points[0]);a++)
            for(unsigned b=0;b<sizeof(points)/sizeof(points[0]);b++) {
                uint64_t ofs=points[a],prev=points[b];
                int ref=ofs && prev && (ofs-1)/bcs>=(prev-1)/bcs;
                assert(selected(ofs,prev,bcs)==ref);cases++;
            }
        for(unsigned i=0;i<10000;i++) {
            uint64_t ofs=random_word(&state),prev=random_word(&state);
            int ref=ofs && prev && (ofs-1)/bcs>=(prev-1)/bcs;
            assert(selected(ofs,prev,bcs)==ref);cases++;
        }
    }
    printf("64-bit cluster comparisons=%u\n",cases);
}
static void format_volume(BYTE type, BYTE copies, DWORD cluster_bytes)
{
    BYTE work[32768];
    MKFS_PARM options = {.fmt = type | FM_SFD, .n_fat = copies, .au_size = cluster_bytes};
    assert(f_mkfs("0:",&options,work,sizeof(work))==FR_OK);
}
static BYTE pattern(unsigned offset) { return (offset*19u + (offset>>16)*71u)&255; }
static void fragmented(void)
{
    FATFS fs;FIL file,other;UINT count;BYTE block[65536],got[129],sector[512];
    assert(!f_mount(&fs,"0:",1));
    assert(!f_open(&file,"/fragment.bin",FA_CREATE_ALWAYS|FA_READ|FA_WRITE));
    assert(!f_open(&other,"/neighbor.bin",FA_CREATE_ALWAYS|FA_READ|FA_WRITE));
    for(unsigned n=0;n<8;n++) {
        for(unsigned i=0;i<sizeof(block);i++)block[i]=pattern(n*sizeof(block)+i);
        assert(!f_write(&file,block,sizeof(block),&count) && count==sizeof(block));
        memset(block,0xc9,sizeof(block));
        assert(!f_write(&other,block,sizeof(block),&count) && count==sizeof(block));
    }
    assert(!f_sync(&file) && !f_sync(&other));
    /* Observe a non-adjacent FAT link at the first interleaved extent boundary. */
    DWORD cluster=file.obj.sclust+sizeof(block)/(fs.csize*512)-1;
    assert(disk_read(0,sector,fs.fatbase+cluster*4/512,1)==RES_OK);
    unsigned at=cluster*4%512;
    DWORD next=(DWORD)sector[at]|(DWORD)sector[at+1]<<8|(DWORD)sector[at+2]<<16|(DWORD)sector[at+3]<<24;
    next&=0x0fffffff;assert(next>1 && next<0x0ffffff0 && next!=cluster+1);
    assert(!f_close(&file) && !f_close(&other) && !f_mount(NULL,"0:",0));
    assert(!f_mount(&fs,"0:",1) && !f_open(&file,"/fragment.bin",FA_READ));
    const unsigned points[]={0,1,511,512,513,65535,65536,65537,131071,131072,262143,458751,524287,524288};
    unsigned num=sizeof(points)/sizeof(points[0]);
    for(unsigned pass=0;pass<3;pass++)for(unsigned n=0;n<num;n++) {
        unsigned pos=points[pass==0?n:pass==1?num-1-n:n*3%num];
        assert(!f_lseek(&file,pos) && f_tell(&file)==pos && !f_read(&file,got,sizeof(got),&count));
        unsigned expected=8*sizeof(block)-pos;if(expected>sizeof(got))expected=sizeof(got);
        assert(count==expected);for(unsigned i=0;i<count;i++)assert(got[i]==pattern(pos+i));
    }
    assert(!f_close(&file) && !f_open(&other,"/neighbor.bin",FA_READ));
    for(unsigned n=0;n<8;n++) {
        assert(!f_read(&other,block,sizeof(block),&count) && count==sizeof(block));
        for(unsigned i=0;i<sizeof(block);i++)assert(block[i]==0xc9);
    }
    unsigned type=fs.fs_type;
    assert(!f_close(&other) && !f_mount(NULL,"0:",0));
    printf("fragmented fs=%u nonadjacent_link=%u->%u positions=%u reads=%u writes=%u\n",type,cluster,next,num,reads,writes);
}
#if FF_FS_EXFAT
static void seek_file(void)
{
    FATFS fs;FIL file,other;UINT count;BYTE marker;
    assert(f_mount(&fs,"0:",1)==0);
    assert(f_open(&other,"/neighbor.bin",FA_CREATE_ALWAYS|FA_WRITE)==0);
    BYTE neighbor[8192],actual[8192];for(unsigned i=0;i<sizeof(neighbor);i++)neighbor[i]=i*17;
    assert(f_write(&other,neighbor,sizeof(neighbor),&count)==0 && count==sizeof(neighbor) && f_close(&other)==0);
    assert(f_open(&file,"/wide.bin",FA_CREATE_ALWAYS|FA_READ|FA_WRITE)==0);
    const FSIZE_t end=5ULL*1024*1024*1024+128;
    assert(f_lseek(&file,end)==0 && f_tell(&file)==end);marker=0xb7;
    assert(f_write(&file,&marker,1,&count)==0 && count==1);
    const FSIZE_t points[]={0,1,65535,65536,65537,0xffffffffULL,0x100000000ULL,
        0x100000001ULL,5ULL*1024*1024*1024-1,5ULL*1024*1024*1024,end};
    unsigned num=sizeof(points)/sizeof(points[0]);
    for(unsigned i=0;i<num;i++) {
        marker=i+0x40;assert(f_lseek(&file,points[i])==0 && f_tell(&file)==points[i]);
        assert(f_write(&file,&marker,1,&count)==0 && count==1);
    }
    assert(f_sync(&file)==0 && f_close(&file)==0 && f_mount(NULL,"0:",0)==0);
    assert(f_mount(&fs,"0:",1)==0 && f_open(&file,"/wide.bin",FA_READ)==0);
    assert(f_size(&file)==end+1);
    for(unsigned pass=0;pass<3;pass++)for(unsigned n=0;n<num;n++) {
        unsigned i=pass==0?n:pass==1?num-n-1:(n*3)%num;
        assert(f_lseek(&file,points[i])==0 && f_tell(&file)==points[i]);
        assert(f_read(&file,&marker,1,&count)==0 && count==1 && marker==i+0x40);
    }
    assert(f_lseek(&file,UINT64_MAX)==0 && f_tell(&file)==f_size(&file));
    assert(f_read(&file,&marker,1,&count)==0 && !count && f_close(&file)==0);
    assert(f_open(&other,"/neighbor.bin",FA_READ)==0);
    assert(f_read(&other,actual,sizeof(actual),&count)==0 && count==sizeof(actual) && !memcmp(neighbor,actual,sizeof(actual)));
    assert(f_close(&other)==0 && f_mount(NULL,"0:",0)==0);
    printf("exFAT wide file bytes=%llu marker_positions=%u reads=%u writes=%u\n",(unsigned long long)end+1,num,reads,writes);
}
#endif

static void mount_geometry(int value)
{
    if(value!=512) {
        assert(!fseeko(disk,value<256?108:109,SEEK_SET));assert(fputc(value&255,disk)!=EOF && !fflush(disk));
    }
    FATFS fs;FRESULT result=f_mount(&fs,"0:",1);
    if(value==512 || value==9)assert(result==FR_OK);
    else assert(result==FR_NO_FILESYSTEM);
    assert(f_mount(NULL,"0:",0)==0);
    printf("mount geometry=%d result=%d\n",value,result);
}

static void check_clean_sync(FIL *file)
{
    unsigned old_writes = writes, old_syncs = syncs;
    assert(f_sync(file) == FR_OK);
    assert(writes == old_writes && syncs == old_syncs);
}

static void create_file(const char *name, const BYTE *data, UINT size)
{
    FIL file;
    UINT count;
    assert(f_open(&file, name, FA_CREATE_NEW | FA_WRITE) == FR_OK);
    assert(f_write(&file, data, size, &count) == FR_OK && count == size);
    assert(f_close(&file) == FR_OK);
}

static void verify_file(const char *name, const BYTE *expected, UINT size)
{
    FIL file;
    BYTE data[1024];
    UINT count;
    assert(f_open(&file, name, FA_READ) == FR_OK && f_size(&file) == size);
    for (UINT offset = 0; offset < size; offset += count) {
        assert(f_read(&file, data, sizeof(data), &count) == FR_OK && count);
        assert(count <= size - offset && !memcmp(data, expected + offset, count));
    }
    assert(f_close(&file) == FR_OK);
}

/* Adapted from paps-g 6e52b09c. Exercise filename decoding through f_open. */
static void utf8_path(int fat32, unsigned scenario)
{
    static const char *const valid[] = {
        "/ascii.txt", "/\xC2\x80-a", "/\xDF\xBF-b",
        "/\xE0\xA0\x80-c", "/\xED\x9F\xBF-d", "/\xEE\x80\x80-e",
        "/\xF0\x90\x80\x80-f", "/\xF4\x8F\xBF\xBF-g"
    };
    static const char *const nonshortest[] = {
        "/\xE0\x82\x80-a", "/\xE0\x9F\xBF-b",
        "/\xF0\x80\x82\x80-c", "/\xF0\x8F\xBF\xBF-d"
    };
    static const char *const malformed[] = {
        "/\xC0\x80-m", "/\xE0\x81\xBF-m", "/\xE0\xA0-m",
        "/\xE0\x41\x80-m", "/\xED\xA0\x80-m", "/\xF4\x90\x80\x80-m",
        "/\xF5\x80\x80\x80-m", "/\x80-m"
    };
    const BYTE marker = 0xa6;
    format_volume(fat32 ? FM_FAT32 : FM_EXFAT, 1, fat32 ? 512 : 65536);
    FATFS fs;
    assert(f_mount(&fs, "0:", 1) == FR_OK && fs.fs_type == (fat32 ? FS_FAT32 : FS_EXFAT));

    if (scenario == 0) {
        for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
            create_file(valid[i], &marker, 1);
        assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
        for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
            verify_file(valid[i], &marker, 1);
            assert(f_unlink(valid[i]) == FR_OK);
        }
    } else {
        const unsigned count = sizeof(nonshortest) / sizeof(nonshortest[0]);
        assert(scenario <= count + sizeof(malformed) / sizeof(malformed[0]));
        const char *name = scenario <= count ? nonshortest[scenario - 1] : malformed[scenario - count - 1];
        FIL file;
        unsigned before = writes;
        FRESULT result = f_open(&file, name, FA_CREATE_NEW | FA_WRITE);
        if (result != FR_INVALID_NAME || writes != before)
            fprintf(stderr, "UTF-8 filename case %u: f_open=%d, writes=%u\n",
                    scenario, result, writes - before);
        assert(result == FR_INVALID_NAME && writes == before);
    }
    assert(f_mount(NULL, "0:", 0) == FR_OK);
}

/* Adapted from paps-g 1816d52c. Exercise the public f_sync/f_close paths. */
static void sync_file(int fat32, int value)
{
    enum { DIRECTORY, DEVICE, CLOSE_DIRECTORY, CLOSE_DEVICE, CLEAN };
    assert(value >= DIRECTORY && value <= CLEAN);
    format_volume(fat32 ? FM_FAT32 : FM_EXFAT, 1, fat32 ? 512 : 65536);
    FATFS fs;
    FIL file;
    UINT count;
    BYTE payload[8192 + 1025], neighbor[2048];
    for (unsigned i = 0; i < sizeof(payload); i++)
        payload[i] = pattern(i + 7 * (i / 512));
    memset(neighbor, 0xc9, sizeof(neighbor));
    assert(f_mount(&fs, "0:", 1) == FR_OK && fs.fs_type == (fat32 ? FS_FAT32 : FS_EXFAT));
    create_file("/neighbor.bin", neighbor, sizeof(neighbor));
    assert(f_open(&file, "/save.bin", FA_CREATE_NEW | FA_READ | FA_WRITE) == FR_OK);
    assert(f_write(&file, payload, 8192, &count) == FR_OK && count == 8192);
    assert(f_sync(&file) == FR_OK);
    check_clean_sync(&file);
#if FF_FS_EXFAT
    if (!fat32) {
        DWORD cluster = file.obj.c_scl ? file.obj.c_scl : (DWORD)fs.dirbase;
        fault_sector = fs.database + (cluster - 2) * fs.csize + file.obj.c_ofs / 512;
    } else
#endif
    {
        fault_sector = file.dir_sect;
    }
    assert(f_write(&file, payload + 8192, 1025, &count) == FR_OK && count == 1025);
    if (value != CLEAN) {
        fail_write = value == DIRECTORY || value == CLOSE_DIRECTORY;
        fail_sync = value == DEVICE || value == CLOSE_DEVICE;
        int close = value == CLOSE_DIRECTORY || value == CLOSE_DEVICE;
        assert(f_sync(&file) == FR_DISK_ERR && failures == 1 && file.obj.fs == &fs);
        FRESULT result = close ? f_close(&file) : f_sync(&file);
        if (result != FR_DISK_ERR || failures != 2 || file.obj.fs != &fs) {
            fprintf(stderr, "Lost pending sync: result=%u, failures=%u, open=%d\n",
                    (unsigned)result, failures, file.obj.fs == &fs);
            abort();
        }
        fail_write = fail_sync = 0;
    }
    if (value == CLOSE_DIRECTORY || value == CLOSE_DEVICE) {
        assert(f_close(&file) == FR_OK && !file.obj.fs);
    } else {
        assert(f_sync(&file) == FR_OK);
        check_clean_sync(&file);
        assert(f_close(&file) == FR_OK);
    }
    assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
    verify_file("/save.bin", payload, sizeof(payload));
    verify_file("/neighbor.bin", neighbor, sizeof(neighbor));
    assert(f_mount(NULL, "0:", 0) == FR_OK);
    printf("sync fs=%s case=%d writes=%u syncs=%u failures=%u\n",
           fat32 ? "fat32" : "exfat", value, writes, syncs, failures);
}

static void compare_fats(const FATFS *fs)
{
    BYTE primary[512], mirror[512];
    assert(fs->n_fats == 2);
    for (LBA_t i = 0; i < fs->fsize; i++) {
        assert(disk_read(0, primary, fs->fatbase + i, 1) == RES_OK);
        assert(disk_read(0, mirror, fs->fatbase + fs->fsize + i, 1) == RES_OK);
        assert(!memcmp(primary, mirror, sizeof(primary)));
    }
}

static DWORD little_word(const BYTE *data)
{
    return (DWORD)data[0] | (DWORD)data[1] << 8 | (DWORD)data[2] << 16 | (DWORD)data[3] << 24;
}

/* Adapted from paps-g a81828e4. Normal allocation supplies the pending FAT sector. */
static void sync_metadata(BYTE type, int value)
{
    enum { MIRROR, PRIMARY, SINGLE, FSINFO, PARTIAL_MIRROR, PARTIAL_PRIMARY, PARTIAL_FSINFO };
    assert(value >= MIRROR && value <= PARTIAL_FSINFO);
    int fat32 = type == FS_FAT32;
    int single = value == SINGLE;
    int fsinfo = value == FSINFO || value == PARTIAL_FSINFO;
    int mirror = value == MIRROR || value == PARTIAL_MIRROR;
    assert(!fsinfo || fat32);
    format_volume(fat32 ? FM_FAT32 : FM_FAT, single ? 1 : 2, 512);
    FATFS fs;
    FIL file;
    FILINFO info;
    UINT count;
    BYTE payload[8192], neighbor[2048];
    for (unsigned i = 0; i < sizeof(payload); i++)
        payload[i] = pattern(i + 7 * (i / 512));
    memset(neighbor, 0xc9, sizeof(neighbor));
    assert(f_mount(&fs, "0:", 1) == FR_OK && fs.fs_type == type);
    assert(fs.n_fats == (single ? 1 : 2));
    create_file("/neighbor.bin", neighbor, sizeof(neighbor));
    assert(f_open(&file, "/save.bin", FA_CREATE_NEW | FA_READ | FA_WRITE) == FR_OK);
    assert(f_lseek(&file, sizeof(payload)) == FR_OK && f_tell(&file) == sizeof(payload));
    assert(fs.wflag && fs.winsect - fs.fatbase < fs.fsize);
    LBA_t pending_sector = fs.winsect;
    BYTE pending[512];
    memcpy(pending, fs.win, sizeof(pending));

    fault_sector = fsinfo ? fs.volbase + 1 : pending_sector + (mirror ? fs.fsize : 0);
    fault_prefix = value >= PARTIAL_MIRROR ? (fsinfo ? 490 : 17) : 0;
    fail_write = !single;
    unsigned before_writes = writes, before_syncs = syncs;
    FRESULT result = f_sync(&file);
    if (single) {
        assert(result == FR_OK && !failures && syncs == before_syncs + 1);
        assert(writes - before_writes == (fat32 ? 3u : 2u));
    } else {
        if (result != FR_DISK_ERR || failures != 1) {
            fprintf(stderr, "Lost metadata error: type=%u, case=%d, result=%u, failures=%u\n",
                    (unsigned)type, value, (unsigned)result, failures);
            abort();
        }
        assert(syncs == before_syncs && file.obj.fs == &fs);
        if (fsinfo) {
            /* FSInfo hints can be regenerated after another sector uses the cache. */
            assert(fs.fsi_flag == 1 && !fs.wflag);
            assert(f_stat("/neighbor.bin", &info) == FR_OK && info.fsize == sizeof(neighbor));
            assert(failures == 1 && fs.winsect != fault_sector);
            assert(f_sync(&file) == FR_DISK_ERR && failures == 2);
        } else {
            unsigned attempts = mirror ? 2 : 1;
            assert(writes - before_writes == attempts);
            assert(fs.wflag && fs.winsect == pending_sector && !memcmp(fs.win, pending, sizeof(pending)));
            before_writes = writes;
            assert(f_stat("/neighbor.bin", &info) == FR_DISK_ERR && failures == 2);
            assert(writes - before_writes == attempts);
            assert(fs.wflag && fs.winsect == pending_sector && !memcmp(fs.win, pending, sizeof(pending)));
        }
        assert(f_close(&file) == FR_DISK_ERR && failures == 3 && file.obj.fs == &fs);
        assert(syncs == before_syncs);
        fail_write = 0;
        assert(f_sync(&file) == FR_OK && syncs == before_syncs + 1);
    }
    check_clean_sync(&file);
    assert(f_lseek(&file, 0) == FR_OK);
    assert(f_write(&file, payload, sizeof(payload), &count) == FR_OK && count == sizeof(payload));
    assert(f_close(&file) == FR_OK);
    DWORD free_clusters = fs.free_clst, last_cluster = fs.last_clst;
    if (fat32) {
        BYTE sector[512];
        assert(disk_read(0, sector, fs.volbase + 1, 1) == RES_OK);
        assert(little_word(sector) == 0x41615252 && little_word(sector + 484) == 0x61417272);
        assert(little_word(sector + 488) == free_clusters && little_word(sector + 492) == last_cluster);
        assert(sector[510] == 0x55 && sector[511] == 0xaa);
    }
    assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
    verify_file("/save.bin", payload, sizeof(payload));
    verify_file("/neighbor.bin", neighbor, sizeof(neighbor));
    if (fat32) assert(fs.free_clst == free_clusters && fs.last_clst == last_cluster);
    if (!single) compare_fats(&fs);
    assert(f_mount(NULL, "0:", 0) == FR_OK);
    printf("metadata type=%u case=%d writes=%u syncs=%u failures=%u\n",
           (unsigned)type, value, writes, syncs, failures);
}

/* Adapted from paps-g 0e259a3a. Fail after real allocation or cached writes. */
static void aborted_write(int fat32, int value)
{
    enum { CACHED, EXTEND, FRAGMENTED, FIRST, DENIED, EMPTY };
    assert(value>=CACHED && value<=EMPTY);
    int cached=value==CACHED, fragmented=value==FRAGMENTED;
    format_volume(fat32 ? FM_FAT32 : FM_EXFAT, 1, fat32 ? 512 : 65536);
    FATFS fs;
    FIL file;
    UINT count;
    assert(f_mount(&fs,"0:",1)==FR_OK && fs.fs_type==(fat32 ? FS_FAT32 : FS_EXFAT));
    unsigned cluster_bytes=fs.csize*512, old_size=2*cluster_bytes;
    unsigned payload_bytes=2*cluster_bytes, position=cached ? 1 : old_size;
    BYTE *expected=malloc(old_size+payload_bytes), *payload=malloc(payload_bytes);
    assert(expected && payload);
    memset(expected,0xc9,old_size+payload_bytes);
    BYTE neighbor[2048];
    memset(neighbor,0x5e,sizeof(neighbor));
    create_file("/neighbor.bin",neighbor,sizeof(neighbor));
    create_file("/save.bin",expected,old_size);
    if (fragmented) create_file("/between.bin",expected,cluster_bytes);
    for (unsigned i=0;i<payload_bytes;i++) payload[i]=pattern(i+7*(i/512));
    assert(f_open(&file,"/save.bin",FA_READ | (value==DENIED ? 0 : FA_WRITE))==FR_OK);
    assert(f_lseek(&file,position)==FR_OK);
    LBA_t next_contiguous=fs.database+(LBA_t)(file.clust-1)*fs.csize;
    abort_fault.payload=payload;
    abort_fault.cache=cached ? fs.win : NULL;
    abort_fault.bytes=payload_bytes;
    abort_fault.fail_at=cached || value==FIRST ? 1 : 2;
    abort_fault.persistent=cached;
    FRESULT result=f_write(&file,payload,value==EMPTY ? 0 : payload_bytes,&count);
    unsigned completed=count;
    if (value==DENIED || value==EMPTY) {
        assert(result==(value==DENIED ? FR_DENIED : FR_OK));
        assert(!completed && !abort_fault.calls);
    } else {
        assert(result==FR_DISK_ERR && failures==1 && file.err==FR_DISK_ERR);
        assert(completed==(cached ? 511 : value==FIRST ? 0 : cluster_bytes));
        if (!cached) {
            assert(fragmented ? abort_fault.first_sector>next_contiguous :
                                abort_fault.first_sector==next_contiguous);
        }
        assert(f_write(&file,payload,1,&count)==FR_DISK_ERR && !count);
        if (cached) {
            assert(f_close(&file)==FR_DISK_ERR && failures==2 && file.obj.fs==&fs);
        }
    }
    abort_fault.payload=NULL;
    assert(f_close(&file)==FR_OK);
    assert(f_mount(NULL,"0:",0)==FR_OK && f_mount(&fs,"0:",1)==FR_OK);
    memcpy(expected+position,payload,completed);
    verify_file("/save.bin",expected,cached ? old_size : old_size+completed);
    verify_file("/neighbor.bin",neighbor,sizeof(neighbor));
    if (fragmented) verify_file("/between.bin",expected,cluster_bytes);
    assert(f_mount(NULL,"0:",0)==FR_OK);
    free(expected);
    free(payload);
    printf("write abort case=%d prefix=%u failures=%u\n",value,completed,failures);
}

static void store_little(BYTE *out, uint64_t value, unsigned size)
{
    for (unsigned i = 0; i < size; ++i) {
        out[i] = (BYTE)value;
        value >>= 8;
    }
}

static void exfat_entry_checksum(BYTE *entry, unsigned available)
{
    assert(available >= 32 && entry[0] == 0x85);
    unsigned bytes = (entry[1] + 1) * 32;
    assert(bytes <= available);
    uint16_t checksum = 0;
    for (unsigned i = 0; i < bytes; ++i)
        if (i != 2 && i != 3)
            checksum = (uint16_t)(((checksum >> 1) | ((uint32_t)checksum << 15)) + entry[i]);
    store_little(entry + 2, checksum, 2);
}

static void set_32bit_fat_link(LBA_t fat, DWORD cluster, DWORD next)
{
    BYTE table[512];
    LBA_t sector = fat + cluster / 128;
    assert(disk_read(0, table, sector, 1) == RES_OK);
    store_little(table + 4 * (cluster % 128), next, 4);
    assert(disk_write(0, table, sector, 1) == RES_OK);
}

static void set_fat32_extflags(unsigned flags)
{
    BYTE boot[512];
    assert(disk_read(0, boot, 0, 1) == RES_OK);
    store_little(boot + 40, flags, 2);
    store_little(boot + 48, 0, 2); /* Force a free-cluster scan. */
    assert(disk_write(0, boot, 0, 1) == RES_OK);
}

static void fat32_active(unsigned flags, int one_fat)
{
    enum { FAT_MIRROR_OFF = 0x80, FAT_ACTIVE_MASK = 0x0f };
    FATFS fs, *reported;
    FIL file;
    UINT count;
    DWORD free_before, free_after;
    BYTE payload[5 * 512], sector[512];
    unsigned copies = one_fat ? 1 : 2;
    int single_active = (flags & FAT_MIRROR_OFF) != 0;
    unsigned active = single_active ? flags & FAT_ACTIVE_MASK : 0;

    format_volume(FM_FAT32, copies, 512);
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    LBA_t base = fs.fatbase, length = fs.fsize, data = fs.database;
    if (active >= copies) {
        assert(f_mount(NULL, "0:", 0) == FR_OK);
        set_fat32_extflags(flags);
        unsigned before = writes;
        assert(f_mount(&fs, "0:", 1) == FR_NO_FILESYSTEM);
        assert(writes == before && fs.fs_type == 0);
        assert(f_mount(NULL, "0:", 0) == FR_OK);
        return;
    }

    for (unsigned i = 0; i < sizeof payload; i++) payload[i] = pattern(i);
    assert(f_open(&file, "/active.bin", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
    assert(f_write(&file, payload, 3 * 512, &count) == FR_OK && count == 3 * 512);
    DWORD first = file.obj.sclust;
    assert(f_close(&file) == FR_OK && f_mount(NULL, "0:", 0) == FR_OK);
    set_fat32_extflags(flags);

    size_t table_bytes = (size_t)length * 512;
    BYTE *inactive = NULL;
    LBA_t inactive_base = 0;
    if (single_active && copies == 2) {
        inactive_base = base + (1 - active) * length;
        /* The unselected table has a stale EOF link and an occupied cluster. */
        set_32bit_fat_link(inactive_base, first, 0x0fffffff);
        set_32bit_fat_link(inactive_base, first + 3, 0x0fffffff);
        inactive = malloc(table_bytes);
        assert(inactive && disk_read(0, inactive, inactive_base, length) == RES_OK);
    }
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    unsigned before = writes;
    verify_file("/active.bin", payload, 3 * 512);
    assert(f_getfree("0:", &free_before, &reported) == FR_OK && reported == &fs);
    assert(free_before == fs.n_fatent - 6 && writes == before);
    assert(fs.n_fats == (single_active ? 1 : copies));
    assert(fs.fatbase == base + active * length && fs.database == data);

    assert(f_open(&file, "/active.bin", FA_OPEN_APPEND | FA_WRITE) == FR_OK);
    assert(f_write(&file, payload + 3 * 512, 512, &count) == FR_OK && count == 512);
    assert(f_close(&file) == FR_OK);
    assert(f_getfree("0:", &free_after, &reported) == FR_OK && free_after + 1 == free_before);
    assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
    verify_file("/active.bin", payload, 4 * 512);

    if (inactive) {
        for (LBA_t i = 0; i < length; i++) {
            assert(disk_read(0, sector, inactive_base + i, 1) == RES_OK);
            assert(!memcmp(inactive + 512 * i, sector, 512));
        }
        free(inactive);
        assert(f_mount(NULL, "0:", 0) == FR_OK);
        for (LBA_t i = 0; i < length; i++) {
            assert(disk_read(0, sector, base + active * length + i, 1) == RES_OK);
            assert(disk_write(0, sector, inactive_base + i, 1) == RES_OK);
        }
        /* Remount the same object as mirrored. The active index is ignored. */
        set_fat32_extflags(0x0f);
        assert(f_mount(&fs, "0:", 1) == FR_OK && fs.fatbase == base);
        assert(f_open(&file, "/active.bin", FA_OPEN_APPEND | FA_WRITE) == FR_OK);
        assert(f_write(&file, payload + 4 * 512, 512, &count) == FR_OK && count == 512);
        assert(f_close(&file) == FR_OK);
        verify_file("/active.bin", payload, 5 * 512);
    }
    if (copies == 2) compare_fats(&fs);
    assert(f_mount(NULL, "0:", 0) == FR_OK);
}

/* Format with the generic fixture and mount with the App fixture. */
static void format_profile_card(int type, int partitioned)
{
    BYTE work[32768];
    MKFS_PARM options = {
        .fmt = (type == FS_FAT32 ? FM_FAT32 : type == FS_EXFAT ? FM_EXFAT : FM_FAT)
             | (partitioned ? 0 : FM_SFD),
        .n_fat = 2, .au_size = 512
    };
    assert(f_mkfs("0:", &options, work, sizeof work) == FR_OK);
    FATFS fs;
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    assert(fs.fs_type == type && !!fs.volbase == partitioned);
    assert(f_mount(NULL, "0:", 0) == FR_OK);
}

static void profile_mount(int type, int partitioned)
{
    FATFS fs;
    BYTE boot[512];
    if (type == FS_FAT32) {
        LBA_t sector = 0;
        if (partitioned) {
            assert(disk_read(0, boot, 0, 1) == RES_OK);
            sector = little_word(boot + 454);
            assert(sector);
        }
        assert(disk_read(0, boot, sector, 1) == RES_OK);
        memset(boot + 82, ' ', 8); /* FAT32 has no authoritative type label. */
        assert(disk_write(0, boot, sector, 1) == RES_OK);
    }
    unsigned before = writes;
    FRESULT res = f_mount(&fs, "0:", 1);
    assert(res == (type == FS_FAT32 ? FR_OK : FR_NO_FILESYSTEM));
    assert(writes == before);
    if (res == FR_OK) {
        assert(fs.fs_type == FS_FAT32 && !!fs.volbase == partitioned);
        /* Fill the first directory cluster and read every file after remount. */
        for (unsigned i = 0; i < 20; i++) {
            char name[24];
            BYTE payload[513];
            snprintf(name, sizeof name, "/長い名前 %02u.bin", i);
            memset(payload, i + 1, sizeof payload);
            create_file(name, payload, sizeof payload);
        }
        assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
        for (unsigned i = 0; i < 20; i++) {
            char name[24];
            BYTE payload[513];
            snprintf(name, sizeof name, "/長い名前 %02u.bin", i);
            memset(payload, i + 1, sizeof payload);
            verify_file(name, payload, sizeof payload);
        }
    } else {
        FIL file;
        assert(!fs.fs_type);
        assert(f_open(&file, "/never.bin", FA_CREATE_ALWAYS | FA_WRITE) == FR_NO_FILESYSTEM);
        assert(writes == before);
    }
    assert(f_mount(NULL, "0:", 0) == FR_OK);
}

static void fat32_max_file(void)
{
    /* Synthesize a valid 4 GiB-1 file without writing gigabytes of test data. */
    enum { SECTORS = 9000000, CLUSTER_SECTORS = 64, FILE_CLUSTERS = 131072, FAT_SECTORS = 1100 };
    BYTE sector[512] = {0};
    assert(sectors == SECTORS);
    sector[0] = 0xeb; sector[2] = 0x90;
    store_little(sector + 11, 512, 2);
    sector[13] = CLUSTER_SECTORS;
    store_little(sector + 14, 32, 2);
    sector[16] = 1;
    sector[21] = 0xf8;
    store_little(sector + 32, SECTORS, 4);
    store_little(sector + 36, FAT_SECTORS, 4);
    store_little(sector + 44, 2, 4);
    store_little(sector + 510, 0xaa55, 2);
    assert(disk_write(0, sector, 0, 1) == RES_OK);

    for (unsigned i = 0; i <= (FILE_CLUSTERS + 2) / 128; i++) {
        for (unsigned j = 0; j < 128; j++) {
            unsigned cluster = i * 128 + j;
            DWORD entry = cluster == 0 ? 0x0ffffff8 :
                          cluster <= 2 || cluster == FILE_CLUSTERS + 2 ? 0x0fffffff :
                          cluster < FILE_CLUSTERS + 2 ? cluster + 1 : 0;
            store_little(sector + 4 * j, entry, 4);
        }
        assert(disk_write(0, sector, 32 + i, 1) == RES_OK);
    }
    memset(sector, 0, sizeof sector);
    memcpy(sector, "MAX     BIN", 11);
    sector[11] = AM_ARC;
    store_little(sector + 26, 3, 2);
    store_little(sector + 28, UINT32_MAX, 4);
    assert(disk_write(0, sector, 32 + FAT_SECTORS, 1) == RES_OK);

    FATFS fs;
    FIL file;
    UINT count;
    BYTE marker = 0x5a, received = 0;
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    assert(f_open(&file, "/MAX.BIN", FA_READ | FA_WRITE) == FR_OK);
    assert(f_size(&file) == UINT32_MAX);
    assert(f_lseek(&file, UINT32_MAX - 1u) == FR_OK);
    assert(f_write(&file, &marker, 1, &count) == FR_OK && count == 1);
    assert(f_tell(&file) == UINT32_MAX);
    unsigned before = writes;
    assert(f_write(&file, &marker, 1, &count) == FR_OK && count == 0);
    assert(writes == before && f_tell(&file) == UINT32_MAX);
    assert(f_close(&file) == FR_OK && f_mount(NULL, "0:", 0) == FR_OK);
    assert(f_mount(&fs, "0:", 1) == FR_OK && f_open(&file, "/MAX.BIN", FA_READ) == FR_OK);
    assert(f_size(&file) == UINT32_MAX && f_lseek(&file, UINT32_MAX - 1u) == FR_OK);
    assert(f_read(&file, &received, 1, &count) == FR_OK && count == 1 && received == marker);
    assert(f_read(&file, &received, 1, &count) == FR_OK && count == 0);
    assert(f_close(&file) == FR_OK && f_mount(NULL, "0:", 0) == FR_OK);
}

#if FF_FS_EXFAT
static void write_exfat_boot(BYTE *boot, LBA_t base)
{
    uint32_t checksum = 0;
    for (unsigned i = 0; i < 11 * 512; ++i)
        if (i != 106 && i != 107 && i != 112)
            checksum = ((checksum >> 1) | (checksum << 31)) + boot[i];
    for (unsigned i = 11 * 512; i < 12 * 512; i += 4)
        store_little(boot + i, checksum, 4);
    assert(disk_write(0, boot, base, 12) == RES_OK);
}

static void set_bitmap_boot_geometry(int fat_span, int large_heap)
{
    for (LBA_t base = 0; base <= 12; base += 12) {
        BYTE boot[12 * 512];
        assert(disk_read(0, boot, base, 11) == RES_OK);
        store_little(boot + 92, large_heap ? (1u << 26) : 8192, 4);
        if (fat_span)
            store_little(boot + 84, 0, 4);
        if (large_heap)
            boot[109] = 6;
        write_exfat_boot(boot, base);
    }
}

/* Adapted from paps-g b483124c. Format with the actual library before
 * varying the bitmap entry and its backing allocation on a host file. */
static void bitmap_case(const char *kind, unsigned value)
{
    int partitioned = strcmp(kind, "partitioned") == 0;
    int formatted = partitioned || strcmp(kind, "formatted") == 0;
    int exact = strcmp(kind, "exact") == 0;
    int padded = strcmp(kind, "padded") == 0;
    int short_chain = strcmp(kind, "short-chain") == 0;
    int outside_heap = strcmp(kind, "outside-heap") == 0;
    int large_heap = strcmp(kind, "large-heap") == 0;
    if (partitioned) {
        BYTE work[32768];
        MKFS_PARM options = {.fmt = FM_EXFAT, .n_fat = 1, .au_size = 512};
        assert(f_mkfs("0:", &options, work, sizeof work) == FR_OK);
    } else {
        format_volume(formatted && value == 0 ? FM_FAT32 : FM_EXFAT, 1,
                      formatted && value ? value * 512 : 512);
    }
    FATFS fs;
    FIL file;
    BYTE neighbor[512] = {0};
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    if (partitioned)
        assert(fs.volbase > 0);
    create_file("/NEIGHBOR.BIN", neighbor, sizeof neighbor);
    create_file("/TARGET.BIN", neighbor, 0);

    if (!formatted) {
        assert(f_open(&file, "/NEIGHBOR.BIN", FA_READ) == FR_OK);
        unsigned entry = file.obj.c_ofs;
        assert(f_close(&file) == FR_OK && f_mount(NULL, "0:", 0) == FR_OK);
        LBA_t data = fs.database, fat = fs.fatbase;
        LBA_t root = data + fs.dirbase - 2;
        BYTE directory[512];
        assert(disk_read(0, directory, root, 1) == RES_OK);
        unsigned bitmap = 0;
        while (bitmap < sizeof directory && directory[bitmap] != 0x81)
            bitmap += 32;
        assert(bitmap < sizeof directory && directory[entry] == 0x85);
        unsigned clusters = short_chain ? 1 : padded ? 3 : 2;
        DWORD first = outside_heap ? 8193 : 2;
        DWORD neighbor_cluster = 2 + clusters;
        unsigned length = strcmp(kind, "short-length") == 0 ? 512 :
                          padded || strcmp(kind, "over-length") == 0 ? 1025 : 1024;
        store_little(directory + bitmap + 20, first, 4);
        store_little(directory + bitmap + 24, length, 8);
        store_little(directory + entry + 52, neighbor_cluster, 4);
        exfat_entry_checksum(directory + entry, sizeof directory - entry);
        assert(disk_write(0, directory, root, 1) == RES_OK);
        for (unsigned i = 0; i < clusters; ++i)
            set_32bit_fat_link(fat, first + i, i + 1 == clusters ? UINT32_MAX : first + i + 1);
        if (strcmp(kind, "fragmented") == 0)
            set_32bit_fat_link(fat, 2, 4);
        set_32bit_fat_link(fat, neighbor_cluster, UINT32_MAX);
        BYTE full[512];
        memset(full, 0xff, sizeof full);
        assert(disk_write(0, full, data, 1) == RES_OK);
        for (unsigned i = 1; i < clusters; ++i)
            assert(disk_write(0, neighbor, data + i, 1) == RES_OK);
        assert(disk_write(0, neighbor, data + neighbor_cluster - 2, 1) == RES_OK);
        set_bitmap_boot_geometry(strcmp(kind, "fat-span") == 0, large_heap);
    } else {
        assert(f_mount(NULL, "0:", 0) == FR_OK);
    }

    reads = writes = 0;
    if (exact)
        fail_read = value;
    FRESULT mount = f_mount(&fs, "0:", 1);
    if (fail_read) {
        assert(mount == FR_DISK_ERR && !fs.fs_type && failures == 1 && !writes);
        fail_read = 0;
        assert(f_mount(NULL, "0:", 0) == FR_OK);
        mount = f_mount(&fs, "0:", 1);
    }

    if (formatted || exact || padded || (short_chain && mount == FR_OK)) {
        assert(mount == FR_OK);
        BYTE payload = 0x5a;
        UINT count;
        assert(f_open(&file, "/TARGET.BIN", FA_WRITE) == FR_OK);
        assert(f_write(&file, &payload, 1, &count) == FR_OK && count == 1);
        assert(f_close(&file) == FR_OK);
        assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
        if (short_chain) {
            BYTE changed[512];
            assert(f_open(&file, "/NEIGHBOR.BIN", FA_READ) == FR_OK);
            assert(f_read(&file, changed, sizeof changed, &count) == FR_OK && count == sizeof changed);
            fprintf(stderr, "short bitmap admitted; neighbor first byte=%u\n", (unsigned)changed[0]);
            assert(f_close(&file) == FR_OK);
        } else {
            verify_file("/TARGET.BIN", &payload, 1);
            verify_file("/NEIGHBOR.BIN", neighbor, sizeof neighbor);
        }
    }
    if (!(formatted || exact || padded)) {
        assert(mount == FR_NO_FILESYSTEM && !fs.fs_type && !writes);
        if (large_heap)
            assert(reads == 1);
    }
    assert(f_mount(NULL, "0:", 0) == FR_OK);
}

#endif

/* Adapted from paps-g 8da2769d. Check admission before any data-area I/O. */
static void fat_mount_extent(unsigned which)
{
    enum { FAT_COUNT_WRAP, RESERVED_WRAP, END_WRAP, END_EXACT, ONE_FAT, TWO_FATS };
    assert(which <= TWO_FATS);
    const DWORD volume = 131072;
    BYTE boot[512] = {0}, mbr[512] = {0};
    unsigned fats = which == FAT_COUNT_WRAP || which == TWO_FATS ? 2 : 1;
    DWORD length = which == FAT_COUNT_WRAP ? 0x80000400 :
                   which == RESERVED_WRAP ? 0xfffffff0 : 1024;
    boot[0] = 0xeb; boot[2] = 0x90;
    store_little(boot + 11, 512, 2);
    boot[13] = 1;
    store_little(boot + 14, 32, 2);
    boot[16] = fats;
    boot[21] = 0xf8;
    store_little(boot + 32, volume, 4);
    store_little(boot + 36, length, 4);
    store_little(boot + 44, 2, 4);
    memcpy(boot + 82, "FAT32   ", 8);
    store_little(boot + 510, 0xaa55, 2);
    if (which == END_WRAP || which == END_EXACT) {
        boot_alias = UINT32_MAX - volume + 1 + (which == END_WRAP);
        mbr[450] = 0x0c;
        store_little(mbr + 454, boot_alias, 4);
        store_little(mbr + 458, volume, 4);
        store_little(mbr + 510, 0xaa55, 2);
        assert(disk_write(0, mbr, 0, 1) == RES_OK);
    }
    assert(disk_write(0, boot, !!boot_alias, 1) == RES_OK);
    reads = writes = 0;
    FATFS fs;
    FRESULT result = f_mount(&fs, "0:", 1);
    printf("FAT32 case %u: result %u, reads %u\n", which, result, reads);
    assert(!writes && reads == 1u + !!boot_alias);
    if (which < END_EXACT) assert(result == FR_NO_FILESYSTEM && !fs.fs_type);
    else {
        assert(result == FR_OK && fs.fs_type == FS_FAT32 && fs.volbase == boot_alias);
        assert(fs.fatbase == boot_alias + 32 && fs.database == fs.fatbase + length*fats);
        assert(fs.n_fatent == volume - 32 - length*fats + 2);
    }
    assert(!f_mount(NULL, "0:", 0));
}

#if FF_FS_EXFAT
static void exfat_mount_extent(unsigned which)
{
    enum { SHORT_FAT, EARLY_FAT, FAT_HEAP_OVERLAP, EXACT_FAT, FORMATTED_FAT };
    assert(which <= FORMATTED_FAT);
    format_volume(FM_EXFAT, 1, 512);
    FATFS fs;
    assert(!f_mount(&fs, "0:", 1) && fs.fs_type == FS_EXFAT && !fs.volbase);
    assert(!f_mount(NULL, "0:", 0));
    DWORD minimum = (fs.n_fatent + 127) / 128;
    assert(minimum > 1 && fs.fsize >= minimum && fs.fatbase >= 24);
    for (LBA_t base = 0; base <= 12; base += 12) {
        BYTE boot[12 * 512];
        assert(disk_read(0, boot, base, 11) == RES_OK);
        if (which == EARLY_FAT) store_little(boot + 80, 23, 4);
        if (which == SHORT_FAT || which == EXACT_FAT)
            store_little(boot + 84, minimum - (which == SHORT_FAT), 4);
        if (which == FAT_HEAP_OVERLAP)
            store_little(boot + 84, fs.database - fs.fatbase + 1, 4);
        write_exfat_boot(boot, base);
    }
    if (which == EARLY_FAT) {
        /* The invalid layout claims a backup boot sector as its FAT. */
        BYTE first[512];
        assert(disk_read(0, first, fs.fatbase, 1) == RES_OK);
        assert(disk_write(0, first, 23, 1) == RES_OK);
    }
    reads = writes = 0;
    FRESULT result = f_mount(&fs, "0:", 1);
    printf("exFAT case %u: result %u, reads %u\n", which, result, reads);
    if (which < EXACT_FAT) assert(result == FR_NO_FILESYSTEM && !fs.fs_type && reads == 1 && !writes);
    else {
        BYTE expected[] = {0x42, 0x7e};
        assert(result == FR_OK && fs.fs_type == FS_EXFAT);
        if (which == EXACT_FAT) assert(fs.fsize == minimum);
        create_file("/DATA.BIN", expected, sizeof expected);
        assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
        verify_file("/DATA.BIN", expected, sizeof expected);
    }
    assert(!f_mount(NULL, "0:", 0));
}

#endif

/* Adapted from paps-g b483124c. Change only the on-disk size of an
 * empty file, leaving its starting cluster absent. */
static void set_empty_file_size(FATFS *fs, FIL *file, unsigned size)
{
    LBA_t sector;
    unsigned offset;
    int exfat = fs->fs_type == FS_EXFAT;
#if FF_FS_EXFAT
    if (exfat) {
        DWORD root = file->obj.c_scl ? file->obj.c_scl : (DWORD)fs->dirbase;
        sector = fs->database + (root - 2) * fs->csize + file->obj.c_ofs / 512;
        offset = file->obj.c_ofs % 512;
    } else
#endif
    {
        sector = file->dir_sect;
        offset = (unsigned)(file->dir_ptr - fs->win);
    }
    assert(!file->obj.sclust && !f_size(file));
    assert(f_close(file) == FR_OK && f_mount(NULL, "0:", 0) == FR_OK);
    BYTE directory[512];
    assert(disk_read(0, directory, sector, 1) == RES_OK);
    if (exfat) {
        assert(offset + 64 <= sizeof directory && directory[offset + 32] == 0xc0);
        store_little(directory + offset + 40, size, 8);
        store_little(directory + offset + 56, size, 8);
        exfat_entry_checksum(directory + offset, sizeof directory - offset);
    } else {
        assert(offset + 32 <= sizeof directory);
        store_little(directory + offset + 28, size, 4);
    }
    assert(disk_write(0, directory, sector, 1) == RES_OK);
    assert(f_mount(fs, "0:", 1) == FR_OK);
}

static void readonly_seek_case(BYTE type, unsigned scenario)
{
    enum { FIRST, LAST, PAST, ZERO, EMPTY, WRITER, HEALTHY };
    assert(scenario <= HEALTHY);
    format_volume(type == FS_EXFAT ? FM_EXFAT : type == FS_FAT32 ? FM_FAT32 : FM_FAT,
                  1, 512);
    FATFS fs, *volume;
    FIL file;
    BYTE neighbor[1024], byte;
    UINT count;
    memset(neighbor, 0x37, sizeof neighbor);
    assert(f_mount(&fs, "0:", 1) == FR_OK && fs.fs_type == type);
    create_file("/NEIGHBOR.BIN", neighbor, sizeof neighbor);
    create_file("/TARGET.BIN", neighbor, 0);
    if (scenario <= ZERO) {
        assert(f_open(&file, "/TARGET.BIN", FA_READ) == FR_OK);
        set_empty_file_size(&fs, &file, 512);
    }

    DWORD free_before;
    assert(f_getfree("0:", &free_before, &volume) == FR_OK && volume == &fs);
    const char *path = scenario == HEALTHY ? "/NEIGHBOR.BIN" : "/TARGET.BIN";
    assert(f_open(&file, path, scenario == WRITER ? FA_WRITE : FA_READ) == FR_OK);
    if (scenario == WRITER) {
        assert(f_lseek(&file, 513) == FR_OK && f_tell(&file) == 513);
        byte = 0x5a;
        assert(f_write(&file, &byte, 1, &count) == FR_OK && count == 1);
    } else if (scenario == HEALTHY) {
        assert(f_lseek(&file, 513) == FR_OK && f_tell(&file) == 513);
        assert(f_read(&file, &byte, 1, &count) == FR_OK && count == 1 && byte == neighbor[513]);
    } else {
        BYTE window[512];
        memcpy(window, fs.win, sizeof window);
        DWORD last = fs.last_clst, free_clusters = fs.free_clst;
        LBA_t window_sector = fs.winsect;
        BYTE dirty = fs.wflag;
        unsigned writes_before = writes;
        FSIZE_t offset = scenario == FIRST ? 1 : scenario == LAST ? 512 :
                         scenario == ZERO ? 0 : 513;
        assert(!file.obj.sclust && f_size(&file) == (scenario == EMPTY ? 0 : 512));
        FRESULT expected = scenario <= PAST ? FR_INT_ERR : FR_OK;
        FRESULT result = f_lseek(&file, offset);
        if (expected == FR_INT_ERR && result == FR_OK) {
            DWORD allocated = file.obj.sclust;
            assert(f_close(&file) == FR_OK);
            verify_file("/NEIGHBOR.BIN", neighbor, sizeof neighbor);
            assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
            DWORD free_after;
            assert(f_getfree("0:", &free_after, &volume) == FR_OK);
            if (type == FS_FAT32) {
                /* FSInfo may retain the old free count. Read the FAT entry. */
                BYTE fat_sector[512];
                assert(allocated >= 2);
                assert(disk_read(0, fat_sector, fs.fatbase + allocated / 128, 1) == RES_OK);
                assert(little_word(fat_sector + (allocated % 128) * 4) & 0x0fffffffu);
                fprintf(stderr, "allocation persisted in the FAT\n");
            }
            fprintf(stderr, "read-only seek allocated cluster %lu on FAT type %u, free %lu to %lu\n",
                    (unsigned long)allocated, type, (unsigned long)free_before, (unsigned long)free_after);
        }
        assert(result == expected);
        assert(!file.obj.sclust && !f_tell(&file) && writes == writes_before);
        assert(fs.last_clst == last && fs.free_clst == free_clusters &&
               fs.winsect == window_sector && fs.wflag == dirty &&
               !memcmp(window, fs.win, sizeof window));
        if (expected == FR_INT_ERR) {
            assert(f_error(&file) == FR_INT_ERR);
            assert(f_read(&file, &byte, 1, &count) == FR_INT_ERR && !count);
        }
    }
    assert(f_close(&file) == FR_OK);
    assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
    verify_file("/NEIGHBOR.BIN", neighbor, sizeof neighbor);
    if (scenario == WRITER) {
        assert(f_open(&file, path, FA_READ) == FR_OK && f_size(&file) == 514);
        assert(f_lseek(&file, 513) == FR_OK);
        assert(f_read(&file, &byte, 1, &count) == FR_OK && count == 1 && byte == 0x5a);
        assert(f_close(&file) == FR_OK);
    } else {
        DWORD free_after;
        assert(f_getfree("0:", &free_after, &volume) == FR_OK && free_before == free_after);
    }
    assert(f_mount(NULL, "0:", 0) == FR_OK);
}

/* Adapted from paps-g 371f5369. Preserve an open, dirty object across reidentification. */
static void retained_media(int fat32, int by_path)
{
    format_volume(fat32 ? FM_FAT32 : FM_EXFAT, 1, fat32 ? 512 : 65536);
    FATFS fs;
    FIL file;
    FILINFO info;
    BYTE payload[517], received[517];
    UINT count;
    for (unsigned i = 0; i < sizeof payload; ++i) payload[i] = pattern(i);
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    assert(f_open(&file, "/session.bin", FA_CREATE_NEW | FA_WRITE) == FR_OK);
    assert(f_write(&file, payload, sizeof payload, &count) == FR_OK && count == sizeof payload);
    BYTE type = fs.fs_type, flags = file.flag, dirty = fs.wflag;
    WORD mount_id = fs.id;
    unsigned previous_reads = reads, previous_writes = writes, previous_syncs = syncs;
    for (enum Media state = MEDIA_ABSENT; state <= MEDIA_DIFFERENT; state++) {
        media = state;
        assert(f_sync(&file) == FR_INVALID_OBJECT && f_close(&file) == FR_INVALID_OBJECT);
        assert(f_stat("/session.bin", &info) == FR_NOT_READY);
        assert(fs.fs_type == type && fs.id == mount_id && fs.wflag == dirty);
        assert(file.obj.fs == &fs && file.flag == flags && f_size(&file) == sizeof payload);
        assert(reads == previous_reads && writes == previous_writes && syncs == previous_syncs);
    }
    media = MEDIA_RETURNED;
    if (by_path) assert(f_stat("/session.bin", &info) == FR_OK && reidentifications == 1);
    assert(f_sync(&file) == FR_OK && reidentifications == 1 && fs.id == mount_id);
    assert(f_close(&file) == FR_OK && f_mount(NULL, "0:", 0) == FR_OK);
    assert(f_mount(&fs, "0:", 1) == FR_OK && f_open(&file, "/session.bin", FA_READ) == FR_OK);
    assert(f_read(&file, received, sizeof received, &count) == FR_OK && count == sizeof received);
    assert(!memcmp(payload, received, sizeof payload));
    assert(f_close(&file) == FR_OK && f_mount(NULL, "0:", 0) == FR_OK);
}

int main(int argc,char **argv)
{
    assert(argc==4);
    if(!strcmp(argv[2],"math")){comparison();return 0;}
    disk=fopen(argv[1],"r+b");assert(disk && !fseeko(disk,0,SEEK_END));sectors=ftello(disk)/512;
    int value = atoi(argv[3]);
    if (!strcmp(argv[2], "format-profile")) {
        format_profile_card(value & 7, value >> 3);
    } else if (!strcmp(argv[2], "mount-profile")) {
        profile_mount(value & 7, value >> 3);
    } else if (!strcmp(argv[2], "max-fat32-file")) {
        fat32_max_file();
    } else if (!strcmp(argv[2], "active-fat32") || !strcmp(argv[2], "single-fat32")) {
        fat32_active((unsigned)value, !strcmp(argv[2], "single-fat32"));
    } else if (!strcmp(argv[2], "retained-media")) {
        retained_media(value & 1, value & 2);
    } else if (!strcmp(argv[2], "sync-fat32") || !strcmp(argv[2], "sync-exfat")) {
        sync_file(!strcmp(argv[2], "sync-fat32"), value);
    } else if (!strcmp(argv[2], "utf8-fat32") || !strcmp(argv[2], "utf8-exfat")) {
        utf8_path(!strcmp(argv[2], "utf8-fat32"), (unsigned)value);
    } else if (!strcmp(argv[2], "write-abort-fat32") || !strcmp(argv[2], "write-abort-exfat")) {
        aborted_write(!strcmp(argv[2], "write-abort-fat32"), value);
    } else if (!strncmp(argv[2], "readonly-", 9)) {
        const char *name = argv[2] + 9;
        BYTE type = !strcmp(name, "fat12") ? FS_FAT12 : !strcmp(name, "fat16") ? FS_FAT16 :
                    !strcmp(name, "fat32") ? FS_FAT32 : FS_EXFAT;
        readonly_seek_case(type, (unsigned)value);
    } else if (!strcmp(argv[2], "mount-fat-extent")) {
        fat_mount_extent((unsigned)value);
#if FF_FS_EXFAT
    } else if (!strcmp(argv[2], "mount-exfat-extent")) {
        exfat_mount_extent((unsigned)value);
    } else if (!strncmp(argv[2], "bitmap-", 7)) {
        bitmap_case(argv[2] + 7, (unsigned)value);
#endif
    } else if (!strncmp(argv[2], "metadata-", 9)) {
        const char *name = argv[2] + 9;
        if (!strcmp(name, "fat12")) sync_metadata(FS_FAT12, value);
        else if (!strcmp(name, "fat16")) sync_metadata(FS_FAT16, value);
        else { assert(!strcmp(name, "fat32")); sync_metadata(FS_FAT32, value); }
    } else {
        int fat32 = !strcmp(argv[2], "fragmented") && value;
        format_volume(fat32 ? FM_FAT32 : FM_EXFAT, 1, fat32 ? 512 : 65536);
#if FF_FS_EXFAT
        if (!strcmp(argv[2], "seek")) seek_file();
        else
#endif
        if (!strcmp(argv[2], "fragmented")) fragmented();
        else { assert(!strcmp(argv[2], "mount")); mount_geometry(value); }
    }
    assert(!fclose(disk));return 0;
}
