#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "menu_load_limits.inc"

typedef uint32_t u32;
_Static_assert(sizeof(FSIZE_t) == 8 && sizeof(UINT) == 4, "exFAT menu load types");

enum { LEDEV_NOFIRM = 1, LEDEV_FILE_ERROR };
enum { OK, MISSING, READ_ERROR, SHORT, EMPTY, CLOSE_ERROR };
static FSIZE_t length;
static unsigned scenario, opens, reads, closes, event, continued;
static UINT requested, loaded;
static unsigned char menu[IMGINFO_ADDR - FIRM_ADDR];

FRESULT f_open(FIL *fp, const TCHAR *name, BYTE mode)
{
    assert(!strcmp(name, "/SAROO/ssfirm.bin") && mode == FA_READ);
    ++opens;
    fp->obj.objsize = length;
    return scenario == MISSING ? FR_NO_FILE : FR_OK;
}

FRESULT f_read(FIL *fp, void *dest, UINT count, UINT *actual)
{
    assert(fp->obj.objsize == length && dest == menu && !closes);
    ++reads;
    requested = count;
    *actual = 0;
    if (count > sizeof menu || scenario == READ_ERROR) return FR_DISK_ERR;
    loaded = scenario == EMPTY ? 0 : count - (scenario == SHORT && count != 0);
    memset(dest, 0x53, loaded);
    *actual = loaded;
    return FR_OK;
}

FRESULT f_close(FIL *fp)
{
    assert(fp->obj.objsize == length && opens == 1);
    ++closes;
    return scenario == CLOSE_ERROR ? FR_DISK_ERR : FR_OK;
}

static void led_event(unsigned value) { assert(!event); event = value; }
#define printk(...) ((void)0)
#include "menu_load.inc"

int main(int argc, char **argv)
{
    assert(argc == 3);
    length = (FSIZE_t)strtoull(argv[1], NULL, 0);
    scenario = (unsigned)strtoul(argv[2], NULL, 0);
    memset(menu, 0xa5, sizeof menu);
    load_menu();
    assert(opens == 1);
    if (scenario == MISSING) {
        assert(!closes && !reads && !continued && event == LEDEV_NOFIRM);
    } else if (length < 8 || length > sizeof menu) {
        assert(closes == 1 && !reads && !continued && event == LEDEV_FILE_ERROR);
    } else {
        assert(closes == 1 && reads == 1 && requested == length);
        unsigned complete = scenario == OK || scenario == CLOSE_ERROR;
        assert(continued == complete && event == (complete ? 0 : LEDEV_FILE_ERROR));
        assert(loaded == (complete ? length : scenario == SHORT ? length - 1 : 0));
    }
    for (UINT i = 0; i < sizeof menu; ++i)
        assert(menu[i] == (i < loaded ? 0x53 : 0xa5));
    return 0;
}
