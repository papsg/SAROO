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
