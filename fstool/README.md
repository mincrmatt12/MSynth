# `fstool`

This folder contains tools related to modifying the flash-filesystem. They are for updating filesystem dumps in-place (for use with a debugger).

## `fsgen` - "FileSystem GENerate"

This tool creates a new filesystem image from a folder. It only assigns flags to folders starting with `$`, marking them as private.

## `msfsutil.py` - "MSynth FileSystem UTILity library"

This is a library that is able to generate and parse the filesystem format.
