# CD read safety and ownership

Starts at public `f9ed8fe`. Port selected **paps-g** contributions `89e644f6`
and `3d2c405e` (2026-09-06), with `ac0aeac1`'s corrected read-count semantics,
as separately verified source changes. Keep original notices and logging.
These self-reported source defects are not attributed to a named game's failure.
Measurements below describe individual checkpoints; models/builds do not qualify
physical Saturn timing, scheduling, concurrency, games or original-MDK firmware.

## MCU-CDC-008: command selectors and sector-information positions

Original: selected paps-g `89e644f6`. Fourteen command guard sites cover fifteen
entry points: set/get range, set/get subheader, get mode/connection, get sector
count, calculate size, sector information, GET/GETDEL, delete, change directory,
read directory and read file. GET/GETDEL share admission. The preceding source
indexes24-entry arrays with a byte that can encode256 values; file reads may
wait for playback before validating that byte. Other selectors already guarded
by the earlier ports remain unchanged.

Sega's [ST-162 CD Communication Interface §8.2.6 functions6.1/6.2, printed p.92](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=80)
specifies24 partitions and equal numbers of selectors/apertures. The same
source's [§8.2.5 functions5.1–5.8](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_15.htm)
defines filter arguments, while [§8.2.8 functions8.1/8.2/8.5](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_18.htm)
defines working selectors for directory/file commands. This is ordinary host CD
command admission, independent of USB and the filesystem worker's later derived
routes. Reject an invalid array selector before state access or playback waits;
an already-active GET/GETDEL retains its existing WAIT precedence.

[§8.2.6 function6.5, printed p.93](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=81)
defines the sector position and the last-sector sentinel, and returns stored FAD
and subheaders. Keep the full16-bit position; the old mask aliases position256
to0 and changes0xffff into255. Resolve the sentinel, validate extent and check
the actual node before reading it. Preserve the cached logical metadata rather
than reparsing cooked data's absent header. No selected transfer length or
stored payload changes in this checkpoint.

Invalid selectors, invalid sector-information positions and inconsistent missing
nodes return existing REJECT status without state mutation. This is a defensive
response policy from the original contribution, not a measured malformed-command
reply on original hardware. The [general range rule, §5.3.4(4)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
specifies WAIT for empty/invalid buffer ranges; existing GET/delete range behavior
is retained here. Exact sector-query error-response differences remain unmeasured.
Valid disconnected outputs, ignored reset arguments, register layouts, returned
interrupt masks and every existing log format/mask remain.

Eight new actual-source methods pass ASan/UBSan and O2. They include3,480 invalid
selector requests across all fifteen commands; all65,536 position words; each
partition's last/empty queries and a short-list check; all8 transfer types' busy
GET precedence;408 valid read/delete tails; all24 filter-register, directory/file
selector and ordinary calculated-size controls. The original source fails three
groups and passes five controls. No fake command implementation or private
preparer supplies those results. FIFO/IRQ/track/wakeup boundaries are host models.

Eighteen compiled mutants are rejected by behavioral assertions or sanitizers,
including removal of each of the14 guard sites, truncation, missing last/null
handling and changed busy precedence. A nineteenth removal of explicit position
extent checking survives on consistent lists because the remaining null guard
also refuses out-of-list positions. That redundant mutant/log is preserved and
is not counted as a detected defect or claimed test success.

The earlier subheader fixture now includes the actual status helper and a small
IRQ model, retaining all byte/state/control oracles. Its three methods also pass
against the preceding public source. All68 current CD methods pass both profiles;
initial fixture-dependency and incomplete SDK-path attempts remain archived.

The full GNU App/RTX/166-vector MCU link adds240 load bytes:58,228→58,468,
leaving7,068 ITCM bytes. Static DTCM remains60,296, with the same required stacks,
pool and workspace. All cached SDK inputs match the preceding receipt. Direct
get-range/get-connection frames grow0→8; sector-info8, common GET48, size24,
file-read32, FIFO24 and IRQ/dispatch24/16 stay unchanged. New invalid-command
status writes use the existing short status critical section; valid-command
payload work and masks are unchanged. No buffer, heap allocation, payload copy,
extra I/O or wait is added. Compiler frames exclude other callees/exception state
and do not measure physical stack peaks or latency.

```sh
python3 tests/test_cdc_bounds.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_bounds.py
make -C tests check-cdc-command-buffer
# Baseline/mutant override: SAROO_CDC_BOUNDS_SOURCE=/path/to/saturn_cdc.c
```

Size semantics009 and output/private-read ownership010/count012 remain pending.
Directory/ISO/XA filesystem011/013 and broader asynchronous copy/move/MPEG
ownership are separate work. This checkpoint includes no device operation,
installed-firmware change, USB integration or added runtime diagnostic.
