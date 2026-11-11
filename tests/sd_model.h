#ifndef SAROO_SD_MODEL_H
#define SAROO_SD_MODEL_H

typedef void *osSemaphoreId_t;
typedef struct osSemaphoreAttr osSemaphoreAttr_t;
/* CMSIS-RTOS2 status values used by these fixtures. */
typedef enum {
    osOK = 0,
    osError = -1,
    osErrorTimeout = -2,
    osErrorResource = -3,
    osErrorParameter = -4,
    osErrorNoMemory = -5,
    osErrorISR = -6
} osStatus_t;

/* STM32H750 SDMMC and RCC bit positions from RM0433 Rev 8. */
#define SDMMC_CMD_CMDTRANS (1u << 6)
#define SDMMC_CMD_CMDSTOP  (1u << 7)
#define SDMMC_CMD_CPSMEN   (1u << 12)
#define SDMMC_DCTRL_DTDIR  (1u << 1)
#define SDMMC_STA_CCRCFAIL (1u << 0)
#define SDMMC_STA_DCRCFAIL (1u << 1)
#define SDMMC_STA_CTIMEOUT (1u << 2)
#define SDMMC_STA_DTIMEOUT (1u << 3)
#define SDMMC_STA_TXUNDERR (1u << 4)
#define SDMMC_STA_RXOVERR  (1u << 5)
#define SDMMC_STA_CMDREND  (1u << 6)
#define SDMMC_STA_CMDSENT  (1u << 7)
#define SDMMC_STA_DATAEND  (1u << 8)
#define SDMMC_STA_DABORT   (1u << 11)
#define SDMMC_STA_DBCKEND  (1u << 10)
#define SDMMC_STA_DPSMACT  (1u << 12)
#define SDMMC_STA_CPSMACT  (1u << 13)
#define SDMMC_STA_BUSYD0   (1u << 20)
#define SDMMC_STA_BUSYD0END (1u << 21)
#define SDMMC_STA_IDMATE   (1u << 27)
#define RCC_AHB3RSTR_MDMARST   (1u << 0)
#define RCC_AHB3RSTR_SDMMC1RST (1u << 16)

#endif
