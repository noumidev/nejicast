/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g1/g1.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <scheduler.hpp>
#include <common/file.hpp>
#include <hw/holly/bus.hpp>
#include <hw/holly/intc.hpp>
#include <hw/g1/flash.hpp>
#include <hw/g1/gdrom.hpp>

namespace hw::g1 {

constexpr usize BOOT_ROM_SIZE = 0x200000;

enum : u32 {
    IO_GDSTAR  = 0x005F7404,
    IO_GDLEN   = 0x005F7408,
    IO_GDDIR   = 0x005F740C,
    IO_GDEN    = 0x005F7414,
    IO_GDST    = 0x005F7418,
    IO_G1RRC   = 0x005F7480,
    IO_G1RWC   = 0x005F7484,
    IO_G1FRC   = 0x005F7488,
    IO_G1FWC   = 0x005F748C,
    IO_G1CRC   = 0x005F7490,
    IO_G1CWC   = 0x005F7494,
    IO_G1GDRC  = 0x005F74A0,
    IO_G1GDWC  = 0x005F74A4,
    IO_G1CRDYC = 0x005F74B4,
    IO_GDAPRO  = 0x005F74B8,
    IO_GDRPRO  = 0x005F74E4,
    IO_GDRPROS = 0x005F74EC,
    IO_GDSTARD = 0x005F74F4,
    IO_GDLEND  = 0x005F74F8,
};

#define SB_GDSTAR  ctx.gdrom_dma.start_address
#define SB_GDLEN   ctx.gdrom_dma.length
#define SB_GDDIR   ctx.gdrom_dma.from_gdrom
#define SB_GDEN    ctx.gdrom_dma.enable
#define SB_GDST    ctx.gdrom_dma.is_running
#define SB_G1RRC   ctx.boot_rom_read_timing
#define SB_G1RWC   ctx.boot_rom_write_timing
#define SB_G1FRC   ctx.flash_rom_read_timing
#define SB_G1FWC   ctx.flash_rom_write_timing
#define SB_G1CRC   ctx.pio_read_timing
#define SB_G1CWC   ctx.pio_write_timing
#define SB_G1GDRC  ctx.dma_read_timing
#define SB_G1GDWC  ctx.dma_write_timing
#define SB_G1CRDYC ctx.enable_io_ready
#define SB_GDAPRO  ctx.address_protection
#define SB_GDRPRO  ctx.boot_rom_protection
#define SB_GDSTARD ctx.gdrom_dma.debug_address
#define SB_GDLEND  ctx.gdrom_dma.debug_length

struct {
    std::vector<u8> boot_rom;

    struct {
        u32 start_address;
        u32 debug_address;
        u32 length;
        u32 debug_length;
        bool from_gdrom;
        bool enable;
        bool is_running;
        bool dma_ready;
    } gdrom_dma;

    union {
        u32 raw;

        struct {
            u32 address_hold   :  3;
            u32                :  1;
            u32 address_setup  :  4;
            u32 cs_pulse_width :  4;
            u32 pulse_delay    :  1;
            u32                : 19;
        };
    } boot_rom_read_timing, boot_rom_write_timing, flash_rom_read_timing, flash_rom_write_timing;

    union {
        u32 raw;

        struct {
            u32 address_hold   :  3;
            u32                :  1;
            u32 address_setup  :  4;
            u32 pulse_width    :  4;
            u32                : 20;
        };
    } pio_read_timing, pio_write_timing;

    union {
        u32 raw;

        struct {
            u32 pulse_width            :  4;
            u32 pulse_delay            :  4;
            u32 delay_time             :  4;
            u32 acknowledge_delay_time :  4;
            u32                        : 16;
        };
    } dma_read_timing, dma_write_timing;

    bool enable_io_ready;

    union {
        u16 raw;

        struct {
            u16 bottom_address : 7;
            u16                : 1;
            u16 top_address    : 7;
            u16                : 1;
        };
    } address_protection;

    u32 boot_rom_protection;
} ctx;

void initialize(const char* boot_path, const char* flash_path) {
    flash::initialize(flash_path);
    gdrom::initialize();

    ctx.boot_rom = common::load_file(boot_path);

    if (ctx.boot_rom.size() != BOOT_ROM_SIZE) {
        std::printf("Invalid boot ROM size %zu\n", ctx.boot_rom.size());
        exit(1);
    }
}

void reset() {
    gdrom::reset();

    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {
    gdrom::shutdown();
}

static void finish_gdrom_dma(const int) {
    constexpr int GDROM_DMA_INTERRUPT = 14;

    SB_GDST = false;

    hw::holly::intc::assert_normal_interrupt(GDROM_DMA_INTERRUPT);

    SB_GDSTARD = SB_GDSTAR + SB_GDLEN;
    SB_GDLEND = SB_GDLEN;
}

void try_gdrom_dma() {
    execute_gdrom_dma();
}

void execute_gdrom_dma() {
    if (!SB_GDST) {
        return;
    }

    if (!ctx.gdrom_dma.dma_ready) {
        std::puts("GD-ROM DMA not ready");
        return;
    }

    std::printf("GD-ROM DMA @ %08X\n", SB_GDSTAR);

    holly::bus::copy_from_bytes(
        SB_GDSTAR,
        SB_GDLEN,
        SB_GDLEN,
        gdrom::get_dma_bytes(SB_GDLEN)
    );

    scheduler::schedule_event(
        "GDROM_DMA_END",
        finish_gdrom_dma,
        0,
        scheduler::to_scheduler_cycles<scheduler::HOLLY_CLOCKRATE>(32 * SB_GDLEN)
    );

    SB_GDSTARD = SB_GDSTAR;
    SB_GDLEND = 0;
}

void set_gdrom_dma_ready(const bool ready) {
    ctx.gdrom_dma.dma_ready = ready;
}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped G1 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

enum {
    ROM_PROTECTION_STATUS_PASSED = 3,
};

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_GDEN:
            std::puts("SB_GDEN read32");

            return SB_GDEN;
        case IO_GDST:
            // std::puts("SB_GDST read32");

            return SB_GDST;
        case IO_GDRPROS:
            std::puts("SB_GDRPROS read32");

            return ROM_PROTECTION_STATUS_PASSED;
        case IO_GDSTARD:
            std::puts("SB_GDSTARD read32");

            return SB_GDSTARD;
        case IO_GDLEND:
            std::puts("SB_GDLEND read32");

            return SB_GDLEND;
        default:
            // std::printf("Unmapped G1 read32 @ %08X\n", addr);
            return 0;
    }
}

template u8 read(u32);
template u16 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped G1 write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u16 data) {
    switch (addr) {
        case IO_G1RRC:
            std::printf("SB_G1RRC write16 = %04X\n", data);

            SB_G1RRC.raw = data;
            break;
        default:
            std::printf("Unmapped G1 write16 @ %08X = %04X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_GDSTAR:
            std::printf("SB_GDSTAR write32 = %08X\n", data);

            SB_GDSTAR = data;
            break;
        case IO_GDLEN:
            std::printf("SB_GDLEN write32 = %08X\n", data);

            SB_GDLEN = data;
            break;
        case IO_GDDIR:
            std::printf("SB_GDDIR write32 = %08X\n", data);

            SB_GDDIR = (data & 1) != 0;
            break;
        case IO_GDEN:
            std::printf("SB_GDEN write32 = %08X\n", data);

            SB_GDEN = (data & 1) != 0;
            break;
        case IO_GDST:
            std::printf("SB_GDST write32 = %08X\n", data);

            SB_GDST = (data & 1) != 0;

            if (SB_GDST) {
                execute_gdrom_dma();
            }
            break;
        case IO_G1RWC:
            std::printf("SB_G1RWC write32 = %08X\n", data);

            SB_G1RWC.raw = data;
            break;
        case IO_G1FRC:
            std::printf("SB_G1FRC write32 = %08X\n", data);

            SB_G1FRC.raw = data;
            break;
        case IO_G1FWC:
            std::printf("SB_G1FWC write32 = %08X\n", data);

            SB_G1FWC.raw = data;
            break;
        case IO_G1CRC:
            std::printf("SB_G1CRC write32 = %08X\n", data);

            SB_G1CRC.raw = data;
            break;
        case IO_G1CWC:
            std::printf("SB_G1CWC write32 = %08X\n", data);

            SB_G1CWC.raw = data;
            break;
        case IO_G1GDRC:
            std::printf("SB_G1GDRC write32 = %08X\n", data);

            SB_G1GDRC.raw = data;
            break;
        case IO_G1GDWC:
            std::printf("SB_G1GDWC write32 = %08X\n", data);

            SB_G1GDWC.raw = data;
            break;
        case IO_G1CRDYC:
            std::printf("SB_G1CRDYC write32 = %08X\n", data);

            SB_G1CRDYC = (data & 1) != 0;
            break;
        case IO_GDAPRO:
            std::printf("SB_GDAPRO write32 = %08X\n", data);

            if ((data & ~0xFFFF) == 0x88430000) {
                SB_GDAPRO.raw = (u16)data;
            }
            break;
        case IO_GDRPRO:
            std::printf("SB_GDRPRO write32 = %08X\n", data);

            SB_GDRPRO = data;
            break;
        default:
            std::printf("Unmapped G1 write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u64);

// For HOLLY access
u8* get_boot_rom_ptr() {
    return ctx.boot_rom.data();
}

}
