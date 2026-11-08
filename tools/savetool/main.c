
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <limits.h>
#include <stdint.h>
#include "bup.h"
#include "bup_format.h"


/******************************************************************************/


/* SAROO slots use a fixed 4,096-unit container, including its directory. */
#define MAX_SAROO_BYTES (4096u * 65536u)

u8 *load_file(char *name, int *size)
{
    FILE *fp;
    u8 *buf = NULL;
    long length;

    *size = 0;
    if(name==NULL || (fp=fopen(name, "rb"))==NULL)
        return NULL;
    if(fseek(fp, 0, SEEK_END)!=0 || (length=ftell(fp))<0 ||
       length>INT_MAX || (uintmax_t)length>SIZE_MAX/2 ||
       fseek(fp, 0, SEEK_SET)!=0)
        goto failed;
    /* Keep the existing spare allocation used when appending a SAROO slot. */
    buf = malloc(length ? (size_t)length*2 : 1);
    if(buf==NULL || fread(buf, 1, (size_t)length, fp)!=(size_t)length || ferror(fp))
        goto failed;
    if(fclose(fp)!=0){
        free(buf);
        return NULL;
    }
    *size = (int)length;
    return buf;

failed:
    free(buf);
    fclose(fp);
    return NULL;
}


int write_file(char *file, void *buf, int size)
{
	FILE *fp;
	int written;

	fp = fopen(file, "wb");
	if(fp==NULL)
		return -1;
	written = fwrite(buf, 1, size, fp);
	fclose(fp);

	return written;
}


u32 get_be32(void *p)
{
	u8 *d = (u8*)p;
	return ((u32)d[0]<<24) | ((u32)d[1]<<16) | ((u32)d[2]<<8) | (u32)d[3];

}


void put_be32(void *p, u32 v)
{
	u8 *d = (u8*)p;

	d[0] = v>>24;
	d[1] = v>>16;
	d[2] = v>>8;
	d[3] = v;
}


u32 get_be16(void *p)
{
	u8 *d = (u8*)p;
	return (d[0]<<8) | d[1];

}


void put_be16(void *p, u32 v)
{
	u8 *d = (u8*)p;

	d[0] = v>>8;
	d[1] = v;
}


void set_bitmap(u8 *bmp, int index, int val)
{
	int byte = index/8;
	int mask = 1<<(index&7);

	if(val)
		bmp[byte] |= mask;
	else
		bmp[byte] &= ~mask;
}


/******************************************************************************/

u8 save_buf[0x100000];
static SAVEINFO saveinfo;

/* This is the basic CRC-32 calculation with some optimization but no table lookup. */
u32 crc32b(u8 *data, int size)
{
	int j;
	u32 crc = 0xFFFFFFFF;

	while (size--){
		crc ^= *data++;          // XOR with next byte.
		for (j=0; j<8; j++){     // Do eight times.
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
	}
	return ~crc;
}


static void load_savebup(SAVEINFO *sinfo, const u8 *fbuf, int fsize)
{
	vmem_bup_header_t *bup_header = (vmem_bup_header_t*)fbuf;

	memset(sinfo, 0, sizeof(SAVEINFO));
	memcpy(sinfo->file_name, bup_header->dir.filename, 11);
	memcpy(sinfo->comment, bup_header->dir.comment, 10);
	sinfo->data_size = bup_header->dir.datasize;
	sinfo->date = bup_header->dir.date;
	sinfo->language = bup_header->dir.language;

	memcpy(save_buf, fbuf+BUP_HEADER_SIZE, fsize-BUP_HEADER_SIZE);
	sinfo->dbuf = save_buf;
}


SAVEINFO *load_saveraw(char *save_name)
{
    SAVEINFO *sinfo = &saveinfo;
    u8 *fbuf;
    int fsize, is_bup;
    u32 crc, data_size;

    if(save_name==NULL){
        printf("An input save filename is required.\n");
        return NULL;
    }
    fbuf = load_file(save_name, &fsize);
    if(fbuf==NULL){
        printf("Failed to load file %s!\n", save_name);
        return NULL;
    }
    /* Both transports have complete 64-byte headers and a bounded payload. */
    if(fsize<BUP_HEADER_SIZE || (size_t)(fsize-BUP_HEADER_SIZE)>sizeof(save_buf)){
        printf("%s: Invalid save file length.\n", save_name);
        goto invalid;
    }
    is_bup = memcmp(fbuf, VMEM_MAGIC_STRING, VMEM_MAGIC_STRING_LEN)==0;
    /* SSAVERAW is eight bytes; the following four bytes are unused. */
    if(!is_bup && memcmp(fbuf, "SSAVERAW", 8)!=0){
        printf("%s: Not a supported Saturn save file!\n", save_name);
        goto invalid;
    }
    data_size = get_be32(fbuf+(is_bup ? 0x2c : 0x1c));
    if(data_size!=(u32)(fsize-BUP_HEADER_SIZE)){
        printf("%s: Declared save size does not match its payload.\n", save_name);
        goto invalid;
    }
    if(is_bup){
        load_savebup(sinfo, fbuf, fsize);
    }else{
        crc = get_be32(fbuf+0x0c);
        if(crc && crc!=crc32b(fbuf+0x10, fsize-0x10)){
            printf("%s: CRC32 mismatch; import rejected.\n", save_name);
            goto invalid;
        }
        memset(sinfo, 0, sizeof(SAVEINFO));
        memcpy(sinfo->file_name, fbuf+0x10, 11);
        memcpy(sinfo->comment, fbuf+0x20, 10);
        memcpy(&sinfo->data_size, fbuf+0x1c, 4);
        memcpy(&sinfo->date, fbuf+0x2c, 4);
        sinfo->language = fbuf[0x2b];
        memcpy(save_buf, fbuf+BUP_HEADER_SIZE, data_size);
        sinfo->dbuf = save_buf;
    }
    free(fbuf);
    return sinfo;

invalid:
    free(fbuf);
    return NULL;
}

/******************************************************************************/

// 0: SAROO save file
// 1: Saturn save file
// 2: SAROO extend save

static int bup_type;
static u8 *bup_buf;
static int bup_size;
static char *bup_name = NULL;


void bup_flush(void)
{
	write_file(bup_name, bup_buf, bup_size);
}


int bup_create(char *game_id)
{
	int retv=-1;

	if(bup_type==0){
		retv =  sr_bup_create(game_id);
	}else if(bup_type==1){
		retv =  ss_bup_create(game_id);
	}else if(bup_type==2){
		retv =  sr_mems_create(game_id);
	}
	if(retv>0){
		bup_size += retv;
		bup_flush();
		return 0;
	}

	return retv;
}


int bup_import(int slot_id, int save_id, char *save_name)
{
	int retv=-1;

	if(bup_type==0){
		retv = sr_bup_import(slot_id, save_id, save_name);
	}else if(bup_type==1){
		retv = ss_bup_import(slot_id, save_id, save_name);
	}else if(bup_type==2){
		retv = sr_mems_import(slot_id, save_id, save_name);
	}
	if(retv==0){
		bup_flush();
	}

	return retv;
}


int bup_delete(int slot_id, int save_id)
{
	int retv=-1;

	if(bup_type==0){
		retv = sr_bup_delete(slot_id, save_id);
	}else if(bup_type==1){
		retv = ss_bup_delete(slot_id, save_id);
	}else if(bup_type==2){
		retv = sr_mems_delete(slot_id, save_id);
	}
	if(retv==0){
		bup_flush();
	}

	return retv;
}


int bup_export(int slot_id, int save_id, int exp_type)
{
	if(bup_type==0){
		return sr_bup_export(slot_id, save_id, exp_type);
	}else if(bup_type==1){
		return ss_bup_export(slot_id, save_id, exp_type);
	}else if(bup_type==2){
		return sr_mems_export(slot_id, save_id, exp_type);
	}

	return -1;
}


int bup_list(int slot_id)
{
	if(bup_type==0){
		return sr_bup_list(slot_id);
	}else if(bup_type==1){
		return ss_bup_list(slot_id);
	}else if(bup_type==2){
		return sr_mems_list(slot_id);
	}

	return -1;
}


static int odd_saturn_signature(const u8 *bytes)
{
    const char *signature = "BackUpRam Format";
    int i;
    for(i=0; i<16; i++)
        if(bytes[i*2+1]!=(u8)signature[i])
            return 0;
    return 1;
}

int bup_load(char *bup_name)
{
    bup_buf = load_file(bup_name, &bup_size);
    if(bup_buf==NULL)
        return -1;

    /* Establish each reader's complete input extent before format dispatch.
     * Internal metadata and block ownership are the readers' responsibility. */
    if(bup_size>=0x10000 && (u32)bup_size<=MAX_SAROO_BYTES && bup_size%0x10000==0 &&
       memcmp(bup_buf, "Saroo Save File", sizeof("Saroo Save File"))==0){
        bup_type = 0;
        return sr_bup_init(bup_buf);
    }
    if(bup_size==0x8000 && memcmp(bup_buf, "BackUpRam Format", 16)==0){
        bup_type = 1;
        return ss_bup_init(bup_buf);
    }
    if(bup_size==0x10000 && odd_saturn_signature(bup_buf)){
        bup_type = 1;
        return ss_bup_init(bup_buf);
    }
    if(bup_size>=0x800000 && memcmp(bup_buf, "SaroMems", 8)==0){
        bup_type = 2;
        return sr_mems_init(bup_buf);
    }
    free(bup_buf);
    bup_buf = NULL;
    return -2;
}


/******************************************************************************/


int main(int argc, char *argv[])
{
	int slot_id = -1;
	int save_id = -1;
	int exp_type = 0;
	int create = 0;
	int import = 0;
	int delete = 0;
	char *save_name = NULL;
	char *game_id = NULL;

	int retv;
	int p = 1;


	if(argc==1){
		printf("Usage: sstool  save_file [-c \"gameid\"][-t n] [-s n] [-i [file]]\n");
		printf("    -t n        Select save slot(for SAROO save).\n");
		printf("    -c \"gameid\" New save slot(for SAROO save).\n");
		printf("    -s n        Select and Export(without -i/-d) save.\n");
		printf("    -x n        Select and Export save as .BUP format.\n");
		printf("    -i [file]   Import save.\n");
		printf("    -d          Delete save.\n");
		printf("                List save.\n");
		printf("\n");
		return 0;
	}


	while(p<argc){
		if(argv[p][0]=='-'){
			switch(argv[p][1]){
			case 't':
				if(p+1==argc)
					goto _invalid_params;
				slot_id = atoi(argv[p+1]);
				p += 1;
				break;
			case 'x':
				exp_type = 1;
			case 's':
				if(p+1==argc)
					goto _invalid_params;
				save_id = atoi(argv[p+1]);
				p += 1;
				break;
			case 'c':
				if(p+1==argc)
					goto _invalid_params;
				create = 1;
				game_id = argv[p+1];
				p += 1;
				break;
			case 'i':
				import = 1;
				if(p+1<argc){
					save_name = argv[p+1];
					p += 1;
				}
				break;
			case 'd':
				delete = 1;
				break;
			default:
_invalid_params:
				printf("Invalid params: %s\n", argv[p]);
				return -1;
			}
		}else if(bup_name==NULL){
			bup_name = argv[p];
		}
		p += 1;
	}

	if(bup_name==NULL){
		printf("Need a Save file!\n");
		return -1;
	}
	if(create){
		if(game_id==NULL){
			printf("Need a Game ID!\n");
			return -1;
		}
	}


	retv = bup_load(bup_name);
	if(retv<0){
		printf("Failed to load save file!\n");
		return retv;
	}

	if(create){
		retv = bup_create(game_id);
	}else if(import){
		retv = bup_import(slot_id, save_id, save_name);
	}else if(delete){
		retv = bup_delete(slot_id, save_id);
	}else if(save_id>=0){
		retv = bup_export(slot_id, save_id, exp_type);
	}else{
		retv = bup_list(slot_id);
	}

	return retv;
}


