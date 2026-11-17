/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-146 / 262a8306; papsg adaptation. No HSEM access. */
#include <stddef.h>
#include <stm32h7xx.h>

#define HSEM_OWNER(value, mask, pos) (((value) << (pos)) & (mask))

_Static_assert(offsetof(HSEM_TypeDef, R) == 0x000U &&
               offsetof(HSEM_TypeDef, RLR) == 0x080U &&
               offsetof(HSEM_TypeDef, CR) == 0x140U &&
               offsetof(HSEM_TypeDef, KEYR) == 0x144U,
               "SSR146 HSEM register layout control");
_Static_assert(HSEM_R_PROCID_Msk == 0x000000ffUL &&
               HSEM_RLR_PROCID_Msk == 0x000000ffUL &&
               HSEM_R_LOCK_Msk == 0x80000000UL &&
               HSEM_RLR_LOCK_Msk == 0x80000000UL &&
               HSEM_CR_KEY_Msk == 0xffff0000UL,
               "SSR146 process, lock and key controls");
_Static_assert(HSEM_CPU1_COREID == 3U &&
               HSEM_CR_COREID_CPU1 == 0x300UL &&
               HSEM_CR_COREID_CURRENT == 0x300UL,
               "SSR146 documented CPU master-ID control");
_Static_assert(HSEM_OWNER(3UL, HSEM_R_COREID_Msk, HSEM_R_COREID_Pos) == 0x300UL &&
               HSEM_OWNER(3UL, HSEM_RLR_COREID_Msk, HSEM_RLR_COREID_Pos) == 0x300UL &&
               HSEM_OWNER(3UL, HSEM_CR_COREID_Msk, HSEM_CR_COREID_Pos) == 0x300UL,
               "SSR146 bounded CPU ID behaves the same");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(HSEM_R_COREID_Msk == 0x0f00UL,
               "SSR146 R master-ID width");
_Static_assert(HSEM_RLR_COREID_Msk == 0x0f00UL,
               "SSR146 RLR master-ID width");
_Static_assert(HSEM_CR_COREID_Msk == 0x0f00UL,
               "SSR146 CR master-ID width");
_Static_assert(((HSEM_R_COREID_Msk | HSEM_RLR_COREID_Msk |
                 HSEM_CR_COREID_Msk) & 0xf000UL) == 0UL,
               "SSR146 reserved owner bits excluded");
_Static_assert(HSEM_OWNER(0xffUL, HSEM_R_COREID_Msk, HSEM_R_COREID_Pos) == 0x0f00UL &&
               HSEM_OWNER(0xffUL, HSEM_RLR_COREID_Msk, HSEM_RLR_COREID_Pos) == 0x0f00UL &&
               HSEM_OWNER(0xffUL, HSEM_CR_COREID_Msk, HSEM_CR_COREID_Pos) == 0x0f00UL,
               "SSR146 arbitrary input clips to field width");
_Static_assert((0x5a5a0000UL |
               HSEM_OWNER(0xffUL, HSEM_CR_COREID_Msk, HSEM_CR_COREID_Pos)) ==
               0x5a5a0f00UL, "SSR146 key and clipped owner packing");
#endif

int stm32_header_hsem_contract(void) { return 0; }
