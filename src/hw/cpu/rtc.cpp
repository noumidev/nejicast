/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/cpg.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::rtc {

#define RMONAR  ctx.rtc_month_alarm
#define RCR1    ctx.rtc_control_1

struct {
    union {
        u8 raw;

        struct {
            u8 ones   : 4;
            u8 tens   : 1;
            u8        : 2;
            u8 enable : 1;
        };
    } rtc_month_alarm;

    union {
        u8 raw;

        struct {
            u8 alarm_flag             : 1;
            u8                        : 2;
            u8 alarm_interrupt_enable : 1;
            u8 carry_interrupt_enable : 1;
            u8                        : 2;
            u8 carry_flag             : 1;
        };
    } rtc_control_1;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

void set_rtc_month_alarm(const u8 data) {
    RMONAR.raw = data;
}

void set_rtc_control_1(const u8 data) {
    RCR1.raw = data;
}

}
