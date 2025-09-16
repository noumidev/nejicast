/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/scif.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::scif {

#define SCSMR2  ctx.serial_mode_2
#define SCBRR2  ctx.bit_rate_2
#define SCSCR2  ctx.serial_control_2
#define SCFSR2  ctx.serial_status_2
#define SCFCR2  ctx.fifo_control_2
#define SCSPTR2 ctx.serial_port_2
#define SCLSR2  ctx.overrun_error

struct {
    union {
        u16 raw;

        struct {
            u16 select_clock     : 2;
            u16                  : 1;
            u16 stop_length      : 1;
            u16 parity_mode      : 1;
            u16 enable_parity    : 1;
            u16 character_length : 1;
            u16                  : 9;
        };
    } serial_mode_2;

    u8 bit_rate_2;

    union {
        u16 raw;

        struct {
            u16                                : 1;
            u16 enable_clock_1                 : 1;
            u16                                : 1;
            u16 enable_receive_error_interrupt : 1;
            u16 enable_receive                 : 1;
            u16 enable_transmit                : 1;
            u16 enable_receive_interrupt       : 1;
            u16 enable_transmit_interrupt      : 1;
            u16                                : 8;
        };
    } serial_control_2;

    union {
        u16 raw;

        struct {
            u16 receive_data_ready  : 1;
            u16 receive_fifo_full   : 1;
            u16 parity_error        : 1;
            u16 framing_error       : 1;
            u16 detect_break        : 1;
            u16 transmit_fifo_empty : 1;
            u16 transmit_end        : 1;
            u16 receive_error       : 1;
            u16 num_framing_errors  : 4;
            u16 num_parity_errors   : 4;
        };
    } serial_status_2;

    union {
        u16 raw;

        struct {
            u16 test_loopback              : 1;
            u16 reset_receive_fifo         : 1;
            u16 reset_transmit_fifo        : 1;
            u16 enable_modem_control       : 1;
            u16 transmit_fifo_data_trigger : 2;
            u16 receive_fifo_data_trigger  : 2;
            u16                            : 8;
        };
    } fifo_control_2;

    union {
        u16 raw;

        struct {
            u16 port_break_data : 1;
            u16 port_break_io   : 1;
            u16                 : 2;
            u16 cts_port_data   : 1;
            u16 cts_port_io     : 1;
            u16 rts_port_data   : 1;
            u16 rts_port_io     : 1;
            u16                 : 8;
        };
    } serial_port_2;

    bool overrun_error;
} ctx;

void initialize() {
    SCBRR2 = 0xFF;
    SCFSR2.raw = 0x06;
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u16 get_serial_status_2() {
    return SCFSR2.raw;
}

u16 get_line_status_2() {
    return SCLSR2;
}

void set_serial_mode_2(const u16 data) {
    SCSMR2.raw = data;
}

void set_bit_rate_2(const u8 data) {
    SCBRR2 = data;
}

void set_serial_control_2(const u16 data) {
    SCSCR2.raw = data;
}

void set_serial_status_2(const u16 data) {
    SCFSR2.raw = data;
}

void set_fifo_control_2(const u16 data) {
    SCFCR2.raw = data;
}

void set_serial_port_2(const u16 data) {
    SCSPTR2.raw = data;
}

void set_line_status_2(const u16 data) {
    SCLSR2 = data;
}

}
