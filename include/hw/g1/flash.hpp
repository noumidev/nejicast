/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025-2026  noumidev
 */

#pragma once

#include <common/types.hpp>

// FLASH ROM functions
namespace hw::g1::flash {

void initialize(const char* flash_path);
void reset();
void shutdown();

template<typename T>
void write(const u32 addr, const T data);

u8* get_flash_rom_ptr();

}
