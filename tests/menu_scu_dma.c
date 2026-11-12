#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t u32;
typedef struct {
    u32 src, dst, count, addv, enable, mode, reserved[2];
} SCUDMA_REG;

static volatile SCUDMA_REG dma[3];
static int channel;
static u32 initial_status;
static unsigned reads;

static u32 read_status(void)
{
    assert(dma[channel].src == 0 && dma[channel].dst == 0);
    assert(dma[channel].count == 0 && dma[channel].enable == 0);
    return reads++ == 0 ? initial_status : 0;
}

#define SCU_DMA_BASE ((uintptr_t)dma)
#define SDSTAT read_status()
#include "scu-dmemcpy.inc"

int main(int argc, char **argv)
{
    assert(argc == 4);
    channel = atoi(argv[1]);
    initial_status = (u32)strtoul(argv[2], NULL, 0);

    scu_dmemcpy((void *)(uintptr_t)0x25e00000, (void *)(uintptr_t)0x06010000,
                64, channel);
    assert(reads == (unsigned)atoi(argv[3]));
    assert(dma[channel].src == 0x06010000 && dma[channel].dst == 0x25e00000);
    assert(dma[channel].count == 64 && dma[channel].addv == 0x101);
    assert(dma[channel].mode == 7 && dma[channel].enable == 0x101);
    for (int other = 0; other < 3; ++other) {
        if (other != channel)
            assert(dma[other].src == 0 && dma[other].enable == 0);
    }
    return 0;
}
