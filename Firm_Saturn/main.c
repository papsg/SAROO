/*
 * Sega Saturn cartridge flash tool
 * by Anders Montonen, 2012
 *
 * Original software by ExCyber
 * Graphics routines by Charles MacDonald
 *
 * Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
 */

#include "main.h"
#include "smpc.h"
#include "vdp2.h"

/**********************************************************/

u16 save_ack;
static u16 file_ack;
static u16 archive_ack;
static u16 menu_ack;
u32 mcu_ver;
u32 debug_flag;
int bios_type;

/**********************************************************/


u32 BE32(void *ptr)
{
	u8 *b = (u8*)ptr;
	return ((u32)b[0]<<24) | ((u32)b[1]<<16) | ((u32)b[2]<<8) | b[3];
}


u32 LE16(void *ptr)
{
	u8 *b = (u8*)ptr;
	return (b[1]<<8) | b[0];
}


u32 LE32(void *ptr)
{
	u8 *b = (u8*)ptr;
	return ((u32)b[3]<<24) | ((u32)b[2]<<16) | ((u32)b[1]<<8) | b[0];
}

void LE32W(void *ptr, u32 val)
{
	u8 *b = (u8*)ptr;
	b[0] = (val>> 0)&0xff;
	b[1] = (val>> 8)&0xff;
	b[2] = (val>>16)&0xff;
	b[3] = (val>>24)&0xff;
}


/**********************************************************/


static volatile int pad_state;

#ifndef PADMODE_DIRECT
/* Consume the SMPC's two port records without retaining a packet buffer. */
enum { PAD_PORT, PAD_ID, PAD_LENGTH, PAD_DATA };
static struct {
	u16 buttons;
	u8 phase, ports, devices, type, size, used;
} pad_stream;
static volatile int pad_result;
/* Last CONT issued by this menu; IREG0 is write-only (ST-169). */
static volatile u8 pad_cont;
static u8 pad_menu_owner;
static u8 pad_pages, pad_vblank_seen;
static u32 pad_deadline, pad_vblank_ready;

static void pad_interrupt(void);

static u32 pad_interrupt_lock(void)
{
#ifdef __sh__
	u32 sr;
	__asm__ volatile ("stc sr,%0" : "=r" (sr) :: "memory");
	__asm__ volatile ("ldc %0,sr" :: "r" (sr | 0xf0u) : "memory", "t");
	return sr;
#else
	return pad_test_interrupt_lock();
#endif
}

static void pad_interrupt_unlock(u32 sr)
{
#ifdef __sh__
	__asm__ volatile ("ldc %0,sr" :: "r" (sr) : "memory", "t");
#else
	pad_test_interrupt_unlock(sr);
#endif
}

static void pad_interrupt_enable(int enable)
{
#ifdef __sh__
	/* The BIOS owns the SCU vector table and cached interrupt mask. */
	void (*set_handler)(u32, void (*)(void)) = (void*)REG32(0x06000300);
	void (*change_mask)(u32, u32) = (void*)REG32(0x06000344);
	u32 sr = pad_interrupt_lock();
	if(enable){
		set_handler(0x47, pad_interrupt);
		change_mask(~0x80u, 0);
		/* System-manager priority is eight; retain all non-mask SR bits. */
		pad_interrupt_unlock((sr & ~0xf0u) | 0x70u);
	}else{
		change_mask(~0u, 0x80);
		set_handler(0x47, 0);
		/* Transfers of control historically leave menu interrupts disabled. */
		pad_interrupt_unlock(sr | 0xf0u);
	}
#else
	pad_test_interrupt_enable(enable, pad_interrupt);
#endif
}

void pad_suspend(void)
{
	pad_interrupt_enable(0);
	pad_menu_owner = 0;
	/* The next owner may change IREG0; retire this transaction. */
	pad_state = 0;
	pad_vblank_seen = 0;
}

void pad_resume(void)
{
	/* Resume with fresh admission, never an old owner's CONT history. */
	pad_state = 0;
	pad_vblank_seen = 0;
	pad_cont = 0;
	pad_menu_owner = 1;
	pad_interrupt_enable(1);
}

static int pad_next_device(void)
{
	if(--pad_stream.devices){
		pad_stream.phase = PAD_ID;
		return 0;
	}
	pad_stream.phase = PAD_PORT;
	return --pad_stream.ports == 0;
}

/* 1: both ports complete; 0: another byte needed; -1: invalid 15-byte mode. */
static int pad_parse_byte(u8 data)
{
	switch(pad_stream.phase){
	case PAD_PORT:
		pad_stream.devices = data & 0x0f;
		if((data & 0xf0)==0xf0 && pad_stream.devices>1)
			return -1;
		if(pad_stream.devices==0)
			return --pad_stream.ports == 0;
		pad_stream.phase = PAD_ID;
		break;
	case PAD_ID:
		pad_stream.type = data >> 4;
		/* Type F is an empty/unknown socket; its low nibble is not a size. */
		if(pad_stream.type==0x0f)
			return pad_next_device();
		pad_stream.size = data & 0x0f;
		pad_stream.used = 0;
		pad_stream.phase = pad_stream.size ? PAD_DATA : PAD_LENGTH;
		break;
	case PAD_LENGTH:
		/* Extended peripherals are truncated to 15 data bytes in this mode. */
		if(data!=15)
			return -1;
		pad_stream.size = data;
		pad_stream.phase = PAD_DATA;
		break;
	case PAD_DATA:
		/* Digital, analog and keyboard formats share these button fields. */
		if(pad_stream.size>=2 && (pad_stream.type==0 ||
		   pad_stream.type==1 || pad_stream.type==3)){
			if(pad_stream.used==0)
				pad_stream.buttons |= (u16)(data ^ 0xff) << 8;
			else if(pad_stream.used==1)
				pad_stream.buttons |= (data ^ 0xff) & 0xf8;
		}
		if(++pad_stream.used==pad_stream.size)
			return pad_next_device();
		break;
	}
	return 0;
}

static void pad_end(int result, int abort)
{
	if(abort){
		/* Keep CONT unchanged: CONT and BREAK together is prohibited. */
		IREG0 = pad_cont | 0x40;
	}
	pad_result = result;
	pad_state = 2;
}

/* Keep the frequent not-ready path free of the parser register-save frame. */
static int __attribute__((noinline)) pad_begin(void)
{
	memset(&pad_stream, 0, sizeof(pad_stream));
	pad_stream.ports = 2;
	pad_pages = 0;
	pad_deadline = get_timer() + MS2TICK(100);
	SF = 0x01;
	pad_cont = 0;
	IREG0 = pad_cont;
	IREG1 = 0x08;
	IREG2 = 0xf0;
	pad_state = 1;
	COMREG = 0x10;
	return -1;
}

static void pad_interrupt(void)
{
	if(pad_state!=1)
		return;
	u8 status = SR;
	if((status & 0x8f)!=0x80 ||
	   (status & 0x40)!=(pad_pages==0 ? 0x40 : 0)){
		pad_end(-1, 1);
		return;
	}
	++pad_pages;
	for(unsigned i=0; i<32; ++i){
		int result = pad_parse_byte(REG8(SMPC_BASE+0x21+i*2));
		if(result){
			int complete = result>0 && (status&0x20)==0;
			pad_end(complete ? pad_stream.buttons : -1, status&0x20);
			return;
		}
	}
	/* Two ports, at most 15 sockets each, 17 bytes per extended record. */
	if((status&0x20)==0 || pad_pages>=16){
		pad_end(-1, status&0x20);
		return;
	}
	pad_cont ^= 0x80;
	IREG0 = pad_cont;
}

/* The fixed conio_getc export is also used by loaded programs with the menu
 * IRQ removed. SF-clear/NPE-clear permits a complete single-page poll
 * (ST-169 Type-D note), without changing that program's vector or mask. */
static int __attribute__((noinline)) pad_poll_result(void)
{
	pad_interrupt();
	if(pad_state!=2)
		return -1;
	int result = pad_result;
	pad_state = 3;
	return result;
}
#else
void pad_suspend(void) {}
void pad_resume(void) {}
#endif

int pad_read(void)
{
#ifdef PADMODE_DIRECT
	u32 bits=0;
	PDR1 = 0x60; bits |= (PDR1 & 0x8) << 0;
	PDR1 = 0x40; bits |= (PDR1 & 0xf) << 8;
	PDR1 = 0x20; bits |= (PDR1 & 0xf) << 12;
	PDR1 = 0x00; bits |= (PDR1 & 0xf) << 4;
	return bits ^ 0xfff8;
#else
	if(pad_state==0){
		if((TVSTAT&0x0008)==0){
			pad_vblank_seen = 0;
			return -1;
		}
		if(!pad_vblank_seen){
            /* Preserve the previous nominal 90-tick delay in microseconds.
             * Sega ST-169-R1 requires at least 300 us after VBlank-in. */
            pad_vblank_seen = 1;
            pad_vblank_ready = get_timer() + 432;
			return -1;
		}
		u32 now = get_timer();
		/* If foreground work skipped an entire field, do not reuse an old
		 * deadline in a later VBlank. Ten ms is longer than either region's
		 * blanking interval and shorter than its field. */
		if(timer_reached(now, pad_vblank_ready + MS2TICK(10))){
			pad_vblank_seen = 0;
			return -1;
		}
		if(!timer_reached(now, pad_vblank_ready) || (SF&1))
			return -1;
		if((TVSTAT&0x0008)==0){
			pad_vblank_seen = 0;
			return -1;
		}
		return pad_begin();
	}else if(pad_state==1){
		if(timer_reached(get_timer(), pad_deadline)){
			u32 sr = pad_interrupt_lock();
			if(pad_state==1){
				/* Timeout has no value to publish; end this field directly. */
				IREG0 = pad_cont | 0x40;
				pad_state = 3;
			}
			pad_interrupt_unlock(sr);
		}
		if(pad_state==1 && !pad_menu_owner && !(SF&1) && !(SR&0x20))
			return pad_poll_result();
	}else if(pad_state==2){
		int result = pad_result;
		pad_state = 3;
		return result;
	}else if(pad_state==3){
		if((TVSTAT&0x0008)==0){
			pad_state = 0;
			pad_vblank_seen = 0;
		}
	}
	return -1;
#endif
}


void pad_init(void)
{
	pad_state = 0;

#ifdef PADMODE_DIRECT
    PDR1 = 0;
    DDR1 = 0x60;
    IOSEL = IOSEL1;
#else
	pad_vblank_seen = 0;
	DDR1 = 0;
	DDR2 = 0;
	EXLE = 0;
    IOSEL = 0;

	pad_cont = 0;
	IREG0 = pad_cont;
	IREG1 = 0x08;
	IREG2 = 0xf0;
	pad_resume();
#endif
}


/**********************************************************/

static int menu_cd_power(MENU_DESC *menu, int physical, int wait)
{
    int result = physical ? cdblock_on(wait) : cdblock_off();
    if (result) menu_error(menu, TT("游戏启动失败! %d"), result);
    return result;
}

/**********************************************************/


int menu_command_wait(void)
{
    u16 pending = SS_CMD;
    /* Legacy startup/preview leave the same opcode after completion. */
    if (pending && (menu_ack || (pending != SSCMD_STARTUP && pending != SSCMD_SELECT))) {
        u32 deadline = get_timer() + MS2TICK(10000);
        while (SS_CMD) {
            if (timer_reached(get_timer(), deadline)) return -15; /* FR_TIMEOUT */
        }
    }
    /* The prior owner has finished using the request before it clears SS_CMD. */
    __asm__ volatile ("" ::: "memory");
    return 0;
}

void stm32_puts(char *str)
{
	if (menu_command_wait()) return;
	u32 stm32_base = TMPBUFF_ADDR;
	int len;

	len = strlen(str);
	LE32W((void*)(stm32_base+0), len);

	memcpy((void*)stm32_base+0x10, str, len+1);

	__asm__ volatile ("" ::: "memory");
	SS_CMD = 0x0001;
	while(SS_CMD);
}


/**********************************************************/

/* The FPGA divides its 100 MHz clock by 100. Read both halves without
 * combining words from opposite sides of a carry. */
const int HZ = 1000000;

void reset_timer(void)
{
    /* A write to either half clears the counter. Do this only at entry. */
    SS_TIMER_HI = 0;
}

u32 get_timer(void)
{
    u32 high, low;
    do {
        high = SS_TIMER_HI;
        low = SS_TIMER_LO;
    } while (high != SS_TIMER_HI);
    return (high << 16) | low;
}

static void sleep_units(u32 count, u32 scale)
{
    u32 now = get_timer();
    u32 deadline = now;
    if (!count)
        count = 1;
    do {
        /* Each deadline advances by at most one billion ticks. */
        u32 chunk = count > 1000000 ? 1000000 : count;
        count -= chunk;
        deadline += chunk * scale;
        while (!timer_reached(now, deadline))
            now = get_timer();
    } while (count);
}

void usleep(u32 us)
{
    sleep_units(us, 1);
}

void msleep(u32 ms)
{
    sleep_units(ms, 1000);
}


/**********************************************************/

// clk = 26874880
// bps = ((clk/32)/n)/(BRR+1)

void sci_init(void)
{
	SCR = 0x00;     /* set TE,RE to 0 */
	SMR = 0x00;     /* set SMR to 8n1 async mode */
//	BRR = 0x15;     /* set BRR to 9600 bps */
//	BRR = 0x01;     /* set BRR to 104980 bps */
	BRR = 0x00;     /* set BRR to 209960 bps */
	SCR = 0xcd;     /* set TIE RIE MPIE TEIE CKE1 CKE0 */
	SCR = 0x31;     /* set TE RE to 1 */

	printk_putc = sci_putc;
}

void sci_putc(int ch)
{
	while((SSR&0x80)==0);
	TDR = ch;
	SSR &= 0x7f;
}

int sci_getc(int timeout)
{
	int ch;

	timeout *= 1500;
	while(timeout){
		if(SSR&0x40){
			ch = RDR;
			SSR &= 0xbf;
			return ch;
		}
		timeout -= 1;
	}

	return -1;
}


/**********************************************************/


void cpu_dmemcpy(void *dst, void *src, int size, int ch)
{
	int chcr = 0x5a11;

	if(((u32)src&0xfff8703f)==0x25800000){
		// CDBLOCK的DATA_PORT
		// CDBLOCK's DATA_PORT
		chcr &= 0xcfff;
	}

	while((CHCR0&0x03)==0x01);
	CHCR0 = 0;
	DMAOR = 1;

	SAR0 = (u32)src;
	DAR0 = (u32)dst;
	TCR0 = (size+3)>>2;
	CHCR0 = chcr;
}

#if 1
void scu_dmemcpy(void *dst, void *src, int size, int ch)
{
	volatile SCUDMA_REG *sdma = (SCUDMA_REG *)(SCU_DMA_BASE+ch*0x20);
	/* D1WT is bit 19. Level 1 does not follow the level-0 layout. */
	u32 mask = ch == 0 ? SDSTAT_LEVEL0_BUSY : ch == 1 ? 0x000a0100 : 0x00003000;
	while (SDSTAT & mask);

	sdma->src = (u32)src;
	sdma->dst = (u32)dst;
	sdma->count = size;
	sdma->addv = (((u32)dst&0x00f00000)==0) ? 0x102 : 0x101;
	sdma->mode = 0x07;
	sdma->enable = 0x101;
}

#else

void scu_dmemcpy(void *dst, void *src, int size, int ch)
{
	while(SDSTAT&0x10030); // 等待传输结束
	// Wait for transfer to complete

	SD0R = (u32)src;
	SD0W = (u32)dst;
	SD0C = size;
	SD0AD = (((u32)dst&0x00f00000)==0) ? 0x102 : 0x101;
	SD0MD = 0x07;
	SD0EN = 0x101;
}

#endif

/**********************************************************/

/* Physical limits for legacy whole-file calls; stricter binary destinations
   exclude BIOS work/vectors, menu/catalog/stack, and cover workspaces. */
static u32 file_capacity(u32 address, int binary)
{
    if (address & 0xc0000000u) return 0;
    address &= 0x1fffffffu;
    if (address >= 0x00200000 && address < 0x00300000) return 0x00300000 - address;
    if (!binary && address >= 0x02000000 && address < 0x03000000) {
        if (address >= 0x020c7000 && address < 0x020c7100) return 0;
        return (address < 0x020c7000 ? 0x020c7000 : 0x03000000) - address;
    }
    if (address >= (binary ? 0x06002000u : 0x06000000u) && address < 0x06100000) return 0x06100000 - address;
    if (address >= 0x020c7100 && address < 0x02400000) return 0x02400000 - address;
    if (address >= 0x02800000 && address < 0x03000000) return 0x03000000 - address;
    return 0;
}

/* The SH7604 has no external-write snooping. Invalidate after the MCU
   finishes, including a possibly modified prefix on an error reply. */
static void file_cache_purge(u32 address, u32 size, int mirrored)
{
    u32 end = address + size;
    if (!size) return;
    address &= ~15u;
    if (mirrored || end - address > 4096) {
        /* One purge covers RAM mirrors and bounds work on large reads.
           Disable before CP; preserve all mode bits and the caller's SR. */
        u32 sr;
        __asm__ volatile ("stc sr,%0" : "=r" (sr) :: "memory");
        __asm__ volatile ("ldc %0,sr" :: "r" (sr | 0xf0u) : "memory", "t");
        u8 mode = REG8(0xfffffe92u);
        REG8(0xfffffe92u) = mode & ~1u;
        REG8(0xfffffe92u) = (mode & ~1u) | 0x10u;
        REG8(0xfffffe92u) = mode;
        __asm__ volatile ("ldc %0,sr" :: "r" (sr) : "memory", "t");
    } else {
        for (; address < end; address += 16)
            REG32(address | 0x40000000u) = 0;
    }
    __asm__ volatile ("" ::: "memory");
}

/* The low mode bit is direction. Modes 0/1 are named files, and 2/3
 * and 4/5 address the two primary owners with a direct FPGA buffer. */
static int file_command(char *name, u32 offset, u32 size, void *buf, int mode)
{
    int archive = mode >> 1, writing = mode & 1;
    unsigned length = 0;
    if (!archive) {
        if (!name || (!buf && size)) return -19; /* FR_INVALID_PARAMETER */
        while (length < 0xf0 && name[length]) ++length;
        if (length == 0xf0) return -19;
    }

    u32 bus_addr = (u32)buf & 0x1fffffff;
    int direct = bus_addr >= 0x02000000 && bus_addr < 0x03000000;
    if (archive && size && !direct) return -19;
    u32 address = direct ? bus_addr : ((TMPBUFF_ADDR & 0x1fffffff) + 0x100);
    /* Stop the bounce buffer before the active cover workspace at 0x02400000. */
    u32 end = direct ? (address < 0x020c7000 ? 0x020c7000 : 0x03000000) : 0x02400000;
    if ((address >= 0x020c7000 && address < 0x020c7100) || size > end - address)
        return -19;
    int ready = menu_command_wait();
    if (ready) return ready;
    /* Capture the mapping before the command. Reset can change SS_CTRL. */
    if (!writing && direct && address < 0x02800000u &&
        address + size > 0x02400000u && (SS_CTRL & 0x3000u) == 0x2000u)
        direct = 2; /* Direct read that may write FPGA RAM mirrors. */
    LE32W((void*)(TMPBUFF_ADDR+0x00), offset);
    LE32W((void*)(TMPBUFF_ADDR+0x04), size);
    LE32W((void*)(TMPBUFF_ADDR+0x08), address);
    if (archive) LE32W((void*)(TMPBUFF_ADDR+0x0c), archive - 1);
    else memcpy((void*)(TMPBUFF_ADDR+0x10), name, length+1);
    if (writing && !direct && size) memcpy((void*)(TMPBUFF_ADDR+0x100), buf, size);
    /* Publish ordinary request/payload stores before the MMIO command. */
    __asm__ volatile ("" ::: "memory");
    int ack = archive ? archive_ack : file_ack;
    SS_ARG = 0;
    SS_CMD = archive ? SSCMD_ARCHIVERD + writing : (SSCMD_FILERD + writing) | ack;
    while (SS_CMD);
    /* Consume shared response/data only after external completion. */
    __asm__ volatile ("" ::: "memory");
    int result = (signed short)SS_ARG;
    u32 count = LE32((void*)(TMPBUFF_ADDR+0x04));
    if (result >= 0) {
        /* Require the negotiated result before using the reported count. */
        if (result != ack) result = -1;
        else result = (count > size || (archive && count != size)) ? -19 : (int)count;
    }
    if (!writing) {
        if (direct) {
            u32 affected = result < 0 ? size : (u32)result;
            int mirrored = direct == 2 && address + affected > 0x02400000u;
            file_cache_purge(address, affected, mirrored);
        } else if (result > 0) {
            memcpy(buf, (void*)(TMPBUFF_ADDR+0x100), result);
        }
    }
    return result;
}

/* Positive lengths read, negative lengths write, and (0, NULL, 0) syncs.
 * The MCU admits existing file extents. The caller owns the buffer and
 * determines when it may access the archive. */
int archive_command(unsigned device, u32 offset, void *buf, int length)
{
    u32 size = length < 0 ? 0u - (u32)length : (u32)length;
    if (!archive_ack) return -12; /* FR_NOT_ENABLED */
    if (device > 1 || size > 65536 ||
        (size ? !buf : (buf || offset)) ||
        (length < 0 && !device && offset < 65536)) return -19;
    return file_command(NULL, offset, size, buf, 2 + device * 2 + (length <= 0));
}

/* Find an existing slot using the primary owner's directory. The caller
 * supplies the read window, which may also contain the input key. */
int archive_find_slot(const u8 *key, void *workspace, u32 capacity)
{
    u8 wanted[16];
    if (!key || !key[0] || !workspace || capacity < 512 || capacity > 65536 ||
        (capacity & 511)) return -19;
    memcpy(wanted, (void *)key, sizeof(wanted));
    unsigned found = 0;
    for (u32 offset = 0; offset < 65536;) {
        u32 size = 65536 - offset;
        if (size > capacity) size = capacity;
        int result = archive_command(0, offset, workspace, size);
        if (result < 0) return result;
        u8 *entry = workspace;
        if (!offset && memcmp(entry, "Saroo Save File", 16)) return -2;
        for (u32 pos = offset ? 0 : 16; pos < size; pos += 16) {
            if (entry[pos] && !strncmp((char *)entry + pos, (char *)wanted, 16)) {
                if (found) return -2;
                found = offset + pos;
            }
        }
        offset += size;
    }
    return found ? (int)(found << 12) : -4; /* FR_NO_FILE */
}

int read_file_bounded(char *name, int offset, int size, void *buf, u32 capacity)
{
    if (offset < 0 || size < 0 || (u32)size > capacity || (!size && !capacity)) return -19;
    u32 requested = size ? (u32)size : capacity;
    int count = file_command(name, offset, requested, buf, 0);
    if (count < 0 || size || (u32)count < requested) return count;
    /* An exact-capacity whole-file read needs one byte of EOF evidence.
       Never issue the legacy unbounded size-zero command. */
    if ((u32)offset > 0x7fffffffu - requested) return -19;
    u8 extra;
    int tail = file_command(name, offset + requested, 1, &extra, 0);
    return tail < 0 ? tail : tail ? -19 : count;
}

/* Preserve the four-argument homebrew ABI at 0x02000f08. Whole-file callers
   must own the remaining permitted region; new callers should give capacity. */
int read_file(char *name, int offset, int size, void *buf)
{
    return read_file_bounded(name, offset, size, buf, size > 0 ? (u32)size : file_capacity((u32)buf, 0));
}

/* Preserve the four-argument homebrew ABI at 0x02000f0c. */
int write_file(char *name, int offset, int size, void *buf)
{
    if (size < 0) return -19;
    return file_command(name, offset, size, buf, 1);
}


/**********************************************************/


static u16 mksum(u32 addr)
{
	return (addr>>16) + addr;
}


int mem_test(int size)
{
	u32 addr0 = 0x22400000;
	u32 addr1 = 0x22600000;
	u16 rd, sum;
	int i;
	
	for(i=0; i<size; i++){
		*(u16*)(addr0) = mksum(addr0);
		addr0 += 2;
		*(u16*)(addr1) = mksum(addr1);
		addr1 += 2;
	}

	for(i=0; i<size; i++){
		addr0 -= 2;
		sum = mksum(addr0);
		rd  = *(u16*)(addr0);
		if(sum!=rd){
			printk("Mismatch0 at %08x! read=%08x calc=%08x\n", addr0, rd, sum);
			return -1;
		}
		*(u16*)(addr0) = ~sum;

		addr1 -= 2;
		sum = mksum(addr1);
		rd  = *(u16*)(addr1);
		if(sum!=rd){
			printk("Mismatch1 at %08x! read=%08x calc=%08x\n", addr1, rd, sum);
			return -2;
		}
		*(u16*)(addr1) = ~sum;
	}

	for(i=0; i<size; i++){
		sum = ~mksum(addr0);
		rd  = *(u16*)(addr0);
		if(sum!=rd){
			printk("Mismatch2 at %08x! read=%08x calc=%08x\n", addr0, rd, sum);
			return -3;
		}
		addr0 += 2;

		sum = ~mksum(addr1);
		rd  = *(u16*)(addr1);
		if(sum!=rd){
			printk("Mismatch3 at %08x! read=%08x calc=%08x\n", addr1, rd, sum);
			return -4;
		}
		addr1 += 2;
	}

	return 0;
}


/**********************************************************/

static int last_w, last_h, last_f;
static int cover_top = 1;

void gif_timer(void)
{
	if((TVSTAT&0x0008)==0)
		return;

	int gif_flag = *(u8*)0x22400000;
	if(gif_flag){
		u8 *dst = (u8*)(VDP2_VRAM+0x20000);
		u8 *src = (u8*)(0x22401000);

		int img_w = LE16((u8*)0x22400004);
		int src_llen = (gif_flag>=3)? img_w : 512;
		int img_h = LE16((u8*)0x22400006);
		int img_x = (short)LE16((u8*)0x22400008);
		int img_y = (short)LE16((u8*)0x2240000a);
		int adj_x = (short)LE16((u8*)0x2240000c);
		int adj_y = (short)LE16((u8*)0x2240000e);
		//printk("img: (%d,%d) %dx%d\n", img_x, img_y, img_w, img_h);

		//if( (last_f!=gif_flag) ){
		//	printk("gflag: %d\n", gif_flag);
		//}
		if( (last_f!=gif_flag) || (gif_flag>=3 && (last_w>img_w || last_h>img_h)) ){
			memset(dst, 0, 240*512);
		}
		last_f = gif_flag;
		last_w = img_w;
		last_h = img_h;

		src += img_y*src_llen+img_x;

		img_x += adj_x;
		if(img_x<0){
			src -= img_x;
			img_w += img_x;
			img_x = 0;
		}
		if(img_x+img_w>fbw){
			img_w = fbw-img_x;
		}

		img_y += adj_y;
		if(img_y<0){
			src -= img_y*src_llen;
			img_h += img_y;
			img_y = 0;
		}
		if(img_y+img_h>fbh){
			img_h = fbh-img_y;
		}

#if 1
		if(cover_top){
			if(gif_flag==3){
				PRINA = 0x0706;
				CCCTL = 0x0000;
				BGON  = 0x0203;
				vdp2_win0(1, 1, img_x, img_y, img_x+img_w-1, img_y+img_h-1);
			}else{
				vdp2_win0(-1, 0, 0, 0, 0, 0);
				PRINA = 0x0707;
				CCCTL = 0x0002;
				BGON  = 0x0003;
			}
		}
#endif

		// An empty clipped rectangle has no DMA rows.
		if(img_w>0 && img_h>0){
			dst += img_y*512+img_x;
			//int start = get_timer();
			for(int y=0; y<img_h; y++){
				scu_dmemcpy(dst, src, img_w, 2);
				dst += 512;
				src += src_llen;
			}
		}
		//int end = get_timer();
		//printk("gt: %d ms\n", TICK2MS(end-start));
		/* Return shared storage only after final channel-2 DMA completion. */
		while (SDSTAT & 0x3000);
		*(volatile u8*)0x22400000 = 0;
	}

	int pal_flag = *(u8*)0x22400002;
	if(pal_flag){
		int i;
		u8 *pal = (u8*)0x22400100;
		for(i=0; i<256; i++){
			nbg1_set_cram(i, pal[2], pal[1], pal[0]);
			pal += 4;
		}
		*(u8*)0x22400002 = 0;
	}
}


static void change_cover_layer(void)
{
	if(cover_top==0){
		vdp2_win0(-1, 0, 0, 0, 0, 0);
		PRINA = 0x0707;
		CCCTL = 0x0002;
		BGON  = 0x0003;
	}else if(last_f==3){
		PRINA = 0x0706;
		CCCTL = 0x0000;
		BGON  = 0x0203;
		int x = 176;
		int y = (last_h>128)? 24 : 88;
		/* Match the viewport clipping applied when gif_timer drew the cover. */
		int ex = x + last_w - 1;
		int ey = y + last_h - 1;
		vdp2_win0(1, 1, x, y, ex < fbw ? ex : fbw - 1,
		          ey < fbh ? ey : fbh - 1);
	}
}

/**********************************************************/

static int total_disc, total_page, page;
static int sel_mode; // 0: game, 1: flat binary list, 2: binary folders
enum { BIN_FOLDER_MODE = 2 };

MENU_DESC sel_menu;

static char *bin_catalog_name(int index, int *directory)
{
    char *catalog = (char *)IMGINFO_ADDR;
    u32 count = LE32(catalog);
    if (count > (0x2800u - 4) / 4 || index < 0 || (u32)index >= count) return NULL;
    u32 entry = LE32(catalog + 4 + index * 4);
    u32 offset = entry & ~BIN_DIRECTORY_ENTRY;
    if (offset < 0x2800 || offset >= 0x23000) return NULL;
    unsigned length = 0;
    while (length < 256 && length < 0x23000u - offset && catalog[offset + length])
        ++length;
    if (!length || length == 256 || length == 0x23000u - offset) return NULL;
    *directory = !!(entry & BIN_DIRECTORY_ENTRY);
    return catalog + offset;
}

static void fill_selmenu(void)
{
	int *disc_path = (int *)(IMGINFO_ADDR+0x04);
	char *path_str = (char*)(IMGINFO_ADDR);
	char tmp[128];
	int i;

	for(i=0; i<MENU_ITEMS; i++){
		int index = page*MENU_ITEMS+i;
		if(index>=total_disc)
			break;

		int directory = 0;
		char *path = sel_mode == BIN_FOLDER_MODE ? bin_catalog_name(index, &directory)
		                                       : path_str + LE32(&disc_path[index]);
		if (!path) {
			total_disc = total_page = sel_menu.num = 0;
			break;
		}
		snprintf(tmp, sizeof(tmp), directory ? "%2d: /%s" : "%2d: %s", index, path);
		tmp[127] = 0;
		if(sel_mode==0){
			// xxxx/xxxx.cue
			char *p = strchr(tmp, '/');
			if(p) *p = 0;
		}else if (!directory){
			// xxxx@xxxx
			char *p = strchr(tmp, '@');
			if(p) *p = 0;
		}

		add_menu_item(&sel_menu, tmp);
	}

	if (!sel_menu.num) sel_menu.items[0][0] = 0;
	int disp_page = (total_page)? page+1 : 0;
	if(sel_mode==0){
		snprintf(sel_menu.title, sizeof(sel_menu.title), TT("选择游戏(%d/%d)"), disp_page, total_page);
	}else{
		snprintf(sel_menu.title, sizeof(sel_menu.title), TT("选择文件(%d/%d)"), disp_page, total_page);
	}
}


static void select_notify(int index)
{
	if(sel_mode==0 && !menu_command_wait()){
		SS_ARG = page*MENU_ITEMS + index;
		SS_CMD = SSCMD_SELECT | menu_ack;
	}
}


static void page_update(int up)
{
	MENU_DESC *menu = &sel_menu;

	menu->num = 0;
	fill_selmenu();

	menu->current = (up && menu->num)? menu->num-1: 0;
	select_notify(menu->current);
	draw_menu_frame(menu);
}


static int load_binary_file(char *path, char *name, int run)
{
    u32 load_addr = 0x06004000;
    char *p = strchr(name, '@');
    if (p) {
        ++p;
        if (p[0]=='0' && (p[1]=='x' || p[1]=='X')) p += 2;
        char *start = p;
        load_addr = 0;
        for (;;) {
            unsigned digit = (unsigned char)*p;
            if (digit >= '0' && digit <= '9') digit -= '0';
            else if ((digit | 32) >= 'a' && (digit | 32) <= 'f') digit = (digit | 32)-'a'+10;
            else break;
            if (load_addr > 0x0fffffff) return -1;
            load_addr = (load_addr << 4) | digit;
            ++p;
        }
        if (p == start || (*p && *p != '.')) return -1;
    }
    u32 capacity = file_capacity(load_addr, 1);
    if (!capacity || (run && (load_addr & 1))) return -1;
    int retv = read_file_bounded(path, 0, 0, (void*)load_addr, capacity);
    if (retv < 0) return retv;
    if (run) {
        int offset = 0;
        if (retv >= 8 && !strncmp((char*)load_addr, "SEGA SEG", 8)) offset = 0x0f00;
        if (retv < offset + 2) return -1;
        printk("Jump to %08x ...\n", load_addr);
        void (*go)(void) = (void(*)(void))(load_addr+offset);
        pad_suspend();
        go();
        pad_resume();
    } else printk("Load to %08x ...\n", load_addr);
    return 0;
}

int run_binary(int index, int run)
{
    char *catalog = (char*)IMGINFO_ADDR;
    u32 count = LE32(catalog);
    if (count > (0x2800-4)/4 || index < 0 || (u32)index >= count) return -1;
    u32 name_offset = LE32(catalog+4+index*4);
    if (name_offset < 0x2800 || name_offset >= 0x23000) return -1;
    char *name = catalog + name_offset;
    char bin_name[128];
    unsigned length = 0;
    while (length < 0x23000-name_offset && length <= sizeof(bin_name)-sizeof("/SAROO/BIN/") && name[length])
        ++length;
    if (length == 0x23000-name_offset || length > sizeof(bin_name)-sizeof("/SAROO/BIN/")) return -1;
    sprintf(bin_name, "/SAROO/BIN/%s", name);
    return load_binary_file(bin_name, name, run);
}


static int sel_handle(int ctrl)
{
	MENU_DESC *menu = &sel_menu;

	total_disc = LE32((u8*)(IMGINFO_ADDR));
	total_page = (total_disc+MENU_ITEMS-1)/MENU_ITEMS;

	if(BUTTON_DOWN(ctrl, PAD_UP)){
		if(menu->current>0){
			select_notify(menu->current-1);
			menu_update(menu, menu->current-1);
		}else if(page>0){
			page -= 1;
			page_update(1);
		}else if(total_page>1){
			page = total_page-1;
			page_update(1);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_DOWN)){
		if(menu->current<(menu->num-1)){
			select_notify(menu->current+1);
			menu_update(menu, menu->current+1);
		}else if((page+1)<total_page){
			page += 1;
			page_update(0);
		}else if(total_page>1){
			page = 0;
			page_update(0);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_LT)){
		if(total_page>1){
			page -= 1;
			if(page<0){
				page = total_page-1;
			}
			page_update(0);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_RT)){
		if(total_page>1){
			page += 1;
			if(page>=total_page){
				page = 0;
			}
			page_update(0);
		}
	}else if(BUTTON_DOWN(ctrl, PAD_A) && (total_page>0)){
		int index = page*MENU_ITEMS + menu->current;
		int retv;

		if(sel_mode==0){
			retv = menu_command_wait();
			if (retv) {
				menu_error(menu, TT("游戏启动失败! %d"), retv);
				return 0;
			}
			menu_status(menu, TT("游戏启动中......"));

			SS_ARG = index;
			SS_CMD = SSCMD_LOADDISC;
			while(SS_CMD);

			retv = (signed short)SS_ARG;
			if(retv==0){
				retv = bios_cd_cmd(0);
			}
			if(retv){
				menu_error(menu, TT("游戏启动失败! %d"), retv);
			}
		}else{
            if (menu_cd_power(menu, 0, 0)) return 0;
			menu_status(menu, TT("加载文件中......"));
			retv = run_binary(index, 1);
			if(retv){
				menu_error(menu, TT("文件加载失败! %d"), retv);
			}
		}
	}else if(BUTTON_DOWN(ctrl, PAD_Z)){
		int index = page*MENU_ITEMS + menu->current;
		int retv;
        if (menu_cd_power(menu, sel_mode != 0, 0)) return 0;

		if(sel_mode==0){
			int index = page*MENU_ITEMS + menu->current;

			retv = menu_command_wait();
			if (retv) {
				menu_error(menu, TT("游戏启动失败! %d"), retv);
				return 0;
			}
			SS_ARG = index;
			SS_CMD = SSCMD_LOADDISC;
			while(SS_CMD);

            retv = (signed short)SS_ARG;
            if (retv) {
                menu_error(menu, TT("游戏启动失败! %d"), retv);
            } else {
                use_sys_load = 0;
                my_cdplayer();
            }
		}else{
			menu_status(menu, TT("加载文件中......"));
			retv = run_binary(index, 1);
			if(retv){
				menu_error(menu, TT("文件加载失败! %d"), retv);
			}
		}
	}else if(BUTTON_DOWN(ctrl, PAD_LEFT)){
		cover_top ^= 1;
		change_cover_layer();
	}else if(BUTTON_DOWN(ctrl, PAD_C)){
		return MENU_EXIT;
	}

	return 0;
}


void select_game(void)
{
	total_disc = LE32((u8*)(IMGINFO_ADDR));
	total_page = (total_disc+MENU_ITEMS-1)/MENU_ITEMS;
	page = 0;

	memset(&sel_menu, 0, sizeof(sel_menu));
	sel_menu.handle = sel_handle;

	fill_selmenu();
	select_notify(sel_menu.current | SELECT_ENTER);

	menu_run(&sel_menu);

	select_notify(SELECT_EXIT);
}


/**********************************************************/


enum { BIN_IDLE, BIN_WAIT, BIN_LISTING, BIN_RESTORING, BIN_FAULT };
enum { BIN_CURRENT = -1, BIN_PARENT = -2 };

typedef struct {
    int phase, target, error;
    u32 next_poll, slow_at;
    u8 listed, leave, slow;
} BIN_BROWSER;

static char bin_path[BIN_PATH_CAPACITY];

static char *bin_current_path(void)
{
    if (!bin_path[0]) memcpy(bin_path, BIN_ROOT, sizeof(BIN_ROOT));
    return bin_path;
}

static int bin_append(char *path, char *name)
{
    unsigned base = strlen(path), length = strlen(name);
    if (!length || name[0] == '.' || name[length - 1] == '.' || name[length - 1] == ' ' ||
        base >= BIN_PATH_CAPACITY || length >= BIN_PATH_CAPACITY - base - 1) return -1;
    for (unsigned i = 0; i < length; ++i) {
        unsigned c = (unsigned char)name[i];
        if (c < ' ' || strchr("/\\:*?\"<>|\x7f", c)) return -1;
    }
    path[base] = '/';
    memcpy(path + base + 1, name, length + 1);
    return 0;
}

static int bin_prepare_path(int target)
{
    char *request = (char *)(TMPBUFF_ADDR + 0x10);
    char *current = bin_current_path();
    unsigned length = strlen(current);
    memcpy(request, current, length + 1);
    if (target == BIN_PARENT) {
        if (length <= sizeof(BIN_ROOT) - 1) return -1;
        while (request[--length] != '/');
        request[length] = 0;
    } else if (target >= 0) {
        int directory;
        char *name = bin_catalog_name(target, &directory);
        if (!name || !directory) return -1;
        return bin_append(request, name);
    }
    return 0;
}

static void bin_draw(unsigned count)
{
    total_disc = count <= (0x2800u - 4) / 4 ? count : 0;
    total_page = (total_disc + MENU_ITEMS - 1) / MENU_ITEMS;
    page = sel_menu.num = sel_menu.current = 0;
    fill_selmenu();
    draw_menu_frame(&sel_menu);
}

static void bin_schedule(BIN_BROWSER *browser, int target)
{
    browser->target = target;
    browser->phase = BIN_WAIT;
    browser->slow = 0;
    browser->next_poll = get_timer();
    browser->slow_at = browser->next_poll + MS2TICK(5000);
    bin_draw(0);
    menu_status(&sel_menu, TT("加载文件中......"));
}

/* Return to the main menu only after the request and any catalog restore retire. */
static int bin_step(BIN_BROWSER *browser)
{
    int phase = browser->phase;
    if (phase == BIN_IDLE || phase == BIN_FAULT) return 0;
    u32 now = get_timer();
    if (!timer_reached(now, browser->next_poll)) return 0;
    browser->next_poll = now + MS2TICK(1);
    if (!browser->slow && timer_reached(now, browser->slow_at)) {
        browser->slow = 1;
        menu_status(&sel_menu, "Still reading... B/C: go back");
    }
    if (SS_CMD) return 0;
    __asm__ volatile ("" ::: "memory");

    if (phase == BIN_WAIT) {
        if (browser->leave) {
            if (!browser->listed) return 1;
            SS_ARG = 0;
            SS_CMD = SSCMD_LISTDISC | SSCMD_SAVE_ACK;
            browser->phase = BIN_RESTORING;
        } else if (bin_prepare_path(browser->target)) {
            browser->phase = BIN_IDLE;
            bin_draw(LE32((void *)IMGINFO_ADDR));
            menu_error(&sel_menu, TT("文件加载失败! %d"), -1);
        } else {
            SS_ARG = 0;
            __asm__ volatile ("" ::: "memory");
            SS_CMD = SSCMD_BINDIR;
            browser->listed = 1;
            browser->phase = BIN_LISTING;
        }
        return 0;
    }

    int wire = (signed short)SS_ARG;
    if (wire != SSCMD_SAVE_ACK && !(wire >= -19 && wire < 0)) {
        browser->phase = BIN_FAULT;
        menu_status(&sel_menu, "MCU response lost. Reset Saturn.");
        return 0;
    }
    int result = wire == SSCMD_SAVE_ACK ? 0 : wire;
    u32 count = LE32((void *)IMGINFO_ADDR);
    if (phase == BIN_LISTING && count > (0x2800u - 4) / 4 && !result) result = -1;
    if (phase == BIN_RESTORING) {
        if (!result) return 1;
        browser->leave = 0;
    } else if (browser->leave) {
        bin_schedule(browser, BIN_CURRENT);
        return 0;
    } else if (result && browser->target != BIN_CURRENT) {
        browser->error = result;
        bin_schedule(browser, BIN_CURRENT);
        return 0;
    } else if (!result) {
        char *request = (char *)(TMPBUFF_ADDR + 0x10);
        unsigned length = 0;
        while (length < BIN_PATH_CAPACITY && request[length]) ++length;
        if (length < BIN_PATH_CAPACITY) memcpy(bin_path, request, length + 1);
        else result = -1;
    }
    browser->phase = BIN_IDLE;
    bin_draw(result ? 0 : count);
    int error = result ? result : browser->error;
    if (error) menu_error(&sel_menu, TT("文件加载失败! %d"), error);
    return 0;
}

static void bin_input(BIN_BROWSER *browser, int ctrl)
{
    int back = BUTTON_DOWN(ctrl, PAD_B) || BUTTON_DOWN(ctrl, PAD_C);
    if (browser->phase != BIN_IDLE) {
        if (browser->phase != BIN_FAULT && back && !browser->leave) {
            browser->leave = 1;
            menu_status(&sel_menu, "Returning when reading finishes...");
        }
        return;
    }

    browser->error = 0;
    if (back) {
        browser->leave = BUTTON_DOWN(ctrl, PAD_B) ||
                         strlen(bin_current_path()) == sizeof(BIN_ROOT) - 1;
        bin_schedule(browser, browser->leave ? BIN_CURRENT : BIN_PARENT);
    } else if (BUTTON_DOWN(ctrl, PAD_A) || BUTTON_DOWN(ctrl, PAD_Z)) {
        if (!sel_menu.num) {
            bin_schedule(browser, BIN_CURRENT);
            return;
        }
        int index = page * MENU_ITEMS + sel_menu.current, directory;
        char *name = bin_catalog_name(index, &directory);
        if (!name) {
            menu_error(&sel_menu, TT("文件加载失败! %d"), -1);
        } else if (directory) {
            bin_schedule(browser, index);
        } else {
            char path[BIN_PATH_CAPACITY];
            char *current = bin_current_path();
            memcpy(path, current, strlen(current) + 1);
            if (bin_append(path, name)) {
                menu_error(&sel_menu, TT("文件加载失败! %d"), -1);
                return;
            }
            int physical = !BUTTON_DOWN(ctrl, PAD_A) && BUTTON_DOWN(ctrl, PAD_Z);
            if (menu_cd_power(&sel_menu, physical, 0)) return;
            menu_status(&sel_menu, TT("加载文件中......"));
            int result = load_binary_file(path, name, 1);
            if (result) menu_error(&sel_menu, TT("文件加载失败! %d"), result);
        }
    } else if (sel_menu.num) {
        sel_handle(ctrl);
    }
}

static void bin_browser(void)
{
    BIN_BROWSER browser = {0};
    sel_mode = BIN_FOLDER_MODE;
    memset(&sel_menu, 0, sizeof(sel_menu));
    sel_menu.version = bin_current_path();
    bin_schedule(&browser, BIN_CURRENT);
    while (!bin_step(&browser)) {
        u32 ctrl = conio_getc();
        if (ctrl) bin_input(&browser, ctrl);
        else {
            menu_timer(&sel_menu);
            gif_timer();
        }
    }
    sel_mode = 0;
}

void select_bins(void)
{
	if (menu_ack && LE32((void *)(SYSINFO_ADDR + 0x34)) == BIN_DIRECTORY_VERSION) {
		bin_browser();
		return;
	}
	if (menu_command_wait()) return;
	SS_CMD = SSCMD_LISTBINS;
	while(SS_CMD);

	select_game();

	if (menu_command_wait()) return;
	SS_ARG = 0;
	SS_CMD = SSCMD_LISTDISC;
	while(SS_CMD);
}


/**********************************************************/


MENU_DESC category_menu;


static int category_handle(int ctrl)
{
	MENU_DESC *menu = &category_menu;

	if(menu_default(menu, ctrl))
		return 0;

	if(BUTTON_DOWN(ctrl, PAD_A)){
		int index = menu->current;

		if (menu_command_wait()) return 0;
		SS_ARG = index;
		SS_CMD = SSCMD_LISTDISC;
		while(SS_CMD);

		select_game();
		return MENU_RESTART;
	}else if(BUTTON_DOWN(ctrl, PAD_C)){
		return MENU_EXIT;
	}

	return 0;
}


void select_category(void)
{
	int category_num = *(u8*)(SYSINFO_ADDR+0x0c);
	if(category_num==0)
		return select_game();

	memset(&category_menu, 0, sizeof(category_menu));
	category_menu.handle = category_handle;

	int i;
	for(i=0; i<category_num; i++){
		char *cat_name = (char*)(SYSINFO_ADDR+0x0e80+i*32);
		add_menu_item(&category_menu, cat_name);
	}

	strcpy(category_menu.title, TT("选择游戏分类") );
	menu_run(&category_menu);
}



/**********************************************************/


char *menu_str[] = {
	"选择游戏",
	"系统CD播放器",
	"运行光盘游戏",
	"运行二进制文件",
	"串口调试工具",
};
int menu_str_nr = sizeof(menu_str)/sizeof(char*);
char *update_str = "固件升级";
int update_index;

MENU_DESC main_menu;


int main_handle(int ctrl)
{
	int index = main_menu.current;

	if(menu_default(&main_menu, ctrl))
		return 0;

	if(BUTTON_DOWN(ctrl, PAD_LT)){
		lang_next();
		return MENU_EXIT;
	}

	if(!BUTTON_DOWN(ctrl, PAD_A) && !BUTTON_DOWN(ctrl, PAD_C))
		return 0;
	if(BUTTON_DOWN(ctrl, PAD_C) && index!=2)
		return 0;

	if(index==0){
		sel_mode = 0;
		select_category();
		return MENU_RESTART;
	}else if(index==1){
        if (menu_cd_power(&main_menu, 1, 0)) return 0;
		use_sys_bup = 1;
		use_sys_load = 1;
		my_cdplayer();
		return MENU_RESTART;
	}else if(index==2){
		int type = 4;
		menu_status(&main_menu, TT("检查光盘中......"));
        if (menu_cd_power(&main_menu, 1, 1)) return 0;
		//int retv = cdblock_check();
		int retv = jhl_auth_hack(100000);
		if(retv<0){
			menu_status(&main_menu, TT("未发现光盘!"));
            menu_cd_power(&main_menu, 0, 0);
			return 0;
		}else if(retv==1){
			// 是Audio CD
			// Is Audio CD
			use_sys_bup = 1;
			use_sys_load = 1;
			my_cdplayer();
		}else if(retv==2){
			type = 2;
		}else if(retv==3){
			// 是刻录游戏CD
			// Is burned game CD
			retv = jhl_auth_hack(100000);
			if(retv!=2){
				menu_error(&main_menu, TT("不是游戏光盘!%d"), retv);
                menu_cd_power(&main_menu, 0, 0);
				return 0;
			}
			type = 2;
		}else if(retv!=4){
			// 是数据CD
			// Is data CD
			menu_error(&main_menu, TT("不是游戏光盘!%d"), retv);
            menu_cd_power(&main_menu, 0, 0);
			return 0;
		}

		// 停止正在播放的背景音乐。
		// Stop currently playing background music.
		retv = menu_command_wait();
		if (retv) {
			menu_error(&main_menu, TT("游戏启动失败! %d"), retv);
			menu_cd_power(&main_menu, 0, 0);
			return 0;
		}
		SS_ARG = 0xffff;
		SS_CMD = SSCMD_LOADDISC;

		menu_status(&main_menu, TT("游戏启动中......"));
		if(BUTTON_DOWN(ctrl, PAD_C)){
			type |= 0x80;
		}
		retv = bios_cd_cmd(type);
		if(retv){
			menu_error(&main_menu, TT("游戏启动失败! %d"), retv);
            menu_cd_power(&main_menu, 0, 0);
		}
		return 0;
	}else if(index==3){
		sel_mode = 1;
		select_bins();
		return MENU_RESTART;
	}else if(index==4){
		menu_status(&main_menu, "Open serial console ...");
		sci_init();
		sci_shell();
		return MENU_EXIT;
	}else if(index==update_index){
		if (menu_command_wait()) return 0;
		menu_status(&main_menu, TT("升级中,请勿断电..."));
		SS_ARG = 0;
		SS_CMD = SSCMD_UPDATE;
		while(SS_CMD);
		if(SS_ARG){
			menu_status(&main_menu, TT("升级失败!"));
		}else{
			menu_status(&main_menu, TT("升级完成,请重新开机!"));
		}
		while(1);
	}

	return 0;
}



int check_update(void)
{
	if (menu_command_wait()) return 0;
	SS_ARG = 0;
	SS_CMD = SSCMD_CHECK;
	while(SS_CMD);

	printk("check_update: %02x\n", SS_ARG);
	return SS_ARG;
}


static char ver_buf[64];

void menu_init(void)
{
	int i;

	nbg1_on();

	memset(&main_menu, 0, sizeof(main_menu));
	strcpy(main_menu.title, TT("SAROO Boot Menu"));

	for(i=0; i<menu_str_nr; i++){
		add_menu_item(&main_menu, TT(menu_str[i]));
	}
	if(check_update()){
		add_menu_item(&main_menu, TT(update_str));
		update_index = i;
	}

	main_menu.handle = main_handle;

	sprintf(ver_buf, "MCU:%06x        SS:%06x        FPGA:%02x", mcu_ver&0xffffff, get_build_date()&0xffffff, SS_VER&0xff);
	main_menu.version = ver_buf;

	menu_run(&main_menu);
}


/**********************************************************/

// Sega Saturn 1.00J : BTR_1.00D1940921  0
// Victor Saturn 1.00: BTR_1.00DJ941018  1
//
// Sega Saturn 1.00a : BTR_1.000U941115  2  40417:f4
// Sega Saturn 1.01a : BTR_1.000U941115  3  40417:54
// Samsung Saturn    : BTR_1.000U941115  4  40417:84
//
// Sega Saturn 1.01J : BTR_1.0191941228  5
// V-Saturn    1.01J : BTR_1.019J950703  6
// HI-Saturn   1.01J : BTR_1.019H950130  7
// HI-Saturn   1.02J : BTR_1.020H950519  8
// HI-Saturn   1.03  : BTRF1.030H950720  9
//
// Sega Saturn 1.003 : BTRD1.0032941012  10
//


static char *bvstr[12] = {"0D1","0DJ","00U","00U","00U","191","19J","19H","20H","30H","032",};

void detect_bios_type(void)
{
	int i;
	char *bv = (char*)0x0807;

	for(i=0; i<11; i++){
		if(strncmp(bv, bvstr[i], 3)==0){
			bios_type = i;
			break;
		}
	}
	if(i==11){
		bios_type = BIOS_UNKNOW;
		return;
	}

	if(bios_type==BIOS_100A){
		if(*(u8*)(0x40417)==0x54){
			bios_type = BIOS_101A;
		}else if(*(u8*)(0x40417)==0x84){
			bios_type = BIOS_SAMS;
		}
	}
}


/**********************************************************/


static int start_menu_services(void)
{
    menu_ack = LE32((void *)(SYSINFO_ADDR + 0x2c)) == MENU_ACK_VERSION ? SSCMD_SAVE_ACK : 0;
    int result = menu_command_wait();
    if (result) return result;
    SS_ARG = 0;
    SS_CMD = SSCMD_STARTUP | menu_ack;
    if (!menu_ack) return 0;
    result = menu_command_wait();
    if (result) return result;
    return (SS_ARG & menu_ack) ? 0 : -1;
}

int _main(void)
{
	reset_timer();
	/* SCSP requires word access. Set both CDDA inputs to -6 dB,
	 * with EXTS0 fully left and EXTS1 fully right. */
	REG16(0x25b00216) = (6u << 5) | 0x1f;
	REG16(0x25b00236) = (6u << 5) | 0x0f;
	// Master Volume
	*(u16*)(0x25b00400) =  0x020f;

	detect_bios_type();

	conio_init();
	if (start_menu_services()) {
		nbg1_on();
		printk("SAROO startup did not complete. Reset to retry.\n");
		return -1;
	}
	pad_init();

	printk("    SRAOOO start!\n");

    save_ack = LE32((void *)(SYSINFO_ADDR + 0x10)) == SAVE_ACK_VERSION ? SSCMD_SAVE_ACK : 0;
    file_ack = LE32((void *)(SYSINFO_ADDR + 0x14)) == FILE_ACK_VERSION ? SSCMD_SAVE_ACK : 0;
    archive_ack = LE32((void *)(SYSINFO_ADDR + 0x18)) == ARCHIVE_IO_VERSION ? SSCMD_SAVE_ACK : 0;
	mcu_ver = LE32((void*)(SYSINFO_ADDR+0x00));
	lang_id = LE32((void*)(SYSINFO_ADDR+0x04));
	debug_flag = LE32((void*)(SYSINFO_ADDR+0x08));

	// restore bios function
	*(u32*)(0x06000288) = 0x000018a8;  // bios_loadcd_boot
	*(u32*)(0x0600028c) = 0x00001874;  // bios_loadcd_readip
	*(u32*)(0x0600029c) = 0x00001904;  // bios_loadcd_init
	*(u32*)(0x060002dc) = *(u32*)(0x2000111c);

	*(u32*)(0x02000f04) =  (u32)cdc_read_sector;
	*(u32*)(0x02000f08) =  (u32)read_file;
	*(u32*)(0x02000f0c) =  (u32)write_file;
    /* Optional bounded-reader ABI v1; original four-argument entries stay intact. */
    *(u32*)(0x02000f10) = (u32)read_file_bounded;
    *(u32*)(0x02000f14) = 0x53464231; /* SFB1 */
	*(u32*)(0x02000f24) =  (u32)conio_getc;
	*(u32*)(0x02000f30) =  (u32)bios_cd_cmd;
	*(u32*)(0x02000f34) =  (u32)page_update;
	*(u32*)(0x02000f38) =  (u32)sci_init;
	*(u32*)(0x02000f3c) =  (u32)printk;

	sys_bup_init = (void*)*(u32*)(0x20000958);

	if(debug_flag&0x0001){
		// sci_putc
		sci_init();
		printk("\n\nSAROOO Serial Monitor Start!\n");
		printk("   MCU ver: %08x\n", mcu_ver);
		printk("    SS ver: %08x\n", get_build_date());
		printk("  FPGA ver: %08x\n", SS_VER);
		printk("   lang_id: %d\n", lang_id);
		printk(" bios_type: %d\n", bios_type);
	}else if(debug_flag&0x0004){
		// conio_putc
	}else if(debug_flag&0x0008){
		// output to mcu
		to_stm32 = 1;
	}else{
		printk_putc = NULL;
	}

    /* Keep the menu available. Each virtual launch verifies CD takeover. */
    cdblock_off();

	if((debug_flag&0x0003)==0x0003){
		sci_shell();
	}

	lang_init();
	while(1){
		menu_init();
	}

	return 0;
}


