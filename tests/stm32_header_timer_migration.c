/* Public compiler contract for the H750's documented F7 timer migration aliases.
 * Adapted from paps-g SSR-156 / a922f865; no interrupt is enabled here. */
#include <stm32h7xx.h>

#define STM32_ALIAS_TEXT_1(name) #name
#define STM32_ALIAS_TEXT(name) STM32_ALIAS_TEXT_1(name)

_Static_assert(__builtin_strcmp(STM32_ALIAS_TEXT(TIM1_BRK_TIM9_IRQHandler),
                                "TIM1_BRK_IRQHandler") == 0,
               "legacy break/TIM9 handler still maps to the native vector");
_Static_assert(__builtin_strcmp(STM32_ALIAS_TEXT(TIM1_TRG_COM_TIM11_IRQHandler),
                                "TIM1_TRG_COM_IRQHandler") == 0,
               "legacy trigger/TIM11 handler still maps to the native vector");
_Static_assert(TIM1_BRK_TIM9_IRQn == TIM1_BRK_IRQn &&
               TIM1_UP_TIM10_IRQn == TIM1_UP_IRQn &&
               TIM1_TRG_COM_TIM11_IRQn == TIM1_TRG_COM_IRQn,
               "the three neighboring IRQ-number mappings remain intact");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(__builtin_strcmp(STM32_ALIAS_TEXT(TIM1_UP_TIM10_IRQHandler),
                                "TIM1_UP_IRQHandler") == 0,
               "legacy update/TIM10 handler must bind the native update vector");
_Static_assert(__builtin_strcmp(STM32_ALIAS_TEXT(TIM1_UP_TIM9_IRQHandler),
                                "TIM1_UP_TIM9_IRQHandler") == 0,
               "the nonexistent legacy update/TIM9 name must remain unmapped");
#endif

int stm32_header_timer_migration_contract(void) { return 0; }
