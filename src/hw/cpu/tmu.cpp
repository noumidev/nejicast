/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/tmu.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::tmu {

#define TOCR  ctx.timer_output_control
#define TSTR  ctx.timer_start
#define TCOR0 ctx.timers[TIMER_0].constant
#define TCNT0 ctx.timers[TIMER_0].counter
#define TCR0  ctx.timers[TIMER_0].control
#define TCOR1 ctx.timers[TIMER_1].constant
#define TCNT1 ctx.timers[TIMER_1].counter
#define TCR1  ctx.timers[TIMER_1].control
#define TCOR2 ctx.timers[TIMER_2].constant
#define TCNT2 ctx.timers[TIMER_2].counter
#define TCR2  ctx.timers[TIMER_2].control

struct {
    union {
        u8 raw;

        struct {
            u8 timer_clock_control : 1;
            u8                     : 7;
        };
    } timer_output_control;

    union {
        u8 raw;

        struct {
            u8 start_counter : 3;
            u8               : 5;
        };
    } timer_start;

    struct {
        u32 constant;
        u32 counter;

        union {
            u16 raw;

            struct {
                u16 prescaler                  : 3;
                u16 clock_edge                 : 2;
                u16 enable_underflow_interrupt : 1;
                u16 enable_input_capture       : 2;
                u16 underflow_flag             : 1;
                u16 input_capture_flag         : 1;
                u16                            : 6;
            };
        } control;
    } timers[NUM_CHANNELS];
} ctx;

void initialize() {
    for (auto& timer : ctx.timers) {
        timer.constant = 0xFFFFFFFF;
        timer.counter = 0xFFFFFFFF;
    }
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u8 get_timer_start() {
    return TSTR.raw;
}

u32 get_counter(const int channel) {
    assert(channel < NUM_CHANNELS);

    // HACK
    return ctx.timers[channel].counter--;
}

u16 get_control(const int channel) {
    assert(channel < NUM_CHANNELS);

    return ctx.timers[channel].control.raw;
}

void set_timer_output_control(const u8 data) {
    TOCR.raw = data;
}

void set_timer_start(const u8 data) {
    TSTR.raw = data;
}

void set_constant(const int channel, const u32 data) {
    assert(channel < NUM_CHANNELS);

    ctx.timers[channel].constant = data;
}

void set_counter(const int channel, const u32 data) {
    assert(channel < NUM_CHANNELS);

    ctx.timers[channel].counter = data;
}

void set_control(const int channel, const u16 data) {
    assert(channel < NUM_CHANNELS);

    ctx.timers[channel].control.raw = data;
}

}
