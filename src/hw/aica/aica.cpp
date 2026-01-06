/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/aica/aica.hpp>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <hw/aica/arm.hpp>
#include <hw/aica/bus.hpp>

namespace hw::aica {

enum : u32 {
    IO_SLOTCTL_L  = 0x00700000,
    IO_ADSR_HL    = 0x00700014,
    IO_INCTL      = 0x00700020,
    IO_LPFADSR_LL = 0x00700040,
    IO_OUTCTL_0   = 0x00702000,
    IO_OUTCTL_17  = 0x00702044,
    IO_MVOL       = 0x00702800,
    IO_MIDIIN     = 0x00702808,
    IO_TIMA       = 0x00702890,
    IO_TACTL      = 0x00702891,
    IO_TIMB       = 0x00702894,
    IO_TBCTL      = 0x00702895,
    IO_TIMC       = 0x00702898,
    IO_TCCTL      = 0x00702899,
    IO_SCIEB_L    = 0x0070289C,
    IO_SCIRE_L    = 0x007028A4,
    IO_SCILV0     = 0x007028A8,
    IO_SCILV1     = 0x007028AC,
    IO_SCILV2     = 0x007028B0,
    IO_ARMRST     = 0x00702C00,
    IO_INTCLR     = 0x00702D04,
    IO_DSPCOEF    = 0x00703000,
};

#define  SLOT(x) ctx.slots[x]
#define TIMER(x) ctx.timers[x]

#define SLOTCTL(x) SLOT(x).control
#define    ADSR(x) SLOT(x).adsr
#define   INCTL(x) SLOT(x).dsp_send
#define LPFADSR(x) SLOT(x).lpf_adsr
#define MIDIIN     ctx.midi_in
#define   SCILV(x) ctx.arm_interrupt_level[x]
#define SCIEB      ctx.arm_interrupt_enable
#define ARMRST     ctx.arm_reset

constexpr usize WAVE_RAM_SIZE = 0x200000;

constexpr u32 ARM_BASE = 0x00800000;
constexpr u32 ARM_OFFSET = 0x100000;

constexpr int NUM_SLOTS = 64;
constexpr u32 SLOT_OFFSET = 0x1FFF;

constexpr int NUM_TIMERS = 3;

struct Slot {
    union {
        u16 raw;

        struct {
            u16        : 7; // [22:16] of SA
            u16 pcms   : 2;
            u16 lpctl  : 1;
            u16 ssctl  : 1;
            u16        : 3;
            u16 kyonb  : 1;
            u16 kyonex : 1;
        };
    } control;

    union {
        u32 raw;

        struct {
            u32 ar     : 5;
            u32        : 1;
            u32 d1r    : 5;
            u32 d2r    : 5;
            u32 rr     : 5;
            u32 dl     : 5;
            u32 krs    : 4;
            u32 lpslnk : 1;
            u32        : 1;
        };
    } adsr;

    union {
        u8 raw;

        struct {
            u8 isel : 4;
            u8 imxl : 4;
        };
    } dsp_send;

    union {

    } lpf_adsr;
};

struct Timer {
    u8 counter;
    u8 subcounter;
    u8 prescaler;
};

struct {
    std::array<u8, WAVE_RAM_SIZE> wave_ram;

    union {
        u8 raw;

        struct {
            u8 miemp : 1;
            u8 miful : 1;
            u8 miovf : 1;
            u8 moemp : 1;
            u8 moful : 1;
            u8       : 3;
        };
    } midi_in;

    u16 arm_interrupt_enable;

    u8 arm_interrupt_level[3];

    union {
        u32 raw;

        struct {
            u32 reset_arm  :  1;
            u32            :  7;
            u32 video_mode :  2;
            u32            : 22;
        };
    } arm_reset;

    Slot slots[NUM_SLOTS];

    Timer timers[NUM_TIMERS];
} ctx;

void initialize() {
    arm::initialize();
    bus::initialize();
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));

    // ARM held in reset on power-up
    ARMRST.reset_arm = 1;

    arm::assert_reset(true);

    bus::reset();

    MIDIIN.miemp = 1;
    MIDIIN.moemp = 1;
}

void shutdown() {
    arm::shutdown();
    bus::shutdown();
}

static void kyonex() {
    std::puts("AICA KYONEX event");

    for (int slot = 0; slot < NUM_SLOTS; slot++) {
        if (SLOTCTL(slot).kyonb) {
            std::printf("Slot %d key-on\n", slot);
            exit(1);
        } else {
            std::printf("Slot %d key-off\n", slot);
        }
    }
}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped AICA read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u32 read(const u32 addr) {
    u32 g2_addr = addr;

    if (g2_addr >= ARM_BASE) {
        g2_addr -= ARM_OFFSET;
    }

    switch (g2_addr) {
        case IO_MIDIIN:
            std::puts("MIDIIN read32");

            return MIDIIN.raw << 8;
        case IO_ARMRST:
            std::puts("ARMRST read32");

            return ARMRST.raw;
        default:
            std::printf("Unhandled AICA read32 @ %08X\n", addr);
            
            if (addr < ARM_BASE) {
                return 0;
            }

            exit(1);
    }
}

template u8 read(u32);
template u16 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped AICA write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u8 data) {
    u32 g2_addr = addr;

    if (g2_addr >= ARM_BASE) {
        g2_addr -= ARM_OFFSET;
    }

    switch (g2_addr) {
        case IO_SCILV0:
            std::printf("SCILV0 write8 = %02X\n", data);

            SCILV(0) = data;
            break;
        case IO_SCILV1:
            std::printf("SCILV1 write8 = %02X\n", data);

            SCILV(1) = data;
            break;
        case IO_SCILV2:
            std::printf("SCILV2 write8 = %02X\n", data);

            SCILV(2) = data;
            break;
        default:
            std::printf("Unmapped AICA write8 @ %08X = %02X\n", addr, data);
            
            if (addr < ARM_BASE) {
                break;
            }

            exit(1);
    }
}

template<>
void write(const u32 addr, const u32 data) {
    u32 g2_addr = addr;

    if (g2_addr >= ARM_BASE) {
        g2_addr -= ARM_OFFSET;
    }

    if ((g2_addr & ~SLOT_OFFSET) == IO_SLOTCTL_L) {
        // Slot registers
        const int slot = (addr & 0x1FFF) / 0x80;

        assert(slot < NUM_SLOTS);

        switch (g2_addr & ~0x1F80) {
            case IO_SLOTCTL_L:
                std::printf("Slot %d SLOTCTL write32 = %08X\n", slot, data);

                SLOTCTL(slot).raw = data;

                if (SLOTCTL(slot).kyonex) {
                    kyonex();

                    SLOTCTL(slot).kyonex = 0;
                }
                break;
            case IO_ADSR_HL:
                std::printf("Slot %d ADSR_HL write32 = %08X\n", slot, data);

                ADSR(slot).raw &= 0xFFFF;
                ADSR(slot).raw |= data << 16;
                break;
            case IO_INCTL:
                std::printf("Slot %d INCTL write32 = %08X\n", slot, data);

                INCTL(slot).raw = data;
                break;
            default:
                std::printf("AICA Unimplemented slot %d write32 @ %08X = %08X\n", slot, addr, data);
            
                if (addr < ARM_BASE) {
                    break;
                }

                exit(1);
        }

        return;
    }

    if ((g2_addr >= IO_OUTCTL_0) && (g2_addr <= IO_OUTCTL_17)) {
        const int slot = (g2_addr - IO_OUTCTL_0) >> 2;

        switch (slot) {
            case 16:
                std::printf("OUTCTL_CDDA_L write32 = %08X\n", data);
                break;
            case 17:
                std::printf("OUTCTL_CDDA_R write32 = %08X\n", data);
                break;
            default:
                std::printf("OUTCTL%d write32 = %08X\n", slot, data);
                break;
        }

        return;
    }

    if (g2_addr >= IO_DSPCOEF) {
        std::printf("AICA Unimplemented DSP write32 @ %08X = %08X\n", addr, data);
        return;
    }

    switch (g2_addr) {
        case IO_MVOL:
            std::printf("MVOL/MEMCTL write32 = %08X\n", data);
            break;
        case IO_TIMA:
            std::printf("TIMA/TACTL write32 = %08X\n", data);

            TIMER(0).counter = data;
            TIMER(0).prescaler = 1 << (u8)(data >> 8);
            break;
        case IO_TIMB:
            std::printf("TIMB/TBCTL write32 = %08X\n", data);

            TIMER(1).counter = data;
            TIMER(1).prescaler = 1 << (u8)(data >> 8);
            break;
        case IO_TIMC:
            std::printf("TIMC/TCCTL write32 = %08X\n", data);

            TIMER(2).counter = data;
            TIMER(2).prescaler = 1 << (u8)(data >> 8);
            break;
        case IO_SCIEB_L:
            std::printf("SCIEB write32 = %08X\n", data);
            
            SCIEB = data;
            break;
        case IO_SCIRE_L:
            std::printf("SCIRE write32 = %08X\n", data);
            
            // SCIPD &= ~data;
            break;
        case IO_ARMRST:
            std::printf("ARMRST write32 = %08X\n", data);

            ARMRST.raw = data;

            if (!ARMRST.reset_arm) {
                FILE* file = std::fopen("waveram.bin", "w+b");

                std::fwrite(ctx.wave_ram.data(), 1, WAVE_RAM_SIZE, file);
                std::fclose(file);
            }
            
            arm::assert_reset(ARMRST.reset_arm);
            break;
        case IO_INTCLR:
            std::printf("INTCLR write32 = %08X\n", data);
            break;
        default:
            std::printf("Unmapped AICA write32 @ %08X = %08X\n", addr, data);
            
            if (addr < ARM_BASE) {
                break;
            }

            exit(1);
    }
}

template void write(u32, u16);
template void write(u32, u64);

// For HOLLY access
u8* get_wave_ram_ptr() {
    return ctx.wave_ram.data();
}

}
