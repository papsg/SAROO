/* Exercise the production stream setup with accepted and refused DMA enables. */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using u32 = uint32_t;
constexpr u32 DMA_SxCR_EN = 0x00000001;
constexpr u32 DMA_SxCR_TRBUFF = 0x00100000;
constexpr u32 USART_ICR_TCCF = 0x40;
constexpr u32 USART_CR1_TCIE = 0x40;
constexpr u32 USART_CR3_DMAT = 0x80;

static bool refuse_enable;
static std::vector<u32> configurations;

struct StreamConfig {
    u32 value = 0;

    operator u32() const { return value; }

    StreamConfig &operator=(u32 next)
    {
        value = next;
        if (next & DMA_SxCR_EN) {
            configurations.push_back(next);
            if (refuse_enable) {
                value &= ~DMA_SxCR_EN;
                refuse_enable = false;
            }
        }
        return *this;
    }
};

static struct { StreamConfig CR; u32 NDTR, PAR, M0AR; } stream;
static struct { u32 LIFCR; } dma;
static struct { u32 ICR, CR1, CR3, TDR; } uart;
#define DMA1_Stream0 (&stream)
#define DMA1 (&dma)
#define UART4 (&uart)

static unsigned barriers;
static void __DMB() { ++barriers; }
static int sprintk(char *, const char *, ...) { return 0; }
static void _puts(char *) {}

#include "uart-dma.inc"

int main(int argc, char **argv)
{
    assert(argc == 2);
    const bool retry = std::strcmp(argv[1], "retry") == 0;
    refuse_enable = retry;
    char payload[3] = {'a', 'b', 'c'};

    uart4_dma_send(payload, sizeof(payload));

    assert(configurations.size() == (retry ? 2 : 1));
    assert(stream.NDTR == sizeof(payload));
    assert(stream.PAR == static_cast<u32>(reinterpret_cast<uintptr_t>(&uart.TDR)));
    assert(stream.M0AR == static_cast<u32>(reinterpret_cast<uintptr_t>(payload)));
    assert(barriers == 1);
    assert(uart.CR3 == USART_CR3_DMAT);
    for (u32 config : configurations) {
        constexpr u32 existing_config = 0x00000441;
        assert((config & ~DMA_SxCR_TRBUFF) == existing_config);
        if (!(config & DMA_SxCR_TRBUFF)) {
            std::fprintf(stderr, "UART DMA enable omitted TRBUFF: 0x%08x\n", config);
            return 1;
        }
    }
    return 0;
}
