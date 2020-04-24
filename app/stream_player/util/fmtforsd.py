#!/usr/bin/env python3
"""
Create a raw SD image from any file that ffmpeg can deal with.
This effectively wraps a couple of scripts together.

You can then write this out directly to an SD card with dd or another raw copy tool.

For example:

$ python3 fmtforsd.py test.mp3 test.img
<... stuff from ffmpeg bla bla bla ...>
$ sudo dd if=test.img of=/dev/sdX bs=4m

where X is the disk ID for the SD card (or if the SD controller is directly integrated to your motherboard, like on a laptop, something
like /dev/mmcblk0)
"""

import subprocess
import sys
import struct
import os
import shutil

if len(sys.argv) != 3:
    print("usage: {} <in> <out>".format(sys.argv[0]))
    exit(2)

FFMPEG = "/usr/bin/ffmpeg"

if not os.path.exists(FFMPEG):
    FFMPEG = shutil.which("ffmpeg")

if not os.path.exists(FFMPEG):
    print("fmtforsd: unable to find ffmpeg")

out = os.path.expanduser(sys.argv[2])

subprocess.run([FFMPEG, "-i", os.path.expanduser(sys.argv[1]), "-f", "s16le", "-c:a", "pcm_s16le", "-ar", "44100", out], check=True)

with open(out, "rb") as f:
    old = f.read()

with open(out, "wb") as f:
    f.write(struct.pack("<I", len(old)))
    f.write(old)
