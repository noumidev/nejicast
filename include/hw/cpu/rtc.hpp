/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH real-time clock I/O
namespace hw::cpu::ocio::rtc {

void initialize();
void reset();
void shutdown();

void set_rtc_month_alarm(const u8 data);
void set_rtc_control_1(const u8 data);

}
