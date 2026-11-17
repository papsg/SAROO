/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-143 / db250f11; papsg adaptation. No Ethernet access. */
#include <stddef.h>
#include <stm32h7xx.h>

#define ETH_STATUS(rx, tx, ax) (((rx) << 8) | ((tx) << 12) | (ax))

#if defined(STM32_CHECK_CONTRACT)
#define ETH_NAMED_RECEIVE(word, name) \
  _Static_assert(((word) & ETH_DMADSR_RPS_Msk) == (name), \
                 "SSR143 named receive state must use receive field")
#else
#define ETH_NAMED_RECEIVE(word, name)
#endif

#define ETH_STATE(rx, name, tx, ax) \
  _Static_assert(((ETH_STATUS(rx, tx, ax) & ETH_DMADSR_RPS_Msk) >> \
                  ETH_DMADSR_RPS_Pos) == (rx), "SSR143 receive extraction control"); \
  _Static_assert(((ETH_STATUS(rx, tx, ax) & ETH_DMADSR_TPS_Msk) >> \
                  ETH_DMADSR_TPS_Pos) == (tx), "SSR143 transmit extraction control"); \
  ETH_NAMED_RECEIVE(ETH_STATUS(rx, tx, ax), name)

#define ETH_RX_STATES(tx, ax) \
  ETH_STATE(0UL, ETH_DMADSR_RPS_STOPPED, tx, ax); \
  ETH_STATE(1UL, ETH_DMADSR_RPS_FETCHING, tx, ax); \
  ETH_STATE(3UL, ETH_DMADSR_RPS_WAITING, tx, ax); \
  ETH_STATE(4UL, ETH_DMADSR_RPS_SUSPENDED, tx, ax); \
  ETH_STATE(5UL, ETH_DMADSR_RPS_CLOSING, tx, ax); \
  ETH_STATE(6UL, ETH_DMADSR_RPS_TIMESTAMP_WR, tx, ax); \
  ETH_STATE(7UL, ETH_DMADSR_RPS_TRANSFERRING, tx, ax)

#define ETH_TX_STATES(ax) \
  ETH_RX_STATES(0UL, ax); ETH_RX_STATES(1UL, ax); \
  ETH_RX_STATES(2UL, ax); ETH_RX_STATES(3UL, ax); \
  ETH_RX_STATES(4UL, ax); ETH_RX_STATES(6UL, ax); \
  ETH_RX_STATES(7UL, ax)

_Static_assert(ETH_DMADSR_RPS_Pos == 8U && ETH_DMADSR_RPS_Msk == 0x00000f00UL,
               "SSR143 aggregate receive field control");
_Static_assert(ETH_DMADSR_TPS_Pos == 12U && ETH_DMADSR_TPS_Msk == 0x0000f000UL,
               "SSR143 aggregate transmit field control");
_Static_assert(ETH_DMADSR_TPS_STOPPED == 0UL &&
               ETH_DMADSR_TPS_FETCHING == 0x1000UL &&
               ETH_DMADSR_TPS_WAITING == 0x2000UL &&
               ETH_DMADSR_TPS_READING == 0x3000UL &&
               ETH_DMADSR_TPS_TIMESTAMP_WR == 0x4000UL &&
               ETH_DMADSR_TPS_SUSPENDED == 0x6000UL &&
               ETH_DMADSR_TPS_CLOSING == 0x7000UL,
               "SSR143 named transmit-state controls");
_Static_assert(offsetof(ETH_TypeDef, DMADSR) == 0x100cU,
               "SSR143 read-only debug register layout control");

ETH_TX_STATES(0UL);
ETH_TX_STATES(1UL);

int stm32_header_ethernet_contract(void) { return 0; }
