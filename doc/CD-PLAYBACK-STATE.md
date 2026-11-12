# CD playback and media state

## Preserve the playback range across SEEK

Adapted from paps-g `4f123d38` (MCU-CDC-023), following the retained endpoint
work in `d78c74f` and `3a68c3d`. SEEK updates the pickup and its report. The
saved PLAY endpoints, endpoint forms and repeat history survive, so a later
PLAY with unchanged endpoints uses the previous range. The old FAD-seek path
replaced the range end with the end of the current audio track or the seek
address on a data track. The old track/index path also replaced the start.
Sega describes the lifetime of the range and repeat settings and defines
SEEK as a pickup move. [CD Communication Interface User's Manual
ST-162-062094, §4.2(3–4), printed pp. 37–38](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=25),
[§8.2.2 No. 2.2, printed p. 83](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=71).

Upstream introduced the earlier behavior for reported cases in Warriors of
Fate and Steam-Heart's. The tests exercise analogous authored CD command
sequences. Both games still require physical qualification.
[Warriors of Fate change](https://github.com/tpunix/SAROO/commit/d12cadbbc1976bd842f5f919af9ee3420027df15),
[Steam-Heart's change](https://github.com/tpunix/SAROO/commit/3268836f8f8b7e91aefc5a8fc32e322eb93399cb).

```sh
python3 -B -m unittest discover -s tests -p test_cdc_drive.py -v
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
```

Set `SAROO_CDC_DRIVE_DIR` to the parent revision's `Firm_MCU/Saturn`
directory when running the drive fixture. The parent fails three methods
on range replacement and passes the bounded wait, missing-media, and
home-to-pause controls. The port passes 63 endpoint/seek combinations and
162 seek, resume and repeat sequences. Both the sanitizer and O2 runs pass
170 CD methods. The GNU MCU App loads 63,164 bytes with 2,372 bytes of ITCM
space. Physical CD timing and game playback remain unqualified.

## Report SEEK completion and status

Adapted from paps-g `13f35e65` (MCU-CDC-024/025). A successful SEEK
acknowledges with BUSY. A subsequent status query returns the settled
PAUSE or STANDBY state. An explicit seek outside the retained range raises
PEND when the repeat budget is exhausted. The handler returns that bit to
the existing dispatcher, which publishes it with CMOK and the usual SCDQ
condition. Inside-range seeks, remaining repeats, stop and pause add no
new PEND. The command never writes the event-clear register.
[ST-162-062094, §4.1(3), printed p. 33](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21),
[§4.2(3–4), table 4.5 in the later manual](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p04_10.htm).

Run the drive fixture with `-k 'seek_'`. With `SAROO_CDC_DRIVE_DIR`
pointing at the parent, select the four new event and response methods.
The parent misses PEND in the boundary and dispatcher cases and replies
with the settled status. The no-event controls pass. The new tests cover
6,720 interval/repeat combinations, reversed bounds, both worker
acknowledgment paths with timeout and retry, and 56 status/position pairs.
Both CD profiles pass 174 methods. The GNU MCU App loads 63,196 bytes
with 2,340 bytes of ITCM space.
Physical interrupt timing and actual game waits remain unqualified.
