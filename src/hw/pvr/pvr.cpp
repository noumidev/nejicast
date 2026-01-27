/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025-2026  noumidev
 */

#include <hw/pvr/pvr.hpp>

#include <algorithm>
#include <array>
#include <bit>
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

    u8 alpha_reference;

    // TSP
    u32 u_size, v_size;

    // Texture control
    u32 texture_addr;

    int polygon_type;
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
u8 read_vram_interleaved(const u32 addr) {
    u32 offset = ((addr >> 3) << 2);

    if ((addr & 4) != 0) {
        offset += 0x400000;
    }

    u32 data;

    std::memcpy(&data, &ctx.video_ram[offset], sizeof(data));

    return data >> ((addr & 3) * 8);
}

template<>
u16 read_vram_interleaved(const u32 addr) {
    u32 offset = ((addr >> 3) << 2);

    if ((addr & 4) != 0) {
        offset += 0x400000;
    }

    u32 data;

    std::memcpy(&data, &ctx.video_ram[offset], sizeof(data));

    return ((addr & 2) != 0) ? data >> 16 : data;
}

template u32 read_vram_interleaved(u32);

static u32 interleave_bits(u32 n) {
    n = (n | (n << 8)) & 0x00FF00FF;
    n = (n | (n << 4)) & 0x0F0F0F0F;
    n = (n | (n << 2)) & 0x33333333;
    n = (n | (n << 1)) & 0x55555555;

    return n;
}

static u32 swizzle_to_linear(u32 x, u32 y) {
    const int u_size = ctx.u_size;
    const int v_size = ctx.v_size;

    const u32 min_dim = std::min(u_size, v_size);
    const u32 mask = min_dim - 1;

    u32 linear_offset = interleave_bits(y & mask) | (interleave_bits(x & mask) << 1);

    if (u_size > v_size) {
        linear_offset |= ((x & ~mask) << (31 - std::countl_zero(min_dim)));
    } else if (v_size > u_size) {
        linear_offset |= ((y & ~mask) << (31 - std::countl_zero(min_dim)));
    }

    return linear_offset;
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
    assert(!ctx.texture_control.regular.use_mipmapping || (ctx.u_size == ctx.v_size));

    u32 addr = ctx.texture_addr;

    if (ctx.texture_control.regular.scan_order == SCAN_ORDER_SWIZZLED) {
        assert(!ctx.texture_control.regular.select_stride);

        const u32 offset = swizzle_to_linear(x, y);

        if (ctx.texture_control.regular.use_compression) {
            // Get index
            const u8 index = read_vram_interleaved<u8>(addr + 0x800 + offset / 4);

            return read_vram_interleaved<u16>(addr + sizeof(u64) * index + 2 * (offset & 3));
        } else {
            addr += 2 * offset;
        }
    } else {
        addr += 2 * ((ctx.texture_control.regular.select_stride ? core::get_stride() : ctx.u_size) * y + x);
    }

    return read_vram_interleaved<u16>(addr);
}

enum : u32 {
    TEXTURE_FORMAT_ARGB1555 = 0,
    TEXTURE_FORMAT_RGB565   = 1,
    TEXTURE_FORMAT_ARGB4444 = 2,
    TEXTURE_FORMAT_YUV422   = 3,
};

static Color unpack_texel(const u16 texel) {
    Color color;

    switch (ctx.texture_control.regular.pixel_format) {
        case TEXTURE_FORMAT_ARGB1555:
        case 7:
            color.a = (texel >> 15);
            color.r = (texel >> 10) << 3;
            color.g = (texel >>  5) << 3;
            color.b = (texel >>  0) << 3;
            color.a *= 0xFF;
            color.r |= color.r >> 5;
            color.g |= color.g >> 5;
            color.b |= color.b >> 5;
            break;
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
        case TEXTURE_FORMAT_YUV422:
            color.raw = 0xFFCC00CC;
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

static u8 clamp_color_channel_f32(const f32 channel) {
    if (channel < 0.0) {
        return 0;
    } else if (channel > 255.0) {
        return 255;
    }

    return (u8)channel;
}

static Color sample_nearest(const f32 tex_x, const f32 tex_y) {
    return unpack_texel(read_texel<u16>((int)tex_x, (int)tex_y));
}

static Color sample_bilinear(const f32 tex_x, const f32 tex_y) {
    const int x0 = (int)tex_x;
    const int x1 = std::min(x0 + 1, (int)(ctx.u_size - 1));
    const int y0 = (int)tex_y;
    const int y1 = std::min(y0 + 1, (int)(ctx.v_size - 1));

    const f32 dx = tex_x - x0;
    const f32 dy = tex_y - y0;

    Color colors[6] = {
        unpack_texel(read_texel<u16>(x0, y0)),
        unpack_texel(read_texel<u16>(x1, y0)),
        unpack_texel(read_texel<u16>(x0, y1)),
        unpack_texel(read_texel<u16>(x1, y1)),
    };

    // Blend top pixels
    colors[4].r = clamp_color_channel_f32((f32)colors[0].r * (1.0 - dx) + (f32)colors[1].r * dx);
    colors[4].g = clamp_color_channel_f32(colors[0].g * (1.0 - dx) + colors[1].g * dx);
    colors[4].b = clamp_color_channel_f32(colors[0].b * (1.0 - dx) + colors[1].b * dx);
    colors[4].a = clamp_color_channel_f32(colors[0].a * (1.0 - dx) + colors[1].a * dx);

    // Blend bottom pixels
    colors[5].r = clamp_color_channel_f32(colors[2].r * (1.0 - dx) + colors[3].r * dx);
    colors[5].g = clamp_color_channel_f32(colors[2].g * (1.0 - dx) + colors[3].g * dx);
    colors[5].b = clamp_color_channel_f32(colors[2].b * (1.0 - dx) + colors[3].b * dx);
    colors[5].a = clamp_color_channel_f32(colors[2].a * (1.0 - dx) + colors[3].a * dx);

    return Color {
        .r = clamp_color_channel_f32(colors[4].r * (1.0 - dy) + colors[5].r * dy),
        .g = clamp_color_channel_f32(colors[4].g * (1.0 - dy) + colors[5].g * dy),
        .b = clamp_color_channel_f32(colors[4].b * (1.0 - dy) + colors[5].b * dy),
        .a = clamp_color_channel_f32(colors[4].a * (1.0 - dy) + colors[5].a * dy),
    };
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

static u8 clamp_color_channel(const int channel) {
    if (channel > 255) {
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
    return std::abs(std::fmodf(uv, 1.0));
}

static u32 interpolate_colors(
    const f32 w0,
    const f32 w1,
    const f32 w2,
    const Vertex& a,
    const Vertex& b,
    const Vertex& c,
    const f32 area,
    const f32 z
) {
    const u8 blue = clamp_color_channel(interpolate(w0, w1, w2, a.color.b * a.z, b.color.b * b.z, c.color.b * c.z, area) / z);
    const u8 green = clamp_color_channel(interpolate(w0, w1, w2, a.color.g * a.z, b.color.g * b.z, c.color.g * c.z, area) / z);
    const u8 red = clamp_color_channel(interpolate(w0, w1, w2, a.color.r * a.z, b.color.r * b.z, c.color.r * c.z, area) / z);
    const u8 alpha = clamp_color_channel(interpolate(w0, w1, w2, a.color.a * a.z, b.color.a * b.z, c.color.a * c.z, area) / z);

    return Color{.b = blue, .g = green, .r = red, .a = alpha}.raw;
}

static u32 interpolate_offset_colors(
    const f32 w0,
    const f32 w1,
    const f32 w2,
    const Vertex& a,
    const Vertex& b,
    const Vertex& c,
    const f32 area,
    const f32 z
) {
    const u8 blue = clamp_color_channel(interpolate(w0, w1, w2, a.offset_color.b * a.z, b.offset_color.b * b.z, c.offset_color.b * c.z, area) / z);
    const u8 green = clamp_color_channel(interpolate(w0, w1, w2, a.offset_color.g * a.z, b.offset_color.g * b.z, c.offset_color.g * c.z, area) / z);
    const u8 red = clamp_color_channel(interpolate(w0, w1, w2, a.offset_color.r * a.z, b.offset_color.r * b.z, c.offset_color.r * c.z, area) / z);
    const u8 alpha = clamp_color_channel(interpolate(w0, w1, w2, a.offset_color.a * a.z, b.offset_color.a * b.z, c.offset_color.a * c.z, area) / z);

    return Color{.b = blue, .g = green, .r = red, .a = alpha}.raw;
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
    COMBINE_MODE_DECAL          = 0,
    COMBINE_MODE_MODULATE       = 1,
    COMBINE_MODE_DECAL_ALPHA    = 2,
    COMBINE_MODE_MODULATE_ALPHA = 3,
};

static u8 color_multiply(const u8 color, const u8 other_color) {
    return (color * other_color) / 255;
}

static Color combine_colors(const Color vertex_color, const Color texel_color, const Color offset_color) {
    Color color{};

    switch (ctx.tsp_instr.shading_instr) {
        case COMBINE_MODE_DECAL:
            color = add_and_clamp(texel_color, offset_color);

            color.a = texel_color.a;
            break;
        case COMBINE_MODE_MODULATE:
            color.r = color_multiply(vertex_color.r, texel_color.r);
            color.g = color_multiply(vertex_color.g, texel_color.g);
            color.b = color_multiply(vertex_color.b, texel_color.b);

            color = add_and_clamp(color, offset_color);

            color.a = texel_color.a;
            break;
        case COMBINE_MODE_DECAL_ALPHA:
            color.r = clamp_color_channel(
                (int)color_multiply(texel_color.r, texel_color.a) +
                (int)color_multiply(vertex_color.r, 255 - texel_color.a)
            );
            color.g = clamp_color_channel(
                (int)color_multiply(texel_color.g, texel_color.a) +
                (int)color_multiply(vertex_color.g, 255 - texel_color.a)
            );
            color.b = clamp_color_channel(
                (int)color_multiply(texel_color.b, texel_color.a) +
                (int)color_multiply(vertex_color.b, 255 - texel_color.a)
            );

            color = add_and_clamp(color, offset_color);

            color.a = vertex_color.a;
            break;
        case COMBINE_MODE_MODULATE_ALPHA:
            color.r = color_multiply(vertex_color.r, texel_color.r);
            color.g = color_multiply(vertex_color.g, texel_color.g);
            color.b = color_multiply(vertex_color.b, texel_color.b);

            color = add_and_clamp(color, offset_color);

            color.a = color_multiply(vertex_color.a, texel_color.a);
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
    BLEND_FUNCTION_INVERSE_OTHER        = 3,
    BLEND_FUNCTION_SOURCE_ALPHA         = 4,
    BLEND_FUNCTION_INVERSE_SOURCE_ALPHA = 5,
    BLEND_FUNCTION_DESTINATION_ALPHA    = 6,
};

static void blend_and_flush(const Color source_color, const u32 x, const u32 y) {
    Color src = source_color;

    if (ctx.tsp_instr.source_select) {
        src = Color{.raw = ctx.secondary_buffer[SCREEN_WIDTH * y + x]};
    }

    Color dst;

    if (ctx.polygon_type == ta::LIST_TYPE_TRANSLUCENT) {
        // TODO: handle translucent modifier volumes
        if (ctx.tsp_instr.destination_select) {
            dst = Color{.raw = ctx.secondary_buffer[SCREEN_WIDTH * y + x]};
        } else {
            dst = Color{.raw = ctx.color_buffer[SCREEN_WIDTH * y + x]};
        }

        const Color src_saved = src;

        switch (ctx.tsp_instr.source_instr) {
            case BLEND_FUNCTION_ZERO:
                src.raw = 0;
                break;
            case BLEND_FUNCTION_ONE:
                // Nothing to do here
                break;
            case BLEND_FUNCTION_SOURCE_ALPHA:
                src.r = color_multiply(src.r, src_saved.a);
                src.g = color_multiply(src.g, src_saved.a);
                src.b = color_multiply(src.b, src_saved.a);
                src.a = color_multiply(src.a, src_saved.a);
                break;
            default:
                std::printf("Unimplemented source blend function %u\n", ctx.tsp_instr.source_instr);
                exit(1);
        }

        switch (ctx.tsp_instr.destination_instr) {
            case BLEND_FUNCTION_ZERO:
                dst.raw = 0;
                break;
            case BLEND_FUNCTION_ONE:
                // Nothing to do here
                break;
            case BLEND_FUNCTION_INVERSE_OTHER:
                dst.a = 255 - src_saved.a;
                dst.r = 255 - src_saved.r;
                dst.g = 255 - src_saved.g;
                dst.b = 255 - src_saved.b;
                break;
            case BLEND_FUNCTION_SOURCE_ALPHA:
                dst.r = color_multiply(dst.r, src_saved.a);
                dst.g = color_multiply(dst.g, src_saved.a);
                dst.b = color_multiply(dst.b, src_saved.a);
                dst.a = color_multiply(dst.a, src_saved.a);
                break;
            case BLEND_FUNCTION_INVERSE_SOURCE_ALPHA:
                dst.a = color_multiply(dst.a, 255 - src_saved.a);
                dst.r = color_multiply(dst.r, 255 - src_saved.a);
                dst.g = color_multiply(dst.g, 255 - src_saved.a);
                dst.b = color_multiply(dst.b, 255 - src_saved.a);
                break;
            case BLEND_FUNCTION_DESTINATION_ALPHA:
                dst.r = color_multiply(dst.r, dst.a);
                dst.g = color_multiply(dst.g, dst.a);
                dst.b = color_multiply(dst.b, dst.a);
                dst.a = color_multiply(dst.a, dst.a);
                break;
            default:
                std::printf("Unimplemented destination blend function %u\n", ctx.tsp_instr.destination_instr);
                exit(1);
        }

        dst = add_and_clamp(src, dst);
    } else {
        dst = src;
    }
    
    if (ctx.tsp_instr.destination_select) {
        ctx.secondary_buffer[SCREEN_WIDTH * y + x] = dst.raw;
    } else {
        ctx.color_buffer[SCREEN_WIDTH * y + x] = dst.raw;
    }
}

static void draw_triangle(const Vertex* vertices) {
    if (
        (ctx.polygon_type == ta::LIST_TYPE_OPAQUE_MODIFIER) ||
        (ctx.polygon_type == ta::LIST_TYPE_TRANSLUCENT_MODIFIER)
    ) {
        // TODO: handle modifier volumes
        return;
    }

    const Vertex& a = vertices[0];
    Vertex b = vertices[1];
    Vertex c = vertices[2];

    if (edge_function(a, b, c) < 0.0) {
        std::swap(b, c);
    }

    const f32 area = edge_function(a, b, c);

    // Calculate bounding box
    const int x_min = std::floor(std::max(std::min(c.x, std::min(a.x, b.x)), 0.0F));
    const int x_max = std::floor(std::min(std::max(c.x, std::max(a.x, b.x)), (f32)SCREEN_WIDTH - 1));
    const int y_min = std::floor(std::max(std::min(c.y, std::min(a.y, b.y)), 0.0F));
    const int y_max = std::floor(std::min(std::max(c.y, std::max(a.y, b.y)), (f32)SCREEN_HEIGHT - 1));

    if constexpr (!SILENT_PVR) std::printf("PVR Bounding box (xmin: %d, xmax: %d, ymin: %d, ymax: %d)\n", x_min, x_max, y_min, y_max);

    if ((x_min >= x_max) || (y_min >= y_max)) {
        return;
    }

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            Vertex p{.x = (f32)x, .y = (f32)y};

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
                Color offset_color = c.offset_color;

                if (ctx.isp_instr.regular.use_gouraud_shading) {
                    color.raw = interpolate_colors(w0, w1, w2, a, b, c, area, z);
                    offset_color.raw = interpolate_offset_colors(w0, w1, w2, a, b, c, area, z);
                }

                if (!ctx.tsp_instr.use_alpha) {
                    color.a = 0xFF;
                }

                if (ctx.isp_instr.regular.use_texture_mapping) {
                    f32 u = interpolate(w0, w1, w2, a.u * a.z, b.u * b.z, c.u * c.z, area) / z;
                    f32 v = interpolate(w0, w1, w2, a.v * a.z, b.v * b.z, c.v * c.z, area) / z;

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

                    if (ctx.tsp_instr.flip_u) {
                        u = 1.0 - u;
                    }

                    if (ctx.tsp_instr.flip_v) {
                        v = 1.0 - v;
                    }

                    const f32 tex_x = (ctx.u_size - 1) * u;
                    const f32 tex_y = (ctx.v_size - 1) * v;

                    const Color texel_color = (ctx.tsp_instr.filter_mode == 0) ? sample_nearest(tex_x, tex_y) : sample_bilinear(tex_x, tex_y);

                    color = combine_colors(
                        color,
                        texel_color,
                        offset_color
                    );
                }

                if (ctx.polygon_type == ta::LIST_TYPE_PUNCHTHROUGH) {
                    if (color.a < ctx.alpha_reference) {
                        continue;
                    }

                    // PT polygons are drawn with an alpha of 1.0 according to my sources
                    color.a = 0xFF;
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

void set_alpha_reference(const u8 alpha_reference) {
    ctx.alpha_reference = alpha_reference;
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

void set_polygon_type(const int list_type) {
    ctx.polygon_type = list_type;
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
