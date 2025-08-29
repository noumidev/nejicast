/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/holly/intc.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::holly::intc {

enum : u32 {
    IO_ISTNRM  = 0x005F6900,
    IO_ISTEXT  = 0x005F6904,
    IO_ISTERR  = 0x005F6908,
    IO_IML2NRM = 0x005F6910,
    IO_IML2EXT = 0x005F6914,
    IO_IML2ERR = 0x005F6918,
    IO_IML4NRM = 0x005F6920,
    IO_IML4EXT = 0x005F6924,
    IO_IML4ERR = 0x005F6928,
    IO_IML6NRM = 0x005F6930,
    IO_IML6EXT = 0x005F6934,
    IO_IML6ERR = 0x005F6938,
    IO_PDTNRM  = 0x005F6940,
    IO_PDTEXT  = 0x005F6944,
    IO_G2DTNRM = 0x005F6950,
    IO_G2DTEXT = 0x005F6954,
};

enum {
    LEVEL_2,
    LEVEL_4,
    LEVEL_6,
    NUM_LEVELS,
};

#define SB_ISTNRM  ctx.normal_flags
#define SB_ISTEXT  ctx.external_flags
#define SB_ISTERR  ctx.error_flags
#define SB_IML2NRM ctx.levels[LEVEL_2].normal_mask
#define SB_IML2EXT ctx.levels[LEVEL_2].external_mask
#define SB_IML2ERR ctx.levels[LEVEL_2].error_mask
#define SB_IML4NRM ctx.levels[LEVEL_4].normal_mask
#define SB_IML4EXT ctx.levels[LEVEL_4].external_mask
#define SB_IML4ERR ctx.levels[LEVEL_4].error_mask
#define SB_IML6NRM ctx.levels[LEVEL_6].normal_mask
#define SB_IML6EXT ctx.levels[LEVEL_6].external_mask
#define SB_IML6ERR ctx.levels[LEVEL_6].error_mask
#define SB_PDTNRM  ctx.pvr_dma.normal_mask
#define SB_PDTEXT  ctx.pvr_dma.external_mask
#define SB_G2DTNRM ctx.g2_dma.normal_mask
#define SB_G2DTEXT ctx.g2_dma.external_mask

struct {
    u32 normal_flags;
    u32 external_flags;
    u32 error_flags;

    struct {
        u32 normal_mask;
        u32 external_mask;
        u32 error_mask;
    } levels[NUM_LEVELS];

    struct {
        u32 normal_mask;
        u32 external_mask;
    } pvr_dma;

    struct {
        u32 normal_mask;
        u32 external_mask;
    } g2_dma;
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped INTC read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_ISTNRM:
            // std::puts("SB_ISTNRM read32");

            return SB_ISTNRM;
        case IO_ISTEXT:
            std::puts("SB_ISTEXT read32");

            return SB_ISTEXT;
        case IO_ISTERR:
            std::puts("SB_ISTERR read32");

            return SB_ISTERR;
        case IO_IML2NRM:
            std::puts("SB_IML2NRM read32");

            return SB_IML2NRM;
        case IO_IML2EXT:
            std::puts("SB_IML2EXT read32");

            return SB_IML2EXT;
        case IO_IML2ERR:
            std::puts("SB_IML2ERR read32");

            return SB_IML2ERR;
        case IO_IML4NRM:
            std::puts("SB_IML4NRM read32");

            return SB_IML4NRM;
        case IO_IML4EXT:
            std::puts("SB_IML4EXT read32");

            return SB_IML4EXT;
        case IO_IML4ERR:
            std::puts("SB_IML4ERR read32");

            return SB_IML4ERR;
        case IO_IML6NRM:
            std::puts("SB_IML6NRM read32");

            return SB_IML6NRM;
        case IO_IML6EXT:
            std::puts("SB_IML6EXT read32");

            return SB_IML6EXT;
        case IO_IML6ERR:
            std::puts("SB_IML6ERR read32");

            return SB_IML6ERR;
        default:
            std::printf("Unmapped INTC read32 @ %08X\n", addr);
            exit(1);
    }
}

template u8 read(u32);
template u16 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped INTC write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_ISTNRM:
            std::printf("SB_ISTNRM write32 = %08X\n", data);

            SB_ISTNRM &= ~data;
            break;
        case IO_ISTERR:
            std::printf("SB_ISTERR write32 = %08X\n", data);

            SB_ISTERR &= ~data;
            break;
        case IO_IML2NRM:
            std::printf("SB_IML2NRM write32 = %08X\n", data);

            SB_IML2NRM = data;
            break;
        case IO_IML2EXT:
            std::printf("SB_IML2EXT write32 = %08X\n", data);

            SB_IML2EXT = data;
            break;
        case IO_IML2ERR:
            std::printf("SB_IML2ERR write32 = %08X\n", data);

            SB_IML2ERR = data;
            break;
        case IO_IML4NRM:
            std::printf("SB_IML4NRM write32 = %08X\n", data);

            SB_IML4NRM = data;
            break;
        case IO_IML4EXT:
            std::printf("SB_IML4EXT write32 = %08X\n", data);

            SB_IML4EXT = data;
            break;
        case IO_IML4ERR:
            std::printf("SB_IML4ERR write32 = %08X\n", data);

            SB_IML4ERR = data;
            break;
        case IO_IML6NRM:
            std::printf("SB_IML6NRM write32 = %08X\n", data);

            SB_IML6NRM = data;
            break;
        case IO_IML6EXT:
            std::printf("SB_IML6EXT write32 = %08X\n", data);

            SB_IML6EXT = data;
            break;
        case IO_IML6ERR:
            std::printf("SB_IML6ERR write32 = %08X\n", data);

            SB_IML6ERR = data;
            break;
        case IO_PDTNRM:
            std::printf("SB_PDTNRM write32 = %08X\n", data);

            SB_PDTNRM = data;
            break;
        case IO_PDTEXT:
            std::printf("SB_PDTEXT write32 = %08X\n", data);

            SB_PDTEXT = data;
            break;
        case IO_G2DTNRM:
            std::printf("SB_G2DTNRM write32 = %08X\n", data);

            SB_G2DTNRM = data;
            break;
        case IO_G2DTEXT:
            std::printf("SB_G2DTEXT write32 = %08X\n", data);

            SB_G2DTEXT = data;
            break;
        default:
            std::printf("Unmapped INTC write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

void assert_normal_interrupt(const int interrupt_number) {
    if ((SB_ISTNRM & (1 << interrupt_number)) == 0) {
        std::printf("Asserting normal interrupt %d\n", interrupt_number);

        SB_ISTNRM |= 1 << interrupt_number;

        // TODO: trigger interrupt
    }
}

void assert_external_interrupt(const int interrupt_number) {
    if ((SB_ISTEXT & (1 << interrupt_number)) == 0) {
        std::printf("Asserting external interrupt %d\n", interrupt_number);

        SB_ISTEXT |= 1 << interrupt_number;

        // TODO: trigger interrupt
    }
}

void clear_external_interrupt(const int interrupt_number) {
    if ((SB_ISTEXT & (1 << interrupt_number)) != 0) {
        std::printf("Clearing external interrupt %d\n", interrupt_number);

        SB_ISTEXT &= ~(1 << interrupt_number);
    }
}

}
