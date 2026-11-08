
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "bup.h"
#include "bup_format.h"


/******************************************************************************/

static u8 *mems_buf;

/******************************************************************************/

typedef struct {
	u32 magic0;
	u32 magic1;
	u32 total_size;
	u16 free_block;
	u16 first_save;
	u8  bitmap[1024-16];
}MEMSHEADER;

#define MEMS  ((MEMSHEADER*)(mems_buf))

static u16 block_size = 1024;
static u16 free_block;

/* The dispatcher establishes the complete MEMS extent before initialization.
 * Check all stored ownership before a command may follow a directory pointer. */
static int claim_block(u8 *seen, unsigned block)
{
    unsigned mask = 1u<<(block%8);
    if(block<8 || block>=8064 || !(MEMS->bitmap[block/8] & mask) || (seen[block/8] & mask))
        return -1;
    seen[block/8] |= mask;
    return 0;
}

static int validate_mems(void)
{
    u8 seen[sizeof(MEMS->bitmap)] = {0};
    int i, j, available = 0;

    if(get_be32(&MEMS->total_size)!=0x800000 || MEMS->bitmap[0]!=0xff)
        return -1;
    /* Reserve every header before admitting any data block. */
    for(i=0; i<448; i++){
        u8 *entry = mems_buf+1024+i*16;
        if(entry[0] && claim_block(seen, get_be16(entry+14))<0)
            return -1;
    }
    for(i=0; i<448; i++){
        u8 *entry = mems_buf+1024+i*16;
        u8 *header;
        u32 size, count;
        if(!entry[0])
            continue;
        header = mems_buf+get_be16(entry+14)*1024;
        size = get_be32(header+12);
        /* Match the entire stored field, including equal raw bytes after NUL. */
        if(memcmp(entry, header, 11)!=0 || size>479u*1024)
            return -1;
        if(size<=960)
            continue;
        count = (size+1023)/1024;
        for(j=0; j<(int)count; j++)
            if(claim_block(seen, get_be16(header+64+j*2))<0)
                return -1;
        if(get_be16(header+64+count*2)!=0)
            return -1;
    }
    /* Preserve old allocated orphans; the bitmap is authoritative. Recount
     * locally so a read-only command does not repair/change the input file. */
    for(i=8; i<8064; i++)
        if(!(MEMS->bitmap[i/8] & (1u<<(i%8))))
            available++;
    free_block = available;
    return 0;
}

/******************************************************************************/


static u8 *get_block_addr(int id)
{
	return (u8*)(mems_buf + block_size*id);
}


static int access_data(int block, u8 *data, int type)
{
	u8 *bp, *bmp;
	int dsize, asize;

	bp = get_block_addr(block);
	dsize = get_be32(bp+0x0c);
	if(dsize<=(block_size-64)){
		if(type==0){	// read
			memcpy(data, bp+0x40, dsize);
		}else{			// write
			memcpy(bp+0x40, data, dsize);
		}
		return 0;
	}

	bmp = bp+0x40;
	while(dsize>0){
		block = get_be16(bmp);
		bmp += 2;
		bp = get_block_addr(block);

		asize = (dsize>block_size)? block_size : dsize;
		//printf("dsize=%04x block=%04x asize=%04x\n", dsize, block, asize);

		if(type==0){	// read
			memcpy(data, bp, asize);
		}else{			// write
			memcpy(bp, data, asize);
		}
		data += asize;
		dsize -= asize;
	}

	return 0;
}


/******************************************************************************/


int sr_mems_export(int slot_id, int save_id, int exp_type)
{
	int block, i, index;
	u8 *bp;

	index = 0;
	for(i=0; i<448; i++){
		bp = (u8*)(mems_buf+1024+i*16);
		if(bp[0]==0)
			continue;
		if(index == save_id){
			block = get_be16(bp+0x0e);
			bp = get_block_addr(block);

			memset(save_buf, 0, 0x100000);

			strcpy((char*)save_buf, "SSAVERAW");
			memcpy(save_buf+0x10, bp+0, 11);    // Filename
			memcpy(save_buf+0x1c, bp+0x0c, 4);  // Size
			memcpy(save_buf+0x20, bp+0x10, 10); // Comment
			save_buf[0x2b] = bp[0x1b];          // Language
			memcpy(save_buf+0x2c, bp+0x1c, 4);  // Date

			access_data(block, save_buf+0x40, 0);

			char fname[16];
			int fsize = get_be32(save_buf+0x1c);
			put_be32(save_buf+0x0c, crc32b(save_buf+0x10, fsize+0x30));
			sprintf(fname, "%s%s", save_buf+0x10, exp_type ? BUP_EXTENSION : ".SRO");

			// if Export format is .BUP, set new header
			if (exp_type==1){
				vmem_bup_header_t bup_header = {0};
				
				memcpy(bup_header.magic, VMEM_MAGIC_STRING, VMEM_MAGIC_STRING_LEN);
				memcpy(bup_header.dir.filename, bp, 11);
				memcpy(bup_header.dir.comment, bp+0x10, 10);
				memcpy(&bup_header.dir.date, bp+0x1c, 4);
				memcpy(&bup_header.dir.datasize, bp+0x0c, 4);
				memcpy(&bup_header.date, bp+0x1c, 4);
				bup_header.dir.language = bp[0x1b];

				memcpy(save_buf, &bup_header, BUP_HEADER_SIZE);
			}

			write_file(fname, save_buf, fsize+0x40);

			printf("Export Save_%d: %s\n", index, fname);
			return 0;
		}
		index += 1;
	}

	printf("Save_%d not found!\n", save_id);
	return -1;
}


/* Plan a resized replacement without releasing the live save's allocation.
 * Keep its header/directory identity; old data blocks may satisfy growth. */
static int resize_save(int header, u32 new_size)
{
    u8 bitmap[sizeof(MEMS->bitmap)];
    u16 blocks[(1024-64)/2-1];
    u8 *bp;
    u32 old_size;
    int old_count, new_count, count, available, i, block;

    if(header<8 || header>=8064 || new_size>sizeof(blocks)/sizeof(blocks[0])*1024u)
        return -1;
    bp = get_block_addr(header);
    old_size = get_be32(bp+0x0c);
    if(old_size>sizeof(blocks)/sizeof(blocks[0])*1024u)
        return -1;
    old_count = old_size<=960 ? 0 : (old_size+1023)/1024;
    new_count = new_size<=960 ? 0 : (new_size+1023)/1024;
    memcpy(bitmap, MEMS->bitmap, sizeof(bitmap));
    if(!(bitmap[header/8] & (1u<<(header%8))))
        return -1;
    for(i=0; i<old_count; i++){
        block = get_be16(bp+64+i*2);
        if(block<8 || block>=8064 || block==header || !(bitmap[block/8] & (1u<<(block%8))))
            return -1;
        /* Clearing only the private bit also rejects repeated old blocks. */
        bitmap[block/8] &= ~(1u<<(block%8));
    }
    count = available = 0;
    for(block=8; block<8064; block++){
        if(!(bitmap[block/8] & (1u<<(block%8)))){
            if(count<new_count){
                blocks[count++] = block;
                bitmap[block/8] |= 1u<<(block%8);
            }else{
                available++;
            }
        }
    }
    if(count!=new_count)
        return -1;

    /* No admission failure remains. access_data writes the replacement next. */
    memset(bp+64, 0, 1024-64);
    for(i=0; i<new_count; i++)
        put_be16(bp+64+i*2, blocks[i]);
    put_be32(bp+0x0c, new_size);
    memcpy(MEMS->bitmap, bitmap, sizeof(bitmap));
    free_block = available;
    put_be16(&MEMS->free_block, free_block);
    return 0;
}


/* Plan header and data allocation before touching the archive. The bitmap,
 * rather than the advisory free count, determines actual block availability. */
static int create_save(int directory, SAVEINFO *save)
{
    u8 bitmap[sizeof(MEMS->bitmap)];
    u16 blocks[(1024-64)/2]; /* Header plus at most 479 data blocks. */
    u32 size = get_be32(&save->data_size);
    int data_count, needed, count = 0, available = 0, block, i;
    u8 *bp;

    if(directory<0 || directory>=448 || save->file_name[0]==0 ||
       size>(sizeof(blocks)/sizeof(blocks[0])-1)*1024u){
        printf("New save cannot fit the MEMS directory or block layout.\n");
        return -1;
    }
    data_count = size<=960 ? 0 : (size+1023)/1024;
    needed = 1+data_count;
    memcpy(bitmap, MEMS->bitmap, sizeof(bitmap));
    for(block=8; block<8064; block++){
        if(!(bitmap[block/8] & (1u<<(block%8)))){
            if(count<needed){
                blocks[count++] = block;
                bitmap[block/8] |= 1u<<(block%8);
            }else{
                available++;
            }
        }
    }
    if(count!=needed){
        printf("Not enough free MEMS blocks for the complete save.\n");
        return -1;
    }

    /* The cleared header supplies the table terminator. Prepare all payload
     * bytes and allocation bookkeeping before publishing the directory entry. */
    bp = get_block_addr(blocks[0]);
    memset(bp, 0, 1024);
    memcpy(bp, save->file_name, 11);
    memcpy(bp+0x0c, &save->data_size, 4);
    memcpy(bp+0x10, save->comment, 10);
    bp[0x1b] = save->language;
    memcpy(bp+0x1c, &save->date, 4);
    for(i=0; i<data_count; i++)
        put_be16(bp+64+i*2, blocks[i+1]);
    access_data(blocks[0], save->dbuf, 2);
    memcpy(MEMS->bitmap, bitmap, sizeof(bitmap));
    free_block = available;
    put_be16(&MEMS->free_block, free_block);
    bp = mems_buf+1024+directory*16;
    memset(bp, 0, 16);
    memcpy(bp, save->file_name, 11);
    put_be16(bp+14, blocks[0]);
    printf("Import %s.\n", save->file_name);
    return 0;
}


int sr_mems_import(int slot_id, int save_id, char *save_name)
{
	SAVEINFO *save;
	int block, i, index, last;
	u8 *bp;

	index = 0;
	block = 0;
	last = -1;
	for(i=0; i<448; i++){
		bp = (u8*)(mems_buf+1024+i*16);
		if(bp[0]==0){
			if(last==-1){
				last = i;
			}
			continue;
		}
		if(index == save_id){
			block = get_be16(bp+0x0e);
			break;
		}
		index += 1;
	}

	if(block){
		// overwrite
		u8 *sbp;
		char sname[20];
		if(save_name==NULL){
			snprintf(sname, sizeof(sname), "%.11s.bin", bp);
			save_name = sname;
		}
		save = load_saveraw(save_name);
		if(save==NULL)
			return -1;

		// bp points to the 16-byte directory entry, not the save block.
		// The date lives in the save header, so update it there.
        if(block<8 || block>=8064){
            printf("Invalid MEMS save header.\n");
            return -1;
        }
        sbp = get_block_addr(block);
        if(get_be32(sbp+0x0c)!=get_be32(&save->data_size) &&
           resize_save(block, get_be32(&save->data_size))<0){
            printf("Replacement save cannot fit the MEMS block layout.\n");
            return -1;
        }
		*(u32*)(sbp+0x1c) = save->date;
		access_data(block, save->dbuf, 2);
		printf("Import %.11s from %s.\n", bp, save_name);
		return 0;
	}
	if(save_id>=0){
		printf("Save_%d not found!\n", save_id);
		return -1;
	}

    save = load_saveraw(save_name);
    if(save==NULL)
        return -1;
    return create_save(last, save);
}


int sr_mems_delete(int slot_id, int save_id)
{
	int block, i, index, last;
	u8 *bp;

	index = 0;
	block = 0;
	last = -1;
	for(i=0; i<448; i++){
		bp = (u8*)(mems_buf+1024+i*16);
		if(bp[0]==0)
			continue;
		if(index == save_id){
			block = get_be16(bp+0x0e);
			last = i;
			break;
		}
		index += 1;
	}
	if(block==0){
		printf("Save_%d not found!\n", save_id);
		return -1;
	}

	bp = get_block_addr(block);

	// 释放开始块
	set_bitmap((u8*)MEMS->bitmap, block, 0);
	free_block += 1;

	// 释放数据块
	int dsize = get_be32(bp+0x0c);
	if(dsize>960){
		u8 *bmp = bp+0x40;
		block = 0;
		while(1){
			block = get_be16(bmp);
			if(block==0)
				break;
			set_bitmap((u8*)MEMS->bitmap, block, 0);
			free_block += 1;
			bmp += 2;
		}
	}

	// 更新last指针
	memset((u8*)mems_buf+1024+last*16, 0, 16);

	put_be16(&MEMS->free_block, free_block);

	printf("Delete Save_%d.\n", save_id);

	return 0;
}


int sr_mems_create(char *game_id)
{
	return -1;
}


int sr_mems_list(int slot_id)
{
	int block, i, index, dsize, bsize;
	u8 *bp;

	index = 0;
	for(i=0; i<448; i++){
		bp = (u8*)(mems_buf+1024+i*16);
		if(bp[0]==0)
			continue;

		block = get_be16(bp+0x0e);
		bp = get_block_addr(block);

		dsize = get_be32(bp+0x0c);
		bsize = 0;
		if(dsize>960){
			bsize = (dsize+block_size-1)/(block_size);
		}
		bsize += 1;

		printf("  Save_%-3d: %.11s  size=%5x  block=%d\n", index, bp, dsize, bsize);

		index += 1;
	}
	printf("\n");

	printf(" Total block: %4d   Free block: %d\n\n", 8064, free_block);
	return 0;
}


int sr_mems_init(u8 *bbuf)
{
    mems_buf = bbuf;
    if(validate_mems()<0){
        printf("Invalid MEMS directory or block metadata.\n");
        return -1;
    }

	return 0;
}


/******************************************************************************/

