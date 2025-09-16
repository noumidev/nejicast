/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH timer unit I/O
namespace hw::cpu::ocio::tmu {

enum {
    CHANNEL_0,
    CHANNEL_1,
    CHANNEL_2,
    NUM_CHANNELS,
};

void initialize();
void reset();
void shutdown();

u8 get_timer_start();
u32 get_counter(const int channel);
u16 get_control(const int channel);

void set_timer_output_control(const u8 data);
void set_timer_start(const u8 data);
void set_constant(const int channel, const u32 data);
void set_counter(const int channel, const u32 data);
void set_control(const int channel, const u16 data);

void step(const i64 cycles);

}
