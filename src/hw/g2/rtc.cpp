/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g2/rtc.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <scheduler.hpp>

namespace hw::g2::rtc {

enum : u32 {
    IO_RTC_HI   = 0x00710000,
    IO_RTC_LO   = 0x00710004,
    IO_RTC_PROT = 0x00710008,
};

#define RTC ctx.counter

struct {
    union {
        u32 raw;

        struct {
            u16 lo;
            u16 hi;
        };
    } counter;

    bool enable_writes;
} ctx;

static void increment_counter(const int);

static void schedule_increment() {
    scheduler::schedule_event(
        "AICA_RTC",
        increment_counter,
        0,
        scheduler::SCHEDULER_CLOCKRATE
    );
}

static void increment_counter(const int) {
    RTC.raw++;

    schedule_increment();
}

void initialize() {
    schedule_increment();
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped RTC read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_RTC_HI:
            std::puts("G2_RTC_HI read32");

            return RTC.hi;
        case IO_RTC_LO:
            std::puts("G2_RTC_LO read32");

            return RTC.lo;
        default:
            std::printf("Unmapped RTC read32 @ %08X\n", addr);
            exit(1);
    }
}

template u8 read(u32);
template u16 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped RTC write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_RTC_HI:
            std::printf("RTC_HI write32 = %08X\n", data);
            
            if (ctx.enable_writes) {
                ctx.counter.hi = data;

                ctx.enable_writes = false;
            }
            break;
        case IO_RTC_LO:
            std::printf("RTC_LO write32 = %08X\n", data);
            
            if (ctx.enable_writes) {
                ctx.counter.lo = data;
            }
            break;
        case IO_RTC_PROT:
            std::printf("RTC_PROT write32 = %08X\n", data);

            ctx.enable_writes = (data & 1) != 0;
            break;
        default:
            std::printf("Unmapped RTC write32 @ %08X = %08X\n",  addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

}
