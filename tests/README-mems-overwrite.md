# Host regressions

Run the MEMS overwrite regression with Python 3 and a C compiler supporting
AddressSanitizer and UndefinedBehaviorSanitizer:

```
python3 -B -m unittest discover -s tests -p 'test_mems_overwrite.py' -v
```

This compiles the actual native save-tool CLI and uses generated, disposable
8 MiB archives. It checks six payload sizes across inline and multi-block saves,
directory gaps, the final directory slot, inferred filenames, export contents,
dates, CRCs and preservation of every archive byte outside the intended edit.
A separate read-only export control passes on the unchanged v0.8 tool.

The correction originates in
[bucanero's original PR #372](https://github.com/tpunix/SAROO/pull/372).
An optional `SAROO_SAVETOOL_BINARY` path runs the same checks against an existing
binary, including a baseline control. Leak detection is disabled for the
original CLI's process-lifetime input buffers; bounds/undefined-behavior checks
remain enabled. These are host-utility tests, not physical Saturn acceptance.
