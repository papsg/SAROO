/* Include the actual module and link the common parser with its CLI renamed.
 * An exact-size destination exposes an odd-length overrun hidden by save_buf. */
#include <assert.h>
#include "ss_bup.c"
int bup_load(char *name);

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(bup_load(argv[1]) == 0);
    BUPINFO *info = GET_BUP_INFO();
    load_blist(info, 2);
    assert(info->dsize == 3);
    u8 *data = malloc(3);
    assert(data);
    assert(read_data(info, data) == 0);
    assert(memcmp(data, "ABC", 3) == 0);
    free(data);
    return 0;
}
