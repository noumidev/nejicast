/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// HOLLY interrupt controller functions
namespace hw::holly::intc {

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

template<typename T>
void write(const u32 addr, const T data);

void assert_normal_interrupt(const int interrupt_number);
void assert_external_interrupt(const int interrupt_number);
void clear_external_interrupt(const int interrupt_number);

}
