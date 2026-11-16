/* Adapted from paps-g 8f8b63c5 (SSR-106). Public save startup/FatFs;
 * generated host media and block I/O only. */
#define main original_startup_main
#define disk_read ordinary_disk_read
#include "mcu_save_startup.c"
#undef disk_read
#undef main

static unsigned reads, injected_read, io_failures;
DRESULT disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
    reads++;
    if (injected_read && reads == injected_read) { io_failures++; return RES_ERROR; }
    return ordinary_disk_read(drive, data, sector, count);
}
static void neighbor(void)
{
    FIL file; UINT count; u8 bytes[1024];
    assert(f_open(&file, "/NEIGHBOR.BIN", FA_READ) == FR_OK);
    assert(f_read(&file, bytes, sizeof bytes, &count) == FR_OK && count == sizeof bytes);
    for (unsigned i=0;i<sizeof bytes;i++) assert(bytes[i] == 0xc9);
    assert(f_close(&file) == FR_OK);
}
static void report(const char *mode, FRESULT result, unsigned count, unsigned before,
                   DWORD mounted_count, DWORD mounted_hint, DWORD free_count)
{
    printf("{\"mode\":\"%s\",\"result\":%u,\"bytes\":%u,\"mount_reads\":%u,"
           "\"operation_reads\":%u,\"writes\":%u,\"mounted_count\":%lu,"
           "\"mounted_hint\":%lu,\"free_count\":%lu,\"io_failures\":%u,\"api_requested_bytes\":%u}\n",
           mode,result,count,before,reads-before,disk_writes,(unsigned long)mounted_count,
           (unsigned long)mounted_hint,(unsigned long)free_count,io_failures,write_bytes);
}
int main(int argc,char **argv)
{
    assert(argc == 4); setvbuf(stdout,NULL,_IONBF,0);
    const char *mode=argv[2]; DWORD expected=(DWORD)strtoul(argv[3],NULL,10);
    disk=fopen(argv[1],"r+b"); assert(disk);
    assert(!fseeko(disk,0,SEEK_END)); disk_sectors=ftello(disk)/512;
    FATFS fs; FIL file; UINT count=0; u8 bytes[1024]; memset(bytes,0xc9,sizeof bytes);
    assert(f_mount(&fs,"0:",1) == FR_OK);
    unsigned mount_reads=reads; DWORD mounted_count=fs.free_clst, mounted_hint=fs.last_clst;
    fault="none";
    if (!strcmp(mode,"prepare")) {
        assert(f_mkdir("/SAROO") == FR_OK);
        assert(f_open(&file,"/NEIGHBOR.BIN",FA_CREATE_NEW|FA_WRITE) == FR_OK);
        assert(f_write(&file,bytes,sizeof bytes,&count) == FR_OK && count == sizeof bytes);
        assert(f_close(&file) == FR_OK);
    } else if (!strcmp(mode,"allocate") || !strcmp(mode,"full")) {
        assert(f_open(&file,"/TARGET.BIN",FA_CREATE_NEW|FA_READ|FA_WRITE) == FR_OK);
        unsigned before=reads; FRESULT result=f_write(&file,bytes,sizeof bytes,&count);
        report(mode,result,count,before,mounted_count,mounted_hint,UINT32_MAX);
        if (!strcmp(mode,"full")) {
            before=reads;
            UINT again;
            assert(f_write(&file,bytes,sizeof bytes,&again) == FR_OK && !again);
            report("full-repeat",FR_OK,again,before,mounted_count,mounted_hint,UINT32_MAX);
        }
        assert(f_close(&file) == FR_OK);
        neighbor(); assert(f_mount(NULL,"0:",0) == FR_OK);
        if (result == FR_OK && count == sizeof bytes) {
            assert(f_mount(&fs,"0:",1) == FR_OK);
            assert(f_open(&file,"/TARGET.BIN",FA_READ) == FR_OK && f_size(&file) == sizeof bytes);
            UINT read_count; memset(bytes,0,sizeof bytes);
            assert(f_read(&file,bytes,sizeof bytes,&read_count) == FR_OK && read_count == sizeof bytes);
            for (unsigned i=0;i<sizeof bytes;i++) assert(bytes[i] == 0xc9);
            assert(f_close(&file) == FR_OK && f_mount(NULL,"0:",0) == FR_OK);
        }
        assert(!fclose(disk));
        return result == FR_OK && count == (!strcmp(mode,"full") ? 0 : sizeof bytes) ? 0 : 42;
    } else if (!strcmp(mode,"query") || !strcmp(mode,"read-failure")) {
        DWORD free_count=0x12345678; FATFS *found=NULL;
        if (!strcmp(mode,"read-failure")) injected_read=reads+1;
        FRESULT result=f_getfree("0:",&free_count,&found);
        report(mode,result,0,mount_reads,mounted_count,mounted_hint,free_count);
        if (injected_read) {
            if (result != FR_DISK_ERR || free_count != 0x12345678 || !io_failures) return 42;
            injected_read=0;
            assert(f_getfree("0:",&free_count,&found) == FR_OK);
        }
        if (result != FR_OK && !io_failures) return 42;
        if (free_count != expected || found != &fs) return 42;
        unsigned before=reads;
        assert(f_getfree("0:",&free_count,&found) == FR_OK && free_count == expected && reads == before);
        neighbor();
    } else {
        assert(!strcmp(mode,"startup")); reset_service();
        unsigned before=reads; int result=open_savefile();
        report(mode,result,0,before,mounted_count,mounted_hint,UINT32_MAX);
        if (result) { assert(!f_mount(NULL,"0:",0)); assert(!fclose(disk)); return 42; }
        assert(f_size(&ss_fp) == 65536 && f_size(&sm_fp) == 0x800000);
        assert(f_close(&ss_fp) == FR_OK && f_close(&sm_fp) == FR_OK);
        assert(f_mount(NULL,"0:",0) == FR_OK && f_mount(&fs,"0:",1) == FR_OK);
        reset_service(); before=disk_writes;
        assert(open_savefile() == 0 && disk_writes == before);
        assert(f_close(&ss_fp) == FR_OK && f_close(&sm_fp) == FR_OK);
        neighbor();
    }
    assert(f_mount(NULL,"0:",0) == FR_OK); assert(!fclose(disk));
    return 0;
}
