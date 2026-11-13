/* Adapted from paps-g's guarded SD sector overlay mount. */
#include "main.h"
#include "ff.h"
#include "cdc.h"
#include "sector_overlay.h"

static void overlay_path(char path[28], const char product[10])
{
    static const char digits[] = "0123456789ABCDEF";
    uint32_t key = sector_overlay_crc32(product, 10);
    memcpy(path, "/SAROO/PATCHES/", 15);
    for (unsigned i = 0; i < 8; ++i)
        path[15 + i] = digits[(key >> (28 - 4 * i)) & 15];
    memcpy(path + 23, ".SOV", 5);
}

/* The old reader is stopped. The lock also owns the shared sector workspace
 * across both f_read and its preimage check. */
int sector_overlay_mount(const char product[10])
{
    char path[28];
    overlay_path(path, product);
    uint8_t *pack = sector_overlay_staging();
    FIL fp;
    UINT got = 0;
    int result = 0;

    fs_lock();
    FRESULT opened = f_open(&fp, path, FA_READ);
    if (opened != FR_OK) {
        result = opened == FR_NO_FILE || opened == FR_NO_PATH ? 0 : -1;
        goto done;
    }
    FSIZE_t length = f_size(&fp);
    FRESULT read = FR_OK;
    if (length >= SECTOR_OVERLAY_HEADER_BYTES && length <= SECTOR_OVERLAY_MAX_BYTES)
        read = f_read(&fp, pack, (UINT)length, &got);
    FRESULT closed = f_close(&fp);
    if (read || closed) { result = -1; goto done; }
    if (length < SECTOR_OVERLAY_HEADER_BYTES || length > SECTOR_OVERLAY_MAX_BYTES)
        goto done;
    if (got != length) { result = -1; goto done; }
    if (sector_overlay_validate(length, product)) goto done;

    uint8_t *scratch = (uint8_t *)0x24002000;
    unsigned track_index = 0;
    for (unsigned i = 0; i < sector_overlay_count(); ++i) {
        uint32_t fad = sector_overlay_fad(i);
        while (track_index < (unsigned)cdb.track_num &&
               fad > cdb.tracks[track_index].fad_end)
            ++track_index;
        if (track_index == (unsigned)cdb.track_num) goto rejected;
        const TRACK_INFO *track = &cdb.tracks[track_index];
        if (track->mode != 1 || TRACK_SIZE(track) != 2352 || fad < track->fad_start ||
            track->postgap > track->fad_end || fad > track->fad_end - track->postgap)
            goto rejected;

        FSIZE_t extent = f_size(track->fp);
        uint32_t delta = fad - track->fad_start;
        if (track->file_offset > extent || delta >= (extent - track->file_offset) / 2352)
            goto rejected;
        uint32_t offset = track->file_offset + delta * 2352;
        if (f_lseek(track->fp, offset) || f_tell(track->fp) != offset ||
            f_read(track->fp, scratch, 2352, &got) || got != 2352) {
            result = -1;
            goto rejected;
        }
        if (sector_overlay_check(i, scratch)) goto rejected;
    }
    sector_overlay_arm();
    goto done;

rejected:
    sector_overlay_clear();
done:
    fs_unlock();
    return result;
}
