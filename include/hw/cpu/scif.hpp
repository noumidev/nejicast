/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH serial comms interface (FIFO) I/O
namespace hw::cpu::ocio::scif {

void initialize();
void reset();
void shutdown();

u16 get_serial_status_2();
u16 get_line_status_2();

void set_serial_mode_2(const u16 data);
void set_bit_rate_2(const u8 data);
void set_serial_control_2(const u16 data);
void set_serial_status_2(const u16 data);
void set_fifo_control_2(const u16 data);
void set_serial_port_2(const u16 data);
void set_line_status_2(const u16 data);

}
