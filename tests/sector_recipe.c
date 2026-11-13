/* Execute the original C callback's memory operations with SH-2 byte order. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
enum { MASK_EXMEM = 0x3000, CS0_RAM1M = 0x2000, CS0_RAM4M = 0x3000 };
static u8 memory[0x100000], expected[sizeof(memory)];
static unsigned cart_action;

static u8 *image_at(uintptr_t address, unsigned bytes)
{
    address &= 0x1fffffff;
    assert(address >= 0x06000000 && bytes <= sizeof(memory));
    unsigned offset = address - 0x06000000;
    assert(offset <= sizeof(memory) - bytes);
    return memory + offset;
}

static u32 read_be(uintptr_t address, unsigned bytes)
{
    const u8 *p = image_at(address, bytes);
    u32 value = 0;
    while(bytes--) value = value << 8 | *p++;
    return value;
}

static void write_be(uintptr_t address, u32 value, unsigned bytes)
{
    u8 *p = image_at(address, bytes);
    while(bytes) {
        p[--bytes] = value;
        value >>= 8;
    }
}

static __attribute__((unused)) void ssctrl_set(unsigned mask, unsigned value)
{
    assert(mask == MASK_EXMEM && (value == CS0_RAM1M || value == CS0_RAM4M));
    cart_action = value == CS0_RAM1M ? 1 : 4;
}

#define crc32 menu_crc32
#include "crc32.inc"
#undef crc32
static u32 crc32(u8 *address, int bytes, u32 seed)
{
    assert(bytes > 0 && seed == 0);
    return menu_crc32(image_at((uintptr_t)address, bytes), bytes, seed);
}

#include "callbacks.inc"

static void load(const char *path, u8 *data)
{
    FILE *fp = fopen(path, "rb");
    assert(fp);
    assert(fread(data, 1, sizeof(memory), fp) == sizeof(memory));
    assert(fgetc(fp) == EOF && !ferror(fp) && !fclose(fp));
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    unsigned index = (unsigned)atoi(argv[1]);
    assert(index < sizeof(callbacks) / sizeof(callbacks[0]));
    load(argv[2], memory);
    load(argv[3], expected);
    callbacks[index]();
    assert(cart_action == (unsigned)atoi(argv[4]));
    assert(!memcmp(memory, expected, sizeof(memory)));
    return 0;
}
