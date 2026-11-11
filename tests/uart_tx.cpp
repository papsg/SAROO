/* Adapted from paps-g acb824af. Model DMA, the UART FIFO, and CPU events independently. */
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <string_view>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using osMutexId_t = unsigned;

[[maybe_unused]] constexpr u32
    DMA_SxCR_EN = 1, DMA_SxCR_TEIE = 4, DMA_SxCR_TCIE = 0x10,
    DMA_SxCR_DIR_0 = 0x40, DMA_SxCR_MINC = 0x400, DMA_SxCR_TRBUFF = 0x100000,
    DMA_LISR_FEIF0 = 1, DMA_LISR_DMEIF0 = 4, DMA_LISR_TEIF0 = 8,
    DMA_LISR_HTIF0 = 0x10, DMA_LISR_TCIF0 = 0x20,
    USART_ICR_TCCF = 0x40, USART_CR1_TCIE = 0x40, USART_CR3_DMAT = 0x80,
    TIM_CR1_CEN = 1, TIM_CR1_URS = 4, TIM_CR1_OPM = 8, TIM_SR_UIF = 1,
    TIM_EGR_UG = 1, TIM_DIER_UIE = 1, RCC_APB1LENR_TIM4EN = 4,
    UART4_IRQn = 52, DMA1_Stream0_IRQn = 11, TIM4_IRQn = 30,
    osWaitForever = ~0u;
[[maybe_unused]] constexpr u32 uart_rx_ready = 0x20, uart_tx_complete = 0x40, uart_tx_ready = 0x80;
[[maybe_unused]] constexpr unsigned fifo_capacity = 16, retry_ticks = 1000;

struct Register {
    uintptr_t value = 0;
    operator uintptr_t() const;
    Register &operator=(uintptr_t next);
    Register &operator|=(uintptr_t bits) { return *this = value | bits; }
    Register &operator&=(uintptr_t bits) { return *this = value & bits; }
};

static struct { Register CR, NDTR, PAR, M0AR, FCR; } stream;
static struct { Register LISR, LIFCR; } dma;
static struct { Register CR1, CR2, CR3, BRR, RQR, ICR, ISR, TDR, RDR; } uart;
static struct { Register CR1, CNT, SR, PSC, ARR, EGR, DIER; } timer;
[[maybe_unused]] static struct { Register APB1LENR; } rcc;
static struct { Register CCR; } mux;

#define DMA1_Stream0 (&stream)
#define DMA1 (&dma)
#define UART4 (&uart)
#define TIM4 (&timer)
#define RCC (&rcc)
#define DMAMUX1_Channel0 (&mux)

static std::array<u8, 4096> memory, dma_visible;
static std::array<bool, 64> irq_enabled;
static std::array<unsigned, 64> irq_priority;
static std::deque<u8> fifo;
static std::string output, expected;
static uintptr_t cursor;
static u8 prefetched;
static unsigned irq_mask, reads, delays, irq_calls, timer_irqs, direct_writes;
static unsigned enables, bad_mode_enables, timer_left, accept_budget, skip_copies;
static unsigned masked_copies, flags_after_read;
static bool in_interrupt, refuse_enable, hold_stop, stall_source, persistent_error;
static bool feeding, has_prefetch;
static void (*copy_hook)();

static void update_uart_ready()
{
    if (fifo.size() < fifo_capacity)
        uart.ISR.value |= uart_tx_ready;
    else
        uart.ISR.value &= ~uintptr_t(uart_tx_ready);
}

static void preload()
{
    if (has_prefetch || stall_source || !(stream.CR.value & DMA_SxCR_EN))
        return;
    if (persistent_error) {
        stream.CR.value &= ~uintptr_t(DMA_SxCR_EN);
        dma.LISR.value |= DMA_LISR_TEIF0;
        return;
    }
    auto first = reinterpret_cast<uintptr_t>(memory.data());
    assert(cursor >= first && cursor < first + memory.size());
    prefetched = dma_visible[cursor - first];
    ++cursor;
    has_prefetch = true;
}

static void feed()
{
    if (feeding || !(stream.CR.value & DMA_SxCR_EN) ||
        !(uart.CR3.value & USART_CR3_DMAT) || !(uart.CR1.value & 1))
        return;
    feeding = true;
    while (fifo.size() < fifo_capacity && (stream.CR.value & DMA_SxCR_EN) && accept_budget) {
        preload();
        if (!has_prefetch) {
            if (stream.CR.value & DMA_SxCR_EN)
                dma.LISR.value |= DMA_LISR_FEIF0;
            break;
        }
        fifo.push_back(prefetched);
        has_prefetch = false;
        uart.ISR.value &= ~uintptr_t(uart_tx_complete);
        assert(stream.NDTR.value);
        --stream.NDTR.value;
        --accept_budget;
        if (stream.NDTR.value == 0) {
            dma.LISR.value |= DMA_LISR_TCIF0;
            stream.CR.value &= ~uintptr_t(DMA_SxCR_EN);
        } else {
            preload(); // A read ahead is not an accepted peripheral write.
        }
    }
    update_uart_ready();
    feeding = false;
}

Register::operator uintptr_t() const
{
    assert(++reads < 8192 && "unbounded register polling");
    auto snapshot = value;
    if (this == &dma.LISR && flags_after_read) {
        dma.LISR.value |= flags_after_read;
        flags_after_read = 0;
    }
    return snapshot;
}

Register &Register::operator=(uintptr_t next)
{
    auto old = value;
    if (this == &stream.CR && (old & DMA_SxCR_EN) && !(next & DMA_SxCR_EN) && hold_stop) {
        value = next | DMA_SxCR_EN;
        return *this;
    }
    if (this == &stream.NDTR || this == &stream.PAR || this == &stream.M0AR || this == &stream.FCR)
        assert(!(stream.CR.value & DMA_SxCR_EN) && "DMA register write before stop acknowledgment");
    value = next;
    if (this == &stream.CR && (next & DMA_SxCR_EN) && !(old & DMA_SxCR_EN)) {
        ++enables;
        if (!(next & DMA_SxCR_TRBUFF))
            ++bad_mode_enables;
        if (refuse_enable) {
            value &= ~uintptr_t(DMA_SxCR_EN);
            return *this;
        }
        cursor = stream.M0AR.value;
        has_prefetch = false;
        feed();
    } else if (this == &dma.LIFCR) {
        assert(!(next & ~uintptr_t(0x3d)) && "cleared flags outside DMA1 Stream 0");
        dma.LISR.value &= ~next;
    } else if (this == &uart.ICR) {
        uart.ISR.value &= ~next;
    } else if (this == &uart.CR1 && !(next & 1)) {
        assert(!(stream.CR.value & DMA_SxCR_EN) && "UART disabled while DMA still owned it");
        assert(fifo.empty() && "recovery discarded accepted UART bytes");
    } else if (this == &uart.CR3 && (next & USART_CR3_DMAT)) {
        feed();
    } else if (this == &uart.TDR) {
        ++direct_writes;
        assert(fifo.size() < fifo_capacity);
        fifo.push_back(u8(next));
        update_uart_ready();
    } else if (this == &timer.CR1) {
        timer_left = (next & TIM_CR1_CEN) ? unsigned(timer.ARR.value + 1) : 0;
    }
    return *this;
}

static unsigned disable_irq()
{
    unsigned saved = irq_mask;
    irq_mask = 1;
    return saved;
}

static void restore_irq(unsigned saved) { irq_mask = saved; }
static bool in_isr() { return in_interrupt; }
static osMutexId_t osMutexNew(void *) { return 1; }
static void osMutexAcquire(osMutexId_t, unsigned) {}
static void osMutexRelease(osMutexId_t) {}
static void osDelay(int) { assert(++delays < 16 && "log producer waited indefinitely"); }
static void NVIC_EnableIRQ(unsigned irq) { irq_enabled[irq] = true; }
static void NVIC_DisableIRQ(unsigned irq) { irq_enabled[irq] = false; }
static void NVIC_SetPriority(unsigned irq, unsigned priority) { irq_priority[irq] = priority; }
static void __DMB() { dma_visible = memory; }
static int sprintk(char *dest, const char *, ...)
{
    std::strcpy(dest, "retry");
    return 5;
}
void _puts(char *);

static void model_copy(void *dest, const void *src, size_t count)
{
    if (count && irq_mask)
        ++masked_copies;
    if (copy_hook && count) {
        if (skip_copies)
            --skip_copies;
        else {
            auto callback = copy_hook;
            copy_hook = nullptr;
            callback();
        }
    }
    std::memcpy(dest, src, count);
}

#define memcpy model_copy
#include "uart.inc"
#undef memcpy

static void deliver_irqs()
{
    if (irq_mask || in_interrupt)
        return;
    in_interrupt = true;
    for (unsigned pass = 0; pass < 8; ++pass) {
        bool serviced = false;
        reads = 0;
#if UART_BOUNDED_TX && !defined(BOOT)
        if (irq_enabled[DMA1_Stream0_IRQn] &&
            (((dma.LISR.value & DMA_LISR_TEIF0) && (stream.CR.value & DMA_SxCR_TEIE)) ||
             ((dma.LISR.value & DMA_LISR_TCIF0) && (stream.CR.value & DMA_SxCR_TCIE)))) {
            ++irq_calls;
            DMA1_Stream0_IRQHandler();
            serviced = true;
        }
        if (irq_enabled[TIM4_IRQn] && (timer.SR.value & TIM_SR_UIF) && (timer.DIER.value & TIM_DIER_UIE)) {
            ++irq_calls;
            ++timer_irqs;
            TIM4_IRQHandler();
            serviced = true;
        }
#elif !UART_BOUNDED_TX
        if (irq_enabled[UART4_IRQn] && (uart.ISR.value & uart_tx_complete) && (uart.CR1.value & USART_CR1_TCIE)) {
            ++irq_calls;
            UART4_IRQHandler();
            serviced = true;
        }
#endif
        if (!serviced) {
            in_interrupt = false;
            return;
        }
    }
    assert(false && "interrupt service did not quiesce");
}

static void step(unsigned count = 1, bool deliver = true)
{
    for (unsigned i = 0; i < count; ++i) {
        if (!fifo.empty()) {
            output.push_back(char(fifo.front()));
            fifo.pop_front();
            if (fifo.empty())
                uart.ISR.value |= uart_tx_complete;
        }
        update_uart_ready();
        feed();
        if ((timer.CR1.value & TIM_CR1_CEN) && --timer_left == 0) {
            timer.CR1.value &= ~uintptr_t(TIM_CR1_CEN);
            timer.SR.value |= TIM_SR_UIF;
        }
        if (deliver)
            deliver_irqs();
    }
}

static void reset(bool retain_stream = false)
{
    irq_mask = reads = delays = irq_calls = timer_irqs = direct_writes = 0;
    enables = bad_mode_enables = masked_copies = flags_after_read = 0;
    in_interrupt = refuse_enable = stall_source = persistent_error = feeding = has_prefetch = false;
    hold_stop = retain_stream;
    cursor = timer_left = skip_copies = 0;
    copy_hook = nullptr;
    accept_budget = ~0u;
    stream = {};
    dma = {};
    uart = {};
    timer = {};
    rcc = {};
    mux = {};
    irq_enabled.fill(false);
    irq_priority.fill(~0u);
    memory.fill(0xa5);
    dma_visible = memory;
    fifo.clear();
    output.clear();
    expected.clear();
    update_uart_ready();
    if (retain_stream)
        stream.CR.value = DMA_SxCR_EN;
    uart4_init();
}

#ifndef BOOT
static void send(std::string message, bool admitted = true)
{
    reads = 0;
    if (admitted)
        expected += message;
    uart4_puts(message.data());
}

static void drain()
{
    for (unsigned i = 0; i < 30000; ++i) {
        step();
        if (output.size() >= expected.size() && fifo.empty() &&
            !(stream.CR.value & DMA_SxCR_EN) && !(timer.CR1.value & TIM_CR1_CEN))
            break;
    }
    assert(output == expected && "the admitted byte stream was reordered, lost, or replayed");
    assert(fifo.empty() && !(stream.CR.value & DMA_SxCR_EN));
}

static void fault()
{
    assert(stream.CR.value & DMA_SxCR_EN);
    stream.CR.value &= ~uintptr_t(DMA_SxCR_EN);
    has_prefetch = false;
    dma.LISR.value |= DMA_LISR_TEIF0;
    deliver_irqs();
}

static void nested_writer()
{
    in_interrupt = true;
    send(std::string(77, 'B'));
    in_interrupt = false;
    step(30);
}

static void rejected_writer()
{
    in_interrupt = true;
    send(std::string(77, 'B'), false);
    in_interrupt = false;
}

static std::string pattern(unsigned count, unsigned seed)
{
    std::string message(count, ' ');
    for (unsigned i = 0; i < count; ++i)
        message[i] = char(33 + (seed + i * 17) % 90);
    return message;
}

static void app_case(std::string_view test)
{
    reset(test == "initial_stop");
    if (test == "healthy") {
        for (unsigned i = 0; i < 100; ++i) {
            send(pattern(211, i));
            step(211);
        }
        drain();
        assert(!masked_copies && !delays && !direct_writes && !timer_irqs);
    } else if (test == "mode") {
        send("uart");
        drain();
        assert(enables == 1 && !bad_mode_enables);
    } else if (test == "uart_tc") {
        send(pattern(300, 1));
        stall_source = true;
        step(25);
        assert((stream.CR.value & DMA_SxCR_EN) && stream.NDTR.value);
        stall_source = false;
        drain();
    } else if (test == "full") {
        stall_source = true;
        send(pattern(4095, 4));
        send("full", false);
        stall_source = false;
        drain();
        send(pattern(4096, 7), false);
        send("valid after overload");
        drain();
        assert(!delays);
    } else if (test == "wrap") {
        send(pattern(4000, 2));
        drain();
        send(std::string(50, 'X'));
        copy_hook = [] { step(500); };
        skip_copies = 1;
        send(std::string(180, 'Y'));
        drain();
    } else if (test == "nested") {
        copy_hook = nested_writer;
        send(std::string(100, 'A'));
        drain();
    } else if (test == "reserve") {
        copy_hook = rejected_writer;
        send(std::string(4070, 'A'));
        drain();
    } else if (test == "partial") {
        for (unsigned left = 1; left <= 300; ++left) {
            reset();
            stall_source = true;
            send(pattern(300, left));
            send(pattern(71, left + 3));
            stall_source = false;
            accept_budget = 300 - left;
            feed();
            step(300 - left);
            assert(stream.NDTR.value == left);
            fault();
            accept_budget = ~0u;
            drain();
        }
    } else if (test == "wrap_residual") {
        send(pattern(4000, 8));
        drain();
        send(pattern(180, 12));
        step(30);
        assert(stream.NDTR.value == 50);
        fault();
        drain();
    } else if (test == "fault_backoff") {
        send(pattern(300, 11));
        step(31);
        persistent_error = true;
        fault();
        unsigned before = irq_calls;
        step(5100);
        assert(timer_irqs == 5 && irq_calls - before <= 10);
        persistent_error = false;
        drain();
    } else if (test == "refused") {
        refuse_enable = true;
        send(pattern(80, 2));
        step(2 * retry_ticks + 500);
        assert(enables == 3 && timer_irqs == 2 && !direct_writes && !delays);
        refuse_enable = false;
        drain();
        assert(!bad_mode_enables);
    } else if (test == "initial_stop") {
        send("waiting for retained DMA to stop");
        step(2 * retry_ticks + 500);
        assert(timer_irqs == 2 && !(uart.CR1.value & 1) && output.empty());
        hold_stop = false;
        stream.CR.value &= ~uintptr_t(DMA_SxCR_EN);
        drain();
    } else if (test == "masked") {
        irq_mask = 1;
        send("masked caller");
        assert(irq_mask == 1 && masked_copies == 1);
        irq_mask = 0;
        drain();
    } else if (test == "feif") {
        stall_source = true;
        send(pattern(100, 7));
        assert(dma.LISR.value & DMA_LISR_FEIF0);
        step(100);
        assert((stream.CR.value & DMA_SxCR_EN) && !irq_calls);
        stall_source = false;
        drain();
    } else if (test == "coalesced") {
        send(pattern(80, 4));
        send(pattern(50, 8));
        step(100, false);
        assert(dma.LISR.value & DMA_LISR_TCIF0);
        dma.LISR.value |= DMA_LISR_TEIF0;
        deliver_irqs();
        assert(timer.CR1.value & TIM_CR1_CEN);
        drain();
    } else if (test == "pending_timer") {
        refuse_enable = true;
        send(pattern(100, 7));
        refuse_enable = false;
        step(retry_ticks, false);
        assert(timer.SR.value & TIM_SR_UIF);
        send(pattern(100, 9));
        assert(enables == 1 && !(stream.CR.value & DMA_SxCR_EN));
        deliver_irqs();
        drain();
    } else if (test == "late_irq") {
        send(pattern(300, 9));
        step(45);
        stall_source = true;
        dma.LISR.value |= DMA_LISR_TEIF0;
        flags_after_read = DMA_LISR_TCIF0;
#if UART_BOUNDED_TX
        DMA1_Stream0_IRQHandler();
        assert(dma.LISR.value & DMA_LISR_TCIF0);
        step(retry_ticks);
        assert(stream.CR.value & DMA_SxCR_EN);
        stream.CR.value &= ~uintptr_t(DMA_SxCR_EN);
        has_prefetch = false;
        stall_source = false;
        drain();
#else
        hold_stop = true;
        uart.ISR.value |= uart_tx_complete;
        deliver_irqs();
#endif
    } else if (test == "stale_timer") {
        send(pattern(180, 3));
#if UART_BOUNDED_TX
        TIM4_IRQHandler();
        assert(!(timer.CR1.value & TIM_CR1_CEN));
#endif
        drain();
    } else if (test == "init_and_empty") {
        send("");
        assert(!enables && !direct_writes && !masked_copies);
        assert(mux.CCR.value == 64 && uart.BRR.value == 100);
#if UART_BOUNDED_TX
        assert((timer.PSC.value + 1) * (timer.ARR.value + 1) == 2000000);
        assert(irq_enabled[DMA1_Stream0_IRQn] && irq_enabled[TIM4_IRQn] && !irq_enabled[UART4_IRQn]);
        assert(irq_priority[DMA1_Stream0_IRQn] == irq_priority[TIM4_IRQn]);
#endif
    } else {
        assert(false && "unknown UART case");
    }
}
#endif

int main(int argc, char **argv)
{
    assert(argc == 2);
#ifdef BOOT
    assert(std::string_view(argv[1]) == "boot");
    reset();
    assert(uart.BRR.value == 100 && uart.CR1.value == 0x2000000d &&
           uart.CR3.value == 0x64000000 && mux.CCR.value == 64);
    assert(!irq_enabled[DMA1_Stream0_IRQn] && !irq_enabled[TIM4_IRQn] && !timer.CR1.value);
    uart.RDR.value = 'q';
    uart.ISR.value |= uart_rx_ready;
    assert(_getc() == 'q' && _getc_tmout(1) == 'q');
    uart.ISR.value &= ~uintptr_t(uart_rx_ready);
    assert(_getc_tmout(0) == -1);
    char text[] = "ok\n";
    _puts(text);
    _putc('!');
    step(10);
    assert(output == "ok\r\n!" && !enables && !delays);
#else
    app_case(argv[1]);
#endif
    std::puts("pass");
}
