# R–W subcode correctness

## SSR-064: unread-packet overrun notification

Port of paps-g `329466e4`, independently adapted/verified by papsg against the
public publisher, command and existing reset paths. This is our source finding;
no original upstream issue/PR or named-game failure is asserted.

Sega [ST-162-062094 §4.3, printed p.40/PDF p.28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=28)
defines unread-pack overwrite as overrun.
[Section 8.2.3, function3.2, printed p.85/PDF p.73](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=73)
assigns bit1 to that error. The cached original SBL6.01 header defines
`CDC_SCD_OVER=0x02`. Preserved library disassembly reads response byte7
(CR4 low in SH-2 byte order) and stores the flag before starting FIFO transfer.
The individually rehashed header/library identities are recorded in
[file-information wire evidence](CD-FILE-INFO-WIRE.md). Static SDK evidence
does not authenticate the publisher archive or execute its target code.

The software publisher previously displaced unread packets silently. It now
latches0x02 inside the same saved-IRQ exclusion that advances queue cursors.
An accepted R–W command snapshots/acknowledges that loss burst in CR4 low before
FIFO transfer. Q, invalid, empty-WAIT and busy requests do not acknowledge it.
New loss during an older transfer remains for the next command, including after
DataEnd. Existing queue retirement on cache/track/tray/reset also retires the
flag; non-reset INIT retains both. Decode-session policy follows separately.

The public adaptation places the new byte in existing padding after the two
small playback fields. Both actual target CDB symbols remain5,056 bytes, with
unchanged static allocation. This differs from the private field placement;
no format, payload queue, capacity or new telemetry is added.

```sh
python3 -B -m unittest discover -s tests -p test_cdc_subrw_overrun.py -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_subrw_overrun.py -v
python3 -B -m unittest discover -s tests -p test_cdc_subrw.py -v
python3 -B -m unittest discover -s tests -p test_cdc_drive.py -v
make -C tests check-cdc-command-buffer
```

Two methods join the actual publisher/get-subcode/trans-start/FIFO/DataEnd/reset
bodies through the existing bounds fixture. The parent has128 missing-flag
assertions in45,312 ordinary checks and192 in6,208 lifetime checks across all64
cursor starts. Loss-free15-frame, empty, intermediate-consumption, queue data,
response words and transfer completion remain controls. Corrected sanitizer/O2
profiles pass both methods; all six existing publication/preemption and57 drive
methods pass. Zero packet bytes isolate loss signaling from CRC; existing
publication controls separately check nonzero payload/order. Modeled MMIO,
IRQs, Q track lookup and idle reset handoff are explicit fixture boundaries.
The initial fixture-entry extraction refusals ran zero tests and remain excluded.

Fresh LTO/non-LTO MCU builds pass166-vector checks and all67 public-source pins.
Load grows44 bytes per profile to59,528/63,800, leaving6,008/1,736 ITCM bytes.
Data/BSS remain200/50,696 and204/50,688; static DTCM span stays50,896. The
publisher text grows124→136 bytes while its direct frame stays20 bytes in both
builds; this excludes callees and interrupts. Menu/FPGA inputs do not change.

The63 usable software slots are not a claim of physical23/24-pack capacity.
Physical pointer/error-clear timing, pack CRC/error correction,3.2ms deadline,
actual RTOS scheduling/FPGA transfers and game compatibility stay unqualified.
Source snapshots, failed/passing cohorts, target receipts and local primary
evidence remain preserved. No runtime diagnostic, USB integration, new overlay
recipe, firmware installation or device operation is introduced.
