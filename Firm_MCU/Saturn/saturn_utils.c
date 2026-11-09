
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
		/* Source follows destination inside gid; forward copying is safe. */
		for(int i=0; i<=len2; i++)
			gid[len1+i] = ver[i];
		ver = NULL;
	}

	return ver;
}


int gameid_match(char *gid1, char *gid2)
{
	char id1[24], id2[24];
	char *ver1, *ver2;

	/* Reject overlong fields before copying or normalizing either ID. */
	if(strnlen(gid1, sizeof(id1))==sizeof(id1) ||
	   strnlen(gid2, sizeof(id2))==sizeof(id2))
		return 0;
	strcpy(id1, gid1);
	strcpy(id2, gid2);

	ver1 = gameid_split(id1);
	ver2 = gameid_split(id2);

	if(strcmp(id1, id2)){
		return 0;
	}

	if(ver1 && ver2){
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

/* This scratch window ends at the CD sector pool. The caller has stopped
 * its reader (or is that reader during a disc change) before using it. */
#define CONFIG_BYTES 0x8000
#define CONFIG_WORDS ((0x0e80 - 0x0100) / 4)
#define CFG_GLOBAL 0x80
#define CFG_HEX 1
#define CFG_DEC 2
#define CFG_MULTI 16
#define CFG_CATEGORY 32

int category_num;

char *get_category(int id)
{
    return (char*)(SYSINFO_ADDR + 0x0e80 + id * 32);
}

static const struct {
    const char *name;
    void *value;
    unsigned char type;
} config_args[] = {
    {"lang_id", &lang_id, CFG_GLOBAL | CFG_DEC},
    {"debug", &debug_flags, CFG_GLOBAL | CFG_HEX},
    {"log_mask", &log_mask, CFG_GLOBAL | CFG_HEX},
    {"auto_update", &auto_update, CFG_GLOBAL | CFG_DEC},
    {"category", NULL, CFG_GLOBAL | CFG_CATEGORY},
    {"sector_delay", &sector_delay, CFG_DEC},
    {"play_delay", &play_delay, CFG_DEC},
    {"pend_delay", &pend_delay, CFG_DEC},
    {"exmem_", NULL, 0},
    {"M_", NULL, 0},
    {"multi_disc", &mdisc_str, CFG_MULTI},
    {"sort_mode", &sort_mode, CFG_GLOBAL | CFG_DEC},
};

static char *config_space(char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static int config_end(char *p)
{
    p = config_space(p);
    return !*p || *p == '#' || *p == ';';
}

/* Return the number of digits, excluding an optional hexadecimal prefix.
 * Check against 32 bits explicitly, including when tested on a 64-bit host. */
static int config_number(char **text, u32 *value, unsigned base)
{
    char *p = config_space(*text);
    if (*p == '+') p++;
    if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    unsigned count = 0, result = 0;
    for (;;) {
        unsigned digit = (unsigned char)*p - '0';
        if (digit > 9) digit = ((unsigned char)*p | 32) - 'a' + 10;
        if (digit >= base) break;
        if (result > (0xffffffffu - digit) / base) return 0;
        result = result * base + digit;
        count++;
        p++;
    }
    *value = result;
    *text = p;
    return count;
}

/* Read one immutable snapshot twice: first validate every selected operation,
 * then publish. Newlines are already NULs; temporary section delimiters are
 * restored. The second pass cannot encounter new I/O or new file contents. */
static int config_walk(char *buffer, unsigned size, char *gameid, int apply)
{
    unsigned pos = 0, words = 0, cats = 0;
    int section = 0, seen = 0;
    u32 *out = (u32*)(SYSINFO_ADDR + 0x0100);
    while (pos < size) {
        char *line = config_space(buffer + pos), *p;
        unsigned next = pos + strlen(buffer + pos) + 1;
        if (*line == '[') {
            if (section == 2 || (section && !gameid)) break;
            p = strchr(line, ']');
            if (!p || !config_end(p + 1)) goto invalid;
            *p = 0;
            int match = !seen ? !strcmp(line + 1, "global") :
                        gameid && gameid_match(line + 1, gameid);
            *p = ']';
            if (!seen && !match) goto invalid;
            section = match ? (seen ? 2 : 1) : 0;
            seen = 1;
        } else if (section) {
            for (unsigned i = 0; i < sizeof(config_args) / sizeof(config_args[0]); i++) {
                unsigned type = config_args[i].type;
                unsigned len = strlen(config_args[i].name);
                if ((type & CFG_GLOBAL) && (gameid || section != 1)) continue;
                type &= ~CFG_GLOBAL;
                if (strncmp(line, config_args[i].name, len)) continue;
                p = line + len;
                if (type && *p && *p != '=' && *p != ' ' && *p != '\t') continue;
                p = config_space(p);
                u32 value = 0, key = 0;
                unsigned count = 0;
                if (!type) {
                    if (*line == 'M') {
                        if (!config_number(&p, &key, 16)) goto invalid;
                        p = config_space(p);
                        if (*p++ != '=') goto invalid;
                        int digits = config_number(&p, &value, 16);
                        if (digits != 2 && digits != 4 && digits != 8) goto invalid;
                        unsigned width = digits / 2;
                        if (key & (width - 1)) goto invalid;
                        key = (width << 28) | (key & 0x0fffffff);
                        count = 2;
                    } else {
                        if ((*p != '1' && *p != '4') || p[1] != 'M') goto invalid;
                        key = 0x30000000 | (*p - '0');
                        p += 2;
                        count = 1;
                    }
                    if (!config_end(p) || count >= CONFIG_WORDS - words) goto invalid;
                    if (apply) {
                        out[words] = key;
                        if (count == 2) out[words + 1] = value;
                        out[words + count] = 0;
                    }
                    words += count;
                } else {
                    if (*p++ != '=') goto invalid;
                    p = config_space(p);
                    if (type >= CFG_MULTI) {
                        if (*p++ != '"') goto invalid;
                        char *end = strchr(p, '"');
                        if (!end || end - p >= type || !config_end(end + 1)) goto invalid;
                        char *dest = config_args[i].value;
                        if (type == CFG_CATEGORY) {
                            if (cats == 12) goto invalid;
                            dest = get_category(cats++);
                        }
                        if (apply) {
                            memcpy(dest, p, end - p);
                            dest[end - p] = 0;
                        }
                    } else {
                        if (!config_number(&p, &value, type == CFG_DEC ? 10 : 16) ||
                            (type == CFG_DEC && value > 0x7fffffffu) || !config_end(p)) goto invalid;
                        if (apply) *(int*)config_args[i].value = value;
                    }
                }
                break;
            }
        }
        pos = next;
    }
    if (apply && !gameid) {
        category_num = cats;
        *(u8*)(SYSINFO_ADDR + 0x0c) = cats;
    }
    return 0;
invalid:
    return -1;
}

/* Keep the closed FIL's stack frame off the validation/publication path. */
static __attribute__((noinline)) int config_load(char *fname, u8 *buffer)
{
    FIL fp;
    u32 count = 0;
    int result = f_open(&fp, fname, FA_READ);
    if (result) return result == FR_NO_FILE ? 0 : -result;
    FSIZE_t bytes = f_size(&fp);
    result = bytes < CONFIG_BYTES ? f_read(&fp, buffer, bytes, &count) : FR_INVALID_OBJECT;
    int closed = f_close(&fp);
    if (!result) result = closed;
    if (!result && count != bytes) result = FR_DISK_ERR;
    if (result) return -result;
    buffer[count] = 0;
    if (memchr(buffer, 0, count)) return -FR_INVALID_PARAMETER;
    if (count >= 3 && !memcmp(buffer, "\xef\xbb\xbf", 3)) memset(buffer, ' ', 3);
    for (unsigned i = 0; i < count; i++)
        if (buffer[i] == '\r' || buffer[i] == '\n') buffer[i] = 0;
    return count;
}

int parse_config(char *fname, char *gameid)
{
    u8 *buffer = (u8*)0x24002000;
    /* Runtime save transfers also own this scratch window under mutex_fs.
     * Startup precedes creation of that mutex and all Saturn worker threads. */
    if (gameid) fs_lock();
    int count = config_load(fname, buffer), result = count;
    if (count < 0) goto failed;
    if (!count) goto defaults;
    if (config_walk((char*)buffer, count, gameid, 0)) {
        result = -FR_INVALID_PARAMETER;
        goto failed;
    }
defaults:
    /* No settings from a previously loaded game may leak into a new one.
     * Global-only scalar defaults retain their initialized values. */
    sector_delay = play_delay = pend_delay = 0;
    mdisc_str[0] = 0;
    *(u32*)(SYSINFO_ADDR + 0x0100) = 0;
    if (!gameid) {
        category_num = 0;
        *(u8*)(SYSINFO_ADDR + 0x0c) = 0;
    }
    if (count) config_walk((char*)buffer, count, gameid, 1);
    if (gameid) fs_unlock();
    return 0;
failed:
    if (gameid) fs_unlock();
    led_event(LEDEV_SCFG_ERROR);
    return result;
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
