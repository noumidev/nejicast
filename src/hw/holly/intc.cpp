/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/holly/intc.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::holly::intc {

namespace Address {
    enum : u32 {
        Iml2Nrm = 0x005F6910,
        Iml2Ext = 0x005F6914,
        Iml2Err = 0x005F6918,
        Iml4Nrm = 0x005F6920,
        Iml4Ext = 0x005F6924,
        Iml4Err = 0x005F6928,
        Iml6Nrm = 0x005F6930,
        Iml6Ext = 0x005F6934,
        Iml6Err = 0x005F6938,
        Pdtnrm  = 0x005F6940,
        Pdtext  = 0x005F6944,
        G2Dtnrm = 0x005F6950,
        G2Dtext = 0x005F6954,
    };
}

enum {
    LEVEL_2,
    LEVEL_4,
    LEVEL_6,
    NUM_LEVELS,
};

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

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped INTC write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case Address::Iml2Nrm:
            std::printf("SB_IML2NRM write32 = %08X\n", data);

            SB_IML2NRM = data;
            break;
        case Address::Iml2Ext:
            std::printf("SB_IML2EXT write32 = %08X\n", data);

            SB_IML2EXT = data;
            break;
        case Address::Iml2Err:
            std::printf("SB_IML2ERR write32 = %08X\n", data);

            SB_IML2ERR = data;
            break;
        case Address::Iml4Nrm:
            std::printf("SB_IML4NRM write32 = %08X\n", data);

            SB_IML4NRM = data;
            break;
        case Address::Iml4Ext:
            std::printf("SB_IML4EXT write32 = %08X\n", data);

            SB_IML4EXT = data;
            break;
        case Address::Iml4Err:
            std::printf("SB_IML4ERR write32 = %08X\n", data);

            SB_IML4ERR = data;
            break;
        case Address::Iml6Nrm:
            std::printf("SB_IML6NRM write32 = %08X\n", data);

            SB_IML6NRM = data;
            break;
        case Address::Iml6Ext:
            std::printf("SB_IML6EXT write32 = %08X\n", data);

            SB_IML6EXT = data;
            break;
        case Address::Iml6Err:
            std::printf("SB_IML6ERR write32 = %08X\n", data);

            SB_IML6ERR = data;
            break;
        case Address::Pdtnrm:
            std::printf("SB_PDTNRM write32 = %08X\n", data);

            SB_PDTNRM = data;
            break;
        case Address::Pdtext:
            std::printf("SB_PDTEXT write32 = %08X\n", data);

            SB_PDTEXT = data;
            break;
        case Address::G2Dtnrm:
            std::printf("SB_G2DTNRM write32 = %08X\n", data);

            SB_G2DTNRM = data;
            break;
        case Address::G2Dtext:
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

}
