# CD command and buffer safety

Start at public `de549dc`. Port selected MCU-CDC-001–004 contributions from
**paps-g**, oldest first, with independent current-source verification. Existing
logging, source/dependency notices and prior public ports remain. No USB, new
diagnostics, private preparer or device operation is included. These self-reported
source findings are not attributed to a named game, audio or save-loss report.

## MCU-CDC-001: separate filter reply status and channel

Original: `f0da2b0b` (2026-09-06). Correct the one expression in command0x43,
`get_filter_subheader`: CD status occupies CR1's high byte and channel occupies
its low byte. The [baseline getter](https://github.com/papsg/SAROO/blob/de549dc/Firm_MCU/Saturn/saturn_cdc.c#L1074)
ORs both into the low byte. Status1/channel2 therefore yields `0x0003` instead
of `0x0102`, losing status and altering the returned condition.

Sega's [CD Communication Interface, §8.2.5 functions5.3/5.4](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_15.htm)
defines the setter/getter of the same subheader conditions. The current
[0x42 setter](https://github.com/papsg/SAROO/blob/de549dc/Firm_MCU/Saturn/saturn_cdc.c#L1052)
places the channel in the low request byte; the [common menu reply reader](https://github.com/papsg/SAROO/blob/de549dc/Firm_Saturn/cdblock.c#L86)
reads status from the high response byte. These actual source contracts plus
an independent set/get round trip establish the selected byte correction.
The local primary HTML/text and source identities are preserved. No exact
hardware register trace or new online observation is claimed.

The manual marks the getter `[S-]` and lists no end flag, while the adjacent
setter lists ESEL. A historical command-wiki ESEL listing is insufficient reason
to change the getter's existing zero return here. Preserve its other three
reply words, complete CD state and interrupt-return mask. This does not qualify
physical HIRQ timing or every aspect of command0x43. Invalid-selector guards
remain later work; this fixture uses the24 valid selectors only.

The public adaptation captures actual current types, functions and inputs
without the private preparer. Three methods pass ASan/UBSan and O2:71,680
independent status/channel/other-field replies,6,144 baseline-valid zero-status
replies and6,144 actual0x42→0x43 round trips. The old getter has71,400 byte-field
failures in the first sweep and fails the round-trip method; the zero-status
control passes. All256 status-byte values are an encoding-domain sweep, not
256 asserted real hardware states. Complete CD-state snapshots and other reply
words are checked. Four mutations detect unshifted status, shifted channel,
changed state and an added ESEL return. Three prior CD setup methods also pass.
The setter's existing log invocation is suppressed only in the host fixture;
production logging is unchanged.

The complete GNU MCU App/RTX/166-vector link grows16 load bytes:
57,396→57,412, leaving8,124 ITCM bytes. Static DTCM remains60,296. Compiler
IRQ/command-dispatch/getter direct frames remain24/16/8 bytes. No new call,
allocation, buffer, payload copy, I/O or interrupt-mask work is introduced.
The historical private zero-growth result is not reused. Source/build receipts
and target resource checks remain separate from original-MDK, physical timing,
stack peaks and game qualification. Menu source/assets remain unchanged.

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
# Preserved baseline/mutation: SAROO_CDC_SUBHEADER_SOURCE=/path/to/saturn_cdc.c
```

## MCU-CDC-002: retained copy/move error and software reset

Queued together with its accepted-software-reset continuation; preserve
ignored/nonreset initialization and unrelated-query retention.

## MCU-CDC-003: buffer admission, accounting and existing-block moves

Queued. Selector/range/busy/capacity guards and zero-copy moves remain separate
from later destination-filter routing, cancellation and general output ownership.

## MCU-CDC-004: bounded incoming filter traversal

Queued. Preserve existing valid-route behavior and logging while checking invalid,
disconnected/cyclic routes, block ownership and completed-destination reporting.
