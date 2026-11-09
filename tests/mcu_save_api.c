/* Adapted from paps-g 1f90bc28. Coupled source test: actual Saturn API -> actual MCU -> actual FatFs.
 * Backup structs use host byte order here; this is not an SH-2 bus emulator. */
#define main startup_test_main
#include "mcu_save_startup.c"
#undef main
static u8 test_ip[17] = "TESTGAME00000001";
#define TEST_IP_ADDR ((uintptr_t)test_ip)
#include "game_save.c"

u16 test_arg;
static u16 command;
static int fault_only_flush;
static unsigned fail_window;
static unsigned header_commands, window_commands, flush_commands;
static u16 *test_command(void)
{
    if (!command) return &command;
    int saved_armed = armed, result;
    int flushing = command == SSCMD_SSAVE || command == SSCMD_SMEMS;
    if (fault_only_flush && !flushing) armed = 0;
    if (command == SSCMD_SSAVE) result = flush_savefile();
    else if (command == SSCMD_SMEMS) result = flush_smems(test_arg);
    else {
        assert(command == SSCMD_LMEMS);
        if (test_arg & START_BLOCK) header_commands++;
        else {
            window_commands++;
            if (fail_window && window_commands == fail_window) {
                disk_read_fault = 1; fail_window = 0;
            }
        }
        result = load_smems(test_arg);
    }
    if (flushing) flush_commands++;
    test_arg = result; command = 0; armed = saved_armed;
    return &command;
}
static void reset_counts(void)
{
    api_reads = api_writes = api_seeks = api_syncs = read_bytes = write_bytes = 0;
    header_commands = window_commands = flush_commands = 0;
    fault_seen = hits = 0;
}
static int phase(int dev)
{
#ifdef SAVE_WRITING
    return pending_save[dev].state;
#else
    (void)dev; return 0;
#endif
}
static u8 *read_live(FIL *fp)
{
    UINT count; u8 *out = malloc(f_size(fp)); assert(out);
    assert(f_lseek(fp,0) == FR_OK);
    assert(f_read(fp,out,f_size(fp),&count) == FR_OK && count == f_size(fp));
    return out;
}
static void reload_buffers(void)
{
    UINT count;
    assert(f_lseek(&ss_fp,ss_index*65536) == 0);
    assert(f_read(&ss_fp,test_shared,65536,&count) == 0 && count == 65536);
    assert(f_lseek(&sm_fp,0) == 0);
    assert(f_read(&sm_fp,(void*)MEMS_HEADER,8192,&count) == 0 && count == 8192);
    msav_blk = mblk_start = mblk_end = sm_index0 = sm_index1 = 0;
    mblk_bmp[0] = mblk_bmp[1] = 0;
#ifdef SAVE_WRITING
    memset(pending_save,0,sizeof(pending_save));
#endif
}
static BUPDIR record(const char *name, int size)
{
    BUPDIR result = {0};
    memcpy(result.file_name,name,11); memcpy(result.comment,"Preserve!",9);
    result.data_size = size; result.date = 12345; result.language = 1;
    return result;
}
static u8 *data_bytes(int size, int seed)
{
    u8 *data = malloc(size ? size : 1); assert(data);
    for (int i=0;i<size;i++) data[i] = (i*7+seed)%251;
    return data;
}
static BUPDIR target, neighbor;
static u8 *original, *keep;
static void fixture(int dev, int size)
{
    target = record("TEST0000001",size); neighbor = record("KEEP0000001",32);
    original = data_bytes(size,7); keep = data_bytes(32,19);
    assert(sro_bup_write(dev,&target,original,0) == 0);
    assert(sro_bup_write(dev,&neighbor,keep,0) == 0);
    reload_buffers(); reset_counts();
}
static void check_record(int dev, BUPDIR *dir, const u8 *data)
{
    u8 *out = malloc(dir->data_size+16); assert(out); memset(out,0xcc,dir->data_size+16);
    assert(sro_bup_read(dev,dir->file_name,out) == 0);
    assert(!memcmp(out,data,dir->data_size));
    for (int i=0;i<16;i++) assert(out[dir->data_size+i] == 0xcc);
    assert(sro_bup_verify(dev,dir->file_name,out) == 0);
    BUPDIR actual = {0};
    assert(sro_bup_dir(dev,dir->file_name,1,&actual) == 1);
    assert(actual.data_size == dir->data_size && actual.date == dir->date);
    assert(!memcmp(actual.comment,dir->comment,10) && actual.language==dir->language);
    free(out);
}
static void check_unchanged(const u8 *slot, const u8 *mems)
{
    u8 *now = read_live(&ss_fp); assert(!memcmp(now,slot,f_size(&ss_fp))); free(now);
    now = read_live(&sm_fp); assert(!memcmp(now,mems,f_size(&sm_fp))); free(now);
}
static void read_failure(const char *mode)
{
    int large = !strcmp(mode,"read") || !strcmp(mode,"verify");
    fixture(1,large ? 131073 : 32);
    u8 *slot = read_live(&ss_fp), *mems = read_live(&sm_fp);
    u8 metadata[8192]; memcpy(metadata,(void*)MEMS_HEADER,8192);
    u8 *out = malloc(target.data_size); assert(out); memset(out,0xcc,target.data_size);
    BUPSTAT stat, previous; memset(&stat,0xcc,sizeof(stat)); previous=stat;
    armed=1; int result;
    if (!strcmp(mode,"read")) result=sro_bup_read(1,target.file_name,out);
    else if (!strcmp(mode,"verify")) result=sro_bup_verify(1,target.file_name,original);
    else if (!strcmp(mode,"stat")) result=sro_bup_stat(1,32,&stat);
    else if (!strcmp(mode,"delete")) result=sro_bup_delete(1,target.file_name);
    else {
        assert(!strcmp(mode,"new-header"));
        BUPDIR next=record("NEW00000001",32);
        result=sro_bup_write(1,&next,keep,0);
    }
    assert(result == BUP_BROKEN && hits == 1 && !api_writes && phase(1) == 0);
    assert(!memcmp(&stat,&previous,sizeof(stat)));
    assert(!memcmp(metadata,(void*)MEMS_HEADER,8192));
    if (!strcmp(mode,"read")) {
        int copied = fault_nth == 5 ? 65536 : 0;
        assert(!memcmp(out,original,copied));
        for (unsigned i=copied;i<target.data_size;i++) assert(out[i]==0xcc);
    }
    armed=0;check_unchanged(slot,mems);
    check_record(1,&target,original);check_record(1,&neighbor,keep);
    free(slot);free(mems);free(out);
}
static void interrupted_write(const char *mode)
{
    if (!strcmp(mode,"overlap-large")) {
        BUPDIR filler=record("FILL0000001",61*1024);
        u8 *bytes=data_bytes(filler.data_size,3);
        assert(sro_bup_write(1,&filler,bytes,0)==0);free(bytes);
        fixture(1,2048);
        assert(sro_bup_delete(1,filler.file_name)==0);
        reload_buffers();reset_counts();
    } else fixture(1,!strcmp(mode,"overwrite-large") ? 131073 : 32);
    BUPDIR request=target;
    if (!strcmp(mode,"new-large")) memcpy(request.file_name,"NEW00000001",11);
    request.data_size=131073;request.date++;
    u8 *first=data_bytes(request.data_size,43), *retry=data_bytes(request.data_size,71);
    armed=1;
    assert(sro_bup_write(1,&request,first,0)==BUP_BROKEN && hits==1 && phase(1)==1);
    if (!strncmp(fault,"write-",6) || !strcmp(fault,"sync-error")) {
        assert(mblk_start && (mblk_bmp[0] || mblk_bmp[1]));
    }
    unsigned calls=api_reads+api_writes+api_syncs;
    BUPSTAT stat;
    assert(sro_bup_stat(1,32,&stat)==BUP_BROKEN);
    BUPDIR wrong=request;wrong.data_size--;
    assert(sro_bup_write(1,&wrong,retry,0)==BUP_BROKEN);
    wrong=request;memcpy(wrong.file_name,"OTHER000001",11);
    assert(sro_bup_write(1,&wrong,retry,0)==BUP_BROKEN);
    assert(sro_bup_write(1,&request,retry,1)==BUP_BROKEN);
    assert(api_reads+api_writes+api_syncs==calls);
    /* An independent slot device remains usable while MEMS is unfinished. */
    assert(sro_bup_stat(0,32,&stat)==0);
    armed=0;request.date++;
    assert(sro_bup_write(1,&request,retry,0)==0 && phase(1)==0);
    reload_buffers();check_record(1,&request,retry);check_record(1,&neighbor,keep);
    if (!strcmp(mode,"new-large")) check_record(1,&target,original);
    free(first);free(retry);
}
static void commit_failure(const char *mode, int dev)
{
    fixture(dev,32);fault_only_flush=1;
    BUPDIR request=target;request.date++;
    int resupply = !strncmp(mode,"commit-resupply",15);
    int new_record = !strcmp(mode,"commit-new") || resupply;
    if (new_record) memcpy(request.file_name,"NEW00000001",11);
    if (!strcmp(mode,"commit-resupply-short")) {
        memset(request.file_name,0,12);memcpy(request.file_name,"SHORT",5);
    }
    u8 *data=data_bytes(32,43),*retry=data_bytes(32,71);
    armed=1;int result;
    if (!strcmp(mode,"format")) result=sro_bup_format(dev);
    else if (!strcmp(mode,"delete-commit")) result=sro_bup_delete(dev,target.file_name);
    else result=sro_bup_write(dev,&request,data,new_record);
    assert(result==BUP_BROKEN && hits==1 && phase(dev)==2);
    /* A second failed attempt must also report failure and retain the commit. */
    fault_seen=hits=0;BUPSTAT stat,old;memset(&stat,0xcc,sizeof(stat));old=stat;
    assert(sro_bup_stat(dev,32,&stat)==BUP_BROKEN && hits==1 && phase(dev)==2);
    assert(!memcmp(&stat,&old,sizeof(stat)));
    armed=0;
    if (resupply) {
        request.date++;
        if (!strcmp(mode,"commit-resupply-short")) {
            memset(request.file_name+6,0x7a,5);
            memcpy(request.comment,"Retried!  ",10);request.language=2;
        }
        assert(sro_bup_write(dev,&request,retry,1)==0);
    } else assert(sro_bup_stat(dev,32,&stat)==0);
    assert(phase(dev)==0);reload_buffers();
    if (!strcmp(mode,"format")) {
        assert(sro_bup_stat(dev,32,&stat)==0 && stat.free_block==(unsigned)(dev?8056:511));
        assert(sro_bup_dir(dev,"",0,NULL)==0);
        assert(sro_bup_write(dev,&request,retry,0)==0);reload_buffers();check_record(dev,&request,retry);
    } else if (!strcmp(mode,"delete-commit")) {
        assert(sro_bup_read(dev,target.file_name,data)==BUP_NOT_FOUND);
        check_record(dev,&neighbor,keep);
    } else {
        check_record(dev,&request,resupply?retry:data);
        check_record(dev,&neighbor,keep);
        if (new_record) check_record(dev,&target,original);
    }
    free(data);free(retry);
}
static void directory_fixture(void)
{
    for (int i=0;i<64;i++) {
        char name[12];snprintf(name,sizeof(name),"IDX%08d",i);
        BUPDIR d=record(name,i==63 ? 2048 : 32);
        u8 *payload=data_bytes(d.data_size,i);
        assert(sro_bup_write(1,&d,payload,0)==0);free(payload);
    }
    reload_buffers();reset_counts();
}
static void directory_case(const char *mode, int copies)
{
    directory_fixture();BUPDIR out[448],before[448];
    memset(out,0xcc,sizeof(out));memcpy(before,out,sizeof(out));
    u8 *slot=read_live(&ss_fp),*mems=read_live(&sm_fp);
    if (!strcmp(mode,"directory-fault")) {
        armed=1;assert(sro_bup_dir(1,"",64,out)==0 && hits==1 && !api_writes);
        assert(!memcmp(out,before,sizeof(out)));
        armed=0;assert(sro_bup_dir(1,"",64,out)==64);
    } else if (!strcmp(mode,"directory-count")) {
        assert(sro_bup_dir(1,"",copies,copies?out:NULL)==(copies<64 ? -64 : 64));
        int n=copies<64 ? copies : 64;
        for (int i=0;i<n;i++) {
            char name[12];snprintf(name,sizeof(name),"IDX%08d",i);
            assert(!memcmp(out[i].file_name,name,12));
            assert(out[i].data_size==(unsigned)(i==63?2048:32));
        }
        assert(!memcmp(out+n,before+n,(448-n)*sizeof(*out)));
    } else {
        u8 payload[2048],buffer[65536];
        assert(sro_bup_read(1,"IDX00000063",payload)==0);
        int window=mblk_start;assert(window);
        memcpy(buffer,(void*)MEMS_BUFFER,65536);reset_counts();
        int small=!strcmp(mode,"directory-warm-small");
        assert(sro_bup_dir(1,small?"IDX00000063":"",small?1:64,out)==(small?1:64));
        assert(!api_writes && !window_commands);
        if (small) assert(mblk_start==window && !memcmp(buffer,(void*)MEMS_BUFFER,65536));
        reset_counts();assert(sro_bup_read(1,"IDX00000063",payload)==0);
        /* With the large table's borrowed cache, this cold lookup batches
         * the 64 headers once, then reloads the requested payload window. */
        assert(window_commands<=(unsigned)(small?0:2) && !api_writes);
        u8 *expected=data_bytes(2048,63);assert(!memcmp(payload,expected,2048));free(expected);
    }
    check_unchanged(slot,mems);free(slot);free(mems);
}
static void disk_retry(int writing)
{
    fixture(1,32);BUPDIR request=target;request.data_size=131073;
    u8 *data=data_bytes(request.data_size,43),*retry=data_bytes(request.data_size,71);
    if (writing) {
        fail_window=2;
        assert(sro_bup_write(1,&request,data,0)==BUP_BROKEN && phase(1)==1);
        assert(sm_fp.err==FR_DISK_ERR);
        assert(sro_bup_write(1,&request,retry,0)==BUP_BROKEN && phase(1)==1);
    } else {
        disk_read_fault=1;memset(data,0xcc,32);
        assert(sro_bup_read(1,target.file_name,data)==BUP_BROKEN && sm_fp.err==FR_DISK_ERR);
        assert(sro_bup_read(1,target.file_name,data)==BUP_BROKEN);
        for (int i=0;i<32;i++) assert(data[i]==0xcc);
    }
    /* Only this test fixture reopens the MCU file; there is no production
     * automatic recovery command yet, and no reset of the pending payload. */
    assert(f_close(&sm_fp)==FR_OK);
    assert(f_open(&sm_fp,SMEMS_FILE,FA_READ|FA_WRITE)==FR_OK);
    if (writing) {
        assert(sro_bup_write(1,&request,retry,0)==0 && phase(1)==0);
        reload_buffers();check_record(1,&request,retry);
    } else check_record(1,&target,original);
    check_record(1,&neighbor,keep);free(data);free(retry);
}
int main(int argc,char **argv)
{
    assert(argc==6);const char *mode=argv[2];int dev=atoi(argv[3]);
    fault=argv[4];fault_nth=atoi(argv[5]);fault_target=dev;
    disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(f_mount(&fs,"0:",1)==0);
    assert(f_mkdir("/SAROO")==0);make_archive(0,65536,0);make_archive(1,0x800000,0);
    assert(open_savefile()==0);assert(load_savefile((char*)test_ip)==0);
    assert(sro_bup_format(0)==0 && sro_bup_format(1)==0);
    if (!strncmp(mode,"directory-",10)) directory_case(mode,fault_nth);
    else if (!strncmp(mode,"disk-retry-",11)) disk_retry(!strcmp(mode,"disk-retry-write"));
    else if (strstr(mode,"large")) interrupted_write(mode);
    else if (!strncmp(mode,"commit-",7) || !strcmp(mode,"format") || !strcmp(mode,"delete-commit")) commit_failure(mode,dev);
    else read_failure(mode);
    assert(!lock_depth && !command);
    printf("coupled %s dev=%d fault=%s nth=%d header=%u window=%u flush=%u reads=%u writes=%u\n",
           mode,dev,fault,fault_nth,header_commands,window_commands,flush_commands,api_reads,api_writes);
    assert(f_close(&ss_fp)==0 && f_close(&sm_fp)==0);free(original);free(keep);assert(!fclose(disk));return 0;
}
