/* Actual menu/MCU/FatFs exchange; adapted from paps-g 4df2e67a. */
#include "ff.h"
#include "diskio.h"
#include "mode.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
static u8 *test_shared, *target;
static u32 target_bus=0x06004000, allocation;
#define FIRM_ADDR ((uintptr_t)test_shared)
#define TMPBUFF_ADDR ((uintptr_t)(test_shared+0xc7000))
#include "protocol.inc"
static uint16_t file_ack;
static uint16_t archive_ack __attribute__((unused));
#define SSLOG(...) ((void)0)
static volatile uint16_t mcu_cmd, mcu_arg;
#define SS_CMD mcu_cmd
#define SS_ARG mcu_arg
static __attribute__((unused)) int save_archive_io(unsigned device, u32 offset,
                                                   void *buffer, int length)
{
    (void)device; (void)offset; (void)buffer; (void)length;
    abort(); /* This fixture sends only ordinary file commands. */
}
#include "mcu-file.inc"
#undef SS_CMD
#undef SS_ARG
static unsigned commands;
static int peer_fault;
enum { RESET_BEFORE_IO = 5, RESET_AFTER_IO, INVALID_POSITIVE_REPLY };
static volatile uint16_t *command_register(void)
{
    if (mcu_cmd) {
        ++commands;
        assert((mcu_cmd & SSCMD_SAVE_ACK) == (HAS_FILE_ACK ? file_ack : 0));
        if (peer_fault==1 || peer_fault==3 || peer_fault==4) {
            *(u32*)(TMPBUFF_ADDR+4)=peer_fault==1 ? *(u32*)(TMPBUFF_ADDR+4)+1 : peer_fault==3 ? 0x80000000u : 0xffffffffu;
            mcu_arg=mcu_cmd & SSCMD_SAVE_ACK; mcu_cmd=0;
        }
        else if (peer_fault==2 && commands==2) { *(u32*)(TMPBUFF_ADDR+4)=0; mcu_arg=(uint16_t)-FR_DISK_ERR; mcu_cmd=0; }
        else if (peer_fault==RESET_BEFORE_IO || peer_fault==RESET_AFTER_IO) {
            if (peer_fault==RESET_AFTER_IO) command();
            mcu_arg=0;
            mcu_cmd=0;
        } else if (peer_fault==INVALID_POSITIVE_REPLY) {
            mcu_arg=(mcu_cmd & SSCMD_SAVE_ACK) | 1;
            mcu_cmd=0;
        } else command();
    }
    return &mcu_cmd;
}
static u32 host_bus_address(void *p)
{
    uintptr_t address=(uintptr_t)p;
    if (address>=(uintptr_t)test_shared && address<(uintptr_t)test_shared+0x1000000)
        return 0x02000000+(u32)(address-(uintptr_t)test_shared);
    if (address>=(uintptr_t)target && address<(uintptr_t)target+allocation)
        return target_bus+(u32)(address-(uintptr_t)target);
    return p ? 0x06001000 : 0; /* A local byte uses the ordinary bounce path. */
}
#define SS_CMD (*command_register())
#define SS_ARG mcu_arg
/* Cache behavior has its own fixture. This one checks the FatFs transfer. */
static volatile u32 purge_dummy __attribute__((unused));
static volatile u8 ccr_dummy __attribute__((unused));
#define REG32(address) (*((void)(address), &purge_dummy))
#define REG8(address) (*((void)(address), &ccr_dummy))
#define cache_sr_read() 0u
#define cache_sr_write(value) ((void)(value))
#define cache_control_read(address) ((void)(address), 0u)
#include "menu-file.inc"

static FILE *disk;
static FATFS fs;
DSTATUS disk_initialize(BYTE drive) { return drive ? STA_NOINIT : 0; }
DSTATUS disk_status(BYTE drive) { return drive ? STA_NOINIT : 0; }
DRESULT disk_read(BYTE drive,BYTE *data,LBA_t sector,UINT count)
{
    assert(!drive && count && sector+count<=131072);
    assert(!fseek(disk,(long)sector*512,SEEK_SET));
    return fread(data,512,count,disk)==count ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drive,const BYTE *data,LBA_t sector,UINT count)
{
    assert(!drive && count && sector+count<=131072);
    assert(!fseek(disk,(long)sector*512,SEEK_SET));
    return fwrite(data,512,count,disk)==count ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive,BYTE cmd,void *buffer)
{ (void)buffer;assert(!drive && cmd==CTRL_SYNC);return fflush(disk) ? RES_ERROR : RES_OK; }

int main(int argc,char **argv)
{
    assert(argc==4);
    int test=atoi(argv[2]);assert(test>=0 && test<40);
    file_ack=atoi(argv[3]);assert(!file_ack || file_ack==SSCMD_SAVE_ACK);
    /* The old table calls keep four arguments; new capacity is opt-in. */
    int (*legacy_read)(char*,int,int,void*)=read_file;
    int (*legacy_write)(char*,int,int,void*)=write_file;
    uintptr_t abi_slots[4]={0};
#include "abi.inc"
    assert(abi_slots[0]==(uintptr_t)legacy_read && abi_slots[1]==(uintptr_t)legacy_write);
    if(HAS_BOUNDED_READER)assert(abi_slots[2]==(uintptr_t)read_file_bounded && abi_slots[3]==0x53464231);
    else assert(!abi_slots[2] && !abi_slots[3]);
    disk=fopen(argv[1],"r+b");assert(disk);
    assert(f_mount(&fs,"0:",1)==FR_OK && fs.fs_type==FS_FAT32);
    char name[256]="/Menu transfer.bin";
    unsigned file_size=128,capacity=256;
    int size=0,offset=0,writing=0,legacy=0,expected=128;
    unsigned expected_commands=1;
    if(test==1){capacity=128;expected_commands=2;}
    if(test==2){file_size=129;capacity=128;expected=-19;expected_commands=2;}
    if(test==3){offset=100;capacity=28;expected=28;expected_commands=2;}
    if(test==4){offset=160;expected=0;}
    if(test==5){file_size=0;expected=0;}
    if(test==6){size=17;capacity=17;expected=17;}
    if(test==7){size=17;capacity=16;expected=-19;expected_commands=0;}
    if(test==8){offset=-1;expected=-19;expected_commands=0;}
    if(test==9){size=-1;expected=-19;expected_commands=0;}
    if(test==10){capacity=0;expected=-19;expected_commands=0;}
    if(test==11){expected=-19;expected_commands=0;}
    if(test==12){size=16;capacity=16;peer_fault=1;expected=-19;}
    if(test==28 || test==29){size=16;capacity=16;peer_fault=test==28?3:4;expected=-19;}
    if(test==13){capacity=128;peer_fault=2;expected=-FR_DISK_ERR;expected_commands=2;}
    if(test==14 || test==15){unsigned n=test==14?240:239;memset(name,'a',n);name[0]='/';name[n]=0;
        if(test==14){expected=-19;expected_commands=0;}}
    if(test>=16 && test<=18){writing=1;size=17;expected=17;
        if(test==17)offset=-1;
        if(test==18){offset=-1;size=0;expected=0;}}
    if(test==19 || test==20){size=0x338f01;capacity=(u32)size;writing=test==20;expected=-19;expected_commands=0;}
    if(test==21){size=32;capacity=32;expected=-19;expected_commands=0;}
    if(test==22){size=16;capacity=16;expected=16;}
    if(test==23){size=16;capacity=16;expected=-19;expected_commands=0;}
    if(test==24){legacy=1;capacity=16;target_bus=0x060ffff0;expected=-19;expected_commands=2;}
    if(test==25){legacy=1;}
    if(test==26){file_size=0xfc000;capacity=file_size;expected=(int)file_size;expected_commands=2;}
    if(test==27){file_size=0x200000;capacity=file_size;size=(int)file_size;expected=(int)file_size;}
    if (test>=30) {
        /* Pairs cover bounced read, write, empty create, and direct read. */
        size=16; capacity=16; expected=-FR_DISK_ERR;
        writing=test==31 || test==33 || test==34 || test==35;
        peer_fault=test==32 || test==33 || test==35 || test==37 ? RESET_AFTER_IO : RESET_BEFORE_IO;
        if (test==34 || test==35 || test==39) { writing=1; offset=-1; size=0; }
        if (test>=38) peer_fault=INVALID_POSITIVE_REPLY;
    }
    if(!HAS_BOUNDED_READER) {
        if(test==1 || test==3 || test==26) expected_commands=1;
        if(test==13) { expected=128; expected_commands=1; peer_fault=0; }
    }
    /* Generate larger valid fixtures in fixed chunks; no source/game images needed. */
    u8 block[4096];for(unsigned i=0;i<sizeof block;i++)block[i]=(i*7+3)%251;
    FIL file;UINT count;assert(f_open(&file,name,FA_CREATE_ALWAYS|FA_WRITE)==FR_OK);
    for(unsigned pos=0;pos<file_size;){unsigned n=file_size-pos;if(n>sizeof block)n=sizeof block;
        assert(f_write(&file,block,n,&count)==FR_OK && count==n);pos+=n;}
    assert(f_close(&file)==FR_OK);
    test_shared=malloc(0x1000000);assert(test_shared);
    allocation=capacity?capacity:1;
    /* Grossly invalid extents must fail before touching this much smaller allocation. */
    if(test==19 || test==20)allocation=16;
    target=malloc(allocation);assert(target);memset(target,0xa5,allocation);
    void *buffer=target;
    if(test==21 || test==22)buffer=test_shared+0xfffff0;
    if(test==23)buffer=test_shared+0xc7000;
    if(test==27)buffer=test_shared+0x400000;
    if(test==36 || test==37)buffer=test_shared+0x800000;
    if(test==21 || test==22 || test==36 || test==37)memset(buffer,0xa5,16);
    if(test==27)memset(buffer,0xa5,file_size);
    memset((void*)TMPBUFF_ADDR,0x55,256);
    u8 request[256];memcpy(request,(void*)TMPBUFF_ADDR,256);
    int result=writing ? legacy_write(name,offset,size,buffer) : legacy ? legacy_read(name,offset,size,buffer) :
        read_file_bounded(test==11?NULL:name,offset,size,buffer,capacity);
    if (result!=expected)
        fprintf(stderr,"case %d returned %d, expected %d\n",test,result,expected);
    assert(result==expected && commands==expected_commands && !mcu_cmd);
    /* The legacy unused request word is not a new capacity/protocol field. */
    assert(*(u32*)(TMPBUFF_ADDR+0x0c)==0x55555555u);
    if(!commands)assert(!memcmp(request,(void*)TMPBUFF_ADDR,256));
    if(!writing && (result>=0 || test==2 || test==13 || test==24)) {
        unsigned moved=result<0?capacity:(unsigned)result;
        for(unsigned i=0;i<moved;i++)assert(((u8*)buffer)[i]==block[(i+(unsigned)offset)%sizeof block]);
        if(buffer==target)for(unsigned i=moved;i<allocation;i++)assert(target[i]==0xa5);
    }
    if(test==12 || test==28 || test==29)for(unsigned i=0;i<allocation;i++)assert(target[i]==0xa5);
    if(test>=30 && !writing)for(unsigned i=0;i<16;i++)
        assert(((u8*)buffer)[i]==(test==37 ? block[i] : 0xa5));
    assert(f_open(&file,name,FA_READ)==FR_OK);
    unsigned actual_size=f_size(&file);
    int write_applied=writing && (result>=0 || peer_fault==RESET_AFTER_IO);
    assert(actual_size==(write_applied && offset==-1 ? (unsigned)size : file_size));
    unsigned inspect=actual_size>sizeof block?sizeof block:actual_size;
    u8 verify[4096];assert(f_read(&file,verify,inspect,&count)==FR_OK && count==inspect);
    assert(f_close(&file)==FR_OK);
    for(unsigned i=0;i<inspect;i++)assert(verify[i]==(write_applied && i<(unsigned)size ? 0xa5 : block[i]));
    assert(!fclose(disk));free(target);free(test_shared);
    puts("Saturn/MCU bounded transfer, content and ABI-wrapper checks pass.");
    return 0;
}
