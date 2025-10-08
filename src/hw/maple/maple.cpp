/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/maple/maple.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <scheduler.hpp>
#include <hw/holly/bus.hpp>
#include <hw/holly/intc.hpp>
#include <hw/maple/controller.hpp>
#include <hw/maple/device.hpp>

namespace hw::maple {

enum : u32 {
    IO_MDSTAR = 0x005F6C04,
    IO_MDTSEL = 0x005F6C10,
    IO_MDEN   = 0x005F6C14,
    IO_MDST   = 0x005F6C18,
    IO_MSYS   = 0x005F6C80,
    IO_MDAPRO = 0x005F6C8C,
    IO_MMSEL  = 0x005F6CE8,
};

#define SB_MDSTAR ctx.command_table_address
#define SB_MDTSEL ctx.is_vblank_trigger
#define SB_MDEN   ctx.enable
#define SB_MDST   ctx.is_running
#define SB_MSYS   ctx.interface_control
#define SB_MDAPRO ctx.address_protection
#define SB_MMSEL  ctx.is_msb_bit_31

constexpr usize NUM_DEVICES = 4;

struct {
    u32 command_table_address;
    bool is_vblank_trigger;
    bool enable;
    bool is_running;

    union {
        u32 raw;

        struct {
            u32 delay_time      :  4;
            u32                 :  4;
            u32 sending_rate    :  2;
            u32                 :  2;
            u32 single_trigger  :  1;
            u32                 :  3;
            u32 timeout_counter : 16;
        };
    } interface_control;

    union {
        u16 raw;

        struct {
            u16 bottom_address : 7;
            u16                : 1;
            u16 top_address    : 7;
            u16                : 1;
        };
    } address_protection;

    bool is_msb_bit_31;

    MapleDevice* devices[NUM_DEVICES];
} ctx;

union Instruction {
    u32 raw;

    struct {
        u32 transfer_length :  8;
        u32 command         :  3;
        u32                 :  5;
        u32 select_port     :  2;
        u32                 : 13;
        u32 end_flag        :  1;
    };
};

static u32 read_word(u32& addr) {
    const u32 data = hw::holly::bus::read<u32>(addr);

    addr += sizeof(data);

    return data;
}

static void write_word(u32& addr, const u32 data) {
    hw::holly::bus::write<u32>(addr, data);

    addr += sizeof(data);
}

constexpr int MAPLE_INTERRUPT = 12;

static void finish_maple_dma(const int) {
    SB_MDST = false;

    hw::holly::intc::assert_normal_interrupt(MAPLE_INTERRUPT);
}

enum {
    MAPLE_COMMAND_TRANSMIT_DATA,
};

enum {
    MAPLE_DEVICE_COMMAND_INFO_REQUEST  = 0x01,
    MAPLE_DEVICE_COMMAND_GET_CONDITION = 0x09,
};

enum {
    MAPLE_DEVICE_CONTROLLER = 0x00000001,
    MAPLE_DEVICE_NONE       = 0xFFFFFFFF,
};

struct Frame {
    // This goes to a peripheral
    u8 port;
    u8 command;
    std::vector<u32> send_bytes;

    // This goes back to SH-4
    u32 receive_addr;
    std::vector<u32> receive_bytes;
};

static Frame decode_frame(const Instruction instr, u32& addr) {
    Frame frame{};

    frame.port = instr.select_port;
    frame.receive_addr = read_word(addr);
    frame.command = read_word(addr);

    for (u32 i = 0; i < instr.transfer_length; i++) {
        frame.receive_bytes.push_back(read_word(addr));
    }

    return frame;
}

static void transmit_data(Frame& frame) {
    std::printf("MAPLE Port %c receive address = %08X\n", 'A' + frame.port, frame.receive_addr);
    std::printf("MAPLE Port %c command %02X\n", 'A' + frame.port, frame.command);

    if (ctx.devices[frame.port] != nullptr) {
        switch (frame.command) {
            case MAPLE_DEVICE_COMMAND_INFO_REQUEST:
                // TODO
                break;
            default:
                std::printf("MAPLE Unimplemented device command %02X\n", frame.command);
                exit(1);
        }
    } else {
        frame.receive_bytes.push_back(MAPLE_DEVICE_NONE);
    }
}

constexpr i64 MAPLE_DELAY = 4096;

static void execute_maple_dma() {
    std::printf("MAPLE DMA @ %08X\n", SB_MDSTAR);

    SB_MDST = true;

    u32 addr = SB_MDSTAR;

    while (true) {
        const Instruction instr = Instruction{.raw = read_word(addr)};

        std::printf("MAPLE instruction @ %08lX = %08X\n", addr - sizeof(instr), instr.raw);

        Frame frame = decode_frame(instr, addr);
        
        switch (instr.command) {
            case MAPLE_COMMAND_TRANSMIT_DATA:
                transmit_data(frame);
                break;
            default:
                std::printf("Unimplemented MAPLE command %u\n", instr.command);
                exit(1);
        }

        for (u32 data : frame.receive_bytes) {
            write_word(frame.receive_addr, data);
        }

        if (instr.end_flag) {
            scheduler::schedule_event(
                "MAPLE_END",
                finish_maple_dma,
                0,
                scheduler::to_scheduler_cycles<scheduler::HOLLY_CLOCKRATE>(MAPLE_DELAY)
            );

            break;
        }
    }
}

void initialize() {
    ctx.devices[0] = new Controller();
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {
    for (auto device : ctx.devices) {
        if (device != nullptr) {
            delete device;
        }
    }
}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped MAPLE read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_MDST:
            std::puts("SB_MDST read32");

            return SB_MDST;
        default:
            std::printf("Unmapped MAPLE read32 @ %08X\n", addr);
            exit(1);
    }
}

template u8 read(u32);
template u16 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped MAPLE write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u32 data) {
    switch (addr) {
        case IO_MDSTAR:
            std::printf("SB_MDSTAR write32 = %08X\n", data);

            SB_MDSTAR = data;
            break;
        case IO_MDTSEL:
            std::printf("SB_MDTSEL write32 = %08X\n", data);

            SB_MDTSEL = (data & 1) != 0;
            break;
        case IO_MDEN:
            std::printf("SB_MDEN write32 = %08X\n", data);

            SB_MDEN = (data & 1) != 0;
            break;
        case IO_MDST:
            std::printf("SB_MDST write32 = %08X\n", data);
            
            // Manual trigger
            if (!SB_MDTSEL && ((data & 1) != 0)) {
                execute_maple_dma();
            } 
            break;
        case IO_MSYS:
            std::printf("SB_MSYS write32 = %08X\n", data);

            SB_MSYS.raw = data;
            break;
        case IO_MDAPRO:
            std::printf("SB_MDAPRO write32 = %08X\n", data);

            if ((data & ~0xFFFF) == 0x61550000) {
                SB_MDAPRO.raw = (u16)data;
            }
            break;
        case IO_MMSEL:
            std::printf("SB_MMSEL write32 = %08X\n", data);

            SB_MMSEL = (data & 1) != 0;
            break;
        default:
            std::printf("Unmapped MAPLE write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u64);

}
