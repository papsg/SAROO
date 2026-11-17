/* Public compiler contract for selected ST DFP 2.3.1 TIM1/TIM8 fields.
 * paps-g SSR-150 / 5955f4d4; papsg adaptation. No timer access. */
#include <stddef.h>
#include <stm32h7xx.h>

/* RM0433 output mode is split: low three bits at pos, bit 3 at pos+12. */
#define TIM_MODE(value, pos) \
  ((((value) & 7UL) << (pos)) | (((value) & 8UL) << ((pos) + 9U)))
#define TIM_REPLACE(word, mask, value, pos) \
  (((word) & ~(mask)) | TIM_MODE((value), (pos)))

_Static_assert(offsetof(TIM_TypeDef, CCMR1) == 0x18U &&
               offsetof(TIM_TypeDef, CCMR2) == 0x1cU &&
               offsetof(TIM_TypeDef, CCMR3) == 0x54U,
               "SSR150 timer register layout control");
_Static_assert(TIM_CCMR1_OC1M_Msk == 0x00010070UL &&
               TIM_CCMR1_OC2M_Msk == 0x01007000UL &&
               TIM_CCMR1_OC1M_3 == 0x00010000UL &&
               TIM_CCMR1_OC2M_3 == 0x01000000UL,
               "SSR150 unchanged channel 1/2 controls");
_Static_assert(TIM_REPLACE(TIM_MODE(12UL, 4U) | TIM_MODE(6UL, 12U),
                           TIM_CCMR1_OC1M_Msk, 6UL, 4U) ==
                   (TIM_MODE(6UL, 4U) | TIM_MODE(6UL, 12U)),
               "SSR150 unchanged channel 1 replacement control");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(TIM_CCMR2_OC3M_Msk == 0x00010070UL &&
               TIM_CCMR2_OC4M_Msk == 0x01007000UL &&
               TIM_CCMR3_OC5M_Msk == 0x00010070UL &&
               TIM_CCMR3_OC6M_Msk == 0x01007000UL,
               "SSR150 extended modes included in all channel 3-6 masks");
_Static_assert(TIM_CCMR2_OC4M_3 == 0x01000000UL &&
               TIM_CCMR3_OC6M_3 == 0x01000000UL,
               "SSR150 channel 4/6 high helpers do not select reserved bit 20");
_Static_assert((TIM_CCMR2_OC3M_0 | TIM_CCMR2_OC3M_1 |
                TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_3) == TIM_CCMR2_OC3M_Msk &&
               (TIM_CCMR2_OC4M_0 | TIM_CCMR2_OC4M_1 |
                TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_3) == TIM_CCMR2_OC4M_Msk &&
               (TIM_CCMR3_OC5M_0 | TIM_CCMR3_OC5M_1 |
                TIM_CCMR3_OC5M_2 | TIM_CCMR3_OC5M_3) == TIM_CCMR3_OC5M_Msk &&
               (TIM_CCMR3_OC6M_0 | TIM_CCMR3_OC6M_1 |
                TIM_CCMR3_OC6M_2 | TIM_CCMR3_OC6M_3) == TIM_CCMR3_OC6M_Msk,
               "SSR150 individual helper bits match complete mode masks");
_Static_assert(TIM_REPLACE(TIM_MODE(12UL, 4U) | TIM_MODE(14UL, 12U),
                           TIM_CCMR2_OC3M_Msk, 6UL, 4U) ==
                   (TIM_MODE(6UL, 4U) | TIM_MODE(14UL, 12U)) &&
               TIM_REPLACE(TIM_MODE(14UL, 4U) | TIM_MODE(12UL, 12U),
                           TIM_CCMR2_OC4M_Msk, 6UL, 12U) ==
                   (TIM_MODE(14UL, 4U) | TIM_MODE(6UL, 12U)),
               "SSR150 CCMR2 mode replacement keeps neighbor and clears old high bit");
_Static_assert(TIM_REPLACE(TIM_MODE(12UL, 4U) | TIM_MODE(14UL, 12U),
                           TIM_CCMR3_OC5M_Msk, 6UL, 4U) ==
                   (TIM_MODE(6UL, 4U) | TIM_MODE(14UL, 12U)) &&
               TIM_REPLACE(TIM_MODE(14UL, 4U) | TIM_MODE(12UL, 12U),
                           TIM_CCMR3_OC6M_Msk, 6UL, 12U) ==
                   (TIM_MODE(14UL, 4U) | TIM_MODE(6UL, 12U)),
               "SSR150 CCMR3 mode replacement keeps neighbor and clears old high bit");
#endif

int stm32_header_timer_modes_contract(void) { return 0; }
