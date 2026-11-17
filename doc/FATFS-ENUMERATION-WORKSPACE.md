# Optional FatFs enumeration workspace

Adapted from paps-g private commit `49a1c53d` (AUD-RES-033). The original
contribution identified that `f_readdir` needs either the FAT long-name
workspace or the exFAT entry-set workspace for one mounted volume, not both
at once. This public port changes only that function's local storage when
`FF_USE_LFN == 2 && FF_FS_EXFAT`. Both FatFs pointers refer to the same
aligned union, and the same volume-grant, rewind, output, failure and
directory-advance paths remain in place. Pathname operations still use
their separate buffers because they can need both representations.

The selected [FatFs configuration comments](../Firm_MCU/FatFS/ffconf.h)
define mode 2 as stack-backed long-name storage and state the two workspace
sizes. The [provider's macros and `f_readdir` body](../Firm_MCU/FatFS/ff.c)
define the exact capacities and use sites. The standard App uses
`FF_USE_LFN=1` and `SAROO_FAT32_ONLY=1`; this branch is therefore absent
from its compiled body. The optional mode is retained for a future
stack-LFN/exFAT profile, without enabling that profile today.

## Reproduction and result

The public parent is `c3d92a0`. The adapted
[host fixture](../tests/fatfs_enumeration_workspace.c) uses the real FatFs
implementation and a file-backed block device. It injects a workspace
probe immediately before `DIR_READ_FILE`, poisons the inactive pointer,
and checks alignment, information-object canaries, ordinary files,
long-name limits, two directory owners, reads after pathname lookups,
rewind, end-of-list, read failures, corruption refusal/fallback and
recovery. The [runner](../tests/test_fatfs_enumeration_workspace.py) copies
the source and records exact inputs and outputs in an ignored build folder.

```sh
git show c3d92a0:Firm_MCU/FatFS/ff.c > /tmp/saroo-ff-parent.c
SAROO_ENUM_SOURCE=/tmp/saroo-ff-parent.c SAROO_ENUM_EXPECT_SHARED=0 \
  SAROO_ENUM_OUTPUT=tests/build/fatfs-enum-parent \
  python3 tests/test_fatfs_enumeration_workspace.py -v
SAROO_ENUM_EXPECT_SHARED=1 \
  SAROO_ENUM_OUTPUT=tests/build/fatfs-enum-selected \
  python3 tests/test_fatfs_enumeration_workspace.py -v
```

Both runs pass three methods and their 12 formatted-media subcases. Their
12 result records are identical: 3,620 workspace checks and 122 injected
read failures in each run. An optional Cortex-M7 `-Os -fstack-usage`
compile with `SAROO_FAT32_ONLY=0`, `FF_USE_LFN=2` and
`FF_FS_REENTRANT=1` measures the direct `f_readdir` frame at 1,152
bytes in the parent and 640 bytes in the port, saving 512. This is a
direct frame measurement, not a whole-task stack bound.

The standard GNU App compile/link check succeeds with all 166 vector
slots and a 59,344-byte load image. Its flat image SHA-256 remains
`d4770c0eda8082a82c30b01b16e24350cf18c0fdb1460e987567de5aaa8c29b8`,
exactly the pre-port image. Nine existing directory/unlink methods
also pass. The GNU build is qualification only; the original MDK image
and physical Saturn behavior were not tested, and this optional exFAT
mode is not enabled in standard firmware.
