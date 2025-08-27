/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/core.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::pvr::core {

enum : u32 {
    IO_SOFTRESET     = 0x005F8008,
    IO_SDRAM_REFRESH = 0x005F80A0,
    IO_SDRAM_CFG     = 0x005F80A8,
};

#define SDRAM_REFRESH ctx.sdram.refresh
#define SDRAM_CFG     ctx.sdram.configuration

struct {
    struct {
        u32 refresh;
        u32 configuration;
    } sdram;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped PVR CORE read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
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
        case IO_SDRAM_REFRESH:
            std::printf("SDRAM_REFRESH write32 = %08X\n", data);
        
            SDRAM_REFRESH = data;
            break;
        case IO_SDRAM_CFG:
            std::printf("SDRAM_CFG write32 = %08X\n", data);
        
            SDRAM_CFG = data;
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
