/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-147 / e0a9922c; papsg adaptation. No JPEG access. */
#include <stddef.h>
#include <stm32h7xx.h>

#define JPEG_CLEAR(eoc, header) \
  (((eoc) ? JPEG_CFR_CEOCF : 0UL) | ((header) ? JPEG_CFR_CHPDF : 0UL))
#define JPEG_RETIRED(status, command) \
  ((status) & ~((command) & (JPEG_SR_EOCF_Msk | JPEG_SR_HPDF_Msk)))

_Static_assert(offsetof(JPEG_TypeDef, CR) == 0x30U &&
               offsetof(JPEG_TypeDef, SR) == 0x34U &&
               offsetof(JPEG_TypeDef, CFR) == 0x38U &&
               offsetof(JPEG_TypeDef, DIR) == 0x40U &&
               offsetof(JPEG_TypeDef, DOR) == 0x44U,
               "SSR147 JPEG register layout control");
_Static_assert(JPEG_SR_EOCF_Msk == 0x20UL && JPEG_SR_HPDF_Msk == 0x40UL &&
               JPEG_SR_COF_Msk == 0x80UL,
               "SSR147 status-flag controls");
_Static_assert(JPEG_CLEAR(0UL, 0UL) == 0UL &&
               JPEG_RETIRED(0xe0UL, 0UL) == 0xe0UL,
               "SSR147 no-request control");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(JPEG_CFR_CEOCF_Pos == 5U && JPEG_CFR_CEOCF_Msk == 0x20UL,
               "SSR147 end-of-conversion clear command");
_Static_assert(JPEG_CFR_CHPDF_Pos == 6U && JPEG_CFR_CHPDF_Msk == 0x40UL,
               "SSR147 header-done clear command");
_Static_assert((JPEG_CFR_CEOCF_Msk | JPEG_CFR_CHPDF_Msk) == 0x60UL &&
               ((JPEG_CFR_CEOCF_Msk | JPEG_CFR_CHPDF_Msk) & 0x1fUL) == 0UL,
               "SSR147 no reserved clear command bits");
_Static_assert(JPEG_CLEAR(1UL, 0UL) == 0x20UL &&
               JPEG_CLEAR(0UL, 1UL) == 0x40UL &&
               JPEG_CLEAR(1UL, 1UL) == 0x60UL,
               "SSR147 all requested clear combinations");
_Static_assert(JPEG_RETIRED(0xe0UL, JPEG_CLEAR(1UL, 0UL)) == 0xc0UL &&
               JPEG_RETIRED(0xe0UL, JPEG_CLEAR(0UL, 1UL)) == 0xa0UL &&
               JPEG_RETIRED(0xe0UL, JPEG_CLEAR(1UL, 1UL)) == 0x80UL,
               "SSR147 symbolic clear leaves unrelated status intact");
#endif

int stm32_header_jpeg_contract(void) { return 0; }
