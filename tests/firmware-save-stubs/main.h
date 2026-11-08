#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
extern u8 test_shared[];
extern u16 test_arg;
static u16 *test_command(void);
#define SAVINFO_ADDR ((uintptr_t)test_shared)
#define SS_ARG test_arg
#define SS_CMD (*test_command())
#define SSCMD_SSAVE 9
#define SSCMD_LMEMS 11
#define SSCMD_SMEMS 12
#define printk(...) ((void)0)
static u32 debug_flag;
static void sci_init(void) {}
static void sys_bup_init(u8 *lib, u8 *work, void *cfg)
{ (void)lib; (void)work; (void)cfg; }
