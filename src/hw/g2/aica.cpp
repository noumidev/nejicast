/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g2/aica.hpp>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::g2::aica {

enum : u32 {
    IO_ARMRST = 0x00702C00,
};

#define ARMRST ctx.arm_reset

constexpr usize WAVE_RAM_SIZE = 0x200000;

struct {
    std::array<u8, WAVE_RAM_SIZE> wave_ram;

    union {
        u32 raw;

        struct {
            u32 reset_arm  :  1;
            u32            :  7;
            u32 video_mode :  2;
            u32            : 22;
        };
    } arm_reset;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped AICA read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_ARMRST:
            std::puts("ARMRST read32");

            return ARMRST.raw;
        default:
            std::printf("Unhandled AICA read32 @ %08X\n", addr);

            return 0;
    }
}

template u8 read(u32);
template u16 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped AICA write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_ARMRST:
            std::printf("ARMRST write32 = %08X\n", data);

            ARMRST.raw = data;
            break;
        default:
            // For now, ignore all registers
            std::printf("Unhandled AICA write32 @ %08X = %08X\n", addr, data);
            break;
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

// For HOLLY access
u8* get_wave_ram_ptr() {
    return ctx.wave_ram.data();
}

}
