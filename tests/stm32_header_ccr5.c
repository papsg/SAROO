/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-141 / a126b1b8; papsg adaptation. No timer access. */
#include <stddef.h>
#include <stm32h7xx.h>

#define CCR5_GROUP(n) ((((n) & 1UL) ? TIM_CCR5_GC5C1 : 0UL) | \
                        (((n) & 2UL) ? TIM_CCR5_GC5C2 : 0UL) | \
                        (((n) & 4UL) ? TIM_CCR5_GC5C3 : 0UL))
#define CCR5_READ(word) (((word) & TIM_CCR5_CCR5_Msk) >> TIM_CCR5_CCR5_Pos)
#define CCR5_SET(old, value) (((old) & ~TIM_CCR5_CCR5_Msk) | \
                               (((value) << TIM_CCR5_CCR5_Pos) & TIM_CCR5_CCR5_Msk))
#define CCR5_PAIR(group, value) \
  _Static_assert(CCR5_READ(CCR5_GROUP(group) | (value)) == (value), \
                 "SSR141 read compare independently of grouping"); \
  _Static_assert(CCR5_SET(CCR5_GROUP(group) | 0xabcdUL, value) == \
                 (CCR5_GROUP(group) | (value)), "SSR141 preserve grouping on value update")
#define CCR5_GROUP_CASE(group) \
  CCR5_PAIR(group, 0UL); CCR5_PAIR(group, 1UL); \
  CCR5_PAIR(group, 0x1234UL); CCR5_PAIR(group, 0xffffUL)

_Static_assert(TIM_CCR5_CCR5_Pos == 0U, "SSR141 compare position control");
_Static_assert(TIM_CCR5_GC5C1 == 0x20000000UL &&
               TIM_CCR5_GC5C2 == 0x40000000UL &&
               TIM_CCR5_GC5C3 == 0x80000000UL, "SSR141 grouping controls");
_Static_assert(TIM_CCR6_CCR6_Msk == 0xffffUL, "SSR141 adjacent CCR6 control");
_Static_assert(offsetof(TIM_TypeDef, CCR5) == 0x58U &&
               offsetof(TIM_TypeDef, CCR6) == 0x5cU,
               "SSR141 register layout controls");
CCR5_GROUP_CASE(0UL);

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(TIM_CCR5_CCR5_Msk == 0xffffUL, "SSR141 sixteen-bit value field");
_Static_assert((TIM_CCR5_CCR5_Msk &
               (TIM_CCR5_GC5C1 | TIM_CCR5_GC5C2 | TIM_CCR5_GC5C3)) == 0UL,
               "SSR141 compare and grouping disjoint");
CCR5_GROUP_CASE(1UL); CCR5_GROUP_CASE(2UL); CCR5_GROUP_CASE(3UL);
CCR5_GROUP_CASE(4UL); CCR5_GROUP_CASE(5UL); CCR5_GROUP_CASE(6UL);
CCR5_GROUP_CASE(7UL);
#endif

int stm32_header_ccr5_contract(void) { return 0; }
