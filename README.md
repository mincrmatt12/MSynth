# MSynth

A simple-ish digital desktop synthesizer unit, on woefully underpowered hardware.

(well it's an STM32F429, it's really just RAM-limited, but hey it only costs ~57 cad for parts + pcbs + assembly)

## Architecture

The project is split up into multiple subprojects, each contained in a folder. They all have READMEs related to their internal structure.
User-facing documentation (i.e. a guide) is stored in the `docs/` folder.

The various subprojects are:

- `bmap`: various resource generation tools
- `board`: PCB CAD files + schematic 
- `bootloader`: The MSynth Bootloader, responsible for installation/updating/management/booting of applications and filesystem management/cleanup.
- `factory`: Test application for verifying board accuracy, component quality and basic unit tests.
- `framework`: Common code and build setup for all applications (clock setup, peripheral access code, filesystem code, etc.)
- `fstool`: Tools for generating and interacting with filesystem images

## Licensing

The MSynth project _source code_ is licensed under the ISC license (see LICENSE.txt). 
Note that any output of MSynth units is _NOT_ covered by this license; you are free to do literally anything with the audio that comes out of this project, and no warranty is provided
for these outputs (again, see LICENSE.txt).

Various font resources used (Lato and DejaVu Sans) are optionally used during the compilation process, but not included here; if you wish to avoid licensing restrictions
you may substitute other or custom fonts with more favorable licensing. 

Any graphical resources (_only in their source form in this repository_), including logos are licensed under the CC-BY-SA-4.0 license. (see LICENSE-graphics.txt)

Copyright (c) 2020, Matthew Mirvish
