/* Adapted from paps-g a09e734b. Menu state and mailbox model for stock code. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "constants.inc"

typedef struct {
    int num, current;
    char title[64], items[MENU_ITEMS][96];
    char *version;
} MENU_DESC;

static MENU_DESC sel_menu;
static int total_disc, total_page, page, sel_mode;
enum { BIN_FOLDER_MODE = 2 };
static volatile u16 command_value, argument;
static u16 menu_ack;
static u8 catalog_bytes[0x23000], shared[256], sysinfo[0x1000];
static u32 now;
static int flat_calls, flat_reads, flat_restores, idle_ticks, gif_ticks;
static int menu_action, wait_ticks, file_reads, last_error, powers, physical;
static const char child[] = "/SAROO/BIN/Tools@Demo";
static const char leaf[] = "Run" "@" "06008000.bin";
#define IMGINFO_ADDR ((uintptr_t)catalog_bytes)
#define TMPBUFF_ADDR ((uintptr_t)shared)
#define SYSINFO_ADDR ((uintptr_t)sysinfo)
#define SS_ARG argument
#define TT(s) (s)
#define printk(...) ((void)0)

static volatile u16 *command_register(void)
{
    if (command_value == SSCMD_LISTBINS) {
        flat_reads++;
        command_value = 0;
    } else if (command_value == SSCMD_LISTDISC) {
        flat_restores++;
        command_value = 0;
    }
    return &command_value;
}
#define SS_CMD (*command_register())

static int menu_command_wait(void) { return command_value ? -15 : 0; }
static void select_game(void) { flat_calls++; }
static u32 LE32(void *ptr)
{
    u8 *p = ptr;
    return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
static void set32(u8 *p, u32 value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}
static void list(const char *name, int directory)
{
    set32(catalog_bytes, 1);
    set32(catalog_bytes + 4, 0x2800u | (directory ? BIN_DIRECTORY_ENTRY : 0));
    strcpy((char *)catalog_bytes + 0x2800, name);
}
static void complete(int result)
{
    assert(command_value && !argument);
    argument = result | SSCMD_SAVE_ACK;
    command_value = 0;
}
static u32 get_timer(void) { return now += MS2TICK(1); }
static int timer_reached(u32 tick, u32 deadline) { return tick - deadline < 0x80000000u; }
#ifdef SAROO_SAVE_GENERATION
static volatile u8 command_lock;
static int overwrite_path_on_release;
#define COMMAND_GATE_ADDRESS (&command_lock)
#define COMMAND_READY_ADDRESS (&command_ready)
#define COMMAND_GATE_LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define COMMAND_GATE_RELEASE(p) do { \
    if ((p) == &command_lock && overwrite_path_on_release) \
        memset(shared + 16, 'X', 16); \
    __atomic_store_n((p), 0, __ATOMIC_RELEASE); \
} while (0)
#define COMMAND_GATE_POISON(p) ((void)__atomic_fetch_or((p), 1, __ATOMIC_RELEASE))
#define COMMAND_GATE_READY(p) __atomic_store_n((p), 1, __ATOMIC_RELEASE)
static int command_try_lock(volatile u8 *p)
{ return __atomic_fetch_or(p, 0x80, __ATOMIC_ACQUIRE) == 0; }
static void file_cache_purge(u32 address, u32 size, int mirrored)
{ assert(address == 0x02000000u && size == 0x00100000u && !mirrored); }
#include "command_gate.c"
#define MENU_CHANNEL_ADDRESS (&menu_channel)
#include "menu_channel.c"
#endif
static void draw_menu_frame(MENU_DESC *menu)
{ assert(menu == &sel_menu && menu->num <= MENU_ITEMS && menu->current >= 0); }
static void menu_status(MENU_DESC *menu, char *message) { assert(menu == &sel_menu && message); }
static void menu_error(MENU_DESC *menu, char *format, ...)
{
    assert(menu == &sel_menu);
    va_list args;
    va_start(args, format);
    last_error = va_arg(args, int);
    va_end(args);
}
static void add_menu_item(MENU_DESC *menu, char *text)
{ assert(menu->num < MENU_ITEMS); snprintf(menu->items[menu->num++], 96, "%s", text); }
static int menu_cd_power(MENU_DESC *menu, int on, int wait)
{ assert(menu == &sel_menu && !wait); powers++; physical = on; return 0; }
static int sel_handle(int ctrl) { (void)ctrl; return 0; }
static void menu_timer(MENU_DESC *menu) { assert(menu == &sel_menu); idle_ticks++; }
static void gif_timer(void) { gif_ticks++; }
static void pad_suspend(void) { abort(); }
static void pad_resume(void) { abort(); }
static u32 file_capacity(u32 addr, int binary)
{ assert(addr == 0x06008000 && binary); return 0xf8000; }
static int read_file_bounded(char *path, int offset, int size, void *dst, u32 capacity)
{
    char expected[240];
    snprintf(expected, sizeof(expected), "%s/%s", child, leaf);
    assert(!strcmp(path, expected) && !offset && !size);
    assert((uintptr_t)dst == 0x06008000 && capacity == 0xf8000);
    file_reads++;
    return -7;
}
static u32 button(u32 bit) { return (bit << 16) | bit; }

static u32 conio_getc(void)
{
    switch (menu_action) {
    case 0:
        assert(command_value == (SSCMD_SELECT | SSCMD_SAVE_ACK) && argument == 0x4321);
        assert(shared[16] == 0xa5);
        if (++wait_ticks < 3) return 0;
        argument = SSCMD_SAVE_ACK;
        command_value = 0;
        break;
    case 1:
        assert(command_value == SSCMD_BINDIR && !strcmp((char *)shared + 16, BIN_ROOT));
        list("Tools@Demo", 1);
        complete(0);
        break;
    case 2:
        assert(sel_menu.num == 1 && !strcmp(sel_menu.items[0], " 0: /Tools@Demo"));
        menu_action++;
        return button(PAD_A);
    case 3:
        assert(command_value == SSCMD_BINDIR && !strcmp((char *)shared + 16, child));
        list(leaf, 0);
        complete(0);
        break;
    case 4:
        assert(!strcmp(sel_menu.version, child) && sel_menu.num == 1);
        assert(!strcmp(sel_menu.items[0], " 0: Run"));
        menu_action++;
        return button(PAD_Z);
    case 5:
        assert(file_reads == 1 && powers == 1 && physical && last_error == -7);
        assert(!strcmp(sel_menu.version, child));
        menu_action++;
        return button(PAD_C);
    case 6:
        assert(command_value == SSCMD_BINDIR && !strcmp((char *)shared + 16, BIN_ROOT));
        list("Tools@Demo", 1);
        complete(0);
        break;
    case 7:
        assert(!strcmp(sel_menu.version, BIN_ROOT));
        menu_action++;
        return button(PAD_B);
    case 8:
        assert(command_value == (SSCMD_LISTDISC | SSCMD_SAVE_ACK));
        list("Game", 0);
        complete(0);
        break;
    default:
        abort();
    }
    menu_action++;
    return 0;
}

#include "browser.inc"

static void browse(void)
{
    menu_ack = SSCMD_SAVE_ACK;
    set32(sysinfo + 0x34, BIN_DIRECTORY_VERSION);
    command_value = SSCMD_SELECT | SSCMD_SAVE_ACK;
    argument = 0x4321;
#ifdef SAROO_SAVE_GENERATION
    menu_channel_init(1);
    menu_channel.flags |= MENU_ACTIVE;
    command_lock = 0x80;
#endif
    memset(shared, 0xa5, sizeof(shared));
    select_bins();
    assert(menu_action == 9 && file_reads == 1 && !flat_calls && !command_value);
    assert(idle_ticks >= 7 && gif_ticks == idle_ticks && !sel_mode);
    assert(!strcmp((char *)catalog_bytes + 0x2800, "Game"));
#ifdef SAROO_SAVE_GENERATION
    assert(!command_lock && !menu_channel.lock && menu_channel.flags == MENU_ENABLED);
#endif
}

static void flat(void)
{
    for (int configuration = 0; configuration < 3; ++configuration) {
        menu_ack = configuration ? SSCMD_SAVE_ACK : 0;
        set32(sysinfo + 0x34, configuration == 1 ? 0 : configuration + 1);
        select_bins();
    }
    assert(flat_calls == 3 && flat_reads == 3 && flat_restores == 3 && !command_value);
}

#if HAS_BIN_BROWSER
static BIN_BROWSER listing(void)
{
    BIN_BROWSER browser = {0};
#ifdef SAROO_SAVE_GENERATION
    menu_channel_init(1);
#endif
    sel_mode = BIN_FOLDER_MODE;
    sel_menu.version = bin_current_path();
    bin_schedule(&browser, BIN_CURRENT);
    assert(!bin_step(&browser) && browser.phase == BIN_LISTING && command_value == SSCMD_BINDIR);
#ifdef SAROO_SAVE_GENERATION
    assert(command_lock == 0x80 && menu_channel.lock == 0x80);
#endif
    return browser;
}

static void cancel(void)
{
    BIN_BROWSER browser = listing();
    bin_input(&browser, button(PAD_B));
    now += MS2TICK(7000);
    assert(!bin_step(&browser) && browser.leave && browser.slow && command_value == SSCMD_BINDIR);
    assert(!argument && !strcmp((char *)shared + 16, BIN_ROOT));
    complete(0);
    assert(!bin_step(&browser) && browser.phase == BIN_WAIT);
    assert(!bin_step(&browser) && command_value == (SSCMD_LISTDISC | SSCMD_SAVE_ACK));
    complete(-5);
    assert(!bin_step(&browser) && browser.phase == BIN_IDLE && last_error == -5 && !browser.leave);
    bin_input(&browser, button(PAD_B));
    assert(!bin_step(&browser) && command_value == (SSCMD_LISTDISC | SSCMD_SAVE_ACK));
    complete(0);
    assert(bin_step(&browser) && !command_value);
}

static void invalid_reply(void)
{
    BIN_BROWSER browser = listing();
    command_value = 0;
    assert(!bin_step(&browser) && browser.phase == BIN_FAULT);
#ifdef SAROO_SAVE_GENERATION
    assert(command_lock == 0x81 && !menu_channel.lock &&
           (menu_channel.flags & MENU_FAULT));
#endif
    bin_input(&browser, button(PAD_A));
    bin_input(&browser, button(PAD_B));
    assert(!bin_step(&browser) && !command_value && browser.phase == BIN_FAULT);
    assert(!strcmp((char *)shared + 16, BIN_ROOT));
}

#ifdef SAROO_SAVE_GENERATION
static void foreign_owner(void)
{
    BIN_BROWSER browser = {0};
    menu_channel_init(1);
    sel_mode = BIN_FOLDER_MODE;
    sel_menu.version = bin_current_path();
    bin_schedule(&browser, BIN_CURRENT);
    command_lock = 0x80; /* Another CPU still owns the command channel. */
    memset(shared, 0xa5, sizeof(shared));
    assert(!bin_step(&browser) && browser.phase == BIN_WAIT);
    assert(command_lock == 0x80 && !command_value && shared[16] == 0xa5);
    command_lock = 0;
    assert(!bin_step(&browser) && browser.phase == BIN_LISTING);
    assert(command_lock == 0x80 && menu_channel.lock == 0x80);
    list("Tools@Demo", 1);
    complete(0);
    assert(!bin_step(&browser) && browser.phase == BIN_IDLE);
    assert(!command_lock && !menu_channel.lock);
}

static void output_owner(void)
{
    BIN_BROWSER browser = listing();
    list("Tools@Demo", 1);
    complete(0);
    overwrite_path_on_release = 1;
    assert(!bin_step(&browser) && browser.phase == BIN_IDLE);
    assert(!strcmp(bin_current_path(), BIN_ROOT));
    assert(shared[16] == 'X' && !command_lock && !menu_channel.lock);
}
#endif

static void retry(void)
{
    BIN_BROWSER browser = listing();
    list("Tools@Demo", 1);
    complete(0);
    assert(!bin_step(&browser) && sel_menu.num == 1);
    bin_input(&browser, button(PAD_A));
    assert(!bin_step(&browser) && !strcmp((char *)shared + 16, child));
    set32(catalog_bytes, 0);
    complete(-5);
    assert(!bin_step(&browser) && browser.target == BIN_CURRENT);
    assert(!bin_step(&browser) && !strcmp((char *)shared + 16, BIN_ROOT));
    complete(-3);
    assert(!bin_step(&browser) && browser.phase == BIN_IDLE && last_error == -3);
    assert(!sel_menu.num && !sel_menu.items[0][0] && !command_value);
    assert(!bin_step(&browser) && !command_value);
}

static void bounds(void)
{
    char path[BIN_PATH_CAPACITY];
    strcpy(path, BIN_ROOT);
    char *invalid[] = {".", "..", "A.", "A ", "X/Y", "X\\Y", "X:Y", ""};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        assert(bin_append(path, invalid[i]) < 0);
        assert(!strcmp(path, BIN_ROOT));
    }
    char name[230];
    memset(name, 'a', sizeof(name));
    name[229] = 0;
    assert(bin_append(path, name) < 0 && !strcmp(path, BIN_ROOT));
    name[228] = 0;
    assert(!bin_append(path, name) && strlen(path) == 239);

    int directory;
    list("Work", 1);
    assert(!strcmp(bin_catalog_name(0, &directory), "Work") && directory);
    set32(catalog_bytes + 4, BIN_DIRECTORY_ENTRY | 0x22fff);
    catalog_bytes[0x22fff] = 'x';
    assert(!bin_catalog_name(0, &directory));
    set32(catalog_bytes, 2560);
    assert(!bin_catalog_name(0, &directory));
}

/* Adapted from paps-g b6b04d6d: full identities, independent launch limits. */
static void utf8(void)
{
    char name[766];
    for (unsigned i = 0; i < 255; ++i) {
        name[3*i] = (char)0xe0; name[3*i+1] = (char)0xa0; name[3*i+2] = (char)0x80;
    }
    name[765] = 0;
    list(name, 1);
    set32(catalog_bytes, 2);
    set32(catalog_bytes + 8, 0x2b00);
    strcpy((char *)catalog_bytes + 0x2b00, leaf);
    strcpy(bin_path, child);
    sel_mode = BIN_FOLDER_MODE;
    bin_draw(2);
    assert(total_disc == 2 && sel_menu.num == 2 && strstr(sel_menu.items[1], "Run"));
    assert(strlen(sel_menu.items[0]) < sizeof(sel_menu.items[0]));
    int directory;
    assert(!strcmp(bin_catalog_name(0, &directory), name) && directory);
    assert(bin_prepare_path(0) < 0 && !strcmp(bin_current_path(), child));

    set32(catalog_bytes + 4, 0x2800);
    bin_draw(2);
    BIN_BROWSER browser = {0};
    bin_input(&browser, button(PAD_A));
    assert(last_error == -1 && !file_reads);
    sel_menu.current = 1;
    bin_input(&browser, button(PAD_A));
    assert(file_reads == 1 && last_error == -7 && !strcmp(bin_current_path(), child));

    sel_mode = 0;
    bin_draw(2);
    assert(total_disc == 2 && sel_menu.num == 2 && strstr(sel_menu.items[1], "Run"));
    /* A leaf with no terminator in the full supported capacity still refuses. */
    memset(catalog_bytes + 0x2800, 'x', 766);
    catalog_bytes[0x2800 + 766] = 0;
    assert(!bin_catalog_name(0, &directory));
}
#endif

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "browse")) browse();
    else if (!strcmp(argv[1], "flat")) flat();
#if HAS_BIN_BROWSER
    else if (!strcmp(argv[1], "cancel")) cancel();
    else if (!strcmp(argv[1], "invalid-reply")) invalid_reply();
#ifdef SAROO_SAVE_GENERATION
    else if (!strcmp(argv[1], "foreign-owner")) foreign_owner();
    else if (!strcmp(argv[1], "output-owner")) output_owner();
#endif
    else if (!strcmp(argv[1], "retry")) retry();
    else if (!strcmp(argv[1], "bounds")) bounds();
    else if (!strcmp(argv[1], "utf8")) utf8();
#endif
    else abort();
    printf("PASS %s\n", argv[1]);
}
