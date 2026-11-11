
#include "main.h"

static void uart4_configure(void)
{
    UART4->CR1 = 0;
    UART4->CR1 = 0x2000000c; /* FIFO, transmitter and receiver. */
    UART4->CR2 = 0;
    UART4->CR3 = 0x64000000;
    UART4->BRR = 0x0064; /* 1 Mbit/s with the existing 100 MHz clock. */
    UART4->RQR = 0x0018;
    UART4->CR1 |= 1u;
    DMAMUX1_Channel0->CCR = 64; /* UART4 TX. */
}

#ifndef BOOT
#define BUF_SIZE 4096
#define TX_DMA_EVENTS (DMA_LISR_TCIF0 | DMA_LISR_TEIF0)
#define TX_DMA_FLAGS (TX_DMA_EVENTS | DMA_LISR_HTIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)

_Static_assert((BUF_SIZE & (BUF_SIZE - 1)) == 0 && BUF_SIZE < 65536,
               "UART queue positions and the initial sentinel fit in 16 bits");

static u8 *const tx_buffer = (u8*)0x30000000;
static u16 read_pos, published_pos, reserved_pos, writers;
static u16 transfer_end; /* BUF_SIZE until the startup DMA stop is acknowledged. */

static int uart4_retry_pending(void)
{
    return (TIM4->CR1 & TIM_CR1_CEN) || (TIM4->SR & TIM_SR_UIF);
}

static void uart4_retry(void)
{
    /* Keep an existing deadline, including an expiry awaiting its IRQ. */
    if (uart4_retry_pending())
        return;
    TIM4->CNT = 0;
    TIM4->CR1 = TIM_CR1_OPM | TIM_CR1_URS | TIM_CR1_CEN;
}

/* DMA and timer IRQs have equal priority. Other callers mask interrupts. */
static void uart4_service(void)
{
    if (DMA1_Stream0->CR & DMA_SxCR_EN) {
        uart4_retry();
        return;
    }

    if (transfer_end == BUF_SIZE) {
        uart4_configure();
        DMA1_Stream0->FCR = 0; /* Direct mode, no IRQ for FIFO backpressure. */
        DMA1_Stream0->PAR = (u32)&UART4->TDR;
        UART4->CR3 |= USART_CR3_DMAT;
    } else if (transfer_end != read_pos) {
        /* EN is clear. NDTR counts writes still owed to the UART FIFO. */
        read_pos = (transfer_end - DMA1_Stream0->NDTR) & (BUF_SIZE - 1);
    }
    transfer_end = read_pos;

    /* The stopped stream cannot add events while its old flags are cleared. */
    DMA1->LIFCR = TX_DMA_FLAGS;
    if (read_pos == published_pos)
        return;

    u32 count = (published_pos - read_pos) & (BUF_SIZE - 1);
    u32 tail = BUF_SIZE - read_pos;
    if (count > tail)
        count = tail;
    transfer_end = (read_pos + count) & (BUF_SIZE - 1);
    DMA1_Stream0->NDTR = count;
    DMA1_Stream0->M0AR = (u32)(tx_buffer + read_pos);
    UART4->ICR = USART_ICR_TCCF;
    __DMB();
    DMA1_Stream0->CR = DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TRBUFF |
                      DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_EN;
    /* A short transfer can finish before EN is read. Leave that event to its IRQ. */
    if (!(DMA1_Stream0->CR & DMA_SxCR_EN) && !(DMA1->LISR & TX_DMA_EVENTS))
        uart4_retry();
}

void DMA1_Stream0_IRQHandler(void)
{
    u32 events = DMA1->LISR & TX_DMA_EVENTS;
    if (!events)
        return;

    /* W1C only the observed events. A stop can complete after this snapshot. */
    DMA1->LIFCR = events;
    if (events & DMA_LISR_TEIF0)
        uart4_retry();
    else if (!uart4_retry_pending())
        uart4_service();
}

void TIM4_IRQHandler(void)
{
    if (!(TIM4->SR & TIM_SR_UIF))
        return;
    TIM4->SR = 0;
    uart4_service();
}

void uart4_puts(char *str)
{
    size_t length = strlen(str);
    if (!length)
        return;

    int key = disable_irq();
    u32 free = (read_pos - reserved_pos - 1u) & (BUF_SIZE - 1);
    if (length > free) {
        restore_irq(key);
        return; /* Preserve admitted output and keep the caller moving. */
    }

    u32 start = reserved_pos;
    reserved_pos = (start + length) & (BUF_SIZE - 1);
    ++writers;
    restore_irq(key);

    size_t tail = BUF_SIZE - start;
    if (tail > length)
        tail = length;
    memcpy(tx_buffer + start, str, tail);
    if (length > tail)
        memcpy(tx_buffer, str + tail, length - tail);

    key = disable_irq();
    /* The last writer publishes the complete prefix even when copies finish
     * out of order. Every armed span ends at a published position. */
    if (--writers == 0) {
        published_pos = reserved_pos;
        if (transfer_end == read_pos && !uart4_retry_pending())
            uart4_service();
    }
    restore_irq(key);
}
#endif

void uart4_init(void)
{
#ifdef BOOT
    uart4_configure(); /* BOOT uses polling I/O. */
#else
    int key = disable_irq();
    read_pos = published_pos = reserved_pos = writers = 0;
    transfer_end = BUF_SIZE;
    NVIC_DisableIRQ(UART4_IRQn);
    DMA1_Stream0->CR &= ~DMA_SxCR_EN;

    RCC->APB1LENR |= RCC_APB1LENR_TIM4EN;
    (void)RCC->APB1LENR;
    TIM4->CR1 = TIM_CR1_URS;
    TIM4->PSC = 1999; /* 200 MHz timer clock. */
    TIM4->ARR = 999; /* 10 ms on failure. */
    TIM4->EGR = TIM_EGR_UG; /* Load the prescaler before the first retry. */
    TIM4->SR = 0;
    TIM4->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(DMA1_Stream0_IRQn, 0);
    NVIC_SetPriority(TIM4_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);
    uart4_service();
    restore_irq(key);
#endif
}


uint8_t _getc(void)
{
	while((UART4->ISR & 0x20)==0){
		osDelay(20);
	}
	return UART4->RDR;
}


int _getc_tmout(int tmout_ms)
{
	int ticks = tmout_ms*20000;

	while((UART4->ISR & 0x20)==0){
		ticks -= 1;
		if(ticks<=0)
			return -1;
	}
	return UART4->RDR;
}

void _putc(uint8_t byte)
{
	while((UART4->ISR & 0x80)==0);
	UART4->TDR = byte;
}


void _puts(char *str)
{
	char *p = str;

	for(p=str; *p; p++) {
		if (*p == '\n')
			_putc('\r');
		_putc(*p);
	}
}


/******************************************************************************/
