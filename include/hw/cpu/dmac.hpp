/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH DMA controller I/O
namespace hw::cpu::ocio::dmac {

enum {
    CHANNEL_0,
    CHANNEL_1,
    CHANNEL_2,
    CHANNEL_3,
    NUM_CHANNELS,
};

void initialize();
void reset();
void shutdown();

u32 get_control(const int channel);

void set_source_address(const int channel, const u32 data);
void set_destination_address(const int channel, const u32 data);
void set_transfer_count(const int channel, const u32 data);
void set_control(const int channel, const u32 data);
void set_dma_operation(const u32 data);

void execute_channel_2_dma(u32& start_address, u32& length, bool& start);

}
