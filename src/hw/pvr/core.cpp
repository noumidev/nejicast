/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/core.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <hw/pvr/spg.hpp>

namespace hw::pvr::core {

enum : u32 {
    IO_SOFTRESET       = 0x005F8008,
    IO_VO_BORDER_COLOR = 0x005F8040,
    IO_FB_R_CTRL       = 0x005F8044,
    IO_SDRAM_REFRESH   = 0x005F80A0,
    IO_SDRAM_CFG       = 0x005F80A8,
    IO_SPG_HBLANK_INT  = 0x005F80C8,
    IO_SPG_VBLANK_INT  = 0x005F80CC,
    IO_SPG_CONTROL     = 0x005F80D0,
    IO_SPG_HBLANK      = 0x005F80D4,
    IO_SPG_LOAD        = 0x005F80D8,
    IO_SPG_VBLANK      = 0x005F80DC,
    IO_SPG_WIDTH       = 0x005F80E0,
    IO_VO_CONTROL      = 0x005F80E8,
    IO_VO_STARTX       = 0x005F80EC,
    IO_VO_STARTY       = 0x005F80F0,
    IO_SPG_STATUS      = 0x005F810C,
};

#define VO_BORDER_COLOR ctx.video_output.border_color
#define FB_R_CTRL       ctx.frame_buffer.read_control
#define SDRAM_REFRESH   ctx.sdram.refresh
#define SDRAM_CFG       ctx.sdram.configuration
#define VO_CONTROL      ctx.video_output.control
#define VO_STARTX       ctx.video_output.horizontal_start
#define VO_STARTY       ctx.video_output.vertical_start

struct {
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
    } frame_buffer;

    struct {
        u32 refresh;
        u32 configuration;
    } sdram;

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
} ctx;

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

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_VO_BORDER_COLOR:
            std::puts("VO_BORDER_COLOR read32");

            return VO_BORDER_COLOR.raw;
        case IO_VO_CONTROL:
            std::puts("VO_CONTROL read32");

            return VO_CONTROL.raw;
        case IO_SPG_STATUS:
            // std::puts("SPG_STATUS read32");

            return spg::get_status();
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
    switch (addr) {
        case IO_SOFTRESET:
            std::printf("SOFTRESET write32 = %08X\n", data);
            break;
        case IO_FB_R_CTRL:
            std::printf("FB_R_CTRL write32 = %08X\n", data);
        
            FB_R_CTRL.raw = data;
            break;
        case IO_SDRAM_REFRESH:
            std::printf("SDRAM_REFRESH write32 = %08X\n", data);
        
            SDRAM_REFRESH = data;
            break;
        case IO_SDRAM_CFG:
            std::printf("SDRAM_CFG write32 = %08X\n", data);
        
            SDRAM_CFG = data;
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
        default:
            std::printf("Unmapped PVR CORE write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

}
