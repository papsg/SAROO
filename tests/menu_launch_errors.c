/* Adapted from paps-g 218abca2. Host fixture for disc-launch mailbox results. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "constants.inc"

typedef uint8_t u8;
typedef struct { int current, num; } MENU_DESC;
static MENU_DESC sel_menu = {.current = 3, .num = MENU_ITEMS};
static int page = 2, total_disc, total_page, sel_mode, use_sys_load, cover_top;
static uint32_t catalog_count = 64;
#define IMGINFO_ADDR (&catalog_count)
#define LE32(p) (*(uint32_t *)(p))
#define TT(s) (s)

static volatile uint16_t reply_arg, command_word;
static uint16_t next_reply;
static int commands, errors, error_value, bios_calls, player_calls, bios_result;
static int power_error, binary_calls, off_calls, on_calls;
static volatile uint16_t *command_register(void)
{
    if (command_word) {
        assert(command_word == SSCMD_LOADDISC);
        assert(reply_arg == page * MENU_ITEMS + sel_menu.current);
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
static void menu_status(MENU_DESC *m, const char *s) { assert(m == &sel_menu && s); }
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

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "power")) { check_power_gate();return 0; }
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
