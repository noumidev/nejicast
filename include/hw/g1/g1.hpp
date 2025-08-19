/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <common/types.hpp>

// G1 bus functions
namespace hw::g1 {

void initialize(const char* boot_path);
void reset();
void shutdown();

u8* get_boot_rom_ptr();

}
