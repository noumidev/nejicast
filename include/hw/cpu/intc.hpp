/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH interrupt controller I/O
namespace hw::cpu::ocio::intc {

enum {
    PRIORITY_A,
    PRIORITY_B,
    PRIORITY_C,
    NUM_PRIORITY_REGS,
};

void initialize();
void reset();
void shutdown();

u16 get_priority(const int priority);

void set_interrupt_control(const u16 data);
void set_priority(const int priority, const u16 data);

}
