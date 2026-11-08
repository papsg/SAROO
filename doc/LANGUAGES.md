# Additional menu languages

IDs 0–13 keep their v0.8 meanings. The usual L-button language cycle includes
the additions below. Select an ID with `lang_id` in the existing `[global]`
section of `/SAROO/saroocfg.txt`; no MCU or FPGA change is required.

| ID | Language | Original contribution |
| --- | --- | --- |
| 14 | European Portuguese | [#256](https://github.com/tpunix/SAROO/pull/256), Hudson Brendon |
| 15 | Welsh / Cymraeg | [#299](https://github.com/tpunix/SAROO/pull/299), Widge-5 |

Each addition supplies all twenty fixed menu labels and uses the existing Latin
font. This fork preserves the original hash-table bounds fix and uses bounded
error formatting before adding translations. Original unrelated reformatting
and incorrect proposed configuration IDs are not imported.

`make -C tests check` verifies lookup/cycling, complete translated errors and
title capacities. `make -C tests check-language-catalog` checks every new label's glyph coverage and pixel
advance/ink extent against the shipped font and the 288-pixel cleared status
width. These are source/host checks, not physical menu or game qualification.
Independent native-speaker editorial review remains outstanding.
