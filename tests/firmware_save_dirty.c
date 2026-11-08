/* Actual firmware mbuf_mark; no hardware or storage commands are executed. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "game_save.c"

_Alignas(8) u8 test_shared[0x23000];
u16 test_arg;
static u16 command;
static u16 *test_command(void) { return &command; }

int main(int argc, char **argv)
{
    _Static_assert(sizeof(int) == 4, "SH-2 dirty words have 32 bits");
    assert(argc == 4);
    int start = atoi(argv[1]), position = atoi(argv[2]), mode = atoi(argv[3]);
    assert(start >= 8 && start <= 8000 && position >= 0 && position < 64);
    bup_dev = mode == 2 ? 0 : 1;
    mblk_start = start;
    mblk_end = start + 63;
    if (mode == 2) {
        mblk_bmp[0] = 0x12345678;
        mblk_bmp[1] = 0x76543210;
        for (int i = 0; i < 64; i++) mbuf_mark(start+i);
        assert(mblk_bmp[0] == 0x12345678 && mblk_bmp[1] == 0x76543210);
    } else {
        uint64_t expected = 0;
        for (int i = 0; i < (mode ? 64 : 1); i++) {
            int bit = mode ? i : position;
            mbuf_mark(start+bit);
            mbuf_mark(start+bit); /* Repeated marking is idempotent. */
            expected |= UINT64_C(1) << bit;
            assert((uint32_t)mblk_bmp[0] == (uint32_t)expected);
            assert((uint32_t)mblk_bmp[1] == (uint32_t)(expected >> 32));
        }
    }
    assert(mblk_start == start && mblk_end == start+63);
    assert(command == 0 && test_arg == 0);
    return 0;
}
