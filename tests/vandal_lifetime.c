/* SPDX-License-Identifier: GPL-2.0-or-later
 * Host-only accepted-delivery/reset comparison by papsg, adapted from the
 * retained standalone frontend. Requires a separately supplied pinned local
 * emulator core; no BIOS/game bytes or device integration are included.
 * retro_reset is this emulator's power-on model, not physical Saturn reset.
 */
/* Read-only Beetle Saturn A/B runner with accepted CDB sector overlays.
 * This standalone frontend never writes the source CUE/BIN or SAROO SD card.
 */
#define _POSIX_C_SOURCE 200809L
#include <libretro.h>
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#include "sector_overlay.h"

#define MAX_RECORDS 12
#define RAW_SIZE 2352
#define RECORD_SIZE 348
#define URI_PREFIX "fixture://disc/"

typedef struct {
    uint32_t fad;
    uint64_t physical_offset;
    bool guard_only;
    uint8_t patched[RAW_SIZE];
    uint64_t read_hits;
} Overlay;
static Overlay overlays[MAX_RECORDS];
static int overlay_count;
static char disc_dir[4096], track1_path[4096], system_dir[4096];
static uint64_t track1_start;
static unsigned current_frame, total_frames, video_frames, audio_samples;
static uint32_t video_crc, audio_crc;
static unsigned video_nonblack, video_width, video_height;
static bool current_video;
static const char *video_snapshot_path;
static unsigned video_snapshot_frame;
static int start_from = -1, start_until = -1, a_from = -1, a_until = -1;
#define MAX_PAD_EVENTS 32
typedef struct { unsigned id, from, until; } PadEvent;
static PadEvent pad_events[MAX_PAD_EVENTS];
static unsigned pad_event_count;
static void die(const char *message);
static void load_pad_script(void) {
    const char *source = getenv("PAD_SCRIPT");
    if (!source || !*source) return;
    char *copy = strdup(source);
    if (!copy) die("cannot allocate pad script");
    for (char *part = strtok(copy, ","); part; part = strtok(NULL, ",")) {
        if (pad_event_count == MAX_PAD_EVENTS) die("too many pad events");
        char name[16];
        unsigned from, until;
        if (sscanf(part, "%15[^@]@%u:%u", name, &from, &until) != 3 ||
            from >= until || until > total_frames) die("invalid pad event");
        static const struct { const char *name; unsigned id; } keys[] = {
            {"start", RETRO_DEVICE_ID_JOYPAD_START},
            {"a", RETRO_DEVICE_ID_JOYPAD_B},
            {"b", RETRO_DEVICE_ID_JOYPAD_A},
            {"c", RETRO_DEVICE_ID_JOYPAD_R},
            {"up", RETRO_DEVICE_ID_JOYPAD_UP},
            {"down", RETRO_DEVICE_ID_JOYPAD_DOWN},
            {"left", RETRO_DEVICE_ID_JOYPAD_LEFT},
            {"right", RETRO_DEVICE_ID_JOYPAD_RIGHT},
        };
        unsigned k = 0;
        while (k < sizeof keys / sizeof keys[0] && strcmp(name, keys[k].name)) ++k;
        if (k == sizeof keys / sizeof keys[0]) die("unsupported pad key");
        pad_events[pad_event_count++] = (PadEvent){keys[k].id, from, until};
    }
    free(copy);
}
static void die(const char *message) { fprintf(stderr, "fatal: %s\n", message); exit(2); }

/* Preflight only; the production C editor runs at accepted CDB delivery. */
static void load_pack(const char *pack_path) {
    if (!pack_path || !strcmp(pack_path, "-")) return;
    FILE *input=fopen(pack_path,"rb");
    if(!input || fseeko(input,0,SEEK_END))die("cannot open/size overlay pack");
    off_t length=ftello(input);
    if(length<24 || length>SECTOR_OVERLAY_MAX_BYTES)die("invalid pack length");
    rewind(input);
    uint8_t *bytes=sector_overlay_staging();
    if(fread(bytes,1,(size_t)length,input)!=(size_t)length || fclose(input))die("short pack read/close");
    if(sector_overlay_validate((size_t)length,(const char*)bytes+8) || bytes[6])die("invalid/alternative pack");
    FILE *source=fopen(track1_path,"rb");
    if(!source)die("cannot open original track");
    unsigned count=sector_overlay_count();
    uint8_t raw[RAW_SIZE];
    for(unsigned i=0;i<count;++i) {
        uint32_t fad=sector_overlay_fad(i);
        uint64_t offset=track1_start+(uint64_t)(fad-150)*RAW_SIZE;
        if(fseeko(source,(off_t)offset,SEEK_SET) || fread(raw,1,RAW_SIZE,source)!=RAW_SIZE)die("short source sector");
        if(sector_overlay_check(i,raw))die("changed preflight source");
        if(!overlay_count || overlays[overlay_count-1].fad!=fad) {
            Overlay *item=&overlays[overlay_count++];
            item->fad=fad;item->physical_offset=offset;
        }
    }
    if(fclose(source))die("source close failed");
    sector_overlay_arm();
}
static int production_sector_delivery(uint32_t fad,uint8_t *sector) {
    uint32_t original_crc=sector_overlay_crc32(sector,RAW_SIZE);
    int result=sector_overlay_apply(fad,sector);
    for(int i=0;i<overlay_count;++i)
        if(overlays[i].fad==fad)++overlays[i].read_hits;
    if(fad==151 || fad==292306 || fad==292308)
        fprintf(stderr,"LIFETIME_DELIVERY,frame=%u,fad=%u,result=%d,before=%08x,after=%08x,action=%u\n",
                current_frame+1,fad,result,original_crc,sector_overlay_crc32(sector,RAW_SIZE),sector_overlay_action());
    return result;
}

typedef struct {
    FILE *file;
    char *virtual_path;
    bool track1;
} Handle;

static bool translate(const char *path, char *physical, size_t capacity) {
    size_t prefix = strlen(URI_PREFIX);
    if (strncmp(path, URI_PREFIX, prefix)) return false;
    const char *relative = path + prefix;
    if (!*relative || *relative == '/' || strchr(relative, '\\'))
        return false;
    for (const char *part = relative; *part;) {
        const char *next = strchr(part, '/');
        size_t length = next ? (size_t)(next - part) : strlen(part);
        if (!length || (length == 2 && part[0] == '.' && part[1] == '.'))
            return false;
        if (!next) break;
        part = next + 1;
    }
    int count = snprintf(physical, capacity, "%s/%s", disc_dir, relative);
    return count > 0 && (size_t)count < capacity;
}

static const char *vfs_get_path(struct retro_vfs_file_handle *stream) {
    return ((Handle *)stream)->virtual_path;
}
static struct retro_vfs_file_handle *vfs_open(const char *path, unsigned mode, unsigned hints) {
    (void)hints;
    char physical[8192];
    if ((mode & RETRO_VFS_FILE_ACCESS_WRITE) || !translate(path, physical, sizeof physical))
        return NULL;
    FILE *file = fopen(physical, "rb");
    if (!file) return NULL;
    Handle *handle = calloc(1, sizeof *handle);
    if (!handle) { fclose(file); return NULL; }
    handle->file = file;
    handle->virtual_path = strdup(path);
    handle->track1 = strcmp(physical, track1_path) == 0;
    if (!handle->virtual_path) { fclose(file); free(handle); return NULL; }
    return (struct retro_vfs_file_handle *)handle;
}
static int vfs_close(struct retro_vfs_file_handle *stream) {
    Handle *handle = (Handle *)stream;
    int result = fclose(handle->file);
    free(handle->virtual_path);
    free(handle);
    return result;
}
static int64_t vfs_size(struct retro_vfs_file_handle *stream) {
    struct stat st;
    return fstat(fileno(((Handle *)stream)->file), &st) ? -1 : st.st_size;
}
static int64_t vfs_tell(struct retro_vfs_file_handle *stream) {
    return ftello(((Handle *)stream)->file);
}
static int64_t vfs_seek(struct retro_vfs_file_handle *stream, int64_t offset, int whence) {
    int origin = whence == RETRO_VFS_SEEK_POSITION_START ? SEEK_SET :
                 whence == RETRO_VFS_SEEK_POSITION_CURRENT ? SEEK_CUR :
                 whence == RETRO_VFS_SEEK_POSITION_END ? SEEK_END : -1;
    if (origin < 0 || fseeko(((Handle *)stream)->file, (off_t)offset, origin)) return -1;
    return ftello(((Handle *)stream)->file);
}
static int64_t vfs_read(struct retro_vfs_file_handle *stream, void *buffer, uint64_t length) {
    Handle *handle = (Handle *)stream;
    off_t from = ftello(handle->file);
    if (from < 0 || length > SIZE_MAX) return -1;
    size_t count = fread(buffer, 1, (size_t)length, handle->file);
    if (!count && ferror(handle->file)) return -1;
    return count;
}
static int64_t vfs_write(struct retro_vfs_file_handle *s, const void *p, uint64_t n) {
    (void)s; (void)p; (void)n; return -1;
}
static int vfs_flush(struct retro_vfs_file_handle *s) { (void)s; return 0; }
static int vfs_remove(const char *p) { (void)p; return -1; }
static int vfs_rename(const char *a, const char *b) { (void)a; (void)b; return -1; }
static int64_t vfs_truncate(struct retro_vfs_file_handle *s, int64_t n) {
    (void)s; (void)n; return -1;
}
static int vfs_stat(const char *path, int32_t *size) {
    char physical[8192]; struct stat st;
    if (!translate(path, physical, sizeof physical) || stat(physical, &st)) return 0;
    if (size) *size = st.st_size > INT32_MAX ? INT32_MAX : (int32_t)st.st_size;
    return RETRO_VFS_STAT_IS_VALID |
           (S_ISDIR(st.st_mode) ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
}
static int vfs_stat64(const char *path, int64_t *size) {
    char physical[8192]; struct stat st;
    if (!translate(path, physical, sizeof physical) || stat(physical, &st)) return 0;
    if (size) *size = st.st_size;
    return RETRO_VFS_STAT_IS_VALID |
           (S_ISDIR(st.st_mode) ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
}
static int vfs_mkdir(const char *p) { (void)p; return -1; }
static struct retro_vfs_dir_handle *vfs_opendir(const char *p, bool h) {
    (void)p; (void)h; return NULL;
}
static bool vfs_readdir(struct retro_vfs_dir_handle *d) { (void)d; return false; }
static const char *vfs_dirent_name(struct retro_vfs_dir_handle *d) { (void)d; return NULL; }
static bool vfs_dirent_is_dir(struct retro_vfs_dir_handle *d) { (void)d; return false; }
static int vfs_closedir(struct retro_vfs_dir_handle *d) { (void)d; return -1; }

static struct retro_vfs_interface vfs = {
    .get_path = vfs_get_path, .open = vfs_open, .close = vfs_close,
    .size = vfs_size, .tell = vfs_tell, .seek = vfs_seek, .read = vfs_read,
    .write = vfs_write, .flush = vfs_flush, .remove = vfs_remove,
    .rename = vfs_rename, .truncate = vfs_truncate, .stat = vfs_stat,
    .mkdir = vfs_mkdir, .opendir = vfs_opendir, .readdir = vfs_readdir,
    .dirent_get_name = vfs_dirent_name, .dirent_is_dir = vfs_dirent_is_dir,
    .closedir = vfs_closedir, .stat_64 = vfs_stat64,
};

static void core_log(enum retro_log_level level, const char *format, ...) {
    if (level < RETRO_LOG_WARN) return;
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
}
static bool environment(unsigned command, void *data) {
    switch (command) {
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
        struct retro_vfs_interface_info *info = data;
        if (info->required_interface_version > 4) return false;
        info->iface = &vfs;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = system_dir; return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = core_log; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = false; return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *variable = data;
        if (!strcmp(variable->key, "beetle_saturn_autortc")) {
            variable->value = "disabled";
            fprintf(stderr, "VAMPIRE_RUN_OPTION,key=beetle_saturn_autortc,value=disabled\n");
            return true;
        }
        const char *region = getenv("SATURN_REGION");
        if (!strcmp(variable->key, "beetle_saturn_region") && region &&
            (!strcmp(region, "jp") || !strcmp(region, "na") || !strcmp(region, "eu"))) {
            variable->value = region;
            fprintf(stderr, "MENU_RUN_OPTION,key=beetle_saturn_region,value=%s\n",region);
            return true;
        }
        const char *cart = getenv("SATURN_CART");
        if (!strcmp(variable->key, "beetle_saturn_cart") && cart &&
            (!strcmp(cart, "none") || !strcmp(cart, "extram1") || !strcmp(cart, "extram4"))) {
            variable->value = cart;
            return true;
        }
        return false;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)data = true; return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        return true;
    default: return false;
    }
}
static void video(const void *data, unsigned width, unsigned height, size_t pitch) {
    video_frames++;
    current_video = data != NULL;
    video_width = width; video_height = height;
    if (data && video_snapshot_path && current_frame + 1 == video_snapshot_frame) {
        FILE *out = fopen(video_snapshot_path, "wb");
        if (!out) die("cannot open video snapshot");
        if (fprintf(out, "P6\n%u %u\n255\n", width, height) < 0)
            die("cannot write video snapshot header");
        for (unsigned y = 0; y < height; ++y) {
            const uint8_t *row = (const uint8_t *)data + y * pitch;
            for (unsigned x = 0; x < width; ++x) {
                const uint8_t *p = row + x * 4;
                uint8_t rgb[3] = {p[2], p[1], p[0]};
                if (fwrite(rgb, 1, 3, out) != 3) die("cannot write video snapshot");
            }
        }
        if (fclose(out)) die("cannot close video snapshot");
    }
    if (!data || (current_frame % 60 && current_frame + 1 != total_frames)) return;
    video_crc = 0; video_nonblack = 0;
    for (unsigned y = 0; y < height; ++y) {
        const uint8_t *row = (const uint8_t *)data + y * pitch;
        video_crc = crc32(video_crc, row, width * 4);
        for (unsigned x = 0; x < width; ++x) {
            const uint8_t *p = row + x * 4;
            video_nonblack += (p[0] | p[1] | p[2]) != 0;
        }
    }
}
static FILE *audio_dump;
static void audio_one(int16_t left, int16_t right) {
    int16_t pair[2] = {left, right};
    if (audio_dump && fwrite(pair, sizeof pair, 1, audio_dump) != 1) die("PCM write failed");
    audio_crc = crc32(audio_crc, (const Bytef *)pair, sizeof pair);
    audio_samples++;
}
static size_t audio_batch(const int16_t *data, size_t frames) {
    if (audio_dump && fwrite(data, 2 * sizeof *data, frames, audio_dump) != frames) die("PCM write failed");
    audio_crc = crc32(audio_crc, (const Bytef *)data,
                      (uInt)(frames * 2 * sizeof *data));
    audio_samples += (unsigned)frames;
    return frames;
}
static void input_poll(void) {}
static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)index;
    if (port || device != RETRO_DEVICE_JOYPAD) return 0;
    for (unsigned n = 0; n < pad_event_count; ++n)
        if (id == pad_events[n].id && current_frame >= pad_events[n].from &&
            current_frame < pad_events[n].until) return 1;
    if (id == RETRO_DEVICE_ID_JOYPAD_START)
        return current_frame >= (unsigned)start_from && current_frame < (unsigned)start_until;
    if (id == RETRO_DEVICE_ID_JOYPAD_A)
        return current_frame >= (unsigned)a_from && current_frame < (unsigned)a_until;
    return 0;
}

static void *symbol(void *library, const char *name) {
    void *p = dlsym(library, name);
    if (!p) { fprintf(stderr, "missing libretro symbol: %s\n", name); exit(2); }
    return p;
}

int main(int argc, char **argv) {
    if (argc != 8) {
        fprintf(stderr, "usage: %s CORE SYSTEM_DIR CUE TRACK1_BIN FRAMES PACK_OR_DASH RESET_FRAME\n", argv[0]);
        return 2;
    }
    const char *cue = argv[3];
    const char *slash = strrchr(cue, '/');
    if (!slash || (size_t)(slash - cue) >= sizeof disc_dir) die("invalid CUE path");
    memcpy(disc_dir, cue, (size_t)(slash - cue));
    disc_dir[slash - cue] = 0;
    if (snprintf(track1_path, sizeof track1_path, "%s", argv[4]) >= (int)sizeof track1_path ||
        snprintf(system_dir, sizeof system_dir, "%s", argv[2]) >= (int)sizeof system_dir)
        die("path too long");
    char *end;
    track1_start = 0;
    end = "";
    if (*end) die("invalid track-one index offset");
    long frames = strtol(argv[5], &end, 10);
    if (*end || frames < 1 || frames > 13000) die("invalid frame count");
    total_frames = (unsigned)frames;
    const char *dump = getenv("AUDIO_DUMP_PATH");
    if (dump && !(audio_dump = fopen(dump, "wb"))) die("cannot open PCM dump");
    load_pad_script();
    video_snapshot_path = getenv("VIDEO_SNAPSHOT_PATH");
    const char *requested_video_frame = getenv("VIDEO_SNAPSHOT_FRAME");
    if (video_snapshot_path) {
        if (!requested_video_frame) die("missing video snapshot frame");
        unsigned long requested = strtoul(requested_video_frame, &end, 10);
        if (*end || requested < 1 || requested > total_frames)
            die("invalid video snapshot frame");
        video_snapshot_frame = (unsigned)requested;
    }
    long reset_frame = strtol(argv[7], &end, 10);
    if (*end || reset_frame < 0 || reset_frame >= frames) die("invalid reset frame");
    load_pack(argv[6]);
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { fprintf(stderr, "%s\n", dlerror()); return 2; }
#define GET(name) __typeof__(name) *name##_fn = symbol(library, #name)
    void (*set_delivery_filter)(int (*)(uint32_t,uint8_t*))=dlsym(library,"SAROO_SetSectorDeliveryFilter");
    if(!set_delivery_filter)die("missing private CDB delivery fixture");
    set_delivery_filter(production_sector_delivery);

    GET(retro_set_environment); GET(retro_set_video_refresh);
    GET(retro_set_audio_sample); GET(retro_set_audio_sample_batch);
    GET(retro_set_input_poll); GET(retro_set_input_state);
    GET(retro_init); GET(retro_deinit); GET(retro_load_game);
    GET(retro_unload_game); GET(retro_run); GET(retro_reset); GET(retro_set_controller_port_device);
    void (*warm_reset_fn)(bool) = NULL;
    if (getenv("WARM_RESET_MODEL")) warm_reset_fn = symbol(library, "SS_Reset");
#undef GET
    retro_set_environment_fn(environment);
    retro_set_video_refresh_fn(video);
    retro_set_audio_sample_fn(audio_one);
    retro_set_audio_sample_batch_fn(audio_batch);
    retro_set_input_poll_fn(input_poll);
    retro_set_input_state_fn(input_state);
    retro_init_fn();
    retro_set_controller_port_device_fn(0, RETRO_DEVICE_JOYPAD);
    char virtual_cue[4096];
    if (snprintf(virtual_cue, sizeof virtual_cue, URI_PREFIX "%s", slash + 1) >= (int)sizeof virtual_cue)
        die("virtual CUE path too long");
    struct retro_game_info game = { .path = virtual_cue };
    if (!retro_load_game_fn(&game)) die("retro_load_game failed");
    fprintf(stdout, "{\"event\":\"loaded\",\"frames_requested\":%ld,\"overlay_records\":%d}\n",
            frames, overlay_count);
    fflush(stdout);
    for (current_frame = 0; current_frame < (unsigned)frames; ++current_frame) {
        if (reset_frame && current_frame == (unsigned)reset_frame) {
            fprintf(stderr,"LIFETIME_RESET,frame=%u,action=%u\n",current_frame,sector_overlay_action());
            if (warm_reset_fn) warm_reset_fn(false);
            else retro_reset_fn();
        }
        current_video = false;
        retro_run_fn();
        if (current_frame % 60 == 0 || current_frame + 1 == (unsigned)frames) {
            uint64_t hits = 0;
            for (int i = 0; i < overlay_count; ++i) hits += overlays[i].read_hits;
            printf("{\"event\":\"frame\",\"frame\":%u,\"video_frames\":%u,"
                   "\"video_present\":%s,\"width\":%u,\"height\":%u,"
                   "\"video_crc32\":\"%08x\",\"nonblack_pixels\":%u,"
                   "\"audio_samples\":%u,\"audio_crc32\":\"%08x\","
                   "\"overlay_read_hits\":%" PRIu64 "}\n",
                   current_frame + 1, video_frames, current_video ? "true" : "false",
                   video_width, video_height, video_crc, video_nonblack,
                   audio_samples, audio_crc, hits);
            fflush(stdout);
        }
    }
    if (audio_dump && fclose(audio_dump)) die("PCM close failed");
    retro_unload_game_fn();
    sector_overlay_clear();
    if(sector_overlay_action())die("stale action after modeled unload");
    puts("{\"event\":\"unloaded\",\"action\":0}");
    retro_deinit_fn();
    dlclose(library);
    return 0;
}
