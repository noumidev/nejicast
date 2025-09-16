/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH clock pulse generator I/O
namespace hw::cpu::ocio::cpg {

void initialize();
void reset();
void shutdown();

u8 get_watchdog_timer_control();

void set_standby_control(const u8 data);
void set_watchdog_timer_counter(const u16 data);
void set_watchdog_timer_control(const u16 data);
void set_standby_control_2(const u8 data);

}
