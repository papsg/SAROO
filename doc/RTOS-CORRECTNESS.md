# Selected RTX 5.5.1 API corrections

## SSR-007: report refused waits

`osDelay(0)` now returns `osErrorParameter`. Relative and absolute waits
return `osError` if the kernel refuses admission, rather than claiming that
the thread waited. Admitted waits and the existing wrap-aware deadline
arithmetic are unchanged. Normal firmware calls positive relative waits;
this is an API contract repair, not evidence of a particular game failure.

Arm CMSIS-RTOS2 API 2.1.3, **Generic Wait Functions**, `osDelay` and
`osDelayUntil`, **Return values**, requires these outcomes. The same
reference documents tick wrap and the maximum absolute delay of 2^31−1:
[Arm Generic Wait Functions](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__Wait.html).
The locally retained matching reference was used; no new online research.
No Sega rule specifies these MCU-internal kernel API results.

The correction adapts paps-g's `692f9716`/SSR-007. Original RTX code and
unchanged private support headers remain attributed to Arm Limited, under
Apache-2.0. See [provider provenance](../Firm_MCU/RTE/CMSIS/RTX/provenance.json)
and its adjacent complete license. Public integration and checks are by papsg.
There is no original tpunix issue/PR identified for this independent finding.

## Provider selection

The original project selects CMSIS 5.6.0 / RTX 5.5.1's Library variant.
Both App and BootLoader explicitly compile the replacement `rtx_delay.c`;
its public symbols resolve before the library's delay member is needed.
Three unchanged source-private headers are the required compilation interface;
public RTX/API headers still come from the selected pack. The override checks
the kernel version at compile time. The GNU builder selects the same source
and excludes the stock delay translation unit, retaining all other SDK units.
It rejects changed base-provider/header hashes, never patches the installed
SDK, and records both original SDK and owned inputs. No compiler ABI, mutex,
dispatcher or other historical private-kernel change is imported.

## Reproduce offline

Build `firmware-build` with the locked Cargo workspace, then run:

```sh
firmware-build rtos-check --repo . --out tests/build/UNUSED \
  --toolchain ARM-GNU-ROOT --cmsis CMSIS-ROOT \
  --stm32 STM32-INCLUDE --rtx CMSIS-RTOS2 --case delay
```

Use a fresh output directory for each run. `--baseline` selects only the
original delay provider and must fail after reporting all five false-success
results; successful/expired/current/masked-IRQ controls pass before that failure.
The selected provider passes 23 assertions, including positive waits, zero,
locked-kernel relative/absolute waits, real idle-thread empty-ready-list
relative/absolute waits, public IRQ-mask refusal and an admitted deadline
across tick wrap. `--lto` repeats the actual API/SVC/exception check with LTO.
`--archive` retains all original kernel units in a GNU archive and links the
owned source first; the same checks prove that the original delay member
does not override the correction. It tests library-member resolution, not
the unavailable proprietary Keil linker or its compiled library.

The maintained Rust command stages pinned inputs, builds and executes the
real kernel and Cortex-M7 exception assembly on local QEMU. Startup, virtual
RAM and semihosting belong solely to the test executable. The standard MCU
contains no test threads, test startup or added runtime diagnostics; software
timers remain disabled. Failed runs and input/command receipts are preserved.
These checks do not qualify physical time, stack high-water, Keil App/BootLoader,
Saturn scheduling or game behavior.

## Public build cost and limits

The same compiler, epoch and SDK inputs rebuild the parent and candidate.
The LTO App grows 8 load bytes to 59,444 (6,092 ITCM bytes free); non-LTO
grows 16 to 63,720 (1,816 free). Data/BSS remain 200/50,696 with LTO and
204/50,688 without; both static DTCM spans remain 50,896 bytes. The relative
service direct frame stays 8 bytes; absolute is unreferenced with LTO and
its non-LTO direct frame changes 16→8. These are compiler direct frames,
not maximum task stack measurements. All 166 linked vector checks pass.
The menu/FPGA and configured software-timer exclusion are unchanged.
