"""Synthetic, sparse FAT32 media for actual firmware/FatFs tests only."""
# Adapted from paps-g's aa74eb3e host fixtures; no SD device operations.
import struct

def make_fat32(path):
    """Small sparse FAT32 fixture with two FATs and a single root cluster."""
    sectors, fat_sectors = 131072, 1024
    boot = bytearray(512)
    boot[:11] = b'\xeb\x58\x90MSDOS5.0'
    struct.pack_into('<HBHBHHBHHHII', boot, 11,
                     512, 1, 32, 2, 0, 0, 0xf8, 0, 32, 16, 0, sectors)
    struct.pack_into('<IHHIHH', boot, 36, fat_sectors, 0, 0, 2, 1, 6)
    boot[64] = 0x80; boot[66] = 0x29
    boot[71:82] = b'SAVE TEST  '
    boot[82:90] = b'FAT32   '; boot[510:512] = b'\x55\xaa'
    info = bytearray(512)
    struct.pack_into('<I', info, 0, 0x41615252)
    struct.pack_into('<III', info, 484, 0x61417272, 0xffffffff, 0xffffffff)
    info[510:512] = b'\x55\xaa'
    with path.open('wb') as f:
        f.truncate(sectors * 512)
        for sector in (0, 6): f.seek(sector * 512); f.write(boot)
        for sector in (1, 7): f.seek(sector * 512); f.write(info)
        for sector in (32, 32 + fat_sectors):
            f.seek(sector * 512); f.write(struct.pack('<III', 0x0ffffff8, 0x0fffffff, 0x0fffffff))
