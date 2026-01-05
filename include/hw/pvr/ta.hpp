/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// PVR Tile Accelerator functions
namespace hw::pvr::ta {

enum {
    LIST_TYPE_OPAQUE               = 0,
    LIST_TYPE_OPAQUE_MODIFIER      = 1,
    LIST_TYPE_TRANSLUCENT          = 2,
    LIST_TYPE_TRANSLUCENT_MODIFIER = 3,
    LIST_TYPE_PUNCHTHROUGH         = 4,
};

void initialize();
void reset();
void shutdown();

u32 get_itp_current_address();

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
