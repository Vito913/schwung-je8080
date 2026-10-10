# JE-8080 for Schwung (experimental)

A Roland **JP-8080** (rack) module for Schwung on Ableton Move, built on
[charlesvestal/schwung-je8086](https://github.com/charlesvestal/schwung-je8086)
and the [gearmulator](https://github.com/dsp56300/gearmulator) JE-8086 engine.

> **Status: boots, not playable.** The JP-8080 firmware boots, runs its own
> factory reset, starts all twelve of its tasks and takes MIDI. The
> rack-only hardware is not emulated yet, so what reaches the DAC is a constant
> buzz instead of the voices. **The module's output is muted** until that is
> fixed. Install it to develop, not to play.

## What works

| | |
|---|---|
| ROM | The 917,504-byte JP-8080 flash image (`.bin`, v1.04 tested). Only a **rack** ROM is accepted: a JP-8000 ROM or an empty folder gives a clear error. jeLib's 14-file `.mid` update dump should also load, but that is untested. |
| Boot | Firmware boots under jeLib's H8S core, runs its factory reset and starts all 12 tasks in its task table. In an instrumented 11 s run, nothing executes in the boot area except our own code; 222 calls to boot routines we do not have return at once (see below). |
| MIDI | MIDI in reaches the firmware's own receive handler through SCI0 (vector 53): 16 note-ons arrive as exactly 48 handler entries. |
| Pipeline | The JE-8086 three-stage fork pipeline runs with no underruns in `plugin_host_test` on a PC. Not yet measured on a Move. |
| Banks | 512 factory patches in eight banks (Factory A–H) and 256 performances in four banks, read from the ROM and named correctly in the browser. Loading user bank files is inherited from JE-8086 and untested here. |

## What does not work yet

The JP-8080 has hardware the JP-8000 does not, and jeLib has no model of it.
The firmware drives three devices outside the JP-8000's four-ASIC window
(0x400000–0x40FFFF):

| Address | Seen from the firmware | Likely |
|---|---|---|
| 0x410000–0x410BFF | ~13k writes during boot | an extra DSP/ASIC (voice modulator?) |
| 0x411000–0x411BFF | ~15k writes during boot | a second extra chip |
| 0x412000–0x412005 | register 3 polled ~14k times, written ~7k | handshake/status with one of the above |

The firmware also does not answer the module's temp-performance dump
requests, so knob pages read `<unavailable>` and **a set cannot save this
module's state** (`state` is unavailable too). The temp image is already sized
for the rack (common 37, part 8, patch 248 bytes) for when it does.

Task 3 calls at least seven routines in the missing boot block (0x3908,
0x3A36, 0x3A64, 0x43EC, 0x44AA, 0x8C28, 0x8C86) whose behaviour is unknown.
They are stubbed to return at once, so whatever task 3 does is not happening;
it may be what answers those dump requests.

With those unanswered, the output is a full-scale tone at harmonics of
88200/256 Hz, the same before and after a note. Backing the range with plain
memory, or returning 0x00, 0x01, 0x80 or 0xFF from the polled register, does
not produce a note (0x80 and 0xFF silence the output). Getting further needs
a model of the rack's extra chips.

[Gearmulator has announced JP-8080 support](https://synthanatomy.com/2025/12/the-usual-suspects-je-8086-free-roland-jp-8080-synthesizer.html)
for a future release (2.2.26+). When it is public, the `libs/gearmulator`
submodule can move to it and the boot block below may no longer be needed.

## The boot block (why a JP-8080 image cannot just be loaded)

jeLib loads every ROM at CPU address 0. That is right for the JP-8000, whose
512 KB image starts with its boot block, with the application at 0x4000. The
JP-8080 image is the **application only, linked at 0x10000**:

- 605 of its 32-bit words point at its own strings when based at 0x10000,
  against 12–33 for the other bases tried (0, 0x8000, 0x18000, 0x20000,
  0x30000);
- offset 0 is the app's interrupt stub table, as 0x4000 is on the JP-8000;
- 0x100 is the C runtime entry and 0x200/0x204 the SCI0 handler entries, as
  0x4100 and 0x4200/0x4204 are on the JP-8000.

The 64 KB boot block below it is not in the dump, and the app depends on it
for the vector table, reset, the SCI0 (MIDI) handlers, six compiler runtime
helpers (~2,000 call sites), one task's init routine and the unknown routines
above. `src/boot/je8080_boot.S` provides all of them, and every unused byte is
RTS so an unknown call returns instead of running into other code. It was
written from what the app expects of each entry point, not copied from a
Roland ROM. Two helpers still come out matching Roland's code over short
runs: the register-save prologue (26 bytes, same as the JP-8000's) and the
32-bit multiply (17 bytes, same as the JP-8080's). There is essentially one
way to write those for this calling convention. `tests/boot` checks the
helpers against C on jeLib's own H8S core (15,771 checks; deliberately broken
helpers fail them).

```
src/boot/je8080_boot.S         the boot block, H8S assembly
tools/gen_boot_header.sh       assemble it -> src/dsp/je8080_boot_block.h (committed)
src/dsp/je8080_rack_image.h    boot block + ROM -> the image handed to jeLib
tests/boot/run.sh              helper unit tests (no ROM needed)
src/tools/je8080_boot_test.cpp boot a ROM, print LCD changes (none yet: the
                               rack's display is not emulated), play A4, optional wav
```

## Install (for development)

1. Build: `./scripts/build.sh` (Docker, cross-compiles to aarch64). Pushing to
   `main` also builds `je8080-module.tar.gz` in GitHub Actions; download it
   from the run's artifacts.
2. Copy `dist/je8080/` to
   `/data/UserData/schwung/modules/sound_generators/je8080/` (or run
   `./scripts/install.sh`).
3. Upload your ROM in Schwung Manager > Modules > JE-8080 > ROMs. It is
   saved as `roms/jp8080.bin`. The first load boots from scratch, which took
   about 17 s on a PC and will be slower on a Move.
4. To hear the raw (currently broken, LOUD) output:
   `touch /data/UserData/schwung/je8080_audio_on` and reload the slot.

Only one JE-8080 **or** JE-8086 can run at a time: they share
`/data/UserData/schwung/jp8000.pipeline.lock` on purpose, because two
pipelines starve Move's UI.

**ROMs are never committed here.** Supply your own.

## Host-side checks

```bash
cmake -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-host --target je8080_boot_test jp8000-move-plugin je8080-dsp plugin_host_test
./tests/boot/run.sh
(cd /path/to/rom_dir && ./build-host/je8080_boot_test . 400000000 out.wav)
mkdir -p mod/roms && cp build-host/dsp.so build-host/je8080-dsp src/*.json mod/ && cp jp8080.bin mod/roms/
./build-host/plugin_host_test mod/dsp.so mod 20     # JE8080_AUDIO_ON=1 to hear it
```

## Credits

- [gearmulator](https://github.com/dsp56300/gearmulator) (The Usual Suspects):
  the JE-8086 emulator, GPL-3.0.
- [charlesvestal/schwung-je8086](https://github.com/charlesvestal/schwung-je8086):
  the Schwung module this is built on, including the fork pipeline.
- JE-8080 changes: vito913.
