/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g1/g1.hpp>

#include <cassert>
#include <cstring>
#include <vector>

#include <common/file.hpp>

namespace hw::g1 {

constexpr usize BOOT_ROM_SIZE = 0x200000;

struct {
    std::vector<u8> boot_rom;
} ctx;

void initialize(const char* boot_path) {
    ctx.boot_rom = common::load_file(boot_path);

    assert(ctx.boot_rom.size() == BOOT_ROM_SIZE);
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

// For HOLLY access
u8* get_boot_rom_ptr() {
    return ctx.boot_rom.data();
}

}
