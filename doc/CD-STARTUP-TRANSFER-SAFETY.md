# CD startup, transfer and save metadata safety

## Read subcode safely after a drive stop

Adapted from paps-g `555eb231` (MCU-CDC-018) for the public v0.8-derived
firmware. A seek to home reports track `0xff`. The preceding
[`dc5e079` implementation](https://github.com/papsg/SAROO/blob/dc5e0798b4992b4afb59641581da2daf5ebda615/Firm_MCU/Saturn/saturn_cdc.c)
used that byte to index the track table. It also signaled data-ready for
unsupported subcode types without starting a transfer.

Sega permits Q queries in every drive state and defines the invalid report
after a seek to home. Q carries ten bytes and R–W carries one twelve-word
pack. [CD Communication Interface User's Manual (CD Part), §§4.2–4.3,
archive page 4 of 19](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p04_10.htm),
[§§7.2.4–7.2.5, archive page 8 of 19](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p07_11.htm).

The MCU uses the existing checked track lookup. With no valid track it
returns `0xffffff` in the relative-position field and retains the other
report fields. Q and available R–W requests publish one transfer after
forming the reply. An active transfer or empty R–W queue still returns
WAIT. An idle unsupported request returns REJECT without signaling data-ready.

```sh
python3 -B -m unittest discover -s tests -p test_cdc_drive.py -k subcode -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_drive.py -k subcode -v
make -C tests check-cdc-command-buffer
```

Run the first command with `SAROO_CDC_DRIVE_DIR` pointing at the parent
commit's `Firm_MCU/Saturn` source to reproduce the failures. The parent
fails the stop transition, track validation and unsupported-type checks.
It passes the valid-payload control. The port passes all four under
ASan/UBSan and O2, including all track bytes for every table capacity
from 1 through 99, all type bytes with busy and empty ownership controls,
and 6,144 valid Q payloads. The full CD group passes 163 methods, including
1,179,648 complete replies in the register fixture.

The [GNU MCU App build](../tools/README.md#original-mcu-app-compilelink-check)
loads 63,212 bytes with 2,324 bytes of ITCM space and 60,216 bytes of
static DTCM. The tracked build inputs match the verified source. FIFO start
and scheduling are modeled in the host fixtures. Physical report timing,
multisession subcode and the original MDK build remain unqualified.
