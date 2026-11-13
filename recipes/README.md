# Reviewed SD sector recipes

Compile a recipe against your matching, untouched local CUE/BIN and copy the
resulting SOV to `/SAROO/PATCHES/`. The compiler checks the complete source
FILE hash and the original target bytes. Select recipes individually while
physical qualification is pending.
[Compiler commands](../tools/README.md#sd-sector-overlay-compiler).

The following recipes adapt paps-g `5d5cd0e8` and the original SAROO menu
callbacks. Each JSON retains its upstream source URL and explains its edits.
Pocket Fighter includes the complete `cdf6dba9` correction. Shichuu uses the
later `3fafb587` source review. Public verification compares sector output
and callback memory operations with synthetic data.
[Source checks, fallback guards and limits](../doc/SD-SECTOR-OVERLAYS.md#stock-callback-recipes).

| Recipe | Inherited operation |
| --- | --- |
| [Amagi Shien JP](amagi-shien-jp-sector.json) | One instruction NOP. |
| [Cotton 2 JP](cotton-2-jp-sector.json) | Skip the ASR0 store. |
| [Cotton Boomerang JP](cotton-boomerang-jp-sector.json) | One instruction NOP. |
| [Dragon's Dream JP](dragons-dream-jp-sector.json) | One peripheral-state byte. |
| [Dungeons & Dragons Disc 2 JP](dungeons-dragons-disc2-jp-sector.json) | Two instruction NOPs on the matching disc. |
| [Final Fight Revenge JP](final-fight-revenge-jp-sector.json) | One instruction NOP. |
| [Friends game disc JP](friends-game-disc-jp-sector.json) | Two instruction NOPs on the matching disc. |
| [Game no Tatsujin 2 JP](game-no-tatsujin-2-jp-sector.json) | The guarded instruction NOP. |
| [Gotha JP](gotha-jp-sector.json) | One NOP and a byte edit. |
| [Gundam Zeta Zenpen JP](gundam-zeta-zenpen-jp-sector.json) | One IP instruction byte. |
| [Hebereke's Popoitto EU](hebereke-popoitto-eu-sector.json) | The EU status comparison byte. |
| [Hissatsu Pachinko Rev A JP](hissatsu-pachinko-jp-reva-sector.json) | The matching revision's status comparison byte. |
| [Manx TT USA](manx-tt-superbike-us-sector.json) | Six bytes in the US branch, selected by the full IP revision. |
| Noel 3 JP [Disc 1](noel-3-disc-1-jp-sector.json), [Disc 2](noel-3-disc-2-jp-sector.json), [Disc 3](noel-3-disc-3-jp-sector.json) | One NOP per disc. Use a three-source bundle for their shared ID. |
| [Pocket Fighter JP](pocket-fighter-jp-sector.json) | The full twelve-byte cartridge initialization replacement, its literal guard and the NOP. |
| [Shichuu Suimei Pitagraph JP](shichuu-suimei-pitagraph-jp-sector.json) | The guarded status comparison byte. Startup timing remains open. |
| [Street Fighter Zero 3 JP](street-fighter-zero-3-jp-sector.json) | One instruction NOP. |
| [Super Real Mahjong P7 JP](super-real-mahjong-p7-jp-sector.json) | One instruction NOP. |
| [Tamagotchi Park JP](tamagotchi-park-jp-sector.json) | One replacement instruction. |
| [Whizz EU](whizz-eu-sector.json) | One IP instruction byte. |

Original compatibility behavior is attributed to the source callbacks.
Underlying game symptoms are documented only where the retained evidence
identifies them. Existing Terra Cresta 3D and UMK3 pilot sources remain
available in this directory.
[Original SAROO source](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).

The expansion RAM group adapts paps-g `4ac082e1`. These eight sources select
1 MB and preserve the corresponding inherited instruction writes. An explicit
user cartridge configuration takes precedence.
[Cartridge handoff and pairing](../doc/SD-SECTOR-OVERLAYS.md#recipe-cartridge-setup).

| Recipe | Inherited operation |
| --- | --- |
| [Fighter's History Dynamite JP](fighters-history-dynamite-jp-sector.json) | 1 MB and four NOPs. |
| [KOF '96 JP](kof96-jp-sector.json) | 1 MB and one NOP. |
| [Metal Slug Rev A JP](metal-slug-reva-jp-sector.json) | 1 MB and one NOP for this revision. |
| [Real Bout JP](real-bout-jp-sector.json) | 1 MB and two NOPs. |
| Real Bout Special JP [v1.001](real-bout-special-jp-v1001-sector.json) and [v1.002](real-bout-special-jp-v1002-sector.json) | 1 MB and one NOP per revision. Bundle the two one-sector alternatives under their shared product key. |
| [Samurai Spirits 3 JP](samurai-spirits-3-jp-sector.json) | 1 MB and two NOPs. |
| [Samurai Spirits 4 JP](samurai-spirits-4-jp-sector.json) | 1 MB and one NOP. |

[Waku Waku 7 JP](waku-waku-7-jp-sector.json) adapts paps-g `4d1a613b` and
the later `e2029f04` linked-reader review. It includes the full 40-byte
routine, the other three writes and 1 MB setup. The routine depends on the
existing SAROO sector-reader pointer. Physical qualification remains open.
[Reader dependency and historical comparison](../doc/SD-SECTOR-OVERLAYS.md#waku-waku-7).
