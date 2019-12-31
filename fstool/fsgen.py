#!/usr/bin/env python3
import os
import struct
import sys

VERSION = 1

FLAG_T_SYSTEM = 1
FLAG_T_PRIVATE = 4

if len(sys.argv) != 3:
    print("fsgen: No input dir and output bin")
    exit(1)

# Enumerate all the top-level folders

toplevel_names = []
for i in os.listdir(sys.argv[1]):
    if os.path.isdir(os.path.join(sys.argv[1], i)):
        if len(i) > 12:
            print("fsgen: Truncating too-long toplevel {} to {}".format(i, i[:12]))
        toplevel_names.append(i[:12])
    else:
        print("fsgen: Ignoring non-dir in toplevel {}".format(i))

if len(toplevel_names) > 16:
    print("fsgen: Too many top-level dirs!")
    exit(2)

print("fsgen: Creating {} filechains".format(len(toplevel_names)))

filechain_info = {
    name: [] for name in toplevel_names # array of (name, flags, length, contents, last)
}

for toplevel in toplevel_names:
    for fname in os.listdir(os.path.join(sys.argv[1], toplevel)):
        path = os.path.join(sys.argv[1], toplevel, fname)

        if os.path.isdir(path):
            print("fsgen: Ignoring nested directory {}".format(path))
            continue

        if len(fname) > 16:
            print("fsgen: Truncating too-long name {0}/{1} to {0}/{2}".format(toplevel, fname, fname[:16]))
            fname = fname[:16]

        with open(path, 'rb') as f:
            contents = f.read()

        
        if filechain_info[toplevel]:
            filechain_info[toplevel][-1][4] = False  # mark last
        filechain_info[toplevel].append(
                [fname, 0, len(contents), contents, True]
        )

# Create blob of resources
datablb = bytearray()
insertptr = 0x080E0000 + 0x188 # start of fs + size of header

filechain_ptr = {
}

for toplevel in toplevel_names:
    # Set the initial ptr
    print("fsgen: Starting filechain {} at 0x{:08x}".format(toplevel, insertptr))
    filechain_ptr[toplevel] = insertptr if filechain_info[toplevel] else 0xFFFFFFFF
    for f in filechain_info[toplevel]:
        # Increment the insertptr by the size of this file
        # 0x38 header + length
        insertptr += 0x24 + f[2]

        # Align to 0x4
        if insertptr % 4 != 0:
            padding = 4 - (insertptr % 4)

            insertptr += padding
        else:
            padding = 0

        # Build a file object, and add it to the blb
        datablb.extend(struct.pack("<4s16sIHHII", b"fLeT", f[0].encode('ascii'), 0, f[2], f[1], 
            insertptr if not f[4] else 0xFFFFFFFF, 0xFFFFFFFF)
        )

        # Add file contents
        datablb.extend(f[3])

        # Add padding
        if padding:
            datablb.extend(0 for x in range(padding))

# Create the main header
header = struct.pack("<4sBBH", b"MSFS", VERSION, 1, # TODO set this properly
    (1 << len(toplevel_names)) - 1                 # set headers
)

blank_toplevel = bytearray(0 for x in range(0x18))
# Write out the final image, generating the TopLevels in the process

with open(sys.argv[2], 'wb') as img:
    img.write(header)

    # Write all toplevels
    for toplevel in toplevel_names:
        img.write(struct.pack("<4s12sIHH", b"fStL", toplevel.encode('ascii'), filechain_ptr[toplevel], 0, # TODO set flags
            0xFFFF
        ))

    # Add padding
    for i in range(16 - len(toplevel_names)):
        img.write(blank_toplevel)

    # Write datablb
    img.write(datablb)

print("fsgen: Wrote image to be flashed at 0x080e0000")
