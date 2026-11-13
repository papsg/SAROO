
/*
 *  stm32_sdio.c
 *    low level driver for STM32H750 SDIO
 *    only SD card suport. no MMC support.
 *
 *  writen by tpu, 2021-01-28
 */


#include "main.h"
#include "RTX_Config.h"


/******************************************************************************/


#define SDIO            SDMMC1
#define SDIO_IRQn       SDMMC1_IRQn
#define SDIO_IRQHandler SDMMC1_IRQHandler


/******************************************************************************/

// fSD_CLK = 200M/(2*div)
#define SDCLK_LOW  400
#define SDCLK_12M  8
#define SDCLK_25M  4
#define SDCLK_50M  2

#define SDCLK_HIGH  SDCLK_25M



#define CMD_RESP_NONE     0x0000
#define CMD_RESP_SHORT    0x0100
#define CMD_RESP_SHORTNC  0x0200
#define CMD_RESP_LONG     0x0300

#define RESP_MASK         0x0300

#define RESP_NONE(cmd)  ((cmd&RESP_MASK)==CMD_RESP_NONE)
#define RESP_SHORT(cmd) ((cmd&RESP_MASK)==CMD_RESP_SHORT || (cmd&RESP_MASK)==CMD_RESP_SHORTNC)
#define RESP_LONG(cmd)  ((cmd&RESP_MASK)==CMD_RESP_LONG)
#define RESP_NOCRC(cmd) ((cmd&RESP_MASK)==CMD_RESP_SHORTNC)

#define HAS_DATA(cmd)   (cmd&SDMMC_CMD_CMDTRANS)


#define CMD_00  ( 0 | CMD_RESP_NONE)
#define CMD_02  ( 2 | CMD_RESP_LONG)
#define CMD_03  ( 3 | CMD_RESP_SHORT)
#define CMD_06  ( 6 | CMD_RESP_SHORT)
#define CMD_07  ( 7 | CMD_RESP_SHORT)
#define CMD_08  ( 8 | CMD_RESP_SHORT)
#define CMD_09  ( 9 | CMD_RESP_LONG)
#define CMD_12  (12 | CMD_RESP_SHORT | SDMMC_CMD_CMDSTOP)
#define CMD_13  (13 | CMD_RESP_SHORT)
#define CMD_16  (16 | CMD_RESP_SHORT)
#define CMD_17  (17 | CMD_RESP_SHORT | SDMMC_CMD_CMDTRANS)
#define CMD_18  (18 | CMD_RESP_SHORT | SDMMC_CMD_CMDTRANS)
#define CMD_23  (23 | CMD_RESP_SHORT)
#define CMD_24  (24 | CMD_RESP_SHORT | SDMMC_CMD_CMDTRANS)
#define CMD_25  (25 | CMD_RESP_SHORT | SDMMC_CMD_CMDTRANS)
#define CMD_41  (41 | CMD_RESP_SHORTNC)
#define CMD_55  (55 | CMD_RESP_SHORT)

#define CMD_ERROR  (SDMMC_STA_CTIMEOUT | SDMMC_STA_CCRCFAIL)
#define DATA_ERROR (SDMMC_STA_IDMATE | SDMMC_STA_RXOVERR | SDMMC_STA_TXUNDERR | SDMMC_STA_DTIMEOUT | SDMMC_STA_DCRCFAIL)
/* SD Physical Layer 3.01, Table 4-36 and the R6 response format. */
#define SD_R1_ERRORS 0xfff98008u
#define SD_R6_ERRORS 0x0000e000u

/******************************************************************************/

static u32 cmd_resp[4];
static u8  sd_cid[16];
static int is_sdhc;
static volatile u32 sd_rca;
static volatile u8 sd_removed;
static u8 sd_known;
static u8 sd_pending_write;

static osSemaphoreId_t cmd_sem;

/******************************************************************************/

static void sd_clk_set(int div)
{
	u32 val;

	val = SDIO->CLKCR;
	val &= 0xffffec00;
	val |= div;
	if(div!=SDCLK_LOW)
		val |= 0x1000;
	SDIO->CLKCR = val;
}

static void sd_bus_width(int width)
{
	u32 val;

	val = SDIO->CLKCR;
	val &= 0xffff3fff;
	if(width==4)
		val |= 0x00004000;
	SDIO->CLKCR = val;
}

int sd_card_insert(void)
{
	if (!(GPIOA->IDR & 0x8000)) return 1; /* Active-low PA15. */
	sd_removed = 1;
	return 0;
}

int sdio_ready(void)
{
	return sd_card_insert() && !sd_removed && sd_rca;
}

/******************************************************************************/



void SDIO_IRQHandler(void)
{
	u32 stat = SDIO->STA;
	if (!(stat & SDIO->MASK)) return;
	if ((stat & SDMMC_STA_BUSYD0) && !(stat & (CMD_ERROR | DATA_ERROR))) {
		SDIO->MASK = SDMMC_STA_BUSYD0END | CMD_ERROR | DATA_ERROR;
	} else {
		SDIO->MASK = 0;
		osSemaphoreRelease(cmd_sem);
	}
}

/* Poll only during error recovery. Keep DMA ownership until both state
 * machines finish, including the STOP response and the card busy interval. */
static int sd_stop(void)
{
	u32 start = osKernelGetTickCount();
	int sent = 0;
	for (;;) {
		u32 stat = SDIO->STA;
		if (sent) {
			if ((stat & (SDMMC_STA_CMDREND | CMD_ERROR)) &&
			    !(SDIO->CMD & SDMMC_CMD_CPSMEN) &&
			    !(stat & (SDMMC_STA_CPSMACT | SDMMC_STA_DPSMACT | SDMMC_STA_BUSYD0)))
				/* The STOP response can repeat status from the failed transfer. */
				return stat & CMD_ERROR ? -1 : 0;
		} else if (!(SDIO->CMD & SDMMC_CMD_CPSMEN)) {
			SDIO->ICR = 0x1fe00fff;
			SDIO->ARG = 0;
			SDIO->CMD = 0x1000 | CMD_12;
			sent = 1;
		}
		if ((u32)(osKernelGetTickCount() - start) >= OS_TICK_FREQ || osDelay(1) != osOK)
			return -1;
	}
}

static void sd_reset_controller(u32 clock, u32 hold_ticks)
{
	RCC->AHB3RSTR |= RCC_AHB3RSTR_SDMMC1RST;
	__DSB();
	if (hold_ticks) osDelay(hold_ticks);
	RCC->AHB3RSTR &= ~RCC_AHB3RSTR_SDMMC1RST;
	SDIO->CLKCR = clock;
	SDIO->DTIMER = 0xffffffff;
	SDIO->POWER = 3;
}

/* Keep rare recovery state out of every successful command's stack frame. */
static __attribute__((noinline)) void sd_recover(u32 cmd)
{
	/* The caller masked and qualified the IRQ. Release increments its token
	 * before deferred RTOS processing, so one drain removes late completion. */
	(void)osSemaphoreAcquire(cmd_sem, 0);
	/* ES0392: a receive IDMA error can retain data even after DATAEND. */
	int reset = (SDIO->STA & SDMMC_STA_IDMATE) && (SDIO->DCTRL & SDMMC_DCTRL_DTDIR);
	if ((cmd & (SDMMC_CMD_CMDTRANS | SDMMC_CMD_CMDSTOP)) ||
	    (SDIO->CMD & SDMMC_CMD_CPSMEN) ||
	    (SDIO->STA & (SDMMC_STA_CPSMACT | SDMMC_STA_DPSMACT))) {
		/* Retry a failed STOP as well as an interrupted data command.
		 * One resynchronization attempt keeps a recoverable card usable. */
		for (int remaining = 2; sd_stop(); ) {
			sd_reset_controller(SDIO->CLKCR, 0);
			reset = 0;
			if (!--remaining) { sd_rca = 0; break; }
		}
	}
	if (reset) sd_reset_controller(SDIO->CLKCR, 0);
}


static int sd_cmd(u32 cmd, u32 arg)
{
	u32 mask, stat, cmdr;
	int retv;


	cmdr = 0x1000 | (cmd&0x03ff);

	if(HAS_DATA(cmd)){
		mask = SDMMC_STA_DATAEND | DATA_ERROR | CMD_ERROR;
	}else if(RESP_NONE(cmd)){
		mask = SDMMC_STA_CMDSENT | CMD_ERROR;
	}else{
		mask = SDMMC_STA_CMDREND | CMD_ERROR;
	}
	SDIO->MASK = mask;
	SDIO->ARG = arg;
	SDIO->CMD = cmdr;

	retv = osSemaphoreAcquire(cmd_sem, 100);
	if(retv!=osOK){
		printk("sd_cmd: irq timeout! CMD=%08x STA=%08x\n", SDIO->CMD, SDIO->STA);
		stat = 0xffffffff;
		goto _exit;
	}

	stat = SDIO->STA;
	if(RESP_NOCRC(cmd)){
		stat &= 0xfffffffe;
	}
	if(stat&(CMD_ERROR|DATA_ERROR)){
		// CMD or DATA error
		printk("sd_cmd: CMD_%02d error %08x!\n", (cmd&0x3f), stat);
		goto _exit;
	}
	stat = 0;

	if(RESP_SHORT(cmd)){
		u32 response = SDIO->RESP1;
		/* CMD3 carries R6, CMD8 carries R7 and ACMD41 carries R3.
		 * The other short responses used here are R1 or R1b. */
		if (cmd == CMD_03) stat = response & SD_R6_ERRORS;
		else if (cmd != CMD_08 && !RESP_NOCRC(cmd)) stat = response & SD_R1_ERRORS;
		if (!stat) cmd_resp[0] = response;
	}else if(RESP_LONG(cmd)){
		cmd_resp[0] = SDIO->RESP4;
		cmd_resp[1] = SDIO->RESP3;
		cmd_resp[2] = SDIO->RESP2;
		cmd_resp[3] = SDIO->RESP1;
	}


_exit:

	SDIO->MASK = 0;
	if (stat) sd_recover(cmd);
	SDIO->CMD = 0;
	if(HAS_DATA(cmd) || stat){
		SDIO->DCTRL = 0;
		SDIO->IDMACTRL = 0;
	}
	SDIO->ICR = 0x1fe00fff;
	return stat;
}
	
static int sd_acmd(u32 rca, u32 cmd, u32 arg)
{
	int retv;

	retv = sd_cmd(CMD_55, rca<<16);
	if(retv)
		return retv;

	retv = sd_cmd(cmd, arg);
	return retv;
}

/******************************************************************************/

/* Check the inclusive end before copying or programming the controller.
 * SDSC uses a 32-bit byte address; SDHC/SDXC use 32-bit sector addresses. */
static int sd_range_valid(u32 start, int count)
{
    if (count <= 0) return 0;
    u32 last = start + (u32)count - 1u;
    return last >= start && (is_sdhc || last < 0x800000u);
}

/* DATAEND ends the transfer. CMD13 confirms the card has finished programming. */
int sd_sync(void)
{
    if (!sdio_ready()) return -1;
    if (!sd_pending_write) return 0;

    u32 start = osKernelGetTickCount();
    do {
        if (sd_cmd(CMD_13, sd_rca << 16) || !sdio_ready()) return -1;
        u32 status = cmd_resp[0] & 0x1f00u;
        if (status == 0x0900u) {
            sd_pending_write = 0;
            return 0;
        }
        unsigned state = status >> 9;
        if (state != 6 && state != 7) return -1;
    } while (osKernelGetTickCount() - start < 5u * OS_TICK_FREQ &&
             osDelay(1) == osOK && sdio_ready());
    return -1;
}

/* One setup/cleanup contract for both directions. Select the final transfer
 * mode before enabling IDMA; no data command runs during write pre-erase.
 * Callers supply only the command index; response/data flags are invariant. */
static int sd_blocks(u32 block, int count, u8 *buf, u32 index)
{
	int writing = index == (CMD_24 & 0x3f);
	if (writing ? !sdio_ready() : sd_sync()) return -1;
	if (count != 1) {
		index++;
		if (writing) {
			int result = sd_acmd(sd_rca, CMD_23, count);
			if (result) return result;
			if (!sdio_ready()) return -1;
		}
	}
	u32 control = writing ? 0x90 : 0x92;
	SDIO->DTIMER = writing ? 0x0fffffff : 0xffffffff;
	SDIO->DLEN = count * 512;
	if (count != 1) control |= 0x0c;
	SDIO->DCTRL = control;
	SDIO->IDMABASE0 = (u32)buf;
	SDIO->IDMACTRL = 1;
	if (!is_sdhc) block *= 512;
	if (writing) sd_pending_write = 1;
	int retv = sd_cmd(index | CMD_RESP_SHORT | SDMMC_CMD_CMDTRANS, block);
	if (!retv && count != 1) retv = sd_cmd(CMD_12, 0);
	return retv;
}

int sd_read_blocks(u32 block, int count, u8 *buf)
{
	return sd_range_valid(block, count) ? sd_blocks(block, count, buf, CMD_17 & 0x3f) : -1;
}

int sd_write_blocks(u32 block, int count, u8 *buf)
{
	return sd_range_valid(block, count) ? sd_blocks(block, count, buf, CMD_24 & 0x3f) : -1;
}

/******************************************************************************/

#define SDBUF_SIZE 8
/* At 25 MHz in four-bit SDR, 2 MiB takes about 168 ms on the wire. */
#define SD_DIRECT_MAX_SECTORS 4096

static u8 *sdbuf = (u8*)0x24000000;

//BLKDEV sd_dev;

/* Direct IDMA needs a reachable, word-aligned address. Other byte buffers
 * use the existing eight-sector bounce path. */
static __attribute__((noinline)) int sd_sectors(u32 start, int size, u8 *buf, u32 index)
{
    u32 address = (u32)buf;
    u32 type = address >> 24;
    if (!sd_range_valid(start, size) || !sdio_ready()) return -1;
    if ((address & 3u) == 0 && (type == 0x24 || type == 0x61)) {
        while (size > SD_DIRECT_MAX_SECTORS) {
            int result = sd_blocks(start, SD_DIRECT_MAX_SECTORS, buf, index);
            if (result) return result;
            buf += SD_DIRECT_MAX_SECTORS * 512;
            size -= SD_DIRECT_MAX_SECTORS;
            start += SD_DIRECT_MAX_SECTORS;
        }
        return sd_blocks(start, size, buf, index);
    }
    while (size) {
        int count = size > SDBUF_SIZE ? SDBUF_SIZE : size;
        unsigned bytes = count * 512;
        if (index == (CMD_24 & 0x3f)) memcpy(sdbuf, buf, bytes);
        int result = sd_blocks(start, count, sdbuf, index);
        if (result) return result;
        if (index == (CMD_17 & 0x3f)) memcpy(buf, sdbuf, bytes);
        buf += bytes;
        size -= count;
        start += count;
    }
    return 0;
}

int sd_read_sector(u32 start, int size, u8 *buf)
{
    return sd_sectors(start, size, buf, CMD_17 & 0x3f);
}

int sd_write_sector(u32 start, int size, u8 *buf)
{
    return sd_sectors(start, size, buf, CMD_24 & 0x3f);
}


int sdcard_identify(void)
{
	int retv, hcs;
	u8 cid[sizeof(sd_cid)];

	/* Publish sector access only after the full identification sequence. */
	sd_rca = 0;
	sd_removed = 0;
	if (!sd_card_insert()) return -1;
	osDelay(10);
	sd_cmd(CMD_00, 0);
	osDelay(10);
	retv = sd_cmd(CMD_00, 0);
	if (retv) return retv;
	osDelay(10);

	retv = sd_cmd(CMD_08, 0x000001aa);
	/* R7 echoes the requested voltage and the eight-bit check pattern. */
	if (!retv && (cmd_resp[0] & 0xfffu) != 0x1aau) return -1;
	if(retv)
		hcs = 0;
	else
		hcs = 1<<30;

	/* Measure the retry budget from the first ACMD41, after its CMD55 prefix. */
	retv = sd_cmd(CMD_55, 0);
	const u32 start = osKernelGetTickCount();
	const u32 ready_ticks = 2u * OS_TICK_FREQ;
	while(!retv){
		cmd_resp[0] = 0;
		retv = sd_cmd(CMD_41, 0x00f00000|hcs);
		if(retv)
			break;
		if(cmd_resp[0]&0x80000000)
			break;
		if ((u32)(osKernelGetTickCount() - start) >= ready_ticks) {
			retv = osErrorTimeout;
			break;
		}
		osDelay(1);
		retv = sd_cmd(CMD_55, 0);
	}
	if(retv){
		printk("ACMD41 error! %08x\n", retv);
		return -1;
	}
	int high_capacity = (cmd_resp[0] & 0x40000000) != 0;

	// Send CID
	retv = sd_cmd(CMD_02, 0x00000000);
	if(retv)
		return retv;
	memcpy(cid, cmd_resp, sizeof(cid));
	if (sd_known && memcmp(sd_cid, cid, sizeof(cid))) return -1;

	// Send RCA
	retv = sd_cmd(CMD_03, 0x00000000);
	if(retv)
		return retv;
	u32 rca = cmd_resp[0] >> 16;
	if (!rca) return -1;

	// Send CSD
	retv = sd_cmd(CMD_09, rca<<16);
	if (retv) return retv;

	// Select CARD
	retv = sd_cmd(CMD_07, rca<<16);
	if (retv) return retv;
	sd_clk_set(SDCLK_HIGH);

	// Set Block Len
	retv = sd_cmd(CMD_16, 512);
	if (retv) return retv;

	// Switch to 4bit
	retv = sd_acmd(rca, CMD_06, 2);
	if(retv)
		return retv;
	sd_bus_width(4);

	// Card send its status
	retv = sd_cmd(CMD_13, rca<<16);
	if(retv)
		return retv;
	osDelay(1);

	/* Also reject a removal sampled by another task during identification. */
	if (!sd_card_insert() || sd_removed) return -1;
	if (!sd_known) {
		memcpy(sd_cid, cid, sizeof(sd_cid));
		sd_known = 1;
	}
	is_sdhc = high_capacity;
	__DMB();
	sd_rca = rca;
	return 0;
}

/******************************************************************************/


void sd_rw_test(void)
{
	int i;

	printk("\nread test ...\n");
	memset(sdbuf, 0, 1024);
	sd_read_blocks(0, 2, sdbuf);
	hex_dump("read", sdbuf, 512*2);

	memset(sdbuf, 0, 1024);
	for(i=0; i<768; i++){
		sdbuf[i] = i;
	}
	printk("\nwrite test ...\n");
	sd_write_blocks(1, 1, sdbuf);

	printk("\nread test ...\n");
	memset(sdbuf, 0, 1024);
	sd_read_blocks(0, 3, sdbuf);
	hex_dump("read", sdbuf, 512*3);

}

int sdio_init(void)
{
	sd_rca = 0;
	sd_pending_write = 0;
	/* Preserve the initial falling-edge clock configuration. */
	sd_reset_controller((1u << 16) | SDCLK_LOW, 0);

	cmd_sem = osSemaphoreNew(1, 0, NULL);
	if (!cmd_sem) return -1;
	NVIC_SetPriority(SDIO_IRQn, 6);
	NVIC_EnableIRQ(SDIO_IRQn);

	if (sd_card_insert()) {
		printk("SDCARD insert!\n");
		return sdcard_identify();
	}
	printk("No SDCARD insert!\n");
	return -1;
}


int sdio_reset(void)
{
	sd_rca = 0;
	if (!cmd_sem || !sd_card_insert()) return -1;
	sd_pending_write = 0;
	printk("\nSDIO reset ...\n");

	/* Preserve reinsertion's reset hold and initial clock phase. */
	sd_reset_controller(SDCLK_LOW, 10);

	int retv = sdcard_identify();
	printk("sdcard_identify: %d\n", retv);

	return retv;
}


/******************************************************************************/
