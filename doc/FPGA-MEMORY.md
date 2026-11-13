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

## Match cache addresses to the configured SDRAM cells

Adapted from paps-g `7b9e51b1` and its required same-edge correction
`e265bd35` (AUD-FPGA-001). On an 8 MiB configuration, logical addresses
that differ only in bit 23 select the same SDRAM cell. The old cache tags
treated them as different lines. A masked write through one alias could
therefore leave the other port returning an old byte. The original
[controller address map](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/tsdram.v#L88)
ignores bits 25:23 for 8 MiB, 25:24 for 16 MiB, and bit 25 for 32 MiB.
All 26 bits still identify addresses in the 64 MiB configuration.
Samsung's *K4S511632D*, rev. 1.0, November 2005, §3.0 p. 4, §7.0 p. 7
and §19.0 p. 16, describes the x16 part's multiplexed row,
bank and column addresses. The smaller mappings are SAROO controller
configuration, not requirements from that datasheet.

The two ports now use the physical address for cache lookup, fill tags
and accepted writes. A geometry change waits for the accepted command
and all four fill words before clearing both caches. A new read cannot
hit the previous geometry on that edge. An already waiting read can
still take its final old word. The refresh count, byte masks, burst
size and line storage are unchanged. The original
[top-level connection](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v)
passes only the low 24 address bits from either external port, so the
8 MiB bit-23 alias can occur there. The higher alias cases exercise
the module's full address input.

```sh
python3 -B -m unittest discover -s tests -p 'test_fpga*.py' -v
# Point SAROO_FPGA_GEOMETRY_SOURCE at the parent's FPGA directory
# to reproduce the stale reads with the current geometry fixture.
```

The pin model assigns values to row/bank/column cells independent of
the controller's current configuration. The parent fails 168 cases
and passes all 32 distinct-cell controls. The port passes all 200 cases,
covering both ports, every ignored bit and byte mask, all geometry
pairs, and switches before the column command, during a fill and as
the last word arrives. Removing just the new-hit gate fails 24 of the
same-edge cases while preserving the older in-flight completion. The
existing cache, reset, refresh and clock checks remain in the suite.

The five-stage Quartus build passes all 21 constrained checks. Logic
elements go from 1,450 to 1,491 and registers from 813 to 818; memory
remains 32,768 bits and pins remain 150. The lowest reported setup/hold
slacks change from 1.103/0.161 ns to 0.559/0.153 ns. Only the two
changed RTL files differ among 18 project inputs. The fit and pin model
do not qualify electrical behavior, external bus timing or game play.

## Schedule SDRAM refresh in Saturn FC1 idle windows

Adapted from paps-g `8a31651a`. The old controller issued one refresh
at FC1 and another during ordinary bus use. An expansion RAM transfer
could then need more time than a game allows when external WAIT
extension is disabled. [Upstream issue #349](https://github.com/tpunix/SAROO/issues/349) ·
[Sega SCU User's Manual ST-097-R5 §3.6 pp. 64–65 and
§3.7 p. 72, AREF](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=88).

Issue two AUTO REFRESH commands per synchronized FC1 event, seven
controller clocks apart. Hold new MCU requests to SDRAM while the
window is active. An MCU request qualifies for one clock before
admission, giving a coincident FC1 edge time to reach the synchronizer.
Accepted commands and cache hits retain their existing path. If FC1
events stop, a watchdog restores guarded autonomous refresh. A zero
configured interval still permits FC1 pairs and disables autonomous
refresh. The public port gates the shared countdown during power-on
and preserves a new FC1 event when an older refresh is acknowledged.

```sh
python3 -B -m unittest discover -s tests -p 'test_fpga*.py' -v
# For the parent, point SAROO_FPGA_WINDOW_SOURCE at its FPGA directory
# and select -k pairs_stay or -k early_fc1 in test_fpga_refresh_window.py.
```

The parent pin model observes an out-of-window command 722 clocks
after FC1. The port emits each pair inside the modeled window. The
maximum 8,192-row intervals are 5,865,472 clocks with FC1, 5,890,886
after FC1 stops, and 5,956,422 when traffic hits every fallback
deadline. All are below 6,080,000 clocks, the 64 ms budget at 95 MHz.
The memory fixture also checks the 18- and 30-clock windows with
competing MCU reads and writes. Power-on sequencing, queued edges,
cache geometry and guarded refresh checks pass.
Samsung K4S511632D Rev. 1.0, §1.0 p. 4 and §13.0 p. 11 note 2
specify 8,192 refresh cycles per 64 ms.

The five-stage Quartus build passes all 21 constrained checks.
Logic elements rise from 1,491 to 1,514 and registers from 818 to
824. Memory remains 32,768 bits and pins remain 150. Minimum reported
setup and hold slacks are 1.076 and 0.168 ns. Only the three changed
RTL files differ among the 18 project inputs.

The historical development build addressed issue #349's reproduced
4 MiB Cinepak freeze on one board. Its control froze after about
five seconds and the corrected FPGA80 build ran the reported case
for at least two minutes. That observation belongs to that build and
workload. The public port has RTL and internal fit verification.

## Capture SDRAM read bursts at the pins

FPGA-SDRAM-001 and FPGA-SDRAM-003, adapted from paps-g `41a2fe2f`
and `3abb5c6c`. The original 100 MHz design sampled DQ through
combinational routing five nominal nanoseconds after a memory clock
edge. The SDRAM can take six nanoseconds to drive valid data. The
port uses I/O registers, moves the forwarded clock to 6.5 ns, and
captures read data on that clock before transferring it into the
main clock domain. The cache-valid window follows the captured
four-word burst by one register delay.

Samsung K4S511632D, Rev. 1.0 (November 2005), §16.0, PDF p. 13
gives the CL2 values for `tSAC`, `tOH`, `tSS` and `tSH` used in
the project constraints. [Micron 512Mb SDRAM Rev. Q, Figure 13,
printed p. 39](https://datasheet.lcsc.com/datasheet/pdf/9cb5539f5d98c9bea79785026615c4e1.pdf?productCode=C20519822)
shows the CL2 word boundaries. The pin fixture places the first
word at T1 plus access delay, then marks the uncertain interval
before each next word.

```sh
python3 -B -m unittest discover -s tests -p 'test_fpga_*.py' -v
```

Set `SAROO_FPGA_SDRAM_SOURCE` to the parent checkout's `FPGA` directory
and select `-k access_time` in `test_fpga_sdram_capture.py` to reproduce
the four original failures. The port receives all four words in order
at each delay from 3 through 6 ns. An added validity stage drops the
first word. The 35 adjacent FPGA methods retain their cache, reset,
geometry, arbitration and refresh results.

The [offline Quartus build](../tools/README.md#original-fpga-project-comparison)
packs 16 input and 54 output-side registers and passes all 30 reported
timing checks. Minimum setup and hold slack are +0.578 ns and +0.159 ns.
The fit uses 1,510 logic elements, 883 registers, 32,768 memory bits,
150 pins and one PLL. PCB skew is outside this fit. A historical
development image with the excess validity stage failed physical
post-eviction reads. A later development build with the corrected
burst alignment passed scoped register, cache and SD-transfer checks.
Physical qualification for the public bitstream remains open.

## Keep MCU halfwords coherent across the FMC bus

FPGA-FMC-001 through FPGA-FMC-003 and the throughput follow-up,
adapted from paps-g `bcd2ae55`, `f491d0ee`, `f1f20c0e` and
`44fe8abd`. The former top level asserted NWAIT only after NADV
had reached its request logic. A single destination register then
fed the write and read paths. The port asserts NWAIT directly
from NEx or the active NADV phase. It re-arms on every NADV phase,
including a second halfword with NEx held low.

The NADV latch sends a toggle through two preserved registers.
The address settles in parallel, and the admission edge captures
the data and byte lanes for downstream consumers. Register and
FIFO operations use the same admission edge as the RAM request.
For RAM, the pin releases on the cache-data or accepted SDRAM
write edge. The public port keeps this early completion signal
separate from the ordinary cache wait output and preserves the
existing FC1 refresh priority and address map.

[ST RM0433 Rev. 8, §22.5, printed pp. 800–801, Figures 110–113,
printed pp. 828–831](https://www.st.com/resource/en/reference_manual/rm0433-stm32h743-753-and-stm32h750-value-line-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf#page=800)
describes split AXI transfers, held NEx, multiplexed NADV and
asynchronous NWAIT. [ST DS12556 Rev. 8, Tables 155–158, printed
pp. 244–245](https://www.st.com/resource/en/datasheet/stm32h750ib.pdf#page=244)
lists the separate read and write NWAIT timing limits.
[Intel Quartus Prime Standard Edition User Guide, Metastability
Analysis](https://www.intel.com/content/www/us/en/docs/programmable/683068/18-1/metastability-analysis.html)
describes destination-register synchronizer chains.

```sh
python3 -B -m unittest discover -s tests -p test_fpga_fmc_stream.py -v
```

Set `SAROO_FPGA_FMC_SOURCE` to the parent checkout's `FPGA`
directory and use `-k grouped` to reproduce its late NWAIT.
The pin-level fixture passes 30 streams across ten relative
clock phases, single and grouped halfwords, byte masks, register
reads and Saturn refresh contention. Each stream checks 576
SDRAM writes, 67 read requests, and the exact write release edge.
The grouped access and late completion controls both fail as
expected. Modeled write rates are 13.20–14.19 MB/s at the selected
20/30 ns return tails.

All five offline Quartus stages and 39 reported timing checks
pass. The first fit missed the 5 ns registered NWAIT release
constraint by 0.150 ns. Pin-adjacent placement from the original
correction gives +0.210 ns with the same constraint. Minimum
reported hold slack is +0.163 ns. The fit uses 1,548 logic elements,
928 registers, 32,768 memory bits, 150 pins and one PLL.
The earlier development image with the full FMC repair passed
scoped cold boot, register/cache, and ten SD-transfer checks.
Model rates and fitted paths do not measure board timing or
qualify the public bitstream.

## Capture Saturn writes on the A-bus data phase

FPGA-ABUS-001, adapted from paps-g `9ba50fc5`. The former FPGA
started a write at a fixed delay from chip select. The Saturn
address, write data and strobes could be in different phases at
that point. The port arms on chip select, synchronizes TIM1
through two preserved registers, and takes one stable snapshot
of the address, data, selects and byte lanes. BCR and CD registers,
interrupts and FIFO writes consume that snapshot. A queued SDRAM
write keeps its own address, data and lane mask until arbitration.
The public FPGA05 address map retains its existing 1 MiB RAM
alias, and the Saturn read path keeps its existing timing.

[Sega SCU User's Manual ST-097-R5-072694, §3.6 and Figure 3.29,
printed p. 64](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=80)
describes external WAIT sampling.
[Sega Technical Bulletin 47, §§7–8, pp. 2–3](https://antime.kapsi.fi/sega/files/ST-TECH-47.pdf#page=2)
describes byte/word/longword writes and fixed waits for the official
RAM cartridge. The [original SAROO board schematic](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/HW/ssmaster_v2.PDF)
shows the bus wiring. [TI SN74LVC16245A, SCES062Q, §7.6, p. 9](https://www.ti.com/lit/ds/symlink/sn74lvc16245a.pdf#page=9)
specifies up to 4 ns propagation at 3.3 V ±0.3 V from −40 to
85 °C.

```sh
python3 -B -m unittest discover -s tests -p test_fpga_abus_write.py -v
```

Set `SAROO_FPGA_ABUS_SOURCE` to the parent checkout's `FPGA`
directory and select `-k data_phase` to reproduce the write with
no TIM1 event. The complete top-level fixture covers ten relative
clock phases, both TIM1 idle levels and 0/4 ns input skew. Each
of the 40 runs checks 13 writes, including the command registers,
HIRQ, FIFO and masked SDRAM writes with both RAM maps. Restoring
the live address makes all ten arbitration-delay controls issue to
the wrong SDRAM address. The normal and delayed cases use the
same register and SDRAM command logic.

The offline Quartus build passes all five stages and 39
reported timing checks. Minimum setup and hold slack are
+0.197 ns and +0.164 ns. The 8 ns bundle constraint has at
least +5.867 ns reported slack. The fit uses 1,666 logic
elements, 1,061 registers, 32,768 memory bits, 150 pins and one
PLL. The fixture uses a digital model and a bounded arbitration
delay. PCB behavior, RAM-cartridge game timing and the public
bitstream's hardware qualification remain open.

## Set Saturn data direction before chip select

FPGA-ABUS-002, adapted from paps-g `92f619b0`. The original FPGA
selected the direction toward Saturn whenever both write strobes
were idle. That left the transceiver pointing toward Saturn until
the write strobe arrived at the same time as chip select. The port
defaults to Saturn-to-FPGA direction and changes to FPGA-to-Saturn
on the early RD signal. Either asserted write lane takes priority.
Chip select, refresh isolation and native CD passthrough keep the
existing output-enable behavior.

The [original SAROO schematic, U3](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/HW/ssmaster_v2.PDF)
shows how DIR and active-low OE select the two sides.
[TI SN74LVC16245A, SCES062Q, §9.4 Table 3, p. 11](https://www.ti.com/lit/ds/symlink/sn74lvc16245a.pdf#page=11)
defines the transceiver direction and isolation truth table.
The [SAROO timing notebook](DEVELOPMENT_EN.md#reading-the-historical-technical-notebook)
places RD one A-bus clock ahead of chip select.

```sh
python3 -B -m unittest discover -s tests -p test_fpga_data_direction.py -v
```

Set `SAROO_FPGA_DIRECTION_SOURCE` to the preceding checkout's FPGA
directory to reproduce the idle-direction failure. The port passes
13 cases, covering reads, both byte lanes, whole-word writes, refresh,
CD passthrough and overlapping strobes. Six adjacent A-bus, FMC
and timer methods pass.

The slow/hot Quartus report gives 6.390 ns from RD to DIR and
7.045 ns on the slowest constrained DIR input against a 12 ns
limit. The fit uses 1,647 logic elements and 1,061 registers,
with minimum setup and hold slack of +0.197 ns and +0.168 ns.
The transceiver datasheet gives no numeric DIR turnaround maximum.
Board-level direction settling and game behavior remain unqualified.

## Quiet the Saturn bus during reset and retain CD state

FPGA-RESET-001 and FPGA-RESET-003, adapted from paps-g `5e95a7cc`
and `bd0aab2e`. The previous public FPGA ignored cartridge SYSRES.
The port synchronizes it through two registers, isolates the data
bus, releases WAIT, masks the Saturn IRQ pin and retires incomplete
write captures. The select and TIM1 samplers keep observing the bus
so a select held across the pulse cannot start a new transfer on release.
SDRAM refresh and the MCU interface continue to run.

CKCHG320/352 use the same cartridge reset wire while the CD block and
work RAM retain their state. The final behavior keeps the CD selection,
cartridge map, FIFO, command and response registers, and retained mailbox.
The MCU can finish an accepted command while the Saturn bus is isolated.
Full FPGA reset and explicit FIFO reset still clear their own state.
The board has no separate CDRES input. Full-system software can still
initialize the emulated CD through its ordinary command interface.

[Sega SMPC User's Manual ST-169-R1-072694, SYSRES and CKCHG,
printed pp. 29–31](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=39)
describes whole-system reset, clock changes and their different CD retention
rules. The [SAROO schematic, FSYSRES](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/HW/ssmaster_v2.PDF)
shows the cartridge reset input used here.

```sh
python3 -B -m unittest discover -s tests -p test_fpga_clock_retention.py -v
```

Set `SAROO_FPGA_RESET_SOURCE` to the preceding checkout's FPGA directory
and select `-k quiets` to reproduce the active bus outputs during SYSRES.
The full-top fixture passes three modeled pulses at each of three phases,
covering retained mapping and CD state, live MCU completion, a write
straddling the pulse and full FPGA reset. A control that resets the mapping
fails the retention check. Eight FC1 tests retain their timing, and the
adjacent FMC, A-bus, mailbox, timer, direction and HIRQ checks pass.
The complete offline Quartus fit uses 1,677 logic elements and 1,063
registers. Minimum reported setup and hold slack are +0.197 ns and
+0.162 ns.
Historical Astal observations established the need to preserve the CD
mapping. They do not qualify the public FPGA on a physical board.
