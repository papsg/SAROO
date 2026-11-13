
#include "main.h"
#include "ff.h"
#include <stdlib.h>
#include "cdc.h"
#include "sector_overlay.h"


/******************************************************************************/

FIL track_fp[100];
SUBINDEX_TABLE index_table;
int current_index;
volatile int mounted_disc = -1;

int sort_mode = 0;

/******************************************************************************/


/* Check path capacity before copying. */
static int join_path(char *out, unsigned capacity, const char *parent, const char *leaf)
{
    unsigned a = strlen(parent), b = strlen(leaf);
    if (a >= capacity || b >= capacity - a - 1)
        return -1;
    if (out != parent) memcpy(out, parent, a);
    out[a] = '/';
    memcpy(out + a + 1, leaf, b + 1);
    return 0;
}

/* All callers receive 256 user-data bytes, independent of sector framing. */
static int read_ip(FIL *fp, FSIZE_t offset, u8 *out)
{
    u32 nread;
    if (f_tell(fp) != offset &&
        (f_lseek(fp, offset) || f_tell(fp) != offset)) return -1;
    if (f_read(fp, out, 256, &nread) || nread != 256) return -1;
    return 0;
}

static int read_track_ip(TRACK_INFO *tk, u8 *out)
{
    unsigned prefix = TRACK_SIZE(tk) == 2352 ? (tk->mode == 2 ? 24 : 16) :
                      TRACK_SIZE(tk) == 2336 ? 8 : 0;
    FSIZE_t size = f_size(tk->fp);
    if (tk->file_offset > size || prefix > size - tk->file_offset) return -1;
    return read_ip(tk->fp, (FSIZE_t)tk->file_offset + prefix, out);
}

/* Zero identifies a Saturn game, one is other media, and negative is an I/O error. */
int get_gameid(char *gameid)
{
    u8 *ip = (u8*)0x24002000;
    if (cdb.tracks[0].mode != 3) {
        if (read_track_ip(&cdb.tracks[0], ip)) return -1;
        if (!memcmp(ip, "SEGA SEGASATURN ", 16)) {
            memcpy(gameid, ip + 0x20, 16);
            gameid[16] = 0;
            return 0;
        }
    }
    gameid[0] = 0;
    return 1;
}


/******************************************************************************/


static int parse_raw(char *fname, int mode, unsigned size)
{
    if (cdb.track_num) return -1;
    FIL *fp = &track_fp[0];
    if (f_open(fp, fname, FA_READ)) return -2;
    /* The sector reader and TRACK_INFO use 32-bit byte offsets. */
    FSIZE_t bytes = f_size(fp);
    if (bytes < size || bytes > 0xffffffffu) { f_close(fp); return -1; }
    TRACK_INFO *tk = &cdb.tracks[0];
    memset(tk, 0, sizeof(*tk));
    tk->fp = fp;
    TRACK_SIZE(tk) = size;
    tk->fad_0 = tk->fad_start = 150;
    tk->fad_end = 149 + (u32)bytes / size;
    tk->mode = mode;
    cdb.track_num = 1;
    init_toc();
    return 0;
}

int load_pcm(char *fname)
{
    return parse_raw(fname, 3, 2352);
}

int parse_iso(char *fname)
{
    return parse_raw(fname, 1, 2048);
}


/* CD TOC entries 0..98 are tracks; slot 99 in track_fp is preview scratch. */
#define CUE_TRACKS MAX_TRACKS
#define CUE_BYTES (MAX_BLOCKS * 2352 - 0x1000)

static int close_tracks(int count);

static char *cue_token(char **line)
{
    char *s = *line, *end;
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return NULL;
    if (*s == '"') {
        end = ++s;
        while (*end && *end != '"') end++;
        if (!*end) { *line = NULL; return NULL; }
        *end++ = 0;
        if (*end && *end != ' ' && *end != '\t') return NULL;
    } else {
        end = s;
        while (*end && *end != ' ' && *end != '\t') end++;
        if (*end) *end++ = 0;
    }
    *line = end;
    return s;
}

/* All callers supply limits below 2^24, so each decimal step fits u32. */
static int cue_number(char **text, char delimiter, unsigned limit)
{
    char *s = *text;
    unsigned value = 0;
    if (!s || *s < '0' || *s > '9') return -1;
    do {
        value = value * 10 + *s++ - '0';
        if (value > limit) return -1;
    } while (*s >= '0' && *s <= '9');
    if (*s != delimiter) return -1;
    *text = s + !!delimiter;
    return value;
}

enum { CUE_MAX_MINUTES = 3728 };
_Static_assert(CUE_MAX_MINUTES < (1u << 25) / 4500,
               "CUE indexed-sector endpoint must fit below 2^25");

static int cue_time(char *text)
{
    int fad = 0;
    for (int i = 0; i < 3; i++) {
        int value = cue_number(&text, i < 2 ? ':' : 0, i == 0 ? CUE_MAX_MINUTES : i == 1 ? 59 : 74);
        if (value < 0) return -1;
        fad = fad * (i == 1 ? 60 : 75) + value;
    }
    return fad;
}

static u32 cue_last_index(const TRACK_INFO *tk)
{
    return tk->max_index > 1 ? TRACK_INDEX_VALUE(tk, tk->max_index-2) : tk->fad_start;
}

/* Finish stored sectors before adding unbacked postgap frames. */
static int cue_end(TRACK_INFO *tk, u32 frames)
{
    if (!tk->max_index || !frames || frames > 0xffffffu - tk->fad_start ||
        tk->postgap > 0xffffffu - tk->fad_start - frames) return -1;
    u32 end = tk->fad_start + frames - 1;
    if (cue_last_index(tk) > end) return -1;
    tk->fad_end = end + tk->postgap;
    return 0;
}

static int cue_finish(TRACK_INFO *tk)
{
    FSIZE_t bytes = f_size(tk->fp);
    if (bytes > 0xffffffffu || tk->file_offset >= bytes) return -1;
    return cue_end(tk, ((u32)bytes - tk->file_offset) / TRACK_SIZE(tk));
}

/* A FILE-relative frame delta uses the preceding/current track width, never
 * the new track width multiplied by the complete FILE timeline. */
static int cue_offset(TRACK_INFO *tk, u32 frames, u32 *offset)
{
    FSIZE_t bytes = f_size(tk->fp);
    if (bytes > 0xffffffffu || tk->file_offset > bytes ||
        frames > ((u32)bytes - tk->file_offset) / TRACK_SIZE(tk)) return -1;
    *offset = tk->file_offset + frames * TRACK_SIZE(tk);
    return 0;
}

int parse_cue(char *fname, u8 *ipbuf)
{
    /* The CUE reader closes before this storage holds preview metadata. */
    union { FIL cue; TRACK_INFO preview; } local;
    u8 *fbuf = (u8*)0x2400b000;
    char *dir_name = (char*)0x2400a000;
    char *full_path = (char*)0x2400a200;
    int retv, count = 0, first = 0, track_relative = -1, file_index1 = 0, gaps = 0;
    u32 next_fad = 150;
    FIL *tk_fp = NULL;
    TRACK_INFO *tk = NULL;
    u32 nread;

    if ((!ipbuf && cdb.track_num) || strlen(fname) >= 512) return -1;
    strcpy(dir_name, fname);
    char *slash = strrchr(dir_name, '/');
    if (slash) *slash = 0;
    else strcpy(dir_name, ".");
    retv = f_open(&local.cue, fname, FA_READ);
    if (retv) return -retv;
    FSIZE_t bytes = f_size(&local.cue);
    retv = bytes && bytes < CUE_BYTES ? f_read(&local.cue, fbuf, bytes, &nread) : FR_INVALID_OBJECT;
    int closed = f_close(&local.cue);
    if (retv || closed || nread != bytes) return -1;
    fbuf[nread] = 0;
    if (strnlen((const char *)fbuf, nread) != nread) return -1;
    int cpos = nread >= 3 && !memcmp(fbuf, "\xef\xbb\xbf", 3) ? 3 : 0;
    char *line;
    if (!ipbuf) current_index = 0;
    retv = -1;
    full_path[0] = 0;
    while ((line = get_line(fbuf, &cpos, nread)) != NULL) {
        char *token = cue_token(&line);
        if (!token) continue;
        if (!strcmp(token, "FILE")) {
            char *file = cue_token(&line), *type = cue_token(&line);
            if ((full_path[0] && !tk_fp) || !file || !*file || !type || strcmp(type, "BINARY") ||
                join_path(full_path, 512, dir_name, file)) goto failed;
            if (tk_fp) {
                if (cue_finish(tk)) goto failed;
                next_fad = tk->fad_end + 1;
            }
            first = count;
            tk_fp = NULL;
        } else if (!strcmp(token, "TRACK")) {
            char *number = cue_token(&line), *mode = cue_token(&line);
            if (!full_path[0] || !mode || count == CUE_TRACKS ||
                cue_number(&number, 0, CUE_TRACKS) != count + 1 ||
                (count > first && !tk->max_index)) goto failed;
            int kind, size;
            if (!strcmp(mode, "AUDIO")) { kind = 3; size = 2352; }
            else {
                if (strncmp(mode, "MODE", 4) ||
                    (mode[4] != '1' && mode[4] != '2') || mode[5] != '/') goto failed;
                kind = mode[4] - '0';
                mode += 6;
                size = cue_number(&mode, 0, 2352);
                if (size != 2048 && size != 2352 && !(kind == 2 && (size == 2336 || size == 2324))) goto failed;
            }
            if (!tk_fp) {
                tk_fp = &track_fp[ipbuf ? 99 : count];
                int opened = f_open(tk_fp, full_path, FA_READ);
                if (opened) { retv = -opened; goto failed; }
            }
            tk = ipbuf ? &local.preview : &cdb.tracks[count];
            count++;
            memset(tk, 0, sizeof(*tk));
            tk->fp = tk_fp;
            TRACK_SIZE(tk) = size;
            tk->mode = kind;
            track_relative = -1;
            gaps = 0;
        } else if (!strcmp(token, "PREGAP") || !strcmp(token, "POSTGAP")) {
            int post = token[1] == 'O';
            int length = cue_time(cue_token(&line));
            if (!tk_fp || count == first || length < 0 || length > 0xffffff || (gaps & (post ? 2 : 1)) ||
                (post ? !tk->max_index : track_relative >= 0)) goto failed;
            if (post) tk->postgap = length;
            else tk->pregap = length;
            gaps |= post ? 2 : 1;
        } else if (!strcmp(token, "INDEX")) {
            char *number = cue_token(&line), *time = cue_token(&line);
            int index = cue_number(&number, 0, 99);
            int relative = cue_time(time);
            if (!tk_fp || count == first || index < 0 || relative < 0 || (gaps & 2)) goto failed;
            if (track_relative < 0) {
                if (index > 1) goto failed;
                u32 offset;
                if (count > first + 1) {
                    TRACK_INFO *previous = tk - 1;
                    if (relative <= file_index1 ||
                        cue_offset(previous, relative - file_index1, &offset) ||
                        cue_end(previous, relative - file_index1)) goto failed;
                    next_fad = previous->fad_end + 1;
                } else if (cue_offset(tk, relative, &offset)) goto failed;
                tk->file_offset = offset;
                tk->fad_0 = next_fad;
                if (tk->pregap > 0xffffffu - next_fad) goto failed;
                tk->fad_start = next_fad + tk->pregap;
                track_relative = relative;
            } else if (index == 0) goto failed;
            if (relative < track_relative) goto failed;
            u32 delta = relative - track_relative, offset;
            /* Validate the complete indexed sector in the shared byte check.
             * cue_time bounds delta below 2^25, so delta+1 cannot wrap. */
            if (cue_offset(tk, delta + 1, &offset)) goto failed;
            offset -= TRACK_SIZE(tk);
            if (index == 1) {
                if (tk->max_index || delta > 0xffffffu - tk->fad_start) goto failed;
                tk->fad_start += delta;
                /* First INDEX01 is LBA0/FAD150, retaining its FILE offset.
                 * Stored prefix frames before FAD0 are not exposed. */
                if (count == 1) {
                    u32 stored = delta < 150 ? 150 - delta : 0;
                    tk->fad_0 = tk->pregap < stored ? stored - tk->pregap : 0;
                    tk->pregap = stored - tk->fad_0;
                    tk->fad_start = 150;
                }
                tk->file_offset = offset;
                file_index1 = track_relative = relative;
                tk->index_base = ipbuf ? 0 : current_index + 1;
                tk->max_index = 1;
                if (ipbuf) {
                    retv = read_track_ip(tk, ipbuf);
                    closed = f_close(tk_fp);
                    return retv || closed ? -1 : 0;
                }
            } else if (index > 1) {
                if (index != tk->max_index + 1 || !tk->max_index ||
                    current_index == 2048 || delta > 0xffffffu - tk->fad_start) goto failed;
                u32 fad = tk->fad_start + delta;
                if (fad <= cue_last_index(tk)) goto failed;
                subindex_store(current_index++, fad);
                tk->max_index = index;
            }
        }
    }
    if (!tk_fp || count == first || !tk || cue_finish(tk)) goto failed;
    cdb.track_num = count;
    init_toc();
    return 0;
failed:
    if (!ipbuf) {
        close_tracks(count);
        current_index = 0;
    } else if (count && tk_fp) f_close(tk_fp);
    return retv;
}


/******************************************************************************/

int category_current = 0;
int total_disc;
static int *disc_path = (int *)(IMGINFO_ADDR+4);
static char *path_str = (char*)(IMGINFO_ADDR);
static int pbptr;


int path_cmp(const void *a, const void *b)
{
    const char *left = path_str + *(const u32*)a;
    const char *right = path_str + *(const u32*)b;
    return sort_mode == 1 ? strcmp(left, right) : strcmp(right, left);
}

/* The shared request path occupies the existing 240-byte name field. */
static int bin_path_valid(const char *path)
{
    const unsigned root = sizeof(BIN_ROOT) - 1;
    unsigned length = 0;
    while (length < BIN_PATH_CAPACITY && path[length]) ++length;
    if (length < root || length == BIN_PATH_CAPACITY ||
        memcmp(path, BIN_ROOT, root)) return 0;
    if (length == root) return 1;
    if (path[root] != '/') return 0;
    for (unsigned i = root + 1; i <= length; ++i) {
        unsigned c = (unsigned char)path[i];
        if (path[i - 1] == '/' && (!c || c == '.' || c == '/')) return 0;
        if (c && (c < ' ' || strchr("\\:*?\"<>|\x7f", c))) return 0;
        if ((!c || c == '/') && (path[i - 1] == '.' || path[i - 1] == ' ')) return 0;
    }
    return 1;
}

enum catalog_kind { CATALOG_BIN_FILES, CATALOG_GAME_DIRS, CATALOG_BIN_FOLDER };

static int list_entries(enum catalog_kind kind, const char *name, int show)
{
    DIR dir;
    FILINFO *info;
    int result;

    /* Publish only complete scans; every live offset/name is rewritten. */
    cover_reset();
    total_disc = 0;
    pbptr = 0x2800;
    *(u32*)IMGINFO_ADDR = 0;
    if (!name || (kind == CATALOG_BIN_FOLDER && !bin_path_valid(name))) return -FR_INVALID_NAME;
    result = f_opendir(&dir, name);
    if (result) return -result;
    info = malloc(sizeof(*info));
    if (!info) {
        result = FR_NOT_ENOUGH_CORE;
    } else {
        while (!(result = f_readdir(&dir, info)) && info->fname[0]) {
            int directory = !!(info->fattrib & AM_DIR);
            if (info->fname[0] == '.' ||
                (kind != CATALOG_BIN_FOLDER && directory != (kind == CATALOG_GAME_DIRS)))
                continue;
            unsigned bytes = strlen(info->fname) + 1;
            if (total_disc >= (0x2800 - 4) / 4 || bytes > 0x23000u - pbptr) {
                result = FR_NOT_ENOUGH_CORE;
                break;
            }
            u32 entry = pbptr;
            if (kind == CATALOG_BIN_FOLDER && directory) entry |= BIN_DIRECTORY_ENTRY;
            ((u32 *)disc_path)[total_disc++] = entry;
            memcpy(path_str + pbptr, info->fname, bytes);
            pbptr += bytes;
        }
        free(info);
    }
    int closed = f_closedir(&dir);
    if (!result) result = closed;
    if (result) {
        total_disc = 0;
        return -result;
    }
    if (kind == CATALOG_GAME_DIRS && sort_mode) qsort4(disc_path, total_disc, path_cmp);
    *(u32*)IMGINFO_ADDR = total_disc;
    if (kind != CATALOG_BIN_FOLDER) printk("Total discs: %d\n", total_disc);
    if (show) {
        for (int i = 0; i < total_disc; i++)
            printk(" %2d:  %s\n", i, path_str + disc_path[i]);
        printk("\n");
    }
    return 0;
}

int list_bins(int show)
{
    return list_entries(CATALOG_BIN_FILES, BIN_ROOT, show);
}

int list_bin_dir(const char *path)
{
    /* Share the volume with the disc worker on non-reentrant FatFs. */
    fs_lock();
    int result = list_entries(CATALOG_BIN_FOLDER, path, 0);
    fs_unlock();
    return result;
}

int list_disc(int cid, int show)
{
    char category_path[48] = "/SAROO/ISO";
    const char *name = category_path;
    if (category_num) {
        if (cid < 0 || cid >= category_num ||
            strnlen(get_category(cid), 32) == 32 ||
            join_path(category_path, sizeof(category_path), category_path, get_category(cid)))
            name = NULL;
    } else cid = 0;
    if (name) category_current = cid;
    return list_entries(CATALOG_GAME_DIRS, name, show);
}


int find_disc(char *name)
{
	int i;

	for(i=0; i<total_disc; i++){
		u32 entry = (u32)disc_path[i];
		if (entry && !(entry & BIN_DIRECTORY_ENTRY) && !strcmp(name, path_str + entry)) {
			return i;
		}
	}
	
	return -1;
}


static int close_tracks(int count)
{
    FIL *last = NULL;
    int result = 0;
    for (int i = 0; i < count; i++) {
        TRACK_INFO *tk = &cdb.tracks[i];
        if (tk->fp && tk->fp != last) {
            int closed = f_close(tk->fp);
            if (!result) result = closed;
            last = tk->fp;
        }
        memset(tk, 0, sizeof(*tk));
    }
    return -result;
}

int unload_disc(void)
{
    sector_overlay_clear();
    invalidate_sector_cache();
    /* Extents and file IDs belong to the retired image, even on close error. */
    invalidate_directory();
    cdb.root_lba = cdb.root_size = cdb.cdir_lba = cdb.cdir_size = 0;
    mounted_disc = next_disc = -1;
    cdb.status = STAT_NODISC;
    set_status(cdb.status);
    int result = -close_tracks(cdb.track_num);
    if (cdb.has_subrw) {
        int closed = f_close(&cdb.subcode_fp);
        if (!result) result = closed;
    }
    cdb.track_num = cdb.has_subrw = current_index = 0;
    cdb.play_track = NULL;
    return -result;
}


/* The recognized suffixes are ASCII; filenames themselves remain UTF-8. */
static int image_suffix(const char *name, const char *suffix)
{
    while (*suffix) {
        unsigned char c = *name++;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != (unsigned char)*suffix++) return 0;
    }
    return *name == 0;
}

int find_cue_iso(char *dirname, char *outname)
{
    DIR dir;
    FILINFO *info;
    int type = 0, result;
    outname[0] = 0;
    result = f_opendir(&dir, dirname);
    if (result) return -result;
    info = malloc(sizeof(*info));
    if (!info) {
        result = FR_NOT_ENOUGH_CORE;
    } else {
        while (!(result = f_readdir(&dir, info)) && info->fname[0]) {
            if (info->fname[0] == '.' || (info->fattrib & AM_DIR)) continue;
            const char *extension = strrchr(info->fname, '.');
            if (!extension) continue;
            int cue = image_suffix(extension, ".cue");
            if (!cue && !image_suffix(extension, ".iso")) continue;
            if (join_path(outname, 256, dirname, info->fname)) {
                result = FR_INVALID_NAME;
                break;
            }
            type = cue ? 1 : 2;
            if (cue) break;
        }
        free(info);
    }
    int closed = f_closedir(&dir);
    if (!result) result = closed;
    if (result) { outname[0] = 0; return -result; }
    return type;
}


static int game_dir(int index, char *out)
{
    if (index < 0 || index >= total_disc) return -1;
    u32 entry = (u32)disc_path[index];
    if (!entry || (entry & BIN_DIRECTORY_ENTRY)) return -1;
    const char *base = "/SAROO/ISO";
    if (category_num) {
        if (category_current < 0 || category_current >= category_num ||
            strnlen(get_category(category_current), 32) == 32 ||
            join_path(out, 256, base, get_category(category_current))) return -1;
        base = out;
    }
    return join_path(out, 256, base, path_str + entry);
}

int load_disc(int index, int start_save)
{
    static unsigned overlay_session;
    unsigned overlay_requested = overlay_session;
    int open_disc = start_save;
    if (start_save) {
        overlay_requested = index & SD_OVERLAY_REQUEST;
        index &= ~SD_OVERLAY_REQUEST;
    }
	int retv;
	char *fname;
	char cat_dir[256];

    if (game_dir(index, cat_dir)) return -1;

	fname = malloc(256);
    if (!fname) return FR_NOT_ENOUGH_CORE;
	retv = find_cue_iso(cat_dir, fname);
	if(retv<=0){
		if(retv==0)
			retv = -2;
		goto _exit;
	}

    if (start_save && pause_disc()) { retv = -1; goto _exit; }
	int unloaded = unload_disc();
	if(unloaded){ retv = unloaded; goto _exit; }

	printk("Load disc: {%s}\n", fname);
	if(retv==1){
		retv = parse_cue(fname, NULL);
	}else{
		retv = parse_iso(fname);
	}
	if(retv){
		printk("  retv=%d\n", retv);
	}else{
		char gameid[20];
		retv = get_gameid(gameid);
		if(retv < 0){
			unload_disc();
			goto _exit;
		}
		if (retv) start_save = 0;
		
		char *p = strrchr(fname, '.');
		p[1] = 's';
		p[2] = 'u';
		p[3] = 'b';
		retv = f_open(&cdb.subcode_fp, fname, FA_READ);
		if(retv==FR_OK){
			cdb.has_subrw = 1;
		}else{
			p[1] = 'c';
			p[2] = 'd';
			p[3] = 'g';
			retv = f_open(&cdb.subcode_fp, fname, FA_READ);
			if(retv==FR_OK){
				cdb.has_subrw = 2;
			}
		}
		if(retv==FR_OK){
			printk("Load %s\n", fname);
		}else{
			cdb.has_subrw = 0;
			retv = 0;
		}

		next_disc = -1;

		retv = parse_config("/SAROO/saroocfg.txt", gameid);
		if (retv) { unload_disc(); goto _exit; }
        if (overlay_requested && gameid[0] && sector_overlay_mount(gameid)) {
            retv = -1;
            unload_disc();
            goto _exit;
        }
		
		if(mdisc_str[0]){
			int mlen = strlen(mdisc_str);
			strcpy(fname, path_str+disc_path[index]);
			char *p = strstr(fname, mdisc_str);
			if(p){
				p[mlen] += 1;
				next_disc = find_disc(fname);
				if(next_disc<0){
					p[mlen] = '1';
					next_disc = find_disc(fname);
				}
				printk("next disc: [%d]  %s\n", next_disc, fname);
			}
		}

		if(sector_delay_force>=0){
			sector_delay = sector_delay_force;
			printk("    force sector_delay = %d\n", sector_delay);
		}
		if(play_delay_force>=0){
			play_delay = play_delay_force;
			printk("    force play_delay = %d\n", play_delay);
		}
		if(pend_delay_force>=0){
			pend_delay = pend_delay_force;
			printk("    force pend_delay = %d\n", pend_delay);
		}

        /* Automatic changes retain the running game's save session. */
        if (open_disc) overlay_session = overlay_requested;
        mounted_disc = index;
        if (disc_removed) {
            cdb.status = STAT_PAUSE;
            set_status(cdb.status);
        }
        disc_removed = 0;
        if (start_save) retv = load_savefile(gameid);
	}

_exit:
	free(fname);
	return retv;
}


int get_disc_ip(int index, u8 *ipbuf)
{
	int retv;
	char *fname;
	char cat_dir[256];

    if (game_dir(index, cat_dir)) return -1;

	fname = malloc(256);
    if (!fname) return -FR_NOT_ENOUGH_CORE;
	retv = find_cue_iso(cat_dir, fname);
	if(retv<=0){
		if(retv==0)
			retv = -2;
		goto _exit;
	}else if(retv==1){
		retv = parse_cue(fname, ipbuf);
	}else{
		FIL *fp = &track_fp[99];
		retv = f_open(fp, fname, FA_READ);
		if(retv){ retv = -retv; goto _exit; }

		retv = read_ip(fp, 0, ipbuf);
		int closed = f_close(fp);
		if (closed) retv = -closed;
	}

_exit:
	free(fname);
	return retv;
}
