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
    f32 u, v;
    Color color;
};

void initialize();
void reset();
void shutdown();

void set_gouraud_shading(const bool use_gouraud_shading);
void set_texture_format(const u32 texture_format);
void set_texture_mapping(const bool use_texture_mapping);
void set_texture_size(const int u_size, const int v_size);
void set_texture_swizzling(const bool use_swizzling);
void set_texture_address(const u32 addr);

void clear_buffers();
void submit_triangle(const Vertex* vertices);
void finish_render();

u32* get_color_buffer_ptr();
u8* get_video_ram_ptr();

}
