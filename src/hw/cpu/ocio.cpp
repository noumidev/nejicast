/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/ocio.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio {

namespace Address {
    enum : u32 {
        Mmucr  = 0x1F000010,
        Ccr    = 0x1F00001C,
        Expevt = 0x1F000024,
        Bcr1   = 0x1F800000,
        Bcr2   = 0x1F800004,
        Wcr1   = 0x1F800008,
        Wcr2   = 0x1F80000C,
        Mcr    = 0x1F800014,
        Rtcsr  = 0x1F80001C,
        Rtcor  = 0x1F800024,
        Rfcr   = 0x1F800028,
        Sdmr3  = 0x1F940000,
    };
}

#define MMUCR  ctx.mmu_control
#define CCR    ctx.cache_control
#define EXPEVT ctx.exception_event
#define BCR1   ctx.bus_control_1
#define BCR2   ctx.bus_control_2
#define WCR1   ctx.wait_control_1
#define WCR2   ctx.wait_control_2
#define MCR    ctx.memory_control
#define RTCSR  ctx.refresh_timer_control
#define RTCNT  ctx.refresh_timer
#define RTCOR  ctx.refresh_time_constant
#define RFCR   ctx.refresh_count
#define SDMR3  ctx.sdram_mode_3

struct {
    union {
        u32 raw;

        struct {
            u32 enable_translation    : 1;
            u32                       : 1;
            u32 invalidate_tlb        : 1;
            u32                       : 5;
            u32 single_virtual_mode   : 1;
            u32 store_queue_mode      : 1;
            u32 utlb_replace_counter  : 6;
            u32                       : 2;
            u32 utlb_replace_boundary : 6;
            u32                       : 2;
            u32 itlb_lru              : 6;
        };
    } mmu_control;

    union {
        u32 raw;

        struct {
            u32 enable_operand_cache           :  1;
            u32 enable_writethrough            :  1;
            u32 enable_copyback                :  1;
            u32 invalidate_operand_cache       :  1;
            u32                                :  1;
            u32 enable_operand_cache_ram       :  1;
            u32                                :  1;
            u32 enable_operand_cache_index     :  1;
            u32 enable_instruction_cache       :  1;
            u32                                :  2;
            u32 invalidate_instruction_cache   :  1;
            u32                                :  3;
            u32 enable_instruction_cache_index :  1;
            u32                                : 16;
        };
    } cache_control;

    u32 exception_event;

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

    u16 refresh_timer, refresh_time_constant, refresh_count;

    u16 sdram_mode_3;
} ctx;

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

void initialize() {
    BCR2.raw = 0x3FFC;
    WCR1.raw = 0x77777777;
    WCR2.raw = 0xFFFEEFFF;
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped SH-4 P4 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u16 read(const u32 addr) {
    switch (addr) {
        case Address::Rfcr:
            std::puts("RFCR read16");

            // HACK
            refresh_dram();

            return RFCR;
        default:
            std::printf("Unmapped SH-4 P4 read16 @ %08X\n", addr);
            exit(1);
    }
}

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case Address::Expevt:
            std::puts("EXPEVT read32");

            return EXPEVT;
        default:
            std::printf("Unmapped SH-4 P4 read32 @ %08X\n", addr);
            exit(1);
    }
}

template u8 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped SH-4 P4 write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u8 data) {
    if ((addr & ~0xFFFF) == Address::Sdmr3) {
        // TODO: handle 32-bis bus SDMR3 writes?
        SDMR3 = (data & 0x1FF8) >> 3;

        std::printf("SDMR3 write = %03X\n", SDMR3);
        return;
    }

    switch (addr) {
        default:
            std::printf("Unmapped SH-4 P4 write8 @ %08X = %02X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u16 data) {
    switch (addr) {
        case Address::Bcr2:
            std::printf("BCR2 write16 = %04X\n", data);

            BCR2.raw = data;
            break;
        case Address::Rtcsr:
            std::printf("RTCSR write16 = %04X\n", data);

            if ((data & 0xFF00) == 0xA500) {
                RTCSR.raw = data & 0xFF;
            }
            break;
        case Address::Rtcor:
            std::printf("RTCOR write16 = %04X\n", data);

            if ((data & 0xFF00) == 0xA500) {
                RTCOR = data & 0xFF;
            }
            break;
        case Address::Rfcr:
            std::printf("RFCR write16 = %04X\n", data);

            if ((data & 0xFC00) == 0xA400) {
                RFCR = data & 0x3FF;
            }
            break;
        default:
            std::printf("Unmapped SH-4 P4 write16 @ %08X = %04X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case Address::Mmucr:
            std::printf("MMUCR write32 = %08X\n", data);

            MMUCR.raw = data;

            assert(MMUCR.enable_translation == 0);
            break;
        case Address::Ccr:
            std::printf("CCR write32 = %08X\n", data);

            CCR.raw = data;

            if (CCR.invalidate_operand_cache) {
                std::puts("SH-4 invalidate operand cache");

                CCR.invalidate_operand_cache = 0;
            }

            if (CCR.invalidate_instruction_cache) {
                std::puts("SH-4 invalidate instruction cache");

                CCR.invalidate_instruction_cache = 0;
            }
            break;
        case Address::Bcr1:
            std::printf("BCR1 write32 = %08X\n", data);

            BCR1.raw = data;
            break;
        case Address::Wcr1:
            std::printf("WCR1 write32 = %08X\n", data);

            WCR1.raw = data;
            break;
        case Address::Wcr2:
            std::printf("WCR2 write32 = %08X\n", data);

            WCR2.raw = data;
            break;
        case Address::Mcr:
            std::printf("MCR write32 = %08X\n", data);

            MCR.raw = data;
            break;
        default:
            std::printf("Unmapped SH-4 P4 write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u64);

void set_exception_event(const u32 event) {
    EXPEVT = event;
}

}
