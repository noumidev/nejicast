/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/aica/aica.hpp>

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <scheduler.hpp>
#include <hw/aica/arm.hpp>
#include <hw/aica/bus.hpp>
#include <hw/holly/intc.hpp>

namespace hw::aica {

constexpr bool SILENT_AICA = true;

enum : u32 {
    IO_SLOTCTL_L  = 0x00700000,
    IO_SLOTCTL_H  = 0x00700001,
    IO_SA         = 0x00700004,
    IO_LSA        = 0x00700008,
    IO_LEA        = 0x0070000C,
    IO_ADSR_L     = 0x00700010,
    IO_ADSR_H     = 0x00700014,
    IO_PITCH      = 0x00700018,
    IO_LFOCTL_L   = 0x0070001C,
    IO_LFOCTL_H   = 0x0070001D,
    IO_INCTL      = 0x00700020,
    IO_DIPAN      = 0x00700024,
    IO_DISDL      = 0x00700025,
    IO_FCTL       = 0x00700028,
    IO_TL         = 0x00700029,
    IO_FLV0       = 0x0070002C,
    IO_FLV1       = 0x00700030,
    IO_FLV2       = 0x00700034,
    IO_FLV3       = 0x00700038,
    IO_FLV4       = 0x0070003C,
    IO_FADSR_L    = 0x00700040,
    IO_FADSR_H    = 0x00700044,
    IO_OUTCTL_0   = 0x00702000,
    IO_OUTCTL_17  = 0x00702044,
    IO_MVOL       = 0x00702800,
    IO_MEMCTL     = 0x00702801,
    IO_RBCTL      = 0x00702804,
    IO_MIDIIN     = 0x00702808,
    IO_MIDIOUT    = 0x0070280C,
    IO_EGSTAT_L   = 0x0070280D,
    IO_EGSTAT_M   = 0x00702810,
    IO_CA         = 0x00702814,
    IO_DMEA_H     = 0x00702880,
    IO_DMEA_L     = 0x00702884,
    IO_TIMA       = 0x00702890,
    IO_TACTL      = 0x00702891,
    IO_TIMB       = 0x00702894,
    IO_TBCTL      = 0x00702895,
    IO_TIMC       = 0x00702898,
    IO_TCCTL      = 0x00702899,
    IO_SCIEB_L    = 0x0070289C,
    IO_SCIPD_L    = 0x007028A0,
    IO_SCIRE_L    = 0x007028A4,
    IO_SCILV0     = 0x007028A8,
    IO_SCILV1     = 0x007028AC,
    IO_SCILV2     = 0x007028B0,
    IO_MCIEB_L    = 0x007028B4,
    IO_MCIPD_L    = 0x007028B8,
    IO_MCIRE_L    = 0x007028BC,
    IO_ARMRST     = 0x00702C00,
    IO_INTREQ     = 0x00702D00,
    IO_INTCLR     = 0x00702D04,
    IO_DSPCOEF    = 0x00703000,
};

#define  SLOT(x) ctx.slots[x]
#define TIMER(x) ctx.timers[x]

#define SLOTCTL(x) SLOT(x).control
#define      SA(x) SLOT(x).start_address
#define     LSA(x) SLOT(x).loop_start
#define     LEA(x) SLOT(x).loop_end
#define    ADSR(x) SLOT(x).adsr
#define   PITCH(x) SLOT(x).pitch
#define  LFOCTL(x) SLOT(x).lfo_control
#define   INCTL(x) SLOT(x).dsp_send
#define   DICTL(x) SLOT(x).direct_control
#define    FCTL(x) SLOT(x).filter_control
#define      TL(x) SLOT(x).total_level
#define     FLV(x) SLOT(x).filter_cutoff
#define   FADSR(x) SLOT(x).filter_adsr
#define LPFADSR(x) SLOT(x).lpf_adsr
#define RBCTL      ctx.ring_buffer_control
#define MIDIIN     ctx.midi_in
#define EGSTAT     ctx.attenuation_monitor
#define CA         ctx.current_address
#define DMEA       ctx.dma_address
#define     TIM(x) TIMER(x).counter
#define    TCTL(x) TIMER(x).prescaler
#define SCIEB      ctx.arm_interrupt_mask
#define SCIPD      ctx.arm_interrupt_flags
#define MCIEB      ctx.sh4_interrupt_mask
#define MCIPD      ctx.sh4_interrupt_flags
#define ARMRST     ctx.arm_reset
#define INTREQ     ctx.active_interrupt

constexpr usize WAVE_RAM_SIZE = 0x200000;

constexpr u32 ARM_BASE = 0x00800000;
constexpr u32 ARM_OFFSET = 0x100000;

constexpr int NUM_SLOTS = 64;
constexpr u32 SLOT_OFFSET = 0x1FFF;

constexpr int NUM_TIMERS = 3;

constexpr int INITIAL_ATTENUATION = 0x280;
constexpr int MAX_ATTENUATION = 0x3C0;

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

    u32 start_address;
    u32 current_address;
    u16 loop_start;
    u16 loop_end;

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
        u16 raw;

        struct {
            u16 fns : 10;
            u16 : 1;
            u16 oct : 4;
            u16 : 1;
        };
    } pitch;

    union {
        u16 raw;

        struct {
            u16 alfos  : 3;
            u16 alfows : 2;
            u16 plfos  : 3;
            u16 plfows : 2;
            u16 lfof   : 5;
            u16 re     : 1;
        };
    } lfo_control;

    union {
        u8 raw;

        struct {
            u8 isel : 4;
            u8 imxl : 4;
        };
    } dsp_send;

    union {
        u8 raw;

        struct {
            u8 pan : 5;
            u8 sdl : 3;
        };
    } direct_control;

    union {
        u8 raw;

        struct {
            u8 q     : 5;
            u8 lpoff : 1;
            u8 voff  : 1;
            u8       : 1;
        };
    } filter_control;

    u8 total_level;

    u16 filter_cutoff[5];

    union {
        u32 raw;

        struct {
            u32 fd1r : 5;
            u32      : 3;
            u32 far  : 5;
            u32      : 3;
            u32 frr  : 5;
            u32      : 3;
            u32 fd2r : 5;
            u32      : 3;
        };
    } filter_adsr;

    int attenuation;
    int adsr_state;
    int adsr_steps;
    int sample_count;
    bool looped;

    f32 phase;
};

enum {
    ADSR_STATE_ATTACK,
    ADSR_STATE_DECAY,
    ADSR_STATE_SUSTAIN,
    ADSR_STATE_RELEASE,
};

struct Timer {
    u8 counter;
    u8 subcounter;
    u8 prescaler;
};

struct {
    std::array<u8, WAVE_RAM_SIZE> wave_ram;

    union {
        u16 raw;

        struct {
            u16 rbp : 12;
            u16     :  1;
            u16 rbl :  2;
            u16     :  1;
        };
    } ring_buffer_control;

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

    union {
        u32 raw;

        struct {
            u32 mslc :  6;
            u32 af   :  1;
            u32      :  1;
            u32 eg   : 13;
            u32 sgc  :  2;
            u32 lp   :  1;
            u32      :  8;
        };
    } attenuation_monitor;

    u16 arm_interrupt_mask;
    u16 arm_interrupt_flags;

    u8 arm_interrupt_levels[8];

    u16 sh4_interrupt_mask;
    u16 sh4_interrupt_flags;

    u32 dma_address;

    union {
        u32 raw;

        struct {
            u32 reset_arm  :  1;
            u32            :  7;
            u32 video_mode :  2;
            u32            : 22;
        };
    } arm_reset;

    u8 active_interrupt;

    Slot slots[NUM_SLOTS];

    Timer timers[NUM_TIMERS];
} ctx;

static void sample_event(const int);
static void slot_step_all();

enum {
    TIMER_A_INTERRUPT =  6,
    TIMER_B_INTERRUPT =  7,
    SAMPLE_INTERRUPT  = 10,
};

static void check_pending_arm_interrupts() {
    if ((INTREQ == 0) && (SCIEB & SCIPD) != 0) {
        // Pick one of the pending interrupt requests
        // TODO: priority?
        int level = std::countr_zero(SCIPD);

        if (level > TIMER_B_INTERRUPT) {
            // INTREQ doesn't support individual priorities for levels 7-10
            level = TIMER_B_INTERRUPT;
        }

        INTREQ = ctx.arm_interrupt_levels[level];

        arm::assert_fast_interrupt();
    } else {
        arm::clear_fast_interrupt();
    }
}

static void check_pending_sh4_interrupts() {
    constexpr int AICA_INTERRUPT = 1;

    if ((MCIEB & MCIPD) != 0) {
        holly::intc::assert_external_interrupt(AICA_INTERRUPT);
    } else {
        holly::intc::clear_external_interrupt(AICA_INTERRUPT);
    }
}

static void assert_arm_interrupt(const int interrupt_number) {
    if ((SCIPD & (1 << interrupt_number)) == 0) {
        std::printf("Asserting AICA (ARM) interrupt %d\n", interrupt_number);

        SCIPD |= 1 << interrupt_number;

        check_pending_arm_interrupts();
    }
}

static void assert_sh4_interrupt(const int interrupt_number) {
    if ((MCIPD & (1 << interrupt_number)) == 0) {
        std::printf("Asserting AICA (SH-4) interrupt %d\n", interrupt_number);

        MCIPD |= 1 << interrupt_number;

        check_pending_sh4_interrupts();
    }
}

static void assert_interrupt(const int interrupt_number) {
    assert_arm_interrupt(interrupt_number);
    assert_sh4_interrupt(interrupt_number);
}

static void schedule_sample_event() {
    scheduler::schedule_event(
        "AICA_SAMPLE",
        sample_event,
        0,
        scheduler::to_scheduler_cycles<scheduler::SAMPLE_CLOCKRATE>(1)
    );
}

static void sample_event(const int) {
    // Update timers
    for (int timer = 0; timer < NUM_TIMERS; timer++) {
        TIMER(timer).subcounter++;

        if (TIMER(timer).subcounter == TCTL(timer)) {
            TIM(timer)++;

            if (TIM(timer) == 0) {
                std::printf("AICA Timer %d overflow\n", timer);
                
                assert_interrupt(TIMER_A_INTERRUPT + timer);
            }
        }
    }

    slot_step_all();

    assert_interrupt(SAMPLE_INTERRUPT);
    schedule_sample_event();
}

void initialize() {
    arm::initialize();
    bus::initialize();

    schedule_sample_event();
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));

    // ARM held in reset on power-up
    ARMRST.reset_arm = 1;

    arm::assert_reset(true);

    bus::reset();

    MIDIIN.miemp = 1;
    MIDIIN.moemp = 1;

    for (Slot& slot : ctx.slots) {
        slot.attenuation = MAX_ATTENUATION;
        slot.adsr_state = ADSR_STATE_RELEASE;
    }
}

void shutdown() {
    arm::shutdown();
    bus::shutdown();
}

static void slot_set_attenuation(const int slot, const int attenuation) {
    SLOT(slot).attenuation = attenuation;

    if (SLOT(slot).attenuation < 0) {
        SLOT(slot).attenuation = 0;
    } else if (SLOT(slot).attenuation > MAX_ATTENUATION) {
        SLOT(slot).attenuation = MAX_ATTENUATION;
    }
}

static void slot_set_adsr_state(const int slot, const int state) {
    SLOT(slot).adsr_state = state;
    SLOT(slot).adsr_steps = 1;
    SLOT(slot).sample_count = 0;
}

static void slot_adsr_step(const int slot) {
    SLOT(slot).adsr_steps++;
    SLOT(slot).sample_count = 0;
}

static int slot_get_adsr_step(const int slot) {
    return SLOT(slot).adsr_steps - 1;
}

static bool slot_is_active(const int slot) {
    return (SLOT(slot).adsr_state != ADSR_STATE_RELEASE) || (SLOT(slot).attenuation != MAX_ATTENUATION);
}

static void slot_key_on(const int slot) {
    if (slot_is_active(slot)) {
        // Ignore key-on when slot is still active
        return;
    }

    slot_set_attenuation(slot, INITIAL_ATTENUATION);
    slot_set_adsr_state(slot, ADSR_STATE_ATTACK);

    SLOT(slot).current_address = 0;
    SLOT(slot).looped = false;
}

static void slot_key_off(const int slot) {
    slot_set_adsr_state(slot, ADSR_STATE_RELEASE);
}

static void slot_generate_phase(const int slot) {
    const int oct = ((i8)(PITCH(slot).oct << 4)) >> 4;
    const u32 fns = PITCH(slot).fns;

    // Reset integer part, add new phase
    SLOT(slot).phase -= std::truncf(SLOT(slot).phase);
    SLOT(slot).phase += std::powf(2.0, (oct + std::log2f(1 + (float)fns / 1024.0)));
}

static void slot_generate_sample_pointer(const int slot) {

}

static int slot_get_transition_rate(const int slot, const u8 rate) {
    const int krs = ADSR(slot).krs;

    int transition_rate = 2 * rate;

    if (krs < 15) {
        transition_rate += 2 * krs + (PITCH(slot).fns >> 9);
        transition_rate = (8 ^ PITCH(slot).oct) + (transition_rate - 8);
    }

    if (transition_rate < 0) {
        transition_rate = 0;
    } else if (transition_rate > 0x3C) {
        transition_rate = 0x3C;
    }

    return transition_rate;
}

static bool slot_take_adsr_step(const int slot, const int transition_rate) {
    constexpr int SAMPLES_PER_STEP[4][8] = {
        {8192, 4096, 4096,    0,    0,    0,    0,    0},
        {8192, 4096, 4096, 4096, 4096, 4096, 4096,    0},
        {4096,    0,    0,    0,    0,    0,    0,    0},
        {4096, 4096, 4096, 2048, 2048,    0,    0,    0},
    };

    constexpr int STEP_LENGTH[4] = {3, 7, 1, 5};

    assert(transition_rate > 1);

    const int row_step = (transition_rate - 2) & 3;

    int samples_per_step = 2;

    if (transition_rate < 0x30) {
        samples_per_step = SAMPLES_PER_STEP[row_step][slot_get_adsr_step(slot) % STEP_LENGTH[row_step]] >> ((transition_rate - 2) / 4);
    }

    if (SLOT(slot).sample_count < samples_per_step) {
        SLOT(slot).sample_count++;

        return false;
    }

    slot_adsr_step(slot);

    return true;
}

static int slot_get_attack_delta(const int slot, const int transition_rate) {
    constexpr i32 ATTACK_DELTAS[11][4] = {
        {3, 4, 4, 4},
        {3, 4, 3, 4},
        {3, 3, 3, 4},
        {3, 3, 3, 3},
        {2, 3, 3, 3},
        {2, 3, 2, 3},
        {2, 2, 2, 3},
        {2, 2, 2, 2},
        {1, 2, 2, 2},
        {1, 2, 1, 2},
        {1, 1, 1, 2},
    };

    if (transition_rate <= 0x30) {
        return 4;
    } else if (transition_rate >= 0x3C) {
        return 1;
    }

    return ATTACK_DELTAS[transition_rate - 0x31][slot_get_adsr_step(slot) & 3];
}

static void slot_attack(const int slot) {
    const int transition_rate = slot_get_transition_rate(slot, ADSR(slot).ar);

    if (transition_rate < 2) {
        // Don't step ADSR
        return;
    }

    if (slot_take_adsr_step(slot, transition_rate)) {
        const int attenuation = SLOT(slot).attenuation;

        slot_set_attenuation(slot, attenuation - ((attenuation >> slot_get_attack_delta(slot, transition_rate)) + 1));

        if (SLOT(slot).attenuation <= 0) {
            // Lowest attenuation reached, move to Decay
            slot_set_adsr_state(slot, ADSR_STATE_DECAY);
        }
    }
}

int slot_get_decay_delta(const int slot, const int transition_rate) {
    constexpr i32 DECAY_DELTAS[11][4] = {
        {2, 1, 1, 1},
        {2, 1, 2, 1},
        {2, 2, 2, 1},
        {2, 2, 2, 2},
        {4, 2, 2, 2},
        {4, 2, 4, 2},
        {4, 4, 4, 2},
        {4, 4, 4, 4},
        {8, 4, 4, 4},
        {8, 4, 8, 4},
        {8, 8, 8, 4},
    };

    if (transition_rate <= 0x30) {
        return 1;
    } else if (transition_rate >= 0x3C) {
        return 8;
    }

    return DECAY_DELTAS[transition_rate - 0x31][slot_get_adsr_step(slot) & 3];
}

static void slot_decay(const int slot, const u8 rate) {
    const int transition_rate = slot_get_transition_rate(slot, rate);

    if (transition_rate < 2) {
        // Don't step ADSR
        return;
    }

    if (slot_take_adsr_step(slot, transition_rate)) {
        slot_set_attenuation(slot, SLOT(slot).attenuation + slot_get_decay_delta(slot, transition_rate));
    }
}

static void slot_step_adsr(const int slot) {
    switch (SLOT(slot).adsr_state) {
        case ADSR_STATE_ATTACK:
            slot_attack(slot);
            break;
        case ADSR_STATE_DECAY:
            slot_decay(slot, ADSR(slot).d1r);

            if ((SLOT(slot).attenuation >> 5) == ADSR(slot).dl) {
                slot_set_adsr_state(slot, ADSR_STATE_SUSTAIN);
            }
            break;
        case ADSR_STATE_SUSTAIN:
            slot_decay(slot, ADSR(slot).d2r);
            
            // Only transition to Release upon key-off
            break;
        case ADSR_STATE_RELEASE:
            slot_decay(slot, ADSR(slot).rr);
            break;
    }
}

static void slot_step_all() {
    for (int slot = 0; slot < NUM_SLOTS; slot++) {
        if (slot_is_active(slot)) {
            // slot_generate_phase(slot);
            slot_step_adsr(slot);

            SLOT(slot).current_address++;

            if (SLOT(slot).current_address >= LEA(slot)) {
                SLOT(slot).current_address = LSA(slot);
                SLOT(slot).looped = true;
            }
        }
    }
}

static void kyonex() {
    std::puts("AICA KYONEX event");

    for (int slot = 0; slot < NUM_SLOTS; slot++) {
        if (SLOTCTL(slot).kyonb) {
            std::printf("Slot %d key-on\n", slot);

            slot_key_on(slot);
        } else {
            std::printf("Slot %d key-off\n", slot);

            slot_key_off(slot);
        }
    }
}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped AICA read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u8 read(const u32 addr) {
    u32 g2_addr = addr;

    if (g2_addr >= ARM_BASE) {
        g2_addr -= ARM_OFFSET;
    }

    if ((g2_addr & ~SLOT_OFFSET) == IO_SLOTCTL_L) {
        // Slot registers
        const int slot = (addr & 0x1FFF) / 0x80;

        switch (g2_addr & ~0x1F80) {
            case IO_SLOTCTL_H:
                if constexpr (!SILENT_AICA) std::printf("Slot %d SLOTCTL_H read8\n", slot);

                return SLOTCTL(slot).raw >> 8;
            case IO_LFOCTL_H:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LFOCTL_H read8\n", slot);

                return LFOCTL(slot).raw >> 8;
            case IO_INCTL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d INCTL read8\n", slot);

                return INCTL(slot).raw;
            default:
                std::printf("AICA Unimplemented slot %d read8 @ %08X\n", slot, addr);
                exit(1);
        }
    }

    if ((g2_addr >= IO_OUTCTL_0) && (g2_addr <= IO_OUTCTL_17)) {
        const int slot = (g2_addr - IO_OUTCTL_0) >> 2;

        switch (slot) {
            case 16:
                if constexpr (!SILENT_AICA) std::puts("OUTCTL_CDDA_L read8");
                break;
            case 17:
                if constexpr (!SILENT_AICA) std::puts("OUTCTL_CDDA_R read8");
                break;
            default:
                if constexpr (!SILENT_AICA) std::printf("OUTCTL%d read8\n", slot);
                break;
        }

        return 0;
    }

    switch (g2_addr) {
        case IO_INTREQ:
            if constexpr (!SILENT_AICA) std::puts("INTREQ read8");

            return INTREQ;
        default:
            std::printf("Unhandled AICA read8 @ %08X\n", addr);
            exit(1);
    }
}

template<>
u32 read(const u32 addr) {
    u32 g2_addr = addr;

    if (g2_addr >= ARM_BASE) {
        g2_addr -= ARM_OFFSET;
    }

    if ((g2_addr & ~SLOT_OFFSET) == IO_SLOTCTL_L) {
        // Slot registers
        const int slot = (addr & 0x1FFF) / 0x80;

        switch (g2_addr & ~0x1F80) {
            case IO_SLOTCTL_L:
                if constexpr (!SILENT_AICA) std::printf("Slot %d SLOTCTL read32\n", slot);

                return SLOTCTL(slot).raw;
            case IO_LSA:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LSA read32\n", slot);

                return LSA(slot);
            case IO_LEA:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LEA read32\n", slot);

                return LEA(slot);
            case IO_ADSR_L:
                if constexpr (!SILENT_AICA) std::printf("Slot %d ADSR_L read32\n", slot);

                return (u16)ADSR(slot).raw;
            case IO_ADSR_H:
                if constexpr (!SILENT_AICA) std::printf("Slot %d ADSR_H read32\n", slot);

                return (u16)(ADSR(slot).raw >> 16);
            case IO_PITCH:
                if constexpr (!SILENT_AICA) std::printf("Slot %d PITCH read32\n", slot);

                return PITCH(slot).raw;
            case IO_LFOCTL_L:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LFOCTL read32\n", slot);

                return LFOCTL(slot).raw;
            case IO_FCTL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FCTL/TL read32\n", slot);

                return (TL(slot) << 8) | FCTL(slot).raw;
            default:
                std::printf("AICA Unimplemented slot %d read32 @ %08X\n", slot, addr);
                exit(1);
        }
    }

    if ((g2_addr >= IO_OUTCTL_0) && (g2_addr <= IO_OUTCTL_17)) {
        const int slot = (g2_addr - IO_OUTCTL_0) >> 2;

        switch (slot) {
            case 16:
                if constexpr (!SILENT_AICA) std::puts("OUTCTL_CDDA_L read32");
                break;
            case 17:
                if constexpr (!SILENT_AICA) std::puts("OUTCTL_CDDA_R read32");
                break;
            default:
                if constexpr (!SILENT_AICA) std::printf("OUTCTL%d read32\n", slot);
                break;
        }

        return 0;
    }

    switch (g2_addr) {
        case IO_MIDIIN:
            if constexpr (!SILENT_AICA) std::puts("MIDIIN read32");

            return MIDIIN.raw << 8;
        case IO_EGSTAT_M:
            // if constexpr (!SILENT_AICA) std::puts("EGSTAT_M read32");

            // Update monitor
            EGSTAT.eg = (SLOT(EGSTAT.mslc).attenuation >= MAX_ATTENUATION) ? 0x1FFF : SLOT(EGSTAT.mslc).attenuation;
            EGSTAT.sgc = SLOT(EGSTAT.mslc).adsr_state;
            EGSTAT.lp = SLOT(EGSTAT.mslc).looped;

            return EGSTAT.raw >> 8;
        case IO_CA:
            // if constexpr (!SILENT_AICA) std::puts("CA read32");
            
            return SLOT(EGSTAT.mslc).current_address;
        case IO_SCIPD_L:
            if constexpr (!SILENT_AICA) std::puts("SCIPD read32");

            return SCIPD;
        case IO_ARMRST:
            std::puts("ARMRST read32");

            return ARMRST.raw;
        case IO_INTREQ:
            if constexpr (!SILENT_AICA) std::puts("INTREQ read32");

            return INTREQ;
        default:
            std::printf("Unhandled AICA read32 @ %08X\n", addr);
            exit(1);
    }
}

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

    if ((g2_addr & ~SLOT_OFFSET) == IO_SLOTCTL_L) {
        // Slot registers
        const int slot = (addr & 0x1FFF) / 0x80;

        assert(slot < NUM_SLOTS);

        switch (g2_addr & ~0x1F80) {
            case IO_SLOTCTL_H:
                if constexpr (!SILENT_AICA) std::printf("Slot %d SLOTCTL_H write8 = %02X\n", slot, data);

                SLOTCTL(slot).raw &= 0xFF;
                SLOTCTL(slot).raw |= data << 8;

                if (SLOTCTL(slot).kyonex) {
                    kyonex();

                    SLOTCTL(slot).kyonex = 0;
                }
                break;
            case IO_LFOCTL_H:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LFOCTL_H write8 = %02X\n", slot, data);

                LFOCTL(slot).raw &= 0xFF;
                LFOCTL(slot).raw |= data << 8;
                break;
            case IO_INCTL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d INCTL write8 = %02X\n", slot, data);

                INCTL(slot).raw = data;
                break;
            case IO_DIPAN:
                if constexpr (!SILENT_AICA) std::printf("Slot %d DIPAN write8 = %02X\n", slot, data);

                DICTL(slot).pan = data;
                break;
            case IO_DISDL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d DISDL write8 = %02X\n", slot, data);

                DICTL(slot).sdl = data;
                break;
            case IO_FCTL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FCTL write8 = %02X\n", slot, data);

                FCTL(slot).raw = data;
                break;
            case IO_TL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d TL write8 = %02X\n", slot, data);

                TL(slot) = data;
                break;
            default:
                std::printf("AICA Unimplemented slot %d write8 @ %08X = %02X\n", slot, addr, data);
            
                if (addr < ARM_BASE) {
                    break;
                }

                exit(1);
        }

        return;
    }

    if ((g2_addr >= IO_OUTCTL_0) && (g2_addr <= (IO_OUTCTL_17 + sizeof(u8)))) {
        const int slot = (g2_addr - IO_OUTCTL_0) >> 2;

        switch (slot) {
            case 16:
                if constexpr (!SILENT_AICA) std::printf("OUTCTL_CDDA_L write8 = %02X\n", data);
                break;
            case 17:
                if constexpr (!SILENT_AICA) std::printf("OUTCTL_CDDA_R write8 = %02X\n", data);
                break;
            default:
                if constexpr (!SILENT_AICA) std::printf("OUTCTL%d write8 = %02X\n", slot, data);
                break;
        }

        return;
    }

    switch (g2_addr) {
        case IO_MVOL:
            if constexpr (!SILENT_AICA) std::printf("MVOL write8 = %02X\n", data);
            break;
        case IO_MEMCTL:
            if constexpr (!SILENT_AICA) std::printf("MEMCTL write8 = %02X\n", data);
            break;
        case IO_MIDIOUT:
            if constexpr (!SILENT_AICA) std::printf("MIDIOUT write8 = %02X\n", data);
            break;
        case IO_EGSTAT_L:
            // if constexpr (!SILENT_AICA) std::printf("EGSTAT_L write8 = %02X\n", data);

            EGSTAT.raw &= ~0xFF;
            EGSTAT.raw |= data;
            break;
        case IO_SCILV0:
            if constexpr (!SILENT_AICA) std::printf("SCILV0 write8 = %02X\n", data);

            for (int level = 0; level < 8; level++) {
                ctx.arm_interrupt_levels[level] &= ~1;
                ctx.arm_interrupt_levels[level] |= (data >> level) & 1;
            }
            break;
        case IO_SCILV1:
            if constexpr (!SILENT_AICA) std::printf("SCILV1 write8 = %02X\n", data);

            for (int level = 0; level < 8; level++) {
                ctx.arm_interrupt_levels[level] &= ~2;
                ctx.arm_interrupt_levels[level] |= ((data >> level) & 1) << 1;
            }
            break;
        case IO_SCILV2:
            if constexpr (!SILENT_AICA) std::printf("SCILV2 write8 = %02X\n", data);

            for (int level = 0; level < 8; level++) {
                ctx.arm_interrupt_levels[level] &= ~4;
                ctx.arm_interrupt_levels[level] |= ((data >> level) & 1) << 2;
            }
            break;
        case IO_INTCLR:
            if constexpr (!SILENT_AICA) std::printf("INTCLR write8 = %02X\n", data);

            INTREQ = 0;

            // check_pending_arm_interrupts();
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
                if constexpr (!SILENT_AICA) std::printf("Slot %d SLOTCTL write32 = %08X\n", slot, data);

                SLOTCTL(slot).raw = data;

                if (SLOTCTL(slot).kyonex) {
                    kyonex();

                    SLOTCTL(slot).kyonex = 0;
                }

                SA(slot) &= 0xFFFF;
                SA(slot) |= (data & 0x7F) << 16;
                break;
            case IO_SA:
                if constexpr (!SILENT_AICA) std::printf("Slot %d SA write32 = %08X\n", slot, data);

                SA(slot) &= ~0xFFFF;
                SA(slot) |= data & 0xFFFF;
                break;
            case IO_LSA:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LSA write32 = %08X\n", slot, data);

                LSA(slot) = data;
                break;
            case IO_LEA:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LEA write32 = %08X\n", slot, data);

                LEA(slot) = data;
                break;
            case IO_ADSR_L:
                if constexpr (!SILENT_AICA) std::printf("Slot %d ADSR_L write32 = %08X\n", slot, data);

                ADSR(slot).raw &= ~0xFFFF;
                ADSR(slot).raw |= data & 0xFFFF;
                break;
            case IO_ADSR_H:
                if constexpr (!SILENT_AICA) std::printf("Slot %d ADSR_H write32 = %08X\n", slot, data);

                ADSR(slot).raw &= 0xFFFF;
                ADSR(slot).raw |= data << 16;
                break;
            case IO_PITCH:
                if constexpr (!SILENT_AICA) std::printf("Slot %d PITCH write32 = %08X\n", slot, data);

                PITCH(slot).raw = data;
                break;
            case IO_LFOCTL_L:
                if constexpr (!SILENT_AICA) std::printf("Slot %d LFOCTL write32 = %08X\n", slot, data);

                LFOCTL(slot).raw = data;
                break;
            case IO_INCTL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d INCTL write32 = %08X\n", slot, data);

                INCTL(slot).raw = data;
                break;
            case IO_DIPAN:
                if constexpr (!SILENT_AICA) std::printf("Slot %d DIPAN/DISDL write32 = %08X\n", slot, data);

                DICTL(slot).pan = data;
                DICTL(slot).sdl = data >> 8;
                break;
            case IO_FCTL:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FCTL/TL write32 = %08X\n", slot, data);

                FCTL(slot).raw = data;
                TL(slot) = data >> 8;
                break;
            case IO_FLV0:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FLV0 write32 = %08X\n", slot, data);

                FLV(slot)[0] = data;
                break;
            case IO_FLV1:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FLV1 write32 = %08X\n", slot, data);

                FLV(slot)[1] = data;
                break;
            case IO_FLV2:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FLV2 write32 = %08X\n", slot, data);

                FLV(slot)[2] = data;
                break;
            case IO_FLV3:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FLV3 write32 = %08X\n", slot, data);

                FLV(slot)[3] = data;
                break;
            case IO_FLV4:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FLV4 write32 = %08X\n", slot, data);

                FLV(slot)[4] = data;
                break;
            case IO_FADSR_L:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FADSR_L write32 = %08X\n", slot, data);

                FADSR(slot).raw &= ~0xFFFF;
                FADSR(slot).raw |= data & 0xFFFF;
                break;
            case IO_FADSR_H:
                if constexpr (!SILENT_AICA) std::printf("Slot %d FADSR_H write32 = %08X\n", slot, data);

                FADSR(slot).raw &= 0xFFFF;
                FADSR(slot).raw |= data << 16;
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
                if constexpr (!SILENT_AICA) std::printf("OUTCTL_CDDA_L write32 = %08X\n", data);
                break;
            case 17:
                if constexpr (!SILENT_AICA) std::printf("OUTCTL_CDDA_R write32 = %08X\n", data);
                break;
            default:
                if constexpr (!SILENT_AICA) std::printf("OUTCTL%d write32 = %08X\n", slot, data);
                break;
        }

        return;
    }

    if (g2_addr >= IO_DSPCOEF) {
        if constexpr (!SILENT_AICA) std::printf("AICA Unimplemented DSP write32 @ %08X = %08X\n", addr, data);
        return;
    }

    switch (g2_addr) {
        case IO_MVOL:
            if constexpr (!SILENT_AICA) std::printf("MVOL/MEMCTL write32 = %08X\n", data);
            break;
        case IO_RBCTL:
            if constexpr (!SILENT_AICA) std::printf("RBCTL write32 = %08X\n", data);

            RBCTL.raw = data;
            break;
        case IO_DMEA_H:
            if constexpr (!SILENT_AICA) std::printf("DMEA_H write32 = %08X\n", data);

            DMEA &= 0xFFFF;
            DMEA |= ((data & 0xFFFF) >> 9) << 16;
            break;
        case IO_DMEA_L:
            if constexpr (!SILENT_AICA) std::printf("DMEA_L write32 = %08X\n", data);

            DMEA &= ~0xFFFF;
            DMEA |= data & 0xFFFF;
            break;
        case IO_TIMA:
            if constexpr (!SILENT_AICA) std::printf("TIMA/TACTL write32 = %08X\n", data);

            TIM(0) = data;
            TCTL(0) = 1 << (u8)(data >> 8);

            // Reset subcounter
            TIMER(0).subcounter = 0;
            break;
        case IO_TIMB:
            if constexpr (!SILENT_AICA) std::printf("TIMB/TBCTL write32 = %08X\n", data);

            TIM(1) = data;
            TCTL(1) = 1 << (u8)(data >> 8);

            // Reset subcounter
            TIMER(1).subcounter = 0;
            break;
        case IO_TIMC:
            if constexpr (!SILENT_AICA) std::printf("TIMC/TCCTL write32 = %08X\n", data);

            TIM(2) = data;
            TCTL(2) = 1 << (u8)(data >> 8);

            // Reset subcounter
            TIMER(2).subcounter = 0;
            break;
        case IO_SCIEB_L:
            if constexpr (!SILENT_AICA) std::printf("SCIEB write32 = %08X\n", data);
            
            SCIEB = data;

            check_pending_arm_interrupts();
            break;
        case IO_SCIPD_L:
            if constexpr (!SILENT_AICA) std::printf("SCIPD write32 = %08X\n", data);
            
            SCIPD |= (data & 0x20);

            check_pending_arm_interrupts();
            break;
        case IO_SCIRE_L:
            if constexpr (!SILENT_AICA) std::printf("SCIRE write32 = %08X\n", data);
            
            SCIPD &= ~data;

            check_pending_arm_interrupts();
            break;
        case IO_SCILV0:
            if constexpr (!SILENT_AICA) std::printf("SCILV0 write32 = %08X\n", data);

            for (int level = 0; level < 8; level++) {
                ctx.arm_interrupt_levels[level] &= ~1;
                ctx.arm_interrupt_levels[level] |= (data >> level) & 1;
            }
            break;
        case IO_SCILV1:
            if constexpr (!SILENT_AICA) std::printf("SCILV1 write32 = %08X\n", data);

            for (int level = 0; level < 8; level++) {
                ctx.arm_interrupt_levels[level] &= ~2;
                ctx.arm_interrupt_levels[level] |= ((data >> level) & 1) << 1;
            }
            break;
        case IO_SCILV2:
            if constexpr (!SILENT_AICA) std::printf("SCILV2 write32 = %08X\n", data);

            for (int level = 0; level < 8; level++) {
                ctx.arm_interrupt_levels[level] &= ~4;
                ctx.arm_interrupt_levels[level] |= ((data >> level) & 1) << 2;
            }
            break;
        case IO_MCIEB_L:
            if constexpr (!SILENT_AICA) std::printf("MCIEB write32 = %08X\n", data);
            
            MCIEB = data;

            check_pending_sh4_interrupts();
            break;
        case IO_MCIPD_L:
            if constexpr (!SILENT_AICA) std::printf("MCIPD write32 = %08X\n", data);
            
            MCIPD |= (data & 0x20);

            check_pending_sh4_interrupts();
            break;
        case IO_MCIRE_L:
            if constexpr (!SILENT_AICA) std::printf("SCIRE write32 = %08X\n", data);
            
            MCIPD &= ~data;

            check_pending_sh4_interrupts();
            break;
        case IO_ARMRST:
            std::printf("ARMRST write32 = %08X\n", data);

            ARMRST.raw = data;
            
            arm::assert_reset(ARMRST.reset_arm);
            break;
        case IO_INTCLR:
            if constexpr (!SILENT_AICA) std::printf("INTCLR write32 = %08X\n", data);

            INTREQ = 0;
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
