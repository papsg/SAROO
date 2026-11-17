# RTX resource adaptations

These checkpoints adapt selected paps-g work to the public v0.8-derived tree.
Each source change is measured against its actual public parent. The original
Arm copyright and Apache-2.0 notices remain in the selected provider files.
The GNU and QEMU checks below do not qualify MDK output, physical scheduling,
or a Saturn game.

## Omit round-robin work when the configured scheduler disables it

Original implementation: **paps-g**, private commit `b483124c`,
AUD-RES-024. Public adaptation: add the existing `OS_ROBIN_ENABLE` configuration
gate to the retained runtime timeout check in `rtx_system.c`. The standard
`RTX_Config.h` sets the switch to zero; `rtx_kernel.c` copies the configured
timeout at initialization, and the selected provider inventory has no later
timeout writer. Explicit yield, priority preemption, timer callbacks, delay
retirement and dispatch remain outside the gated block. A future runtime
round-robin setter would require revisiting this specialization.

[Arm's RTX configuration reference](https://arm-software.github.io/CMSIS_5/RTOS2/html/config_rtx5.html#RoundRobin)
defines `OS_ROBIN_ENABLE` as the round-robin switch and describes its timeout.
The pinned [RTX 5.5.1 tick handler](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_system.c#L117-L163)
already tests the runtime timeout. The extra compile-time gate follows the
actual project configuration; it is a SAROO size specialization, not an Arm
erratum or a correction to the enabled scheduler.

The fresh GNU App baseline at public `a4aeb86` loads 59,640 bytes and leaves
5,896 ITCM bytes. With only this source change, it loads **59,576 bytes**, saving
64, with 5,960 free. Static DTCM remains 50,904 bytes. The linked
`osRtxTick_Handler` falls from 276 to 208 bytes. The no-LTO App also links
(63,892 load / 1,644 free) and its BootLoader object compiles. These are link
sizes, not measured tick latency or peak stack use.

Both old and selected tick sources were target-compiled at `-Os` with
`OS_ROBIN_ENABLE=1`. After excluding assembler source-location comments, all 69
tick-handler instruction/directive lines are byte-identical. Four existing
configured RTX host methods pass, including tick/delay wrap and the enabled
software-timer control. The actual Cortex-M7 QEMU/LTO `delay` case passes with
the selected public provider. The complete GNU LTO App, no-LTO App and
BootLoader object all build. No new firmware was installed.

Reproduce with `firmware-build mcu` and `firmware-build rtos-check` using the
explicit toolchain, CMSIS, STM32 and RTX roots documented in
[the offline build instructions](../tools/README.md#original-mcu-app-compilelink-check).
Run `python3 -m unittest discover -s tests -p test_rtx_profile.py` for the
configured-profile controls. Compare the same public parent and candidate
with fresh output directories; the original private image sizes are not used
as the public baseline.
