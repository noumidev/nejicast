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

union IspInstruction {
    u32 raw;

    struct {
        u32                     : 20;
        u32 d_control           :  1;
        u32 bypass_cache        :  1;
        u32 short_uv            :  1;
        u32 use_gouraud_shading :  1;
        u32 use_offset_color    :  1;
        u32 use_texture_mapping :  1;
        u32 disable_z_write     :  1;
        u32 culling_mode        :  2;
        u32 depth_mode          :  3;
    } regular;

    struct {
        u32              : 27;
        u32 culling_mode :  2;
        u32 volume_instr :  3;
    } modifier_volume;
};

static_assert(sizeof(IspInstruction) == sizeof(u32));

union TspInstruction {
    u32 raw;

    struct {
        u32 v_size             : 3;
        u32 u_size             : 3;
        u32 shading_instr      : 2;
        u32 d_adjust           : 4;
        u32 super_sample       : 1;
        u32 filter_mode        : 2;
        u32 clamp_v            : 1;
        u32 clamp_u            : 1;
        u32 flip_v             : 1;
        u32 flip_u             : 1;
        u32 ignore_tex_alpha   : 1;
        u32 use_alpha          : 1;
        u32 clamp_color        : 1;
        u32 fog_control        : 2;
        u32 destination_select : 1;
        u32 source_select      : 1;
        u32 destination_instr  : 3;
        u32 source_instr       : 3;
    };
};

static_assert(sizeof(TspInstruction) == sizeof(u32));

union TextureControlWord {
    u32 raw;

    struct {
        u32 texture_addr    : 21;
        u32                 :  4;
        u32 select_stride   :  1;
        u32 scan_order      :  1;
        u32 pixel_format    :  3;
        u32 use_compression :  1;
        u32 use_mipmapping  :  1;
    } regular;
};

static_assert(sizeof(TextureControlWord) == sizeof(u32));

void initialize();
void reset();
void shutdown();

template<typename T>
T read_vram_linear(const u32 addr);

template<typename T>
T read_vram_interleaved(const u32 addr);

void set_isp_instruction(const IspInstruction isp_instr);
void set_tsp_instruction(const TspInstruction tsp_instr);
void set_texture_control(const TextureControlWord texture_control);

void set_translucent(const bool is_translucent);

void clear_buffers();
void submit_triangle(const Vertex* vertices);
void finish_render();

u32* get_color_buffer_ptr();
u8* get_video_ram_ptr();

}
