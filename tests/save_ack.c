/* Adapted from paps-g 74999ab0. Actual handlers on modeled MMIO. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "commands.h"
static u16 command, argument, save_ack;
static int result, calls, operation, supplied, reset_mode;
static u32 sysinfo[1024];
static char scratch[64];
#define SYSINFO_ADDR ((uintptr_t)sysinfo)
#define TMPBUFF_ADDR ((uintptr_t)scratch)
#define SS_CMD command
#define SS_ARG argument
static int flush_savefile(void) { calls++; operation = 9; return result; }
static int load_savefile(char *name)
{ assert(name == scratch + 16); calls++; operation = 10; return result; }
static int load_smems(int id)
{ calls++; operation = 11; supplied = id; return result; }
static int flush_smems(int flags)
{ calls++; operation = 12; supplied = flags; return result; }
#include "mcu-command.inc"
static void publish(void)
{
#include "publish.inc"
}
#include "le32.inc"
static void latch(void)
{
#include "latch.inc"
}
static u16 *test_command(void)
{
    if (command) {
        if (reset_mode != 1) mcu_command();
        if (reset_mode) command = argument = 0;
    }
    return &command;
}
#undef SS_CMD
#define SS_CMD (*test_command())
#define BUP_BROKEN 4
static u32 mblk_bmp[2];
#define menu_command_wait() 0
#include "menu-command.inc"

static unsigned checks;
static void capability(void)
{
    memset(sysinfo, 0, sizeof sysinfo); save_ack = 123; latch();
    assert(save_ack == 0); checks++;
    publish(); latch(); assert(save_ack == SSCMD_SAVE_ACK);
    static const unsigned char wire[] = {0x31, 0x56, 0x41, 0x53};
    assert(!memcmp((char *)sysinfo + 16, wire, sizeof wire));
    for (unsigned i = 0; i < sizeof sysinfo; i++)
        if (i < 16 || i >= 20) assert(((u8 *)sysinfo)[i] == 0);
    checks++;
    for (unsigned bit = 0; bit < 32; bit++) {
        sysinfo[4] = SAVE_ACK_VERSION ^ (1u << bit); latch();
        assert(save_ack == 0); checks++;
    }
    publish(); latch(); assert(save_ack == SSCMD_SAVE_ACK);
    memset(sysinfo, 0, sizeof sysinfo);
    /* A reset changing the published word does not downgrade an active caller. */
    assert(save_ack == SSCMD_SAVE_ACK); checks++;
}
static void dispatch(void)
{
    for (unsigned cmd = 0; cmd <= UINT16_MAX; cmd++) {
        unsigned base = cmd & ~SSCMD_SAVE_ACK;
        command = cmd; argument = 0xa5c3; result = 0; calls = 0;
        mcu_command();
        if (base >= SSCMD_SSAVE && base <= SSCMD_SMEMS) {
            assert(calls == 1 && operation == (int)base && command == 0);
            assert(argument == (cmd & SSCMD_SAVE_ACK));
            if (base >= SSCMD_LMEMS) assert(supplied == 0xa5c3);
        } else assert(!calls && command == cmd && argument == 0xa5c3);
        checks++;
    }
    for (unsigned tag = 0; tag <= SSCMD_SAVE_ACK; tag += SSCMD_SAVE_ACK) {
        for (int cmd = SSCMD_SSAVE; cmd <= SSCMD_SMEMS; cmd++) {
            for (result = 0; result <= 19; result++) {
                command = cmd | tag; argument = 7; calls = 0;
                mcu_command();
                assert(!command && calls == 1 && argument == (result | tag));
                checks++;
            }
        }
    }
    command = 0;
}
static void completion(void)
{
    reset_mode = 0;
    for (unsigned tag = 0; tag <= SSCMD_SAVE_ACK; tag += SSCMD_SAVE_ACK) {
        save_ack = tag;
        for (result = 0; result <= 19; result++) {
            for (int flag = 1; flag <= BUPSAVE; flag++) {
                command = 0; mblk_bmp[0] = 0x98765432; mblk_bmp[1] = 0xabcdef01;
                assert(bup_update(flag) == (result ? BUP_BROKEN : 0));
                assert(operation == (flag == BUPSAVE ? SSCMD_SSAVE : SSCMD_SMEMS));
                assert(mblk_bmp[0] == ((!result && (flag & MEMS_BUF)) ? 0 : 0x98765432));
                assert(mblk_bmp[1] == ((!result && (flag & MEMS_BUF)) ? 0 : 0xabcdef01));
                checks++;
            }
#ifdef HAS_SAVE_ACK
            assert(save_command(SSCMD_LSAVE, 0) == result);
            assert(operation == SSCMD_LSAVE);
            checks++;
#endif
            for (unsigned arg = 0; arg <= UINT16_MAX; arg++) {
                assert(mems_load(arg) == (result ? BUP_BROKEN : 0));
                assert(supplied == (int)arg && operation == SSCMD_LMEMS);
                checks++;
            }
        }
    }
    save_ack = SSCMD_SAVE_ACK; result = 0;
    memset(pending_save, 0, sizeof pending_save);
    assert(commit_save(0, BUPSAVE) == 0 && !pending_save[0].state);
    assert(commit_save(1, MEMS_ALL) == 0 && !pending_save[1].state);
    checks += 2;
}
static void reset(void)
{
    save_ack = SSCMD_SAVE_ACK; result = 0; command = 0;
    for (reset_mode = 1; reset_mode <= 2; reset_mode++) {
        for (int dev = 0; dev <= 1; dev++) {
            mblk_bmp[0] = 0x98765432; mblk_bmp[1] = 0xabcdef01;
            calls = 0;
            assert(commit_save(dev, dev ? MEMS_ALL : BUPSAVE) == BUP_BROKEN);
            assert(pending_save[dev].state == SAVE_COMMIT);
            assert(mblk_bmp[0] == 0x98765432 && mblk_bmp[1] == 0xabcdef01);
            assert(calls == (reset_mode == 2));
            /* A second cleared-register response cannot finish the continuation. */
            assert(finish_pending(dev) == BUP_BROKEN && pending_save[dev].state == SAVE_COMMIT);
            checks += 2;
        }
#ifdef HAS_SAVE_ACK
        assert(save_command(SSCMD_LSAVE, 0) != 0);
        checks++;
#endif
        for (unsigned id = 8; id <= 8063; id++) {
            assert(mems_load(id) == BUP_BROKEN);
            assert(mems_load(id | 0x8000) == BUP_BROKEN);
            checks += 2;
        }
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "capability")) capability();
    else if (!strcmp(argv[1], "dispatch")) dispatch();
    else if (!strcmp(argv[1], "completion")) completion();
    else { assert(!strcmp(argv[1], "reset")); reset(); }
    printf("PASS %s %u checks\n", argv[1], checks);
    return 0;
}
