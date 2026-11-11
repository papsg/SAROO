/* Adapted from paps-g 3f6e0121. Paired host transport on FatFs images. */
/* Preserve actual primary owners and faulting FatFs; translate bus addresses
 * and CPU cache-register access only. This is not cartridge timing evidence. */
#include <stdint.h>
static uint16_t ram_control = 0xb000u;
#define SS_CTRL ram_control
static void *owner_file_pointer(uintptr_t address);
#define FILE_TEST_POINTER(address) owner_file_pointer(address)
#define SAVE_OWNER_TEST_ENTRY existing_owner_main
#include "mcu_save_owner.c"
#undef SAVE_OWNER_TEST_ENTRY

static u8 *protocol_shared;
static u32 bus_alias;
static void *bus_pointer(u32 offset)
{
    if (offset >= 0xa4000 && offset < 0xc7000)
        return test_shared + (offset - 0xa4000);
    return protocol_shared + offset;
}
static __attribute__((unused)) void *owner_file_pointer(uintptr_t address)
{
    if (address >= 0x61000000u && address < 0x62000000u)
        return bus_pointer(address - 0x61000000u);
    return (void *)address;
}
static u32 host_bus_address(void *p)
{
    uintptr_t address = (uintptr_t)p;
    if (address >= (uintptr_t)protocol_shared && address < (uintptr_t)protocol_shared + 0x1000000)
        return (0x02000000 + (u32)(address - (uintptr_t)protocol_shared)) | bus_alias;
    if (address >= (uintptr_t)test_shared && address < (uintptr_t)test_shared + sizeof test_shared)
        return (0x020a4000 + (u32)(address - (uintptr_t)test_shared)) | bus_alias;
    return p ? 0x06001000 : 0;
}
#undef TMPBUFF_ADDR
#define TMPBUFF_ADDR ((uintptr_t)(protocol_shared + 0xc7000))
#define FIRM_ADDR ((uintptr_t)protocol_shared)
#define SSLOG(...) ((void)0)
#include "protocol.h"
static volatile u16 mcu_command, mcu_argument;
#define SS_CMD mcu_command
#define SS_ARG mcu_argument
#include "mcu-owner.inc"
#undef SS_CMD
#undef SS_ARG
static unsigned requests, purges, peer_fault;
static volatile u16 *command_register(void)
{
    if (mcu_command) {
        requests++;
        if (peer_fault != 1) command();
        if (peer_fault == 1 || peer_fault == 2) mcu_argument = mcu_command = 0;
        if (peer_fault == 3) { mcu_argument = SSCMD_SAVE_ACK | 1; mcu_command = 0; }
        if (peer_fault == 4) *(u32 *)(TMPBUFF_ADDR + 4) -= 1;
    }
    return &mcu_command;
}
#define SS_CMD (*command_register())
#define SS_ARG mcu_argument
static u16 file_ack, archive_ack;
static volatile u32 purge_dummy;
static volatile u8 control_dummy;
static volatile u32 *purge_register(u32 address)
{
    assert(address & 0x40000000); purges++; return &purge_dummy;
}
#define REG32(address) (*purge_register(address))
#define REG8(address) (*((void)(address), &control_dummy))
#define cache_sr_read() 0u
#define cache_sr_write(value) ((void)(value))
#define cache_control_read(address) ((void)(address), ++purges, 0u)
#include "menu-owner.inc"
#define SYSINFO_ADDR TEST_SAVE_SYSINFO_ADDR
static void latch(void)
{
#include "archive-latch.inc"
}

static void reset_peer(void) { requests = purges = peer_fault = 0; clear_io(); }

static void check_transport(unsigned device, u8 *buffer)
{
    ownership(); reset_peer();
    u8 *expected = device ? mems_before : slot_before;
    unsigned opens = api_opens;
    for (unsigned alias = 0; alias < 2; alias++) {
        bus_alias = alias ? 0x20000000 : 0;
        memset(buffer, 0xa5, 65538);
        assert(archive_command(device, 65536, buffer + 1, 65536) == 65536);
        assert(buffer[0] == 0xa5 && buffer[65537] == 0xa5);
        assert(!memcmp(buffer + 1, expected + 65536, 65536));
        assert(purges == alias + 1);
    }
    memset(buffer, 0x69, 65536);
    assert(archive_command(device, 65536, buffer, -65536) == 65536);
    assert(archive_command(device, 0, NULL, 0) == 0);
    memcpy(expected + 65536, buffer, 65536);
    assert(requests == 4 && purges == 2 && api_opens == opens);
    if (!device) { assert(!ss_index); identity(NULL); }
    else {
        assert(flush_smems(7) != 0);
        assert(archive_command(1, 0, (void *)SMEMS_HDR, 8192) == 8192);
        assert(!sm_index0 && !sm_index1 && !flush_smems(1));
    }
}

static void check_errors(unsigned device, u8 *buffer)
{
    u8 *expected = device ? mems_before : slot_before;
    const char *failures[] = {"read-error", "read-short", "read-partial-error", "seek-any-error", "seek-any-short"};
    for (unsigned i = 0; i < sizeof failures / sizeof *failures; i++) {
        reset_peer(); arm(failures[i], device); memset(buffer, 0xa5, 64);
        assert(archive_command(device, 65536, buffer + 1, 32) < 0);
        assert(hits && requests == 1 && purges == 3);
        assert(buffer[0] == 0xa5 && buffer[33] == 0xa5); armed = 0;
    }
    for (unsigned failure = 1; failure <= 4; failure++) {
        reset_peer(); peer_fault = failure;
        assert(archive_command(device, 65536, buffer + 1, 32) < 0);
        assert(requests == 1 && purges == 3);
        reset_peer(); peer_fault = failure; memset(buffer, 0xa7, 32);
        assert(archive_command(device, 65536, buffer, -32) < 0);
        assert(requests == 1 && !purges);
        if (failure != 1) memcpy(expected + 65536, buffer, 32);
        peer_fault = 0; assert(!archive_command(device, 0, NULL, 0));
    }
    reset_peer(); arm("write-short", device); memset(buffer, 0x39, 32);
    assert(archive_command(device, 65536, buffer, -32) < 0);
    assert(hits && !purges); armed = 0;
    /* Repair using the same owner; a failed write can have changed a prefix. */
    assert(archive_command(device, 65536, buffer, -32) == 32);
    memcpy(expected + 65536, buffer, 32);
    arm("sync-error", device); assert(archive_command(device, 0, NULL, 0) < 0);
    armed = 0; assert(!archive_command(device, 0, NULL, 0));
}

static void check_bounds(unsigned device, u8 *buffer)
{
    reset_peer();
    const u32 versions[] = {0, 0x534f5730, 0xffffffff, ARCHIVE_IO_VERSION};
    for (unsigned i = 0; i < sizeof versions / sizeof *versions; i++) {
        test_save_sysinfo[6] = versions[i]; latch();
        if (versions[i] != ARCHIVE_IO_VERSION) {
            assert(archive_command(device, 0, NULL, 0) == -FR_NOT_ENABLED);
            assert(archive_find_slot((const u8 *)key_a,buffer,60928) == -FR_NOT_ENABLED);
            assert(!requests);
        }
    }
    /* Clearing live capability cannot silently downgrade the latched reply. */
    test_save_sysinfo[6] = 0;
    assert(archive_ack == SSCMD_SAVE_ACK);
    struct { unsigned dev; u32 offset; void *data; int length; } invalid[] = {
        {2,0,NULL,0}, {UINT_MAX,0,NULL,0}, {device,1,NULL,0},
        {device,0,buffer,0}, {device,0,NULL,1}, {device,0,NULL,-1},
        {device,65536,buffer,65537}, {device,65536,buffer,-65537},
        {device,65536,buffer,INT_MIN}, {device,65536,buffer,INT_MAX},
        {0,0,buffer,-1}, {0,65535,buffer,-2},
        {device,65536,slot_before,1}, {device,65536,slot_before,-1},
        {device,65536,protocol_shared+0xc7000,1},
        {device,65536,protocol_shared+0xc6fff,2},
        {device,65536,protocol_shared+0xffffff,2}
    };
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; i++) {
        assert(archive_command(invalid[i].dev,invalid[i].offset,invalid[i].data,invalid[i].length) == -FR_INVALID_PARAMETER);
        assert(!requests && !purges);
    }
    assert(archive_command(device,UINT_MAX,buffer,1) < 0);
    assert(requests == 1 && purges && !api_reads && !api_writes);
}

static void write_directory(unsigned entry, const void *key)
{
    require_lock = 0; raw_write(&ss_fp,entry*16,key,16); require_lock = 1;
    memcpy(slot_before+entry*16,key,16);
}
static void check_lookup(u8 *buffer, int failed)
{
    reset_peer();
    if (failed) {
        for (unsigned nth = 1; nth <= 2; nth++) {
            memset(buffer,0x4b,65536); memcpy(buffer,key_a,16);
            arm("read-partial-error",0); fault_nth = nth;
            assert(archive_find_slot(buffer,buffer,60928) < 0);
            assert(hits); armed = 0;
            for (unsigned i=60928;i<65536;i++) assert(buffer[i]==0x4b);
        }
        return;
    }
    memset(buffer,0x4b,65536); memcpy(buffer,key_a,16);
    assert(archive_find_slot(buffer,buffer,60928)==65536 && requests==2);
    for(unsigned i=60928;i<65536;i++) assert(buffer[i]==0x4b);
    assert(archive_find_slot((const u8*)key_b,buffer,65536)==3*65536);
    assert(archive_find_slot((const u8*)key_d,buffer,512)==-FR_NO_FILE);
    /* Last entry lookup is bounded to the directory; application separately
     * checks the destination's existing extent through the owner. */
    write_directory(4095,key_d);
    assert(archive_find_slot((const u8*)key_d,buffer,60928)==4095*65536);
    write_directory(2,key_a);
    assert(archive_find_slot((const u8*)key_a,buffer,60928)==-FR_INT_ERR);
    char short_key[16]="SHORT", padded[16]; memset(padded,0x6b,16); memcpy(padded,short_key,6);
    write_directory(2,padded);
    assert(archive_find_slot((u8*)short_key,buffer,60928)==2*65536);
    write_directory(4095,short_key);
    assert(archive_find_slot((u8*)short_key,buffer,60928)==-FR_INT_ERR);
    char bad_signature[16]="Saroo Save File";
    bad_signature[15]=1;
    write_directory(0,bad_signature);
    assert(archive_find_slot((const u8*)key_a,buffer,60928)==-FR_INT_ERR);
    write_directory(0,key_a);
    assert(archive_find_slot((const u8*)key_a,buffer,60928)==-FR_INT_ERR);
    reset_peer();
    const u32 sizes[]={0,16,511,513,65537,UINT_MAX};
    for(unsigned i=0;i<sizeof sizes/sizeof *sizes;i++)
        assert(archive_find_slot((const u8*)key_a,buffer,sizes[i])==-FR_INVALID_PARAMETER);
    assert(archive_find_slot(NULL,buffer,512)==-FR_INVALID_PARAMETER);
    assert(archive_find_slot((const u8*)"",buffer,512)==-FR_INVALID_PARAMETER);
    assert(!requests);
}

static unsigned cpu_offset(unsigned address)
{
    unsigned offset = address & 0xffffffu;
    if ((offset >> 22) == 1) offset &= ~0x180000u;
    return offset;
}
static void check_owner_ram(unsigned device)
{
    static const unsigned addresses[] = {
        0x02400003,0x02480003,0x02500003,0x02580003,
        0x02600003,0x02680003,0x02700003,0x02780003,
        0x023ffff0,0x0247fff0,0x025ffff0,0x027ffff0
    };
    u8 *expected = device ? mems_before : slot_before;
    u8 *memory = malloc(0x1000000); assert(memory);
    const unsigned offset = 65536 + 13, size = 39;
    ownership(); ram_control = 0xa000;
    for (unsigned a = 0; a < sizeof addresses / sizeof *addresses; a++) {
        unsigned address = addresses[a];
        void *buffer = protocol_shared + (address & 0xffffffu);
        for (unsigned writing = 0; writing < 2; writing++) {
            for (unsigned i = 0; i < 0x1000000; i++)
                protocol_shared[i] = (u8)(i * 11u + (i >> 19) * 79u);
            memcpy(memory, protocol_shared, 0x1000000); reset_peer(); clear_io();
            unsigned opens = api_opens;
            assert(archive_command(device, offset, buffer, writing ? -(int)size : (int)size) == (int)size);
            assert(requests == 1 && api_opens == opens && api_seeks == 1);
            for (unsigned i = 0; i < size; i++) {
                unsigned at = cpu_offset(address + i);
                if (writing) expected[offset + i] = memory[at];
                else memory[at] = expected[offset + i];
            }
            memcpy(memory + 0xc7000, protocol_shared + 0xc7000, 256);
            assert(!memcmp(memory, protocol_shared, 0x1000000));
            assert(!archive_command(device, 0, NULL, 0));
            assert_archives(slot_before, slot_size, mems_before);
        }
    }
    /* Existing owner rules require an exact root reload after MEMS writes. */
    if (device) assert(archive_command(1, 0, (void *)SMEMS_HDR, 8192) == 8192);
    /* A bad whole extent must be refused before a first split write occurs. */
    ownership(); reset_peer(); clear_io();
    unsigned end = device ? 0x800000 : slot_size;
    assert(archive_command(device, end - 8, protocol_shared + 0x47fff0, -39) == -FR_INVALID_PARAMETER);
    assert(requests == 1 && !api_reads && !api_writes && !api_seeks);
    /* The second physical span can fail after modifying half of its 23 bytes. */
    memset(protocol_shared, 0xa5, 0x1000000); memcpy(memory, protocol_shared, 0x1000000);
    reset_peer(); arm("read-partial-error", device); fault_nth = 2;
    assert(archive_command(device, offset, protocol_shared + 0x47fff0, 39) == -FR_DISK_ERR);
    assert(hits == 1 && requests == 1 && api_reads == 2 && api_seeks == 1);
    for (unsigned i = 0; i < 27; i++) memory[cpu_offset(0x0247fff0 + i)] = expected[offset + i];
    memcpy(memory + 0xc7000, protocol_shared + 0xc7000, 256);
    assert(!memcmp(memory, protocol_shared, 0x1000000)); armed = 0;
    assert_archives(slot_before, slot_size, mems_before);
    free(memory);
}

int main(int argc,char **argv)
{
    assert(argc==5); unsigned device=atoi(argv[4]); assert(device<2);
    disk=fopen(argv[1],"r+b"); assert(disk); fault="none";
    BYTE work[32768];
    MKFS_PARM options={.fmt=(!strcmp(argv[2],"fat32")?FM_FAT32:FM_EXFAT)|FM_SFD,
        .n_fat=1,.au_size=!strcmp(argv[2],"fat32")?512:4096};
    assert(!f_mkfs("0:",&options,work,sizeof work));
    FATFS fs; assert(!f_mount(&fs,"0:",1)); fixture_trailer=1; initialize(); require_lock=1;
    protocol_shared=malloc(0x1000000); assert(protocol_shared);
    test_save_sysinfo[6]=ARCHIVE_IO_VERSION; latch();
    u8 *buffer=protocol_shared+0xc7100;
    if(!strcmp(argv[3],"transport")) check_transport(device,buffer);
    else if(!strcmp(argv[3],"errors")) check_errors(device,buffer);
    else if(!strcmp(argv[3],"bounds")) check_bounds(device,buffer);
    else if(!strcmp(argv[3],"ram")) check_owner_ram(device);
    else check_lookup(buffer,!strcmp(argv[3],"lookup-fault"));
    assert_archives(slot_before,slot_size,mems_before);
    require_lock=0;
    assert(!f_close(&ss_fp)&&!f_close(&sm_fp)&&!f_mount(NULL,"0:",0));
    assert(!fclose(disk)); free(protocol_shared);free(slot_before);free(mems_before);
    puts("PASS paired menu/MCU owner transport and complete archives");
}
