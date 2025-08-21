/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g1/g1.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <common/file.hpp>

namespace hw::g1 {

constexpr usize BOOT_ROM_SIZE = 0x200000;

namespace Address {
    enum : u32 {
        G1Rrc = 0x005F7480,
    };
}

#define SB_G1RRC ctx.boot_rom_read_timing

struct {
    std::vector<u8> boot_rom;

    union {
        u16 raw;

        struct {
            u16 address_hold   :  3;
            u16                :  1;
            u16 address_setup  :  4;
            u16 cs_pulse_width :  4;
            u16 oe_pulse_delay :  1;
            u16                : 15;
        };
    } boot_rom_read_timing;
} ctx;

void initialize(const char* boot_path) {
    ctx.boot_rom = common::load_file(boot_path);

    assert(ctx.boot_rom.size() == BOOT_ROM_SIZE);
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped G1 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped G1 write%zu @ %08X = %0*X\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), data);
    exit(1);
}

template<>
void write(const u32 addr, const u16 data) {
    switch (addr) {
        case Address::G1Rrc:
            std::printf("SB_G1RRC write16 = %04X\n", data);

            SB_G1RRC.raw = data;
            break;
        default:
            std::printf("Unmapped G1 write16 @ %08X = %04X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u32);

// For HOLLY access
u8* get_boot_rom_ptr() {
    return ctx.boot_rom.data();
}

}
