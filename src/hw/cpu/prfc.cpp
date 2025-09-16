/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/prfc.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::prfc {

#define PMCR0 ctx.channels[CHANNEL_0].control
#define PMCR1 ctx.channels[CHANNEL_1].control

struct {
    struct {
        union {
            u16 raw;

            struct {
                u16 event_mode : 7;
                u16            : 1;
                u16 clock_type : 1;
                u16            : 4;
                u16 clear      : 1;
                u16 start      : 1;
                u16 enable     : 1;
            };
        } control;
    } channels[NUM_CHANNELS];
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u16 get_control(const int channel) {
    assert(channel < NUM_CHANNELS);

    return ctx.channels[channel].control.raw;
}

void set_control(const int channel, const u16 data) {
    assert(channel < NUM_CHANNELS);

    ctx.channels[channel].control.raw = data;
}

}
