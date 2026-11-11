/* Adapted from paps-g 855d0092/d1079400. Actual MCU and menu file transfer. */
/* Actual menu requests and MCU/FatFs transfer; independently model the pinned
 * FPGA's 1 MiB address decoder for Saturn CPU reads and writes. */
#include <stdint.h>
#include "ff.h"
static uint16_t ram_control;
#define SS_CTRL ram_control
static FRESULT ram_read(FIL *, void *, UINT, UINT *);
static FRESULT ram_write(FIL *, const void *, UINT, UINT *);
#define f_read ram_read
#define f_write ram_write
#define main existing_file_fixture_main
#include "menu_file_transfer.c"
#undef main
#undef f_read
#undef f_write

static unsigned fail_transfer, transfers, transferred;
static FRESULT ram_read(FIL *file, void *buffer, UINT size, UINT *count)
{
    int fail = ++transfers == fail_transfer;
    FRESULT result = f_read(file, buffer, fail && size > 3 ? 3 : size, count);
    transferred += *count;
    return fail ? FR_DISK_ERR : result;
}
static FRESULT ram_write(FIL *file, const void *buffer, UINT size, UINT *count)
{
    int fail = ++transfers == fail_transfer;
    FRESULT result = f_write(file, buffer, fail && size > 3 ? 3 : size, count);
    transferred += *count;
    return fail ? FR_DISK_ERR : result;
}

static unsigned saturn_offset(unsigned address)
{
    unsigned low = address & 0xffffffu;
    /* SSMaster.v: SS_ADDR[23:22]==01 and type==10 forces [20:19]=00. */
    if ((ram_control & 0x3000u) == 0x2000u && (low >> 22) == 1)
        low &= ~0x180000u;
    return low;
}

static u8 file_pattern(unsigned offset)
{ return (u8)(offset * 29u + (offset >> 9) * 17u + (offset >> 17) * 71u); }
static u8 memory_pattern(unsigned offset)
{ return (u8)(offset * 7u + (offset >> 9) * 23u + (offset >> 19) * 101u); }

int main(int argc, char **argv)
{
    assert(argc == 9);
    unsigned address = strtoul(argv[2], NULL, 0), size = strtoul(argv[3], NULL, 0);
    ram_control = 0x8000u | (strtoul(argv[4], NULL, 0) << 12);
    int writing = atoi(argv[5]), short_tail = atoi(argv[6]);
    fail_transfer = strtoul(argv[7], NULL, 0);
    int native = atoi(argv[8]);
    assert(address >= 0x020c7100u && address < 0x03000000u);
    assert(size && size <= 0x03000000u - address && size < 0x800000);
    file_ack = SSCMD_SAVE_ACK;
    disk = fopen(argv[1], "r+b"); assert(disk);
    assert(!f_mount(&fs, "0:", 1) && fs.fs_type == FS_FAT32);
    test_shared = malloc(0x1000000); assert(test_shared);
    for (unsigned i = 0; i < 0x1000000; i++) test_shared[i] = memory_pattern(i);
    u8 *expected_memory = malloc(0x1000000); assert(expected_memory);
    memcpy(expected_memory, test_shared, 0x1000000);
    const char *name = "/RAM mapping.bin";
    unsigned available = size - (short_tail ? 7 : 0);
    unsigned offset = 13, extent = offset + available;
    FIL file; UINT count; u8 block[4096];
    assert(!f_open(&file, name, FA_CREATE_ALWAYS | FA_WRITE));
    for (unsigned pos = 0; pos < extent;) {
        unsigned n = extent - pos; if (n > sizeof block) n = sizeof block;
        for (unsigned i = 0; i < n; i++) block[i] = file_pattern(pos + i);
        assert(!f_write(&file, block, n, &count) && count == n); pos += n;
    }
    assert(!f_close(&file));
    /* The direct pointer retains the Saturn address. Host code never copies
     * this logical view; actual MCU requests access their own physical view. */
    void *buffer = test_shared + (address & 0xffffffu);
    int result;
    if (native) {
#ifdef HAS_MAPPED_FILE_BUFFER
        assert(!f_open(&file, name, writing ? FA_WRITE : FA_READ));
        assert(!f_lseek(&file, offset));
        void *physical = (void *)(uintptr_t)(0x61000000u | (address & 0xffffffu));
        assert(!file_buffer_io(&file, physical, size, writing, &count));
        assert(!f_close(&file));
        result = (int)count;
#else
        abort();
#endif
    } else {
        result = writing ? write_file((char *)name, offset, size, buffer) :
                           read_file_bounded((char *)name, offset, size, buffer, size);
    }
    if (fail_transfer) assert(result == -FR_DISK_ERR && transfers == fail_transfer);
    else assert(result == (int)(writing ? size : available));
    assert(!mcu_cmd && commands == (unsigned)!native);
    if (!writing) {
        assert(transferred <= available);
        for (unsigned i = 0; i < transferred; i++)
            expected_memory[native ? (address + i) & 0xffffffu : saturn_offset(address + i)] = file_pattern(offset + i);
    }
    /* Reads modify the mapped bytes. The command envelope is the only
       other writable memory, and a write uses the mapped bytes as input. */
    if (!native) memcpy(expected_memory + 0xc7000, test_shared + 0xc7000, 256);
    assert(!memcmp(test_shared, expected_memory, 0x1000000));
    assert(!f_open(&file, name, FA_READ));
    if (!fail_transfer) assert(f_size(&file) == (writing ? offset + size : extent));
    for (unsigned pos = 0; pos < f_size(&file);) {
        unsigned n = f_size(&file) - pos; if (n > sizeof block) n = sizeof block;
        assert(!f_read(&file, block, n, &count) && count == n);
        for (unsigned i = 0; i < n; i++) {
            unsigned at = pos + i;
            unsigned source = address + at - offset;
            unsigned source_offset = native ? source & 0xffffffu : saturn_offset(source);
            u8 expected = writing && at >= offset && at - offset < transferred ?
                memory_pattern(source_offset) : file_pattern(at);
            assert(block[i] == expected);
        }
        pos += n;
    }
    assert(!f_close(&file) && !fclose(disk));
    printf("RAM mapping: type=%u address=%x size=%u write=%d short=%d commands=%u transfers=%u pass\n",
           (ram_control >> 12) & 3, address, size, writing, short_tail, commands, transfers);
    free(expected_memory); free(test_shared);
    return 0;
}
