/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/bsc.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define BCR1   ctx.bus_control_1
#define BCR2   ctx.bus_control_2
#define WCR1   ctx.wait_control_1
#define WCR2   ctx.wait_control_2
#define WCR3   ctx.wait_control_3
#define MCR    ctx.memory_control
#define RTCSR  ctx.refresh_timer_control
#define RTCNT  ctx.refresh_timer
#define RTCOR  ctx.refresh_time_constant
#define RFCR   ctx.refresh_count
#define PCTRA  ctx.ports[PORT_A].control
#define PDTRA  ctx.ports[PORT_A].latched_data
#define PCTRB  ctx.ports[PORT_B].control
#define PDTRB  ctx.ports[PORT_B].latched_data
#define GPIOIC ctx.gpio_interrupt_control
#define SDMR3  ctx.sdram_mode_3

namespace hw::cpu::ocio::bsc {

struct {
    union {
        u32 raw;

        struct {
            u32 a56_pcmcia       : 1;
            u32                  : 1;
            u32 a23_memory_type  : 3;
            u32 enable_a6_burst  : 3;
            u32 enable_a5_burst  : 3;
            u32 a0_burst_control : 3;
            u32 high_z_control   : 2;
            u32 dma_burst        : 1;
            u32 enable_mpx       : 1;
            u32 partial_sharing  : 1;
            u32 enable_breq      : 1;
            u32 a4_byte_control  : 1;
            u32 a1_byte_control  : 1;
            u32                  : 2;
            u32 opup_control     : 1;
            u32 ipup_control     : 1;
            u32 dpup_control     : 1;
            u32                  : 2;
            u32 a0_mpx           : 1;
            u32 slave_mode       : 1;
            u32 little_endian    : 1;
        };
    } bus_control_1;

    union {
        u16 raw;

        struct {
            u16 enable_port  : 1;
            u16              : 1;
            u16 a1_bus_width : 2;
            u16 a2_bus_width : 2;
            u16 a3_bus_width : 2;
            u16 a4_bus_width : 2;
            u16 a5_bus_width : 2;
            u16 a6_bus_width : 2;
            u16 a0_bus_width : 2;
        };
    } bus_control_2;

    union {
        u32 raw;

        struct {
            u32 a0_idle_cycles  : 3;
            u32                 : 1;
            u32 a1_idle_cycles  : 3;
            u32                 : 1;
            u32 a2_idle_cycles  : 3;
            u32                 : 1;
            u32 a3_idle_cycles  : 3;
            u32                 : 1;
            u32 a4_idle_cycles  : 3;
            u32                 : 1;
            u32 a5_idle_cycles  : 3;
            u32                 : 1;
            u32 a6_idle_cycles  : 3;
            u32                 : 1;
            u32 dma_idle_cycles : 3;
            u32                 : 1;
        };
    } wait_control_1;

    union {
        u32 raw;

        struct {
            u32 a0_burst_pitch : 3;
            u32 a0_wait_states : 3;
            u32 a1_wait_states : 3;
            u32 a2_wait_states : 3;
            u32                : 1;
            u32 a3_wait_states : 3;
            u32                : 1;
            u32 a4_wait_states : 3;
            u32 a5_burst_pitch : 3;
            u32 a5_wait_states : 3;
            u32 a6_burst_pitch : 3;
            u32 a6_wait_states : 3;
        };
    } wait_control_2;

    union {
        u32 raw;

        struct {
            u32 a0_hold_time     : 2;
            u32 a0_strobe_time   : 1;
            u32                  : 1;
            u32 a1_hold_time     : 2;
            u32 a1_strobe_time   : 1;
            u32 a1_negate_timing : 1;
            u32 a2_hold_time     : 2;
            u32 a2_strobe_time   : 1;
            u32                  : 1;
            u32 a3_hold_time     : 2;
            u32 a3_strobe_time   : 1;
            u32                  : 1;
            u32 a4_hold_time     : 2;
            u32 a4_strobe_time   : 1;
            u32 a4_negate_timing : 1;
            u32 a5_hold_time     : 2;
            u32 a5_strobe_time   : 1;
            u32                  : 1;
            u32 a6_hold_time     : 2;
            u32 a6_strobe_time   : 1;
            u32                  : 5;
        };
    } wait_control_3;

    union {
        u32 raw;

        struct {
            u32 edo_mode              : 1;
            u32 refresh_mode          : 1;
            u32 refresh_control       : 1;
            u32 address_multiplexing  : 4;
            u32 dram_width            : 2;
            u32 enable_burst          : 1;
            u32 refresh_period        : 3;
            u32 write_precharge_delay : 3;
            u32 ras_cas_delay         : 2;
            u32                       : 1;
            u32 ras_precharge_period  : 3;
            u32                       : 1;
            u32 cas_negation_period   : 1;
            u32                       : 3;
            u32 ras_precharge_time    : 3;
            u32 mode_set              : 1;
            u32 ras_down              : 1;
        };
    } memory_control;

    union {
        u16 raw;

        struct {
            u16 select_limit              : 1;
            u16 enable_overflow_interrupt : 1;
            u16 overflow_flag             : 1;
            u16 select_clock              : 3;
            u16 enable_match_interrupt    : 1;
            u16 match_flag                : 1;
            u16                           : 8;
        };
    } refresh_timer_control;

    u16 refresh_timer;
    u16 refresh_time_constant;
    u16 refresh_count;

    struct {
        u32 control;
        u16 latched_data;
    } ports[NUM_PORTS];

    u16 gpio_interrupt_control;

    u16 sdram_mode_3;
} ctx;

void initialize() {
    BCR2.raw = 0x3FFC;
    WCR1.raw = 0x77777777;
    WCR2.raw = 0xFFFEEFFF;
    WCR3.raw = 0x07777777;
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

// Simulates DRAM refresh
static void refresh_dram() {
    constexpr u16 COUNT_LIMIT[2] = {1024, 512};

    RTCNT++;

    if (RTCNT >= RTCOR) {
        RTCNT = 0;

        RTCSR.match_flag = 1;

        if (RTCSR.enable_match_interrupt) {
            std::puts("Unimplemented SH-4 refresh timer match interrupt");
            exit(1);
        }

        RFCR++;

        if (RFCR >= COUNT_LIMIT[RTCSR.select_limit]) {
            RFCR = 0;

            RTCSR.overflow_flag = 1;

            if (RTCSR.enable_overflow_interrupt) {
                std::puts("Unimplemented SH-4 refresh count overflow interrupt");
                exit(1);
            }
        }
    }
}

constexpr int NUM_PINS = 16;

enum {
    PIN_0             = 0,
    PIN_1             = 1,
    PIN_VIDEO_MODE_LO = 8,
    PIN_VIDEO_MODE_HI = 9,
};

enum {
    VIDEO_MODE_VGA,
};

static u16 read_port_a() {
    u16 port_data = 0;

    for (int i = 0; i < NUM_PINS; i++) {
        const bool is_output = ((PCTRA >> (2 * i + 0)) & 1) != 0;
        const bool is_pull_up = ((PCTRA >> (2 * i + 1)) & 1) == 0;

        if (is_output) {
            // Return latched value
            port_data |= PDTRA & (1 << i);
        } else {
            switch (i) {
                case PIN_0:
                case PIN_1:
                    port_data |= 1 << i;
                    break;
                case PIN_VIDEO_MODE_LO:
                    port_data |= (VIDEO_MODE_VGA & 1) << 8;
                    break;
                case PIN_VIDEO_MODE_HI:
                    port_data |= (VIDEO_MODE_VGA & 2) << 8;
                    break;
                default:
                    if (is_pull_up) {
                        port_data |= 1 << i;
                    }
                    break;
            }
        }
        
        std::printf("Pin %d read (output: %d, pull-up: %d)\n", i, is_output, is_pull_up);
    }

    // On Dreamcast, these two pins are shorted
    if ((port_data & 3) != 3) {
        port_data &= ~3;
    }

    std::printf("Port A data = %04X\n", port_data);

    return port_data;
}

u16 get_refresh_count() {
    // HACK
    refresh_dram();

    return RFCR;
}

u32 get_port_control(const int port) {
    assert(port < NUM_PORTS);

    return ctx.ports[port].control;
}

u16 get_port_data(const int port) {
    // TODO: implement port B reads
    assert(port == PORT_A);

    return read_port_a();
}

void set_bus_control_1(const u32 data) {
    BCR1.raw = data;
}

void set_bus_control_2(const u16 data) {
    BCR2.raw = data;
}

void set_wait_control_1(const u32 data) {
    WCR1.raw = data;
}

void set_wait_control_2(const u32 data) {
    WCR2.raw = data;
}

void set_wait_control_3(const u32 data) {
    WCR3.raw = data;
}

void set_memory_control(const u16 data) {
    MCR.raw = data;
}

void set_refresh_timer_control(const u16 data) {
    if ((data & 0xFF00) == 0xA500) {
        RTCSR.raw = data & 0xFF;
    }
}

void set_refresh_time_constant(const u16 data) {
    if ((data & 0xFF00) == 0xA500) {
        RTCOR = data & 0xFF;
    }
}

void set_refresh_count(const u16 data) {
    if ((data & 0xFC00) == 0xA400) {
        RFCR = data & 0x3FF;
    }
}

void set_port_control(const int port, const u32 data) {
    assert(port < NUM_PORTS);

    ctx.ports[port].control = data;
}

void set_port_data(const int port, const u16 data) {
    assert(port < NUM_PORTS);

    const u32& port_control = (port == PORT_A) ? PCTRA : PCTRB;
    u16& port_data = (port == PORT_A) ? PDTRA : PDTRB;

    for (int i = 0; i < NUM_PINS; i++) {
        const bool is_output = ((port_control >> (2 * i)) & 1) != 0;

        if (is_output) {
            const u16 bit = (data >> i) & 1;

            std::printf("Port %d:%d write = %u\n", 'A' + port, i, bit);
        }
    }

    // Update latched value
    port_data = data;
}

void set_gpio_interrupt_control(const u16 data) {
    GPIOIC = data;
}

void set_sdram_mode_3(const u16 data) {
    SDMR3 = data;
}

}
