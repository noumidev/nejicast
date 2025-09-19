/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// PVR functions
namespace hw::pvr {

union Color {
    u32 raw;

    struct {
        u8 b;
        u8 g;
        u8 r;
        u8 a;
    };
};

struct Vertex {
    f32 x, y, z;
    Color color;
};

void initialize();
void reset();
void shutdown();

void set_gouraud_shading(const bool use_gouraud_shading);

void clear_buffers();
void submit_triangle(const Vertex* vertices);
void finish_render();

u32* get_color_buffer_ptr();

}
