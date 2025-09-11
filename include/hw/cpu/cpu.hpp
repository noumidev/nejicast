/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

namespace hw::cpu {

void initialize();
void reset();
void shutdown();

void setup_for_sideload(const u32 entry);

void assert_interrupt(const int interrupt_level);
void clear_interrupt(const int interrupt_level);

void step();

i64* get_cycles();

}
