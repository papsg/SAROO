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
