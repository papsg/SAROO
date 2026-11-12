/* Adapted from paps-g 77b8f027, dca425eb and 39a45215.
 * Model buffered PSC, one-pulse mode and IRQ delivery for the actual CD timer
 * functions. Physical peripheral latency and RTX scheduling are outside the model. */
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "timer-config.h"

enum Register { CR1, ARR, CNT, SR, PSC, EGR, DIER, REGISTERS };
enum Bank { CD_TIM2, CD_TIM6 };
enum Completion { Normal, Missing, Pending, Posted };
static constexpr Bank cd_bank = CD_TIMER_LEGACY ? CD_TIM6 : CD_TIM2;
static constexpr unsigned TIM_CR1_CEN = 1, TIM_CR1_URS = 4, TIM_CR1_OPM = 8;
static constexpr unsigned TIM_SR_UIF = 1, TIM_EGR_UG = 1, TIM_DIER_UIE = 1;
static constexpr unsigned RCC_APB1LRSTR_TIM2RST = 1, RCC_APB1LRSTR_TIM6RST = 16;
static constexpr int TIM2_IRQn = 28, TIM6_DAC_IRQn = 54;
static uint32_t regs[2][REGISTERS], active_psc[2];
static bool enabled[2], pending[2], masked, barrier;
static uint32_t apb_reset = 0x80000000;
static unsigned resets, releases, software_irqs, tokens, drains;
static uint64_t elapsed_cycles;
static constexpr uint64_t cycles_per_tick = 200000000u / OS_TICK_FREQ;
static uint64_t completion_at, pause_at = UINT64_MAX, hint_at = UINT64_MAX, hint_period;
static uint64_t tick_phase;
static uint32_t tick_base;
static unsigned hint_count;
static bool deliver_late_irq;
static int pause_kind;
static struct { int pause_request; } cdb;
static Completion completion;

#if CD_TIMER_LEGACY
void TIM6_DAC_IRQHandler(void);
#define CD_IRQ TIM6_DAC_IRQHandler
#else
void TIM2_IRQHandler(void);
#define CD_IRQ TIM2_IRQHandler
#endif

static void dispatch(Bank bank)
{
    if (!masked && enabled[bank] && pending[bank]) {
        assert(bank == cd_bank);
        pending[bank] = false;
        CD_IRQ();
    }
}

static void update(Bank bank, bool software, bool deliver = true)
{
    auto &r = regs[bank];
    r[CNT] = 0;
    if (r[CR1] & 2u) return; // UDIS
    active_psc[bank] = r[PSC];
    if (software && (r[CR1] & TIM_CR1_URS)) return;
    if (!software && (r[CR1] & TIM_CR1_OPM)) r[CR1] &= ~TIM_CR1_CEN;
    r[SR] |= TIM_SR_UIF;
    if (r[DIER] & TIM_DIER_UIE) {
        pending[bank] = true;
        software_irqs += software;
        if (deliver) dispatch(bank);
    }
}

struct TimerRegister {
    Bank bank;
    Register reg;
    operator uint32_t() const { return regs[bank][reg]; }
    void operator=(uint32_t value) const {
        auto &r = regs[bank];
        if (reg == ARR || reg == EGR) assert(!(r[CR1] & TIM_CR1_CEN));
        if (reg == EGR) {
            if (value & TIM_EGR_UG) update(bank, true);
        } else if (reg == SR) {
            r[SR] &= value;
            barrier = false;
        } else {
            r[reg] = value & (bank == CD_TIM2 && (reg == ARR || reg == CNT)
                              ? UINT32_MAX : UINT16_MAX);
            if (reg == CR1 && (value & TIM_CR1_CEN)) {
                completion_at = elapsed_cycles +
                    (uint64_t(r[ARR]) + 1 - r[CNT]) * (active_psc[bank] + 1);
            }
        }
    }
};
struct Timer {
    TimerRegister CR1, ARR, CNT, SR, PSC, EGR, DIER;
};
static Timer timer2{{CD_TIM2, CR1}, {CD_TIM2, ARR}, {CD_TIM2, CNT}, {CD_TIM2, SR},
                    {CD_TIM2, PSC}, {CD_TIM2, EGR}, {CD_TIM2, DIER}};
static Timer timer6{{CD_TIM6, CR1}, {CD_TIM6, ARR}, {CD_TIM6, CNT}, {CD_TIM6, SR},
                    {CD_TIM6, PSC}, {CD_TIM6, EGR}, {CD_TIM6, DIER}};
#define TIM2 (&timer2)
#define TIM6 (&timer6)

struct ResetRegister {
    operator uint32_t() const { return apb_reset; }
    void operator=(uint32_t value) const {
        const unsigned mask = RCC_APB1LRSTR_TIM2RST | RCC_APB1LRSTR_TIM6RST;
        assert((value & ~mask) == 0x80000000u);
        apb_reset = value;
        resets |= value & mask;
        for (Bank bank : {CD_TIM2, CD_TIM6}) {
            if (!(value & (bank == CD_TIM2 ? 1u : 16u))) continue;
            std::memset(regs[bank], 0, sizeof regs[bank]);
            active_psc[bank] = 0;
        }
    }
};
static struct { ResetRegister APB1LRSTR; } rcc;
#define RCC (&rcc)

using osSemaphoreId_t = unsigned;
using osMutexId_t = unsigned;
static constexpr uint32_t osWaitForever = UINT32_MAX;
static constexpr int osOK = 0, osErrorTimeout = -2;
static unsigned sem_cd_timer, sem_wait_irq, sem_wait_disc, sem_wait_pause, mutex_fs;
#define sem_tim6 sem_cd_timer
static unsigned allocation_failure, allocations, live_semaphores;
static unsigned worker_tokens;

static unsigned timer_semaphore()
{
    return CD_TIMER_SHARED ? sem_wait_disc : sem_cd_timer;
}

static uint32_t osKernelGetTickCount()
{
    return tick_base + uint32_t((elapsed_cycles + tick_phase) / cycles_per_tick);
}

static unsigned osSemaphoreNew(unsigned maximum, unsigned initial, void *attributes)
{
    assert(maximum == 1 && !initial && !attributes);
    if (++allocations == allocation_failure) return 0;
    live_semaphores |= 1u << allocations;
    return allocations;
}

static void osSemaphoreDelete(unsigned id)
{
    assert(live_semaphores & (1u << id));
    live_semaphores &= ~(1u << id);
}

static unsigned osMutexNew(void *attributes)
{
    assert(!attributes && allocations == (CD_TIMER_SHARED ? 3u : 4u));
    return ++allocations == allocation_failure ? 0 : allocations;
}

static int osSemaphoreRelease(unsigned id)
{
    assert(id && (id == timer_semaphore() || id == sem_wait_disc));
    auto &ready = id == timer_semaphore() ? tokens : worker_tokens;
    if (ready) return -1;
    ++releases;
    ++ready;
    return osOK;
}

static void disk_task_wakeup() { osSemaphoreRelease(sem_wait_disc); }

static int osSemaphoreAcquire(unsigned id, uint32_t timeout)
{
    assert(id && id == timer_semaphore());
    assert(BOOT_APB1_CLOCKS & (cd_bank == CD_TIM2 ? 1u : 16u));
    auto &r = regs[cd_bank];
    if (deliver_late_irq) {
        assert(pending[cd_bank] && (r[CR1] & TIM_CR1_CEN));
        dispatch(cd_bank);
        deliver_late_irq = false;
    }
    if (!timeout && !CD_TIMER_SHARED) {
        assert(!pending[cd_bank] && !(r[SR] & TIM_SR_UIF) && !(r[CR1] & TIM_CR1_CEN));
        ++drains;
        if (!tokens) return osErrorTimeout;
    }
    uint64_t deadline = elapsed_cycles;
    if (timeout == osWaitForever) deadline = UINT64_MAX;
    else if (timeout) {
        deadline += uint64_t(timeout) * cycles_per_tick -
                    (elapsed_cycles + tick_phase) % cycles_per_tick;
    }
    for (;;) {
        // Posted models a completion delivered just as the wait expires.
        if (tokens && (!timeout || completion != Posted)) {
            --tokens;
            return osOK;
        }
        if (elapsed_cycles == deadline) return osErrorTimeout;
        uint64_t timer = (r[CR1] & TIM_CR1_CEN) && completion != Missing
                        ? completion_at : UINT64_MAX;
        auto next = std::min({deadline, timer, pause_at, hint_at});
        assert(next >= elapsed_cycles && next != UINT64_MAX);
        elapsed_cycles = next;
        if (next == timer) {
            assert(enabled[cd_bank] && r[ARR]);
            update(cd_bank, false, completion != Pending);
        }
        if (next == pause_at) {
            cdb.pause_request = pause_kind;
            pause_at = UINT64_MAX;
            disk_task_wakeup();
        }
        if (next == hint_at) {
            ++hint_count;
            hint_at = hint_period ? hint_at + hint_period : UINT64_MAX;
            disk_task_wakeup();
        }
    }
}

static int disable_irq()
{
    bool previous = masked;
    masked = true;
    barrier = false;
    return previous;
}

static void __DSB() { barrier = true; }

static void NVIC_ClearPendingIRQ(int irq)
{
    assert(irq == TIM2_IRQn);
    if (masked) assert(barrier && !regs[CD_TIM2][SR]);
    pending[CD_TIM2] = false;
}

static void restore_irq(int previous)
{
    assert(masked && barrier);
    masked = previous;
    dispatch(CD_TIM2);
}

static void NVIC_EnableIRQ(int irq)
{
    assert(timer_semaphore() && (irq == TIM2_IRQn || irq == TIM6_DAC_IRQn));
    Bank bank = irq == TIM2_IRQn ? CD_TIM2 : CD_TIM6;
    enabled[bank] = true;
    dispatch(bank);
}

#include "timer-functions.inc"

static int configure()
{
#if CD_TIMER_LEGACY
    tim6_init();
    legacy_sync_init();
#else
    if (cd_sync_init()) return -1;
    cd_timer_init();
#endif
    return 0;
}

static int wait(int us)
{
#if CD_TIMER_LEGACY
    hw_delay(us);
    return 0;
#else
    return hw_delay(us);
#endif
}

static void interval(int us)
{
    uint64_t ticks = us > 0 ? (uint64_t(us) + 9) / 10 : 2;
    uint64_t before = elapsed_cycles;
    unsigned old_releases = releases;
    assert(wait(us) == 0);
    assert(elapsed_cycles - before == (ticks + 1) * 2000);
    assert(releases == old_releases + 1 && !software_irqs && !tokens);
    assert(!regs[cd_bank][CNT] && !regs[cd_bank][SR] && !(regs[cd_bank][CR1] & TIM_CR1_CEN));
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    const char *test = argv[1];
    if (!std::strcmp(test, "allocation")) {
        assert(argc == 3);
        allocation_failure = unsigned(std::strtoul(argv[2], nullptr, 10));
        assert(configure() == -1 && allocations == allocation_failure);
        assert(!live_semaphores && !sem_cd_timer && !mutex_fs);
        assert(!sem_wait_irq && !sem_wait_disc && !sem_wait_pause);
        assert(!enabled[CD_TIM2] && !enabled[CD_TIM6] && !resets);
    } else {
        unsigned inherited = !std::strcmp(test, "first") ? unsigned(std::atoi(argv[2])) : 0;
        for (Bank bank : {CD_TIM2, CD_TIM6}) {
            regs[bank][PSC] = active_psc[bank] = inherited;
            regs[bank][CNT] = 371;
        }
        assert(configure() == 0);
        if (!std::strcmp(test, "first")) {
            interval(10000);
        } else if (!std::strcmp(test, "ordinary")) {
            assert(wait(10000) == 0);
            for (int us : {0, 1, 9, 10, 11, 10000, 13500, 100000, 655350}) interval(us);
        } else if (!std::strcmp(test, "long")) {
            assert(wait(10000) == 0);
            interval(655351);
            interval(900000);
        } else if (!std::strcmp(test, "limit")) {
            assert(wait(10000) == 0);
            interval(INT_MAX);
        } else if (!std::strcmp(test, "ownership")) {
            assert(resets == 17u && apb_reset == 0x80000000u);
            assert(!regs[CD_TIM6][CR1] && !regs[CD_TIM6][DIER] && !enabled[CD_TIM6]);
            assert(enabled[CD_TIM2] && regs[CD_TIM2][CR1] == (TIM_CR1_OPM | TIM_CR1_URS));
            assert(allocations == (CD_TIMER_SHARED ? 4u : 5u) && mutex_fs == allocations);
            assert(live_semaphores == (CD_TIMER_SHARED ? 14u : 30u));
            assert(sem_wait_irq == 1 && sem_wait_disc == 2 && sem_wait_pause == 3);
            assert(sem_cd_timer == (CD_TIMER_SHARED ? 0u : 4u));
        } else if (!std::strcmp(test, "stale")) {
            CD_IRQ();
            assert(!releases && !tokens);
            interval(10000);
        } else if (!std::strcmp(test, "timeout")) {
            assert(argc == 3);
            completion = !std::strcmp(argv[2], "missing") ? Missing :
                         !std::strcmp(argv[2], "pending") ? Pending : Posted;
            assert(wait(13500) == -1);
            assert(!masked && !pending[cd_bank] && drains == (CD_TIMER_SHARED ? 0u : 1u));
            assert(tokens == unsigned(CD_TIMER_SHARED && completion == Posted));
            assert(releases == unsigned(completion == Posted));
            completion = Normal;
            interval(13500);
        } else if (!std::strcmp(test, "pause")) {
            assert(argc == 3);
            pause_kind = std::atoi(argv[2]);
            pause_at = cycles_per_tick * 3 + 1;
            uint64_t expected = pause_at;
            assert(wait(INT_MAX) == 0);
            assert(elapsed_cycles == expected && cdb.pause_request == pause_kind);
            assert(!(regs[cd_bank][CR1] & TIM_CR1_CEN) && !tokens);
            // A request already pending must leave the timer unarmed.
            const auto previous_period = completion_at;
            assert(wait(13500) == 0 && completion_at == previous_period);
            cdb.pause_request = 0;
            interval(13500);
        } else if (!std::strcmp(test, "hints")) {
            disk_task_wakeup();
            hint_at = 3000;
            hint_period = 500000;
            auto before = elapsed_cycles;
            assert(wait(13500) == 0 && elapsed_cycles - before == 2702000);
            assert(hint_count == 6 && !tokens && !(regs[cd_bank][CR1] & TIM_CR1_CEN));
            hint_at = UINT64_MAX;
            interval(10000);
            if (CD_TIMER_SHARED) {
                completion = Pending;
                pause_kind = 1;
                pause_at = elapsed_cycles + 2002001;
                assert(wait(10000) == 0 && pending[cd_bank] && cdb.pause_request);
                cdb.pause_request = 0;
                completion = Normal;
                deliver_late_irq = true;
                before = elapsed_cycles;
                assert(wait(10000) == 0 && elapsed_cycles - before == 2002000);
                assert(!pending[cd_bank] && !tokens && !deliver_late_irq);
            }
        } else if (!std::strcmp(test, "deadline")) {
            tick_base = UINT32_MAX - 1;
            tick_phase = cycles_per_tick - 5;
            hint_at = 1;
            hint_period = cycles_per_tick / 3;
            completion = Missing;
            assert(wait(13500) == -1);
            assert(elapsed_cycles == cycles_per_tick * 4 - tick_phase);
            assert(hint_count >= 9 && osKernelGetTickCount() == 2);
            assert(!tokens && !(regs[cd_bank][CR1] & TIM_CR1_CEN));
            hint_at = UINT64_MAX;
            completion = Normal;
            interval(13500);
        } else {
            std::abort();
        }
    }
    std::puts("pass: CD pacing");
}
