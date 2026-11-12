
#include "main.h"
#include "RTX_Config.h"
#include "ff.h"
#include "cdc.h"

/******************************************************************************/

osSemaphoreId_t sem_wait_irq;
osSemaphoreId_t sem_wait_disc;
osSemaphoreId_t sem_wait_pause;
osMutexId_t mutex_fs;

int lang_id = 0;
int debug_flags = 0;
int sector_delay = 0;
int sector_delay_force = -1;
int play_delay = 0;
int play_delay_force = -1;
int pend_delay = 0;
int pend_delay_force = -1;
int auto_update = 0;
char mdisc_str[16];
int next_disc = -1;

int log_mask = LOG_MASK_DEFAULT;

static int hw_delay(int us);


/******************************************************************************/


// Track 信息
void init_toc(void)
{
	TRACK_INFO *last;
	int i;

	if(cdb.track_num==0)
		return;

	for(i=0; i<102; i++){
		cdb.TOC[i] = 0xffffffff;
	}

	for(i=0; i<cdb.track_num; i++){
		cdb.TOC[i] = bswap32((TRACK_CTRL(&cdb.tracks[i])<<24) | cdb.tracks[i].fad_start);
		SSLOG(_INFO, "track %2d: fad_0=%08x fad_start=%08x fad_end=%08x offset=%08x %d\n",
					i, cdb.tracks[i].fad_0, cdb.tracks[i].fad_start, cdb.tracks[i].fad_end,
					cdb.tracks[i].file_offset, cdb.tracks[i].sector_size);
	}

	last = &cdb.tracks[cdb.track_num-1];
	cdb.TOC[99]  = bswap32((TRACK_CTRL(&cdb.tracks[0])<<24) | 0x010000);
	cdb.TOC[100] = bswap32((TRACK_CTRL(last)<<24) | (cdb.track_num<<16));
	cdb.TOC[101] = bswap32((TRACK_CTRL(last)<<24) | (last->fad_end+1));
	
	cdb.status = STAT_PAUSE;
	set_status(cdb.status);

	sector_delay = 0;
}


/* Upper bound of the parser's strictly increasing INDEX02..99 table. */
u8 track_subindex(const TRACK_INFO *tk, u32 fad)
{
    if (fad < tk->fad_start) return 0;
    unsigned low = 1, high = tk->max_index;
    if (tk->index_base) while (low < high) {
        unsigned mid = (low + high) / 2;
        if (fad >= TRACK_INDEX_VALUE(tk, mid-1)) low = mid + 1;
        else high = mid;
    }
    return low;
}

u32 track_to_fad(u16 track_index)
{
    if (!track_index) return 0;
    TRACK_INFO *tk = get_track_info(track_index);
    if (!tk) return 0xffffffffu;
    unsigned index = track_index & 255;
    if (index <= 1) return tk->fad_start;
    if (index > (unsigned)tk->max_index) return tk->fad_end;
    if (!tk->index_base) return 0xffffffffu;
    return TRACK_INDEX_VALUE(tk, index-2);
}

int fad_to_track(u32 fad)
{
	int i;

	for(i=0; i<cdb.track_num; i++){
		if(fad>=cdb.tracks[i].fad_0 && fad<=cdb.tracks[i].fad_end)
			return i+1;
	}

	return 0xff;
}

TRACK_INFO *get_track_info(int track_index)
{
    int t = track_index == 0xffff ? cdb.track_num-1 : (track_index>>8)-1;
    if (!cdb.tracks || cdb.track_num <= 0 || cdb.track_num > MAX_TRACKS ||
        t < 0 || t >= cdb.track_num) return NULL;
    return &cdb.tracks[t];
}


/******************************************************************************/

// 内部扇区缓存, 32K大小, 大概16个2k的扇区. 位于SDRAM中
// FATFS层以512字节为单位读取, 每次读64个单位.
static u8 *sector_buffer = (u8*)0x24002000;

// 内部缓存中保存的FAD扇区范围
static u32 buf_fad_start = 0;
static u32 buf_fad_end = 0;
// 由于存在2352这样的扇区格式, FAD扇区不是2048字节对齐的.
// FATFS层以512为单位读取数据. FAD扇区相对于sector_buffer有一个偏移.
static u16 buf_fad_offset = 0;
// 内部缓存中的扇区大小
static u16 buf_fad_size = 0;
static u8 buf_subcode_mode = 0;


static u8 reorder[24] = {
	0x00, 0x42, 0x7d, 0xbf, 0x64, 0x32, 0x96, 0xaf,
	0x08, 0x21, 0x3a, 0x53, 0x6c, 0x85, 0x9e, 0xb7,
	0x10, 0x29, 0x19, 0x5b, 0x74, 0x8d, 0xa6, 0x4b
};

void subcode_convert(void)
{
	u8 *dst = cdb.subcode_buf;
	u8 *sub = sector_buffer;
	u8 *tbuf = sector_buffer+2048;
	int p, i, j, k;

	p = 0;
	while(p<1536){
		memset(tbuf, 0, 96);
	//	for(i=0; i<8; i++){ // 带PQ通道
		for(i=2; i<8; i++){ // 无PQ通道
			for(j=0; j<12; j++){
				u8 d = sub[i*12+j];
				for(k=0; k<8; k++){
					u8 b = (d>>(7-k))&1;
					tbuf[j*8+k] |= b<<(7-i);
				}
			}
		}
		tbuf += 96;
		sub += 96;
		p += 96;
	}

	sub = sector_buffer+2048;
	p = 0;
	while(p<(96*14)){
		for(j=0; j<24; j++){
			dst[j] = sub[reorder[j]];
		}
		p += 24;
		dst += 24;
		sub += 24;
	}
}


/* Shared locked transfer checks the actual seek position before reading. */
static int read_disc_bytes(FIL *fp, u32 position, u32 bytes)
{
    u32 nread;
    fs_lock();
    int retv = f_lseek(fp, position);
    if (!retv && f_tell(fp) != position) retv = FR_DISK_ERR;
    if (!retv) retv = f_read(fp, sector_buffer, bytes, &nread);
    fs_unlock();
    return retv ? -1 : (int)nread;
}

int get_sector(int fad, BLOCK *wblk)
{
    int nread;
    if (cdb.play_track == NULL || (u32)fad < cdb.play_track->fad_0 ||
        (u32)fad > cdb.play_track->fad_end) {
        cdb.track = fad_to_track(fad);
        SSLOG(_DTASK, "Change to track %d\n", cdb.track);
        cdb.play_track = get_track_info(cdb.track << 8);
        if (!cdb.play_track) {
            SSLOG(_DTASK, "play_track not found!\n");
            cdb.status = STAT_ERROR;
            return -1;
        }
        int key = disable_irq();
        cdb.subrw_wp = cdb.subrw_rp;
        restore_irq(key);
    }
    TRACK_INFO *tk = cdb.play_track;
    cdb.ctrladdr = TRACK_CTRL(tk);
    cdb.index = track_subindex(tk, fad);
    if (!wblk) return 0;
    /* Complete one operation with a single mode across all reader stages.
     * Initialization waits for this reader before changing its policy. */
    u8 subcode_mode = (cdb.iflag & 2) ? cdb.has_subrw : 0;
    if ((u32)fad >= buf_fad_start && (u32)fad < buf_fad_end &&
        buf_subcode_mode == subcode_mode) {
        //SSLOG(_DTASK, " fad_%08x found at buffer!\n", fad);
    } else {
        //SSLOG(_DTASK, " fad_%08x not found. need read from file.\n", fad);
        const u32 size = TRACK_SIZE(tk);
        u32 first = tk->fad_0 + tk->pregap;
        u32 end = tk->fad_end - tk->postgap + 1;
        int blank = (u32)fad < first || (u32)fad >= end;
        u32 offset = 0, position = 0, bytes = 0, frames;
        if (blank) {
            frames = ((u32)fad < first ? first : tk->fad_end + 1) - fad;
            if (frames > 16) frames = 16;
        } else {
            position = tk->file_offset;
            if ((u32)fad < tk->fad_start) position -= (tk->fad_start - fad) * size;
            else position += ((u32)fad - tk->fad_start) * size;
            offset = position & 511;
            position &= ~511u;
            bytes = (end - fad) * size + offset;
            if (bytes > 0x8000) bytes = 0x8000;
            frames = (bytes - offset) / size;
        }
        /* Sixteen planar input frames yield fourteen deinterleaved frames. */
        if (subcode_mode == 1 && frames > 14) {
            frames = 14;
            if (!blank) bytes = offset + frames*size;
        }
        if (subcode_mode) {
            if (fad < 150) goto read_error;
            u32 sub_position = ((u32)fad - 150) * 96;
            u32 sub_bytes = (frames + (subcode_mode == 1 ? 2 : 0))*96;
            nread = read_disc_bytes(&cdb.subcode_fp, sub_position, sub_bytes);
            if (nread < (int)(frames*96) || nread % 96) goto read_error;
            memset(sector_buffer + nread, 0, 1536 - nread);
            if (subcode_mode == 1) subcode_convert();
            else memcpy(cdb.subcode_buf, sector_buffer, 1536);
        }
        if (blank) {
            memset(sector_buffer, 0, 2352);
        } else {
            nread = read_disc_bytes(tk->fp, position, bytes);
            if (nread < (int)(offset + size)) goto read_error;
            u32 available = (nread - offset) / size;
            if (frames > available) frames = available;
        }
        buf_subcode_mode = subcode_mode;
        buf_fad_start = fad;
        buf_fad_end = fad + frames;
        buf_fad_offset = offset;
        /* Zero stride reuses one silent/zero-payload frame across a gap. */
        buf_fad_size = blank ? 0 : size;
    }
    wblk->data = sector_buffer + buf_fad_offset + (fad - buf_fad_start)*buf_fad_size;
	wblk->size = buf_fad_size ? buf_fad_size : tk->mode == 3 ? 2352 : 2048;
	wblk->fad = fad;
    const u8 *subheader = NULL;
    if (tk->mode != 3) {
        if (wblk->size == 2352 && wblk->data[15] == 2) subheader = wblk->data + 16;
        else if (wblk->size == 2336) subheader = wblk->data;
    }
    if (subheader) {
        wblk->fn = subheader[0]; wblk->cn = subheader[1];
        wblk->sm = subheader[2]; wblk->ci = subheader[3];
    } else {
        wblk->fn = wblk->cn = wblk->ci = 0;
        wblk->sm = wblk->size == 2324 ? 0x20 : 0;
    }

    return 0;
read_error:
    SSLOG(_DTASK, "f_read error!\n");
    cdb.status = STAT_ERROR;
    buf_fad_end = 0;
    return -2;
}

/* Only the disk task publishes. The higher-priority command task
 * consumes a slot and copies it to the FIFO without yielding. */
static void publish_subrw(u32 fad)
{
    if (!buf_subcode_mode) return;
    const u8 *pack = cdb.subcode_buf + (fad - buf_fad_start)*96;
    for (int i = 0; i < 4; i++, pack += 24) {
        u8 slot = cdb.subrw_wp;
        u8 next = (slot + 1) % 64;
        /* The spare slot stays invisible until both cursors are committed. */
        memcpy(cdb.subrw_buf + slot*24, pack, 24);
        int key = disable_irq();
        if (next == cdb.subrw_rp) cdb.subrw_rp = (next + 1) % 64;
        cdb.subrw_wp = next;
        restore_irq(key);
    }
    /* The host can clear the earlier Q notification before R-W is ready. */
    HIRQ = HIRQ_SCDQ;
}


/******************************************************************************/


void show_pt(int id)
{
	PARTITION *pt;
	BLOCK *bk;
	int i;

	pt = &cdb.part[id];
	SSLOG(_INFO, "Part_%d: nblks=%d\n", id, pt->numblocks);
	bk = pt->head;
	for(i=0; i<pt->numblocks; i++){
		SSLOG(_INFO, "  %2d: size=%4d fad=%08x data=%08x\n", i, bk->size, bk->fad, bk->data);
		bk = bk->next;
	}
}


void set_peri_report(void)
{
	int irqs;

	irqs = disable_irq();

	if((ST_STAT&0x1000)==0){
		set_report(cdb.status|STAT_PERI);
	}

	restore_irq(irqs);
}


__attribute__((noinline)) int outside_play_range(void)
{
    return cdb.fad < cdb.play_fad_start || cdb.fad >= cdb.play_fad_end;
}

static int acknowledge_drive_request(void)
{
    /* A timed-out request can be replaced by command service. Decide against
     * the current request and retire it before a new one can be published. */
    int irqs = disable_irq();
    int keep_reader = cdb.pause_request == -PLAYTYPE_SECTOR &&
                      cdb.play_type == PLAYTYPE_SECTOR;
    if (!keep_reader) {
        cdb.status = STAT_PAUSE;
        if (cdb.play_type == PLAYTYPE_DIR) invalidate_directory();
        cdb.play_type = 0;
    }
    cdb.pause_request = 0;
    restore_irq(irqs);
    if (!keep_reader) SSLOG(_DTASK, "play_task: Recv PAUSE request!\n");
    set_pause_ok();
    return keep_reader;
}

static __attribute__((noinline)) void log_play_end(void)
{
    SSLOG(_DTASK, "\nplay end! fad=%08x end=%08x repcnt=%d maxrep=%d play_type=%d\n",
          cdb.fad, cdb.play_fad_end, cdb.repcnt, cdb.max_repeat, cdb.play_type);
}

void disk_task(void *arg)
{
	int retv;
	BLOCK wblk;

	cdb.status = STAT_NODISC;
	set_status(cdb.status);

	int wait_ticks = 10;
	int play_wait_count = 0;
	int retained_startup;
	while(1){
_restart_wait:
		wait_ticks = (cdb.play_wait || cdb.block_free!=MAX_BLOCKS)? 1: 2;
		retv = osSemaphoreAcquire(sem_wait_disc, wait_ticks);
		if(retv==osErrorTimeout){
			if(cdb.pause_request){
				goto _restart_nowait;
			}
			if(cdb.play_wait){
				play_wait_count += 1;
				if(play_wait_count==10){
					play_wait_count = 0;
					if(cdb.block_free){
						cdb.play_wait = 0;
						goto _restart_nowait;
					}
				}
			}
			if(next_disc>=0){
				int sd_card_insert(void);
				if(cdb.status==STAT_OPEN){
					if(sd_card_insert()){
                        int requested = next_disc;
                        fs_lock();
                        retv = disk_reidentify();
                        fs_unlock();
                        if (!retv) retv = load_disc(requested, 0);
                        if (retv) {
                            /* Wait for another removal/insertion, retaining
                             * the requested disc without a tight retry loop. */
                            unload_disc();
                            next_disc = requested;
                        }
						HIRQ = HIRQ_DCHG;
					}
				}else{
					if(sd_card_insert()==0){
						cdb.status = STAT_OPEN;
					}
				}
			}
			HIRQ = HIRQ_SCDQ;
			set_peri_report();

			goto _restart_wait;
		}

_restart_nowait:
		retained_startup = 0;
		if(cdb.pause_request){
			if (!acknowledge_drive_request()) goto _restart_wait;
			retained_startup = 1;
		}
		
        /* A queued wake can outlive its reader. Keep the settled state. */
        if (!cdb.play_type) goto _idle_wait;
        if (cdb.play_type == PLAYTYPE_SECTOR && outside_play_range()) goto _play_end;
        if (get_sector(cdb.fad, NULL)) goto _read_error;

		if(cdb.play_fad_start==0){
			cdb.status = STAT_PAUSE;
			goto _idle_wait;
		}

		if(cdb.play_type!=PLAYTYPE_FILE && cdb.play_type!=PLAYTYPE_DIR && cdb.play_track->mode==3){
		}else{
			SSLOG(_DTASK, "\nplay_task! fad_start=%08x(lba_%d) fad_end=%08x fad=%08x type=%d free=%d\n",
					cdb.play_fad_start, cdb.play_fad_start-150, cdb.play_fad_end, cdb.fad, cdb.play_type, cdb.block_free);
		}

		if(cdb.fad==0){
			cdb.fad = cdb.play_fad_start;
		}
		int delayed_start = 0;
		if(!retained_startup && cdb.play_type!=PLAYTYPE_FILE && cdb.play_fad_start == cdb.fad){
			if(play_delay){
				int calc_delay = play_delay;
				cdb.status = STAT_BUSY;
				if (hw_delay(10000)) goto _delay_error;
				if (cdb.pause_request) goto _restart_nowait;
				cdb.status = STAT_SEEK;
				if (hw_delay(calc_delay)) goto _delay_error;
				if (cdb.pause_request) goto _restart_nowait;
				delayed_start = 1;
			}
		}

		int old_status = cdb.status;
		if (!delayed_start) cdb.status = STAT_PLAY;
		if(old_status!=cdb.status){
			if(cdb.play_track->mode==3){
				cdb.options = 0x00;
			}
		}

		while(cdb.fad<cdb.play_fad_end){
_play_checkpoint:
			if(cdb.pause_request){
				if (!acknowledge_drive_request()) goto _restart_wait;
				if (outside_play_range()) goto _play_end;
			}

            retv = get_sector(cdb.fad, &wblk);
            if (retv) goto _read_error;
			led_event(LEDEV_CSCT);
			HIRQ = HIRQ_SCDQ;
			set_peri_report();

			if(cdb.play_type!=PLAYTYPE_FILE && cdb.play_type!=PLAYTYPE_DIR && cdb.play_track->mode==3){
				int asize = fill_audio_buffer(wblk.data);
				if(asize<0){
					cdb.play_wait = 1;
					goto _restart_wait;
				}
                publish_subrw(wblk.fad);
                if (cdb.fad+1 == cdb.play_fad_end && cdb.repcnt >= cdb.max_repeat)
                    spi2_flush();
				if (hw_delay(asize <= 4 ? 10000 : 13500)) goto _delay_error;
			}else
			{
				//printk("filter sector %08x...\n", cdb.fad);
				if(cdb.play_type==PLAYTYPE_SECTOR && sector_delay){
					if (hw_delay(sector_delay)) goto _delay_error;
				}
				retv = filter_sector(cdb.play_track, &wblk);
				HIRQ = HIRQ_CSCT;

				if(retv==2){
					SSLOG(_DTASK, "buffer full! wait ...\n");
					// block缓存已满, 需要等待某个事件再继续
					/* A buffer-full pause leaves file completion pending. */
					HIRQ = HIRQ_BFUL;
					cdb.status = STAT_PAUSE;
					cdb.play_wait = 1;
					goto _restart_wait;
				}
                /* A deliberate data discard advances playback; a failed
                 * directory delivery retires without accepting the sector. */
                if (retv == 0 || cdb.play_type != PLAYTYPE_DIR)
                    publish_subrw(wblk.fad);
                if (cdb.play_type == PLAYTYPE_DIR) {
                    if (retv != 0) {
                        finish_read(0);
                        goto _restart_wait;
                    }
                    int next = handle_diread();
                    if (next == 2) goto _restart_nowait;
                    if (!next) goto _restart_wait;
                }
			}

			cdb.fad++;
            if (cdb.play_type == PLAYTYPE_FILE) advance_file_unit();
		}

_play_end:
		log_play_end();

        /* A pending range update may extend the old endpoint. */
        if (cdb.pause_request) goto _play_checkpoint;
        if (cdb.play_type && outside_play_range()) {
			{
				if(cdb.repcnt>=cdb.max_repeat || cdb.play_fad_start>=cdb.play_fad_end){
					if(pend_delay){
						if (hw_delay(pend_delay)) goto _delay_error;
						if (cdb.pause_request) goto _restart_nowait;
						cdb.status = STAT_BUSY;
						if (hw_delay(pend_delay)) goto _delay_error;
						if (cdb.pause_request) goto _restart_nowait;
					}
					cdb.play_track = NULL;
					cdb.status = STAT_PAUSE;
					if(cdb.play_type==PLAYTYPE_FILE){
						HIRQ = HIRQ_EFLS;
					}
					HIRQ = HIRQ_PEND;
					cdb.index = 0;
                    /* The command task may preempt as soon as it sees idle. */
                    __asm__ volatile ("" ::: "memory");
					cdb.play_type = 0;
				}else{
					cdb.play_track = NULL;
					if(cdb.repcnt<14)
						cdb.repcnt += 1;
					cdb.fad = cdb.play_fad_start;
					cdb.track = fad_to_track(cdb.fad);
					goto _restart_nowait;
				}
			}
		}

		set_peri_report();
		goto _restart_wait;

_idle_wait:
        SSLOG(_DTASK, "play_task: play_type=%d! play_fad_start=%08x!\n", cdb.play_type, cdb.play_fad_start);
        goto _restart_wait;

_read_error:
        log_play_end();
        set_peri_report();
        finish_read(0);
        goto _restart_wait;

_delay_error:
        cdb.play_wait = 0;
        if (cdb.play_type == PLAYTYPE_DIR) {
            finish_read(0);
        } else {
            int irqs = disable_irq();
            cdb.status = STAT_ERROR;
            cdb.play_type = 0;
            cdb.play_track = NULL;
            restore_irq(irqs);
        }
        set_peri_report();
	}
}

static void disk_task_notify(void)
{
	if(cdb.play_wait){
		cdb.play_wait = 0;
		disk_task_wakeup();
	}
}


void disk_task_wakeup(void)
{
	osSemaphoreRelease(sem_wait_disc);
}


void set_pause_ok(void)
{
	osSemaphoreRelease(sem_wait_pause);
}

void wait_pause_ok(void)
{
	osSemaphoreAcquire(sem_wait_pause, 2);
}

/* Explicit callers wait before replacing readers. Automatic changes run on
 * the producer itself and must not wait for their own acknowledgment. */
int synchronize_drive(int keep_sector)
{
    if (cdb.play_type || cdb.pause_request) {
        cdb.pause_request = keep_sector ? -PLAYTYPE_SECTOR : 1;
        cdb.play_wait = 0;
        disk_task_wakeup();
        for (int retries = 500; cdb.pause_request && retries; retries--)
            wait_pause_ok();
        if (cdb.pause_request) return -1;
    }
    return 0;
}

int pause_drive(void)
{
    return synchronize_drive(0);
}

/* Replacing a reader invalidates its cache only after successful handoff. */
int pause_disc(void)
{
    if (pause_drive()) return -1;
    /* An already-idle reader may still own cached sectors. */
    buf_fad_start = buf_fad_end = 0;
    return 0;
}

void cdc_delay(int ticks)
{
	osDelay(ticks);
}

void cdc_dump(int status)
{
	printk("CDB:\n");
	printk("  status     : %02x\n", cdb.status);
	printk("  block_free : %d\n",   cdb.block_free);
	printk("  fad        : %08x\n", cdb.fad);
	
	if(status){
		cdb.status = status;
	}
}

/******************************************************************************/


/* Named and primary-archive transfers share the mapped buffer bounds.
 * Keep the named file object out of the persistent CD-task frame. */
static __attribute__((noinline)) int ss_file_transfer(int mode)
{
    FIL fp;
    int writing = mode & 1;
    int archive = mode & 2;
    u32 request_offset = *(u32 *)(TMPBUFF_ADDR + 0x00);
    u32 size = *(u32 *)(TMPBUFF_ADDR + 0x04);
    u32 baddr = *(u32 *)(TMPBUFF_ADDR + 0x08);
    char *name = (char *)(TMPBUFF_ADDR + 0x10);
    u8 *buffer = (u8 *)(uintptr_t)((baddr & 0x00ffffffu) | 0x02000000u);
    /* FPGA 05 exposes 24 SDRAM address bits. Never cross the window or
       the live request/name area; the legacy bounce payload starts at +0x100. */
    u32 position = baddr & 0x00ffffff;
    const u32 request = TMPBUFF_ADDR - FIRM_ADDR;
    u32 capacity = (position < request ? request : 0x01000000) - position;
    *(u32 *)(TMPBUFF_ADDR + 0x04) = 0;
    if ((!archive || size) &&
        ((position >= request && position < request + 0x100) || size > capacity))
        return FR_INVALID_PARAMETER;
    if (archive) {
        if (size > 65536 || (!size && !writing)) return FR_INVALID_PARAMETER;
        int result = save_archive_io(*(u32 *)(TMPBUFF_ADDR + 0x0c), request_offset,
                                     size ? buffer : NULL, writing ? -(int)size : (int)size);
        if (!result) *(u32 *)(TMPBUFF_ADDR + 0x04) = size;
        return result;
    }
    if (!memchr(name, 0, 0xf0)) return FR_INVALID_PARAMETER;
    int offset = (int)request_offset;
    int flags = writing ? ((offset == -1 ? FA_CREATE_ALWAYS : FA_OPEN_ALWAYS) | FA_WRITE) : FA_READ;
    fs_lock();
    int retv = f_open(&fp, name, flags);
    if (retv) {
        fs_unlock();
        return retv;
    }
    u32 count = 0;
    if (writing && offset < 0) offset = 0;
    retv = f_lseek(&fp, offset);
    /* FatFs may report a successful short seek when extension runs out of space. */
    if (!retv && writing && f_tell(&fp) != (FSIZE_t)offset) retv = FR_INT_ERR;
    if (!retv) {
        if (!writing && size == 0) {
            /* Check the remaining extent at full exFAT width before narrowing. */
            FSIZE_t remaining = f_size(&fp) - f_tell(&fp);
            if (remaining > capacity) retv = FR_INVALID_PARAMETER;
            else size = (u32)remaining;
        }
        if (!retv)
            retv = file_buffer_io(&fp, buffer, size, writing, &count);
    }
    int closed = f_close(&fp);
    fs_unlock();
    if (!retv) retv = closed;
    SSLOG(_INFO, writing ? "\nSSCMD_FILEWR: retv=%d wsize=%08x  %s\n" :
                          "\nSSCMD_FILERD: retv=%d rsize=%08x  %s\n", retv, count, name);
    *(u32 *)(TMPBUFF_ADDR + 0x04) = count;
    return retv;
}

/* Save replies carry a FatFs result below the negotiated tag bit. */
_Static_assert(FR_INVALID_PARAMETER < SSCMD_SAVE_ACK, "save result overlaps acknowledgment");
_Static_assert(SSCMD_SAVE_ACK > 0 && SSCMD_SAVE_ACK < 0x8000 &&
               !(SSCMD_SAVE_ACK & (SSCMD_SAVE_ACK - 1)),
               "file/save acknowledgment must be one positive transported bit");
_Static_assert(SSCMD_SELECT < SSCMD_SAVE_ACK && SSCMD_SMEMS < SSCMD_SAVE_ACK &&
               SSCMD_FILEWR == SSCMD_FILERD + 1 && SSCMD_LISTBIN == SSCMD_FILERD + 2 &&
               SSCMD_SSAVE == SSCMD_FILERD + 3 &&
               SSCMD_LSAVE == SSCMD_SSAVE + 1 && SSCMD_LMEMS == SSCMD_SSAVE + 2 &&
               SSCMD_SMEMS == SSCMD_SSAVE + 3,
               "file/save aliases must remain separate from existing commands");
_Static_assert(SSCMD_ARCHIVEWR == SSCMD_ARCHIVERD + 1 &&
               (SSCMD_ARCHIVERD & SSCMD_SAVE_ACK) != 0 && (SSCMD_ARCHIVEWR & SSCMD_SAVE_ACK) != 0 &&
               (SSCMD_ARCHIVERD ^ SSCMD_SAVE_ACK) > SSCMD_SELECT && SSCMD_ARCHIVEWR < 0x8000,
               "archive commands overlap existing aliases or lose the acknowledgment bit");
_Static_assert(SSCMD_PRINTF == 1 && SSCMD_LISTDISC == 2 && SSCMD_LOADDISC == 3 &&
               SSCMD_CHECK == 4 && SSCMD_UPDATE == 5 && SSCMD_STARTUP == 13 && SSCMD_SELECT == 14,
               "menu opcode assignments changed");
_Static_assert((SSCMD_BINDIR & SSCMD_SAVE_ACK) &&
               (SSCMD_BINDIR ^ SSCMD_SAVE_ACK) > SSCMD_SELECT &&
               SSCMD_BINDIR > SSCMD_ARCHIVEWR && SSCMD_BINDIR < 0x8000,
               "BIN directory command collides with an existing operation");

void ss_cmd_handle(void)
{
	int retv;
	u32 cmd = SS_CMD;

	//SSLOG(_INFO, "scmd_task: %04x\n", SS_CMD);

    /* Menu v1 uses the existing tag for all fourteen legacy operations. */
    u32 save_cmd = cmd & ~SSCMD_SAVE_ACK;
    if (save_cmd - SSCMD_PRINTF > SSCMD_SELECT - SSCMD_PRINTF)
        save_cmd = cmd;
    switch(save_cmd){
	case SSCMD_PRINTF:
		// 输出信息
		printk("%s", (char*)(TMPBUFF_ADDR+0x10));
        if (cmd & SSCMD_SAVE_ACK) SS_ARG = SSCMD_SAVE_ACK;
		SS_CMD = 0;
		break;
	case SSCMD_BINDIR:
        retv = list_bin_dir((const char *)(TMPBUFF_ADDR + 0x10));
        goto save_reply;
	case SSCMD_LISTBIN:
		// 列出bin信息
		retv = list_bins(1);
        goto save_reply;
	case SSCMD_LISTDISC:
		// 列出镜像信息
		retv = list_disc(SS_ARG, 0);
        goto save_reply;
	case SSCMD_LOADDISC:
		// 装载镜像
		/* Stop the GIF reader before disc parsing reuses its workspace. */
		gif_decode_exit();
		if (SS_ARG == 0xffff) {
            retv = pause_disc();
            if (!retv) retv = unload_disc();
        } else retv = load_disc(SS_ARG, 1);
        goto save_reply;
	case SSCMD_CHECK:
	{
		// 检查是否有升级固件
		int fpga = (fpga_update(1)==0)? 1: 0;
		int firm = (flash_update(1)==0)? 1: 0;
        retv = (fpga<<1) | firm;
        goto save_reply;
	}
	case SSCMD_UPDATE:
	{
		// 固件升级
		int fpga = (fpga_update(0)>=-1)? 0: 1;
		int firm = (flash_update(0)>=-1)? 0: 1;
        retv = (fpga<<1) | firm;
        goto save_reply;
	}
	case SSCMD_FILERD:
	case SSCMD_FILEWR:
        /* Negative FatFs errors already have the tag bit set. */
        retv = -ss_file_transfer(save_cmd == SSCMD_FILEWR);
        goto save_reply;
    case SSCMD_ARCHIVERD:
    case SSCMD_ARCHIVEWR:
        retv = -ss_file_transfer(2 | (save_cmd == SSCMD_ARCHIVEWR));
        goto save_reply;
	case SSCMD_SSAVE:
        retv = flush_savefile();
        goto save_reply;
	case SSCMD_LSAVE:
        retv = load_savefile((char *)(TMPBUFF_ADDR + 0x10));
        goto save_reply;
	case SSCMD_SMEMS:
        retv = flush_smems(SS_ARG);
        goto save_reply;
	case SSCMD_LMEMS:
        retv = load_smems(SS_ARG);
    save_reply:
        SS_ARG = retv | (cmd & SSCMD_SAVE_ACK);
        SS_CMD = 0;
        break;
	case SSCMD_STARTUP: {
		// 土星端启动!

		// 复位buffer
		buf_fad_start = 0;
		buf_fad_end = 0;
		buf_fad_size = 0;
		// 镜像列表
        int listed = list_disc(0, 0);

		// 封面数据
		cover_init();

		// 背景音乐
		int audio_repeat = 0;
		retv = load_pcm("/SAROO/bgsound.pcm");
		if(retv){
			retv = load_pcm("/SAROO/bgsound_r.pcm");
			if(retv==0){
				audio_repeat = 1;
			}
		}
        if (!(cmd & SSCMD_SAVE_ACK)) SS_ARG = retv;
		if(retv==0){
            cdb.play_pos_types = 0;
			cdb.play_fad_start = track_to_fad(0x0100);
			cdb.play_fad_end   = track_to_fad(0x0163)+1;
			cdb.track = 0x01;
			cdb.index = 0x00;
			cdb.fad = cdb.play_fad_start;
			cdb.max_repeat = (audio_repeat)? 0x0f : 0x00;
			cdb.repcnt = 0;
			cdb.options = 0x00;
			cdb.pause_request = 0;
			cdb.play_type = PLAYTYPE_SECTOR;
			disk_task_wakeup();
		}

		gif_decode_init();
        /* Tagged startup reports the catalog result after all services retire. */
        if (cmd & SSCMD_SAVE_ACK) { retv = listed; goto save_reply; }
		break;
	}
	case SSCMD_SELECT: {
		int index = SS_ARG;
		printk("\nSelect %04x\n", index);
		
		if(index&0x8000){
			gif_decode_exit();
		}

		if(index&0x4000){
			gif_decode_init();
		}else{
			load_cover(index);
		}
        /* The preview request has retired, even if artwork was unavailable. */
        if (cmd & SSCMD_SAVE_ACK) { retv = 0; goto save_reply; }
		break;
	}
	default:
		SSLOG(_INFO, "[SS] unkonw cmd: %04x\n", cmd);
		break;
	}

}


/******************************************************************************/


void sirq_task(void *arg)
{
	u32 stat;

	while(1){
		stat= ST_STAT;
		ST_STAT = stat;
		//printk("  stat=%04x CR1=%04x\n", stat, SSCR1);
		if((stat&0x0007)==0){
			ST_CTRL |= 7;
			osSemaphoreAcquire(sem_wait_irq, osWaitForever);
			continue;
		}

		if(stat&0x0004){
			// 数据传输相关
			trans_handle();
		}

		if(stat&0x0001){
			cdb.cr1 = SSCR1;
			cdb.cr2 = SSCR2;
			cdb.cr3 = SSCR3;
			cdb.cr4 = SSCR4;
			cdc_cmd_process();
		}

		if(stat&0x0002){
			ss_cmd_handle();
		}
		
	}

}

void EXTI1_IRQHandler(void)
{
	EXTI->PR1 = 0x0002;
	ST_CTRL &= ~7;
	//printk("\nST_IRQ!\n");

	osSemaphoreRelease(sem_wait_irq);
}


/******************************************************************************/

static int cd_sync_init(void)
{
    osSemaphoreId_t sems[3];
    osMutexId_t mutex;
    unsigned count = 0;
    while (count < sizeof sems / sizeof *sems) {
        sems[count] = osSemaphoreNew(1, 0, NULL);
        if (!sems[count]) goto failed;
        count++;
    }
    mutex = osMutexNew(NULL);
    if (!mutex) goto failed;

    sem_wait_irq = sems[0];
    sem_wait_disc = sems[1];
    sem_wait_pause = sems[2];
    mutex_fs = mutex;
    return 0;

failed:
    while (count) osSemaphoreDelete(sems[--count]);
    return -1;
}

void TIM2_IRQHandler(void)
{
    if (!(TIM2->SR & TIM_SR_UIF)) return;
    TIM2->SR = 0;
    disk_task_wakeup();
}

static int hw_delay(int us)
{
    if (cdb.pause_request) return 0;
    unsigned ticks = us > 0 ? ((unsigned)us + 9u) / 10u : 2u;
    /* URS stays set. UG loads PSC and clears the counter without an IRQ. */
    TIM2->ARR = ticks;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_OPM | TIM_CR1_URS | TIM_CR1_CEN;

    /* Allow for the inclusive period, tick phase and an extra scheduler tick. */
    uint32_t deadline = osKernelGetTickCount() + (uint64_t)ticks * OS_TICK_FREQ / 100000u + 3u;
    for (;;) {
        int32_t remaining = (int32_t)(deadline - osKernelGetTickCount());
        int result = osSemaphoreAcquire(sem_wait_disc, remaining > 0 ? remaining : 0);
        if (cdb.pause_request) {
            TIM2->CR1 = TIM_CR1_OPM | TIM_CR1_URS;
            return 0;
        }
        if (result != osOK) break;
        /* A worker notification only completes the delay once TIM2 stops. */
        if (!(TIM2->CR1 & TIM_CR1_CEN)) return 0;
        if (remaining <= 0) break;
    }

    /* Retire a failed period without consuming a pending worker notification. */
    int irqs = disable_irq();
    TIM2->CR1 = TIM_CR1_OPM | TIM_CR1_URS;
    TIM2->SR = 0;
    __DSB();
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    restore_irq(irqs);
    return -1;
}

static void cd_timer_init(void)
{
    /* BOOT enables TIM2. Reset the active and retired CD timers only. */
    unsigned mask = RCC_APB1LRSTR_TIM2RST | RCC_APB1LRSTR_TIM6RST;
    unsigned reset = RCC->APB1LRSTR & ~mask;
    RCC->APB1LRSTR = reset | mask;
    RCC->APB1LRSTR = reset;

    TIM2->CR1 = TIM_CR1_OPM | TIM_CR1_URS;
    TIM2->PSC = 2000 - 1; /* 200 MHz to 100 kHz */
    TIM2->DIER = TIM_DIER_UIE;
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
}


void ss_hw_init(void)
{
	// Release FPGA_NRESET
	//GPIOC->BSRR = (1<<5);

	// EXTI1 -> PB1
	SYSCFG->EXTICR[0] &= 0xffffff0f;
	SYSCFG->EXTICR[0] |= 0x00000010;

	EXTI->RTSR1 |= 0x00000002;
	EXTI->FTSR1 &= 0xfffffffd;
	EXTI->IMR1 |= 0x00000002;
	EXTI->PR1   = 0x00000002;

	HIRQ = 0x0be1;
	SSCR1 = ( 0 <<8) | 'C';
	SSCR2 = ('D'<<8) | 'B';
	SSCR3 = ('L'<<8) | 'O';
	SSCR4 = ('C'<<8) | 'K';

	//NVIC_SetPriority(EXTI1_IRQn, 10);
	NVIC_EnableIRQ(EXTI1_IRQn);

	ST_CTRL = 0x0203;

    cd_timer_init();
}


void ss_sw_init(void)
{
	int i;

	memset(&cdb, 0, sizeof(cdb));

	cdb.status = STAT_PAUSE;
	cdb.fad = 150;
	cdb.ctrladdr = 0x41;
	cdb.track = 1;
	cdb.index = 1;

	cdb.sector_size = 2048;
	cdb.put_sector_size = 2048;
	cdb.block_free = MAX_BLOCKS;

	for(i=0; i<MAX_BLOCKS; i++){
		cdb.block[i].size = -1;
		cdb.block[i].data = (u8*)(0x2400a000+i*2352);
	}

	u8 *info_buf = (u8*)0x2400a000+MAX_BLOCKS*2352; // 0x72d80

	cdb.FINFO = (u8*)(info_buf+0x00000);
	cdb.TOC  = (u32*)(info_buf+0x00c00);
	cdb.SUBH  = (u8*)(info_buf+0x00da0);
    _Static_assert(0x0e00 + 100*sizeof(TRACK_INFO) + 2*1536 <=
                   0x80000 - (0x0a000 + MAX_BLOCKS*2352), "CD metadata exceeds AXI SRAM");
	cdb.tracks = (TRACK_INFO*)(info_buf+0x00e00); // 0x7db80
	memset(cdb.tracks, 0, 100*sizeof(TRACK_INFO));
	cdb.subcode_buf = (u8*)(info_buf+0x0e00+100*sizeof(TRACK_INFO));
	cdb.subrw_buf = cdb.subcode_buf+1536;

	for(i=0; i<MAX_SELECTORS; i++){
		cdb.filter[i].c_true = i;
		cdb.filter[i].c_false = 0xff;

		cdb.part[i].numblocks = 0;
		cdb.part[i].head = NULL;
		cdb.part[i].tail = NULL;
	}

	cdb.last_buffer = 0xff;
	cdb.cddev_filter = 0xff;
}


void fs_lock(void)
{
	osMutexAcquire(mutex_fs, osWaitForever);
}


void fs_unlock(void)
{
	osMutexRelease(mutex_fs);
}


void saturn_config(void)
{
	FIL fp;
	int retv;
	u32 rv;

	memset((u8*)SYSINFO_ADDR, 0, 0x1000);
	if (parse_config("/SAROO/saroocfg.txt", NULL)) return;

	if(auto_update){
		int fp, fl;

		fp = fpga_update(1);
		fl = flash_update(1);
		if(fp==-1 && fl==-1){
			// 没有升级文件
			led_event(LEDEV_NONE);
		}else{
			led_event(LEDEV_BUSY);
			if(fp==0){
				fpga_update(0);
			}
			if(fl==0){
				fl = flash_update(0);
				if(fl<-1){
					led_event(LEDEV_FILE_ERROR);
					return;
				}
			}
			led_event(LEDEV_OK);
			return;
		}
	}

	if(debug_flags&0xc0000000){
		led_event(LEDEV_BUSY);
		int cnt = 0;
		while(1){
			retv = mem_test(0x61000000, 0x00800000);
			if(retv){
				led_event(LEDEV_SDRAM_ERROR);
				//return;
			}
			if((debug_flags&0x40000000)==0)
				break;
			cnt += 1;
			printk("  times: %d\n", cnt);
		}
		led_event(LEDEV_NONE);
	}


	// 检查是否有bootrom. 如果有,就加载到FPGA中
	retv = f_open(&fp, "/SAROO/ssfirm.bin", FA_READ);
	if(retv){
		printk("NO bootrom file found!\n");
		led_event(LEDEV_NOFIRM);
		return;
	}
	/* Check the full exFAT size before narrowing it to a read count. */
	FSIZE_t extent = f_size(&fp);
	if (extent < 8 || extent > IMGINFO_ADDR - FIRM_ADDR) {
		f_close(&fp);
		led_event(LEDEV_FILE_ERROR);
		return;
	}
	u32 fsize = (u32)extent;
	printk("Found Saturn bootrom file.\n");
	printk("    Size %08x\n", fsize);

	rv = 0;
	retv = f_read(&fp, (void*)FIRM_ADDR, fsize, &rv);
	printk("    f_read: retv=%d rv=%08x\n", retv, rv);
	f_close(&fp);
	if (retv || rv != fsize) {
		led_event(LEDEV_FILE_ERROR);
		return;
	}


	// 打开存档文件
	retv = open_savefile();
	if (retv) { led_event(LEDEV_FILE_ERROR); return; }


	// 放置系统信息
	*(u32*)(SYSINFO_ADDR+0x00) = get_build_date(); // 编译日期
	*(u32*)(SYSINFO_ADDR+0x04) = lang_id;          // 菜单语言
	*(u32*)(SYSINFO_ADDR+0x08) = debug_flags;      // 调试选项
    *(u32*)(SYSINFO_ADDR+0x10) = SAVE_ACK_VERSION;
    *(u32*)(SYSINFO_ADDR+0x14) = FILE_ACK_VERSION;
    *(u32*)(SYSINFO_ADDR+0x18) = ARCHIVE_IO_VERSION;
    *(u32*)(SYSINFO_ADDR+0x2c) = MENU_ACK_VERSION;
    *(u32*)(SYSINFO_ADDR+0x34) = BIN_DIRECTORY_VERSION;

    if (cd_sync_init()) return;
    ss_sw_init();

	// I2S
	spi2_init();
	spi2_set_notify(disk_task_notify);

	ss_hw_init();

	osThreadAttr_t attr;
	memset(&attr, 0, sizeof(attr));

	attr.priority = osPriorityNormal+7;
	osThreadNew(sirq_task, NULL, &attr);

    /* Preserve the effective Normal default explicitly: partition work must
     * remain below command/FIFO service, including nonblocking borrowed reads. */
    attr.priority = osPriorityNormal;
    osThreadNew(disk_task, NULL, &attr);

}

/******************************************************************************/

