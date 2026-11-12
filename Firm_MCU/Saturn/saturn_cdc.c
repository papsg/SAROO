
#include "main.h"
#include "ff.h"
#include "cdc.h"


/******************************************************************************/

CDBLOCK cdb;

/* Retain the result until accepted copy/move or CD software reset. */
static u8 copy_move_error;

/******************************************************************************/
// 数据传输

BLOCK *alloc_block(void)
{
	int i, irqs;
	BLOCK *blk;

	blk = NULL;
	irqs = disable_irq();

	for(i=0; i<MAX_BLOCKS; i++){
		if(cdb.block[i].size==-1){
			cdb.block_free -= 1;

			cdb.block[i].size = cdb.sector_size;
			blk = &cdb.block[i];
			break;
		}
	}

	if(cdb.block_free==0){
		HIRQ = HIRQ_BFUL;
	}

	restore_irq(irqs);
	return blk;
}

void free_block(BLOCK *block)
{
	int irqs;

	irqs = disable_irq();
	block->size = -1;
	cdb.block_free += 1;
	restore_irq(irqs);

	if(cdb.play_wait==1 && cdb.block_free>16){
		cdb.play_wait = 0;
		disk_task_wakeup();
	}
}


BLOCK *dup_block(BLOCK *block)
{
	if(block==NULL)
		return NULL;

	BLOCK *new = alloc_block();
	if(new==NULL)
		return NULL;

	new->size = block->size;
	new->fad  = block->fad;
	new->cn   = block->cn;
	new->fn   = block->fn;
	new->sm   = block->sm;
	new->ci   = block->ci;
	memcpy(new->data, block->data, 2352);

	new->next = NULL;
	return new;
}


BLOCK *find_block(PARTITION *part, int index)
{
	BLOCK *p;
	int i, irqs;

	if(part->numblocks==0)
		return NULL;

	irqs = disable_irq();

	p = part->head;
	i = 0;

	while(p){
		if(i==index){
			restore_irq(irqs);
			return p;
		}
		i += 1;
		p = p->next;
	}

	restore_irq(irqs);
	return NULL;
}


void append_block(PARTITION *part, BLOCK *block)
{
    int irqs = disable_irq();
    if (part->tail) part->tail->next = block;
    else part->head = block;
    part->tail = block;
    block->next = NULL;
    if (part->size < 0) part->size = 0;
    part->size += block->size;
    part->numblocks += 1;
    restore_irq(irqs);
}

/* Caller holds the interrupt mask; detached blocks remain allocated. */
static void unlink_block(PARTITION *part, BLOCK *prev, BLOCK *block)
{
    if (prev) prev->next = block->next;
    else part->head = block->next;
    if (part->tail == block) part->tail = prev;
    part->size -= block->size;
    part->numblocks -= 1;
    block->next = NULL;
}

/* Stop a partition's host output before mutation. GETDEL retains its private
 * selection until DataEnd; disk-side callers run below the command task. */
static void disconnect_partition_output(PARTITION *part)
{
    int irqs = disable_irq();
    if ((cdb.trans_type == TRANS_DATA || cdb.trans_type == TRANS_DATA_DEL) &&
        cdb.trans_part_index < MAX_SELECTORS &&
        part == &cdb.part[cdb.trans_part_index]) {
        cdb.trans_part_index = 0xff;
        cdb.trans_finish = 1;
        ST_CTRL &= ~STIRQ_DAT;
        ST_STAT = STIRQ_DAT;
        HIRQ = HIRQ_EHST;
    }
    restore_irq(irqs);
}

/* Caller masks interrupts and admits the range. Validate the complete chain
 * before changing ownership; reuse links without copying sector payloads. */
static BLOCK *detach_sector_range(PARTITION *part, int start, int count)
{
    BLOCK *prev = NULL, *head = part->head, *last = NULL, *next;
    while (head && start--) { prev = head; head = head->next; }
    next = head;
    int bytes = 0;
    for (int i = 0; i < count; i++) {
        if (!next) return NULL;
        bytes += next->size;
        last = next;
        next = next->next;
    }
    if (prev) prev->next = next;
    else part->head = next;
    if (part->tail == last) part->tail = prev;
    part->size -= bytes;
    part->numblocks -= count;
    last->next = NULL;
    return head;
}

static void free_block_list(BLOCK *head)
{
    while (head) {
        BLOCK *next = head->next;
        free_block(head);
        head = next;
    }
}

void remove_block(PARTITION *part, int index)
{
    int irqs = disable_irq();
    BLOCK *prev = NULL, *block = part->head;
    while (block && index > 0) {
        prev = block;
        block = block->next;
        index -= 1;
    }
    if (block && index == 0) {
        disconnect_partition_output(part);
        unlink_block(part, prev, block);
        free_block(block);
    }
    restore_irq(irqs);
}

void fill_fifo(u8 *addr, int size)
{
	int i;

	for(i=0; i<size; i+=2){
		FIFO_DATA = *(u16*)(addr+i);
	}
}

static int min_num;

/* Call only with a validated partition; resolve independent range sentinels. */
static int sector_range(PARTITION *part, int *start, int *count)
{
    if (*start == 0xffff) *start = part->numblocks-1;
    if (*count == 0xffff) *count = part->numblocks-*start;
    return *start >= 0 && *count > 0 && *start < part->numblocks &&
           *count <= part->numblocks-*start;
}

/* Keep calculation and both FIFO producers on the same selected-length rule. */
static int sector_transfer_size(const BLOCK *block)
{
    if (cdb.sector_size == 2048 && block->data[15] == 2 &&
        (block->data[18] & 0x20)) return 2324;
    return cdb.sector_size;
}

void trans_start(void)
{
	u8 *dp = NULL;

	min_num = 40960;
	cdb.cdwnum = 0;
	cdb.trans_finish = 0;

	if(cdb.trans_type==TRANS_DATA || cdb.trans_type==TRANS_DATA_DEL){
        /* Admission publishes this pointer after all potentially blocking logs. */
        BLOCK *bt = cdb.trans_block;
        cdb.trans_bk = 0;

		if(cdb.sector_size==2352){
			dp = bt->data;
		}else if(cdb.sector_size==2340){
			dp = bt->data+12;
		}else if(cdb.sector_size==2336){
			dp = bt->data+16;
		}else{
			dp = bt->data+24;
		}

		cdb.trans_size = sector_transfer_size(bt);

		cdb.trans_block = bt;
		cdb.trans_bk += 1;
	}else if(cdb.trans_type==TRANS_TOC){
		dp = (u8*)cdb.TOC;
		cdb.trans_size = 102*4;
	}else if(cdb.trans_type==TRANS_FINFO_ALL){
		dp = (u8*)cdb.FINFO;
		cdb.trans_size = cdb.cdir_pfnum*12;
	}else if(cdb.trans_type==TRANS_FINFO_ONE){
		dp = (u8*)cdb.FINFO;
		cdb.trans_size = 12;
	}else if(cdb.trans_type==TRANS_SUBQ){
		dp = cdb.SUBH;
		cdb.trans_size = 10;
	}else if(cdb.trans_type==TRANS_SUBRW){
#if 0
		if(cdb.subrw_rp == cdb.subrw_wp){
			memset(cdb.SUBH, 0, 24);
			dp = cdb.SUBH;
		}else
#endif
		{
			dp = cdb.subrw_buf+cdb.subrw_rp*24;
			cdb.subrw_rp = (cdb.subrw_rp+1)%64;
		}
		cdb.trans_size = 24;
	}

	FIFO_RCNT = 0;
	ST_CTRL &= ~0x0200;
	fill_fifo(dp, cdb.trans_size);
	ST_STAT  = STIRQ_DAT;
	ST_CTRL |= STIRQ_DAT;
}


// 一个block的数据已经写入FIFO中
// 准备写入下一个block的数据，或者删除已经写完的block
void trans_handle(void)
{
    if (!cdb.trans_type) return;
    if ((cdb.trans_type == TRANS_DATA || cdb.trans_type == TRANS_DATA_DEL) &&
        cdb.trans_part_index >= MAX_SELECTORS) return;
    if (cdb.trans_type == TRANS_PUT) {
        ST_CTRL &= ~STIRQ_DAT;
        /* Acknowledge before draining: preserve a subsequent threshold event. */
        ST_STAT = STIRQ_DAT;
        if (cdb.trans_part_index >= MAX_SELECTORS) return;
        int stat = FIFO_STAT;
        int words = (stat & 0x0800) ? 2048 : (stat & 0x07ff);
        int offs = cdb.trans_size == 2048 ? 24 : 2352-cdb.trans_size;
        BLOCK *bt = cdb.trans_block;
        /* One available snapshot, at most4096 bytes; never await future input. */
        while (words && bt) {
            int count = (cdb.trans_size-bt->size)/2;
            if (count > words) count = words;
            u16 *out = (u16 *)(bt->data + offs + bt->size);
            for (int i = 0; i < count; i++) out[i] = FIFO_DATA;
            bt->size += count*2;
            cdb.cdwnum += count*2;
            words -= count;
            if (bt->size == cdb.trans_size) {
                bt = bt->next;
                cdb.trans_bk -= 1;
            }
        }
        cdb.trans_block = bt;
        if (bt) ST_CTRL |= STIRQ_DAT;
        return;
    }

	if(cdb.trans_finish==0){
		// 中断在传输结束时会重复进入一次。这里做个判断，以免误加。
		cdb.cdwnum += cdb.trans_size;
	}

	if(cdb.trans_type==TRANS_DATA || cdb.trans_type==TRANS_DATA_DEL){

		if(cdb.trans_bk==cdb.trans_block_end){
			cdb.trans_finish = 1;
			ST_CTRL &= ~STIRQ_DAT;
		}else{
			BLOCK *bt = cdb.trans_block->next;
			u8 *dp;
			int stat;

			if(cdb.sector_size==2352){
				dp = bt->data;
			}else if(cdb.sector_size==2340){
				dp = bt->data+12;
			}else if(cdb.sector_size==2336){
				dp = bt->data+16;
			}else{
				dp = bt->data+24;
			}

			cdb.trans_size = sector_transfer_size(bt);

			cdb.trans_block = bt;
			cdb.trans_bk += 1;

			stat = FIFO_STAT;
			stat &= 0x0fff;
			if(stat<min_num)
				min_num = stat;

			ST_CTRL &= ~STIRQ_DAT;
			fill_fifo(dp, cdb.trans_size);
			ST_STAT  = STIRQ_DAT;
			ST_CTRL |= STIRQ_DAT;
			HIRQ = HIRQ_SCDQ;
		}
	}else{
		cdb.trans_finish = 1;
		ST_CTRL &= ~STIRQ_DAT;
	}
}


/******************************************************************************/

/* CR3 occupies the low half of this internal argument; the peripheral still
 * receives separate halfword writes in CR1, CR2, CR3, CR4 order. */
static inline u32 reply_pair(u16 cr3, u16 cr4)
{
    return cr3 | ((u32)cr4 << 16);
}

/* The command task owns these replies. Three arguments leave a scratch
 * register for the peripheral base, avoiding a saved register/stack frame. */
static __attribute__((noinline,noclone)) void command_reply(u16 cr1, u16 cr2, u32 cr34)
{
    SSCR1 = cr1;
    SSCR2 = cr2;
    SSCR3 = (u16)cr34;
    SSCR4 = (u16)(cr34 >> 16);
}

void set_report(u8 status)
{
	int irqs;

	irqs = disable_irq();

	SSCR1 = (status<<8) | ((cdb.options&0x0f)<<4) |
             (cdb.options == 0xff ? 15 : cdb.repcnt&15);
	SSCR2 = (cdb.ctrladdr<<8) | cdb.track;
	SSCR3 = (cdb.index<<8) | ((cdb.fad>>16)&0xff);
	SSCR4 = cdb.fad&0xffff;

	restore_irq(irqs);
}


void set_status(u8 status)
{
	int irqs = disable_irq();
	SSCR1 = (status<<8) | (SSCR1&0x00ff);
	restore_irq(irqs);
}


/* Route normalized producer metadata. Callers retain ownership of the block.
 * The logging flag preserves the existing incoming mode1 false-disconnect log. */
static int sector_destination(int fid, const BLOCK *block, int log_false_disconnect)
{
    for (int hops = 0; hops < MAX_SELECTORS && fid < MAX_SELECTORS; hops++) {
        const FILTER *ft = &cdb.filter[fid];
        int match = 1;
        if ((ft->mode & 0x01) && block->fn != ft->fid) match = 0;
        if ((ft->mode & 0x02) && block->cn != ft->chan) match = 0;
        if ((ft->mode & 0x04) && (block->sm & ft->smmask) != ft->smval) match = 0;
        if ((ft->mode & 0x08) && (block->ci & ft->cimask) != ft->cival) match = 0;
        if (ft->mode & 0x10) match ^= 1;
        if ((ft->mode & 0x40) &&
            (block->fad < ft->fad || block->fad - ft->fad >= ft->range)) match = 0;
        if (match) {
            int dst = ft->c_true;
            return dst < MAX_SELECTORS ? dst : 0xff;
        }
        if (ft->c_false == 0xff && log_false_disconnect)
            printk("  c_false is FF!\n");
        fid = ft->c_false;
    }
    return 0xff;
}

int filter_sector(TRACK_INFO *track, BLOCK *wblk)
{
    PARTITION *pt;
    BLOCK *blk;
    int irqs;
    int dst = sector_destination(cdb.cddev_filter, wblk, track && track->mode==1);

    if (dst >= MAX_SELECTORS) {
        cdb.last_buffer = 0xff;
        return 1;
    }
    pt = &cdb.part[dst];

	blk = alloc_block();
	if(blk==NULL){
		return 2;
	}

	blk->size = cdb.sector_size;
	blk->fad  = wblk->fad;
	blk->fn   = wblk->fn;
	blk->cn   = wblk->cn;
	blk->sm   = wblk->sm;
	blk->ci   = wblk->ci;

	if(wblk->size==2048){
		memset(blk->data, 0, 24);
		memcpy32(blk->data+24, wblk->data, 2048);
	}else if(wblk->size==2336){
        memset(blk->data, 0, 16);
        blk->data[15] = 2;
        memcpy32(blk->data+16, wblk->data, 2336);
        if(wblk->sm & 0x20) blk->size = 2324;
	}else if(wblk->size==2324){
        memset(blk->data, 0, 24);
        blk->data[15] = 2;
        blk->data[18] = blk->data[22] = 0x20;
        memcpy32(blk->data+24, wblk->data, 2324);
        memset(blk->data+2348, 0, 4);
        blk->size = 2324;
	}else if(wblk->size==2352){
		if(track->mode==3 || wblk->data[15]==2){
			// MODE2 or AUDIO
			memcpy32(blk->data, wblk->data, 2352);
			if(wblk->sm & 0x20){
				blk->size = 2324;
			}
		}else{
			// MODE1
			memcpy32(blk->data, wblk->data, 16);
			memset(blk->data+16, 0, 8);
			memcpy32(blk->data+24, wblk->data+16, 2048);
		}
	}

	irqs = disable_irq();
	cdb.last_buffer = dst;

	if(pt->size==-1)
		pt->size = 0;

	// 疑问: partition中的size, 是按wblk的size算,还是按sector_size算?
	// 已经确认,是按设置的sector_size算.
	pt->size += blk->size;
	pt->numblocks += 1;

	if(pt->head==NULL){
		pt->head = blk;
		pt->tail = blk;
	}else{
		pt->tail->next = blk;
		pt->tail = blk;
	}
	blk->next = NULL;

	restore_irq(irqs);
	//printk("  filter done.\n");
	return 0;
}


/******************************************************************************/
// 普通命令


// 0x00 [SR]
int get_cd_status(void)
{
	set_report(cdb.status);
	return 0;
}

// 0x01 [S-]
int get_hw_info(void)
{
	SSLOG(_INFO, "get_hw_info\n");

	command_reply((cdb.status<<8), 0x0001, reply_pair(0x0000, 0x0400));

	return 0;
}

// 0x02 [S-]
int get_toc_info(void)
{
	SSLOG(_INFO, "get_toc_info\n");

	if(cdb.trans_type){
		set_status(STAT_WAIT | cdb.status);
		return 0;
	}

	command_reply(0x4000|(cdb.status<<8), 0x00cc, reply_pair(0x0000, 0x0000));

	cdb.trans_type = TRANS_TOC;
	trans_start();

	return HIRQ_DRDY;
}

// 0x03 [S-]
int get_session_info(void)
{
	int sid, fad;

	sid = cdb.cr1&0xff;
	SSLOG(_INFO, "get_session_info: sid=%d\n", sid);

	SSCR1 = (cdb.status<<8);
	SSCR2 = 0x0000;
	if(sid==0){
		fad = bswap32(cdb.TOC[101]);
		SSCR3 = 0x0100 | ((fad>>16)&0xff);
		SSCR4 = fad&0xffff;
	}else if(sid==1){
		SSCR3 = 0x0100;
		SSCR4 = 0x0000;
	}else{
		SSCR3 = 0xffff;
		SSCR4 = 0xffff;
	}

	return 0;
}

/* Clear condition fields while the caller owns connection changes. */
static __attribute__((noinline,noclone)) void reset_filter_conditions(FILTER *filter)
{
    filter->mode = 0;
    filter->fad = 0;
    filter->range = 0;
    filter->fid = 0;
    filter->chan = 0;
    filter->smmask = 0;
    filter->cimask = 0;
    filter->smval = 0;
    filter->cival = 0;
}

// 0x04 [SR]
//   iflag[0]: 复位CDBLOCK软件(其他位忽略)
//   iflag[1]: 解码subcode_rw
//   iflag[2]: 不确认MODE2 subheader
//   iflag[3]: Form2读取重试
//   iflag[4]: CD-ROM 2x
//   iflag[7]: 忽略所有位
//
//   stnby: 从PAUSE到STANDY的时间。默认0x0000(180秒)
//
int init_cdblock(void)
{
	int i;

	int iflag = cdb.cr1&0xff;
	int stnby = cdb.cr2;
	int ecc   = cdb.cr4>>8;
	int retry = cdb.cr4&0xff;

	SSLOG(_INFO, "init_cdblock  iflag=%02x stnby=%04x ecc=%02x retry=%02x\n", iflag, stnby, ecc, retry);

    /* The reader still owns its policy and buffers until it acknowledges.
     * Keep its request pending if the bounded handoff cannot complete. */
    if (pause_drive()) { set_status(STAT_WAIT|cdb.status); return 0; }
    if (iflag & 0x80) {
        set_report(cdb.status);
        return 0;
    }
    cdb.iflag = (iflag & 1) ? 0 : iflag;

	if((iflag&0x01)==0){
		set_report(cdb.status);
		return 0;
	}


	copy_move_error = 0;
	cdb.subrw_rp = 0;
	cdb.subrw_wp = 0;

	// disable fifo irq
	ST_CTRL &= ~(STIRQ_DAT | 0x0200);
	// reset fifo
	ST_CTRL |= 0x0100;
	ST_CTRL &=~0x0100;

	cdb.actsize = 0;
	cdb.sector_size = 2048;
	cdb.put_sector_size = 2048;
	cdb.trans_type = 0;
	cdb.trans_block = NULL;
	cdb.trans_bk = 0;
	cdb.cdwnum = 0;
	// Galaxy Fight: 在play之后发出init_cdblock命令. 此处不能设置当前fad，否则会破坏之前的play。
	//cdb.fad = 150;
	invalidate_directory();
	cdb.cdir_lba = 0;
	cdb.root_lba = 0;

	// free all block
	cdb.block_free = MAX_BLOCKS;
	for(i=0; i<MAX_BLOCKS; i++){
		cdb.block[i].size = -1;
	}

	// reset all filter
	for(i=0; i<MAX_SELECTORS; i++){
		cdb.part[i].size = -1;
		cdb.part[i].numblocks = 0;
		cdb.part[i].head = NULL;
		cdb.part[i].tail = NULL;

		cdb.filter[i].c_false = 0xff;
		cdb.filter[i].c_true = i;
		reset_filter_conditions(&cdb.filter[i]);
	}

	cdb.saturn_auth = 0;
	cdb.mpeg_auth = 0;

	HIRQ_CLR = HIRQ_PEND | HIRQ_DRDY | HIRQ_BFUL;

	set_report(cdb.status);
	return HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST;
}


// 0x05 [S-]
int open_tray(void)
{
	SSLOG(_INFO, "open_tray\n");

	command_reply((cdb.status<<8), 0x0000, reply_pair(0x0000, 0x0000));

	return 0;
}


// 0x06 [S-]
int end_trans(void)
{
	int fifo_remain;
    int success = cdb.trans_type != 0;

	// disable fifo irq
	ST_CTRL &= ~STIRQ_DAT;
	ST_STAT = STIRQ_DAT;

    if (cdb.trans_type == TRANS_PUT) {
        if (cdb.trans_bk) {
            SSLOG(_BUFIO, "end_trans: wait trans_put ...\n");
        }
        trans_handle();
        ST_CTRL &= ~(STIRQ_DAT | 0x0200);
        ST_STAT = STIRQ_DAT;
        BLOCK *bt = &cdb.block[cdb.trans_block_start];
        while (bt) {
            BLOCK *next = bt->next;
            const u8 *data = bt->data;
            /* Early DataEnd incorporates all reserved sectors. Unwritten
             * payload stays undefined; only absent prefixes were initialized. */
            bt->size = cdb.trans_size;
            bt->fad = ((data[12] >> 4)*10 + (data[12] & 15))*4500 +
                      ((data[13] >> 4)*10 + (data[13] & 15))*75 +
                      ((data[14] >> 4)*10 + (data[14] & 15));
            bt->fn = data[15]==2 ? data[16] : 0;
            bt->cn = data[15]==2 ? data[17] : 0;
            bt->sm = data[15]==2 ? data[18] : 0;
            bt->ci = data[15]==2 ? data[19] : 0;
            int dst = sector_destination(cdb.trans_part_index, bt, 0);
            if (dst < MAX_SELECTORS) append_block(&cdb.part[dst], bt);
            else free_block(bt);
            bt = next;
        }
        cdb.trans_block = NULL;
        cdb.trans_bk = 0;
        if (cdb.trans_part_index >= MAX_SELECTORS) cdb.cdwnum = 0;
    } else {
		SSLOG(_BUFIO, "end_trans: cdwnum=%08x FIFO_STAT=%08x RCNT=%04x min_num=%d\n", cdb.cdwnum, FIFO_STAT, FIFO_RCNT, min_num);
        if (cdb.trans_type) {
		fifo_remain = (FIFO_STAT&0x0fff)*2; // FIFO中还有多少字节未读
        /* Reads report CD-produced words, including the pending window;
         * host consumption does not subtract from that output count. */
        if (!cdb.trans_finish) cdb.cdwnum += cdb.trans_size;
        if (fifo_remain > cdb.cdwnum) success = 0;
        }
		SSLOG(_BUFIO, "         : cdwnum=%08x\n", cdb.cdwnum);
	}

	// reset fifo
	ST_CTRL |= 0x0100;
	ST_CTRL &=~0x0100;

    if (cdb.trans_type == TRANS_DATA_DEL)
        free_block_list(&cdb.block[cdb.trans_block_start]);
    if ((cdb.trans_type == TRANS_DATA || cdb.trans_type == TRANS_DATA_DEL) &&
        cdb.trans_part_index >= MAX_SELECTORS) success = 0;
    if (cdb.trans_type == TRANS_PUT && cdb.trans_part_index >= MAX_SELECTORS)
        success = 0;

    if (success) {
		SSCR1 = (cdb.status<<8) | ((cdb.cdwnum>>17)&0xff);
		SSCR2 = (cdb.cdwnum>>1)&0xffff;
	}else{
		SSCR1 = (cdb.status<<8) | 0xff;
		SSCR2 = 0xffff;
	}
	SSCR3 = 0;
	SSCR4 = 0;

	cdb.cdwnum = 0;
	cdb.trans_type = 0;
    cdb.trans_block = NULL;
    cdb.trans_bk = 0;

	return HIRQ_EHST;
}

/******************************************************************************/
// CD drive 相关命令

// cdb.fad: 当前播放的位置

// 0x10 [SR]
/* Return an exclusive end for end positions, and a sector FAD for starts.
 * Callers handle NOCHG and FAD-length ends separately. Track/index exceptions
 * follow Sega CdcPos 6.3; no caller forms an unchecked track pointer. */
static u32 drive_position(u32 position, int ending)
{
    u32 leadout = cdb.tracks[cdb.track_num-1].fad_end+1;
    if (!position) return ending ? leadout : 150;
    if (position & 0x800000) {
        u32 fad = position & 0x7fffff;
        return fad < 150 ? 150 : fad > leadout ? leadout : fad;
    }
    unsigned track = (position>>8)&255, index = position&255;
    if (!track) track = ending ? cdb.track_num : 1;
    else if (track > (unsigned)cdb.track_num) { track = cdb.track_num; index = 0; }
    if (!index) index = ending ? 99 : 1;
    TRACK_INFO *tk = &cdb.tracks[track-1];
    if (ending) {
        if (index < (unsigned)tk->max_index && tk->index_base) return TRACK_INDEX(tk)[index-1];
        return tk->fad_end+1;
    }
    if (index <= 1) return tk->fad_start;
    if (index <= (unsigned)tk->max_index && tk->index_base) return TRACK_INDEX(tk)[index-2];
    return track < (unsigned)cdb.track_num ? cdb.tracks[track].fad_start : leadout;
}

static void set_drive_position(u32 fad)
{
    cdb.old_fad = cdb.fad;
    cdb.fad = fad;
    cdb.track = fad_to_track(fad);
    TRACK_INFO *tk = get_track_info(cdb.track<<8);
    cdb.play_track = tk;
    cdb.index = tk ? track_subindex(tk, fad) : 1;
    cdb.ctrladdr = tk ? TRACK_CTRL(tk) : 0xff;
}

/* Incoming positions are 24-bit. Normalized FADs lose their original form. */
static unsigned retained_position_type(u32 position, unsigned previous)
{
    return position == 0xffffff ? previous :
           !position ? 0 : (position >> 23) + 1;
}

int play_cd(void)
{
    u32 start_pos = ((u32)(cdb.cr1&255)<<16) | cdb.cr2;
    u32 end_pos = ((u32)(cdb.cr3&255)<<16) | cdb.cr4;
    unsigned mode = cdb.cr3>>8;
    if (!get_track_info(0xffff)) { set_status(STAT_REJECT); return 0; }
    unsigned types = retained_position_type(start_pos, cdb.play_pos_types & 3) |
                     (retained_position_type(end_pos, cdb.play_pos_types >> 2) << 2);
    if (((types | (types >> 2)) & 3) == 3) { set_status(STAT_REJECT); return 0; }
    u32 start = start_pos == 0xffffff ? cdb.play_fad_start : drive_position(start_pos, 0);
    u32 end = cdb.play_fad_end;
    if (end_pos != 0xffffff) {
        if (end_pos & 0x800000) {
            u32 leadout = cdb.tracks[cdb.track_num-1].fad_end+1;
            end = start + (end_pos & 0x7fffff);
            if (end > leadout) end = leadout;
        } else end = drive_position(end_pos, 1);
    }
    if (cdb.play_type) SSLOG(_CDRV, "play_cd: Send PAUSE request!\n");
    if (pause_drive()) { set_status(STAT_WAIT|cdb.status); return 0; }
    SSLOG(_CDRV, "play_cd: start=%08x end=%08x mode=%02x\n", start_pos, end_pos, mode);
    HIRQ_CLR = HIRQ_PEND;
    int repeat = (mode&0x7f) == 0x7f ? cdb.max_repeat : mode&15;
    /* Only a changed range/limit clears history. Observe worker progress
     * after acknowledgement; failed handoff publishes no new settings. */
    if (start != cdb.play_fad_start || end != cdb.play_fad_end || repeat != cdb.max_repeat)
        cdb.repcnt = 0;
    cdb.play_pos_types = types;
    cdb.play_fad_start = start;
    cdb.play_fad_end = end;
    cdb.max_repeat = repeat;
    /* Retain the existing explicit track/data restart used by Steam-Heart's. */
    u32 pickup = cdb.fad == 0xffffffff ? 150 : cdb.fad;
    if (!(mode&0x80)) pickup = start;
    else if (start_pos && start_pos < 0x800000 && end_pos && end_pos < 0x800000) {
        TRACK_INFO *tk = get_track_info(fad_to_track(start)<<8);
        if (tk && tk->mode != 3) pickup = start;
    }
    set_drive_position(pickup);
    cdb.options = 0x08;
    cdb.pause_request = 0;
    cdb.play_wait = 0;
    cdb.play_type = PLAYTYPE_SECTOR;
    disk_task_wakeup();
    set_report(STAT_BUSY);
    return 0;
}

// 0x11 [SR]
int seek_cd(void)
{
    u32 pos = ((u32)(cdb.cr1&255)<<16) | cdb.cr2;
    SSLOG(_CDRV, "\nseek_cd: pos=%08x\n", pos);
    int explicit_seek = pos && pos != 0xffffff;
    int home_pause = pos == 0xffffff && cdb.fad == 0xffffffff;
    if ((explicit_seek || home_pause) && !get_track_info(0xffff)) {
        set_status(STAT_REJECT); return 0;
    }
    u32 fad = explicit_seek ? drive_position(pos, 0) : cdb.fad;
    if (cdb.play_type) SSLOG(_CDRV, "       : Send PAUSE request!\n");
    if (pause_drive()) { set_status(STAT_WAIT|cdb.status); return 0; }
    cdb.status = STAT_PAUSE;
    if (pos == 0xffffff) SSLOG(_CDRV, "       : PAUSE cmd\n");
    if (!pos) {
        cdb.status = STAT_STANDBY;
        cdb.fad = 0xffffffff;
        /* An invalid report must not overwrite the retained repeat count. */
        cdb.track = cdb.index = cdb.ctrladdr = cdb.options = 0xff;
        cdb.play_track = NULL;
    } else if (home_pause) {
        set_drive_position(150);
        cdb.options = 0;
    } else if (explicit_seek) {
        /* SEEK moves the pickup. PLAY owns the retained range and repeat settings. */
        set_drive_position(fad);
        cdb.options = 0;
    }
    SSLOG(_CDRV, "       : fad=%08x track=%d index=%d ctladr=%02x opt=%02x\n",
          cdb.fad, cdb.track, cdb.index, cdb.ctrladdr, cdb.options);
    /* Acknowledge the drive command with BUSY. Status queries see the settled state. */
    set_report(STAT_BUSY);
    if (explicit_seek && cdb.repcnt >= cdb.max_repeat &&
        (cdb.fad < cdb.play_fad_start || cdb.fad >= cdb.play_fad_end))
        return HIRQ_PEND;
    return 0;
}

// 0x12 [SR]
// Scan not support

/******************************************************************************/
// Subcode 相关命令


// 0x20 [S-]
int get_subcode(void)
{
    unsigned type = cdb.cr1 & 0xff;
    if (cdb.trans_type || (type == 1 && cdb.subrw_rp == cdb.subrw_wp)) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    if (type > 1) {
        set_status(STAT_REJECT);
        return 0;
    }

    if (type == 0) {
        /* Q is readable in all drive states (Sega CD I/F, §4.3).
         * A home/stop report has no track or relative position. */
        TRACK_INFO *track = get_track_info(cdb.track << 8);
        u32 fad = cdb.fad;
        u32 relative = track ? fad - track->fad_start : 0xffffffffu;
        u8 *out = cdb.SUBH;
        out[0] = cdb.ctrladdr;
        out[1] = cdb.track;
        out[2] = cdb.index;
        out[3] = relative >> 16;
        out[4] = relative >> 8;
        out[5] = relative;
        out[6] = 0;
        out[7] = fad >> 16;
        out[8] = fad >> 8;
        out[9] = fad;
    }
    command_reply(0x4000 | (cdb.status << 8), type ? 12 : 5, reply_pair(0, 0));
    cdb.trans_type = type ? TRANS_SUBRW : TRANS_SUBQ;
    trans_start();
    return HIRQ_DRDY;
}


/******************************************************************************/
// CD-ROM device 相关命令

/* Caller serializes connection changes; true outputs may share partitions. */
static void disconnect_filter_input(int fid)
{
    if (fid >= MAX_SELECTORS) return;
    if (cdb.trans_type == TRANS_PUT && cdb.trans_part_index == fid) {
        /* Retain the private list until DataEnd/reset after losing its input. */
        cdb.trans_part_index = 0xff;
        ST_CTRL &= ~STIRQ_DAT;
        HIRQ = HIRQ_EHST;
    }
    if (cdb.cddev_filter == fid) cdb.cddev_filter = 0xff;
    for (int i = 0; i < MAX_SELECTORS; i++)
        if (cdb.filter[i].c_false == fid) cdb.filter[i].c_false = 0xff;
}

// 0x30 [SR]
int set_cdev_connect(void)
{
    int fid = cdb.cr3 >> 8;
    SSLOG(_CDRV, "set_cdev_connect: %d\n", fid);
    if (fid >= MAX_SELECTORS && fid != 0xff) {
        set_status(STAT_REJECT);
        return 0;
    }
    int irqs = disable_irq();
    disconnect_filter_input(fid);
    cdb.cddev_filter = fid;
    restore_irq(irqs);
    return HIRQ_ESEL;
}

// 0x31 [S-]
int get_cdev_connect(void)
{
	command_reply((cdb.status<<8), 0, reply_pair(cdb.cddev_filter<<8, 0));

	return 0;
}

// 0x32 [S-]
int get_last_buffer(void)
{
	command_reply((cdb.status<<8), 0, reply_pair(cdb.last_buffer<<8, 0));

	return 0;
}

/******************************************************************************/
// 过滤器相关命令

static void _set_filter(int fid, int mode, int fad, int range)
{
	FILTER *ft = &cdb.filter[fid];
	
	ft->mode = mode;
	ft->fad = fad;
	ft->range = range;
}


/* Keep selector admission shared while each command owns its reply. */
static __attribute__((noinline)) FILTER *command_filter(unsigned fid)
{
    if (fid >= MAX_SELECTORS)
        return NULL;
    return &cdb.filter[fid];
}

// 0x40 [SR]
int set_filter_range(void)
{
	int fid;

	fid = cdb.cr3>>8;
	FILTER *filter = command_filter(fid);
    if (!filter) {
        set_status(STAT_REJECT);
        return 0;
    }
	filter->fad   = ((cdb.cr1&0xff)<<16) | cdb.cr2;
	filter->range = ((cdb.cr3&0xff)<<16) | cdb.cr4;
	SSLOG(_FILTER, "set_filter_range: fid=%d fad=%08x range=%08x\n", fid, filter->fad, filter->range);

	return HIRQ_ESEL;
}

// 0x41 [S-]
int get_filter_range(void)
{
	int fid;
	int fad, range;

	fid = cdb.cr3>>8;
	FILTER *filter = command_filter(fid);
    if (!filter) {
        set_status(STAT_REJECT);
        return 0;
    }

	fad   = filter->fad;
	range = filter->range;

	command_reply((cdb.status<<8) | ((fad>>16)&0xff), fad&0xffff, reply_pair((range>>16)&0xff, range&0xffff));

	return 0;
}

// 0x42 [SR]
int set_filter_subheader(void)
{
	int fid;

	fid = cdb.cr3>>8;
	FILTER *filter = command_filter(fid);
    if (!filter) {
        set_status(STAT_REJECT);
        return 0;
    }

	filter->chan   = cdb.cr1&0xff;
	filter->smmask = cdb.cr2>>8;
	filter->cimask = cdb.cr2&0xff;
	filter->fid    = cdb.cr3&0xff;
	filter->smval  = cdb.cr4>>8;
	filter->cival  = cdb.cr4&0xff;
	SSLOG(_FILTER, "set_filter_subh: fid=%d  FN=%02x CN=%02x SMM=%02x CIM=%02x SM=%02x CI=%02x\n", fid,
		filter->fid, filter->chan,
		filter->smmask, filter->cimask,
		filter->smval, filter->cival
	);

	return HIRQ_ESEL;
}

// 0x43 [S-]
int get_filter_subheader(void)
{
	int fid;

	fid = cdb.cr3>>8;
	FILTER *filter = command_filter(fid);
    if (!filter) {
        set_status(STAT_REJECT);
        return 0;
    }

	command_reply((cdb.status<<8) | filter->chan, (filter->smmask<<8) | filter->cimask, reply_pair(filter->fid, (filter->smval<<8) | filter->cival));

	return 0;
}

// 0x44 [SR]
int set_filter_mode(void)
{
	int fid;

	fid = cdb.cr3>>8;
	FILTER *filter = command_filter(fid);
    if (!filter) {
        set_status(STAT_REJECT);
        return 0;
    }
	filter->mode = cdb.cr1&0xff;
	SSLOG(_FILTER, "set_filter_mode: fid=%d  mode=%02x\n", fid, cdb.cr1&0xff);

	if(filter->mode&0x80){
		reset_filter_conditions(filter);
	}

	return HIRQ_ESEL;
}

// 0x45 [S-]
int get_filter_mode(void)
{
	int fid;

	fid = cdb.cr3>>8;
	FILTER *filter = command_filter(fid);
    if (!filter) {
        set_status(STAT_REJECT);
        return 0;
    }

	command_reply((cdb.status<<8) | filter->mode, 0, reply_pair(0, 0));

	return 0;
}

// 0x46 [SR]
int set_filter_connection(void)
{
    int flag = cdb.cr1 & 0xff, ct = cdb.cr2 >> 8, cf = cdb.cr2 & 0xff;
    int fid = cdb.cr3 >> 8;
    SSLOG(_FILTER, "set_filter_connection: fid=%d  flag=%d true=%02x false=%02x\n", fid, flag, ct, cf);
    if (fid >= MAX_SELECTORS ||
        ((flag & 1) && ct >= MAX_SELECTORS && ct != 0xff) ||
        ((flag & 2) && cf >= MAX_SELECTORS && cf != 0xff)) {
        set_status(STAT_REJECT);
        return 0;
    }
    int irqs = disable_irq();
    if (flag & 1) cdb.filter[fid].c_true = ct;
    if (flag & 2) {
        disconnect_filter_input(cf);
        cdb.filter[fid].c_false = cf;
    }
    restore_irq(irqs);
    return HIRQ_ESEL;
}

// 0x47 [S-]
int get_filter_connection(void)
{
	int fid;

	fid = cdb.cr3>>8;
	FILTER *filter = command_filter(fid);
    if (!filter) {
        set_status(STAT_REJECT);
        return 0;
    }

	command_reply((cdb.status<<8), (filter->c_true<<8) | filter->c_false, reply_pair(0, 0));

	return 0;
}

void free_partition(PARTITION *pt)
{
    int irqs = disable_irq();
    disconnect_partition_output(pt);
    BLOCK *head = pt->head;
    pt->head = NULL;
    pt->tail = NULL;
    pt->numblocks = 0;
    pt->size = -1;
    restore_irq(irqs);
    free_block_list(head);
}


// 0x48 [SR]
int reset_filter(void)
{
	int mode, fid;
	int i;

	mode = cdb.cr1&0xff;
	fid = cdb.cr3>>8;
	SSLOG(_FILTER, "reset_filter: mode=%02x fid=%d\n", mode, fid);

    if (mode == 0 && fid >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }
    /* Nonzero reset modes ignore fid; pause only for incoming data erasure. */
    if (((mode & 0x04) || (mode == 0 && fid == cdb.cddev_filter)) &&
        cdb.cddev_filter < MAX_SELECTORS && cdb.play_type != 0 &&
        cdb.play_track && cdb.play_track->mode != 3) {
		SSLOG(_CDRV, "reset_filter: Send PAUSE request!\n");
        if (pause_drive()) { set_status(STAT_WAIT|cdb.status); return 0; }
	}

	if(mode==0){
		if(fid<MAX_SELECTORS){
			free_partition(&cdb.part[fid]);
		}
	}else{
        if (mode & 0x20) {
            int irqs = disable_irq();
            if (cdb.trans_type == TRANS_PUT)
                disconnect_filter_input(cdb.trans_part_index);
            cdb.cddev_filter = 0xff;
            restore_irq(irqs);
        }
		for(i=0; i<MAX_SELECTORS; i++){
			if(mode & (0x80|0x20)){
				cdb.filter[i].c_false = 0xff;
			}
			if(mode&0x40){
				cdb.filter[i].c_true = i;
			}
			if(mode&0x10){
				reset_filter_conditions(&cdb.filter[i]);
			}
			if(mode&0x04){
				free_partition(&cdb.part[i]);
			}
		}

		/* Only partition-owned blocks were released above. */
	}

	return HIRQ_ESEL;
}

/******************************************************************************/
// buffer info 相关命令


u8 old_block_free;

// 0x50 [S-]
int get_buffer_size(void)
{
	command_reply((cdb.status<<8), cdb.block_free, reply_pair(MAX_SELECTORS<<8, MAX_BLOCKS));

	if(old_block_free!=cdb.block_free){
		SSLOG(_BUFINFO, "get_buffer_size: free=%d\n", cdb.block_free);
	}
	old_block_free = cdb.block_free;

	return 0;
}

static int old_numblocks;

// 0x51 [S-]
int get_sector_num(void)
{
	int pt;

	pt = cdb.cr3>>8;
    if (pt >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }

	command_reply((cdb.status<<8), 0, reply_pair(0, cdb.part[pt].numblocks));

	if(old_numblocks != cdb.part[pt].numblocks){
		SSLOG(_BUFINFO, "get_sector_num: part=%d(%d)\n", pt, cdb.part[pt].numblocks);
	}
	old_numblocks = cdb.part[pt].numblocks;

	return 0;
}

// 0x52 [SR]
int cal_actual_size(void)
{
	int pt, spos, snum, size;
	BLOCK *bp;

	spos = cdb.cr2;
	pt   = cdb.cr3>>8;
	snum = cdb.cr4;
    if (pt >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }
	SSLOG(_BUFINFO, "cacl_act_size: pt=%d spos=%d snum=%d\n", pt, spos, snum);

    if (!sector_range(&cdb.part[pt], &spos, &snum)) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    disconnect_partition_output(&cdb.part[pt]);
    size = snum*cdb.sector_size;
    if (cdb.sector_size == 2048) {
        bp = find_block(&cdb.part[pt], spos);
        size = 0;
        while (snum--) {
            if (!bp) { size = 0x01fffffe; break; }
            size += sector_transfer_size(bp);
            bp = bp->next;
        }
    }
	cdb.actsize = size;

	return HIRQ_ESEL;
}

// 0x53 [S-]
int get_actual_size(void)
{
	command_reply((cdb.status<<8) | (((cdb.actsize/2)>>16)&0xff), (cdb.actsize/2)&0xffff, reply_pair(0, 0));

	SSLOG(_BUFINFO, "get_actual_size: %08x\n", cdb.actsize);

	return 0;
}

// 0x54 [S-]
int get_sector_info(void)
{
	int pt, st;
	PARTITION *pp;
	BLOCK *bp;

	pt = cdb.cr3>>8;
    if (pt >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }
	st = cdb.cr2;

	pp = &cdb.part[pt];
    if (st == 0xffff) st = pp->numblocks-1;
    if (st < 0 || st >= pp->numblocks) {
        set_status(STAT_REJECT);
        return 0;
    }
    bp = find_block(pp, st);
    if (!bp) {
        set_status(STAT_REJECT);
        return 0;
    }

	command_reply((cdb.status<<8) | ((bp->fad>>16)&0xff), (bp->fad)&0xffff, reply_pair((bp->fn<<8) | (bp->cn), (bp->sm<<8) | (bp->ci)));

	return 0;
}

// 0x55 [SR]
// 0x56 [SR]


/******************************************************************************/
// buffer I/O相关命令


static const u16 sector_len_table[4] = {2048, 2336, 2340, 2352};

// 0x60 [SR]
int set_sector_length(void)
{
	int id_get, id_put;

	id_get = SSCR1&0xff;
	id_put = SSCR2>>8;

	SSLOG(_INFO, "set_sector_length:  get=%d put=%d\n", id_get, id_put);
	if(id_get<4){
		cdb.sector_size = sector_len_table[id_get];
	}
	if(id_put<4){
		cdb.put_sector_size = sector_len_table[id_put];
	}

	return HIRQ_ESEL;
}

// 0x61 [SR]
/* Both data commands have the same range/transfer contract. */
static __attribute__((noinline)) int get_sector_data_common(int type)
{
    if (cdb.trans_type) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    int pt = cdb.cr3 >> 8, spos = cdb.cr2, snum = cdb.cr4;
    if (pt >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }
    PARTITION *pp = &cdb.part[pt];
    if (type == TRANS_DATA_DEL)
        SSLOG(_BUFIO, "get_del_sector_data: part=%d(%d) spos=%d snum=%d\n", pt, pp->numblocks, spos, snum);
    int valid = sector_range(pp, &spos, &snum);
    SSLOG(_BUFIO, type == TRANS_DATA ? "get_sector_data: part=%d(%d) spos=%d snum=%d\n" :
                                      "                   : part=%d(%d) spos=%d snum=%d\n",
          pt, pp->numblocks, spos, snum);
    if (!valid) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    SSLOG(_TRANS, "trans_start: pt=%d start=%d size=%d\n", pt, spos, snum);
    /* Logs may wait on UART capacity and let disk work mutate the partition.
     * Re-admit the original request before publishing any borrowed pointer. */
    int irqs = disable_irq();
    spos = cdb.cr2;
    snum = cdb.cr4;
    BLOCK *head = NULL;
    if (sector_range(pp, &spos, &snum))
        head = type == TRANS_DATA_DEL ? detach_sector_range(pp, spos, snum) : find_block(pp, spos);
    if (!head) {
        restore_irq(irqs);
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    cdb.trans_type = type;
    cdb.trans_part_index = pt;
    cdb.trans_block_start = head-cdb.block;
    cdb.trans_block_end = snum;
    cdb.trans_block = head;
    restore_irq(irqs);
    trans_start();
    set_status(STAT_TRNS | cdb.status);
    /* DataEnd or cancellation signals completion of the host transfer. */
    return HIRQ_DRDY;
}
int get_sector_data(void)
{
    return get_sector_data_common(TRANS_DATA);
}


// 0x62 [SR]
int del_sector_data(void)
{
	int pt, spos, snum;
	PARTITION *pp;

	pt   = cdb.cr3>>8;
    if (pt >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }
	spos = cdb.cr2;
	snum = cdb.cr4;

	pp = &cdb.part[pt];

    int valid = sector_range(pp, &spos, &snum);
	SSLOG(_BUFIO, "del_sector_data: part=%d spos=%d snum=%d\n", pt, spos, snum);
	if(!valid){
		// 梦幻之星1会传入spos=180导致后续错误
		set_status(STAT_WAIT | cdb.status);
		return 0;
	}

    int irqs = disable_irq();
    spos = cdb.cr2;
    snum = cdb.cr4;
    BLOCK *head = NULL;
    if (sector_range(pp, &spos, &snum)) head = detach_sector_range(pp, spos, snum);
    if (head) disconnect_partition_output(pp);
    restore_irq(irqs);
    if (!head) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    free_block_list(head);

	return HIRQ_EHST;
}

// 0x63 [SR]
int get_del_sector_data(void)
{
    return get_sector_data_common(TRANS_DATA_DEL);
}


// 0x64 [SR]
int put_sector_data(void)
{
    if (cdb.trans_type) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    int fid = cdb.cr3 >> 8, count = cdb.cr4;
    if (fid >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }
    if (count <= 0 || count > cdb.block_free) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    SSLOG(_BUFIO, "put_sector_data: part=%d snum=%d put_sector_size=%d\n", fid, count, cdb.put_sector_size);
    int irqs = disable_irq();
    disconnect_filter_input(fid);
    restore_irq(irqs);
    BLOCK *head = NULL, *tail = NULL;
    int size = cdb.put_sector_size;
    int offs = size == 2048 ? 24 : 2352-size;
    /* Reserve before DRDY, outside partitions; no input-time allocations.
     * PUT uses the existing start byte as a pool index, GET as a position. */
    int remaining = count;
    while (remaining--) {
        BLOCK *bt = alloc_block();
        if (!bt) {
            while (head) {
                BLOCK *next = head->next;
                free_block(head);
                head = next;
            }
            cdb.cdwnum = 0;
            set_status(cdb.status);
            return HIRQ_EHST;
        }
        bt->size = 0;
        bt->next = NULL;
        memset(bt->data, 0, offs);
        if (tail) tail->next = bt;
        else head = bt;
        tail = bt;
    }
    cdb.trans_type = TRANS_PUT;
    cdb.trans_part_index = fid;
    cdb.trans_block_start = head-cdb.block;
    cdb.trans_block = head;
    cdb.trans_bk = count;
    cdb.trans_size = size;
    cdb.cdwnum = 0;
    ST_CTRL |= 0x0100;
    ST_CTRL &= ~0x0100;
    ST_STAT = STIRQ_DAT;
    ST_CTRL |= 0x0200 | STIRQ_DAT;
    set_status(STAT_TRNS | cdb.status);
    return HIRQ_DRDY;
}



// 0x65 [SR]
/* Copy/move share admission, destination filtering and input ownership. */
static __attribute__((noinline)) int copy_move_sector_data(int moving)
{
    int src = cdb.cr3 >> 8, dst = cdb.cr1 & 0xff;
    int spos = cdb.cr2, snum = cdb.cr4;
    if (src >= MAX_SELECTORS || dst >= MAX_SELECTORS) {
        set_status(STAT_REJECT);
        return 0;
    }
    PARTITION *ps = &cdb.part[src];
    if (moving) {
        SSLOG(_BUFIO, "move_sector_data: src=%d(%d) spos=%d snum=%d dst=%d\n", src, ps->numblocks, spos, snum, dst);
    } else {
        SSLOG(_BUFIO, "copy_sector_data: src=%d(%d) spos=%d snum=%d dst=%d\n", src, ps->numblocks, spos, snum, dst);
    }
    int valid = sector_range(ps, &spos, &snum);
    SSLOG(_BUFIO, "                : src=%d(%d) spos=%d snum=%d\n", src, ps->numblocks, spos, snum);
    if (cdb.trans_type || !valid ||
        (!moving && snum > cdb.block_free)) {
        set_status(STAT_WAIT | cdb.status);
        return 0;
    }
    int irqs = disable_irq();
    disconnect_filter_input(dst);
    restore_irq(irqs);
    copy_move_error = 0;
    BLOCK *prev = NULL, *block = ps->head;
    for (int i = 0; block && i < spos; i++) {
        prev = block;
        block = block->next;
    }
    while (snum--) {
        if (!block) { copy_move_error = 1; break; }
        BLOCK *next = block->next;
        int target = sector_destination(dst, block, 0);
        if (moving) {
            int irqs = disable_irq();
            unlink_block(ps, prev, block);
            if (target < MAX_SELECTORS) append_block(&cdb.part[target], block);
            else free_block(block);
            restore_irq(irqs);
        } else if (target < MAX_SELECTORS) {
            BLOCK *copy = dup_block(block);
            if (!copy) { copy_move_error = 1; break; }
            append_block(&cdb.part[target], copy);
        }
        block = next;
    }
    set_status(cdb.status);
    return HIRQ_ECPY;
}

int copy_sector_data(void)
{
    return copy_move_sector_data(0);
}

// 0x66 [SR]
int move_sector_data(void)
{
    return copy_move_sector_data(1);
}

// 0x67 [S-]
int get_copy_error(void)
{
	command_reply((cdb.status<<8) | copy_move_error, 0, reply_pair(0, 0));

	return 0;
}

/******************************************************************************/

/* Directory state: 0 invalid, 1 volume descriptors, 2 records, 3 valid page.
 * Only the disk worker advances the parser; command readers wait while active.
 * Stop/replace commands wait for the worker before changing its source state. */
static u32 directory_record;

static u32 isonum_733(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16) | ((u32)p[3]<<24);
}

/* ISO 9660 records cannot cross a logical-sector boundary. No temporary
 * terminator: filenames are not part of the CD file reply. The old trace
 * uses a separate bounded name copy only when its existing log mask is set. */
static int read_directory_record(FILE_INFO *fi, const u8 *p, unsigned available)
{
    if (available < 34 || p[0] < 34 || p[0] > available ||
        !p[32] || 33u + p[32] + !(p[32] & 1) > p[0]) return 0;
    unsigned su = 33u + (p[32] | 1u);
    unsigned su_size = p[0]-su;
    int xa = su_size >= 8 && p[su+6] == 'X' && p[su+7] == 'A';
    if (xa && su_size < 14) return 0;
    /* An interleaved extended attribute occupies the entire first unit;
     * file data starts after that unit's following gap (ECMA-119 6.4.3). */
    unsigned prefix = p[1];
    if (!p[26] && p[27]) return 0;
    if (p[26] && prefix) {
        if (prefix != p[26]) return 0;
        prefix += p[27];
    }
    u32 lba = isonum_733(p+2);
    if (lba > 0xffffffffu-prefix) return 0;
    fi->lba = lba+prefix;
    fi->size = isonum_733(p+10);
    fi->attr = (p[25] & 2) | (xa ? (p[su+4] & 0xf8) | FILE_ATTR_XA : 0);
    fi->unit = p[26];
    fi->gap = p[27];
    fi->fid = xa ? p[su+8] : 0;
    return p[0];
}

/* XA presence uses a reserved internal bit; the 12-byte reply masks it out.
 * With XA metadata, its directory flag takes precedence over the ISO flag. */
static int file_is_directory(const FILE_INFO *fi)
{
    return fi->attr & ((fi->attr & FILE_ATTR_XA) ? 0x80 : 2);
}

/* Logical sector indices exclude gaps. UINT32_MAX-byte files have at most
 * 2^21 sectors, so multiplication by an eight-bit gap cannot overflow. */
static u32 file_sector_offset(u32 sector, unsigned unit, unsigned gap)
{
    return sector + (gap ? sector/unit*gap : 0);
}

/* Advance only after consumption/discard, never on a full-buffer retry.
 * Unit boundaries use comparisons/addition, with division confined to setup. */
void advance_file_unit(void)
{
    if (cdb.file_gap && cdb.fad == cdb.file_next_unit &&
        cdb.fad < cdb.play_fad_end) {
        cdb.fad += cdb.file_gap;
        cdb.file_next_unit = cdb.fad+cdb.file_unit;
    }
}

static u32 file_sector_count(u32 bytes)
{
    return (bytes >> 11) + ((bytes & 2047) != 0);
}

/* Keep on-disc LBAs and counts in the 24-bit CD command address domain. */
static int valid_file_extent(u32 lba, u32 sectors)
{
    return sectors && lba <= 0xffffffu-150 &&
           sectors <= 0x1000000u-(lba+150);
}

void invalidate_directory(void)
{
    cdb.rdir_state = 0;
    cdb.cdir_pfnum = 0;
    cdb.cdir_drend = 0;
}

int finish_directory(int valid)
{
    if (valid) {
        SSLOG(_FILEIO, "  cdir_pfid=%d fnum=%d\n", cdb.cdir_pfid, cdb.cdir_pfnum);
    }
    int irqs = disable_irq();
    if (valid) cdb.rdir_state = 3;
    else invalidate_directory();
    cdb.status = valid ? STAT_PAUSE : STAT_ERROR;
    cdb.play_type = 0;
    cdb.play_track = NULL;
    HIRQ = HIRQ_EFLS;
    restore_irq(irqs);
    return 0;
}

/* A working selector uses its own partition, independent of its old true
 * output. Configure connections through the same ownership rules as commands. */
static void set_file_selector(int selnum, u32 fad, u32 count)
{
    int irqs = disable_irq();
    disconnect_filter_input(selnum);
    cdb.cddev_filter = selnum;
    cdb.filter[selnum].c_true = selnum;
    cdb.filter[selnum].c_false = 0xff;
    _set_filter(selnum, 0x40, fad, count);
    restore_irq(irqs);
}

static void start_file_play(int selnum, u32 fad, u32 sectors, int type)
{
    free_partition(&cdb.part[selnum]);
    set_file_selector(selnum, fad, sectors);
    cdb.play_pos_types = 0;
    cdb.repcnt = 0;
    cdb.max_repeat = 0;
    /* Commands already paused. Internal directory phases must retain a new
     * pause request until the worker acknowledges it. */
    cdb.play_wait = 0;
    cdb.file_gap = 0;
    cdb.play_type = type;
    cdb.play_fad_start = fad;
    cdb.play_fad_end = fad+sectors;
    cdb.track = fad_to_track(fad);
    cdb.old_fad = cdb.fad;
    cdb.fad = fad;
}

static __attribute__((noinline)) void start_directory_read(int selnum)
{
    cdb.dir_filter = selnum;
    cdb.rdir_state = 2;
    cdb.cdir_pfnum = 0;
    cdb.cdir_drend = 0;
    if (cdb.cdir_pfid < 2) cdb.cdir_pfid = 2;
    directory_record = 0;
    start_file_play(selnum, cdb.cdir_lba+150,
                    file_sector_count(cdb.cdir_size), PLAYTYPE_DIR);
    disk_task_wakeup();
}

/* Preserve the original trace without writing a terminator into the record.
 * Only called with validated records, outside masks and while owning the block. */
static __attribute__((noinline)) void log_directory_record(u32 fid, const FILE_INFO *fi, const u8 *p)
{
    if (log_mask & _FILEIO) {
        char name[256];
        int nlen = p[32];
        if (nlen > 2) nlen -= 2;
        memcpy(name, p+33, nlen);
        name[nlen] = 0;
        SSLOG(_FILEIO, "  fid %3d:  lba=%08x size=%08x attr=%02x  %d %s\n",
              fid, fi->lba, fi->size, fi->attr, nlen, name);
    }
}

/* Consume one privately detached sector. Return 1 for more input, 0 for a
 * complete page, -1 for invalid records. One extra record establishes more. */
static int fill_fileinfo(const u8 *buf, unsigned available)
{
    unsigned offset = 0;
    while (offset < available && buf[offset]) {
        FILE_INFO fi;
        int length = read_directory_record(&fi, buf+offset, available-offset);
        if (!length) return -1;
        if (directory_record < 2) {
            if (!file_is_directory(&fi) || buf[offset+32] != 1 ||
                buf[offset+33] != directory_record) return -1;
            cdb.fi[directory_record] = fi;
            if (cdb.cdir_pfid == 2) log_directory_record(directory_record, &fi, buf+offset);
        } else if (directory_record >= cdb.cdir_pfid) {
            if (cdb.cdir_pfnum == 254) return 0;
            cdb.fi[cdb.cdir_pfnum+2] = fi;
            cdb.cdir_pfnum++;
            log_directory_record(directory_record, &fi, buf+offset);
        }
        directory_record++;
        offset += length;
    }
    return 1;
}

/* Called immediately after routing each directory sector. Public commands may
 * preempt parsing, so detach before following payload pointers, without copying.
 * A reset/abort waits for this worker and cannot reuse its private block. */
int handle_diread(void)
{
    SSLOG(_FILEIO, "handle_diread %d\n", cdb.rdir_state);
    int sel = cdb.dir_filter;
    if (cdb.play_type != PLAYTYPE_DIR) return 0;
    if (sel >= MAX_SELECTORS || cdb.cddev_filter != sel ||
        cdb.filter[sel].c_true != sel) return finish_directory(0);
    int irqs = disable_irq();
    PARTITION *pt = &cdb.part[sel];
    BLOCK *blk = pt->head;
    if (!blk || blk->fad != cdb.fad) {
        restore_irq(irqs);
        return finish_directory(0);
    }
    disconnect_partition_output(pt);
    unlink_block(pt, NULL, blk);
    restore_irq(irqs);
    const u8 *buf = blk->data+24;
    int result = -1;
    if (cdb.rdir_state == 1) {
        FILE_INFO root;
        if (buf[6] == 1 && !memcmp(buf+1, "CD001", 5)) {
            if (buf[0] == 1 && buf[128] == 0 && buf[129] == 8 &&
                read_directory_record(&root, buf+156, 34) &&
                file_is_directory(&root) && !buf[189] && !root.unit && !root.gap &&
                valid_file_extent(root.lba, file_sector_count(root.size))) {
                cdb.root_lba = root.lba;
                cdb.root_size = root.size;
                SSLOG(_FILEIO, "  root_lba=%08x size=%08x\n", cdb.root_lba, file_sector_count(cdb.root_size));
                result = 2;
            } else if (buf[0] != 1 && buf[0] != 255 && cdb.fad < 0xffffff) {
                cdb.play_fad_end++;
                cdb.filter[sel].range++;
                result = 1;
            }
        }
    } else if (cdb.rdir_state == 2) {
        SSLOG(_FILEIO, "  cdir_lba=%08x size=%08x\n", cdb.cdir_lba, file_sector_count(cdb.cdir_size));
        u32 consumed = (cdb.fad-cdb.play_fad_start)*2048u;
        if (consumed < cdb.cdir_size) {
            u32 available = cdb.cdir_size-consumed;
            if (available > 2048) available = 2048;
            result = fill_fileinfo(buf, available);
            if (result == 1 && available == cdb.cdir_size-consumed &&
                cdb.fad+1 == cdb.play_fad_end) {
                if (directory_record < 2) result = -1;
                else { cdb.cdir_drend = 1; result = 0; }
            }
        }
    }
    free_block(blk);
    if (result == 2) {
        cdb.cdir_lba = cdb.root_lba;
        cdb.cdir_size = cdb.root_size;
        start_directory_read(sel);
        return 2;
    }
    if (result <= 0) return finish_directory(result == 0);
    return 1;
}

static int file_info_index(u32 fid)
{
    if (cdb.rdir_state != 3 || cdb.cdir_pfnum > 254) return -1;
    if (fid < 2) return fid;
    if (fid < cdb.cdir_pfid || fid-cdb.cdir_pfid >= cdb.cdir_pfnum) return -1;
    return fid-cdb.cdir_pfid+2;
}

// 0x70 [SR]
int change_dir(void)
{
    int sel = cdb.cr3 >> 8;
    u32 fid = ((u32)(cdb.cr3 & 255)<<16) | cdb.cr4;
    if (sel >= MAX_SELECTORS) { set_status(STAT_REJECT); return 0; }
    SSLOG(_FILEIO, "change_dir: sel=%d fid=%08x root_lba=%08x\n", sel, fid, cdb.root_lba);
    if (cdb.play_type == PLAYTYPE_DIR) { set_status(STAT_WAIT|cdb.status); return 0; }
    u32 lba = cdb.root_lba, bytes = cdb.root_size;
    if (fid != 0xffffff) {
        int index = file_info_index(fid);
        if (index < 0 || !file_is_directory(&cdb.fi[index]) ||
            cdb.fi[index].unit || cdb.fi[index].gap) {
            set_status(STAT_REJECT); return 0;
        }
        lba = cdb.fi[index].lba; bytes = cdb.fi[index].size;
    }
    if ((lba || fid != 0xffffff) &&
        !valid_file_extent(lba, file_sector_count(bytes))) {
        set_status(STAT_REJECT); return 0;
    }
    if (pause_disc()) { set_status(STAT_WAIT|cdb.status); return 0; }
    cdb.cdir_pfid = 2;
    if (fid == 0xffffff && !cdb.root_lba) {
        invalidate_directory();
        cdb.dir_filter = sel;
        cdb.rdir_state = 1;
        start_file_play(sel, 166, 1, PLAYTYPE_DIR);
        disk_task_wakeup();
    } else {
        cdb.cdir_lba = lba; cdb.cdir_size = bytes;
        start_directory_read(sel);
    }
    set_report(cdb.status);
    return 0;
}

// 0x71 [SR]
int read_dir(void)
{
    int sel = cdb.cr3 >> 8;
    u32 fid = ((u32)(cdb.cr3 & 255)<<16) | cdb.cr4;
    if (sel >= MAX_SELECTORS) { set_status(STAT_REJECT); return 0; }
    SSLOG(_FILEIO, "read_dir: sel=%d fid=%08x\n", sel, fid);
    if (cdb.play_type == PLAYTYPE_DIR) { set_status(STAT_WAIT|cdb.status); return 0; }
    if (cdb.rdir_state != 3 ||
        !valid_file_extent(cdb.cdir_lba, file_sector_count(cdb.cdir_size))) {
        set_status(STAT_REJECT); return 0;
    }
    if (pause_disc()) { set_status(STAT_WAIT|cdb.status); return 0; }
    cdb.cdir_pfid = fid;
    start_directory_read(sel);
    set_report(cdb.status);
    return 0;
}

// 0x72 [S-]
int get_file_scope(void)
{
    SSLOG(_FILEIO, "get_file_scope: pfid=%d  pfnum=%d\n", cdb.cdir_pfid, cdb.cdir_pfnum);
    if (cdb.play_type == PLAYTYPE_DIR) { set_status(STAT_WAIT|cdb.status); return 0; }
    if (cdb.rdir_state != 3) { set_status(STAT_REJECT); return 0; }
    command_reply(cdb.status << 8, cdb.cdir_pfnum,
                  reply_pair((cdb.cdir_drend << 8) | (cdb.cdir_pfid >> 16), cdb.cdir_pfid));
    return 0;
}

// 0x73 [S-]
int get_file_info(void)
{
    if (cdb.trans_type || cdb.play_type == PLAYTYPE_DIR) {
        set_status(STAT_WAIT|cdb.status); return 0;
    }
    u32 fid = ((u32)(cdb.cr3&255)<<16) | cdb.cr4;
    SSLOG(_FILEIO, "get_file_info: fid=%08x\n", fid);
    int index = fid == 0xffffff ? 2 : file_info_index(fid);
    int count = fid == 0xffffff ? cdb.cdir_pfnum : 1;
    if (cdb.rdir_state != 3 || index < 0 || !count || count > 254) {
        set_status(STAT_REJECT); return 0;
    }
    for (int i = 0; i < count; i++) {
        FILE_INFO *fi = &cdb.fi[index+i];
        u8 *out = cdb.FINFO+i*12;
        *(u32*)(out+0) = bswap32(fi->lba+150);
        *(u32*)(out+4) = bswap32(fi->size);
        out[8] = fi->gap; out[9] = fi->unit; out[10] = fi->fid; out[11] = fi->attr & 0xfa;
    }
    cdb.trans_type = fid == 0xffffff ? TRANS_FINFO_ALL : TRANS_FINFO_ONE;
    trans_start();
    SSCR1 = 0x4000 | (cdb.status<<8);
    SSCR2 = count*6;
    SSCR3 = SSCR4 = 0;
    return HIRQ_DRDY;
}

// 0x74 [SR]
int read_file(void)
{
    int sel = cdb.cr3 >> 8;
    if (sel >= MAX_SELECTORS) { set_status(STAT_REJECT); return 0; }
    if (cdb.play_type == PLAYTYPE_DIR) { set_status(STAT_WAIT|cdb.status); return 0; }
    u32 fid = ((u32)(cdb.cr3 & 255)<<16) | cdb.cr4;
    int index = file_info_index(fid);
    if (index < 0) { set_status(STAT_REJECT); return 0; }
    FILE_INFO fi = cdb.fi[index];
    u32 sectors = file_sector_count(fi.size);
    u32 offset = ((u32)(cdb.cr1 & 255)<<16) | cdb.cr2;
    if (offset >= sectors || (!fi.unit && fi.gap)) {
        set_status(STAT_REJECT); return 0;
    }
    u32 span = file_sector_offset(sectors-1, fi.unit, fi.gap)+1;
    if (!valid_file_extent(fi.lba, span)) { set_status(STAT_REJECT); return 0; }
    u32 skip = file_sector_offset(offset, fi.unit, fi.gap);
    if (cdb.play_type) SSLOG(_FILEIO, "read_file: waiting ...\n");
    SSLOG(_FILEIO, "read_file: sel=%d fid=%08x\n", sel, fid);
    if (pause_disc()) { set_status(STAT_WAIT|cdb.status); return 0; }
    start_file_play(sel, fi.lba+skip+150, span-skip, PLAYTYPE_FILE);
    cdb.filter[sel].fid = fi.fid;
    cdb.filter[sel].mode = 0x41; /* FAD range and file-number selection. */
    cdb.file_unit = fi.unit;
    cdb.file_gap = fi.gap;
    if (fi.gap) cdb.file_next_unit = cdb.fad+fi.unit-(offset%fi.unit);
    cdb.options = 0x08;
    disk_task_wakeup();
    set_report(cdb.status);
    return 0;
}

// 0x75 [SR]
int abort_file(void)
{
    SSLOG(_FILEIO, "abort_file\n");
    int directory = cdb.play_type == PLAYTYPE_DIR;
    if (directory || cdb.play_type == PLAYTYPE_FILE) {
        if (pause_disc()) { set_status(STAT_WAIT|cdb.status); return 0; }
        if (directory) invalidate_directory();
    }
    return HIRQ_EFLS;
}

/******************************************************************************/

// 0xe0 [S-]
int auth_device(void)
{
	int mpeg_auth, hirq;

	SSLOG(_INFO, "auth_device\n");

	mpeg_auth = cdb.cr2&0xff;
	if(mpeg_auth){
		hirq = HIRQ_MPED;
		cdb.mpeg_auth = 2;
	}else{
		hirq = HIRQ_EFLS | HIRQ_CSCT;
		cdb.saturn_auth = 4;
	}

	command_reply((STAT_BUSY<<8) | 0xff, 0xffff, reply_pair(0xffff, 0xffff));

	return hirq;
}

// 0xe1 [S-]
int get_auth_status(void)
{
	SSLOG(_INFO, "get_auth_status\n");

	if(cdb.cr2&0xff){
		SSCR2 = cdb.mpeg_auth;
	}else{
		SSCR2 = cdb.saturn_auth;
	}
	SSCR1 = (cdb.status<<8);
	SSCR3 = 0;
	SSCR4 = 0;

	return 0;
}


/******************************************************************************/

char *cmd_str(int cmd)
{
	switch(cmd){
	// common
	case 0x00: return "get_cd_status";
	case 0x01: return "get_hw_info";
	case 0x02: return "get_toc_info";
	case 0x03: return "get_session_info";
	case 0x04: return "init_cdblock";
	case 0x06: return "end_trans";

	// cd drive
	case 0x10: return "play_cd";
	case 0x11: return "seek_cd";

	// subcode
	case 0x20: return "get_subcode";

	// cd device
	case 0x30: return "set_cdev_connect";
	case 0x31: return "get_cdev_connect";
	case 0x32: return "get_last_buffer";

	// filter
	case 0x40: return "set_filter_range";
	case 0x41: return "get_filter_range";
	case 0x42: return "set_filter_subheader";
	case 0x43: return "get_filter_subheader";
	case 0x44: return "set_filter_mode";
	case 0x45: return "get_filter_mode";
	case 0x46: return "set_filter_connection";
	case 0x47: return "get_filter_connection";
	case 0x48: return "reset_filter";

	// buffer info
	case 0x50: return "get_buffer_size";
	case 0x51: return "get_sector_num";
	case 0x52: return "cal_actual_size";
	case 0x53: return "get_actual_size";
	case 0x54: return "get_sector_info";

	// buffer io
	case 0x60: return "set_sector_length";
	case 0x61: return "get_sector_data";
	case 0x62: return "del_sector_data";
	case 0x63: return "get_del_sector_data";
	case 0x64: return "put_sector_data";
	case 0x65: return "copy_sector_data";
	case 0x66: return "move_sector_data";
	case 0x67: return "get_copy_error";

	// file io
	case 0x70: return "change_dir";
	case 0x71: return "read_dir";
	case 0x72: return "get_file_scope";
	case 0x73: return "get_file_info";
	case 0x74: return "read_file";
	case 0x75: return "abort_file";

	case 0xe0: return "auth_device";
	case 0xe1: return "get_auth_status";

	default  : return "unkonw_cmd";
	}
}

void cdc_cmd_process(void)
{
	u32 cmd, hirq;

	if((SS_CTRL&0x8000)==0)
		return;

	cmd = (cdb.cr1>>8)&0xff;

	//if(cmd!=0)
		SSLOG(_DEBUG, "\nSS CDC: HIRQ=%04x STAT=%02x  %s\n", HIRQ, cdb.status, cmd_str(cmd));

	cmd = (cdb.cr1>>8)&0xff;
	switch(cmd){
	// common
	case 0x00: hirq = get_cd_status();
		break;
	case 0x01: hirq = get_hw_info();
		break;
	case 0x02: hirq = get_toc_info();
		break;
	case 0x03: hirq = get_session_info();
		break;
	case 0x04: hirq = init_cdblock();
		break;
	case 0x05: hirq = open_tray();
		break;
	case 0x06: hirq = end_trans();
		break;

	// cd drive
	case 0x10: hirq = play_cd();
		break;
	case 0x11: hirq = seek_cd();
		break;

	// subcode
	case 0x20: hirq = get_subcode();
		break;

	// cd device
	case 0x30: hirq = set_cdev_connect();
		break;
	case 0x31: hirq = get_cdev_connect();
		break;
	case 0x32: hirq = get_last_buffer();
		break;

	// filter
	case 0x40: hirq = set_filter_range();
		break;
	case 0x41: hirq = get_filter_range();
		break;
	case 0x42: hirq = set_filter_subheader();
		break;
	case 0x43: hirq = get_filter_subheader();
		break;
	case 0x44: hirq = set_filter_mode();
		break;
	case 0x45: hirq = get_filter_mode();
		break;
	case 0x46: hirq = set_filter_connection();
		break;
	case 0x47: hirq = get_filter_connection();
		break;
	case 0x48: hirq = reset_filter();
		break;

	// buffer info
	case 0x50: hirq = get_buffer_size();
		break;
	case 0x51: hirq = get_sector_num();
		break;
	case 0x52: hirq = cal_actual_size();
		break;
	case 0x53: hirq = get_actual_size();
		break;
	case 0x54: hirq = get_sector_info();
		break;

	// buffer io
	case 0x60: hirq = set_sector_length();
		break;
	case 0x61: hirq = get_sector_data();
		break;
	case 0x62: hirq = del_sector_data();
		break;
	case 0x63: hirq = get_del_sector_data();
		break;
	case 0x64: hirq = put_sector_data();
		break;
	case 0x65: hirq = copy_sector_data();
		break;
	case 0x66: hirq = move_sector_data();
		break;
	case 0x67: hirq = get_copy_error();
		break;

	// file io
	case 0x70: hirq = change_dir();
		break;
	case 0x71: hirq = read_dir();
		break;
	case 0x72: hirq = get_file_scope();
		break;
	case 0x73: hirq = get_file_info();
		break;
	case 0x74: hirq = read_file();
		break;
	case 0x75: hirq = abort_file();
		break;

	case 0xe0: hirq = auth_device();
		break;
	case 0xe1: hirq = get_auth_status();
		break;

	default:
		printk("SS CDC: unknow cmd!  CR1=%04x CR2=%04x CR3=%04x CR4=%04x\n",
			cdb.cr1, cdb.cr2, cdb.cr3, cdb.cr4);
		hirq = 0;
		break;
	}

	// 很多游戏依靠检测SCDQ来等待数据。这里当有数据时，始终发送SCDQ。
	if(cdb.block_free!=MAX_BLOCKS){
		hirq |= HIRQ_SCDQ;
	}
	HIRQ = hirq | HIRQ_CMOK;
#if 0
	if(cmd!=0)
		printk("      : CR1=%04x CR2=%04x CR3=%04x CR4=%04x HIRQ=%04x\n", RESP1, RESP2, RESP3, RESP4, HIRQ);
#endif

}


/******************************************************************************/

