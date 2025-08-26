/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/interface.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::pvr::interface {

enum : u32 {
    IO_PDSTAP = 0x005F7C00,
    IO_PDSTAR = 0x005F7C04,
    IO_PDLEN  = 0x005F7C08,
    IO_PDDIR  = 0x005F7C0C,
    IO_PDTSEL = 0x005F7C10,
    IO_PDEN   = 0x005F7C14,
    IO_PDST   = 0x005F7C18,
    IO_PDAPRO = 0x005F7C80,
};

#define SB_PDSTAP ctx.pvr_dma.pvr_start_address
#define SB_PDSTAR ctx.pvr_dma.ram_start_address
#define SB_PDLEN  ctx.pvr_dma.length
#define SB_PDDIR  ctx.pvr_dma.from_pvr
#define SB_PDTSEL ctx.pvr_dma.is_interrupt_trigger
#define SB_PDEN   ctx.pvr_dma.enable
#define SB_PDST   ctx.pvr_dma.is_running
#define SB_PDAPRO ctx.address_protection

struct {
    struct {
        u32 pvr_start_address;
        u32 ram_start_address;
        u32 length;
        bool from_pvr;
        bool is_interrupt_trigger;
        bool enable;
        bool is_running;
    } pvr_dma;

    union {
        u16 raw;

        struct {
            u16 bottom_address : 7;
            u16                : 1;
            u16 top_address    : 7;
            u16                : 1;
        };
    } address_protection;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped PVR I/F read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped PVR I/F write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_PDSTAP:
            std::printf("SB_PDSTAP write32 = %08X\n", data);

            SB_PDSTAP = data;
            break;
        case IO_PDSTAR:
            std::printf("SB_PDSTAR write32 = %08X\n", data);

            SB_PDSTAR = data;
            break;
        case IO_PDLEN:
            std::printf("SB_PDLEN write32 = %08X\n", data);

            SB_PDLEN = data;
            break;
        case IO_PDDIR:
            std::printf("SB_PDDIR write32 = %08X\n", data);

            SB_PDDIR = (data & 1) != 0;
            break;
        case IO_PDTSEL:
            std::printf("SB_PDTSEL write32 = %08X\n", data);

            SB_PDTSEL = (data & 1) != 0;
            break;
        case IO_PDEN:
            std::printf("SB_PDEN write32 = %08X\n", data);

            SB_PDEN = (data & 1) != 0;
            break;
        case IO_PDST:
            std::printf("SB_PDST write32 = %08X\n", data);

            assert((data & 1) == 0);
            break;
        case IO_PDAPRO:
            std::printf("SB_MDAPRO write32 = %08X\n", data);

            if ((data & ~0xFFFF) == 0x67020000) {
                SB_PDAPRO.raw = (u16)data;
            }
            break;
        default:
            std::printf("Unmapped PVR I/F write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

}
