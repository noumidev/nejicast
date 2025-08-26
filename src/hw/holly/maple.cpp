/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/holly/maple.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::holly::maple {

enum : u32 {
    IO_MDSTAR = 0x005F6C04,
    IO_MDTSEL = 0x005F6C10,
    IO_MDEN   = 0x005F6C14,
    IO_MDST   = 0x005F6C18,
    IO_MSYS   = 0x005F6C80,
    IO_MDAPRO = 0x005F6C8C,
    IO_MMSEL  = 0x005F6CE8,
};

#define SB_MDSTAR ctx.command_table_address
#define SB_MDTSEL ctx.is_vblank_trigger
#define SB_MDEN   ctx.enable
#define SB_MDST   ctx.is_running
#define SB_MSYS   ctx.interface_control
#define SB_MDAPRO ctx.address_protection
#define SB_MMSEL  ctx.is_msb_bit_31

struct {
    u32 command_table_address;
    bool is_vblank_trigger;
    bool enable;
    bool is_running;

    union {
        u32 raw;

        struct {
            u32 delay_time      :  4;
            u32                 :  4;
            u32 sending_rate    :  2;
            u32                 :  2;
            u32 single_trigger  :  1;
            u32                 :  3;
            u32 timeout_counter : 16;
        };
    } interface_control;

    union {
        u16 raw;

        struct {
            u16 bottom_address : 7;
            u16                : 1;
            u16 top_address    : 7;
            u16                : 1;
        };
    } address_protection;

    bool is_msb_bit_31;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped MAPLE read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped MAPLE write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_MDSTAR:
            std::printf("SB_MDSTAR write32 = %08X\n", data);

            SB_MDSTAR = data;
            break;
        case IO_MDTSEL:
            std::printf("SB_MDTSEL write32 = %08X\n", data);

            SB_MDTSEL = (data & 1) != 0;
            break;
        case IO_MDEN:
            std::printf("SB_MDEN write32 = %08X\n", data);

            SB_MDEN = (data & 1) != 0;
            break;
        case IO_MDST:
            std::printf("SB_MDST write32 = %08X\n", data);
            
            assert((data & 1) == 0);
            break;
        case IO_MSYS:
            std::printf("SB_MSYS write32 = %08X\n", data);

            SB_MSYS.raw = data;
            break;
        case IO_MDAPRO:
            std::printf("SB_MDAPRO write32 = %08X\n", data);

            if ((data & ~0xFFFF) == 0x61550000) {
                SB_MDAPRO.raw = (u16)data;
            }
            break;
        case IO_MMSEL:
            std::printf("SB_MMSEL write32 = %08X\n", data);

            SB_MMSEL = (data & 1) != 0;
            break;
        default:
            std::printf("Unmapped MAPLE write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

}
