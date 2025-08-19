/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <common/types.hpp>

// HOLLY bus functions
namespace hw::holly::bus {

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

template<typename T>
void write(const u32 addr, const T data);

}