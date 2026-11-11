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
static unsigned reads,writes,syncs,failures;
static LBA_t fault_sector;
static UINT fault_prefix;
static int fail_write,fail_sync;
static pthread_mutex_t mutex;
int ff_mutex_create(int vol) { assert(!vol);return pthread_mutex_init(&mutex,NULL)==0; }
void ff_mutex_delete(int vol) { assert(!vol);assert(!pthread_mutex_destroy(&mutex)); }
int ff_mutex_take(int vol) { assert(!vol);return pthread_mutex_lock(&mutex)==0; }
void ff_mutex_give(int vol) { assert(!vol);assert(!pthread_mutex_unlock(&mutex)); }
DSTATUS disk_initialize(BYTE drive) { return drive?STA_NOINIT:0; }
DSTATUS disk_status(BYTE drive) { return drive?STA_NOINIT:0; }
DRESULT disk_read(BYTE drive,BYTE *out,LBA_t sector,UINT count)
{
    assert(!drive && count && sector+count<=sectors);reads++;
    assert(!fseeko(disk,(off_t)sector*512,SEEK_SET));return fread(out,512,count,disk)==count?RES_OK:RES_ERROR;
}
DRESULT disk_write(BYTE drive,const BYTE *data,LBA_t sector,UINT count)
{
    assert(!drive && count && sector+count<=sectors);writes++;
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
    assert(!drive);
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

static void create_neighbor(const BYTE *data, UINT size)
{
    FIL file;
    UINT count;
    assert(f_open(&file, "/neighbor.bin", FA_CREATE_NEW | FA_WRITE) == FR_OK);
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
    create_neighbor(neighbor, sizeof(neighbor));
    assert(f_open(&file, "/save.bin", FA_CREATE_NEW | FA_READ | FA_WRITE) == FR_OK);
    assert(f_write(&file, payload, 8192, &count) == FR_OK && count == 8192);
    assert(f_sync(&file) == FR_OK);
    check_clean_sync(&file);
    if (fat32) {
        fault_sector = file.dir_sect;
    } else {
        DWORD cluster = file.obj.c_scl ? file.obj.c_scl : (DWORD)fs.dirbase;
        fault_sector = fs.database + (cluster - 2) * fs.csize + file.obj.c_ofs / 512;
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
    create_neighbor(neighbor, sizeof(neighbor));
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

int main(int argc,char **argv)
{
    assert(argc==4);
    if(!strcmp(argv[2],"math")){comparison();return 0;}
    disk=fopen(argv[1],"r+b");assert(disk && !fseeko(disk,0,SEEK_END));sectors=ftello(disk)/512;
    int value = atoi(argv[3]);
    if (!strcmp(argv[2], "sync-fat32") || !strcmp(argv[2], "sync-exfat")) {
        sync_file(!strcmp(argv[2], "sync-fat32"), value);
    } else if (!strncmp(argv[2], "metadata-", 9)) {
        const char *name = argv[2] + 9;
        if (!strcmp(name, "fat12")) sync_metadata(FS_FAT12, value);
        else if (!strcmp(name, "fat16")) sync_metadata(FS_FAT16, value);
        else { assert(!strcmp(name, "fat32")); sync_metadata(FS_FAT32, value); }
    } else {
        int fat32 = !strcmp(argv[2], "fragmented") && value;
        format_volume(fat32 ? FM_FAT32 : FM_EXFAT, 1, fat32 ? 512 : 65536);
        if (!strcmp(argv[2], "seek")) seek_file();
        else if (!strcmp(argv[2], "fragmented")) fragmented();
        else { assert(!strcmp(argv[2], "mount")); mount_geometry(value); }
    }
    assert(!fclose(disk));return 0;
}
