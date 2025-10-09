/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/pvr.hpp>

#include <algorithm>
#include <array>
#include <cassert>
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

    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> color_buffer;
    std::array<f32, SCREEN_WIDTH * SCREEN_HEIGHT> depth_buffer;

    bool use_gouraud_shading;
    bool use_texture_mapping;

    // Texture control
    int u_size;
    int v_size;

    u32 texture_format;
    bool use_swizzling;

    u32 texture_address;
} ctx;

template<typename T>
static T read_texel(const u32, const u32) {
    std::printf("Unmapped texture memory read%zu\n", 8 * sizeof(T));
    exit(1);
}

template<>
u16 read_texel(const u32 x, const u32 y) {
    u32 addr = ctx.texture_address;

    if (ctx.use_swizzling) {
        u32 z = 0;

        for (u32 i = 0; i < 16; i++) {
            z |= ((y >> i) & 1) << (2 * i);
            z |= ((x >> i) & 1) << (2 * i + 1);
        }

        addr += 2 * z;
    } else {
        addr += 2 * (ctx.u_size * y + x);
    }

    const u32 offset = addr >> 2;

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
    const int x_max = std::min(std::max(c.x, std::max(a.x, b.x)), (f32)SCREEN_WIDTH);
    const int y_min = std::max(std::min(c.y, std::min(a.y, b.y)), 0.0F);
    const int y_max = std::min(std::max(c.y, std::max(a.y, b.y)), (f32)SCREEN_HEIGHT);

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

                if (z < ctx.depth_buffer[SCREEN_WIDTH * y + x]) {
                    continue;
                }

                Color color = c.color;

                if (ctx.use_texture_mapping) {
                    f32 u = interpolate(w0, w1, w2, a.u / a.z, b.u / b.z, c.u / c.z, area);
                    f32 v = interpolate(w0, w1, w2, a.v / a.z, b.v / b.z, c.v / c.z, area);

                    const f32 correct_z = interpolate(w0, w1, w2, 1.0 / a.z, 1.0 / b.z, 1.0 / c.z, area);

                    u *= 1.0 / correct_z;
                    v *= 1.0 / correct_z;

                    assert((u >= 0.0) && (u <= 1.0));
                    assert((v >= 0.0) && (v <= 1.0));

                    const int tex_x = ctx.u_size * u;
                    const int tex_y = ctx.v_size * v;

                    const u16 tex_color = read_texel<u16>(tex_x, tex_y);

                    if (tex_color == 0) {
                        continue;
                    }

                    switch (ctx.texture_format) {
                        case TEXTURE_FORMAT_RGB565:
                            color.a = 0xFF;
                            color.r = (tex_color >> 11) << 3;
                            color.g = (tex_color >>  5) << 2;
                            color.b = (tex_color >>  0) << 3;
                            color.r |= color.r >> 5;
                            color.g |= color.g >> 6;
                            color.b |= color.b >> 5;
                            break;
                        case TEXTURE_FORMAT_ARGB4444:
                            color.a = (tex_color >> 12) << 4;
                            color.r = (tex_color >>  8) << 4;
                            color.g = (tex_color >>  4) << 4;
                            color.b = (tex_color >>  0) << 4;
                            color.a |= color.a >> 4;
                            color.r |= color.r >> 4;
                            color.g |= color.g >> 4;
                            color.b |= color.b >> 4;
                            break;
                        default:
                            std::printf("PVR Unimplemented texture format %u\n", ctx.texture_format);
                            exit(1);
                    }
                } else if (ctx.use_gouraud_shading) {
                    color.raw = interpolate_colors(w0, w1, w2, a, b, c, area);
                }

                ctx.color_buffer[SCREEN_WIDTH * y + x] = color.raw;
                ctx.depth_buffer[SCREEN_WIDTH * y + x] = z;
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

void set_gouraud_shading(const bool use_gouraud_shading) {
    ctx.use_gouraud_shading = use_gouraud_shading;
}

void set_texture_format(const u32 texture_format) {
    ctx.texture_format = texture_format;
}

void set_texture_mapping(const bool use_texture_mapping) {
    ctx.use_texture_mapping = use_texture_mapping;
}

void set_texture_size(const int u_size, const int v_size) {
    ctx.u_size = u_size;
    ctx.v_size = v_size;
}

void set_texture_swizzling(const bool use_swizzling) {
    ctx.use_swizzling = use_swizzling;
}

void set_texture_address(const u32 addr) {
    ctx.texture_address = addr;
}

void clear_buffers() {
    ctx.color_buffer.fill(0);
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
