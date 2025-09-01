/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH on-chip I/O (P4 area)
namespace hw::cpu::ocio {

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

template<typename T>
void write(const u32 addr, const T data);

void set_exception_event(const u32 event);
void set_interrupt_event(const u32 event);

void flush_store_queue(const u32 addr);

void execute_channel_2_dma(u32& start_address, u32& length, bool& start);

}
