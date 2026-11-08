/* Actual menu parser, independent IREG0 read value and write history.
 * Only MMIO stores are adapted to callbacks by the Python constructor. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static volatile u8 SF, SR, IREG1, IREG2, COMREG;
static volatile u8 DDR1, DDR2, EXLE, IOSEL;
static volatile u16 TVSTAT;
static u8 test_oreg[64], read_value, writes[64];
static unsigned write_count, read_count, oreg_reads, interrupt_enabled, locked;
static u32 clock_now;
static void (*handler)(void);
static u32 get_timer(void) { return clock_now; }
static u32 pad_test_interrupt_lock(void)
{
    unsigned prior = locked; locked = 1; return prior;
}
static void pad_test_interrupt_unlock(u32 prior) { locked = prior; }
static void pad_test_interrupt_enable(int enable, void (*callback)(void))
{
    interrupt_enabled = enable; handler = enable ? callback : NULL;
}
static u8 pad_test_ireg0_read(void)
{
    ++read_count;
    return read_value;
}
static void pad_test_ireg0_write(u8 value)
{
    assert(write_count < sizeof(writes));
    writes[write_count++] = value;
}
#define IREG0 pad_test_ireg0_read()
#define SMPC_BASE 0x20100000u
static u8 *oreg_at(u32 addr)
{
    assert(addr >= SMPC_BASE + 0x21 && addr <= SMPC_BASE + 0x5f);
    unsigned offset = addr - SMPC_BASE - 0x21;
    assert(!(offset & 1)); ++oreg_reads;
    return &test_oreg[offset];
}
#define REG8(addr) (*oreg_at(addr))
#include "pad-writeonly.inc"

static void begin(void)
{
    TVSTAT = 8; SF = 0;
    unsigned before = write_count;
    assert(pad_read() == -1);
    clock_now += EXPECTED_PAD_ADMISSION;
    assert(pad_read() == -1 && COMREG == 0x10 && SF == 1);
    assert(write_count == before + 1 && writes[before] == 0);
}

static void reset(unsigned value)
{
    clock_now = UINT32_MAX - 2000;
    SF = SR = 0; COMREG = 0; read_value = value;
    write_count = read_count = oreg_reads = locked = 0;
    pad_init();
    assert(write_count == 1 && writes[0] == 0);
    assert(interrupt_enabled && handler);
    begin();
}

/* Maximum valid 15-byte-mode packet: two 15-socket ports, each device
 * has an extended length byte and 15 bytes of data. Sixteen complete pages. */
static void page(unsigned number, unsigned status)
{
    u8 packet[512]; unsigned at = 0;
    for (unsigned port = 0; port < 2; ++port) {
        packet[at++] = 0xef;
        for (unsigned slot = 0; slot < 15; ++slot) {
            packet[at++] = 0x00; packet[at++] = 15;
            for (unsigned n = 0; n < 15; ++n) packet[at++] = 0xff;
        }
    }
    assert(at == sizeof(packet) && number < 16);
    for (unsigned i = 0; i < 32; ++i) test_oreg[2*i] = packet[number*32+i];
    SR = status; SF = (status & 0x20) != 0;
    assert(handler && interrupt_enabled && !locked);
    handler();
}

static void continuations(unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        unsigned before = write_count;
        page(i, i ? 0xa0 : 0xe0);
        assert(write_count == before + 1);
        /* Oracle follows the independent issued-byte history, not parser state. */
        assert(writes[before] == (i % 2 ? 0 : 0x80));
        assert(pad_read() == -1);
    }
}

static void restart(void)
{
    TVSTAT = 0; assert(pad_read() == -1);
    begin();
    u8 empty[2] = {0xf0, 0xf0};
    for (unsigned i = 0; i < 32; ++i) test_oreg[2*i] = i < 2 ? empty[i] : 0;
    unsigned before = write_count;
    SR = 0xc0; SF = 0; handler();
    assert(pad_read() == 0 && write_count == before);
}

static void complete(unsigned value)
{
    reset(value); continuations(15);
    unsigned before = write_count;
    page(15, 0x80);
    assert(write_count == before); /* NPE=0 requires no redundant BREAK. */
    assert(pad_read() == 0 && oreg_reads == 512);
    assert(pad_read() == -1);
    restart();
    assert(read_count == 0);
}

static void cancel(unsigned value, unsigned pages, unsigned timeout)
{
    reset(value); continuations(pages);
    unsigned before = write_count;
    u8 prior_cont = writes[before-1] & 0x80;
    if (timeout) {
        clock_now += EXPECTED_PAD_TIMEOUT;
        assert(pad_read() == -1);
    } else {
        /* Invalid peripheral mode; zero OREG reads may occur. */
        page(pages, 0x81);
        assert(pad_read() == -1);
    }
    assert(write_count == before+1 && writes[before] == (prior_cont | 0x40));
    assert(oreg_reads == pages*32);
    unsigned count = write_count;
    handler(); /* Late interrupt cannot send another continuation/BREAK. */
    assert(write_count == count);
    restart();
    assert(read_count == 0);
}

static void handoff(unsigned value, unsigned pages)
{
    reset(value); continuations(pages);
    unsigned before = write_count;
    pad_suspend();
    assert(!handler && !interrupt_enabled);
    /* Another owner may leave any readback value. The menu must admit a new
     * INTBACK before deriving further CONT/BREAK requests after resume. */
    read_value = value ^ 0xff; SF = 0;
    pad_resume();
    assert(handler && interrupt_enabled);
    handler();
    assert(write_count == before);
    begin();
    continuations(1);
    assert(read_count == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    /* Keep the read callback available even when the fixed parser never reads. */
    (void)pad_test_ireg0_read;
    for (unsigned value = 0; value < 256; ++value) {
        if (!strcmp(argv[1], "complete")) { complete(value); continue; }
        for (unsigned n = 1; n <= 16; ++n) {
            unsigned pages = n % 16;
            if (!strcmp(argv[1], "invalid")) cancel(value, pages, 0);
            else if (!strcmp(argv[1], "timeout")) cancel(value, pages, 1);
            else if (!strcmp(argv[1], "handoff")) handoff(value, pages);
            else return 2;
        }
    }
    printf("writeonly: pass (%s; 256 independent readbacks; "
           "16 packet/cancel positions)\n", argv[1]);
}
