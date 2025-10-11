/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include "common/types.hpp"
#include <hw/pvr/pvr.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <nejicast.hpp>
#include <hw/pvr/core.hpp>
#include <hw/pvr/interface.hpp>
#include <hw/pvr/spg.hpp>
#include <hw/pvr/ta.hpp>

namespace hw::pvr {

using nejicast::SCREEN_WIDTH;
using nejicast::SCREEN_HEIGHT;

constexpr bool SILENT_PVR = true;

constexpr usize VRAM_SIZE = 0x800000;

struct {
    std::array<u8, VRAM_SIZE> video_ram;

    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> color_buffer, secondary_buffer;
    std::array<f32, SCREEN_WIDTH * SCREEN_HEIGHT> depth_buffer;

    IspInstruction isp_instr;
    TspInstruction tsp_instr;
    TextureControlWord texture_control;

    // TSP
    u32 u_size, v_size;

    // Texture control
    u32 texture_addr;

    bool is_translucent;
} ctx;

template<typename T>
T read_vram_linear(const u32 addr) {
    T data;

    std::memcpy(&data, &ctx.video_ram[addr & (VRAM_SIZE - 1)], sizeof(data));

    return data;
}

template u16 read_vram_linear(u32);
template u32 read_vram_linear(u32);

template<typename T>
T read_vram_interleaved(const u32 addr) {
    std::printf("Unimplemented texture memory read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u16 read_vram_interleaved(const u32 addr) {
    const u32 masked_addr = addr & (VRAM_SIZE - 1);

    const u32 offset = masked_addr >> 2;

    u32 data;

    if ((offset & 1) != 0) {
        // Second VRAM module
        std::memcpy(&data, &ctx.video_ram[(VRAM_SIZE >> 1) + sizeof(u32) * (offset >> 1)], sizeof(data));
    } else {
        // First VRAM module
        std::memcpy(&data, &ctx.video_ram[sizeof(u32) * (offset >> 1)], sizeof(data));
    }

    return ((addr & 1) != 0) ? data >> 16 : data;
}

template u32 read_vram_interleaved(u32);

static u32 swizzle_to_linear(const u32 x, const u32 y) {
    u32 n = 0;

    // Interleave bits
    for (u32 i = 0; i < 16; i++) {
        n |= ((y >> i) & 1) << (2 * i);
        n |= ((x >> i) & 1) << (2 * i + 1);
    }

    return n;
}

enum {
    SCAN_ORDER_SWIZZLED,
    SCAN_ORDER_LINEAR,
};

template<typename T>
static T read_texel(const u32, const u32) {
    std::printf("Unmapped texel read%zu\n", 8 * sizeof(T));
    exit(1);
}

template<>
u16 read_texel(const u32 x, const u32 y) {
    u32 addr = ctx.texture_addr;

    if (ctx.texture_control.regular.scan_order == SCAN_ORDER_SWIZZLED) {
        addr += 2 * swizzle_to_linear(x, y);
    } else {
        addr += 2 * (ctx.u_size * y + x);
    }

    return read_vram_interleaved<u16>(addr);
}

static f32 edge_function(const Vertex& a, const Vertex& b, const Vertex& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static f32 interpolate(
    const f32 w0,
    const f32 w1,
    const f32 w2,
    const f32 a,
    const f32 b,
    const f32 c,
    const f32 area
) {
    return (w0 * a + w1 * b + w2 * c) / area;
}

static u8 clamp_color_channel(const f32 channel) {
    if (channel < 0.0) {
        return 0;
    } else if (channel > 255.0) {
        return 255;
    }

    return (u8)channel;
}

static u8 clamp_color_channel(const int channel) {
    if (channel < 0) {
        return 0;
    } else if (channel > 255) {
        return 255;
    }

    return (u8)channel;
}

static Color add_and_clamp(const Color color, const Color other_color) {
    return Color{
        .a = clamp_color_channel(color.a + other_color.a),
        .r = clamp_color_channel(color.r + other_color.r),
        .g = clamp_color_channel(color.g + other_color.g),
        .b = clamp_color_channel(color.b + other_color.b),
    };
}

static f32 clamp_uv(const f32 uv) {
    if (uv < 0.0) {
        return 0.0;
    } else if (uv > 1.0) {
        return 1.0;
    }

    return uv;
}

static f32 repeat_uv(const f32 uv) {
    if ((uv < 0.0) || (uv > 1.0)) {
        return std::abs(std::fmodf(uv, 1.0));
    }

    return uv;
}

static u32 interpolate_colors(
    const f32 w0,
    const f32 w1,
    const f32 w2,
    const Vertex& a,
    const Vertex& b,
    const Vertex& c,
    const f32 area
) {
    const u8 blue = clamp_color_channel(interpolate(w0, w1, w2, a.color.b, b.color.b, c.color.b, area));
    const u8 green = clamp_color_channel(interpolate(w0, w1, w2, a.color.g, b.color.g, c.color.g, area));
    const u8 red = clamp_color_channel(interpolate(w0, w1, w2, a.color.r, b.color.r, c.color.r, area));

    return Color{.b = blue, .g = green, .r = red, .a = a.color.a}.raw;
}

enum : u32 {
    TEXTURE_FORMAT_RGB565   = 1,
    TEXTURE_FORMAT_ARGB4444 = 2,
};

static Color unpack_texel(const u16 texel) {
    Color color;

    switch (ctx.texture_control.regular.pixel_format) {
        case TEXTURE_FORMAT_RGB565:
            color.a = 0xFF;
            color.r = (texel >> 11) << 3;
            color.g = (texel >>  5) << 2;
            color.b = (texel >>  0) << 3;
            color.r |= color.r >> 5;
            color.g |= color.g >> 6;
            color.b |= color.b >> 5;
            break;
        case TEXTURE_FORMAT_ARGB4444:
            color.a = (texel >> 12) << 4;
            color.r = (texel >>  8) << 4;
            color.g = (texel >>  4) << 4;
            color.b = (texel >>  0) << 4;
            color.a |= color.a >> 4;
            color.r |= color.r >> 4;
            color.g |= color.g >> 4;
            color.b |= color.b >> 4;
            break;
        default:
            std::printf("TSP Unimplemented texture format %u\n", ctx.texture_control.regular.pixel_format);
            exit(1);
    }

    if (ctx.tsp_instr.ignore_tex_alpha) {
        color.a = 0xFF;
    }

    return color;
}

enum {
    DEPTH_MODE_NEVER,
    DEPTH_MODE_LESS,
    DEPTH_MODE_EQUAL,
    DEPTH_MODE_LESS_OR_EQUAL,
    DEPTH_MODE_GREATER,
    DEPTH_MODE_NOT_EQUAL,
    DEPTH_MODE_GREATER_OR_EQUAL,
    DEPTH_MODE_ALWAYS,
};

static bool depth_test(const f32 z, const u32 x, const u32 y) {
    const f32 old_z = ctx.depth_buffer[SCREEN_WIDTH * y + x];

    bool passed = true;

    switch (ctx.isp_instr.regular.depth_mode) {
        case DEPTH_MODE_NEVER:
            // Never writes back new Z
            return false;
        case DEPTH_MODE_LESS:
            passed = z < old_z;
            break;
        case DEPTH_MODE_EQUAL:
            // No need to write new Z
            return z == old_z;
        case DEPTH_MODE_LESS_OR_EQUAL:
            passed = z <= old_z;
            break;
        case DEPTH_MODE_GREATER:
            passed = z > old_z;
            break;
        case DEPTH_MODE_NOT_EQUAL:
            passed = z != old_z;
            break;
        case DEPTH_MODE_GREATER_OR_EQUAL:
            passed = z >= old_z;
            break;
        case DEPTH_MODE_ALWAYS:
            break;
    }

    if (passed && !ctx.isp_instr.regular.disable_z_write) {
        ctx.depth_buffer[SCREEN_WIDTH * y + x] = z;
    }

    return passed;
}

enum {
    COMBINE_MODE_MODULATE       = 1,
    COMBINE_MODE_MODULATE_ALPHA = 3,
};

static u8 color_multiply(const u8 color, const u8 other_color) {
    return (color * other_color) / 255;
}

static Color combine_colors(const Color vertex_color, const Color texel_color) {
    Color color{};

    switch (ctx.tsp_instr.shading_instr) {
        case COMBINE_MODE_MODULATE:
            color.a = texel_color.a;
            color.r = color_multiply(vertex_color.r, texel_color.r);
            color.g = color_multiply(vertex_color.g, texel_color.g);
            color.b = color_multiply(vertex_color.b, texel_color.b);
            break;
        case COMBINE_MODE_MODULATE_ALPHA:
            color.a = color_multiply(vertex_color.a, texel_color.a);
            color.r = color_multiply(vertex_color.r, texel_color.r);
            color.g = color_multiply(vertex_color.g, texel_color.g);
            color.b = color_multiply(vertex_color.b, texel_color.b);
            break;
        default:
            std::printf("Unimplemented shading instruction %u\n", ctx.tsp_instr.shading_instr);
            exit(1);
    }

    return color;
}

enum {
    BLEND_FUNCTION_ZERO                 = 0,
    BLEND_FUNCTION_ONE                  = 1,
    BLEND_FUNCTION_SOURCE_ALPHA         = 4,
    BLEND_FUNCTION_INVERSE_SOURCE_ALPHA = 5,
};

static void blend_and_flush(const Color source_color, const u32 x, const u32 y) {
    Color src = source_color;

    if (ctx.tsp_instr.source_select) {
        src = Color{.raw = ctx.secondary_buffer[SCREEN_WIDTH * y + x]};
    }

    Color dst;

    if (ctx.tsp_instr.destination_select) {
        dst = Color{.raw = ctx.secondary_buffer[SCREEN_WIDTH * y + x]};
    } else {
        dst = Color{.raw = ctx.color_buffer[SCREEN_WIDTH * y + x]};
    }

    switch (ctx.tsp_instr.source_instr) {
        case BLEND_FUNCTION_ONE:
            // Nothing to do here
            break;
        case BLEND_FUNCTION_SOURCE_ALPHA:
            src.r = color_multiply(src.r, src.a);
            src.g = color_multiply(src.g, src.a);
            src.b = color_multiply(src.b, src.a);
            src.a = color_multiply(src.a, src.a);
            break;
        default:
            std::printf("Unimplemented source blend function %u\n", ctx.tsp_instr.source_instr);
            exit(1);
    }

    switch (ctx.tsp_instr.destination_instr) {
        case BLEND_FUNCTION_ZERO:
            dst.raw = 0;
            break;
        case BLEND_FUNCTION_INVERSE_SOURCE_ALPHA:
            dst.a = color_multiply(dst.a, 255 - src.a);
            dst.r = color_multiply(dst.r, 255 - src.a);
            dst.g = color_multiply(dst.g, 255 - src.a);
            dst.b = color_multiply(dst.b, 255 - src.a);
            break;
        default:
            std::printf("Unimplemented destination blend function %u\n", ctx.tsp_instr.destination_instr);
            exit(1);
    }

    dst = add_and_clamp(src, dst);
    
    if (ctx.tsp_instr.destination_select) {
        ctx.secondary_buffer[SCREEN_WIDTH * y + x] = dst.raw;
    } else {
        ctx.color_buffer[SCREEN_WIDTH * y + x] = dst.raw;
    }
}

static void draw_triangle(const Vertex* vertices) {
    const Vertex& a = vertices[0];
    Vertex b = vertices[1];
    Vertex c = vertices[2];

    if (edge_function(a, b, c) < 0.0) {
        std::swap(b, c);
    }

    const f32 area = edge_function(a, b, c);

    // Calculate bounding box
    const int x_min = std::max(std::min(c.x, std::min(a.x, b.x)), 0.0F);
    const int x_max = std::min(std::max(c.x, std::max(a.x, b.x)), (f32)SCREEN_WIDTH - 1);
    const int y_min = std::max(std::min(c.y, std::min(a.y, b.y)), 0.0F);
    const int y_max = std::min(std::max(c.y, std::max(a.y, b.y)), (f32)SCREEN_HEIGHT - 1);

    if constexpr (!SILENT_PVR) std::printf("PVR Bounding box (xmin: %d, xmax: %d, ymin: %d, ymax: %d)\n", x_min, x_max, y_min, y_max);

    if ((x_min >= x_max) || (y_min >= y_max)) {
        return;
    }

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            Vertex p{};

            p.x = (f32)x;
            p.y = (f32)y;

            // Calculate weights
            const f32 w0 = edge_function(b, c, p);
            const f32 w1 = edge_function(c, a, p);
            const f32 w2 = edge_function(a, b, p);

            if ((w0 >= 0.0) && (w1 >= 0.0) && (w2 >= 0.0)) {
                const f32 z = interpolate(w0, w1, w2, a.z, b.z, c.z, area);

                if (!depth_test(z, x, y)) {
                    continue;
                }

                Color color = c.color;

                if (!ctx.tsp_instr.use_alpha) {
                    color.a = 0xFF;
                } else if (ctx.isp_instr.regular.use_gouraud_shading) {
                    color.raw = interpolate_colors(w0, w1, w2, a, b, c, area);
                }

                if (ctx.isp_instr.regular.use_texture_mapping) {
                    f32 u = interpolate(w0, w1, w2, a.u / a.z, b.u / b.z, c.u / c.z, area);
                    f32 v = interpolate(w0, w1, w2, a.v / a.z, b.v / b.z, c.v / c.z, area);

                    const f32 correct_z = interpolate(w0, w1, w2, 1.0 / a.z, 1.0 / b.z, 1.0 / c.z, area);

                    u *= 1.0 / correct_z;
                    v *= 1.0 / correct_z;

                    if (ctx.tsp_instr.clamp_u) {
                        u = clamp_uv(u);
                    } else {
                        u = repeat_uv(u);
                    }

                    if (ctx.tsp_instr.clamp_v) {
                        v = clamp_uv(v);
                    } else {
                        v = repeat_uv(v);
                    }

                    const int tex_x = ctx.u_size * u;
                    const int tex_y = ctx.v_size * v;

                    color = combine_colors(
                        color,
                        unpack_texel(read_texel<u16>(tex_x, tex_y))
                    );
                }

                blend_and_flush(color, x, y);
            }
        }
    }
}

void finish_render() {
    /* FILE* file = std::fopen("frame_dump.ppm", "w+");

    std::fprintf(file, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);

    for (u32 color : ctx.color_buffer) {
        fputc(color >> 16, file);
        fputc(color >> 8, file);
        fputc(color, file);
    }

    std::fclose(file); */
}

void initialize() {
    core::initialize();
    interface::initialize();
    spg::initialize();
    ta::initialize();
}

void reset() {
    core::reset();
    interface::reset();
    spg::reset();
    ta::reset();

    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {
    core::shutdown();
    interface::shutdown();
    spg::shutdown();
    ta::shutdown();
}

void set_isp_instruction(const IspInstruction isp_instr) {
    ctx.isp_instr = isp_instr;
}

void set_tsp_instruction(const TspInstruction tsp_instr) {
    ctx.tsp_instr = tsp_instr;

    // Update settings
    ctx.u_size = 8 << tsp_instr.u_size;
    ctx.v_size = 8 << tsp_instr.v_size;
}

void set_texture_control(const TextureControlWord texture_control) {
    ctx.texture_control = texture_control;

    // Update settings
    ctx.texture_addr = texture_control.regular.texture_addr * sizeof(u64);
}

void set_translucent(const bool is_translucent) {
    ctx.is_translucent = is_translucent;
}

void clear_buffers() {
    ctx.color_buffer.fill(0);
    ctx.secondary_buffer.fill(0);
    ctx.depth_buffer.fill(0.0);
}

void submit_triangle(const Vertex* vertices) {
    draw_triangle(vertices);
}

u32* get_color_buffer_ptr() {
    return ctx.color_buffer.data();
}

// For HOLLY access
u8* get_video_ram_ptr() {
    return ctx.video_ram.data();
}

}
