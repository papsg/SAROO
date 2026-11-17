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

## Specialize the configured RTX allocation policy

Original implementation: **paps-g**, private commit `b483124c`, AUD-RES-020.
The public adaptation gates the selected thread, default-stack, mutex and
semaphore fixed-pool branches with their `RTX_Config.h` switches while retaining
the runtime pool-pointer and ownership checks. The pinned `rtx_lib.c` constructs
each fixed-pool pointer from precisely these switches, and `rtx_kernel.c`
copies those pointers into runtime state during initialization. Standard SAROO
sets all four switches to zero, so these branches cannot select a fixed pool.
The nonzero configuration still reaches the original runtime checks. The
selected semaphore provider is now an owned source in both original MDK
targets; the GNU builder verifies its selection and its recorded source hash.

[Arm's RTX memory-pool configuration](https://arm-software.github.io/CMSIS_5/RTOS2/html/config_rtx5.html)
describes the object-specific fixed-block pools. The pinned
[RTX 5.5.1 configuration initializer](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_lib.c#L486-L539)
is the source of the compile-time pointer relationship. The new selected
semaphore source retains Arm's Apache-2.0 header; the recorded original hash
identifies the unmodified pinned provider. This is a configured-size
specialization, not a change to the CMSIS allocation API.

On the exact public parent `614562f`, the GNU LTO App loads 59,576 bytes and
leaves 5,960 ITCM bytes. The selected App loads **59,380 bytes**, saving 196,
and leaves 6,156 free. The no-LTO App falls from 63,892 to 63,648 loaded bytes,
saving 244 and leaving 1,888 free. Static DTCM stays 50,904 bytes in both
build modes. The two App builds and BootLoader object compile. Actual
Cortex-M7/QEMU LTO `delay`, `mutex` and `pool` cases pass, as do the builder's
Rust tests, formatting and strict Clippy checks. With all four fixed-pool
switches enabled, target-compiled thread, mutex and semaphore disassemblies
match their exact-parent providers (1,986, 529 and 481 lines respectively).
The QEMU cases exercise the standard disabled-fixed-pool configuration;
enabled fixed-pool runtime behavior is not qualified on hardware. The first
runtime-pointer-only adaptation saved 196
LTO bytes but grew the no-LTO image by 60 bytes, so it was replaced by this
compile-time gate. No firmware was installed.

Reproduce with the same [offline GNU build instructions](../tools/README.md#original-mcu-app-compilelink-check)
on public parent `614562f` and this checkpoint, using fresh output directories.
Run `firmware-build rtos-check --case delay`, `--case mutex` and `--case pool`
with `--lto` and the same pinned dependencies. MDK output, physical allocation
timing and Saturn games remain unqualified.
