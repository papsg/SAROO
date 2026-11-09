
#include "main.h"
#include "ff.h"
#include <stdlib.h>
#include "cdc.h"


/******************************************************************************/


u32 adler32(u8 *data, int len) 
{
	u32 a = 1, b = 0;
	int i;

	for(i=0; i<len; i++) {
		a = (a + data[i]) % 65521;
		b = (b + a) % 65521;
	}

	return (b<<16) | a;
}


/******************************************************************************/


// 将gameid分解为id与ver
static char *gameid_split(char *gid)
{
	int len1, len2;
	char *ver, *p;

	len2 = 0;
	ver = strchr(gid, ' ');
	if(ver){
		*ver++ = 0;
		while(*ver==' ') ver++;
		p = strchr(ver, ' ');
		if(p) *p = 0;
		len2 = strlen(ver);
	}
	len1 = strlen(gid);
	if(len1+len2==16 && ver){
		strcpy(gid+len1, ver);
		ver = NULL;
	}

	return ver;
}


int gameid_match(char *gid1, char *gid2)
{
	char id1[24], id2[24];
	char *ver1, *ver2;

	strcpy(id1, gid1);
	strcpy(id2, gid2);

	ver1 = gameid_split(id1);
	ver2 = gameid_split(id2);

	if(strcmp(id1, id2)){
		return 0;
	}

	if(ver1 && ver1){
		if(strcmp(ver1, ver2)){
			return 0;
		}
	}else if(ver1==NULL && ver2==NULL){
	}else{
		return 0;
	}

	return 1;
}


/******************************************************************************/


static int max_depth;
static int *qstack = (int*)0x2400a000;
static int qtop;

#define qs_push(lp, rp)  ( qstack[qtop++] = ((lp)<<16) | (rp))

void qsort4(void *idata, int num, int (*cmp_func)(const void*, const void*))
{
	int *data = (int*)idata;
	int ilp, irp, lp, rp, flag;

	if(num<2)
		return;

	max_depth = 0;
	qtop = 0;
	qs_push(0, num-1);

	while(qtop){
		qtop -= 1;
		ilp = qstack[qtop]>>16;
		irp = qstack[qtop]&0xffff;

		lp = ilp;
		rp = irp;
		flag = -1;
		while(rp > lp){
			if(cmp_func(data+lp, data+rp)>0){
				int temp = data[rp];
				data[rp] = data[lp];
				data[lp] = temp;
				flag = -flag;
			}
			if(flag<0){
				rp -= 1;
			}else{
				lp += 1;
			}
		}
		if(ilp<lp-1) qs_push(ilp, lp-1);
		if(lp+1<irp) qs_push(lp+1, irp);
		if(max_depth<qtop){
			max_depth = qtop;
		}
	}
	//printk("qsort4: max_depth=%d\n", max_depth);
}


/******************************************************************************/


char *get_token(char **str_in)
{
	char *str, *start, match;

	if(str_in==NULL || *str_in==NULL)
		return NULL;
	str = *str_in;

	while(*str==' ' || *str=='\t') str++;

	if(*str=='"'){
		match = '"';
		start = str+1;
		str += 1;
	}else if(*str){
		match = ' ';
		start = str;
	}else{
		return NULL;
	}

	while(*str && *str!=match) str++;
	if(*str==match){
		*str = 0;
		*str_in = str+1;
	}else{
		*str_in = NULL;
	}

	return start;
}


char *get_line(u8 *buf, int *pos, int size)
{
	char *line = (char*)buf+*pos;
	
	if(*pos>=size)
		return NULL;

	while(*pos<size && (buf[*pos]!='\r' && buf[*pos]!='\n')) *pos = (*pos)+1;
	while(*pos<size && (buf[*pos]=='\r' || buf[*pos]=='\n')){
		buf[*pos] = 0;
		*pos = (*pos)+1;
	}

	return line;
}


/******************************************************************************/

static int sscfg_p = 0;

void ss_config_init(void)
{
	sscfg_p = 0;
	*(u32*)(SYSINFO_ADDR+0x0100) = 0;
}

void ss_config_put(u32 val)
{
	*(u32*)(SYSINFO_ADDR+0x0100+sscfg_p) = val;
	sscfg_p += 4;
	*(u32*)(SYSINFO_ADDR+0x0100+sscfg_p) = 0;
}

/******************************************************************************/


int config_exmem(char *lbuf)
{
	if(lbuf[1]!='M')
		return -1;

	if(lbuf[0]=='1'){
		ss_config_put(0x30000001);
	}else if(lbuf[0]=='4'){
		ss_config_put(0x30000004);
	}else{
		return -1;
	}

	printk("    exmem_%s\n", lbuf);
	return 0;
}


int config_wrmem(char *lbuf)
{
	char *p;

	int addr = strtoul(lbuf, &p, 16);

	p = strchr(lbuf, '=');
	if(p==NULL)
		return -1;
	p += 1;
	while(*p==' ') p += 1;

	int width = strlen(p);
	int val = strtoul(p, NULL, 16);
	addr = ((width/2)<<28) | (addr&0x0fffffff);
	ss_config_put(addr);
	ss_config_put(val);
	printk("    M_%08x=%x\n", addr, val);

	return 0;
}

static char catbuf[32];
int category_num = 0;

int config_category(char *lbuf)
{
	char *category = (char*)(SYSINFO_ADDR+0x0e80); // 610a3e80

	if(category_num<12){
		memcpy(category+category_num*32, catbuf, 31);
		category_num += 1;
		*(u8*)(SYSINFO_ADDR+0x0c) = category_num;
	}
	return 0;
}

char *get_category(int id)
{
	return (char*)(SYSINFO_ADDR + 0x0e80 + id*32); // 610a3e80
}


/******************************************************************************/


#define ARG_NON  0
#define ARG_HEX  1
#define ARG_DEC  2
#define ARG_STR  3

#define GA       0x0100


typedef struct {
	char *name;
	int type;
	void *action;
	void *action_ex;
}CFGARG;


CFGARG arg_list [] = {
	{"lang_id",      GA|ARG_DEC, &lang_id,},
	{"debug",        GA|ARG_HEX, &debug_flags,},
	{"log_mask",     GA|ARG_HEX, &log_mask,},
	{"auto_update",  GA|ARG_DEC, &auto_update,},
	{"category",     GA|ARG_STR, &catbuf, config_category},
	{"sector_delay",    ARG_DEC, &sector_delay,},
	{"play_delay",      ARG_DEC, &play_delay,},
	{"pend_delay",      ARG_DEC, &pend_delay,},
	{"exmem_",          ARG_NON, config_exmem,},
	{"M_",              ARG_NON, config_wrmem,},
	{"multi_disc",      ARG_STR, &mdisc_str,},
	{"sort_mode",    GA|ARG_DEC, &sort_mode,},

	{NULL},
};


int parse_config(char *fname, char *gameid)
{
	FIL fp;
	u8 *fbuf = (u8*)0x24002000;
	char *p = NULL;
	int retv;

	ss_config_init();

	retv = f_open(&fp, fname, FA_READ);
	if(retv){
		return -2;
	}

	u32 nread;
	retv = f_read(&fp, fbuf, f_size(&fp), &nread);
	f_close(&fp);
	if(retv){
		return -3;
	}
	fbuf[nread] = 0;

	printk("\nLoad config [%s] for [%s]\n", fname, (gameid==NULL)?"Global":gameid);

	int cpos = 0;
	int g_sec = 0;
	int in_sec = 0;
	char *lbuf;
	while((lbuf=get_line(fbuf, &cpos, nread))!=NULL){
_next_section:
		//查找section: [xxxxxx]
		if(in_sec==0){
			if(lbuf[0]=='['){
				p = strrchr(lbuf, ']');
				if(p==NULL){
					return -4;
				}
				*p = 0;
				if(g_sec==0){
					// 第一个section一定是global
					if(strcmp(lbuf+1, "global")){
						return -5;
					}
					g_sec = 1;
					in_sec = 1;
					printk("Global config:\n");
				}else if(gameid && gameid_match(lbuf+1, gameid)){
					// 找到了与game_id匹配的section
					in_sec = 1;
					printk("Game config:\n");
				}
			}
			continue;
		}else{
			if(lbuf[0]=='['){
				if(g_sec==1){
					g_sec = 2;
					in_sec = 0;
					if(gameid==NULL)
						break;
					goto _next_section;
				}else{
					break;
				}
			}
			CFGARG *arg = arg_list;
			while(arg->name){
				int nlen = strlen(arg->name);
				int type = arg->type;
				int global = (type&GA);
				type &= 0xff;
				if(global && (gameid || g_sec!=1) ){
					arg += 1;
					continue;
				}
				if(strncmp(lbuf, arg->name, nlen)==0){
					if(type==ARG_NON){
						int (*action)(char*) = arg->action;
						retv = action(lbuf+nlen);
					}else if(type==ARG_STR){
						p = strchr(lbuf+nlen, '"');
						if(p){
							char *st = p+1;
							p = strchr(st, '"');
							if(p){
								*p = 0;
								printk("    %s = \"%s\"\n", arg->name, st);
								strcpy((char*)(arg->action), st);
								retv = 0;
							}else{
								retv = -1;
							}
						}else{
							retv = -1;
						}
					}else{
						int base = (type==ARG_DEC)? 10 : 16;
						p = strchr(lbuf+nlen, '=');
						if(p){
							int value = strtoul(p+1, NULL, base);
							if(base==10){
								printk("    %s = %d\n", arg->name, value);
							}else{
								printk("    %s = %08x\n", arg->name, value);
							}
							*(int*)(arg->action) = value;
							retv = 0;
						}else{
							retv = -1;
						}
					}
					if(retv){
						printk("Invalid config line: {%s}\n", lbuf);
						led_event(LEDEV_SCFG_ERROR);
					}else{
						if(arg->action_ex){
							void (*action_ex)(void) = arg->action_ex;
							action_ex();
						}
					}
				}
				arg += 1;
			}

		}
	}
	printk("\n");

	return 0;
}

/******************************************************************************/

#define SSAVE_FILE "/SAROO/SS_SAVE.BIN"
#define SSAVE_ADDR (SAVINFO_ADDR+0x00000)

static FIL ss_fp;
static int ss_index;


#define SMEMS_FILE "/SAROO/SS_MEMS.BIN"
#define SMEMS_HDR  (SAVINFO_ADDR+0x10000)
#define SMEMS_BUF  (SAVINFO_ADDR+0x13000)

static FIL sm_fp;
static int sm_index0;
static int sm_index1;


int open_savefile(void)
{
    u32 rv;
    int retv = 0, mems;
    u8 *fbuf = (u8*)0x24002000;
    ss_index = sm_index0 = sm_index1 = 0;

    for (mems = 0; mems < 2; mems++) {
        FIL *fp = mems ? &sm_fp : &ss_fp;
        const char *name = mems ? SMEMS_FILE : SSAVE_FILE;
        const char *signature = mems ? "SaroMems" : "Saroo Save File";
        u8 *buffer = mems ? (u8*)SMEMS_HDR : fbuf;
        u32 header_size = mems ? 8192 : 65536;
        u32 signature_size = mems ? 8 : 16;

        retv = f_open(fp, name, FA_READ | FA_WRITE);
        if (retv == FR_NO_FILE) {
            /* Create exclusively: errors and existing damaged files must never
             * become permission to truncate an archive. */
            retv = f_open(fp, name, FA_READ | FA_WRITE | FA_CREATE_NEW);
            if (retv) goto failed;
            memset(buffer, 0, header_size);
            memcpy(buffer, signature, signature_size);
            if (mems) {
                /* Canonical big-endian metadata; unused newly allocated payload
                 * bytes are unspecified, not assumed to contain zeros. */
                buffer[9] = 0x80;
                buffer[12] = 0x1f; buffer[13] = 0x78;
                buffer[16] = 0xff;
            }
            retv = f_write(fp, buffer, header_size, &rv);
            if (retv) goto failed;
            if (rv != header_size) { retv = FR_DISK_ERR; goto failed; }
            if (mems) {
                retv = f_lseek(fp, 0x800000);
                if (retv) goto failed;
                if (f_tell(fp) != 0x800000) { retv = FR_DENIED; goto failed; }
            }
            retv = f_sync(fp);
            if (retv) goto failed;
        } else if (retv) goto failed;

        /* An unindexed incomplete slot append is not a malformed whole file.
         * Runtime selection must check each published payload's full extent.
         * MEMS trailers are opaque; the firmware uses only the first 8 MiB. */
        if (mems ? f_size(fp) < 0x800000 :
            (f_size(fp) < 65536 || f_size(fp) > 0x10000000)) {
            retv = FR_INT_ERR; goto failed;
        }
        retv = f_lseek(fp, 0);
        if (retv) goto failed;
        u32 read_size = mems ? 8192 : 64;
        retv = f_read(fp, buffer, read_size, &rv);
        if (retv) goto failed;
        if (rv != read_size || memcmp(buffer, signature, signature_size)) {
            retv = FR_INT_ERR; goto failed;
        }
    }
    return 0;

failed:
    /* Preserve the initial error even if cleanup also fails. Incomplete newly
     * created files stay visible for recovery, never silently truncated. */
    f_close(&ss_fp);
    f_close(&sm_fp);
    return retv;
}

/* Checked transfers share one implementation; no additional I/O buffer. */
static int save_io(FIL *fp, u32 offset, void *buffer, u32 size, int write)
{
    u32 count;
    int result;
    /* Read-side seeks must not extend an archive opened for writing. */
    if (!write && (offset > f_size(fp) || size > f_size(fp) - offset))
        return FR_INT_ERR;
    result = f_lseek(fp, offset);
    if (result)
        return result;
    if (f_tell(fp) != offset)
        return FR_INT_ERR;
    result = write ? f_write(fp, buffer, size, &count) :
                     f_read(fp, buffer, size, &count);
    return result ? result : count == size ? FR_OK : FR_INT_ERR;
}

int load_savefile(char *gameid)
{
    u8 *directory = (u8*)0x24002000;
    char key[16];
    int result, found = 0, empty = 0;

    fs_lock();
    ss_index = 0;
    if (!gameid || !gameid[0]) {
        result = FR_INVALID_PARAMETER;
        goto done;
    }
    /* Both callers provide the fixed 16-byte disc ID. Short C inputs are
     * zero-padded; do not append a seventeenth byte to a directory entry. */
    strncpy(key, gameid, sizeof(key));
    /* An incomplete unindexed tail is allowed; every published extent below
     * must still contain its complete payload before selection. */
    if (f_size(&ss_fp) < 0x10000 || f_size(&ss_fp) > 0x10000000) {
        result = FR_INT_ERR;
        goto done;
    }
    result = save_io(&ss_fp, 0, directory, 0x10000, 0);
    if (result)
        goto done;
    if (memcmp(directory, "Saroo Save File", 16)) {
        result = FR_INT_ERR;
        goto done;
    }
    for (int i = 1; i < 4096; i++) {
        u8 *entry = directory + i * 16;
        if (!entry[0]) {
            if (!empty) empty = i;
            continue;
        }
        if ((u32)(i + 1) * 0x10000 > f_size(&ss_fp)) {
            result = FR_INT_ERR;
            goto done;
        }
        if (!strncmp((char*)entry, key, sizeof(key))) {
            if (found) {
                result = FR_INT_ERR;
                goto done;
            }
            found = i;
        }
    }
    if (found) {
        result = save_io(&ss_fp, found * 0x10000, (u8*)SSAVE_ADDR, 0x10000, 0);
    } else {
        if (!empty) {
            result = FR_DENIED;
            goto done;
        }
        found = empty;
        memset((u8*)SSAVE_ADDR, 0, 0x10000);
        result = save_io(&ss_fp, found * 0x10000, (u8*)SSAVE_ADDR, 0x10000, 1);
        if (!result) result = f_sync(&ss_fp);
        /* A published ID may name only a fully initialized payload. */
        if (!result) result = save_io(&ss_fp, found * 16, key, sizeof(key), 1);
        if (!result) result = f_sync(&ss_fp);
    }
    if (!result) ss_index = found;
done:
    fs_unlock();
    return result;
}

int flush_savefile(void)
{
    int result;
    fs_lock();
    if (ss_index <= 0 || ss_index >= 4096 ||
        (u32)(ss_index + 1) * 0x10000 > f_size(&ss_fp)) {
        result = FR_INVALID_PARAMETER;
    } else {
        result = save_io(&ss_fp, ss_index * 0x10000, (u8*)SSAVE_ADDR, 0x10000, 1);
        if (!result) result = f_sync(&ss_fp);
    }
    fs_unlock();
    return result;
}

int load_smems(int id)
{
    int header = id & 0x8000;
    int *index = header ? &sm_index0 : &sm_index1;
    int result;
    fs_lock();
    /* A partial transfer invalidates the destination's previous ownership. */
    *index = 0;
    if ((id & ~0xffff) || (id & 0x7fff) < 8 || (id & 0x7fff) >= 8064 ||
        f_size(&sm_fp) < 0x800000) {
        result = FR_INVALID_PARAMETER;
    } else {
        id &= 0x7fff;
        result = save_io(&sm_fp, id * 1024,
                         header ? (u8*)SMEMS_HDR + 8192 : (u8*)SMEMS_BUF,
                         header ? 1024 : 65536, 0);
        if (!result) *index = id;
    }
    fs_unlock();
    return result;
}

int flush_smems(int flag)
{
    int result = FR_INVALID_PARAMETER;
    fs_lock();
    if (flag <= 0 || (flag & ~7) || f_size(&sm_fp) < 0x800000 ||
        ((flag & 2) && (sm_index0 < 8 || sm_index0 >= 8064)) ||
        ((flag & 4) && (sm_index1 < 8 || sm_index1 >= 8064)))
        goto done;

    int d = sm_index0 - sm_index1;
    int overlap = sm_index0 >= 8 && sm_index0 < 8064 &&
                  sm_index1 >= 8 && sm_index1 < 8064 && d >= 0 && d < 64;
    if (overlap)
        memcpy32((u8*)SMEMS_BUF + d * 1024, (u8*)SMEMS_HDR + 8192, 1024);

    /* Stop on the first error. Publish allocation/directory metadata last. */
    result = 0;
    if (flag & 4)
        result = save_io(&sm_fp, sm_index1 * 1024, (u8*)SMEMS_BUF, 65536, 1);
    if (!result && (flag & 2) && (!(flag & 4) || !overlap))
        result = save_io(&sm_fp, sm_index0 * 1024, (u8*)SMEMS_HDR + 8192, 1024, 1);
    if (!result && (flag & 1))
        result = save_io(&sm_fp, 0, (u8*)SMEMS_HDR, 8192, 1);
    if (!result) result = f_sync(&sm_fp);
done:
    fs_unlock();
    return result;
}


static FIL cover_fp;
/* 0: closed, 1: open but unavailable, 2: complete header table. */
static int cover_inited;
static int last_chdr;
#define COVER_COUNT 2048
#define COVER_SLOTS ((0x2800 - 4) / 4)
static u16 * const ip_cache = (u16*)0x614e0000;
static u8 * const cover_buf = (u8*)0x614f0000;

/* A cache entry belongs to one published catalog, including its ordering. */
void cover_reset(void)
{
    memset(ip_cache, 0, COVER_SLOTS * sizeof(*ip_cache));
    last_chdr = 0;
}

static int find_cover(char *gid, u32 fsum)
{
    if (strnlen(gid, 13) > 12) return 0xffff;
    for (int i = 0; i < COVER_COUNT; i++) {
        u8 *hdr = cover_buf + i * 32;
        if (!hdr[0]) break;
        if (!strncmp((char*)hdr, gid, 12) && *(u32*)(hdr + 12) == fsum)
            return i + 1;
    }
    return 0xffff;
}

void cover_init(void)
{
    cover_reset();
    if (cover_inited && f_close(&cover_fp)) {
        cover_inited = 1;
        return;
    }
    cover_inited = 0;
    if (f_open(&cover_fp, "/SAROO/cover.bin", FA_READ)) return;
    cover_inited = 1;
    if (!save_io(&cover_fp, 0, cover_buf, COVER_COUNT * 32, 0))
        cover_inited = 2;
}

void load_cover(int index)
{
    int chdr = 0xffff, w = 128, h = 128, flag = 4;
    if (index & 0x8000) last_chdr = 0;
    if ((unsigned)index > 0xffff) goto unavailable;
    index &= 0x7fff;
    if (cover_inited != 2 || total_disc < 0 || total_disc > COVER_SLOTS ||
        index >= total_disc || (unsigned)category_current >= 12 ||
        (unsigned)category_current >= (unsigned)(category_num ? category_num : 1))
        goto unavailable;
    chdr = ip_cache[index];
    if (!chdr) {
        u8 ipbuf[256];
        char gid[17];
        /* Preview already returns a normalized user-data header. A transient
         * read failure must remain retryable, not become a cached absence. */
        if (get_disc_ip(index, ipbuf) < 0) goto unavailable;
        if (memcmp(ipbuf, "SEGA SEGASATURN ", 16)) goto unavailable;
        memcpy(gid, ipbuf + 0x20, 16);
        gid[16] = 0;
        char *p = strrchr(gid, 'V');
        if (p) *p = 0;
        p = strchr(gid, ' ');
        if (p) *p = 0;
        chdr = find_cover(gid, adler32(ipbuf + 0x30, 64));
        ip_cache[index] = chdr;
    }
    if (chdr > COVER_COUNT) goto unavailable;
    if (chdr == last_chdr) return;
    u8 *hdr = cover_buf + (chdr - 1) * 32;
    w = *(u16*)(hdr + 0x14);
    h = *(u16*)(hdr + 0x16);
    /* Fit the existing 512 by 240 display staging allocation. */
    if (!w || w > 512 || !h || h > 240 || *(u32*)(hdr + 0x10) < COVER_COUNT * 32)
        goto unavailable;
    flag = 3;
    goto display;
unavailable:
    chdr = 0xffff;
    if (chdr == last_chdr) return;
    w = h = 128;
display:
    /* The Saturn clears both flags after finishing its copies. Wait before
     * reusing that storage, with a bounded exit if the consumer has stopped. */
    for (unsigned tries = 10; *(volatile u8*)0x61400000 || *(volatile u8*)0x61400002; tries--) {
        if (!tries) return;
        osDelay(1);
    }
    if (flag == 3) {
        u8 *hdr = cover_buf + (chdr - 1) * 32;
        int result = save_io(&cover_fp, *(u32*)(hdr + 0x10), (u8*)0x61400c00, w * h + 0x400, 0);
        if (result) {
            chdr = 0xffff;
            w = h = 128;
            flag = 4;
        } else {
            memcpy((u8*)0x61400100, (u8*)0x61400c00, 0x400);
        }
    }
    if (flag == 4) memset((u8*)0x61401000, 0, 128 * 128);
    *(u16*)0x61400004 = w;
    *(u16*)0x61400006 = h;
    *(u16*)0x61400008 = 0;
    *(u16*)0x6140000a = 0;
    *(u16*)0x6140000c = 176;
    *(u16*)0x6140000e = h > 128 ? 24 : 88;
    last_chdr = chdr;
    __DMB();
    if (flag == 3) *(volatile u8*)0x61400002 = 1;
    *(volatile u8*)0x61400000 = flag;
}


/******************************************************************************/
