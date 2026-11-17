/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-140 / d032da23; papsg adaptation. No QSPI access. */
#include <stm32h7xx.h>

#define QSPI_SET(old, value) (((old) & ~QUADSPI_CR_FTHRES_Msk) | \
                              (((value) << QUADSPI_CR_FTHRES_Pos) & QUADSPI_CR_FTHRES_Msk))
#define QSPI_READ(word) (((word) & QUADSPI_CR_FTHRES_Msk) >> QUADSPI_CR_FTHRES_Pos)
#define QSPI_CHECK(value) \
  _Static_assert(QSPI_SET(0UL, value) == ((value) << 8), "SSR140 insertion"); \
  _Static_assert(QSPI_READ((value) << 8) == (value), "SSR140 extraction")

_Static_assert(QUADSPI_CR_FTHRES_Pos == 8U, "SSR140 field position control");
_Static_assert(QUADSPI_CR_FTHRES_0 == 0x100U &&
               QUADSPI_CR_FTHRES_1 == 0x200U &&
               QUADSPI_CR_FTHRES_2 == 0x400U &&
               QUADSPI_CR_FTHRES_3 == 0x800U, "SSR140 low helper controls");
_Static_assert(QUADSPI_CR_FSEL == 0x80U && QUADSPI_CR_FTIE == 0x40000U,
               "SSR140 neighboring control fields");
QSPI_CHECK(0UL);
QSPI_CHECK(1UL);
QSPI_CHECK(7UL);
QSPI_CHECK(15UL);

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(QUADSPI_CR_FTHRES_Msk == 0x1f00UL, "SSR140 five-bit mask");
_Static_assert(QUADSPI_CR_FTHRES_4 == 0x1000UL, "SSR140 fifth helper");
QSPI_CHECK(2UL); QSPI_CHECK(3UL); QSPI_CHECK(4UL); QSPI_CHECK(5UL);
QSPI_CHECK(6UL); QSPI_CHECK(8UL); QSPI_CHECK(9UL); QSPI_CHECK(10UL);
QSPI_CHECK(11UL); QSPI_CHECK(12UL); QSPI_CHECK(13UL); QSPI_CHECK(14UL);
QSPI_CHECK(16UL); QSPI_CHECK(17UL); QSPI_CHECK(18UL); QSPI_CHECK(19UL);
QSPI_CHECK(20UL); QSPI_CHECK(21UL); QSPI_CHECK(22UL); QSPI_CHECK(23UL);
QSPI_CHECK(24UL); QSPI_CHECK(25UL); QSPI_CHECK(26UL); QSPI_CHECK(27UL);
QSPI_CHECK(28UL); QSPI_CHECK(29UL); QSPI_CHECK(30UL); QSPI_CHECK(31UL);
_Static_assert(QSPI_SET(0x1000UL | QUADSPI_CR_FSEL, 7UL) ==
               (0x700UL | QUADSPI_CR_FSEL), "SSR140 clear old high bit");
_Static_assert(QSPI_SET(0x80000000UL | QUADSPI_CR_FSEL, 31UL) ==
               (0x80001f80UL), "SSR140 preserve unrelated bits");
#endif

int stm32_header_qspi_contract(void) { return 0; }
