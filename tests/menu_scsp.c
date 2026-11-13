/* Adapted from paps-g 268ce1cc and eb4826d3. Modeled menu startup MMIO. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

enum { BASE = 0x25b00000, LEFT = 0x216, RIGHT = 0x236, MASTER = 0x400 };
static uint8_t regs[MASTER + 4], initial[sizeof regs];
static unsigned bytes, words, reset_count;

static void reset_timer(void)
{
    assert(++reset_count == 1 && !bytes && !words);
}

static void write_byte(unsigned address, unsigned value) __attribute__((unused));
static void write_byte(unsigned address, unsigned value)
{
    assert(reset_count == 1 && address >= BASE && address - BASE < sizeof regs);
    regs[address - BASE] = value;
    ++bytes;
}

static void write_word(unsigned address, unsigned value)
{
    assert(!(address & 1) && address + 1 >= BASE && address + 1 - BASE < sizeof regs);
    assert(reset_count == 1);
    regs[address - BASE] = value >> 8;
    regs[address + 1 - BASE] = value;
    ++words;
}

#include "startup.inc"

static unsigned word(unsigned offset)
{
    return (unsigned)regs[offset] << 8 | regs[offset + 1];
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    for (unsigned i = 0; i < sizeof regs; ++i)
        regs[i] = initial[i] = 0x91 + i % 47;
    assert(_main() == 0);

    if (!strcmp(argv[1], "width")) {
        assert(bytes == 0 && words == 3);
    } else if (!strcmp(argv[1], "stereo")) {
        unsigned left = word(LEFT);
        unsigned right = word(RIGHT);
        assert((left >> 5) == 6 && (right >> 5) == 6);
        assert((left & 31) == 0x1f && (right & 31) == 0x0f);
    } else {
        assert(!strcmp(argv[1], "neighbors") && word(MASTER) == 0x020f);
        for (unsigned i = 0; i < sizeof regs; ++i)
            if ((i & ~1u) != LEFT && (i & ~1u) != RIGHT && (i & ~1u) != MASTER)
                assert(regs[i] == initial[i]);
    }
    return 0;
}
