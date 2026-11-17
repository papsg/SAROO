/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-145 / 3dc1e892; papsg adaptation. No EXTI register access. */
#include <stddef.h>
#include <stm32h7xx.h>

#define D3_MR25_UPDATE(old, value) \
  (((old) & ~EXTI_D3PMR1_MR25_Msk) | ((value) ? EXTI_D3PMR1_MR25_Msk : 0UL))

_Static_assert(offsetof(EXTI_TypeDef, D3PMR1) == 0x0cU &&
               offsetof(EXTI_TypeDef, D3PCR1H) == 0x14U,
               "SSR145 D3 pending and clear-select layout controls");
_Static_assert(EXTI_D3PMR1_MR0_Msk == 0x00000001UL &&
               EXTI_D3PMR1_MR15_Msk == 0x00008000UL &&
               EXTI_D3PMR1_MR19_Msk == 0x00080000UL &&
               EXTI_D3PMR1_MR20_Msk == 0x00100000UL &&
               EXTI_D3PMR1_MR21_Msk == 0x00200000UL,
               "SSR145 neighboring pending-line controls");
_Static_assert(EXTI_D3PCR1H_PCS25_Pos == 18U &&
               EXTI_D3PCR1H_PCS25_Msk == 0x000c0000UL,
               "SSR145 independent clear-source selector control");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(EXTI_D3PMR1_MR25_Pos == 25U,
               "SSR145 line 25 position");
_Static_assert(EXTI_D3PMR1_MR25_Msk == 0x02000000UL &&
               EXTI_D3PMR1_MR25 == EXTI_D3PMR1_MR25_Msk,
               "SSR145 line 25 mask and alias");
_Static_assert((EXTI_D3PMR1_MR25_Msk & 0x01c00000UL) == 0UL,
               "SSR145 no reserved D3 pending bits");
_Static_assert(D3_MR25_UPDATE(0UL, 1UL) == 0x02000000UL &&
               D3_MR25_UPDATE(0x02000000UL, 0UL) == 0UL,
               "SSR145 enable and clear line 25");
_Static_assert(D3_MR25_UPDATE(0x0038ffffUL, 1UL) == 0x0238ffffUL &&
               D3_MR25_UPDATE(0x0238ffffUL, 0UL) == 0x0038ffffUL,
               "SSR145 retain other supported lines");
#endif

int stm32_header_exti_contract(void) { return 0; }
