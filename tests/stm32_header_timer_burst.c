/* Public compiler contract for selected ST DFP 2.3.1 timer capabilities.
 * paps-g SSR-153 / e2a7a5b3; papsg adaptation. No DMA transfer. */
#include <stddef.h>
#include <stm32h7xx.h>

_Static_assert(offsetof(TIM_TypeDef, DCR) == 0x48U &&
               offsetof(TIM_TypeDef, DMAR) == 0x4cU &&
               TIM_DCR_DBL_Msk == 0x1f00UL &&
               TIM_DCR_DBA_Msk == 0x001fUL &&
               TIM_DMAR_DMAB_Msk == 0xffffUL,
               "SSR153 timer DMA-burst register and field controls");
_Static_assert(IS_TIM_DMABURST_INSTANCE(TIM1) &&
               IS_TIM_DMABURST_INSTANCE(TIM2) &&
               IS_TIM_DMABURST_INSTANCE(TIM3) &&
               IS_TIM_DMABURST_INSTANCE(TIM4) &&
               IS_TIM_DMABURST_INSTANCE(TIM5) &&
               IS_TIM_DMABURST_INSTANCE(TIM8),
               "SSR153 six existing DMA-burst members remain");
_Static_assert(!IS_TIM_DMABURST_INSTANCE(TIM6) &&
               !IS_TIM_DMABURST_INSTANCE(TIM7) &&
               !IS_TIM_DMABURST_INSTANCE(TIM12) &&
               !IS_TIM_DMABURST_INSTANCE(TIM13) &&
               !IS_TIM_DMABURST_INSTANCE(TIM14),
               "SSR153 five documented DMA-burst nonmembers remain rejected");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(IS_TIM_DMABURST_INSTANCE(TIM15),
               "SSR153 TIM15 has DMA-burst registers");
_Static_assert(IS_TIM_DMABURST_INSTANCE(TIM16),
               "SSR153 TIM16 has DMA-burst registers");
_Static_assert(IS_TIM_DMABURST_INSTANCE(TIM17),
               "SSR153 TIM17 has DMA-burst registers");
#endif

int stm32_header_timer_burst_contract(void) { return 0; }
