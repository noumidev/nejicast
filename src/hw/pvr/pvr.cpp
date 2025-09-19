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

#include <hw/pvr/core.hpp>
#include <hw/pvr/interface.hpp>
#include <hw/pvr/spg.hpp>
#include <hw/pvr/ta.hpp>

namespace hw::pvr {

constexpr int SCREEN_WIDTH = 640;
constexpr int SCREEN_HEIGHT = 480;

struct {
    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> color_buffer;
    std::array<f32, SCREEN_WIDTH * SCREEN_HEIGHT> depth_buffer;
} ctx;

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
            Vertex p{(f32)x, (f32)y, 0.0, {0}};

            // Calculate weights
            const f32 w0 = edge_function(b, c, p);
            const f32 w1 = edge_function(c, a, p);
            const f32 w2 = edge_function(a, b, p);

            if ((w0 >= 0.0) && (w1 >= 0.0) && (w2 >= 0.0)) {
                const f32 z = interpolate(w0, w1, w2, a.z, b.z, c.z, area);

                if (z < ctx.depth_buffer[SCREEN_WIDTH * y + x]) {
                    continue;
                }

                ctx.color_buffer[SCREEN_WIDTH * y + x] = interpolate_colors(w0, w1, w2, a, b, c, area);
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

}
