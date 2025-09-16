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

u16 get_serial_status();
u16 get_line_status();

void set_serial_mode(const u16 data);
void set_bit_rate(const u8 data);
void set_serial_control(const u16 data);
void set_transmit_fifo_data(const u8 data);
void set_serial_status(const u16 data);
void set_fifo_control(const u16 data);
void set_serial_port(const u16 data);
void set_line_status(const u16 data);

}
