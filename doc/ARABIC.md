# Arabic fixed menu labels

This fork adapts [Array in a Matrix's PR #107](https://github.com/tpunix/SAROO/pull/107),
reviewed at [`0b6c4ea6`](https://github.com/tpunix/SAROO/commit/0b6c4ea60b4a3b8eaa10a3e02efaf2bccd81e77c).
Arabic is language ID **18**, after the existing IDs 0–17. Use `lang_id=18`
in the existing `[global]` config section, or the usual L-button language cycle.
This is an adapted feature, not a newly reproduced game or audio defect.

The original proposal has nineteen strings and selects ID 9, which is Italian
in v0.8. Neither baseline font contains its Arabic letters. The original
renderer also consumes source-order codepoints without joining or bidirectional
layout, as noted in the [original discussion](https://github.com/tpunix/SAROO/pull/107#issuecomment-2097420023).
Copying the original table unchanged therefore cannot provide a complete
readable menu. This adaptation supplies all twenty labels and the needed glyphs.

## Corrections and scope

The logical source is [`arabic.json`](../tools/menu-assets/arabic.json).
The host asset compiler shapes the fixed unvocalized words, including lam-alef
ligatures, and writes visual-order UTF-8 presentation forms to the existing
`lang_ar` C table. Explicit numeric/Latin runs keep pagination, signed error
codes and `SAROO` left-to-right. The firmware uses its existing translation,
UTF-8 decoder, lookup and raster functions: no runtime shaping engine, cache,
allocation, extra pass or MCU/FPGA change is introduced.

Joining follows [Unicode 17, §9.2 Arabic, contextual shaping and ligatures](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-9/#G20596).
Authored run ordering is deliberately scoped to these fixed labels; it is not
an implementation of [Unicode Bidirectional Algorithm UAX #9](https://www.unicode.org/reports/tr9/).
Arabic filenames, arbitrary bidirectional editing, diacritics and joiner controls
are outside this feature. Unsupported build inputs fail explicitly. Existing
left anchors, row geometry and navigation remain; the whole UI is not mirrored.

Wording retains the proposal's intent, with these editorial adaptations:

| Purpose | Adaptation in this fork |
| --- | --- |
| Selection | Consistent imperative `اختر`; add the missing category label |
| Progress and failure | Distinguish starting a game from loading a file; retain full error codes |
| System player and serial terminal | Keep the system-disc action distinct from game-disc loading; describe serial debugging |
| Update | Retain the power warning and reboot instruction with complete wording |
| Title and missing disc | Correct the proposed title spelling and missing-disc construction |

These wording adaptations have not received independent native-speaker review.
The source intent is credited to Array in a Matrix; the shaping, additional
label, font integration, error-format adaptation and verification are this
fork's work, rather than changes attributed to the original author.

Both disc-type errors now format the entire translated message instead of
translating a prefix and appending the number. Slot 18 gets `%d` in every
language, preserving all previous complete messages while allowing Arabic to
place the full signed number. There is no language-specific runtime branch.
Prerequisites are the language bounds fix, bounded formatter/error storage,
the established ID sequence, and the preceding CJK font stage.

## Font source and resources

The 59 required glyphs come from drawings by **MansourSorosoro / Eternal Dream
Arabization**, attributed under OFL-1.1 in the pinned
[bitmapfont README](https://github.com/hajimehoshi/bitmapfont/blob/8cffbe61878377cb76a201780888de233557abbb/README.md).
The [source bundle](../tools/menu-assets/font-source/FONT-NOTICE.txt) preserves
the presentation-form atlas, original README, OFL-1.1 text, separate library
Apache-2.0 license and per-file provenance. The BDF declares `PIXEL_SIZE 12`
for the existing C converter; all original pixels, 6/12-pixel advances,
12 drawing rows and three-row top bearing are preserved.

All 10,736 preceding font records and the Latin font remain byte-for-byte
unchanged. The complete font has 10,795 records. The 59 additions match the
preserved independent record digest and add 295 index plus 921 record bytes.
No unrelated Romanian glyph or legacy font geometry repair is included.

| Resource | Before | Arabic | Change |
| --- | ---: | ---: | ---: |
| Linked code/data before BSS | 74,016 | 75,056 | +1,040 bytes |
| Executable text, including identical memory helpers | 38,468 | 38,448 | −20 bytes |
| Pooled strings | 15,154 | 16,130 | +976 bytes |
| Other read-only constants | 188 | 192 | +4 bytes |
| Initialized data | 20,192 | 20,272 | +80 bytes |
| BSS excluding reserved stack | 7,180 | 7,180 | 0 |
| Reserved stack | 16,384 | 16,384 | 0 |
| Appended font | 356,463 | 357,679 | +1,216 bytes |
| Complete padded menu image | 487,535 | 488,751 | +1,216 bytes |

The body stays padded to 128 KiB. The complete image leaves 35,537 bytes before
the 512 KiB menu/catalog boundary. Existing lookup and drawing work still applies
to every rendered character; no measured latency or whole-program stack bound is
claimed. There is no per-label SD read or new persistent workspace.

## Verification and limits

[`tests/test_arabic.py`](../tests/test_arabic.py) makes the checks reproducible:

- All 36 label/argument cases match a recorded independent reference from
  `arabic-reshaper` 3.0.0 plus `python-bidi` 0.6.7, with explicit LTR embeddings.
  Normal builds/tests do not need those oracle engines.
- Actual production translation, custom formatter, decoder and raster functions
  pass those 36 cases plus 648 previous-language cases under ASan/UBSan, with
  independent pixels and framebuffer guards. Earlier languages retain their
  existing clipping/replacement-glyph behavior; this does not repair or qualify
  every old glyph. New labels separately pass complete glyph and 288-pixel checks.
- All 8,496 pixels in the 59 selected atlas cells match independently decoded
  font records. Removing those records reconstructs the exact preceding font;
  the CJK stage still reproduces through the original C converter.
- Whole-asset regeneration is exact. Rust tests reject bad language IDs,
  unsupported marks/control input, format changes, corrupt/overlapping fonts,
  capacity overflow and input/output aliasing, and preserve unowned temporary
  files. The existing C converter's process-lifetime allocations have leak
  reporting disabled; address/undefined-behavior checks remain enabled.
- Nineteen language IDs pass 2,280 actual lookup/cycle checks. The three complete
  error paths pass 342 signed-code cases, using a source-extracted disc-error call and checking that both callers use
  the whole-message format; title and bounded-formatter controls also pass.

The host count-load adaptation models SH-2 big-endian access and is documented
in the shared renderer fixture. It does not change production code. Historical
private-tree results are reference evidence, not fresh qualification of this port.
Native-speaker editorial review, physical menu appearance, startup latency and
responsiveness remain unqualified. The Saturn was not accessed for this work.

## Build qualification

Both the preceding public `67c51f3` tree and this candidate compile and fully link
all sixteen menu units using GCC 13.2.0 / Yaul 0.3.1-3, in
`ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6`
(`linux/amd64`). Only Yaul's existing `memcpy`/`memset` and the GCC runtime are
needed; no diagnostic stub or private source is linked. This clears the earlier
available-toolchain missing-libc limitation for this assembled menu. Inherited
compiler/linker warnings remain; a successful build is not hardware acceptance.

Inside that environment, from `Firm_Saturn`:

```sh
export SOURCE_DATE_EPOCH=1765029659
tool_prefix=/opt/tool-chains/sh2eb-elf/bin/sh2eb-elf-
make "CC=${tool_prefix}gcc -mb" "AS=${tool_prefix}as" "OBJDUMP=${tool_prefix}objdump" "OBJCOPY=${tool_prefix}objcopy" \
  'LDFLAGS=-m2 -mb -nostartfiles -nostdlib -T ldscript -Wl,--oformat=elf32-sh,--build-id=none,-Map,ssfirm.map' \
  'LIBS=-lyaul -lgcc'
```

The menu's existing target-specific `-fno-builtin` formatter flag is retained.
The big-endian SuperH ELF, 128 KiB body, appended font and code/BSS/stack extents
are checked. Source, objects, ELF, image, map, symbols and host results are
preserved as local porting evidence. MCU/FPGA firmware and installed files are
not modified by this feature.
