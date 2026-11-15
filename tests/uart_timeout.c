/* Adapted from paps-g 18dd95d7: exact legacy C receive function.
 * Probe counts check the loop budget, not physical milliseconds. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
_Static_assert(INT_MAX == 2147483647, "target signed-int domain");
static struct uart_regs { uint32_t ISR, RDR; } uart;
static unsigned reads, ready_after, read_limit;
static struct uart_regs *uart_access(void)
{
    assert(++reads <= read_limit);
    if (ready_after && reads >= ready_after) uart.ISR = 0x20;
    return &uart;
}
#define UART4 uart_access()
#include "timeout.inc"

static void setup(unsigned ready, unsigned limit)
{
    uart = (struct uart_regs){0, 0x5a};
    reads = 0; ready_after = ready; read_limit = limit;
}
int main(int argc, char **argv)
{
    assert(argc == 1 || (argc == 2 && !strcmp(argv[1], "valid")));
    const int durations[] = {0,10,1000,107374,-1,-107374,107375,INT_MAX,-107375,INT_MIN};
    unsigned count = argc == 1 ? sizeof durations / sizeof *durations : 6;
    for (unsigned i = 0; i < count; ++i) {
        setup(1,2);
        assert(_getc_tmout(durations[i]) == 0x5a && reads == 2);
        if (durations[i] > 0) {
            setup(3,4);
            assert(_getc_tmout(durations[i]) == 0x5a && reads == 4);
        } else {
            setup(0,1);
            assert(_getc_tmout(durations[i]) == -1 && reads == 1);
        }
    }
    for (unsigned ms = 1; ms <= 2; ++ms) {
        unsigned budget = ms == 1 ? 20000 : 40000;
        setup(0,budget);
        assert(_getc_tmout(ms) == -1 && reads == budget);
        setup(budget,budget+1);
        assert(_getc_tmout(ms) == 0x5a && reads == budget+1);
    }
    puts("pass: signed input domain, readiness and independent probe budgets");
}
