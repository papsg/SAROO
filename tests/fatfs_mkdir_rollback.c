/* Adapted from paps-g 99d0b565 and 4842feaa (SSR-107).
 * Actual public FatFs; synthetic media and block faults only. */
#include "ff.h"
/* The reused scaffold's unused formatter must never run in this image fixture. */
#define f_mkfs(...) (abort(), FR_INT_ERR)
#define main original_sync_main
#define disk_write ordinary_disk_write
#define disk_ioctl ordinary_disk_ioctl
#define disk_read ordinary_disk_read
#define disk_status ordinary_disk_status
#define disk_initialize ordinary_disk_initialize
#include "fatfs_seek.c"
#undef disk_initialize
#undef disk_status
#undef disk_read
#undef disk_ioctl
#undef disk_write
#undef main
#undef f_mkfs

static LBA_t child_sector, fatbase, fat_size;
static DWORD child_cluster;
static unsigned fat_type, fat_copies;
static int clear_armed, hold_cleanup, partial_clear;
static const char *fault_mode;
static unsigned clear_failures, cleanup_failures;
static unsigned io_reads, io_writes, io_syncs;
static int dual_mode, dual_tail_written;
static DSTATUS status_override;

static DWORD fat_offset(DWORD cluster)
{
    return fat_type == FS_FAT12 ? cluster + cluster/2 :
           cluster * (fat_type == FS_FAT16 ? 2 : 4);
}

DSTATUS disk_status(BYTE drive) { return ordinary_disk_status(drive) | status_override; }
DSTATUS disk_initialize(BYTE drive) { return ordinary_disk_initialize(drive) | status_override; }
DRESULT disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
    io_reads++;
    LBA_t address = fatbase + fat_offset(child_cluster)/512;
    int selected = dual_mode ? dual_tail_written && sector <= fatbase && fatbase-sector < count :
                   clear_failures && !strcmp(fault_mode,"read") && sector <= address && address-sector < count;
    if (hold_cleanup && selected) { cleanup_failures++; return RES_ERROR; }
    return ordinary_disk_read(drive,data,sector,count);
}

DRESULT disk_write(BYTE drive,const BYTE *data,LBA_t sector,UINT count)
{
    io_writes++;
    if (clear_armed && sector <= child_sector && child_sector-sector < count) {
        clear_armed=0; clear_failures++;
        if (partial_clear) {
            assert(!fseeko(disk,(off_t)child_sector*512,SEEK_SET));
            assert(fwrite(data+(child_sector-sector)*512,1,256,disk)==256);
        }
        return RES_ERROR;
    }
    if (hold_cleanup && clear_failures) {
        LBA_t target = fatbase + fat_offset(child_cluster)/512;
        if (!strcmp(fault_mode,"mirror")) target += fat_size;
        if ((!strcmp(fault_mode,"fat") || !strcmp(fault_mode,"mirror")) &&
            sector <= target && target-sector < count) {
            cleanup_failures++; return RES_ERROR;
        }
    }
    DRESULT result = ordinary_disk_write(drive,data,sector,count);
    if (dual_mode && result == RES_OK && sector <= fatbase+1 && fatbase+1-sector < count)
        dual_tail_written = 1;
    return result;
}
DRESULT disk_ioctl(BYTE drive,BYTE cmd,void *out)
{
    if (cmd == CTRL_SYNC) io_syncs++;
    if (hold_cleanup && clear_failures && !strcmp(fault_mode,"sync") && cmd==CTRL_SYNC) {
        cleanup_failures++; return RES_ERROR;
    }
    return ordinary_disk_ioctl(drive,cmd,out);
}

static DWORD raw_free_count(FATFS *fs)
{
    size_t length = (size_t)fat_size * 512;
    BYTE *table = malloc(length); assert(table && length <= 8*1024*1024 && !fflush(disk));
    assert(!fseeko(disk,(off_t)fatbase*512,SEEK_SET) && fread(table,1,length,disk)==length);
    DWORD free_count = 0;
    for (DWORD i=2;i<fs->n_fatent;i++) {
        size_t at = fat_offset(i);
        DWORD value = table[at] | (DWORD)table[at+1]<<8;
        if (fat_type==FS_FAT12) value = (i&1 ? value>>4 : value)&0xfff;
        else if (fat_type==FS_FAT32) value = (value | (DWORD)table[at+2]<<16 | (DWORD)table[at+3]<<24)&0x0fffffff;
        if (!value) free_count++;
    }
    free(table); return free_count;
}

static void mutex_is_released(void)
{
#if FF_FS_REENTRANT
    assert(!pthread_mutex_trylock(&mutex) && !pthread_mutex_unlock(&mutex));
#endif
}
static DWORD raw_fat(DWORD cluster, unsigned copy)
{
    BYTE bytes[4]; assert(!fflush(disk));
    assert(copy < fat_copies);
    assert(!fseeko(disk,(off_t)(fatbase+copy*fat_size)*512+fat_offset(cluster),SEEK_SET));
    assert(fread(bytes,1,4,disk)==4);
    if (fat_type == FS_FAT12) {
        DWORD entry=(DWORD)bytes[0]|(DWORD)bytes[1]<<8;
        return (cluster&1 ? entry>>4 : entry)&0xfff;
    }
    if (fat_type == FS_FAT16) return (DWORD)bytes[0]|(DWORD)bytes[1]<<8;
    return ((DWORD)bytes[0]|(DWORD)bytes[1]<<8|(DWORD)bytes[2]<<16|(DWORD)bytes[3]<<24)&0x0fffffff;
}

/* Force a parent-extension link read to fail after it allocated cluster128,
 * while the unpublished child owns127. All placement uses actual public APIs. */
static int simultaneous_parent(FATFS *fs, int unknown)
{
    assert(fat_type==FS_FAT32 && fs->csize==1 && !f_mkdir("/PARENT"));
    DWORD parent = fs->last_clst;
    unsigned size = (126-fs->last_clst)*512;
    assert(parent < 126 && size <= 65536);
    BYTE *payload = malloc(size); assert(payload); memset(payload,0x55,size);
    FIL file; UINT count;
    assert(!f_open(&file,"/ANCHOR.BIN",FA_CREATE_NEW|FA_WRITE));
    assert(!f_write(&file,payload,size,&count) && count==size && !f_close(&file));
    assert(fs->last_clst==126);
    for (unsigned i=0;i<14;i++) {
        char name[40]; snprintf(name,sizeof(name),"/PARENT/F%07u",i);
        assert(!f_open(&file,name,FA_CREATE_NEW|FA_WRITE) && !f_close(&file));
    }
    FATFS *mounted; DWORD before = raw_free_count(fs), after;
    if (!unknown) assert(!f_getfree("0:",&after,&mounted) && after==before);
    else assert(fs->free_clst==UINT32_MAX);
    child_cluster=127; dual_mode=hold_cleanup=1;
    FRESULT made=f_mkdir("/PARENT/CHILDX");
    assert(made==FR_DISK_ERR && dual_tail_written && cleanup_failures);
    for (unsigned i=0;i<3;i++) assert(f_mount(NULL,"0:",0)==FR_DISK_ERR && fs->fs_type);
    hold_cleanup=0;
    assert(!f_mount(NULL,"0:",0));
    DWORD child=raw_fat(127,0),extension=raw_fat(128,0);
    for (unsigned i=1;i<fat_copies;i++) {
        assert(raw_fat(127,i)==child && raw_fat(128,i)==extension);
        assert(raw_fat(parent,i)==raw_fat(parent,0));
    }
    assert(!f_mount(fs,"0:",1) && f_stat("/PARENT/CHILDX",NULL)==FR_NO_FILE);
    verify_file("/ANCHOR.BIN",payload,size); free(payload);
    BYTE neighbor[512]; memset(neighbor,0xc9,sizeof(neighbor)); verify_file("/NEIGHBOR.BIN",neighbor,512);
    assert(!f_getfree("0:",&after,&mounted));
    printf("{\"dual\":1,\"unknown\":%d,\"child\":%lu,\"extension\":%lu,\"before_free\":%lu,\"after_free\":%lu}\n",
           unknown,(unsigned long)child,(unsigned long)extension,(unsigned long)before,(unsigned long)after);
    assert(!f_mount(NULL,"0:",0) && !fclose(disk));
    return !child && !extension && before==after ? 0 : 42;
}
int main(int argc,char **argv)
{
    assert(argc==7);setvbuf(stdout,NULL,_IONBF,0);
    disk=fopen(argv[1],"r+b");assert(disk && !fseeko(disk,0,SEEK_END));
    sectors=ftello(disk)/512;
    fault_mode=argv[2];partial_clear=atoi(argv[3]);int lookup=atoi(argv[4]);
    int unknown=atoi(argv[6]);
    FATFS fs,*mounted;FIL file;UINT count;BYTE data[512];memset(data,0xc9,sizeof data);
    assert(f_mount(&fs,"0:",1)==FR_OK);
    fat_type=fs.fs_type;fat_copies=fs.n_fats;fat_size=fs.fsize;
    assert(fat_type==(!strcmp(argv[5],"fat12") ? FS_FAT12 :
                      !strcmp(argv[5],"fat16") ? FS_FAT16 : FS_FAT32));
    assert(f_open(&file,"/NEIGHBOR.BIN",FA_CREATE_NEW|FA_WRITE)==FR_OK);
    assert(f_write(&file,data,sizeof data,&count)==FR_OK && count==sizeof data);
    DWORD neighbor_cluster=file.obj.sclust;
    assert(f_close(&file)==FR_OK);
    fatbase=fs.fatbase;
    if (!strcmp(fault_mode,"dual")) return simultaneous_parent(&fs,unknown);
    DWORD before=raw_free_count(&fs),after;
    if (!unknown) assert(f_getfree("0:",&after,&mounted)==FR_OK && before==after);
    else assert(fs.free_clst==UINT32_MAX);
    child_cluster=fs.last_clst+1;
    DWORD neighbor_entry=raw_fat(neighbor_cluster,0);
    child_sector=fs.database+(child_cluster-2)*fs.csize;
    assert(child_cluster<fs.n_fatent && raw_fat(child_cluster,0)==0);
    BYTE marker[512]; memset(marker,0x5a,sizeof(marker));
    for (unsigned i=0;i<fs.csize;i++) {
        assert(!fseeko(disk,(off_t)(child_sector+i)*512,SEEK_SET));
        assert(fwrite(marker,1,sizeof(marker),disk)==sizeof(marker));
    }
    assert(!fflush(disk));
    hold_cleanup=!strcmp(fault_mode,"fat") || !strcmp(fault_mode,"sync") ||
                 !strcmp(fault_mode,"read") || !strcmp(fault_mode,"mirror");
    FIL held;
    if (hold_cleanup) assert(f_open(&held,"/NEIGHBOR.BIN",FA_READ)==FR_OK);
    clear_armed=strcmp(fault_mode,"healthy")!=0;
    FRESULT made=f_mkdir("/CHILDX");
    unsigned failed=strcmp(fault_mode,"healthy")!=0;
    assert(made==(failed?FR_DISK_ERR:FR_OK) && clear_failures==failed);
    if (hold_cleanup) {
        FRESULT unmount=f_mount(NULL,"0:",0);
        printf("held_cleanup made=%u unmount=%u failures=%u\n",made,unmount,cleanup_failures);
        if (unmount!=FR_DISK_ERR || !cleanup_failures || !fs.fs_type) return 42;
        DWORD expected=unknown ? UINT32_MAX : before-!strcmp(fault_mode,"read");
        assert(fs.free_clst==expected); /* A failed FAT read has not credited yet. */
        unsigned previous=cleanup_failures;
        assert(f_open(&file,"/OTHER.BIN",FA_CREATE_NEW|FA_WRITE)==FR_DISK_ERR);
        assert(cleanup_failures>previous);
        assert(fs.free_clst==expected); mutex_is_released();
        BYTE sample[16]; UINT read_count;
        assert(f_read(&held,sample,sizeof(sample),&read_count)==FR_DISK_ERR && !f_tell(&held));
        assert(f_close(&held)==FR_DISK_ERR && held.obj.fs==&fs); mutex_is_released();
        FATFS replacement,unchanged; memset(&replacement,0xa5,sizeof(replacement));
        memcpy(&unchanged,&replacement,sizeof(replacement));
        assert(f_mount(&replacement,"0:",0)==FR_DISK_ERR && !memcmp(&replacement,&unchanged,sizeof(replacement)));
        for (unsigned i=0;i<2;i++) {
            FATFS owner; memcpy(&owner,&fs,sizeof(owner));
            status_override=i ? STA_PROTECT : STA_NOINIT;
            FRESULT refused=i ? FR_WRITE_PROTECTED : FR_NOT_READY;
            assert(f_mount(NULL,"0:",0)==refused && fs.fs_type);
            /* Stock validate reports an unavailable open object as invalid;
             * mount/allocation report not-ready. Neither retires its owner. */
            FRESULT read_refused=i ? FR_WRITE_PROTECTED : FR_INVALID_OBJECT;
            assert(f_read(&held,sample,sizeof(sample),&read_count)==read_refused && held.obj.fs==&fs);
            assert(f_open(&file,"/BLOCKED.BIN",FA_CREATE_NEW|FA_WRITE)==refused);
            assert(!memcmp(&owner,&fs,sizeof(owner))); mutex_is_released();
        }
        status_override=0;
        hold_cleanup=0;
        assert(f_close(&held)==FR_OK && fs.free_clst==(unknown ? UINT32_MAX : before));
    }
    if (lookup) assert(f_stat("/CHILDX",NULL)==(failed?FR_NO_FILE:FR_OK));
    assert(f_mount(NULL,"0:",0)==FR_OK);
    if (failed) {
        assert(!fseeko(disk,(off_t)child_sector*512,SEEK_SET) && fread(marker,1,512,disk)==512);
        for (unsigned i=0;i<512;i++) assert(marker[i]==(partial_clear && i<256 ? 0 : 0x5a));
    }
    DWORD raw=raw_fat(child_cluster,0);
    for (unsigned copy=1;copy<fat_copies;++copy) assert(raw_fat(child_cluster,copy)==raw);
    /* FAT12 packs the adjacent live entry into the same bytes. Check its
     * full value, rather than relying only on a one-cluster payload read. */
    for (unsigned copy=0;copy<fat_copies;++copy)
        assert(raw_fat(neighbor_cluster,copy)==neighbor_entry);
    assert(f_mount(&fs,"0:",1)==FR_OK);
    FRESULT found=f_stat("/CHILDX",NULL);
    assert(f_getfree("0:",&after,&mounted)==FR_OK);
    verify_file("/NEIGHBOR.BIN",data,512);
    printf("{\"format\":\"%s\",\"made\":%u,\"found\":%u,\"cluster\":%lu,\"raw_fat\":%lu,"
           "\"before_free\":%lu,\"after_free\":%lu,\"clear_failures\":%u,\"cleanup_failures\":%u,"
           "\"reads\":%u,\"writes\":%u,\"syncs\":%u,\"filesystem_bytes\":%zu}\n",
           argv[5],made,found,(unsigned long)child_cluster,(unsigned long)raw,(unsigned long)before,
           (unsigned long)after,clear_failures,cleanup_failures,io_reads,io_writes,io_syncs,sizeof(fs));
    assert(f_mount(NULL,"0:",0)==FR_OK && !fclose(disk));
    if (failed) return raw==0 && found==FR_NO_FILE && before==after ? 0 : 42;
    DWORD eoc=fat_type==FS_FAT12 ? 0xff8 : fat_type==FS_FAT16 ? 0xfff8 : 0x0ffffff8;
    return raw>=eoc && found==FR_OK && before-after==1 ? 0 : 42;
}
