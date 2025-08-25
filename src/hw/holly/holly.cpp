/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/holly/holly.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <hw/holly/bus.hpp>
#include <hw/holly/intc.hpp>
#include <hw/holly/maple.hpp>

namespace hw::holly {

namespace Address {
    enum : u32 {
        C2Dstat = 0x005F6800,
        C2Dlen  = 0x005F6804,
        C2Dst   = 0x005F6808,
        Sdstaw  = 0x005F6810,
        Sdbaaw  = 0x005F6814,
        Sdwlt   = 0x005F6818,
        Sdlas   = 0x005F681C,
        Sdst    = 0x005F6820,
        Dbreqm  = 0x005F6840,
        Bavlwc  = 0x005F6844,
        C2Dpryc = 0x005F6848,
        C2Dmaxl = 0x005F684C,
        Lmmode0 = 0x005F6884,
        Lmmode1 = 0x005F6888,
        Rbsplt  = 0x005F68A0,
    };
}

#define SB_C2DSTAT ctx.channel_2.destination_address
#define SB_C2DLEN  ctx.channel_2.length
#define SB_C2DST   ctx.channel_2.is_running
#define SB_SDSTAW  ctx.sort_dma.link_start_address
#define SB_SDBAAW  ctx.sort_dma.link_base_address
#define SB_SDWLT   ctx.sort_dma.is_32_bit
#define SB_SDLAS   ctx.sort_dma.is_shift
#define SB_SDST    ctx.sort_dma.is_running
#define SB_DBREQM  ctx.ddt_interface.is_dbreq_masked
#define SB_BAVLWC  ctx.ddt_interface.bavl_wait_count
#define SB_C2DPRYC ctx.ddt_interface.dma_priority_count
#define SB_C2DMAXL ctx.ddt_interface.dma_burst_length
#define SB_LMMODE0 ctx.tile_accelerator.is_bus_32_bit_1
#define SB_LMMODE1 ctx.tile_accelerator.is_bus_32_bit_2
#define SB_RBSPLT  ctx.enable_root_bus_split

struct {
    struct {
        u32 destination_address;
        u32 length;
        bool is_running;
    } channel_2;

    struct {
        u32 link_start_address;
        u32 link_base_address;
        bool is_32_bit;
        bool is_shift;
        bool is_running;
    } sort_dma;

    struct {
        bool is_dbreq_masked;
        u32 bavl_wait_count;
        u32 dma_priority_count;
        u32 dma_burst_length;
    } ddt_interface;

    struct {
        bool is_bus_32_bit_1;
        bool is_bus_32_bit_2;
    } tile_accelerator;

    bool enable_root_bus_split;
} ctx;

void initialize() {
    bus::initialize();
    intc::initialize();
    maple::initialize();
}

void reset() {
    bus::reset();
    intc::reset();
    maple::reset();

    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {
    bus::shutdown();
    intc::shutdown();
    maple::shutdown();
}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case Address::C2Dstat:
            std::printf("SB_C2DSTAT write32 = %08X\n", data);

            SB_C2DSTAT = data;
            break;
        case Address::C2Dlen:
            std::printf("SB_C2DLEN write32 = %08X\n", data);

            SB_C2DLEN = data;
            break;
        case Address::C2Dst:
            std::printf("SB_C2DST write32 = %08X\n", data);

            SB_C2DST = (data & 1) != 0;

            assert(!SB_C2DST);
            break;
        case Address::Sdstaw:
            std::printf("SB_SDSTAW write32 = %08X\n", data);

            SB_SDSTAW = data | (1 << 27); // Bit 27 is hardwired to 1
            break;
        case Address::Sdbaaw:
            std::printf("SB_SDBAAW write32 = %08X\n", data);

            SB_SDBAAW = data | (1 << 27); // Bit 27 is hardwired to 1
            break;
        case Address::Sdwlt:
            std::printf("SB_SDWLT write32 = %08X\n", data);

            SB_SDWLT = (data & 1) != 0;
            break;
        case Address::Sdlas:
            std::printf("SB_SDLAS write32 = %08X\n", data);

            SB_SDLAS = (data & 1) != 0;
            break;
        case Address::Sdst:
            std::printf("SB_SDST write32 = %08X\n", data);

            SB_SDST = (data & 1) != 0;

            assert(!SB_SDST);
            break;
        case Address::Dbreqm:
            std::printf("SB_DBREQM write32 = %08X\n", data);

            SB_DBREQM = (data & 1) != 0;
            break;
        case Address::Bavlwc:
            std::printf("SB_BAVLWC write32 = %08X\n", data);

            SB_BAVLWC = data;
            break;
        case Address::C2Dpryc:
            std::printf("SB_C2DPRYC write32 = %08X\n", data);

            SB_C2DPRYC = data;
            break;
        case Address::C2Dmaxl:
            std::printf("SB_C2DMAXL write32 = %08X\n", data);

            SB_C2DMAXL = data;
            break;
        case Address::Lmmode0:
            std::printf("SB_LMMODE0 write32 = %08X\n", data);

            SB_LMMODE0 = (data & 1) != 0;
            break;
        case Address::Lmmode1:
            std::printf("SB_LMMODE1 write32 = %08X\n", data);

            SB_LMMODE1 = (data & 1) != 0;
            break;
        case Address::Rbsplt:
            std::printf("SB_RBSPLT write32 = %08X\n", data);

            SB_RBSPLT = (data >> 31) != 0;
            break;
        case 0x005F68A4:
        case 0x005F68AC:
            std::printf("Unknown HOLLY write32 @ %08X = %08X\n", addr, data);
            break;
        default:
            std::printf("Unmapped write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

}
