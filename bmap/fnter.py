#!/usr/bin/env python3
import sys
import freetype
import string
import math
import os.path
import struct
from functools import reduce
import itertools

CHARS = string.ascii_letters + string.digits + string.punctuation + " "
CHAR_NUMS = [ord(x) for x in CHARS]

FLAG_COMPRESSED = 1
FLAG_ITALIC = 2
FLAG_BOLD = 4
FLAG_KERNDAT = 8

if len(sys.argv) not in [4, 5]:
    print("Usage: {} <font file> <font size in pixels> <output path> [flags]".format(sys.argv[0]))
    print("       where flags is a combination of:")
    print("          c -- compressed")
    exit(1)
else:
    face_name = sys.argv[1]
    size_pixels = int(sys.argv[2])
    output_path = sys.argv[3]
    flags = sys.argv[4] if len(sys.argv) == 5 else ""

    compressed = "c" in flags


face = freetype.Face(face_name)
face.set_pixel_sizes(0, size_pixels)

def get_character_as_bool_array(c):
    """
    Render character c

    Result contains bitmap data in a 2d bool-valued list
    """
    global face

    face.load_char(c, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)

    bitmap = face.glyph.bitmap

    result = []
    buffercopy = bitmap.buffer

    if bitmap.pitch == 0 or bitmap.rows == 0 :
        return [[]]

    for i in range(0, bitmap.rows * bitmap.pitch, bitmap.pitch):
        arr = []
        seen = []
        for j in range(bitmap.width):
            byte, bit = divmod(j, 8)
            bit = 7 - bit
            if byte not in seen:
                seen.append(byte)
            arr.append(buffercopy[i+byte] & (1 << bit) != 0)
        result.append(arr)
    
    return result

def get_metrics(c):
    """
    Return metrics for character c in pixels:

    advance, bearingX, bearingY
    """
    global face

    face.load_char(c, freetype.FT_LOAD_TARGET_MONO)
    return [
            int(face.glyph.metrics.horiAdvance / 64.0),
            int(face.glyph.metrics.horiBearingX / 64.0),
            int(face.glyph.metrics.horiBearingY / 64.0)
    ]

def round_away_from_zero(x):
    a = abs(x)
    r = math.floor(a) + math.floor(2 * (a % 1))
    return r if x >= 0 else -r

def calc_kerning(c):
    """
    Create kerning data for character c as first character
    """
    global face
    
    entries = []
    for other in sorted(CHAR_NUMS):
        kerning = face.get_kerning(c, chr(other))
        pix = int(round_away_from_zero(kerning.x / 64))
        if pix == 0:
            continue
        else:
            entries.append((ord(c), other, pix))
    return entries

def convert_bitmap(bitmap, compress, bytelength=-1):
    """
    Convert bitmap to bytes (optionally compressing it)

    returns (compress = 0? data)
            (compress = 1? data bytelength newwidth)
    """

    width = len(bitmap[0])

    if compress and bytelength == -1:
        options = [[convert_bitmap(bitmap, True, 0), 0, len(bitmap[0])]]

        def appendoption(width, bitmap):
            nonlocal options
            for bl in range(width):
                if width % (bl + 1) != 0:
                    continue
                options.append((convert_bitmap(bitmap, True, bl+1), bl+1, width))

        appendoption(width, bitmap)
        
        # create one extended if width % 2 == 1
        if width % 2 == 1:
            newbitmap = []
            for i in bitmap:
                newbitmap.append(i[:] + [0])
            appendoption(width + 1, newbitmap)

        value = min(options, key=lambda x: len(x[0]))

        return value

    height = len(bitmap)

    if not compress:
        stride = (width // 8 + 1) if width % 8 != 0 else width // 8
        data = [0 for x in range((stride) * height)]

        for y in range(height):
            for x in range(stride):
                data[y*(stride) + x] = reduce(lambda x, y: (x << 1) | int(y), reversed(bitmap[y][x*8:(x+1)*8]), 0)
        
        return data
    
    # Try and compress
    bits = []

    if bytelength > 0:
        for y in range(height):
            if y != 0 and bitmap[y] == bitmap[y-1]:
                bits.extend((0, 0, 1))
            else:
                possible = []
                usefull = True
                for x in range(0, width, bytelength):
                    segment = bitmap[y][x:x+bytelength]
                    if y != 0 and segment == bitmap[y - 1][x:x+bytelength] and sum(int(x) for x in segment) != 0:
                        possible.extend((0, 0, 0))
                        usefull = False
                    else:
                        if sum(int(x) for x in segment) == 0:
                            possible.extend((1, 1))
                            usefull = False
                        else:
                            possible.extend((0, 1))
                            possible.extend(int(x) for x in segment)
                if usefull:
                    bits.extend((1, 0))
                    bits.extend(int(x) for x in bitmap[y])
                else:
                    bits.extend(possible)
    else:
        bits.extend((0, 0))
        for i in bitmap:
            bits.extend(i)

    while len(bits) % 8 != 0:
        bits.append(0)  # pad
        
    return [reduce(lambda x, y: (x << 1) | y, bits[v:v+8], 0) for v in range(0, len(bits), 8)]

# metrics about the bitmaps
metrics = {}
bitmaps = {}

# generate the data arrays
for i in range(256):
    if i in CHAR_NUMS:
        img = get_character_as_bool_array(chr(i))
        bitmaps[i] = img
        metrics[i] = len(img[0]), len(img), int(math.ceil(len(img[0]) / 8))

for i in range(256):
    if i not in metrics:
        continue
    j = list(metrics[i])
    j.extend(get_metrics(chr(i)))
    metrics[i] = j

# full table of kerning
table = []
if face.has_kerning:
    for i in sorted(CHAR_NUMS):
        table.extend(calc_kerning(chr(i)))

# convert all bitmaps
for i in CHAR_NUMS:
    if compressed:
        bitmaps[i], metrics[i][2], metrics[i][0] = convert_bitmap(bitmaps[i], True)
    else:
        bitmaps[i] = convert_bitmap(bitmaps[i], False)

# link all the data together

# start by creating the datablob
bitmap_ptrs = {} # offset from the dataptr
datablb = bytearray()
for i in CHAR_NUMS:
    bitmap_ptrs[i] = len(datablb)
    metric = metrics[i]
    datablb.extend(struct.pack("<BBBbbb", *metric))
    datablb.extend(bitmaps[i])

# finally, create the kerning blob
kernblb = bytearray()
for i in table:
    kernblb.extend(struct.pack("<BBb", *i))

payloadblb = bytearray() + kernblb  # create a copy
dataptr_base = len(payloadblb) + 0xC + 512 # length of dataptr table
payloadblb += datablb

# create entire file
payload = bytearray()

payload += "MFnt".encode("ascii")

flags = 0
if compressed:
    flags |= FLAG_COMPRESSED
if face.has_kerning:
    flags |= FLAG_KERNDAT

# todo italic

payload += struct.pack("<BxHHH", flags, 0xC, len(table), 0xC + 512)

# create ptrtable

dataptrtable = bytearray()
for i in range(256):
    dataptrtable.extend(struct.pack("<H", dataptr_base + bitmap_ptrs[i]) if i in bitmap_ptrs else (0, 0))

payload += dataptrtable
payload += payloadblb

with open(output_path, "wb") as f:
    f.write(payload)
