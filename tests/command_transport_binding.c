/* Compile the public entry points extracted from Firm_Saturn/main.c. Only the
 * SH-2 atomic/address primitive and owned transport endpoints are substituted. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static volatile u16 test_command, test_argument;
#define SS_CMD test_command
#define SS_ARG test_argument
#define COMMAND_GATE_ADDRESS (&command_lock)
#define COMMAND_READY_ADDRESS (&command_ready)
#define COMMAND_GATE_LOAD(lock) __atomic_load_n((lock), __ATOMIC_ACQUIRE)
#define COMMAND_GATE_RELEASE(lock) __atomic_store_n((lock), 0, __ATOMIC_RELEASE)
#define COMMAND_GATE_POISON(lock) ((void)__atomic_fetch_or((lock), 1, __ATOMIC_RELEASE))
#define COMMAND_GATE_READY(lock) __atomic_store_n((lock), 1, __ATOMIC_RELEASE)
static int command_try_lock(volatile u8 *lock)
{
    return __atomic_fetch_or(lock, 0x80, __ATOMIC_ACQUIRE) == 0;
}
static void file_cache_purge(u32 address, u32 size, int mirrored)
{
    assert(address == 0x02000000 && size == 0x00100000 && !mirrored);
}
#include "command_gate.c"

static int calls, nested, inject_fault;
static int ordinary_logs, owned_logs;
int printk(char *format, ...)
{
    (void)format;
    assert(!command_lock);
    ++ordinary_logs;
    return 0;
}
int command_printk(char *format, ...)
{
    (void)format;
    assert(command_lock == 0x80);
    ++owned_logs;
    return 0;
}
#include "transport-compare.inc"

int archive_command(unsigned, u32, void *, int);
int archive_command_owned(unsigned device, u32 offset, void *buf, int length)
{
    assert(command_lock == 0x80 && device == 1 && offset == 17 && length == 3 && buf);
    ++calls;
    if (inject_fault) { command_fault(); return 3; }
    assert(archive_command(device, offset, buf, length) == -16);
    ++nested;
    return 3;
}
int archive_find_slot_owned(const u8 *key, void *workspace, u32 capacity)
{
    assert(command_lock == 0x80 && key && workspace && capacity == 512);
    ++calls;
    return 0x10000;
}
int read_file_bounded_owned(char *name, int offset, int size, void *buf, u32 capacity)
{
    assert(command_lock == 0x80 && name && offset == 2 && size == 4 && buf && capacity == 8);
    ++calls;
    return -4;
}
int write_file_owned(char *name, int offset, int size, void *buf)
{
    assert(command_lock == 0x80 && name && offset == 2 && size == 4 && buf);
    ++calls;
    return 4;
}
#include "transport-public.inc"

int main(void)
{
    u8 bytes[512] = {1};
    char name[] = "file";
    /* The exact public wrappers must refuse a foreign owner before invoking
     * any owned endpoint or modifying its caller's storage. */
    command_lock = 0x80;
    assert(archive_command(1, 17, bytes, 3) == -16);
    assert(archive_find_slot(bytes, bytes, sizeof bytes) == -16);
    assert(read_file_bounded(name, 2, 4, bytes, 8) == -16);
    assert(write_file(name, 2, 4, bytes) == -16);
    assert(calls == 0 && bytes[0] == 1 && test_command == 0 && test_argument == 0);
    command_lock = 0;
    assert(archive_command(1, 17, bytes, 3) == 3 && !command_lock);
    assert(archive_find_slot(bytes, bytes, sizeof bytes) == 0x10000 && !command_lock);
    assert(read_file_bounded(name, 2, 4, bytes, 8) == -4 && !command_lock);
    assert(write_file(name, 2, 4, bytes) == 4 && !command_lock);
    assert(calls == 4 && nested == 1);
    u8 changed[] = {1, 2}, expected[] = {1, 3};
    assert(memcmp(changed, expected, 2) < 0 && ordinary_logs == 3);
    assert(command_enter());
    assert(command_memcmp(changed, expected, 2) < 0 && owned_logs == 3);
    assert(command_leave());
    inject_fault = 1;
    assert(archive_command(1, 17, bytes, 3) == -2);
    assert(command_lock == 0x81 && command_faulted());
    assert(write_file(name, 2, 4, bytes) == -16 && calls == 5);
    puts("PASS public command ownership and quarantine");
    return 0;
}
