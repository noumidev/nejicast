/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/ta.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::pvr::ta {

#define TA_ALLOC_CTRL     ctx.allocation_control
#define TA_GLOB_TILE_CLIP ctx.global_tile_clip
#define TA_ISP_BASE       ctx.isp_list_base
#define TA_ISP_LIMIT      ctx.isp_list_limit
#define TA_OL_BASE        ctx.object_list_base
#define TA_OL_LIMIT       ctx.object_list_limit

struct {
    union {
        u32 raw;

        struct {
            u32 opaque_block_size             :  2;
            u32                               :  2;
            u32 opaque_volume_block_size      :  2;
            u32                               :  2;
            u32 translucent_block_size        :  2;
            u32                               :  2;
            u32 translucent_volume_block_size :  2;
            u32                               :  2;
            u32 punch_through_block_size      :  2;
            u32                               :  2;
            u32 block_direction               :  1;
            u32                               : 11;
        };
    } allocation_control;

    union {
        u32 raw;

        struct {
            u32 tile_x :  6;
            u32        : 10;
            u32 tile_y :  6;
            u32        : 10;
        };
    } global_tile_clip;

    u32 isp_list_base;
    u32 isp_list_limit;
    u32 object_list_base;
    u32 object_list_limit;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

void set_allocation_control(const u32 data) {
    TA_ALLOC_CTRL.raw = data;
}

void set_global_tile_clip(const u32 data) {
    // Boot ROM sets a weird value, is this even used?
    TA_GLOB_TILE_CLIP.raw = data;
}

void set_isp_list_base(const u32 data) {
    TA_ISP_BASE = data;
}

void set_isp_list_limit(const u32 data) {
    TA_ISP_LIMIT = data;
}

void set_object_list_base(const u32 data) {
    TA_OL_BASE = data;
}

void set_object_list_limit(const u32 data) {
    TA_OL_LIMIT = data;
}

void initialize_lists() {
    // TODO: initialize TA lists
}

}
