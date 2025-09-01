/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// Sync Pulse Generator functions
namespace hw::pvr::spg {

void initialize();
void reset();
void shutdown();

u32 get_status();
u32 get_vblank_control();

void set_control(const u32 data);
void set_hblank_control(const u32 data);
void set_hblank_interrupt(const u32 data);
void set_load(const u32 data);
void set_vblank_control(const u32 data);
void set_vblank_interrupt(const u32 data);
void set_width(const u32 data);

void step(const i64 video_cycles);

}
