/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-139 / 2789ea4e; papsg adaptation. No register writes. */
#include <stm32h7xx.h>

#define SDRAM_MODE(n) ((((n) & 1U) ? FMC_SDCMR_MODE_0 : 0U) | \
                       (((n) & 2U) ? FMC_SDCMR_MODE_1 : 0U) | \
                       (((n) & 4U) ? FMC_SDCMR_MODE_2 : 0U))

_Static_assert(FMC_SDCMR_MODE_Pos == 0U, "SSR139 mode position control");
_Static_assert(FMC_SDCMR_MODE_Msk == 7U, "SSR139 aggregate mode control");
_Static_assert(FMC_SDCMR_MODE_0 == 1U, "SSR139 bit zero control");
_Static_assert(FMC_SDCMR_MODE_1 == 2U, "SSR139 bit one control");
_Static_assert(FMC_SDCMR_CTB1 == 0x10U, "SSR139 bank one control");
_Static_assert(FMC_SDCMR_CTB2 == 0x08U, "SSR139 bank two control");
_Static_assert(FMC_SDCMR_NRFS_Pos == 5U && FMC_SDCMR_NRFS_Msk == 0x1e0U,
               "SSR139 refresh-count control");
_Static_assert(SDRAM_MODE(0U) == 0U, "SSR139 normal-mode control");
_Static_assert(SDRAM_MODE(1U) == 1U, "SSR139 clock-enable control");
_Static_assert(SDRAM_MODE(2U) == 2U, "SSR139 precharge control");
_Static_assert(SDRAM_MODE(3U) == 3U, "SSR139 auto-refresh control");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(FMC_SDCMR_MODE_2 == (1UL << 2), "SSR139 bit two");
_Static_assert((FMC_SDCMR_MODE_2 & (FMC_SDCMR_MODE_0 | FMC_SDCMR_MODE_1)) == 0U,
               "SSR139 mode bits must be independent");
_Static_assert(SDRAM_MODE(4U) == 4U, "SSR139 load-mode command");
_Static_assert(SDRAM_MODE(5U) == 5U, "SSR139 self-refresh command");
_Static_assert(SDRAM_MODE(6U) == 6U, "SSR139 power-down command");
_Static_assert(SDRAM_MODE(7U) == 7U, "SSR139 reserved algebra only");
_Static_assert((FMC_SDCMR_CTB1 | SDRAM_MODE(4U)) == 0x14U,
               "SSR139 bank-one load mode");
_Static_assert((FMC_SDCMR_CTB1 | SDRAM_MODE(5U)) == 0x15U,
               "SSR139 bank-one self refresh");
_Static_assert((FMC_SDCMR_CTB1 | SDRAM_MODE(6U)) == 0x16U,
               "SSR139 bank-one power down");
#endif

int stm32_header_sdram_contract(void) { return 0; }
