/* When integrated, include this after file_cache_purge and put every direct
 * menu command and launch/reset path under the lease. The host fixture
 * substitutes only the SH-2 atomic operation/address mapping; cartridge-bus
 * behavior still requires physical qualification. */
static volatile u8 command_lock;
static volatile u8 command_ready;

#ifndef COMMAND_READY_ADDRESS
#define COMMAND_READY_ADDRESS ((volatile u8 *)((u32)&command_ready | 0x20000000u))
#endif
#ifndef COMMAND_GATE_ADDRESS
#define COMMAND_GATE_ADDRESS ((volatile u8 *)((u32)&command_lock | 0x20000000u))
static int command_try_lock(volatile u8 *lock)
{
    int acquired;
    __asm__ volatile ("tas.b @%1; movt %0"
                      : "=r" (acquired) : "r" (lock) : "t", "memory");
    return acquired;
}
#endif
#ifndef COMMAND_GATE_RELEASE
#define COMMAND_GATE_RELEASE(lock) (*(lock) = 0)
#endif
#ifndef COMMAND_GATE_LOAD
#define COMMAND_GATE_LOAD(lock) (*(lock))
#endif
#ifndef COMMAND_GATE_POISON
#define COMMAND_GATE_POISON(lock) (*(lock) |= 1)
#endif
#ifndef COMMAND_GATE_READY
#define COMMAND_GATE_READY(address) (*(address) = 1)
#endif

/* TAS owns bit 7. Bit 0 is a sticky protocol fault in that same byte: no
 * extra state or releasable interval exists between fault and wrapper exit.
 * Only a complete, qualified restart can establish a new session. */
int command_faulted(void)
{
    return COMMAND_GATE_LOAD(COMMAND_GATE_ADDRESS) & 1;
}

void command_fault(void)
{
    COMMAND_GATE_POISON(COMMAND_GATE_ADDRESS);
}

int command_leave(void)
{
    __asm__ volatile ("" ::: "memory");
    if (command_faulted()) return 0;
    COMMAND_GATE_RELEASE(COMMAND_GATE_ADDRESS);
    return 1;
}

/* Called with the lease after command retirement. Signed errors are admitted
 * only by their command family; nonnegative replies must retain the exact tag.
 * An untagged positive value must not become an accepted negative error after
 * subtracting the tag. Matching tags alone do not establish reset generation. */
int command_reply(int tag, int minimum, int maximum)
{
    int result = (signed short)SS_ARG;
    if (result < 0) {
        if (result >= minimum) return result;
    } else {
        result -= tag;
        if ((unsigned)result <= (unsigned)maximum) return result;
    }
    command_fault();
    return -2;
}

int command_enter(void)
{
    if (!command_try_lock(COMMAND_GATE_ADDRESS)) return 0;
    /* The Reset button restarts the Saturn CPUs but leaves the FPGA command
     * register intact. A nonzero command therefore remains the request-owner
     * fence until the MCU has made its final shared reply/payload access and
     * clears it. MCU replacement remains a separate lifecycle. */
    if (SS_CMD) {
        command_leave();
        return 0;
    }
    if (!COMMAND_GATE_LOAD(COMMAND_READY_ADDRESS))
        COMMAND_GATE_READY(COMMAND_READY_ADDRESS);
    return 1;
}

/* Before the first successful idle admission, a nonzero FPGA command can only
 * be inherited across Saturn hot reset. The explicit startup path may drain
 * that owner within its existing aggregate deadline. Ordinary live leases
 * retain a set readiness byte and still return busy immediately. */
int command_boot_pending(void)
{
    return !COMMAND_GATE_LOAD(COMMAND_GATE_ADDRESS) &&
           !COMMAND_GATE_LOAD(COMMAND_READY_ADDRESS) && SS_CMD;
}

int backup_enter(void)
{
    if (!command_enter()) return 0;
    /* SH-2 data writes are write-through; invalidate this CPU's stale copies
     * before reading allocator/menu state last used by the other CPU. The
     * existing helper bounds this to one whole-cache purge (4 KiB), including
     * its interrupt-state preservation. File requests use uncached request
     * storage and don't inspect allocator state, so they do not need this
     * extra purge. Its physical cost remains to be measured. */
    file_cache_purge(0x02000000u, 0x00100000u, 0);
    return 1;
}
