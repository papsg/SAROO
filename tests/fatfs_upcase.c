/* Adapted from paps-g SSR-163 / f3714b9e. Real public FatFs with host FILE
 * sectors and per-volume pthread grants; no SAROO device operation. */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
_Static_assert(sizeof(DWORD)==4 && sizeof(WORD)==2, "filesystem scalar ABI");
static FILE *disks[FF_VOLUMES];
static LBA_t capacity[FF_VOLUMES];
static pthread_mutex_t grants[FF_VOLUMES+1];
static unsigned reads[FF_VOLUMES], writes[FF_VOLUMES], table_reads;
static unsigned fail_read, fail_table;
static LBA_t table_start, table_end;
int ff_mutex_create(int vol) { assert(vol>=0 && vol<=FF_VOLUMES);return !pthread_mutex_init(&grants[vol],0); }
void ff_mutex_delete(int vol) { assert(vol>=0 && vol<=FF_VOLUMES);assert(!pthread_mutex_destroy(&grants[vol])); }
int ff_mutex_take(int vol) { assert(vol>=0 && vol<=FF_VOLUMES);return !pthread_mutex_lock(&grants[vol]); }
void ff_mutex_give(int vol) { assert(vol>=0 && vol<=FF_VOLUMES);assert(!pthread_mutex_unlock(&grants[vol])); }
DSTATUS disk_initialize(BYTE drive) { return drive<FF_VOLUMES && disks[drive] ? 0 : STA_NOINIT; }
DSTATUS disk_status(BYTE drive) { return disk_initialize(drive); }
DRESULT disk_read(BYTE drive,BYTE *data,LBA_t sector,UINT count)
{
    assert(drive<FF_VOLUMES && disks[drive] && count && sector<=capacity[drive] && count<=capacity[drive]-sector);
    reads[drive]++;
    if (fail_read && reads[drive]==fail_read) return RES_ERROR;
    if (!drive && sector<table_end && sector+count>table_start) {
        table_reads++;
        if (fail_table && table_reads==fail_table) return RES_ERROR;
    }
    assert(!fseeko(disks[drive],(off_t)sector*512,SEEK_SET));
    return fread(data,512,count,disks[drive])==count ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drive,const BYTE *data,LBA_t sector,UINT count)
{
    assert(drive<FF_VOLUMES && disks[drive] && count && sector<=capacity[drive] && count<=capacity[drive]-sector);
    writes[drive]++;
    assert(!fseeko(disks[drive],(off_t)sector*512,SEEK_SET));
    return fwrite(data,512,count,disks[drive])==count ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive,BYTE command,void *out)
{
    assert(drive<FF_VOLUMES && disks[drive]);
    if (command==CTRL_SYNC) return fflush(disks[drive]) ? RES_ERROR : RES_OK;
    if (command==GET_SECTOR_COUNT) { *(LBA_t*)out=capacity[drive];return RES_OK; }
    if (command==GET_BLOCK_SIZE) { *(DWORD*)out=1;return RES_OK; }
    if (command==GET_SECTOR_SIZE) { *(WORD*)out=512;return RES_OK; }
    return RES_PARERR;
}
#include "ff.c"
static void open_disk(unsigned drive,const char *path)
{
    assert(drive<FF_VOLUMES);disks[drive]=fopen(path,"r+b");assert(disks[drive]);
    assert(!fseeko(disks[drive],0,SEEK_END));off_t bytes=ftello(disks[drive]);assert(bytes>0 && bytes%512==0);capacity[drive]=bytes/512;
}
static FRESULT payload(const char *name,BYTE value)
{
    FIL fp={0};FRESULT res=f_open(&fp,name,FA_READ);if(res)return res;
    BYTE data[512];UINT got=0;assert(f_size(&fp)==512);
    assert(!f_read(&fp,data,sizeof(data),&got) && got==512);
    for(unsigned i=0;i<512;i++)assert(data[i]==value);
    assert(!f_close(&fp));return FR_OK;
}
#if !FF_FS_READONLY
static void write_value(const char *name, BYTE value)
{
    FIL fp={0};BYTE data[512];UINT put=0;memset(data,value,sizeof(data));
    assert(!f_open(&fp,name,FA_CREATE_NEW|FA_WRITE));assert(!f_write(&fp,data,sizeof(data),&put) && put==sizeof(data));assert(!f_close(&fp));
}
static void create_payload(const char *name) { write_value(name,0x5a); }
#endif
#if FF_VOLUMES>=2
static void *parallel_read(void *argument)
{
    unsigned drive=(unsigned)(uintptr_t)argument;char upper[40],lower[40];
    snprintf(upper,sizeof upper,"%u:/\xe2\xb1\xaf.BIN",drive);
    snprintf(lower,sizeof lower,"%u:/\xc9\x90.BIN",drive);
    for(unsigned i=0;i<40;i++){
        assert(payload(upper,0xc9)==FR_OK);
        assert(payload(lower,0xc9)==(drive ? FR_OK : FR_NO_FILE));
    }
    return 0;
}
#endif
int main(int argc,char **argv)
{
    assert(argc>=3);setvbuf(stdout,0,_IONBF,0);const char *mode=argv[2];open_disk(0,argv[1]);FATFS fs={0};
    if(!strcmp(mode,"mount-fault")) { assert(argc==4);fail_read=(unsigned)strtoul(argv[3],0,10); }
#if FF_USE_MKFS && !FF_FS_READONLY
    if(!strcmp(mode,"format")) {
        BYTE work[32768];MKFS_PARM options={.fmt=FM_EXFAT|FM_SFD,.n_fat=1,.au_size=512};
        assert(!f_mkfs("0:",&options,work,sizeof(work)));
    }
#endif
    FRESULT result=f_mount(&fs,"0:",1);unsigned mount_reads=reads[0];
    if(!strcmp(mode,"refuse")) {
        assert(result==FR_NO_FILESYSTEM && !fs.fs_type && !writes[0]);
    }else if(!strcmp(mode,"mount-fault")) {
        assert(result==FR_DISK_ERR && !fs.fs_type && !writes[0]);
        fail_read=0;assert(!f_mount(&fs,"0:",1));assert(payload("/\xc9\x90.BIN",0xc9)==FR_OK);
    }else{
        assert(result==FR_OK && fs.fs_type==FS_EXFAT);
        if(!strcmp(mode,"maps")) {
#ifndef SSR163_BASELINE
            assert(argc==4);FILE *oracle=fopen(argv[3],"rb");assert(oracle);unsigned char bytes[2];WCHAR actual;
            ff_mutex_take(0);
            for(unsigned code=0;code<65536;code++){
                assert(fread(bytes,1,2,oracle)==2);unsigned expected=bytes[0]|bytes[1]<<8;
                FRESULT r=upcase_char(&fs,(WCHAR)code,&actual);
                if(r || actual!=expected){fprintf(stderr,"map=%X expected=%X actual=%X result=%u\n",code,expected,actual,(unsigned)r);return 42;}
            }
            ff_mutex_give(0);assert(fgetc(oracle)==EOF && !fclose(oracle));assert(!writes[0]);
#else
            assert(0 && "mapping API unavailable in baseline");
#endif
        }else if(!strcmp(mode,"lookup-old")) {
#ifdef SSR163_BASELINE
            assert(payload("/\xe2\xb1\xaf.BIN",0xc9)==FR_OK);
            assert(payload("/\xc9\x90.BIN",0xc9)==FR_NO_FILE);
            assert(!writes[0]);
#else
            assert(0 && "baseline control only");
#endif
        }else if(!strcmp(mode,"lookup") || !strcmp(mode,"lookup-fault")) {
            fs.winsect=(LBA_t)-1;reads[0]=0;
            if(!strcmp(mode,"lookup-fault")){assert(argc==4);fail_read=(unsigned)strtoul(argv[3],0,10);}
            FIL fp={0};FRESULT r=f_open(&fp,"/\xc9\x90.BIN",FA_READ);
            if(fail_read){assert(r==FR_DISK_ERR && !writes[0]);fail_read=0;assert(payload("/\xc9\x90.BIN",0xc9)==FR_OK);}
            else {assert(r==FR_OK);assert(!f_close(&fp));}
        }else if(!strcmp(mode,"format")) {
#if FF_USE_MKFS && !FF_FS_READONLY
            write_value("/\xe2\xb1\xaf.BIN",0xc9);write_value("/KEEP.BIN",0xc9);
#else
            assert(0);
#endif
        }else if(!strcmp(mode,"ops")) {
#if !FF_FS_READONLY
            create_payload("/\xc9\x90.NEW");assert(payload("/\xe2\xb1\xaf.NEW",0x5a)==FR_OK);
            unsigned before=writes[0];FIL duplicate={0};assert(f_open(&duplicate,"/\xe2\xb1\xaf.NEW",FA_CREATE_NEW|FA_WRITE)==FR_EXIST && writes[0]==before);
            assert(!f_rename("/\xe2\xb1\xaf.NEW","/\xc9\x90.REN"));assert(payload("/\xe2\xb1\xaf.REN",0x5a)==FR_OK);
            assert(!f_mkdir("/\xc9\x90.DIR"));create_payload("/\xe2\xb1\xaf.DIR/CHILD.BIN");assert(payload("/\xc9\x90.DIR/child.bin",0x5a)==FR_OK);
            assert(!f_unlink("/\xe2\xb1\xaf.DIR/CHILD.BIN") && !f_unlink("/\xe2\xb1\xaf.DIR"));
            assert(payload("/\xe2\xb1\xaf.BIN",0xc9)==FR_OK && payload("/KEEP.BIN",0xc9)==FR_OK);
#else
            assert(0);
#endif
        }else if(!strcmp(mode,"create-probe") || !strcmp(mode,"create-fault")) {
#if !FF_FS_READONLY
            assert(argc==6);table_start=(LBA_t)strtoul(argv[3],0,10);table_end=(LBA_t)strtoul(argv[4],0,10);fail_table=(unsigned)strtoul(argv[5],0,10);table_reads=0;fs.winsect=(LBA_t)-1;
            FIL fp={0};FRESULT r=f_open(&fp,"/\xc9\x90.NEW",FA_CREATE_NEW|FA_WRITE);
            if(fail_table){assert(r==FR_DISK_ERR && !writes[0]);}
            else{assert(r==FR_OK && !f_close(&fp));}
            fail_table=0;
#else
            assert(0);
#endif
        }else if(!strcmp(mode,"two-volumes")) {
#if FF_VOLUMES>=2
            assert(argc==4);open_disk(1,argv[3]);FATFS other={0};assert(!f_mount(&other,"1:",1));
            pthread_t a,b;assert(!pthread_create(&a,0,parallel_read,0) && !pthread_create(&b,0,parallel_read,(void*)(uintptr_t)1));assert(!pthread_join(a,0) && !pthread_join(b,0));
            assert(!writes[0] && !writes[1]);assert(!f_mount(0,"1:",0) && !fclose(disks[1]));
#else
            assert(0);
#endif
        }else assert(!strcmp(mode,"mount"));
    }
    assert(!f_mount(0,"0:",0));assert(!fclose(disks[0]));
#ifdef SSR163_BASELINE
    size_t index_bytes = 0;
#else
    size_t index_bytes = sizeof(fs.upcase_index);
#endif
    printf("{\"mode\":\"%s\",\"mount_reads\":%u,\"reads\":%u,\"writes\":%u,\"table_reads\":%u,\"fatfs_bytes\":%zu,\"index_bytes\":%zu}\n",mode,mount_reads,reads[0],writes[0],table_reads,sizeof(FATFS),index_bytes);
    return 0;
}
