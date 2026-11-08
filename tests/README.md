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
