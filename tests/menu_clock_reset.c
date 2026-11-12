/* Adapted from paps-g cc2cafa2 and ff3f9d2f. BIOS and MMIO boundary model. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
enum { BUDGET = 1 << 24 };
static unsigned sf_reads, latch_reads, counter_reads, pre_busy, post_busy, unsafe;
static unsigned calls, issued, completed, line, external, debug_flag;
static unsigned last_busy, last_line;
static u8 sf, command;
static u32 asr, aref, save_vector, player_vector, published;
static unsigned copies;
static int rom_path;
static void jump_06b0(void);
static void jump_copy_code(int type) { (void)type; assert(0); }
static __attribute__((unused)) void rom_reset_issue(void) { assert(0); }

static u8 *sf_port(void)
{
    assert(++sf_reads <= BUDGET + 2);
    if (issued) {
        last_busy = post_busy != 0;
        if (post_busy) post_busy--;
        else completed = 1;
    } else {
        last_busy = pre_busy != 0;
        if (pre_busy) pre_busy--;
    }
    sf = 0xa0 | last_busy;
    return &sf;
}

static u8 *command_port(void)
{
    assert(sf == 1 && !last_busy && !issued);
    assert(calls == (rom_path ? 2u : 1u));
    assert(asr == (rom_path ? 0x34403440u : 0x38803880u));
    if (rom_path) assert(aref == 0x13);
    assert(!external && (!latch_reads || last_line < 192));
    issued++;
    return &command;
}

static __attribute__((unused)) unsigned ext_latch(void)
{
    assert(!issued && !last_busy);
    latch_reads++;
    return external ? 0x0200 : 0;
}

static __attribute__((unused)) unsigned counter(void)
{
    assert(!external && latch_reads > counter_reads);
    counter_reads++;
    last_line = latch_reads <= unsafe ? 240 : line;
    return last_line | 0xfc00;
}

static void bios_clock(void)
{
    assert(rom_path && !calls++);
    asr = aref = 0;
}

static void bios_scu(void)
{
    assert(calls++ == (rom_path ? 1u : 0u));
    if (rom_path) assert(aref == 0x13);
    asr = 0x1ff01ff0;
    aref = 0x1f;
}

static void sci_init(void) { assert(0); }
static void printk(const char *fmt, ...) { (void)fmt; }

static u32 *clock_vector(void)
{
    assert(copies == 3);
    return &published;
}

static void install_fragment(void *destination, const void *source, size_t size)
{
    assert(!published);
    if (copies == 0) assert(destination == (void *)0x200000 && source == jump_copy_code && size == 96);
    else if (copies == 1) assert(destination == (void *)0x6000c80 && source == jump_06b0 && size == 96);
    else assert(copies == 2 && destination == (void *)0x60006b8 && source == rom_reset_issue && size == 36);
    copies++;
}

#define SF (*sf_port())
#define COMREG (*command_port())
#define EXTEN (ext_latch())
#define VCNT (counter())
#undef memcpy
#define memcpy install_fragment
#include "actual.inc"
#undef memcpy

static void reset(int path)
{
    rom_path = path;
    sf_reads = latch_reads = counter_reads = pre_busy = post_busy = unsafe = 0;
    calls = issued = completed = external = last_busy = last_line = 0;
    sf = command = 0;
    asr = aref = 0;
    line = 32;
}

static void run(void)
{
    if (rom_path) jump_06b0();
    else my_06b0();
    assert(calls == (rom_path ? 2u : 1u));
    assert(asr == (rom_path ? 0x34403440u : 0x38803880u));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *test = argv[1];
    if (!strcmp(test, "install")) {
        card_init();
        assert(copies == 3 && published == 0x6000c80);
        assert(save_vector == 0x7d600 && player_vector == 0x186c);
        return 0;
    }
    for (int path = 0; path < 2; path++) {
        reset(path);
        if (!strcmp(test, "healthy")) {
            post_busy = 3;
            run();
            assert(issued && completed && command == 0x19);
        } else if (!strcmp(test, "admission")) {
            pre_busy = 5;
            post_busy = 4;
            unsafe = 2;
            run();
            assert(issued && completed && command == 0x19 && latch_reads == 3);
        } else if (!strcmp(test, "refusal")) {
            external = 1;
            run();
            assert(!issued && !counter_reads && latch_reads == BUDGET);
            reset(path);
            pre_busy = BUDGET + 1;
            run();
            assert(!issued && !latch_reads && sf_reads == BUDGET);
        } else {
            assert(!strcmp(test, "completion"));
            pre_busy = 5;
            post_busy = BUDGET + 1;
            run();
            assert(issued && !completed && command == 0x19);
            assert(sf_reads == BUDGET + 1 && post_busy == 7);
        }
    }
}
