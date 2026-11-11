
# SEGA Saturn SAROO Cover Tool

This tool is designed to work with SEGA Saturn cover images stored in SAROO's `cover.bin` binary format.

The tool can operate in 3 modes: unpacking, packing, and disc hash.

## Unpacking

In unpacking mode, it reads the `cover.bin` file, extracts each cover image, and saves it as a BMP file.

## Packing

In packing mode, it reads a list of BMP files from a `.txt` file, generates a cover index, and writes the cover images into a new `cover.bin` file.
The cover images are stored in a specific RAW format, including a palette and pixel data.
When packing, the tool also detects duplicate images to reduce output size.

## Game Disc Hash

To identify Saturn games and match the covers, SAROO uses an [Adler32 hash](#ipbin-hash) of the disc's data.

You can use SAROO Cover Tool to generate the correct disc Hash for your Saturn game image.

## Usage

```
Usage: ./covertool -p coverlist.txt | -u cover.bin | -i disc-image

    -u          Unpack a cover.bin file to BMP images and coverlist.txt
    -p          Pack listed BMP images and generate cover.bin
    -i          Generate IP.BIN Hash for a Saturn game disc image

Example:
    ./covertool -u cover.bin
    ./covertool -p coverlist.txt
    ./covertool -i SaturnGame.bin
```

## Image Format

Cover images should follow these specifications:
- BMP format (8-bit, uncompressed)
- 256 colors palette
- Width 1–128, height 1–192 (usual covers are 128x128 or 128x192)
- Windows DIB sizes 40, 52, 56, 108 or 124; padded and top-down rows supported

Operations fail on malformed/incomplete inputs. Packing stages `cover.bin`;
extraction validates metadata first, publishes complete BMPs and writes the
manifest last. Existing files are preserved on prepublication failure. Earlier
completed BMPs can remain after a later extraction failure. New files use mode
0600; protected destinations, special files, output symlinks and active-input
aliases are refused.

List identities use the basename `cover_SERIAL~12345678.bmp`, with 1–12 serial
bytes. Export escapes unsafe/lowercase bytes as `%HH`; packing decodes them.
The IP serial/checksum identifies artwork, independently of the disc filename.
See [native safety, citations, validation and limits](../../doc/NATIVE-COVER-SAFETY.md).

## Build

```sh
make
```

## Technical Details

The cover images are stored in a binary file named `cover.bin`, which contains an index of cover images.
Each cover image is represented by a header structure that contains metadata about the image,
such as the serial ID, image offset, dimensions, and a hash value.

The index header structure is defined as:

```c
typedef struct cover_header
{
	char serial_id[12];
	uint32_t ip_hash; // disc IP.BIN adler32 hash
	uint32_t img_offset;
	uint16_t width;
	uint16_t height;
	char unused[8];
} cover_hdr_t;
```

The host index and MCU lookup both support up to 16,384 entries. The MCU reads
the first 2,048 entries at startup and loads later pages as needed.
[Firmware lookup and compatibility](../../doc/COVER-INDEX-CAPACITY.md)

### IP.BIN Hash

To identify Saturn game images, SAROO uses an [Adler32 hash](https://github.com/tpunix/SAROO/blob/65d8764c24aa3a55f6c57f05c45b97f550d1fb49/Firm_MCU/Saturn/saturn_utils.c#L702) of the disc's IP.BIN data.

The hash is calculated using 64 bytes from the IP.BIN data, starting at offset `0x30`. For example:

```c
hash = adler32(ip_bin+0x30, 64);
```

## Credits

SAROO Cover Tool by [Bucanero](https://github.com/bucanero) - Licensed under GNU GPLv3

Safety maintenance: paps-g (`2231f4b5`, protected-destination refinement `3151c6f5`),
ported as direct public source changes. Original Bucanero GPL-3.0-or-later notice
remains in `main.c`.
