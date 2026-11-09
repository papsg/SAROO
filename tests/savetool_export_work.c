/* Public actual-CLI fixture; inspired by paps-g 99127a6a's poisoned-buffer checks. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bup.h"
#include "savetool_export_work.h"

static size_t cleared, crc_bytes;
static unsigned clear_calls, crc_calls;
int savetool_cli_main(int, char **);
u32 export_work_crc_impl(u8 *, int);

void export_work_clear(void *out, int value, size_t count)
{
    assert(out == save_buf && value == 0 && count <= sizeof(save_buf));
    /* A complete payload must overwrite stale storage; zero BSS cannot mask it. */
    memset(save_buf, 0xa5, sizeof(save_buf));
    clear_calls++;
    cleared += count;
    memset(out, value, count);
}

u32 crc32b(u8 *data, int size)
{
    assert(size >= 0);
    crc_calls++;
    crc_bytes += (unsigned)size;
    return export_work_crc_impl(data, size);
}

int main(int argc, char **argv)
{
    int result = savetool_cli_main(argc, argv);
    fprintf(stderr, "EXPORT_WORK clear_calls=%u clear_bytes=%zu crc_calls=%u crc_bytes=%zu\n",
            clear_calls, cleared, crc_calls, crc_bytes);
    return result;
}
