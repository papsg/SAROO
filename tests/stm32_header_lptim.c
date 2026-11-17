/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-151 / 7bef6096; papsg adaptation. No LPTIM access. */
#include <stddef.h>
#include <stm32h7xx.h>

_Static_assert(offsetof(LPTIM_TypeDef, CR) == 0x10U &&
               offsetof(LPTIM_TypeDef, CFGR) == 0x0cU,
               "SSR151 LPTIM register layout control");
_Static_assert(LPTIM_CR_SNGSTRT_Pos == 1U &&
               LPTIM_CR_ENABLE_Msk == 0x01UL &&
               LPTIM_CR_CNTSTRT_Msk == 0x04UL &&
               LPTIM_CR_COUNTRST_Msk == 0x08UL &&
               LPTIM_CR_RSTARE_Msk == 0x10UL,
               "SSR151 unchanged command positions and neighbors");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(LPTIM_CR_SNGSTRT_Msk == 0x02UL &&
               LPTIM_CR_SNGSTRT == 0x02UL,
               "SSR151 single-start command is exactly bit one");
_Static_assert((LPTIM_CR_SNGSTRT_Msk & 0x00080000UL) == 0UL,
               "SSR151 reserved bit nineteen excluded");
_Static_assert((LPTIM_CR_ENABLE | LPTIM_CR_SNGSTRT) == 0x03UL &&
               (LPTIM_CR_ENABLE | LPTIM_CR_SNGSTRT |
                LPTIM_CR_RSTARE) == 0x13UL,
               "SSR151 intended enabled single-start command words");
#endif

int stm32_header_lptim_contract(void) { return 0; }
