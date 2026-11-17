/* Public compiler contract for selected ST DFP 2.3.1 H750 fields.
 * paps-g SSR-152 / 1997cb02; papsg adaptation. No HRTIM access. */
#include <stddef.h>
#include <stm32h7xx.h>

_Static_assert(offsetof(HRTIM_TypeDef, sCommonRegs) == 0x380U &&
               offsetof(HRTIM_Common_TypeDef, CR1) == 0U,
               "SSR152 HRTIM common CR1 layout control");
_Static_assert(HRTIM_CR1_ADC4USRC_Pos == 25U &&
               HRTIM_CR1_ADC4USRC_Msk == 0x0e000000UL &&
               HRTIM_CR1_ADC4USRC_0 == 0x02000000UL &&
               HRTIM_CR1_ADC4USRC_1 == 0x04000000UL,
               "SSR152 unchanged trigger-four field and low helpers");
_Static_assert(HRTIM_CR1_ADC1USRC_2 == 0x00040000UL &&
               HRTIM_CR1_ADC2USRC_2 == 0x00200000UL &&
               HRTIM_CR1_ADC3USRC_2 == 0x01000000UL &&
               HRTIM_CR1_TEUDIS_Msk == 0x20UL,
               "SSR152 other trigger and update-disable controls");
_Static_assert((HRTIM_CR1_ADC4USRC_0 |
                HRTIM_CR1_ADC4USRC_1) >> HRTIM_CR1_ADC4USRC_Pos == 3UL,
               "SSR152 unchanged source C encoding control");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(HRTIM_CR1_ADC4USRC_2 == 0x08000000UL,
               "SSR152 trigger-four high source helper is bit 27");
_Static_assert((HRTIM_CR1_ADC4USRC_0 | HRTIM_CR1_ADC4USRC_1 |
                HRTIM_CR1_ADC4USRC_2) == HRTIM_CR1_ADC4USRC_Msk,
               "SSR152 all three helpers cover the source field");
_Static_assert((HRTIM_CR1_ADC4USRC_2 >> HRTIM_CR1_ADC4USRC_Pos) == 4UL &&
               ((HRTIM_CR1_ADC4USRC_2 | HRTIM_CR1_ADC4USRC_0) >>
                HRTIM_CR1_ADC4USRC_Pos) == 5UL,
               "SSR152 source D and E encodings");
#endif

int stm32_header_hrtim_contract(void) { return 0; }
