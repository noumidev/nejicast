/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/pvr/spg.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <scheduler.hpp>
#include <hw/holly/intc.hpp>

namespace hw::pvr::spg {

#define SPG_CONTROL    ctx.control
#define SPG_HBLANK     ctx.hblank_control
#define SPG_HBLANK_INT ctx.hblank_interrupt
#define SPG_LOAD       ctx.load
#define SPG_STATUS     ctx.status
#define SPG_VBLANK     ctx.vblank_control
#define SPG_VBLANK_INT ctx.vblank_interrupt
#define SPG_WIDTH      ctx.width

#define HCOUNTER ctx.horizontal_counter
#define VCOUNTER ctx.status.scanline

struct {
    u32 horizontal_counter;
    u32 hblank_lines;

    union {
        u32 raw;

        struct {
            u32 hsync_polarity            :  1;
            u32 vsync_polarity            :  1;
            u32 csync_polarity            :  1;
            u32 external_synchronization  :  1;
            u32 enable_interlacing        :  1;
            u32 force_field_2             :  1;
            u32 ntsc_mode                 :  1;
            u32 pal_mode                  :  1;
            u32 synchronization_direction :  1;
            u32 csync_on_hsync            :  1;
            u32                           : 22;
        };
    } control;

    union {
        u32 raw;

        struct {
            u32 start : 10;
            u32       :  6;
            u32 end   : 10;
            u32       :  6;
        };
    } hblank_control;

    union {
        u32 raw;

        struct {
            u32 compare_line   : 10;
            u32                :  2;
            u32 interrupt_mode :  2;
            u32                :  2;
            u32 in_position    : 10;
            u32                :  6;
        };
    } hblank_interrupt;

    union {
        u32 raw;

        struct {
            u32 horizontal_count : 10;
            u32                  :  6;
            u32 vertical_count   : 10;
            u32                  :  6;
        };
    } load;

    union {
        u32 raw;

        struct {
            u32 scanline     : 10;
            u32 field_number :  1;
            u32 blank_flag   :  1;
            u32 hsync_flag   :  1;
            u32 vsync_flag   :  1;
            u32              : 18;
        };
    } status;

    union {
        u32 raw;

        struct {
            u32 start : 10;
            u32       :  6;
            u32 end   : 10;
            u32       :  6;
        };
    } vblank_control;

    union {
        u32 raw;

        struct {
            u32 in_position  : 10;
            u32              :  6;
            u32 out_position : 10;
            u32              :  6;
        };
    } vblank_interrupt;

    union {
        u32 raw;

        struct {
            u32 hsync            : 7;
            u32                  : 1;
            u32 vsync            : 4;
            u32 broad_pulse      : 10;
            u32 equivalent_pulse : 10;
        };
    } width;
} ctx;

constexpr int VBLANK_IN_INTERRUPT = 3;
constexpr int VBLANK_OUT_INTERRUPT = 4;
constexpr int HBLANK_INTERRUPT = 5;

enum {
    HBLANK_MODE_ONESHOT,
    HBLANK_MODE_COUNT,
    HBLANK_MODE_EVERY_LINE,
};

static void hblank(const int);

static void schedule_hblank() {
    scheduler::schedule_event(
        "HBLANK",
        hblank,
        0,
        scheduler::to_scheduler_cycles<scheduler::PIXEL_CLOCKRATE>(SPG_LOAD.horizontal_count)
    );
}

static void hblank(const int) {
    switch (SPG_HBLANK_INT.interrupt_mode) {
        case HBLANK_MODE_ONESHOT:
            if (VCOUNTER == SPG_HBLANK_INT.compare_line) {
                hw::holly::intc::assert_normal_interrupt(HBLANK_INTERRUPT);
            }
            break;
        case HBLANK_MODE_COUNT:
            if (ctx.hblank_lines < SPG_HBLANK_INT.compare_line) {
                hw::holly::intc::assert_normal_interrupt(HBLANK_INTERRUPT);

                ctx.hblank_lines++;
            }
            break;
        case HBLANK_MODE_EVERY_LINE:
            hw::holly::intc::assert_normal_interrupt(HBLANK_INTERRUPT);
            break;
    }

    VCOUNTER++;

    if (VCOUNTER == SPG_VBLANK_INT.in_position) {
        hw::holly::intc::assert_normal_interrupt(VBLANK_IN_INTERRUPT);
    } else if (VCOUNTER == SPG_VBLANK_INT.out_position) {
        hw::holly::intc::assert_normal_interrupt(VBLANK_OUT_INTERRUPT);
    }

    if (VCOUNTER >= SPG_LOAD.vertical_count) {
        VCOUNTER -= SPG_LOAD.vertical_count;

        ctx.hblank_lines = 0;
    }

    SPG_STATUS.vsync_flag = (VCOUNTER <= SPG_VBLANK.end) || (VCOUNTER >= SPG_VBLANK.start);
    SPG_STATUS.blank_flag = SPG_STATUS.hsync_flag | SPG_STATUS.vsync_flag;

    schedule_hblank();
}

void initialize() {
    SPG_HBLANK_INT.raw = 0x031D0000;
    SPG_VBLANK_INT.raw = 0x01500104;
    SPG_HBLANK.raw = 0x007E0345;
    SPG_LOAD.raw = 0x01060359;
    SPG_VBLANK.raw = 0x01500104;

    schedule_hblank();
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u32 get_status() {
    return SPG_STATUS.raw;
}

u32 get_vblank_control() {
    return SPG_VBLANK.raw;
}

void set_control(const u32 data) {
    SPG_CONTROL.raw = data;
}

void set_hblank_control(const u32 data) {
    SPG_HBLANK.raw = data;
}

void set_hblank_interrupt(const u32 data) {
    SPG_HBLANK_INT.raw = data;
}

void set_load(const u32 data) {
    SPG_LOAD.raw = data;
}

void set_vblank_control(const u32 data) {
    SPG_VBLANK.raw = data;
}

void set_vblank_interrupt(const u32 data) {
    SPG_VBLANK_INT.raw = data;
}

void set_width(const u32 data) {
    SPG_WIDTH.raw = data;
}

}
