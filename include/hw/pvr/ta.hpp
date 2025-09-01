/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// PVR Tile Accelerator functions
namespace hw::pvr::ta {

void initialize();
void reset();
void shutdown();

void set_allocation_control(const u32 data);
void set_global_tile_clip(const u32 data);
void set_isp_list_base(const u32 data);
void set_isp_list_limit(const u32 data);
void set_next_object_pointer_block(const u32 data);
void set_object_list_base(const u32 data);
void set_object_list_limit(const u32 data);

void initialize_lists();

void fifo_block_write(const u8* bytes);

}
