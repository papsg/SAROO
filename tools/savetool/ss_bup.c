
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "bup.h"
#include "bup_format.h"


/******************************************************************************/

static u8 bup_mem[32768];
static int ss_bup_type;

typedef struct {
	u32 total_size;
	u32 total_block;
	u32 block_size;
	u8  block_map[64];

	// 描述某个存档的信息
	int dsize;      // 数据大小
	int bsize;      // 一共有多少块
	u16 blist[512];
	int nbp;        // blist中下一块的索引
	int dp;         // 块中的数据指针
}BUPINFO;


static BUPINFO bup_info;
#define GET_BUP_INFO()  (&bup_info)


/******************************************************************************/


static u8 *get_block_addr(BUPINFO *binfo, int id)
{
	return (u8*)(bup_mem + binfo->block_size*id);
}


/* Every save has a terminated table, including a one-block save. Table words
 * and payload continue after the four-byte tags of noncontiguous blocks. */
static int load_blist(BUPINFO *binfo, int first)
{
    u8 seen[64] = {0};
    u8 *bp;
    unsigned cursor = 0, offset = 34, count = 1, block, capacity;
    u32 size;
    if (first < 2 || first >= 512)
        return -1;
    bp = get_block_addr(binfo, first);
    size = get_be32(bp + 30);
    if (size > sizeof(bup_mem))
        return -1;
    binfo->blist[0] = first;
    seen[first / 8] |= 1u << (first % 8);
    for (;;) {
        if (offset == 64) {
            cursor++;
            offset = 4;
        }
        if (cursor >= count)
            return -1;
        bp = get_block_addr(binfo, binfo->blist[cursor]);
        block = get_be16(bp + offset);
        offset += 2;
        if (!block)
            break;
        if (block < 2 || block >= 512 || count >= 510 ||
            (seen[block / 8] & (1u << (block % 8))))
            return -1;
        seen[block / 8] |= 1u << (block % 8);
        binfo->blist[count++] = block;
    }
    capacity = 64 - offset + (count - cursor - 1) * 60;
    if (size > capacity)
        return -1;
    binfo->dsize = size;
    binfo->bsize = count;
    binfo->nbp = cursor + 1;
    binfo->dp = offset;
    return 0;
}

static int read_data(BUPINFO *binfo, u8 *data)
{
    int cursor = binfo->nbp - 1, offset = binfo->dp, remaining = binfo->dsize;
    while (remaining > 0) {
        int count;
        if (offset == 64) {
            cursor++;
            offset = 4;
        }
        if (cursor >= binfo->bsize)
            return -1;
        count = 64 - offset;
        if (count > remaining)
            count = remaining;
        memcpy(data, get_block_addr(binfo, binfo->blist[cursor]) + offset, count);
        data += count;
        remaining -= count;
        offset += count;
    }
    return 0;
}

static int scan_save(BUPINFO *binfo)
{
    int block, i;
    memset(binfo->block_map, 0, sizeof(binfo->block_map));
    binfo->block_map[0] = 3;
    /* Claim every header before data, including later physical headers. */
    for (block = 2; block < 512; block++)
        if (get_be32(get_block_addr(binfo, block)) == 0x80000000)
            set_bitmap(binfo->block_map, block, 1);
    for (block = 2; block < 512; block++) {
        u8 *bp = get_block_addr(binfo, block);
        if (get_be32(bp) != 0x80000000)
            continue;
        if (!bp[4] || load_blist(binfo, block) < 0)
            return -1;
        for (i = 1; i < binfo->bsize; i++) {
            unsigned id = binfo->blist[i], mask = 1u << (id % 8);
            if (binfo->block_map[id / 8] & mask)
                return -1;
            binfo->block_map[id / 8] |= mask;
        }
    }
    return 0;
}


/******************************************************************************/


int ss_bup_export(int slot_id, int index, int exp_type)
{
	BUPINFO *binfo = GET_BUP_INFO();
	int block, i;
	u8 *bp;

	i = 0;
	for(block=2; block<binfo->total_block; block++){
		bp = get_block_addr(binfo, block);
		if(get_be32(bp)==0x80000000){
			// 找到了save起始块
			if(i == index){
				/* The validated reader overwrites every emitted payload byte. */
				memset(save_buf, 0, BUP_HEADER_SIZE);
				strcpy((char*)save_buf, "SSAVERAW");
				memcpy(save_buf+0x10, bp+4, 11);    // Filename
				memcpy(save_buf+0x1c, bp+0x1e, 4);  // Size
				memcpy(save_buf+0x20, bp+0x10, 10); // Comment
				save_buf[0x2b] = bp[0x0f];          // Language
				memcpy(save_buf+0x2c, bp+0x1a, 4);  // Date

                if (load_blist(binfo, block) < 0 || read_data(binfo, save_buf+0x40) < 0)
                    return -1;

				char fname[SAVE_FILENAME_SIZE];
				int fsize = get_be32(save_buf+0x1c);
				if (exp_type != 1) put_be32(save_buf+0x0c, crc32b(save_buf+0x10, fsize+0x30));
				if(save_filename(fname, save_buf+0x10, exp_type ? BUP_EXTENSION : ".SRO")<0)
					return -1;

				// if Export format is .BUP, set new header
				if (exp_type==1){
					vmem_bup_header_t bup_header = {0};
					
					memcpy(bup_header.magic, VMEM_MAGIC_STRING, VMEM_MAGIC_STRING_LEN);
					memcpy(bup_header.dir.filename, bp+4, 11);
					memcpy(bup_header.dir.comment, bp+0x10, 10);
					memcpy(&bup_header.dir.date, bp+0x1a, 4);
					memcpy(&bup_header.dir.datasize, bp+0x1e, 4);
					memcpy(&bup_header.date, bp+0x1a, 4);
					bup_header.dir.language = bp[0x0f];

					memcpy(save_buf, &bup_header, BUP_HEADER_SIZE);
				}

				if(export_save_file(fname, save_buf, fsize+0x40)<0)
					return -1;

				printf("Export Save_%d: %s\n", index, fname);
				return 0;
			}
			i += 1;
		}
	}

	return -1;
}


int ss_bup_import(int slot_id, int save_id, char *save_name)
{
	return -1;
}


int ss_bup_delete(int slot_id, int save_id)
{
	return -1;
}


int ss_bup_create(char *game_id)
{
	return -1;
}


int ss_bup_list(int slot_id)
{
	BUPINFO *binfo = GET_BUP_INFO();
	int i, snum;
	u8 *bp;

	snum = 0;
	for(i=2; i<binfo->total_block; i++){
		bp = get_block_addr(binfo, i);
		if(get_be32(bp)==0x80000000){
			// 找到了save起始块
			printf("Save_%d: %.11s\n", snum, bp+4);
			snum += 1;
		}
	}

	return 0;
}


int ss_bup_init(u8 *bbuf, int bytes)
{
    BUPINFO *binfo = GET_BUP_INFO();
    int i;
    if (bytes == 65536) {
        ss_bup_type = 0;
        for (i = 0; i < 32768; i++)
            bup_mem[i] = bbuf[i * 2 + 1];
    } else if (bytes == 32768) {
        ss_bup_type = 1;
        memcpy(bup_mem, bbuf, 32768);
    } else {
        return -1;
    }
    if (memcmp(bup_mem, "BackUpRam Format", 16) != 0)
        return -1;
    memset(binfo, 0, sizeof(BUPINFO));
    binfo->total_size = 32768;
    binfo->total_block = 512;
    binfo->block_size = 64;
    if (scan_save(binfo) < 0) {
        printf("Invalid Saturn backup block table or payload extent.\n");
        return -1;
    }
    return 0;
}


/******************************************************************************/


