#ifndef SAROO_SECTOR_OVERLAY_H
#define SAROO_SECTOR_OVERLAY_H

#include <stddef.h>
#include <stdint.h>

#define SECTOR_OVERLAY_HEADER_BYTES 24u
#define SECTOR_OVERLAY_RECORD_BYTES 348u
#define SECTOR_OVERLAY_MAX_RECORDS 8u
#define SECTOR_OVERLAY_MAX_BYTES (SECTOR_OVERLAY_HEADER_BYTES + \
                                 SECTOR_OVERLAY_MAX_RECORDS * SECTOR_OVERLAY_RECORD_BYTES)
/* Part of the MCU/menu SYSINFO contract. */
#define SD_OVERLAY_REPLACEMENT 0x534f5680u

/* The disc owner stages and checks a pack before arming it for delivery. */
void sector_overlay_clear(void);
uint8_t *sector_overlay_staging(void);
uint32_t sector_overlay_crc32(const void *data, size_t size);
int sector_overlay_validate(size_t size, const char product[10]);
unsigned sector_overlay_count(void);
uint32_t sector_overlay_fad(unsigned index);
int sector_overlay_replaces_callback(void);
int sector_overlay_check(unsigned index, const uint8_t sector[2352]);
void sector_overlay_arm(void);
/* A negative result leaves both the destination and the mounted pack intact. */
int sector_overlay_apply(uint32_t fad, uint8_t sector[2352]);
/* Runs after the new CUE/ISO tracks are open and before disc publication. */
int sector_overlay_mount(const char product[10]);
void sector_overlay_reset_handoff(void);
void sector_overlay_publish_handoff(void);

#endif
