/* Adapted from paps-g 0dc69582: actual public parser and matcher.
 * The production parser and matcher run against fixed-size, guarded memory.
 * File operations model independent failures; no card or device is accessed. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t FSIZE_t;
typedef struct { FSIZE_t size; } FIL;
enum { FR_OK, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE,
       FR_NO_PATH, FR_INVALID_NAME, FR_DENIED, FR_EXIST, FR_INVALID_OBJECT,
       FR_WRITE_PROTECTED, FR_INVALID_DRIVE, FR_NOT_ENABLED, FR_NO_FILESYSTEM,
       FR_MKFS_ABORTED, FR_TIMEOUT, FR_LOCKED, FR_NOT_ENOUGH_CORE,
       FR_TOO_MANY_OPEN_FILES, FR_INVALID_PARAMETER };
#define FA_READ 1
#define LEDEV_SCFG_ERROR 1
#define f_size(fp) ((fp)->size)

static u8 *scratch;
static _Alignas(4) u8 system_info[0x1000];
#define TEST_SCRATCH_ADDR ((uintptr_t)scratch)
#define SYSINFO_ADDR ((uintptr_t)system_info)
static int lang_id = 41, debug_flags = 42, log_mask = 43, auto_update = 44;
static int sector_delay = 45, play_delay = 46, pend_delay = 47, sort_mode = 48;
static char mdisc_str[16] = "old-disc";
static u8 *input;
static size_t input_size;
static FSIZE_t reported_size;
static int fault, fault_code, opens, reads, closes, leds, locks, unlocks, depth, runtime_config;
static u32 requested, transferred;

static int f_open(FIL *fp, const char *name, int flags)
{
    (void)name;
    assert(flags == FA_READ);
    opens++;
    if (fault == 1) return fault_code;
    fp->size = reported_size;
    return 0;
}
static int f_read(FIL *fp, void *out, u32 size, u32 *count)
{
    (void)fp;
    reads++;
    assert(!runtime_config || depth==1);
    requested = size;
    /* Large-file rejection must occur before requesting an unsafe transfer. */
    assert(size < 0x8000);
    if (fault == 2) { *count = 0; return fault_code; }
    size_t actual = size < input_size ? size : input_size;
    if (fault == 3 && actual) actual -= (unsigned)fault_code <= actual ? fault_code : actual;
    memcpy(out, input, actual);
    *count = transferred = actual;
    return 0;
}
static int f_close(FIL *fp)
{
    (void)fp;
    closes++;
    return fault == 4 ? fault_code : 0;
}
static int printk(const char *fmt, ...) { (void)fmt; return 0; }
static void led_event(int event) { assert(event == LEDEV_SCFG_ERROR); leds++; }
static void fs_lock(void) { assert(!depth); depth++; locks++; }
static void fs_unlock(void) { assert(depth == 1); depth--; unlocks++; }

#include "config-loader.inc"

static int started, startup_result;
static int startup_parse(char *name,char *id)
{ startup_result=parse_config(name,id);return startup_result; }
static void startup_gate(void)
{
    {
#define parse_config startup_parse
#include "config-startup-gate.inc"
#undef parse_config
    }
    started++;
}

static void report(int result)
{
    printf("{\"result\":%d,\"values\":[%d,%u,%u,%d,%d,%d,%d,%d],",
           result, lang_id, (u32)debug_flags, (u32)log_mask, auto_update,
           sector_delay, play_delay, pend_delay, sort_mode);
    printf("\"multi_hex\":\"");
    for (unsigned i = 0; i < sizeof(mdisc_str); i++) printf("%02x", (u8)mdisc_str[i]);
    printf("\",\"category_num\":%d,\"system_hex\":\"", category_num);
    for (unsigned i = 0; i < sizeof(system_info); i++) printf("%02x", system_info[i]);
    printf("\",\"io\":[%d,%d,%d,%u,%u],\"leds\":%d,\"locks\":[%d,%d,%d],\"started\":%d}\n",
           opens, reads, closes, requested, transferred, leds, locks, unlocks, depth, started);
}

int main(int argc, char **argv)
{
    assert(argc == 6 || argc == 7);
    FILE *file = fopen(argv[1], "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END));
    long length = ftell(file); assert(length >= 0);
    rewind(file);
    input_size = length;
    input = malloc(input_size ? input_size : 1); assert(input);
    assert(fread(input, 1, input_size, file) == input_size);
    assert(!fclose(file));
    reported_size = strtoull(argv[5], NULL, 10);
    scratch = malloc(0x8000); assert(scratch);
    memset(scratch, 0xcc, 0x8000);
    memset(system_info, 0xa5, sizeof(system_info));
    category_num = 3;
    fault = atoi(argv[3]); fault_code = atoi(argv[4]);
    char *game = strcmp(argv[2], "-") ? argv[2] : NULL;
    runtime_config=game!=NULL;
    int result = 0;
    if (argc == 7 && !strcmp(argv[6], "startup")) {
        startup_gate();
        result = startup_result;
    } else if (argc == 7) {
        unsigned repeats = strtoul(argv[6], NULL, 10);
        assert(repeats && repeats <= 1000000);
        sector_delay = play_delay = pend_delay = category_num = 0;
        mdisc_str[0] = 0;
        memset(system_info, 0, sizeof(system_info));
        struct timespec begin, end;
        assert(!clock_gettime(CLOCK_MONOTONIC, &begin));
        for (unsigned i = 0; i < repeats; i++) {
            result = parse_config("/SAROO/saroocfg.txt", game);
            assert(!result);
        }
        assert(!clock_gettime(CLOCK_MONOTONIC, &end));
        double ns = (end.tv_sec - begin.tv_sec) * 1e9 + end.tv_nsec - begin.tv_nsec;
        fprintf(stderr, "{\"repeats\":%u,\"nanoseconds_per_parse\":%.3f}\n", repeats, ns / repeats);
    } else result = parse_config("/SAROO/saroocfg.txt", game);
    report(result);
    free(scratch); free(input);
    return 0;
}
