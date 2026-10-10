#pragma once
/*
 * JE-8080: turn a JP-8080 flash image into something jeLib's H8S can boot.
 *
 * WHY THIS EXISTS
 *
 * jeLib loads every ROM at CPU address 0. That is right for the JP-8000: its
 * 512 KB image starts with the boot block (vectors, reset code, the boot
 * loader's own SCI handlers) and the application follows at 0x4000.
 *
 * The JP-8080 image (896 KB, 917,504 bytes; also what jeLib assembles from
 * the 14-file .mid update) is NOT that. It is the application ONLY, built to
 * run at CPU 0x10000:
 *
 *   - 605 of its 4-byte words point at its own strings when the image is based
 *     at 0x10000, against 21-33 for any other base;
 *   - offset 0 is the application's interrupt STUB table (`JMP @aa:24` per
 *     vector), the same scheme the JP-8000 keeps at 0x4000;
 *   - offset 0x100 is the C runtime entry (clear on-chip RAM, copy .data,
 *     set SP, jump to main), as 0x4100 is on the keyboard;
 *   - offsets 0x200 / 0x204 are the entry points the boot loader's SCI0
 *     handlers jump to (each starts with POP.L ER0), as 0x4200 / 0x4204 are.
 *
 * The 64 KB boot block that sits below it in the real flash is not in the dump
 * (a firmware update never rewrites it). Loaded at 0, the stub table was taken
 * for the vector table, reset landed in the middle of the SCI handler, and the
 * first MIDI byte -- vector 53, which on BOTH machines points into the boot
 * block -- jumped into empty memory.
 *
 * The app also calls into that block for six compiler runtime helpers
 * (0xA000-0xA0D2, ~2,000 call sites) and one task init routine (0x587E).
 *
 * WHAT IT DOES
 *
 * Puts a 64 KB boot block of our own (src/boot/je8080_boot.S, assembled into
 * je8080_boot_block.h) under the image: vectors, reset, SCI0 forwarders,
 * runtime helpers. See the .S for each entry point. Nothing in it is copied
 * from a Roland ROM; tests/boot checks the helpers against C on jeLib's own
 * H8S core.
 */
#include <cstdint>
#include <vector>
#include "je8080_boot_block.h"

namespace je8080 {

static constexpr uint32_t kAppBase       = 0x10000;  // CPU address of image offset 0
static constexpr size_t   kRackImageSize = 917504;   // jeLib::Rom::RomSizeRack

/* A rack ROM (as jeLib::Rom::getData() returns it) -> the memory image to hand
 * jeLib::Je8086. Empty if the input is not a rack image. */
static inline std::vector<uint8_t> rack_boot_image(const std::vector<uint8_t> &rom) {
    if (rom.size() != kRackImageSize || kBootBlockLen > kAppBase) return {};
    std::vector<uint8_t> m(kAppBase + rom.size(), 0xFF);   // erased flash reads 0xFF
    std::copy(kBootBlock, kBootBlock + kBootBlockLen, m.begin());
    std::copy(rom.begin(), rom.end(), m.begin() + kAppBase);
    return m;
}

}  // namespace je8080
