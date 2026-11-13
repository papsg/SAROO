/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "main.h"


/* Definitions of physical drive number for each drive */
#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */

int sd_read_sector(u32 block, int count, u8 *buf);
int sd_write_sector(u32 block, int count, u8 *buf);
int sd_sync(void);

static osSemaphoreId_t sem_diskio;

/* Exclude in-flight disk transfers from reidentification. */
static int disk_recover(int force)
{
    if (!force && sdio_ready())
        return 0;
    if (!sem_diskio || osSemaphoreAcquire(sem_diskio, 100) != osOK)
        return -1;
    int result = force || !sdio_ready() ? sdio_reset() : 0;
    osSemaphoreRelease(sem_diskio);
    return result;
}

int disk_reidentify(void)
{
    return disk_recover(1);
}

int disk_resume(void)
{
    return disk_recover(0);
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (BYTE pdrv)
{
	return sem_diskio ? 0 : STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (BYTE pdrv)
{
	if (!sem_diskio)
		sem_diskio = osSemaphoreNew(1, 1, NULL);
	return sem_diskio ? 0 : STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

/* The drive number occupies one byte, leaving a bit for direction in the
 * same argument. Keep the FatFs entry points and one owner for all retries. */
#define DISK_TRANSFER_WRITE 0x80000000u
static __attribute__((noinline)) DRESULT disk_transfer(unsigned operation, BYTE *buff,
                                                     LBA_t sector, UINT count)
{
    if (osSemaphoreAcquire(sem_diskio, 100) != osOK)
        return RES_ERROR;
#if FF_FS_READONLY == 0
    int writing = (operation & DISK_TRANSFER_WRITE) != 0;
#else
    int writing = 0;
#endif
    int result, retry = 4;
    do {
#if FF_FS_READONLY == 0
        if (writing)
            result = sd_write_sector(sector, count, buff);
        else
#endif
            result = sd_read_sector(sector, count, buff);
        if (!result) {
            if (retry < 4)
                printk(writing ? "    retry write OK.\n" : "    retry read OK.\n");
            break;
        }
        printk(writing ? "disk_write: pdrv=%d sector=%08x count=%d\n" :
                         "disk_read: pdrv=%d sector=%08x count=%d\n",
               (int)(operation & 0xffu), sector, count);
        printk("    retv=%08x\n", result);
#if FF_FS_READONLY == 0
        if (writing && retry > 1 && sd_sync()) break;
#endif
    } while (--retry);
    osSemaphoreRelease(sem_diskio);
    return result ? RES_ERROR : RES_OK;
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    return disk_transfer(pdrv, buff, sector, count);
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    return disk_transfer(pdrv | DISK_TRANSFER_WRITE, (BYTE *)buff, sector, count);
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
	if (cmd == CTRL_SYNC) {
		if (osSemaphoreAcquire(sem_diskio, 100) != osOK) return RES_ERROR;
		int result = sd_sync();
		osSemaphoreRelease(sem_diskio);
		return result ? RES_ERROR : RES_OK;
	}
	if(cmd==GET_SECTOR_SIZE)
		*(int*)buff = 512;

	return RES_OK;
}


