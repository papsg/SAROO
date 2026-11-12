# FPGA SDRAM cache and refresh

## Apply accepted writes to both caches

Adapted from paps-g `eefa5aca` (FPGA-CACHE-001). The Saturn and MCU
each have a read cache. The old controllers updated their copies when
a write was requested. Each copy favored its own port when two requests
arrived together. SDRAM accepted both writes, but each cache could
discard the other port's update. A request-time update could also displace
a word arriving from SDRAM while the fill still marked that word valid.
[Original v0.8 cache block](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/cacheblk.v),
[memory arbiter](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/memhub.v),
[SDRAM controller](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/tsdram.v).

The arbiter now sends both cache blocks the address, data and byte mask
of each accepted write. SDRAM serializes that event after the preceding
read burst. Both caches observe the same write order and preserve their
existing valid bits and storage. The public port includes the existing
memory-command acknowledgment reset.

```sh
python3 -B -m unittest discover -s tests -p test_fpga_cache.py -v
python3 -B -m unittest discover -s tests -p test_fpga_memory.py -v
```

Set `SAROO_FPGA_CACHE_SOURCE` to the parent checkout's `FPGA` directory
and select `-k regressions` or `-k controls` to reproduce the comparison.
The parent fails five concurrent-write and write-during-fill cases and
passes three masked or separated controls. The port passes 416 cases
across both ports, all word and byte-mask combinations, and multiple
fill phases. The four existing reset tests pass. The fixture records
commands from the actual four RTL modules and compares their cache
results with modeled SDRAM. Physical bus timing remains unqualified.

The complete Quartus fit goes from 1,479 to 1,426 logic elements and
from 816 to 814 registers, with 32,768 memory bits and 150 pins in
both builds. All 21 reported constrained timing checks pass. The
comparison uses the same 18 project inputs as the fitted public
baseline, with changes in the three cache/arbiter source files.
[Offline Quartus build instructions](../tools/README.md#original-fpga-project-comparison).

## Clear refresh acknowledgment on reset

Adapted from paps-g `eefa5aca` (FPGA-REFRESH-001). The SDRAM controller
asserts `ref_ack` when it accepts refresh, then clears it when it issues
the refresh command. A reset between those steps used to leave
`ref_ack` high. That made the counter reload the normal refresh
interval on every clock and kept the controller in power-on startup.
The public port derives the one-clock acknowledgment from the state
entered at refresh acceptance. Reset goes to the power-on state,
clearing the acknowledgment along with it. The controller still uses
its normal startup count and refresh interval. A separate register
with an explicit reset passed simulation and missed the reported
setup requirement by 0.202 ns in its full fit. That candidate was
discarded.
[Original v0.8 refresh counter and controller](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/tsdram.v#L130-L201).

```sh
python3 -B -m unittest discover -s tests -p test_fpga_memory.py -v
```

Set `SAROO_FPGA_REFRESH_BASELINE` to the preceding checkout's `FPGA`
directory to compare its exact controller. Reset before refresh issue
stalls both parent ports. Reset just after issue passes on both. The
corrected RTL passes all 48 port, phase and byte-mask combinations
across refresh and memory-command reset. The 416 accepted-write
controls pass as well. The fixtures model the FPGA reset input
and emitted SDRAM commands. Physical power-on timing remains
unqualified.

The selected fit uses 1,413 logic elements and 813 registers. All
21 reported constrained checks pass. The memory bits, pins and
clock configuration match the preceding cache-write fit.

## Wait for the requested cache word

Adapted from paps-g `6b156a19` (FPGA-CACHE-002). The old controller
completed a read as soon as the cache line address matched. During a
four-word fill, this could return data from the previous line before
the requested word arrived. At the reset tag, it could return old data
even with every valid bit cleared.
[v0.8 read decision](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/cachebus.v#L74-L85),
[v0.8 cache block reset and fill](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/cacheblk.v#L74-L94).

A matching valid word still completes immediately. A matching line
with its requested word pending uses the existing wait state to finish
that fill. A completely invalid line takes the ordinary miss path.
This relies on the accepted-write port preserving fill validity
when it updates the data in both caches.
[Accepted-write prerequisite](https://github.com/papsg/SAROO/commit/36174aca6fa36a89b7739592e96b4bbc773b7666).

```sh
python3 -B -m unittest discover -s tests -p test_fpga_cache_read.py -v
# Set SAROO_FPGA_CACHE_READ_SOURCE to the parent checkout's FPGA directory.
# On the parent, select -k regressions for the stale reads or -k controls
# for warm hits and ordinary misses.
```

Both ports on the exact parent return `0x9013` in place of `0x9023`
during a partial fill and `0x9010` in place of `0xc401` at the reset
tag. All four targeted valid controls pass. The port passes 200
distinct read scenarios over both ports, all words, and multiple
request spacings. Valid words complete without a new wait and the
read count confirms that a partial fill shares the original burst.
The command-count control detects a third SDRAM read on each port
when a test variant refetches the missing word.
The 416 accepted-write scenarios and six memory reset methods also pass.
[Public parent](https://github.com/papsg/SAROO/commit/a9a64b32fe8fa4bdcf99c8338ad57786b64b9c23).

The reset-tag address exceeds the low 16 MiB exposed by both ports
in the v0.8 top-level design. The partial-fill test uses low addresses
and short module-level request spacing. The fixture models SDRAM
command and data flow. Physical request spacing and game effects
remain unqualified.
[v0.8 address mapping](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L674-L712).

The complete five-stage Quartus build passes all 21 reported
constrained checks. Logic elements increase from 1,413 to 1,435.
Both fits use 813 registers and 32,768 memory bits. The lowest
reported setup slack goes from 0.968 ns to 1.093 ns. The compiler,
clock constraints and 17 other project inputs match the parent.
[Offline Quartus build instructions](../tools/README.md#original-fpga-project-comparison).

## Bound autonomous refresh spacing

Adapted from paps-g `cd22aeb1` (AUD-FPGA-002). At the default reload
of 781, the controller used to issue refresh commands 783 clocks
apart while idle and 792 clocks apart when it admitted a read or write
as the counter expired. At the nominal 100 MHz controller clock, even
the idle interval takes 64.143 ms over 8,192 refreshes.

The reload now subtracts 64 clocks from values above 64, allowing time
for an in-flight command and clock tolerance. The default reload yields
719 clocks idle and 728 with aligned traffic. The latter is 59.638 ms
for 8,192 refreshes at 100 MHz, or 62.777 ms at 95 MHz. The budget
calculation applies to the default reload. The controller keeps the
zero disable setting, small reload values, external refresh requests,
and the existing startup and acknowledgment behavior.

Samsung's *512Mb D-die SDRAM K4S511632D*, Rev. 1.0, November 2005,
§1.0 p. 4 and §13.0 p. 11 note 2, gives the 8,192-cycle, 64 ms refresh
requirement. The original [default reload](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v)
and [clock configuration](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/mainpll.v)
are the inputs to the calculation.

```sh
python3 -B -m unittest discover -s tests -p 'test_fpga*.py' -v
# Use SAROO_FPGA_INTERVAL_SOURCE=/path/to/parent/FPGA for the parent reproduction.
```

The parent fails both default interval cases. The port passes all 23
FPGA methods, including two 8,192-interval runs, reload boundaries,
external-request collisions, reset recovery and cache controls. The
shorter interval consumes more refresh command slots. The simulation
observes digital commands and leaves physical retention and throughput
unmeasured.

The five-stage Quartus build passes all 21 constrained checks. Logic
elements increase from 1,435 to 1,450; registers (813), memory bits
(32,768), and pins (150) are unchanged. The lowest reported setup
slack goes from 1.093 ns to 1.103 ns. Only the SDRAM controller changes
among the 18 project inputs.
[Offline Quartus build instructions](../tools/README.md#original-fpga-project-comparison).
