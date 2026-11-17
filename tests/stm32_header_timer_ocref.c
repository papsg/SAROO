/* Public compiler contract for selected ST DFP 2.3.1 timer capabilities.
 * paps-g SSR-154 / 67e23e05; papsg adaptation. No timer register access. */
#include <stddef.h>
#include <stm32h7xx.h>

_Static_assert(IS_TIM_OCXREF_CLEAR_INSTANCE(TIM1) &&
               IS_TIM_OCXREF_CLEAR_INSTANCE(TIM2) &&
               IS_TIM_OCXREF_CLEAR_INSTANCE(TIM3),
               "SSR154 three existing OCxREF clear members remain");
_Static_assert(!IS_TIM_OCXREF_CLEAR_INSTANCE(TIM6) &&
               !IS_TIM_OCXREF_CLEAR_INSTANCE(TIM7) &&
               !IS_TIM_OCXREF_CLEAR_INSTANCE(TIM12) &&
               !IS_TIM_OCXREF_CLEAR_INSTANCE(TIM13) &&
               !IS_TIM_OCXREF_CLEAR_INSTANCE(TIM14) &&
               !IS_TIM_OCXREF_CLEAR_INSTANCE(TIM15) &&
               !IS_TIM_OCXREF_CLEAR_INSTANCE(TIM16) &&
               !IS_TIM_OCXREF_CLEAR_INSTANCE(TIM17),
               "SSR154 eight documented nonmembers remain rejected");
_Static_assert(offsetof(TIM_TypeDef, SMCR) == 0x08U &&
               offsetof(TIM_TypeDef, CCMR1) == 0x18U &&
               TIM_CCMR1_OC1CE_Msk == 0x80UL &&
               TIM_SMCR_ETPS_Msk == 0x3000UL &&
               TIM_SMCR_ECE_Msk == 0x4000UL &&
               TIM_SMCR_ETF_Msk == 0x0f00UL,
               "SSR154 OCxCE, ETR and layout controls");
_Static_assert(!IS_TIM_CC5_INSTANCE(TIM4) &&
               !IS_TIM_CC5_INSTANCE(TIM5) &&
               IS_TIM_CC5_INSTANCE(TIM8),
               "SSR154 existing channel limits remain distinct");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(IS_TIM_OCXREF_CLEAR_INSTANCE(TIM4),
               "SSR154 TIM4 supports OCxREF clearing");
_Static_assert(IS_TIM_OCXREF_CLEAR_INSTANCE(TIM5),
               "SSR154 TIM5 supports OCxREF clearing");
_Static_assert(IS_TIM_OCXREF_CLEAR_INSTANCE(TIM8),
               "SSR154 TIM8 supports OCxREF clearing");
#endif

int stm32_header_timer_ocref_contract(void) { return 0; }
