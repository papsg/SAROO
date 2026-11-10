# Host regressions

Run the menu language regression with a C99 compiler supporting AddressSanitizer
and UndefinedBehaviorSanitizer:

```
make -C tests check-language
```

The test includes the actual `Firm_Saturn/language.c`, supplying only fixed-width
platform types and the standard string library. It checks all fourteen existing
language IDs and twenty labels, copied lookup keys, repeated initialization,
language cycling and an unknown-label control. The v0.8 source fails during
hash-table initialization under the sanitizers.

The bounds correction originates in
[calebshub's original PR #377](https://github.com/tpunix/SAROO/pull/377).
The accompanying array-element count preserves the fourteen-entry count on SH-2
and lets the same module run on a 64-bit host. This portability change is not a
separate claim of a Saturn defect. Tests do not qualify physical menu behavior.

## FPGA interrupt clear regression

Install Icarus Verilog (`iverilog` and `vvp`) alongside the Rust toolchain.
`make -C tests check-host-tools` includes the FPGA checks. Run them alone with:

```sh
cargo test --locked --manifest-path tools/Cargo.toml -p firmware-build --test hirq
```

The Rust test extracts the actual HIRQ declarations/always block unchanged from
`FPGA/SSMaster.v`. The independent per-bit Verilog oracle checks simultaneous
MCU/Saturn clears, same/different bits, multibit masks, existing SET priority,
invalid decodes, hold and asynchronous reset. Unique source/testbench hash
receipts, generated wrappers, compiler output and simulator logs remain under
`tests/build/fpga-hirq`; no runtime probe or hardware access is involved.

Reproduce the original clear/clear failure while retaining the valid controls:

```sh
mkdir -p tests/build
git show ba387b083bb9d2a001ae6a22a24d6e6785e515bd:FPGA/SSMaster.v > tests/build/hirq-before.v
SAROO_FPGA_HIRQ_SOURCE="$PWD/tests/build/hirq-before.v" \
  cargo test --locked --manifest-path tools/Cargo.toml -p firmware-build --test hirq
```

The baseline should fail the simultaneous-clear method and pass the unchanged
control method. This narrow correction deliberately retains FPGA05 MCU-SET
priority, including its separate lost-acknowledgement limitation. See the
[contract, citations, attribution and qualification limits](../doc/CD-DRIVE-SAFETY.md#fpga-hirq-001-combine-simultaneous-clears-on-the-original-fpga).
Host RTL checks do not qualify physical bus strobes, crossings or games.
