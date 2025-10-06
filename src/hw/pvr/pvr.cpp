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

    u32 texture_address;
} ctx;

template<typename T>
static T read_texel(const u32, const u32) {
    std::printf("Unmapped texture memory read%zu\n", 8 * sizeof(T));
    exit(1);
}

template<>
u16 read_texel(const u32 x, const u32 y) {
    const u32 addr = ctx.texture_address + (ctx.u_size * y + x) * 2;
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

    std::printf("PVR Bounding box (xmin: %d, xmax: %d, ymin: %d, ymax: %d)\n", x_min, x_max, y_min, y_max);

    if ((x_min >= x_max) || (y_min >= y_max)) {
        return;
    }

    for (int y = y_min; y < y_max; y++) {
        for (int x = x_min; x < x_max; x++) {
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
                    const f32 u = interpolate(w0, w1, w2, a.u, b.u, c.u, area);
                    const f32 v = interpolate(w0, w1, w2, a.v, b.v, c.v, area);

                    assert((u >= 0.0) && (u <= 1.0));
                    assert((v >= 0.0) && (v <= 1.0));

                    const int tex_x = ctx.u_size * u;
                    const int tex_y = ctx.v_size * v;

                    const u16 tex_color = read_texel<u16>(tex_x, tex_y);

                    color.a = 0xFF;
                    color.r = (tex_color >> 11) << 3;
                    color.g = (tex_color >> 5) << 2;
                    color.b = (tex_color >> 0) << 3;
                    color.r |= color.r >> 5;
                    color.g |= color.g >> 6;
                    color.b |= color.b >> 5;
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

void set_texture_mapping(const bool use_texture_mapping) {
    ctx.use_texture_mapping = use_texture_mapping;
}

void set_texture_size(const int u_size, const int v_size) {
    ctx.u_size = u_size;
    ctx.v_size = v_size;
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
