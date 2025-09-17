/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/ta.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <scheduler.hpp>
#include <hw/holly/intc.hpp>
#include <hw/pvr/core.hpp>

namespace hw::pvr::ta {

constexpr bool SILENT_TA = true;

#define TA_ALLOC_CTRL     ctx.allocation_control
#define TA_GLOB_TILE_CLIP ctx.global_tile_clip
#define TA_ISP_BASE       ctx.isp_list_base
#define TA_ISP_LIMIT      ctx.isp_list_limit
#define TA_ITP_CURRENT    ctx.itp_current_address
#define TA_OL_BASE        ctx.object_list_base
#define TA_OL_LIMIT       ctx.object_list_limit
#define TA_NEXT_OPB_INIT  ctx.next_object_pointer_block

union ParameterControlWord {
    u32 raw;

    struct {
        u32 object_control : 16;
        u32 group_control  :  8;
        u32 list_type      :  3;
        u32                :  1;
        u32 end_of_strip   :  1;
        u32 parameter_type :  3;
    };
};

struct {
    ParameterControlWord current_global_parameter;

    bool has_list_type;
    bool is_first_vertex;

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
    u32 next_object_pointer_block;
    u32 itp_current_address;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u32 get_itp_current_address() {
    return TA_ITP_CURRENT;
}

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

void set_next_object_pointer_block(const u32 data) {
    TA_NEXT_OPB_INIT = data;
}

void set_object_list_base(const u32 data) {
    TA_OL_BASE = data;
}

void set_object_list_limit(const u32 data) {
    TA_OL_LIMIT = data;
}

void initialize_lists() {
    // TODO: initialize TA lists
    ctx.has_list_type = false;
    ctx.is_first_vertex = true;
}

enum {
    LIST_TYPE_OPAQUE = 0,
};

constexpr i64 TA_DELAY = 1024;

static void finish_list(const int list_type) {
    assert(ctx.has_list_type);

    scheduler::schedule_event(
        "TA_LIST_END",
        hw::holly::intc::assert_normal_interrupt,
        7 + list_type,
        scheduler::to_scheduler_cycles<scheduler::HOLLY_CLOCKRATE>(TA_DELAY)
    );

    ctx.has_list_type = false;
}

enum {
    PARAM_TYPE_END_OF_LIST    = 0,
    PARAM_TYPE_GLOBAL_POLYGON = 4,
    PARAM_TYPE_VERTEX         = 7,
};

void fifo_block_write(const u8 *bytes) {
    u32 fifo_bytes[8];

    std::memcpy(fifo_bytes, bytes, sizeof(fifo_bytes));

    if constexpr (!SILENT_TA) {
        for (int i = 0; i < 8; i++) {
            std::printf("TA FIFO write = %08X\n", fifo_bytes[i]);
        }
    }

    const ParameterControlWord parameter_control{.raw = fifo_bytes[0]};

    switch (parameter_control.parameter_type) {
        case PARAM_TYPE_END_OF_LIST:
            if constexpr (!SILENT_TA) std::puts("TA End of list");
            
            finish_list(ctx.current_global_parameter.list_type);
            break;
        case PARAM_TYPE_GLOBAL_POLYGON:
            if constexpr (!SILENT_TA) std::puts("TA Global parameter (polygon)");

            ctx.current_global_parameter = parameter_control;

            if (!ctx.has_list_type) {
                switch (ctx.current_global_parameter.list_type) {
                    case LIST_TYPE_OPAQUE:
                        if constexpr (!SILENT_TA) std::puts("TA Opaque list");
                        break;
                    default:
                        printf("Unimplemented TA list type %u\n", ctx.current_global_parameter.list_type);
                        exit(1);
                }

                ctx.has_list_type = true;
            }
            break;
        case PARAM_TYPE_VERTEX:
            if (ctx.is_first_vertex) {
                core::begin_vertex_strip();

                ctx.is_first_vertex = false;
            }

            core::push_vertex(
                core::Vertex {
                    to_f32(fifo_bytes[1]),
                    to_f32(fifo_bytes[2]),
                    to_f32(fifo_bytes[3]),
                    fifo_bytes[6]
                }
            );

            if (parameter_control.end_of_strip) {
                core::end_vertex_strip();

                ctx.is_first_vertex = true;
            }
            break;
        default:
            printf("Unimplemented TA parameter type %u\n", parameter_control.parameter_type);
            exit(1);
    }
}

}
