/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <vector>

#include <common/types.hpp>

// MAPLE functions
namespace hw::maple {

struct Frame {
    // This goes to a peripheral
    u8 port;
    u8 recipient_addr;
    u8 command;
    std::vector<u32> send_bytes;

    // This goes back to SH-4
    u8 result_code;
    u8 sender_addr;
    u32 receive_addr;
    std::vector<u32> receive_bytes;
};

enum {
    MAPLE_DEVICE_CONTROLLER = 0x01000000,
    MAPLE_DEVICE_STORAGE    = 0x02000000,
    MAPLE_DEVICE_SCREEN     = 0x04000000,
    MAPLE_DEVICE_TIMER      = 0x08000000,
    MAPLE_DEVICE_NONE       = 0xFFFFFFFF,
};

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

template<typename T>
void write(const u32 addr, const T data);

}
