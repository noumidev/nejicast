/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>
#include <hw/pvr/pvr.hpp>

// PVR CORE functions
namespace hw::pvr::core {

void initialize();
void reset();
void shutdown();

template<typename T>
T read(const u32 addr);

template<typename T>
void write(const u32 addr, const T data);

u8* get_video_ram_ptr();

void begin_vertex_strip();
void push_vertex(const pvr::Vertex vertex);
void end_vertex_strip();

}
