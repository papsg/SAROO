/* Host model of the documented TAS primitive; production gate logic is real. */
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static volatile u16 test_command, test_arg;
static unsigned purge_count;
#define SS_CMD test_command
#define SS_ARG test_arg
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
    __atomic_fetch_add(&purge_count, 1, __ATOMIC_RELAXED);
}
#include "command_gate.c"

static void boot_and_fault(void)
{
    test_command = 0x55;
    test_arg = 0x1234;
    assert(command_boot_pending());
    assert(!command_enter());
    assert(!command_lock && !command_ready && test_command == 0x55 && test_arg == 0x1234);
    test_command = 0;
    assert(backup_enter());
    assert(command_ready && !command_boot_pending() && purge_count == 1);
    assert(!command_enter());
    assert(command_leave() && !command_lock);
    test_command = 0x66;
    assert(!command_enter() && !command_boot_pending());
    test_command = 0;
    assert(command_enter());
    test_arg = 0; /* Untagged zero cannot complete a tagged request. */
    assert(command_reply(32, 0, 0) == -2);
    assert(command_faulted() && !command_leave() && command_lock == 0x81);
    assert(!backup_enter() && purge_count == 1);
}

static void all_replies(void)
{
    const int limits[][2] = {{0, 19}, {-19, 0}, {-19, 19}, {-19, 3}, {0, 0}};
    unsigned cases = 0;
    for (int tag = 0; tag <= 32; tag += 32)
        for (unsigned family = 0; family < sizeof limits / sizeof limits[0]; family++)
            for (unsigned raw = 0; raw <= UINT16_MAX; raw++) {
                /* Each row models a separate qualified startup. */
                command_lock = command_ready = test_command = 0;
                test_arg = raw;
                assert(command_enter());
                int signed_result = raw >= 32768 ? (int)raw - 65536 : (int)raw;
                int valid = raw >= 32768 ? signed_result >= limits[family][0] :
                    raw >= (unsigned)tag && raw <= (unsigned)(tag + limits[family][1]);
                int expected = raw >= 32768 ? signed_result : (int)raw - tag;
                assert(command_reply(tag, limits[family][0], limits[family][1]) ==
                       (valid ? expected : -2));
                assert(!!command_faulted() == !valid);
                assert(command_leave() == valid);
                assert(command_lock == (valid ? 0 : 0x81));
                cases++;
            }
    assert(cases == 2u * 5u * 65536u);
    printf("PASS replies=%u\n", cases);
}

static unsigned shared, accepted[2], rejected[2];
static pthread_mutex_t start_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
static unsigned waiting;

static void *race(void *argument)
{
    unsigned id = (uintptr_t)argument;
    pthread_mutex_lock(&start_mutex);
    waiting++;
    pthread_cond_broadcast(&start_cond);
    while (waiting < 2) pthread_cond_wait(&start_cond, &start_mutex);
    pthread_mutex_unlock(&start_mutex);
    for (unsigned i = 0; i < 20000; i++) {
        if (!backup_enter()) { rejected[id]++; sched_yield(); continue; }
        unsigned old = shared;
        shared = old + 1;
        accepted[id]++;
        assert(command_leave());
        if (!(i & 127)) sched_yield();
    }
    return NULL;
}

static void contention(void)
{
    command_lock = command_ready = test_command = test_arg = 0;
    purge_count = 0;
    pthread_t threads[2];
    assert(!pthread_create(&threads[0], NULL, race, (void *)(uintptr_t)0));
    assert(!pthread_create(&threads[1], NULL, race, (void *)(uintptr_t)1));
    assert(!pthread_join(threads[0], NULL) && !pthread_join(threads[1], NULL));
    assert(accepted[0] && accepted[1]);
    assert(shared == accepted[0] + accepted[1]);
    assert(rejected[0] + rejected[1] + shared == 40000);
    assert(purge_count == shared && !command_lock);
    printf("PASS accepted=%u rejected=%u\n", shared, rejected[0] + rejected[1]);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (argv[1][0] == 'b') { boot_and_fault(); puts("PASS boot/fault"); }
    else if (argv[1][0] == 'r') all_replies();
    else if (argv[1][0] == 'c') contention();
    else abort();
    return 0;
}
