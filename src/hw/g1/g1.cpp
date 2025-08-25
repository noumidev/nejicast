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

#include <common/file.hpp>

namespace hw::g1 {

constexpr usize BOOT_ROM_SIZE = 0x200000;

namespace Address {
    enum : u32 {
        Gdstar  = 0x005F7404,
        Gdlen   = 0x005F7408,
        Gddir   = 0x005F740C,
        Gden    = 0x005F7414,
        Gdst    = 0x005F7418,
        G1Rrc   = 0x005F7480,
        G1Rwc   = 0x005F7484,
        G1Frc   = 0x005F7488,
        G1Fwc   = 0x005F748C,
        G1Crc   = 0x005F7490,
        G1Cwc   = 0x005F7494,
        G1Gdrc  = 0x005F74A0,
        G1Gdwc  = 0x005F74A4,
        G1Crdyc = 0x005F74B4,
        Gdapro  = 0x005F74B8,
    };
}

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

struct {
    std::vector<u8> boot_rom;

    struct {
        u32 start_address;
        u32 length;
        bool from_gdrom;
        bool enable;
        bool is_running;
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
} ctx;

void initialize(const char* boot_path) {
    ctx.boot_rom = common::load_file(boot_path);

    assert(ctx.boot_rom.size() == BOOT_ROM_SIZE);
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped G1 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped G1 write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u16 data) {
    switch (addr) {
        case Address::G1Rrc:
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
        case Address::Gdstar:
            std::printf("SB_GDSTAR write32 = %08X\n", data);

            SB_GDSTAR = data;
            break;
        case Address::Gdlen:
            std::printf("SB_GDLEN write32 = %08X\n", data);

            SB_GDLEN = data;
            break;
        case Address::Gddir:
            std::printf("SB_GDDIR write32 = %08X\n", data);

            SB_GDDIR = (data & 1) != 0;
            break;
        case Address::Gden:
            std::printf("SB_GDEN write32 = %08X\n", data);

            SB_GDEN = (data & 1) != 0;
            break;
        case Address::Gdst:
            std::printf("SB_GDST write32 = %08X\n", data);

            assert((data & 1) == 0);
            break;
        case Address::G1Rwc:
            std::printf("SB_G1RWC write32 = %08X\n", data);

            SB_G1RWC.raw = data;
            break;
        case Address::G1Frc:
            std::printf("SB_G1FRC write32 = %08X\n", data);

            SB_G1FRC.raw = data;
            break;
        case Address::G1Fwc:
            std::printf("SB_G1FWC write32 = %08X\n", data);

            SB_G1FWC.raw = data;
            break;
        case Address::G1Crc:
            std::printf("SB_G1CRC write32 = %08X\n", data);

            SB_G1CRC.raw = data;
            break;
        case Address::G1Cwc:
            std::printf("SB_G1CWC write32 = %08X\n", data);

            SB_G1CWC.raw = data;
            break;
        case Address::G1Gdrc:
            std::printf("SB_G1GDRC write32 = %08X\n", data);

            SB_G1GDRC.raw = data;
            break;
        case Address::G1Gdwc:
            std::printf("SB_G1GDWC write32 = %08X\n", data);

            SB_G1GDWC.raw = data;
            break;
        case Address::G1Crdyc:
            std::printf("SB_G1CRDYC write32 = %08X\n", data);

            SB_G1CRDYC = (data & 1) != 0;
            break;
        case Address::Gdapro:
            std::printf("SB_GDAPRO write32 = %08X\n", data);

            if ((data & ~0xFFFF) == 0x88430000) {
                SB_GDAPRO.raw = (u16)data;
            }
            break;
        case 0x005F74E4:
            std::printf("Unknown G1 write32 @ %08X = %08X\n", addr, data);
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
