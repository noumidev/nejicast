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
#include <hw/pvr/pvr.hpp>

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
        u32 use_short_texture_coordinates : 1;
        u32 use_gouraud_shading           : 1;
        u32 use_bump_mapping              : 1;
        u32 use_texture_mapping           : 1;
        u32 color_type                    : 2;
        u32 volume_type                   : 2;
        u32                               : 8;
        u32 group_control                 : 8;
        u32 list_type                     : 3;
        u32                               : 1;
        u32 end_of_strip                  : 1;
        u32 parameter_type                : 3;
    };
};

struct {
    ParameterControlWord current_global_parameter;

    u32 intensity_colors[4];

    u32 current_tsp_instr;
    u32 current_texture_control;

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
    LIST_TYPE_OPAQUE               = 0,
    LIST_TYPE_OPAQUE_MODIFIER      = 1,
    LIST_TYPE_TRANSLUCENT          = 2,
    LIST_TYPE_TRANSLUCENT_MODIFIER = 3,
    LIST_TYPE_PUNCHTHROUGH         = 4,
};

enum {
    INTERRUPT_OPAQUE_LIST               =  7,
    INTERRUPT_OPAQUE_MODIFIER_LIST      =  8,
    INTERRUPT_TRANSLUCENT_LIST          =  9,
    INTERRUPT_TRANSLUCENT_MODIFIER_LIST = 10,
    INTERRUPT_PUNCHTHROUGH_LIST         = 21,
};

static void send_interrupt(const int list_type) {
    if (list_type == LIST_TYPE_PUNCHTHROUGH) {
        hw::holly::intc::assert_normal_interrupt(INTERRUPT_PUNCHTHROUGH_LIST);
    } else {
        hw::holly::intc::assert_normal_interrupt(list_type + INTERRUPT_OPAQUE_LIST);
    }
}

constexpr i64 TA_DELAY = 0x1000;

static void finish_list(const int list_type) {
    assert(ctx.has_list_type);

    scheduler::schedule_event(
        "TA_LIST_END",
        send_interrupt,
        list_type,
        // NOTE: how long does this actually take?
        scheduler::to_scheduler_cycles<scheduler::HOLLY_CLOCKRATE>(TA_DELAY)
    );

    ctx.has_list_type = false;
}

static Color from_floats(const u32* float_bytes) {
    return Color{
        .b = (u8)(255.0F * to_f32(float_bytes[3])),
        .g = (u8)(255.0F * to_f32(float_bytes[2])),
        .r = (u8)(255.0F * to_f32(float_bytes[1])),
        .a = (u8)(255.0F * to_f32(float_bytes[0]))
    };
}

enum {
    PARAM_TYPE_END_OF_LIST    = 0,
    PARAM_TYPE_GLOBAL_POLYGON = 4,
    PARAM_TYPE_VERTEX         = 7,
};

enum {
    COLOR_TYPE_PACKED,
    COLOR_TYPE_FLOAT,
    COLOR_TYPE_INTENSITY_1,
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

            std::memcpy(ctx.intensity_colors, &fifo_bytes[4], sizeof(ctx.intensity_colors));

            ctx.current_tsp_instr = fifo_bytes[2];
            ctx.current_texture_control = fifo_bytes[3];

            if (ctx.current_texture_control != 0) {
                std::printf("TSP instruction = %08X\n", ctx.current_texture_control);
            }

            assert(!ctx.current_global_parameter.use_bump_mapping);
            assert(ctx.current_global_parameter.volume_type == 0);

            if (ctx.current_global_parameter.use_texture_mapping) {
                std::printf("TSP instruction = %08X\n", ctx.current_tsp_instr);
            }

            if (!ctx.has_list_type) {
                switch (ctx.current_global_parameter.list_type) {
                    case LIST_TYPE_OPAQUE:
                        if constexpr (!SILENT_TA) std::puts("TA Opaque list");
                        break;
                    case LIST_TYPE_OPAQUE_MODIFIER:
                        if constexpr (!SILENT_TA) std::puts("TA Opaque Modifier list");
                        break;
                    case LIST_TYPE_TRANSLUCENT:
                        if constexpr (!SILENT_TA) std::puts("TA Translucent list");
                        break;
                    case LIST_TYPE_TRANSLUCENT_MODIFIER:
                        if constexpr (!SILENT_TA) std::puts("TA Translucent Modifier list");
                        break;
                    case LIST_TYPE_PUNCHTHROUGH:
                        if constexpr (!SILENT_TA) std::puts("TA Punchthrough list");
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
                core::begin_vertex_strip(
                    ctx.current_tsp_instr,
                    ctx.current_texture_control
                );

                ctx.is_first_vertex = false;
            }

            {
                Color color;

                switch (ctx.current_global_parameter.color_type) {
                    case COLOR_TYPE_PACKED:
                        color.raw = fifo_bytes[6];
                        break;
                    case COLOR_TYPE_FLOAT:
                        color = from_floats(&fifo_bytes[4]);
                        break;
                    case COLOR_TYPE_INTENSITY_1:
                        color = from_floats(ctx.intensity_colors);
                        break;
                    default:
                        std::printf("PVR Unimplemented color type %u\n", ctx.current_global_parameter.color_type);
                        exit(1);
                }

                // FIXME: this only works for a small number of vertex configs
                core::push_vertex(
                    pvr::Vertex {
                        to_f32(fifo_bytes[1]),
                        to_f32(fifo_bytes[2]),
                        to_f32(fifo_bytes[3]),
                        to_f32(fifo_bytes[4]),
                        to_f32(fifo_bytes[5]),
                        color
                    }
                );
            }

            if (parameter_control.end_of_strip) {
                core::end_vertex_strip(
                    ctx.current_global_parameter.use_gouraud_shading,
                    ctx.current_global_parameter.use_texture_mapping
                );

                ctx.is_first_vertex = true;
            }
            break;
        default:
            printf("Unimplemented TA parameter type %u\n", parameter_control.parameter_type);
            exit(1);
    }
}

}
