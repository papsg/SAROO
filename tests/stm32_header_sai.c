/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-149 / 5b37a2f0; papsg adaptation. No SAI register access. */
#include <stddef.h>
#include <stm32h7xx.h>

#define SAI_SUPPORTED_CLEARS \
  (SAI_xCLRFR_COVRUDR | SAI_xCLRFR_CMUTEDET | SAI_xCLRFR_CWCKCFG | \
   SAI_xCLRFR_CCNRDY | SAI_xCLRFR_CAFSDET | SAI_xCLRFR_CLFSDET)

_Static_assert(SAI_SUPPORTED_CLEARS == 0x77UL,
               "SSR149 six documented SAI clear commands");
_Static_assert(SAI_xSR_FREQ_Pos == 3U && SAI_xSR_FREQ_Msk == 0x08UL,
               "SSR149 read-only FIFO request status remains available");
_Static_assert(SAI_xIMR_FREQIE_Pos == 3U && SAI_xIMR_FREQIE_Msk == 0x08UL,
               "SSR149 FIFO request interrupt enable remains available");
_Static_assert(offsetof(SAI_Block_TypeDef, SR) == 0x14U &&
               offsetof(SAI_Block_TypeDef, CLRFR) == 0x18U &&
               SAI1_Block_A_BASE - SAI1_BASE == 0x04UL &&
               SAI1_Block_B_BASE - SAI1_BASE == 0x24UL,
               "SSR149 SAI block and register layout controls");

#if defined(STM32_CHECK_CONTRACT)
#if defined(SAI_xCLRFR_CFREQ_Pos)
#define SAI_UNSUPPORTED_POS 1
#else
#define SAI_UNSUPPORTED_POS 0
#endif
#if defined(SAI_xCLRFR_CFREQ_Msk)
#define SAI_UNSUPPORTED_MASK 1
#else
#define SAI_UNSUPPORTED_MASK 0
#endif
#if defined(SAI_xCLRFR_CFREQ)
#define SAI_UNSUPPORTED_ALIAS 1
#else
#define SAI_UNSUPPORTED_ALIAS 0
#endif
_Static_assert(!SAI_UNSUPPORTED_POS, "SSR149 no reserved clear position");
_Static_assert(!SAI_UNSUPPORTED_MASK, "SSR149 no reserved clear mask");
_Static_assert(!SAI_UNSUPPORTED_ALIAS, "SSR149 no reserved clear alias");
#endif

int stm32_header_sai_contract(void) { return 0; }
