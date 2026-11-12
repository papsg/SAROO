/* Exercise the actual SystemInit body with simulated register readiness. */
#include <cassert>
#include <cstdint>
#include <cstdlib>

static struct { uint32_t IDCODE; } debug_device;
#define DBGMCU (&debug_device)
static volatile uint32_t issuing;
static unsigned accesses, expected_accesses, priority_group;
[[maybe_unused]] static volatile uint32_t *axi_target()
{
    ++accesses;
    return &issuing;
}

struct StartupRegister {
    uint32_t value = 0;
    StartupRegister &operator=(uint32_t word)
    {
        assert(accesses == expected_accesses && "AXI workaround must precede startup configuration");
        value = word;
        return *this;
    }
};
static struct { StartupRegister CPACR, VTOR; } scb;
#define SCB (&scb)
static uint32_t __Vectors;
static void NVIC_SetPriorityGrouping(unsigned group)
{
    assert(accesses == expected_accesses);
    priority_group = group;
}

#ifdef BOOT
struct ReadyRegister {
    uint32_t ready, value = 0;
    operator uint32_t() const { return ready | value; }
    ReadyRegister &operator=(uint32_t word) { value = word; return *this; }
    ReadyRegister &operator|=(uint32_t word) { value |= word; return *this; }
};
static struct {
    ReadyRegister CR{0x0a020000};
    uint32_t CIER, PLLCFGR, PLLCKSELR, PLL1DIVR, PLL2FRACR, PLL2DIVR;
    uint32_t D1CFGR, D2CFGR, D3CFGR, D1CCIPR, D2CCIP1R, D2CCIP2R, D3CCIPR, CFGR;
} rcc;
static struct { uint32_t CR3; ReadyRegister D3CR{0x2000}; } pwr;
#define RCC (&rcc)
#define PWR (&pwr)
#endif

#include "system-init.inc"

int main(int argc, char **argv)
{
    assert(argc == 4);
    debug_device.IDCODE = std::strtoul(argv[1], nullptr, 0);
    uint32_t before = std::strtoul(argv[2], nullptr, 0);
    expected_accesses = std::strtoul(argv[3], nullptr, 0);
    issuing = before;
    SystemInit();
    assert(accesses == expected_accesses && issuing == (before | expected_accesses));
    assert(priority_group == 3 && scb.VTOR.value == (uint32_t)(uintptr_t)&__Vectors);
#ifdef BOOT
    assert(scb.CPACR.value == 0x00f00000 && rcc.CFGR == 0x2003);
#else
    assert(scb.CPACR.value == 0);
#endif
}
