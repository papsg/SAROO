/* Public compiler contract for the selected ST DFP 2.3.1 H750 header.
 * paps-g SSR-144 / 4cfad282; papsg adaptation. No DMA2D transfer. */
#include <stddef.h>
#include <stm32h7xx.h>

#define COLOR_SET(old, mask, pos, value) \
  (((old) & ~(mask)) | (((value) << (pos)) & (mask)))
#define COLOR_READ(word, mask, pos) (((word) & (mask)) >> (pos))

_Static_assert(offsetof(DMA2D_TypeDef, OPFCCR) == 0x34U &&
               offsetof(DMA2D_TypeDef, OCOLR) == 0x38U &&
               offsetof(DMA2D_TypeDef, OMAR) == 0x3cU,
               "SSR144 output register layout control");
_Static_assert(DMA2D_OCOLR_BLUE_2_Msk == 0x001fUL &&
               DMA2D_OCOLR_BLUE_3_Msk == 0x001fUL &&
               DMA2D_OCOLR_ALPHA_3_Msk == 0x8000UL,
               "SSR144 packed blue and alpha controls");
_Static_assert(DMA2D_OCOLR_GREEN_1_Msk == 0x0000ff00UL &&
               DMA2D_OCOLR_RED_1_Msk == 0x00ff0000UL &&
               DMA2D_OCOLR_GREEN_4_Msk == 0x00f0UL &&
               DMA2D_OCOLR_RED_4_Msk == 0x0f00UL,
               "SSR144 unaffected format controls");
_Static_assert(COLOR_READ(0xf81fUL, DMA2D_OCOLR_BLUE_2_Msk,
                          DMA2D_OCOLR_BLUE_2_Pos) == 0x1fUL &&
               COLOR_READ(0x83ffUL, DMA2D_OCOLR_ALPHA_3_Msk,
                          DMA2D_OCOLR_ALPHA_3_Pos) == 1UL,
               "SSR144 unaffected packed-neighbor controls");

#if defined(STM32_CHECK_CONTRACT)
_Static_assert(DMA2D_OCOLR_GREEN_2_Msk == 0x07e0UL,
               "SSR144 RGB565 green width");
_Static_assert(DMA2D_OCOLR_RED_2_Msk == 0xf800UL,
               "SSR144 RGB565 red width");
_Static_assert(DMA2D_OCOLR_GREEN_3_Msk == 0x03e0UL,
               "SSR144 ARGB1555 green width");
_Static_assert(DMA2D_OCOLR_RED_3_Msk == 0x7c00UL,
               "SSR144 ARGB1555 red width");
_Static_assert((DMA2D_OCOLR_GREEN_2_Msk & DMA2D_OCOLR_RED_2_Msk) == 0UL &&
               (DMA2D_OCOLR_GREEN_3_Msk & DMA2D_OCOLR_RED_3_Msk) == 0UL &&
               (DMA2D_OCOLR_RED_3_Msk & DMA2D_OCOLR_ALPHA_3_Msk) == 0UL,
               "SSR144 packed fields do not overlap");
_Static_assert(COLOR_READ(0x07e0UL, DMA2D_OCOLR_GREEN_2_Msk,
                          DMA2D_OCOLR_GREEN_2_Pos) == 0x3fUL &&
               COLOR_READ(0xf800UL, DMA2D_OCOLR_RED_2_Msk,
                          DMA2D_OCOLR_RED_2_Pos) == 0x1fUL &&
               COLOR_READ(0x03e0UL, DMA2D_OCOLR_GREEN_3_Msk,
                          DMA2D_OCOLR_GREEN_3_Pos) == 0x1fUL &&
               COLOR_READ(0x7c00UL, DMA2D_OCOLR_RED_3_Msk,
                          DMA2D_OCOLR_RED_3_Pos) == 0x1fUL,
               "SSR144 full-scale component extraction");
_Static_assert(COLOR_SET(0xf81fUL, DMA2D_OCOLR_GREEN_2_Msk,
                         DMA2D_OCOLR_GREEN_2_Pos, 1UL) == 0xf83fUL &&
               COLOR_SET(0x83ffUL, DMA2D_OCOLR_RED_3_Msk,
                         DMA2D_OCOLR_RED_3_Pos, 1UL) == 0x87ffUL,
               "SSR144 component replacement preserves neighbors");
#endif

int stm32_header_dma2d_contract(void) { return 0; }
