/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH user break controller I/O
namespace hw::cpu::ocio::ubc {

enum {
    CHANNEL_A,
    CHANNEL_B,
    NUM_CHANNELS,
};

void initialize();
void reset();
void shutdown();

void set_asid(const int channel, const u8 data);
void set_address(const int channel, const u32 data);
void set_address_mask(const int channel, const u8 data);
void set_bus_cycle(const int channel, const u16 data);
void set_break_control(const u16 data);

}
