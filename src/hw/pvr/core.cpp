/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/core.hpp>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <scheduler.hpp>
#include <hw/holly/intc.hpp>
#include <hw/pvr/pvr.hpp>
#include <hw/pvr/spg.hpp>
#include <hw/pvr/ta.hpp>

namespace hw::pvr::core {

enum : u32 {
    IO_ID                = 0x005F8000,
    IO_REVISION          = 0x005F8004,
    IO_SOFTRESET         = 0x005F8008,
    IO_STARTRENDER       = 0x005F8014,
    IO_PARAM_BASE        = 0x005F8020,
    IO_REGION_BASE       = 0x005F802C,
    IO_SPAN_SORT_CFG     = 0x005F8030,
    IO_VO_BORDER_COLOR   = 0x005F8040,
    IO_FB_R_CTRL         = 0x005F8044,
    IO_FB_W_CTRL         = 0x005F8048,
    IO_FB_W_LINESTRIDE   = 0x005F804C,
    IO_FB_R_SOF1         = 0x005F8050,
    IO_FB_R_SOF2         = 0x005F8054,
    IO_FB_R_SIZE         = 0x005F805C,
    IO_FB_W_SOF1         = 0x005F8060,
    IO_FB_W_SOF2         = 0x005F8064,
    IO_FB_X_CLIP         = 0x005F8068,
    IO_FB_Y_CLIP         = 0x005F806C,
    IO_FPU_SHAD_SCALE    = 0x005F8074,
    IO_FPU_CULL_VAL      = 0x005F8078,
    IO_FPU_PARAM_CFG     = 0x005F807C,
    IO_HALF_OFFSET       = 0x005F8080,
    IO_FPU_PERP_VAL      = 0x005F8084,
    IO_ISP_BACKGND_D     = 0x005F8088,
    IO_ISP_BACKGND_T     = 0x005F808C,
    IO_ISP_FEED_CFG      = 0x005F8098,
    IO_SDRAM_REFRESH     = 0x005F80A0,
    IO_SDRAM_CFG         = 0x005F80A8,
    IO_FOG_COL_RAM       = 0x005F80B0,
    IO_FOG_COL_VERT      = 0x005F80B4,
    IO_FOG_DENSITY       = 0x005F80B8,
    IO_FOG_CLAMP_MAX     = 0x005F80BC,
    IO_FOG_CLAMP_MIN     = 0x005F80C0,
    IO_SPG_HBLANK_INT    = 0x005F80C8,
    IO_SPG_VBLANK_INT    = 0x005F80CC,
    IO_SPG_CONTROL       = 0x005F80D0,
    IO_SPG_HBLANK        = 0x005F80D4,
    IO_SPG_LOAD          = 0x005F80D8,
    IO_SPG_VBLANK        = 0x005F80DC,
    IO_SPG_WIDTH         = 0x005F80E0,
    IO_TEXT_CONTROL      = 0x005F80E4,
    IO_VO_CONTROL        = 0x005F80E8,
    IO_VO_STARTX         = 0x005F80EC,
    IO_VO_STARTY         = 0x005F80F0,
    IO_SCALER_CTL        = 0x005F80F4,
    IO_SPG_STATUS        = 0x005F810C,
    IO_FB_BURSTCTRL      = 0x005F8110,
    IO_Y_COEFF           = 0x005F8118,
    IO_TA_OL_BASE        = 0x005F8124,
    IO_TA_ISP_BASE       = 0x005F8128,
    IO_TA_OL_LIMIT       = 0x005F812C,
    IO_TA_ISP_LIMIT      = 0x005F8130,
    IO_TA_ITP_CURRENT    = 0x005F8138,
    IO_TA_GLOB_TILE_CLIP = 0x005F813C,
    IO_TA_ALLOC_CTRL     = 0x005F8140,
    IO_TA_LIST_INIT      = 0x005F8144,
    IO_TA_NEXT_OPB_INIT  = 0x005F8164,
    IO_FOG_TABLE         = 0x005F8200,
};

#define PARAM_BASE      ctx.isp_parameter_base
#define REGION_BASE     ctx.region_base
#define SPAN_SORT_CFG   ctx.span_sort_configuration
#define VO_BORDER_COLOR ctx.video_output.border_color
#define FB_R_CTRL       ctx.frame_buffer.read_control
#define FB_W_CTRL       ctx.frame_buffer.write_control
#define FB_W_LINESTRIDE ctx.frame_buffer.line_stride
#define FB_R_SOF1       ctx.frame_buffer.read_address_1
#define FB_R_SOF2       ctx.frame_buffer.read_address_2
#define FB_R_SIZE       ctx.frame_buffer.read_size
#define FB_W_SOF1       ctx.frame_buffer.write_address_1
#define FB_W_SOF2       ctx.frame_buffer.write_address_2
#define FB_X_CLIP       ctx.frame_buffer.x_clip
#define FB_Y_CLIP       ctx.frame_buffer.y_clip
#define FPU_SHAD_SCALE  ctx.fpu.shadow_scale
#define FPU_CULL_VAL    ctx.fpu.cull_value
#define FPU_PARAM_CFG   ctx.fpu.parameter_configuration
#define HALF_OFFSET     ctx.half_offset
#define FPU_PERP_VAL    ctx.fpu.perpendicular_compare_val
#define ISP_BACKGND_D   ctx.isp.background_depth
#define ISP_BACKGND_T   ctx.isp.background_tag
#define ISP_FEED_CFG    ctx.isp.feed_configuration
#define SDRAM_REFRESH   ctx.sdram.refresh
#define SDRAM_CFG       ctx.sdram.configuration
#define FOG_COL_RAM     ctx.fog.color_lut_mode
#define FOG_COL_VERT    ctx.fog.color_vertex_mode
#define FOG_DENSITY     ctx.fog.density
#define FOG_CLAMP_MAX   ctx.fog.clamp_max
#define FOG_CLAMP_MIN   ctx.fog.clamp_min
#define TEXT_CONTROL    ctx.texture_control
#define VO_CONTROL      ctx.video_output.control
#define VO_STARTX       ctx.video_output.horizontal_start
#define VO_STARTY       ctx.video_output.vertical_start
#define SCALER_CTL      ctx.scaler_control
#define FB_BURSTCTRL    ctx.frame_buffer.burst_control
#define Y_COEFF         ctx.y_coefficient

constexpr usize VRAM_SIZE = 0x800000;
constexpr usize FOG_TABLE_SIZE = 0x80;

struct VertexStrip {
    bool use_gouraud_shading;

    std::vector<Vertex> vertices;
};

struct {
    std::array<u8, VRAM_SIZE> video_ram;
    std::array<u16, FOG_TABLE_SIZE> fog_table;

    std::vector<VertexStrip> vertex_strips;

    u32 isp_parameter_base;
    u32 region_base;

    union {
        u32 raw;

        struct {
            u32 enable_span_sort   :  1;
            u32                    :  7;
            u32 enable_offset_sort :  1;
            u32                    :  7;
            u32 bypass_cache       :  1;
            u32                    : 15;
        };
    } span_sort_configuration;
    
    struct {
        union {
            u32 raw;

            struct {
                u32 enable              : 1;
                u32 line_double         : 1;
                u32 depth               : 2;
                u32 concat_value        : 3;
                u32                     : 1;
                u32 chroma_threshold    : 8;
                u32 strip_size          : 6;
                u32 enable_strip_buffer : 1;
                u32 vclk_divider        : 1;
                u32                     : 8;
            };
        } read_control;

        union {
            u32 raw;

            struct {
                u32 format           : 3;
                u32 enable_dithering : 1;
                u32                  : 4;
                u32 k_value          : 8;
                u32 alpha_threshold  : 8;
                u32                  : 8;
            };
        } write_control;

        u32 read_address_1, read_address_2;
        u32 write_address_1, write_address_2;

        union {
            u32 raw;

            struct {
                u32 x       : 10;
                u32 y       : 10;
                u32 modulus : 10;
                u32         :  2;
            };
        } read_size;

        union {
            u32 raw;

            struct {
                u32 clipping_min : 11;
                u32              :  5;
                u32 clipping_max : 11;
                u32              :  5;
            };
        } x_clip, y_clip;

        u32 line_stride;

        union {
            u32 raw;

            struct {
                u32 read_burst     :  6;
                u32                :  2;
                u32 remaining_data :  8;
                u32 write_burst    :  4;
                u32                : 12;
            };
        } burst_control;
    } frame_buffer;

    union {
        u32 raw;

        struct {
            u32 texel_half_offset :  1;
            u32 pixel_half_offset :  1;
            u32 fpu_half_offset   :  1;
            u32                   : 29;
        };
    } half_offset;

    struct {
        union {
            u32 raw;

            struct {
                u32 scale_value             :  8;
                u32 enable_intensity_volume :  1;
                u32                         : 23;
            };
        } shadow_scale;

        f32 cull_value;

        union {
            u32 raw;

            struct {
                u32 pointer_first_burst_size      :  4;
                u32 pointer_burst_size            :  4;
                u32 isp_parameter_burst_threshold :  6;
                u32 tsp_parameter_burst_threshold :  6;
                u32                               :  1;
                u32 region_header_type            :  1;
                u32                               : 10;
            };
        } parameter_configuration;

        f32 perpendicular_compare_val;
    } fpu;

    struct {
        f32 background_depth;

        union {
            u32 raw;

            struct {
                u32 tag_offset    :  3;
                u32 tag_address   : 21;
                u32 skip          :  3;
                u32 enable_shadow :  1;
                u32 bypass_cache  :  1;
                u32               :  3;
            };
        } background_tag;

        union {
            u32 raw;

            struct {
                u32 pre_sort_mode            :  1;
                u32                          :  2;
                u32 discard_mode             :  1;
                u32 punch_through_chunk_size : 10;
                u32 translucency_cache_size  : 10;
                u32                          :  8;
            };
        } feed_configuration;
    } isp;

    struct {
        u32 refresh;
        u32 configuration;
    } sdram;

    struct {
        union {
            u32 raw;

            struct {
                u32 blue  : 8;
                u32 green : 8;
                u32 red   : 8;
                u32 alpha : 8;
            };
        } color_lut_mode, color_vertex_mode, clamp_max, clamp_min;

        union {
            u32 raw;

            struct {
                u32 exponent :  8;
                u32 mantissa :  8;
                u32          : 16;
            };
        } density;
    } fog;

    struct {
        union {
            u32 raw;

            struct {
                u32 blue   : 8;
                u32 green  : 8;
                u32 red    : 8;
                u32 chroma : 1;
                u32        : 7;
            };
        } border_color;

        union {
            u32 raw;

            struct {
                u32 hsync_polarity :  1;
                u32 vsync_polarity :  1;
                u32 blank_polarity :  1;
                u32 blank_video    :  1;
                u32 field_mode     :  4;
                u32 double_pixel   :  1;
                u32                :  7;
                u32 pclk_delay     :  6;
                u32                : 10;
            };
        } control;

        u32 horizontal_start;
        
        union {
            u32 raw;

            struct {
                u32 field_1_start : 10;
                u32               :  6;
                u32 field_2_start : 10;
                u32               :  6;
            };
        } vertical_start;
    } video_output;

    union {
        u32 raw;

        struct {
            u32 stride               : 5;
            u32                      : 3;
            u32 bank_bit             : 5;
            u32                      : 3;
            u32 index_endianness     : 1;
            u32 code_book_endianness : 1;
            u32                      : 14;
        };
    } texture_control;

    union {
        u32 raw;

        struct {
            u32 vertical_scale_factor     : 16;
            u32 enable_horizontal_scaling :  1;
            u32 enable_interlace          :  1;
            u32 select_field              :  1;
            u32                           : 13;
        };
    } scaler_control;

    union {
        u32 raw;

        struct {
            u32 coefficient_0 :  8;
            u32 coefficient_1 :  8;
            u32               : 16;
        };
    } y_coefficient;
} ctx;

constexpr i64 CORE_DELAY = 0x8000;
constexpr int CORE_INTERRUPT = 2;

static void start_render() {
    pvr::clear_buffers();

    for (const auto& strip : ctx.vertex_strips) {
        assert(strip.vertices.size() > 2);

        pvr::set_gouraud_shading(strip.use_gouraud_shading);
        
        for (usize i = 0; i < (strip.vertices.size() - 2); i++) {
            pvr::submit_triangle(&strip.vertices[i]);
        }
    }

    pvr::finish_render();

    scheduler::schedule_event(
        "CORE_IRQ",
        hw::holly::intc::assert_normal_interrupt,
        CORE_INTERRUPT,
        scheduler::to_scheduler_cycles<scheduler::HOLLY_CLOCKRATE>(CORE_DELAY)
    );

    ctx.vertex_strips.clear();
}

void initialize() {
    VO_CONTROL.raw = 0x00000108;
    VO_STARTX = 0x9D;
    VO_STARTY.raw = 0x00150015;
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped PVR CORE read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

constexpr u32 CORE_ID = 0x17FD11DB;
constexpr u32 CORE_REVISION = 0x11;

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_ID:
            std::puts("ID read32");

            return CORE_ID;
        case IO_REVISION:
            std::puts("REVISION read32");

            return CORE_REVISION;
        case IO_VO_BORDER_COLOR:
            std::puts("VO_BORDER_COLOR read32");

            return VO_BORDER_COLOR.raw;
        case IO_FB_R_CTRL:
            std::puts("FB_R_CTRL read32");

            return FB_R_CTRL.raw;
        case IO_SPG_VBLANK:
            std::puts("SPG_VBLANK read32");

            return spg::get_vblank_control();
        case IO_VO_CONTROL:
            std::puts("VO_CONTROL read32");

            return VO_CONTROL.raw;
        case IO_SPG_STATUS:
            // std::puts("SPG_STATUS read32");

            return spg::get_status();
        case IO_TA_ITP_CURRENT:
            std::puts("TA_ITP_CURRENT read32");

            return ta::get_itp_current_address();
        case IO_TA_LIST_INIT:
            std::puts("TA_LIST_INIT read32");

            // Always returns 0?
            return 0;
        default:
            std::printf("Unmapped PVR CORE read32 @ %08X\n", addr);
            exit(1);
    }
}

template u8 read(u32);
template u16 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped PVR CORE write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    if ((addr & ~0x1FF) == IO_FOG_TABLE) {
        const u32 idx = (addr >> 2) & (FOG_TABLE_SIZE - 1);

        ctx.fog_table[idx] = data;

        std::printf("FOG_TABLE[%03u] write32 = %08X\n", idx, data);
        return;
    }

    switch (addr) {
        case IO_SOFTRESET:
            std::printf("SOFTRESET write32 = %08X\n", data);
            break;
        case IO_STARTRENDER:
            std::printf("STARTRENDER write32 = %08X\n", data);
            
            start_render();
            break;
        case IO_PARAM_BASE:
            std::printf("PARAM_BASE write32 = %08X\n", data);
        
            PARAM_BASE = data;
            break;
        case IO_REGION_BASE:
            std::printf("REGION_BASE write32 = %08X\n", data);
        
            REGION_BASE = data;
            break;
        case IO_SPAN_SORT_CFG:
            std::printf("SPAN_SORT_CFG write32 = %08X\n", data);
        
            SPAN_SORT_CFG.raw = data;
            break;
        case IO_VO_BORDER_COLOR:
            std::printf("VO_BORDER_COLOR write32 = %08X\n", data);
        
            VO_BORDER_COLOR.raw = data;
            break;
        case IO_FB_R_CTRL:
            std::printf("FB_R_CTRL write32 = %08X\n", data);
        
            FB_R_CTRL.raw = data;
            break;
        case IO_FB_W_CTRL:
            std::printf("FB_W_CTRL write32 = %08X\n", data);
        
            FB_W_CTRL.raw = data;
            break;
        case IO_FB_W_LINESTRIDE:
            std::printf("FB_W_LINESTRIDE write32 = %08X\n", data);
        
            FB_W_LINESTRIDE = data;
            break;
        case IO_FB_R_SOF1:
            std::printf("FB_R_SOF1 write32 = %08X\n", data);
        
            FB_R_SOF1 = data;
            break;
        case IO_FB_R_SOF2:
            std::printf("FB_R_SOF2 write32 = %08X\n", data);
        
            FB_R_SOF2 = data;
            break;
        case IO_FB_R_SIZE:
            std::printf("FB_R_SIZE write32 = %08X\n", data);
        
            FB_R_SIZE.raw = data;
            break;
        case IO_FB_W_SOF1:
            std::printf("FB_W_SOF1 write32 = %08X\n", data);
        
            FB_W_SOF1 = data;
            break;
        case IO_FB_W_SOF2:
            std::printf("FB_W_SOF2 write32 = %08X\n", data);
        
            FB_W_SOF2 = data;
            break;
        case IO_FB_X_CLIP:
            std::printf("FB_X_CLIP write32 = %08X\n", data);
        
            FB_X_CLIP.raw = data;
            break;
        case IO_FB_Y_CLIP:
            std::printf("FB_Y_CLIP write32 = %08X\n", data);
        
            FB_Y_CLIP.raw = data;
            break;
        case IO_FPU_SHAD_SCALE:
            std::printf("FPU_SHAD_SCALE write32 = %08X\n", data);
        
            FPU_SHAD_SCALE.raw = data;
            break;
        case IO_FPU_CULL_VAL:
            std::printf("FPU_CULL_VAL write32 = %08X\n", data);
        
            FPU_CULL_VAL = to_f32(data);
            break;
        case IO_FPU_PARAM_CFG:
            std::printf("FPU_PARAM_CFG write32 = %08X\n", data);
        
            FPU_PARAM_CFG.raw = data;
            break;
        case IO_HALF_OFFSET:
            std::printf("HALF_OFFSET write32 = %08X\n", data);
        
            HALF_OFFSET.raw = data;
            break;
        case IO_FPU_PERP_VAL:
            std::printf("FPU_PERP_VAL write32 = %08X\n", data);
        
            FPU_PERP_VAL = to_f32(data);
            break;
        case IO_ISP_BACKGND_D:
            std::printf("ISP_BACKGND_D write32 = %08X\n", data);
        
            ISP_BACKGND_D = to_f32(data);
            break;
        case IO_ISP_BACKGND_T:
            std::printf("ISP_BACKGND_T write32 = %08X\n", data);
        
            ISP_BACKGND_T.raw = data;
            break;
        case IO_ISP_FEED_CFG:
            std::printf("ISP_FEED_CFG write32 = %08X\n", data);
        
            ISP_FEED_CFG.raw = data;
            break;
        case IO_SDRAM_REFRESH:
            std::printf("SDRAM_REFRESH write32 = %08X\n", data);
        
            SDRAM_REFRESH = data;
            break;
        case IO_SDRAM_CFG:
            std::printf("SDRAM_CFG write32 = %08X\n", data);
        
            SDRAM_CFG = data;
            break;
        case IO_FOG_COL_RAM:
            std::printf("FOG_COL_RAM write32 = %08X\n", data);
        
            FOG_COL_RAM.raw = data;
            break;
        case IO_FOG_COL_VERT:
            std::printf("FOG_COL_VERT write32 = %08X\n", data);
        
            FOG_COL_VERT.raw = data;
            break;
        case IO_FOG_DENSITY:
            std::printf("FOG_DENSITY write32 = %08X\n", data);
        
            FOG_DENSITY.raw = data;
            break;
        case IO_FOG_CLAMP_MAX:
            std::printf("FOG_CLAMP_MAX write32 = %08X\n", data);
        
            FOG_CLAMP_MAX.raw = data;
            break;
        case IO_FOG_CLAMP_MIN:
            std::printf("FOG_CLAMP_MIN write32 = %08X\n", data);
        
            FOG_CLAMP_MIN.raw = data;
            break;
        case IO_SPG_HBLANK_INT:
            std::printf("SPG_HBLANK_INT write32 = %08X\n", data);
        
            spg::set_hblank_interrupt(data);
            break;
        case IO_SPG_VBLANK_INT:
            std::printf("SPG_VBLANK_INT write32 = %08X\n", data);
        
            spg::set_vblank_interrupt(data);
            break;
        case IO_SPG_CONTROL:
            std::printf("SPG_CONTROL write32 = %08X\n", data);
        
            spg::set_control(data);
            break;
        case IO_SPG_HBLANK:
            std::printf("SPG_HBLANK write32 = %08X\n", data);
        
            spg::set_hblank_control(data);
            break;
        case IO_SPG_LOAD:
            std::printf("SPG_LOAD write32 = %08X\n", data);
        
            spg::set_load(data);
            break;
        case IO_SPG_VBLANK:
            std::printf("SPG_VBLANK write32 = %08X\n", data);
        
            spg::set_vblank_control(data);
            break;
        case IO_SPG_WIDTH:
            std::printf("SPG_WIDTH write32 = %08X\n", data);
        
            spg::set_width(data);
            break;
        case IO_TEXT_CONTROL:
            std::printf("TEXT_CONTROL write32 = %08X\n", data);
        
            TEXT_CONTROL.raw = data;
            break;
        case IO_VO_CONTROL:
            std::printf("VO_CONTROL write32 = %08X\n", data);
        
            VO_CONTROL.raw = data;
            break;
        case IO_VO_STARTX:
            std::printf("VO_STARTX write32 = %08X\n", data);
        
            VO_STARTX = data;
            break;
        case IO_VO_STARTY:
            std::printf("VO_STARTY write32 = %08X\n", data);
        
            VO_STARTY.raw = data;
            break;
        case IO_SCALER_CTL:
            std::printf("SCALER_CTL write32 = %08X\n", data);
        
            SCALER_CTL.raw = data;
            break;
        case IO_FB_BURSTCTRL:
            std::printf("FB_BURSTCTRL write32 = %08X\n", data);
        
            FB_BURSTCTRL.raw = data;
            break;
        case IO_Y_COEFF:
            std::printf("Y_COEFF write32 = %08X\n", data);
        
            Y_COEFF.raw = data;
            break;
        case IO_TA_OL_BASE:
            std::printf("TA_OL_BASE write32 = %08X\n", data);
        
            ta::set_object_list_base(data);
            break;
        case IO_TA_ISP_BASE:
            std::printf("TA_ISP_BASE write32 = %08X\n", data);
        
            ta::set_isp_list_base(data);
            break;
        case IO_TA_OL_LIMIT:
            std::printf("TA_OL_LIMIT write32 = %08X\n", data);
        
            ta::set_object_list_limit(data);
            break;
        case IO_TA_ISP_LIMIT:
            std::printf("TA_ISP_LIMIT write32 = %08X\n", data);
        
            ta::set_isp_list_limit(data);
            break;
        case IO_TA_GLOB_TILE_CLIP:
            std::printf("TA_GLOB_TILE_CLIP write32 = %08X\n", data);
        
            ta::set_global_tile_clip(data);
            break;
        case IO_TA_ALLOC_CTRL:
            std::printf("TA_ALLOC_CTRL write32 = %08X\n", data);
        
            ta::set_allocation_control(data);
            break;
        case IO_TA_LIST_INIT:
            std::printf("TA_LIST_INIT write32 = %08X\n", data);
        
            if ((data >> 31) != 0) {
                ta::initialize_lists();
            }
            break;
        case IO_TA_NEXT_OPB_INIT:
            std::printf("TA_NEXT_OPB_INIT write32 = %08X\n", data);
        
            ta::set_next_object_pointer_block(data);
            break;
        default:
            std::printf("Unmapped PVR CORE write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

// For HOLLY access
u8* get_video_ram_ptr() {
    return ctx.video_ram.data();
}

void begin_vertex_strip() {
    // Create empty strip
    ctx.vertex_strips.emplace_back(VertexStrip{});
}

void push_vertex(const Vertex vertex) {
    const usize length = ctx.vertex_strips.size() - 1;

    std::printf("CORE Strip %zu vertex %zu (x = %f, y = %f, z = %f, color = %08X\n",
        length,
        ctx.vertex_strips[length].vertices.size(),
        vertex.x,
        vertex.y,
        vertex.z,
        vertex.color.raw
    );

    ctx.vertex_strips[length].vertices.push_back(vertex);
}

void end_vertex_strip(const bool use_gouraud_shading) {
    ctx.vertex_strips[ctx.vertex_strips.size() - 1].use_gouraud_shading = use_gouraud_shading;
}

}
