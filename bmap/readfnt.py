#!/usr/bin/env python3
import struct
import sys
import itertools

FLAG_COMPRESSED = 1
FLAG_ITALIC = 2
FLAG_BOLD = 4
FLAG_KERNDAT = 8

if len(sys.argv) != 2:
    print("readfnt: No input!")
    exit(1)

def decode_uncompressed(width, height, stride, data):
    output = []
    for i in range(height):
        output.append([])
        for byte in data[i*stride:(i+1)*stride]:
            for j in range(8):
                if len(output[-1]) == width:
                    break
                output[-1].append(bool(byte & 1))
                byte >>= 1
    return output

def decode_compressed(width, height, wordlen, datao):
    bits = []
    rowbuffer = [] # organized in words

    ptr = [0, 8]
    def readbit():
        nonlocal ptr
        if ptr[1] == 0:
            ptr[1] = 8
            ptr[0] += 1
        ptr[1] -= 1
        res = (datao[ptr[0]] & (1 << ptr[1])) >> ptr[1]
        return res
    
    def readbits(l):
        return [readbit() for x in range(l)]

    sf = True  # start flag
    wi = 0 # word index

    disasm = ""
    def add(cmd, msg):
        nonlocal disasm
        disasm += f"({''.join(str(x) for x in cmd)}) {msg}\n"

    def genrow(data):
        x = []
        for i in range(0, width, wordlen):
            x.append(data[i:i+wordlen])
        return x

    if wordlen == 0:
        wordlen = 1

    while len(bits) < width*height:
        # read command
        cmd_1 = readbits(2)
        if sf and cmd_1 == [0, 0]:
            add(cmd_1, "UNCOMPRESSED_RAW")
            # boring decode
            output = []
            for i in range(height):
                output.append([bool(x) for x in readbits(width)])
            return output, disasm[:-1]
        elif cmd_1 == [0, 0]:
            # duplicate command
            cmd_2 = readbit()
            if cmd_2 == 1:
                # repeat row
                add((0, 0, 1), "REPEAT_ROW")
                for j in rowbuffer:
                    bits.extend(j)
            else:
                add((0, 0, 0), "REPEAT_WORD (@{})".format(wi))
                bits.extend(rowbuffer[wi])

                wi += 1
                if wi == width // wordlen:
                    wi = 0
                    rowbuffer = genrow(bits[-width:])
        elif cmd_1 == [1, 0]:
            data = readbits(width)
            add(cmd_1, "COPY_ROW [{}]".format(''.join(str(x) for x in data)))
            bits.extend(data)

            wi = 0
            rowbuffer = genrow(data)
        elif cmd_1 == [1, 1]:
            add(cmd_1, "ZERO_WORD")
            bits.extend(itertools.repeat(0, wordlen))

            wi += 1
            if wi == width // wordlen:
                wi = 0
                rowbuffer = genrow(bits[-width:])
        elif cmd_1 == [0, 1]:
            data = readbits(wordlen)
            add(cmd_1, "COPY_WORD [{}]".format(''.join(str(x) for x in data)))
            bits.extend(data)

            wi += 1
            if wi == width // wordlen:
                wi = 0
                rowbuffer = genrow(bits[-width:])

        sf = False

    # reshape
    output = [bits[x:x+width] for x in range(0, len(bits), width)]
    return output, disasm


with open(sys.argv[1], "rb") as f:
    # Try and get header
    try:
        magic = f.read(4)
        header = struct.unpack("<BxHHH", f.read(0x8))
    except EOFError:
        print("readfnt: EOF before header!")
        exit(2)

    if magic != b'MFnt':
        print("readfnt: Invalid magic")
        exit(3)

    compressed = bool(header[0] & FLAG_COMPRESSED)
    haskerning = bool(header[0] & FLAG_KERNDAT)

    datablb = f.read()
    kernlen = header[2]
    kernptr = header[3] - 0xC
    dataptr = header[1] - 0xC

    print(f"Got MFnt, {'un' if not compressed else ''}compressed, {'with' if haskerning else 'without'} kerning")
    print()
    # TODO: show italics

    if haskerning:
        print(f"Start kerning table with {kernlen} entries; at offset {dataptr:02x}")
        print("First   Last     Offset")
        print()

        for i in range(kernlen):
            first, last, offs = struct.unpack_from("<BBb", datablb, kernptr + i*3)
            print(f"{first:02x} ({chr(first)})  {last:02x} ({chr(last)})  {offs}")

        print()

    print(f"Start {'un' if not compressed else ''}compressed font data")
    print()
    for j, i in enumerate(range(dataptr, dataptr+512, 2)):
        offset = struct.unpack_from("<H", datablb, i)[0] 
        if offset == 0:
            continue

        wid, hgt, third, advance, bX, bY = struct.unpack_from("<bbbBBB", datablb, offset - 12)
        print(f"Entry {j} ({chr(j)}), @{offset:04X}")
        offset -= 12
        print(f" Glyph : {wid}x{hgt} {'wordlen' if compressed else 'stride'} = {third}")
        print(f" Metrics : advance = {advance}, bearing = [{bX}, {bY}]")

        if wid == 0 or hgt == 0:
            print(" No bitmap")
            continue

        if not compressed:
            data = decode_uncompressed(wid, hgt, third, datablb[offset+6:offset+6+(third*hgt)])

            print(" Bitmap :")
            for row in data:
                print("  " + ''.join('#' if x else '.' for x in row))
        else:
            data, log = decode_compressed(wid, hgt, third, datablb[offset+6:])

            print(" Disasm :")
            for i in log.splitlines():
                print("  " + i)

            print(" Bitmap :")
            for row in data:
                print("  " + ''.join('#' if x else '.' for x in row))
            

