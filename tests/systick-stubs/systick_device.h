#ifndef SAROO_TEST_SYSTICK_DEVICE_H
#define SAROO_TEST_SYSTICK_DEVICE_H
#include <stdint.h>
#define __WEAK __attribute__((weak))
#define __CORTEX_M 7U
typedef struct { volatile uint32_t CTRL, LOAD, VAL, CALIB; } TickRegisters;
typedef struct { volatile uint8_t SHPR[12]; volatile uint32_t ICSR; } CoreRegisters;
TickRegisters *tick_registers(void);
CoreRegisters *core_registers(void);
extern uint32_t SystemCoreClock;
#define SysTick (tick_registers())
#define SCB (core_registers())
#define SysTick_IRQn (-1)
#define SysTick_CTRL_ENABLE_Msk 1U
#define SysTick_CTRL_TICKINT_Msk 2U
#define SysTick_CTRL_CLKSOURCE_Msk 4U
#define SCB_ICSR_PENDSTCLR_Msk (1U << 25)
#define SCB_ICSR_PENDSTSET_Msk (1U << 26)
#endif
