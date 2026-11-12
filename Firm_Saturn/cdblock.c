
#include "main.h"
#include "smpc.h"


/**********************************************************/


void clear_hirq(int mask)
{
	HIRQ = ~mask;
}


int wait_hirq(int mask)
{
	int i;

	for(i=0; i<0x240000; i++){
		if(HIRQ&mask){
			return 0;
		}
	}

	return -3;
}


int wait_busy(void)
{
	int i;

	for(i=0; i<0x240000; i++){
		int cds = (CR1>>8)&0x0f;
		if(cds)
			return 0;
	}

	return -3;
}


void cdc_dump(int count)
{
	int hirq_old=-1, cr1_old=-1;
	int hirq, cr1, cr2, cr3, cr4;

	while(count){
		hirq = HIRQ;
		cr1 = CR1;
		cr2 = CR2;
		cr3 = CR3;
		cr4 = CR4;
		if( (hirq_old != hirq) || (cr1_old != cr1) ){
			printk("HIRQ: %04x CR1:%04x CR2:%04x CR3:%04x CR4:%04x\n", hirq, cr1, cr2, cr3, cr4);
			hirq_old = hirq;
			cr1_old = cr1;
		}
		count -= 1;
	}
}


int cdc_cmd(int wait, CDCMD *cmd, CDCMD *resp, char *cmdname)
{

	//printk("\ncdc_cmd: %04x %04x %04x %04x  %s\n", cmd->cr1, cmd->cr2, cmd->cr3, cmd->cr4, cmdname);

	clear_hirq(wait|HIRQ_CMOK);

	CR1 = cmd->cr1;
	CR2 = cmd->cr2;
	CR3 = cmd->cr3;
	CR4 = cmd->cr4;

	if(wait_hirq(HIRQ_CMOK)){
		printk("CMOK TIMEOUT! HIRQ=%04x wait=%04x %s\n", HIRQ, wait, cmdname);
		return -3;
	}

	resp->cr1 = CR1;
	resp->cr2 = CR2;
	resp->cr3 = CR3;
	resp->cr4 = CR4;

	if((resp->cr1>>8)==0xff){
		//printk("CMD REJECT!\n");
		return -5;
	}
	if((resp->cr1>>8)&0x80){
		printk("CMD WAIT! HIRQ=%04x CR1=%04x %s\n", HIRQ, resp->cr1, cmdname);
		return -6;
	}

	if(wait){
		if(wait_hirq(wait)){
			printk("HIRQ TIMEOUT! HIRQ=%04x wait=%04x %s\n", HIRQ, wait, cmdname);
			/* End an accepted transfer even when its FIFO never becomes ready. */
			if(wait == HIRQ_DRDY && (resp->cr1 & 0x4000))
				cdc_end_trans(NULL);
			return -3;
		}
	}
	//clear_hirq(wait|HIRQ_CMOK);

	//printk("   resp: %04x %04x %04x %04x\n", resp->cr1,resp->cr2,resp->cr3,resp->cr4);

	return 0;
}

/**********************************************************/


int cdc_get_status(int *status)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x0000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_status");

	if(status && !retv)
		*status = resp.cr1>>8;
	return retv;
}

int cdc_get_hwinfo(int *status)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x0100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_hwinfo");

	if(status && !retv)
		*status = resp.cr1>>8;
	return retv;
}


int cdc_cdb_init(int standby)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x0401;
	cmd.cr2 = standby;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x040f;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_cdb_init");
}


int cdc_end_trans(int *cdwnum)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x0600;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_end_trans");
	if(retv) return retv;
	if(cdwnum){
		cdwnum[0] = ((resp.cr1&0xff)<<16) | resp.cr2;
	}
	clear_hirq(HIRQ_DRDY);

	return 0;
}

int cdc_get_toc(u8 *buf)
{
	CDCMD cmd, resp;
	int i, retv;

	cmd.cr1 = 0x0200;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_toc");
	if(retv) return retv;

	for(i=0; i<102*4; i+=2){
		*(u16*)(buf+i) = REG16(0x25898000);
	}

	retv = cdc_end_trans(NULL);

	return retv;
}


/**********************************************************/


int cdc_play_fad(int mode, int start_fad, int range)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x1080 | (start_fad>>16);
	cmd.cr2 = start_fad&0xffff;
	cmd.cr3 = (mode<<8) | 0x80 | (range>>16);
	cmd.cr4 = range&0xffff;

	return cdc_cmd(0, &cmd, &resp, "cdc_play_fad");
}


/**********************************************************/

int cdc_cddev_connect(int filter)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x3000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = filter<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_cddev_connect");
}

int cdc_get_cddev(int *selnum)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x3100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_cddev");
	if(retv) return retv;

	*selnum = resp.cr3>>8;
	return retv;
}

/**********************************************************/


int cdc_set_filter_range(int selnum, int start_fad, int num_sector)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4000 | (start_fad>>16);
	cmd.cr2 = start_fad&0xffff;
	cmd.cr3 = (selnum<<8) | (num_sector>>16);
	cmd.cr4 = num_sector&0xffff;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_filter_range");
}

int cdc_get_filter_range(int selnum, int *fad, int *num_sector)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x4100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_filter_range");
	if(retv) return retv;

	*fad = ((resp.cr1&0xff)<<16) | resp.cr2;
	*num_sector = ((resp.cr3&0xff)<<16) | resp.cr4;

	return retv;
}


int cdc_set_filter_mode(int selnum, int mode)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4400 | mode;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_filter_mode");
}

int cdc_get_filter_mode(int selnum, int *mode)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x4500;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_filter_mode");
	if(retv) return retv;

	*mode = resp.cr1&0xff;
	return retv;
}

int cdc_set_filter_connect(int selnum, int c_true, int c_false)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4603;
	cmd.cr2 = (c_true<<8) | c_false;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_filter_connect");
}

int cdc_get_filter_connect(int selnum, int *c_true, int *c_false)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x4700;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_filter_connect");
	if(retv) return retv;

	*c_true = resp.cr2>>8;
	*c_false = resp.cr2&0xff;
	return retv;
}


int cdc_reset_selector(int flag, int selnum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x4800 | flag;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_reset_selector");
}

void show_selector(int id)
{
	int i;
	int mode, fad, range, c_true, c_false;

	for(i=0; i<24; i++){
		if(id==-1 || id==i){
			if(cdc_get_filter_mode(i, &mode)) return;
			if(cdc_get_filter_range(i, &fad, &range)) return;
			if(cdc_get_filter_connect(i, &c_true, &c_false)) return;
			printk("filter %2d: mode_%02x fad_%08x range_%08x true_%02x false_%02x\n",
					i, mode, fad, range, c_true, c_false);
		}
	}

}

/**********************************************************/

int cdc_get_buffer_size(int *total, int *pnum, int *free)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x5000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_buffer_size");
	if(retv) return retv;

	*free = resp.cr2;
	*pnum = resp.cr3>>8;
	*total = resp.cr4;
	return retv;
}

int cdc_get_numsector(int selnum, int *snum)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x5100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = selnum<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_numsector");
	if(retv) return retv;
	*snum = resp.cr4;
	return retv;
}


int cdc_calc_actsize(int bufno, int spos, int snum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x5200;
	cmd.cr2 = spos;
	cmd.cr3 = bufno<<8;
	cmd.cr4 = snum;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_calc_actsize");
}

int cdc_get_actsize(int *actsize)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x5300;
	cmd.cr2 = 0;
	cmd.cr3 = 0;
	cmd.cr4 = 0;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_actsize");
	if(retv) return retv;

	*actsize = ((resp.cr1&0xff)<<16) | resp.cr2;
	return retv;
}

int cdc_get_sector_info(int bufno, int secno, int *fad)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x5400;
	cmd.cr2 = secno;
	cmd.cr3 = bufno<<8;
	cmd.cr4 = 0x0000;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_sector_info");
	if(retv) return retv;

	*fad = ((resp.cr1&0xff)<<16) | resp.cr2;
	return retv;
}

/**********************************************************/

int cdc_set_size(int size)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x6000 | size;
	cmd.cr2 = size<<8;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	return cdc_cmd(HIRQ_ESEL, &cmd, &resp, "cdc_set_size");
}


int cdc_get_data(int bufnum, int spos, int snum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x6100;
	cmd.cr2 = spos;
	cmd.cr3 = bufnum<<8;
	cmd.cr4 = snum;

	return cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_data");
}

int cdc_get_del_data(int bufnum, int spos, int snum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x6300;
	cmd.cr2 = spos;
	cmd.cr3 = bufnum<<8;
	cmd.cr4 = snum;

	return cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_del_data");
}


int cdc_put_data(int bufnum, int snum)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x6400;
	cmd.cr2 = 0x0000;
	cmd.cr3 = bufnum<<8;
	cmd.cr4 = snum;

	return cdc_cmd(0, &cmd, &resp, "cdc_put_data");
}




void cdc_trans_data(u8 *buf, int length)
{
    if (length <= 0) return;
    /* Preserve aligned longword reads and use the word FIFO for tails. */
    if (((u32)buf & 3) == 0) {
        unsigned count = (unsigned)length >> 2;
        length &= 3;
        while (count--) {
            *(u32*)buf = REG(0x25818000);
            buf += 4;
        }
    }
    unsigned count = (unsigned)length >> 1;
    while (count--) {
        u16 word = REG16(0x25898000);
        buf[0] = word >> 8;
        buf[1] = word;
        buf += 2;
    }
    if (length & 1)
        *buf = REG16(0x25898000) >> 8;
}

/**********************************************************/


/**********************************************************/


int cdc_auth_status(int *status)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0xe100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	int retv = cdc_cmd(0, &cmd, &resp, "cdc_auth_status");
	if(retv) return retv;

	*status = resp.cr2;
	return retv;
}


int cdc_auth_device(void)
{
	CDCMD cmd, resp;
	int status, retv;

	//printk("\n\nCD_AUTH ... HIRQ=%04x\n", HIRQ);

	cmd.cr1 = 0xe000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	clear_hirq(HIRQ_EFLS);
	retv = cdc_cmd(0, &cmd, &resp, "cdc_auth_device");
	if(retv) return retv;
	while(1){
		if(!wait_hirq(HIRQ_EFLS))
			break;
	}

	retv = cdc_auth_status(&status);
	if(retv) return retv;
	printk("AUTH: %02x\n\n", status);

	return status;
}

/**********************************************************/

int cdc_change_dir(int selnum, int fid)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7000;
	cmd.cr2 = 0x0000;
	cmd.cr3 = (selnum<<8) | ((fid>>16)&0xff);
	cmd.cr4 = fid&0xffff;

	return cdc_cmd(HIRQ_EFLS, &cmd, &resp, "cdc_change_dir");
}

int cdc_read_dir(int selnum, int fid)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7100;
	cmd.cr2 = 0x0000;
	cmd.cr3 = (selnum<<8) | ((fid>>16)&0xff);
	cmd.cr4 = fid&0xffff;

	return cdc_cmd(HIRQ_EFLS, &cmd, &resp, "cdc_read_dir");
}

int cdc_get_file_scope(int *fid, int *fnum, int *drend)
{
	CDCMD cmd, resp;
	int retv;

	cmd.cr1 = 0x7200;
	cmd.cr2 = 0;
	cmd.cr3 = 0;
	cmd.cr4 = 0;

	retv = cdc_cmd(0, &cmd, &resp, "cdc_get_file_scope");
	if(retv) return retv;

	*fnum = resp.cr2;
	*drend = resp.cr3>>8;
	*fid = ((resp.cr3&0xff)<<16) | resp.cr4;
	return retv;
}

/* Return the admitted record count for callers that enumerate the results. */
static int cdc_file_info(int fid, CdcFile *buf)
{
    CDCMD cmd, resp;
    if (!buf) return -1;
    cmd.cr1 = 0x7300;
    cmd.cr2 = 0;
    cmd.cr3 = (fid >> 16) & 0xff;
    cmd.cr4 = fid & 0xffff;
    int result = cdc_cmd(HIRQ_DRDY, &cmd, &resp, "cdc_get_file_info");
    if (result) return result;

    int count = resp.cr2 / 6;
    int all = (fid & 0xffffff) == 0xffffff;
    if (count * 6 != resp.cr2 || count > 254 || (!all && count != 1))
        count = -1;
    else
        cdc_trans_data((u8*)buf, resp.cr2 * 2);
    result = cdc_end_trans(NULL);
    if (count < 0) return count;
    return result ? result : count;
}

int cdc_get_file_info(int fid, CdcFile *buf)
{
    int count = cdc_file_info(fid, buf);
    return count < 0 ? count : 0;
}

int cdc_read_file(int selnum, int fid, int offset)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7400 | ((offset>>16)&0xff);
	cmd.cr2 = offset&0xffff;
	cmd.cr3 = (selnum<<8) | ((fid>>16)&0xff);
	cmd.cr4 = fid&0xffff;

	return cdc_cmd(0, &cmd, &resp, "cdc_read_file");
//	return cdc_cmd(HIRQ_EFLS, &cmd, &resp, "cdc_read_file");
}


int cdc_abort_file(void)
{
	CDCMD cmd, resp;

	cmd.cr1 = 0x7500;
	cmd.cr2 = 0x0000;
	cmd.cr3 = 0x0000;
	cmd.cr4 = 0x0000;

	return cdc_cmd(0, &cmd, &resp, "cdc_abort_file");
}


/**********************************************************/


void cdc_auth_hram(CDCMD *resp, int delay)
{
	int status;

	// cdc_put_data(0, 150);
	HIRQ = ~(HIRQ_CMOK|HIRQ_EHST);
	CR1 = 0x6400;
	CR2 = 0x0000;
	CR3 = 0x0000;
	CR4 = 150;
	while(HIRQ&HIRQ_CMOK);
	resp->cr1 = CR1;
	resp->cr2 = CR2;
	resp->cr3 = CR3;
	resp->cr4 = CR4;
	status = resp->cr1>>8;
	if(status&0x80){
		printk("CMD WAIT! HIRQ=%04x status=%02x cdc_put_data\n", HIRQ, status);
	}

	for(int i=0; i<2352*150; i+=4){
		REG(0x25818000) = 0x00020002;
	}

	// cdc_end_trans
	HIRQ = ~(HIRQ_CMOK|HIRQ_EHST);
	CR1 = 0x0600;
	CR2 = 0x0000;
	CR3 = 0x0000;
	CR4 = 0x0000;
	while(HIRQ&HIRQ_CMOK);
	resp->cr1 = CR1;
	resp->cr2 = CR2;
	resp->cr3 = CR3;
	resp->cr4 = CR4;
	status = resp->cr1>>8;
	if(status&0x80){
		printk("CMD WAIT! HIRQ=%04x status=%02x cdc_end_trans\n", HIRQ, status);
	}
	while(HIRQ&HIRQ_EHST);
	HIRQ = ~(HIRQ_DRDY);

	for(int i=0; i<delay; i++){
		__asm volatile("":::"memory"); // 防止被gcc优化掉
		// Prevent gcc from optimizing it away
	}

	// cdc_move_data(0,0,0,50);
	HIRQ = ~(HIRQ_CMOK);
	CR1 = 0x6600;
	CR2 = 0x0000;
	CR3 = 0x0000;
	CR4 = 50;
	while(HIRQ&HIRQ_CMOK);
	resp->cr1 = CR1;
	resp->cr2 = CR2;
	resp->cr3 = CR3;
	resp->cr4 = CR4;
	status = resp->cr1>>8;
	if(status&0x80){
		printk("CMD WAIT! HIRQ=%04x status=%02x cdc_put_data\n", HIRQ, status);
	}

	// cdc_auth
	HIRQ = ~(HIRQ_CMOK|HIRQ_DCHG|HIRQ_EFLS);
	CR1 = 0xe000;
	CR2 = 0x0000;
	CR3 = 0x0000;
	CR4 = 0x0000;
	while(HIRQ&HIRQ_CMOK);
	resp->cr1 = CR1;
	resp->cr2 = CR2;
	resp->cr3 = CR3;
	resp->cr4 = CR4;
	status = resp->cr1>>8;
	if(status&0x80){
		printk("CMD WAIT! HIRQ=%04x status=%02x cdc_auth\n", HIRQ, status);
	}
	while(HIRQ&HIRQ_EFLS);
}


int jhl_auth_hack(int delay)
{
	int status, retv, cleanup;
	CDCMD resp;

	memcpy((u8*)0x06080000, cdc_auth_hram, 0x400);
	void (*go)(CDCMD*, int) = (void*)0x06080000;

#if 1
	while(1){
		status = (CR1>>8)&0x0f;
		if(status>=STATUS_OPEN){
			printk("State error! %04x\n", status);
			return status;
		}
		if(status==STATUS_PAUSE)
			break;
	}
#endif

	retv = cdc_set_size(3);
	if(retv) return retv;

	go(&resp, delay);

	int old = 0xffffffff;
	while(1){
		for(int i=0; i<100000; i++){ }
		retv = cdc_get_status(&status);
		if(retv) goto restore_size;
		if(status!=old){
			printk("stat: %04x\n", status);
		}
		old = status;
		status &= 0x0f;
		if(status==STATUS_FATAL)
			break;
		if(status==STATUS_PAUSE)
			break;
		if(status==STATUS_OPEN || status==STATUS_NODISC || status==STATUS_ERROR){
			retv = -1;
			goto restore_size;
		}
	}

	retv = cdc_auth_status(&status);
	if(!retv) printk("AUTH: %02x\n\n", status);

restore_size:
	/* Preserve the first error even if restoration also fails. */
	cleanup = cdc_set_size(0);
	if(retv) return retv;
	if(cleanup) return cleanup;
	return status;
}


/**********************************************************/


void cdc_init(void)
{
	cdc_abort_file();
	cdc_cdb_init(0);
	cdc_end_trans(NULL);
	cdc_reset_selector(0xfc, 0);

#if 0
	cdc_auth_device();

	int status;
	cdc_auth_status(&status);
	if(status==3){
		for(int i=0; i<8; i++){
			status = jhl_auth_hack(10000);
			if(status==2)
				break;;
		}
	}
#endif

}


int cdc_read_sector(int fad, int size, u8 *buf)
{
    if (size < 0 || (size && !buf)) return -1;
    if (!size) return 0;
    int result = cdc_set_size(0);
    if (result) return result;
    result = cdc_reset_selector(0, 0);
    if (result) return result;
    result = cdc_cddev_connect(0);
    if (result) return result;
    result = cdc_play_fad(0, fad, ((size - 1) >> 11) + 1);
    if (result) return result;

    int remaining = size;
    while (remaining > 0) {
        int available, words;
        result = cdc_get_numsector(0, &available);
        if (result) return result;
        if (!available) {
            /* Each empty-buffer episode gets a fresh firmware deadline. */
            u32 deadline = get_timer() + MS2TICK(10000);
            do {
                int status = (CR1 >> 8) & 0x0f;
                if (status >= STATUS_OPEN && status <= STATUS_FATAL && status != STATUS_RETRY)
                    return -1;
                if (timer_reached(get_timer(), deadline)) return -3;
                result = cdc_get_numsector(0, &available);
                if (result) return result;
            } while (!available);
        }
        int bytes = remaining >= 2048 ? 2048 : remaining;
        result = cdc_get_del_data(0, 0, 1);
        if (result) return result;
        cdc_trans_data(buf, bytes);
        result = cdc_end_trans(&words);
        if (result) return result;
        /* SAROO reports the entire sector in size mode zero, including
         * the remainder of a partially consumed final sector. */
        if (words != 1024 && words != 1162) return -1;
        buf += bytes;
        remaining -= bytes;
    }
    return size;
}


/* Bound both SF waits with one firmware deadline. */
static int cdblock_power(int cmd)
{
    printk("SMPC_CMD(%02x) ...\n", cmd);
    u32 deadline = get_timer() + MS2TICK(1000);
    while (SF & 1) {
        if (timer_reached(get_timer(), deadline)) return -3;
    }

    /* Release the CD interface before the physical drive powers on. */
    if (cmd == CDON) SS_CTRL = CS0_RAM4M;
    SF = 1;
    COMREG = cmd;
    while (SF & 1) {
        if (timer_reached(get_timer(), deadline)) return -3;
    }
    if (OREG31 != cmd) return -1;

    if (cmd == CDOFF) SS_CTRL = SAROO_EN | CS0_RAM4M;
    printk("    done. SS=%02x\n", SR);
    return 0;
}

int cdblock_on(int wait)
{
    int result = cdblock_power(CDON);
    if (result || !wait) return result;

    u32 deadline = get_timer() + MS2TICK(10000);
    for (;;) {
        int status = (CR1 >> 8) & 0x0f;
        (void)CR2;
        (void)CR3;
        (void)CR4;
        if (status) return 0;
        if (timer_reached(get_timer(), deadline)) return -3;
    }
}


int cdblock_off(void)
{
    return cdblock_power(CDOFF);
}


int cdblock_check(void)
{
	int retv, cdstat;

	cdc_abort_file();
	cdc_cdb_init(0);
	cdc_end_trans(NULL);
	cdc_reset_selector(0xfc, 0);

	while(1){
		retv = cdc_get_status(&cdstat);
		if(retv) return retv;
		cdstat &= 0x0f;
		// cdc_reset_selector会有比较长的BUSY状态。
		// cdc_reset_selector has a relatively long BUSY state.
		if(cdstat==STATUS_BUSY)
			continue;
		if(cdstat==STATUS_OPEN || cdstat==STATUS_NODISC){
			return -1;
		}
		if(cdstat==STATUS_PAUSE)
			break;
	}

	// 0: no Disc 
	// 1: Audio Disc
	// 2: Data Disc
	// 3: Pirated Saturn Disc
	// 4: Saturn Disc
	retv = cdc_auth_device();

	return retv;
}


/**********************************************************/


u32 *toc_buf = (u32*)0x06090000;

void show_toc(void)
{
	int i;

	for(i=0; i<102; i++){
		if(toc_buf[i]!=0xffffffff)
			printk("TOC %3d:  %08x\n", i+1, toc_buf[i]);
	}
}


void cdc_file_test(void)
{
	int i;
	int fid, fnum, drend;
	int total, npart, free;
	CdcFile finfo[256];
	u8 *sbuf = (u8*)0x06080000;

	cdc_init();

	cdc_get_toc((u8*)toc_buf);
	show_toc();

	cdc_change_dir(0, 0xffffff);

	fnum = 0;
	if(cdc_get_file_scope(&fid, &fnum, &drend)) return;
	printk("file scope: fid=%d fnum=%d drend=%d\n", fid, fnum, drend);
	fnum = cdc_file_info(0xffffff, finfo);
	if(fnum < 0) return;
	for(i=0; i<fnum; i++){
		printk("file %4d: fad=%08x size=%08x %02x %02x %02x %02x\n", i,
				finfo[i].fad, finfo[i].size,
				finfo[i].unit, finfo[i].gap, finfo[i].fn, finfo[i].attr);
	}

	cdc_read_file(1, 6, 0);
	show_selector(1);

	do{
		if(cdc_get_numsector(1, &free)) return;
		printk("cdc_get_numsector 1: %d HIRQ=%04x\n", free, HIRQ);
	}while(free==0);

	if(cdc_get_buffer_size(&total, &npart, &free)) return;
	printk("cdc_get_buffer_size: free=%d\n", free);

	cdc_get_del_data(1, 0, free);
	cdc_trans_data(sbuf, 2048*free);
	cdc_end_trans(NULL);

	while(1){
		if(cdc_get_numsector(1, &free)) return;
		if(free==200)
			break;
	}

	if(cdc_get_numsector(1, &free)) return;
	printk("cdc_get_numsector 1: %d HIRQ=%04x\n", free, HIRQ);
	cdc_get_buffer_size(&total, &npart, &free);
}


/**********************************************************/


void cdc_read_test(void)
{
	int status;
	int nsec, actsize;
	int total, npart, free;
	int hirq, cr1, cr2, cr3, cr4;
	int old_hirq=0, old_cr1=0, old_cr2=0, old_cr3=0, old_cr4=0;

	u8 *sbuf = (u8*)0x06040000;
	char *logbuf = (char*)0x06080000;
	int log_p = 0;

	cdc_set_size(0);
	cdc_reset_selector(0, 0);
	cdc_cddev_connect(0);

	HIRQ = 0;
	cdc_play_fad(0, 0xd5, 0x40);

	printk("\n\nwait play end ...\n");
	u32 start = get_timer();
	while(1){
		hirq = HIRQ;
		cr1 = CR1;
		cr2 = CR2;
		cr3 = CR3;
		cr4 = CR4;
		if(old_hirq!=hirq || old_cr1!=cr1 || old_cr4!=cr4){
			if(cdc_get_buffer_size(&total, &npart, &free)) return;
			printk("%08x: HIRQ=%04x CR1=%04x CR4=%04x free=%d\n", get_timer() - start, hirq, cr1, cr4, free);
			log_p += sprintf(logbuf+log_p, "%08x: HIRQ=%04x CR1=%04x CR4=%04x free=%d\n",
							get_timer() - start, hirq, cr1, cr4, free);
		}
		old_hirq = hirq;
		old_cr1 = cr1;
		old_cr2 = cr2;
		old_cr3 = cr3;
		old_cr4 = cr4;
		if(hirq&HIRQ_PEND)
			break;
		if(hirq)
			clear_hirq(hirq);

		//cdc_get_status(&status);
		//printk("STATUS: %02x  HIRQ=%04x\n", status, HIRQ);
	}
	//logbuf[log_p] = 0;
	printk("LOG: log_p=%08x\n", log_p);
	int i=0;
	while(i<log_p){
		printk("%s", logbuf+i);
		i += strlen(logbuf+i);
		i += 1;
	}

	cdc_get_numsector(0, &nsec);

	cdc_calc_actsize(0, 0, 2);
	cdc_get_actsize(&actsize);
	cdc_get_buffer_size(&total, &npart, &free);

	cdc_get_data(0, 0, 0xffff);
	cdc_trans_data(sbuf, 2048*0xc0);
	cdc_end_trans(&status);

	cdc_get_status(&status);
	cdc_get_buffer_size(&total, &npart, &free);
	mem_dump("Sector Data", sbuf, 256);

	memcmp((u8*)0x22100000, (u8*)0x06040000, 2048*0xc0);
}


/**********************************************************/



void cd_cmd(void)
{
	cdc_init();

	cdc_get_toc((u8*)toc_buf);
	show_toc();

	cdc_read_test();

	//cdc_read_sector(0xd5, 0x195, 0x06040000);

}

void cd_speed_test(void)
{
	int cmd_cnt;
	u32 tm;
	//int status;

	cmd_cnt = 0;

	u32 start = get_timer();

	while(1){
		//cdc_get_status(&status);
		int nsec;
		cdc_get_numsector(0, &nsec);
		cmd_cnt += 1;
		tm = get_timer() - start;
		if(tm>=HZ)
			break;
	}

	printk("cd_speed_test: cmd_cnt=%d tm=%d\n", cmd_cnt, tm);
}

