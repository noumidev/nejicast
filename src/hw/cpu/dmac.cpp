/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/dmac.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <scheduler.hpp>
#include <hw/holly/bus.hpp>
#include <hw/holly/intc.hpp>

namespace hw::cpu::ocio::dmac {

#define SAR0    ctx.dma_channels[CHANNEL_0].source_address
#define DAR0    ctx.dma_channels[CHANNEL_0].destination_address
#define DMATCR0 ctx.dma_channels[CHANNEL_0].transfer_count
#define CHCR0   ctx.dma_channels[CHANNEL_0].control
#define SAR1    ctx.dma_channels[CHANNEL_1].source_address
#define DAR1    ctx.dma_channels[CHANNEL_1].destination_address
#define DMATCR1 ctx.dma_channels[CHANNEL_1].transfer_count
#define CHCR1   ctx.dma_channels[CHANNEL_1].control
#define SAR2    ctx.dma_channels[CHANNEL_2].source_address
#define DAR2    ctx.dma_channels[CHANNEL_2].destination_address
#define DMATCR2 ctx.dma_channels[CHANNEL_2].transfer_count
#define CHCR2   ctx.dma_channels[CHANNEL_2].control
#define SAR3    ctx.dma_channels[CHANNEL_3].source_address
#define DAR3    ctx.dma_channels[CHANNEL_3].destination_address
#define DMATCR3 ctx.dma_channels[CHANNEL_3].transfer_count
#define CHCR3   ctx.dma_channels[CHANNEL_3].control
#define DMAOR   ctx.dma_operation

struct {
    struct {
        u32 source_address;
        u32 destination_address;
        u32 transfer_count;

        union {
            u32 raw;

            struct {
                u32 enable_dmac              : 1;
                u32 transfer_end             : 1;
                u32 enable_interrupt         : 1;
                u32                          : 1;
                u32 transmit_size            : 3;
                u32 burst_mode               : 1;
                u32 select_resource          : 4;
                u32 source_mode              : 2;
                u32 destination_mode         : 2;
                u32 acknowledge_level        : 1;
                u32 acknowledge_mode         : 1;
                u32 request_check_level      : 1;
                u32 select_dreq              : 1;
                u32                          : 4;
                u32 destination_wait_control : 1;
                u32 destination_attribute    : 3;
                u32 source_wait_control      : 1;
                u32 source_attribute         : 3;
            };
        } control;
    } dma_channels[NUM_CHANNELS];

    union {
        u32 raw;

        struct {
            u32 master_enable_dmac :  1;
            u32 nmi_flag           :  1;
            u32 address_error_flag :  1;
            u32                    :  5;
            u32 priority_mode      :  2;
            u32                    :  6;
            u32 on_demand_mode     :  1;
            u32                    : 16;
        };
    } dma_operation;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u32 get_control(const int channel) {
    assert(channel < NUM_CHANNELS);

    return ctx.dma_channels[channel].control.raw;
}

void set_source_address(const int channel, const u32 data) {
    assert(channel < NUM_CHANNELS);

    ctx.dma_channels[channel].source_address = data;
}

void set_destination_address(const int channel, const u32 data) {
    assert(channel < NUM_CHANNELS);

    ctx.dma_channels[channel].destination_address = data;
}

void set_transfer_count(const int channel, const u32 data) {
    assert(channel < NUM_CHANNELS);

    ctx.dma_channels[channel].transfer_count = data;
}

void set_control(const int channel, const u32 data) {
    assert(channel < NUM_CHANNELS);

    ctx.dma_channels[channel].control.raw = data;
}

void set_dma_operation(const u32 data) {
    DMAOR.raw = data;
}

constexpr int CHANNEL_2_INTERRUPT = 19;

void execute_channel_2_dma(u32 &start_address, u32 &length, bool &start) {
    assert(DMAOR.master_enable_dmac);
    assert(CHCR2.enable_dmac);
    assert(CHCR2.transmit_size == 4); // 32-byte

    assert((start_address % 32) == 0);
    assert((length % 32) == 0);

    if (CHCR2.enable_interrupt) {
        scheduler::schedule_event(
            "CH2_IRQ",
            hw::holly::intc::assert_normal_interrupt,
            CHANNEL_2_INTERRUPT,
            8 * length
        );
    }

    // TODO: mask this through CPU
    SAR2 &= 0x1FFFFFFF;
    
    u8 dma_bytes[32];

    while (length > 0) {
        hw::holly::bus::block_read(SAR2, dma_bytes);
        hw::holly::bus::block_write(start_address, dma_bytes);

        switch (CHCR2.source_mode) {
            case 0: // Fixed
            case 3: // ??
                break;
            case 1:
                SAR2 += sizeof(dma_bytes);
                break;
            case 2:
                SAR2 -= sizeof(dma_bytes);
                break;
        }

        switch (CHCR2.destination_mode) {
            case 0: // Fixed
            case 3: // ??
                break;
            case 1:
                start_address += sizeof(dma_bytes);
                break;
            case 2:
                start_address -= sizeof(dma_bytes);
                break;
        }

        length -= sizeof(dma_bytes);
    }

    start = false;
}

}
