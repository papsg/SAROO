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

## Continue playback through a pickup-preserving PLAY

Adapted from paps-g `b8c71ee2` (MCU-CDC-026/027/028). The disk worker can
acknowledge an update at the next read checkpoint and keep the current
sector reader, pickup and audio state. It checks for an update before
finishing the old range. A shorter range runs the ordinary end/repeat
decision at the new bounds. A file or directory reader and a PLAY that
moves the pickup use the existing pause and restart path. A paused reader
resuming midrange enters PLAY even with the seek delay configured.
[CD Communication Interface User's Manual, §4.2(3–4), table 4.5](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p04_10.htm),
[ST-162-062094, §8.2.2 No. 2.1, printed p. 82](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=70).

The port uses the public IRQ-protected idle publication for an actual pause.
An update waits for worker acknowledgment before publishing settings. The
worker checks the reader kind at acknowledgment because a read can fail
while the command waits. A timeout leaves the old range and repeat settings
in force. The public UART logger queues output without waiting for transmission,
so command publication does not block on the log between acknowledgment
and reply. The previous track-form restart and delayed status behavior were
introduced as compatibility changes. Physical game behavior remains open.
[Steam-Heart's change](https://github.com/tpunix/SAROO/commit/56f3e1d78d0ed30d06ee32d7112e8726dda2d6f8),
[Delayed PLAY change](https://github.com/tpunix/SAROO/commit/eb27380af199deeab20db35bf78ba42dee7761a6).

```sh
python3 -B -m unittest discover -s tests -p test_cdc_play_continuity.py -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_play_continuity.py -v
```

Set `SAROO_CDC_DRIVE_DIR` to the parent's `Firm_MCU/Saturn` directory to
reproduce eight failing methods and the valid delayed-start/read-error
control. The fixture runs the real disk task and command handler with
authored sectors and a host thread gate that models command-task preemption.
It covers 64 active/old-end updates, shorter ranges, repeat, delayed resume,
file/directory replacement, timeout and reader failure. Both CD profiles
pass 183 methods. The 13 disc/save handoff methods also pass. The GNU MCU App
loads 63,228 bytes with 2,308 bytes of ITCM space. Physical scheduling,
audio timing and game playback remain unqualified.
