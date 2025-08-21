/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/ocio.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio {

namespace Address {
    enum : u32 {
        ExceptionEvent = 0x1F000024,
    };
}

#define EXPEVT ctx.exception_event

struct {
    u32 exception_event;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped SH-4 P4 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case Address::ExceptionEvent:
            std::puts("EXPEVT read32");

            return EXPEVT;
        default:
            std::printf("Unmapped SH-4 P4 read32 @ %08X\n", addr);
            exit(1);
    }
}

template u8 read(u32);
template u16 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped write%zu @ %08X = %0*X\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), data);
    exit(1);
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u32);

void set_exception_event(const u32 event) {
    EXPEVT = event;
}

}
