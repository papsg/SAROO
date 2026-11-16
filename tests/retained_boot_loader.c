/* papsg adaptation of paps-g 52632dd7/SSR-027. Existing BOOT admission only. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t u32;
typedef uint32_t UINT;
/* Model an exFAT-capable original extent to expose narrowing before admission. */
typedef uint64_t FSIZE_t;
typedef int FRESULT;
typedef struct { FSIZE_t size; } FIL;
enum { FR_OK = 0, FA_READ = 1, LEDEV_NOFIRM = 0x1234 };
#define f_size(file) ((file)->size)
static FSIZE_t size;
static UINT returned, requested;
static int open_result, read_result, close_result;
static u32 stack, reset;
static unsigned reads, closes, vector_reads, entries, stack_sets, control_sets, errors;

static int f_open(FIL *file, const char *name, int mode)
{
    assert(!strcmp(name, "/SAROO/mcuapp.bin") && mode == FA_READ);
    file->size = size; return open_result;
}
static int f_read(FIL *file, void *destination, UINT length, UINT *count)
{
    assert(file && destination == NULL && count);
    reads++; requested = length; *count = returned;
    return read_result;
}
static int f_close(FIL *file) { assert(file); closes++; return close_result; }
static void printk(const char *format, ...) { assert(format); }
static void led_event(int event) { assert(event == LEDEV_NOFIRM); errors++; }
static u32 boot_word(u32 address)
{
    assert(address == 0 || address == 4); vector_reads++;
    return address ? reset : stack;
}
static void __set_MSP(u32 value) { assert(value == stack); stack_sets++; }
static void __set_CONTROL(u32 value) { assert(value == 0); control_sets++; }
static void entered(void) { entries++; }
static void (*boot_entry(u32 address))(void) { assert(address == reset); return entered; }
#include "loader.inc"

static void initialize(void)
{
    size = returned = 512; stack = 0x20010000; reset = 0x101;
    open_result = read_result = close_result = 0;
    reads = closes = vector_reads = entries = stack_sets = control_sets = errors = requested = 0;
}

static int run(const char *name, int enter, unsigned expected_reads, unsigned expected_vectors)
{
    load_app();
    int ok = entries == (unsigned)enter && reads == expected_reads &&
        closes == (unsigned)!open_result && vector_reads == expected_vectors &&
        stack_sets == (unsigned)enter && control_sets == (unsigned)enter &&
        errors == (unsigned)!enter && (!reads || requested == size);
    printf("%s size=%llu status=%d count=%u read_calls=%u request=%u vector_reads=%u entry=%u\n",
           name, (unsigned long long)size, read_result, returned, reads, requested, vector_reads, entries);
    return ok ? 0 : 42;
}

int main(int argc, char **argv)
{
    assert(argc == 2); initialize();
    const char *name = argv[1]; int result;
    if (!strcmp(name, "complete")) result = run(name, 1, 1, 2);
    else if (!strcmp(name, "minimum")) { size = returned = 10; reset = 9; result = run(name, 1, 1, 2); }
    else if (!strcmp(name, "maximum")) { size = returned = 65536; reset = 65535; result = run(name, 1, 1, 2); }
    else if (!strcmp(name, "open-error")) { open_result = 4; result = run(name, 0, 0, 0); }
    else if (!strncmp(name, "size-", 5)) {
        size = strtoull(name+5, NULL, 10); returned = (UINT)size;
        result = run(name, 0, 0, 0);
    } else if (!strcmp(name, "stack-controls")) {
        const u32 stacks[] = {0x20000008, 0x20020000, 0x24000008, 0x24080000,
                              0x30000008, 0x30048000, 0x38000008, 0x38010000};
        result = 0;
        for (unsigned i = 0; i < sizeof stacks/sizeof stacks[0]; i++) {
            initialize(); stack = stacks[i]; if (run(name, 1, 1, 2)) result = 42;
        }
    } else if (!strcmp(name, "vectors")) {
        const u32 bad_stack[] = {0, 0x20000000, 0x2001ffff, 0x20020008, 0x24000000,
                                 0x24080008, 0x30000000, 0x30048008, 0x38000000,
                                 0x38010008, 0xffffffff};
        const u32 bad_reset[] = {0, 1, 7, 8, 0x100, 0x1ff + 2, 0x10001, 0xffffffff};
        result = 0;
        for (unsigned i = 0; i < sizeof bad_stack/sizeof bad_stack[0]; i++) {
            initialize(); stack = bad_stack[i]; if (run(name, 0, 1, 2)) result = 42;
        }
        for (unsigned i = 0; i < sizeof bad_reset/sizeof bad_reset[0]; i++) {
            initialize(); reset = bad_reset[i]; if (run(name, 0, 1, 2)) result = 42;
        }
        initialize(); size = returned = 511; reset = 511;
        if (run(name, 0, 1, 2)) result = 42; /* Only one entry byte present. */
    } else {
        if (!strcmp(name, "error-zero")) { read_result = 1; returned = 0; }
        else if (!strcmp(name, "error-prefix")) { read_result = 1; returned = 8; }
        else if (!strcmp(name, "error-complete")) read_result = 1;
        else if (!strcmp(name, "short-zero")) returned = 0;
        else if (!strcmp(name, "short-prefix")) returned = 8;
        else if (!strcmp(name, "short-one")) returned = 511;
        else if (!strcmp(name, "over-count")) returned = 513;
        else if (!strcmp(name, "close-error")) close_result = 1;
        else return 90;
        result = run(name, 0, 1, 0);
    }
    if (!result) puts("pass");
    return result;
}
