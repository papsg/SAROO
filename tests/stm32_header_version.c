/* Public compiler contract for the selected ST DFP 2.3.1 wrapper.
 * paps-g SSR-138 / 10a51a75; papsg adaptation. No target execution. */
#if defined(STM32_CHECK_ALIAS)
#define __CMSIS_DEVICE_VERSION_MAIN 2U
#define __CMSIS_DEVICE_HAL_VERSION_SUB1 3U
#define __CMSIS_DEVICE_HAL_VERSION_SUB2 4U
#define __CMSIS_DEVICE_HAL_VERSION_RC 5U
#endif
#include <stm32h7xx.h>

_Static_assert(__STM32H7xx_CMSIS_DEVICE_VERSION_MAIN == 1, "SSR138 main component");
_Static_assert(__STM32H7xx_CMSIS_DEVICE_VERSION_SUB1 == 9, "SSR138 sub1 component");
_Static_assert(__STM32H7xx_CMSIS_DEVICE_VERSION_SUB2 == 0, "SSR138 sub2 component");
_Static_assert(__STM32H7xx_CMSIS_DEVICE_VERSION_RC == 0, "SSR138 rc component");

#if defined(STM32_CHECK_CONTRACT) || defined(STM32_CHECK_ALIAS)
_Static_assert(__STM32H7xx_CMSIS_DEVICE_VERSION == 0x01090000U,
               "SSR138 aggregate must use its own four components");
#endif

int stm32_header_version_contract(void) { return 0; }
