/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// HOLLY bus functions
namespace hw::holly::bus {

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

void block_read(const u32 addr, u8* bytes);

template<typename T>
void write(const u32 addr, const T data);

void block_write(const u32 addr, const u8* bytes);

void dump_memory(
    const u32 addr,
    const u32 size,
    const char* path
);

}