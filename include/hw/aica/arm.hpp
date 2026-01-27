/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025-2026  noumidev
 */

#pragma once

#include <common/types.hpp>

namespace hw::aica::arm {

void initialize();
void reset();
void shutdown();

void assert_fast_interrupt();
void clear_fast_interrupt();

void assert_reset(const bool is_reset);
void step();

i64* get_cycles();

}
