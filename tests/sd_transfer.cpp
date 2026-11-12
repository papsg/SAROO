/* Adapted from paps-g 433866e8. Actual SAROO C with a host SDMMC event model. */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "RTX_Config.h"
#include "sd_model.h"

using u32 = uint32_t;
using u8 = uint8_t;
enum { STA, MASK, CMD, ARG, DCTRL, IDMACTRL, ICR, CLKCR, POWER, DTIMER, DLEN, BASE, RESET };
struct Register {
    u32 value = 0;
    int id = -1;
    operator u32() const;
    Register &operator=(u32 v);
    Register &operator|=(u32 v) { return *this = value | v; }
    Register &operator&=(u32 v) { return *this = value & v; }
};
struct Controller {
    Register STA, MASK, CMD, ARG, DCTRL, IDMACTRL, ICR, CLKCR, POWER, DTIMER, DLEN, IDMABASE0;
    u32 RESP1 = 0, RESP2 = 0, RESP3 = 0, RESP4 = 0;
} controller;
struct { Register AHB3RSTR; } rcc;
#define SDIO (&controller)
#define RCC (&rcc)
static u32 cmd_resp[4];
static u32 sd_rca;
static int is_sdhc;
static osSemaphoreId_t cmd_sem = (void *)1;
static void SDIO_IRQHandler(void);
static unsigned reads, tokens, releases, drains, waits, delays, resets, barriers, clock_reads;
static unsigned mmio_writes;
static u32 tick, issued, fault_status;
static unsigned current, fault_command, fault_uses, stop_before_reset, stop_after_reset;
static bool in_irq, faulty, awaiting, late_irq, pending_irq, busy_error, bad_delay;
static bool permanent_cpsm, healthy_busy, pending_start;
static unsigned reset_hold, identified, irq_enables;
static bool fail_semaphore_creation;
static int identify_result;
struct { u32 IDR; } gpio;
#define GPIOA (&gpio)
static constexpr int SDIO_IRQn = 49;
static void NVIC_SetPriority(int irq, int priority) { assert(irq == SDIO_IRQn && priority == 6); }
static void NVIC_EnableIRQ(int irq) { assert(irq == SDIO_IRQn); irq_enables++; }
static osStatus_t wait_failure;
static std::vector<unsigned> commands;
static std::vector<u32> arguments;
static constexpr u32 active = SDMMC_STA_CPSMACT | SDMMC_STA_DPSMACT;
static constexpr u32 selected_clock = 0x15004;

static void irq(bool forced = false)
{
    if (!in_irq && (forced || (SDIO->STA.value & SDIO->MASK.value))) {
        in_irq = true;
        SDIO_IRQHandler();
        in_irq = false;
    }
}

Register::operator u32() const
{
    assert(++reads < 100000 && "driver spun without bounded scheduler progress");
    return value;
}

Register &Register::operator=(u32 v)
{
    ++mmio_writes;
    if (id == DCTRL || id == IDMACTRL || id == DLEN || id == BASE)
        assert(!(SDIO->STA.value & SDMMC_STA_DPSMACT) && "data register write while DMA/DPSM owns buffer");
    if (id == ARG || id == CMD)
        assert(!(SDIO->CMD.value & SDMMC_CMD_CPSMEN) && !(SDIO->STA.value & SDMMC_STA_CPSMACT) && "command fields replaced while CPSM active");
    if (id == CLKCR || id == DTIMER) assert(!(SDIO->STA.value & active));
    value = v;
    if (id == ICR) {
        SDIO->STA.value &= ~(v & 0x1fe00fff);
    } else if (id == CMD && (v & SDMMC_CMD_CPSMEN)) {
        current = v & 63;
        commands.push_back(current); arguments.push_back(SDIO->ARG.value);
        issued = tick;
        awaiting = true;
        faulty = current == fault_command && fault_uses++ == 0;
        if (!pending_start || current == 12) SDIO->STA.value |= SDMMC_STA_CPSMACT;
        if (v & SDMMC_CMD_CMDTRANS) SDIO->STA.value |= SDMMC_STA_DPSMACT;
    } else if (id == MASK) {
        if (pending_irq && v && !in_irq) { pending_irq = false; irq(true); }
        irq();
    } else if (id == RESET && (v & 0x10000)) {
        assert(v == (RCC_AHB3RSTR_SDMMC1RST | RCC_AHB3RSTR_MDMARST) && "unrelated reset bit changed");
        resets++;
        SDIO->STA.value = SDIO->MASK.value = SDIO->CMD.value = 0;
        SDIO->DCTRL.value = SDIO->IDMACTRL.value = 0;
        SDIO->CLKCR.value = SDIO->POWER.value = SDIO->DTIMER.value = 0;
        awaiting = false;
    }
    return *this;
}

static void advance(void)
{
    tick++;
    if (awaiting) {
        u32 age = tick - issued;
        if (current == 12 && !faulty) {
            unsigned behavior = resets ? stop_after_reset : stop_before_reset;
            if (behavior == 6) {
                SDIO->STA.value = SDMMC_STA_CMDREND;
                if (age >= 3) {
                    SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
                    awaiting = false;
                }
            } else if (behavior == 4 || behavior == 5) {
                SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
                SDIO->STA.value &= ~active;
                SDIO->STA.value |= SDMMC_STA_CMDREND | SDMMC_STA_BUSYD0;
                if (behavior == 4 && age >= 4) { SDIO->STA.value &= ~SDMMC_STA_BUSYD0; awaiting = false; }
            } else if (behavior != 1) {
                SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
                SDIO->STA.value &= ~SDMMC_STA_CPSMACT;
                SDIO->STA.value |= behavior == 3 ? SDMMC_STA_CTIMEOUT : SDMMC_STA_CMDREND;
                if (behavior != 2 && age >= 3) {
                    SDIO->STA.value &= ~(SDMMC_STA_DPSMACT | SDMMC_STA_BUSYD0);
                    SDIO->STA.value |= SDMMC_STA_DABORT | SDMMC_STA_BUSYD0END;
                    awaiting = false;
                } else if (behavior == 2) SDIO->STA.value |= SDMMC_STA_DPSMACT;
            }
        } else if (faulty) {
            if (!(fault_status & SDMMC_STA_CPSMACT) || (age >= 3 && !permanent_cpsm)) {
                if (wait_failure != osOK && age < 3) return;
                SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
                SDIO->STA.value = fault_status & ~SDMMC_STA_CPSMACT;
                awaiting = false;
            }
        } else if (healthy_busy && current == 7) {
            SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
            SDIO->STA.value = SDMMC_STA_CMDREND | (age < 3 ? SDMMC_STA_BUSYD0 : SDMMC_STA_BUSYD0END);
            if (age >= 3) awaiting = false;
        } else if (busy_error && current == 7) {
            SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
            SDIO->STA.value = SDMMC_STA_CMDREND | SDMMC_STA_DPSMACT | SDMMC_STA_BUSYD0;
            if (age >= 2) SDIO->STA.value |= SDMMC_STA_DTIMEOUT;
        } else {
            SDIO->STA.value = (SDIO->CMD.value & SDMMC_CMD_CMDTRANS) ?
                              SDMMC_STA_DATAEND :
                              (SDIO->CMD.value & 0x300) ? SDMMC_STA_CMDREND : SDMMC_STA_CMDSENT;
            SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
            SDIO->RESP1 = 0x11223344 + current;
            SDIO->RESP2 = 0x55667788; SDIO->RESP3 = 0x99aabbcc; SDIO->RESP4 = 0xddeeff00;
            awaiting = false;
        }
    }
    irq();
}

static void __DSB(void) { barriers++; }
uint32_t osKernelGetTickCount(void) { clock_reads++; return tick; }
osStatus_t osDelay(uint32_t duration)
{
    if (duration == 10) {
        assert(RCC->AHB3RSTR.value & RCC_AHB3RSTR_SDMMC1RST);
        reset_hold += duration; tick += duration; return osOK;
    }
    assert(duration == 1);
    if (bad_delay) return osErrorISR;
    delays++; advance(); return osOK;
}
osStatus_t osSemaphoreRelease(osSemaphoreId_t sem)
{
    if (!sem) return osErrorParameter;
    releases++;
    if (tokens) return osErrorResource;
    tokens = 1; return osOK;
}
osStatus_t osSemaphoreAcquire(osSemaphoreId_t sem, uint32_t timeout)
{
    if (!timeout) {
        drains++;
        assert(SDIO->MASK.value == 0);
        if (!sem) return osErrorParameter;
        if (!tokens) return osErrorResource;
        tokens = 0; return osOK;
    }
    assert(timeout == 100 && "normal command deadline changed");
    assert(!tokens && "old completion accepted by a new command");
    waits++;
    if (!sem) return osErrorParameter;
    if (faulty && wait_failure != osOK) return wait_failure;
    for (unsigned i = 0; i < timeout; i++) {
        advance();
        if (tokens) { tokens = 0; return osOK; }
    }
    return osErrorTimeout;
}
static void printk(const char *, ...)
{
    if (late_irq) {
        late_irq = false;
        SDIO->CMD.value &= ~SDMMC_CMD_CPSMEN;
        SDIO->STA.value = SDMMC_STA_DATAEND;
        awaiting = false;
        irq();
        assert(tokens == 1);
    }
}

static u8 span_data[17*512], span_bounce[8*512];
static u32 span_address;
static u32 model_sd_address(u8 *buf)
{
    return buf == span_data ? span_address : (u32)(uintptr_t)buf;
}

#include "driver.inc"
#include "support.inc"
osSemaphoreId_t osSemaphoreNew(uint32_t maximum, uint32_t initial, const osSemaphoreAttr_t *)
{
    assert(maximum == 1 && initial == 0 && !(SDIO->STA.value & active));
    tokens = 0; return fail_semaphore_creation ? nullptr : (void *)1;
}
static int sdcard_identify(void)
{
    assert(!(SDIO->STA.value & active));
    assert(!SDIO->MASK.value && !SDIO->CMD.value && !SDIO->DCTRL.value && !SDIO->IDMACTRL.value);
    assert(sd_rca == 0 && SDIO->POWER.value == 3);
    identified++; sd_rca = identify_result ? 0 : 0x1234; return identify_result;
}
#include "init.inc"

static void setup(void)
{
    controller = {}; rcc = {};
    Register *regs[] = {&SDIO->STA, &SDIO->MASK, &SDIO->CMD, &SDIO->ARG, &SDIO->DCTRL,
                       &SDIO->IDMACTRL, &SDIO->ICR, &SDIO->CLKCR, &SDIO->POWER,
                       &SDIO->DTIMER, &SDIO->DLEN, &SDIO->IDMABASE0, &RCC->AHB3RSTR};
    for (unsigned i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) regs[i]->id = i;
    sd_rca = 0x1234; is_sdhc = 1; cmd_sem = (void *)1;
    reads = tokens = releases = drains = waits = delays = resets = barriers = clock_reads = mmio_writes = 0;
    tick = issued = fault_status = current = fault_uses = 0;
    fault_command = 99; stop_before_reset = stop_after_reset = 0;
    in_irq = faulty = awaiting = late_irq = pending_irq = busy_error = bad_delay = false;
    wait_failure = osOK;
    permanent_cpsm = healthy_busy = pending_start = fail_semaphore_creation = false;
    reset_hold = identified = irq_enables = 0; identify_result = 0; gpio.IDR = 0;
    commands.clear(); arguments.clear();
    SDIO->CLKCR.value = selected_clock; SDIO->POWER.value = 3; RCC->AHB3RSTR.value = RCC_AHB3RSTR_MDMARST;
    for (unsigned i = 0; i < 4; i++) cmd_resp[i] = 0xa0b0c000 + i;
}
static u8 *const buffer = (u8 *)uintptr_t(0x24002000);
static void clean(void)
{
    assert(!(SDIO->STA.value & active));
    assert(!SDIO->MASK.value && !SDIO->CMD.value && !SDIO->IDMACTRL.value && !SDIO->DCTRL.value);
    assert(!tokens && RCC->AHB3RSTR.value == RCC_AHB3RSTR_MDMARST);
    assert(SDIO->CLKCR.value == selected_clock && SDIO->POWER.value == 3);
}
static void data_fault(u32 flags, osStatus_t wait = osOK)
{
    fault_command = 17; fault_status = flags; wait_failure = wait;
}

/* Adapted from paps-g bf429abf. Exercise the real command setup and the
 * public direct/bounce admissions with synthetic DMA addresses. */
enum SpanPath { Blocks, Axi, Cartridge, UnalignedAxi, UnalignedCartridge, Bounce };
static int span_io(SpanPath path, bool write, bool high, u32 first, int count)
{
    const u32 addresses[] = {0x24002000, 0x24002000, 0x61002000,
                            0x24002001, 0x61002001, 0x20002000};
    setup();
    is_sdhc = high;
    span_address = addresses[path];
    sdbuf = span_bounce;
    std::memset(span_data, 0xa5, sizeof span_data);
    std::memset(span_bounce, 0x59, sizeof span_bounce);
    if (path == Blocks)
        return write ? sd_write_blocks(first,count,span_data) : sd_read_blocks(first,count,span_data);
    return write ? sd_write_sector(first,count,span_data) : sd_read_sector(first,count,span_data);
}

static void reject_span(SpanPath path, bool write, bool high, u32 first, int count)
{
    int result = span_io(path,write,high,first,count);
    assert(!mmio_writes && commands.empty() && "unrepresentable span reached the controller");
    assert(result == -1 && !reads && !waits && !resets);
    for (u8 b : span_data) assert(b == 0xa5);
    for (u8 b : span_bounce) assert(b == 0x59);
    clean();
}

static void accept_span(SpanPath path, bool write, bool high, u32 first, int count)
{
    assert(span_io(path,write,high,first,count) == 0);
    clean();
    size_t i = 0;
    for (int done=0;done<count;) {
        int chunk = count-done;
        if (path >= UnalignedAxi && chunk > 8) chunk = 8;
        if (write && chunk != 1) {
            assert(commands.at(i)==55 && arguments.at(i)==(0x1234u<<16));
            ++i;
            assert(commands.at(i)==23 && arguments.at(i)==u32(chunk));
            ++i;
        }
        u32 block = first+u32(done);
        assert(commands.at(i)==unsigned((write ? 24 : 17)+(chunk != 1)));
        assert(arguments.at(i)==(high ? block : block*512u));
        ++i;
        if (chunk != 1) {
            assert(commands.at(i)==12);
            ++i;
        }
        done += chunk;
    }
    assert(i == commands.size() && mmio_writes && !resets);
}

int main(int argc, char **argv)
{
    assert(argc == 2); int which = std::atoi(argv[1]); setup();
    if (which == 0) {
        for (int n : {1, 2, 8, 65535}) {
            setup(); assert(sd_read_blocks(42, n, buffer) == 0); clean();
            assert(commands == (n == 1 ? std::vector<unsigned>{17} : std::vector<unsigned>{18,12}));
            assert(!delays && !clock_reads && !drains && !resets);
            setup(); assert(sd_write_blocks(42, n, buffer) == 0); clean();
            assert(commands == (n == 1 ? std::vector<unsigned>{24} : std::vector<unsigned>{55,23,25,12}));
            assert(!delays && !clock_reads && !drains && !resets);
        }
        for (u32 cmd : {u32(CMD_00), u32(CMD_08), u32(CMD_02), u32(CMD_41)}) {
            setup(); assert(sd_cmd(cmd, 0x1234) == 0);
            if (RESP_SHORT(cmd)) assert(cmd_resp[0] == 0x11223344 + (cmd & 63));
            if (RESP_LONG(cmd)) assert(cmd_resp[0] == SDIO->RESP4 && cmd_resp[3] == SDIO->RESP1);
        }
    } else if (which == 1) {
        data_fault(SDMMC_STA_DCRCFAIL | SDMMC_STA_DPSMACT);
        assert(sd_read_blocks(42,1,buffer) != 0); clean(); assert(delays >= 3 && !resets && sd_rca);
    } else if (which == 2) {
        data_fault(SDMMC_STA_DPSMACT | SDMMC_STA_CPSMACT, osErrorTimeout);
        assert(sd_read_blocks(42,1,buffer) == -1); clean(); assert(delays >= 6 && !resets);
    } else if (which == 3 || which == 4 || which == 12) {
        data_fault(SDMMC_STA_DCRCFAIL | SDMMC_STA_DPSMACT);
        stop_before_reset = 2;
        if (which == 4) stop_after_reset = 2;
        if (which == 12) tick = UINT32_MAX - 50;
        u32 start = tick;
        assert(sd_read_blocks(42,1,buffer) != 0); clean();
        assert((u32)(tick - start) <= 2 * OS_TICK_FREQ + 10);
        assert(resets == (which == 4 ? 2u : 1u));
        assert((sd_rca != 0) == (which != 4));
        if (which == 4) { unsigned before = commands.size(); assert(sd_read_sector(42,1,buffer) == -1); assert(commands.size() == before); }
    } else if (which == 5) {
        cmd_sem = nullptr;
        assert(sd_read_blocks(42,1,buffer) == -1); clean();
        setup(); bad_delay = true; data_fault(SDMMC_STA_DPSMACT, osErrorParameter);
        assert(sd_read_blocks(42,1,buffer) == -1); clean(); assert(!sd_rca && resets == 2);
    } else if (which == 6) {
        SDIO->STA.value = SDMMC_STA_DATAEND;
        irq(true); assert(!tokens && !releases);
        SDIO->STA.value = SDMMC_STA_DBCKEND; SDIO->MASK.value = SDMMC_STA_CMDREND;
        irq(true); assert(!tokens && !releases);
        SDIO->STA.value = SDMMC_STA_CMDREND; irq(true);
        assert(tokens == 1 && releases == 1 && SDIO->MASK.value == 0);
        irq(true); assert(releases == 1);
    } else if (which == 7) {
        data_fault(SDMMC_STA_DPSMACT, osErrorTimeout); late_irq = true;
        assert(sd_read_blocks(42,1,buffer) == -1); clean(); assert(drains == 1);
        assert(sd_cmd(CMD_08,0x1aa) == 0);
        assert(cmd_resp[0] == 0x11223344 + 8);
    } else if (which == 8) {
        busy_error = true;
        assert(sd_cmd(CMD_07,0) != 0); clean();
        assert(tick < 10 && sd_rca);
    } else if (which == 9) {
        for (unsigned cmd : {55u,23u}) {
            setup(); fault_command = cmd; fault_status = SDMMC_STA_CCRCFAIL;
            assert(sd_write_blocks(42,2,buffer) != 0); clean();
            assert(commands.size() == (cmd == 55 ? 1u : 2u) && !resets && !delays);
        }
    } else if (which == 10) {
        pending_irq = true;
        assert(sd_cmd(CMD_08,0x1aa) == 0 && releases == 1 && !drains);
    } else if (which == 11) {
        data_fault(SDMMC_STA_DATAEND | SDMMC_STA_IDMATE); stop_before_reset = 3;
        assert(sd_read_blocks(42,1,buffer) != 0); clean(); assert(resets == 1 && sd_rca);
        assert(sd_read_blocks(42,1,buffer) == 0); clean();
    } else if (which == 13) {
        data_fault(active, osErrorTimeout); permanent_cpsm = true;
        assert(sd_read_blocks(42,1,buffer) == -1); clean();
        assert(resets == 1 && sd_rca && delays >= OS_TICK_FREQ);
        assert(commands == (std::vector<unsigned>{17,12}));
    } else if (which == 14) {
        healthy_busy = true;
        assert(sd_cmd(CMD_07,0) == 0); clean();
        assert(tick == 3 && waits == 1 && releases == 1);
        assert(!drains && !delays && !clock_reads && !resets);
    } else if (which == 15) {
        for (bool present : {false,true}) {
            setup(); gpio.IDR = present ? 0 : 0x8000;
            SDIO->STA.value = active; SDIO->IDMACTRL.value = 1; SDIO->MASK.value = 0xffffffff;
            assert(sdio_init() == (present ? 0 : -1));
            assert(resets == 1 && !reset_hold && identified == unsigned(present));
            assert(SDIO->CLKCR.value == ((1u << 16) | SDCLK_LOW));
            assert(!(SDIO->STA.value & active) && !SDIO->IDMACTRL.value);
        }
    } else if (which == 16) {
        for (int result : {0,-1}) {
            setup(); identify_result = result;
            SDIO->STA.value = active; SDIO->IDMACTRL.value = 1;
            assert(sdio_reset() == result);
            assert(resets == 1 && reset_hold == 10 && identified == 1);
            assert(SDIO->CLKCR.value == SDCLK_LOW && !SDIO->IDMACTRL.value);
            assert(bool(sd_rca) == !result);
        }
    } else if (which == 17) {
        fault_command = 8; fault_status = SDMMC_STA_CPSMACT;
        wait_failure = osErrorTimeout; pending_start = true;
        assert(sd_cmd(CMD_08,0x1aa) == -1); clean();
        assert(delays >= 4 && !resets && commands == (std::vector<unsigned>{8,12}));
    } else if (which == 18) {
        for (bool writing : {false,true}) for (int count : {1,8}) for (bool high_capacity : {false,true}) {
            setup(); is_sdhc = high_capacity;
            int result = writing ? sd_write_blocks(42,count,buffer) : sd_read_blocks(42,count,buffer);
            assert(result == 0); clean();
            unsigned data_index = writing && count != 1 ? 2 : 0;
            assert(arguments[data_index] == (high_capacity ? 42u : 42u * 512));
            assert(SDIO->DLEN.value == u32(count * 512) && SDIO->IDMABASE0.value == 0x24002000);
            assert(SDIO->DTIMER.value == (writing ? 0x0fffffffu : 0xffffffffu));
            setup(); fault_command = writing ? (count == 1 ? 24 : 25) : (count == 1 ? 17 : 18);
            fault_status = SDMMC_STA_DCRCFAIL | SDMMC_STA_DPSMACT;
            result = writing ? sd_write_blocks(42,count,buffer) : sd_read_blocks(42,count,buffer);
            assert(result != 0); clean(); assert(!resets && sd_rca && commands.back() == 12);
        }
    } else if (which == 19) {
        for (bool writing : {false,true}) {
            setup(); fault_command = 12; fault_status = SDMMC_STA_CCRCFAIL;
            int result = writing ? sd_write_blocks(42,8,buffer) : sd_read_blocks(42,8,buffer);
            assert(result != 0); clean(); assert(!resets && sd_rca);
            assert(commands == (writing ? std::vector<unsigned>{55,23,25,12,12} : std::vector<unsigned>{18,12,12}));
        }
    } else if (which == 20) {
        data_fault(SDMMC_STA_DCRCFAIL | SDMMC_STA_DPSMACT); stop_before_reset = 4;
        assert(sd_read_blocks(42,1,buffer) != 0); clean();
        assert(!resets && !(SDIO->STA.value & SDMMC_STA_BUSYD0) && delays >= 4);
    } else if (which == 21) {
        data_fault(SDMMC_STA_DCRCFAIL | SDMMC_STA_DPSMACT);
        stop_before_reset = stop_after_reset = 5;
        assert(sd_read_blocks(42,1,buffer) != 0); clean();
        assert(resets == 2 && !sd_rca && delays >= 2 * OS_TICK_FREQ);
    } else if (which == 22) {
        data_fault(SDMMC_STA_DCRCFAIL | SDMMC_STA_DPSMACT);
        stop_before_reset = 6;
        assert(sd_read_blocks(42,1,buffer) != 0); clean();
        assert(!resets && sd_rca && delays >= 3);
    } else if (which == 23) {
        fail_semaphore_creation = true;
        assert(sdio_init() == -1 && !irq_enables && !identified && !sd_rca);
        setup();
        identify_result = -17;
        assert(sdio_init() == -17 && irq_enables == 1 && identified == 1 && !sd_rca);
    } else if (which == 24) {
        cmd_sem = nullptr;
        assert(sdio_reset() == -1 && !resets && !identified && !sd_rca);
    } else if (which >= 25 && which <= 27) {
        for (SpanPath path : {Bounce, Axi, Cartridge, UnalignedAxi, UnalignedCartridge, Blocks})
        for (bool writing : {false,true}) {
            if (which == 25) {
                // The first bounce chunk fits; the entire request does not.
                reject_span(path,writing,false,0x7ffff8u,9);
                reject_span(path,writing,false,0x7ffff0u,17);
                reject_span(path,writing,false,0x80002au,1);
                reject_span(path,writing,true,0xfffffff8u,9);
                reject_span(path,writing,true,0xfffffff0u,17);
            } else if (which == 26) {
                for (int count : {1,8,9,17}) {
                    accept_span(path,writing,false,0x800000u-u32(count),count);
                    accept_span(path,writing,true,0u-u32(count),count);
                }
            } else {
                for (bool high : {false,true}) for (int count : {0,-1})
                    reject_span(path,writing,high,42,count);
            }
        }
    } else assert(!"unknown case");
    std::printf("SD abort case %d passes; ticks=%u resets=%u delays=%u\n", which,tick,resets,delays);
}
