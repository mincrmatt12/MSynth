# MSynth App Framework

This is our replacement for platformio, since it wasn't up to this particular task.

## Create new bucket

(internal name for a set of apps)

- Copy the template cmakelists to a new folder.
- Customize it.
- Run cmake, passing `-DCMAKE_TOOLCHAIN_FILE=<path to toolchain.cmake>`
- Run cmake again, not passing the toolchain (it doesn't work the first time for whatever reason)

## Structure

```
|
-- ld: link script
-- startup: startup (+ header generation) asm
-- lib: common library for msynth
```

## App structure

Currently just contains a `src` folder per app, with all the sources.
Eventually, this will contain places to put filesystem resources/etc.
