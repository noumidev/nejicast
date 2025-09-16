/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH performance counter I/O
namespace hw::cpu::ocio::prfc {

enum {
    CHANNEL_0,
    CHANNEL_1,
    NUM_CHANNELS,
};

void initialize();
void reset();
void shutdown();

u16 get_control(const int channel);

void set_control(const int channel, const u16 data);

}
