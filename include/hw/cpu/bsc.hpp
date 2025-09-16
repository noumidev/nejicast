/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH bus state controller I/O
namespace hw::cpu::ocio::bsc {

enum {
    PORT_A,
    PORT_B,
    NUM_PORTS,
};

void initialize();
void reset();
void shutdown();

u16 get_refresh_count();
u32 get_port_control(const int port);
u16 get_port_data(const int port);

void set_bus_control_1(const u32 data);
void set_bus_control_2(const u16 data);
void set_wait_control_1(const u32 data);
void set_wait_control_2(const u32 data);
void set_wait_control_3(const u32 data);
void set_memory_control(const u16 data);
void set_refresh_timer_control(const u16 data);
void set_refresh_time_constant(const u16 data);
void set_refresh_count(const u16 data);
void set_port_control(const int port, const u32 data);
void set_port_data(const int port, const u16 data);
void set_gpio_interrupt_control(const u16 data);
void set_sdram_mode_3(const u16 data);

}
