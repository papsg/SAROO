/* Adapted from paps-g 218abca2. Host fixture for disc-launch mailbox results. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "constants.inc"

typedef struct { int current, num; } MENU_DESC;
static MENU_DESC sel_menu = {.current = 3, .num = MENU_ITEMS};
static int page = 2, total_disc, total_page, sel_mode, use_sys_load, cover_top;
static int skip_patch;
static int no_game_patches __attribute__((unused));
#ifdef HAS_LAUNCH_CHOICE
static uint32_t now;
static uint32_t get_timer(void) { return now; }
#endif
static uint32_t catalog_count = 64;
#define IMGINFO_ADDR (&catalog_count)
#define LE32(p) (*(uint32_t *)(p))
#define TT(s) (s)
#ifdef SD_OVERLAY_REQUEST
static uint8_t sysinfo[0x40];
static unsigned expected_overlay_bit;
static unsigned expected_load_command = SSCMD_LOADDISC;
#define SYSINFO_ADDR ((uintptr_t)sysinfo)
#define REG8(address) (*(uint8_t *)(address))
#else
enum { expected_overlay_bit = 0 };
#endif

static volatile uint16_t reply_arg, command_word;
static uint16_t next_reply;
static int commands, errors, error_value, bios_calls, player_calls, bios_result;
static int power_error, binary_calls, off_calls, on_calls;
static unsigned offers, cancels;
static volatile uint16_t *command_register(void)
{
    if (command_word) {
        assert(command_word == expected_load_command);
        assert(reply_arg == ((page * MENU_ITEMS + sel_menu.current) | expected_overlay_bit));
        commands++;
        reply_arg = next_reply;
        command_word = 0;
    }
    return &command_word;
}
#define SS_ARG reply_arg
#define SS_CMD (*command_register())
static void select_notify(int n) { (void)n; assert(0); }
static void menu_update(MENU_DESC *m, int n) { (void)m; (void)n; assert(0); }
static void page_update(int n) { (void)n; assert(0); }
static void menu_status(MENU_DESC *m, const char *s) {
    assert(m == &sel_menu);
    if (!s) ++cancels;
    else if (!strcmp(s, "A: Without game patches  B: Cancel")) ++offers;
}
static void menu_error(MENU_DESC *m, const char *s, int value)
{ assert(m == &sel_menu && s); errors++; error_value = value; }
static int bios_cd_cmd(int command) { assert(command == 0); bios_calls++; return bios_result; }
static void my_cdplayer(void) { player_calls++; }
static int run_binary(int n, int run)
{ assert(n == page * MENU_ITEMS + sel_menu.current && run == 1); binary_calls++; return 0; }
static int cdblock_on(int n) { assert(!n);on_calls++;return power_error; }
static __attribute__((unused)) int cdblock_off(void) { off_calls++;return power_error; }
static void change_cover_layer(void) { assert(0); }

#define menu_command_wait() 0
#include "selector.inc"

static void check(int button, unsigned word, int boot_error)
{
    next_reply = word;
    reply_arg = 0xdead;
    command_word = 0;
    commands = errors = bios_calls = player_calls = 0;
    error_value = 123456;
    use_sys_load = 7;
    bios_result = boot_error;
    int expected = word >= 32768 ? (int)word - 65536 : (int)word;
    if (!word && button == PAD_A) expected = boot_error;
    assert(sel_handle((button << 16) | button) == 0);
#ifdef HAS_LAUNCH_CHOICE
    if (button == PAD_A) {
        assert(!commands);
        assert(!sel_handle(PAD_A << 16));
        assert(!no_game_patches);
    }
#endif
    assert(commands == 1 && command_word == 0);
    assert(bios_calls == (button == PAD_A && !word));
    assert(player_calls == (button == PAD_Z && !word));
    assert(use_sys_load == ((button == PAD_Z && !word) ? 0 : 7));
    assert(errors == (expected != 0));
    if (expected && error_value != expected) {
        fprintf(stderr, "button=%c reply=%u expected=%d displayed=%d\n",
                button == PAD_A ? 'A' : 'Z', word, expected, error_value);
        assert(error_value == expected);
    }
}

static void check_power_gate(void)
{
    for (int mode = 0; mode < 2; mode++) {
        for (int choice = 0; choice < 2; choice++) {
            if (!mode && !choice) continue;
            sel_mode = mode;
            int button = choice ? PAD_Z : PAD_A;
            use_sys_load = 7;
            commands = errors = binary_calls = off_calls = on_calls = player_calls = 0;
            power_error = -3;
            assert(!sel_handle((button << 16) | button));
            assert(!commands && !binary_calls && !player_calls && use_sys_load == 7);
            assert(errors == 1 && error_value == -3);
            assert(on_calls == (mode && choice) && off_calls == !(mode && choice));
        }
    }
}

static void check_choice(void)
{
    for (unsigned capability = 0; capability <= 5; ++capability) {
        sysinfo[0x3c] = capability;
        next_reply = (uint16_t)-5;
        commands = errors = bios_calls = player_calls = offers = cancels = 0;
        expected_load_command = SSCMD_LOADDISC;
        expected_overlay_bit = 0;
#ifdef HAS_LAUNCH_CHOICE
        sel_launch_choice = (struct launch_choice){0};
#endif
        assert(!sel_handle(PAD_A | PAD_A << 16));
        assert(!commands); /* Parent launches here: this is a feature control. */
#ifdef HAS_LAUNCH_CHOICE
        now += 649999;
        menu_launch_choice_idle(&sel_menu);
        assert(!offers && !commands);
        now++;
        menu_launch_choice_idle(&sel_menu);
        assert(offers == 1 && !commands);
        assert(!sel_handle(PAD_A | PAD_A << 16) && !commands);
        assert(!sel_handle(PAD_A << 16) && !commands);
        assert(!sel_handle(PAD_A | PAD_A << 16));
        assert(commands == 1 && errors == 1 && !bios_calls && no_game_patches);
        assert(!sel_handle(PAD_A | PAD_A << 16) && commands == 1);
        assert(!sel_handle(PAD_A << 16));
        expected_overlay_bit = capability >= 1 && capability < 3 ? SD_OVERLAY_REQUEST : 0;
        expected_load_command = capability >= 3 && capability <= SD_OVERLAY_VERSION ?
            (capability >= 4 ? SSCMD_ROMDISC : SSCMD_FIRSTDISC) : SSCMD_LOADDISC;
        check(PAD_A, 0, 0);
        assert(!no_game_patches);
        assert(!sel_handle(PAD_A | PAD_A << 16));
        now += 650000;
        menu_launch_choice_idle(&sel_menu);
        assert(!sel_handle(0x01000100u | PAD_A));
        assert(cancels == 1);
        assert(!sel_handle(PAD_A | PAD_A << 16));
        assert(!sel_handle(PAD_A << 16));
        expected_overlay_bit = 0;
        expected_load_command = SSCMD_LOADDISC;
        no_game_patches = 1;
        check(PAD_Z, 0, 0);
        assert(!no_game_patches);
#endif
    }
}

static void check_choice_timing(void)
{
#ifdef HAS_LAUNCH_CHOICE
    for (unsigned wrap = 0; wrap < 2; ++wrap) {
        uint32_t start = wrap ? UINT32_MAX - 300000u : 500u;
        for (unsigned duration = 0; duration <= 650000; duration += 1000) {
            struct launch_choice s = {0};
            assert(launch_choice_poll(&s, 0x04000400u, start) == LAUNCH_CONSUMED);
            assert(launch_choice_poll(&s, 0x04000000u, start + duration) ==
                   (duration < 650000 ? LAUNCH_NORMAL : LAUNCH_OFFER));
            if (duration < 650000) assert(!s.phase);
            else {
                assert(s.phase == 3);
                assert(launch_choice_poll(&s, 0x04000400u, start + 700000) == LAUNCH_UNPATCHED);
                assert(launch_choice_poll(&s, 0x04000400u, start + 900000) == LAUNCH_CONSUMED);
                assert(launch_choice_poll(&s, 0x04000000u, start + 910000) == LAUNCH_CONSUMED);
                assert(!s.phase);
            }
        }
        struct launch_choice s = {0};
        assert(launch_choice_poll(&s, 0x04000400u, start) == LAUNCH_CONSUMED);
        for (unsigned time = 500000; time < 650000; time += 10000)
            assert(launch_choice_poll(&s, 0x04000400u, start + time) == LAUNCH_CONSUMED);
        assert(launch_choice_poll(&s, 0, start + 650000) == LAUNCH_OFFER);
        assert(launch_choice_poll(&s, 0, start + 900000) == LAUNCH_CONSUMED);
        assert(launch_choice_poll(&s, 0x01000500u, start + 910000) == LAUNCH_CANCEL);
        assert(launch_choice_poll(&s, 0x04000400u, start + 920000) == LAUNCH_CONSUMED);
        assert(launch_choice_poll(&s, 0x04000000u, start + 930000) == LAUNCH_CONSUMED);
        assert(!s.phase);
    }
#else
    assert(0 && "parent has no hold-A state machine");
#endif
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "choice")) { check_choice(); return 0; }
    if (!strcmp(argv[1], "timing")) { check_choice_timing(); return 0; }
    if (!strcmp(argv[1], "power")) { check_power_gate();return 0; }
#ifdef SD_OVERLAY_REQUEST
    if (!strcmp(argv[1], "overlay")) {
        for (unsigned capability = 0; capability <= 5; ++capability) {
            sysinfo[0x3c] = capability;
            expected_overlay_bit = capability >= 1 && capability < 3 ?
                                   SD_OVERLAY_REQUEST : 0;
            expected_load_command = capability >= 3 && capability <= SD_OVERLAY_VERSION ?
                                    (capability >= 4 ? SSCMD_ROMDISC : SSCMD_FIRSTDISC) : SSCMD_LOADDISC;
            check(PAD_A, 0, 0);
            expected_overlay_bit = 0;
            expected_load_command = SSCMD_LOADDISC;
            check(PAD_Z, 0, 0);
            skip_patch = 1;
            check(PAD_A, 0, 0);
            skip_patch = 0;
        }
        return 0;
    }
#endif
    int button = argv[1][0] == 'A' ? PAD_A : PAD_Z;
    /* Exercise the reported value first, then every possible mailbox reply. */
    check(button, 65530, 0);
    for (unsigned word = 0; word < 65536; word++) check(button, word, 0);
    if (button == PAD_A) {
        check(button, 0, -1);
        check(button, 0, -6);
        check(button, 0, 17);
    }
    puts("all 65536 replies preserve launch admission and error values");
    return 0;
}
