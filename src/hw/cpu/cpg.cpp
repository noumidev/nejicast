/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/cpg.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::cpg {

#define STBCR  ctx.standby_control
#define WTCNT  ctx.watchdog_timer_counter
#define WTCSR  ctx.watchdog_timer_control
#define STBCR2 ctx.standby_control_2

struct {
    union {
        u8 raw;

        struct {
            u8 stop_sci_clock  : 1;
            u8 stop_rtc_clock  : 1;
            u8 stop_tmu_clock  : 1;
            u8 stop_scif_clock : 1;
            u8 stop_dmac_clock : 1;
            u8 ppu_pup_control : 1;
            u8 ppu_hiz_control : 1;
            u8 standby_mode    : 1;
        };
    } standby_control;

    u8 watchdog_timer_counter;

    union {
        u8 raw;

        struct {
            u8 select_clock           : 3;
            u8 interval_overflow_flag : 1;
            u8 watchdog_overflow_flag : 1;
            u8 select_reset           : 1;
            u8 select_mode            : 1;
            u8 enable_timer           : 1;
        };
    } watchdog_timer_control;

    union {
        u8 raw;

        struct {
            u8                 : 7;
            u8 deep_sleep_mode : 1;
        };
    } standby_control_2;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u8 get_watchdog_timer_control() {
    return WTCNT;
}

void set_standby_control(const u8 data) {
    STBCR.raw = data;
}

void set_watchdog_timer_counter(const u16 data) {
    if ((data & 0xFF00) == 0x5A00) {
        WTCNT = (u8)data;
    }
}

void set_watchdog_timer_control(const u16 data) {
    if ((data & 0xFF00) == 0x5A00) {
        WTCSR.raw = (u8)data;
    }
}

void set_standby_control_2(const u8 data) {
    STBCR2.raw = data;
}

}
