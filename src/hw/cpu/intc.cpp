/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/intc.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::intc {

#define ICR  ctx.interrupt_control
#define IPRA ctx.interrupt_priority[PRIORITY_A]
#define IPRB ctx.interrupt_priority[PRIORITY_B]
#define IPRC ctx.interrupt_priority[PRIORITY_C]

struct {
    union {
        u16 raw;

        struct {
            u16                    : 7;
            u16 pin_mode           : 1;
            u16 select_nmi_edge    : 1;
            u16 nmi_block_mode     : 1;
            u16                    : 4;
            u16 nmi_interrupt_mask : 1;
            u16 nmi_input_level    : 1;
        };
    } interrupt_control;

    u16 interrupt_priority[NUM_PRIORITY_REGS];
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u16 get_priority(const int priority) {
    assert(priority < NUM_PRIORITY_REGS);

    return ctx.interrupt_priority[priority];
}

void set_interrupt_control(const u16 data) {
    ICR.raw = data;
}

void set_priority(const int priority, const u16 data) {
    assert(priority < NUM_PRIORITY_REGS);

    ctx.interrupt_priority[priority] = data;
}

}
