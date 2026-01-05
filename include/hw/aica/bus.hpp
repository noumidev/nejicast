/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// AICA ARM bus functions
namespace hw::aica::bus {

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

template<typename T>
void write(const u32 addr, const T data);

void copy_from_bytes(
    const u32 addr,
    const u32 copy_size,
    const u32 total_size,
    const u8* bytes
);

void dump_memory(
    const u32 addr,
    const u32 size,
    const char* path
);

}