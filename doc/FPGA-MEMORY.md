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
