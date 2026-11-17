/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-142 / 688c5c06; papsg adaptation. No DAC access or status writes. */
#include <stddef.h>
#include <stm32h7xx.h>

#define DAC_STATUS(n) ((((n) & 1UL) ? DAC_SR_DMAUDR1 : 0UL) | \
                       (((n) & 2UL) ? DAC_SR_CAL_FLAG1 : 0UL) | \
                       (((n) & 4UL) ? (1UL << 15) : 0UL) | \
                       (((n) & 8UL) ? DAC_SR_CAL_FLAG2 : 0UL) | \
                       (((n) & 16UL) ? DAC_SR_BWST2 : 0UL) | \
                       (((n) & 32UL) ? DAC_SR_DMAUDR2 : 0UL))
#define DAC_CHECK(n) \
  _Static_assert(((DAC_STATUS(n) & DAC_SR_BWST1_Msk) != 0UL) == (((n) & 4UL) != 0UL), \
                 "SSR142 busy predicate independent of channel-two underrun"); \
  _Static_assert(((DAC_STATUS(n) & DAC_SR_BWST1_Msk) >> DAC_SR_BWST1_Pos) == \
                 (((n) & 4UL) ? 1UL : 0UL), "SSR142 busy extraction is one bit"); \
  _Static_assert(((DAC_STATUS(n) & DAC_SR_DMAUDR2_Msk) >> DAC_SR_DMAUDR2_Pos) == \
                 (((n) & 32UL) ? 1UL : 0UL), "SSR142 channel-two underrun control"); \
  _Static_assert(((DAC_STATUS(n) & DAC_SR_BWST2_Msk) >> DAC_SR_BWST2_Pos) == \
                 (((n) & 16UL) ? 1UL : 0UL), "SSR142 channel-two busy control")
#define DAC_CHECK8(base) \
  DAC_CHECK((base) + 0UL); DAC_CHECK((base) + 1UL); \
  DAC_CHECK((base) + 2UL); DAC_CHECK((base) + 3UL); \
  DAC_CHECK((base) + 4UL); DAC_CHECK((base) + 5UL); \
  DAC_CHECK((base) + 6UL); DAC_CHECK((base) + 7UL)

_Static_assert(DAC_SR_BWST1_Pos == 15U, "SSR142 busy position control");
_Static_assert(DAC_SR_DMAUDR2_Pos == 29U && DAC_SR_DMAUDR2_Msk == 0x20000000UL,
               "SSR142 underrun control");
_Static_assert(DAC_SR_DMAUDR1_Msk == 0x2000UL &&
               DAC_SR_CAL_FLAG1_Msk == 0x4000UL &&
               DAC_SR_CAL_FLAG2_Msk == 0x40000000UL &&
               DAC_SR_BWST2_Msk == 0x80000000UL, "SSR142 other status controls");
_Static_assert(offsetof(DAC_TypeDef, SR) == 0x34U &&
               offsetof(DAC_TypeDef, SHSR1) == 0x40U &&
               offsetof(DAC_TypeDef, SHSR2) == 0x44U,
               "SSR142 status and sample-time layout controls");
DAC_CHECK8(0UL);
DAC_CHECK8(8UL);
DAC_CHECK8(16UL);
DAC_CHECK8(24UL);

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(DAC_SR_BWST1_Msk == 0x00008000UL, "SSR142 one-bit channel-one busy mask");
_Static_assert((DAC_SR_BWST1_Msk & DAC_SR_DMAUDR2_Msk) == 0UL,
               "SSR142 busy and underrun disjoint");
DAC_CHECK8(32UL); DAC_CHECK8(40UL); DAC_CHECK8(48UL); DAC_CHECK8(56UL);
#endif

int stm32_header_dac_contract(void) { return 0; }
