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

enum {
    GEOMETRY_TYPE_NONE,
    GEOMETRY_TYPE_POLYGON,
    GEOMETRY_TYPE_SPRITE,
    GEOMETRY_TYPE_MODIFIER_VOLUME,
};

struct {
    u32 fifo_bytes[16];

    ParameterControlWord current_parameter;
    ParameterControlWord current_global_parameter;

    IspInstruction current_isp_instr;
    TspInstruction current_tsp_instr;
    TextureControlWord current_texture_control;

    Color global_color, global_offset_color;
    Color prev_color;

    bool has_list_type;
    bool has_parameter_control;
    bool is_first_vertex;

    int geometry_type;

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
    ctx.has_parameter_control = false;
    ctx.is_first_vertex = true;
}

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
    PARAM_TYPE_END_OF_LIST     = 0,
    PARAM_TYPE_USER_TILE_CLIP  = 1,
    PARAM_TYPE_OBJECT_LIST_SET = 2,
    PARAM_TYPE_GLOBAL_POLYGON  = 4,
    PARAM_TYPE_GLOBAL_SPRITE   = 5,
    PARAM_TYPE_GLOBAL_MODIFIER = 6,
    PARAM_TYPE_VERTEX          = 7,
};

enum {
    COLOR_TYPE_PACKED,
    COLOR_TYPE_FLOAT,
    COLOR_TYPE_INTENSITY_1,
    COLOR_TYPE_INTENSITY_2,
};

enum {
    GLOBAL_TYPE_PC_FC,
    GLOBAL_TYPE_PC_2V,
    GLOBAL_TYPE_I,
    GLOBAL_TYPE_I_OC,
    GLOBAL_TYPE_I_2V,
};

static int get_global_type() {
    if (ctx.current_global_parameter.use_bump_mapping) {
        // OC
        assert((ctx.current_global_parameter.volume_type & 1) == 0);

        switch (ctx.current_global_parameter.color_type) {
            case COLOR_TYPE_PACKED:
            case COLOR_TYPE_FLOAT:
                return GLOBAL_TYPE_PC_FC; // ??
            case COLOR_TYPE_INTENSITY_1:
            case COLOR_TYPE_INTENSITY_2:
                return GLOBAL_TYPE_I_OC;
            default:
                std::puts("TA Invalid color type for polygon with offset color");
                exit(1);
        }
    } else if ((ctx.current_global_parameter.volume_type & 1) != 0) {
        // 2V
        assert(!ctx.current_global_parameter.use_bump_mapping);

        switch (ctx.current_global_parameter.color_type) {
            case COLOR_TYPE_PACKED:
                return GLOBAL_TYPE_PC_2V;
            case COLOR_TYPE_INTENSITY_1:
            case COLOR_TYPE_INTENSITY_2:
                return GLOBAL_TYPE_I_2V;
            default:
                std::puts("TA Invalid color type for polygon with two volumes");
                exit(1);
        }
    } else {
        switch (ctx.current_global_parameter.color_type) {
            case COLOR_TYPE_PACKED:
            case COLOR_TYPE_FLOAT:
                return GLOBAL_TYPE_PC_FC;
            case COLOR_TYPE_INTENSITY_1:
            case COLOR_TYPE_INTENSITY_2:
                return GLOBAL_TYPE_I;
            default:
                std::puts("TA Invalid color type for polygon");
                exit(1);
        }
    }
}

static bool can_parse_global_polygon() {
    const int global_type = get_global_type();

    switch (global_type) {
        case GLOBAL_TYPE_I_2V:
        case GLOBAL_TYPE_I_OC:
            return false;
        default:
            return true;
    }
}

enum {
    POLYGON_TYPE_NT_PC,
    POLYGON_TYPE_NT_FC,
    POLYGON_TYPE_NT_I,
    POLYGON_TYPE_NT_PC_2V,
    POLYGON_TYPE_NT_I_2V,
    POLYGON_TYPE_PC,
    POLYGON_TYPE_FC,
    POLYGON_TYPE_I,
    POLYGON_TYPE_PC_2V,
    POLYGON_TYPE_I_2V,
    POLYGON_TYPE_PC_16UV,
    POLYGON_TYPE_FC_16UV,
    POLYGON_TYPE_I_16UV,
    POLYGON_TYPE_PC_16UV_2V,
    POLYGON_TYPE_I_16UV_2V,
};

// A little ugly, but gets the job done
static int get_polygon_type() {
    if (ctx.current_isp_instr.regular.use_texture_mapping) {
        if ((ctx.current_global_parameter.volume_type & 1) != 0) {
            // 2V
            if (ctx.current_global_parameter.use_short_texture_coordinates) {
                // 16UV
                switch (ctx.current_global_parameter.color_type) {
                    case COLOR_TYPE_PACKED:
                        return POLYGON_TYPE_PC_16UV_2V;
                    case COLOR_TYPE_INTENSITY_1:
                    case COLOR_TYPE_INTENSITY_2:
                        return POLYGON_TYPE_I_16UV_2V;
                    default:
                        std::puts("TA Invalid color type for 16-bit UV textured polygons with two volumes");
                        exit(1);
                }
            } else {
                switch (ctx.current_global_parameter.color_type) {
                    case COLOR_TYPE_PACKED:
                        return POLYGON_TYPE_PC_2V;
                    case COLOR_TYPE_INTENSITY_1:
                    case COLOR_TYPE_INTENSITY_2:
                        return POLYGON_TYPE_I_2V;
                    default:
                        std::puts("TA Invalid color type for textured polygons with two volumes");
                        exit(1);
                }
            }
        } else {
            if (ctx.current_global_parameter.use_short_texture_coordinates) {
                // 16UV
                switch (ctx.current_global_parameter.color_type) {
                    case COLOR_TYPE_PACKED:
                        return POLYGON_TYPE_PC_16UV;
                    case COLOR_TYPE_FLOAT:
                        return POLYGON_TYPE_FC_16UV;
                    case COLOR_TYPE_INTENSITY_1:
                    case COLOR_TYPE_INTENSITY_2:
                        return POLYGON_TYPE_I_16UV;
                    default:
                        std::puts("TA Invalid color type for 16-bit UV textured polygons");
                        exit(1);
                }
            } else {
                switch (ctx.current_global_parameter.color_type) {
                    case COLOR_TYPE_PACKED:
                        return POLYGON_TYPE_PC;
                    case COLOR_TYPE_FLOAT:
                        return POLYGON_TYPE_FC;
                    case COLOR_TYPE_INTENSITY_1:
                    case COLOR_TYPE_INTENSITY_2:
                        return POLYGON_TYPE_I;
                    default:
                        std::puts("TA Invalid color type for textured polygons");
                        exit(1);
                }
            }
        }
    } else {
        // NT
        if ((ctx.current_global_parameter.volume_type & 1) != 0) {
            // 2V
            switch (ctx.current_global_parameter.color_type) {
                case COLOR_TYPE_PACKED:
                    return POLYGON_TYPE_NT_PC_2V;
                case COLOR_TYPE_INTENSITY_1:
                case COLOR_TYPE_INTENSITY_2:
                    return POLYGON_TYPE_NT_I_2V;
                default:
                    std::puts("TA Invalid color type for non-textured polygons with two volumes");
                    exit(1);
            }
        } else {
            switch (ctx.current_global_parameter.color_type) {
                case COLOR_TYPE_PACKED:
                    return POLYGON_TYPE_NT_PC;
                case COLOR_TYPE_FLOAT:
                    return POLYGON_TYPE_NT_FC;
                case COLOR_TYPE_INTENSITY_1:
                case COLOR_TYPE_INTENSITY_2:
                    return POLYGON_TYPE_NT_I;
                default:
                    std::puts("TA Invalid color type for non-textured polygons");
                    exit(1);
            }
        }
    }
}

static bool can_parse_vertex() {
    if (ctx.geometry_type != GEOMETRY_TYPE_POLYGON) {
        return false;
    }

    const int polygon_type = get_polygon_type();

    switch (polygon_type) {
        case POLYGON_TYPE_FC:
        case POLYGON_TYPE_FC_16UV:
        case POLYGON_TYPE_PC_2V:
        case POLYGON_TYPE_PC_16UV_2V:
        case POLYGON_TYPE_I_2V:
        case POLYGON_TYPE_I_16UV_2V:
            return false;
        default:
            return true;
    }
}

static void ta_end_of_list() {
    if constexpr (!SILENT_TA) std::puts("TA End of list");
    
    finish_list(ctx.current_global_parameter.list_type);
}

static void ta_user_tile_clip() {
    if constexpr (!SILENT_TA) std::puts("TA User tile clip");
}

static void ta_object_list_set() {
    if constexpr (!SILENT_TA) std::puts("TA Object list set");
}

static void ta_global_polygon() {
    constexpr const char* GLOBAL_TYPES[] = {
        "GLOBAL_TYPE_PC_FC", "GLOBAL_TYPE_PC_2V", "GLOBAL_TYPE_I",
        "GLOBAL_TYPE_I_OC", "GLOBAL_TYPE_I_2V",
    };

    if constexpr (!SILENT_TA) std::puts("TA Global parameter (polygon)");

    ctx.current_isp_instr = IspInstruction{.raw = ctx.fifo_bytes[1]};
    ctx.current_tsp_instr = TspInstruction{.raw = ctx.fifo_bytes[2]};
    ctx.current_texture_control = TextureControlWord{.raw = ctx.fifo_bytes[3]};
    
    if constexpr (!SILENT_TA) {
        std::printf("ISP instruction = %08X\n", ctx.current_isp_instr.raw);
        std::printf("TSP instruction = %08X\n", ctx.current_tsp_instr.raw);
        std::printf("Texture control = %08X\n", ctx.current_texture_control.raw);   
    }

    ctx.current_isp_instr.regular.short_uv = ctx.current_global_parameter.use_short_texture_coordinates;
    ctx.current_isp_instr.regular.use_gouraud_shading = ctx.current_global_parameter.use_gouraud_shading;
    ctx.current_isp_instr.regular.use_texture_mapping = ctx.current_global_parameter.use_texture_mapping;
    ctx.current_isp_instr.regular.use_offset_color = ctx.current_global_parameter.use_bump_mapping;

    const int global_type = get_global_type();

    switch (global_type) {
        case GLOBAL_TYPE_PC_FC:
            break;
        case GLOBAL_TYPE_PC_2V:
            break;
        case GLOBAL_TYPE_I:
            ctx.global_color = from_floats(&ctx.fifo_bytes[4]);
            ctx.global_offset_color = Color{.raw = 0};
            break;
        case GLOBAL_TYPE_I_OC:
            ctx.global_color = from_floats(&ctx.fifo_bytes[8]);
            ctx.global_offset_color = from_floats(&ctx.fifo_bytes[12]);
            break;
        default:
            std::printf("TA Unimplemented global type %s\n", GLOBAL_TYPES[global_type]);
            exit(1);
    }

    if (!ctx.has_list_type) {
        if (ctx.current_global_parameter.list_type == LIST_TYPE_OPAQUE) {
            core::begin_display_list();
        }

        ctx.has_list_type = true;
    }
    
    ctx.geometry_type = GEOMETRY_TYPE_POLYGON;
    ctx.has_parameter_control = false;
}

static void ta_global_sprite() {
    if constexpr (!SILENT_TA) std::puts("TA Global parameter (sprite)");

    ctx.current_isp_instr = IspInstruction{.raw = ctx.fifo_bytes[1]};
    ctx.current_tsp_instr = TspInstruction{.raw = ctx.fifo_bytes[2]};
    ctx.current_texture_control = TextureControlWord{.raw = ctx.fifo_bytes[3]};
    
    if constexpr (!SILENT_TA) {
        std::printf("ISP instruction = %08X\n", ctx.current_isp_instr.raw);
        std::printf("TSP instruction = %08X\n", ctx.current_tsp_instr.raw);
        std::printf("Texture control = %08X\n", ctx.current_texture_control.raw);   
    }

    ctx.current_isp_instr.regular.short_uv = ctx.current_global_parameter.use_short_texture_coordinates;
    ctx.current_isp_instr.regular.use_gouraud_shading = ctx.current_global_parameter.use_gouraud_shading;
    ctx.current_isp_instr.regular.use_texture_mapping = ctx.current_global_parameter.use_texture_mapping;
    ctx.current_isp_instr.regular.use_offset_color = ctx.current_global_parameter.use_bump_mapping;

    ctx.global_color.raw = ctx.fifo_bytes[4];
    ctx.global_offset_color.raw = ctx.fifo_bytes[5];

    ctx.geometry_type = GEOMETRY_TYPE_SPRITE;
    ctx.has_parameter_control = false;
}

static void ta_global_modifier_volume() {
    if constexpr (!SILENT_TA) std::puts("TA Global parameter (modifier volume)");

    ctx.current_isp_instr = IspInstruction{.raw = ctx.fifo_bytes[1]};

    ctx.geometry_type = GEOMETRY_TYPE_MODIFIER_VOLUME;
    ctx.has_parameter_control = false;
}

static void ta_vertex() {
    constexpr const char* POLYGON_TYPES[] = {
        "POLYGON_TYPE_NT_PC", "POLYGON_TYPE_NT_FC", "POLYGON_TYPE_NT_I", "POLYGON_TYPE_NT_PC_2V",
        "POLYGON_TYPE_NT_I_2V", "POLYGON_TYPE_PC", "POLYGON_TYPE_FC", "POLYGON_TYPE_I",
        "POLYGON_TYPE_PC_2V", "POLYGON_TYPE_I_2V", "POLYGON_TYPE_PC_16UV", "POLYGON_TYPE_FC_16UV",
        "POLYGON_TYPE_I_16UV", "POLYGON_TYPE_PC_16UV_2V", "POLYGON_TYPE_I_16UV_2V",
    };

    if constexpr (!SILENT_TA) std::puts("TA Vertex");

    if (ctx.is_first_vertex) {
        core::begin_vertex_strip(
            ctx.current_isp_instr,
            ctx.current_tsp_instr,
            ctx.current_texture_control
        );

        ctx.is_first_vertex = false;
    }

    const f32 x = to_f32(ctx.fifo_bytes[1]);
    const f32 y = to_f32(ctx.fifo_bytes[2]);
    const f32 z = to_f32(ctx.fifo_bytes[3]);

    f32 u = 0.0;
    f32 v = 0.0;

    Color color, offset_color = {.raw = 0};

    const int polygon_type = get_polygon_type();

    // Oh boy
    switch (polygon_type) {
        case POLYGON_TYPE_NT_PC:
            color.raw = ctx.fifo_bytes[6];
            break;
        case POLYGON_TYPE_NT_PC_2V:
            // Not handling this correctly for now
            color.raw = ctx.fifo_bytes[4];
            break;
        case POLYGON_TYPE_NT_FC:
            color = from_floats(&ctx.fifo_bytes[4]);
            break;
        case POLYGON_TYPE_NT_I:
            // Missing intensity calculation
            if (ctx.current_global_parameter.color_type == COLOR_TYPE_INTENSITY_1) {
                ctx.prev_color = ctx.global_color;
            }

            color.raw = ctx.prev_color.raw * to_f32(ctx.fifo_bytes[4]);
            offset_color = Color{.raw = 0};
            break;
        case POLYGON_TYPE_PC:
            color.raw = ctx.fifo_bytes[6];
            offset_color.raw = ctx.fifo_bytes[7];

            u = to_f32(ctx.fifo_bytes[4]);
            v = to_f32(ctx.fifo_bytes[5]);
            break;
        case POLYGON_TYPE_PC_16UV:
            color.raw = ctx.fifo_bytes[6];
            offset_color.raw = ctx.fifo_bytes[7];

            u = to_f32(ctx.fifo_bytes[4] & 0xFFFF0000);
            v = to_f32((ctx.fifo_bytes[4] & 0xFFFF) << 16);
            break;
        case POLYGON_TYPE_FC:
            color = from_floats(&ctx.fifo_bytes[8]);
            offset_color = from_floats(&ctx.fifo_bytes[12]);

            u = to_f32(ctx.fifo_bytes[4]);
            v = to_f32(ctx.fifo_bytes[5]);
            break;
        case POLYGON_TYPE_I:
            if (ctx.current_global_parameter.color_type == COLOR_TYPE_INTENSITY_1) {
                ctx.prev_color = ctx.global_color;
            }

            color.raw = ctx.prev_color.raw * to_f32(ctx.fifo_bytes[6]);
            offset_color.raw = ctx.global_offset_color.raw * to_f32(ctx.fifo_bytes[7]);

            u = to_f32(ctx.fifo_bytes[4]);
            v = to_f32(ctx.fifo_bytes[5]);
            break;
        case POLYGON_TYPE_I_16UV:
            if (ctx.current_global_parameter.color_type == COLOR_TYPE_INTENSITY_1) {
                ctx.prev_color = ctx.global_color;
            }

            color.raw = ctx.prev_color.raw * to_f32(ctx.fifo_bytes[6]);
            offset_color.raw = ctx.global_offset_color.raw * to_f32(ctx.fifo_bytes[7]);

            u = to_f32(ctx.fifo_bytes[4] & 0xFFFF0000);
            v = to_f32((ctx.fifo_bytes[4] & 0xFFFF) << 16);
            break;
        default:
            std::printf("TA Unimplemented polygon type %s\n", POLYGON_TYPES[polygon_type]);
            exit(1);
    }

    if (!ctx.current_global_parameter.use_bump_mapping) {
        offset_color = Color{.raw = 0};
    }

    core::push_vertex(
        pvr::Vertex {
            x,
            y,
            z,
            u,
            v,
            color,
            offset_color
        }
    );

    if (ctx.current_parameter.end_of_strip) {
        core::end_vertex_strip(
            ctx.current_global_parameter.list_type
        );

        ctx.is_first_vertex = true;
    }

    ctx.has_parameter_control = false;
}

static inline float interpolate_plane(
    const pvr::Vertex* vertices,
    const f32 aq,
    const f32 bq,
    const f32 cq
) {
    const f32 dx1 = vertices[1].x - vertices[0].x;
    const f32 dy1 = vertices[1].y - vertices[0].y;
    const f32 dq1 = bq - aq;
    const f32 dx2 = vertices[2].x - vertices[0].x;
    const f32 dy2 = vertices[2].y - vertices[0].y;
    const f32 dq2 = cq - aq;

    const f32 det = dx1 * dy2 - dx2 * dy1;

    assert(det != 0.0);

    const f32 a = (dq1 * dy2 - dq2 * dy1) / det;
    const f32 b = (dx1 * dq2 - dx2 * dq1) / det;
    const f32 c = aq - a * vertices[0].x - b * vertices[0].y;

    return a * vertices[3].x + b * vertices[3].y + c;
}

static void ta_sprite() {
    if constexpr (!SILENT_TA) std::puts("TA Sprite");

    core::begin_vertex_strip(
        ctx.current_isp_instr,
        ctx.current_tsp_instr,
        ctx.current_texture_control
    );

    pvr::Vertex vertices[4];

    for (int i = 0; i < 3; i++) {
        vertices[i].x = to_f32(ctx.fifo_bytes[3 * i + 1]);
        vertices[i].y = to_f32(ctx.fifo_bytes[3 * i + 2]);
        vertices[i].z = to_f32(ctx.fifo_bytes[3 * i + 3]);

        vertices[i].u = 0.0;
        vertices[i].v = 0.0;

        if (ctx.current_global_parameter.use_texture_mapping) {
            vertices[i].u = to_f32(ctx.fifo_bytes[13 + i] & 0xFFFF0000);
            vertices[i].v = to_f32((ctx.fifo_bytes[13 + i] & 0xFFFF) << 16);
        }

        vertices[i].color = ctx.global_color;
        vertices[i].offset_color = ctx.global_offset_color;

        core::push_vertex(vertices[i]);
    }

    vertices[3].x = to_f32(ctx.fifo_bytes[10]);
    vertices[3].y = to_f32(ctx.fifo_bytes[11]);
    vertices[3].color = ctx.global_color;
    vertices[3].offset_color = ctx.global_offset_color;

    // Infer Z, U and V for the last vertex
    vertices[3].z = interpolate_plane(vertices, vertices[0].z, vertices[1].z, vertices[2].z);
    vertices[3].u = interpolate_plane(vertices, vertices[0].u, vertices[1].u, vertices[2].u);
    vertices[3].v = interpolate_plane(vertices, vertices[0].v, vertices[1].v, vertices[2].v);

    core::push_vertex(vertices[3]);

    core::end_vertex_strip(
        ctx.current_global_parameter.list_type
    );

    ctx.has_parameter_control = false;
}

static void ta_modifier_volume() {
    if constexpr (!SILENT_TA) std::puts("TA Modifier volume");

    // TODO...

    ctx.has_parameter_control = false;
}

void fifo_block_write(const u8 *bytes) {
    std::memcpy(&ctx.fifo_bytes[8 * ctx.has_parameter_control], bytes, 32);

    if constexpr (!SILENT_TA) {
        for (int i = 0; i < 8; i++) {
            std::printf("TA FIFO write = %08X\n", ctx.fifo_bytes[8 * ctx.has_parameter_control + i]);
        }
    }

    if (!ctx.has_parameter_control) {
        // Fetch new control word
        ctx.current_parameter = {.raw = ctx.fifo_bytes[0]};

        switch (ctx.current_parameter.parameter_type) {
            case PARAM_TYPE_END_OF_LIST:
                ta_end_of_list();
                break;
            case PARAM_TYPE_USER_TILE_CLIP:
                ta_user_tile_clip();
                break;
            case PARAM_TYPE_OBJECT_LIST_SET:
                ta_object_list_set();
                break;
            case PARAM_TYPE_GLOBAL_POLYGON:
                // 8 or 16 words
                ctx.current_global_parameter = ctx.current_parameter;

                if (can_parse_global_polygon()) {
                    ta_global_polygon();
                } else {
                    ctx.has_parameter_control = true;
                }
                break;
            case PARAM_TYPE_GLOBAL_SPRITE:
                ctx.current_global_parameter = ctx.current_parameter;

                ta_global_sprite();
                break;
            case PARAM_TYPE_GLOBAL_MODIFIER:
                ctx.current_global_parameter = ctx.current_parameter;

                ta_global_modifier_volume();
                break;
            case PARAM_TYPE_VERTEX:
                // 8 or 16 words
                if (can_parse_vertex()) {
                    ta_vertex();
                } else {
                    ctx.has_parameter_control = true;
                }
                break;
            default:
                printf("Unimplemented TA parameter type %u\n", ctx.current_parameter.parameter_type);
                exit(1);
        }
    } else {
        switch (ctx.current_parameter.parameter_type) {
            case PARAM_TYPE_GLOBAL_POLYGON:
                ta_global_polygon();
                break;
            case PARAM_TYPE_VERTEX:
                switch (ctx.geometry_type) {
                    case GEOMETRY_TYPE_POLYGON:
                        ta_vertex();
                        break;
                    case GEOMETRY_TYPE_SPRITE:
                        ta_sprite();
                        break;
                    case GEOMETRY_TYPE_MODIFIER_VOLUME:
                        ta_modifier_volume();
                        break;
                    default:
                        std::puts("TA Invalid geometry type");
                        exit(1);
                }
                break;
            default:
                printf("Unimplemented TA parameter type %u\n", ctx.current_parameter.parameter_type);
                exit(1);
        }
    }
}

}
