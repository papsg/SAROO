/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-148 / 3a3d4ad7; papsg adaptation. No LTDC access. */
#include <stddef.h>
#include <stdint.h>
#include <stm32h7xx.h>

#define LTDC_PUT(value, mask, pos) (((value) << (pos)) & (mask))

_Static_assert(LTDC_LxWHPCR_WHSTPOS_Msk == 0x00000fffUL &&
               LTDC_LxWHPCR_WHSTPOS_Pos == 0U,
               "SSR148 unchanged horizontal-start control");
_Static_assert(LTDC_CPSR_CXPOS_Msk == 0xffff0000UL &&
               LTDC_CPSR_CYPOS_Msk == 0x0000ffffUL,
               "SSR148 sixteen-bit read-only status controls");
_Static_assert(offsetof(LTDC_TypeDef, CPSR) == 0x44U &&
               offsetof(LTDC_Layer_TypeDef, WHPCR) == 0x04U &&
               offsetof(LTDC_Layer_TypeDef, WVPCR) == 0x08U &&
               LTDC_Layer1_BASE - LTDC_BASE == 0x84UL &&
               LTDC_Layer2_BASE - LTDC_BASE == 0x104UL,
               "SSR148 LTDC layer and member layout controls");
_Static_assert((LTDC_PUT(0x14UL, LTDC_LxWHPCR_WHSTPOS_Msk,
                         LTDC_LxWHPCR_WHSTPOS_Pos) |
                LTDC_PUT(0x28aUL, LTDC_LxWHPCR_WHSPPOS_Msk,
                         LTDC_LxWHPCR_WHSPPOS_Pos)) == 0x028a0014UL &&
               (LTDC_PUT(0xeUL, LTDC_LxWVPCR_WVSTPOS_Msk,
                         LTDC_LxWVPCR_WVSTPOS_Pos) |
                LTDC_PUT(0x1daUL, LTDC_LxWVPCR_WVSPPOS_Msk,
                         LTDC_LxWVPCR_WVSPPOS_Pos)) == 0x01da000eUL,
               "SSR148 manual's bounded example values remain valid");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(LTDC_LxWHPCR_WHSPPOS_Msk == 0x0fff0000UL,
               "SSR148 horizontal-stop width");
_Static_assert(LTDC_LxWVPCR_WVSTPOS_Msk == 0x000007ffUL,
               "SSR148 vertical-start width");
_Static_assert(LTDC_LxWVPCR_WVSPPOS_Msk == 0x07ff0000UL,
               "SSR148 vertical-stop width");
_Static_assert((LTDC_LxWHPCR_WHSPPOS_Msk & 0xf0000000UL) == 0UL &&
               (LTDC_LxWVPCR_WVSTPOS_Msk & 0x00000800UL) == 0UL &&
               (LTDC_LxWVPCR_WVSPPOS_Msk & 0xf8000000UL) == 0UL,
               "SSR148 reserved window bits excluded");
_Static_assert(LTDC_PUT(UINT32_MAX, LTDC_LxWHPCR_WHSPPOS_Msk,
                         LTDC_LxWHPCR_WHSPPOS_Pos) == 0x0fff0000UL &&
               LTDC_PUT(UINT32_MAX, LTDC_LxWVPCR_WVSTPOS_Msk,
                         LTDC_LxWVPCR_WVSTPOS_Pos) == 0x000007ffUL &&
               LTDC_PUT(UINT32_MAX, LTDC_LxWVPCR_WVSPPOS_Msk,
                         LTDC_LxWVPCR_WVSPPOS_Pos) == 0x07ff0000UL,
               "SSR148 arbitrary inputs clip to documented widths");
#endif

int stm32_header_ltdc_contract(void) { return 0; }
