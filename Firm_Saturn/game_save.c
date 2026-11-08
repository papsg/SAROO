
#include "main.h"
#include "smpc.h"



/******************************************************************************/

#define BUP_WORK_ADDR 0x06000354
#define BUP_MEM_ADDR  (SAVINFO_ADDR)

#define MEMS_HEADER   (SAVINFO_ADDR+0x10000)
#define MEMS_TMP      (SAVINFO_ADDR+0x12000)
#define MEMS_BUFFER   (SAVINFO_ADDR+0x13000)

#define START_BLOCK   0x8000

#define	BUP_NON                 (1)
#define	BUP_UNFORMAT            (2)
#define	BUP_WRITE_PROTECT       (3)
#define	BUP_NOT_ENOUGH_MEMORY   (4)
#define	BUP_NOT_FOUND           (5)
#define	BUP_FOUND               (6)
#define	BUP_NO_MATCH            (7)
#define	BUP_BROKEN              (8)


typedef struct {
	u16 unit_id;
	u16 partition;
}BUPCFG;

typedef struct {
	u32 total_size;
	u32 total_block;
	u32 block_size;
	u32 free_size;
	u32 free_block;
	u32 data_num;
}BUPSTAT;

typedef struct {
	char file_name[12];
	char comment[11];
	char language;
	u32  date;
	u32  data_size;
	u16  block_size;
}BUPDIR;

typedef struct {
	u8 year;
	u8 month;
	u8 day;
	u8 time;
	u8 min;
	u8 week;
}BUPDATE;


static int bup_dev;

// 64k buffer at 220c4000, 64 blocks
static int mblk_start;
static int mblk_end;
static int mblk_bmp[2];
// header at 220c1000, 8192 bytes
// save_hdr at 220c3000, 1024 bytes
static int msav_blk;


/******************************************************************************/

typedef struct {
	u32 magic0;       // 00
	u32 magic1;
	u32 total_size;
	u16 block_size;
	u16 free_block;
	u8  unuse_10[16]; // 10
	u8  gameid[16];   // 20
	u8  unuse_30[14]; // 30
	u16 first_save;   // 3e
	u8  bitmap[64];   // 40
}BUPHEADER;

#define BUPMEM  ((volatile BUPHEADER*)(BUP_MEM_ADDR))


typedef struct {
	u32 magic0;
	u32 magic1;
	u32 total_size;
	u16 free_block;
	u16 first_save;
	u8  bitmap[1024-16];
}MEMSHEADER;

#define MEMS  ((volatile MEMSHEADER*)(MEMS_HEADER))


/******************************************************************************/

#define MEMS_HDR 1
#define MEMS_SAV 2
#define MEMS_BUF 4
#define MEMS_ALL 7
#define BUPSAVE  8

static void bup_update(int flag)
{
	int cmd = 0;
	int arg = 0;

	if(flag==BUPSAVE){
		cmd = SSCMD_SSAVE;
	}else{
		arg = flag;
		cmd = SSCMD_SMEMS;
	}

	SS_ARG = arg;
	SS_CMD = cmd;
	while(SS_CMD);
    /* A successful buffer flush commits these dirty bits. A different command
     * or a reported failure does not release this window for read-only reuse. */
    if ((flag & MEMS_BUF) && SS_ARG == 0) {
        mblk_bmp[0] = 0;
        mblk_bmp[1] = 0;
    }
}


static void mems_load(int blk)
{
	//printk("mems_load %04x\n", blk);
	SS_ARG = blk;
	SS_CMD = SSCMD_LMEMS;
	while(SS_CMD);
}


static void set_bitmap(u8 *bmp, int index, int val)
{
	int byte = index/8;
	int mask = 1<<(index&7);

	if(val)
		bmp[byte] |= mask;
	else
		bmp[byte] &= ~mask;
}


static int get_free_block(int pos)
{
	int i, byte, mask, bsize;
	u8 *bmp;
	u16 *free_block;

	if(bup_dev==0){
		bmp = (u8*)BUPMEM->bitmap;
		free_block = (u16*)&BUPMEM->free_block;
		bsize = 512;
	}else{
		bmp = (u8*)MEMS->bitmap;
		free_block = (u16*)&MEMS->free_block;
		bsize = 8064;
	}

	for(i=pos; i<bsize; i++){
		byte = i/8;
		mask = 1<<(i&7);
		if( (bmp[byte]&mask) == 0 ){
			bmp[byte] |= mask;
			*free_block -= 1;
			return i;
		}
	}

	return 0;
}


static u8 *get_block_addr(int id)
{
	int is_hdr = id&0x8000;
	id &= 0x7fff;

	if(bup_dev==0){
		return (u8*)(BUP_MEM_ADDR + id*128);
	}

	if(is_hdr){
		if(id!=msav_blk){
			mems_load(0x8000|id);
			msav_blk = id;
		}
		return (u8*)(MEMS_HEADER+0x2000);
	}

	if(id<mblk_start || id>mblk_end){
		if(mblk_bmp[0] || mblk_bmp[1]){
			bup_update(MEMS_BUF);
		}
		mems_load(id);
		mblk_start = id;
		mblk_end = id+63;
		mblk_bmp[0] = 0;
		mblk_bmp[1] = 0;
	}
	return (u8*)(MEMS_BUFFER+(id-mblk_start)*1024);
}


static int get_next_block(u8 *bmp, int pos)
{
	int byte, mask;

	if(bup_dev==1){
		return *(u16*)bmp;
	}

	while(pos<512){
		byte = pos/8;
		mask = 1<<(pos&7);
		if(bmp[byte]&mask){
			return pos;
		}
		pos += 1;
	}

	return 0;
}


static void mbuf_mark(int id)
{
	if(bup_dev==0)
		return;

	id -= mblk_start;
	if(id<32){
		mblk_bmp[0] |= 1u<<id;
	}else{
		mblk_bmp[1] |= 1u<<(id-32);
	}
}

#ifdef BUP_CHECK
static int chksum(u8 *data, int size)
{
	int sum = 0;

	while(size>0){
		sum += *(u16*)data;
		size -= 2;
		data += 2;
	}

	return sum;
}
#endif

static int access_data(int block, u8 *data, int type)
{
	u8 *bp, *bmp;
	int dsize, asize, bsize;

	bsize = (bup_dev==0) ? 128 : 1024;

	bp = get_block_addr(START_BLOCK|block);
	dsize = *(u32*)(bp+0x0c);
    if (!dsize)
        return 0;
    if (data == NULL) {
        return BUP_BROKEN;
    }

	if(bup_dev==1 && dsize<=960){
		if(type==0){ // read
			memcpy(data, bp+0x40, dsize);
		}else if(type==1){
			if(memcmp(data, bp+0x40, dsize)){
				return BUP_NO_MATCH;
			}
		}else{       // write
			memcpy(bp+0x40, data, dsize);
		}

		return 0;
	}

	bmp = bp+0x40;
	block = 0;
	while(dsize>0){
		block = get_next_block(bmp, block);
		bp = get_block_addr(block);

		asize = (dsize>bsize)? bsize : dsize;
		//printk("dsize=%04x block=%04x asize=%04x", dsize, block, asize);

		if(type==0){ // read
			memcpy(data, bp, asize);
#ifdef BUP_CHECK
			printk("  sum=%08x\n", chksum(data, asize));
#endif
		}else if(type==1){
			if(memcmp(data, bp, asize)){
				return BUP_NO_MATCH;
			}
		}else{       // write
#ifdef BUP_CHECK
			printk("  sum=%08x\n", chksum(data, asize));
#endif
			memcpy(bp, data, asize);
			mbuf_mark(block);
		}
		data += asize;
		dsize -= asize;
		block += 1;
		if(bup_dev==1)
			bmp += 2;
	}

	return 0;
}


static int find_save_kind(char *file_name, int offset, int *last, int prefix)
{
	int block, len, index;
	u8 *bp;

    if (file_name == NULL)
        return -1;
    for (len = 0; len < 11 && file_name[len]; len++);
    if (!prefix && !len) {
        return -1;
    }

	index = 0;

	if(bup_dev==0){
		block = BUPMEM->first_save;
		if(last) *last = 0;

		while(block){
			bp = get_block_addr(block);
			if(strncmp(file_name, (char*)bp, len)==0 && (prefix || len==11 || bp[len]==0)){
				if(index==offset){
					printk("  found at %04x\n", block);
					return block;
				}
				index += 1;
			}
			if(last)
				*last = block;
			block = *(u16*)(bp+0x3e);
		}
	}else{
		if(last) *last = -1;

		for(int i=0; i<448; i++){
			bp = (u8*)(MEMS_HEADER+1024+i*16);
			if(bp[0]==0 && last && *last==-1){
				*last = i;
			}
			if(bp[0] && strncmp(file_name, (char*)bp, len)==0 && (prefix || len==11 || bp[len]==0)){
				if(index==offset){
					block = *(u16*)(bp+0x0e);
					printk("  found at %04x\n", block);
					if(last)
						*last = i;
					return block;
				}
				index += 1;
			}
		}
	}

	printk("  not found!\n");
	if(last) printk("    last=%04x\n", *last);
	return -1;
}


static int find_save(char *name, int offset, int *last)
{
    return find_save_kind(name, offset, last, 1);
}

static int find_exact_save(char *name, int *last)
{
    return find_save_kind(name, 0, last, 0);
}

/* Validation and resizing run sequentially and share this scratch space.
 * No archive bytes change until all headers and block ownership are valid. */
static u8 resize_bitmap[1008];
static u16 resize_blocks[510];
static int checked_free, checked_saves;

static int claim_block(int block)
{
    int first = bup_dev ? 8 : 1;
    int total = bup_dev ? 8064 : 512;
    u8 *bitmap = bup_dev ? (u8 *)MEMS->bitmap : (u8 *)BUPMEM->bitmap;
    if (block < first || block >= total ||
        !(bitmap[block / 8] & (1u << (block % 8))) ||
        (resize_bitmap[block / 8] & (1u << (block % 8))))
        return BUP_BROKEN;
    resize_bitmap[block / 8] |= 1u << (block % 8);
    return 0;
}

static int validate_save_blocks(int header, u8 *entry)
{
    u8 *bp = get_block_addr(START_BLOCK | header);
    u32 size = *(u32 *)(bp + 12);
    int count = 0, expected, i, block;
    if (!bp[0] || size > (bup_dev ? 479u * 1024 : 510u * 128) ||
        (entry && memcmp(bp, entry, 11)))
        return BUP_BROKEN;
    if (bup_dev && size <= 960)
        return 0; /* Inline bytes are payload, not a pointer table. */
    expected = (size + (bup_dev ? 1023 : 127)) / (bup_dev ? 1024 : 128);
    for (i = 0; i < (bup_dev ? expected : 512); i++) {
        if (bup_dev) {
            block = *(u16 *)(bp + 64 + i * 2);
        } else {
            if (!(bp[64 + i / 8] & (1u << (i % 8))))
                continue;
            block = i;
        }
        if (claim_block(block))
            return BUP_BROKEN;
        count++;
    }
    if (count != expected ||
        (bup_dev && *(u16 *)(bp + 64 + expected * 2) != 0))
        return BUP_BROKEN;
    return 0;
}

static int check_backup(int dev)
{
    int block, i;
    u8 *entry, *bitmap;
    if (dev < 0 || dev > 1)
        return BUP_NON;
    bup_dev = dev;
    if (dev == 0) {
        if (BUPMEM->magic0 != 0x5361726f || BUPMEM->magic1 != 0x53617665)
            return BUP_UNFORMAT;
        if (BUPMEM->total_size != 65536 || BUPMEM->block_size != 128 ||
            !(BUPMEM->bitmap[0] & 1))
            return BUP_BROKEN;
        bitmap = (u8 *)BUPMEM->bitmap;
    } else {
        if (MEMS->magic0 != 0x5361726f || MEMS->magic1 != 0x4d656d73)
            return BUP_UNFORMAT;
        if (MEMS->total_size != 0x800000 || MEMS->bitmap[0] != 255)
            return BUP_BROKEN;
        bitmap = (u8 *)MEMS->bitmap;
    }
    memset(resize_bitmap, 0, sizeof(resize_bitmap));
    checked_saves = 0;
    /* Reserve every live header first, so data can never claim another header.
     * A repeated slot header also terminates a malformed linked-list cycle. */
    if (dev == 0) {
        block = BUPMEM->first_save;
        while (block) {
            if (claim_block(block))
                return BUP_BROKEN;
            checked_saves++;
            entry = get_block_addr(block);
            block = *(u16 *)(entry + 62);
        }
        block = BUPMEM->first_save;
        while (block) {
            if (validate_save_blocks(block, NULL))
                return BUP_BROKEN;
            entry = get_block_addr(block);
            block = *(u16 *)(entry + 62);
        }
    } else {
        for (i = 0; i < 448; i++) {
            entry = (u8 *)MEMS_HEADER + 1024 + i * 16;
            if (entry[0]) {
                if (claim_block(*(u16 *)(entry + 14)))
                    return BUP_BROKEN;
                checked_saves++;
            }
        }
        for (i = 0; i < 448; i++) {
            entry = (u8 *)MEMS_HEADER + 1024 + i * 16;
            if (entry[0] && validate_save_blocks(*(u16 *)(entry + 14), entry))
                return BUP_BROKEN;
        }
    }
    /* Extra allocated blocks remain reserved. Recount rather than trusting the
     * persisted free count; publish a corrected count only during a mutation. */
    checked_free = 0;
    for (i = dev ? 8 : 1; i < (dev ? 8064 : 512); i++)
        if (!(bitmap[i / 8] & (1u << (i % 8))))
            checked_free++;
    return 0;
}

static void set_free_count(void)
{
    if (bup_dev) MEMS->free_block = checked_free;
    else BUPMEM->free_block = checked_free;
}

/* Only untouched storage is initialized automatically. A damaged signature
 * in nonblank storage must remain available for recovery or explicit format. */
static int backup_blank(int dev)
{
    const volatile u8 *bp = dev ? (u8 *)MEMS_HEADER : (u8 *)BUP_MEM_ADDR;
    int size = dev ? 8192 : 65536;
    u8 value = bp[0];
    if (value != 0 && value != 255)
        return 0;
    for (int i = 1; i < size; i++)
        if (bp[i] != value)
            return 0;
    return 1;
}




/******************************************************************************/


// 根据BIOS的BUP代码，只有dev为2时才检测num。
// The original BIOS BUP convention checks the partition number only for device 2.
int sro_bup_sel_part(int dev, int num)
{
	printk("bup_sel_part(%d): %d\n", dev, num);

	if(dev<0 || dev>=2)
		return BUP_NON;

	return 0;
}


int sro_bup_format(int dev)
{
	printk("bup_format(%d)\n", dev);

	if(dev<0 || dev>1)
		return BUP_NON;

	if(dev==0){
		memset((u8*)BUP_MEM_ADDR, 0, 0x8000);
		// SaroSave
		BUPMEM->magic0 = 0x5361726f;
		BUPMEM->magic1 = 0x53617665;
		BUPMEM->total_size = 0x10000;
		BUPMEM->block_size = 0x80;
		memcpy((u8 *)BUPMEM->gameid, (u8*)0x06000c20, 16);
		memset((u8 *)BUPMEM->bitmap, 0, 0x40);
		BUPMEM->first_save = 0x0000;
		BUPMEM->free_block = 512-1;
		BUPMEM->bitmap[0] = 0x01;

		bup_update(BUPSAVE);
	}else{
		memset((u8*)MEMS_HEADER, 0, 0x2000);
		// SaroMems
		MEMS->magic0 = 0x5361726f;
		MEMS->magic1 = 0x4d656d73;
		MEMS->total_size = 0x00800000;
		MEMS->free_block = 1008*8-8;
		MEMS->bitmap[0] = 0xff;

		bup_update(MEMS_HDR);
	}

	return 0;
}


int sro_bup_stat(int dev, int dsize, BUPSTAT *stat)
{
    int result = check_backup(dev);
    u32 count, limit = dev ? 479u * 1024 : 510u * 128;
    if (result)
        return result;
    if (stat == NULL)
        return BUP_BROKEN;
    stat->total_size = dev ? 0x800000 : 65536;
    stat->block_size = dev ? 1024 : 128;
    stat->total_block = dev ? 8064 : 512;
    stat->free_block = checked_free;
    stat->free_size = checked_free ? (checked_free - 1) * stat->block_size : 0;
    stat->data_num = 0;
    if ((u32)dsize <= limit) {
        count = dev && dsize <= 960 ? 0 :
                ((u32)dsize + stat->block_size - 1) / stat->block_size;
        stat->data_num = checked_free / (count + 1);
        if (dev && stat->data_num > (u32)(448 - checked_saves))
            stat->data_num = 448 - checked_saves;
    }
    return 0;
}


static int resize_save(int header, u32 new_size)
{
    int first = bup_dev ? 8 : 1;
    int total = bup_dev ? 8064 : 512;
    int bsize = bup_dev ? 1024 : 128;
    int bitmap_size = bup_dev ? 1008 : 64;
    u32 limit = bup_dev ? 479u * 1024 : 510u * 128;
    u8 *bitmap = bup_dev ? (u8 *)MEMS->bitmap : (u8 *)BUPMEM->bitmap;
    u8 *bp;
    u32 old_size;
    int old_count, new_count, count, block, i, available;

    if (new_size > limit)
        return BUP_NOT_ENOUGH_MEMORY;
    if (header < first || header >= total ||
        (bitmap[0] & ((1u << first) - 1)) != (1u << first) - 1)
        return BUP_BROKEN;
    bp = get_block_addr(START_BLOCK | header);
    old_size = *(u32 *)(bp + 12);
    if (old_size > limit)
        return BUP_BROKEN;
    old_count = bup_dev && old_size <= 960 ? 0 : (old_size + bsize - 1) / bsize;
    new_count = bup_dev && new_size <= 960 ? 0 : (new_size + bsize - 1) / bsize;
    memcpy(resize_bitmap, bitmap, bitmap_size);
    if (!(resize_bitmap[header / 8] & (1u << (header % 8))))
        return BUP_BROKEN;

    count = 0;
    for (i = 0; i < (bup_dev ? old_count : 512); i++) {
        if (bup_dev) {
            block = *(u16 *)(bp + 64 + i * 2);
        } else {
            if (!(bp[64 + i / 8] & (1u << (i % 8))))
                continue;
            block = i;
        }
        if (block < first || block >= total || block == header ||
            !(resize_bitmap[block / 8] & (1u << (block % 8))))
            return BUP_BROKEN;
        /* Clear only the private bitmap; repeated references then fail. */
        resize_bitmap[block / 8] &= ~(1u << (block % 8));
        count++;
    }
    if (count != old_count ||
        (bup_dev && old_count && *(u16 *)(bp + 64 + old_count * 2) != 0))
        return BUP_BROKEN;

    count = available = 0;
    for (block = first; block < total; block++) {
        if (!(resize_bitmap[block / 8] & (1u << (block % 8)))) {
            if (count < new_count) {
                resize_blocks[count++] = block;
                resize_bitmap[block / 8] |= 1u << (block % 8);
            } else {
                available++;
            }
        }
    }
    if (count != new_count)
        return BUP_NOT_ENOUGH_MEMORY;

    /* Plan complete: publish in RAM, then let the caller store the full
     * payload and flush the new allocation metadata with the save. */
    memset(bp + 64, 0, bsize - 64);
    for (i = 0; i < new_count; i++) {
        if (bup_dev)
            *(u16 *)(bp + 64 + i * 2) = resize_blocks[i];
        else
            set_bitmap(bp + 64, resize_blocks[i], 1);
    }
    *(u32 *)(bp + 12) = new_size;
    memcpy(bitmap, resize_bitmap, bitmap_size);
    if (bup_dev)
        MEMS->free_block = available;
    else
        BUPMEM->free_block = available;
    return 0;
}


int sro_bup_write(int dev, BUPDIR *dir, u8 *data, int mode)
{
	int block_need, block, hdr, last, i;
	u8 *bp;

    int result = check_backup(dev);
    if (result)
        return result;
    if (dir == NULL || !dir->file_name[0] || (dir->data_size && data == NULL))
        return BUP_BROKEN;
    if (dir->data_size > (dev ? 479u * 1024 : 510u * 128))
        return BUP_NOT_ENOUGH_MEMORY;
    printk("bup_write(%d): %.11s data=%08x dsize=%08x\n", dev, dir->file_name, data, dir->data_size);

	// 计算所需的块数量。
	int block_size = (dev==0) ? 128 : 1024;
	if(dev==1 && (dir->data_size <= (1024-64))){
		block_need = 0;
	}else{
		block_need = (dir->data_size+block_size-1)/block_size;
	}

	// 1. 根据dir查找存档
	block = find_exact_save(dir->file_name, &last);

	// 2. 如果找到，且允许覆盖，则覆盖写入
	if(block>0){
		if(mode)
			return BUP_FOUND;

        bp = get_block_addr(START_BLOCK|block);
        int resized = *(u32 *)(bp + 12) != dir->data_size;
        if (resized) {
            int result = resize_save(block, dir->data_size);
            if (result)
                return result;
        }
        if (!resized)
            set_free_count();
        *(u32 *)(bp + 28) = dir->date;
        access_data(block, data, 2);

		if(dev==0){
			bup_update(BUPSAVE);
		}else{
			bup_update(MEMS_HDR | MEMS_SAV | (block_need ? MEMS_BUF : 0));
		}
		printk("Over write.\n");
		return 0;
	}

	// 3. 未找到。新建存档。
	if (dir->data_size > (dev ? 479u * 1024 : 510u * 128) || (dev == 1 && last < 0))
		return BUP_NOT_ENOUGH_MEMORY;
	int free_block = checked_free;
	printk("block_need=%d\n", block_need+1);
	if((block_need+1) > free_block){
		return BUP_NOT_ENOUGH_MEMORY;
	}

	// 分配起始块
    set_free_count();
	block = get_free_block(0);
	hdr = block;

	bp = get_block_addr(START_BLOCK|hdr);
	printk("start at %04x %08x\n", hdr, bp);

	// 写开始块
	memset(bp, 0, block_size);
	memcpy(bp+0x00, dir->file_name, 11);
	*(u32*)(bp+0x0c) = dir->data_size;
	memcpy(bp+0x10, dir->comment, 10);
	bp[0x1b] = dir->language;
	*(u32*)(bp+0x1c) = dir->date;

	// 分配块
	block = 0;
	for(i=0; i<block_need; i++){
		block = get_free_block(block);
		if(dev==0){
			set_bitmap(bp+0x40, block, 1);
		}else{
			*(u16*)(bp+0x40+i*2) = block;
		}
		block += 1;
	}

	// 写数据
	access_data(hdr, data, 2);

	// 更新last指针
	if(dev==1){
		memcpy((u8*)MEMS_HEADER+0x400+last*16, dir->file_name, 11);
		*(u16*)(MEMS_HEADER+0x400+last*16+0x0e) = hdr;
	}else{
		printk("Link save: %04x -> %04x\n", last, hdr);
		bp = get_block_addr(last);
		*(u16*)(bp+0x3e) = hdr;
	}

	if(dev==0){
		bup_update(BUPSAVE);
	}else{
		bup_update((block_need)? MEMS_ALL: MEMS_HDR|MEMS_SAV);
	}

	printk("Write done.\n");
	return 0;
}


int sro_bup_read(int dev, char *file_name, u8 *data)
{
    int block;
    int result = check_backup(dev);
    if (result)
        return result;
    if (file_name == NULL || !file_name[0])
        return BUP_NOT_FOUND;
    printk("bup_read(%d): %.11s  data=%08x\n", dev, file_name, data);
    block = find_exact_save(file_name, NULL);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	return access_data(block, data, 0);
}


int sro_bup_delete(int dev, char *file_name)
{
    int block, last, has_data;
    u8 *bp;
    int result = check_backup(dev);
    if (result)
        return result;
    if (file_name == NULL || !file_name[0])
        return BUP_NOT_FOUND;
    printk("bup_delete(%d): %.11s\n", dev, file_name);
    block = find_exact_save(file_name, &last);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	bp = get_block_addr(START_BLOCK|block);

    set_free_count();
	has_data = 1;
	// 释放开始块
	// Release the starting block.
	if(dev==0){
		set_bitmap((u8*)BUPMEM->bitmap, block, 0);
		BUPMEM->free_block += 1;
	}else{
		set_bitmap((u8*)MEMS->bitmap, block, 0);
		MEMS->free_block += 1;
		int dsize = *(u32*)(bp+0x0c);
		if(dsize<=960)
			has_data = 0;
	}

	// 释放数据块
	// Release the data blocks.
	if(has_data){
		u8 *bmp = bp+0x40;
		block = 0;
		while(1){
			block = get_next_block(bmp, block);
			if(block==0)
				break;
			if(dev==0){
				set_bitmap((u8*)BUPMEM->bitmap, block, 0);
				BUPMEM->free_block += 1;
			}else{
				set_bitmap((u8*)MEMS->bitmap, block, 0);
				MEMS->free_block += 1;
			}
			if(dev==1){
				bmp += 2;
			}else{
				block += 1;
			}
		}
	}

	// 更新last指针
	// Update the linked save list or cartridge directory entry.
	if(dev==1){
		memset((u8*)MEMS_HEADER+1024+last*16, 0, 16);
	}else{
		u8 *last_bp = get_block_addr(last);
		*(u16*)(last_bp+0x3e) = *(u16*)(bp+0x3e);
	}

	if(dev==0){
		bup_update(BUPSAVE);
	}else{
		bup_update(MEMS_HDR);
	}

	return 0;
}


int sro_bup_dir(int dev, char *file_name, int tbsize, BUPDIR *dir)
{
    int block, fnum;
    u8 *bp;
    if (check_backup(dev) || file_name == NULL || tbsize < 0 || (tbsize && dir == NULL))
        return 0;
    printk("bup_dir(%d): %.11s  %d\n", dev, file_name, tbsize);

	int block_size = (dev==0) ? 128 : 1024;

	fnum = 0;
	while(1){
		block = find_save(file_name, fnum, NULL);
		if(block<0){
			break;
		}

		if(fnum<tbsize){
			bp = get_block_addr(START_BLOCK|block);
			memcpy(dir->file_name, (char*)(bp+0), 11);
            dir->file_name[11] = 0;
			dir->data_size = *(u32*)(bp+0x0c);
			memcpy(dir->comment, bp+0x10, 10);
            dir->comment[10] = 0;
			dir->language = bp[0x1b];
			dir->date = *(u32*)(bp+0x1c);
			dir->block_size = (dir->data_size + block_size-1)/block_size;

			dir += 1;
		}
		fnum += 1;
	}

	if(fnum>tbsize){
		return -fnum;
	}
	return fnum;
}


int sro_bup_verify(int dev, char *file_name, u8 *data)
{
    int block;
    int result = check_backup(dev);
    if (result)
        return result;
    if (file_name == NULL || !file_name[0])
        return BUP_NOT_FOUND;
    printk("bup_verify(%d): %.11s\n", dev, file_name);
    block = find_exact_save(file_name, NULL);
	if(block<0){
		return BUP_NOT_FOUND;
	}

	return access_data(block, data, 1);
}


static char mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void sro_bup_get_date(u32 date, BUPDATE *bdt)
{
	int y, d, h, m;
	printk("bup_get_date: %08x\n", date);

	d = date/(24*60); date -= d*24*60;
	h = date/60; date -= h*60;

	y = d/(365*4+1); d -= y*(365*4+1);
	m = d/365; d -= m*365;
	if(m==3){
		if(d==0){
			m = 2;
			d = 365;
		}else{
			d -= 1;
		}
	}
	y = y*4+m;

	mdays[1] = (m==2)? 29: 28;
	m = 0;
	while(d>mdays[m]){
		d -= mdays[m];
		m += 1;
	}
	m += 1;
	d += 1;

	bdt->year = y;
	bdt->month = m;
	bdt->day = d;
	bdt->time = h;
	bdt->min = date;

	printk("  : %2d-%2d-%2d %2d:%2d\n",
		bdt->year, bdt->month, bdt->day, bdt->time, bdt->min);
}


u32 sro_bup_set_date(BUPDATE *bdt)
{
	u32 date;
	int y, m ,rem;

	printk("bup_set_date: %2d-%2d-%2d %2d:%2d\n",
		bdt->year, bdt->month, bdt->day, bdt->time, bdt->min);

	y = bdt->year;
	date = (y/4)*0x5b5;
	rem = y%4;
	if(rem){
		date += rem*0x16d+1;
	}

	mdays[1] = (rem==0)? 29: 28;
	m = bdt->month;
	while(m>1){
		date += mdays[m-2];
		m -= 1;
	}
	date += bdt->day;
	date *= 1440;

	date += (bdt->time)*0x3c;
	date += bdt->min;

	printk("  date=%08x\n", date);
	return date;
}


/******************************************************************************/


int use_sys_bup = 0;


int (*sys_bup_sel_part)(int dev, int num);
int (*sys_bup_format)(int dev);
int (*sys_bup_stat)(int dev, int dsize, BUPSTAT *stat);
int (*sys_bup_write)(int dev, BUPDIR *dir, u8 *data, int mode);
int (*sys_bup_read)(int dev, char *file_name, u8 *data);
int (*sys_bup_delete)(int dev, char *file_name);
int (*sys_bup_dir)(int dev, char *file_name, int tbsize, BUPDIR *dir);
int (*sys_bup_verify)(int dev, char *file_name, u8 *data);
void (*sys_bup_get_date)(u32 date, BUPDATE *bdt);
u32 (*sys_bup_set_date)(BUPDATE *bdt);


int bup_sel_part(int dev, int num)
{
	if(use_sys_bup && dev==0){
		return sys_bup_sel_part(dev, num);
	}else{
		return sro_bup_sel_part(dev, num);
	}
}


int bup_format(int dev)
{
	if(use_sys_bup && dev==0){
		return sys_bup_format(dev);
	}else{
		return sro_bup_format(dev);
	}
}


int bup_stat(int dev, int dsize, BUPSTAT *stat)
{
	if(use_sys_bup && dev==0){
		return sys_bup_stat(dev, dsize, stat);
	}else{
		return sro_bup_stat(dev, dsize, stat);
	}
}


int bup_write(int dev, BUPDIR *dir, u8 *data, int mode)
{
	if(use_sys_bup && dev==0){
		return sys_bup_write(dev, dir, data, mode);
	}else{
		return sro_bup_write(dev, dir, data, mode);
	}
}


int bup_read(int dev, char *file_name, u8 *data)
{
	if(use_sys_bup && dev==0){
		return sys_bup_read(dev, file_name, data);
	}else{
		return sro_bup_read(dev, file_name, data);
	}
}


int bup_delete(int dev, char *file_name)
{
	if(use_sys_bup && dev==0){
		return sys_bup_delete(dev, file_name);
	}else{
		return sro_bup_delete(dev, file_name);
	}
}


int bup_dir(int dev, char *file_name, int tbsize, BUPDIR *dir)
{
	if(use_sys_bup && dev==0){
		return sys_bup_dir(dev, file_name, tbsize, dir);
	}else{
		return sro_bup_dir(dev, file_name, tbsize, dir);
	}
}


int bup_verify(int dev, char *file_name, u8 *data)
{
	if(use_sys_bup && dev==0){
		return sys_bup_verify(dev, file_name, data);
	}else{
		return sro_bup_verify(dev, file_name, data);
	}
}


void bup_get_date(u32 date, BUPDATE *bdt)
{
	if(use_sys_bup){
		return sys_bup_get_date(date, bdt);
	}else{
		return sro_bup_get_date(date, bdt);
	}
}


u32 bup_set_date(BUPDATE *bdt)
{
	if(use_sys_bup){
		return sys_bup_set_date(bdt);
	}else{
		return sro_bup_set_date(bdt);
	}
}


/******************************************************************************/


void bup_init(u8 *lib_addr, u8 *work_addr, void *cfg_ptr)
{
	BUPCFG *cfg = (BUPCFG*)cfg_ptr;

	if(debug_flag&1)
		sci_init();

	printk("bup_init: lib=%08x work=%08x\n", lib_addr, work_addr);

	if(use_sys_bup){
		printk("sys_bup_init ... %08x\n", sys_bup_init);
		sys_bup_init(lib_addr, work_addr, cfg_ptr);
		printk("done.\n");

		sys_bup_sel_part = (void*)*(u32*)(work_addr+0x04);
		sys_bup_format   = (void*)*(u32*)(work_addr+0x08);
		sys_bup_stat     = (void*)*(u32*)(work_addr+0x0c);
		sys_bup_write    = (void*)*(u32*)(work_addr+0x10);
		sys_bup_read     = (void*)*(u32*)(work_addr+0x14);
		sys_bup_delete   = (void*)*(u32*)(work_addr+0x18);
		sys_bup_dir      = (void*)*(u32*)(work_addr+0x1c);
		sys_bup_verify   = (void*)*(u32*)(work_addr+0x20);
		sys_bup_get_date = (void*)*(u32*)(work_addr+0x24);
		sys_bup_set_date = (void*)*(u32*)(work_addr+0x28);
	}else{
		*(u32*)(BUP_WORK_ADDR) = (u32)work_addr;
		*(u32*)(work_addr+0x00) = (u32)lib_addr;
		cfg[0].unit_id = 1;
		cfg[0].partition = 1;
	}

	*(u32*)(work_addr+0x04) = (u32)bup_sel_part;
	*(u32*)(work_addr+0x08) = (u32)bup_format;
	*(u32*)(work_addr+0x0c) = (u32)bup_stat;
	*(u32*)(work_addr+0x10) = (u32)bup_write;
	*(u32*)(work_addr+0x14) = (u32)bup_read;
	*(u32*)(work_addr+0x18) = (u32)bup_delete;
	*(u32*)(work_addr+0x1c) = (u32)bup_dir;
	*(u32*)(work_addr+0x20) = (u32)bup_verify;
	*(u32*)(work_addr+0x24) = (u32)bup_get_date;
	*(u32*)(work_addr+0x28) = (u32)bup_set_date;

	cfg[1].unit_id = 2;
	cfg[1].partition = 1;
	cfg[2].unit_id = 0;
	cfg[2].partition = 0;

	if(use_sys_bup==0){
		// Check "SaroSave"
		if((BUPMEM->magic0!=0x5361726f || BUPMEM->magic1!=0x53617665) && backup_blank(0)){
			printk("  empty bup memory, need format.\n");
			bup_format(0);
		}
	}

	// Check "SaroMems"
	if((MEMS->magic0!=0x5361726f || MEMS->magic1!=0x4d656d73) && backup_blank(1)){
		printk("  empty memory card, need format.\n");
		bup_format(1);
	}

}


/******************************************************************************/




