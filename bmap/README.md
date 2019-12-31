# `bmap` - resource file tools

These are similar to the tools located in SignCode, but instead output binary files and support slightly more options.

## `fnter` - "FoNT generatER"

Takes a font file and a size in pixels and outputs a binary file in the `MFnt` format.

The format begins with a header:

| Offset | Format | Description |
| ---- | -------| --------- |
| 0x00 | `"MFnt"` | 4-byte magic |
| 0x04 | u8 | Font flags |
| 0x06 | u16 | ptr to bitmaps/metrics |
| 0x08 | u16 | length of kern table (0 if not present, number of entries) |
| 0x0A | u16 | ptr to kern table (undefined if not present) |

The font flags are:

```
0000 1 10 1
|/// | || |
|    | || -- is compressed
|    | |-- is italic
|    | -- is bold
|    -- has kerning data  
-- unused
```

### Metrics

The metrics data is the following structure:

| Offset | Format | Description |
| ---- | -------| --------- |
| 0x00 | u8 | glyph width |
| 0x01 | u8 | glyph height |
| 0x02 | u8 | uncompressed: stride, compressed: "byte" length in bits (0 if using uncompressed encoding) |
| 0x03 | s8 | pen advance |
| 0x04 | s8 | bearingX |
| 0x05 | s8 | bearingY |

No alignment constraints are placed on the metrics data, and it is placed immediately before each bitmap.

### Uncompressed bitmap format
The bitmaps are stored left to right, top to bottom, though to simplify
the decoding logic the bits are reversed:

```
hgfedcba      kji
00000111 00000001  | number of bits = width; number of bytes = stride

srqponml      vut
00000001 00000010  | number of rows = height; size in bytes = stride * rows

abcdefghijk
lmnopqrstuv
```

### Compressed bitmap format

In compressed mode, the bitmaps are interpreted as a stream of prefix-encoded instructions.
The data should be read as a continuous stream of bits, and a buffer should be allocated
for the previously read row.

The first 2-3 bits make up a command, one of:

| Prefix | Meaning |
| :---: | --------- |
| 10 | Copy full row of data |
| 11 | Zero byte |
| 01 | Copy next byte of data |
| 001 | Repeat previous row |
| 000 | Duplicate previous row at current byte |
| 00 | (only as first command) Copy entire bitmap verbatim |

The process of reading command, then (optionally) reading its data continues until
the entire glyph is read, based on the data in the metrics table.

Additionally, the "byte" size can be redefined in the metrics table, for more optimal compression; although
it must be a factor of the row length.

### Kerning table

The kerning table is an array of the following structure:

| Offset | Format | Description |
| ---- | -------| --------- |
| 0x00 | u8 | First char |
| 0x01 | u8 | Second char |
| 0x02 | s16 | Pen offset |

The structure should be aligned to 2 bytes.

## `readfnt` - "READ FoNT"

This tool reads a font, and shows all of its internal data in an easy to see manner. This is mostly present for verifying the output of `testfnt`.
