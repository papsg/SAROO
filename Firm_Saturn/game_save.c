
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

/* Keep only the continuation's location and phase; payload remains in the
 * existing shared buffers. A caller must resupply an interrupted payload. */
#define SAVE_WRITING 1
#define SAVE_COMMIT  2
typedef struct {
    u16 header;
    short last;
    u8 state, flags, mode;
} PendingSave;
static PendingSave pending_save[2];

/* The capability is latched at menu entry. A cleared pair of registers
 * cannot acknowledge a tagged request. */
int save_command(int cmd, int arg)
{
    int ready = menu_command_wait();
    if (ready) return ready;
    int ack = save_ack;
    SS_ARG = arg;
    SS_CMD = cmd | ack;
    while (SS_CMD);
    return SS_ARG ^ ack;
}

static int bup_update(int flag)
{
    if (save_command(flag == BUPSAVE ? SSCMD_SSAVE : SSCMD_SMEMS,
                     flag == BUPSAVE ? 0 : flag))
        return BUP_BROKEN;
    if (flag & MEMS_BUF)
        mblk_bmp[0] = mblk_bmp[1] = 0;
    return 0;
}

static int finish_pending(int dev)
{
    PendingSave *pending = &pending_save[dev];
    if (pending->state == SAVE_WRITING)
        return BUP_BROKEN;
    if (pending->state == SAVE_COMMIT) {
        if (bup_update(pending->flags))
            return BUP_BROKEN;
        pending->state = 0;
    }
    return 0;
}

static int commit_save(int dev, int flags)
{
    pending_save[dev].flags = flags;
    pending_save[dev].state = SAVE_COMMIT;
    return finish_pending(dev);
}

static int mems_load(int blk)
{
    return save_command(SSCMD_LMEMS, blk) ? BUP_BROKEN : 0;
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
    int header = id & START_BLOCK;
    id &= 0x7fff;
    if (bup_dev == 0)
        return id < 512 ? (u8*)(BUP_MEM_ADDR + id * 128) : NULL;
    if (id < 8 || id >= 8064)
        return NULL;
    if (header) {
        if (id != msav_blk) {
            msav_blk = 0;
            if (mems_load(START_BLOCK | id))
                return NULL;
            msav_blk = id;
        }
        return (u8*)MEMS_TMP;
    }
    if (id < mblk_start || id > mblk_end) {
        if ((mblk_bmp[0] || mblk_bmp[1]) && bup_update(MEMS_BUF))
            return NULL; /* Keep this dirty window and its ownership for retry. */
        mblk_start = mblk_end = 0;
        if (mems_load(id))
            return NULL;
        mblk_start = id;
        mblk_end = id + 63;
        mblk_bmp[0] = mblk_bmp[1] = 0;
    }
    return (u8*)(MEMS_BUFFER + (id - mblk_start) * 1024);
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
    if (!bp) return BUP_BROKEN;
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
        if (!bp) return BUP_BROKEN;

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
            if (!bp) return -1;
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

static int validate_save_layout(u8 *bp, u8 *entry)
{
    if (!bp) return BUP_BROKEN;
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

static u8 *mems_entry(int index)
{
    return (u8 *)MEMS_HEADER + 1024 + index * 16;
}

static int mems_entry_block(int index)
{
    return *(u16 *)(mems_entry(index) + 14);
}

/* Sort directory indices in existing allocation scratch. A heap bounds the
 * unordered case; already ordered directories need only a linear check. */
static void sift_headers(int root, int count)
{
    u16 value = resize_blocks[root];
    int block = mems_entry_block(value);
    while (root * 2 + 1 < count) {
        int child = root * 2 + 1;
        if (child + 1 < count &&
            mems_entry_block(resize_blocks[child]) < mems_entry_block(resize_blocks[child + 1]))
            child++;
        if (block >= mems_entry_block(resize_blocks[child]))
            break;
        resize_blocks[root] = resize_blocks[child];
        root = child;
    }
    resize_blocks[root] = value;
}

static void sort_headers(int count)
{
    int i;
    for (i = 1; i < count; i++)
        if (mems_entry_block(resize_blocks[i - 1]) > mems_entry_block(resize_blocks[i]))
            break;
    if (i == count || count < 2)
        return;
    for (i = count / 2; i > 0; i--)
        sift_headers(i - 1, count);
    for (i = count - 1; i > 0; i--) {
        u16 value = resize_blocks[0];
        resize_blocks[0] = resize_blocks[i];
        resize_blocks[i] = value;
        sift_headers(0, i);
    }
}

static int validate_mems_headers(void)
{
    int i = 0;
    /* Preserve a live payload cache. Evicting it to batch a few headers could
     * add a later 64 KiB data read, outweighing the saved command exchanges. */
    if (mblk_start || mblk_end || checked_saves < 64) {
        for (i = 0; i < checked_saves; i++) {
            int block = mems_entry_block(resize_blocks[i]);
            if (validate_save_layout(get_block_addr(START_BLOCK | block),
                                     mems_entry(resize_blocks[i])))
                return BUP_BROKEN;
        }
        return 0;
    }
    sort_headers(checked_saves);
    while (i < checked_saves) {
        int first = mems_entry_block(resize_blocks[i]);
        int count = 1;
        while (count < 64 && i + count < checked_saves &&
               mems_entry_block(resize_blocks[i + count]) == first + count)
            count++;
        if (count < 64) {
            for (int j = 0; j < count; j++) {
                if (validate_save_layout(get_block_addr(START_BLOCK | (first + j)),
                                         mems_entry(resize_blocks[i + j])))
                    return BUP_BROKEN;
            }
        } else {
            /* Use the existing 64-block loader only for a full run: no extra
             * header bytes and no second file handle. Never discard unexpected
             * dirty data. Keep the Saturn payload window invalid so subsequent
             * payload access reloads both sides' indices before any writes. */
            if (mblk_bmp[0] || mblk_bmp[1])
                return BUP_BROKEN;
            mblk_start = mblk_end = 0;
            if (mems_load(first))
                return BUP_BROKEN;
            for (int j = 0; j < count; j++) {
                u8 *bp = (u8 *)MEMS_BUFFER + j * 1024;
                if (validate_save_layout(bp, mems_entry(resize_blocks[i + j])))
                    return BUP_BROKEN;
            }
        }
        i += count;
    }
    return 0;
}

static int check_backup(int dev)
{
    int block, i;
    u8 *entry, *bitmap;
    if (dev < 0 || dev > 1)
        return BUP_NON;
    bup_dev = dev;
    if (finish_pending(dev)) return BUP_BROKEN;
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
            if (!entry) return BUP_BROKEN;
            block = *(u16 *)(entry + 62);
        }
        block = BUPMEM->first_save;
        while (block) {
            if (validate_save_layout(get_block_addr(START_BLOCK | block), NULL))
                return BUP_BROKEN;
            entry = get_block_addr(block);
            if (!entry) return BUP_BROKEN;
            block = *(u16 *)(entry + 62);
        }
    } else {
        for (i = 0; i < 448; i++) {
            entry = (u8 *)MEMS_HEADER + 1024 + i * 16;
            if (entry[0]) {
                if (claim_block(*(u16 *)(entry + 14)))
                    return BUP_BROKEN;
                resize_blocks[checked_saves++] = i;
            }
        }
        if (validate_mems_headers())
            return BUP_BROKEN;
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

    pending_save[dev].state = 0;
    pending_save[dev].header = 0;
    if (dev) {
        msav_blk = mblk_start = mblk_end = 0;
        mblk_bmp[0] = mblk_bmp[1] = 0;
    }

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

		return commit_save(dev, BUPSAVE);
	}else{
		memset((u8*)MEMS_HEADER, 0, 0x2000);
		// SaroMems
		MEMS->magic0 = 0x5361726f;
		MEMS->magic1 = 0x4d656d73;
		MEMS->total_size = 0x00800000;
		MEMS->free_block = 1008*8-8;
		MEMS->bitmap[0] = 0xff;

		return commit_save(dev, MEMS_HDR);
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
    if (!bp) return BUP_BROKEN;
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


static int same_pending_write(int dev, BUPDIR *dir, int mode)
{
    PendingSave *pending = &pending_save[dev];
    if (!pending->header || (dev && msav_blk != pending->header) ||
        pending->mode != !!mode)
        return 0;
    u8 *bp = get_block_addr(START_BLOCK | pending->header);
    return bp && !strncmp((char*)bp, dir->file_name, 11) &&
           *(u32*)(bp + 12) == dir->data_size;
}

static int write_pending(int dev, BUPDIR *dir, u8 *data)
{
    PendingSave *pending = &pending_save[dev];
    int header = pending->header;
    u8 *bp = get_block_addr(START_BLOCK | header);
    if (!bp)
        return BUP_BROKEN;
    pending->state = SAVE_WRITING;
    *(u32*)(bp + 28) = dir->date;
    bp[27] = dir->language;
    memcpy(bp + 16, dir->comment, 10);
    int result = access_data(header, data, 2);
    if (result)
        return result;
    if (pending->last >= 0) {
        if (dev) {
            u8 *entry = (u8*)MEMS_HEADER + 1024 + pending->last * 16;
            memcpy(entry, bp, 11);
            *(u16*)(entry + 14) = header;
        } else {
            bp = get_block_addr(pending->last);
            if (!bp)
                return BUP_BROKEN;
            *(u16*)(bp + 62) = header;
        }
    }
    return commit_save(dev, pending->flags);
}

int sro_bup_write(int dev, BUPDIR *dir, u8 *data, int mode)
{
    if (dev < 0 || dev > 1)
        return BUP_NON;
    if (!dir || !dir->file_name[0] || (dir->data_size && !data))
        return BUP_BROKEN;
    if (dir->data_size > (dev ? 479u * 1024 : 510u * 128))
        return BUP_NOT_ENOUGH_MEMORY;
    bup_dev = dev;
    PendingSave *pending = &pending_save[dev];
    if (pending->state && same_pending_write(dev, dir, mode))
        return write_pending(dev, dir, data);
    int result = check_backup(dev);
    if (result)
        return result;

    int bsize = dev ? 1024 : 128;
    int blocks = dev && dir->data_size <= 960 ? 0 : (dir->data_size + bsize - 1) / bsize;
    int last;
    int header = find_exact_save(dir->file_name, &last);
    u8 *bp;
    if (header > 0) {
        if (mode)
            return BUP_FOUND;
        bp = get_block_addr(START_BLOCK | header);
        if (!bp)
            return BUP_BROKEN;
        if (*(u32*)(bp + 12) != dir->data_size) {
            result = resize_save(header, dir->data_size);
            if (result)
                return result;
        } else {
            set_free_count();
        }
        last = -1;
    } else {
        if ((dev && last < 0) || blocks + 1 > checked_free)
            return BUP_NOT_ENOUGH_MEMORY;
        set_free_count();
        header = get_free_block(0);
        bp = get_block_addr(START_BLOCK | header);
        if (!bp) {
            /* Header loading precedes all other allocation/publication. */
            set_bitmap(dev ? (u8*)MEMS->bitmap : (u8*)BUPMEM->bitmap, header, 0);
            if (dev) MEMS->free_block++; else BUPMEM->free_block++;
            return BUP_BROKEN;
        }
        memset(bp, 0, bsize);
        memcpy(bp, dir->file_name, 11);
        *(u32*)(bp + 12) = dir->data_size;
        int block = 0;
        for (int i = 0; i < blocks; i++) {
            block = get_free_block(block);
            if (dev) *(u16*)(bp + 64 + i * 2) = block;
            else set_bitmap(bp + 64, block, 1);
            block++;
        }
    }
    pending->header = header;
    pending->last = last;
    pending->flags = dev ? MEMS_HDR | MEMS_SAV | (blocks ? MEMS_BUF : 0) : BUPSAVE;
    pending->mode = !!mode;
    pending->state = SAVE_WRITING;
    return write_pending(dev, dir, data);
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
    if (!bp) return BUP_BROKEN;

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
        pending_save[dev].header = 0;
		return commit_save(dev, BUPSAVE);
	}else{
        pending_save[dev].header = 0;
		return commit_save(dev, MEMS_HDR);
	}

	return 0;
}


static void directory_record(BUPDIR *dir, u8 *bp, int bsize)
{
    memset(dir, 0, sizeof(*dir));
    memcpy(dir->file_name, bp, 11);
    memcpy(dir->comment, bp + 16, 10);
    dir->data_size = *(u32*)(bp + 12);
    dir->language = bp[27];
    dir->date = *(u32*)(bp + 28);
    dir->block_size = (dir->data_size + bsize - 1) / bsize;
}

int sro_bup_dir(int dev, char *file_name, int tbsize, BUPDIR *dir)
{
    if (check_backup(dev) || !file_name || tbsize < 0 || (tbsize && !dir))
        return 0;
    int len = 0, count = 0;
    while (len < 11 && file_name[len]) len++;
    if (!dev) {
        /* Slot records are already resident and fully validated. */
        int block = BUPMEM->first_save;
        while (block) {
            u8 *bp = get_block_addr(block);
            if (!strncmp((char*)bp, file_name, len)) {
                if (count < tbsize) directory_record(dir + count, bp, 128);
                count++;
            }
            block = *(u16*)(bp + 62);
        }
    } else {
        for (int i = 0; i < 448; i++) {
            u8 *entry = mems_entry(i);
            if (entry[0] && !strncmp((char*)entry, file_name, len)) count++;
        }
        int copies = count < tbsize ? count : tbsize;
        int bytes = copies * sizeof(BUPDIR);
        /* Validation scratch fits 28 records. Larger tables borrow a clean
         * payload window, avoiding a permanent table-sized allocation. */
        u8 *stage = resize_bitmap;
        if (bytes > (int)sizeof(resize_bitmap)) {
            if (mblk_bmp[0] || mblk_bmp[1]) return 0;
            mblk_start = mblk_end = 0;
            stage = (u8*)MEMS_BUFFER;
        }
        int out = 0;
        for (int i = 0; i < 448 && out < copies; i++) {
            u8 *entry = mems_entry(i);
            if (!entry[0] || strncmp((char*)entry, file_name, len)) continue;
            u8 *bp = get_block_addr(START_BLOCK | *(u16*)(entry + 14));
            if (!bp) return 0;
            BUPDIR record;
            directory_record(&record, bp, 1024);
            memcpy(stage + out * sizeof(record), &record, sizeof(record));
            out++;
        }
        if (bytes) memcpy(dir, stage, bytes);
    }
    return count > tbsize ? -count : count;
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


/* Gregorian dates in the API's 8-bit year range: 1980 through 2235.
 * Century exceptions within this range are 2100 (120) and 2200 (220). */
static const u16 date_month_start[13] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

static unsigned date_year_start(unsigned year)
{
    return year * 365 + (year + 3) / 4 - (year > 120) - (year > 220);
}

static unsigned date_leap(unsigned year)
{
    return !(year & 3) && year != 120 && year != 220;
}

void sro_bup_get_date(u32 date, BUPDATE *bdt)
{
    unsigned days, year, day, leap, low = 0, high = 12, minute;
    if (!bdt) return;
    days = date / 1440;
    if (days >= date_year_start(256)) {
        /* No status channel in this ABI: zero month/day denote invalid data. */
        memset(bdt, 0, sizeof(*bdt));
        return;
    }
    printk("bup_get_date: %08x\n", date);
    /* This four-year estimate is at most one year low in the bounded range. */
    year = days * 4 / 1461;
    if (days >= date_year_start(year + 1)) year++;
    day = days - date_year_start(year);
    leap = date_leap(year);
    while (low + 1 < high) {
        unsigned middle = (low + high) / 2;
        if (day >= date_month_start[middle] + (leap && middle >= 2))
            low = middle;
        else
            high = middle;
    }
    minute = date % 1440;
    bdt->year = year;
    bdt->month = low + 1;
    bdt->day = day - date_month_start[low] - (leap && low >= 2) + 1;
    bdt->time = minute / 60;
    bdt->min = minute % 60;
    bdt->week = (days + 2) % 7; /* 1980-01-01 was Tuesday; Sunday is zero. */
    printk("  : %2d-%2d-%2d %2d:%2d\n",
           bdt->year, bdt->month, bdt->day, bdt->time, bdt->min);
}

u32 sro_bup_set_date(BUPDATE *bdt)
{
    unsigned month, leap, days;
    if (!bdt || bdt->month < 1 || bdt->month > 12 || !bdt->day ||
        bdt->time >= 24 || bdt->min >= 60)
        return 0xffffffffu;
    month = bdt->month - 1;
    leap = date_leap(bdt->year);
    if (bdt->day > date_month_start[month + 1] - date_month_start[month] +
                   (leap && month == 1))
        return 0xffffffffu;
    printk("bup_set_date: %2d-%2d-%2d %2d:%2d\n",
           bdt->year, bdt->month, bdt->day, bdt->time, bdt->min);
    /* Week is derived from the date; packing neither needs nor modifies it. */
    days = date_year_start(bdt->year) + date_month_start[month] +
           (leap && month >= 2) + bdt->day - 1;
    u32 date = days * 1440 + bdt->time * 60 + bdt->min;
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




