/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// GD-ROM functions
namespace hw::g1::gdrom {

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

template<typename T>
void write(const u32 addr, const T data);

}
