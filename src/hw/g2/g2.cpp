/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g2/g2.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::g2 {

enum : u32 {
    IO_ADSTAG  = 0x005F7800,
    IO_ADSTAR  = 0x005F7804,
    IO_ADLEN   = 0x005F7808,
    IO_ADDIR   = 0x005F780C,
    IO_ADTSEL  = 0x005F7810,
    IO_ADEN    = 0x005F7814,
    IO_ADST    = 0x005F7818,
    IO_ADSUSP  = 0x005F781C,
    IO_E1STAG  = 0x005F7820,
    IO_E1STAR  = 0x005F7824,
    IO_E1LEN   = 0x005F7828,
    IO_E1DIR   = 0x005F782C,
    IO_E1TSEL  = 0x005F7830,
    IO_E1EN    = 0x005F7834,
    IO_E1ST    = 0x005F7838,
    IO_E1SUSP  = 0x005F783C,
    IO_E2STAG  = 0x005F7840,
    IO_E2STAR  = 0x005F7844,
    IO_E2LEN   = 0x005F7848,
    IO_E2DIR   = 0x005F784C,
    IO_E2TSEL  = 0x005F7850,
    IO_E2EN    = 0x005F7854,
    IO_E2ST    = 0x005F7858,
    IO_E2SUSP  = 0x005F785C,
    IO_DDSTAG  = 0x005F7860,
    IO_DDSTAR  = 0x005F7864,
    IO_DDLEN   = 0x005F7868,
    IO_DDDIR   = 0x005F786C,
    IO_DDTSEL  = 0x005F7870,
    IO_DDEN    = 0x005F7874,
    IO_DDST    = 0x005F7878,
    IO_DDSUSP  = 0x005F787C,
    IO_G2DSTO  = 0x005F7890,
    IO_G2TRTO  = 0x005F7894,
    IO_G2MDMTO = 0x005F7898,
    IO_G2MDMW  = 0x005F789C,
    IO_G2APRO  = 0x005F78BC,
};

enum {
    AICA_DMA,
    EXT1_DMA,
    EXT2_DMA,
    DEVT_DMA,
    NUM_DMA_CHANNELS,
};

#define SB_ADSTAG  ctx.dma_channels[AICA_DMA].g2_start_address
#define SB_ADSTAR  ctx.dma_channels[AICA_DMA].ram_start_address
#define SB_ADLEN   ctx.dma_channels[AICA_DMA].length
#define SB_ADDIR   ctx.dma_channels[AICA_DMA].from_peripheral
#define SB_ADTSEL  ctx.dma_channels[AICA_DMA].select_trigger
#define SB_ADEN    ctx.dma_channels[AICA_DMA].enable
#define SB_ADST    ctx.dma_channels[AICA_DMA].is_running
#define SB_ADSUSP  ctx.dma_channels[AICA_DMA].suspend
#define SB_E1STAG  ctx.dma_channels[EXT1_DMA].g2_start_address
#define SB_E1STAR  ctx.dma_channels[EXT1_DMA].ram_start_address
#define SB_E1LEN   ctx.dma_channels[EXT1_DMA].length
#define SB_E1DIR   ctx.dma_channels[EXT1_DMA].from_peripheral
#define SB_E1TSEL  ctx.dma_channels[EXT1_DMA].select_trigger
#define SB_E1EN    ctx.dma_channels[EXT1_DMA].enable
#define SB_E1ST    ctx.dma_channels[EXT1_DMA].is_running
#define SB_E1SUSP  ctx.dma_channels[EXT1_DMA].suspend
#define SB_E2STAG  ctx.dma_channels[EXT2_DMA].g2_start_address
#define SB_E2STAR  ctx.dma_channels[EXT2_DMA].ram_start_address
#define SB_E2LEN   ctx.dma_channels[EXT2_DMA].length
#define SB_E2DIR   ctx.dma_channels[EXT2_DMA].from_peripheral
#define SB_E2TSEL  ctx.dma_channels[EXT2_DMA].select_trigger
#define SB_E2EN    ctx.dma_channels[EXT2_DMA].enable
#define SB_E2ST    ctx.dma_channels[EXT2_DMA].is_running
#define SB_E2SUSP  ctx.dma_channels[EXT2_DMA].suspend
#define SB_DDSTAG  ctx.dma_channels[DEVT_DMA].g2_start_address
#define SB_DDSTAR  ctx.dma_channels[DEVT_DMA].ram_start_address
#define SB_DDLEN   ctx.dma_channels[DEVT_DMA].length
#define SB_DDDIR   ctx.dma_channels[DEVT_DMA].from_peripheral
#define SB_DDTSEL  ctx.dma_channels[DEVT_DMA].select_trigger
#define SB_DDEN    ctx.dma_channels[DEVT_DMA].enable
#define SB_DDST    ctx.dma_channels[DEVT_DMA].is_running
#define SB_DDSUSP  ctx.dma_channels[DEVT_DMA].suspend
#define SB_G2DSTO  ctx.ds_timeout
#define SB_G2TRTO  ctx.tr_timeout
#define SB_G2MDMTO ctx.modem_timeout
#define SB_G2MDMW  ctx.modem_wait
#define SB_G2APRO  ctx.address_protection

struct {
    struct {
        u32 g2_start_address;
        u32 ram_start_address;
        u32 length;
        bool from_peripheral;
        u32 select_trigger;
        bool enable;
        bool is_running;

        union {
            u32 raw;

            struct {
                u32 request_suspend :  1;
                u32                 :  3;
                u32 suspend_flag    :  1;
                u32 request_flag    :  1;
                u32                 : 26;
            };
        } suspend;
    } dma_channels[NUM_DMA_CHANNELS];

    u32 ds_timeout;
    u32 tr_timeout;
    u32 modem_timeout, modem_wait;

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

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped G2 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped G2 write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_ADSTAG:
            std::printf("SB_ADSTAG write32 = %08X\n", data);

            SB_ADSTAG = data;
            break;
        case IO_ADSTAR:
            std::printf("SB_ADSTAR write32 = %08X\n", data);

            SB_ADSTAR = data;
            break;
        case IO_ADLEN:
            std::printf("SB_ADLEN write32 = %08X\n", data);

            SB_ADLEN = data;
            break;
        case IO_ADDIR:
            std::printf("SB_ADDIR write32 = %08X\n", data);

            SB_ADDIR = (data & 1) != 0;
            break;
        case IO_ADTSEL:
            std::printf("SB_ADTSEL write32 = %08X\n", data);

            SB_ADTSEL = data;
            break;
        case IO_ADEN:
            std::printf("SB_ADEN write32 = %08X\n", data);

            SB_ADEN = (data & 1) != 0;
            break;
        case IO_ADST:
            std::printf("SB_ADST write32 = %08X\n", data);

            assert((data & 1) == 0);
            break;
        case IO_ADSUSP:
            std::printf("SB_ADSUSP write32 = %08X\n", data);

            SB_ADSUSP.raw = data;
            break;
        case IO_E1STAG:
            std::printf("SB_E1STAG write32 = %08X\n", data);

            SB_E1STAG = data;
            break;
        case IO_E1STAR:
            std::printf("SB_E1STAR write32 = %08X\n", data);

            SB_E1STAR = data;
            break;
        case IO_E1LEN:
            std::printf("SB_E1LEN write32 = %08X\n", data);

            SB_E1LEN = data;
            break;
        case IO_E1DIR:
            std::printf("SB_E1DIR write32 = %08X\n", data);

            SB_E1DIR = (data & 1) != 0;
            break;
        case IO_E1TSEL:
            std::printf("SB_E1TSEL write32 = %08X\n", data);

            SB_E1TSEL = data;
            break;
        case IO_E1EN:
            std::printf("SB_E1EN write32 = %08X\n", data);

            SB_E1EN = (data & 1) != 0;
            break;
        case IO_E1ST:
            std::printf("SB_E1ST write32 = %08X\n", data);

            assert((data & 1) == 0);
            break;
        case IO_E1SUSP:
            std::printf("SB_E1SUSP write32 = %08X\n", data);

            SB_E1SUSP.raw = data;
            break;
        case IO_E2STAG:
            std::printf("SB_E2STAG write32 = %08X\n", data);

            SB_E2STAG = data;
            break;
        case IO_E2STAR:
            std::printf("SB_E2STAR write32 = %08X\n", data);

            SB_E2STAR = data;
            break;
        case IO_E2LEN:
            std::printf("SB_E2LEN write32 = %08X\n", data);

            SB_E2LEN = data;
            break;
        case IO_E2DIR:
            std::printf("SB_E2DIR write32 = %08X\n", data);

            SB_E2DIR = (data & 1) != 0;
            break;
        case IO_E2TSEL:
            std::printf("SB_E2TSEL write32 = %08X\n", data);

            SB_E2TSEL = data;
            break;
        case IO_E2EN:
            std::printf("SB_E2EN write32 = %08X\n", data);

            SB_E2EN = (data & 1) != 0;
            break;
        case IO_E2ST:
            std::printf("SB_E2ST write32 = %08X\n", data);

            assert((data & 1) == 0);
            break;
        case IO_E2SUSP:
            std::printf("SB_E2SUSP write32 = %08X\n", data);

            SB_E2SUSP.raw = data;
            break;
        case IO_DDSTAG:
            std::printf("SB_DDSTAG write32 = %08X\n", data);

            SB_DDSTAG = data;
            break;
        case IO_DDSTAR:
            std::printf("SB_DDSTAR write32 = %08X\n", data);

            SB_DDSTAR = data;
            break;
        case IO_DDLEN:
            std::printf("SB_DDLEN write32 = %08X\n", data);

            SB_DDLEN = data;
            break;
        case IO_DDDIR:
            std::printf("SB_DDDIR write32 = %08X\n", data);

            SB_DDDIR = (data & 1) != 0;
            break;
        case IO_DDTSEL:
            std::printf("SB_DDTSEL write32 = %08X\n", data);

            SB_DDTSEL = data;
            break;
        case IO_DDEN:
            std::printf("SB_DDEN write32 = %08X\n", data);

            SB_DDEN = (data & 1) != 0;
            break;
        case IO_DDST:
            std::printf("SB_DDST write32 = %08X\n", data);

            assert((data & 1) == 0);
            break;
        case IO_DDSUSP:
            std::printf("SB_DDSUSP write32 = %08X\n", data);

            SB_DDSUSP.raw = data;
            break;
        case IO_G2DSTO:
            std::printf("SB_G2DSTO write32 = %08X\n", data);

            SB_G2DSTO = data;
            break;
        case IO_G2TRTO:
            std::printf("SB_G2TRTO write32 = %08X\n", data);

            SB_G2TRTO = data;
            break;
        case IO_G2MDMTO:
            std::printf("SB_G2MDMTO write32 = %08X\n", data);

            SB_G2MDMTO = data;
            break;
        case IO_G2MDMW:
            std::printf("SB_G2MDMW write32 = %08X\n", data);

            SB_G2MDMW = data;
            break;
        case IO_G2APRO:
            std::printf("SB_G2APRO write32 = %08X\n", data);

            if ((data & ~0xFFFF) == 0x46590000) {
                SB_G2APRO.raw = (u16)data;
            }
            break;
        case 0x005F78A0:
        case 0x005F78A4:
        case 0x005F78A8:
        case 0x005F78AC:
        case 0x005F78B0:
        case 0x005F78B4:
        case 0x005F78B8:
            std::printf("Unknown G2 write32 @ %08X = %08X\n", addr, data);
            break;
        default:
            std::printf("Unmapped G2 write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

}
