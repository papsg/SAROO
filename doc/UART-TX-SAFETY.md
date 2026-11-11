# UART transmit safety

## STM32-DMA-001 — enable the required transfer mode

The v0.8-derived MCU configures DMA1 Stream 0 for UART4 TX with
`0x00000441` on both the first enable and the retry. That omits `TRBUFF`
(bit 20), required for UART, USART and LPUART in [ST RM0433 Rev 8][rm0433],
§15.5.5, printed p. 659. UART and I2S audio can issue DMA requests concurrently
on DMA1 Streams 0 and 1. The wrong configuration is reproducible from source
and the register writes. Its relationship to physical audio or UART symptoms
has not been established.

The fix sets `DMA_SxCR_TRBUFF` on every Stream 0 transmit enable while keeping the
existing byte width, direction, increment, and enable bits. Credit goes to
paps-g for identifying the requirement and correcting the private driver in
`c8335e2f`. Applying the same correction to both writes in the original
driver is the public adaptation.

Run `make -C tests check-uart` with a C++17 compiler and ASan/UBSan. The fixture
compiles the production setup and observes its register writes for a normal
enable and for one refused enable followed by success. The unchanged public
baseline `bd498fd` fails both checks at `0x00000441`. The corrected value is
`0x00100441` on both paths. Independently omitting the bit from the first or
second write is detected. The fixture also checks the transfer count, addresses,
DMA TX request, and the barrier.

The complete [GNU MCU App build](../tools/README.md#original-mcu-app-compilelink-check)
links 166 vectors. The image stays at 61,960 bytes with 3,576 bytes of ITCM
available and 60,184 bytes of static DTCM. The compiler's direct frame for
`uart4_dma_send` grows from 72 to 80 bytes. The linked setup contains the
corrected constant. These are host and compile/link checks. Physical DMA
contention, serial timing, hardware, games and MDK output remain unqualified.

[rm0433]: https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
