/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/ubc.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::ubc {

#define BASRA ctx.break_channels[CHANNEL_A].asid
#define BASRB ctx.break_channels[CHANNEL_B].asid
#define BARA  ctx.break_channels[CHANNEL_A].address
#define BARB  ctx.break_channels[CHANNEL_B].address
#define BAMRA ctx.break_channels[CHANNEL_A].address_mask
#define BAMRB ctx.break_channels[CHANNEL_B].address_mask
#define BBRA  ctx.break_channels[CHANNEL_A].bus_cycle
#define BBRB  ctx.break_channels[CHANNEL_B].bus_cycle
#define BRCR  ctx.break_control

struct {
    struct {
        u8 asid;
        u32 address;
        u8 address_mask;

        union {
            u16 raw;

            struct {
                u16 select_size_01    : 2;
                u16 select_read_write : 2;
                u16 select_access     : 2;
                u16 select_size_2     : 1;
                u16                   : 9;
            };
        } bus_cycle;
    } break_channels[NUM_CHANNELS];

    union {
        u16 raw;

        struct {
            u16 enable_user_break_debug   : 1;
            u16                           : 2;
            u16 select_sequence_condition : 1;
            u16                           : 2;
            u16 select_pc_break_b         : 1;
            u16 enable_data_break_b       : 1;
            u16                           : 2;
            u16 select_pc_break_a         : 1;
            u16                           : 2;
            u16 condition_match_flag_b    : 1;
            u16 condition_match_flag_a    : 1;
        };
    } break_control;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

void set_asid(const int channel, const u8 data) {
    assert(channel < NUM_CHANNELS);

    ctx.break_channels[channel].asid = data;
}

void set_address(const int channel, const u32 data) {
    assert(channel < NUM_CHANNELS);

    ctx.break_channels[channel].address = data;
}

void set_address_mask(const int channel, const u8 data) {
    assert(channel < NUM_CHANNELS);

    ctx.break_channels[channel].address_mask = data;
}

void set_bus_cycle(const int channel, const u16 data) {
    assert(channel < NUM_CHANNELS);

    ctx.break_channels[channel].bus_cycle.raw = data;
}

void set_break_control(const u16 data) {
    BRCR.raw = data;
}

}
