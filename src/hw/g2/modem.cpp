/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g2/modem.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::g2::modem {

enum : u32 {
    IO_ID1  = 0x00600004,
};

enum {
    ID1_NO_MODEM = 0xFF, // Is this what happens on hardware?
};

struct {} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped MODEM read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u8 read(const u32 addr) {
    switch (addr) {
        case IO_ID1:
            std::puts("MODEM_ID1 read8");

            return ID1_NO_MODEM;
        default:
            std::printf("Unmapped MODEM read8 @ %08X\n", addr);
            exit(1);
    }
}

template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped MODEM write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u32);
template void write(u32, u64);

}
