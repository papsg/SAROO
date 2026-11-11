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

## MCU-SCHED-002 and MCU-UART-001 — bound failure and preserve queue order

The public driver at `d24d860` can wait for queue space indefinitely or spin
with interrupts masked if a DMA enable is refused. It also spins in the UART
IRQ while waiting for EN to clear. A wrapped copy can expose a new write
position before the second copy, and a nested interrupt producer can reuse
an unreserved span. UART transmission-complete can retire an entire DMA span
during a DMA source stall, even though the UART has only drained the bytes it
received so far. Ordinary disc-loading code uses this queue.
[Public baseline driver](https://github.com/papsg/SAROO/blob/d24d860/Firm_MCU/Main/uart4.c).
[Disc-loading caller](../Firm_MCU/Saturn/cdimg.c).

The App reserves a complete message before copying. The last outstanding
writer publishes all completed reservations in order. Each armed span stops at
that published boundary. The barrier before enabling DMA orders SRAM1 writes
before DMA reads under the existing non-write-back-cache boot configuration.
Queue storage stays at 4,096 bytes with 4,095 usable. When a message does not
fit, its whole admission fails immediately and previously admitted bytes keep
their order. [Arm DUI0646C §3.12.4, printed p. 3-175][arm-dmb].

The DMA IRQ retires only the destination writes accounted for by NDTR after EN
clears. Already accepted bytes stay in the UART FIFO while the exact suffix is
retried. DMA completion and UART wire completion are separate events. The
USART stays enabled between spans. Transfer errors and refused enables use a
TIM4 one-shot retry, nominally 10 ms from the existing 200 MHz timer clock.
A pending timer event keeps its deadline, and a stale timer IRQ does no work.
A nonfatal direct-mode FIFO warning can leave the stream running. UART4 TX
continues to use the required TRBUFF mode.
[ST RM0433 Rev 8][rm0433], §15.3.19–20 pp. 653–655, §15.5.5–6 pp. 659–662,
§48.5.19 pp. 2059–2060, and TIM4 §39.4.1 and §39.4.5–6 pp. 1676, 1683–1684.
[Boot clock setup](../Firm_MCU/Startup/system_init.c).

Queue ownership, exact retirement and deferred retry are adapted from paps-g
`acb824af`. The public adaptation preserves the original BOOT polling
configuration and receive functions, keeps the App's mode correction, limits
active IRQ acknowledgment to the observed DMA events, and waits for the
owned DMA or timer event before trying a busy queue again. BOOT configures
UART4 for its existing polling I/O. App enables DMA1 Stream 0 and TIM4 at
equal priority and services its own stream's flags. The shell keeps its
existing receive path and XMODEM continues to use the polling byte functions. [Public driver](../Firm_MCU/Main/uart4.c).

### Reproduction and verification

Run `make -C tests check-uart`. `SAROO_UART_PROFILE=release` runs the same cases
with optimization and without sanitizers. Use the preceding firmware source
to see the reproduced queue behavior.

```sh
baseline=$(mktemp)
git show d24d860:Firm_MCU/Main/uart4.c > "$baseline"
SAROO_UART_SOURCE="$baseline" make -C tests check-uart
rm "$baseline"
```

The baseline fails 13 of 20 methods. Ordinary output across repeated ring wraps,
short DMA mode, FIFO warning, interrupt mask, initial configuration, stale
timer control and BOOT polling cases pass. The failing cases include invalid
stream data under wrapped and nested publication, early retirement on UART TC,
and bounded-model failures for the original unbounded waits. The updated driver
passes all 20 methods with ASan/UBSan and O2, including all 300 residual counts
and a partial span ending at the ring boundary. Eight independently compiled
changes are detected for early publication, reservation accounting, the arm
barrier, residual loss, over-cleared IRQ events, stale timer work, producer
bypass of a pending retry, and a missing mode bit. The initial two-path
transfer-mode fixture remains reproducible at `d24d860`.

The full GNU App build links the new strong handlers at vector entries 27
and 46. The image is 62,012 bytes (+52), with 3,524 bytes of ITCM available
and 60,176 bytes of static DTCM (−8). The UART BOOT translation unit also
compiles under GNU with identical
instruction bytes for `_getc`, `_getc_tmout`, `_putc` and `_puts`.
The tests model register and FIFO behavior. A persistent hardware failure may
still prevent output and lead to additional dropped messages. Physical
contention, interrupt and serial timing, MDK output, and Saturn or game behavior
remain unqualified.

[rm0433]: https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
[arm-dmb]: https://developer.arm.com/documentation/dui0646/c
