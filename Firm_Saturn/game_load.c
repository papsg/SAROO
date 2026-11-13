
#include "main.h"
#include "smpc.h"


/**********************************************************/

static int (*cdp_boot_game)(void);
static int sk0, sk1;
static int bios_ver;
static int disc_type;


void (*sys_bup_init)(u8*, u8*, void*);
int use_sys_load = 0;

static void (*orig_func)(void);

int my_bios_loadcd_read(void);
int my_bios_loadcd_init(void);


/**********************************************************/


static int ipstat = -1;


static int cdp_auth(void)
{
    if (ipstat == -2) goto power_reset;
	if(ipstat<0){
		ipstat = 0;
	}

	int cdstat = (CR1>>8)&0x0f;
	printk("cdp_auth: ipstat=%d  cdstat=%02x\n", ipstat, cdstat);

	if(cdstat==STATUS_OPEN || cdstat==STATUS_NODISC){
		// 开盖了
		// The next medium needs a fresh session.
		ipstat = 0;
		return -2;
	}
	if(cdstat>=STATUS_RETRY){
		// 发生错误，复位重试
		// Error occurred, reset and retry
		ipstat = 0;
	}

	if(ipstat==0){
power_reset:
        /* Keep the power retry pending until both commands complete. */
        ipstat = -2;
        if (cdblock_off() || cdblock_on(0)) return -1;
		ipstat = 1;
	}else if(ipstat==1){
		// 等待cdblock进入工作状态
		// Wait for cdblock to enter working state
		if(cdstat==STATUS_PAUSE){
			if(my_bios_loadcd_init()){
				ipstat = 0;
				return -1;
			}
			ipstat = 2;
		}
	}else if(ipstat==2){
		// 等待reset_selector完成
		// Wait for reset_selector to complete
		if(cdstat==STATUS_PAUSE){
			ipstat = 3;
		}
	}else if(ipstat>=3 && ipstat<11){
		// 执行伪认证，重试8次
		// Try the existing authentication operation up to eight times.
		int retv = jhl_auth_hack(100000);
		if(retv==2){
			ipstat = 12;
		}else{
			ipstat += 1;
		}
	}else if(ipstat==11){
		// 认证失败
		// Authentication failed
		ipstat = 0;
		return -1;
	}else if(ipstat>=12 && ipstat<16){
		// 读IP，重试4次
		// Try reading the initial program up to four times.
		int retv = my_bios_loadcd_read();
		if(retv==0){
			ipstat = 0;
			return 0;
		}
		ipstat += 1;
	}else{
		// 读IP失败
		// Failed to read IP
		ipstat = 0;
		return -2;
	}

	return 1;
}


/**********************************************************/


static void cdp_hook(void)
{
	sk0 = *(u16*)0x06020232;
	sk1 = *(u16*)0x06020236;
	orig_func();
}


static void hook_getkey(void)
{
	if(bios_ver==0){
		if(*(u32*)(0x0604cd18) == 0x0604406c){
			orig_func = (void*)0x0604406c;
			*(u32*)(0x0604cd18) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604ccf4) == 0x0604406c){
			// V-Saturn 1.00
			orig_func = (void*)0x0604406c;
			*(u32*)(0x0604ccf4) = (u32)cdp_hook;
		}
	}else if(bios_ver==1){
		if(*(u32*)(0x0604d8e0) == 0x06044204){
			orig_func = (void*)0x06044204;
			*(u32*)(0x0604d8e0) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604d8b0) == 0x06044204){
			// V-Saturn 1.01
			orig_func = (void*)0x06044204;
			*(u32*)(0x0604d8b0) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604ecec) == 0x06044ae0){
			// 1.00a(U)
			orig_func = (void*)0x06044ae0;
			*(u32*)(0x0604ecec) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604ed0c) == 0x06044ae0){
			// 1.01a(U)
			orig_func = (void*)0x06044ae0;
			*(u32*)(0x0604ed0c) = (u32)cdp_hook;
		}else if(*(u32*)(0x0604ed28) == 0x06044ae0){
			// Samsung-Saturn
			orig_func = (void*)0x06044ae0;
			*(u32*)(0x0604ed28) = (u32)cdp_hook;
		}
	}else{
		printk("Unkonw BIOS ver!\n");
	}
}


static int cdp_boot(void)
{
	int pad = (sk0)? sk0 : sk1;
	printk("\ncdp_boot! PAD=%04x\n", pad);

	hook_getkey();

	if(pad & PAD_START){
		void (*go)(void) = (void*)(0x02000f00);
		go();
	}

	if(pad & PAD_C){
		// SAROO启动，但用系统存档
		// SAROO boot, but use system save
		bios_cd_cmd(disc_type|0x80);
	}else if(pad & PAD_A){
		// SAROO启动
		// SAROO boot
		bios_cd_cmd(disc_type);
	}

	return 0;
}



static int cdp_read_ip(void)
{
	if(debug_flag&1)
		sci_init();
	hook_getkey();

	printk("\ncdp_read_ip!\n");

	if(disc_type==0){
		// SAROO ISO
		int (*go)(void) = (void*)0x1874;
		int status = go();
		if((status==-8)||(status==-4)){
			status = 0;
		}
		return status;
	}else{
		// 光盘
		// Optical disc
		return cdp_auth();
	}
}


static int cdp_init(void)
{
    if (disc_type && ipstat == -2) return 0;
	printk("\ncdp_init! HIRQ=%04x STAT=%04x\n", HIRQ, CR1);

	if(disc_type==0){
		int (*go)() = (void*)0x1904;
		return go();
	}

	if(HIRQ&HIRQ_DCHG){
		ipstat = 0;
	}else{
		ipstat = 12;
	}

	return 0;
}


/**********************************************************/


static void (*orig_0344)(int, int);

static void my_0344(int and, int or)
{
	hook_getkey();
	orig_0344(and, or);
}


// 调用04c8的代码必须在内部RAM中运行。
// Code that calls 04c8 must run in internal RAM.
static void _call_04c8(void)
{
	void (*go)(void) = (void*)(0x04c8);
	go();
	scu_set_asr0(0x38803880);
}

void my_cdplayer(void)
{
	void (*go)(int);
	pad_suspend();

	if(debug_flag&1) sci_init();
	printk("\nLoad my multiPlayer ...\n");


	/* Disable the cache before purging it, then enable normal four-way mode. */
	REG8(0xfffffe92u) = 0x00;
	REG8(0xfffffe92u) = 0x10;
	REG8(0xfffffe92u) = 0x01;

	*(u32*)(0xffffff40) = 0x00;
	*(u32*)(0xffffff44) = 0x00;
	*(u16*)(0xffffff48) = 0x00;
	*(u32*)(0xffffff60) = 0x00;
	*(u32*)(0xffffff64) = 0x00;
	*(u16*)(0xffffff68) = 0x00;	
	*(u32*)(0xffffff70) = 0x00;
	*(u32*)(0xffffff74) = 0x00;
	*(u16*)(0xffffff78) = 0x00;	

	/* Stop both channels and read sticky flags before clearing them. */
	(void)DMAOR;
	DMAOR = 0;
	(void)CHCR0;
	(void)CHCR1;
	CHCR0 = 0;
	CHCR1 = 0;
	DRCR0 = 0;
	DRCR1 = 0;
	SAR0 = 0;
	DAR0 = 0;
	TCR0 = 1;
	SAR1 = 0;
	DAR1 = 0;
	TCR1 = 1;

	*(u32*)(0xffffff08) = 0x00;

	go = (void*)*(u32*)(0x060002dc);
	go(3);


	*(u32*)(0x06000348) = 0xffffffff;

	memcpy((u8*)0x00280000, _call_04c8, 64);
	go = (void*)0x00280000;
	go(0);

	if(debug_flag&1) sci_init();
	(void)DMAOR;
	DMAOR = 0;
	/* Confirm reset-button NMI is disabled before replacing high work RAM. */
	if (smpc_disable_reset()) {
		printk("SMPC reset-disable failed\n");
		return;
	}

	memset((u8*)0x0600a000, 0, 0xf6000);
	memset((u8*)0x06000c00, 0, 0x09400);
	memset((u8*)0x06000b00, 0, 0x00100);
	memcpy((u8*)0x06000000, (u8*)0x20000600, 0x0210);
	memcpy((u8*)0x06000220, (u8*)0x20000820, 0x08e0);
	memcpy((u8*)0x06001100, (u8*)0x20001100, 0x0700);
	memcpy((u8*)0x060002c0, (u8*)0x20001100, 0x0020);

	*(u32*)(0x06000358) = (u32)bup_init;

	if(use_sys_load==0){
		if(*(u32*)(0x06001340)==0x18a8){
			// 1.00
			bios_ver = 0;
			cdp_boot_game = (void*)0x18a8;
			*(u32*)(0x06001340) = (u32)cdp_boot;
			*(u32*)(0x06001344) = (u32)cdp_read_ip;
			*(u32*)(0x06001348) = (u32)cdp_init;
		}else if(*(u32*)(0x0600134c)==0x18a8){
			// 1.01 1.02
			bios_ver = 1;
			cdp_boot_game = (void*)0x18a8;
			*(u32*)(0x0600134c) = (u32)cdp_boot;
			*(u32*)(0x06001350) = (u32)cdp_read_ip;
			*(u32*)(0x06001354) = (u32)cdp_init;
		}else if(*(u32*)(0x060013d8)==0x19b8){
			// 1.03
			bios_ver = 2;
			cdp_boot_game = (void*)0x19b8;
			*(u32*)(0x060013d8) = (u32)cdp_boot;
			*(u32*)(0x060013dc) = (u32)cdp_read_ip;
			*(u32*)(0x060013e0) = (u32)cdp_init;
		}

		orig_0344 = (void*)*(u32*)(0x06000344);
		*(u32*)(0x06000344) = (u32)my_0344;
	}

	*(u32*)(0x06000234) = 0x02ac;
	*(u32*)(0x06000238) = 0x02bc;
	*(u32*)(0x0600023c) = 0x0350;
	*(u32*)(0x06000328) = 0x04c8;
	*(u32*)(0x0600024c) = 0x4843444d;

	go = (void*)0x06000680;
	go(0);
	pad_resume();
}


/**********************************************************/


int my_bios_loadcd_init(void)
{
	printk("\nmy_bios_loadcd_init!\n");

	*(u32*)(0x06000278) = 0;
	*(u32*)(0x0600027c) = 0;

	/* Retire the previous notification before starting initialization.
	 * A disc change during the following commands must remain visible. */
	clear_hirq(HIRQ_DCHG);
	int retv = cdc_abort_file();
	if(retv) return retv;
	retv = cdc_end_trans(NULL);
	if(retv) return retv;
	retv = cdc_reset_selector(0xfc, 0);
	if(retv) return retv;
	retv = cdc_set_size(0);
	if(retv) return retv;

	return cdc_get_hwinfo(NULL);
}


int my_bios_loadcd_read(void)
{
	int status, tm;
	u8 *sbuf = (u8*)0x06002000;
	char ipstr[64];

	printk("\nmy_bios_loadcd_read!\n");

	/* TOC uses the IP workspace before any IP bytes are live. */
	if(cdc_get_toc(sbuf)) return -1;
	int first = sbuf[397], last = sbuf[401];
	if(first < 1 || last < first || last > 99) return -1;
	int control = sbuf[(first-1)*4];
	if(control == 0xff) return -1;
	if(!(control & 0x40)) return -2; /* Audio: enter the existing player. */

	if(cdc_reset_selector(0, 0) || cdc_cddev_connect(0)) return -1;
	/* Clear the old event before PLAY; a fast completion must remain visible. */
	clear_hirq(HIRQ_PEND);
	if(cdc_play_fad(0, 150, 16)) return -1;
	tm = 10000000;
	while(tm){
		if(HIRQ&HIRQ_PEND)
			break;
		tm -= 1;
	}
	if(tm==0){
		printk("  PLAY timeout! HIRQ=%04x CR1=%04x\n", HIRQ, CR1);
		cdc_dump(10000000);
		return -1;
	}

	if(cdc_get_del_data(0, 0, 16)) return -1;
	cdc_trans_data(sbuf, 2048*16);
	if(cdc_end_trans(&status) || status != 2048*16/2) return -1;

	if(strncmp("SEGA SEGASATURN ", (char*)0x06002000, 16)){
		printk("Not a gamedisc!\n");
		return -2;
	}

	if(bios_type<=BIOS_100V){
		// 1.00j and V-Saturn 1.00
		*(u16*)(0x06000380) = 1;
	}else{
		*(u16*)(0x060003a0) = 1;
	}

	memcpy(ipstr, (u8*)0x06002020, 16);
	ipstr[16] = 0;
	printk("\nLoad game: %s\n", ipstr);
	memcpy(ipstr, (u8*)0x06002060, 32);
	ipstr[32] = 0;
	printk("  %s\n\n", ipstr);

	return 0;
}


/* After CKCHG, stay in WRAM until the A-bus read and timing restore finish.
 * These 28 bytes and the callback pointer fit before the BIOS literal at
 * 060006dc. */
static u16 code_06b8[14] = {
	0xd103, // mov.l dummy, r1 (060006c8)
	0x6112, // mov.l @r1, r1
	0xd103, // mov.l asr0, r1 (060006cc)
	0xd204, // mov.l setting, r2 (060006d0)
	0x2122, // mov.l r2, @r1
	0xd104, // mov.l next_call, r1 (060006d4)
	0x412b, // jmp   @r1
	0x4f26, // lds.l @r15+, pr
	0x2440,0x0000,
	0x25fe,0x00b0,
	0x3880,0x3880,
	// 060006d4: .long next_call
};


static void my_06b0(void)
{
	if(debug_flag&1) sci_init();
	printk("\nbios_set_clock_speed(%d)!\n", *(u32*)(0x06000324));

	void (*go)(void) = (void*)0x1800;
	go();

	scu_set_asr0(0x38803880);

    (void)smpc_enable_reset(192);
}


static void read_1st(void)
{
	printk("Read main ...\n");

	void (*go)(void) = (void*) *(u32*)(0x06000284);
	go();

	patch_game((char*)0x06002020);
	if(need_bup){
		*(u32*)(0x06000358) = (u32)bup_init;
		*(u32*)(0x0600026c) = (u32)my_cdplayer;
	}

	// 0x06000320: 0x060006b0  bios_set_clock_speed
	memcpy((u8*)0x060006b8, code_06b8, sizeof(code_06b8));
	*(u32*)(0x060006d4) = (u32)my_06b0;


	if(game_break_pc){
		set_break_pc(game_break_pc, 0);
		install_ubr_isr();
//		install_isr(ISR_NMI);
//		void reloc_vbr(void);
//		reloc_vbr();
		to_stm32 = 1;
		gets_from_stm32 = 1;
		*(u32*)(0x22820000) = 0;
	}
}


int bios_cd_cmd(int type)
{
	int retv, ip_size;

    if (!(type & 0x7f)) {
        retv = cdblock_off();
        if (retv) return retv;
    }

	use_sys_bup = (type&0x80)>>7;
	use_sys_load = 0;
	type &= 0x7f;
	disc_type = type;

	printk("bios_cd_cmd: type=%02x\n", type);

#if 0
	if(type!=2){
		// 0:ISO, 4:光盘
		// 0:ISO, 4:Optical disc
		bios_loadcd_init();
		while(1){
			ip_size = bios_loadcd_read();
			if(ip_size==0x8000)
				break;
		}

		char ipstr[64];

		memcpy(ipstr, (u8*)0x06002020, 16);
		ipstr[16] = 0;
		printk("\nLoad game: %s\n", ipstr);
		memcpy(ipstr, (u8*)0x06002060, 32);
		ipstr[32] = 0;
		printk("  %s\n\n", ipstr);
	}else
#endif
	{
		// 2: 刻录游戏盘
		// 2: Burned game disc
		retv = my_bios_loadcd_init();
		if(retv) return retv;
		retv = my_bios_loadcd_read();
		if(retv<0){
			if(retv==-2){
				/* Virtual images retain the same callbacks as the Z player path. */
				use_sys_load = (type != 0);
				my_cdplayer();
			}
			return retv;
		}

        retv = prepare_game_rom((const char *)0x06002020);
        if (retv)
            return retv;

		// emulate bios_loadcd_boot
		*(u32*)(0x06000290) = 3;
		ip_size = bios_loadcd_read();//1912读取ip文件
	}

	if(type>0 && use_sys_bup==0){
		// 光盘游戏。需要通知MCU加载SAVE。
		// Optical disc game. Need to notify MCU to load SAVE.
		if (menu_command_wait()) return -1;
		memcpy((void*)(TMPBUFF_ADDR+0x10), (u8*)0x06002020, 16);
		*(u8*)(TMPBUFF_ADDR+0x20) = 0;

        if (save_command(SSCMD_LSAVE, 0)) return -1;
	}

	// 模拟执行18fe()
	// Simulate executing 18fe()
	retv = cdc_change_dir(0, 0xffffff);
	if(retv) return retv;
	retv = cdc_read_file(0, 2, 0);
	if(retv) return retv;

	/* Publish final boot metadata and hooks after both CD commands succeed. */
	// 模拟执行1A3c()
	// 跳过各种验证
	// Simulate executing 1A3c()
	// Skip various validations
	memcpy((u8*)0x060002a0, (u8*)0x060020e0, 32);
	memcpy((u8*)0x06000c00, (u8*)0x06002000, 256);
	*(u32*)(0x06000254) = 0x6002100;

	*(u32*)(0x06002270) = (u32)read_1st;
	*(u32*)(0x02000f04) = (u32)cdc_read_sector;
	*(u16*)(0x0600220c) = 9;

	int (*go)(void) = (void*)0x18d2;
	pad_suspend();
	retv = go();
	pad_resume();

	printk("bios_loadcd_boot  retv=%d\n", retv);

	return retv;
}


/**********************************************************/

