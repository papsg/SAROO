/* Adapted from paps-g ce4de2ff. Actual CD power handlers with modeled registers. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t u32;
enum { PORT_SF, PORT_CMD, PORT_ACK, PORT_SR };
static uint8_t reg8[4];
static uint16_t route, cdreg[4];
static u32 clock_value;
static int pre_busy, finish_after, pending, finish_polls, wrong_ack, ready_after;
static unsigned sf_reads, commands, route_writes, cr_reads, timer_reads;

static u32 get_timer(void)
{
    u32 value = clock_value;
    clock_value += 1000;
    timer_reads++;
    return value;
}
static uint8_t *register8(int port)
{
    if (port == PORT_SF) {
        /* Stop a broken fixture/legacy endless loop with an executed assertion. */
        assert(++sf_reads < 50000);
        if (pending) {
            if (finish_after >= 0 && finish_polls++ >= finish_after) {
                pending = 0;
                reg8[PORT_SF] = 0;
                reg8[PORT_ACK] = wrong_ack ? 0x10 : reg8[PORT_CMD];
            }
        } else if (pre_busy > 0) {
            if (!--pre_busy) reg8[PORT_SF] = 0;
        }
    } else if (port == PORT_CMD) {
        assert(reg8[PORT_SF] == 1 && !pending && !pre_busy);
        commands++;
        pending = 1;
        finish_polls = 0;
    }
    return &reg8[port];
}
static uint16_t *control_register(void)
{
    assert(!pending && !pre_busy);
    route_writes++;
    return &route;
}
static uint16_t *cd_register(unsigned port)
{
    assert(!pending && !reg8[PORT_SF] && commands && route == 0x3000);
    assert(port == cr_reads % 4);
    if (!port && ready_after >= 0 && cr_reads / 4 >= (unsigned)ready_after)
        cdreg[0] = 0x0100;
    assert(++cr_reads < 100000);
    return &cdreg[port];
}
#define SF (*register8(PORT_SF))
#define COMREG (*register8(PORT_CMD))
#define OREG31 (*register8(PORT_ACK))
#define SR (*register8(PORT_SR))
#define SS_CTRL (*control_register())
#define CR1 (*cd_register(0))
#define CR2 (*cd_register(1))
#define CR3 (*cd_register(2))
#define CR4 (*cd_register(3))
static void printk(const char *format, ...) { (void)format; }
#include "actual.inc"

static void reset(u32 start)
{
    memset(reg8, 0, sizeof reg8); memset(cdreg, 0, sizeof cdreg);
    reg8[PORT_SR] = 0x5a;
    route = CS0_RAM4M;
    clock_value = start;
    pre_busy = pending = finish_polls = wrong_ack = 0;
    finish_after = ready_after = 0;
    sf_reads = commands = route_writes = cr_reads = timer_reads = 0;
}
static void recover(void)
{
    assert(route == CS0_RAM4M);
    unsigned before = commands;
    /* The timed-out operation completes late, without a second CPU action. */
    pending = 0; reg8[PORT_SF] = 0; reg8[PORT_ACK] = reg8[PORT_CMD];
    finish_after = wrong_ack = 0;
    assert(route == CS0_RAM4M);
    assert(cdblock_off() == 0 && commands == before + 1);
    assert(route == (SAROO_EN | CS0_RAM4M));
}
static void healthy(void)
{
    for (int on = 0; on < 2; on++) for (int wait = 0; wait < 2; wait++) {
        reset(0); finish_after = 5; ready_after = 2;
        if (on) route |= SAROO_EN;
        assert((on ? cdblock_on(wait) : cdblock_off()) == 0);
        assert(commands == 1 && reg8[PORT_CMD] == (on ? CDON : CDOFF));
        assert(route_writes == 1 && route == (on ? CS0_RAM4M : (CS0_RAM4M | SAROO_EN)));
        assert(cr_reads == (on && wait ? 12u : 0u));
    }
}
static void idle(void)
{
    for (int on = 0; on < 2; on++) {
        reset(0); pre_busy = -1; reg8[PORT_SF] = 1;
        if (on) route |= SAROO_EN;
        uint16_t before = route;
        assert((on ? cdblock_on(1) : cdblock_off()) == -3);
        assert(!commands && !route_writes && !cr_reads && route == before);
        assert(timer_reads <= 1002);
    }
}
static void completion(void)
{
    for (int on = 0; on < 2; on++) {
        reset(0); finish_after = -1;
        if (on) route |= SAROO_EN;
        assert((on ? cdblock_on(1) : cdblock_off()) == -3);
        assert(commands == 1 && route_writes == (unsigned)on && !cr_reads);
        assert(pending && reg8[PORT_SF] && route == CS0_RAM4M && timer_reads <= 1002);
        recover();
    }
}
static void shared_budget(void)
{
    reset(0);
    pre_busy = 998;
    reg8[PORT_SF] = 1;
    finish_after = 20;
    assert(cdblock_off() == -3);
    assert(commands == 1 && pending && !route_writes && timer_reads <= 1002);
    recover();
}
static void wrong(void)
{
    for (int on = 0; on < 2; on++) {
        reset(0); wrong_ack = 1;
        assert((on ? cdblock_on(1) : cdblock_off()) == -1);
        assert(commands == 1 && !cr_reads && route_writes == (unsigned)on);
        recover();
    }
}
static void drive(void)
{
    reset(0); ready_after = -1;
    assert(cdblock_on(1) == -3);
    assert(commands == 1 && cr_reads >= 4 && !(cr_reads % 4));
    assert(timer_reads <= 10003 && route == CS0_RAM4M);
    recover();
}
static void wrap(void)
{
    const u32 starts[] = {0, UINT32_MAX - 500, UINT32_MAX - 999999};
    for (unsigned i = 0; i < sizeof starts / sizeof *starts; i++) {
        reset(starts[i]); finish_after = -1;
        assert(cdblock_off() == -3 && timer_reads <= 1002 && !route_writes);
        reset(starts[i]); ready_after = -1;
        assert(cdblock_on(1) == -3 && timer_reads <= 10003 && !(cr_reads % 4));
        for (int status = 1; status < 16; status++) {
            reset(starts[i]); ready_after = -1; cdreg[0] = status << 8;
            assert(cdblock_on(1) == 0 && cr_reads == 4);
        }
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "healthy")) healthy();
    else if (!strcmp(argv[1], "idle")) idle();
    else if (!strcmp(argv[1], "completion")) completion();
    else if (!strcmp(argv[1], "budget")) shared_budget();
    else if (!strcmp(argv[1], "wrong")) wrong();
    else if (!strcmp(argv[1], "drive")) drive();
    else if (!strcmp(argv[1], "wrap")) wrap();
    else abort();
    puts("ordered CD power and failure recovery checks pass");
    return 0;
}
