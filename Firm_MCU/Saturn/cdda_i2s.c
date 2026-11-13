
#include "main.h"
#include <stdlib.h>
#include "ff.h"
#include "cdc.h"

#define ABUF_NUM 32u
#define ABUF_BYTES 2352u
#define ABUF_WORDS (ABUF_BYTES/2)
#define ABUF_BASE 0x30002000u
#define ABUF_SILENCE (ABUF_BASE+ABUF_NUM*ABUF_BYTES)
#define AUDIO_DMA_FLAGS 0x00000f40u
#define AUDIO_DMA_ERROR 0x00000200u
#define AUDIO_DMA_TC 0x00000800u
#define AUDIO_DMA_CT (1u<<19)
#define AUDIO_DMA_DBM (1u<<18)
#define AUDIO_I2S_CLEAR (SPI_IFCR_SUSPC | SPI_IFCR_TIFREC | SPI_IFCR_OVRC | SPI_IFCR_UDRC)
#define AUDIO_I2S_CSTART 0x200u
#define AUDIO_I2S_CSUSP 0x400u
#define AUDIO_I2S_SUSP 0x800u
/* A late publication leaves at least 16 sample transfers before bank switch.
 * No interrupt can delay this bounded register sequence. If hardware still
 * switches before the write, TEIF disables DMA and the error path recovers. */
#define AUDIO_PUBLISH_GUARD 16u
_Static_assert((ABUF_NUM & (ABUF_NUM-1)) == 0, "Audio ring must be a power of two");
_Static_assert(ABUF_SILENCE+ABUF_BYTES <= 0x30020000u, "Audio must stay in SRAM1");

enum { AUDIO_IDLE, AUDIO_PLAYING, AUDIO_STOPPING, AUDIO_RECOVER, AUDIO_STARTING,
       AUDIO_INIT, AUDIO_SUSPENDING, AUDIO_CANCELING, AUDIO_CANCELING_INIT };
static volatile u32 awp, arp;
static volatile unsigned audio_copying;
static volatile int aplay;
/* Only real TIM7 update events consume suspend grace intervals. */
static volatile unsigned audio_suspend_wait;
static void (*spi2_dma_notify)(void);

static u8 *audio_slot(u32 index)
{
    return (u8 *)(ABUF_BASE+(index & (ABUF_NUM-1))*ABUF_BYTES);
}

void spi2_set_notify(void *handle)
{
    spi2_dma_notify = handle;
}

static void audio_retry(unsigned ticks)
{
    /* TIM7 is dormant during healthy playback. The APB timer clock is 200 MHz,
     * as for the CD pacing timer: 100 counts at 100 kHz = 1 ms. */
    TIM7->CR1 = 0;
    TIM7->ARR = ticks-1;
    TIM7->CNT = 0;
    TIM7->SR = 0;
    TIM7->CR1 = 0x000d;
}

static void audio_suspend(void)
{
    SPI2->IER = 0;
    audio_suspend_wait = 2;
    SPI2->CR1 |= AUDIO_I2S_CSUSP;
    audio_retry(6); /* More than one complete 44.1 kHz stereo frame. */
}

static void audio_configure(void)
{
    SPI2->CR1 = 0;
    DMA1_Stream1->FCR = 0; /* Direct mode: nonfatal FEIF uses backpressure. */
    DMA1_Stream1->PAR = (u32)&SPI2->TXDR;
    SPI2->I2SCFGR = 0x02040015;
    SPI2->CR2 = 0;
    /* I2S mode: enable transmit DMA and retain the other reset fields. */
    SPI2->CFG1 = 0x00078007;
    SPI2->CFG2 = 0;
    SPI2->IER = 0;
    SPI2->IFCR = AUDIO_I2S_CLEAR;
    aplay = AUDIO_IDLE;
}

/* Caller owns the interrupt mask or runs in one of the equal-priority audio
 * handlers. Disabling DMA can finish asynchronously; never spin waiting on it. */
static int audio_stop(void)
{
    DMA1_Stream1->CR &= ~1u;
    if (DMA1_Stream1->CR & 1u) { audio_retry(100); return 0; }
    SPI2->IER = 0;
    SPI2->IFCR = SPI_IFCR_SUSPC;
    SPI2->CR1 = 0;
    DMA1->LIFCR = AUDIO_DMA_FLAGS;
    TIM7->CR1 = 0;
    TIM7->SR = 0;
    aplay = AUDIO_IDLE;
    return 1;
}

static void audio_service(void);

/* EN is acknowledged clear. A short normal transfer finishes only the
 * interrupted payload; its TC restores full-size DBM without resetting I2S. */
static void audio_dma_arm(u32 left, u32 active)
{
    u32 count = awp-arp;
    DMA1->LIFCR = AUDIO_DMA_FLAGS;
    DMA1_Stream1->M1AR = count >= 2 ? (u32)audio_slot(arp+1) : ABUF_SILENCE;
    u32 first = count ? (u32)audio_slot(arp) : ABUF_SILENCE;
    u32 mode = AUDIO_DMA_DBM;
    /* Quiet words need only preserve stereo parity, not a sector's duration.
     * One zero finishes an odd interrupted pair before a new left sample. */
    if (active >= ABUF_SILENCE) left &= 1u;
    if (left && left < ABUF_WORDS) {
        first = active >= ABUF_SILENCE ? ABUF_SILENCE : first+ABUF_BYTES-left*2;
        mode = 0;
    } else left = ABUF_WORDS;
    DMA1_Stream1->NDTR = left;
    DMA1_Stream1->M0AR = first;
    __DMB();
    DMA1_Stream1->CR = 0x00002c55 | mode; /* 16-bit, increment, TC/TE; no CIRC. */
}

static void audio_start(unsigned minimum)
{
    if (aplay != AUDIO_IDLE || awp-arp < minimum) return;
    if (DMA1_Stream1->CR & 1u) {
        aplay = AUDIO_STOPPING;
        if (SPI2->CR1 & AUDIO_I2S_CSTART) {
            aplay = AUDIO_SUSPENDING;
            audio_suspend();
        } else audio_retry(100);
        return;
    }
    SPI2->IFCR = AUDIO_I2S_CLEAR;
    aplay = AUDIO_STARTING;
    audio_dma_arm(0, 0);
    SPI2->CR1 = 1;
    /* Bound startup work; a stalled source uses the existing one-shot retry. */
    for (unsigned n=0; n<32 && (SPI2->SR & SPI_SR_TXP); n++) {}
    audio_service();
}

/* Old FIFO words may finish. Retire the SRAM queue only after DMA and
 * any in-progress producer copy have released it. The DMA residue supplies
 * the stereo parity for the first quiet transfer. */
static __attribute__((noinline)) void audio_retire_to_silence(void)
{
    if (audio_copying) return; /* The publishing caller will finish retirement. */
    DMA1_Stream1->CR &= ~1u;
    if (DMA1_Stream1->CR & 1u) { audio_retry(100); return; }
    u32 left = DMA1_Stream1->NDTR;
    awp = arp = 0;
    aplay = SPI2->CR1 & AUDIO_I2S_CSTART ? AUDIO_PLAYING : AUDIO_STARTING;
    audio_dma_arm(left, ABUF_SILENCE);
    audio_service();
    if (spi2_dma_notify) spi2_dma_notify();
}

static void audio_service(void)
{
    if (aplay == AUDIO_CANCELING) {
        audio_retire_to_silence();
    } else if (aplay == AUDIO_INIT || aplay == AUDIO_CANCELING_INIT) {
        int cancel = aplay == AUDIO_CANCELING_INIT;
        if (cancel && audio_copying) return;
        DMA1_Stream1->CR &= ~1u;
        if (DMA1_Stream1->CR & 1u) { audio_retry(100); return; }
        if (cancel) awp = arp = 0;
        audio_configure();
        audio_start(0); /* Start with valid silence before the first CDDA request. */
        if (cancel && spi2_dma_notify) spi2_dma_notify();
    } else if (aplay == AUDIO_RECOVER) {
        u32 cr = DMA1_Stream1->CR;
        if ((cr & 1u) || (DMA1->LISR & (AUDIO_DMA_TC | AUDIO_DMA_ERROR))) {
            audio_retry(100); return; /* Let the IRQ retire any late completion. */
        }
        u32 left = DMA1_Stream1->NDTR;
        u32 active = cr & AUDIO_DMA_CT ? DMA1_Stream1->M1AR : DMA1_Stream1->M0AR;
        /* Keep the FIFO, clocks and hardware underrun channel phase intact.
         * NDTR counts remaining peripheral writes, including any lost prefetch. */
        aplay = SPI2->CR1 & 0x200 ? AUDIO_PLAYING : AUDIO_STARTING;
        audio_dma_arm(left, active);
        audio_service();
    } else if (aplay == AUDIO_STOPPING || aplay == AUDIO_SUSPENDING) {
        /* CSUSP reads zero. Require frame completion and its SUSP status;
         * only actual timer expiries permit a stalled-clock forced abort. */
        if (aplay == AUDIO_SUSPENDING && audio_suspend_wait &&
            ((SPI2->CR1 & AUDIO_I2S_CSTART) || !(SPI2->SR & AUDIO_I2S_SUSP))) {
            return;
        }
        if (audio_stop()) audio_start(0);
    } else if (aplay == AUDIO_STARTING) {
        if (!(SPI2->SR & SPI_SR_TXP)) {
            aplay = AUDIO_PLAYING;
            SPI2->IER = SPI_IER_UDRIE;
            SPI2->CR1 = 0x201;
        } else audio_retry(100);
    } else if (aplay == AUDIO_PLAYING) {
        if (!(SPI2->SR & SPI_SR_UDR)) {
            SPI2->IER = SPI_IER_UDRIE;
        } else {
            /* TXP=0 with the one-sample threshold proves the FIFO is full.
             * Do not re-enable UDRIE until its asynchronous clear is visible. */
            if (!(SPI2->SR & SPI_SR_TXP)) SPI2->IFCR = SPI_IFCR_UDRC;
            audio_retry(100);
        }
    }
}

void SPI2_IRQHandler(void)
{
    SPI2->IER = 0;
    audio_service();
}

void TIM7_IRQHandler(void)
{
    if (!(TIM7->SR & 1u)) return; /* Stale NVIC pending is not a timer expiry. */
    TIM7->SR = 0;
    if (aplay == AUDIO_SUSPENDING && audio_suspend_wait && --audio_suspend_wait)
        audio_retry(100);
    audio_service();
}

void spi2_init(void)
{
    awp = arp = 0;
    aplay = AUDIO_INIT;
    memset((void*)ABUF_SILENCE, 0, ABUF_BYTES);
    RCC->APB1LENR |= RCC_APB1LENR_TIM7EN;
    TIM7->CR1 = 0;
    TIM7->PSC = 1999;
    TIM7->ARR = 99;
    TIM7->EGR = 1; /* Load the prescaler before the first one-shot. */
    TIM7->SR = 0;
    TIM7->DIER = 1;
    DMAMUX1_Channel1->CCR = 40;
    NVIC_SetPriority(DMA1_Stream1_IRQn, 0);
    NVIC_SetPriority(SPI2_IRQn, 0);
    NVIC_SetPriority(TIM7_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Stream1_IRQn);
    NVIC_EnableIRQ(SPI2_IRQn);
    NVIC_EnableIRQ(TIM7_IRQn);
    audio_service();
}

void DMA1_Stream1_IRQHandler(void)
{
    u32 flags = DMA1->LISR;
    /* A later TC/error still owns its notification and must reach the next IRQ. */
    DMA1->LIFCR = flags & AUDIO_DMA_FLAGS;
    if (aplay != AUDIO_PLAYING && aplay != AUDIO_STARTING && aplay != AUDIO_RECOVER) return;
    u32 cr = DMA1_Stream1->CR;
    u32 ct = cr & AUDIO_DMA_CT;
    volatile uint32_t *done = (cr & AUDIO_DMA_DBM) && !ct ? &DMA1_Stream1->M1AR : &DMA1_Stream1->M0AR;
    u32 active = ct ? DMA1_Stream1->M1AR : DMA1_Stream1->M0AR;
    int completed = (flags & AUDIO_DMA_TC) && *done < ABUF_SILENCE &&
                    ((cr & AUDIO_DMA_DBM) || !DMA1_Stream1->NDTR);
    if (completed) arp++;
    if (flags & AUDIO_DMA_ERROR) {
        /* One retry per millisecond also bounds a persistent AHB error. */
        aplay = AUDIO_RECOVER;
        SPI2->IER = 0;
        audio_retry(100);
    } else if (flags & AUDIO_DMA_TC) {
        if (aplay == AUDIO_RECOVER || !(cr & AUDIO_DMA_DBM)) {
            aplay = AUDIO_RECOVER;
            audio_service();
        } else {
            u32 next = arp + (active != ABUF_SILENCE);
            *done = next != awp ? (u32)audio_slot(next) : ABUF_SILENCE;
        }
    }
    if (completed && spi2_dma_notify) spi2_dma_notify();
}

static void audio_publish_next(void)
{
    if (aplay != AUDIO_PLAYING && aplay != AUDIO_STARTING) return;
    u32 cr = DMA1_Stream1->CR;
    /* A stopped, exhausted quiet stream can be rearmed on demand. Pending
     * completion/error and any payload bank still belong to the DMA IRQ. */
    if (!(cr & 1u) && !DMA1_Stream1->NDTR &&
        DMA1_Stream1->M0AR == ABUF_SILENCE && DMA1_Stream1->M1AR == ABUF_SILENCE &&
        !(DMA1->LISR & (AUDIO_DMA_TC | AUDIO_DMA_ERROR))) {
        SPI2->IER = 0;
        aplay = AUDIO_RECOVER;
        audio_service();
        return;
    }
    if (!(cr & AUDIO_DMA_DBM)) return; /* Normal-mode addresses are protected. */
    u32 ct = cr & AUDIO_DMA_CT;
    u32 left = DMA1_Stream1->NDTR;
    /* A pending completion invalidates the software read position. The normal
     * completion handler will publish the next bank instead. */
    if (left <= AUDIO_PUBLISH_GUARD || (DMA1->LISR & (AUDIO_DMA_TC | AUDIO_DMA_ERROR))) {
        audio_retry(40);
        return;
    }
    volatile uint32_t *next_bank = ct ? &DMA1_Stream1->M0AR : &DMA1_Stream1->M1AR;
    if (*next_bank != ABUF_SILENCE) return;
    u32 active = ct ? DMA1_Stream1->M1AR : DMA1_Stream1->M0AR;
    u32 next = arp + (active != ABUF_SILENCE);
    if (next != awp) {
        *next_bank = (u32)audio_slot(next);
        if (active == ABUF_SILENCE) audio_retry(4);
    }
}

void spi2_flush(void)
{
    int key = disable_irq();
    audio_start(1); /* A completed one-sector request must not wait for a second. */
    restore_irq(key);
}

static inline int audio_cancel_pending(void)
{
    int state = aplay;
    return state == AUDIO_CANCELING || state == AUDIO_CANCELING_INIT;
}

void spi2_cancel(void)
{
    int key = disable_irq();
    if (!audio_cancel_pending()) {
        if (aplay == AUDIO_INIT) {
            /* A startup stop can still be pending. Configure I2S before
             * starting the quiet link, even when its queued data is retired. */
            aplay = AUDIO_CANCELING_INIT;
            audio_service();
        } else if (!audio_copying && awp == arp &&
                   DMA1_Stream1->M0AR == ABUF_SILENCE && DMA1_Stream1->M1AR == ABUF_SILENCE) {
            awp = arp = 0;
        } else {
            SPI2->IER = 0;
            audio_suspend_wait = 0;
            aplay = AUDIO_CANCELING;
            audio_service();
        }
    }
    restore_irq(key);
}

static __attribute__((noinline)) int queue_audio_sector(u8 *data, int attenuate)
{
    int key = disable_irq();
    if (audio_copying || audio_cancel_pending() || awp-arp == ABUF_NUM) {
        restore_irq(key);
        return -1;
    }
    audio_copying = 1;
    u8 *slot = audio_slot(awp);
    restore_irq(key);

    if (attenuate) {
        /* Build the -12 dB sector before DMA can see its slot. */
        for (unsigned i = 0; i < ABUF_BYTES; i += 2) {
            int sample = (short)(data[i] | (data[i+1] << 8));
            unsigned quiet = (unsigned)(short)(sample / 4);
            slot[i] = quiet;
            slot[i+1] = quiet >> 8;
        }
    } else {
        memcpy32(slot, data, ABUF_BYTES);
    }
    __DMB(); /* Publish only after every SRAM payload store is visible to DMA. */

    key = disable_irq();
    audio_copying = 0;
    if (audio_cancel_pending()) {
        audio_service();
        restore_irq(key);
        return -1;
    }
    awp++;
    audio_publish_next();
    int count = awp-arp;
    restore_irq(key);
    return count;
}

int fill_audio_buffer(u8 *data)
{
    return queue_audio_sector(data, 0);
}

int fill_scan_audio_buffer(u8 *data)
{
    return queue_audio_sector(data, 1);
}

void spi2_trans(u32 data)
{
	if(SPI2->SR&0x0020)
		SPI2->IFCR = 0x0020;
	while((SPI2->SR&SPI_SR_TXP)==0);
	*(u16*)&SPI2->TXDR = data;
}

// test
void play_i2s(char *audio_file)
{
	FIL fp;
	int retv, size;
	u32 rv;

	retv = f_open(&fp, audio_file, FA_READ);
	if(retv){
		printk("Failed open audio file!\n");
		return;
	}
	size = f_size(&fp);
	printk("Open %s ...\n", audio_file);
	printk("    Size %08x\n", size);

	int count = 0;
	int sec = 0;
	//size = 2352*1024;
	while(size>0){
		rv = 0;
		f_read(&fp, (void*)0x24002000, 2352*8, &rv);
		
		int i;
		for(i=0; i<rv; i+=2352){
			while(fill_audio_buffer((u8*)0x24002000+i)<0){
				osDelay(1);
			}
		}

		size -= rv;
		count += rv;
		if(count>176400){
			printk("%3d ...\n", sec);
			sec += 1;
			count -= 176400;
		}
	}

	f_close(&fp);
}


/******************************************************************************/



