# Controller and multitap input

This adapts calebshub's [original PR #380](https://github.com/tpunix/SAROO/pull/380),
deduplicating the identical input code in [#378](https://github.com/tpunix/SAROO/pull/378)
and [#379](https://github.com/tpunix/SAROO/pull/379). The original first-socket,
fixed-offset algorithm is replaced by our reviewed streaming implementation.
The language bounds correction included in those requests is already on main.
The author reported Japanese OEM multitap socket-1 tests on v0.9/FPGA 06,
including navigation and game launches. Those observations apply to that
narrower implementation; physical qualification of this port remains open.

The v0.8 parser advances into the first controller's button data instead of the
next port. Multitap socket occupancy and variable peripheral lengths also make
fixed offsets insufficient. Two populated six-player taps exceed one 32-byte
OREG block. The new parser follows both port headers, each socket's actual
length, extended records and continued blocks. Any supported socket may navigate;
simultaneous compatible button fields are ORed using the existing menu policy.
Mouse, gun-coordinate and other unsupported layouts are skipped, allowing a
normal pad in the other port to navigate without interpreting motion as buttons.

The BIOS-managed system-manager handler consumes at most one 32-byte OREG page
per interrupt. It uses SR readiness and a software CONT shadow: IREG0 is
write-only. Normal NPE-clear completion ends in hardware; invalid or timed-out
collections send BREAK while preserving the last CONT. Partial/invalid samples
are not published. The bound is sixteen pages / 512 bytes, covering two ports
of fifteen maximum-size extended records without a packet buffer.

Command admission remains inside VBlank after its prohibited opening window.
This v0.8 port uses the existing FRT at system-clock/128: a conservative
90-tick wait exceeds 300 microseconds at both documented 26.8/28.6 MHz CPU
modes. It keeps the baseline FRT without a cartridge-clock or FPGA change. Stale admission from a skipped
field is discarded. Pending recovery uses 20,849 inherited timer ticks,
nominally 100 ms. The timer's accumulator is made unsigned and deadline
comparison handles 32-bit wrap; no unrelated repeat/scroll behavior changes.
Clock resets, missed overflows during long foreground stalls and physical
regional calibration remain inherited clock limits. Host clock fixtures do not
establish a strict wall-clock recovery bound.

The menu registers its handler and changes the cached interrupt mask through
the BIOS services. CPU masking protects the timeout/result race. Binary
execution, BIOS boot and CD-player takeover remove/mask the handler before
transferring control or replacing BIOS vectors; returning paths restore it.
The existing direct-pad build mode retains its original behavior. No custom
diagnostic mode or direct-pad timing experiment is imported.
With the pinned SH-2 compiler, the changed objects add 944 text/readonly bytes
and 24 BSS bytes, with unchanged initialized data. These are object-section
totals, not a complete linked-image or physical-stack measurement. The streaming
state itself is eight bytes; no heap or packet allocation is added.

## Primary requirements

- [Sega SMPC manual ST-169-R1](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf):
  register access types, printed pp. 6–7; Type-D flow, p. 16; INTBACK command
  window, pp. 17–18; CONT/BREAK, pp. 49–53 and 58; OREG acquisition,
  pp. 61–66; port/record formats, pp. 67–73; compatible button formats,
  pp. 76–79.
- [Sega final SCU precautions ST-210](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf),
  No. 07, printed p. 4: software must not write IST. This supersedes the older
  write-zero-to-clear interpretation. This implementation never accesses IST.
- [Hitachi SH7604 manual](https://antime.kapsi.fi/sega/files/sh7604.pdf),
  section 11.2.6, printed p. 302: TCR value 2 selects system-clock/128.
- [Sega Saturn overview ST-103-R1](https://antime.kapsi.fi/sega/files/ST-103-R1-040194.pdf),
  section 2.1, printed p. 6: the two SH-2 clock modes used for admission's
  conservative tick bound.

## Reproducible checks and limits

Run `make -C tests check-peripherals`. Eleven ASan/UBSan methods compile the
actual input subsystem with register/interrupt backing and an advancing test
clock. They cover 13,422 complete packets: 233 direct/type/length cases,
4,096 two-multitap occupancy combinations, 8,192 mouse/pad combinations and
901 mixed/extended/page-alignment cases. They also cover busy yielding,
malformed/incomplete data, interrupt readiness, field admission, timeout/wrap,
late interrupts, handler lifecycle and the actual FRT signed/unsigned boundary.
The busy case advances past admission before checking that 1,000 polls preserve
SF and COMREG; deleting the SF guard fails this control.

The independent IREG0 fixture varies all 256 readback values and every
cancellation/handoff page position. It validates issued bytes, not a readable
mirror of writes. Restoring read-modify-write CONT/BREAK expressions fails all
four of those checks. The baseline and exact original PR parser fail eight
behavior checks and time out in the busy test; the lifecycle-only legacy
control passes, and its unavailable continued collector test is skipped.

These are actual-code host protocol checks. BIOS dispatch, electrical timing,
real multitaps/third-party devices, particular gun combinations and game
handoffs still need physical qualification. Source and target-object inspection
check the transfer boundaries; they do not execute BIOS/game code on the host.
The available toolchain compiles target units but lacks the original libc for
complete firmware linking. No Saturn access or firmware installation is used.
